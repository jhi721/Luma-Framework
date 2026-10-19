#define GAME_BAYONETTA 1

#define GEOMETRY_SHADER_SUPPORT 0
#define DISABLE_AUTO_DEBUGGER 1
// The UI pixel shaders get the clamp of the 8-bit canvas they were made for (see "PatchShaderBytecodeSync")
#define LUMA_PATCH_BYTECODE_SYNC 1
// Outside DEVELOPMENT only this define makes original_draw_dispatch_func non-null (the scene copy and the present blit draw it)
#define ENABLE_POST_DRAW_DISPATCH_CALLBACK 1
// The motion vector draw key reads "last_draw_dispatch_data", which Publishing only fills with this
#define ENABLE_DRAW_DISPATCH_DATA_CACHE 1
// Luma TAA among the upscalers (in process; DLSS and FSR 3 run in the SR bridge's x64 helper, "UseLumaSRBridge"). Off for now: the
// floor flickers under it on a static frame (DLAA and FSR don't).
#define ENABLE_LUMA_TAA 0
// The game has MSAA but no shader AA: SMAA is added on the canvas after the scene copy (see "DrawSMAAOnCanvas")
#define ENABLE_SMAA 1
// SMAA's area texture without the U-shape smoothing (see Luma_SMAA_impl.hlsl), as TW1 and ME1
#define SMAA_SMOOTH_U_SHAPES 0
// RCAS sharpening after SMAA, opt-in (ME1/TW2)
#define ENABLE_RCAS 1

#include "..\..\Core\core.hpp"
#include "MotionVectorPatches.h"
#include "..\..\Core\includes\patched_draws.h"
#include "..\..\Core\includes\coverage_patch.h"

#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")

// Bayonetta (PC, 2017) is x86 D3D9Ex: it runs through dgVoodoo (D3D9 -> D3D11), so every hash below is a dgVoodoo translation.
// Hashes come from an offline mapping of the game's shader.dat through both dgVoodoo builds. Each pair is dgVoodoo 2.87.5 (ps_5_0, the
// main zip) then 2.81.3 (ps_4_0, the Linux zip); the 2.81.3 replacements are forwarders to the 2.87.5 bodies (same signature and rows).
// filter00: per-channel 1D curve LUT + saturation + vertical colour gradient (the grade, drawn before bloom).
// filter04: the same grade with the curve on Y' only (GradeLuma_0xB7A08D4A).
const ShaderHashesList shader_hashes_Grade = {.pixel_shaders = {0x719339A9, 0x3BE8E47F, 0xB7A08D4A, 0x2D4416C4}};
// filtershadercopytex, the game's copy for everything. Its ResetRender draw (the scene into the D3D9 back buffer, before the UI)
// carries the HDR display map in the replacement (Copy_0x91A6C437).
const ShaderHashesList shader_hashes_Copy = {.pixel_shaders = {0x91A6C437, 0x7584B641}};
// dgVoodoo's present blit (also TW1's, the same on both builds): when the swapchain format differs from the D3D9 back buffer's,
// dgVoodoo copies the back buffer into an intermediate texture (CopyResource) and draws that into the swapchain with this.
const ShaderHashesList shader_hashes_DgVoodooPresent = {.pixel_shaders = {0x880A17D3}};
// filtershaderssao: reads the depth prepass (R32F 1 - z/w) on t2, the SMAA predication source
const ShaderHashesList shader_hashes_SSAO = {.pixel_shaders = {0x40AF18CC, 0xC32759B1}};
// The passes whose kernels step in pixels of their target, by "g_TargetSize" c194 (cb4[202], the target's size): the shadow mask blurs
// (filtershadow_1 to _3), the DoF gathers (filter08_0 to _4) and the SSAO (filtershaderssao). Authored at 1280x720, at 4K each kernel was
// a third of its size (PCGamingWiki: razor sharp shadows, little depth of field). Their c194 reads come from "TARGET_SIZE_CB_SLOT" instead
// ("PatchShaderBytecodeSync"), the target's size at 720p with "g_resolution_scaled_effects" (all of them draw into scene sized targets).
// Not the shadow receivers (modelshadershadowrec00, shadowfixed): their PCF offsets are also c194 based (the screen's size, read in game)
// but step on the point sampled 4096 shadow map, so at 720p's size the taps landed ~1.6 texels apart and drew offset copies of the
// shadow edges (stripes in game). Pairs (2.87.5, 2.81.3) in that order.
const ShaderHashesList shader_hashes_PixelKernels = {.pixel_shaders = {0x0788DCAD, 0xB02CEC5A, 0xA7C3CCD7, 0x60FA1018, 0x4894A725, 0xD4F6B2D2, 0x0B1A2B23, 0x917C0C95, 0x58C43DE9, 0x11E66C0B, 0x2A8528AE, 0xA801906F, 0x2B93FD4F, 0xA0BD8BBA, 0x40D84AAB,
                                                        0x29CE44D6, 0x40AF18CC, 0xC32759B1}};
// The shadow receivers, replaced ("ShadowReceive_*": bilinear PCF at the map's size in place of the 512 texel grid's weights): they step
// their taps by the kernel target size too, without the c194 redirection of their originals. rec00_xxxxx, rec00_bxxxx, shadowfixed;
// pairs (2.87.5, 2.81.3).
const ShaderHashesList shader_hashes_ShadowReceivers = {.pixel_shaders = {0x98BD17D3, 0x5088AF5D, 0x45E82A5F, 0x8B9A11DF, 0xCDA8ACDD, 0x06EB4448}};
// The Sofdec movies' Y'CbCr -> R'G'B', drawn on the canvas after the scene (Video_0xC60649FD)
const ShaderHashesList shader_hashes_Video = {.pixel_shaders = {0xC60649FD, 0xED0DA20F}};
// The bloom chain (_tools/bayonetta/NOTES.md "Bloom"), all into the fixed size 320x176 bloom target: filter05 (the scene into level 0,
// then each level's threshold and halving into the next), filter02 / filter01 (each level's V / H blur), then copytex (the levels added
// into level 0, and the composite onto the scene)
const ShaderHashesList shader_hashes_BloomSource = {.pixel_shaders = {0x8B6A8D61, 0x29FE50C2}};
const ShaderHashesList shader_hashes_BloomBlur = {.pixel_shaders = {0x91CC01D3, 0xA5207DF1, 0xCE62BA4B, 0x4B605EEB}};
// The scene's first post pass, which ends a motion vector frame (the fill and the upscaler run right before it): OtPost's motion blur
// (87A169B9) and its cm* passes, BlurSrcWeight (651AC335) and BlurWeight (45BE027E), the DoF filter08 passes, then the grade. The frame
// before them: prepass + opaque, SSAO, the scene copy, ~400 blended particle draws (resolves with MSAA only). Pairs: modelshaderblur,
// blurcm4, blurcm8, blurcme, blurcmx (= blurcmy), blursrcweight, blurweight, filter08, filter08_0 to _4, filter00, filter04.
const ShaderHashesList shader_hashes_ScenePost = {.pixel_shaders = {0x87A169B9, 0x5BF04DF8, 0x7EE1D9A2, 0x45A01432, 0x3669A853, 0x5BF73D0E, 0xDBAC8B89, 0x0C72D63B, 0xAC7672ED, 0x163BC9B9, 0x651AC335, 0x988429CF, 0x45BE027E, 0x352E7E25, 0x9D71029B, 0xCCDA0B04, 0x0B1A2B23, 0x917C0C95, 0x58C43DE9, 0x11E66C0B, 0x2A8528AE, 0xA801906F, 0x2B93FD4F, 0xA0BD8BBA, 0x40D84AAB, 0x29CE44D6, 0x719339A9, 0x3BE8E47F, 0xB7A08D4A, 0x2D4416C4}};
// Screen space refraction in the scene: espshadershimmer, shimmer2, shimmerblur, waterwave. They redraw the scene copy displaced
// (rain sheets, heat haze), often over the whole screen with a quad that follows the camera: their own motion vectors (near zero)
// would replace the moving background's under them, and DLSS smeared the cutscenes' panning. Jittered, no motion vectors.
const ShaderHashesList shader_hashes_SceneRefraction = {.pixel_shaders = {0x4AFBF378, 0xCA4B5A1A, 0xEC4DF64B, 0x0CE2225B, 0x0B7B615E, 0xADD71AF8, 0x8CD13FE4, 0x9F85DEF7}};
// ModelShaderBlurSrcWeight, the skinned velocity for the motion blur, into an A8R8G8B8 target that the upgrades make fp16: the blur
// passes (cm*) decode it as (x - 128/255) * 0.25 UV, so unclamped it blurred across most of the screen and an unquantized 0.5 offset
// still objects by ~2 pixels. Clamped and rounded as the 8-bit target did.
const std::unordered_set<uint32_t> velocity_pixel_shaders = {0x651AC335, 0x988429CF};
// Drawn on the canvas after the scene: the 22 "idshader*" (UI, 2D effects), the runtime-compiled "HwShader*" (flat colour, textured
// quad, font). Some UI sprites output colours above 1 that the 8-bit canvas clipped. Pairs: idshaderbase, blur_p1, blur_p2, mask,
// mask2, maskandps, maskandpsedge, maskandreverseps, maskandreversepsedge, maskps, maskpsedge, maskreverse, maskreverse2,
// maskreverseps, maskreversepsedge, mess, mono, overlay, prev, previd, shimmer, shimmerreverse; HwShaderCharacter (= PFT), PF, PFV.
const std::unordered_set<uint32_t> canvas_clamped_pixel_shaders = {0x89843FB5, 0xA3B5337A, 0x24216ACC, 0x1F874192, 0x0BEC2A08, 0x8F6C7857, 0xF69D8D0A, 0x38CD0037, 0xA6B8C979, 0x44078C71, 0xF117E74C, 0x30842062, 0x21203CE7, 0xDBCA953B, 0x2C0D108F, 0xC2D1B526, 0x2B2C8854, 0xF519B6CE, 0x0961A82E, 0xE4BBB0B8, 0x157EFEF1, 0x9C79E1C6, 0xC32416F2, 0x8DB8D7DD, 0x7FAB7814, 0x3F10F71A, 0x1138503C, 0xDB514E84, 0xAA13DF37, 0xDBFAB108, 0xC6612FB9, 0x602A5F60, 0xF5CB254B, 0xE588CA92, 0x10E4B174, 0x75DAF3EF, 0xC288FB0A, 0x76E5C1D4, 0x7C5B0BBD, 0x0A41A152, 0x2C8B9F07, 0x8BE974D6, 0x16D99ADC, 0x9ACD218A, 0x30FFBB4D, 0xD4129BC5, 0x04A44467, 0x219808B4, 0x142D02C1, 0x95AA780A};

static bool g_msaa_resolve_enable = true; // HDR-weighted MSAA resolve (Core's "DrawMSAAResolve()") on the fp16 mirrors
#if DEVELOPMENT
static bool g_msaa_coverage_enable = true; // The alpha tested materials' alpha to coverage (with the resolve)
static bool g_msaa_resolve_wide = false;   // The resolve's 1 pixel radius filter ("MSAA Resolve Wide PS"), for A/B
#else
constexpr bool g_msaa_coverage_enable = true;
constexpr bool g_msaa_resolve_wide = false;
#endif
static bool g_smaa_enable = true;
static bool g_smaa_t2x = false;             // SMAA T2x: a two phase jitter and the previous frame, through the upscalers' motion vectors
static bool g_smaa_predication = true;      // On geometry, from the depth prepass's edges
static float g_smaa_pred_tolerance = 0.02f; // A fraction of view depth (TW2/ME1's)
static float g_rcas_sharpness = 0.f;        // RCAS on the antialiased canvas, 0 = off
static bool g_hide_ui = false;              // Session-only, never persisted: a stuck "on" would look like a broken HUD
#if DEVELOPMENT
static bool g_smaa_pred_debug = false; // The predication mask instead of the antialiased canvas
#endif
// The pixel sized kernels at their 1280x720 size on screen ("shader_hashes_PixelKernels"); off: the PC version's, smaller as the
// resolution rises
static bool g_resolution_scaled_effects = true;
static bool g_fix_frame_pacing = true;        // The game's frame limiters replaced (see "FrameLimiter")
static bool g_60_fps_cutscenes = true;        // The loop's 30 fps bit kept clear (see "FrameLimiter::Apply60FpsCutscenes")
static bool g_luma_bloom_enable = true;       // The game's bloom chain drawn at a higher resolution (see "DrawBloomPass")
static bool g_high_resolution_shadows = true; // An 8192 shadow map at the highest shadow quality (see "ShadowMap")
#if DEVELOPMENT
static int g_bloom_scale_override = 0; // Luma Bloom's scale, 0 = from the resolution
#else
constexpr int g_bloom_scale_override = 0;
#endif
// The target size the pixel kernel passes read instead of c194, not used by dgVoodoo (b3, b4) nor Luma (b12, b13)
static constexpr UINT TARGET_SIZE_CB_SLOT = 10;
static bool g_gtao_enable = true; // XeGTAO replaces the game's SSAO (see "RunXeGTAO")
// XeGTAO's knobs, "register(b9)" in Luma_Bayonetta_XeGTAO.hlsl and Luma_Bayonetta_XeGTAOApply.hlsl
static constexpr UINT GTAO_KNOBS_CB_SLOT = 9;
static float g_gtao_final_value_power = 1.f; // Darkness dial (higher = darker)
static float g_gtao_depth_scale = 1.f;       // View depth divisor (game units -> ~meters)
static float g_gtao_radius_override = 0.f;   // > 0 overrides the shader's EFFECT_RADIUS (view units after the depth scale)
#if DEVELOPMENT
static int g_gtao_debug_view = 0; // 0 off, 1 depth gradient, 2 normals, 3 AO x8, 4 edges (the shader's debug blocks are DEVELOPMENT only)
#endif
// The reactive masks' resources go after this many frames without their pass
constexpr uint32_t IDLE_RELEASE_FRAMES = 600;

#if DEVELOPMENT
static bool g_mv_enable = false;         // Motion vectors without an upscaler
static bool g_mv_debug_view = false;     // Core's debug draw of the motion vector target
static bool g_mv_force_jitter = false;   // The projection jitter without an upscaler
static bool g_bound_state_check = false; // The tracked bindings against the bound ones ("mv.bound_state_mismatches")
static bool g_sr_reactive_enable = true;
static float g_sr_reactive_scale = 1.f;      // The alpha blended draws' reactivity, scaled (AMD's default 1)
static float g_sr_reactive_threshold = 0.5f; // Under it 0, over it 0.9 (AMD's 0.2; Mass Effect 2007's and Borderlands 2's 0.5); 0: the scaled reactivity, capped at 0.9
static bool g_sr_reactive_debug_view = false;
#else
constexpr bool g_mv_enable = false;
constexpr bool g_mv_force_jitter = false;
constexpr bool g_bound_state_check = false;
constexpr bool g_sr_reactive_enable = true;
constexpr float g_sr_reactive_scale = 1.f;
constexpr float g_sr_reactive_threshold = 0.5f;
#endif
// "DrawWithMotionVectors" refusals, counted per frame in DEVELOPMENT ("mv.rejected.*")
enum MotionVectorReject
{
   REJECT_EXTRA_TARGET,
   REJECT_NO_SCENE,
   REJECT_DEPTH_TEST,
   REJECT_BLEND,
   REJECT_OTHER_DEPTH_COLOR,
   REJECT_FORMAT,
   REJECT_SIZE,
   REJECT_CREATE,
   REJECT_SHADERS,
   REJECT_NO_CAMERA,
   REJECT_COUNT
};
#if DEVELOPMENT
constexpr const char* MOTION_VECTOR_REJECT_NAMES[REJECT_COUNT] = {"extra_target", "no_scene", "depth_test", "blend", "other_depth_color", "format", "size", "create", "shaders", "no_camera"};
#endif

// A draw's blend for the reactive masks (see "ClassifyBoundBlend")
enum class ReactiveBlend : uint8_t
{
   NONE,
   ALPHA,
   ADDITIVE,
};

// A pixel shader's patch: the motion vector target, the mask target of an alpha blended or additive draw, or an alpha blended draw's own motion vectors; or the MSAA alpha to coverage
enum class PixelShaderPatch : uint8_t
{
   MOTION_VECTORS,
   MASK_ALPHA,
   MASK_ADDITIVE,
   BLENDED,
   COVERAGE,
   COUNT
};
constexpr const char* PIXEL_SHADER_PATCH_NAMES[size_t(PixelShaderPatch::COUNT)] = {"MV PS", "MV PS (mask)", "MV PS (mask)", "MV PS (blended)", "Coverage PS"};

// The Steam executable, whose fixed addresses the game code patches below use (relocations stripped: fixed base)
static bool IsSteamExecutable()
{
   constexpr DWORD STEAM_TIME_DATE_STAMP = 0x591AD330;
   const auto* const image = reinterpret_cast<const uint8_t*>(GetModuleHandleW(nullptr));
   return reinterpret_cast<const IMAGE_NT_HEADERS*>(image + reinterpret_cast<const IMAGE_DOS_HEADER*>(image)->e_lfanew)->FileHeader.TimeDateStamp == STEAM_TIME_DATE_STAMP;
}

// The game's frame limiters, replaced with "g_fix_frame_pacing" (_tools/bayonetta/NOTES.md "Frame limiter and 30 fps cutscenes",
// docs/Frame-Limiter-Best-Practices.md FL-n). The main loop calls FUN_00c54430 right before Present at 60 fps, FUN_00c54340 at 30
// (cutscenes): both SleepEx until 2 ms before a float32 ms deadline stepping 16.683 ms (59.94 fps), so frames leave up to 2 ms early,
// beat against a 60 Hz display (a repeated frame every ~17 s) and lose precision with play time. The replacement keeps their contract
// with the loop: the previous and next deadline globals (float ms on the game's QPC time base; the movie sync rewrites the next one),
// the late flag (the loop skips the next Present), its counter and the resync the loop allows. Game speed is per frame: the cap stays.
namespace FrameLimiter
{
   uint8_t* const LIMIT_60 = reinterpret_cast<uint8_t*>(0xC54430);
   uint8_t* const LIMIT_30 = reinterpret_cast<uint8_t*>(0xC54340);
   // Both start with "movss xmm0, [next deadline]; sub esp, 0Ch; movss [previous deadline], xmm0"
   constexpr std::array<uint8_t, 19> PROLOGUE = {0xF3, 0x0F, 0x10, 0x05, 0x28, 0x95, 0xBB, 0x05, 0x83, 0xEC, 0x0C, 0xF3, 0x0F, 0x11, 0x05, 0x24, 0x95, 0xBB, 0x05};
   float* const next_deadline_ms = reinterpret_cast<float*>(0x5BB9528);
   float* const previous_deadline_ms = reinterpret_cast<float*>(0x5BB9524);
   uint32_t* const late = reinterpret_cast<uint32_t*>(0x5BB9578);
   int32_t* const late_count = reinterpret_cast<int32_t*>(0x5BB957C); // +1 per late frame at 60 fps, +2 at 30
   const uint32_t* const resync_allowed = reinterpret_cast<const uint32_t*>(0xF45CC8);
   const int64_t* const time_base = reinterpret_cast<const int64_t*>(0x5BCFBF8); // The QPC at the game's start (FUN_00c78090)
   // The game's late thresholds (-24.19 and -48.38 ms) are 1.45 of its steps
   constexpr double LATE_PERIODS = 1.45;

   // FUN_0045b4a0, run by the loop before each frame's 30/60 choice, applies a pending 30 fps request (DAT_051adefc, DAT_051adf00) to
   // the loop's 30 fps bit (DAT_051ade24 0x200000); a 30 fps event (real-time cutscene) also sets the bit at its start (FUN_00536010).
   // The loop updates the game at 60 Hz either way: the bit only skips every other update's draw and Present. Patched (BayoHook's bytes:
   // "jz" no request -> 2 nops, "jz" request off -> "jmp") the function clears the bit before every frame.
   uint8_t* const APPLY_30_FPS_REQUEST = reinterpret_cast<uint8_t*>(0x45B4A0);
   constexpr std::array<uint8_t, 27> APPLY_30_FPS_REQUEST_ORIGINAL = {0x83, 0x3D, 0xFC, 0xDE, 0x1A, 0x05, 0x00, 0x74, 0x28, 0x83, 0x3D, 0x00, 0xDF, 0x1A, 0x05, 0x00, 0xC7, 0x05, 0xFC, 0xDE, 0x1A, 0x05, 0x00, 0x00, 0x00, 0x00, 0x74};
   constexpr std::array<uint8_t, 27> APPLY_30_FPS_REQUEST_PATCHED = {0x83, 0x3D, 0xFC, 0xDE, 0x1A, 0x05, 0x00, 0x90, 0x90, 0x83, 0x3D, 0x00, 0xDF, 0x1A, 0x05, 0x00, 0xC7, 0x05, 0xFC, 0xDE, 0x1A, 0x05, 0x00, 0x00, 0x00, 0x00, 0xEB};

   using Limiter = void(__cdecl*)();
   Limiter original_60 = nullptr;
   Limiter original_30 = nullptr;
   bool installed = false;
   bool cutscenes_patchable = false; // FUN_0045b4a0 as shipped (not already patched by another mod)
   bool cutscenes_patched = false;

   // The game's main thread only. No VBlank phase lock (FL-9): D3DKMTGetScanLine, the only live VBlank source under independent flip,
   // stalls ~3.7 ms in 0.1-0.5% of its calls (p999 3.72 ms, measured 2026-10-19), a hitch before Present every few seconds.
   int64_t frequency = 0;
   int64_t next = 0; // QPC
   // The deadline last written: any other value came from the game (its start, the movie sync, its own limiter while toggled off)
   float written_next_ms = std::numeric_limits<float>::quiet_NaN();
   HANDLE timer = nullptr;
   int64_t spin_margin = 0; // FL-7
   uint32_t display_query_frame = 0;
   int64_t refresh_period = 0; // QPC ticks, 0 unknown
#if DEVELOPMENT
   uint32_t late_frames = 0;
   uint32_t resyncs = 0;
   uint32_t adopted_deadlines = 0;
   uint32_t period_us = 0;
   uint32_t spin_margin_us = 0;
#endif

   void Limit(uint32_t frames)
   {
      const auto qpc = []
      {
         LARGE_INTEGER now;
         QueryPerformanceCounter(&now);
         return now.QuadPart;
      };
      const double ticks_per_ms = double(frequency) / 1000.0;
      const float game_next_ms = *next_deadline_ms;
      *previous_deadline_ms = game_next_ms;
      if (game_next_ms != written_next_ms)
      {
         next = *time_base + int64_t(double(game_next_ms) * ticks_per_ms);
#if DEVELOPMENT
         adopted_deadlines++;
#endif
      }

      // Each second: the display's period (DWM's is right under independent flip, its VBlank time isn't: FL-9a)
      if (display_query_frame++ % 60 == 0)
      {
         DWM_TIMING_INFO timing{.cbSize = sizeof(timing)};
         refresh_period = (SUCCEEDED(DwmGetCompositionTimingInfo(nullptr, &timing)) ? int64_t(timing.qpcRefreshPeriod) : 0);
      }
      // FL-9b: whole refreshes when that's the game's rate within 0.5% (60, 120, 240 and 59.94 Hz displays), else exactly 60 fps
      const int64_t game_period = int64_t(frames) * frequency / 60;
      const int64_t refreshes = (refresh_period > 0 ? (game_period + refresh_period / 2) / refresh_period : 0);
      const bool display_period = refreshes > 0 && std::abs(refreshes * refresh_period - game_period) * 200 <= game_period;
      const int64_t period = (display_period ? refreshes * refresh_period : game_period);

      // FL-4/5: a high resolution timer until the measured wake-up lateness before the deadline, then a spin
      int64_t now = qpc();
      const int64_t wake = next - spin_margin;
      if (timer && wake > now)
      {
         const LARGE_INTEGER due{.QuadPart = -(wake - now) * 10'000'000 / frequency}; // Relative, in 100 ns
         if (SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE) && WaitForSingleObject(timer, INFINITE) == WAIT_OBJECT_0)
         {
            now = qpc();
            // FL-7: the worst recent lateness (decaying by 1/64 a frame) plus a guard
            const int64_t guard = frequency / 4000;
            spin_margin = std::clamp((std::max)(spin_margin - spin_margin / 64, now - wake + guard), guard, frequency / 250);
         }
      }
      while (now < next)
      {
         YieldProcessor();
         now = qpc();
      }

      const bool is_late = now - next > int64_t(LATE_PERIODS * double(period));
      *late_count = (is_late ? *late_count + int32_t(frames) : 0);
      *late = is_late;
#if DEVELOPMENT
      late_frames += is_late;
#endif
      if (*resync_allowed && *late_count > int32_t(4 * frames - 1))
      {
         *late_count = 0;
         next = now;
#if DEVELOPMENT
         resyncs++;
#endif
      }
      next += period;
      written_next_ms = float(double(next - *time_base) / ticks_per_ms);
      *next_deadline_ms = written_next_ms;
#if DEVELOPMENT
      period_us = uint32_t(period * 1'000'000 / frequency);
      spin_margin_us = uint32_t(spin_margin * 1'000'000 / frequency);
#endif
   }

   // On the game's main thread (from the limiters), the thread that runs FUN_0045b4a0
   void Apply60FpsCutscenes()
   {
      const bool patch = cutscenes_patchable && g_60_fps_cutscenes;
      if (patch == cutscenes_patched)
         return;
      const auto& bytes = (patch ? APPLY_30_FPS_REQUEST_PATCHED : APPLY_30_FPS_REQUEST_ORIGINAL);
      DWORD old_protect;
      if (!VirtualProtect(APPLY_30_FPS_REQUEST, bytes.size(), PAGE_EXECUTE_READWRITE, &old_protect))
         return;
      std::memcpy(APPLY_30_FPS_REQUEST, bytes.data(), bytes.size());
      VirtualProtect(APPLY_30_FPS_REQUEST, bytes.size(), old_protect, &old_protect);
      FlushInstructionCache(GetCurrentProcess(), APPLY_30_FPS_REQUEST, bytes.size());
      cutscenes_patched = patch;
   }

   void __cdecl Limit60()
   {
      Apply60FpsCutscenes();
      if (g_fix_frame_pacing)
      {
         Limit(1);
      }
      else
      {
         original_60();
      }
   }

   // The caller pushes an unused argument (cdecl: it pops it)
   void __cdecl Limit30()
   {
      Apply60FpsCutscenes();
      if (g_fix_frame_pacing)
      {
         Limit(2);
      }
      else
      {
         original_30();
      }
   }

   void Install()
   {
      if (!IsSteamExecutable() || std::memcmp(LIMIT_60, PROLOGUE.data(), PROLOGUE.size()) != 0 ||
          std::memcmp(LIMIT_30, PROLOGUE.data(), PROLOGUE.size()) != 0 || !InitializeMinHook())
         return;
      LARGE_INTEGER qpc_frequency;
      QueryPerformanceFrequency(&qpc_frequency);
      frequency = qpc_frequency.QuadPart;
      spin_margin = frequency / 1000;
      timer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
      if (!timer)
      {
         timer = CreateWaitableTimerExW(nullptr, nullptr, 0, TIMER_ALL_ACCESS); // Before Windows 10 1803
      }
      // FL-6: Windows 11 ignores the timer resolution and slows the process down when its window is occluded or in the background
      PROCESS_POWER_THROTTLING_STATE throttling{.Version = PROCESS_POWER_THROTTLING_CURRENT_VERSION, .ControlMask = PROCESS_POWER_THROTTLING_EXECUTION_SPEED | PROCESS_POWER_THROTTLING_IGNORE_TIMER_RESOLUTION, .StateMask = 0};
      SetProcessInformation(GetCurrentProcess(), ProcessPowerThrottling, &throttling, sizeof(throttling));
      installed = MH_CreateHook(LIMIT_60, reinterpret_cast<void*>(&Limit60), reinterpret_cast<void**>(&original_60)) == MH_OK &&
                  MH_CreateHook(LIMIT_30, reinterpret_cast<void*>(&Limit30), reinterpret_cast<void**>(&original_30)) == MH_OK &&
                  MH_EnableHook(LIMIT_60) == MH_OK && MH_EnableHook(LIMIT_30) == MH_OK;
      if (!installed)
      {
         MH_RemoveHook(LIMIT_60);
         MH_RemoveHook(LIMIT_30);
         return;
      }
      cutscenes_patchable = std::memcmp(APPLY_30_FPS_REQUEST, APPLY_30_FPS_REQUEST_ORIGINAL.data(), APPLY_30_FPS_REQUEST_ORIGINAL.size()) == 0;
   }
} // namespace FrameLimiter

// The shadow map's size: FUN_00402270 (applying the options) sets DAT_05abbe6c from {1024, 2048, 4096} (DAT_00d9d74c) by the shadow
// quality (options +0x17c, DAT_00fde17c), and FUN_0045b320 (after the options, level loads) recreates the map's targets (DAT_05abbe80,
// DAT_051af108) when it differs from their size (DAT_05abbe70). Every other use reads the size from there (clears, viewports); the
// receivers' replacements filter at the map's real size ("Includes/ShadowReceive.hlsl").
namespace ShadowMap
{
   uint32_t* const SIZES = reinterpret_cast<uint32_t*>(0xD9D74C); // .rdata
   const uint32_t* const QUALITY = reinterpret_cast<const uint32_t*>(0xFDE17C);
   uint32_t* const SIZE = reinterpret_cast<uint32_t*>(0x5ABBE6C);
   constexpr std::array<uint32_t, 3> ORIGINAL_SIZES = {1024, 2048, 4096};
   constexpr uint32_t HIGH_RESOLUTION_SIZE = 8192; // 3 targets (R32F, RGBA8, D24S8): 768 MB, 576 more than at 4096
   bool patchable = false;

   void Install()
   {
      patchable = IsSteamExecutable() && std::memcmp(SIZES, ORIGINAL_SIZES.data(), sizeof(ORIGINAL_SIZES)) == 0;
   }

   // The highest quality's size; the current size follows at once when that quality is chosen, the targets with the game's next check
   void Apply()
   {
      if (!patchable)
         return;
      const uint32_t size = (g_high_resolution_shadows ? HIGH_RESOLUTION_SIZE : ORIGINAL_SIZES.back());
      if (!System::PatchMemory(&SIZES[2], &size, sizeof(size), System::PatchMemoryType::Data))
         return;
      if (*QUALITY == 2)
      {
         *SIZE = size;
      }
   }
} // namespace ShadowMap

struct BayonettaGameDeviceData final : public GameDeviceData
{
   // The present blit's source and dgVoodoo's D3D9 back buffer (the canvas), copied into it or straight into the swapchain
   std::atomic<uint64_t> present_source = 0;
   std::atomic<uint64_t> canvas = 0;
   // dgVoodoo draws the game's frame and its present blit from different threads, serialized on the immediate context
   std::atomic<bool> has_copied_scene_to_canvas = false;
   std::atomic<bool> has_drawn_reset_render = false; // The scene copy ran this frame: the canvas' later game draws are the UI (Hide UI)
   // The canvas right after ResetRender, before the UI: the present blit clamps what the UI added (Present_0x880A17D3)
   ComPtr<ID3D11Texture2D> scene_texture;
   ComPtr<ID3D11ShaderResourceView> scene_srv;
   // The game's 320x176 bloom target (the chain's first filter05 draws into it from the scene), identity only
   uint64_t bloom_target = 0;
   // Luma Bloom ("DrawBloomPass"): the target's chain at "bloom_scale" times its size. Each pass draws into the target's top left
   // (the level's size) and dgVoodoo then copies that region into a texture of the region's size, which the next passes sample (the
   // levels' last ones are what the composite adds up): "bloom_shadows" are those, by the game's texture, at the same scale
   // ("MirrorBloomCopy").
   struct BloomShadow
   {
      uint64_t game_texture = 0;
      ComPtr<ID3D11Texture2D> texture;
      ComPtr<ID3D11ShaderResourceView> srv;
   };
   uint32_t bloom_scale = 0;
   ComPtr<ID3D11Texture2D> bloom_texture;
   ComPtr<ID3D11RenderTargetView> bloom_rtv;
   ComPtr<ID3D11ShaderResourceView> bloom_srv;
   std::vector<BloomShadow> bloom_shadows;
   // The depth prepass as this frame's SSAO pass read it (null without SSAO): the SMAA predication source
   ComPtr<ID3D11ShaderResourceView> depth_srv;
   // What "shader_hashes_PixelKernels" read in place of c194 (see "BindPixelKernelTargetSize"), and its value
   com_ptr<ID3D11Buffer> pixel_kernel_target_size_buffer;
   std::array<float, 4> pixel_kernel_target_size = {};
   // XeGTAO (see "RunXeGTAO"): the scratch at the SSAO depth's size ("gtao_size", kept on a failed allocation so it isn't retried
   // every frame) and its knobs
   ComPtr<ID3D11Texture2D> gtao_depth_mips; // R32F view depth, 5 mips
   ComPtr<ID3D11UnorderedAccessView> gtao_depth_mip_uavs[5];
   ComPtr<ID3D11ShaderResourceView> gtao_depth_mips_srv;
   ComPtr<ID3D11Texture2D> gtao_working[2]; // R8G8 AO and edges, ping-pong
   ComPtr<ID3D11UnorderedAccessView> gtao_working_uavs[2];
   ComPtr<ID3D11ShaderResourceView> gtao_working_srvs[2];
   ComPtr<ID3D11Texture2D> gtao_output; // R8 visibility
   ComPtr<ID3D11UnorderedAccessView> gtao_output_uav;
   ComPtr<ID3D11ShaderResourceView> gtao_output_srv;
   uint2 gtao_size = {};
   com_ptr<ID3D11Buffer> gtao_knobs_buffer;
   // The camera: a copy of a depth prepass draw's vertex constants (dgVoodoo's cb4) and the rows of its two matrices there
   // ("CB::GTAOKnobs"), taken once per frame ("gtao_camera_taken"); the last frame's serves a frame without one. The prepass vertex
   // shaders' layouts by hash (see "ClassifyPrepassCamera").
   com_ptr<ID3D11Buffer> gtao_camera_buffer;
   uint2 gtao_camera_rows = {};
   bool gtao_camera_taken = false;
   std::unordered_map<uint32_t, uint2> gtao_camera_layouts;
   ComPtr<ID3D11Texture2D> smaa_predication;
   ComPtr<ID3D11UnorderedAccessView> smaa_predication_uav;
   ComPtr<ID3D11ShaderResourceView> smaa_predication_srv;
   com_ptr<ID3D11Buffer> smaa_predication_buffer;
   com_ptr<ID3D11Buffer> smaa_metrics_buffer;
   bool smaa_resources_live = false;
   // SMAA T2x (see "DrawSMAAOnCanvas"), set at present for the whole frame like "sr_active". The scene jitters only after a frame
   // the resolve ran ("t2x_phase" 0 or 1, -1 for none). The two canvas sized frames (linear colour, velocity length in alpha)
   // alternate; the other one is the history only if "t2x_frame", the last frame the resolve ran, was the frame before.
   bool t2x_active = false;
   int t2x_phase = -1;
   ComPtr<ID3D11Texture2D> t2x_frames[2];
   ComPtr<ID3D11RenderTargetView> t2x_frame_rtvs[2];
   ComPtr<ID3D11ShaderResourceView> t2x_frame_srvs[2];
   uint32_t t2x_frame = 0;
   // "msaa_resolved" this frame, "msaa_active" the last one (the scene's coverage draws come before this frame's resolves)
   std::atomic<bool> msaa_resolved = false;
   std::atomic<bool> msaa_active = false;
#if DEVELOPMENT
   // For the MCP "luma_dev_values" tool: this frame's counts and the last finished frame's
   struct AAStats
   {
      uint32_t msaa_resolves = 0; // Resolves drawn by "DrawMSAAResolve()"
      uint32_t smaa = 0;          // SMAA on the canvas
      uint32_t smaa_predicated = 0;
      uint32_t msaa_coverage = 0; // Draws swapped to their alpha to coverage pixel shader
   };
   AAStats aa_stats;
   AAStats aa_last_stats;
#endif

   // ---- DLAA / FSR 3 Native AA / Luma TAA with motion vectors and jitter from patched shaders (Mirror's Edge's path with The Witcher
   // 2's CPU cuts, one per-draw buffer: dgVoodoo's vc4) ----
   // DLSS and FSR run in the x64 helper of Core's SR bridge (the game is 32-bit, see "SRBridge.h"), Luma TAA in process.
   // An upscaler is picked, hasn't failed (it then gives way to SMAA until picked again) and the game's MSAA is off, latched at present
   // for the whole frame: a selection made after the motion vector state was set would otherwise run the upscaler on mixed state
   bool sr_active = false;
   // The upscaler's output goes back into the scene (and its copy, see "mv_scene_copy") without its alpha (the bloom source's
   // threshold input), drawn from this view of it
   com_ptr<ID3D11BlendState> sr_rgb_blend_state;
   com_ptr<ID3D11ShaderResourceView> sr_output_srv;

   std::atomic<bool> mv_active = false; // Motion vectors and jitter this frame: an upscaler or the DEV toggle (set at present)
   std::shared_mutex mv_mutex;
   // A game shader's patched version (null if refused), patched on first use, by its hash; a vertex shader's with the bytes of vc4 it
   // reads ("DXBC::ConstantBufferBytes": the previous frame's copy uploads only those), the vc4 byte offset of its camera rows (the
   // view projection or the world view projection, 0 for none: no motion vectors) and of its tie-break transform (0 for none)
   template <typename T>
   struct PatchedShader
   {
      com_ptr<T> shader;
      UINT read_size = 0;
      UINT camera_offset = 0;
      UINT translation_offset = 0;
   };
   std::unordered_map<uint32_t, PatchedShader<ID3D11VertexShader>> mv_vertex_shaders;
   // By "PixelShaderPatch" (the coverage one null for every shader without a masked material's clip, see "CoveragePatch::PatchPixelShader")
   std::unordered_map<uint32_t, PatchedShader<ID3D11PixelShader>> patched_pixel_shaders[size_t(PixelShaderPatch::COUNT)];
   // The motion vector target (sized like the scene; the alpha blending states blend it like their color, the others write it
   // unblended, see "OnCreateBlendState")
   com_ptr<ID3D11Texture2D> mv_texture;
   com_ptr<ID3D11RenderTargetView> mv_rtv;
   com_ptr<ID3D11UnorderedAccessView> mv_uav; // Null without typed UAV loads of its format (then no upscaler)
   // The upscaler's depth, built from the depth prepass by the fill (the game's depth has no shader resource view)
   com_ptr<ID3D11Texture2D> mv_device_depth;
   com_ptr<ID3D11UnorderedAccessView> mv_device_depth_uav;
   // The reactive and transparency & composition masks (FSR, Luma TAA), written by the fill from "mv_reactive_target"
   com_ptr<ID3D11Texture2D> mv_reactive;
   com_ptr<ID3D11UnorderedAccessView> mv_reactive_uav;
   com_ptr<ID3D11Texture2D> mv_transparency;
   com_ptr<ID3D11UnorderedAccessView> mv_transparency_uav;
   // What the alpha blended draws wrote (x reactive, y transparency & composition; max blended, see "OnCreateBlendState"), read by the
   // fill. Created when a mask user first runs, gone "IDLE_RELEASE_FRAMES" after the fill last wrote them.
   com_ptr<ID3D11Texture2D> mv_reactive_target;
   com_ptr<ID3D11RenderTargetView> mv_reactive_target_rtv;
   com_ptr<ID3D11ShaderResourceView> mv_reactive_target_srv;
   uint32_t sr_reactive_frame = 0;
   // A frame opens at its first mesh draw into the scene (the jitter is chosen there), starts at its first motion vector draw (the
   // target is cleared) and ends at the first post pass ("shader_hashes_ScenePost"), once per present.
   bool mv_scene_open = false;
   bool mv_scene_done = false;
   bool mv_frame_ended = true;
   bool mv_fill_pending = false;
   // The upscaler drew the last frame: only then the scene jitters (frames it skips would reach the screen jittered)
   bool mv_jitter_allowed = false;
   float sr_vert_fov = 1.0471976f; // FSR's vertical FOV (radians): the last camera's, 60 degrees until one is seen
   // The upscalers' near and far (game units): the last camera's with a usable projection, Core's defaults until one is seen
   float sr_near_plane = SR::SuperResolutionImpl::DrawData{}.near_plane;
   float sr_far_plane = SR::SuperResolutionImpl::DrawData{}.far_plane;
   // The scene's size, the one of its depth: the 16:9 rect of the canvas with PreserveAspect, not the output's. Everything the
   // upscaler reads is this size.
   uint2 mv_scene_size = {};
   com_ptr<ID3D11Resource> mv_depth; // The scene depth (the depth view's resource)
   // The fp16 scene the motion vector draws write, and the game's view of it (the upscaler's copy back)
   com_ptr<ID3D11Resource> mv_scene_color;
   com_ptr<ID3D11RenderTargetView> mv_scene_rtv;
   // The copy of the scene the first post pass reads, null if it reads none. The upscaler's output goes into both
   // ("Bayonetta Copy Back PS"). Found from the game's last CopyResource of the scene: the camera blur (cmx) also reads a copy of the
   // velocity target, whose description matches the scene's.
   com_ptr<ID3D11Resource> mv_scene_copy;
   com_ptr<ID3D11Resource> mv_scene_last_copy;
   com_ptr<ID3D11RenderTargetView> mv_scene_copy_rtv;
   com_ptr<ID3D11Buffer> mv_fill_buffer;
   // The depth prepass' R32F target (1 - z/w, next to the normals): where the fill reads the depth. "prepass_drawn": it was drawn into
   // this frame; "prepass_rtv" and "prepass_is_depth" classify the last target seen next to a second one.
   com_ptr<ID3D11Resource> prepass_depth;
   com_ptr<ID3D11ShaderResourceView> prepass_depth_srv;
   ID3D11RenderTargetView* prepass_rtv = nullptr;
   bool prepass_is_depth = false;
   bool prepass_drawn = false;
   // The projection jitter (pixels, +y down), chosen when the scene opens; its NDC offset is at VS "MotionVectorPatches::jitter_slot"
   // of every mesh draw depth tested against the scene
   std::array<float, 2> mv_jitter = {};
   std::array<float, 2> mv_jitter_ndc = {};  // The same offset in NDC (y up), as the jitter buffer holds it
   com_ptr<ID3D11ShaderResourceView> mv_srv; // SMAA T2x's view of the motion vector target
   bool mv_filled = false;                   // The fill wrote this frame's motion vectors (reset at present)
   com_ptr<ID3D11Buffer> mv_jitter_buffer;
   // Per-draw lookups kept for the next draw (reset when the scene opens): the jitter path's last depth view and whether it's the scene
   // depth, its last depth stencil state's depth test and write, the motion vector path's last accepted targets and the last blend
   // state's opacity, the last vertex and pixel shader's patched versions (owned by the maps above, never erased)
   ID3D11DepthStencilView* jitter_dsv = nullptr;
   bool jitter_dsv_scene = false;
   ID3D11DepthStencilState* jitter_depth_stencil_state = nullptr;
   bool jitter_depth_test = true;
   ID3D11DepthStencilView* mv_accepted_dsv = nullptr;
   ID3D11BlendState* mv_blend_state = nullptr; // See "ClassifyBoundBlend"
   bool mv_blend_opaque = true;
   bool mv_alpha_blended = false;
   ReactiveBlend mv_reactive_blend = ReactiveBlend::NONE;
   uint32_t mv_last_vertex_shader_hash = 0;
   const PatchedShader<ID3D11VertexShader>* mv_last_vertex_shader = nullptr;
   uint32_t mv_last_pixel_shader_hash = 0;
   ID3D11PixelShader* mv_last_pixel_shader = nullptr;
   PatchedDraws::BoundShader<ID3D11VertexShader> mv_bound_vertex_shader;
   PatchedDraws::BoundShader<ID3D11PixelShader> mv_bound_pixel_shader;
   // The scene's targets refused by "DrawWithMotionVectors" (other depth or color, format, size) and the reason, the targets
   // "OpenScene" refused (not the scene's); reset when the scene opens (the refused ones at present). The shadow maps and the
   // prepass' targets are refused once per frame, not per draw.
   ID3D11RenderTargetView* mv_refused_rtv = nullptr;
   ID3D11DepthStencilView* mv_refused_dsv = nullptr;
   MotionVectorReject mv_refused_reason = REJECT_COUNT;
   ID3D11RenderTargetView* open_refused_rtv = nullptr;
   ID3D11DepthStencilView* open_refused_dsv = nullptr;
   bool mv_jitter_bound = false;
   // The marker texels got their camera motion before the frame's first alpha blended motion vector draw (see "DrawWithMotionVectors")
   bool mv_markers_filled = false;
   // The immediate context's blend and depth stencil states, vc4 (VS "MotionVectorPatches::object_slot"), vertex buffer 0 and index
   // buffer as the game last bound them ("OnBindPipeline", "OnPushConstantBuffers", "OnBindVertexBuffers", "OnBindIndexBuffer"), so
   // the draw hooks don't query them (an AddRef and a Release each, several per draw). Not referenced: an object lives while bound.
   // Luma's own passes bind theirs natively and put the game's back. False after a bind this can't read (another add-on's combined
   // pipeline): queried then.
   ID3D11BlendState* bound_blend_state = nullptr;
   ID3D11DepthStencilState* bound_depth_stencil_state = nullptr;
   ID3D11Buffer* bound_object_buffer = nullptr;
   ID3D11Buffer* bound_vertex_buffer = nullptr;
   UINT bound_vertex_offset = 0;
   ID3D11Buffer* bound_index_buffer = nullptr;
   UINT bound_index_offset = 0;
   bool bound_states_tracked = true;

   // Pointer and "HashCombine" keys: one multiply spreads them over the buckets (std::hash runs FNV-1a on every byte)
   struct KeyHash
   {
      size_t operator()(uint64_t key) const noexcept
      {
         return size_t((key * 0x9E3779B97F4A7C15ull) >> 32);
      }
   };

   // CPU copies of the vc4 buffers the motion vector draws bind, by buffer (an entry registers it, empty until its first upload), from
   // a Map(WRITE_DISCARD) at its Unmap or an UpdateSubresource: a draw's constants are its buffer's latest upload. Unlocked: the
   // buffer hooks and the draws are all calls on the immediate context, which D3D11 requires its caller to serialize (dgVoodoo has no
   // deferred contexts). Not always from one thread: the D3D9 Reset of a resolution change maps from the game's window thread.
   using ConstantsCopy = std::shared_ptr<const std::vector<uint8_t>>;
   struct RegisteredConstants
   {
      // The last upload, rewritten in place: dgVoodoo maps a vc4 buffer for most draws, motion vector draws or not
      std::vector<uint8_t> latest;
      ConstantsCopy copy;      // "latest" whole, for the draws since that upload reading all of it (skinned: shared; made by the first)
      void* mapped = nullptr;  // Mapped now, until its Unmap
      uint32_t generation = 0; // "mv_constants_generation" at the upload: an older one predates a pause of the hooks
   };
   std::unordered_map<uint64_t, RegisteredConstants, KeyHash> mv_constants_copies;
   // Raised at present when motion vectors restart: the uploads missed while the hooks were idle make every older "latest" stale. A
   // counter rather than emptying the entries, which the frame's thread owns (dgVoodoo presents from another thread).
   std::atomic<uint32_t> mv_constants_generation = 1;
   // The first registered buffers, their sizes and entries (a node's address stays while the map grows), so a buffer hook finds them
   // without a lookup. With more registered, every buffer takes the map lookup.
   static constexpr uint32_t MAX_FILTERED_BUFFERS = 8;
   struct FilteredBuffer
   {
      uint64_t handle = 0;
      UINT size = 0;
      RegisteredConstants* entry = nullptr;
   };
   std::array<FilteredBuffer, MAX_FILTERED_BUFFERS> mv_filtered_buffers = {};
   uint32_t mv_filtered_buffer_count = 0;
   bool mv_filter_overflow = false;
   // Every pooled vc4 copy, and by size those nobody held anymore at the last present (taken by the next copies of that size)
   std::vector<std::shared_ptr<std::vector<uint8_t>>> mv_constants_pool;
   std::unordered_map<size_t, std::vector<uint32_t>> mv_constants_pool_free;
   size_t mv_constants_made = 0; // Copies asked for since the last present
   // Previous frame constants of the motion vector draws (see "PatchedDraws::PreviousConstants")
   PatchedDraws::PreviousConstants mv_previous_constants;
   // Motion vector draws by draw key (shaders, buffers, arguments), with their tie-break transform and vc4. A draw takes the previous
   // frame's vc4 of its key's nearest draw (same object, a frame earlier), its camera included.
   struct MotionVectorObject
   {
      PatchedDraws::ObjectTransform transform;
      ConstantsCopy constants;
   };
   std::unordered_map<uint64_t, std::vector<MotionVectorObject>, KeyHash> mv_objects;
   std::unordered_map<uint64_t, std::vector<MotionVectorObject>, KeyHash> mv_previous_objects;
   // The frame's camera (vc4 of its first scene depth writing view projection draw) and the previous frame's
   ConstantsCopy mv_camera;
   ConstantsCopy mv_previous_camera;
   // Current clip space to the previous frame's (column vectors), identity without a previous camera: the fill's, and the world view
   // projection of a draw without a previous one
   Math::Matrix44D mv_reprojection;
   uint32_t mv_frame_index = 0;              // The Luma frame index of the last motion vector frame
   std::vector<uint8_t> mv_camera_only_copy; // An unmatched draw's constants with last frame's camera (reused)

#if DEVELOPMENT
   // Per frame counts for the MCP (the last complete frame's)
   struct MotionVectorStats
   {
      uint32_t motion_vector_draws = 0, jitter_draws = 0, reactive_draws = 0, matched = 0, camera_only = 0, other_camera = 0, uncopied = 0, maps = 0, updates = 0, other_maps = 0, sr_draws = 0;
      uint32_t tiebreak_collisions = 0; // Objects sharing a key and a transform with other constants ("PatchedDraws::CountTieBreakCollisions")
      uint32_t untransformed = 0;       // Motion vector draws of vertex shaders without a tie-break transform (camera motion only)
      uint32_t bound_state_mismatches = 0;
      uint32_t constants_pool = 0;
      uint32_t prepass_drawn = 0;
      uint32_t ended_by = 0; // The ending pass's PS hash
      uint32_t rejected[REJECT_COUNT] = {};
   };
   MotionVectorStats mv_stats, mv_last_stats;
   MotionVectorReject mv_draw_reject = REJECT_COUNT; // The current draw's refusal ("REJECT_COUNT" for none), for the MCP trace note
#endif

   // "sr_rgb_blend_state" (the upscaler copy back and SMAA T2x's resolve: the canvas alpha stays), made at its first use
   ID3D11BlendState* GetRGBBlendState(ID3D11Device* native_device)
   {
      if (!sr_rgb_blend_state)
      {
         D3D11_BLEND_DESC blend_desc = CD3D11_BLEND_DESC(D3D11_DEFAULT);
         blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_RED | D3D11_COLOR_WRITE_ENABLE_GREEN | D3D11_COLOR_WRITE_ENABLE_BLUE;
         native_device->CreateBlendState(&blend_desc, &sr_rgb_blend_state);
      }
      return sr_rgb_blend_state.get();
   }

   // XeGTAO's scratch (~90 MB at 4K), recreated at its next use
   void ReleaseGTAOScratch()
   {
      gtao_depth_mips.reset();
      for (auto& uav : gtao_depth_mip_uavs)
      {
         uav.reset();
      }
      gtao_depth_mips_srv.reset();
      for (int i = 0; i < 2; i++)
      {
         gtao_working[i].reset();
         gtao_working_uavs[i].reset();
         gtao_working_srvs[i].reset();
      }
      gtao_output.reset();
      gtao_output_uav.reset();
      gtao_output_srv.reset();
      gtao_size = {};
   }

   void ReleaseT2xFrames()
   {
      for (int i = 0; i < 2; i++)
      {
         t2x_frames[i].reset();
         t2x_frame_rtvs[i].reset();
         t2x_frame_srvs[i].reset();
      }
   }

   // The motion vector target and the device depth the fill writes, recreated at their next use
   void ReleaseMotionVectorTargets()
   {
      mv_texture.reset();
      mv_rtv.reset();
      mv_srv.reset();
      mv_uav.reset();
      mv_device_depth.reset();
      mv_device_depth_uav.reset();
      ReleaseReactiveMasks();
   }

   // The reactive masks and what the draws wrote for them, recreated at their next use
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
};

class Bayonetta final : public Game
{
   static BayonettaGameDeviceData& GetGameDeviceData(DeviceData& device_data)
   {
      return *static_cast<BayonettaGameDeviceData*>(device_data.game);
   }

   // The weighted resolve reads the samples of the MSAA targets, which dgVoodoo creates without a shader resource bind (their mirrors
   // copy it). The bind flag only: the formats stay as created.
   static bool OnCreateResource(reshade::api::device* device, reshade::api::resource_desc& desc, reshade::api::subresource_data* initial_data, reshade::api::resource_usage initial_state)
   {
      if (desc.type != reshade::api::resource_type::texture_2d || desc.texture.samples <= 1 || (desc.usage & reshade::api::resource_usage::render_target) == 0 || (desc.usage & reshade::api::resource_usage::shader_resource) != 0)
         return false;
      if (desc.texture.format != reshade::api::format::r8g8b8a8_typeless && desc.texture.format != reshade::api::format::r8g8b8a8_unorm)
         return false;
      desc.usage |= reshade::api::resource_usage::shader_resource;
      return true;
   }

   // SMAA ULTRA on the canvas the scene copy (ResetRender) just wrote, display mapped, before the UI, predicated by the depth
   // prepass's edges ("Luma_Bayonetta_DepthEdges.hlsl"). TW1's "DrawSMAAOnScene" on the snapshot the UI clamp already takes
   // ("scene_texture"). "scene_rect" is the scene copy's viewport: the scene's 16:9 rect in the canvas with PreserveAspect.
   static void DrawSMAAOnCanvas(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, DeviceData& device_data, ID3D11RenderTargetView* canvas_rtv, const D3D11_VIEWPORT& scene_rect)
   {
      auto& gd = GetGameDeviceData(device_data);
      if (!HasSMAAShaders(device_data) || !gd.scene_srv)
         return;
      const uint2 size = GetViewTextureSize(gd.scene_srv.get());
      gd.smaa_resources_live = true;

      // Scale and mask fall back together: 2 with a null mask raises the threshold frame wide
      auto* const predication_shader = FindShader(device_data.native_compute_shaders, CompileTimeStringHash("Bayonetta Depth Edges CS"));
      bool predication = g_smaa_predication && predication_shader && gd.depth_srv && GetViewTextureSize(gd.depth_srv.get()) == uint2{uint32_t(scene_rect.Width), uint32_t(scene_rect.Height)};
      if (predication && (!gd.smaa_predication_srv || GetViewTextureSize(gd.smaa_predication_srv.get()) != size))
      {
         gd.smaa_predication_srv.reset();
         gd.smaa_predication_uav.reset();
         gd.smaa_predication.reset();
         const CD3D11_TEXTURE2D_DESC desc(DXGI_FORMAT_R16_FLOAT, size.x, size.y, 1, 1, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS);
         if (SUCCEEDED(native_device->CreateTexture2D(&desc, nullptr, gd.smaa_predication.put())) && SUCCEEDED(native_device->CreateUnorderedAccessView(gd.smaa_predication.get(), nullptr, gd.smaa_predication_uav.put())))
         {
            native_device->CreateShaderResourceView(gd.smaa_predication.get(), nullptr, gd.smaa_predication_srv.put());
         }
      }
      const float predication_constants[4] = {g_smaa_pred_tolerance, scene_rect.TopLeftX, scene_rect.TopLeftY, 0.f};
      predication = predication && gd.smaa_predication_srv && PatchedDraws::WriteDynamicConstants(native_device, native_device_context, std::addressof(gd.smaa_predication_buffer), predication_constants, sizeof(predication_constants));
      // SMAA T2x: SMAA into this frame's linear target with the velocity, then the resolve with the previous frame into the canvas
      // (Mirror's Edge's). It needs this frame's motion vectors from the fill, at the canvas' size: the motion vectors cover the scene
      // rect, so a canvas wider than the scene (PreserveAspect) stays 1x. Without them this frame is 1x and the next one doesn't jitter.
      auto* const copy_vs = FindShader(device_data.native_vertex_shaders, CompileTimeStringHash("Copy VS"));
      auto* const t2x_weight_ps = FindShader(device_data.native_pixel_shaders, CompileTimeStringHash("Bayonetta SMAA T2x Blending Weight Calculation PS"));
      auto* const t2x_blend_ps = FindShader(device_data.native_pixel_shaders, CompileTimeStringHash("Bayonetta SMAA T2x Neighborhood Blending PS"));
      auto* const t2x_resolve_ps = FindShader(device_data.native_pixel_shaders, CompileTimeStringHash("Bayonetta SMAA T2x Resolve PS"));
      const uint32_t frame_index = cb_luma_global_settings.FrameIndex;
      bool t2x = gd.t2x_active && gd.mv_filled && gd.mv_srv && copy_vs && t2x_weight_ps && t2x_blend_ps && t2x_resolve_ps && GetViewTextureSize(gd.mv_srv.get()) == size;
      const int t2x_current = int(frame_index & 1);
      bool t2x_history = gd.t2x_frames[0] && gd.t2x_frame + 1 == frame_index;
      if (t2x && GetViewTextureSize(gd.t2x_frame_srvs[0].get()) != size)
      {
         t2x_history = false;
         gd.ReleaseT2xFrames();
         for (int i = 0; i < 2; i++)
         {
            const CD3D11_TEXTURE2D_DESC desc(DXGI_FORMAT_R16G16B16A16_FLOAT, size.x, size.y, 1, 1, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET);
            if (FAILED(native_device->CreateTexture2D(&desc, nullptr, gd.t2x_frames[i].put())) || FAILED(native_device->CreateRenderTargetView(gd.t2x_frames[i].get(), nullptr, gd.t2x_frame_rtvs[i].put())) ||
                FAILED(native_device->CreateShaderResourceView(gd.t2x_frames[i].get(), nullptr, gd.t2x_frame_srvs[i].put())))
            {
               gd.ReleaseT2xFrames();
               break;
            }
         }
         t2x = gd.t2x_frames[0].get() != nullptr;
      }
      // b1 ("Luma_SMAA_impl.hlsl"'s "SmaaMetricsCB"): the metrics, the predication threshold scale and T2x's subsample indices for the
      // jitter phase (see "OpenScene"), 0 for 1x
      const float subsample_indices = ((t2x && gd.t2x_phase >= 0) ? float(gd.t2x_phase + 1) : 0.f);
      const float metrics[12] = {1.f / float(size.x), 1.f / float(size.y), float(size.x), float(size.y), predication ? 2.f : 1.f, 0.f, 0.f, 0.f, subsample_indices, subsample_indices, subsample_indices, 0.f};
      if (!PatchedDraws::WriteDynamicConstants(native_device, native_device_context, std::addressof(gd.smaa_metrics_buffer), metrics, sizeof(metrics)))
         return;

      if (predication)
      {
         ID3D11ShaderResourceView* const srv = gd.depth_srv.get();
         ID3D11UnorderedAccessView* const uav = gd.smaa_predication_uav.get();
         ID3D11Buffer* const buffer = gd.smaa_predication_buffer.get();
         native_device_context->CSSetShaderResources(0, 1, &srv);
         native_device_context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);
         native_device_context->CSSetConstantBuffers(0, 1, &buffer);
         native_device_context->CSSetShader(predication_shader, nullptr, 0);
         native_device_context->Dispatch((size.x + 7) / 8, (size.y + 7) / 8, 1);
         ID3D11ShaderResourceView* const null_srv = nullptr;
         ID3D11UnorderedAccessView* const null_uav = nullptr;
         native_device_context->CSSetShaderResources(0, 1, &null_srv);
         native_device_context->CSSetUnorderedAccessViews(0, 1, &null_uav, nullptr);
      }
#if DEVELOPMENT
      // Calibration aid: predication's effect is the absence of smearing, which the eye misjudges; judge the mask instead (red)
      if (predication && g_smaa_pred_debug)
      {
         auto* const copy_ps = FindShader(device_data.native_pixel_shaders, CompileTimeStringHash("Copy PS"));
         if (copy_vs && copy_ps)
         {
            DrawCustomPixelShader(native_device_context, device_data.default_depth_stencil_state.get(), device_data.default_blend_state.get(), nullptr, copy_vs, copy_ps, gd.smaa_predication_srv.get(), canvas_rtv, size.x, size.y, false);
            return;
         }
      }
#endif

      // Writing the canvas is safe: the chain samples the snapshot, never the canvas itself
      if (!t2x)
      {
         DrawSMAA(native_device, native_device_context, device_data, canvas_rtv, gd.scene_srv.get(), gd.scene_srv.get(), predication ? gd.smaa_predication_srv.get() : nullptr, gd.smaa_metrics_buffer.get());
      }
      else
      {
         // The game's state back after the resolve (dgVoodoo doesn't rebind what it thinks is bound)
         DrawStateStack<DrawStateStackType::FullGraphics> graphics_state;
         graphics_state.Cache(native_device_context, device_data.uav_max_count);
         // The metrics at VS+PS b1, bound here rather than through "DrawSMAA()" (which restores the slot): the resolve reads them too
         ID3D11Buffer* const metrics_buffer = gd.smaa_metrics_buffer.get();
         native_device_context->VSSetConstantBuffers(1, 1, &metrics_buffer);
         native_device_context->PSSetConstantBuffers(1, 1, &metrics_buffer);
         const SMAAT2xPasses t2x_passes = {.blending_weight_calculation_ps = t2x_weight_ps, .neighborhood_blending_ps = t2x_blend_ps, .velocity = gd.mv_srv.get()};
         DrawSMAA(native_device, native_device_context, device_data, gd.t2x_frame_rtvs[t2x_current].get(), gd.scene_srv.get(), gd.scene_srv.get(), predication ? gd.smaa_predication_srv.get() : nullptr, nullptr, &t2x_passes);
         // The resolve: t0 this frame, t1 the previous one or this one again without a history, t2 the motion vectors, s0 linear and
         // s1 point. RGB only: the canvas alpha stays the scene copy's (T2x's alpha is the velocity length).
         native_device_context->OMSetRenderTargets(0, nullptr, nullptr);
         ID3D11ShaderResourceView* const resolve_srvs[2] = {gd.t2x_frame_srvs[t2x_history ? (t2x_current ^ 1) : t2x_current].get(), gd.mv_srv.get()};
         native_device_context->PSSetShaderResources(1, 2, resolve_srvs);
         ID3D11SamplerState* const resolve_samplers[2] = {device_data.sampler_state_linear.get(), device_data.sampler_state_point.get()};
         native_device_context->PSSetSamplers(0, 2, resolve_samplers);
         DrawCustomPixelShader(native_device_context, device_data.default_depth_stencil_state.get(), gd.GetRGBBlendState(native_device), nullptr, copy_vs, t2x_resolve_ps, gd.t2x_frame_srvs[t2x_current].get(), canvas_rtv, size.x, size.y, false);
         graphics_state.Restore(native_device_context);
         gd.t2x_frame = frame_index;
      }
#if DEVELOPMENT
      gd.aa_stats.smaa++;
      gd.aa_stats.smaa_predicated += (predication ? 1 : 0);
#endif
   }

   // ---- DLAA / FSR 3 Native AA / Luma TAA (vc4 layout in "MotionVectorPatches") ----
   // The camera rows of a vertex shader (column vector matrices, one row per clip component, c8 for the materials) or the world view
   // projection of the effects (c24), a matrix's 4 rows
   static constexpr UINT VIEW_PROJECTION_OFFSET = MotionVectorPatches::view_projection_row * 16;
   static constexpr UINT WORLD_VIEW_PROJECTION_OFFSET = MotionVectorPatches::world_view_projection_row * 16;
   static constexpr UINT CAMERA_SIZE = 4 * 16;
   static_assert(sizeof(PatchedDraws::ObjectTransform) == 3 * 16);

   // The upscalers that read the reactive mask the alpha blended draws write: FSR and Luma TAA (DLSS's current presets ignore it,
   // DLSS-Best-Practices TRN-2)
   static bool IsReactiveMaskUsed(const DeviceData& device_data)
   {
      return g_sr_reactive_enable && static_cast<const BayonettaGameDeviceData*>(device_data.game)->sr_active && (device_data.sr_type == SR::Type::FSR || device_data.sr_type == SR::Type::LumaTAA);
   }

   // The registered vc4 buffer's entry in "mv_constants_copies", null if the buffer isn't one; "size" its size if known (else 0)
   static BayonettaGameDeviceData::RegisteredConstants* FindRegisteredBuffer(BayonettaGameDeviceData* gd, uint64_t handle, UINT* size)
   {
      *size = 0;
      if (!gd->mv_filter_overflow)
      {
         for (uint32_t i = 0; i < gd->mv_filtered_buffer_count; i++)
         {
            if (const auto& [filtered_handle, filtered_size, entry] = gd->mv_filtered_buffers[i]; filtered_handle == handle)
            {
               *size = filtered_size;
               return entry;
            }
         }
         return nullptr;
      }
      const auto registered = gd->mv_constants_copies.find(handle);
      return (registered != gd->mv_constants_copies.end() ? &registered->second : nullptr);
   }

   // A buffer registered in "mv_constants_copies" joins the filter while it has room
   static void AddFilteredBuffer(BayonettaGameDeviceData* gd, ID3D11Buffer* buffer, BayonettaGameDeviceData::RegisteredConstants* entry)
   {
      const uint32_t count = gd->mv_filtered_buffer_count;
      if (count >= BayonettaGameDeviceData::MAX_FILTERED_BUFFERS)
      {
         gd->mv_filter_overflow = true;
         return;
      }
      D3D11_BUFFER_DESC desc;
      buffer->GetDesc(&desc);
      gd->mv_filtered_buffers[count] = {.handle = reinterpret_cast<uint64_t>(buffer), .size = desc.ByteWidth, .entry = entry};
      gd->mv_filtered_buffer_count = count + 1;
   }

   static UINT GetBufferSize(uint64_t handle, UINT known_size)
   {
      if (known_size != 0)
         return known_size;
      D3D11_BUFFER_DESC desc;
      reinterpret_cast<ID3D11Buffer*>(handle)->GetDesc(&desc);
      return desc.ByteWidth;
   }

   // A vc4 copy of "size" bytes from "bytes": one nobody held anymore at the last present, else a new one
   static std::shared_ptr<std::vector<uint8_t>> NewConstantsCopy(BayonettaGameDeviceData* gd, const uint8_t* bytes, size_t size)
   {
      gd->mv_constants_made++;
      if (const auto free_copies = gd->mv_constants_pool_free.find(size); free_copies != gd->mv_constants_pool_free.end() && !free_copies->second.empty())
      {
         auto copy = gd->mv_constants_pool[free_copies->second.back()];
         free_copies->second.pop_back();
         std::memcpy(copy->data(), bytes, size);
         return copy;
      }
      auto copy = std::make_shared<std::vector<uint8_t>>(bytes, bytes + size);
      gd->mv_constants_pool.push_back(copy);
      return copy;
   }

   // Motion vectors: a registered vc4 buffer mapped for a whole rewrite, remembered until its Unmap
   static void OnMapBufferRegion(reshade::api::device* device, reshade::api::resource resource, uint64_t offset, uint64_t size, reshade::api::map_access access, void** data)
   {
      DeviceData* const device_data = device->get_private_data<DeviceData>();
      if (!device_data || !device_data->game || !GetGameDeviceData(*device_data).mv_active || !data || !*data)
         return;
      auto& gd = GetGameDeviceData(*device_data);
      UINT buffer_size;
      BayonettaGameDeviceData::RegisteredConstants* const registered = FindRegisteredBuffer(&gd, resource.handle, &buffer_size);
      if (!registered)
         return;
      // Overwritten at every Map, so a pointer from before a pause of the hooks never reaches an Unmap
      registered->mapped = (access == reshade::api::map_access::write_discard && offset == 0) ? *data : nullptr;
#if DEVELOPMENT
      if (!registered->mapped)
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
      UINT buffer_size;
      BayonettaGameDeviceData::RegisteredConstants* const registered = FindRegisteredBuffer(&gd, resource.handle, &buffer_size);
      if (!registered || !registered->mapped)
         return;
      const uint8_t* const mapped = static_cast<const uint8_t*>(std::exchange(registered->mapped, nullptr));
      registered->latest.assign(mapped, mapped + GetBufferSize(resource.handle, buffer_size));
      registered->copy.reset();
      registered->generation = gd.mv_constants_generation;
#if DEVELOPMENT
      gd.mv_stats.maps++;
#endif
   }

   // Motion vectors: the CPU copy of a vc4 buffer from an UpdateSubresource (before it runs); a partial update is merged into the
   // last copy. Needs the unsigned "full add-on support" ReShade (the signed build doesn't raise the event).
   static bool OnUpdateBufferRegion(reshade::api::device* device, const void* data, reshade::api::resource resource, uint64_t offset, uint64_t size)
   {
      DeviceData* const device_data = device->get_private_data<DeviceData>();
      if (!device_data || !device_data->game || !data || !GetGameDeviceData(*device_data).mv_active)
         return false;
      auto& gd = GetGameDeviceData(*device_data);
      UINT known_size;
      BayonettaGameDeviceData::RegisteredConstants* const registered = FindRegisteredBuffer(&gd, resource.handle, &known_size);
      if (!registered)
         return false;
      const UINT buffer_size = GetBufferSize(resource.handle, known_size);
      if (offset >= buffer_size)
         return false;
      const size_t updated_size = size_t((std::min)(size, uint64_t(buffer_size) - offset));
      // Merged into the last upload of the same size, else into zeros
      std::vector<uint8_t>& latest = registered->latest;
      const uint32_t generation = gd.mv_constants_generation;
      if (latest.size() != buffer_size || registered->generation != generation)
      {
         latest.assign(buffer_size, 0);
      }
      std::memcpy(latest.data() + offset, data, updated_size);
      registered->copy.reset();
      registered->generation = generation;
#if DEVELOPMENT
      gd.mv_stats.updates++;
#endif
      return false;
   }

   // "bound_blend_state" and "bound_depth_stencil_state", from the game's binds on the immediate context (ReShade raises one per state,
   // and a reset binds none to every stage)
   static void OnBindPipeline(reshade::api::command_list* cmd_list, reshade::api::pipeline_stage stages, reshade::api::pipeline pipeline)
   {
      using reshade::api::pipeline_stage;
      if ((stages & (pipeline_stage::output_merger | pipeline_stage::depth_stencil)) == 0)
         return;
      DeviceData* const device_data = cmd_list->get_device()->get_private_data<DeviceData>();
      const CommandListData* const cmd_list_data = cmd_list->get_private_data<CommandListData>();
      if (!device_data || !device_data->game || !cmd_list_data || !cmd_list_data->is_primary)
         return;
      auto& gd = GetGameDeviceData(*device_data);
      if (stages == pipeline_stage::output_merger)
      {
         gd.bound_blend_state = reinterpret_cast<ID3D11BlendState*>(pipeline.handle);
      }
      else if (stages == pipeline_stage::depth_stencil)
      {
         gd.bound_depth_stencil_state = reinterpret_cast<ID3D11DepthStencilState*>(pipeline.handle);
      }
      else if (pipeline.handle == 0)
      {
         gd.bound_blend_state = nullptr;
         gd.bound_depth_stencil_state = nullptr;
      }
      else
      {
         gd.bound_states_tracked = false;
      }
   }

   // vc4's tracked binding (see "bound_object_buffer"); a game bind over the jitter slot (none seen) has it bound again
   static void OnPushConstantBuffers(reshade::api::command_list* cmd_list, reshade::api::shader_stage stages, reshade::api::pipeline_layout layout, uint32_t layout_param, const reshade::api::descriptor_table_update& update)
   {
      constexpr uint32_t slot = MotionVectorPatches::object_slot;
      const auto covers = [&](uint32_t binding)
      { return binding >= update.binding && binding < update.binding + update.count; };
      if ((stages & reshade::api::shader_stage::vertex) == 0 || update.type != reshade::api::descriptor_type::constant_buffer || (!covers(slot) && !covers(MotionVectorPatches::jitter_slot)))
         return;
      DeviceData* const device_data = cmd_list->get_device()->get_private_data<DeviceData>();
      const CommandListData* const cmd_list_data = cmd_list->get_private_data<CommandListData>();
      if (!device_data || !device_data->game || !cmd_list_data || !cmd_list_data->is_primary)
         return;
      auto& gd = GetGameDeviceData(*device_data);
      if (covers(MotionVectorPatches::jitter_slot))
      {
         gd.mv_jitter_bound = false;
      }
      if (covers(slot))
      {
         const auto* const ranges = static_cast<const reshade::api::buffer_range*>(update.descriptors);
         gd.bound_object_buffer = reinterpret_cast<ID3D11Buffer*>(ranges[slot - update.binding].buffer.handle);
      }
   }

   // Vertex buffer 0's tracked binding (see "bound_vertex_buffer")
   static void OnBindVertexBuffers(reshade::api::command_list* cmd_list, uint32_t first, uint32_t count, const reshade::api::resource* buffers, const uint64_t* offsets, const uint32_t* strides)
   {
      if (first != 0 || count == 0)
         return;
      DeviceData* const device_data = cmd_list->get_device()->get_private_data<DeviceData>();
      const CommandListData* const cmd_list_data = cmd_list->get_private_data<CommandListData>();
      if (!device_data || !device_data->game || !cmd_list_data || !cmd_list_data->is_primary)
         return;
      auto& gd = GetGameDeviceData(*device_data);
      gd.bound_vertex_buffer = reinterpret_cast<ID3D11Buffer*>(buffers[0].handle);
      gd.bound_vertex_offset = UINT(offsets[0]);
   }

   // The index buffer's tracked binding (see "bound_index_buffer")
   static void OnBindIndexBuffer(reshade::api::command_list* cmd_list, reshade::api::resource buffer, uint64_t offset, uint32_t index_size)
   {
      DeviceData* const device_data = cmd_list->get_device()->get_private_data<DeviceData>();
      const CommandListData* const cmd_list_data = cmd_list->get_private_data<CommandListData>();
      if (!device_data || !device_data->game || !cmd_list_data || !cmd_list_data->is_primary)
         return;
      auto& gd = GetGameDeviceData(*device_data);
      gd.bound_index_buffer = reinterpret_cast<ID3D11Buffer*>(buffer.handle);
      gd.bound_index_offset = UINT(offset);
   }

   // A bound object of the immediate context (not referenced: it lives while bound): "tracked" (see "bound_blend_state"), else what
   // "query" writes to its argument (an object pointer to take over). "g_bound_state_check" counts the times they differ.
   template <typename T, typename Query>
   static T* BoundState(BayonettaGameDeviceData* gd, T* tracked, const Query& query)
   {
      if (gd->bound_states_tracked && !g_bound_state_check)
         return tracked;
      com_ptr<T> queried;
      query(&queried);
#if DEVELOPMENT
      if (g_bound_state_check && gd->bound_states_tracked && queried.get() != tracked)
      {
         gd->mv_stats.bound_state_mismatches++;
      }
#endif
      return queried.get();
   }

   // A game pixel shader's "patch", from the bytecode its shader object was created with; empty with "error" if it can't be patched. Any
   // thread ("AddShaderWarmupVariants" too).
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
      case PixelShaderPatch::COVERAGE:
         // The alpha tested materials clip at 0.5 (42 shaders in the 2.87.5 dump); the clips at 0.001 (blended materials' invisible
         // texels) and the depth prepass's at 0.05 are below the threshold and stay as they are
         return CoveragePatch::PatchPixelShader(code, code_size, error, 0.1f);
      }
      return {};
   }

   // The bound shader's patched version ("patch" for pixel shaders, see above), patched from Core's bytecode copy on first use (null if
   // it can't be, e.g. a vertex shader that doesn't place vertices with the view projection). The maps' nodes are never erased.
   template <typename T>
   static const BayonettaGameDeviceData::PatchedShader<T>& GetPatchedShader(ID3D11Device* native_device, DeviceData& device_data, uint32_t hash, reshade::api::pipeline pipeline,
      PixelShaderPatch patch = PixelShaderPatch::MOTION_VECTORS)
   {
      constexpr bool vertex = std::is_same_v<T, ID3D11VertexShader>;
      auto& gd = GetGameDeviceData(device_data);
      auto* const shaders = [&]
      {
         if constexpr (vertex)
            return &gd.mv_vertex_shaders;
         else
            return &gd.patched_pixel_shaders[size_t(patch)];
      }();
      {
         const std::shared_lock lock(gd.mv_mutex);
         if (const auto it = shaders->find(hash); it != shaders->end())
            return it->second;
      }
#if DEVELOPMENT
      const auto create_start = std::chrono::steady_clock::now();
#endif
      std::vector<uint8_t> patched;
      std::string error; // Written by a refusal only
      UINT read_size = 0;
      UINT camera_offset = 0;
      UINT translation_offset = 0;
      {
         const std::shared_lock lock(s_mutex_generic);
         if (const auto it = device_data.pipeline_cache_by_pipeline_handle.find(pipeline.handle); it != device_data.pipeline_cache_by_pipeline_handle.end() && it->second->subobjects_cache)
         {
            const auto* desc = static_cast<const reshade::api::shader_desc*>(it->second->subobjects_cache[0].data);
            // On top of the shader as created ("PatchShaderBytecodeSync"'s clamps; the pipeline cache keeps the original bytecode)
            const auto sync_patched = device_data.patch_context.GetShaderData(hash);
            const auto* code = (sync_patched ? sync_patched->code.data() : static_cast<const uint8_t*>(desc->code));
            const size_t code_size = (sync_patched ? sync_patched->code.size() : desc->code_size);
            if constexpr (vertex)
            {
               patched = MotionVectorPatch::PatchVertexShader(code, code_size, MotionVectorPatches::layout, &error);
               if (!patched.empty())
               {
                  constexpr uint32_t slot = MotionVectorPatches::object_slot;
                  read_size = DXBC::ConstantBufferBytes(code, code_size, slot);
                  // No vertex shader reads both cameras (census of the dump). The tie-break transform: the first bone (world space,
                  // indexed relatively), else the world matrix of the materials or of the effects that have one.
                  if (DXBC::ReadsConstantRow(code, code_size, slot, MotionVectorPatches::view_projection_row))
                  {
                     camera_offset = VIEW_PROJECTION_OFFSET;
                  }
                  else if (DXBC::ReadsConstantRow(code, code_size, slot, MotionVectorPatches::world_view_projection_row))
                  {
                     camera_offset = WORLD_VIEW_PROJECTION_OFFSET;
                  }
                  if (DXBC::ReadsConstantRow(code, code_size, slot, MotionVectorPatches::first_bone_row, /* relative_base */ true))
                  {
                     translation_offset = MotionVectorPatches::first_bone_row * 16;
                  }
                  else if (DXBC::ReadsConstantRow(code, code_size, slot, MotionVectorPatches::world_row))
                  {
                     translation_offset = MotionVectorPatches::world_row * 16;
                  }
                  else if (DXBC::ReadsConstantRow(code, code_size, slot, MotionVectorPatches::effect_world_row))
                  {
                     translation_offset = MotionVectorPatches::effect_world_row * 16;
                  }
               }
            }
            else
            {
               patched = PatchPixelShader(patch, code, code_size, &error);
            }
         }
         else
         {
            error = "no bytecode";
         }
      }
      com_ptr<T> shader;
      if (!patched.empty())
      {
         const HRESULT hr = [&]
         {
            if constexpr (vertex)
               return native_device->CreateVertexShader(patched.data(), patched.size(), nullptr, &shader);
            else
               return native_device->CreatePixelShader(patched.data(), patched.size(), nullptr, &shader);
         }();
         if (FAILED(hr))
         {
            error = std::format("create 0x{:08X}", uint32_t(hr));
         }
#if DEVELOPMENT
         else
         {
            Perf::g_shader_creates.AddLuma(std::chrono::steady_clock::now() - create_start);
         }
#endif
      }
      // Failures in every build (bug reports), except the expected refusals (screen space; every shader without a clip offered to the
      // coverage patch, logged in development only when patched); every patched shader only in development
      const bool expected = error.starts_with("screen space") || error.starts_with("no clip");
      if ((DEVELOPMENT && (shader || patch != PixelShaderPatch::COVERAGE)) || (!shader && !expected))
      {
         // A patched shader's note (the coverage's "inward only") follows "patched"
         std::string status = error;
         if (shader)
         {
            status = (error.empty() ? std::string("patched") : "patched, " + error);
         }
         reshade::log::message(((shader || expected) ? reshade::log::level::info : reshade::log::level::warning),
            std::format("[Bayonetta] {} 0x{:08X} {}", (vertex ? "MV VS" : PIXEL_SHADER_PATCH_NAMES[size_t(patch)]), hash, status).c_str());
      }
      const std::unique_lock lock(gd.mv_mutex);
      const auto [entry, inserted] = shaders->try_emplace(hash, BayonettaGameDeviceData::PatchedShader<T>{.shader = shader, .read_size = read_size, .camera_offset = camera_offset, .translation_offset = translation_offset});
      return entry->second;
   }

   // The bound vertex shader's patched version (null shader if refused), looked up again only when the game's shader changes
   static const BayonettaGameDeviceData::PatchedShader<ID3D11VertexShader>& GetPatchedVertexShader(ID3D11Device* native_device, const CommandListData& cmd_list_data, DeviceData& device_data, uint32_t hash)
   {
      auto& gd = GetGameDeviceData(device_data);
      if (hash != gd.mv_last_vertex_shader_hash || !gd.mv_last_vertex_shader)
      {
         gd.mv_last_vertex_shader = &GetPatchedShader<ID3D11VertexShader>(native_device, device_data, hash, cmd_list_data.pipeline_state_original_vertex_shader);
         gd.mv_last_vertex_shader_hash = hash;
      }
      return *gd.mv_last_vertex_shader;
   }

   // Classifies the bound blend state (cached in "mv_blend_state"): "mv_blend_opaque" for the motion vectors (additive lights, decals
   // and translucents keep the motion vectors of what's behind them, and so do colorless draws: the occlusion query bounding boxes), and
   // "mv_reactive_blend": NONE not alpha blended (opaque, lights ONE/ONE, shadows DEST_COLOR), ALPHA (SRC_ALPHA / INV_SRC_ALPHA:
   // smoke, glass, water; reactive and transparency & composition), ADDITIVE (SRC_ALPHA / ONE: sparks, glows; reactive)
   static void ClassifyBoundBlend(ID3D11DeviceContext* native_device_context, BayonettaGameDeviceData* gd)
   {
      ID3D11BlendState* const blend_state = BoundState(gd, gd->bound_blend_state, [&](ID3D11BlendState** state)
         { native_device_context->OMGetBlendState(state, nullptr, nullptr); });
      if (blend_state == gd->mv_blend_state)
         return;
      D3D11_BLEND_DESC blend_desc = CD3D11_BLEND_DESC(D3D11_DEFAULT);
      if (blend_state)
      {
         blend_state->GetDesc(&blend_desc);
      }
      const D3D11_RENDER_TARGET_BLEND_DESC& rt0 = blend_desc.RenderTarget[0];
      gd->mv_blend_opaque = rt0.RenderTargetWriteMask != 0 && (!rt0.BlendEnable || (rt0.SrcBlend == D3D11_BLEND_ONE && rt0.DestBlend == D3D11_BLEND_ZERO && rt0.BlendOp == D3D11_BLEND_OP_ADD));
      gd->mv_reactive_blend = ((!rt0.BlendEnable || rt0.SrcBlend != D3D11_BLEND_SRC_ALPHA) ? ReactiveBlend::NONE : (rt0.DestBlend == D3D11_BLEND_ONE ? ReactiveBlend::ADDITIVE : ReactiveBlend::ALPHA));
      // Exactly the states whose motion vector target "PatchedDraws::OnCreateBlendState" blends ("blend_alpha_blended_target")
      gd->mv_alpha_blended = rt0.BlendEnable && rt0.SrcBlend == D3D11_BLEND_SRC_ALPHA && rt0.DestBlend == D3D11_BLEND_INV_SRC_ALPHA && rt0.BlendOp == D3D11_BLEND_OP_ADD;
      gd->mv_blend_state = blend_state;
   }

   // The bound depth stencil state tests depth (meshes do; full screen passes and composites don't), cached by state
   static bool IsDepthTested(ID3D11DeviceContext* native_device_context, BayonettaGameDeviceData* gd)
   {
      ID3D11DepthStencilState* const depth_stencil_state = BoundState(gd, gd->bound_depth_stencil_state, [&](ID3D11DepthStencilState** state)
         { native_device_context->OMGetDepthStencilState(state, nullptr); });
      if (depth_stencil_state != gd->jitter_depth_stencil_state)
      {
         D3D11_DEPTH_STENCIL_DESC depth_desc = CD3D11_DEPTH_STENCIL_DESC(D3D11_DEFAULT);
         if (depth_stencil_state)
         {
            depth_stencil_state->GetDesc(&depth_desc);
         }
         gd->jitter_depth_test = depth_desc.DepthEnable;
         gd->jitter_depth_stencil_state = depth_stencil_state;
      }
      return gd->jitter_depth_test;
   }

   // Opens the scene at the frame's first mesh draw into its targets: a single sample fp16 color or the depth prepass' R32F (with the
   // normals in the second target), of the bound depth's size, which is the scene's (a full viewport of it). The scene is the output's
   // size or smaller (the 16:9 rect of the canvas with PreserveAspect): the shadow maps and the other offscreen passes aren't. Takes the
   // scene depth, rolls the last frame's camera and objects over and picks the jitter the whole scene draws with. Once per present (the
   // UI and later passes never reopen it).
   static void OpenScene(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, const CommandListData& cmd_list_data, DeviceData& device_data, uint32_t vertex_shader_hash,
      ID3D11RenderTargetView* rtv, bool has_second_target, ID3D11DepthStencilView* dsv)
   {
      auto& gd = GetGameDeviceData(device_data);
      const auto refuse = [&]
      {
         gd.open_refused_rtv = rtv;
         gd.open_refused_dsv = dsv;
      };
      uint4 depth_size;
      DXGI_FORMAT unused_format;
      GetResourceInfo(dsv, depth_size, unused_format);
      const uint2 output_size = {uint32_t(device_data.output_resolution.x), uint32_t(device_data.output_resolution.y)};
      if (depth_size.x > output_size.x || depth_size.y > output_size.y || depth_size.x * 2 < output_size.x || depth_size.y * 2 < output_size.y)
      {
         refuse();
         return;
      }
      D3D11_RENDER_TARGET_VIEW_DESC rtv_desc;
      rtv->GetDesc(&rtv_desc);
      com_ptr<ID3D11Resource> color;
      rtv->GetResource(&color);
      com_ptr<ID3D11Texture2D> color_texture;
      D3D11_TEXTURE2D_DESC color_desc = {};
      if (SUCCEEDED(color->QueryInterface(&color_texture)))
      {
         color_texture->GetDesc(&color_desc);
      }
      // Only the depth prepass (R32F 1 - z/w next to the normals) opens the scene: OtMirror draws its reflection earlier into another
      // full size fp16 target, with no prepass (and a view projection whose determinant has the opposite sign of the scene's). The scene color is the first fp16 target drawn with this depth ("DrawWithMotionVectors").
      const bool scene_format = rtv_desc.Format == DXGI_FORMAT_R32_FLOAT && has_second_target;
      if (!scene_format || color_desc.SampleDesc.Count != 1 || color_desc.Width != depth_size.x || color_desc.Height != depth_size.y)
      {
         refuse();
         return;
      }
      UINT viewport_count = 1;
      D3D11_VIEWPORT viewport = {};
      native_device_context->RSGetViewports(&viewport_count, &viewport);
      if (viewport_count == 0 || viewport.Width != float(depth_size.x) || viewport.Height != float(depth_size.y) || !GetPatchedVertexShader(native_device, cmd_list_data, device_data, vertex_shader_hash).shader)
         return;
      gd.mv_scene_open = true;
      const uint2 scene_size = {depth_size.x, depth_size.y};
      const bool same_size = gd.mv_scene_size == scene_size;
      gd.mv_scene_size = scene_size;
      gd.mv_depth.reset();
      dsv->GetResource(&gd.mv_depth);
      gd.mv_scene_color.reset();
      gd.mv_scene_rtv.reset();
      gd.mv_scene_last_copy.reset();
      gd.jitter_dsv = nullptr;
      gd.jitter_depth_stencil_state = nullptr;
      gd.jitter_depth_test = true;
      gd.mv_accepted_dsv = nullptr;
      gd.mv_refused_rtv = nullptr;
      gd.mv_refused_dsv = nullptr;
      gd.mv_jitter_bound = false;
      gd.mv_blend_state = nullptr;
      gd.mv_blend_opaque = true;
      gd.mv_alpha_blended = false;
      gd.mv_reactive_blend = ReactiveBlend::NONE;
      gd.prepass_rtv = nullptr;
      gd.prepass_drawn = false;
      // Last frame's camera and objects are the previous ones, unless frames without a scene (menus, videos) came between
      const bool previous_valid = gd.mv_camera && same_size && cb_luma_global_settings.FrameIndex - gd.mv_frame_index <= 1;
      gd.mv_previous_camera = (previous_valid ? gd.mv_camera : nullptr);
      gd.mv_camera = nullptr;
      gd.mv_frame_index = cb_luma_global_settings.FrameIndex;
      // Swapped, not rebuilt: the lists keep their nodes and capacity (an empty list matches nothing); keys drawn in neither of the
      // last two frames go
      gd.mv_previous_objects.swap(gd.mv_objects);
#if DEVELOPMENT
      // Objects without a transform match by their draw key alone: any twin has the same vertices
      gd.mv_stats.tiebreak_collisions = PatchedDraws::CountTieBreakCollisions(gd.mv_previous_objects, [](const auto& a, const auto& b)
         { return a.transform == PatchedDraws::ObjectTransform{} || PatchedDraws::SameBytes(a.constants, b.constants); });
#endif
      std::erase_if(gd.mv_objects, [](const auto& entry)
         { return entry.second.empty(); });
      for (auto& [key, objects] : gd.mv_objects)
      {
         objects.clear();
      }
      if (!previous_valid)
      {
         gd.mv_previous_objects.clear();
      }
      // Halton (2, 3) over the upscaler's phase count; pixels to NDC (y up). None until the upscaler is ready (the bridge's helper
      // starting shows the scene as it is, antialiased with SMAA).
      const SR::InstanceData* sr_instance_data = (gd.sr_active ? device_data.GetSRInstanceData() : nullptr);
      if (sr_instance_data && !sr_implementations[device_data.sr_type]->IsReady(sr_instance_data))
      {
         sr_instance_data = nullptr;
      }
      gd.mv_jitter = (((sr_instance_data && gd.mv_jitter_allowed) || g_mv_force_jitter) ? GetSRJitter(device_data, sr_instance_data) : std::array<float, 2>{});
      // SMAA T2x: the sample offsets (0.25, 0.25) then (-0.25, -0.25) in y down pixels, for "SMAA.hlsl"'s T2x subsample indices 1 and
      // 2. That is its table read with y up, which measured better (docs/SMAA-Lab.md). The projection moves the other way.
      gd.t2x_phase = ((gd.t2x_active && gd.t2x_frames[0] && gd.t2x_frame + 1 == cb_luma_global_settings.FrameIndex) ? int(cb_luma_global_settings.FrameIndex & 1) : -1);
      if (gd.t2x_phase >= 0)
      {
         const float shift = (gd.t2x_phase == 0 ? -0.25f : 0.25f);
         gd.mv_jitter = {shift, shift};
      }
      gd.mv_jitter_ndc = {gd.mv_jitter[0] * 2.f / float(scene_size.x), gd.mv_jitter[1] * -2.f / float(scene_size.y)};
      const float ndc_jitter[4] = {gd.mv_jitter_ndc[0], gd.mv_jitter_ndc[1], 0.f, 0.f};
      if (!PatchedDraws::WriteDynamicConstants(native_device, native_device_context, std::addressof(gd.mv_jitter_buffer), ndc_jitter, sizeof(ndc_jitter)))
      {
         // No stale jitter on the scene draws either: no motion vectors this frame
         gd.mv_jitter = {};
         gd.mv_jitter_ndc = {};
         gd.mv_jitter_buffer.reset();
         gd.t2x_phase = -1;
      }
   }

#if DEVELOPMENT
#define MV_REJECT(reason) \
   ([&](auto& gd) { gd.mv_stats.rejected[reason]++; gd.mv_draw_reject = reason; return false; }(GetGameDeviceData(device_data)))
#else
#define MV_REJECT(reason) false
#endif

   // The jitter buffer, bound once per scene (no translated shader reads b9, Luma draws nothing during the scene; a game bind over it
   // resets "mv_jitter_bound", see "OnPushConstantBuffers")
   static void BindJitter(ID3D11DeviceContext* native_device_context, BayonettaGameDeviceData* gd)
   {
      if (std::exchange(gd->mv_jitter_bound, true))
         return;
      ID3D11Buffer* const jitter = gd->mv_jitter_buffer.get();
      native_device_context->VSSetConstantBuffers(MotionVectorPatches::jitter_slot, 1, &jitter);
   }

   // What the next frame keeps of a draw's vc4: only the bytes its vertex shader reads (at least its camera and tie-break transform),
   // copied from the buffer's latest upload. A draw reading all of it (skinned) shares the entry's one copy. Null if the upload doesn't
   // hold the camera yet.
   static BayonettaGameDeviceData::ConstantsCopy SnapshotConstants(BayonettaGameDeviceData* gd, BayonettaGameDeviceData::RegisteredConstants* entry, const BayonettaGameDeviceData::PatchedShader<ID3D11VertexShader>& vertex_shader)
   {
      const size_t camera_end = vertex_shader.camera_offset + CAMERA_SIZE;
      if (entry->latest.size() < camera_end)
         return nullptr;
      const size_t min_size = (std::max)(camera_end, vertex_shader.translation_offset != 0 ? vertex_shader.translation_offset + sizeof(PatchedDraws::ObjectTransform) : 0);
      const size_t read_size = (vertex_shader.read_size != 0 ? std::clamp<size_t>(vertex_shader.read_size, (std::min)(min_size, entry->latest.size()), entry->latest.size()) : entry->latest.size());
      if (read_size < entry->latest.size())
         return NewConstantsCopy(gd, entry->latest.data(), read_size);
      if (!entry->copy)
      {
         entry->copy = NewConstantsCopy(gd, entry->latest.data(), read_size);
      }
      return entry->copy;
   }

   // A scene draw's vc4 buffer with its upload registered for copies at every Map and Update. The frame's camera is taken here, from
   // the first scene view projection draw; the reprojection with it.
   // "entry" is null without an upload holding the draw's camera. For a vertex shader with a camera.
   struct SceneDrawConstants
   {
      ID3D11Buffer* buffer = nullptr;
      BayonettaGameDeviceData::RegisteredConstants* entry = nullptr;
   };
   static SceneDrawConstants ReadSceneDrawConstants(ID3D11DeviceContext* native_device_context, BayonettaGameDeviceData* gd, const BayonettaGameDeviceData::PatchedShader<ID3D11VertexShader>& vertex_shader)
   {
      SceneDrawConstants result;
      result.buffer = BoundState(gd, gd->bound_object_buffer, [&](ID3D11Buffer** buffer)
         { native_device_context->VSGetConstantBuffers(MotionVectorPatches::object_slot, 1, buffer); });
      if (!result.buffer)
         return result;
      UINT known_size;
      BayonettaGameDeviceData::RegisteredConstants* entry = FindRegisteredBuffer(gd, reinterpret_cast<uint64_t>(result.buffer), &known_size);
      if (!entry)
      {
         const auto [registered_buffer, registered] = gd->mv_constants_copies.try_emplace(reinterpret_cast<uint64_t>(result.buffer));
         entry = &registered_buffer->second;
         if (registered)
         {
            AddFilteredBuffer(gd, result.buffer, entry);
         }
      }
      // An upload from before the hooks paused is stale (the draw stays uncopied until the next one)
      if (entry->latest.size() < vertex_shader.camera_offset + CAMERA_SIZE || entry->generation != gd->mv_constants_generation)
         return result;
      result.entry = entry;
      // Any scene view projection draw: the prepass and the base passes share the camera, and the prepass' draws reach here only once
      // they are jittered (after the upscaler's first frame, which needs this camera for its fill)
      if (!gd->mv_camera && vertex_shader.camera_offset == VIEW_PROJECTION_OFFSET)
      {
         gd->mv_camera = SnapshotConstants(gd, entry, vertex_shader);
         // Previous clip space from the current: previous * inverse(current), in double (absolute world translation)
         gd->mv_reprojection.SetIdentity();
         if (gd->mv_previous_camera)
         {
            Math::Matrix44D current, previous;
            std::copy_n(reinterpret_cast<const float*>(gd->mv_camera->data() + VIEW_PROJECTION_OFFSET), 16, current.GetData());
            std::copy_n(reinterpret_cast<const float*>(gd->mv_previous_camera->data() + VIEW_PROJECTION_OFFSET), 16, previous.GetData());
            current.Invert();
            gd->mv_reprojection = previous * current;
         }
      }
      return result;
   }

   // Draws an opaque draw into the fp16 scene (the scene target alone, with the scene depth) with the patched shaders, adding the
   // motion vector target ("target_slot", past the game's) and the previous frame's vc4 ("previous_slots"). False if it can't (the draw
   // then goes to "DrawWithJitter").
   static bool DrawWithMotionVectors(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, const CommandListData& cmd_list_data, DeviceData& device_data,
      const ShaderHashesList<OneShaderPerPipeline>& original_shader_hashes, const std::function<void()>& draw, const com_ptr<ID3D11RenderTargetView> (&rtvs)[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT], ID3D11DepthStencilView* dsv)
   {
      auto& gd = GetGameDeviceData(device_data);
      // The scene target alone, plus the motion vector and mask targets the last patched draws left bound
      for (UINT slot = 1; slot < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; slot++)
      {
         if (rtvs[slot] && (slot != MotionVectorPatches::target_slot || rtvs[slot] != gd.mv_rtv) && (slot != MotionVectorPatches::reactive_slot || rtvs[slot] != gd.mv_reactive_target_rtv))
            return MV_REJECT(REJECT_EXTRA_TARGET);
      }
      if (!rtvs[0] || !dsv || !gd.mv_scene_open)
         return MV_REJECT(REJECT_NO_SCENE);
      if (!IsDepthTested(native_device_context, &gd))
         return MV_REJECT(REJECT_DEPTH_TEST);
      ClassifyBoundBlend(native_device_context, &gd);
      // Alpha blended draws (SRC_ALPHA / INV_SRC_ALPHA) take their own motion vectors too, blended by their alpha: the particles are
      // such draws. Additive ones and the other blends keep what's behind them.
      const bool blended = gd.mv_alpha_blended;
      if ((!gd.mv_blend_opaque && !blended) || (blended && original_shader_hashes.Contains(shader_hashes_SceneRefraction)))
         return MV_REJECT(REJECT_BLEND);
      // Known targets: checked, and the motion vector target built for them (the depth prepass' R32F targets are refused here)
      if (rtvs[0] != gd.mv_scene_rtv || dsv != gd.mv_accepted_dsv)
      {
         if (rtvs[0].get() == gd.mv_refused_rtv && dsv == gd.mv_refused_dsv)
            return MV_REJECT(gd.mv_refused_reason);
         const auto refuse = [&](MotionVectorReject reason)
         {
            gd.mv_refused_rtv = rtvs[0].get();
            gd.mv_refused_dsv = dsv;
            gd.mv_refused_reason = reason;
            return MV_REJECT(reason);
         };
         com_ptr<ID3D11Resource> depth;
         dsv->GetResource(&depth);
         com_ptr<ID3D11Resource> color;
         rtvs[0]->GetResource(&color);
         if (depth != gd.mv_depth || !color || (gd.mv_scene_color && color != gd.mv_scene_color))
            return refuse(REJECT_OTHER_DEPTH_COLOR);
         D3D11_RENDER_TARGET_VIEW_DESC rtv_desc;
         rtvs[0]->GetDesc(&rtv_desc);
         com_ptr<ID3D11Texture2D> color_texture;
         D3D11_TEXTURE2D_DESC color_desc = {};
         if (SUCCEEDED(color->QueryInterface(&color_texture)))
         {
            color_texture->GetDesc(&color_desc);
         }
         // The upscaler reads the scene: it needs a shader resource view. Filtered by the resource, not the view (dgVoodoo binds
         // single-slice array views). A multisampled scene is refused here only on MSAA's first frame ("msaa_active" then stops this path).
         if (rtv_desc.Format != DXGI_FORMAT_R16G16B16A16_FLOAT || color_desc.ArraySize != 1 || color_desc.SampleDesc.Count != 1 || (color_desc.BindFlags & D3D11_BIND_SHADER_RESOURCE) == 0)
            return refuse(REJECT_FORMAT);
         const uint2 size = {color_desc.Width, color_desc.Height};
         if (size != gd.mv_scene_size)
            return refuse(REJECT_SIZE);
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
            if (FAILED(SRBridge::CreateSharableTexture(native_device, desc, &gd.mv_texture)) || FAILED(native_device->CreateRenderTargetView(gd.mv_texture.get(), nullptr, &gd.mv_rtv)))
            {
               gd.mv_texture.reset();
               gd.mv_rtv.reset();
               return MV_REJECT(REJECT_CREATE);
            }
            if (typed_uav_load)
            {
               native_device->CreateUnorderedAccessView(gd.mv_texture.get(), nullptr, &gd.mv_uav);
            }
            native_device->CreateShaderResourceView(gd.mv_texture.get(), nullptr, &gd.mv_srv);
            const CD3D11_TEXTURE2D_DESC depth_desc(DXGI_FORMAT_R32_FLOAT, size.x, size.y, 1, 1, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS);
            if (SUCCEEDED(SRBridge::CreateSharableTexture(native_device, depth_desc, &gd.mv_device_depth)))
            {
               native_device->CreateUnorderedAccessView(gd.mv_device_depth.get(), nullptr, &gd.mv_device_depth_uav);
            }
            gd.mv_frame_ended = true;
         }
         gd.mv_scene_color = color;
         gd.mv_scene_rtv = rtvs[0];
         gd.mv_accepted_dsv = dsv;
      }

      const auto& vertex_shader = GetPatchedVertexShader(native_device, cmd_list_data, device_data, original_shader_hashes.vertex_shaders[0]);
      // The alpha blended draws write no masks: they write their own motion vectors, and FSR's reactive and transparency & composition
      // masks are for content without them (AMD's FSR manual)
      ID3D11PixelShader* pixel_shader = nullptr;
      if (blended)
      {
         pixel_shader = GetPatchedShader<ID3D11PixelShader>(native_device, device_data, original_shader_hashes.pixel_shaders[0], cmd_list_data.pipeline_state_original_pixel_shader, PixelShaderPatch::BLENDED).shader.get();
      }
      else
      {
         if (const uint32_t pixel_shader_hash = original_shader_hashes.pixel_shaders[0]; pixel_shader_hash != gd.mv_last_pixel_shader_hash)
         {
            gd.mv_last_pixel_shader = GetPatchedShader<ID3D11PixelShader>(native_device, device_data, pixel_shader_hash, cmd_list_data.pipeline_state_original_pixel_shader).shader.get();
            gd.mv_last_pixel_shader_hash = pixel_shader_hash;
         }
         pixel_shader = gd.mv_last_pixel_shader;
      }
      if (!vertex_shader.shader || !pixel_shader || !gd.mv_jitter_buffer)
         return MV_REJECT(REJECT_SHADERS);
      // A shader without a camera (view projection or world view projection) has no motion to rebuild
      if (vertex_shader.camera_offset == 0)
         return MV_REJECT(REJECT_NO_CAMERA);
      // The game's vc4 (object, camera and bones in one). The slots added past it stay bound after the draw: no translated shader reads
      // a constant buffer past b4.
      const SceneDrawConstants scene_constants = ReadSceneDrawConstants(native_device_context, &gd, vertex_shader);
      if (std::exchange(gd.mv_frame_ended, false))
      {
         gd.mv_markers_filled = false;
         gd.mv_fill_pending = gd.mv_uav && gd.mv_device_depth_uav && gd.prepass_drawn && gd.prepass_depth_srv && HasShaders(device_data.native_compute_shaders, CompileTimeStringHash("Bayonetta Motion Vector Fill CS"));
         // The fill's marker: the largest float16 (a larger clear value is stored as it in R16G16_FLOAT)
         const FLOAT clear_value = (gd.mv_fill_pending ? 65504.f : 0.f);
         const FLOAT clear[4] = {clear_value, clear_value, 0.f, 0.f};
         native_device_context->ClearRenderTargetView(gd.mv_rtv.get(), clear);
         // The reactive masks: what the alpha blended draws write, from zero
         if (IsReactiveMaskUsed(device_data) && !gd.mv_reactive_target)
         {
            D3D11_TEXTURE2D_DESC mv_desc;
            gd.mv_texture->GetDesc(&mv_desc);
            const CD3D11_TEXTURE2D_DESC reactive_desc(DXGI_FORMAT_R8G8_UNORM, mv_desc.Width, mv_desc.Height, 1, 1, D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE);
            const std::unique_lock lock(gd.mv_mutex);
            if (SUCCEEDED(native_device->CreateTexture2D(&reactive_desc, nullptr, &gd.mv_reactive_target)) && SUCCEEDED(native_device->CreateRenderTargetView(gd.mv_reactive_target.get(), nullptr, &gd.mv_reactive_target_rtv)))
            {
               native_device->CreateShaderResourceView(gd.mv_reactive_target.get(), nullptr, &gd.mv_reactive_target_srv);
            }
            gd.sr_reactive_frame = cb_luma_global_settings.FrameIndex;
         }
         if (gd.mv_reactive_target_rtv)
         {
            const FLOAT zero[4] = {};
            native_device_context->ClearRenderTargetView(gd.mv_reactive_target_rtv.get(), zero);
         }
      }

      // The buffer's copy as of this draw (null until its first upload)
      BayonettaGameDeviceData::ConstantsCopy constants;
      if (scene_constants.entry)
      {
         constants = SnapshotConstants(&gd, scene_constants.entry, vertex_shader);
      }
      // The previous frame's vc4: the same object's from last frame, else this draw's with last frame's camera (no object motion).
      // None (no CPU copy yet, another camera): the current one (zero motion).
      const std::vector<uint8_t>* upload = nullptr;
      if (constants)
      {
         // Draw key: same mesh, same shaders, no instance count
         // The offsets go with the buffers: queried with them, else tracked with them
         UINT vertex_offset = gd.bound_vertex_offset;
         ID3D11Buffer* const vertex_buffer = BoundState(&gd, gd.bound_vertex_buffer, [&](ID3D11Buffer** buffer)
            {
               UINT stride;
               native_device_context->IAGetVertexBuffers(0, 1, buffer, &stride, &vertex_offset); });
         UINT index_offset = gd.bound_index_offset;
         ID3D11Buffer* const index_buffer = BoundState(&gd, gd.bound_index_buffer, [&](ID3D11Buffer** buffer)
            {
               DXGI_FORMAT format;
               native_device_context->IAGetIndexBuffer(buffer, &format, &index_offset); });
#if DEVELOPMENT
         if (g_bound_state_check && gd.bound_states_tracked && (vertex_offset != gd.bound_vertex_offset || index_offset != gd.bound_index_offset))
         {
            gd.mv_stats.bound_state_mismatches++;
         }
#endif
         const DrawDispatchData& draw_data = last_draw_dispatch_data;
         uint64_t key = 0;
         for (const uint64_t value : {uint64_t(original_shader_hashes.vertex_shaders[0]), uint64_t(original_shader_hashes.pixel_shaders[0]), reinterpret_cast<uint64_t>(vertex_buffer), uint64_t(vertex_offset),
                 reinterpret_cast<uint64_t>(index_buffer), uint64_t(index_offset), uint64_t(draw_data.index_count), uint64_t(draw_data.first_index), uint64_t(uint32_t(draw_data.vertex_offset)),
                 uint64_t(draw_data.vertex_count), uint64_t(draw_data.first_vertex)})
         {
            HashCombine(key, value);
         }
         // The tie-break between the objects sharing a key (props, characters): the 3x4 transform at the shader's translation row (the
         // first bone, the world matrix; see "PatchedDraws::ObjectTransform"), copied as it is. The effects without a world matrix have
         // none: their draws keep their own constants with last frame's camera (static).
         PatchedDraws::ObjectTransform transform = {};
         const bool untransformed = vertex_shader.translation_offset == 0;
         if (!untransformed)
         {
            std::memcpy(transform.data(), constants->data() + vertex_shader.translation_offset, sizeof(transform));
         }
#if DEVELOPMENT
         gd.mv_stats.untransformed += untransformed;
#endif

         // ponytail: linear search among the key's candidates (a handful at most); a spatial lookup if big crowds share a mesh
         const BayonettaGameDeviceData::MotionVectorObject* match = nullptr;
         if (const auto previous = gd.mv_previous_objects.find(key); previous != gd.mv_previous_objects.end() && !untransformed)
         {
            match = PatchedDraws::FindNearest(previous->second, transform, [&](const auto& candidate)
               { return candidate.constants->size() == constants->size(); });
         }
         const bool view_projection_shader = vertex_shader.camera_offset == VIEW_PROJECTION_OFFSET;
         // Drawn with the frame's camera (a world view projection shader's can't be told)
         const bool frame_camera = gd.mv_camera && (!view_projection_shader || std::memcmp(constants->data() + VIEW_PROJECTION_OFFSET, gd.mv_camera->data() + VIEW_PROJECTION_OFFSET, CAMERA_SIZE) == 0);
         if (match)
         {
            // Last frame's list outlives the draw ("mv_previous_objects" only changes at the next frame start)
            upload = &*match->constants;
#if DEVELOPMENT
            gd.mv_stats.matched++;
#endif
         }
         else if (gd.mv_previous_camera && frame_camera)
         {
            // Not found, drawn with the frame's camera: its own constants with last frame's camera (camera motion only). A view
            // projection takes the previous one; a world view projection (a static object's) previous * inverse(current) * itself.
            gd.mv_camera_only_copy.assign(constants->begin(), constants->end());
            if (view_projection_shader)
            {
               std::memcpy(gd.mv_camera_only_copy.data() + VIEW_PROJECTION_OFFSET, gd.mv_previous_camera->data() + VIEW_PROJECTION_OFFSET, CAMERA_SIZE);
            }
            else
            {
               Math::Matrix44D world_view_projection;
               std::copy_n(reinterpret_cast<const float*>(constants->data() + WORLD_VIEW_PROJECTION_OFFSET), 16, world_view_projection.GetData());
               const Math::Matrix44D previous_world_view_projection = gd.mv_reprojection * world_view_projection;
               float* const destination = reinterpret_cast<float*>(gd.mv_camera_only_copy.data() + WORLD_VIEW_PROJECTION_OFFSET);
               for (int i = 0; i < 16; i++)
               {
                  destination[i] = float(previous_world_view_projection.GetData()[i]);
               }
            }
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
         gd.mv_objects[key].push_back({.transform = transform, .constants = std::move(constants)});
      }
#if DEVELOPMENT
      else
      {
         gd.mv_stats.uncopied++;
      }
#endif
      // An alpha blended draw blends its motion vector with what's under it: texels no opaque motion vector draw wrote still hold the
      // fill's marker (65504), which the blend would turn into a huge motion. The fill gives them their camera motion first, from the
      // depth prepass (the end of the scene fills the rest and the masks).
      if (blended && gd.mv_fill_pending && !std::exchange(gd.mv_markers_filled, true))
      {
         DrawStateStack<DrawStateStackType::FullGraphics> graphics_state;
         DrawStateStack<DrawStateStackType::Compute> compute_state;
         graphics_state.Cache(native_device_context, device_data.uav_max_count);
         compute_state.Cache(native_device_context, device_data.uav_max_count);
         FillMotionVectors(native_device, native_device_context, device_data, /* write_reactive */ false);
         compute_state.Restore(native_device_context);
         graphics_state.Restore(native_device_context);
      }
      ID3D11Buffer* const previous_current[] = {scene_constants.buffer};
      gd.mv_previous_constants.Bind(native_device, native_device_context, MotionVectorPatches::previous_slots, {&upload, 1}, previous_current, "Bayonetta", {&vertex_shader.read_size, 1});
      BindJitter(native_device_context, &gd);
      // Left bound after the draw (set directly, bypassing Core's state tracking): the game's next draws either bind their own targets
      // and shaders, or are motion vector draws too. The game's pixel shaders write o0 only, so the motion vector target keeps its
      // contents; a mask target left by an earlier draw goes (its max blend would take an undefined value).
      if (rtvs[MotionVectorPatches::target_slot] != gd.mv_rtv || rtvs[MotionVectorPatches::reactive_slot] != nullptr)
      {
         ID3D11RenderTargetView* targets[MotionVectorPatches::target_slot + 1] = {rtvs[0].get()};
         targets[MotionVectorPatches::target_slot] = gd.mv_rtv.get();
         native_device_context->OMSetRenderTargets(MotionVectorPatches::target_slot + 1, targets, dsv);
      }
      PatchedDraws::BindPatchedShader(native_device_context, vertex_shader.shader.get(), &gd.mv_bound_vertex_shader);
      PatchedDraws::BindPatchedShader(native_device_context, pixel_shader, &gd.mv_bound_pixel_shader);

      draw();
#if DEVELOPMENT
      gd.mv_stats.motion_vector_draws++;
#endif
      return true;
   }

   // Jitter for the scene's mesh draws without motion vectors (patched vertex shader, game pixel shader): the depth prepass, lights,
   // shadow projections, decals, translucents. Every draw depth tested against the scene takes the same jitter, or jittered and
   // unjittered depths of the same surface fail each other's test. False if it can't (the draw runs untouched).
   static bool DrawWithJitter(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, const CommandListData& cmd_list_data, DeviceData& device_data,
      const ShaderHashesList<OneShaderPerPipeline>& original_shader_hashes, const std::function<void()>& draw, const com_ptr<ID3D11RenderTargetView> (&rtvs)[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT], ID3D11DepthStencilView* dsv)
   {
      auto& gd = GetGameDeviceData(device_data);
      if (!gd.mv_scene_open || gd.mv_jitter == std::array<float, 2>{} || !gd.mv_jitter_buffer)
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
      if (!gd.jitter_dsv_scene || !IsDepthTested(native_device_context, &gd))
         return false;
      const ID3D11Buffer* const vertex_buffer = BoundState(&gd, gd.bound_vertex_buffer, [&](ID3D11Buffer** buffer)
         {
            UINT stride, offset;
            native_device_context->IAGetVertexBuffers(0, 1, buffer, &stride, &offset); });
      if (!vertex_buffer)
         return false;
      const auto& vertex_shader = GetPatchedVertexShader(native_device, cmd_list_data, device_data, original_shader_hashes.vertex_shaders[0]);
      if (!vertex_shader.shader)
         return false;
      if (vertex_shader.camera_offset != 0)
      {
         // Takes the frame's camera from the prepass' draws, which come through here
         ReadSceneDrawConstants(native_device_context, &gd, vertex_shader);
      }

      // An alpha blended draw into the scene writes its mask, reactive or transparency & composition (its pixel shader patched, the mask
      // target added past the motion vector one)
      ID3D11PixelShader* reactive_shader = nullptr;
      if (IsReactiveMaskUsed(device_data) && gd.mv_reactive_target_rtv && rtvs[0] && rtvs[0] == gd.mv_scene_rtv)
      {
         ClassifyBoundBlend(native_device_context, &gd);
         if (const ReactiveBlend blend = gd.mv_reactive_blend; blend != ReactiveBlend::NONE)
         {
            reactive_shader = GetPatchedShader<ID3D11PixelShader>(native_device, device_data, original_shader_hashes.pixel_shaders[0], cmd_list_data.pipeline_state_original_pixel_shader, (blend == ReactiveBlend::ADDITIVE ? PixelShaderPatch::MASK_ADDITIVE : PixelShaderPatch::MASK_ALPHA)).shader.get();
         }
      }

      // The patched vertex shader and the jitter stay bound after the draw (see "DrawWithMotionVectors"), with the game's pixel shader
      // (a motion vector draw's is put back) or its mask version, and the mask target
      PatchedDraws::BindPatchedShader(native_device_context, vertex_shader.shader.get(), &gd.mv_bound_vertex_shader);
      BindJitter(native_device_context, &gd);
      // A mask target left by an earlier draw goes when this one doesn't write it
      if (ID3D11RenderTargetView* const mask_rtv = (reactive_shader ? gd.mv_reactive_target_rtv.get() : nullptr); rtvs[MotionVectorPatches::reactive_slot].get() != mask_rtv)
      {
         ID3D11RenderTargetView* targets[MotionVectorPatches::reactive_slot + 1] = {};
         for (uint32_t slot = 0; slot < MotionVectorPatches::reactive_slot; slot++)
         {
            targets[slot] = rtvs[slot].get();
         }
         targets[MotionVectorPatches::reactive_slot] = mask_rtv;
         native_device_context->OMSetRenderTargets(MotionVectorPatches::reactive_slot + 1, targets, dsv);
      }
      if (reactive_shader)
      {
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

   // DLAA, FSR 3 Native AA or Luma TAA on the jittered scene, its depth (from the depth prepass, see the fill) and the motion vectors;
   // the result goes back into the scene's color (and its copy, see "mv_scene_copy"), its alpha (the bloom source's threshold input)
   // kept. False if it didn't draw (missing input, or the upscaler failed). "reactive_mask": the fill wrote this scene's masks.
   static bool DrawUpscaler(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, DeviceData& device_data, bool reactive_mask)
   {
      auto& gd = GetGameDeviceData(device_data);
      auto* const copy_vs = FindShader(device_data.native_vertex_shaders, CompileTimeStringHash("Copy VS"));
      auto* const copy_ps = FindShader(device_data.native_pixel_shaders, CompileTimeStringHash("Copy PS"));
      if (!gd.mv_texture || !gd.mv_camera || !gd.mv_device_depth || !gd.mv_scene_color || !gd.mv_scene_rtv || !copy_vs || !copy_ps)
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
      {
         device_data.sr_output_color->GetDesc(&output_desc);
      }
      bool output_recreated = false;
      if (output_desc.Width != scene_desc.Width || output_desc.Height != scene_desc.Height)
      {
         device_data.sr_output_color.reset();
         gd.sr_output_srv.reset();
         output_desc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R16G16B16A16_FLOAT, scene_desc.Width, scene_desc.Height, 1, 1, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS);
         if (SUCCEEDED(SRBridge::CreateSharableTexture(native_device, output_desc, &device_data.sr_output_color)))
         {
            native_device->CreateShaderResourceView(device_data.sr_output_color.get(), nullptr, &gd.sr_output_srv);
         }
         output_recreated = true;
      }
      if (!device_data.sr_output_color || !gd.sr_output_srv)
      {
         // Back to SMAA until the upscaler is picked again
         device_data.sr_suppressed = true;
         return false;
      }
      if (!gd.GetRGBBlendState(native_device))
         return false;

      // FSR needs the camera (column vectors: vc4's view projection rows are the clip components)
      const SR::ViewProjectionCamera camera = SR::GetViewProjectionCamera(reinterpret_cast<const float*>(gd.mv_camera->data() + VIEW_PROJECTION_OFFSET), /* row_vectors */ false);

      const SR::SettingsData settings_data = {
         .output_width = scene_desc.Width,
         .output_height = scene_desc.Height,
         .render_width = scene_desc.Width,
         .render_height = scene_desc.Height,
         // The gamma encoded scene (1 = SDR white, the values above it from the lighting, grade and bloom)
         .hdr = true,
         // The motion vectors are UV deltas, previous minus current
         .mvs_x_scale = float(scene_desc.Width),
         .mvs_y_scale = float(scene_desc.Height),
         // DLSS's auto exposure; FSR's clips highlights (FSR-Best-Practices FIN-3), so it runs at exposure 1
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
         .reset = device_data.force_reset_sr || output_recreated,
         .output_color = device_data.sr_output_color.get(),
         .source_color = scene.get(),
         .motion_vectors = gd.mv_texture.get(),
         .depth_buffer = gd.mv_device_depth.get(),
         .bias_mask = (reactive_mask ? gd.mv_reactive.get() : nullptr),
         .transparency_alpha = (reactive_mask ? gd.mv_transparency.get() : nullptr),
         // As applied (pixels, +y down)
         .jitter_x = gd.mv_jitter[0],
         .jitter_y = gd.mv_jitter[1],
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
      // The RGB write mask keeps the scene's alpha; the copy is the first post pass' input, written in the same pass (with a view like
      // the scene's: dgVoodoo's are single slice arrays), else copied whole after it
      ID3D11RenderTargetView* copy_rtv = nullptr;
      auto* const copy_back_ps = FindShader(device_data.native_pixel_shaders, CompileTimeStringHash("Bayonetta Copy Back PS"));
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
               D3D11_TEXTURE2D_DESC copy_desc;
               copy->GetDesc(&copy_desc);
               D3D11_RENDER_TARGET_VIEW_DESC rtv_desc;
               gd.mv_scene_rtv->GetDesc(&rtv_desc);
               if (copy_desc.BindFlags & D3D11_BIND_RENDER_TARGET)
               {
                  native_device->CreateRenderTargetView(copy.get(), &rtv_desc, &gd.mv_scene_copy_rtv);
               }
            }
         }
         copy_rtv = gd.mv_scene_copy_rtv.get();
      }
      DrawCustomPixelShader(native_device_context, device_data.default_depth_stencil_state.get(), gd.sr_rgb_blend_state.get(), nullptr, copy_vs, copy_rtv ? copy_back_ps : copy_ps,
         gd.sr_output_srv.get(), gd.mv_scene_rtv.get(), scene_desc.Width, scene_desc.Height, true, copy_rtv);
      if (gd.mv_scene_copy && !copy_rtv)
      {
         native_device_context->CopyResource(gd.mv_scene_copy.get(), scene.get());
      }
      // Not while the bridge's helper starts (the color copied as it is): SMAA stays on and the next frame resets
      device_data.has_drawn_sr = sr_implementations[device_data.sr_type]->IsReady(sr_instance_data);
      return true;
   }

   // The fill's constants ("Luma_Bayonetta_MotionVectorFill.hlsl" b0)
   struct MotionVectorFillConstants
   {
      float reprojection[16]; // Current clip space to the previous frame's, row major
      float jitter_ndc[2];
      float reactive_scale;
      float reactive_threshold;
      float reactive_enabled;
      float padding[3];
   };
   static_assert(sizeof(MotionVectorFillConstants) == 96);

   // The depth and camera motion fill (see "Luma_Bayonetta_MotionVectorFill.hlsl"), with the reactive masks if "write_reactive". The
   // caller caches and restores the state. False if it didn't run.
   static bool FillMotionVectors(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, DeviceData& device_data, bool write_reactive)
   {
      auto& gd = GetGameDeviceData(device_data);
      auto* const fill_shader = FindShader(device_data.native_compute_shaders, CompileTimeStringHash("Bayonetta Motion Vector Fill CS"));
      if (!fill_shader || !gd.mv_camera || !gd.prepass_drawn || !gd.prepass_depth_srv)
         return false;
      MotionVectorFillConstants constants = {
         .jitter_ndc = {gd.mv_jitter_ndc[0], gd.mv_jitter_ndc[1]},
         .reactive_scale = g_sr_reactive_scale,
         .reactive_threshold = g_sr_reactive_threshold,
         .reactive_enabled = (write_reactive ? 1.f : 0.f),
      };
      for (int i = 0; i < 16; i++)
      {
         constants.reprojection[i] = float(gd.mv_reprojection.GetData()[i]);
      }
      if (!PatchedDraws::WriteDynamicConstants(native_device, native_device_context, std::addressof(gd.mv_fill_buffer), &constants, sizeof(constants)))
         return false;
      // The scene and the motion vectors may be bound as render targets
      native_device_context->OMSetRenderTargets(0, nullptr, nullptr);
      ID3D11Buffer* const buffer = gd.mv_fill_buffer.get();
      ID3D11ShaderResourceView* const srvs[2] = {gd.prepass_depth_srv.get(), write_reactive ? gd.mv_reactive_target_srv.get() : nullptr};
      ID3D11UnorderedAccessView* const uavs[4] = {gd.mv_uav.get(), gd.mv_device_depth_uav.get(), write_reactive ? gd.mv_reactive_uav.get() : nullptr, write_reactive ? gd.mv_transparency_uav.get() : nullptr};
      native_device_context->CSSetConstantBuffers(0, 1, &buffer);
      native_device_context->CSSetShaderResources(0, UINT(std::size(srvs)), srvs);
      native_device_context->CSSetUnorderedAccessViews(0, UINT(std::size(uavs)), uavs, nullptr);
      native_device_context->CSSetShader(fill_shader, nullptr, 0);
      native_device_context->Dispatch((gd.mv_scene_size.x + 7) / 8, (gd.mv_scene_size.y + 7) / 8, 1);
      ID3D11UnorderedAccessView* const null_uavs[std::size(uavs)] = {};
      ID3D11ShaderResourceView* const null_srvs[std::size(srvs)] = {};
      native_device_context->CSSetUnorderedAccessViews(0, UINT(std::size(null_uavs)), null_uavs, nullptr);
      native_device_context->CSSetShaderResources(0, UINT(std::size(null_srvs)), null_srvs);
      if (write_reactive)
      {
         gd.sr_reactive_frame = cb_luma_global_settings.FrameIndex;
      }
      return true;
   }

   // Ends the scene at its first post pass: the fill (the last camera motion, the depth, the reactive masks), then the upscaler, both
   // before that pass reads the scene
   static void EndScene(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, DeviceData& device_data)
   {
      auto& gd = GetGameDeviceData(device_data);
      gd.mv_scene_open = false;
      gd.mv_scene_done = true;
      DrawStateStack<DrawStateStackType::FullGraphics> graphics_state;
      DrawStateStack<DrawStateStackType::Compute> compute_state;
      graphics_state.Cache(native_device_context, device_data.uav_max_count);
      compute_state.Cache(native_device_context, device_data.uav_max_count);
      // The reactive and transparency & composition masks, written by the fill from what the alpha blended draws wrote
      const bool reactive = IsReactiveMaskUsed(device_data) && gd.mv_reactive_target_srv;
      if (reactive)
      {
         D3D11_TEXTURE2D_DESC desc = {};
         if (gd.mv_reactive)
         {
            gd.mv_reactive->GetDesc(&desc);
         }
         if (desc.Width != gd.mv_scene_size.x || desc.Height != gd.mv_scene_size.y)
         {
            const std::unique_lock lock(gd.mv_mutex);
            gd.mv_reactive.reset();
            gd.mv_reactive_uav.reset();
            gd.mv_transparency.reset();
            gd.mv_transparency_uav.reset();
            desc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R8_UNORM, gd.mv_scene_size.x, gd.mv_scene_size.y, 1, 1, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS);
            if (SUCCEEDED(SRBridge::CreateSharableTexture(native_device, desc, &gd.mv_reactive)))
            {
               native_device->CreateUnorderedAccessView(gd.mv_reactive.get(), nullptr, &gd.mv_reactive_uav);
            }
            if (SUCCEEDED(SRBridge::CreateSharableTexture(native_device, desc, &gd.mv_transparency)))
            {
               native_device->CreateUnorderedAccessView(gd.mv_transparency.get(), nullptr, &gd.mv_transparency_uav);
            }
         }
      }
      const bool write_reactive = reactive && gd.mv_reactive_uav && gd.mv_transparency_uav;
      const bool filled = std::exchange(gd.mv_fill_pending, false) && FillMotionVectors(native_device, native_device_context, device_data, write_reactive);
      gd.mv_filled = filled;
      // The upscaler's depth comes from the fill
      if (gd.sr_active && filled)
      {
         [[maybe_unused]] const bool drawn = DrawUpscaler(native_device, native_device_context, device_data, write_reactive);
#if DEVELOPMENT
         gd.mv_stats.sr_draws += drawn;
#endif
      }
      compute_state.Restore(native_device_context);
      graphics_state.Restore(native_device_context);
   }

public:
   // The motion vector target in every blend state, the tracked bindings (always current, so the first motion vector frame finds
   // them), the MSAA targets' shader resource bind and the vc4 copies
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
      set.template operator()<reshade::addon_event::create_pipeline>(PatchedDraws::OnCreateBlendState<MotionVectorPatches::target_slot, MotionVectorPatches::reactive_slot, false, true>);
      set.template operator()<reshade::addon_event::bind_pipeline>(OnBindPipeline);
      set.template operator()<reshade::addon_event::push_descriptors>(OnPushConstantBuffers);
      set.template operator()<reshade::addon_event::bind_vertex_buffers>(OnBindVertexBuffers);
      set.template operator()<reshade::addon_event::bind_index_buffer>(OnBindIndexBuffer);
      set.template operator()<reshade::addon_event::create_resource>(OnCreateResource);
      set.template operator()<reshade::addon_event::copy_resource>(OnCopyResource);
      set.template operator()<reshade::addon_event::copy_texture_region>(OnCopyTextureRegion);
      // The vc4 copies' events stay registered too: ReShade changes its callback lists without a lock, and dgVoodoo presents from
      // another thread than the frame's (registering them at present would race the frame's events). They return at once while
      // "mv_active" is off.
      set.template operator()<reshade::addon_event::map_buffer_region>(OnMapBufferRegion);
      set.template operator()<reshade::addon_event::unmap_buffer_region>(OnUnmapBufferRegion);
      set.template operator()<reshade::addon_event::update_buffer_region>(OnUpdateBufferRegion);
   }

   void OnInit(bool async) override
   {
      // The canvas stays gamma encoded (1 = paper white, gamma 2.2 in and out as the other dgVoodoo mods). The scene copy before the
      // UI does the HDR display map and pre-scales the scene by the UI paper white, which the composition applies to the canvas.
      std::vector<ShaderDefineData> game_shader_defines_data = {
         {"TONEMAP_TYPE", '1', true, false, "0 - SDR: Vanilla (clamped reference)\n1 - HDR: extended grade + PsychoV31 display map", 1},
         {"XE_GTAO_QUALITY", '3', true, false, "XeGTAO quality (slice count)\n0 - Low\n1 - Medium\n2 - High\n3 - Very High\n4 - Ultra", 4},
      };
      shader_defines_data.append_range(game_shader_defines_data);
      assert(shader_defines_data.size() < MAX_SHADER_DEFINES);
      GetShaderDefineData(POST_PROCESS_SPACE_TYPE_HASH).SetDefaultValue('0');
      GetShaderDefineData(EARLY_DISPLAY_ENCODING_HASH).SetDefaultValue('0');
      GetShaderDefineData(VANILLA_ENCODING_TYPE_HASH).SetDefaultValue('1');
      GetShaderDefineData(GAMMA_CORRECTION_TYPE_HASH).SetDefaultValue('1');
      GetShaderDefineData(UI_DRAW_TYPE_HASH).SetDefaultValue('2');

      // dgVoodoo's translated shaders use b3 (its state) and b4 (the D3D9 constants)
      luma_settings_cbuffer_index = 13;
      luma_data_cbuffer_index = 12;
      luma_ui_cbuffer_index = -1;

      sr_game_tooltip = "Needs Anti-Aliasing off in the game's options. DLSS and FSR 3 require Luma-Upscaler.exe next to the game's exe.\n";

      default_luma_global_game_settings.Dithering = 1.f;
      // Light AutoHDR on the movies (the TW1/ME1 pair: at 0.5 the peak is 165 nits)
      default_luma_global_game_settings.VideoAutoHDREnable = 1.f;
      default_luma_global_game_settings.VideoAutoHDRBoost = 0.5f;
      // The scene copy's PsychoV31 grading, neutral
      default_luma_global_game_settings.Contrast = 1.f;
      default_luma_global_game_settings.Saturation = 1.f;
      default_luma_global_game_settings.HighlightsDesaturation = 0.f;
      default_luma_global_game_settings.ColorGradingIntensity = 1.f;
      default_luma_global_game_settings.BloomIntensity = 1.f;
      cb_luma_global_settings.GameSettings = default_luma_global_game_settings;

      FrameLimiter::Install();
      ShadowMap::Install();
#if DEVELOPMENT
      Mcp::RegisterCounters({{"frame_limiter.late_frames", &FrameLimiter::late_frames}, {"frame_limiter.resyncs", &FrameLimiter::resyncs}, {"frame_limiter.adopted_deadlines", &FrameLimiter::adopted_deadlines},
         {"frame_limiter.period_us", &FrameLimiter::period_us}, { "frame_limiter.spin_margin_us",
            &FrameLimiter::spin_margin_us }});
      Mcp::RegisterToggles({
         {"fix_frame_pacing", &g_fix_frame_pacing},
         {"60_fps_cutscenes", &g_60_fps_cutscenes},
         {"luma_bloom_enable", &g_luma_bloom_enable},
         {"hide_ui", &g_hide_ui},
         {"msaa_resolve_enable", &g_msaa_resolve_enable},
         {"msaa_coverage_enable", &g_msaa_coverage_enable},
         {"msaa_resolve_wide", &g_msaa_resolve_wide},
         {"smaa_enable", &g_smaa_enable},
         {"smaa_t2x", &g_smaa_t2x},
         {"smaa_predication", &g_smaa_predication},
         {"smaa_pred_debug", &g_smaa_pred_debug},
         {"mv_enable", &g_mv_enable},
         {"mv_debug_view", &g_mv_debug_view},
         {"mv_force_jitter", &g_mv_force_jitter},
         {"bound_state_check", &g_bound_state_check},
         {"sr_reactive_enable", &g_sr_reactive_enable},
         {"sr_reactive_debug_view", &g_sr_reactive_debug_view},
      });
      Mcp::RegisterValues({{"smaa_pred_tolerance", &g_smaa_pred_tolerance, 0.002f, 0.2f}, {"rcas_sharpness", &g_rcas_sharpness, 0.f, 1.f}, {"sr_reactive_scale", &g_sr_reactive_scale, 0.f, 4.f}, {"sr_reactive_threshold", &g_sr_reactive_threshold, 0.f, 1.f}});
      Mcp::RegisterToggles({{"gtao_enable", &g_gtao_enable}, { "resolution_scaled_effects",
                               &g_resolution_scaled_effects }});
      Mcp::RegisterValues({{"gtao_final_value_power", &g_gtao_final_value_power, 0.3f, 4.5f}, {"gtao_depth_scale", &g_gtao_depth_scale, 0.01f, 200.f}, {"gtao_radius_override", &g_gtao_radius_override, 0.f, 5.f}});
      Mcp::RegisterInts({{"gtao_debug_view", &g_gtao_debug_view, 0, 4}});
      Mcp::RegisterTextures({MCP_GAME_TEXTURE("gtao.depth_mips", gtao_depth_mips), MCP_GAME_TEXTURE("gtao.output", gtao_output)});
      Mcp::RegisterTextures({MCP_GAME_TEXTURE("bloom.target", bloom_texture)});
      Mcp::RegisterInts({{"bloom_scale_override", &g_bloom_scale_override, 0, 4}});
      Mcp::RegisterTextures({MCP_GAME_TEXTURE("smaa.pred_mask", smaa_predication_srv), MCP_GAME_TEXTURE("canvas.scene", scene_srv), MCP_GAME_TEXTURE("depth_prepass", depth_srv),
         MCP_GAME_TEXTURE("mv.velocity", mv_texture), MCP_GAME_TEXTURE("mv.depth", mv_device_depth), MCP_GAME_TEXTURE("mv.prepass", prepass_depth_srv),
         MCP_GAME_TEXTURE("sr.reactive", mv_reactive), MCP_GAME_TEXTURE("sr.transparency", mv_transparency), MCP_GAME_TEXTURE("sr.draws_mask", mv_reactive_target)});
#endif

      // SMAA: Core registers its 6 passes ("Luma_SMAA_impl.hlsl"); the predication mask from the depth prepass
      native_shaders_definitions.emplace(CompileTimeStringHash("Bayonetta Depth Edges CS"), ShaderDefinition("Luma_Bayonetta_DepthEdges", reshade::api::pipeline_subobject_type::compute_shader));
      // SMAA T2x's own passes ("SMAA_T2X" in Luma_SMAA_impl.hlsl). The edge detection and the vertex shaders are 1x's.
      native_shaders_definitions.emplace(CompileTimeStringHash("Bayonetta SMAA T2x Blending Weight Calculation PS"),
         ShaderDefinition{"Luma_SMAA_impl", reshade::api::pipeline_subobject_type::pixel_shader, nullptr, "smaa_blending_weight_calculation_ps", {{"SMAA_T2X", "1"}}});
      native_shaders_definitions.emplace(CompileTimeStringHash("Bayonetta SMAA T2x Neighborhood Blending PS"),
         ShaderDefinition{"Luma_SMAA_impl", reshade::api::pipeline_subobject_type::pixel_shader, nullptr, "smaa_neighborhood_blending_ps", {{"SMAA_T2X", "1"}}});
      native_shaders_definitions.emplace(CompileTimeStringHash("Bayonetta SMAA T2x Resolve PS"),
         ShaderDefinition{"Luma_SMAA_impl", reshade::api::pipeline_subobject_type::pixel_shader, nullptr, "smaa_resolve_ps", {{"SMAA_T2X", "1"}}});
      // DLAA/FSR/Luma TAA: their depth and the camera motion from the depth prepass, the CPU copies of vc4 (dgVoodoo maps it or updates
      // it), and the motion vector target written by every blend state
      native_shaders_definitions.emplace(CompileTimeStringHash("Bayonetta Motion Vector Fill CS"), ShaderDefinition("Luma_Bayonetta_MotionVectorFill", reshade::api::pipeline_subobject_type::compute_shader));
      native_shaders_definitions.emplace(CompileTimeStringHash("Bayonetta Copy Back PS"), ShaderDefinition("Luma_Bayonetta_CopyBack", reshade::api::pipeline_subobject_type::pixel_shader));
      // XeGTAO in place of the game's SSAO (see "RunXeGTAO"); the two denoisers differ only by XE_GTAO_FINAL_APPLY
      native_shaders_definitions.emplace(CompileTimeStringHash("Bayonetta XeGTAO Prefilter Depths CS"),
         ShaderDefinition{"Luma_Bayonetta_XeGTAO", reshade::api::pipeline_subobject_type::compute_shader, nullptr, "prefilter_depths16x16_cs"});
      native_shaders_definitions.emplace(CompileTimeStringHash("Bayonetta XeGTAO Main Pass CS"),
         ShaderDefinition{"Luma_Bayonetta_XeGTAO", reshade::api::pipeline_subobject_type::compute_shader, nullptr, "main_pass_cs"});
      native_shaders_definitions.emplace(CompileTimeStringHash("Bayonetta XeGTAO Denoise Pass 1 CS"),
         ShaderDefinition{"Luma_Bayonetta_XeGTAO", reshade::api::pipeline_subobject_type::compute_shader, nullptr, "denoise_pass_cs", {{"XE_GTAO_FINAL_APPLY", "0"}}});
      native_shaders_definitions.emplace(CompileTimeStringHash("Bayonetta XeGTAO Denoise Pass 2 CS"),
         ShaderDefinition{"Luma_Bayonetta_XeGTAO", reshade::api::pipeline_subobject_type::compute_shader, nullptr, "denoise_pass_cs", {{"XE_GTAO_FINAL_APPLY", "1"}}});
      native_shaders_definitions.emplace(CompileTimeStringHash("Bayonetta XeGTAO Apply PS"), ShaderDefinition("Luma_Bayonetta_XeGTAOApply", reshade::api::pipeline_subobject_type::pixel_shader));
      // The weighted MSAA resolve on the samples as they are: the scene is gamma encoded, its values above 1 from the lighting, grade
      // and bloom (1 = SDR white). The 8-bit targets averaged samples clipped at 1, the fp16 mirrors the unclipped ones, which the weight
      // brings back to the edge gradient the game was made for. Alpha (the bloom source's threshold input) keeps the average.
      native_shaders_definitions.emplace(CompileTimeStringHash("MSAA Resolve PS"), ShaderDefinition("Luma_MSAAResolve_PS", reshade::api::pipeline_subobject_type::pixel_shader));
#if DEVELOPMENT
      native_shaders_definitions.emplace(CompileTimeStringHash("MSAA Resolve Wide PS"), ShaderDefinition{"Luma_MSAAResolve_PS", reshade::api::pipeline_subobject_type::pixel_shader, nullptr, nullptr, { {"MSAA_RESOLVE_FILTER_RADIUS", "1.0"} }});
#endif
      SetEvents(true);
   }

   std::unique_ptr<std::byte[]> PatchShaderBytecodeSync(const std::byte* code, size_t& size, reshade::api::pipeline_subobject_type type, uint64_t shader_hash, const std::byte* shader_object, size_t shader_object_size) override
   {
      if (type != reshade::api::pipeline_subobject_type::pixel_shader)
         return nullptr;
      // The pixel kernels' target size from "TARGET_SIZE_CB_SLOT" (see "shader_hashes_PixelKernels"), then the clamps below on top
      std::unique_ptr<std::byte[]> redirected;
      if (shader_hashes_PixelKernels.Contains(uint32_t(shader_hash), reshade::api::shader_stage::pixel))
      {
         redirected = DXBC::RedirectConstantRow(code, &size, MotionVectorPatches::object_slot, 202, TARGET_SIZE_CB_SLOT, 0);
         ASSERT_ONCE(redirected);
         if (redirected)
         {
            code = redirected.get();
         }
      }
      // A clamp's patch, else the redirected program (or none)
      const auto patched = [&](std::unique_ptr<std::byte[]> clamped)
      {
         return clamped ? std::move(clamped) : std::move(redirected);
      };
      if (velocity_pixel_shaders.contains(uint32_t(shader_hash)))
         return patched(DXBC::ClampOutput0(code, &size, DXBC::Output0Clamp::UNORM8));
      if (canvas_clamped_pixel_shaders.contains(uint32_t(shader_hash)))
         return patched(DXBC::ClampOutput0(code, &size));
      // Every other float target 0 (dgVoodoo's own R32_UINT outputs aside): the game's targets are A8R8G8B8 (upgraded to fp16, which
      // doesn't clamp blend inputs) or R32F (depth in 0-1). Scene alpha reached 2 in captures, making the SRC_ALPHA/INV_SRC_ALPHA
      // blends' destination weight negative.
      std::vector<DXBC::Chunk> chunks;
      std::vector<DXBC::SignatureElement> outputs;
      DXBC::Chunk* output_signature = nullptr;
      if (DXBC::ReadChunks(reinterpret_cast<const uint8_t*>(shader_object), shader_object_size, &chunks))
      {
         output_signature = DXBC::FindChunk(&chunks, DXBC::FourCC("OSGN"));
      }
      if (!output_signature || !DXBC::ReadSignature(output_signature->data, &outputs))
         return patched(nullptr);
      const auto target = std::ranges::find_if(outputs, [](const DXBC::SignatureElement& element)
         { return MotionVectorPatch::IsTarget(element) && element.reg == 0; });
      if (target == outputs.end() || target->component_type != D3D_REGISTER_COMPONENT_FLOAT32)
         return patched(nullptr);
      return patched(DXBC::ClampOutput0(code, &size, DXBC::Output0Clamp::BLEND_INPUT, target->mask));
   }

   // Every patch a draw may ask "GetPatchedShader" for: which one it takes depends on its state
   void AddShaderWarmupVariants(reshade::api::pipeline_subobject_type type, const std::vector<uint8_t>& code, std::vector<std::vector<uint8_t>>* variants) override
   {
      std::string error;
      if (type == reshade::api::pipeline_subobject_type::vertex_shader)
      {
         if (std::vector<uint8_t> patched = MotionVectorPatch::PatchVertexShader(code.data(), code.size(), MotionVectorPatches::layout, &error); !patched.empty())
         {
            variants->push_back(std::move(patched));
         }
         return;
      }
      for (size_t patch = 0; patch < size_t(PixelShaderPatch::COUNT); patch++)
      {
         if (std::vector<uint8_t> patched = PatchPixelShader(PixelShaderPatch(patch), code.data(), code.size(), &error); !patched.empty())
         {
            variants->push_back(std::move(patched));
         }
      }
   }

   void OnCreateDevice(ID3D11Device* native_device, DeviceData& device_data) override
   {
      device_data.game = new BayonettaGameDeviceData;
      device_data.taa_detected = true; // No TAA to replace, but Core's upscaler status checks for it
#if DEVELOPMENT
      const auto& stats = GetGameDeviceData(device_data).aa_last_stats;
      Mcp::RegisterCounters(
         {
            {"msaa_resolves", &stats.msaa_resolves},
            {"smaa", &stats.smaa},
            {"smaa_predicated", &stats.smaa_predicated},
            {"msaa_coverage", &stats.msaa_coverage},
         },
         &device_data);
      // The last complete frame's motion vector counts
      const auto& mv_stats = GetGameDeviceData(device_data).mv_last_stats;
      Mcp::RegisterCounters({{"mv.draws", &mv_stats.motion_vector_draws}, {"mv.jitter_draws", &mv_stats.jitter_draws}, {"mv.reactive_draws", &mv_stats.reactive_draws}, {"mv.matched", &mv_stats.matched},
                               {"mv.camera_only", &mv_stats.camera_only}, {"mv.other_camera", &mv_stats.other_camera}, {"mv.uncopied", &mv_stats.uncopied}, {"mv.maps", &mv_stats.maps}, {"mv.updates", &mv_stats.updates},
                               {"mv.other_maps", &mv_stats.other_maps}, {"mv.sr_draws", &mv_stats.sr_draws}, {"mv.tiebreak_collisions", &mv_stats.tiebreak_collisions}, {"mv.untransformed", &mv_stats.untransformed},
                               {"mv.bound_state_mismatches", &mv_stats.bound_state_mismatches}, {"mv.constants_pool", &mv_stats.constants_pool},
                               {"mv.prepass_drawn", &mv_stats.prepass_drawn}, { "mv.ended_by_hash",
                                  &mv_stats.ended_by }},
         &device_data);
      for (size_t i = 0; i < std::size(MOTION_VECTOR_REJECT_NAMES); i++)
      {
         Mcp::RegisterCounter(std::string("mv.rejected.") + MOTION_VECTOR_REJECT_NAMES[i], &mv_stats.rejected[i], &device_data);
      }
#endif
   }

   void OnDestroyDeviceData(DeviceData& device_data) override
   {
#if DEVELOPMENT
      Mcp::Unregister(&device_data);
#endif
      Game::OnDestroyDeviceData(device_data);
   }

   // The game's MSAA colour resolves, on the fp16 mirrors: Core's weighted one instead. Anything else, or a pair it can't bind, keeps
   // the hardware resolve. A scene resolve marks the game's MSAA on ("msaa_active") whether Luma's draws it or not.
   bool OverrideResolveTextureRegion(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, DeviceData& device_data, uint64_t dst_resource, uint64_t src_resource, DXGI_FORMAT format) override
   {
      if (format != DXGI_FORMAT_R16G16B16A16_FLOAT)
         return false;
      GetGameDeviceData(device_data).msaa_resolved = true;
      if (!g_msaa_resolve_enable)
         return false;
      // Held through the draw so a shader reload can't release them mid-use
      const std::shared_lock lock_shader_objects(s_mutex_shader_objects);
      const uint32_t resolve_ps = (g_msaa_resolve_wide ? CompileTimeStringHash("MSAA Resolve Wide PS") : CompileTimeStringHash("MSAA Resolve PS"));
      if (!DrawMSAAResolve(native_device, native_device_context, device_data, reinterpret_cast<ID3D11Resource*>(src_resource), reinterpret_cast<ID3D11Resource*>(dst_resource), format, nullptr, nullptr, resolve_ps))
         return false;
#if DEVELOPMENT
      GetGameDeviceData(device_data).aa_stats.msaa_resolves++;
#endif
      return true;
   }

   bool OverrideCopyResource(ID3D11Device* native_device, DeviceData& device_data, uint64_t& dst_resource, uint64_t& src_resource) override
   {
      auto& game_device_data = GetGameDeviceData(device_data);
      if (device_data.back_buffers.contains(dst_resource) || dst_resource == game_device_data.present_source)
      {
         game_device_data.canvas = src_resource;
      }
      if (game_device_data.mv_scene_open && game_device_data.mv_scene_color && src_resource == reinterpret_cast<uint64_t>(game_device_data.mv_scene_color.get()))
      {
         game_device_data.mv_scene_last_copy = reinterpret_cast<ID3D11Resource*>(dst_resource);
      }
      return false;
   }

   // The target size "shader_hashes_PixelKernels" read in place of c194: render target 0's, or what it would be at 720p (width * 720 /
   // height, 720) with "g_resolution_scaled_effects". Kept bound: neither dgVoodoo nor Luma uses its slot.
   static void BindPixelKernelTargetSize(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, BayonettaGameDeviceData* gd)
   {
      com_ptr<ID3D11RenderTargetView> rtv;
      native_device_context->OMGetRenderTargets(1, &rtv, nullptr);
      if (!rtv)
         return;
      const uint2 size = GetViewTextureSize(rtv.get());
      if (size.y == 0)
         return;
      const float scale = (g_resolution_scaled_effects ? 720.f / float(size.y) : 1.f);
      const std::array<float, 4> target_size = {float(size.x) * scale, float(size.y) * scale, 0.f, 0.f};
      if (!gd->pixel_kernel_target_size_buffer)
      {
         const D3D11_BUFFER_DESC desc = {.ByteWidth = sizeof(target_size), .Usage = D3D11_USAGE_DEFAULT, .BindFlags = D3D11_BIND_CONSTANT_BUFFER};
         if (FAILED(native_device->CreateBuffer(&desc, nullptr, &gd->pixel_kernel_target_size_buffer)))
            return;
         gd->pixel_kernel_target_size = {};
      }
      if (target_size != gd->pixel_kernel_target_size)
      {
         native_device_context->UpdateSubresource(gd->pixel_kernel_target_size_buffer.get(), 0, nullptr, target_size.data(), 0, 0);
         gd->pixel_kernel_target_size = target_size;
      }
      ID3D11Buffer* const buffer = gd->pixel_kernel_target_size_buffer.get();
      native_device_context->PSSetConstantBuffers(TARGET_SIZE_CB_SLOT, 1, &buffer);
   }

   // XeGTAO's camera: where a depth prepass vertex shader keeps its affine view transform and the same transform followed by the
   // projection, as rows of dgVoodoo's cb4 (D3D9 c(n) = row n + 20): "Rmodelview" c20 + "Rmodelviewproj" c24 (static meshes, grass) or
   // "view" c0 + "g_ViewProjection" c8 (skinned meshes). {} for any other shader. Cached by hash.
   static uint2 ClassifyPrepassCamera(DeviceData& device_data, BayonettaGameDeviceData* gd, uint32_t hash, reshade::api::pipeline pipeline)
   {
      if (const auto it = gd->gtao_camera_layouts.find(hash); it != gd->gtao_camera_layouts.end())
         return it->second;
      uint2 rows = {};
      {
         const std::shared_lock lock(s_mutex_generic);
         if (const auto it = device_data.pipeline_cache_by_pipeline_handle.find(pipeline.handle); it != device_data.pipeline_cache_by_pipeline_handle.end() && it->second->subobjects_cache)
         {
            const auto* desc = static_cast<const reshade::api::shader_desc*>(it->second->subobjects_cache[0].data);
            const auto reads = [&](uint32_t row)
            {
               return DXBC::ReadsConstantRow(static_cast<const uint8_t*>(desc->code), desc->code_size, MotionVectorPatches::object_slot, row);
            };
            if (reads(40) && reads(44))
            {
               rows = {40, 44};
            }
            else if (reads(20) && reads(28))
            {
               rows = {20, 28};
            }
         }
      }
      gd->gtao_camera_layouts.emplace(hash, rows);
      return rows;
   }

   // XeGTAO's camera from a depth prepass draw (R32F "1 - z/w" in target 0, the normals in target 1): its vertex constants copied as they
   // are on the GPU (the projection is solved in the shader), and the rows of its matrices there
   static void TakePrepassCamera(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, const CommandListData& cmd_list_data, DeviceData& device_data, uint32_t vertex_shader_hash)
   {
      auto& gd = GetGameDeviceData(device_data);
      const uint2 rows = ClassifyPrepassCamera(device_data, &gd, vertex_shader_hash, cmd_list_data.pipeline_state_original_vertex_shader);
      if (rows.x == 0)
         return;
      com_ptr<ID3D11RenderTargetView> rtvs[2];
      native_device_context->OMGetRenderTargets(2, &rtvs[0], nullptr);
      if (!rtvs[0] || !rtvs[1])
         return;
      D3D11_RENDER_TARGET_VIEW_DESC rtv_desc;
      rtvs[0]->GetDesc(&rtv_desc);
      if (rtv_desc.Format != DXGI_FORMAT_R32_FLOAT)
         return;
      com_ptr<ID3D11Buffer> constants;
      native_device_context->VSGetConstantBuffers(MotionVectorPatches::object_slot, 1, &constants);
      if (!constants)
         return;
      D3D11_BUFFER_DESC desc;
      constants->GetDesc(&desc);
      D3D11_BUFFER_DESC copy_desc = {};
      if (gd.gtao_camera_buffer)
      {
         gd.gtao_camera_buffer->GetDesc(&copy_desc);
      }
      if (copy_desc.ByteWidth != desc.ByteWidth)
      {
         gd.gtao_camera_buffer.reset();
         const D3D11_BUFFER_DESC new_desc = {.ByteWidth = desc.ByteWidth, .Usage = D3D11_USAGE_DEFAULT, .BindFlags = D3D11_BIND_CONSTANT_BUFFER};
         if (FAILED(native_device->CreateBuffer(&new_desc, nullptr, &gd.gtao_camera_buffer)))
            return;
      }
      native_device_context->CopyResource(gd.gtao_camera_buffer.get(), constants.get());
      gd.gtao_camera_rows = rows;
      gd.gtao_camera_taken = true;
   }

   // dgVoodoo's copy of the bloom target's region a chain pass just wrote into the texture the next passes sample, repeated from
   // "bloom_texture" into that texture's "bloom_shadows" entry (made at the same scale on its first copy)
   static void MirrorBloomCopy(reshade::api::command_list* cmd_list, reshade::api::resource source, reshade::api::resource dest, const reshade::api::subresource_box* source_box, const reshade::api::subresource_box* dest_box)
   {
      DeviceData* const device_data = cmd_list->get_device()->get_private_data<DeviceData>();
      if (!g_luma_bloom_enable || !device_data || !device_data->game)
         return;
      auto& gd = GetGameDeviceData(*device_data);
      if (source.handle != gd.bloom_target || !gd.bloom_texture)
         return;
      auto* const dest_texture = reinterpret_cast<ID3D11Texture2D*>(dest.handle);
      D3D11_TEXTURE2D_DESC dest_desc;
      dest_texture->GetDesc(&dest_desc);
      const uint32_t scale = gd.bloom_scale;
      auto shadow = std::ranges::find(gd.bloom_shadows, dest.handle, &BayonettaGameDeviceData::BloomShadow::game_texture);
      if (shadow == gd.bloom_shadows.end())
      {
         D3D11_TEXTURE2D_DESC desc;
         gd.bloom_texture->GetDesc(&desc);
         desc.Width = dest_desc.Width * scale;
         desc.Height = dest_desc.Height * scale;
         desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
         BayonettaGameDeviceData::BloomShadow created{.game_texture = dest.handle};
         ComPtr<ID3D11Device> native_device;
         gd.bloom_texture->GetDevice(native_device.put());
         if (FAILED(native_device->CreateTexture2D(&desc, nullptr, created.texture.put())) || FAILED(native_device->CreateShaderResourceView(created.texture.get(), nullptr, created.srv.put())))
            return;
         shadow = gd.bloom_shadows.insert(gd.bloom_shadows.end(), std::move(created));
      }
      const uint32_t width = source_box ? (source_box->right - source_box->left) : dest_desc.Width;
      const uint32_t height = source_box ? (source_box->bottom - source_box->top) : dest_desc.Height;
      const uint32_t left = source_box ? source_box->left : 0;
      const uint32_t top = source_box ? source_box->top : 0;
      const D3D11_BOX box = {.left = left * scale, .top = top * scale, .front = 0, .right = (left + width) * scale, .bottom = (top + height) * scale, .back = 1};
      auto* const native_device_context = reinterpret_cast<ID3D11DeviceContext*>(cmd_list->get_native());
      native_device_context->CopySubresourceRegion(shadow->texture.get(), 0, dest_box ? dest_box->left * scale : 0, dest_box ? dest_box->top * scale : 0, 0, gd.bloom_texture.get(), 0, &box);
   }

   static bool OnCopyResource(reshade::api::command_list* cmd_list, reshade::api::resource source, reshade::api::resource dest)
   {
      MirrorBloomCopy(cmd_list, source, dest, nullptr, nullptr);
      return false;
   }

   static bool OnCopyTextureRegion(reshade::api::command_list* cmd_list, reshade::api::resource source, uint32_t source_subresource, const reshade::api::subresource_box* source_box, reshade::api::resource dest, uint32_t dest_subresource, const reshade::api::subresource_box* dest_box, reshade::api::filter_mode filter)
   {
      MirrorBloomCopy(cmd_list, source, dest, source_box, dest_box);
      return false;
   }

   // A bloom chain draw (filter05, the blurs, a level copy). Records the bloom target at the chain's first draw (the scene into it);
   // with Luma Bloom, draws into "bloom_texture" instead: the bloom target's size times the scene's height / 720 (level 0 a quarter
   // of the screen, as at the 1280x720 the game was made for), the viewport scaled alike, sampling the scene or a "bloom_shadows"
   // texture, the game's shaders and constants unchanged (their taps are UV fractions: the same footprint, sampled finer). False, for
   // the game's draw, on anything else.
   static bool DrawBloomPass(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, CommandListData& cmd_list_data, DeviceData& device_data, reshade::api::shader_stage stages, bool is_bloom_source, const std::function<void()>& draw, bool* updated_cbuffers)
   {
      auto& gd = GetGameDeviceData(device_data);
      // The copies (the game's most drawn pass: particles too) only matter into a known bloom target
      if (!is_bloom_source && (!g_luma_bloom_enable || gd.bloom_target == 0))
         return false;
      ComPtr<ID3D11RenderTargetView> rtv;
      native_device_context->OMGetRenderTargets(1, rtv.put(), nullptr);
      ComPtr<ID3D11ShaderResourceView> source_srv;
      native_device_context->PSGetShaderResources(0, 1, source_srv.put());
      if (!rtv || !source_srv)
         return false;
      ComPtr<ID3D11Resource> target;
      rtv->GetResource(target.put());
      const uint2 target_size = GetViewTextureSize(rtv.get());
      const uint2 source_size = GetViewTextureSize(source_srv.get());
      const bool from_scene = is_bloom_source && source_size.y > target_size.y;
      if (from_scene)
      {
         gd.bloom_target = uint64_t(target.get());
      }
      if (!g_luma_bloom_enable || uint64_t(target.get()) != gd.bloom_target)
         return false;

      if (from_scene)
      {
         const uint32_t scale = (g_bloom_scale_override > 0) ? uint32_t(g_bloom_scale_override) : (std::max)(1u, uint32_t(std::lround(source_size.y / 720.0)));
         if (scale != gd.bloom_scale || !gd.bloom_rtv || GetViewTextureSize(gd.bloom_rtv.get()) != uint2{target_size.x * scale, target_size.y * scale})
         {
            gd.bloom_rtv.reset();
            gd.bloom_srv.reset();
            gd.bloom_texture.reset();
            gd.bloom_shadows.clear();
            // 16 bit UNORM: the 8 bit target's [0, 1] range (the chain relies on its clamp) without its banding at the finer size
            const CD3D11_TEXTURE2D_DESC desc(DXGI_FORMAT_R16G16B16A16_UNORM, target_size.x * scale, target_size.y * scale, 1, 1, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET);
            const bool created = SUCCEEDED(native_device->CreateTexture2D(&desc, nullptr, gd.bloom_texture.put())) && SUCCEEDED(native_device->CreateRenderTargetView(gd.bloom_texture.get(), nullptr, gd.bloom_rtv.put())) &&
                                 SUCCEEDED(native_device->CreateShaderResourceView(gd.bloom_texture.get(), nullptr, gd.bloom_srv.put()));
            if (!created)
            {
               gd.bloom_rtv.reset();
            }
            gd.bloom_scale = scale;
         }
      }
      if (!gd.bloom_rtv)
         return false;
      ID3D11ShaderResourceView* source = source_srv.get();
      if (!from_scene)
      {
         ComPtr<ID3D11Resource> game_source;
         source_srv->GetResource(game_source.put());
         const auto shadow = std::ranges::find(gd.bloom_shadows, uint64_t(game_source.get()), &BayonettaGameDeviceData::BloomShadow::game_texture);
         // Not copied yet (Luma Bloom just turned on): the game's pass, this frame
         if (shadow == gd.bloom_shadows.end())
            return false;
         source = shadow->srv.get();
      }

      DrawStateStack<DrawStateStackType::FullGraphics> graphics_state;
      graphics_state.Cache(native_device_context, device_data.uav_max_count);
      D3D11_VIEWPORT viewports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
      UINT viewport_count = std::size(viewports);
      native_device_context->RSGetViewports(&viewport_count, viewports);
      // The origin isn't scaled past a quarter texel right of the game's (dgVoodoo's 0.5, D3D9's pixel center): the game's quads
      // start just inside the target's first texel, so scaled exactly they leave the first Luma texels unwritten, black for the
      // blurs' clamped taps (a dark top and left edge). Measured at k 1 to 3 against the game's bloom: no shift, edges matched.
      for (UINT i = 0; i < viewport_count; i++)
      {
         viewports[i].TopLeftX = (std::min)(viewports[i].TopLeftX * gd.bloom_scale, viewports[i].TopLeftX + 0.25f);
         viewports[i].TopLeftY = (std::min)(viewports[i].TopLeftY * gd.bloom_scale, viewports[i].TopLeftY + 0.25f);
         viewports[i].Width *= gd.bloom_scale;
         viewports[i].Height *= gd.bloom_scale;
      }
      native_device_context->RSSetViewports(viewport_count, viewports);
      D3D11_RECT scissors[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
      UINT scissor_count = std::size(scissors);
      native_device_context->RSGetScissorRects(&scissor_count, scissors);
      for (UINT i = 0; i < scissor_count; i++)
      {
         scissors[i].left *= gd.bloom_scale;
         scissors[i].top *= gd.bloom_scale;
         scissors[i].right *= gd.bloom_scale;
         scissors[i].bottom *= gd.bloom_scale;
      }
      native_device_context->RSSetScissorRects(scissor_count, scissors);
      ID3D11RenderTargetView* const bloom_rtv = gd.bloom_rtv.get();
      native_device_context->OMSetRenderTargets(1, &bloom_rtv, nullptr);
      native_device_context->PSSetShaderResources(0, 1, &source);
      // filter05 (BloomSource_0x8B6A8D61) averages the whole footprint of each texel
      SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, stages, LumaConstantBufferType::LumaSettings);
      SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, stages, LumaConstantBufferType::LumaData, is_bloom_source ? 1 : 0);
      *updated_cbuffers = true;
      draw();
      graphics_state.Restore(native_device_context);
      return true;
   }

   // XeGTAO in place of the game's SSAO, from the SSAO draw's inputs (t1 the prepass' view space normals, t2 its "1 - z/w") and the
   // prepass' camera ("TakePrepassCamera"): 4 compute passes into "gtao_output", then the draw with "Bayonetta XeGTAO Apply PS" in place
   // of the game's (scene * visibility). False, for the game's SSAO to draw, without the shaders, a camera, or on a failed allocation.
   static bool RunXeGTAO(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, DeviceData& device_data, const std::function<void()>& draw)
   {
      auto& gd = GetGameDeviceData(device_data);
      if (!gd.gtao_camera_buffer)
         return false;
      auto* const prefilter_cs = FindShader(device_data.native_compute_shaders, CompileTimeStringHash("Bayonetta XeGTAO Prefilter Depths CS"));
      auto* const main_cs = FindShader(device_data.native_compute_shaders, CompileTimeStringHash("Bayonetta XeGTAO Main Pass CS"));
      auto* const denoise_1_cs = FindShader(device_data.native_compute_shaders, CompileTimeStringHash("Bayonetta XeGTAO Denoise Pass 1 CS"));
      auto* const denoise_2_cs = FindShader(device_data.native_compute_shaders, CompileTimeStringHash("Bayonetta XeGTAO Denoise Pass 2 CS"));
      auto* const apply_ps = FindShader(device_data.native_pixel_shaders, CompileTimeStringHash("Bayonetta XeGTAO Apply PS"));
      if (!prefilter_cs || !main_cs || !denoise_1_cs || !denoise_2_cs || !apply_ps)
         return false;
      com_ptr<ID3D11ShaderResourceView> inputs[2]; // t1, t2
      native_device_context->PSGetShaderResources(1, 2, &inputs[0]);
      ID3D11ShaderResourceView* const normals = inputs[0].get();
      ID3D11ShaderResourceView* const depth = inputs[1].get();
      if (!normals || !depth)
         return false;
      const uint2 size = GetViewTextureSize(depth);
      if (size.x == 0 || size.y == 0)
         return false;

      if (size != gd.gtao_size)
      {
         gd.ReleaseGTAOScratch();
         const CD3D11_TEXTURE2D_DESC depth_desc(DXGI_FORMAT_R32_FLOAT, size.x, size.y, 1, 5 /* XE_GTAO_DEPTH_MIP_LEVELS */, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS);
         bool created = SUCCEEDED(native_device->CreateTexture2D(&depth_desc, nullptr, gd.gtao_depth_mips.put()));
         for (UINT mip = 0; created && mip < 5; mip++)
         {
            const CD3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc(D3D11_UAV_DIMENSION_TEXTURE2D, depth_desc.Format, mip);
            created = SUCCEEDED(native_device->CreateUnorderedAccessView(gd.gtao_depth_mips.get(), &uav_desc, gd.gtao_depth_mip_uavs[mip].put()));
         }
         created = created && SUCCEEDED(native_device->CreateShaderResourceView(gd.gtao_depth_mips.get(), nullptr, gd.gtao_depth_mips_srv.put()));
         const CD3D11_TEXTURE2D_DESC working_desc(DXGI_FORMAT_R8G8_UNORM, size.x, size.y, 1, 1, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS);
         for (int i = 0; created && i < 2; i++)
         {
            created = SUCCEEDED(native_device->CreateTexture2D(&working_desc, nullptr, gd.gtao_working[i].put())) && SUCCEEDED(native_device->CreateUnorderedAccessView(gd.gtao_working[i].get(), nullptr, gd.gtao_working_uavs[i].put())) &&
                      SUCCEEDED(native_device->CreateShaderResourceView(gd.gtao_working[i].get(), nullptr, gd.gtao_working_srvs[i].put()));
         }
         const CD3D11_TEXTURE2D_DESC output_desc(DXGI_FORMAT_R8_UNORM, size.x, size.y, 1, 1, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS);
         created = created && SUCCEEDED(native_device->CreateTexture2D(&output_desc, nullptr, gd.gtao_output.put())) && SUCCEEDED(native_device->CreateUnorderedAccessView(gd.gtao_output.get(), nullptr, gd.gtao_output_uav.put())) &&
                   SUCCEEDED(native_device->CreateShaderResourceView(gd.gtao_output.get(), nullptr, gd.gtao_output_srv.put()));
         if (!created)
         {
            gd.ReleaseGTAOScratch();
         }
         // Kept on a failure too: the game's SSAO draws at this size, without an allocation attempt per frame (32-bit address space)
         gd.gtao_size = size;
      }
      if (!gd.gtao_output_srv)
         return false;

#if DEVELOPMENT
      const float debug_view = float(g_gtao_debug_view);
#else
      const float debug_view = 0.f;
#endif
      const CB::GTAOKnobs knobs = {
         .final_value_power = g_gtao_final_value_power,
         .depth_scale = g_gtao_depth_scale,
         .radius_override = g_gtao_radius_override,
         .debug_view = debug_view,
         .viewport_pixel_size = {1.f / float(size.x), 1.f / float(size.y)},
         .noise_index = (gd.sr_active ? float(cb_luma_global_settings.FrameIndex % 64) : 0.f),
         .camera_affine_row = float(gd.gtao_camera_rows.x),
         .camera_matrix_row = float(gd.gtao_camera_rows.y),
      };
      if (!PatchedDraws::WriteDynamicConstants(native_device, native_device_context, std::addressof(gd.gtao_knobs_buffer), &knobs, sizeof(knobs)))
         return false;
      ID3D11Buffer* const knobs_buffer = gd.gtao_knobs_buffer.get();

      {
         DrawStateStack<DrawStateStackType::Compute> compute_state;
         compute_state.Cache(native_device_context, device_data.uav_max_count);
         ID3D11Buffer* const camera_buffer = gd.gtao_camera_buffer.get();
         native_device_context->CSSetConstantBuffers(MotionVectorPatches::object_slot, 1, &camera_buffer);
         native_device_context->CSSetConstantBuffers(GTAO_KNOBS_CB_SLOT, 1, &knobs_buffer);
         ID3D11SamplerState* const point_sampler = device_data.sampler_state_point.get();
         native_device_context->CSSetSamplers(0, 1, &point_sampler);
         // Each pass binds its destination UAVs before its source SRVs: D3D11 otherwise nulls an SRV that still aliases the previous
         // pass' bound UAV
         ID3D11ShaderResourceView* const null_srvs[2] = {};
         ID3D11UnorderedAccessView* const null_uavs[5] = {};
         const auto pass = [&](ID3D11ComputeShader* shader, UINT uav_count, ID3D11UnorderedAccessView* const* uavs, ID3D11ShaderResourceView* const(&srvs)[2], UINT groups_x, UINT groups_y)
         {
            native_device_context->CSSetShaderResources(0, 2, null_srvs);
            native_device_context->CSSetUnorderedAccessViews(0, uav_count, uavs, nullptr);
            native_device_context->CSSetShaderResources(0, 2, srvs);
            native_device_context->CSSetShader(shader, nullptr, 0);
            native_device_context->Dispatch(groups_x, groups_y, 1);
            native_device_context->CSSetUnorderedAccessViews(0, uav_count, null_uavs, nullptr);
         };
         ID3D11UnorderedAccessView* const mip_uavs[5] = {gd.gtao_depth_mip_uavs[0].get(), gd.gtao_depth_mip_uavs[1].get(), gd.gtao_depth_mip_uavs[2].get(), gd.gtao_depth_mip_uavs[3].get(), gd.gtao_depth_mip_uavs[4].get()};
         ID3D11UnorderedAccessView* const working_uavs[2] = {gd.gtao_working_uavs[0].get(), gd.gtao_working_uavs[1].get()};
         ID3D11UnorderedAccessView* const output_uav = gd.gtao_output_uav.get();
         pass(prefilter_cs, 5, mip_uavs, {depth, nullptr}, (size.x + 15) / 16, (size.y + 15) / 16);
         pass(main_cs, 1, &working_uavs[0], {gd.gtao_depth_mips_srv.get(), normals}, (size.x + 7) / 8, (size.y + 7) / 8);
         pass(denoise_1_cs, 1, &working_uavs[1], {gd.gtao_working_srvs[0].get(), nullptr}, (size.x + 15) / 16, (size.y + 7) / 8);
         pass(denoise_2_cs, 1, &output_uav, {gd.gtao_working_srvs[1].get(), nullptr}, (size.x + 15) / 16, (size.y + 7) / 8);
#if DEVELOPMENT
         QueueInjectedPassTrace(native_device_context, "Luma XeGTAO", output_uav);
#endif
         compute_state.Restore(native_device_context);
      }

      // The game's draw (its vertex shader, target, t0 scene and t2 depth) with the apply shader, the visibility on t3 and the knobs
      com_ptr<ID3D11PixelShader> game_shader;
      native_device_context->PSGetShader(&game_shader, nullptr, nullptr);
      com_ptr<ID3D11ShaderResourceView> game_t3;
      native_device_context->PSGetShaderResources(3, 1, &game_t3);
      com_ptr<ID3D11Buffer> game_knobs_slot;
      native_device_context->PSGetConstantBuffers(GTAO_KNOBS_CB_SLOT, 1, &game_knobs_slot);
      ID3D11ShaderResourceView* const output_srv = gd.gtao_output_srv.get();
      native_device_context->PSSetShader(apply_ps, nullptr, 0);
      native_device_context->PSSetShaderResources(3, 1, &output_srv);
      native_device_context->PSSetConstantBuffers(GTAO_KNOBS_CB_SLOT, 1, &knobs_buffer);
      draw();
      native_device_context->PSSetShader(game_shader.get(), nullptr, 0);
      ID3D11ShaderResourceView* const t3 = game_t3.get();
      native_device_context->PSSetShaderResources(3, 1, &t3);
      ID3D11Buffer* const knobs_slot = game_knobs_slot.get();
      native_device_context->PSSetConstantBuffers(GTAO_KNOBS_CB_SLOT, 1, &knobs_slot);
      return true;
   }

   DrawOrDispatchOverrideType OnDrawOrDispatch(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, CommandListData& cmd_list_data, DeviceData& device_data, reshade::api::shader_stage stages, const ShaderHashesList<OneShaderPerPipeline>& original_shader_hashes, bool is_custom_pass, bool& updated_cbuffers, std::function<void()>* original_draw_dispatch_func) override
   {
      auto& game_device_data = GetGameDeviceData(device_data);
      if (original_draw_dispatch_func == nullptr || !*original_draw_dispatch_func)
      {
         return DrawOrDispatchOverrideType::None;
      }
      const bool is_immediate = native_device_context->GetType() == D3D11_DEVICE_CONTEXT_IMMEDIATE;
      // The bound render target 0's view if it's the canvas (dgVoodoo's D3D9 back buffer), else null
      const auto bound_canvas_rtv = [&]
      {
         ComPtr<ID3D11RenderTargetView> rtv;
         native_device_context->OMGetRenderTargets(1, rtv.put(), nullptr);
         ComPtr<ID3D11Resource> rt;
         if (rtv)
         {
            rtv->GetResource(rt.put());
         }
         if (!rt || uint64_t(rt.get()) != game_device_data.canvas)
         {
            rtv.reset();
         }
         return rtv;
      };
      // Hide UI: cancel the game's draws into the canvas after this frame's scene copy (the HUD, menus and 2D effects), not the movies.
      // An original pixel shader hash is what separates a game draw.
      if (g_hide_ui && game_device_data.has_drawn_reset_render && original_shader_hashes.pixel_shaders[0] != SHADER_HASH_NONE && !original_shader_hashes.Contains(shader_hashes_Video) && bound_canvas_rtv())
      {
         return DrawOrDispatchOverrideType::Replaced;
      }
      // Draws the replaced pass with the Luma cbuffers, then copies the canvas it drew on into "scene_texture": the floor the
      // present blit clamps the later UI to
      auto draw_and_snapshot_canvas = [&](ID3D11RenderTargetView* rtv, ID3D11Resource* canvas, uint32_t custom_data)
      {
         SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, stages, LumaConstantBufferType::LumaSettings);
         SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, stages, LumaConstantBufferType::LumaData, custom_data);
         updated_cbuffers = true;
         (*original_draw_dispatch_func)();
         D3D11_TEXTURE2D_DESC canvas_desc;
         static_cast<ID3D11Texture2D*>(canvas)->GetDesc(&canvas_desc);
         D3D11_TEXTURE2D_DESC scene_desc = {};
         if (game_device_data.scene_texture)
         {
            game_device_data.scene_texture->GetDesc(&scene_desc);
         }
         if (scene_desc.Width != canvas_desc.Width || scene_desc.Height != canvas_desc.Height || scene_desc.Format != canvas_desc.Format)
         {
            game_device_data.scene_srv.reset();
            game_device_data.scene_texture.reset();
            canvas_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            canvas_desc.MiscFlags = 0;
            if (SUCCEEDED(native_device->CreateTexture2D(&canvas_desc, nullptr, game_device_data.scene_texture.put())))
            {
               // A typeless canvas needs the render target's view format
               D3D11_RENDER_TARGET_VIEW_DESC rtv_desc;
               rtv->GetDesc(&rtv_desc);
               const D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {.Format = rtv_desc.Format, .ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D, .Texture2D = {.MostDetailedMip = 0, .MipLevels = 1}};
               native_device->CreateShaderResourceView(game_device_data.scene_texture.get(), &srv_desc, game_device_data.scene_srv.put());
            }
         }
         if (game_device_data.scene_srv)
         {
            native_device_context->CopyResource(game_device_data.scene_texture.get(), canvas);
            game_device_data.has_copied_scene_to_canvas = true;
            // SMAA then RCAS on the scene copy (not on the movies), each reading the snapshot and writing the canvas, then the snapshot
            // again: the UI clamp floor must be the final canvas. An upscaler that drew this frame antialiased it: SMAA stays out, RCAS
            // stays on.
            if (custom_data == 1)
            {
               game_device_data.has_drawn_reset_render = true;
               bool canvas_changed = false;
               D3D11_VIEWPORT scene_rect;
               UINT viewport_count = 1;
               native_device_context->RSGetViewports(&viewport_count, &scene_rect);
               if (g_smaa_enable && !device_data.has_drawn_sr && viewport_count == 1)
               {
                  DrawSMAAOnCanvas(native_device, native_device_context, device_data, rtv, scene_rect);
                  canvas_changed = true;
               }
               if (g_rcas_sharpness > 0.f && PrepareRCAS(native_device, device_data))
               {
                  if (canvas_changed)
                  {
                     native_device_context->CopyResource(game_device_data.scene_texture.get(), canvas);
                  }
                  DrawRCAS(native_device_context, device_data, game_device_data.scene_srv.get(), rtv, g_rcas_sharpness);
                  canvas_changed = true;
               }
               if (canvas_changed)
               {
                  native_device_context->CopyResource(game_device_data.scene_texture.get(), canvas);
               }
            }
         }
         return DrawOrDispatchOverrideType::Replaced;
      };

      // The pixel kernels' target size
      if (original_shader_hashes.Contains(shader_hashes_PixelKernels) || original_shader_hashes.Contains(shader_hashes_ShadowReceivers))
      {
         BindPixelKernelTargetSize(native_device, native_device_context, &game_device_data);
      }
      // XeGTAO's camera, from the frame's first depth prepass draw with a known layout. Before the motion vector draws, which replace the
      // prepass' draws when they jitter.
      if (g_gtao_enable && is_immediate && !game_device_data.gtao_camera_taken && (stages & reshade::api::shader_stage::vertex) == reshade::api::shader_stage::vertex)
      {
         TakePrepassCamera(native_device, native_device_context, cmd_list_data, device_data, uint32_t(original_shader_hashes.vertex_shaders[0]));
      }

      // DLAA/FSR/Luma TAA: the scene's draws with motion vectors or jitter (see "DrawWithMotionVectors"), and the fill and upscaler at
      // its first post pass
      if (game_device_data.mv_active && is_immediate)
      {
         // A game draw with a vertex shader, which the patched shaders can redo: replaced ones too (the particles translate to copytex,
         // replaced by "Copy_0x91A6C437"; their patched versions come from the game's code, see "GetPatchedShader")
         const bool game_mesh_draw = (stages & reshade::api::shader_stage::vertex) == reshade::api::shader_stage::vertex && original_shader_hashes.pixel_shaders[0] != SHADER_HASH_NONE;
         if (!game_device_data.mv_scene_done && original_shader_hashes.Contains(shader_hashes_ScenePost))
         {
            if (game_device_data.mv_scene_open)
            {
#if DEVELOPMENT
               game_device_data.mv_stats.ended_by = uint32_t(original_shader_hashes.pixel_shaders[0]);
               game_device_data.mv_stats.prepass_drawn = game_device_data.prepass_drawn;
#endif
               // The scene's last copy, if the pass reads it
               game_device_data.mv_scene_copy.reset();
               if (game_device_data.mv_scene_last_copy)
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
                     if (resource && resource == game_device_data.mv_scene_last_copy)
                     {
                        game_device_data.mv_scene_copy = resource;
                        break;
                     }
                  }
               }
               EndScene(native_device, native_device_context, device_data);
            }
         }
         // Until the scene opens only its targets matter (the shadow maps come first)
         else if (!game_device_data.mv_scene_done && !game_device_data.mv_scene_open && game_mesh_draw)
         {
            com_ptr<ID3D11RenderTargetView> rtvs[2];
            com_ptr<ID3D11DepthStencilView> scene_dsv;
            native_device_context->OMGetRenderTargets(2, &rtvs[0], &scene_dsv);
            if (rtvs[0] && scene_dsv && (rtvs[0].get() != game_device_data.open_refused_rtv || scene_dsv.get() != game_device_data.open_refused_dsv))
            {
               OpenScene(native_device, native_device_context, cmd_list_data, device_data, original_shader_hashes.vertex_shaders[0], rtvs[0].get(), rtvs[1] != nullptr, scene_dsv.get());
            }
         }
         if (game_device_data.mv_scene_open && game_mesh_draw)
         {
            com_ptr<ID3D11RenderTargetView> rtvs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
            com_ptr<ID3D11DepthStencilView> dsv;
            native_device_context->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, &rtvs[0], &dsv);
            // The depth prepass writes 1 - z/w to an R32F target next to the normals' (the fill's depth). Classified once per target.
            if (rtvs[0] && rtvs[1])
            {
               if (rtvs[0].get() != game_device_data.prepass_rtv)
               {
                  game_device_data.prepass_rtv = rtvs[0].get();
                  game_device_data.prepass_is_depth = false;
                  D3D11_RENDER_TARGET_VIEW_DESC rtv_desc;
                  rtvs[0]->GetDesc(&rtv_desc);
                  if (rtv_desc.Format == DXGI_FORMAT_R32_FLOAT)
                  {
                     com_ptr<ID3D11Resource> resource;
                     rtvs[0]->GetResource(&resource);
                     com_ptr<ID3D11Texture2D> texture;
                     D3D11_TEXTURE2D_DESC desc = {};
                     if (SUCCEEDED(resource->QueryInterface(&texture)))
                     {
                        texture->GetDesc(&desc);
                     }
                     if (desc.SampleDesc.Count == 1 && (desc.BindFlags & D3D11_BIND_SHADER_RESOURCE) != 0 && uint2{desc.Width, desc.Height} == game_device_data.mv_scene_size)
                     {
                        game_device_data.prepass_is_depth = true;
                        if (resource != game_device_data.prepass_depth)
                        {
                           game_device_data.prepass_depth = resource;
                           game_device_data.prepass_depth_srv.reset();
                           // dgVoodoo's targets are single-slice arrays: a plain 2D view of the first slice
                           const CD3D11_SHADER_RESOURCE_VIEW_DESC srv_desc(D3D11_SRV_DIMENSION_TEXTURE2D, DXGI_FORMAT_R32_FLOAT);
                           native_device->CreateShaderResourceView(resource.get(), &srv_desc, &game_device_data.prepass_depth_srv);
                        }
                     }
                  }
               }
               game_device_data.prepass_drawn |= game_device_data.prepass_is_depth;
            }
            const bool motion_vectors = DrawWithMotionVectors(native_device, native_device_context, cmd_list_data, device_data, original_shader_hashes, *original_draw_dispatch_func, rtvs, dsv.get());
            // An alpha blending state blends the motion vector target by the alpha written there: a draw that doesn't write it (refused
            // above) must not have it bound
            if (!motion_vectors && rtvs[MotionVectorPatches::target_slot] && rtvs[MotionVectorPatches::target_slot] == game_device_data.mv_rtv)
            {
               ClassifyBoundBlend(native_device_context, &game_device_data);
               if (game_device_data.mv_alpha_blended)
               {
                  rtvs[MotionVectorPatches::target_slot].reset();
                  ID3D11RenderTargetView* targets[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
                  for (UINT slot = 0; slot < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; slot++)
                  {
                     targets[slot] = rtvs[slot].get();
                  }
                  native_device_context->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, targets, dsv.get());
               }
            }
            const bool jitter = !motion_vectors && DrawWithJitter(native_device, native_device_context, cmd_list_data, device_data, original_shader_hashes, *original_draw_dispatch_func, rtvs, dsv.get());
#if DEVELOPMENT
            Mcp::Annotate(cmd_list_data, motion_vectors ? "mv" : (jitter ? "jitter" : "unpatched"));
            if (const MotionVectorReject reject = std::exchange(game_device_data.mv_draw_reject, REJECT_COUNT); reject != REJECT_COUNT)
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
         PatchedDraws::RestoreGameShader(native_device_context, &game_device_data.mv_bound_vertex_shader);
         PatchedDraws::RestoreGameShader(native_device_context, &game_device_data.mv_bound_pixel_shader);
      }

      const bool is_bloom_source = original_shader_hashes.Contains(shader_hashes_BloomSource);
      if (is_immediate && (is_bloom_source || original_shader_hashes.Contains(shader_hashes_BloomBlur) || original_shader_hashes.Contains(shader_hashes_Copy)) &&
          DrawBloomPass(native_device, native_device_context, cmd_list_data, device_data, stages, is_bloom_source, *original_draw_dispatch_func, &updated_cbuffers))
         return DrawOrDispatchOverrideType::Replaced;

      if (original_shader_hashes.Contains(shader_hashes_Grade))
      {
         device_data.has_drawn_main_post_processing = true;
      }
      else if (original_shader_hashes.Contains(shader_hashes_SSAO))
      {
         game_device_data.depth_srv.reset();
         native_device_context->PSGetShaderResources(2, 1, game_device_data.depth_srv.put());
         if (g_gtao_enable && is_immediate && RunXeGTAO(native_device, native_device_context, device_data, *original_draw_dispatch_func))
            return DrawOrDispatchOverrideType::Replaced;
      }
      else if (original_shader_hashes.Contains(shader_hashes_DgVoodooPresent))
      {
         ComPtr<ID3D11ShaderResourceView> srv;
         native_device_context->PSGetShaderResources(0, 1, srv.put());
         if (srv)
         {
            ComPtr<ID3D11Resource> source;
            srv->GetResource(source.put());
            game_device_data.present_source = uint64_t(source.get());
         }
         SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, stages, LumaConstantBufferType::LumaSettings);
         SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, stages, LumaConstantBufferType::LumaData);
         updated_cbuffers = true;
         ID3D11ShaderResourceView* const scene_srv = (game_device_data.has_copied_scene_to_canvas ? game_device_data.scene_srv.get() : nullptr);
         native_device_context->PSSetShaderResources(1, 1, &scene_srv);
         (*original_draw_dispatch_func)();
         ID3D11ShaderResourceView* const null_srv = nullptr;
         native_device_context->PSSetShaderResources(1, 1, &null_srv);
         return DrawOrDispatchOverrideType::Replaced;
      }
      else if (is_custom_pass && (original_shader_hashes.Contains(shader_hashes_Copy) || original_shader_hashes.Contains(shader_hashes_Video)))
      {
         const bool is_video = original_shader_hashes.Contains(shader_hashes_Video);
         // The bloom composite (FUN_0044bc90): the bloom target blended onto the scene, its mode 1 SRCALPHA/INVSRCALPHA or 2
         // SRCALPHA/ONE (mode 0, opaque, is its copies between levels). The shader scales the source alpha by "Bloom Intensity"
         // (LumaData.CustomData1 2 when it adds, 3 when it lerps); with Luma Bloom it reads "bloom_texture" (CustomData2 1: a B-spline
         // magnification).
         ComPtr<ID3D11ShaderResourceView> source_srv;
         if (!is_video && game_device_data.bloom_target != 0)
         {
            native_device_context->PSGetShaderResources(0, 1, source_srv.put());
         }
         ComPtr<ID3D11Resource> source;
         if (source_srv)
         {
            source_srv->GetResource(source.put());
         }
         if (source && uint64_t(source.get()) == game_device_data.bloom_target)
         {
            ComPtr<ID3D11BlendState> blend_state;
            native_device_context->OMGetBlendState(blend_state.put(), nullptr, nullptr);
            D3D11_BLEND_DESC blend_desc = {};
            if (blend_state)
            {
               blend_state->GetDesc(&blend_desc);
            }
            const D3D11_RENDER_TARGET_BLEND_DESC& blend = blend_desc.RenderTarget[0];
            if (blend.BlendEnable && blend.SrcBlend == D3D11_BLEND_SRC_ALPHA && (blend.DestBlend == D3D11_BLEND_ONE || blend.DestBlend == D3D11_BLEND_INV_SRC_ALPHA))
            {
               ID3D11ShaderResourceView* const luma_bloom_srv = (g_luma_bloom_enable ? game_device_data.bloom_srv.get() : nullptr);
               SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, stages, LumaConstantBufferType::LumaSettings);
               SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, stages, LumaConstantBufferType::LumaData, (blend.DestBlend == D3D11_BLEND_ONE ? 2 : 3), luma_bloom_srv ? 1 : 0);
               updated_cbuffers = true;
               if (!luma_bloom_srv)
                  return DrawOrDispatchOverrideType::None;
               native_device_context->PSSetShaderResources(0, 1, &luma_bloom_srv);
               (*original_draw_dispatch_func)();
               ID3D11ShaderResourceView* const game_srv = source_srv.get();
               native_device_context->PSSetShaderResources(0, 1, &game_srv);
               return DrawOrDispatchOverrideType::Replaced;
            }
         }
         // ResetRender is the frame's first opaque copy into the canvas (the bloom composite also copies, additively, into the scene)
         if (!is_video && game_device_data.has_copied_scene_to_canvas)
         {
            return DrawOrDispatchOverrideType::None;
         }
         const ComPtr<ID3D11RenderTargetView> rtv = bound_canvas_rtv();
         if (!rtv)
         {
            return DrawOrDispatchOverrideType::None;
         }
         auto* const canvas = reinterpret_cast<ID3D11Resource*>(uint64_t(game_device_data.canvas));
         // The movies' AutoHDR goes above 1, which the present blit's clamp must keep
         if (is_video)
         {
            return draw_and_snapshot_canvas(rtv.get(), canvas, 0);
         }
         ComPtr<ID3D11BlendState> blend_state;
         native_device_context->OMGetBlendState(blend_state.put(), nullptr, nullptr);
         D3D11_BLEND_DESC blend_desc = {};
         if (blend_state)
         {
            blend_state->GetDesc(&blend_desc);
         }
         if (!blend_desc.RenderTarget[0].BlendEnable)
         {
            return draw_and_snapshot_canvas(rtv.get(), canvas, 1);
         }
      }
      // MSAA (4x and up, as Mirror's Edge: 2x gives a coarser edge than the alpha test): the alpha tested materials' draws into the
      // multisampled scene with their alpha to coverage version, the game's shader put back after. The coverage version keeps the
      // in place "PatchShaderBytecodeSync" clamp of the float target 0 shaders
      else if (g_msaa_resolve_enable && g_msaa_coverage_enable && game_device_data.msaa_active && !is_custom_pass && original_shader_hashes.pixel_shaders[0] != SHADER_HASH_NONE)
      {
         if (ID3D11PixelShader* const coverage_shader = GetPatchedShader<ID3D11PixelShader>(native_device, device_data, uint32_t(original_shader_hashes.pixel_shaders[0]), cmd_list_data.pipeline_state_original_pixel_shader, PixelShaderPatch::COVERAGE).shader.get())
         {
            ComPtr<ID3D11RenderTargetView> rtv;
            native_device_context->OMGetRenderTargets(1, rtv.put(), nullptr);
            ComPtr<ID3D11Resource> rt;
            ComPtr<ID3D11Texture2D> rt_texture;
            D3D11_TEXTURE2D_DESC rt_desc = {};
            if (rtv)
            {
               rtv->GetResource(rt.put());
               if (SUCCEEDED(rt->QueryInterface(rt_texture.put())))
               {
                  rt_texture->GetDesc(&rt_desc);
               }
            }
            if (rt_desc.SampleDesc.Count >= 4)
            {
               com_ptr<ID3D11PixelShader> game_shader;
               native_device_context->PSGetShader(&game_shader, nullptr, nullptr);
               native_device_context->PSSetShader(coverage_shader, nullptr, 0);
               (*original_draw_dispatch_func)();
               native_device_context->PSSetShader(game_shader.get(), nullptr, 0);
#if DEVELOPMENT
               game_device_data.aa_stats.msaa_coverage++;
#endif
               return DrawOrDispatchOverrideType::Replaced;
            }
         }
      }
      return DrawOrDispatchOverrideType::None;
   }

   void OnPresent(ID3D11Device* native_device, DeviceData& device_data) override
   {
      auto& game_device_data = GetGameDeviceData(device_data);
      game_device_data.has_copied_scene_to_canvas = false;
      game_device_data.has_drawn_reset_render = false;
      const bool msaa_was_active = game_device_data.msaa_active;
      game_device_data.msaa_active = game_device_data.msaa_resolved.exchange(false);
      if (game_device_data.msaa_active && !msaa_was_active)
      {
         reshade::log::message(reshade::log::level::warning, "[Bayonetta] The game's MSAA is on: DLAA, FSR and SMAA T2x stay off until Anti-Aliasing is off in the game's options");
      }
#if DEVELOPMENT
      game_device_data.aa_last_stats = std::exchange(game_device_data.aa_stats, {});
#endif
      game_device_data.depth_srv.reset(); // Don't hold the game's view across frames (a resize recreates it)
      game_device_data.gtao_camera_taken = false;
      // Turned off, XeGTAO gives its address space back (recreated at its next use)
      if (!g_gtao_enable && game_device_data.gtao_size != uint2{})
      {
         game_device_data.ReleaseGTAOScratch();
      }

      // DLSS/FSR/Luma TAA: the history restarts after any frame it didn't draw (menus, loading, just picked); the selection and the
      // motion vector state are fixed here for the next frame (see "sr_active")
      game_device_data.mv_jitter_allowed = device_data.has_drawn_sr;
      // Not on a multisampled scene (the motion vector draws refuse it): with MSAA the motion vector path stays off
      game_device_data.sr_active = LatchSRFrame(device_data) && !game_device_data.msaa_active;
      // SMAA T2x runs on the upscalers' motion vectors and gives way to an upscaler. Turned off, its frames go, and the motion vectors
      // go as when no upscaler is picked.
      game_device_data.t2x_phase = -1;
      game_device_data.mv_filled = false;
      const bool t2x_was_active = game_device_data.t2x_active;
      game_device_data.t2x_active = g_smaa_enable && g_smaa_t2x && !game_device_data.sr_active && !game_device_data.msaa_active;
      if (t2x_was_active && !game_device_data.t2x_active)
      {
         game_device_data.ReleaseT2xFrames();
         device_data.release_sr_resources = true;
      }
      if (game_device_data.t2x_frames[0] && cb_luma_global_settings.FrameIndex - game_device_data.t2x_frame > IDLE_RELEASE_FRAMES)
      {
         game_device_data.ReleaseT2xFrames();
      }
      const bool mv_was_active = game_device_data.mv_active.exchange((game_device_data.sr_active || game_device_data.t2x_active || g_mv_enable) && !game_device_data.msaa_active);
      if (game_device_data.mv_active && !mv_was_active)
      {
         game_device_data.mv_constants_generation++;
      }
      // SMAA turned off or replaced by an upscaler that drew: its ~80 MB of 4K intermediates go
      if ((!g_smaa_enable || game_device_data.mv_jitter_allowed) && game_device_data.smaa_resources_live)
      {
         ReleaseSMAAIntermediates(device_data);
         game_device_data.smaa_predication_srv.reset();
         game_device_data.smaa_predication_uav.reset();
         game_device_data.smaa_predication.reset();
         game_device_data.smaa_resources_live = false;
      }
      // None picked: Core stopped the SR bridge's helper ("ReleaseResources"), our upscaler inputs and output go too. Recreated when an
      // upscaler is picked again (the helper takes seconds to start).
      if (device_data.sr_type == SR::Type::None && device_data.release_sr_resources.exchange(false))
      {
         game_device_data.sr_output_srv.reset();
         if (!g_mv_enable && !game_device_data.t2x_active)
         {
            const std::unique_lock lock(game_device_data.mv_mutex);
            game_device_data.ReleaseMotionVectorTargets();
            // The game's scene, depth, prepass and scene copy (taken again at the next scene), so a resize after None doesn't keep the
            // old ones alive; and the vc4 copies the object tables and the cameras hold
            game_device_data.mv_depth.reset();
            game_device_data.mv_scene_color.reset();
            game_device_data.mv_scene_rtv.reset();
            game_device_data.mv_scene_copy.reset();
            game_device_data.mv_scene_last_copy.reset();
            game_device_data.mv_scene_copy_rtv.reset();
            game_device_data.prepass_depth.reset();
            game_device_data.prepass_depth_srv.reset();
            game_device_data.mv_scene_size = {};
            game_device_data.mv_objects.clear();
            game_device_data.mv_previous_objects.clear();
            game_device_data.mv_camera.reset();
            game_device_data.mv_previous_camera.reset();
            game_device_data.mv_fill_buffer.reset();
            game_device_data.mv_jitter_buffer.reset();
            game_device_data.mv_previous_constants = {}; // Its 4 MiB dynamic ring is address space too
            game_device_data.mv_constants_pool.clear();
            game_device_data.mv_constants_pool_free.clear();
         }
      }
      {
         // The pooled vc4 copies only the pool holds (superseded, no object or camera keeps them) are free for the next ones, as many as
         // the last frame asked for: frames without a scene (loading, videos, menus) would keep their peak otherwise. Without motion
         // vectors, none.
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
      // The reactive masks go once the fill stopped writing them (DLSS, the masks off, SMAA)
      if (game_device_data.mv_reactive_target && cb_luma_global_settings.FrameIndex - game_device_data.sr_reactive_frame > IDLE_RELEASE_FRAMES)
      {
         const std::unique_lock lock(game_device_data.mv_mutex);
         game_device_data.ReleaseReactiveMasks();
      }
      // A scene no post pass ended ends here: its jitter must not reach the next frame's draws before its first mesh
      game_device_data.mv_scene_open = false;
      game_device_data.mv_scene_done = false;
      game_device_data.mv_frame_ended = true;
      game_device_data.mv_fill_pending = false;
      game_device_data.open_refused_rtv = nullptr;
      game_device_data.open_refused_dsv = nullptr;
      // -1 at native resolution once the upscaler draws (Core biases the game's samplers at bias 0, the trilinear ones upgraded to
      // AF16x, see "DllMain"); none while the bridge's helper starts (SMAA on the unjittered scene)
      SetTextureMipLodBias(nullptr, device_data, ((game_device_data.sr_active && game_device_data.mv_jitter_allowed) ? SR::GetMipLODBias(device_data.output_resolution.y, device_data.output_resolution.y) : 0.f));
#if DEVELOPMENT
      game_device_data.mv_last_stats = std::exchange(game_device_data.mv_stats, {});
      // "MV Debug View": Core's debug draw of the target, absolute values in pixels
      {
         const std::shared_lock lock(game_device_data.mv_mutex);
         if (g_mv_debug_view && game_device_data.mv_texture)
         {
            D3D11_TEXTURE2D_DESC desc;
            game_device_data.mv_texture->GetDesc(&desc);
            debug_draw_auto_clear_texture = false;
            debug_draw_options |= (uint32_t)DebugDrawTextureOptionsMask::Abs | (uint32_t)DebugDrawTextureOptionsMask::UVToPixelSpace;
            device_data.debug_draw_texture = game_device_data.mv_texture.get();
            device_data.debug_draw_texture_format = desc.Format;
            device_data.debug_draw_texture_size = {desc.Width, desc.Height, 1, 1};
         }
         else if (g_sr_reactive_debug_view && game_device_data.mv_reactive)
         {
            debug_draw_auto_clear_texture = false;
            debug_draw_options |= (uint32_t)DebugDrawTextureOptionsMask::RedOnly;
            debug_draw_options &= ~((uint32_t)DebugDrawTextureOptionsMask::Abs | (uint32_t)DebugDrawTextureOptionsMask::UVToPixelSpace);
            device_data.debug_draw_texture = game_device_data.mv_reactive.get();
            device_data.debug_draw_texture_format = DXGI_FORMAT_R8_UNORM;
            device_data.debug_draw_texture_size = {game_device_data.mv_scene_size.x, game_device_data.mv_scene_size.y, 1, 1};
         }
         else if (device_data.debug_draw_texture && (device_data.debug_draw_texture.get() == game_device_data.mv_texture.get() || device_data.debug_draw_texture.get() == game_device_data.mv_reactive.get()))
         {
            device_data.debug_draw_texture = nullptr;
         }
      }
#endif
   }

   void LoadConfigs() override
   {
      reshade::get_config_value(nullptr, NAME, "MSAAResolveEnable", g_msaa_resolve_enable);
      reshade::get_config_value(nullptr, NAME, "SMAAEnable", g_smaa_enable);
      reshade::get_config_value(nullptr, NAME, "SMAAT2x", g_smaa_t2x);
#if DEVELOPMENT // Development settings only (see "DrawImGuiSettings"): a saved "off" must not outlive its control
      reshade::get_config_value(nullptr, NAME, "SMAAPredication", g_smaa_predication);
      reshade::get_config_value(nullptr, NAME, "SMAAPredicationTolerance", g_smaa_pred_tolerance);
#endif
      reshade::get_config_value(nullptr, NAME, "RCASSharpness", g_rcas_sharpness);
      reshade::get_config_value(nullptr, NAME, "GTAOEnable", g_gtao_enable);
      reshade::get_config_value(nullptr, NAME, "FixHighResolutionEffects", g_resolution_scaled_effects);
      reshade::get_config_value(nullptr, NAME, "FixFramePacing", g_fix_frame_pacing);
      reshade::get_config_value(nullptr, NAME, "60FPSCutscenes", g_60_fps_cutscenes);
      reshade::get_config_value(nullptr, NAME, "LumaBloomEnable", g_luma_bloom_enable);
      reshade::get_config_value(nullptr, NAME, "HighResolutionShadows", g_high_resolution_shadows);
      ShadowMap::Apply();
      reshade::get_config_value(nullptr, NAME, "Dithering", cb_luma_global_settings.GameSettings.Dithering);
      reshade::get_config_value(nullptr, NAME, "VideoAutoHDREnable", cb_luma_global_settings.GameSettings.VideoAutoHDREnable);
      reshade::get_config_value(nullptr, NAME, "VideoAutoHDRBoost", cb_luma_global_settings.GameSettings.VideoAutoHDRBoost);
      reshade::get_config_value(nullptr, NAME, "Contrast", cb_luma_global_settings.GameSettings.Contrast);
      reshade::get_config_value(nullptr, NAME, "Saturation", cb_luma_global_settings.GameSettings.Saturation);
      reshade::get_config_value(nullptr, NAME, "HighlightsDesaturation", cb_luma_global_settings.GameSettings.HighlightsDesaturation);
      reshade::get_config_value(nullptr, NAME, "ColorGradingIntensity", cb_luma_global_settings.GameSettings.ColorGradingIntensity);
      reshade::get_config_value(nullptr, NAME, "BloomIntensity", cb_luma_global_settings.GameSettings.BloomIntensity);
   }

   void DrawImGuiSettings(DeviceData& device_data) override
   {
      ImGui::SeparatorText("Anti-Aliasing");
      const auto& gd = GetGameDeviceData(device_data);
      if (gd.msaa_active)
      {
         ImGui::TextColored(ImVec4(1.f, 0.8f, 0.f, 1.f), "MSAA is on: DLAA, FSR and SMAA T2x need Anti-Aliasing off in the game's options.");
      }
      // The HDR-weighted resolve and alpha to coverage: both act only with the game's MSAA on (Saints Row's merged toggle)
      ImGui::BeginDisabled(!gd.msaa_active);
      DrawSettingCheckbox("Luma MSAA Enable", "MSAAResolveEnable", &g_msaa_resolve_enable, true, "Replaces the game's MSAA with an HDR-aware one (smoother edges on bright lights, foliage and fences; requires Anti-Aliasing enabled in the game's options; not used with DLSS/FSR).", !gd.msaa_active);
      ImGui::EndDisabled();
      // The upscaler (Super Resolution, in the Settings tab) replaces SMAA: greyed, its value kept. Not with MSAA ("sr_active"): SMAA runs then
      ImGui::BeginDisabled(gd.sr_active);
      DrawSettingCheckbox("SMAA Enable", "SMAAEnable", &g_smaa_enable, true, "Adds SMAA anti-aliasing on top of the game's own MSAA (or alone with it off; not used with DLSS/FSR).", gd.sr_active);
      ImGui::EndDisabled();
      // T2x needs the motion vectors, which a multisampled scene doesn't get
      const bool t2x_unavailable = gd.sr_active || gd.msaa_active;
      ImGui::BeginDisabled(!g_smaa_enable || t2x_unavailable);
      DrawSettingCheckbox("SMAA T2x", "SMAAT2x", &g_smaa_t2x, false, "Smoother edges and less shimmering, by blending each frame with the previous one.\nCan look slightly softer in motion. Not used with DLSS/FSR or the game's MSAA (Anti-Aliasing in the game's options).", t2x_unavailable);
      ImGui::EndDisabled();
#if DEVELOPMENT
      // Predication only relaxes the edge threshold back to ULTRA's on geometry, never below: off is strictly worse (MoH 2010, ME1)
      if (g_smaa_enable)
      {
         DrawSettingCheckbox("SMAA Predication", "SMAAPredication", &g_smaa_predication, true, "Edges from the depth prepass' geometry (needs SSAO on in the game's options).");
         ImGui::BeginDisabled(!g_smaa_predication);
         DrawSettingSlider("SMAA Predication Tolerance", "SMAAPredicationTolerance", &g_smaa_pred_tolerance, 0.02f, 0.002f, 0.2f, "Deviation from the local depth plane that counts as an edge, as a fraction of view depth. Lower = more edges.", "%.3f", ImGuiSliderFlags_Logarithmic);
         ImGui::EndDisabled();
         ImGui::Checkbox("SMAA Predication Debug View", &g_smaa_pred_debug);
         if (ImGui::IsItemHovered())
         {
            ImGui::SetTooltip("Show the predication mask (red) instead of the frame.\nWant: black on flat surfaces, red across silhouettes.");
         }
      }
#endif
      DrawSettingSlider("RCAS Sharpness", "RCASSharpness", &g_rcas_sharpness, 0.f, 0.f, 1.f, "Sharpening applied on top of anti-aliasing (0 = off).");

      auto& gs = cb_luma_global_settings.GameSettings;
      ImGui::SeparatorText("Grade");
      // Read in Copy_0x91A6C437.ps_5_0.hlsl (PsychoV31)
      DrawSettingSlider("Contrast", "Contrast", &gs.Contrast, default_luma_global_game_settings.Contrast, 0.f, 2.f, "Overall image contrast, HDR only (1 = vanilla).");
      DrawSettingSlider("Saturation", "Saturation", &gs.Saturation, default_luma_global_game_settings.Saturation, 0.f, 2.f, "Color saturation, HDR only (1 = vanilla).");
      DrawSettingSlider("Highlights Desaturation", "HighlightsDesaturation", &gs.HighlightsDesaturation, default_luma_global_game_settings.HighlightsDesaturation, 0.f, 1.f, "How far the brightest sources fade to neutral white, HDR only (0 = keep color at any brightness).");
      // Read in Grade_0x719339A9.ps_5_0.hlsl and GradeLuma_0xB7A08D4A.ps_5_0.hlsl
      DrawSettingSlider("Color Grading Intensity", "ColorGradingIntensity", &gs.ColorGradingIntensity, default_luma_global_game_settings.ColorGradingIntensity, 0.f, 1.f, "Strength of the game's own color grading (1 = vanilla, 0 = neutral).");

      ImGui::SeparatorText("Bloom");
      DrawSettingCheckbox("Luma Bloom Enable", "LumaBloomEnable", &g_luma_bloom_enable, true, "Draws the game's bloom at a higher resolution: the same look, without its blocky, flickering glow.");
      // Read in Copy_0x91A6C437.ps_5_0.hlsl on the bloom composite
      DrawSettingSlider("Bloom Intensity", "BloomIntensity", &gs.BloomIntensity, default_luma_global_game_settings.BloomIntensity, 0.f, 2.f, "Bloom strength (1 = vanilla, 0 = none).");

      ImGui::SeparatorText("Ambient Occlusion");
      DrawSettingCheckbox("XeGTAO Enable", "GTAOEnable", &g_gtao_enable, true, "Replaces the game's SSAO with XeGTAO (cleaner, more accurate ambient occlusion; requires SSAO enabled in the game's options).");
#if DEVELOPMENT || TEST
      ImGui::BeginDisabled(!g_gtao_enable);
      ImGui::SliderFloat("GTAO Final Value Power", &g_gtao_final_value_power, 0.3f, 4.5f, "%.2f");
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
      {
         ImGui::SetTooltip("Darkness dial (higher = darker AO).");
      }
      ImGui::SliderFloat("GTAO Depth Scale", &g_gtao_depth_scale, 0.01f, 200.f, "%.2f", ImGuiSliderFlags_Logarithmic);
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
      {
         ImGui::SetTooltip("View depth divisor (game units -> meters): the scale XeGTAO's radius and falloff are tuned for.");
      }
      ImGui::SliderFloat("GTAO Radius Override", &g_gtao_radius_override, 0.f, 5.f, "%.3f");
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
      {
         ImGui::SetTooltip("0 = the shader's EFFECT_RADIUS; > 0 overrides it (view units after the depth scale).");
      }
#if DEVELOPMENT // The shader's debug blocks are DEVELOPMENT only
      ImGui::Combo("GTAO Debug View", &g_gtao_debug_view, "Off\0Depth gradient\0Normals\0AO x8\0Edges\0");
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
      {
         ImGui::SetTooltip("Shows XeGTAO's diagnostics instead of the scene. Depth gradient dead or flat, or blocky normals = a wrong input; AO x8 = broad over-occlusion.");
      }
#endif
      ImGui::EndDisabled();
#endif

      ImGui::SeparatorText("Effects");
      // Read in Video_0xC60649FD.ps_5_0.hlsl. Inert in SDR: peak == paper white makes PumboAutoHDR an identity.
      DrawSettingCheckbox("Video AutoHDR", "VideoAutoHDREnable", &gs.VideoAutoHDREnable, default_luma_global_game_settings.VideoAutoHDREnable, "Adds HDR highlights to pre-rendered videos (HDR only).");

      ImGui::BeginDisabled(gs.VideoAutoHDREnable <= 0.5f);
      DrawSettingSlider("Video HDR Boost", "VideoAutoHDRBoost", &gs.VideoAutoHDRBoost, default_luma_global_game_settings.VideoAutoHDRBoost, 0.f, 1.f, "Video highlight strength (0 = off).");
      ImGui::EndDisabled();

      DrawSettingCheckbox("Dithering", "Dithering", &gs.Dithering, default_luma_global_game_settings.Dithering, "Reduces gradient banding.");

      // On the Steam executable only ("ShadowMap::Install")
      if (DrawSettingCheckbox("High Resolution Shadows", "HighResolutionShadows", &g_high_resolution_shadows, true,
             "Sharper, steadier shadow outlines at the game's highest shadow quality. Applies with the next level load or options change.",
             !ShadowMap::patchable))
      {
         ShadowMap::Apply();
      }

      ImGui::SeparatorText("UI");
      ImGui::Checkbox("Hide Gameplay UI", &g_hide_ui);
      if (ImGui::IsItemHovered())
      {
         ImGui::SetTooltip("Disables the in-game UI.");
      }

      ImGui::SeparatorText("Fixes");
      DrawSettingCheckbox("Fix High Resolution Effects", "FixHighResolutionEffects", &g_resolution_scaled_effects, true,
         "Keeps shadow softness, depth of field and SSAO the size they had at 1280x720, the resolution the game was made for. Above it the game shrinks them (razor sharp shadows and little depth of field at 4K).");
      // Only the Steam executable's limiters are hooked ("FrameLimiter::Install")
      ImGui::BeginDisabled(!FrameLimiter::installed);
      DrawSettingCheckbox("Fix Frame Pacing", "FixFramePacing", &g_fix_frame_pacing, true,
         "Replaces the game's frame limiter with a precise one at the display's refresh rate: even frame times and no periodic stutter at 60 Hz (the game paces at 59.94 fps and releases frames up to 2 ms early). Turn off an external frame rate limit with it.", !FrameLimiter::installed);
      ImGui::EndDisabled();
      // Patched from the limiter hooks, on the Steam executable only ("FrameLimiter::Apply60FpsCutscenes")
      ImGui::BeginDisabled(!FrameLimiter::cutscenes_patchable);
      DrawSettingCheckbox("60 FPS Cutscenes", "60FPSCutscenes", &g_60_fps_cutscenes, true,
         "Draws the real-time cutscenes at 60 fps instead of 30. Their speed and audio don't change: the game updates at 60 fps in them too and only drew every other update.", !FrameLimiter::cutscenes_patchable);
      ImGui::EndDisabled();
#if DEVELOPMENT
      ImGui::Text("Frame limiter: period %u us, spin margin %u us, late %u, resyncs %u, adopted %u", FrameLimiter::period_us, FrameLimiter::spin_margin_us, FrameLimiter::late_frames, FrameLimiter::resyncs, FrameLimiter::adopted_deadlines);
#endif

#if DEVELOPMENT
      ImGui::SeparatorText("Motion Vectors (DLSS/FSR)");
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
   }

   void PrintImGuiAbout() override
   {
      ImGui::PushTextWrapPos(0.f);
      ImGui::Text(
         "Luma for \"Bayonetta\" is developed by DristoforColumb and is open source and free.\n"
         "It adds HDR, DLAA or FSR 3 native anti-aliasing, SMAA (with T2x) and an HDR-aware MSAA with alpha to coverage.\n"
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
      Globals::SetGlobals(cleared_project_name, "Bayonetta Luma mod", "", mod_version);

      swapchain_format_upgrade_type = TextureFormatUpgradesType::AllowedEnabled;
      swapchain_upgrade_type = SwapchainUpgradeType::scRGB;

      // Exclusive fullscreen -> borderless, also when leaving it, as the other dgVoodoo mods
      prevent_fullscreen_state = true;
      force_borderless = true;

      // dgVoodoo forwards D3D9 SetGammaRamp to the OS device ramp, which distorts HDR output
      allow_disabling_gamma_ramp = true;

      // The upscalers' mip bias. Mode 3: only samplers at bias 0 get it, the game already sets -1 on its material samplers (trace)
      // and adding ours would make -2. force_upgrade_linear_samplers also gives the trilinear ones the bias (Core rewrites only
      // anisotropic samplers otherwise, TW2).
      enable_samplers_upgrade = true; // boot-time only (cannot be changed after device creation)
      samplers_upgrade_mode = 3;
      force_upgrade_linear_samplers = true;

      // Every scene target is A8R8G8B8. Indirect (creation-time mirrors, substituted at bind): changing dgVoodoo's own
      // creation formats breaks its bookkeeping (TW2).
      texture_format_upgrades_type = TextureFormatUpgradesType::AllowedEnabled;
      enable_indirect_texture_format_upgrades = true;
      enable_chain_indirect_texture_format_upgrades = ChainTextureFormatUpgradesType::DirectDependencies;
      texture_upgrade_formats = {
         reshade::api::format::r8g8b8a8_typeless, // dgVoodoo's D3D9 back buffer (the canvas) and the scene/post targets
         reshade::api::format::r8g8b8a8_unorm,
      };
      // "No1Px" as in the other dgVoodoo mods: the wrapper binds 1x1 placeholders in unused sampler slots, which pass the aspect filter
      texture_format_upgrades_2d_size_filters = (uint32_t)TextureFormatUpgrades2DSizeFilters::SwapchainResolution | (uint32_t)TextureFormatUpgrades2DSizeFilters::SwapchainAspectRatio | (uint32_t)TextureFormatUpgrades2DSizeFilters::No1Px;
      // With PreserveAspect (the default) the scene targets are cut to 16:9 inside any back buffer
      texture_format_upgrades_2d_size_filters |= (uint32_t)TextureFormatUpgrades2DSizeFilters::CustomAspectRatio;
      texture_format_upgrades_2d_custom_aspect_ratios = {16.f / 9.f};
      // dgVoodoo creates its D3D9 back buffer (the canvas) before it sizes the swapchain (a 1x1 one at device creation, the old size
      // on a resolution change), so the size filters miss it at creation: its mirror is made when the scene copy (ResetRender) binds it
      auto_texture_format_upgrade_shader_hashes[0x91A6C437] = {.rtv_slots = {0}};
      auto_texture_format_upgrade_shader_hashes[0x7584B641] = {.rtv_slots = {0}};

#if DEVELOPMENT
      // Offline map of the post shaders (both dgVoodoo builds), for traces and the DevKit
      for (const auto& [hash, name] : std::initializer_list<std::pair<const char*, const char*>>{
              {"719339A9", "Grade (filter00)"},
              {"3BE8E47F", "Grade (filter00)"},
              {"B7A08D4A", "Grade Luma Curve (filter04)"},
              {"2D4416C4", "Grade Luma Curve (filter04)"},
              {"8B6A8D61", "Bloom Source (filter05)"},
              {"29FE50C2", "Bloom Source (filter05)"},
              {"49231C65", "Bloom Blur H (filter06)"},
              {"FB14B417", "Bloom Blur H (filter06)"},
              {"F758B2DC", "Bloom Blur V (filter07)"},
              {"1F962E82", "Bloom Blur V (filter07)"},
              {"CE62BA4B", "Blur H (filter01)"},
              {"4B605EEB", "Blur H (filter01)"},
              {"91CC01D3", "Blur V (filter02)"},
              {"A5207DF1", "Blur V (filter02)"},
              {"68B566FE", "Centre Average (filter03)"},
              {"AB34862D", "Centre Average (filter03)"},
              {"9D71029B", "DoF CoC (filter08)"},
              {"CCDA0B04", "DoF CoC (filter08)"},
              {"40AF18CC", "SSAO"},
              {"C32759B1", "SSAO"},
              {"91A6C437", "Copy"},
              {"7584B641", "Copy"},
              {"B4CE6220", "Copy No Alpha"},
              {"F70BABE8", "Copy No Alpha"},
              {"87A169B9", "Object Motion Blur"},
              {"5BF04DF8", "Object Motion Blur"},
              {"651AC335", "Skinned Velocity (BlurSrcWeight)"},
              {"988429CF", "Skinned Velocity (BlurSrcWeight)"},
              {"C60649FD", "Video YUV"},
              {"ED0DA20F", "Video YUV"},
           })
      {
         forced_shader_names.emplace(std::stoul(hash, nullptr, 16), name);
      }
#endif

      game = new Bayonetta();
   }
   else if (ul_reason_for_call == DLL_PROCESS_DETACH)
   {
      Bayonetta::SetEvents(false);
      if (FrameLimiter::installed)
      {
         MH_RemoveHook(FrameLimiter::LIMIT_60);
         MH_RemoveHook(FrameLimiter::LIMIT_30);
      }
      if (FrameLimiter::timer)
      {
         CloseHandle(FrameLimiter::timer);
      }
   }

   CoreMain(hModule, ul_reason_for_call, lpReserved);

   return TRUE;
}
