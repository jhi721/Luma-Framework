#define GAME_MIRRORS_EDGE 1

#define GEOMETRY_SHADER_SUPPORT 0
#define DISABLE_AUTO_DEBUGGER 1
// The UI and LDR overlay passes get their UNORM clamp back in place (see "PatchShaderBytecodeSync")
#define LUMA_PATCH_BYTECODE_SYNC 1
// SMAA ULTRA (+ RCAS) right after TdToneMapping (see "RunPostTonemapSMAA"); Core registers its 6 passes from Luma_SMAA_impl.hlsl
#define ENABLE_SMAA 1
#define ENABLE_RCAS 1
// SMAA's area texture without the U-shape smoothing (see Luma_SMAA_impl.hlsl)
#define SMAA_SMOOTH_U_SHAPES 0
// Makes "original_draw_dispatch_func" non-null: the tonemap draw runs first, then SMAA on its output; the scene's draws run with the
// motion vector shaders
#define ENABLE_POST_DRAW_DISPATCH_CALLBACK 1
// The motion vector draw key reads the draw's arguments ("last_draw_dispatch_data")
#define ENABLE_DRAW_DISPATCH_DATA_CACHE 1
// A "Super Resolution" choice next to the SR bridge's DLSS, FSR and XeSS, drawn in process on any GPU
#define ENABLE_LUMA_TAA 1

#include "..\..\Core\core.hpp"
#include "..\..\External\WDK\includes\d3d11TokenizedProgramFormat.hpp"
#include "MotionVectorPatches.h"
#include "..\..\Core\includes\patched_draws.h"
#include "..\..\Core\includes\coverage_patch.h"

// Mirror's Edge (2008, Steam) is x86 UE3 on D3D9: it runs through dgVoodoo (D3D9 -> D3D11), so every hash below is a dgVoodoo
// 2.87.5 (ps_5_0) and 2.81.3 (ps_4_0, the Linux build) translation, mapped offline from the cooked RefShaderCache (NOTES.md).
// 2.87.5 hashes match the game; 2.81.3 ones reproduce BL2's Bink (0x7EBF990A).
// TdToneMapping is the only grade of the post chain (HDR -> LDR); GammaCorrection replaces it when the "TdTonemapping" system setting is off.
const ShaderHashesList shader_hashes_ToneMapping = {.pixel_shaders = {0x1A760388, 0x602DA054}};
// Passes drawn onto the post-tonemap canvas, which used to be UNORM: it clamped their output (dgVoodoo drops the SM3 _sat). Pairs are
// 2.87.5, 2.81.3 (Source/Games/Mirror's Edge/NOTES.md, UI table). Each has a single o0.xyzw output and a single final ret.
// The Td UI blur and alpha passes write the fp16 UI target, which never clamped: no clamp.
// UI drawn straight onto the canvas: saturate, as the UNORM store did.
// Simple Element, Simple Element Gamma (HUD, text), Simple Element Masked Gamma, menu panels
const std::unordered_set<uint32_t> ui_saturated_pixel_shaders = {0x088F76F5, 0x889B0C84, 0xD81C8836, 0x3DFF6336, 0xC39EFBF2, 0x5325C38B, 0xACB81A27, 0x6B9A89FF};
// Passes over the scene they read: rgb floored at 0 and alpha saturated (blending's inputs, as the UNORM store clamped them), no
// upper rgb clamp, which would clip the HDR scene under them.
// Td UI Compositing (lerp(scene, UI, UI.a)), pause background blur, sniper scope, hurt filter, speed lines
const std::unordered_set<uint32_t> scene_blend_input_pixel_shaders = {0xA5AC2B70, 0xFB0A84E8, 0x713B6AF9, 0xBFF59A82, 0x946977C0, 0xC982CDCC, 0x70AD2D05, 0x15B360EC, 0x26F03F7F, 0x63D0CA51};
const ShaderHashesList shader_hashes_GammaCorrection = {.pixel_shaders = {0x0160196C, 0x87D136D3}};
// The last scene pass: it writes the canvas the HUD then draws onto
const ShaderHashesList shader_hashes_TdMotionBlur = {.pixel_shaders = {0x4177BC6E, 0xA99AD9DD}};
// DLAA / FSR / Luma TAA: the scene's first post passes (FX_PostProcess chain order, NOTES.md), which end a motion
// vector frame (the upscaler runs right before the first one). Pairs are 2.87.5, 2.81.3: DOF And Bloom Gather (and Max Samples), DOF
// And Bloom Blend, engine Motion Blur (and Dynamic Velocities Only; off in the shipped chain), Uber Post Process Blend (cooked, unused),
// then TdToneMapping and GammaCorrection, which end a chain without the others. TdDirectionalHaze (0x19BCE666, 0x9C77EAD7) stays in
// the scene: it adds its haze per pixel from the scene alpha's depth, which keeps the jitter, so after the upscaler silhouettes
// against the sky would alternate between two haze amounts every frame. It keeps the alpha, which the fill reads.
const ShaderHashesList shader_hashes_TdDirectionalHaze = {.pixel_shaders = {0x19BCE666, 0x9C77EAD7}};
const ShaderHashesList shader_hashes_ScenePost = {.pixel_shaders = {0x2BE8B514, 0x587465D1, 0xB977A1C4, 0x69A1F64A, 0x780BCE69, 0x8B012337, 0x8F95BB29, 0x37EF3D5C, 0xFF77A7B0, 0xE9224963, 0xFC10C0BD, 0x39B803FA, 0x1A760388, 0x602DA054, 0x0160196C, 0x87D136D3}};

// User settings, persisted in the [Luma] config section (LoadConfigs)
static bool g_smaa_enable = true;
static bool g_smaa_t2x = false; // SMAA T2x: a two phase jitter and the previous frame, through the upscalers' motion vectors
static float g_rcas_sharpness = 0.f;
// DEV tuning: predication from the scene alpha's linear depth, and its plane-deviation tolerance (Luma_ME_DepthExtract.hlsl)
static bool g_smaa_predication = true;
static float g_smaa_pred_tolerance = 0.02f;
// HDR-weighted MSAA resolve (Core's "DrawMSAAResolve()") in place of the game's, with the game's Anti-Aliasing on
static bool g_msaa_resolve_enable = true;
#if DEVELOPMENT
static bool g_msaa_resolve_average_depth = false; // The game's averaged depth in the resolved alpha, for halo A/B
static bool g_msaa_coverage_enable = true;        // The masked materials' alpha to coverage (with the resolve, "g_msaa_resolve_enable")
#else
constexpr bool g_msaa_resolve_average_depth = false;
constexpr bool g_msaa_coverage_enable = true;
#endif
// Session only, never persisted: a stuck "on" would look like a broken HUD (Mass Effect 2007)
static bool g_hide_ui = false;
// SMAA's, T2x's and the reactive masks' resources go after this many frames without their pass (MEM-11)
constexpr uint32_t IDLE_RELEASE_FRAMES = 600;

#if DEVELOPMENT
static bool g_mv_enable = false;       // Motion vectors without an upscaler
static bool g_mv_debug_view = false;   // Core's debug draw of the motion vector target
static bool g_mv_force_jitter = false; // The projection jitter without an upscaler
static bool g_sr_reactive_enable = true;
static float g_sr_reactive_scale = 1.f;      // The alpha blended draws' reactivity, scaled (AMD's default 1)
static float g_sr_reactive_threshold = 0.5f; // Under it 0, over it 0.9 (AMD's 0.2; Mass Effect 2007's and Borderlands 2's 0.5); 0: the scaled reactivity, capped at 0.9
static bool g_sr_reactive_debug_view = false;
#else
constexpr bool g_mv_enable = false;
constexpr bool g_mv_force_jitter = false;
constexpr bool g_sr_reactive_enable = true;
constexpr float g_sr_reactive_scale = 1.f;
constexpr float g_sr_reactive_threshold = 0.5f;
#endif

using SceneMotionVectors::PixelShaderPatch;
// The MSAA alpha to coverage is the game's first pixel shader patch after Core's
constexpr PixelShaderPatch COVERAGE_PATCH = PixelShaderPatch::GAME_0;

struct MirrorsEdgeGameDeviceData final : public GameDeviceData
{
   // SMAA metrics CB (b1) = (1/w, 1/h, w, h) + (predication threshold scale, 0, 0, 0) + T2x's subsample indices: scale 2.0 with
   // predication, else 1.0. Written every frame.
   com_ptr<ID3D11Buffer> cb_smaa_metrics;

   // SMAA and RCAS input: a snapshot of the tonemap's output (fp16, gamma encoded). Edge detection reads it as stored, the
   // neighborhood blend filters it in linear light (Luma_SMAA_impl.hlsl)
   ComPtr<ID3D11Texture2D> tex_input_encoded;
   ComPtr<ID3D11ShaderResourceView> srv_input_encoded;
   // The frames SMAA, the snapshot's users (SMAA or RCAS) and SMAA's output temp last ran, for "IDLE_RELEASE_FRAMES"
   uint32_t smaa_frame = 0;
   uint32_t snapshot_frame = 0;
   uint32_t smaa_out_frame = 0;

   // SMAA output temp, only with RCAS (its input). Without RCAS SMAA renders into the tonemap's target directly
   ComPtr<ID3D11Texture2D> tex_smaa_out;
   ComPtr<ID3D11RenderTargetView> tex_smaa_out_rtv;
   ComPtr<ID3D11ShaderResourceView> tex_smaa_out_srv;

   // Predication: the tonemap's scene SRV (linear depth in .a), captured at its draw, and the R16F edge-ness built from it
   ComPtr<ID3D11ShaderResourceView> srv_scene_depth;
   // TdToneMapping's exposure (t3, 1x1, exposure / 64), kept across frames: the MSAA resolves run before this frame's is built
   ComPtr<ID3D11ShaderResourceView> srv_exposure;
   com_ptr<ID3D11Buffer> cb_pred;
   ComPtr<ID3D11Texture2D> tex_pred;
   ComPtr<ID3D11UnorderedAccessView> uav_pred;
   ComPtr<ID3D11ShaderResourceView> srv_pred;

   // The canvas TdMotionBlur wrote this frame (the upgrade's mirror when upgraded: the bound resource either way), for Hide UI
   com_ptr<ID3D11Resource> canvas_res;

   // SMAA T2x (see "RunPostTonemapSMAA"), set at present for the whole frame like "sr_active". The scene jitters only after a frame
   // the resolve ran. The phase is chosen when the scene opens, -1 if it didn't jitter.
   bool t2x_active = false;
   int t2x_phase = -1;
   // SMAA's output of this frame and of the previous one, alternating: linear RGB with the velocity length in alpha. The previous one
   // is the history only if "t2x_frame", the last frame the resolve ran, was the frame before.
   ComPtr<ID3D11Texture2D> t2x_frames[2];
   ComPtr<ID3D11RenderTargetView> t2x_frame_rtvs[2];
   ComPtr<ID3D11ShaderResourceView> t2x_frame_srvs[2];
   uint32_t t2x_frame = 0;

   // ---- DLAA / FSR 3 Native AA / Luma TAA with motion vectors and jitter from patched shaders (Mass Effect 2007's path, one per-draw
   // buffer: dgVoodoo's vc4) ----
   // DLSS and FSR run in the x64 helper of Core's SR bridge (the game is 32-bit, see "SRBridge.h"), Luma TAA in process.
   // An upscaler is picked, hasn't failed (it then gives way to SMAA until picked again) and the game's MSAA is off, latched at present
   // for the whole frame: a selection made after the motion vector state was set would otherwise run the upscaler on mixed state
   bool sr_active = false;
   // The upscaler's output goes back into the scene (and its copy, see "mv_scene_copy") without its alpha (the linear depth the post
   // passes read), drawn from this view of it
   com_ptr<ID3D11BlendState> sr_rgb_blend_state;
   com_ptr<ID3D11ShaderResourceView> sr_output_srv;

   // Motion vectors and jitter this frame (an upscaler, SMAA T2x or the DEV toggle, set at present) and their state machine (Core's)
   SceneMotionVectors::State mv;
   bool mv_filled = false; // The fill wrote this frame's motion vectors (reset at present)
   // The upscaler drew the last frame: only then the scene jitters (frames it skips would reach the screen jittered)
   bool mv_jitter_allowed = false;
   float sr_vert_fov = 1.0471976f; // FSR's vertical FOV (radians): the last camera's, 60 degrees until one is seen
   // The upscalers' near and far (game units): the last camera's with a usable projection, Core's defaults until one is seen
   float sr_near_plane = SR::SuperResolutionImpl::DrawData{}.near_plane;
   float sr_far_plane = SR::SuperResolutionImpl::DrawData{}.far_plane;
   com_ptr<ID3D11ShaderResourceView> mv_scene_srv; // The fill's view of the scene, kept while the scene is the same resource
   // The copy of the scene the first post pass reads (UE3 resolves the scene surface into a texture), null if it reads none. The
   // upscaler's output goes into both ("ME Copy Back PS"): a post pass that draws onto the scene and resolves it again would
   // otherwise resolve the jittered scene (Borderlands 2's light shafts). Found from the game's last CopyResource of the scene (dgVoodoo
   // copies a whole surface with it) or TdDirectionalHaze's target: a texture matched by description can be another target's copy
   // (Bayonetta's camera blur velocity). It's also the upscaler's input.
   com_ptr<ID3D11Resource> mv_scene_copy;
   com_ptr<ID3D11Resource> mv_scene_last_copy;
   com_ptr<ID3D11RenderTargetView> mv_scene_copy_rtv;
   // The depth view "OpenScene" refused (not output sized), reset at present
   ID3D11DepthStencilView* open_refused_dsv = nullptr;
   // MSAA is on in the game (a multisampled scene resolved): the upscalers and SMAA T2x can't run (shown in the settings).
   // "msaa_resolved" this frame, "msaa_active" the last one (the coverage draws come before this frame's resolves).
   bool msaa_resolved = false;
   bool msaa_active = false;
#if DEVELOPMENT
   uint32_t msaa_resolves = 0, last_msaa_resolves = 0; // "OnResolveTextureRegion"'s replaced resolves, for the MCP
#endif

   void ReleaseT2xFrames()
   {
      for (int i = 0; i < 2; i++)
      {
         t2x_frames[i].reset();
         t2x_frame_rtvs[i].reset();
         t2x_frame_srvs[i].reset();
      }
   }

   // SMAA's own resources (predication, output temp), apart from Core's ("ReleaseSMAAIntermediates"); recreated at their next use
   void ReleaseSMAAScratch()
   {
      tex_pred.reset();
      uav_pred.reset();
      srv_pred.reset();
      ReleaseSMAAOutput();
   }

   void ReleaseSMAAOutput()
   {
      tex_smaa_out.reset();
      tex_smaa_out_rtv.reset();
      tex_smaa_out_srv.reset();
   }

   void ReleaseSnapshotScratch()
   {
      tex_input_encoded.reset();
      srv_input_encoded.reset();
   }
};

static MirrorsEdgeGameDeviceData& GetGameDeviceData(DeviceData& device_data)
{
   return *static_cast<MirrorsEdgeGameDeviceData*>(device_data.game);
}

static const MirrorsEdgeGameDeviceData& GetGameDeviceData(const DeviceData& device_data)
{
   return *static_cast<const MirrorsEdgeGameDeviceData*>(device_data.game);
}

class MirrorsEdge final : public Game
{
   static bool CreateDefaultTex(ID3D11Device* device, uint32_t w, uint32_t h, UINT bind_flags, ComPtr<ID3D11Texture2D>* out, DXGI_FORMAT format)
   {
      out->reset();
      const CD3D11_TEXTURE2D_DESC td(format, w, h, 1, 1, bind_flags);
      return SUCCEEDED(device->CreateTexture2D(&td, nullptr, out->put()));
   }

   // SMAA on the tonemap's output (gamma encoded, before TdMotionBlur and the HUD, so the UI is never antialiased), Borderlands 2's
   // chain: snapshot -> predication extract CS -> DrawSMAA -> optional RCAS, the last pass writing the tonemap's target. Without
   // "smaa" (an upscaler antialiased the frame) only RCAS runs.
   void RunPostTonemapSMAA(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, DeviceData& device_data, MirrorsEdgeGameDeviceData* gd, ID3D11RenderTargetView* ldr_rtv, bool smaa)
   {
      ComPtr<ID3D11Resource> ldr_res;
      ldr_rtv->GetResource(ldr_res.put());
      uint4 ldr_size{};
      DXGI_FORMAT ldr_format = DXGI_FORMAT_UNKNOWN;
      GetResourceInfo(ldr_res.get(), ldr_size, ldr_format);
      const uint32_t w = ldr_size.x, h = ldr_size.y;
      if (w == 0 || h == 0 || ldr_format == DXGI_FORMAT_UNKNOWN)
         return;

      // DrawSMAA looks its passes up with ".at()": a shader reload must not release them mid-chain (Sunset Overdrive)
      const std::shared_lock lock_shader_objects(s_mutex_shader_objects);

      auto* copy_vs = FindShader(device_data.native_vertex_shaders, CompileTimeStringHash("Copy VS"));
      bool do_sharpen = g_rcas_sharpness > 0.f && PrepareRCAS(native_device, device_data);

      const uint32_t frame_index = cb_luma_global_settings.FrameIndex;
      const auto snapshot = [&]
      {
         gd->snapshot_frame = frame_index;
         if (!gd->srv_input_encoded || GetViewTextureSize(gd->srv_input_encoded.get()) != uint2{w, h})
         {
            gd->ReleaseSnapshotScratch();
            // The tonemap target's format: CopyResource requires identical formats
            if (CreateDefaultTex(native_device, w, h, D3D11_BIND_SHADER_RESOURCE, std::addressof(gd->tex_input_encoded), ldr_format))
            {
               native_device->CreateShaderResourceView(gd->tex_input_encoded.get(), nullptr, gd->srv_input_encoded.put());
            }
         }
         if (!gd->srv_input_encoded)
            return false;
         native_device_context->CopyResource(gd->tex_input_encoded.get(), ldr_res.get());
         return true;
      };

      if (!smaa)
      {
         if (!do_sharpen)
            return;
         if (snapshot())
         {
            DrawRCAS(native_device_context, device_data, gd->srv_input_encoded.get(), ldr_rtv, g_rcas_sharpness);
         }
         return;
      }

      // All or nothing: skip SMAA this frame while any pass is missing (async load, live reload)
      if (!HasSMAAShaders(device_data))
         return;
      gd->smaa_frame = frame_index;

      // Predication from the scene the tonemap read, which maps 1:1 only at the same size. Plain ULTRA when any input is missing
      // (never scale 2.0 with a null texture)
      auto* pred_cs = FindShader(device_data.native_compute_shaders, CompileTimeStringHash("ME Depth Extract CS"));
      bool pred_ok = g_smaa_predication && gd->srv_scene_depth && pred_cs != nullptr && GetViewTextureSize(gd->srv_scene_depth.get()) == uint2{w, h};
      if (pred_ok)
      {
         const CB::DepthExtractConstants extract_constants = {.tolerance = g_smaa_pred_tolerance};
         pred_ok = PatchedDraws::WriteDynamicConstants(native_device, native_device_context, std::addressof(gd->cb_pred), &extract_constants, sizeof(extract_constants));
         if (!gd->srv_pred || GetViewTextureSize(gd->srv_pred.get()) != uint2{w, h})
         {
            gd->uav_pred.reset();
            gd->srv_pred.reset();
            if (CreateDefaultTex(native_device, w, h, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, std::addressof(gd->tex_pred), DXGI_FORMAT_R16_FLOAT))
            {
               native_device->CreateUnorderedAccessView(gd->tex_pred.get(), nullptr, gd->uav_pred.put());
               native_device->CreateShaderResourceView(gd->tex_pred.get(), nullptr, gd->srv_pred.put());
            }
         }
         pred_ok = pred_ok && gd->uav_pred && gd->srv_pred;
      }

      if (do_sharpen)
      {
         gd->smaa_out_frame = frame_index;
         if (!gd->tex_smaa_out_srv || GetViewTextureSize(gd->tex_smaa_out_srv.get()) != uint2{w, h})
         {
            gd->ReleaseSMAAOutput();
            if (CreateDefaultTex(native_device, w, h, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET, std::addressof(gd->tex_smaa_out), ldr_format))
            {
               native_device->CreateRenderTargetView(gd->tex_smaa_out.get(), nullptr, gd->tex_smaa_out_rtv.put());
               native_device->CreateShaderResourceView(gd->tex_smaa_out.get(), nullptr, gd->tex_smaa_out_srv.put());
            }
         }
         do_sharpen = gd->tex_smaa_out_rtv && gd->tex_smaa_out_srv;
      }

      if (!snapshot())
         return;

      // Unbinding the mask's UAV before DrawSMAA is what makes it readable: an SRV of a resource still bound as a UAV reads as null
      if (pred_ok)
      {
         ID3D11ShaderResourceView* cs_srv = gd->srv_scene_depth.get();
         ID3D11UnorderedAccessView* cs_uav = gd->uav_pred.get();
         ID3D11Buffer* cs_cb = gd->cb_pred.get();
         native_device_context->CSSetUnorderedAccessViews(0, 1, &cs_uav, nullptr);
         native_device_context->CSSetShaderResources(0, 1, &cs_srv);
         native_device_context->CSSetConstantBuffers(0, 1, &cs_cb);
         native_device_context->CSSetShader(pred_cs, nullptr, 0);
         native_device_context->Dispatch((w + 7) / 8, (h + 7) / 8, 1);
         ID3D11UnorderedAccessView* const null_uav = nullptr;
         native_device_context->CSSetUnorderedAccessViews(0, 1, &null_uav, nullptr);
      }

      // SMAA T2x: SMAA into this frame's linear target with the velocity, then the resolve with the previous frame into the chain's
      // output (Borderlands 2's). It needs this frame's motion vectors from the fill, at the target's size. Without them this frame is
      // 1x and the next one doesn't jitter.
      auto* const t2x_weight_ps = FindShader(device_data.native_pixel_shaders, CompileTimeStringHash("ME SMAA T2x Blending Weight Calculation PS"));
      auto* const t2x_blend_ps = FindShader(device_data.native_pixel_shaders, CompileTimeStringHash("ME SMAA T2x Neighborhood Blending PS"));
      auto* const t2x_resolve_ps = FindShader(device_data.native_pixel_shaders, CompileTimeStringHash("ME SMAA T2x Resolve PS"));
      bool t2x = gd->t2x_active && gd->mv_filled && gd->mv.srv && t2x_weight_ps && t2x_blend_ps && t2x_resolve_ps && copy_vs && GetViewTextureSize(gd->mv.srv.get()) == uint2{w, h};
      const int t2x_current = int(frame_index & 1);
      bool t2x_history = gd->t2x_frames[0] && gd->t2x_frame + 1 == frame_index;
      if (t2x && GetViewTextureSize(gd->t2x_frame_srvs[0].get()) != uint2{w, h})
      {
         t2x_history = false;
         gd->ReleaseT2xFrames();
         for (int i = 0; i < 2; i++)
         {
            if (!CreateDefaultTex(native_device, w, h, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET, std::addressof(gd->t2x_frames[i]), DXGI_FORMAT_R16G16B16A16_FLOAT) ||
                FAILED(native_device->CreateRenderTargetView(gd->t2x_frames[i].get(), nullptr, gd->t2x_frame_rtvs[i].put())) ||
                FAILED(native_device->CreateShaderResourceView(gd->t2x_frames[i].get(), nullptr, gd->t2x_frame_srvs[i].put())))
            {
               gd->ReleaseT2xFrames();
               break;
            }
         }
         t2x = gd->t2x_frames[0].get() != nullptr;
      }

      // b1: the metrics, the predication threshold scale and T2x's subsample indices for the jitter phase (see "OpenScene"), 0 for 1x
      const float subsample_indices = ((t2x && gd->t2x_phase >= 0) ? float(gd->t2x_phase + 1) : 0.f);
      const CB::SmaaMetrics metrics = {
         .rt_metrics = {1.f / (float)w, 1.f / (float)h, (float)w, (float)h},
         .predication = {(pred_ok ? 2.0f : 1.0f), 0.f, 0.f, 0.f},
         .subsample_indices = {subsample_indices, subsample_indices, subsample_indices, 0.f},
      };
      if (!PatchedDraws::WriteDynamicConstants(native_device, native_device_context, std::addressof(gd->cb_smaa_metrics), &metrics, sizeof(metrics)))
         return;

      // Metrics at VS+PS b1, bound here rather than through "DrawSMAA()" (which restores the slot): the T2x resolve reads them too
      ID3D11Buffer* metrics_cb = gd->cb_smaa_metrics.get();
      native_device_context->VSSetConstantBuffers(1, 1, &metrics_cb);
      native_device_context->PSSetConstantBuffers(1, 1, &metrics_cb);

      // Rendering into the tonemap's target is safe: SMAA and RCAS sample the snapshot, never the target itself
      ID3D11RenderTargetView* const output_rtv = (do_sharpen ? gd->tex_smaa_out_rtv.get() : ldr_rtv);
      const SMAAT2xPasses t2x_passes = {.blending_weight_calculation_ps = t2x_weight_ps, .neighborhood_blending_ps = t2x_blend_ps, .velocity = gd->mv.srv.get()};
      DrawSMAA(native_device, native_device_context, device_data,
         t2x ? gd->t2x_frame_rtvs[t2x_current].get() : output_rtv,
         gd->srv_input_encoded.get() /*neighborhood blend (filtered in linear light)*/,
         gd->srv_input_encoded.get() /*edge detection (gamma)*/,
         pred_ok ? gd->srv_pred.get() : nullptr,
         nullptr /*metrics, bound above*/,
         t2x ? &t2x_passes : nullptr);

      if (t2x)
      {
         // The resolve: t0 this frame, t1 the previous one or this one again without a history, t2 the motion vectors, s0 linear and
         // s1 point
         // The patched draws may leave the motion vector target bound, and its view would then read as null
         native_device_context->OMSetRenderTargets(0, nullptr, nullptr);
         ID3D11ShaderResourceView* const resolve_srvs[2] = {gd->t2x_frame_srvs[t2x_history ? (t2x_current ^ 1) : t2x_current].get(), gd->mv.srv.get()};
         native_device_context->PSSetShaderResources(1, 2, resolve_srvs);
         ID3D11SamplerState* const resolve_samplers[2] = {device_data.sampler_state_linear.get(), device_data.sampler_state_point.get()};
         native_device_context->PSSetSamplers(0, 2, resolve_samplers);
         DrawCustomPixelShader(native_device_context, device_data.default_depth_stencil_state.get(), device_data.default_blend_state.get(), nullptr,
            copy_vs, t2x_resolve_ps, gd->t2x_frame_srvs[t2x_current].get(), output_rtv, w, h, false);
         gd->t2x_frame = frame_index;
      }

      if (do_sharpen)
      {
         DrawRCAS(native_device_context, device_data, gd->tex_smaa_out_srv.get(), ldr_rtv, g_rcas_sharpness);
      }
   }

   // ---- DLAA / FSR 3 Native AA / Luma TAA (vc4 layout in "MotionVectorPatches", the state machine in Core's "SceneMotionVectors") ----
   // c0-c3, row vectors
   static constexpr size_t VIEW_PROJECTION_OFFSET = MotionVectorPatches::view_projection_row * 16;

   // The main view's camera in vc4 (Mass Effect Legendary Edition's checks): not mirrored (a planar reflection's view projection has a
   // negative 3x3 determinant) and projecting (clip w depends on the position: Mass Effect Legendary Edition's screen space draw after
   // shooting an enemy has an identity one). Row vectors: w is each row's 4th value.
   static bool IsMainViewCamera(const std::vector<uint8_t>& constants)
   {
      const Math::Matrix44D view_projection = SceneMotionVectors::ReadMatrix(constants, VIEW_PROJECTION_OFFSET);
      const double* const m = view_projection.GetData();
      const bool perspective = std::abs(m[3]) + std::abs(m[7]) + std::abs(m[11]) >= 0.5;
      return view_projection.Determinant() >= 0.f && perspective;
   }

   // The game's motion vector state for Core's buffer and bind events, null before the device data exists
   static SceneMotionVectors::State* GetSceneMotionVectors(reshade::api::device* device)
   {
      DeviceData* const device_data = device->get_private_data<DeviceData>();
      return ((device_data && device_data->game) ? &GetGameDeviceData(*device_data).mv : nullptr);
   }

   // What "SceneMotionVectors" asks of the game
   struct MotionVectorGame
   {
      static constexpr const char* log_tag = "Mirror's Edge";
      static constexpr MotionVectorPatch::Layout layout = MotionVectorPatches::layout;
      static constexpr auto cameras = MotionVectorPatches::cameras;
      static constexpr auto tie_break_rows = MotionVectorPatches::tie_break_rows;
      static constexpr auto frame_camera = MotionVectorPatches::frame_camera;
      static constexpr const char* patch_names[size_t(PixelShaderPatch::COUNT)] = {"MV PS", "MV PS (mask)", "MV PS (mask)", "MV PS (blended)", "Coverage PS", "", "", ""};
      // The paper scraps blown around (VS 0xDA140247) are vertices the CPU rewrites each frame under a fixed LocalToWorld: their motion
      // comes from their previous vertices, read past the second run's resources (t64-t79)
      static constexpr uint32_t previous_vertices_slot = MotionVectorPatches::layout.previous_resources_slot + MotionVectorPatches::layout.resource_slots;

      // The fp16 scene, single sample, with a shader resource view (the fill and the upscaler read it). A multisampled scene is refused
      // here only on MSAA's first frame ("msaa_active" then stops this path).
      static bool AcceptSceneColor(const D3D11_RENDER_TARGET_VIEW_DESC& rtv_desc, const D3D11_TEXTURE2D_DESC& color_desc)
      {
         return rtv_desc.Format == DXGI_FORMAT_R16G16B16A16_FLOAT && color_desc.ArraySize == 1 && color_desc.SampleDesc.Count == 1 && (color_desc.BindFlags & D3D11_BIND_SHADER_RESOURCE) != 0;
      }

      // The chain link fences are alpha blended draws with their own motion vectors, their wires opaque (see
      // "PatchPixelShaderReactive"'s "motion_vectors"): none excluded
      static bool IsExcludedPixelShader(uint32_t pixel_shader_hash)
      {
         return false;
      }

      static bool AcceptFrameCamera(const std::vector<uint8_t>& constants)
      {
         return IsMainViewCamera(constants);
      }

      // The upscalers that read the reactive mask the alpha blended draws write: FSR and Luma TAA (DLSS's current presets ignore it,
      // DLSS-Best-Practices TRN-2)
      static bool ReactiveMaskUsed(const DeviceData& device_data)
      {
         return g_sr_reactive_enable && GetGameDeviceData(device_data).sr_active && (device_data.sr_type == SR::Type::FSR || device_data.sr_type == SR::Type::LumaTAA);
      }

      static bool CanFill(DeviceData& device_data)
      {
         return HasShaders(device_data.native_compute_shaders, CompileTimeStringHash("ME Motion Vector Fill CS"));
      }

      // The marker texels' camera motion from the opaque scene's depth (the end of the scene fills the rest and the masks)
      static void FillMarkersBeforeBlend(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, DeviceData& device_data)
      {
         FillMotionVectors(native_device, native_device_context, device_data, /* write_reactive */ false);
      }

      // A game pixel shader's "patch", from the bytecode its shader object was created with; empty with "error" if it can't be patched
      static std::vector<uint8_t> PatchPixelShader(PixelShaderPatch patch, const uint8_t* code, size_t code_size, std::string* error)
      {
         switch (patch)
         {
         case PixelShaderPatch::MOTION_VECTORS:
            return MotionVectorPatch::PatchPixelShader(code, code_size, MotionVectorPatches::layout, error, /* targets_only */ true);
         case PixelShaderPatch::MASK_ALPHA:
         case PixelShaderPatch::MASK_ADDITIVE:
            return MotionVectorPatch::PatchPixelShaderReactive(code, code_size, MotionVectorPatches::layout, MotionVectorPatches::reactive_slot,
               (patch == PixelShaderPatch::MASK_ADDITIVE ? MotionVectorPatch::ReactiveMode::ADDITIVE : MotionVectorPatch::ReactiveMode::ALPHA), error);
         case PixelShaderPatch::BLENDED:
            return MotionVectorPatch::PatchPixelShaderReactive(code, code_size, MotionVectorPatches::layout, UINT32_MAX, MotionVectorPatch::ReactiveMode::ALPHA_MOTION_VECTORS, error);
         case COVERAGE_PATCH:
            return CoveragePatch::PatchPixelShader(code, code_size, error);
         }
         return {};
      }
   };

   // Opens the scene at the frame's first mesh draw into output sized depth (the menu's 1280x720 scene never opens it): takes the scene
   // depth, rolls the last frame's camera and objects over and picks the jitter the whole scene draws with. Once per present (the HUD
   // and later passes never reopen it).
   static void OpenScene(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, const CommandListData& cmd_list_data, DeviceData& device_data, uint32_t vertex_shader_hash, ID3D11DepthStencilView* dsv)
   {
      auto& gd = GetGameDeviceData(device_data);
      const uint2 depth_size = GetViewTextureSize(dsv);
      if (depth_size.x != uint32_t(device_data.output_resolution.x) || depth_size.y != uint32_t(device_data.output_resolution.y))
      {
         gd.open_refused_dsv = dsv;
         return;
      }
      // The menu draws its scene twice: first into a 1280x720 viewport of the same output sized targets (then GammaCorrection), then
      // at full size. Only the full size one is the scene (NOTES.md).
      UINT viewport_count = 1;
      D3D11_VIEWPORT viewport = {};
      native_device_context->RSGetViewports(&viewport_count, &viewport);
      if (viewport_count == 0 || viewport.Width != float(depth_size.x) || viewport.Height != float(depth_size.y) || !SceneMotionVectors::GetPatchedVertexShader<MotionVectorGame>(&gd.mv, native_device, cmd_list_data, device_data, vertex_shader_hash).shader)
         return;
      // Planar reflections (SceneCaptureReflectActor: the main menu, Edge_Pt2, Factory_Arena; cooked map scan, NOTES.md) draw a mirrored
      // view into the scene's own targets before the main view, at their texture's size: the viewport check misses them only when the
      // output has that size, and "IsMainViewCamera" refuses them. Before the vc4 buffer's first copied upload the view is unknown and
      // opens it (the frame's camera is still checked, "AcceptFrameCamera").
      if (ID3D11Buffer* const object_buffer = gd.mv.bound_states.ObjectBuffer(native_device_context))
      {
         constexpr size_t view_projection_end = VIEW_PROJECTION_OFFSET + SceneMotionVectors::CAMERA_SIZE;
         if (const auto constants = gd.mv.constants.Snapshot(gd.mv.constants.Register(object_buffer), view_projection_end, view_projection_end); constants && constants->size() >= view_projection_end && !IsMainViewCamera(*constants))
            return;
      }
      gd.mv_scene_last_copy.reset();
      // Halton (2, 3) over the upscaler's phase count; pixels to NDC (y up). None until the upscaler is ready (the bridge's helper
      // starting shows the scene as it is, antialiased with SMAA).
      const SR::InstanceData* sr_instance_data = (gd.sr_active ? device_data.GetSRInstanceData() : nullptr);
      if (sr_instance_data && !sr_implementations[device_data.sr_type]->IsReady(sr_instance_data))
      {
         sr_instance_data = nullptr;
      }
      std::array<float, 2> jitter = (((sr_instance_data && gd.mv_jitter_allowed) || g_mv_force_jitter) ? GetSRJitter(device_data, sr_instance_data) : std::array<float, 2>{});
      // SMAA T2x: the sample offsets (0.25, 0.25) then (-0.25, -0.25) in y down pixels, for "SMAA.hlsl"'s T2x subsample indices 1 and
      // 2. That is its table read with y up, which measured better (docs/SMAA-Lab.md). The projection moves the other way.
      gd.t2x_phase = ((gd.t2x_active && gd.t2x_frames[0] && gd.t2x_frame + 1 == cb_luma_global_settings.FrameIndex) ? int(cb_luma_global_settings.FrameIndex & 1) : -1);
      if (gd.t2x_phase >= 0)
      {
         const float shift = (gd.t2x_phase == 0 ? -0.25f : 0.25f);
         jitter = {shift, shift};
      }
      if (!SceneMotionVectors::BeginFrame(&gd.mv, native_device, native_device_context, dsv, depth_size, jitter))
      {
         gd.t2x_phase = -1;
      }
   }

   // DLAA, FSR 3 Native AA or Luma TAA on the jittered scene, its depth (from its alpha, see the fill) and the motion vectors; the result
   // goes back into the scene's color (and its copy, see "mv_scene_copy"), its alpha (the post passes' linear depth) kept. False if it
   // didn't draw (missing input, or the upscaler failed). "reactive_mask": the fill wrote this scene's masks.
   static bool DrawUpscaler(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, DeviceData& device_data, bool reactive_mask)
   {
      auto& gd = GetGameDeviceData(device_data);
      auto* const copy_vs = FindShader(device_data.native_vertex_shaders, CompileTimeStringHash("Copy VS"));
      auto* const copy_ps = FindShader(device_data.native_pixel_shaders, CompileTimeStringHash("Copy PS"));
      if (!gd.mv.texture || !gd.mv.camera || !gd.mv.device_depth || !gd.mv.scene_color || !gd.mv.scene_rtv || !copy_vs || !copy_ps)
         return false;
      com_ptr<ID3D11Texture2D> scene;
      if (FAILED(gd.mv.scene_color->QueryInterface(&scene)))
         return false;
      D3D11_TEXTURE2D_DESC scene_desc = {};
      scene->GetDesc(&scene_desc);
      // The upscaler's input is what the post passes read: the scene's copy (TdDirectionalHaze's hazed one), else the scene
      com_ptr<ID3D11Texture2D> source = scene;
      if (com_ptr<ID3D11Texture2D> copy; gd.mv_scene_copy && SUCCEEDED(gd.mv_scene_copy->QueryInterface(&copy)))
      {
         D3D11_TEXTURE2D_DESC copy_desc = {};
         copy->GetDesc(&copy_desc);
         if (copy_desc.Width == scene_desc.Width && copy_desc.Height == scene_desc.Height && copy_desc.Format == scene_desc.Format)
         {
            source = copy;
         }
      }
      SR::InstanceData* const sr_instance_data = device_data.GetSRInstanceData();
      if (!sr_instance_data)
         return false;

      D3D11_TEXTURE2D_DESC output_desc = {};
      if (device_data.sr_output_color)
      {
         device_data.sr_output_color->GetDesc(&output_desc);
      }
      if (output_desc.Width != scene_desc.Width || output_desc.Height != scene_desc.Height)
      {
         device_data.sr_output_color.reset();
         gd.sr_output_srv.reset();
         output_desc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R16G16B16A16_FLOAT, scene_desc.Width, scene_desc.Height, 1, 1, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS);
         if (SUCCEEDED(SRBridge::CreateSharableTexture(native_device, output_desc, &device_data.sr_output_color)))
         {
            native_device->CreateShaderResourceView(device_data.sr_output_color.get(), nullptr, &gd.sr_output_srv);
         }
      }
      if (!device_data.sr_output_color || !gd.sr_output_srv)
      {
         // Back to SMAA until the upscaler is picked again
         device_data.sr_suppressed = true;
         return false;
      }
      if (!gd.sr_rgb_blend_state)
      {
         D3D11_BLEND_DESC blend_desc = CD3D11_BLEND_DESC(D3D11_DEFAULT);
         blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_RED | D3D11_COLOR_WRITE_ENABLE_GREEN | D3D11_COLOR_WRITE_ENABLE_BLUE;
         native_device->CreateBlendState(&blend_desc, &gd.sr_rgb_blend_state);
         if (!gd.sr_rgb_blend_state)
            return false;
      }

      // FSR needs the camera (vc4's view projection multiplies row vectors)
      const SR::ViewProjectionCamera camera = SR::GetViewProjectionCamera(reinterpret_cast<const float*>(gd.mv.camera->data() + VIEW_PROJECTION_OFFSET), /* row_vectors */ true);

      const SR::SettingsData settings_data = {
         .output_width = scene_desc.Width,
         .output_height = scene_desc.Height,
         .render_width = scene_desc.Width,
         .render_height = scene_desc.Height,
         .hdr = true,
         // The motion vectors are UV deltas, previous minus current
         .mvs_x_scale = float(scene_desc.Width),
         .mvs_y_scale = float(scene_desc.Height),
         // The scene is not exposed yet: TdToneMapping multiplies it by its eye adaptation after the upscaler. DLSS's auto exposure
         // covers that; FSR's clips highlights (FSR-Best-Practices FIN-3), so it runs at exposure 1 (Borderlands 2's).
         .auto_exposure = device_data.sr_type != SR::Type::FSR,
         // DLAA on scene-referred HDR input takes preset J or K, never L or M (DLSS-Best-Practices PRE-4): "Default" picks K (11), so
         // NVIDIA can't change it over the air; an explicit choice is kept
         .render_preset = (dlss_render_preset != 0 ? dlss_render_preset : 11u),
      };
      sr_implementations[device_data.sr_type]->UpdateSettings(sr_instance_data, native_device_context, settings_data);

      // FSR requires a FOV (it errors on 0), and both take near and far: a camera without an up axis or a usable projection keeps the
      // last ones. A finite far (depth 1) even when the projection has none (FSR's context is FFX_FSR3_ENABLE_DEPTH_INFINITE only with
      // inverted depth).
      if (camera.vert_fov > 0.0)
      {
         gd.sr_vert_fov = float(camera.vert_fov);
      }
      if (camera.near_plane > 0.0)
      {
         gd.sr_near_plane = float(camera.near_plane);
         gd.sr_far_plane = float(camera.far_plane);
      }
      // The reactive masks (FSR and Luma TAA; DLSS ignores them)
      const SR::SuperResolutionImpl::DrawData draw_data = {
         .reset = device_data.force_reset_sr,
         .output_color = device_data.sr_output_color.get(),
         .source_color = source.get(),
         .motion_vectors = gd.mv.texture.get(),
         .depth_buffer = gd.mv.device_depth.get(),
         .bias_mask = (reactive_mask ? gd.mv.reactive.get() : nullptr),
         .transparency_alpha = (reactive_mask ? gd.mv.transparency.get() : nullptr),
         // As applied (pixels, +y down)
         .jitter_x = gd.mv.jitter[0],
         .jitter_y = gd.mv.jitter[1],
         .vert_fov = gd.sr_vert_fov,
         .near_plane = gd.sr_near_plane,
         .far_plane = gd.sr_far_plane,
      };
      if (!sr_implementations[device_data.sr_type]->Draw(sr_instance_data, native_device_context, draw_data))
      {
         // Back to SMAA until the upscaler is picked again
         device_data.sr_suppressed = true;
         return false;
      }
      // The RGB write mask keeps the scene's linear depth; the copy is the game's resolve of the same scene, written in the same pass
      // (with a view like the scene's: dgVoodoo's are single slice arrays), else copied whole after it
      ID3D11RenderTargetView* copy_rtv = nullptr;
      auto* const copy_back_ps = FindShader(device_data.native_pixel_shaders, CompileTimeStringHash("ME Copy Back PS"));
      if (gd.mv_scene_copy && copy_back_ps)
      {
         com_ptr<ID3D11Resource> copy_rtv_resource;
         if (gd.mv_scene_copy_rtv)
         {
            gd.mv_scene_copy_rtv->GetResource(&copy_rtv_resource);
         }
         if (copy_rtv_resource != gd.mv_scene_copy)
         {
            gd.mv_scene_copy_rtv.reset();
            com_ptr<ID3D11Texture2D> copy;
            if (SUCCEEDED(gd.mv_scene_copy->QueryInterface(&copy)))
            {
               D3D11_TEXTURE2D_DESC copy_desc = {};
               copy->GetDesc(&copy_desc);
               D3D11_RENDER_TARGET_VIEW_DESC rtv_desc = {};
               gd.mv.scene_rtv->GetDesc(&rtv_desc);
               if (copy_desc.BindFlags & D3D11_BIND_RENDER_TARGET)
               {
                  native_device->CreateRenderTargetView(copy.get(), &rtv_desc, &gd.mv_scene_copy_rtv);
               }
            }
         }
         copy_rtv = gd.mv_scene_copy_rtv.get();
      }
      DrawCustomPixelShader(native_device_context, device_data.default_depth_stencil_state.get(), gd.sr_rgb_blend_state.get(), nullptr, copy_vs, copy_rtv ? copy_back_ps : copy_ps,
         gd.sr_output_srv.get(), gd.mv.scene_rtv.get(), scene_desc.Width, scene_desc.Height, true, copy_rtv);
      if (gd.mv_scene_copy && !copy_rtv)
      {
         native_device_context->CopyResource(gd.mv_scene_copy.get(), scene.get());
      }
      // Not while the bridge's helper starts (the color copied as it is): SMAA stays on and the next frame resets
      device_data.has_drawn_sr = sr_implementations[device_data.sr_type]->IsReady(sr_instance_data);
      return true;
   }

   // The depth and camera motion fill (see "Luma_ME_MotionVectorFill.hlsl"), with the reactive masks if "write_reactive". The caller
   // caches and restores the state. False if it didn't run.
   static bool FillMotionVectors(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, DeviceData& device_data, bool write_reactive)
   {
      auto& gd = GetGameDeviceData(device_data);
      auto* const fill_shader = FindShader(device_data.native_compute_shaders, CompileTimeStringHash("ME Motion Vector Fill CS"));
      if (gd.mv_scene_srv)
      {
         com_ptr<ID3D11Resource> srv_resource;
         gd.mv_scene_srv->GetResource(&srv_resource);
         if (srv_resource != gd.mv.scene_color)
         {
            gd.mv_scene_srv.reset();
         }
      }
      if (!fill_shader || !gd.mv.camera || !gd.mv.scene_color ||
          (!gd.mv_scene_srv && FAILED(native_device->CreateShaderResourceView(gd.mv.scene_color.get(), nullptr, &gd.mv_scene_srv))))
         return false;
      const SR::ViewProjectionCamera camera = SR::GetViewProjectionCamera(reinterpret_cast<const float*>(gd.mv.camera->data() + VIEW_PROJECTION_OFFSET), /* row_vectors */ true);
      const CB::MotionVectorFillConstants constants = {
         .reprojection = gd.mv.reprojection,
         .jitter_ndc = {gd.mv.jitter_ndc[0], gd.mv.jitter_ndc[1]},
         .depth_from_view = {float(camera.depth_a), float(camera.depth_b)},
         .reactive_scale = g_sr_reactive_scale,
         .reactive_threshold = g_sr_reactive_threshold,
         .reactive_enabled = (write_reactive ? 1.f : 0.f),
      };
      if (!PatchedDraws::WriteDynamicConstants(native_device, native_device_context, std::addressof(gd.mv.fill_buffer), &constants, sizeof(constants)))
         return false;
      // The scene and the motion vectors may be bound as render targets
      native_device_context->OMSetRenderTargets(0, nullptr, nullptr);
      ID3D11Buffer* const buffer = gd.mv.fill_buffer.get();
      ID3D11ShaderResourceView* const srvs[2] = {gd.mv_scene_srv.get(), write_reactive ? gd.mv.reactive_target_srv.get() : nullptr};
      ID3D11UnorderedAccessView* const uavs[4] = {gd.mv.uav.get(), gd.mv.device_depth_uav.get(), write_reactive ? gd.mv.reactive_uav.get() : nullptr, write_reactive ? gd.mv.transparency_uav.get() : nullptr};
      native_device_context->CSSetConstantBuffers(0, 1, &buffer);
      native_device_context->CSSetShaderResources(0, UINT(std::size(srvs)), srvs);
      native_device_context->CSSetUnorderedAccessViews(0, UINT(std::size(uavs)), uavs, nullptr);
      native_device_context->CSSetShader(fill_shader, nullptr, 0);
      native_device_context->Dispatch((uint32_t(device_data.output_resolution.x) + 7) / 8, (uint32_t(device_data.output_resolution.y) + 7) / 8, 1);
      ID3D11UnorderedAccessView* const null_uavs[std::size(uavs)] = {};
      ID3D11ShaderResourceView* const null_srvs[std::size(srvs)] = {};
      native_device_context->CSSetUnorderedAccessViews(0, UINT(std::size(null_uavs)), null_uavs, nullptr);
      native_device_context->CSSetShaderResources(0, UINT(std::size(null_srvs)), null_srvs);
      gd.mv_filled = true;
      if (write_reactive)
      {
         gd.mv.sr_reactive_frame = cb_luma_global_settings.FrameIndex;
      }
      return true;
   }

   // Ends the scene at its first post pass: the fill (the last camera motion, the depth, the reactive masks), then the upscaler, both
   // before that pass reads the scene
   static void EndScene(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, DeviceData& device_data)
   {
      auto& gd = GetGameDeviceData(device_data);
      gd.mv.scene_open = false;
      gd.mv.scene_done = true;
      DrawStateStack<DrawStateStackType::FullGraphics> graphics_state;
      DrawStateStack<DrawStateStackType::Compute> compute_state;
      graphics_state.Cache(native_device_context, device_data.uav_max_count);
      compute_state.Cache(native_device_context, device_data.uav_max_count);
      // The reactive and transparency & composition masks, written by the fill from what the alpha blended draws wrote
      const bool reactive = MotionVectorGame::ReactiveMaskUsed(device_data) && gd.mv.reactive_target_srv;
      if (reactive)
      {
         D3D11_TEXTURE2D_DESC desc = {};
         if (gd.mv.reactive)
         {
            gd.mv.reactive->GetDesc(&desc);
         }
         if (desc.Width != uint32_t(device_data.output_resolution.x) || desc.Height != uint32_t(device_data.output_resolution.y))
         {
            const std::unique_lock lock(gd.mv.mutex);
            gd.mv.reactive.reset();
            gd.mv.reactive_uav.reset();
            gd.mv.transparency.reset();
            gd.mv.transparency_uav.reset();
            desc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R8_UNORM, uint32_t(device_data.output_resolution.x), uint32_t(device_data.output_resolution.y), 1, 1, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS);
            if (SUCCEEDED(SRBridge::CreateSharableTexture(native_device, desc, &gd.mv.reactive)))
            {
               native_device->CreateUnorderedAccessView(gd.mv.reactive.get(), nullptr, &gd.mv.reactive_uav);
            }
            if (SUCCEEDED(SRBridge::CreateSharableTexture(native_device, desc, &gd.mv.transparency)))
            {
               native_device->CreateUnorderedAccessView(gd.mv.transparency.get(), nullptr, &gd.mv.transparency_uav);
            }
         }
      }
      const bool write_reactive = reactive && gd.mv.reactive_uav && gd.mv.transparency_uav;
      const bool filled = std::exchange(gd.mv.fill_pending, false) && FillMotionVectors(native_device, native_device_context, device_data, write_reactive);
      // The upscaler's depth comes from the fill
      if (gd.sr_active && filled)
      {
         [[maybe_unused]] const bool drawn = DrawUpscaler(native_device, native_device_context, device_data, write_reactive);
#if DEVELOPMENT
         gd.mv.stats.sr_draws += drawn;
#endif
      }
      compute_state.Restore(native_device_context);
      graphics_state.Restore(native_device_context);
   }

   // The weighted resolve reads the samples of the fp16 MSAA scene, which the game (dgVoodoo) only ever resolves and creates without
   // a shader resource bind. The bind flag only: dgVoodoo's formats stay as created.
   static bool OnCreateResource(reshade::api::device* device, reshade::api::resource_desc& desc, reshade::api::subresource_data* initial_data, reshade::api::resource_usage initial_state)
   {
      if (desc.type != reshade::api::resource_type::texture_2d || desc.texture.samples <= 1 || (desc.usage & reshade::api::resource_usage::render_target) == 0)
         return false;
      if (desc.texture.format != reshade::api::format::r16g16b16a16_float && desc.texture.format != reshade::api::format::r16g16b16a16_typeless)
         return false;
      if ((desc.usage & reshade::api::resource_usage::shader_resource) != 0)
         return false;
      // The scene's are output sized; any size before the swapchain is known (an unbindable one keeps the hardware resolve)
      if (const DeviceData* device_data = device->get_private_data<DeviceData>();
         device_data && device_data->output_resolution.x > 0 && (desc.texture.width != uint32_t(device_data->output_resolution.x) || desc.texture.height != uint32_t(device_data->output_resolution.y)))
         return false;
      desc.usage |= reshade::api::resource_usage::shader_resource;
      return true;
   }

   // The game's ResolveSubresource of the fp16 MSAA scene (three a frame, all into the texture the refraction and post passes read):
   // Core's weighted one instead. Anything else, or a pair it can't bind, keeps the hardware resolve.
   static bool OnResolveTextureRegion(reshade::api::command_list* cmd_list, reshade::api::resource source, uint32_t source_subresource, const reshade::api::subresource_box* source_box, reshade::api::resource dest, uint32_t dest_subresource, uint32_t dest_x, uint32_t dest_y, uint32_t dest_z, reshade::api::format format)
   {
      if (format != reshade::api::format::r16g16b16a16_float || source_subresource != 0 || dest_subresource != 0 || source_box != nullptr || dest_x != 0 || dest_y != 0 || dest_z != 0)
         return false;
      DeviceData* const device_data = cmd_list->get_device()->get_private_data<DeviceData>();
      if (!device_data || !device_data->game)
         return false;
      auto& gd = GetGameDeviceData(*device_data);
      // MSAA is on in the game (a resolve source is always multisampled), whatever the AA picked or the toggle (the settings, and the
      // coverage draws' gate)
      gd.msaa_resolved = true;
      if (!g_msaa_resolve_enable)
         return false;

      // Held through the draw so a shader reload can't release them mid-use
      const std::shared_lock lock_shader_objects(s_mutex_shader_objects);
      const uint32_t resolve_ps = (g_msaa_resolve_average_depth ? CompileTimeStringHash("ME MSAA Resolve Average Depth PS") : CompileTimeStringHash("MSAA Resolve PS"));
      if (!DrawMSAAResolve((ID3D11Device*)(cmd_list->get_device()->get_native()), (ID3D11DeviceContext*)(cmd_list->get_native()), *device_data, reinterpret_cast<ID3D11Resource*>(source.handle), reinterpret_cast<ID3D11Resource*>(dest.handle), DXGI_FORMAT_R16G16B16A16_FLOAT, gd.srv_exposure.get(), nullptr, resolve_ps))
         return false;
#if DEVELOPMENT
      gd.msaa_resolves++;
#endif
      return true;
   }

public:
   // The motion vector events (Core's: the motion vector target in every blend state, the tracked bindings and the vc4 copies) and the
   // MSAA resolve
   static void SetEvents(bool enable)
   {
      const auto set = [enable]<reshade::addon_event event>(typename reshade::addon_event_traits<event>::decl callback)
      {
         if (enable)
         {
            reshade::register_event<event>(callback);
         }
         else
         {
            reshade::unregister_event<event>(callback);
         }
      };
      SceneMotionVectors::SetEvents<GetSceneMotionVectors>(enable);
      set.template operator()<reshade::addon_event::create_resource>(OnCreateResource);
      set.template operator()<reshade::addon_event::resolve_texture_region>(OnResolveTextureRegion);
   }

   std::unique_ptr<std::byte[]> PatchShaderBytecodeSync(const std::byte* code, size_t& size, reshade::api::pipeline_subobject_type type, uint64_t shader_hash, const std::byte* shader_object, size_t shader_object_size) override
   {
      if (type != reshade::api::pipeline_subobject_type::pixel_shader)
         return nullptr;
      if (ui_saturated_pixel_shaders.contains(uint32_t(shader_hash)))
         return DXBC::ClampOutput0(code, &size);
      if (scene_blend_input_pixel_shaders.contains(uint32_t(shader_hash)))
         return DXBC::ClampOutput0(code, &size, DXBC::Output0Clamp::BLEND_INPUT);
      return nullptr;
   }

   void OnInit(bool async) override
   {
      std::vector<ShaderDefineData> game_shader_defines_data = {
         {"TONEMAP_TYPE", '1', true, false, "0 - SDR: Vanilla (clamped reference)\n1 - HDR: extended native grade + MacLeod-Boynton hue emulation + C-infinity display map (SDR displays keep the vanilla grade)", 1},
      };
      shader_defines_data.append_range(game_shader_defines_data);
      assert(shader_defines_data.size() < MAX_SHADER_DEFINES);

      // Core registers the 6 SMAA passes and RCAS (ENABLE_RCAS); these are this game's own.
      // SMAA predication: the scene alpha's linear depth -> R16F plane-deviation edge-ness
      native_shaders_definitions.emplace(CompileTimeStringHash("ME Depth Extract CS"),
         ShaderDefinition{.file_name = "Luma_ME_DepthExtract", .type = reshade::api::pipeline_subobject_type::compute_shader});
      // SMAA T2x's own passes ("SMAA_T2X" in Luma_SMAA_impl.hlsl). The edge detection and the vertex shaders are 1x's.
      native_shaders_definitions.emplace(CompileTimeStringHash("ME SMAA T2x Blending Weight Calculation PS"),
         ShaderDefinition{.file_name = "Luma_SMAA_impl", .type = reshade::api::pipeline_subobject_type::pixel_shader, .function_name = "smaa_blending_weight_calculation_ps", .defines_data = {{"SMAA_T2X", "1"}}});
      native_shaders_definitions.emplace(CompileTimeStringHash("ME SMAA T2x Neighborhood Blending PS"),
         ShaderDefinition{.file_name = "Luma_SMAA_impl", .type = reshade::api::pipeline_subobject_type::pixel_shader, .function_name = "smaa_neighborhood_blending_ps", .defines_data = {{"SMAA_T2X", "1"}}});
      native_shaders_definitions.emplace(CompileTimeStringHash("ME SMAA T2x Resolve PS"),
         ShaderDefinition{.file_name = "Luma_SMAA_impl", .type = reshade::api::pipeline_subobject_type::pixel_shader, .function_name = "smaa_resolve_ps", .defines_data = {{"SMAA_T2X", "1"}}});

      // DLSS/FSR/Luma TAA: their depth and the camera motion from the scene's alpha, the CPU copies of vc4 (dgVoodoo maps it or updates
      // it), and the motion vector target written by every blend state
      sr_game_tooltip = "DLSS, FSR and XeSS require Luma-Upscaler.exe next to the game's exe (Luma TAA doesn't).\n";
      native_shaders_definitions.emplace(CompileTimeStringHash("ME Motion Vector Fill CS"),
         ShaderDefinition{.file_name = "Luma_ME_MotionVectorFill", .type = reshade::api::pipeline_subobject_type::compute_shader});
      native_shaders_definitions.emplace(CompileTimeStringHash("ME Copy Back PS"),
         ShaderDefinition{.file_name = "Luma_ME_CopyBack", .type = reshade::api::pipeline_subobject_type::pixel_shader});
      // The weighted MSAA resolve in TdToneMapping's exposure (1x1, exposure / 64, last frame's: the game builds this frame's after the
      // resolves; unbound before the first tonemap). Alpha is the linear view depth (sky 65504) the post passes read (TdDirectionalHaze,
      // DOF, bloom's sky exclusion): the nearest sample's, the game's average for the development A/B.
      ShaderDefinition msaa_resolve{.file_name = "Luma_MSAAResolve_PS", .type = reshade::api::pipeline_subobject_type::pixel_shader, .defines_data = {{"MSAA_RESOLVE_EXPOSURE", "1"}, {"MSAA_RESOLVE_EXPOSURE_SCALE", "64.0"}}};
#if DEVELOPMENT
      native_shaders_definitions.emplace(CompileTimeStringHash("ME MSAA Resolve Average Depth PS"), msaa_resolve);
#endif
      msaa_resolve.defines_data.push_back({"MSAA_RESOLVE_ALPHA_MIN", "1"});
      native_shaders_definitions.emplace(CompileTimeStringHash("MSAA Resolve PS"), std::move(msaa_resolve));
      SetEvents(true);

#if DEVELOPMENT
      // For the MCP "luma_dev_values" tool
      Mcp::RegisterToggles({{"smaa_enable", &g_smaa_enable}, {"smaa_t2x", &g_smaa_t2x}, {"smaa_predication", &g_smaa_predication}, {"hide_ui", &g_hide_ui}, {"mv_enable", &g_mv_enable},
         {"mv_debug_view", &g_mv_debug_view}, {"mv_force_jitter", &g_mv_force_jitter}, {"sr_reactive_enable", &g_sr_reactive_enable}, {"sr_reactive_debug_view", &g_sr_reactive_debug_view},
         {"msaa_resolve_enable", &g_msaa_resolve_enable}, {"msaa_resolve_average_depth", &g_msaa_resolve_average_depth}, { "msaa_coverage_enable",
            &g_msaa_coverage_enable }});
      Mcp::RegisterValues({{"rcas_sharpness", &g_rcas_sharpness, 0.f, 1.f}, {"smaa_pred_tolerance", &g_smaa_pred_tolerance, 0.002f, 0.2f}, {"sr_reactive_scale", &g_sr_reactive_scale, 0.f, 4.f},
         {"sr_reactive_threshold", &g_sr_reactive_threshold, 0.f, 1.f}});
      Mcp::RegisterTextures({MCP_GAME_TEXTURE("smaa.input", tex_input_encoded), MCP_GAME_TEXTURE("smaa.pred_mask", tex_pred), MCP_GAME_TEXTURE("smaa.output", tex_smaa_out),
         MCP_GAME_TEXTURE("mv.velocity", mv.texture), MCP_GAME_TEXTURE("mv.depth", mv.device_depth), MCP_GAME_TEXTURE("sr.reactive", mv.reactive),
         MCP_GAME_TEXTURE("sr.transparency", mv.transparency), MCP_GAME_TEXTURE("sr.draws_mask", mv.reactive_target)});
#endif

      // The canvas stays in gamma space: the HUD blends onto it right after TdMotionBlur. The tonemap pre-scales by
      // Game/UI paper white (UI_DRAW_TYPE 2), composition encodes scRGB.
      GetShaderDefineData(POST_PROCESS_SPACE_TYPE_HASH).SetDefaultValue('0');
      GetShaderDefineData(EARLY_DISPLAY_ENCODING_HASH).SetDefaultValue('0');
      GetShaderDefineData(VANILLA_ENCODING_TYPE_HASH).SetDefaultValue('1');
      GetShaderDefineData(GAMMA_CORRECTION_TYPE_HASH).SetDefaultValue('1');
      GetShaderDefineData(GAMUT_MAPPING_TYPE_HASH).SetDefaultValue('1');
      GetShaderDefineData(UI_DRAW_TYPE_HASH).SetDefaultValue('2');

      // dgVoodoo's translated shaders use b3 (its state) and b4 (the D3D9 constants)
      luma_settings_cbuffer_index = 13;
      luma_data_cbuffer_index = 12;
      luma_ui_cbuffer_index = -1;

      // Scene and UI paper white sliders (the UI one needs UI_DRAW_TYPE >= 1 and no OS reference white)
      use_os_reference_white_level = false;

      default_luma_global_game_settings.Exposure = 1.f;
      default_luma_global_game_settings.Contrast = 1.f;
      default_luma_global_game_settings.Saturation = 1.f;
      default_luma_global_game_settings.HighlightsDesaturation = 0.f;
      default_luma_global_game_settings.ColorGradingIntensity = 1.f;
      default_luma_global_game_settings.Dithering = 1.f;
      // Bink bypasses the scene passes: without AutoHDR the movies sit flat at paper white. BL2's calibrated pair.
      default_luma_global_game_settings.VideoAutoHDREnable = 1.f;
      default_luma_global_game_settings.VideoAutoHDRBoost = 0.5f;
      cb_luma_global_settings.GameSettings = default_luma_global_game_settings;
   }

   void OnCreateDevice(ID3D11Device* native_device, DeviceData& device_data) override
   {
      device_data.game = new MirrorsEdgeGameDeviceData;
#if DEVELOPMENT
      // For the MCP "luma_dev_values" tool: the last complete frame's counts
      auto& gd = GetGameDeviceData(device_data);
      SceneMotionVectors::RegisterDevValues(gd.mv, &device_data);
      Mcp::RegisterCounter("msaa_resolves", &gd.last_msaa_resolves, &device_data);
#endif
   }

   void OnDestroyDeviceData(DeviceData& device_data) override
   {
#if DEVELOPMENT
      Mcp::Unregister(&device_data);
#endif
      Game::OnDestroyDeviceData(device_data);
   }

   bool OverrideCopyResource(ID3D11Device* native_device, DeviceData& device_data, uint64_t& dst_resource, uint64_t& src_resource) override
   {
      auto& gd = GetGameDeviceData(device_data);
      if (gd.mv.scene_open && gd.mv.scene_color && src_resource == reinterpret_cast<uintptr_t>(gd.mv.scene_color.get()))
      {
         gd.mv_scene_last_copy = reinterpret_cast<ID3D11Resource*>(dst_resource);
      }
      return false;
   }

   void OnPresent(ID3D11Device* native_device, DeviceData& device_data) override
   {
      auto& gd = GetGameDeviceData(device_data);
      // SMAA's resources go once it stopped running, the snapshot too once RCAS stopped as well (MEM-11); between frames
      if (cb_luma_global_settings.FrameIndex - gd.smaa_frame > IDLE_RELEASE_FRAMES)
      {
         gd.ReleaseSMAAScratch();
         ReleaseSMAAIntermediates(device_data);
      }
      if (cb_luma_global_settings.FrameIndex - gd.snapshot_frame > IDLE_RELEASE_FRAMES)
      {
         gd.ReleaseSnapshotScratch();
      }
      if (gd.tex_smaa_out && cb_luma_global_settings.FrameIndex - gd.smaa_out_frame > IDLE_RELEASE_FRAMES)
      {
         gd.ReleaseSMAAOutput();
      }
      // Captured again at the next tonemap and TdMotionBlur; holding them would keep the game's targets alive across a resize
      gd.srv_scene_depth.reset();
      const bool msaa_was_active = gd.msaa_active;
      gd.msaa_active = std::exchange(gd.msaa_resolved, false);
      if (gd.msaa_active && !msaa_was_active)
      {
         reshade::log::message(reshade::log::level::warning, "[Mirror's Edge] The game's MSAA is on: DLAA, FSR, Luma TAA and SMAA T2x stay off until Anti-Aliasing is off in the game's display settings");
      }
      gd.canvas_res.reset();
      gd.open_refused_dsv = nullptr;

      // DLSS/FSR/Luma TAA: the history restarts after any frame it didn't draw (menus, loading, just picked); the selection and the
      // motion vector state are fixed here for the next frame (see "sr_active")
      gd.mv_jitter_allowed = device_data.has_drawn_sr;
      // Not on a multisampled scene (the motion vector draws refuse it): with MSAA the motion vector path stays off
      gd.sr_active = LatchSRFrame(device_data) && !gd.msaa_active;
      // SMAA T2x runs on the upscalers' motion vectors and gives way to an upscaler. Turned off, its frames go, and the motion vectors
      // go as when no upscaler is picked.
      gd.t2x_phase = -1;
      gd.mv_filled = false;
      const bool t2x_was_active = gd.t2x_active;
      gd.t2x_active = g_smaa_enable && g_smaa_t2x && !gd.sr_active && !gd.msaa_active;
      if (t2x_was_active && !gd.t2x_active)
      {
         gd.ReleaseT2xFrames();
         device_data.release_sr_resources = true;
      }
      if (gd.t2x_frames[0] && cb_luma_global_settings.FrameIndex - gd.t2x_frame > IDLE_RELEASE_FRAMES)
      {
         gd.ReleaseT2xFrames();
      }
      // The uploads the buffer hooks missed while motion vectors were off make every earlier copy stale
      const bool mv_active = (gd.sr_active || gd.t2x_active || g_mv_enable) && !gd.msaa_active;
      if (!gd.mv.active.exchange(mv_active) && mv_active)
      {
         gd.mv.constants.Restart();
      }
      // None picked: Core stopped the SR bridge's helper ("ReleaseResources"), our upscaler inputs and output go too. Recreated when an
      // upscaler is picked again (the helper takes seconds to start).
      if (device_data.sr_type == SR::Type::None && device_data.release_sr_resources.exchange(false))
      {
         gd.sr_output_srv.reset();
         if (!g_mv_enable && !gd.t2x_active)
         {
            const std::unique_lock lock(gd.mv.mutex);
            gd.mv.ReleaseMotionVectorTargets();
            // The game's scene, depth and scene copy (taken again at the next scene), so a resize after None doesn't keep the old ones
            // alive; and the vc4 copies the object tables and the cameras hold
            gd.mv.depth.reset();
            gd.mv.scene_color.reset();
            gd.mv.scene_rtv.reset();
            gd.mv_scene_srv.reset();
            gd.mv_scene_copy.reset();
            gd.mv_scene_last_copy.reset();
            gd.mv_scene_copy_rtv.reset();
            gd.mv.objects.Reset();
            gd.mv.camera.reset();
            gd.mv.previous_camera.reset();
            gd.mv.fill_buffer.reset();
            gd.mv.jitter_buffer.reset();
            gd.mv.previous_constants = {}; // Its 4 MiB dynamic ring is address space too
            gd.mv.constants.ReleasePool();
         }
      }
      gd.mv.constants.TrimPool(gd.mv.active);
#if DEVELOPMENT
      gd.mv.stats.constants_pool = uint32_t(gd.mv.constants.pool.size());
#endif
      // The reactive masks go once the fill stopped writing them (DLSS, the masks off, SMAA), like SMAA's resources above
      if (gd.mv.reactive_target && cb_luma_global_settings.FrameIndex - gd.mv.sr_reactive_frame > IDLE_RELEASE_FRAMES)
      {
         const std::unique_lock lock(gd.mv.mutex);
         gd.mv.ReleaseReactiveMasks();
      }
      // A scene no post pass ended ends here: its jitter must not reach the next frame's draws before its first mesh
      gd.mv.scene_open = false;
      gd.mv.scene_done = false;
      gd.mv.frame_ended = true;
      gd.mv.fill_pending = false;
      // -1 at native resolution, 0 with XeSS once the upscaler draws (Core biases the game's anisotropic samplers, the trilinear ones upgraded to
      // AF16x); none while the bridge's helper starts (SMAA on the unjittered scene)
      SetTextureMipLodBias(nullptr, device_data, ((gd.sr_active && gd.mv_jitter_allowed) ? SR::GetMipLODBias(device_data.sr_type, device_data.output_resolution.y, device_data.output_resolution.y) : 0.f));
#if DEVELOPMENT
      gd.mv.last_stats = std::exchange(gd.mv.stats, {});
      gd.last_msaa_resolves = std::exchange(gd.msaa_resolves, 0);
      // "MV Debug View": Core's debug draw of the target, absolute values in pixels
      {
         const std::shared_lock lock(gd.mv.mutex);
         if (g_mv_debug_view && gd.mv.texture)
         {
            D3D11_TEXTURE2D_DESC desc = {};
            gd.mv.texture->GetDesc(&desc);
            debug_draw_auto_clear_texture = false;
            debug_draw_options |= (uint32_t)DebugDrawTextureOptionsMask::Abs | (uint32_t)DebugDrawTextureOptionsMask::UVToPixelSpace;
            device_data.debug_draw_texture = gd.mv.texture.get();
            device_data.debug_draw_texture_format = desc.Format;
            device_data.debug_draw_texture_size = {desc.Width, desc.Height, 1, 1};
         }
         else if (g_sr_reactive_debug_view && gd.mv.reactive)
         {
            debug_draw_auto_clear_texture = false;
            debug_draw_options |= (uint32_t)DebugDrawTextureOptionsMask::RedOnly;
            debug_draw_options &= ~((uint32_t)DebugDrawTextureOptionsMask::Abs | (uint32_t)DebugDrawTextureOptionsMask::UVToPixelSpace);
            device_data.debug_draw_texture = gd.mv.reactive.get();
            device_data.debug_draw_texture_format = DXGI_FORMAT_R8_UNORM;
            device_data.debug_draw_texture_size = {uint32_t(device_data.output_resolution.x), uint32_t(device_data.output_resolution.y), 1, 1};
         }
         else if (device_data.debug_draw_texture && (device_data.debug_draw_texture.get() == gd.mv.texture.get() || device_data.debug_draw_texture.get() == gd.mv.reactive.get()))
         {
            device_data.debug_draw_texture = nullptr;
         }
      }
#endif
   }

   void LoadConfigs() override
   {
      reshade::get_config_value(nullptr, NAME, "SMAAEnable", g_smaa_enable);
      reshade::get_config_value(nullptr, NAME, "SMAAT2x", g_smaa_t2x);
      reshade::get_config_value(nullptr, NAME, "RCASSharpness", g_rcas_sharpness);
      reshade::get_config_value(nullptr, NAME, "SMAAPredication", g_smaa_predication);
      reshade::get_config_value(nullptr, NAME, "SMAAPredicationTolerance", g_smaa_pred_tolerance);
      reshade::get_config_value(nullptr, NAME, "MSAAResolveEnable", g_msaa_resolve_enable);
      auto& gs = cb_luma_global_settings.GameSettings;
      reshade::get_config_value(nullptr, NAME, "Exposure", gs.Exposure);
      reshade::get_config_value(nullptr, NAME, "Contrast", gs.Contrast);
      reshade::get_config_value(nullptr, NAME, "Saturation", gs.Saturation);
      reshade::get_config_value(nullptr, NAME, "HighlightsDesaturation", gs.HighlightsDesaturation);
      reshade::get_config_value(nullptr, NAME, "ColorGradingIntensity", gs.ColorGradingIntensity);
      reshade::get_config_value(nullptr, NAME, "Dithering", gs.Dithering);
      reshade::get_config_value(nullptr, NAME, "VideoAutoHDREnable", gs.VideoAutoHDREnable);
      reshade::get_config_value(nullptr, NAME, "VideoAutoHDRBoost", gs.VideoAutoHDRBoost);
   }

   void DrawImGuiSettings(DeviceData& device_data) override
   {
      ImGui::SeparatorText("Anti-Aliasing");
      const auto& gd = GetGameDeviceData(device_data);
      if (gd.msaa_active)
      {
         ImGui::TextColored(ImVec4(1.f, 0.8f, 0.f, 1.f), "MSAA is on: DLAA, FSR, Luma TAA and SMAA T2x need Anti-Aliasing off in the game's display settings.");
      }
      // Only does something with the game's MSAA
      DrawSettingCheckbox("Luma MSAA Enable", "MSAAResolveEnable", &g_msaa_resolve_enable, true, "Replaces the game's MSAA with an HDR-aware one (smoother edges on bright lights, sky, foliage and fences; requires Anti-Aliasing enabled in the game's display settings; not used with DLSS/FSR or Luma TAA).", !gd.msaa_active);
      // The upscaler (Super Resolution, in the Settings tab) replaces SMAA. Not with MSAA ("sr_active"): SMAA runs then
      DrawSettingCheckbox("SMAA Enable", "SMAAEnable", &g_smaa_enable, true, "Adds SMAA anti-aliasing on top of the game's own (most noticeable with Anti-Aliasing off in the game's display settings; not used with DLSS/FSR or Luma TAA).", gd.sr_active);
      // T2x needs the motion vectors, which a multisampled scene doesn't get
      const bool t2x_unavailable = gd.sr_active || gd.msaa_active;
      ImGui::BeginDisabled(!g_smaa_enable || t2x_unavailable);
      DrawSettingCheckbox("SMAA T2x", "SMAAT2x", &g_smaa_t2x, false, "Smoother edges and less shimmering, by blending each frame with the previous one.\nCan look slightly softer in motion. Not used with DLSS/FSR, Luma TAA or the game's MSAA (Anti-Aliasing in the game's display settings).", t2x_unavailable);
      ImGui::EndDisabled();
      ImGui::BeginDisabled(!g_smaa_enable && !gd.sr_active);
      DrawSettingSlider("RCAS Sharpness", "RCASSharpness", &g_rcas_sharpness, 0.f, 0.f, 1.f, "Sharpening applied on top of SMAA, DLSS/FSR or Luma TAA (0 = off).");
      ImGui::EndDisabled();
#if DEVELOPMENT
      DrawSettingCheckbox("SMAA Predication", "SMAAPredication", &g_smaa_predication, true, nullptr);
      DrawSettingSlider("SMAA Predication Tolerance", "SMAAPredicationTolerance", &g_smaa_pred_tolerance, 0.02f, 0.002f, 0.2f, nullptr, "%.3f", ImGuiSliderFlags_Logarithmic);

      ImGui::SeparatorText("Motion Vectors (DLSS/FSR/Luma TAA)");
      ImGui::Checkbox("MV Enable", &g_mv_enable);
      if (ImGui::IsItemHovered())
      {
         ImGui::SetTooltip("Draws the scene with the motion vector shaders without an upscaler. The image must not change; the debug view is\nblack with a static camera and lights up only what moves. Not saved.");
      }
      ImGui::Checkbox("MV Debug View", &g_mv_debug_view);
      ImGui::Checkbox("MV Force Jitter", &g_mv_force_jitter);
      ImGui::Checkbox("Reactive Mask", &g_sr_reactive_enable);
      ImGui::Checkbox("Reactive Mask Debug View", &g_sr_reactive_debug_view);
#endif

      auto& gs = cb_luma_global_settings.GameSettings;
      const auto& gs_def = default_luma_global_game_settings;
      ImGui::SeparatorText("Grade");

      DrawSettingSlider("Exposure", "Exposure", &gs.Exposure, gs_def.Exposure, 0.f, 2.f, "Overall image brightness (1 = vanilla).");
      DrawSettingSlider("Contrast", "Contrast", &gs.Contrast, gs_def.Contrast, 0.f, 2.f, "Overall image contrast, HDR only (1 = vanilla).");
      DrawSettingSlider("Saturation", "Saturation", &gs.Saturation, gs_def.Saturation, 0.f, 2.f, "Color saturation, HDR only (1 = vanilla).");
      DrawSettingSlider("Highlights Desaturation", "HighlightsDesaturation", &gs.HighlightsDesaturation, gs_def.HighlightsDesaturation, 0.f, 1.f,
         "How far the brightest sources fade to neutral white, HDR only (0 = keep color at any brightness).");
      DrawSettingSlider("Color Grading Intensity", "ColorGradingIntensity", &gs.ColorGradingIntensity, gs_def.ColorGradingIntensity, 0.f, 1.f,
         "Strength of the game's own color grading (1 = vanilla, 0 = neutral).");

      ImGui::SeparatorText("Effects");
      DrawSettingCheckbox("Video AutoHDR", "VideoAutoHDREnable", &gs.VideoAutoHDREnable, gs_def.VideoAutoHDREnable, "Adds HDR highlights to pre-rendered videos (HDR only).");
      if (gs.VideoAutoHDREnable > 0.5f)
      {
         DrawSettingSlider("Video HDR Boost", "VideoAutoHDRBoost", &gs.VideoAutoHDRBoost, gs_def.VideoAutoHDRBoost, 0.f, 1.f, "Video highlight strength (0 = off).");
      }
      DrawSettingCheckbox("Dithering", "Dithering", &gs.Dithering, gs_def.Dithering, "Reduces gradient banding.");

      ImGui::SeparatorText("UI");
      ImGui::Checkbox("Hide Gameplay UI", &g_hide_ui);
      if (ImGui::IsItemHovered())
      {
         ImGui::SetTooltip("Disables the in-game UI.");
      }
   }

   DrawOrDispatchOverrideType OnDrawOrDispatch(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, CommandListData& cmd_list_data, DeviceData& device_data, reshade::api::shader_stage stages, const ShaderHashesList<OneShaderPerPipeline>& original_shader_hashes, bool is_custom_pass, bool& updated_cbuffers, std::function<void()>* original_draw_dispatch_func) override
   {
      const bool is_immediate = native_device_context->GetType() == D3D11_DEVICE_CONTEXT_IMMEDIATE;
      auto& gd = GetGameDeviceData(device_data);
      // The game's lists are all pixel shaders: looked up by this hash (a list's "Contains" of another walks the list)
      const uint32_t pixel_shader_hash = uint32_t(original_shader_hashes.pixel_shaders[0]);
      // A game draw (not Luma's) that can be drawn again with patched shaders
      const bool game_draw = !is_custom_pass && original_draw_dispatch_func && *original_draw_dispatch_func;
      // Render target 0's resource (null without one)
      const auto bound_render_target_resource = [&]
      {
         com_ptr<ID3D11RenderTargetView> rtv;
         native_device_context->OMGetRenderTargets(1, &rtv, nullptr);
         com_ptr<ID3D11Resource> resource;
         if (rtv)
         {
            rtv->GetResource(&resource);
         }
         return resource;
      };
      if (is_immediate && shader_hashes_TdMotionBlur.Contains(pixel_shader_hash, reshade::api::shader_stage::pixel))
      {
         gd.canvas_res = bound_render_target_resource();
      }
      // Hide UI: cancel this frame's later draws into the canvas (the HUD, menus and overlays; "canvas_res" only lives from TdMotionBlur
      // to the present)
      else if (g_hide_ui && is_immediate && gd.canvas_res && !is_custom_pass)
      {
         // Any view of the canvas
         if (bound_render_target_resource() == gd.canvas_res)
            return DrawOrDispatchOverrideType::Replaced;
      }

      // MSAA (4x and up, as Saints Row: The Third's: 2x gives a coarser edge than the alpha test): the masked materials' draws into
      // the multisampled scene with their alpha to coverage version (see "CoveragePatch::PatchPixelShader"), the game's shader put back after
      // Ahead of the motion vector path, which refuses multisampled scenes anyway
      if (g_msaa_resolve_enable && g_msaa_coverage_enable && gd.msaa_active && is_immediate && game_draw && original_shader_hashes.pixel_shaders[0] != UINT64_MAX)
      {
         // The target first: most draws with MSAA on (the UI, post) aren't multisampled, and need no shader lookup
         com_ptr<ID3D11Texture2D> rt_texture;
         D3D11_TEXTURE2D_DESC rt_desc = {};
         if (const com_ptr<ID3D11Resource> rt = bound_render_target_resource(); rt && SUCCEEDED(rt->QueryInterface(&rt_texture)))
         {
            rt_texture->GetDesc(&rt_desc);
         }
         if (rt_desc.SampleDesc.Count >= 4)
         {
            if (ID3D11PixelShader* const coverage_shader = SceneMotionVectors::GetPatchedShader<MotionVectorGame, ID3D11PixelShader>(&gd.mv, native_device, device_data, pixel_shader_hash, cmd_list_data.pipeline_state_original_pixel_shader, COVERAGE_PATCH).shader.get())
            {
               com_ptr<ID3D11PixelShader> game_shader;
               native_device_context->PSGetShader(&game_shader, nullptr, nullptr);
               native_device_context->PSSetShader(coverage_shader, nullptr, 0);
               (*original_draw_dispatch_func)();
               native_device_context->PSSetShader(game_shader.get(), nullptr, 0);
               return DrawOrDispatchOverrideType::Replaced;
            }
         }
      }

      // DLAA/FSR/Luma TAA and SMAA T2x: the scene's draws with motion vectors or jitter (see "DrawWithMotionVectors"), and the fill and
      // upscaler at its first post pass
      if (gd.mv.active && is_immediate)
      {
         // With a vertex shader, which the patched shaders can redo
         const bool game_mesh_draw = game_draw && (stages & reshade::api::shader_stage::vertex) == reshade::api::shader_stage::vertex;
         // TdDirectionalHaze writes the hazed scene into another texture (from the scene's copy), which the post passes read instead
         if (gd.mv.scene_open && shader_hashes_TdDirectionalHaze.Contains(pixel_shader_hash, reshade::api::shader_stage::pixel))
         {
            gd.mv_scene_last_copy = bound_render_target_resource();
         }
         if (!gd.mv.scene_done && shader_hashes_ScenePost.Contains(pixel_shader_hash, reshade::api::shader_stage::pixel))
         {
            if (gd.mv.scene_open)
            {
#if DEVELOPMENT
               gd.mv.stats.ended_by = pixel_shader_hash;
#endif
               // The scene's last copy, if the pass reads it
               gd.mv_scene_copy.reset();
               if (gd.mv_scene_last_copy)
               {
                  com_ptr<ID3D11ShaderResourceView> srvs[8];
                  native_device_context->PSGetShaderResources(0, 8, &srvs[0]);
                  for (const auto& srv : srvs)
                  {
                     com_ptr<ID3D11Resource> resource;
                     if (srv)
                     {
                        srv->GetResource(&resource);
                     }
                     if (resource == gd.mv_scene_last_copy)
                     {
                        gd.mv_scene_copy = resource;
                        break;
                     }
                  }
               }
               EndScene(native_device, native_device_context, device_data);
            }
         }
         // Until the scene opens only the depth view matters (the shadow maps and the menu's 1280x720 pass come first)
         else if (!gd.mv.scene_done && !gd.mv.scene_open && game_mesh_draw)
         {
            com_ptr<ID3D11DepthStencilView> scene_dsv;
            native_device_context->OMGetRenderTargets(0, nullptr, &scene_dsv);
            if (scene_dsv && scene_dsv.get() != gd.open_refused_dsv)
            {
               OpenScene(native_device, native_device_context, cmd_list_data, device_data, original_shader_hashes.vertex_shaders[0], scene_dsv.get());
            }
         }
         if (gd.mv.scene_open && game_mesh_draw)
         {
            com_ptr<ID3D11RenderTargetView> rtvs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
            com_ptr<ID3D11DepthStencilView> dsv;
            native_device_context->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, &rtvs[0], &dsv);
            const bool motion_vectors = SceneMotionVectors::DrawWithMotionVectors<MotionVectorGame>(&gd.mv, native_device, native_device_context, cmd_list_data, device_data, original_shader_hashes, *original_draw_dispatch_func, rtvs, dsv.get());
            // An alpha blending state blends the motion vector target by the alpha written there: a draw that doesn't write it (refused
            // above) must not have it bound
            if (!motion_vectors && rtvs[SceneMotionVectors::target_slot] && rtvs[SceneMotionVectors::target_slot] == gd.mv.rtv)
            {
               SceneMotionVectors::ClassifyBoundBlend(native_device_context, &gd.mv);
               if (gd.mv.alpha_blended)
               {
                  rtvs[SceneMotionVectors::target_slot].reset();
                  ID3D11RenderTargetView* targets[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
                  for (UINT slot = 0; slot < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; slot++)
                  {
                     targets[slot] = rtvs[slot].get();
                  }
                  native_device_context->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, targets, dsv.get());
               }
            }
            const bool jitter = !motion_vectors && SceneMotionVectors::DrawWithJitter<MotionVectorGame>(&gd.mv, native_device, native_device_context, cmd_list_data, device_data, original_shader_hashes, *original_draw_dispatch_func, rtvs, dsv.get());
#if DEVELOPMENT
            Mcp::Annotate(cmd_list_data, motion_vectors ? "mv" : (jitter ? "jitter" : "unpatched"));
            if (const SceneMotionVectors::MotionVectorReject reject = std::exchange(gd.mv.draw_reject, SceneMotionVectors::REJECT_COUNT); reject != SceneMotionVectors::REJECT_COUNT)
            {
               Mcp::Annotate(cmd_list_data, "mv_reject", int(reject));
            }
#endif
            if (motion_vectors || jitter)
               return DrawOrDispatchOverrideType::Replaced;
         }
      }
      // Every draw without a patched shader: the game's own, if the last patched draw's are still bound
      if (is_immediate)
      {
         PatchedDraws::RestoreGameShader(native_device_context, &gd.mv.bound_vertex_shader);
         PatchedDraws::RestoreGameShader(native_device_context, &gd.mv.bound_pixel_shader);
      }

#if DEVELOPMENT
      // Census of the passes drawn onto the canvas after TdToneMapping (UI and LDR overlays): each pixel shader logged once, to find the
      // ones the clamp lists above miss. Keyed on the tonemap's frame, not "has_drawn_main_post_processing": the menu's 1280x720 scene
      // ends in GammaCorrection before the main scene is drawn.
      static std::atomic<uint32_t> tonemap_frame = UINT32_MAX;
#endif
      if (const bool tonemap = shader_hashes_ToneMapping.Contains(pixel_shader_hash, reshade::api::shader_stage::pixel); tonemap || shader_hashes_GammaCorrection.Contains(pixel_shader_hash, reshade::api::shader_stage::pixel))
      {
         device_data.has_drawn_main_post_processing = true;
#if DEVELOPMENT
         if (tonemap)
         {
            tonemap_frame = cb_luma_global_settings.FrameIndex;
         }
#endif
         if (tonemap && is_immediate)
         {
            native_device_context->PSGetShaderResources(3, 1, gd.srv_exposure.put());
            // TdToneMapping: run it, then SMAA on its target. The bindings are read here, before any "is_custom_pass" gate: the
            // replaced tonemap is a custom pass
            const bool antialias = (device_data.has_drawn_sr ? g_rcas_sharpness > 0.f : g_smaa_enable);
            if (antialias && original_draw_dispatch_func != nullptr)
            {
               ComPtr<ID3D11RenderTargetView> rtv;
               native_device_context->OMGetRenderTargets(1, rtv.put(), nullptr);
               // The scene (linear depth in .a) for the predication; the tonemap only reads it
               native_device_context->PSGetShaderResources(0, 1, gd.srv_scene_depth.put());
               // Returning Replaced skips Core's per-pass SetLumaConstantBuffers: upload them here or the draw reads last frame's values
               SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, reshade::api::shader_stage::pixel, LumaConstantBufferType::LumaSettings);
               updated_cbuffers = true;
               (*original_draw_dispatch_func)();
               if (rtv)
               {
                  // One state save and restore around the whole chain (its passes bind their own, see "RunPostTonemapSMAA")
                  DrawStateStack<DrawStateStackType::FullGraphics> graphics_state;
                  DrawStateStack<DrawStateStackType::Compute> compute_state;
                  graphics_state.Cache(native_device_context, device_data.uav_max_count);
                  compute_state.Cache(native_device_context, device_data.uav_max_count);
                  RunPostTonemapSMAA(native_device, native_device_context, device_data, &gd, rtv.get(), !device_data.has_drawn_sr);
                  compute_state.Restore(native_device_context);
                  graphics_state.Restore(native_device_context);
               }
               return DrawOrDispatchOverrideType::Replaced;
            }
         }
      }
#if DEVELOPMENT
      else if (tonemap_frame == cb_luma_global_settings.FrameIndex && (stages & reshade::api::shader_stage::pixel) != 0)
      {
         static std::shared_mutex census_mutex;
         static std::unordered_set<uint32_t> census;
         const std::unique_lock lock(census_mutex);
         if (const auto [it, inserted] = census.insert(pixel_shader_hash); inserted)
         {
            reshade::log::message(reshade::log::level::info,
               std::format("[Mirror's Edge] Post-tonemap PS 0x{:08X} ({}), frame {}", pixel_shader_hash, (ui_saturated_pixel_shaders.contains(pixel_shader_hash) ? "saturated" : (scene_blend_input_pixel_shaders.contains(pixel_shader_hash) ? "blend input" : "UNCLAMPED")), cb_luma_global_settings.FrameIndex).c_str());
         }
      }
#endif
      return DrawOrDispatchOverrideType::None;
   }

   void PrintImGuiAbout() override
   {
      ImGui::PushTextWrapPos(0.f);
      ImGui::Text(
         "Luma for \"Mirror's Edge\" is developed by DristoforColumb and is open source and free.\n"
         "It adds HDR, DLAA, FSR 3 native anti-aliasing or Luma TAA, SMAA (with T2x) and an HDR-aware MSAA with alpha to coverage, plus 16x anisotropic filtering.\n"
         "It runs through dgVoodoo2 (DirectX 9 -> 11).\n"
         "Do NOT run another HDR mod (e.g. RenoDX) alongside it.\n"
         "Thanks to the Luma team and contributors.\n"
         "If you enjoy it, consider donating.");
      ImGui::PopTextWrapPos();

      ImGui::NewLine();
      ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(70, 134, 0, 255));
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(70 + 9, 134 + 9, 0, 255));
      ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(70 + 18, 134 + 18, 0, 255));
      static const std::string donation_link = std::string("Buy DristoforColumb a Coffee on ko-fi ") + std::string(ICON_FK_OK);
      if (ImGui::Button(donation_link.c_str()))
      {
         ShellExecuteA(nullptr, "open", "https://ko-fi.com/dristoforcolumb", nullptr, nullptr, SW_SHOWNORMAL);
      }
      ImGui::PopStyleColor(3);

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
      {
         ShellExecuteA(nullptr, "open", "https://github.com/Filoppi/Luma-Framework", nullptr, nullptr, SW_SHOWNORMAL);
      }

      ImGui::NewLine();
      ImGui::Text("Build Date: %s %s", __DATE__, __TIME__);

      ImGui::NewLine();
      ImGui::Text("Credits:"
                  "\n\nMain:"
                  "\nDristoforColumb"
                  "\n\nThird Party:"
                  "\nReShade"
                  "\nImGui"
                  "\nRenoDX (HDR tonemap method)"
                  "\nMacLeod-Boynton hue emulation (RenoDX)"
                  "\nSMAA (Iryoku)"
                  "\nAMD FidelityFX (RCAS + FSR 3)"
                  "\nNVIDIA NGX (DLSS)"
                  "\ndgVoodoo2 by Dege (DirectX 9 -> 11 wrapper, required)");
   }
};

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
   if (ul_reason_for_call == DLL_PROCESS_ATTACH)
   {
      const char* project_name = PROJECT_NAME;
      const char* cleared_project_name = ((project_name[0] == '_') ? (project_name + 1) : project_name);

      uint32_t mod_version = 1;
      Globals::SetGlobals(cleared_project_name, "Mirror's Edge Luma mod", "", mod_version);

      swapchain_format_upgrade_type = TextureFormatUpgradesType::AllowedEnabled;
      swapchain_upgrade_type = SwapchainUpgradeType::scRGB;

      // "force_borderless" also covers leaving fullscreen, so the window can't come back with a title bar (TW2)
      prevent_fullscreen_state = true;
      force_borderless = true;

      // The scene is already fp16 (alpha = linear depth); TdToneMapping writes fp16 too, then TdMotionBlur and the HUD
      // draw into the r8g8b8a8_typeless canvas that is copied to the swapchain: that canvas is the only HDR clip.
      // Indirect (mirror substituted at bind): changing a dgVoodoo resource's creation format breaks its bookkeeping
      // (black screen, MOHA/BL2). The A16B16G16R16 UNORM exposure and bloom buffers stay UNORM: their passes rely on its [0, 1] clamp
      // (the exposure sees clipped input, bloom is stored / 4; _tools/mirrors_edge/NOTES.md).
      texture_format_upgrades_type = TextureFormatUpgradesType::AllowedEnabled;
      enable_indirect_texture_format_upgrades = true;
      enable_chain_indirect_texture_format_upgrades = ChainTextureFormatUpgradesType::DirectDependencies;
      texture_upgrade_formats = {
         reshade::api::format::r8g8b8a8_typeless,
      };
      // "No1Px" as in the other dgVoodoo mods: the wrapper binds 1x1 placeholders in unused sampler slots, which pass the aspect filter
      texture_format_upgrades_2d_size_filters = (uint32_t)TextureFormatUpgrades2DSizeFilters::SwapchainResolution | (uint32_t)TextureFormatUpgrades2DSizeFilters::SwapchainAspectRatio | (uint32_t)TextureFormatUpgrades2DSizeFilters::No1Px;

      // The upscalers' mip bias (see "OnPresent") reaches only upgraded samplers. AF16x, as in the other UE3 dgVoodoo mods;
      // force_upgrade_linear_samplers also gives the trilinear ones the bias (Core rewrites only anisotropic samplers otherwise, TW2).
      enable_samplers_upgrade = true; // boot-time only (cannot be changed after device creation)
      samplers_upgrade_mode = 4;
      force_upgrade_linear_samplers = true;

#if DEVELOPMENT
      // Offline map of the global post/UI/video pixel shaders (cooked RefShaderCache type names), for traces and the DevKit.
      // Each name twice: 2.87.5, then 2.81.3
      for (const auto& [hash, name] : std::initializer_list<std::pair<const char*, const char*>>{
              {"19BCE666", "Td Directional Haze"},
              {"9C77EAD7", "Td Directional Haze"},
              {"2BE8B514", "DOF And Bloom Gather"},
              {"587465D1", "DOF And Bloom Gather"},
              {"B977A1C4", "DOF And Bloom Gather (Max Samples)"},
              {"69A1F64A", "DOF And Bloom Gather (Max Samples)"},
              {"780BCE69", "DOF And Bloom Blend"},
              {"8B012337", "DOF And Bloom Blend"},
              {"25752EDB", "Td Tone Map Exposure"},
              {"9F2AF349", "Td Tone Map Exposure"},
              {"1A760388", "Td Tone Mapping"},
              {"602DA054", "Td Tone Mapping"},
              {"0160196C", "Gamma Correction"},
              {"87D136D3", "Gamma Correction"},
              {"FC10C0BD", "Uber Post Process Blend"},
              {"39B803FA", "Uber Post Process Blend"},
              {"8F95BB29", "Motion Blur"},
              {"37EF3D5C", "Motion Blur"},
              {"FF77A7B0", "Motion Blur (Dynamic Velocities Only)"},
              {"E9224963", "Motion Blur (Dynamic Velocities Only)"},
              {"4177BC6E", "Td Motion Blur"},
              {"A99AD9DD", "Td Motion Blur"},
              {"C1095A48", "Td Contrast Setting"},
              {"8FC13045", "Td Contrast Setting"},
              {"50D42156", "Td Interpolate"},
              {"A9884E2F", "Td Interpolate"},
              {"BA7480CD", "Td Clear"},
              {"75EB6E89", "Td Clear"},
              {"F66C72BC", "Distortion Apply Screen"},
              {"B414DB90", "Distortion Apply Screen"},
              {"A5AC2B70", "Td UI Compositing"},
              {"FB0A84E8", "Td UI Compositing"},
              {"92F6FB51", "Td UI Blur Effect"},
              {"49DB6B1C", "Td UI Blur Effect"},
              {"E93BE915", "Td UI Blur Gather"},
              {"A288BD8B", "Td UI Blur Gather"},
              {"C58A89A8", "Td UI Blur Gather (Max Samples)"},
              {"C086B17E", "Td UI Blur Gather (Max Samples)"},
              {"F8554FA4", "Td UI Approximate Alpha"},
              {"643DB945", "Td UI Approximate Alpha"},
              {"088F76F5", "Simple Element"},
              {"889B0C84", "Simple Element"},
              {"D81C8836", "Simple Element Gamma"},
              {"3DFF6336", "Simple Element Gamma"},
              {"C39EFBF2", "Simple Element Masked Gamma"},
              {"5325C38B", "Simple Element Masked Gamma"},
              {"E41621CF", "Bink YCrCb To RGB"},
              {"7EBF990A", "Bink YCrCb To RGB"},
              {"B19ED21F", "Bink YCrCb To RGB (Alpha)"},
              {"C430F4E0", "Bink YCrCb To RGB (Alpha)"},
              {"39D9728E", "Ambient Occlusion (High Quality)"},
              {"AB839F6F", "Ambient Occlusion (High Quality)"},
              {"63885863", "Ambient Occlusion (Low Quality)"},
              {"C8D0904C", "Ambient Occlusion (Low Quality)"},
              {"F53BEC3C", "AO Apply"},
              {"2BD8E1EC", "AO Apply"},
              {"24BBD0BD", "AO History Update"},
              {"B8DAA094", "AO History Update"},
              {"9A32CD46", "Downsample Depth"},
              {"3A3184FB", "Downsample Depth"},
           })
      {
         forced_shader_names.emplace(Shader::Hash_StrToNum(hash), name);
      }
#endif

      game = new MirrorsEdge();
   }
   else if (ul_reason_for_call == DLL_PROCESS_DETACH)
   {
      MirrorsEdge::SetEvents(false);
   }

   CoreMain(hModule, ul_reason_for_call, lpReserved);

   return TRUE;
}
