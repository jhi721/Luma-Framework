// 1 where a subtracting UI sprite subtracts anything (vertex color times stage 0, weighed by alpha), else the fp16 max (see "main.cpp")
#include "../Includes/Math.hlsl"

cbuffer SpriteGlobals : register(b0)
{
   float nStageNum;
}

SamplerState stage0Sampler : register(s0);
Texture2D<float4> stage0Texture : register(t0);

float4 main(float4 pos : SV_Position, float4 color : TEXCOORD0, float4 uv : TEXCOORD1) : SV_Target0
{
   float4 sprite = color;
   if ((int)nStageNum > 0)
      sprite *= stage0Texture.Sample(stage0Sampler, uv.xy);
   return max3(sprite.rgb) * sprite.a > 1.0 / 255.0 ? 1.0 : FLT16_MAX;
}
