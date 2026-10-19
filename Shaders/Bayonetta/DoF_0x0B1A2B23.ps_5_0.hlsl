// clang-format off
#include "Includes/Common.hlsl" // game-local: LumaGameSettings before the shared Settings.hlsl, keep FIRST
#include "Includes/DoF.hlsl"
// clang-format on

// Bayonetta "filter08_0" (dgVoodoo 2.87.5 -> ps_5_0 0x0B1A2B23), the DoF's plain variant (mode 0): the average of 4 taps at
// (+-k / 2, +-k / 2) - (0.5, 0.5) kernel pixels (k = g_Param2.x), no depth weights; alpha is the centre's CoC. See
// "Includes/DoF.hlsl".
void main(DGV_MAIN_SIGNATURE)
{
   const DoFContext context = MakeDoFContext(v5);
   const float half_k = cb4[194].x * 0.5;
   float3 color_sum = 0.0;
   [unroll] for (int i = 0; i < 4; i++)
   {
      const float2 sign = float2((i & 1) ? 1.0 : -1.0, (i & 2) ? 1.0 : -1.0);
      color_sum += FootprintColor(context, v5.xy + (sign * half_k - 0.5) * context.kernel_texel);
   }
   o0 = float4(color_sum * 0.25, CircleOfConfusion(DepthTerm(context, v5.xy))) * cb4[192];
}
