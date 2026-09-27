// rl_prim_2d_tex_s_01: the textured vint UI (HUD, menus). Vanilla drew it onto the 8-bit UNORM swapchain, which clamped its output
// to [0,1] before blending; Luma's is FP16, where a tint above 1 (hud_btnmash.lua passes 255) or an additive element went on
// unclamped. The output is saturated as the UNORM target did; the rest is the original.

cbuffer vc1 : register(b1)
{
   float2 Prim_tex_texel_size : packoffset(c8);
}

cbuffer vc4 : register(b4)
{
   float2 Tint_saturation : packoffset(c0);
   float4 Tint_color : packoffset(c1);
   float Alpha_test_ref : packoffset(c17);
}

SamplerState Diffuse_MapSampler : register(s0);
Texture2D<float4> Diffuse_MapTexture : register(t0);

void main(float4 pos : SV_Position, float2 texcoord : TEXCOORD0, float4 color : COLOR0, out float4 o0 : SV_Target0)
{
   float4 tex = Diffuse_MapTexture.Sample(Diffuse_MapSampler, texcoord + Prim_tex_texel_size);
   const float luma = dot(float3(0.3, 0.59, 0.11), tex.rgb);
   tex.rgb = Tint_saturation.x * (tex.rgb - luma) + luma;
   tex *= color;
   if (tex.a * Tint_color.a - Alpha_test_ref < 0.0)
      discard;
   o0 = saturate(tex * Tint_color);
}
