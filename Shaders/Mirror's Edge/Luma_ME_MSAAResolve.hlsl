// Mirror's Edge: HDR-weighted MSAA resolve, drawn instead of the game's ResolveSubresource of the fp16 MSAA scene (three a frame: two
// for the refraction passes, the last for the post chain). Saints Row: The Third's (Luma_SR3_MSAAResolve), with this game's exposure
// and depth.
//
// The hardware resolve is a plain average of linear HDR samples, before the tonemap: a sky sample at 10 next to a building at 0.2
// averages to 5, which the tonemap still clips to white, so silhouettes against the sky look un-antialiased. Each sample is weighted
// by 1 / (1 + max3) of its exposed value (AMD's reversible-tonemapper resolve: max3 keeps the hue of edges between two colours),
// which makes bright edges erode instead of expand. The exposure is TdToneMapping's 1x1 texture (exposure / 64), last frame's: the
// game builds this frame's after the resolve. Unbound (no tonemap yet) it reads 0 and the weight falls back to 1, a box resolve.
//
// Alpha is the linear view depth (sky 65504) the post passes read (TdDirectionalHaze, DOF, bloom's sky exclusion). Averaged it makes
// depths that exist on neither side of a silhouette; the nearest sample is a real one (RESOLVE_AVERAGE_DEPTH 1: the game's average).

#include "../Includes/Math.hlsl"

#ifndef RESOLVE_AVERAGE_DEPTH
#define RESOLVE_AVERAGE_DEPTH 0
#endif

Texture2DMS<float4> sceneMS : register(t0);
Texture2D<float4> exposureTexture : register(t1);

float4 main(float4 pos : SV_Position) : SV_Target0
{
   uint width, height, sampleCount;
   sceneMS.GetDimensions(width, height, sampleCount);
   const float exposure = exposureTexture.Load(int3(0, 0, 0)).x * 64.0;

   float3 weightedColor = 0.0;
   float weightSum = 0.0;
   float depth = RESOLVE_AVERAGE_DEPTH ? 0.0 : FLT16_MAX;
   uint finiteCount = 0;
   for (uint i = 0; i < sampleCount; i++)
   {
      const float4 color = sceneMS.Load(int2(pos.xy), i);
      // An all-ones exponent is NaN or Inf: one of either would poison the whole pixel, so the sample is dropped.
      if (any((asuint(color) & 0x7F800000) == 0x7F800000))
         continue;
      const float weight = rcp(1.0 + max3(max(color.rgb, 0.0)) * exposure);
      weightedColor += color.rgb * weight;
      weightSum += weight;
      depth = RESOLVE_AVERAGE_DEPTH ? (depth + color.a) : min(depth, color.a);
      finiteCount++;
   }
   if (finiteCount == 0)
      return 0.0;
   return float4(weightedColor / weightSum, RESOLVE_AVERAGE_DEPTH ? (depth / finiteCount) : depth);
}
