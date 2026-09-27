// Borderlands GOTY Enhanced — Luma HDR + SMAA mod (Unreal Engine 3.5, D3D11).
// - HDR: swapchain -> scRGB fp16; replaced UE3 final-color PS (0xB030BAA6 / 0xFE88487E) runs the game's own grade
//   as an extended HDR function, takes its hue from a soft ReinhardPiecewise reference through MacLeod-Boynton
//   emulation, then DICE-maps to the display. The video (0x0E97A4A0) and lens-flare (0x010371F2) passes are
//   replaced too. Core Display Composition does the paper-white scale + encode.
//   One HDR mod owns the swapchain -> any other HDR mod must be removed from the game folder.
// - AA: compute FXAA (3.11 work-queue) -> SMAA (ULTRA + color edge + depth predication) + optional RCAS. Edge
//   detection reads the scene as stored, in GAMMA space (POST_PROCESS_SPACE_TYPE 0, 1.0 = paper white); the blend
//   reads its linear decode. The result is CopyResource'd back into the swapchain.
// - DLSS / FSR 3 (DLAA): the game renders no motion vectors and no jitter; both come from patched shaders (MotionVectorPatches.h,
//   docs/BorderlandsGOTY-DLAA-Research.md). The upscaler runs before the DOF/Bloom gather, SMAA then steps aside (RCAS stays).
//   Render Scale below 100% sets the engine's ScreenPercentage: the scene renders into a top-left sub-rect, which the upscaler fills
//   the output from.
// - AO: XeGTAO replaces the native HBAO+ (hash block below). Plus AF16x and a fix for the game's movie RAM leak.

#define GAME_BORDERLANDS_GOTY 1

// Don't pop the DEVELOPMENT auto-debugger MessageBox on DLL attach: under a borderless/fullscreen game it's
// invisible and blocks the loader (ReShade times out the addon load -> error 1114).
#define DISABLE_AUTO_DEBUGGER 1

#define GEOMETRY_SHADER_SUPPORT 0
#define ENABLE_SMAA 1
// The motion vector and jitter draws wrap the game's own draws, so they need "original_draw_dispatch_func"
#define ENABLE_POST_DRAW_DISPATCH_CALLBACK 1
// The motion vector draw key reads the draw's arguments ("last_draw_dispatch_data")
#define ENABLE_DRAW_DISPATCH_DATA_CACHE 1

#include "..\..\Core\core.hpp"
#include "..\..\External\WDK\includes\d3d11TokenizedProgramFormat.hpp"
#include "MotionVectorPatches.h"
#include <shellapi.h> // ShellExecuteA for About links (system() hangs the render thread in exclusive fullscreen)

// FXAA is a compute work-queue implementation (FXAA 3.11 CS):
//   0x81CDE53D = pass 1 edge-detect (builds WorkQueue into scratch buffers; leaves color untouched — left running).
//   0x08891303 = pass 2 resolve: WorkQueue + Luma + InColor(t2) -> Color(u0, swapchain in-place). Replaced with SMAA.
static constexpr uint32_t kFXAAResolveHash = 0x08891303; // FXAA resolve CS — replaced with SMAA
static constexpr uint32_t kCelShadingHash = 0x08DC66D1;  // cel-shading edge PS — binds scene depth at t0 (predication source)
// UE3's FilterVertexShader<N> (1 to 8 samples, every one in the shader cache): the DOF/Bloom gather's blurs, by sample count
static constexpr std::pair<uint32_t, const char*> kFilterVertexShaders[] = {{0x1C49F98D, "1"}, {0xEB19968C, "2"}, {0x54035855, "3"}, {0xD39A2F25, "4"}, {0xE4078BF4, "5"}, {0x448C5B69, "6"}, {0xA2A5AD0B, "7"}, {0xAF68CEAD, "8"}};

// AO: XeGTAO replaces the game's native NVIDIA HBAO+ (GFSDK_SSAO). Full-res 4K chain:
// deinterleave 0xFFE232A6 -> normals 0xB2B47225 (left running) -> coarse horizon 0xF534EB09 -> bilateral
// blur 0x4E1BEE34 -> apply-multiply PS 0x44764BF6. We capture scene depth at the deinterleave and the
// packed view normals at the coarse pass (skipping both), then at the blur dispatch run the 4 XeGTAO passes
// into ITS u0 (the game's FINAL r16g16_float AO; apply reads .x) so the apply blit composites our AO
// unchanged. XeGTAO reads the game's own cb0 ($Globals: ProjInfo) + cb2 (MinZ_MaxZRatioCS), still bound at
// the injection point. No TAA -> NoiseIndex frozen 0, spatial denoise x2.
static constexpr uint32_t kAODeinterleaveHash = 0xFFE232A6; // scene depth -> quarter-res array — skipped (we build our own mip pyramid)
static constexpr uint32_t kAOCoarseHash = 0xF534EB09;       // HBAO+ horizon march (x2), binds view normals at t0 — skipped (capture normals)
static constexpr uint32_t kAOBlurHash = 0x4E1BEE34;         // bilateral blur -> FINAL r16g16_float u0 — replaced with XeGTAO

// DLSS / FSR: the scene's first post passes, which end a motion vector frame (the upscaler runs right before): the DOF/Bloom
// gather CS (reads the scene copy and the depth copy), else the uber post (DOF and Bloom off in the game's settings).
static constexpr uint32_t kDOFBloomGatherHash = 0xF0D8D818;
static constexpr uint32_t kUberPostHashes[] = {0xB030BAA6, 0xFE88487E};

// Motion vectors for DLSS / FSR (the game renders none, see MotionVectorPatches.h): the opaque draws into the fp16 scene draw with
// patched shaders that also write an extra target, the vertex shader's second run reading the draw's previous frame b0 / b1 / b3.
#if DEVELOPMENT
static bool g_mv_enable = false;
static bool g_mv_debug_view = false;
static bool g_mv_force_jitter = false;       // The projection jitter without an upscaler
static bool g_mv_disable_jitter = false;     // No projection jitter under the upscaler (A/B of jitter-dependent artifacts)
static bool g_skip_outlines = false;         // No cel outlines (world and weapon Sobel), A/B of distant thin-geometry flicker
static bool g_sr_reversible_tonemap = false; // DLSS / FSR on c / (1 + max(c)), undone after (Luma_BL_SRTonemap.hlsl)
#else
static constexpr bool g_mv_enable = false;
static constexpr bool g_mv_force_jitter = false;
static constexpr bool g_mv_disable_jitter = false;
#endif

// DLSS/FSR render scale: Luma sets the engine's ScreenPercentage to it while the upscaler is selected, else to 100 (see "OnPresent")
static float g_render_scale = 1.f;
static constexpr float min_render_scale = 0.5f;
// The engine's ScreenPercentage as Luma last set it, as a fraction
static float g_screen_percentage = 1.f;

// The top-left sub-rect the engine renders the scene into: UE3's appTrunc(output x ScreenPercentage) (ScaleScreenCoords)
static uint2 GetScreenPercentageRenderSize(const float2& output_resolution)
{
   return {uint32_t(g_screen_percentage * output_resolution.x), uint32_t(g_screen_percentage * output_resolution.y)};
}

// ScreenPercentage < 100 shrinks every scene view through FSystemSettings::ScaleScreenCoords (exe 1.5.0.0 RVA 0x4d41e0), not
// only the player's: the scene renders into render targets (the inventory item icons) draw into a top-left sub-rect that nothing
// upscales. Those call sites are skipped (5-byte NOP), each only if its bytes match. The other call sites stay scaled (DEV toggles):
// 0x2b2454 (0x2b2370, the player's main view), 0x81177f (0x810d30: the player's scene view, unscaled it renders full size and the
// stretch zooms into the top left), 0x815d15 (0x813860, GameViewportClient::Draw).
namespace ScaleScreenCoordsPatch
{
   constexpr uint32_t kFunction = 0x4d41e0;
   constexpr uint8_t kFunctionPrologue[] = {0x48, 0x89, 0x5C, 0x24, 0x10, 0x57, 0xF3, 0x0F, 0x10, 0x05}; // mov [rsp+10h], rbx; push rdi; movss xmm0, [ScreenPercentage]
   constexpr uint32_t kRenderTargetViewCalls[] = {0xa598d1, 0x119039d, 0xafda92, 0xae7041};
   constexpr uint32_t kOtherCalls[] = {0x2b2454, 0x81177f, 0x815d15};
   constexpr uint8_t kNop5[] = {0x0F, 0x1F, 0x44, 0x00, 0x00};

   // The exe base if the image reaches "end" and holds this build's ScaleScreenCoords (its prologue and the ScreenPercentage operand)
   std::byte* GetBase(uint32_t end)
   {
      auto* const base = reinterpret_cast<std::byte*>(GetModuleHandle(nullptr));
      const auto* const nt_headers = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + reinterpret_cast<const IMAGE_DOS_HEADER*>(base)->e_lfanew);
      if (nt_headers->OptionalHeader.SizeOfImage < std::max<uint32_t>(end, kFunction + sizeof(kFunctionPrologue) + sizeof(int32_t)) || std::memcmp(base + kFunction, kFunctionPrologue, sizeof(kFunctionPrologue)) != 0)
         return nullptr;
      return base;
   }

   // GSystemSettings.ScreenPercentage (100 = off), from the RIP-relative operand of the prologue's movss. UE3 reads it for every view.
   float* GetScreenPercentage()
   {
      std::byte* const base = GetBase(0);
      if (!base)
         return nullptr;
      constexpr uint32_t operand = kFunction + sizeof(kFunctionPrologue);
      int32_t displacement;
      std::memcpy(&displacement, base + operand, sizeof(displacement));
      return reinterpret_cast<float*>(base + operand + sizeof(displacement) + displacement);
   }

   // Returns whether the call site is in the requested state
   bool SetSkipped(uint32_t call_rva, bool skip)
   {
      std::byte* const base = GetBase(call_rva + sizeof(kNop5));
      if (!base)
         return false;
      uint8_t call[sizeof(kNop5)] = {0xE8};
      const int32_t displacement = int32_t(kFunction) - int32_t(call_rva + sizeof(call));
      std::memcpy(call + 1, &displacement, sizeof(displacement));
      std::byte* const site = base + call_rva;
      const uint8_t* const wanted = skip ? kNop5 : call;
      if (std::memcmp(site, wanted, sizeof(call)) == 0)
         return true;
      if (std::memcmp(site, skip ? call : kNop5, sizeof(call)) != 0)
         return false;
      return System::PatchMemory(site, wanted, sizeof(call), System::PatchMemoryType::Code);
   }

   void Apply(bool render_target_views, const bool (&other)[std::size(kOtherCalls)])
   {
      const auto set = [](uint32_t call_rva, bool skip)
      {
         if (!SetSkipped(call_rva, skip))
            reshade::log::message(reshade::log::level::warning, std::format("[BL] ScaleScreenCoords call 0x{:X}: unexpected bytes, not patched", call_rva).c_str());
      };
      for (const uint32_t call_rva : kRenderTargetViewCalls)
         set(call_rva, render_target_views);
      for (size_t i = 0; i < std::size(kOtherCalls); i++)
         set(kOtherCalls[i], other[i]);
   }
} // namespace ScaleScreenCoordsPatch
static float* g_engine_screen_percentage = nullptr; // "ScaleScreenCoordsPatch::GetScreenPercentage", nullptr for another exe build
#if DEVELOPMENT
static bool g_sp_unscaled_render_target_views = true;
static bool g_sp_unscaled_other_calls[std::size(ScaleScreenCoordsPatch::kOtherCalls)] = {};
static bool g_sp_fix_hud_canvas = true;
// The Canvas draws after the stretch, this frame and the last one: "t0 WxH format, blend, transform" -> count
static std::map<std::string, uint32_t> g_sp_canvas_kinds, g_sp_canvas_kinds_last;
#else
static constexpr bool g_sp_unscaled_render_target_views = true;
static constexpr bool g_sp_unscaled_other_calls[std::size(ScaleScreenCoordsPatch::kOtherCalls)] = {};
static constexpr bool g_sp_fix_hud_canvas = true;
#endif

// User settings, persisted in the [Luma] config section (LoadConfigs) unless noted otherwise.
static bool g_smaa_enable = true;
static float g_rcas_sharpness = 0.f; // RCAS sharpen on SMAA output; default off — the ink outlines are already clean and sharpening haloes them.
static bool g_hide_ui = false;       // hide the game's HUD (skips swapchain-targeting UI draws) — for clean screenshots
// SMAA predication on geometry. The signal is plane-deviation edge-ness built from the scene depth, not the depth
// itself (see Luma_BL_DepthExtract.hlsl); the tolerance is the only free parameter and is a fraction of view
// depth, so MoH Airborne's calibrated 0.02 carries over unchanged.
static bool g_smaa_predication = true;
static float g_smaa_pred_tolerance = 0.02f;
#if DEVELOPMENT
// Calibration aids for the tolerance: what predication does is an ABSENCE of smearing, which the eye reads badly
// and worse in motion, so judge the mask itself rather than the frame.
static bool g_smaa_pred_debug = false;   // show the predication mask instead of the antialiased frame
static bool g_smaa_pred_measure = false; // one-shot: read the mask back and log its distribution (UI button)
#endif

// Ambient Occlusion: XeGTAO replaces the native HBAO+ (default ON = supersede it). Persisted as "XeGTAOEnable".
static bool g_gtao_enable = true;
// Runtime XeGTAO knobs (LumaGTAO cb b11); their sliders are DEVELOPMENT/TEST only. FinalValuePower = primary darkness dial
// (calibrate to the vanilla HBAO+ histogram — its PowExponent does not transfer numerically). DepthScale =
// viewZ divisor (UE3 units, near plane ~10 -> ~meters) so Intel's tuned radius/falloff apply; the dial
// against broad over-occlusion. RadiusOverride > 0 overrides EFFECT_RADIUS (in scaled units).
static float g_gtao_final_value_power = 1.0f;
static float g_gtao_depth_scale = 50.f;
static float g_gtao_radius_override = 0.f;
#if DEVELOPMENT
static int g_gtao_debug_view = 0; // 0=off 1=depth gradient 2=normals 3=AO x8 4=edges (drawn via the game's apply blit). DEV only: the shader's DebugViewRT blocks are #if DEVELOPMENT.
#endif

// Loading-movie memory-leak fix (toggle "Fix Movie Memory Leak" under Fixes).
// The game's Bink movies create D3D11 YUV decode buffers and never release them -> linear RAM
// growth -> OOM. The leak is the GAME, not Luma. We drop the game's leaked COM refs on OLD movie
// generations (orphaned: a movie's buffers are sampled only during its own playback). Tagged by
// creation call-stack RVAs in BorderlandsGOTY.exe (frozen remaster; non-matching build tags nothing
// = safe no-op). Movies keep playing. Diagnosis/validation: _tools/BL Leak Tracker.
static bool g_fix_movie_leak = true; // default ON; persisted as "FixMovieLeak"

namespace BLMovieLeakFix
{
   // Build-specific RVAs (frozen remaster). A non-matching build shifts these -> nothing tags ->
   // silent no-op; BUILD_CHECK_FRAME drives a one-shot telemetry warning for that case.
   constexpr uintptr_t RVA_CREATE_WRAPPER = 0xBFF27; // ret addr in the RHI resource-create wrapper; measured movie-path-only (never hit without the span below)
   constexpr uintptr_t RVA_STREAM_LO = 0x58A000;     // streaming/movie fn span (create call sites)
   constexpr uintptr_t RVA_STREAM_HI = 0x58C000;
   constexpr uint32_t BUILD_CHECK_FRAME = 18000; // ~5 min; movies tag well before this if build matches
   constexpr uint32_t NEW_GEN_GAP_FRAMES = 90;   // frame gap that separates two movies into "generations"
   constexpr int MAX_FRAMES = 32, SKIP_FRAMES = 1;
   constexpr uint64_t STACKWALK_MIN_BYTES = 2ull * 1024 * 1024; // only walk the stack for big resources (cheap)
   constexpr int RELEASE_GEN_LAG = 2;                           // release only gen <= cur_gen-2 (keep current + previous)
   constexpr uint32_t RELEASE_IDLE_FRAMES = 600;                // and only after this many frames since creation (~10s)
   constexpr uint32_t RELEASE_FLUSH_PERIOD = 120;               // flush at most every N present frames

   struct MovieTex
   {
      uint64_t bytes;
      int gen;
      uint32_t created_frame;
   };

   static std::mutex g_mtx;
   static std::unordered_map<uint64_t, MovieTex> g_movie;    // resource handle -> info
   static std::unordered_map<uint64_t, uint64_t> g_view2res; // view handle -> resource handle (tagged buffers only)
   static uintptr_t g_exe_base = 0;
   static int g_cur_gen = 0;
   static uint32_t g_last_movie_frame = 0;
   // Coarse cross-thread gates (present thread writes g_frame; workers read). atomic = no data race;
   // real map sync is g_mtx.
   static std::atomic<bool> g_have_movie{false};
   static std::atomic<uint32_t> g_frame{0};
   static uint64_t g_freed_bytes = 0;
   static uint32_t g_freed_tex = 0;
   static bool g_build_checked = false; // one-shot build-mismatch telemetry guard (present thread only)

   inline bool StackIsMovie(void* const* frames, int n)
   {
      if (!g_exe_base)
         return false;
      bool has_create = false, has_stream = false;
      for (int i = 0; i < n; ++i)
      {
         const uintptr_t a = reinterpret_cast<uintptr_t>(frames[i]);
         if (a < g_exe_base)
            continue;
         const uintptr_t rva = a - g_exe_base;
         if (rva == RVA_CREATE_WRAPPER)
            has_create = true;
         else if (rva >= RVA_STREAM_LO && rva < RVA_STREAM_HI)
            has_stream = true;
      }
      return has_create && has_stream;
   }

   // Tag movie YUV decode buffers at creation (stack walk only for >= 2MB buffers).
   void OnInitResource(reshade::api::device*, const reshade::api::resource_desc& desc, const reshade::api::subresource_data*, reshade::api::resource_usage, reshade::api::resource handle)
   {
      // Decode targets are BUFFERS (measured); gating on type excludes the textures that share the
      // streaming-fn span -> no mistag/UAF, and skips the stack walk for every texture.
      if (desc.type != reshade::api::resource_type::buffer)
         return;
      const uint64_t bytes = desc.buffer.size;
      if (bytes < STACKWALK_MIN_BYTES)
         return;
      void* frames[MAX_FRAMES];
      const int n = RtlCaptureStackBackTrace(SKIP_FRAMES, MAX_FRAMES, frames, nullptr);
      if (n <= 0 || !StackIsMovie(frames, n))
         return;
      const std::lock_guard<std::mutex> lk(g_mtx);
      const uint32_t f = g_frame;
      if (g_cur_gen == 0 || (f - g_last_movie_frame) > NEW_GEN_GAP_FRAMES)
         g_cur_gen += 1;
      g_last_movie_frame = f;
      g_movie[handle.handle] = MovieTex{bytes, g_cur_gen, f};
      g_have_movie = true;
   }

   void OnDestroyResource(reshade::api::device*, reshade::api::resource handle)
   {
      if (!g_have_movie)
         return;
      const std::lock_guard<std::mutex> lk(g_mtx);
      g_movie.erase(handle.handle);
   }

   void OnInitResourceView(reshade::api::device*, reshade::api::resource res, reshade::api::resource_usage, const reshade::api::resource_view_desc&, reshade::api::resource_view view)
   {
      if (!g_have_movie || !view.handle)
         return;
      const std::lock_guard<std::mutex> lk(g_mtx);
      if (g_movie.find(res.handle) != g_movie.end())
         g_view2res[view.handle] = res.handle;
   }

   void OnDestroyResourceView(reshade::api::device*, reshade::api::resource_view view)
   {
      if (!g_have_movie)
         return;
      const std::lock_guard<std::mutex> lk(g_mtx);
      g_view2res.erase(view.handle);
   }

   // Release the game's leaked COM refs on old orphaned generations. Re-entrancy-safe: our Release()
   // re-enters OnDestroyResource[View] on this thread -> COLLECT+UNLINK under lock, then RELEASE unlocked.
   void Flush()
   {
      struct Plan
      {
         uintptr_t res;
         std::vector<uintptr_t> views;
         uint64_t bytes;
      };
      std::vector<Plan> plans;
      {
         const std::lock_guard<std::mutex> lk(g_mtx);
         for (const auto& kv : g_movie) // Phase A: pick victims (keep current + previous gen, must be idle)
         {
            const MovieTex& m = kv.second;
            if (m.gen > g_cur_gen - RELEASE_GEN_LAG)
               continue;
            if ((g_frame - m.created_frame) < RELEASE_IDLE_FRAMES)
               continue;
            plans.push_back({kv.first, {}, m.bytes});
         }
         if (plans.empty())
            return;
         std::unordered_map<uintptr_t, size_t> idx;
         for (size_t i = 0; i < plans.size(); ++i)
            idx[plans[i].res] = i;
         for (const auto& vk : g_view2res)
         {
            auto it = idx.find(vk.second);
            if (it != idx.end())
               plans[it->second].views.push_back(vk.first);
         }
         for (const auto& p : plans) // Phase B: UNLINK before any Release (re-entrant callbacks then find nothing)
         {
            for (uintptr_t v : p.views)
               g_view2res.erase(v);
            g_movie.erase(p.res);
         }
      }
      uint32_t ft = 0;
      uint64_t fb = 0; // Phase C: RELEASE with the lock released
      bool partial = false;
      for (const Plan& p : plans)
      {
         for (uintptr_t v : p.views)
            reinterpret_cast<IUnknown*>(v)->Release();
         // rc==0 => actually freed (count it); rc>0 => game holds extra refs, not reclaimed. (rc is a
         // by-value ULONG, never a deref of a freed object.)
         const ULONG rc = reinterpret_cast<IUnknown*>(p.res)->Release();
         if (rc == 0)
         {
            ft++;
            fb += p.bytes;
         }
         else
            partial = true;
      }
      g_freed_tex += ft;
      g_freed_bytes += fb;
#if DEVELOPMENT || TEST
      if (ft || partial)
      {
         char b[224];
         snprintf(b, sizeof(b), "[BL-Leak] reclaimed %u movie textures, ~%.1f MB (cum %.1f MB)%s",
            ft, fb / 1048576.0, g_freed_bytes / 1048576.0,
            partial ? " [WARN: some buffers held extra game refs, not reclaimed]" : "");
         reshade::log::message(reshade::log::level::info, b);
      }
#else
      (void)partial;
#endif
   }
} // namespace BLMovieLeakFix

struct BorderlandsGotyGameDeviceData final : public GameDeviceData
{
#if DEVELOPMENT || TEST
   bool logged_no_fp16 = false; // warned once: swapchain not fp16 (HDR upgrade absent) -> SMAA skipped
#endif

   // Predication inputs, captured per frame from the cel-shading pass: scene depth (D24S8, viewed
   // r24_unorm_x8_uint) and the PSOffsetConstants (MinZ_MaxZRatio) that pass linearizes it with.
   ComPtr<ID3D11ShaderResourceView> srv_depth;
   ComPtr<ID3D11Buffer> cb_game_offsets;

   // SMAA predication: plane-deviation edge-ness (R16F) built from srv_depth by the BL Depth Extract CS.
   ComPtr<ID3D11Buffer> cb_pred;
   float pred_tolerance = -1.f;
   ComPtr<ID3D11Texture2D> tex_pred;
   ComPtr<ID3D11UnorderedAccessView> uav_pred;
   ComPtr<ID3D11ShaderResourceView> srv_pred;
#if DEVELOPMENT
   // Staging copy for the one-shot mask readback (see LogPredicationStats), allocated on first use.
   struct TextureCapture
   {
      ComPtr<ID3D11Texture2D> staging;
      UINT width = 0;
      UINT height = 0;
      DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
      bool copy_pending = false;
   };
   TextureCapture pred_measure;
#endif

   // SMAA metrics CB (b1) = (1/w, 1/h, w, h) + (predication scale,0,0,0), recreated on resolution or predication-state change.
   ComPtr<ID3D11Buffer> cb_smaa_metrics;
   float smaa_metrics_pred_scale = -1.f; // 2.0 = valid depth (predication active), 1.0 = no/mismatched depth (plain ULTRA threshold)

   // The color size the SMAA, predication and RCAS resources were built at (see the FXAA resolve replacement)
   uint32_t smaa_w = 0, smaa_h = 0;

   // SMAA inputs (fp16), recreated on resolution change: scene-color snapshot (CopyResource'd each frame, edge
   // detection) and its linear-light decode (neighborhood blend).
   ComPtr<ID3D11Texture2D> tex_input;
   ComPtr<ID3D11ShaderResourceView> srv_input;
   ComPtr<ID3D11Texture2D> tex_input_linear;
   ComPtr<ID3D11UnorderedAccessView> uav_input_linear;
   ComPtr<ID3D11ShaderResourceView> srv_input_linear;

   // SMAA output temp (fp16, SRV+RTV). Copied into the swapchain target after SMAA (or fed to RCAS first).
   ComPtr<ID3D11Texture2D> tex_smaa_out;
   ComPtr<ID3D11RenderTargetView> tex_smaa_out_rtv;
   ComPtr<ID3D11ShaderResourceView> tex_smaa_out_srv;

   // RCAS sharpen CB (b0) = (w, h, sharpness, 0), recreated on resolution/sharpness change.
   ComPtr<ID3D11Buffer> cb_sharpen;
   float sharpen_amount = -1.f;
   // RCAS output temp (fp16, RTV). RCAS reads tex_smaa_out_srv -> writes here -> copied into the swapchain target.
   ComPtr<ID3D11Texture2D> tex_rcas_out;
   ComPtr<ID3D11RenderTargetView> tex_rcas_out_rtv;

   // --- XeGTAO scratch (all at the game's AO full-res; cached, rebuilt on size change). ---
   // Game inputs captured per frame (reset in OnPresent): full-res r24 scene depth (deinterleave t0) and
   // the packed view normals (coarse-AO t0). A captured srv_gtao_depth arms the coarse/blur takeover.
   ComPtr<ID3D11ShaderResourceView> srv_gtao_depth;
   ComPtr<ID3D11ShaderResourceView> srv_gtao_normals;
   // Prefiltered viewspace-depth MIP pyramid (R32F, 5 mips) — 5 per-mip UAVs + one full SRV.
   ComPtr<ID3D11Texture2D> tex_gtao_depth_mips;
   ComPtr<ID3D11UnorderedAccessView> gtao_depth_mip_uavs[5];
   ComPtr<ID3D11ShaderResourceView> srv_gtao_depth_mips;
   // Two working AO+edges buffers (R8G8_UNORM) ping-ponged by main pass -> denoise 1.
   ComPtr<ID3D11Texture2D> tex_gtao_working[2];
   ComPtr<ID3D11UnorderedAccessView> uav_gtao_working[2];
   ComPtr<ID3D11ShaderResourceView> srv_gtao_working[2];
   uint32_t gtao_w = 0, gtao_h = 0;
   // LumaGTAO knob CB (b11) = (FinalValuePower, DepthScale, RadiusOverride, DebugView); recreated on change.
   ComPtr<ID3D11Buffer> cb_gtao;
   float gtao_cb_data[4] = {}; // What cb_gtao was built with; meaningful only while cb_gtao exists

   // "IsSRActive", taken at present: Core's "Super Resolution" selection changes after it, mid frame for the draws
   bool sr_active = false;
   // "DrawUpscaler" made a new upscaler output texture (see there)
   bool sr_output_recreated = false;
#if DEVELOPMENT
   com_ptr<ID3D11Texture2D> sr_tonemap_input; // "SR Reversible Tonemap": the upscaler's input
#endif

   // ScreenPercentage < 100: the uber's target in place of the game's R8G8B8A8 intermediate (HDR), read by the stretch
   com_ptr<ID3D11Texture2D> sp_intermediate;
   com_ptr<ID3D11RenderTargetView> sp_intermediate_rtv;
   com_ptr<ID3D11ShaderResourceView> sp_intermediate_srv;
   // ... and RCAS on it under DLSS / FSR, which the stretch then reads (see "OnDrawOrDispatch")
   ComPtr<ID3D11Texture2D> sp_sharpened;
   ComPtr<ID3D11RenderTargetView> sp_sharpened_rtv;
   ComPtr<ID3D11ShaderResourceView> sp_sharpened_srv;
   com_ptr<ID3D11Buffer> sp_sharpen_cb;
   // DLSS / FSR below ScreenPercentage 100: the upscaler's output (Core's) and the view the uber reads it through
   com_ptr<ID3D11Texture2D> sp_upscaled_texture;
   com_ptr<ID3D11ShaderResourceView> sp_upscaled_srv;
   // ... and its downscale into the render sub-rect of the scene copy (see "DrawUpscaler")
   com_ptr<ID3D11Texture2D> sp_downscale_target;
   com_ptr<ID3D11RenderTargetView> sp_downscale_rtv;
   // The DOF/Bloom gather and its blurs at output resolution this frame (see "Luma_BL_OutputGather.hlsl"): their constants (b5) and
   // the gather's target, which the blurs' copies go back into
   bool sp_post_output_res = false;
   com_ptr<ID3D11Buffer> sp_post_scale_cb;
   uint64_t sp_gather_target = 0;
   // DLSS / FSR: the per render pixel DOF blur amount accumulated over frames (Luma_BL_DOFHistory.hlsl), ping-pong
   com_ptr<ID3D11Texture2D> dof_history[2];
   com_ptr<ID3D11ShaderResourceView> dof_history_srvs[2];
   com_ptr<ID3D11UnorderedAccessView> dof_history_uavs[2];
   uint32_t dof_history_index = 0;
   bool dof_history_valid = false;
   bool sp_stretched = false; // The stretch drew this frame: the UI follows

   // Motion vectors: shaders patched on first use, by original hash (null on failure), and the target (sized like the scene; every
   // blend state writes it unblended, see "OnCreateBlendState")
   std::atomic<bool> mv_active = false; // Motion vectors and jitter this frame: an upscaler is active, or the DEV toggle (set at present)
   std::shared_mutex mv_mutex;
   std::unordered_map<uint32_t, com_ptr<ID3D11VertexShader>> mv_vertex_shaders;
   std::unordered_map<uint32_t, com_ptr<ID3D11PixelShader>> mv_pixel_shaders;
   // The patched vertex shaders that read b3 (skinned), and the byte offset of each one's LocalToWorld translation row in b0 (none
   // for world space geometry, matched by draw key alone), by original hash
   std::unordered_set<uint32_t> mv_bone_vertex_shaders;
   std::unordered_map<uint32_t, uint32_t> mv_translation_offsets;
   com_ptr<ID3D11Texture2D> mv_texture;
   com_ptr<ID3D11RenderTargetView> mv_rtv;
   com_ptr<ID3D11ShaderResourceView> mv_srv;  // Read by the DOF history
   com_ptr<ID3D11UnorderedAccessView> mv_uav; // Null without typed UAV loads of R32G32_FLOAT (then no fill)
   // A frame opens at its first mesh draw into output sized depth (the depth prepass: the jitter is chosen there), starts at its
   // first motion vector draw (the target is cleared) and ends at the first post pass ("kDOFBloomGatherHash", the uber post), once
   // per present.
   bool mv_scene_open = false;
   bool mv_scene_done = false;
   bool mv_frame_ended = true;
   // The frame's scene depth (the depth view's resource), its last copy (the post passes and the outlines read that copy), and a
   // view of the one the camera fill and the upscaler read (the depth itself if it can be read, else the copy)
   com_ptr<ID3D11Resource> mv_depth;
   uint64_t mv_depth_copy = 0;
   com_ptr<ID3D11ShaderResourceView> mv_depth_srv;
   bool mv_fill_pending = false;
   com_ptr<ID3D11Buffer> mv_fill_buffer;
   // The fp16 scene the motion vector draws write, and its last copy (the DOF/Bloom gather and the uber post read the copy)
   com_ptr<ID3D11Resource> mv_scene_color;
   uint64_t mv_scene_color_copy = 0;
   // The projection jitter (pixels, +y down), chosen when the scene opens; its NDC offset is at VS "MotionVectorPatches::jitter_slot"
   // of every mesh draw depth tested against the scene
   std::array<float, 2> mv_jitter = {};
   std::array<float, 2> mv_jitter_ndc = {}; // The same offset in NDC (y up), as the jitter buffer holds it
   com_ptr<ID3D11Buffer> mv_jitter_buffer;
   // Per-draw lookups kept for the next draw (reset when the scene opens, views and states can be recreated between scenes): the
   // jitter path's last depth view and whether it's the scene depth, its last depth stencil state's depth test, the motion vector
   // path's last accepted targets and the last blend state's opacity (null = the default state, opaque), the last vertex and pixel
   // shader's patched versions (owned by "mv_vertex_shaders" / "mv_pixel_shaders", never erased) and the vertex shader's facts.
   ID3D11DepthStencilView* jitter_dsv = nullptr;
   bool jitter_dsv_scene = false;
   ID3D11DepthStencilState* jitter_depth_stencil_state = nullptr;
   bool jitter_depth_test = true;
   ID3D11RenderTargetView* mv_accepted_rtv = nullptr;
   ID3D11DepthStencilView* mv_accepted_dsv = nullptr;
   ID3D11BlendState* mv_blend_state = nullptr;
   bool mv_blend_opaque = true;
   uint32_t mv_last_vertex_shader_hash = 0;
   ID3D11VertexShader* mv_last_vertex_shader = nullptr;
   bool mv_last_vertex_shader_skinned = false;
   uint32_t mv_last_vertex_shader_translation = UINT_MAX;
   uint32_t mv_last_pixel_shader_hash = 0;
   ID3D11PixelShader* mv_last_pixel_shader = nullptr;
   // A patched shader left bound after its draw, with the game's shader it replaced (see "BindPatchedShader")
   template <typename T>
   struct BoundShader
   {
      T* patched = nullptr;
      com_ptr<T> game;
   };
   BoundShader<ID3D11VertexShader> mv_bound_vertex_shader;
   BoundShader<ID3D11PixelShader> mv_bound_pixel_shader;

   // CPU copies of the b0 / b1 / b3 buffers the motion vector draws bind, by buffer (an entry registers it, null until its first
   // update): the engine uploads every one with UpdateSubresource (whole buffer, only when a constant changed), so a draw's constants
   // are its buffer's latest copy
   using ConstantsCopy = std::shared_ptr<const std::vector<uint8_t>>;
   std::mutex mv_constants_mutex;
   std::unordered_map<uint64_t, ConstantsCopy> mv_constants_copies;
   // ScreenPercentage < 100: the last "Transform" (cb0, a float4x4) uploaded to each buffer a Canvas draw after the stretch bound (empty
   // until its first update)
   std::unordered_map<uint64_t, std::optional<std::array<float, 16>>> sp_canvas_transforms;
   // The previous frame's b0 / b1 / b3 uploads, by "MotionVectorPatches::previous_slots" index: one dynamic buffer written in order
   // without renaming and bound at an offset (D3D11.1 constant buffer offsetting), restarted with a discard when full; without device
   // support, a dynamic buffer per slot, renamed at every upload
   static constexpr UINT mv_ring_size = 4 << 20;
   com_ptr<ID3D11Buffer> mv_ring;
   ID3D11DeviceContext1* mv_ring_context = nullptr;
   UINT mv_ring_offset = 0;
   bool mv_ring_checked = false;
   com_ptr<ID3D11Buffer> mv_previous_buffers[std::size(MotionVectorPatches::previous_slots)];
   // Motion vector draws by draw key (shaders, buffers, arguments), with the LocalToWorld translation and b0 / b1 / b3. A draw takes
   // the previous frame's constants of its key's nearest draw (same object, a frame earlier), its camera included: the first person
   // weapon draws with a camera of its own.
   struct MotionVectorObject
   {
      std::array<float, 3> translation;
      ConstantsCopy object; // b0
      ConstantsCopy camera; // b1
      ConstantsCopy bones;  // b3, skinned draws only
   };
   std::unordered_map<uint64_t, std::vector<MotionVectorObject>> mv_objects;
   std::unordered_map<uint64_t, std::vector<MotionVectorObject>> mv_previous_objects;
   // The frame's world camera (b1 of its first motion vector draw) and the previous frame's
   ConstantsCopy mv_camera;
   ConstantsCopy mv_previous_camera;
   uint32_t mv_frame_index = 0; // The Luma frame index of the last motion vector frame

#if DEVELOPMENT
   // Per frame counts for the DEV panel (the last complete frame's shown)
   struct MotionVectorStats
   {
      uint32_t motion_vector_draws = 0, jitter_draws = 0, matched = 0, camera_only = 0, other_camera = 0, uncopied = 0, updates = 0, sr_draws = 0;
      uint32_t ended_by = 0;
      uint32_t rejected[8] = {};                                                                     // "DrawWithMotionVectors" refusals by reason ("MV_REJECT")
      uint32_t rejected_format = 0, rejected_dimension = 0, rejected_width = 0, rejected_height = 0; // The last target refused by format or size
   };
   MotionVectorStats mv_stats, mv_last_stats;
#endif

   void ReleaseGTAOScratch()
   {
      tex_gtao_depth_mips.reset();
      for (auto& uav : gtao_depth_mip_uavs)
         uav.reset();
      srv_gtao_depth_mips.reset();
      for (int i = 0; i < 2; i++)
      {
         tex_gtao_working[i].reset();
         uav_gtao_working[i].reset();
         srv_gtao_working[i].reset();
      }
      gtao_w = gtao_h = 0;
   }
};

class BorderlandsGoty final : public Game
{
   static BorderlandsGotyGameDeviceData& GetGameDeviceData(DeviceData& device_data)
   {
      return *static_cast<BorderlandsGotyGameDeviceData*>(device_data.game);
   }

   // Named injected shaders live in unordered_maps the render thread otherwise only reads: look them up with
   // "find" (operator[] would default-insert on a miss and mutate a map DrawSMAA reads concurrently).
   template <typename ShaderMap>
   static auto FindShader(const ShaderMap& shaders, uint32_t name_hash)
   {
      const auto it = shaders.find(name_hash);
      return it != shaders.end() ? it->second.get() : nullptr;
   }

   template <typename ShaderMap>
   static bool AllShadersReady(const ShaderMap& shaders, std::initializer_list<uint32_t> name_hashes)
   {
      for (uint32_t name_hash : name_hashes)
      {
         if (FindShader(shaders, name_hash) == nullptr)
            return false;
      }
      return true;
   }

   // Create an IMMUTABLE constant buffer holding `size` bytes from `data`. Resets `out`; returns true on success.
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

   // Create a DEFAULT-usage 2D texture (1 mip, 1 sample) of w×h with the given bind flags, fp16 unless another
   // format is asked for. Resets `out`.
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

#if DEVELOPMENT
   // One-shot readback of the predication mask, so g_smaa_pred_tolerance is calibrated from numbers rather than
   // from screenshots. A mean would be useless here: on a well-tuned frame the mask is 0 nearly everywhere and any
   // average drowns in that, so this reports COVERAGE at the level SMAA actually compares against (0.5) plus the
   // shape either side of it. Every texel is read, not a stride - a 1px silhouette is precisely the signal a
   // subsample would step over. Copies on the frame the button is pressed and maps on a later one (non-blocking),
   // so the press never stalls the render thread.
   static void LogPredicationStats(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, BorderlandsGotyGameDeviceData& gd)
   {
      auto& capture = gd.pred_measure;
      if (!gd.tex_pred || (!g_smaa_pred_measure && !capture.copy_pending))
         return;

      D3D11_TEXTURE2D_DESC td = {};
      gd.tex_pred->GetDesc(&td);
      if (capture.width != td.Width || capture.height != td.Height || capture.format != td.Format)
      {
         D3D11_TEXTURE2D_DESC sd = td;
         sd.Usage = D3D11_USAGE_STAGING;
         sd.BindFlags = 0;
         sd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
         sd.MiscFlags = 0;
         capture.staging.reset();
         capture.copy_pending = false;
         if (FAILED(native_device->CreateTexture2D(&sd, nullptr, capture.staging.put())) || !capture.staging)
         {
            capture.width = 0;
            return;
         }
         capture.width = td.Width;
         capture.height = td.Height;
         capture.format = td.Format;
      }

      if (!capture.copy_pending)
      {
         g_smaa_pred_measure = false;
         native_device_context->CopyResource(capture.staging.get(), gd.tex_pred.get());
         capture.copy_pending = true;
         return;
      }

      D3D11_MAPPED_SUBRESOURCE mapped = {};
      if (FAILED(native_device_context->Map(capture.staging.get(), 0, D3D11_MAP_READ, D3D11_MAP_FLAG_DO_NOT_WAIT, &mapped)) || mapped.pData == nullptr)
         return; // still in flight, retry next frame

      constexpr uint32_t kBins = 256;
      uint64_t histogram[kBins] = {};
      uint64_t total = 0;
      uint64_t non_finite = 0;
      for (UINT y = 0; y < capture.height; y++)
      {
         const uint16_t* row = (const uint16_t*)((const uint8_t*)mapped.pData + (size_t)y * mapped.RowPitch);
         for (UINT x = 0; x < capture.width; x++)
         {
            const float v = DirectX::PackedVector::XMConvertHalfToFloat(row[x]);
            // The range test, not a clamp: a NaN fails BOTH ordered comparisons, so the clamped form would fall
            // through to (uint32_t)NaN and index far outside this stack array. The extract CS saturates, so a NaN
            // here means it met one upstream; count them rather than bin them, a silent 0 would read as flat.
            if (!(v >= 0.f && v <= 1.f))
            {
               non_finite++;
               total++;
               continue;
            }
            histogram[(uint32_t)(v * (float)(kBins - 1))]++;
            total++;
         }
      }
      native_device_context->Unmap(capture.staging.get(), 0);
      capture.copy_pending = false;

      auto fraction_above = [&](float level)
      {
         uint64_t hits = 0;
         for (uint32_t b = (uint32_t)(level * (float)(kBins - 1)) + 1u; b < kBins; b++)
            hits += histogram[b];
         return 100.0 * (double)hits / (double)total;
      };
      auto percentile = [&](double p)
      {
         const uint64_t target = (uint64_t)(p * (double)total);
         uint64_t running = 0;
         for (uint32_t b = 0; b < kBins; b++)
         {
            running += histogram[b];
            if (running >= target)
               return (float)b / (float)(kBins - 1);
         }
         return 1.f;
      };

      char line[512];
      snprintf(line, sizeof(line),
         "[BL-Pred] tol=%.4f | FIRES(>0.5)=%.3f%% | >0.1=%.3f%% >0.25=%.3f%% >0.75=%.3f%% >0.9=%.3f%% | flat(bin0)=%.2f%% | p50=%.3f p90=%.3f p99=%.3f p999=%.3f | nonfinite=%llu | %ux%u",
         g_smaa_pred_tolerance, fraction_above(0.5f), fraction_above(0.1f), fraction_above(0.25f), fraction_above(0.75f), fraction_above(0.9f),
         100.0 * (double)histogram[0] / (double)total, percentile(0.5), percentile(0.9), percentile(0.99), percentile(0.999),
         (unsigned long long)non_finite, capture.width, capture.height);
      reshade::log::message(reshade::log::level::info, line);
   }
#endif

   // Writes a dynamic constant buffer, (re)created at the data's size; false if it can't
   static bool WriteConstants(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, com_ptr<ID3D11Buffer>* buffer, const void* data, UINT size)
   {
      D3D11_BUFFER_DESC desc = {};
      if (*buffer)
         (*buffer)->GetDesc(&desc);
      if (desc.ByteWidth != size)
      {
         buffer->reset();
         desc = {size, D3D11_USAGE_DYNAMIC, D3D11_BIND_CONSTANT_BUFFER, D3D11_CPU_ACCESS_WRITE};
         native_device->CreateBuffer(&desc, nullptr, &(*buffer));
      }
      D3D11_MAPPED_SUBRESOURCE mapped;
      if (!*buffer || FAILED(native_device_context->Map(buffer->get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
         return false;
      std::memcpy(mapped.pData, data, size);
      native_device_context->Unmap(buffer->get(), 0);
      return true;
   }

   // Whether a resource is one of the swapchain's back buffers (the UI draws into them, the uber does at ScreenPercentage 100)
   static bool IsBackBuffer(DeviceData& device_data, ID3D11Resource* resource)
   {
      const std::shared_lock lock(device_data.mutex);
      return device_data.back_buffers.contains(reinterpret_cast<uint64_t>(resource));
   }

   // An upscaler is picked and hasn't failed (it then gives way to SMAA until picked again). Fixed for the whole frame (see
   // "OnPresent"): a selection made after the motion vector state was set would otherwise run the upscaler on mixed state.
   static bool IsSRActive(DeviceData& device_data)
   {
      return GetGameDeviceData(device_data).sr_active;
   }

   // Binds a motion vector draw's previous b0 / b1 / b3 at "MotionVectorPatches::previous_slots": each upload (null: the draw's
   // current buffer, zero motion) in the ring when the device supports constant buffer offsets, else in its own renamed buffer
   static void BindPreviousConstants(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, BorderlandsGotyGameDeviceData* game_device_data, const std::vector<uint8_t>* const (&uploads)[std::size(MotionVectorPatches::previous_slots)], ID3D11Buffer* const (&current)[std::size(MotionVectorPatches::previous_slots)])
   {
      constexpr size_t count = std::size(MotionVectorPatches::previous_slots);
      if (!std::exchange(game_device_data->mv_ring_checked, true))
      {
         D3D11_FEATURE_DATA_D3D11_OPTIONS options = {};
         com_ptr<ID3D11DeviceContext1> context1;
         const D3D11_BUFFER_DESC desc = {BorderlandsGotyGameDeviceData::mv_ring_size, D3D11_USAGE_DYNAMIC, D3D11_BIND_CONSTANT_BUFFER, D3D11_CPU_ACCESS_WRITE};
         if (SUCCEEDED(native_device->CheckFeatureSupport(D3D11_FEATURE_D3D11_OPTIONS, &options, sizeof(options))) && options.ConstantBufferOffsetting && options.MapNoOverwriteOnDynamicConstantBuffer && SUCCEEDED(native_device_context->QueryInterface(&context1)) && SUCCEEDED(native_device->CreateBuffer(&desc, nullptr, &game_device_data->mv_ring)))
         {
            // Not referenced: the immediate context lives as long as the device (holding it would keep the device alive)
            game_device_data->mv_ring_context = context1.get();
            game_device_data->mv_ring_offset = BorderlandsGotyGameDeviceData::mv_ring_size; // The first write discards
         }
         reshade::log::message(reshade::log::level::info, std::format("[BL MV] previous constants {}", game_device_data->mv_ring ? "in a ring (constant buffer offsets)" : "in renamed buffers (no constant buffer offsets)").c_str());
      }

      ID3D11Buffer* buffers[count];
      UINT sizes[count] = {};
      UINT total = 0;
      for (size_t i = 0; i < count; i++)
      {
         buffers[i] = current[i];
         // "FirstConstant" and "NumConstants" are multiples of 16 constants (256 bytes)
         if (uploads[i])
            total += sizes[i] = (UINT(uploads[i]->size()) + 255u) & ~255u;
      }
      if (game_device_data->mv_ring && total != 0 && total <= BorderlandsGotyGameDeviceData::mv_ring_size)
      {
         const bool restart = game_device_data->mv_ring_offset + total > BorderlandsGotyGameDeviceData::mv_ring_size;
         D3D11_MAPPED_SUBRESOURCE mapped;
         if (SUCCEEDED(native_device_context->Map(game_device_data->mv_ring.get(), 0, restart ? D3D11_MAP_WRITE_DISCARD : D3D11_MAP_WRITE_NO_OVERWRITE, 0, &mapped)))
         {
            UINT offset = restart ? 0u : game_device_data->mv_ring_offset;
            UINT first_constants[count], constant_counts[count];
            for (size_t i = 0; i < count; i++)
            {
               if (!uploads[i])
                  continue;
               std::memcpy(static_cast<uint8_t*>(mapped.pData) + offset, uploads[i]->data(), uploads[i]->size());
               first_constants[i] = offset / 16;
               constant_counts[i] = sizes[i] / 16;
               offset += sizes[i];
            }
            native_device_context->Unmap(game_device_data->mv_ring.get(), 0);
            game_device_data->mv_ring_offset = offset;
            ID3D11Buffer* const ring = game_device_data->mv_ring.get();
            for (size_t i = 0; i < count; i++)
            {
               if (uploads[i])
                  game_device_data->mv_ring_context->VSSetConstantBuffers1(MotionVectorPatches::previous_slots[i].second, 1, &ring, &first_constants[i], &constant_counts[i]);
               else
                  native_device_context->VSSetConstantBuffers(MotionVectorPatches::previous_slots[i].second, 1, &buffers[i]);
            }
            return;
         }
      }
      for (size_t i = 0; i < count; i++)
      {
         if (uploads[i] && WriteConstants(native_device, native_device_context, std::addressof(game_device_data->mv_previous_buffers[i]), uploads[i]->data(), UINT(uploads[i]->size())))
            buffers[i] = game_device_data->mv_previous_buffers[i].get();
         native_device_context->VSSetConstantBuffers(MotionVectorPatches::previous_slots[i].second, 1, &buffers[i]);
      }
   }

   // A b0 / b1 / b3 buffer's CPU copy (null until its first update); registers it for a copy at every update. Under "mv_constants_mutex".
   static BorderlandsGotyGameDeviceData::ConstantsCopy GetConstantsCopy(BorderlandsGotyGameDeviceData* game_device_data, ID3D11Buffer* buffer)
   {
      return buffer ? game_device_data->mv_constants_copies[reinterpret_cast<uint64_t>(buffer)] : nullptr;
   }

   // Motion vectors: the CPU copy of a registered b0 / b1 / b3 buffer, from the game's UpdateSubresource (before it runs). The engine
   // uploads whole buffers (no box: "size" is UINT64_MAX); a partial update is merged into the last copy.
   // ponytail: one allocation per update (a few thousand a frame); pool the copies if the hook shows in a profile
   static bool OnUpdateBufferRegion(reshade::api::device* device, const void* data, reshade::api::resource resource, uint64_t offset, uint64_t size)
   {
      DeviceData* const device_data = device->get_private_data<DeviceData>();
      if (!device_data || !device_data->game || !data)
         return false;
      auto& game_device_data = GetGameDeviceData(*device_data);
      if (g_screen_percentage >= 1.f && !game_device_data.mv_active)
         return false;
      const std::lock_guard lock(game_device_data.mv_constants_mutex);
      if (const auto canvas = game_device_data.sp_canvas_transforms.find(resource.handle); canvas != game_device_data.sp_canvas_transforms.end() && offset == 0 && size >= sizeof(float) * 16)
         std::memcpy(canvas->second.emplace().data(), data, sizeof(float) * 16);
      if (!game_device_data.mv_active)
         return false;
      const auto copy = game_device_data.mv_constants_copies.find(resource.handle);
      if (copy == game_device_data.mv_constants_copies.end())
         return false;
      D3D11_BUFFER_DESC desc;
      reinterpret_cast<ID3D11Buffer*>(resource.handle)->GetDesc(&desc);
      if (offset >= desc.ByteWidth)
         return false;
      const size_t updated_size = size_t((std::min)(size, uint64_t(desc.ByteWidth) - offset));
      const auto* const bytes = static_cast<const uint8_t*>(data);
      if (updated_size == desc.ByteWidth)
      {
         copy->second = std::make_shared<std::vector<uint8_t>>(bytes, bytes + updated_size);
      }
      else
      {
         auto updated = copy->second && copy->second->size() == desc.ByteWidth ? std::make_shared<std::vector<uint8_t>>(*copy->second) : std::make_shared<std::vector<uint8_t>>(desc.ByteWidth);
         std::memcpy(updated->data() + offset, bytes, updated_size);
         copy->second = std::move(updated);
      }
#if DEVELOPMENT
      game_device_data.mv_stats.updates++;
#endif
      return false;
   }

   // Motion vectors: the copies of the scene and of its depth (the last ones before the first post pass are what post reads)
   bool OverrideCopyResource(ID3D11Device* native_device, DeviceData& device_data, uint64_t& dst_resource, uint64_t& src_resource) override
   {
      auto& game_device_data = GetGameDeviceData(device_data);
      if (game_device_data.mv_scene_open)
      {
         if (src_resource == uint64_t(game_device_data.mv_scene_color.get()))
            game_device_data.mv_scene_color_copy = dst_resource;
         else if (src_resource == uint64_t(game_device_data.mv_depth.get()))
            game_device_data.mv_depth_copy = dst_resource;
      }
      return false;
   }
   bool OverrideCopyTextureRegion(ID3D11Device* native_device, DeviceData& device_data, uint64_t& dst_resource, uint32_t dst_subresource, const D3D11_BOX* dst_box, uint64_t& src_resource, uint32_t src_subresource, const D3D11_BOX* src_box) override
   {
      // The gather's blurs at output resolution: their copies back into its target take the whole texture, not the render sub-rect
      if (auto& game_device_data = GetGameDeviceData(device_data); game_device_data.sp_post_output_res && src_box && dst_resource == game_device_data.sp_gather_target)
      {
         com_ptr<ID3D11DeviceContext> immediate_context;
         native_device->GetImmediateContext(&immediate_context);
         immediate_context->CopyResource(reinterpret_cast<ID3D11Resource*>(dst_resource), reinterpret_cast<ID3D11Resource*>(src_resource));
         return true;
      }
      return OverrideCopyResource(native_device, device_data, dst_resource, src_resource);
   }

   // The bound shader's motion vector version, patched from Core's bytecode copy on first use (null if it can't be). Vertex shaders
   // that don't place vertices with b1's ViewProjectionMatrix (height fog quads bind it for the camera position only) are refused:
   // jittered, they would shift against their own UVs.
   template <typename T>
   static com_ptr<T> GetMotionVectorShader(ID3D11Device* native_device, DeviceData& device_data, std::unordered_map<uint32_t, com_ptr<T>>* shaders, uint32_t hash, reshade::api::pipeline pipeline)
   {
      constexpr bool vertex = std::is_same_v<T, ID3D11VertexShader>;
      auto& game_device_data = GetGameDeviceData(device_data);
      {
         const std::shared_lock lock(game_device_data.mv_mutex);
         if (const auto it = shaders->find(hash); it != shaders->end())
            return it->second;
      }
      std::vector<uint8_t> patched;
      std::string error = "no bytecode";
      bool screen_space = false;
      {
         const std::shared_lock lock(s_mutex_generic);
         if (const auto it = device_data.pipeline_cache_by_pipeline_handle.find(pipeline.handle); it != device_data.pipeline_cache_by_pipeline_handle.end() && it->second->subobjects_cache)
         {
            const auto* desc = static_cast<const reshade::api::shader_desc*>(it->second->subobjects_cache[0].data);
            const auto* code = static_cast<const uint8_t*>(desc->code);
            if constexpr (vertex)
               patched = MotionVectorPatches::PatchVertexShader(code, desc->code_size, &error);
            else
               patched = MotionVectorPatches::PatchPixelShader(code, desc->code_size, &error);
            com_ptr<ID3D11ShaderReflection> reflection;
            if (vertex && !patched.empty() && Shader::d3d_reflect && SUCCEEDED(Shader::d3d_reflect(code, desc->code_size, IID_PPV_ARGS(&reflection))))
            {
               D3D11_SHADER_VARIABLE_DESC variable_desc;
               if (FAILED(reflection->GetConstantBufferByName("VSOffsetConstants")->GetVariableByName("ViewProjectionMatrix")->GetDesc(&variable_desc)) || (variable_desc.uFlags & D3D_SVF_USED) == 0)
               {
                  patched.clear();
                  error = "screen space (ViewProjectionMatrix unused)";
                  screen_space = true;
               }
               else
               {
                  D3D11_SHADER_INPUT_BIND_DESC bind_desc;
                  const bool skinned = SUCCEEDED(reflection->GetResourceBindingDescByName("VSBoneConstants", &bind_desc)) && bind_desc.BindPoint == MotionVectorPatches::previous_slots[2].first;
                  // The object's translation, LocalToWorld's 4th row (row vectors)
                  const bool translated = SUCCEEDED(reflection->GetConstantBufferByName("$Globals")->GetVariableByName("LocalToWorld")->GetDesc(&variable_desc)) && (variable_desc.uFlags & D3D_SVF_USED) != 0;
                  const std::unique_lock lock(game_device_data.mv_mutex);
                  if (skinned)
                     game_device_data.mv_bone_vertex_shaders.insert(hash);
                  if (translated)
                     game_device_data.mv_translation_offsets[hash] = variable_desc.StartOffset + 3 * 16;
               }
            }
         }
      }
      com_ptr<T> shader;
      if (!patched.empty())
      {
         HRESULT hr;
         if constexpr (vertex)
            hr = native_device->CreateVertexShader(patched.data(), patched.size(), nullptr, &shader);
         else
            hr = native_device->CreatePixelShader(patched.data(), patched.size(), nullptr, &shader);
         if (FAILED(hr))
            error = std::format("create 0x{:08X}", uint32_t(hr));
      }
      // Failures in every build (bug reports), every patched shader only in development
      if (DEVELOPMENT || !shader)
         reshade::log::message((shader || screen_space) ? reshade::log::level::info : reshade::log::level::warning, std::format("[BL MV] {} 0x{:08X} {}", vertex ? "VS" : "PS", hash, shader ? "patched" : error).c_str());
      const std::unique_lock lock(game_device_data.mv_mutex);
      return shaders->try_emplace(hash, shader).first->second;
   }

   // The bound vertex shader's patched version (null if refused), looked up again only when the game's changes
   static ID3D11VertexShader* GetPatchedVertexShader(ID3D11Device* native_device, CommandListData& cmd_list_data, DeviceData& device_data, uint32_t hash)
   {
      auto& game_device_data = GetGameDeviceData(device_data);
      if (hash != game_device_data.mv_last_vertex_shader_hash)
      {
         game_device_data.mv_last_vertex_shader = GetMotionVectorShader(native_device, device_data, &game_device_data.mv_vertex_shaders, hash, cmd_list_data.pipeline_state_original_vertex_shader).get();
         const std::shared_lock lock(game_device_data.mv_mutex);
         game_device_data.mv_last_vertex_shader_skinned = game_device_data.mv_bone_vertex_shaders.contains(hash);
         const auto translation = game_device_data.mv_translation_offsets.find(hash);
         game_device_data.mv_last_vertex_shader_translation = translation != game_device_data.mv_translation_offsets.end() ? translation->second : UINT_MAX;
         game_device_data.mv_last_vertex_shader_hash = hash;
      }
      return game_device_data.mv_last_vertex_shader;
   }

   template <typename T>
   static com_ptr<T> GetBoundShader(ID3D11DeviceContext* native_device_context)
   {
      com_ptr<T> shader;
      if constexpr (std::is_same_v<T, ID3D11VertexShader>)
         native_device_context->VSGetShader(&shader, nullptr, nullptr);
      else
         native_device_context->PSGetShader(&shader, nullptr, nullptr);
      return shader;
   }
   template <typename T>
   static void SetBoundShader(ID3D11DeviceContext* native_device_context, T* shader)
   {
      if constexpr (std::is_same_v<T, ID3D11VertexShader>)
         native_device_context->VSSetShader(shader, nullptr, 0);
      else
         native_device_context->PSSetShader(shader, nullptr, 0);
   }

   // Binds a patched shader and leaves it bound after the draw (set directly, bypassing Core's state tracking). If the game hasn't
   // bound another since, the next draw has the same original shader: it binds it again (a no-op) or puts the game's back first.
   template <typename T>
   static void BindPatchedShader(ID3D11DeviceContext* native_device_context, T* patched, BorderlandsGotyGameDeviceData::BoundShader<T>* bound)
   {
      com_ptr<T> current = GetBoundShader<T>(native_device_context);
      if (current.get() == patched)
         return;
      bound->game = std::move(current);
      bound->patched = patched;
      SetBoundShader(native_device_context, patched);
   }

   // Puts the game's shader back where a patched one is still bound, before a draw that must not use it
   template <typename T>
   static void RestoreGameShader(ID3D11DeviceContext* native_device_context, BorderlandsGotyGameDeviceData::BoundShader<T>* bound)
   {
      if (!bound->patched)
         return;
      if (GetBoundShader<T>(native_device_context).get() == bound->patched)
         SetBoundShader(native_device_context, bound->game.get());
      bound->patched = nullptr;
      bound->game.reset();
   }

   // Opens the scene at the frame's first mesh draw into output sized depth (the world depth prepass): takes the scene depth and
   // picks the jitter the whole scene draws with. Once per present (the HUD and later passes never reopen it).
   static void OpenScene(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, CommandListData& cmd_list_data, DeviceData& device_data, uint32_t vertex_shader_hash, ID3D11DepthStencilView* dsv)
   {
      auto& game_device_data = GetGameDeviceData(device_data);
      uint4 depth_size;
      DXGI_FORMAT unused_format;
      GetResourceInfo(dsv, depth_size, unused_format);
      if (depth_size.x != device_data.output_resolution.x || depth_size.y != device_data.output_resolution.y || !GetPatchedVertexShader(native_device, cmd_list_data, device_data, vertex_shader_hash))
         return;
      game_device_data.mv_scene_open = true;
      game_device_data.mv_depth.reset();
      dsv->GetResource(&game_device_data.mv_depth);
      game_device_data.mv_depth_copy = 0;
      game_device_data.mv_scene_color.reset();
      game_device_data.mv_scene_color_copy = 0;
      game_device_data.jitter_dsv = nullptr;
      game_device_data.jitter_depth_stencil_state = nullptr;
      game_device_data.jitter_depth_test = true;
      game_device_data.mv_accepted_rtv = nullptr;
      game_device_data.mv_accepted_dsv = nullptr;
      game_device_data.mv_blend_state = nullptr;
      game_device_data.mv_blend_opaque = true;
      // Halton (2, 3) over the upscaler's phase count; pixels to NDC (y up)
      const SR::InstanceData* const sr_instance_data = IsSRActive(device_data) ? device_data.GetSRInstanceData() : nullptr;
      const int phases = sr_instance_data ? (std::max)(sr_implementations[device_data.sr_type]->GetJitterPhases(sr_instance_data), 1) : SR::GetDefaultJitterPhases();
      const unsigned int phase = cb_luma_global_settings.FrameIndex % phases;
      game_device_data.mv_jitter = (sr_instance_data || g_mv_force_jitter) && !g_mv_disable_jitter ? std::array<float, 2>{SR::HaltonSequence(phase, 2), SR::HaltonSequence(phase, 3)} : std::array<float, 2>{};
      // The viewport is the render sub-rect below ScreenPercentage 100
      const uint2 render_size = GetScreenPercentageRenderSize(device_data.output_resolution);
      game_device_data.mv_jitter_ndc = {game_device_data.mv_jitter[0] * 2.f / float(render_size.x), game_device_data.mv_jitter[1] * -2.f / float(render_size.y)};
      const float ndc_jitter[4] = {game_device_data.mv_jitter_ndc[0], game_device_data.mv_jitter_ndc[1], 0.f, 0.f};
      if (!WriteConstants(native_device, native_device_context, std::addressof(game_device_data.mv_jitter_buffer), ndc_jitter, sizeof(ndc_jitter)))
      {
         // No stale jitter on the scene draws either: no motion vectors this frame
         game_device_data.mv_jitter = {};
         game_device_data.mv_jitter_ndc = {};
         game_device_data.mv_jitter_buffer.reset();
      }
   }

#if DEVELOPMENT
#define MV_REJECT(reason) (GetGameDeviceData(device_data).mv_stats.rejected[reason]++, false)
#else
#define MV_REJECT(reason) false
#endif

   // Draws an opaque draw into the fp16 scene (the world and weapon base passes: the scene target alone, output sized, with the scene
   // depth) with the patched shaders, adding the motion vector target ("target_slot", past the game's) and the previous frame's
   // b0 / b1 / b3 ("previous_slots"). False if it can't (the draw then goes to "DrawWithJitter").
   static bool DrawWithMotionVectors(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, CommandListData& cmd_list_data, DeviceData& device_data, const ShaderHashesList<OneShaderPerPipeline>& original_shader_hashes, const std::function<void()>& draw, const com_ptr<ID3D11RenderTargetView> (&rtvs)[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT], ID3D11DepthStencilView* dsv)
   {
      auto& game_device_data = GetGameDeviceData(device_data);
      // The scene target alone, plus the motion vector target the last motion vector draw left bound
      for (UINT slot = 1; slot < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; slot++)
      {
         if (rtvs[slot] && (slot != MotionVectorPatches::target_slot || rtvs[slot] != game_device_data.mv_rtv))
            return MV_REJECT(0);
      }
      if (!rtvs[0] || !dsv || !game_device_data.mv_scene_open)
         return MV_REJECT(1);
      // Known targets: checked, and the motion vector target built for them
      if (rtvs[0].get() != game_device_data.mv_accepted_rtv || dsv != game_device_data.mv_accepted_dsv)
      {
         com_ptr<ID3D11Resource> depth;
         dsv->GetResource(&depth);
         com_ptr<ID3D11Resource> color;
         rtvs[0]->GetResource(&color);
         if (depth != game_device_data.mv_depth || !color || (game_device_data.mv_scene_color && color != game_device_data.mv_scene_color))
            return MV_REJECT(2);
         // UE3 views its single sample targets as multisampled (TEXTURE2DMS)
         D3D11_RENDER_TARGET_VIEW_DESC rtv_desc;
         rtvs[0]->GetDesc(&rtv_desc);
         com_ptr<ID3D11Texture2D> color_texture;
         D3D11_TEXTURE2D_DESC color_desc = {};
         if (SUCCEEDED(color->QueryInterface(&color_texture)))
            color_texture->GetDesc(&color_desc);
#if DEVELOPMENT
         game_device_data.mv_stats.rejected_format = rtv_desc.Format;
         game_device_data.mv_stats.rejected_dimension = rtv_desc.ViewDimension;
         game_device_data.mv_stats.rejected_width = color_desc.Width;
         game_device_data.mv_stats.rejected_height = color_desc.Height;
#endif
         if (rtv_desc.Format != DXGI_FORMAT_R16G16B16A16_FLOAT || (rtv_desc.ViewDimension != D3D11_RTV_DIMENSION_TEXTURE2D && rtv_desc.ViewDimension != D3D11_RTV_DIMENSION_TEXTURE2DMS) || color_desc.SampleDesc.Count != 1)
            return MV_REJECT(3);
         const uint2 size = {color_desc.Width, color_desc.Height};
         if (size.x != device_data.output_resolution.x || size.y != device_data.output_resolution.y)
            return MV_REJECT(4);
         game_device_data.mv_scene_color = color;
         const std::unique_lock lock(game_device_data.mv_mutex);
         D3D11_TEXTURE2D_DESC desc = {};
         if (game_device_data.mv_texture)
            game_device_data.mv_texture->GetDesc(&desc);
         if (desc.Width != size.x || desc.Height != size.y)
         {
            game_device_data.mv_texture.reset();
            game_device_data.mv_rtv.reset();
            game_device_data.mv_srv.reset();
            game_device_data.mv_uav.reset();
            // The fill reads the target back through its UAV
            D3D11_FEATURE_DATA_FORMAT_SUPPORT2 support = {DXGI_FORMAT_R32G32_FLOAT};
            const bool typed_uav_load = SUCCEEDED(native_device->CheckFeatureSupport(D3D11_FEATURE_FORMAT_SUPPORT2, &support, sizeof(support))) && (support.OutFormatSupport2 & D3D11_FORMAT_SUPPORT2_UAV_TYPED_LOAD) != 0;
            desc = {size.x, size.y, 1, 1, DXGI_FORMAT_R32G32_FLOAT, {1, 0}, D3D11_USAGE_DEFAULT, D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | (typed_uav_load ? D3D11_BIND_UNORDERED_ACCESS : 0u)};
            if (FAILED(native_device->CreateTexture2D(&desc, nullptr, &game_device_data.mv_texture)) || FAILED(native_device->CreateRenderTargetView(game_device_data.mv_texture.get(), nullptr, &game_device_data.mv_rtv)))
            {
               game_device_data.mv_texture.reset();
               game_device_data.mv_rtv.reset();
               return MV_REJECT(5);
            }
            native_device->CreateShaderResourceView(game_device_data.mv_texture.get(), nullptr, &game_device_data.mv_srv);
            if (typed_uav_load)
               native_device->CreateUnorderedAccessView(game_device_data.mv_texture.get(), nullptr, &game_device_data.mv_uav);
            game_device_data.mv_frame_ended = true;
         }
         game_device_data.mv_accepted_rtv = rtvs[0].get();
         game_device_data.mv_accepted_dsv = dsv;
      }
      com_ptr<ID3D11BlendState> blend_state;
      native_device_context->OMGetBlendState(&blend_state, nullptr, nullptr);
      if (blend_state.get() != game_device_data.mv_blend_state)
      {
         D3D11_BLEND_DESC blend_desc = CD3D11_BLEND_DESC(D3D11_DEFAULT);
         if (blend_state)
            blend_state->GetDesc(&blend_desc);
         // Additive lights, decals and translucents keep the motion vectors of what's behind them
         const D3D11_RENDER_TARGET_BLEND_DESC& rt0_blend = blend_desc.RenderTarget[0];
         game_device_data.mv_blend_opaque = !rt0_blend.BlendEnable || (rt0_blend.SrcBlend == D3D11_BLEND_ONE && rt0_blend.DestBlend == D3D11_BLEND_ZERO && rt0_blend.BlendOp == D3D11_BLEND_OP_ADD);
         game_device_data.mv_blend_state = blend_state.get();
      }
      if (!game_device_data.mv_blend_opaque)
         return MV_REJECT(6);

      ID3D11VertexShader* const vertex_shader = GetPatchedVertexShader(native_device, cmd_list_data, device_data, original_shader_hashes.vertex_shaders[0]);
      if (const uint32_t pixel_shader_hash = original_shader_hashes.pixel_shaders[0]; pixel_shader_hash != game_device_data.mv_last_pixel_shader_hash)
      {
         game_device_data.mv_last_pixel_shader = GetMotionVectorShader(native_device, device_data, &game_device_data.mv_pixel_shaders, pixel_shader_hash, cmd_list_data.pipeline_state_original_pixel_shader).get();
         game_device_data.mv_last_pixel_shader_hash = pixel_shader_hash;
      }
      ID3D11PixelShader* const pixel_shader = game_device_data.mv_last_pixel_shader;
      if (!vertex_shader || !pixel_shader || !game_device_data.mv_jitter_buffer)
         return MV_REJECT(7);
      if (std::exchange(game_device_data.mv_frame_ended, false))
      {
         // The camera fill's and the upscaler's depth: the scene depth if it can be read, else its copy (taken after the depth prepass)
         game_device_data.mv_depth_srv.reset();
         com_ptr<ID3D11Resource> depth = game_device_data.mv_depth;
         com_ptr<ID3D11Texture2D> depth_texture;
         D3D11_TEXTURE2D_DESC depth_desc = {};
         if (depth && SUCCEEDED(depth->QueryInterface(&depth_texture)))
            depth_texture->GetDesc(&depth_desc);
         if ((depth_desc.BindFlags & D3D11_BIND_SHADER_RESOURCE) == 0 && game_device_data.mv_depth_copy)
         {
            depth_texture.reset();
            depth_desc = {};
            if (SUCCEEDED(reinterpret_cast<ID3D11Resource*>(game_device_data.mv_depth_copy)->QueryInterface(&depth_texture)))
               depth_texture->GetDesc(&depth_desc);
         }
         if ((depth_desc.BindFlags & D3D11_BIND_SHADER_RESOURCE) != 0 && (depth_desc.Format == DXGI_FORMAT_R24G8_TYPELESS || depth_desc.Format == DXGI_FORMAT_R32G8X24_TYPELESS || depth_desc.Format == DXGI_FORMAT_R32_TYPELESS) && depth_desc.SampleDesc.Count == 1)
         {
            const DXGI_FORMAT srv_format = depth_desc.Format == DXGI_FORMAT_R24G8_TYPELESS ? DXGI_FORMAT_R24_UNORM_X8_TYPELESS : (depth_desc.Format == DXGI_FORMAT_R32G8X24_TYPELESS ? DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS : DXGI_FORMAT_R32_FLOAT);
            const CD3D11_SHADER_RESOURCE_VIEW_DESC srv_desc(D3D11_SRV_DIMENSION_TEXTURE2D, srv_format, 0, 1);
            native_device->CreateShaderResourceView(depth_texture.get(), &srv_desc, &game_device_data.mv_depth_srv);
         }
         game_device_data.mv_fill_pending = game_device_data.mv_depth_srv && game_device_data.mv_uav && FindShader(device_data.native_compute_shaders, CompileTimeStringHash("BL Motion Vector Fill CS")) != nullptr;
         const FLOAT clear_value = game_device_data.mv_fill_pending ? FLT_MAX : 0.f;
         const FLOAT clear[4] = {clear_value, clear_value, 0.f, 0.f};
         native_device_context->ClearRenderTargetView(game_device_data.mv_rtv.get(), clear);
         // Last frame's camera and objects are the previous ones, unless frames without a scene (menus, videos) came between
         const bool previous_valid = game_device_data.mv_camera && cb_luma_global_settings.FrameIndex - game_device_data.mv_frame_index <= 1;
         game_device_data.mv_previous_camera = previous_valid ? game_device_data.mv_camera : nullptr;
         game_device_data.mv_camera = nullptr;
         game_device_data.mv_frame_index = cb_luma_global_settings.FrameIndex;
         // Swapped, not rebuilt: the lists keep their nodes and capacity (an empty list matches nothing); keys drawn in neither of the last
         // two frames go
         game_device_data.mv_previous_objects.swap(game_device_data.mv_objects);
         std::erase_if(game_device_data.mv_objects, [](const auto& entry)
            { return entry.second.empty(); });
         for (auto& entry : game_device_data.mv_objects)
            entry.second.clear();
         if (!previous_valid)
            game_device_data.mv_previous_objects.clear();
      }

      // The game's b0 / b1 / b3. The slots added past them stay bound after the draw: no game vertex shader reads a constant buffer
      // at slot 4 or above.
      com_ptr<ID3D11Buffer> game_cbs[4];
      native_device_context->VSGetConstantBuffers(0, UINT(std::size(game_cbs)), &game_cbs[0]);
      ID3D11Buffer* const current[std::size(MotionVectorPatches::previous_slots)] = {game_cbs[MotionVectorPatches::previous_slots[0].first].get(), game_cbs[MotionVectorPatches::previous_slots[1].first].get(), game_cbs[MotionVectorPatches::previous_slots[2].first].get()};
      const bool skinned = game_device_data.mv_last_vertex_shader_skinned;
      BorderlandsGotyGameDeviceData::ConstantsCopy object, camera, bones;
      {
         const std::lock_guard lock(game_device_data.mv_constants_mutex);
         object = GetConstantsCopy(&game_device_data, current[0]);
         camera = GetConstantsCopy(&game_device_data, current[1]);
         if (skinned)
            bones = GetConstantsCopy(&game_device_data, current[2]);
      }
      const auto copy_size = [](const BorderlandsGotyGameDeviceData::ConstantsCopy& copy)
      { return copy ? copy->size() : size_t(0); };
      // The previous frame's b0 / b1 / b3: the same object's from last frame, else this draw's with last frame's world camera (no object
      // motion). None (no CPU copy yet, or an unmatched weapon draw): the current ones (zero motion).
      const std::vector<uint8_t>* uploads[std::size(MotionVectorPatches::previous_slots)] = {};
      if (object && camera && (!skinned || bones))
      {
         // The world camera: the frame's first motion vector draw's (the world base pass comes before the weapon's)
         if (!game_device_data.mv_camera)
            game_device_data.mv_camera = camera;

         // Draw key: same mesh, same shaders. Objects sharing it (props) are told apart by translation. No instance count.
         com_ptr<ID3D11Buffer> vertex_buffer;
         UINT vertex_stride = 0, vertex_offset = 0;
         native_device_context->IAGetVertexBuffers(0, 1, &vertex_buffer, &vertex_stride, &vertex_offset);
         com_ptr<ID3D11Buffer> index_buffer;
         DXGI_FORMAT index_format;
         UINT index_offset = 0;
         native_device_context->IAGetIndexBuffer(&index_buffer, &index_format, &index_offset);
         const DrawDispatchData& draw_data = last_draw_dispatch_data;
         uint64_t key = 0;
         for (const uint64_t value : {uint64_t(original_shader_hashes.vertex_shaders[0]), uint64_t(original_shader_hashes.pixel_shaders[0]), reinterpret_cast<uint64_t>(vertex_buffer.get()), uint64_t(vertex_offset), reinterpret_cast<uint64_t>(index_buffer.get()), uint64_t(index_offset), uint64_t(draw_data.index_count), uint64_t(draw_data.first_index), uint64_t(uint32_t(draw_data.vertex_offset)), uint64_t(draw_data.vertex_count), uint64_t(draw_data.first_vertex)})
            HashCombine(key, value);
         std::array<float, 3> translation = {};
         if (const uint32_t offset = game_device_data.mv_last_vertex_shader_translation; offset != UINT_MAX && offset + sizeof(translation) <= object->size())
            std::memcpy(translation.data(), object->data() + offset, sizeof(translation));

         // ponytail: linear search among the key's candidates (a handful at most); a spatial lookup if big crowds share a mesh
         const BorderlandsGotyGameDeviceData::MotionVectorObject* match = nullptr;
         if (const auto previous = game_device_data.mv_previous_objects.find(key); previous != game_device_data.mv_previous_objects.end())
         {
            float nearest = FLT_MAX;
            for (const auto& candidate : previous->second)
            {
               const float dx = candidate.translation[0] - translation[0], dy = candidate.translation[1] - translation[1], dz = candidate.translation[2] - translation[2];
               const float distance = dx * dx + dy * dy + dz * dz;
               if (copy_size(candidate.object) == object->size() && copy_size(candidate.camera) == camera->size() && copy_size(candidate.bones) == copy_size(bones) && distance < nearest)
               {
                  nearest = distance;
                  match = &candidate;
               }
            }
         }
         if (match)
         {
            // Last frame's list outlives the draw ("mv_previous_objects" only changes at the next frame start)
            uploads[0] = match->object.get();
            uploads[1] = match->camera.get();
            if (skinned)
               uploads[2] = match->bones.get();
#if DEVELOPMENT
            game_device_data.mv_stats.matched++;
#endif
         }
         else if (game_device_data.mv_previous_camera && copy_size(game_device_data.mv_previous_camera) == camera->size() && std::memcmp(camera->data(), game_device_data.mv_camera->data(), camera->size()) == 0)
         {
            // Not found, drawn with the world camera: its own constants with last frame's world camera (camera motion only)
            uploads[1] = game_device_data.mv_previous_camera.get();
#if DEVELOPMENT
            game_device_data.mv_stats.camera_only++;
#endif
         }
#if DEVELOPMENT
         else
         {
            game_device_data.mv_stats.other_camera++;
         }
#endif
         // Kept as drawn for the next frame
         game_device_data.mv_objects[key].push_back({translation, object, camera, bones});
      }
#if DEVELOPMENT
      else
      {
         game_device_data.mv_stats.uncopied++;
      }
#endif
      BindPreviousConstants(native_device, native_device_context, &game_device_data, uploads, current);
      ID3D11Buffer* const jitter = game_device_data.mv_jitter_buffer.get();
      native_device_context->VSSetConstantBuffers(MotionVectorPatches::jitter_slot, 1, &jitter);
      // Left bound after the draw (set directly, bypassing Core's state tracking): the game's next draws either bind their own
      // targets and shaders, or are motion vector draws too. The draws in between write no "o4" (no game pixel shader in the scene
      // declares a fifth target), so the motion vector target keeps its contents.
      if (rtvs[MotionVectorPatches::target_slot] != game_device_data.mv_rtv)
      {
         ID3D11RenderTargetView* targets[MotionVectorPatches::target_slot + 1] = {rtvs[0].get()};
         targets[MotionVectorPatches::target_slot] = game_device_data.mv_rtv.get();
         native_device_context->OMSetRenderTargets(MotionVectorPatches::target_slot + 1, targets, dsv);
      }
      BindPatchedShader(native_device_context, vertex_shader, &game_device_data.mv_bound_vertex_shader);
      BindPatchedShader(native_device_context, pixel_shader, &game_device_data.mv_bound_pixel_shader);

      draw();
#if DEVELOPMENT
      game_device_data.mv_stats.motion_vector_draws++;
#endif
      return true;
   }

   // Jitter for the scene's mesh draws without motion vectors (patched vertex shader, game pixel shader): the depth prepasses,
   // additive lights, decals, fog volumes, distortion and translucents. Every draw depth tested against the scene takes the same
   // jitter, or jittered and unjittered depths of the same surface fail each other's test. False if it can't (the draw runs untouched).
   static bool DrawWithJitter(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, CommandListData& cmd_list_data, DeviceData& device_data, const ShaderHashesList<OneShaderPerPipeline>& original_shader_hashes, const std::function<void()>& draw, ID3D11DepthStencilView* dsv)
   {
      auto& game_device_data = GetGameDeviceData(device_data);
      if (!game_device_data.mv_scene_open || game_device_data.mv_jitter == std::array<float, 2>{} || !game_device_data.mv_jitter_buffer)
         return false;
      // Meshes only (full screen passes have no vertex buffer or no depth test), into the scene depth (not shadows)
      if (!dsv)
         return false;
      if (dsv != game_device_data.jitter_dsv)
      {
         com_ptr<ID3D11Resource> depth;
         dsv->GetResource(&depth);
         game_device_data.jitter_dsv = dsv;
         game_device_data.jitter_dsv_scene = depth == game_device_data.mv_depth;
      }
      if (!game_device_data.jitter_dsv_scene)
         return false;
      com_ptr<ID3D11DepthStencilState> depth_stencil_state;
      native_device_context->OMGetDepthStencilState(&depth_stencil_state, nullptr);
      if (depth_stencil_state.get() != game_device_data.jitter_depth_stencil_state)
      {
         D3D11_DEPTH_STENCIL_DESC depth_desc = CD3D11_DEPTH_STENCIL_DESC(D3D11_DEFAULT);
         if (depth_stencil_state)
            depth_stencil_state->GetDesc(&depth_desc);
         game_device_data.jitter_depth_test = depth_desc.DepthEnable;
         game_device_data.jitter_depth_stencil_state = depth_stencil_state.get();
      }
      if (!game_device_data.jitter_depth_test)
         return false;
      com_ptr<ID3D11Buffer> vertex_buffer;
      UINT vertex_stride, vertex_offset;
      native_device_context->IAGetVertexBuffers(0, 1, &vertex_buffer, &vertex_stride, &vertex_offset);
      if (!vertex_buffer)
         return false;
      ID3D11VertexShader* const vertex_shader = GetPatchedVertexShader(native_device, cmd_list_data, device_data, original_shader_hashes.vertex_shaders[0]);
      if (!vertex_shader)
         return false;

      // The patched vertex shader and the jitter stay bound after the draw (see "DrawWithMotionVectors"), with the game's pixel shader
      // (a motion vector draw's is put back)
      BindPatchedShader(native_device_context, vertex_shader, &game_device_data.mv_bound_vertex_shader);
      ID3D11Buffer* const jitter = game_device_data.mv_jitter_buffer.get();
      native_device_context->VSSetConstantBuffers(MotionVectorPatches::jitter_slot, 1, &jitter);
      RestoreGameShader(native_device_context, &game_device_data.mv_bound_pixel_shader);
      draw();
#if DEVELOPMENT
      game_device_data.mv_stats.jitter_draws++;
#endif
      return true;
   }

   // DLSS / FSR on the jittered scene (its last copy, which the DOF/Bloom gather and the uber post read, else the scene itself), the
   // scene depth and the motion vectors, from the render sub-rect (ScreenPercentage) to the output. At native resolution the result goes
   // back into both; below it, the uber and the output resolution gather read the output texture, and it goes back into the scene
   // copy's sub-rect downscaled. False if it didn't draw (missing input, or the upscaler failed).
   static bool DrawUpscaler(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, DeviceData& device_data)
   {
      auto& game_device_data = GetGameDeviceData(device_data);
      if (!game_device_data.mv_texture || !game_device_data.mv_camera || game_device_data.mv_camera->size() < 16 * sizeof(float) || !game_device_data.mv_depth_srv || !game_device_data.mv_scene_color)
         return false;
      ID3D11Resource* const scene_resource = game_device_data.mv_scene_color_copy ? reinterpret_cast<ID3D11Resource*>(game_device_data.mv_scene_color_copy) : game_device_data.mv_scene_color.get();
      com_ptr<ID3D11Texture2D> scene;
      if (FAILED(scene_resource->QueryInterface(&scene)))
         return false;
      D3D11_TEXTURE2D_DESC scene_desc, mv_desc;
      scene->GetDesc(&scene_desc);
      game_device_data.mv_texture->GetDesc(&mv_desc);
      if ((scene_desc.Format != DXGI_FORMAT_R16G16B16A16_FLOAT && scene_desc.Format != DXGI_FORMAT_R16G16B16A16_TYPELESS) || scene_desc.SampleDesc.Count != 1 || (scene_desc.BindFlags & D3D11_BIND_SHADER_RESOURCE) == 0 || scene_desc.Width != mv_desc.Width || scene_desc.Height != mv_desc.Height)
         return false;
      // None when "Super Resolution" changed after present (see "IsSRActive"): no output texture is made for it
      SR::InstanceData* const sr_instance_data = device_data.GetSRInstanceData();
      if (!sr_instance_data)
         return false;
      com_ptr<ID3D11Resource> depth;
      game_device_data.mv_depth_srv->GetResource(&depth);

      D3D11_TEXTURE2D_DESC output_desc = {};
      if (device_data.sr_output_color)
         device_data.sr_output_color->GetDesc(&output_desc);
      if (output_desc.Width != scene_desc.Width || output_desc.Height != scene_desc.Height)
      {
         device_data.sr_output_color.reset();
         output_desc = {scene_desc.Width, scene_desc.Height, 1, 1, DXGI_FORMAT_R16G16B16A16_FLOAT, {1, 0}, D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS};
         native_device->CreateTexture2D(&output_desc, nullptr, &device_data.sr_output_color);
         game_device_data.sr_output_recreated = true;
      }
      if (!device_data.sr_output_color)
         return false;

      // FSR needs the camera (DLSS ignores it). b1's ViewProjectionMatrix multiplies row vectors with absolute world translation: column
      // 1 is the up axis / tan(fov / 2); UE3's projection has an infinite far plane with clip w - clip z = the near plane (element 3,3
      // minus 3,2).
      const float* const view_projection = reinterpret_cast<const float*>(game_device_data.mv_camera->data());
      const double up_length = std::sqrt(double(view_projection[1]) * view_projection[1] + double(view_projection[5]) * view_projection[5] + double(view_projection[9]) * view_projection[9]);
      const double vert_fov = up_length > 0.0 ? 2.0 * std::atan(1.0 / up_length) : 0.0;
      const double near_plane = double(view_projection[15]) - double(view_projection[14]);

      const uint2 render_size = GetScreenPercentageRenderSize(device_data.output_resolution);
#if DEVELOPMENT
      // "SR Reversible Tonemap": the upscaler reads the scene's sub-rect through c / (1 + max(c)), its output is undone after it
      auto* const untonemap_shader = g_sr_reversible_tonemap ? FindShader(device_data.native_compute_shaders, CompileTimeStringHash("BL SR Untonemap CS")) : nullptr;
      auto* const tonemap_shader = untonemap_shader ? FindShader(device_data.native_compute_shaders, CompileTimeStringHash("BL SR Tonemap CS")) : nullptr;
      bool tonemapped = false;
      if (tonemap_shader)
      {
         D3D11_TEXTURE2D_DESC input_desc = {};
         if (game_device_data.sr_tonemap_input)
            game_device_data.sr_tonemap_input->GetDesc(&input_desc);
         if (input_desc.Width != scene_desc.Width || input_desc.Height != scene_desc.Height)
         {
            game_device_data.sr_tonemap_input.reset();
            input_desc = {scene_desc.Width, scene_desc.Height, 1, 1, DXGI_FORMAT_R16G16B16A16_FLOAT, {1, 0}, D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS};
            native_device->CreateTexture2D(&input_desc, nullptr, &game_device_data.sr_tonemap_input);
         }
         com_ptr<ID3D11ShaderResourceView> scene_srv;
         com_ptr<ID3D11UnorderedAccessView> input_uav;
         const CD3D11_SHADER_RESOURCE_VIEW_DESC scene_srv_desc(D3D11_SRV_DIMENSION_TEXTURE2D, DXGI_FORMAT_R16G16B16A16_FLOAT, 0, 1);
         if (game_device_data.sr_tonemap_input && SUCCEEDED(native_device->CreateShaderResourceView(scene.get(), &scene_srv_desc, &scene_srv)) && SUCCEEDED(native_device->CreateUnorderedAccessView(game_device_data.sr_tonemap_input.get(), nullptr, &input_uav)))
         {
            // The scene may still be bound as a render target (EndScene restores the state)
            native_device_context->OMSetRenderTargets(0, nullptr, nullptr);
            ID3D11ShaderResourceView* const srv = scene_srv.get();
            ID3D11UnorderedAccessView* const uav = input_uav.get();
            native_device_context->CSSetShaderResources(0, 1, &srv);
            native_device_context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);
            native_device_context->CSSetShader(tonemap_shader, nullptr, 0);
            native_device_context->Dispatch((render_size.x + 7) / 8, (render_size.y + 7) / 8, 1);
            ID3D11ShaderResourceView* const null_srv = nullptr;
            ID3D11UnorderedAccessView* const null_uav = nullptr;
            native_device_context->CSSetShaderResources(0, 1, &null_srv);
            native_device_context->CSSetUnorderedAccessViews(0, 1, &null_uav, nullptr);
            tonemapped = true;
         }
      }
#else
      constexpr bool tonemapped = false;
#endif
      SR::SettingsData settings_data;
      settings_data.output_width = scene_desc.Width;
      settings_data.output_height = scene_desc.Height;
      settings_data.render_width = render_size.x;
      settings_data.render_height = render_size.y;
      settings_data.dynamic_resolution = false;
      settings_data.hdr = !tonemapped;
      settings_data.inverted_depth = false;
      settings_data.mvs_jittered = false;
      // The motion vectors are UV deltas of the scene's viewport (the render sub-rect), previous minus current
      settings_data.mvs_x_scale = float(render_size.x);
      settings_data.mvs_y_scale = float(render_size.y);
      settings_data.auto_exposure = !tonemapped;
      settings_data.render_preset = dlss_render_preset;
      sr_implementations[device_data.sr_type]->UpdateSettings(sr_instance_data, native_device_context, settings_data);

      SR::SuperResolutionImpl::DrawData draw_data;
#if DEVELOPMENT
      draw_data.source_color = tonemapped ? game_device_data.sr_tonemap_input.get() : scene.get();
#else
      draw_data.source_color = scene.get();
#endif
      draw_data.output_color = device_data.sr_output_color.get();
      draw_data.motion_vectors = game_device_data.mv_texture.get();
      draw_data.depth_buffer = depth.get();
      draw_data.render_width = render_size.x;
      draw_data.render_height = render_size.y;
      // As applied (pixels, +y down)
      draw_data.jitter_x = game_device_data.mv_jitter[0];
      draw_data.jitter_y = game_device_data.mv_jitter[1];
      draw_data.reset = device_data.force_reset_sr;
      if (vert_fov > 0.0)
         draw_data.vert_fov = float(vert_fov);
      if (near_plane > 0.0)
      {
         // ponytail: FSR gets a large finite far for UE3's infinite one (Core sets FFX_FSR3_ENABLE_DEPTH_INFINITE only with inverted depth)
         draw_data.near_plane = float(near_plane);
         draw_data.far_plane = float(near_plane * 1e6);
      }
      if (!sr_implementations[device_data.sr_type]->Draw(sr_instance_data, native_device_context, draw_data))
      {
         // Back to SMAA until the upscaler is picked again
         device_data.sr_suppressed = true;
         return false;
      }
#if DEVELOPMENT
      if (com_ptr<ID3D11UnorderedAccessView> output_uav; tonemapped && SUCCEEDED(native_device->CreateUnorderedAccessView(device_data.sr_output_color.get(), nullptr, &output_uav)))
      {
         ID3D11UnorderedAccessView* uav = output_uav.get();
         native_device_context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);
         native_device_context->CSSetShader(untonemap_shader, nullptr, 0);
         native_device_context->Dispatch((scene_desc.Width + 7) / 8, (scene_desc.Height + 7) / 8, 1);
         uav = nullptr;
         native_device_context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);
      }
#endif
      if (render_size.x == scene_desc.Width && render_size.y == scene_desc.Height)
      {
         native_device_context->CopySubresourceRegion(scene.get(), 0, 0, 0, 0, device_data.sr_output_color.get(), 0, nullptr);
         if (scene_resource != game_device_data.mv_scene_color.get())
            native_device_context->CopySubresourceRegion(game_device_data.mv_scene_color.get(), 0, 0, 0, 0, device_data.sr_output_color.get(), 0, nullptr);
      }
      else
      {
         // Below native the game's DOF/Bloom gather, when its output resolution replacement can't run, reads the render sub-rect of the
         // scene copy: the output goes back into it, downscaled (the jittered sub-rect made the bloom flicker around thin geometry)
         if (game_device_data.sp_upscaled_texture != device_data.sr_output_color)
         {
            game_device_data.sp_upscaled_texture = device_data.sr_output_color;
            game_device_data.sp_upscaled_srv.reset();
            native_device->CreateShaderResourceView(game_device_data.sp_upscaled_texture.get(), nullptr, &game_device_data.sp_upscaled_srv);
         }
         if (game_device_data.sp_downscale_target != scene)
         {
            game_device_data.sp_downscale_target = scene;
            game_device_data.sp_downscale_rtv.reset();
            const CD3D11_RENDER_TARGET_VIEW_DESC rtv_desc(D3D11_RTV_DIMENSION_TEXTURE2D, DXGI_FORMAT_R16G16B16A16_FLOAT);
            if (scene_desc.BindFlags & D3D11_BIND_RENDER_TARGET)
               native_device->CreateRenderTargetView(scene.get(), &rtv_desc, &game_device_data.sp_downscale_rtv);
         }
         // Core's bilinear scaling copy (a 2x2 box at 50%)
         auto* const scale_vs = FindShader(device_data.native_vertex_shaders, CompileTimeStringHash("Scale VS"));
         auto* const scale_ps = FindShader(device_data.native_pixel_shaders, CompileTimeStringHash("Scale PS"));
         if (scale_vs && scale_ps && game_device_data.sp_upscaled_srv && game_device_data.sp_downscale_rtv)
         {
            // The caller (EndScene) restores the state. The upscaler may leave its output bound as a compute UAV, which would null its SRV.
            ID3D11UnorderedAccessView* const null_uavs[D3D11_1_UAV_SLOT_COUNT] = {};
            native_device_context->CSSetUnorderedAccessViews(0, device_data.uav_max_count, null_uavs, nullptr);
            DrawCustomPixelShader(native_device_context, device_data.default_depth_stencil_state.get(), device_data.default_blend_state.get(), device_data.sampler_state_linear.get(), scale_vs, scale_ps, game_device_data.sp_upscaled_srv.get(), game_device_data.sp_downscale_rtv.get(), render_size.x, render_size.y);
            // Unbound now: EndScene restores the compute state first, and the gather's SRV of the copy would be nulled while it's a target
            native_device_context->OMSetRenderTargets(0, nullptr, nullptr);
         }
      }
      // DLSS draws nothing into a new output texture (the session's first, or one made after "None", which Core frees): the frame shows
      // the texture's stale memory until its feature is created again after a draw. Settings changed once here force that at the next
      // frame's "UpdateSettings".
      if (std::exchange(game_device_data.sr_output_recreated, false) && device_data.sr_type == SR::Type::DLSS)
      {
         SR::SettingsData throwaway_settings_data = settings_data;
         throwaway_settings_data.mvs_jittered = !throwaway_settings_data.mvs_jittered;
         sr_implementations[device_data.sr_type]->UpdateSettings(sr_instance_data, native_device_context, throwaway_settings_data);
      }
      device_data.has_drawn_sr = true;
      device_data.render_resolution = {float(render_size.x), float(render_size.y)};
      return true;
   }

   // Ends the scene at its first post pass (immediate context): the camera motion fill for the pixels no motion vector draw wrote, then
   // DLSS / FSR ("DrawUpscaler"), both before that pass reads the scene
   static void EndScene(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, DeviceData& device_data)
   {
      auto& game_device_data = GetGameDeviceData(device_data);
      game_device_data.mv_scene_open = false;
      game_device_data.mv_scene_done = true;
      game_device_data.mv_frame_ended = true;
      DrawStateStack<DrawStateStackType::FullGraphics> graphics_state;
      DrawStateStack<DrawStateStackType::Compute> compute_state;
      graphics_state.Cache(native_device_context, device_data.uav_max_count);
      compute_state.Cache(native_device_context, device_data.uav_max_count);
      auto* const fill_shader = FindShader(device_data.native_compute_shaders, CompileTimeStringHash("BL Motion Vector Fill CS"));
      if (std::exchange(game_device_data.mv_fill_pending, false) && fill_shader)
      {
         // Current clip space to the previous frame's: previous * inverse(current) for column vectors (b1 holds the row vector
         // matrix, transposed here), in double (absolute world translation)
         Math::Matrix44D current, previous;
         current.SetIdentity();
         previous.SetIdentity();
         if (game_device_data.mv_camera && game_device_data.mv_previous_camera && game_device_data.mv_camera->size() >= 16 * sizeof(float) && game_device_data.mv_previous_camera->size() >= 16 * sizeof(float))
         {
            std::copy_n(reinterpret_cast<const float*>(game_device_data.mv_camera->data()), 16, current.GetData());
            std::copy_n(reinterpret_cast<const float*>(game_device_data.mv_previous_camera->data()), 16, previous.GetData());
            current.Transpose();
            previous.Transpose();
            current.Invert();
         }
         const Math::Matrix44D reprojection = previous * current;
         const uint2 render_size = GetScreenPercentageRenderSize(device_data.output_resolution);
         float constants[20] = {};
         for (int i = 0; i < 16; i++)
            constants[i] = float(reprojection.GetData()[i]);
         constants[16] = game_device_data.mv_jitter_ndc[0];
         constants[17] = game_device_data.mv_jitter_ndc[1];
         constants[18] = float(render_size.x);
         constants[19] = float(render_size.y);
         if (WriteConstants(native_device, native_device_context, std::addressof(game_device_data.mv_fill_buffer), constants, sizeof(constants)))
         {
            // The depth may be bound as the depth target, and the motion vectors as a render target
            native_device_context->OMSetRenderTargets(0, nullptr, nullptr);
            ID3D11Buffer* const buffer = game_device_data.mv_fill_buffer.get();
            ID3D11ShaderResourceView* const srv = game_device_data.mv_depth_srv.get();
            ID3D11UnorderedAccessView* const uav = game_device_data.mv_uav.get();
            native_device_context->CSSetConstantBuffers(0, 1, &buffer);
            native_device_context->CSSetShaderResources(0, 1, &srv);
            native_device_context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);
            native_device_context->CSSetShader(fill_shader, nullptr, 0);
            native_device_context->Dispatch((render_size.x + 7) / 8, (render_size.y + 7) / 8, 1);
            ID3D11UnorderedAccessView* const null_uav = nullptr;
            native_device_context->CSSetUnorderedAccessViews(0, 1, &null_uav, nullptr);
         }
      }
      if (IsSRActive(device_data))
      {
         const bool drawn = DrawUpscaler(native_device, native_device_context, device_data);
#if DEVELOPMENT
         game_device_data.mv_stats.sr_draws += drawn;
#else
         (void)drawn;
#endif
      }
      compute_state.Restore(native_device_context);
      graphics_state.Restore(native_device_context);
   }

   // Every blend state writes the motion vector target ("MotionVectorPatches::target_slot", bound only by the motion vector draws)
   // unblended: no per-draw copy of the game's state. ReShade turns independent blending on only when a target now differs.
   static bool OnCreateBlendState(reshade::api::device* device, reshade::api::pipeline_layout layout, uint32_t subobject_count, const reshade::api::pipeline_subobject* subobjects)
   {
      for (uint32_t i = 0; i < subobject_count; i++)
      {
         if (subobjects[i].type != reshade::api::pipeline_subobject_type::blend_state)
            continue;
         auto& desc = *static_cast<reshade::api::blend_desc*>(subobjects[i].data);
         desc.blend_enable[MotionVectorPatches::target_slot] = false;
         desc.render_target_write_mask[MotionVectorPatches::target_slot] = 0xF;
         return true;
      }
      return false;
   }

public:
   void OnInit(bool async) override
   {
      g_engine_screen_percentage = ScaleScreenCoordsPatch::GetScreenPercentage();
      if (!g_engine_screen_percentage)
         reshade::log::message(reshade::log::level::warning, "[BL] ScaleScreenCoords: unexpected bytes, no render scale");
      ScaleScreenCoordsPatch::Apply(g_sp_unscaled_render_target_views, g_sp_unscaled_other_calls);

      // Game-specific defines: TONEMAP_TYPE drives Luma_BL_Tonemap.hlsl, XE_GTAO_QUALITY drives Luma_BL_XeGTAO.hlsl.
      std::vector<ShaderDefineData> game_shader_defines_data = {
         {"TONEMAP_TYPE", '1', true, false, "0 - SDR: Vanilla clamped reference\n1 - HDR: Extended UE3 grade + DICE"},
         {"XE_GTAO_QUALITY", '3', true, false, "Ambient Occlusion (XeGTAO) quality (slice count)\n0 - Low\n1 - Medium\n2 - High\n3 - Very High\n4 - Ultra", 4},
      };
      shader_defines_data.append_range(game_shader_defines_data);

      // Post-process buffers in GAMMA space (UE3 engine, gamma-2.2 SDR): the gamma-SDR HUD blends on top in
      // gamma to look vanilla (linear space washes it out). Tonemap pre-scales by GamePaperWhite/UIPaperWhite
      // (UI_DRAW_TYPE 2); the core composition decodes gamma + applies paper white + scRGB encode.
      GetShaderDefineData(POST_PROCESS_SPACE_TYPE_HASH).SetDefaultValue('0');
      GetShaderDefineData(EARLY_DISPLAY_ENCODING_HASH).SetDefaultValue('0');
      GetShaderDefineData(VANILLA_ENCODING_TYPE_HASH).SetDefaultValue('1'); // game shipped gamma-2.2 SDR
      GetShaderDefineData(GAMMA_CORRECTION_TYPE_HASH).SetDefaultValue('1');
      GetShaderDefineData(GAMUT_MAPPING_TYPE_HASH).SetDefaultValue('1'); // gamut-map wild colors in composition
      GetShaderDefineData(UI_DRAW_TYPE_HASH).SetDefaultValue('2');       // HUD gets its own UIPaperWhite + gamma blend

      // Linear-light decode of the SMAA input for its neighborhood blend.
      native_shaders_definitions.emplace(CompileTimeStringHash("BL SMAA Linearize CS"),
         ShaderDefinition("Luma_BL_SMAALinearize", reshade::api::pipeline_subobject_type::compute_shader));
      // Depth-extract CS for SMAA predication: hardware d24 -> R16F plane-deviation edge-ness.
      native_shaders_definitions.emplace(CompileTimeStringHash("BL Depth Extract CS"),
         ShaderDefinition("Luma_BL_DepthExtract", reshade::api::pipeline_subobject_type::compute_shader));
      // RCAS sharpen PS (drawn via core "Copy VS" + DrawCustomPixelShader after SMAA).
      native_shaders_definitions.emplace(CompileTimeStringHash("BL Sharpen PS"),
         ShaderDefinition{"Luma_BL_Sharpen", reshade::api::pipeline_subobject_type::pixel_shader, nullptr, "sharpen_ps"});

      // XeGTAO (replaces the game's native HBAO+; see the AO hash block above). 4 compute passes out of one
      // file; the two denoise variants differ only by XE_GTAO_FINAL_APPLY (the final one writes the game's
      // r16g16_float AO target).
      native_shaders_definitions.emplace(CompileTimeStringHash("BL XeGTAO Prefilter Depths CS"),
         ShaderDefinition{"Luma_BL_XeGTAO", reshade::api::pipeline_subobject_type::compute_shader, nullptr, "prefilter_depths16x16_cs"});
      native_shaders_definitions.emplace(CompileTimeStringHash("BL XeGTAO Main Pass CS"),
         ShaderDefinition{"Luma_BL_XeGTAO", reshade::api::pipeline_subobject_type::compute_shader, nullptr, "main_pass_cs"});
      native_shaders_definitions.emplace(CompileTimeStringHash("BL XeGTAO Denoise Pass 1 CS"),
         ShaderDefinition{"Luma_BL_XeGTAO", reshade::api::pipeline_subobject_type::compute_shader, nullptr, "denoise_pass_cs", {{"XE_GTAO_FINAL_APPLY", "0"}}});
      native_shaders_definitions.emplace(CompileTimeStringHash("BL XeGTAO Denoise Pass 2 CS"),
         ShaderDefinition{"Luma_BL_XeGTAO", reshade::api::pipeline_subobject_type::compute_shader, nullptr, "denoise_pass_cs", {{"XE_GTAO_FINAL_APPLY", "1"}}});

      // DLSS / FSR: camera motion for the motion vector pixels no patched draw wrote (sky, unpatched draws), the DOF/Bloom gather and its
      // blurs at output resolution with the DOF history, the CPU copies of the per-draw constants, and the motion vector target written
      // by every blend state
      native_shaders_definitions.emplace(CompileTimeStringHash("BL Motion Vector Fill CS"),
         ShaderDefinition("Luma_BL_MotionVectorFill", reshade::api::pipeline_subobject_type::compute_shader));
      native_shaders_definitions.emplace(CompileTimeStringHash("BL DOF History CS"),
         ShaderDefinition("Luma_BL_DOFHistory", reshade::api::pipeline_subobject_type::compute_shader));
      native_shaders_definitions.emplace(CompileTimeStringHash("BL Output Gather CS"),
         ShaderDefinition{"Luma_BL_OutputGather", reshade::api::pipeline_subobject_type::compute_shader, nullptr, "gather_cs"});
      // Keyed by the game's vertex shader hash, one per sample count
      for (const auto& [hash, samples] : kFilterVertexShaders)
         native_shaders_definitions.emplace(hash, ShaderDefinition{"Luma_BL_OutputBlur", reshade::api::pipeline_subobject_type::vertex_shader, nullptr, "blur_vs", {{"SAMPLES", samples}}});
#if DEVELOPMENT
      native_shaders_definitions.emplace(CompileTimeStringHash("BL SR Tonemap CS"),
         ShaderDefinition{"Luma_BL_SRTonemap", reshade::api::pipeline_subobject_type::compute_shader, nullptr, "tonemap_cs"});
      native_shaders_definitions.emplace(CompileTimeStringHash("BL SR Untonemap CS"),
         ShaderDefinition{"Luma_BL_SRTonemap", reshade::api::pipeline_subobject_type::compute_shader, nullptr, "untonemap_cs"});
#endif
      reshade::register_event<reshade::addon_event::update_buffer_region>(OnUpdateBufferRegion);
      reshade::register_event<reshade::addon_event::create_pipeline>(OnCreateBlendState);

      // Game uses CB slots b0-b3, so b11 (XeGTAO knobs) and b12/b13 are free for Luma.
      // luma_data is used by the Display Composition; luma_ui stays off (UI drawn by the game).
      luma_settings_cbuffer_index = 13;
      luma_data_cbuffer_index = 12;
      luma_ui_cbuffer_index = -1;

      // Manual Scene + UI Paper White sliders instead of the OS HDR reference level. Core gates the separate
      // "UI Paper White" slider on UI_DRAW_TYPE >= 1 && !use_os_reference_white_level. Default 203 nits (BT.2408).
      use_os_reference_white_level = false;

      // User grade controls (read in Luma_BL_Tonemap.hlsl via LumaSettings.GameSettings). All vanilla by default.
      default_luma_global_game_settings.Exposure = 1.f; // multiplier (1x)
      default_luma_global_game_settings.Saturation = 1.f;
      default_luma_global_game_settings.HighlightDechroma = 0.f; // off by default; only the mandatory DICE/gamut desaturation applies. Slider = optional taste.
      default_luma_global_game_settings.BloomIntensity = 1.f;
      default_luma_global_game_settings.Contrast = 1.f;
      default_luma_global_game_settings.Dithering = 1.f;          // subtle anti-banding on by default
      default_luma_global_game_settings.FlareOut = 1.f;           // additive lens-flare/glare scale (1 = vanilla)
      default_luma_global_game_settings.VideoAutoHDREnable = 1.f; // light AutoHDR on Bink movies, HDR only (on by default)
      default_luma_global_game_settings.VideoAutoHDRBoost = 0.5f; // highlight-expansion strength (peak ~165 nits at 0.5)
      cb_luma_global_settings.GameSettings = default_luma_global_game_settings;
   }

   void OnCreateDevice(ID3D11Device* native_device, DeviceData& device_data) override
   {
      device_data.game = new BorderlandsGotyGameDeviceData;
      device_data.taa_detected = true; // No TAA to replace, but Core's upscaler UI checks for it
   }

   void OnDestroyDeviceData(DeviceData& device_data) override
   {
      // GameDeviceData lacks a virtual destructor; delete through the concrete type to release derived members.
      delete static_cast<BorderlandsGotyGameDeviceData*>(device_data.game);
      device_data.game = nullptr;
   }

   DrawOrDispatchOverrideType OnDrawOrDispatch(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, CommandListData& cmd_list_data, DeviceData& device_data, reshade::api::shader_stage stages, const ShaderHashesList<OneShaderPerPipeline>& original_shader_hashes, bool is_custom_pass, bool& updated_cbuffers, std::function<void()>* original_draw_dispatch_func) override
   {
      auto& gd = GetGameDeviceData(device_data);

      // Every pass handled here draws on the immediate context
      if (native_device_context->GetType() != D3D11_DEVICE_CONTEXT_IMMEDIATE)
         return DrawOrDispatchOverrideType::None;
      const bool uber = original_shader_hashes.Contains(kUberPostHashes[0], reshade::api::shader_stage::pixel) || original_shader_hashes.Contains(kUberPostHashes[1], reshade::api::shader_stage::pixel);
      const bool gather = original_shader_hashes.Contains(kDOFBloomGatherHash, reshade::api::shader_stage::compute);

      // ScreenPercentage < 100: the uber writes the game's R8G8B8A8 intermediate, not the swapchain (the stretch "0xB431B8F7" then
      // draws it onto the swapchain). It writes an fp16 texture of ours instead, which the stretch reads. At 100 the uber writes the
      // swapchain, or the target of an effect that follows it in another post chain (cutscenes, near death, scope): left alone.
      if (uber)
      {
         // The scene ends here when no gather ran: DLSS / FSR first, so the uber knows whether it reads the upscaled scene
         if (gd.mv_active && gd.mv_scene_open)
            EndScene(native_device, native_device_context, device_data);
         com_ptr<ID3D11RenderTargetView> rtvs[2];
         com_ptr<ID3D11DepthStencilView> dsv;
         native_device_context->OMGetRenderTargets(UINT(std::size(rtvs)), &rtvs[0], &dsv);
         com_ptr<ID3D11Resource> target;
         if (rtvs[0])
            rtvs[0]->GetResource(&target);
         const bool swapchain_target = !target || IsBackBuffer(device_data, target.get());
         com_ptr<ID3D11Texture2D> target_texture;
         if (g_screen_percentage < 1.f && !swapchain_target && SUCCEEDED(target->QueryInterface(&target_texture)))
         {
            D3D11_TEXTURE2D_DESC desc;
            target_texture->GetDesc(&desc);
            D3D11_TEXTURE2D_DESC own_desc = {};
            if (gd.sp_intermediate)
               gd.sp_intermediate->GetDesc(&own_desc);
            if (own_desc.Width != desc.Width || own_desc.Height != desc.Height)
            {
               gd.sp_intermediate_rtv.reset();
               gd.sp_intermediate_srv.reset();
               gd.sp_intermediate.reset();
               own_desc = {desc.Width, desc.Height, 1, 1, DXGI_FORMAT_R16G16B16A16_FLOAT, {1, 0}, D3D11_USAGE_DEFAULT, D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE};
               if (FAILED(native_device->CreateTexture2D(&own_desc, nullptr, &gd.sp_intermediate)) || FAILED(native_device->CreateRenderTargetView(gd.sp_intermediate.get(), nullptr, &gd.sp_intermediate_rtv)) || FAILED(native_device->CreateShaderResourceView(gd.sp_intermediate.get(), nullptr, &gd.sp_intermediate_srv)))
               {
                  gd.sp_intermediate_rtv.reset();
                  gd.sp_intermediate_srv.reset();
                  gd.sp_intermediate.reset();
               }
            }
            if (gd.sp_intermediate_rtv)
            {
               ID3D11RenderTargetView* const targets[2] = {gd.sp_intermediate_rtv.get(), rtvs[1].get()};
               native_device_context->OMSetRenderTargets(UINT(std::size(targets)), targets, dsv.get());
            }
         }
         // DLSS / FSR made the scene output sized: the uber reads it and covers its whole target (its quad covers the render sub-rect: the
         // viewport scales up by the inverse share from the top left)
         if (!swapchain_target && g_screen_percentage < 1.f && device_data.has_drawn_sr && device_data.sr_output_color && original_draw_dispatch_func && *original_draw_dispatch_func)
         {
            D3D11_VIEWPORT viewport;
            UINT viewport_count = 1;
            native_device_context->RSGetViewports(&viewport_count, &viewport);
            // Made for this output texture in "DrawUpscaler"
            if (gd.sp_upscaled_srv && gd.sp_upscaled_texture == device_data.sr_output_color && viewport_count == 1)
            {
               com_ptr<ID3D11ShaderResourceView> game_srv;
               native_device_context->PSGetShaderResources(0, 1, &game_srv);
               ID3D11ShaderResourceView* const upscaled_srv = gd.sp_upscaled_srv.get();
               native_device_context->PSSetShaderResources(0, 1, &upscaled_srv);
               const D3D11_VIEWPORT game_viewport = viewport;
               const uint2 render_size = GetScreenPercentageRenderSize(device_data.output_resolution);
               viewport.Width *= device_data.output_resolution.x / float(render_size.x);
               viewport.Height *= device_data.output_resolution.y / float(render_size.y);
               native_device_context->RSSetViewports(1, &viewport);
               SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, stages, LumaConstantBufferType::LumaSettings);
               SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, stages, LumaConstantBufferType::LumaData);
               updated_cbuffers = true;
               (*original_draw_dispatch_func)();
               native_device_context->RSSetViewports(1, &game_viewport);
               ID3D11ShaderResourceView* const game_srv_raw = game_srv.get();
               native_device_context->PSSetShaderResources(0, 1, &game_srv_raw);
               return DrawOrDispatchOverrideType::Replaced;
            }
         }
      }
      else if (g_screen_percentage < 1.f && original_shader_hashes.Contains(0xB431B8F7, reshade::api::shader_stage::pixel))
      {
         ID3D11ShaderResourceView* srv = gd.sp_intermediate_srv.get();
         // RCAS on the upscaled image runs here: the FXAA resolve, where it runs at 100%, writes the swapchain before this stretch
         // overwrites it
         auto* const sharpen_vs = FindShader(device_data.native_vertex_shaders, CompileTimeStringHash("Copy VS"));
         auto* const sharpen_ps = FindShader(device_data.native_pixel_shaders, CompileTimeStringHash("BL Sharpen PS"));
         if (srv && device_data.has_drawn_sr && g_rcas_sharpness > 0.f && sharpen_vs && sharpen_ps)
         {
            D3D11_TEXTURE2D_DESC desc, sharpened_desc = {};
            gd.sp_intermediate->GetDesc(&desc);
            if (gd.sp_sharpened)
               gd.sp_sharpened->GetDesc(&sharpened_desc);
            if (sharpened_desc.Width != desc.Width || sharpened_desc.Height != desc.Height)
            {
               gd.sp_sharpened_rtv.reset();
               gd.sp_sharpened_srv.reset();
               if (CreateDefaultTex(native_device, desc.Width, desc.Height, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET, gd.sp_sharpened))
               {
                  native_device->CreateRenderTargetView(gd.sp_sharpened.get(), nullptr, gd.sp_sharpened_rtv.put());
                  native_device->CreateShaderResourceView(gd.sp_sharpened.get(), nullptr, gd.sp_sharpened_srv.put());
               }
            }
            const float sharpen_constants[4] = {float(desc.Width), float(desc.Height), g_rcas_sharpness, 0.f};
            if (gd.sp_sharpened_rtv && gd.sp_sharpened_srv && WriteConstants(native_device, native_device_context, std::addressof(gd.sp_sharpen_cb), sharpen_constants, sizeof(sharpen_constants)))
            {
               DrawStateStack<DrawStateStackType::FullGraphics> sharpen_state;
               sharpen_state.Cache(native_device_context, device_data.uav_max_count);
               ID3D11Buffer* const cb = gd.sp_sharpen_cb.get();
               native_device_context->PSSetConstantBuffers(0, 1, &cb);
               DrawCustomPixelShader(native_device_context, device_data.default_depth_stencil_state.get(), device_data.default_blend_state.get(), nullptr, sharpen_vs, sharpen_ps, srv, gd.sp_sharpened_rtv.get(), desc.Width, desc.Height, false);
               sharpen_state.Restore(native_device_context);
               srv = gd.sp_sharpened_srv.get();
            }
         }
         if (srv)
            native_device_context->PSSetShaderResources(0, 1, &srv);
         gd.sp_stretched = true;
      }
      // ScreenPercentage < 100, the UE3 Canvas (simple element: texture x vertex colour) after the stretch. An opaque tile (blend off, seen
      // with 0x8C32A0F6, purpose unknown) covers the stretch: skipped. The HUD offset applies to every simple element permutation in the
      // shader cache.
      else if (gd.sp_stretched && (original_shader_hashes.Contains(0x8C32A0F6, reshade::api::shader_stage::pixel) || original_shader_hashes.Contains(0x26475125, reshade::api::shader_stage::pixel) || original_shader_hashes.Contains(0x75F95219, reshade::api::shader_stage::pixel)))
      {
         com_ptr<ID3D11BlendState> blend_state;
         native_device_context->OMGetBlendState(&blend_state, nullptr, nullptr);
         D3D11_BLEND_DESC blend_desc = CD3D11_BLEND_DESC(D3D11_DEFAULT);
         if (blend_state)
            blend_state->GetDesc(&blend_desc);
         if (!blend_desc.RenderTarget[0].BlendEnable)
            return original_shader_hashes.Contains(0x8C32A0F6, reshade::api::shader_stage::pixel) ? DrawOrDispatchOverrideType::Skip : DrawOrDispatchOverrideType::None;
         // Canvas elements with vertices in pixels (the mouse cursor) have the plain pixel to clip "Transform" of the viewport. The HUD's
         // (the crosshair) also carries the player's canvas origin: stock UE3's ScaleScreenCoords centres the shrunk view, (output - view)
         // / 2 in integers (634, 356 at 4K 67%), but this build renders it at the top left, so the HUD lands down right by that much, and
         // bigger by output / view around the screen centre. Its viewport moves back by the origin and shrinks around the centre by the
         // share. A buffer's transform is unknown until its first update after its first Canvas draw.
         D3D11_VIEWPORT viewport;
         UINT viewport_count = 1;
         native_device_context->RSGetViewports(&viewport_count, &viewport);
         com_ptr<ID3D11Buffer> constant_buffer;
         native_device_context->VSGetConstantBuffers(0, 1, &constant_buffer);
         std::array<float, 16> transform = {};
         bool transform_known;
         {
            const std::lock_guard lock(gd.mv_constants_mutex);
            const auto& known = gd.sp_canvas_transforms[reinterpret_cast<uint64_t>(constant_buffer.get())];
            transform_known = known.has_value();
            if (transform_known)
               transform = *known;
         }
         const auto close_to = [](float value, float expected)
         { return std::abs(value - expected) <= 0.002f * (std::max)(1.f, std::abs(expected)); };
         const bool pixel_transform = transform_known && close_to(transform[0] * viewport.Width * 0.5f, 1.f) && close_to(transform[1], 0.f) && close_to(transform[4], 0.f) && close_to(-transform[5] * viewport.Height * 0.5f, 1.f) && close_to(transform[12], -1.f) && close_to(transform[13], 1.f);
         const bool move = g_sp_fix_hud_canvas && transform_known && !pixel_transform && viewport_count == 1 && original_draw_dispatch_func && *original_draw_dispatch_func;
#if DEVELOPMENT
         {
            com_ptr<ID3D11ShaderResourceView> srv;
            native_device_context->PSGetShaderResources(0, 1, &srv);
            D3D11_TEXTURE2D_DESC texture_desc = {};
            if (srv)
            {
               com_ptr<ID3D11Resource> texture;
               srv->GetResource(&texture);
               if (com_ptr<ID3D11Texture2D> texture_2d; texture && SUCCEEDED(texture->QueryInterface(&texture_2d)))
                  texture_2d->GetDesc(&texture_desc);
            }
            std::string kind = std::format("t0 {}x{} fmt {}, {} transform{}", texture_desc.Width, texture_desc.Height, unsigned(texture_desc.Format), !transform_known ? "unknown" : (pixel_transform ? "pixel" : "HUD"), move ? ", moved" : "");
            // A HUD transform's raw rows (cb0[0..3])
            if (transform_known && !pixel_transform)
               kind += std::format(" | {:.5g} {:.5g} {:.5g} {:.5g} / {:.5g} {:.5g} {:.5g} {:.5g} / {:.5g} {:.5g} {:.5g} {:.5g} / {:.5g} {:.5g} {:.5g} {:.5g}", transform[0], transform[1], transform[2], transform[3], transform[4], transform[5], transform[6], transform[7], transform[8], transform[9], transform[10], transform[11], transform[12], transform[13], transform[14], transform[15]);
            g_sp_canvas_kinds[kind]++;
         }
#endif
         if (move)
         {
            const D3D11_VIEWPORT game_viewport = viewport;
            const uint2 render_size = GetScreenPercentageRenderSize(device_data.output_resolution);
            const float2 origin = {float((uint32_t(device_data.output_resolution.x) - render_size.x) / 2), float((uint32_t(device_data.output_resolution.y) - render_size.y) / 2)};
            const float2 centre = {device_data.output_resolution.x * 0.5f, device_data.output_resolution.y * 0.5f};
            const float2 share = {render_size.x / device_data.output_resolution.x, render_size.y / device_data.output_resolution.y};
            viewport.TopLeftX = centre.x + share.x * (viewport.TopLeftX - origin.x - centre.x);
            viewport.TopLeftY = centre.y + share.y * (viewport.TopLeftY - origin.y - centre.y);
            viewport.Width *= share.x;
            viewport.Height *= share.y;
            native_device_context->RSSetViewports(1, &viewport);
            (*original_draw_dispatch_func)();
            native_device_context->RSSetViewports(1, &game_viewport);
            return DrawOrDispatchOverrideType::Replaced;
         }
      }

      // DLSS / FSR: the scene's draws with motion vectors or jitter (see "DrawWithMotionVectors"), and the upscaler at its first post pass
      if (gd.mv_active)
      {
         if (gather || uber)
         {
            if (gd.mv_scene_open)
            {
#if DEVELOPMENT
               gd.mv_stats.ended_by = gather ? kDOFBloomGatherHash : uint32_t(original_shader_hashes.pixel_shaders[0]);
#endif
               EndScene(native_device, native_device_context, device_data);
            }
         }
         else if (!gd.mv_scene_done && !is_custom_pass && original_draw_dispatch_func && *original_draw_dispatch_func && (stages & reshade::api::shader_stage::vertex) == reshade::api::shader_stage::vertex)
         {
            com_ptr<ID3D11RenderTargetView> rtvs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
            com_ptr<ID3D11DepthStencilView> dsv;
            native_device_context->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, &rtvs[0], &dsv);
            if (!gd.mv_scene_open && dsv)
               OpenScene(native_device, native_device_context, cmd_list_data, device_data, original_shader_hashes.vertex_shaders[0], dsv.get());
            if (DrawWithMotionVectors(native_device, native_device_context, cmd_list_data, device_data, original_shader_hashes, *original_draw_dispatch_func, rtvs, dsv.get()) ||
                DrawWithJitter(native_device, native_device_context, cmd_list_data, device_data, original_shader_hashes, *original_draw_dispatch_func, dsv.get()))
               return DrawOrDispatchOverrideType::Replaced;
         }
      }
      // Every draw without a patched shader: the game's own, if the last patched draw's are still bound
      RestoreGameShader(native_device_context, &gd.mv_bound_vertex_shader);
      RestoreGameShader(native_device_context, &gd.mv_bound_pixel_shader);

      // DLSS / FSR: the DOF/Bloom gather takes its DOF amount from a per pixel history (the jittered depth flipped whole blocks, see
      // Luma_BL_OutputGather.hlsl). Below native it also reads the upscaled scene at output resolution and fills its whole target (the
      // game's covers the render sub-rect, which the uber magnified), then its blurs cover the whole target too ("BlurUVScale" in the uber).
      if (device_data.has_drawn_sr && original_draw_dispatch_func && *original_draw_dispatch_func)
      {
         const bool below_native = g_screen_percentage < 1.f;
         const uint2 render_size = GetScreenPercentageRenderSize(device_data.output_resolution);
         const float2 share = {render_size.x / device_data.output_resolution.x, render_size.y / device_data.output_resolution.y};
         if (gather)
         {
            auto* const gather_shader = FindShader(device_data.native_compute_shaders, CompileTimeStringHash("BL Output Gather CS"));
            auto* const history_shader = FindShader(device_data.native_compute_shaders, CompileTimeStringHash("BL DOF History CS"));
            // The DOF history: render pixels, in textures of the scene's (output) size
            D3D11_TEXTURE2D_DESC history_desc = {};
            if (gd.dof_history[0])
               gd.dof_history[0]->GetDesc(&history_desc);
            if (history_desc.Width != uint32_t(device_data.output_resolution.x) || history_desc.Height != uint32_t(device_data.output_resolution.y))
            {
               gd.dof_history_valid = false;
               history_desc = {uint32_t(device_data.output_resolution.x), uint32_t(device_data.output_resolution.y), 1, 1, DXGI_FORMAT_R16_FLOAT, {1, 0}, D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS};
               for (size_t i = 0; i < std::size(gd.dof_history); i++)
               {
                  gd.dof_history[i].reset();
                  gd.dof_history_srvs[i].reset();
                  gd.dof_history_uavs[i].reset();
                  if (SUCCEEDED(native_device->CreateTexture2D(&history_desc, nullptr, &gd.dof_history[i])))
                  {
                     native_device->CreateShaderResourceView(gd.dof_history[i].get(), nullptr, &gd.dof_history_srvs[i]);
                     native_device->CreateUnorderedAccessView(gd.dof_history[i].get(), nullptr, &gd.dof_history_uavs[i]);
                  }
               }
            }
            const uint32_t previous = gd.dof_history_index, next = gd.dof_history_index ^ 1;
            // Weight of this frame: 0.1 (about ten frames), 1 when the history restarts with the upscaler's
            const float constants[12] = {share.x, share.y, 1.f / share.x, 1.f / share.y, 0.f, 0.f, float(render_size.x), float(render_size.y), (gd.dof_history_valid && !device_data.force_reset_sr) ? 0.1f : 1.f, 0.f, 0.f, 0.f};
            // The game's scene copy (at native it holds the upscaled scene already) and depth (the last copy, before the first person weapon)
            com_ptr<ID3D11ShaderResourceView> game_srvs[2];
            native_device_context->CSGetShaderResources(0, UINT(std::size(game_srvs)), &game_srvs[0]);
            if (gather_shader && history_shader && gd.dof_history_srvs[previous] && gd.dof_history_uavs[next] && gd.dof_history_srvs[next] && gd.mv_srv && game_srvs[1] && (!below_native || (gd.sp_upscaled_srv && gd.sp_upscaled_texture == device_data.sr_output_color)) && WriteConstants(native_device, native_device_context, std::addressof(gd.sp_post_scale_cb), constants, sizeof(constants)))
            {
               com_ptr<ID3D11UnorderedAccessView> target_uav;
               native_device_context->CSGetUnorderedAccessViews(0, 1, &target_uav);
               com_ptr<ID3D11Resource> target;
               if (target_uav)
                  target_uav->GetResource(&target);
               gd.sp_gather_target = reinterpret_cast<uint64_t>(target.get());
               DrawStateStack<DrawStateStackType::Compute> compute_state;
               DrawStateStack<DrawStateStackType::SimpleGraphics> graphics_state;
               compute_state.Cache(native_device_context, device_data.uav_max_count);
               graphics_state.Cache(native_device_context, device_data.uav_max_count);
               // The motion vector target may still be bound for output (see "DrawWithMotionVectors"): its SRV would be null
               native_device_context->OMSetRenderTargets(0, nullptr, nullptr);

               ID3D11Buffer* const cb = gd.sp_post_scale_cb.get();
               ID3D11SamplerState* const linear_sampler = device_data.sampler_state_linear.get();
               native_device_context->CSSetConstantBuffers(5, 1, &cb);
               native_device_context->CSSetSamplers(0, 1, &linear_sampler);

               // Outputs are bound before inputs in both passes: D3D11 nulls an SRV of a resource still bound for output (the new history
               // is the first pass's output and the gather's input)

               // The DOF amount history: the game's depth, the motion vectors and last frame's history in, the next one out
               {
                  ID3D11ShaderResourceView* const srvs[3] = {game_srvs[1].get(), gd.mv_srv.get(), gd.dof_history_srvs[previous].get()};
                  ID3D11UnorderedAccessView* const uav = gd.dof_history_uavs[next].get();
                  native_device_context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);
                  native_device_context->CSSetShaderResources(0, UINT(std::size(srvs)), srvs);
                  native_device_context->CSSetShader(history_shader, nullptr, 0);
                  native_device_context->Dispatch((render_size.x + 7) / 8, (render_size.y + 7) / 8, 1);
               }

               // The gather: the scene, the game's depth and the new history in, its target out
               {
                  ID3D11ShaderResourceView* const srvs[3] = {below_native ? gd.sp_upscaled_srv.get() : game_srvs[0].get(), game_srvs[1].get(), gd.dof_history_srvs[next].get()};
                  ID3D11UnorderedAccessView* const uav = target_uav.get();
                  native_device_context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);
                  native_device_context->CSSetShaderResources(0, UINT(std::size(srvs)), srvs);
                  native_device_context->CSSetShader(gather_shader, nullptr, 0);
                  // One thread per 2x2 scene texels, as the game's at 100%
                  native_device_context->Dispatch((uint32_t(device_data.output_resolution.x) + 15) / 16, (uint32_t(device_data.output_resolution.y) + 15) / 16, 1);
               }

               compute_state.Restore(native_device_context);
               graphics_state.Restore(native_device_context);
               gd.dof_history_index = next;
               gd.dof_history_valid = true;
               gd.sp_post_output_res = below_native;
               return DrawOrDispatchOverrideType::Replaced;
            }
         }
         else if (gd.sp_post_output_res && std::ranges::any_of(kFilterVertexShaders, [&](const auto& filter)
                                              { return original_shader_hashes.Contains(filter.first, reshade::api::shader_stage::vertex); }))
         {
            auto* const blur_shader = FindShader(device_data.native_vertex_shaders, original_shader_hashes.vertex_shaders[0]);
            D3D11_VIEWPORT viewport;
            UINT viewport_count = 1;
            native_device_context->RSGetViewports(&viewport_count, &viewport);
            if (blur_shader && viewport_count == 1)
            {
               DrawStateStack<DrawStateStackType::SimpleGraphics> graphics_state;
               graphics_state.Cache(native_device_context, device_data.uav_max_count);
               // The quad covers the sub-rect, of the viewport or with a sub-rect viewport: scaled from the top left, it covers the target
               // (and so does the scissor, which may clip to the sub-rect)
               viewport.Width /= share.x;
               viewport.Height /= share.y;
               D3D11_RECT scissor;
               UINT scissor_count = 1;
               native_device_context->RSGetScissorRects(&scissor_count, &scissor);
               scissor.right = LONG(std::ceil(scissor.right / share.x));
               scissor.bottom = LONG(std::ceil(scissor.bottom / share.y));
               ID3D11Buffer* const cb = gd.sp_post_scale_cb.get();
               native_device_context->RSSetViewports(1, &viewport);
               if (scissor_count == 1)
                  native_device_context->RSSetScissorRects(1, &scissor);
               native_device_context->VSSetConstantBuffers(5, 1, &cb);
               native_device_context->VSSetShader(blur_shader, nullptr, 0);
               (*original_draw_dispatch_func)();
               graphics_state.Restore(native_device_context);
               return DrawOrDispatchOverrideType::Replaced;
            }
         }
      }

      // Hide HUD: cancel the game's UI draws (for clean screenshots). A UI draw = one targeting a swapchain back
      // buffer — the same detection the core UI handling uses (RTV resource in device_data.back_buffers; see
      // core.hpp). Compute/offscreen draws have no swapchain RTV so they're untouched.
      if (g_hide_ui && !is_custom_pass)
      {
         ComPtr<ID3D11RenderTargetView> rtv;
         native_device_context->OMGetRenderTargets(1, rtv.put(), nullptr);
         ComPtr<ID3D11Resource> rtv_res;
         if (rtv)
            rtv->GetResource(rtv_res.put());
         if (rtv_res && IsBackBuffer(device_data, rtv_res.get()))
            return DrawOrDispatchOverrideType::Replaced; // drop the UI draw
      }

      // SMAA predication inputs: the cel-shading edge pass binds the D24S8 depth at PS t0 and linearizes it with
      // PSOffsetConstants at PS b2.
      if (original_shader_hashes.Contains(kCelShadingHash, reshade::api::shader_stage::pixel))
      {
         gd.srv_depth.reset();
         gd.cb_game_offsets.reset();
         native_device_context->PSGetShaderResources(0, 1, gd.srv_depth.put());
         native_device_context->PSGetConstantBuffers(2, 1, gd.cb_game_offsets.put());
#if DEVELOPMENT
         if (g_skip_outlines)
            return DrawOrDispatchOverrideType::Skip;
#endif
      }
#if DEVELOPMENT
      if (g_skip_outlines && original_shader_hashes.Contains(0xC3C26F77, reshade::api::shader_stage::pixel)) // Weapon outline
         return DrawOrDispatchOverrideType::Skip;
#endif

      // XeGTAO replaces the game's native HBAO+ (chain order: deinterleave x2 -> normals 0xB2B47225 ->
      // coarse x2 -> blur -> apply blit). We take the chain over ONLY when everything is ready at the first
      // dispatch — a failure there leaves the whole native chain untouched (graceful fallback, like the SMAA
      // fp16 guard). The separate normals pass 0xB2B47225 is left running (not hooked) so its ViewNormalTex
      // output is valid for our main pass.
      if (g_gtao_enable)
      {
         // (a) Deinterleave: capture the full-res r24 scene depth (t0) + build/validate ALL scratch, then skip
         // the native dispatch. Both dispatches of the pair hit this branch (second is a cheap re-capture).
         if (original_shader_hashes.Contains(kAODeinterleaveHash, reshade::api::shader_stage::compute))
         {
            if (!AllShadersReady(device_data.native_compute_shaders, {CompileTimeStringHash("BL XeGTAO Prefilter Depths CS"), CompileTimeStringHash("BL XeGTAO Main Pass CS"), CompileTimeStringHash("BL XeGTAO Denoise Pass 1 CS"), CompileTimeStringHash("BL XeGTAO Denoise Pass 2 CS")}))
               return DrawOrDispatchOverrideType::None;

            ComPtr<ID3D11ShaderResourceView> srv_d;
            native_device_context->CSGetShaderResources(0, 1, srv_d.put());
            if (!srv_d)
               return DrawOrDispatchOverrideType::None;
            uint4 dinfo{};
            DXGI_FORMAT dfmt = DXGI_FORMAT_UNKNOWN;
            GetResourceInfo(srv_d.get(), dinfo, dfmt);
            if (dinfo.x == 0 || dinfo.y == 0)
               return DrawOrDispatchOverrideType::None;
            // Size the scratch (and dispatches) from the input depth desc (the game's HBAO+ full-res). The
            // game's own cb0 (InvFullResolution etc.) is content-dimensioned, so the shader's pixel<->UV math
            // is correct; the final pass writes the game's AO buffer at identical pixel coords.
            const uint32_t w = dinfo.x, h = dinfo.y;

            // (Re)create the scratch at the game's AO full-res (cached; NOT per-frame).
            if (gd.gtao_w != w || gd.gtao_h != h || !gd.tex_gtao_depth_mips || !gd.tex_gtao_working[1])
            {
               gd.ReleaseGTAOScratch();

               D3D11_TEXTURE2D_DESC td = {};
               td.Width = w;
               td.Height = h;
               td.MipLevels = 5; // XE_GTAO_DEPTH_MIP_LEVELS
               td.ArraySize = 1;
               td.Format = DXGI_FORMAT_R32_FLOAT;
               td.SampleDesc.Count = 1;
               td.Usage = D3D11_USAGE_DEFAULT;
               td.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
               bool ok = SUCCEEDED(native_device->CreateTexture2D(&td, nullptr, gd.tex_gtao_depth_mips.put()));
               D3D11_UNORDERED_ACCESS_VIEW_DESC ud = {};
               ud.Format = td.Format;
               ud.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
               for (int i = 0; ok && i < 5; i++)
               {
                  ud.Texture2D.MipSlice = i;
                  ok = SUCCEEDED(native_device->CreateUnorderedAccessView(gd.tex_gtao_depth_mips.get(), &ud, gd.gtao_depth_mip_uavs[i].put()));
               }
               ok = ok && SUCCEEDED(native_device->CreateShaderResourceView(gd.tex_gtao_depth_mips.get(), nullptr, gd.srv_gtao_depth_mips.put()));

               td.MipLevels = 1;
               td.Format = DXGI_FORMAT_R8G8_UNORM;
               for (int i = 0; ok && i < 2; i++)
               {
                  ok = ok && SUCCEEDED(native_device->CreateTexture2D(&td, nullptr, gd.tex_gtao_working[i].put()));
                  ok = ok && SUCCEEDED(native_device->CreateUnorderedAccessView(gd.tex_gtao_working[i].get(), nullptr, gd.uav_gtao_working[i].put()));
                  ok = ok && SUCCEEDED(native_device->CreateShaderResourceView(gd.tex_gtao_working[i].get(), nullptr, gd.srv_gtao_working[i].put()));
               }
               if (!ok)
               {
                  gd.ReleaseGTAOScratch();
                  return DrawOrDispatchOverrideType::None; // native chain runs whole
               }
               gd.gtao_w = w;
               gd.gtao_h = h;
            }

            // Knob CB (b11): immutable, recreated when a slider moves.
#if DEVELOPMENT
            const float dbg = (float)g_gtao_debug_view;
#else
            const float dbg = 0.f; // shader DebugViewRT is DEV-only; keep 0 elsewhere so the knob CB never churns
#endif
            const float knobs[4] = {g_gtao_final_value_power, g_gtao_depth_scale, g_gtao_radius_override, dbg};
            if ((!gd.cb_gtao || std::memcmp(gd.gtao_cb_data, knobs, sizeof(knobs)) != 0) && CreateImmutableCB(native_device, knobs, sizeof(knobs), gd.cb_gtao))
               std::memcpy(gd.gtao_cb_data, knobs, sizeof(knobs));
            if (!gd.cb_gtao)
               return DrawOrDispatchOverrideType::None;

            gd.srv_gtao_depth = srv_d;

            return DrawOrDispatchOverrideType::Replaced; // skip the native deinterleave
         }

         // (b) Coarse horizon march: capture the packed view normals (t0), skip the native dispatch. Only when
         // we own the chain this frame — otherwise the native pipeline is left fully intact.
         if (original_shader_hashes.Contains(kAOCoarseHash, reshade::api::shader_stage::compute))
         {
            if (!gd.srv_gtao_depth)
               return DrawOrDispatchOverrideType::None;
            ComPtr<ID3D11ShaderResourceView> srv_n;
            native_device_context->CSGetShaderResources(0, 1, srv_n.put());
            if (srv_n)
               gd.srv_gtao_normals = srv_n;
            return DrawOrDispatchOverrideType::Replaced; // skip the native coarse march
         }

         // (c) Bilateral blur: ITS u0 is the game's FINAL r16g16_float AO — run the 4 XeGTAO passes into it and
         // cancel the native dispatch. The game's apply blit (untouched) then multiplies it into the scene.
         if (original_shader_hashes.Contains(kAOBlurHash, reshade::api::shader_stage::compute))
         {
            if (!gd.srv_gtao_depth)
               return DrawOrDispatchOverrideType::None;

            ComPtr<ID3D11UnorderedAccessView> uav_final;
            native_device_context->CSGetUnorderedAccessViews(0, 1, uav_final.put());
            // No output bound: unreachable in practice (the bilateral blur always binds its u0). Without a target
            // there is nothing for either path to write, so leave the native blur running rather than cancel it.
            if (!uav_final)
               return DrawOrDispatchOverrideType::None;
            if (!gd.srv_gtao_normals)
            {
               // Shouldn't happen (chain order is fixed); write "no AO" so a stale buffer can't apply.
               const FLOAT ones[4] = {1.f, 1.f, 1.f, 1.f};
               native_device_context->ClearUnorderedAccessViewFloat(uav_final.get(), ones);
               return DrawOrDispatchOverrideType::Replaced;
            }

            const uint32_t w = gd.gtao_w, h = gd.gtao_h;
            auto* cs_prefilter = FindShader(device_data.native_compute_shaders, CompileTimeStringHash("BL XeGTAO Prefilter Depths CS"));
            auto* cs_main = FindShader(device_data.native_compute_shaders, CompileTimeStringHash("BL XeGTAO Main Pass CS"));
            auto* cs_denoise_1 = FindShader(device_data.native_compute_shaders, CompileTimeStringHash("BL XeGTAO Denoise Pass 1 CS"));
            auto* cs_denoise_2 = FindShader(device_data.native_compute_shaders, CompileTimeStringHash("BL XeGTAO Denoise Pass 2 CS"));
            DrawStateStack<DrawStateStackType::Compute> st;
            st.Cache(native_device_context, device_data.uav_max_count);

            ID3D11Buffer* kcb = gd.cb_gtao.get();
            native_device_context->CSSetConstantBuffers(11, 1, &kcb);
            ID3D11SamplerState* smp = device_data.sampler_state_point.get();
            native_device_context->CSSetSamplers(0, 1, &smp);
            // cb0 ($Globals: ProjInfo etc.) and cb2 (MinZ_MaxZRatioCS) are read directly by our shaders at the
            // same b0/b2 slots, INHERITED from the game — bound by the deinterleave/coarse passes earlier in the
            // same contiguous HBAO+ chain, not rebound here by design (immediate context, fixed pass order, no
            // ClearState between them). If a future variant reorders the AO passes, capture+rebind explicitly.

            static constexpr std::array<ID3D11UnorderedAccessView*, 5> uav_nulls5 = {};
            static constexpr std::array<ID3D11ShaderResourceView*, 2> srv_nulls2 = {};

            // 1) Prefilter: game full-res depth -> our R32F mip pyramid (each thread does 2x2 -> 16x16 per group).
            {
               native_device_context->CSSetShaderResources(0, 2, srv_nulls2.data());
               ID3D11ShaderResourceView* srv = gd.srv_gtao_depth.get();
               ID3D11UnorderedAccessView* uavs[5] = {gd.gtao_depth_mip_uavs[0].get(), gd.gtao_depth_mip_uavs[1].get(),
                  gd.gtao_depth_mip_uavs[2].get(), gd.gtao_depth_mip_uavs[3].get(), gd.gtao_depth_mip_uavs[4].get()};
               native_device_context->CSSetUnorderedAccessViews(0, 5, uavs, nullptr);
               native_device_context->CSSetShaderResources(0, 1, &srv);
               native_device_context->CSSetShader(cs_prefilter, nullptr, 0);
               native_device_context->Dispatch((w + 15) / 16, (h + 15) / 16, 1);
               native_device_context->CSSetUnorderedAccessViews(0, 5, uav_nulls5.data(), nullptr);
            }
            // Binding ORDER matters: the UAV must be set BEFORE the SRVs in every pass. Binding an SRV whose
            // resource is still bound as a CS UAV (from the previous pass) makes the runtime silently NULL the
            // SRV (the existing UAV wins the hazard) — the whole chain then reads zeros while every write
            // "succeeds". Setting the new UAV first auto-unbinds the old one, so the SRV bind is clean.
            // 2) Main pass: pyramid + game view normals -> AO+edges (working0).
            {
               native_device_context->CSSetShaderResources(0, 2, srv_nulls2.data());
               ID3D11ShaderResourceView* srvs[2] = {gd.srv_gtao_depth_mips.get(), gd.srv_gtao_normals.get()};
               ID3D11UnorderedAccessView* uav = gd.uav_gtao_working[0].get();
               native_device_context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);
               native_device_context->CSSetShaderResources(0, 2, srvs);
               native_device_context->CSSetShader(cs_main, nullptr, 0);
               native_device_context->Dispatch((w + 7) / 8, (h + 7) / 8, 1);
            }
            // 3) Denoise 1: working0 -> working1 (2 horizontal pixels per thread).
            {
               native_device_context->CSSetShaderResources(0, 2, srv_nulls2.data());
               ID3D11ShaderResourceView* srv = gd.srv_gtao_working[0].get();
               ID3D11UnorderedAccessView* uav = gd.uav_gtao_working[1].get();
               native_device_context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);
               native_device_context->CSSetShaderResources(0, 1, &srv);
               native_device_context->CSSetShader(cs_denoise_1, nullptr, 0);
               native_device_context->Dispatch((w + 15) / 16, (h + 7) / 8, 1);
            }
            // 4) Denoise 2 (final): working1 -> the game's r16g16_float AO target.
            {
               native_device_context->CSSetShaderResources(0, 2, srv_nulls2.data());
               ID3D11ShaderResourceView* srv = gd.srv_gtao_working[1].get();
               ID3D11UnorderedAccessView* uav = uav_final.get();
               native_device_context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);
               native_device_context->CSSetShaderResources(0, 1, &srv);
               native_device_context->CSSetShader(cs_denoise_2, nullptr, 0);
               native_device_context->Dispatch((w + 15) / 16, (h + 7) / 8, 1);
            }

            st.Restore(native_device_context);
            return DrawOrDispatchOverrideType::Replaced; // cancel the native blur
         }
      }

#if ENABLE_SMAA
      // Replace the compute FXAA resolve with SMAA. Replace EVERY occurrence in the frame (the game can run the
      // resolve more than once — e.g. menu/transition frames have two), each with its own InColor/Color target.
      // After DLSS / FSR (which already antialiased the scene, FXAA would blur it) only RCAS runs, or nothing.
      if ((g_smaa_enable || device_data.has_drawn_sr) && !is_custom_pass &&
          original_shader_hashes.Contains(kFXAAResolveHash, reshade::api::shader_stage::compute))
      {
         // FXAA resolve is IN-PLACE: InColor (t2) aliases Color (u0 = swapchain), so D3D auto-unbinds the SRV at
         // dispatch (t2 reads null). We therefore source the scene color from the UAV's resource (it holds the
         // tonemapped pre-FXAA color, since we're replacing FXAA) by copying it into an SRV-capable temp.
         ComPtr<ID3D11UnorderedAccessView> uav_color;
         native_device_context->CSGetUnorderedAccessViews(0, 1, uav_color.put());
         if (!uav_color)
            return DrawOrDispatchOverrideType::None;

         ComPtr<ID3D11Resource> color_res; // swapchain target (CopyResource source + destination)
         uav_color->GetResource(color_res.put());
         if (!color_res)
            return DrawOrDispatchOverrideType::None;

         uint4 cinfo{};
         DXGI_FORMAT cfmt = DXGI_FORMAT_UNKNOWN;
         GetResourceInfo(color_res.get(), cinfo, cfmt);
         const uint32_t w = cinfo.x, h = cinfo.y;
         if (w == 0 || h == 0)
            return DrawOrDispatchOverrideType::None;
         // fp16 guard: the SMAA path forces R16G16B16A16_FLOAT temps; CopyResource silently no-ops on a format
         // mismatch, so a non-fp16 swapchain (HDR upgrade absent) would feed SMAA uninitialized memory and copy
         // garbage back. Bail to the game's native FXAA instead of shipping a corrupt frame.
         if (cfmt != DXGI_FORMAT_R16G16B16A16_FLOAT)
         {
#if DEVELOPMENT || TEST
            if (!gd.logged_no_fp16)
            {
               gd.logged_no_fp16 = true;
               reshade::log::message(reshade::log::level::warning,
                  "[BL-SMAA] swapchain not fp16 (HDR upgrade absent) -> SMAA skipped, native FXAA kept.");
            }
#endif
            return DrawOrDispatchOverrideType::None;
         }
         // Below ScreenPercentage 100 the stretch overwrites this target: RCAS runs at the stretch instead
         const bool smaa = !device_data.has_drawn_sr;
         if (!smaa && (g_rcas_sharpness <= 0.f || g_screen_percentage < 1.f))
            return DrawOrDispatchOverrideType::Replaced;

         // A new color size (in-game Resolution Scale) drops every resource sized like it; each is rebuilt below when missing. DrawSMAA
         // sizes its core-managed intermediates from the first RTV and rebuilds them only on swapchain re-init, so those go too.
         if (gd.smaa_w != w || gd.smaa_h != h)
         {
            gd.tex_pred.reset();
            gd.uav_pred.reset();
            gd.srv_pred.reset();
            gd.cb_smaa_metrics.reset();
            gd.tex_input.reset();
            gd.srv_input.reset();
            gd.tex_input_linear.reset();
            gd.uav_input_linear.reset();
            gd.srv_input_linear.reset();
            gd.tex_smaa_out.reset();
            gd.tex_smaa_out_rtv.reset();
            gd.tex_smaa_out_srv.reset();
            gd.cb_sharpen.reset();
            gd.tex_rcas_out.reset();
            gd.tex_rcas_out_rtv.reset();
            auto& mr = device_data.managed_resources;
            mr.depth_stencil_views[CompileTimeStringHash("smaa_dsv")].reset();
            mr.render_target_views[CompileTimeStringHash("smaa_edge_detection")].reset();
            mr.render_target_views[CompileTimeStringHash("smaa_blending_weight_calculation")].reset();
            gd.smaa_w = w;
            gd.smaa_h = h;
         }

         // Predication depth is valid only if captured this frame AND it matches the color dimensions (a resolution
         // change can leave a different-size depth). When invalid we pass a null predication texture and a scale of
         // 1.0 so SMAA uses the plain ULTRA threshold rather than the doubled predicated baseline.
         auto* pred_cs = FindShader(device_data.native_compute_shaders, CompileTimeStringHash("BL Depth Extract CS"));
         bool pred_ok = smaa && g_smaa_predication && gd.srv_depth && gd.cb_game_offsets && pred_cs != nullptr;
         if (pred_ok)
         {
            // The extract CS maps texels 1:1, so a depth buffer of a different size would read a sub-rect and
            // misalign the mask against the colour grid.
            uint4 dinfo{};
            DXGI_FORMAT dfmt = DXGI_FORMAT_UNKNOWN;
            GetResourceInfo(gd.srv_depth.get(), dinfo, dfmt);
            pred_ok = (dinfo.x == w && dinfo.y == h);
         }
         if (pred_ok)
         {
            if (!gd.cb_pred || gd.pred_tolerance != g_smaa_pred_tolerance)
            {
               const float pp[4] = {g_smaa_pred_tolerance, 0.f, 0.f, 0.f};
               if (CreateImmutableCB(native_device, pp, sizeof(pp), gd.cb_pred))
                  gd.pred_tolerance = g_smaa_pred_tolerance;
            }
            if (!gd.tex_pred && CreateDefaultTex(native_device, w, h, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, gd.tex_pred, DXGI_FORMAT_R16_FLOAT))
            {
               native_device->CreateUnorderedAccessView(gd.tex_pred.get(), nullptr, gd.uav_pred.put());
               native_device->CreateShaderResourceView(gd.tex_pred.get(), nullptr, gd.srv_pred.put());
            }
            pred_ok = gd.cb_pred && gd.uav_pred && gd.srv_pred;
         }
         const float pred_scale = pred_ok ? 2.f : 1.f;

         // Shader-readiness gate (async loader / dev live-reload). If anything is missing, fall through to native FXAA.
         auto* linearize_cs = FindShader(device_data.native_compute_shaders, CompileTimeStringHash("BL SMAA Linearize CS"));
         if (smaa && (linearize_cs == nullptr || !AllShadersReady(device_data.native_pixel_shaders, {CompileTimeStringHash("SMAA Edge Detection PS"), CompileTimeStringHash("SMAA Blending Weight Calculation PS"), CompileTimeStringHash("SMAA Neighborhood Blending PS")}) ||
                        !AllShadersReady(device_data.native_vertex_shaders, {CompileTimeStringHash("SMAA Edge Detection VS"), CompileTimeStringHash("SMAA Blending Weight Calculation VS"), CompileTimeStringHash("SMAA Neighborhood Blending VS")})))
            return DrawOrDispatchOverrideType::None;

         // (Re)create the SMAA metrics CB on resolution change OR when the predication state flips (depth
         // present/absent). pred_scale only toggles 1.0<->2.0 on menu/transition boundaries, so recreation is rare.
         if (smaa && (!gd.cb_smaa_metrics || gd.smaa_metrics_pred_scale != pred_scale))
         {
            const float metrics[8] = {1.f / (float)w, 1.f / (float)h, (float)w, (float)h, pred_scale, 0.f, 0.f, 0.f};
            if (CreateImmutableCB(native_device, metrics, sizeof(metrics), gd.cb_smaa_metrics))
               gd.smaa_metrics_pred_scale = pred_scale;
         }
         if (smaa && !gd.cb_smaa_metrics)
            return DrawOrDispatchOverrideType::None;

         // (Re)create the SMAA output temp (fp16, SRV+RTV).
         if (!gd.tex_smaa_out && CreateDefaultTex(native_device, w, h, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET, gd.tex_smaa_out))
         {
            native_device->CreateRenderTargetView(gd.tex_smaa_out.get(), nullptr, gd.tex_smaa_out_rtv.put());
            native_device->CreateShaderResourceView(gd.tex_smaa_out.get(), nullptr, gd.tex_smaa_out_srv.put());
         }
         if (!gd.tex_smaa_out_rtv || !gd.tex_smaa_out_srv)
            return DrawOrDispatchOverrideType::None;

         if (smaa)
         {
            // (Re)create the SMAA inputs on resolution change (cached like tex_smaa_out, no full-res fp16 alloc/free
            // every replaced frame).
            if (!gd.srv_input && CreateDefaultTex(native_device, w, h, D3D11_BIND_SHADER_RESOURCE, gd.tex_input) &&
                CreateDefaultTex(native_device, w, h, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, gd.tex_input_linear))
            {
               native_device->CreateShaderResourceView(gd.tex_input.get(), nullptr, gd.srv_input.put());
               native_device->CreateUnorderedAccessView(gd.tex_input_linear.get(), nullptr, gd.uav_input_linear.put());
               native_device->CreateShaderResourceView(gd.tex_input_linear.get(), nullptr, gd.srv_input_linear.put());
            }
            if (!gd.srv_input || !gd.uav_input_linear || !gd.srv_input_linear)
               return DrawOrDispatchOverrideType::None;

            // Snapshot the (in-place) scene color out of the swapchain so SMAA can read it (no alloc — reuses tex_input).
            native_device_context->CopyResource(gd.tex_input.get(), color_res.get());

            // Linear-light decode of the snapshot for the neighborhood blend (Luma_BL_SMAALinearize.hlsl).
            {
               DrawStateStack<DrawStateStackType::Compute> linearize_state;
               linearize_state.Cache(native_device_context, device_data.uav_max_count);
               ID3D11ShaderResourceView* lin_srv = gd.srv_input.get();
               ID3D11UnorderedAccessView* lin_uav = gd.uav_input_linear.get();
               native_device_context->CSSetUnorderedAccessViews(0, 1, &lin_uav, nullptr);
               native_device_context->CSSetShaderResources(0, 1, &lin_srv);
               native_device_context->CSSetShader(linearize_cs, nullptr, 0);
               native_device_context->Dispatch((w + 7) / 8, (h + 7) / 8, 1);
               linearize_state.Restore(native_device_context);
            }
         }
         else
         {
            // RCAS reads the upscaled output from the SMAA output temp
            native_device_context->CopyResource(gd.tex_smaa_out.get(), color_res.get());
         }

         // Predication extract: hardware d24 -> plane-deviation edge-ness in R16F. Core's Compute state stack restores
         // the game's CS state and unbinds our UAV before DrawSMAA reads it as an SRV (an SRV of a resource still bound
         // as a UAV reads as null).
         if (pred_ok)
         {
            DrawStateStack<DrawStateStackType::Compute> pred_state;
            pred_state.Cache(native_device_context, device_data.uav_max_count);
            ID3D11ShaderResourceView* pred_srv = gd.srv_depth.get();
            ID3D11UnorderedAccessView* pred_uav = gd.uav_pred.get();
            ID3D11Buffer* pred_cbs[1] = {gd.cb_pred.get()};
            ID3D11Buffer* game_cb[1] = {gd.cb_game_offsets.get()};
            native_device_context->CSSetUnorderedAccessViews(0, 1, &pred_uav, nullptr);
            native_device_context->CSSetShaderResources(0, 1, &pred_srv);
            native_device_context->CSSetConstantBuffers(0, 1, pred_cbs);
            native_device_context->CSSetConstantBuffers(2, 1, game_cb); // PSOffsetConstants: MinZ_MaxZRatio
            native_device_context->CSSetShader(pred_cs, nullptr, 0);
            native_device_context->Dispatch((w + 7) / 8, (h + 7) / 8, 1);
            pred_state.Restore(native_device_context);
         }

#if DEVELOPMENT
         // Both calibration aids read the mask the extract CS just wrote. The numeric one runs first so it still
         // reports while the debug view is on (that path returns early).
         if (pred_ok)
            LogPredicationStats(native_device, native_device_context, gd);

         if (pred_ok && g_smaa_pred_debug)
         {
            auto* debug_vs = FindShader(device_data.native_vertex_shaders, CompileTimeStringHash("Copy VS"));
            auto* debug_ps = FindShader(device_data.native_pixel_shaders, CompileTimeStringHash("Copy PS"));
            if (debug_vs != nullptr && debug_ps != nullptr)
            {
               // The mask is single-channel, so the core copy lands it in RED - unmistakably a debug view. It
               // REPLACES the antialiased frame, reusing the same temp-then-copy route the real path takes.
               DrawStateStack<DrawStateStackType::FullGraphics> debug_state;
               debug_state.Cache(native_device_context, device_data.uav_max_count);
               DrawCustomPixelShader(native_device_context, device_data.default_depth_stencil_state.get(), device_data.default_blend_state.get(), nullptr,
                  debug_vs, debug_ps, gd.srv_pred.get(), gd.tex_smaa_out_rtv.get(), w, h, false);
               debug_state.Restore(native_device_context);
               native_device_context->CopyResource(color_res.get(), gd.tex_smaa_out.get());
               return DrawOrDispatchOverrideType::Replaced;
            }
         }
#endif

         // --- SMAA (3 passes) into the temp RTV, then copy into the swapchain target. ---
         if (smaa)
         {
            // Bind metrics at VS+PS b1 (DrawSMAA restores VS/PS/SRVs/RTs but NOT cbuffer slots).
            ComPtr<ID3D11Buffer> vs_cb1_orig, ps_cb1_orig;
            native_device_context->VSGetConstantBuffers(1, 1, vs_cb1_orig.put());
            native_device_context->PSGetConstantBuffers(1, 1, ps_cb1_orig.put());
            ID3D11Buffer* mcb = gd.cb_smaa_metrics.get();
            native_device_context->VSSetConstantBuffers(1, 1, &mcb);
            native_device_context->PSSetConstantBuffers(1, 1, &mcb);

            // Pass depth for predication only when valid (same-size, captured this frame); otherwise null + the
            // pred_scale=1.0 baked into the metrics CB make SMAA run as plain ULTRA instead of degraded predication.
            DrawSMAA(native_device, native_device_context, device_data,
               gd.tex_smaa_out_rtv.get(), gd.srv_input_linear.get(), gd.srv_input.get(),
               pred_ok ? gd.srv_pred.get() : nullptr /*predication (plane-deviation edge-ness)*/);

            ID3D11Buffer* vcb = vs_cb1_orig.get();
            ID3D11Buffer* pcb = ps_cb1_orig.get();
            native_device_context->VSSetConstantBuffers(1, 1, &vcb);
            native_device_context->PSSetConstantBuffers(1, 1, &pcb);
         }

         // --- Optional RCAS sharpen on the SMAA output, then copy into the swapchain target. ---
         // SMAA output -> RCAS -> tex_rcas_out -> color_res. If sharpening is off or anything isn't ready, copy the
         // SMAA output straight through (never leave the swapchain unwritten on a Replaced dispatch).
         auto* sharpen_vs = FindShader(device_data.native_vertex_shaders, CompileTimeStringHash("Copy VS"));
         auto* sharpen_ps = FindShader(device_data.native_pixel_shaders, CompileTimeStringHash("BL Sharpen PS"));
         bool do_sharpen = g_rcas_sharpness > 0.f && sharpen_vs != nullptr && sharpen_ps != nullptr;
         if (do_sharpen)
         {
            // (Re)create the RCAS CB on resolution/sharpness change.
            if (!gd.cb_sharpen || gd.sharpen_amount != g_rcas_sharpness)
            {
               const float sp[4] = {(float)w, (float)h, g_rcas_sharpness, 0.f};
               if (CreateImmutableCB(native_device, sp, sizeof(sp), gd.cb_sharpen))
                  gd.sharpen_amount = g_rcas_sharpness;
            }
            // (Re)create the RCAS output temp (fp16, RTV+SRV-capable) on resolution change.
            if (!gd.tex_rcas_out && CreateDefaultTex(native_device, w, h, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET, gd.tex_rcas_out))
               native_device->CreateRenderTargetView(gd.tex_rcas_out.get(), nullptr, gd.tex_rcas_out_rtv.put());
            if (!gd.cb_sharpen || !gd.tex_rcas_out_rtv)
               do_sharpen = false;
         }

         if (do_sharpen)
         {
            // DrawCustomPixelShader does NOT restore state -> wrap in core's DrawStateStack<FullGraphics>.
            DrawStateStack<DrawStateStackType::FullGraphics> sharpen_state;
            sharpen_state.Cache(native_device_context, device_data.uav_max_count);

            ID3D11Buffer* scb = gd.cb_sharpen.get();
            native_device_context->PSSetConstantBuffers(0, 1, &scb);
            DrawCustomPixelShader(native_device_context, device_data.default_depth_stencil_state.get(), device_data.default_blend_state.get(), nullptr,
               sharpen_vs, sharpen_ps, gd.tex_smaa_out_srv.get(), gd.tex_rcas_out_rtv.get(), w, h, false);

            sharpen_state.Restore(native_device_context);

            native_device_context->CopyResource(color_res.get(), gd.tex_rcas_out.get());
         }
         else
         {
            native_device_context->CopyResource(color_res.get(), gd.tex_smaa_out.get());
         }

         return DrawOrDispatchOverrideType::Replaced; // cancel the FXAA resolve dispatch
      }
#endif // ENABLE_SMAA

      return DrawOrDispatchOverrideType::None;
   }

   void UpdateLumaInstanceDataCB(CB::LumaInstanceDataPadded& data, CommandListData& cmd_list_data, DeviceData& device_data) override
   {
      // The sub-rect the engine draws, as a share of the output. DLSS / FSR below ScreenPercentage 100 made the scene output sized: the
      // uber covers its whole target and reads the upscaled scene ("SceneUVScale" undoes its sub-rect UVs), and the stretch copies 1:1.
      const uint2 render_size = GetScreenPercentageRenderSize(device_data.output_resolution);
      const float2 share = {float(render_size.x) / device_data.output_resolution.x, float(render_size.y) / device_data.output_resolution.y};
      const bool upscaled = g_screen_percentage < 1.f && device_data.has_drawn_sr;
      data.GameData.ScreenPercentageScale = upscaled ? float2{1.f, 1.f} : share;
      data.GameData.SceneUVScale = upscaled ? float2{1.f / share.x, 1.f / share.y} : float2{1.f, 1.f};
      data.GameData.BlurUVScale = GetGameDeviceData(device_data).sp_post_output_res ? float2{1.f / share.x, 1.f / share.y} : float2{1.f, 1.f};
   }

   void OnPresent(ID3D11Device* native_device, DeviceData& device_data) override
   {
      auto& gd = GetGameDeviceData(device_data);

      // DLSS / FSR: the upscaler's history restarts after any frame it didn't draw (menus, loading, just picked); the selection and the
      // motion vector state are fixed here for the next frame (see "IsSRActive")
      device_data.force_reset_sr = !device_data.has_drawn_sr;
      device_data.has_drawn_sr = false;
      gd.sr_active = device_data.sr_type != SR::Type::None && !device_data.sr_suppressed;
      gd.mv_active = IsSRActive(device_data) || g_mv_enable;
      gd.sp_post_output_res = false;
      // Render scale: the engine renders every view into a top-left sub-rect of its full size targets, so ScreenPercentage applies live.
      // It is rewritten whenever it differs (the game may restore its own).
      // ponytail: the game thread can run a frame ahead of the renderer, so the first frame after a change may mismatch Luma's sub-rect
      // (the upscaler restarts); latch the engine's actual view size if that frame shows
      if (g_engine_screen_percentage)
      {
         const float screen_percentage = gd.sr_active ? std::round(g_render_scale * 100.f) : 100.f;
         if (*g_engine_screen_percentage != screen_percentage)
         {
            *g_engine_screen_percentage = screen_percentage;
            device_data.force_reset_sr = true;
         }
         g_screen_percentage = screen_percentage / 100.f;
      }
      // A scene no post pass ended ends here: its jitter must not reach the next frame's draws before the prepass
      gd.mv_scene_open = false;
      gd.mv_scene_done = false;
      gd.mv_frame_ended = true;
      gd.sp_stretched = false;
#if DEVELOPMENT
      g_sp_canvas_kinds_last = std::move(g_sp_canvas_kinds);
      g_sp_canvas_kinds.clear();
#endif
      gd.mv_fill_pending = false;
      if (!custom_texture_mip_lod_bias_offset)
      {
         const std::unique_lock lock(s_mutex_samplers);
         // -1 at native resolution (Core biases the anisotropic samplers, all of the game's with the AF16x upgrade)
         device_data.texture_mip_lod_bias_offset = IsSRActive(device_data) ? SR::GetMipLODBias(float(GetScreenPercentageRenderSize(device_data.output_resolution).y), device_data.output_resolution.y) : 0.f;
      }
#if DEVELOPMENT
      gd.mv_last_stats = std::exchange(gd.mv_stats, {});
      // The DEV panel's counts in ReShade.log every 300 frames while motion vectors run
      if (const auto& stats = gd.mv_last_stats; gd.mv_active && cb_luma_global_settings.FrameIndex % 300 == 0)
         reshade::log::message(reshade::log::level::info, std::format("[BL MV] frame {}: {} mv ({} matched, {} camera only, {} other camera, {} uncopied), {} jitter, {} updates, sr {} ({}), ended by 0x{:08X}, refused {}/{}/{}/{}/{}/{}/{}/{}", cb_luma_global_settings.FrameIndex, stats.motion_vector_draws, stats.matched, stats.camera_only, stats.other_camera, stats.uncopied, stats.jitter_draws, stats.updates, stats.sr_draws, int(device_data.sr_type), stats.ended_by, stats.rejected[0], stats.rejected[1], stats.rejected[2], stats.rejected[3], stats.rejected[4], stats.rejected[5], stats.rejected[6], stats.rejected[7]).c_str());
      // "MV Debug View": Core's debug draw of the target, absolute values in pixels
      {
         const std::shared_lock lock(gd.mv_mutex);
         if (g_mv_debug_view && gd.mv_texture)
         {
            D3D11_TEXTURE2D_DESC desc;
            gd.mv_texture->GetDesc(&desc);
            debug_draw_auto_clear_texture = false;
            debug_draw_options |= (uint32_t)DebugDrawTextureOptionsMask::Abs | (uint32_t)DebugDrawTextureOptionsMask::UVToPixelSpace;
            device_data.debug_draw_texture = gd.mv_texture.get();
            device_data.debug_draw_texture_format = desc.Format;
            device_data.debug_draw_texture_size = {desc.Width, desc.Height, 1, 1};
         }
         else if (device_data.debug_draw_texture && device_data.debug_draw_texture.get() == gd.mv_texture.get())
         {
            device_data.debug_draw_texture = nullptr;
         }
      }
#endif

      // Leak fix: detect in OnInitResource (any thread), release here (present thread, no create in flight).
      BLMovieLeakFix::g_frame++;
      if (g_fix_movie_leak && (BLMovieLeakFix::g_frame % BLMovieLeakFix::RELEASE_FLUSH_PERIOD) == 0)
         BLMovieLeakFix::Flush();

      // One-shot telemetry: warn if a non-matching build tagged nothing (otherwise a silent no-op).
      if (g_fix_movie_leak && !BLMovieLeakFix::g_build_checked && BLMovieLeakFix::g_frame >= BLMovieLeakFix::BUILD_CHECK_FRAME)
      {
         BLMovieLeakFix::g_build_checked = true;
         if (!BLMovieLeakFix::g_have_movie.load())
            reshade::log::message(reshade::log::level::warning,
               "[BL-Leak] no movie buffers detected after warmup -- game build may differ from the one this leak fix targets; the fix is inactive (RAM-growth crash not mitigated).");
      }

      // Predication inputs are captured per-frame at the cel pass; drop them every present so a frame without that
      // pass (menu/transition/reorder) uses NO predication, not last frame's (possibly wrong-size) depth.
      gd.srv_depth.reset();
      gd.cb_game_offsets.reset();

      // XeGTAO inputs are captured per-frame at the deinterleave/coarse passes; disarm the takeover + drop the
      // captured SRVs every present so a frame without the AO chain (AO off in-game / transition) can't apply a
      // stale AO buffer.
      gd.srv_gtao_depth.reset();
      gd.srv_gtao_normals.reset();

      device_data.has_drawn_main_post_processing = true;
   }

   void LoadConfigs() override
   {
      reshade::get_config_value(nullptr, NAME, "SMAAEnable", g_smaa_enable);
      reshade::get_config_value(nullptr, NAME, "RCASSharpness", g_rcas_sharpness);
      reshade::get_config_value(nullptr, NAME, "RenderScale", g_render_scale);
      g_render_scale = std::clamp(g_render_scale, min_render_scale, 1.f);
      // Predication has no shipping UI (see DrawImGuiSettings), but stays overridable from the ini.
      reshade::get_config_value(nullptr, NAME, "SMAAPredication", g_smaa_predication);
      reshade::get_config_value(nullptr, NAME, "SMAAPredicationTolerance", g_smaa_pred_tolerance);
      reshade::get_config_value(nullptr, NAME, "XeGTAOEnable", g_gtao_enable);
      // Grade sliders (cb_luma_global_settings_dirty is already true at init -> uploaded on first frame).
      reshade::get_config_value(nullptr, NAME, "Exposure", cb_luma_global_settings.GameSettings.Exposure);
      reshade::get_config_value(nullptr, NAME, "Saturation", cb_luma_global_settings.GameSettings.Saturation);
      reshade::get_config_value(nullptr, NAME, "HighlightDechroma", cb_luma_global_settings.GameSettings.HighlightDechroma);
      reshade::get_config_value(nullptr, NAME, "BloomIntensity", cb_luma_global_settings.GameSettings.BloomIntensity);
      reshade::get_config_value(nullptr, NAME, "Contrast", cb_luma_global_settings.GameSettings.Contrast);
      reshade::get_config_value(nullptr, NAME, "Dithering", cb_luma_global_settings.GameSettings.Dithering);
      reshade::get_config_value(nullptr, NAME, "FlareOut", cb_luma_global_settings.GameSettings.FlareOut);
      reshade::get_config_value(nullptr, NAME, "VideoAutoHDREnable", cb_luma_global_settings.GameSettings.VideoAutoHDREnable);
      reshade::get_config_value(nullptr, NAME, "VideoAutoHDRBoost", cb_luma_global_settings.GameSettings.VideoAutoHDRBoost);
      reshade::get_config_value(nullptr, NAME, "HideUI", g_hide_ui);
      reshade::get_config_value(nullptr, NAME, "FixMovieLeak", g_fix_movie_leak);
   }

   void DrawImGuiSettings(DeviceData& device_data) override
   {
      ImGui::SeparatorText("Anti-Aliasing");
      const bool sr_active = IsSRActive(device_data);
      ImGui::BeginDisabled(!sr_active || !g_engine_screen_percentage);
      int render_scale = int(std::round(g_render_scale * 100.f));
      if (ImGui::SliderInt("Render Scale (%)", &render_scale, int(min_render_scale * 100.f), 100, "%d%%", ImGuiSliderFlags_AlwaysClamp))
         g_render_scale = float(render_scale) / 100.f;
      if (ImGui::IsItemDeactivatedAfterEdit())
         reshade::set_config_value(nullptr, NAME, "RenderScale", g_render_scale);
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
         ImGui::SetTooltip("The resolution the game renders at, upscaled by DLSS/FSR.");
      DrawResetButton(g_render_scale, 1.f, "RenderScale");
      ImGui::EndDisabled();

      // The upscaler (Super Resolution, in the Settings tab) replaces SMAA: shown off, the saved choice is kept
      ImGui::BeginDisabled(sr_active);
      bool smaa_shown = g_smaa_enable && !sr_active;
      if (ImGui::Checkbox("SMAA Enable", sr_active ? &smaa_shown : &g_smaa_enable))
         reshade::set_config_value(nullptr, NAME, "SMAAEnable", g_smaa_enable);
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
         ImGui::SetTooltip("Replaces the game's FXAA with SMAA (requires AA enabled in the game's video settings; not used with DLSS/FSR).");
      ImGui::EndDisabled();
      ImGui::BeginDisabled(!g_smaa_enable && !sr_active);
      ImGui::SliderFloat("RCAS Sharpness", &g_rcas_sharpness, 0.f, 1.f); // updates live; persist on release
      if (ImGui::IsItemDeactivatedAfterEdit())
         reshade::set_config_value(nullptr, NAME, "RCASSharpness", g_rcas_sharpness);
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
         ImGui::SetTooltip("Sharpening applied on top of SMAA or DLSS/FSR (0 = off; requires AA enabled in the game's video settings).");
#if DEVELOPMENT
      // Predication is not a preference: it only relaxes the edge threshold back to base ULTRA on geometry and
      // never below, so off is strictly worse. Kept as a bisect switch for devs, shipped on and out of sight.
      if (ImGui::Checkbox("SMAA Predication", &g_smaa_predication))
         reshade::set_config_value(nullptr, NAME, "SMAAPredication", g_smaa_predication);
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Finds edges by geometry (scene depth) instead of by brightness alone.\nKeeps texture noise from being antialiased while still catching real silhouettes.");
      if (ImGui::SliderFloat("SMAA Predication Tolerance", &g_smaa_pred_tolerance, 0.002f, 0.2f, "%.3f", ImGuiSliderFlags_Logarithmic))
         reshade::set_config_value(nullptr, NAME, "SMAAPredicationTolerance", g_smaa_pred_tolerance);
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("How far a surface may deviate from its local plane before it counts as an edge,\nas a fraction of view depth. Lower = more edges. This is the calibration lever,\nnot the SMAA threshold. Logarithmic: the parameter is relative.");
      ImGui::Checkbox("SMAA Predication Debug View", &g_smaa_pred_debug);
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Show the predication mask (red) instead of the frame.\nWant: black on flat surfaces, red across silhouettes.\nAll red = tolerance too low (predication is doing nothing).\nAll black = too high (silhouettes never regain sensitivity).");
      if (ImGui::Button("Measure Predication"))
         g_smaa_pred_measure = true;
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Read the mask back and log its distribution to ReShade.log.\nStand still, set a tolerance, press; repeat per value and compare the lines.\nFIRES(>0.5) is the share of the frame that regains base sensitivity.");
#endif
      ImGui::EndDisabled();

      // --- Grade (read in Luma_BL_Tonemap.hlsl via LumaSettings.GameSettings) ---
      auto& gs = cb_luma_global_settings.GameSettings;
      const auto slider = [&](const char* label, float* value, float default_value, const char* key, float max_value, const char* tooltip)
      {
         if (ImGui::SliderFloat(label, value, 0.f, max_value))
         {
            reshade::set_config_value(nullptr, NAME, key, *value);
            device_data.cb_luma_global_settings_dirty = true;
         }
         if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", tooltip);
         if (DrawResetButton(*value, default_value, key))
            device_data.cb_luma_global_settings_dirty = true;
      };
      ImGui::SeparatorText("Grade");
      slider("Exposure", &gs.Exposure, default_luma_global_game_settings.Exposure, "Exposure", 2.f, "Overall image brightness (1 = vanilla).");
      slider("Contrast", &gs.Contrast, default_luma_global_game_settings.Contrast, "Contrast", 2.f, "Overall image contrast, HDR only (1 = vanilla).");
      slider("Saturation", &gs.Saturation, default_luma_global_game_settings.Saturation, "Saturation", 2.f, "Color saturation, HDR only (1 = vanilla).");
      slider("Highlights Desaturation", &gs.HighlightDechroma, default_luma_global_game_settings.HighlightDechroma, "HighlightDechroma", 1.f, "How far the brightest sources fade to neutral white, HDR only (0 = keep color at any brightness).");

      ImGui::SeparatorText("Bloom");
      slider("Bloom Intensity", &gs.BloomIntensity, default_luma_global_game_settings.BloomIntensity, "BloomIntensity", 2.f, "Bloom strength (1 = vanilla, 0 = none).");

      // --- Ambient Occlusion: XeGTAO replaces the game's native HBAO+ ---
      ImGui::SeparatorText("Ambient Occlusion");
      if (ImGui::Checkbox("XeGTAO Enable", &g_gtao_enable))
         reshade::set_config_value(nullptr, NAME, "XeGTAOEnable", g_gtao_enable);
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Replaces the game's HBAO+ with XeGTAO (cleaner, more accurate ambient occlusion).");
#if DEVELOPMENT || TEST
      // DEVELOPMENT/TEST calibration only (drive cb_gtao, recreated on change at the deinterleave hook). Not persisted.
      ImGui::BeginDisabled(!g_gtao_enable);
      ImGui::SliderFloat("GTAO Final Value Power", &g_gtao_final_value_power, 0.3f, 4.5f);                 // midtone-shadow contrast dial
      ImGui::SliderFloat("GTAO Depth Scale", &g_gtao_depth_scale, 1.f, 200.f);                             // UE3 units -> ~meters; the anti-over-occlusion dial
      ImGui::SliderFloat("GTAO Radius Override", &g_gtao_radius_override, 0.f, 5.f);                       // 0 = use EFFECT_RADIUS
#if DEVELOPMENT                                                                                            // shader DebugViewRT blocks are #if DEVELOPMENT — don't draw a dead combo in TEST
      ImGui::Combo("GTAO Debug View", &g_gtao_debug_view, "Off\0Depth gradient\0Normals\0AO x8\0Edges\0"); // diagnostics through the AO apply
#endif
      ImGui::EndDisabled();
#endif

      // --- Post-effect scales + output toggle ---
      ImGui::SeparatorText("Effects");

      slider("Lens Flare Intensity", &gs.FlareOut, default_luma_global_game_settings.FlareOut, "FlareOut", 1.f, "Lens-flare / glare strength (1 = vanilla, 0 = off).");

      bool video_auto_hdr = gs.VideoAutoHDREnable > 0.5f;
      if (ImGui::Checkbox("Video AutoHDR", &video_auto_hdr))
      {
         gs.VideoAutoHDREnable = video_auto_hdr ? 1.f : 0.f;
         reshade::set_config_value(nullptr, NAME, "VideoAutoHDREnable", gs.VideoAutoHDREnable);
         device_data.cb_luma_global_settings_dirty = true;
      }
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Adds HDR highlights to pre-rendered videos (HDR only).");

      ImGui::BeginDisabled(!video_auto_hdr);
      if (ImGui::SliderFloat("Video HDR Boost", &gs.VideoAutoHDRBoost, 0.f, 1.f))
         device_data.cb_luma_global_settings_dirty = true;
      if (ImGui::IsItemDeactivatedAfterEdit())
         reshade::set_config_value(nullptr, NAME, "VideoAutoHDRBoost", gs.VideoAutoHDRBoost);
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
         ImGui::SetTooltip("Video highlight strength (0 = off).");
      if (DrawResetButton(gs.VideoAutoHDRBoost, default_luma_global_game_settings.VideoAutoHDRBoost, "VideoAutoHDRBoost"))
         device_data.cb_luma_global_settings_dirty = true;
      ImGui::EndDisabled();

      bool dithering = gs.Dithering > 0.5f;
      if (ImGui::Checkbox("Dithering", &dithering))
      {
         gs.Dithering = dithering ? 1.f : 0.f;
         reshade::set_config_value(nullptr, NAME, "Dithering", gs.Dithering);
         device_data.cb_luma_global_settings_dirty = true;
      }
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Reduces gradient banding.");

      ImGui::SeparatorText("UI");
      if (ImGui::Checkbox("Hide Gameplay UI", &g_hide_ui))
         reshade::set_config_value(nullptr, NAME, "HideUI", g_hide_ui);
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Disables the in-game UI.");

      ImGui::SeparatorText("Fixes");
      if (ImGui::Checkbox("Fix Movie Memory Leak", &g_fix_movie_leak))
         reshade::set_config_value(nullptr, NAME, "FixMovieLeak", g_fix_movie_leak);
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Frees the loading/cutscene movie memory the game leaks on each transition (fixes the RAM-growth crash). Movies still play.");
      if (g_fix_movie_leak && !BLMovieLeakFix::g_have_movie.load() && BLMovieLeakFix::g_frame >= BLMovieLeakFix::BUILD_CHECK_FRAME)
         ImGui::TextColored(ImVec4(1.f, 0.6f, 0.f, 1.f), "Inactive: no movie buffers detected (game build may differ).");
#if DEVELOPMENT || TEST
      ImGui::Text("freed: %u textures, %.1f MB", BLMovieLeakFix::g_freed_tex, BLMovieLeakFix::g_freed_bytes / 1048576.0);
#endif
   }

#if DEVELOPMENT
   void DrawImGuiDevSettings(DeviceData& device_data) override
   {
      const auto& stats = GetGameDeviceData(device_data).mv_last_stats;
      ImGui::SeparatorText("Motion vector research");
      ImGui::Checkbox("MV Enable", &g_mv_enable);
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Draws the scene with the motion vector shaders without DLSS/FSR: camera and object motion (each draw finds its own\nprevious frame b0/b1/b3). The image must not change; the debug view is black with a static camera and lights up only\nwhat moves. ReShade.log: patched/refused shaders. Not saved.");
      ImGui::Checkbox("MV Force Jitter", &g_mv_force_jitter);
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Jitters the scene (Halton 2/3, 8 phases) without an upscaler, with MV Enable. The image shakes by a subpixel; nothing\nmay flicker or lose pixels, and the debug view stays black with a static camera. Not saved.");
      ImGui::Checkbox("MV Disable Jitter", &g_mv_disable_jitter);
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("No projection jitter under DLSS/FSR (the upscaler gets zero jitter): isolates artifacts that come from the jitter. Not saved.");
      ImGui::Checkbox("Skip Cel Outlines", &g_skip_outlines);
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Skips the world and weapon outline passes (Sobel on the jittered depth): A/B of distant thin-geometry flicker.\nThe mip LOD bias A/B is Core's \"Custom Texture Samplers Mip LOD Bias\". Not saved.");
      ImGui::Checkbox("SR Reversible Tonemap", &g_sr_reversible_tonemap);
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Runs DLSS/FSR (in their SDR mode) on c / (1 + max(c)) and undoes it after: A/B of distant thin geometry flickering\nagainst the bright sky. Not saved.");
      ImGui::Checkbox("MV Debug View", &g_mv_debug_view);
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Shows the motion vector target (absolute, in pixels) through Core's debug draw.");
      ImGui::Text("Last frame: %u motion vector draws (%u matched, %u camera only, %u other camera, %u uncopied), %u jitter draws", stats.motion_vector_draws, stats.matched, stats.camera_only, stats.other_camera, stats.uncopied, stats.jitter_draws);
      ImGui::Text("Constant updates copied: %u, upscaler draws: %u, scene ended by 0x%08X", stats.updates, stats.sr_draws, stats.ended_by);
      ImGui::Text("Refused: %u extra target, %u no scene, %u other depth/color, %u format, %u size, %u create, %u blend, %u shaders", stats.rejected[0], stats.rejected[1], stats.rejected[2], stats.rejected[3], stats.rejected[4], stats.rejected[5], stats.rejected[6], stats.rejected[7]);
      ImGui::Text("Last checked target: format %u, dimension %u, %ux%u (output %.0fx%.0f)", stats.rejected_format, stats.rejected_dimension, stats.rejected_width, stats.rejected_height, double(device_data.output_resolution.x), double(device_data.output_resolution.y));

      ImGui::SeparatorText("Render scale research");
      ImGui::Text("ScreenPercentage %.2f", g_screen_percentage);
      ImGui::Checkbox("SP Fix HUD Canvas", &g_sp_fix_hud_canvas);
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Moves the Canvas draws with a HUD transform (the crosshair) after the stretch back by the centred view origin UE3 gives them. Not saved.");
      for (const auto& [kind, count] : g_sp_canvas_kinds_last)
         ImGui::Text("Canvas last frame: %u x %s", count, kind.c_str());
      bool changed = ImGui::Checkbox("SP Unscaled Render Target Views", &g_sp_unscaled_render_target_views);
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Skips ScreenPercentage for the scene views rendered into render targets (the inventory item icons). Not saved.");
      for (size_t i = 0; i < std::size(ScaleScreenCoordsPatch::kOtherCalls); i++)
         changed |= ImGui::Checkbox(std::format("SP Unscaled Call 0x{:X}", ScaleScreenCoordsPatch::kOtherCalls[i]).c_str(), &g_sp_unscaled_other_calls[i]);
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Skips ScreenPercentage at one ScaleScreenCoords call site (see ScaleScreenCoordsPatch). Not saved.");
      if (changed)
         ScaleScreenCoordsPatch::Apply(g_sp_unscaled_render_target_views, g_sp_unscaled_other_calls);
   }
#endif

   void PrintImGuiAbout() override
   {
      ImGui::PushTextWrapPos(0.f);
      ImGui::Text(
         "Luma for \"Borderlands GOTY Enhanced\" is developed by DristoforColumb and is open source and free.\n"
         "It adds HDR, DLSS or FSR 3 native anti-aliasing (DLAA), and replaces the game's FXAA with SMAA and its HBAO+ with XeGTAO, plus 16x anisotropic filtering and a fix for the game's movie memory leak.\n"
         "Enable Anti-Aliasing and Ambient Occlusion in the game's video settings for SMAA and XeGTAO to apply.\n"
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
                  "\nMacLeod-Boynton hue emulation (RenoDX)"
                  "\nSMAA (Iryoku)"
                  "\nXeGTAO (Intel)"
                  "\nAMD FidelityFX (RCAS + FSR 3)"
                  "\nNVIDIA NGX (DLSS)");
   }
};

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
   if (ul_reason_for_call == DLL_PROCESS_ATTACH)
   {
      const char* project_name = PROJECT_NAME;
      const char* cleared_project_name = (project_name[0] == '_') ? (project_name + 1) : project_name;

      uint32_t mod_version = 3; // a bump resets stale shader-define slots and cached settings/shaders
      Globals::SetGlobals(cleared_project_name, "Borderlands GOTY Enhanced Luma HDR + SMAA mod", "", mod_version);

      // Native HDR: swapchain -> scRGB fp16; core Display Composition does the paper-white scale + scRGB encode +
      // gamut map at present. Replaced tonemap PS writes gamma-encoded HDR (1.0 = paper white) into the (now fp16) post chain.
      swapchain_format_upgrade_type = TextureFormatUpgradesType::AllowedEnabled;
      swapchain_upgrade_type = SwapchainUpgradeType::scRGB; // r8g8b8a8_unorm backbuffer -> r16g16b16a16_float
      texture_format_upgrades_type = TextureFormatUpgradesType::AllowedEnabled;
      // Safety minimum: the remaster already renders its post chain in fp16. r10g10b10a2 covers any 10-bit target at
      // swapchain size (the backbuffer itself is r8g8b8a8); r11g11b10_float includes the HBAO+ view normals XeGTAO
      // reads. r8/b8 formats are left alone deliberately - nothing downstream needs them, and _srgb -> fp16 risks a
      // sampling shift.
      texture_upgrade_formats = {
         reshade::api::format::r10g10b10a2_unorm,
         reshade::api::format::r10g10b10a2_typeless,
         reshade::api::format::r11g11b10_float,
      };
      texture_format_upgrades_2d_size_filters = 0 | (uint32_t)TextureFormatUpgrades2DSizeFilters::SwapchainResolution | (uint32_t)TextureFormatUpgrades2DSizeFilters::SwapchainAspectRatio;
      force_disable_display_composition = false; // core composition does the scRGB encode + paper white

      // AF16x: mode 4 upgrades the game's AF samplers to MaxAnisotropy=16 (clarity on oblique surfaces).
      // LOD bias offset 0 without DLSS/FSR (no TAA: a negative bias would shimmer), set per frame with them (see OnPresent).
      enable_samplers_upgrade = true; // boot-time only
      samplers_upgrade_mode = 4;

      game = new BorderlandsGoty();
   }

   CoreMain(hModule, ul_reason_for_call, lpReserved);

   // Leak-fix resource hooks: register AFTER CoreMain's reshade::register_addon. Multiple callbacks per
   // event are allowed (coexist with core); CoreMain's DLL_PROCESS_DETACH unregister_addon removes them.
   if (ul_reason_for_call == DLL_PROCESS_ATTACH)
   {
      BLMovieLeakFix::g_exe_base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
      reshade::register_event<reshade::addon_event::init_resource>(BLMovieLeakFix::OnInitResource);
      reshade::register_event<reshade::addon_event::destroy_resource>(BLMovieLeakFix::OnDestroyResource);
      reshade::register_event<reshade::addon_event::init_resource_view>(BLMovieLeakFix::OnInitResourceView);
      reshade::register_event<reshade::addon_event::destroy_resource_view>(BLMovieLeakFix::OnDestroyResourceView);
   }

   return TRUE;
}
