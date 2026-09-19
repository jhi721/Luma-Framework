#ifndef LUMA_ME3_GAME_BINDINGS
#define LUMA_ME3_GAME_BINDINGS

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

// dgVoodoo texture-format emulation masks: one (mask, fill) pair per sampler slot, s<N> at cb3[44 + 2N] / cb3[45 + 2N].
// Every texture fetch in a translated shader is followed by this pair; dropping it shifts colour.
float4 ApplyDgvMask(float4 value, uint slot)
{
   return asfloat((asuint(value) & asuint(DgvConstants[44 + 2 * slot])) | asuint(DgvConstants[45 + 2 * slot]));
}

// The grade's pow() as the original computes it. The tiny floor replaces the compiler's own log2(0) guard.
float3 PowUE3(float3 base, float3 exponent)
{
   return exp2(exponent * log2(max(abs(base), 1e-30)));
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

// dgVoodoo's fixed interpolator layout (linkage is by register): every entry point declares all 13, read or not. 2.87.3
// declares the colours centroid in the Scaleform/canvas shaders.
#define DGV_SIGNATURE_TAIL          float4 v4 : TEXCOORD9, float4 v5 : TEXCOORD0, float4 v6 : TEXCOORD1, float4 v7 : TEXCOORD2, float4 v8 : TEXCOORD3, float4 v9 : TEXCOORD4, float4 v10 : TEXCOORD5, float4 v11 : TEXCOORD6, float4 v12 : TEXCOORD7, out float4 o0 : SV_TARGET0
#define DGV_MAIN_SIGNATURE          float4 v0 : SV_POSITION0, float4 v1 : TEXCOORD8, float4 v2 : COLOR0, float4 v3 : COLOR1, DGV_SIGNATURE_TAIL
#define DGV_MAIN_SIGNATURE_CENTROID float4 v0 : SV_POSITION0, float4 v1 : TEXCOORD8, centroid float4 v2 : COLOR0, centroid float4 v3 : COLOR1, DGV_SIGNATURE_TAIL

#endif // LUMA_ME3_GAME_BINDINGS
