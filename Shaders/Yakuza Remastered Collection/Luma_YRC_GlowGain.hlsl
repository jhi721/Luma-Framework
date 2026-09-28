// Luma Bloom's glow_pass0: runs at its draw, with its b5, b11, s0 and scene (t0) still bound, from the prefilter (t1, one
// texel per pixel) into a second 1024x512 target. Vanilla pass0 wrote rgb = glow x cb5[2].rgb (+ the scene term) and
// alpha = its luma x cb5[2].w (+ the scene term's luma), each saturated per texel; glow_pass2 weights them by its
// cb5[0].yzw and .x, 2.0 in every log (Y3R/Y5R), so their sum blurs as one color and the composite applies one weight.
// The scene term keeps vanilla's footprint, one 512x256 level texel (RMS: a smaller window loses energy on small lights),
// sampled at this target's density. LumaData.CustomData1 != 0 = Y5R's material curve.
// Vanilla saturates its level-0 texel, which is already blurred (the 2:1 downsample; Y5R also re-expands it), so a small
// bright light stays under 1 there and keeps the cb5[2] tint in its halo. Saturating this sharper texel instead clips the
// light to white first. So the clamp ratio comes from a Gaussian of the prefilter at the level-0 blur (LumaData.CustomData3
// = its sigma in prefilter texels, from 2x2 bilinear boxes at s2) and scales the sharp texel: the same energy per level
// texel, and vanilla's tint.
#include "Includes/GlowPass0.hlsl"

SamplerState linearSampler : register(s2);

float4 glow_gain_ps(float4 pos : SV_Position) : SV_Target
{
   const float2 texel = 1.0 / float2(YRC_GLOW_PREFILTER_WIDTH, YRC_GLOW_PREFILTER_HEIGHT);
   const float2 uv = pos.xy * texel;
   const float sigma = LumaData.CustomData3;
   const int radius = int(ceil(sigma * 1.5)); // 3 sigma in steps of 2 texels
   float3 blurred = 0.0;
   float weight_sum = 0.0;
   for (int y = -radius; y <= radius; y++)
   {
      for (int x = -radius; x <= radius; x++)
      {
         const float2 offset = float2(x, y) * 2.0 + 0.5; // a texel corner: the bilinear tap averages 2x2
         const float weight = exp(-dot(offset, offset) / (2.0 * sigma * sigma));
         blurred += weight * t1.SampleLevel(linearSampler, uv + offset * texel, 0.0).rgb;
         weight_sum += weight;
      }
   }
   blurred /= weight_sum;

   const float3 g = t1.Load(int3(pos.xy, 0)).rgb;
   float4 glow = float4(g, dot(kGlowLumaWeights, g)) * cb5[2];
   float4 level = float4(blurred, dot(kGlowLumaWeights, blurred)) * cb5[2];
   if ((cb11[0].y & 8u) != 0u)
   {
      const float4 scene = GlowSceneThreshold(uv, float2(2.0 * texel.x * 0.2, 0.0), float2(0.0, 2.0 * texel.y * (1.0 / 6.0)), LumaData.CustomData1 != 0u);
      glow += scene;
      level += scene;
   }
   glow *= saturate(level) / max(level, 1e-6);
   return float4(glow.rgb + glow.a, 1.0);
}
