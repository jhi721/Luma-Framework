// Bayonetta "filter08_3" (raw ps_3_0 CRC32 0xFC2A113C = dgVoodoo 2.87.5
// 0x2B93FD4F), mode 3: filter08_1's gather, and in focus the plain average A of
// its 16 taps, blended in at depth edges (the centre's depth term against the
// inner taps' extremes, g_Param2.y the edge scale): colour lerp(A, gather,
// CoC), alpha lerp(edge, 1, CoC). See Includes/DoF_sm3.hlsli.
#include "Includes/DoF_sm3.hlsli"

float4 main(float4 v0 : TEXCOORD0) : COLOR0
{
   const DoFContext context = MakeDoFContext(v0);
   const DoFRing ring = GatherRing(context, g_Param2.x);
   const float centre_depth_term = DepthTerm(context, v0.xy);
   const float edge = saturate(max(abs(centre_depth_term - ring.inner_max),
                                   abs(centre_depth_term - ring.inner_min)) *
                               g_Param2.y);
   const float coc = CircleOfConfusion(centre_depth_term);
   return float4(lerp(ring.color_sum / 16.0,
                      ResolveGather(context, ring.weighted), coc),
                 lerp(edge, 1.0, coc)) *
          g_MatrialColor;
}
