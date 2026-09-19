// Mass Effect 3 (2012) - Devastator Mode feedback material (DLC Resurgence, Devastator_FB_Mat). Additive with no
// scene term, so only the saturate is restored (vanilla's UNORM target clamp). See Includes/FXMaterial.hlsl.
#include "Includes/FXMaterial.hlsl"

void main(DGV_MAIN_SIGNATURE)
{
   const float3 band = FX_DarkBand(FX_Scene(FX_ScreenUV(v5.xy * 1.5 - 0.6)), 80.0, 10.0, 50.0, 200.0);
   const float mask = ApplyDgvMask(Tex1.Sample(Sampler1, v5.xy), 1).y;
   o0 = float4(saturate((PsConstants[11].x * band * float3(50.0, 0.0, 0.0) * mask + PsConstants[8].xyz) * v9.w), 0.0);
}
