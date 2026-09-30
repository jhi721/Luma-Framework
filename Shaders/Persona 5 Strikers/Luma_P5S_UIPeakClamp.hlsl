// The display's peak in swapchain units (relative to UI paper white), MIN blended under additive UI that exceeded it
#include "Includes/Common.hlsl"

float4 main() : SV_Target
{
   return float4(P5S_UIPeak().xxx, 1.0);
}
