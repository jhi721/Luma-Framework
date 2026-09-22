// ps_glow_pass0 (Y5R sh_devil_w64.par). See Includes/GlowPass0.hlsl.
#include "Includes/GlowPass0.hlsl"

void main(float2 v0 : TEXCOORD0, out float4 o0 : SV_Target0)
{
   o0 = GlowPass0(v0);
}
