#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <format>
#include <ranges>
#include <shared_mutex>
#include <string>
#include <unordered_map>

#include "..\..\Core\includes\minhook.h"
#include "..\..\Core\utils\system.h"

// Gameplay bugs of the PC port at frame rates other than 30 fps, fixed in the Steam executable (see "HomingSteering" and the
// namespaces after it)
namespace FrameRateFixes
{
   static bool enabled = true;

   // The Steam executable (patch 1.05). The hooks below use its fixed addresses, as it has no relocations.
   static bool IsSteamExecutable()
   {
      constexpr DWORD STEAM_TIME_DATE_STAMP = 0x596642C6;
      const auto* const image = reinterpret_cast<const uint8_t*>(GetModuleHandleW(nullptr));
      return reinterpret_cast<const IMAGE_NT_HEADERS*>(image + reinterpret_cast<const IMAGE_DOS_HEADER*>(image)->e_lfanew)->FileHeader.TimeDateStamp == STEAM_TIME_DATE_STAMP;
   }

   // How often each fix changed the game's values, logged every 2 s of game time ("LogStats"); counted in DEVELOPMENT only
   enum class Stat
   {
      HOMING_PRESCALED,
      HOMING_RICOCHET,
      HOMING_DEFLECTION,
      UNSCALED_TURN,
      COUNTER_TICK,
      COUNTER_HOLD,
      DAMAGE_LANDED,
      DAMAGE_DROPPED,
      APPROACH_UNSCALED,
      APPROACH_LINEAR,
      APPROACH_STEP_SCALED,
      ENEMY_TURN,
      SNAP_UNSCALED,
      SNAP_LINEAR,
      SNAP_POW,
      VECTOR_UNSCALED,
      VECTOR_LINEAR,
      STUCK_HELD,
      DRAG_XZ, // The "BlockCalls" blocks, in their order
      DRAG_Y,
      EASE_ACTIVE,
      EASE_EXPIRED,
      LEAN,
      DUST_TIMER,
      COUNT,
   };
   constexpr std::array<const char*, size_t(Stat::COUNT)> STAT_NAMES = {"homing_prescaled", "homing_ricochet", "homing_deflection", "unscaled_turn", "counter_tick", "counter_hold",
      "damage_landed", "damage_dropped", "approach_unscaled", "approach_linear", "approach_step_scaled", "enemy_turn", "snap_unscaled", "snap_linear", "snap_pow",
      "vector_unscaled", "vector_linear", "stuck_held", "drag_xz", "drag_y", "ease_active", "ease_expired", "lean", "dust_timer"};
#if DEVELOPMENT
   static std::array<std::atomic<uint32_t>, size_t(Stat::COUNT)> stats = {};
#endif

   static void Count(Stat stat)
   {
#if DEVELOPMENT
      stats[size_t(stat)].fetch_add(1, std::memory_order_relaxed);
#endif
   }

   // DAT_00f0318c: the frame time in 1/30 s
   const float* const FRAME_TIME = reinterpret_cast<const float*>(0xF0318C);
   // An object's time scale; its frame step is time scale * frame time
   constexpr size_t TIME_SCALE = 0x440;

   // A console per frame rate applied over the frame time (per 1/30 s)
   template <typename T>
   static T PerConsoleFrame(T rate, T frame_time)
   {
      return T(1) - std::pow(T(1) - rate, frame_time);
   }

   // The input scale of "v = v * k + x * a" over the frame time that keeps its steady state and its approach per 1/30 s
   static double EmaInputScale(double k, double frame_time)
   {
      return (1.0 - std::pow(k, frame_time)) / (1.0 - k);
   }

   // The hook goes on only if the function starts as expected
   template <typename Function, size_t N>
   static bool AttachVerified(MinHook::HookedFunction<Function>* hooked, uint8_t* target, const std::array<uint8_t, N>& prologue, std::type_identity_t<Function> detour)
   {
      return std::memcmp(target, prologue.data(), N) == 0 && MinHook::Attach(hooked, target, detour);
   }

   // The fix table's uninstall for a MinHook detour
   template <auto* hooked>
   static void Detach()
   {
      MinHook::Detach(hooked);
   }

   // A call or jump at code[offset - 1] to target, its rel32 written into patched (a copy of the bytes at code)
   static void WriteRel32(uint8_t* patched, const uint8_t* code, size_t offset, uintptr_t target)
   {
      const int32_t rel32 = int32_t(intptr_t(target) - reinterpret_cast<intptr_t>(code + offset + sizeof(int32_t)));
      std::memcpy(patched + offset, &rel32, sizeof(rel32));
   }

   // Homing projectiles turned slower the higher the frame rate (half as fast at 60 fps as at 30).
   // FUN_004a3030 (this = projectile, target, direction, rate, max step, scale) turns the projectile by
   // min(angle * rate, max step) (FUN_00b9dec0); with "scale" set it multiplies both by the projectile's frame step (its time scale
   // * DAT_00f0318c, the frame time in 1/30 s). 18 of its 20 callers had already multiplied the max step by that step, one the rate too,
   // so the per second turn shrank with the frame time. The other two are one-shot re-aims, which on the consoles (frame time 1) scaled
   // with the time scale alone, and so shrank with the frame time too. The detour divides the extra frame time back out at those calls:
   // nothing changes at 30 fps, the consoles' rate.
   namespace HomingSteering
   {
      using Steer = void(__fastcall*)(void* projectile, void* edx, const float* target, const float* direction, float rate, float max_step, int scale);
      uint8_t* const STEER = reinterpret_cast<uint8_t*>(0x4A3030);
      // "push ebp; mov ebp, esp; and esp, 0FFFFFFF0h; sub esp, 98h"
      constexpr std::array<uint8_t, 12> PROLOGUE = {0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF0, 0x81, 0xEC, 0x98, 0x00, 0x00, 0x00};
      // FUN_004a1a90: the projectile's frame step, the value "scale" multiplies by
      using FrameStep = float(__thiscall*)(void* projectile);
      const FrameStep FRAME_STEP = reinterpret_cast<FrameStep>(0x4A1A90);
      // The return addresses of the calls whose max step already holds the frame step (FUN_004a3030 xrefs)
      constexpr std::array<uintptr_t, 18> PRESCALED_MAX_STEP_RETURNS = {0x4A4E35, 0x4A4E7A, 0x4A5553, 0x4A75E0, 0x4A80BE, 0x4A8B95, 0x4A9CE9, 0x4A9FDE, 0x4AAF4B,
         0x4AB1F8, 0x4AB482, 0x4AD4EB, 0x4AD549, 0x4AD8BD, 0x4B52DE, 0x4BA615, 0x4BAA64, 0x4BB225};
      // FUN_004a79d0's call, whose rate holds it too, capped at 0.8 after the multiply
      constexpr uintptr_t PRESCALED_RATE_RETURN = 0x4A80BE;
      constexpr float PRESCALED_RATE_CAP = 0.8f;
      // FUN_004bb2e0's re-aim at the next target after a ricochet: rate 1, max pi, scaled (a snap at time scale 1)
      constexpr uintptr_t RICOCHET_RETURN = 0x4BBA7E;
      // FUN_004aa150's (em0023 bullet) one-shot deflection: a snap (rate 1, unscaled) when the frame step is below 0.5, meant for AR slow
      // motion (time scale below 0.5), otherwise rate 0.3, scaled; max 60 degrees either way. Above 60 fps it snapped outside slow motion.
      constexpr uintptr_t DEFLECTION_RETURN = 0x4AA694;
      constexpr float DEFLECTION_RATE = 0.3f;   // 0xD7B3FC
      constexpr float SLOW_MOTION_SCALE = 0.5f; // 0xD7A5F8

      static MinHook::HookedFunction<Steer> hooked;

      static void __fastcall Detour(void* projectile, void* edx, const float* target, const float* direction, float rate, float max_step, int scale)
      {
         const uintptr_t caller = reinterpret_cast<uintptr_t>(_ReturnAddress());
         const float frame_time = *FRAME_TIME;
         if (!enabled || !(frame_time > 0.f))
         {
            hooked.original(projectile, edx, target, direction, rate, max_step, scale);
            return;
         }
         if (std::ranges::find(PRESCALED_MAX_STEP_RETURNS, caller) != PRESCALED_MAX_STEP_RETURNS.end())
         {
            Count(Stat::HOMING_PRESCALED);
            max_step /= frame_time;
            if (caller == PRESCALED_RATE_RETURN)
            {
               rate = (std::min)(rate / frame_time, PRESCALED_RATE_CAP);
            }
         }
         else if (caller == RICOCHET_RETURN)
         {
            Count(Stat::HOMING_RICOCHET);
            rate /= frame_time;
            max_step /= frame_time;
         }
         else if (caller == DEFLECTION_RETURN)
         {
            Count(Stat::HOMING_DEFLECTION);
            // The branch decided by the time scale, not the frame step
            const bool slow_motion = FRAME_STEP(projectile) < SLOW_MOTION_SCALE * frame_time;
            rate = slow_motion ? 1.f : (DEFLECTION_RATE / frame_time);
            max_step = slow_motion ? max_step : (max_step / frame_time);
            scale = slow_motion ? 0 : 1;
         }
         hooked.original(projectile, edx, target, direction, rate, max_step, scale);
      }

      static bool Install()
      {
         return AttachVerified(&hooked, STEER, PROLOGUE, &Detour);
      }
   } // namespace HomingSteering

   // Some enemies turned or righted themselves faster the higher the frame rate (twice as fast at 60 fps). FUN_00b9dec0 (out matrix,
   // current axis, target axis, rotation axis, rate, max angle) builds the rotation by min(angle * rate, max angle); these callers
   // pass constants every frame and accumulate the result into the object's matrix, so the turn per second grew with the frame rate.
   // The detour applies them over the frame time instead (as at 30 fps, the consoles' rate; also in AR slow motion, where they
   // ignored the time scale): rate 1 - (1 - rate)^frame time, max angle * frame time. Snaps (rate 1) are left alone.
   namespace UnscaledTurns
   {
      using Rotate = float*(__cdecl*)(float* out, const float* current, const float* target, const float* axis, float rate, float max_angle);
      uint8_t* const ROTATE = reinterpret_cast<uint8_t*>(0xB9DEC0);
      // "push ebp; mov ebp, esp; and esp, 0FFFFFFF0h; mov ecx, [ebp+0Ch]"
      constexpr std::array<uint8_t, 9> PROLOGUE = {0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF0, 0x8B, 0x4D, 0x0C};
      // The return addresses of the per-frame calls with constant arguments: em0092 ground alignment (FUN_007e99e0, FUN_0080ef80,
      // ~0x80f3b0; 0.2, 5 degrees), em0130 ground alignment (FUN_00872bb0; 0.3, 10), em00f1 (FUN_0094e030; 0.8, 10), object 0x3000c
      // turning to the player (FUN_00578980; 0.3, 30), em0090-92 self-righting (FUN_007ae730; 0.1, pi), the character collision up axis (FUN_005e9410 from FUN_00605210,
      // "TtV_CHAR_COLLISION"; 0.3, 10).
      constexpr std::array<uintptr_t, 8> UNSCALED_RETURNS = {0x7E9C4A, 0x80F35E, 0x80FB3E, 0x872CAE, 0x94EBE8, 0x578E7D, 0x7AE81E, 0x5E965F};

      static MinHook::HookedFunction<Rotate> hooked;

      static float* __cdecl Detour(float* out, const float* current, const float* target, const float* axis, float rate, float max_angle)
      {
         const float frame_time = *FRAME_TIME;
         if (enabled && frame_time > 0.f && rate < 1.f &&
             std::ranges::find(UNSCALED_RETURNS, reinterpret_cast<uintptr_t>(_ReturnAddress())) != UNSCALED_RETURNS.end())
         {
            Count(Stat::UNSCALED_TURN);
            rate = PerConsoleFrame(rate, frame_time);
            max_angle *= frame_time;
         }
         return hooked.original(out, current, target, axis, rate, max_angle);
      }

      static bool Install()
      {
         return AttachVerified(&hooked, ROTATE, PROLOGUE, &Detour);
      }
   } // namespace UnscaledTurns

   // The player's damage update, FUN_00553bc0 (once a frame), counted some windows in frames, so above 30 fps they were shorter:
   // the "can't die" window after some hits (HP held at 5), and three short cooldowns. On frames between the 30 Hz ticks the detour
   // keeps their decrement from happening, so they count 1/30 s as on the consoles. Below 30 fps every frame ticks, as before.
   namespace PlayerFrameCounters
   {
      using Update = void(__fastcall*)(uint8_t* player, void* edx);
      uint8_t* const UPDATE = reinterpret_cast<uint8_t*>(0x553BC0);
      // "push ebp; mov ebp, esp; and esp, 0FFFFFFF0h; sub esp, 0B4h"
      constexpr std::array<uint8_t, 12> PROLOGUE = {0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF0, 0x81, 0xEC, 0xB4, 0x00, 0x00, 0x00};
      // Decremented when non zero at the top of the update (0x553BD0, 0x553BE5)
      constexpr size_t NO_DEATH_FRAMES = 0xEAD4;       // uint16_t: 11 from FUN_004ce500; HP <= 0 is set back to 5 while non zero (0x55536C)
      constexpr size_t TYPE_8F_HIT_COOLDOWN = 0x13714; // int32_t: 4 after a type 0x8F hit with flag 0x20 (0x554EEB)
      // Decremented further down, on some paths only (0x554A49, 0x554802)
      constexpr size_t HIT_COOLDOWN = 0x10240;     // int32_t: 10 on a hit (0x554D03)
      constexpr size_t FLAGGED_COOLDOWN = 0x12AD4; // uint16_t: 10 (0x554563), counted while the byte at 0x12AD6 is set

      static MinHook::HookedFunction<Update> hooked;
      static std::atomic<bool> is_tick_frame = true;
      static float tick_phase = 0.f; // In 1/30 s

      // After the frame time update, for this frame: a tick every 1/30 s, on the nearest frame
      static void OnFrame()
      {
         const float frame_time = *FRAME_TIME;
         if (!(frame_time > 0.f && frame_time < 1.f))
         {
            tick_phase = 0.f;
            is_tick_frame = true;
            return;
         }
         tick_phase += frame_time;
         const bool tick = tick_phase + frame_time * 0.5f >= 1.f;
         if (tick)
         {
            tick_phase = (std::max)(tick_phase - 1.f, 0.f);
         }
         is_tick_frame = tick;
      }

      static void __fastcall Detour(uint8_t* player, void* edx)
      {
         if (!enabled || is_tick_frame)
         {
            if (enabled)
            {
               Count(Stat::COUNTER_TICK);
            }
            hooked.original(player, edx);
            return;
         }
         Count(Stat::COUNTER_HOLD);
         auto* const no_death_frames = reinterpret_cast<uint16_t*>(player + NO_DEATH_FRAMES);
         auto* const type_8f_hit_cooldown = reinterpret_cast<int32_t*>(player + TYPE_8F_HIT_COOLDOWN);
         auto* const hit_cooldown = reinterpret_cast<int32_t*>(player + HIT_COOLDOWN);
         auto* const flagged_cooldown = reinterpret_cast<uint16_t*>(player + FLAGGED_COOLDOWN);
         // The top decrements are unconditional: pre-increment, so the update reads the held value
         if (*no_death_frames != 0)
         {
            ++*no_death_frames;
         }
         if (*type_8f_hit_cooldown != 0)
         {
            ++*type_8f_hit_cooldown;
         }
         // The others are undone after the update if they ran (a reset to 10 then a decrement is held at 10 too)
         const int32_t hit_cooldown_before = *hit_cooldown;
         const uint16_t flagged_cooldown_before = *flagged_cooldown;
         hooked.original(player, edx);
         if (hit_cooldown_before != 0 && *hit_cooldown == hit_cooldown_before - 1)
         {
            *hit_cooldown = hit_cooldown_before;
         }
         if (flagged_cooldown_before != 0 && *flagged_cooldown == flagged_cooldown_before - 1)
         {
            *flagged_cooldown = flagged_cooldown_before;
         }
      }

      static bool Install()
      {
         return AttachVerified(&hooked, UPDATE, PROLOGUE, &Detour);
      }
   } // namespace PlayerFrameCounters

   // Beams and other continuous attacks did less damage and stunned longer the higher the frame rate.
   // FUN_00607aa0 gathers one hit record per touching attack body each frame; for 15 continuous attack types (BigLaser 0xC4, HomingLaser
   // 0xC5, ...) it scaled the damage by DAT_00f0318c (the frame time in 1/30 s). The receivers apply one record a frame behind time based
   // gates, so above 30 fps the hit that landed was the first frame's, already scaled down, and the player's flinch counter, which counts
   // hit frames, ran out sooner. The scaling block is replaced by a call that lets each receiver take these records at full damage at most
   // 30 times a second, as on the consoles; below 30 fps the game's own scaling stays.
   namespace ContinuousDamage
   {
      // The scaling block, up to the record append at 0x608852: "movss xmm1, [0xF0318C]" ... "mov [esp+0C0h], 1"
      uint8_t* const SCALE_BLOCK = reinterpret_cast<uint8_t*>(0x60881F);
      constexpr std::array<uint8_t, 51> ORIGINAL = {0xF3, 0x0F, 0x10, 0x0D, 0x8C, 0x31, 0xF0, 0x00, 0x0F, 0x57, 0xC0, 0xF3, 0x0F, 0x2A, 0xC1, 0x0F, 0x5A,
         0xC0, 0x0F, 0x5A, 0xC9, 0xF2, 0x0F, 0x59, 0xC1, 0xF2, 0x0F, 0x2C, 0xC0, 0x89, 0x84, 0x24, 0xC0, 0x00, 0x00, 0x00, 0x85, 0xC0, 0x7F, 0x0B, 0xC7, 0x84,
         0x24, 0xC0, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00};
      constexpr size_t CALL_OFFSET = 6; // The rel32 of "call TakeDamage"

      struct Receivers
      {
         std::shared_mutex mutex;
         uint64_t frame = 0;
         double time = 0.0; // The sum of the frame times, in 1/30 s
         struct Hits
         {
            uint64_t frame;
            double next_time;
         };
         std::unordered_map<const void*, Hits> hits; // By the receiver's collision object
      };
      static Receivers receivers;
      static bool installed = false;

      // The damage of a continuous hit record, 0 to drop it (ecx = the attack's damage, [esp+5Ch] = FUN_00607aa0's this)
      static int __stdcall TakeDamage(const void* receiver, int damage)
      {
         const float frame_time = *FRAME_TIME;
         if (!enabled || !(frame_time > 0.f && frame_time < 1.f))
         {
            return (std::max)(int(double(float(damage)) * double(frame_time)), 1);
         }
         const std::unique_lock lock(receivers.mutex);
         const auto [it, is_new] = receivers.hits.try_emplace(receiver, Receivers::Hits{.frame = receivers.frame, .next_time = receivers.time + 1.0});
         auto& [hit_receiver, hits] = *it;
         // Every record of the frame of a hit lands (several bodies can touch at once); the next hit is due 1/30 s later, on the nearest frame
         if (!is_new && hits.frame != receivers.frame)
         {
            if (receivers.time + frame_time * 0.5 < hits.next_time)
            {
               Count(Stat::DAMAGE_DROPPED);
               return 0;
            }
            hits.next_time = (receivers.time - hits.next_time < 1.0) ? (hits.next_time + 1.0) : (receivers.time + 1.0);
            hits.frame = receivers.frame;
         }
         Count(Stat::DAMAGE_LANDED);
         return (std::max)(damage, 1);
      }

      static void OnFrame()
      {
         if (!installed)
            return;
         const std::unique_lock lock(receivers.mutex);
         receivers.frame++;
         receivers.time += *FRAME_TIME;
         if (receivers.hits.size() > 256)
         {
            std::erase_if(receivers.hits, [](const auto& entry)
               {
                  const auto& [hit_receiver, hits] = entry;
                  return hits.next_time < receivers.time; });
         }
      }

      static bool Install()
      {
         if (std::memcmp(SCALE_BLOCK, ORIGINAL.data(), ORIGINAL.size()) != 0)
            return false;
         // push ecx; push [esp+60h]; call TakeDamage; test eax, eax; jz drop
         // mov [esp+0C0h], eax; jmp 0x608852 (append the record)
         // drop: mov edi, [esp+1Ch]; mov esi, [esp+24h] (as 0x60886E after an append); jmp 0x60887E (the next contact); nop padding
         std::array<uint8_t, ORIGINAL.size()> patched = {0x51, 0xFF, 0x74, 0x24, 0x60, 0xE8, 0x00, 0x00, 0x00, 0x00, 0x85, 0xC0, 0x74, 0x09, 0x89, 0x84, 0x24,
            0xC0, 0x00, 0x00, 0x00, 0xEB, 0x1C, 0x8B, 0x7C, 0x24, 0x1C, 0x8B, 0x74, 0x24, 0x24, 0xEB, 0x3E};
         std::fill(patched.begin() + 33, patched.end(), uint8_t(0x90));
         WriteRel32(patched.data(), SCALE_BLOCK, CALL_OFFSET, reinterpret_cast<uintptr_t>(&TakeDamage));
         installed = System::PatchMemory(SCALE_BLOCK, patched.data(), patched.size(), System::PatchMemoryType::Code);
         return installed;
      }

      static void Uninstall()
      {
         if (installed && System::PatchMemory(SCALE_BLOCK, ORIGINAL.data(), ORIGINAL.size(), System::PatchMemoryType::Code))
         {
            installed = false;
         }
      }
   } // namespace ContinuousDamage

   // Below 30 fps the game ran too fast (1.5x at 20 fps). The consoles' slowdown, FUN_009e3870, sets DAT_00ef5c88 to the last frame's
   // time in 1/30 s clamped to [1, 2] and multiplies it into the time scales (DAT_01136170..011361ac) that the PC port's frame step
   // (time scale * DAT_00f0318c) multiplies by the frame time again. Its gate "test eax, edx; je" becomes "xor eax, eax; je": the path the
   // game takes when it turns the slowdown off itself (DAT_00ef5c80 = 0), DAT_00ef5c88 staying 1. Patched each frame, with the
   // game running: the instruction is 2 bytes, 2 byte aligned.
   namespace ConsoleSlowdown
   {
      uint8_t* const GATE = reinterpret_cast<uint8_t*>(0x9E3A2A);
      constexpr std::array<uint8_t, 2> ORIGINAL = {0x85, 0xD0}; // test eax, edx (followed by "je 0x9E3BF8": 0F 84 C6 01 00 00)
      constexpr std::array<uint8_t, 2> PATCHED = {0x31, 0xC0};  // xor eax, eax
      constexpr std::array<uint8_t, 6> JUMP = {0x0F, 0x84, 0xC6, 0x01, 0x00, 0x00};

      static bool patchable = false;
      static bool patched = false;

      static bool Install()
      {
         patchable = std::memcmp(GATE, ORIGINAL.data(), ORIGINAL.size()) == 0 && std::memcmp(GATE + ORIGINAL.size(), JUMP.data(), JUMP.size()) == 0;
         return patchable;
      }

      static void Apply(bool patch)
      {
         if (!patchable || patch == patched)
            return;
         const auto& bytes = (patch ? PATCHED : ORIGINAL);
         if (System::PatchMemory(GATE, bytes.data(), bytes.size(), System::PatchMemoryType::Code))
         {
            patched = patch;
         }
      }
   } // namespace ConsoleSlowdown

   // Some enemies aimed faster the higher the frame rate. FUN_00b9b680 (current angle, target angle, rate, max step) returns the angle
   // moved by clamp(wrap(target - current) * rate, +-max step); these callers pass constants every frame. As in "UnscaledTurns", the
   // detour applies them over the frame time: rate 1 - (1 - rate)^frame time, max step * frame time.
   namespace UnscaledApproaches
   {
      using Approach = float(__cdecl*)(float current, float target, float rate, float max_step);
      uint8_t* const APPROACH = reinterpret_cast<uint8_t*>(0xB9B680);
      // "movss xmm1, [esp+4]; movss xmm0, [esp+8]"
      constexpr std::array<uint8_t, 12> PROLOGUE = {0xF3, 0x0F, 0x10, 0x4C, 0x24, 0x04, 0xF3, 0x0F, 0x10, 0x44, 0x24, 0x08};
      // The return addresses of the per-frame calls with constant arguments: em00f0's sweep (FUN_0094c6c0 sub-states 1 and 2, yaw and
      // pitch; 0.3 and 0.15, 1.5 degrees), the yaw clamp of FUN_008dd680's move (rate 1, 6 degrees), and the angles +0x7050/+0x7054
      // eased to their targets (0.95, 4.5 to 9 degrees) in FUN_007d7650, FUN_008d8e00 and FUN_008e8e00 (next to step scaled code).
      constexpr std::array<uintptr_t, 11> UNSCALED_RETURNS = {0x94C9AB, 0x94CAAF, 0x94CAF2, 0x8DD8D9, 0x7D771A, 0x7D7758, 0x7D7795, 0x8D8EC2, 0x8D8EFF,
         0x8E8EC1, 0x8E8EFE};
      // The calls whose rate and max step the caller already multiplied by the frame step (linear, as in "LinearTurnRate"): the enemy yaw
      // +0x84 in FUN_0097fdb0 (0.6 or 0.3). The rate / frame time is the console per frame rate (rate * time scale), so it becomes
      // 1 - (1 - rate / frame time)^frame time; frame times from 1 up stay the game's.
      constexpr std::array<uintptr_t, 3> LINEAR_RETURNS = {0x98002A, 0x9800B9, 0x980270};
      // The calls whose result minus the current angle the caller then adds times the frame step (0x8A7610, 0x8A76D0, 0x8A2820,
      // 0x8A28F0): the aim yaw/pitch +0x6F0C/+0x6F10 and +0x6D84/+0x6D88 (0.95, 3 degrees), edi = the object. The rate becomes the
      // console's per frame rate (rate * time scale) applied over the frame time, divided by the step multiplied in afterwards.
      constexpr std::array<uintptr_t, 6> STEP_SCALED_RETURNS = {0x7D907B, 0x7D90B6, 0x7D95A4, 0x8DC088, 0x8DC0C3, 0x8DC5A5};

      static MinHook::HookedFunction<Approach> hooked;
      // ponytail: one global for the game thread's calls; a per-thread copy if FUN_00b9b680 ever runs on several threads at once
      static uintptr_t caller_edi = 0;

      static float __cdecl Detour(float current, float target, float rate, float max_step)
      {
         const float frame_time = *FRAME_TIME;
         if (!enabled || !(frame_time > 0.f))
            return hooked.original(current, target, rate, max_step);
         const auto return_address = reinterpret_cast<uintptr_t>(_ReturnAddress());
         if (std::ranges::find(UNSCALED_RETURNS, return_address) != UNSCALED_RETURNS.end())
         {
            Count(Stat::APPROACH_UNSCALED);
            if (rate < 1.f)
            {
               rate = PerConsoleFrame(rate, frame_time);
            }
            max_step *= frame_time;
         }
         else if (frame_time < 1.f && std::ranges::find(LINEAR_RETURNS, return_address) != LINEAR_RETURNS.end())
         {
            Count(Stat::APPROACH_LINEAR);
            rate = PerConsoleFrame((std::min)(rate / frame_time, 1.f), frame_time);
         }
         else if (frame_time < 1.f && std::ranges::find(STEP_SCALED_RETURNS, return_address) != STEP_SCALED_RETURNS.end())
         {
            Count(Stat::APPROACH_STEP_SCALED);
            const float time_scale = *reinterpret_cast<const float*>(caller_edi + TIME_SCALE);
            if (time_scale > 0.f)
            {
               rate = PerConsoleFrame((std::min)(rate * time_scale, 1.f), frame_time) / (time_scale * frame_time);
            }
         }
         return hooked.original(current, target, rate, max_step);
      }

      // The hook's entry: edi is callee saved, so it still holds the caller's
      static __declspec(naked) void Entry()
      {
         __asm
         {
         mov caller_edi, edi
         jmp Detour
         }
      }

      static bool Install()
      {
         return AttachVerified(&hooked, APPROACH, PROLOGUE, reinterpret_cast<Approach>(&Entry));
      }
   } // namespace UnscaledApproaches

   // Enemies turned slower the higher the frame rate when the rate, not the max step, limited the turn. FUN_006285d0 (this = enemy, rate,
   // max step; 292 callers) eases the yaw +0x84 to +0x3D4 through FUN_00b9b680 with the rate and max step multiplied by the enemy's frame
   // step (time scale +0x440 * frame time): linear, so a 0.95 rate turned 95% of the way per 1/30 s at 30 fps and 72% at 60.
   // The detour turns 1 - (1 - rate * time scale)^frame time instead, per 1/30 s what the consoles turned per frame. The snap (rate >= 1,
   // max step >= pi) and frame times from 1 up stay the game's.
   namespace LinearTurnRate
   {
      using Turn = void(__fastcall*)(uint8_t* enemy, void* edx, float rate, float max_step);
      uint8_t* const TURN = reinterpret_cast<uint8_t*>(0x6285D0);
      // "movss xmm2, [esp+4]; comiss xmm2, [0xD7A458]"
      constexpr std::array<uint8_t, 13> PROLOGUE = {0xF3, 0x0F, 0x10, 0x54, 0x24, 0x04, 0x0F, 0x2F, 0x15, 0x58, 0xA4, 0xD7, 0x00};
      constexpr size_t YAW = 0x84;
      constexpr size_t TARGET_YAW = 0x3D4;
      constexpr double SNAP_MAX_STEP = 3.1415927410125732; // 0xD78CD0

      static MinHook::HookedFunction<Turn> hooked;

      static void __fastcall Detour(uint8_t* enemy, void* edx, float rate, float max_step)
      {
         const float frame_time = *FRAME_TIME;
         if (!enabled || !(frame_time > 0.f && frame_time < 1.f) || (rate >= 1.f && double(max_step) >= SNAP_MAX_STEP))
         {
            hooked.original(enemy, edx, rate, max_step);
            return;
         }
         const float time_scale = *reinterpret_cast<const float*>(enemy + TIME_SCALE);
         Count(Stat::ENEMY_TURN);
         const float console_rate = (std::min)(rate * time_scale, 1.f);
         auto* const yaw = reinterpret_cast<float*>(enemy + YAW);
         *yaw = UnscaledApproaches::hooked.original(*yaw, *reinterpret_cast<const float*>(enemy + TARGET_YAW), PerConsoleFrame(console_rate, frame_time),
            max_step * time_scale * frame_time);
      }

      static bool Install()
      {
         // Calls FUN_00b9b680 through its trampoline
         return UnscaledApproaches::hooked.target && AttachVerified(&hooked, TURN, PROLOGUE, &Detour);
      }
   } // namespace LinearTurnRate

   // Players and enemies (large actors most) turned faster the higher the frame rate. FUN_006287f0 (out, current, target angle, rate, snap
   // angle, max step; 132 callers) snaps within the snap angle, else moves by FUN_00b9b680 but at least by the snap angle. Per call, so
   // per frame: the rate, the snap angle and the max step become per 1/30 s as in "UnscaledApproaches", by return address. The linear
   // callers multiplied only the rate by the frame step (rate / frame time = rate per console frame); they keep the game's from 1 up.
   // The callers passing rate k^step get 1 - (1 - k)^step. Not here: the snaps (rate 1, max step pi) and step scaled arguments.
   // The player auto-turn (FUN_004de2f0, 3 degrees per call while its 10 frame timer +0xE9F4 counts down) is only fixed together
   // with that timer in "FrameConstants".
   namespace SnapApproaches
   {
      using Approach = int(__fastcall*)(void* ecx, void* edx, float* out, float current, float target, float rate, float snap, float max_step);
      uint8_t* const APPROACH = reinterpret_cast<uint8_t*>(0x6287F0);
      // "movss xmm0, [esp+0Ch]; movss xmm1, [esp+8]"
      constexpr std::array<uint8_t, 12> PROLOGUE = {0xF3, 0x0F, 0x10, 0x44, 0x24, 0x0C, 0xF3, 0x0F, 0x10, 0x4C, 0x24, 0x08};
      // Player states, rate frame time (or step) * 0.1 to 0.6, snap 0.01 to 10 degrees, max step 3 to 30 degrees
      constexpr std::array<uintptr_t, 28> LINEAR_RETURNS = {0x4CE6D3, 0x4DE8A5, 0x4DE94C, 0x4DF550, 0x4DF866, 0x4E0FC0, 0x4E12D6, 0x4E1F3D, 0x4E2253,
         0x4E2C3B, 0x4E60A2, 0x4E623B, 0x4FFAB4, 0x500645, 0x500F75, 0x502065, 0x5033A6, 0x5042E8, 0x504B4D, 0x5062A5, 0x506BD5, 0x507C07, 0x507D81,
         0x508E67, 0x508FBA, 0x50ABC5, 0x50B4E5, 0x53FA43};
      // Rate 0.1 to 0.5 (or 1), snap 0.01 to 3 degrees, max step 0.1 to 30 degrees (or the large actors' +0x21FB4/+0x21FB8 turn speeds)
      constexpr std::array<uintptr_t, 88> UNSCALED_RETURNS = {0x535AFC, 0x535B5A, 0x53DA1B, 0x53DA7A, 0x53DBBB, 0x53DD72, 0x53EC41, 0x53ECA0, 0x553720,
         0x553773, 0x55D04F, 0x55D09F, 0x573BB6, 0x5826A2, 0x58277B, 0x5A28A6, 0x5A2987, 0x5A34DA, 0x5A3606, 0x5A9470, 0x5A9650, 0x5A97D9, 0x5A9829,
         0x5A9934, 0x5A9984, 0x5A9B0B, 0x5A9B5B, 0x5A9F93, 0x5A9FE3, 0x686493, 0x686570, 0x687E10, 0x687E5D, 0x68C096, 0x68C0DC, 0x68C135, 0x68C438,
         0x68C49B, 0x68C4E1, 0x68C539, 0x68CC20, 0x692939, 0x692A1A, 0x692C09, 0x692CEA, 0x692F4C, 0x69302D, 0x693200, 0x6932E6, 0x693388, 0x695B1F,
         0x695BF2, 0x696EFD, 0x696F53, 0x696FA9, 0x6971DA, 0x6972C5, 0x69732D, 0x697522, 0x697589, 0x6976D0, 0x697A68, 0x6A151A, 0x6A1567, 0x6A2148,
         0x6A2195, 0x6A46CC, 0x6A471D, 0x6A4812, 0x6AB4B9, 0x6AB5A4, 0x6AB607, 0x6B01B1, 0x6C03C4, 0x6C0411, 0x7AEB91, 0x896CD2, 0x896E8E, 0x8FC3BC,
         0x8FC49D, 0x8FC78B, 0x8FC8A1, 0x9071AA, 0x907289, 0x9266EF, 0x9267CE, 0x926B0F, 0x926BF0};
      // The rate passed as k^step (k 0.9 and 0.45; FUN_0051d5a0): the per-call share k as a share kept, so more per call the higher the
      // frame rate. Snap 0.1 degrees, max step 45 and 90 degrees
      constexpr std::array<uintptr_t, 2> POW_RATE_RETURNS = {0x51DDA0, 0x51DEAC};
      // The large actors' (FUN_00695620) k^step rates (k 0.1 and 0.3), with the snap and max step multiplied by the step twice
      constexpr std::array<uintptr_t, 6> POW_RATE_SQUARED_RETURNS = {0x696154, 0x696307, 0x6963E0, 0x6964C3, 0x696797, 0x69686A};
      static_assert(std::ranges::is_sorted(LINEAR_RETURNS) && std::ranges::is_sorted(UNSCALED_RETURNS)); // binary searched

      static MinHook::HookedFunction<Approach> hooked;

      static int __fastcall Detour(void* ecx, void* edx, float* out, float current, float target, float rate, float snap, float max_step)
      {
         const float frame_time = *FRAME_TIME;
         if (!enabled || !(frame_time > 0.f))
            return hooked.original(ecx, edx, out, current, target, rate, snap, max_step);
         const auto return_address = reinterpret_cast<uintptr_t>(_ReturnAddress());
         if (std::ranges::binary_search(UNSCALED_RETURNS, return_address))
         {
            Count(Stat::SNAP_UNSCALED);
            if (rate < 1.f)
            {
               rate = PerConsoleFrame(rate, frame_time);
            }
            snap *= frame_time;
            max_step *= frame_time;
         }
         else if (frame_time < 1.f && std::ranges::binary_search(LINEAR_RETURNS, return_address))
         {
            Count(Stat::SNAP_LINEAR);
            rate = PerConsoleFrame((std::min)(rate / frame_time, 1.f), frame_time);
            snap *= frame_time;
            max_step *= frame_time;
         }
         else
         {
            const bool squared = std::ranges::find(POW_RATE_SQUARED_RETURNS, return_address) != POW_RATE_SQUARED_RETURNS.end();
            if (squared || std::ranges::find(POW_RATE_RETURNS, return_address) != POW_RATE_RETURNS.end())
            {
               // These callers pass the object in ecx
               const float step = *reinterpret_cast<const float*>(static_cast<uint8_t*>(ecx) + TIME_SCALE) * frame_time;
               if (step > 0.f && rate > 0.f)
               {
                  Count(Stat::SNAP_POW);
                  const float k = std::pow(rate, 1.f / step);
                  rate = PerConsoleFrame(k, step);
                  const float extent_scale = squared ? (1.f / step) : step;
                  snap *= extent_scale;
                  max_step *= extent_scale;
               }
            }
         }
         return hooked.original(ecx, edx, out, current, target, rate, snap, max_step);
      }

      static bool Install()
      {
         return AttachVerified(&hooked, APPROACH, PROLOGUE, &Detour);
      }
   } // namespace SnapApproaches

   // FUN_00628970 (out, current, target 4D vectors, rate, snap distance, max step; 15 callers) is "SnapApproaches"' function for vectors:
   // the player states' and a projectile's (slot 41 of the table 0xE89890) eased positions moved faster the higher the frame rate. Same
   // fixes by return address. The step linear callers already scale the snap distance and max step by the step, so only their rate
   // changes. Not here: the rate 1 callers 0x5001FF, 0x505E84 and 0x50A79E (extents step scaled).
   namespace VectorApproaches
   {
      using Approach = int(__fastcall*)(void* ecx, void* edx, float* out, const float* current, const float* target, float rate, float snap, float max_step);
      uint8_t* const APPROACH = reinterpret_cast<uint8_t*>(0x628970);
      // "push ebp; mov ebp, esp; and esp, -10h; sub esp, 30h; mov ecx, [ebp+0Ch]"
      constexpr std::array<uint8_t, 12> PROLOGUE = {0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF0, 0x83, 0xEC, 0x30, 0x8B, 0x4D, 0x0C};
      // Rate 0.1 to 0.5, snap 0.01 to 0.1, max step 0.5 or the projectile's +0x8
      constexpr std::array<uintptr_t, 6> UNSCALED_RETURNS = {0x4C7237, 0x4DEFFB, 0x4E03EF, 0x4E0BCB, 0x4E1B45, 0x6B039E};
      // Player states: rate step * 0.6, snap step * 0.05, max step step * 0.15
      constexpr std::array<uintptr_t, 6> LINEAR_RETURNS = {0x4DF4D2, 0x4DF7E8, 0x4E0F42, 0x4E1258, 0x4E1EBF, 0x4E21D5};

      static MinHook::HookedFunction<Approach> hooked;

      static int __fastcall Detour(void* ecx, void* edx, float* out, const float* current, const float* target, float rate, float snap, float max_step)
      {
         const float frame_time = *FRAME_TIME;
         if (!enabled || !(frame_time > 0.f))
            return hooked.original(ecx, edx, out, current, target, rate, snap, max_step);
         const auto return_address = reinterpret_cast<uintptr_t>(_ReturnAddress());
         if (std::ranges::find(UNSCALED_RETURNS, return_address) != UNSCALED_RETURNS.end())
         {
            Count(Stat::VECTOR_UNSCALED);
            rate = PerConsoleFrame(rate, frame_time);
            snap *= frame_time;
            max_step *= frame_time;
         }
         else if (std::ranges::find(LINEAR_RETURNS, return_address) != LINEAR_RETURNS.end())
         {
            // rate / frame time = the console per frame rate (rate * time scale), as in "SnapApproaches"' linear callers
            if (frame_time < 1.f)
            {
               Count(Stat::VECTOR_LINEAR);
               rate = PerConsoleFrame((std::min)(rate / frame_time, 1.f), frame_time);
            }
         }
         return hooked.original(ecx, edx, out, current, target, rate, snap, max_step);
      }

      static bool Install()
      {
         return AttachVerified(&hooked, APPROACH, PROLOGUE, &Detour);
      }
   } // namespace VectorApproaches

   // Some per-frame updates kept their console constants, so they ran faster the higher the frame rate (twice at 60 fps): frame counters
   // (object 0x3000c's 8 s lifetime, enemy waits and give-up counters), the projectiles' gravity, speed decay and speed-up, and the
   // player's slide and boost-stop slowdown. Each listed "op xmm, qword [constant]" (F2 0F op ModRM, then the address) reads a value set
   // each frame from the frame time in 1/30 s instead (the constant with the fix off): what the consoles applied per frame, applied
   // per 1/30 s. The frame time, not the time scale: these ignored it on the consoles too.
   namespace FrameConstants
   {
      enum class Scaling
      {
         PER_FRAME,   // c * frame time: "-= 1" timers, gravity
         DECAY,       // c^frame time: "v *= c"
         DECAY_INPUT, // The "a" of "v = (v + a) * k": the same steady state and approach per 1/30 s (see "OnFrame")
         EMA_INPUT,   // The "a" of "v = v * k + x * a": a * (1 - k^frame time) / (1 - k), its "k" a DECAY site
      };
      struct Site
      {
         uintptr_t instruction;
         Scaling scaling;
         double k = 0.0; // DECAY_INPUT's and EMA_INPUT's decay
      };
      constexpr auto SITES = std::to_array<Site>({
         // Frame counters of 1.0 (0xD78CF8). Object 0x3000c's lifetime +0x2B38 (FUN_00578980, 240 frames, then it is freed)
         {0x5791EC, Scaling::PER_FRAME},
         // The player's auto-turn timer +0xE9F4 (FUN_004de2f0, 10 frames of 3 degrees; its turns are in "SnapApproaches")
         {0x4DE8D7, Scaling::PER_FRAME},
         // em00f0: the waits +0x6D88 of FUN_0086fc50 (60) and FUN_0094c6c0 (30, 180)
         {0x86FCDE, Scaling::PER_FRAME},
         {0x94C948, Scaling::PER_FRAME},
         {0x94CA1A, Scaling::PER_FRAME},
         // +0x2B74 counting to +0x2B70: FUN_00588480's wait before an attack burst (15-30), FUN_00593bd0 (60-150) and FUN_00593da0 (45-60)
         {0x58851D, Scaling::PER_FRAME},
         {0x593C92, Scaling::PER_FRAME},
         {0x593E4B, Scaling::PER_FRAME},
         // em0020 (FUN_006c0220) drops what it holds after 60 frames without line of sight (+0x22098)
         {0x6C071A, Scaling::PER_FRAME},
         // FUN_00929480's self-destruct after 60 frames of its attached object's axis below -8 (+0x6D08)
         {0x929750, Scaling::PER_FRAME},
         // Projectiles (bullet update table 0xE89890, which also holds the homing missiles; speed +0x2A4, moved by speed * step):
         // the drop velocity +0x254 of slots 14 and 15 (FUN_004a8ea0, FUN_004a9740), gravity 0.009 and 0.0015 then * 0.92
         {0x4A91DF, Scaling::PER_FRAME},
         {0x4A9AE8, Scaling::PER_FRAME},
         {0x4A93FF, Scaling::DECAY},
         {0x4A9D8E, Scaling::DECAY},
         // Slots 21 and 23 (FUN_004abe90, FUN_004ace20): flight * 0.98, speed-up (v + 0.05 [+ 0.15 for type 0x6C]) * 0.94
         {0x4AC663, Scaling::DECAY},
         {0x4ACA47, Scaling::DECAY_INPUT, 0.94},
         {0x4ACA61, Scaling::DECAY_INPUT, 0.94},
         {0x4ACA84, Scaling::DECAY},
         {0x4AD440, Scaling::DECAY},
         {0x4AD827, Scaling::DECAY_INPUT, 0.94},
         {0x4AD837, Scaling::DECAY},
         // Slot 22 (FUN_004c86c0): flight * 0.96, speed-up (v + 0.1) * 0.96
         {0x4C8C87, Scaling::DECAY},
         {0x4C9214, Scaling::DECAY_INPUT, 0.96},
         {0x4C9224, Scaling::DECAY},
         // The player: move speed +0x7A48 * 0.99 (FUN_004e2640), slides +0x7A44 * 0.7 (FUN_00537600) and * 0.85 (FUN_00539380,
         // FUN_00539750, FUN_005398f0), the boost stop * 0.5, twice in a row on some frames (FUN_00511080, "movsd xmm1" for both)
         {0x4E359A, Scaling::DECAY},
         {0x538266, Scaling::DECAY},
         {0x5394D7, Scaling::DECAY},
         {0x5397EB, Scaling::DECAY},
         {0x539981, Scaling::DECAY},
         {0x512433, Scaling::DECAY},
         // Path speed factor EMAs "v * 0.9 + FUN_006914b0(path) * 0.1" of the path followers (FUN_007b1b30 +0x78D4, FUN_007d6770
         // +0x6EE8, FUN_00897080 +0x6E74, FUN_008b3650 +0x76F4, FUN_00980b40 +0x6E44) and the enemy locomotion (FUN_00802580 +0x81A8)
         {0x7B1BBD, Scaling::DECAY},
         {0x7B1BB2, Scaling::EMA_INPUT, 0.9},
         {0x7D67EA, Scaling::DECAY},
         {0x7D67DF, Scaling::EMA_INPUT, 0.9},
         {0x89710D, Scaling::DECAY},
         {0x897102, Scaling::EMA_INPUT, 0.9},
         {0x8B36DD, Scaling::DECAY},
         {0x8B36D2, Scaling::EMA_INPUT, 0.9},
         {0x980BE9, Scaling::DECAY},
         {0x980BF4, Scaling::EMA_INPUT, 0.9},
         {0x8025F5, Scaling::DECAY},
         {0x802600, Scaling::EMA_INPUT, 0.9},
         // FUN_008d4e50's trajectory offsets +0x6EF0/+0x6EF4: "t * 0.2 + o * 0.8" while its timer +0x6D04 runs (the 0.2 loaded once
         // for both), then "o * 0.95" (FUN_008d56a0's are in "BlockCalls")
         {0x8D5335, Scaling::EMA_INPUT, 0.8},
         {0x8D5362, Scaling::DECAY},
         {0x8D5390, Scaling::DECAY},
         {0x8D53AC, Scaling::DECAY},
         {0x8D53C7, Scaling::DECAY},
         // The large actors' turn speeds +0x21FB4/+0x21FB8 (the max steps of their "SnapApproaches" turns): "+= 0.03 degrees" up to
         // 1 degree while turning, else "*= 0.9" (FUN_00695620, FUN_00697350, FUN_006ab2c0)
         {0x697158, Scaling::PER_FRAME},
         {0x697194, Scaling::DECAY},
         {0x697254, Scaling::PER_FRAME},
         {0x6972D7, Scaling::DECAY},
         {0x6974B0, Scaling::PER_FRAME},
         {0x697533, Scaling::DECAY},
         {0x6AB436, Scaling::PER_FRAME},
         {0x6AB473, Scaling::DECAY},
         {0x6AB532, Scaling::PER_FRAME},
         {0x6AB5B1, Scaling::DECAY},
         // Camera (0xFDC430): collision ease, timer "-= 1" for 30 frames with "+= (full - d) * 0.15" (FUN_005bb3f0)
         {0x5BDA4E, Scaling::PER_FRAME},
         {0x5BDB05, Scaling::EMA_INPUT, 0.85},
         // Its recoil kick +0x2C58 "*= 0.8" (FUN_005b0740)
         {0x5B0C93, Scaling::DECAY},
         // Its free-look +0x2940/+0x2944 (FUN_005b8ba0): a stick rate per 1/30 s (the mouse too: FUN_00401610 divides it by the frame time)
         // added per frame, "*= 0.8" when released
         {0x5B8E6F, Scaling::PER_FRAME},
         {0x5B8EEC, Scaling::PER_FRAME},
         {0x5B902C, Scaling::DECAY},
         {0x5B904B, Scaling::DECAY},
         // Its aim zoom offset +0x2DC4 "*= 0.1" when not aiming (FUN_0054f780), eased in by 0.3 in the aim states 0x53D140, 0x53E600, 0x53F2A0
         {0x550618, Scaling::DECAY},
         {0x53D368, Scaling::EMA_INPUT, 0.7},
         {0x53E728, Scaling::EMA_INPUT, 0.7},
         {0x53F398, Scaling::EMA_INPUT, 0.7},
         // Player aim twist +0x719C (FUN_0054f780)
         {0x5506C8, Scaling::EMA_INPUT, 0.7},
         {0x550723, Scaling::DECAY},
         // Enemy look-at return "*= 0.7" (slot 14 pose updates)
         {0x7BCC35, Scaling::DECAY},
         {0x7C8E66, Scaling::DECAY},
         {0x87C26A, Scaling::DECAY},
         {0x8A9BB9, Scaling::DECAY},
         // Player state slot 238 (FUN_00520c40) slide velocity +0x7C00..+0x7C0C "*= 0.94" / "*= 0.7" (a movsd, four uses each)
         {0x520DF7, Scaling::DECAY},
         {0x520E99, Scaling::DECAY},
         // Effect keyframe age +0x474 "+= 1" (FUN_00488fe0; keys fire on prev < key <= age, so fractional ages are safe)
         {0x48933E, Scaling::PER_FRAME},
      });
      constexpr std::array<uint8_t, 4> OPCODES = {0x10, 0x58, 0x59, 0x5C}; // movsd, addsd, mulsd, subsd
      constexpr size_t OPCODE_OFFSET = 2;
      constexpr size_t MODRM_OFFSET = 3;
      constexpr size_t ADDRESS_OFFSET = 4;

      static std::array<uint32_t, SITES.size()> constant_addresses = {};
      static std::array<double, SITES.size()> values = {}; // Read by the game in place of the constants
      static bool installed = false;

      // Only after "Install" verified every site
      static void Restore()
      {
         for (size_t i = 0; i < SITES.size(); i++)
         {
            System::PatchMemory(reinterpret_cast<void*>(SITES[i].instruction + ADDRESS_OFFSET), &constant_addresses[i], sizeof(uint32_t), System::PatchMemoryType::Code);
         }
      }

      static void OnFrame()
      {
         if (!installed)
            return;
         const double frame_time = *FRAME_TIME;
         const bool fix = enabled && frame_time > 0.0;
         for (size_t i = 0; i < SITES.size(); i++)
         {
            const double constant = *reinterpret_cast<const double*>(uintptr_t(constant_addresses[i]));
            if (!fix)
            {
               values[i] = constant;
               continue;
            }
            switch (SITES[i].scaling)
            {
            case Scaling::PER_FRAME:
               values[i] = constant * frame_time;
               break;
            case Scaling::DECAY:
               values[i] = std::pow(constant, frame_time);
               break;
            case Scaling::DECAY_INPUT:
            {
               // v' = (v + a') * k^t, with a' such that the steady state a * k / (1 - k) is approached as v' - s = (v - s) * k^t
               const double k = SITES[i].k;
               const double decay = std::pow(k, frame_time);
               values[i] = constant * k * (1.0 - decay) / ((1.0 - k) * decay);
               break;
            }
            case Scaling::EMA_INPUT:
               values[i] = constant * EmaInputScale(SITES[i].k, frame_time);
               break;
            }
         }
      }

      static bool Install()
      {
         for (size_t i = 0; i < SITES.size(); i++)
         {
            const auto* const code = reinterpret_cast<const uint8_t*>(SITES[i].instruction);
            if (code[0] != 0xF2 || code[1] != 0x0F || std::ranges::find(OPCODES, code[OPCODE_OFFSET]) == OPCODES.end() || (code[MODRM_OFFSET] & 0xC7) != 0x05)
               return false;
            std::memcpy(&constant_addresses[i], code + ADDRESS_OFFSET, sizeof(uint32_t));
            values[i] = *reinterpret_cast<const double*>(uintptr_t(constant_addresses[i]));
         }
         installed = true;
         for (size_t i = 0; i < SITES.size() && installed; i++)
         {
            const auto value_address = uint32_t(reinterpret_cast<uintptr_t>(&values[i]));
            installed = System::PatchMemory(reinterpret_cast<void*>(SITES[i].instruction + ADDRESS_OFFSET), &value_address, sizeof(value_address), System::PatchMemoryType::Code);
         }
         if (!installed)
         {
            Restore();
         }
         return installed;
      }

      static void Uninstall()
      {
         if (installed)
         {
            Restore();
            installed = false;
         }
      }
   } // namespace FrameConstants

   // FUN_008d4e50 eases +0x6F48 to "(t - v) * 0.1" by 0.1 * frame time per frame and its only reader moves +0x6EF8 += v * 4 * dt, so
   // the path speed fell with the frame rate (-24% at 60 fps). Its two frame time loads (0x8D54FA, 0x8D5556: "movss xmm, [0xF0318C]")
   // read 1.0 instead (the 30 fps update every frame; bit-identical at dt = 1). Patched at install only: the disp32 at 0x8D54FE
   // straddles a cache line, so it is never rewritten while the toggle moves.
   namespace ConsoleFrameTime
   {
      constexpr std::array<uintptr_t, 2> SITES = {0x8D54FA, 0x8D5556};
      constexpr std::array<std::array<uint8_t, 8>, 2> ORIGINALS = {{{0xF3, 0x0F, 0x10, 0x2D, 0x8C, 0x31, 0xF0, 0x00}, {0xF3, 0x0F, 0x10, 0x15, 0x8C, 0x31, 0xF0, 0x00}}};
      constexpr size_t ADDRESS_OFFSET = 4;

      static float value = 1.f; // Read by the game in place of the frame time
      static bool installed = false;

      static void OnFrame()
      {
         if (installed)
         {
            value = (enabled && *FRAME_TIME > 0.f) ? 1.f : *FRAME_TIME;
         }
      }

      static void Restore()
      {
         const auto original_address = uint32_t(reinterpret_cast<uintptr_t>(FRAME_TIME));
         for (const uintptr_t site : SITES)
         {
            System::PatchMemory(reinterpret_cast<void*>(site + ADDRESS_OFFSET), &original_address, sizeof(original_address), System::PatchMemoryType::Code);
         }
      }

      static bool Install()
      {
         for (size_t i = 0; i < SITES.size(); i++)
         {
            if (std::memcmp(reinterpret_cast<const void*>(SITES[i]), ORIGINALS[i].data(), ORIGINALS[i].size()) != 0)
               return false;
         }
         value = *FRAME_TIME;
         installed = true;
         const auto value_address = uint32_t(reinterpret_cast<uintptr_t>(&value));
         for (size_t i = 0; i < SITES.size() && installed; i++)
         {
            installed = System::PatchMemory(reinterpret_cast<void*>(SITES[i] + ADDRESS_OFFSET), &value_address, sizeof(value_address), System::PatchMemoryType::Code);
         }
         if (!installed)
         {
            Restore();
         }
         return installed;
      }

      static void Uninstall()
      {
         if (installed)
         {
            Restore();
            installed = false;
         }
      }
   } // namespace ConsoleFrameTime

   // The player's move update, FUN_00549840, re-places the player when the ground probe finds nothing and the position has not changed
   // for more than 15 frames (+0x7C10), so above 30 fps sooner (0.27 s at 60 fps instead of 0.53 s). Its count
   // "inc [esi+7C10h]; cmp [esi+7C10h], 0Fh; jle 0x549C97" becomes a call that counts on "PlayerFrameCounters"' 30 Hz ticks only.
   namespace StuckRecovery
   {
      uint8_t* const COUNT = reinterpret_cast<uint8_t*>(0x549B15);
      constexpr std::array<uint8_t, 19> ORIGINAL = {0xFF, 0x86, 0x10, 0x7C, 0x00, 0x00, 0x83, 0xBE, 0x10, 0x7C, 0x00, 0x00, 0x0F, 0x0F, 0x8E, 0x6F, 0x01, 0x00, 0x00};
      constexpr uintptr_t NOT_STUCK = 0x549C97;
      constexpr size_t CALL_OFFSET = 2;  // The rel32 of "call IsStuck"
      constexpr size_t JUMP_OFFSET = 10; // The rel32 of "jz NOT_STUCK"
      constexpr size_t STUCK_FRAMES = 0x7C10;
      constexpr int32_t MAX_STUCK_FRAMES = 15;

      static bool installed = false;

      // esi = the player; eax, ecx and edx are free there (all set again before their next read)
      static BOOL __stdcall IsStuck(uint8_t* player)
      {
         auto* const stuck_frames = reinterpret_cast<int32_t*>(player + STUCK_FRAMES);
         if (!enabled || PlayerFrameCounters::is_tick_frame)
         {
            ++*stuck_frames;
         }
         else
         {
            Count(Stat::STUCK_HELD);
         }
         return *stuck_frames > MAX_STUCK_FRAMES;
      }

      static bool Install()
      {
         if (std::memcmp(COUNT, ORIGINAL.data(), ORIGINAL.size()) != 0)
            return false;
         // push esi; call IsStuck; test eax, eax; jz NOT_STUCK; nop padding
         std::array<uint8_t, ORIGINAL.size()> patched = {0x56, 0xE8, 0x00, 0x00, 0x00, 0x00, 0x85, 0xC0, 0x0F, 0x84, 0x00, 0x00, 0x00, 0x00, 0x90, 0x90, 0x90, 0x90, 0x90};
         WriteRel32(patched.data(), COUNT, CALL_OFFSET, reinterpret_cast<uintptr_t>(&IsStuck));
         WriteRel32(patched.data(), COUNT, JUMP_OFFSET, NOT_STUCK);
         installed = System::PatchMemory(COUNT, patched.data(), patched.size(), System::PatchMemoryType::Code);
         return installed;
      }

      static void Uninstall()
      {
         if (installed && System::PatchMemory(COUNT, ORIGINAL.data(), ORIGINAL.size(), System::PatchMemoryType::Code))
         {
            installed = false;
         }
      }
   } // namespace StuckRecovery

   // Per frame updates the port left linear in the step, or as console constants in registers a constant redirect ("FrameConstants")
   // cannot reach. Each block's first bytes become "call Entry; <replayed original instructions>; jmp end" and its handler does the
   // block's work over the frame time instead; with the fix off, the game's math.
   // - Enemies coasting without thrust (FUN_00802580, the enemy locomotion: velocity +0x6DD0..D8, position += step * velocity) lost
   //   speed by "v *= step * 0.9" (x, z) and "v *= step * 0.8" (y), so they stopped sooner the higher the frame rate (20% of the speed
   //   left per 1/30 s at 60 fps instead of 90%; below ~27 fps it grew): now v *= (k * time scale)^frame time, the game's at 30 fps.
   // - FUN_008d56a0's trajectory offsets +0x6EF0/+0x6EF4 ease to +0x6F00/+0x6F04 by "o = t * 0.2 + o * 0.8" (0.1 and 0.9 once its timer
   //   +0x6F5C expired) per frame, the 0.2 and 0.1 held in registers the code after the blocks reads too: now k^frame time.
   // - FUN_0051d5a0's body lean +0xE840..E848 (esi = player): the hit impulse +0xE844 decays by 0.2 per 1/30 s boundary since the hit (the
   //   console held it a whole 1/30 s) and the lean +0xE848 eases to it by 0.2 per 1/30 s: now 1 - 0.8^frame time.
   // - FUN_00511080's boost dust timer +0xEB14: the reset after a spawn "v = 1" becomes v = max(v, -1) + 1, keeping the overshoot of its
   //   "v -= frame time" so the spawn rate is 30 per second at any frame rate.
   namespace BlockCalls
   {
      constexpr size_t VELOCITY = 0x6DD0;
      constexpr size_t TRAJECTORY_OFFSET = 0x6EF0;
      constexpr size_t TRAJECTORY_TARGET = 0x6F00;
      const double* const DRAG_XZ = reinterpret_cast<const double*>(0xD7BCA8);            // 0.9
      const double* const DRAG_Y = reinterpret_cast<const double*>(0xD7BAB8);             // 0.8
      const double* const EASE_INPUT_ACTIVE = reinterpret_cast<const double*>(0xD7AFD8);  // 0.2
      const double* const EASE_INPUT_EXPIRED = reinterpret_cast<const double*>(0xD7B1E0); // 0.1
      constexpr size_t LEAN_IMPULSE_HELD = 0xE801;
      constexpr size_t LEAN_TARGET = 0xE840;
      constexpr size_t LEAN_IMPULSE = 0xE844;
      constexpr size_t LEAN = 0xE848;
      constexpr size_t DUST_TIMER = 0xEB14;

      static void Drag(uint8_t* enemy, double drag, std::initializer_list<size_t> axes)
      {
         const float time_scale = *reinterpret_cast<const float*>(enemy + TIME_SCALE);
         const float frame_time = *FRAME_TIME;
         // The game's: the step rounded to float, times the drag
         double factor = double(time_scale * frame_time) * drag;
         if (enabled && frame_time > 0.f)
         {
            factor = std::pow(double(time_scale) * drag, double(frame_time));
         }
         auto* const velocity = reinterpret_cast<float*>(enemy + VELOCITY);
         for (const size_t axis : axes)
         {
            velocity[axis] = float(factor * double(velocity[axis]));
         }
      }

      static void Ease(uint8_t* object, double input, double k)
      {
         const float frame_time = *FRAME_TIME;
         if (enabled && frame_time > 0.f)
         {
            input *= EmaInputScale(k, double(frame_time));
            k = std::pow(k, double(frame_time));
         }
         auto* const offset = reinterpret_cast<float*>(object + TRAJECTORY_OFFSET);
         const auto* const target = reinterpret_cast<const float*>(object + TRAJECTORY_TARGET);
         for (size_t i = 0; i < 2; i++)
         {
            offset[i] = float(double(target[i]) * input + double(offset[i]) * k);
         }
      }

      static void Lean(uint8_t* player)
      {
         const double k = *EASE_INPUT_ACTIVE;
         const float frame_time = *FRAME_TIME;
         const auto* const target = reinterpret_cast<const float*>(player + LEAN_TARGET);
         auto* const impulse = reinterpret_cast<float*>(player + LEAN_IMPULSE);
         auto* const lean = reinterpret_cast<float*>(player + LEAN);
         const bool impulse_held = player[LEAN_IMPULSE_HELD] != 0;
         double rate = k;
         if (enabled && frame_time > 0.f)
         {
            // ponytail: one static age (one player at a time); per-object storage if two FUN_0051d5a0 objects ever coexist
            static double impulse_age = 0.0;
            if (impulse_held)
            {
               impulse_age = 0.0;
            }
            else
            {
               const double age = impulse_age + frame_time;
               *impulse = float(double(*impulse) * std::pow(k, std::floor(age) - std::floor(impulse_age)));
               impulse_age = age;
            }
            rate = PerConsoleFrame(k, double(frame_time));
         }
         else if (!impulse_held)
         {
            *impulse = float(double(*impulse) * k);
         }
         *lean = float(((double(*impulse) + double(*target)) - double(*lean)) * rate + double(*lean));
      }

      static void DustTimer(uint8_t* player)
      {
         auto* const timer = reinterpret_cast<float*>(player + DUST_TIMER);
         *timer = enabled ? ((std::max)(-1.f, *timer) + 1.f) : 1.f;
      }

      struct Block
      {
         uint8_t* start;
         uintptr_t end;               // Where the game continues after the block
         uintptr_t check_instruction; // "F2 0F xx xx" or "F3 0F xx xx" + check_address: the block's constant, verified before patching
         uint32_t check_address;
         void (*handler)(uint8_t* object); // esi
         uintptr_t tail = 0;               // Original instructions replayed after the call, their outputs read after the block
         size_t tail_size = 0;
      };
      constexpr size_t CALL_SIZE = 5;
      constexpr size_t JUMP_SIZE = 5;
      constexpr size_t MAX_TAIL_SIZE = 8;
      const std::array<Block, 6> BLOCKS = {{
         // FUN_00802580: x and z ("movsd xmm1, [0xD7BCA8]"), y ("mulsd xmm0, [0xD7BAB8]", only without upward thrust, edi = 0)
         {reinterpret_cast<uint8_t*>(0x8033A8), 0x80342D, 0x8033D5, 0xD7BCA8, [](uint8_t* enemy)
            { Drag(enemy, *DRAG_XZ, {0, 2}); }},
         {reinterpret_cast<uint8_t*>(0x803431), 0x803476, 0x80345B, 0xD7BAB8, [](uint8_t* enemy)
            { Drag(enemy, *DRAG_Y, {1}); }},
         // FUN_008d56a0: the timer running (0.8; "movsd xmm3, [0xD78C80]" replayed) and expired (0.9)
         {reinterpret_cast<uint8_t*>(0x8D59E3), 0x8D5A53, 0x8D59FA, 0xD7BAB8, [](uint8_t* object)
            { Ease(object, *EASE_INPUT_ACTIVE, *DRAG_Y); }, 0x8D5A3C, 8},
         {reinterpret_cast<uint8_t*>(0x8D5AF3), 0x8D5B52, 0x8D5B01, 0xD7BCA8, [](uint8_t* object)
            { Ease(object, *EASE_INPUT_EXPIRED, *DRAG_XZ); }},
         // FUN_0051d5a0: "movsd xmm2, [0xD7AFD8]"; the tail is "movss xmm0, [esi+0xE848]", read after the block
         {reinterpret_cast<uint8_t*>(0x51DB48), 0x51DBAB, 0x51DB25, 0xD7AFD8, Lean, 0x51DBF5, 8},
         // FUN_00511080: "movss xmm0, [0xD7A458]" (1.0); the tail "add esp, 10h" lies inside the block
         {reinterpret_cast<uint8_t*>(0x51226E), 0x512281, 0x51226E, 0xD7A458, DustTimer, 0x512276, 3},
      }};

      static std::array<std::array<uint8_t, CALL_SIZE + MAX_TAIL_SIZE + JUMP_SIZE>, BLOCKS.size()> originals = {};
      static bool installed = false;

      // return_address: Entry's, right after the patched block's call
      static void __stdcall Run(uint8_t* object, uintptr_t return_address)
      {
         for (size_t i = 0; i < BLOCKS.size(); i++)
         {
            if (return_address == uintptr_t(BLOCKS[i].start + CALL_SIZE))
            {
               Count(Stat(size_t(Stat::DRAG_XZ) + i));
               BLOCKS[i].handler(object);
               return;
            }
         }
      }

      // esi = the object. Keeps every general register, the flags and xmm0-7 (the blocks' outputs are all in memory)
      static __declspec(naked) void Entry()
      {
         __asm
         {
         pushfd
         pushad
         sub esp, 0x80
         movdqu [esp], xmm0
         movdqu [esp + 0x10], xmm1
         movdqu [esp + 0x20], xmm2
         movdqu [esp + 0x30], xmm3
         movdqu [esp + 0x40], xmm4
         movdqu [esp + 0x50], xmm5
         movdqu [esp + 0x60], xmm6
         movdqu [esp + 0x70], xmm7
         push dword ptr [esp + 0xA4] // The return address, above 0x80 + pushad's 0x20 + pushfd's 4
         push esi
         call Run
         movdqu xmm0, [esp]
         movdqu xmm1, [esp + 0x10]
         movdqu xmm2, [esp + 0x20]
         movdqu xmm3, [esp + 0x30]
         movdqu xmm4, [esp + 0x40]
         movdqu xmm5, [esp + 0x50]
         movdqu xmm6, [esp + 0x60]
         movdqu xmm7, [esp + 0x70]
         add esp, 0x80
         popad
         popfd
         ret
         }
      }

      static size_t PatchSize(const Block& block)
      {
         return CALL_SIZE + block.tail_size + JUMP_SIZE;
      }

      static void Restore(size_t count)
      {
         for (size_t i = 0; i < count; i++)
         {
            System::PatchMemory(BLOCKS[i].start, originals[i].data(), PatchSize(BLOCKS[i]), System::PatchMemoryType::Code);
         }
      }

      static bool Install()
      {
         for (const Block& block : BLOCKS)
         {
            const auto* const code = reinterpret_cast<const uint8_t*>(block.check_instruction);
            if ((code[0] != 0xF2 && code[0] != 0xF3) || code[1] != 0x0F || std::memcmp(code + 4, &block.check_address, sizeof(uint32_t)) != 0)
               return false;
         }
         installed = true;
         for (size_t i = 0; i < BLOCKS.size() && installed; i++)
         {
            const Block& block = BLOCKS[i];
            const size_t size = PatchSize(block);
            std::memcpy(originals[i].data(), block.start, size);
            std::array<uint8_t, CALL_SIZE + MAX_TAIL_SIZE + JUMP_SIZE> patched = {0xE8};
            WriteRel32(patched.data(), block.start, 1, reinterpret_cast<uintptr_t>(&Entry));
            // The replayed instructions only use absolute addresses (no rel32), so they run anywhere
            // A tail inside the block is copied from the saved original, not the live bytes
            const bool tail_in_block = block.tail >= uintptr_t(block.start) && block.tail < uintptr_t(block.start) + size;
            const void* const tail = tail_in_block ? originals[i].data() + (block.tail - uintptr_t(block.start)) : reinterpret_cast<const void*>(block.tail);
            std::memcpy(patched.data() + CALL_SIZE, tail, block.tail_size);
            patched[CALL_SIZE + block.tail_size] = 0xE9;
            WriteRel32(patched.data(), block.start, CALL_SIZE + block.tail_size + 1, block.end);
            installed = System::PatchMemory(block.start, patched.data(), size, System::PatchMemoryType::Code);
            if (!installed)
            {
               Restore(i);
            }
         }
         return installed;
      }

      static void Uninstall()
      {
         if (installed)
         {
            Restore(BLOCKS.size());
            installed = false;
         }
      }
   } // namespace BlockCalls

   // Every fix above, installed in this order and uninstalled in reverse (LinearTurnRate calls UnscaledApproaches' trampoline);
   // "on_frame" runs after the game sets the frame time ("FrameTimeUpdate")
   struct Fix
   {
      bool (*install)();
      void (*uninstall)();
      void (*on_frame)() = nullptr;
   };
   constexpr auto FIXES = std::to_array<Fix>({
      {HomingSteering::Install, Detach<&HomingSteering::hooked>},
      {ContinuousDamage::Install, ContinuousDamage::Uninstall, ContinuousDamage::OnFrame},
      {UnscaledTurns::Install, Detach<&UnscaledTurns::hooked>},
      {PlayerFrameCounters::Install, Detach<&PlayerFrameCounters::hooked>, PlayerFrameCounters::OnFrame},
      {ConsoleSlowdown::Install, []
         { ConsoleSlowdown::Apply(false); }, []
         { ConsoleSlowdown::Apply(enabled); }},
      {UnscaledApproaches::Install, Detach<&UnscaledApproaches::hooked>},
      {LinearTurnRate::Install, Detach<&LinearTurnRate::hooked>},
      {SnapApproaches::Install, Detach<&SnapApproaches::hooked>},
      {VectorApproaches::Install, Detach<&VectorApproaches::hooked>},
      {FrameConstants::Install, FrameConstants::Uninstall, FrameConstants::OnFrame},
      {ConsoleFrameTime::Install, ConsoleFrameTime::Uninstall, ConsoleFrameTime::OnFrame},
      {StuckRecovery::Install, StuckRecovery::Uninstall},
      {BlockCalls::Install, BlockCalls::Uninstall},
   });

   // FUN_00b8fc80 writes DAT_00f0318c, the frame time, right after the game's Present (its only call, 0xB96629): the per-frame values
   // are set here, on the game thread, from the frame time the next simulation step uses
   namespace FrameTimeUpdate
   {
      using Update = int(__cdecl*)();
      uint8_t* const UPDATE = reinterpret_cast<uint8_t*>(0xB8FC80);
      // "push ebp; mov ebp, esp; and esp, 0FFFFFFF8h; sub esp, 20h"
      constexpr std::array<uint8_t, 9> PROLOGUE = {0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF8, 0x83, 0xEC, 0x20};

      static MinHook::HookedFunction<Update> hooked;

#if DEVELOPMENT
      // Every 2 s of game time: the frame time range, the per-frame values and each fix's counts since the last line
      static void LogStats()
      {
         static double elapsed = 0.0; // In 1/30 s
         static uint32_t frames = 0;
         static float min_frame_time = FLT_MAX;
         static float max_frame_time = 0.f;
         const float frame_time = *FRAME_TIME;
         elapsed += frame_time;
         frames++;
         min_frame_time = (std::min)(min_frame_time, frame_time);
         max_frame_time = (std::max)(max_frame_time, frame_time);
         if (elapsed < 60.0)
            return;
         std::string line = std::format("[Vanquish] Frame rate fixes {}: {} frames, frame time {:.3f}-{:.3f}, slowdown gate {}, console frame time {:.3f}, frame constant 0 {:.3f} |",
            enabled ? "on" : "off", frames, min_frame_time, max_frame_time, ConsoleSlowdown::patched ? "patched" : "original", ConsoleFrameTime::value,
            FrameConstants::values[0]);
         for (size_t i = 0; i < stats.size(); i++)
         {
            line += std::format(" {}={}", STAT_NAMES[i], stats[i].exchange(0, std::memory_order_relaxed));
         }
         reshade::log::message(reshade::log::level::info, line.c_str());
         elapsed = 0.0;
         frames = 0;
         min_frame_time = FLT_MAX;
         max_frame_time = 0.f;
      }
#endif

      static int __cdecl Detour()
      {
         const int result = hooked.original();
#if DEVELOPMENT
         LogStats();
#endif
         for (const Fix& fix : FIXES)
         {
            if (fix.on_frame)
            {
               fix.on_frame();
            }
         }
         return result;
      }
   } // namespace FrameTimeUpdate

   static void Uninstall()
   {
      MinHook::Detach(&FrameTimeUpdate::hooked);
      for (const Fix& fix : FIXES | std::views::reverse)
      {
         fix.uninstall();
      }
   }

   // Whether any fix went in (only the Steam executable is patched). The frame time hook goes in last and out first: without it
   // the per-frame values would never follow the frame time, so nothing stays in.
   static bool Install()
   {
      if (!IsSteamExecutable())
         return false;
      bool installed = false;
      for (const Fix& fix : FIXES)
      {
         installed |= fix.install();
      }
      if (installed && !AttachVerified(&FrameTimeUpdate::hooked, FrameTimeUpdate::UPDATE, FrameTimeUpdate::PROLOGUE, &FrameTimeUpdate::Detour))
      {
         Uninstall();
         return false;
      }
      return installed;
   }
} // namespace FrameRateFixes
