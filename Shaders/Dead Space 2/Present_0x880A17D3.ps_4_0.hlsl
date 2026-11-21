// clang-format off
#include "Includes/Common.hlsl" // game-local: LumaGameSettings before the shared Settings.hlsl, keep FIRST
#include "Includes/DisplayMap.hlsl"
// clang-format on

// Dead Space 2: dgVoodoo 2.87.6's present blit (ps_4_0 0x880A17D3), the frame copied 1:1 into the swapchain after the UI. The display
// map normally ran where the scene ended (MarkerPatch's SMAA neighborhood or FXAA replacement, or Luma_DS2_DisplayMap before the
// UI; LumaData.CustomData1 != 0 here): then this only keeps the fp16 frame in range. Otherwise the whole frame, UI included, is mapped
// here.

Texture2D<float4> t0 : register(t0); // The frame
Texture2D<float4> t1 : register(t1); // The frame right after the display map: the UNORM clamp's bound for what drew after it

void main(float4 v0 : SV_POSITION0, float2 v1 : TEXCOORD0, out float4 o0 : SV_TARGET0)
{
   o0 = t0.Load(int3(int2(v1), 0));
   o0.a = saturate(o0.a); // Blends pushed the fp16 frame's alpha up to 2; the unorm swapchain kept it in range
   [branch] if (LumaData.CustomData1 != 0)
   {
      o0.rgb = IsNaN_Strict(o0.rgb) ? 0.0 : max(o0.rgb, 0.0);
      // HDR: vanilla blended the draws after the display map (additive particles, holograms) into its UNORM frame, clamped at 1 per
      // blend. For non-negative additive chains clamping the end result is the same; the bound keeps the scene's highlights.
      [branch] if (LumaSettings.DisplayMode == 1 && LumaData.CustomData4 != 0)
      {
         uint2 size0;
         uint2 size1;
         t0.GetDimensions(size0.x, size0.y);
         t1.GetDimensions(size1.x, size1.y);
         // SSAA maps on its 2x target. ponytail: point load, take the max of the block if SSAA edges show
         const int2 coord = int2(v1 * size1 / size0);
         // One bound for all channels: per channel, white light over a scene highlight above 1 in only some channels turned pink
         const float3 bound = t1.Load(int3(coord, 0)).rgb;
         o0.rgb = min(o0.rgb, max(max(bound.r, max(bound.g, bound.b)), 1.0));
      }
      // SDR: the unorm swapchain clipped the UI drawn after the display map
      if (LumaSettings.DisplayMode != 1)
      {
         o0.rgb = saturate(o0.rgb);
      }
      return;
   }
   uint2 size;
   t0.GetDimensions(size.x, size.y);
   o0.rgb = DisplayMapCanvas(o0.rgb, v0.xy / size);
}
