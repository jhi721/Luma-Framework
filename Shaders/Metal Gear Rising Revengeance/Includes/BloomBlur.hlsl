#ifndef MGRR_BLOOM_BLUR_HLSL
#define MGRR_BLOOM_BLUR_HLSL

// MGR:R bloom blurs "filter02" (V, BlurV_0xE454B49F) and "filter01" (H, BlurH_0xBBDF2A11) (shader.dat, dgVoodoo 2.87.5 -> ps_5_0),
// transcribed from the translated disassembly: 5 bilinear taps along one axis at 0, +-1.5 and +-3.5 texels of the whole 320x176
// bloom target, as constant UV offsets, weights 1 / 0.8 / 0.45 (sum 3.5).
// In Luma Bloom's chain (main.cpp "DrawBloomPass", LumaData.CustomData3 = its scale k) the source is k times finer: the same UV
// offsets land k times more texels apart, and the bilinear filter that filled the gaps between the taps at the game's size covers
// a k-th of it (a comb). Each tap is a tent over one game texel (2k - 1 texels) instead, as the game's bilinear tap was.

cbuffer cb3 : register(b3)
{
   float4 cb3[77]; // dgVoodoo state: [44]/[45] t0's masks
}

cbuffer cb4 : register(b4)
{
   float4 cb4[236]; // D3D9 constants at c<N> = cb4[N + 8]
}

Texture2D<float4> t0 : register(t0);
SamplerState s0_s : register(s0);

float4 BlurTap(float2 uv, float2 texelStep, int scale)
{
   float4 tap;
   [branch] if (scale > 1)
   {
      tap = 0.0;
      for (int i = 1 - scale; i < scale; i++)
      {
         tap += ApplyDgvMask(t0.SampleLevel(s0_s, uv + i * texelStep, 0.0), cb3[44], cb3[45]) * (scale - abs(i));
      }
      tap /= scale * scale;
   }
   else
   {
      tap = ApplyDgvMask(t0.Sample(s0_s, uv), cb3[44], cb3[45]);
   }
   return tap;
}

// The taps' weighted mean (the game's * 2 / 7); "nearOffset" / "farOffset" are the 1.5 and 3.5 texel offsets along the axis
float4 BloomBlur(float2 uv, float2 nearOffset, float2 farOffset)
{
   const int scale = int(LumaData.CustomData3 + 0.5);
   float2 size;
   t0.GetDimensions(size.x, size.y);
   const float2 texelStep = (nearOffset.x != 0.0 ? float2(1.0 / size.x, 0.0) : float2(0.0, 1.0 / size.y));
   float4 sum = BlurTap(uv, texelStep, scale);
   sum += BlurTap(uv + farOffset, texelStep, scale) * 0.45;
   sum += BlurTap(uv + nearOffset, texelStep, scale) * 0.8;
   sum += BlurTap(uv - farOffset, texelStep, scale) * 0.45;
   sum += BlurTap(uv - nearOffset, texelStep, scale) * 0.8;
   return sum * (2.0 / 7.0);
}

#endif // MGRR_BLOOM_BLUR_HLSL
