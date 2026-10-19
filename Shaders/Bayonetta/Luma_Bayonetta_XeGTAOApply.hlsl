// Bayonetta: XeGTAO's visibility applied to the scene, drawn with the game's SSAO draw (its vertex shader, target and t0/t2) in place of
// its pixel shader ("filtershaderssao"). That shader multiplied the scene by its AO where the depth prepass' "1 - z/w" is at least
// 0.005 (the sky stays as it is) and kept the scene's alpha. All the textures are the target's size.

#include "Includes/GameCBuffers.hlsl"

Texture2D<float4> scene : register(t0);
Texture2D<float> prepass_depth : register(t2);
Texture2D<float> ambient_visibility : register(t3); // XeGTAO's final output (Luma_Bayonetta_XeGTAO.hlsl), bound by main.cpp

// Mirrored by "GTAO_KNOBS_CB_SLOT" (main.cpp)
cbuffer LumaGTAO : register(b9)
{
   CB::GTAOKnobs gtao_knobs;
}

float4 main(float4 pos : SV_Position) : SV_Target
{
   const int3 pixel = int3(pos.xy, 0);
   const float4 color = scene.Load(pixel);
#if DEVELOPMENT
   // XeGTAO's debug views alone, not multiplied by the scene
   if (gtao_knobs.debug_view > 0.5)
   {
      return float4(ambient_visibility.Load(pixel).xxx, color.a);
   }
#endif
   const float visibility = (prepass_depth.Load(pixel) >= 0.005) ? ambient_visibility.Load(pixel) : 1.0;
   return float4(color.rgb * visibility, color.a);
}
