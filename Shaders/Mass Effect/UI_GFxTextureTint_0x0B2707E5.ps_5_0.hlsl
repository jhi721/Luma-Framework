// Mass Effect (2007) - GFx textured fill blended against the vertex colour (COLOR1.z is the texture weight, COLOR1.w
// a second alpha). The most common HUD shader. See Includes/GFxUI.hlsl: vanilla body plus the 8-bit canvas clamp.
#include "Includes/GFxUI.hlsl"

Texture2D<float4> t0 : register(t0);
SamplerState s0_s : register(s0);

// 13 interpolators, all declared in order - linkage is by REGISTER (see Luma_ME1_Tonemap.hlsl). This shader reads
// COLOR0 (v2), COLOR1 (v3) and TEXCOORD0 (v5.xy).
void main(DGV_MAIN_SIGNATURE)
{
   float4 texel = ApplyDgvMask(t0.Sample(s0_s, v5.xy), DgvMaskT0, DgvFillT0);
   float4 factors = saturate(v3);
   float4 color = GFxCxform(lerp(saturate(v2), texel, factors.z));
   color.w *= factors.w;
   o0 = saturate(color);
}
