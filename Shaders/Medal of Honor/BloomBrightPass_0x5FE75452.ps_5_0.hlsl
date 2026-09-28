// Medal of Honor (2010) — bloom bright pass (VS 0xF2A35A74). Four bilinear taps of the canvas, each scaled by its
// own (alpha * 8) overbright hint, averaged, then the bloom threshold is subtracted.
//
// The original ends on a bare `mad o0.xyz, sum, 0.25, -cb4[8].x` with NO clamp: it relies on the r8g8b8a8_unorm
// render target to clip the negative result of that subtraction to zero. Luma upgrades the whole post chain to
// fp16, which removes that implicit clamp, so every sub-threshold pixel — i.e. almost the entire frame — kept its
// negative value, the five pyramid levels blurred it around, and the composite (which reduces to `out = sum * k`
// wherever the base is dark) drove the canvas to roughly -20. That is what blacked the scene out while the HUD and
// the menu, which never run this pass, stayed correct.
//
// The UNORM target enforced a CEILING as well, and that half matters just as much. Vanilla's overbright trick
// feeds this pass `canvas * max(rawMax, 1)`, which the 8-bit target then clipped at 1.0 -- so no matter how bright
// the source, one pyramid level could never carry more than diffuse white. In HDR the weight resolves to 1 and the
// canvas itself carries the overbright instead, up to peak/paper white (~2.9 in this gamma encoding at 2280 nits),
// so the same subtraction reaches ~2.0 where vanilla topped out at 1.0. Twice the energy, summed over five pyramid
// levels by a screen blend, is what turned the vanilla fallback's glow into wide halos.
//
// Both halves of the clamp are therefore restored, against the SAME paper-white anchor the composite blends at
// (PaperWhiteOnCanvas, game-local Common.hlsl). Clamping only here is enough for the whole chain: every later level
// is a weighted average of this output, so bounding the source bounds the pyramid.
//
// Note the ceiling belongs to the OUTPUT, not to the taps -- vanilla deliberately let the taps exceed 1 (that is
// what the alpha side-channel is for) and clipped only what it stored.

// clang-format off
#include "Includes/DgVoodoo.hlsl" // b3/b4 + ApplyDgvMask: the wrapper contract, one copy for every replacement
#include "Includes/Common.hlsl" // game-local: LumaGameSettings before the shared Settings.hlsl
// clang-format on

// HDR / vanilla, shared with the tonemap replacement.
#ifndef TONEMAP_TYPE
#define TONEMAP_TYPE 1
#endif

// .x is the bloom threshold, subtracted from the averaged taps.
#define BloomThreshold PsConstants[8].x

SamplerState CanvasSampler_s : register(s0);
Texture2D<float4> CanvasTexture : register(t0);

// One tap: the canvas RGB, clipped to the paper-white anchor, weighted by the overbright hint in its own alpha.
//
// Both halves are the vanilla contract and neither works without the other. `alpha * 8` recovers `max(rawMax, 1)`,
// the pre-tonemap overbright the 8-bit canvas could not hold -- and being exactly 1 below scene white, it is what
// made the vanilla glow SELECTIVE: mid-tones contribute their canvas value and nothing more, so they never clear
// the 0.9 threshold, and only real overbright sources bloom. The clip is the other half: vanilla's canvas was
// UNORM, so the RGB entering this multiply was bounded at diffuse white no matter how bright the source was.
//
// Feeding the raw fp16 canvas here instead puts the display map's output into a chain calibrated for a clipped
// one. Everything HDR lifted past 1.0 -- a wide band immediately below diffuse white, not just the highlights --
// starts clearing the threshold, five pyramid levels integrate it, and the result is a full-frame veil rather than
// a glow. With both halves restored this is arithmetically the vanilla pass at W = 1.
float3 SampleWeighted(float2 uv, float ceiling)
{
   float4 tap = ApplyDgvMask(CanvasTexture.Sample(CanvasSampler_s, uv), DgvMaskScene, DgvFillScene);
   return min(tap.rgb, ceiling) * (tap.w * 8.0);
}

// Full 13-entry interpolator layout, declared in order even where unread: linkage is by REGISTER. This pass reads
// TEXCOORD0 and TEXCOORD1, which carry the four tap UVs packed as xy/zw pairs.
void main(
    float4 v0 : SV_POSITION0,
    float4 v1 : TEXCOORD8,
    float4 v2 : COLOR0,
    float4 v3 : COLOR1,
    float4 v4 : TEXCOORD9,
    float4 v5 : TEXCOORD0,
    float4 v6 : TEXCOORD1,
    float4 v7 : TEXCOORD2,
    float4 v8 : TEXCOORD3,
    float4 v9 : TEXCOORD4,
    float4 v10 : TEXCOORD5,
    float4 v11 : TEXCOORD6,
    float4 v12 : TEXCOORD7,
    out float4 o0 : SV_TARGET0)
{
#if TONEMAP_TYPE >= 1
   // The Luma pyramid replaces this glow entirely (Luma_MOH_Tonemap.hlsl adds it in linear scene light, before the
   // display map). Writing 0 here empties the whole downstream pyramid, so the composite has nothing to add and
   // there is no double glow — cheaper and more honest than gating five later passes.
   if (LumaSettings.GameSettings.LumaBloomEnable > 0.5)
   {
      o0 = float4(0.0, 0.0, 0.0, 0.0);
      return;
   }
#endif

#if TONEMAP_TYPE >= 1
   const float W = PaperWhiteOnCanvas();
#else
   const float W = 1.0; // the vanilla UNORM canvas, reproduced literally for the parity gate
#endif

   // Accumulated in the original's order; the sum is not associative in floating point.
   float3 sum = SampleWeighted(v5.zw, W);
   sum += SampleWeighted(v5.xy, W);
   sum += SampleWeighted(v6.xy, W);
   sum += SampleWeighted(v6.zw, W);

   const float3 bright = sum * 0.25 - BloomThreshold;

#if TONEMAP_TYPE >= 1
   // Same range the 8-bit target produced, expressed against the canvas' paper-white anchor (= 1.0 whenever UI and
   // Game Paper White match, i.e. bit-identical to vanilla in the default configuration).
   o0.xyz = clamp(bright, 0.0, W);
#else
   // Vanilla reference: emulate the 8-bit target the original wrote to, so the parity gate stays honest.
   o0.xyz = saturate(bright);
#endif
   o0.w = 0.0; // the original writes a constant 0
}
