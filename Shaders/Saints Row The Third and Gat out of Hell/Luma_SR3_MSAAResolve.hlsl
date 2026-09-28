// Saints Row: The Third and Gat out of Hell (same grab_scene_color path) - HDR-weighted MSAA resolve, drawn instead of the game's single ResolveSubresource of the FP16
// scene (rl_pc_scene_renderer::grab_scene_color).
//
// The hardware resolve is a plain average of linear HDR samples, before the tonemap: a sample at 16 next to one at 0
// averages to 8, which the tonemap still maps to white, so bright edges look un-antialiased. Each sample is weighted by
// 1 / (1 + max3) before averaging (Karis; max3 rather than a weighted sum after AMD's reversible-tonemapper resolve, so edges
// between two colours keep their hue), which makes bright edges erode instead of expand. The weight must see the displayed
// exposure, which the scene does not have yet: the game's eye adaptation is Tint_color, applied only in the final composite
// (measured 0.20-0.33), so main.cpp binds last frame's copy of the composite's vc4 here. Without it (first frame) the
// buffer reads 0 and the weight falls back to 1, a plain box resolve. Alpha keeps the plain average.

#include "../Includes/Math.hlsl"

Texture2DMS<float4> sceneMS : register(t0);

cbuffer vc4 : register(b4)
{
   float4 Tint_color : packoffset(c1);
}

float4 main(float4 pos : SV_Position) : SV_Target0
{
   uint width, height, sampleCount;
   sceneMS.GetDimensions(width, height, sampleCount);

   float3 weightedColor = 0.0;
   float weightSum = 0.0;
   float alphaSum = 0.0;
   uint finiteCount = 0;
   for (uint i = 0; i < sampleCount; i++)
   {
      const float4 color = sceneMS.Load(int2(pos.xy), i);
      // An all-ones exponent is NaN or Inf: one of either would poison the whole pixel, so the sample is dropped.
      if (any((asuint(color) & 0x7F800000) == 0x7F800000))
         continue;
      const float weight = rcp(1.0 + max3(max(color.rgb, 0.0) * Tint_color.rgb));
      weightedColor += color.rgb * weight;
      weightSum += weight;
      alphaSum += color.a;
      finiteCount++;
   }
   return finiteCount > 0 ? float4(weightedColor / weightSum, alphaSum / finiteCount) : 0.0;
}
