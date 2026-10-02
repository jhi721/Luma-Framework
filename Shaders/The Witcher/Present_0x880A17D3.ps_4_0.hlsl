// clang-format off
#include "Includes/Common.hlsl" // game-local: LumaGameSettings before the shared Settings.hlsl, keep FIRST
#include "../Includes/DICE.hlsl"
// clang-format on

// The Witcher EE: dgVoodoo 2.87.3's present blit (ps_4_0 0x880A17D3), the canvas copied 1:1 into the swapchain after the UI.
// The game has no tonemap: the fp16 canvas holds the gamma-encoded SDR image (1 = paper white, gamma 2.2 display) with the
// unclipped highlights of the lighting and the glow above 1. Every frame goes through this blit whatever post effects the Lua
// chain ran, so the HDR display map lives here: linearize, DICE by luminance to the peak, encode back for the Core composition.
// The shoulder starts at a third of the peak, above the UI's paper white range, so the UI passes through.

Texture2D<float4> t0 : register(t0); // the canvas

void main(float4 v0 : SV_POSITION0, float2 v1 : TEXCOORD0, out float4 o0 : SV_TARGET0)
{
   o0 = t0.Load(int3(int2(v1), 0));
   o0.a = saturate(o0.a); // additive blends pushed the unclamped canvas alpha up to 5; the unorm swapchain kept it in range
   uint2 size;
   t0.GetDimensions(size.x, size.y);
   const float2 uv = v0.xy / size;
   if (LumaSettings.DisplayMode < 1) // SDR: the composition clips like the vanilla 8-bit swapchain
   {
      // Anti-banding dither, one step of the output quantizer (the 8-bit code)
      [branch] if (LumaSettings.GameSettings.Dithering > 0.5)
      {
         ApplyDithering(o0.rgb, uv, true, 1.0, 8u, LumaSettings.FrameIndex, true);
      }
      return;
   }
   float3 color = IsNaN_Strict(o0.rgb) ? 0.0 : max(o0.rgb, 0.0);
   const float paperWhite = GamePaperWhiteNits / sRGB_WhiteLevelNits;
   const float peakWhite = PeakWhiteNits / sRGB_WhiteLevelNits;
   const DICESettings settings = DefaultDICESettings(DICE_TYPE_BY_LUMINANCE_PQ_CORRECT_CHANNELS_BEYOND_PEAK_WHITE);
   color = DICETonemap(gamma_to_linear(color, GCT_MIRROR) * paperWhite, peakWhite, settings) / paperWhite;
   // Anti-banding dither, one step of the output quantizer: 10-bit BT.2020 PQ at the game paper white the composition applies (ME1)
   [branch] if (LumaSettings.GameSettings.Dithering > 0.5)
   {
      const float pqScale = GamePaperWhiteNits / HDR10_MaxWhiteNits;
      float3 pq = Linear_to_PQ(BT709_To_BT2020(color * pqScale), GCT_MIRROR);
      ApplyDithering(pq, uv, true, 1.0, 10u, LumaSettings.FrameIndex, true);
      color = BT2020_To_BT709(PQ_to_Linear(pq, GCT_MIRROR)) / pqScale;
   }
   o0.rgb = linear_to_gamma(color, GCT_MIRROR);
}
