// clang-format off
#include "Includes/Common.hlsl" // game-local: LumaGameSettings before the shared Settings.hlsl, keep FIRST
#include "Includes/DoF.hlsl"
// clang-format on

// Bayonetta "filter08_3" (dgVoodoo 2.87.5 -> ps_5_0 0x2B93FD4F), mode 3 (mode 1 with the exe's DAT_050a5de4 set, g_Param2.y its
// edge scale DAT_050a5dcc): filter08_1's gather, and in focus the plain average A of its 16 taps, blended in at depth edges (the
// centre's depth term against the inner taps' extremes): colour lerp(A, gather, CoC), alpha lerp(edge, 1, CoC). See
// "Includes/DoF.hlsl".
void main(DGV_MAIN_SIGNATURE)
{
   const DoFContext context = MakeDoFContext(v5);
   const DoFRing ring = GatherRing(context, cb4[194].x);
   const float centre_depth_term = DepthTerm(context, v5.xy);
   const float inner_max = max(max(ring.inner_depth_terms.x, ring.inner_depth_terms.y), max(ring.inner_depth_terms.z, ring.inner_depth_terms.w));
   const float inner_min = min(min(ring.inner_depth_terms.x, ring.inner_depth_terms.y), min(ring.inner_depth_terms.z, ring.inner_depth_terms.w));
   const float edge = saturate(max(abs(centre_depth_term - inner_max), abs(centre_depth_term - inner_min)) * cb4[194].y);
   const float coc = CircleOfConfusion(centre_depth_term);
   o0 = float4(lerp(ring.color_sum / 16.0, ResolveGather(context, ring.weighted), coc), lerp(edge, 1.0, coc)) * cb4[192];
}
