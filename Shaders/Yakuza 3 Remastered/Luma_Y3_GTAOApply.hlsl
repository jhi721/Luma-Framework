// Runs as the ASSAO apply draw's pixel shader (0x6A73BA10 swapped out by main.cpp), keeping that draw's full-screen VS,
// viewport and multiply blend onto the scene RT: XeGTAO's final AO (Luma_Y3_XeGTAO.hlsl, full res) in the native output
// shape, AO in rgb and alpha 1.

Texture2D<float> ao : register(t0);

float4 main(float4 pos : SV_Position) : SV_Target
{
   return float4(ao.Load(int3(pos.xy, 0)).xxx, 1.0);
}
