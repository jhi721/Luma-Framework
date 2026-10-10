// Mirror's Edge: the upscaler's output back into the scene and its copy (the game's resolve of it) in one pass, instead of a draw into
// the scene and a CopyResource of the whole scene into the copy (main.cpp "DrawUpscaler"). The blend state's RGB write mask keeps both
// targets' alpha, the linear view depth the post passes read. Borderlands 2's.
Texture2D<float4> source : register(t0);

void main(float4 pos : SV_Position, out float4 scene : SV_Target0, out float4 scene_copy : SV_Target1)
{
   scene = source.Load(int3(pos.xy, 0));
   scene_copy = scene;
}
