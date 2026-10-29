// Katana engine PostEffect3 bloom cbuffers: per pass (b3) and per frame (b4)

cbuffer cbBloomLoop : register(b3)
{
   float4 g_vMainTexSize : packoffset(c0); // The source's texel size in .xy, its mip level in .w
   float4 g_vSampleScale : packoffset(c1); // Upsample tent scale in .x (0.5 + the fractional part of the iteration count), max UV in .zw
}

cbuffer cbBloomMain : register(b4)
{
   float4 g_vBloomInfo0 : packoffset(c0); // Threshold curve: threshold - knee, 2 * knee, 0.25 / knee, threshold
   float4 g_vBloomInfo1 : packoffset(c1); // Exposure (< 0 = auto exposure), offset in texels, anti-flicker (the first downsample is then a Karis average), threshold on the max channel (else weighted RGB)
}
