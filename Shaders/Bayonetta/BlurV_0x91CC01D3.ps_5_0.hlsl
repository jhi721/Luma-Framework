// clang-format off
#include "Includes/Common.hlsl" // game-local: LumaGameSettings before the shared Settings.hlsl, keep FIRST
#include "../Includes/DgVoodoo.hlsl" // DGV_MAIN_SIGNATURE, ApplyDgvMask
#include "Includes/BloomBlur.hlsl"
// clang-format on

// Bayonetta bloom blur "filter02" (V, dgVoodoo 2.87.5 -> ps_5_0 0x91CC01D3). See Includes/BloomBlur.hlsl.
void main(DGV_MAIN_SIGNATURE)
{
   o0 = BloomBlur(v5.xy, float2(0.0, 1.0));
}
