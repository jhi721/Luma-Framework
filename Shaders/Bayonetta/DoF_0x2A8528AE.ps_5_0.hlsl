// clang-format off
#include "Includes/Common.hlsl" // game-local: LumaGameSettings before the shared Settings.hlsl, keep FIRST
#include "Includes/DoF.hlsl"
// clang-format on

// Bayonetta "filter08_2" (dgVoodoo 2.87.5 -> ps_5_0 0x2A8528AE), mode 2 (mode 1 with the exe's DAT_050a5de4 set, g_Param2.y its
// edge scale DAT_050a5dcc): filter08_1's gather, and in focus the average A of a 1 kernel pixel cross, blended in by local contrast:
// colour lerp(A, gather, CoC), alpha lerp(1 - contrast, 1, CoC). See "Includes/DoF.hlsl".
void main(DGV_MAIN_SIGNATURE)
{
   const DoFContext context = MakeDoFContext(v5);
   const DoFRing ring = GatherRing(context, cb4[194].x);
   const float3 cross_average = (FootprintColor(context, v5.xy + float2(-1.0, 0.0) * context.kernel_texel) + FootprintColor(context, v5.xy + float2(1.0, 0.0) * context.kernel_texel) +
                                 FootprintColor(context, v5.xy + float2(0.0, -1.0) * context.kernel_texel) + FootprintColor(context, v5.xy + float2(0.0, 1.0) * context.kernel_texel)) *
                                0.25;
   const float contrast = saturate(length(FootprintColor(context, v5.xy) - cross_average) * cb4[194].y * 16.0);
   const float coc = CircleOfConfusion(DepthTerm(context, v5.xy));
   o0 = float4(lerp(cross_average, ResolveGather(context, ring.weighted), coc), lerp(1.0 - contrast, 1.0, coc)) * cb4[192];
}
