// d3d9 logging proxy (x86): how much of a 32-bit D3D9 game's address space is
// texture shadow copies, and does the game touch MANAGED resources after their
// first upload (relocks, READONLY locks)? Install: rename the real D3D9.dll
// (dgVoodoo or a copy of the system one) to d3d9_chain.dll, put this as
// d3d9.dll next to the exe. With d3d9_memlog.on next to this dll, every 5 s
// (D3D9_MEMLOG_MS) it appends a line to d3d9_memlog.log there: live bytes per
// pool, the process's VA, lock stats. Without it nothing is logged.
//
// VA fix, on when d3d9_vafix.on exists next to this dll:
// - the device is created as D3D9Ex, so DEFAULT resources survive Reset;
// - MANAGED 2D and cube textures become DEFAULT; their CPU copy lives in a
//   pagefile-backed section (outside the address space), and a level is mapped
//   only while it is locked (DXVK's idea);
// - on the last unlock of a written level it is uploaded through a pooled
//   SYSTEMMEM staging texture (LRU, 64 MB) and UpdateSurface. When the runtime
//   takes user memory (Windows d3d9; dgVoodoo doesn't), staging memory comes
//   from one 64 MB arena reserved against the address space ceiling;
// - level views are mapped top-down, so the fix stays away from the middle of
//   the address space where the game's big allocations need room;
// - MANAGED volumes become DEFAULT DYNAMIC, MANAGED buffers DEFAULT (small in
//   UE3 games).
// DEFAULT resources count against dgVoodoo's VRAM setting (MANAGED ones don't),
// so with the fix dgVoodoo needs VRAM = 4096; the log warns near the limit.
//
// d3d9_nomt.on next to this dll (experiment): the device is created without
// D3DCREATE_MULTITHREADED, so dgVoodoo skips its per-call critical section.
// Only safe while one thread makes every D3D9 call.
//
// d3d9_csmt.on next to this dll: D3D9 calls run on a worker thread (csmt.h),
// which also makes dropping D3DCREATE_MULTITHREADED safe. With d3d9_memlog.on
// the report adds the layer's per-frame counters.
#include "csmt.h"
#include <algorithm>
#include <atomic>
#include <intrin.h>
#include <cstdio>
#include <d3d9.h>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <psapi.h>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <windows.h>

// Everything but Direct3DCreate9 goes straight to the real D3D9 (the loader
// resolves d3d9_chain.dll from the exe's folder).
#pragma comment(linker, "/EXPORT:Direct3DCreate9=_ProxyDirect3DCreate9@4")
#pragma comment(linker, \
   "/EXPORT:Direct3DCreate9Ex=d3d9_chain.Direct3DCreate9Ex")
#pragma comment(linker, \
   "/EXPORT:D3DPERF_BeginEvent=d3d9_chain.D3DPERF_BeginEvent")
#pragma comment(linker, "/EXPORT:D3DPERF_EndEvent=d3d9_chain.D3DPERF_EndEvent")
#pragma comment(linker, \
   "/EXPORT:D3DPERF_GetStatus=d3d9_chain.D3DPERF_GetStatus")
#pragma comment( \
   linker,       \
   "/EXPORT:D3DPERF_QueryRepeatFrame=d3d9_chain.D3DPERF_QueryRepeatFrame")
#pragma comment(linker, \
   "/EXPORT:D3DPERF_SetMarker=d3d9_chain.D3DPERF_SetMarker")
#pragma comment(linker, \
   "/EXPORT:D3DPERF_SetOptions=d3d9_chain.D3DPERF_SetOptions")
#pragma comment(linker, \
   "/EXPORT:D3DPERF_SetRegion=d3d9_chain.D3DPERF_SetRegion")
#pragma comment(linker, "/EXPORT:DebugSetMute=d3d9_chain.DebugSetMute")
#pragma comment( \
   linker,       \
   "/EXPORT:Direct3DShaderValidatorCreate9=d3d9_chain.Direct3DShaderValidatorCreate9")

namespace
{
   constexpr double MB = 1048576.0;

   // vtable slots, shared by Patch and Original.
   constexpr int SLOT_RELEASE = 2;           // IUnknown
   constexpr int SLOT_SET_LOD = 11;          // IDirect3DBaseTexture9
   constexpr int SLOT_SURFACE_GET_DESC = 12; // IDirect3DSurface9
   constexpr int SLOT_SURFACE_LOCK_RECT = 13;
   constexpr int SLOT_SURFACE_UNLOCK_RECT = 14;
   constexpr int SLOT_GET_LEVEL_DESC =
      17;                        // IDirect3DTexture9, IDirect3DCubeTexture9
   constexpr int SLOT_LOCK = 19; // LockRect (2D, cube), LockBox (volume)
   constexpr int SLOT_UNLOCK = 20;
   constexpr int SLOT_CREATE_DEVICE = 16; // IDirect3D9
   constexpr int SLOT_RESET = 16;         // IDirect3DDevice9
   constexpr int SLOT_CREATE_TEXTURE = 23;
   constexpr int SLOT_CREATE_VOLUME_TEXTURE = 24;
   constexpr int SLOT_CREATE_CUBE_TEXTURE = 25;
   constexpr int SLOT_CREATE_VERTEX_BUFFER = 26;
   constexpr int SLOT_CREATE_INDEX_BUFFER = 27;
   constexpr int SLOT_UPDATE_SURFACE = 30;
   constexpr int SLOT_UPDATE_TEXTURE = 31;

   wchar_t g_dir[MAX_PATH] = {};
   FILE* g_log = nullptr;              // "NUL" without d3d9_memlog.on
   bool g_log_enabled = false;         // d3d9_memlog.on present
   bool g_fix_requested = false;       // d3d9_vafix.on present
   bool g_strip_multithreaded = false; // d3d9_nomt.on present
   bool g_csmt_requested = false;      // d3d9_csmt.on present
   bool g_fix_active = false;          // the device really is D3D9Ex (one device assumed)
   bool g_user_memory = false;         // the runtime takes user-memory SYSTEMMEM textures
   UINT g_available_texture_mb =
      0; // what the runtime reports (dgVoodoo: its VRAM cap)

   HMODULE Chain()
   {
      static HMODULE chain = []
      {
         wchar_t path[MAX_PATH];
         swprintf_s(path, L"%sd3d9_chain.dll", g_dir);
         HMODULE module = LoadLibraryW(path);
         fprintf(g_log, "chain %ls -> %p (error %lu)\n", path, module,
            module ? 0 : GetLastError());
         fflush(g_log);
         return module;
      }();
      return chain;
   }
   // The product name in the chained runtime's version resource.
   bool ChainIsDgVoodoo()
   {
      static const bool dgvoodoo = []
      {
         HRSRC info = FindResourceW(Chain(), MAKEINTRESOURCEW(VS_VERSION_INFO), RT_VERSION);
         const auto* data = (const wchar_t*)(info ? LockResource(LoadResource(Chain(), info)) : nullptr);
         const std::wstring_view block(data, data ? SizeofResource(Chain(), info) / sizeof(wchar_t) : 0);
         return block.find(L"dgVoodoo") != std::wstring_view::npos;
      }();
      return dgvoodoo;
   }

   // The runtime validates calls such as UpdateSurface and StretchRect by calling
   // GetDesc through the vtable (Windows d3d9): reporting MANAGED to it makes it
   // reject our own uploads. Only callers outside the runtime get the game's view.
   bool CalledByRuntime(void* return_address)
   {
      static const auto [begin, end] = []
      {
         MODULEINFO info = {};
         GetModuleInformation(GetCurrentProcess(), Chain(), &info, sizeof(info));
         char* base = (char*)info.lpBaseOfDll;
         return std::pair{base, base + info.SizeOfImage};
      }();
      return return_address >= begin && return_address < end;
   }

   // ---- vtable hooks ----------------------------------------------------------

   // Originals per patched slot (dgVoodoo may use several classes per
   // interface). Append-only: readers scan without a lock, Patch publishes the
   // original before it swaps the slot.
   struct HookedSlot
   {
      void** slot;
      void* original;
   };
   constexpr int MAX_HOOKED_SLOTS = 128;
   HookedSlot g_hooked[MAX_HOOKED_SLOTS];
   std::atomic<int> g_hooked_count{0};
   std::mutex g_patch_mutex;

   template <typename T>
   T Original(void* object, int index)
   {
      void** slot = &(*(void***)object)[index];
      const int count = g_hooked_count.load(std::memory_order_acquire);
      for (int i = 0; i < count; i++)
      {
         if (g_hooked[i].slot == slot)
            return reinterpret_cast<T>(g_hooked[i].original);
      }
      // Never patched (GetDesc with the fix off, read by the LockRect hook): the
      // slot still holds the runtime's method. A Patch racing this read can
      // return the hook, which then finds its recorded original.
      return reinterpret_cast<T>(*slot);
   }

   void Patch(void* object, int index, void* hook)
   {
      void** slot = &(*(void***)object)[index];
      if (*slot == hook) // unlocked read; a stale value just takes the locked path
         return;
      std::lock_guard lock(g_patch_mutex);
      const int count = g_hooked_count.load(std::memory_order_relaxed);
      if (*slot == hook || count == MAX_HOOKED_SLOTS)
         return;
      for (int i = 0; i < count; i++)
      {
         if (g_hooked[i].slot == slot)
            return; // patched before; the CSMT layer may sit above our hook now
      }
      // A slot the CSMT layer owns: our hook goes below it, so it runs in the
      // layer's order (on the worker for deferred calls).
      if (void* below = Csmt::InsertBelow(object, index, hook))
      {
         g_hooked[count] = {.slot = slot, .original = below};
         g_hooked_count.store(count + 1, std::memory_order_release);
         return;
      }
      g_hooked[count] = {.slot = slot, .original = *slot};
      g_hooked_count.store(count + 1, std::memory_order_release);
      DWORD old_protect;
      VirtualProtect(slot, sizeof(void*), PAGE_EXECUTE_READWRITE, &old_protect);
      *slot = hook;
      VirtualProtect(slot, sizeof(void*), old_protect, &old_protect);
   }

   // ---- counters and report
   // -----------------------------------------------------

   // Reset by every report. Atomic so neither the hooks nor the fix take a lock
   // just to count.
   enum Counter
   {
      MANAGED_LOCKS,    // locks of MANAGED textures (any kind)
      MANAGED_RELOCKS,  // a MANAGED texture locked more times than it has levels
      MANAGED_READONLY, // D3DLOCK_READONLY on MANAGED: the game reads a copy back
      MANAGED_PARTIAL,  // non-null rect/box
      SURFACE_MANAGED,  // IDirect3DSurface9::LockRect on a MANAGED surface (D3DX)
      SURFACE_READONLY,
      OTHER_LOCKS, // DEFAULT/SYSTEMMEM texture locks
      UPLOADS,
      UPLOAD_BYTES,
      UPLOAD_FAILURES,
      POOL_HITS,        // staging texture reused
      POOL_MISSES,      // staging texture created
      POOL_BUSY,        // pooled staging still read by the GPU
      POOL_EVICTIONS,   // over budget, released
      TOP_DOWN_VIEWS,   // level views mapped top-down
      OS_VIEWS,         // no MapViewOfFile3: placed by the OS
      ARENA_STAGING,    // staging memory from the arena
      TOP_DOWN_STAGING, // arena full: our staging memory top-down outside it
      RUNTIME_STAGING,  // allocated by the runtime (no user memory, tiny DXT chains)
      AUTOGEN,          // GenerateMipSubLevels after a level 0 upload
      SURFACE_SERVED,   // surface locks served from a shadow
      STALE_SURFACES,   // surface entries of a texture that died through a surface
      LOCK_FAILURES,
      SET_LOD_REPLACED,      // SetLOD on a replaced texture (DEFAULT ignores it)
      UPDATES_INTO_REPLACED, // game UpdateSurface/UpdateTexture into a replaced
                             // texture: its shadow goes stale
      COUNTER_COUNT,
   };
   std::atomic<uint64_t> g_counters[COUNTER_COUNT];

   void Count(Counter counter, uint64_t amount = 1)
   {
      g_counters[counter].fetch_add(amount, std::memory_order_relaxed);
   }

   // Live totals, kept current by create/release/map/unmap so a report never has
   // to walk the resources. Pool index = D3DPOOL (DEFAULT, MANAGED, SYSTEMMEM,
   // SCRATCH), as the game asked for it.
   struct Totals
   {
      std::atomic<int64_t> texture_bytes[4], texture_count[4], buffer_bytes[4];
      std::atomic<int64_t> rendertarget_bytes, shadow_bytes, shadow_count;
      std::atomic<int64_t> mapped_bytes, mapped_count, arena_bytes;
      std::atomic<uint64_t> resets, creates_failed, converted_volumes,
         converted_buffers, section_failures, shadow_fallbacks;
   };
   Totals g_totals;

   void Report(const char* event)
   {
      size_t free_total = 0, largest_free = 0, committed = 0;
      MEMORY_BASIC_INFORMATION mbi;
      for (char* p = nullptr; VirtualQuery(p, &mbi, sizeof(mbi));
         p = (char*)mbi.BaseAddress + mbi.RegionSize)
      {
         if (mbi.State == MEM_FREE)
         {
            free_total += mbi.RegionSize;
            largest_free = max(largest_free, mbi.RegionSize);
         }
         else if (mbi.State == MEM_COMMIT)
         {
            committed += mbi.RegionSize;
         }
         if ((uintptr_t)mbi.BaseAddress + mbi.RegionSize <
             (uintptr_t)mbi.BaseAddress)
            break;
      }
      PROCESS_MEMORY_COUNTERS_EX pmc = {sizeof(pmc)};
      GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc,
         sizeof(pmc));
      uint64_t c[COUNTER_COUNT];
      for (int i = 0; i < COUNTER_COUNT; i++)
         c[i] = g_counters[i].exchange(0, std::memory_order_relaxed);
      const Totals& t = g_totals;
      SYSTEMTIME now;
      GetLocalTime(&now);
      fprintf(
         g_log,
         "%02u:%02u:%02u %-13s | VA commit %6.0f private %6.0f free %6.0f "
         "largest %5.0f | tex MANAGED %5lld %6.0f MB  DEFAULT %5lld %6.0f MB "
         "(RT/DS %5.0f)  SYSMEM %5.0f  SCRATCH %4.0f | buf MANAGED %5.0f "
         "DEFAULT %5.0f SYSMEM %4.0f | locks M %llu relock %llu RO %llu partial "
         "%llu surfM %llu surfRO %llu other %llu | resets %llu fails %llu\n",
         now.wHour, now.wMinute, now.wSecond, event, committed / MB,
         pmc.PrivateUsage / MB, free_total / MB, largest_free / MB,
         t.texture_count[1].load(), t.texture_bytes[1] / MB,
         t.texture_count[0].load(), t.texture_bytes[0] / MB,
         t.rendertarget_bytes / MB, t.texture_bytes[2] / MB,
         t.texture_bytes[3] / MB, t.buffer_bytes[1] / MB, t.buffer_bytes[0] / MB,
         t.buffer_bytes[2] / MB, c[MANAGED_LOCKS], c[MANAGED_RELOCKS],
         c[MANAGED_READONLY], c[MANAGED_PARTIAL], c[SURFACE_MANAGED],
         c[SURFACE_READONLY], c[OTHER_LOCKS], t.resets.load(),
         t.creates_failed.load());
      if (g_fix_active)
      {
         // With the fix every MANAGED texture and buffer is DEFAULT to the runtime.
         const double default_mb = (t.texture_bytes[0] + t.texture_bytes[1] +
                                      t.buffer_bytes[0] + t.buffer_bytes[1]) /
                                   MB;
         fprintf(
            g_log,
            "         fix | shadows %5lld %6.0f MB (outside VA), mapped levels now "
            "%lld %5.1f MB | uploads %llu %6.1f MB, failed %llu, autogen %llu | "
            "staging pool hit %llu miss %llu busy %llu evicted %llu | views "
            "top-down %llu os %llu, staging arena %llu top-down %llu runtime %llu, "
            "arena used %.1f MB | DEFAULT %.0f of %u MB | surface served %llu, "
            "stale surfaces %llu, lock failures %llu, set LOD %llu, updates into "
            "replaced %llu | converted volumes %llu buffers %llu, section "
            "failures %llu, shadow fallbacks %llu\n",
            t.shadow_count.load(), t.shadow_bytes / MB, t.mapped_count.load(),
            t.mapped_bytes / MB, c[UPLOADS], c[UPLOAD_BYTES] / MB,
            c[UPLOAD_FAILURES], c[AUTOGEN], c[POOL_HITS], c[POOL_MISSES],
            c[POOL_BUSY], c[POOL_EVICTIONS], c[TOP_DOWN_VIEWS], c[OS_VIEWS],
            c[ARENA_STAGING], c[TOP_DOWN_STAGING], c[RUNTIME_STAGING],
            t.arena_bytes / MB, default_mb, g_available_texture_mb,
            c[SURFACE_SERVED], c[STALE_SURFACES], c[LOCK_FAILURES],
            c[SET_LOD_REPLACED], c[UPDATES_INTO_REPLACED],
            t.converted_volumes.load(), t.converted_buffers.load(),
            t.section_failures.load(), t.shadow_fallbacks.load());
         // Below ~4 GB the reported value is a real cap (dgVoodoo: VRAM - 5 MB);
         // at 4095+ it is clamped and says nothing.
         static bool warned = false;
         if (!warned && g_available_texture_mb < 4000 &&
             default_mb > 0.9 * g_available_texture_mb)
         {
            warned = true;
            fprintf(g_log,
               "VA fix: WARNING DEFAULT resources at %.0f of %u MB: raise the "
               "runtime's video memory (dgVoodoo VRAM = 4096) or creates will "
               "fail\n",
               default_mb, g_available_texture_mb);
         }
      }
      fflush(g_log);
   }

   // Logged at once, with a full report: a game that can't handle the failure
   // (UE3 aborts) dies before the next tick.
   HRESULT CreateFailed(HRESULT hr, const char* kind, UINT width, UINT height,
      DWORD format, D3DPOOL asked, bool replaced, DWORD usage)
   {
      if (g_totals.creates_failed++ < 100)
      {
         fprintf(
            g_log,
            "Create%s failed 0x%08lX%s: %s %u%s%u format/fvf %lu usage 0x%lX pool "
            "%d%s\n",
            kind, hr, (hr == D3DERR_OUTOFVIDEOMEMORY) ? " (OUTOFVIDEOMEMORY)" : "",
            height ? "size" : "length", width, height ? "x" : "", height, format,
            usage, int(asked), replaced ? " -> DEFAULT (VA fix)" : "");
         Report("create failed");
      }
      return hr;
   }

   // ---- formats ---------------------------------------------------------------

   struct FormatInfo
   {
      UINT bytes; // per pixel, or per 4x4 block
      bool block;
   };

   FormatInfo GetFormatInfo(D3DFORMAT format)
   {
      switch (format)
      {
      case D3DFMT_DXT1:
      case MAKEFOURCC('A', 'T', 'I', '1'):
         return {.bytes = 8, .block = true};
      case D3DFMT_DXT2:
      case D3DFMT_DXT3:
      case D3DFMT_DXT4:
      case D3DFMT_DXT5:
      case MAKEFOURCC('A', 'T', 'I', '2'):
         return {.bytes = 16, .block = true};
      case D3DFMT_L8:
      case D3DFMT_A8:
      case D3DFMT_P8:
         return {.bytes = 1};
      case D3DFMT_R5G6B5:
      case D3DFMT_X1R5G5B5:
      case D3DFMT_A1R5G5B5:
      case D3DFMT_A4R4G4B4:
      case D3DFMT_A8L8:
      case D3DFMT_V8U8:
      case D3DFMT_L16:
      case D3DFMT_R16F:
      case D3DFMT_D16:
         return {.bytes = 2};
      case D3DFMT_A16B16G16R16:
      case D3DFMT_A16B16G16R16F:
      case D3DFMT_G32R32F:
         return {.bytes = 8};
      case D3DFMT_A32B32G32R32F:
         return {.bytes = 16};
      default:
         return {.bytes = 4};
      }
   }

   // One level, tightly packed (pitch = one row of pixels or of 4x4 blocks).
   struct LevelSize
   {
      UINT width, height, pitch, rows;
      size_t bytes;
   };

   LevelSize GetLevelSize(D3DFORMAT format, UINT width, UINT height, UINT level)
   {
      const FormatInfo info = GetFormatInfo(format);
      const UINT w = max(width >> level, 1u);
      const UINT h = max(height >> level, 1u);
      const UINT pitch =
         (info.block ? max((w + 3) / 4, 1u) * info.bytes : w * info.bytes);
      const UINT rows = (info.block ? max((h + 3) / 4, 1u) : h);
      return {.width = w,
         .height = h,
         .pitch = pitch,
         .rows = rows,
         .bytes = size_t(pitch) * rows};
   }

   uint64_t ChainBytes(D3DFORMAT format, UINT width, UINT height, UINT depth,
      UINT levels)
   {
      uint64_t total = 0;
      for (UINT level = 0; level < levels; level++)
      {
         total += uint64_t(GetLevelSize(format, width, height, level).bytes) *
                  max(depth >> level, 1u);
      }
      return total;
   }

   // ---- placement
   // ---------------------------------------------------------------

   using MapViewOfFile3Fn = PVOID(WINAPI*)(HANDLE, HANDLE, PVOID, ULONG64, SIZE_T,
      ULONG, ULONG, MEM_EXTENDED_PARAMETER*,
      ULONG);

   // Top-down keeps the fix's views against the address space ceiling, away from
   // the middle where the game's heaps grow. MapViewOfFile has no top-down flag;
   // MapViewOfFile3 (Windows 10 1803+) takes MEM_TOP_DOWN.
   char* MapTopDown(HANDLE section, size_t offset, size_t size)
   {
      static const auto map_view3 = (MapViewOfFile3Fn)GetProcAddress(
         GetModuleHandleW(L"kernelbase.dll"), "MapViewOfFile3");
      if (map_view3)
      {
         if (void* view =
                map_view3(section, GetCurrentProcess(), nullptr, offset, size,
                   MEM_TOP_DOWN, PAGE_READWRITE, nullptr, 0))
         {
            Count(TOP_DOWN_VIEWS);
            return (char*)view;
         }
      }
      Count(OS_VIEWS);
      return (char*)MapViewOfFile(section, FILE_MAP_ALL_ACCESS,
         DWORD(uint64_t(offset) >> 32), DWORD(offset),
         size);
   }

   // The staging pool is the fix's only long-lived allocation, and pool entries
   // come and go, so allocated one by one they would leave holes near the
   // ceiling. They come from one reservation made at device creation instead:
   // pages are committed per block and decommitted on free, blocks are first fit
   // with neighbours merged. Callers know each block's size.
   class StagingArena
   {
   public:
      static constexpr size_t SIZE = 64 << 20; // = the pool budget
      static constexpr size_t PAGE = 4096;

      // Once per process: a second device reuses it.
      void Reserve()
      {
         std::lock_guard lock(mutex_);
         if (base_)
            return;
         base_ = (char*)VirtualAlloc(nullptr, SIZE, MEM_RESERVE | MEM_TOP_DOWN,
            PAGE_READWRITE);
         if (base_)
         {
            free_[0] = SIZE;
         }
      }

      char* Base()
      {
         std::lock_guard lock(mutex_);
         return base_;
      }

      void* Allocate(size_t size)
      {
         size = Round(size);
         std::lock_guard lock(mutex_);
         for (auto it = free_.begin(); it != free_.end(); ++it)
         {
            auto [offset, block] = *it;
            if (block < size)
               continue;
            if (!VirtualAlloc(base_ + offset, size, MEM_COMMIT, PAGE_READWRITE))
               return nullptr;
            free_.erase(it);
            if (block > size)
            {
               free_[offset + size] = block - size;
            }
            g_totals.arena_bytes += size;
            return base_ + offset;
         }
         return nullptr;
      }

      // False if the memory isn't from the arena.
      bool Free(void* memory, size_t size)
      {
         size = Round(size);
         std::lock_guard lock(mutex_);
         if (!base_ || memory < base_ || memory >= base_ + SIZE)
            return false;
         VirtualFree(memory, size, MEM_DECOMMIT);
         g_totals.arena_bytes -= size;
         size_t start = (char*)memory - base_;
         if (auto next = free_.find(start + size); next != free_.end())
         {
            size += next->second;
            free_.erase(next);
         }
         if (auto next = free_.lower_bound(start); next != free_.begin())
         {
            if (auto previous = std::prev(next);
               previous->first + previous->second == start)
            {
               start = previous->first;
               size += previous->second;
               free_.erase(previous);
            }
         }
         free_[start] = size;
         return true;
      }

   private:
      static size_t Round(size_t size)
      {
         return (size + PAGE - 1) & ~(PAGE - 1);
      }

      std::mutex mutex_;
      char* base_ = nullptr;
      std::map<size_t, size_t> free_; // offset -> size
   };
   StagingArena g_staging_arena;

   // Committed staging memory: the arena, else top-down outside it.
   void* AllocStagingMemory(size_t size)
   {
      if (void* memory = g_staging_arena.Allocate(size))
      {
         Count(ARENA_STAGING);
         return memory;
      }
      void* memory = VirtualAlloc(
         nullptr, size, MEM_RESERVE | MEM_COMMIT | MEM_TOP_DOWN, PAGE_READWRITE);
      if (memory)
      {
         Count(TOP_DOWN_STAGING);
      }
      return memory;
   }

   void FreeStagingMemory(void* memory, size_t size)
   {
      if (!g_staging_arena.Free(memory, size))
      {
         VirtualFree(memory, 0, MEM_RELEASE);
      }
   }

   // ---- tracked resources -----------------------------------------------------

   // VA fix: CPU copy of a replaced texture. Levels (x faces) are packed with
   // tight pitch in one section; a level is mapped only while it is locked.
   struct Shadow
   {
      struct Level
      {
         size_t offset; // in the section
         LevelSize size;
         IDirect3DSurface9* surface; // the DEFAULT level, lives as long as the texture
         char* base = nullptr;       // view (granularity aligned), while locked
         char* bits = nullptr;       // the level inside the view
         // Locked from the first Lock until Unlock has uploaded and unmapped; a
         // second lock meanwhile fails, as the runtime's own would.
         bool locked = false;
         bool writable = false; // the lock was not READONLY: upload on unlock

         void Map(char* view, size_t level_offset_in_view)
         {
            base = view;
            bits = view + level_offset_in_view;
            g_totals.mapped_bytes += size.bytes;
            g_totals.mapped_count++;
         }

         // Returns the view to unmap, or nullptr.
         char* Unmap()
         {
            char* view = base;
            base = bits = nullptr;
            if (view)
            {
               g_totals.mapped_bytes -= size.bytes;
               g_totals.mapped_count--;
            }
            return view;
         }
      };
      HANDLE section = nullptr;
      size_t bytes = 0;
      D3DFORMAT format = D3DFMT_UNKNOWN;
      bool autogen = false;
      IDirect3DDevice9* device = nullptr; // outlives its resources
      UINT level_count = 0;
      std::vector<Level> levels; // face * level_count + level

      Level* Find(UINT face, UINT level)
      {
         const UINT sub = face * level_count + level;
         return (level < level_count && sub < levels.size()) ? &levels[sub]
                                                             : nullptr;
      }

      ~Shadow()
      {
         for (Level& level : levels)
         {
            if (char* view = level.Unmap())
            {
               UnmapViewOfFile(view);
            }
         }
         if (section)
         {
            CloseHandle(section);
         }
      }
   };

   struct Resource
   {
      bool buffer;
      D3DPOOL pool; // as the game asked for it
      DWORD usage;
      uint64_t bytes;
      UINT subresources; // levels x faces, 1 for buffers
      UINT locks = 0;
      std::unique_ptr<Shadow> shadow; // VA fix
      // Set by Track; a Release erases the entry only if it still has the value it
      // saw, since a concurrent Create may reuse the address once the object dies.
      uint64_t generation = 0;
   };

   struct SurfaceRef
   {
      void* texture;
      UINT face, level;
   };

   // mutex is a leaf lock: never held across a call into the D3D runtime (a
   // runtime may call our hooked methods while holding its own lock).
   struct State
   {
      std::mutex mutex;
      std::unordered_map<void*, Resource> resources;
      std::unordered_map<void*, SurfaceRef>
         surfaces; // levels of replaced textures
      uint64_t last_generation = 0;
   };
   State g_state;

   // Caller holds g_state.mutex.
   Resource* FindResource(void* object)
   {
      auto it = g_state.resources.find(object);
      return (it == g_state.resources.end()) ? nullptr : &it->second;
   }

   void AddTotals(const Resource& resource, int64_t sign)
   {
      const int pool = min(int(resource.pool), 3);
      const int64_t bytes = sign * int64_t(resource.bytes);
      if (resource.buffer)
      {
         g_totals.buffer_bytes[pool] += bytes;
         return;
      }
      g_totals.texture_bytes[pool] += bytes;
      g_totals.texture_count[pool] += sign;
      if (resource.usage & (D3DUSAGE_RENDERTARGET | D3DUSAGE_DEPTHSTENCIL))
      {
         g_totals.rendertarget_bytes += bytes;
      }
      if (resource.shadow)
      {
         g_totals.shadow_bytes += sign * int64_t(resource.shadow->bytes);
         g_totals.shadow_count += sign;
      }
   }

   // Caller holds g_state.mutex.
   void SetSurfaces(void* texture, const Shadow& shadow, bool add)
   {
      for (UINT sub = 0; sub < shadow.levels.size(); sub++)
      {
         IDirect3DSurface9* surface = shadow.levels[sub].surface;
         if (add)
         {
            g_state.surfaces[surface] = {.texture = texture,
               .face = sub / shadow.level_count,
               .level = sub % shadow.level_count};
         }
         else
         {
            g_state.surfaces.erase(surface);
         }
      }
   }

   // Caller holds g_state.mutex and erases the entry; the returned shadow is
   // destroyed (unmapped, closed) after the lock is released.
   std::unique_ptr<Shadow> Untrack(void* object, Resource* resource)
   {
      AddTotals(*resource, -1);
      if (resource->shadow)
      {
         SetSurfaces(object, *resource->shadow, false);
      }
      return std::move(resource->shadow);
   }

   ULONG STDMETHODCALLTYPE HookRelease(IUnknown* self)
   {
      uint64_t generation = 0; // 0 = untracked (our staging textures)
      {
         std::lock_guard lock(g_state.mutex);
         if (const Resource* resource = FindResource(self))
         {
            generation = resource->generation;
         }
      }
      const ULONG refs = Original<ULONG(STDMETHODCALLTYPE*)(IUnknown*)>(
         self, SLOT_RELEASE)(self);
      if (refs != 0 || generation == 0)
         return refs;
      std::unique_ptr<Shadow> shadow; // destroyed after the lock
      std::lock_guard lock(g_state.mutex);
      if (auto it = g_state.resources.find(self);
         it != g_state.resources.end() && it->second.generation == generation)
      {
         shadow = Untrack(self, &it->second);
         g_state.resources.erase(it);
      }
      return refs;
   }

   // Common tail of every Create* hook.
   void Track(void* object, Resource resource)
   {
      Patch(object, SLOT_RELEASE, (void*)&HookRelease);
      std::unique_ptr<Shadow> stale; // destroyed after the lock
      std::lock_guard lock(g_state.mutex);
      resource.generation = ++g_state.last_generation;
      AddTotals(resource, +1);
      Resource* entry = FindResource(object);
      if (entry)
      {
         // An entry whose last reference went through a level surface's Release,
         // which isn't hooked.
         stale = Untrack(object, entry);
         *entry = std::move(resource);
      }
      else
      {
         entry =
            &g_state.resources.emplace(object, std::move(resource)).first->second;
      }
      if (entry->shadow)
      {
         SetSurfaces(object, *entry->shadow, true);
      }
   }

   // Drops a stale entry at the address of an object we create ourselves.
   void ForgetStale(void* object)
   {
      std::unique_ptr<Shadow> stale; // destroyed after the lock
      std::lock_guard lock(g_state.mutex);
      if (auto it = g_state.resources.find(object); it != g_state.resources.end())
      {
         stale = Untrack(object, &it->second);
         g_state.resources.erase(it);
      }
   }

   // Section and level table for a replaced texture; nullptr if the section can't
   // be created.
   std::unique_ptr<Shadow> CreateShadow(IDirect3DBaseTexture9* texture,
      IDirect3DDevice9* device, D3DFORMAT format,
      UINT width, UINT height, UINT faces,
      DWORD usage)
   {
      auto shadow = std::make_unique<Shadow>();
      shadow->format = format;
      shadow->autogen = usage & D3DUSAGE_AUTOGENMIPMAP;
      shadow->device = device;
      shadow->level_count = texture->GetLevelCount();
      for (UINT face = 0; face < faces; face++)
      {
         for (UINT level = 0; level < shadow->level_count; level++)
         {
            IDirect3DSurface9* surface = nullptr;
            if (faces == 1)
            {
               ((IDirect3DTexture9*)texture)->GetSurfaceLevel(level, &surface);
            }
            else
            {
               ((IDirect3DCubeTexture9*)texture)
                  ->GetCubeMapSurface(D3DCUBEMAP_FACES(face), level, &surface);
            }
            if (surface)
            {
               surface->Release(); // the texture keeps it alive
            }
            const LevelSize size = GetLevelSize(format, width, height, level);
            shadow->levels.push_back(
               {.offset = shadow->bytes, .size = size, .surface = surface});
            shadow->bytes += size.bytes;
         }
      }
      shadow->section =
         CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0,
            DWORD(shadow->bytes), nullptr);
      if (!shadow->section)
      {
         g_totals.section_failures++;
         return nullptr;
      }
      return shadow;
   }

   // ---- staging pool ----------------------------------------------------------

   struct StagingKey
   {
      IDirect3DDevice9* device;
      D3DFORMAT format;
      UINT width, height, levels;
      bool operator==(const StagingKey&) const = default;
   };

   // SYSTEMMEM staging textures, reused across uploads. They live in the address
   // space, so the pool is capped; the least recently used go first. Entries with
   // the same key are interchangeable and the pool is small, so a list is enough.
   class StagingPool
   {
   public:
      static constexpr size_t BUDGET = StagingArena::SIZE;
      static constexpr size_t MAX_ENTRIES = 256; // tiny levels still reserve 64 KB

      struct Entry
      {
         StagingKey key;
         IDirect3DTexture9* texture;
         size_t bytes;
         void* memory; // ours (user-memory SYSTEMMEM texture, bytes long) or nullptr
      };

      std::optional<Entry> Acquire(const StagingKey& key)
      {
         std::lock_guard lock(mutex_);
         auto it = std::find_if(lru_.begin(), lru_.end(), [&](const Entry& entry)
            { return entry.key == key; });
         if (it == lru_.end())
            return std::nullopt;
         Entry entry = *it;
         bytes_ -= entry.bytes;
         lru_.erase(it);
         return entry;
      }

      void Put(const Entry& entry)
      {
         std::vector<Entry> evicted;
         {
            std::lock_guard lock(mutex_);
            lru_.push_front(entry);
            bytes_ += entry.bytes;
            while (bytes_ > BUDGET || lru_.size() > MAX_ENTRIES)
            {
               evicted.push_back(lru_.back());
               bytes_ -= lru_.back().bytes;
               lru_.pop_back();
            }
         }
         for (const Entry& old : evicted)
            Destroy(old);
         Count(POOL_EVICTIONS, evicted.size());
      }

      size_t Size()
      {
         std::lock_guard lock(mutex_);
         return lru_.size();
      }

      void Flush()
      {
         std::list<Entry> entries;
         {
            std::lock_guard lock(mutex_);
            entries.swap(lru_);
            bytes_ = 0;
         }
         for (const Entry& entry : entries)
            Destroy(entry);
      }

      static void Destroy(const Entry& entry)
      {
         if (entry.memory)
         {
            // Pending UpdateSurface reads of our memory must finish before its pages
            // are decommitted or reused; a blocking write lock waits for them (a
            // READONLY one needn't).
            if (D3DLOCKED_RECT lr;
               SUCCEEDED(entry.texture->LockRect(0, &lr, nullptr, 0)))
            {
               entry.texture->UnlockRect(0);
            }
         }
         entry.texture->Release();
         if (entry.memory)
         {
            FreeStagingMemory(entry.memory, entry.bytes);
         }
      }

   private:
      std::mutex mutex_;
      std::list<Entry> lru_; // front = most recently returned
      size_t bytes_ = 0;
   };
   StagingPool g_staging_pool;

   using CreateTextureFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, UINT,
      UINT, UINT, DWORD,
      D3DFORMAT, D3DPOOL,
      IDirect3DTexture9**,
      HANDLE*);
   using CreateCubeTextureFn = HRESULT(STDMETHODCALLTYPE*)(
      IDirect3DDevice9*, UINT, UINT, DWORD, D3DFORMAT, D3DPOOL,
      IDirect3DCubeTexture9**, HANDLE*);
   using UpdateSurfaceFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*,
      IDirect3DSurface9*,
      const RECT*,
      IDirect3DSurface9*,
      const POINT*);

   // A new staging texture. D3D9Ex takes a single-level SYSTEMMEM texture over
   // our memory (passed through pSharedHandle), so it can live in the arena.
   std::optional<StagingPool::Entry> CreateStaging(const StagingKey& key)
   {
      auto create = Original<CreateTextureFn>(key.device, SLOT_CREATE_TEXTURE);
      const size_t bytes =
         size_t(ChainBytes(key.format, key.width, key.height, 1, key.levels));
      IDirect3DTexture9* texture = nullptr;
      if (key.levels == 1 && g_user_memory)
      {
         if (void* memory = AllocStagingMemory(bytes))
         {
            HANDLE user_memory = memory;
            if (SUCCEEDED(create(key.device, key.width, key.height, 1, 0, key.format,
                   D3DPOOL_SYSTEMMEM, &texture, &user_memory)))
            {
               ForgetStale(texture);
               return StagingPool::Entry{
                  .key = key, .texture = texture, .bytes = bytes, .memory = memory};
            }
            FreeStagingMemory(memory, bytes);
         }
      }
      if (FAILED(create(key.device, key.width, key.height, key.levels, 0,
             key.format, D3DPOOL_SYSTEMMEM, &texture, nullptr)))
         return std::nullopt;
      ForgetStale(texture);
      Count(RUNTIME_STAGING);
      return StagingPool::Entry{
         .key = key, .texture = texture, .bytes = bytes, .memory = nullptr};
   }

   // Copies one shadow level into the DEFAULT texture's surface.
   bool Upload(IDirect3DDevice9* device, IDirect3DSurface9* destination,
      D3DFORMAT format, const LevelSize& size, const char* bits)
   {
      // DXT levels under 4x4 can't be a top level: stage the smallest chain whose
      // top is at least 4x4 and use its matching level.
      UINT staging_level = 0;
      while (GetFormatInfo(format).block && ((size.width << staging_level) < 4 ||
                                               (size.height << staging_level) < 4))
         staging_level++;
      const StagingKey key = {.device = device,
         .format = format,
         .width = size.width << staging_level,
         .height = size.height << staging_level,
         .levels = staging_level + 1};
      D3DLOCKED_RECT lr = {};
      HRESULT hr = E_FAIL;
      std::optional<StagingPool::Entry> staging = g_staging_pool.Acquire(key);
      if (staging)
      {
         hr = staging->texture->LockRect(staging_level, &lr, nullptr,
            D3DLOCK_DONOTWAIT);
         if (SUCCEEDED(hr))
         {
            Count(POOL_HITS);
         }
         else
         {
            // Still read by the GPU: back to the pool. Any other failure: unusable.
            if (hr == D3DERR_WASSTILLDRAWING)
            {
               Count(POOL_BUSY);
               g_staging_pool.Put(*staging);
            }
            else
            {
               StagingPool::Destroy(*staging);
            }
            staging.reset();
         }
      }
      if (!staging)
      {
         Count(POOL_MISSES);
         staging = CreateStaging(key);
         if (!staging)
            return false;
         hr = staging->texture->LockRect(staging_level, &lr, nullptr, 0);
      }
      bool ok = false;
      if (SUCCEEDED(hr))
      {
         if (UINT(lr.Pitch) == size.pitch)
         {
            memcpy(lr.pBits, bits, size.bytes);
         }
         else
         {
            for (UINT row = 0; row < size.rows; row++)
            {
               memcpy((char*)lr.pBits + size_t(row) * lr.Pitch,
                  bits + size_t(row) * size.pitch, size.pitch);
            }
         }
         staging->texture->UnlockRect(staging_level);
         if (IDirect3DSurface9* surface = nullptr;
            SUCCEEDED(staging->texture->GetSurfaceLevel(staging_level, &surface)))
         {
            ok = SUCCEEDED(Original<UpdateSurfaceFn>(device, SLOT_UPDATE_SURFACE)(
               device, surface, nullptr, destination, nullptr));
            surface->Release();
         }
      }
      g_staging_pool.Put(*staging);
      return ok;
   }

   // ---- locks -----------------------------------------------------------------

   // Set while a texture lock is forwarded to the runtime: the Windows runtime
   // locks the level's surface inside, and that nested call needs no lookup.
   thread_local bool t_in_texture_call = false;

   struct TextureCallScope
   {
      TextureCallScope()
      {
         t_in_texture_call = true;
      }
      ~TextureCallScope()
      {
         t_in_texture_call = false;
      }
   };

   // Caller holds g_state.mutex. nullptr = untracked (our staging textures).
   void CountTextureLock(Resource* resource, DWORD flags, bool partial)
   {
      if (!resource || resource->pool != D3DPOOL_MANAGED)
      {
         Count(OTHER_LOCKS);
         return;
      }
      Count(MANAGED_LOCKS);
      if (resource->locks++ >= resource->subresources)
      {
         Count(MANAGED_RELOCKS);
      }
      if (flags & D3DLOCK_READONLY)
      {
         Count(MANAGED_READONLY);
      }
      if (partial)
      {
         Count(MANAGED_PARTIAL);
      }
   }

   void FillLocked(const Shadow& shadow, const Shadow::Level& level,
      const RECT* rect, D3DLOCKED_RECT* locked)
   {
      size_t offset = 0;
      if (rect)
      {
         const FormatInfo info = GetFormatInfo(shadow.format);
         offset = (info.block ? size_t(rect->top / 4) * level.size.pitch +
                                   size_t(rect->left / 4) * info.bytes
                              : size_t(rect->top) * level.size.pitch +
                                   size_t(rect->left) * info.bytes);
      }
      locked->pBits = level.bits + offset;
      locked->Pitch = int(level.size.pitch);
   }

   // Counts a texture lock (count_lock) and, for a replaced texture, serves it
   // from the shadow. S_FALSE = not replaced: the caller forwards it.
   HRESULT LockSubresource(void* texture, UINT face, UINT level,
      D3DLOCKED_RECT* locked, const RECT* rect, DWORD flags,
      bool count_lock)
   {
      static const DWORD granularity = []
      {
         SYSTEM_INFO info;
         GetSystemInfo(&info);
         return info.dwAllocationGranularity;
      }();
      Shadow* shadow = nullptr;
      Shadow::Level* target = nullptr;
      {
         std::lock_guard lock(g_state.mutex);
         Resource* resource = FindResource(texture);
         if (count_lock)
         {
            CountTextureLock(resource, flags, rect != nullptr);
         }
         shadow = (resource ? resource->shadow.get() : nullptr);
         if (!shadow)
            return S_FALSE;
         target = shadow->Find(face, level);
         if (!target || !locked || target->locked)
         {
            Count(LOCK_FAILURES);
            return D3DERR_INVALIDCALL;
         }
         target->locked = true;
      }
      // Map without holding the global lock; the level is ours until Unlock.
      const size_t aligned = target->offset - target->offset % granularity;
      char* view = MapTopDown(shadow->section, aligned,
         target->offset - aligned + target->size.bytes);
      std::lock_guard lock(g_state.mutex);
      if (!view)
      {
         target->locked = false;
         Count(LOCK_FAILURES);
         return D3DERR_INVALIDCALL;
      }
      target->Map(view, target->offset - aligned);
      if (!(flags & D3DLOCK_READONLY))
      {
         target->writable = true;
      }
      FillLocked(*shadow, *target, rect, locked);
      return S_OK;
   }

   // Uploads the level if the lock wrote it, then unmaps it and unlocks.
   // S_FALSE = not replaced.
   HRESULT UnlockSubresource(IDirect3DBaseTexture9* texture, UINT face,
      UINT level)
   {
      Shadow* shadow = nullptr;
      Shadow::Level* target = nullptr;
      bool upload = false;
      {
         std::lock_guard lock(g_state.mutex);
         Resource* resource = FindResource(texture);
         shadow = (resource ? resource->shadow.get() : nullptr);
         if (!shadow)
            return S_FALSE;
         target = shadow->Find(face, level);
         if (!target || !target->locked)
            return D3DERR_INVALIDCALL;
         upload = target->writable;
         target->writable = false;
      }
      bool retry = false;
      if (upload)
      {
         if (Upload(shadow->device, target->surface, shadow->format, target->size,
                target->bits))
         {
            Count(UPLOADS);
            Count(UPLOAD_BYTES, target->size.bytes);
            if (level == 0 && shadow->autogen)
            {
               texture->GenerateMipSubLevels();
               Count(AUTOGEN);
            }
         }
         else
         {
            Count(UPLOAD_FAILURES);
            retry = true; // the next unlock of this level tries again
         }
      }
      char* view = nullptr;
      {
         std::lock_guard lock(g_state.mutex);
         if (retry)
         {
            target->writable = true;
         }
         view = target->Unmap();
         target->locked = false;
      }
      UnmapViewOfFile(view);
      return S_OK;
   }

   // The replaced level a surface is, checked against the surface's container: an
   // entry can outlive its texture when the last reference went through a level
   // surface's Release, and a new surface can then get its address.
   std::optional<SurfaceRef> ReplacedSurface(IDirect3DSurface9* surface)
   {
      std::optional<SurfaceRef> ref;
      {
         std::lock_guard lock(g_state.mutex);
         if (auto it = g_state.surfaces.find(surface);
            it != g_state.surfaces.end())
         {
            ref = it->second;
         }
      }
      if (!ref)
         return std::nullopt;
      IDirect3DBaseTexture9* container = nullptr;
      surface->GetContainer(IID_IDirect3DBaseTexture9, (void**)&container);
      if (container)
      {
         container->Release(); // still referenced by the surface
      }
      if (container == ref->texture)
         return ref;
      Count(STALE_SURFACES);
      std::lock_guard lock(g_state.mutex);
      if (auto it = g_state.surfaces.find(surface);
         it != g_state.surfaces.end() && it->second.texture == ref->texture)
      {
         g_state.surfaces.erase(it);
      }
      return std::nullopt;
   }

   HRESULT STDMETHODCALLTYPE HookSurfaceGetDesc(IDirect3DSurface9* self,
      D3DSURFACE_DESC* desc)
   {
      const HRESULT hr = Original<decltype(&HookSurfaceGetDesc)>(
         self, SLOT_SURFACE_GET_DESC)(self, desc);
      if (SUCCEEDED(hr) && !CalledByRuntime(_ReturnAddress()))
      {
         std::lock_guard lock(g_state.mutex);
         if (g_state.surfaces.contains(self))
         {
            desc->Pool = D3DPOOL_MANAGED; // as the game created it (D3DX then locks)
         }
      }
      return hr;
   }

   HRESULT STDMETHODCALLTYPE HookSurfaceLockRect(IDirect3DSurface9* self,
      D3DLOCKED_RECT* locked,
      const RECT* rect, DWORD flags)
   {
      auto original =
         Original<decltype(&HookSurfaceLockRect)>(self, SLOT_SURFACE_LOCK_RECT);
      if (t_in_texture_call)
         return original(self, locked, rect, flags);
      const std::optional<SurfaceRef> ref = ReplacedSurface(self);
      if (D3DSURFACE_DESC desc;
         ref || (SUCCEEDED(Original<decltype(&HookSurfaceGetDesc)>(
                    self, SLOT_SURFACE_GET_DESC)(self, &desc)) &&
                   desc.Pool == D3DPOOL_MANAGED))
      {
         Count(SURFACE_MANAGED);
         if (flags & D3DLOCK_READONLY)
         {
            Count(SURFACE_READONLY);
         }
      }
      if (!ref)
         return original(self, locked, rect, flags);
      Count(SURFACE_SERVED);
      return LockSubresource(ref->texture, ref->face, ref->level, locked, rect,
         flags, false);
   }

   HRESULT STDMETHODCALLTYPE HookSurfaceUnlockRect(IDirect3DSurface9* self)
   {
      auto original = Original<decltype(&HookSurfaceUnlockRect)>(
         self, SLOT_SURFACE_UNLOCK_RECT);
      if (t_in_texture_call)
         return original(self);
      if (std::optional<SurfaceRef> ref = ReplacedSurface(self))
         return UnlockSubresource((IDirect3DBaseTexture9*)ref->texture, ref->face,
            ref->level);
      return original(self);
   }

   HRESULT STDMETHODCALLTYPE HookTextureLockRect(IDirect3DTexture9* self,
      UINT level,
      D3DLOCKED_RECT* locked,
      const RECT* rect, DWORD flags)
   {
      if (const HRESULT hr =
             LockSubresource(self, 0, level, locked, rect, flags, true);
         hr != S_FALSE)
         return hr;
      TextureCallScope scope;
      return Original<decltype(&HookTextureLockRect)>(self, SLOT_LOCK)(
         self, level, locked, rect, flags);
   }

   HRESULT STDMETHODCALLTYPE HookTextureUnlockRect(IDirect3DTexture9* self,
      UINT level)
   {
      if (const HRESULT hr = UnlockSubresource(self, 0, level); hr != S_FALSE)
         return hr;
      TextureCallScope scope;
      return Original<decltype(&HookTextureUnlockRect)>(self, SLOT_UNLOCK)(self,
         level);
   }

   HRESULT STDMETHODCALLTYPE HookCubeLockRect(IDirect3DCubeTexture9* self,
      D3DCUBEMAP_FACES face, UINT level,
      D3DLOCKED_RECT* locked,
      const RECT* rect, DWORD flags)
   {
      if (const HRESULT hr =
             LockSubresource(self, UINT(face), level, locked, rect, flags, true);
         hr != S_FALSE)
         return hr;
      TextureCallScope scope;
      return Original<decltype(&HookCubeLockRect)>(self, SLOT_LOCK)(
         self, face, level, locked, rect, flags);
   }

   HRESULT STDMETHODCALLTYPE HookCubeUnlockRect(IDirect3DCubeTexture9* self,
      D3DCUBEMAP_FACES face,
      UINT level)
   {
      if (const HRESULT hr = UnlockSubresource(self, UINT(face), level);
         hr != S_FALSE)
         return hr;
      TextureCallScope scope;
      return Original<decltype(&HookCubeUnlockRect)>(self, SLOT_UNLOCK)(self, face,
         level);
   }

   HRESULT STDMETHODCALLTYPE HookVolumeLockBox(IDirect3DVolumeTexture9* self,
      UINT level, D3DLOCKED_BOX* locked,
      const D3DBOX* box, DWORD flags)
   {
      {
         std::lock_guard lock(g_state.mutex);
         CountTextureLock(FindResource(self), flags, box != nullptr);
      }
      return Original<decltype(&HookVolumeLockBox)>(self, SLOT_LOCK)(
         self, level, locked, box, flags);
   }

   // Replaced textures still report MANAGED, as the game created them. Same slot
   // and signature for 2D and cube textures.
   HRESULT STDMETHODCALLTYPE HookGetLevelDesc(IDirect3DBaseTexture9* self,
      UINT level, D3DSURFACE_DESC* desc)
   {
      const HRESULT hr = Original<decltype(&HookGetLevelDesc)>(
         self, SLOT_GET_LEVEL_DESC)(self, level, desc);
      if (SUCCEEDED(hr) && !CalledByRuntime(_ReturnAddress()))
      {
         std::lock_guard lock(g_state.mutex);
         if (Resource* resource = FindResource(self); resource && resource->shadow)
         {
            desc->Pool = D3DPOOL_MANAGED;
         }
      }
      return hr;
   }

   // Counted only: SetLOD caps the mips of a MANAGED texture and is ignored on
   // DEFAULT, so a game that relies on it would see full detail.
   DWORD STDMETHODCALLTYPE HookSetLOD(IDirect3DBaseTexture9* self, DWORD lod)
   {
      {
         std::lock_guard lock(g_state.mutex);
         if (Resource* resource = FindResource(self); resource && resource->shadow)
         {
            Count(SET_LOD_REPLACED);
         }
      }
      return Original<decltype(&HookSetLOD)>(self, SLOT_SET_LOD)(self, lod);
   }

   // Takes the reference GetSurfaceLevel/GetCubeMapSurface returned.
   void PatchSurfaces(IDirect3DSurface9* surface)
   {
      if (!surface)
         return;
      Patch(surface, SLOT_SURFACE_LOCK_RECT, (void*)&HookSurfaceLockRect);
      Patch(surface, SLOT_SURFACE_UNLOCK_RECT, (void*)&HookSurfaceUnlockRect);
      if (g_fix_active)
      {
         Patch(surface, SLOT_SURFACE_GET_DESC, (void*)&HookSurfaceGetDesc);
      }
      surface->Release();
   }

   // Methods shared by 2D and cube textures.
   void PatchBaseTexture(IDirect3DBaseTexture9* texture)
   {
      Patch(texture, SLOT_GET_LEVEL_DESC, (void*)&HookGetLevelDesc);
      if (g_fix_active)
      {
         Patch(texture, SLOT_SET_LOD, (void*)&HookSetLOD);
      }
   }

   // ---- device hooks ----------------------------------------------------------

   HRESULT STDMETHODCALLTYPE HookCreateTexture(IDirect3DDevice9* self, UINT width,
      UINT height, UINT levels,
      DWORD usage, D3DFORMAT format,
      D3DPOOL pool,
      IDirect3DTexture9** texture,
      HANDLE* shared)
   {
      const bool replace = g_fix_active && pool == D3DPOOL_MANAGED;
      auto create = Original<CreateTextureFn>(self, SLOT_CREATE_TEXTURE);
      HRESULT hr = create(self, width, height, levels, usage, format,
         replace ? D3DPOOL_DEFAULT : pool, texture, shared);
      if (FAILED(hr))
         return CreateFailed(hr, "Texture", width, height, format, pool, replace,
            usage);
      std::unique_ptr<Shadow> shadow;
      if (replace)
      {
         shadow = CreateShadow(*texture, self, format, width, height, 1, usage);
         if (!shadow)
         {
            // No section: a DYNAMIC texture the runtime locks itself (MANAGED isn't
            // allowed on D3D9Ex).
            (*texture)->Release();
            hr = create(self, width, height, levels, usage | D3DUSAGE_DYNAMIC, format,
               D3DPOOL_DEFAULT, texture, shared);
            if (FAILED(hr))
               return CreateFailed(hr, "Texture", width, height, format, pool, replace,
                  usage);
            g_totals.shadow_fallbacks++;
         }
      }
      IDirect3DTexture9* tex = *texture;
      PatchBaseTexture(tex);
      Patch(tex, SLOT_LOCK, (void*)&HookTextureLockRect);
      Patch(tex, SLOT_UNLOCK, (void*)&HookTextureUnlockRect);
      IDirect3DSurface9* surface = nullptr;
      tex->GetSurfaceLevel(0, &surface);
      PatchSurfaces(surface);
      const UINT actual_levels = tex->GetLevelCount();
      Track(tex, {.buffer = false,
                    .pool = pool,
                    .usage = usage,
                    .bytes = ChainBytes(format, width, height, 1, actual_levels),
                    .subresources = actual_levels,
                    .shadow = std::move(shadow)});
      return hr;
   }

   HRESULT STDMETHODCALLTYPE HookCreateVolumeTexture(
      IDirect3DDevice9* self, UINT width, UINT height, UINT depth, UINT levels,
      DWORD usage, D3DFORMAT format, D3DPOOL pool,
      IDirect3DVolumeTexture9** texture, HANDLE* shared)
   {
      const bool replace = g_fix_active && pool == D3DPOOL_MANAGED;
      const HRESULT hr = Original<decltype(&HookCreateVolumeTexture)>(
         self, SLOT_CREATE_VOLUME_TEXTURE)(
         self, width, height, depth, levels,
         replace ? (usage | D3DUSAGE_DYNAMIC) : usage, format,
         replace ? D3DPOOL_DEFAULT : pool, texture, shared);
      if (FAILED(hr))
         return CreateFailed(hr, "VolumeTexture", width, height, format, pool,
            replace, usage);
      if (replace)
      {
         g_totals.converted_volumes++;
      }
      IDirect3DVolumeTexture9* tex = *texture;
      Patch(tex, SLOT_LOCK, (void*)&HookVolumeLockBox);
      const UINT actual_levels = tex->GetLevelCount();
      Track(tex, {.buffer = false,
                    .pool = pool,
                    .usage = usage,
                    .bytes = ChainBytes(format, width, height, depth, actual_levels),
                    .subresources = actual_levels});
      return hr;
   }

   HRESULT STDMETHODCALLTYPE HookCreateCubeTexture(IDirect3DDevice9* self,
      UINT edge, UINT levels,
      DWORD usage, D3DFORMAT format,
      D3DPOOL pool,
      IDirect3DCubeTexture9** texture,
      HANDLE* shared)
   {
      const bool replace = g_fix_active && pool == D3DPOOL_MANAGED;
      auto create = Original<CreateCubeTextureFn>(self, SLOT_CREATE_CUBE_TEXTURE);
      HRESULT hr = create(self, edge, levels, usage, format,
         replace ? D3DPOOL_DEFAULT : pool, texture, shared);
      if (FAILED(hr))
         return CreateFailed(hr, "CubeTexture", edge, edge, format, pool, replace,
            usage);
      std::unique_ptr<Shadow> shadow;
      if (replace)
      {
         shadow = CreateShadow(*texture, self, format, edge, edge, 6, usage);
         if (!shadow)
         {
            // As for 2D textures: fall back to a DYNAMIC texture.
            (*texture)->Release();
            hr = create(self, edge, levels, usage | D3DUSAGE_DYNAMIC, format,
               D3DPOOL_DEFAULT, texture, shared);
            if (FAILED(hr))
               return CreateFailed(hr, "CubeTexture", edge, edge, format, pool,
                  replace, usage);
            g_totals.shadow_fallbacks++;
         }
      }
      IDirect3DCubeTexture9* tex = *texture;
      PatchBaseTexture(tex);
      Patch(tex, SLOT_LOCK, (void*)&HookCubeLockRect);
      Patch(tex, SLOT_UNLOCK, (void*)&HookCubeUnlockRect);
      IDirect3DSurface9* surface = nullptr;
      tex->GetCubeMapSurface(D3DCUBEMAP_FACE_POSITIVE_X, 0, &surface);
      PatchSurfaces(surface);
      const UINT actual_levels = tex->GetLevelCount();
      Track(tex, {.buffer = false,
                    .pool = pool,
                    .usage = usage,
                    .bytes = ChainBytes(format, edge, edge, 1, actual_levels) * 6,
                    .subresources = actual_levels * 6,
                    .shadow = std::move(shadow)});
      return hr;
   }

   HRESULT STDMETHODCALLTYPE HookCreateVertexBuffer(
      IDirect3DDevice9* self, UINT length, DWORD usage, DWORD fvf, D3DPOOL pool,
      IDirect3DVertexBuffer9** buffer, HANDLE* shared)
   {
      const bool replace = g_fix_active && pool == D3DPOOL_MANAGED;
      const HRESULT hr = Original<decltype(&HookCreateVertexBuffer)>(
         self, SLOT_CREATE_VERTEX_BUFFER)(self, length, usage, fvf,
         replace ? D3DPOOL_DEFAULT : pool, buffer,
         shared);
      if (FAILED(hr))
         return CreateFailed(hr, "VertexBuffer", length, 0, fvf, pool, replace,
            usage);
      if (replace)
      {
         g_totals.converted_buffers++;
      }
      Track(*buffer, {.buffer = true,
                        .pool = pool,
                        .usage = usage,
                        .bytes = length,
                        .subresources = 1});
      return hr;
   }

   HRESULT STDMETHODCALLTYPE HookCreateIndexBuffer(IDirect3DDevice9* self,
      UINT length, DWORD usage,
      D3DFORMAT format, D3DPOOL pool,
      IDirect3DIndexBuffer9** buffer,
      HANDLE* shared)
   {
      const bool replace = g_fix_active && pool == D3DPOOL_MANAGED;
      const HRESULT hr = Original<decltype(&HookCreateIndexBuffer)>(
         self, SLOT_CREATE_INDEX_BUFFER)(self, length, usage, format,
         replace ? D3DPOOL_DEFAULT : pool, buffer,
         shared);
      if (FAILED(hr))
         return CreateFailed(hr, "IndexBuffer", length, 0, format, pool, replace,
            usage);
      if (replace)
      {
         g_totals.converted_buffers++;
      }
      Track(*buffer, {.buffer = true,
                        .pool = pool,
                        .usage = usage,
                        .bytes = length,
                        .subresources = 1});
      return hr;
   }

   // Counts game uploads into a replaced texture (its shadow doesn't see them).
   // The destination is either a level surface or the texture itself.
   void CountUpdateInto(void* destination)
   {
      std::lock_guard lock(g_state.mutex);
      const Resource* resource = FindResource(destination);
      if (g_state.surfaces.contains(destination) ||
          (resource && resource->shadow))
      {
         Count(UPDATES_INTO_REPLACED);
      }
   }

   HRESULT STDMETHODCALLTYPE HookUpdateSurface(IDirect3DDevice9* self,
      IDirect3DSurface9* source,
      const RECT* rect,
      IDirect3DSurface9* destination,
      const POINT* point)
   {
      CountUpdateInto(destination);
      return Original<UpdateSurfaceFn>(self, SLOT_UPDATE_SURFACE)(
         self, source, rect, destination, point);
   }

   HRESULT STDMETHODCALLTYPE
   HookUpdateTexture(IDirect3DDevice9* self, IDirect3DBaseTexture9* source,
      IDirect3DBaseTexture9* destination)
   {
      CountUpdateInto(destination);
      return Original<decltype(&HookUpdateTexture)>(self, SLOT_UPDATE_TEXTURE)(
         self, source, destination);
   }

   // Every pooled staging texture holds a device reference, so the game's last
   // Release would otherwise leave the device (and the pool) alive forever. After
   // the flush the device is gone (verified on Windows d3d9; dgVoodoo's staging
   // textures are assumed to reference the device the same way).
   ULONG STDMETHODCALLTYPE HookDeviceRelease(IDirect3DDevice9* self)
   {
      const ULONG refs =
         Original<decltype(&HookDeviceRelease)>(self, SLOT_RELEASE)(self);
      if (refs == 0 || refs != g_staging_pool.Size())
         return refs;
      fprintf(g_log, "device released by the game, flushing %lu staging textures\n",
         refs);
      g_staging_pool.Flush();
      Report("device released");
      return 0;
   }

   HRESULT STDMETHODCALLTYPE HookReset(IDirect3DDevice9* self,
      D3DPRESENT_PARAMETERS* params)
   {
      g_totals.resets++;
      Report("before Reset");
      const HRESULT hr =
         Original<decltype(&HookReset)>(self, SLOT_RESET)(self, params);
      fprintf(g_log, "Reset -> 0x%08lX\n", hr);
      fflush(g_log);
      return hr;
   }

   // Whether the runtime takes a SYSTEMMEM texture over our memory (D3D9Ex user
   // memory; Windows d3d9 does, dgVoodoo doesn't). A property of the runtime, so
   // it is asked once, before the device's CreateTexture is hooked.
   bool AcceptsUserMemory(IDirect3DDevice9* device)
   {
      void* memory =
         VirtualAlloc(nullptr, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
      HANDLE user_memory = memory;
      IDirect3DTexture9* texture = nullptr;
      auto create = Original<CreateTextureFn>(device, SLOT_CREATE_TEXTURE);
      if (!create)
      {
         create = (CreateTextureFn)(*(void***)device)[SLOT_CREATE_TEXTURE];
      }
      const bool accepted =
         memory && SUCCEEDED(create(device, 1, 1, 1, 0, D3DFMT_A8R8G8B8,
                      D3DPOOL_SYSTEMMEM, &texture, &user_memory));
      if (texture)
      {
         texture->Release();
      }
      if (memory)
      {
         VirtualFree(memory, 0, MEM_RELEASE);
      }
      return accepted;
   }

   HRESULT STDMETHODCALLTYPE HookCreateDevice(IDirect3D9* self, UINT adapter,
      D3DDEVTYPE type, HWND window,
      DWORD behavior,
      D3DPRESENT_PARAMETERS* params,
      IDirect3DDevice9** device)
   {
      // CSMT runs only on dgVoodoo: on a native runtime NVIDIA's D3D9 driver hung
      // when work recorded on the worker was waited for from the game thread (a
      // query's GetData, a managed texture's UnlockRect), with or without
      // MULTITHREADED, and the driver already threads its own work there. Under
      // MULTITHREADED dgVoodoo takes its own critical section in every call; with
      // CSMT only one thread calls it at a time, so the flag only costs.
      const bool csmt = g_csmt_requested && ChainIsDgVoodoo();
      if (g_strip_multithreaded || csmt)
      {
         behavior &= ~D3DCREATE_MULTITHREADED;
      }
      HRESULT hr = E_FAIL;
      bool ex_device = false;
      if (IDirect3D9Ex* ex = nullptr;
         g_fix_requested && params &&
         SUCCEEDED(self->QueryInterface(IID_IDirect3D9Ex, (void**)&ex)))
      {
         D3DDISPLAYMODEEX mode = {.Size = sizeof(mode),
            .Width = params->BackBufferWidth,
            .Height = params->BackBufferHeight,
            .RefreshRate = params->FullScreen_RefreshRateInHz,
            .Format = params->BackBufferFormat,
            .ScanLineOrdering =
               D3DSCANLINEORDERING_PROGRESSIVE};
         hr = ex->CreateDeviceEx(adapter, type, window, behavior, params,
            params->Windowed ? nullptr : &mode,
            (IDirect3DDevice9Ex**)device);
         ex->Release();
         ex_device = SUCCEEDED(hr);
         fprintf(g_log, "VA fix: CreateDeviceEx -> 0x%08lX%s\n", hr,
            ex_device ? "" : ", falling back to CreateDevice (fix off)");
      }
      if (!ex_device)
      {
         hr = Original<decltype(&HookCreateDevice)>(self, SLOT_CREATE_DEVICE)(
            self, adapter, type, window, behavior, params, device);
      }
      fprintf(g_log, "CreateDevice behavior 0x%08lX %ux%u windowed %d -> 0x%08lX\n",
         behavior, params ? params->BackBufferWidth : 0,
         params ? params->BackBufferHeight : 0, params ? params->Windowed : -1,
         hr);
      if (FAILED(hr))
      {
         fflush(g_log);
         return hr;
      }
      g_fix_active = ex_device;
      IDirect3DDevice9* dev = *device;
      g_available_texture_mb = UINT(dev->GetAvailableTextureMem() / MB);
      fprintf(g_log, "available texture memory %u MB\n", g_available_texture_mb);
      if (g_fix_active)
      {
         g_user_memory = AcceptsUserMemory(dev);
         if (g_user_memory)
         {
            g_staging_arena.Reserve(); // while the ceiling is still empty
         }
         fprintf(g_log, "VA fix: user-memory staging %s, staging arena %p\n",
            g_user_memory ? "yes" : "no (runtime-allocated staging)",
            g_staging_arena.Base());
         Patch(dev, SLOT_RELEASE, (void*)&HookDeviceRelease);
      }
      fflush(g_log);
      Patch(dev, SLOT_RESET, (void*)&HookReset);
      Patch(dev, SLOT_CREATE_TEXTURE, (void*)&HookCreateTexture);
      Patch(dev, SLOT_CREATE_VOLUME_TEXTURE, (void*)&HookCreateVolumeTexture);
      Patch(dev, SLOT_CREATE_CUBE_TEXTURE, (void*)&HookCreateCubeTexture);
      Patch(dev, SLOT_CREATE_VERTEX_BUFFER, (void*)&HookCreateVertexBuffer);
      Patch(dev, SLOT_CREATE_INDEX_BUFFER, (void*)&HookCreateIndexBuffer);
      Patch(dev, SLOT_UPDATE_SURFACE, (void*)&HookUpdateSurface);
      Patch(dev, SLOT_UPDATE_TEXTURE, (void*)&HookUpdateTexture);
      if (csmt)
      {
         Csmt::Start(dev, g_log); // last: it patches above the hooks above
      }
      else if (g_csmt_requested)
      {
         fprintf(g_log, "CSMT: requested, but the runtime isn't dgVoodoo: off\n");
      }
      fflush(g_log);
      return hr;
   }

   DWORD WINAPI ReportThread(void*)
   {
      char interval[16] = {};
      const DWORD interval_ms =
         (GetEnvironmentVariableA("D3D9_MEMLOG_MS", interval, sizeof(interval))
               ? atoi(interval)
               : 5000);
      for (;;)
      {
         Sleep(interval_ms);
         Report("tick");
         Csmt::Report(g_log, interval_ms / 1000.0);
         fflush(g_log);
      }
   }

} // namespace

extern "C" IDirect3D9* WINAPI ProxyDirect3DCreate9(UINT sdk_version)
{
   HMODULE chain = Chain();
   IDirect3D9* d3d = nullptr;
   // VA fix: an IDirect3D9Ex, so CreateDevice can make a D3D9Ex device.
   using CreateEx = HRESULT(WINAPI*)(UINT, IDirect3D9Ex**);
   if (auto create_ex =
          ((chain && g_fix_requested)
                ? (CreateEx)GetProcAddress(chain, "Direct3DCreate9Ex")
                : nullptr))
   {
      IDirect3D9Ex* ex = nullptr;
      const HRESULT hr = create_ex(sdk_version, &ex);
      fprintf(g_log, "VA fix: Direct3DCreate9Ex -> 0x%08lX\n", hr);
      d3d = (SUCCEEDED(hr) ? ex : nullptr);
   }
   using Create = IDirect3D9*(WINAPI*)(UINT);
   if (auto create =
          ((!d3d && chain) ? (Create)GetProcAddress(chain, "Direct3DCreate9")
                           : nullptr))
   {
      d3d = create(sdk_version);
   }
   fprintf(g_log, "Direct3DCreate9(%u) -> %p\n", sdk_version, d3d);
   fflush(g_log);
   if (d3d)
   {
      Patch(d3d, SLOT_CREATE_DEVICE, (void*)&HookCreateDevice);
      if (g_log_enabled)
      {
         static const HANDLE thread =
            CreateThread(nullptr, 0, ReportThread, nullptr, 0, nullptr);
      }
   }
   return d3d;
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, void*)
{
   if (reason == DLL_PROCESS_ATTACH)
   {
      DisableThreadLibraryCalls(instance);
      GetModuleFileNameW(instance, g_dir, MAX_PATH);
      *(wcsrchr(g_dir, L'\\') + 1) = L'\0';
      wchar_t log_flag_path[MAX_PATH];
      swprintf_s(log_flag_path, L"%sd3d9_memlog.on", g_dir);
      g_log_enabled = GetFileAttributesW(log_flag_path) != INVALID_FILE_ATTRIBUTES;
      wchar_t log_path[MAX_PATH];
      swprintf_s(log_path, L"%sd3d9_memlog.log", g_dir);
      g_log = _wfopen((g_log_enabled ? log_path : L"NUL"), (g_log_enabled ? L"a" : L"w"));
      if (!g_log)
         return FALSE;
      wchar_t flag_path[MAX_PATH];
      swprintf_s(flag_path, L"%sd3d9_vafix.on", g_dir);
      g_fix_requested = GetFileAttributesW(flag_path) != INVALID_FILE_ATTRIBUTES;
      swprintf_s(flag_path, L"%sd3d9_nomt.on", g_dir);
      g_strip_multithreaded = GetFileAttributesW(flag_path) != INVALID_FILE_ATTRIBUTES;
      swprintf_s(flag_path, L"%sd3d9_csmt.on", g_dir);
      g_csmt_requested = GetFileAttributesW(flag_path) != INVALID_FILE_ATTRIBUTES;
      fprintf(g_log, "---- d3d9_memlog attached, pid %lu, VA fix %s, multithreaded flag %s, CSMT %s\n",
         GetCurrentProcessId(), g_fix_requested ? "requested" : "off",
         g_strip_multithreaded ? "stripped" : (g_csmt_requested ? "stripped with CSMT" : "kept"),
         g_csmt_requested ? "requested" : "off");
      fflush(g_log);
   }
   return TRUE;
}
