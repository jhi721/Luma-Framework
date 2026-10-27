// The scene the upscaler reads, from a copy of it (main.cpp, "DrawUpscaler"): DLSS and FSR keep a history, so a NaN or an infinity in
// their input would stay in it for good. NaNs go to 0, infinities to the largest float16. Drawn with an RGB write mask: the scene's
// alpha is game data (the glow and the grade read it) and stays as it is.
#include "../Includes/Math.hlsl" // IsNaN_Strict

Texture2D<float4> scene_copy : register(t0);

float4 main(float4 pos : SV_Position) : SV_Target0
{
   const float3 color = scene_copy.Load(int3(pos.xy, 0)).rgb;
   return float4(IsNaN_Strict(color) ? 0.0 : clamp(color, -65504.0, 65504.0), 0.0);
}
