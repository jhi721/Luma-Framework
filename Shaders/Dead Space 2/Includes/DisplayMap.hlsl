// Where the scene ends, before the UI (the SMAA neighborhood and FXAA replacements, Luma_DS2_DisplayMap, or the present blit when
// none of them ran): the canvas to the display (see "GammaCanvasDisplayMap"), pre-scaled for the UI paper white. SDR displays keep the
// vanilla canvas.
#ifndef LUMA_DS2_DISPLAY_MAP
#define LUMA_DS2_DISPLAY_MAP

#include "../../Includes/GammaCanvasDisplayMap.hlsl" // GammaCanvasDisplayMap, EncodeGammaCanvas

// "canvas": gamma encoded, 1 = paper white (gamma 2.2 display, VANILLA_ENCODING_TYPE 1)
float3 DisplayMapCanvas(float3 canvas, float2 uv)
{
   canvas = IsNaN_Strict(canvas) ? 0.0 : canvas;
   // The game's brightness slider, per channel as its gamma ramp (identity at 1); what draws after the map (UI, video) is left out
   canvas = sign(canvas) * pow(abs(canvas), LumaSettings.GameSettings.GammaRampExponent);
   // SDR: the 8 bit canvas clipped the scene before the UI blended on it
   float3 color = gamma_to_linear(LumaSettings.DisplayMode == 1 ? max(0.0, canvas) : saturate(canvas));
   [branch] if (LumaSettings.DisplayMode == 1)
   {
      // LumaData.CustomData2: the uber post drew this frame, its LUT tail already turned the hue as the vanilla clip did
      color = GammaCanvasDisplayMap(color, LumaSettings.GameSettings.Contrast, LumaSettings.GameSettings.Saturation, LumaSettings.GameSettings.HighlightsDesaturation,
                                    LumaData.CustomData2 != 0 ? 0.0 : 1.0);
   }
   return EncodeGammaCanvas(color, uv, LumaSettings.GameSettings.Dithering > 0.5);
}

#endif // LUMA_DS2_DISPLAY_MAP
