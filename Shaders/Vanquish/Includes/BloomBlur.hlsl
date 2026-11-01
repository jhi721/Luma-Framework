#ifndef VANQUISH_BLOOM_BLUR_HLSL
#define VANQUISH_BLOOM_BLUR_HLSL

// Vanquish bloom blurs "filter02" (V, BlurV_0xCC2E3748) and "filter01" (H, BlurH_0x55E02134) (shader.dat, dgVoodoo 2.87.6 -> ps_5_0),
// transcribed from the translated disassembly: 5 bilinear taps along one axis at 0, +-1.5 and +-3.5 texels of the whole 320x176
// bloom target, as constant UV offsets, weights 1 / 0.8 / 0.45 (sum 3.5); out rgb = mean.rgb * mean.a * c184.rgb, alpha c184.w
// (c184 = cb4[192]). Centred on a texel, the taps between texels split their weight: per texel 1 at 0, 0.4 at +-1 and +-2, 0.225 at
// +-3 and +-4.
// In Luma Bloom's chain (main.cpp "DrawBloomPass", LumaData.CustomData3 = its scale k) the source is k times finer: the same taps
// land on a comb of fine texels. That per texel kernel is applied over the fine texels instead, each game texel k of them wide. A
// tent per tap (MGR:R's and Bayonetta's) overlaps its neighbours unevenly: the kernel rises again 1.5 game texels out, a stripe of
// that period across the bloom (measured in game at k 3).

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

// The vanilla weight of the game texel at distance "x" (game texels)
float VanillaWeight(float x)
{
   return (x < 0.5) ? 1.0 : ((x < 2.5) ? 0.4 : ((x < 4.5) ? 0.225 : 0.0));
}

// The taps' weighted mean (the game's * 2 / 7) as the game outputs it; "nearOffset" / "farOffset" are the 1.5 and 3.5 texel offsets
// along the axis
float4 BloomBlur(float2 uv, float2 nearOffset, float2 farOffset)
{
   const int scale = int(LumaData.CustomData3 + 0.5);
   float4 mean;
   [branch] if (scale > 1)
   {
      float2 size;
      t0.GetDimensions(size.x, size.y);
      const float2 texelStep = (nearOffset.x != 0.0 ? float2(1.0 / size.x, 0.0) : float2(0.0, 1.0 / size.y));
      const int reach = int(4.5 * scale);
      float4 sum = 0.0;
      float total = 0.0;
      for (int i = -reach; i <= reach; i++)
      {
         // A fine texel on a game texel boundary (even k) belongs half to each
         const float x = abs(i) / float(scale);
         const float weight = 0.5 * (VanillaWeight(x - 0.001) + VanillaWeight(x + 0.001));
         sum += ApplyDgvMask(t0.SampleLevel(s0_s, uv + i * texelStep, 0.0), cb3[44], cb3[45]) * weight;
         total += weight;
      }
      mean = sum / total;
   }
   else
   {
      float4 sum = ApplyDgvMask(t0.Sample(s0_s, uv), cb3[44], cb3[45]);
      sum += ApplyDgvMask(t0.Sample(s0_s, uv + farOffset), cb3[44], cb3[45]) * 0.45;
      sum += ApplyDgvMask(t0.Sample(s0_s, uv + nearOffset), cb3[44], cb3[45]) * 0.8;
      sum += ApplyDgvMask(t0.Sample(s0_s, uv - farOffset), cb3[44], cb3[45]) * 0.45;
      sum += ApplyDgvMask(t0.Sample(s0_s, uv - nearOffset), cb3[44], cb3[45]) * 0.8;
      mean = sum * (2.0 / 7.0);
   }
   return float4(mean.rgb * mean.a * cb4[192].rgb, cb4[192].w);
}

#endif // VANQUISH_BLOOM_BLUR_HLSL
