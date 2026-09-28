// Mass Effect 3 (2012) - UE3 FGammaCorrectionPixelShader: the encode after a gamma-less uber perm and the feedback
// material effects. Vanilla: pow(saturate(lerp(c * ColorScale, Overlay.rgb, Overlay.a)), InverseGamma), alpha kept.
// HDR output skips the clamp and uses linear_to_gamma; both paths end in ME3_FinalEncode.
#define ME3_UBER_NO_MAIN
#include "Luma_ME3_Tonemap.hlsl"

void main(DGV_MAIN_SIGNATURE)
{
   const float4 scene = FETCH(0, v5.xy);
   const float3 c = lerp(scene.rgb * PsConstants[8].rgb, PsConstants[11].rgb, PsConstants[11].a);
   const float3 encoded = (TONEMAP_TYPE >= 1 && LumaSettings.DisplayMode == 1) ? linear_to_gamma(max(c, 0.0), GCT_POSITIVE) : PowUE3(saturate(c), PsConstants[12].xxx);
   o0 = float4(ME3_FinalEncode(encoded, v5.xy).rgb, scene.a);
}
