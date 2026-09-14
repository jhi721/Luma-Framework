// Material-less frames (squad, galaxy map): the material's display map + encode, drawn on a copy of the uber output.
#include "Luma_ME2_Tonemap.hlsl"

float4 display_map_ps(float4 pos : SV_Position) : SV_Target
{
   float4 c = MatSceneTexture.Load(int3(pos.xy, 0));
#if TONEMAP_TYPE >= 1
   c.rgb = Sanitize(EncodeME2Canvas(c.rgb, true));
#endif
   return c;
}
