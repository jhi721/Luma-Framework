// The game's UI sprite vertex shader (0x8B19022A), for the sprite that composes a 3D layer drawn at the output resolution onto the
// swapchain (the pause screen's): its texture coordinates address the layer's render resolution corner, which the layer now fills whole.
#include "Includes/LayerCorner.hlsl"

cbuffer Globals : register(b0)
{
   float4 texIdx;
   float4 cDiff;
   float4 cAmb;
   float4 vDiffSrc;
   row_major float4x4 mW2P;
   float4 mL2W[192];
};

void main(float4 v0 : POSITION0, float4 v1 : COLOR0, float2 v2 : TEXCOORD0, float2 v3 : TEXCOORD1, float2 v4 : TEXCOORD2, float2 v5 : TEXCOORD3, float v6 : BLENDWEIGHT0, out float4 o0 : SV_Position0, out float4 o1 : TEXCOORD0, out float4 o2 : TEXCOORD1, out float4 o3 : TEXCOORD2)
{
   const int row = int(v6) * 3;
   const float3 world = float3(dot(v0, mL2W[row]), dot(v0, mL2W[row + 1]), dot(v0, mL2W[row + 2]));
   o0 = mul(float4(world, v0.w), mW2P);
   o1 = (v1 * vDiffSrc.x + vDiffSrc.w) * cDiff * cAmb;

   o2.xy = texIdx.x > 0.0 ? v3 : v2;
   o2.zw = texIdx.y > 0.0 ? v3.yx : v2.yx;
   float4 uv = float4(texIdx.z > 0.0 ? v3 : v2, texIdx.w > 0.0 ? v3.yx : v2.yx);
   uv.xy = texIdx.z > 1.0 ? v4 : uv.xy;
   uv.zw = texIdx.w > 1.0 ? v4.yx : uv.zw;
   o3.xy = texIdx.z > 2.0 ? v5 : uv.xy;
   o3.zw = texIdx.w > 2.0 ? v5.yx : uv.zw;

   o2 = float4(LayerCornerToWhole(o2.xy), LayerCornerToWhole(o2.zw));
   o3 = float4(LayerCornerToWhole(o3.xy), LayerCornerToWhole(o3.zw));
}
