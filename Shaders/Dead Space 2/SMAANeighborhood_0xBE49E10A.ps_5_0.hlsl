// clang-format off
#include "Includes/Common.hlsl" // game-local: LumaGameSettings before the shared Settings.hlsl, keep FIRST
#include "Includes/DisplayMap.hlsl"
// clang-format on
#include "../Includes/DgVoodoo.hlsl"

// Dead Space 2: MarkerPatch's SMAA 1x neighborhood blending (ImprovedAntiAliasingMode 2/4, dgVoodoo 2.87.6 -> ps_5_0 0xBE49E10A), the
// frame's last pass before the UI (RIG holograms, menus, video logs draw into its output next). Transcribed from the translation, then
// the HDR display map (Includes/DisplayMap.hlsl). MarkerPatch blends in linear (SRGBTEXTURE in, SRGBWRITEENABLE out): the fp16 mirrors
// dropped that pair, so the sRGB decode and encode are done here. With an upscaler (LumaData.CustomData1 != 0) the frame is already
// antialiased and the blending weights weren't drawn: only the display map runs.

cbuffer cb3 : register(b3)
{
   float4 cb3[77]; // dgVoodoo state: [44]/[45] t0's masks, [46]/[47] t1's
}

cbuffer cb4 : register(b4)
{
   float4 cb4[236]; // D3D9 constants at c<N> = cb4[N + 8]: c0.xy SMAA_RT_METRICS (1 / size)
}

Texture2D<float4> t0 : register(t0); // The frame (MarkerPatch's copy of it)
Texture2D<float4> t1 : register(t1); // The blending weights
SamplerState s0_s : register(s0);
SamplerState s1_s : register(s1);

float4 SampleColor(float2 uv)
{
   const float4 color = ApplyDgvMask(t0.SampleLevel(s0_s, uv, 0.0), cb3[44], cb3[45]);
   return float4(gamma_sRGB_to_linear(color.rgb, GCT_MIRROR), color.a);
}

// v5.xy = texcoord, v6 = SMAA's offset (right texel's .x weight at xy, bottom texel's .y weight at zw)
void main(DGV_MAIN_SIGNATURE)
{
   float4 color;
   [branch] if (LumaData.CustomData1 != 0)
   {
      color = SampleColor(v5.xy);
   }
   else
   {
      float4 weights;
      weights.x = ApplyDgvMask(t1.Sample(s1_s, v6.xy), cb3[46], cb3[47]).w;
      weights.y = ApplyDgvMask(t1.Sample(s1_s, v6.zw), cb3[46], cb3[47]).y;
      weights.wz = ApplyDgvMask(t1.Sample(s1_s, v5.xy), cb3[46], cb3[47]).xz;
      [branch] if (dot(weights, 1.0) < 1e-5)
      {
         color = SampleColor(v5.xy);
      }
      else
      {
         // The larger of the horizontal and vertical blends, as "SMAANeighborhoodBlendingPS"
         const bool horizontal = max(weights.x, weights.z) > max(weights.y, weights.w);
         float4 blendingOffset = horizontal ? float4(weights.x, 0.0, weights.z, 0.0) : float4(0.0, weights.y, 0.0, weights.w);
         float2 blendingWeight = horizontal ? weights.xz : weights.yw;
         blendingWeight *= DgVoodooRcp(dot(blendingWeight, 1.0));
         const float4 blendingCoord = blendingOffset * float4(cb4[8].xy, -cb4[8].xy) + v5.xyxy;
         color = blendingWeight.x * SampleColor(blendingCoord.xy) + blendingWeight.y * SampleColor(blendingCoord.zw);
      }
   }
   o0 = float4(linear_to_sRGB_gamma(color.rgb, GCT_MIRROR), color.a);
   // LumaData.CustomData3: the display map runs elsewhere (Luma_DS2_DisplayMap already ran, or runs later before the UI, or the present
   // blit maps the frame): only frames with the uber post map here
   [branch] if (LumaData.CustomData3 == 0)
   {
      o0.rgb = DisplayMapCanvas(o0.rgb, v0.xy * cb4[8].xy);
   }
}
