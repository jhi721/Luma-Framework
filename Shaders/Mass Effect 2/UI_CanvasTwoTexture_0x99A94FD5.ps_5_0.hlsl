// Mass Effect 2 (2010) UE3 Canvas two-texture crossfade: t1 is lerped toward t0 by COLOR1.z before the Canvas
// transform. See Includes/CanvasUI.hlsl - vanilla body plus the 8-bit clamp.
#include "Includes/CanvasUI.hlsl"
Texture2D<float4> t0 : register(t0);
SamplerState s0_s : register(s0);
Texture2D<float4> t1 : register(t1);
SamplerState s1_s : register(s1);

// This shader reads COLOR1.z (v3.z), TEXCOORD0 (v5.xy) and TEXCOORD1 (v6.xy).
void main(ME2_MAIN_SIGNATURE)
{
   const float4 a = ApplyDgvMask(t0.Sample(s0_s, v5.xy), DgvMaskT0, DgvFillT0);
   const float4 b = ApplyDgvMask(t1.Sample(s1_s, v6.xy), DgvMaskT1, DgvFillT1);
   // `add r3, r0, -r1` then `mad r2, v3.zzzz, r3, r1` - a lerp from t1 to t0.
   o0 = saturate(ME2_CanvasTransform(lerp(b, a, v3.z)));
}
