// Mass Effect (2007) - GFx vertex-colour fill with a second per-vertex alpha (COLOR1.w). See Includes/GFxUI.hlsl:
// vanilla body plus the 8-bit canvas clamp.
#include "Includes/GFxUI.hlsl"

// 13 interpolators, all declared in order - linkage is by REGISTER (see Luma_ME1_Tonemap.hlsl). This shader reads
// COLOR0 (v2) and COLOR1.w (v3.w).
void main(DGV_MAIN_SIGNATURE)
{
   float4 color = GFxCxform(saturate(v2));
   color.w *= saturate(v3.w);
   o0 = saturate(color);
}
