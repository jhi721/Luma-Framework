// Bayonetta bloom blurs "filter02" (V, 0x563F5C20) and "filter01" (H,
// 0x0F24B7EF), native D3D9 ps_3_0 port of
// the former dgVoodoo mod's Includes/BloomBlur.hlsl. Gamma encoded R'G'B' (8-bit bloom
// pyramid values) throughout. Vanilla: 5 bilinear taps along one axis at 0,
// +-7.5 and +-15 times 1 / c194.x (UV units; the bloom target's width on both
// axes), weights 1 / 0.8 / 0.45 (sum 3.5); out rgb = mean.rgb * mean.a *
// c184.rgb, alpha c184.w. Luma Bloom's chain (g_LumaBloom.x = k >= 2, t0 k
// times finer, size in g_LumaTex.xy): each tap becomes a tent over the 2k - 1
// texels one game texel covered (k <= 16).
#include "Luma_Block_sm3.hlsli"

sampler2D Texture : register(s0);
float4 g_MatrialColor : register(c184);
float4 g_TargetSize : register(c194);

float4 BlurTap(float2 uv, float2 texel_step, float scale)
{
   float4 tap = 0.0;
   [branch] if (scale > 1.5)
   {
      [loop] for (int i = 0; i < 32; i++)
      {
         const float offset = i + 1.0 - scale;
         if (offset >= scale)
            break;
         tap += tex2Dlod(Texture, float4(uv + offset * texel_step, 0.0, 0.0)) *
                (scale - abs(offset));
      }
      tap /= scale * scale;
   }
   else
   {
      tap = tex2Dlod(Texture, float4(uv, 0.0, 0.0)); // level 0: the pyramid targets have one mip
   }
   return tap;
}

// "axis" is (1, 0) for H, (0, 1) for V
float4 BloomBlur(float2 uv, float2 axis)
{
   const float scale = floor(g_LumaBloom.x + 0.5);
   const float2 texel_step = axis / g_LumaTex.xy;
   const float unit = 1.0 / g_TargetSize.x;
   float4 sum = BlurTap(uv, texel_step, scale);
   sum += BlurTap(uv + axis * (15.0 * unit), texel_step, scale) * 0.45;
   sum += BlurTap(uv + axis * (7.5 * unit), texel_step, scale) * 0.8;
   sum += BlurTap(uv - axis * (15.0 * unit), texel_step, scale) * 0.45;
   sum += BlurTap(uv - axis * (7.5 * unit), texel_step, scale) * 0.8;
   const float4 mean = sum * (2.0 / 7.0);
   return float4(mean.rgb * mean.a * g_MatrialColor.rgb, g_MatrialColor.w);
}
