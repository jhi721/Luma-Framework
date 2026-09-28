// rl_prim_2d_s_01: the untextured vint UI (solid rects, fades). Vanilla drew it onto the 8-bit UNORM swapchain, which clamped its
// output to [0,1] before blending; the output is clamped to [0,1] as that target did (see "UI_0x901E0D91.ps_4_0.hlsl").

cbuffer vc4 : register(b4)
{
   float4 Tint_color : packoffset(c1);
   float Alpha_test_ref : packoffset(c17);
}

void main(float4 pos : SV_Position, float4 color : COLOR0, out float4 o0 : SV_Target0)
{
   if (color.a * Tint_color.a - Alpha_test_ref < 0.0)
      discard;
   o0 = saturate(color * Tint_color);
}
