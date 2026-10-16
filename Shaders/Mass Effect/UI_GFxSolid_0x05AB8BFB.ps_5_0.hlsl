// Mass Effect (2007) - GFx vertex-colour fill (676 draws per frame in the docking-bay elevator). See Includes/GFxUI.hlsl:
// vanilla body plus the 8-bit canvas clamp.
#include "Includes/GFxUI.hlsl"

// 13 interpolators, all declared in order - linkage is by REGISTER (see Luma_ME1_Tonemap.hlsl). This shader reads only
// COLOR0 (v2).
void main(DGV_MAIN_SIGNATURE)
{
   o0 = saturate(GFxCxform(saturate(v2)));
}
