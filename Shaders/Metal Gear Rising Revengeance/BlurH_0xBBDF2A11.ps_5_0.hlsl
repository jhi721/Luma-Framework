// clang-format off
#include "Includes/Common.hlsl" // game-local: LumaGameSettings before the shared Settings.hlsl, keep FIRST
#include "../Includes/DgVoodoo.hlsl" // DGV_MAIN_SIGNATURE, ApplyDgvMask
#include "Includes/BloomBlur.hlsl"
// clang-format on

// MGR:R bloom blur "filter01" (H, dgVoodoo 2.87.5 -> ps_5_0 0xBBDF2A11): the taps at +-1.5 / +-3.5 texels of 320, the glare colour
// lerp(g_GlareColor1, g_GlareColor2, alpha) (c185 / c186 = cb4[193] / cb4[194]) on rgb, then times c184 (cb4[192]). See
// Includes/BloomBlur.hlsl.
void main(DGV_MAIN_SIGNATURE)
{
   float4 color = BloomBlur(v5.xy, float2(1.5 / 320.0, 0.0), float2(3.5 / 320.0, 0.0));
   color.rgb *= lerp(cb4[193].rgb, cb4[194].rgb, color.a);
   o0 = color * cb4[192];
}
