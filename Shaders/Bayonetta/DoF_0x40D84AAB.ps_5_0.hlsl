// clang-format off
#include "Includes/Common.hlsl" // game-local: LumaGameSettings before the shared Settings.hlsl, keep FIRST
#include "Includes/DoF.hlsl"
// clang-format on

// Bayonetta "filter08_4" (dgVoodoo 2.87.5 -> ps_5_0 0x40D84AAB), the DoF gather's 3x3 variant (mode 4, k = 2 * g_Param2.x from the
// exe): 8 taps at (+-k, +-k), (+-k, 0) and (0, +-k) kernel pixels. See "Includes/DoF.hlsl".
void main(DGV_MAIN_SIGNATURE)
{
   const DoFContext context = MakeDoFContext(v5);
   const float k = cb4[194].x;
   const float2 offsets[8] = {float2(-k, -k), float2(-k, k), float2(k, -k), float2(k, k), float2(-k, 0.0), float2(k, 0.0), float2(0.0, -k), float2(0.0, k)};
   float4 weighted = 0.0;
   [unroll] for (int i = 0; i < 8; i++)
   {
      const float2 uv = v5.xy + offsets[i] * context.kernel_texel;
      const float weight = CircleOfConfusion(DepthTerm(context, uv));
      weighted += float4(weight * FootprintColor(context, uv), weight);
   }
   o0 = float4(ResolveGather(context, weighted), CircleOfConfusion(DepthTerm(context, v5.xy))) * cb4[192];
}
