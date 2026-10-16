#ifndef LUMA_ME3_GAME_BINDINGS
#define LUMA_ME3_GAME_BINDINGS

#include "../../Includes/DgVoodoo.hlsl"
#include "../../Includes/UE3Grade.hlsl" // PowUE3, UE3Grade

// Bindings and vanilla math shared by the replaced passes: no textures, samplers or cbuffer-row aliases, since slot and
// row meaning is per pass.

// b3/b4 are dgVoodoo's D3D9 constant mirrors, declared at the original's sizes (CB3[77], CB4[236]). SM3 c<N> = cb4[N+8].
cbuffer DgVoodooState : register(b3)
{
   float4 DgvConstants[77] : packoffset(c0);
}

cbuffer PixelShaderConstants : register(b4)
{
   float4 PsConstants[236] : packoffset(c0);
}

// dgVoodoo's texture format emulation ("ApplyDgvMask"): one (mask, fill) pair per sampler slot, s<N> at cb3[44 + 2N] / cb3[45 + 2N]
float4 ApplyDgvMask(float4 value, uint slot)
{
   return ApplyDgvMask(value, DgvConstants[44 + 2 * slot], DgvConstants[45 + 2 * slot]);
}

// UE3's DoF blur weight from view depth, as both the full-res uber and the gather compute it: pow(saturate(|depth -
// focus| * 1/range), falloff), capped per side. The rows differ per pass: `dofParams` = (focus, 1/range, falloff),
// `maxBlur` = (near, far).
float ME3_DoFBlur(float depth, float3 dofParams, float2 maxBlur)
{
   const float signedDistance = depth - dofParams.x;
   const float t = saturate(abs(signedDistance) * dofParams.y);
   const float blur = t >= 1e-6 ? exp2(log2(t) * dofParams.z) : 0.0;
   return min(blur, signedDistance >= 0.0 ? maxBlur.y : maxBlur.x);
}

#endif // LUMA_ME3_GAME_BINDINGS
