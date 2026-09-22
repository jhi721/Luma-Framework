// ps_render_fb (sh_ogre3_w64.par). See Includes/RenderFB.hlsl.
#include "Includes/RenderFB.hlsl"

void main(float2 v0 : TEXCOORD0, out float4 o0 : SV_Target0)
{
   o0 = RenderFB(v0);
}
