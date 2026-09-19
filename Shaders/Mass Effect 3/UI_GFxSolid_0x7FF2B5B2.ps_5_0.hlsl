// Mass Effect 3 (2012) - GFx vertex-colour fill. See Includes/GFxUI.hlsl: vanilla body plus the alpha clamp.
#include "Includes/GFxUI.hlsl"

void main(DGV_MAIN_SIGNATURE_CENTROID)
{
   o0 = GFxOutput(v2, 1.0);
}
