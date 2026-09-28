// ps_glow_pass0 (sh_ogre3_w64.par). See Includes/GlowPass0.hlsl.
#include "Includes/GlowPass0.hlsl"

void main(float4 v0 : COLOR0, float2 v1 : TEXCOORD0, out float4 o0 : SV_Target0)
{
   o0 = GlowPass0(v1);
}
