// Medal of Honor (2010) — the dgVoodoo wrapper contract, shared by every hash replacement in this game.
//
// These are not per-pass transcription: the array sizes and the mask function are the same in every shader the
// wrapper emits, so a replacement that spells them differently is wrong rather than merely different. Keeping one
// copy is also what stops a slot index being renumbered in one shader and not its neighbour.
//
// Every texture fetch in a translated shader is followed by an `and`/`or` pair against b3 — dgVoodoo's D3D9 format
// emulation. Reproducing it is mandatory: dropping it shifts colour.

#ifndef LUMA_MOH_DGVOODOO
#define LUMA_MOH_DGVOODOO

cbuffer DgVoodooState : register(b3)
{
   float4 DgvConstants[77] : packoffset(c0);
}

cbuffer PixelShaderConstants : register(b4)
{
   float4 PsConstants[236] : packoffset(c0);
}

// One (mask, fill) pair per sampler slot: s0 at 44/45, s1 at 46/47, and so on.
#define DgvMaskScene DgvConstants[44]
#define DgvFillScene DgvConstants[45]
#define DgvMaskLUT   DgvConstants[46]
#define DgvFillLUT   DgvConstants[47]
#define DgvMaskBlur  DgvConstants[48]
#define DgvFillBlur  DgvConstants[49]
#define DgvMaskDepth DgvConstants[50]
#define DgvFillDepth DgvConstants[51]

float4 ApplyDgvMask(float4 value, float4 mask, float4 fill)
{
   return asfloat((asuint(value) & asuint(mask)) | asuint(fill));
}

#endif // LUMA_MOH_DGVOODOO
