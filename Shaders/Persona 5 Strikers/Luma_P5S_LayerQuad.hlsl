// The game's quad vertex shader (0x2B6CA9A0) for the 3D layers drawn at the output resolution (see "DrawLayerAtOutputResolution"):
// its vertices' texture coordinates address the render resolution corner of output sized textures, which the layer now fills whole.
cbuffer Globals : register(b0)
{
   float4 ktglViewport;
   float4 ktglColorScale;
};

cbuffer LayerScale : register(b5)
{
   float2 uvScale; // Output resolution / render resolution
};

void main(float3 position : POSITION, float4 color : COLOR, float2 uv : TEXCOORD, out float4 out_position : SV_Position, out float4 out_color : COLOR, out float2 out_uv : TEXCOORD)
{
   out_position = float4(position.xy * ktglViewport.xy + ktglViewport.zw, position.z, 1.0);
   out_color = color * ktglColorScale;
   out_uv = uv * uvScale;
}
