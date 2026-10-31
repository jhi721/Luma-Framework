#define GAME_BAYONETTA 1

#define GEOMETRY_SHADER_SUPPORT 0
#define DISABLE_AUTO_DEBUGGER 1
// The UI pixel shaders get the clamp of the 8-bit canvas they were made for (see "PatchShaderBytecodeSync")
#define LUMA_PATCH_BYTECODE_SYNC 1
// Outside DEVELOPMENT only this define makes original_draw_dispatch_func non-null (the scene copy and the present blit draw it)
#define ENABLE_POST_DRAW_DISPATCH_CALLBACK 1
// The motion vector draw key reads "last_draw_dispatch_data", which Publishing only fills with this
#define ENABLE_DRAW_DISPATCH_DATA_CACHE 1
// Luma TAA among the upscalers (in process; DLSS and FSR 3 run in the SR bridge's x64 helper, "UseLumaSRBridge"). The floor
// flickered under it on a static frame (DLAA and FSR don't).
#define ENABLE_LUMA_TAA 1
// The game has MSAA but no shader AA: SMAA is added on the canvas after the scene copy (see "DrawSMAAOnCanvas")
#define ENABLE_SMAA 1
// SMAA's area texture without the U-shape smoothing (see "SMAA_AreaTex_SharpU.h"), as TW1 and ME1
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
// The bloom chain (_tools/bayonetta/NOTES.md "Frame order"), all into the fixed size 320x176 bloom target: filter05 (the scene
// into level 0, then each level's threshold and halving into the next), filter02 / filter01 (each level's V / H blur), then copytex
// (the levels added into level 0, and the composite onto the scene)
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
// Their c194 row in dgVoodoo's cb4 (pixel shaders: D3D9 c(n) = row n + 8)
static constexpr UINT TARGET_SIZE_ROW = 194 + 8;
static bool g_gtao_enable = true; // XeGTAO replaces the game's SSAO (see "RunXeGTAO")
// XeGTAO's knobs, "register(b9)" in Luma_Bayonetta_XeGTAO.hlsl and Luma_Bayonetta_XeGTAOApply.hlsl
static constexpr UINT GTAO_KNOBS_CB_SLOT = 9;
static float g_gtao_final_value_power = 1.f; // Darkness dial (higher = darker)
static float g_gtao_depth_scale = 1.f;       // View depth divisor (game units -> ~meters)
static float g_gtao_radius_override = 0.f;   // > 0 overrides the shader's EFFECT_RADIUS (view units after the depth scale)
#if DEVELOPMENT
static int g_gtao_debug_view = 0; // 0 off, 1 depth gradient, 2 normals, 3 AO x8, 4 edges (the shader's debug blocks are DEVELOPMENT only)
#else
constexpr int g_gtao_debug_view = 0;
#endif
// The reactive masks' resources go after this many frames without their pass
constexpr uint32_t IDLE_RELEASE_FRAMES = 600;

#if DEVELOPMENT
static bool g_mv_enable = false;         // Motion vectors without an upscaler
static bool g_mv_debug_view = false;     // Core's debug draw of the motion vector target
static bool g_mv_force_jitter = false;   // The projection jitter without an upscaler
static bool g_bound_state_check = false; // The tracked bindings against the bound ones ("mv.bound_state_mismatches")
static bool g_sr_reactive_enable = true;
// The additive draws' reactivity, scaled and capped at 0.9 (laser cores write ~0.3, glows 0.05-0.2: under a threshold they smeared)
static float g_sr_reactive_scale = 2.f;
static float g_sr_reactive_threshold = 0.5f; // The alpha blended draws': under it 0, over it 0.9 (Mass Effect 2007's and Borderlands 2's 0.5)
static bool g_sr_reactive_debug_view = false;
#else
constexpr bool g_mv_enable = false;
constexpr bool g_mv_force_jitter = false;
constexpr bool g_bound_state_check = false;
constexpr bool g_sr_reactive_enable = true;
constexpr float g_sr_reactive_scale = 2.f;
constexpr float g_sr_reactive_threshold = 0.5f;
#endif
using SceneMotionVectors::PixelShaderPatch;
// The coverage patch is the game's first pixel shader patch after Core's
constexpr PixelShaderPatch COVERAGE_PATCH = PixelShaderPatch::GAME_0;

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
   static Limiter original_60 = nullptr;
   static Limiter original_30 = nullptr;
   static bool installed = false;
   static bool cutscenes_patchable = false; // FUN_0045b4a0 as shipped (not already patched by another mod)
   static bool cutscenes_patched = false;

   // The game's main thread only. No VBlank phase lock (FL-9): D3DKMTGetScanLine, the only live VBlank source under independent flip,
   // stalls ~3.7 ms in 0.1-0.5% of its calls (p999 3.72 ms), a hitch before Present every few seconds.
   static int64_t frequency = 0;
   static int64_t next = 0; // QPC
   // The deadline last written: any other value came from the game (its start, the movie sync, its own limiter while toggled off)
   static float written_next_ms = std::numeric_limits<float>::quiet_NaN();
   static HANDLE timer = nullptr;
   static int64_t spin_margin = 0; // FL-7
   static uint32_t display_query_frame = 0;
   static int64_t refresh_period = 0; // QPC ticks, 0 unknown
#if DEVELOPMENT
   static uint32_t late_frames = 0;
   static uint32_t resyncs = 0;
   static uint32_t adopted_deadlines = 0;
   static uint32_t period_us = 0;
   static uint32_t spin_margin_us = 0;
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
      if (!System::PatchMemory(APPLY_30_FPS_REQUEST, bytes.data(), bytes.size(), System::PatchMemoryType::Code))
         return;
      cutscenes_patched = patch;
   }

   void LimitOrCallOriginal(uint32_t frames, Limiter original)
   {
      Apply60FpsCutscenes();
      if (g_fix_frame_pacing)
      {
         Limit(frames);
      }
      else
      {
         original();
      }
   }

   void __cdecl Limit60()
   {
      LimitOrCallOriginal(1, original_60);
   }

   // The caller pushes an unused argument (cdecl: it pops it)
   void __cdecl Limit30()
   {
      LimitOrCallOriginal(2, original_30);
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
      installed = MH_CreateHook(LIMIT_60, reinterpret_cast<void*>(&Limit60), reinterpret_cast<void**>(&original_60)) == MH_OK &&
                  MH_CreateHook(LIMIT_30, reinterpret_cast<void*>(&Limit30), reinterpret_cast<void**>(&original_30)) == MH_OK &&
                  MH_EnableHook(LIMIT_60) == MH_OK && MH_EnableHook(LIMIT_30) == MH_OK;
      if (!installed)
      {
         MH_RemoveHook(LIMIT_60);
         MH_RemoveHook(LIMIT_30);
         return;
      }
      // FL-6: Windows 11 ignores the timer resolution and slows the process down when its window is occluded or in the background
      PROCESS_POWER_THROTTLING_STATE throttling{.Version = PROCESS_POWER_THROTTLING_CURRENT_VERSION, .ControlMask = PROCESS_POWER_THROTTLING_EXECUTION_SPEED | PROCESS_POWER_THROTTLING_IGNORE_TIMER_RESOLUTION, .StateMask = 0};
      SetProcessInformation(GetCurrentProcess(), ProcessPowerThrottling, &throttling, sizeof(throttling));
      cutscenes_patchable = std::memcmp(APPLY_30_FPS_REQUEST, APPLY_30_FPS_REQUEST_ORIGINAL.data(), APPLY_30_FPS_REQUEST_ORIGINAL.size()) == 0;
   }

   // From "DLL_PROCESS_DETACH": the game's limiters and its 30 fps request back as shipped
   void Uninstall()
   {
      if (installed)
      {
         MH_RemoveHook(LIMIT_60);
         MH_RemoveHook(LIMIT_30);
         installed = false;
      }
      if (cutscenes_patched && System::PatchMemory(APPLY_30_FPS_REQUEST, APPLY_30_FPS_REQUEST_ORIGINAL.data(), APPLY_30_FPS_REQUEST_ORIGINAL.size(), System::PatchMemoryType::Code))
      {
         cutscenes_patched = false;
      }
      if (timer)
      {
         CloseHandle(timer);
         timer = nullptr;
      }
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
   static bool patchable = false;

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

   // From "DLL_PROCESS_DETACH": the game's size table back (the map keeps its size until the game's next options change or level load)
   void Uninstall()
   {
      if (patchable && SIZES[2] != ORIGINAL_SIZES.back())
      {
         System::PatchMemory(&SIZES[2], &ORIGINAL_SIZES.back(), sizeof(uint32_t), System::PatchMemoryType::Data);
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
   // 2's CPU cuts, one per-draw buffer: dgVoodoo's vc4; the state machine is Core's "SceneMotionVectors") ----
   // DLSS and FSR run in the x64 helper of Core's SR bridge (the game is 32-bit, see "SRBridge.h"), Luma TAA in process.
   // An upscaler is picked, hasn't failed (it then gives way to SMAA until picked again) and the game's MSAA is off, latched at present
   // for the whole frame: a selection made after the motion vector state was set would otherwise run the upscaler on mixed state
   bool sr_active = false;
   // The upscaler's output goes back into the scene (and its copy, see "mv_scene_copy") without its alpha (the bloom source's
   // threshold input), drawn from this view of it
   com_ptr<ID3D11BlendState> sr_rgb_blend_state;
   com_ptr<ID3D11ShaderResourceView> sr_output_srv;

   // The motion vector targets, camera, objects and vc4 copies; "mv.active": motion vectors and jitter this frame (an upscaler or the
   // DEV toggle, set at present)
   SceneMotionVectors::State mv;
   // The upscaler drew the last frame: only then the scene jitters (frames it skips would reach the screen jittered)
   bool mv_jitter_allowed = false;
   float sr_vert_fov = 1.0471976f; // FSR's vertical FOV (radians): the last camera's, 60 degrees until one is seen
   // The upscalers' near and far (game units): the last camera's with a usable projection, Core's defaults until one is seen
   float sr_near_plane = SR::SuperResolutionImpl::DrawData{}.near_plane;
   float sr_far_plane = SR::SuperResolutionImpl::DrawData{}.far_plane;
   // The copy of the scene the first post pass reads, null if it reads none. The upscaler's output goes into both
   // ("Bayonetta Copy Back PS"). Found from the game's last CopyResource of the scene: the camera blur (cmx) also reads a copy of the
   // velocity target, whose description matches the scene's.
   com_ptr<ID3D11Resource> mv_scene_copy;
   com_ptr<ID3D11Resource> mv_scene_last_copy;
   com_ptr<ID3D11RenderTargetView> mv_scene_copy_rtv;
   // The depth prepass' R32F target (1 - z/w, next to the normals): where the fill reads the depth. "prepass_drawn": it was drawn into
   // this frame; "prepass_rtv" and "prepass_is_depth" classify the last target seen next to a second one.
   com_ptr<ID3D11Resource> prepass_depth;
   com_ptr<ID3D11ShaderResourceView> prepass_depth_srv;
   ID3D11RenderTargetView* prepass_rtv = nullptr;
   bool prepass_is_depth = false;
   bool prepass_drawn = false;
   bool mv_filled = false; // The fill wrote this frame's motion vectors (reset at present)
   // The targets "OpenScene" refused (not the scene's); reset at present
   ID3D11RenderTargetView* open_refused_rtv = nullptr;
   ID3D11DepthStencilView* open_refused_dsv = nullptr;

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
};

class Bayonetta final : public Game
{
   static BayonettaGameDeviceData& GetGameDeviceData(DeviceData& device_data)
   {
      return *static_cast<BayonettaGameDeviceData*>(device_data.game);
   }

   // Zeroed for a resource that isn't a 2D texture
   static D3D11_TEXTURE2D_DESC GetTexture2DDesc(ID3D11Resource* resource)
   {
      D3D11_TEXTURE2D_DESC desc = {};
      com_ptr<ID3D11Texture2D> texture;
      if (resource && SUCCEEDED(resource->QueryInterface(&texture)))
      {
         texture->GetDesc(&desc);
      }
      return desc;
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
      const CB::DepthEdgesConstants predication_constants = {.tolerance = g_smaa_pred_tolerance, .scene_origin = {scene_rect.TopLeftX, scene_rect.TopLeftY}};
      predication = predication && gd.smaa_predication_srv && PatchedDraws::WriteDynamicConstants(native_device, native_device_context, std::addressof(gd.smaa_predication_buffer), &predication_constants, sizeof(predication_constants));
      // SMAA T2x: SMAA into this frame's linear target with the velocity, then the resolve with the previous frame into the canvas
      // (Mirror's Edge's). It needs this frame's motion vectors from the fill, at the canvas' size: the motion vectors cover the scene
      // rect, so a canvas wider than the scene (PreserveAspect) stays 1x. Without them this frame is 1x and the next one doesn't jitter.
      auto* const copy_vs = FindShader(device_data.native_vertex_shaders, CompileTimeStringHash("Copy VS"));
      auto* const t2x_weight_ps = FindShader(device_data.native_pixel_shaders, CompileTimeStringHash("Bayonetta SMAA T2x Blending Weight Calculation PS"));
      auto* const t2x_blend_ps = FindShader(device_data.native_pixel_shaders, CompileTimeStringHash("Bayonetta SMAA T2x Neighborhood Blending PS"));
      auto* const t2x_resolve_ps = FindShader(device_data.native_pixel_shaders, CompileTimeStringHash("Bayonetta SMAA T2x Resolve PS"));
      const uint32_t frame_index = cb_luma_global_settings.FrameIndex;
      bool t2x = gd.t2x_active && gd.mv_filled && gd.mv.srv && copy_vs && t2x_weight_ps && t2x_blend_ps && t2x_resolve_ps && GetViewTextureSize(gd.mv.srv.get()) == size;
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
      const CB::SmaaMetrics metrics = {
         .rt_metrics = {1.f / float(size.x), 1.f / float(size.y), float(size.x), float(size.y)},
         .predication = {(predication ? 2.f : 1.f), 0.f, 0.f, 0.f},
         .subsample_indices = {subsample_indices, subsample_indices, subsample_indices, 0.f},
      };
      if (!PatchedDraws::WriteDynamicConstants(native_device, native_device_context, std::addressof(gd.smaa_metrics_buffer), &metrics, sizeof(metrics)))
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
         const SMAAT2xPasses t2x_passes = {.blending_weight_calculation_ps = t2x_weight_ps, .neighborhood_blending_ps = t2x_blend_ps, .velocity = gd.mv.srv.get()};
         DrawSMAA(native_device, native_device_context, device_data, gd.t2x_frame_rtvs[t2x_current].get(), gd.scene_srv.get(), gd.scene_srv.get(), predication ? gd.smaa_predication_srv.get() : nullptr, nullptr, &t2x_passes);
         // The resolve: t0 this frame, t1 the previous one or this one again without a history, t2 the motion vectors, s0 linear and
         // s1 point. RGB only: the canvas alpha stays the scene copy's (T2x's alpha is the velocity length).
         native_device_context->OMSetRenderTargets(0, nullptr, nullptr);
         ID3D11ShaderResourceView* const resolve_srvs[2] = {gd.t2x_frame_srvs[t2x_history ? (t2x_current ^ 1) : t2x_current].get(), gd.mv.srv.get()};
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

   // ---- DLAA / FSR 3 Native AA / Luma TAA (vc4 layout in "MotionVectorPatches"; the state machine is Core's "SceneMotionVectors") ----
   // The view projection's vc4 byte offset: the camera FSR reads
   static constexpr UINT VIEW_PROJECTION_OFFSET = MotionVectorPatches::view_projection_row * 16;

   // The upscalers that read the reactive mask the alpha blended draws write: FSR and Luma TAA (DLSS's current presets ignore it,
   // DLSS-Best-Practices TRN-2)
   static bool IsReactiveMaskUsed(const DeviceData& device_data)
   {
      return g_sr_reactive_enable && static_cast<const BayonettaGameDeviceData*>(device_data.game)->sr_active && (device_data.sr_type == SR::Type::FSR || device_data.sr_type == SR::Type::LumaTAA);
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
      static constexpr const char* log_tag = "Bayonetta";
      static constexpr MotionVectorPatch::Layout layout = MotionVectorPatches::layout;
      static constexpr auto cameras = MotionVectorPatches::cameras;
      static constexpr auto tie_break_rows = MotionVectorPatches::tie_break_rows;
      static constexpr auto frame_camera = MotionVectorPatches::frame_camera;
      static constexpr const char* patch_names[size_t(PixelShaderPatch::COUNT)] = {"MV PS", "MV PS (mask)", "MV PS (mask)", "MV PS (blended)", "Coverage PS", "", "", ""};

      // The scene color: fp16, single sample, with a shader resource view (the upscaler reads it)
      static bool AcceptSceneColor(const D3D11_RENDER_TARGET_VIEW_DESC& rtv_desc, const D3D11_TEXTURE2D_DESC& color_desc)
      {
         return rtv_desc.Format == DXGI_FORMAT_R16G16B16A16_FLOAT && color_desc.ArraySize == 1 && color_desc.SampleDesc.Count == 1 && (color_desc.BindFlags & D3D11_BIND_SHADER_RESOURCE) != 0;
      }

      static bool IsExcludedPixelShader(uint32_t pixel_shader_hash)
      {
         return shader_hashes_SceneRefraction.Contains(pixel_shader_hash, reshade::api::shader_stage::pixel);
      }

      static bool ReactiveMaskUsed(const DeviceData& device_data)
      {
         return IsReactiveMaskUsed(device_data);
      }

      static bool CanFill(DeviceData& device_data)
      {
         auto& gd = GetGameDeviceData(device_data);
         return gd.prepass_drawn && gd.prepass_depth_srv && HasShaders(device_data.native_compute_shaders, CompileTimeStringHash("Bayonetta Motion Vector Fill CS"));
      }

      static void FillMarkersBeforeBlend(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, DeviceData& device_data)
      {
         FillMotionVectors(native_device, native_device_context, device_data, /* write_reactive */ false);
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
         case COVERAGE_PATCH:
            // The alpha tested materials clip at 0.5; the clips at 0.001 (blended materials' invisible texels) and the depth prepass's
            // at 0.05 are below the threshold and stay as they are
            return CoveragePatch::PatchPixelShader(code, code_size, error, 0.1f);
         }
         return {};
      }
   };

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
      const uint2 depth_size = GetViewTextureSize(dsv);
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
      const D3D11_TEXTURE2D_DESC color_desc = GetTexture2DDesc(color.get());
      // Only the depth prepass (R32F 1 - z/w next to the normals) opens the scene: OtMirror draws its reflection earlier into another
      // full size fp16 target, with no prepass (and a view projection whose determinant has the opposite sign of the scene's). The
      // scene color is the first fp16 target drawn with this depth ("SceneMotionVectors::DrawWithMotionVectors").
      const bool scene_format = rtv_desc.Format == DXGI_FORMAT_R32_FLOAT && has_second_target;
      if (!scene_format || color_desc.SampleDesc.Count != 1 || color_desc.Width != depth_size.x || color_desc.Height != depth_size.y)
      {
         refuse();
         return;
      }
      UINT viewport_count = 1;
      D3D11_VIEWPORT viewport = {};
      native_device_context->RSGetViewports(&viewport_count, &viewport);
      if (viewport_count == 0 || viewport.Width != float(depth_size.x) || viewport.Height != float(depth_size.y) || !SceneMotionVectors::GetPatchedVertexShader<MotionVectorGame>(&gd.mv, native_device, cmd_list_data, device_data, vertex_shader_hash).shader)
         return;
      gd.mv_scene_last_copy.reset();
      gd.prepass_rtv = nullptr;
      gd.prepass_drawn = false;
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

   // DLAA, FSR 3 Native AA or Luma TAA on the jittered scene, its depth (from the depth prepass, see the fill) and the motion vectors;
   // the result goes back into the scene's color (and its copy, see "mv_scene_copy"), its alpha (the bloom source's threshold input)
   // kept. False if it didn't draw (missing input, or the upscaler failed). "reactive_mask": the fill wrote this scene's masks.
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
      ID3D11BlendState* const rgb_blend_state = gd.GetRGBBlendState(native_device);
      if (!rgb_blend_state)
         return false;

      // FSR needs the camera (column vectors: vc4's view projection rows are the clip components)
      const SR::ViewProjectionCamera camera = SR::GetViewProjectionCamera(reinterpret_cast<const float*>(gd.mv.camera->data() + VIEW_PROJECTION_OFFSET), /* row_vectors */ false);

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
               gd.mv.scene_rtv->GetDesc(&rtv_desc);
               if (copy_desc.BindFlags & D3D11_BIND_RENDER_TARGET)
               {
                  native_device->CreateRenderTargetView(copy.get(), &rtv_desc, &gd.mv_scene_copy_rtv);
               }
            }
         }
         copy_rtv = gd.mv_scene_copy_rtv.get();
      }
      DrawCustomPixelShader(native_device_context, device_data.default_depth_stencil_state.get(), rgb_blend_state, nullptr, copy_vs, copy_rtv ? copy_back_ps : copy_ps,
         gd.sr_output_srv.get(), gd.mv.scene_rtv.get(), scene_desc.Width, scene_desc.Height, true, copy_rtv);
      if (gd.mv_scene_copy && !copy_rtv)
      {
         native_device_context->CopyResource(gd.mv_scene_copy.get(), scene.get());
      }
      // Not while the bridge's helper starts (the color copied as it is): SMAA stays on and the next frame resets
      device_data.has_drawn_sr = sr_implementations[device_data.sr_type]->IsReady(sr_instance_data);
      return true;
   }

   // The depth and camera motion fill (see "Luma_Bayonetta_MotionVectorFill.hlsl"), with the reactive masks if "write_reactive". The
   // caller caches and restores the state. False if it didn't run.
   static bool FillMotionVectors(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, DeviceData& device_data, bool write_reactive)
   {
      auto& gd = GetGameDeviceData(device_data);
      auto* const fill_shader = FindShader(device_data.native_compute_shaders, CompileTimeStringHash("Bayonetta Motion Vector Fill CS"));
      if (!fill_shader || !gd.mv.camera || !gd.prepass_drawn || !gd.prepass_depth_srv)
         return false;
#if ENABLE_LUMA_TAA
      // Luma TAA without its 2x history (Medium; the game renders at the output size, so no TAAU) flickered on lasers at the High
      // scale in the TAA lab: half of it balanced flicker and smear (_tools/taa_lab/reactive_bayo_eval.py, 2026-10-31)
      const float reactive_scale = (device_data.sr_type == SR::Type::LumaTAA && LumaTAA::quality.load() < LumaTAA::history_2x_quality ? 0.5f : 1.f) * g_sr_reactive_scale;
#else
      const float reactive_scale = g_sr_reactive_scale;
#endif
      CB::MotionVectorFillConstants constants = {
         .jitter_ndc = {gd.mv.jitter_ndc[0], gd.mv.jitter_ndc[1]},
         .reactive_scale = reactive_scale,
         .reactive_threshold = g_sr_reactive_threshold,
         .reactive_enabled = (write_reactive ? 1.f : 0.f),
      };
      for (int i = 0; i < 16; i++)
      {
         constants.reprojection.GetData()[i] = float(gd.mv.reprojection.GetData()[i]);
      }
      if (!PatchedDraws::WriteDynamicConstants(native_device, native_device_context, std::addressof(gd.mv.fill_buffer), &constants, sizeof(constants)))
         return false;
      // The scene and the motion vectors may be bound as render targets
      native_device_context->OMSetRenderTargets(0, nullptr, nullptr);
      ID3D11Buffer* const buffer = gd.mv.fill_buffer.get();
      ID3D11ShaderResourceView* const srvs[2] = {gd.prepass_depth_srv.get(), write_reactive ? gd.mv.reactive_target_srv.get() : nullptr};
      ID3D11UnorderedAccessView* const uavs[5] = {gd.mv.uav.get(), gd.mv.device_depth_uav.get(), write_reactive ? gd.mv.reactive_uav.get() : nullptr, write_reactive ? gd.mv.transparency_uav.get() : nullptr,
         write_reactive ? gd.mv.reactive_previous_uav.get() : nullptr};
      native_device_context->CSSetConstantBuffers(0, 1, &buffer);
      native_device_context->CSSetShaderResources(0, UINT(std::size(srvs)), srvs);
      native_device_context->CSSetUnorderedAccessViews(0, UINT(std::size(uavs)), uavs, nullptr);
      native_device_context->CSSetShader(fill_shader, nullptr, 0);
      native_device_context->Dispatch((gd.mv.scene_size.x + 7) / 8, (gd.mv.scene_size.y + 7) / 8, 1);
      ID3D11UnorderedAccessView* const null_uavs[std::size(uavs)] = {};
      ID3D11ShaderResourceView* const null_srvs[std::size(srvs)] = {};
      native_device_context->CSSetUnorderedAccessViews(0, UINT(std::size(null_uavs)), null_uavs, nullptr);
      native_device_context->CSSetShaderResources(0, UINT(std::size(null_srvs)), null_srvs);
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
      const bool reactive = IsReactiveMaskUsed(device_data) && gd.mv.reactive_target_srv;
      if (reactive)
      {
         D3D11_TEXTURE2D_DESC desc = {};
         if (gd.mv.reactive)
         {
            gd.mv.reactive->GetDesc(&desc);
         }
         if (desc.Width != gd.mv.scene_size.x || desc.Height != gd.mv.scene_size.y)
         {
            const std::unique_lock lock(gd.mv.mutex);
            gd.mv.reactive.reset();
            gd.mv.reactive_uav.reset();
            gd.mv.transparency.reset();
            gd.mv.transparency_uav.reset();
            gd.mv.reactive_previous.reset();
            gd.mv.reactive_previous_uav.reset();
            desc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R8_UNORM, gd.mv.scene_size.x, gd.mv.scene_size.y, 1, 1, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS);
            if (SUCCEEDED(SRBridge::CreateSharableTexture(native_device, desc, &gd.mv.reactive)))
            {
               native_device->CreateUnorderedAccessView(gd.mv.reactive.get(), nullptr, &gd.mv.reactive_uav);
            }
            if (SUCCEEDED(SRBridge::CreateSharableTexture(native_device, desc, &gd.mv.transparency)))
            {
               native_device->CreateUnorderedAccessView(gd.mv.transparency.get(), nullptr, &gd.mv.transparency_uav);
            }
            const CD3D11_TEXTURE2D_DESC previous_desc(DXGI_FORMAT_R32_FLOAT, gd.mv.scene_size.x, gd.mv.scene_size.y, 1, 1, D3D11_BIND_UNORDERED_ACCESS);
            if (SUCCEEDED(native_device->CreateTexture2D(&previous_desc, nullptr, &gd.mv.reactive_previous)) &&
                SUCCEEDED(native_device->CreateUnorderedAccessView(gd.mv.reactive_previous.get(), nullptr, &gd.mv.reactive_previous_uav)))
            {
               constexpr float zero[4] = {};
               native_device_context->ClearUnorderedAccessViewFloat(gd.mv.reactive_previous_uav.get(), zero);
            }
         }
      }
      const bool write_reactive = reactive && gd.mv.reactive_uav && gd.mv.transparency_uav && gd.mv.reactive_previous_uav;
      const bool filled = std::exchange(gd.mv.fill_pending, false) && FillMotionVectors(native_device, native_device_context, device_data, write_reactive);
      gd.mv_filled = filled;
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

public:
   // Core's motion vector events (the motion vector target in every blend state, the tracked bindings, the vc4 copies) and the MSAA
   // targets' shader resource bind
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
      if (enable)
      {
         SceneMotionVectors::RegisterEvents<GetSceneMotionVectors>();
      }
      else
      {
         SceneMotionVectors::UnregisterEvents<GetSceneMotionVectors>();
      }
      set.template operator()<reshade::addon_event::create_resource>(OnCreateResource);
      set.template operator()<reshade::addon_event::copy_resource>(OnCopyResource);
      set.template operator()<reshade::addon_event::copy_texture_region>(OnCopyTextureRegion);
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

      sr_game_tooltip = "Needs Anti-Aliasing off in the game's options. DLSS, FSR and XeSS require Luma-Upscaler.exe next to the game's exe.\n";

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
         MCP_GAME_TEXTURE("mv.velocity", mv.texture), MCP_GAME_TEXTURE("mv.depth", mv.device_depth), MCP_GAME_TEXTURE("mv.prepass", prepass_depth_srv),
         MCP_GAME_TEXTURE("sr.reactive", mv.reactive), MCP_GAME_TEXTURE("sr.transparency", mv.transparency), MCP_GAME_TEXTURE("sr.draws_mask", mv.reactive_target)});
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
         redirected = DXBC::RedirectConstantRow(code, &size, MotionVectorPatches::object_slot, TARGET_SIZE_ROW, TARGET_SIZE_CB_SLOT, 0);
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

   // Every patch a draw may ask "SceneMotionVectors::GetPatchedShader" for: which one it takes depends on its state
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
      // dgVoodoo's own compute shaders have no variants
      if (type != reshade::api::pipeline_subobject_type::pixel_shader)
         return;
      for (size_t patch = 0; patch <= size_t(COVERAGE_PATCH); patch++)
      {
         if (std::vector<uint8_t> patched = MotionVectorGame::PatchPixelShader(PixelShaderPatch(patch), code.data(), code.size(), &error); !patched.empty())
         {
            variants->push_back(std::move(patched));
         }
      }
   }

   void OnCreateDevice(ID3D11Device* native_device, DeviceData& device_data) override
   {
      device_data.game = new BayonettaGameDeviceData;
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
      SceneMotionVectors::RegisterDevValues(GetGameDeviceData(device_data).mv, &device_data);
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
      if (game_device_data.mv.scene_open && game_device_data.mv.scene_color && src_resource == reinterpret_cast<uint64_t>(game_device_data.mv.scene_color.get()))
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
      if (target_size != gd->pixel_kernel_target_size)
      {
         if (!PatchedDraws::WriteDynamicConstants(native_device, native_device_context, std::addressof(gd->pixel_kernel_target_size_buffer), target_size.data(), sizeof(target_size)))
            return;
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
      constexpr uint2 MODELVIEW_ROWS = {MotionVectorPatches::object_row_offset + 20, MotionVectorPatches::object_row_offset + 24}; // "Rmodelview" c20, "Rmodelviewproj" c24
      constexpr uint2 VIEW_ROWS = {MotionVectorPatches::object_row_offset, MotionVectorPatches::view_projection_row};              // "view" c0, "g_ViewProjection" c8
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
            if (reads(MODELVIEW_ROWS.x) && reads(MODELVIEW_ROWS.y))
            {
               rows = MODELVIEW_ROWS;
            }
            else if (reads(VIEW_ROWS.x) && reads(VIEW_ROWS.y))
            {
               rows = VIEW_ROWS;
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
      const D3D11_BOX box = (source_box ? D3D11_BOX{.left = source_box->left * scale, .top = source_box->top * scale, .front = 0, .right = source_box->right * scale, .bottom = source_box->bottom * scale, .back = 1}
                                        : D3D11_BOX{.left = 0, .top = 0, .front = 0, .right = dest_desc.Width * scale, .bottom = dest_desc.Height * scale, .back = 1});
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
      if (!rtv)
         return false;
      ComPtr<ID3D11Resource> target;
      rtv->GetResource(target.put());
      // Before the source and sizes: most copies draw elsewhere
      if (!is_bloom_source && uint64_t(target.get()) != gd.bloom_target)
         return false;
      ComPtr<ID3D11ShaderResourceView> source_srv;
      native_device_context->PSGetShaderResources(0, 1, source_srv.put());
      if (!source_srv)
         return false;
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
         const uint32_t scale = ((g_bloom_scale_override > 0) ? uint32_t(g_bloom_scale_override) : (std::max)(1u, uint32_t(std::lround(source_size.y / 720.0))));
         if (scale != gd.bloom_scale || !gd.bloom_rtv || GetViewTextureSize(gd.bloom_rtv.get()) != uint2{target_size.x * scale, target_size.y * scale})
         {
            gd.bloom_rtv.reset();
            gd.bloom_srv.reset();
            gd.bloom_texture.reset();
            gd.bloom_shadows.clear();
            // 16 bit UNORM: the 8 bit target's [0, 1] range (the chain relies on its clamp) without its banding at the finer size
            const CD3D11_TEXTURE2D_DESC desc(DXGI_FORMAT_R16G16B16A16_UNORM, target_size.x * scale, target_size.y * scale, 1, 1, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET);
            if (FAILED(native_device->CreateTexture2D(&desc, nullptr, gd.bloom_texture.put())) || FAILED(native_device->CreateRenderTargetView(gd.bloom_texture.get(), nullptr, gd.bloom_rtv.put())) ||
                FAILED(native_device->CreateShaderResourceView(gd.bloom_texture.get(), nullptr, gd.bloom_srv.put())))
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
      // blurs' clamped taps (a dark top and left edge).
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

      const CB::GTAOKnobs knobs = {
         .final_value_power = g_gtao_final_value_power,
         .depth_scale = g_gtao_depth_scale,
         .radius_override = g_gtao_radius_override,
         .debug_view = float(g_gtao_debug_view),
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
      // The game's lists are all pixel shaders, looked up by this hash ("Contains" with a whole hash list would walk it)
      const uint32_t pixel_shader_hash = uint32_t(original_shader_hashes.pixel_shaders[0]);
      const auto bind_luma_cbuffers = [&](uint32_t custom_data_1 = 0, uint32_t custom_data_2 = 0)
      {
         SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, stages, LumaConstantBufferType::LumaSettings);
         SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, stages, LumaConstantBufferType::LumaData, custom_data_1, custom_data_2);
         updated_cbuffers = true;
      };
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
      if (g_hide_ui && game_device_data.has_drawn_reset_render && original_shader_hashes.pixel_shaders[0] != SHADER_HASH_NONE && !shader_hashes_Video.Contains(pixel_shader_hash, reshade::api::shader_stage::pixel) && bound_canvas_rtv())
      {
         return DrawOrDispatchOverrideType::Replaced;
      }
      // Draws the replaced pass with the Luma cbuffers, then copies the canvas it drew on into "scene_texture": the floor the
      // present blit clamps the later UI to
      auto draw_and_snapshot_canvas = [&](ID3D11RenderTargetView* rtv, ID3D11Resource* canvas, uint32_t custom_data)
      {
         bind_luma_cbuffers(custom_data);
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
         if (!game_device_data.scene_srv)
            return DrawOrDispatchOverrideType::Replaced;
         native_device_context->CopyResource(game_device_data.scene_texture.get(), canvas);
         game_device_data.has_copied_scene_to_canvas = true;
         // SMAA then RCAS on the scene copy (not on the movies), each reading the snapshot and writing the canvas, then the snapshot
         // again: the UI clamp floor must be the final canvas. An upscaler that drew this frame antialiased it: SMAA stays out, RCAS
         // stays on.
         if (custom_data != 1)
            return DrawOrDispatchOverrideType::Replaced;
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
         return DrawOrDispatchOverrideType::Replaced;
      };

      // The pixel kernels' target size
      if (shader_hashes_PixelKernels.Contains(pixel_shader_hash, reshade::api::shader_stage::pixel) || shader_hashes_ShadowReceivers.Contains(pixel_shader_hash, reshade::api::shader_stage::pixel))
      {
         BindPixelKernelTargetSize(native_device, native_device_context, &game_device_data);
      }
      // XeGTAO's camera, from the frame's first depth prepass draw with a known layout. Before the motion vector draws, which replace the
      // prepass' draws when they jitter.
      const bool has_vertex_shader = (stages & reshade::api::shader_stage::vertex) == reshade::api::shader_stage::vertex;
      if (g_gtao_enable && is_immediate && !game_device_data.gtao_camera_taken && has_vertex_shader)
      {
         TakePrepassCamera(native_device, native_device_context, cmd_list_data, device_data, uint32_t(original_shader_hashes.vertex_shaders[0]));
      }

      // DLAA/FSR/Luma TAA: the scene's draws with motion vectors or jitter (see "DrawWithMotionVectors"), and the fill and upscaler at
      // its first post pass
      if (game_device_data.mv.active && is_immediate)
      {
         // A game draw with a vertex shader, which the patched shaders can redo: replaced ones too (the particles translate to copytex,
         // replaced by "Copy_0x91A6C437"; their patched versions come from the game's code, see "SceneMotionVectors::GetPatchedShader")
         const bool game_mesh_draw = has_vertex_shader && original_shader_hashes.pixel_shaders[0] != SHADER_HASH_NONE;
         if (!game_device_data.mv.scene_done && shader_hashes_ScenePost.Contains(pixel_shader_hash, reshade::api::shader_stage::pixel))
         {
            if (game_device_data.mv.scene_open)
            {
#if DEVELOPMENT
               game_device_data.mv.stats.ended_by = pixel_shader_hash;
               game_device_data.mv.stats.prepass_drawn = game_device_data.prepass_drawn;
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
         else if (!game_device_data.mv.scene_done && !game_device_data.mv.scene_open && game_mesh_draw)
         {
            com_ptr<ID3D11RenderTargetView> rtvs[2];
            com_ptr<ID3D11DepthStencilView> scene_dsv;
            native_device_context->OMGetRenderTargets(2, &rtvs[0], &scene_dsv);
            if (rtvs[0] && scene_dsv && (rtvs[0].get() != game_device_data.open_refused_rtv || scene_dsv.get() != game_device_data.open_refused_dsv))
            {
               OpenScene(native_device, native_device_context, cmd_list_data, device_data, original_shader_hashes.vertex_shaders[0], rtvs[0].get(), rtvs[1] != nullptr, scene_dsv.get());
            }
         }
         if (game_device_data.mv.scene_open && game_mesh_draw)
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
                     const D3D11_TEXTURE2D_DESC desc = GetTexture2DDesc(resource.get());
                     if (desc.SampleDesc.Count == 1 && (desc.BindFlags & D3D11_BIND_SHADER_RESOURCE) != 0 && uint2{desc.Width, desc.Height} == game_device_data.mv.scene_size)
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
            const bool motion_vectors = SceneMotionVectors::DrawWithMotionVectors<MotionVectorGame>(&game_device_data.mv, native_device, native_device_context, cmd_list_data, device_data, original_shader_hashes, *original_draw_dispatch_func, rtvs, dsv.get());
            // An alpha blending state blends the motion vector target by the alpha written there: a draw that doesn't write it (refused
            // above) must not have it bound
            if (!motion_vectors && rtvs[MotionVectorPatches::target_slot] && rtvs[MotionVectorPatches::target_slot] == game_device_data.mv.rtv)
            {
               SceneMotionVectors::ClassifyBoundBlend(native_device_context, &game_device_data.mv);
               if (game_device_data.mv.alpha_blended)
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
            const bool jitter = !motion_vectors && SceneMotionVectors::DrawWithJitter<MotionVectorGame>(&game_device_data.mv, native_device, native_device_context, cmd_list_data, device_data, original_shader_hashes, *original_draw_dispatch_func, rtvs, dsv.get());
#if DEVELOPMENT
            Mcp::Annotate(cmd_list_data, motion_vectors ? "mv" : (jitter ? "jitter" : "unpatched"));
            if (const SceneMotionVectors::MotionVectorReject reject = std::exchange(game_device_data.mv.draw_reject, SceneMotionVectors::REJECT_COUNT); reject != SceneMotionVectors::REJECT_COUNT)
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
         PatchedDraws::RestoreGameShader(native_device_context, &game_device_data.mv.bound_vertex_shader);
         PatchedDraws::RestoreGameShader(native_device_context, &game_device_data.mv.bound_pixel_shader);
      }

      const bool is_bloom_source = shader_hashes_BloomSource.Contains(pixel_shader_hash, reshade::api::shader_stage::pixel);
      const bool is_copy = shader_hashes_Copy.Contains(pixel_shader_hash, reshade::api::shader_stage::pixel);
      if (is_immediate && (is_bloom_source || shader_hashes_BloomBlur.Contains(pixel_shader_hash, reshade::api::shader_stage::pixel) || is_copy) &&
          DrawBloomPass(native_device, native_device_context, cmd_list_data, device_data, stages, is_bloom_source, *original_draw_dispatch_func, &updated_cbuffers))
         return DrawOrDispatchOverrideType::Replaced;

      if (shader_hashes_Grade.Contains(pixel_shader_hash, reshade::api::shader_stage::pixel))
      {
         device_data.has_drawn_main_post_processing = true;
      }
      else if (shader_hashes_SSAO.Contains(pixel_shader_hash, reshade::api::shader_stage::pixel))
      {
         game_device_data.depth_srv.reset();
         native_device_context->PSGetShaderResources(2, 1, game_device_data.depth_srv.put());
         if (g_gtao_enable && is_immediate && RunXeGTAO(native_device, native_device_context, device_data, *original_draw_dispatch_func))
            return DrawOrDispatchOverrideType::Replaced;
      }
      else if (shader_hashes_DgVoodooPresent.Contains(pixel_shader_hash, reshade::api::shader_stage::pixel))
      {
         ComPtr<ID3D11ShaderResourceView> srv;
         native_device_context->PSGetShaderResources(0, 1, srv.put());
         if (srv)
         {
            ComPtr<ID3D11Resource> source;
            srv->GetResource(source.put());
            game_device_data.present_source = uint64_t(source.get());
         }
         bind_luma_cbuffers();
         ID3D11ShaderResourceView* const scene_srv = (game_device_data.has_copied_scene_to_canvas ? game_device_data.scene_srv.get() : nullptr);
         native_device_context->PSSetShaderResources(1, 1, &scene_srv);
         (*original_draw_dispatch_func)();
         ID3D11ShaderResourceView* const null_srv = nullptr;
         native_device_context->PSSetShaderResources(1, 1, &null_srv);
         return DrawOrDispatchOverrideType::Replaced;
      }
      else if (is_custom_pass && (is_copy || shader_hashes_Video.Contains(pixel_shader_hash, reshade::api::shader_stage::pixel)))
      {
         const bool is_video = !is_copy;
         // The bloom composite (FUN_0044bc90): the bloom target blended onto the scene, its mode 1 SRCALPHA/INVSRCALPHA or 2
         // SRCALPHA/ONE (mode 0, opaque, is its copies between levels). The shader scales the source alpha by "Bloom Intensity"
         // (LumaData.CustomData1 2 when it adds, 3 when it lerps); with Luma Bloom it reads "bloom_texture" (CustomData2 1: a B-spline
         // magnification).
         ComPtr<ID3D11BlendState> blend_state;
         native_device_context->OMGetBlendState(blend_state.put(), nullptr, nullptr);
         D3D11_BLEND_DESC blend_desc = {};
         if (blend_state)
         {
            blend_state->GetDesc(&blend_desc);
         }
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
            const D3D11_RENDER_TARGET_BLEND_DESC& blend = blend_desc.RenderTarget[0];
            if (blend.BlendEnable && blend.SrcBlend == D3D11_BLEND_SRC_ALPHA && (blend.DestBlend == D3D11_BLEND_ONE || blend.DestBlend == D3D11_BLEND_INV_SRC_ALPHA))
            {
               ID3D11ShaderResourceView* const luma_bloom_srv = (g_luma_bloom_enable ? game_device_data.bloom_srv.get() : nullptr);
               bind_luma_cbuffers((blend.DestBlend == D3D11_BLEND_ONE ? 2 : 3), luma_bloom_srv ? 1 : 0);
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
         if (!is_video && (game_device_data.has_copied_scene_to_canvas || blend_desc.RenderTarget[0].BlendEnable))
         {
            return DrawOrDispatchOverrideType::None;
         }
         const ComPtr<ID3D11RenderTargetView> rtv = bound_canvas_rtv();
         if (!rtv)
         {
            return DrawOrDispatchOverrideType::None;
         }
         // The movies' AutoHDR goes above 1, which the present blit's clamp must keep
         return draw_and_snapshot_canvas(rtv.get(), reinterpret_cast<ID3D11Resource*>(uint64_t(game_device_data.canvas)), is_video ? 0 : 1);
      }
      // MSAA (4x and up, as Mirror's Edge: 2x gives a coarser edge than the alpha test): the alpha tested materials' draws into the
      // multisampled scene with their alpha to coverage version, the game's shader put back after. The coverage version keeps the
      // in place "PatchShaderBytecodeSync" clamp of the float target 0 shaders.
      else if (g_msaa_resolve_enable && g_msaa_coverage_enable && game_device_data.msaa_active && !is_custom_pass && original_shader_hashes.pixel_shaders[0] != SHADER_HASH_NONE)
      {
         // The target first: most draws with MSAA on (the UI, post) aren't multisampled, and need no shader lookup
         ComPtr<ID3D11RenderTargetView> rtv;
         native_device_context->OMGetRenderTargets(1, rtv.put(), nullptr);
         ComPtr<ID3D11Resource> rt;
         if (rtv)
         {
            rtv->GetResource(rt.put());
         }
         if (GetTexture2DDesc(rt.get()).SampleDesc.Count >= 4)
         {
            if (ID3D11PixelShader* const coverage_shader = SceneMotionVectors::GetPatchedShader<MotionVectorGame, ID3D11PixelShader>(&game_device_data.mv, native_device, device_data, pixel_shader_hash, cmd_list_data.pipeline_state_original_pixel_shader, COVERAGE_PATCH).shader.get())
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
      const bool mv_was_active = game_device_data.mv.active.exchange((game_device_data.sr_active || game_device_data.t2x_active || g_mv_enable) && !game_device_data.msaa_active);
      if (game_device_data.mv.active && !mv_was_active)
      {
         game_device_data.mv.constants.Restart();
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
            const std::unique_lock lock(game_device_data.mv.mutex);
            game_device_data.mv.ReleaseMotionVectorTargets();
            // The game's scene, depth, prepass and scene copy (taken again at the next scene), so a resize after None doesn't keep the
            // old ones alive; and the vc4 copies the object tables and the cameras hold
            game_device_data.mv.depth.reset();
            game_device_data.mv.scene_color.reset();
            game_device_data.mv.scene_rtv.reset();
            game_device_data.mv_scene_copy.reset();
            game_device_data.mv_scene_last_copy.reset();
            game_device_data.mv_scene_copy_rtv.reset();
            game_device_data.prepass_depth.reset();
            game_device_data.prepass_depth_srv.reset();
            game_device_data.mv.scene_size = {};
            game_device_data.mv.objects.Reset();
            game_device_data.mv.camera.reset();
            game_device_data.mv.previous_camera.reset();
            game_device_data.mv.fill_buffer.reset();
            game_device_data.mv.jitter_buffer.reset();
            game_device_data.mv.previous_constants = {}; // Its 4 MiB dynamic ring is address space too
            game_device_data.mv.constants.ReleasePool();
         }
      }
      game_device_data.mv.constants.TrimPool(game_device_data.mv.active);
#if DEVELOPMENT
      game_device_data.mv.stats.constants_pool = uint32_t(game_device_data.mv.constants.pool.size());
      game_device_data.mv.bound_states.check = g_bound_state_check;
      game_device_data.mv.stats.bound_state_mismatches += std::exchange(game_device_data.mv.bound_states.mismatches, 0);
#endif
      // The reactive masks go once the fill stopped writing them (DLSS, the masks off, SMAA)
      if (game_device_data.mv.reactive_target && cb_luma_global_settings.FrameIndex - game_device_data.mv.sr_reactive_frame > IDLE_RELEASE_FRAMES)
      {
         const std::unique_lock lock(game_device_data.mv.mutex);
         game_device_data.mv.ReleaseReactiveMasks();
      }
      // A scene no post pass ended ends here: its jitter must not reach the next frame's draws before its first mesh
      game_device_data.mv.scene_open = false;
      game_device_data.mv.scene_done = false;
      game_device_data.mv.frame_ended = true;
      game_device_data.mv.fill_pending = false;
      game_device_data.open_refused_rtv = nullptr;
      game_device_data.open_refused_dsv = nullptr;
      // -1 at native resolution once the upscaler draws (Core biases the game's samplers at bias 0, the trilinear ones upgraded to
      // AF16x, see "DllMain"); none while the bridge's helper starts (SMAA on the unjittered scene)
      SetTextureMipLodBias(nullptr, device_data, ((game_device_data.sr_active && game_device_data.mv_jitter_allowed) ? SR::GetMipLODBias(device_data.output_resolution.y, device_data.output_resolution.y) : 0.f));
#if DEVELOPMENT
      game_device_data.mv.last_stats = std::exchange(game_device_data.mv.stats, {});
      // "MV Debug View": Core's debug draw of the target, absolute values in pixels
      {
         const std::shared_lock lock(game_device_data.mv.mutex);
         if (g_mv_debug_view && game_device_data.mv.texture)
         {
            D3D11_TEXTURE2D_DESC desc;
            game_device_data.mv.texture->GetDesc(&desc);
            debug_draw_auto_clear_texture = false;
            debug_draw_options |= (uint32_t)DebugDrawTextureOptionsMask::Abs | (uint32_t)DebugDrawTextureOptionsMask::UVToPixelSpace;
            device_data.debug_draw_texture = game_device_data.mv.texture.get();
            device_data.debug_draw_texture_format = desc.Format;
            device_data.debug_draw_texture_size = {desc.Width, desc.Height, 1, 1};
         }
         else if (g_sr_reactive_debug_view && game_device_data.mv.reactive)
         {
            debug_draw_auto_clear_texture = false;
            debug_draw_options |= (uint32_t)DebugDrawTextureOptionsMask::RedOnly;
            debug_draw_options &= ~((uint32_t)DebugDrawTextureOptionsMask::Abs | (uint32_t)DebugDrawTextureOptionsMask::UVToPixelSpace);
            device_data.debug_draw_texture = game_device_data.mv.reactive.get();
            device_data.debug_draw_texture_format = DXGI_FORMAT_R8_UNORM;
            device_data.debug_draw_texture_size = {game_device_data.mv.scene_size.x, game_device_data.mv.scene_size.y, 1, 1};
         }
         else if (device_data.debug_draw_texture && (device_data.debug_draw_texture.get() == game_device_data.mv.texture.get() || device_data.debug_draw_texture.get() == game_device_data.mv.reactive.get()))
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
      // The upscaler (Super Resolution, in the Settings tab) replaces SMAA: greyed, its value kept. Not with MSAA ("sr_active"): SMAA
      // runs then.
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
      DrawSettingCheckbox("Luma Bloom Enable", "LumaBloomEnable", &g_luma_bloom_enable, true, "Replaces the game's bloom with a higher resolution one (the same look, without its blocky, flickering glow).");
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
         "Keeps shadow softness, depth of field and SSAO the size they had at 1280x720, the resolution the game was made for.");
      // Only the Steam executable's limiters are hooked ("FrameLimiter::Install")
      ImGui::BeginDisabled(!FrameLimiter::installed);
      DrawSettingCheckbox("Fix Frame Pacing", "FixFramePacing", &g_fix_frame_pacing, true,
         "Replaces the game's frame limiter with a precise one. Turn off an external frame rate limit with it.", !FrameLimiter::installed);
      ImGui::EndDisabled();
      // Patched from the limiter hooks, on the Steam executable only ("FrameLimiter::Apply60FpsCutscenes")
      ImGui::BeginDisabled(!FrameLimiter::cutscenes_patchable);
      DrawSettingCheckbox("60 FPS Cutscenes", "60FPSCutscenes", &g_60_fps_cutscenes, true,
         "Draws the real-time cutscenes at 60 fps instead of 30.", !FrameLimiter::cutscenes_patchable);
      ImGui::EndDisabled();
#if DEVELOPMENT
      ImGui::Text("Frame limiter: period %u us, spin margin %u us, late %u, resyncs %u, adopted %u", FrameLimiter::period_us, FrameLimiter::spin_margin_us, FrameLimiter::late_frames, FrameLimiter::resyncs, FrameLimiter::adopted_deadlines);

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
         "It adds HDR, DLAA or FSR 3 native anti-aliasing, SMAA (with T2x) and an HDR-aware MSAA with alpha to coverage, and replaces the game's bloom with a higher resolution one and its SSAO with XeGTAO, plus higher resolution shadows, even frame pacing and 60 fps cutscenes.\n"
         "It runs through dgVoodoo2 (DirectX 9 -> 11).\n"
         "Enable SSAO in the game's options for XeGTAO to apply; SMAA works either way.\n"
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

      // The upscalers' mip bias. Mode 3: only samplers at bias 0 get it, the game already sets -1 on its material samplers (seen in a
      // trace) and adding ours would make -2. force_upgrade_linear_samplers also gives the trilinear ones the bias (Core rewrites only
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
      for (const uint32_t hash : shader_hashes_Copy.pixel_shaders)
      {
         auto_texture_format_upgrade_shader_hashes[hash] = {.rtv_slots = {0}};
      }

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
              {"C60649FD", "Video Y'CbCr"},
              {"ED0DA20F", "Video Y'CbCr"},
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
      FrameLimiter::Uninstall();
      ShadowMap::Uninstall();
   }

   CoreMain(hModule, ul_reason_for_call, lpReserved);

   return TRUE;
}
