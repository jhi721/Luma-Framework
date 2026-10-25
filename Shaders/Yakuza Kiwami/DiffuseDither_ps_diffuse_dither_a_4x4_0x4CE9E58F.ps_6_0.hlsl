// Yakuza Kiwami built-in ps_diffuse_dither_a_4x4 (0x4CE9E58F, DXIL SM 6.0): COLOR, its alpha offset by a 4x4 ordered dither
// (_tools/ykr/hlsl/0x4CE9E58F.ps_6_0.hlsl). The screen effects of a transition draw with it (over and additive) like the fade with sys_diffuse:
// on their draws main.cpp flags LumaData.CustomData1 and the color gets the same pre-scale (see SysDiffuse_sys_diffuse_0x85067FE9). Its
// world-space draws are unchanged.

#include "Includes/Common.hlsl"

// Rows by y, columns by x
static const float4x4 alpha_dither = {
    0.0f, 0.0019607844296842813f, 0.0004901961074210703f, 0.0024509804788976908f,
    0.0029411765281111f, 0.0009803922148421407f, 0.0034313725773245096f, 0.00147058826405555f,
    0.000735294132027775f, 0.0026960785035043955f, 0.00024509805371053517f, 0.002205882454290986f,
    0.0036764706019312143f, 0.0017156862886622548f, 0.003186274552717805f, 0.0012254902394488454f};

float4 main(linear float4 COLOR : COLOR, noperspective float4 SV_Position : SV_Position) : SV_Target
{
   float4 color = COLOR;
   const uint2 cell = uint2(frac((SV_Position.xy - 0.5) * 0.25) * 4.0);
   color.a += alpha_dither[cell.y][cell.x];
   color.rgb = YKR_ScreenEffectColor(color.rgb);
   return color;
}
