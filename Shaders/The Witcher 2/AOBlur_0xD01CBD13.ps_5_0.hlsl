// The Witcher 2 AO blur (dgVoodoo -> ps_5_0, hash 0xD01CBD13): the bilateral blur run twice (horizontal, then vertical by
// cb4[62].xy) after the AO pack, on the packed AO (.x) and linear depth (.y). Transcribed from the disassembly, same operation order.
// Its step is cb4[60].xy (1 / the full surface) texels, so under the render scale, where a texel is a render pixel, the 13 taps
// covered twice the view angle at 50% and washed out contact AO (from XeGTAO or the native generator alike). The step is scaled by
// the render area's share of the surface, cb4[42].zw (c34 "PSC_ViewportSubSize"; 1 at native, which leaves the math unchanged).
#include "Includes/Common.hlsl"
#include "Includes/GameBindings.hlsl" // b3/b4, the dgVoodoo masks, ApplyDgvMask

Texture2D<float4> t0 : register(t0); // .x AO, .y linear view depth
SamplerState s0_s : register(s0);

void main(
    float4 v0 : SV_POSITION0,
    float4 v1 : TEXCOORD8,
    float4 v2 : COLOR0,
    float4 v3 : COLOR1,
    float4 v4 : TEXCOORD9,
    float4 v5 : TEXCOORD0,
    float4 v6 : TEXCOORD1,
    float4 v7 : TEXCOORD2,
    float4 v8 : TEXCOORD3,
    float4 v9 : TEXCOORD4,
    float4 v10 : TEXCOORD5,
    float4 v11 : TEXCOORD6,
    float4 v12 : TEXCOORD7,
    out float4 o0 : SV_TARGET0)
{
   // cb4[60]: .xy the surface's texel size (min clamp, a half texel in), .zw the area's max UV; cb4[62]: .xy the direction, .z the
   // depth falloff; cb4[63..69].x the kernel weights
   const float2 uv = min(cb4[60].zw, max(v5.xy, cb4[60].xy));
   const float2 step = cb4[62].xy * cb4[60].xy * cb4[42].zw;
   const float4 center = ApplyDgvMask(t0.Sample(s0_s, uv), DgvMaskT0, DgvFillT0);

   float sum = center.x * cb4[63].x;
   float norm = cb4[63].x;
   [unroll] for (int i = 1; i <= 6; i++)
   {
      [unroll] for (int side = 0; side < 2; side++)
      {
         const float offset = (side == 0 ? 2.0 : -2.0) * float(i);
         const float4 tap = ApplyDgvMask(t0.Sample(s0_s, step * offset + uv), DgvMaskT0, DgvFillT0);
         // Depth aware weight, relaxed toward 1 with distance (full at 100 units)
         const float depth_weight = saturate(1.0 - cb4[62].z * abs(tap.y - center.y));
         const float weight = saturate(tap.y * 0.01) * (1.0 - depth_weight) + depth_weight;
         norm += cb4[63 + i].x * weight;
         sum += weight * cb4[63 + i].x * tap.x;
      }
   }
   o0.x = sum * ((abs(norm) > 0.0) ? rcp(norm) : 9999999933815812510711506376257961984.0);
   o0.yzw = float3(center.y, 0.0, 0.0);
}
