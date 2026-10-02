// The Witcher 2: Assassins of Kings Enhanced Edition — Luma HDR mod (REDengine, 32-bit, DX9 -> D3D11 via dgVoodoo2).
//
// Hashes are the dgVoodoo-TRANSLATED ones and change with every wrapper build: 2.87.3 and 2.81.3 are keyed
// (2.81.3 emits ps_4_0 for the same passes), any other build needs a re-dump.
// The post chain is fp16 throughout and linear until the final grade's vMidtone power encodes it; the "tonemap" is
// an adaptive exposure multiply, no curve or clamp. The UI blends src-alpha onto the graded gamma canvas; the main
// menu draws no tonemap at all.
// No motion vectors of its own: DLAA / FSR 3 Native AA get them from patched shaders (see MotionVectorPatches.h), and run in Core's
// SR bridge helper (NGX is x64-only).
// Only ONE Luma .addon, and no other swapchain-hooking ReShade addon: they crash through dgVoodoo.

// The MessageBox is invisible under a borderless/fullscreen game and blocks the loader -> ReShade error 1114.
#define DISABLE_AUTO_DEBUGGER 1

#define GAME_THE_WITCHER_2 1

#define GEOMETRY_SHADER_SUPPORT 0

#define ENABLE_SMAA 1 // replaces the final grade's built-in FXAA with SMAA ULTRA (+RCAS); core registers the 6 "SMAA ..." passes
// SMAA runs POST-final-grade via the post-draw callback, so it needs original_draw_dispatch_func non-null.
#define ENABLE_POST_DRAW_DISPATCH_CALLBACK 1
// The motion vector draw key reads the draw's arguments ("last_draw_dispatch_data")
#define ENABLE_DRAW_DISPATCH_DATA_CACHE 1

// No Luma bloom: the engine already draws a thresholdless glow around every light, so a pyramid on top would re-bloom what the
// canvas contains.

#include "..\..\Core\core.hpp"
#include "..\..\External\WDK\includes\d3d11TokenizedProgramFormat.hpp"
#include "MotionVectorPatches.h"
#include "..\..\Core\includes\patched_draws.h"
#include "..\..\External\reshade\deps\minhook\include\MinHook.h"
#if DEVELOPMENT
#include "..\..\Core\includes\perf_test.h"
#endif

// Every pass is keyed under both dgVoodoo builds: 2.87.3, and 2.81.3 (the build that runs under Proton), which emits
// ps_4_0 and therefore different hashes. Dump-verified as signature-identical: same interpolators, t/s registers and cb
// slots, so the replacements and the slot-based captures are shared.
struct DgVoodooHashes
{
   uint32_t v2873;
   uint32_t v2813;
};

// Tonemap ("exposure") permutations.
static constexpr DgVoodooHashes TONEMAP_EXPOSURE = {0x91348C0F, 0x6CF3E8B7};    // DX9 0xC5ADBC35: exposure+scale, alpha passthrough
static constexpr DgVoodooHashes TONEMAP_BRIGHT_PASS = {0x00E31BF9, 0xB293C5B1}; // DX9 0xF01A691E: bloom bright-pass (threshold ramp, saturation, colour)
// The two static permutations (levels from PSC_LumRanges: the engine's software path, dead on dgVoodoo, which blends R32F; whether
// a frozen adaptation cutscene picks them is open) were never captured in game: their hashes come from running the cache's D3D9 blobs through both dgVoodoo builds offline,
// which reproduced every keyed hash above.
static constexpr DgVoodooHashes TONEMAP_EXPOSURE_STATIC = {0xC47569A2, 0xA98DDDFB};    // DX9 3366c0e4 (cache md5)
static constexpr DgVoodooHashes TONEMAP_BRIGHT_PASS_STATIC = {0x6587B8D6, 0x85FCD33B}; // DX9 c46227f6 (cache md5)
// Final grade (FXAA + colour balance + split toning + vignette), last pass before UI; hosts the HDR block and the SMAA hook.
static constexpr DgVoodooHashes FINAL_GRADE = {0xDE5CF9CD, 0x517DC6D5};
static constexpr DgVoodooHashes FINAL_GRADE_NO_AA = {0xCF3B72A9, 0xBBFEC706};       // game AA off: no FXAA block, scene alpha passed through
static constexpr DgVoodooHashes FINAL_GRADE_NO_VIGNETTE = {0xBABBFFAD, 0x2CA0631E}; // no FXAA and no vignette
// FXAA on, vignette off: the fourth corner of the 2x2 permutation matrix.
static constexpr DgVoodooHashes FINAL_GRADE_AA_NO_VIGNETTE = {0x058E2498, 0xA966D512};
// Native SSAO generator (HBAO variant, VS 0x5D9D0449): half-res r32_float LINEAR view depth at t0 -> half-res
// r8g8b8a8 (.x = AO, .y = viewZ). Only this draw is replaced; the vanilla chain downstream reads just .x:
// pack 0x953119B5 -> ping-pong 0xC131C40D x2 -> blur 0xD01CBD13 x2 -> apply 0x5C63E1C2.
// Needs SSAO on in the game's video settings.
static constexpr DgVoodooHashes AO_GEN = {0x3FEEC0F7, 0x6EC596CA};
// AO pack (t0 = full-res r32_float LINEAR depth, t1 = the AO target): depth-capture fallback for SMAA
// predication, since it runs every frame while the tonemap capture only fires on the BRIGHT-PASS perm.
static constexpr DgVoodooHashes AO_PACK = {0x953119B5, 0x495E9133};

// The engine's glow chain (halo around candles and torches, distinct from the god rays): copy 0x5A8E5532 and the
// 12-tap blur 0x88C500CF x2 stay vanilla; the screen blend 0x12931281 is replaced (LightShaftBlend_0x12931281).

// User settings, persisted in the [Luma] config section (LoadConfigs) unless noted otherwise.
static bool g_smaa_enable = true;
static float g_rcas_sharpness = 0.f;   // RCAS after any anti-aliasing, in place of the game's sharpen (0 = off)
static bool g_smaa_predication = true; // SMAA depth predication (r32f depth captured at the bright-pass tonemap or the AO pack pass)
// Plane deviation counted as a full edge, as a fraction of view depth, so it is resolution independent.
// 0.02 = ~14 cm at the measured 7 m median depth; XeGTAO uses 0.011 and ASSAO 0.040 for the same test.
static float g_smaa_pred_tolerance = 0.02f;
static bool g_gtao_enable = true; // XeGTAO replaces the native SSAO generator (AO_GEN)
static bool g_hide_ui = false;    // hide the game's HUD (for clean screenshots); session-only, never persisted

// XeGTAO knobs CB slot, must match "register(b9)" in Luma_TW2_XeGTAO.hlsl. Not b11: core's DrawBloom owns
// that slot for its own constants.
static constexpr UINT GTAO_KNOBS_CB_SLOT = 9;
// XeGTAO calibration knobs (DEV sliders). DepthScale and RadiusOverride ship at their calibrated values;
// FinalValuePower does NOT - 2.2 is a preference, while 1.0 is the value whose AO histogram matches the
// game's own HBAO (mean 0.90 against native 0.89).
static float g_gtao_final_value_power = 2.2f; // primary darkness dial (higher = darker)
static float g_gtao_depth_scale = 1.f;        // viewZ divisor (game units -> ~meters); dial against broad over-occlusion
static float g_gtao_radius_override = 0.f;    // > 0 overrides the shader's EFFECT_RADIUS (view units after DepthScale)
#if DEVELOPMENT
static int g_gtao_debug_view = 0; // 0=off 1=depth gradient 2=normals 3=AO x8 4=edges (the shader's debug blocks are DEVELOPMENT-only too)
#endif

// DLAA / FSR 3 Native AA (Mass Effect 2007's motion vector path; vc4 layout in "MotionVectorPatches")
#if DEVELOPMENT
static float g_sr_reactive_scale = 1.f;      // FSR reactive mask: the alpha blended draws' reactivity, scaled (AMD's default 1)
static float g_sr_reactive_threshold = 0.5f; // Under it 0, over it 0.9 (AMD's 0.2; Mass Effect's 0.5); 0: the scaled reactivity, capped at 0.9
static bool g_sr_reactive_enable = true;
static bool g_sr_reactive_debug_view = false;
static bool g_mv_enable = false;
static bool g_mv_debug_view = false;
static bool g_mv_force_jitter = false;        // The projection jitter without an upscaler
static bool g_mv_disable_jitter = false;      // No projection jitter under the upscaler (A/B of jitter-dependent artifacts)
static bool g_mv_untransformed_static = true; // Draws without a tie-break transform take no other draw's previous frame (see "DrawWithMotionVectors")
// A/B of the CPU savings (see "MayBeRegisteredBuffer", "NewConstantsCopy")
static bool g_mv_buffer_filter = true;
static bool g_mv_constants_pool = true;
static bool g_mv_vc4_slots = true; // The filtered vc4 buffers' Map/Unmap by their filter slot (see "OnMapBufferRegion")
// "Performance Test" (see "OnPresent"): GPU timestamps and hook CPU time to ReShade.log ("[TW2 Perf]"). A mode ("Perf::g_test") sets
// the anti-aliasing, or turns one of the CPU savings above off, while it runs (the user's values come back on leaving it, never saved).
// Borderlands 2's harness.
struct PerfTestMode
{
   const char* name;
   bool set_aa = false; // Else the current anti-aliasing (the next two fields unused)
   SR::Type sr_type = SR::Type::None;
   bool smaa = false;
   int motion_vector_draws = 2; // 2 patched (motion vectors and jitter), 1 jitter only, 0 untouched (unjittered)
   // One CPU saving off, the others as the user set them
   bool vc4_filter_off = false;
   bool vc4_pool_off = false;
   bool vc4_slots_off = false;
};
constexpr PerfTestMode PERF_TEST_MODES[] = {
   {.name = "Off"},
   {.name = "Current Settings"},
   {.name = "DLSS", .set_aa = true, .sr_type = SR::Type::DLSS},
   {.name = "DLSS Jitter Only", .set_aa = true, .sr_type = SR::Type::DLSS, .motion_vector_draws = 1},
   {.name = "DLSS Without Motion Vector Draws", .set_aa = true, .sr_type = SR::Type::DLSS, .motion_vector_draws = 0},
   {.name = "FSR 3", .set_aa = true, .sr_type = SR::Type::FSR},
   {.name = "SMAA", .set_aa = true, .smaa = true},
   {.name = "No AA", .set_aa = true},
   {.name = "Without vc4 Filter", .vc4_filter_off = true},
   {.name = "Without vc4 Pool", .vc4_pool_off = true},
   {.name = "Without vc4 Slots", .vc4_slots_off = true},
};
// "Sweep": these modes in turn, a log window each, over several rounds (interleaved, so the scene's drift averages out), then a median
// per mode against the last one. "CPU Sweep": the CPU savings each off in turn under the current anti-aliasing (DLSS or FSR: without
// motion vectors the buffer hooks return early), against "Current Settings".
constexpr int PERF_SWEEP_MODES[] = {2, 3, 4, 5, 6, 7};
constexpr int PERF_CPU_SWEEP_MODES[] = {8, 9, 10, 1};
static_assert(std::string_view(PERF_TEST_MODES[PERF_SWEEP_MODES[std::size(PERF_SWEEP_MODES) - 1]].name) == "No AA");
static_assert(std::string_view(PERF_TEST_MODES[PERF_CPU_SWEEP_MODES[std::size(PERF_CPU_SWEEP_MODES) - 1]].name) == "Current Settings");
constexpr Perf::SweepDef PERF_SWEEPS[] = {{"Sweep", PERF_SWEEP_MODES}, {"CPU Sweep", PERF_CPU_SWEEP_MODES}};
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
   PERF_COLUMN_COUNT
};
static Perf::Sweep<PERF_COLUMN_COUNT> g_perf_sweep = {.defs = PERF_SWEEPS, .rounds = 3, .windows = 1};
constexpr int PERF_SETTLE_FRAMES = 30; // Skipped after a settings change (history reset, targets rebuilt) and the upscaler being ready
// Skipped after leaving the upscaler: the SR bridge's helper exits on its own time, and its GPU context slows the frame while it does
// (Borderlands 2)
constexpr int PERF_HELPER_EXIT_SETTLE_FRAMES = 600;
// The scene from its opening to its end (the exposure), then the end's parts: the fill, the upscaler, its copy back
enum PerfStamp : size_t
{
   PERF_SCENE_START = Perf::FIRST_GAME_STAMP,
   PERF_SCENE_END,
   PERF_FILL_END,
   PERF_UPSCALER_END,
   PERF_SCENE_TAIL_END,
   PERF_STAMP_COUNT
};
static int GetPerfMotionVectorDraws()
{
   return PERF_TEST_MODES[Perf::g_test].motion_vector_draws;
}
#else
static constexpr float g_sr_reactive_scale = 1.f;
static constexpr float g_sr_reactive_threshold = 0.5f;
static constexpr bool g_sr_reactive_enable = true;
static constexpr bool g_mv_enable = false;
static constexpr bool g_mv_force_jitter = false;
static constexpr bool g_mv_disable_jitter = false;
static constexpr bool g_mv_untransformed_static = true;
static constexpr bool g_mv_buffer_filter = true;
static constexpr bool g_mv_constants_pool = true;
static constexpr bool g_mv_vc4_slots = true;
static constexpr int GetPerfMotionVectorDraws()
{
   return 2;
}
#endif

// Render scale for DLSS/FSR (NOTES.md "Engine render targets and UberSampling"). REDengine renders its scene into the top-left
// "render area" of full size surfaces (the viewport's +0xc/+0x10, copied into each frame at its build). A 3D frame's scene renders
// into an area shrunk to the scale; at the post chain's entry the area goes back to the window size for every post pass, the grade,
// the UI and the present blit, and the upscaler turns the shrunk scene into the whole surface there, before any post pass (light
// shafts, motion blur, DoF, the bright-pass copy, flares, luminance) reads it ("ResolveRenderArea"). DLAA / FSR Native AA runs there
// too (DLSS-Best-Practices PLC-1: none of the post chain baked into the history). Steam and GOG exes (addresses): other builds upscale
// at the exposure, after the post chain's first passes, and have no render scale.
static float g_render_scale = 1.f;
constexpr float MIN_RENDER_SCALE = 0.5f;
namespace RenderArea
{
   // A supported executable: its link time and the addresses the hooks use, as virtual addresses at the preferred base (the GOG exe
   // has ASLR). Steam's come from a memory dump (SteamStub); GOG's were matched to them by code (NOTES.md "GOG build").
   struct Executable
   {
      DWORD time_date_stamp;
      uint32_t renderer_global;        // The renderer (its viewports at +4, their count at +8)
      uint32_t render_settings_global; // The render settings ("UberSampling" at +0x4c)
      // A 3D frame's scene (__stdcall(int, frame info*), the frame info's first field is the CRenderFrame); the frame driver runs the
      // post chain after it
      uint32_t frame_scene;
      // The post chain (__stdcall, 6 arguments): reads the viewport's area (argument 5) and the frame info's (argument 3, +0x2a8) at
      // its entry, and keeps them for every pass. Its input is the scene color surface (slot 0) the scene render last drew into.
      uint32_t post_chain;
      // The engine's current D3DVIEWPORT9's Width and Height (Steam: 0x237e988 + 8, applied by FUN_0052f030): post passes (motion blur
      // "004ab9ce", radial blur "004a8efb", "004a7f37", "004a4d2d") derive their VS UV scale (c180) and sample clamp (motion blur c56)
      // from it at their start, before they set their own viewport. The scene leaves its area there.
      uint32_t current_viewport_size;
      std::array<uint8_t, 8> frame_scene_prologue;
      std::array<uint8_t, 8> post_chain_prologue;
   };
   constexpr uint32_t PREFERRED_BASE = 0x400000;
   // MSVC's aligned stack frame (push ebx; mov ebx, esp; ...)
   constexpr std::array<uint8_t, 8> ALIGNED_FRAME = {0x53, 0x8B, 0xDC, 0x51, 0x51, 0x83, 0xE4, 0xF0};
   constexpr Executable EXECUTABLES[] = {
      // Steam (2013)
      {.time_date_stamp = 0x518B881C, .renderer_global = 0x160C6EC, .render_settings_global = 0x2580E28, .frame_scene = 0xB14859, .post_chain = 0x9D7B67, .current_viewport_size = 0x237E990, .frame_scene_prologue = ALIGNED_FRAME, .post_chain_prologue = ALIGNED_FRAME},
      // GOG (2023 rebuild: the post chain's frame is ebp based, same arguments and "ret 0x18")
      {.time_date_stamp = 0x644689F9, .renderer_global = 0x167D4DC, .render_settings_global = 0x25EAEB8, .frame_scene = 0xB4DD26, .post_chain = 0xA28F79, .current_viewport_size = 0x23E8A20, .frame_scene_prologue = ALIGNED_FRAME, .post_chain_prologue = {0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF0, 0x81, 0xEC}},
   };
   constexpr size_t UBER_SAMPLING_OFFSET = 0x4C; // Render settings: "UberSampling", the scene rendered N x N times when > 1
   constexpr size_t VIEWPORT_AREA_OFFSET = 0xC;  // CRenderViewport: the area's width and height, then the window's at +0x1c/+0x20
   constexpr size_t FRAME_AREA_OFFSET = 0x2B8;   // CRenderFrame: its passes' area (the frame info's +0x2a8)
   constexpr size_t FRAME_INFO_AREA_OFFSET = 0x2A8;

   // The running executable's ("Install")
   uint8_t* const* renderer_global = nullptr;
   const uint8_t* const* render_settings_global = nullptr;
   uint8_t* frame_scene = nullptr;
   uint8_t* post_chain = nullptr;
   uint32_t* current_viewport_size = nullptr;

   bool installed = false;       // The post chain hook
   bool scale_installed = false; // The render scale's frame hook
   // On the game thread, as dgVoodoo's D3D11 calls and the present
   float next_scale = 1.f;                   // The next 3D frame's scale ("OnPresent")
   std::array<uint32_t, 2> render_size = {}; // This frame's shrunk area, 0 when not shrunk
   void (*resolve)() = nullptr;              // The upscaler, at the post chain's entry ("ResolveRenderArea")
   void(__stdcall* frame_scene_original)(int, uint8_t**) = nullptr;
   void(__stdcall* post_chain_original)(void*, int, uint8_t*, int, uint8_t*, uint32_t) = nullptr;

   // The game's (single) viewport's area, then its window size at [4] and [5], or null
   uint32_t* ViewportArea()
   {
      const uint8_t* const renderer = *renderer_global;
      if (!renderer || *reinterpret_cast<const uint32_t*>(renderer + 8) == 0)
         return nullptr;
      uint8_t* const viewport = **reinterpret_cast<uint8_t* const* const*>(renderer + 4);
      return viewport ? reinterpret_cast<uint32_t*>(viewport + VIEWPORT_AREA_OFFSET) : nullptr;
   }

   void __stdcall FrameSceneDetour(int renderer_part, uint8_t** frame_info)
   {
      render_size = {};
      uint32_t* const area = (next_scale < 1.f ? ViewportArea() : nullptr);
      const uint8_t* const render_settings = *render_settings_global;
      if (area && *frame_info && render_settings && *reinterpret_cast<const uint32_t*>(render_settings + UBER_SAMPLING_OFFSET) <= 1)
      {
         render_size = {(std::max)(uint32_t(float(area[4]) * next_scale + 0.5f), 1u), (std::max)(uint32_t(float(area[5]) * next_scale + 0.5f), 1u)};
         std::memcpy(area, render_size.data(), sizeof(render_size));
         std::memcpy(*frame_info + FRAME_AREA_OFFSET, render_size.data(), sizeof(render_size));
      }
      frame_scene_original(renderer_part, frame_info);
      if (render_size[0] != 0)
      {
         // The window size again, for the present blit (the frame's area stays shrunk until the post chain)
         area[0] = area[4];
         area[1] = area[5];
      }
   }

   void __stdcall PostChainDetour(void* renderer_part, int a2, uint8_t* frame_info, int a4, uint8_t* viewport, uint32_t flags)
   {
      if (render_size[0] != 0 && viewport && frame_info)
      {
         uint32_t* const area = reinterpret_cast<uint32_t*>(viewport + VIEWPORT_AREA_OFFSET);
         area[0] = area[4];
         area[1] = area[5];
         std::memcpy(frame_info + FRAME_INFO_AREA_OFFSET, area, sizeof(render_size));
         std::memcpy(current_viewport_size, area, sizeof(render_size));
      }
      // dgVoodoo translates synchronously: the scene's draws are in the D3D11 context already. UberSampling renders the scene N x N
      // times: it keeps the exposure's upscaler.
      const uint8_t* const render_settings = *render_settings_global;
      if (resolve && render_settings && *reinterpret_cast<const uint32_t*>(render_settings + UBER_SAMPLING_OFFSET) <= 1)
      {
         resolve();
      }
      post_chain_original(renderer_part, a2, frame_info, a4, viewport, flags);
   }

   void Install()
   {
      uint8_t* const image = reinterpret_cast<uint8_t*>(GetModuleHandleW(nullptr));
      const auto* const dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(image);
      const auto* const nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(image + dos->e_lfanew);
      const auto executable = std::find_if(std::begin(EXECUTABLES), std::end(EXECUTABLES), [nt](const Executable& candidate)
         { return candidate.time_date_stamp == nt->FileHeader.TimeDateStamp; });
      if (executable == std::end(EXECUTABLES))
         return;
      const auto at = [image](uint32_t address)
      { return image + (address - PREFERRED_BASE); };
      renderer_global = reinterpret_cast<uint8_t* const*>(at(executable->renderer_global));
      render_settings_global = reinterpret_cast<const uint8_t* const*>(at(executable->render_settings_global));
      frame_scene = at(executable->frame_scene);
      post_chain = at(executable->post_chain);
      current_viewport_size = reinterpret_cast<uint32_t*>(at(executable->current_viewport_size));
      if (std::memcmp(post_chain, executable->post_chain_prologue.data(), executable->post_chain_prologue.size()) != 0 || MH_Initialize() != MH_OK)
         return;
      installed = MH_CreateHook(post_chain, reinterpret_cast<void*>(&PostChainDetour), reinterpret_cast<void**>(&post_chain_original)) == MH_OK;
      scale_installed = installed && std::memcmp(frame_scene, executable->frame_scene_prologue.data(), executable->frame_scene_prologue.size()) == 0 &&
                        MH_CreateHook(frame_scene, reinterpret_cast<void*>(&FrameSceneDetour), reinterpret_cast<void**>(&frame_scene_original)) == MH_OK;
      installed = installed && MH_EnableHook(MH_ALL_HOOKS) == MH_OK;
      if (!installed)
      {
         scale_installed = false;
         MH_Uninitialize();
      }
   }
} // namespace RenderArea

// Optional resources (FSR's masks, SMAA's) go after this many presents without use: ~5 s, so menus and loading screens between
// frames that use them don't recreate them each time
constexpr uint32_t IDLE_RELEASE_FRAMES = 600;

// Why "DrawWithMotionVectors" refused a draw ("MV_REJECT", counted per reason in DEVELOPMENT)
enum class MotionVectorReject : uint8_t
{
   EXTRA_TARGET,  // The draw already binds the motion vector target's slot
   NO_SCENE,      // No scene depth taken yet
   OTHER_DEPTH,   // Another depth than the scene's
   ARRAY_OR_MSAA, // A target that isn't a single 2D slice
   SIZE,          // A target of another size than the output
   CREATE,        // The motion vector target couldn't be created
   BLEND,         // Blending into the G-buffer's targets
   SHADERS,       // No patched shader pair, or no jitter buffer
   COUNT
};
#if DEVELOPMENT
constexpr std::array<const char*, size_t(MotionVectorReject::COUNT)> MOTION_VECTOR_REJECT_NAMES = {"extra_target", "no_scene", "other_depth", "array_or_msaa", "size", "create", "blend", "shaders"};
#endif

struct TheWitcher2GameDeviceData final : public GameDeviceData
{
   // Set when the final grade runs, cleared every Present: scopes the Hide UI skip to this frame's
   // post-grade span.
   bool final_grade_fired_this_frame = false;

   // ---- DLAA / FSR Native AA (Mass Effect 2007's motion vector path, one per-draw buffer: dgVoodoo's vc4) ----
   // DLSS and FSR run in the x64 helper of Core's SR bridge (the game is 32-bit, see "SRBridge.h").
   // "IsSRActive", taken at present: Core's "Super Resolution" selection changes after it, mid frame for the draws
   bool sr_active = false;
   bool fsr_masks_active = false; // FSR with the reactive masks on, latched with "sr_active" (DLSS ignores the masks)
   // The upscaler's output goes back into the exposure's target without its alpha (the scene's, passed through), drawn from this
   // view of it
   com_ptr<ID3D11BlendState> sr_rgb_blend_state;
   com_ptr<ID3D11ShaderResourceView> sr_output_srv;
   // None was picked ("CleanExtraSRResources", from the overlay): the upscaler's resources go at the next present
   std::atomic<bool> release_sr_resources = false;
   std::atomic<bool> mv_active = false; // Motion vectors and jitter this frame: an upscaler is active, or the DEV toggle (set at present)
   std::shared_mutex mv_mutex;
   // A game shader's patched version (null if refused), patched on first use, by its hash; a vertex shader's with the bytes of vc4
   // it reads ("DXBC::ConstantBufferBytes"): the previous frame's copy uploads only those
   template <typename T>
   struct PatchedShader
   {
      com_ptr<T> shader;
      UINT read_size = 0;
      UINT transform_offset = 0; // Vertex shaders: vc4's rows that tell objects apart (LocalToWorld, skinned: bone 0)
      // Vertex shaders: it samples textures (terrain heightmaps), which its second run reads at "previous_resources_slot"
      bool reads_resources = false;
   };
   std::unordered_map<uint32_t, PatchedShader<ID3D11VertexShader>> mv_vertex_shaders;
   std::unordered_map<uint32_t, PatchedShader<ID3D11PixelShader>> mv_pixel_shaders;
   // The alpha blended draws' pixel shaders with the mask target, by blend (see "ClassifyBoundBlend", index - 1)
   std::unordered_map<uint32_t, PatchedShader<ID3D11PixelShader>> mv_reactive_pixel_shaders[2];
   // The motion vector target (output sized; every blend state writes it unblended, see "OnCreateBlendState")
   com_ptr<ID3D11Texture2D> mv_texture;
   com_ptr<ID3D11RenderTargetView> mv_rtv;
   com_ptr<ID3D11UnorderedAccessView> mv_uav; // Null without typed UAV loads of its format (then no upscaler)
   // The upscaler's depth, built from the G-buffer's linear depth by the fill (the game's depth has no shader resource view)
   com_ptr<ID3D11Texture2D> mv_device_depth;
   com_ptr<ID3D11UnorderedAccessView> mv_device_depth_uav;
   // A frame opens at its first mesh draw into output sized depth (the jitter is chosen there), starts at its first motion vector
   // draw (the target is cleared) and ends at the exposure ("TONEMAP_EXPOSURE"), once per present.
   bool mv_scene_open = false;
   bool mv_scene_done = false;
   bool mv_frame_ended = true;
   bool mv_fill_pending = false;
   // The last frame's scene ended at the exposure: only then does this one jitter (menus and loading screens never reach it, and
   // an upscaler that doesn't run would leave the jitter visible)
   bool mv_previous_scene_done = false;
   float sr_vert_fov = 1.0471976f; // FSR's vertical FOV (radians): the last camera's, 60 degrees until one is seen
   float scene_mip_bias = 0.f;     // The scene's texture mip bias under the upscaler, set at present (see "SetMipBias")
   // FSR's reactive and transparency & composition masks (written by the fill from "mv_reactive_target", see
   // "Luma_TW2_MotionVectorFill.hlsl")
   com_ptr<ID3D11Texture2D> mv_reactive;
   com_ptr<ID3D11UnorderedAccessView> mv_reactive_uav;
   com_ptr<ID3D11Texture2D> mv_transparency;
   com_ptr<ID3D11UnorderedAccessView> mv_transparency_uav;
   // The masks the alpha blended draws write (R8G8: x reactive from all, y transparency & composition from the non-additive ones;
   // max blended, see "OnCreateBlendState"), read by the fill
   com_ptr<ID3D11Texture2D> mv_reactive_target;
   com_ptr<ID3D11RenderTargetView> mv_reactive_target_rtv;
   com_ptr<ID3D11ShaderResourceView> mv_reactive_target_srv;
   uint32_t sr_reactive_frame = 0; // The Luma frame index the fill last wrote the masks at: they go after a while without (DLSS, off)

   void ReleaseMotionVectorTargets()
   {
      mv_texture.reset();
      mv_rtv.reset();
      mv_uav.reset();
      mv_device_depth.reset();
      mv_device_depth_uav.reset();
   }
   void ReleaseReactiveMasks()
   {
      mv_reactive.reset();
      mv_reactive_uav.reset();
      mv_transparency.reset();
      mv_transparency_uav.reset();
      mv_reactive_target.reset();
      mv_reactive_target_rtv.reset();
      mv_reactive_target_srv.reset();
   }
   com_ptr<ID3D11Resource> mv_depth; // The scene depth (the depth view's resource)
   // The G-buffer's linear view depth (its fourth target, R32_FLOAT), the fill's input, and its view
   com_ptr<ID3D11Resource> mv_linear_depth;
   com_ptr<ID3D11ShaderResourceView> mv_linear_depth_srv;
   com_ptr<ID3D11Buffer> mv_fill_buffer;
   // The scene's render area (the output size, smaller under the render scale, see "RenderArea"), set when the scene opens, and the
   // last one the upscaler drew (a change restarts its history)
   std::array<uint32_t, 2> mv_render_size = {};
   std::array<uint32_t, 2> sr_render_size = {};
   // The upscaler runs before the post chain, on the linear scene: the scene draws' last target (the scene color). Its exposure (1x1,
   // written by the fill) from the last exposure pass's adaptation texture (t1, the gain at .z) and constants; under the render scale
   // the linear depth's copy and view for its stretch over the surface
   com_ptr<ID3D11RenderTargetView> mv_scene_color_rtv;
   ID3D11RenderTargetView* mv_seen_rtv = nullptr; // The last scene draw's first target, checked once per change
   com_ptr<ID3D11ShaderResourceView> sr_adaptation_srv;
   com_ptr<ID3D11Buffer> sr_exposure_constants; // A copy of the last exposure pass's vc4 (its cap, post-scale and static levels)
   bool sr_exposure_static = false;             // The last exposure pass was a static perm (no adaptation texture)
   com_ptr<ID3D11Texture2D> sr_exposure;
   com_ptr<ID3D11UnorderedAccessView> sr_exposure_uav;
   com_ptr<ID3D11Texture2D> render_area_depth_copy;
   com_ptr<ID3D11ShaderResourceView> render_area_depth_copy_srv;
   com_ptr<ID3D11RenderTargetView> render_area_depth_rtv;
   // The projection jitter (pixels, +y down), chosen when the scene opens; its NDC offset is at VS "MotionVectorPatches::jitter_slot"
   // of every mesh draw depth tested against the scene
   std::array<float, 2> mv_jitter = {};
   std::array<float, 2> mv_jitter_ndc = {}; // The same offset in NDC (y up), as the jitter buffer holds it
   com_ptr<ID3D11Buffer> mv_jitter_buffer;
   // Per-draw lookups kept for the next draw (reset when the scene opens): the jitter path's last depth view and whether it's the scene
   // depth, its last depth stencil state's depth test, the motion vector path's last accepted targets (and the G-buffer's), the last
   // blend state's opacity, the reactive path's last target and whether it's a scene target, the last vertex and pixel shader's patched
   // versions (owned by the maps above, never erased)
   ID3D11DepthStencilView* jitter_dsv = nullptr;
   ID3D11DepthStencilView* mv_not_scene_dsv = nullptr; // This frame's last depth view of another size than the scene (shadow maps)
   bool jitter_dsv_scene = false;
   ID3D11DepthStencilState* jitter_depth_stencil_state = nullptr;
   bool jitter_depth_test = true;
   ID3D11RenderTargetView* mv_accepted_rtv = nullptr;
   ID3D11DepthStencilView* mv_accepted_dsv = nullptr;
   ID3D11RenderTargetView* mv_linear_depth_rtv = nullptr;
   std::array<ID3D11RenderTargetView*, MotionVectorPatches::target_slot> mv_gbuffer_rtvs = {};
   ID3D11RenderTargetView* mv_reactive_rtv = nullptr;
   bool mv_reactive_rtv_scene = false;
   ID3D11BlendState* mv_blend_state = nullptr; // See "ClassifyBoundBlend"
   bool mv_blend_opaque = true;
   uint8_t mv_reactive_blend = 0;
   // The last draw's patched shaders, by the game's hash
   uint32_t mv_last_vertex_shader_hash = 0;
   PatchedShader<ID3D11VertexShader> mv_last_vertex;
   uint32_t mv_last_pixel_shader_hash = 0;
   PatchedShader<ID3D11PixelShader> mv_last_pixel;
   PatchedDraws::BoundShader<ID3D11VertexShader> mv_bound_vertex_shader;
   PatchedDraws::BoundShader<ID3D11PixelShader> mv_bound_pixel_shader;

   // CPU copies of the vc4 buffers the motion vector draws bind, by buffer (an entry registers it, null until its first upload), from
   // a Map(WRITE_DISCARD) at its Unmap or an UpdateSubresource: a draw's constants are its buffer's latest copy
   using ConstantsCopy = std::shared_ptr<const std::vector<uint8_t>>;
   std::shared_mutex mv_constants_mutex;
   std::unordered_map<uint64_t, ConstantsCopy> mv_constants_copies;
   std::unordered_map<uint64_t, void*> mv_mapped_constants; // Registered buffers mapped now, until their Unmap
   // The first registered buffers and their sizes, read by the buffer hooks without the lock (written under it, on the immediate
   // context's thread as the hooks), so the game's other Map/Unmap calls skip both. With more registered, every buffer takes the lock.
   static constexpr uint32_t MAX_FILTERED_BUFFERS = 8;
   std::array<std::atomic<uint64_t>, MAX_FILTERED_BUFFERS> mv_filtered_buffers = {};
   std::array<UINT, MAX_FILTERED_BUFFERS> mv_filtered_buffer_sizes = {};
   std::atomic<uint32_t> mv_filtered_buffer_count = 0;
   // By filter slot ("g_mv_vc4_slots"): the buffer's entry in "mv_constants_copies" (nodes are never erased) and its mapped memory
   // until its Unmap. Only the hooks' thread.
   std::array<ConstantsCopy*, MAX_FILTERED_BUFFERS> mv_filtered_copies = {};
   std::array<void*, MAX_FILTERED_BUFFERS> mv_filtered_mapped = {};
   std::atomic<bool> mv_filter_overflow = false;
   // Every pooled vc4 copy, and by size those nobody held anymore at the last present (taken by the next copies of that size, instead
   // of a new allocation per Unmap). Under "mv_constants_mutex".
   std::vector<std::shared_ptr<std::vector<uint8_t>>> mv_constants_pool;
   std::unordered_map<size_t, std::vector<uint32_t>> mv_constants_pool_free;
   size_t mv_constants_made = 0; // Copies asked for since the last present
   // Previous frame constants of the motion vector draws (see "PatchedDraws::PreviousConstants")
   PatchedDraws::PreviousConstants mv_previous_constants;
   // Motion vector draws by draw key (shaders, buffers, arguments), with their transform and vc4. A draw takes the previous frame's
   // vc4 of its key's nearest draw (same object, a frame earlier), its camera included.
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
   uint32_t mv_logged_untransformed_frame = 0;
   uint32_t mv_logged_terrain_frame = 0;
   // Matches whose nearest candidate wasn't exact, by vertex shader: count and largest transform distance (logged every 300 frames)
   struct InexactMatches
   {
      uint32_t count = 0;
      float max_distance = 0.f;
   };
   std::unordered_map<uint32_t, InexactMatches> mv_inexact_by_vs;
   uint32_t mv_logged_inexact_frame = 0;
#endif

#if DEVELOPMENT
   bool mv_logged_jitter = false; // The jitter state "[TW2 SR]" last logged
   // Per frame counts for the DEV panel and the MCP (the last complete frame's shown)
   struct MotionVectorStats
   {
      uint32_t motion_vector_draws = 0, jitter_draws = 0, reactive_draws = 0, matched = 0, camera_only = 0, other_camera = 0, uncopied = 0, maps = 0, updates = 0, other_maps = 0, sr_draws = 0;
      uint32_t untransformed_ambiguous = 0; // Draws without a tie-break transform whose key had several previous candidates
      uint32_t terrain_inexact = 0;         // Terrain chunks whose nearest previous candidate was another chunk (not drawn last frame)
      uint32_t offset_bindings = 0;         // vc4 bound at a constant buffer offset (a ring: the per-buffer copies would be wrong)
      uint32_t constants_pool = 0;          // At present: the pooled copies (see "NewConstantsCopy")
      uint32_t tiebreak_collisions = 0;     // The previous frame's, see "PatchedDraws::CountTieBreakCollisions"
      uint32_t ended_by = 0;
      float near_plane = 0.f, far_plane = 0.f;                   // The upscaler's, from the camera's projection (0: none found)
      uint32_t rejected[size_t(MotionVectorReject::COUNT)] = {}; // "DrawWithMotionVectors" refusals by reason ("MV_REJECT")
   };
   MotionVectorStats mv_stats, mv_last_stats;
   int mv_draw_reject = -1; // The current draw's "MV_REJECT" reason (-1 for none), for the MCP trace note
   // "Performance Test": GPU timestamps per frame (present to present, the scene from its opening to the exposure, the end of the
   // scene: the fill, the upscaler and its copy back, see "PerfStamp"); and the CPU time in the scene hooks
   struct PerfStats
   {
      Perf::Stat frame, scene, end;
      Perf::Stat end_parts[3]; // The end of the scene: fill, upscaler, copy back (with the state restore)
   };
   Perf::TimestampRing<PERF_STAMP_COUNT> perf_timestamps;
   Perf::Window<PerfStats> perf_window;
   bool perf_sr_active = false; // The last measured frame's upscaler
   // The user's anti-aliasing and CPU savings while a mode that sets its own runs
   SR::Type perf_user_sr_type = SR::Type::None;
   bool perf_user_smaa = false;
   bool perf_user_vc4_filter = true;
   bool perf_user_vc4_pool = true;
   bool perf_user_vc4_slots = true;
#endif

   // ---- SMAA (see RunPostFinalGradeSMAA) ----
   // The one resolution every surface below is sized to, core's own DrawSMAA intermediates included: they all come
   // from the same canvas, so a change drops the lot and the per-pointer checks rebuild it.
   uint32_t scratch_w = 0, scratch_h = 0;
   // The Luma frame index the SMAA chain last ran at, and the snapshot was last used at (SMAA, or RCAS alone after the upscaler): their
   // resources go after a while without (see "OnPresent")
   uint32_t smaa_frame = 0;
   uint32_t snapshot_frame = 0;
   // SMAA metrics CB (b1) = (1/w,1/h,w,h) + (predication scale,0,0,0); scale 2.0 when predication on, else 1.0.
   ComPtr<ID3D11Buffer> cb_smaa_metrics;
   // SMAA scratch. tex_input = SRV snapshot of the canvas (gamma): edge detection reads it, and the neighborhood blend filters it in
   // linear light itself (no linear copy, SMAA_NEIGHBORHOOD_GAMMA_IN_LINEAR); RCAS alone reads it after the upscaler.
   ComPtr<ID3D11Texture2D> tex_input;
   ComPtr<ID3D11ShaderResourceView> srv_input;
   // RCAS input temp (SRV+RTV), allocated ONLY while sharpening is on: with RCAS off, SMAA's last pass writes
   // the canvas directly and this stays null.
   ComPtr<ID3D11Texture2D> tex_smaa_out;
   ComPtr<ID3D11RenderTargetView> tex_smaa_out_rtv;
   ComPtr<ID3D11ShaderResourceView> tex_smaa_out_srv;
   // RCAS sharpen CB (b0) = (w,h,sharpness,0) + output temp (canvas format, RTV).
   ComPtr<ID3D11Buffer> cb_sharpen;
   float sharpen_amount = 0.f; // the value cb_sharpen was built with; meaningful only while cb_sharpen exists
   // Full-res r32_float depth, captured at whichever comes first: the BRIGHT-PASS tonemap draw (t1) or the AO
   // pack pass (t0). tex_pred is the R16F edge-ness from the Depth Extract CS, not a depth.
   ComPtr<ID3D11ShaderResourceView> srv_scene_depth;
   ComPtr<ID3D11Texture2D> tex_pred;
   ComPtr<ID3D11UnorderedAccessView> uav_pred;
   ComPtr<ID3D11ShaderResourceView> srv_pred;
   ComPtr<ID3D11Buffer> cb_pred;
   // The values cb_pred and cb_smaa_metrics were built with; meaningful only while that CB exists.
   float pred_tolerance = 0.f;
   float smaa_metrics_pred_scale = 0.f; // recreate the metrics CB when predication turns on/off

   // ---- XeGTAO (see RunXeGTAO) ----
   // Sized from the depth SRV captured at the hooked draw, so no per-present reset is needed. tex_gtao_final
   // is our copy source: the game's AO RT is created without D3D11_BIND_UNORDERED_ACCESS.
   ComPtr<ID3D11Texture2D> tex_gtao_depth_mips; // R32F, 5 mips (prefiltered view-space depth pyramid)
   ComPtr<ID3D11UnorderedAccessView> gtao_depth_mip_uavs[5];
   ComPtr<ID3D11ShaderResourceView> srv_gtao_depth_mips;
   ComPtr<ID3D11Texture2D> tex_gtao_working[2]; // R8G8_UNORM AO+edges ping-pong
   ComPtr<ID3D11UnorderedAccessView> uav_gtao_working[2];
   ComPtr<ID3D11ShaderResourceView> srv_gtao_working[2];
   ComPtr<ID3D11Texture2D> tex_gtao_final; // gtao_final_fmt, CopyResource'd into the game's AO RT
   ComPtr<ID3D11UnorderedAccessView> uav_gtao_final;
   // The size and format the set was built for. Committed even when the allocation fails, so a null set under a
   // matching triple means "failed" and no per-frame retry fragments the 32-bit address space. ReleaseGTAOScratch
   // clears it.
   uint32_t gtao_w = 0, gtao_h = 0;
   DXGI_FORMAT gtao_final_fmt = DXGI_FORMAT_UNKNOWN; // actual (possibly Luma-upgraded) AO RT format
   com_ptr<ID3D11Buffer> cb_gtao;                    // knobs + viewport (GTAO_KNOBS_CB_SLOT), dynamic: the noise index changes every frame

#if DEVELOPMENT
   // ---- Vanilla constant logger (see LogVanillaGrade / LogVanillaTonemap) ----
   // Staging copies are mapped a frame later without waiting, so the render thread never stalls.
   struct ConstantCapture
   {
      ComPtr<ID3D11Buffer> staging;
      UINT bytes = 0;
      bool copy_pending = false;
   };
   ConstantCapture grade_cb;
   ConstantCapture tonemap_cb;
   ConstantCapture aogen_cb;
   ComPtr<ID3D11Texture2D> adaptation_staging; // 1x1 copy of the exposure pass's sLumFinal
   bool adaptation_copy_pending = false;
   std::string last_grade_line;
   std::string last_tonemap_line;
   std::string last_aogen_line;
#endif

   void ReleaseGTAOScratch()
   {
      tex_gtao_depth_mips.reset();
      for (auto& uav : gtao_depth_mip_uavs)
      {
         uav.reset();
      }
      srv_gtao_depth_mips.reset();
      for (int i = 0; i < 2; i++)
      {
         tex_gtao_working[i].reset();
         uav_gtao_working[i].reset();
         srv_gtao_working[i].reset();
      }
      tex_gtao_final.reset();
      uav_gtao_final.reset();
      gtao_w = 0;
      gtao_h = 0;
      gtao_final_fmt = DXGI_FORMAT_UNKNOWN;
      cb_gtao.reset(); // it holds the viewport size
   }

   // Turning a feature off gives the address space back: at 4K these hold ~200 MB (SMAA) and ~30 MB (GTAO)
   // in a 32-bit process, and everything is recreated on demand.
   void ReleaseSMAAScratch()
   {
      srv_input.reset();
      tex_input.reset();
      ReleaseSharpenScratch();
   }

   // The RCAS intermediate exists only while sharpening is on: with RCAS at 0 the SMAA chain writes the canvas
   // directly, so this is ~66 MB (4K rgba16f) of pure waste until the next resolution change.
   void ReleaseSharpenScratch()
   {
      tex_smaa_out_rtv.reset();
      tex_smaa_out_srv.reset();
      tex_smaa_out.reset();
      cb_sharpen.reset();
   }

   void ReleasePredicationScratch()
   {
      uav_pred.reset();
      srv_pred.reset();
      tex_pred.reset();
      cb_pred.reset();
   }
};

class TheWitcher2Game final : public Game
{
   static TheWitcher2GameDeviceData& GetGameDeviceData(DeviceData& device_data)
   {
      return *static_cast<TheWitcher2GameDeviceData*>(device_data.game);
   }

   // Matches a pass by both of its keyed dgVoodoo hashes.
   static bool ContainsPixelShader(const ShaderHashesList<OneShaderPerPipeline>& shader_hashes, const DgVoodooHashes& pass)
   {
      return shader_hashes.Contains(pass.v2873, reshade::api::shader_stage::pixel) || shader_hashes.Contains(pass.v2813, reshade::api::shader_stage::pixel);
   }

   static bool IsTonemap(const ShaderHashesList<OneShaderPerPipeline>& shader_hashes)
   {
      return ContainsPixelShader(shader_hashes, TONEMAP_EXPOSURE) || ContainsPixelShader(shader_hashes, TONEMAP_BRIGHT_PASS) ||
             ContainsPixelShader(shader_hashes, TONEMAP_EXPOSURE_STATIC) || ContainsPixelShader(shader_hashes, TONEMAP_BRIGHT_PASS_STATIC);
   }

#if DEVELOPMENT
   // Another process can own ReShade.log (the BL2 launcher precedent), so every line also goes to our own file
   // beside the exe.
   static void LogVanillaLine(const std::string& line)
   {
      reshade::log::message(reshade::log::level::info, line.c_str());
      std::ofstream file("Luma-TW2.log", std::ios::app);
      if (file)
      {
         file << line << std::endl;
      }
   }

   // Copy the pass's PS cb4 and hand back `row_count` rows of it, one frame late (the BL2 CaptureConstantRows path).
   // False = nothing to read yet, normal on the first calls.
   static bool CaptureConstantRows(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, TheWitcher2GameDeviceData::ConstantCapture* capture, uint32_t first_row, uint32_t row_count, float* out)
   {
      ComPtr<ID3D11Buffer> cb;
      native_device_context->PSGetConstantBuffers(4, 1, cb.put());
      if (!cb)
         return false;
      D3D11_BUFFER_DESC bd = {};
      cb->GetDesc(&bd);
      if (bd.ByteWidth < (first_row + row_count) * 16)
         return false;

      if (capture->bytes != bd.ByteWidth)
      {
         const D3D11_BUFFER_DESC sd = {.ByteWidth = bd.ByteWidth, .Usage = D3D11_USAGE_STAGING, .CPUAccessFlags = D3D11_CPU_ACCESS_READ};
         capture->staging.reset();
         capture->copy_pending = false;
         if (FAILED(native_device->CreateBuffer(&sd, nullptr, capture->staging.put())) || !capture->staging)
         {
            capture->bytes = 0;
            return false;
         }
         capture->bytes = bd.ByteWidth;
      }

      bool have_rows = false;
      if (capture->copy_pending)
      {
         D3D11_MAPPED_SUBRESOURCE mapped = {};
         if (SUCCEEDED(native_device_context->Map(capture->staging.get(), 0, D3D11_MAP_READ, D3D11_MAP_FLAG_DO_NOT_WAIT, &mapped)) && mapped.pData != nullptr)
         {
            std::memcpy(out, (const uint8_t*)mapped.pData + (size_t)first_row * 16, (size_t)row_count * 16);
            native_device_context->Unmap(capture->staging.get(), 0);
            capture->copy_pending = false;
            have_rows = true;
         }
      }
      if (!capture->copy_pending)
      {
         native_device_context->CopyResource(capture->staging.get(), cb.get());
         capture->copy_pending = true;
      }
      return have_rows;
   }

   // CEnvFinalColorBalanceParameters at the final grade (cb4[60..71] = DX9 c52..c63), logged whenever a value changes.
   // vMidtone should sit near 1/2.2: it is the only power stage between the linear lighting and the gamma-space UI.
   static void LogVanillaGrade(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, TheWitcher2GameDeviceData* gd)
   {
      if ((cb_luma_global_settings.FrameIndex & 15u) != 0u)
         return;
      float rows[12 * 4] = {};
      if (!CaptureConstantRows(native_device, native_device_context, &gd->grade_cb, 60, 12, rows))
         return;
      const auto row = [&rows](uint32_t cb4_index)
      {
         const float* v = &rows[(cb4_index - 60) * 4];
         return std::format("({:.4f}, {:.4f}, {:.4f}, {:.4f})", v[0], v[1], v[2], v[3]);
      };
      std::string line = std::format("[TW2-Grade] vHighlight={} vMidtone={} vShadow={} vVignetteWeights={} vVignetteColor={} vSplitToneShadows={} vSplitToneHighlights={} vSplitToneBalance={} vSplitToneRange={}",
         row(60), row(61), row(62), row(66), row(67), row(68), row(69), row(70), row(71));
      if (line == gd->last_grade_line)
         return;
      LogVanillaLine(std::format("{} frame={}", line, cb_luma_global_settings.FrameIndex));
      gd->last_grade_line = std::move(line);
   }

   // The exposure pass at its main draw: PSC_LumWeights and PSC_LumRanges2 (cb4[58..59]) whenever they change, and the
   // 1x1 adaptation texel at t1 every 120 frames, which holds (black, white, gain = 1 / max(white - black, 0.01)).
   static void LogVanillaTonemap(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, TheWitcher2GameDeviceData* gd)
   {
      if ((cb_luma_global_settings.FrameIndex % 120u) != 0u)
         return;
      float rows[2 * 4] = {};
      if (CaptureConstantRows(native_device, native_device_context, &gd->tonemap_cb, 58, 2, rows))
      {
         std::string line = std::format("[TW2-Tonemap] PSC_LumWeights=({:.4f}, {:.4f}, {:.4f}, {:.4f}) PSC_LumRanges2=({:.4f}, {:.4f}, {:.4f}, {:.4f})", rows[0], rows[1], rows[2], rows[3], rows[4], rows[5], rows[6], rows[7]);
         if (line != gd->last_tonemap_line)
         {
            LogVanillaLine(std::format("{} frame={}", line, cb_luma_global_settings.FrameIndex));
            gd->last_tonemap_line = std::move(line);
         }
      }

      // Validated on every call, not once: anything but a 1x1 fp16 texel would decode as garbage.
      ComPtr<ID3D11ShaderResourceView> srv;
      native_device_context->PSGetShaderResources(1, 1, srv.put());
      if (!srv)
         return;
      ComPtr<ID3D11Resource> res;
      srv->GetResource(res.put());
      ComPtr<ID3D11Texture2D> tex;
      if (!res || FAILED(res->QueryInterface(tex.put())))
         return;
      D3D11_TEXTURE2D_DESC td = {};
      tex->GetDesc(&td);
      if (td.Format != DXGI_FORMAT_R16G16B16A16_FLOAT || td.Width != 1 || td.Height != 1)
         return;

      if (gd->adaptation_copy_pending)
      {
         D3D11_MAPPED_SUBRESOURCE mapped = {};
         if (SUCCEEDED(native_device_context->Map(gd->adaptation_staging.get(), 0, D3D11_MAP_READ, D3D11_MAP_FLAG_DO_NOT_WAIT, &mapped)) && mapped.pData != nullptr)
         {
            const uint16_t* texel = static_cast<const uint16_t*>(mapped.pData);
            LogVanillaLine(std::format("[TW2-Adaptation] black={:.5f} white={:.5f} gain={:.5f} frame={}", DirectX::PackedVector::XMConvertHalfToFloat(texel[0]),
               DirectX::PackedVector::XMConvertHalfToFloat(texel[1]), DirectX::PackedVector::XMConvertHalfToFloat(texel[2]), cb_luma_global_settings.FrameIndex));
            native_device_context->Unmap(gd->adaptation_staging.get(), 0);
            gd->adaptation_copy_pending = false;
         }
      }
      if (!gd->adaptation_copy_pending)
      {
         if (!gd->adaptation_staging)
         {
            D3D11_TEXTURE2D_DESC sd = td;
            sd.MipLevels = 1;
            sd.ArraySize = 1;
            sd.Usage = D3D11_USAGE_STAGING;
            sd.BindFlags = 0;
            sd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            sd.MiscFlags = 0;
            if (FAILED(native_device->CreateTexture2D(&sd, nullptr, gd->adaptation_staging.put())))
               return;
         }
         native_device_context->CopySubresourceRegion(gd->adaptation_staging.get(), 0, 0, 0, 0, tex.get(), 0, nullptr);
         gd->adaptation_copy_pending = true;
      }
   }

   // The native SSAO generator's layout and constants, logged on change. Meaning read from its disassembly (NVIDIA HBAO
   // shape): cb4[8] = AO target (W, H, 1/W, 1/H), cb4[9] = (1/tanX, 1/tanY, tanX, tanY), cb4[10] = (directions, steps,
   // tan angle bias, angle bias), cb4[11] = (R, R^2, 1/R, H/W), cb4[14] = (noise tile scale, distance fade rate, fade
   // amount, attenuation), cb4[15].y = contrast, cb4[16].y = max kernel radius in AO target pixels. XeGTAO reuses only
   // cb4[9].zw; the rest are candidates for following the game per environment.
   static void LogAOGenLayout(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, const DeviceData& device_data, TheWitcher2GameDeviceData* gd)
   {
      if ((cb_luma_global_settings.FrameIndex & 15u) != 0u)
         return;
      float rows[9 * 4] = {};
      if (!CaptureConstantRows(native_device, native_device_context, &gd->aogen_cb, 8, 9, rows))
         return;
      D3D11_VIEWPORT viewport = {};
      UINT viewport_count = 1;
      native_device_context->RSGetViewports(&viewport_count, &viewport);
      ComPtr<ID3D11RenderTargetView> rtv;
      native_device_context->OMGetRenderTargets(1, rtv.put(), nullptr);
      ComPtr<ID3D11ShaderResourceView> srv_depth;
      native_device_context->PSGetShaderResources(0, 1, srv_depth.put());
      uint4 rt_info{}, depth_info{};
      DXGI_FORMAT rt_fmt = DXGI_FORMAT_UNKNOWN, depth_fmt = DXGI_FORMAT_UNKNOWN;
      GetResourceInfo(rtv.get(), rt_info, rt_fmt);
      GetResourceInfo(srv_depth.get(), depth_info, depth_fmt);
      const auto row = [&rows](uint32_t cb4_index)
      {
         const float* v = &rows[(cb4_index - 8) * 4];
         return std::format("({:.6f}, {:.6f}, {:.6f}, {:.6f})", v[0], v[1], v[2], v[3]);
      };
      std::string line = std::format("[TW2-AOGen] output={}x{} viewport=({:.1f}, {:.1f}) {:.1f}x{:.1f} rt={}x{} depth={}x{} cb4[8]={} cb4[9]={} cb4[10]={} cb4[11]={} cb4[14]={} cb4[15]={} cb4[16]={}",
         (uint32_t)device_data.output_resolution.x, (uint32_t)device_data.output_resolution.y, viewport.TopLeftX, viewport.TopLeftY, viewport.Width, viewport.Height,
         rt_info.x, rt_info.y, depth_info.x, depth_info.y, row(8), row(9), row(10), row(11), row(14), row(15), row(16));
      if (line == gd->last_aogen_line)
         return;
      LogVanillaLine(std::format("{} frame={}", line, cb_luma_global_settings.FrameIndex));
      gd->last_aogen_line = std::move(line);
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

   // Any of the four final-grade permutations (FXAA and vignette are compiled in or out independently); all
   // four host the Luma HDR block through the same shader file.
   static bool IsFinalGrade(const ShaderHashesList<OneShaderPerPipeline>& shader_hashes)
   {
      return ContainsPixelShader(shader_hashes, FINAL_GRADE) || ContainsPixelShader(shader_hashes, FINAL_GRADE_NO_AA) || ContainsPixelShader(shader_hashes, FINAL_GRADE_NO_VIGNETTE) || ContainsPixelShader(shader_hashes, FINAL_GRADE_AA_NO_VIGNETTE);
   }

   // ---- DLAA / FSR Native AA with motion vectors and jitter from patched shaders (Mass Effect 2007's path; vc4 layout in
   // "MotionVectorPatches") ----
   // WorldToScreen (c4-c7): the rows of a column vector matrix (the vertex shaders "dp4" the position with each), unlike UE3's
   static constexpr size_t VIEW_PROJECTION_OFFSET = MotionVectorPatches::view_projection_row * 16;
   // ... up to the camera (c16-c19), with WorldToView (c8-c10): what an unmatched draw takes from last frame's camera
   static constexpr size_t CAMERA_SIZE = 16 * 16;
   // LocalToWorld (c0-c2, 3x4), the tie-break between the objects sharing a draw key
   static constexpr size_t LOCAL_TO_WORLD_OFFSET = MotionVectorPatches::object_row_offset * 16;
   // Skinned vertex shaders read no LocalToWorld, their bones are in world space: bone 0 (c70-c72) tells objects apart
   static constexpr uint32_t BONES_ROW = MotionVectorPatches::object_row_offset + 70;
   // Terrain chunks share LocalToWorld (and their draw key): their position and UVs (c24-c26) tell them apart
   static constexpr uint32_t TERRAIN_CHUNK_ROW = MotionVectorPatches::object_row_offset + 24;
   static_assert(sizeof(PatchedDraws::ObjectTransform) == 3 * 16);

   // An upscaler is picked and hasn't failed (it then gives way to SMAA until picked again). Fixed for the whole frame (see
   // "OnPresent"): a selection made after the motion vector state was set would otherwise run the upscaler on mixed state.
   static bool IsSRActive(DeviceData& device_data)
   {
      return GetGameDeviceData(device_data).sr_active;
   }

   // False if the buffer surely isn't a registered vc4 one; "size" its size if known (else 0), "slot" its filter slot if known (else
   // -1). Lock free.
   static bool MayBeRegisteredBuffer(const TheWitcher2GameDeviceData& gd, uint64_t handle, UINT* size, int* slot)
   {
      *size = 0;
      *slot = -1;
      if (!g_mv_buffer_filter || gd.mv_filter_overflow.load(std::memory_order_relaxed))
         return true;
      const uint32_t count = gd.mv_filtered_buffer_count.load(std::memory_order_acquire);
      for (uint32_t i = 0; i < count; i++)
      {
         if (gd.mv_filtered_buffers[i].load(std::memory_order_relaxed) == handle)
         {
            *size = gd.mv_filtered_buffer_sizes[i];
            *slot = (g_mv_vc4_slots ? int(i) : -1);
            return true;
         }
      }
      return false;
   }

   // A buffer registered in "mv_constants_copies" joins the lock free filter. Under "mv_constants_mutex".
   static void AddFilteredBuffer(TheWitcher2GameDeviceData* gd, ID3D11Buffer* buffer, TheWitcher2GameDeviceData::ConstantsCopy* copy)
   {
      const uint32_t count = gd->mv_filtered_buffer_count.load(std::memory_order_relaxed);
      if (count >= TheWitcher2GameDeviceData::MAX_FILTERED_BUFFERS)
      {
         gd->mv_filter_overflow = true;
         return;
      }
      D3D11_BUFFER_DESC desc;
      buffer->GetDesc(&desc);
      gd->mv_filtered_buffer_sizes[count] = desc.ByteWidth;
      gd->mv_filtered_copies[count] = copy;
      gd->mv_filtered_buffers[count].store(reinterpret_cast<uint64_t>(buffer), std::memory_order_relaxed);
      gd->mv_filtered_buffer_count.store(count + 1, std::memory_order_release);
   }

   static UINT GetBufferSize(uint64_t handle, UINT known_size)
   {
      if (known_size != 0)
         return known_size;
      D3D11_BUFFER_DESC desc;
      reinterpret_cast<ID3D11Buffer*>(handle)->GetDesc(&desc);
      return desc.ByteWidth;
   }

   // A vc4 copy of "size" bytes from "bytes" (null: zeroed): one nobody held anymore at the last present, else a new one. Under
   // "mv_constants_mutex".
   static std::shared_ptr<std::vector<uint8_t>> NewConstantsCopy(TheWitcher2GameDeviceData* gd, const uint8_t* bytes, size_t size)
   {
      gd->mv_constants_made++;
      if (const auto free_copies = gd->mv_constants_pool_free.find(size); g_mv_constants_pool && free_copies != gd->mv_constants_pool_free.end() && !free_copies->second.empty())
      {
         auto copy = gd->mv_constants_pool[free_copies->second.back()];
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
      auto copy = (bytes ? std::make_shared<std::vector<uint8_t>>(bytes, bytes + size) : std::make_shared<std::vector<uint8_t>>(size));
      if (g_mv_constants_pool)
      {
         gd->mv_constants_pool.push_back(copy);
      }
      return copy;
   }

   // A vc4 buffer's copy from its mapped memory, at its Unmap. Under "mv_constants_mutex".
   static void StoreMappedConstants(TheWitcher2GameDeviceData* gd, TheWitcher2GameDeviceData::ConstantsCopy* copy, const void* mapped, UINT size)
   {
      // A copy no draw took (only the registry and the pool hold it: the shadow passes' ~1800 Unmaps a frame, draws that aren't motion
      // vector ones) is rewritten in place instead of taking another one (Borderlands GOTY's "in place rewrite": a much smaller pool)
      if (g_mv_constants_pool && *copy && copy->use_count() == 2 && (*copy)->size() == size)
      {
         std::memcpy(const_cast<uint8_t*>((*copy)->data()), mapped, size);
      }
      else
      {
         *copy = NewConstantsCopy(gd, static_cast<const uint8_t*>(mapped), size);
      }
#if DEVELOPMENT
      gd->mv_stats.maps++;
#endif
   }

   // Motion vectors: a registered vc4 buffer mapped for a whole rewrite, remembered until its Unmap
   static void OnMapBufferRegion(reshade::api::device* device, reshade::api::resource resource, uint64_t offset, uint64_t size, reshade::api::map_access access, void** data)
   {
      DeviceData* const device_data = device->get_private_data<DeviceData>();
      if (!device_data || !device_data->game || !GetGameDeviceData(*device_data).mv_active || !data || !*data)
         return;
      auto& gd = GetGameDeviceData(*device_data);
#if DEVELOPMENT
      const Perf::HookTimer perf_timer{&gd.perf_window.hook_ns};
#endif
      UINT buffer_size;
      int slot;
      if (!MayBeRegisteredBuffer(gd, resource.handle, &buffer_size, &slot))
         return;
      const bool whole_write = access == reshade::api::map_access::write_discard && offset == 0;
      // A filter slot is a registered buffer: no lookups, no lock
      if (slot >= 0)
      {
         gd.mv_filtered_mapped[slot] = (whole_write ? *data : nullptr);
#if DEVELOPMENT
         gd.mv_stats.other_maps += !whole_write;
#endif
         return;
      }
      const std::lock_guard lock(gd.mv_constants_mutex);
      if (!gd.mv_constants_copies.contains(resource.handle))
         return;
      if (whole_write)
      {
         gd.mv_mapped_constants[resource.handle] = *data;
      }
#if DEVELOPMENT
      else
      {
         gd.mv_stats.other_maps++; // A partial or appending write: the copies would miss it
      }
#endif
   }

   // Motion vectors: the CPU copy of a vc4 buffer, before its Unmap (the game has written it). Reads the mapped memory back.
   static void OnUnmapBufferRegion(reshade::api::device* device, reshade::api::resource resource)
   {
      DeviceData* const device_data = device->get_private_data<DeviceData>();
      if (!device_data || !device_data->game || !GetGameDeviceData(*device_data).mv_active)
         return;
      auto& gd = GetGameDeviceData(*device_data);
#if DEVELOPMENT
      const Perf::HookTimer perf_timer{&gd.perf_window.hook_ns};
#endif
      UINT buffer_size;
      int slot;
      if (!MayBeRegisteredBuffer(gd, resource.handle, &buffer_size, &slot))
         return;
      if (slot >= 0)
      {
         const void* const mapped = std::exchange(gd.mv_filtered_mapped[slot], nullptr);
         if (!mapped)
            return;
         const std::lock_guard lock(gd.mv_constants_mutex);
         StoreMappedConstants(&gd, gd.mv_filtered_copies[slot], mapped, buffer_size);
         return;
      }
      const std::lock_guard lock(gd.mv_constants_mutex);
      const auto mapped = gd.mv_mapped_constants.find(resource.handle);
      if (mapped == gd.mv_mapped_constants.end())
         return;
      StoreMappedConstants(&gd, &gd.mv_constants_copies[resource.handle], mapped->second, GetBufferSize(resource.handle, buffer_size));
      gd.mv_mapped_constants.erase(mapped);
   }

   // Motion vectors: the CPU copy of a vc4 buffer from an UpdateSubresource (before it runs); a partial update is merged into the
   // last copy. Needs the unsigned "full add-on support" ReShade (the signed build doesn't raise the event).
   static bool OnUpdateBufferRegion(reshade::api::device* device, const void* data, reshade::api::resource resource, uint64_t offset, uint64_t size)
   {
      DeviceData* const device_data = device->get_private_data<DeviceData>();
      if (!device_data || !device_data->game || !data || !GetGameDeviceData(*device_data).mv_active)
         return false;
      auto& gd = GetGameDeviceData(*device_data);
#if DEVELOPMENT
      const Perf::HookTimer perf_timer{&gd.perf_window.hook_ns};
#endif
      UINT known_size;
      int slot;
      if (!MayBeRegisteredBuffer(gd, resource.handle, &known_size, &slot))
         return false;
      const std::lock_guard lock(gd.mv_constants_mutex);
      const auto copy = gd.mv_constants_copies.find(resource.handle);
      if (copy == gd.mv_constants_copies.end())
         return false;
      const UINT buffer_size = GetBufferSize(resource.handle, known_size);
      if (offset >= buffer_size)
         return false;
      const size_t updated_size = size_t((std::min)(size, uint64_t(buffer_size) - offset));
      const auto* const bytes = static_cast<const uint8_t*>(data);
      if (updated_size == buffer_size)
      {
         copy->second = NewConstantsCopy(&gd, bytes, updated_size);
      }
      else
      {
         const bool merge = copy->second && copy->second->size() == buffer_size;
         auto updated = NewConstantsCopy(&gd, merge ? copy->second->data() : nullptr, buffer_size);
         std::memcpy(updated->data() + offset, bytes, updated_size);
         copy->second = std::move(updated);
      }
#if DEVELOPMENT
      gd.mv_stats.updates++;
#endif
      return false;
   }

   // The bound shader's motion vector version (or, "reactive" > 0, a pixel shader's reactive mask one, see "ClassifyBoundBlend"),
   // patched from Core's bytecode copy on first use (null if it can't be, e.g. a vertex shader that doesn't place vertices with
   // WorldToScreen)
   template <typename T>
   static TheWitcher2GameDeviceData::PatchedShader<T> GetMotionVectorShader(ID3D11Device* native_device, DeviceData& device_data,
      std::unordered_map<uint32_t, TheWitcher2GameDeviceData::PatchedShader<T>>* shaders, uint32_t hash, reshade::api::pipeline pipeline, uint8_t reactive = 0)
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
      UINT transform_offset = 0;
      bool reads_resources = false;
      {
         const std::shared_lock lock(s_mutex_generic);
         if (const auto it = device_data.pipeline_cache_by_pipeline_handle.find(pipeline.handle); it != device_data.pipeline_cache_by_pipeline_handle.end() && it->second->subobjects_cache)
         {
            const auto* desc = static_cast<const reshade::api::shader_desc*>(it->second->subobjects_cache[0].data);
            const auto* code = static_cast<const uint8_t*>(desc->code);
            if constexpr (vertex)
            {
               patched = MotionVectorPatch::PatchVertexShader(code, desc->code_size, MotionVectorPatches::layout, &error);
               read_size = DXBC::ConstantBufferBytes(code, desc->code_size, MotionVectorPatches::object_slot);
               // None (world space vertices: billboards) leaves the transform zero
               if (DXBC::ReadsConstantRow(code, desc->code_size, MotionVectorPatches::object_slot, TERRAIN_CHUNK_ROW))
               {
                  transform_offset = TERRAIN_CHUNK_ROW * 16;
               }
               else if (DXBC::ReadsConstantRow(code, desc->code_size, MotionVectorPatches::object_slot, MotionVectorPatches::object_row_offset))
               {
                  transform_offset = LOCAL_TO_WORLD_OFFSET;
               }
               else if (DXBC::ReadsConstantRow(code, desc->code_size, MotionVectorPatches::object_slot, BONES_ROW, true))
               {
                  transform_offset = BONES_ROW * 16;
               }
               std::vector<DXBC::Chunk> chunks;
               std::vector<uint32_t> tokens;
               std::vector<DXBC::Instruction> instructions;
               size_t first_body = 0;
               if (DXBC::ReadChunks(code, desc->code_size, &chunks))
               {
                  if (const DXBC::Chunk* const program = DXBC::FindChunk(&chunks, DXBC::FourCC("SHEX"), DXBC::FourCC("SHDR"));
                     program && DXBC::ReadProgram(*program, &tokens, &instructions, &first_body))
                  {
                     reads_resources = std::any_of(instructions.begin(), instructions.begin() + first_body, [](const DXBC::Instruction& instruction)
                        { return instruction.opcode == D3D10_SB_OPCODE_DCL_RESOURCE; });
                  }
               }
            }
            else if (reactive != 0)
            {
               patched = MotionVectorPatch::PatchPixelShaderReactive(code, desc->code_size, MotionVectorPatches::layout, MotionVectorPatches::reactive_slot, reactive == 2, &error);
            }
            else
            {
               MotionVectorPatch::PixelShader pixel_shader;
               if (MotionVectorPatch::ReadPixelShader(code, desc->code_size, MotionVectorPatches::layout, &pixel_shader, &error))
               {
                  patched = MotionVectorPatch::PatchPixelShader(code, desc->code_size, MotionVectorPatches::layout, &error);
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
         {
            hr = native_device->CreatePixelShader(patched.data(), patched.size(), nullptr, &shader);
         }
         if (FAILED(hr))
         {
            error = std::format("create 0x{:08X}", uint32_t(hr));
         }
      }
      // Failures in every build (bug reports), except the expected screen space refusals; every patched shader only in development
      const bool screen_space = error.starts_with("screen space");
      if (DEVELOPMENT || (!shader && !screen_space))
      {
         reshade::log::message((shader || screen_space) ? reshade::log::level::info : reshade::log::level::warning,
            std::format("[TW2 MV] {} 0x{:08X} {}", vertex ? "VS" : (reactive == 0 ? "PS" : (reactive == 2 ? "PS reactive additive" : "PS reactive alpha")), hash, shader ? "patched" : error).c_str());
      }
      const std::unique_lock lock(gd.mv_mutex);
      return shaders->try_emplace(hash, TheWitcher2GameDeviceData::PatchedShader<T>{shader, read_size, transform_offset, reads_resources}).first->second;
   }

   // The bound vertex shader's patched version (null if refused), looked up again only when the game's changes
   static ID3D11VertexShader* GetPatchedVertexShader(ID3D11Device* native_device, CommandListData& cmd_list_data, DeviceData& device_data, uint32_t hash)
   {
      auto& gd = GetGameDeviceData(device_data);
      if (hash != gd.mv_last_vertex_shader_hash)
      {
         gd.mv_last_vertex = GetMotionVectorShader(native_device, device_data, &gd.mv_vertex_shaders, hash, cmd_list_data.pipeline_state_original_vertex_shader);
         gd.mv_last_vertex_shader_hash = hash;
      }
      return gd.mv_last_vertex.shader.get();
   }

   // Classifies the bound blend state (cached in "mv_blend_state"): "mv_blend_opaque" for the motion vectors (decals, lights and
   // translucents keep the motion vectors of what's behind them, and so do colorless draws), and "mv_reactive_blend": 0 not alpha
   // blended (opaque, additive lights and emissive passes ONE / ONE), 1 alpha blended (SRC_ALPHA or, premultiplied, ONE / INV_SRC_ALPHA:
   // smoke, glass, water; reactive and transparency & composition), 2 additive (SRC_ALPHA / ONE: sparks, glows; reactive). REDengine's
   // translucents are premultiplied (ONE / INV_SRC_ALPHA, e.g. 0x171B986E): Mass Effect 2007's SRC_ALPHA-only test found none.
   static void ClassifyBoundBlend(ID3D11DeviceContext* native_device_context, TheWitcher2GameDeviceData* gd)
   {
      com_ptr<ID3D11BlendState> blend_state;
      native_device_context->OMGetBlendState(&blend_state, nullptr, nullptr);
      if (blend_state.get() == gd->mv_blend_state)
         return;
      D3D11_BLEND_DESC blend_desc = CD3D11_BLEND_DESC(D3D11_DEFAULT);
      if (blend_state)
      {
         blend_state->GetDesc(&blend_desc);
      }
      const D3D11_RENDER_TARGET_BLEND_DESC& rt0 = blend_desc.RenderTarget[0];
      gd->mv_blend_opaque = rt0.RenderTargetWriteMask != 0 && (!rt0.BlendEnable || (rt0.SrcBlend == D3D11_BLEND_ONE && rt0.DestBlend == D3D11_BLEND_ZERO && rt0.BlendOp == D3D11_BLEND_OP_ADD));
      const bool alpha_blended = rt0.BlendEnable && rt0.DestBlend == D3D11_BLEND_INV_SRC_ALPHA && (rt0.SrcBlend == D3D11_BLEND_SRC_ALPHA || rt0.SrcBlend == D3D11_BLEND_ONE);
      const bool additive = rt0.BlendEnable && rt0.SrcBlend == D3D11_BLEND_SRC_ALPHA && rt0.DestBlend == D3D11_BLEND_ONE;
      gd->mv_reactive_blend = (alpha_blended ? 1 : (additive ? 2 : 0));
      gd->mv_blend_state = blend_state.get();
   }

   // The upscaler's mip bias reaches only the scene's draws (Core swaps the game's samplers for biased ones): the shadow maps before the
   // scene, whose alpha tested foliage it thinned (smaller, flickering shadows, also at DLAA), and the post chain and the UI after it keep
   // the game's mips. The bound samplers are swapped right away: the scene opens inside its first draw, after Core's own check.
   static void SetMipBias(ID3D11DeviceContext* native_device_context, DeviceData& device_data, float bias)
   {
      if (custom_texture_mip_lod_bias_offset)
         return;
      const std::unique_lock lock(s_mutex_samplers);
      if (device_data.texture_mip_lod_bias_offset == bias)
         return;
      device_data.texture_mip_lod_bias_offset = bias;
      RebindUpgradedSamplers(native_device_context, device_data);
   }

   // Opens the scene at the frame's first mesh draw into output sized depth: takes the scene depth and picks the jitter the whole
   // scene draws with. Once per present (the HUD and later passes never reopen it). False if it didn't; a depth of another size is
   // remembered for the frame ("mv_not_scene_dsv").
   static bool OpenScene(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, CommandListData& cmd_list_data, DeviceData& device_data, uint32_t vertex_shader_hash, ID3D11DepthStencilView* dsv)
   {
      auto& gd = GetGameDeviceData(device_data);
      const uint2 depth_size = GetViewTextureSize(dsv);
      if (depth_size.x != device_data.output_resolution.x || depth_size.y != device_data.output_resolution.y)
      {
         gd.mv_not_scene_dsv = dsv;
         return false;
      }
      if (!GetPatchedVertexShader(native_device, cmd_list_data, device_data, vertex_shader_hash))
         return false;
      gd.mv_scene_open = true;
      SetMipBias(native_device_context, device_data, gd.scene_mip_bias);
#if DEVELOPMENT
      if (auto* const perf_queries = gd.perf_timestamps.frame)
      {
         perf_queries->Mark(native_device_context, PERF_SCENE_START);
      }
#endif
      gd.mv_depth.reset();
      dsv->GetResource(&gd.mv_depth);
      // FSR's masks target, the draws' (made here, under FSR with the masks on only: DLSS ignores them)
      if (gd.fsr_masks_active)
      {
         D3D11_TEXTURE2D_DESC desc = {};
         if (gd.mv_reactive_target)
         {
            gd.mv_reactive_target->GetDesc(&desc);
         }
         if (desc.Width != depth_size.x || desc.Height != depth_size.y)
         {
            const std::unique_lock lock(gd.mv_mutex);
            gd.ReleaseReactiveMasks();
            const CD3D11_TEXTURE2D_DESC reactive_desc(DXGI_FORMAT_R8G8_UNORM, depth_size.x, depth_size.y, 1, 1, D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE);
            if (SUCCEEDED(native_device->CreateTexture2D(&reactive_desc, nullptr, &gd.mv_reactive_target)) && SUCCEEDED(native_device->CreateRenderTargetView(gd.mv_reactive_target.get(), nullptr, &gd.mv_reactive_target_rtv)))
            {
               native_device->CreateShaderResourceView(gd.mv_reactive_target.get(), nullptr, &gd.mv_reactive_target_srv);
            }
            gd.sr_reactive_frame = cb_luma_global_settings.FrameIndex;
         }
      }
      gd.jitter_dsv = nullptr;
      gd.jitter_depth_stencil_state = nullptr;
      gd.jitter_depth_test = true;
      gd.mv_accepted_rtv = nullptr;
      gd.mv_accepted_dsv = nullptr;
      gd.mv_linear_depth_rtv = nullptr; // A view recreated at the same address (resize) would keep the old texture
      gd.mv_scene_color_rtv.reset();
      gd.mv_seen_rtv = nullptr;
      gd.mv_gbuffer_rtvs = {};
      gd.mv_reactive_rtv = nullptr;
      gd.mv_blend_state = nullptr;
      gd.mv_blend_opaque = true;
      gd.mv_reactive_blend = 0;
      // Halton (2, 3) over the upscaler's phase count; pixels to NDC (y up). Only after a frame the upscaler drew ("force_reset_sr" is
      // set at present when it didn't): not while the bridge's helper starts (the scene shows as it is, antialiased with SMAA), not
      // when the motion vector fill can't run (no upscaler then), not after a frame whose scene didn't reach the exposure (menus,
      // loading screens). SR stays latched active until the next present after "None" is picked: no implementation then, nor
      // instance data.
      const SR::InstanceData* sr_instance_data = (IsSRActive(device_data) ? device_data.GetSRInstanceData() : nullptr);
      if (sr_instance_data && !sr_implementations[device_data.sr_type]->IsReady(sr_instance_data))
      {
         sr_instance_data = nullptr;
      }
      const unsigned int phase = cb_luma_global_settings.FrameIndex % (sr_instance_data ? (std::max)(sr_implementations[device_data.sr_type]->GetJitterPhases(sr_instance_data), 1) : SR::GetDefaultJitterPhases());
      const bool upscaled_last_frame = sr_instance_data && !device_data.force_reset_sr;
      const bool jitter = (upscaled_last_frame || g_mv_force_jitter) && !g_mv_disable_jitter && gd.mv_previous_scene_done && GetPerfMotionVectorDraws() != 0;
      gd.mv_jitter = (jitter ? std::array<float, 2>{SR::HaltonSequence(phase, 2), SR::HaltonSequence(phase, 3)} : std::array<float, 2>{});
#if DEVELOPMENT
      // The upscaler's start as it happens: when the jitter turns on or off, and why
      if (jitter != gd.mv_logged_jitter)
      {
         gd.mv_logged_jitter = jitter;
         reshade::log::message(reshade::log::level::info, std::format("[TW2 SR] jitter {} at frame {} (upscaler active {}, ready {}, upscaled last frame {}, last scene done {})",
                                                             jitter ? "on" : "off", cb_luma_global_settings.FrameIndex, IsSRActive(device_data), sr_instance_data != nullptr,
                                                             upscaled_last_frame, gd.mv_previous_scene_done)
                                                             .c_str());
      }
#endif
      // The jitter is in render area pixels (NDC spans the area); zw: the area's share of the surface, for "Luma_TW2_RenderArea.hlsl"
      gd.mv_render_size = (RenderArea::render_size[0] != 0 ? RenderArea::render_size : std::array<uint32_t, 2>{depth_size.x, depth_size.y});
      gd.mv_jitter_ndc = {gd.mv_jitter[0] * 2.f / float(gd.mv_render_size[0]), gd.mv_jitter[1] * -2.f / float(gd.mv_render_size[1])};
      const float ndc_jitter[4] = {gd.mv_jitter_ndc[0], gd.mv_jitter_ndc[1], float(gd.mv_render_size[0]) / float(depth_size.x), float(gd.mv_render_size[1]) / float(depth_size.y)};
      if (!PatchedDraws::WriteDynamicConstants(native_device, native_device_context, std::addressof(gd.mv_jitter_buffer), ndc_jitter, sizeof(ndc_jitter)))
      {
         // No stale jitter on the scene draws either: no motion vectors this frame
         gd.mv_jitter = {};
         gd.mv_jitter_ndc = {};
         gd.mv_jitter_buffer.reset();
      }
      return true;
   }

#if DEVELOPMENT
#define MV_REJECT(reason) \
   ([&](auto& gd) { gd.mv_stats.rejected[size_t(reason)]++; gd.mv_draw_reject = int(reason); return false; }(GetGameDeviceData(device_data)))
#else
#define MV_REJECT(reason) false
#endif

   // A texture Core's SR bridge hands to its helper as is (NT handle shared), else a plain one (it copies those)
   static HRESULT CreateSharableTexture(ID3D11Device* native_device, D3D11_TEXTURE2D_DESC desc, ID3D11Texture2D** texture)
   {
      desc.MiscFlags |= D3D11_RESOURCE_MISC_SHARED | D3D11_RESOURCE_MISC_SHARED_NTHANDLE;
      if (SUCCEEDED(native_device->CreateTexture2D(&desc, nullptr, texture)))
         return S_OK;
      desc.MiscFlags &= ~(D3D11_RESOURCE_MISC_SHARED | D3D11_RESOURCE_MISC_SHARED_NTHANDLE);
      return native_device->CreateTexture2D(&desc, nullptr, texture);
   }

   // The game's targets with one of ours at "slot" (the motion vector target, or the masks target past it), unless already bound
   static void BindWithExtraTarget(ID3D11DeviceContext* native_device_context, const com_ptr<ID3D11RenderTargetView> (&rtvs)[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT], UINT slot,
      ID3D11RenderTargetView* extra, ID3D11DepthStencilView* dsv)
   {
      if (rtvs[slot].get() == extra)
         return;
      ID3D11RenderTargetView* targets[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = {};
      for (UINT i = 0; i < slot; i++)
      {
         targets[i] = rtvs[i].get();
      }
      targets[slot] = extra;
      native_device_context->OMSetRenderTargets(slot + 1, targets, dsv);
   }

   // Draws an opaque draw into output sized targets with the scene depth (the G-buffer: albedo, specular, normals, linear depth; or
   // an opaque draw into the lit scene) with the patched shaders, adding the motion vector target ("target_slot", past the game's)
   // and the previous frame's vc4 ("previous_slots"). False if it can't (the draw then goes to "DrawWithJitter").
   static bool DrawWithMotionVectors(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, CommandListData& cmd_list_data, DeviceData& device_data,
      const ShaderHashesList<OneShaderPerPipeline>& original_shader_hashes, const std::function<void()>& draw, const com_ptr<ID3D11RenderTargetView> (&rtvs)[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT],
      ID3D11DepthStencilView* dsv)
   {
      auto& gd = GetGameDeviceData(device_data);
      // The game's targets (D3D9's 4 at most), plus the motion vector and mask targets the last patched draws left bound
      for (UINT slot = MotionVectorPatches::target_slot; slot < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; slot++)
      {
         if (rtvs[slot] && (slot != MotionVectorPatches::target_slot || rtvs[slot] != gd.mv_rtv) && (slot != MotionVectorPatches::reactive_slot || rtvs[slot] != gd.mv_reactive_target_rtv))
            return MV_REJECT(MotionVectorReject::EXTRA_TARGET);
      }
      if (!rtvs[0] || !dsv || !gd.mv_scene_open)
         return MV_REJECT(MotionVectorReject::NO_SCENE);
      ClassifyBoundBlend(native_device_context, &gd);
      if (!gd.mv_blend_opaque)
         return MV_REJECT(MotionVectorReject::BLEND);
      // Known targets: checked, and the motion vector target built for them
      if (rtvs[0] != gd.mv_accepted_rtv || dsv != gd.mv_accepted_dsv)
      {
         com_ptr<ID3D11Resource> depth;
         dsv->GetResource(&depth);
         com_ptr<ID3D11Resource> color;
         rtvs[0]->GetResource(&color);
         if (depth != gd.mv_depth || !color)
            return MV_REJECT(MotionVectorReject::OTHER_DEPTH);
         com_ptr<ID3D11Texture2D> color_texture;
         D3D11_TEXTURE2D_DESC color_desc = {};
         if (SUCCEEDED(color->QueryInterface(&color_texture)))
         {
            color_texture->GetDesc(&color_desc);
         }
         // Filtered by the resource, not the view (dgVoodoo binds single-slice array views)
         if (color_desc.ArraySize != 1 || color_desc.SampleDesc.Count != 1)
            return MV_REJECT(MotionVectorReject::ARRAY_OR_MSAA);
         const uint2 size = {color_desc.Width, color_desc.Height};
         if (size.x != device_data.output_resolution.x || size.y != device_data.output_resolution.y)
            return MV_REJECT(MotionVectorReject::SIZE);
         const std::unique_lock lock(gd.mv_mutex);
         D3D11_TEXTURE2D_DESC desc = {};
         if (gd.mv_texture)
         {
            gd.mv_texture->GetDesc(&desc);
         }
         // R16G16_FLOAT: FSR keeps 16 bits internally; the error is under 0.1% of the motion (BL GOTY)
         constexpr DXGI_FORMAT format = DXGI_FORMAT_R16G16_FLOAT;
         if (desc.Width != size.x || desc.Height != size.y)
         {
            gd.ReleaseMotionVectorTargets();
            // The fill reads the target back through its UAV
            D3D11_FEATURE_DATA_FORMAT_SUPPORT2 support = {format};
            const bool typed_uav_load = SUCCEEDED(native_device->CheckFeatureSupport(D3D11_FEATURE_FORMAT_SUPPORT2, &support, sizeof(support))) && (support.OutFormatSupport2 & D3D11_FORMAT_SUPPORT2_UAV_TYPED_LOAD) != 0;
            desc = CD3D11_TEXTURE2D_DESC(format, size.x, size.y, 1, 1, D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | (typed_uav_load ? D3D11_BIND_UNORDERED_ACCESS : 0u));
            if (FAILED(CreateSharableTexture(native_device, desc, &gd.mv_texture)) || FAILED(native_device->CreateRenderTargetView(gd.mv_texture.get(), nullptr, &gd.mv_rtv)))
            {
               gd.mv_texture.reset();
               gd.mv_rtv.reset();
               return MV_REJECT(MotionVectorReject::CREATE);
            }
            if (typed_uav_load)
            {
               native_device->CreateUnorderedAccessView(gd.mv_texture.get(), nullptr, &gd.mv_uav);
            }
            const CD3D11_TEXTURE2D_DESC depth_desc(DXGI_FORMAT_R32_FLOAT, size.x, size.y, 1, 1, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS);
            if (SUCCEEDED(CreateSharableTexture(native_device, depth_desc, &gd.mv_device_depth)))
            {
               native_device->CreateUnorderedAccessView(gd.mv_device_depth.get(), nullptr, &gd.mv_device_depth_uav);
            }
            gd.mv_frame_ended = true;
         }
         gd.mv_accepted_rtv = rtvs[0].get();
         gd.mv_accepted_dsv = dsv;
      }
      // The G-buffer's targets (the reactive masks skip the draws into them), and its linear view depth (the fill's input). Only from
      // the G-buffer's own (4 targets) draws: the opaque forward draws into the lit scene take this path too, and so does water (the
      // scene and the linear depth).
      if (rtvs[3])
      {
         for (UINT slot = 0; slot < MotionVectorPatches::target_slot; slot++)
         {
            gd.mv_gbuffer_rtvs[slot] = rtvs[slot].get();
         }
         if (rtvs[3].get() != gd.mv_linear_depth_rtv)
         {
            gd.mv_linear_depth_rtv = rtvs[3].get();
            D3D11_RENDER_TARGET_VIEW_DESC rtv_desc;
            rtvs[3]->GetDesc(&rtv_desc);
            com_ptr<ID3D11Resource> linear_depth;
            rtvs[3]->GetResource(&linear_depth);
            if (rtv_desc.Format == DXGI_FORMAT_R32_FLOAT && linear_depth != gd.mv_linear_depth)
            {
               gd.mv_linear_depth = linear_depth;
               gd.mv_linear_depth_srv.reset();
            }
         }
      }

      ID3D11VertexShader* const vertex_shader = GetPatchedVertexShader(native_device, cmd_list_data, device_data, original_shader_hashes.vertex_shaders[0]);
      if (const uint32_t pixel_shader_hash = original_shader_hashes.pixel_shaders[0]; pixel_shader_hash != gd.mv_last_pixel_shader_hash)
      {
         gd.mv_last_pixel = GetMotionVectorShader(native_device, device_data, &gd.mv_pixel_shaders, pixel_shader_hash, cmd_list_data.pipeline_state_original_pixel_shader);
         gd.mv_last_pixel_shader_hash = pixel_shader_hash;
      }
      ID3D11PixelShader* const pixel_shader = gd.mv_last_pixel.shader.get();
      if (!vertex_shader || !pixel_shader || !gd.mv_jitter_buffer)
         return MV_REJECT(MotionVectorReject::SHADERS);
      if (std::exchange(gd.mv_frame_ended, false))
      {
         gd.mv_fill_pending = gd.mv_uav && gd.mv_device_depth_uav && FindShader(device_data.native_compute_shaders, CompileTimeStringHash("TW2 Motion Vector Fill CS")) != nullptr;
         // The fill's marker: the largest float16 (a larger clear value is stored as it in R16G16_FLOAT)
         const FLOAT clear_value = (gd.mv_fill_pending ? 65504.f : 0.f);
         const FLOAT clear[4] = {clear_value, clear_value, 0.f, 0.f};
         native_device_context->ClearRenderTargetView(gd.mv_rtv.get(), clear);
         if (gd.mv_reactive_target_rtv)
         {
            const FLOAT zero[4] = {};
            native_device_context->ClearRenderTargetView(gd.mv_reactive_target_rtv.get(), zero);
         }
         // Last frame's camera and objects are the previous ones, unless frames without a scene (menus, videos) came between
         const bool previous_valid = gd.mv_camera && cb_luma_global_settings.FrameIndex - gd.mv_frame_index <= 1;
         gd.mv_previous_camera = (previous_valid ? gd.mv_camera : nullptr);
         gd.mv_camera = nullptr;
         gd.mv_frame_index = cb_luma_global_settings.FrameIndex;
         // Swapped, not rebuilt: the lists keep their nodes and capacity (an empty list matches nothing); keys drawn in neither of
         // the last two frames go
         gd.mv_previous_objects.swap(gd.mv_objects);
#if DEVELOPMENT
         // Objects without a transform (world space vertices) match by their draw key alone: any twin has the same vertices
         gd.mv_stats.tiebreak_collisions = PatchedDraws::CountTieBreakCollisions(gd.mv_previous_objects, [](const auto& a, const auto& b)
            { return a.transform == PatchedDraws::ObjectTransform{} || PatchedDraws::SameBytes(a.constants, b.constants); });
#endif
         std::erase_if(gd.mv_objects, [](const auto& entry)
            { return entry.second.empty(); });
         for (auto& entry : gd.mv_objects)
         {
            entry.second.clear();
         }
         if (!previous_valid)
         {
            gd.mv_previous_objects.clear();
         }
      }

      // The game's vc4 (object, camera and bones in one). The slots added past it stay bound after the draw: no translated shader
      // reads a constant buffer past b5.
      com_ptr<ID3D11Buffer> current;
      native_device_context->VSGetConstantBuffers(MotionVectorPatches::object_slot, 1, &current);
#if DEVELOPMENT
      if (gd.mv_previous_constants.ring_context)
      {
         com_ptr<ID3D11Buffer> bound;
         UINT first_constant = 0, constant_count = 0;
         gd.mv_previous_constants.ring_context->VSGetConstantBuffers1(MotionVectorPatches::object_slot, 1, &bound, &first_constant, &constant_count);
         gd.mv_stats.offset_bindings += first_constant != 0;
      }
#endif
      TheWitcher2GameDeviceData::ConstantsCopy constants;
      // What the next frame keeps of this draw's vc4: only the bytes its vertex shader reads (at least the camera), so the buffer's
      // full copy goes back to the registry alone and its next upload rewrites it in place. Skinned draws read it all and share it.
      TheWitcher2GameDeviceData::ConstantsCopy object_constants;
      {
         const std::lock_guard lock(gd.mv_constants_mutex);
         // The buffer's CPU copy (null until its first upload); the lookup registers it for a copy at every upload
         if (current)
         {
            const auto [copy, registered] = gd.mv_constants_copies.try_emplace(reinterpret_cast<uint64_t>(current.get()));
            constants = copy->second;
            if (registered)
            {
               AddFilteredBuffer(&gd, current.get(), &copy->second);
            }
         }
         if (constants && constants->size() >= VIEW_PROJECTION_OFFSET + CAMERA_SIZE)
         {
            const size_t read_size = (gd.mv_last_vertex.read_size != 0 ? std::clamp<size_t>(gd.mv_last_vertex.read_size, VIEW_PROJECTION_OFFSET + CAMERA_SIZE, constants->size()) : constants->size());
            object_constants = (read_size == constants->size() ? constants : NewConstantsCopy(&gd, constants->data(), read_size));
         }
      }
      // The previous frame's vc4: the same object's from last frame, else this draw's with last frame's camera (no object motion).
      // None (no CPU copy yet, another camera): the current one (zero motion).
      const std::vector<uint8_t>* upload = nullptr;
      if (object_constants)
      {
         // The frame's camera: its first motion vector draw's
         if (!gd.mv_camera)
         {
            gd.mv_camera = constants;
         }
         const bool frame_camera = std::memcmp(constants->data() + VIEW_PROJECTION_OFFSET, gd.mv_camera->data() + VIEW_PROJECTION_OFFSET, 4 * 16) == 0;

         // Draw key: same mesh, same shaders, no instance count
         com_ptr<ID3D11Buffer> vertex_buffer;
         UINT vertex_stride = 0, vertex_offset = 0;
         native_device_context->IAGetVertexBuffers(0, 1, &vertex_buffer, &vertex_stride, &vertex_offset);
         com_ptr<ID3D11Buffer> index_buffer;
         DXGI_FORMAT index_format;
         UINT index_offset = 0;
         native_device_context->IAGetIndexBuffer(&index_buffer, &index_format, &index_offset);
         const DrawDispatchData& draw_data = last_draw_dispatch_data;
         uint64_t key = 0;
         for (const uint64_t value : {uint64_t(original_shader_hashes.vertex_shaders[0]), uint64_t(original_shader_hashes.pixel_shaders[0]), reinterpret_cast<uint64_t>(vertex_buffer.get()),
                 uint64_t(vertex_offset), reinterpret_cast<uint64_t>(index_buffer.get()), uint64_t(index_offset), uint64_t(draw_data.index_count), uint64_t(draw_data.first_index),
                 uint64_t(uint32_t(draw_data.vertex_offset)), uint64_t(draw_data.vertex_count), uint64_t(draw_data.first_vertex)})
         {
            HashCombine(key, value);
         }
         // LocalToWorld (skinned: bone 0) separates objects that share a key (props, characters), as a tie-break only (see
         // "PatchedDraws::ObjectTransform")
         PatchedDraws::ObjectTransform transform = {};
         if (const size_t transform_offset = gd.mv_last_vertex.transform_offset; transform_offset != 0 && constants->size() >= transform_offset + sizeof(transform))
         {
            std::memcpy(transform.data(), constants->data() + transform_offset, sizeof(transform));
         }

         // ponytail: linear search among the key's candidates (a handful at most); a spatial lookup if big crowds share a mesh
         const TheWitcher2GameDeviceData::MotionVectorObject* match = nullptr;
         const auto previous = gd.mv_previous_objects.find(key);
         // Without a tie-break transform (a vertex shader reading neither LocalToWorld nor bones at the rows the tie-break knows) the
         // copies of a mesh sharing a key can't be told apart, so any pick is a guess. Such a draw keeps its own constants with last
         // frame's camera (static).
         const bool untransformed = gd.mv_last_vertex.transform_offset == 0;
#if DEVELOPMENT
         if (untransformed && previous != gd.mv_previous_objects.end() && previous->second.size() > 1)
         {
            gd.mv_stats.untransformed_ambiguous++;
            if (cb_luma_global_settings.FrameIndex - gd.mv_logged_untransformed_frame >= 300)
            {
               gd.mv_logged_untransformed_frame = cb_luma_global_settings.FrameIndex;
               reshade::log::message(reshade::log::level::info, std::format("[TW2 MV] frame {}: no tie-break transform, {} candidates, VS 0x{:08X} PS 0x{:08X}", cb_luma_global_settings.FrameIndex,
                                                                   previous->second.size(), uint32_t(original_shader_hashes.vertex_shaders[0]), uint32_t(original_shader_hashes.pixel_shaders[0]))
                                                                   .c_str());
            }
         }
#endif
         if (previous != gd.mv_previous_objects.end() && !(untransformed && g_mv_untransformed_static))
         {
            float nearest = FLT_MAX;
            for (const auto& candidate : previous->second)
            {
               const float distance = PatchedDraws::TransformDistance(candidate.transform, transform);
               if (candidate.constants->size() == object_constants->size() && distance < nearest)
               {
                  nearest = distance;
                  match = &candidate;
               }
            }
            // Terrain doesn't move: a chunk not drawn last frame (the chunk set changes even with a still camera) would take its
            // nearest neighbor's position and heightmap window, all its ground moving by hundreds of pixels
            if (match && nearest != 0.f && gd.mv_last_vertex.transform_offset == TERRAIN_CHUNK_ROW * 16)
            {
               match = nullptr;
#if DEVELOPMENT
               gd.mv_stats.terrain_inexact++;
               if (cb_luma_global_settings.FrameIndex - gd.mv_logged_terrain_frame >= 300)
               {
                  gd.mv_logged_terrain_frame = cb_luma_global_settings.FrameIndex;
                  reshade::log::message(reshade::log::level::info, std::format("[TW2 MV] frame {}: terrain chunk without last frame's, nearest distance {}, {} candidates, VS 0x{:08X}", cb_luma_global_settings.FrameIndex,
                                                                      nearest, previous->second.size(), uint32_t(original_shader_hashes.vertex_shaders[0]))
                                                                      .c_str());
               }
#endif
            }
#if DEVELOPMENT
            if (match && nearest != 0.f)
            {
               auto& inexact = gd.mv_inexact_by_vs[uint32_t(original_shader_hashes.vertex_shaders[0])];
               inexact.count++;
               inexact.max_distance = (std::max)(inexact.max_distance, nearest);
            }
            if (cb_luma_global_settings.FrameIndex - gd.mv_logged_inexact_frame >= 300)
            {
               gd.mv_logged_inexact_frame = cb_luma_global_settings.FrameIndex;
               std::vector<std::pair<uint32_t, TheWitcher2GameDeviceData::InexactMatches>> sorted(gd.mv_inexact_by_vs.begin(), gd.mv_inexact_by_vs.end());
               std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b)
                  { return a.second.count > b.second.count; });
               std::string line = std::format("[TW2 MV] frame {}: inexact matches over 300 frames (VS count max_distance):", cb_luma_global_settings.FrameIndex);
               for (size_t i = 0; i < (std::min<size_t>)(sorted.size(), 16); i++)
               {
                  line += std::format(" 0x{:08X} {} {:.3g};", sorted[i].first, sorted[i].second.count, sorted[i].second.max_distance);
               }
               reshade::log::message(reshade::log::level::info, line.c_str());
               gd.mv_inexact_by_vs.clear();
            }
#endif
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
            // Not found, drawn with the frame's camera: its own constants with last frame's camera (camera motion only)
            gd.mv_camera_only_copy.assign(object_constants->begin(), object_constants->end());
            std::memcpy(gd.mv_camera_only_copy.data() + VIEW_PROJECTION_OFFSET, gd.mv_previous_camera->data() + VIEW_PROJECTION_OFFSET, CAMERA_SIZE);
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
         gd.mv_objects[key].push_back({transform, std::move(object_constants)});
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
      gd.mv_previous_constants.Bind(native_device, native_device_context, MotionVectorPatches::previous_slots, {&upload, 1}, previous_current, "TW2", {&gd.mv_last_vertex.read_size, 1});
      ID3D11Buffer* const jitter = gd.mv_jitter_buffer.get();
      native_device_context->VSSetConstantBuffers(MotionVectorPatches::jitter_slot, 1, &jitter);
      // The textures a vertex shader samples (the terrain's heightmap: static) as the previous frame's too. Unbound, the second run
      // read 0 there: the terrain's previous position at height 0, motion vectors of several screens on all the ground.
      if (gd.mv_last_vertex.reads_resources)
      {
         ID3D11ShaderResourceView* resources[MotionVectorPatches::resource_slots] = {};
         native_device_context->VSGetShaderResources(0, MotionVectorPatches::resource_slots, resources);
         native_device_context->VSSetShaderResources(MotionVectorPatches::previous_resources_slot, MotionVectorPatches::resource_slots, resources);
         for (ID3D11ShaderResourceView* resource : resources)
         {
            if (resource)
            {
               resource->Release();
            }
         }
      }
      // Left bound after the draw (set directly, bypassing Core's state tracking): the game's next draws either bind their own
      // targets and shaders, or are motion vector draws too. No translated pixel shader writes past o3, so the target keeps its
      // contents.
      BindWithExtraTarget(native_device_context, rtvs, MotionVectorPatches::target_slot, gd.mv_rtv.get(), dsv);
      PatchedDraws::BindPatchedShader(native_device_context, vertex_shader, &gd.mv_bound_vertex_shader);
      PatchedDraws::BindPatchedShader(native_device_context, pixel_shader, &gd.mv_bound_pixel_shader);

      draw();
#if DEVELOPMENT
      gd.mv_stats.motion_vector_draws++;
#endif
      return true;
   }

   // Jitter for the scene's mesh draws without motion vectors (patched vertex shader, game pixel shader): lights, decals, translucents.
   // Every draw depth tested against the scene takes the same jitter, or jittered and unjittered depths of the same surface fail
   // each other's test. The deferred lights read the G-buffer at their pixel ("vPos"), so the jitter doesn't misplace them. False if
   // it can't (the draw runs untouched).
   static bool DrawWithJitter(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, CommandListData& cmd_list_data, DeviceData& device_data,
      const ShaderHashesList<OneShaderPerPipeline>& original_shader_hashes, const std::function<void()>& draw, const com_ptr<ID3D11RenderTargetView> (&rtvs)[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT],
      ID3D11DepthStencilView* dsv)
   {
      auto& gd = GetGameDeviceData(device_data);
      // Without jitter only for FSR's masks (their draws come through here)
      if (!gd.mv_scene_open || (gd.mv_jitter == std::array<float, 2>{} && !gd.fsr_masks_active) || !gd.mv_jitter_buffer)
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
         D3D11_DEPTH_STENCIL_DESC depth_desc = CD3D11_DEPTH_STENCIL_DESC(D3D11_DEFAULT);
         if (depth_stencil_state)
         {
            depth_stencil_state->GetDesc(&depth_desc);
         }
         gd.jitter_depth_test = depth_desc.DepthEnable;
         gd.jitter_depth_stencil_state = depth_stencil_state.get();
      }
      if (!gd.jitter_depth_test)
         return false;
      com_ptr<ID3D11Buffer> vertex_buffer;
      UINT vertex_stride, vertex_offset;
      native_device_context->IAGetVertexBuffers(0, 1, &vertex_buffer, &vertex_stride, &vertex_offset);
      if (!vertex_buffer)
         return false;
      ID3D11VertexShader* const vertex_shader = GetPatchedVertexShader(native_device, cmd_list_data, device_data, original_shader_hashes.vertex_shaders[0]);
      if (!vertex_shader)
         return false;

      // An alpha blended draw into an output sized scene target (not the G-buffer's: decals) writes its mask, reactive or
      // transparency & composition (its pixel shader patched, the mask target added past the motion vector one)
      ID3D11PixelShader* reactive_shader = nullptr;
      if (gd.fsr_masks_active && gd.mv_reactive_target_rtv && rtvs[0])
      {
         if (rtvs[0].get() != gd.mv_reactive_rtv)
         {
            gd.mv_reactive_rtv = rtvs[0].get();
            const uint2 size = GetViewTextureSize(rtvs[0].get());
            gd.mv_reactive_rtv_scene = std::ranges::find(gd.mv_gbuffer_rtvs, rtvs[0].get()) == gd.mv_gbuffer_rtvs.end() && size.x == device_data.output_resolution.x && size.y == device_data.output_resolution.y;
         }
         ClassifyBoundBlend(native_device_context, &gd);
         if (const uint8_t blend = gd.mv_reactive_blend; blend != 0 && gd.mv_reactive_rtv_scene)
         {
            reactive_shader = GetMotionVectorShader(native_device, device_data, &gd.mv_reactive_pixel_shaders[blend - 1], original_shader_hashes.pixel_shaders[0], cmd_list_data.pipeline_state_original_pixel_shader, blend).shader.get();
         }
      }

      // The patched vertex shader and the jitter stay bound after the draw (see "DrawWithMotionVectors"), with the game's pixel
      // shader (a motion vector draw's is put back) or its reactive version, and the mask target
      PatchedDraws::BindPatchedShader(native_device_context, vertex_shader, &gd.mv_bound_vertex_shader);
      ID3D11Buffer* const jitter = gd.mv_jitter_buffer.get();
      native_device_context->VSSetConstantBuffers(MotionVectorPatches::jitter_slot, 1, &jitter);
      if (reactive_shader)
      {
         BindWithExtraTarget(native_device_context, rtvs, MotionVectorPatches::reactive_slot, gd.mv_reactive_target_rtv.get(), dsv);
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

   // The projection's depth row from WorldToScreen (column vectors: clip = M * p, rows c4-c7): row 2 = A * row 3 + (0, 0, 0, B) for an
   // affine view, so device depth = A + B / view depth (row 3 gives the view depth, as the G-buffer's linear depth)
   static std::array<double, 2> GetDepthFromView(const float* view_projection)
   {
      double dot_23 = 0.0, dot_33 = 0.0;
      for (int column = 0; column < 3; column++)
      {
         dot_23 += double(view_projection[8 + column]) * view_projection[12 + column];
         dot_33 += double(view_projection[12 + column]) * view_projection[12 + column];
      }
      const double a = (dot_33 > 0.0 ? dot_23 / dot_33 : 1.0);
      return {a, double(view_projection[11]) - a * view_projection[15]};
   }

   // DLAA or FSR 3 Native AA on the exposed, jittered scene (the exposure's target), or under the render scale DLSS/FSR on the linear
   // scene's render area before the exposure (with the exposure the fill wrote); its depth (from the G-buffer's, see the fill) and the
   // motion vectors. The result goes back into that target, its alpha kept. False if it didn't draw (missing input, or the upscaler
   // failed). "reactive_mask": the fill wrote this scene's masks.
   static bool DrawUpscaler(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, DeviceData& device_data, ID3D11RenderTargetView* scene_rtv, bool reactive_mask)
   {
      auto& gd = GetGameDeviceData(device_data);
      auto* const copy_vs = FindShader(device_data.native_vertex_shaders, CompileTimeStringHash("Copy VS"));
      auto* const copy_ps = FindShader(device_data.native_pixel_shaders, CompileTimeStringHash("Copy PS"));
      if (!scene_rtv || !gd.mv_texture || !gd.mv_camera || !gd.mv_device_depth || !copy_vs || !copy_ps)
         return false;
      com_ptr<ID3D11Resource> scene_resource;
      scene_rtv->GetResource(&scene_resource);
      com_ptr<ID3D11Texture2D> scene;
      if (!scene_resource || FAILED(scene_resource->QueryInterface(&scene)))
         return false;
      D3D11_TEXTURE2D_DESC scene_desc;
      scene->GetDesc(&scene_desc);
      if (scene_desc.Width != uint32_t(device_data.output_resolution.x) || scene_desc.Height != uint32_t(device_data.output_resolution.y) || scene_desc.ArraySize != 1 || scene_desc.SampleDesc.Count != 1)
         return false;
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
         if (SUCCEEDED(CreateSharableTexture(native_device, output_desc, &device_data.sr_output_color)))
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

      // FSR needs the camera. WorldToScreen's row 1 is the up axis / tan(fov / 2).
      const float* const view_projection = reinterpret_cast<const float*>(gd.mv_camera->data() + VIEW_PROJECTION_OFFSET);
      const double up_length = std::sqrt(double(view_projection[4]) * view_projection[4] + double(view_projection[5]) * view_projection[5] + double(view_projection[6]) * view_projection[6]);
      const double vert_fov = (up_length > 0.0 ? 2.0 * std::atan(1.0 / up_length) : 0.0);
      const auto [depth_a, depth_b] = GetDepthFromView(view_projection);
      const double near_plane = (depth_a != 0.0 ? -depth_b / depth_a : 0.0);

      const uint32_t render_width = (std::min)(gd.mv_render_size[0], scene_desc.Width);
      const uint32_t render_height = (std::min)(gd.mv_render_size[1], scene_desc.Height);
      const SR::SettingsData settings_data = {
         .output_width = scene_desc.Width,
         .output_height = scene_desc.Height,
         .render_width = render_width,
         .render_height = render_height,
         .hdr = true,
         // The motion vectors are UV deltas of the render area, previous minus current
         .mvs_x_scale = float(render_width),
         .mvs_y_scale = float(render_height),
         // DLSS's own (DLSS-Best-Practices EXP-4 canon; presets L and M ignore the texture anyway); FSR's clips highlights
         // (FSR-Best-Practices FIN-3), it takes the tonemap's exposure (FIN-4)
         .auto_exposure = device_data.sr_type != SR::Type::FSR,
         .render_preset = dlss_render_preset,
      };
      sr_implementations[device_data.sr_type]->UpdateSettings(sr_instance_data, native_device_context, settings_data);

      // FSR requires a FOV (it errors on 0): a camera without an up axis keeps the last one
      if (vert_fov > 0.0)
      {
         gd.sr_vert_fov = float(vert_fov);
      }
      SR::SuperResolutionImpl::DrawData draw_data = {
         .reset = device_data.force_reset_sr || gd.sr_render_size != std::array<uint32_t, 2>{render_width, render_height},
         .output_color = device_data.sr_output_color.get(),
         .source_color = scene.get(),
         .motion_vectors = gd.mv_texture.get(),
         .depth_buffer = gd.mv_device_depth.get(),
         .exposure = gd.sr_exposure.get(),
         .bias_mask = (reactive_mask ? gd.mv_reactive.get() : nullptr),
         .transparency_alpha = (reactive_mask ? gd.mv_transparency.get() : nullptr),
         .render_width = render_width,
         .render_height = render_height,
         // As applied (pixels, +y down)
         .jitter_x = gd.mv_jitter[0],
         .jitter_y = gd.mv_jitter[1],
         .vert_fov = gd.sr_vert_fov,
      };
      if (near_plane > 0.0)
      {
         // A finite far (depth 1) when the projection has one, else a large one (FSR's context is FFX_FSR3_ENABLE_DEPTH_INFINITE only
         // with inverted depth)
         const double far_plane = (depth_a > 1.0 + 1e-6 ? depth_b / (1.0 - depth_a) : near_plane * 1e6);
         draw_data.near_plane = float(near_plane);
         draw_data.far_plane = float(far_plane);
      }
#if DEVELOPMENT
      gd.mv_stats.near_plane = draw_data.near_plane;
      gd.mv_stats.far_plane = draw_data.far_plane;
#endif
      if (!sr_implementations[device_data.sr_type]->Draw(sr_instance_data, native_device_context, draw_data))
      {
         // Back to SMAA until the upscaler is picked again
         device_data.sr_suppressed = true;
         return false;
      }
#if DEVELOPMENT
      if (auto* const perf_queries = gd.perf_timestamps.frame; perf_queries && perf_queries->Marked(PERF_FILL_END))
      {
         perf_queries->Mark(native_device_context, PERF_UPSCALER_END);
      }
#endif
      gd.sr_render_size = {render_width, render_height};
      // The scene's alpha (passed through by the exposure) kept by the RGB write mask
      DrawCustomPixelShader(native_device_context, device_data.default_depth_stencil_state.get(), gd.sr_rgb_blend_state.get(), nullptr, copy_vs, copy_ps, gd.sr_output_srv.get(), scene_rtv, scene_desc.Width, scene_desc.Height);
      // Not while the bridge's helper starts (the color copied as it is): SMAA stays on and the next frame resets
      device_data.has_drawn_sr = sr_implementations[device_data.sr_type]->IsReady(sr_instance_data);
#if DEVELOPMENT
      if (device_data.has_drawn_sr && device_data.force_reset_sr)
      {
         reshade::log::message(reshade::log::level::info, std::format("[TW2 SR] upscaled frame {} after frames without it (history reset)", cb_luma_global_settings.FrameIndex).c_str());
      }
#endif
      return true;
   }

   // Ends the scene right after the exposure drew into "scene_rtv" (null: no upscaler), or under the render scale before the post
   // chain's first pass with the linear scene: the depth and camera motion fill (see "Luma_TW2_MotionVectorFill.hlsl"), then the
   // upscaler, before anything reads the scene. Under the render scale, then the linear depth stretched over its surface.
   static void EndScene(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, DeviceData& device_data, ID3D11RenderTargetView* scene_rtv, bool exposed)
   {
      auto& gd = GetGameDeviceData(device_data);
      gd.mv_scene_open = false;
      gd.mv_scene_done = true;
      SetMipBias(native_device_context, device_data, 0.f);
#if DEVELOPMENT
      auto* const perf_queries = gd.perf_timestamps.frame;
      if (perf_queries && perf_queries->Marked(PERF_SCENE_START))
      {
         perf_queries->Mark(native_device_context, PERF_SCENE_END);
      }
#endif
      DrawStateStack<DrawStateStackType::FullGraphics> graphics_state;
      DrawStateStack<DrawStateStackType::Compute> compute_state;
      graphics_state.Cache(native_device_context, device_data.uav_max_count);
      compute_state.Cache(native_device_context, device_data.uav_max_count);
      auto* const fill_shader = FindShader(device_data.native_compute_shaders, CompileTimeStringHash("TW2 Motion Vector Fill CS"));
      if (gd.mv_linear_depth && !gd.mv_linear_depth_srv)
      {
         // dgVoodoo's targets are single-slice arrays: a plain 2D view of the first slice
         const CD3D11_SHADER_RESOURCE_VIEW_DESC srv_desc(D3D11_SRV_DIMENSION_TEXTURE2D, DXGI_FORMAT_R32_FLOAT);
         native_device->CreateShaderResourceView(gd.mv_linear_depth.get(), &srv_desc, &gd.mv_linear_depth_srv);
      }
      // The reactive and transparency & composition masks, written by the fill from what the alpha blended draws wrote. FSR only:
      // DLSS's current presets ignore them (DLSS-Best-Practices TRN-2)
      const bool reactive = gd.fsr_masks_active && gd.mv_reactive_target_srv;
      if (reactive)
      {
         D3D11_TEXTURE2D_DESC desc = {};
         if (gd.mv_reactive)
         {
            gd.mv_reactive->GetDesc(&desc);
         }
         if (desc.Width != uint32_t(device_data.output_resolution.x) || desc.Height != uint32_t(device_data.output_resolution.y))
         {
            gd.mv_reactive.reset();
            gd.mv_reactive_uav.reset();
            gd.mv_transparency.reset();
            gd.mv_transparency_uav.reset();
            desc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R8_UNORM, uint32_t(device_data.output_resolution.x), uint32_t(device_data.output_resolution.y), 1, 1, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS);
            if (SUCCEEDED(CreateSharableTexture(native_device, desc, &gd.mv_reactive)))
            {
               native_device->CreateUnorderedAccessView(gd.mv_reactive.get(), nullptr, &gd.mv_reactive_uav);
            }
            if (SUCCEEDED(CreateSharableTexture(native_device, desc, &gd.mv_transparency)))
            {
               native_device->CreateUnorderedAccessView(gd.mv_transparency.get(), nullptr, &gd.mv_transparency_uav);
            }
         }
      }
      const bool write_reactive = reactive && gd.mv_reactive_uav && gd.mv_transparency_uav;
      // The upscaler's exposure: on the linear scene (the post chain's entry) the last adaptation's gain (1 with the static exposure
      // perms, which have no adaptation texture), on the exposure's output 1. Always given: the SR bridge restarts its helper when an
      // input comes or goes.
      const bool scaled = gd.mv_render_size != std::array<uint32_t, 2>{uint32_t(device_data.output_resolution.x), uint32_t(device_data.output_resolution.y)};
      if (IsSRActive(device_data) && !gd.sr_exposure_uav)
      {
         gd.sr_exposure.reset();
         if (SUCCEEDED(CreateSharableTexture(native_device, CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R32_FLOAT, 1, 1, 1, 1, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS), &gd.sr_exposure)))
         {
            native_device->CreateUnorderedAccessView(gd.sr_exposure.get(), nullptr, &gd.sr_exposure_uav);
         }
      }
      const bool game_exposure = !exposed && gd.sr_exposure_constants && (gd.sr_exposure_static || gd.sr_adaptation_srv);
      if (write_reactive)
      {
         gd.sr_reactive_frame = cb_luma_global_settings.FrameIndex;
      }
      bool filled = false;
      if (std::exchange(gd.mv_fill_pending, false) && fill_shader && gd.mv_camera && gd.mv_linear_depth_srv)
      {
         // Current clip space to the previous frame's: previous * inverse(current), column vectors as vc4 holds them, in double
         // (absolute world translation)
         const float* const view_projection = reinterpret_cast<const float*>(gd.mv_camera->data() + VIEW_PROJECTION_OFFSET);
         Math::Matrix44D current, previous;
         current.SetIdentity();
         previous.SetIdentity();
         if (gd.mv_previous_camera)
         {
            std::copy_n(view_projection, 16, current.GetData());
            std::copy_n(reinterpret_cast<const float*>(gd.mv_previous_camera->data() + VIEW_PROJECTION_OFFSET), 16, previous.GetData());
            current.Invert();
         }
         const Math::Matrix44D reprojection = previous * current;
         const auto depth_from_view = GetDepthFromView(view_projection);
         CB::MotionVectorFillConstants constants = {
            .jitter_ndc = {gd.mv_jitter_ndc[0], gd.mv_jitter_ndc[1]},
            .depth_from_view = {float(depth_from_view[0]), float(depth_from_view[1])},
            .reactive_scale = g_sr_reactive_scale,
            .reactive_threshold = g_sr_reactive_threshold,
            .reactive_enabled = (write_reactive ? 1.f : 0.f),
            .exposure_enabled = (game_exposure ? 1.f : 0.f),
            .render_size = {float(gd.mv_render_size[0]), float(gd.mv_render_size[1])},
            .user_exposure = (GetShaderDefineCompiledNumericalValue(char_ptr_crc32("TONEMAP_TYPE")) >= 1 ? cb_luma_global_settings.GameSettings.Exposure : 1.f),
            .exposure_static = (gd.sr_exposure_static ? 1.f : 0.f),
         };
         for (int i = 0; i < 16; i++)
         {
            constants.reprojection.GetData()[i] = float(reprojection.GetData()[i]);
         }
         if (PatchedDraws::WriteDynamicConstants(native_device, native_device_context, std::addressof(gd.mv_fill_buffer), &constants, sizeof(constants)))
         {
            // The G-buffer, the motion vectors and the scene may be bound as render targets
            native_device_context->OMSetRenderTargets(0, nullptr, nullptr);
            ID3D11Buffer* const buffers[2] = {gd.mv_fill_buffer.get(), game_exposure ? gd.sr_exposure_constants.get() : nullptr};
            ID3D11ShaderResourceView* const srvs[3] = {gd.mv_linear_depth_srv.get(), write_reactive ? gd.mv_reactive_target_srv.get() : nullptr, game_exposure ? gd.sr_adaptation_srv.get() : nullptr};
            ID3D11UnorderedAccessView* const uavs[5] = {gd.mv_uav.get(), gd.mv_device_depth_uav.get(), write_reactive ? gd.mv_reactive_uav.get() : nullptr, write_reactive ? gd.mv_transparency_uav.get() : nullptr,
               gd.sr_exposure_uav.get()};
            native_device_context->CSSetConstantBuffers(0, UINT(std::size(buffers)), buffers);
            native_device_context->CSSetShaderResources(0, UINT(std::size(srvs)), srvs);
            native_device_context->CSSetUnorderedAccessViews(0, UINT(std::size(uavs)), uavs, nullptr);
            native_device_context->CSSetShader(fill_shader, nullptr, 0);
            native_device_context->Dispatch((gd.mv_render_size[0] + 7) / 8, (gd.mv_render_size[1] + 7) / 8, 1);
            ID3D11UnorderedAccessView* const null_uavs[std::size(uavs)] = {};
            ID3D11ShaderResourceView* const null_srvs[std::size(srvs)] = {};
            native_device_context->CSSetUnorderedAccessViews(0, UINT(std::size(null_uavs)), null_uavs, nullptr);
            native_device_context->CSSetShaderResources(0, UINT(std::size(null_srvs)), null_srvs);
            filled = true;
#if DEVELOPMENT
            if (perf_queries && perf_queries->Marked(PERF_SCENE_END))
            {
               perf_queries->Mark(native_device_context, PERF_FILL_END);
            }
#endif
         }
      }
      // The upscaler's depth comes from the fill
      if (IsSRActive(device_data) && filled)
      {
         [[maybe_unused]] const bool drawn = DrawUpscaler(native_device, native_device_context, device_data, scene_rtv, write_reactive);
#if DEVELOPMENT
         gd.mv_stats.sr_draws += drawn;
#endif
      }
      // Render scale: DoF and the light shafts read the G-buffer's linear depth over the whole surface, the scene drew its area. The
      // upscaler's depth is the fill's, so stretched (nearest) after it, from a copy.
      auto* const stretch_ps = FindShader(device_data.native_pixel_shaders, CompileTimeStringHash("TW2 Render Area Depth Stretch PS"));
      auto* const copy_vs = FindShader(device_data.native_vertex_shaders, CompileTimeStringHash("Copy VS"));
      com_ptr<ID3D11Texture2D> linear_depth;
      if (scaled && gd.mv_linear_depth && gd.mv_jitter_buffer && stretch_ps && copy_vs && SUCCEEDED(gd.mv_linear_depth->QueryInterface(&linear_depth)))
      {
         D3D11_TEXTURE2D_DESC depth_desc, copy_desc = {};
         linear_depth->GetDesc(&depth_desc);
         if (gd.render_area_depth_copy)
         {
            gd.render_area_depth_copy->GetDesc(&copy_desc);
         }
         if (copy_desc.Width != depth_desc.Width || copy_desc.Height != depth_desc.Height || copy_desc.Format != depth_desc.Format)
         {
            gd.render_area_depth_copy.reset();
            gd.render_area_depth_copy_srv.reset();
            gd.render_area_depth_rtv.reset();
            copy_desc = depth_desc;
            copy_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            copy_desc.MiscFlags = 0;
            copy_desc.CPUAccessFlags = 0;
            copy_desc.Usage = D3D11_USAGE_DEFAULT;
            // dgVoodoo's targets are single-slice arrays: plain 2D views of the first slice
            if (SUCCEEDED(native_device->CreateTexture2D(&copy_desc, nullptr, &gd.render_area_depth_copy)))
            {
               const CD3D11_SHADER_RESOURCE_VIEW_DESC srv_desc(D3D11_SRV_DIMENSION_TEXTURE2D, DXGI_FORMAT_R32_FLOAT);
               const CD3D11_RENDER_TARGET_VIEW_DESC rtv_desc(D3D11_RTV_DIMENSION_TEXTURE2D, DXGI_FORMAT_R32_FLOAT);
               native_device->CreateShaderResourceView(gd.render_area_depth_copy.get(), &srv_desc, &gd.render_area_depth_copy_srv);
               native_device->CreateRenderTargetView(linear_depth.get(), &rtv_desc, &gd.render_area_depth_rtv);
            }
         }
         if (gd.render_area_depth_copy_srv && gd.render_area_depth_rtv)
         {
            native_device_context->CopyResource(gd.render_area_depth_copy.get(), linear_depth.get());
            ID3D11Buffer* const area_scale = gd.mv_jitter_buffer.get();
            native_device_context->PSSetConstantBuffers(0, 1, &area_scale);
            DrawCustomPixelShader(native_device_context, device_data.default_depth_stencil_state.get(), device_data.default_blend_state.get(), nullptr, copy_vs, stretch_ps, gd.render_area_depth_copy_srv.get(), gd.render_area_depth_rtv.get(), depth_desc.Width, depth_desc.Height);
         }
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

   // The post chain starts (the game thread, outside any D3D11 call): the scene ends here, upscaled (over the whole surface from a
   // shrunk area under the render scale) from its last target (the scene color, linear) before any post pass reads it
   static void ResolveRenderArea()
   {
      const std::shared_lock lock(s_mutex_device);
      if (global_devices_data.empty())
         return;
      DeviceData& device_data = *global_devices_data[0];
      auto& gd = GetGameDeviceData(device_data);
      if (!gd.mv_scene_open)
         return;
      ID3D11RenderTargetView* const scene_rtv = gd.mv_scene_color_rtv.get();
      com_ptr<ID3D11DeviceContext> native_device_context;
      device_data.native_device->GetImmediateContext(&native_device_context);
#if DEVELOPMENT
      gd.mv_stats.ended_by = uint32_t(reinterpret_cast<uintptr_t>(RenderArea::post_chain)) | (scene_rtv ? 0u : 0x80000000u);
#endif
      EndScene(device_data.native_device, native_device_context.get(), device_data, scene_rtv, false);
   }

#if DEVELOPMENT
   // A mode of the "Performance Test" combo. The upscaler latches at the next present ("sr_active"): the settle covers the frame in
   // between.
   static void ApplyPerfTestMode(DeviceData& device_data, int mode_index)
   {
      auto& gd = GetGameDeviceData(device_data);
      const PerfTestMode& mode = PERF_TEST_MODES[mode_index];
      const PerfTestMode& previous_mode = PERF_TEST_MODES[Perf::g_test];
      if (!previous_mode.set_aa && mode.set_aa)
      {
         gd.perf_user_sr_type = device_data.sr_type;
         gd.perf_user_smaa = g_smaa_enable;
      }
      if (mode.set_aa || previous_mode.set_aa)
      {
         SetSRType(device_data, (mode.set_aa ? mode.sr_type : gd.perf_user_sr_type));
         g_smaa_enable = (mode.set_aa ? mode.smaa : gd.perf_user_smaa);
      }
      const auto sets_cpu = [](const PerfTestMode& test_mode)
      { return test_mode.vc4_filter_off || test_mode.vc4_pool_off || test_mode.vc4_slots_off; };
      if (!sets_cpu(previous_mode) && sets_cpu(mode))
      {
         gd.perf_user_vc4_filter = g_mv_buffer_filter;
         gd.perf_user_vc4_pool = g_mv_constants_pool;
         gd.perf_user_vc4_slots = g_mv_vc4_slots;
      }
      if (sets_cpu(mode) || sets_cpu(previous_mode))
      {
         g_mv_buffer_filter = gd.perf_user_vc4_filter && !mode.vc4_filter_off;
         g_mv_constants_pool = gd.perf_user_vc4_pool && !mode.vc4_pool_off;
         g_mv_vc4_slots = gd.perf_user_vc4_slots && !mode.vc4_slots_off;
      }
      Perf::g_test = mode_index;
   }

#endif

   // Every blend state writes the motion vector target ("MotionVectorPatches::target_slot", bound only by the motion vector draws)
   // unblended, and max blends the mask target: no per-draw copy of the game's state. ReShade turns independent blending on only when
   // a target now differs.
   static bool OnCreateBlendState(reshade::api::device* device, reshade::api::pipeline_layout layout, uint32_t subobject_count, const reshade::api::pipeline_subobject* subobjects)
   {
      for (uint32_t i = 0; i < subobject_count; i++)
      {
         if (subobjects[i].type != reshade::api::pipeline_subobject_type::blend_state)
            continue;
         auto& desc = *static_cast<reshade::api::blend_desc*>(subobjects[i].data);
         // dgVoodoo sometimes leaves blending on for a secondary render target while RT0 has it off. D3D9 has one global blend state
         // and only per-RT write masks (D3DRS_COLORWRITEENABLE1/2/3), so the game never asked for it and that target is corrupted.
         // Flotsam water (PS 0xDA16C815): RT1 is the r32_float linear depth fog reads, and the shader ends "mov o1.xyzw, v7.xxxx", so
         // src_alpha is the depth. RT0's blend goes to the game's other targets, their write masks stay (legal per-RT in D3D9). An
         // unbound target's blend does nothing, so repairing every state equals repairing the bound targets of each draw.
         if (!desc.blend_enable[0])
         {
            for (uint32_t rt = 1; rt < MotionVectorPatches::target_slot; rt++)
            {
               if (!desc.blend_enable[rt])
                  continue;
               desc.blend_enable[rt] = false;
               desc.logic_op_enable[rt] = desc.logic_op_enable[0];
               desc.source_color_blend_factor[rt] = desc.source_color_blend_factor[0];
               desc.dest_color_blend_factor[rt] = desc.dest_color_blend_factor[0];
               desc.color_blend_op[rt] = desc.color_blend_op[0];
               desc.source_alpha_blend_factor[rt] = desc.source_alpha_blend_factor[0];
               desc.dest_alpha_blend_factor[rt] = desc.dest_alpha_blend_factor[0];
               desc.alpha_blend_op[rt] = desc.alpha_blend_op[0];
               desc.logic_op[rt] = desc.logic_op[0];
            }
         }
         desc.blend_enable[MotionVectorPatches::target_slot] = false;
         desc.render_target_write_mask[MotionVectorPatches::target_slot] = 0xF;
         // The masks (reactive x, transparency & composition y): the strongest alpha blended draw per pixel (max never exceeds what one
         // wrote)
         constexpr uint32_t reactive = MotionVectorPatches::reactive_slot;
         desc.blend_enable[reactive] = true;
         desc.source_color_blend_factor[reactive] = desc.dest_color_blend_factor[reactive] = reshade::api::blend_factor::one;
         desc.source_alpha_blend_factor[reactive] = desc.dest_alpha_blend_factor[reactive] = reshade::api::blend_factor::one;
         desc.color_blend_op[reactive] = desc.alpha_blend_op[reactive] = reshade::api::blend_op::max;
         desc.render_target_write_mask[reactive] = 0x3;
         return true;
      }
      return false;
   }

   static bool CreateImmutableCB(ID3D11Device* device, const void* data, UINT size, ComPtr<ID3D11Buffer>* out)
   {
      out->reset();
      const D3D11_BUFFER_DESC bd = {.ByteWidth = size, .Usage = D3D11_USAGE_IMMUTABLE, .BindFlags = D3D11_BIND_CONSTANT_BUFFER};
      const D3D11_SUBRESOURCE_DATA sd = {.pSysMem = data};
      return SUCCEEDED(device->CreateBuffer(&bd, &sd, out->put()));
   }

   static bool CreateDefaultTex(ID3D11Device* device, uint32_t w, uint32_t h, UINT bind_flags, ComPtr<ID3D11Texture2D>* out, DXGI_FORMAT format)
   {
      out->reset();
      const CD3D11_TEXTURE2D_DESC td(format, w, h, 1, 1, bind_flags);
      return SUCCEEDED(device->CreateTexture2D(&td, nullptr, out->put()));
   }

#if ENABLE_SMAA
   // SMAA on the graded gamma canvas, after the original final-grade draw and before the UI draws on it; the
   // replaced grade skipped its own FXAA via LumaData.CustomData2. Without "smaa" (the upscaler antialiased the frame, Mass Effect
   // 2007's shape) only RCAS runs: snapshot -> RCAS -> canvas.
   void RunPostFinalGradeSMAA(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, DeviceData& device_data, TheWitcher2GameDeviceData* gd, ID3D11Resource* canvas_res, ID3D11RenderTargetView* canvas_rtv, bool smaa)
   {
      uint4 cinfo{};
      DXGI_FORMAT cfmt = DXGI_FORMAT_UNKNOWN;
      GetResourceInfo(canvas_res, cinfo, cfmt);
      uint32_t w = cinfo.x, h = cinfo.y;
      if (w == 0 || h == 0 || cfmt == DXGI_FORMAT_UNKNOWN)
      {
         return;
      }

      // One resolution latch: every surface here is canvas-sized, so a change drops the lot (Core's SMAA intermediates follow the size
      // themselves), and the per-pointer checks below rebuild it.
      if (gd->scratch_w != w || gd->scratch_h != h)
      {
         gd->ReleaseSMAAScratch();
         gd->ReleasePredicationScratch();
         gd->cb_smaa_metrics.reset(); // it holds the resolution itself
         gd->scratch_w = w;
         gd->scratch_h = h;
      }

      // RCAS (Copy VS + RCAS PS, see "sharpen"), resolved first: it decides whether the last SMAA pass writes the canvas RTV
      // directly, which removes both the copy back and the intermediate.
      auto* sharpen_vs = FindShader(device_data.native_vertex_shaders, CompileTimeStringHash("Copy VS"));
      auto* sharpen_ps = FindShader(device_data.native_pixel_shaders, CompileTimeStringHash("TW2 Sharpen PS"));
      bool do_sharpen = g_rcas_sharpness > 0.f && sharpen_vs != nullptr && sharpen_ps != nullptr;
      if (do_sharpen && (!gd->cb_sharpen || gd->sharpen_amount != g_rcas_sharpness))
      {
         const float sharpen_params[4] = {(float)w, (float)h, g_rcas_sharpness, 0.f};
         if (CreateImmutableCB(native_device, sharpen_params, sizeof(sharpen_params), std::addressof(gd->cb_sharpen)))
         {
            gd->sharpen_amount = g_rcas_sharpness;
         }
      }
      do_sharpen = do_sharpen && gd->cb_sharpen;
      const auto sharpen = [&](ID3D11ShaderResourceView* source)
      {
         DrawStateStack<DrawStateStackType::FullGraphics> sharpen_state;
         sharpen_state.Cache(native_device_context, device_data.uav_max_count);

         native_device_context->PSSetConstantBuffers(0, 1, gd->cb_sharpen.get_addressof());
         DrawCustomPixelShader(native_device_context, device_data.default_depth_stencil_state.get(), device_data.default_blend_state.get(), nullptr,
            sharpen_vs, sharpen_ps, source, canvas_rtv, w, h, false);

         sharpen_state.Restore(native_device_context);
      };

      if (!smaa)
      {
         if (!do_sharpen)
            return;
         if (!gd->tex_input && CreateDefaultTex(native_device, w, h, D3D11_BIND_SHADER_RESOURCE, std::addressof(gd->tex_input), cfmt))
         {
            native_device->CreateShaderResourceView(gd->tex_input.get(), nullptr, gd->srv_input.put());
         }
         if (!gd->srv_input)
            return;
         native_device_context->CopyResource(gd->tex_input.get(), canvas_res);
         gd->snapshot_frame = cb_luma_global_settings.FrameIndex;
         sharpen(gd->srv_input.get());
         return;
      }

      // Skip SMAA this frame if a pass is still missing (async loader / live reload).
      const bool smaa_ready = AllShadersReady(device_data.native_pixel_shaders, {CompileTimeStringHash("SMAA Edge Detection PS"), CompileTimeStringHash("SMAA Blending Weight Calculation PS"), CompileTimeStringHash("SMAA Neighborhood Blending PS")}) &&
                              AllShadersReady(device_data.native_vertex_shaders, {CompileTimeStringHash("SMAA Edge Detection VS"), CompileTimeStringHash("SMAA Blending Weight Calculation VS"), CompileTimeStringHash("SMAA Neighborhood Blending VS")});
      if (!smaa_ready)
      {
         return;
      }

      // Fall back to plain ULTRA when an input is missing (never scale 2.0 with a null texture) or the depth
      // size differs: the extract CS maps texels 1:1, so a mismatch reads a sub-rect and misaligns the mask.
      auto* pred_cs = FindShader(device_data.native_compute_shaders, CompileTimeStringHash("TW2 Depth Extract CS"));
      bool pred_ok = g_smaa_predication && gd->srv_scene_depth.get() != nullptr && pred_cs != nullptr;
      if (pred_ok)
      {
         pred_ok = GetViewTextureSize(gd->srv_scene_depth.get()) == uint2{w, h};
      }
      if (pred_ok)
      {
         if (!gd->cb_pred || gd->pred_tolerance != g_smaa_pred_tolerance)
         {
            const float pred_params[4] = {g_smaa_pred_tolerance, 0.f, 0.f, 0.f};
            if (CreateImmutableCB(native_device, pred_params, sizeof(pred_params), std::addressof(gd->cb_pred)))
            {
               gd->pred_tolerance = g_smaa_pred_tolerance;
            }
         }
         if (!gd->tex_pred && CreateDefaultTex(native_device, w, h, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, std::addressof(gd->tex_pred), DXGI_FORMAT_R16_FLOAT))
         {
            native_device->CreateUnorderedAccessView(gd->tex_pred.get(), nullptr, gd->uav_pred.put());
            native_device->CreateShaderResourceView(gd->tex_pred.get(), nullptr, gd->srv_pred.put());
         }
         pred_ok = gd->cb_pred && gd->uav_pred && gd->srv_pred;
      }

      // Metrics CB: predication scale 2.0 when active, else 1.0. Recreate on resolution or predication flip.
      const float pred_scale = (pred_ok ? 2.0f : 1.0f);
      if (!gd->cb_smaa_metrics || gd->smaa_metrics_pred_scale != pred_scale)
      {
         const float metrics[8] = {1.f / (float)w, 1.f / (float)h, (float)w, (float)h, pred_scale, 0.f, 0.f, 0.f};
         if (CreateImmutableCB(native_device, metrics, sizeof(metrics), std::addressof(gd->cb_smaa_metrics)))
         {
            gd->smaa_metrics_pred_scale = pred_scale;
         }
      }
      if (!gd->cb_smaa_metrics)
         return;

      if (do_sharpen)
      {
         if (!gd->tex_smaa_out && CreateDefaultTex(native_device, w, h, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET, std::addressof(gd->tex_smaa_out), cfmt))
         {
            native_device->CreateRenderTargetView(gd->tex_smaa_out.get(), nullptr, gd->tex_smaa_out_rtv.put());
            native_device->CreateShaderResourceView(gd->tex_smaa_out.get(), nullptr, gd->tex_smaa_out_srv.put());
         }
         if (!gd->tex_smaa_out_rtv || !gd->tex_smaa_out_srv)
         {
            do_sharpen = false;
         }
      }

      if (!gd->tex_input && CreateDefaultTex(native_device, w, h, D3D11_BIND_SHADER_RESOURCE, std::addressof(gd->tex_input), cfmt))
      {
         native_device->CreateShaderResourceView(gd->tex_input.get(), nullptr, gd->srv_input.put());
      }
      if (!gd->srv_input)
         return;
      gd->smaa_frame = cb_luma_global_settings.FrameIndex;
      gd->snapshot_frame = gd->smaa_frame;

      // Snapshot the canvas color: the chain writes the canvas, so it must sample this copy, not the canvas.
      native_device_context->CopyResource(gd->tex_input.get(), canvas_res);

      // Predication signal: the game's linear r32f depth -> plane-deviation edge-ness in R16F (gd->tex_pred);
      // see Luma_TW2_DepthExtract.hlsl for why this is an edge test rather than a depth rescale.
      if (pred_ok)
      {
         DrawStateStack<DrawStateStackType::Compute> pred_cs_state;
         pred_cs_state.Cache(native_device_context, device_data.uav_max_count);

         native_device_context->CSSetShaderResources(0, 1, gd->srv_scene_depth.get_addressof());
         native_device_context->CSSetUnorderedAccessViews(0, 1, gd->uav_pred.get_addressof(), nullptr);
         native_device_context->CSSetConstantBuffers(0, 1, gd->cb_pred.get_addressof());
         native_device_context->CSSetShader(pred_cs, nullptr, 0);
         native_device_context->Dispatch((w + 7) / 8, (h + 7) / 8, 1);

         pred_cs_state.Restore(native_device_context);
      }

      // SMAA (3 passes). Metrics CB at VS+PS b1 (DrawSMAA restores VS/PS/SRVs/RTs, not cbuffers).
      ComPtr<ID3D11Buffer> vs_cb1_orig, ps_cb1_orig;
      native_device_context->VSGetConstantBuffers(1, 1, vs_cb1_orig.put());
      native_device_context->PSGetConstantBuffers(1, 1, ps_cb1_orig.put());
      native_device_context->VSSetConstantBuffers(1, 1, gd->cb_smaa_metrics.get_addressof());
      native_device_context->PSSetConstantBuffers(1, 1, gd->cb_smaa_metrics.get_addressof());

      // The last pass of the chain renders straight into the canvas RTV — no write-back copy. Reading the
      // canvas is safe because SMAA/RCAS sample the snapshot (tex_input), never the canvas itself.
      DrawSMAA(native_device, native_device_context, device_data,
         do_sharpen ? gd->tex_smaa_out_rtv.get() : canvas_rtv, gd->srv_input.get(), gd->srv_input.get(),
         pred_ok ? gd->srv_pred.get() : nullptr /*predication signal*/);

      if (do_sharpen)
      {
         sharpen(gd->tex_smaa_out_srv.get());
      }

      native_device_context->VSSetConstantBuffers(1, 1, vs_cb1_orig.get_addressof());
      native_device_context->PSSetConstantBuffers(1, 1, ps_cb1_orig.get_addressof());
   }
#endif // ENABLE_SMAA

   // Take over the AO_GEN draw: 4 XeGTAO compute passes into our scratch, then CopyResource into the game's AO
   // RT so the vanilla chain keeps working. Inputs come from the hooked draw (depth SRV t0, RTV, cb4 for the
   // NDC->view ray scale); a missing one returns None so the native HBAO draw runs.
   DrawOrDispatchOverrideType RunXeGTAO(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, DeviceData& device_data, TheWitcher2GameDeviceData* gd)
   {
      // The four passes, null until the async loader (or a live reload) has them: the native draw runs then
      auto* cs_prefilter = FindShader(device_data.native_compute_shaders, CompileTimeStringHash("TW2 XeGTAO Prefilter Depths CS"));
      auto* cs_main = FindShader(device_data.native_compute_shaders, CompileTimeStringHash("TW2 XeGTAO Main Pass CS"));
      auto* cs_denoise_1 = FindShader(device_data.native_compute_shaders, CompileTimeStringHash("TW2 XeGTAO Denoise Pass 1 CS"));
      auto* cs_denoise_2 = FindShader(device_data.native_compute_shaders, CompileTimeStringHash("TW2 XeGTAO Denoise Pass 2 CS"));
      if (cs_prefilter == nullptr || cs_main == nullptr || cs_denoise_1 == nullptr || cs_denoise_2 == nullptr)
         return DrawOrDispatchOverrideType::None;

      // Game depth (the hooked draw's t0): half-res r32_float linear view-space depth.
      ComPtr<ID3D11ShaderResourceView> srv_depth;
      native_device_context->PSGetShaderResources(0, 1, srv_depth.put());
      if (!srv_depth)
         return DrawOrDispatchOverrideType::None;
      const auto [w, h] = GetViewTextureSize(srv_depth.get());
      if (w == 0 || h == 0)
         return DrawOrDispatchOverrideType::None;

      // The game's AO render target; must match the depth size (both half render res).
      ComPtr<ID3D11RenderTargetView> rtv;
      native_device_context->OMGetRenderTargets(1, rtv.put(), nullptr);
      if (!rtv)
         return DrawOrDispatchOverrideType::None;
      ComPtr<ID3D11Resource> rt_res;
      rtv->GetResource(rt_res.put());
      // Read the ACTUAL descriptor: our own indirect upgrade can make this RT rgba16_float while ReShade
      // metadata still reports the original, and CopyResource silently no-ops on a family mismatch (frozen AO).
      uint4 rt_info{};
      DXGI_FORMAT rt_fmt = DXGI_FORMAT_UNKNOWN;
      GetResourceInfo(rt_res.get(), rt_info, rt_fmt); // no resource reads as 0x0, which fails the size match
      if (rt_info.x != w || rt_info.y != h)
         return DrawOrDispatchOverrideType::None;
      // Typeless -> typed within the same family (so CopyResource stays legal), since our UAV-written copy source needs
      // a typed format. Typed UAV store is only guaranteed for these two, and they are the only formats the upgrade
      // list can produce; anything else would fail at CreateTexture2D.
      const DXGI_FORMAT final_fmt = (DXGI_FORMAT)reshade::api::format_to_default_typed((reshade::api::format)rt_fmt);
      if (final_fmt != DXGI_FORMAT_R8G8B8A8_UNORM && final_fmt != DXGI_FORMAT_R16G16B16A16_FLOAT)
         return DrawOrDispatchOverrideType::None;

      // The XeGTAO shader derives its NDC->view ray scale from the game's cb4 (bound to the PS stage at the
      // hooked draw); it is copied to the CS stage below (in-place takeover, same frame state).
      ComPtr<ID3D11Buffer> game_cb4;
      native_device_context->PSGetConstantBuffers(4, 1, game_cb4.put());
      if (!game_cb4)
         return DrawOrDispatchOverrideType::None;

      // (Re)create the scratch set on first use, resolution change, or RT format change (all-or-nothing).
      if (gd->gtao_w != w || gd->gtao_h != h || gd->gtao_final_fmt != final_fmt)
      {
         gd->ReleaseGTAOScratch();

         const CD3D11_TEXTURE2D_DESC td(DXGI_FORMAT_R32_FLOAT, w, h, 1, 5 /* XE_GTAO_DEPTH_MIP_LEVELS */, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS);
         bool ok = SUCCEEDED(native_device->CreateTexture2D(&td, nullptr, gd->tex_gtao_depth_mips.put()));
         for (UINT i = 0; ok && i < 5; i++)
         {
            const CD3D11_UNORDERED_ACCESS_VIEW_DESC ud(D3D11_UAV_DIMENSION_TEXTURE2D, td.Format, i);
            ok = SUCCEEDED(native_device->CreateUnorderedAccessView(gd->tex_gtao_depth_mips.get(), &ud, gd->gtao_depth_mip_uavs[i].put()));
         }
         ok = ok && SUCCEEDED(native_device->CreateShaderResourceView(gd->tex_gtao_depth_mips.get(), nullptr, gd->srv_gtao_depth_mips.put()));

         for (int i = 0; ok && i < 2; i++)
         {
            ok = ok && CreateDefaultTex(native_device, w, h, td.BindFlags, std::addressof(gd->tex_gtao_working[i]), DXGI_FORMAT_R8G8_UNORM);
            ok = ok && SUCCEEDED(native_device->CreateUnorderedAccessView(gd->tex_gtao_working[i].get(), nullptr, gd->uav_gtao_working[i].put()));
            ok = ok && SUCCEEDED(native_device->CreateShaderResourceView(gd->tex_gtao_working[i].get(), nullptr, gd->srv_gtao_working[i].put()));
         }

         // Final AO in the game's channel layout (.x = AO), in the RT's ACTUAL format so CopyResource is legal.
         // The shader writes a plain float4 UAV, valid against both unorm8 and fp16.
         ok = ok && CreateDefaultTex(native_device, w, h, td.BindFlags, std::addressof(gd->tex_gtao_final), final_fmt);
         ok = ok && SUCCEEDED(native_device->CreateUnorderedAccessView(gd->tex_gtao_final.get(), nullptr, gd->uav_gtao_final.put()));

         if (!ok)
         {
            gd->ReleaseGTAOScratch(); // drop the partials
         }
         // Commit the target triple either way: a null set under it then reads as "failed", with no retry.
         gd->gtao_w = w;
         gd->gtao_h = h;
         gd->gtao_final_fmt = final_fmt;
      }
      if (!gd->tex_gtao_final)
         return DrawOrDispatchOverrideType::None; // the allocation failed for this size and format: the native draw runs

#if DEVELOPMENT
      const float debug_view = (float)g_gtao_debug_view;
#else
      const float debug_view = 0.f;
#endif
      // Knobs + viewport CB. The viewport rides along so the shader never trusts game constants for it.
      const CB::GTAOKnobs knobs = {
         .final_value_power = g_gtao_final_value_power,
         .depth_scale = g_gtao_depth_scale,
         .radius_override = g_gtao_radius_override,
         .debug_view = debug_view,
         .viewport_pixel_size = {1.f / float(w), 1.f / float(h)},
         .area_scale = {(RenderArea::render_size[0] != 0 ? float(RenderArea::render_size[0]) / device_data.output_resolution.x : 1.f),
            (RenderArea::render_size[1] != 0 ? float(RenderArea::render_size[1]) / device_data.output_resolution.y : 1.f)},
         .noise_index = (IsSRActive(device_data) ? float(cb_luma_global_settings.FrameIndex % 64) : 0.f),
      };
      if (!PatchedDraws::WriteDynamicConstants(native_device, native_device_context, std::addressof(gd->cb_gtao), &knobs, sizeof(knobs)))
         return DrawOrDispatchOverrideType::None;

      DrawStateStack<DrawStateStackType::Compute> compute_state;
      compute_state.Cache(native_device_context, device_data.uav_max_count);

      native_device_context->CSSetConstantBuffers(4, 1, game_cb4.get_addressof());
      ID3D11Buffer* const knobs_cb = gd->cb_gtao.get();
      native_device_context->CSSetConstantBuffers(GTAO_KNOBS_CB_SLOT, 1, &knobs_cb);
      ID3D11SamplerState* point_sampler = device_data.sampler_state_point.get();
      native_device_context->CSSetSamplers(0, 1, &point_sampler);

      static constexpr std::array<ID3D11UnorderedAccessView*, 5> uav_nulls5 = {};
      static constexpr std::array<ID3D11ShaderResourceView*, 2> srv_nulls2 = {};

      // Prefilter game depth into the R32F mip pyramid; each thread covers 2x2 pixels.
      {
         native_device_context->CSSetShaderResources(0, 2, srv_nulls2.data());
         ID3D11UnorderedAccessView* uavs[5] = {gd->gtao_depth_mip_uavs[0].get(), gd->gtao_depth_mip_uavs[1].get(),
            gd->gtao_depth_mip_uavs[2].get(), gd->gtao_depth_mip_uavs[3].get(), gd->gtao_depth_mip_uavs[4].get()};
         native_device_context->CSSetUnorderedAccessViews(0, 5, uavs, nullptr);
         native_device_context->CSSetShaderResources(0, 1, srv_depth.get_addressof());
         native_device_context->CSSetShader(cs_prefilter, nullptr, 0);
         native_device_context->Dispatch((w + 15) / 16, (h + 15) / 16, 1);
         native_device_context->CSSetUnorderedAccessViews(0, 5, uav_nulls5.data(), nullptr);
      }
      // Bind each destination UAV before its source SRVs: D3D11 otherwise keeps the previous UAV hazard and
      // silently nulls the conflicting SRV. Normals are generated from depth inside the shader.
      {
         native_device_context->CSSetShaderResources(0, 2, srv_nulls2.data());
         native_device_context->CSSetUnorderedAccessViews(0, 1, gd->uav_gtao_working[0].get_addressof(), nullptr);
         native_device_context->CSSetShaderResources(0, 1, gd->srv_gtao_depth_mips.get_addressof());
         native_device_context->CSSetShader(cs_main, nullptr, 0);
         native_device_context->Dispatch((w + 7) / 8, (h + 7) / 8, 1);
      }
      // First denoiser writes working1, two horizontal pixels per thread.
      {
         native_device_context->CSSetShaderResources(0, 2, srv_nulls2.data());
         native_device_context->CSSetUnorderedAccessViews(0, 1, gd->uav_gtao_working[1].get_addressof(), nullptr);
         native_device_context->CSSetShaderResources(0, 1, gd->srv_gtao_working[0].get_addressof());
         native_device_context->CSSetShader(cs_denoise_1, nullptr, 0);
         native_device_context->Dispatch((w + 15) / 16, (h + 7) / 8, 1);
      }
      {
         native_device_context->CSSetShaderResources(0, 2, srv_nulls2.data());
         native_device_context->CSSetUnorderedAccessViews(0, 1, gd->uav_gtao_final.get_addressof(), nullptr);
         native_device_context->CSSetShaderResources(0, 1, gd->srv_gtao_working[1].get_addressof());
         native_device_context->CSSetShader(cs_denoise_2, nullptr, 0);
         native_device_context->Dispatch((w + 15) / 16, (h + 7) / 8, 1);
         native_device_context->CSSetUnorderedAccessViews(0, 1, uav_nulls5.data(), nullptr);
      }

      compute_state.Restore(native_device_context);

      // The destination is still bound as the draw's render target, and copying into an OM-bound resource is a
      // D3D11 hazard the runtime does not resolve: unbind around the copy, then restore the game's binding.
      {
         ComPtr<ID3D11DepthStencilView> dsv_orig;
         native_device_context->OMGetRenderTargets(0, nullptr, dsv_orig.put());
         native_device_context->OMSetRenderTargets(0, nullptr, nullptr);
         native_device_context->CopyResource(rt_res.get(), gd->tex_gtao_final.get());
         native_device_context->OMSetRenderTargets(1, rtv.get_addressof(), dsv_orig.get());
      }

      return DrawOrDispatchOverrideType::Replaced;
   }

public:
   void OnInit(bool async) override
   {
      RenderArea::resolve = &ResolveRenderArea;
      RenderArea::Install();
#if DEVELOPMENT
      // For the MCP "luma_dev_values" tool
      Mcp::RegisterToggles({{"smaa_enable", &g_smaa_enable}, {"smaa_predication", &g_smaa_predication}, {"gtao_enable", &g_gtao_enable}, { "hide_ui",
                               &g_hide_ui }});
      Mcp::RegisterValues({{"rcas_sharpness", &g_rcas_sharpness, 0.f, 1.f}, {"smaa_pred_tolerance", &g_smaa_pred_tolerance, 0.002f, 0.1f}, {"gtao_final_value_power", &g_gtao_final_value_power, 0.3f, 4.5f},
         {"gtao_depth_scale", &g_gtao_depth_scale, 0.01f, 200.f}, {"gtao_radius_override", &g_gtao_radius_override, 0.f, 5.f}});
      Mcp::RegisterInts({{"gtao_debug_view", &g_gtao_debug_view, 0, 4}});
      Mcp::RegisterTextures({MCP_GAME_TEXTURE("smaa.input", tex_input),
         MCP_GAME_TEXTURE("smaa.pred_mask", tex_pred),
         MCP_GAME_TEXTURE("smaa.output", tex_smaa_out),
         MCP_GAME_TEXTURE("gtao.depth_mips", tex_gtao_depth_mips),
         MCP_GAME_TEXTURE("gtao.output", tex_gtao_final)});
      Mcp::RegisterToggles({{"mv_enable", &g_mv_enable}, {"mv_debug_view", &g_mv_debug_view}, {"mv_force_jitter", &g_mv_force_jitter}, {"mv_disable_jitter", &g_mv_disable_jitter}, {"mv_untransformed_static", &g_mv_untransformed_static},
         {"sr_reactive_enable", &g_sr_reactive_enable}, { "sr_reactive_debug_view",
            &g_sr_reactive_debug_view }});
      Mcp::RegisterValues({{"sr_reactive_scale", &g_sr_reactive_scale, 0.f, 4.f}, {"sr_reactive_threshold", &g_sr_reactive_threshold, 0.f, 1.f}});
      Mcp::RegisterValues({{"render_scale", &g_render_scale, MIN_RENDER_SCALE, 1.f}});
      Mcp::RegisterToggles({{ "mv_vc4_slots",
         &g_mv_vc4_slots }});
      Mcp::RegisterToggles({{"mv_buffer_filter", &g_mv_buffer_filter}, {"mv_constants_pool", &g_mv_constants_pool}, { "perf_hook_timers",
                               &Perf::g_hook_timers }});
      // As the "Performance Test" combo: a mode cancels a running sweep; "perf_sweep" 1 starts the GPU "Sweep", 2 the "CPU Sweep", 0 stops it
      Mcp::RegisterInts({{"perf_test", &Perf::g_test, 0, int(std::size(PERF_TEST_MODES)) - 1, [](DeviceData& device_data, double value)
                            {
                               g_perf_sweep.Stop();
                               ApplyPerfTestMode(device_data, int(value));
                               return std::string();
                            }},
         { "perf_sweep",
            &g_perf_sweep.running,
            0,
            int(std::size(PERF_SWEEPS)),
            [](DeviceData& device_data, double value)
            {
               if (value == 0.0)
               {
                  g_perf_sweep.Stop();
                  ApplyPerfTestMode(device_data, 0);
               }
               else
               {
                  ApplyPerfTestMode(device_data, g_perf_sweep.Start(int(value) - 1));
               }
               return std::string();
            } }});
      Mcp::RegisterTextures({MCP_GAME_TEXTURE("mv.velocity", mv_texture),
         MCP_GAME_TEXTURE("mv.depth", mv_device_depth),
         MCP_GAME_TEXTURE("sr.reactive", mv_reactive),
         MCP_GAME_TEXTURE("sr.transparency", mv_transparency),
         MCP_GAME_TEXTURE("sr.draws_mask", mv_reactive_target)});
#endif
      // Game-specific HDR toggle, read by the tonemap, glow/shaft blend and final grade replacements.
      std::vector<ShaderDefineData> game_shader_defines_data = {
         {"TONEMAP_TYPE", '1', true, false, "0 - SDR: Vanilla (bit-exact reference)\n1 - HDR: extended native grade + MacLeod-Boynton hue + DICE display map", 1},
         {"XE_GTAO_QUALITY", '3', true, false, "XeGTAO quality (slice count)\n0 - Low\n1 - Medium\n2 - High\n3 - Very High\n4 - Ultra", 4},
      };
      shader_defines_data.append_range(game_shader_defines_data);
      assert(shader_defines_data.size() < MAX_SHADER_DEFINES);

      // DX9-era gamma-2.2 SDR: the scene is linear until the final grade's vMidtone power encodes it (1/2.2 in the
      // neutral environment), so the canvas the HUD blends onto is GAMMA, like vanilla. The final grade pre-scales by
      // GamePaperWhite/UIPaperWhite (UI_DRAW_TYPE 2) so the HUD lands at its own level.
      GetShaderDefineData(POST_PROCESS_SPACE_TYPE_HASH).SetDefaultValue('0');
      GetShaderDefineData(EARLY_DISPLAY_ENCODING_HASH).SetDefaultValue('0');
      GetShaderDefineData(VANILLA_ENCODING_TYPE_HASH).SetDefaultValue('1'); // Gamma 2.2 in and out
      GetShaderDefineData(GAMMA_CORRECTION_TYPE_HASH).SetDefaultValue('1');
      GetShaderDefineData(GAMUT_MAPPING_TYPE_HASH).SetDefaultValue('1'); // gamut-map wild colors in composition
      GetShaderDefineData(UI_DRAW_TYPE_HASH).SetDefaultValue('2');       // HUD gets its own UIPaperWhite + gamma blend

      // Manual Scene + UI Paper White sliders instead of the OS HDR reference level (UI default 203 nits, BT.2408).
      use_os_reference_white_level = false;

      // The game (via dgVoodoo) binds b0-b5 only (runtime-verified); b12/b13 are free for Luma.
      luma_settings_cbuffer_index = 13;
      luma_data_cbuffer_index = 12;
      luma_ui_cbuffer_index = -1;

      // Grade controls: Exposure in Luma_TW2_Tonemap.hlsl, the video knobs in Video_0x30BE6D87, the rest in the final
      // grade. Vanilla no-ops by default apart from Dithering and Video AutoHDR. No highlight-expansion knob: the fp16
      // overshoot already reaches ~2-6x.
      default_luma_global_game_settings.Exposure = 1.f;              // scene multiplier (1x)
      default_luma_global_game_settings.Saturation = 1.f;            // saturation around luminance
      default_luma_global_game_settings.HighlightDechroma = 0.f;     // off; only mandatory DICE/gamut desat applies
      default_luma_global_game_settings.Dithering = 1.f;             // animated triangular dither at output (HDR and SDR), anti-banding on
      default_luma_global_game_settings.VideoAutoHDREnable = 1.f;    // light AutoHDR on pre-rendered videos (HDR only)
      default_luma_global_game_settings.VideoAutoHDRBoost = 0.5f;    // highlight-expansion strength (peak ~165 nits at 0.5)
      default_luma_global_game_settings.VignetteIntensity = 1.f;     // vanilla vignette darkening (0 = none)
      default_luma_global_game_settings.Contrast = 1.f;              // slope contrast around 18% mid-gray (HDR path)
      default_luma_global_game_settings.BloomIntensity = 1.f;        // engine glow scale (0 = no halo around lights)
      default_luma_global_game_settings.ColorGradingIntensity = 1.f; // vanilla shadow/highlight split toning strength (0 = no split toning)
      cb_luma_global_settings.GameSettings = default_luma_global_game_settings;

#if ENABLE_SMAA
      // Core auto-registers the 6 SMAA passes. Both read the GAMMA canvas snapshot; the blend decodes it to linear itself.
      // RCAS sharpen PS (drawn via core "Copy VS" + DrawCustomPixelShader after SMAA).
      native_shaders_definitions.emplace(CompileTimeStringHash("TW2 Sharpen PS"),
         ShaderDefinition{"Luma_TW2_Sharpen", reshade::api::pipeline_subobject_type::pixel_shader, nullptr, "sharpen_ps"});
      // Depth-extract CS for SMAA predication: game r32f LINEAR view-space depth -> R16F edge-ness in [0,1].
      native_shaders_definitions.emplace(CompileTimeStringHash("TW2 Depth Extract CS"),
         ShaderDefinition("Luma_TW2_DepthExtract", reshade::api::pipeline_subobject_type::compute_shader));
#endif

      // XeGTAO compute passes (Luma_TW2_XeGTAO.hlsl); the two denoisers differ only by XE_GTAO_FINAL_APPLY.
      native_shaders_definitions.emplace(CompileTimeStringHash("TW2 XeGTAO Prefilter Depths CS"),
         ShaderDefinition{"Luma_TW2_XeGTAO", reshade::api::pipeline_subobject_type::compute_shader, nullptr, "prefilter_depths16x16_cs"});
      native_shaders_definitions.emplace(CompileTimeStringHash("TW2 XeGTAO Main Pass CS"),
         ShaderDefinition{"Luma_TW2_XeGTAO", reshade::api::pipeline_subobject_type::compute_shader, nullptr, "main_pass_cs"});
      native_shaders_definitions.emplace(CompileTimeStringHash("TW2 XeGTAO Denoise Pass 1 CS"),
         ShaderDefinition{"Luma_TW2_XeGTAO", reshade::api::pipeline_subobject_type::compute_shader, nullptr, "denoise_pass_cs", {{"XE_GTAO_FINAL_APPLY", "0"}}});
      native_shaders_definitions.emplace(CompileTimeStringHash("TW2 XeGTAO Denoise Pass 2 CS"),
         ShaderDefinition{"Luma_TW2_XeGTAO", reshade::api::pipeline_subobject_type::compute_shader, nullptr, "denoise_pass_cs", {{"XE_GTAO_FINAL_APPLY", "1"}}});

      // DLSS/FSR: their depth and the camera motion from the G-buffer's linear depth, the CPU copies of vc4 (dgVoodoo maps it or
      // updates it), and the motion vector target written by every blend state
      sr_game_tooltip = "Requires Luma-Upscaler.exe next to the game's exe.\n";
      native_shaders_definitions.emplace(CompileTimeStringHash("TW2 Motion Vector Fill CS"),
         ShaderDefinition("Luma_TW2_MotionVectorFill", reshade::api::pipeline_subobject_type::compute_shader));
      native_shaders_definitions.emplace(CompileTimeStringHash("TW2 Render Area Depth Stretch PS"),
         ShaderDefinition{"Luma_TW2_RenderArea", reshade::api::pipeline_subobject_type::pixel_shader, nullptr, "depth_stretch_ps"});
      reshade::register_event<reshade::addon_event::map_buffer_region>(OnMapBufferRegion);
      reshade::register_event<reshade::addon_event::unmap_buffer_region>(OnUnmapBufferRegion);
      reshade::register_event<reshade::addon_event::update_buffer_region>(OnUpdateBufferRegion);
      reshade::register_event<reshade::addon_event::create_pipeline>(OnCreateBlendState);
   }

   static void UnregisterEvents()
   {
      reshade::unregister_event<reshade::addon_event::map_buffer_region>(OnMapBufferRegion);
      reshade::unregister_event<reshade::addon_event::unmap_buffer_region>(OnUnmapBufferRegion);
      reshade::unregister_event<reshade::addon_event::update_buffer_region>(OnUpdateBufferRegion);
      reshade::unregister_event<reshade::addon_event::create_pipeline>(OnCreateBlendState);
   }

   void OnCreateDevice(ID3D11Device* native_device, DeviceData& device_data) override
   {
      device_data.game = new TheWitcher2GameDeviceData;
      device_data.taa_detected = true; // No TAA to replace, but Core's upscaler status checks for it
#if DEVELOPMENT
      // For the MCP "luma_dev_values" tool: the last complete frame's counts
      const auto& stats = GetGameDeviceData(device_data).mv_last_stats;
      Mcp::RegisterCounters({{"mv.draws", &stats.motion_vector_draws}, {"mv.jitter_draws", &stats.jitter_draws}, {"mv.reactive_draws", &stats.reactive_draws}, {"mv.matched", &stats.matched},
                               {"mv.camera_only", &stats.camera_only}, {"mv.untransformed_ambiguous", &stats.untransformed_ambiguous}, {"mv.terrain_inexact", &stats.terrain_inexact}, {"mv.other_camera", &stats.other_camera}, {"mv.uncopied", &stats.uncopied}, {"mv.maps", &stats.maps}, {"mv.updates", &stats.updates},
                               {"mv.other_maps", &stats.other_maps}, {"mv.sr_draws", &stats.sr_draws}, {"mv.offset_bindings", &stats.offset_bindings}, {"mv.constants_pool", &stats.constants_pool},
                               {"mv.tiebreak_collisions", &stats.tiebreak_collisions}, { "mv.ended_by_hash",
                                  &stats.ended_by }},
         &device_data);
      for (size_t i = 0; i < MOTION_VECTOR_REJECT_NAMES.size(); i++)
      {
         Mcp::RegisterCounter(std::string("mv.rejected.") + MOTION_VECTOR_REJECT_NAMES[i], &stats.rejected[i], &device_data);
      }
#endif
   }

   void OnDestroyDeviceData(DeviceData& device_data) override
   {
#if DEVELOPMENT
      Mcp::Unregister(&device_data);
#endif
      // GameDeviceData lacks a virtual destructor; delete through the concrete type to release derived members.
      delete static_cast<TheWitcher2GameDeviceData*>(device_data.game);
      device_data.game = nullptr;
   }

   DrawOrDispatchOverrideType OnDrawOrDispatch(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, CommandListData& cmd_list_data, DeviceData& device_data, reshade::api::shader_stage stages, const ShaderHashesList<OneShaderPerPipeline>& original_shader_hashes, bool is_custom_pass, bool& updated_cbuffers, std::function<void()>* original_draw_dispatch_func) override
   {
      auto& game_device_data = GetGameDeviceData(device_data);
      const bool is_final_grade = IsFinalGrade(original_shader_hashes);

      // After the final grade the HUD is the only alpha-blended geometry while the post chain is opaque, so
      // keying on blend state within this frame's post-grade span hides it without touching the scene.
      if (g_hide_ui && game_device_data.final_grade_fired_this_frame && !is_custom_pass && !is_final_grade)
      {
         ComPtr<ID3D11BlendState> blend_state;
         FLOAT blend_factor[4];
         UINT sample_mask = 0;
         native_device_context->OMGetBlendState(blend_state.put(), blend_factor, &sample_mask);
         if (blend_state)
         {
            D3D11_BLEND_DESC bd;
            blend_state->GetDesc(&bd);
            if (bd.RenderTarget[0].BlendEnable != FALSE)
               return DrawOrDispatchOverrideType::Skip;
         }
      }

      const bool is_immediate = native_device_context->GetType() == D3D11_DEVICE_CONTEXT_IMMEDIATE;
      // DLSS/FSR: the scene's draws with motion vectors or jitter (see "DrawWithMotionVectors"), the upscaler after the exposure
      if (game_device_data.mv_active && is_immediate && !game_device_data.mv_scene_done && !is_custom_pass && original_draw_dispatch_func && *original_draw_dispatch_func && (stages & reshade::api::shader_stage::vertex) == reshade::api::shader_stage::vertex)
      {
#if DEVELOPMENT
         const Perf::HookTimer perf_timer{&game_device_data.perf_window.hook_ns}; // The draws' own submission included
#endif
         // Before the scene opens (the shadow maps: ~1800 draws a frame) only the depth view matters, and a depth view already found not
         // to be the scene's this frame needs nothing more
         if (!game_device_data.mv_scene_open)
         {
            com_ptr<ID3D11DepthStencilView> dsv;
            native_device_context->OMGetRenderTargets(0, nullptr, &dsv);
            if (dsv && dsv.get() != game_device_data.mv_not_scene_dsv)
            {
               OpenScene(native_device, native_device_context, cmd_list_data, device_data, original_shader_hashes.vertex_shaders[0], dsv.get());
            }
         }
         if (game_device_data.mv_scene_open)
         {
            com_ptr<ID3D11RenderTargetView> rtvs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
            com_ptr<ID3D11DepthStencilView> dsv;
            native_device_context->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, &rtvs[0], &dsv);
            // Render scale: the scene color, the last output sized fp16 target the scene's draws wrote (the G-buffer's aren't fp16)
            if (rtvs[0] && rtvs[0].get() != game_device_data.mv_seen_rtv)
            {
               game_device_data.mv_seen_rtv = rtvs[0].get();
               D3D11_RENDER_TARGET_VIEW_DESC desc;
               rtvs[0]->GetDesc(&desc);
               const uint2 size = GetViewTextureSize(rtvs[0].get());
               if (desc.Format == DXGI_FORMAT_R16G16B16A16_FLOAT && size.x == uint32_t(device_data.output_resolution.x) && size.y == uint32_t(device_data.output_resolution.y))
               {
                  game_device_data.mv_scene_color_rtv = rtvs[0];
               }
            }
            const std::function<void()>& draw = *original_draw_dispatch_func;
            const bool motion_vectors = DrawWithMotionVectors(native_device, native_device_context, cmd_list_data, device_data, original_shader_hashes, draw, rtvs, dsv.get());
            const bool jitter = !motion_vectors && DrawWithJitter(native_device, native_device_context, cmd_list_data, device_data, original_shader_hashes, draw, rtvs, dsv.get());
#if DEVELOPMENT
            Mcp::Annotate(cmd_list_data, motion_vectors ? "mv" : (jitter ? "jitter" : "unpatched"));
            if (const int reject = std::exchange(game_device_data.mv_draw_reject, -1); reject >= 0)
            {
               Mcp::Annotate(cmd_list_data, "mv_reject", reject);
            }
#endif
            if (motion_vectors || jitter)
               return DrawOrDispatchOverrideType::Replaced;
         }
      }
      // Every draw without a patched shader: the game's own, if the last patched draw's are still bound
      if (is_immediate)
      {
         PatchedDraws::RestoreGameShader(native_device_context, &game_device_data.mv_bound_vertex_shader);
         PatchedDraws::RestoreGameShader(native_device_context, &game_device_data.mv_bound_pixel_shader);
      }

      if (IsTonemap(original_shader_hashes))
      {
         // One permutation, two roles: the MAIN grade draws at swapchain size or larger, aux draws feed DoF/flare
         // smaller. Both stay bit-exact vanilla; the role only picks which draw marks main post processing.
         ComPtr<ID3D11RenderTargetView> rtv;
         native_device_context->OMGetRenderTargets(1, rtv.put(), nullptr);
         const uint2 rt_info = GetViewTextureSize(rtv.get()); // no render target reads as 0x0: not main
         // Matching aspect plus at-least-swapchain size excludes the smaller aux targets. (UberSampling doesn't enlarge the
         // targets: it renders the scene N x N times at output size, see NOTES.md.)
         const UINT out_w = (UINT)device_data.output_resolution.x;
         const UINT out_h = (UINT)device_data.output_resolution.y;
         const bool is_main = rt_info.x >= out_w && rt_info.y >= out_h && out_h != 0 && rt_info.y != 0 && fabsf(((float)rt_info.x / (float)rt_info.y) - ((float)out_w / (float)out_h)) < 0.05f;

         if (is_main)
         {
            device_data.has_drawn_main_post_processing = true;
         }

#if DEVELOPMENT
         if (is_main && ContainsPixelShader(original_shader_hashes, TONEMAP_EXPOSURE))
         {
            LogVanillaTonemap(native_device, native_device_context, &game_device_data);
         }
#endif

#if ENABLE_SMAA
         // The bright-pass perms bind the full-res r32_float depth at t1 (declared-but-unused there); capture it for
         // SMAA predication. Bindings are read regardless of the pass being hash-replaced. The frame's first capture wins (the AO
         // pack's, earlier, binds the same depth), and only an r32_float view: t1 of the static perm was never seen.
         if (g_smaa_predication && !game_device_data.srv_scene_depth &&
             (ContainsPixelShader(original_shader_hashes, TONEMAP_BRIGHT_PASS) || ContainsPixelShader(original_shader_hashes, TONEMAP_BRIGHT_PASS_STATIC)))
         {
            ComPtr<ID3D11ShaderResourceView> depth_srv;
            native_device_context->PSGetShaderResources(1, 1, depth_srv.put());
            D3D11_SHADER_RESOURCE_VIEW_DESC depth_desc = {};
            if (depth_srv)
            {
               depth_srv->GetDesc(&depth_desc);
            }
            if (depth_desc.Format == DXGI_FORMAT_R32_FLOAT)
            {
               game_device_data.srv_scene_depth = std::move(depth_srv);
            }
         }
#endif
      }
      else if (is_final_grade)
      {
         // CustomData2 = SMAA active or the upscaler antialiased the frame, which makes the grade skip its built-in FXAA.
         game_device_data.final_grade_fired_this_frame = true; // opens the Hide UI window for the rest of the frame
#if DEVELOPMENT
         LogVanillaGrade(native_device, native_device_context, &game_device_data);
#endif

         const bool upscaled = device_data.has_drawn_sr;
         SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, stages, LumaConstantBufferType::LumaSettings);
         SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, stages, LumaConstantBufferType::LumaData, 0u, (g_smaa_enable || upscaled) ? 1u : 0u);
         updated_cbuffers = true;

#if ENABLE_SMAA
         // Run the original grade draw, then SMAA on its output (after the upscaler RCAS alone). Without the callback this falls
         // back to a normal draw, and the grade has already skipped FXAA for this frame: one frame of no AA.
         // RCAS replaces the game's sharpen (cut from its scene pass, see ColModFog_0xC70077EC), so it also runs without SMAA
         const bool smaa = !upscaled && g_smaa_enable;
         if ((smaa || g_rcas_sharpness > 0.f) && original_draw_dispatch_func != nullptr)
         {
            (*original_draw_dispatch_func)();
            ComPtr<ID3D11RenderTargetView> grade_rtv;
            native_device_context->OMGetRenderTargets(1, grade_rtv.put(), nullptr);
            if (grade_rtv)
            {
               ComPtr<ID3D11Resource> canvas_res;
               grade_rtv->GetResource(canvas_res.put());
               if (canvas_res)
               {
                  RunPostFinalGradeSMAA(native_device, native_device_context, device_data, &game_device_data, canvas_res.get(), grade_rtv.get(), smaa);
               }
            }
            return DrawOrDispatchOverrideType::Replaced; // we ran the original draw ourselves
         }
#endif
      }
      const bool is_exposure = is_immediate && (ContainsPixelShader(original_shader_hashes, TONEMAP_EXPOSURE) || ContainsPixelShader(original_shader_hashes, TONEMAP_EXPOSURE_STATIC));
      // The upscaler's exposure on the linear scene comes from the exposure pass's inputs: the adaptation it read (its t1; the static
      // perms have none) and its vc4, copied whole (dgVoodoo binds it at offset 0 and rewrites it for the next pass)
      if (is_exposure && game_device_data.mv_active)
      {
         game_device_data.sr_exposure_static = !ContainsPixelShader(original_shader_hashes, TONEMAP_EXPOSURE);
         game_device_data.sr_adaptation_srv.reset();
         if (!game_device_data.sr_exposure_static)
         {
            native_device_context->PSGetShaderResources(1, 1, &game_device_data.sr_adaptation_srv);
         }
         com_ptr<ID3D11Buffer> constants;
         native_device_context->PSGetConstantBuffers(4, 1, &constants);
         if (constants)
         {
            D3D11_BUFFER_DESC desc, copy_desc = {};
            constants->GetDesc(&desc);
            if (game_device_data.sr_exposure_constants)
            {
               game_device_data.sr_exposure_constants->GetDesc(&copy_desc);
            }
            if (copy_desc.ByteWidth != desc.ByteWidth)
            {
               game_device_data.sr_exposure_constants.reset();
               const CD3D11_BUFFER_DESC new_desc(desc.ByteWidth, D3D11_BIND_CONSTANT_BUFFER);
               native_device->CreateBuffer(&new_desc, nullptr, &game_device_data.sr_exposure_constants);
            }
            if (game_device_data.sr_exposure_constants)
            {
               native_device_context->CopyResource(game_device_data.sr_exposure_constants.get(), constants.get());
            }
         }
      }
      // DLSS/FSR without the post chain hook (not the Steam exe) or with UberSampling: the exposed scene is the upscaler's input,
      // upscaled in place right after the exposure (adaptive or static) wrote it, before the glow, DoF and the grade read it (the
      // light shafts and motion blur before it are in its history). The exposure only multiplies, so it doesn't mind the jitter.
      if (game_device_data.mv_scene_open && is_exposure && original_draw_dispatch_func && *original_draw_dispatch_func)
      {
         ComPtr<ID3D11RenderTargetView> scene_rtv;
         native_device_context->OMGetRenderTargets(1, scene_rtv.put(), nullptr);
         const uint2 scene_size = GetViewTextureSize(scene_rtv.get());
         // The scene's own exposure only, the output sized one
         if (scene_size.x != uint32_t(device_data.output_resolution.x) || scene_size.y != uint32_t(device_data.output_resolution.y))
            return DrawOrDispatchOverrideType::None;
         SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, stages, LumaConstantBufferType::LumaSettings);
         SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, stages, LumaConstantBufferType::LumaData);
         updated_cbuffers = true;
         (*original_draw_dispatch_func)();
#if DEVELOPMENT
         game_device_data.mv_stats.ended_by = uint32_t(original_shader_hashes.pixel_shaders[0]);
#endif
         EndScene(native_device, native_device_context, device_data, scene_rtv.get(), true);
         return DrawOrDispatchOverrideType::Replaced; // we ran the original draw ourselves
      }

      // Native SSAO generator -> XeGTAO takeover; independent of the tonemap/grade chain above.
      if (ContainsPixelShader(original_shader_hashes, AO_GEN))
      {
#if DEVELOPMENT
         LogAOGenLayout(native_device, native_device_context, device_data, &game_device_data); // with XeGTAO off too
#endif
         if (g_gtao_enable)
            return RunXeGTAO(native_device, native_device_context, device_data, &game_device_data);
      }
#if ENABLE_SMAA
      if (ContainsPixelShader(original_shader_hashes, AO_PACK))
      {
         // Depth capture for SMAA predication: the AO pack pass binds the same r32_float depth at t0 (with SSAO on), before the
         // bright-pass. The frame's first capture wins.
         if (g_smaa_predication && !game_device_data.srv_scene_depth)
         {
            native_device_context->PSGetShaderResources(0, 1, game_device_data.srv_scene_depth.put());
         }
      }
#endif

      return DrawOrDispatchOverrideType::None;
   }

   void OnPresent(ID3D11Device* native_device, DeviceData& device_data) override
   {
      auto& game_device_data = GetGameDeviceData(device_data);

      // Menu/loading frames run no tonemap: "has_drawn_main_post_processing" stays false there so the core
      // display composition treats the frame as plain SDR UI at UIPaperWhite (do NOT force it here).
      game_device_data.final_grade_fired_this_frame = false; // re-arm the Hide UI window for the next frame

      // DLSS/FSR: the history restarts after any frame it didn't draw (menus, loading, just picked); the selection and the motion
      // vector state are fixed here for the next frame (see "IsSRActive")
      device_data.force_reset_sr = !device_data.has_drawn_sr;
      device_data.has_drawn_sr = false;
      game_device_data.sr_active = device_data.sr_type != SR::Type::None && !device_data.sr_suppressed;
      game_device_data.fsr_masks_active = game_device_data.sr_active && device_data.sr_type == SR::Type::FSR && g_sr_reactive_enable;
      game_device_data.mv_active = IsSRActive(device_data) || g_mv_enable;
      // Render scale: the next 3D frame's scene shrinks only with an upscaler ready to draw (not while the SR bridge's helper starts,
      // which passes the color through) and after a frame whose scene reached its end (menus and loading screens have no post chain,
      // and the first frame after them upscales at full size)
      const SR::InstanceData* const sr_instance_data = (IsSRActive(device_data) ? device_data.GetSRInstanceData() : nullptr);
      RenderArea::next_scale = ((RenderArea::scale_installed && g_render_scale < 1.f && game_device_data.mv_scene_done && sr_instance_data && sr_implementations[device_data.sr_type]->IsReady(sr_instance_data)) ? g_render_scale : 1.f);
      RenderArea::render_size = {};
      // None picked: Core stopped the SR bridge's helper ("ReleaseResources"), our upscaler inputs and output go too. Recreated when
      // an upscaler is picked again (the helper takes seconds to start).
      if (device_data.sr_type == SR::Type::None && game_device_data.release_sr_resources.exchange(false))
      {
         game_device_data.sr_output_srv.reset();
         game_device_data.sr_adaptation_srv.reset();
         game_device_data.sr_exposure_constants.reset();
         game_device_data.sr_exposure.reset();
         game_device_data.sr_exposure_uav.reset();
         game_device_data.mv_scene_color_rtv.reset();
         game_device_data.render_area_depth_copy.reset();
         game_device_data.render_area_depth_copy_srv.reset();
         game_device_data.render_area_depth_rtv.reset();
         if (!g_mv_enable)
         {
            const std::unique_lock lock(game_device_data.mv_mutex);
            game_device_data.ReleaseMotionVectorTargets();
            game_device_data.ReleaseReactiveMasks();
            // The game's depth resources (taken again at the next scene, so a resize after None doesn't keep the old ones alive), the vc4
            // copies the object tables and the cameras hold, our constant buffers and the 4 MiB previous constants ring (address space)
            game_device_data.mv_depth.reset();
            game_device_data.mv_linear_depth.reset();
            game_device_data.mv_linear_depth_srv.reset();
            game_device_data.mv_linear_depth_rtv = nullptr;
            game_device_data.mv_accepted_rtv = nullptr;
            game_device_data.mv_objects.clear();
            game_device_data.mv_previous_objects.clear();
            game_device_data.mv_camera.reset();
            game_device_data.mv_previous_camera.reset();
            game_device_data.mv_fill_buffer.reset();
            game_device_data.mv_jitter_buffer.reset();
            game_device_data.mv_previous_constants = {};
            const std::lock_guard constants_lock(game_device_data.mv_constants_mutex);
            // Nulled, not erased: the buffers stay registered (and in the lock free filter)
            for (auto& [buffer, copy] : game_device_data.mv_constants_copies)
            {
               copy.reset();
            }
            game_device_data.mv_mapped_constants.clear();
            game_device_data.mv_filtered_mapped = {};
            game_device_data.mv_constants_pool.clear();
            game_device_data.mv_constants_pool_free.clear();
         }
      }
      if (game_device_data.mv_reactive_target && cb_luma_global_settings.FrameIndex - game_device_data.sr_reactive_frame > IDLE_RELEASE_FRAMES)
      {
         const std::unique_lock lock(game_device_data.mv_mutex);
         game_device_data.ReleaseReactiveMasks();
      }
#if DEVELOPMENT
      // "Performance Test": closes this frame's timestamp set, reads back the finished ones (a log line every 120 frames, the first 30
      // after a settings change or a pause skipped), opens the next frame's
      {
         com_ptr<ID3D11DeviceContext> perf_context;
         native_device->GetImmediateContext(&perf_context);
         game_device_data.perf_timestamps.Close(perf_context.get());
         if (Perf::g_test != 0)
         {
            auto& window = game_device_data.perf_window;
            const char* const aa = (IsSRActive(device_data) ? (device_data.sr_type == SR::Type::DLSS ? "DLSS" : "FSR") : (g_mv_enable ? "MV only" : (g_smaa_enable ? "SMAA" : "none")));
            const std::string settings = std::format("mode=\"{}\" hook_timers={} aa={} render_scale={:.2f} vc4_filter={} vc4_pool={} vc4_slots={} output={}x{}", PERF_TEST_MODES[Perf::g_test].name,
               Perf::g_hook_timers, aa, RenderArea::next_scale, g_mv_buffer_filter, g_mv_constants_pool, g_mv_vc4_slots, uint32_t(device_data.output_resolution.x), uint32_t(device_data.output_resolution.y));
            // Also until the upscaler draws (the SR bridge's helper takes seconds to start, passing the color through meanwhile), and
            // longer after leaving it
            const bool sr_exit = std::exchange(game_device_data.perf_sr_active, IsSRActive(device_data)) && !IsSRActive(device_data);
            const bool sr_starting = IsSRActive(device_data) && !sr_implementations[device_data.sr_type]->IsReady(device_data.GetSRInstanceData());
            const bool measuring = window.Present(settings, sr_exit ? PERF_HELPER_EXIT_SETTLE_FRAMES : PERF_SETTLE_FRAMES, sr_exit || sr_starting);
            auto& stats = window.stats;
            window.disjoint += game_device_data.perf_timestamps.Collect(perf_context.get(), measuring, [&](const auto& frame)
               {
                  stats.frame.Add(frame.Ms(Perf::FRAME_START, Perf::FRAME_END));
                  if (!frame.Has(PERF_SCENE_START) || !frame.Has(PERF_SCENE_END))
                     return;
                  stats.scene.Add(frame.Ms(PERF_SCENE_START, PERF_SCENE_END));
                  if (!frame.Has(PERF_SCENE_TAIL_END))
                     return;
                  stats.end.Add(frame.Ms(PERF_SCENE_END, PERF_SCENE_TAIL_END));
                  if (frame.Has(PERF_FILL_END) && frame.Has(PERF_UPSCALER_END))
                  {
                     const size_t bounds[4] = {PERF_SCENE_END, PERF_FILL_END, PERF_UPSCALER_END, PERF_SCENE_TAIL_END};
                     for (int part = 0; part < 3; part++)
                     {
                        stats.end_parts[part].Add(frame.Ms(bounds[part], bounds[part + 1]));
                     }
                  } });
            window.Finish(measuring, [&]
               {
               const double fill = stats.end_parts[0].Average(), upscaler = stats.end_parts[1].Average(), copy_back = stats.end_parts[2].Average();
               const double hooks = window.HookMs();
               reshade::log::message(reshade::log::level::info, std::format("[TW2 Perf] {} gpu frame avg/max={:.3f}/{:.3f} ms scene avg/max={:.3f}/{:.3f} ms ({}) end avg/max={:.3f}/{:.3f} ms ({}) = fill {:.3f} + upscaler {:.3f} + copy back {:.3f} ms ({}) cpu frame avg={:.3f} ms cpu hooks={:.3f} ms/frame samples={}/{} disjoint={}",
                  settings, stats.frame.Average(), stats.frame.max_ms, stats.scene.Average(), stats.scene.max_ms, stats.scene.samples, stats.end.Average(), stats.end.max_ms, stats.end.samples, fill, upscaler, copy_back, stats.end_parts[0].samples, window.CpuFrameMs(), hooks, stats.frame.samples, window.frames, window.disjoint).c_str());
               // The row in "PerfColumn" order
               g_perf_sweep.OnWindow(Perf::g_test, {stats.frame.Average(), stats.scene.Average(), stats.end.Average(), hooks, window.CpuFrameMs(), fill, upscaler, copy_back}, [&](int mode_index)
                  { ApplyPerfTestMode(device_data, mode_index); }, [&](int mode, int baseline_mode, double baseline)
                  {
                     const double cpu_frame = g_perf_sweep.Median(mode, PERF_COLUMN_CPU_FRAME);
                     reshade::log::message(reshade::log::level::info, std::format("[TW2 Perf] sweep mode=\"{}\" hook_timers={} windows={} cpu frame median={:.3f} ms ({:.1f} fps) gpu frame median={:.3f} ms ({:+.3f} vs \"{}\") scene median={:.3f} ms end median={:.3f} ms (fill {:.3f} upscaler {:.3f} copy back {:.3f}) cpu hooks median={:.3f} ms/frame", PERF_TEST_MODES[mode].name, Perf::g_hook_timers, g_perf_sweep.results[mode].size(), cpu_frame, cpu_frame > 0.0 ? 1000.0 / cpu_frame : 0.0, g_perf_sweep.Median(mode, PERF_COLUMN_FRAME), g_perf_sweep.Median(mode, PERF_COLUMN_FRAME) - baseline, PERF_TEST_MODES[baseline_mode].name, g_perf_sweep.Median(mode, PERF_COLUMN_SCENE), g_perf_sweep.Median(mode, PERF_COLUMN_TAIL), g_perf_sweep.Median(mode, PERF_COLUMN_FILL), g_perf_sweep.Median(mode, PERF_COLUMN_UPSCALER), g_perf_sweep.Median(mode, PERF_COLUMN_COPY_BACK), g_perf_sweep.Median(mode, PERF_COLUMN_HOOKS)).c_str()); }); });
            game_device_data.perf_timestamps.Open(native_device, perf_context.get());
         }
      }
#endif
      // A scene no exposure ended ends here: its jitter must not reach the next frame's draws before its first mesh, and the next
      // frame doesn't jitter (see "mv_previous_scene_done")
      game_device_data.mv_previous_scene_done = game_device_data.mv_scene_done;
      game_device_data.mv_scene_open = false;
      game_device_data.mv_scene_done = false;
      game_device_data.mv_frame_ended = true;
      game_device_data.mv_not_scene_dsv = nullptr;
      game_device_data.mv_fill_pending = false;
      // Core biases the anisotropic samplers (all of the game's with the AF16x upgrade): -1 at native resolution, -1.58 at 67%, -2 at 50%
      game_device_data.scene_mip_bias = (IsSRActive(device_data) ? SR::GetMipLODBias(device_data.output_resolution.y * RenderArea::next_scale, device_data.output_resolution.y) : 0.f);
      {
         com_ptr<ID3D11DeviceContext> native_device_context;
         native_device->GetImmediateContext(&native_device_context);
         SetMipBias(native_device_context.get(), device_data, 0.f);
      }
      {
         // The pooled vc4 copies only the pool holds (superseded, no object or camera keeps them) are free for the next ones, as many as
         // the last frame asked for: frames without a scene (loading, videos, menus) still copy every Unmap, and would keep their peak
         // otherwise. Without motion vectors, none.
         const std::lock_guard lock(game_device_data.mv_constants_mutex);
         size_t kept_free = (game_device_data.mv_active ? std::exchange(game_device_data.mv_constants_made, 0) : 0);
         std::erase_if(game_device_data.mv_constants_pool, [&](const auto& copy)
            {
               if (copy.use_count() != 1)
                  return false;
               if (kept_free == 0)
                  return true;
               kept_free--;
               return false; });
         for (auto& [size, free_copies] : game_device_data.mv_constants_pool_free)
         {
            free_copies.clear();
         }
         for (uint32_t i = 0; i < uint32_t(game_device_data.mv_constants_pool.size()); i++)
         {
            if (game_device_data.mv_constants_pool[i].use_count() == 1)
            {
               game_device_data.mv_constants_pool_free[game_device_data.mv_constants_pool[i]->size()].push_back(i);
            }
         }
#if DEVELOPMENT
         game_device_data.mv_stats.constants_pool = uint32_t(game_device_data.mv_constants_pool.size());
#endif
      }
#if DEVELOPMENT
      game_device_data.mv_last_stats = std::exchange(game_device_data.mv_stats, {});
      // The DEV counts in ReShade.log every 300 frames while motion vectors run
      if (const auto& stats = game_device_data.mv_last_stats; game_device_data.mv_active && cb_luma_global_settings.FrameIndex % 300 == 0)
      {
         std::string refused;
         for (size_t i = 0; i < MOTION_VECTOR_REJECT_NAMES.size(); i++)
         {
            refused += std::format(" {} {}", MOTION_VECTOR_REJECT_NAMES[i], stats.rejected[i]);
         }
         reshade::log::message(reshade::log::level::info, std::format("[TW2 MV] frame {}: {} mv ({} matched, {} camera only, {} other camera, {} uncopied, {} tie-break collisions), {} jitter ({} reactive), {} maps, {} updates, {} other maps, {} offset bindings, sr {} ({}), linear depth {}, near {:.3f} far {:.0f}, ended by 0x{:08X}, refused{}",
                                                             cb_luma_global_settings.FrameIndex, stats.motion_vector_draws, stats.matched, stats.camera_only, stats.other_camera, stats.uncopied, stats.tiebreak_collisions, stats.jitter_draws, stats.reactive_draws, stats.maps, stats.updates,
                                                             stats.other_maps, stats.offset_bindings, stats.sr_draws, int(device_data.sr_type), game_device_data.mv_linear_depth != nullptr, stats.near_plane, stats.far_plane, stats.ended_by, refused)
                                                             .c_str());
      }
      // "MV Debug View" / "FSR Reactive Mask Debug View": Core's debug draw of the target (motion vectors as absolute pixels)
      {
         const std::shared_lock lock(game_device_data.mv_mutex);
         ID3D11Texture2D* const debug_texture = (g_mv_debug_view ? game_device_data.mv_texture.get() : (g_sr_reactive_debug_view ? game_device_data.mv_reactive.get() : nullptr));
         if (debug_texture)
         {
            D3D11_TEXTURE2D_DESC desc;
            debug_texture->GetDesc(&desc);
            debug_draw_auto_clear_texture = false;
            if (g_mv_debug_view)
            {
               debug_draw_options |= (uint32_t)DebugDrawTextureOptionsMask::Abs | (uint32_t)DebugDrawTextureOptionsMask::UVToPixelSpace;
            }
            device_data.debug_draw_texture = debug_texture;
            device_data.debug_draw_texture_format = desc.Format;
            device_data.debug_draw_texture_size = {desc.Width, desc.Height, 1, 1};
         }
         else if (device_data.debug_draw_texture && (device_data.debug_draw_texture.get() == game_device_data.mv_texture.get() || device_data.debug_draw_texture.get() == game_device_data.mv_reactive.get()))
         {
            device_data.debug_draw_texture = nullptr;
         }
      }
#endif

      // Give the scratch back when a feature is switched off; it is all lazily recreated. Predication and the
      // RCAS intermediate are separate because either can be off while SMAA runs.
      if (!g_gtao_enable && game_device_data.gtao_w != 0)
      {
         game_device_data.ReleaseGTAOScratch();
      }
#if ENABLE_SMAA
      // SMAA's resources go after "IDLE_RELEASE_FRAMES" presents without it (turned off, or the upscaler antialiasing). The snapshot
      // also serves RCAS alone after the upscaler, and goes once neither ran (Memory-Optimization-Checklist MEM-11).
      if (game_device_data.cb_smaa_metrics && cb_luma_global_settings.FrameIndex - game_device_data.smaa_frame > IDLE_RELEASE_FRAMES)
      {
         ReleaseSMAA(device_data);
         game_device_data.ReleasePredicationScratch();
         game_device_data.ReleaseSharpenScratch();
         game_device_data.cb_smaa_metrics.reset(); // the marker of this release
      }
      if (game_device_data.tex_input && cb_luma_global_settings.FrameIndex - game_device_data.snapshot_frame > IDLE_RELEASE_FRAMES)
      {
         game_device_data.ReleaseSMAAScratch();
      }
      if (!g_smaa_predication && game_device_data.tex_pred)
      {
         game_device_data.ReleasePredicationScratch();
      }
      if (g_rcas_sharpness <= 0.f && game_device_data.tex_smaa_out)
      {
         game_device_data.ReleaseSharpenScratch();
      }
      // Per-frame capture: never let a stale depth SRV from a previous scene leak into a frame whose
      // tonemap didn't re-capture it (menus; the SMAA pass then falls back to null predication).
      game_device_data.srv_scene_depth.reset();
#endif
   }

   void CleanExtraSRResources(DeviceData& device_data) override
   {
      GetGameDeviceData(device_data).release_sr_resources = true;
   }

   void LoadConfigs() override
   {
#if ENABLE_SMAA
      reshade::get_config_value(nullptr, NAME, "SMAAEnable", g_smaa_enable);
      reshade::get_config_value(nullptr, NAME, "RCASSharpness", g_rcas_sharpness);
      reshade::get_config_value(nullptr, NAME, "SMAAPredication", g_smaa_predication);
#endif
      reshade::get_config_value(nullptr, NAME, "GTAOEnable", g_gtao_enable);
      reshade::get_config_value(nullptr, NAME, "RenderScale", g_render_scale);
      g_render_scale = std::clamp(g_render_scale, MIN_RENDER_SCALE, 1.f);

      // HDR grade sliders (cb_luma_global_settings_dirty is already true at init -> uploaded on first frame).
      auto& gs = cb_luma_global_settings.GameSettings;
      reshade::get_config_value(nullptr, NAME, "Exposure", gs.Exposure);
      reshade::get_config_value(nullptr, NAME, "Saturation", gs.Saturation);
      reshade::get_config_value(nullptr, NAME, "HighlightDechroma", gs.HighlightDechroma);
      reshade::get_config_value(nullptr, NAME, "Dithering", gs.Dithering);
      reshade::get_config_value(nullptr, NAME, "VignetteIntensity", gs.VignetteIntensity);
      reshade::get_config_value(nullptr, NAME, "Contrast", gs.Contrast);
      reshade::get_config_value(nullptr, NAME, "BloomIntensity", gs.BloomIntensity);
      reshade::get_config_value(nullptr, NAME, "ColorGradingIntensity", gs.ColorGradingIntensity);
      // "Hide Gameplay UI" is deliberately NOT persisted: a saved HUD-less state makes the
      // next launch look broken.
      {
         bool video_auto_hdr = gs.VideoAutoHDREnable > 0.5f;
         reshade::get_config_value(nullptr, NAME, "VideoAutoHDREnable", video_auto_hdr);
         gs.VideoAutoHDREnable = (video_auto_hdr ? 1.f : 0.f);
      }
      reshade::get_config_value(nullptr, NAME, "VideoAutoHDRBoost", gs.VideoAutoHDRBoost);
   }

   void DrawImGuiSettings(DeviceData& device_data) override
   {
#if ENABLE_SMAA
      ImGui::SeparatorText("Anti-Aliasing");
      const bool sr_active = IsSRActive(device_data);
      if (RenderArea::scale_installed)
      {
         ImGui::BeginDisabled(!sr_active);
         // Applied on release: every render size recreates the DLSS/FSR feature, a hitch per 1% step while dragging
         static int held_render_scale = 0; // The slider's value while it's held, else 0
         int render_scale = (held_render_scale != 0 ? held_render_scale : int(std::round(g_render_scale * 100.f)));
         ImGui::SliderInt("Render Scale (%)", &render_scale, int(MIN_RENDER_SCALE * 100.f), 100, "%d%%", ImGuiSliderFlags_AlwaysClamp);
         held_render_scale = (ImGui::IsItemActive() ? render_scale : 0);
         if (ImGui::IsItemDeactivatedAfterEdit())
         {
            g_render_scale = float(render_scale) / 100.f;
            reshade::set_config_value(nullptr, NAME, "RenderScale", g_render_scale);
         }
         if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
         {
            ImGui::SetTooltip("The resolution the game renders at, upscaled by DLSS/FSR.");
         }
         DrawResetButton(g_render_scale, 1.f, "RenderScale");
         ImGui::EndDisabled();
      }
      // The upscaler (Super Resolution, in the Settings tab) replaces SMAA: shown off, the saved choice is kept
      ImGui::BeginDisabled(sr_active);
      bool smaa_shown = g_smaa_enable && !sr_active;
      if (ImGui::Checkbox("SMAA Enable", sr_active ? &smaa_shown : &g_smaa_enable))
      {
         reshade::set_config_value(nullptr, NAME, "SMAAEnable", g_smaa_enable);
      }
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
      {
         ImGui::SetTooltip("Replaces the game's FXAA with SMAA (works with the game's Anti-aliasing setting on or off; not used with DLSS/FSR).");
      }
      ImGui::EndDisabled();
      // Canon deviation (docs/UI-Toggle-Standard.md), as Saints Row The Third Remastered: RCAS runs after any anti-aliasing, in place
      // of the game's sharpen (cut from its scene pass)
      ImGui::SliderFloat("RCAS Sharpness", &g_rcas_sharpness, 0.f, 1.f);
      if (ImGui::IsItemDeactivatedAfterEdit())
      {
         reshade::set_config_value(nullptr, NAME, "RCASSharpness", g_rcas_sharpness);
      }
      if (ImGui::IsItemHovered())
      {
         ImGui::SetTooltip("Sharpening applied on top of anti-aliasing (0 = off). Replaces the game's Sharpen setting.");
      }
      DrawResetButton(g_rcas_sharpness, 0.f, "RCASSharpness");
      ImGui::BeginDisabled(!g_smaa_enable || sr_active);
#if DEVELOPMENT || TEST
      if (ImGui::Checkbox("SMAA Predication", &g_smaa_predication))
      {
         reshade::set_config_value(nullptr, NAME, "SMAAPredication", g_smaa_predication);
      }
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
      {
         ImGui::SetTooltip("Relaxes the edge threshold back to base ULTRA on geometric silhouettes only. Off = plain ULTRA everywhere (threshold scale 1.0).");
      }
      ImGui::SliderFloat("SMAA Predication Tolerance", &g_smaa_pred_tolerance, 0.002f, 0.1f, "%.3f", ImGuiSliderFlags_Logarithmic);
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
      {
         ImGui::SetTooltip("Plane deviation counted as a full edge, as a fraction of view depth. The ONLY predication dial — the shader threshold stays 0.5 by design (see Luma_TW2_DepthExtract.hlsl). AO precedents: 0.011 (XeGTAO) .. 0.040 (ASSAO).");
      }
#endif
      ImGui::EndDisabled();
#endif

#if DEVELOPMENT
      ImGui::SeparatorText("Motion Vectors (DLSS/FSR)");
      ImGui::Checkbox("MV Enable", &g_mv_enable);
      if (ImGui::IsItemHovered())
      {
         ImGui::SetTooltip("Draws the scene with the motion vector shaders without an upscaler: camera and object motion (each draw finds its own\nprevious frame vc4). The image must not change; the debug view is black with a static camera and lights up only\nwhat moves. ReShade.log: patched/refused shaders, \"[TW2 MV]\" counts every 300 frames. Not saved.");
      }
      ImGui::Checkbox("MV Debug View", &g_mv_debug_view);
      if (ImGui::IsItemHovered())
      {
         ImGui::SetTooltip("Shows the motion vector target (absolute value, in pixels) instead of the frame. Not saved.");
      }
      ImGui::Checkbox("MV Force Jitter", &g_mv_force_jitter);
      if (ImGui::IsItemHovered())
      {
         ImGui::SetTooltip("Jitters the scene (Halton 2/3, 8 phases) without an upscaler, with MV Enable. The image shakes by a subpixel; nothing\nmay flicker or lose pixels, and the debug view stays black with a static camera. Not saved.");
      }
      ImGui::Checkbox("MV Disable Jitter", &g_mv_disable_jitter);
      if (ImGui::IsItemHovered())
      {
         ImGui::SetTooltip("No projection jitter under the upscaler (it gets zero jitter): isolates artifacts that come from the jitter. Not saved.");
      }
      ImGui::Checkbox("FSR Reactive Mask", &g_sr_reactive_enable);
      if (ImGui::IsItemHovered())
      {
         ImGui::SetTooltip("FSR's reactive and transparency & composition masks from the alpha blended draws (smoke, glass, water, sparks). Not saved.");
      }
      ImGui::Checkbox("FSR Reactive Mask Debug View", &g_sr_reactive_debug_view);
      if (ImGui::IsItemHovered())
      {
         ImGui::SetTooltip("Shows FSR's reactive mask instead of the frame. Not saved.");
      }
      ImGui::SliderFloat("FSR Reactive Scale", &g_sr_reactive_scale, 0.f, 4.f);
      if (ImGui::IsItemHovered())
      {
         ImGui::SetTooltip("The alpha blended draws' reactivity, scaled before the threshold (AMD's default 1). Not saved.");
      }
      ImGui::SliderFloat("FSR Reactive Threshold", &g_sr_reactive_threshold, 0.f, 1.f);
      if (ImGui::IsItemHovered())
      {
         ImGui::SetTooltip("Reactivity under it 0, over it 0.9 (AMD's 0.2, Mass Effect's 0.5); 0 = the scaled reactivity, capped at 0.9. Not saved.");
      }
      const auto& stats = GetGameDeviceData(device_data).mv_last_stats;
      ImGui::Text("MV draws %u (matched %u, camera only %u, other camera %u, uncopied %u), jitter draws %u, upscaled %u", stats.motion_vector_draws, stats.matched, stats.camera_only, stats.other_camera, stats.uncopied, stats.jitter_draws, stats.sr_draws);
      Perf::DrawCombo(PERF_TEST_MODES, &g_perf_sweep, [&](int mode_index)
         { ApplyPerfTestMode(device_data, mode_index); });
      if (ImGui::IsItemHovered())
      {
         ImGui::SetTooltip("Logs GPU and CPU times every 120 frames ([TW2 Perf] in ReShade.log): the frame, the scene, the end of the scene\n(fill, the upscaler, copy back) and the scene hooks' CPU time. The first 30 frames after a settings change are skipped.\nKeep the camera still. \"Sweep\" runs the anti-aliasing modes, 3 rounds, then logs medians against No AA; \"CPU Sweep\"\nthe CPU savings each off in turn under the current DLSS/FSR, against Current Settings. Not saved.");
      }
      ImGui::Checkbox("Hook Timers", &Perf::g_hook_timers);
      if (ImGui::IsItemHovered())
      {
         ImGui::SetTooltip("Times the motion vector draw and buffer hooks for \"cpu hooks\" (two clock reads each, thousands a frame).\nRun a Sweep with it off to see their own cost in the frame times. Not saved.");
      }
#endif

      // Exposure and Color Grading Intensity act on SDR and HDR alike; the gated block below is HDR-only.
      ImGui::SeparatorText("Grade");
      auto& gs = cb_luma_global_settings.GameSettings;
      auto& gs_def = default_luma_global_game_settings;

      // One grade slider: draw, mark the cbuffer dirty, persist when the edit ends, tooltip, reset (DrawResetButton
      // writes the config itself). AllowWhenDisabled so the tooltip still shows inside a BeginDisabled block.
      const auto slider = [&](const char* label, float* value, float default_value, const char* key, float max_value, const char* tooltip)
      {
         if (ImGui::SliderFloat(label, value, 0.f, max_value))
         {
            device_data.cb_luma_global_settings_dirty = true;
         }
         if (ImGui::IsItemDeactivatedAfterEdit())
         {
            reshade::set_config_value(nullptr, NAME, key, *value);
         }
         if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
         {
            ImGui::SetTooltip("%s", tooltip);
         }
         if (DrawResetButton(*value, default_value, key))
         {
            device_data.cb_luma_global_settings_dirty = true;
         }
      };

      slider("Exposure", &gs.Exposure, gs_def.Exposure, "Exposure", 2.f, "Overall image brightness (1 = vanilla).");
      // Not HDR-gated: the split toning this fades lives in the vanilla grade tail, so it applies in SDR as well.
      slider("Color Grading Intensity", &gs.ColorGradingIntensity, gs_def.ColorGradingIntensity, "ColorGradingIntensity", 1.f, "Strength of the game's own color grading (1 = vanilla, 0 = neutral).");

      // Hidden outside HDR: the final grade's SDR branch skips the whole HDR block, so these would be dead
      // controls. Matches the shader's own "DisplayMode == 1" test.
      if (cb_luma_global_settings.DisplayMode == DisplayModeType::HDR)
      {
         slider("Contrast", &gs.Contrast, gs_def.Contrast, "Contrast", 2.f, "Overall image contrast, HDR only (1 = vanilla).");
         slider("Saturation", &gs.Saturation, gs_def.Saturation, "Saturation", 2.f, "Color saturation, HDR only (1 = vanilla).");
         slider("Highlights Desaturation", &gs.HighlightDechroma, gs_def.HighlightDechroma, "HighlightDechroma", 1.f,
            "How far the brightest sources fade to neutral white, HDR only (0 = keep color at any brightness).");
      }

      // No "Luma Bloom Enable": the engine's glow is kept and only scaled. Applies in SDR too.
      ImGui::SeparatorText("Bloom");
      slider("Bloom Intensity", &gs.BloomIntensity, gs_def.BloomIntensity, "BloomIntensity", 2.f, "Bloom strength (1 = vanilla, 0 = none).");

      ImGui::SeparatorText("Ambient Occlusion");
      if (ImGui::Checkbox("XeGTAO Enable", &g_gtao_enable))
      {
         reshade::set_config_value(nullptr, NAME, "GTAOEnable", g_gtao_enable);
      }
      if (ImGui::IsItemHovered())
      {
         ImGui::SetTooltip("Replaces the game's SSAO with XeGTAO (cleaner, more accurate ambient occlusion; requires SSAO enabled in the game's video settings).");
      }
#if DEVELOPMENT || TEST
      ImGui::BeginDisabled(!g_gtao_enable);
      ImGui::SliderFloat("GTAO Final Value Power", &g_gtao_final_value_power, 0.3f, 4.5f, "%.2f");
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
      {
         ImGui::SetTooltip("Primary darkness dial (higher = darker AO). Shipped at 2.2 by preference; 1.0 is the value that matches the native HBAO histogram, mean 0.89 against XeGTAO's 0.90.");
      }
      ImGui::SliderFloat("GTAO Depth Scale", &g_gtao_depth_scale, 0.01f, 200.f, "%.2f", ImGuiSliderFlags_Logarithmic);
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
      {
         ImGui::SetTooltip("viewZ divisor (game depth units -> meters). Stays 1.0 in this game: its depth buffer is already LINEAR view-space metres (measured p50 7.3, max 686).");
      }
      ImGui::SliderFloat("GTAO Radius Override", &g_gtao_radius_override, 0.f, 5.f, "%.3f");
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
      {
         ImGui::SetTooltip("0 = shader EFFECT_RADIUS define (0.81, anchored to the native 1.18 m radius from cb4[11]); > 0 overrides it, in metres.");
      }
#if DEVELOPMENT // the shader's debug blocks exist in DEVELOPMENT only
      ImGui::Combo("GTAO Debug View", &g_gtao_debug_view, "Off\0Depth gradient\0Normals\0AO x8\0Edges\0");
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
      {
         ImGui::SetTooltip("Draws diagnostics through the game's AO apply (multiplied into the scene; DEVELOPMENT shader only). Depth gradient dead/flat or normals blocky = wrong input; AO x8 = spot broad over-occlusion.");
      }
#endif
      ImGui::EndDisabled();
#endif

      ImGui::SeparatorText("Effects");

      // Vignette lives in the vanilla grade tail, so this applies in SDR as well as HDR.
      slider("Vignette Intensity", &gs.VignetteIntensity, gs_def.VignetteIntensity, "VignetteIntensity", 1.f, "Scales the game's vignette darkening (1 = vanilla, 0 = none).");

      // HDR-path only: PumboAutoHDR self-noops when peak == paper white, which both SDR modes force.
      if (cb_luma_global_settings.DisplayMode == DisplayModeType::HDR)
      {
         bool video_auto_hdr = gs.VideoAutoHDREnable > 0.5f;
         if (ImGui::Checkbox("Video AutoHDR", &video_auto_hdr))
         {
            gs.VideoAutoHDREnable = (video_auto_hdr ? 1.f : 0.f);
            device_data.cb_luma_global_settings_dirty = true;
            reshade::set_config_value(nullptr, NAME, "VideoAutoHDREnable", video_auto_hdr);
         }
         if (ImGui::IsItemHovered())
         {
            ImGui::SetTooltip("Adds HDR highlights to pre-rendered videos (HDR only).");
         }
         ImGui::BeginDisabled(!video_auto_hdr);
         slider("Video HDR Boost", &gs.VideoAutoHDRBoost, gs_def.VideoAutoHDRBoost, "VideoAutoHDRBoost", 1.f, "Video highlight strength (0 = off).");
         ImGui::EndDisabled();
      }

      // The final grade dithers in SDR and HDR alike, so this stays outside the HDR gate above.
      bool dithering = gs.Dithering > 0.5f;
      if (ImGui::Checkbox("Dithering", &dithering))
      {
         gs.Dithering = (dithering ? 1.f : 0.f);
         device_data.cb_luma_global_settings_dirty = true;
         reshade::set_config_value(nullptr, NAME, "Dithering", gs.Dithering);
      }
      if (ImGui::IsItemHovered())
      {
         ImGui::SetTooltip("Reduces gradient banding.");
      }

      ImGui::SeparatorText("UI");
      ImGui::Checkbox("Hide Gameplay UI", &g_hide_ui); // Session-only to avoid a confusing HUD-less restart.
      if (ImGui::IsItemHovered())
      {
         ImGui::SetTooltip("Disables the in-game UI.");
      }
   }

   void PrintImGuiAbout() override
   {
      ImGui::PushTextWrapPos(0.f);
      ImGui::Text(
         "Luma for \"The Witcher 2: Assassins of Kings Enhanced Edition\" is developed by DristoforColumb and is open source and free.\n"
         "It adds HDR, DLAA or FSR 3 native anti-aliasing, and replaces the game's FXAA with SMAA and its SSAO with XeGTAO, plus 16x anisotropic filtering.\n"
         "It runs through dgVoodoo2 (DirectX 9 -> 11).\n"
         "Enable SSAO in the game's video settings for XeGTAO to apply; SMAA works either way.\n"
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
                  "\nDICE (HDR tonemapper)"
                  "\nMacLeod-Boynton hue emulation (RenoDX)"
                  "\nSMAA (Iryoku)"
                  "\nXeGTAO (Intel)"
                  "\nAMD FidelityFX (RCAS + FSR 3)"
                  "\nNVIDIA NGX (DLSS)"
                  "\ndgVoodoo2 by Dege (DirectX 9 -> 11 wrapper, required)");
   }
};

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
   if (ul_reason_for_call == DLL_PROCESS_ATTACH)
   {
      Globals::SetGlobals(PROJECT_NAME, "The Witcher 2 Luma mod");
      Globals::DEVELOPMENT_STATE = Globals::ModDevelopmentState::Finished;

      // scRGB fp16 swapchain (the game's backbuffer is b8g8r8x8/b8g8r8a8 8-bit).
      swapchain_format_upgrade_type = TextureFormatUpgradesType::AllowedEnabled;
      swapchain_upgrade_type = SwapchainUpgradeType::scRGB;

      // Exclusive fullscreen -> borderless. force_borderless extends that to LEAVING fullscreen, so the title
      // bar cannot come back after a mode switch or an alt-tab.
      prevent_fullscreen_state = true;
      force_borderless = true;

      // The only resource still clipping the HDR signal is dgVoodoo's swapchain-resolution present-blit
      // intermediate. Upgrade it INDIRECTLY: it is wrapper-internal, and changing its creation format breaks
      // the translator's bookkeeping, giving a black screen even in menus.
      // One format plus the size filters keeps this to a single extra fp16 mirror at 4K; a broad list hit
      // bad_alloc in this 32-bit process. Fullscreen only: that path's intermediate is r8g8b8a8_typeless.
      texture_format_upgrades_type = TextureFormatUpgradesType::AllowedEnabled;
      enable_indirect_texture_format_upgrades = true; // creation-time mirrors, substituted at bind
      enable_chain_indirect_texture_format_upgrades = ChainTextureFormatUpgradesType::DirectDependencies;
      texture_upgrade_formats = {
         reshade::api::format::r8g8b8a8_typeless, // dgVoodoo's present-blit intermediates (narrow list: 32-bit VA budget)
      };
      // "No1Px" as in BL2, ME1 and MoHA: dgVoodoo fills every unused SRV slot with 1x1 placeholders (devkit: a 1x1
      // r8g8b8a8 texture and a 1x1 cube on every post draw), and a 1x1 trivially passes the aspect filter, so core would
      // mirror a typeless one too (and assert in DEVELOPMENT).
      texture_format_upgrades_2d_size_filters = (uint32_t)TextureFormatUpgrades2DSizeFilters::SwapchainResolution | (uint32_t)TextureFormatUpgrades2DSizeFilters::SwapchainAspectRatio | (uint32_t)TextureFormatUpgrades2DSizeFilters::No1Px;

      // AF16x. A sampler census found only MIN_MAG_MIP_LINEAR (0x15, world), MIN_MAG_LINEAR_MIP_POINT (0x14,
      // post/UI) and MIN_MAG_MIP_POINT, not one anisotropic sampler: the game has no AF option. Mode 4 alone is
      // therefore a no-op, and force_upgrade_linear_samplers is what buys AF by promoting the trilinear class.
      // The census also reports MipLODBias 0.000 everywhere, so there is no negative bias to clamp.
      enable_samplers_upgrade = true; // boot-time only (cannot change after device creation)
      samplers_upgrade_mode = 4;
      force_upgrade_linear_samplers = true;

      game = new TheWitcher2Game();
   }
   else if (ul_reason_for_call == DLL_PROCESS_DETACH)
   {
      TheWitcher2Game::UnregisterEvents();
      if (RenderArea::installed)
      {
         MH_DisableHook(MH_ALL_HOOKS);
         MH_Uninitialize();
      }
   }

   CoreMain(hModule, ul_reason_for_call, lpReserved);

   return TRUE;
}
