// ps_down_sample (Y4R sh_soul_w64.par, also in Y5R): 5x6-tap RMS of the swapchain-sized glow source into the 512x256
// bloom level. The source is fp16 now; vanilla clamped every texel to 1 before this sum. See SampleGlowSource.
#include "Includes/Common.hlsl"

SamplerState s0_s : register(s0);
Texture2D<float4> t0 : register(t0);

void main(float4 v0 : COLOR0, float2 v1 : TEXCOORD0, out float4 o0 : SV_Target0)
{
   const float2 stepX = ddx_coarse(v1.xy) * 0.2;
   const float2 stepY = ddy_coarse(v1.xy) * (1.0 / 6.0);
   float4 sum = 0.0;
   for (int y = -2; y < 4; y++)
   {
      for (int x = -2; x < 3; x++)
      {
         const float4 c = SampleGlowSource(t0, s0_s, v1.xy + stepX * x + stepY * y);
         sum += c * c;
      }
   }
   o0 = sqrt(sum * 0.033333);
}
