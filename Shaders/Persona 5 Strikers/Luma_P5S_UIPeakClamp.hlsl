// The display's peak in swapchain units (relative to UI paper white), MIN blended under additive UI that exceeded it
#include "Includes/Common.hlsl"

float4 main() : SV_Target
{
   const float peak = LumaSettings.DisplayMode == 1 ? LumaSettings.PeakWhiteNits / max(LumaSettings.UIPaperWhiteNits, 1.0) : 1.0;
   return float4(peak, peak, peak, 1.0);
}
