// RGB copy of the upscaler's output back into the scene color (COLORWRITEENABLE = RGB: the destination's alpha is the
// bloom threshold input and stays). Pixels from VPOS, point sampled; the source and the target have the same size.

sampler2D source : register(s0);
float4 size : register(c0); // width, height

float4 main(float2 vpos : VPOS) : COLOR0
{
   const float2 uv = (floor(vpos) + 0.5) / size.xy;
   return float4(tex2Dlod(source, float4(uv, 0.0, 0.0)).rgb, 1.0);
}
