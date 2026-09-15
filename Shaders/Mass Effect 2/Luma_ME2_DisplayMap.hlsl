// Material-less frames (squad, galaxy map): the uber output, read from a copy, finished as the material finishes its
// canvas - display map, encode, UI pre-scale, dither.
#include "Luma_ME2_Tonemap.hlsl"

float4 display_map_ps(float4 pos : SV_Position) : SV_Target
{
   float4 c = MatSceneTexture.Load(int3(pos.xy, 0));
#if TONEMAP_TYPE >= 1
   c.rgb = FinishME2Canvas(EncodeME2Canvas(MapME2ToDisplay(c.rgb)), pos.xy * LumaSettings.SwapchainInvSize, true);
#endif
   return c;
}
