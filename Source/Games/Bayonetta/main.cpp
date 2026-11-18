// Bayonetta (PC, 2017) is x86 D3D9: Luma runs on the game's own device
// (upgraded to D3D9Ex) through Core's native D3D9 module
// (Source/Core/d3d9/core_d3d9.hpp), which hands the frame to an in-process
// D3D12 presenter. This is the game half: motion vectors and jitter for the SR
// bridge / Luma TAA / SMAA T2x, XeGTAO, Luma Bloom, Hide UI, the c208 Luma
// block's host rows, the kernels' c194, the frame limiter, the shadow map size
// and the settings. Pixel shaders are identified by the CRC32 of their SM3
// bytecode (ShaderHashes.h). Single game thread assumed (no CSMT).
#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

// clang-format off
#include "../../../Shaders/Bayonetta/Includes/GameCBuffers.hlsl" // CB::LumaGameSettings, before Core's cbuffers.h
#include "../../Core/d3d9/core_d3d9.hpp"
// clang-format on
#include "../../Core/utils/system.h"
#include "ShaderHashes.h"

#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")

// The settings (ini section NAME, the dgVoodoo mod's keys), read in
// "Bayonetta::OnInit" and given to Core's state by "ApplySettings"
static bool g_smaa_enable = true;
static bool g_smaa_t2x = false; // SMAA T2x: a two phase jitter and the previous
                                // frame, through the motion vectors
static bool g_smaa_predication =
   true; // On geometry, from the depth prepass's edges
static float g_smaa_pred_tolerance =
   0.02f;                            // A fraction of view depth (TW2/ME1's)
static float g_rcas_sharpness = 0.f; // RCAS on the antialiased canvas, 0 = off
static bool g_hide_ui = false;       // Session-only, never persisted: a stuck "on"
                                     // would look like a broken HUD
// The pixel sized kernels at their 1280x720 size on screen (ShaderHashes.h
// kPixelKernelPs); off: the PC version's, smaller as the resolution rises
static bool g_resolution_scaled_effects = true;
static bool g_fix_frame_pacing =
   true;                                // The game's frame limiters replaced (see "FrameLimiter")
static bool g_60_fps_cutscenes = true;  // The loop's 30 fps bit kept clear (see
                                        // "FrameLimiter::Apply60FpsCutscenes")
static bool g_luma_bloom_enable = true; // The game's bloom chain drawn at a
                                        // higher resolution (see "BloomDraw")
static bool g_high_resolution_shadows =
   true; // An 8192 shadow map at the highest shadow quality (see "ShadowMap")
static bool g_gtao_enable =
   true; // XeGTAO replaces the game's SSAO (see "RunGtao")
// Darkness dial (higher = darker): about halfway between power 1's and that of
// the power that matched the thin contact lines of the game's SSAO (Fix High
// Resolution Effects on) on the lit station platform. Other scenes differ
// (NOTES.md).
static float g_gtao_final_value_power = 0.55f;
static float g_gtao_depth_scale =
   1.f; // View depth divisor (game units -> ~meters)
static float g_gtao_radius_override =
   0.f; // > 0 overrides the shader's EFFECT_RADIUS (view units after the depth
        // scale)
// GT-VBAO's visibility bitmask main pass instead of XeGTAO's horizons, with its own darkness dial (about 0.18 matched the game's SSAO
// where XeGTAO's 0.3 did, NOTES.md)
static bool g_gtvbao_enable = false;
static float g_gtvbao_final_value_power = 0.35f;
// XE_GTAO_QUALITY of the pass device's XeGTAO (slice count, 0 Low .. 4 Ultra); a change ("gtao_defines") reopens the pass device at its
// next use
static int g_gtao_quality = 3;
static std::vector<std::string> GtaoDefines()
{
   return {"XE_GTAO_QUALITY=" + std::to_string(g_gtao_quality)};
}
// The XeGTAO and GT-VBAO darkness dials' range (sliders and dev values)
constexpr float GTAO_FINAL_VALUE_POWER_MIN = 0.1f;
constexpr float GTAO_FINAL_VALUE_POWER_MAX = 4.5f;
#if DEVELOPMENT
static int g_gtao_debug_view = 0; // 0 off, 1 depth gradient, 2 normals, 3 AO x8, 4 edges (the shader's debug blocks are DEVELOPMENT only)
// The AO history's weight under an upscaler (see "RunGtao"), 0 turns it off
static float g_gtao_history_weight = 0.9f;
#else
constexpr int g_gtao_debug_view = 0;
constexpr float g_gtao_history_weight = 0.9f;
#endif
// Read in the SM3 ports through the Luma block ("UpdateLumaRows"), the display
// map and the composition (Core's state)
constexpr CB::LumaGameSettings DEFAULT_GAME_SETTINGS = {
   .Dithering = 1.f,
   // Light AutoHDR on the movies (the TW1/ME1 pair: at 0.5 the peak is 165
   // nits)
   .VideoAutoHDREnable = 1.f,
   .VideoAutoHDRBoost = 0.5f,
   .Exposure = 1.f,
   .Contrast = 1.f,
   .Saturation = 1.f,
   .HighlightsDesaturation = 0.f,
   .ColorGradingIntensity = 1.f,
   .BloomIntensity = 1.f};

// The Steam executable, whose fixed addresses the game code patches below use
// (relocations stripped: fixed base)
static bool IsSteamExecutable()
{
   constexpr DWORD STEAM_TIME_DATE_STAMP = 0x591AD330;
   const auto* const image =
      reinterpret_cast<const uint8_t*>(GetModuleHandleW(nullptr));
   return reinterpret_cast<const IMAGE_NT_HEADERS*>(
             image +
             reinterpret_cast<const IMAGE_DOS_HEADER*>(image)->e_lfanew)
             ->FileHeader.TimeDateStamp == STEAM_TIME_DATE_STAMP;
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
   float* const NEXT_DEADLINE_MS = reinterpret_cast<float*>(0x5BB9528);
   float* const PREVIOUS_DEADLINE_MS = reinterpret_cast<float*>(0x5BB9524);
   uint32_t* const LATE = reinterpret_cast<uint32_t*>(0x5BB9578);
   int32_t* const LATE_COUNT = reinterpret_cast<int32_t*>(0x5BB957C); // +1 per late frame at 60 fps, +2 at 30
   const uint32_t* const RESYNC_ALLOWED = reinterpret_cast<const uint32_t*>(0xF45CC8);
   const int64_t* const TIME_BASE = reinterpret_cast<const int64_t*>(0x5BCFBF8); // The QPC at the game's start (FUN_00c78090)
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
      const float game_next_ms = *NEXT_DEADLINE_MS;
      *PREVIOUS_DEADLINE_MS = game_next_ms;
      if (game_next_ms != written_next_ms)
      {
         next = *TIME_BASE + int64_t(double(game_next_ms) * ticks_per_ms);
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
      *LATE_COUNT = (is_late ? *LATE_COUNT + int32_t(frames) : 0);
      *LATE = is_late;
#if DEVELOPMENT
      late_frames += is_late;
#endif
      if (*RESYNC_ALLOWED && *LATE_COUNT > int32_t(4 * frames - 1))
      {
         *LATE_COUNT = 0;
         next = now;
#if DEVELOPMENT
         resyncs++;
#endif
      }
      next += period;
      written_next_ms = float(double(next - *TIME_BASE) / ticks_per_ms);
      *NEXT_DEADLINE_MS = written_next_ms;
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
// receivers' replacements filter at the map's real size ("Includes/ShadowReceive_sm3.hlsli").
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

// Bayonetta's draw-time state
static struct
{
   // The SSAO pass (env D9N_SSAO_PS adds CRCs): its s2 is the R32F depth prepass
   // (1 - z/w) of the scene rect, SMAA's predication source. Valid for the frame
   // it was drawn in.
   std::set<uint32_t> ssao_crcs{std::begin(kSsaoPs), std::end(kSsaoPs)};
   uint64_t depth_texture = 0;
   uint32_t depth_present = UINT32_MAX;
   // Pixel kernels whose c194 becomes the kernel target size at their draw (env
   // D9N_KERNEL_PS adds CRCs)
   std::set<uint32_t> kernel_crcs{std::begin(kPixelKernelPs),
      std::end(kPixelKernelPs)};
   bool c194_overridden = false;
} bay;

// ===========================================================================
// Motion vectors and jitter for the SR bridge (env D9N_MV=1, implied by
// D9N_SR). Port of Core's patched-draw scheme to native D3D9 (plan:
// resilient-whistling-zebra.md):
//  * init_pipeline: every VS / PS goes through sm3_mv_patch.h and the patched
//    copy is created natively (the VS runs twice: current and previous camera /
//    world / bones from a vertex texture; the PS writes the UV delta to oC1).
//  * The scene opens at the R32F depth prepass (RT1 bound, full viewport, depth
//    of the output size or its half) and ends at the first post-pass PS. Draws
//    into the prepass RT / the fp16 scene RT with the scene depth get the
//    jittered patched VS; opaque ones with a patched PS also write motion into
//    our G16R16F target (RT1).
//  * Previous constants per draw: Core's key + tie-break + FindNearest, with
//    the camera fallbacks.
//  * Scene end: fill pass (marker texels -> camera motion, depth from the
//    prepass) into the shared SR textures, then the SR bridge, whose output is
//    copied back RGB-only into the scene RT and the game's scene copy.
// There is no post-draw event on D3D9: the patched state is put back lazily at
// the next event that could observe it (next draw, render target bind, clear,
// present), see MvRestoreGame.
// ===========================================================================
static bool g_mv = false;

struct Mat4
{
   double m[4][4] = {};
};
static Mat4 MatFromRows(const float* rows)
{
   Mat4 result;
   for (int row = 0; row < 4; ++row)
   {
      for (int column = 0; column < 4; ++column)
      {
         result.m[row][column] = rows[row * 4 + column];
      }
   }
   return result;
}
static Mat4 MatIdentity()
{
   Mat4 result;
   for (int i = 0; i < 4; ++i)
   {
      result.m[i][i] = 1.0;
   }
   return result;
}
static Mat4 MatMul(const Mat4& a, const Mat4& b)
{
   Mat4 result;
   for (int row = 0; row < 4; ++row)
   {
      for (int column = 0; column < 4; ++column)
      {
         for (int k = 0; k < 4; ++k)
         {
            result.m[row][column] += a.m[row][k] * b.m[k][column];
         }
      }
   }
   return result;
}
static bool MatInvert(const Mat4& input, Mat4* inverse)
{
   double a[4][8] = {};
   for (int row = 0; row < 4; ++row)
   {
      for (int column = 0; column < 4; ++column)
      {
         a[row][column] = input.m[row][column];
      }
      a[row][4 + row] = 1.0;
   }
   for (int column = 0; column < 4; ++column)
   {
      int pivot = column;
      for (int row = column + 1; row < 4; ++row)
      {
         if (fabs(a[row][column]) > fabs(a[pivot][column]))
         {
            pivot = row;
         }
      }
      if (fabs(a[pivot][column]) < 1e-300)
      {
         return false;
      }
      for (int k = 0; k < 8; ++k)
      {
         std::swap(a[column][k], a[pivot][k]);
      }
      const double scale = 1.0 / a[column][column];
      for (int k = 0; k < 8; ++k)
      {
         a[column][k] *= scale;
      }
      for (int row = 0; row < 4; ++row)
      {
         if (row == column)
         {
            continue;
         }
         const double factor = a[row][column];
         for (int k = 0; k < 8; ++k)
         {
            a[row][k] -= factor * a[column][k];
         }
      }
   }
   for (int row = 0; row < 4; ++row)
   {
      for (int column = 0; column < 4; ++column)
      {
         inverse->m[row][column] = a[row][4 + column];
      }
   }
   return true;
}

// SR::GetViewProjectionCamera (super_resolution.h:170-190), row_vectors false
// (clip component i = row i of the 4x4 at the camera register).
struct MvCamera
{
   double vert_fov = 0.0, near_plane = 0.0, far_plane = 0.0;
};
static MvCamera MvViewProjectionCamera(const float* m)
{
   const auto at = [&](int row, int column)
   {
      return double(m[row * 4 + column]);
   };
   double dot_23 = 0.0, dot_33 = 0.0;
   for (int column = 0; column < 3; ++column)
   {
      dot_23 += at(2, column) * at(3, column);
      dot_33 += at(3, column) * at(3, column);
   }
   const double depth_a = dot_33 > 0.0 ? dot_23 / dot_33 : 1.0;
   const double depth_b = at(2, 3) - depth_a * at(3, 3);
   const double up_length =
      sqrt(at(1, 0) * at(1, 0) + at(1, 1) * at(1, 1) + at(1, 2) * at(1, 2));
   MvCamera camera;
   camera.vert_fov = up_length > 0.0 ? 2.0 * atan(1.0 / up_length) : 0.0;
   camera.near_plane = depth_a != 0.0 ? -depth_b / depth_a : 0.0;
   if (camera.near_plane > 0.0)
   {
      camera.far_plane = depth_a > 1.0 + 1e-6 ? depth_b / (1.0 - depth_a)
                                              : camera.near_plane * 1e6;
   }
   return camera;
}

// SR::HaltonSequence: [-0.5, 0.5] pixels, index from 0.
static float MvHalton(unsigned index, unsigned base)
{
   index += 1;
   float result = 0.0f;
   const float inv_base = 1.0f / float(base);
   float fraction = inv_base;
   while (index > 0)
   {
      result += float(index % base) * fraction;
      index /= base;
      fraction *= inv_base;
   }
   return result - 0.5f;
}
constexpr unsigned MV_JITTER_PHASES = 8; // SR::GetDefaultJitterPhases (1x)

constexpr uint32_t MV_VTF_WIDTH = Sm3MvPatch::kBoneTexelCount; // texels
constexpr uint32_t MV_BONE_FLOATS = MV_VTF_WIDTH * 4;
constexpr int MV_VS_CONSTANTS = 256;

struct MvShader
{
   IDirect3DVertexShader9* vs = nullptr; // patched copy
   IDirect3DPixelShader9* ps = nullptr;
   Sm3MvPatch::VertexInfo info = {};
   uint32_t crc = 0; // CRC32 of the game's (original) bytecode
};

// One draw of a frame (Core's MotionVectorObject): what the next frame's same
// draw takes as its previous constants.
struct MvObject
{
   std::array<float, 12> transform = {};
   std::array<float, 16> camera = {}, world = {}, effect_world = {};
   std::vector<float> bones; // 144 float4 (skinned VS only)
   int row = -1;             // its row in this frame's previous-bones texture
};
using MvObjectMap = std::unordered_map<uint64_t, std::vector<MvObject>>;

struct Mv
{
   std::map<uint64_t, MvShader> shaders; // by pipeline handle (VS and PS)
   std::set<uint32_t> post_crcs{std::begin(kScenePostPs), std::end(kScenePostPs)},
      refraction_crcs{std::begin(kRefractionPs), std::end(kRefractionPs)};
   uint64_t game_vs = 0, game_ps = 0;
   float vs_const[MV_VS_CONSTANTS * 4] = {};
   // Render targets as the game bound them (events), without our own RT1
   uint64_t rt_view[2] = {}, dsv = 0;
   bool other_rt = false;
   format rt0_format = format::unknown;
   uint32_t rt0_w = 0, rt0_h = 0, dsv_w = 0, dsv_h = 0;
   resource rt0_resource = {0};
   // State we changed since the game's last bind
   bool applied_vs = false, applied_ps = false, applied_rt1 = false;
   DWORD saved_cwe1 = 0xF;
   // Resources (DEFAULT pool: freed by DestroyMv before a Reset)
   IDirect3DTexture9* textures[4] = {}; // shared: motion, depth, output, color
   HANDLE kmt[4] = {};
   UINT tex_w = 0, tex_h = 0;
   IDirect3DTexture9* motion_rt = nullptr; // our RT1
   IDirect3DSurface9* motion_surface = nullptr;
   IDirect3DTexture9* vtf = nullptr;
   UINT vtf_rows = 0;
   IDirect3DStateBlock9* block = nullptr;
   IDirect3DSurface9 *probe_motion = nullptr, *probe_depth = nullptr,
                     *probe_raw = nullptr;
   IDirect3DVertexShader9* vs_quad = nullptr;
   IDirect3DPixelShader9 *ps_fill = nullptr, *ps_marker = nullptr,
                         *ps_copy = nullptr, *ps_gtao = nullptr,
                         *ps_gtao_debug = nullptr; // GTAO_DEBUG_VIEW 1: XeGTAO's debug views alone
   bool shaders_tried = false, vtf_checked = false, disabled = false;
   bool ps_gtao_tried = false, ps_gtao_debug_tried = false; // One load per variant until "MvFreeShaders()"
   // XeGTAO's camera (Bayonetta's TakePrepassCamera): the last depth prepass
   // draw's c0..c27 as rows 0..27 of the shader's b4 (dgVoodoo's b4 has them at
   // 20..47), so its rows are the D3D9 registers; the frame it was taken in
   float gtao_camera[28 * 4] = {};
   uint32_t gtao_affine_row = 0, gtao_matrix_row = 0;
   uint32_t gtao_camera_frame = UINT32_MAX;
   uint32_t gtao_frame = UINT32_MAX; // The last frame XeGTAO ran in (its history restarts after a frame without it)
   // Frame
   uint32_t frame_index = 0, scene_frame = 0;
   bool open = false, ended = false, sr_ran = false, sr_ran_last = false;
   // The fill wrote this frame's motion vectors; SMAA T2x resolved this / last
   // frame (Bayonetta's t2x_frame) and this scene's jitter phase (-1 none)
   bool filled = false, t2x_ran = false, t2x_ran_last = false;
   int t2x_phase = -1;
   uint64_t refused_rt0 = 0;
   uint64_t scene_depth = 0, scene_view = 0, prepass_view = 0;
   IDirect3DSurface9* scene_surface = nullptr; // AddRef-ed while the scene lives
   resource scene_resource = {0}, copy_dest = {0};
   IDirect3DTexture9* prepass = nullptr; // AddRef-ed
   UINT scene_w = 0, scene_h = 0, last_w = 0, last_h = 0;
   float jitter[2] = {}, jitter_ndc[2] = {};
   bool camera_valid = false, previous_valid = false;
   std::array<float, 16> camera = {}, previous_camera = {};
   Mat4 reprojection = MatIdentity();
   MvObjectMap current, previous;
};
static Mv mv;

static void MvReleaseShader(uint64_t handle)
{
   const auto it = mv.shaders.find(handle);
   if (it == mv.shaders.end())
   {
      return;
   }
   if (it->second.vs)
   {
      it->second.vs->Release();
      --state.mv_shaders_live;
   }
   if (it->second.ps)
   {
      it->second.ps->Release();
      --state.mv_shaders_live;
   }
   mv.shaders.erase(it);
}

// init_pipeline: patch and create natively (D3D9 runtime only). Env D9N_MV off:
// nothing happens.
static void MvInitPipeline(device* host, uint32_t count,
   const pipeline_subobject* subobjects,
   pipeline handle)
{
   IDirect3DDevice9* native = NativeOf(host);
   MvReleaseShader(handle.handle);
   for (uint32_t i = 0; i < count; ++i)
   {
      const bool vertex =
         subobjects[i].type == pipeline_subobject_type::vertex_shader;
      if (!vertex &&
          subobjects[i].type != pipeline_subobject_type::pixel_shader)
      {
         continue;
      }
      const auto* desc = static_cast<const shader_desc*>(subobjects[i].data);
      if (!desc->code || desc->code_size < 8)
      {
         continue;
      }
      const auto* tokens = static_cast<const uint32_t*>(desc->code);
      const size_t token_count = desc->code_size / 4;
      MvShader entry;
      entry.crc = vertex ? Crc32(desc->code, desc->code_size) : out.pending_crc;
      std::vector<uint32_t> patched;
      std::string error;
      bool ok = vertex ? Sm3MvPatch::PatchVertexShader(
                            tokens, token_count, Sm3MvPatch::VsMode::Production,
                            &patched, &entry.info, &error, 512)
                       : Sm3MvPatch::PatchPixelShader(tokens, token_count,
                            &patched, &error);
      if (ok)
      {
         const HRESULT hr =
            vertex
               ? native->CreateVertexShader(
                    reinterpret_cast<const DWORD*>(patched.data()), &entry.vs)
               : native->CreatePixelShader(
                    reinterpret_cast<const DWORD*>(patched.data()), &entry.ps);
         if (FAILED(hr))
         {
            ok = false;
            error = "native create failed 0x" + std::to_string(uint32_t(hr));
         }
      }
      uint32_t& patched_count =
         vertex ? state.mv_patched_vs : state.mv_patched_ps;
      uint32_t& refused_count =
         vertex ? state.mv_refused_vs : state.mv_refused_ps;
      if (ok)
      {
         ++patched_count;
         ++state.mv_shaders_live;
      }
      else if (++refused_count <= 8)
      {
         Log("d9native: MV patch refused a %s %08X: %s", vertex ? "VS" : "PS",
            entry.crc, error.c_str());
      }
      // Both stages are one pipeline each in D3D9: the entry holds the patched
      // shader (or only the CRC when refused: the key needs it).
      if (!ok)
      {
         entry.vs = nullptr;
         entry.ps = nullptr;
      }
      mv.shaders[handle.handle] = entry;
   }
}

static void MvDestroyPipeline(uint64_t handle)
{
   MvReleaseShader(handle);
}

// Puts the game's shaders, RT1 and COLORWRITEENABLE1 back after a patched
// draw. Cheap when nothing was changed.
static void MvRestoreGame(IDirect3DDevice9* native)
{
   if (mv.applied_vs)
   {
      native->SetVertexShader(
         reinterpret_cast<IDirect3DVertexShader9*>(mv.game_vs));
      mv.applied_vs = false;
   }
   if (mv.applied_ps)
   {
      native->SetPixelShader(BoundPixelShader(mv.game_ps));
      mv.applied_ps = false;
   }
   if (mv.applied_rt1)
   {
      native->SetRenderTarget(1, nullptr);
      native->SetRenderState(D3DRS_COLORWRITEENABLE1, mv.saved_cwe1);
      mv.applied_rt1 = false;
   }
}

// Save / restore of everything a pass of ours touches. The state block holds
// the shaders, constants, textures, samplers and render states; the render
// targets, depth surface and viewport are not part of it.
struct PassScope
{
   IDirect3DDevice9* native;
   IDirect3DSurface9 *rt0 = nullptr, *rt1 = nullptr, *depth = nullptr;
   D3DVIEWPORT9 viewport = {};
   explicit PassScope(IDirect3DDevice9* device) : native(device)
   {
      if (!mv.block)
      {
         native->CreateStateBlock(D3DSBT_ALL, &mv.block);
      }
      if (mv.block)
      {
         mv.block->Capture();
      }
      native->GetRenderTarget(0, &rt0);
      native->GetRenderTarget(1, &rt1);
      native->GetDepthStencilSurface(&depth);
      native->GetViewport(&viewport);
      // A pass: opaque, no depth, no culling, all channels
      static const std::pair<D3DRENDERSTATETYPE, DWORD> pass_states[] = {
         {D3DRS_ZENABLE, 0},
         {D3DRS_ZWRITEENABLE, 0},
         {D3DRS_ALPHABLENDENABLE, 0},
         {D3DRS_SEPARATEALPHABLENDENABLE, 0},
         {D3DRS_ALPHATESTENABLE, 0},
         {D3DRS_CULLMODE, D3DCULL_NONE},
         {D3DRS_STENCILENABLE, 0},
         {D3DRS_SCISSORTESTENABLE, 0},
         {D3DRS_SRGBWRITEENABLE, 0},
         {D3DRS_FILLMODE, D3DFILL_SOLID},
         {D3DRS_COLORWRITEENABLE, 0xF},
         {D3DRS_COLORWRITEENABLE1, 0xF}};
      for (const auto& [render_state, value] : pass_states)
      {
         native->SetRenderState(render_state, value);
      }
   }
   ~PassScope()
   {
      if (mv.block)
      {
         mv.block->Apply();
      }
      native->SetRenderTarget(0, rt0);
      native->SetRenderTarget(1, rt1);
      native->SetDepthStencilSurface(depth);
      native->SetViewport(&viewport);
      Free(&rt0);
      Free(&rt1);
      Free(&depth);
   }
   PassScope(const PassScope&) = delete;
   PassScope& operator=(const PassScope&) = delete;
};

// Full screen triangle with vs_quad + `pixel_shader` into the bound targets.
static void MvDrawQuad(IDirect3DDevice9* native, UINT width, UINT height,
   IDirect3DPixelShader9* pixel_shader)
{
   static const float triangle[] = {-1, -1, 0, -1, 3, 0, 3, -1, 0};
   const D3DVIEWPORT9 viewport = {0, 0, width, height, 0, 1};
   native->SetViewport(&viewport);
   native->SetFVF(D3DFVF_XYZ);
   native->SetVertexShader(mv.vs_quad);
   native->SetPixelShader(pixel_shader);
   native->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 1, triangle, sizeof(float) * 3);
}

static void MvSetPointClamp(IDirect3DDevice9* native, DWORD sampler,
   IDirect3DBaseTexture9* texture)
{
   native->SetTexture(sampler, texture);
   for (D3DSAMPLERSTATETYPE type :
      {D3DSAMP_MINFILTER, D3DSAMP_MAGFILTER, D3DSAMP_MIPFILTER})
   {
      native->SetSamplerState(sampler, type, D3DTEXF_POINT);
   }
   native->SetSamplerState(sampler, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
   native->SetSamplerState(sampler, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
   native->SetSamplerState(sampler, D3DSAMP_SRGBTEXTURE, 0);
}

// One-time: the four pass shaders (Luma_Bayonetta_MotionVectorFill_SM3.hlsl,
// Luma_Bayonetta_RGBCopy_SM3.hlsl).
// Reloaded through "OnShadersReloaded()", as is the GTAO apply PS.
static bool MvLoadShaders(IDirect3DDevice9* native)
{
   if (mv.shaders_tried)
   {
      return mv.vs_quad && mv.ps_fill && mv.ps_marker && mv.ps_copy;
   }
   mv.shaders_tried = true;
   const std::string fill_file = "Bayonetta/Luma_Bayonetta_MotionVectorFill_SM3.hlsl";
   const std::vector<uint8_t>
      quad = LoadShader(L"mv_quad.vs_3_0.cso", fill_file, "vs_quad", "vs_3_0"),
      fill = LoadShader(L"mv_fill.ps_3_0.cso", fill_file, "ps_fill", "ps_3_0"),
      marker = LoadShader(L"mv_marker.ps_3_0.cso", fill_file, "ps_marker", "ps_3_0"),
      copy = LoadShader(L"rgb_copy.ps_3_0.cso",
         "Bayonetta/Luma_Bayonetta_RGBCopy_SM3.hlsl", "main",
         "ps_3_0");
   const bool ok =
      !quad.empty() && !fill.empty() && !marker.empty() && !copy.empty() &&
      SUCCEEDED(native->CreateVertexShader(
         reinterpret_cast<const DWORD*>(quad.data()), &mv.vs_quad)) &&
      SUCCEEDED(native->CreatePixelShader(
         reinterpret_cast<const DWORD*>(fill.data()), &mv.ps_fill)) &&
      SUCCEEDED(native->CreatePixelShader(
         reinterpret_cast<const DWORD*>(marker.data()), &mv.ps_marker)) &&
      SUCCEEDED(native->CreatePixelShader(
         reinterpret_cast<const DWORD*>(copy.data()), &mv.ps_copy));
   Log("d9native: MV pass shaders %s", ok ? "loaded" : "FAILED to load");
   return ok;
}

// A32B32G32R32F vertex texture fetch (the previous bones), else MV is off.
static bool MvCheckVtf(IDirect3DDevice9* native)
{
   if (mv.vtf_checked)
   {
      return state.mv_vtf_ok != 0;
   }
   mv.vtf_checked = true;
   IDirect3D9* d3d = nullptr;
   D3DDISPLAYMODE mode = {};
   D3DCAPS9 caps = {};
   HRESULT hr = native->GetDirect3D(&d3d);
   if (SUCCEEDED(hr))
   {
      native->GetDisplayMode(0, &mode);
      native->GetDeviceCaps(&caps);
      hr = d3d->CheckDeviceFormat(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, mode.Format,
         D3DUSAGE_QUERY_VERTEXTEXTURE, D3DRTYPE_TEXTURE,
         D3DFMT_A32B32G32R32F);
      d3d->Release();
   }
   state.mv_vtf_ok = SUCCEEDED(hr);
   state.mv_indep_mrt =
      (caps.PrimitiveMiscCaps & D3DPMISCCAPS_MRTINDEPENDENTBITDEPTHS) != 0;
   if (!state.mv_vtf_ok)
   {
      Log("d9native: MV disabled: no A32B32G32R32F vertex texture fetch "
          "(CheckDeviceFormat 0x%08X)",
         uint32_t(hr));
   }
   Log("d9native: MV: vertex texture A32B32G32R32F %s, MRT independent bit "
       "depths %d",
      state.mv_vtf_ok ? "ok" : "unsupported", state.mv_indep_mrt);
   return state.mv_vtf_ok != 0;
}

static void MvFreeTargets(IDirect3DDevice9* native)
{
   if (native)
   {
      native->SetTexture(D3DVERTEXTEXTURESAMPLER0 + Sm3MvPatch::kVtfSampler,
         nullptr);
   }
   Free(&mv.probe_motion);
   Free(&mv.probe_depth);
   Free(&mv.probe_raw);
   Free(&mv.motion_surface);
   Free(&mv.motion_rt);
   Free(&mv.vtf);
   for (IDirect3DTexture9*& texture : mv.textures)
   {
      Free(&texture);
   }
   std::fill(std::begin(mv.kmt), std::end(mv.kmt), nullptr);
   mv.vtf_rows = 0;
   mv.tex_w = mv.tex_h = 0;
}

// The shared SR textures (motion, depth, output, color), the render-size motion
// target and the previous-bones texture for `rows` bone sets. Recreated when
// the size changed or the rows do not fit. False = MV cannot run this frame.
static bool MvEnsureTextures(IDirect3DDevice9* native, UINT width,
   UINT height)
{
   if (mv.textures[0] && mv.tex_w == width && mv.tex_h == height)
   {
      return true;
   }
   MvFreeTargets(native);
   const D3DFORMAT formats[4] = {D3DFMT_G16R16F, D3DFMT_R32F,
      D3DFMT_A16B16G16R16F, D3DFMT_A16B16G16R16F};
   bool ok = true;
   for (int i = 0; ok && i < 4; ++i)
   {
      ok = SUCCEEDED(native->CreateTexture(
         width, height, 1, D3DUSAGE_RENDERTARGET, formats[i], D3DPOOL_DEFAULT,
         &mv.textures[i], &mv.kmt[i]));
   }
   for (int i = 0; ok && i < 2; ++i)
   {
      IDirect3DSurface9* surface = nullptr;
      mv.textures[i]->GetSurfaceLevel(0, &surface);
      native->ColorFill(surface, nullptr,
         i ? D3DCOLOR_ARGB(0xFF, 0x80, 0, 0) : 0); // depth 0.5
      Free(&surface);
   }
   ok = ok &&
        SUCCEEDED(native->CreateTexture(
           width, height, 1, D3DUSAGE_RENDERTARGET,
           state.mv_indep_mrt ? D3DFMT_G16R16F : D3DFMT_A16B16G16R16F,
           D3DPOOL_DEFAULT, &mv.motion_rt, nullptr)) &&
        SUCCEEDED(mv.motion_rt->GetSurfaceLevel(0, &mv.motion_surface));
   if (!ok)
   {
      Log("d9native: MV targets %ux%u FAILED", width, height);
      MvFreeTargets(native);
      return false;
   }
   mv.tex_w = width;
   mv.tex_h = height;
   return true;
}

static bool MvEnsureVtf(IDirect3DDevice9* native, UINT rows)
{
   if (mv.vtf && mv.vtf_rows >= rows)
   {
      return true;
   }
   Free(&mv.vtf);
   const HRESULT hr = native->CreateTexture(
      MV_VTF_WIDTH, rows, 1, D3DUSAGE_DYNAMIC, D3DFMT_A32B32G32R32F,
      D3DPOOL_DEFAULT, &mv.vtf, nullptr);
   mv.vtf_rows = SUCCEEDED(hr) ? rows : 0;
   if (FAILED(hr))
   {
      Log("d9native: MV previous-bones texture %ux%u FAILED 0x%08X", MV_VTF_WIDTH,
         rows, uint32_t(hr));
   }
   return SUCCEEDED(hr);
}

// The pass shaders and the GTAO apply PS, loaded again at their next use
static void MvFreeShaders()
{
   Free(&mv.vs_quad);
   Free(&mv.ps_fill);
   Free(&mv.ps_marker);
   Free(&mv.ps_copy);
   Free(&mv.ps_gtao);
   Free(&mv.ps_gtao_debug);
   mv.shaders_tried = mv.ps_gtao_tried = mv.ps_gtao_debug_tried = false;
}

static void MvDestroy(device* host)
{
   IDirect3DDevice9* native = host ? NativeOf(host) : nullptr;
   if (native)
   {
      MvRestoreGame(native);
   }
   Free(&mv.scene_surface);
   Free(&mv.prepass);
   mv.open = mv.ended = false;
   mv.scene_resource = mv.copy_dest = {0};
   mv.camera_valid = mv.previous_valid = false;
   mv.current.clear();
   mv.previous.clear();
   Free(&mv.block);
   MvFreeTargets(native);
   MvFreeShaders();
   mv.last_w = mv.last_h = 0;
   mv.disabled = mv.vtf_checked && !state.mv_vtf_ok;
}

// The motion RT's raw content at the probes (before the fill) and the shared
// motion / depth after it: test readback, one GPU stall per scene.
static float MvHalfToFloat(uint16_t half)
{
   return HalfToFloat(half);
}

static bool MvReadSurface(IDirect3DDevice9* native, IDirect3DSurface9* source,
   IDirect3DSurface9** sysmem, D3DFORMAT format,
   D3DLOCKED_RECT* locked)
{
   if (!*sysmem &&
       FAILED(native->CreateOffscreenPlainSurface(
          mv.tex_w, mv.tex_h, format, D3DPOOL_SYSTEMMEM, sysmem, nullptr)))
   {
      return false;
   }
   return SUCCEEDED(native->GetRenderTargetData(source, *sysmem)) &&
          SUCCEEDED((*sysmem)->LockRect(locked, nullptr, D3DLOCK_READONLY));
}

static void MvReadProbes(IDirect3DDevice9* native, bool raw)
{
   const int count = std::clamp<int>(state.mv_probe_count, 0, MV_MAX_PROBES);
   if (!count)
   {
      return;
   }
   D3DLOCKED_RECT locked = {};
   if (raw)
   {
      // The motion target: G16R16F (R, G half floats) or A16B16G16R16F; both
      // keep x, y first
      const bool two_channels = state.mv_indep_mrt != 0;
      if (!MvReadSurface(native, mv.motion_surface, &mv.probe_raw,
             two_channels ? D3DFMT_G16R16F : D3DFMT_A16B16G16R16F,
             &locked))
      {
         return;
      }
      const size_t texel = two_channels ? 4 : 8;
      for (int i = 0; i < count; ++i)
      {
         const auto* base = static_cast<const uint8_t*>(locked.pBits) +
                            size_t(state.mv_probe_xy[2 * i + 1]) * locked.Pitch +
                            size_t(state.mv_probe_xy[2 * i]) * texel;
         uint16_t halves[2];
         memcpy(halves, base, 4);
         state.mv_probe_raw[i][0] = MvHalfToFloat(halves[0]);
         state.mv_probe_raw[i][1] = MvHalfToFloat(halves[1]);
      }
      mv.probe_raw->UnlockRect();
      return;
   }
   IDirect3DSurface9* surface = nullptr;
   mv.textures[0]->GetSurfaceLevel(0, &surface);
   if (MvReadSurface(native, surface, &mv.probe_motion, D3DFMT_G16R16F,
          &locked))
   {
      for (int i = 0; i < count; ++i)
      {
         const auto* base = static_cast<const uint8_t*>(locked.pBits) +
                            size_t(state.mv_probe_xy[2 * i + 1]) * locked.Pitch +
                            size_t(state.mv_probe_xy[2 * i]) * 4;
         uint16_t halves[2];
         memcpy(halves, base, 4);
         state.mv_probe_motion[i][0] = MvHalfToFloat(halves[0]);
         state.mv_probe_motion[i][1] = MvHalfToFloat(halves[1]);
      }
      mv.probe_motion->UnlockRect();
   }
   Free(&surface);
   mv.textures[1]->GetSurfaceLevel(0, &surface);
   if (MvReadSurface(native, surface, &mv.probe_depth, D3DFMT_R32F, &locked))
   {
      for (int i = 0; i < count; ++i)
      {
         const auto* base = static_cast<const uint8_t*>(locked.pBits) +
                            size_t(state.mv_probe_xy[2 * i + 1]) * locked.Pitch +
                            size_t(state.mv_probe_xy[2 * i]) * 4;
         memcpy(&state.mv_probe_depth[i], base, 4);
      }
      mv.probe_depth->UnlockRect();
   }
   Free(&surface);
}

// Opens the scene at the depth prepass (Bayonetta main OpenScene): R32F RT0
// with a second target, of the bound depth's size, which is the output's or up
// to half of it, full viewport. Rolls last frame's camera and objects over and
// picks the jitter.
static void MvTryOpenScene(device* host, IDirect3DDevice9* native)
{
   mv.refused_rt0 = mv.rt_view[0];
   const UINT width = mv.dsv_w, height = mv.dsv_h;
   if (!width || mv.rt0_w != width || mv.rt0_h != height ||
       width > state.width || height > state.height || width * 2 < state.width ||
       height * 2 < state.height)
   {
      return;
   }
   D3DVIEWPORT9 viewport = {};
   native->GetViewport(&viewport);
   if (viewport.X || viewport.Y || viewport.Width != width ||
       viewport.Height != height)
   {
      return;
   }
   // The SR bridge's textures are the render size's (no render scale here)
   if (g_sr_upscaler >= 0 &&
       (width != state.render_width || height != state.render_height))
   {
      return;
   }
   IDirect3DTexture9* prepass = nullptr;
   if (!mv.rt0_resource.handle ||
       FAILED(reinterpret_cast<IUnknown*>(mv.rt0_resource.handle)
             ->QueryInterface(IID_PPV_ARGS(&prepass))))
   {
      return;
   }
   if (!MvCheckVtf(native) || !MvLoadShaders(native) ||
       !MvEnsureTextures(native, width, height))
   {
      mv.disabled = true;
      Free(&prepass);
      return;
   }
   mv.refused_rt0 = 0;
   Free(&mv.prepass);
   mv.prepass = prepass;
   mv.prepass_view = mv.rt_view[0];
   mv.scene_depth = mv.dsv;
   mv.scene_w = width;
   mv.scene_h = height;
   mv.scene_view = 0;
   Free(&mv.scene_surface);
   mv.scene_resource = mv.copy_dest = {0};
   mv.open = true;
   mv.ended = false;
   ++state.mv_scene_frames;

   // Last frame's camera and objects are the previous ones, unless frames
   // without a scene came between (SMV:747-792).
   const bool same_size = mv.last_w == width && mv.last_h == height;
   mv.previous_valid =
      mv.camera_valid && same_size && mv.frame_index - mv.scene_frame <= 1;
   mv.previous_camera = mv.camera;
   mv.camera_valid = false;
   mv.reprojection = MatIdentity();
   mv.scene_frame = mv.frame_index;
   mv.last_w = width;
   mv.last_h = height;
   mv.previous = std::move(mv.current);
   mv.current.clear();
   if (!mv.previous_valid)
   {
      mv.previous.clear();
   }

   // Jitter: Halton (2, 3) over the 1x phase count, pixels to NDC (y up), only
   // when the upscaler drew last frame (or forced by the test).
   mv.jitter[0] = mv.jitter[1] = 0.0f;
   if (state.force_jitter || mv.sr_ran_last)
   {
      const unsigned phase = mv.frame_index % MV_JITTER_PHASES;
      mv.jitter[0] = MvHalton(phase, 2);
      mv.jitter[1] = MvHalton(phase, 3);
   }
   // SMAA T2x (Bayonetta's OpenScene): (-0.25, -0.25) then (0.25, 0.25) in y
   // down pixels for its subsample indices 1 and 2, only after a frame it
   // resolved
   mv.t2x_phase = -1;
   if (state.smaa && state.smaa_t2x && !sr.ready && !state.luma_taa &&
       mv.t2x_ran_last)
   {
      mv.t2x_phase = int(mv.frame_index & 1);
      mv.jitter[0] = mv.jitter[1] = (mv.t2x_phase == 0) ? -0.25f : 0.25f;
   }
   state.t2x_phase = mv.t2x_phase;
   mv.jitter_ndc[0] = mv.jitter[0] * 2.0f / float(width);
   mv.jitter_ndc[1] = mv.jitter[1] * -2.0f / float(height);
   state.jitter_x = mv.jitter[0];
   state.jitter_y = mv.jitter[1];

   // Previous bones: one row per skinned object of last frame, written once per
   // frame (unmatched draws select their current bones in the VS instead)
   UINT skinned = 0;
   for (auto& [key, objects] : mv.previous)
   {
      for (MvObject& object : objects)
      {
         object.row = object.bones.empty() ? -1 : int(skinned++);
      }
   }
   if (!MvEnsureVtf(native, std::max(skinned, 1u)))
   {
      mv.disabled = true;
      return;
   }
   if (D3DLOCKED_RECT locked = {};
      SUCCEEDED(mv.vtf->LockRect(0, &locked, nullptr, D3DLOCK_DISCARD)))
   {
      // Zeroed: a current-bones draw still fetches a row and multiplies it by 0
      auto* texels = static_cast<uint8_t*>(locked.pBits);
      memset(texels, 0, size_t(mv.vtf_rows) * locked.Pitch);
      if (!Mutated(MUT_NO_PREV_BONES))
      {
         for (const auto& [key, objects] : mv.previous)
         {
            for (const MvObject& object : objects)
            {
               if (object.row >= 0)
               {
                  memcpy(texels + size_t(object.row) * locked.Pitch,
                     object.bones.data(), MV_BONE_FLOATS * sizeof(float));
               }
            }
         }
      }
      mv.vtf->UnlockRect(0);
   }
   MvSetPointClamp(native, D3DVERTEXTEXTURESAMPLER0 + Sm3MvPatch::kVtfSampler,
      mv.vtf);

   // Motion target = the marker (D3D9 Clear can't express 65504)
   {
      PassScope scope(native);
      native->SetRenderTarget(1, nullptr);
      native->SetRenderTarget(0, mv.motion_surface);
      native->SetDepthStencilSurface(nullptr);
      MvDrawQuad(native, width, height, mv.ps_marker);
   }
}

// FindNearest (PD:792-830): this draw's place in its key's order and its
// neighbours with an exact transform, else the nearest transform.
static const MvObject* MvFindNearest(const std::vector<MvObject>& candidates,
   size_t order,
   const std::array<float, 12>& transform)
{
   for (const size_t place : {order, order + 1, order - 1})
   {
      if (place < candidates.size() && candidates[place].transform == transform)
      {
         return &candidates[place];
      }
   }
   // ponytail: always the linear scan (Core indexes lists over 32 candidates)
   const MvObject* match = nullptr;
   float nearest = FLT_MAX;
   for (const MvObject& candidate : candidates)
   {
      float distance = 0.0f;
      for (size_t i = 0; i < transform.size(); ++i)
      {
         const float difference = candidate.transform[i] - transform[i];
         distance += difference * difference;
      }
      if (distance < nearest)
      {
         nearest = distance;
         match = &candidate;
         if (distance == 0.0f)
         {
            break;
         }
      }
   }
   return match;
}

static uint64_t MvHash(uint64_t seed, uint64_t value)
{
   return seed ^ (value + 0x9e3779b97f4a7c15ull + (seed << 6) + (seed >> 2));
}

// The patched VS (jitter, previous constants) and optionally the patched PS +
// RT1 for this draw.
static void MvApplyState(IDirect3DDevice9* native, const MvShader& vertex,
   const MvShader* pixel)
{
   native->SetVertexShader(vertex.vs);
   mv.applied_vs = true;
   if (pixel)
   {
      native->SetPixelShader(pixel->ps);
      mv.applied_ps = true;
      if (!mv.applied_rt1)
      {
         native->GetRenderState(D3DRS_COLORWRITEENABLE1, &mv.saved_cwe1);
         native->SetRenderTarget(1, mv.motion_surface);
         native->SetRenderState(D3DRS_COLORWRITEENABLE1,
            D3DCOLORWRITEENABLE_RED |
               D3DCOLORWRITEENABLE_GREEN);
         mv.applied_rt1 = true;
      }
   }
   else
   {
      if (mv.applied_ps)
      {
         native->SetPixelShader(BoundPixelShader(mv.game_ps));
         mv.applied_ps = false;
      }
      if (mv.applied_rt1)
      {
         native->SetRenderTarget(1, nullptr);
         native->SetRenderState(D3DRS_COLORWRITEENABLE1, mv.saved_cwe1);
         mv.applied_rt1 = false;
      }
   }
}

// The SR output back, RGB only (the alpha is the bloom threshold input), into
// the scene and into the game's last copy of it.
static void MvCopyBack(IDirect3DDevice9* native)
{
   PassScope scope(native);
   native->SetRenderTarget(1, nullptr);
   native->SetDepthStencilSurface(nullptr);
   native->SetRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_RED |
                                                     D3DCOLORWRITEENABLE_GREEN |
                                                     D3DCOLORWRITEENABLE_BLUE);
   MvSetPointClamp(native, 0, mv.textures[2]);
   const float size[4] = {float(mv.scene_w), float(mv.scene_h), 0, 0};
   native->SetPixelShaderConstantF(0, size, 1);
   native->SetRenderTarget(0, mv.scene_surface);
   MvDrawQuad(native, mv.scene_w, mv.scene_h, mv.ps_copy);
   ++state.mv_copy_backs;
   if (mv.copy_dest.handle && !Mutated(MUT_NO_COPY_BACK))
   {
      IDirect3DTexture9* texture = nullptr;
      IDirect3DSurface9* surface = nullptr;
      D3DSURFACE_DESC desc = {};
      if (SUCCEEDED(reinterpret_cast<IUnknown*>(mv.copy_dest.handle)
                ->QueryInterface(IID_PPV_ARGS(&texture))) &&
          SUCCEEDED(texture->GetLevelDesc(0, &desc)) &&
          desc.Width == mv.scene_w && desc.Height == mv.scene_h &&
          (desc.Usage & D3DUSAGE_RENDERTARGET) &&
          SUCCEEDED(texture->GetSurfaceLevel(0, &surface)) &&
          SUCCEEDED(native->SetRenderTarget(0, surface)))
      {
         MvDrawQuad(native, mv.scene_w, mv.scene_h, mv.ps_copy);
         ++state.mv_copy_backs;
      }
      Free(&surface);
      Free(&texture);
   }
}

// Scene end: marker texels -> camera motion + depth (the fill), then Luma TAA
// (pass device) or the SR bridge on a shared copy of the scene, whose output
// goes back RGB-only.
static void MvEndScene(device* host, IDirect3DDevice9* native)
{
   mv.ended = true;
   ++state.mv_scene_ends;
   if (!mv.scene_surface || !mv.prepass)
   {
      return;
   }
   MvReadProbes(native, true);
   // Fill (own state scope)
   {
      PassScope scope(native);
      float constants[5 * 4] = {};
      for (int row = 0; row < 4; ++row)
      {
         for (int column = 0; column < 4; ++column)
         {
            constants[row * 4 + column] = float(mv.reprojection.m[row][column]);
         }
      }
      constants[16] = mv.jitter_ndc[0];
      constants[17] = mv.jitter_ndc[1];
      constants[18] = float(mv.scene_w);
      constants[19] = float(mv.scene_h);
      IDirect3DSurface9 *motion = nullptr, *depth = nullptr;
      mv.textures[0]->GetSurfaceLevel(0, &motion);
      mv.textures[1]->GetSurfaceLevel(0, &depth);
      native->SetRenderTarget(0, motion);
      native->SetRenderTarget(1, depth);
      native->SetDepthStencilSurface(nullptr);
      MvSetPointClamp(native, 0, mv.motion_rt);
      MvSetPointClamp(native, 1, mv.prepass);
      native->SetPixelShaderConstantF(0, constants, 5);
      MvDrawQuad(native, mv.scene_w, mv.scene_h, mv.ps_fill);
      native->SetRenderTarget(1, nullptr);
      Free(&motion);
      Free(&depth);
   }
   mv.filled = true;
   MvReadProbes(native, false);

   // SR on the scene: Luma TAA on the pass device, else the SR bridge
   const bool taa = state.luma_taa && PassEnsureOpen(host);
   if ((!taa && !sr.ready) || !out.query9)
   {
      return;
   }
   if (!taa &&
       (!hp.gpu_sync || WaitForSingleObject(sr.process, 0) == WAIT_OBJECT_0))
   {
      Log("d9native: SR bridge stopped (%s)",
         hp.gpu_sync ? "it exited" : "the presenter lost GPU sync");
      StopUpscaler();
      return;
   }
   D3DSURFACE_DESC scene_desc = {};
   mv.scene_surface->GetDesc(&scene_desc);
   if (scene_desc.Format != D3DFMT_A16B16G16R16F ||
       scene_desc.Width != mv.scene_w || scene_desc.Height != mv.scene_h)
   {
      return;
   }
   IDirect3DSurface9* color = nullptr;
   mv.textures[3]->GetSurfaceLevel(0, &color);
   const HRESULT copy_hr = native->StretchRect(mv.scene_surface, nullptr, color,
      nullptr, D3DTEXF_NONE);
   Free(&color);
   out.query9->Issue(D3DISSUE_END);
   const HRESULT query_hr = out.query9->GetData(nullptr, 0, D3DGETDATA_FLUSH);
   if (FAILED(copy_hr) || FAILED(query_hr))
   {
      Log("d9native: SR scene copy failed (StretchRect 0x%08X, query 0x%08X)",
         uint32_t(copy_hr), uint32_t(query_hr));
      return;
   }
   // FSR needs the camera (DLSS ignores it); defaults without one
   MvCamera camera = {1.0, 0.1, 1000.0};
   if (mv.camera_valid)
   {
      camera = MvViewProjectionCamera(mv.camera.data());
   }
   if (taa)
   {
      if (g_pass.taa_color != mv.kmt[3])
      {
         g_pass.taa_color =
            D3D9Passes::SetTaaTextures(mv.kmt[3], mv.kmt[1], mv.kmt[0], mv.kmt[2])
               ? mv.kmt[3]
               : nullptr;
      }
      const bool reset = !mv.previous_valid || Mutated(MUT_NO_TAA_HISTORY);
      const uint64_t value = ++g_pass.value;
      if (!g_pass.taa_color ||
          !KmtSignalObject(&g_pass.sync, g_pass.in.object, value) ||
          !D3D9Passes::RunTaa(value, {.jitter = {mv.jitter[0], mv.jitter[1]},
                                        .near_plane = float(camera.near_plane),
                                        .far_plane = float(camera.far_plane),
                                        .reset = reset}) ||
          !KmtWaitObject(&g_pass.sync, g_pass.out.object, value))
      {
         ++state.pass_failures;
         McpTraceEvent("custom", "luma_taa_failed");
         Log("d9native: Luma TAA failed (%s, kmt %s 0x%08X)", D3D9Passes::Error(),
            g_pass.sync.first_error_call,
            static_cast<uint32_t>(g_pass.sync.first_error));
         return;
      }
      ++state.taa_runs;
      McpTraceEvent("custom", "luma_taa");
      state.taa_resets += reset ? 1 : 0;
      mv.sr_ran = true;
      MvCopyBack(native);
      return;
   }
   SRBridgeProtocol::Frame frame = {.jitter_x = mv.jitter[0],
      .jitter_y = mv.jitter[1],
      .reset = mv.previous_valid ? 0 : 1,
      .render_width = mv.scene_w,
      .render_height = mv.scene_h,
      .pre_exposure = 1.0f,
      .sharpness = -1.0f,
      .near_plane = float(camera.near_plane),
      .far_plane = float(camera.far_plane),
      .vert_fov = float(camera.vert_fov)};
   if (!D3D9SR::RunFrame(&sr, &frame))
   {
      McpTraceEvent("custom", "sr_dispatch_failed");
      Log("d9native: SR bridge frame %llu failed (%s 0x%08X), stopped", frame.n,
         sr.sync.first_error_call, static_cast<uint32_t>(sr.sync.first_error));
      StopUpscaler();
      return;
   }
   ++state.sr_frames;
   ++state.sr_mid_frames;
   McpTraceEvent("custom", "sr_dispatch");
   state.sr_frame_jitter[0] = mv.jitter[0];
   state.sr_frame_jitter[1] = mv.jitter[1];
   state.sr_frame_reset = frame.reset;
   state.sr_frame_camera[0] = float(camera.vert_fov);
   state.sr_frame_camera[1] = float(camera.near_plane);
   state.sr_frame_camera[2] = float(camera.far_plane);
   mv.sr_ran = true;
   MvCopyBack(native);
}

// Every draw event (before the game's draw): scene end, scene open, then the
// patched state for draws into the scene.
static void MvOnDraw(device* host, uint32_t count, uint32_t first,
   int32_t vertex_offset)
{
   if (mv.disabled)
   {
      return;
   }
   IDirect3DDevice9* native = NativeOf(host);
   const bool prepass_like = mv.rt0_format == format::r32_float &&
                             mv.rt_view[1] && !mv.other_rt && mv.dsv;
   if (!mv.open && !mv.ended && prepass_like && !state.msaa_active &&
       mv.refused_rt0 != mv.rt_view[0])
   {
      MvRestoreGame(native);
      MvTryOpenScene(host, native);
   }
   if (mv.open && !mv.ended && mv.post_crcs.contains(out.bound_ps_crc))
   {
      MvRestoreGame(native);
      MvEndScene(host, native);
   }
   const auto restore = [&]
   { MvRestoreGame(native); };
   if (!mv.open || mv.ended || mv.dsv != mv.scene_depth ||
       mv.rt0_w != mv.scene_w || mv.rt0_h != mv.scene_h || mv.other_rt)
   {
      restore();
      return;
   }
   // Draw class: the prepass RT (jitter only, its PS writes oC1 itself) or the
   // fp16 scene color (the first one drawn with this depth)
   const bool is_prepass = prepass_like && mv.rt_view[0] == mv.prepass_view;
   bool is_color = false;
   if (!is_prepass && mv.rt0_format == format::r16g16b16a16_float &&
       !mv.rt_view[1])
   {
      if (!mv.scene_view)
      {
         mv.scene_view = mv.rt_view[0];
         mv.scene_resource = mv.rt0_resource;
         mv.scene_surface = reinterpret_cast<IDirect3DSurface9*>(mv.scene_view);
         mv.scene_surface->AddRef();
      }
      is_color = mv.rt_view[0] == mv.scene_view;
   }
   if (!is_prepass && !is_color)
   {
      restore();
      return;
   }
   DWORD depth_test = 0, blend = 0;
   native->GetRenderState(D3DRS_ZENABLE, &depth_test);
   const auto vertex_entry = mv.shaders.find(mv.game_vs);
   if (!depth_test || vertex_entry == mv.shaders.end() ||
       !vertex_entry->second.vs)
   {
      ++state.mv_draws_unpatched;
      restore();
      return;
   }
   const MvShader& vertex = vertex_entry->second;
   const Sm3MvPatch::VertexInfo& info = vertex.info;
   const float* constants = mv.vs_const;
   const int camera_register = info.camera_register;
   std::array<float, 16> current_camera = {};
   if (camera_register >= 0)
   {
      memcpy(current_camera.data(), constants + camera_register * 4,
         sizeof(current_camera));
      // The frame's camera: the first c8 draw's view projection;
      // reprojection = previous * inverse(current), in double (SMV:846)
      if (camera_register == 8 && !mv.camera_valid)
      {
         mv.camera = current_camera;
         mv.camera_valid = true;
         mv.reprojection = MatIdentity();
         Mat4 inverse;
         if (mv.previous_valid &&
             MatInvert(MatFromRows(mv.camera.data()), &inverse))
         {
            mv.reprojection =
               MatMul(MatFromRows(mv.previous_camera.data()), inverse);
         }
      }
   }
   const float jitter_row[4] = {mv.jitter_ndc[0], mv.jitter_ndc[1], 0, 0};
   native->SetVertexShaderConstantF(Sm3MvPatch::kJitterRegister, jitter_row, 1);
   if (is_prepass)
   {
      ++state.mv_draws_prepass;
      // XeGTAO's camera: "Rmodelview" c20 + "g_WorldViewProj" c24 (static
      // meshes) or "view" c0 + "g_ViewProjection" c8 (skinned)
      // ponytail: the c8 layout assumes every skinned prepass VS reads "view" at
      // c0, as Bayonetta's do; check the reads (Core DXBC::ReadsConstantRow) if
      // another game shares this
      const bool modelview =
                    camera_register == 24 && info.effect_world_register == 20,
                 skinned_view = camera_register == 8 && info.skinned;
      if (modelview || skinned_view)
      {
         const uint32_t affine = modelview ? 20 : 0;
         memcpy(mv.gtao_camera, constants, sizeof(mv.gtao_camera));
         mv.gtao_affine_row = affine;
         mv.gtao_matrix_row = uint32_t(camera_register);
         mv.gtao_camera_frame = mv.frame_index;
      }
      MvApplyState(native, vertex, nullptr);
      return;
   }

   native->GetRenderState(D3DRS_ALPHABLENDENABLE, &blend);
   const auto pixel_entry = mv.shaders.find(mv.game_ps);
   const bool refraction = mv.refraction_crcs.contains(out.bound_ps_crc);
   const bool patch_pixel = !blend && !refraction && !info.jitter_only &&
                            pixel_entry != mv.shaders.end() &&
                            pixel_entry->second.ps;
   state.mv_draws_blended += blend != 0;
   state.mv_draws_jitter_only += info.jitter_only;
   if (!patch_pixel)
   {
      ++state.mv_draws_patched;
      MvApplyState(native, vertex, nullptr);
      return;
   }
   ++state.mv_draws_patched;

   // The draw's identity (SMV:1076): VS, PS, stream 0, index buffer, draw
   // arguments. The buffers come from the device: ReShade's bind_vertex_buffers
   // / bind_index_buffer events make DrawPrimitiveUP use a fake buffer that
   // dangles after a Reset (the next UP draw crashes in the runtime).
   IDirect3DVertexBuffer9* vertex_buffer = nullptr;
   IDirect3DIndexBuffer9* index_buffer = nullptr;
   UINT vertex_buffer_offset = 0, vertex_buffer_stride = 0;
   native->GetStreamSource(0, &vertex_buffer, &vertex_buffer_offset,
      &vertex_buffer_stride);
   native->GetIndices(&index_buffer);
   const uint64_t vertex_buffer_id = reinterpret_cast<uint64_t>(vertex_buffer),
                  index_buffer_id = reinterpret_cast<uint64_t>(index_buffer);
   Free(&vertex_buffer);
   Free(&index_buffer);
   uint64_t key = 0;
   for (const uint64_t value :
      {uint64_t(vertex.crc), uint64_t(out.bound_ps_crc), vertex_buffer_id,
         uint64_t(vertex_buffer_offset), index_buffer_id, uint64_t(count),
         uint64_t(first), uint64_t(uint32_t(vertex_offset))})
   {
      key = MvHash(key, value);
   }
   MvObject object;
   if (info.tie_break_register >= 0)
   {
      memcpy(object.transform.data(), constants + info.tie_break_register * 4,
         sizeof(object.transform));
   }
   object.camera = current_camera;
   if (info.world_register >= 0)
   {
      memcpy(object.world.data(), constants + info.world_register * 4,
         sizeof(object.world));
   }
   if (info.effect_world_register >= 0)
   {
      memcpy(object.effect_world.data(),
         constants + info.effect_world_register * 4,
         sizeof(object.effect_world));
   }
   if (info.skinned)
   {
      object.bones.assign(constants + Sm3MvPatch::kBoneFirstRegister * 4,
         constants + Sm3MvPatch::kBoneFirstRegister * 4 +
            MV_BONE_FLOATS);
   }
   const MvObject* match = nullptr;
   if (mv.previous_valid && info.tie_break_register >= 0)
   {
      if (const auto candidates = mv.previous.find(key);
         candidates != mv.previous.end())
      {
         const auto drawn = mv.current.find(key);
         match =
            MvFindNearest(candidates->second,
               drawn != mv.current.end() ? drawn->second.size() : 0,
               object.transform);
      }
   }
   std::array<float, 16> previous_camera = current_camera,
                         previous_world = object.world,
                         previous_effect_world = object.effect_world;
   int bones_row = -1;
   if (match)
   {
      previous_camera = match->camera;
      previous_world = match->world;
      previous_effect_world = match->effect_world;
      bones_row = match->row;
      ++state.mv_matches;
   }
   else if (mv.previous_valid && mv.camera_valid && camera_register >= 0)
   {
      if (camera_register == 8)
      {
         // A c8 draw with the frame's camera: last frame's camera only
         if (memcmp(current_camera.data(), mv.camera.data(),
                size_t(info.camera_rows) * 4 * sizeof(float)) == 0)
         {
            previous_camera = mv.previous_camera;
            ++state.mv_fallback_camera;
         }
         else
         {
            ++state.mv_fallback_zero;
         }
      }
      else
      {
         // A c24 world view projection: reprojection * WVP (camera only)
         const Mat4 reprojected =
            MatMul(mv.reprojection, MatFromRows(current_camera.data()));
         for (int row = 0; row < 4; ++row)
         {
            for (int column = 0; column < 4; ++column)
            {
               previous_camera[row * 4 + column] = float(reprojected.m[row][column]);
            }
         }
         ++state.mv_fallback_reprojection;
      }
   }
   else
   {
      ++state.mv_fallback_zero;
   }
   // An unmatched skinned draw's previous bones are its current ones: the
   // patched VS selects them over the VTF (c252.zw = 1, 0)
   const bool current_bones = info.skinned && bones_row < 0;
   state.mv_current_bones += current_bones;
   mv.current[key].push_back(std::move(object));

   float previous_constants[13 * 4] = {};
   memcpy(previous_constants, previous_camera.data(), sizeof(previous_camera));
   memcpy(previous_constants + 16, previous_world.data(),
      sizeof(previous_world));
   memcpy(previous_constants + 32, previous_effect_world.data(),
      sizeof(previous_effect_world));
   previous_constants[48] = 1.0f / float(MV_VTF_WIDTH);
   previous_constants[49] =
      (float(std::max(bones_row, 0)) + 0.5f) / float(mv.vtf_rows);
   previous_constants[50] = current_bones ? 1.0f : 0.0f;
   previous_constants[51] = current_bones ? 0.0f : 1.0f;
   native->SetVertexShaderConstantF(Sm3MvPatch::kPrevCameraRegister,
      previous_constants, 13);
   MvApplyState(native, vertex, &pixel_entry->second);
}

static void MvOnBindRenderTargets(device* host, uint32_t count,
   const resource_view* rtvs,
   resource_view depth)
{
   MvRestoreGame(NativeOf(host));
   mv.rt_view[0] = count > 0 ? rtvs[0].handle : 0;
   uint64_t second = count > 1 ? rtvs[1].handle : 0;
   if (mv.motion_surface &&
       second == reinterpret_cast<uint64_t>(mv.motion_surface))
   {
      second = 0; // ours, restored just now
   }
   mv.rt_view[1] = second;
   mv.other_rt = false;
   for (uint32_t i = 2; i < count; ++i)
   {
      mv.other_rt |= rtvs[i].handle != 0;
   }
   mv.dsv = depth.handle;
   mv.rt0_format = format::unknown;
   mv.rt0_resource = {0};
   mv.rt0_w = mv.rt0_h = mv.dsv_w = mv.dsv_h = 0;
   if (mv.rt_view[0])
   {
      const resource rt0 =
         host->get_resource_from_view(resource_view{mv.rt_view[0]});
      if (rt0.handle)
      {
         const resource_desc desc = host->get_resource_desc(rt0);
         mv.rt0_resource = rt0;
         mv.rt0_format = desc.texture.format;
         mv.rt0_w = desc.texture.width;
         mv.rt0_h = desc.texture.height;
      }
   }
   if (mv.dsv)
   {
      const resource depth_resource = host->get_resource_from_view(depth);
      if (depth_resource.handle)
      {
         const resource_desc desc = host->get_resource_desc(depth_resource);
         mv.dsv_w = desc.texture.width;
         mv.dsv_h = desc.texture.height;
      }
   }
}

static void MvOnPresent(device* host)
{
   IDirect3DDevice9* native = NativeOf(host);
   MvRestoreGame(native);
   if (mv.open)
   {
      if (!mv.ended)
      {
         Log("d9native: MV scene without a post pass (no SR this frame)");
      }
      Free(&mv.scene_surface);
      Free(&mv.prepass);
   }
   mv.sr_ran_last = mv.sr_ran;
   mv.sr_ran = false;
   mv.t2x_ran_last = mv.t2x_ran;
   mv.t2x_ran = mv.filled = false;
   mv.open = mv.ended = false;
   mv.refused_rt0 = 0;
   mv.scene_view = 0;
   mv.rt0_resource = {0};
   ++mv.frame_index;
}

// The frame's depth prepass (the SSAO pass' s2) shared (ShareDepth) when it
// is R32F of the scene rect's size (as Bayonetta's DrawSMAAOnCanvas). False:
// no predication this frame.
static bool CopyDepth(device* device)
{
   if (bay.depth_present != state.presents || !bay.depth_texture)
   {
      return false;
   }
   auto* const texture =
      reinterpret_cast<IDirect3DTexture9*>(bay.depth_texture);
   D3DSURFACE_DESC desc = {};
   if (FAILED(texture->GetLevelDesc(0, &desc)) || desc.Format != D3DFMT_R32F ||
       desc.Width != out.scene_rect.Width ||
       desc.Height != out.scene_rect.Height)
   {
      return false;
   }
   return ShareDepth(device, texture, desc);
}

// XeGTAO in place of the game's SSAO draw (Bayonetta's RunXeGTAO): its s2
// depth shared, the passes on the pass device into the shared AO, then the
// apply PS over the game's target with its s0 scene and s2 depth. False: the
// game's draw runs (no camera this frame, no pass device, a failure).
static bool RunGtao(device* device)
{
   IDirect3DDevice9* const native = NativeOf(device);
   auto* const depth = reinterpret_cast<IDirect3DTexture9*>(out.ps_textures[2]);
   auto* const scene = reinterpret_cast<IDirect3DTexture9*>(out.ps_textures[0]);
   D3DSURFACE_DESC desc = {};
   if (mv.gtao_camera_frame != mv.frame_index || !mv.vs_quad || !scene ||
       !out.query9 || FAILED(depth->GetLevelDesc(0, &desc)) ||
       desc.Format != D3DFMT_R32F || !PassEnsureOpen(device))
   {
      return false;
   }
   const bool debug_view = g_gtao_debug_view > 0;
   IDirect3DPixelShader9*& apply_ps = (debug_view ? mv.ps_gtao_debug : mv.ps_gtao);
   bool& apply_ps_tried = (debug_view ? mv.ps_gtao_debug_tried : mv.ps_gtao_tried);
   // A failed compile isn't retried every frame, only after a reload
   if (!apply_ps_tried)
   {
      apply_ps_tried = true;
      const std::vector<uint8_t> code =
         LoadShader(debug_view ? L"gtao_apply_debug.ps_3_0.cso" : L"gtao_apply.ps_3_0.cso",
            "Bayonetta/Luma_Bayonetta_GTAOApply_SM3.hlsl", "main", "ps_3_0",
            debug_view ? std::vector<std::string>{"GTAO_DEBUG_VIEW=1"} : std::vector<std::string>{});
      if (!code.empty())
      {
         native->CreatePixelShader(reinterpret_cast<const DWORD*>(code.data()), &apply_ps);
      }
   }
   if (!apply_ps)
   {
      return false;
   }
   if (g_pass.ao_width != desc.Width || g_pass.ao_height != desc.Height)
   {
      D3D9Passes::SetGtaoOutput(nullptr);
      if (g_pass.ao.handle)
      {
         device->destroy_resource(g_pass.ao);
         g_pass.ao = {0};
      }
      HANDLE kmt = nullptr;
      if (!CreateSharedR32f(device, desc.Width, desc.Height, &g_pass.ao, &kmt) ||
          !D3D9Passes::SetGtaoOutput(kmt))
      {
         Log("d9native: XeGTAO output %ux%u failed (%s)", desc.Width, desc.Height,
            D3D9Passes::Error());
         return false;
      }
      g_pass.ao_width = desc.Width;
      g_pass.ao_height = desc.Height;
   }
   auto* const ao = reinterpret_cast<IDirect3DTexture9*>(g_pass.ao.handle);
   if (!ShareDepth(device, depth, desc))
   {
      return false;
   }
   if (Mutated(MUT_NO_GTAO_PASS))
   {
      IDirect3DSurface9* surface = nullptr;
      ao->GetSurfaceLevel(0, &surface);
      native->ColorFill(surface, nullptr, 0xFFFFFFFF);
      Free(&surface);
   }
   else
   {
      // The AO history under an upscaler only, on this frame's motion so far (the open scene's RT1: object motion where a patched
      // draw wrote one, the marker elsewhere) copied into the shared motion texture, which the fill overwrites at the scene's end
      bool history = (sr.ready || state.luma_taa) && g_gtao_history_weight > 0.f && mv.open && !mv.ended && mv.ps_copy &&
                     mv.motion_rt && mv.kmt[0] && mv.tex_w == desc.Width && mv.tex_h == desc.Height;
      if (history)
      {
         PassScope scope(native);
         IDirect3DSurface9* motion = nullptr;
         mv.textures[0]->GetSurfaceLevel(0, &motion);
         native->SetRenderTarget(1, nullptr);
         native->SetRenderTarget(0, motion);
         native->SetDepthStencilSurface(nullptr);
         MvSetPointClamp(native, 0, mv.motion_rt);
         const float size[4] = {float(mv.tex_w), float(mv.tex_h), 0, 0};
         native->SetPixelShaderConstantF(0, size, 1);
         MvDrawQuad(native, mv.tex_w, mv.tex_h, mv.ps_copy);
         Free(&motion);
         if (g_pass.motion != mv.kmt[0])
         {
            g_pass.motion = (D3D9Passes::SetMotion(mv.kmt[0]) ? mv.kmt[0] : nullptr);
         }
         history = g_pass.motion != nullptr;
      }
      D3D9Passes::GtaoParams params = {.affine_row = mv.gtao_affine_row,
         .matrix_row = mv.gtao_matrix_row,
         .final_value_power = (g_gtvbao_enable ? g_gtvbao_final_value_power : g_gtao_final_value_power),
         .depth_scale = g_gtao_depth_scale,
         .radius_override = g_gtao_radius_override,
         .gtvbao = g_gtvbao_enable,
         .debug_view = float(g_gtao_debug_view),
         .noise_index = (history ? float(mv.frame_index % 64) : 0.f),
         .history_weight = (history ? g_gtao_history_weight : 0.f),
         .history_reset = !mv.previous_valid || mv.gtao_frame + 1 != mv.frame_index,
         .jitter_ndc = {mv.jitter_ndc[0], mv.jitter_ndc[1]}};
      memcpy(params.camera, mv.gtao_camera, sizeof(params.camera));
      for (int row = 0; row < 4; ++row)
      {
         for (int column = 0; column < 4; ++column)
         {
            params.reprojection[row * 4 + column] = float(mv.reprojection.m[row][column]);
         }
      }
      out.query9->Issue(D3DISSUE_END);
      const uint64_t value = ++g_pass.value;
      if (FAILED(out.query9->GetData(nullptr, 0, D3DGETDATA_FLUSH)) ||
          !KmtSignalObject(&g_pass.sync, g_pass.in.object, value) ||
          !D3D9Passes::RunGtao(value, params) ||
          !KmtWaitObject(&g_pass.sync, g_pass.out.object, value))
      {
         ++state.pass_failures;
         Log("d9native: XeGTAO failed (%s)", D3D9Passes::Error());
         return false;
      }
      mv.gtao_frame = mv.frame_index;
   }
   PassScope scope(native);
   MvSetPointClamp(native, 0, scene);
   MvSetPointClamp(native, 2, depth);
   MvSetPointClamp(native, 3, ao);
   const float size[4] = {float(desc.Width), float(desc.Height), 0, 0};
   native->SetPixelShaderConstantF(0, size, 1);
   MvDrawQuad(native, desc.Width, desc.Height, apply_ps);
   ++state.gtao_runs;
   McpTraceEvent("custom", "gtao");
   return true;
}
// The MV module's render-size motion, depth, output and color textures shared
// by KMT, two fences on the D3D9 kernel device, then the "bridge" start line;
// "out" reaches kReady once the upscaler exists (EnsureHelper polls it). The
// upscaler runs mid frame at the scene end (MvEndScene).
static void StartUpscaler(device* device)
{
   sr.gave_up = true;
   auto* native = reinterpret_cast<IDirect3DDevice9*>(device->get_native());
   const UINT width = state.render_width, height = state.render_height;
   LUID luid = {};
   const bool ok = D3D9AdapterLuid(device, &luid) &&
                   D3D9SR::Open(&sr, KmtGeneration(device)) &&
                   MvCheckVtf(native) && MvEnsureTextures(native, width, height);
   // Render = output size, hdr, unjittered motion (the patched PS writes it),
   // motion scale = render size (UV delta). DLSS's auto exposure; FSR's clips
   // highlights (FSR-Best-Practices FIN-3), so it runs at exposure 1. DLAA on
   // scene-referred HDR input takes preset J or K, never L or M
   // (DLSS-Best-Practices PRE-4): "Default" picks K (11), so NVIDIA can't
   // change it over the air; an explicit choice is kept.
   // Textures: color motion depth, no reactive / transparency / exposure,
   // output.
   if (!ok ||
       !D3D9SR::Start(&sr, ModuleFilePath(L""), g_sr_upscaler, luid,
          {.render_width = width,
             .render_height = height,
             .output_width = width,
             .output_height = height,
             .hdr = true,
             .auto_exposure = g_sr_upscaler != SRBridgeProtocol::kFsr,
             .motion_scale_x = float(width),
             .motion_scale_y = float(height),
             .dlss_preset = int(dlss_render_preset != 0 ? dlss_render_preset : 11u),
             .fsr_version = sr_fsr_version},
          {mv.kmt[3], mv.kmt[0], mv.kmt[1], nullptr, nullptr,
             nullptr, mv.kmt[2]}))
   {
      if (sr.launch_error)
      {
         Log("d9native: CreateProcess(%ls) failed %lu",
            ModuleFilePath(SRBridgeProtocol::kHelperName).c_str(),
            sr.launch_error);
      }
      Log("d9native: SR bridge start failed (%s 0x%08X)",
         sr.sync.first_error_call, static_cast<uint32_t>(sr.sync.first_error));
      StopUpscaler();
      return;
   }
   ++state.sr_starts;
   Log("d9native: SR bridge %d started %ux%u (pid %lu)", g_sr_upscaler, width,
      height, GetProcessId(sr.process));
}
// P4.5 Luma Bloom: Bayonetta's DrawBloomPass / MirrorBloomCopy. The game's
// bloom chain (filter05 from the scene into its 320x176 target, the blurs and
// level copies) drawn into a 16-bit UNORM texture "scale" times the target's
// size (scene height / 720), the viewport and scissor scaled alike, with the
// game's shaders: each StretchRect of the target's region into a level texture
// is repeated from the Luma texture into that texture's shadow at the same
// scale, which the next chain draws sample. A composite (copytex sampling the
// target, SRC_ALPHA + ONE or INV_SRC_ALPHA) samples the Luma texture. The game
// draws with DrawPrimitiveUP, which can't be issued again: its draw runs on
// our RT0 / viewport / scissor / s0, and the game's come back at the next
// draw, copy, clear or present unless it set them itself first.
// ponytail: the DX11 pass' copy of row / column 1 into the texture's first
// row / column (the game's quads start inside the first texel) is not done;
// add it with a temp RT if the top / left edge darkens.
struct BloomShadow
{
   uint64_t game_texture = 0;
   IDirect3DTexture9* texture = nullptr;
};
static struct
{
   std::set<uint32_t> source_crcs{std::begin(kBloomSourcePs), std::end(kBloomSourcePs)},
      blur_crcs{std::begin(kBloomBlurPs), std::end(kBloomBlurPs)},
      copy_crcs{std::begin(kBloomCopyPs), std::end(kBloomCopyPs)};
   uint64_t target = 0; // the game's bloom target (identity only)
   uint32_t scale = 0;
   IDirect3DTexture9* texture = nullptr;
   IDirect3DSurface9* surface = nullptr;
   std::vector<BloomShadow> shadows;
   // The game's state under a redirected draw
   IDirect3DSurface9* game_rt0 = nullptr;
   IDirect3DBaseTexture9* game_s0 = nullptr;
   D3DVIEWPORT9 game_viewport = {};
   RECT game_scissor = {};
   bool restore_rt0 = false, restore_viewport = false, restore_scissor = false,
        restore_s0 = false;
} bloom;

static void BloomRestore(IDirect3DDevice9* native)
{
   if (bloom.restore_rt0)
   {
      native->SetRenderTarget(0, bloom.game_rt0); // resets the viewport
   }
   if (bloom.restore_viewport)
   {
      native->SetViewport(&bloom.game_viewport);
   }
   if (bloom.restore_scissor)
   {
      native->SetScissorRect(&bloom.game_scissor);
   }
   if (bloom.restore_s0)
   {
      native->SetTexture(0, bloom.game_s0);
   }
   bloom.restore_rt0 = bloom.restore_viewport = bloom.restore_scissor =
      bloom.restore_s0 = false;
   Free(&bloom.game_rt0);
   Free(&bloom.game_s0);
}

static void BloomRelease()
{
   for (BloomShadow& shadow : bloom.shadows)
   {
      Free(&shadow.texture);
   }
   bloom.shadows.clear();
   Free(&bloom.surface);
   Free(&bloom.texture);
}

static IDirect3DTexture9* BloomCreateTexture(IDirect3DDevice9* native,
   UINT width, UINT height)
{
   IDirect3DTexture9* texture = nullptr;
   // 16 bit UNORM: the 8 bit target's [0, 1] range (the chain relies on its
   // clamp) without its banding at the finer size
   native->CreateTexture(width, height, 1, D3DUSAGE_RENDERTARGET,
      D3DFMT_A16B16G16R16, D3DPOOL_DEFAULT, &texture,
      nullptr);
   return texture;
}

// The game's StretchRect of the target's region into a level texture,
// repeated from the Luma texture into that texture's shadow
static void BloomMirrorCopy(device* device, resource source, resource dest,
   const subresource_box* source_box,
   const subresource_box* dest_box)
{
   if (source.handle != bloom.target || !bloom.texture || !state.luma_bloom ||
       luma_bypassed || Mutated(MUT_NO_LUMA_BLOOM))
   {
      return;
   }
   IDirect3DDevice9* native = NativeOf(device);
   const resource_desc dest_desc = device->get_resource_desc(dest);
   // Level textures are render targets (a GetRenderTargetData readback is not)
   if ((dest_desc.usage & resource_usage::render_target) == 0)
   {
      return;
   }
   const UINT scale = bloom.scale;
   auto shadow =
      std::ranges::find(bloom.shadows, dest.handle, &BloomShadow::game_texture);
   if (shadow == bloom.shadows.end())
   {
      IDirect3DTexture9* texture =
         BloomCreateTexture(native, dest_desc.texture.width * scale,
            dest_desc.texture.height * scale);
      if (!texture)
      {
         return;
      }
      bloom.shadows.push_back({.game_texture = dest.handle, .texture = texture});
      shadow = bloom.shadows.end() - 1;
   }
   const auto scaled = [scale](const subresource_box* box, UINT width,
                          UINT height)
   {
      return box ? RECT{LONG(box->left * scale), LONG(box->top * scale),
                      LONG(box->right * scale), LONG(box->bottom * scale)}
                 : RECT{0, 0, LONG(width * scale), LONG(height * scale)};
   };
   const RECT from =
      scaled(source_box, dest_desc.texture.width, dest_desc.texture.height);
   RECT to = scaled(dest_box, dest_desc.texture.width, dest_desc.texture.height);
   if (dest_box && !source_box)
   {
      to.right = to.left + (from.right - from.left);
      to.bottom = to.top + (from.bottom - from.top);
   }
   IDirect3DSurface9* destination = nullptr;
   shadow->texture->GetSurfaceLevel(0, &destination);
   if (destination &&
       SUCCEEDED(native->StretchRect(bloom.surface, &from, destination, &to,
          D3DTEXF_POINT)))
   {
      ++state.bloom_mirrors;
   }
   Free(&destination);
}

// A bloom chain draw or composite: redirects it and gives the rows it reads
// (c209 composite mode, c210 t0 size, c211 the chain's scale / footprint).
// False for the game's draw as it is.
static bool BloomDraw(device* device, float rows[3][4])
{
   const uint32_t crc = out.bound_ps_crc;
   const bool is_source = bloom.source_crcs.contains(crc),
              is_blur = bloom.blur_crcs.contains(crc),
              is_copy = bloom.copy_crcs.contains(crc);
   if (!is_source && !is_blur && !is_copy)
   {
      return false;
   }
   const bool enabled = state.luma_bloom && !luma_bypassed && !Mutated(MUT_NO_LUMA_BLOOM);
   IDirect3DDevice9* native = NativeOf(device);
   const uint64_t source = out.ps_textures[0];
   if (!source || !out.rt0.handle)
   {
      return false;
   }
   // The composite: copytex sampling the target, blended
   if (is_copy && source == bloom.target && bloom.target)
   {
      DWORD blend = 0, source_blend = 0, dest_blend = 0;
      native->GetRenderState(D3DRS_ALPHABLENDENABLE, &blend);
      native->GetRenderState(D3DRS_SRCBLEND, &source_blend);
      native->GetRenderState(D3DRS_DESTBLEND, &dest_blend);
      if (!blend || source_blend != D3DBLEND_SRCALPHA ||
          (dest_blend != D3DBLEND_ONE && dest_blend != D3DBLEND_INVSRCALPHA))
      {
         return false;
      }
      const bool luma = enabled && bloom.texture;
      memcpy(rows[0], state.injected + 4, sizeof(rows[0])); // c209: z intensity
      rows[0][0] = (dest_blend == D3DBLEND_ONE ? 2.0f : 3.0f);
      rows[0][1] = luma ? 1.0f : 0.0f;
      TextureSize(device, source, rows[1]);
      if (luma)
      {
         native->GetTexture(0, &bloom.game_s0);
         bloom.restore_s0 = true;
         native->SetTexture(0, bloom.texture);
         TextureSize(device, reinterpret_cast<uint64_t>(bloom.texture), rows[1]);
         ++state.bloom_composites;
      }
      return true;
   }
   if (is_copy && !is_source && !is_blur && out.rt0.handle != bloom.target)
   {
      return false;
   }
   const resource_desc target_desc = device->get_resource_desc(out.rt0);
   const resource_desc source_desc = device->get_resource_desc(resource{source});
   const bool from_scene =
      is_source && source_desc.texture.height > target_desc.texture.height;
   if (from_scene)
   {
      bloom.target = out.rt0.handle;
   }
   if (!enabled || out.rt0.handle != bloom.target)
   {
      return false;
   }
   if (from_scene)
   {
      const uint32_t scale =
         std::max(1u, uint32_t(std::lround(source_desc.texture.height / 720.0)));
      D3DSURFACE_DESC luma_desc = {};
      if (bloom.surface)
      {
         bloom.surface->GetDesc(&luma_desc);
      }
      if (scale != bloom.scale || !bloom.surface ||
          luma_desc.Width != target_desc.texture.width * scale ||
          luma_desc.Height != target_desc.texture.height * scale)
      {
         BloomRelease();
         bloom.texture =
            BloomCreateTexture(native, target_desc.texture.width * scale,
               target_desc.texture.height * scale);
         if (bloom.texture)
         {
            bloom.texture->GetSurfaceLevel(0, &bloom.surface);
         }
         bloom.scale = state.bloom_scale = scale;
      }
   }
   if (!bloom.surface)
   {
      return false;
   }
   IDirect3DTexture9* shadow_texture = nullptr;
   if (!from_scene)
   {
      const auto shadow =
         std::ranges::find(bloom.shadows, source, &BloomShadow::game_texture);
      // Not copied yet (Luma Bloom just turned on): the game's pass, this frame
      if (shadow == bloom.shadows.end())
      {
         return false;
      }
      shadow_texture = shadow->texture;
   }
   native->GetRenderTarget(0, &bloom.game_rt0);
   native->GetViewport(&bloom.game_viewport);
   native->GetScissorRect(&bloom.game_scissor);
   native->SetRenderTarget(0, bloom.surface);
   D3DVIEWPORT9 viewport = bloom.game_viewport;
   viewport.X *= bloom.scale;
   viewport.Y *= bloom.scale;
   viewport.Width *= bloom.scale;
   viewport.Height *= bloom.scale;
   native->SetViewport(&viewport);
   const RECT scissor = {LONG(bloom.game_scissor.left * bloom.scale),
      LONG(bloom.game_scissor.top * bloom.scale),
      LONG(bloom.game_scissor.right * bloom.scale),
      LONG(bloom.game_scissor.bottom * bloom.scale)};
   native->SetScissorRect(&scissor);
   bloom.restore_rt0 = bloom.restore_viewport = bloom.restore_scissor = true;
   uint64_t sampled = source;
   if (shadow_texture)
   {
      native->GetTexture(0, &bloom.game_s0);
      native->SetTexture(0, shadow_texture);
      bloom.restore_s0 = true;
      sampled = reinterpret_cast<uint64_t>(shadow_texture);
   }
   memcpy(rows[0], state.injected + 4, sizeof(rows[0]));
   TextureSize(device, sampled, rows[1]);
   // filter05 averages the footprint of each texel (y, zw: source texels per
   // target texel); the blurs widen each tap to a game texel at the scale (x)
   rows[2][0] = float(bloom.scale);
   rows[2][1] = is_source ? 1.0f : 0.0f;
   rows[2][2] = rows[1][0] / float(target_desc.texture.width * bloom.scale);
   rows[2][3] = rows[1][1] / float(target_desc.texture.height * bloom.scale);
   ++state.bloom_draws;
   McpTraceEvent("custom", "luma_bloom");
   return true;
}

// RT0's size (the back buffer's while none was bound since the last Reset)
// and the kernel target size of Fix High Resolution Effects (kernel_720: RT0
// size * 720 / RT0 height).
static void Rt0Size(float* width, float* height)
{
   *width = float(out.rt0_width ? out.rt0_width : state.width);
   *height = float(out.rt0_height ? out.rt0_height : state.height);
}

static std::array<float, 4> KernelSize()
{
   float rt0_width = 0, rt0_height = 0;
   Rt0Size(&rt0_width, &rt0_height);
   const float kernel_scale =
      (state.kernel_720 && rt0_height > 0 ? 720.0f / rt0_height : 1.0f);
   return {rt0_width * kernel_scale, rt0_height * kernel_scale, 0, 0};
}

// The settings into Core's state (SMAA, RCAS, the display map, the dither,
// XeGTAO, Luma Bloom, the kernels' c194, Hide UI) and the shadow map size.
// Core's D9N_* settings (DEVELOPMENT env, ini) read at create_device win over
// these until a setting is edited.
static void ApplySettings()
{
   const CB::LumaGameSettings& settings = cb_luma_global_settings.GameSettings;
   state.smaa = g_smaa_enable;
   state.smaa_t2x = g_smaa_t2x;
   state.smaa_predication = g_smaa_predication;
   state.rcas_sharpness = g_rcas_sharpness;
   state.dither = settings.Dithering > 0.5f;
   state.map_contrast = settings.Contrast;
   state.map_saturation = settings.Saturation;
   state.map_highlights_desaturation = settings.HighlightsDesaturation;
   state.gtao = g_gtao_enable;
   state.luma_bloom = g_luma_bloom_enable;
   state.kernel_720 = g_resolution_scaled_effects;
   state.hide_ui = g_hide_ui;
   ShadowMap::Apply();
}

// The Luma block's settings rows (Includes/Luma_Block_sm3.hlsli): c208 the
// grade's transfer-encoded exposure gain, its intensity (1 = vanilla, always
// written: 0 is an intensity) and its HDR path (TONEMAP_TYPE 1, in "OnInit",
// on an HDR display mode), c209.z the bloom intensity, c214 the movies'
// AutoHDR and its peak relative to the UI paper white (1 in SDR: an
// identity). Per present: the display mode can change under the game. Inert
// under "luma_bypassed": the game's own grades draw then.
static void UpdateLumaRows()
{
   const CB::LumaGameSettings& settings = cb_luma_global_settings.GameSettings;
   const bool hdr = EffectiveMode() != OUTPUT_SRGB;
   state.injected[0] = std::pow(settings.Exposure, 1.f / 2.2f);
   state.injected[1] = settings.ColorGradingIntensity;
   state.injected[2] = (hdr ? 1.f : 0.f);
   state.injected[4 * (209 - INJECT_BASE_REGISTER) + 2] =
      settings.BloomIntensity;
   float* const c214 = state.injected + 4 * (214 - INJECT_BASE_REGISTER);
   c214[0] = settings.VideoAutoHDREnable;
   const float video_peak =
      std::lerp(srgb_white_level, 250.f, settings.VideoAutoHDRBoost) /
      srgb_white_level;
   c214[1] = (hdr ? std::max(std::min(video_peak,
                                cb_luma_global_settings.ScenePeakWhite /
                                   cb_luma_global_settings.UIPaperWhite),
                       1.f)
                  : 1.f);
}

class Bayonetta final : public D3D9Game
{
public:
   void OnInit() override
   {
      shaders_folder = "Bayonetta";
      shader_defines = {"VANILLA_ENCODING_TYPE=1", "UI_DRAW_TYPE=2",
         "TONEMAP_TYPE=1", "LUMA_SETTINGS_CB_INDEX=b13",
         "LUMA_DATA_CB_INDEX=b12", "LUMA_UI_DATA_CB_INDEX=b11"};
      display_map_file = "Luma_Bayonetta_DisplayMap.hlsl";
      smaa_file = "Luma_SMAA_impl.hlsl";
      depth_edges_file = "Luma_Bayonetta_DepthEdges.hlsl";
      // XeGTAO at the chosen quality ("XeGTAO Quality", the dgVoodoo mod's
      // advanced define); the final denoise writes R32F, which D3D9 can share
      gtao_file = "Luma_Bayonetta_XeGTAO.hlsl";
      if (!OfflineHarness())
      {
         reshade::get_config_value(nullptr, NAME, "GTAOQuality", g_gtao_quality);
         g_gtao_quality = std::clamp(g_gtao_quality, 0, 4);
      }
      gtao_defines = GtaoDefines();
      // ponytail: ResetRender = the frame's first hand-off copy into the canvas,
      // without the D3D11 mod's opaque-blend test
      out.handoff_crcs.insert(std::begin(kHandoffPs), std::end(kHandoffPs));
      out.video_crcs.insert(std::begin(kVideoPs), std::end(kVideoPs));
      out.velocity_crcs.insert(std::begin(kVelocityPs), std::end(kVelocityPs));
      // The old mod's canvas UI clamp (BAYONETTA_PARITY.md "TOK UI clamp"): idshader* and HwShader* UI PS that
      // overshoot 1 once their target is fp16
      out.saturate_crcs = {0x71EEA0A8, 0x00E292BD, 0xE14F8261, 0x0D39EA9E, 0x834AB455, 0x489680FE, 0xE216D71E,
         0x186D092D, 0x74DD20F6, 0xA3BCCE79, 0xB8CDE205, 0x735B0BC2, 0xBF42633C, 0x368CF889, 0x43AA62C8, 0x4F7114E1,
         0x085B47D2, 0xBD99E5E8, 0x8A6C4B76, 0xBC0FCE3A, 0x8B655F18, 0x741266F1, 0x6C31FC9D, 0xA32E28C7, 0x41A1D13C};
      development_crc_lists = {{"D9N_SCENE_POST_PS", &mv.post_crcs},
         {"D9N_KERNEL_PS", &bay.kernel_crcs},
         {"D9N_SSAO_PS", &bay.ssao_crcs},
         {"D9N_BLOOM_SOURCE_PS", &bloom.source_crcs},
         {"D9N_BLOOM_BLUR_PS", &bloom.blur_crcs},
         {"D9N_BLOOM_COPY_PS", &bloom.copy_crcs}};
      char mv_text[8] = "";
      const bool mv_set = ReadSetting("MV", mv_text, sizeof(mv_text));
      if (OfflineHarness())
      {
         // The harness' runs: motion vectors only when asked for (MV, GTAO or SR),
         // its own inputs
         char gtao_text[8] = "";
         ReadSetting("GTAO", gtao_text, sizeof(gtao_text));
         g_mv = (mv_text[0] && mv_text[0] != '0') ||
                (gtao_text[0] && gtao_text[0] != '0') || g_sr_upscaler >= 0;
      }
      else
      {
         // ponytail: motion vectors always on (MV=0 turns them off): XeGTAO's
         // camera, SMAA T2x, Luma TAA and the SR bridge need them, and the patched
         // shaders are created with the game's; gate them on those settings if the
         // doubled scene VS costs too much
         g_mv = !mv_set || mv_text[0] != '0';
         reshade::get_config_value(nullptr, NAME, "SMAAEnable", g_smaa_enable);
         reshade::get_config_value(nullptr, NAME, "SMAAT2x", g_smaa_t2x);
#if DEVELOPMENT // Development settings only (see "DrawImGuiSettings"): a saved
                // "off" must not outlive its control
         reshade::get_config_value(nullptr, NAME, "SMAAPredication",
            g_smaa_predication);
         reshade::get_config_value(nullptr, NAME, "SMAAPredicationTolerance",
            g_smaa_pred_tolerance);
#endif
         reshade::get_config_value(nullptr, NAME, "RCASSharpness",
            g_rcas_sharpness);
         reshade::get_config_value(nullptr, NAME, "GTAOEnable", g_gtao_enable);
         reshade::get_config_value(nullptr, NAME, "GTVBAOEnable", g_gtvbao_enable);
         reshade::get_config_value(nullptr, NAME, "FixHighResolutionEffects",
            g_resolution_scaled_effects);
         reshade::get_config_value(nullptr, NAME, "FixFramePacing",
            g_fix_frame_pacing);
         reshade::get_config_value(nullptr, NAME, "60FPSCutscenes",
            g_60_fps_cutscenes);
         reshade::get_config_value(nullptr, NAME, "LumaBloomEnable",
            g_luma_bloom_enable);
         reshade::get_config_value(nullptr, NAME, "HighResolutionShadows",
            g_high_resolution_shadows);
         CB::LumaGameSettings& settings = cb_luma_global_settings.GameSettings;
         settings = DEFAULT_GAME_SETTINGS;
         reshade::get_config_value(nullptr, NAME, "Dithering", settings.Dithering);
         reshade::get_config_value(nullptr, NAME, "VideoAutoHDREnable",
            settings.VideoAutoHDREnable);
         reshade::get_config_value(nullptr, NAME, "VideoAutoHDRBoost",
            settings.VideoAutoHDRBoost);
         reshade::get_config_value(nullptr, NAME, "Exposure", settings.Exposure);
         reshade::get_config_value(nullptr, NAME, "Contrast", settings.Contrast);
         reshade::get_config_value(nullptr, NAME, "Saturation",
            settings.Saturation);
         reshade::get_config_value(nullptr, NAME, "HighlightsDesaturation",
            settings.HighlightsDesaturation);
         reshade::get_config_value(nullptr, NAME, "ColorGradingIntensity",
            settings.ColorGradingIntensity);
         reshade::get_config_value(nullptr, NAME, "BloomIntensity",
            settings.BloomIntensity);
         FrameLimiter::Install();
         ShadowMap::Install();
         ApplySettings();
         UpdateLumaRows();
         // The canvas stays gamma encoded (VANILLA_ENCODING_TYPE 1): the scene
         // copy before the UI does the HDR display map and pre-scales the scene by
         // the UI paper white, the composition decodes it and the UI clamp keeps
         // the UI at its paper white
         g_decode_gamma = 2.2f;
         state.display_map = state.present_clamp = 1;
      }
      state.mv_enabled = g_mv;
      mv_text[0] = 0;
      state.force_jitter =
         DevelopmentEnv("D9N_MV_JITTER", mv_text, sizeof(mv_text)) &&
         mv_text[0] != '0';
   }

   // Core's settings KERNEL_720, GTAO, LUMA_BLOOM and HIDE_UI (DEVELOPMENT env,
   // ini) over the game's
   void OnCreateDevice() override
   {
      if (OfflineHarness())
      {
         state.kernel_720 = state.luma_bloom = 1;
      }
      char text[64] = "";
      if (ReadSetting("KERNEL_720", text, sizeof(text)))
      {
         state.kernel_720 = atoi(text) != 0;
      }
      if (ReadSetting("GTAO", text, sizeof(text)))
      {
         state.gtao = atoi(text) != 0;
      }
      if (ReadSetting("LUMA_BLOOM", text, sizeof(text)))
      {
         state.luma_bloom = atoi(text) != 0;
      }
      if (ReadSetting("HIDE_UI", text, sizeof(text)))
      {
         state.hide_ui = atoi(text) != 0;
      }
   }

   // The largest 16:9 rectangle inside the back buffer (Bayonetta renders a
   // 16:9 cut of its back buffer).
   void GetRenderSize(uint32_t bb_width, uint32_t bb_height, uint32_t* width,
      uint32_t* height) override
   {
      *height = std::min<UINT>(bb_height, bb_width * 9 / 16);
      *width = std::min<UINT>(bb_width, bb_height * 16 / 9);
      if (*width * 9 > *height * 16)
      {
         *width = *height * 16 / 9;
      }
      else
      {
         *height = *width * 9 / 16;
      }
   }

   void OnInitPipeline(device* device, uint32_t count,
      const pipeline_subobject* subobjects,
      pipeline pipeline) override
   {
      if (g_mv)
      {
         MvInitPipeline(device, count, subobjects, pipeline);
      }
   }

   void OnDestroyPipeline(uint64_t pipeline) override
   {
      if (g_mv)
      {
         MvDestroyPipeline(pipeline);
      }
   }

   void OnBindPipeline(pipeline_stage stages, pipeline pipeline) override
   {
      if (!g_mv)
      {
         return;
      }
      // The game's own bind replaces whatever patched shader we put on the
      // device
      if ((stages & pipeline_stage::vertex_shader) != 0)
      {
         mv.game_vs = pipeline.handle;
         mv.applied_vs = false;
      }
      if ((stages & pipeline_stage::pixel_shader) != 0)
      {
         mv.game_ps = pipeline.handle;
         mv.applied_ps = false;
      }
   }

   void OnShadersReloaded() override
   {
      MvFreeShaders();
   }

   // The MV copies of the replaced pipelines of "crc" ("MvInitPipeline()" patched the create-time replacement), patched again from the
   // reloaded one
   void OnReplacementReloaded(uint32_t crc, std::span<const uint8_t> bytecode) override
   {
      if (!g_mv || !g_device)
      {
         return;
      }
      std::vector<uint32_t> patched;
      std::string error;
      if (!Sm3MvPatch::PatchPixelShader(reinterpret_cast<const uint32_t*>(bytecode.data()), bytecode.size() / 4, &patched, &error))
      {
         Log("d9native: MV patch refused the reloaded PS %08X: %s", crc, error.c_str());
         patched.clear();
      }
      for (auto& [handle, entry] : mv.shaders)
      {
         // A VS pipeline's CRC there is 0
         if (entry.vs || !out.pipeline_crc.contains(handle) || out.pipeline_crc.at(handle) != crc || !out.matched_pipelines.contains(handle))
         {
            continue;
         }
         if (entry.ps)
         {
            Free(&entry.ps);
            --state.mv_shaders_live;
         }
         if (!patched.empty() &&
             SUCCEEDED(NativeOf(g_device)->CreatePixelShader(reinterpret_cast<const DWORD*>(patched.data()), &entry.ps)))
         {
            ++state.mv_shaders_live;
         }
      }
   }

   void OnBindRenderTargets(device* device, uint32_t count,
      const resource_view* rtvs,
      resource_view depth_stencil) override
   {
      if (g_mv)
      {
         MvOnBindRenderTargets(device, count, rtvs, depth_stencil);
      }
      // Its RT0 (and the viewport D3D9 resets with it) replaces ours
      bloom.restore_rt0 = bloom.restore_viewport = false;
   }

   void OnBindPixelTextures(uint32_t first, uint32_t count) override
   {
      if (first == 0 && count > 0)
      {
         bloom.restore_s0 = false;
      }
   }

   // The vertex constants (MV's previous constants) and the shadow of the
   // game's c194, which the kernel draws overwrite.
   void OnPushConstants(shader_stage stages, uint32_t layout_param,
      uint32_t first, uint32_t count,
      const void* values) override
   {
      if (g_mv && stages == shader_stage::vertex && layout_param == 2 &&
          first + count <= MV_VS_CONSTANTS * 4)
      {
         memcpy(mv.vs_const + first, values, count * sizeof(float));
      }
      constexpr uint32_t c194_first = GAME_TARGET_SIZE_REGISTER * 4;
      if (stages != shader_stage::pixel || layout_param != 5 ||
          first + count <= c194_first || first >= c194_first + 4)
      {
         return;
      }
      for (uint32_t i = std::max(first, c194_first);
         i < std::min(first + count, c194_first + 4); ++i)
      {
         state.game_c194[i - c194_first] =
            static_cast<const float*>(values)[i - first];
      }
      bay.c194_overridden = false; // the game's value is on the device again
   }

   void OnDrawBegin(device* device, uint32_t count, uint32_t first,
      int32_t vertex_offset) override
   {
      BloomRestore(NativeOf(device));
      // Under "luma_bypassed" no scene opens: the game's VS and PS draw (no jitter, no RT1), no camera, no SR
      if (g_mv && !luma_bypassed)
      {
         MvOnDraw(device, count, first, vertex_offset);
      }
   }

   // c210.xy s0 size, c211.zw s0 texels per RT0 pixel, c212.xy kernel target
   // size, c213.xy s14 size.
   void OnInjectConstants(device* device, float* rows) override
   {
      if (Mutated(MUT_NO_HOST_CONSTS))
      {
         return;
      }
      float rt0_width = 0, rt0_height = 0;
      Rt0Size(&rt0_width, &rt0_height);
      float* const c210 = rows + 4 * (210 - INJECT_BASE_REGISTER);
      float *const c211 = c210 + 4, *const c212 = c210 + 8,
                   *const c213 = c210 + 12;
      TextureSize(device, out.ps_textures[0], c210);
      if (out.ps_textures[0] && rt0_width > 0 && rt0_height > 0)
      {
         c211[2] = c210[0] / rt0_width; // source texels per target pixel (what
                                        // BloomSource's ddx/ddy gave)
         c211[3] = c210[1] / rt0_height;
      }
      memcpy(c212, KernelSize().data(), sizeof(float) * 2);
      TextureSize(device, out.ps_textures[SHADOW_MAP_SAMPLER], c213);
      ++state.host_rows_draws;
   }

   // Luma Bloom's rows, the kernels' c194, XeGTAO in place of the SSAO draw,
   // HideUI.
   bool OnDraw(command_list* cmd_list) override
   {
      device* device = cmd_list->get_device();
      if (float rows[3][4] = {}; BloomDraw(device, rows))
      {
         cmd_list->push_constants(shader_stage::pixel, pipeline_layout{0}, 5,
            (state.inject_base + 1) * 4, 12, rows);
      }
      // Fix High Resolution Effects for the unreplaced kernels (route C): their
      // c194 is the kernel size for this draw only (the game's own under "luma_bypassed").
      if (!luma_bypassed && !Mutated(MUT_NO_HOST_CONSTS) && state.kernel_720 &&
          bay.kernel_crcs.contains(out.bound_ps_crc))
      {
         cmd_list->push_constants(shader_stage::pixel, pipeline_layout{0}, 5,
            GAME_TARGET_SIZE_REGISTER * 4, 4,
            KernelSize().data());
         bay.c194_overridden = true;
         ++state.kernel_pushes;
      }
      else if (bay.c194_overridden && !Mutated(MUT_NO_KERNEL_RESTORE))
      {
         cmd_list->push_constants(shader_stage::pixel, pipeline_layout{0}, 5,
            GAME_TARGET_SIZE_REGISTER * 4, 4,
            state.game_c194);
         bay.c194_overridden = false;
         ++state.kernel_restores;
      }
      if (bay.ssao_crcs.contains(out.bound_ps_crc) && out.ps_textures[2])
      {
         bay.depth_texture = out.ps_textures[2];
         bay.depth_present = state.presents;
         if (state.gtao && !luma_bypassed)
         {
            if (RunGtao(device))
            {
               return true;
            }
            ++state.gtao_fallbacks;
            McpTraceEvent("custom", "gtao_fallback");
         }
      }
      // HideUI: every draw into the canvas after this frame's ResetRender is the
      // UI, except the movies
      if (state.hide_ui && !luma_bypassed && !Mutated(MUT_NO_HIDE_UI) && out.rt0_is_back_buffer &&
          Redirecting() && out.last_reset_render == state.presents &&
          !out.video_crcs.contains(out.bound_ps_crc))
      {
         ++state.hidden_ui_draws;
         return true;
      }
      return false;
   }

   // A clear with our RT1 still bound would wipe the motion target too.
   void OnClear(IDirect3DDevice9* native) override
   {
      if (g_mv)
      {
         MvRestoreGame(native);
      }
      BloomRestore(native);
   }

   void OnCopyTextureRegion(device* device, resource source,
      resource destination,
      const subresource_box* source_box,
      const subresource_box* destination_box) override
   {
      BloomRestore(NativeOf(device));
      BloomMirrorCopy(device, source, destination, source_box, destination_box);
      if (g_mv && mv.open && !mv.ended && mv.scene_resource.handle &&
          source.handle == mv.scene_resource.handle)
      {
         mv.copy_dest =
            destination; // the game's copy of the scene (post pass input)
      }
   }

   void OnDestroyResource(uint64_t resource) override
   {
      if (g_mv && mv.copy_dest.handle == resource)
      {
         mv.copy_dest = {0};
      }
      if (resource == bloom.target)
      {
         bloom.target = 0;
      }
      if (const auto shadow = std::ranges::find(bloom.shadows, resource,
             &BloomShadow::game_texture);
         shadow != bloom.shadows.end())
      {
         Free(&shadow->texture);
         bloom.shadows.erase(shadow);
      }
      if (bay.depth_texture == resource)
      {
         bay.depth_texture = 0;
      }
   }

   void OnRunPasses(device* device, const D3DSURFACE_DESC& desc,
      D3D9Passes::Params* params) override
   {
      params->predication =
         params->smaa && state.smaa_predication && CopyDepth(device);
      params->predication_tolerance = g_smaa_pred_tolerance;
      // T2x on this frame's motion vectors (filled at the scene's end, so
      // ordered before the D3D9 signal too); Bayonetta's DrawSMAAOnCanvas: off
      // under SR
      params->t2x = params->smaa && state.smaa_t2x && !sr.ready &&
                    !state.luma_taa && mv.filled && mv.kmt[0] &&
                    mv.tex_w == desc.Width && mv.tex_h == desc.Height;
      if (params->t2x && g_pass.motion != mv.kmt[0])
      {
         params->t2x = D3D9Passes::SetMotion(mv.kmt[0]);
         g_pass.motion = params->t2x ? mv.kmt[0] : nullptr;
      }
      params->t2x_history = mv.t2x_ran_last && !Mutated(MUT_NO_T2X_HISTORY);
      params->t2x_current = int(mv.frame_index & 1);
      params->t2x_phase = mv.t2x_phase;
   }

   void OnPassesRan(const D3D9Passes::Params& params) override
   {
      mv.t2x_ran = params.t2x;
   }

   void OnStartUpscaler(device* device) override
   {
      StartUpscaler(device);
   }

   bool UpscaledLastFrame() const override
   {
      return mv.sr_ran_last;
   }

   // Its shared textures and previous bones go before a Reset
   void OnDestroyShared(device* device) override
   {
      if (g_mv)
      {
         MvDestroy(device);
      }
   }

   void OnDestroySwapchain(device* device, bool resize) override
   {
      BloomRestore(NativeOf(device));
      BloomRelease();
      bloom.target = 0;
      if (g_mv && !resize)
      {
         // ReShade's D3D9 runtime fires no destroy_pipeline for the game's
         // shaders: the patched copies go with the device
         while (!mv.shaders.empty())
         {
            MvReleaseShader(mv.shaders.begin()->first);
         }
      }
   }

   void OnPresent(device* device) override
   {
      BloomRestore(NativeOf(device));
      if (!OfflineHarness())
      {
         UpdateLumaRows();
      }
      if (g_mv && g_bound)
      {
         MvOnPresent(device);
      }
      // A "luma_bypassed" change restarts every history: the next scene has no previous camera, so the SR frame resets and Luma TAA
      // and XeGTAO restart; SMAA T2x restarts as no bypassed frame resolved
      if (bypass_changed)
      {
         mv.camera_valid = mv.previous_valid = false;
      }
   }

   void DrawImGuiSettings() override
   {
      bool changed = false;
      ImGui::SeparatorText("Anti-Aliasing");
      if (state.msaa_active)
      {
         ImGui::TextColored(ImVec4(1.f, 0.8f, 0.f, 1.f),
            "MSAA is on: DLAA, FSR, XeSS, Luma TAA and SMAA T2x "
            "need Anti-Aliasing off in the game's options.");
      }
      // The upscalers (Core's SR / LUMA_TAA settings) replace SMAA: greyed, its
      // value kept. Not with MSAA: SMAA runs then.
      const bool sr_active =
         (g_sr_upscaler >= 0 || state.luma_taa) && !state.msaa_active;
      ImGui::BeginDisabled(sr_active);
      changed |= DrawSettingCheckbox(
         "SMAA Enable", "SMAAEnable", &g_smaa_enable, true,
         "Adds SMAA anti-aliasing on top of the game's own MSAA (or alone with it "
         "off; not used with DLSS/FSR/XeSS or Luma TAA).",
         sr_active);
      ImGui::EndDisabled();
      // T2x needs the motion vectors, which a multisampled scene doesn't get
      const bool t2x_unavailable = sr_active || state.msaa_active;
      ImGui::BeginDisabled(!g_smaa_enable || t2x_unavailable);
      changed |= DrawSettingCheckbox(
         "SMAA T2x", "SMAAT2x", &g_smaa_t2x, false,
         "Smoother edges and less shimmering, by blending each frame with the "
         "previous one.\nCan look slightly softer in motion. Not used with "
         "DLSS/FSR/XeSS, Luma TAA or the game's MSAA (Anti-Aliasing in the game's "
         "options).",
         t2x_unavailable);
      ImGui::EndDisabled();
#if DEVELOPMENT
      // Predication only relaxes the edge threshold back to ULTRA's on geometry,
      // never below: off is strictly worse (MoH 2010, ME1)
      if (g_smaa_enable)
      {
         changed |= DrawSettingCheckbox("SMAA Predication", "SMAAPredication",
            &g_smaa_predication, true,
            "Edges from the depth prepass' geometry "
            "(needs SSAO on in the game's options).");
         ImGui::BeginDisabled(!g_smaa_predication);
         DrawSettingSlider("SMAA Predication Tolerance", "SMAAPredicationTolerance",
            &g_smaa_pred_tolerance, 0.02f, 0.002f, 0.2f,
            "Deviation from the local depth plane that counts as an "
            "edge, as a fraction of view depth. Lower = more edges.",
            "%.3f", ImGuiSliderFlags_Logarithmic);
         ImGui::EndDisabled();
      }
#endif
      changed |= DrawSettingSlider(
         "RCAS Sharpness", "RCASSharpness", &g_rcas_sharpness, 0.f, 0.f, 1.f,
         "Sharpening applied on top of anti-aliasing (0 = off).");

      CB::LumaGameSettings& settings = cb_luma_global_settings.GameSettings;
      ImGui::SeparatorText("Grade");
      // c208.x in Grade_* and CentreAverage_*
      DrawSettingSlider("Exposure", "Exposure", &settings.Exposure,
         DEFAULT_GAME_SETTINGS.Exposure, 0.f, 2.f,
         "Overall image brightness (1 = vanilla).");
      // The display map (Luma_Bayonetta_DisplayMap.hlsl)
      changed |=
         DrawSettingSlider("Contrast", "Contrast", &settings.Contrast,
            DEFAULT_GAME_SETTINGS.Contrast, 0.f, 2.f,
            "Overall image contrast, HDR only (1 = vanilla).");
      changed |= DrawSettingSlider("Saturation", "Saturation", &settings.Saturation,
         DEFAULT_GAME_SETTINGS.Saturation, 0.f, 2.f,
         "Color saturation, HDR only (1 = vanilla).");
      changed |=
         DrawSettingSlider("Highlights Desaturation", "HighlightsDesaturation",
            &settings.HighlightsDesaturation,
            DEFAULT_GAME_SETTINGS.HighlightsDesaturation, 0.f, 1.f,
            "How far the brightest sources fade to neutral white, "
            "HDR only (0 = keep color at any brightness).");
      // c208.y in Grade_*
      DrawSettingSlider("Color Grading Intensity", "ColorGradingIntensity",
         &settings.ColorGradingIntensity,
         DEFAULT_GAME_SETTINGS.ColorGradingIntensity, 0.f, 1.f,
         "Strength of the game's own color grading (1 = vanilla, 0 = neutral).");

      ImGui::SeparatorText("Bloom");
      changed |= DrawSettingCheckbox(
         "Luma Bloom Enable", "LumaBloomEnable", &g_luma_bloom_enable, true,
         "Replaces the game's bloom with a higher resolution one (the same look, "
         "without its blocky, flickering glow).");
      // c209.z on the bloom composite (Copy_0xB8690130)
      DrawSettingSlider("Bloom Intensity", "BloomIntensity",
         &settings.BloomIntensity,
         DEFAULT_GAME_SETTINGS.BloomIntensity, 0.f, 2.f,
         "Bloom strength (1 = vanilla, 0 = none).");

      ImGui::SeparatorText("Ambient Occlusion");
      changed |= DrawSettingCheckbox(
         "XeGTAO Enable", "GTAOEnable", &g_gtao_enable, true,
         "Replaces the game's SSAO with XeGTAO (cleaner, more accurate ambient "
         "occlusion; requires SSAO enabled in the game's options).");
      // Read in "RunGtao" at each run
      DrawSettingCheckbox("GT-VBAO Enable", "GTVBAOEnable", &g_gtvbao_enable, false,
         "Replaces XeGTAO's occlusion with GT-VBAO (less dark halo around thin "
         "objects). Needs XeGTAO Enable.",
         !g_gtao_enable, false);
      ImGui::BeginDisabled(!g_gtao_enable);
      if (ImGui::Combo("XeGTAO Quality", &g_gtao_quality,
             "Low\0Medium\0High\0Very High\0Ultra\0"))
      {
         reshade::set_config_value(nullptr, NAME, "GTAOQuality", g_gtao_quality);
         gtao_defines = GtaoDefines();
      }
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
      {
         ImGui::SetTooltip("XeGTAO's slice count (Very High = the default). "
                           "Recompiles its shaders when changed.");
      }
      ImGui::EndDisabled();
#if DEVELOPMENT || TEST
      ImGui::BeginDisabled(!g_gtao_enable);
      ImGui::SliderFloat("GTAO Final Value Power", &g_gtao_final_value_power, GTAO_FINAL_VALUE_POWER_MIN,
         GTAO_FINAL_VALUE_POWER_MAX, "%.2f");
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
      {
         ImGui::SetTooltip("XeGTAO's darkness dial (higher = darker AO).");
      }
      ImGui::SliderFloat("GT-VBAO Final Value Power", &g_gtvbao_final_value_power,
         GTAO_FINAL_VALUE_POWER_MIN, GTAO_FINAL_VALUE_POWER_MAX, "%.2f");
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
      {
         ImGui::SetTooltip("GT-VBAO's darkness dial (higher = darker AO).");
      }
      ImGui::SliderFloat("GTAO Depth Scale", &g_gtao_depth_scale, 0.01f, 200.f,
         "%.2f", ImGuiSliderFlags_Logarithmic);
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
      {
         ImGui::SetTooltip("View depth divisor (game units -> meters): the scale "
                           "XeGTAO's radius and falloff are tuned for.");
      }
      ImGui::SliderFloat("GTAO Radius Override", &g_gtao_radius_override, 0.f, 5.f,
         "%.3f");
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
      {
         ImGui::SetTooltip("0 = the shader's EFFECT_RADIUS; > 0 overrides it (view "
                           "units after the depth scale).");
      }
#if DEVELOPMENT // The shader's debug blocks are DEVELOPMENT only
      ImGui::Combo("GTAO Debug View", &g_gtao_debug_view,
         "Off\0Depth gradient\0Normals\0AO x8\0Edges\0");
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
      {
         ImGui::SetTooltip("Shows XeGTAO's diagnostics instead of the scene. Depth "
                           "gradient dead or flat, or blocky normals = a wrong "
                           "input; AO x8 = broad over-occlusion.");
      }
      ImGui::SliderFloat("GTAO History Weight", &g_gtao_history_weight, 0.f, 0.98f,
         "%.2f");
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
      {
         ImGui::SetTooltip("The AO history's weight under DLSS/FSR/XeSS or Luma TAA "
                           "(0 = off).");
      }
#endif
      ImGui::EndDisabled();
#endif

      ImGui::SeparatorText("Effects");
      // c214 in Video_0x831047DD. Inert in SDR (see "UpdateLumaRows").
      DrawSettingCheckbox("Video AutoHDR", "VideoAutoHDREnable",
         &settings.VideoAutoHDREnable,
         DEFAULT_GAME_SETTINGS.VideoAutoHDREnable,
         "Adds HDR highlights to pre-rendered videos (HDR only).");
      ImGui::BeginDisabled(settings.VideoAutoHDREnable <= 0.5f);
      DrawSettingSlider("Video HDR Boost", "VideoAutoHDRBoost",
         &settings.VideoAutoHDRBoost,
         DEFAULT_GAME_SETTINGS.VideoAutoHDRBoost, 0.f, 1.f,
         "Video highlight strength (0 = off).");
      ImGui::EndDisabled();
      changed |= DrawSettingCheckbox("Dithering", "Dithering", &settings.Dithering,
         DEFAULT_GAME_SETTINGS.Dithering,
         "Reduces gradient banding.");
      // On the Steam executable only ("ShadowMap::Install")
      changed |= DrawSettingCheckbox(
         "High Resolution Shadows", "HighResolutionShadows",
         &g_high_resolution_shadows, true,
         "Sharper, steadier shadow outlines at the game's highest shadow quality. "
         "Applies with the next level load or options change.",
         !ShadowMap::patchable);

      ImGui::SeparatorText("UI");
      changed |= ImGui::Checkbox("Hide Gameplay UI", &g_hide_ui);
      if (ImGui::IsItemHovered())
      {
         ImGui::SetTooltip("Disables the in-game UI.");
      }

      ImGui::SeparatorText("Fixes");
      changed |= DrawSettingCheckbox(
         "Fix High Resolution Effects", "FixHighResolutionEffects",
         &g_resolution_scaled_effects, true,
         "Keeps shadow softness, depth of field and SSAO the size they had at "
         "1280x720, the resolution the game was made for.");
      // Only the Steam executable's limiters are hooked ("FrameLimiter::Install")
      ImGui::BeginDisabled(!FrameLimiter::installed);
      DrawSettingCheckbox("Fix Frame Pacing", "FixFramePacing", &g_fix_frame_pacing,
         true,
         "Replaces the game's frame limiter with a precise one. "
         "Turn off an external frame rate limit with it.",
         !FrameLimiter::installed);
      ImGui::EndDisabled();
      // Patched from the limiter hooks, on the Steam executable only
      // ("FrameLimiter::Apply60FpsCutscenes")
      ImGui::BeginDisabled(!FrameLimiter::cutscenes_patchable);
      DrawSettingCheckbox("60 FPS Cutscenes", "60FPSCutscenes", &g_60_fps_cutscenes,
         true,
         "Draws the real-time cutscenes at 60 fps instead of 30.",
         !FrameLimiter::cutscenes_patchable);
      ImGui::EndDisabled();
#if DEVELOPMENT
      ImGui::Text("Frame limiter: period %u us, spin margin %u us, late %u, "
                  "resyncs %u, adopted %u",
         FrameLimiter::period_us, FrameLimiter::spin_margin_us,
         FrameLimiter::late_frames, FrameLimiter::resyncs,
         FrameLimiter::adopted_deadlines);
#endif
      if (changed)
      {
         ApplySettings();
      }
   }

#if DEVELOPMENT
   // The knobs Core's state doesn't mirror (read where they are used, so live), the motion vector, bloom and depth textures
   void RegisterDevValues() override
   {
      McpD3D9::RegisterValue("smaa_pred_tolerance", "float", &g_smaa_pred_tolerance, 0.002, 0.2);
      McpD3D9::RegisterValue("gtao_final_value_power", "float", &g_gtao_final_value_power, GTAO_FINAL_VALUE_POWER_MIN, GTAO_FINAL_VALUE_POWER_MAX);
      McpD3D9::RegisterValue("gtao_depth_scale", "float", &g_gtao_depth_scale, 0.001, 1000);
      McpD3D9::RegisterValue("gtao_radius_override", "float", &g_gtao_radius_override, 0, 5);
      McpD3D9::RegisterValue("gtvbao_final_value_power", "float", &g_gtvbao_final_value_power, GTAO_FINAL_VALUE_POWER_MIN, GTAO_FINAL_VALUE_POWER_MAX);
      McpD3D9::RegisterValue("gtao_history_weight", "float", &g_gtao_history_weight, 0, 0.98);
      McpD3D9::RegisterValue("gtao_debug_view", "int", &g_gtao_debug_view, 0, 4);
      McpD3D9::RegisterValue("gtvbao_enable", "toggle", &g_gtvbao_enable, 0, 1);
      // The VS / PS patches are made as the game creates its shaders
      McpD3D9::RegisterCounter("mv", &g_mv, "Create time: restart to change");
      CB::LumaGameSettings& settings = cb_luma_global_settings.GameSettings;
      // Into the Luma block's rows at every present ("UpdateLumaRows")
      McpD3D9::RegisterValue("exposure", "float", &settings.Exposure, 0, 2);
      McpD3D9::RegisterValue("bloom_intensity", "float", &settings.BloomIntensity, 0, 2);
      McpD3D9::RegisterValue("video_autohdr_enable", "float", &settings.VideoAutoHDREnable, 0, 1);
      McpD3D9::RegisterValue("video_autohdr_boost", "float", &settings.VideoAutoHDRBoost, 0, 1);
      McpD3D9::RegisterCounter("frame_limiter_late_frames", &FrameLimiter::late_frames);
      McpD3D9::RegisterCounter("frame_limiter_resyncs", &FrameLimiter::resyncs);
      McpD3D9::RegisterCounter("frame_limiter_adopted_deadlines", &FrameLimiter::adopted_deadlines);
      // The shared SR textures (motion, depth, output, color) and our motion RT1
      for (const auto& [name, index] : {std::pair{"mv_motion", 0}, {"mv_depth", 1}, {"mv_output", 2}, {"mv_color", 3}})
      {
         McpD3D9::RegisterTexture(name, [index]
            { return static_cast<IDirect3DResource9*>(mv.textures[index]); });
      }
      McpD3D9::RegisterTexture("mv_motion_rt", []
         { return static_cast<IDirect3DResource9*>(mv.motion_rt); });
      McpD3D9::RegisterTexture("bloom_texture", []
         { return static_cast<IDirect3DResource9*>(bloom.texture); });
      // The game's R32F depth prepass (the SSAO draw's s2), valid in the frame it was drawn
      McpD3D9::RegisterTexture("depth_prepass", []
         { return reinterpret_cast<IDirect3DResource9*>(bay.depth_texture); });
      McpD3D9::setting_keys.insert(McpD3D9::setting_keys.end(), {"GTAO", "HIDE_UI", "KERNEL_720", "LUMA_BLOOM", "MV", "GTAOQuality",
                                                                   "SMAAEnable", "SMAAT2x", "SMAAPredication", "SMAAPredicationTolerance", "RCASSharpness", "GTAOEnable", "GTVBAOEnable",
                                                                   "FixHighResolutionEffects", "FixFramePacing", "60FPSCutscenes", "LumaBloomEnable", "HighResolutionShadows", "Dithering",
                                                                   "VideoAutoHDREnable", "VideoAutoHDRBoost", "Exposure", "Contrast", "Saturation", "HighlightsDesaturation", "ColorGradingIntensity",
                                                                   "BloomIntensity"});
   }
#endif

   void PrintImGuiAbout() override
   {
      ImGui::PushTextWrapPos(0.f);
      ImGui::Text("Luma for \"Bayonetta\" is developed by DristoforColumb and is "
                  "open source and free.\n"
                  "It adds HDR, DLAA, FSR 3 or XeSS native anti-aliasing or Luma "
                  "TAA and SMAA (with T2x), and replaces the game's bloom with a "
                  "higher resolution one and its SSAO with XeGTAO, plus higher "
                  "resolution shadows, even frame pacing and 60 fps cutscenes.\n"
                  "It runs on the game's own DirectX 9 device and presents through "
                  "DirectX 12.\n"
                  "Enable SSAO in the game's options for XeGTAO to apply; SMAA "
                  "works either way.\n"
                  "Do NOT run another HDR mod (e.g. RenoDX) alongside it.\n"
                  "Thanks to the Luma team and contributors.\n"
                  "If you enjoy it, consider donating.");
      ImGui::PopTextWrapPos();

      ImGui::NewLine();
      ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(70, 134, 0, 255));
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
         IM_COL32(70 + 9, 134 + 9, 0, 255));
      ImGui::PushStyleColor(ImGuiCol_ButtonActive,
         IM_COL32(70 + 18, 134 + 18, 0, 255));
      if (ImGui::Button("Buy DristoforColumb a Coffee on ko-fi"))
      {
         ShellExecuteA(nullptr, "open", "https://ko-fi.com/dristoforcolumb", nullptr,
            nullptr, SW_SHOWNORMAL);
      }
      ImGui::PopStyleColor(3);

      ImGui::NewLine();
      if (ImGui::Button("Join our \"HDR Den\" Discord"))
      {
         // Unique link for Luma's HDR Den (tracks the origin of people joining); do
         // not share for other purposes.
         static const std::string discord_link =
            std::string("https://discord.gg/J9fM") + std::string("3EVuEZ");
         ShellExecuteA(nullptr, "open", discord_link.c_str(), nullptr, nullptr,
            SW_SHOWNORMAL);
      }
      if (ImGui::Button("Contribute on Github"))
      {
         ShellExecuteA(nullptr, "open", "https://github.com/Filoppi/Luma-Framework",
            nullptr, nullptr, SW_SHOWNORMAL);
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
                  "\nXeGTAO (Intel)"
                  "\nAMD FidelityFX (RCAS + FSR 3)"
                  "\nNVIDIA NGX (DLSS)"
                  "\nIntel XeSS");
   }
};

static Bayonetta g_bayonetta;

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID)
{
   if (reason == DLL_PROCESS_ATTACH)
   {
      Globals::SetGlobals("Bayonetta", "Bayonetta Luma mod");
   }
   else if (reason == DLL_PROCESS_DETACH)
   {
      FrameLimiter::Uninstall();
      ShadowMap::Uninstall();
   }
   return CoreMain(module, reason, &g_bayonetta);
}
