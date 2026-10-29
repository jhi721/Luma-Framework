// The game's letterbox (0xEC5254F6): its full frame target onto the swapchain, within the black bars, at custom resolutions (see
// "letterbox_hash"). Vanilla "exp(log(color) * gamma)" turns the negative values of the upgraded target (upscaler and sharpening
// undershoot, 0 in UNORM) into NaN: the power keeps the sign instead.
cbuffer _Globals : register(b0)
{
   float gamma : packoffset(c0);
}

SamplerState smp : register(s0);
Texture2D<float4> tex : register(t0);

void main(float4 v0 : SV_Position0, float4 v1 : COLOR0, float2 v2 : TEXCOORD0, out float4 o0 : SV_Target0)
{
   const float4 color = tex.Sample(smp, v2);
   o0 = float4(sign(color.rgb) * pow(abs(color.rgb), gamma), color.a);
}
