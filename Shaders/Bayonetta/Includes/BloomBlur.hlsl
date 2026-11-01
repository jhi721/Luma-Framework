#ifndef BAYONETTA_BLOOM_BLUR_HLSL
#define BAYONETTA_BLOOM_BLUR_HLSL

// Bayonetta bloom blurs "filter02" (V, BlurV_0x91CC01D3) and "filter01" (H, BlurH_0xCE62BA4B) (D3D9 ps_3_0, dgVoodoo 2.87.5 ->
// ps_5_0), transcribed from the translated disassembly: 5 bilinear taps along one axis at 0, +-7.5 and +-15 times 1 / c202.x (the
// bloom target's 320 width, on both axes) of UV, weights 1 / 0.8 / 0.45 (sum 3.5); out rgb = mean.rgb * mean.a * c192.rgb, alpha c192.w.
// In Luma Bloom's chain (main.cpp "DrawBloomPass", LumaData.CustomData3 = its scale k) the source is k times finer: the same UV
// offsets land k times more texels apart, and each bilinear tap, which covered one texel of the game's source, covers a k-th of it
// (a comb). Each tap is a tent over one game texel (2k - 1 texels) instead, as the game's bilinear tap was.

cbuffer cb3 : register(b3)
{
   float4 cb3[77]; // dgVoodoo state: [44]/[45] t0's masks
}

cbuffer cb4 : register(b4)
{
   float4 cb4[236]; // D3D9 constants: c192 (output scale, alpha), c202.x (the tap unit's inverse)
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

// "axis" is (1, 0) for H, (0, 1) for V
float4 BloomBlur(float2 uv, float2 axis)
{
   const int scale = int(LumaData.CustomData3 + 0.5);
   float2 size;
   t0.GetDimensions(size.x, size.y);
   const float2 texelStep = axis / size;
   const float unit = (abs(cb4[202].x) > 0.0 ? 1.0 / cb4[202].x : 1e37);
   float4 sum = BlurTap(uv, texelStep, scale);
   sum += BlurTap(uv + axis * (15.0 * unit), texelStep, scale) * 0.45;
   sum += BlurTap(uv + axis * (7.5 * unit), texelStep, scale) * 0.8;
   sum += BlurTap(uv - axis * (15.0 * unit), texelStep, scale) * 0.45;
   sum += BlurTap(uv - axis * (7.5 * unit), texelStep, scale) * 0.8;
   const float4 mean = sum * (2.0 / 7.0);
   return float4(mean.rgb * mean.a * cb4[192].rgb, cb4[192].w);
}

#endif // BAYONETTA_BLOOM_BLUR_HLSL
