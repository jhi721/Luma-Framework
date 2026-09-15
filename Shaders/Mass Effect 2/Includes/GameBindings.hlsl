#ifndef LUMA_ME2_GAME_BINDINGS
#define LUMA_ME2_GAME_BINDINGS

// Mass Effect 2 (2010) - bindings the replaced passes share, and nothing else. Deliberately no textures, samplers
// or game-content row aliases: slot and row meaning is per pass. Name rows in the pass that reads them.

// b3/b4 are dgVoodoo's D3D9 constant mirrors, declared at the original's sizes (CB3[77], CB4[236]).
cbuffer DgVoodooState : register(b3)
{
   float4 DgvConstants[77] : packoffset(c0);
}

cbuffer PixelShaderConstants : register(b4)
{
   float4 PsConstants[236] : packoffset(c0);
}

// dgVoodoo texture-format emulation masks: one (mask, fill) pair per sampler slot, s0 at 44/45, s1 at 46/47, s2 at 48/49.
// Every texture fetch in a translated shader is followed by this pair; dropping it shifts colour.
#define DgvMaskT0 DgvConstants[44]
#define DgvFillT0 DgvConstants[45]
#define DgvMaskT1 DgvConstants[46]
#define DgvFillT1 DgvConstants[47]
#define DgvMaskT2 DgvConstants[48]
#define DgvFillT2 DgvConstants[49]

float4 ApplyDgvMask(float4 value, float4 mask, float4 fill)
{
   return asfloat((asuint(value) & asuint(mask)) | asuint(fill));
}

// The grade's pow() as the original computes it. The tiny floor replaces the compiler's own log2(0) guard.
float3 PowUE3(float3 base, float3 exponent)
{
   return exp2(exponent * log2(max(abs(base), 1e-30)));
}

#endif // LUMA_ME2_GAME_BINDINGS
