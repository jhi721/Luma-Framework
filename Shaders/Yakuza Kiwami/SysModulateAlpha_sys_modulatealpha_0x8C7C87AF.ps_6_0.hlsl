// Yakuza Kiwami built-in sys_modulatealpha (0x8C7C87AF, DXIL SM 6.0; also the bytecode of sys_modulatealpha_point, _repeat and
// _point_repeat): texture x COLOR. The HUD and most 2D draw with it; among the screen effects the
// 2D image (over) and the random noise (additive). On those main.cpp flags LumaData.CustomData1 and the color gets the fade's pre-scale
// (see SysDiffuse_sys_diffuse_0x85067FE9). Every other draw is unchanged.

#include "Includes/Common.hlsl"

Texture2D<float4> s0_t : register(t0);
SamplerState s0_s : register(s0);

float4 main(linear float4 COLOR : COLOR, linear float2 TEXCOORD : TEXCOORD) : SV_Target
{
   float4 color = s0_t.Sample(s0_s, TEXCOORD) * COLOR;
   color.rgb = YKR_ScreenEffectColor(color.rgb);
   return color;
}
