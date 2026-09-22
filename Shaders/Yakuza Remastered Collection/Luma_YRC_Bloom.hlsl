// Luma Bloom around the shared pyramid (Luma_Bloom_impl.hlsl): the prefilter of the vanilla glow source and the
// composite that replaces glow_pass2 (glow_pass0's gains and scene term: Luma_YRC_GlowGain.hlsl).
#include "Includes/Common.hlsl"

// Runs at the glow downsample (Copy VS) into a 1024x512 target: every texel averages the 4K glow source over its own
// footprint with the vanilla downsample's 5x6 tap pattern, each tap clamped per texel as the 8-bit source was.
// LumaData.CustomData2 != 0 = root mean square (Y4R's downsample), else the mean (Y3R/Y5R's 2-tap average, unaliased).
// Y4R takes the RMS twice (the downsample at its 512x256 texel, then glow_pass0 again on that level): one RMS over 3x this
// texel's footprint matches its energy within 3% for features of 2+ px (modelled offline).
// LumaData.CustomData1 != 0 = Y5R: the source comes from the extended material tone curve (see SampleSaturatedBilinear).
Texture2D<float4> source : register(t0); // the glow source (prefilter) or the Luma bloom with all its mips (composite)
SamplerState linearSampler : register(s0);

float4 glow_prefilter_ps(float4 pos : SV_Position) : SV_Target
{
   const float2 texel = 1.0 / float2(1024.0, 512.0);
   const float2 uv = pos.xy * texel;
   const bool rms = LumaData.CustomData2 != 0u;
   const float2 footprint = texel * (rms ? 3.0 : 1.0);
   float3 sum = 0.0;
   for (int y = -2; y < 4; y++)
   {
      for (int x = -2; x < 3; x++)
      {
         const float3 c = SampleSaturatedBilinear(source, linearSampler, uv + footprint * float2(x * 0.2, y * (1.0 / 6.0)), LumaData.CustomData1 != 0u).rgb;
         sum += rms ? c * c : c;
      }
   }
   sum *= 1.0 / 30.0;
   return float4(rms ? sqrt(sum) : sum, 1.0);
}

// Runs as glow_pass2's pixel shader (swapped by main.cpp), keeping its VS, viewport, cb5 and additive blend onto the scene.
// Vanilla pass2 takes the 5 levels at 0.2 each, rgb * cb5[0].yzw plus alpha * cb5[0].x, saturated; the pyramid holds
// pass0's rgb + alpha (Luma_YRC_GlowGain.hlsl), so one weight, cb5[0].yzw (= .x in every log). DrawBloom accumulates
// a_k = 0.5 * m_k + 0.5 * a_k+1, so the equal-weight sum of its 5 levels is 2 * a0 + a1 + a2 + a3.
// Vanilla levels 1-4 are b8g8r8a8: their faint tails round to 0, so mips 1-3 fade out below 1.5/255 (from 0.75/255),
// which matches the vanilla halo within ~15% where it is faintest (modelled offline).
// LumaData.CustomData3/4 = 1 / the target size.
cbuffer GlowPass2 : register(b5)
{
   float4 cb5[1];
}

float4 bloom_composite_ps(float4 pos : SV_Position) : SV_Target
{
   const float2 uv = pos.xy * float2(LumaData.CustomData3, LumaData.CustomData4);
   float3 s = 2.0 * source.SampleLevel(linearSampler, uv, 0.0).rgb;
   for (int mip = 1; mip < 4; mip++)
   {
      const float3 m = source.SampleLevel(linearSampler, uv, mip).rgb;
      s += m * saturate(m * (2.0 * 255.0 / 1.5) - 1.0);
   }
   s *= 0.2;
   float3 c = saturate(s * cb5[0].yzw);
   c *= LumaSettings.GameSettings.BloomIntensity;
   return float4(c, max3(c));
}
