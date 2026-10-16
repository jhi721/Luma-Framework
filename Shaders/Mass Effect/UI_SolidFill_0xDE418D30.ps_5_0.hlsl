// Mass Effect (2007) - constant-colour fill (GFx solid / UE3 one-colour quad: fades, letterbox). Seen on the canvas
// after the gamma pass with blending OFF, so it overwrites the HDR canvas. See Includes/GFxUI.hlsl: vanilla body plus
// the 8-bit canvas clamp.
#include "Includes/GFxUI.hlsl"

// 13 interpolators, all declared in order - linkage is by REGISTER (see Luma_ME1_Tonemap.hlsl). This shader reads no
// interpolator.
void main(DGV_MAIN_SIGNATURE)
{
   o0 = saturate(GFxSolidColor);
}
