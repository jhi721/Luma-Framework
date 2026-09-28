// ps_texture_y_threshold (Y5R sh_devil_w64.par): the bright-pass of the star glare, keeping the color whose luma exceeds
// cb5[0].x. Its source was 8-bit UNORM in vanilla and is fp16 now; the mask depends on every channel, so the source is
// taken per texel as vanilla stored it, through the vanilla material curve, before the test (a clamped output would still
// pass or reject other pixels). Otherwise verbatim.
#include "Includes/Common.hlsl"

cbuffer cb5 : register(b5)
{
   float4 cb5[1];
}

SamplerState s0_s : register(s0);
Texture2D<float4> t0 : register(t0);

void main(float4 v0 : TEXCOORD0, out float4 o0 : SV_Target0)
{
   const float3 c = SampleSaturatedBilinear(t0, s0_s, v0.xy, true).rgb;
   o0 = float4(dot(c, float3(0.299, 0.587, 0.114)) - cb5[0].x > 0.0 ? c : 0.0, 1.0);
}
