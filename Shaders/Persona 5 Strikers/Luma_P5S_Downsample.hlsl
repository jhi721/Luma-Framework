// The upscaler's output scaled back down into the render resolution scene, for the post passes that read it there (DOF, bloom, exposure),
// as the DLAA path's copy back does. Bilinear, so a 2x ratio averages each 2x2 block. Drawn as a 4 vertex triangle strip.
Texture2D<float4> source : register(t0);
SamplerState linearSampler : register(s0);

void vs_main(uint vertex_id : SV_VertexID, out float4 pos : SV_Position, out float2 uv : TEXCOORD0)
{
   uv = float2(vertex_id & 1, vertex_id >> 1);
   pos = float4(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0, 0.0, 1.0);
}

float4 ps_main(float4 pos : SV_Position, float2 uv : TEXCOORD0) : SV_Target
{
   return source.SampleLevel(linearSampler, uv, 0);
}
