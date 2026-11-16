// Bayonetta "filter08_2" (raw ps_3_0 CRC32 0x548C7346 = dgVoodoo 2.87.5
// 0x2A8528AE), mode 2: filter08_1's gather, and in focus the average A of a 1
// kernel pixel cross, blended in by local contrast (|centre - A| in encoded
// R'G'B', g_Param2.y its edge scale): colour lerp(A, gather, CoC), alpha lerp(1
// - contrast, 1, CoC). See Includes/DoF_sm3.hlsli.
#include "Includes/DoF_sm3.hlsli"

float4 main(float4 v0 : TEXCOORD0) : COLOR0
{
   const DoFContext context = MakeDoFContext(v0);
   const DoFRing ring = GatherRing(context, g_Param2.x);
   const float3 cross_average =
       (FootprintColor(context,
                       v0.xy + float2(-1.0, 0.0) * context.kernel_texel) +
        FootprintColor(context,
                       v0.xy + float2(1.0, 0.0) * context.kernel_texel) +
        FootprintColor(context,
                       v0.xy + float2(0.0, -1.0) * context.kernel_texel) +
        FootprintColor(context,
                       v0.xy + float2(0.0, 1.0) * context.kernel_texel)) *
       0.25;
   const float contrast =
       saturate(length(FootprintColor(context, v0.xy) - cross_average) *
                g_Param2.y * 16.0);
   const float coc = CircleOfConfusion(DepthTerm(context, v0.xy));
   return float4(lerp(cross_average, ResolveGather(context, ring.weighted), coc),
                 lerp(1.0 - contrast, 1.0, coc)) *
          g_MatrialColor;
}
