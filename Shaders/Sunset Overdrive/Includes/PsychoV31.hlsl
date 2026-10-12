#ifndef LUMA_PSYCHOV31_HLSL
#define LUMA_PSYCHOV31_HLSL

// PsychoV31, RenoDX's tonemapper (Carlos Lopez and Musa Haji, MIT), with the RenoDX shader files it needs in "RenoDX/".
// They are copied as is from clshortfuse/renodx 811a37a2: "psychov_customtest31.hlsl" is the "customtest31.hlsli" shared
// by its Onimusha, Pragmata, AC Black Flag and 007 First Light mods, minus its include of the game's "common.hlsli".
// Call "renodx::tonemap::psychov::custom_psychotm_test31()".

// Luma's "Math.hlsl" defines these as macros, which would break RenoDX's constants of the same names. fxc has no "push_macro",
// so they are defined again below, as in "Math.hlsl" (keep them in sync).
#undef FLT_MIN
#undef FLT_MAX
#undef FLT_EPSILON
#undef FLT10_MAX
#undef FLT11_MAX
#undef FLT16_MAX
#undef PI

#include "RenoDX/color/macleod_boynton.hlsl"
#include "RenoDX/tonemap/psychov/test30.hlsl"

namespace renodx
{
namespace tonemap
{
// From RenoDX's "tonemap/neutwo.hlsl" (Carlos Lopez, MIT): the only part of it PsychoV31 uses, the rest needs all of RenoDX's
// color library
float Neutwo(float x, float peak)
{
   // also written as x * rhypot(x, peak)
   float p = peak;

   float numerator = p * x;
   float denominator_squared = mad(x, x, p * p);
   return numerator * rsqrt(denominator_squared);
}
} // namespace tonemap
} // namespace renodx

// fxc, unlike RenoDX's dxc: X3577 drops "isnan()" checks it proves redundant (they are, there), and X4000 flags the early
// returns of "custom_psycho31_ShoulderMeanA2Weight()" as an uninitialized result. Both come at code generation, so they stay
// off for the whole shader, as in "Common.hlsl".
#pragma warning(disable : 3577)
#pragma warning(disable : 4000)
#include "RenoDX/psychov_customtest31.hlsl"

#define FLT_MIN     asfloat(0x00800000)
#define FLT_MAX     asfloat(0x7F7FFFFF)
#define FLT_EPSILON 1.192092896e-07
#define FLT10_MAX   64512.f
#define FLT11_MAX   65024.f
#define FLT16_MAX   65504.f
#define PI          3.141592653589793238462643383279502884197

#endif // LUMA_PSYCHOV31_HLSL
