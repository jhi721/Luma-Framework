// Bayonetta bloom source "filter05" (raw D3D9 ps_3_0 blob, CRC32 0x84049DCA =
// dgVoodoo 2.87.5 0x8B6A8D61), native D3D9 port of
// the former dgVoodoo mod's BloomSource_0x8B6A8D61.ps_5_0.hlsl. All values are gamma
// encoded R'G'B' (the game's scene), not linear. Vanilla: one bilinear tap per
// bloom target texel, rgb * 2, alpha = (a * c184.x + Y' - c184.y) * c184.w, Y'
// = the game's weighted sum of encoded rgb (0.2989/0.5866/0.1145), i.e. luma,
// not relative luminance. Luma Bloom chain (g_LumaBloom.y = 1): the mean of the
// outputs of the source texels under the target texel; the host passes the
// footprint (source texels per target texel, what the dgVoodoo port got from
// ddx/ddy of the UV). Each tap is clamped as the 8-bit scene stored it; the
// output is clamped as the 8-bit target stores it (the bloom stays vanilla
// bounded). Native registers: c184 g_MatrialColor, Luma block c208..c223
// (Luma_Block_sm3.hlsli). Texture size in c210.xy.
#include "Includes/Luma_Block_sm3.hlsli"

sampler2D Texture : register(s0);
float4 g_MatrialColor : register(c184); // x alpha weight, y threshold, w scale

static const float3 kLumaWeights = float3(0.298911989, 0.586610973, 0.114478);

float4 BloomSource(float4 sample)
{
   const float4 scene = saturate(sample);
   return saturate(float4(scene.rgb + scene.rgb,
                          (scene.a * g_MatrialColor.x +
                           dot(scene.rgb, kLumaWeights) - g_MatrialColor.y) *
                              g_MatrialColor.w));
}

float4 main(float2 uv : TEXCOORD0) : COLOR0
{
   float4 result;
   [branch] if (g_LumaBloom.y > 0.5)
   {
      // The mean of the clamped outputs, not the output of the mean: the
      // threshold's floor at 0 would let a dark part cancel a bright one
      const float2 taps = clamp(ceil(g_LumaBloom.zw), 1.0, 16.0);
      const float2 step = g_LumaBloom.zw / (taps * g_LumaTex.xy);
      const float2 first = uv - 0.5 * (taps - 1.0) * step;
      float4 sum = 0.0;
      [loop] for (int y = 0; y < 16; y++)
      {
         if (y >= taps.y)
            break;
         [loop] for (int x = 0; x < 16; x++)
         {
            if (x >= taps.x)
               break;
            sum += BloomSource(
                tex2Dlod(Texture, float4(first + float2(x, y) * step, 0.0, 0.0)));
         }
      }
      result = sum / (taps.x * taps.y);
   }
   else
   {
      result = BloomSource(tex2D(Texture, uv));
   }
   return result;
}
