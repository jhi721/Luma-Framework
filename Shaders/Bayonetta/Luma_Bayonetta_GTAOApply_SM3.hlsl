// The former dgVoodoo mod's Luma_Bayonetta_XeGTAOApply.hlsl on D3D9: XeGTAO's visibility applied to the
// scene in place of the game's SSAO draw
// ("filtershaderssao"), which multiplied the scene by its AO where the depth
// prepass' "1 - z/w" is at least 0.005 (the sky stays as it is) and kept the
// scene's alpha. All the textures are the target's size; pixels from VPOS,
// point sampled.

sampler2D scene : register(s0);
sampler2D prepass_depth : register(s2);
sampler2D ambient_visibility
    : register(s3);         // XeGTAO's R32F output (d9passes.cpp)
float4 size : register(c0); // width, height

float4 main(float2 vpos : VPOS) : COLOR0
{
   const float4 uv = float4((floor(vpos) + 0.5) / size.xy, 0.0, 0.0);
   const float4 color = tex2Dlod(scene, uv);
   const float visibility = (tex2Dlod(prepass_depth, uv).r >= 0.005)
                                ? tex2Dlod(ambient_visibility, uv).r
                                : 1.0;
   return float4(color.rgb * visibility, color.a);
}
