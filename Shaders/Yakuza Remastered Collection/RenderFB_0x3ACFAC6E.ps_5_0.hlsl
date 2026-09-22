// ps_render_fb (sh_ogre3_w64.par): the screen fade/tint, the last pass before the present blit. Vanilla multiplies in
// linear 2.2 space: pow(pow(x, 2.2) * tint, 1/2.2). On the fp16 chain its input can exceed 1 or carry small negatives
// (bicubic resample ringing), and pow() of a negative is NaN, so the transfer mirrors the sign.
#include "Includes/Common.hlsl"

cbuffer cb5 : register(b5)
{
   float4 cb5[1];
}

SamplerState s0_s : register(s0);
Texture2D<float4> t0 : register(t0);

void main(float2 v0 : TEXCOORD0, out float4 o0 : SV_Target0)
{
   float4 color = t0.Sample(s0_s, v0.xy);
   o0.rgb = linear_to_gamma(gamma_to_linear(color.rgb, GCT_MIRROR) * cb5[0].rgb, GCT_MIRROR);
   o0.a = color.a;

   // Anti-banding dither on the last pass (after CAS and the HUD), one step of the output quantizer: the 8-bit code in
   // SDR, 10-bit BT.2020 PQ in HDR. The composition scales this image by UIPaperWhite, so that is the PQ scale.
   [branch] if (LumaSettings.GameSettings.Dithering > 0.5)
   {
      if (LumaSettings.DisplayMode == 0)
         ApplyDithering(o0.rgb, v0.xy, true, 1.0, 8u, LumaSettings.FrameIndex, true);
      else
      {
         const float pqScale = max(LumaSettings.UIPaperWhiteNits, 1.0) / HDR10_MaxWhiteNits;
         float3 pq = Linear_to_PQ(BT709_To_BT2020(gamma_to_linear(o0.rgb, GCT_MIRROR) * pqScale), GCT_MIRROR);
         ApplyDithering(pq, v0.xy, true, 1.0, 10u, LumaSettings.FrameIndex, true);
         o0.rgb = linear_to_gamma(BT2020_To_BT709(PQ_to_Linear(pq, GCT_MIRROR)) / pqScale, GCT_MIRROR);
      }
   }
}
