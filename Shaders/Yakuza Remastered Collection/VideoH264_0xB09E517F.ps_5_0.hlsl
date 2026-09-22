// ps_sofdec_h264 (Y4R sh_soul_w64.par / Y5R sh_devil_w64.par; absent in Y3R): CRI Sofdec H.264 video, NV12 (t0 = Y,
// t1 = interleaved CbCr), BT.601 limited-range YUV -> RGB with a radial 4x4 sinc(x) * sinc(x / 2) reconstruction (Lanczos-2
// without its window), drawn opaque in the UI stage. Rewritten from the disassembly with the same taps, weights and matrix.
// Kernel ringing and limited-range excursions leave [0,1] like the other two decoders; the only change is the final output
// (Includes/Video.hlsl).
#include "Includes/Video.hlsl"

Texture2D<float4> t0 : register(t0);

Texture2D<float4> t1 : register(t1);

SamplerState s0_s : register(s0);

SamplerState s1_s : register(s1);

void main(
    float4 v0 : COLOR0,
    float2 v1 : TEXCOORD0,
    float4 v2 : SV_POSITION0,
    out float4 o0 : SV_Target0)
{
   float2 size;
   t0.GetDimensions(size.x, size.y);
   const float2 pos = v1 * size - 0.5;
   const float2 base = floor(pos);
   float3 sum = 0;
   float weightSum = 0;
   [unroll] for (int y = -1; y <= 2; y++)
   {
      [unroll] for (int x = -1; x <= 2; x++)
      {
         const float2 tap = base + float2(x, y);
         // Radial sinc(x) * sinc(x / 2), not windowed at 2, with its limit pi^2/2 at the center.
         const float d = length(tap - pos);
         const float weight = d == 0.0 ? 4.93480206 : sin(d * 1.57079637) * sin(d * 3.14159274) / (d * d);
         const float2 uv = (tap + 0.5) / size;
         const float3 yuv = float3(t0.Sample(s0_s, uv).x, t1.Sample(s1_s, uv).xy) - float3(16.0 / 255.0, 128.0 / 255.0, 128.0 / 255.0);
         sum += weight * float3(dot(float2(1.164384, 1.596027), yuv.xz), dot(float3(1.164384, -0.391607, -0.812813), yuv), dot(float2(1.164384, 2.017232), yuv.xy));
         weightSum += weight;
      }
   }
   o0 = float4(sum / weightSum, 1.0) * v0;
   o0.rgb = VideoToOutput(o0.rgb);
   o0.a = saturate(o0.a);
}
