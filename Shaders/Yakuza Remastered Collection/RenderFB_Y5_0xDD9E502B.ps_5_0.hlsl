// ps_render_fb (Y5R sh_devil_w64.par): pow(pow(x, 2.2) * cb5[0].x, cb5[0].z / 2.2). See Includes/RenderFB.hlsl.
#define RENDER_FB_SCALE_AND_GAMMA 1
#include "Includes/RenderFB.hlsl"

void main(float2 v0 : TEXCOORD0, out float4 o0 : SV_Target0)
{
   o0 = RenderFB(v0);
}
