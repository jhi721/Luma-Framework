// Luma's injected pixel shader constants for the Bayonetta native D3D9 ports:
// c208..c223 (ps_3_0 has 224 float registers, the game's highest device read is
// c202, SSAO). The host writes whichever a draw's shader reads
// (SetPixelShaderConstantF) before the draw. Unread declarations are dropped by
// fxc, so including this costs nothing. All floats: the ports use no integer or
// bool constants.
#ifndef LUMA_BLOCK_SM3_HLSLI
#define LUMA_BLOCK_SM3_HLSLI

// x = encoded gain on g_MatrialColor: pow(Exposure, 1 / 2.2), 1 = vanilla
// (Grade_*, CentreAverage)
float4 g_LumaGrade : register(c208);

// x = copy mode (0 vanilla, 2 bloom composite, 3 composite with a lerp blend),
// y = 1 if t0 is Luma Bloom's texture (B-spline), z = BloomIntensity
float4 g_LumaCopy : register(c209);

// xy = t0 size in texels (Copy B-spline, BloomSource, bloom blurs' tent)
float4 g_LumaTex : register(c210);

// x = blur tent scale k (1 = vanilla), y = 1 for BloomSource's footprint
// average, zw = source texels per target texel
float4 g_LumaBloom : register(c211);

// xy = kernel target size in pixels (the 720p-scaled unit with Fix High
// Resolution Effects, else = c194.xy)
float4 g_LumaKernel : register(c212);

// xy = shadow map (R32F) size in texels
float4 g_LumaShadow : register(c213);

// x = Video AutoHDR enable (0/1), y = AutoHDRMaxWhite = max(min(lerp(80, 250,
// boost) / 80, PeakWhiteNits / UIPaperWhiteNits), 1)
float4 g_LumaVideo : register(c214);

#endif
