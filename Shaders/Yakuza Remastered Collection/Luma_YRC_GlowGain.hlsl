// Luma Bloom's glow_pass0: runs at its draw, with its b5, b11, s0 and scene (t0) still bound, from the prefilter (t1, one
// texel per pixel) into a second 1024x512 target. Vanilla pass0 wrote rgb = glow x cb5[2].rgb (+ the scene term) and
// alpha = its luma x cb5[2].w (+ the scene term's luma), each saturated per texel; glow_pass2 weights them by its
// cb5[0].yzw and .x, 2.0 in every log (Y3R/Y5R), so their sum blurs as one color and the composite applies one weight.
// The scene term keeps vanilla's footprint, one 512x256 level texel (RMS: a smaller window loses energy on small lights),
// sampled at this target's density. LumaData.CustomData1 != 0 = Y5R's material curve.
#include "Includes/GlowPass0.hlsl"

float4 glow_gain_ps(float4 pos : SV_Position) : SV_Target
{
   const float3 g = t1.Load(int3(pos.xy, 0)).rgb;
   float4 glow = float4(g, dot(kGlowLumaWeights, g)) * cb5[2];
   if ((cb11[0].y & 8u) != 0u)
   {
      const float2 texel = 1.0 / float2(YRC_GLOW_PREFILTER_WIDTH, YRC_GLOW_PREFILTER_HEIGHT);
      glow += GlowSceneThreshold(pos.xy * texel, float2(2.0 * texel.x * 0.2, 0.0), float2(0.0, 2.0 * texel.y * (1.0 / 6.0)), LumaData.CustomData1 != 0u);
   }
   glow = saturate(glow);
   return float4(glow.rgb + glow.a, 1.0);
}
