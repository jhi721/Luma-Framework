// ResetRender's display map as a D3D12 pass over the canvas (d9passes.cpp): the
// HDR path of Bayonetta's Copy_0x91A6C437
// ("ResetRenderDisplayMap()"), on the whole canvas right after the scene was
// copied into it (the letterbox bars are 0, which the map keeps). LumaSettings
// at b13 as root constants (Core's CB::LumaGlobalSettings, DEVELOPMENT 0),
// built with Bayonetta's defines (build.ps1).
// clang-format off
#include "Includes/Common.hlsl"
#include "Includes/DisplayMap.hlsl"
// clang-format on

Texture2D<float4> canvas : register(t0);

float4 main(float4 position : SV_Position) : SV_Target
{
   float4 color = canvas.Load(int3(position.xy, 0));
   // SDR: the 8-bit canvas clipped the scene before the UI blended on it
   if (LumaSettings.DisplayMode != 1)
   {
      color.rgb = saturate(color.rgb);
      return color;
   }
   color.rgb = ResetRenderDisplayMap(color.rgb);
   return color;
}
