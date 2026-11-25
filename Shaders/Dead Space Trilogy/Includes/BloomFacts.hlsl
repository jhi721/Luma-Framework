// Dead Space 2 and 3 (the same dgVoodoo translations): bloom.fx's passes that read the canvas (bb_texture, D3D9 CTAB "bloom_facts"), transcribed from their dgVoodoo
// translations. Each tap is saturated first: the canvas was UNORM in vanilla, its fp16 mirror goes above 1, and these passes'
// "sat(tap - bloom_dcolor)" then grew up to 1 / (1 - threshold) times stronger (bright, blotchy bloom). Variants:
// BLOOM_LOW_HIGH: 0.5 + (highs - lows) * weight (the low/high pass, D3D9 c1 bloom_blackcolor), else highs * weight (the bright pass);
// BLOOM_MODULATE: the weight is also scaled by modulate_sampler (s1) at bloom_facts.zw.

#include "../../Includes/DgVoodoo.hlsl"
#include "Common.hlsl"

cbuffer cb3 : register(b3)
{
   float4 cb3[77]; // dgVoodoo state: [44]/[45] t0's masks, [46]/[47] t1's
}

cbuffer cb4 : register(b4)
{
   float4 cb4[236]; // D3D9 constants at c<N> = cb4[N + 8]
}

Texture2D<float4> t0 : register(t0); // bb_texture: the canvas
SamplerState s0_s : register(s0);
#if BLOOM_MODULATE
Texture2D<float4> t1 : register(t1); // modulate_sampler
SamplerState s1_s : register(s1);
#endif

#if BLOOM_LOW_HIGH
// c0 bloom_dcolor (rgb threshold, w weight), c1 bloom_blackcolor, c2 bloom_facts (xy tap step, zw modulate uv), c3 g_halfPixelOffset
static const float4 bloom_dcolor = cb4[8];
static const float4 bloom_blackcolor = cb4[9];
static const float4 bloom_facts = cb4[10];
static const float4 half_pixel_offset = cb4[11];
#else
// c0 bloom_dcolor, c1 bloom_facts, c2 g_halfPixelOffset
static const float4 bloom_dcolor = cb4[8];
static const float4 bloom_facts = cb4[9];
static const float4 half_pixel_offset = cb4[10];
#endif

void main(DGV_MAIN_SIGNATURE)
{
   const float2 uv = half_pixel_offset.zw + v5.xy;
   const float2 offsets[4] = {float2(-0.5, -0.5), float2(0.5, -0.5), float2(-0.5, 0.5), float2(0.5, 0.5)};
   float3 highs = 0.0;
#if BLOOM_LOW_HIGH
   float3 lows = 0.0;
#endif
   [unroll] for (int i = 0; i < 4; i++)
   {
      const float3 tap = saturate(ApplyDgvMask(t0.Sample(s0_s, bloom_facts.xy * offsets[i] + uv), cb3[44], cb3[45]).rgb);
      highs += saturate(tap - bloom_dcolor.rgb);
#if BLOOM_LOW_HIGH
      lows += saturate(bloom_blackcolor.rgb - tap);
#endif
   }
   float weight = bloom_dcolor.w;
#if BLOOM_MODULATE
   weight *= ApplyDgvMask(t1.Sample(s1_s, bloom_facts.zw), cb3[46], cb3[47]).x;
#endif
#if BLOOM_LOW_HIGH
   o0 = float4((highs - lows) * weight + 0.5, 1.0);
#else
   o0 = float4(highs * weight, 1.0);
#endif
}
