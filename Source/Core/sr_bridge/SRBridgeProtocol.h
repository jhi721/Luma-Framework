#pragma once

#include <cstdint>

// The protocol between a 32-bit game's SR bridge ("SRBridge.h") and the x64 "Luma-Upscaler.exe" next to the game's exe
// (Source/Tools/SR Bridge Helper), which runs DLSS or FSR 3 (both x64 only) on NT-handle shared textures between two shared fences.
// Text lines over the helper's stdin, its stdout and stderr go to "Luma-Upscaler.log" next to the exe.
//
// Start line: "bridge <version> <upscaler> <luid low> <luid high> <render width> <render height> <output width> <output height>
//   <hdr> <inverted depth> <jittered motion vectors> <auto exposure> <dynamic resolution> <motion vector scale x> <motion vector scale y>
//   <DLSS render preset> <a handle per "Resource" (0: none)> <fence in> <fence out>"
// The helper signals "out" at "kReady" once the upscaler exists (it exits if it can't create it).
// Frame line: "<n> <jitter x> <jitter y> <reset> <render width> <render height> <pre exposure> <sharpness> <near> <far>
//   <vertical fov>": the helper waits for "in" n, upscales and signals "out" n (n past "kReady"). EOF ends it.
namespace SRBridgeProtocol
{
   constexpr int kVersion = 2;

   constexpr const wchar_t* kHelperName = L"Luma-Upscaler.exe";
   constexpr const wchar_t* kLogName = L"Luma-Upscaler.log";

   enum Upscaler
   {
      kDlss,
      kFsr,
   };

   // The shared textures, in the order their handles travel. A missing optional one (0) isn't passed to the upscaler.
   enum Resource
   {
      kColor,        // The render resolution color
      kMotion,       // Motion vectors
      kDepth,        // Device depth
      kReactive,     // Optional: reactive / bias mask
      kTransparency, // Optional: FSR's transparency & composition mask
      kExposure,     // Optional: 1x1 exposure
      kOutput,       // The output resolution color (the helper writes it through a UAV)
      kCount
   };

   constexpr uint64_t kReady = 1;
} // namespace SRBridgeProtocol
