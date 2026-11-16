// Bayonetta "filter08_0" (raw ps_3_0 CRC32 0xEA10C34E = dgVoodoo 2.87.5
// 0x0B1A2B23), the DoF's plain variant (mode 0): the average of 4 taps at (+-k
// / 2, +-k / 2) - (0.5, 0.5) kernel pixels (k = g_Param2.x), no depth weights;
// alpha is the centre's CoC. See Includes/DoF_sm3.hlsli.
#include "Includes/DoF_sm3.hlsli"

float4 main(float4 v0 : TEXCOORD0) : COLOR0
{
   const DoFContext context = MakeDoFContext(v0);
   const float half_k = g_Param2.x * 0.5;
   float3 color_sum = 0.0;
   [unroll] for (int i = 0; i < 4; i++)
   {
      const float2 sign = float2((frac(i * 0.5) > 0.25) ? 1.0 : -1.0, (i > 1.5) ? 1.0 : -1.0);
      color_sum += FootprintColor(context, v0.xy + (sign * half_k - 0.5) *
                                                       context.kernel_texel);
   }
   return float4(color_sum * 0.25,
                 CircleOfConfusion(DepthTerm(context, v0.xy))) *
          g_MatrialColor;
}
