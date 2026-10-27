// The Witcher 2 adaptation's first luminance pass (dgVoodoo -> ps_5_0, hash 0x156E66D6): a 2x2 box (taps 2 texels apart) of the
// linear scene at quarter size, its luminance into r32_float. Its downsamples end in the 1x1 adaptation, which every frame blends
// into the previous one: a frame history. Transcribed from the disassembly, same operation order.
// Vanilla clamps with max(lum, 0), which already turns a NaN into 0 (D3D11 min/max return the non NaN operand); an infinity
// (an fp16 overflow) passes it and would stay in the adaptation for good, so it is held at the largest float16 too.
#include "Includes/GameBindings.hlsl" // b3/b4, the dgVoodoo masks, ApplyDgvMask

Texture2D<float4> t0 : register(t0); // linear scene
SamplerState s0_s : register(s0);

void main(DGV_MAIN_SIGNATURE)
{
   const float2 step = 2.0 * cb4[61].xy; // cb4[61].xy: one scene texel
   float3 sum = ApplyDgvMask(t0.Sample(s0_s, v5.xy + float2(step.x, 0.0)), DgvMaskT0, DgvFillT0).rgb;
   sum += ApplyDgvMask(t0.Sample(s0_s, v5.xy), DgvMaskT0, DgvFillT0).rgb;
   sum += ApplyDgvMask(t0.Sample(s0_s, v5.xy + float2(0.0, step.y)), DgvMaskT0, DgvFillT0).rgb;
   sum += ApplyDgvMask(t0.Sample(s0_s, v5.xy + step), DgvMaskT0, DgvFillT0).rgb;
   const float luminance = dot(cb4[60].xyz, sum * 0.25);
   o0 = min(max(luminance, 0.0), 65504.0);
}
