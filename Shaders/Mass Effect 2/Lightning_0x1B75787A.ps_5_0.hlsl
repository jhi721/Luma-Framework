// Mass Effect 2 (2010) - Overlord lightning feedback material (Lightning_FB_Mat). See Includes/FXMaterial.hlsl.
#include "Includes/FXMaterial.hlsl"

void main(ME2_MAIN_SIGNATURE)
{
   o0 = FX_SquareFlash(v5.xy, v10, 3, FXC(2).xyz);
}
