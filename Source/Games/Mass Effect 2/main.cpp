// Mass Effect 2 (2010) Luma HDR mod: UE3, 32-bit, DX9 -> D3D11 via dgVoodoo2 2.87.3. Every hash here is 2.87.3-only.
// Pipeline, register map, perms and design record: NOTES.md.

// No DEVELOPMENT auto-debugger MessageBox on DLL attach: invisible under borderless/fullscreen and it blocks the
// loader (ReShade times out the addon load -> error 1114). Same failure as ME1/BL2/TW2 under dgVoodoo.
#define DISABLE_AUTO_DEBUGGER 1

#define GAME_MASS_EFFECT_2 1

#define ENABLE_NGX 0 // NGX is x64-only, the game is 32-bit, and there are no motion vectors anyway
#define ENABLE_FIDELITY_SK 0
#define GEOMETRY_SHADER_SUPPORT 0
// The game ships no AA at all, so SMAA adds rather than replaces. Core auto-registers the 6 "SMAA ..." passes.
#define ENABLE_SMAA 1
// Luma's multi-scale HDR bloom pyramid REPLACES the game's own quarter-res bright-pass glow (which the replaced
// gather then stops writing). Core auto-registers the 4 "Bloom ..." passes from Luma_Bloom_impl.hlsl.
#define ENABLE_BLOOM 1
// SMAA runs post-material via the post-draw callback. Outside DEVELOPMENT this define is what makes
// "original_draw_dispatch_func" non-null; without it the callback silently never fires.
#define ENABLE_POST_DRAW_DISPATCH_CALLBACK 1

#include "..\..\Core\core.hpp"
#include <shellapi.h> // ShellExecuteA for About links (system() hangs the render thread in exclusive fullscreen)
#include <unordered_set>

// ---- Measured pass hashes, dgVoodoo 2.87.3 (ps_5_0) then 2.81.3 (ps_4_0). Replacements are bound by FILE NAME, so
// a constant is only needed where the C++ keys on a pass (hooks, DEVELOPMENT telemetry). Evidence in NOTES.md.
static constexpr uint32_t kUberPostFilmicHash = 0xDB1022A7;  // UberPostProcessBlend + Hejl filmic curve (live perm)
static constexpr uint32_t kUberPostHash = 0x88CBF48C;        // same pass, tonemap-less perm (hard clip)
static constexpr uint32_t kMaterialHash = 0x277DA7AE;        // BioSceneEffect: vignette only (film grain off)
static constexpr uint32_t kMaterialGrainHash = 0xCF0CB35A;   // BioSceneEffect: vignette + film grain
static constexpr uint32_t kDofBloomGatherHash = 0x565795ED;  // DOFAndBloomGather, 16 taps (QualityBloom=TRUE)
static constexpr uint32_t kDofBloomGather4Hash = 0x3BEE36AD; // DOFAndBloomGather, 4 taps (QualityBloom=FALSE)
static constexpr uint32_t kGammaCorrectionHash = 0x0160196C; // UE3 FGammaCorrection: compiled, never yet observed drawing
static constexpr uint32_t kVideoHash = 0xE41621CF;           // Bink YUV->RGB
static constexpr uint32_t kCanvasTextHash = 0x89A6B3EC;      // UE3 Canvas: text glyph
static constexpr uint32_t kCanvasTileHash = 0x33FB5223;      // UE3 Canvas: tile/quad
static constexpr uint32_t kCanvasSolidHash = 0x1B3AC3BE;     // UE3 Canvas: solid quad, no texture
static constexpr uint32_t kCanvasSolidFadeHash = 0x33E6F2A1; // UE3 Canvas: solid quad, alpha faded by COLOR1.w
static constexpr uint32_t kCanvasTileTintHash = 0x2C849E5C;  // UE3 Canvas: tile lerped to the vertex colour, faded
static constexpr uint32_t kCanvasTwoTexHash = 0x99A94FD5;    // UE3 Canvas: two-texture crossfade
static constexpr uint32_t kSolidFillHash = 0xDE418D30;       // UE3 FOneColor: constant fill (fades, letterbox)
static constexpr uint32_t kSolidFillAlphaHash = 0xD8C503D9;  // the same with an interpolated alpha

// ---- The same passes under dgVoodoo 2.81.3, which emits ps_4_0 and therefore hashes differently. Dump-verified
// equivalent to their 2.87.3 twins apart from register numbering, `centroid` and rcp-vs-div (Bink and the Canvas tile
// are byte-identical). Found by constant/opcode fingerprint over 367 ps_4_0 shaders; FGammaCorrection has no entry
// because it never drew in that session either.
static constexpr uint32_t kUberPostFilmicHash_v281 = 0x9EE2F5B0;
static constexpr uint32_t kUberPostHash_v281 = 0xD95F610B;
static constexpr uint32_t kMaterialHash_v281 = 0xFEC7717C;
static constexpr uint32_t kMaterialGrainHash_v281 = 0xEA297C13;
static constexpr uint32_t kDofBloomGatherHash_v281 = 0xE5D70519;
static constexpr uint32_t kDofBloomGather4Hash_v281 = 0xD4C3E9E1;
static constexpr uint32_t kVideoHash_v281 = 0x7EBF990A;
static constexpr uint32_t kCanvasTextHash_v281 = 0x8DA32F73;
static constexpr uint32_t kCanvasTileHash_v281 = 0x6DB9CD5A;
static constexpr uint32_t kCanvasSolidHash_v281 = 0xC42B1F9B;
static constexpr uint32_t kCanvasSolidFadeHash_v281 = 0x82409C2F;
static constexpr uint32_t kCanvasTileTintHash_v281 = 0x7F95C9B7;
static constexpr uint32_t kCanvasTwoTexHash_v281 = 0x64C9AEC4;
static constexpr uint32_t kSolidFillHash_v281 = 0x7B090F39;
static constexpr uint32_t kSolidFillAlphaHash_v281 = 0x7504EFB1;

// Every pass the DEV first-draw log watches, both wrapper builds. Count taken FROM the array so a new hash cannot be
// added without the gate following it (same reason as g_bloom_sigmas below). Which half of the list reports also
// says which dgVoodoo build is live.
static constexpr uint32_t kLoggedPassHashes[] = {kUberPostFilmicHash, kUberPostHash, kMaterialHash, kMaterialGrainHash,
   kDofBloomGatherHash, kDofBloomGather4Hash, kGammaCorrectionHash, kVideoHash, kCanvasTextHash, kCanvasTileHash,
   kCanvasSolidHash, kCanvasSolidFadeHash, kCanvasTileTintHash, kCanvasTwoTexHash, kSolidFillHash, kSolidFillAlphaHash,
   kUberPostFilmicHash_v281, kUberPostHash_v281, kMaterialHash_v281, kMaterialGrainHash_v281,
   kDofBloomGatherHash_v281, kDofBloomGather4Hash_v281, kVideoHash_v281, kCanvasTextHash_v281, kCanvasTileHash_v281,
   kCanvasSolidHash_v281, kCanvasSolidFadeHash_v281, kCanvasTileTintHash_v281, kCanvasTwoTexHash_v281,
   kSolidFillHash_v281, kSolidFillAlphaHash_v281};

// User settings, persisted in the [Luma] config section (LoadConfigs) unless noted otherwise.
static bool g_hide_ui = false; // hide the game's HUD (for clean screenshots); session-only, never persisted
#if ENABLE_SMAA
static bool g_smaa_enable = true;
static bool g_smaa_predication = true;      // predicate SMAA on geometry, using the depth in the scene buffer's alpha
static float g_smaa_pred_tolerance = 0.02f; // plane deviation counted as a full edge, as a fraction of view depth
// RCAS sharpen on the SMAA output, opt-in at 0 (ME1/BL2/TW2 precedent): how much sharpening is wanted is a
// preference, not a target. At 0 the pass does not run and its full-resolution intermediate is never allocated.
static float g_rcas_sharpness = 0.f;
#if DEVELOPMENT
// Calibration aids for g_smaa_pred_tolerance, the only free parameter here: predication's effect is the ABSENCE of
// smearing, which the eye reads badly and worse in motion, so judge the mask itself instead of the frame.
static bool g_smaa_pred_debug = false;   // show the predication mask instead of the antialiased frame
static bool g_smaa_pred_measure = false; // one-shot: log the mask's coverage above 0.5 and percentiles
#endif
#endif

#if ENABLE_BLOOM
// Luma bloom pyramid mip 0, read by the uber replacements at register(t6): clear of the two slots those shaders
// actually declare (t0 scene, t1 blur).
static constexpr uint32_t kLumaBloomSlot = 6;
// ME1 2007's set, count taken FROM the array so the two cannot drift. The pyramid is energy-preserving, so the
// BloomScaleLive gain alone makes Bloom Intensity 1 vanilla strength; the wider shape is deliberate (NOTES.md).
static float g_bloom_sigmas[] = {1.5f, 2.f, 2.f, 2.f, 1.f, 0.5f};
// Mirrored into GameSettings.LumaBloomEnable, which both the uber (composite) and the replaced gather (stop writing
// the vanilla glow) read - so this single switch really swaps one bloom for the other.
static bool g_luma_bloom_enable = true;
#endif

struct MassEffect2GameDeviceData final : public GameDeviceData
{
   bool has_drawn_uber = false;
   bool has_drawn_material = false;

   // Wrong-wrapper guard (ME1's shape). Every replacement is bound by shader hash, and the hashes are dgVoodoo's
   // OUTPUT: any build other than 2.87.3 or 2.81.3 re-hashes all of them, so the whole mod goes silently inactive -
   // vanilla picture, dead sliders, not one error. Sticky across frames, unlike has_drawn_* above.
   bool ever_matched_keyed_pass = false;
   bool build_check_done = false;
   uint32_t frames_presented = 0;
#if DEVELOPMENT
   // One log line per pass per session, so a capture-free run still shows which perms this scene uses.
   std::unordered_set<uint32_t> logged_passes;
#endif

   // The canvas the material pass wrote into this frame, captured from its bound RTV. Consumed by Hide UI (which
   // needs to recognise later draws targeting it) and by the SMAA hook. Released every Present.
   ComPtr<ID3D11Resource> canvas_res;

#if ENABLE_SMAA || ENABLE_BLOOM
   // The game's fp16 scene from the UBER draw's t0: bloom-pyramid source, and its ALPHA is the linear depth SMAA
   // predication reads. Released every Present.
   ComPtr<ID3D11ShaderResourceView> srv_scene;
#endif

#if ENABLE_BLOOM
   // Deferred constant-buffer readback (ME1/MoHA/MELE shape): copy at the draw, map the copy made a few frames
   // earlier with a non-blocking Map, because the synchronous form would stall the GPU every frame.
   struct DeferredCBRing
   {
      static constexpr uint32_t kSlots = 3;
      ComPtr<ID3D11Buffer> staging[kSlots];
      uint32_t bytes = 0;
      uint32_t writes = 0;
      // One advance per frame, re-armed at Present: a second capture would evict the oldest slot before the GPU is
      // done, so the non-blocking Map would fail forever. The gather draws more than once per frame in some scenes.
      bool advanced_this_frame = false;
   };
   // The gather's BloomScale (cb4[11].x), per post-process volume and read in a different pass than the uber that
   // needs it, so it travels through GameSettings.BloomScaleLive with a few frames of latency.
   DeferredCBRing bloom_cb_ring;
   float bloom_scale_live = 0.f;
   // Validity is a flag, NOT the sign of the value: BloomScale 0 is a real reading (vanilla bloom off in that volume)
   // and must reach the shader, or the pyramid keeps glowing on the previous zone's gain.
   bool bloom_scale_valid = false;

   bool karis_released = false; // latch for the bloom-off release in OnPresent (see ReleaseCoreKarisAverage)

   // Non-owning view onto core's DrawBloom mip 0 (AddRef'd by DrawBloom; the pyramid itself is core-managed).
   // Rebuilt every frame the feature is on, released every Present.
   ComPtr<ID3D11ShaderResourceView> srv_luma_bloom;
#endif

#if ENABLE_SMAA
   // ---- SMAA (ME1 2007 shape, see RunPostMaterialSMAA) ----
   // Metrics CB (b1) = (1/w, 1/h, w, h) + (predication scale, 0, 0, 0).
   ComPtr<ID3D11Buffer> cb_smaa_metrics;
   float smaa_metrics_pred_scale = -1.f;
   // The one resolution every surface below is sized to, including core's own DrawSMAA intermediates. Not one latch
   // per resource: they all come from the same canvas and cannot legally disagree (see RunPostMaterialSMAA).
   uint32_t scratch_w = 0, scratch_h = 0;
   // SRV-readable snapshot of the canvas; the chain writes the canvas, so it must sample this copy instead.
   ComPtr<ID3D11Texture2D> tex_input;
   ComPtr<ID3D11ShaderResourceView> srv_input;

   // SMAA predication: srv_scene's alpha turned into an edge-ness mask by the depth-extract CS.
   ComPtr<ID3D11Buffer> cb_pred;
   float pred_tolerance = -1.f;
   ComPtr<ID3D11Texture2D> tex_pred;
   ComPtr<ID3D11UnorderedAccessView> uav_pred;
   ComPtr<ID3D11ShaderResourceView> srv_pred;

   void ReleasePredicationScratch()
   {
      uav_pred.reset();
      srv_pred.reset();
      tex_pred.reset();
   }

   // RCAS. The intermediate exists ONLY while sharpening is on: with the slider at 0 the SMAA chain renders
   // straight into the canvas, which saves both this full-resolution copy and a write-back.
   ComPtr<ID3D11Buffer> cb_sharpen;
   float sharpen_amount = -1.f;
   ComPtr<ID3D11Texture2D> tex_smaa_out;
   ComPtr<ID3D11RenderTargetView> tex_smaa_out_rtv;
   ComPtr<ID3D11ShaderResourceView> tex_smaa_out_srv;

   void ReleaseSharpenScratch()
   {
      tex_smaa_out_rtv.reset();
      tex_smaa_out_srv.reset();
      tex_smaa_out.reset();
   }

   // Turning the feature off must give the address space back: ~66 MB at 4K here, plus core's SMAA intermediates.
   // 32-bit process, so the 4 GB ceiling matters even though MassEffect2.exe is large-address-aware (0x0122).
   void ReleaseSMAAScratch()
   {
      srv_input.reset();
      tex_input.reset();
      ReleasePredicationScratch();
      ReleaseSharpenScratch();
   }
#endif
};

class MassEffect2Game final : public Game
{
   static MassEffect2GameDeviceData& GetGameDeviceData(DeviceData& device_data)
   {
      return *static_cast<MassEffect2GameDeviceData*>(device_data.game);
   }

   // Pass identity by shader hash. Both perms of each pass fold into one test: an unreplaced perm would cost the
   // whole Luma tail on that frame (it cost TW2 its HDR block, SMAA and Hide-UI gate at once).
   // Folds both supported dgVoodoo builds, so every hook below keys on either (ME1's ContainsPixelShader shape).
   static bool ContainsPixelShader(const ShaderHashesList<OneShaderPerPipeline>& hashes, uint32_t hash, uint32_t hash_v281)
   {
      return hashes.Contains(hash, reshade::api::shader_stage::pixel) || hashes.Contains(hash_v281, reshade::api::shader_stage::pixel);
   }

   static bool IsUberPass(const ShaderHashesList<OneShaderPerPipeline>& hashes)
   {
      return ContainsPixelShader(hashes, kUberPostFilmicHash, kUberPostFilmicHash_v281) || ContainsPixelShader(hashes, kUberPostHash, kUberPostHash_v281);
   }

   // The BioSceneEffect material: the frame's LAST colour pass (its render target is the present blit's source), so
   // the canvas capture and SMAA both key on it rather than on the uber.
   static bool IsMaterialPass(const ShaderHashesList<OneShaderPerPipeline>& hashes)
   {
      return ContainsPixelShader(hashes, kMaterialHash, kMaterialHash_v281) || ContainsPixelShader(hashes, kMaterialGrainHash, kMaterialGrainHash_v281);
   }

   // The resource behind the currently bound RTV 0, or null. Tests whether a draw targets the same canvas the
   // material wrote. Not DEVELOPMENT-only: Hide UI needs it to ship.
   static ComPtr<ID3D11Resource> GetBoundRenderTargetResource(ID3D11DeviceContext* native_device_context)
   {
      ComPtr<ID3D11Resource> res;
      ComPtr<ID3D11RenderTargetView> rtv;
      native_device_context->OMGetRenderTargets(1, rtv.put(), nullptr);
      if (rtv)
         rtv->GetResource(res.put());
      return res;
   }

#if ENABLE_BLOOM
   // UE3 DOFAndBloomGather, both tap counts. Replaced (it owns the vanilla glow's only off switch) and read for the
   // engine's BloomScale.
   static bool IsDofBloomGather(const ShaderHashesList<OneShaderPerPipeline>& hashes)
   {
      return ContainsPixelShader(hashes, kDofBloomGatherHash, kDofBloomGatherHash_v281) || ContainsPixelShader(hashes, kDofBloomGather4Hash, kDofBloomGather4Hash_v281);
   }
#endif

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

#if ENABLE_SMAA
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

   // Core's DrawSMAA intermediates, ~83 MB at 4K, sized from the RTV handed to them and dropped only on swapchain
   // init, not on this canvas' resize. The SRVs hold their own reference, so release both.
   static void ReleaseCoreSMAAIntermediates(DeviceData& device_data)
   {
      auto& mr = device_data.managed_resources;
      mr.depth_stencil_views[CompileTimeStringHash("smaa_dsv")].reset();
      mr.render_target_views[CompileTimeStringHash("smaa_edge_detection")].reset();
      mr.render_target_views[CompileTimeStringHash("smaa_blending_weight_calculation")].reset();
      mr.shader_resource_views[CompileTimeStringHash("smaa_edge_detection")].reset();
      mr.shader_resource_views[CompileTimeStringHash("smaa_blending_weight_calculation")].reset();
   }
#endif // ENABLE_SMAA

#if ENABLE_BLOOM
   // One cbuffer row from the copy made kSlots frames ago. False when there is nothing to read yet: fresh ring,
   // failed allocation, or a slot still in flight.
   static bool ReadCBRowDeferred(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, ID3D11Buffer* cb,
      MassEffect2GameDeviceData::DeferredCBRing& ring, uint32_t row, float out[4])
   {
      if (cb == nullptr)
         return false;
      D3D11_BUFFER_DESC bd = {};
      cb->GetDesc(&bd);
      if (bd.ByteWidth < (row + 1) * 16)
         return false;

      if (ring.bytes != bd.ByteWidth)
      {
         D3D11_BUFFER_DESC sd = {};
         sd.ByteWidth = bd.ByteWidth;
         sd.Usage = D3D11_USAGE_STAGING;
         sd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
         for (auto& s : ring.staging)
         {
            s.reset();
            if (FAILED(native_device->CreateBuffer(&sd, nullptr, s.put())))
            {
               for (auto& r : ring.staging) // drop a partial allocation rather than run on half a ring
                  r.reset();
               ring.bytes = 0;
               return false;
            }
         }
         ring.bytes = bd.ByteWidth;
         ring.writes = 0;
      }

      if (ring.advanced_this_frame)
         return false; // see DeferredCBRing::advanced_this_frame

      constexpr uint32_t kSlots = MassEffect2GameDeviceData::DeferredCBRing::kSlots;
      native_device_context->CopyResource(ring.staging[ring.writes % kSlots].get(), cb);
      ring.writes++;
      ring.advanced_this_frame = true;
      if (ring.writes < kSlots)
         return false; // nothing old enough to read yet

      // The slot about to be overwritten next is the oldest one, i.e. the copy issued kSlots frames ago.
      ID3D11Buffer* oldest = ring.staging[ring.writes % kSlots].get();
      D3D11_MAPPED_SUBRESOURCE mapped = {};
      if (FAILED(native_device_context->Map(oldest, 0, D3D11_MAP_READ, D3D11_MAP_FLAG_DO_NOT_WAIT, &mapped)) || mapped.pData == nullptr)
         return false; // still in flight; try again next frame rather than blocking
      std::memcpy(out, (const uint8_t*)mapped.pData + (size_t)row * 16, 16);
      native_device_context->Unmap(oldest, 0);
      return true;
   }

   // The engine's BloomScale out of dgVoodoo's b4 mirror at the gather draw, deferred so the Map never stalls. Row and
   // window are inlined because ME2 has no second consumer; implausible data is rejected, keeping the last good value.
   static void TrackBloomScale(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, MassEffect2GameDeviceData& gd)
   {
      ComPtr<ID3D11Buffer> cb;
      native_device_context->PSGetConstantBuffers(4, 1, cb.put());
      float row[4];
      if (!ReadCBRowDeferred(native_device, native_device_context, cb.get(), gd.bloom_cb_ring, 11, row))
         return;
      if (row[0] >= 0.f && row[0] <= 4.f)
      {
         gd.bloom_scale_live = row[0];
         gd.bloom_scale_valid = true; // separate from the value: 0 is a legitimate reading (volume with bloom off)
      }
   }

   // Core drops only DrawKarisAverage's UAV, and only on swapchain init, so a feature-off toggle releases both views
   // itself. ⚠ Karis only: DrawBloom's mip statics have no reachable release, ~66 MB stays (NOTES.md).
   static void ReleaseCoreKarisAverage(DeviceData& device_data)
   {
      auto& mr = device_data.managed_resources;
      mr.unordered_access_views[CompileTimeStringHash("luma_karis_average")].reset();
      mr.shader_resource_views[CompileTimeStringHash("luma_karis_average")].reset();
   }
#endif // ENABLE_BLOOM

#if DEVELOPMENT && ENABLE_SMAA
   // Calibration readback for g_smaa_pred_tolerance: coverage above threshold 0.5 is the geometry fraction, and real
   // silhouettes are ~1% of a frame. Reading the numbers: NOTES.md. One-shot, the Map stalls.
   static void MeasurePredicationMask(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, ID3D11Texture2D* pred)
   {
      // Desc taken off the mask, not rebuilt: CopyResource requires the two to agree, and the walk below has to
      // cover exactly what was copied.
      D3D11_TEXTURE2D_DESC td;
      pred->GetDesc(&td);
      td.Usage = D3D11_USAGE_STAGING;
      td.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
      td.BindFlags = 0;
      td.MiscFlags = 0;
      ComPtr<ID3D11Texture2D> staging;
      if (FAILED(native_device->CreateTexture2D(&td, nullptr, staging.put())))
         return;
      native_device_context->CopyResource(staging.get(), pred);
      D3D11_MAPPED_SUBRESOURCE mapped = {};
      if (FAILED(native_device_context->Map(staging.get(), 0, D3D11_MAP_READ, 0, &mapped)) || mapped.pData == nullptr)
         return;

      constexpr int kBuckets = 1024;
      uint64_t histogram[kBuckets] = {};
      uint64_t total = 0, above_half = 0, nans = 0;
      double sum = 0.0;
      for (uint32_t y = 0; y < td.Height; y++)
      {
         const uint16_t* row = reinterpret_cast<const uint16_t*>(static_cast<const uint8_t*>(mapped.pData) + (size_t)y * mapped.RowPitch);
         for (uint32_t x = 0; x < td.Width; x++)
         {
            const float v = DirectX::PackedVector::XMConvertHalfToFloat(row[x]);
            if (std::isnan(v))
            {
               nans++; // the CS met inf - inf in the scene alpha: counted, kept out of the mean
               continue;
            }
            total++;
            sum += v;
            if (v > 0.5f)
               above_half++;
            histogram[std::clamp((int)(v * kBuckets), 0, kBuckets - 1)]++;
         }
      }
      native_device_context->Unmap(staging.get(), 0);
      if (total == 0)
         return;

      auto percentile = [&](double fraction)
      {
         const uint64_t target = (uint64_t)(fraction * (double)total);
         uint64_t running = 0;
         for (int i = 0; i < kBuckets; i++)
         {
            running += histogram[i];
            if (running >= target)
               return (float)(i + 1) / (float)kBuckets;
         }
         return 1.f;
      };
      char msg[512];
      std::snprintf(msg, sizeof(msg), "[Luma] ME2 DIAG: SMAA predication mask %ux%u tolerance %.4f -> above 0.5 = %.2f%% | mean %.4f | p50 %.4f p90 %.4f p99 %.4f | NaN %llu",
         td.Width, td.Height, g_smaa_pred_tolerance, 100.0 * (double)above_half / (double)total, sum / (double)total, percentile(0.50), percentile(0.90), percentile(0.99), (unsigned long long)nans);
      reshade::log::message(reshade::log::level::info, msg);
   }
#endif // DEVELOPMENT && ENABLE_SMAA

public:
   void OnInit(bool async) override
   {
      // TONEMAP_TYPE 0 (vanilla) is the scaffold default on purpose: it is the reference the HDR work gets compared
      // against, and it keeps a half-finished port from shipping a changed picture.
      std::vector<ShaderDefineData> game_shader_defines_data = {
         {"TONEMAP_TYPE", /*default value*/ '0', true, false, /*tooltip*/ "0 - Vanilla SDR\n1 - Luma HDR (Vanilla+)", /*max value*/ 1},
      };
      shader_defines_data.append_range(game_shader_defines_data);
      assert(shader_defines_data.size() < MAX_SHADER_DEFINES);

#if ENABLE_SMAA
      // The 6 SMAA passes are auto-registered by core from Luma_SMAA_impl. Only the predication CS is ours: it
      // turns the scene buffer's alpha (linear depth) into an R16F edge-ness signal in [0,1].
      native_shaders_definitions.emplace(CompileTimeStringHash("ME2 Depth Extract CS"),
         ShaderDefinition("Luma_ME2_DepthExtract", reshade::api::pipeline_subobject_type::compute_shader));
      // RCAS sharpen PS, drawn via core "Copy VS" + DrawCustomPixelShader after SMAA.
      native_shaders_definitions.emplace(CompileTimeStringHash("ME2 Sharpen PS"),
         ShaderDefinition{"Luma_ME2_Sharpen", reshade::api::pipeline_subobject_type::pixel_shader, nullptr, "sharpen_ps"});
#endif

      // All measured, none assumed. The canvas stores gamma so the game's UE3 Canvas HUD blends onto it exactly as
      // vanilla; core's display composition decodes and applies Game Paper White.
      GetShaderDefineData(POST_PROCESS_SPACE_TYPE_HASH).SetDefaultValue('0');
      GetShaderDefineData(EARLY_DISPLAY_ENCODING_HASH).SetDefaultValue('0');
      // The game encodes with a plain power, [Engine.Client] DisplayGamma=2.2 (ME1 2007 shipped 1.6). Same pair of
      // values the ME1 port ships, and here they match the engine default rather than working around it.
      GetShaderDefineData(VANILLA_ENCODING_TYPE_HASH).SetDefaultValue('1');
      GetShaderDefineData(GAMMA_CORRECTION_TYPE_HASH).SetDefaultValue('1');
      // The HUD draws onto the same canvas after the material, so it needs its own UIPaperWhite and a gamma blend.
      GetShaderDefineData(UI_DRAW_TYPE_HASH).SetDefaultValue('2');

      // Slots 12/13 verified free: a devkit capture of the uber draw shows the game binding only b0..b5, with the devkit
      // and Display Commander loaded alongside.
      luma_settings_cbuffer_index = 13;
      luma_data_cbuffer_index = 12;
      luma_ui_cbuffer_index = -1;

      // Defaults live in ONE place and are copied into the live cbuffer, so the reset buttons and the initial state
      // cannot drift apart. These MUST be set: the cbuffer starts zeroed, and Exposure 0 is a black screen.
      default_luma_global_game_settings.Exposure = 1.f;
      default_luma_global_game_settings.Saturation = 1.f;
      default_luma_global_game_settings.Contrast = 1.f;
      default_luma_global_game_settings.HighlightDechroma = 0.f;     // off; only the mandatory DICE/gamut desaturation applies
      default_luma_global_game_settings.HighlightsHueStrength = 1.f; // full shift toward the RenoDX synthetic reference; safe at 1.0 thanks to the powerless guard in the shader
      default_luma_global_game_settings.HighlightsHueChroma = 0.f;   // keep the highlight's colour; DICE already whitens at the display peak
      default_luma_global_game_settings.Dithering = 1.f;
      default_luma_global_game_settings.VignetteIntensity = 1.f;
      default_luma_global_game_settings.FilmGrainIntensity = 1.f;
      // Movies default ON with a modest boost: Bink bypasses the scene passes, so without this a cutscene sits flat at
      // paper white. Peak is capped at 250 nits in the shader - Bink is low-bitrate and peak amplifies block artifacts.
      default_luma_global_game_settings.VideoAutoHDREnable = 1.f;
      default_luma_global_game_settings.VideoAutoHDRBoost = 0.5f;
      default_luma_global_game_settings.BloomIntensity = 1.f;
      default_luma_global_game_settings.LumaBloomEnable = ENABLE_BLOOM ? 1.f : 0.f;
      // 1.0 is exactly where the game's own bright-pass sits (both gather perms: any channel > 1.0).
      default_luma_global_game_settings.BloomThreshold = 1.f;
      // Until the gather reports. MEASURED on ME2 (DEV log): the engine drives this per zone over a 5x range, so the seed
      // is 1.0, the highest observed, and covers only the frames before the first readback. Readings in NOTES.md.
      default_luma_global_game_settings.BloomScaleLive = 1.f;
      cb_luma_global_settings.GameSettings = default_luma_global_game_settings;
   }

   void LoadConfigs() override
   {
      // cb_luma_global_settings_dirty is already true at init, so these reach the GPU on the first frame.
      auto& gs = cb_luma_global_settings.GameSettings;
      reshade::get_config_value(nullptr, NAME, "Exposure", gs.Exposure);
      reshade::get_config_value(nullptr, NAME, "Contrast", gs.Contrast);
      reshade::get_config_value(nullptr, NAME, "Saturation", gs.Saturation);
      reshade::get_config_value(nullptr, NAME, "HighlightsDesaturation", gs.HighlightDechroma);
      reshade::get_config_value(nullptr, NAME, "HighlightsHueStrength", gs.HighlightsHueStrength);
      reshade::get_config_value(nullptr, NAME, "HighlightsHueChroma", gs.HighlightsHueChroma);
      reshade::get_config_value(nullptr, NAME, "Dithering", gs.Dithering);
      reshade::get_config_value(nullptr, NAME, "VignetteIntensity", gs.VignetteIntensity);
      reshade::get_config_value(nullptr, NAME, "FilmGrainIntensity", gs.FilmGrainIntensity);
      reshade::get_config_value(nullptr, NAME, "VideoAutoHDREnable", gs.VideoAutoHDREnable);
      reshade::get_config_value(nullptr, NAME, "VideoAutoHDRBoost", gs.VideoAutoHDRBoost);

#if ENABLE_BLOOM
      reshade::get_config_value(nullptr, NAME, "BloomIntensity", gs.BloomIntensity);
      reshade::get_config_value(nullptr, NAME, "BloomThreshold", gs.BloomThreshold);
      reshade::get_config_value(nullptr, NAME, "LumaBloomEnable", g_luma_bloom_enable);
      gs.LumaBloomEnable = g_luma_bloom_enable ? 1.f : 0.f; // mirror to both consumers (the uber and the gather)
#endif

#if ENABLE_SMAA
      // Plain globals rather than cbuffer fields: these drive the mod's own injected passes, not a shader setting.
      reshade::get_config_value(nullptr, NAME, "SMAAEnable", g_smaa_enable);
      reshade::get_config_value(nullptr, NAME, "SMAAPredication", g_smaa_predication);
      reshade::get_config_value(nullptr, NAME, "SMAAPredicationTolerance", g_smaa_pred_tolerance);
      reshade::get_config_value(nullptr, NAME, "RCASSharpness", g_rcas_sharpness);
#endif
   }

   void DrawImGuiSettings(DeviceData& device_data) override
   {
      // Everything here is read through LumaSettings.GameSettings in Luma_ME2_Tonemap.hlsl and
      // Video_0xE41621CF.ps_5_0.hlsl. Except Dithering and the two native-look controls, all of it is HDR-path only.
      auto& gs = cb_luma_global_settings.GameSettings;
      auto& dirty = device_data.cb_luma_global_settings_dirty;

      // One settings widget: draw, persist, mark the cbuffer dirty, tooltip, reset. The repetition is where a forgotten
      // dirty flag hides. AllowWhenDisabled so tooltips still work inside a BeginDisabled block.
      auto Slider = [&](const char* label, float& value, float default_value, const char* key, float min_value, float max_value, const char* tooltip, const char* format = "%.3f")
      {
         if (ImGui::SliderFloat(label, &value, min_value, max_value, format))
         {
            reshade::set_config_value(nullptr, NAME, key, value);
            dirty = true;
         }
         if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip(tooltip);
         if (DrawResetButton(value, default_value, key)) // writes the config itself (Serialize defaults true)
            dirty = true;
      };
      // Same, for a 0/1 float the shaders read as a boolean. Returns the state so a dependent control can grey out.
      auto Toggle = [&](const char* label, float& value, const char* key, const char* tooltip)
      {
         bool on = value > 0.5f;
         if (ImGui::Checkbox(label, &on))
         {
            value = on ? 1.f : 0.f;
            reshade::set_config_value(nullptr, NAME, key, value);
            dirty = true;
         }
         if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip(tooltip);
         return on;
      };

#if ENABLE_SMAA
      // Nothing in this section touches cb_luma_global_settings, so none of it sets the dirty flag: these are the
      // mod's own injected passes, driven by plain globals.
      ImGui::SeparatorText("Anti-Aliasing");
      if (ImGui::Checkbox("SMAA Enable", &g_smaa_enable))
         reshade::set_config_value(nullptr, NAME, "SMAAEnable", g_smaa_enable);
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Adds SMAA anti-aliasing (the game has none of its own).");
      if (g_smaa_enable)
      {
#if DEVELOPMENT
         // Predication is not a preference: it only relaxes the edge threshold back to base ULTRA on geometry and
         // never below, so off is strictly worse. Kept as a bisect switch for devs, shipped on and out of sight.
         if (ImGui::Checkbox("SMAA Predication", &g_smaa_predication))
            reshade::set_config_value(nullptr, NAME, "SMAAPredication", g_smaa_predication);
         if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Finds edges by geometry (scene depth) instead of by brightness alone.\nKeeps textures sharp while still antialiasing real silhouettes.");
         if (ImGui::SliderFloat("SMAA Predication Tolerance", &g_smaa_pred_tolerance, 0.002f, 0.2f, "%.3f", ImGuiSliderFlags_Logarithmic))
            reshade::set_config_value(nullptr, NAME, "SMAAPredicationTolerance", g_smaa_pred_tolerance);
         if (ImGui::IsItemHovered())
            ImGui::SetTooltip("How far a surface may deviate from its local plane before it counts as an edge,\nas a fraction of view depth. Lower = more edges. This is the calibration lever,\nnot the SMAA threshold. Logarithmic: the parameter is relative.");
         ImGui::Checkbox("SMAA Predication Debug View", &g_smaa_pred_debug);
         if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Show the predication mask (red) instead of the frame.\nWant: black on flat surfaces, red across silhouettes.\nAll red = tolerance too low (predication is doing nothing).\nAll black = too high (silhouettes never regain sensitivity).");
         if (ImGui::Button("Measure Predication Mask"))
            g_smaa_pred_measure = true;
         if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Log the mask's coverage above 0.5 plus percentiles to ReShade.log.\nReal silhouettes are ~1% of a typical frame; a working mask barely moves across a 20x tolerance sweep.\nAn all-zero mask means the scene alpha is not carrying depth.\nStalls the GPU for one frame.");
#endif

         if (ImGui::SliderFloat("RCAS Sharpness", &g_rcas_sharpness, 0.f, 1.f))
            reshade::set_config_value(nullptr, NAME, "RCASSharpness", g_rcas_sharpness);
         if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Sharpening applied on top of SMAA (0 = off).");
         DrawResetButton(g_rcas_sharpness, 0.f, "RCASSharpness"); // writes the config itself (Serialize defaults true)
      }
#endif // ENABLE_SMAA

      ImGui::SeparatorText("Grade");

      // Applied scene-referred, before the grade - which is implementation, so it stays out of the tooltip.
      Slider("Exposure", gs.Exposure, default_luma_global_game_settings.Exposure, "Exposure", 0.f, 2.f, "Overall image brightness (1 = vanilla).");
      Slider("Contrast", gs.Contrast, default_luma_global_game_settings.Contrast, "Contrast", 0.f, 2.f, "Overall image contrast, HDR only (1 = vanilla).");
      Slider("Saturation", gs.Saturation, default_luma_global_game_settings.Saturation, "Saturation", 0.f, 2.f, "Color saturation, HDR only (1 = vanilla).");
      Slider("Highlights Desaturation", gs.HighlightDechroma, default_luma_global_game_settings.HighlightDechroma, "HighlightsDesaturation", 0.f, 1.f,
         "How soon bright sources fade to neutral white, HDR only (0 = keep color at any brightness).");

      Slider("Highlights Hue Shift", gs.HighlightsHueStrength, default_luma_global_game_settings.HighlightsHueStrength, "HighlightsHueStrength", 0.f, 1.f,
         "Turns blown highlights toward the hue an SDR limiter gives them (fire reads yellow, as in the RenoDX ports).\n1 = full, 0 = the light's real hue. HDR only.", "%.2f");
      Slider("Highlights Whitening", gs.HighlightsHueChroma, default_luma_global_game_settings.HighlightsHueChroma, "HighlightsHueChroma", 0.f, 1.f,
         "Desaturates blown highlights toward the same SDR limiter's saturation.\n0 = keep colour, 1 = full. HDR only.", "%.2f");

#if ENABLE_BLOOM
      ImGui::SeparatorText("Bloom");
      if (ImGui::Checkbox("Luma Bloom Enable", &g_luma_bloom_enable))
      {
         reshade::set_config_value(nullptr, NAME, "LumaBloomEnable", g_luma_bloom_enable);
         gs.LumaBloomEnable = g_luma_bloom_enable ? 1.f : 0.f; // mirrored: the uber and the gather both read it
         dirty = true;
      }
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Replaces the game's bloom with a wider, softer HDR bloom.");

      // Everything below drives the Luma pyramid and nothing else, so it all greys out with it: no slider here can
      // reach the game's own glow, which shares a buffer with the DoF blur and only has an off switch.
      ImGui::BeginDisabled(!g_luma_bloom_enable);
      Slider("Bloom Intensity", gs.BloomIntensity, default_luma_global_game_settings.BloomIntensity, "BloomIntensity", 0.f, 2.f, "Bloom strength (1 = vanilla, 0 = none).");
#if DEVELOPMENT
      Slider("Bloom Threshold", gs.BloomThreshold, default_luma_global_game_settings.BloomThreshold, "BloomThreshold", 0.f, 4.f,
         "Linear scene brightness where bloom starts. 1.0 is where the game's own bright-pass sits.\nNear 0 the whole scene glows - that is the failure mode, not a setting.", "%.2f");
      // The engine gain the intensity slider multiplies. Read-only: it comes from the gather, not from the user.
      ImGui::Text("Engine BloomScale: %.5f", gs.BloomScaleLive);
#endif // DEVELOPMENT
      ImGui::EndDisabled();
#endif // ENABLE_BLOOM

      ImGui::SeparatorText("Effects");

      // Row order is the house one (docs/UI-Toggle-Standard.md; MELE is the exact twin). Both sliders scale only the
      // effect, never the vignette's blue-tinted white point, which is part of the vanilla grade.
      Slider("Vignette Intensity", gs.VignetteIntensity, default_luma_global_game_settings.VignetteIntensity, "VignetteIntensity", 0.f, 1.f,
         "Scales the game's vignette darkening (1 = vanilla, 0 = none).");
      Slider("Film Grain Intensity", gs.FilmGrainIntensity, default_luma_global_game_settings.FilmGrainIntensity, "FilmGrainIntensity", 0.f, 1.f,
         "Scales the game's film grain (1 = vanilla, 0 = off).");

      // Gated on TONEMAP_TYPE in the shader, not on display mode (NOTES.md). The config key stays "VideoAutoHDRBoost"
      // under the canon "Video HDR Boost" label, so renaming the row does not reset the setting for existing users.
      const bool video_auto_hdr = Toggle("Video AutoHDR", gs.VideoAutoHDREnable, "VideoAutoHDREnable",
         "Adds HDR highlights to pre-rendered videos (HDR only).");
      ImGui::BeginDisabled(!video_auto_hdr);
      Slider("Video HDR Boost", gs.VideoAutoHDRBoost, default_luma_global_game_settings.VideoAutoHDRBoost, "VideoAutoHDRBoost", 0.f, 1.f,
         "Video highlight strength (0 = off).");
      ImGui::EndDisabled();

      // No "(HDR output)" qualifier: this dither is gated on TONEMAP_TYPE, not on the display mode, so it runs in
      // SDR too whenever that mode is on (as in the sibling ME1 2007 and Witcher 2 ports).
      Toggle("Dithering", gs.Dithering, "Dithering", "Reduces gradient banding.");

      ImGui::SeparatorText("UI");
      ImGui::Checkbox("Hide Gameplay UI", &g_hide_ui); // Session-only: a stuck "on" would look like a broken HUD.
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Disables the in-game UI.");
   }

   void OnCreateDevice(ID3D11Device* native_device, DeviceData& device_data) override
   {
      device_data.game = new MassEffect2GameDeviceData;

      // Device scope on purpose: core only reads this flag, and scenes that skip the post chain (loading, movies, some
      // UI) must still count or composition treats the frame as unfinished. ME2's gamma-only path never draws.
      device_data.has_drawn_main_post_processing = true;
   }

   // Mandatory once this data owns ComPtrs: GameDeviceData has no virtual destructor, so core's delete through the
   // base pointer never runs ours - a 4K fp16 texture would leak on every dgVoodoo device teardown.
   void OnDestroyDeviceData(DeviceData& device_data) override
   {
      delete static_cast<MassEffect2GameDeviceData*>(device_data.game);
      device_data.game = nullptr;
   }

#if ENABLE_SMAA
   // SMAA on the graded gamma canvas, after the material and before the UE3 Canvas HUD draws onto it.
   // ME1/TW2/BL2 chain: snapshot -> SRV -> DrawSMAA, last pass into the canvas RTV. No-op if incomplete.
   void RunPostMaterialSMAA(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, DeviceData& device_data, MassEffect2GameDeviceData& gd, ID3D11Resource* canvas_res, ID3D11RenderTargetView* canvas_rtv)
   {
      uint4 cinfo{};
      DXGI_FORMAT cfmt = DXGI_FORMAT_UNKNOWN;
      GetResourceInfo(canvas_res, cinfo, cfmt);
      const uint32_t w = cinfo.x, h = cinfo.y;
      if (w == 0 || h == 0 || cfmt == DXGI_FORMAT_UNKNOWN)
         return;
      // Scratch takes the canvas' TYPED format: a typeless canvas (upgrades off) makes CreateShaderResourceView(nullptr)
      // fail and SMAA vanish silently. Same typeless family either way, so CopyResource stays legal.
      const DXGI_FORMAT scratch_fmt = (DXGI_FORMAT)reshade::api::format_to_default_typed((reshade::api::format)cfmt);

      // Shader-readiness gate (async loader / dev live-reload): skip SMAA this frame if anything is missing.
      const bool smaa_ready =
         AllShadersReady(device_data.native_pixel_shaders, {CompileTimeStringHash("SMAA Edge Detection PS"), CompileTimeStringHash("SMAA Blending Weight Calculation PS"), CompileTimeStringHash("SMAA Neighborhood Blending PS")}) && AllShadersReady(device_data.native_vertex_shaders, {CompileTimeStringHash("SMAA Edge Detection VS"), CompileTimeStringHash("SMAA Blending Weight Calculation VS"), CompileTimeStringHash("SMAA Neighborhood Blending VS")});
      if (!smaa_ready)
         return;

      // ONE resolution latch: every surface here is canvas-sized, so on a change drop the lot and let the per-pointer
      // checks rebuild. Core's SMAA intermediates included - core resets them only on swapchain init (MoHA).
      if (gd.scratch_w != w || gd.scratch_h != h)
      {
         gd.ReleaseSMAAScratch();
         ReleaseCoreSMAAIntermediates(device_data);
         gd.cb_smaa_metrics.reset(); // holds the resolution itself
         gd.cb_sharpen.reset();
         gd.scratch_w = w;
         gd.scratch_h = h;
      }

      // Edge-ness from the scene alpha (linear depth). Scale and mask fall back together: 2.0 with a null mask would
      // raise the threshold frame-wide. The CS maps texels 1:1, hence the size check.
      auto* pred_cs = FindShader(device_data.native_compute_shaders, CompileTimeStringHash("ME2 Depth Extract CS"));
      bool pred_ok = g_smaa_predication && gd.srv_scene.get() != nullptr && pred_cs != nullptr;
      if (pred_ok)
      {
         uint4 sinfo{};
         DXGI_FORMAT sfmt = DXGI_FORMAT_UNKNOWN;
         GetResourceInfo(gd.srv_scene.get(), sinfo, sfmt);
         pred_ok = sinfo.x == w && sinfo.y == h;
      }
      if (pred_ok)
      {
         if (!gd.cb_pred || gd.pred_tolerance != g_smaa_pred_tolerance)
         {
            const float p[4] = {g_smaa_pred_tolerance, 0.f, 0.f, 0.f};
            if (CreateImmutableCB(native_device, p, sizeof(p), gd.cb_pred))
               gd.pred_tolerance = g_smaa_pred_tolerance;
         }
         // Not a clone of the canvas (single-channel + UAV), so this one is built from scratch.
         if (!gd.tex_pred)
         {
            const CD3D11_TEXTURE2D_DESC td(DXGI_FORMAT_R16_FLOAT, w, h, 1, 1, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS);
            if (SUCCEEDED(native_device->CreateTexture2D(&td, nullptr, gd.tex_pred.put())))
            {
               native_device->CreateUnorderedAccessView(gd.tex_pred.get(), nullptr, gd.uav_pred.put());
               native_device->CreateShaderResourceView(gd.tex_pred.get(), nullptr, gd.srv_pred.put());
            }
         }
         pred_ok = gd.cb_pred && gd.uav_pred && gd.srv_pred;
      }

      const float pred_scale = pred_ok ? 2.f : 1.f;
      if (!gd.cb_smaa_metrics || gd.smaa_metrics_pred_scale != pred_scale)
      {
         const float metrics[8] = {1.f / (float)w, 1.f / (float)h, (float)w, (float)h, pred_scale, 0.f, 0.f, 0.f};
         if (CreateImmutableCB(native_device, metrics, sizeof(metrics), gd.cb_smaa_metrics))
            gd.smaa_metrics_pred_scale = pred_scale;
      }
      if (!gd.cb_smaa_metrics)
         return;

      // RCAS decides the chain's SHAPE, so resolve it before allocating: with sharpening off the last SMAA pass writes
      // the canvas directly. Core's fullscreen "Copy VS" is shared by RCAS and the predication debug view.
      auto* copy_vs = FindShader(device_data.native_vertex_shaders, CompileTimeStringHash("Copy VS"));
      auto* sharpen_ps = FindShader(device_data.native_pixel_shaders, CompileTimeStringHash("ME2 Sharpen PS"));
      bool do_sharpen = g_rcas_sharpness > 0.f && copy_vs != nullptr && sharpen_ps != nullptr;
      if (do_sharpen)
      {
         if (!gd.cb_sharpen || gd.sharpen_amount != g_rcas_sharpness)
         {
            const float sp[4] = {(float)w, (float)h, g_rcas_sharpness, 0.f};
            if (CreateImmutableCB(native_device, sp, sizeof(sp), gd.cb_sharpen))
               gd.sharpen_amount = g_rcas_sharpness;
         }
         if (!gd.tex_smaa_out)
         {
            // A clone of the canvas keeping its render-target bind: SMAA writes here, RCAS reads it back. .get() because
            // CloneTexture returns ReShade's com_ptr while our members are core's ComPtr, which AddRefs on assignment.
            gd.tex_smaa_out = CloneTexture<ID3D11Texture2D>(native_device, canvas_res, scratch_fmt,
               D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET, 0, false, false)
                                 .get();
            if (gd.tex_smaa_out)
            {
               native_device->CreateRenderTargetView(gd.tex_smaa_out.get(), nullptr, gd.tex_smaa_out_rtv.put());
               native_device->CreateShaderResourceView(gd.tex_smaa_out.get(), nullptr, gd.tex_smaa_out_srv.put());
            }
         }
         if (!gd.cb_sharpen || !gd.tex_smaa_out_rtv || !gd.tex_smaa_out_srv)
            do_sharpen = false; // allocation failed: fall back to the un-sharpened chain rather than dropping SMAA
      }

      if (!gd.tex_input)
      {
         // A clone of the canvas with the render-target bind swapped for a shader-resource one, since the chain writes the
         // canvas itself. Refreshed by the CopyResource below every frame, hence no initial copy.
         gd.tex_input = CloneTexture<ID3D11Texture2D>(native_device, canvas_res, scratch_fmt,
            D3D11_BIND_SHADER_RESOURCE, D3D11_BIND_RENDER_TARGET, false, false)
                           .get();
         if (gd.tex_input)
            native_device->CreateShaderResourceView(gd.tex_input.get(), nullptr, gd.srv_input.put());
      }
      if (!gd.srv_input)
         return;

      native_device_context->CopyResource(gd.tex_input.get(), canvas_res);

      // Scene alpha (linear depth) -> plane-deviation edge-ness in R16F; see Luma_ME2_DepthExtract.hlsl for why
      // this is an edge test rather than a depth rescale.
      if (pred_ok)
      {
         DrawStateStack<DrawStateStackType::Compute> pred_cs_state;
         pred_cs_state.Cache(native_device_context, device_data.uav_max_count);

         ID3D11ShaderResourceView* ps_srv = gd.srv_scene.get();
         ID3D11UnorderedAccessView* ps_uav = gd.uav_pred.get();
         ID3D11Buffer* ps_cb = gd.cb_pred.get();
         native_device_context->CSSetShaderResources(0, 1, &ps_srv);
         native_device_context->CSSetUnorderedAccessViews(0, 1, &ps_uav, nullptr);
         native_device_context->CSSetConstantBuffers(0, 1, &ps_cb);
         native_device_context->CSSetShader(pred_cs, nullptr, 0);
         native_device_context->Dispatch((w + 7) / 8, (h + 7) / 8, 1);

         pred_cs_state.Restore(native_device_context);
      }

#if DEVELOPMENT
      // Calibration aids, driven from the Anti-Aliasing section. Both read the mask that was just written.
      if (g_smaa_pred_measure)
      {
         g_smaa_pred_measure = false;
         if (pred_ok)
            MeasurePredicationMask(native_device, native_device_context, gd.tex_pred.get());
         else
            reshade::log::message(reshade::log::level::warning, "[Luma] ME2 DIAG: SMAA predication mask not measured: predication inactive this frame (no scene capture, size mismatch or CS missing)");
      }
      if (pred_ok && g_smaa_pred_debug)
      {
         auto* copy_ps = FindShader(device_data.native_pixel_shaders, CompileTimeStringHash("Copy PS"));
         if (copy_vs != nullptr && copy_ps != nullptr)
         {
            // The mask is single-channel, so the core copy lands it in RED — unmistakably a debug view. Replaces
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
            copy_vs, sharpen_ps, gd.tex_smaa_out_srv.get(), canvas_rtv, w, h, false);

         sharpen_state.Restore(native_device_context);
      }

      ID3D11Buffer* vcb = vs_cb1_orig.get();
      ID3D11Buffer* pcb = ps_cb1_orig.get();
      native_device_context->VSSetConstantBuffers(1, 1, &vcb);
      native_device_context->PSSetConstantBuffers(1, 1, &pcb);
   }
#endif // ENABLE_SMAA

   DrawOrDispatchOverrideType OnDrawOrDispatch(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, CommandListData& cmd_list_data, DeviceData& device_data, reshade::api::shader_stage stages, const ShaderHashesList<OneShaderPerPipeline>& original_shader_hashes, bool is_custom_pass, bool& updated_cbuffers, std::function<void()>* original_draw_dispatch_func) override
   {
      auto& game_device_data = GetGameDeviceData(device_data);
      // Deferred contexts replay later, so a capture or an injected pass issued from one would land out of order
      // (BL GOTY, MEA and MoHA gate every hook the same way). Cached by core; avoids a per-draw virtual query.
      const bool is_immediate = cmd_list_data.is_primary;

#if DEVELOPMENT
      // Before any hook, since the material block can return early. Gated on the set's size, and on is_immediate because
      // `emplace` rehashes with no core mutex here. No !is_custom_pass clause - most of this list is hash-replaced.
      if (is_immediate && game_device_data.logged_passes.size() < std::size(kLoggedPassHashes))
         for (uint32_t hash : kLoggedPassHashes)
         {
            if (original_shader_hashes.Contains(hash, reshade::api::shader_stage::pixel) && game_device_data.logged_passes.emplace(hash).second)
            {
               char msg[128];
               std::snprintf(msg, sizeof(msg), "[Luma] ME2 DIAG: pass 0x%08X drew for the first time this session", hash);
               reshade::log::message(reshade::log::level::info, msg);
            }
         }
#endif

      // Hide HUD: cancel draws after the material that target the same canvas - the render-target test is load-bearing,
      // since "everything after" also swallows the present blit. The custom-pass clause rejects only Luma's own passes.
      if (g_hide_ui && is_immediate && (!is_custom_pass || original_shader_hashes.pixel_shaders[0] != UINT64_MAX) && game_device_data.has_drawn_material && game_device_data.canvas_res)
      {
         if (GetBoundRenderTargetResource(native_device_context).get() == game_device_data.canvas_res.get())
            return DrawOrDispatchOverrideType::Replaced;
      }

#if ENABLE_BLOOM
      // The gather runs before the uber: read its BloomScale here (deferred, no stall). Bloom-only work, so it
      // follows the feature's own switch - GameSettings.BloomScaleLive is read by nothing else.
      if (g_luma_bloom_enable && is_immediate && IsDofBloomGather(original_shader_hashes))
         TrackBloomScale(native_device, native_device_context, game_device_data);
#endif

      if (is_immediate && !game_device_data.has_drawn_uber && IsUberPass(original_shader_hashes))
      {
         game_device_data.has_drawn_uber = true;
         game_device_data.ever_matched_keyed_pass = true;

         // Push LumaSettings at the seam, not inside a feature block: core uploads only after this callback returns, so
         // every b13 consumer below would read last frame's buffer. Above the state stack, and "updated_cbuffers" is left alone.
         SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, reshade::api::shader_stage::pixel, LumaConstantBufferType::LumaSettings);

#if ENABLE_SMAA || ENABLE_BLOOM
         // t0 is the game's fp16 scene: bloom source, and its ALPHA is the linear depth SMAA predication reads. Nothing
         // overwrites it before the material draws. Read before any early-out - is_custom_pass covers replaced passes.
         game_device_data.srv_scene.reset();
         native_device_context->PSGetShaderResources(0, 1, game_device_data.srv_scene.put());
#endif

#if ENABLE_BLOOM
         // Luma bloom pyramid off the fp16 LINEAR scene at t0, pre-glow by construction (the halo is added later,
         // in the uber's own composite). Karis average first: no TAA, so fireflies have to die spatially.
         game_device_data.srv_luma_bloom.reset(); // DrawBloom AddRef's its mip 0 into this
         // Readiness gate: both core helpers reach their shaders with .at(), which THROWS while the async loader
         // or a dev live-reload has not compiled them yet.
         const bool bloom_ready =
            AllShadersReady(device_data.native_pixel_shaders, {CompileTimeStringHash("Bloom Prefilter PS"), CompileTimeStringHash("Bloom Downsample PS"), CompileTimeStringHash("Bloom Upsample PS")}) && AllShadersReady(device_data.native_vertex_shaders, {CompileTimeStringHash("Bloom VS")}) && AllShadersReady(device_data.native_compute_shaders, {CompileTimeStringHash("Karis Average CS")});
         if (g_luma_bloom_enable && bloom_ready && game_device_data.srv_scene)
         {
            DrawStateStack<DrawStateStackType::FullGraphics> bloom_state;
            bloom_state.Cache(native_device_context, device_data.uav_max_count);

            ComPtr<ID3D11ShaderResourceView> srv_karis;
            DrawKarisAverage(native_device, native_device_context, device_data, game_device_data.srv_scene.get(), srv_karis.put());
            // The sigmas are in mip texels, so nothing here is resolution-dependent.
            if (srv_karis)
               DrawBloom(native_device, native_device_context, device_data, srv_karis.get(), (int)std::size(g_bloom_sigmas), g_bloom_sigmas, game_device_data.srv_luma_bloom.put());

            bloom_state.Restore(native_device_context);
         }
         {
            // Bound every frame, null included: the composite is gated on LumaBloomEnable, not on the slot, and
            // dgVoodoo's 1x1 placeholder would otherwise be sampled as garbage. The composite ADDS this.
            ID3D11ShaderResourceView* bloom_srv = game_device_data.srv_luma_bloom.get();
            native_device_context->PSSetShaderResources(kLumaBloomSlot, 1, &bloom_srv);
         }
#endif
      }
      // The material is the last scene pass, so it - not the uber - is what "main post processing ran" means here.
      if (is_immediate && !game_device_data.has_drawn_material && IsMaterialPass(original_shader_hashes))
      {
         game_device_data.has_drawn_material = true;
         game_device_data.ever_matched_keyed_pass = true;

         // The canvas SMAA antialiases and the present blit's source. Captured every frame because an upgrade or resize can
         // swap the mirror, and kept in device data because Hide UI needs the identity on later draws.
         ComPtr<ID3D11RenderTargetView> canvas_rtv;
         native_device_context->OMGetRenderTargets(1, canvas_rtv.put(), nullptr);
         game_device_data.canvas_res.reset();
         if (canvas_rtv)
            canvas_rtv->GetResource(game_device_data.canvas_res.put());

#if ENABLE_SMAA
         // Run the pass ourselves, then SMAA on the canvas, so the antialiasing lands before the HUD. Falls back to
         // a plain draw when the callback is unavailable (one frame without AA) rather than skipping the encode.
         if (g_smaa_enable && original_draw_dispatch_func != nullptr && canvas_rtv && game_device_data.canvas_res)
         {
            // Returning Replaced skips core's cbuffer upload for this draw, and this replacement reads LumaSettings.
            // Above any state stack, since Restore() rolls PS constant buffers back wholesale.
            SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, reshade::api::shader_stage::pixel, LumaConstantBufferType::LumaSettings);

            (*original_draw_dispatch_func)();
            RunPostMaterialSMAA(native_device, native_device_context, device_data, game_device_data, game_device_data.canvas_res.get(), canvas_rtv.get());
            return DrawOrDispatchOverrideType::Replaced; // we ran the original draw ourselves
         }
#endif
      }

      return DrawOrDispatchOverrideType::None;
   }

   void OnPresent(ID3D11Device* native_device, DeviceData& device_data) override
   {
      auto& game_device_data = GetGameDeviceData(device_data);

      // One-shot wrong-wrapper warning (ME1/BL GOTY precedent). Deliberately generous: unlike ME1, whose keyed pass
      // also runs in the menu, ME2's uber and material do not draw during the Bink boot logos - measured ~15 s
      // between the movie pass and the first material draw - so a short budget would cry wolf on a slow intro.
      constexpr uint32_t kBuildCheckFrames = 3600;
      if (!game_device_data.build_check_done && ++game_device_data.frames_presented >= kBuildCheckFrames)
      {
         game_device_data.build_check_done = true;
         if (!game_device_data.ever_matched_keyed_pass)
            reshade::log::message(reshade::log::level::warning,
               "[Luma] ME2: no keyed scene pass seen after warmup -- the dgVoodoo build is probably neither 2.87.3 nor 2.81.3, so every shader replacement is inactive (re-dump the shaders for it).");
      }

      game_device_data.has_drawn_uber = false;
      game_device_data.has_drawn_material = false;
      game_device_data.canvas_res.reset(); // do not hold a reference across frames: it would outlive a resize or a mirror swap

#if ENABLE_SMAA || ENABLE_BLOOM
      // Recaptured every frame at the uber pass; never held across one (a resize or a mirror swap would outlive it).
      game_device_data.srv_scene.reset();
#endif

#if ENABLE_BLOOM
      game_device_data.srv_luma_bloom.reset(); // same rule: core owns the pyramid, we only borrow mip 0 per frame
      game_device_data.bloom_cb_ring.advanced_this_frame = false;

      // The gather's BloomScale, the gain that makes Bloom Intensity 1 vanilla strength.
      if (game_device_data.bloom_scale_valid && std::abs(cb_luma_global_settings.GameSettings.BloomScaleLive - game_device_data.bloom_scale_live) > 1e-5f)
      {
         cb_luma_global_settings.GameSettings.BloomScaleLive = game_device_data.bloom_scale_live;
         device_data.cb_luma_global_settings_dirty = true;
#if DEVELOPMENT
         // One line per change, so the seeded default can be replaced with a measured value (and so a zone with
         // vanilla bloom off is visible as a real 0 rather than read as a broken readback).
         char msg[128];
         std::snprintf(msg, sizeof(msg), "[Luma] ME2 DIAG: engine BloomScale now %.5f", game_device_data.bloom_scale_live);
         reshade::log::message(reshade::log::level::info, msg);
#endif
      }

      // Give the address space back when the pyramid is off, on the render thread. Its own block because either feature
      // can be compiled out independently, and latched so the two map lookups do not run every frame.
      if (!g_luma_bloom_enable)
      {
         if (!game_device_data.karis_released)
         {
            ReleaseCoreKarisAverage(device_data);
            game_device_data.karis_released = true;
         }
      }
      else
      {
         game_device_data.karis_released = false; // DrawKarisAverage recreates lazily
      }
#endif

#if ENABLE_SMAA
      // Give the address space back when a feature is off. Done here rather than in the ImGui handler so the
      // release happens on the render thread, never while a frame is mid-flight.
      if (!g_smaa_enable && game_device_data.tex_input)
      {
         game_device_data.ReleaseSMAAScratch();
         ReleaseCoreSMAAIntermediates(device_data);
         game_device_data.scratch_w = game_device_data.scratch_h = 0; // core recreates lazily; keep our latch from claiming they are current
      }
      else
      {
         if (!g_smaa_predication && game_device_data.tex_pred)
            game_device_data.ReleasePredicationScratch();
         if (g_rcas_sharpness <= 0.f && game_device_data.tex_smaa_out)
            game_device_data.ReleaseSharpenScratch();
      }
#endif
   }

   void PrintImGuiAbout() override
   {
      // Same shape as the sibling dgVoodoo ports, byte-identical but for the game name. ShellExecuteA rather than
      // system("start"), which hangs the render thread in exclusive fullscreen.
      ImGui::PushTextWrapPos(0.f);
      ImGui::Text(
         "Luma for \"Mass Effect 2\" (2010) is developed by DristoforColumb and is open source and free.\n"
         "It adds native HDR, HDR bloom, SMAA anti-aliasing, and 16x anisotropic filtering.\n"
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
         ShellExecuteA(nullptr, "open", "https://ko-fi.com/dristoforcolumb", nullptr, nullptr, SW_SHOWNORMAL);
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
         ShellExecuteA(nullptr, "open", "https://github.com/Filoppi/Luma-Framework", nullptr, nullptr, SW_SHOWNORMAL);

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
                  "\nDICE (HDR tonemapper)"
                  "\nOklab (hue/chroma restoration)"
                  "\nSMAA (Iryoku)"
                  "\nAMD FidelityFX (RCAS)"
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
      // The website MUST be "" and never nullptr: SetGlobals strncpy's it, and a null AVs in ucrtbase during
      // DllMain, which ReShade reports as addon load error 1114.
      Globals::SetGlobals(cleared_project_name, "Mass Effect 2 (2010) Luma mod", "", mod_version);

      swapchain_format_upgrade_type = TextureFormatUpgradesType::AllowedEnabled;
      swapchain_upgrade_type = SwapchainUpgradeType::scRGB;
      // The in-game brightness slider is a D3D9 SetGammaRamp that dgVoodoo forwards to the OS device ramp, which
      // would distort the scRGB output. This only exposes core's "Reset Gamma Ramp" button.
      allow_disabling_gamma_ramp = true;

      // Two clipping families, upgraded INDIRECTLY because changing a dgVoodoo resource's CREATION format black-screens
      // the game. Keyed by format, not hash; the size filters are load-bearing in a 32-bit process. Details: NOTES.md.
      texture_format_upgrades_type = TextureFormatUpgradesType::AllowedEnabled;
      enable_indirect_texture_format_upgrades = true;
      enable_chain_indirect_texture_format_upgrades = ChainTextureFormatUpgradesType::DirectDependencies;
      texture_upgrade_formats = {
         reshade::api::format::r8g8b8a8_typeless,     // the two 4K canvases: uber target and material target
         reshade::api::format::r16g16b16a16_typeless, // DoF/bloom gather, blur and blit targets, viewed as unorm
      };
      // "No1Px" is mandatory under dgVoodoo: the wrapper binds 1x1 placeholders in every unused sampler slot (all
      // 32 of them in ME2's captures) and a 1x1 trivially passes the aspect filter, so core would mirror those too.
      texture_format_upgrades_2d_size_filters = (uint32_t)TextureFormatUpgrades2DSizeFilters::SwapchainResolution | (uint32_t)TextureFormatUpgrades2DSizeFilters::SwapchainAspectRatio | (uint32_t)TextureFormatUpgrades2DSizeFilters::No1Px;

      // Force AF over the shipped MaxAnisotropy=4. "force_upgrade_linear_samplers" is the load-bearing line: mode 4 alone
      // is a no-op under this wrapper, which binds no ANISOTROPIC samplers. LOD bias stays 0, no temporal AA here.
      enable_samplers_upgrade = true;
      samplers_upgrade_mode = 4;
      force_upgrade_linear_samplers = true;

      game = new MassEffect2Game();
   }

   CoreMain(hModule, ul_reason_for_call, lpReserved);

   return TRUE;
}
