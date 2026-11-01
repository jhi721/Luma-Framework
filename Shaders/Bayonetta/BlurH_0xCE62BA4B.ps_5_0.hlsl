// clang-format off
#include "Includes/Common.hlsl" // game-local: LumaGameSettings before the shared Settings.hlsl, keep FIRST
#include "../Includes/DgVoodoo.hlsl" // DGV_MAIN_SIGNATURE, ApplyDgvMask
#include "Includes/BloomBlur.hlsl"
// clang-format on

// Bayonetta bloom blur "filter01" (H, dgVoodoo 2.87.5 -> ps_5_0 0xCE62BA4B). See Includes/BloomBlur.hlsl.
void main(DGV_MAIN_SIGNATURE)
{
   o0 = BloomBlur(v5.xy, float2(1.0, 0.0));
}
