// Medal of Honor (2010), single player. Unreal Engine 3 on Direct3D 9, 32-bit, running through dgVoodoo2
// (D3D9 -> D3D11); the shipped D3D10 path was never implemented, so this is the only render path.
//
// The scene lives in an r16g16b16a16_float SceneColor, and ONE pass per frame turns it into the 8-bit canvas the
// bloom chain and the HUD then work on: an EdgeAA + filmic + 3D-LUT tonemap that exists in four permutations
// (EdgeAA on/off x motion-blur/DoF on/off), each with its own grade constant register. Replacing those four is
// the whole HDR port; everything downstream of them is 8-bit by design.
//
// Constraints inherited from the wrapper and the 32-bit process:
//  - shader hashes are the dgVoodoo-TRANSLATED ones and are only valid for 2.87.3 (see the telemetry in OnPresent);
//  - ReShade must be installed as dxgi.dll, and only ONE swapchain-hooking addon may be present;
//  - 2 GB of address space, so no feature here allocates render-sized scratch yet.

#define DISABLE_AUTO_DEBUGGER 1 // the DEVELOPMENT MessageBox is invisible under the wrapper and stalls the ReShade loader (error 1114)
#define GAME_MEDAL_OF_HONOR_2010 1
#define ENABLE_NGX 0 // NGX is x64 only, and the game has no motion vectors
#define ENABLE_FIDELITY_SK 0
#define GEOMETRY_SHADER_SUPPORT 0
// The wrapper blend repair below re-issues the draw through "original_draw_dispatch_func", and outside DEVELOPMENT
// that pointer is null unless this is set - the hook would silently never fire in Publishing.
#define ENABLE_POST_DRAW_DISPATCH_CALLBACK 1
// Luma's multi-scale HDR bloom pyramid REPLACES the game's own post-tonemap glow. The engine builds that glow from
// the gamma-encoded canvas the tonemap already display-mapped, where a five-level sum becomes a ~50x multiplication
// once decoded; no reshaping of the blend fixes that, only moving the glow before the display map does. Core
// auto-registers the 4 "Bloom ..." passes from Luma_Bloom_impl.hlsl. Airborne precedent, same engine and wrapper.
#define ENABLE_BLOOM 1
// The only native AA is EdgeAA, a luma-gradient directional blur fused into the tonemap itself - a softener, not
// a resolve. SMAA replaces it (the tonemap gates RunEdgeAA on GameSettings.SMAAEnable). Core auto-registers the
// 6 "SMAA ..." passes from Luma_SMAA_impl.hlsl.
#define ENABLE_SMAA 1

#include "..\..\Core\core.hpp"

#include <shellapi.h> // ShellExecuteA: system() hangs the render thread in exclusive fullscreen

// The tonemap + 3D LUT pass, in its four settings-driven permutations. They share one implementation
// (Shaders/Medal of Honor/Luma_MOH_Tonemap.hlsl); the wrappers differ only in feature blocks and in which cb4 row
// holds the grade tail, so a hash is never enough to know the register map — see the table in that file.
static constexpr uint32_t kTonemapHash = 0x3EDE377E;             // plain: no EdgeAA, no motion blur. Grade cb4[12]
static constexpr uint32_t kTonemapEdgeAAHash = 0x8B4ED0E3;       // EdgeAA. Grade cb4[13]
static constexpr uint32_t kTonemapMotionBlurHash = 0x77653D15;   // motion blur / DoF. Grade cb4[14]
static constexpr uint32_t kTonemapEdgeAAMotionHash = 0x02B86437; // EdgeAA + motion blur / DoF. Grade cb4[14]

// The bloom composite. Also replaced, and it reads LumaSettings to bound the screen blend by the same canvas
// ceiling the tonemap respects, so it needs the Luma cbuffers uploaded as well.
static constexpr uint32_t kBloomCompositeHash = 0x844582AD;

// The bloom bright pass. Replaced too, and it reads LumaBloomEnable to stand down when the Luma pyramid owns the
// glow, so it needs the Luma cbuffers as well.
static constexpr uint32_t kBloomBrightPassHash = 0x5FE75452;

#if ENABLE_BLOOM
// Luma bloom pyramid mip 0, read by the tonemap replacements at register(t6): clear of the four slots those
// shaders actually declare (t0 scene, t2 blur, t3 depth, t33 LUT).
static constexpr uint32_t kLumaBloomSlot = 6;
// One sigma per mip, and the mip count is taken FROM the array so the two cannot drift. Compile-time on purpose:
// DrawBloom only resizes its mip cache in DEVELOPMENT, so a runtime change is a no-op in Publishing.
// Airborne's set, which is BL2's with the last octave halved.
static float g_bloom_sigmas[] = {1.5f, 2.f, 2.f, 2.f, 1.f, 0.5f};
static constexpr int kBloomNMips = (int)std::size(g_bloom_sigmas);

// Mirrored into GameSettings.LumaBloomEnable, which the tonemap (add the pyramid), the bright pass (write 0) and
// the composite (pass through) all read — so this single switch really swaps one bloom for the other.
static bool g_luma_bloom_enable = true;
#endif

#if ENABLE_SMAA
// Anti-aliasing. Mirrored into GameSettings.SMAAEnable, which the tonemap reads to stand the engine's EdgeAA down.
static bool g_smaa_enable = true;
static bool g_smaa_predication = true;      // predicate SMAA on geometry, using the depth in the scene buffer's alpha
static float g_smaa_pred_tolerance = 0.02f; // plane deviation counted as a full edge, as a fraction of view depth
// RCAS sharpen on the SMAA output, opt-in at 0 (Airborne/BL2/TW2 precedent): how much sharpening is wanted is a
// taste call, and the port already gets sharper by removing EdgeAA.
static float g_rcas_sharpness = 0.f;
#if DEVELOPMENT
static bool g_smaa_pred_debug = false; // show the predication mask instead of the frame
#endif
#endif

// Hide the game's HUD, for clean screenshots and for judging the grade/bloom/AA calibrations without a HUD in the
// frame. Session-only, never persisted (Airborne precedent): a value that survived a restart would read as a
// broken HUD rather than as a setting someone left on.
static bool g_hide_ui = false;
#if DEVELOPMENT || TEST
// Budget of cancelled draws to log after each toggle-on, so one in-game toggle proves WHAT the blend test caught.
static int g_hide_ui_log_budget = 0;
#endif

struct MedalOfHonorGameDeviceData final : public GameDeviceData
{
   // Repaired blend states, keyed by the ORIGINAL desc (Airborne/DXHR precedent). Keying by desc rather than by
   // the source state's pointer means a released state can't leave a stale key that a later allocation reuses.
   struct BlendDescCompare
   {
      bool operator()(const D3D11_BLEND_DESC& a, const D3D11_BLEND_DESC& b) const
      {
         return memcmp(&a, &b, sizeof(D3D11_BLEND_DESC)) < 0;
      }
   };
   std::map<D3D11_BLEND_DESC, ComPtr<ID3D11BlendState>, BlendDescCompare> fixed_blend_states;

   bool has_drawn_tonemap = false; // reset every Present

#if ENABLE_BLOOM
   // Non-owning view onto core's DrawBloom mip 0 (AddRef'd by DrawBloom; the pyramid itself is core-managed and
   // released with the swapchain). Rebuilt every frame the feature is on.
   ComPtr<ID3D11ShaderResourceView> srv_luma_bloom;
#endif

#if ENABLE_SMAA
   // ---- SMAA (Airborne/TW2/BL2 shape, see RunPostTonemapSMAA) ----
   // Metrics CB (b1) = (1/w, 1/h, w, h) + (predication scale, 0, 0, 0).
   ComPtr<ID3D11Buffer> cb_smaa_metrics;
   uint32_t smaa_metrics_w = 0, smaa_metrics_h = 0;
   float smaa_metrics_pred_scale = -1.f;
   uint32_t smaa_core_w = 0, smaa_core_h = 0;
   // SRV-readable snapshot of the canvas; the chain writes the canvas, so it must sample this copy instead.
   ComPtr<ID3D11Texture2D> tex_input;
   ComPtr<ID3D11ShaderResourceView> srv_input;
   uint32_t smaa_temps_w = 0, smaa_temps_h = 0;

   // Predication: the scene alpha, linearised, turned into an edge-ness mask by the depth-extract CS.
   bool depth_cb_live = false; // DEV readout only: was a motion-blur permutation (the one that declares cb4[10]) last seen
   ComPtr<ID3D11Buffer> cb_pred;
   float pred_tolerance = -1.f;
   ComPtr<ID3D11Texture2D> tex_pred;
   ComPtr<ID3D11UnorderedAccessView> uav_pred;
   ComPtr<ID3D11ShaderResourceView> srv_pred;
   uint32_t pred_w = 0, pred_h = 0;

   void ReleasePredicationScratch()
   {
      uav_pred.reset();
      srv_pred.reset();
      tex_pred.reset();
      pred_w = pred_h = 0;
   }

   // RCAS. The intermediate exists ONLY while sharpening is on: with the slider at 0 the SMAA chain renders
   // straight into the canvas, which saves both this full-resolution copy and a write-back.
   ComPtr<ID3D11Buffer> cb_sharpen;
   uint32_t sharpen_w = 0, sharpen_h = 0;
   float sharpen_amount = -1.f;
   ComPtr<ID3D11Texture2D> tex_smaa_out;
   ComPtr<ID3D11RenderTargetView> tex_smaa_out_rtv;
   ComPtr<ID3D11ShaderResourceView> tex_smaa_out_srv;
   uint32_t smaa_out_w = 0, smaa_out_h = 0;

   void ReleaseSharpenScratch()
   {
      tex_smaa_out_rtv.reset();
      tex_smaa_out_srv.reset();
      tex_smaa_out.reset();
      smaa_out_w = smaa_out_h = 0;
   }

   // Turning the feature off must give the address space back: moh.exe is not large-address-aware (2 GB), and the
   // mod already spends an fp16 swapchain, the upgraded post chain and the bloom pyramid. The snapshot alone is
   // ~66 MB at 4K, and core's own SMAA intermediates come on top.
   void ReleaseSMAAScratch()
   {
      srv_input.reset();
      tex_input.reset();
      smaa_temps_w = smaa_temps_h = 0;
      ReleasePredicationScratch();
      ReleaseSharpenScratch();
   }
#endif

#if DEVELOPMENT || TEST
   // Read-back of the engine's OWN bloom constants, purely diagnostic. The question it answers is whether the
   // engine authors them per area: if they never move, the per-area tracking Airborne carries would be inert here
   // and the plain sliders are correct. One staging copy per pass per frame, mapped on the NEXT frame so the GPU
   // is never waited on.
   struct Cb4Readback
   {
      ComPtr<ID3D11Buffer> staging;
      bool pending = false;
   };
   Cb4Readback rb_tonemap;              // cb4 as bound to the TONEMAP: row 8 is SceneExposure there, not a bloom constant
   Cb4Readback rb_bright;               // cb4 as bound to the bright pass
   Cb4Readback rb_composite;            // cb4 as bound to the composite
   float engine_bloom_threshold = -1.f; // cb4[8].x at the bright pass, -1 = never captured
   float engine_bloom_amount = -1.f;    // cb4[8].z at the composite,   -1 = never captured
   float scene_exposure = -1.f;         // cb4[8].z at the TONEMAP: the anchor the bloom threshold is a fraction of
   float engine_bloom_threshold_logged = -2.f;
   float engine_bloom_amount_logged = -2.f;
   float scene_exposure_logged = -2.f;

   bool logged_samplers = false; // AF census, once per session

   // Frames that present WITHOUT our tonemap are outside the mod's contract. Core's Display Composition still
   // runs on them (UI_DRAW_TYPE 2 lights its gate unconditionally), it decodes the canvas as gamma and multiplies
   // by Game Paper White, and it applies NO peak roll-off -- ours lives inside the tonemap. So whatever those
   // frames leave on the canvas goes to the display as-is. The menu has such frames, and they read tens of
   // thousands of nits.
   //
   // Vanilla could not do this: every buffer in that chain was r8g8b8a8_unorm and the ROP clipped at 1.0 for free
   // (the same removed clamp behind the black frame in section 13 and the bloom halos in section 19). To fix it
   // in the pass that causes it, that pass has to be named first, and a devkit snapshot is unreliable here
   // because the condition is transient. So: remember the last draws of every frame, and dump them if the frame
   // turns out to have presented untonemapped.
   static constexpr uint32_t kDrawRingSize = 48;
   uint32_t draw_ring[kDrawRingSize] = {};
   uint32_t draws_this_frame = 0;
   uint32_t untonemapped_frames_logged = 0;
#endif

   // Wrapper-build telemetry: an unkeyed dgVoodoo build fails silently (the addon loads, no replacement binds).
   bool ever_matched_tonemap = false;
   bool build_check_done = false;
   uint32_t frames_presented = 0;
};

class MedalOfHonor final : public Game
{
   static bool IsTonemap(const ShaderHashesList<OneShaderPerPipeline>& hashes)
   {
      return hashes.Contains(kTonemapHash, reshade::api::shader_stage::pixel) || hashes.Contains(kTonemapEdgeAAHash, reshade::api::shader_stage::pixel) || hashes.Contains(kTonemapMotionBlurHash, reshade::api::shader_stage::pixel) || hashes.Contains(kTonemapEdgeAAMotionHash, reshade::api::shader_stage::pixel);
   }

   static bool IsBloomComposite(const ShaderHashesList<OneShaderPerPipeline>& hashes)
   {
      return hashes.Contains(kBloomCompositeHash, reshade::api::shader_stage::pixel);
   }

   static bool IsBloomBrightPass(const ShaderHashesList<OneShaderPerPipeline>& hashes)
   {
      return hashes.Contains(kBloomBrightPassHash, reshade::api::shader_stage::pixel);
   }

#if DEVELOPMENT || TEST
   // Copies the currently bound PS b4 into a staging buffer and hands back the row captured on the PREVIOUS call.
   // Mapping the copy issued this frame would stall the GPU; mapping last frame's costs nothing and the values
   // this watches change at walking speed. DO_NOT_WAIT keeps even that read non-blocking: a miss just retries.
   //
   // Diagnostic use only. The predication CS used to get its depth pair this way; it now reads the game's cb4
   // directly at the dispatch, which is same-frame and needs no staging copy at all.
   static bool CaptureCb4Row(ID3D11Device* device, ID3D11DeviceContext* context, MedalOfHonorGameDeviceData::Cb4Readback& rb, uint32_t row_index, float out_row[4])
   {
      ComPtr<ID3D11Buffer> cb;
      context->PSGetConstantBuffers(4, 1, cb.put());
      if (!cb)
         return false;

      D3D11_BUFFER_DESC desc = {};
      cb->GetDesc(&desc);
      const uint32_t row_offset = row_index * 4u * sizeof(float);
      if (desc.ByteWidth < row_offset + 4u * sizeof(float))
         return false;

      bool captured = false;
      if (rb.staging && rb.pending)
      {
         D3D11_MAPPED_SUBRESOURCE mapped = {};
         if (SUCCEEDED(context->Map(rb.staging.get(), 0, D3D11_MAP_READ, D3D11_MAP_FLAG_DO_NOT_WAIT, &mapped)) && mapped.pData != nullptr)
         {
            memcpy(out_row, static_cast<const uint8_t*>(mapped.pData) + row_offset, 4u * sizeof(float));
            context->Unmap(rb.staging.get(), 0);
            rb.pending = false;
            captured = true;
         }
      }

      // CopyResource needs identical descs, so a resized cbuffer invalidates the staging copy. The size comes from
      // the staging buffer itself rather than a shadow field, which cannot then disagree with it.
      if (rb.staging)
      {
         D3D11_BUFFER_DESC staging_desc = {};
         rb.staging->GetDesc(&staging_desc);
         if (staging_desc.ByteWidth != desc.ByteWidth)
         {
            rb.staging.reset();
            rb.pending = false;
         }
      }
      if (!rb.staging)
      {
         D3D11_BUFFER_DESC staging_desc = {};
         staging_desc.ByteWidth = desc.ByteWidth;
         staging_desc.Usage = D3D11_USAGE_STAGING;
         staging_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
         if (FAILED(device->CreateBuffer(&staging_desc, nullptr, rb.staging.put())))
            return captured;
      }

      // Only when the previous copy has actually been read. Re-copying over an unread slot pushes its fence
      // forward every frame, so on a driver queuing two or three frames the DO_NOT_WAIT map can never retire and
      // the value never arrives -- the copies are pure waste and the read silently starves.
      if (!rb.pending)
      {
         context->CopyResource(rb.staging.get(), cb.get());
         rb.pending = true;
      }
      return captured;
   }
#endif // DEVELOPMENT || TEST

   static MedalOfHonorGameDeviceData& GetGameDeviceData(DeviceData& device_data)
   {
      return *static_cast<MedalOfHonorGameDeviceData*>(device_data.game);
   }

#if ENABLE_SMAA
   // Named injected shaders live in unordered_maps the render thread otherwise only reads: look them up with
   // "find" (operator[] would default-insert on a miss, mutating a map core's draw helpers read concurrently).
   template <typename ShaderMap>
   static auto FindShader(const ShaderMap& shaders, uint32_t name)
   {
      const auto it = shaders.find(name);
      return it != shaders.end() ? it->second.get() : nullptr;
   }
   template <typename ShaderMap>
   static bool AllShadersReady(const ShaderMap& shaders, std::initializer_list<uint32_t> names)
   {
      for (const uint32_t name : names)
      {
         if (FindShader(shaders, name) == nullptr)
            return false;
      }
      return true;
   }

   static bool CreateImmutableCB(ID3D11Device* device, const void* data, UINT size, ComPtr<ID3D11Buffer>& out)
   {
      out.reset();
      D3D11_BUFFER_DESC bd = {};
      bd.ByteWidth = size;
      bd.Usage = D3D11_USAGE_IMMUTABLE;
      bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
      D3D11_SUBRESOURCE_DATA sd = {};
      sd.pSysMem = data;
      return SUCCEEDED(device->CreateBuffer(&bd, &sd, out.put()));
   }

   // The SMAA snapshot must pass the canvas' live format: CopyResource requires source and destination formats
   // to match.
   static bool CreateDefaultTex(ID3D11Device* device, uint32_t w, uint32_t h, UINT bind_flags, ComPtr<ID3D11Texture2D>& out, DXGI_FORMAT format = DXGI_FORMAT_R16G16B16A16_FLOAT)
   {
      out.reset();
      D3D11_TEXTURE2D_DESC td = {};
      td.Width = w;
      td.Height = h;
      td.MipLevels = 1;
      td.ArraySize = 1;
      td.Format = format;
      td.SampleDesc.Count = 1;
      td.Usage = D3D11_USAGE_DEFAULT;
      td.BindFlags = bind_flags;
      return SUCCEEDED(device->CreateTexture2D(&td, nullptr, out.put()));
   }
   // Core's DrawKarisAverage buffer: a FULL-RES fp16 texture (~66 MB at 4K), sized from the scene it is handed.
   // Core's own swapchain hook resets only the UAV, and the SRV entry holds its own reference, so nothing frees
   // this when the pyramid is switched off unless the game does it.
   static void ReleaseCoreKarisAverage(DeviceData& device_data)
   {
      auto& mr = device_data.managed_resources;
      mr.unordered_access_views[CompileTimeStringHash("luma_karis_average")].reset();
      mr.shader_resource_views[CompileTimeStringHash("luma_karis_average")].reset();
   }

   // Core's DrawSMAA intermediates: ~83 MB at 4K (D24S8 depth-stencil + RG8 edges + RGBA8 weights), lazily built
   // and sized from the RTV they are handed, i.e. the canvas. Core only drops them on swapchain init, which is not
   // the same event: the canvas is dgVoodoo's internal D3D9 surface, so the game invalidates them on ITS size.
   // The SRVs go with the RTVs: they hold their own reference, so dropping the RTV/DSV alone would free nothing
   // on a feature-off release.
   static void ReleaseCoreSMAAIntermediates(DeviceData& device_data)
   {
      auto& mr = device_data.managed_resources;
      mr.depth_stencil_views[CompileTimeStringHash("smaa_dsv")].reset();
      mr.render_target_views[CompileTimeStringHash("smaa_edge_detection")].reset();
      mr.render_target_views[CompileTimeStringHash("smaa_blending_weight_calculation")].reset();
      mr.shader_resource_views[CompileTimeStringHash("smaa_edge_detection")].reset();
      mr.shader_resource_views[CompileTimeStringHash("smaa_blending_weight_calculation")].reset();
   }

   // SMAA on the canvas the tonemap just wrote, driven from that pass' post-draw callback: before the bloom chain
   // and DoF, which is where antialiasing belongs, and long before the HUD is drawn on the same canvas.
   void RunPostTonemapSMAA(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, DeviceData& device_data, MedalOfHonorGameDeviceData& gd, ID3D11Resource* canvas_res, ID3D11RenderTargetView* canvas_rtv, ID3D11ShaderResourceView* srv_scene, ID3D11Buffer* game_cb4)
   {
      uint4 cinfo{};
      DXGI_FORMAT cfmt = DXGI_FORMAT_UNKNOWN;
      GetResourceInfo(canvas_res, cinfo, cfmt);
      const uint32_t w = cinfo.x, h = cinfo.y;
      if (w == 0 || h == 0 || cfmt == DXGI_FORMAT_UNKNOWN)
         return;

      // Shader-readiness gate (async loader / dev live-reload): skip SMAA this frame if anything is missing.
      const bool smaa_ready =
         AllShadersReady(device_data.native_pixel_shaders, {CompileTimeStringHash("SMAA Edge Detection PS"), CompileTimeStringHash("SMAA Blending Weight Calculation PS"), CompileTimeStringHash("SMAA Neighborhood Blending PS")}) && AllShadersReady(device_data.native_vertex_shaders, {CompileTimeStringHash("SMAA Edge Detection VS"), CompileTimeStringHash("SMAA Blending Weight Calculation VS"), CompileTimeStringHash("SMAA Neighborhood Blending VS")});
      if (!smaa_ready)
         return;

      // Drop DrawSMAA's core-managed intermediates on resolution change so they recreate at the new size.
      if (gd.smaa_core_w != w || gd.smaa_core_h != h)
      {
         ReleaseCoreSMAAIntermediates(device_data);
         gd.smaa_core_w = w;
         gd.smaa_core_h = h;
      }

      // Edge-ness from the scene alpha. Scale and mask fall back together: advertising 2.0 with a null mask would
      // raise the threshold frame-wide with nothing to relax it back on geometric edges. The size check matters
      // because the CS maps texels 1:1 - a mismatch reads a sub-rect and misaligns the whole mask. A null
      // "game_cb4" is the cb4[10] liveness gate: outside the motion-blur permutations that row is not declared.
      auto* pred_cs = FindShader(device_data.native_compute_shaders, CompileTimeStringHash("MOH Depth Extract CS"));
      bool pred_ok = g_smaa_predication && game_cb4 != nullptr && srv_scene != nullptr && pred_cs != nullptr;
      if (pred_ok)
      {
         uint4 sinfo{};
         DXGI_FORMAT sfmt = DXGI_FORMAT_UNKNOWN;
         GetResourceInfo(srv_scene, sinfo, sfmt);
         pred_ok = sinfo.x == w && sinfo.y == h;
      }
      if (pred_ok)
      {
         // Tolerance only. The depth linearisation pair lives in the game's own cb4, bound below, so nothing here
         // has to be rebuilt when the camera changes - which matters because this buffer is IMMUTABLE.
         if (!gd.cb_pred || gd.pred_tolerance != g_smaa_pred_tolerance)
         {
            const float p[4] = {g_smaa_pred_tolerance, 0.f, 0.f, 0.f};
            if (CreateImmutableCB(native_device, p, sizeof(p), gd.cb_pred))
               gd.pred_tolerance = g_smaa_pred_tolerance;
         }
         if (!gd.tex_pred || gd.pred_w != w || gd.pred_h != h)
         {
            gd.ReleasePredicationScratch();
            if (CreateDefaultTex(native_device, w, h, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, gd.tex_pred, DXGI_FORMAT_R16_FLOAT))
            {
               native_device->CreateUnorderedAccessView(gd.tex_pred.get(), nullptr, gd.uav_pred.put());
               native_device->CreateShaderResourceView(gd.tex_pred.get(), nullptr, gd.srv_pred.put());
               gd.pred_w = w;
               gd.pred_h = h;
            }
         }
         pred_ok = gd.cb_pred && gd.uav_pred && gd.srv_pred;
      }

      const float pred_scale = pred_ok ? 2.f : 1.f;
      if (!gd.cb_smaa_metrics || gd.smaa_metrics_w != w || gd.smaa_metrics_h != h || gd.smaa_metrics_pred_scale != pred_scale)
      {
         const float metrics[8] = {1.f / (float)w, 1.f / (float)h, (float)w, (float)h, pred_scale, 0.f, 0.f, 0.f};
         if (CreateImmutableCB(native_device, metrics, sizeof(metrics), gd.cb_smaa_metrics))
         {
            gd.smaa_metrics_w = w;
            gd.smaa_metrics_h = h;
            gd.smaa_metrics_pred_scale = pred_scale;
         }
      }
      if (!gd.cb_smaa_metrics)
         return;

      // RCAS decides the chain's SHAPE, so resolve it before allocating anything: with sharpening off the last
      // SMAA pass writes the canvas directly, which removes both a full-frame write-back and the intermediate.
      auto* sharpen_vs = FindShader(device_data.native_vertex_shaders, CompileTimeStringHash("Copy VS"));
      auto* sharpen_ps = FindShader(device_data.native_pixel_shaders, CompileTimeStringHash("MOH Sharpen PS"));
      bool do_sharpen = g_rcas_sharpness > 0.f && sharpen_vs != nullptr && sharpen_ps != nullptr;
      if (do_sharpen)
      {
         if (!gd.cb_sharpen || gd.sharpen_w != w || gd.sharpen_h != h || gd.sharpen_amount != g_rcas_sharpness)
         {
            const float sp[4] = {(float)w, (float)h, g_rcas_sharpness, 0.f};
            if (CreateImmutableCB(native_device, sp, sizeof(sp), gd.cb_sharpen))
            {
               gd.sharpen_w = w;
               gd.sharpen_h = h;
               gd.sharpen_amount = g_rcas_sharpness;
            }
         }
         if (!gd.tex_smaa_out || gd.smaa_out_w != w || gd.smaa_out_h != h)
         {
            gd.ReleaseSharpenScratch();
            if (CreateDefaultTex(native_device, w, h, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET, gd.tex_smaa_out, cfmt))
            {
               native_device->CreateRenderTargetView(gd.tex_smaa_out.get(), nullptr, gd.tex_smaa_out_rtv.put());
               native_device->CreateShaderResourceView(gd.tex_smaa_out.get(), nullptr, gd.tex_smaa_out_srv.put());
               gd.smaa_out_w = w;
               gd.smaa_out_h = h;
            }
         }
         if (!gd.cb_sharpen || !gd.tex_smaa_out_rtv || !gd.tex_smaa_out_srv)
            do_sharpen = false; // allocation failed: fall back to the un-sharpened chain rather than dropping SMAA
      }

      if (!gd.tex_input || gd.smaa_temps_w != w || gd.smaa_temps_h != h)
      {
         gd.srv_input.reset();
         gd.tex_input.reset();
         if (CreateDefaultTex(native_device, w, h, D3D11_BIND_SHADER_RESOURCE, gd.tex_input, cfmt))
         {
            native_device->CreateShaderResourceView(gd.tex_input.get(), nullptr, gd.srv_input.put());
            gd.smaa_temps_w = w;
            gd.smaa_temps_h = h;
         }
      }
      if (!gd.srv_input)
         return;

      native_device_context->CopyResource(gd.tex_input.get(), canvas_res);

      // Scene alpha -> linear depth -> plane-deviation edge-ness in R16F; see Luma_MOH_DepthExtract.hlsl for why
      // this is an edge test rather than a depth rescale.
      if (pred_ok)
      {
         DrawStateStack<DrawStateStackType::Compute> pred_cs_state;
         pred_cs_state.Cache(native_device_context, device_data.uav_max_count);

         ID3D11ShaderResourceView* cs_srv = srv_scene;
         ID3D11UnorderedAccessView* cs_uav = gd.uav_pred.get();
         ID3D11Buffer* cs_cbs[2] = {gd.cb_pred.get(), game_cb4};
         native_device_context->CSSetShaderResources(0, 1, &cs_srv);
         native_device_context->CSSetUnorderedAccessViews(0, 1, &cs_uav, nullptr);
         native_device_context->CSSetConstantBuffers(0, 2, cs_cbs); // b0 tolerance, b1 the game's own cb4
         native_device_context->CSSetShader(pred_cs, nullptr, 0);
         native_device_context->Dispatch((w + 7) / 8, (h + 7) / 8, 1);

         pred_cs_state.Restore(native_device_context);
      }

#if DEVELOPMENT
      // Calibration aid, driven from the Anti-Aliasing section. Reads the mask that was just written.
      if (pred_ok && g_smaa_pred_debug)
      {
         auto* copy_vs = FindShader(device_data.native_vertex_shaders, CompileTimeStringHash("Copy VS"));
         auto* copy_ps = FindShader(device_data.native_pixel_shaders, CompileTimeStringHash("Copy PS"));
         if (copy_vs != nullptr && copy_ps != nullptr)
         {
            // The mask is single-channel, so the core copy lands it in RED - unmistakably a debug view. Replaces
            // the antialiased frame rather than blending over it, hence the early return.
            DrawStateStack<DrawStateStackType::FullGraphics> debug_state;
            debug_state.Cache(native_device_context, device_data.uav_max_count);
            DrawCustomPixelShader(native_device_context, device_data.default_depth_stencil_state.get(), device_data.default_blend_state.get(), nullptr,
               copy_vs, copy_ps, gd.srv_pred.get(), canvas_rtv, w, h, false);
            debug_state.Restore(native_device_context);
            return;
         }
      }
#endif

      // Metrics CB at VS+PS b1 (DrawSMAA restores VS/PS/SRVs/RTs, but not cbuffers).
      ComPtr<ID3D11Buffer> vs_cb1_orig, ps_cb1_orig;
      native_device_context->VSGetConstantBuffers(1, 1, vs_cb1_orig.put());
      native_device_context->PSGetConstantBuffers(1, 1, ps_cb1_orig.put());
      ID3D11Buffer* mcb = gd.cb_smaa_metrics.get();
      native_device_context->VSSetConstantBuffers(1, 1, &mcb);
      native_device_context->PSSetConstantBuffers(1, 1, &mcb);

      // Reading the canvas as the target is safe: the chain samples the snapshot, never the canvas itself.
      DrawSMAA(native_device, native_device_context, device_data, do_sharpen ? gd.tex_smaa_out_rtv.get() : canvas_rtv, gd.srv_input.get(), gd.srv_input.get(), pred_ok ? gd.srv_pred.get() : nullptr /*predication signal*/);

      // RCAS on the SMAA output, written into the canvas.
      if (do_sharpen)
      {
         DrawStateStack<DrawStateStackType::FullGraphics> sharpen_state;
         sharpen_state.Cache(native_device_context, device_data.uav_max_count);

         ID3D11Buffer* scb = gd.cb_sharpen.get();
         native_device_context->PSSetConstantBuffers(0, 1, &scb);
         DrawCustomPixelShader(native_device_context, device_data.default_depth_stencil_state.get(), device_data.default_blend_state.get(), nullptr,
            sharpen_vs, sharpen_ps, gd.tex_smaa_out_srv.get(), canvas_rtv, w, h, false);

         sharpen_state.Restore(native_device_context);
      }

      ID3D11Buffer* vcb = vs_cb1_orig.get();
      ID3D11Buffer* pcb = ps_cb1_orig.get();
      native_device_context->VSSetConstantBuffers(1, 1, &vcb);
      native_device_context->PSSetConstantBuffers(1, 1, &pcb);
   }
#endif // ENABLE_SMAA

   // dgVoodoo sometimes leaves blending ENABLED on a secondary render target while RT0 has it off. D3D9 has one
   // global blend state and only per-RT write masks (D3DRS_COLORWRITEENABLE1/2/3), so the game never asked for it
   // and that target is corrupted. Diagnosed in TW2 on the same wrapper (water PS 0xDA16C815: RT1 is the linear
   // depth fog reads, so src_alpha IS the depth). Preventive here, as in Airborne; the DEVELOPMENT log reports if
   // it occurs at all -- no multi-target draw has been seen in this game's captures so far.
   // Repair = copy RT0's blend fields onto the offending targets; write masks stay, legal per-RT in D3D9. The
   // inverse shape is only reported: enabling blending where the wrapper left it off could only add damage.
   // Returns true iff it ran the original draw itself (caller returns Replaced).
   static bool FixImpossiblePerRTBlend(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, MedalOfHonorGameDeviceData& gd, reshade::api::shader_stage stages, const ShaderHashesList<OneShaderPerPipeline>& original_shader_hashes, bool is_custom_pass, std::function<void()>* original_draw_dispatch_func)
   {
      // Our own injected passes set their blend state deliberately, and hash-replaced passes report as custom too.
      // Re-issuing the draw is the only way to apply a different state, so without that callback there is nothing
      // to do.
      if (is_custom_pass || (stages & reshade::api::shader_stage::pixel) == 0 || original_draw_dispatch_func == nullptr)
         return false;

      ComPtr<ID3D11BlendState> blend_state;
      FLOAT blend_factor[4];
      UINT sample_mask = 0;
      native_device_context->OMGetBlendState(blend_state.put(), blend_factor, &sample_mask);
      if (!blend_state)
         return false; // no state object = default (blending off everywhere)

      D3D11_BLEND_DESC bd;
      blend_state->GetDesc(&bd);
      if (!bd.IndependentBlendEnable)
         return false; // one state for all targets: already D3D9-shaped

      const bool rt0_blending = bd.RenderTarget[0].BlendEnable != FALSE;
#if !DEVELOPMENT
      // Only the "RT0 off, RTn on" shape is ever repaired, so outside DEVELOPMENT (where the inverse shape is
      // logged) a blending RT0 has nothing to do here: skip the descriptor scan and the render-target query both.
      if (rt0_blending)
         return false;
#endif

      // Only BOUND targets count: the wrapper leaves stale BlendEnable in the unused slots, so the descriptor
      // alone matches nearly every single-target draw and this would take over passes belonging to other hooks.
      // The descriptor is scanned first because it is free.
      bool disagreement = false;
      for (UINT i = 1; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT && !disagreement; i++)
         disagreement = bd.RenderTarget[i].BlendEnable != bd.RenderTarget[0].BlendEnable;
      if (!disagreement)
         return false;

      ID3D11RenderTargetView* rtvs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = {};
      native_device_context->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, rtvs, nullptr);
      bool bound[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = {};
      for (UINT i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
      {
         bound[i] = rtvs[i] != nullptr;
         if (rtvs[i])
            rtvs[i]->Release(); // OMGetRenderTargets hands back references; only the bound/not-bound answer is kept
      }

      // RT0's blend bit is loop-invariant, so the two shapes are mutually exclusive: one flag out of the loop.
      bool bound_disagreement = false;
      for (UINT i = 1; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT && !bound_disagreement; i++)
         bound_disagreement = bound[i] && bd.RenderTarget[i].BlendEnable != bd.RenderTarget[0].BlendEnable;
      const bool needs_fix = bound_disagreement && !rt0_blending;
      [[maybe_unused]] const bool inverse_shape = bound_disagreement && rt0_blending;

#if DEVELOPMENT
      // One line per distinct shader, so a play session reports every pass that carries this.
      if (needs_fix || inverse_shape)
      {
         // Locked: this function deliberately runs on every context (see its header), so two can reach the set.
         static std::mutex logged_shaders_mutex;
         static std::unordered_set<uint64_t> logged_shaders;
         const uint64_t pixel_shader_hash = original_shader_hashes.pixel_shaders[0];
         const std::scoped_lock logged_shaders_lock(logged_shaders_mutex);
         if (logged_shaders.emplace(pixel_shader_hash).second)
         {
            reshade::log::message(reshade::log::level::warning,
               std::format("[MOH-BlendFix] impossible per-RT blend state (dgVoodoo artefact) on pixel shader 0x{:X} - {}", pixel_shader_hash, needs_fix ? "repaired" : "inverse shape, left alone").c_str());
         }
      }
#endif

      if (!needs_fix)
         return false;

      ComPtr<ID3D11BlendState> fixed_state;
      if (const auto it = gd.fixed_blend_states.find(bd); it != gd.fixed_blend_states.end())
      {
         fixed_state = it->second;
      }
      else
      {
         D3D11_BLEND_DESC fixed_desc = bd;
         for (UINT i = 1; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
         {
            if (fixed_desc.RenderTarget[i].BlendEnable == bd.RenderTarget[0].BlendEnable)
               continue;
            const UINT8 write_mask = fixed_desc.RenderTarget[i].RenderTargetWriteMask; // legal per-RT in D3D9, keep it
            fixed_desc.RenderTarget[i] = bd.RenderTarget[0];
            fixed_desc.RenderTarget[i].RenderTargetWriteMask = write_mask;
         }
         if (FAILED(native_device->CreateBlendState(&fixed_desc, fixed_state.put())) || !fixed_state)
            return false; // leave the draw untouched rather than run it half-applied
         gd.fixed_blend_states[bd] = fixed_state;
      }

      native_device_context->OMSetBlendState(fixed_state.get(), blend_factor, sample_mask);
      (*original_draw_dispatch_func)();
      native_device_context->OMSetBlendState(blend_state.get(), blend_factor, sample_mask); // hand the game back its own state
      return true;
   }

public:
   void OnInit(bool async) override
   {
      // Game-specific toggle consumed by the replaced pass (Luma_MOH_Tonemap.hlsl).
      std::vector<ShaderDefineData> game_shader_defines_data = {
         {"TONEMAP_TYPE", '1', true, false, "0 - SDR: Vanilla (bit-exact reference)\n1 - HDR: recover highlights + DICE display map"},
      };
      shader_defines_data.append_range(game_shader_defines_data);
      assert(shader_defines_data.size() < MAX_SHADER_DEFINES);

      // Post-process buffers stay in GAMMA space: the HUD blends src-alpha onto the same canvas the tonemap writes,
      // and a linear buffer washes it out. The replacement pre-scales by GamePaperWhite/UIPaperWhite (UI_DRAW_TYPE 2);
      // the core Display Composition decodes, applies paper white, encodes scRGB and gamut-maps at present.
      GetShaderDefineData(POST_PROCESS_SPACE_TYPE_HASH).SetDefaultValue('0');
      GetShaderDefineData(EARLY_DISPLAY_ENCODING_HASH).SetDefaultValue('0');
      GetShaderDefineData(VANILLA_ENCODING_TYPE_HASH).SetDefaultValue('1');
      GetShaderDefineData(GAMMA_CORRECTION_TYPE_HASH).SetDefaultValue('1');
      GetShaderDefineData(GAMUT_MAPPING_TYPE_HASH).SetDefaultValue('1');
      GetShaderDefineData(UI_DRAW_TYPE_HASH).SetDefaultValue('2');

      // dgVoodoo binds b0-b5 only (measured on every captured draw of this game), so b12/b13 are free for Luma.
      luma_settings_cbuffer_index = 13;
      luma_data_cbuffer_index = 12;
      luma_ui_cbuffer_index = -1; // the game draws its own UI onto the canvas

      // Manual Scene + UI Paper White sliders instead of the OS HDR reference level.
      use_os_reference_white_level = false;

      // User grade controls (read in Luma_MOH_Tonemap.hlsl via LumaSettings.GameSettings). All vanilla by default.
      default_luma_global_game_settings.Exposure = 1.f;
      default_luma_global_game_settings.Saturation = 1.f;
      default_luma_global_game_settings.HighlightDechroma = 0.f; // off; only the mandatory DICE/gamut desaturation applies
      default_luma_global_game_settings.Contrast = 1.f;
      default_luma_global_game_settings.Dithering = 1.f; // subtle anti-banding on by default
      // Read only by the legacy DevSetting04 A/B in the tonemap; no shipped path touches them (see NOTES section 20).
      default_luma_global_game_settings.HighlightsHueStrength = 0.8f;
      default_luma_global_game_settings.HighlightsHueChroma = 0.f;
      default_luma_global_game_settings.LumaBloomEnable = ENABLE_BLOOM ? 1.f : 0.f;
      default_luma_global_game_settings.SMAAEnable = ENABLE_SMAA ? 1.f : 0.f; // stands the engine's EdgeAA blur down
      default_luma_global_game_settings.BloomIntensity = 1.f;
      // A FRACTION OF SDR WHITE, not an absolute scene value: the prefilter reads the game's own SceneExposure.z
      // and resolves 1.0 to the exact scene brightness the vanilla tonemap clips at. So 1.0 is a derived default,
      // not a guess, and it tracks the engine's per-area exposure without the user touching anything.
      default_luma_global_game_settings.BloomThreshold = 1.f;
      cb_luma_global_settings.GameSettings = default_luma_global_game_settings;
   }

   void OnCreateDevice(ID3D11Device* native_device, DeviceData& device_data) override
   {
      device_data.game = new MedalOfHonorGameDeviceData;
   }

   // Core never frees "device_data.game" itself, and GameDeviceData has no virtual destructor, so delete through
   // the concrete type.
   void OnDestroyDeviceData(DeviceData& device_data) override
   {
      // Core's own device teardown asserts this map is already empty (core.hpp, OnDestroyDevice), because it expects
      // every sampler to have been announced through OnDestroySampler first. dgVoodoo does not: it keeps its samplers
      // alive until the device itself is released, and it creates and destroys several short-lived devices while
      // enumerating video modes at boot. The result is a modal assertion box during startup that this game cannot
      // dismiss (the same reason DISABLE_AUTO_DEBUGGER is set at the top of this file), so the mod never launches.
      //
      // Releasing them here is what core does one line AFTER that assertion anyway, only early enough to matter, and
      // it leaks nothing: the map owns strong references to samplers WE created, and the device is going away. This
      // callback runs before the assertion, which is the whole reason the fix can live here instead of in core.
      // Core is not modified; the mismatched invariant is written up in NOTES.md section 19.
      {
         const std::unique_lock lock_samplers(s_mutex_samplers);
         device_data.custom_sampler_by_original_sampler.clear();
      }

      delete static_cast<MedalOfHonorGameDeviceData*>(device_data.game);
      device_data.game = nullptr;
   }

   DrawOrDispatchOverrideType OnDrawOrDispatch(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, CommandListData& cmd_list_data, DeviceData& device_data, reshade::api::shader_stage stages, const ShaderHashesList<OneShaderPerPipeline>& original_shader_hashes, bool is_custom_pass, bool& updated_cbuffers, std::function<void()>* original_draw_dispatch_func) override
   {
      auto& gd = GetGameDeviceData(device_data);

      // Hide HUD: once this frame's tonemap has run, the HUD is the only ALPHA-BLENDED geometry left. Everything
      // between the tonemap and the HUD -- bright pass, pyramid, composite, DoF -- is an opaque full-screen write
      // (the vanilla composite does its screen blend in the shader, not in the ROP), so the blend flag separates
      // them cleanly. Keying on blend rather than on the render target is what this game needs: the tonemap writes
      // 0xCBD86D64 while the HUD lands on 0x3C404864, dgVoodoo's emulated D3D9 backbuffer, so Airborne's
      // canvas-identity compare would match nothing here.
      // Safe against blanking the frame: the pass that carries that canvas into the real swapchain is a
      // CopyResource, not a draw, so it never reaches this callback and cannot be cancelled.
      const bool is_immediate = native_device_context->GetType() == D3D11_DEVICE_CONTEXT_IMMEDIATE;

#if DEVELOPMENT || TEST
      // One store per draw, no device queries: the ring is only read on a frame that ends untonemapped.
      if (is_immediate && !is_custom_pass)
      {
         gd.draw_ring[gd.draws_this_frame % MedalOfHonorGameDeviceData::kDrawRingSize] = (uint32_t)original_shader_hashes.pixel_shaders[0];
         ++gd.draws_this_frame;
      }
#endif

      if (g_hide_ui && !is_custom_pass && gd.has_drawn_tonemap && is_immediate)
      {
         ComPtr<ID3D11BlendState> blend_state;
         FLOAT blend_factor[4] = {};
         UINT sample_mask = 0;
         native_device_context->OMGetBlendState(blend_state.put(), blend_factor, &sample_mask);
         if (blend_state)
         {
            D3D11_BLEND_DESC bd = {};
            blend_state->GetDesc(&bd);
            if (bd.RenderTarget[0].BlendEnable)
            {
#if DEVELOPMENT || TEST
               // Name what is being cancelled: a bloom or DoF hash in this list would mean the blend argument above
               // is wrong for this frame and those hashes need excluding.
               if (g_hide_ui_log_budget > 0)
               {
                  --g_hide_ui_log_budget;
                  std::stringstream s;
                  s << "[Luma] MoH 2010 Hide UI cancelled a draw: PS 0x" << std::hex << std::uppercase << original_shader_hashes.pixel_shaders[0];
                  reshade::log::message(reshade::log::level::info, s.str().c_str());
               }
#endif
               return DrawOrDispatchOverrideType::Replaced;
            }
         }
      }

      // Read the hash list before any early-out: is_custom_pass is true for hash-replaced passes too, so gating on
      // it would never see our own replacement. Nothing below returns unless it has taken the draw over, because the
      // wrapper blend repair at the end of the function has to be reached on EVERY draw, not only on ours.
      const bool is_tonemap = IsTonemap(original_shader_hashes);
      const bool is_bright = IsBloomBrightPass(original_shader_hashes);
      const bool is_composite = IsBloomComposite(original_shader_hashes);

      if (is_tonemap)
         gd.ever_matched_tonemap = true; // ahead of the immediate-context gate: telemetry must see the pass either way

      // Shared by all three replaced passes.
      if (is_immediate && (is_tonemap || is_bright || is_composite))
      {
         // Exactly one tonemap perm runs per frame; that pass ends the main post processing (the Display Composition
         // and the core's UI/paper-white handling key off it). The composite must not move this flag: it runs after.
         if (is_tonemap && !gd.has_drawn_tonemap)
         {
            gd.has_drawn_tonemap = true;
            device_data.has_drawn_main_post_processing = true;
         }

         // Upload BOTH: once updated_cbuffers is set core skips its own upload, and sending only LumaData would leave
         // LumaSettings (b13) zeroed -> GamePaperWhiteNits 0 -> the UI prescale blacks the screen.
         SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, stages, LumaConstantBufferType::LumaSettings);
         SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, stages, LumaConstantBufferType::LumaData);
         updated_cbuffers = true;

#if DEVELOPMENT || TEST
         // Diagnostic only: watch the engine's own bloom constants. cb4[8].x is the bright pass' threshold, cb4[8].z
         // the composite's amount -- both in the GAMMA canvas domain, so neither can be used directly for the Luma
         // pyramid (its threshold lives in raw linear scene units). Only their VARIATION between areas is meaningful.
         {
            float row[4] = {};
            if (is_tonemap)
            {
               // Row 8 is SceneExposure HERE, and the bloom threshold / amount at the two passes below. One index,
               // three meanings; the pass decides which.
               if (CaptureCb4Row(native_device, native_device_context, gd.rb_tonemap, 8, row))
                  gd.scene_exposure = row[2];
            }
            else if (is_bright)
            {
               if (CaptureCb4Row(native_device, native_device_context, gd.rb_bright, 8, row))
                  gd.engine_bloom_threshold = row[0];
            }
            else if (is_composite)
            {
               if (CaptureCb4Row(native_device, native_device_context, gd.rb_composite, 8, row))
                  gd.engine_bloom_amount = row[2];
            }
         }
#endif
      }

      // Tonemap-only injections, in their own block so the shared one above does not have to re-test the pass.
      if (is_immediate && is_tonemap)
      {

         // The game's cb4, live on this draw. Two injected passes read it: the bloom prefilter takes
         // SceneExposure.z (cb4[8].z) to anchor its threshold to SDR white, and the predication CS takes the depth
         // linearisation pair (cb4[10].zw). Row 8 is declared by all four permutations; row 10 only by the
         // motion-blur ones, which is why predication gates on that separately below.
         ComPtr<ID3D11Buffer> game_cb4;
         native_device_context->PSGetConstantBuffers(4, 1, game_cb4.put());

#if ENABLE_BLOOM
         {
            // Luma HDR bloom pyramid off the fp16 LINEAR scene at t0 — the same texture this draw is about to read.
            // That source is pre-glow by construction (this engine builds its halo downstream, out of the canvas this
            // very pass writes), which is exactly what makes the port viable. Karis average first: no TAA here, so
            // fireflies have to die spatially. Full-graphics state stack because DrawBloom overwrites the render
            // target, shaders and SRVs this draw still needs.
            gd.srv_luma_bloom.reset(); // DrawBloom AddRef's its mip 0 into this
            ComPtr<ID3D11ShaderResourceView> srv_scene;
            native_device_context->PSGetShaderResources(0, 1, srv_scene.put());
            if (g_luma_bloom_enable && srv_scene)
            {
               DrawStateStack<DrawStateStackType::FullGraphics> bloom_state;
               bloom_state.Cache(native_device_context, device_data.uav_max_count);

               // The prefilter reads BloomThreshold from LumaSettings at b13; the upload above already pushed it, but
               // the state stack restores whatever the game had, so it has to be pushed again inside the scope.
               SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, reshade::api::shader_stage::pixel, LumaConstantBufferType::LumaSettings);

               // ...and the game's own cb4, because that threshold is a FRACTION OF SDR WHITE and the prefilter
               // needs SceneExposure.z to know where SDR white is. DrawBloom backs up and restores only b11, so
               // everything else bound here reaches its passes. Bound explicitly rather than inherited: this draw
               // already has it at b4, but relying on that is the documented way this breaks silently.
               ID3D11Buffer* prefilter_cb4 = game_cb4.get();
               native_device_context->PSSetConstantBuffers(4, 1, &prefilter_cb4);

               ComPtr<ID3D11ShaderResourceView> srv_karis;
               DrawKarisAverage(native_device, native_device_context, device_data, srv_scene.get(), srv_karis.put());
               if (srv_karis)
                  DrawBloom(native_device, native_device_context, device_data, srv_karis.get(), kBloomNMips, g_bloom_sigmas, gd.srv_luma_bloom.put());

               bloom_state.Restore(native_device_context);
            }
            // Bound every frame, null included: the tonemap gates on LumaBloomEnable rather than on what happens to
            // be in the slot, and leaving dgVoodoo's 1x1 placeholder there would be sampling garbage.
            ID3D11ShaderResourceView* bloom_srv = gd.srv_luma_bloom.get();
            native_device_context->PSSetShaderResources(kLumaBloomSlot, 1, &bloom_srv);
         }
#endif

#if ENABLE_SMAA
         // Run the tonemap ourselves, then SMAA on its output, so the antialiasing lands before the bloom chain
         // reads the canvas and long before the HUD is drawn on it. Falls back to a plain draw when the callback
         // is unavailable (a frame without AA, which recovers on the next one) rather than skipping the tonemap.
         if (g_smaa_enable && original_draw_dispatch_func != nullptr)
         {
            ComPtr<ID3D11RenderTargetView> canvas_rtv;
            native_device_context->OMGetRenderTargets(1, canvas_rtv.put(), nullptr);
            ComPtr<ID3D11Resource> canvas_res;
            if (canvas_rtv)
               canvas_rtv->GetResource(canvas_res.put());
            ComPtr<ID3D11ShaderResourceView> srv_scene;
            native_device_context->PSGetShaderResources(0, 1, srv_scene.put());

            // Only the motion-blur permutations declare cb4[10], so anything else hands the CS a null and
            // predication falls back to plain ULTRA -- mask and threshold scale together.
            const bool mb_perm = original_shader_hashes.Contains(kTonemapMotionBlurHash, reshade::api::shader_stage::pixel) || original_shader_hashes.Contains(kTonemapEdgeAAMotionHash, reshade::api::shader_stage::pixel);
            ID3D11Buffer* pred_cb4 = mb_perm ? game_cb4.get() : nullptr;
            gd.depth_cb_live = pred_cb4 != nullptr;

            if (canvas_res)
            {
               (*original_draw_dispatch_func)();
               RunPostTonemapSMAA(native_device, native_device_context, device_data, gd, canvas_res.get(), canvas_rtv.get(), srv_scene.get(), pred_cb4);
               return DrawOrDispatchOverrideType::Replaced; // we ran the original draw ourselves
            }
         }
#endif
      }

      // Wrapper blend repair, LAST on purpose: it re-issues the draw itself, so it must yield to every hook above
      // or it runs vanilla a pass another hook meant to take over — which is how it silently disabled a hook in
      // TW2. Not gated on the immediate context: blend state belongs to the context it is set on.
      if (FixImpossiblePerRTBlend(native_device, native_device_context, gd, stages, original_shader_hashes, is_custom_pass, original_draw_dispatch_func))
         return DrawOrDispatchOverrideType::Replaced;

      return DrawOrDispatchOverrideType::None; // the passes themselves are replaced by hash, never cancelled
   }

   void OnPresent(ID3D11Device* native_device, DeviceData& device_data) override
   {
      auto& gd = GetGameDeviceData(device_data);

      // One-shot telemetry: the menu runs the same pass, so a short warmup is enough to tell a wrong wrapper build
      // from "not in gameplay yet".
      // Long on purpose. 120 frames elapse during the intro and the menu, before the game has drawn a single
      // tonemapped scene, so a short warmup reported "every shader replacement is inactive" on a perfectly healthy
      // session. The wrapper-build failure this watches for is permanent, so waiting costs nothing.
      constexpr uint32_t kBuildCheckFrame = 3000;
      if (!gd.build_check_done && ++gd.frames_presented >= kBuildCheckFrame)
      {
         gd.build_check_done = true;
         if (!gd.ever_matched_tonemap)
            reshade::log::message(reshade::log::level::warning,
               "[Luma] MoH 2010: no keyed tonemap pass seen after warmup -- the dgVoodoo build is probably not 2.87.3, so every shader replacement is inactive (re-dump the shaders for it).");
      }

#if DEVELOPMENT || TEST
      // Log only on a real move (1% relative, or the first capture), so walking through the level leaves a
      // readable trail in ReShade.log instead of one line per frame.
      {
         auto moved = [](float now, float logged)
         {
            if (now < 0.f)
               return false; // never captured
            if (logged < -1.f)
               return true; // first capture
            return abs(now - logged) > 0.01f * max(abs(logged), 1e-4f);
         };
         if (moved(gd.engine_bloom_threshold, gd.engine_bloom_threshold_logged) || moved(gd.engine_bloom_amount, gd.engine_bloom_amount_logged) || moved(gd.scene_exposure, gd.scene_exposure_logged))
         {
            gd.engine_bloom_threshold_logged = gd.engine_bloom_threshold;
            gd.engine_bloom_amount_logged = gd.engine_bloom_amount;
            gd.scene_exposure_logged = gd.scene_exposure;
            std::stringstream s;
            s << "[Luma] MoH 2010 engine constants: SceneExposure.z " << gd.scene_exposure
              << " (SDR white at scene " << (gd.scene_exposure > 1e-4f ? 1.7f / gd.scene_exposure : -1.f) << ")"
              << " | bloom threshold (cb4[8].x) " << gd.engine_bloom_threshold
              << " | bloom amount (cb4[8].z) " << gd.engine_bloom_amount;
            reshade::log::message(reshade::log::level::info, s.str().c_str());
         }
      }

      // AF census (one-shot, MEA precedent): dump the samplers core actually rewrote under samplers_upgrade_mode 4.
      // An EMPTY map is the answer that matters -- it means dgVoodoo handed D3D11 no anisotropic sampler at all and
      // mode 4 is a silent no-op, which is the only thing that would justify "force_upgrade_linear_samplers".
      if (!gd.logged_samplers)
      {
         std::shared_lock sl(s_mutex_samplers);
         if (!device_data.custom_sampler_by_original_sampler.empty())
         {
            gd.logged_samplers = true;
            int total = 0, shown = 0;
            for (const auto& by_orig : device_data.custom_sampler_by_original_sampler)
            {
               for (const auto& by_bias : by_orig.second)
               {
                  ++total;
                  if (by_bias.second && shown < 4)
                  {
                     ++shown;
                     D3D11_SAMPLER_DESC d = {};
                     by_bias.second->GetDesc(&d);
                     std::stringstream s;
                     s << "[Luma] MoH 2010 AF: custom sampler filter " << (int)d.Filter << " (85 = ANISOTROPIC) | maxAniso " << d.MaxAnisotropy
                       << " | mipBias " << d.MipLODBias << " | minLOD " << d.MinLOD;
                     reshade::log::message(reshade::log::level::info, s.str().c_str());
                  }
               }
            }
            std::stringstream s;
            s << "[Luma] MoH 2010 AF: " << total << " upgraded sampler(s) active";
            reshade::log::message(reshade::log::level::info, s.str().c_str());
         }
      }
#endif

#if ENABLE_BLOOM
      gd.srv_luma_bloom.reset(); // rebuilt at the tonemap pass every frame; never held across one

      // Core's Karis buffer is the only pyramid allocation reachable from here: DrawBloom keeps its own mips in
      // function-local statics. Unconditional while off — resetting already-empty entries is two map lookups.
      if (!g_luma_bloom_enable)
         ReleaseCoreKarisAverage(device_data);
#endif
#if ENABLE_SMAA
      // Hand the address space back the moment a feature is switched off: this process is 32-bit, and the mod's
      // own snapshot (~66 MB at 4K) is only half of it — core's DrawSMAA intermediates are ~83 MB more, and core
      // drops those only on swapchain init, which a feature toggle is not. The latch on our own scratch keeps this
      // to one pass rather than every frame the feature stays off.
      if (!g_smaa_enable)
      {
         if (gd.tex_input)
         {
            gd.ReleaseSMAAScratch();
            ReleaseCoreSMAAIntermediates(device_data);
            gd.smaa_core_w = gd.smaa_core_h = 0; // core recreates lazily; do not let the latch claim they are current
         }
      }
      else
      {
         if (!g_smaa_predication)
            gd.ReleasePredicationScratch();
         if (g_rcas_sharpness <= 0.f)
            gd.ReleaseSharpenScratch();
      }
#endif

#if DEVELOPMENT || TEST
      // Cap the reports: this fires on every loading screen and every menu frame, and one episode is enough to
      // name the pass. Skip frames that drew almost nothing -- those are the genuinely empty ones (loading), not
      // a scene going out unbounded.
      constexpr uint32_t kMaxUntonemappedReports = 4;
      constexpr uint32_t kMinInterestingDraws = 8;
      if (!gd.has_drawn_tonemap && gd.draws_this_frame >= kMinInterestingDraws && gd.untonemapped_frames_logged < kMaxUntonemappedReports)
      {
         ++gd.untonemapped_frames_logged;
         const uint32_t count = min(gd.draws_this_frame, MedalOfHonorGameDeviceData::kDrawRingSize);
         const uint32_t first = gd.draws_this_frame - count;
         std::stringstream s;
         s << "[Luma] MoH 2010 UNTONEMAPPED FRAME " << gd.frames_presented << ": " << gd.draws_this_frame
           << " draws, no keyed tonemap. Composition still ran, so this frame reached the display unbounded."
           << " Last " << count << " pixel shaders (oldest first): " << std::hex << std::uppercase;
         for (uint32_t i = 0; i < count; ++i)
            s << "0x" << gd.draw_ring[(first + i) % MedalOfHonorGameDeviceData::kDrawRingSize] << ' ';
         reshade::log::message(reshade::log::level::warning, s.str().c_str());
      }
      gd.draws_this_frame = 0;
#endif

      gd.has_drawn_tonemap = false;
      // Core never clears this one itself, so leaving it set would claim a scene was tonemapped on frames that ran
      // no tonemap at all (movies, loading).
      device_data.has_drawn_main_post_processing = false;
   }

   void LoadConfigs() override
   {
      // Grade sliders (cb_luma_global_settings_dirty is already true at init -> uploaded on the first frame).
      reshade::get_config_value(nullptr, NAME, "Exposure", cb_luma_global_settings.GameSettings.Exposure);
      reshade::get_config_value(nullptr, NAME, "Saturation", cb_luma_global_settings.GameSettings.Saturation);
      reshade::get_config_value(nullptr, NAME, "HighlightsDesaturation", cb_luma_global_settings.GameSettings.HighlightDechroma);
      reshade::get_config_value(nullptr, NAME, "Contrast", cb_luma_global_settings.GameSettings.Contrast);
      reshade::get_config_value(nullptr, NAME, "Dithering", cb_luma_global_settings.GameSettings.Dithering);
#if ENABLE_BLOOM
      reshade::get_config_value(nullptr, NAME, "LumaBloomEnable", g_luma_bloom_enable);
      cb_luma_global_settings.GameSettings.LumaBloomEnable = g_luma_bloom_enable ? 1.f : 0.f;
      reshade::get_config_value(nullptr, NAME, "BloomIntensity", cb_luma_global_settings.GameSettings.BloomIntensity);
      reshade::get_config_value(nullptr, NAME, "BloomThreshold", cb_luma_global_settings.GameSettings.BloomThreshold);
#endif
#if ENABLE_SMAA
      reshade::get_config_value(nullptr, NAME, "SMAAEnable", g_smaa_enable);
      cb_luma_global_settings.GameSettings.SMAAEnable = g_smaa_enable ? 1.f : 0.f;
      reshade::get_config_value(nullptr, NAME, "SMAAPredication", g_smaa_predication);
      reshade::get_config_value(nullptr, NAME, "SMAAPredicationTolerance", g_smaa_pred_tolerance);
      reshade::get_config_value(nullptr, NAME, "RCASSharpness", g_rcas_sharpness);
#endif
   }

   void DrawImGuiSettings(DeviceData& device_data) override
   {
      auto& gs = cb_luma_global_settings.GameSettings;
      const auto& gs_def = default_luma_global_game_settings;

#if ENABLE_SMAA
      ImGui::SeparatorText("Anti-Aliasing");

      if (ImGui::Checkbox("SMAA Enable", &g_smaa_enable))
      {
         reshade::set_config_value(nullptr, NAME, "SMAAEnable", g_smaa_enable);
         gs.SMAAEnable = g_smaa_enable ? 1.f : 0.f;
         device_data.cb_luma_global_settings_dirty = true;
      }
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Replaces the game's EdgeAA with SMAA (sharper edges instead of a blur).");

      ImGui::BeginDisabled(!g_smaa_enable);

#if DEVELOPMENT
      // Predication is not a preference: it only relaxes the edge threshold back to base ULTRA on geometry and
      // never below, so off is strictly worse. Kept as a bisect switch for devs, shipped on and out of sight
      // (Airborne ships it the same way). The motion-blur precondition needs no user-facing warning either: the
      // fallback to plain SMAA is silent and harmless.
      if (ImGui::Checkbox("SMAA Predication", &g_smaa_predication))
         reshade::set_config_value(nullptr, NAME, "SMAAPredication", g_smaa_predication);
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
         ImGui::SetTooltip("Finds edges by geometry (scene depth) instead of by brightness alone.\nKeeps textures sharp while still antialiasing real silhouettes.\nNeeds the game's Motion Blur ON - cb4[10] is only declared by those permutations; otherwise plain SMAA.");

      if (ImGui::SliderFloat("SMAA Predication Tolerance", &g_smaa_pred_tolerance, 0.001f, 0.2f, "%.3f", ImGuiSliderFlags_Logarithmic))
         reshade::set_config_value(nullptr, NAME, "SMAAPredicationTolerance", g_smaa_pred_tolerance);
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
         ImGui::SetTooltip("Plane deviation counted as a full edge, as a fraction of view depth.\nThis is the ONLY lever - never touch SMAA_PREDICATION_THRESHOLD, which is unitless by construction.\nToo low: a grazing floor lights up. Too high: silhouettes go missing.");
      ImGui::Checkbox("SMAA Predication Debug View", &g_smaa_pred_debug);
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
         ImGui::SetTooltip("Show the predication mask (red) instead of the frame. Black on flat surfaces, white along silhouettes.");
      {
         const auto* gd_ui = static_cast<const MedalOfHonorGameDeviceData*>(device_data.game);
         const bool live = gd_ui != nullptr && gd_ui->depth_cb_live;
         ImGui::Text("  depth linearisation: %s", live ? "captured (predication live)" : "unavailable (plain SMAA)");
      }
#endif

      if (ImGui::SliderFloat("RCAS Sharpness", &g_rcas_sharpness, 0.f, 1.f, "%.2f"))
         reshade::set_config_value(nullptr, NAME, "RCASSharpness", g_rcas_sharpness);
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
         ImGui::SetTooltip("Sharpening applied on top of SMAA (0 = off).");

      ImGui::EndDisabled();
#endif // ENABLE_SMAA

      ImGui::SeparatorText("Grade");

      if (ImGui::SliderFloat("Exposure", &gs.Exposure, 0.f, 2.f))
         device_data.cb_luma_global_settings_dirty = true;
      if (ImGui::IsItemDeactivatedAfterEdit())
         reshade::set_config_value(nullptr, NAME, "Exposure", gs.Exposure);
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Overall image brightness (1 = vanilla).");
      if (DrawResetButton<float, false>(gs.Exposure, gs_def.Exposure, "Exposure"))
      {
         device_data.cb_luma_global_settings_dirty = true;
         reshade::set_config_value(nullptr, NAME, "Exposure", gs.Exposure);
      }

      if (ImGui::SliderFloat("Contrast", &gs.Contrast, 0.5f, 1.5f))
         device_data.cb_luma_global_settings_dirty = true;
      if (ImGui::IsItemDeactivatedAfterEdit())
         reshade::set_config_value(nullptr, NAME, "Contrast", gs.Contrast);
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Overall image contrast, HDR only (1 = vanilla).");
      if (DrawResetButton<float, false>(gs.Contrast, gs_def.Contrast, "Contrast"))
      {
         device_data.cb_luma_global_settings_dirty = true;
         reshade::set_config_value(nullptr, NAME, "Contrast", gs.Contrast);
      }

      if (ImGui::SliderFloat("Saturation", &gs.Saturation, 0.f, 2.f))
         device_data.cb_luma_global_settings_dirty = true;
      if (ImGui::IsItemDeactivatedAfterEdit())
         reshade::set_config_value(nullptr, NAME, "Saturation", gs.Saturation);
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Color saturation, HDR only (1 = vanilla).");
      if (DrawResetButton<float, false>(gs.Saturation, gs_def.Saturation, "Saturation"))
      {
         device_data.cb_luma_global_settings_dirty = true;
         reshade::set_config_value(nullptr, NAME, "Saturation", gs.Saturation);
      }

      if (ImGui::SliderFloat("Highlights Desaturation", &gs.HighlightDechroma, 0.f, 1.f))
         device_data.cb_luma_global_settings_dirty = true;
      if (ImGui::IsItemDeactivatedAfterEdit())
         reshade::set_config_value(nullptr, NAME, "HighlightsDesaturation", gs.HighlightDechroma);
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("How soon bright sources fade to neutral white, HDR only (0 = keep color at any brightness).");
      if (DrawResetButton<float, false>(gs.HighlightDechroma, gs_def.HighlightDechroma, "HighlightsDesaturation"))
      {
         device_data.cb_luma_global_settings_dirty = true;
         reshade::set_config_value(nullptr, NAME, "HighlightsDesaturation", gs.HighlightDechroma);
      }

#if DEVELOPMENT
      // LEGACY A/B ONLY, and guarded to match the shader: the only code that reads these is the DevSetting04
      // branch in Luma_MOH_Tonemap.hlsl, which is #if DEVELOPMENT. The shipped max-channel path needs no hue
      // restoration. Both go when that A/B is settled.
      if (ImGui::SliderFloat("Vanilla Clip Hue", &gs.HighlightsHueStrength, 0.f, 1.f, "%.2f"))
         device_data.cb_luma_global_settings_dirty = true;
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("hueStrength of the vanilla highlight emulation: how much of the vanilla clip's hue angle the recovered highlight adopts (0.8 default). Never changes saturation by itself. Keep it below 1.0 — the hue runs away as the helper's chrominance ratio goes singular and white-hot pixels turn cyan.");
      if (ImGui::SliderFloat("Vanilla Clip Whitening", &gs.HighlightsHueChroma, 0.f, 1.f, "%.2f"))
         device_data.cb_luma_global_settings_dirty = true;
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("chrominanceStrength of the same call: the fraction of the vanilla clip's own chroma loss that gets reproduced (0 = vanilla hue but our saturation). The only one of the two knobs that can whiten; DICE and Highlights Desaturation already whiten at peak, so raise this only against a measured A/B.");
#endif

#if ENABLE_BLOOM
      ImGui::SeparatorText("Bloom");

      if (ImGui::Checkbox("Luma Bloom Enable", &g_luma_bloom_enable))
      {
         reshade::set_config_value(nullptr, NAME, "LumaBloomEnable", g_luma_bloom_enable);
         gs.LumaBloomEnable = g_luma_bloom_enable ? 1.f : 0.f;
         device_data.cb_luma_global_settings_dirty = true;
      }
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Replaces the game's bloom with a wider, softer HDR bloom.");

      // Everything below drives the Luma pyramid and nothing else, so it all greys out with it: no slider here
      // can reach the game's own glow.
      ImGui::BeginDisabled(!g_luma_bloom_enable);

      if (ImGui::SliderFloat("Bloom Intensity", &gs.BloomIntensity, 0.f, 2.f))
         device_data.cb_luma_global_settings_dirty = true;
      if (ImGui::IsItemDeactivatedAfterEdit())
         reshade::set_config_value(nullptr, NAME, "BloomIntensity", gs.BloomIntensity);
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
         ImGui::SetTooltip("Bloom strength (1 = vanilla, 0 = none).");
      if (DrawResetButton<float, false>(gs.BloomIntensity, gs_def.BloomIntensity, "BloomIntensity"))
      {
         device_data.cb_luma_global_settings_dirty = true;
         reshade::set_config_value(nullptr, NAME, "BloomIntensity", gs.BloomIntensity);
      }

#if DEVELOPMENT || TEST
      if (ImGui::SliderFloat("Bloom Threshold", &gs.BloomThreshold, 0.f, 4.f, "%.2f"))
         device_data.cb_luma_global_settings_dirty = true;
      if (ImGui::IsItemDeactivatedAfterEdit())
         reshade::set_config_value(nullptr, NAME, "BloomThreshold", gs.BloomThreshold);
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
         ImGui::SetTooltip("Where the glow starts, as a FRACTION OF SDR WHITE.\n1.0 = exactly what the game's own tonemap clips. The prefilter resolves it from the live SceneExposure.z,\nso it follows the engine per-area exposure on its own. Near 0 the whole scene glows.");
      if (DrawResetButton<float, false>(gs.BloomThreshold, gs_def.BloomThreshold, "BloomThreshold"))
      {
         device_data.cb_luma_global_settings_dirty = true;
         reshade::set_config_value(nullptr, NAME, "BloomThreshold", gs.BloomThreshold);
      }

#endif // DEVELOPMENT || TEST

      ImGui::EndDisabled();

#if DEVELOPMENT || TEST
      // OUTSIDE the disabled scope on purpose: these are the ENGINE's constants, so they are read from the pass
      // that runs precisely when the Luma pyramid is off. Greying them out with it hides them exactly when they
      // are the thing being measured.
      // Both values live in the gamma canvas domain and cannot drive the Luma pyramid directly; only their
      // variation between locations decides whether per-area tracking is worth building. Also logged on change.
      {
         const auto* gd_ui = static_cast<const MedalOfHonorGameDeviceData*>(device_data.game);
         const float t = gd_ui != nullptr ? gd_ui->engine_bloom_threshold : -1.f;
         const float a = gd_ui != nullptr ? gd_ui->engine_bloom_amount : -1.f;
         const float e = gd_ui != nullptr ? gd_ui->scene_exposure : -1.f;
         ImGui::Text("  engine bloom: threshold %.4f | amount %.4f  (-1 = not captured)", t, a);
         ImGui::Text("  SceneExposure.z %.4f -> SDR white at scene %.4f", e, e > 1e-4f ? 1.7f / e : -1.f);
      }
#endif
#endif // ENABLE_BLOOM

      ImGui::SeparatorText("Effects");

      bool dithering = gs.Dithering > 0.5f;
      if (ImGui::Checkbox("Dithering", &dithering))
      {
         gs.Dithering = dithering ? 1.f : 0.f;
         device_data.cb_luma_global_settings_dirty = true;
         reshade::set_config_value(nullptr, NAME, "Dithering", gs.Dithering);
      }
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Reduces gradient banding.");

      ImGui::SeparatorText("UI");

      if (ImGui::Checkbox("Hide Gameplay UI", &g_hide_ui)) // Session-only: a stuck "on" would look like a broken HUD.
      {
#if DEVELOPMENT || TEST
         if (g_hide_ui)
            g_hide_ui_log_budget = 8;
#endif
      }
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Disables the in-game UI.");
   }

   void PrintImGuiAbout() override
   {
      ImGui::PushTextWrapPos(0.f);
      ImGui::Text(
         "Luma for \"Medal of Honor\" (2010) is open source and free.\n"
         "It adds native HDR, HDR bloom and SMAA anti-aliasing to the single player campaign.\n"
         "It runs through dgVoodoo2 (DirectX 9 -> 11), build 2.87.3.\n"
         "Do NOT run another HDR mod (e.g. RenoDX) alongside it.\n"
         "Thanks to the Luma team and contributors.");
      ImGui::PopTextWrapPos();

      ImGui::NewLine();
      static const std::string social_link = std::string("Join our \"HDR Den\" Discord ") + std::string(ICON_FK_SEARCH);
      if (ImGui::Button(social_link.c_str()))
      {
         // Unique link for Luma's HDR Den (tracks the origin of people joining); do not share for other purposes.
         static const std::string discord_link = std::string("https://discord.gg/J9fM") + std::string("3EVuEZ");
         ShellExecuteA(nullptr, "open", discord_link.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
      }
      static const std::string contributing_link = std::string("Contribute on Github ") + std::string(ICON_FK_FILE_CODE);
      if (ImGui::Button(contributing_link.c_str()))
         ShellExecuteA(nullptr, "open", "https://github.com/Filoppi/Luma-Framework", nullptr, nullptr, SW_SHOWNORMAL);

      ImGui::NewLine();
      ImGui::Text("Build Date: %s %s", __DATE__, __TIME__);

      ImGui::NewLine();
      ImGui::Text("Credits:"
                  "\n\nThird Party:"
                  "\nReShade"
                  "\nImGui"
                  "\nRenoDX (HDR tonemap method)"
                  "\nDICE (HDR tonemapper)"
                  "\nOklab (hue/chroma restoration)"
                  "\ndgVoodoo2 (DirectX 9 -> 11 wrapper, required)");
   }
};

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
   if (ul_reason_for_call == DLL_PROCESS_ATTACH)
   {
      const char* project_name = PROJECT_NAME;
      const char* cleared_project_name = (project_name[0] == '_') ? (project_name + 1) : project_name;

      uint32_t mod_version = 1;
      Globals::SetGlobals(cleared_project_name, "Medal of Honor (2010) Luma HDR mod", "", mod_version);
      Globals::DEVELOPMENT_STATE = Globals::ModDevelopmentState::WorkInProgress;

      // scRGB fp16 swapchain (the game's backbuffer is 8-bit).
      swapchain_format_upgrade_type = TextureFormatUpgradesType::AllowedEnabled;
      swapchain_upgrade_type = SwapchainUpgradeType::scRGB;

      // Exclusive fullscreen -> borderless. "force_borderless" covers LEAVING fullscreen too, so the window cannot
      // come back with a title bar after alt-tab.
      prevent_fullscreen_state = true;
      force_borderless = true;

      // Everything the tonemap writes into is r8g8b8a8, so without this the HDR write is clipped at 1.0 immediately.
      // Upgraded INDIRECTLY (a mirror substituted at bind): changing a dgVoodoo-internal resource's creation format
      // breaks the translator's bookkeeping and black-screens even the menus (Airborne/TW2 precedent).
      texture_format_upgrades_type = TextureFormatUpgradesType::AllowedEnabled;
      enable_indirect_texture_format_upgrades = true;
      enable_chain_indirect_texture_format_upgrades = ChainTextureFormatUpgradesType::DirectDependencies;
      texture_upgrade_formats = {
         reshade::api::format::r8g8b8a8_typeless, // tonemap target, bloom pyramid, DoF and the dgVoodoo canvas
      };
      // "No1Px" is mandatory under dgVoodoo: the wrapper binds 1x1 placeholders in every unused sampler slot and a
      // 1x1 trivially passes the aspect filter, so core would mirror those too (and assert in DEVELOPMENT).
      texture_format_upgrades_2d_size_filters = (uint32_t)TextureFormatUpgrades2DSizeFilters::SwapchainResolution | (uint32_t)TextureFormatUpgrades2DSizeFilters::SwapchainAspectRatio | (uint32_t)TextureFormatUpgrades2DSizeFilters::No1Px;

      // AF 16x. A floor, not an upgrade, on this machine: the engine ini already asks for 16, but DefaultCompat.ini
      // has tiers that hand low-end detections 1 or 4, and this pins all of them to 16.
      // Mode 4 rewrites only samplers the game already created as ANISOTROPIC, and the census in OnPresent measured
      // that there are none: dgVoodoo binds filter 21 MIN_MAG_MIP_LINEAR, 20 MIN_MAG_LINEAR_MIP_POINT and 0
      // MIN_MAG_MIP_POINT, all with MaxAnisotropy 0. Exactly the TW2 census, so mode 4 alone is a no-op and
      // "force_upgrade_linear_samplers" is load bearing here too, as it is in Airborne and TW2 on this wrapper.
      // (The engine ini asking for MaxAnisotropy 16 turns out to say nothing about what the wrapper emits.)
      // It converts every MIN_MAG_MIP_LINEAR sampler, the 32-cubed 3D LUT at t33 included; anisotropy degenerates to
      // trilinear when the derivatives are uniform, which they are on a full-screen quad, so watch the grade for
      // softening at colour discontinuities and drop this line if it appears.
      // Mip LOD bias stays 0: no TAA here, so a negative bias would buy sharpness and pay in shimmer.
      enable_samplers_upgrade = true; // boot-time only (cannot be changed after device creation)
      samplers_upgrade_mode = 4;
      force_upgrade_linear_samplers = true;

      game = new MedalOfHonor();
   }

   CoreMain(hModule, ul_reason_for_call, lpReserved);

   return TRUE;
}
