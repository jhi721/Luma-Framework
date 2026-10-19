// clang-format off
#include "Includes/Common.hlsl" // game-local: LumaGameSettings before the shared Settings.hlsl, keep FIRST
#include "Includes/DoF.hlsl"
// clang-format on

// Bayonetta "filter08_1" (dgVoodoo 2.87.5 -> ps_5_0 0x58C43DE9), the cutscenes' DoF gather (mode 1): the 16 tap ring ("GatherRing",
// k = g_Param2.x, 0.6 in the chapter 1 cutscene). See "Includes/DoF.hlsl".
void main(DGV_MAIN_SIGNATURE)
{
   const DoFContext context = MakeDoFContext(v5);
   const DoFRing ring = GatherRing(context, cb4[194].x);
   o0 = float4(ResolveGather(context, ring.weighted), CircleOfConfusion(DepthTerm(context, v5.xy))) * cb4[192];
}
