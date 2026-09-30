// Borderlands GOTY Enhanced — Luma HDR + SMAA mod (Unreal Engine 3.5, D3D11).
// - HDR: swapchain -> scRGB fp16; replaced UE3 final-color PS (0xB030BAA6 / 0xFE88487E) runs the game's own grade
//   as an extended HDR function, takes its hue from a soft ReinhardPiecewise reference through MacLeod-Boynton
//   emulation, then DICE-maps to the display. The video (0x0E97A4A0) and lens-flare (0x010371F2) passes are
//   replaced too. Core Display Composition does the paper-white scale + encode.
//   One HDR mod owns the swapchain -> any other HDR mod must be removed from the game folder.
// - AA: compute FXAA (3.11 work-queue) -> SMAA (ULTRA + color edge + depth predication) + optional RCAS. Edge
//   detection reads the scene as stored, in GAMMA space (POST_PROCESS_SPACE_TYPE 0, 1.0 = paper white); the blend
//   filters it in linear light. The last pass writes the swapchain.
// - DLSS / FSR 3 (DLAA): the game renders no motion vectors and no jitter; both come from patched shaders
//   (MotionVectorPatches.h). The upscaler runs before the DOF/Bloom gather, SMAA then steps aside (RCAS stays).
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
#include "..\..\Core\includes\patched_draws.h"
#include <shellapi.h> // ShellExecuteA for About links (system() hangs the render thread in exclusive fullscreen)

// FXAA is a compute work-queue implementation (FXAA 3.11 CS), four dispatches in this order:
//   0x81CDE53D = edge detection: a work queue per edge orientation, each pixel with its two colors packed as float16 (t0 = Color).
//   0x43A10668 = the indirect dispatch arguments from the queue counts.
//   0x78019A89 = resolve of one queue: blends each queued pixel's colors and writes Color (u0, the swapchain) in place.
//   0x08891303 = resolve of the other queue, the same: WorkQueue + Luma + InColor(t2) -> Color(u0, swapchain in-place).
// All four are skipped when SMAA or DLSS / FSR own the resolve (see "fxaa_replaced"): the last one is where SMAA / RCAS run. Left
// running, the first resolve's half of FXAA would reach SMAA's input and the upscaled image.
static constexpr uint32_t kFXAAEdgeHash = 0x81CDE53D;
static constexpr uint32_t kFXAAArgumentsHash = 0x43A10668;
static constexpr uint32_t kFXAAFirstResolveHash = 0x78019A89;
static constexpr uint32_t kFXAAResolveHash = 0x08891303; // Replaced with SMAA (RCAS only after DLSS / FSR)
static constexpr uint32_t kCelShadingHash = 0x08DC66D1;  // cel-shading edge PS — binds scene depth at t0 (predication source)

// AO: XeGTAO replaces the game's native NVIDIA HBAO+ (GFSDK_SSAO). Full-res 4K chain:
// deinterleave 0xFFE232A6 -> normals 0xB2B47225 (left running) -> coarse horizon 0xF534EB09 -> bilateral
// blur 0x4E1BEE34 -> apply-multiply PS 0x44764BF6. We capture scene depth at the deinterleave and the
// packed view normals at the coarse pass (skipping both), then at the blur dispatch run the 4 XeGTAO passes
// into ITS u0 (the game's FINAL r16g16_float AO; apply reads .x) so the apply blit composites our AO
// unchanged. XeGTAO reads the game's own cb0 ($Globals: ProjInfo) + cb2 (MinZ_MaxZRatioCS), still bound at
// the injection point. Noise frozen and denoise x2 without an upscaler, per frame and x1 with DLSS/FSR (see "IsGTAOTemporal").
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
static bool g_mv_force_jitter = false;   // The projection jitter without an upscaler
static bool g_mv_disable_jitter = false; // No projection jitter under the upscaler (A/B of jitter-dependent artifacts)
static bool g_mv_log_tiebreak = false;   // One-shot: the draws "PatchedDraws::CountTieBreakCollisions" counts, to ReShade.log
// A/B of the constant copies' CPU savings (see "MayBeRegisteredBuffer", "NewConstantsCopy", "PatchedShader::read_sizes")
static bool g_mv_buffer_filter = true;
static bool g_mv_constants_pool = true;
static bool g_mv_read_sizes = true;
#else
static constexpr bool g_mv_enable = false;
static constexpr bool g_mv_force_jitter = false;
static constexpr bool g_mv_disable_jitter = false;
static constexpr bool g_mv_buffer_filter = true;
static constexpr bool g_mv_constants_pool = true;
static constexpr bool g_mv_read_sizes = true;
#endif

#if DEVELOPMENT
// "Performance Test" (see "OnPresent"): the mode, and the anti-aliasing it sets while it runs (the user's is restored on "Off" or
// "Current Settings", and never saved)
static int g_perf_test = 0;
static bool g_perf_hook_timers = true; // The hooks' CPU time (two clock reads per hooked draw, themselves a cost to measure)
struct PerfTestMode
{
   const char* name;
   bool set_aa = false; // Else the current settings (the fields below too)
   SR::Type sr_type = SR::Type::None;
   unsigned int dlss_preset = 0; // NVSDK_NGX_DLSS_Hint_Render_Preset (5 = E, 11 = K, ...)
   bool smaa = false;
   int motion_vector_draws = 2; // 2 patched (motion vectors and jitter), 1 jitter only, 0 untouched
};
static constexpr PerfTestMode perf_test_modes[] = {
   {"Off"},
   {"Current Settings"},
   {"DLSS K", true, SR::Type::DLSS, 11},
   {"DLSS K Jitter Only", true, SR::Type::DLSS, 11, false, 1},
   {"DLSS K Without Motion Vector Draws", true, SR::Type::DLSS, 11, false, 0},
   {"DLSS L", true, SR::Type::DLSS, 12},
   {"DLSS M", true, SR::Type::DLSS, 13},
   {"DLSS E (CNN)", true, SR::Type::DLSS, 5},
   {"FSR 3", true, SR::Type::FSR},
   {"SMAA", true, SR::Type::None, 0, true},
   {"No AA", true, SR::Type::None, 0, false},
};
// "Sweep": these modes in turn, a few log windows each, over several rounds (interleaved, so the scene's drift averages out), then a
// median per mode
static bool g_perf_sweep = false;
static constexpr int perf_sweep_modes[] = {2, 3, 4, 10};
static_assert(std::string_view(perf_test_modes[perf_sweep_modes[0]].name) == "DLSS K" && std::string_view(perf_test_modes[perf_sweep_modes[std::size(perf_sweep_modes) - 1]].name) == "No AA");
static_assert(std::size(perf_test_modes) <= 32); // 5 bits in "perf_settings"
static constexpr int perf_sweep_rounds = 3;
static constexpr int perf_sweep_windows = 2; // Per mode and round
// "DrawWithMotionVectors" refusals, by "MV_REJECT" reason
static constexpr const char* mv_reject_names[] = {"extra_target", "no_scene", "other_depth_color", "format", "size", "create", "blend", "shaders"};
#endif

// SMAA's resources go after this many presents without it (the upscaler antialiasing, or the game's AA off): ~5 s, so menus and
// loading screens between upscaled frames, which run SMAA, don't recreate them each time
static constexpr uint32_t smaa_idle_release_frames = 600;

// User settings, persisted in the [Luma] config section (LoadConfigs) unless noted otherwise.
static bool g_smaa_enable = true;
static float g_rcas_sharpness = 0.f; // RCAS on the SMAA or DLSS / FSR output; off by default: the ink outlines are clean and sharpening haloes them
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

// Ambient Occlusion: XeGTAO replaces the native HBAO+, on by default. Persisted as "XeGTAOEnable".
static bool g_gtao_enable = true;
// Runtime XeGTAO knobs (LumaGTAO cb b11); their sliders are DEVELOPMENT/TEST only. FinalValuePower = primary darkness dial
// (calibrate to the vanilla HBAO+ histogram — its PowExponent does not transfer numerically). DepthScale =
// viewZ divisor (UE3 units, near plane ~10 -> ~meters) so Intel's tuned radius/falloff apply; the dial
// against broad over-occlusion. RadiusOverride > 0 overrides EFFECT_RADIUS (in scaled units).
static float g_gtao_final_value_power = 1.0f;
static float g_gtao_depth_scale = 50.f;
static float g_gtao_radius_override = 0.f;
#if DEVELOPMENT
static int g_gtao_temporal = 0;   // 0 = with DLSS/FSR (see "IsGTAOTemporal"), 1 = off, 2 = on
static int g_gtao_debug_view = 0; // 0 off, 1 depth gradient, 2 normals, 3 AO x8, 4 edges, through the game's apply blit (DebugViewRT is DEV only)
#else
static constexpr int g_gtao_temporal = 0;
static constexpr int g_gtao_debug_view = 0; // The shader's DebugViewRT is DEV only
#endif

// Loading-movie memory-leak fix ("Fix Movie Memory Leak" under Fixes). The game's Bink movies create D3D11 Y'CbCr decode buffers
// and never release them -> linear RAM growth -> OOM; the leak is the game's, not Luma's. We drop the game's leaked COM refs on OLD
// movie generations (orphaned: a movie's buffers are sampled only during its own playback), tagged by creation call-stack RVAs in
// BorderlandsGOTY.exe (frozen remaster; a non-matching build tags nothing = safe no-op). Movies keep playing. Diagnosed and
// validated with a resource tracker addon (live bytes by creation stack).
static bool g_fix_movie_leak = true; // default ON; persisted as "FixMovieLeak"

namespace BLMovieLeakFix
{
   // Build-specific RVAs (see above); BUILD_CHECK_FRAME drives a one-shot warning when a non-matching build tags nothing.
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
   // Coarse cross-thread gates (the present thread writes g_frame, workers read it); the maps are synchronized by g_mtx.
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
   com_ptr<ID3D11Buffer> cb_pred;
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

   // SMAA metrics CB (b1) = (1/w, 1/h, w, h) + (predication scale,0,0,0): 2.0 = valid depth (predication active), 1.0 = no/mismatched
   // depth (plain ULTRA threshold)
   com_ptr<ID3D11Buffer> cb_smaa_metrics;

   // The color size the SMAA, predication and RCAS resources were built at (see the FXAA resolve replacement)
   uint32_t smaa_w = 0, smaa_h = 0;

   // Scene-color snapshot (fp16, CopyResource'd from the swapchain each frame, the resolve being in place): SMAA's input (edge
   // detection, and the neighborhood blend in linear light), or RCAS's after DLSS / FSR
   ComPtr<ID3D11Texture2D> tex_input;
   ComPtr<ID3D11ShaderResourceView> srv_input;

   // SMAA output temp (fp16, SRV+RTV): RCAS's input when it follows SMAA, or copied into the swapchain target if that has no RTV
   ComPtr<ID3D11Texture2D> tex_smaa_out;
   ComPtr<ID3D11RenderTargetView> tex_smaa_out_rtv;
   ComPtr<ID3D11ShaderResourceView> tex_smaa_out_srv;

   // RCAS sharpen CB (b0) = (w, h, sharpness, 0)
   com_ptr<ID3D11Buffer> cb_sharpen;
   // RCAS output temp (fp16, RTV), copied into the swapchain target, only if that has no RTV
   ComPtr<ID3D11Texture2D> tex_rcas_out;
   ComPtr<ID3D11RenderTargetView> tex_rcas_out_rtv;
   // This FXAA chain is ours (SMAA or DLSS / FSR own its resolve): its passes are skipped, decided at its edge detection
   bool fxaa_replaced = false;
   // The frames SMAA, the snapshot's users (SMAA or RCAS), and SMAA's output temp (RCAS after SMAA, or no RTV on the target) were last
   // used, for "smaa_idle_release_frames"
   uint32_t smaa_frame = 0;
   uint32_t snapshot_frame = 0;
   uint32_t smaa_out_frame = 0;

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
   // LumaGTAO knob CB (b11) = (FinalValuePower, DepthScale, RadiusOverride, DebugView, NoiseIndex), written every frame
   com_ptr<ID3D11Buffer> cb_gtao;

   // "IsSRActive", taken at present: Core's "Super Resolution" selection changes after it, mid frame for the draws
   bool sr_active = false;
   // "DrawUpscaler" made a new upscaler output texture (see there)
   bool sr_output_recreated = false;
   // The upscaler's output, read in place of its input (the scene's last copy, else the scene) by the uber and the DOF/Bloom gather
   com_ptr<ID3D11ShaderResourceView> sr_output_srv;
   ID3D11Resource* sr_input = nullptr;
   // None was picked ("CleanExtraSRResources", from the overlay): the upscaler's resources go at the next present
   std::atomic<bool> release_sr_resources = false;

   // DLSS / FSR: the per pixel DOF blur amount accumulated over frames (Luma_BL_DOFGather.hlsl), ping-pong, and its constants (b5)
   com_ptr<ID3D11Texture2D> dof_history[2];
   com_ptr<ID3D11ShaderResourceView> dof_history_srvs[2];
   com_ptr<ID3D11UnorderedAccessView> dof_history_uavs[2];
   uint32_t dof_history_index = 0;
   bool dof_history_valid = false;
   com_ptr<ID3D11Buffer> dof_history_cb;

   // Motion vectors: shaders patched on first use, by original hash (null on failure), and the target (sized like the scene; every
   // blend state writes it unblended, see "OnCreateBlendState")
   std::atomic<bool> mv_active = false; // Motion vectors and jitter this frame: an upscaler is active, or the DEV toggle (set at present)
   std::shared_mutex mv_mutex;
   // A patched shader (null if refused) with, for a vertex shader, whether it reads b3 (skinned), the byte offset of its LocalToWorld
   // translation row in b0 (none for world space geometry, matched by draw key alone), and the bytes of b0 / b1 / b3 it reads
   // ("DXBC::ConstantBufferBytes": the previous frame's copies upload only those)
   template <typename T>
   struct PatchedShader
   {
      com_ptr<T> shader;
      bool skinned = false;
      uint32_t translation_offset = UINT_MAX;
      std::array<UINT, std::size(MotionVectorPatches::previous_slots)> read_sizes = {};
   };
   std::unordered_map<uint32_t, PatchedShader<ID3D11VertexShader>> mv_vertex_shaders;
   std::unordered_map<uint32_t, PatchedShader<ID3D11PixelShader>> mv_pixel_shaders;
   com_ptr<ID3D11Texture2D> mv_texture;
   com_ptr<ID3D11RenderTargetView> mv_rtv;
   com_ptr<ID3D11ShaderResourceView> mv_srv;  // Read by the DOF gather (its history)
   com_ptr<ID3D11UnorderedAccessView> mv_uav; // Null without typed UAV loads of its format (then no fill)
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
   // shader's patched versions (owned by "mv_vertex_shaders" / "mv_pixel_shaders", never erased).
   ID3D11DepthStencilView* jitter_dsv = nullptr;
   bool jitter_dsv_scene = false;
   ID3D11DepthStencilState* jitter_depth_stencil_state = nullptr;
   bool jitter_depth_test = true;
   ID3D11RenderTargetView* mv_accepted_rtv = nullptr;
   ID3D11DepthStencilView* mv_accepted_dsv = nullptr;
   ID3D11BlendState* mv_blend_state = nullptr;
   bool mv_blend_opaque = true;
   uint32_t mv_last_vertex_shader_hash = 0;
   const PatchedShader<ID3D11VertexShader>* mv_last_vertex_shader = nullptr;
   uint32_t mv_last_pixel_shader_hash = 0;
   ID3D11PixelShader* mv_last_pixel_shader = nullptr;
   PatchedDraws::BoundShader<ID3D11VertexShader> mv_bound_vertex_shader;
   PatchedDraws::BoundShader<ID3D11PixelShader> mv_bound_pixel_shader;

   // CPU copies of the b0 / b1 / b3 buffers the motion vector draws bind, by buffer (an entry registers it, null until its first
   // update): the engine uploads every one with UpdateSubresource (whole buffer, only when a constant changed), so a draw's constants
   // are its buffer's latest copy
   using ConstantsCopy = std::shared_ptr<const std::vector<uint8_t>>;
   std::shared_mutex mv_constants_mutex;
   std::unordered_map<uint64_t, ConstantsCopy> mv_constants_copies;
   // The first registered buffers and their sizes again, for the hook to skip the game's other buffers without the lock and the lookup
   // (see "MayBeRegisteredBuffer"). Written under "mv_constants_mutex"; with more registered, every buffer takes the lock.
   static constexpr uint32_t max_filtered_buffers = 32;
   static constexpr uint64_t destroyed_buffer_slot = 1; // A destroyed buffer's slot, reused by the next registration (never a handle)
   std::array<std::atomic<uint64_t>, max_filtered_buffers> mv_filtered_buffers = {};
   std::array<UINT, max_filtered_buffers> mv_filtered_buffer_sizes = {};
   std::atomic<uint32_t> mv_filtered_buffer_count = 0; // Every registered buffer, past "max_filtered_buffers" too
   // Every copy made, and by size those nobody held anymore at the last present (the next copies take them, see "NewConstantsCopy").
   // Every copy is in the pool, so one with a use count of 2 is held by "mv_constants_copies" and the pool alone. Under
   // "mv_constants_mutex".
   std::vector<std::shared_ptr<std::vector<uint8_t>>> mv_constants_pool;
   std::unordered_map<size_t, std::vector<uint32_t>> mv_constants_pool_free;
   size_t mv_constants_made = 0; // Copies asked for since the last present
   // Previous frame constants of the motion vector draws (see "PatchedDraws::PreviousConstants")
   PatchedDraws::PreviousConstants mv_previous_constants;
   // Motion vector draws by draw key (shaders, buffers, arguments), with the LocalToWorld translation and b0 / b1 / b3. A draw takes
   // the previous frame's constants of its key's nearest draw (same object, a frame earlier), its camera included: the first person
   // weapon draws with a camera of its own.
   struct MotionVectorObject
   {
      std::array<float, 3> translation;
      ConstantsCopy object; // b0
      ConstantsCopy camera; // b1
      ConstantsCopy bones;  // b3, skinned draws only
#if DEVELOPMENT
      uint32_t vertex_shader_hash = 0, pixel_shader_hash = 0, index_count = 0; // For the "mv_log_tiebreak" log
      // Its vertex shader reads LocalToWorld (else matched by draw key alone, its translation 0)
      bool translated = false;
#endif
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
      uint32_t tiebreak_collisions = 0;   // The previous frame's, see "PatchedDraws::CountTieBreakCollisions"
      uint32_t registered_buffers = 0;    // At present: the b0 / b1 / b3 buffers with CPU copies (see "MayBeRegisteredBuffer")
      uint32_t constants_pool = 0;        // At present: the pooled copies (see "NewConstantsCopy")
      uint32_t refused_region_copies = 0; // Region copies of the open scene or its depth that aren't whole (see "OverrideCopyTextureRegion")
      uint32_t fxaa_skipped = 0;          // FXAA dispatches skipped before the replaced resolve (see "fxaa_replaced")
      uint32_t ended_by = 0;
      uint32_t rejected[std::size(mv_reject_names)] = {};                                            // "DrawWithMotionVectors" refusals by reason ("MV_REJECT")
      uint32_t rejected_format = 0, rejected_dimension = 0, rejected_width = 0, rejected_height = 0; // The last target refused by format or size
   };
   MotionVectorStats mv_stats, mv_last_stats;
   uint32_t mv_destroyed_buffers = 0; // Registered buffers the game destroyed, in the session (see "OnDestroyResource")
   int mv_draw_reject = -1;           // The current draw's "MV_REJECT" reason (-1 for none), for the MCP trace note
   // "Performance Test": GPU timestamps per frame (present to present, the scene from the depth prepass to its first post pass, the
   // upscaler), all on the immediate context, in a ring read back a few frames later without waiting; and the CPU time in the
   // motion vector hooks. Inside them: the camera fill (the scene's end) and the replaced DOF/Bloom gather.
   struct PerfQueries
   {
      com_ptr<ID3D11Query> disjoint, frame_start, scene_start, fill_start, scene_end, sr_end, dof_start, dof_end, frame_end;
      bool scene_started = false; // scene_start issued
      bool fill = false;          // fill_start issued
      bool scene = false;         // ... and scene_end
      bool sr = false;            // ... and sr_end
      bool dof = false;           // dof_start and dof_end issued
      bool pending = false;
   };
   struct PerfStats
   {
      double frame_ms = 0.0, frame_max_ms = 0.0, scene_ms = 0.0, scene_max_ms = 0.0, sr_ms = 0.0, sr_max_ms = 0.0, cpu_frame_ms = 0.0;
      double fill_ms = 0.0, dof_ms = 0.0, unused_max_ms = 0.0;
      uint32_t samples = 0, scene_samples = 0, sr_samples = 0, disjoint = 0, frames = 0;
      uint32_t fill_samples = 0, dof_samples = 0;
   };
   std::array<PerfQueries, 8> perf_queries;
   size_t perf_query_index = 0;
   PerfQueries* perf_frame_queries = nullptr; // This frame's, from present to present
   PerfStats perf_stats;                      // This log window's
   int perf_settle_frames = 0;                // Frames skipped after a change (targets rebuilt, history reset)
   uint32_t perf_settings = 0;                // The measured settings, to restart the settle on a change
   std::chrono::steady_clock::time_point perf_last_present;
   std::atomic<int64_t> perf_hook_ns = 0; // This log window's
   // The user's anti-aliasing, while a mode that sets its own runs
   SR::Type perf_user_sr_type = SR::Type::None;
   unsigned int perf_user_dlss_preset = 0;
   bool perf_user_smaa = false;
   // "Sweep": the step over all rounds, the log windows done in it, and per mode each window's frame, scene, SR, hook, fill and DOF
   // gather times
   int perf_sweep_step = 0;
   int perf_sweep_windows_done = 0;
   std::vector<std::array<double, 6>> perf_sweep_results[std::size(perf_test_modes)];
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

   // SMAA's own resources (predication, output temp), apart from Core's ("ReleaseSMAA")
   void ReleaseSMAAScratch()
   {
      tex_pred.reset();
      uav_pred.reset();
      srv_pred.reset();
      tex_smaa_out.reset();
      tex_smaa_out_rtv.reset();
      tex_smaa_out_srv.reset();
   }

   // What SMAA and RCAS share: the snapshot, and RCAS's output temp
   void ReleaseSnapshotScratch()
   {
      tex_input.reset();
      srv_input.reset();
      tex_rcas_out.reset();
      tex_rcas_out_rtv.reset();
   }
};

class BorderlandsGoty final : public Game
{
   static BorderlandsGotyGameDeviceData& GetGameDeviceData(DeviceData& device_data)
   {
      return *static_cast<BorderlandsGotyGameDeviceData*>(device_data.game);
   }

#if DEVELOPMENT
   // "Performance Test": adds the scope's CPU time to the motion vector hooks' total, while the test runs
   struct PerfHookTimer
   {
      std::atomic<int64_t>& total_ns;
      const bool enabled = g_perf_test != 0 && g_perf_hook_timers;
      const std::chrono::steady_clock::time_point start = enabled ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
      ~PerfHookTimer()
      {
         if (enabled)
            total_ns += std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count();
      }
   };

   // A mode that sets an upscaler this GPU doesn't have can't run
   static bool IsPerfTestModeAvailable(const DeviceData& device_data, const PerfTestMode& mode)
   {
      return !mode.set_aa || mode.sr_type == SR::Type::None || device_data.sr_implementations_instances.contains(mode.sr_type);
   }
   // "Performance Test": switches to a mode, setting its anti-aliasing as Core's "Super Resolution" and "DLSS Preset" selection do
   // (without saving), keeping the user's while any mode that sets its own runs and restoring it after
   static void ApplyPerfTestMode(DeviceData& device_data, int mode_index)
   {
      auto& game_device_data = GetGameDeviceData(device_data);
      const PerfTestMode& mode = perf_test_modes[mode_index];
      const PerfTestMode& previous_mode = perf_test_modes[g_perf_test];
      if (!previous_mode.set_aa && mode.set_aa)
      {
         game_device_data.perf_user_sr_type = device_data.sr_type;
         game_device_data.perf_user_dlss_preset = dlss_render_preset;
         game_device_data.perf_user_smaa = g_smaa_enable;
      }
      if (mode.set_aa || previous_mode.set_aa)
      {
         device_data.sr_type = mode.set_aa ? mode.sr_type : game_device_data.perf_user_sr_type;
         device_data.sr_suppressed = false;
         dlss_render_preset = mode.set_aa && mode.sr_type == SR::Type::DLSS ? mode.dlss_preset : game_device_data.perf_user_dlss_preset;
         g_smaa_enable = mode.set_aa ? mode.smaa : game_device_data.perf_user_smaa;
      }
      g_perf_test = mode_index;
   }
#endif

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

   // A DEFAULT usage 2D texture (1 mip, 1 sample), fp16 unless "format" says otherwise. Resets "out".
   static bool CreateDefaultTex(ID3D11Device* device, uint32_t w, uint32_t h, UINT bind_flags, ComPtr<ID3D11Texture2D>* out, DXGI_FORMAT format = DXGI_FORMAT_R16G16B16A16_FLOAT)
   {
      out->reset();
      D3D11_TEXTURE2D_DESC td = {};
      td.Width = w;
      td.Height = h;
      td.MipLevels = 1;
      td.ArraySize = 1;
      td.Format = format;
      td.SampleDesc.Count = 1;
      td.Usage = D3D11_USAGE_DEFAULT;
      td.BindFlags = bind_flags;
      return SUCCEEDED(device->CreateTexture2D(&td, nullptr, out->put()));
   }

#if DEVELOPMENT
   // One-shot readback of the predication mask, to calibrate g_smaa_pred_tolerance from numbers rather than screenshots. It reports
   // COVERAGE at the level SMAA compares against (0.5) and the shape either side, not a mean: on a well-tuned frame the mask is 0
   // nearly everywhere. Every texel is read, as a 1px silhouette is exactly what a stride would step over. Copies on the press frame
   // and maps on a later one without waiting, so the press never stalls the render thread.
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

   // XeGTAO noise per frame and one denoise pass (Intel's XeGTAO.h with TAA) while DLSS / FSR accumulate the lit scene the AO multiplies
   // into; without them a moving pattern would boil, so it stays frozen and denoises twice
   static bool IsGTAOTemporal(DeviceData& device_data)
   {
      return g_gtao_temporal ? g_gtao_temporal == 2 : IsSRActive(device_data);
   }

   // An upscaler is picked and hasn't failed (it then gives way to SMAA until picked again). Fixed for the whole frame (see
   // "OnPresent"): a selection made after the motion vector state was set would otherwise run the upscaler on mixed state.
   static bool IsSRActive(DeviceData& device_data)
   {
      return GetGameDeviceData(device_data).sr_active;
   }

   // A b0 / b1 / b3 buffer's CPU copy (null until its first update); registers it for a copy at every update, and for the lock free
   // filter ("MayBeRegisteredBuffer") with its size. Under "mv_constants_mutex".
   static BorderlandsGotyGameDeviceData::ConstantsCopy GetConstantsCopy(BorderlandsGotyGameDeviceData* game_device_data, ID3D11Buffer* buffer)
   {
      if (!buffer)
         return nullptr;
      const auto [entry, registered] = game_device_data->mv_constants_copies.try_emplace(reinterpret_cast<uint64_t>(buffer));
      if (registered)
      {
         // A destroyed buffer's slot, else the next one (the size before the handle, which the hook reads first)
         const uint32_t count = game_device_data->mv_filtered_buffer_count.load(std::memory_order_relaxed);
         uint32_t slot = 0;
         while (slot < (std::min)(count, BorderlandsGotyGameDeviceData::max_filtered_buffers) && game_device_data->mv_filtered_buffers[slot].load(std::memory_order_relaxed) != BorderlandsGotyGameDeviceData::destroyed_buffer_slot)
            slot++;
         if (slot < BorderlandsGotyGameDeviceData::max_filtered_buffers)
         {
            D3D11_BUFFER_DESC desc;
            buffer->GetDesc(&desc);
            game_device_data->mv_filtered_buffer_sizes[slot] = desc.ByteWidth;
            game_device_data->mv_filtered_buffers[slot].store(reinterpret_cast<uint64_t>(buffer), std::memory_order_release);
         }
         // Past the list, the count still grows: every buffer then takes the lock
         if (slot >= count)
         {
            game_device_data->mv_filtered_buffer_count.store(count + 1, std::memory_order_release);
         }
      }
      return entry->second;
   }

   // False if the buffer surely isn't a registered b0 / b1 / b3 one; "size" its size if known (else 0). Lock free: the hook sees every
   // UpdateSubresource of the game (a few thousand a frame, most into other buffers).
   static bool MayBeRegisteredBuffer(const BorderlandsGotyGameDeviceData& game_device_data, uint64_t handle, UINT* size)
   {
      *size = 0;
      const uint32_t count = game_device_data.mv_filtered_buffer_count.load(std::memory_order_acquire);
      if (!g_mv_buffer_filter || count > BorderlandsGotyGameDeviceData::max_filtered_buffers)
         return true;
      for (uint32_t i = 0; i < count; i++)
      {
         if (game_device_data.mv_filtered_buffers[i].load(std::memory_order_acquire) == handle)
         {
            *size = game_device_data.mv_filtered_buffer_sizes[i];
            return true;
         }
      }
      return false;
   }

   // A b0 / b1 / b3 copy of "size" bytes from "bytes" (null: zeroed): one of that size nobody held anymore at the last present, else a
   // new one. Under "mv_constants_mutex".
   static std::shared_ptr<std::vector<uint8_t>> NewConstantsCopy(BorderlandsGotyGameDeviceData* game_device_data, const uint8_t* bytes, size_t size)
   {
      game_device_data->mv_constants_made++;
      if (g_mv_constants_pool)
      {
         if (const auto free_copies = game_device_data->mv_constants_pool_free.find(size); free_copies != game_device_data->mv_constants_pool_free.end() && !free_copies->second.empty())
         {
            auto copy = game_device_data->mv_constants_pool[free_copies->second.back()];
            free_copies->second.pop_back();
            if (bytes)
            {
               std::memcpy(copy->data(), bytes, size);
            }
            else
            {
               std::fill(copy->begin(), copy->end(), uint8_t(0));
            }
            return copy;
         }
      }
      auto copy = (bytes ? std::make_shared<std::vector<uint8_t>>(bytes, bytes + size) : std::make_shared<std::vector<uint8_t>>(size));
      // Pooled even without "g_mv_constants_pool", which only gates the reuse: the in-place rewrite relies on it (see "mv_constants_pool")
      game_device_data->mv_constants_pool.push_back(copy);
      return copy;
   }

   // Motion vectors: the CPU copy of a registered b0 / b1 / b3 buffer, from the game's UpdateSubresource (before it runs). The engine
   // uploads whole buffers (no box: "size" is UINT64_MAX); a partial update is merged into the last copy.
   static bool OnUpdateBufferRegion(reshade::api::device* device, const void* data, reshade::api::resource resource, uint64_t offset, uint64_t size)
   {
      DeviceData* const device_data = device->get_private_data<DeviceData>();
      if (!device_data || !device_data->game || !data)
         return false;
      auto& game_device_data = GetGameDeviceData(*device_data);
      if (!game_device_data.mv_active)
         return false;
#if DEVELOPMENT
      const PerfHookTimer timer{game_device_data.perf_hook_ns};
#endif
      UINT buffer_size = 0;
      if (!MayBeRegisteredBuffer(game_device_data, resource.handle, &buffer_size))
         return false;
      const std::lock_guard lock(game_device_data.mv_constants_mutex);
      const auto entry = game_device_data.mv_constants_copies.find(resource.handle);
      if (entry == game_device_data.mv_constants_copies.end())
         return false;
      if (buffer_size == 0)
      {
         D3D11_BUFFER_DESC desc;
         reinterpret_cast<ID3D11Buffer*>(resource.handle)->GetDesc(&desc);
         buffer_size = desc.ByteWidth;
      }
      if (offset >= buffer_size)
         return false;
      const size_t updated_size = size_t((std::min)(size, uint64_t(buffer_size) - offset));
      const auto* const bytes = static_cast<const uint8_t*>(data);
      // The buffer's last copy, rewritten in place when no draw or object holds it (only the map and the pool do, see
      // "mv_constants_pool"): most of the updates go to buffers no motion vector draw read since. Made non-const ("NewConstantsCopy"), so
      // writing it is defined.
      auto& copy = entry->second;
      if (g_mv_constants_pool && copy && copy.use_count() == 2 && copy->size() == buffer_size)
      {
         std::memcpy(const_cast<uint8_t*>(copy->data()) + offset, bytes, updated_size);
      }
      else if (updated_size == buffer_size)
      {
         copy = NewConstantsCopy(&game_device_data, bytes, updated_size);
      }
      else
      {
         const bool whole = copy && copy->size() == buffer_size;
         auto updated = NewConstantsCopy(&game_device_data, whole ? copy->data() : nullptr, buffer_size);
         std::memcpy(updated->data() + offset, bytes, updated_size);
         copy = std::move(updated);
      }
#if DEVELOPMENT
      game_device_data.mv_stats.updates++;
#endif
      return false;
   }

   // Motion vectors: a destroyed b0 / b1 / b3 buffer leaves the registry and the filter (its address can come back as another buffer,
   // of another size)
   static void OnDestroyResource(reshade::api::device* device, reshade::api::resource resource)
   {
      DeviceData* const device_data = device->get_private_data<DeviceData>();
      if (!device_data || !device_data->game)
         return;
      auto& game_device_data = GetGameDeviceData(*device_data);
      UINT unused_size = 0;
      if (!MayBeRegisteredBuffer(game_device_data, resource.handle, &unused_size))
         return;
      const std::lock_guard lock(game_device_data.mv_constants_mutex);
      if (game_device_data.mv_constants_copies.erase(resource.handle) == 0)
         return;
      const uint32_t count = (std::min)(game_device_data.mv_filtered_buffer_count.load(std::memory_order_relaxed), BorderlandsGotyGameDeviceData::max_filtered_buffers);
      for (uint32_t i = 0; i < count; i++)
      {
         if (game_device_data.mv_filtered_buffers[i].load(std::memory_order_relaxed) == resource.handle)
         {
            game_device_data.mv_filtered_buffers[i].store(BorderlandsGotyGameDeviceData::destroyed_buffer_slot, std::memory_order_release);
            break;
         }
      }
#if DEVELOPMENT
      game_device_data.mv_destroyed_buffers++;
#endif
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
      // UE3 copies the scene and its depth with CopySubresourceRegion too (10 a gameplay frame, see "refused_region_copies"). Only a
      // whole texture copy is one of them (the upscaler reads it, and the post passes read it in full), as MELE.
      auto& game_device_data = GetGameDeviceData(device_data);
      const bool scene_or_depth = src_resource == uint64_t(game_device_data.mv_scene_color.get()) || src_resource == uint64_t(game_device_data.mv_depth.get());
      bool whole = dst_subresource == 0 && src_subresource == 0 && (!dst_box || (dst_box->left == 0 && dst_box->top == 0 && dst_box->front == 0));
      if (whole && src_box && scene_or_depth)
      {
         uint4 size;
         DXGI_FORMAT unused_format;
         GetResourceInfo(reinterpret_cast<ID3D11Resource*>(src_resource), size, unused_format);
         whole = src_box->left == 0 && src_box->top == 0 && src_box->right >= size.x && src_box->bottom >= size.y;
      }
      if (!whole)
      {
#if DEVELOPMENT
         if (game_device_data.mv_scene_open && scene_or_depth)
         {
            game_device_data.mv_stats.refused_region_copies++;
         }
#endif
         return false;
      }
      return OverrideCopyResource(native_device, device_data, dst_resource, src_resource);
   }

   // The bound shader's motion vector version, patched from Core's bytecode copy on first use (a null shader if it can't be); the entry
   // is never erased. Vertex shaders that don't place vertices with b1's ViewProjectionMatrix (height fog quads bind it for the camera
   // position only) are refused: jittered, they would shift against their own UVs.
   template <typename T>
   static const BorderlandsGotyGameDeviceData::PatchedShader<T>& GetMotionVectorShader(ID3D11Device* native_device, DeviceData& device_data, std::unordered_map<uint32_t, BorderlandsGotyGameDeviceData::PatchedShader<T>>* shaders, uint32_t hash, reshade::api::pipeline pipeline)
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
      BorderlandsGotyGameDeviceData::PatchedShader<T> entry;
      {
         const std::shared_lock lock(s_mutex_generic);
         if (const auto it = device_data.pipeline_cache_by_pipeline_handle.find(pipeline.handle); it != device_data.pipeline_cache_by_pipeline_handle.end() && it->second->subobjects_cache)
         {
            const auto* desc = static_cast<const reshade::api::shader_desc*>(it->second->subobjects_cache[0].data);
            const auto* code = static_cast<const uint8_t*>(desc->code);
            if constexpr (vertex)
               patched = MotionVectorPatch::PatchVertexShader(code, desc->code_size, MotionVectorPatches::layout, &error);
            else
               patched = MotionVectorPatch::PatchPixelShader(code, desc->code_size, MotionVectorPatches::layout, &error);
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
                  entry.skinned = SUCCEEDED(reflection->GetResourceBindingDescByName("VSBoneConstants", &bind_desc)) && bind_desc.BindPoint == MotionVectorPatches::previous_slots[2].first;
                  // The object's translation, LocalToWorld's 4th row (row vectors)
                  if (SUCCEEDED(reflection->GetConstantBufferByName("$Globals")->GetVariableByName("LocalToWorld")->GetDesc(&variable_desc)) && (variable_desc.uFlags & D3D_SVF_USED) != 0)
                  {
                     entry.translation_offset = variable_desc.StartOffset + 3 * 16;
                  }
                  for (size_t i = 0; i < entry.read_sizes.size(); i++)
                     entry.read_sizes[i] = DXBC::ConstantBufferBytes(code, desc->code_size, MotionVectorPatches::previous_slots[i].first);
               }
            }
         }
      }
      if (!patched.empty())
      {
         HRESULT hr;
         if constexpr (vertex)
         {
            hr = native_device->CreateVertexShader(patched.data(), patched.size(), nullptr, &entry.shader);
         }
         else
         {
            hr = native_device->CreatePixelShader(patched.data(), patched.size(), nullptr, &entry.shader);
         }
         if (FAILED(hr))
         {
            error = std::format("create 0x{:08X}", uint32_t(hr));
         }
      }
      // Failures in every build (bug reports), every patched shader only in development
      if (DEVELOPMENT || !entry.shader)
      {
         reshade::log::message((entry.shader || screen_space) ? reshade::log::level::info : reshade::log::level::warning, std::format("[BL MV] {} 0x{:08X} {}", vertex ? "VS" : "PS", hash, entry.shader ? "patched" : error).c_str());
      }
      const std::unique_lock lock(game_device_data.mv_mutex);
      return shaders->try_emplace(hash, std::move(entry)).first->second;
   }

   // The bound vertex shader's patched version (null if refused), looked up again only when the game's changes
   static ID3D11VertexShader* GetPatchedVertexShader(ID3D11Device* native_device, const CommandListData& cmd_list_data, DeviceData& device_data, uint32_t hash)
   {
      auto& game_device_data = GetGameDeviceData(device_data);
      if (hash != game_device_data.mv_last_vertex_shader_hash || !game_device_data.mv_last_vertex_shader)
      {
         game_device_data.mv_last_vertex_shader = &GetMotionVectorShader(native_device, device_data, &game_device_data.mv_vertex_shaders, hash, cmd_list_data.pipeline_state_original_vertex_shader);
         game_device_data.mv_last_vertex_shader_hash = hash;
      }
      return game_device_data.mv_last_vertex_shader->shader.get();
   }

   // Opens the scene at the frame's first mesh draw into output sized depth (the world depth prepass): takes the scene depth and
   // picks the jitter the whole scene draws with. Once per present (the HUD and later passes never reopen it).
   static void OpenScene(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, const CommandListData& cmd_list_data, DeviceData& device_data, uint32_t vertex_shader_hash, ID3D11DepthStencilView* dsv)
   {
      auto& game_device_data = GetGameDeviceData(device_data);
      const uint2 depth_size = GetViewTextureSize(dsv);
      if (depth_size.x != device_data.output_resolution.x || depth_size.y != device_data.output_resolution.y || !GetPatchedVertexShader(native_device, cmd_list_data, device_data, vertex_shader_hash))
         return;
      game_device_data.mv_scene_open = true;
#if DEVELOPMENT
      if (auto* const perf_queries = game_device_data.perf_frame_queries; perf_queries && !std::exchange(perf_queries->scene_started, true))
         native_device_context->End(perf_queries->scene_start.get());
#endif
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
      const unsigned int phase = cb_luma_global_settings.FrameIndex % (sr_instance_data ? (std::max)(sr_implementations[device_data.sr_type]->GetJitterPhases(sr_instance_data), 1) : SR::GetDefaultJitterPhases());
      game_device_data.mv_jitter = (sr_instance_data || g_mv_force_jitter) && !g_mv_disable_jitter ? std::array<float, 2>{SR::HaltonSequence(phase, 2), SR::HaltonSequence(phase, 3)} : std::array<float, 2>{};
      game_device_data.mv_jitter_ndc = {game_device_data.mv_jitter[0] * 2.f / device_data.output_resolution.x, game_device_data.mv_jitter[1] * -2.f / device_data.output_resolution.y};
      const float ndc_jitter[4] = {game_device_data.mv_jitter_ndc[0], game_device_data.mv_jitter_ndc[1], 0.f, 0.f};
      if (!PatchedDraws::WriteDynamicConstants(native_device, native_device_context, std::addressof(game_device_data.mv_jitter_buffer), ndc_jitter, sizeof(ndc_jitter)))
      {
         // No stale jitter on the scene draws either: no motion vectors this frame
         game_device_data.mv_jitter = {};
         game_device_data.mv_jitter_ndc = {};
         game_device_data.mv_jitter_buffer.reset();
      }
   }

#if DEVELOPMENT
#define MV_REJECT(reason) ([&](auto& gd) { gd.mv_stats.rejected[reason]++; gd.mv_draw_reject = int(reason); return false; }(GetGameDeviceData(device_data)))
#else
#define MV_REJECT(reason) false
#endif

   // Draws an opaque draw into the fp16 scene (the world and weapon base passes: the scene target alone, output sized, with the scene
   // depth) with the patched shaders, adding the motion vector target ("target_slot", past the game's) and the previous frame's
   // b0 / b1 / b3 ("previous_slots"). False if it can't (the draw then goes to "DrawWithJitter").
   static bool DrawWithMotionVectors(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, const CommandListData& cmd_list_data, DeviceData& device_data, const ShaderHashesList<OneShaderPerPipeline>& original_shader_hashes, const std::function<void()>& draw, const com_ptr<ID3D11RenderTargetView> (&rtvs)[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT], ID3D11DepthStencilView* dsv)
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
      // Before a target is accepted: a colorless draw (UE3's occlusion query boxes) into another target would claim it as the scene
      com_ptr<ID3D11BlendState> blend_state;
      native_device_context->OMGetBlendState(&blend_state, nullptr, nullptr);
      if (blend_state.get() != game_device_data.mv_blend_state)
      {
         D3D11_BLEND_DESC blend_desc = CD3D11_BLEND_DESC(D3D11_DEFAULT);
         if (blend_state)
            blend_state->GetDesc(&blend_desc);
         // Additive lights, decals and translucents keep the motion vectors of what's behind them, and so do draws that write no color
         // (occlusion query bounding boxes: every blend state writes the motion vector target, see "OnCreateBlendState")
         const D3D11_RENDER_TARGET_BLEND_DESC& rt0_blend = blend_desc.RenderTarget[0];
         game_device_data.mv_blend_opaque = rt0_blend.RenderTargetWriteMask != 0 && (!rt0_blend.BlendEnable || (rt0_blend.SrcBlend == D3D11_BLEND_ONE && rt0_blend.DestBlend == D3D11_BLEND_ZERO && rt0_blend.BlendOp == D3D11_BLEND_OP_ADD));
         game_device_data.mv_blend_state = blend_state.get();
      }
      if (!game_device_data.mv_blend_opaque)
         return MV_REJECT(6);
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
         // R16G16_FLOAT: DLSS takes it (and RG32), FSR keeps 16 bits internally; the error is under 0.1% of the motion, and it saved
         // 0.25 ms at 4K with DLSS K against R32G32 (the upscaler and the DOF gather read it too)
         constexpr DXGI_FORMAT format = DXGI_FORMAT_R16G16_FLOAT;
         if (desc.Width != size.x || desc.Height != size.y)
         {
            game_device_data.mv_texture.reset();
            game_device_data.mv_rtv.reset();
            game_device_data.mv_srv.reset();
            game_device_data.mv_uav.reset();
            // The fill reads the target back through its UAV
            D3D11_FEATURE_DATA_FORMAT_SUPPORT2 support = {format};
            const bool typed_uav_load = SUCCEEDED(native_device->CheckFeatureSupport(D3D11_FEATURE_FORMAT_SUPPORT2, &support, sizeof(support))) && (support.OutFormatSupport2 & D3D11_FORMAT_SUPPORT2_UAV_TYPED_LOAD) != 0;
            desc = {size.x, size.y, 1, 1, format, {1, 0}, D3D11_USAGE_DEFAULT, D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | (typed_uav_load ? D3D11_BIND_UNORDERED_ACCESS : 0u)};
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

      ID3D11VertexShader* const vertex_shader = GetPatchedVertexShader(native_device, cmd_list_data, device_data, original_shader_hashes.vertex_shaders[0]);
      if (const uint32_t pixel_shader_hash = original_shader_hashes.pixel_shaders[0]; pixel_shader_hash != game_device_data.mv_last_pixel_shader_hash)
      {
         game_device_data.mv_last_pixel_shader = GetMotionVectorShader(native_device, device_data, &game_device_data.mv_pixel_shaders, pixel_shader_hash, cmd_list_data.pipeline_state_original_pixel_shader).shader.get();
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
         // The fill's marker: the largest float16 (a larger clear value is stored as it in R16G16_FLOAT)
         const FLOAT clear_value = game_device_data.mv_fill_pending ? 65504.f : 0.f;
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
#if DEVELOPMENT
         // Objects without a translation (world space and instanced geometry, placed by their vertex inputs and b1) all share one by design:
         // whichever of them matches gives the same previous position (VS 0x9B0EA126, 0xB4A05DE8: instance matrix in v4-v7)
         game_device_data.mv_stats.tiebreak_collisions = PatchedDraws::CountTieBreakCollisions(game_device_data.mv_previous_objects, [](const auto& a, const auto& b)
            { return !a.translated || (PatchedDraws::SameBytes(a.object, b.object) && PatchedDraws::SameBytes(a.bones, b.bones)); });
         if (std::exchange(g_mv_log_tiebreak, false))
         {
            // The first byte two copies differ at (-1: the same bytes, -2: one missing or another size)
            const auto first_difference = [](const BorderlandsGotyGameDeviceData::ConstantsCopy& a, const BorderlandsGotyGameDeviceData::ConstantsCopy& b) -> int64_t
            {
               if (PatchedDraws::SameBytes(a, b))
                  return -1;
               if (!a || !b || a->size() != b->size())
                  return -2;
               return std::mismatch(a->begin(), a->end(), b->begin()).first - a->begin();
            };
            for (const auto& [key, objects] : game_device_data.mv_previous_objects)
            {
               for (size_t i = 0; i < objects.size(); i++)
               {
                  for (size_t j = i + 1; j < objects.size(); j++)
                  {
                     const auto& a = objects[i];
                     const auto& b = objects[j];
                     if (!a.translated || a.translation != b.translation || (PatchedDraws::SameBytes(a.object, b.object) && PatchedDraws::SameBytes(a.bones, b.bones)))
                        continue;
                     reshade::log::message(reshade::log::level::info, std::format("[BL MV] tie-break collision: VS 0x{:08X} PS 0x{:08X}, {} indices, objects {} and {} of {} at ({}, {}, {}); b0 of {} bytes first differs at {}, b3 at {}", a.vertex_shader_hash, a.pixel_shader_hash, a.index_count, i, j, objects.size(), a.translation[0], a.translation[1], a.translation[2], a.object ? a.object->size() : 0, first_difference(a.object, b.object), first_difference(a.bones, b.bones)).c_str());
                  }
               }
            }
         }
#endif
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
      const bool skinned = game_device_data.mv_last_vertex_shader->skinned;
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
#if DEVELOPMENT
      // "Performance Test" without motion vector draws: the frame (camera, target clear, camera fill, upscaler) still happens, the draws
      // run jittered only, or untouched
      if (perf_test_modes[g_perf_test].motion_vector_draws < 2)
      {
         if (object && camera && !game_device_data.mv_camera)
            game_device_data.mv_camera = camera;
         return false;
      }
#endif
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
         if (const uint32_t offset = game_device_data.mv_last_vertex_shader->translation_offset; offset != UINT_MAX && offset + sizeof(translation) <= object->size())
         {
            std::memcpy(translation.data(), object->data() + offset, sizeof(translation));
         }

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
         else if (game_device_data.mv_previous_camera && copy_size(game_device_data.mv_previous_camera) == camera->size() && (camera == game_device_data.mv_camera || (copy_size(game_device_data.mv_camera) == camera->size() && std::memcmp(camera->data(), game_device_data.mv_camera->data(), camera->size()) == 0)))
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
         auto& drawn_objects = game_device_data.mv_objects[key];
         drawn_objects.push_back({translation, object, camera, bones});
#if DEVELOPMENT
         drawn_objects.back().vertex_shader_hash = original_shader_hashes.vertex_shaders[0];
         drawn_objects.back().pixel_shader_hash = original_shader_hashes.pixel_shaders[0];
         drawn_objects.back().index_count = draw_data.index_count;
         drawn_objects.back().translated = game_device_data.mv_last_vertex_shader->translation_offset != UINT_MAX;
#endif
      }
#if DEVELOPMENT
      else
      {
         game_device_data.mv_stats.uncopied++;
      }
#endif
      // Uploads only the bytes the vertex shader reads (the engine's buffers are pooled by size, larger than most shaders' constants)
      const std::span<const UINT> read_sizes = (g_mv_read_sizes ? std::span<const UINT>(game_device_data.mv_last_vertex_shader->read_sizes) : std::span<const UINT>());
      game_device_data.mv_previous_constants.Bind(native_device, native_device_context, MotionVectorPatches::previous_slots, uploads, current, "BL", read_sizes);
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
      PatchedDraws::BindPatchedShader(native_device_context, vertex_shader, &game_device_data.mv_bound_vertex_shader);
      PatchedDraws::BindPatchedShader(native_device_context, pixel_shader, &game_device_data.mv_bound_pixel_shader);

      draw();
#if DEVELOPMENT
      game_device_data.mv_stats.motion_vector_draws++;
#endif
      return true;
   }

   // Jitter for the scene's mesh draws without motion vectors (patched vertex shader, game pixel shader): the depth prepasses,
   // additive lights, decals, fog volumes, distortion and translucents. Every draw depth tested against the scene takes the same
   // jitter, or jittered and unjittered depths of the same surface fail each other's test. False if it can't (the draw runs untouched).
   static bool DrawWithJitter(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, const CommandListData& cmd_list_data, DeviceData& device_data, const ShaderHashesList<OneShaderPerPipeline>& original_shader_hashes, const std::function<void()>& draw, ID3D11DepthStencilView* dsv)
   {
      auto& game_device_data = GetGameDeviceData(device_data);
      if (!game_device_data.mv_scene_open || game_device_data.mv_jitter == std::array<float, 2>{} || !game_device_data.mv_jitter_buffer)
         return false;
#if DEVELOPMENT
      // "Performance Test" without motion vector draws: the whole frame unjittered, so its depth tests stay consistent
      if (perf_test_modes[g_perf_test].motion_vector_draws < 1)
         return false;
#endif
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
      PatchedDraws::BindPatchedShader(native_device_context, vertex_shader, &game_device_data.mv_bound_vertex_shader);
      ID3D11Buffer* const jitter = game_device_data.mv_jitter_buffer.get();
      native_device_context->VSSetConstantBuffers(MotionVectorPatches::jitter_slot, 1, &jitter);
      PatchedDraws::RestoreGameShader(native_device_context, &game_device_data.mv_bound_pixel_shader);
      draw();
#if DEVELOPMENT
      game_device_data.mv_stats.jitter_draws++;
#endif
      return true;
   }

   // DLSS / FSR on the jittered scene (its last copy, which the DOF/Bloom gather and the uber post read, else the scene itself), the
   // scene depth and the motion vectors, at native resolution; the uber and the gather read its output in place of its input (see
   // "OnDrawOrDispatch"). False if it didn't draw (missing input, or the upscaler failed).
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
         game_device_data.sr_output_srv.reset();
         if (SUCCEEDED(native_device->CreateTexture2D(&output_desc, nullptr, &device_data.sr_output_color)) && FAILED(native_device->CreateShaderResourceView(device_data.sr_output_color.get(), nullptr, &game_device_data.sr_output_srv)))
            device_data.sr_output_color.reset();
         game_device_data.sr_output_recreated = true;
      }
      if (!device_data.sr_output_color)
      {
         // Back to SMAA until the upscaler is picked again
         device_data.sr_suppressed = true;
         return false;
      }

      // FSR needs the camera (DLSS ignores it). b1's ViewProjectionMatrix multiplies row vectors with absolute world translation: column
      // 1 is the up axis / tan(fov / 2); UE3's projection has an infinite far plane with clip w - clip z = the near plane (element 3,3
      // minus 3,2).
      const float* const view_projection = reinterpret_cast<const float*>(game_device_data.mv_camera->data());
      const double up_length = std::sqrt(double(view_projection[1]) * view_projection[1] + double(view_projection[5]) * view_projection[5] + double(view_projection[9]) * view_projection[9]);
      const double vert_fov = up_length > 0.0 ? 2.0 * std::atan(1.0 / up_length) : 0.0;
      const double near_plane = double(view_projection[15]) - double(view_projection[14]);

      SR::SettingsData settings_data;
      settings_data.output_width = scene_desc.Width;
      settings_data.output_height = scene_desc.Height;
      settings_data.render_width = scene_desc.Width;
      settings_data.render_height = scene_desc.Height;
      settings_data.dynamic_resolution = false;
      settings_data.hdr = true;
      settings_data.inverted_depth = false;
      settings_data.mvs_jittered = false;
      // The motion vectors are UV deltas, previous minus current
      settings_data.mvs_x_scale = float(scene_desc.Width);
      settings_data.mvs_y_scale = float(scene_desc.Height);
      settings_data.auto_exposure = true;
      settings_data.render_preset = dlss_render_preset;
      sr_implementations[device_data.sr_type]->UpdateSettings(sr_instance_data, native_device_context, settings_data);

      SR::SuperResolutionImpl::DrawData draw_data;
      draw_data.source_color = scene.get();
      draw_data.output_color = device_data.sr_output_color.get();
      draw_data.motion_vectors = game_device_data.mv_texture.get();
      draw_data.depth_buffer = depth.get();
      draw_data.render_width = scene_desc.Width;
      draw_data.render_height = scene_desc.Height;
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
      // The post passes after it read the output in place of the input (see "OnDrawOrDispatch"); nothing reads the scene or its copy after
      // the gather (probed in gameplay, the main menu, the inventory, the sniper scope and Fight For Your Life)
      game_device_data.sr_input = scene_resource;
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
      return true;
   }

   // Ends the scene at its first post pass (immediate context): the camera motion fill for the pixels no motion vector draw wrote, then
   // DLSS / FSR ("DrawUpscaler"), both before that pass reads the scene
   static void EndScene(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, DeviceData& device_data)
   {
      auto& game_device_data = GetGameDeviceData(device_data);
      game_device_data.mv_scene_open = false;
      game_device_data.mv_scene_done = true;
      DrawStateStack<DrawStateStackType::FullGraphics> graphics_state;
      DrawStateStack<DrawStateStackType::Compute> compute_state;
      graphics_state.Cache(native_device_context, device_data.uav_max_count);
      compute_state.Cache(native_device_context, device_data.uav_max_count);
      auto* const fill_shader = FindShader(device_data.native_compute_shaders, CompileTimeStringHash("BL Motion Vector Fill CS"));
#if DEVELOPMENT
      auto* const perf_queries = game_device_data.perf_frame_queries;
      if (perf_queries && perf_queries->scene_started && !std::exchange(perf_queries->fill, true))
      {
         native_device_context->End(perf_queries->fill_start.get());
      }
#endif
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
         float constants[20] = {}; // A multiple of 16 bytes
         for (int i = 0; i < 16; i++)
            constants[i] = float(reprojection.GetData()[i]);
         constants[16] = game_device_data.mv_jitter_ndc[0];
         constants[17] = game_device_data.mv_jitter_ndc[1];
         if (PatchedDraws::WriteDynamicConstants(native_device, native_device_context, std::addressof(game_device_data.mv_fill_buffer), constants, sizeof(constants)))
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
            native_device_context->Dispatch((uint32_t(device_data.output_resolution.x) + 7) / 8, (uint32_t(device_data.output_resolution.y) + 7) / 8, 1);
            ID3D11UnorderedAccessView* const null_uav = nullptr;
            native_device_context->CSSetUnorderedAccessViews(0, 1, &null_uav, nullptr);
         }
      }
#if DEVELOPMENT
      if (perf_queries && perf_queries->scene_started && !std::exchange(perf_queries->scene, true))
      {
         native_device_context->End(perf_queries->scene_end.get());
      }
#endif
      if (IsSRActive(device_data))
      {
         const bool drawn = DrawUpscaler(native_device, native_device_context, device_data);
#if DEVELOPMENT
         game_device_data.mv_stats.sr_draws += drawn;
         if (perf_queries && perf_queries->scene && !perf_queries->sr && drawn)
         {
            native_device_context->End(perf_queries->sr_end.get());
            perf_queries->sr = true;
         }
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
#if DEVELOPMENT
      // For the MCP "luma_dev_values" tool
      Mcp::RegisterToggles({{"mv_enable", &g_mv_enable}, {"mv_debug_view", &g_mv_debug_view}, {"mv_force_jitter", &g_mv_force_jitter}, {"mv_disable_jitter", &g_mv_disable_jitter}, {"mv_log_tiebreak", &g_mv_log_tiebreak},
         {"mv_buffer_filter", &g_mv_buffer_filter}, {"mv_constants_pool", &g_mv_constants_pool}, {"mv_read_sizes", &g_mv_read_sizes}});
      Mcp::RegisterToggles({{"smaa_enable", &g_smaa_enable}, {"smaa_predication", &g_smaa_predication}, {"smaa_pred_debug", &g_smaa_pred_debug}, {"smaa_pred_measure", &g_smaa_pred_measure},
         {"hide_ui", &g_hide_ui}, {"gtao_enable", &g_gtao_enable}, {"fix_movie_leak", &g_fix_movie_leak}, {"perf_hook_timers", &g_perf_hook_timers}});
      Mcp::RegisterValues({{"rcas_sharpness", &g_rcas_sharpness, 0.f, 1.f}, {"smaa_pred_tolerance", &g_smaa_pred_tolerance, 0.002f, 0.2f}, {"gtao_final_value_power", &g_gtao_final_value_power, 0.3f, 4.5f},
         {"gtao_depth_scale", &g_gtao_depth_scale, 1.f, 200.f}, {"gtao_radius_override", &g_gtao_radius_override, 0.f, 5.f}});
      Mcp::RegisterInts({{"gtao_debug_view", &g_gtao_debug_view, 0, 4}, {"gtao_temporal", &g_gtao_temporal, 0, 2},
         {"perf_test", &g_perf_test, 0, int(std::size(perf_test_modes)) - 1, [](DeviceData& device_data, double value)
            {
               // As the "Performance Test" combo
               const PerfTestMode& mode = perf_test_modes[int(value)];
               if (!IsPerfTestModeAvailable(device_data, mode))
                  return std::format("{} needs an upscaler this GPU doesn't have", mode.name);
               g_perf_sweep = false;
               ApplyPerfTestMode(device_data, int(value));
               return std::string();
            }}});
      Mcp::RegisterTextures({MCP_GAME_TEXTURE("smaa.input", tex_input),
         MCP_GAME_TEXTURE("smaa.pred_mask", tex_pred),
         MCP_GAME_TEXTURE("smaa.output", tex_smaa_out),
         MCP_GAME_TEXTURE("rcas.output", tex_rcas_out),
         MCP_GAME_TEXTURE("gtao.depth_mips", tex_gtao_depth_mips),
         MCP_GAME_TEXTURE("mv.velocity", mv_texture)});
#endif
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

      // Depth-extract CS for SMAA predication: hardware d24 -> R16F plane-deviation edge-ness.
      native_shaders_definitions.emplace(CompileTimeStringHash("BL Depth Extract CS"),
         ShaderDefinition("Luma_BL_DepthExtract", reshade::api::pipeline_subobject_type::compute_shader));
      // RCAS sharpen PS (drawn via core "Copy VS" + DrawCustomPixelShader after SMAA or DLSS / FSR).
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

      // DLSS / FSR: camera motion for the motion vector pixels no patched draw wrote (sky, unpatched draws), the DOF/Bloom gather with the
      // DOF history, the CPU copies of the per-draw constants, and the motion vector target written by every blend state
      native_shaders_definitions.emplace(CompileTimeStringHash("BL Motion Vector Fill CS"),
         ShaderDefinition("Luma_BL_MotionVectorFill", reshade::api::pipeline_subobject_type::compute_shader));
      native_shaders_definitions.emplace(CompileTimeStringHash("BL DOF Gather CS"),
         ShaderDefinition{"Luma_BL_DOFGather", reshade::api::pipeline_subobject_type::compute_shader, nullptr, "gather_cs"});
      reshade::register_event<reshade::addon_event::update_buffer_region>(OnUpdateBufferRegion);
      reshade::register_event<reshade::addon_event::destroy_resource>(OnDestroyResource);
      reshade::register_event<reshade::addon_event::create_pipeline>(OnCreateBlendState);

      // The game uses constant buffer slots b0-b3; Luma takes b12/b13 here, and b5 (DOF gather, CS), b8-b11 (motion vector patches,
      // VS) and b11 (XeGTAO knobs, CS) elsewhere.
      // luma_data is used by the Display Composition; luma_ui stays off (UI drawn by the game).
      luma_settings_cbuffer_index = 13;
      luma_data_cbuffer_index = 12;
      luma_ui_cbuffer_index = -1;

      // Manual Scene + UI Paper White sliders instead of the OS HDR reference level. Core gates the separate
      // "UI Paper White" slider on UI_DRAW_TYPE >= 1 && !use_os_reference_white_level. Default 203 nits (BT.2408).
      use_os_reference_white_level = false;

      // User controls (LumaSettings.GameSettings, see GameCBuffers.hlsl); the grade is vanilla by default.
      default_luma_global_game_settings.Exposure = 1.f; // multiplier (1x)
      default_luma_global_game_settings.Saturation = 1.f;
      default_luma_global_game_settings.HighlightDechroma = 0.f; // Off: only the DICE and gamut mapping desaturation applies
      default_luma_global_game_settings.BloomIntensity = 1.f;
      default_luma_global_game_settings.Contrast = 1.f;
      default_luma_global_game_settings.Dithering = 1.f;          // subtle anti-banding on by default
      default_luma_global_game_settings.FlareOut = 1.f;           // additive lens-flare/glare scale (1 = vanilla)
      default_luma_global_game_settings.VideoAutoHDREnable = 1.f; // light AutoHDR on Bink movies, HDR only
      default_luma_global_game_settings.VideoAutoHDRBoost = 0.5f; // highlight-expansion strength (peak ~165 nits at 0.5)
      cb_luma_global_settings.GameSettings = default_luma_global_game_settings;
   }

   void OnCreateDevice(ID3D11Device* native_device, DeviceData& device_data) override
   {
      device_data.game = new BorderlandsGotyGameDeviceData;
      device_data.taa_detected = true; // No TAA to replace, but Core's upscaler UI checks for it
#if DEVELOPMENT
      // For the MCP "luma_dev_values" tool: the last complete frame's counts
      const auto& stats = GetGameDeviceData(device_data).mv_last_stats;
      Mcp::RegisterCounters({{"mv.draws", &stats.motion_vector_draws}, {"mv.jitter_draws", &stats.jitter_draws}, {"mv.matched", &stats.matched}, {"mv.camera_only", &stats.camera_only},
         {"mv.other_camera", &stats.other_camera}, {"mv.uncopied", &stats.uncopied}, {"mv.updates", &stats.updates}, {"mv.sr_draws", &stats.sr_draws}, {"mv.tiebreak_collisions", &stats.tiebreak_collisions}, {"mv.ended_by_hash", &stats.ended_by},
         {"mv.registered_buffers", &stats.registered_buffers}, {"mv.constants_pool", &stats.constants_pool},
         {"mv.destroyed_buffers", &GetGameDeviceData(device_data).mv_destroyed_buffers}, {"mv.refused_region_copies", &stats.refused_region_copies},
         {"fxaa.skipped", &stats.fxaa_skipped}}, &device_data);
      for (size_t i = 0; i < std::size(mv_reject_names); i++)
         Mcp::RegisterCounter(std::string("mv.rejected.") + mv_reject_names[i], &stats.rejected[i], &device_data);
#endif
   }

   void OnDestroyDeviceData(DeviceData& device_data) override
   {
#if DEVELOPMENT
      Mcp::Unregister(&device_data);
#endif
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
#if DEVELOPMENT
            const PerfHookTimer timer{gd.perf_hook_ns}; // The draw's own submission included
#endif
            com_ptr<ID3D11RenderTargetView> rtvs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
            com_ptr<ID3D11DepthStencilView> dsv;
            native_device_context->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, &rtvs[0], &dsv);
            if (!gd.mv_scene_open && dsv)
               OpenScene(native_device, native_device_context, cmd_list_data, device_data, original_shader_hashes.vertex_shaders[0], dsv.get());
            const bool motion_vectors = DrawWithMotionVectors(native_device, native_device_context, cmd_list_data, device_data, original_shader_hashes, *original_draw_dispatch_func, rtvs, dsv.get());
            const bool jitter = !motion_vectors && DrawWithJitter(native_device, native_device_context, cmd_list_data, device_data, original_shader_hashes, *original_draw_dispatch_func, dsv.get());
#if DEVELOPMENT
            Mcp::Annotate(cmd_list_data, motion_vectors ? "mv" : (jitter ? "jitter" : "unpatched"));
            if (const int reject = std::exchange(gd.mv_draw_reject, -1); reject >= 0)
               Mcp::Annotate(cmd_list_data, "mv_reject", reject);
#endif
            if (motion_vectors || jitter)
               return DrawOrDispatchOverrideType::Replaced;
         }
      }
      // DLSS / FSR: the uber and the DOF/Bloom gather (the game's, or ours below) read the upscaled scene where they bind its input
      if ((uber || gather) && device_data.has_drawn_sr && gd.sr_input)
      {
         com_ptr<ID3D11ShaderResourceView> bound;
         if (uber)
            native_device_context->PSGetShaderResources(0, 1, &bound);
         else
            native_device_context->CSGetShaderResources(0, 1, &bound);
         com_ptr<ID3D11Resource> resource;
         if (bound)
            bound->GetResource(&resource);
         if (resource.get() == gd.sr_input)
         {
            ID3D11ShaderResourceView* const upscaled = gd.sr_output_srv.get();
            if (uber)
               native_device_context->PSSetShaderResources(0, 1, &upscaled);
            else
               native_device_context->CSSetShaderResources(0, 1, &upscaled);
         }
      }

      // Every draw without a patched shader: the game's own, if the last patched draw's are still bound
      PatchedDraws::RestoreGameShader(native_device_context, &gd.mv_bound_vertex_shader);
      PatchedDraws::RestoreGameShader(native_device_context, &gd.mv_bound_pixel_shader);

      // DLSS / FSR: the DOF/Bloom gather takes its DOF amount from a per pixel history, updated in the same pass (the jittered depth
      // flipped whole blocks, see Luma_BL_DOFGather.hlsl)
      if (gather && device_data.has_drawn_sr && original_draw_dispatch_func && *original_draw_dispatch_func)
      {
         auto* const gather_shader = FindShader(device_data.native_compute_shaders, CompileTimeStringHash("BL DOF Gather CS"));
         const uint2 size = {uint32_t(device_data.output_resolution.x), uint32_t(device_data.output_resolution.y)};
         // The history, at the scene's size
         D3D11_TEXTURE2D_DESC history_desc = {};
         if (gd.dof_history[0])
            gd.dof_history[0]->GetDesc(&history_desc);
         if (history_desc.Width != size.x || history_desc.Height != size.y)
         {
            gd.dof_history_valid = false;
            history_desc = {size.x, size.y, 1, 1, DXGI_FORMAT_R16_FLOAT, {1, 0}, D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS};
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
         const float constants[4] = {(gd.dof_history_valid && !device_data.force_reset_sr) ? 0.1f : 1.f};
         // The upscaled scene (bound above) and the game's depth (the last copy, before the first person weapon)
         com_ptr<ID3D11ShaderResourceView> game_srvs[2];
         native_device_context->CSGetShaderResources(0, UINT(std::size(game_srvs)), &game_srvs[0]);
         if (gather_shader && gd.dof_history_srvs[previous] && gd.dof_history_uavs[next] && gd.mv_srv && game_srvs[1] && PatchedDraws::WriteDynamicConstants(native_device, native_device_context, std::addressof(gd.dof_history_cb), constants, sizeof(constants)))
         {
            com_ptr<ID3D11UnorderedAccessView> target_uav;
            native_device_context->CSGetUnorderedAccessViews(0, 1, &target_uav);
            DrawStateStack<DrawStateStackType::Compute> compute_state;
            DrawStateStack<DrawStateStackType::SimpleGraphics> graphics_state;
            compute_state.Cache(native_device_context, device_data.uav_max_count);
            graphics_state.Cache(native_device_context, device_data.uav_max_count);
            // The motion vector target may still be bound for output (see "DrawWithMotionVectors"): its SRV would be null
            native_device_context->OMSetRenderTargets(0, nullptr, nullptr);
#if DEVELOPMENT
            auto* const perf_queries = gd.perf_frame_queries && !gd.perf_frame_queries->dof ? gd.perf_frame_queries : nullptr;
            if (perf_queries)
               native_device_context->End(perf_queries->dof_start.get());
#endif

            // The scene, the game's depth, the motion vectors and last frame's history in; the gather's target and the next history out
            // (outputs bound first: D3D11 nulls an SRV of a resource still bound for output)
            ID3D11Buffer* const cb = gd.dof_history_cb.get();
            ID3D11SamplerState* const linear_sampler = device_data.sampler_state_linear.get();
            ID3D11UnorderedAccessView* const uavs[2] = {target_uav.get(), gd.dof_history_uavs[next].get()};
            ID3D11ShaderResourceView* const srvs[4] = {game_srvs[0].get(), game_srvs[1].get(), gd.mv_srv.get(), gd.dof_history_srvs[previous].get()};
            native_device_context->CSSetConstantBuffers(5, 1, &cb);
            native_device_context->CSSetSamplers(0, 1, &linear_sampler);
            native_device_context->CSSetUnorderedAccessViews(0, UINT(std::size(uavs)), uavs, nullptr);
            native_device_context->CSSetShaderResources(0, UINT(std::size(srvs)), srvs);
            native_device_context->CSSetShader(gather_shader, nullptr, 0);
            // One thread per 2x2 scene texels, as the game's
            native_device_context->Dispatch((size.x + 15) / 16, (size.y + 15) / 16, 1);
#if DEVELOPMENT
            if (perf_queries)
            {
               native_device_context->End(perf_queries->dof_end.get());
               perf_queries->dof = true;
            }
#endif

            compute_state.Restore(native_device_context);
            graphics_state.Restore(native_device_context);
            gd.dof_history_index = next;
            gd.dof_history_valid = true;
            return DrawOrDispatchOverrideType::Replaced;
         }
      }

      // Hide HUD: cancel the game's UI draws, those into a swapchain back buffer ("device_data.back_buffers", as Core's UI detection).
      // Compute and offscreen draws have no swapchain RTV, so they're untouched.
      if (g_hide_ui && !is_custom_pass)
      {
         ComPtr<ID3D11RenderTargetView> rtv;
         native_device_context->OMGetRenderTargets(1, rtv.put(), nullptr);
         ComPtr<ID3D11Resource> rtv_res;
         if (rtv)
            rtv->GetResource(rtv_res.put());
         const std::shared_lock lock(device_data.mutex);
         if (rtv_res && device_data.back_buffers.contains(reinterpret_cast<uint64_t>(rtv_res.get())))
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
      }

      // XeGTAO over the native HBAO+ (see the AO hash block above; deinterleave x2 -> normals -> coarse x2 -> blur -> apply blit).
      // The chain is taken over only when everything is ready at the first dispatch; a failure there leaves the whole native chain
      // untouched, like the SMAA fp16 guard. The normals pass 0xB2B47225 isn't hooked: our main pass reads its ViewNormalTex output.
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
            // Size the scratch (and dispatches) from the input depth desc (the game's HBAO+ full-res). The
            // game's own cb0 (InvFullResolution etc.) is content-dimensioned, so the shader's pixel<->UV math
            // is correct; the final pass writes the game's AO buffer at identical pixel coords.
            const uint2 depth_size = GetViewTextureSize(srv_d.get());
            if (depth_size.x == 0 || depth_size.y == 0)
               return DrawOrDispatchOverrideType::None;
            const uint32_t w = depth_size.x, h = depth_size.y;

            // (Re)create the scratch at the game's AO full-res (cached; NOT per-frame).
            if (gd.gtao_w != w || gd.gtao_h != h)
            {
               gd.ReleaseGTAOScratch();

               // 5 mips: XE_GTAO_DEPTH_MIP_LEVELS
               const D3D11_TEXTURE2D_DESC td = {.Width = w, .Height = h, .MipLevels = 5, .ArraySize = 1, .Format = DXGI_FORMAT_R32_FLOAT, .SampleDesc = {.Count = 1}, .Usage = D3D11_USAGE_DEFAULT, .BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS};
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

               for (int i = 0; ok && i < 2; i++)
               {
                  ok = CreateDefaultTex(native_device, w, h, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, std::addressof(gd.tex_gtao_working[i]), DXGI_FORMAT_R8G8_UNORM);
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

            // Knob CB (b11); the last is the noise index
            const float knobs[8] = {g_gtao_final_value_power, g_gtao_depth_scale, g_gtao_radius_override, float(g_gtao_debug_view), IsGTAOTemporal(device_data) ? float(cb_luma_global_settings.FrameIndex % 64) : 0.f};
            if (!PatchedDraws::WriteDynamicConstants(native_device, native_device_context, std::addressof(gd.cb_gtao), knobs, sizeof(knobs)))
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
            // No output bound: unreachable in practice (the blur always binds its u0); with nothing for either path to write, the
            // native blur runs.
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
            // cb0 ($Globals: ProjInfo etc.) and cb2 (MinZ_MaxZRatioCS) are read at the same b0 / b2 slots, inherited from the
            // deinterleave / coarse passes earlier in the same contiguous HBAO+ chain (immediate context, fixed pass order, no
            // ClearState between them). If a variant reorders the AO passes, capture and rebind them.

            // Every pass sets its UAVs before its SRVs and unbinds its UAVs after its dispatch: an SRV of a resource still bound as a
            // CS UAV is silently bound as null (the UAV wins the hazard), and the chain then reads zeros while every write "succeeds".
            const auto dispatch = [&](ID3D11ComputeShader* shader, std::initializer_list<ID3D11ShaderResourceView*> srvs, std::initializer_list<ID3D11UnorderedAccessView*> uavs, UINT groups_x, UINT groups_y)
            {
               static constexpr std::array<ID3D11ShaderResourceView*, 2> srv_nulls = {};
               static constexpr std::array<ID3D11UnorderedAccessView*, 5> uav_nulls = {};
               native_device_context->CSSetShaderResources(0, UINT(srv_nulls.size()), srv_nulls.data());
               native_device_context->CSSetUnorderedAccessViews(0, UINT(uavs.size()), uavs.begin(), nullptr);
               native_device_context->CSSetShaderResources(0, UINT(srvs.size()), srvs.begin());
               native_device_context->CSSetShader(shader, nullptr, 0);
               native_device_context->Dispatch(groups_x, groups_y, 1);
               native_device_context->CSSetUnorderedAccessViews(0, UINT(uavs.size()), uav_nulls.data(), nullptr);
            };
            // 1) Prefilter: game full-res depth -> our R32F mip pyramid (each thread does 2x2 -> 16x16 per group).
            dispatch(cs_prefilter, {gd.srv_gtao_depth.get()}, {gd.gtao_depth_mip_uavs[0].get(), gd.gtao_depth_mip_uavs[1].get(), gd.gtao_depth_mip_uavs[2].get(), gd.gtao_depth_mip_uavs[3].get(), gd.gtao_depth_mip_uavs[4].get()}, (w + 15) / 16, (h + 15) / 16);
            // 2) Main pass: pyramid + game view normals -> AO+edges (working0).
            dispatch(cs_main, {gd.srv_gtao_depth_mips.get(), gd.srv_gtao_normals.get()}, {gd.uav_gtao_working[0].get()}, (w + 7) / 8, (h + 7) / 8);
            // 3) Denoise 1: working0 -> working1 (2 horizontal pixels per thread). Skipped when temporal: the final pass reads working0.
            const bool single_denoise = IsGTAOTemporal(device_data);
            if (!single_denoise)
               dispatch(cs_denoise_1, {gd.srv_gtao_working[0].get()}, {gd.uav_gtao_working[1].get()}, (w + 15) / 16, (h + 7) / 8);
            // 4) Denoise 2 (final): working1 (working0 when single) -> the game's r16g16_float AO target.
            dispatch(cs_denoise_2, {gd.srv_gtao_working[single_denoise ? 0 : 1].get()}, {uav_final.get()}, (w + 15) / 16, (h + 7) / 8);

            st.Restore(native_device_context);
            return DrawOrDispatchOverrideType::Replaced; // cancel the native blur
         }
      }

#if ENABLE_SMAA
      // FXAA's passes before the resolve SMAA / RCAS replace are skipped too (the first resolve writes the swapchain): decided at the edge
      // detection, whose t0 is the swapchain target. The replaced resolve then never falls back to the game's, whose queue would be stale.
      if (original_shader_hashes.Contains(kFXAAEdgeHash, reshade::api::shader_stage::compute))
      {
         gd.fxaa_replaced = false;
         if ((g_smaa_enable || device_data.has_drawn_sr) && !is_custom_pass)
         {
            // fp16 only (as the resolve's guard below): without the HDR swapchain upgrade the game's FXAA runs whole
            com_ptr<ID3D11ShaderResourceView> color;
            native_device_context->CSGetShaderResources(0, 1, &color);
            uint4 unused_size;
            DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
            if (color)
            {
               GetResourceInfo(color.get(), unused_size, format);
            }
            gd.fxaa_replaced = format == DXGI_FORMAT_R16G16B16A16_FLOAT;
         }
         if (gd.fxaa_replaced)
         {
#if DEVELOPMENT
            gd.mv_stats.fxaa_skipped++;
#endif
            return DrawOrDispatchOverrideType::Replaced;
         }
      }
      if (gd.fxaa_replaced && (original_shader_hashes.Contains(kFXAAArgumentsHash, reshade::api::shader_stage::compute) || original_shader_hashes.Contains(kFXAAFirstResolveHash, reshade::api::shader_stage::compute)))
      {
#if DEVELOPMENT
         gd.mv_stats.fxaa_skipped++;
#endif
         return DrawOrDispatchOverrideType::Replaced;
      }

      // Replace the compute FXAA resolve with SMAA. Replace EVERY occurrence in the frame (the game can run the
      // resolve more than once — e.g. menu/transition frames have two), each with its own InColor/Color target.
      // After DLSS / FSR (which already antialiased the scene, FXAA would blur it) only RCAS runs, or nothing.
      if ((g_smaa_enable || device_data.has_drawn_sr) && !is_custom_pass &&
          original_shader_hashes.Contains(kFXAAResolveHash, reshade::api::shader_stage::compute))
      {
         // Without this chain's earlier passes the game's resolve would read a stale queue: anything that can't run here skips it
         const DrawOrDispatchOverrideType fallback = (gd.fxaa_replaced ? DrawOrDispatchOverrideType::Replaced : DrawOrDispatchOverrideType::None);
         // FXAA resolve is IN-PLACE: InColor (t2) aliases Color (u0 = swapchain), so D3D auto-unbinds the SRV at
         // dispatch (t2 reads null). We therefore source the scene color from the UAV's resource (it holds the
         // tonemapped pre-FXAA color, since we're replacing FXAA) by copying it into an SRV-capable temp.
         ComPtr<ID3D11UnorderedAccessView> uav_color;
         native_device_context->CSGetUnorderedAccessViews(0, 1, uav_color.put());
         if (!uav_color)
            return fallback;

         ComPtr<ID3D11Resource> color_res; // swapchain target (CopyResource source + destination)
         uav_color->GetResource(color_res.put());
         if (!color_res)
            return fallback;

         uint4 cinfo{};
         DXGI_FORMAT cfmt = DXGI_FORMAT_UNKNOWN;
         GetResourceInfo(color_res.get(), cinfo, cfmt);
         const uint32_t w = cinfo.x, h = cinfo.y;
         if (w == 0 || h == 0)
            return fallback;
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
            return fallback;
         }
         const bool smaa = !device_data.has_drawn_sr;
         if (!smaa && g_rcas_sharpness <= 0.f)
            return DrawOrDispatchOverrideType::Replaced;

         // A new color size (a resolution change) drops every resource sized like it; each is rebuilt below when missing (DrawSMAA
         // rebuilds its own intermediates)
         if (gd.smaa_w != w || gd.smaa_h != h)
         {
            gd.ReleaseSMAAScratch();
            gd.ReleaseSnapshotScratch();
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
            // misalign the mask against the color grid.
            pred_ok = GetViewTextureSize(gd.srv_depth.get()) == uint2{w, h};
         }
         if (pred_ok)
         {
            if (!gd.tex_pred && CreateDefaultTex(native_device, w, h, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, std::addressof(gd.tex_pred), DXGI_FORMAT_R16_FLOAT))
            {
               native_device->CreateUnorderedAccessView(gd.tex_pred.get(), nullptr, gd.uav_pred.put());
               native_device->CreateShaderResourceView(gd.tex_pred.get(), nullptr, gd.srv_pred.put());
            }
            const float pp[4] = {g_smaa_pred_tolerance, 0.f, 0.f, 0.f};
            pred_ok = gd.uav_pred && gd.srv_pred && PatchedDraws::WriteDynamicConstants(native_device, native_device_context, std::addressof(gd.cb_pred), pp, sizeof(pp));
         }
         const float pred_scale = pred_ok ? 2.f : 1.f;

         // Shader-readiness gate (async loader / dev live-reload): anything missing takes "fallback".
         if (smaa && (!AllShadersReady(device_data.native_pixel_shaders, {CompileTimeStringHash("SMAA Edge Detection PS"), CompileTimeStringHash("SMAA Blending Weight Calculation PS"), CompileTimeStringHash("SMAA Neighborhood Blending PS")}) ||
                        !AllShadersReady(device_data.native_vertex_shaders, {CompileTimeStringHash("SMAA Edge Detection VS"), CompileTimeStringHash("SMAA Blending Weight Calculation VS"), CompileTimeStringHash("SMAA Neighborhood Blending VS")})))
            return fallback;

         const float metrics[8] = {1.f / (float)w, 1.f / (float)h, (float)w, (float)h, pred_scale, 0.f, 0.f, 0.f};
         if (smaa && !PatchedDraws::WriteDynamicConstants(native_device, native_device_context, std::addressof(gd.cb_smaa_metrics), metrics, sizeof(metrics)))
            return fallback;

         auto* copy_vs = FindShader(device_data.native_vertex_shaders, CompileTimeStringHash("Copy VS"));
         auto* sharpen_ps = FindShader(device_data.native_pixel_shaders, CompileTimeStringHash("BL Sharpen PS"));
         bool do_sharpen = g_rcas_sharpness > 0.f && copy_vs != nullptr && sharpen_ps != nullptr;
         if (do_sharpen)
         {
            const float sp[4] = {(float)w, (float)h, g_rcas_sharpness, 0.f};
            do_sharpen = PatchedDraws::WriteDynamicConstants(native_device, native_device_context, std::addressof(gd.cb_sharpen), sp, sizeof(sp));
         }
         // After DLSS / FSR with RCAS not ready: the upscaled image stays as it is
         if (!smaa && !do_sharpen)
            return DrawOrDispatchOverrideType::Replaced;

         // Snapshot the in-place scene color out of the swapchain so SMAA / RCAS can read it
         if (!gd.srv_input && CreateDefaultTex(native_device, w, h, D3D11_BIND_SHADER_RESOURCE, std::addressof(gd.tex_input)))
         {
            native_device->CreateShaderResourceView(gd.tex_input.get(), nullptr, gd.srv_input.put());
         }
         if (!gd.srv_input)
            return fallback;
         native_device_context->CopyResource(gd.tex_input.get(), color_res.get());
         gd.snapshot_frame = cb_luma_global_settings.FrameIndex;

         if (smaa)
         {
            gd.smaa_frame = cb_luma_global_settings.FrameIndex;
            // The predication extract ("BL Depth Extract CS"). Core's Compute state stack restores the game's CS state and unbinds our
            // UAV before DrawSMAA reads it as an SRV (an SRV of a resource still bound as a UAV reads as null).
            if (pred_ok)
            {
               DrawStateStack<DrawStateStackType::Compute> compute_state;
               compute_state.Cache(native_device_context, device_data.uav_max_count);
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
               compute_state.Restore(native_device_context);
            }
         }

         // SMAA's output temp, when RCAS follows it or the swapchain target has no RTV
         const auto create_smaa_out = [&]()
         {
            gd.smaa_out_frame = cb_luma_global_settings.FrameIndex;
            if (!gd.tex_smaa_out && CreateDefaultTex(native_device, w, h, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET, std::addressof(gd.tex_smaa_out)))
            {
               native_device->CreateRenderTargetView(gd.tex_smaa_out.get(), nullptr, gd.tex_smaa_out_rtv.put());
               native_device->CreateShaderResourceView(gd.tex_smaa_out.get(), nullptr, gd.tex_smaa_out_srv.put());
            }
            return gd.tex_smaa_out_rtv && gd.tex_smaa_out_srv;
         };

#if DEVELOPMENT
         // Both calibration aids read the mask the extract CS just wrote. The numeric one runs first so it still
         // reports while the debug view is on (that path returns early).
         if (pred_ok)
            LogPredicationStats(native_device, native_device_context, gd);

         if (pred_ok && g_smaa_pred_debug)
         {
            auto* debug_ps = FindShader(device_data.native_pixel_shaders, CompileTimeStringHash("Copy PS"));
            if (copy_vs != nullptr && debug_ps != nullptr && create_smaa_out())
            {
               // The mask is single-channel, so the core copy lands it in RED - unmistakably a debug view. It
               // REPLACES the antialiased frame, reusing the same temp-then-copy route the fallback path takes.
               DrawStateStack<DrawStateStackType::FullGraphics> debug_state;
               debug_state.Cache(native_device_context, device_data.uav_max_count);
               DrawCustomPixelShader(native_device_context, device_data.default_depth_stencil_state.get(), device_data.default_blend_state.get(), nullptr,
                  copy_vs, debug_ps, gd.srv_pred.get(), gd.tex_smaa_out_rtv.get(), w, h, false);
               debug_state.Restore(native_device_context);
               native_device_context->CopyResource(color_res.get(), gd.tex_smaa_out.get());
               return DrawOrDispatchOverrideType::Replaced;
            }
         }
#endif

         // The last pass writes the swapchain target itself (no temp copied back), through a view made for this dispatch: a kept one would
         // hold the back buffer through a swapchain resize. Without one (a target without RTV binding), a temp copied into it.
         ComPtr<ID3D11RenderTargetView> color_rtv;
         native_device->CreateRenderTargetView(color_res.get(), nullptr, color_rtv.put());
         // The target is the resolve's CS UAV: D3D11 unbinds it there once it's bound as a render target, put back after
         DrawStateStack<DrawStateStackType::Compute> resolve_compute_state;
         resolve_compute_state.Cache(native_device_context, device_data.uav_max_count);

         // --- SMAA (3 passes), into the swapchain target, or into the temp RCAS reads (or that's copied into it) ---
         const bool smaa_into_temp = smaa && (do_sharpen || !color_rtv);
         ID3D11ShaderResourceView* sharpen_source = gd.srv_input.get();
         if (smaa)
         {
            if (smaa_into_temp && !create_smaa_out())
               return fallback;

            // Bind metrics at VS+PS b1 (DrawSMAA restores VS/PS/SRVs/RTs but NOT cbuffer slots).
            ComPtr<ID3D11Buffer> vs_cb1_orig, ps_cb1_orig;
            native_device_context->VSGetConstantBuffers(1, 1, vs_cb1_orig.put());
            native_device_context->PSGetConstantBuffers(1, 1, ps_cb1_orig.put());
            ID3D11Buffer* mcb = gd.cb_smaa_metrics.get();
            native_device_context->VSSetConstantBuffers(1, 1, &mcb);
            native_device_context->PSSetConstantBuffers(1, 1, &mcb);

            // The snapshot for both the edge detection and the blend (filtered in linear light, Luma_SMAA_impl.hlsl). Null predication
            // when invalid ("pred_ok"): with pred_scale 1.0 in the metrics, plain ULTRA.
            DrawSMAA(native_device, native_device_context, device_data,
               smaa_into_temp ? gd.tex_smaa_out_rtv.get() : color_rtv.get(), gd.srv_input.get(), gd.srv_input.get(),
               pred_ok ? gd.srv_pred.get() : nullptr /*predication (plane-deviation edge-ness)*/);

            ID3D11Buffer* vcb = vs_cb1_orig.get();
            ID3D11Buffer* pcb = ps_cb1_orig.get();
            native_device_context->VSSetConstantBuffers(1, 1, &vcb);
            native_device_context->PSSetConstantBuffers(1, 1, &pcb);
            sharpen_source = gd.tex_smaa_out_srv.get();
         }

         // --- Optional RCAS on the SMAA (or upscaled) output, into the swapchain target (or a temp copied into it) ---
         ID3D11RenderTargetView* sharpen_target = color_rtv.get();
         if (do_sharpen && !sharpen_target)
         {
            if (!gd.tex_rcas_out && CreateDefaultTex(native_device, w, h, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET, std::addressof(gd.tex_rcas_out)))
            {
               native_device->CreateRenderTargetView(gd.tex_rcas_out.get(), nullptr, gd.tex_rcas_out_rtv.put());
            }
            sharpen_target = gd.tex_rcas_out_rtv.get();
         }
         if (do_sharpen && sharpen_target)
         {
            // DrawCustomPixelShader does NOT restore state -> wrap in core's DrawStateStack<FullGraphics>.
            DrawStateStack<DrawStateStackType::FullGraphics> sharpen_state;
            sharpen_state.Cache(native_device_context, device_data.uav_max_count);

            ID3D11Buffer* scb = gd.cb_sharpen.get();
            native_device_context->PSSetConstantBuffers(0, 1, &scb);
            DrawCustomPixelShader(native_device_context, device_data.default_depth_stencil_state.get(), device_data.default_blend_state.get(), nullptr,
               copy_vs, sharpen_ps, sharpen_source, sharpen_target, w, h, false);

            sharpen_state.Restore(native_device_context);
            if (!color_rtv)
            {
               native_device_context->CopyResource(color_res.get(), gd.tex_rcas_out.get());
            }
         }
         else if (smaa_into_temp)
         {
            // SMAA went into its temp and no RCAS followed
            native_device_context->CopyResource(color_res.get(), gd.tex_smaa_out.get());
         }

         resolve_compute_state.Restore(native_device_context);
         return DrawOrDispatchOverrideType::Replaced; // cancel the FXAA resolve dispatch
      }
#endif // ENABLE_SMAA

      return DrawOrDispatchOverrideType::None;
   }

   void CleanExtraSRResources(DeviceData& device_data) override
   {
      GetGameDeviceData(device_data).release_sr_resources = true;
   }

   void OnPresent(ID3D11Device* native_device, DeviceData& device_data) override
   {
      auto& gd = GetGameDeviceData(device_data);

      // DLSS / FSR: the upscaler's history restarts after any frame it didn't draw (menus, loading, just picked); the selection and the
      // motion vector state are fixed here for the next frame (see "IsSRActive")
      device_data.force_reset_sr = !device_data.has_drawn_sr;
      device_data.has_drawn_sr = false;
      gd.sr_active = device_data.sr_type != SR::Type::None && !device_data.sr_suppressed;
      const bool mv_was_active = gd.mv_active.exchange(IsSRActive(device_data) || g_mv_enable);
      if (mv_was_active && !gd.mv_active)
      {
         // The copies stop following the uploads: a draw after the upscaler comes back must find none ("uncopied", zero motion) until its
         // buffer is uploaded again, not a stale one
         {
            const std::lock_guard lock(gd.mv_constants_mutex);
            for (auto& [buffer, copy] : gd.mv_constants_copies)
               copy.reset();
         }
         gd.mv_objects.clear();
         gd.mv_previous_objects.clear();
         gd.mv_camera.reset();
         gd.mv_previous_camera.reset();
      }
      // None picked: Core freed its upscaler resources and output, ours go too (recreated when an upscaler is picked again)
      if (device_data.sr_type == SR::Type::None && gd.release_sr_resources.exchange(false) && !g_mv_enable)
      {
         gd.sr_output_srv.reset();
         gd.sr_input = nullptr;
         {
            const std::unique_lock lock(gd.mv_mutex);
            gd.mv_texture.reset();
            gd.mv_rtv.reset();
            gd.mv_srv.reset();
            gd.mv_uav.reset();
         }
         gd.mv_depth_srv.reset();
         gd.mv_depth.reset();
         gd.mv_scene_color.reset();
         for (size_t i = 0; i < std::size(gd.dof_history); i++)
         {
            gd.dof_history[i].reset();
            gd.dof_history_srvs[i].reset();
            gd.dof_history_uavs[i].reset();
         }
         gd.dof_history_valid = false;
         gd.dof_history_cb.reset();
         gd.mv_fill_buffer.reset();
         gd.mv_jitter_buffer.reset();
         gd.mv_previous_constants = {};
      }
      // A scene no post pass ended ends here: its jitter must not reach the next frame's draws before the prepass
      gd.mv_scene_open = false;
      gd.mv_scene_done = false;
      gd.mv_frame_ended = true;
      gd.mv_fill_pending = false;
      gd.fxaa_replaced = false;
      if (!custom_texture_mip_lod_bias_offset)
      {
         const std::unique_lock lock(s_mutex_samplers);
         // -1 at native resolution (Core biases the anisotropic samplers, all of the game's with the AF16x upgrade)
         device_data.texture_mip_lod_bias_offset = IsSRActive(device_data) ? SR::GetMipLODBias(device_data.output_resolution.y, device_data.output_resolution.y) : 0.f;
      }
      // SMAA's resources go once it stopped running (the upscaler antialiases, or the game's AA is off), the snapshot too once RCAS
      // stopped as well; on the render thread, between frames
      if (cb_luma_global_settings.FrameIndex - gd.smaa_frame > smaa_idle_release_frames)
      {
         gd.ReleaseSMAAScratch();
         ReleaseSMAA(device_data);
      }
      if (cb_luma_global_settings.FrameIndex - gd.snapshot_frame > smaa_idle_release_frames)
      {
         gd.ReleaseSnapshotScratch();
      }
      // SMAA writes the swapchain itself once RCAS is off again: its temp goes too
      if (gd.tex_smaa_out && cb_luma_global_settings.FrameIndex - gd.smaa_out_frame > smaa_idle_release_frames)
      {
         gd.tex_smaa_out.reset();
         gd.tex_smaa_out_rtv.reset();
         gd.tex_smaa_out_srv.reset();
      }
      // The pooled constant copies only the pool holds (superseded, no object keeps them) are free for the next ones, as many as the
      // last frame asked for: a burst between two presents (loading screens) would keep its peak otherwise (74k copies, ~126 MB
      // measured). Without motion vectors, none.
      {
         const std::lock_guard lock(gd.mv_constants_mutex);
         size_t kept_free = std::exchange(gd.mv_constants_made, 0);
         if (!gd.mv_active)
         {
            kept_free = 0;
         }
         std::erase_if(gd.mv_constants_pool, [&](const auto& copy)
            {
               if (copy.use_count() != 1)
                  return false;
               if (kept_free == 0)
                  return true;
               kept_free--;
               return false; });
         for (auto& [size, free_copies] : gd.mv_constants_pool_free)
            free_copies.clear();
         for (uint32_t i = 0; i < uint32_t(gd.mv_constants_pool.size()); i++)
         {
            if (gd.mv_constants_pool[i].use_count() == 1)
            {
               gd.mv_constants_pool_free[gd.mv_constants_pool[i]->size()].push_back(i);
            }
         }
#if DEVELOPMENT
         gd.mv_stats.registered_buffers = gd.mv_filtered_buffer_count.load(std::memory_order_relaxed);
         gd.mv_stats.constants_pool = uint32_t(gd.mv_constants_pool.size());
#endif
      }
#if DEVELOPMENT
      // "Performance Test": closes this frame's timestamp set, reads back the finished ones (a log line every 120 frames, the first 60
      // after a change of mode or AA settings skipped), opens the next frame's
      com_ptr<ID3D11DeviceContext> native_device_context;
      native_device->GetImmediateContext(&native_device_context);
      if (auto* const queries = std::exchange(gd.perf_frame_queries, nullptr))
      {
         native_device_context->End(queries->frame_end.get());
         native_device_context->End(queries->disjoint.get());
         queries->pending = true;
      }
      if (g_perf_test != 0)
      {
         const auto now = std::chrono::steady_clock::now();
         const uint32_t settings = uint32_t(g_perf_test) | (uint32_t(int(device_data.sr_type) + 1) << 5) | (dlss_render_preset << 8) | (uint32_t(g_smaa_enable) << 16) | (uint32_t(g_gtao_enable) << 17) | (uint32_t(g_perf_hook_timers) << 18);
         // Also after a pause (the game stops presenting while unfocused): the upscaler history and the clocks restart
         if (std::exchange(gd.perf_settings, settings) != settings || now - gd.perf_last_present > std::chrono::milliseconds(250))
            gd.perf_settle_frames = 60;
         const bool measuring = gd.perf_settle_frames <= 0;
         auto& stats = gd.perf_stats;
         for (auto& queries : gd.perf_queries)
         {
            D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjoint;
            if (!queries.pending || native_device_context->GetData(queries.disjoint.get(), &disjoint, sizeof(disjoint), D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK)
               continue;
            queries.pending = false;
            if (!measuring)
               continue;
            if (disjoint.Disjoint || disjoint.Frequency == 0)
            {
               stats.disjoint++;
               continue;
            }
            const auto read = [&](const com_ptr<ID3D11Query>& query, UINT64* ticks)
            { return native_device_context->GetData(query.get(), ticks, sizeof(*ticks), D3D11_ASYNC_GETDATA_DONOTFLUSH) == S_OK; };
            const auto add = [&](UINT64 start, UINT64 end, double* total, double* max_ms, uint32_t* samples)
            {
               const double ms = 1000.0 * double(end - start) / double(disjoint.Frequency);
               *total += ms;
               *max_ms = (std::max)(*max_ms, ms);
               ++*samples;
            };
            UINT64 frame_start, frame_end, scene_start, fill_start, scene_end, sr_end, dof_start, dof_end;
            if (!read(queries.frame_start, &frame_start) || !read(queries.frame_end, &frame_end))
               continue;
            add(frame_start, frame_end, &stats.frame_ms, &stats.frame_max_ms, &stats.samples);
            if (queries.scene && read(queries.scene_start, &scene_start) && read(queries.scene_end, &scene_end))
            {
               add(scene_start, scene_end, &stats.scene_ms, &stats.scene_max_ms, &stats.scene_samples);
               if (queries.fill && read(queries.fill_start, &fill_start))
                  add(fill_start, scene_end, &stats.fill_ms, &stats.unused_max_ms, &stats.fill_samples);
               if (queries.sr && read(queries.sr_end, &sr_end))
               {
                  add(scene_end, sr_end, &stats.sr_ms, &stats.sr_max_ms, &stats.sr_samples);
               }
            }
            if (queries.dof && read(queries.dof_start, &dof_start) && read(queries.dof_end, &dof_end))
               add(dof_start, dof_end, &stats.dof_ms, &stats.unused_max_ms, &stats.dof_samples);
         }
         if (!measuring)
         {
            gd.perf_settle_frames--;
            stats = {};
            gd.perf_hook_ns = 0;
         }
         else
         {
            stats.cpu_frame_ms += std::chrono::duration<double, std::milli>(now - gd.perf_last_present).count();
            if (++stats.frames >= 120)
            {
               const auto average = [](double total, uint32_t samples)
               { return samples != 0 ? total / samples : 0.0; };
               std::string aa = g_smaa_enable ? "SMAA" : "None";
               if (IsSRActive(device_data))
                  aa = device_data.sr_type == SR::Type::FSR ? "FSR" : (dlss_render_preset != 0 ? std::format("DLSS_{}", char('A' + dlss_render_preset - 1)) : "DLSS_Default");
               const std::array<double, 6> window = {average(stats.frame_ms, stats.samples), average(stats.scene_ms, stats.scene_samples), average(stats.sr_ms, stats.sr_samples), double(gd.perf_hook_ns.exchange(0)) / 1e6 / stats.frames, average(stats.fill_ms, stats.fill_samples), average(stats.dof_ms, stats.dof_samples)};
               reshade::log::message(reshade::log::level::info, std::format("[BL Perf] mode=\"{}\" aa={} hook_timers={} gtao={} rcas={:.2f} output={}x{} gpu frame avg/max={:.3f}/{:.3f} ms scene avg/max={:.3f}/{:.3f} ms ({}) sr avg/max={:.3f}/{:.3f} ms ({}) cpu frame avg={:.3f} ms cpu hooks={:.3f} ms/frame fill={:.3f} dof={:.3f} ms ({}) samples={}/{} disjoint={}", perf_test_modes[g_perf_test].name, aa, g_perf_hook_timers, g_gtao_enable, g_rcas_sharpness, uint32_t(device_data.output_resolution.x), uint32_t(device_data.output_resolution.y), window[0], stats.frame_max_ms, window[1], stats.scene_max_ms, stats.scene_samples, window[2], stats.sr_max_ms, stats.sr_samples, stats.cpu_frame_ms / stats.frames, window[3], window[4], window[5], stats.dof_samples, stats.samples, stats.frames, stats.disjoint).c_str());
               stats = {};

               if (g_perf_sweep)
               {
                  gd.perf_sweep_results[g_perf_test].push_back(window);
                  if (++gd.perf_sweep_windows_done >= perf_sweep_windows)
                  {
                     gd.perf_sweep_windows_done = 0;
                     const int step = ++gd.perf_sweep_step;
                     if (step < perf_sweep_rounds * int(std::size(perf_sweep_modes)))
                     {
                        ApplyPerfTestMode(device_data, perf_sweep_modes[step % std::size(perf_sweep_modes)]);
                     }
                     else
                     {
                        // Per mode: the median window (and the frame's range), and the frame against the last mode's (No AA)
                        const auto median = [&](int mode, size_t column)
                        {
                           std::vector<double> values;
                           for (const auto& result : gd.perf_sweep_results[mode])
                              values.push_back(result[column]);
                           std::nth_element(values.begin(), values.begin() + values.size() / 2, values.end());
                           return values.empty() ? 0.0 : values[values.size() / 2];
                        };
                        const double baseline = median(perf_sweep_modes[std::size(perf_sweep_modes) - 1], 0);
                        for (const int mode : perf_sweep_modes)
                        {
                           const auto& results = gd.perf_sweep_results[mode];
                           const auto [min_frame, max_frame] = std::minmax_element(results.begin(), results.end(), [](const auto& a, const auto& b)
                              { return a[0] < b[0]; });
                           reshade::log::message(reshade::log::level::info, std::format("[BL Perf] sweep mode=\"{}\" hook_timers={} windows={} gpu frame median={:.3f} ms (min/max {:.3f}/{:.3f}, {:+.3f} vs \"{}\") scene median={:.3f} ms sr median={:.3f} ms cpu hooks median={:.3f} ms/frame fill={:.3f} dof={:.3f} ms (medians)", perf_test_modes[mode].name, g_perf_hook_timers, results.size(), median(mode, 0), results.empty() ? 0.0 : (*min_frame)[0], results.empty() ? 0.0 : (*max_frame)[0], median(mode, 0) - baseline, perf_test_modes[perf_sweep_modes[std::size(perf_sweep_modes) - 1]].name, median(mode, 1), median(mode, 2), median(mode, 3), median(mode, 4), median(mode, 5)).c_str());
                        }
                        g_perf_sweep = false;
                        ApplyPerfTestMode(device_data, 0);
                     }
                  }
               }
            }
         }
         gd.perf_last_present = now;

         auto& queries = gd.perf_queries[gd.perf_query_index];
         if (!queries.pending)
         {
            if (!queries.disjoint)
            {
               const D3D11_QUERY_DESC disjoint_desc = {D3D11_QUERY_TIMESTAMP_DISJOINT}, timestamp_desc = {D3D11_QUERY_TIMESTAMP};
               native_device->CreateQuery(&disjoint_desc, &queries.disjoint);
               for (auto* const query : {&queries.frame_start, &queries.scene_start, &queries.fill_start, &queries.scene_end, &queries.sr_end, &queries.dof_start, &queries.dof_end, &queries.frame_end})
                  native_device->CreateQuery(&timestamp_desc, &*query);
            }
            if (queries.disjoint && queries.frame_start && queries.scene_start && queries.fill_start && queries.scene_end && queries.sr_end && queries.dof_start && queries.dof_end && queries.frame_end)
            {
               native_device_context->Begin(queries.disjoint.get());
               native_device_context->End(queries.frame_start.get());
               queries.scene_started = queries.fill = queries.scene = queries.sr = queries.dof = false;
               gd.perf_frame_queries = &queries;
               gd.perf_query_index = (gd.perf_query_index + 1) % gd.perf_queries.size();
            }
         }
      }
      gd.mv_last_stats = std::exchange(gd.mv_stats, {});
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

      // XeGTAO inputs are captured per frame at the deinterleave / coarse passes; dropping them every present disarms the takeover,
      // so a frame without the AO chain (AO off in game, transitions) can't apply a stale AO buffer.
      gd.srv_gtao_depth.reset();
      gd.srv_gtao_normals.reset();

      device_data.has_drawn_main_post_processing = true;
   }

   void LoadConfigs() override
   {
      reshade::get_config_value(nullptr, NAME, "SMAAEnable", g_smaa_enable);
      reshade::get_config_value(nullptr, NAME, "RCASSharpness", g_rcas_sharpness);
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

      ImGui::SeparatorText("Ambient Occlusion");
      if (ImGui::Checkbox("XeGTAO Enable", &g_gtao_enable))
         reshade::set_config_value(nullptr, NAME, "XeGTAOEnable", g_gtao_enable);
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Replaces the game's HBAO+ with XeGTAO (cleaner, more accurate ambient occlusion).");
#if DEVELOPMENT || TEST
      // DEVELOPMENT/TEST calibration only (drive cb_gtao, written at the deinterleave hook). Not persisted.
      ImGui::BeginDisabled(!g_gtao_enable);
      ImGui::SliderFloat("GTAO Final Value Power", &g_gtao_final_value_power, 0.3f, 4.5f);                 // midtone-shadow contrast dial
      ImGui::SliderFloat("GTAO Depth Scale", &g_gtao_depth_scale, 1.f, 200.f);                             // UE3 units -> ~meters; the anti-over-occlusion dial
      ImGui::SliderFloat("GTAO Radius Override", &g_gtao_radius_override, 0.f, 5.f);                       // 0 = use EFFECT_RADIUS
#if DEVELOPMENT                                                                                            // Both knobs are constexpr outside it (the shader's DebugViewRT is DEVELOPMENT only)
      ImGui::Combo("GTAO Debug View", &g_gtao_debug_view, "Off\0Depth gradient\0Normals\0AO x8\0Edges\0"); // diagnostics through the AO apply
      ImGui::Combo("GTAO Temporal Noise", &g_gtao_temporal, "Auto (DLSS/FSR)\0Off (frozen, 2 denoise passes)\0On (frame % 64, 1 denoise pass)\0");
#endif
      ImGui::EndDisabled();
#endif

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
      ImGui::Checkbox("MV Debug View", &g_mv_debug_view);
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Shows the motion vector target (absolute, in pixels) through Core's debug draw.");
      ImGui::Text("Last frame: %u motion vector draws (%u matched, %u camera only, %u other camera, %u uncopied), %u jitter draws", stats.motion_vector_draws, stats.matched, stats.camera_only, stats.other_camera, stats.uncopied, stats.jitter_draws);
      ImGui::Text("Constant updates copied: %u, upscaler draws: %u, scene ended by 0x%08X", stats.updates, stats.sr_draws, stats.ended_by);
      std::string refused = "Refused:";
      for (size_t i = 0; i < std::size(mv_reject_names); i++)
         refused += std::format(" {} {}", stats.rejected[i], mv_reject_names[i]);
      ImGui::TextUnformatted(refused.c_str());
      ImGui::Text("Last checked target: format %u, dimension %u, %ux%u (output %.0fx%.0f)", stats.rejected_format, stats.rejected_dimension, stats.rejected_width, stats.rejected_height, double(device_data.output_resolution.x), double(device_data.output_resolution.y));

      ImGui::SeparatorText("Performance");
      auto& game_device_data = GetGameDeviceData(device_data);
      const std::string sweep_label = std::format("Sweep ({}/{})", game_device_data.perf_sweep_step + 1, perf_sweep_rounds * std::size(perf_sweep_modes));
      if (ImGui::BeginCombo("Performance Test", g_perf_sweep ? sweep_label.c_str() : perf_test_modes[g_perf_test].name))
      {
         for (int i = 0; i < int(std::size(perf_test_modes)); i++)
         {
            const PerfTestMode& mode = perf_test_modes[i];
            ImGui::BeginDisabled(!IsPerfTestModeAvailable(device_data, mode));
            if (ImGui::Selectable(mode.name, !g_perf_sweep && g_perf_test == i) && (g_perf_sweep || g_perf_test != i))
            {
               g_perf_sweep = false;
               ApplyPerfTestMode(device_data, i);
            }
            ImGui::EndDisabled();
         }
         ImGui::BeginDisabled(!device_data.sr_implementations_instances.contains(SR::Type::DLSS));
         if (ImGui::Selectable("Sweep", g_perf_sweep) && !g_perf_sweep)
         {
            for (auto& results : game_device_data.perf_sweep_results)
               results.clear();
            game_device_data.perf_sweep_step = 0;
            game_device_data.perf_sweep_windows_done = 0;
            g_perf_sweep = true;
            ApplyPerfTestMode(device_data, perf_sweep_modes[0]);
         }
         ImGui::EndDisabled();
         ImGui::EndCombo();
      }
      ImGui::Checkbox("Hook Timers", &g_perf_hook_timers);
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Times the motion vector hooks for \"cpu hooks\" (two clock reads per hooked draw and constant upload).\nRun a Sweep with it off to see their own cost in the frame times.");
   }
#endif

   void PrintImGuiAbout() override
   {
      ImGui::PushTextWrapPos(0.f);
      ImGui::Text(
         "Luma for \"Borderlands GOTY Enhanced\" is developed by DristoforColumb and is open source and free.\n"
         "It adds HDR and DLAA or FSR 3 native anti-aliasing, and replaces the game's FXAA with SMAA and its HBAO+ with XeGTAO, plus 16x anisotropic filtering and a fix for the game's movie memory leak.\n"
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
      // reads. r8g8b8a8 / b8g8r8a8 are left alone deliberately - nothing downstream needs them, and _srgb -> fp16 risks a
      // sampling shift.
      texture_upgrade_formats = {
         reshade::api::format::r10g10b10a2_unorm,
         reshade::api::format::r10g10b10a2_typeless,
         reshade::api::format::r11g11b10_float,
      };
      texture_format_upgrades_2d_size_filters = 0 | (uint32_t)TextureFormatUpgrades2DSizeFilters::SwapchainResolution | (uint32_t)TextureFormatUpgrades2DSizeFilters::SwapchainAspectRatio;
      force_disable_display_composition = false;

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
