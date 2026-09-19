// Mass Effect 3 (2012) - Adrenaline Rush feedback material (Adrenaline_FB_Mat). See Includes/FXMaterial.hlsl.
#include "Includes/FXMaterial.hlsl"

void main(DGV_MAIN_SIGNATURE)
{
   o0 = FX_Whiteout(v5.xy, v10, PsConstants[16].w, PsConstants[17].x, PsConstants[17].y, float3(-0.249, -0.245, -0.24), 3.0, false);
}