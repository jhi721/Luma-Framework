// The rl_hdr final composites, one per hash (Core defines "_<HASH>" for the one being built):
// rl_hdr_09 [final] (0xED6DDA24): bloom, the 1.49 shoulder and the colour LUT (PostProcess 1/2).
// rl_hdr_10 [final_no_lut] (0xD742C62C): rl_hdr_09 without the colour LUT, ending in pow(1/2.2).
// rl_hdr_08 [final_diffracted] (0x966367E4): rl_hdr_09 with the Signal_noise channel split.
// rl_hdr_06 [final_no_lut_diffracted] (0x9F1F6557): rl_hdr_10 with the Signal_noise channel split.
// rl_hdr_05 [no_tonemapping] (0xC235DDDD): the final composite with post processing off (PostProcess 0).
#include "Luma_SR4_Tonemap.hlsl"

void main(float4 pos : SV_Position, float2 texcoord0 : TEXCOORD0, float4 texcoord1 : TEXCOORD1, out float4 o0 : SV_Target0)
{
#if _C235DDDD
   o0 = SR4_TonemapNoPost(texcoord0, texcoord1);
#elif _ED6DDA24
   o0 = SR4_TonemapFinal(texcoord0, texcoord1, false, true);
#elif _D742C62C
   o0 = SR4_TonemapFinal(texcoord0, texcoord1, false, false);
#elif _966367E4
   o0 = SR4_TonemapFinal(texcoord0, texcoord1, true, true);
#else // _9F1F6557
   o0 = SR4_TonemapFinal(texcoord0, texcoord1, true, false);
#endif
}
