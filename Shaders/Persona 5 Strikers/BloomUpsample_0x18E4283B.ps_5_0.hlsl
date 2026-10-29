// Katana engine bloom upsample (0x18E4283B): 3x3 tent, added into the next mip up. See "Includes/BloomSample.hlsl". 0xE40794E8 is the
// g_vMaxUV clamped twin.
#include "Includes/BloomSample.hlsl"

void main(float4 v0 : SV_Position0, float2 v1 : TEXCOORD0, out float4 o0 : SV_Target0)
{
   o0 = float4(BloomUpsample(v1, GetBloomSampleScale()), 1.0);
}
