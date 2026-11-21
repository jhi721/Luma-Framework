// clang-format off
#include "Includes/Common.hlsl" // game-local: LumaGameSettings before the shared Settings.hlsl, keep FIRST
#include "Includes/DisplayMap.hlsl"
// clang-format on

// Dead Space 2: the HDR display map at the scene's end, before what draws over it (menus, RIG holograms, video logs), when MarkerPatch's
// antialiasing slot doesn't draw there (an upscaler, its AA off): as the SMAA neighborhood and FXAA replacements do in that slot.

Texture2D<float4> source : register(t0); // A copy of the frame

float4 main(float4 position : SV_Position) : SV_Target
{
   float4 color = source.Load(int3(position.xy, 0));
   uint2 size;
   source.GetDimensions(size.x, size.y);
   color.rgb = DisplayMapCanvas(color.rgb, position.xy / size);
   return color;
}
