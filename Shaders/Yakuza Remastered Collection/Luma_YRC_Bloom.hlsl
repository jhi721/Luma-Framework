// Luma Bloom around the shared pyramid (Luma_Bloom_impl.hlsl): the prefilter of the vanilla glow source and the
// composite that replaces glow_pass2.
#include "Includes/Common.hlsl"

// Runs at the glow downsample (Copy VS) into a 1024x512 target: every texel averages the 4K glow source over its own
// footprint with the vanilla downsample's 5x6 tap pattern, each tap clamped per texel as the 8-bit source was.
// LumaData.CustomData2 != 0 = root mean square (Y4R's downsample), else the mean (Y3R/Y5R's 2-tap average, unaliased).
// LumaData.CustomData1 != 0 = Y5R: the source comes from the extended material tone curve (see SampleSaturatedBilinear).
Texture2D<float4> source : register(t0); // the glow source (prefilter) or the Luma bloom with all its mips (composite)
SamplerState linearSampler : register(s0);

float4 glow_prefilter_ps(float4 pos : SV_Position) : SV_Target
{
   const float2 texel = 1.0 / float2(1024.0, 512.0);
   const float2 uv = pos.xy * texel;
   const bool rms = LumaData.CustomData2 != 0u;
   float3 sum = 0.0;
   for (int y = -2; y < 4; y++)
   {
      for (int x = -2; x < 3; x++)
      {
         const float3 c = SampleSaturatedBilinear(source, linearSampler, uv + texel * float2(x * 0.2, y * (1.0 / 6.0)), LumaData.CustomData1 != 0u).rgb;
         sum += rms ? c * c : c;
      }
   }
   sum *= 1.0 / 30.0;
   return float4(rms ? sqrt(sum) : sum, 1.0);
}

// Runs as glow_pass2's pixel shader (swapped by main.cpp), keeping its VS, viewport, cb5 and additive blend onto the scene.
// Vanilla: glow_pass0 writes (glow, luma(glow)) x its cb5[2], per scene (e.g. (1,1,1,1), (0.8,1,1.1,0.2), (1.5,1.5,1,3)),
// and pass2 takes the 5 levels at 0.2 each, rgb * cb5[0].yzw plus alpha * cb5[0].x, saturated. The blur is linear, so the
// gains apply to the blurred glow here: rgb * pass0 cb5[2].rgb, alpha = its luma * pass0 cb5[2].w (a copy of pass0's b5
// made at pass0, at b6). DrawBloom accumulates a_k = 0.5 * m_k + 0.5 * a_k+1, so the equal-weight sum of its 5 levels is
// 2 * a0 + a1 + a2 + a3. LumaData.CustomData3/4 = 1 / the target size.
cbuffer GlowPass2 : register(b5)
{
   float4 cb5[1];
}
cbuffer GlowPass0Copy : register(b6)
{
   float4 pass0_cb5[3];
}

float4 bloom_composite_ps(float4 pos : SV_Position) : SV_Target
{
   const float2 uv = pos.xy * float2(LumaData.CustomData3, LumaData.CustomData4);
   float3 s = 2.0 * source.SampleLevel(linearSampler, uv, 0.0).rgb;
   for (int mip = 1; mip < 4; mip++)
      s += source.SampleLevel(linearSampler, uv, mip).rgb;
   s *= 0.2;
   float3 c = saturate(s * pass0_cb5[2].rgb * cb5[0].yzw + dot(s, float3(0.298912, 0.586611, 0.114478)) * pass0_cb5[2].w * cb5[0].x);
   c *= LumaSettings.GameSettings.BloomIntensity;
   return float4(c, max3(c));
}
