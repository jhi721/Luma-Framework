// Medal of Honor (2010) — bloom composite (VS 0x4C89DAEA). Sums the five blurred pyramid levels and blends them
// onto the full-res canvas the tonemap wrote.
//
// Vanilla is a screen blend: out = 1 - (1 - base) * (1 - sum * k), which expands exactly to
//   out = base + sum * k * (1 - base)
// That weight goes NEGATIVE once the canvas carries HDR values, so above 1.0 the pass stops adding glow and starts
// compressing the very highlights the HDR tonemap just recovered (base 2.0 comes out as 1.9). Clamping the weight
// at zero keeps the blend bit-identical for base <= 1 and simply stops it from eating anything brighter.
//
// The second half of the same problem is the CEILING. Vanilla could never exceed 1.0 here because the render
// target was r8g8b8a8_unorm and the ROP clipped; the fp16 upgrade removed that clamp. The bloom term is the sum of
// five pyramid levels of a canvas that now carries HDR, and it is summed in GAMMA space, where a 5x sum becomes a
// ~50x multiplication once decoded — measured 12.19 on the canvas, i.e. 246x paper white, ~50000 nits after the
// display composition decodes it.
//
// Bounding it is not enough on its own: re-scaling the blend to the canvas' peak-white ceiling stretches the curve
// by peak/paper (2.9x in gamma, 22x in linear) and a hard min() then lands every over-bright bloom pixel on
// exactly that ceiling, so the core reads as one flat 2280-nit plateau.
//
// The blend therefore stays ANCHORED AT PAPER WHITE, where vanilla anchored it; whatever the tonemap already put
// above paper white passes through untouched.
//
// One deliberate departure from vanilla remains, and it is a look decision rather than a fidelity one: the bloom
// term is bounded by its MAX CHANNEL instead of per channel. See the call site.
//
// It deliberately stops there. This glow carries NO HDR information — it is a gamma-space SDR effect whose energy
// is an artefact of summing five gamma-encoded levels, and pushing it above diffuse white invents brightness the
// game never had. Measured with the engine's own constants (threshold cb4[8].x = 0.9, amount cb4[8].z = 0.2) the
// term reaches 2.0 in gamma on its own, so an HDR glare term on top lifted a BLACK pixel to ~630 nits. HDR glow
// has a proper owner now — the Luma pyramid, added in linear before the display map — and this path's only job is
// to be a faithful vanilla reference.
//
// The bright pass upstream is fed the vanilla signal for the same reason: the canvas clipped to the paper-white
// anchor, times the `max(rawMax, 1)` overbright hint in its alpha. See BloomBrightPass_0x5FE75452.

// clang-format off
#include "Includes/DgVoodoo.hlsl" // b3/b4 + ApplyDgvMask: the wrapper contract, one copy for every replacement
#include "Includes/Common.hlsl" // game-local: LumaGameSettings before the shared Settings.hlsl
// clang-format on

// HDR / vanilla, shared with the tonemap replacement. 0 = the literal vanilla screen blend.
#ifndef TONEMAP_TYPE
#define TONEMAP_TYPE 1
#endif

// .xy UV scale for the full-res base, .z bloom amount.
#define BloomParams PsConstants[8]

SamplerState BaseSampler_s : register(s0);
SamplerState Level1Sampler_s : register(s1);
SamplerState Level2Sampler_s : register(s2);
SamplerState Level3Sampler_s : register(s3);
SamplerState Level4Sampler_s : register(s4);
SamplerState Level5Sampler_s : register(s5);
Texture2D<float4> BaseTexture : register(t0);
Texture2D<float4> Level1Texture : register(t1);
Texture2D<float4> Level2Texture : register(t2);
Texture2D<float4> Level3Texture : register(t3);
Texture2D<float4> Level4Texture : register(t4);
Texture2D<float4> Level5Texture : register(t5);

// Full 13-entry interpolator layout, declared in order even where unread: linkage is by REGISTER. This pass reads
// only TEXCOORD0, the scene UV in v5.
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
   const float2 uv = v5.xy;

   float3 sum = ApplyDgvMask(Level1Texture.Sample(Level1Sampler_s, uv), DgvMaskLUT, DgvFillLUT).rgb;
   sum += ApplyDgvMask(Level2Texture.Sample(Level2Sampler_s, uv), DgvMaskBlur, DgvFillBlur).rgb;
   sum += ApplyDgvMask(Level3Texture.Sample(Level3Sampler_s, uv), DgvMaskDepth, DgvFillDepth).rgb;
   sum += ApplyDgvMask(Level4Texture.Sample(Level4Sampler_s, uv), DgvConstants[52], DgvConstants[53]).rgb;
   sum += ApplyDgvMask(Level5Texture.Sample(Level5Sampler_s, uv), DgvConstants[54], DgvConstants[55]).rgb;

   // The base is the only tap the original scales the UV of.
   float3 base = ApplyDgvMask(BaseTexture.Sample(BaseSampler_s, uv * BloomParams.xy), DgvMaskScene, DgvFillScene).rgb;

   const float3 bloom = sum * BloomParams.z;

#if TONEMAP_TYPE >= 1
   // The Luma pyramid owns the glow: it was already added in linear scene light before the display map, so this
   // pass is a pass-through. The bright pass upstream writes 0 in that mode, so the blend below already reduces to
   // `base` algebraically; taking the branch avoids evaluating (W - (W - sdrBase)) * ... / W, whose cancellation
   // costs precision on a dark base for no reason. (It is NOT a stale-pyramid guard: the bright pass, the five
   // blur levels and this pass all run in the same frame off the same cbuffer upload.)
   if (LumaSettings.GameSettings.LumaBloomEnable > 0.5)
   {
      o0 = float4(base, 1.0);
      return;
   }

   // The vanilla screen blend on the paper-white part, plus the HDR the tonemap already produced. Bounding both
   // operands at the anchor is what the 8-bit target used to do for free; anything the tonemap put above it passes
   // through untouched, since splitting the base into sdrBase + excess is lossless.
   const float W = PaperWhiteOnCanvas();
   const float3 sdrBase = min(base, W);
   const float3 excess = max(base - W, 0.0);

   // The bloom term is bounded BY ITS MAX CHANNEL, not per channel. Vanilla clipped each channel separately at its
   // UNORM target, and over a large saturated source -- a fire filling the frame -- all three channels reach the
   // ceiling together, the screen blend resolves to exactly W in R, G and B, and the glow turns into a neutral grey
   // veil. Scaling the triplet by W/max keeps the same ceiling on the brightest channel, so not one nit is added,
   // but the ratio between channels survives and an orange fire glows orange. Same fix, same reasoning, as the
   // highlight path in the tonemap: clip the magnitude, never the hue.
   const float bloomMax = max(bloom.r, max(bloom.g, bloom.b));
   const float3 bloomClipped = bloomMax > W ? bloom * (W / max(bloomMax, 1e-6)) : bloom;

   o0.xyz = (W - (W - sdrBase) * (W - bloomClipped) / W) + excess;
#else
   // Vanilla reference: emulate the 8-bit target the original wrote to, so the parity gate stays honest.
   o0.xyz = saturate(1.0 - (1.0 - base) * (1.0 - bloom));
#endif
   o0.w = 1.0; // the original writes a constant 1; this canvas' alpha is not read downstream
}
