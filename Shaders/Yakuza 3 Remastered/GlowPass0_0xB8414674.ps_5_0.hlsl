// ps_glow_pass0 (sh_ogre3_w64.par): bloom build, a 5x6-tap RMS of the downsampled scene (t1) x cb5[2], plus (cb11[0].y & 8)
// a thresholded scene term from t0. Vanilla read and wrote 8-bit UNORM targets. The 512x256/512x512 targets are now fp16
// (for the DoF), so every input and the output are saturated to keep the vanilla bloom bounded. Otherwise verbatim.

cbuffer cb5 : register(b5)
{
   float4 cb5[3];
}
cbuffer cb11 : register(b11)
{
   uint4 cb11[1];
}

SamplerState s0_s : register(s0);
SamplerState s1_s : register(s1);
Texture2D<float4> t0 : register(t0);
Texture2D<float4> t1 : register(t1);

void main(float4 v0 : COLOR0, float2 v1 : TEXCOORD0, out float4 o0 : SV_Target0)
{
   const float3 weights = float3(0.298912, 0.586611, 0.114478);
   const float2 stepX = ddx_coarse(v1.xy) * 0.2;
   const float2 stepY = ddy_coarse(v1.xy) * (1.0 / 6.0);

   float3 sum = 0.0;
   for (int y = -2; y < 4; y++)
   {
      for (int x = -2; x < 3; x++)
      {
         float3 c = saturate(t1.Sample(s1_s, v1.xy + stepX * x + stepY * y).rgb);
         sum += c * c;
      }
   }
   float3 rms = sqrt(sum * (1.0 / 30.0));
   float4 glow = float4(rms, dot(weights, rms)) * cb5[2];

   if ((cb11[0].y & 8u) != 0u)
   {
      float3 thresholdSum = 0.0;
      float thresholdLuma = 0.0;
      for (int y = -2; y < 4; y++)
      {
         for (int x = -2; x < 3; x++)
         {
            float3 c = saturate(t0.Sample(s0_s, v1.xy + stepX * x + stepY * y).rgb);
            float4 t = saturate(float4(saturate(dot(weights, c) - cb5[0].x), saturate(c - cb5[0].yzw)) * cb5[1]);
            thresholdSum += t.yzw * t.yzw;
            thresholdLuma += t.x;
         }
      }
      glow += float4(sqrt(thresholdSum * (1.0 / 30.0)), thresholdLuma * (1.0 / 30.0));
   }

   o0 = saturate(glow);
}
