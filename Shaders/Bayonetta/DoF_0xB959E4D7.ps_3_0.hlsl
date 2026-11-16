// Bayonetta "filter08_1" (raw ps_3_0 CRC32 0xB959E4D7 = dgVoodoo 2.87.5
// 0x58C43DE9), the cutscenes' DoF gather (mode 1): the 16 tap ring (k =
// g_Param2.x, 0.6 in the chapter 1 cutscene). See Includes/DoF_sm3.hlsli.
#include "Includes/DoF_sm3.hlsli"

float4 main(float4 v0 : TEXCOORD0) : COLOR0
{
   const DoFContext context = MakeDoFContext(v0);
   const DoFRing ring = GatherRing(context, g_Param2.x);
   return float4(ResolveGather(context, ring.weighted),
                 CircleOfConfusion(DepthTerm(context, v0.xy))) *
          g_MatrialColor;
}
