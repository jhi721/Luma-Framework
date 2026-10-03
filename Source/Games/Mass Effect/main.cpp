// Mass Effect (2007) Luma HDR mod (UE3 2007, 32-bit, DX9 -> D3D11 via dgVoodoo2), cloned from the MoH Airborne port;
// hashes are dgVoodoo-translated, 2.87.3 and 2.81.3 keyed. No depth SRV and no motion vectors of its own: the upscalers' are
// built by patched shaders (see MotionVectorPatches.h).

// No auto-debugger MessageBox: invisible under borderless and it blocks the loader (ReShade error 1114, as BL2/TW2).
#define DISABLE_AUTO_DEBUGGER 1

#define GAME_MASS_EFFECT 1

#define GEOMETRY_SHADER_SUPPORT 0
// The game ships no AA at all (no option, no post AA pass, sampleCount 1), so SMAA adds rather than replaces.
#define ENABLE_SMAA 1
// Replaces the game's quarter-res bright-pass glow, which the replaced gather then stops writing.
#define ENABLE_BLOOM 1
// Outside DEVELOPMENT only this define makes original_draw_dispatch_func non-null; without it the callback never fires.
#define ENABLE_POST_DRAW_DISPATCH_CALLBACK 1
// The motion vector draw key reads the draw's arguments ("last_draw_dispatch_data")
#define ENABLE_DRAW_DISPATCH_DATA_CACHE 1

#include "..\..\Core\core.hpp"
#include "..\..\External\WDK\includes\d3d11TokenizedProgramFormat.hpp"
#include "MotionVectorPatches.h"
#include "..\..\Core\includes\patched_draws.h"
#if DEVELOPMENT
#include "..\..\Core\includes\perf_test.h"
#endif
#include <shellapi.h> // ShellExecuteA for About links (system() hangs the render thread in exclusive fullscreen)
#include <unordered_set>
#if DEVELOPMENT
#include <dxgi1_4.h> // "Memory Sweep": IDXGIAdapter3::QueryVideoMemoryInfo
#include <psapi.h>   // "Memory Sweep": GetProcessMemoryInfo
#endif

#if DEVELOPMENT
// "SR Bridge Profile": 600 upscaled frames of the SR bridge's steps (a GPU timestamp and a QPC time each, from "SR_BRIDGE_PROFILE")
// and of the frame (present to present) into "LumaBridgeProfile\game.csv" next to the exe, the helper's own into "helper.csv"
// ("LUMA_UPSCALER_PROFILE", set in this process only, for the restarted helper), joined by their bridge frame numbers.
namespace BridgeProfile
{
   constexpr int kSteps = 6; // SRBridge.cpp's "SR_BRIDGE_PROFILE" steps
   constexpr uint32_t kFrames = 600;
   constexpr uint32_t kWarmupFrames = 60; // After the helper is ready
   struct Frame
   {
      uint64_t bridge_n = 0; // The bridge's frame number (0: it didn't upscale)
      int64_t cpu_begin = 0, cpu_end = 0, cpu_step[kSteps] = {};
      com_ptr<ID3D11Query> disjoint, gpu_begin, gpu_end, gpu_step[kSteps];
   };
   enum class State
   {
      Off,
      Requested, // The helper restarts with the profile
      WarmingUp,
      Recording,
   };
   State state = State::Off;
   uint32_t warmup = 0;
   std::filesystem::path run_directory; // This profile's: the upscaler, the time and the experiments in its name
   std::vector<Frame> frames;
   Frame* current = nullptr; // The frame being recorded, until the next present

   // "SR Bridge Profile Sweep": these modes in turn, over rounds (interleaved, so the scene's drift averages out), a profile each,
   // then a "[ME1 Bridge] sweep" line per mode (the median of its runs' medians). The user's upscaler comes back.
   struct SweepMode
   {
      const char* name;
      SR::Type sr_type;
   };
   constexpr SweepMode kSweepModes[] = {
      {"DLSS", SR::Type::DLSS},
      {"FSR 3", SR::Type::FSR},
   };
   constexpr int kSweepRounds = 2;
   constexpr int kSweepSteps = int(std::size(kSweepModes)) * kSweepRounds;
   int sweep_step = -1; // -1: off
   SR::Type sweep_user_sr_type = SR::Type::None;
   // Per mode: each run's frame GPU time, the bridge's on the game's GPU queue and its CPU time in "Draw" (medians, ms)
   std::vector<double> sweep_results[std::size(kSweepModes)][3];
   std::filesystem::path sweep_directory;

   int64_t Now()
   {
      LARGE_INTEGER time;
      QueryPerformanceCounter(&time);
      return time.QuadPart;
   }

   com_ptr<ID3D11Query> Query(ID3D11DeviceContext* context, D3D11_QUERY type)
   {
      com_ptr<ID3D11Device> device;
      context->GetDevice(&device);
      const D3D11_QUERY_DESC desc = {type, 0};
      com_ptr<ID3D11Query> query;
      device->CreateQuery(&desc, &query);
      return query;
   }

   std::filesystem::path OutputDirectory()
   {
      return System::GetModulePath().parent_path() / "LumaBridgeProfile";
   }

   // "HHMMSS" now, for the output folders
   std::string TimeStamp()
   {
      SYSTEMTIME time;
      GetLocalTime(&time);
      return std::format("{:02}{:02}{:02}", time.wHour, time.wMinute, time.wSecond);
   }
} // namespace BridgeProfile

// Called by Core's SR bridge at each step of its frame (see "BridgeProfile.h")
void ProfileBridgeStep(ID3D11DeviceContext* context, int step, unsigned long long bridge_frame)
{
   auto* const frame = BridgeProfile::current;
   if (!frame)
      return;
   frame->bridge_n = bridge_frame; // The last step's: the bridge numbers its frame after the input copies
   frame->gpu_step[step] = BridgeProfile::Query(context, D3D11_QUERY_TIMESTAMP);
   context->End(frame->gpu_step[step].get());
   frame->cpu_step[step] = BridgeProfile::Now();
}
#endif

// Both replaced, separate register maps. The gamma correction pass ends every frame; the uber runs first when it
// runs at all - the engine skips it in elevators and some loading scenes, which b12 UberRanThisFrame reports.
static constexpr uint32_t kUberPostHash = 0xAC8341E0;        // HDR grade -> fp16 intermediate
static constexpr uint32_t kGammaCorrectionHash = 0x17CE0932; // canvas encode, the FINAL pass
// Engine copy scene B -> A. Not replaced; DEVELOPMENT dumps its gamma row: a non-1 exponent would move the encode here.
static constexpr uint32_t kCopyPassHash = 0x1E37D75B;

// UE3 DOFAndBloomGather, REPLACED: bloom and DoF blur share one quarter-res target, so only it can drop the glow.
static constexpr uint32_t kDofBloomGatherHash = 0x56854256;  // QualityBloom=TRUE, 16 taps
static constexpr uint32_t kDofBloomGather4Hash = 0x28F8DB16; // QualityBloom=FALSE, 4 taps
// UE3 DOFAndBloomBlend, REPLACED: the standalone DoF/bloom composite of uber-less chains (Space_PostProcess_DOF2, the
// default UI chains). It composites the Luma glow there, since the replaced gather drops the native one.
static constexpr uint32_t kDofBloomBlendHash = 0x218D4AE2;
// UE3 FilterPixelShader, 9 taps, NOT replaced: PS cb4[10..18] weights and VS cb4[25..29] offsets set the glow radius.
static constexpr uint32_t kBloomFilterHash = 0x6A1129DF;

// The same passes under dgVoodoo 2.81.3 (emits ps_4_0, so different hashes). Only C++-keyed hashes need a constant.
static constexpr uint32_t kUberPostHash_v281 = 0x786BC3B3;
static constexpr uint32_t kGammaCorrectionHash_v281 = 0x3BEF1CD6;
static constexpr uint32_t kCopyPassHash_v281 = 0xDDEAEB7C;
static constexpr uint32_t kDofBloomGatherHash_v281 = 0x4B65EEAE;
static constexpr uint32_t kDofBloomGather4Hash_v281 = 0xAA369C00;
static constexpr uint32_t kDofBloomBlendHash_v281 = 0x63B94FF3;
static constexpr uint32_t kBloomFilterHash_v281 = 0x464E33BB;

// DLAA / FSR Native AA: the scene's first post passes, which end a motion vector frame (the upscaler runs right before). The
// BioSceneEffect material and its Render_Style variants, else motion blur, normally come first; the others end uber-less or
// effect-less chains. Not the copy, a generic blit.
static constexpr uint32_t kScenePostHashes[] = {
   0xA640835C, // BioSceneEffect material
   0x79B4213D, // ... 2.81.3
   0x4DC34367, // Render_Style Filmgrain01
   0x17820D5D, // ... 2.81.3
   0x6D59058D, // Render_Style Warp
   0xC3FC2ADE, // ... 2.81.3
   0x099EF93B, // motion blur
   0x94B53023, // ... 2.81.3
   kDofBloomGatherHash,
   kDofBloomGatherHash_v281,
   kDofBloomGather4Hash,
   kDofBloomGather4Hash_v281,
   kDofBloomBlendHash,
   kDofBloomBlendHash_v281,
   kUberPostHash,
   kUberPostHash_v281,
   kGammaCorrectionHash,
   kGammaCorrectionHash_v281,
};

// Motion vectors for the upscaler (the game renders none, see MotionVectorPatches.h): the opaque draws into the fp16 scene draw with patched
// shaders that also write an extra target, the vertex shader's second run reading the draw's previous frame vc4.
// CPU optimizations of the hooks, each one off in its own "Performance Test" modes (A/B in the "CPU Sweep"). Always on outside
// DEVELOPMENT.
enum CpuOptimization : uint32_t
{
   // The buffer hooks tell the registered vc4 buffers apart without the lock and the lookup (every Map/Unmap of any resource paid
   // them), with their size kept (no GetDesc per copy)
   kCpuVc4Filter = 1 << 0,
   // A vc4 copy reuses a copy nobody holds anymore instead of a new allocation (4.4 KB per Unmap)
   kCpuVc4Pool = 1 << 1,
   // The per-target blend repair's checks remembered for the bound blend state (a GetDesc and a loop on every draw of the frame)
   kCpuBlendMemo = 1 << 2,
   kCpuAll = (1 << 3) - 1,
};
#if DEVELOPMENT
static uint32_t g_cpu_optimizations = kCpuAll;
static uint32_t g_perf_cpu_optimizations_off = 0; // The running "Performance Test" mode's
static bool IsCpuOptimized(uint32_t optimization)
{
   return (g_cpu_optimizations & ~g_perf_cpu_optimizations_off & optimization) != 0;
}
#else
constexpr bool IsCpuOptimized(uint32_t optimization)
{
   return true;
}
#endif
#if DEVELOPMENT
static float g_sr_reactive_scale = 1.f;      // FSR reactive mask: the alpha blended draws' reactivity, scaled (AMD's default 1)
static float g_sr_reactive_threshold = 0.5f; // Under it 0, over it 0.9 (AMD's 0.2; 0.5 tuned in game: lower shakes static glows); 0: the scaled reactivity, capped at 0.9
static bool g_mv_enable = false;
static bool g_mv_debug_view = false;
static bool g_mv_force_jitter = false;   // The projection jitter without an upscaler
static bool g_mv_disable_jitter = false; // No projection jitter under the upscaler (A/B of jitter-dependent artifacts)
// "Performance Test" (see "OnPresent"): GPU timestamps and hook CPU time to ReShade.log ("[ME1 Perf]"). A mode sets the
// anti-aliasing while it runs (the user's is restored on "Off" or "Current Settings", and never saved). The mode is "Perf::g_test".
struct PerfTestMode
{
   const char* name;
   bool set_aa = false; // Else the current anti-aliasing (the next three fields unused)
   SR::Type sr_type = SR::Type::None;
   bool smaa = false;
   bool reactive_mask = false;
   int motion_vector_draws = 2;        // 2 patched (motion vectors and jitter), 1 jitter only, 0 untouched (unjittered)
   uint32_t cpu_optimizations_off = 0; // "CpuOptimization" bits
};
constexpr PerfTestMode perf_test_modes[] = {
   {"Off"},
   {"Current Settings"},
   {"DLSS", true, SR::Type::DLSS},
   {"FSR 3 + Reactive Mask", true, SR::Type::FSR, false, true},
   {"FSR 3", true, SR::Type::FSR},
   {"FSR 3 Jitter Only", true, SR::Type::FSR, false, false, 1},
   {"FSR 3 Without Motion Vector Draws", true, SR::Type::FSR, false, false, 0},
   {"SMAA", true, SR::Type::None, true},
   {"No AA", true, SR::Type::None, false},
   {"FSR 3 No CPU Optimizations", true, SR::Type::FSR, false, false, 2, kCpuAll},
   {"FSR 3 Without vc4 Filter", true, SR::Type::FSR, false, false, 2, kCpuVc4Filter},
   {"FSR 3 Without vc4 Pool", true, SR::Type::FSR, false, false, 2, kCpuVc4Pool},
   {"FSR 3 Without Blend Memo", true, SR::Type::FSR, false, false, 2, kCpuBlendMemo},
   {"No AA No CPU Optimizations", true, SR::Type::None, false, false, 2, kCpuAll},
};
// Experiments on the game to helper handoff of the SR bridge (see "SR Bridge Profile"): a flush right before the bridge's
// frame (the frame's work submitted first, the input copies and the signal on their own), and the game device's GPU thread
// priority ("IDXGIDevice::SetGPUThreadPriority", -7 to 7). Not saved.
static bool g_bridge_flush_before = false;
static int g_gpu_thread_priority = 0;
// "Sweep": these modes in turn, a log window each, over several rounds, then a median per mode against the last one ("No AA")
constexpr int perf_sweep_modes[] = {2, 3, 4, 5, 6, 7, 8};
// "CPU Sweep": the CPU optimizations each off in turn, with and without an upscaler
constexpr int perf_cpu_sweep_modes[] = {4, 9, 10, 11, 12, 13, 8};
static_assert(std::string_view(perf_test_modes[perf_sweep_modes[std::size(perf_sweep_modes) - 1]].name) == "No AA");
static_assert(std::string_view(perf_test_modes[perf_cpu_sweep_modes[std::size(perf_cpu_sweep_modes) - 1]].name) == "No AA");
constexpr Perf::SweepDef perf_sweeps[] = {{"Sweep", perf_sweep_modes}, {"CPU Sweep", perf_cpu_sweep_modes}};
// Per mode, each window's GPU frame, scene, end, hook and CPU frame times, and the end's parts. A log window (120 frames) per mode and
// round.
enum PerfColumn : size_t
{
   PERF_COLUMN_FRAME,
   PERF_COLUMN_SCENE,
   PERF_COLUMN_TAIL,
   PERF_COLUMN_HOOKS,
   PERF_COLUMN_CPU_FRAME,
   PERF_COLUMN_FILL,
   PERF_COLUMN_UPSCALER,
   PERF_COLUMN_COPY_BACK,
   PERF_COLUMN_SCENE_COPY,
   PERF_COLUMN_COUNT
};
static Perf::Sweep<PERF_COLUMN_COUNT> g_perf_sweep = {.defs = perf_sweeps, .rounds = 3, .windows = 1};
constexpr int perf_settle_frames = 30; // Skipped after a settings change (history reset, targets rebuilt) and the upscaler being ready
// The scene's tail, from its end: after the fill (with the reactive mask), the upscaler and its copy back, and the tail's end
enum PerfStamp : size_t
{
   PERF_SCENE_START = Perf::FIRST_GAME_STAMP,
   PERF_SCENE_END,
   PERF_FILL_END,
   PERF_UPSCALER_END,
   PERF_COPY_END,
   PERF_SCENE_TAIL_END,
   PERF_STAMP_COUNT
};
static int GetPerfMotionVectorDraws()
{
   return perf_test_modes[Perf::g_test].motion_vector_draws;
}
// "Memory Sweep": these modes in order, one "[ME1 Mem]" log line each once settled (the upscaler ready, then a fixed frame count),
// so the deltas between lines are each feature's cost. The user's settings are restored at the end.
struct MemorySweepMode
{
   const char* name;
   SR::Type sr_type = SR::Type::None;
   bool smaa = false;
   bool bloom = false;
   float rcas = 0.f;
   bool reactive_mask = false;
};
constexpr MemorySweepMode memory_sweep_modes[] = {
   {"No AA, No Bloom"},
   {"SMAA", SR::Type::None, true},
   {"SMAA + Bloom", SR::Type::None, true, true},
   {"SMAA + Bloom + RCAS", SR::Type::None, true, true, 0.5f},
   {"DLSS + SMAA On + Bloom", SR::Type::DLSS, true, true},
   {"DLSS + SMAA Off + Bloom", SR::Type::DLSS, false, true},
   {"FSR 3 + Reactive Mask + Bloom", SR::Type::FSR, false, true, 0.f, true},
   {"No AA + Bloom (after SR)", SR::Type::None, false, true},
   {"No AA, No Bloom (after all)"},
};
constexpr int memory_sweep_settle_frames = 720;    // Past SMAA's idle release ("smaa_idle_release_frames")
constexpr int memory_sweep_max_wait_frames = 3000; // For the upscaler to get ready
static int g_memory_sweep_step = -1;               // -1 off
static bool g_mv_dump_scene = false;               // One frame of the scene's draws to ReShade.log (route, blend, depth)
static bool g_sr_reactive_debug_view = false;
static bool g_sr_tc_from_mask = false;       // The reactive mask as FSR's transparency & composition mask too, instead of the draws' own (OptiScaler does it)
static bool g_sr_reactive_pass = true;       // Off: the mask still runs, FSR doesn't get it (isolation test)
static bool g_sr_reactive_skip_fill = false; // The draws still write their mask, the fill doesn't pass it on (isolation test)
static bool g_sr_reactive_zero_test = false; // The mask cleared to 0 but still passed (isolates FSR's reaction to having one)
static bool g_sr_reactive_enable = true;
#else
static constexpr float g_sr_reactive_scale = 1.f;
static constexpr float g_sr_reactive_threshold = 0.5f;
static constexpr bool g_mv_enable = false;
static constexpr bool g_mv_force_jitter = false;
static constexpr bool g_mv_disable_jitter = false;
static constexpr bool g_sr_reactive_enable = true;
static constexpr int GetPerfMotionVectorDraws()
{
   return 2;
}
static constexpr bool g_sr_reactive_pass = true;
static constexpr bool g_sr_reactive_skip_fill = false;
#endif

// Luma bloom pyramid mip 0 for the uber/blend replacements; clear of t0 (scene) and t1 (blur), the only slots they declare.
static constexpr uint32_t kLumaBloomSlot = 6;
// One sigma per mip, count taken FROM the array so the two cannot drift (MELE). Blended 0.5/0.5 = energy-preserving.
static constexpr float g_bloom_sigmas[] = {1.5f, 2.f, 2.f, 2.f, 1.f, 0.5f};

static bool g_hide_ui = false; // Session-only, never persisted: a stuck "on" would look like a broken HUD
#if DEVELOPMENT
// ReShade's runtime for "Take Screenshot" (the settings callback isn't given it)
static reshade::api::effect_runtime* g_effect_runtime = nullptr;
#endif
#if ENABLE_SMAA
static bool g_smaa_enable = true;
static bool g_smaa_predication = true;      // on geometry, from the depth in the scene buffer's alpha
static float g_smaa_pred_tolerance = 0.02f; // a fraction of view depth
// SMAA's resources go after this many presents without it (the upscaler antialiasing, see "OnPresent"): ~5 s, so menus and
// loading screens between upscaled frames, which run SMAA, don't recreate them each time
constexpr uint32_t smaa_idle_release_frames = 600;
#if DEVELOPMENT
static_assert(memory_sweep_settle_frames > int(smaa_idle_release_frames));
#endif
// RCAS on the SMAA or upscaler output, opt-in (BL2/TW2); 0 = off (see "tex_smaa_out").
static float g_rcas_sharpness = 0.f;
#if DEVELOPMENT
// Calibration aid: predication's effect is the ABSENCE of smearing, which the eye misjudges - judge the mask instead.
static bool g_smaa_pred_debug = false;   // show the predication mask instead of the antialiased frame
static bool g_smaa_pred_measure = false; // one-shot: log the mask's coverage above 0.5 and percentiles
#endif
#endif
#if DEVELOPMENT
// One-shot dump of the copy/uber/gamma cb4 grade rows, disarmed by the gamma pass. On demand: each dump blocks.
static bool g_dump_pass_cb = false;
#endif

// Mirrored into GameSettings.LumaBloomEnable for the composites (uber, blend) and the replaced gather: one switch swaps
// the blooms.
static bool g_luma_bloom_enable = true;

struct MassEffectGameDeviceData final : public GameDeviceData
{
   // Repaired blend states, keyed by the ORIGINAL desc (DXHR) so a released state cannot leave a stale key behind.
   struct BlendDescCompare
   {
      bool operator()(const D3D11_BLEND_DESC& a, const D3D11_BLEND_DESC& b) const
      {
         return memcmp(&a, &b, sizeof(D3D11_BLEND_DESC)) < 0;
      }
   };
   std::map<D3D11_BLEND_DESC, ComPtr<ID3D11BlendState>, BlendDescCompare> fixed_blend_states;

   bool uber_ran_this_frame = false; // bloom injected, scene captured
   // "Canvas captured, SMAA done" is core's device_data.has_drawn_main_post_processing, set at the final pass.
   // An unkeyed dgVoodoo build fails SILENTLY (format upgrades still fire, replacements don't); reported after warmup.
   bool ever_matched_final_pass = false;
   uint32_t frames_presented = 0;
#if DEVELOPMENT
   // One-shot per DEVICE, not per process: dgVoodoo recreates the device on a resize, which is worth re-reading.
   bool diag_logged_gather = false;
   bool diag_logged_rt = false;
   // Vanilla bloom kernel capture, one-shot: the two filter draws of one frame (H then V) and the gather's constants.
   uint32_t diag_filter_dumps = 0;
   // HUD family: each lands on the fp16 mirror unclamped and needs a saturating UI_* replacement. Unreplaced PS only.
   std::unordered_set<uint32_t> diag_post_final_ps;
#endif
   // Deferred cbuffer readback (MoHA/MELE): copy at the draw, Map the copy from two frames ago without waiting.
   struct DeferredCBRing
   {
      static constexpr uint32_t kSlots = 3;
      ComPtr<ID3D11Buffer> staging[kSlots];
      uint32_t bytes = 0;
      uint32_t writes = 0;
      // One advance per frame (re-armed at Present): more would read copies still in flight; the Map never lands.
      bool advanced_this_frame = false;
   };
   // Gamma pass 1/display gamma (cb4[11].x, measured 0.625 = 1/1.6; uber and copy are 1.0), sent via GameSettings.
   DeferredCBRing gamma_cb_ring;
   float gamma_inverse_live = 0.f;
   bool gamma_inverse_valid = false;
   // Gather BloomScale (cb4[11].x, measured 0.1), same scheme. Per post-process volume in UE3, so it changes per area.
   DeferredCBRing bloom_cb_ring;
   float bloom_scale_live = 0.f;
   // Validity is a FLAG, not the sign: bloom off in a volume reports BloomScale 0, a real value the shader must get.
   bool bloom_scale_valid = false;

   // The canvas the final color pass wrote, from its bound RTV. Used by Hide UI and SMAA. Released every Present.
   ComPtr<ID3D11Resource> canvas_res;

   // A reference to core's DrawBloom mip 0 (the chain is core's, see "ReleaseBloom"). Rebuilt every frame.
   ComPtr<ID3D11ShaderResourceView> srv_luma_bloom;

   // The scene from t0 of the uber (of the gamma pass when the engine skips the uber): the bloom source, and its ALPHA carries the
   // linear depth SMAA predication reads.
   ComPtr<ID3D11ShaderResourceView> srv_scene;

   // ---- DLAA / FSR Native AA (BL GOTY's motion vector path, one per-draw buffer: dgVoodoo's vc4) ----
   // DLSS and FSR run in the x64 helper of Core's SR bridge (the game is 32-bit, see "SRBridge.h").
   // "IsSRActive", taken at present: Core's "Super Resolution" selection changes after it, mid frame for the draws
   bool sr_active = false;
   // The upscaler's output goes back into the scene (or its copy, see "mv_scene_copy") without its alpha (the linear depth the post
   // passes read), drawn from this view of it
   com_ptr<ID3D11BlendState> sr_rgb_blend_state;
   com_ptr<ID3D11ShaderResourceView> sr_output_srv;
   // None was picked ("CleanExtraSRResources", from the overlay): the upscaler's resources go at the next present
   std::atomic<bool> release_sr_resources = false;
#if DEVELOPMENT
   bool mv_dumping = false; // This frame's scene draws go to the log ("MV Dump Scene Draws")
   uint32_t mv_dump_index = 0;
   // "Performance Test": GPU timestamps per frame (present to present, the scene from its opening to its first post pass, the end
   // of the scene: fill (with the reactive mask), the upscaler and copies, see "PerfStamp"); and the CPU time in the scene hooks
   struct PerfStats
   {
      Perf::Stat frame, scene, end;
      Perf::Stat end_parts[4]; // The end of the scene: fill, upscaler, copy back, scene copy (the rest)
   };
   Perf::TimestampRing<PERF_STAMP_COUNT> perf_timestamps;
   Perf::Window<PerfStats> perf_window;
   // The user's anti-aliasing while a mode that sets its own runs
   SR::Type perf_user_sr_type = SR::Type::None;
   bool perf_user_smaa = false;
   bool perf_user_reactive_mask = true;
   // "Memory Sweep": frames left before the current mode's log line, frames waited for the upscaler, and the user's settings to restore
   int memory_sweep_settle_frames = 0;
   int memory_sweep_wait_frames = 0;
   int gpu_thread_priority = 0;                        // "g_gpu_thread_priority" as last set on the device
   std::unordered_set<uint64_t> canvas_resources_seen; // The final pass's render targets (mirrors when upgraded), tagged in the report
   SR::Type memory_user_sr_type = SR::Type::None;
   bool memory_user_smaa = false;
   bool memory_user_bloom = false;
   float memory_user_rcas = 0.f;
   bool memory_user_reactive_mask = true;
#endif
   std::atomic<bool> mv_active = false; // Motion vectors and jitter this frame: an upscaler is active, or the DEV toggle (set at present)
   std::shared_mutex mv_mutex;
   // A game shader's patched version (null if refused), patched on first use, by its hash; a vertex shader's with the bytes of
   // vc4 it reads ("DXBC::ConstantBufferBytes"): the previous frame's copy uploads only those
   template <typename T>
   struct PatchedShader
   {
      com_ptr<T> shader;
      UINT read_size = 0;
      UINT translation_offset = 0; // Vertex shaders: vc4's LocalToWorld translation row (c8, skinned c233)
   };
   std::unordered_map<uint32_t, PatchedShader<ID3D11VertexShader>> mv_vertex_shaders;
   std::unordered_map<uint32_t, PatchedShader<ID3D11PixelShader>> mv_pixel_shaders;
   // The alpha blended draws' pixel shaders with the mask target, by blend (see "ClassifyBoundBlend", index - 1)
   std::unordered_map<uint32_t, PatchedShader<ID3D11PixelShader>> mv_reactive_pixel_shaders[2];
   // The motion vector target (sized like the scene; every blend state writes it unblended, see "OnCreateBlendState")
   com_ptr<ID3D11Texture2D> mv_texture;
   com_ptr<ID3D11RenderTargetView> mv_rtv;
   com_ptr<ID3D11UnorderedAccessView> mv_uav; // Null without typed UAV loads of its format (then no upscaler)
   // The upscaler's depth, built from the scene's alpha by the fill (the game's depth has no shader resource view)
   com_ptr<ID3D11Texture2D> mv_device_depth;
   com_ptr<ID3D11UnorderedAccessView> mv_device_depth_uav;
   // A frame opens at its first mesh draw into output sized depth (the jitter is chosen there), starts at its first motion vector
   // draw (the target is cleared) and ends at the first post pass ("kScenePostHashes"), once per present.
   bool mv_scene_open = false;
   bool mv_scene_done = false;
   bool mv_frame_ended = true;
   bool mv_fill_pending = false;
   float sr_vert_fov = 1.0471976f; // FSR's vertical FOV (radians): the last camera's, 60 degrees until one is seen
   // FSR's reactive and transparency & composition masks (written by the fill from "mv_reactive_target", see
   // "Luma_ME1_MotionVectorFill.hlsl")
   com_ptr<ID3D11Texture2D> mv_reactive;
   com_ptr<ID3D11UnorderedAccessView> mv_reactive_uav;
   com_ptr<ID3D11Texture2D> mv_transparency;
   com_ptr<ID3D11UnorderedAccessView> mv_transparency_uav;
   // The masks the alpha blended draws write (R8G8: x reactive from all, y transparency & composition from the non-additive
   // ones; max blended, see "OnCreateBlendState"), read by the fill
   com_ptr<ID3D11Texture2D> mv_reactive_target;
   com_ptr<ID3D11RenderTargetView> mv_reactive_target_rtv;
   com_ptr<ID3D11ShaderResourceView> mv_reactive_target_srv;
   com_ptr<ID3D11Resource> mv_depth; // The scene depth (the depth view's resource)
   // The fp16 scene the motion vector draws write, and the game's view of it (the upscaler's copy back)
   com_ptr<ID3D11Resource> mv_scene_color;
   com_ptr<ID3D11RenderTargetView> mv_scene_rtv;
   com_ptr<ID3D11ShaderResourceView> mv_scene_srv; // The fill's view of it, kept while the scene is the same resource
   // The copy of the scene the first post pass reads (UE3 resolves the scene surface into a texture), null if it reads none. The
   // upscaler's output goes into the copy only: the first post pass overwrites the scene before anything reads it again (measured,
   // "scene_reads_after_end"). Into both if the pass also reads the scene or the copy is not render target bindable.
   com_ptr<ID3D11Resource> mv_scene_copy;
   com_ptr<ID3D11RenderTargetView> mv_scene_copy_rtv;
   bool mv_scene_read_by_end = false; // The first post pass reads the scene itself
   com_ptr<ID3D11Buffer> mv_fill_buffer;
   // The projection jitter (pixels, +y down), chosen when the scene opens; its NDC offset is at VS "MotionVectorPatches::jitter_slot"
   // of every mesh draw depth tested against the scene
   std::array<float, 2> mv_jitter = {};
   std::array<float, 2> mv_jitter_ndc = {}; // The same offset in NDC (y up), as the jitter buffer holds it
   com_ptr<ID3D11Buffer> mv_jitter_buffer;
   // Per-draw lookups kept for the next draw (reset when the scene opens): the jitter path's last depth view and whether it's the scene
   // depth, its last depth stencil state's depth test, the motion vector path's last accepted targets and the last blend state's
   // opacity, the last vertex and pixel shader's patched versions (owned by the maps above, never erased)
   ID3D11DepthStencilView* jitter_dsv = nullptr;
   bool jitter_dsv_scene = false;
   ID3D11DepthStencilState* jitter_depth_stencil_state = nullptr;
   bool jitter_depth_test = true;
   ID3D11DepthStencilView* mv_accepted_dsv = nullptr;
   ID3D11BlendState* mv_blend_state = nullptr; // See "ClassifyBoundBlend"
   bool mv_blend_opaque = true;
   uint8_t mv_reactive_blend = 0;
   uint32_t mv_last_vertex_shader_hash = 0;
   ID3D11VertexShader* mv_last_vertex_shader = nullptr;
   UINT mv_last_vertex_read_size = 0;
   UINT mv_last_vertex_translation_offset = 0;
   uint32_t mv_last_pixel_shader_hash = 0;
   ID3D11PixelShader* mv_last_pixel_shader = nullptr;
   PatchedDraws::BoundShader<ID3D11VertexShader> mv_bound_vertex_shader;
   PatchedDraws::BoundShader<ID3D11PixelShader> mv_bound_pixel_shader;

   // CPU copies of the vc4 buffers the motion vector draws bind, by buffer (an entry registers it, null until its first upload), from
   // a Map(WRITE_DISCARD) at its Unmap or an UpdateSubresource: a draw's constants are its buffer's latest copy
   using ConstantsCopy = std::shared_ptr<const std::vector<uint8_t>>;
   std::mutex mv_constants_mutex;
   std::unordered_map<uint64_t, ConstantsCopy> mv_constants_copies;
   std::unordered_map<uint64_t, void*> mv_mapped_constants; // Registered buffers mapped now, until their Unmap
   // "kCpuVc4Filter": the first registered buffers and their sizes, read by the buffer hooks without the lock (written under it, on
   // the immediate context's thread as the hooks). With more registered, every buffer takes the lock as without the filter.
   static constexpr uint32_t kMaxFilteredBuffers = 8;
   std::array<std::atomic<uint64_t>, kMaxFilteredBuffers> mv_filtered_buffers = {};
   std::array<UINT, kMaxFilteredBuffers> mv_filtered_buffer_sizes = {};
   std::atomic<uint32_t> mv_filtered_buffer_count = 0;
   std::atomic<bool> mv_filter_overflow = false;
   // "kCpuVc4Pool": every pooled copy, and those nobody held anymore at the last present (taken by the next copies). Under
   // "mv_constants_mutex".
   std::vector<std::shared_ptr<std::vector<uint8_t>>> mv_constants_pool;
   std::vector<uint32_t> mv_constants_pool_free;
   size_t mv_constants_made = 0; // Copies asked for since the last present
   // Previous frame constants of the motion vector draws (see "PatchedDraws::PreviousConstants")
   PatchedDraws::PreviousConstants mv_previous_constants;
   // Motion vector draws by draw key (shaders, buffers, arguments), with LocalToWorld and vc4. A draw takes the previous frame's vc4
   // of its key's nearest draw (same object, a frame earlier), its camera included.
   struct MotionVectorObject
   {
      PatchedDraws::ObjectTransform transform;
      ConstantsCopy constants;
   };
   std::unordered_map<uint64_t, std::vector<MotionVectorObject>> mv_objects;
   std::unordered_map<uint64_t, std::vector<MotionVectorObject>> mv_previous_objects;
   // The frame's camera (vc4 of its first motion vector draw) and the previous frame's
   ConstantsCopy mv_camera;
   ConstantsCopy mv_previous_camera;
   uint32_t mv_frame_index = 0;              // The Luma frame index of the last motion vector frame
   std::vector<uint8_t> mv_camera_only_copy; // An unmatched draw's constants with last frame's camera (reused)

#if DEVELOPMENT
   // Per frame counts for the DEV panel (the last complete frame's shown)
   struct MotionVectorStats
   {
      uint32_t motion_vector_draws = 0, jitter_draws = 0, reactive_draws = 0, matched = 0, camera_only = 0, other_camera = 0, uncopied = 0, maps = 0, updates = 0, other_maps = 0, sr_draws = 0;
      uint32_t offset_bindings = 0;     // vc4 bound at a constant buffer offset (a ring: the per-buffer copies would be wrong)
      uint32_t constants_pool = 0;      // At present: the pooled copies (see "NewConstantsCopy")
      uint32_t tiebreak_collisions = 0; // The previous frame's, see "PatchedDraws::CountTieBreakCollisions"
      uint32_t ended_by = 0;
      float near_plane = 0.f, far_plane = 0.f;                                                       // The upscaler's, from the camera's projection (0: none found)
      int ended_by_scene_slot = -1;                                                                  // The PS slot the ending pass reads the scene (or its copy) from, -1 if neither
      uint32_t scene_reads_after_end = 0;                                                            // Draws, dispatches and copies reading the scene itself after its end, before a pass or copy overwrote it
      uint32_t scene_reader = 0;                                                                     // The last such draw's PS (or CS) hash
      uint32_t rejected[8] = {};                                                                     // "DrawWithMotionVectors" refusals by reason ("MV_REJECT")
      uint32_t rejected_format = 0, rejected_dimension = 0, rejected_width = 0, rejected_height = 0; // The last target refused by format or size
   };
   MotionVectorStats mv_stats, mv_last_stats;
   int mv_draw_reject = -1;               // The current draw's "MV_REJECT" reason (-1 for none), for the MCP trace note
   uint64_t mv_scene_reads_after_end = 0; // The session's total of "MotionVectorStats::scene_reads_after_end"
   bool mv_scene_overwritten = false;     // A pass or copy wrote the scene after its end (later reads see that, not the upscaled scene)
#endif

#if ENABLE_SMAA
   // ---- SMAA (TW2/BL2 shape, see RunPostFinalGradeSMAA) ----
   // Metrics CB (b1) = (1/w, 1/h, w, h) + (predication scale, 0, 0, 0).
   // Canvas size every resource below (and core's DrawSMAA intermediates) was created for. A change releases them all
   // at once; each is then recreated on first use, so no resource tracks a size of its own.
   uint32_t smaa_w = 0, smaa_h = 0;
   uint32_t smaa_idle_frames = 0; // Presents since SMAA last ran
   ComPtr<ID3D11Buffer> cb_smaa_metrics;
   float smaa_metrics_pred_scale = -1.f;
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

   // RCAS. The intermediate exists ONLY while sharpening is on: at 0 the pass never runs and the last SMAA pass writes the
   // canvas directly, saving a write-back.
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

   // Turning the feature off (or the upscaler antialiasing) gives the memory back: ~80 MB at 4K here, plus core's intermediates
   // (~83 MB, "ReleaseSMAA").
   void ReleaseSMAAScratch()
   {
      srv_input.reset();
      tex_input.reset();
      ReleasePredicationScratch();
      ReleaseSharpenScratch();
   }
#endif
};

class MassEffect final : public Game
{
   // Pass identity by hash, folding both dgVoodoo builds (MoHA ContainsPixelShader, TW2 IsTonemap, BL2 IsBL2Tonemap).
   static bool ContainsPixelShader(const ShaderHashesList<OneShaderPerPipeline>& hashes, uint32_t hash, uint32_t hash_v281)
   {
      return hashes.Contains(hash, reshade::api::shader_stage::pixel) || hashes.Contains(hash_v281, reshade::api::shader_stage::pixel);
   }

   static bool IsDofBloomGather(const ShaderHashesList<OneShaderPerPipeline>& hashes)
   {
      return ContainsPixelShader(hashes, kDofBloomGatherHash, kDofBloomGatherHash_v281) || ContainsPixelShader(hashes, kDofBloomGather4Hash, kDofBloomGather4Hash_v281);
   }

   static bool IsUberPostPass(const ShaderHashesList<OneShaderPerPipeline>& hashes)
   {
      return ContainsPixelShader(hashes, kUberPostHash, kUberPostHash_v281);
   }

   // The last colour pass before the HUD: it writes the canvas, so the capture, Hide UI and SMAA all key on it.
   static bool IsFinalColorPass(const ShaderHashesList<OneShaderPerPipeline>& hashes)
   {
      return ContainsPixelShader(hashes, kGammaCorrectionHash, kGammaCorrectionHash_v281);
   }

   static MassEffectGameDeviceData& GetGameDeviceData(DeviceData& device_data)
   {
      return *static_cast<MassEffectGameDeviceData*>(device_data.game);
   }

#if ENABLE_BLOOM
   // Pyramid off the fp16 LINEAR scene at t0, pre-glow, bound at t6 for the pass compositing it: the uber, or the
   // standalone DOFAndBloom blend on uber-less chains (no cooked chain has both, so the glow is never doubled).
   static void BindLumaBloom(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, DeviceData& device_data, ID3D11ShaderResourceView* srv_scene)
   {
      auto& gd = GetGameDeviceData(device_data);
      gd.srv_luma_bloom.reset(); // DrawBloom AddRef's its mip 0 into this
      if (g_luma_bloom_enable && srv_scene)
      {
         DrawStateStack<DrawStateStackType::FullGraphics> bloom_state;
         bloom_state.Cache(native_device_context, device_data.uav_max_count);

         // Karis average first, so fireflies die spatially (only an upscaler antialiases temporally). The sigmas are in mip
         // texels: resolution-independent.
         ComPtr<ID3D11ShaderResourceView> srv_karis;
         DrawKarisAverage(native_device, native_device_context, device_data, srv_scene, srv_karis.put());
         if (srv_karis)
            DrawBloom(native_device, native_device_context, device_data, srv_karis.get(), (int)std::size(g_bloom_sigmas), g_bloom_sigmas, gd.srv_luma_bloom.put());

         bloom_state.Restore(native_device_context);
      }
      // Bound every time, null included (the composite gates on LumaBloomEnable): a stale slot samples garbage.
      ID3D11ShaderResourceView* bloom_srv = gd.srv_luma_bloom.get();
      native_device_context->PSSetShaderResources(kLumaBloomSlot, 1, &bloom_srv);
   }
#endif

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

   // Pass the canvas' live format: CopyResource requires source and destination to match.
   static bool CreateDefaultTex(ID3D11Device* device, uint32_t w, uint32_t h, UINT bind_flags, ComPtr<ID3D11Texture2D>& out, DXGI_FORMAT format)
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

   // One cbuffer row from the copy two frames ago. False when unreadable: fresh ring, failed alloc or slot in flight.
   static bool ReadCBRowDeferred(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, ID3D11Buffer* cb,
      MassEffectGameDeviceData::DeferredCBRing& ring, uint32_t row, float out[4])
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

      constexpr uint32_t kSlots = MassEffectGameDeviceData::DeferredCBRing::kSlots;
      native_device_context->CopyResource(ring.staging[ring.writes % kSlots].get(), cb);
      ring.writes++;
      ring.advanced_this_frame = true;
      if (ring.writes < kSlots)
         return false; // nothing old enough to read yet

      // The next slot to be overwritten is the oldest: the copy issued kSlots - 1 frames ago.
      ID3D11Buffer* oldest = ring.staging[ring.writes % kSlots].get();
      D3D11_MAPPED_SUBRESOURCE mapped = {};
      if (FAILED(native_device_context->Map(oldest, 0, D3D11_MAP_READ, D3D11_MAP_FLAG_DO_NOT_WAIT, &mapped)) || mapped.pData == nullptr)
         return false; // still in flight; try again next frame rather than blocking
      std::memcpy(out, (const uint8_t*)mapped.pData + (size_t)row * 16, 16);
      native_device_context->Unmap(oldest, 0);
      return true;
   }

   // Track one row of dgVoodoo's PS constant mirror (b4), deferred so the Map never stalls. The row differs per pass.
   static void TrackCB4Row(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context,
      MassEffectGameDeviceData::DeferredCBRing& ring, uint32_t row_index, float min_valid, float max_valid, float* out, bool* out_valid)
   {
      ComPtr<ID3D11Buffer> cb;
      native_device_context->PSGetConstantBuffers(4, 1, cb.put());
      float row[4];
      if (!ReadCBRowDeferred(native_device, native_device_context, cb.get(), ring, row_index, row))
         return;
      if (row[0] >= min_valid && row[0] <= max_valid)
      {
         *out = row[0];
         *out_valid = true; // separate from the value: min_valid can legitimately BE 0 (bloom)
      }
   }

   // dgVoodoo leaves blending ON for a secondary RT with RT0 off; D3D9 has one global state (TW2 water 0xDA16C815).
   static bool FixImpossiblePerRTBlend(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, MassEffectGameDeviceData& gd, reshade::api::shader_stage stages, const ShaderHashesList<OneShaderPerPipeline>& original_shader_hashes, bool is_custom_pass, std::function<void()>* original_draw_dispatch_func)
   {
      // Injected passes set their blend deliberately; without the dispatch func the draw cannot be re-issued.
      if (is_custom_pass || (stages & reshade::api::shader_stage::pixel) == 0 || original_draw_dispatch_func == nullptr)
         return false;

      ComPtr<ID3D11BlendState> blend_state;
      FLOAT blend_factor[4];
      UINT sample_mask = 0;
      native_device_context->OMGetBlendState(blend_state.put(), blend_factor, &sample_mask);
      if (!blend_state)
         return false; // no state object = default (blending off everywhere)

      // Only BOUND game targets count: stale BlendEnable in unused slots would match nearly every single-target draw, and
      // Luma's own slots (from "MotionVectorPatches::target_slot") differ by design (see "OnCreateBlendState").
      constexpr UINT game_targets = MotionVectorPatches::target_slot;
      // "kCpuBlendMemo": the checks on the state alone, remembered for it. Per thread (this runs on every context) and per frame (a
      // released state's pointer can come back as another's).
      struct BlendMemo
      {
         ID3D11BlendState* state = nullptr;
         uint32_t frame = UINT32_MAX;
         bool candidate = false;
         D3D11_BLEND_DESC desc;
      };
      thread_local BlendMemo memo;
      const bool memoized = IsCpuOptimized(kCpuBlendMemo) && memo.state == blend_state.get() && memo.frame == cb_luma_global_settings.FrameIndex;
      if (memoized && !memo.candidate)
         return false;
      D3D11_BLEND_DESC queried;
      if (!memoized)
      {
         D3D11_BLEND_DESC& bd = queried;
         blend_state->GetDesc(&bd);
         // One state for all targets is already D3D9-shaped
         bool candidate = bd.IndependentBlendEnable;
#if !DEVELOPMENT
         // Only "RT0 off, RTn on" is repaired, so outside DEVELOPMENT a blending RT0 skips the scan and the RT query.
         candidate &= !bd.RenderTarget[0].BlendEnable;
#endif
         bool disagreement = false;
         for (UINT i = 1; i < game_targets && !disagreement; i++)
            disagreement = bd.RenderTarget[i].BlendEnable != bd.RenderTarget[0].BlendEnable;
         candidate &= disagreement;
         if (IsCpuOptimized(kCpuBlendMemo))
         {
            memo.state = blend_state.get();
            memo.frame = cb_luma_global_settings.FrameIndex;
            memo.candidate = candidate;
            if (candidate)
               memo.desc = bd;
         }
         if (!candidate)
            return false;
      }
      const D3D11_BLEND_DESC& bd = memoized ? memo.desc : queried;
      const bool rt0_blending = bd.RenderTarget[0].BlendEnable != FALSE;

      // RT0's blend bit is loop-invariant, so the two shapes are mutually exclusive: one flag out of the loop.
      ID3D11RenderTargetView* rtvs[game_targets] = {};
      native_device_context->OMGetRenderTargets(game_targets, rtvs, nullptr);
      bool bound_disagreement = false;
      for (UINT i = 0; i < game_targets; i++)
      {
         if (rtvs[i] == nullptr)
            continue;
         bound_disagreement |= i > 0 && bd.RenderTarget[i].BlendEnable != bd.RenderTarget[0].BlendEnable;
         rtvs[i]->Release(); // references handed back; only bound/not-bound matters
      }
      const bool needs_fix = bound_disagreement && !rt0_blending;
      [[maybe_unused]] const bool inverse_shape = bound_disagreement && rt0_blending;

#if DEVELOPMENT
      // One line per distinct shader. Locked: this function runs on every context, so two can reach the set.
      if (needs_fix || inverse_shape)
      {
         static std::mutex logged_shaders_mutex;
         static std::unordered_set<uint64_t> logged_shaders;
         const uint64_t pixel_shader_hash = original_shader_hashes.pixel_shaders[0];
         const std::scoped_lock logged_shaders_lock(logged_shaders_mutex);
         if (logged_shaders.emplace(pixel_shader_hash).second)
         {
            reshade::log::message(reshade::log::level::warning,
               std::format("[ME1-BlendFix] impossible per-RT blend state (dgVoodoo artefact) on pixel shader 0x{:X} - {}", pixel_shader_hash, needs_fix ? "repaired" : "inverse shape, left alone").c_str());
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
         for (UINT i = 1; i < game_targets; i++)
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

   // The resource behind bound RTV 0. Identifies the canvas and tests later draws against it; Hide UI needs it to ship.
   static ComPtr<ID3D11Resource> GetBoundRenderTargetResource(ID3D11DeviceContext* native_device_context)
   {
      ComPtr<ID3D11Resource> res;
      ComPtr<ID3D11RenderTargetView> rtv;
      native_device_context->OMGetRenderTargets(1, rtv.put(), nullptr);
      if (rtv)
         rtv->GetResource(res.put());
      return res;
   }

#if DEVELOPMENT
   // The only honest read of an indirect upgrade: the devkit reports the original handle either way (BL2).
   static void DumpBoundRenderTarget(ID3D11DeviceContext* native_device_context, const char* label)
   {
      char msg[256];
      ComPtr<ID3D11RenderTargetView> rtv;
      native_device_context->OMGetRenderTargets(1, rtv.put(), nullptr);
      if (!rtv)
      {
         std::snprintf(msg, sizeof(msg), "[Luma] ME1 DIAG: %s RTV0 NOT BOUND", label);
         reshade::log::message(reshade::log::level::info, msg);
         return;
      }
      uint4 size;
      DXGI_FORMAT format;
      GetResourceInfo(rtv.get(), size, format);
      std::snprintf(msg, sizeof(msg), "[Luma] ME1 DIAG: %s RTV0 %ux%u res_fmt %s -> upgrade %s",
         label, size.x, size.y, GetFormatNameSafe(format),
         format == DXGI_FORMAT_R16G16B16A16_FLOAT ? "OK (fp16)" : "MISSING (not fp16)");
      reshade::log::message(reshade::log::level::info, msg);
   }

   // One-shot synchronous readback of b4: the devkit shows the buffers but not their contents. Stalls once per pass.
   static void DumpConstantRows(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, const char* label, uint32_t first_row, uint32_t row_count, bool vertex_stage = false)
   {
      ComPtr<ID3D11Buffer> cb;
      if (vertex_stage)
         native_device_context->VSGetConstantBuffers(4, 1, cb.put());
      else
         native_device_context->PSGetConstantBuffers(4, 1, cb.put());
      if (!cb)
         return;
      D3D11_BUFFER_DESC bd = {};
      cb->GetDesc(&bd);
      if (bd.ByteWidth < (first_row + row_count) * 16)
         return;
      D3D11_BUFFER_DESC sd = {};
      sd.ByteWidth = bd.ByteWidth;
      sd.Usage = D3D11_USAGE_STAGING;
      sd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
      ComPtr<ID3D11Buffer> staging;
      if (FAILED(native_device->CreateBuffer(&sd, nullptr, staging.put())))
         return;
      native_device_context->CopyResource(staging.get(), cb.get());
      D3D11_MAPPED_SUBRESOURCE mapped = {};
      if (FAILED(native_device_context->Map(staging.get(), 0, D3D11_MAP_READ, 0, &mapped)) || mapped.pData == nullptr)
         return;
      const float* rows = (const float*)mapped.pData;
      for (uint32_t r = first_row; r < first_row + row_count; r++)
      {
         char msg[256];
         std::snprintf(msg, sizeof(msg), "[Luma] ME1 DIAG: %s cb4[%u] = %.6f %.6f %.6f %.6f", label, r, rows[r * 4 + 0], rows[r * 4 + 1], rows[r * 4 + 2], rows[r * 4 + 3]);
         reshade::log::message(reshade::log::level::info, msg);
      }
      native_device_context->Unmap(staging.get(), 0);
   }

#if ENABLE_SMAA
   // Calibration readback (MoHA method): coverage above 0.5 = geometry fraction, typically ~1%. One-shot; stalls.
   static void MeasurePredicationMask(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, ID3D11Texture2D* pred)
   {
      // Desc taken off the mask, not rebuilt: CopyResource requires agreement and the walk must cover what was copied.
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
      std::snprintf(msg, sizeof(msg), "[Luma] ME1 DIAG: SMAA predication mask %ux%u tolerance %.4f -> above 0.5 = %.2f%% | mean %.4f | p50 %.4f p90 %.4f p99 %.4f | NaN %llu",
         td.Width, td.Height, g_smaa_pred_tolerance, 100.0 * (double)above_half / (double)total, sum / (double)total, percentile(0.50), percentile(0.90), percentile(0.99), (unsigned long long)nans);
      reshade::log::message(reshade::log::level::info, msg);
   }
#endif // ENABLE_SMAA
#endif // DEVELOPMENT

   // ---- DLAA / FSR Native AA with motion vectors and jitter from patched shaders (BL GOTY's path; vc4 layout in "MotionVectorPatches") ----
   // c0-c3, row vectors
   static constexpr size_t kViewProjectionOffset = MotionVectorPatches::view_projection_row * 16;
   // ... and c4, the camera position
   static constexpr size_t kCameraSize = 5 * 16;
   // c8: LocalToWorld's translation row
   static constexpr size_t kTranslationOffset = (MotionVectorPatches::view_projection_row + 8) * 16;
   // Skinned vertex shaders (bones from c5) hold LocalToWorld at c230-c233
   static constexpr size_t kSkinnedTranslationOffset =
      (MotionVectorPatches::view_projection_row + 233) * 16;
   // A camera ("mv_camera") comes from constants holding the translation row, so it always holds the whole camera
   static_assert(kTranslationOffset + 16 >= kViewProjectionOffset + kCameraSize);

   // An upscaler is picked and hasn't failed (it then gives way to SMAA until picked again). Fixed for the whole frame (see
   // "OnPresent"): a selection made after the motion vector state was set would otherwise run the upscaler on mixed state.
   static bool IsSRActive(DeviceData& device_data)
   {
      return GetGameDeviceData(device_data).sr_active;
   }

   // "kCpuVc4Filter": false if the buffer surely isn't a registered vc4 one; "size" its size if known (else 0). Lock free.
   static bool MayBeRegisteredBuffer(const MassEffectGameDeviceData& gd, uint64_t handle, UINT* size)
   {
      *size = 0;
      if (!IsCpuOptimized(kCpuVc4Filter) || gd.mv_filter_overflow.load(std::memory_order_relaxed))
         return true;
      const uint32_t count = gd.mv_filtered_buffer_count.load(std::memory_order_acquire);
      for (uint32_t i = 0; i < count; i++)
      {
         if (gd.mv_filtered_buffers[i].load(std::memory_order_relaxed) == handle)
         {
            *size = gd.mv_filtered_buffer_sizes[i];
            return true;
         }
      }
      return false;
   }

   // A buffer registered in "mv_constants_copies" joins the filter ("kCpuVc4Filter"). Under "mv_constants_mutex".
   static void AddFilteredBuffer(MassEffectGameDeviceData& gd, ID3D11Buffer* buffer)
   {
      const uint32_t count = gd.mv_filtered_buffer_count.load(std::memory_order_relaxed);
      if (count >= MassEffectGameDeviceData::kMaxFilteredBuffers)
      {
         gd.mv_filter_overflow = true;
         return;
      }
      D3D11_BUFFER_DESC desc;
      buffer->GetDesc(&desc);
      gd.mv_filtered_buffer_sizes[count] = desc.ByteWidth;
      gd.mv_filtered_buffers[count].store(reinterpret_cast<uint64_t>(buffer), std::memory_order_relaxed);
      gd.mv_filtered_buffer_count.store(count + 1, std::memory_order_release);
   }

   static UINT GetBufferSize(uint64_t handle, UINT known_size)
   {
      if (known_size != 0)
         return known_size;
      D3D11_BUFFER_DESC desc;
      reinterpret_cast<ID3D11Buffer*>(handle)->GetDesc(&desc);
      return desc.ByteWidth;
   }

   // A vc4 copy of "size" bytes from "bytes" (null: zeroed): one nobody held anymore at the last present ("kCpuVc4Pool"), else a
   // new one. Under "mv_constants_mutex".
   static std::shared_ptr<std::vector<uint8_t>> NewConstantsCopy(MassEffectGameDeviceData& gd, const uint8_t* bytes, size_t size)
   {
      gd.mv_constants_made++;
      if (IsCpuOptimized(kCpuVc4Pool) && !gd.mv_constants_pool_free.empty())
      {
         auto copy = gd.mv_constants_pool[gd.mv_constants_pool_free.back()];
         gd.mv_constants_pool_free.pop_back();
         if (bytes)
            copy->assign(bytes, bytes + size);
         else
            copy->assign(size, 0);
         return copy;
      }
      auto copy = bytes ? std::make_shared<std::vector<uint8_t>>(bytes, bytes + size) : std::make_shared<std::vector<uint8_t>>(size);
      if (IsCpuOptimized(kCpuVc4Pool))
         gd.mv_constants_pool.push_back(copy);
      return copy;
   }

   // Motion vectors: a registered vc4 buffer mapped for a whole rewrite, remembered until its Unmap
   static void OnMapBufferRegion(reshade::api::device* device,
      reshade::api::resource resource, uint64_t offset,
      uint64_t size, reshade::api::map_access access,
      void** data)
   {
      DeviceData* const device_data = device->get_private_data<DeviceData>();
      if (!device_data || !device_data->game ||
          !GetGameDeviceData(*device_data).mv_active || !data || !*data)
         return;
      auto& gd = GetGameDeviceData(*device_data);
#if DEVELOPMENT
      const Perf::HookTimer perf_timer{&gd.perf_window.hook_ns};
#endif
      UINT buffer_size;
      if (!MayBeRegisteredBuffer(gd, resource.handle, &buffer_size))
         return;
      const std::lock_guard lock(gd.mv_constants_mutex);
      if (!gd.mv_constants_copies.contains(resource.handle))
         return;
      if (access == reshade::api::map_access::write_discard && offset == 0)
         gd.mv_mapped_constants[resource.handle] = *data;
#if DEVELOPMENT
      else
         gd.mv_stats.other_maps++; // A partial or appending write: the copies would miss it
#endif
   }

   // Motion vectors: the CPU copy of a vc4 buffer, before its Unmap (the game has written it). Reads the mapped memory back.
   static void OnUnmapBufferRegion(reshade::api::device* device,
      reshade::api::resource resource)
   {
      DeviceData* const device_data = device->get_private_data<DeviceData>();
      if (!device_data || !device_data->game ||
          !GetGameDeviceData(*device_data).mv_active)
         return;
      auto& gd = GetGameDeviceData(*device_data);
#if DEVELOPMENT
      const Perf::HookTimer perf_timer{&gd.perf_window.hook_ns};
#endif
      UINT buffer_size;
      if (!MayBeRegisteredBuffer(gd, resource.handle, &buffer_size))
         return;
      const std::lock_guard lock(gd.mv_constants_mutex);
      const auto mapped = gd.mv_mapped_constants.find(resource.handle);
      if (mapped == gd.mv_mapped_constants.end())
         return;
      gd.mv_constants_copies[resource.handle] = NewConstantsCopy(gd, static_cast<const uint8_t*>(mapped->second), GetBufferSize(resource.handle, buffer_size));
      gd.mv_mapped_constants.erase(mapped);
#if DEVELOPMENT
      gd.mv_stats.maps++;
#endif
   }

   // Motion vectors: the CPU copy of a vc4 buffer from an UpdateSubresource (before it runs); a partial update is merged into the
   // last copy. Needs the unsigned "full add-on support" ReShade (the signed build doesn't raise the event).
   static bool OnUpdateBufferRegion(reshade::api::device* device, const void* data,
      reshade::api::resource resource,
      uint64_t offset, uint64_t size)
   {
      DeviceData* const device_data = device->get_private_data<DeviceData>();
      if (!device_data || !device_data->game || !data ||
          !GetGameDeviceData(*device_data).mv_active)
         return false;
      auto& gd = GetGameDeviceData(*device_data);
#if DEVELOPMENT
      const Perf::HookTimer perf_timer{&gd.perf_window.hook_ns};
#endif
      UINT known_size;
      if (!MayBeRegisteredBuffer(gd, resource.handle, &known_size))
         return false;
      const std::lock_guard lock(gd.mv_constants_mutex);
      const auto copy = gd.mv_constants_copies.find(resource.handle);
      if (copy == gd.mv_constants_copies.end())
         return false;
      const UINT buffer_size = GetBufferSize(resource.handle, known_size);
      if (offset >= buffer_size)
         return false;
      const size_t updated_size =
         size_t((std::min)(size, uint64_t(buffer_size) - offset));
      const auto* const bytes = static_cast<const uint8_t*>(data);
      if (updated_size == buffer_size)
      {
         copy->second = NewConstantsCopy(gd, bytes, updated_size);
      }
      else
      {
         const bool merge = copy->second && copy->second->size() == buffer_size;
         auto updated = NewConstantsCopy(gd, merge ? copy->second->data() : nullptr, buffer_size);
         std::memcpy(updated->data() + offset, bytes, updated_size);
         copy->second = std::move(updated);
      }
#if DEVELOPMENT
      gd.mv_stats.updates++;
#endif
      return false;
   }

   // The bound shader's motion vector version (or, "reactive" > 0, a pixel shader's reactive mask one, see "ClassifyBoundBlend"),
   // patched from Core's bytecode copy on first use (null if it can't be, e.g. a vertex shader that doesn't place vertices with the
   // view projection)
   template <typename T>
   static MassEffectGameDeviceData::PatchedShader<T>
   GetMotionVectorShader(ID3D11Device* native_device, DeviceData& device_data,
      std::unordered_map<uint32_t, MassEffectGameDeviceData::PatchedShader<T>>* shaders,
      uint32_t hash, reshade::api::pipeline pipeline, uint8_t reactive = 0)
   {
      constexpr bool vertex = std::is_same_v<T, ID3D11VertexShader>;
      auto& gd = GetGameDeviceData(device_data);
      {
         const std::shared_lock lock(gd.mv_mutex);
         if (const auto it = shaders->find(hash); it != shaders->end())
            return it->second;
      }
      std::vector<uint8_t> patched;
      std::string error = "no bytecode";
      UINT read_size = 0;
      UINT translation_offset = kTranslationOffset;
      {
         const std::shared_lock lock(s_mutex_generic);
         if (const auto it =
                device_data.pipeline_cache_by_pipeline_handle.find(pipeline.handle);
            it != device_data.pipeline_cache_by_pipeline_handle.end() &&
            it->second->subobjects_cache)
         {
            const auto* desc = static_cast<const reshade::api::shader_desc*>(
               it->second->subobjects_cache[0].data);
            const auto* code = static_cast<const uint8_t*>(desc->code);
            if constexpr (vertex)
            {
               patched = MotionVectorPatch::PatchVertexShader(code, desc->code_size, MotionVectorPatches::layout, &error);
               read_size = DXBC::ConstantBufferBytes(code, desc->code_size, MotionVectorPatches::object_slot);
               if (DXBC::ReadsConstantRow(code, desc->code_size, MotionVectorPatches::object_slot, kSkinnedTranslationOffset / 16))
                  translation_offset = kSkinnedTranslationOffset;
            }
            else if (reactive != 0)
               patched = MotionVectorPatch::PatchPixelShaderReactive(code, desc->code_size, MotionVectorPatches::layout,
                  MotionVectorPatches::reactive_slot, reactive == 2, &error);
            else
               patched = MotionVectorPatch::PatchPixelShader(code, desc->code_size, MotionVectorPatches::layout, &error, true);
         }
      }
      com_ptr<T> shader;
      if (!patched.empty())
      {
         HRESULT hr;
         if constexpr (vertex)
            hr = native_device->CreateVertexShader(patched.data(), patched.size(),
               nullptr, &shader);
         else
            hr = native_device->CreatePixelShader(patched.data(), patched.size(),
               nullptr, &shader);
         if (FAILED(hr))
            error = std::format("create 0x{:08X}", uint32_t(hr));
      }
      // Failures in every build (bug reports), except the expected screen space refusals; every patched shader only in development
      const bool screen_space = error.starts_with("screen space");
      if (DEVELOPMENT || (!shader && !screen_space))
         reshade::log::message(
            (shader || screen_space) ? reshade::log::level::info
                                     : reshade::log::level::warning,
            std::format("[ME1 MV] {} 0x{:08X} {}", vertex ? "VS" : (reactive == 0 ? "PS" : (reactive == 2 ? "PS reactive additive" : "PS reactive alpha")), hash,
               shader ? "patched" : error)
               .c_str());
      const std::unique_lock lock(gd.mv_mutex);
      return shaders->try_emplace(hash, MassEffectGameDeviceData::PatchedShader<T>{shader, read_size, translation_offset}).first->second;
   }

   // The bound vertex shader's patched version (null if refused), looked up again only when the game's changes
   static ID3D11VertexShader*
   GetPatchedVertexShader(ID3D11Device* native_device,
      CommandListData& cmd_list_data, DeviceData& device_data,
      uint32_t hash)
   {
      auto& gd = GetGameDeviceData(device_data);
      if (hash != gd.mv_last_vertex_shader_hash)
      {
         const auto patched = GetMotionVectorShader(native_device, device_data, &gd.mv_vertex_shaders, hash, cmd_list_data.pipeline_state_original_vertex_shader);
         gd.mv_last_vertex_shader = patched.shader.get();
         gd.mv_last_vertex_read_size = patched.read_size;
         gd.mv_last_vertex_translation_offset = patched.translation_offset;
         gd.mv_last_vertex_shader_hash = hash;
      }
      return gd.mv_last_vertex_shader;
   }

   // Classifies the bound blend state (cached in "mv_blend_state"): "mv_blend_opaque" for the motion vectors (additive lights,
   // decals and translucents keep the motion vectors of what's behind them, and so do colorless draws: the occlusion query
   // bounding boxes), and "mv_reactive_blend": 0 not alpha blended (opaque, lights ONE/ONE, shadows DEST_COLOR), 1 alpha blended
   // (SRC_ALPHA / INV_SRC_ALPHA: smoke, glass, water; reactive and transparency & composition), 2 additive (SRC_ALPHA / ONE: sparks,
   // glows; reactive)
   static void ClassifyBoundBlend(ID3D11DeviceContext* native_device_context, MassEffectGameDeviceData* gd)
   {
      com_ptr<ID3D11BlendState> blend_state;
      native_device_context->OMGetBlendState(&blend_state, nullptr, nullptr);
      if (blend_state.get() == gd->mv_blend_state)
         return;
      D3D11_BLEND_DESC blend_desc = CD3D11_BLEND_DESC(D3D11_DEFAULT);
      if (blend_state)
         blend_state->GetDesc(&blend_desc);
      const D3D11_RENDER_TARGET_BLEND_DESC& rt0 = blend_desc.RenderTarget[0];
      gd->mv_blend_opaque = rt0.RenderTargetWriteMask != 0 &&
                            (!rt0.BlendEnable || (rt0.SrcBlend == D3D11_BLEND_ONE && rt0.DestBlend == D3D11_BLEND_ZERO && rt0.BlendOp == D3D11_BLEND_OP_ADD));
      gd->mv_reactive_blend = (!rt0.BlendEnable || rt0.SrcBlend != D3D11_BLEND_SRC_ALPHA) ? 0 : (rt0.DestBlend == D3D11_BLEND_ONE ? 2 : 1);
      gd->mv_blend_state = blend_state.get();
   }

   // Opens the scene at the frame's first mesh draw into output sized depth: takes the scene depth and picks the jitter the whole
   // scene draws with. Once per present (the HUD and later passes never reopen it).
   static void OpenScene(ID3D11Device* native_device,
      ID3D11DeviceContext* native_device_context,
      CommandListData& cmd_list_data, DeviceData& device_data,
      uint32_t vertex_shader_hash,
      ID3D11DepthStencilView* dsv)
   {
      auto& gd = GetGameDeviceData(device_data);
      uint4 depth_size;
      DXGI_FORMAT unused_format;
      GetResourceInfo(dsv, depth_size, unused_format);
      if (depth_size.x != device_data.output_resolution.x ||
          depth_size.y != device_data.output_resolution.y ||
          !GetPatchedVertexShader(native_device, cmd_list_data, device_data,
             vertex_shader_hash))
         return;
      gd.mv_scene_open = true;
#if DEVELOPMENT
      if (auto* const perf_queries = gd.perf_timestamps.frame)
      {
         perf_queries->Mark(native_device_context, PERF_SCENE_START);
      }
#endif
      gd.mv_depth.reset();
      dsv->GetResource(&gd.mv_depth);
      gd.mv_scene_color.reset();
      gd.mv_scene_rtv.reset();
      gd.jitter_dsv = nullptr;
      gd.jitter_depth_stencil_state = nullptr;
      gd.jitter_depth_test = true;
      gd.mv_accepted_dsv = nullptr;
      gd.mv_blend_state = nullptr;
      gd.mv_blend_opaque = true;
      gd.mv_reactive_blend = 0;
      // Halton (2, 3) over the upscaler's phase count; pixels to NDC (y up). None until the upscaler is ready (the bridge's
      // helper starting shows the scene as it is, antialiased with SMAA). SR stays latched active until the next present after
      // "None" is picked: no implementation then, nor instance data.
      const SR::InstanceData* sr_instance_data = IsSRActive(device_data) ? device_data.GetSRInstanceData() : nullptr;
      if (sr_instance_data && !sr_implementations[device_data.sr_type]->IsReady(sr_instance_data))
         sr_instance_data = nullptr;
      const unsigned int phase = cb_luma_global_settings.FrameIndex % (sr_instance_data ? (std::max)(sr_implementations[device_data.sr_type]->GetJitterPhases(sr_instance_data), 1) : SR::GetDefaultJitterPhases());
      gd.mv_jitter = (sr_instance_data || g_mv_force_jitter) && !g_mv_disable_jitter && GetPerfMotionVectorDraws() != 0
                        ? std::array<float, 2>{SR::HaltonSequence(phase, 2),
                             SR::HaltonSequence(phase, 3)}
                        : std::array<float, 2>{};
      gd.mv_jitter_ndc = {gd.mv_jitter[0] * 2.f / device_data.output_resolution.x,
         gd.mv_jitter[1] * -2.f / device_data.output_resolution.y};
      const float ndc_jitter[4] = {gd.mv_jitter_ndc[0], gd.mv_jitter_ndc[1], 0.f,
         0.f};
      if (!PatchedDraws::WriteDynamicConstants(native_device, native_device_context,
             std::addressof(gd.mv_jitter_buffer), ndc_jitter,
             sizeof(ndc_jitter)))
      {
         // No stale jitter on the scene draws either: no motion vectors this frame
         gd.mv_jitter = {};
         gd.mv_jitter_ndc = {};
         gd.mv_jitter_buffer.reset();
      }
   }

#if DEVELOPMENT
#define MV_REJECT(reason) \
   ([&](auto& gd) { gd.mv_stats.rejected[reason]++; gd.mv_draw_reject = int(reason); return false; }(GetGameDeviceData(device_data)))
#else
#define MV_REJECT(reason) false
#endif

   // Draws an opaque draw into the fp16 scene (the scene target alone, output sized, with the scene depth) with the patched shaders,
   // adding the motion vector target ("target_slot", past the game's) and the previous frame's vc4 ("previous_slots"). False if it
   // can't (the draw then goes to "DrawWithJitter").
   static bool DrawWithMotionVectors(
      ID3D11Device* native_device, ID3D11DeviceContext* native_device_context,
      CommandListData& cmd_list_data, DeviceData& device_data,
      const ShaderHashesList<OneShaderPerPipeline>& original_shader_hashes,
      const std::function<void()>& draw,
      const com_ptr<ID3D11RenderTargetView> (
         &rtvs)[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT],
      ID3D11DepthStencilView* dsv)
   {
      auto& gd = GetGameDeviceData(device_data);
      // The scene target alone, plus the motion vector and mask targets the last patched draws left bound
      for (UINT slot = 1; slot < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; slot++)
      {
         if (rtvs[slot] &&
             (slot != MotionVectorPatches::target_slot || rtvs[slot] != gd.mv_rtv) &&
             (slot != MotionVectorPatches::reactive_slot || rtvs[slot] != gd.mv_reactive_target_rtv))
            return MV_REJECT(0);
      }
      if (!rtvs[0] || !dsv || !gd.mv_scene_open)
         return MV_REJECT(1);
      ClassifyBoundBlend(native_device_context, &gd);
      if (!gd.mv_blend_opaque)
         return MV_REJECT(6);
      // Targets other than the last accepted ones: checked, and the motion vector target sized for them
      if (rtvs[0] != gd.mv_scene_rtv || dsv != gd.mv_accepted_dsv)
      {
         com_ptr<ID3D11Resource> depth;
         dsv->GetResource(&depth);
         com_ptr<ID3D11Resource> color;
         rtvs[0]->GetResource(&color);
         if (depth != gd.mv_depth || !color ||
             (gd.mv_scene_color && color != gd.mv_scene_color))
            return MV_REJECT(2);
         D3D11_RENDER_TARGET_VIEW_DESC rtv_desc;
         rtvs[0]->GetDesc(&rtv_desc);
         com_ptr<ID3D11Texture2D> color_texture;
         D3D11_TEXTURE2D_DESC color_desc = {};
         if (SUCCEEDED(color->QueryInterface(&color_texture)))
            color_texture->GetDesc(&color_desc);
#if DEVELOPMENT
         gd.mv_stats.rejected_format = rtv_desc.Format;
         gd.mv_stats.rejected_dimension = rtv_desc.ViewDimension;
         gd.mv_stats.rejected_width = color_desc.Width;
         gd.mv_stats.rejected_height = color_desc.Height;
#endif
         // The fill reads the scene: it needs a shader resource view. Filtered by the resource, not the view (dgVoodoo binds
         // single-slice array views).
         if (rtv_desc.Format != DXGI_FORMAT_R16G16B16A16_FLOAT ||
             color_desc.ArraySize != 1 ||
             color_desc.SampleDesc.Count != 1 ||
             (color_desc.BindFlags & D3D11_BIND_SHADER_RESOURCE) == 0)
            return MV_REJECT(3);
         const uint2 size = {color_desc.Width, color_desc.Height};
         if (size.x != device_data.output_resolution.x ||
             size.y != device_data.output_resolution.y)
            return MV_REJECT(4);
         const std::unique_lock lock(gd.mv_mutex);
         D3D11_TEXTURE2D_DESC desc = {};
         if (gd.mv_texture)
            gd.mv_texture->GetDesc(&desc);
         // R16G16_FLOAT: FSR keeps 16 bits internally; the error is under 0.1% of the motion (BL GOTY)
         constexpr DXGI_FORMAT format = DXGI_FORMAT_R16G16_FLOAT;
         if (desc.Width != size.x || desc.Height != size.y)
         {
            gd.mv_texture.reset();
            gd.mv_rtv.reset();
            gd.mv_uav.reset();
            gd.mv_device_depth.reset();
            gd.mv_device_depth_uav.reset();
            // The fill reads the target back through its UAV
            D3D11_FEATURE_DATA_FORMAT_SUPPORT2 support = {format};
            const bool typed_uav_load =
               SUCCEEDED(native_device->CheckFeatureSupport(
                  D3D11_FEATURE_FORMAT_SUPPORT2, &support, sizeof(support))) &&
               (support.OutFormatSupport2 & D3D11_FORMAT_SUPPORT2_UAV_TYPED_LOAD) !=
                  0;
            desc = CD3D11_TEXTURE2D_DESC(format, size.x, size.y, 1, 1,
               D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | (typed_uav_load ? D3D11_BIND_UNORDERED_ACCESS : 0u));
            if (FAILED(
                   SRBridge::CreateSharableTexture(native_device, desc, &gd.mv_texture)) ||
                FAILED(native_device->CreateRenderTargetView(gd.mv_texture.get(),
                   nullptr, &gd.mv_rtv)))
            {
               gd.mv_texture.reset();
               gd.mv_rtv.reset();
               return MV_REJECT(5);
            }
            if (typed_uav_load)
               native_device->CreateUnorderedAccessView(gd.mv_texture.get(), nullptr,
                  &gd.mv_uav);
            const CD3D11_TEXTURE2D_DESC depth_desc(DXGI_FORMAT_R32_FLOAT, size.x, size.y, 1, 1, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS);
            if (SUCCEEDED(SRBridge::CreateSharableTexture(native_device, depth_desc,
                   &gd.mv_device_depth)))
               native_device->CreateUnorderedAccessView(
                  gd.mv_device_depth.get(), nullptr, &gd.mv_device_depth_uav);
            gd.mv_reactive_target.reset();
            gd.mv_reactive_target_rtv.reset();
            gd.mv_reactive_target_srv.reset();
            const CD3D11_TEXTURE2D_DESC reactive_desc(DXGI_FORMAT_R8G8_UNORM, size.x, size.y, 1, 1, D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE);
            if (SUCCEEDED(native_device->CreateTexture2D(&reactive_desc, nullptr, &gd.mv_reactive_target)) &&
                SUCCEEDED(native_device->CreateRenderTargetView(gd.mv_reactive_target.get(), nullptr, &gd.mv_reactive_target_rtv)))
               native_device->CreateShaderResourceView(gd.mv_reactive_target.get(), nullptr, &gd.mv_reactive_target_srv);
            gd.mv_frame_ended = true;
         }
         gd.mv_scene_color = color;
         gd.mv_scene_rtv = rtvs[0];
         gd.mv_accepted_dsv = dsv;
      }

      ID3D11VertexShader* const vertex_shader =
         GetPatchedVertexShader(native_device, cmd_list_data, device_data,
            original_shader_hashes.vertex_shaders[0]);
      if (const uint32_t pixel_shader_hash =
             original_shader_hashes.pixel_shaders[0];
         pixel_shader_hash != gd.mv_last_pixel_shader_hash)
      {
         gd.mv_last_pixel_shader =
            GetMotionVectorShader(
               native_device, device_data, &gd.mv_pixel_shaders, pixel_shader_hash,
               cmd_list_data.pipeline_state_original_pixel_shader)
               .shader.get();
         gd.mv_last_pixel_shader_hash = pixel_shader_hash;
      }
      ID3D11PixelShader* const pixel_shader = gd.mv_last_pixel_shader;
      if (!vertex_shader || !pixel_shader || !gd.mv_jitter_buffer)
         return MV_REJECT(7);
      if (std::exchange(gd.mv_frame_ended, false))
      {
         gd.mv_fill_pending =
            gd.mv_uav && gd.mv_device_depth_uav &&
            FindShader(device_data.native_compute_shaders,
               CompileTimeStringHash("ME1 Motion Vector Fill CS")) !=
               nullptr;
         // The fill's marker: the largest float16 (a larger clear value is stored as it in R16G16_FLOAT)
         const FLOAT clear_value = gd.mv_fill_pending ? 65504.f : 0.f;
         const FLOAT clear[4] = {clear_value, clear_value, 0.f, 0.f};
         native_device_context->ClearRenderTargetView(gd.mv_rtv.get(), clear);
         if (gd.mv_reactive_target_rtv)
         {
            const FLOAT zero[4] = {};
            native_device_context->ClearRenderTargetView(gd.mv_reactive_target_rtv.get(), zero);
         }
         // Last frame's camera and objects are the previous ones, unless frames without a scene (menus, videos) came between
         const bool previous_valid =
            gd.mv_camera &&
            cb_luma_global_settings.FrameIndex - gd.mv_frame_index <= 1;
         gd.mv_previous_camera = previous_valid ? gd.mv_camera : nullptr;
         gd.mv_camera = nullptr;
         gd.mv_frame_index = cb_luma_global_settings.FrameIndex;
         // Swapped, not rebuilt: the lists keep their nodes and capacity (an empty list matches nothing); keys drawn in neither
         // of the last two frames go
         gd.mv_previous_objects.swap(gd.mv_objects);
#if DEVELOPMENT
         gd.mv_stats.tiebreak_collisions = PatchedDraws::CountTieBreakCollisions(gd.mv_previous_objects, [](const auto& a, const auto& b)
            { return PatchedDraws::SameBytes(a.constants, b.constants); });
#endif
         std::erase_if(gd.mv_objects,
            [](const auto& entry)
            { return entry.second.empty(); });
         for (auto& entry : gd.mv_objects)
            entry.second.clear();
         if (!previous_valid)
            gd.mv_previous_objects.clear();
      }

      // The game's vc4 (object, camera and bones in one). The slots added past it stay bound after the draw: no translated shader
      // reads a constant buffer past b4.
      com_ptr<ID3D11Buffer> current;
      native_device_context->VSGetConstantBuffers(MotionVectorPatches::object_slot,
         1, &current);
#if DEVELOPMENT
      // Diagnostics, off while a "Performance Test" runs (they cost CPU on every draw that Publishing doesn't pay)
      if (Perf::g_test == 0 && gd.mv_previous_constants.ring_context)
      {
         com_ptr<ID3D11Buffer> bound;
         UINT first_constant = 0, constant_count = 0;
         gd.mv_previous_constants.ring_context->VSGetConstantBuffers1(MotionVectorPatches::object_slot,
            1, &bound, &first_constant,
            &constant_count);
         gd.mv_stats.offset_bindings += first_constant != 0;
      }
#endif
      MassEffectGameDeviceData::ConstantsCopy constants;
      {
         const std::lock_guard lock(gd.mv_constants_mutex);
         // The buffer's CPU copy (null until its first upload); the lookup registers it for a copy at every upload
         if (current)
         {
            const auto [copy, registered] = gd.mv_constants_copies.try_emplace(reinterpret_cast<uint64_t>(current.get()));
            constants = copy->second;
            if (registered)
               AddFilteredBuffer(gd, current.get());
         }
      }
      // The previous frame's vc4: the same object's from last frame, else this draw's with last frame's camera (no object motion).
      // None (no CPU copy yet, another camera): the current one (zero motion).
      const std::vector<uint8_t>* upload = nullptr;
      if (constants && constants->size() >= kTranslationOffset + 16)
      {
         // The frame's camera: its first motion vector draw's
         if (!gd.mv_camera)
            gd.mv_camera = constants;
         const bool frame_camera =
            std::memcmp(constants->data() + kViewProjectionOffset,
               gd.mv_camera->data() + kViewProjectionOffset,
               kCameraSize) == 0;

         // Draw key: same mesh, same shaders, no instance count
         com_ptr<ID3D11Buffer> vertex_buffer;
         UINT vertex_stride = 0, vertex_offset = 0;
         native_device_context->IAGetVertexBuffers(0, 1, &vertex_buffer,
            &vertex_stride, &vertex_offset);
         com_ptr<ID3D11Buffer> index_buffer;
         DXGI_FORMAT index_format;
         UINT index_offset = 0;
         native_device_context->IAGetIndexBuffer(&index_buffer, &index_format,
            &index_offset);
         const DrawDispatchData& draw_data = last_draw_dispatch_data;
         uint64_t key = 0;
         for (const uint64_t value :
            {uint64_t(original_shader_hashes.vertex_shaders[0]),
               uint64_t(original_shader_hashes.pixel_shaders[0]),
               reinterpret_cast<uint64_t>(vertex_buffer.get()),
               uint64_t(vertex_offset),
               reinterpret_cast<uint64_t>(index_buffer.get()),
               uint64_t(index_offset), uint64_t(draw_data.index_count),
               uint64_t(draw_data.first_index),
               uint64_t(uint32_t(draw_data.vertex_offset)),
               uint64_t(draw_data.vertex_count), uint64_t(draw_data.first_vertex)})
            HashCombine(key, value);
         // LocalToWorld (c5-c8) separates objects that share a key (props), as a tie-break only (see "PatchedDraws::ObjectTransform").
         // Skinned meshes' c8 is a bone row, the same for copies of a model (holstered weapons), which matched each other's history.
         const size_t translation_offset = constants->size() >= gd.mv_last_vertex_translation_offset + 16 ? gd.mv_last_vertex_translation_offset : kTranslationOffset;
         const PatchedDraws::ObjectTransform transform = PatchedDraws::ReadRowVectorTransform(constants->data() + translation_offset - 3 * 16);

         // ponytail: linear search among the key's candidates (a handful at most); a spatial lookup if big crowds share a mesh
         const MassEffectGameDeviceData::MotionVectorObject* match = nullptr;
         if (const auto previous = gd.mv_previous_objects.find(key);
            previous != gd.mv_previous_objects.end())
         {
            match = PatchedDraws::FindNearest(previous->second, transform, [&](const auto& candidate)
               { return candidate.constants->size() == constants->size(); })
                       .first;
         }
         if (match)
         {
            // Last frame's list outlives the draw ("mv_previous_objects" only changes at the next frame start)
            upload = &*match->constants;
#if DEVELOPMENT
            gd.mv_stats.matched++;
#endif
         }
         else if (frame_camera && gd.mv_previous_camera)
         {
            // Not found, drawn with the frame's camera: its own constants with last frame's camera (camera motion only), only
            // what the shader reads (at least the camera)
            const size_t copy_size = gd.mv_last_vertex_read_size != 0 ? std::clamp<size_t>(gd.mv_last_vertex_read_size, kViewProjectionOffset + kCameraSize, constants->size())
                                                                      : constants->size();
            gd.mv_camera_only_copy.assign(constants->begin(), constants->begin() + copy_size);
            std::memcpy(gd.mv_camera_only_copy.data() + kViewProjectionOffset,
               gd.mv_previous_camera->data() + kViewProjectionOffset,
               kCameraSize);
            upload = &gd.mv_camera_only_copy;
#if DEVELOPMENT
            gd.mv_stats.camera_only++;
#endif
         }
#if DEVELOPMENT
         else
         {
            gd.mv_stats.other_camera++;
         }
#endif
         // Kept as drawn for the next frame
         gd.mv_objects[key].push_back({transform, constants});
      }
#if DEVELOPMENT
      else
      {
         gd.mv_stats.uncopied++;
      }
#endif
      // "Performance Test" modes without motion vector draws: the draw goes to "DrawWithJitter" (jittered or, without jitter, untouched)
      if (GetPerfMotionVectorDraws() < 2)
         return false;
      ID3D11Buffer* const previous_current[] = {current.get()};
      gd.mv_previous_constants.Bind(native_device, native_device_context, MotionVectorPatches::previous_slots, {&upload, 1}, previous_current, "ME1", {&gd.mv_last_vertex_read_size, 1});
      ID3D11Buffer* const jitter = gd.mv_jitter_buffer.get();
      native_device_context->VSSetConstantBuffers(MotionVectorPatches::jitter_slot,
         1, &jitter);
      // Left bound after the draw (set directly, bypassing Core's state tracking): the game's next draws either bind their own
      // targets and shaders, or are motion vector draws too. No dumped pixel shader writes past o0, so the target keeps its contents.
      if (rtvs[MotionVectorPatches::target_slot] != gd.mv_rtv)
      {
         ID3D11RenderTargetView* targets[MotionVectorPatches::target_slot + 1] = {
            rtvs[0].get()};
         targets[MotionVectorPatches::target_slot] = gd.mv_rtv.get();
         native_device_context->OMSetRenderTargets(
            MotionVectorPatches::target_slot + 1, targets, dsv);
      }
      PatchedDraws::BindPatchedShader(native_device_context, vertex_shader,
         &gd.mv_bound_vertex_shader);
      PatchedDraws::BindPatchedShader(native_device_context, pixel_shader,
         &gd.mv_bound_pixel_shader);

      draw();
#if DEVELOPMENT
      gd.mv_stats.motion_vector_draws++;
#endif
      return true;
   }

   // Jitter for the scene's mesh draws without motion vectors (patched vertex shader, game pixel shader): depth passes, lights,
   // decals, translucents. Every draw depth tested against the scene takes the same jitter, or jittered and unjittered depths of
   // the same surface fail each other's test. False if it can't (the draw runs untouched).
   static bool DrawWithJitter(
      ID3D11Device* native_device, ID3D11DeviceContext* native_device_context,
      CommandListData& cmd_list_data, DeviceData& device_data,
      const ShaderHashesList<OneShaderPerPipeline>& original_shader_hashes,
      const std::function<void()>& draw,
      const com_ptr<ID3D11RenderTargetView> (
         &rtvs)[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT],
      ID3D11DepthStencilView* dsv)
   {
      auto& gd = GetGameDeviceData(device_data);
      if (!gd.mv_scene_open || gd.mv_jitter == std::array<float, 2>{} ||
          !gd.mv_jitter_buffer)
         return false;
      // Meshes only (full screen passes have no vertex buffer or no depth test), into the scene depth (not shadows)
      if (!dsv)
         return false;
      if (dsv != gd.jitter_dsv)
      {
         com_ptr<ID3D11Resource> depth;
         dsv->GetResource(&depth);
         gd.jitter_dsv = dsv;
         gd.jitter_dsv_scene = depth == gd.mv_depth;
      }
      if (!gd.jitter_dsv_scene)
         return false;
      com_ptr<ID3D11DepthStencilState> depth_stencil_state;
      native_device_context->OMGetDepthStencilState(&depth_stencil_state, nullptr);
      if (depth_stencil_state.get() != gd.jitter_depth_stencil_state)
      {
         D3D11_DEPTH_STENCIL_DESC depth_desc =
            CD3D11_DEPTH_STENCIL_DESC(D3D11_DEFAULT);
         if (depth_stencil_state)
            depth_stencil_state->GetDesc(&depth_desc);
         gd.jitter_depth_test = depth_desc.DepthEnable;
         gd.jitter_depth_stencil_state = depth_stencil_state.get();
      }
      if (!gd.jitter_depth_test)
         return false;
      com_ptr<ID3D11Buffer> vertex_buffer;
      UINT vertex_stride, vertex_offset;
      native_device_context->IAGetVertexBuffers(0, 1, &vertex_buffer,
         &vertex_stride, &vertex_offset);
      if (!vertex_buffer)
         return false;
      ID3D11VertexShader* const vertex_shader =
         GetPatchedVertexShader(native_device, cmd_list_data, device_data,
            original_shader_hashes.vertex_shaders[0]);
      if (!vertex_shader)
         return false;

      // An alpha blended draw into the scene writes its mask, reactive or transparency & composition (its pixel shader patched,
      // the mask target added past the motion vector one)
      ID3D11PixelShader* reactive_shader = nullptr;
      if (g_sr_reactive_enable && IsSRActive(device_data) && device_data.sr_type == SR::Type::FSR &&
          gd.mv_reactive_target_rtv && rtvs[0] && rtvs[0] == gd.mv_scene_rtv)
      {
         ClassifyBoundBlend(native_device_context, &gd);
         if (const uint8_t blend = gd.mv_reactive_blend; blend != 0)
            reactive_shader = GetMotionVectorShader(native_device, device_data, &gd.mv_reactive_pixel_shaders[blend - 1],
               original_shader_hashes.pixel_shaders[0], cmd_list_data.pipeline_state_original_pixel_shader, blend)
                                 .shader.get();
      }

      // The patched vertex shader and the jitter stay bound after the draw (see "DrawWithMotionVectors"), with the game's pixel
      // shader (a motion vector draw's is put back) or its reactive version, and the mask target
      PatchedDraws::BindPatchedShader(native_device_context, vertex_shader,
         &gd.mv_bound_vertex_shader);
      ID3D11Buffer* const jitter = gd.mv_jitter_buffer.get();
      native_device_context->VSSetConstantBuffers(MotionVectorPatches::jitter_slot,
         1, &jitter);
      if (reactive_shader)
      {
         if (rtvs[MotionVectorPatches::reactive_slot] != gd.mv_reactive_target_rtv)
         {
            ID3D11RenderTargetView* targets[MotionVectorPatches::reactive_slot + 1] = {};
            for (uint32_t slot = 0; slot < MotionVectorPatches::reactive_slot; slot++)
               targets[slot] = rtvs[slot].get();
            targets[MotionVectorPatches::reactive_slot] = gd.mv_reactive_target_rtv.get();
            native_device_context->OMSetRenderTargets(MotionVectorPatches::reactive_slot + 1, targets, dsv);
         }
         PatchedDraws::BindPatchedShader(native_device_context, reactive_shader, &gd.mv_bound_pixel_shader);
#if DEVELOPMENT
         gd.mv_stats.reactive_draws++;
#endif
      }
      else
      {
         PatchedDraws::RestoreGameShader(native_device_context, &gd.mv_bound_pixel_shader);
      }
      draw();
#if DEVELOPMENT
      gd.mv_stats.jitter_draws++;
#endif
      return true;
   }

   // DLAA or FSR 3 Native AA on the jittered scene, its depth (from its alpha, see the fill) and the motion vectors; the result goes
   // back into the scene's color (or its copy, see "mv_scene_copy"), its alpha (the post passes' linear depth) kept. False if it
   // didn't draw (missing input, or the upscaler failed). "reactive_mask": the fill wrote this scene's mask.
   static bool DrawUpscaler(ID3D11Device* native_device,
      ID3D11DeviceContext* native_device_context,
      DeviceData& device_data, bool reactive_mask)
   {
      auto& gd = GetGameDeviceData(device_data);
      auto* const copy_vs = FindShader(device_data.native_vertex_shaders,
         CompileTimeStringHash("Copy VS"));
      auto* const copy_ps = FindShader(device_data.native_pixel_shaders,
         CompileTimeStringHash("Copy PS"));
      if (!gd.mv_texture || !gd.mv_camera ||
          !gd.mv_device_depth || !gd.mv_scene_color || !gd.mv_scene_rtv ||
          !copy_vs || !copy_ps)
         return false;
      com_ptr<ID3D11Texture2D> scene;
      if (FAILED(gd.mv_scene_color->QueryInterface(&scene)))
         return false;
      D3D11_TEXTURE2D_DESC scene_desc;
      scene->GetDesc(&scene_desc);
      SR::InstanceData* const sr_instance_data = device_data.GetSRInstanceData();
      if (!sr_instance_data)
         return false;

      D3D11_TEXTURE2D_DESC output_desc = {};
      if (device_data.sr_output_color)
         device_data.sr_output_color->GetDesc(&output_desc);
      if (output_desc.Width != scene_desc.Width ||
          output_desc.Height != scene_desc.Height)
      {
         device_data.sr_output_color.reset();
         gd.sr_output_srv.reset();
         output_desc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R16G16B16A16_FLOAT, scene_desc.Width, scene_desc.Height, 1, 1, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS);
         if (SUCCEEDED(SRBridge::CreateSharableTexture(native_device, output_desc,
                &device_data.sr_output_color)))
            native_device->CreateShaderResourceView(device_data.sr_output_color.get(),
               nullptr, &gd.sr_output_srv);
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
         blend_desc.RenderTarget[0].RenderTargetWriteMask =
            D3D11_COLOR_WRITE_ENABLE_RED | D3D11_COLOR_WRITE_ENABLE_GREEN |
            D3D11_COLOR_WRITE_ENABLE_BLUE;
         native_device->CreateBlendState(&blend_desc, &gd.sr_rgb_blend_state);
         if (!gd.sr_rgb_blend_state)
            return false;
      }

      // FSR needs the camera (vc4's view projection multiplies row vectors)
      const SR::ViewProjectionCamera camera = SR::GetViewProjectionCamera(reinterpret_cast<const float*>(gd.mv_camera->data() + kViewProjectionOffset), true);

      SR::SettingsData settings_data;
      settings_data.output_width = scene_desc.Width;
      settings_data.output_height = scene_desc.Height;
      settings_data.render_width = scene_desc.Width;
      settings_data.render_height = scene_desc.Height;
      settings_data.hdr = true;
      // The motion vectors are UV deltas, previous minus current
      settings_data.mvs_x_scale = float(scene_desc.Width);
      settings_data.mvs_y_scale = float(scene_desc.Height);
      settings_data.auto_exposure = false; // FSR's clips highlights (FSR-Best-Practices FIN-3); the scene is already exposed
      settings_data.render_preset = dlss_render_preset;
      sr_implementations[device_data.sr_type]->UpdateSettings(sr_instance_data, native_device_context, settings_data);

      SR::SuperResolutionImpl::DrawData draw_data;
      draw_data.source_color = scene.get();
      draw_data.output_color = device_data.sr_output_color.get();
      draw_data.motion_vectors = gd.mv_texture.get();
      draw_data.depth_buffer = gd.mv_device_depth.get();
      draw_data.bias_mask = reactive_mask && g_sr_reactive_pass ? gd.mv_reactive.get() : nullptr;
      draw_data.transparency_alpha = reactive_mask && g_sr_reactive_pass ? gd.mv_transparency.get() : nullptr;
#if DEVELOPMENT
      if (g_sr_tc_from_mask)
         draw_data.transparency_alpha = draw_data.bias_mask;
#endif
      // As applied (pixels, +y down)
      draw_data.jitter_x = gd.mv_jitter[0];
      draw_data.jitter_y = gd.mv_jitter[1];
      draw_data.reset = device_data.force_reset_sr;
      // FSR requires a FOV (it errors on 0): a camera without an up axis keeps the last one
      if (camera.vert_fov > 0.0)
         gd.sr_vert_fov = float(camera.vert_fov);
      draw_data.vert_fov = gd.sr_vert_fov;
      // A finite far (depth 1) even when the projection has none (FSR's context is FFX_FSR3_ENABLE_DEPTH_INFINITE only with inverted
      // depth)
      if (camera.near_plane > 0.0)
      {
         draw_data.near_plane = float(camera.near_plane);
         draw_data.far_plane = float(camera.far_plane);
      }
#if DEVELOPMENT
      gd.mv_stats.near_plane = draw_data.near_plane;
      gd.mv_stats.far_plane = draw_data.far_plane;
      if (g_bridge_flush_before)
         native_device_context->Flush();
#endif
      if (!sr_implementations[device_data.sr_type]->Draw(sr_instance_data, native_device_context, draw_data))
      {
         // Back to SMAA until the upscaler is picked again
         device_data.sr_suppressed = true;
         return false;
      }
#if DEVELOPMENT
      auto* const perf_queries = gd.perf_timestamps.frame;
      const bool perf_end_parts = perf_queries && perf_queries->Marked(PERF_SCENE_END) && !perf_queries->Marked(PERF_SCENE_TAIL_END);
      if (perf_end_parts)
      {
         perf_queries->Mark(native_device_context, PERF_UPSCALER_END);
      }
#endif
      // The copy's alpha is the game's resolve of the same scene (the RGB write mask keeps its linear depth)
      bool copy_only = false;
      if (gd.mv_scene_copy && !gd.mv_scene_read_by_end)
      {
         com_ptr<ID3D11Resource> copy_rtv_resource;
         if (gd.mv_scene_copy_rtv)
            gd.mv_scene_copy_rtv->GetResource(&copy_rtv_resource);
         if (copy_rtv_resource != gd.mv_scene_copy)
         {
            gd.mv_scene_copy_rtv.reset();
            com_ptr<ID3D11Texture2D> copy;
            D3D11_TEXTURE2D_DESC copy_desc;
            D3D11_RENDER_TARGET_VIEW_DESC rtv_desc;
            if (SUCCEEDED(gd.mv_scene_copy->QueryInterface(&copy)))
            {
               copy->GetDesc(&copy_desc);
               gd.mv_scene_rtv->GetDesc(&rtv_desc);
               if (copy_desc.BindFlags & D3D11_BIND_RENDER_TARGET)
                  native_device->CreateRenderTargetView(copy.get(), &rtv_desc, &gd.mv_scene_copy_rtv);
            }
         }
         copy_only = gd.mv_scene_copy_rtv != nullptr;
      }
      DrawCustomPixelShader(native_device_context,
         device_data.default_depth_stencil_state.get(),
         gd.sr_rgb_blend_state.get(), nullptr, copy_vs, copy_ps,
         gd.sr_output_srv.get(), copy_only ? gd.mv_scene_copy_rtv.get() : gd.mv_scene_rtv.get(),
         scene_desc.Width, scene_desc.Height);
#if DEVELOPMENT
      if (perf_end_parts)
      {
         perf_queries->Mark(native_device_context, PERF_COPY_END);
      }
#endif
      if (gd.mv_scene_copy && !copy_only)
         native_device_context->CopyResource(gd.mv_scene_copy.get(), scene.get());
      // Not while the bridge's helper starts (the color copied as it is): SMAA stays on and the next frame resets
      device_data.has_drawn_sr = sr_implementations[device_data.sr_type]->IsReady(sr_instance_data);
      return true;
   }

   // Ends the scene at its first post pass: the depth and camera motion fill (see "Luma_ME1_MotionVectorFill.hlsl"), then the
   // upscaler, both before that pass reads the scene
   static void EndScene(ID3D11Device* native_device,
      ID3D11DeviceContext* native_device_context,
      DeviceData& device_data)
   {
      auto& gd = GetGameDeviceData(device_data);
#if DEVELOPMENT
      auto* const perf_queries = gd.perf_timestamps.frame;
      if (perf_queries && perf_queries->Marked(PERF_SCENE_START))
      {
         perf_queries->Mark(native_device_context, PERF_SCENE_END);
      }
#endif
      gd.mv_scene_open = false;
      gd.mv_scene_done = true;
#if DEVELOPMENT
      gd.mv_scene_overwritten = false;
#endif
      DrawStateStack<DrawStateStackType::FullGraphics> graphics_state;
      DrawStateStack<DrawStateStackType::Compute> compute_state;
      graphics_state.Cache(native_device_context, device_data.uav_max_count);
      compute_state.Cache(native_device_context, device_data.uav_max_count);
      auto* const fill_shader =
         FindShader(device_data.native_compute_shaders,
            CompileTimeStringHash("ME1 Motion Vector Fill CS"));
      bool filled = false;
      if (gd.mv_scene_srv)
      {
         com_ptr<ID3D11Resource> srv_resource;
         gd.mv_scene_srv->GetResource(&srv_resource);
         if (srv_resource != gd.mv_scene_color)
            gd.mv_scene_srv.reset();
      }
      // The reactive and transparency & composition masks, written by the fill from what the alpha blended draws wrote. FSR only:
      // DLSS's current presets ignore them (DLSS-Best-Practices TRN-2)
      const bool reactive = IsSRActive(device_data) && device_data.sr_type == SR::Type::FSR && g_sr_reactive_enable && gd.mv_reactive_target_srv && !g_sr_reactive_skip_fill;
      if (reactive)
      {
         D3D11_TEXTURE2D_DESC desc = {};
         if (gd.mv_reactive)
            gd.mv_reactive->GetDesc(&desc);
         if (desc.Width != uint32_t(device_data.output_resolution.x) || desc.Height != uint32_t(device_data.output_resolution.y))
         {
            gd.mv_reactive.reset();
            gd.mv_reactive_uav.reset();
            gd.mv_transparency.reset();
            gd.mv_transparency_uav.reset();
            desc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R8_UNORM, uint32_t(device_data.output_resolution.x), uint32_t(device_data.output_resolution.y), 1, 1, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS);
            if (SUCCEEDED(SRBridge::CreateSharableTexture(native_device, desc, &gd.mv_reactive)))
               native_device->CreateUnorderedAccessView(gd.mv_reactive.get(), nullptr, &gd.mv_reactive_uav);
            if (SUCCEEDED(SRBridge::CreateSharableTexture(native_device, desc, &gd.mv_transparency)))
               native_device->CreateUnorderedAccessView(gd.mv_transparency.get(), nullptr, &gd.mv_transparency_uav);
         }
      }
      const bool write_reactive = reactive && gd.mv_reactive_uav && gd.mv_transparency_uav;
      if (std::exchange(gd.mv_fill_pending, false) && fill_shader && gd.mv_camera &&
          gd.mv_scene_color &&
          (gd.mv_scene_srv || SUCCEEDED(native_device->CreateShaderResourceView(gd.mv_scene_color.get(), nullptr, &gd.mv_scene_srv))))
      {
         // Current clip space to the previous frame's: previous * inverse(current) for column vectors (vc4 holds the row vector
         // matrix, transposed here), in double (absolute world translation)
         const float* const view_projection = reinterpret_cast<const float*>(
            gd.mv_camera->data() + kViewProjectionOffset);
         Math::Matrix44D current, previous;
         current.SetIdentity();
         previous.SetIdentity();
         if (gd.mv_previous_camera)
         {
            std::copy_n(view_projection, 16, current.GetData());
            std::copy_n(reinterpret_cast<const float*>(
                           gd.mv_previous_camera->data() + kViewProjectionOffset),
               16, previous.GetData());
            current.Transpose();
            previous.Transpose();
            current.Invert();
         }
         const Math::Matrix44D reprojection = previous * current;
         const SR::ViewProjectionCamera camera = SR::GetViewProjectionCamera(view_projection, true);
         float constants[24] = {}; // A multiple of 16 bytes
         for (int i = 0; i < 16; i++)
            constants[i] = float(reprojection.GetData()[i]);
         constants[16] = gd.mv_jitter_ndc[0];
         constants[17] = gd.mv_jitter_ndc[1];
         constants[18] = float(camera.depth_a);
         constants[19] = float(camera.depth_b);
         constants[20] = g_sr_reactive_scale;
         constants[21] = g_sr_reactive_threshold;
         constants[22] = write_reactive ? 1.f : 0.f;
         if (PatchedDraws::WriteDynamicConstants(native_device, native_device_context,
                std::addressof(gd.mv_fill_buffer), constants,
                sizeof(constants)))
         {
            // The scene and the motion vectors may be bound as render targets
            native_device_context->OMSetRenderTargets(0, nullptr, nullptr);
            ID3D11Buffer* const buffer = gd.mv_fill_buffer.get();
            ID3D11ShaderResourceView* const srvs[2] = {gd.mv_scene_srv.get(), write_reactive ? gd.mv_reactive_target_srv.get() : nullptr};
            ID3D11UnorderedAccessView* const uavs[4] = {gd.mv_uav.get(), gd.mv_device_depth_uav.get(), write_reactive ? gd.mv_reactive_uav.get() : nullptr, write_reactive ? gd.mv_transparency_uav.get() : nullptr};
            native_device_context->CSSetConstantBuffers(0, 1, &buffer);
            native_device_context->CSSetShaderResources(0, 2, srvs);
            native_device_context->CSSetUnorderedAccessViews(0, UINT(std::size(uavs)), uavs, nullptr);
            native_device_context->CSSetShader(fill_shader, nullptr, 0);
            native_device_context->Dispatch(
               (uint32_t(device_data.output_resolution.x) + 7) / 8,
               (uint32_t(device_data.output_resolution.y) + 7) / 8, 1);
            ID3D11UnorderedAccessView* const null_uavs[std::size(uavs)] = {};
            ID3D11ShaderResourceView* const null_srvs[2] = {};
            native_device_context->CSSetUnorderedAccessViews(0, UINT(std::size(null_uavs)), null_uavs, nullptr);
            native_device_context->CSSetShaderResources(0, 2, null_srvs);
            filled = true;
#if DEVELOPMENT
            if (perf_queries && perf_queries->Marked(PERF_SCENE_END) && !perf_queries->Marked(PERF_SCENE_TAIL_END))
            {
               perf_queries->Mark(native_device_context, PERF_FILL_END);
            }
#endif
#if DEVELOPMENT
            if (write_reactive && g_sr_reactive_zero_test)
            {
               const FLOAT zero[4] = {};
               native_device_context->ClearUnorderedAccessViewFloat(gd.mv_reactive_uav.get(), zero);
               native_device_context->ClearUnorderedAccessViewFloat(gd.mv_transparency_uav.get(), zero);
            }
#endif
         }
      }
      // The upscaler's depth comes from the fill
      if (IsSRActive(device_data) && filled)
      {
         [[maybe_unused]] const bool drawn =
            DrawUpscaler(native_device, native_device_context, device_data, write_reactive);
#if DEVELOPMENT
         gd.mv_stats.sr_draws += drawn;
#endif
      }
      compute_state.Restore(native_device_context);
      graphics_state.Restore(native_device_context);
#if DEVELOPMENT
      if (perf_queries && perf_queries->Marked(PERF_SCENE_END))
      {
         perf_queries->Mark(native_device_context, PERF_SCENE_TAIL_END);
      }
#endif
   }

#if DEVELOPMENT
   static void OnInitEffectRuntime(reshade::api::effect_runtime* runtime)
   {
      g_effect_runtime = runtime;
   }

   static void OnDestroyEffectRuntime(reshade::api::effect_runtime* runtime)
   {
      if (g_effect_runtime == runtime)
         g_effect_runtime = nullptr;
   }
#endif

public:
   static void UnregisterEvents()
   {
      reshade::unregister_event<reshade::addon_event::map_buffer_region>(OnMapBufferRegion);
      reshade::unregister_event<reshade::addon_event::unmap_buffer_region>(OnUnmapBufferRegion);
      reshade::unregister_event<reshade::addon_event::update_buffer_region>(OnUpdateBufferRegion);
      reshade::unregister_event<reshade::addon_event::create_pipeline>(PatchedDraws::OnCreateBlendState<MotionVectorPatches::target_slot, MotionVectorPatches::reactive_slot>);
#if DEVELOPMENT
      reshade::unregister_event<reshade::addon_event::init_effect_runtime>(OnInitEffectRuntime);
      reshade::unregister_event<reshade::addon_event::destroy_effect_runtime>(OnDestroyEffectRuntime);
#endif
   }

   void OnInit(bool async) override
   {
#if DEVELOPMENT
      // For the MCP "luma_dev_values" tool
      Mcp::RegisterToggles({{"mv_enable", &g_mv_enable}, {"mv_debug_view", &g_mv_debug_view}, {"mv_force_jitter", &g_mv_force_jitter}, {"mv_disable_jitter", &g_mv_disable_jitter}, {"mv_dump_scene", &g_mv_dump_scene},
         {"sr_reactive_enable", &g_sr_reactive_enable}, {"sr_reactive_debug_view", &g_sr_reactive_debug_view}, {"sr_tc_from_mask", &g_sr_tc_from_mask}, {"sr_reactive_pass", &g_sr_reactive_pass},
         {"sr_reactive_skip_fill", &g_sr_reactive_skip_fill}, { "sr_reactive_zero_test",
            &g_sr_reactive_zero_test }});
      Mcp::RegisterValues({{"sr_reactive_scale", &g_sr_reactive_scale, 0.f, 4.f}, {"sr_reactive_threshold", &g_sr_reactive_threshold, 0.f, 1.f}});
      Mcp::RegisterToggles({{"hide_ui", &g_hide_ui}, { "dump_pass_cb",
                               &g_dump_pass_cb }});
      Mcp::RegisterInts({{ "perf_test",
         &Perf::g_test,
         0,
         int(std::size(perf_test_modes)) - 1,
         [](DeviceData& device_data, double value)
         {
            g_perf_sweep.Stop(); // As the "Performance Test" combo
            ApplyPerfTestMode(device_data, int(value));
            return std::string();
         } }});
      Mcp::RegisterMirroredToggle("luma_bloom_enable", &g_luma_bloom_enable, &cb_luma_global_settings.GameSettings.LumaBloomEnable);
      Mcp::RegisterTextures({MCP_GAME_TEXTURE("mv.velocity", mv_texture),
         MCP_GAME_TEXTURE("mv.depth", mv_device_depth),
         MCP_GAME_TEXTURE("sr.reactive", mv_reactive),
         MCP_GAME_TEXTURE("sr.transparency", mv_transparency),
         MCP_GAME_TEXTURE("sr.draws_mask", mv_reactive_target)});
#if ENABLE_SMAA
      Mcp::RegisterToggles({{"smaa_enable", &g_smaa_enable}, {"smaa_predication", &g_smaa_predication}, {"smaa_pred_debug", &g_smaa_pred_debug}, { "smaa_pred_measure",
                               &g_smaa_pred_measure }});
      Mcp::RegisterValues({{"smaa_pred_tolerance", &g_smaa_pred_tolerance, 0.002f, 0.2f}, {"rcas_sharpness", &g_rcas_sharpness, 0.f, 1.f}});
      Mcp::RegisterTextures({MCP_GAME_TEXTURE("smaa.input", tex_input),
         MCP_GAME_TEXTURE("smaa.pred_mask", tex_pred),
         MCP_GAME_TEXTURE("smaa.output", tex_smaa_out)});
#endif
#endif
      // Game-specific toggles consumed by the uber and gamma replacements (Luma_ME1_Tonemap.hlsl).
      std::vector<ShaderDefineData> game_shader_defines_data = {
         {"TONEMAP_TYPE", '1', true, false, "0 - SDR: Vanilla (clamped reference)\n1 - HDR: extended native grade + MacLeod-Boynton hue + DICE display map"},
      };
      shader_defines_data.append_range(game_shader_defines_data);
      assert(shader_defines_data.size() < MAX_SHADER_DEFINES);

      sr_game_tooltip = "Requires Luma-Upscaler.exe next to the game's exe.\n";

#if ENABLE_SMAA
      // Core auto-registers the 6 SMAA passes. Added here: the predication CS turning scene alpha into R16F edge-ness in [0,1].
      native_shaders_definitions.emplace(CompileTimeStringHash("ME1 Depth Extract CS"),
         ShaderDefinition("Luma_ME1_DepthExtract", reshade::api::pipeline_subobject_type::compute_shader));
      // RCAS PS, drawn via core "Copy VS" + DrawCustomPixelShader.
      native_shaders_definitions.emplace(CompileTimeStringHash("ME1 Sharpen PS"),
         ShaderDefinition{"Luma_RCAS_PS", reshade::api::pipeline_subobject_type::pixel_shader, nullptr, "sharpen_ps"});
#endif

      // DLSS/FSR: its depth and the camera motion from the scene's alpha, the CPU copies of vc4 (dgVoodoo maps it or updates it), and the
      // motion vector target written by every blend state
      native_shaders_definitions.emplace(CompileTimeStringHash("ME1 Motion Vector Fill CS"),
         ShaderDefinition("Luma_ME1_MotionVectorFill", reshade::api::pipeline_subobject_type::compute_shader));
      reshade::register_event<reshade::addon_event::map_buffer_region>(OnMapBufferRegion);
      reshade::register_event<reshade::addon_event::unmap_buffer_region>(OnUnmapBufferRegion);
      reshade::register_event<reshade::addon_event::update_buffer_region>(OnUpdateBufferRegion);
      reshade::register_event<reshade::addon_event::create_pipeline>(PatchedDraws::OnCreateBlendState<MotionVectorPatches::target_slot, MotionVectorPatches::reactive_slot>);
#if DEVELOPMENT
      reshade::register_event<reshade::addon_event::init_effect_runtime>(OnInitEffectRuntime);
      reshade::register_event<reshade::addon_event::destroy_effect_runtime>(OnDestroyEffectRuntime);
#endif

      // Address-space ceiling, PROBED: no LARGE_ADDRESS_AWARE (0x0102 measured) = 2 GB, Luma's 4K scratch ~365 MB.
      static bool address_space_checked = false;
      if (!address_space_checked)
      {
         address_space_checked = true;
         SYSTEM_INFO sys_info = {};
         GetSystemInfo(&sys_info);
         if (reinterpret_cast<uintptr_t>(sys_info.lpMaximumApplicationAddress) < 0x80000000u)
            reshade::log::message(reshade::log::level::warning,
               "[Luma] ME1: this executable is NOT large-address-aware, so the process is capped at 2 GB of address "
               "space. Luma's own scratch is ~365 MB at 4K, on top of dgVoodoo's copies: expect std::bad_alloc (UE3 "
               "reports it as a rendering GPF) with 4K texture packs, with the devkit, or both. Patch the "
               "LARGE_ADDRESS_AWARE bit, or lower the resolution / turn off SMAA and HDR bloom.");
      }

      // GAMMA space: the gamma-SDR HUD blends onto this canvas and linear washes it out. Composition encodes scRGB.
      GetShaderDefineData(POST_PROCESS_SPACE_TYPE_HASH).SetDefaultValue('0');
      GetShaderDefineData(EARLY_DISPLAY_ENCODING_HASH).SetDefaultValue('0');
      GetShaderDefineData(VANILLA_ENCODING_TYPE_HASH).SetDefaultValue('1'); // game shipped gamma-2.2 SDR
      GetShaderDefineData(GAMMA_CORRECTION_TYPE_HASH).SetDefaultValue('1');
      GetShaderDefineData(GAMUT_MAPPING_TYPE_HASH).SetDefaultValue('1'); // gamut-map wild colors in composition
      GetShaderDefineData(UI_DRAW_TYPE_HASH).SetDefaultValue('2');       // HUD gets its own UIPaperWhite + gamma blend

      // dgVoodoo's D3D9 mirrors are b3/b4, and no translated shader reads a constant buffer past b4: b9/b10 (the motion vector
      // draws, see "MotionVectorPatches"), b11 (core DrawBloom's own constants) and b12/b13 are free, as in MoHA.
      // luma_ui stays off: the game draws its own UI.
      luma_settings_cbuffer_index = 13;
      luma_data_cbuffer_index = 12;
      luma_ui_cbuffer_index = -1;

      // Manual paper-white sliders, not the OS reference level. Core gates the UI one on UI_DRAW_TYPE >= 1.
      use_os_reference_white_level = false;

      default_luma_global_game_settings.Exposure = 1.f;
      default_luma_global_game_settings.Saturation = 1.f;
      default_luma_global_game_settings.HighlightDechroma = 0.f; // off by default; only the mandatory DICE/gamut desaturation applies
      default_luma_global_game_settings.BloomIntensity = 1.f;
      default_luma_global_game_settings.Contrast = 1.f;
      default_luma_global_game_settings.Dithering = 1.f;
      default_luma_global_game_settings.LumaBloomEnable = ENABLE_BLOOM ? 1.f : 0.f;
      // 1.0 is exactly where the game's own bright-pass sits (DofBloomGather_0x56854256: any channel > 1.0).
      default_luma_global_game_settings.BloomThreshold = 1.f;
      // Light AutoHDR on the Bink pass (Video_0x1A82565B): movies bypass the scene passes. BL2's calibrated pair: at 0.5
      // the peak is 165 nits, ~2x sRGB white.
      default_luma_global_game_settings.VideoAutoHDREnable = 1.f;
      default_luma_global_game_settings.VideoAutoHDRBoost = 0.5f;
      // Until the first readback lands (~3 frames): the shipped DisplayGamma 1.6 (DefaultEngine.ini), measured 0.625.
      default_luma_global_game_settings.DisplayGammaInverse = 0.625f;
      // Until the gather reports: the value measured on the Citadel.
      default_luma_global_game_settings.BloomScaleLive = 0.1f;
      cb_luma_global_settings.GameSettings = default_luma_global_game_settings;
   }

   void OnCreateDevice(ID3D11Device* native_device, DeviceData& device_data) override
   {
      device_data.game = new MassEffectGameDeviceData;
      device_data.taa_detected = true; // No TAA to replace, but Core's upscaler status checks for it
#if DEVELOPMENT
      // For the MCP "luma_dev_values" tool: the last complete frame's counts
      const auto& stats = GetGameDeviceData(device_data).mv_last_stats;
      Mcp::RegisterCounters({{"mv.draws", &stats.motion_vector_draws}, {"mv.jitter_draws", &stats.jitter_draws}, {"mv.reactive_draws", &stats.reactive_draws}, {"mv.matched", &stats.matched},
                               {"mv.camera_only", &stats.camera_only}, {"mv.other_camera", &stats.other_camera}, {"mv.uncopied", &stats.uncopied}, {"mv.maps", &stats.maps}, {"mv.updates", &stats.updates},
                               {"mv.other_maps", &stats.other_maps}, {"mv.sr_draws", &stats.sr_draws}, {"mv.offset_bindings", &stats.offset_bindings}, {"mv.constants_pool", &stats.constants_pool}, {"mv.tiebreak_collisions", &stats.tiebreak_collisions}, {"mv.ended_by_hash", &stats.ended_by},
                               {"mv.scene_reads_after_end", &stats.scene_reads_after_end}, { "mv.scene_reader_hash",
                                  &stats.scene_reader }},
         &device_data);
      constexpr const char* reject_names[] = {"extra_target", "no_scene", "other_depth_color", "format", "size", "create", "blend", "shaders"};
      for (size_t i = 0; i < std::size(reject_names); i++)
         Mcp::RegisterCounter(std::string("mv.rejected.") + reject_names[i], &stats.rejected[i], &device_data);
#endif
   }

   // Core's default deletes through GameDeviceData*, which has no virtual destructor: delete the concrete type.
   void OnDestroyDeviceData(DeviceData& device_data) override
   {
#if DEVELOPMENT
      Mcp::Unregister(&device_data);
#endif
      delete static_cast<MassEffectGameDeviceData*>(device_data.game);
      device_data.game = nullptr;
   }

   // b12 at the gamma seam: whether the uber ran (skipped in elevators), which the gamma replacement branches on.
   void UpdateLumaInstanceDataCB(CB::LumaInstanceDataPadded& data, CommandListData& cmd_list_data, DeviceData& device_data) override
   {
      data.GameData.UberRanThisFrame = GetGameDeviceData(device_data).uber_ran_this_frame ? 1.f : 0.f;
   }

#if ENABLE_SMAA
   // SMAA after the grade, before the HUD (TW2/BL2 chain): snapshot -> DrawSMAA (its blend filters the snapshot in linear light)
   // -> [RCAS] -> canvas. Without "smaa" (the upscaler antialiased the frame, SR4's shape) only RCAS runs: snapshot -> RCAS -> canvas.
   void RunPostFinalGradeSMAA(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, DeviceData& device_data, MassEffectGameDeviceData& gd, ID3D11Resource* canvas_res, ID3D11RenderTargetView* canvas_rtv, bool smaa)
   {
      uint4 cinfo{};
      DXGI_FORMAT cfmt = DXGI_FORMAT_UNKNOWN;
      GetResourceInfo(canvas_res, cinfo, cfmt);
      const uint32_t w = cinfo.x, h = cinfo.y;
      if (w == 0 || h == 0 || cfmt == DXGI_FORMAT_UNKNOWN)
         return;

      // Resolution change: drop every size-bound resource of ours, so each is recreated at the new size (below; Core's
      // DrawSMAA checks its own). The predication CB does not depend on the size and stays.
      if (gd.smaa_w != w || gd.smaa_h != h)
      {
         gd.ReleaseSMAAScratch();
         gd.cb_smaa_metrics.reset();
         gd.cb_sharpen.reset();
         gd.smaa_w = w;
         gd.smaa_h = h;
      }

      // RCAS decides the chain's SHAPE (see "tex_smaa_out").
      auto* copy_vs = FindShader(device_data.native_vertex_shaders, CompileTimeStringHash("Copy VS"));
      auto* sharpen_ps = FindShader(device_data.native_pixel_shaders, CompileTimeStringHash("ME1 Sharpen PS"));
      bool do_sharpen = g_rcas_sharpness > 0.f && copy_vs != nullptr && sharpen_ps != nullptr;
      if (do_sharpen && (!gd.cb_sharpen || gd.sharpen_amount != g_rcas_sharpness))
      {
         const float sp[4] = {(float)w, (float)h, g_rcas_sharpness, 0.f};
         if (CreateImmutableCB(native_device, sp, sizeof(sp), gd.cb_sharpen))
            gd.sharpen_amount = g_rcas_sharpness;
      }
      do_sharpen = do_sharpen && gd.cb_sharpen;
      const auto sharpen = [&](ID3D11ShaderResourceView* source)
      {
         DrawStateStack<DrawStateStackType::FullGraphics> sharpen_state;
         sharpen_state.Cache(native_device_context, device_data.uav_max_count);

         ID3D11Buffer* scb = gd.cb_sharpen.get();
         native_device_context->PSSetConstantBuffers(0, 1, &scb);
         DrawCustomPixelShader(native_device_context, device_data.default_depth_stencil_state.get(), device_data.default_blend_state.get(), nullptr,
            copy_vs, sharpen_ps, source, canvas_rtv, w, h, false);

         sharpen_state.Restore(native_device_context);
      };

      if (!smaa)
      {
         if (!do_sharpen)
            return;
         if (!gd.srv_input)
         {
            gd.tex_input.reset();
            if (CreateDefaultTex(native_device, w, h, D3D11_BIND_SHADER_RESOURCE, gd.tex_input, cfmt))
               native_device->CreateShaderResourceView(gd.tex_input.get(), nullptr, gd.srv_input.put());
         }
         if (!gd.srv_input)
            return;
         native_device_context->CopyResource(gd.tex_input.get(), canvas_res);
         sharpen(gd.srv_input.get());
         return;
      }
      gd.smaa_idle_frames = 0;

      // Shader-readiness gate (async loader / dev live-reload): skip SMAA this frame if anything is missing.
      const bool smaa_ready = HasShaders(device_data.native_pixel_shaders, CompileTimeStringHash("SMAA Edge Detection PS"), CompileTimeStringHash("SMAA Blending Weight Calculation PS"), CompileTimeStringHash("SMAA Neighborhood Blending PS")) && HasShaders(device_data.native_vertex_shaders, CompileTimeStringHash("SMAA Edge Detection VS"), CompileTimeStringHash("SMAA Blending Weight Calculation VS"), CompileTimeStringHash("SMAA Neighborhood Blending VS"));
      if (!smaa_ready)
         return;

      // Scale and mask fall back together: 2.0 with a null mask raises the threshold frame-wide. CS maps texels 1:1.
      auto* pred_cs = FindShader(device_data.native_compute_shaders, CompileTimeStringHash("ME1 Depth Extract CS"));
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
         if (!gd.uav_pred || !gd.srv_pred)
         {
            gd.ReleasePredicationScratch();
            if (CreateDefaultTex(native_device, w, h, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, gd.tex_pred, DXGI_FORMAT_R16_FLOAT))
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

      if (do_sharpen)
      {
         if (!gd.tex_smaa_out_rtv || !gd.tex_smaa_out_srv)
         {
            gd.ReleaseSharpenScratch();
            if (CreateDefaultTex(native_device, w, h, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET, gd.tex_smaa_out, cfmt))
            {
               native_device->CreateRenderTargetView(gd.tex_smaa_out.get(), nullptr, gd.tex_smaa_out_rtv.put());
               native_device->CreateShaderResourceView(gd.tex_smaa_out.get(), nullptr, gd.tex_smaa_out_srv.put());
            }
         }
         if (!gd.tex_smaa_out_rtv || !gd.tex_smaa_out_srv)
            do_sharpen = false; // allocation failed: fall back to the un-sharpened chain rather than dropping SMAA
      }

      if (!gd.srv_input)
      {
         gd.tex_input.reset();
         if (CreateDefaultTex(native_device, w, h, D3D11_BIND_SHADER_RESOURCE, gd.tex_input, cfmt))
            native_device->CreateShaderResourceView(gd.tex_input.get(), nullptr, gd.srv_input.put());
      }
      if (!gd.srv_input)
         return;

      native_device_context->CopyResource(gd.tex_input.get(), canvas_res);

      // Scene alpha -> plane-deviation edge-ness (R16F); why an edge test, not a rescale: Luma_ME1_DepthExtract.hlsl.
      if (pred_ok)
      {
         DrawStateStack<DrawStateStackType::Compute> pred_cs_state;
         pred_cs_state.Cache(native_device_context, device_data.uav_max_count);

         ID3D11ShaderResourceView* pred_srv = gd.srv_scene.get();
         ID3D11UnorderedAccessView* pred_uav = gd.uav_pred.get();
         ID3D11Buffer* pred_cb = gd.cb_pred.get();
         native_device_context->CSSetShaderResources(0, 1, &pred_srv);
         native_device_context->CSSetUnorderedAccessViews(0, 1, &pred_uav, nullptr);
         native_device_context->CSSetConstantBuffers(0, 1, &pred_cb);
         native_device_context->CSSetShader(pred_cs, nullptr, 0);
         native_device_context->Dispatch((w + 7) / 8, (h + 7) / 8, 1);

         pred_cs_state.Restore(native_device_context);
      }

#if DEVELOPMENT
      // Calibration aids: both read the mask just written.
      if (g_smaa_pred_measure)
      {
         g_smaa_pred_measure = false;
         if (pred_ok)
            MeasurePredicationMask(native_device, native_device_context, gd.tex_pred.get());
         else
            reshade::log::message(reshade::log::level::warning, "[Luma] ME1 DIAG: SMAA predication mask not measured: predication inactive this frame (no scene capture, size mismatch or CS missing)");
      }
      if (pred_ok && g_smaa_pred_debug)
      {
         auto* copy_ps = FindShader(device_data.native_pixel_shaders, CompileTimeStringHash("Copy PS"));
         if (copy_vs != nullptr && copy_ps != nullptr)
         {
            // Single-channel, so the copy shows it in RED. It replaces the frame, hence the early return.
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

      if (do_sharpen)
         sharpen(gd.tex_smaa_out_srv.get());

      ID3D11Buffer* vcb = vs_cb1_orig.get();
      ID3D11Buffer* pcb = ps_cb1_orig.get();
      native_device_context->VSSetConstantBuffers(1, 1, &vcb);
      native_device_context->PSSetConstantBuffers(1, 1, &pcb);
   }
#endif // ENABLE_SMAA

   DrawOrDispatchOverrideType OnDrawOrDispatch(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, CommandListData& cmd_list_data, DeviceData& device_data, reshade::api::shader_stage stages, const ShaderHashesList<OneShaderPerPipeline>& original_shader_hashes, bool is_custom_pass, bool& updated_cbuffers, std::function<void()>* original_draw_dispatch_func) override
   {
      auto& gd = GetGameDeviceData(device_data);

      const bool is_immediate = native_device_context->GetType() == D3D11_DEVICE_CONTEXT_IMMEDIATE;

      // Hide HUD: cancel post-final draws targeting the same canvas - the render-target test is the load-bearing half.
      // The UI families are hash-replaced, so is_custom_pass is true for them: an original PS hash (not UINT64_MAX)
      // is what separates a replaced game draw from one of Luma's own injected passes.
      if (g_hide_ui && is_immediate && (!is_custom_pass || original_shader_hashes.pixel_shaders[0] != UINT64_MAX) && device_data.has_drawn_main_post_processing && gd.canvas_res)
      {
         ComPtr<ID3D11Resource> rt = GetBoundRenderTargetResource(native_device_context);
         if (rt.get() == gd.canvas_res.get())
            return DrawOrDispatchOverrideType::Replaced;
      }

#if DEVELOPMENT
      // Whether anything reads the upscaled scene itself after its end, while the post passes read a copy of it (the upscaler's
      // output could then go to the copy only). Off while a "Performance Test" runs: every draw after the scene reads all its views here.
      if (Perf::g_test == 0 && gd.mv_active && gd.mv_scene_done && !gd.mv_scene_overwritten && gd.mv_scene_copy && gd.mv_scene_color)
      {
         com_ptr<ID3D11ShaderResourceView> srvs[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
         if ((stages & reshade::api::shader_stage::compute) == reshade::api::shader_stage::compute)
            native_device_context->CSGetShaderResources(0, UINT(std::size(srvs)), &srvs[0]);
         else
            native_device_context->PSGetShaderResources(0, UINT(std::size(srvs)), &srvs[0]);
         for (const auto& srv : srvs)
         {
            com_ptr<ID3D11Resource> resource;
            if (srv)
               srv->GetResource(&resource);
            if (resource && resource == gd.mv_scene_color)
            {
               gd.mv_stats.scene_reads_after_end++;
               gd.mv_stats.scene_reader = uint32_t((stages & reshade::api::shader_stage::compute) == reshade::api::shader_stage::compute ? original_shader_hashes.compute_shaders[0] : original_shader_hashes.pixel_shaders[0]);
               gd.mv_scene_reads_after_end++;
               break;
            }
         }
         if (GetBoundRenderTargetResource(native_device_context).get() == gd.mv_scene_color.get())
            gd.mv_scene_overwritten = true;
      }
#endif

      // DLSS/FSR: the scene's draws with motion vectors or jitter (see "DrawWithMotionVectors"), and the upscaler at its first post pass
      if (gd.mv_active && is_immediate)
      {
#if DEVELOPMENT
         const Perf::HookTimer perf_timer{&gd.perf_window.hook_ns}; // The draws' own submission included
#endif
         if (!gd.mv_scene_done && std::ranges::any_of(kScenePostHashes, [&](uint32_t hash)
                                     { return original_shader_hashes.Contains(hash, reshade::api::shader_stage::pixel); }))
         {
            if (gd.mv_scene_open)
            {
#if DEVELOPMENT
               gd.mv_stats.ended_by = uint32_t(original_shader_hashes.pixel_shaders[0]);
#endif
               // The scene or a copy of it (same description, CopyResource compatible) among the pass's textures
               gd.mv_scene_copy.reset();
               gd.mv_scene_read_by_end = false;
               if (gd.mv_scene_color)
               {
                  com_ptr<ID3D11ShaderResourceView> srvs[8];
                  native_device_context->PSGetShaderResources(0, 8, &srvs[0]);
                  for (int slot = 0; slot < 8; slot++)
                  {
                     com_ptr<ID3D11Resource> resource;
                     if (srvs[slot])
                        srvs[slot]->GetResource(&resource);
                     if (!resource || (resource != gd.mv_scene_color && !AreResourcesEqual(resource.get(), gd.mv_scene_color.get())))
                        continue;
#if DEVELOPMENT
                     if (!gd.mv_scene_copy && !gd.mv_scene_read_by_end)
                        gd.mv_stats.ended_by_scene_slot = slot;
#endif
                     if (resource == gd.mv_scene_color)
                        gd.mv_scene_read_by_end = true;
                     else if (!gd.mv_scene_copy)
                        gd.mv_scene_copy = resource;
                  }
               }
               EndScene(native_device, native_device_context, device_data);
#if DEVELOPMENT
               if (GetBoundRenderTargetResource(native_device_context).get() == gd.mv_scene_color.get())
                  gd.mv_scene_overwritten = true;
#endif
            }
         }
         else if (!gd.mv_scene_done && !is_custom_pass && original_draw_dispatch_func && *original_draw_dispatch_func && (stages & reshade::api::shader_stage::vertex) == reshade::api::shader_stage::vertex)
         {
            com_ptr<ID3D11RenderTargetView> rtvs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
            com_ptr<ID3D11DepthStencilView> dsv;
            native_device_context->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, &rtvs[0], &dsv);
            if (!gd.mv_scene_open && dsv)
               OpenScene(native_device, native_device_context, cmd_list_data, device_data, original_shader_hashes.vertex_shaders[0], dsv.get());
            // The draw with dgVoodoo's per-target blend repaired, as every other draw (see "FixImpossiblePerRTBlend")
            const std::function<void()> draw = [&]
            {
               if (!FixImpossiblePerRTBlend(native_device, native_device_context, gd, stages, original_shader_hashes, is_custom_pass, original_draw_dispatch_func))
                  (*original_draw_dispatch_func)();
            };
            const bool motion_vectors = DrawWithMotionVectors(native_device, native_device_context, cmd_list_data, device_data, original_shader_hashes, draw, rtvs, dsv.get());
            const bool jitter = !motion_vectors && DrawWithJitter(native_device, native_device_context, cmd_list_data, device_data, original_shader_hashes, draw, rtvs, dsv.get());
#if DEVELOPMENT
            Mcp::Annotate(cmd_list_data, motion_vectors ? "mv" : (jitter ? "jitter" : "unpatched"));
            if (const int reject = std::exchange(gd.mv_draw_reject, -1); reject >= 0)
               Mcp::Annotate(cmd_list_data, "mv_reject", reject);
#endif
#if DEVELOPMENT
            if (gd.mv_dumping && gd.mv_scene_open)
            {
               com_ptr<ID3D11BlendState> blend_state;
               native_device_context->OMGetBlendState(&blend_state, nullptr, nullptr);
               D3D11_BLEND_DESC blend_desc = CD3D11_BLEND_DESC(D3D11_DEFAULT);
               if (blend_state)
                  blend_state->GetDesc(&blend_desc);
               com_ptr<ID3D11DepthStencilState> depth_state;
               UINT stencil_ref = 0;
               native_device_context->OMGetDepthStencilState(&depth_state, &stencil_ref);
               D3D11_DEPTH_STENCIL_DESC depth_desc = CD3D11_DEPTH_STENCIL_DESC(D3D11_DEFAULT);
               if (depth_state)
                  depth_state->GetDesc(&depth_desc);
               com_ptr<ID3D11Resource> color;
               if (rtvs[0])
                  rtvs[0]->GetResource(&color);
               const auto& rt0 = blend_desc.RenderTarget[0];
               reshade::log::message(reshade::log::level::info, std::format("[ME1 DUMP] {} vs 0x{:08X} ps 0x{:08X} {} rt0 {} blend {} {}/{}/{} a {}/{}/{} mask {:X} depth {} func {} write {} stencil {}",
                                                                   gd.mv_dump_index++, uint32_t(original_shader_hashes.vertex_shaders[0]), uint32_t(original_shader_hashes.pixel_shaders[0]),
                                                                   motion_vectors ? "MV" : (jitter ? "JIT" : "-"), !color ? "none" : (color == gd.mv_scene_color ? "scene" : "other"),
                                                                   int(rt0.BlendEnable), int(rt0.SrcBlend), int(rt0.DestBlend), int(rt0.BlendOp), int(rt0.SrcBlendAlpha), int(rt0.DestBlendAlpha), int(rt0.BlendOpAlpha), int(rt0.RenderTargetWriteMask),
                                                                   int(depth_desc.DepthEnable), int(depth_desc.DepthFunc), int(depth_desc.DepthWriteMask), int(depth_desc.StencilEnable))
                                                                   .c_str());
            }
#endif
            if (motion_vectors || jitter)
               return DrawOrDispatchOverrideType::Replaced;
         }
      }
      // Every draw without a patched shader: the game's own, if the last patched draw's are still bound
      if (is_immediate)
      {
         PatchedDraws::RestoreGameShader(native_device_context, &gd.mv_bound_vertex_shader);
         PatchedDraws::RestoreGameShader(native_device_context, &gd.mv_bound_pixel_shader);
      }

#if ENABLE_BLOOM
      // The gather runs before the uber: read BloomScale here, deferred. Bloom-only, so it follows the bloom switch.
      if (g_luma_bloom_enable && is_immediate && !gd.bloom_cb_ring.advanced_this_frame && IsDofBloomGather(original_shader_hashes))
         TrackCB4Row(native_device, native_device_context, gd.bloom_cb_ring, 11, 0.f, 4.f, &gd.bloom_scale_live, &gd.bloom_scale_valid); // engine BloomScale, measured 0.1
#endif

#if DEVELOPMENT
      // HUD permutation net: logs unreplaced post-final canvas draws. UI_*.hlsl cover eight families x both dgVoodoo
      // builds; the net is complete only FOR WHAT DREW.
      if (is_immediate && !is_custom_pass && device_data.has_drawn_main_post_processing && gd.canvas_res && original_shader_hashes.pixel_shaders[0] != UINT64_MAX)
      {
         const uint32_t ps_hash = (uint32_t)original_shader_hashes.pixel_shaders[0];
         if (!gd.diag_post_final_ps.contains(ps_hash) && GetBoundRenderTargetResource(native_device_context).get() == gd.canvas_res.get())
         {
            gd.diag_post_final_ps.insert(ps_hash);
            ComPtr<ID3D11BlendState> blend_state;
            float blend_factor[4];
            UINT sample_mask;
            native_device_context->OMGetBlendState(blend_state.put(), blend_factor, &sample_mask);
            D3D11_BLEND_DESC blend_desc = {};
            if (blend_state)
               blend_state->GetDesc(&blend_desc);
            const D3D11_RENDER_TARGET_BLEND_DESC& rt0 = blend_desc.RenderTarget[0];
            char label[128];
            std::snprintf(label, sizeof(label), "post-final canvas PS 0x%08X UNREPLACED blend %u src %u dst %u srcA %u dstA %u", ps_hash, rt0.BlendEnable, rt0.SrcBlend, rt0.DestBlend, rt0.SrcBlendAlpha, rt0.DestBlendAlpha);
            DumpConstantRows(native_device, native_device_context, label, 10, 2);
         }
      }

      if (is_immediate && !gd.diag_logged_gather && IsDofBloomGather(original_shader_hashes))
      {
         gd.diag_logged_gather = true;
         DumpBoundRenderTarget(native_device_context, "bloom buffer");
         // Vanilla bloom model: BloomScale (PS row 11.x) and 16 tap offsets (VS rows 20..27, xy/wz pairs, UV units).
         DumpConstantRows(native_device, native_device_context, "gather PS", 8, 4);
         DumpConstantRows(native_device, native_device_context, "gather VS", 20, 8, true);
      }
      // The separable blur: 9 weights (PS rows 10..18), 4 offset pairs and the centre (VS rows 25..29), both directions of one frame.
      if (is_immediate && gd.diag_filter_dumps < 2 && ContainsPixelShader(original_shader_hashes, kBloomFilterHash, kBloomFilterHash_v281))
      {
         gd.diag_filter_dumps++;
         DumpConstantRows(native_device, native_device_context, gd.diag_filter_dumps == 1 ? "filter#1 PS" : "filter#2 PS", 10, 9);
         DumpConstantRows(native_device, native_device_context, gd.diag_filter_dumps == 1 ? "filter#1 VS" : "filter#2 VS", 25, 5, true);
      }
#endif

      // Deliberately AHEAD of the gate below: an unkeyed build must be reported whichever context saw the pass.
      if (!gd.ever_matched_final_pass && (IsFinalColorPass(original_shader_hashes) || IsUberPostPass(original_shader_hashes)))
         gd.ever_matched_final_pass = true;

#if DEVELOPMENT
      // Which pass encodes: uber row 15.w, copy row 8, gamma row 11.x - measured 1.0 / 1.0 / 0.625.
      if (is_immediate && g_dump_pass_cb && ContainsPixelShader(original_shader_hashes, kCopyPassHash, kCopyPassHash_v281))
         DumpConstantRows(native_device, native_device_context, "copy 0x1E37D75B", 8, 1);
#endif

      // The uber pass. Read the hash list before any early-out: is_custom_pass is true for hash-replaced passes too.
      if (is_immediate && !gd.uber_ran_this_frame && IsUberPostPass(original_shader_hashes))
      {
         gd.uber_ran_this_frame = true;

         // Push LumaSettings at the seam, not in a feature block (MoHA): bloom off left the grade on a stale upload.
         SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, reshade::api::shader_stage::pixel, LumaConstantBufferType::LumaSettings);

         // Scene at t0 here (fp16, alpha = linear depth): the bloom and SMAA predication source, no extra pass.
         gd.srv_scene.reset();
         native_device_context->PSGetShaderResources(0, 1, gd.srv_scene.put());

#if DEVELOPMENT
         if (g_dump_pass_cb)
         {
            DumpBoundRenderTarget(native_device_context, "uber target (scene B)");
            DumpConstantRows(native_device, native_device_context, "uber 0xAC8341E0", 8, 9);
         }
#endif

#if ENABLE_BLOOM
         BindLumaBloom(native_device, native_device_context, device_data, gd.srv_scene.get());
#endif
      }

#if ENABLE_BLOOM
      // Uber-less chains composite DoF and bloom here instead: the replacement adds the Luma glow the uber would have.
      if (is_immediate && ContainsPixelShader(original_shader_hashes, kDofBloomBlendHash, kDofBloomBlendHash_v281))
      {
         SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, reshade::api::shader_stage::pixel, LumaConstantBufferType::LumaSettings);
         ComPtr<ID3D11ShaderResourceView> srv_blend_scene;
         native_device_context->PSGetShaderResources(0, 1, srv_blend_scene.put());
         BindLumaBloom(native_device, native_device_context, device_data, srv_blend_scene.get());
      }
#endif

      if (is_immediate && !device_data.has_drawn_main_post_processing && IsFinalColorPass(original_shader_hashes))
      {
         device_data.has_drawn_main_post_processing = true;

         // Same seam rule: this pass reads LumaSettings and LumaData, and the SMAA path skips core's upload.
         SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, reshade::api::shader_stage::pixel, LumaConstantBufferType::LumaSettings);
         SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, reshade::api::shader_stage::pixel, LumaConstantBufferType::LumaData);

         // Gamma-only frames: nothing captured the scene for predication, and this pass reads the RAW fp16 scene at t0.
         if (!gd.uber_ran_this_frame)
         {
            gd.srv_scene.reset();
            native_device_context->PSGetShaderResources(0, 1, gd.srv_scene.put());
         }

         // Hide UI needs the resource, SMAA the view. Recaptured each frame: upgrades and resizes swap the mirror.
         ComPtr<ID3D11RenderTargetView> canvas_rtv;
         native_device_context->OMGetRenderTargets(1, canvas_rtv.put(), nullptr);
         gd.canvas_res.reset();
         if (canvas_rtv)
            canvas_rtv->GetResource(gd.canvas_res.put());

#if DEVELOPMENT
         if (gd.canvas_res && gd.canvas_resources_seen.insert(uint64_t(gd.canvas_res.get())).second)
            reshade::log::message(reshade::log::level::info, std::format("[ME1 Mem] new canvas 0x{:X} (#{}) at frame {}: sr={} drawn={} smaa={} uber={}", uint64_t(gd.canvas_res.get()), gd.canvas_resources_seen.size(), cb_luma_global_settings.FrameIndex, int(device_data.sr_type), device_data.has_drawn_sr.load(), g_smaa_enable, gd.uber_ran_this_frame).c_str());
         if (!gd.diag_logged_rt)
         {
            gd.diag_logged_rt = true;
            DumpBoundRenderTarget(native_device_context, "canvas");
         }
         if (g_dump_pass_cb)
         {
            DumpConstantRows(native_device, native_device_context, "gamma 0x17CE0932", 8, 4);
            g_dump_pass_cb = false; // last of the frame's three dumps (copy -> uber -> gamma), so disarm here
         }
#endif

         // The display gamma this pass applies, for the grade's SDR reference. Once per frame, via the gate above.
         TrackCB4Row(native_device, native_device_context, gd.gamma_cb_ring, 11, 0.25f, 1.f, &gd.gamma_inverse_live, &gd.gamma_inverse_valid); // 1/gamma for gamma in [1, 4]

#if ENABLE_SMAA
         // Run the pass ourselves, then SMAA, so AA lands before the HUD. Otherwise the game draws it without AA.
         // On a frame the upscaler already antialiased only RCAS runs.
         const bool antialias = device_data.has_drawn_sr ? g_rcas_sharpness > 0.f : g_smaa_enable;
         if (antialias && original_draw_dispatch_func != nullptr && canvas_rtv && gd.canvas_res)
         {
            (*original_draw_dispatch_func)();
            RunPostFinalGradeSMAA(native_device, native_device_context, device_data, gd, gd.canvas_res.get(), canvas_rtv.get(), !device_data.has_drawn_sr);
            return DrawOrDispatchOverrideType::Replaced; // we ran the original draw ourselves
         }
#endif
      }

      // Blend repair LAST: it re-issues the draw, so it must yield to every hook above (it disabled one in TW2).
      if (FixImpossiblePerRTBlend(native_device, native_device_context, gd, stages, original_shader_hashes, is_custom_pass, original_draw_dispatch_func))
         return DrawOrDispatchOverrideType::Replaced;

      return DrawOrDispatchOverrideType::None; // never cancel the original draw (the replacement is by hash)
   }

#if DEVELOPMENT
   static void ApplyPerfTestMode(DeviceData& device_data, int mode_index)
   {
      auto& gd = GetGameDeviceData(device_data);
      const PerfTestMode& mode = perf_test_modes[mode_index];
      const PerfTestMode& previous_mode = perf_test_modes[Perf::g_test];
      if (!previous_mode.set_aa && mode.set_aa)
      {
         gd.perf_user_sr_type = device_data.sr_type;
         gd.perf_user_smaa = g_smaa_enable;
         gd.perf_user_reactive_mask = g_sr_reactive_enable;
      }
      if (mode.set_aa || previous_mode.set_aa)
      {
         SetSRType(device_data, (mode.set_aa ? mode.sr_type : gd.perf_user_sr_type));
         device_data.sr_suppressed = false;
         g_smaa_enable = mode.set_aa ? mode.smaa : gd.perf_user_smaa;
         g_sr_reactive_enable = mode.set_aa ? mode.reactive_mask : gd.perf_user_reactive_mask;
      }
      g_perf_cpu_optimizations_off = mode.cpu_optimizations_off;
      Perf::g_test = mode_index;
   }

#if ENABLE_SR_BRIDGE
   // "SR Bridge Profile Sweep": the next mode whose upscaler this device supports ("sweep_step" -1 to start), its profile requested
   // at the next present (which latches the upscaler). False when none is left.
   static bool AdvanceBridgeProfileSweep(DeviceData& device_data)
   {
      using namespace BridgeProfile;
      while (++sweep_step < kSweepSteps && !device_data.sr_implementations_instances.contains(kSweepModes[sweep_step % std::size(kSweepModes)].sr_type))
         ;
      if (sweep_step >= kSweepSteps)
         return false;
      SetSRType(device_data, kSweepModes[sweep_step % std::size(kSweepModes)].sr_type);
      state = State::Requested;
      return true;
   }
#endif

   static void ApplyMemorySweepMode(DeviceData& device_data, int step)
   {
      auto& gd = GetGameDeviceData(device_data);
      if (g_memory_sweep_step < 0)
      {
         gd.memory_user_sr_type = device_data.sr_type;
         gd.memory_user_smaa = g_smaa_enable;
         gd.memory_user_bloom = g_luma_bloom_enable;
         gd.memory_user_rcas = g_rcas_sharpness;
         gd.memory_user_reactive_mask = g_sr_reactive_enable;
      }
      const bool restore = step < 0;
      const MemorySweepMode& mode = memory_sweep_modes[restore ? 0 : step];
      SetSRType(device_data, restore ? gd.memory_user_sr_type : mode.sr_type);
      g_smaa_enable = restore ? gd.memory_user_smaa : mode.smaa;
      g_luma_bloom_enable = restore ? gd.memory_user_bloom : mode.bloom;
      g_rcas_sharpness = restore ? gd.memory_user_rcas : mode.rcas;
      g_sr_reactive_enable = restore ? gd.memory_user_reactive_mask : mode.reactive_mask;
      g_memory_sweep_step = step;
      gd.memory_sweep_settle_frames = memory_sweep_settle_frames;
      gd.memory_sweep_wait_frames = 0;
   }

   // The process's GPU memory (DXGI, this process only: the SR bridge's helper is not in it), commit and address space, plus the
   // mod's own bookkeeping: fp16 mirrors (single mip size), the per pipeline bytecode copies and the motion vector constants copies
   static void LogMemoryReport(ID3D11Device* native_device, DeviceData& device_data, const char* label)
   {
      auto& gd = GetGameDeviceData(device_data);
      DXGI_QUERY_VIDEO_MEMORY_INFO local = {}, non_local = {};
      com_ptr<IDXGIDevice> dxgi_device;
      com_ptr<IDXGIAdapter> adapter;
      com_ptr<IDXGIAdapter3> adapter3;
      if (SUCCEEDED(native_device->QueryInterface(&dxgi_device)) && SUCCEEDED(dxgi_device->GetAdapter(&adapter)) && SUCCEEDED(adapter->QueryInterface(&adapter3)))
      {
         adapter3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &local);
         adapter3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL, &non_local);
      }
      PROCESS_MEMORY_COUNTERS_EX pmc = {sizeof(pmc)};
      GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc), sizeof(pmc));
      size_t free_va = 0, largest_free_va = 0;
      MEMORY_BASIC_INFORMATION mbi;
      for (uintptr_t address = 0; VirtualQuery(reinterpret_cast<void*>(address), &mbi, sizeof(mbi)); address = uintptr_t(mbi.BaseAddress) + mbi.RegionSize)
      {
         if (mbi.State == MEM_FREE)
         {
            free_va += mbi.RegionSize;
            largest_free_va = (std::max)(largest_free_va, size_t(mbi.RegionSize));
         }
         if (uintptr_t(mbi.BaseAddress) + mbi.RegionSize < uintptr_t(mbi.BaseAddress))
            break;
      }
      size_t mirrors = 0, mirror_bytes = 0;
      std::string mirror_list;
      {
         const std::shared_lock lock(device_data.mutex);
         for (const auto& [original, mirror] : device_data.resource_upgrades.original_resources_to_mirrored_upgraded_resources)
         {
            mirrors++;
            mirror_bytes += size_t(mirror.mirror_width) * mirror.mirror_height * 8;
            // Which ones: the original's format, size, mips and bind flags
            com_ptr<ID3D11Texture2D> texture;
            D3D11_TEXTURE2D_DESC desc = {};
            if (SUCCEEDED(reinterpret_cast<ID3D11Resource*>(original)->QueryInterface(&texture)))
               texture->GetDesc(&desc);
            mirror_list += std::format(" [{}x{} format {} mips {} bind 0x{:X} misc 0x{:X}{}{}]", desc.Width, desc.Height, int(desc.Format), desc.MipLevels, desc.BindFlags, desc.MiscFlags, mirror.is_scaled ? " scaled" : "", gd.canvas_resources_seen.contains(mirror.mirror_handle) || gd.canvas_resources_seen.contains(original) ? " canvas" : "");
         }
      }
      size_t pipelines = 0, bytecode_bytes = 0;
      {
         const std::shared_lock lock(s_mutex_generic);
         for (const auto& [handle, pipeline] : device_data.pipeline_cache_by_pipeline_handle)
         {
            pipelines++;
            if (pipeline->subobjects_cache && pipeline->subobjects_cache[0].data)
               bytecode_bytes += static_cast<const reshade::api::shader_desc*>(pipeline->subobjects_cache[0].data)->code_size;
         }
      }
      size_t constants_copies = 0, constants_bytes = 0;
      {
         const std::scoped_lock lock(gd.mv_constants_mutex);
         for (const auto& [buffer, copy] : gd.mv_constants_copies)
         {
            constants_copies++;
            constants_bytes += copy ? copy->size() : 0;
         }
      }
      constexpr double MiB = 1024.0 * 1024.0;
      reshade::log::message(reshade::log::level::info, std::format("[ME1 Mem] \"{}\" vram local={:.1f} non-local={:.1f} MiB private={:.1f} MiB va free={:.1f} largest={:.1f} MiB mirrors={} ({:.1f} MiB) pipelines={} (bytecode {:.1f} MiB) mv constants copies={} ({:.1f} MiB) mv shaders vs={} ps={} output={}x{}",
                                                          label, local.CurrentUsage / MiB, non_local.CurrentUsage / MiB, pmc.PrivateUsage / MiB, free_va / MiB, largest_free_va / MiB, mirrors, mirror_bytes / MiB, pipelines, bytecode_bytes / MiB, constants_copies, constants_bytes / MiB, gd.mv_vertex_shaders.size(), gd.mv_pixel_shaders.size(), uint32_t(device_data.output_resolution.x), uint32_t(device_data.output_resolution.y))
                                                          .c_str());
      reshade::log::message(reshade::log::level::info, std::format("[ME1 Mem] \"{}\" mirrored originals:{}", label, mirror_list).c_str());
   }
#endif

#if DEVELOPMENT
   // Copies of the scene itself after its end count as reads of it (see "scene_reads_after_end")
   static void CountSceneCopyAfterEnd(DeviceData& device_data, uint64_t dst_resource, uint64_t src_resource)
   {
      auto& gd = GetGameDeviceData(device_data);
      if (gd.mv_active && gd.mv_scene_done && !gd.mv_scene_overwritten && gd.mv_scene_copy && gd.mv_scene_color)
      {
         if (src_resource == uint64_t(gd.mv_scene_color.get()))
         {
            gd.mv_stats.scene_reads_after_end++;
            gd.mv_stats.scene_reader = 0;
            gd.mv_scene_reads_after_end++;
         }
         if (dst_resource == uint64_t(gd.mv_scene_color.get()))
            gd.mv_scene_overwritten = true;
      }
   }

   bool OverrideCopyResource(ID3D11Device* native_device, DeviceData& device_data, uint64_t& dst_resource, uint64_t& src_resource) override
   {
      CountSceneCopyAfterEnd(device_data, dst_resource, src_resource);
      return false;
   }

   bool OverrideCopyTextureRegion(ID3D11Device* native_device, DeviceData& device_data, uint64_t& dst_resource, uint32_t dst_subresource, const D3D11_BOX* dst_box, uint64_t& src_resource, uint32_t src_subresource, const D3D11_BOX* src_box) override
   {
      CountSceneCopyAfterEnd(device_data, dst_resource, src_resource);
      return false;
   }
#endif

   void CleanExtraSRResources(DeviceData& device_data) override
   {
      GetGameDeviceData(device_data).release_sr_resources = true;
   }

   void OnPresent(ID3D11Device* native_device, DeviceData& device_data) override
   {
      auto& gd = GetGameDeviceData(device_data);
      // One-shot telemetry (BL GOTY). Warmup budget only: the menu runs the same pass, so a few frames are enough.
      constexpr uint32_t kBuildCheckFrame = 120;
      if (gd.frames_presented < kBuildCheckFrame && ++gd.frames_presented == kBuildCheckFrame && !gd.ever_matched_final_pass)
         reshade::log::message(reshade::log::level::warning,
            "[Luma] ME1: no keyed final color pass seen after warmup -- the dgVoodoo build is probably neither 2.87.3/2.87.5 nor 2.81.3, so every shader replacement is inactive (re-dump the shaders for it).");

      // DLSS/FSR: the history restarts after any frame it didn't draw (menus, loading, just picked); the selection and the motion
      // vector state are fixed here for the next frame (see "IsSRActive")
      gd.sr_active = LatchSRFrame(device_data);
      gd.mv_active = IsSRActive(device_data) || g_mv_enable;
      // None picked: Core stopped the SR bridge's helper ("ReleaseResources"), our upscaler inputs and output go too. Recreated when
      // an upscaler is picked again (the helper takes seconds to start).
      if (device_data.sr_type == SR::Type::None && gd.release_sr_resources.exchange(false))
      {
         gd.sr_output_srv.reset();
         if (!g_mv_enable)
         {
            const std::unique_lock lock(gd.mv_mutex);
            gd.mv_texture.reset();
            gd.mv_rtv.reset();
            gd.mv_uav.reset();
            gd.mv_device_depth.reset();
            gd.mv_device_depth_uav.reset();
            gd.mv_reactive.reset();
            gd.mv_reactive_uav.reset();
            gd.mv_transparency.reset();
            gd.mv_transparency_uav.reset();
            gd.mv_reactive_target.reset();
            gd.mv_reactive_target_rtv.reset();
            gd.mv_reactive_target_srv.reset();
            // The game's scene, depth and scene copy (taken again at the next scene), so a resize after None doesn't keep the old
            // ones alive; and the vc4 copies the object tables and the cameras hold (heap, 4.4 KB each)
            gd.mv_depth.reset();
            gd.mv_scene_color.reset();
            gd.mv_scene_rtv.reset();
            gd.mv_scene_srv.reset();
            gd.mv_scene_copy.reset();
            gd.mv_scene_copy_rtv.reset();
            gd.mv_objects.clear();
            gd.mv_previous_objects.clear();
            gd.mv_camera.reset();
            gd.mv_previous_camera.reset();
            const std::lock_guard constants_lock(gd.mv_constants_mutex);
            gd.mv_constants_pool.clear();
            gd.mv_constants_pool_free.clear();
         }
      }
#if DEVELOPMENT
      gd.mv_dumping = std::exchange(g_mv_dump_scene, false);
      gd.mv_dump_index = 0;
      if (gd.gpu_thread_priority != g_gpu_thread_priority)
      {
         com_ptr<IDXGIDevice> dxgi_device;
         if (SUCCEEDED(native_device->QueryInterface(&dxgi_device)) && SUCCEEDED(dxgi_device->SetGPUThreadPriority(g_gpu_thread_priority)))
            gd.gpu_thread_priority = g_gpu_thread_priority;
         else
            g_gpu_thread_priority = gd.gpu_thread_priority;
      }
#if ENABLE_SR_BRIDGE
      // "SR Bridge Profile" (see "BridgeProfile"): a frame is recorded from present to present
      {
         using namespace BridgeProfile;
         com_ptr<ID3D11DeviceContext> context;
         if (current || state != State::Off)
            native_device->GetImmediateContext(&context);
         if (current)
         {
            current->gpu_end = Query(context.get(), D3D11_QUERY_TIMESTAMP);
            context->End(current->gpu_end.get());
            context->End(current->disjoint.get());
            current->cpu_end = Now();
            current = nullptr;
         }
         if (state == State::Requested)
         {
            state = State::Off;
            run_directory = (sweep_step >= 0 ? sweep_directory : OutputDirectory()) / std::format("{}-{}{}{}", device_data.sr_type == SR::Type::DLSS ? "dlss" : "fsr", TimeStamp(),
                                                                                         g_bridge_flush_before ? "-flush" : "", g_gpu_thread_priority != 0 ? std::format("-priority{}", g_gpu_thread_priority) : "");
            std::error_code error;
            std::filesystem::create_directories(run_directory, error);
            if (!IsSRActive(device_data))
               reshade::log::message(reshade::log::level::warning, "[ME1 Bridge] the profile needs DLSS or FSR 3 picked");
            else if (std::filesystem::is_directory(run_directory, error))
            {
               SetEnvironmentVariableW(L"LUMA_UPSCALER_PROFILE", (run_directory / "helper.csv").c_str());
               sr_implementations[device_data.sr_type]->ReleaseResources(device_data.GetSRInstanceData()); // The profile starts with the helper
               frames.clear();
               frames.reserve(kFrames); // "current" points into it
               warmup = 0;
               state = State::WarmingUp;
            }
         }
         else if (state == State::WarmingUp && IsSRActive(device_data) && sr_implementations[device_data.sr_type]->IsReady(device_data.GetSRInstanceData()) && ++warmup >= kWarmupFrames)
         {
            state = State::Recording;
         }
         if (state == State::Recording && frames.size() < kFrames)
         {
            auto& frame = frames.emplace_back();
            frame.cpu_begin = Now();
            frame.disjoint = Query(context.get(), D3D11_QUERY_TIMESTAMP_DISJOINT);
            context->Begin(frame.disjoint.get());
            frame.gpu_begin = Query(context.get(), D3D11_QUERY_TIMESTAMP);
            context->End(frame.gpu_begin.get());
            current = &frame;
         }
         else if (state == State::Recording)
         {
            // All recorded: read back (a one-time wait), write, then the helper restarts without the profile (it writes its
            // file's end on exit)
            const auto read = [&](ID3D11Query* query) -> uint64_t
            {
               uint64_t value = 0;
               while (query && context->GetData(query, &value, sizeof(value), 0) == S_FALSE)
                  Sleep(0);
               return value;
            };
            LARGE_INTEGER qpc_frequency;
            QueryPerformanceFrequency(&qpc_frequency);
            std::vector<double> frame_gpu_ms, bridge_gpu_ms, draw_cpu_ms;
            std::string csv = "frame,bridge_n,cpu_begin,cpu_end";
            for (int i = 0; i < kSteps; i++)
               csv += std::format(",cpu_step{}", i);
            csv += ",gpu_begin,gpu_loaded,gpu_end";
            for (int i = 0; i < kSteps; i++)
               csv += std::format(",gpu_step{}", i);
            csv += ",gpu_frequency,disjoint,qpc_frequency\n";
            for (size_t i = 0; i < frames.size(); i++)
            {
               const Frame& frame = frames[i];
               D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjoint = {};
               while (context->GetData(frame.disjoint.get(), &disjoint, sizeof(disjoint), 0) == S_FALSE)
                  Sleep(0);
               csv += std::format("{},{},{},{}", i, frame.bridge_n, frame.cpu_begin, frame.cpu_end);
               for (int j = 0; j < kSteps; j++)
                  csv += std::format(",{}", frame.cpu_step[j]);
               const uint64_t gpu_begin = read(frame.gpu_begin.get()), gpu_end = read(frame.gpu_end.get());
               csv += std::format(",{},{},{}", gpu_begin, gpu_begin, gpu_end); // No load span: "gpu_loaded" is the begin
               uint64_t gpu_steps[kSteps];
               for (int j = 0; j < kSteps; j++)
               {
                  gpu_steps[j] = read(frame.gpu_step[j].get());
                  csv += std::format(",{}", gpu_steps[j]);
               }
               csv += std::format(",{},{},{}\n", disjoint.Frequency, int(disjoint.Disjoint), qpc_frequency.QuadPart);
               if (frame.bridge_n && !disjoint.Disjoint && disjoint.Frequency)
               {
                  const double gpu_ms = 1000.0 / double(disjoint.Frequency), cpu_ms = 1000.0 / double(qpc_frequency.QuadPart);
                  frame_gpu_ms.push_back(double(gpu_end - gpu_begin) * gpu_ms);
                  bridge_gpu_ms.push_back(double(gpu_steps[kSteps - 1] - gpu_steps[0]) * gpu_ms);
                  draw_cpu_ms.push_back(double(frame.cpu_step[kSteps - 1] - frame.cpu_step[0]) * cpu_ms);
               }
            }
            std::ofstream(run_directory / "game.csv", std::ios::binary) << csv;
            SetEnvironmentVariableW(L"LUMA_UPSCALER_PROFILE", nullptr);
            if (auto* instance = device_data.GetSRInstanceData())
               sr_implementations[device_data.sr_type]->ReleaseResources(instance);
            frames.clear();
            state = State::Off;
            reshade::log::message(reshade::log::level::info, std::format("[ME1 Bridge] profile of {} frames written to \"{}\"", kFrames, run_directory.string()).c_str());
            if (sweep_step >= 0)
            {
               auto& results = sweep_results[sweep_step % std::size(kSweepModes)];
               results[0].push_back(Perf::Median(frame_gpu_ms));
               results[1].push_back(Perf::Median(bridge_gpu_ms));
               results[2].push_back(Perf::Median(draw_cpu_ms));
               if (!AdvanceBridgeProfileSweep(device_data))
               {
                  std::string summary;
                  for (size_t i = 0; i < std::size(kSweepModes); i++)
                  {
                     if (sweep_results[i][0].empty())
                        continue;
                     const std::string line = std::format("[ME1 Bridge] sweep mode=\"{}\" runs={} frame GPU median={:.3f} ms bridge GPU median={:.3f} ms bridge CPU in Draw median={:.3f} ms",
                        kSweepModes[i].name, sweep_results[i][0].size(), Perf::Median(sweep_results[i][0]), Perf::Median(sweep_results[i][1]), Perf::Median(sweep_results[i][2]));
                     reshade::log::message(reshade::log::level::info, line.c_str());
                     summary += line + "\n";
                  }
                  std::ofstream(sweep_directory / "sweep.txt", std::ios::binary) << summary;
                  SetSRType(device_data, sweep_user_sr_type);
                  sweep_step = -1;
               }
            }
         }
      }
#endif
      // "Memory Sweep": a mode settles for a fixed frame count after the upscaler is ready (lazy targets created, released ones
      // flushed), or logs anyway once the helper failed to get ready in time
      if (g_memory_sweep_step >= 0)
      {
         const bool not_ready = IsSRActive(device_data) && !sr_implementations[device_data.sr_type]->IsReady(device_data.GetSRInstanceData());
         if (not_ready && ++gd.memory_sweep_wait_frames < memory_sweep_max_wait_frames)
            gd.memory_sweep_settle_frames = memory_sweep_settle_frames;
         else if (--gd.memory_sweep_settle_frames <= 0)
         {
            LogMemoryReport(native_device, device_data, std::format("{}{}", memory_sweep_modes[g_memory_sweep_step].name, not_ready ? " (upscaler not ready)" : "").c_str());
            ApplyMemorySweepMode(device_data, g_memory_sweep_step + 1 < int(std::size(memory_sweep_modes)) ? g_memory_sweep_step + 1 : -1);
         }
      }
      // "Performance Test": closes this frame's timestamp set, reads back the finished ones (a log line every 120 frames, the first 30
      // after a settings change or a pause skipped), opens the next frame's
      com_ptr<ID3D11DeviceContext> perf_context;
      native_device->GetImmediateContext(&perf_context);
      gd.perf_timestamps.Close(perf_context.get());
      if (Perf::g_test != 0)
      {
         auto& window = gd.perf_window;
         const std::string aa = IsSRActive(device_data) ? (device_data.sr_type == SR::Type::DLSS ? "DLSS" : "FSR") : (g_mv_enable ? "MV only" : "none");
         const std::string settings = std::format("mode=\"{}\" hook_timers={} cpu_optimizations=0x{:02X} aa={} mask={} scale={:.2f} threshold={:.2f} tc={} output={}x{}", perf_test_modes[Perf::g_test].name, Perf::g_hook_timers, g_cpu_optimizations & ~g_perf_cpu_optimizations_off, aa, IsSRActive(device_data) && device_data.sr_type == SR::Type::FSR && g_sr_reactive_enable, g_sr_reactive_scale, g_sr_reactive_threshold, g_sr_tc_from_mask, uint32_t(device_data.output_resolution.x), uint32_t(device_data.output_resolution.y));
         // Also until the upscaler draws (the SR bridge's helper takes seconds to start, passing the color through meanwhile)
         const bool measuring = window.Present(settings, perf_settle_frames, IsSRActive(device_data) && !sr_implementations[device_data.sr_type]->IsReady(device_data.GetSRInstanceData()));
         auto& stats = window.stats;
         window.disjoint += gd.perf_timestamps.Collect(perf_context.get(), measuring, [&](const auto& frame)
            {
               stats.frame.Add(frame.Ms(Perf::FRAME_START, Perf::FRAME_END));
               if (!frame.Has(PERF_SCENE_START) || !frame.Has(PERF_SCENE_END))
                  return;
               stats.scene.Add(frame.Ms(PERF_SCENE_START, PERF_SCENE_END));
               if (!frame.Has(PERF_SCENE_TAIL_END))
                  return;
               stats.end.Add(frame.Ms(PERF_SCENE_END, PERF_SCENE_TAIL_END));
               if (frame.Has(PERF_FILL_END) && frame.Has(PERF_UPSCALER_END) && frame.Has(PERF_COPY_END))
               {
                  const size_t bounds[5] = {PERF_SCENE_END, PERF_FILL_END, PERF_UPSCALER_END, PERF_COPY_END, PERF_SCENE_TAIL_END};
                  for (int part = 0; part < 4; part++)
                     stats.end_parts[part].Add(frame.Ms(bounds[part], bounds[part + 1]));
               } });
         window.Finish(measuring, [&]
            {
            const double fill = stats.end_parts[0].Average(), upscaler = stats.end_parts[1].Average(), copy_back = stats.end_parts[2].Average(), scene_copy = stats.end_parts[3].Average();
            const double hooks = window.HookMs();
            reshade::log::message(reshade::log::level::info, std::format("[ME1 Perf] {} gpu frame avg/max={:.3f}/{:.3f} ms scene avg/max={:.3f}/{:.3f} ms ({}) end avg/max={:.3f}/{:.3f} ms ({}) = fill {:.3f} + upscaler {:.3f} + copy back {:.3f} + scene copy {:.3f} ms ({}) cpu frame avg={:.3f} ms cpu hooks={:.3f} ms/frame samples={}/{} disjoint={}",
                                                                settings, stats.frame.Average(), stats.frame.max_ms, stats.scene.Average(), stats.scene.max_ms, stats.scene.samples, stats.end.Average(), stats.end.max_ms, stats.end.samples, fill, upscaler, copy_back, scene_copy, stats.end_parts[0].samples, window.CpuFrameMs(), hooks, stats.frame.samples, window.frames, window.disjoint)
                                                                .c_str());
            // The row in "PerfColumn" order
            g_perf_sweep.OnWindow(Perf::g_test, {stats.frame.Average(), stats.scene.Average(), stats.end.Average(), hooks, window.CpuFrameMs(), fill, upscaler, copy_back, scene_copy}, [&](int mode_index)
               { ApplyPerfTestMode(device_data, mode_index); }, [&](int mode, int baseline_mode, double baseline)
               {
                     const double cpu_frame = g_perf_sweep.Median(mode, PERF_COLUMN_CPU_FRAME);
                     reshade::log::message(reshade::log::level::info, std::format("[ME1 Perf] sweep mode=\"{}\" hook_timers={} windows={} cpu frame median={:.3f} ms ({:.1f} fps) gpu frame median={:.3f} ms ({:+.3f} vs \"{}\") scene median={:.3f} ms end median={:.3f} ms (fill {:.3f} upscaler {:.3f} copy back {:.3f} scene copy {:.3f}) cpu hooks median={:.3f} ms/frame", perf_test_modes[mode].name, Perf::g_hook_timers, g_perf_sweep.results[mode].size(), cpu_frame, cpu_frame > 0.0 ? 1000.0 / cpu_frame : 0.0, g_perf_sweep.Median(mode, PERF_COLUMN_FRAME), g_perf_sweep.Median(mode, PERF_COLUMN_FRAME) - baseline, perf_test_modes[baseline_mode].name, g_perf_sweep.Median(mode, PERF_COLUMN_SCENE), g_perf_sweep.Median(mode, PERF_COLUMN_TAIL), g_perf_sweep.Median(mode, PERF_COLUMN_FILL), g_perf_sweep.Median(mode, PERF_COLUMN_UPSCALER), g_perf_sweep.Median(mode, PERF_COLUMN_COPY_BACK), g_perf_sweep.Median(mode, PERF_COLUMN_SCENE_COPY), g_perf_sweep.Median(mode, PERF_COLUMN_HOOKS)).c_str()); }); });
         gd.perf_timestamps.Open(native_device, perf_context.get());
      }
#endif
      // A scene no post pass ended ends here: its jitter must not reach the next frame's draws before its first mesh
      gd.mv_scene_open = false;
      gd.mv_scene_done = false;
      gd.mv_frame_ended = true;
      gd.mv_fill_pending = false;
      if (!custom_texture_mip_lod_bias_offset)
      {
         const std::unique_lock lock(s_mutex_samplers);
         // -1 at native resolution (Core biases the anisotropic samplers, all of the game's with the AF16x upgrade)
         device_data.texture_mip_lod_bias_offset = IsSRActive(device_data) ? SR::GetMipLODBias(device_data.output_resolution.y, device_data.output_resolution.y) : 0.f;
      }
      {
         // The pooled vc4 copies only the pool holds (superseded, no object or camera keeps them) are free for the next ones, as many
         // as the last frame asked for: frames without a scene (loading, videos, menus) still copy every Unmap, and would keep their
         // peak otherwise. Without motion vectors, none.
         const std::lock_guard lock(gd.mv_constants_mutex);
         size_t kept_free = gd.mv_active ? std::exchange(gd.mv_constants_made, 0) : 0;
         std::erase_if(gd.mv_constants_pool, [&](const auto& copy)
            {
               if (copy.use_count() != 1)
                  return false;
               if (kept_free == 0)
                  return true;
               kept_free--;
               return false; });
         gd.mv_constants_pool_free.clear();
         for (uint32_t i = 0; i < uint32_t(gd.mv_constants_pool.size()); i++)
         {
            if (gd.mv_constants_pool[i].use_count() == 1)
               gd.mv_constants_pool_free.push_back(i);
         }
#if DEVELOPMENT
         gd.mv_stats.constants_pool = uint32_t(gd.mv_constants_pool.size());
#endif
      }
#if DEVELOPMENT
      gd.mv_last_stats = std::exchange(gd.mv_stats, {});
      // The DEV panel's counts in ReShade.log every 300 frames while motion vectors run
      if (const auto& stats = gd.mv_last_stats; gd.mv_active && cb_luma_global_settings.FrameIndex % 300 == 0)
         reshade::log::message(reshade::log::level::info, std::format("[ME1 MV] frame {}: {} mv ({} matched, {} camera only, {} other camera, {} uncopied), {} jitter ({} reactive), {} maps, {} updates, {} other maps, {} offset bindings, sr {} ({}), near {:.3f} far {:.0f}, ended by 0x{:08X} (scene slot {}, copy {}), refused {}/{}/{}/{}/{}/{}/{}/{}, scene reads after end {} (last by 0x{:08X}, {} total)", cb_luma_global_settings.FrameIndex, stats.motion_vector_draws, stats.matched, stats.camera_only, stats.other_camera, stats.uncopied, stats.jitter_draws, stats.reactive_draws, stats.maps, stats.updates, stats.other_maps, stats.offset_bindings, stats.sr_draws, int(device_data.sr_type), stats.near_plane, stats.far_plane, stats.ended_by, stats.ended_by_scene_slot, gd.mv_scene_copy != nullptr, stats.rejected[0], stats.rejected[1], stats.rejected[2], stats.rejected[3], stats.rejected[4], stats.rejected[5], stats.rejected[6], stats.rejected[7], stats.scene_reads_after_end, stats.scene_reader, gd.mv_scene_reads_after_end).c_str());
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
         else if (g_sr_reactive_debug_view && gd.mv_reactive)
         {
            debug_draw_auto_clear_texture = false;
            debug_draw_options |= (uint32_t)DebugDrawTextureOptionsMask::RedOnly;
            debug_draw_options &= ~((uint32_t)DebugDrawTextureOptionsMask::Abs | (uint32_t)DebugDrawTextureOptionsMask::UVToPixelSpace);
            device_data.debug_draw_texture = gd.mv_reactive.get();
            device_data.debug_draw_texture_format = DXGI_FORMAT_R8_UNORM;
            device_data.debug_draw_texture_size = {uint32_t(device_data.output_resolution.x), uint32_t(device_data.output_resolution.y), 1, 1};
         }
         else if (device_data.debug_draw_texture && (device_data.debug_draw_texture.get() == gd.mv_texture.get() || device_data.debug_draw_texture.get() == gd.mv_reactive.get()))
         {
            device_data.debug_draw_texture = nullptr;
         }
      }
#endif

      gd.uber_ran_this_frame = false;
      gd.gamma_cb_ring.advanced_this_frame = false; // re-arm the once-per-frame ring advances
      gd.bloom_cb_ring.advanced_this_frame = false;

      // Publish the gamma pass's inverse display gamma to the grade (GameSettings is the only channel between passes).
      if (gd.gamma_inverse_valid && std::abs(cb_luma_global_settings.GameSettings.DisplayGammaInverse - gd.gamma_inverse_live) > 1e-4f)
      {
         cb_luma_global_settings.GameSettings.DisplayGammaInverse = gd.gamma_inverse_live;
         device_data.cb_luma_global_settings_dirty = true;
      }
#if ENABLE_BLOOM
      // Same for the gather's BloomScale, the gain that makes Bloom Intensity 1 vanilla strength.
      if (gd.bloom_scale_valid && std::abs(cb_luma_global_settings.GameSettings.BloomScaleLive - gd.bloom_scale_live) > 1e-5f)
      {
         cb_luma_global_settings.GameSettings.BloomScaleLive = gd.bloom_scale_live;
         device_data.cb_luma_global_settings_dirty = true;
      }
#endif

      gd.canvas_res.reset(); // do not hold a reference across frames: it would outlive a resize or a mirror swap
      // Core never clears this: left set, it would claim a tonemapped scene on movie and loading frames.
      gd.srv_scene.reset(); // recaptured every frame; never held across one

#if ENABLE_BLOOM
      // Give the memory back on the render thread: core's DrawKarisAverage output (~66 MB at 4K) and DrawBloom's mip chains
      // (~66 MB). Unconditional while off: resetting empty entries is a few lookups.
      if (!g_luma_bloom_enable)
      {
         ReleaseKarisAverage(device_data);
         ReleaseBloom();
      }
#endif

#if ENABLE_SMAA
      // Released here, not in the ImGui handler, so it happens on the render thread and never mid-frame. Also once SMAA stopped
      // running (the upscaler antialiases every frame it draws). RCAS on the upscaler keeps the snapshot.
      const bool smaa_idle = !g_smaa_enable || ++gd.smaa_idle_frames > smaa_idle_release_frames;
      if (smaa_idle)
      {
         // Unconditional while idle, as the bloom's above
         if (IsSRActive(device_data) && g_rcas_sharpness > 0.f)
         {
            gd.ReleasePredicationScratch();
            gd.ReleaseSharpenScratch();
         }
         else if (gd.tex_input)
         {
            gd.ReleaseSMAAScratch();
            gd.smaa_w = gd.smaa_h = 0; // core recreates lazily; keep the latch from claiming anything is current
         }
         ReleaseSMAA(device_data);
      }
      else
      {
         if (!g_smaa_predication && gd.tex_pred)
            gd.ReleasePredicationScratch();
         if (g_rcas_sharpness <= 0.f && gd.tex_smaa_out)
            gd.ReleaseSharpenScratch();
      }
#endif
   }

   void LoadConfigs() override
   {
      // Grade sliders (cb_luma_global_settings_dirty is already true at init -> uploaded on first frame).
      reshade::get_config_value(nullptr, NAME, "Exposure", cb_luma_global_settings.GameSettings.Exposure);
      reshade::get_config_value(nullptr, NAME, "Saturation", cb_luma_global_settings.GameSettings.Saturation);
      reshade::get_config_value(nullptr, NAME, "HighlightsDesaturation", cb_luma_global_settings.GameSettings.HighlightDechroma);
#if ENABLE_BLOOM
      // Inside the guard because it drives the Luma pyramid alone: with no pyramid there is nothing for it to scale.
      reshade::get_config_value(nullptr, NAME, "BloomIntensity", cb_luma_global_settings.GameSettings.BloomIntensity);
      reshade::get_config_value(nullptr, NAME, "LumaBloomEnable", g_luma_bloom_enable);
      cb_luma_global_settings.GameSettings.LumaBloomEnable = g_luma_bloom_enable ? 1.f : 0.f; // mirror to the shaders
      reshade::get_config_value(nullptr, NAME, "BloomThreshold", cb_luma_global_settings.GameSettings.BloomThreshold);
#endif
      reshade::get_config_value(nullptr, NAME, "Contrast", cb_luma_global_settings.GameSettings.Contrast);
      reshade::get_config_value(nullptr, NAME, "Dithering", cb_luma_global_settings.GameSettings.Dithering);
      reshade::get_config_value(nullptr, NAME, "VideoAutoHDREnable", cb_luma_global_settings.GameSettings.VideoAutoHDREnable);
      reshade::get_config_value(nullptr, NAME, "VideoAutoHDRBoost", cb_luma_global_settings.GameSettings.VideoAutoHDRBoost);
#if ENABLE_SMAA
      reshade::get_config_value(nullptr, NAME, "SMAAEnable", g_smaa_enable);
      reshade::get_config_value(nullptr, NAME, "SMAAPredication", g_smaa_predication);
      reshade::get_config_value(nullptr, NAME, "SMAAPredicationTolerance", g_smaa_pred_tolerance);
      reshade::get_config_value(nullptr, NAME, "RCASSharpness", g_rcas_sharpness);
#endif
   }

   void DrawImGuiSettings(DeviceData& device_data) override
   {
#if ENABLE_SMAA
      ImGui::SeparatorText("Anti-Aliasing");
      const bool sr_active = IsSRActive(device_data);
      // The upscaler (Super Resolution, in the Settings tab) replaces SMAA: shown off, the saved choice is kept
      ImGui::BeginDisabled(sr_active);
      bool smaa_shown = g_smaa_enable && !sr_active;
      if (ImGui::Checkbox("SMAA Enable", sr_active ? &smaa_shown : &g_smaa_enable))
         reshade::set_config_value(nullptr, NAME, "SMAAEnable", g_smaa_enable);
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
         ImGui::SetTooltip("Adds SMAA anti-aliasing (the game has none of its own; not used with DLSS/FSR).");
      ImGui::EndDisabled();
      if (g_smaa_enable && !sr_active)
      {
#if DEVELOPMENT
         // Not a preference: on geometry it relaxes the threshold back to base ULTRA, never below. A bisect switch.
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
            ImGui::SetTooltip("Log the mask's coverage above 0.5 plus percentiles to ReShade.log.\nReal silhouettes are ~1% of a typical frame; a working mask barely moves across a 20x tolerance sweep.\nStalls the GPU for one frame.");
#endif
      }
      if (g_smaa_enable || sr_active)
      {
         if (ImGui::SliderFloat("RCAS Sharpness", &g_rcas_sharpness, 0.f, 1.f))
            reshade::set_config_value(nullptr, NAME, "RCASSharpness", g_rcas_sharpness);
         if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Sharpening applied on top of SMAA or DLSS/FSR (0 = off).");
         DrawResetButton(g_rcas_sharpness, 0.f, "RCASSharpness"); // writes the config itself (Serialize defaults true)
      }
#endif

#if DEVELOPMENT
      ImGui::SeparatorText("Motion Vectors (DLSS/FSR)");
      const auto& stats = GetGameDeviceData(device_data).mv_last_stats;
      ImGui::Checkbox("MV Enable", &g_mv_enable);
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Draws the scene with the motion vector shaders without an upscaler: camera and object motion (each draw finds its own\nprevious frame vc4). The image must not change; the debug view is black with a static camera and lights up only\nwhat moves. ReShade.log: patched/refused shaders. Not saved.");
      ImGui::Checkbox("MV Force Jitter", &g_mv_force_jitter);
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Jitters the scene (Halton 2/3, 8 phases) without an upscaler, with MV Enable. The image shakes by a subpixel; nothing\nmay flicker or lose pixels, and the debug view stays black with a static camera. Not saved.");
      ImGui::Checkbox("MV Disable Jitter", &g_mv_disable_jitter);
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("No projection jitter under the upscaler (it gets zero jitter): isolates artifacts that come from the jitter. Not saved.");
      Perf::DrawCombo(perf_test_modes, &g_perf_sweep, [&](int mode_index)
         { ApplyPerfTestMode(device_data, mode_index); });
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Logs GPU and CPU times every 120 frames ([ME1 Perf] in ReShade.log): the frame, the scene, the end of the scene\n(fill with the reactive mask, the upscaler, copies) and the scene hooks' CPU time. The first 30 frames after a settings change are skipped.\nKeep the camera still; compare by toggling FSR and the mask. \"Sweep\" runs every mode, 3 rounds, then logs medians;\n\"CPU Sweep\" the CPU optimizations each off in turn (FSR 3, then no AA), the same way. Not saved.");
      if (ImGui::TreeNode("CPU Optimizations"))
      {
         static constexpr std::pair<uint32_t, const char*> optimizations[] = {{kCpuVc4Filter, "vc4 Filter"}, {kCpuVc4Pool, "vc4 Pool"}, {kCpuBlendMemo, "Blend Memo"}};
         for (const auto& [bit, name] : optimizations)
            ImGui::CheckboxFlags(name, &g_cpu_optimizations, bit);
         if (ImGui::IsItemHovered())
            ImGui::SetTooltip("The hooks' CPU optimizations (always on in Publishing); a \"Performance Test\" mode can turn some off on top. Not saved.");
         ImGui::TreePop();
      }
      ImGui::Checkbox("Hook Timers", &Perf::g_hook_timers);
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Times the motion vector draw and buffer hooks for \"cpu hooks\" (two clock reads each, thousands a frame).\nRun a Sweep with it off to see their own cost in the frame times. The test also turns off the per draw diagnostics.");
      const std::string memory_sweep_label = g_memory_sweep_step >= 0 ? std::format("Memory Sweep ({}/{})", g_memory_sweep_step + 1, std::size(memory_sweep_modes)) : std::string("Memory Sweep");
      if (ImGui::Button(memory_sweep_label.c_str()) && g_memory_sweep_step < 0 && Perf::g_test == 0)
      {
         LogMemoryReport(device_data.native_device, device_data, "Current Settings");
         ApplyMemorySweepMode(device_data, 0);
      }
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Runs each mode (no AA, SMAA, bloom, RCAS, DLSS with and without SMAA, FSR 3 with the mask, back to none) and logs the\nprocess's GPU memory, commit and address space once each settles ([ME1 Mem] in ReShade.log), then restores the settings.\nKeep the camera still. Not with a Performance Test. Not saved.");
#if ENABLE_SR_BRIDGE
      if (ImGui::Button(BridgeProfile::state == BridgeProfile::State::Off ? "SR Bridge Profile" : "SR Bridge Profile (running)") && BridgeProfile::state == BridgeProfile::State::Off)
         BridgeProfile::state = BridgeProfile::State::Requested;
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("With DLSS or FSR 3 picked: restarts the helper with its profile, then records %u upscaled frames of the bridge's steps\n(GPU timestamps and CPU times) into \"LumaBridgeProfile\\<upscaler>-<time>\" next to the exe: game.csv and helper.csv.\nThe helper restarts again at the end. Keep the camera still.", BridgeProfile::kFrames);
      {
         using namespace BridgeProfile;
         const std::string sweep_label = sweep_step >= 0 ? std::format("SR Bridge Profile Sweep ({}/{})", sweep_step + 1, kSweepSteps) : std::string("SR Bridge Profile Sweep");
         if (ImGui::Button(sweep_label.c_str()) && sweep_step < 0 && state == State::Off)
         {
            sweep_user_sr_type = device_data.sr_type;
            for (auto& results : sweep_results)
               for (auto& column : results)
                  column.clear();
            sweep_directory = OutputDirectory() / ("sweep-" + TimeStamp());
            sweep_step = -1;
            if (!AdvanceBridgeProfileSweep(device_data))
               sweep_step = -1;
         }
         if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Profiles DLSS and FSR 3, %d rounds interleaved\n(~10 s a run), into \"LumaBridgeProfile\\sweep-<time>\", then logs \"[ME1 Bridge] sweep\" lines (medians) and restores\nthe upscaler. Keep the camera still and the overlay closed.", kSweepRounds);
      }
      ImGui::Checkbox("SR Bridge Flush Before", &g_bridge_flush_before);
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Flushes the game's commands right before the SR bridge's frame, so its input copies and signal to the helper go on\ntheir own. An experiment on the handoff's latency (profile with and without). Not saved.");
      ImGui::SliderInt("GPU Thread Priority", &g_gpu_thread_priority, -7, 7);
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("IDXGIDevice::SetGPUThreadPriority on the game's device (0 = default). An experiment on the SR bridge's handoff. Not saved.");
#endif
      if (ImGui::Button("MV Dump Scene Draws"))
         g_mv_dump_scene = true;
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Logs one frame of the scene's draws ([ME1 DUMP] in ReShade.log): route, target, blend and depth state.");
      ImGui::Checkbox("FSR Reactive Mask", &g_sr_reactive_enable);
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Marks the pixels alpha blended draws drew, so FSR trusts their history less: all of them as reactive (over the\nthreshold), the non-additive ones (smoke, glass, water) also as transparency & composition. Not saved.");
      ImGui::SliderFloat("FSR Reactive Scale", &g_sr_reactive_scale, 0.f, 4.f);
      ImGui::SliderFloat("FSR Reactive Threshold", &g_sr_reactive_threshold, 0.f, 1.f);
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Scaled reactivity under it is 0, over it 0.9 (AMD's binary mask; AMD 0.2, default 0.5: lower makes static glows shake). 0: the scaled reactivity itself.");
      ImGui::Checkbox("FSR Reactive Debug View", &g_sr_reactive_debug_view);
      ImGui::Checkbox("FSR T&C From Mask", &g_sr_tc_from_mask);
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Passes the reactive mask as FSR's transparency & composition mask too, instead of the alpha blended draws' own. Not saved.");
      ImGui::Checkbox("FSR Reactive Zero Test", &g_sr_reactive_zero_test);
      ImGui::Checkbox("FSR Reactive Pass", &g_sr_reactive_pass);
      ImGui::Checkbox("FSR Reactive Skip Fill", &g_sr_reactive_skip_fill);
      ImGui::Checkbox("MV Debug View", &g_mv_debug_view);
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Shows the motion vector target (absolute, in pixels) through Core's debug draw.");
      ImGui::Text("Last frame: %u motion vector draws (%u matched, %u camera only, %u other camera, %u uncopied), %u jitter draws", stats.motion_vector_draws, stats.matched, stats.camera_only, stats.other_camera, stats.uncopied, stats.jitter_draws);
      ImGui::Text("vc4 copies: %u maps, %u updates, %u other maps, %u offset bindings; upscaler draws %u (near %.3f, far %.0f), scene ended by 0x%08X", stats.maps, stats.updates, stats.other_maps, stats.offset_bindings, stats.sr_draws, double(stats.near_plane), double(stats.far_plane), stats.ended_by);
      ImGui::Text("Refused: %u extra target, %u no scene, %u other depth/color, %u format, %u size, %u create, %u blend, %u shaders", stats.rejected[0], stats.rejected[1], stats.rejected[2], stats.rejected[3], stats.rejected[4], stats.rejected[5], stats.rejected[6], stats.rejected[7]);
      ImGui::Text("Last checked target: format %u, dimension %u, %ux%u (output %.0fx%.0f)", stats.rejected_format, stats.rejected_dimension, stats.rejected_width, stats.rejected_height, double(device_data.output_resolution.x), double(device_data.output_resolution.y));
#endif

      // --- Grade (read in Luma_ME1_Tonemap.hlsl; which fields reach the vanilla SDR path: see "LumaGameSettings") ---
      auto& gs = cb_luma_global_settings.GameSettings;
      ImGui::SeparatorText("Grade");

      if (ImGui::SliderFloat("Exposure", &gs.Exposure, 0.f, 2.f))
      {
         reshade::set_config_value(nullptr, NAME, "Exposure", gs.Exposure);
         device_data.cb_luma_global_settings_dirty = true;
      }
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Overall image brightness (1 = vanilla).");
      if (DrawResetButton(gs.Exposure, default_luma_global_game_settings.Exposure, "Exposure"))
         device_data.cb_luma_global_settings_dirty = true;

      if (ImGui::SliderFloat("Contrast", &gs.Contrast, 0.f, 2.f))
      {
         reshade::set_config_value(nullptr, NAME, "Contrast", gs.Contrast);
         device_data.cb_luma_global_settings_dirty = true;
      }
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Overall image contrast, HDR only (1 = vanilla).");
      if (DrawResetButton(gs.Contrast, default_luma_global_game_settings.Contrast, "Contrast"))
         device_data.cb_luma_global_settings_dirty = true;

      if (ImGui::SliderFloat("Saturation", &gs.Saturation, 0.f, 2.f))
      {
         reshade::set_config_value(nullptr, NAME, "Saturation", gs.Saturation);
         device_data.cb_luma_global_settings_dirty = true;
      }
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Color saturation, HDR only (1 = vanilla).");
      if (DrawResetButton(gs.Saturation, default_luma_global_game_settings.Saturation, "Saturation"))
         device_data.cb_luma_global_settings_dirty = true;

      if (ImGui::SliderFloat("Highlights Desaturation", &gs.HighlightDechroma, 0.f, 1.f))
      {
         reshade::set_config_value(nullptr, NAME, "HighlightsDesaturation", gs.HighlightDechroma);
         device_data.cb_luma_global_settings_dirty = true;
      }
      if (ImGui::IsItemHovered())
         // "How far", not "how soon": DICE's ramp starts at a third of peak, so the slider sets depth, not onset.
         ImGui::SetTooltip("How far the brightest sources fade to neutral white, HDR only (0 = keep color at any brightness).");
      if (DrawResetButton(gs.HighlightDechroma, default_luma_global_game_settings.HighlightDechroma, "HighlightsDesaturation"))
         device_data.cb_luma_global_settings_dirty = true;

#if DEVELOPMENT
      if (ImGui::Button("Dump Pass Constants"))
         g_dump_pass_cb = true;
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Log the copy/uber/gamma passes' cb4 grade rows for the next frame to ReShade.log.\nMeasured 1.0 / 1.0 / 0.625 - press it on a gameplay frame, not on a loading fade.\nEach dump blocks on a GPU read.");
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

      // Everything below drives the Luma pyramid and greys out with it: nothing here reaches the game's own glow.
      ImGui::BeginDisabled(!g_luma_bloom_enable);

      if (ImGui::SliderFloat("Bloom Intensity", &gs.BloomIntensity, 0.f, 2.f))
      {
         reshade::set_config_value(nullptr, NAME, "BloomIntensity", gs.BloomIntensity);
         device_data.cb_luma_global_settings_dirty = true;
      }
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
         ImGui::SetTooltip("Bloom strength (1 = vanilla, 0 = none).");
      if (DrawResetButton(gs.BloomIntensity, default_luma_global_game_settings.BloomIntensity, "BloomIntensity"))
         device_data.cb_luma_global_settings_dirty = true;

#if DEVELOPMENT
      if (ImGui::SliderFloat("Bloom Threshold", &gs.BloomThreshold, 0.f, 4.f, "%.2f"))
      {
         reshade::set_config_value(nullptr, NAME, "BloomThreshold", gs.BloomThreshold);
         device_data.cb_luma_global_settings_dirty = true;
      }
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Linear scene brightness where bloom starts. 1.0 is where the game's own bright-pass sits.\nNear 0 the whole scene glows — that is the failure mode, not a setting.");
      if (DrawResetButton(gs.BloomThreshold, default_luma_global_game_settings.BloomThreshold, "BloomThreshold"))
         device_data.cb_luma_global_settings_dirty = true;

#endif // DEVELOPMENT
      ImGui::EndDisabled();
#endif // ENABLE_BLOOM

      ImGui::SeparatorText("Effects");
      // Read in Video_0x1A82565B.ps_5_0.hlsl. Inert in SDR: peak == paper white makes PumboAutoHDR an identity.
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
      {
         reshade::set_config_value(nullptr, NAME, "VideoAutoHDRBoost", gs.VideoAutoHDRBoost);
         device_data.cb_luma_global_settings_dirty = true;
      }
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
      ImGui::Checkbox("Hide Gameplay UI", &g_hide_ui);
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Disables the in-game UI.");
#if DEVELOPMENT
      ImGui::BeginDisabled(g_effect_runtime == nullptr);
      if (ImGui::Button("Take Screenshot"))
         g_effect_runtime->save_screenshot();
      ImGui::EndDisabled();
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
         ImGui::SetTooltip("Saves a screenshot through ReShade (its screenshot folder and format; HDR PNG in HDR). The Luma panel isn't captured.");
#endif
   }

   void PrintImGuiAbout() override
   {
      ImGui::PushTextWrapPos(0.f);
      ImGui::Text(
         "Luma for \"Mass Effect\" (2007) is developed by DristoforColumb and is open source and free.\n"
         "It adds HDR, DLAA or FSR 3 native anti-aliasing, HDR bloom and SMAA anti-aliasing, plus 16x anisotropic filtering.\n"
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
      const char* cleared_project_name = (project_name[0] == '_') ? (project_name + 1) : project_name;

      uint32_t mod_version = 1;
      Globals::SetGlobals(cleared_project_name, "Mass Effect (2007) Luma HDR mod", "", mod_version);
      Globals::DEVELOPMENT_STATE = Globals::ModDevelopmentState::Finished;

      // scRGB fp16 swapchain (the game's backbuffer is 8-bit).
      swapchain_format_upgrade_type = TextureFormatUpgradesType::AllowedEnabled;
      swapchain_upgrade_type = SwapchainUpgradeType::scRGB;
      // The brightness slider is a D3D9 SetGammaRamp that dgVoodoo forwards to the OS ramp, distorting scRGB.
      // The game's DisplayGamma 1.6 is already applied in the grade.
      allow_disabling_gamma_ramp = true;

      // force_borderless also covers LEAVING fullscreen, so alt-tab cannot restore a title bar (TW2).
      prevent_fullscreen_state = true;
      force_borderless = true;

      // Two HDR-clipping families, upgraded INDIRECTLY: changing a dgVoodoo creation format black-screens it (MEA).
      texture_format_upgrades_type = TextureFormatUpgradesType::AllowedEnabled;
      enable_indirect_texture_format_upgrades = true; // creation-time mirrors, substituted at bind (BL2/TW2 scheme)
      enable_chain_indirect_texture_format_upgrades = ChainTextureFormatUpgradesType::DirectDependencies;
      texture_upgrade_formats = {
         reshade::api::format::r8g8b8a8_typeless,     // dgVoodoo's D3D9 backbuffer surface (the LDR canvas)
         reshade::api::format::r16g16b16a16_typeless, // DoF/bloom gather, blur and blit targets, viewed as unorm
      };
      // "No1Px" is mandatory under dgVoodoo (BL2): 1x1 placeholders fill unused slots and pass the aspect filter.
      texture_format_upgrades_2d_size_filters = (uint32_t)TextureFormatUpgrades2DSizeFilters::SwapchainResolution | (uint32_t)TextureFormatUpgrades2DSizeFilters::SwapchainAspectRatio | (uint32_t)TextureFormatUpgrades2DSizeFilters::No1Px;

      // AF16x. force_upgrade_linear_samplers is load-bearing: otherwise core rewrites only anisotropic samplers (TW2).
      enable_samplers_upgrade = true; // boot-time only (cannot be changed after device creation)
      samplers_upgrade_mode = 4;
      force_upgrade_linear_samplers = true;

      game = new MassEffect();
   }
   else if (ul_reason_for_call == DLL_PROCESS_DETACH)
   {
      MassEffect::UnregisterEvents();
   }

   CoreMain(hModule, ul_reason_for_call, lpReserved);

   return TRUE;
}
