// Mass Effect 3 (2012) - EMP feedback material (DLC Leviathan, EMP_FB_Mat). Additive with no scene term, so only the
// saturate is restored (vanilla's UNORM target clamp; unclamped peak ~17). See Includes/FXMaterial.hlsl.
#include "Includes/FXMaterial.hlsl"

void main(DGV_MAIN_SIGNATURE)
{
   const float2 noise = ApplyDgvMask(Tex1.Sample(Sampler1, v5.xy * 4.0 + PsConstants[11].xy), 1).xy;
   const float3 band = FX_DarkBand(FX_Scene(FX_ScreenUV(noise * 0.01 + v5.xy - 0.002)), 50.0, 20.0, 20.0, 200.0);
   o0 = float4(saturate((band * PsConstants[12].y * float3(2.0, 5.0, 20.0) + PsConstants[8].xyz) * v9.w), 0.0);
}
