// SR output hand-off copies (CS).
// copy_color_cs: straight copy through the game's OWN typed UAV. The in-game "Buffer Format" setting
// swaps the whole HDR color chain between rgba16f and r11g11b10_float — including the TAA resolve
// target (u2) and history (u3) our DLSS/FSR output must land in — and a plain CopySubresourceRegion
// across formats silently no-ops (black scene, live UI). The typed store does the conversion, and the
// format is guaranteed writable because the game's native resolve stores through the very same view.
// copy_color_history_cs: history (u3) copy. The native resolve (0xD7E13B2A, the u3 store) keeps its history as
// encoded-domain RGB x512 — enc = (log2(c + 0.008632) - 6.643856)/13.5 + 1, read back as sampled/512. Raw linear
// RGB written there reads back as garbage history on the frames where the native resolve actually runs
// (SR<->native transitions, the dialogue run-native path); its neighborhood clamp squashes that into a
// de-facto history reset (1-2 frames of shimmer), but an honest encode makes the hand-off seamless.
// w carries no current-frame data natively — store 0.
Texture2D<float4> src : register(t0);   // Luma SR output (rgba16f)
RWTexture2D<float4> dst : register(u0); // the game's resolve/history target, bound via its own typed UAV

// The native history encoding: enc = (log2(c + offset) - log2(100)) / range + 1, stored x512
static const float kHistoryLogOffset = 0.008632;
static const float kHistoryLog2Of100 = 6.643856;
static const float kHistoryLogRange = 13.5;
static const float kHistoryScale = 512.0;

// clang-format glues the attribute to the function and indents the second entry point
// clang-format off
[numthreads(8, 8, 1)]
void copy_color_cs(uint2 dtid : SV_DispatchThreadID)
{
   uint w, h;
   dst.GetDimensions(w, h);
   if (dtid.x >= w || dtid.y >= h)
      return;
   dst[dtid] = src.Load(int3(dtid, 0));
}

[numthreads(8, 8, 1)]
void copy_color_history_cs(uint2 dtid : SV_DispatchThreadID)
{
   uint w, h;
   dst.GetDimensions(w, h);
   if (dtid.x >= w || dtid.y >= h)
      return;
   // Not a range clamp: below -kHistoryLogOffset the log2 is NaN (the encode has no sign)
   float3 c = max(src.Load(int3(dtid, 0)).rgb, 0.0);
   float3 enc = (log2(c + kHistoryLogOffset) - kHistoryLog2Of100) / kHistoryLogRange + 1.0;
   dst[dtid] = float4(enc * kHistoryScale, 0.0);
}
// clang-format on
