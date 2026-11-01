// clang-format off
#include "Includes/Common.hlsl" // game-local: LumaGameSettings before the shared Settings.hlsl, keep FIRST
#include "../Includes/DgVoodoo.hlsl" // DGV_MAIN_SIGNATURE, ApplyDgvMask
#include "Includes/BloomBlur.hlsl"
// clang-format on

// Vanquish bloom blur "filter01" (H, dgVoodoo 2.87.6 -> ps_5_0 0x55E02134): the taps at +-1.5 / +-3.5 texels of 320, times c184
// (cb4[192]). See Includes/BloomBlur.hlsl.
void main(DGV_MAIN_SIGNATURE)
{
   o0 = BloomBlur(v5.xy, float2(1.5 / 320.0, 0.0), float2(3.5 / 320.0, 0.0));
}
