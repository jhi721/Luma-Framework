// Protocol between the d9native add-on and its presenter thread
// (d9present.cpp, same DLL), the game's output device. The presenter opens the
// game's D3D9Ex render targets by KMT handle on its own D3D12 device, composes
// them (compose.hlsl, plus ReShade's D3D9 overlay) into a D3D12 flip swapchain
// on the game's window and presents. NOTES.md "Presenter helper", "P1".
//
// Text lines over an anonymous pipe; the presenter logs to d9present.log next
// to the add-on.
// start <version> <hwnd> <luid low> <luid high> <stats mapping>
//   stats mapping: the PresenterStats file mapping handle (this process).
// textures <scene kmt> <canvas kmt> <overlay kmt> <snapshot kmt>
//   KMT handles of the D3D9 shared render targets (0 = none). Sent after every
//   (re)creation; the old ones are released.
// fences <in NT> <out NT>
//   D3DKMT monitored fences created on the D3D9 kernel device (kmt_sync.h),
//   NT handles duplicated for the presenter (it closes them). Sent once the
//   add-on found the D3D9 queue (a few frames after the start).
// frame <n> <source 0 scene|1 canvas> <mode> <paper white> <decode gamma>
//       <sync interval> <overlay 0|1> <test flags> <follow window 0|1> <width>
//       <height> <latency wait 0|1> <block 0|1> <probe count> <x0> <y0> ..
//       <x3> <y3> <gpu wait 0|1> <colour refresh>
//   gpu wait 1: the D3D9 queue signals "in" n after writing the textures, the
//   presenter's queue waits for it; the presenter's queue signals "out" n after
//   its reads and the D3D9 queue waits for that before writing them again (no
//   CPU wait on either side). gpu wait 0 (E0, before the fences or after a KMT
//   failure): the D3D9 side drained its queue before the line and waits for
//   frames = n. Probe / block frames: the presenter waits for its GPU, writes
//   the stats, then frames = n (the add-on waits for that). width/height: the
//   output size when not following the window's client size, applied when it
//   changes (a size reached by following the window stays). colour refresh: a
//   counter; a change re-applies SetColorSpace1 (display changed). test flags:
//   1 no encode, 2 no BT.2020 matrix (mutations), 8 present clamp of the canvas
//   to max(snapshot, 1), 16 the snapshot holds this frame's ResetRender.
// EOF: the thread exits 0. Any failure: error text in the stats, exit 1. The
// add-on's watchdog signals "out" to UINT64_MAX when the thread ends or hangs,
// so no D3D9 GPU wait outlives it.
#pragma once
#include <cstdint>

constexpr int PRESENTER_VERSION = 2;

struct PresenterStats
{
   double overlay_block_sum; // sum of RGB over the top-left 256x256 of the back
                             // buffer after composition (scrgb, block frames)
   float probe_rgba[4][4];   // back buffer probes: fp16 values (scrgb), code /
                             // 1023 (hdr10), code / 255 (srgb)
   int32_t ready;            // swapchain exists
   int32_t swapchain_hr, color_space_hr, last_present_hr, resize_hr, latency_hr;
   uint32_t width, height; // swapchain size
   uint32_t vp[4];         // letterbox viewport of the last composition (x y w h)
   int32_t mode, format, color_space;
   uint32_t swapchain_flags;
   uint32_t present_ok, present_failed, resizes, swapchain_creates,
      swapchain_releases, recoveries, overlay_block_frames, latency_waits,
      latency_timeouts;
   int32_t latency_flag;
   int32_t shared_desc_ok; // scene texture: one mip, no MSAA, SRV allowed
   // Test faults, added by the add-on and consumed by the presenter.
   volatile long fault_present_invalid, fault_resize, fault_cs_unsupported;
   volatile long no_recovery; // MUT_NO_RECOVERY
   char error[128];           // first failure
   volatile uint64_t frames;  // last frame done (written last)
};
