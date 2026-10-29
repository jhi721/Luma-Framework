// The bloom iterations the game skips below the output resolution: its iteration count follows the render height, one more per
// doubling (Kino), so the output resolution chain adds the missing downsamples and upsamples (see "ExtendBloom").
#include "Includes/BloomSample.hlsl"

float4 downsample_ps(float4 v0 : SV_Position0, float2 v1 : TEXCOORD0) : SV_Target0
{
   return float4(BloomDownsample(v1, false), 1.0);
}

float4 upsample_ps(float4 v0 : SV_Position0, float2 v1 : TEXCOORD0) : SV_Target0
{
   return float4(BloomUpsample(v1, GetBloomSampleScale()), 1.0);
}
