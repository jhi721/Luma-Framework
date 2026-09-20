// Mass Effect 2 (2010) UE3 Canvas solid-colour quad: no texture, colour straight from the vertex colour. Menu
// backdrops and full-screen fades. See Includes/CanvasUI.hlsl - vanilla body plus the 8-bit clamp.
#include "Includes/CanvasUI.hlsl"

// This shader reads COLOR0 (v2) only.
void main(ME2_MAIN_SIGNATURE)
{
   o0 = saturate(ME2_CanvasTransform(v2));
}
