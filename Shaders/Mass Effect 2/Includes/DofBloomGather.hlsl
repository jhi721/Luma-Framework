#ifndef LUMA_ME2_DOF_BLOOM_GATHER
#define LUMA_ME2_DOF_BLOOM_GATHER

// Mass Effect 2 (2010) UE3 DOFAndBloomGather, body shared by both perms. Replaced for ONE purpose: to switch the
// game's own bloom off at its source when the Luma pyramid replaces it. Perms, taps and evidence: NOTES.md.

// clang-format off
// ORDER IS LOAD-BEARING, do not sort: game-local Common.hlsl first, or GameSettings resolves to the empty dummy.
#include "Common.hlsl"       // game-local: LumaSettings.GameSettings.LumaBloomEnable
#include "GameBindings.hlsl" // b3/b4, the dgVoodoo masks, ApplyDgvMask, PowUE3
// clang-format on

// Only what this pass samples.
SamplerState SceneColorTextureSampler_s : register(s0);
Texture2D<float4> SceneColorTexture : register(t0); // fp16 scene color; .w carries SCENE DEPTH, not alpha

// cb4 is dgVoodoo's SHARED mirror, so a row means different things per pass: row 11 is the grade's shadows lift on
// the uber but the engine's bloom scale HERE. Named per pass on purpose; rows 8/10 match the uber.
#define DoFParams     PsConstants[8]  // .x focus distance, .y 1/range, .z falloff exponent
#define DoFMaxBlur    PsConstants[10] // .x max blur near, .y max blur far
#define DoFBloomScale PsConstants[11] // .x = bloom scale

// The vanilla glow's gain, or zero when the Luma pyramid owns the glow. Read as a BOOLEAN - a weight would leave
// half the glow here, and this is the glow's ONLY switch. Uniform, so it folds in once instead of per tap.
static const float LumaGatherBloomScale = (LumaSettings.GameSettings.LumaBloomEnable > 0.5) ? 0.0 : DoFBloomScale.x;

// One tap: the raw sample accumulates for the average (depth included, in .w), the bright pass accumulates
// separately. Vanilla bright-pass: the tap passes through whole when any channel is above 1.0.
void GatherTap(float2 uv, inout float4 sceneSum, inout float3 bloomSum)
{
   const float4 s = ApplyDgvMask(SceneColorTexture.Sample(SceneColorTextureSampler_s, uv), DgvMaskT0, DgvFillT0);
   sceneSum += s;
   bloomSum += any(s.xyz > 1.0) ? s.xyz : (float3)0.0;
}

// The tail, identical in both perms apart from invN: one DoF weight from the AVERAGE depth, the bloom as a separate
// summand, then the 1/4 pre-divide - the alpha the uber reads back as `blurred.w * 4.0` is this same blur weight.
float4 GatherTail(float4 sceneSum, float3 bloomSum, float invN)
{
   const float4 avg = sceneSum * invN;

   // Vanilla DoF weight. Luma_ME2_Tonemap.hlsl computes the same thing and the two are deliberately NOT shared: they
   // are independent transcriptions of two DIFFERENT vanilla shaders, and a helper could not carry the rows anyway.
   const float signedDistance = avg.w - DoFParams.x;
   const float normalizedDistance = saturate(abs(signedDistance) * DoFParams.y);
   const float maxBlur = (signedDistance >= 0.0) ? DoFMaxBlur.y : DoFMaxBlur.x;
   // The original floors the pow's base at 1e-4 before its log; passed explicitly so PowUE3's own 1e-30 floor never
   // engages and the transcription stays exact.
   const float blurAmount = min(PowUE3(max(normalizedDistance, 1e-4).xxx, DoFParams.zzz).x, maxBlur);

   const float3 bloom = bloomSum * LumaGatherBloomScale * invN;
   return float4(blurAmount * avg.xyz + bloom, blurAmount) * 0.25;
}

#endif // LUMA_ME2_DOF_BLOOM_GATHER
