// Bayonetta "filter08_4" (raw ps_3_0 CRC32 0xCB1D0471 = dgVoodoo 2.87.5
// 0x40D84AAB), the DoF gather's 3x3 variant (mode 4, k = g_Param2.x, which the
// exe doubles for this mode): 8 taps at (+-k, +-k), (+-k, 0) and (0, +-k)
// kernel pixels. See Includes/DoF_sm3.hlsli.
#include "Includes/DoF_sm3.hlsli"

float4 main(float4 v0 : TEXCOORD0) : COLOR0
{
   const DoFContext context = MakeDoFContext(v0);
   const float k = g_Param2.x;
   const float2 offsets[8] = {float2(-k, -k), float2(-k, k), float2(k, -k),
                              float2(k, k), float2(-k, 0.0), float2(k, 0.0),
                              float2(0.0, -k), float2(0.0, k)};
   float4 weighted = 0.0;
   [unroll] for (int i = 0; i < 8; i++)
   {
      const float2 uv = v0.xy + offsets[i] * context.kernel_texel;
      const float weight = CircleOfConfusion(DepthTerm(context, uv));
      weighted += float4(weight * FootprintColor(context, uv), weight);
   }
   return float4(ResolveGather(context, weighted),
                 CircleOfConfusion(DepthTerm(context, v0.xy))) *
          g_MatrialColor;
}
