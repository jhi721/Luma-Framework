// The canvas to the display (see "GammaCanvasDisplayMap"), pre-scaled for the UI paper white. SDR displays keep the vanilla canvas.
// Dead Space 1/2: where the scene ends, before the UI (Luma_DS_DisplayMap; Dead Space 2's SMAA neighborhood and FXAA replacements), or
// in the present blit when none of them ran. Dead Space 3: in the final display pass (the UI layer goes over it).
#ifndef LUMA_DS_DISPLAY_MAP
#define LUMA_DS_DISPLAY_MAP

#include "../../Includes/GammaCanvasDisplayMap.hlsl" // GammaCanvasDisplayMap, EncodeGammaCanvas

// "canvas": gamma encoded, 1 = paper white (gamma 2.2 display, VANILLA_ENCODING_TYPE 1)
float3 DisplayMapCanvas(float3 canvas, float2 uv)
{
   canvas = IsNaN_Strict(canvas) ? 0.0 : canvas;
#if !GAME_DEAD_SPACE_3
   // The game's brightness slider, per channel as its gamma ramp (identity at 1); what draws after the map (UI, video) is left out.
   // Dead Space 3 has none.
   canvas = sign(canvas) * pow(abs(canvas), LumaSettings.GameSettings.GammaRampExponent);
#endif
#if GAME_DEAD_SPACE_1
#if DEVELOPMENT
   // The uber A/B's suggested route (DS1_Uber) maps to the display itself, the exposure before its Reinhard
   bool display_ready = LumaSettings.DisplayMode == 1 && LumaSettings.DevSetting01 >= 2.0 / 3.0 && LumaData.CustomData2 != 0;
#else
   bool display_ready = false;
#endif
   float3 color = gamma_to_linear(max(0.0, canvas)) * (display_ready ? 1.0 : LumaSettings.GameSettings.Exposure);
   [branch] if (display_ready)
   {
      // No-op: its Reinhard already mapped to the peak
   }
   else if (LumaSettings.DisplayMode == 1)
#else
   // The plain form, not "display_ready = false": fxc schedules the folded one differently in Dead Space 2's SMAA neighborhood
   float3 color = gamma_to_linear(max(0.0, canvas)) * LumaSettings.GameSettings.Exposure;
   [branch] if (LumaSettings.DisplayMode == 1)
#endif
   {
      // LumaData.CustomData2: the uber post drew this frame, its LUT tail already turned the hue as the vanilla clip did
      color = GammaCanvasDisplayMap(color, LumaSettings.GameSettings.Contrast, LumaSettings.GameSettings.Saturation, LumaSettings.GameSettings.HighlightsDesaturation,
                                    LumaData.CustomData2 != 0 ? 0.0 : 1.0);
   }
   else
   {
      // SDR: the 8 bit canvas (Dead Space 3: swapchain) clipped the frame before the UI blended on it
      color = saturate(color);
   }
   return EncodeGammaCanvas(color, uv, LumaSettings.GameSettings.Dithering > 0.5);
}

#endif // LUMA_DS_DISPLAY_MAP
