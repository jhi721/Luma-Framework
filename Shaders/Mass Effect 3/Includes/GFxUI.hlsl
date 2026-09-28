#ifndef LUMA_ME3_GFX_UI
#define LUMA_ME3_GFX_UI

// Scaleform GFx HUD shaders, dgVoodoo transcription. The only change is the alpha clamp: vanilla RGB already ends with
// exp_sat, but alpha leaves unclamped, and the 8-bit UNORM canvas used to clamp it before blending. On the fp16 canvas
// a cxform multiplier > 1 gives alpha > 1 and the SrcAlpha blend extrapolates (ME1 2007 measured ~10k nits).

#include "GameBindings.hlsl"

#define GFxCxformMul PsConstants[8]
#define GFxCxformAdd PsConstants[11]
#define GFxGamma     PsConstants[12] // .x exponent applied to the colour

// Flash colour transform (alpha included); `alphaScale` is the extra per-vertex alpha factor some perms multiply in.
float4 GFxCxform(float4 color, float alphaScale)
{
   float4 c = color * GFxCxformMul + GFxCxformAdd;
   c.a *= alphaScale;
   return c;
}

// The colour's pow(c, gamma) with the original 1e-6 floor and exp_sat, plus the alpha clamp.
float4 GFxEncode(float4 c)
{
   return float4(saturate(exp2(log2(max(abs(c.rgb), 1e-6)) * GFxGamma.x)), saturate(c.a));
}

float4 GFxOutput(float4 color, float alphaScale)
{
   return GFxEncode(GFxCxform(color, alphaScale));
}

// Multiply-blend perms fade the cxformed colour toward white by its own alpha, all four channels (alpha a^2 - a + 1).
float4 GFxMultiplyFade(float4 c)
{
   return c.a * (c - 1.0) + 1.0;
}
float4 GFxMultiplyOutput(float4 color, float alphaScale)
{
   return GFxEncode(GFxMultiplyFade(GFxCxform(color, alphaScale)));
}

// sRGB perms: colour clamped, then a fixed 1/2.2 instead of the gamma row.
float4 GFxSRGBOutput(float4 c)
{
   return float4(exp2(log2(max(saturate(c.rgb), 1e-6)) * (1.0 / 2.2)), saturate(c.a));
}

#endif // LUMA_ME3_GFX_UI
