// ps_ccr________________ (Y5R sh_devil_w64.par): the passthrough grade, byte-identical to ps_texture_a255 (a copy with
// alpha 1) in every game. Only the draw the addon flags (LumaData.CustomData1, the swapchain-sized Y5R grade) is graded.
#define CCR_HLS            0
#define CCR_SC             0
#define CCR_GM             0
#define CCR_GI             0
#define CCR_OF             0
#define CCR_MASK           0
#define CCR_COLLECTION     0
#define CCR_MATERIAL_CURVE 1
#include "Includes/ColorCorrect.hlsl"

void main(float2 v0 : TEXCOORD0, out float4 o0 : SV_Target0)
{
   if (LumaData.CustomData1 == 0u)
      o0 = float4(t0.Sample(s0_s, v0).rgb, 1.0);
   else
      o0 = ColorCorrect(1.0, float4(v0, 0.0, 0.0));
}
