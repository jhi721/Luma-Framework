// Mass Effect 3 (2012) - GFx multiply-blend vertex-colour fill, extra alpha COLOR1.w. See Includes/GFxUI.hlsl:
// vanilla body plus the alpha clamp.
#include "Includes/GFxUI.hlsl"

void main(DGV_MAIN_SIGNATURE_CENTROID)
{
   o0 = GFxMultiplyOutput(v2, v3.w);
}
