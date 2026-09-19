// Mass Effect 3 (2012) - Krogan Berserk feedback material (KroganBerserk_FB_Mat). See Includes/FXMaterial.hlsl.
#include "Includes/FXMaterial.hlsl"

void main(DGV_MAIN_SIGNATURE)
{
   o0 = FX_Whiteout(v5.xy, v10, PsConstants[16].y, PsConstants[16].z, PsConstants[16].w, float3(-0.249, -0.248, -0.247), 1.5, true);
}