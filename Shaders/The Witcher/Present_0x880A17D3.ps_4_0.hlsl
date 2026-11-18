// clang-format off
#include "Includes/Common.hlsl" // game-local: LumaGameSettings before the shared Settings.hlsl, keep FIRST
#include "../Includes/CInfinityRolloff.hlsl"
// clang-format on

// The Witcher EE: dgVoodoo 2.87.3's present blit (ps_4_0 0x880A17D3), the canvas copied 1:1 into the swapchain after the UI.
// The game has no tonemap: the fp16 canvas holds the gamma-encoded SDR image (1 = paper white, gamma 2.2 display) with the
// unclipped highlights of the lighting and the glow above 1. Every frame goes through this blit whatever post effects the Lua
// chain ran, so the HDR display map lives here: linearize, C-infinity max-channel rolloff to the peak (BL2's, Bayonetta's), encode
// back for the Core composition. The rolloff leaves paper white within 0.2% at a 1000 nit peak, so the UI passes through.

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
         ApplyOutputDithering(o0.rgb, uv, true, false, 1.0);
      }
      return;
   }
   float3 color = IsNaN_Strict(o0.rgb) ? 0.0 : max(o0.rgb, 0.0);
   // Linear BT.709, 1 = paper white; the peak can't go below paper white (the rolloff needs it above its 0.18 anchor)
   const float paperWhite = max(GamePaperWhiteNits, 1.0);
   color = Tonemap::CInfinityRollOff::MaxChannel(gamma_to_linear(color, GCT_MIRROR), max(PeakWhiteNits, paperWhite) / paperWhite, 0.18, 1.0);
   // Anti-banding dither, one step of the output quantizer: 10-bit BT.2020 PQ at the game paper white the composition applies (ME1)
   [branch] if (LumaSettings.GameSettings.Dithering > 0.5)
   {
      ApplyOutputDithering(color, uv, false, true, GamePaperWhiteNits);
   }
   o0.rgb = linear_to_gamma(color, GCT_MIRROR);
}
