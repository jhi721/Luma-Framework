// Bayonetta "filtershadercopytex" (raw ps_3_0 CRC32 0xB8690130 =
// dgVoodoo 2.87.5 0x91A6C437): t0 * c184, the game's copy for bloom levels, the
// bloom composite and the scene to the back buffer. Gamma encoded R'G'B', 1 =
// paper white; native D3D9 port of the composite part of
// the former dgVoodoo mod's Copy_0x91A6C437.ps_5_0.hlsl. The ResetRender display map is
// a D3D11 pass (mode 0/1 here: the plain copy). The host sets g_LumaCopy per
// draw: x = 0 vanilla copy, 2 bloom composite (SRC_ALPHA, ONE: alpha scaled by
// BloomIntensity z), 3 composite with a lerp blend (alpha scaled by
// min(BloomIntensity, 1)); y = 1 when t0 is Luma Bloom's texture (cubic
// B-spline magnification instead of the bilinear one's creases, source size in
// g_LumaTex.xy). Alpha is clamped as the 8-bit targets did (additive blends
// read it). 0xA3E80995 ("espshadermultiparticle*", the same code) needs no
// replacement: the host never sets the mode for it.
#include "Includes/Luma_Block_sm3.hlsli"

sampler2D Texture : register(s0);
float4 g_MatrialColor : register(c184);

// Cubic B-spline in 4 bilinear taps (Sigg and Hadwiger, "Fast Third-Order
// Texture Filtering")
float4 SampleBSpline(float2 uv)
{
   const float2 size = g_LumaTex.xy;
   const float2 grid = uv * size - 0.5;
   const float2 index = floor(grid);
   const float2 f = grid - index;
   const float2 g = 1.0 - f;
   const float2 w0 = g * g * g / 6.0;
   const float2 w1 = 2.0 / 3.0 - 0.5 * f * f * (2.0 - f);
   const float2 w2 = 2.0 / 3.0 - 0.5 * g * g * (2.0 - g);
   const float2 w3 = f * f * f / 6.0;
   const float2 g0 = w0 + w1;
   const float2 h0 = (index + w1 / g0 - 0.5) / size;
   const float2 h1 = (index + w3 / (w2 + w3) + 1.5) / size;
   const float4 t00 = tex2Dlod(Texture, float4(h0, 0.0, 0.0));
   const float4 t10 = tex2Dlod(Texture, float4(h1.x, h0.y, 0.0, 0.0));
   const float4 t01 = tex2Dlod(Texture, float4(h0.x, h1.y, 0.0, 0.0));
   const float4 t11 = tex2Dlod(Texture, float4(h1, 0.0, 0.0));
   return lerp(lerp(t11, t01, g0.x), lerp(t10, t00, g0.x), g0.y);
}

float4 main(float2 uv : TEXCOORD0) : COLOR0
{
   float4 result = tex2D(Texture, uv);
   if (g_LumaCopy.y > 0.5)
   {
      result = SampleBSpline(uv);
   }
   result *= g_MatrialColor;
   result.a = saturate(result.a);
   if (g_LumaCopy.x > 1.5)
   {
      result.a *= (g_LumaCopy.x < 2.5) ? g_LumaCopy.z : min(g_LumaCopy.z, 1.0);
   }
   return result;
}
