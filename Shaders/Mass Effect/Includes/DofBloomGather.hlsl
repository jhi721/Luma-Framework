#ifndef LUMA_ME1_DOF_BLOOM_GATHER
#define LUMA_ME1_DOF_BLOOM_GATHER

// Mass Effect (2007) - UE3 DOFAndBloomGather, the per-tap body shared by both permutations. Replaced for ONE purpose:
// to switch the game's own bloom off at its source when the Luma HDR bloom pyramid replaces it (same reasoning as the
// MoH Airborne gather).
//
// This pass writes a single quarter-res target that carries BOTH the depth-of-field blur and the bloom, summed into
// .xyz, with the DoF weight in .w, so nothing downstream can drop the bloom without taking DoF with it. Here the bloom
// is still a separate summand, so zeroing it leaves the DoF math untouched, bit for bit.
//
// Two permutations, keyed by BIOEngine.ini [SystemSettings] QualityBloom (measured 2026-09-02):
//  - TRUE  -> 0x56854256, SIXTEEN taps (v5.xy, v5.wz, v6.xy, v6.wz, ... v12.wz), output * 1/64;
//  - FALSE -> 0x28F8DB16, FOUR taps (v5.xy, v5.wz, v6.xy, v6.wz), output * 1/16.
// Same per-tap math in both: 1/N for the tap average, then a 1/4 pre-divide so the write fits the target's UNORM
// view, which clamps at 1.0 whatever its bit depth (the grade multiplies by 4; the fp16 upgrade lifts that ceiling
// but not the scale).
// Everything else is a transcription of the dgVoodoo 2.87.3 disassembly. This UE3 build differs from MoH Airborne's in
// three measured ways, all reproduced rather than "fixed": every tap feeds the bloom at 1x gain (MoHA: first two taps
// at 2x), the whole tap passes when any channel exceeds 1.0 (not the excess over the threshold), and the DoF weight is
// computed PER TAP from that tap's depth and multiplies that tap's colour (MoHA weighs the 4-tap average by the
// average depth).

// clang-format off
// ORDER IS LOAD-BEARING - do not sort. The game-local "Common.hlsl" MUST come first: it defines LUMA_GAME_CB_STRUCTS
// (via GameCBuffers.hlsl) BEFORE any shared header pulls Settings.hlsl, so LumaSettings.GameSettings resolves to the
// real grade struct rather than the empty dummy.
#include "Common.hlsl"       // game-local: LumaSettings.GameSettings.LumaBloomEnable
#include "GameBindings.hlsl" // b3/b4, the dgVoodoo masks, ApplyDgvMask, PowUE3
// clang-format on

// Only what this pass samples.
SamplerState SceneColorTextureSampler_s : register(s0);
Texture2D<float4> SceneColorTexture : register(t0); // fp16 scene color; .w carries SCENE DEPTH, not alpha

// cb4 is dgVoodoo's SHARED constant mirror, so the same row means different things per pass: row 11 is the grade's
// shadows lift, but HERE it is the engine's bloom scale. Named per pass on purpose. Rows 8/10 match the uber pass.
#define DoFParams     PsConstants[8]  // .x focus distance, .y 1/range, .z falloff exponent
#define DoFMaxBlur    PsConstants[10] // .x max blur near, .y max blur far
#define DoFBloomScale PsConstants[11] // .x = bloom scale

// The vanilla glow's gain, or zero when the Luma pyramid owns the glow and this buffer is pure defocus. LumaBloomEnable
// is read as a BOOLEAN, like the tonemap does: the field is fed by a C++ bool, and a weight here would leave half the
// vanilla glow in this buffer. This is also the ONLY switch the vanilla glow has - BloomIntensity cannot reach it.
// Uniform, so it is folded into the scale ONCE per pixel rather than tested inside GatherTap, which cost 15 extra
// instruction slots per pixel across the 16 taps (measured, fxc /O3); the donor keeps its gate outside the taps too.
static const float LumaGatherBloomScale = (LumaSettings.GameSettings.LumaBloomEnable > 0.5) ? 0.0 : DoFBloomScale.x;

// One tap: (blur * colour + bloom, blur). Vanilla bright-pass: the tap passes through when any channel is above 1.0.
float4 GatherTap(float2 uv)
{
   const float4 s = ApplyDgvMask(SceneColorTexture.Sample(SceneColorTextureSampler_s, uv), DgvMaskT0, DgvFillT0);

   const float3 bloom = any(s.xyz > 1.0) ? s.xyz * LumaGatherBloomScale : (float3)0.0;

   // Vanilla DoF weight, per tap. Same math as Luma_ME1_Tonemap.hlsl off the same rows, deliberately kept separate:
   // see the note there.
   const float signedDistance = s.w - DoFParams.x;
   const float normalizedDistance = saturate(abs(signedDistance) * DoFParams.y);
   const float maxBlur = (signedDistance >= 0.0) ? DoFMaxBlur.y : DoFMaxBlur.x;
   const float blurAmount = min(PowUE3(normalizedDistance.xxx, DoFParams.zzz).x, maxBlur);

   return float4(blurAmount * s.xyz + bloom, blurAmount);
}

#endif // LUMA_ME1_DOF_BLOOM_GATHER
