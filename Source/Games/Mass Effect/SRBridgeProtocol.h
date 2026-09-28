#pragma once

#include <cstdint>

// The addon's side of the x64 "sr_bridge_helper.exe" protocol (see "DrawBridge"), shared with the helper
// (_tools/dlss_x86_bridge, "bridge_common.h" includes this file)
namespace SRBridgeProtocol
{
   // The NT-handle shared textures, in the order their handles travel over the helper's stdin (0: none)
   enum Resource
   {
      kColor,        // R16G16B16A16_FLOAT, the scene's copy
      kMotion,       // R16G16_FLOAT, motion vectors (UV deltas)
      kDepth,        // R32_FLOAT, device depth
      kReactive,     // R8_UNORM, reactive / bias mask
      kMask2,        // R8G8_UNORM, the draws' mask target (the offline test's only)
      kOutput,       // R16G16B16A16_FLOAT, the upscaler's output
      kOutput2,      // the same, the other one of the offline test's pipelined pair
      kTransparency, // R8_UNORM, FSR's transparency & composition mask
      kCount
   };

   // The "bridge" mode's ready signal ("out" before the first frame): the upscaler it runs
   constexpr uint64_t kReadyDlss = 1;
   constexpr uint64_t kReadyFsr = 2;
} // namespace SRBridgeProtocol
