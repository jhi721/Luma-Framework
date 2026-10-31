// Yakuza Kiwami built-in sys_diffuse (0x85067FE9, DXIL SM 6.0): "return COLOR". The screen fade draws
// with it over the post-ccr canvas, which YKR_EncodeOutput pre-scaled to the UI paper white (gamma 2.2). On the fade's draws main.cpp flags
// LumaData.CustomData1: the color is pre-scaled the same way, so a fade to white lands on the scene's paper white. Over and additive blends
// are linear in it, black stays black. The other draws (Rain2 before the ccr, the debug primitives) are unchanged.

#include "Includes/Common.hlsl"

float4 main(linear float4 COLOR : COLOR) : SV_Target
{
   float4 color = COLOR;
   color.rgb = YKR_ScreenEffectColor(color.rgb);
   return color;
}
