// ps_down_sample_4x4_with_glare (Y5R sh_devil_w64.par): the first step of the auto-exposure meter, a 5x6-tap RMS of the
// scene (t0) plus the glow (t1) x cb5[0].x, reduced to BT.601 luma into an r32_float target and averaged down to 1x1.
// Both inputs were 8-bit UNORM in vanilla and are fp16 now; the meter would read the HDR highlights and change the
// exposure, so both samples are saturated. Otherwise verbatim.

cbuffer cb5 : register(b5)
{
   float4 cb5[1];
}

SamplerState s0_s : register(s0);
SamplerState s1_s : register(s1);
Texture2D<float4> t0 : register(t0);
Texture2D<float4> t1 : register(t1);

void main(float2 v0 : TEXCOORD0, out float4 o0 : SV_Target0)
{
   const float2 stepX = ddx_coarse(v0.xy) * 0.2;
   const float2 stepY = ddy_coarse(v0.xy) * (1.0 / 6.0);

   float3 scene = 0.0;
   float3 glow = 0.0;
   for (int y = -2; y < 4; y++)
   {
      for (int x = -2; x < 3; x++)
      {
         const float2 uv = v0.xy + stepX * x + stepY * y;
         const float3 s = saturate(t0.Sample(s0_s, uv).xyz);
         scene += s * s;
         const float3 g = saturate(t1.Sample(s1_s, uv).xyz) * cb5[0].x;
         glow += g * g;
      }
   }
   o0 = dot(sqrt(scene * (1.0 / 30.0)) + sqrt(glow * (1.0 / 30.0)), float3(0.299, 0.587, 0.114));
}
