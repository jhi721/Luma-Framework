// E3 GPU-side D3D9 <-> D3D11 sync for d9native (NOTES.md "Mid-frame sync",
// proven in t9sync.cpp; this is its E3 path made reusable). win32u!NtGdiDdDDI*
// hooks (MinHook) record the kernel devices / contexts / hw queues created
// while the D3D9 device is being created (generation = KmtBeginD3D9Device) and
// count submissions per queue / context; KmtSelect picks the one the D3D9 UMD
// submits its 3D work to. A shared D3D11 fence is opened kernel-side for that
// D3D9 device (only NoGPUAccess works: NOTES.md E3 details), then KmtSignal /
// KmtWait put signal / wait packets into the D3D9 queue (HAGS: hw queue entry
// points; else the context ones). Spike code: NVIDIA + HAGS measured only; any
// failed call drops the caller back to CPU waits (E0).
#pragma once
#include <d3d11_4.h>
#include <d3dkmthk.h>
#include <emmintrin.h>
#include <winternl.h>
#include <map>
#include <mutex>
#include <vector>

#include "../includes/minhook.h"

#pragma comment(lib, "gdi32.lib")

struct KmtObjects
{
   std::mutex lock;
   int generation = 0; // > 0 while a D3D9 device is being created (its creation
                       // number), 0 otherwise
   int last_generation = 0;
   struct Context
   {
      D3DKMT_HANDLE device, handle;
      int generation;
   };
   struct Queue
   {
      D3DKMT_HANDLE hw_context, handle;
      int generation;
   };
   std::vector<Context> contexts;
   std::vector<Queue> queues;
   std::map<D3DKMT_HANDLE, long> context_submits, queue_submits;
   std::map<const void*, int>
      device_generations; // ReShade device -> the generation its kernel objects
                          // were created in
   bool hooked = false;
};
inline KmtObjects g_kmt_objects;

struct KmtSync
{
   bool ready = false, hw_queue = false;
   D3DKMT_HANDLE device = 0, context = 0, queue = 0, sync_object = 0;
   HANDLE fence_handle = nullptr;
   ID3D11Fence* fence = nullptr;
   ID3D11DeviceContext4* context4 = nullptr;
   uint64_t value = 0; // last fence value used
   int generation = 0; // of the D3D9 device the queue belongs to
   NTSTATUS first_error = 0;
   const char* first_error_call = "";
};

namespace kmt_detail
{
   using NtFn = NTSTATUS(NTAPI*)(void*);
   inline NtFn create_context, create_context_virtual, create_hw_queue, submit,
      submit_hw_queue;

   inline void AddContext(D3DKMT_HANDLE device, D3DKMT_HANDLE handle)
   {
      std::lock_guard guard(g_kmt_objects.lock);
      if (g_kmt_objects.generation)
      {
         g_kmt_objects.contexts.push_back(
            {device, handle, g_kmt_objects.generation});
      }
   }
   inline NTSTATUS NTAPI CreateContext(D3DKMT_CREATECONTEXT* args)
   {
      const NTSTATUS status = create_context(args);
      if (status >= 0)
      {
         AddContext(args->hDevice, args->hContext);
      }
      return status;
   }
   inline NTSTATUS NTAPI CreateContextVirtual(D3DKMT_CREATECONTEXTVIRTUAL* args)
   {
      const NTSTATUS status = create_context_virtual(args);
      if (status >= 0)
      {
         AddContext(args->hDevice, args->hContext);
      }
      return status;
   }
   inline NTSTATUS NTAPI CreateHwQueue(D3DKMT_CREATEHWQUEUE* args)
   {
      const NTSTATUS status = create_hw_queue(args);
      if (status >= 0)
      {
         std::lock_guard guard(g_kmt_objects.lock);
         if (g_kmt_objects.generation)
         {
            g_kmt_objects.queues.push_back(
               {args->hHwContext, args->hHwQueue, g_kmt_objects.generation});
         }
      }
      return status;
   }
   inline NTSTATUS NTAPI Submit(D3DKMT_SUBMITCOMMAND* args)
   {
      {
         std::lock_guard guard(g_kmt_objects.lock);
         for (UINT i = 0;
            i < args->BroadcastContextCount && i < D3DDDI_MAX_BROADCAST_CONTEXT;
            ++i)
         {
            ++g_kmt_objects.context_submits[args->BroadcastContext[i]];
         }
      }
      return submit(args);
   }
   inline NTSTATUS NTAPI SubmitHwQueue(D3DKMT_SUBMITCOMMANDTOHWQUEUE* args)
   {
      {
         std::lock_guard guard(g_kmt_objects.lock);
         ++g_kmt_objects.queue_submits[args->hHwQueue];
      }
      return submit_hw_queue(args);
   }

   inline void Fail(KmtSync* sync, const char* call, NTSTATUS status)
   {
      if (status < 0 && !sync->first_error)
      {
         sync->first_error = status;
         sync->first_error_call = call;
      }
   }
} // namespace kmt_detail

// Before the game's D3D9 CreateDevice (ReShade create_device event). Hooks are
// installed once.
inline bool KmtBeginD3D9Device()
{
   using namespace kmt_detail;
   if (!g_kmt_objects.hooked)
   {
      if (!InitializeMinHook())
      {
         return false;
      }
      const struct
      {
         const char* name;
         void* detour;
         NtFn* original;
      } hooks[] = {
         {"NtGdiDdDDICreateContext", reinterpret_cast<void*>(CreateContext),
            &create_context},
         {"NtGdiDdDDICreateContextVirtual",
            reinterpret_cast<void*>(CreateContextVirtual),
            &create_context_virtual},
         {"NtGdiDdDDICreateHwQueue", reinterpret_cast<void*>(CreateHwQueue),
            &create_hw_queue},
         {"NtGdiDdDDISubmitCommand", reinterpret_cast<void*>(Submit), &submit},
         {"NtGdiDdDDISubmitCommandToHwQueue",
            reinterpret_cast<void*>(SubmitHwQueue), &submit_hw_queue}};
      // Only enable the hooks created here: other MinHook owners in this DLL
      // keep theirs (see minhook.h).
      void* targets[std::size(hooks)] = {};
      for (size_t i = 0; i < std::size(hooks); ++i)
      {
         if (MH_CreateHookApiEx(L"win32u", hooks[i].name, hooks[i].detour,
                reinterpret_cast<void**>(hooks[i].original),
                &targets[i]) != MH_OK)
         {
            return false;
         }
      }
      for (void* target : targets)
      {
         if (MH_EnableHook(target) != MH_OK)
         {
            return false;
         }
      }
      g_kmt_objects.hooked = true;
   }
   std::lock_guard guard(g_kmt_objects.lock);
   g_kmt_objects.generation = ++g_kmt_objects.last_generation;
   return true;
}

// After the D3D9 device exists (init_device event): its kernel objects are the
// ones of this generation.
inline void KmtEndD3D9Device(const void* device)
{
   std::lock_guard guard(g_kmt_objects.lock);
   if (g_kmt_objects.generation)
   {
      g_kmt_objects.device_generations[device] = g_kmt_objects.generation;
   }
   g_kmt_objects.generation = 0;
}

// 0 = the device was created without the hooks (no E3 for it).
inline int KmtGeneration(const void* device)
{
   std::lock_guard guard(g_kmt_objects.lock);
   const auto found = g_kmt_objects.device_generations.find(device);
   return found == g_kmt_objects.device_generations.end() ? 0 : found->second;
}

inline void KmtRelease(KmtSync* sync)
{
   if (sync->sync_object)
   {
      const D3DKMT_DESTROYSYNCHRONIZATIONOBJECT destroy = {sync->sync_object};
      D3DKMTDestroySynchronizationObject(&destroy);
   }
   if (sync->fence_handle)
   {
      CloseHandle(sync->fence_handle);
   }
   if (sync->fence)
   {
      sync->fence->Release();
   }
   if (sync->context4)
   {
      sync->context4->Release();
   }
   const NTSTATUS first_error = sync->first_error;
   const char* first_error_call = sync->first_error_call;
   *sync = {};
   sync->first_error = first_error; // kept for the log / test
   sync->first_error_call = first_error_call;
}

// Picks the queue (HAGS) or context of the D3D9 device of `generation` with the
// most submissions (other devices, e.g. a launcher's dummy device, are other
// generations). false = none found.
inline bool KmtSelect(KmtSync* sync, int generation)
{
   using namespace kmt_detail;
   KmtRelease(sync);
   sync->generation = generation;
   {
      std::lock_guard guard(g_kmt_objects.lock);
      long best_queue = 0, best_context = 0;
      for (const auto& queue : g_kmt_objects.queues)
      {
         const auto found = g_kmt_objects.queue_submits.find(queue.handle);
         if (queue.generation == generation &&
             found != g_kmt_objects.queue_submits.end() &&
             found->second > best_queue)
         {
            best_queue = found->second;
            sync->queue = queue.handle;
            for (const auto& context : g_kmt_objects.contexts)
            {
               if (context.handle == queue.hw_context)
               {
                  sync->device = context.device;
               }
            }
         }
      }
      for (const auto& context : g_kmt_objects.contexts)
      {
         const auto found = g_kmt_objects.context_submits.find(context.handle);
         if (context.generation == generation &&
             found != g_kmt_objects.context_submits.end() &&
             found->second > best_context)
         {
            best_context = found->second;
            sync->context = context.handle;
            if (!best_queue)
            {
               sync->device = context.device;
            }
         }
      }
      sync->hw_queue = best_queue > 0;
   }
   if (!sync->device || (!sync->queue && !sync->context))
   {
      Fail(sync, "no D3D9 queue/context with submissions",
         static_cast<NTSTATUS>(0xC0000225));
      return false;
   }
   return true;
}

// KmtSelect, then a shared D3D11 fence opened for that kernel device. false =
// stay on E0 (first_error says why when a call failed).
inline bool KmtSetup(KmtSync* sync, int generation, ID3D11Device* device11,
   ID3D11DeviceContext* context11)
{
   using namespace kmt_detail;
   if (!KmtSelect(sync, generation))
   {
      return false;
   }
   ID3D11Device5* device5 = nullptr;
   HRESULT hr = device11->QueryInterface(IID_PPV_ARGS(&device5));
   if (SUCCEEDED(hr))
   {
      hr = device5->CreateFence(0, D3D11_FENCE_FLAG_SHARED,
         IID_PPV_ARGS(&sync->fence));
      device5->Release();
   }
   if (SUCCEEDED(hr))
   {
      hr = sync->fence->CreateSharedHandle(nullptr, GENERIC_ALL, nullptr,
         &sync->fence_handle);
   }
   if (SUCCEEDED(hr))
   {
      hr = context11->QueryInterface(IID_PPV_ARGS(&sync->context4));
   }
   if (FAILED(hr))
   {
      Fail(sync, "D3D11 fence / ID3D11DeviceContext4", static_cast<NTSTATUS>(hr));
      KmtRelease(sync);
      return false;
   }
   D3DKMT_OPENSYNCOBJECTFROMNTHANDLE2 open = {};
   open.hNtHandle = sync->fence_handle;
   open.hDevice = sync->device;
   open.Flags.NoGPUAccess = 1;
   const NTSTATUS status = D3DKMTOpenSyncObjectFromNtHandle2(&open);
   if (status < 0 || !open.hSyncObject)
   {
      Fail(sync, "D3DKMTOpenSyncObjectFromNtHandle2",
         status < 0 ? status : static_cast<NTSTATUS>(0xC0000001));
      KmtRelease(sync);
      return false;
   }
   sync->sync_object = open.hSyncObject;
   sync->ready = true;
   return true;
}

// GPU signal of `value` on the D3D9 queue, after everything the D3D9 UMD has
// submitted so far (flush it first).
inline bool KmtSignalObject(KmtSync* sync, D3DKMT_HANDLE object,
   uint64_t value)
{
   using namespace kmt_detail;
   NTSTATUS status;
   if (sync->hw_queue)
   {
      const D3DKMT_HANDLE queue = sync->queue;
      D3DKMT_SUBMITSIGNALSYNCOBJECTSTOHWQUEUE args = {};
      args.BroadcastHwQueueCount = 1;
      args.BroadcastHwQueueArray = &queue;
      args.ObjectCount = 1;
      args.ObjectHandleArray = &object;
      args.FenceValueArray = &value;
      status = D3DKMTSubmitSignalSyncObjectsToHwQueue(&args);
      Fail(sync, "D3DKMTSubmitSignalSyncObjectsToHwQueue", status);
   }
   else
   {
      const D3DKMT_HANDLE context = sync->context;
      D3DKMT_SIGNALSYNCHRONIZATIONOBJECTFROMGPU2 args = {};
      args.ObjectCount = 1;
      args.ObjectHandleArray = &object;
      args.BroadcastContextCount = 1;
      args.BroadcastContextArray = &context;
      args.MonitoredFenceValueArray = &value;
      status = D3DKMTSignalSynchronizationObjectFromGpu2(&args);
      Fail(sync, "D3DKMTSignalSynchronizationObjectFromGpu2", status);
   }
   return status >= 0;
}

// GPU wait for `value` on the D3D9 queue: D3D9 work submitted after this call
// runs after the fence reached it.
inline bool KmtWaitObject(KmtSync* sync, D3DKMT_HANDLE object,
   uint64_t value)
{
   using namespace kmt_detail;
   NTSTATUS status;
   if (sync->hw_queue)
   {
      D3DKMT_SUBMITWAITFORSYNCOBJECTSTOHWQUEUE args = {};
      args.hHwQueue = sync->queue;
      args.ObjectCount = 1;
      args.ObjectHandleArray = &object;
      args.FenceValueArray = &value;
      status = D3DKMTSubmitWaitForSyncObjectsToHwQueue(&args);
      Fail(sync, "D3DKMTSubmitWaitForSyncObjectsToHwQueue", status);
   }
   else
   {
      D3DKMT_WAITFORSYNCHRONIZATIONOBJECTFROMGPU args = {};
      args.hContext = sync->context;
      args.ObjectCount = 1;
      args.ObjectHandleArray = &object;
      args.MonitoredFenceValueArray = &value;
      status = D3DKMTWaitForSynchronizationObjectFromGpu(&args);
      Fail(sync, "D3DKMTWaitForSynchronizationObjectFromGpu", status);
   }
   return status >= 0;
}
inline bool KmtSignal(KmtSync* sync, uint64_t value)
{
   return KmtSignalObject(sync, sync->sync_object, value);
}
inline bool KmtWait(KmtSync* sync, uint64_t value)
{
   return KmtWaitObject(sync, sync->sync_object, value);
}

// Monitored fence created on the selected D3D9 kernel device and shared as an
// NT handle (kmt12_probe/bridge_probe: D3D12 opens it with OpenSharedHandle).
// completed = the read-only CPU mapping of its value.
struct KmtFence
{
   D3DKMT_HANDLE object = 0;
   const volatile uint64_t* completed = nullptr;
   HANDLE nt = nullptr;
};

inline bool KmtCreateSharedFence(KmtSync* sync, KmtFence* fence)
{
   D3DKMT_CREATESYNCHRONIZATIONOBJECT2 create = {};
   create.hDevice = sync->device;
   create.Info.Type = D3DDDI_MONITORED_FENCE;
   create.Info.Flags.Shared = 1;
   create.Info.Flags.NtSecuritySharing = 1;
   create.Info.Flags.NoGPUAccess = 1;
   NTSTATUS status = D3DKMTCreateSynchronizationObject2(&create);
   kmt_detail::Fail(sync, "D3DKMTCreateSynchronizationObject2", status);
   if (status < 0)
   {
      return false;
   }
   fence->object = create.hSyncObject;
   fence->completed = static_cast<const volatile uint64_t*>(
      create.Info.MonitoredFence.FenceValueCPUVirtualAddress);
   OBJECT_ATTRIBUTES attributes = {sizeof(OBJECT_ATTRIBUTES)};
   status = D3DKMTShareObjects(1, &fence->object, &attributes, GENERIC_ALL,
      &fence->nt);
   kmt_detail::Fail(sync, "D3DKMTShareObjects", status);
   return status >= 0;
}

// x86: one aligned 64-bit read (the mapping is read-only: no interlocked
// read-modify-write, RenoDX gpu_fence.hpp).
inline uint64_t KmtCompleted(const KmtFence& fence)
{
   uint64_t value;
   _mm_storel_epi64(reinterpret_cast<__m128i*>(&value),
      _mm_loadl_epi64(reinterpret_cast<const __m128i*>(
         const_cast<const uint64_t*>(fence.completed))));
   return value;
}

// CPU signal (any thread): releases GPU waits for values up to `value`.
inline bool KmtCpuSignal(KmtSync* sync, const KmtFence& fence, uint64_t value)
{
   D3DKMT_SIGNALSYNCHRONIZATIONOBJECTFROMCPU signal = {};
   signal.hDevice = sync->device;
   signal.ObjectCount = 1;
   signal.ObjectHandleArray = &fence.object;
   signal.FenceValueArray = &value;
   return D3DKMTSignalSynchronizationObjectFromCpu(&signal) >= 0;
}

inline void KmtDestroyFence(KmtFence* fence)
{
   if (fence->nt)
   {
      CloseHandle(fence->nt);
   }
   if (fence->object)
   {
      const D3DKMT_DESTROYSYNCHRONIZATIONOBJECT destroy = {fence->object};
      D3DKMTDestroySynchronizationObject(&destroy);
   }
   *fence = {};
}
