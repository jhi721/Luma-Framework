// rl_bokeh_cs_00: spawns a bokeh sprite (drawn additively by rl_bokeh_sprite_01/02) where a pixel's luma (0.21 / 0.72 / 0.07 over
// the gamma-encoded composite) stands out from its ring. It reads a copy of the final composite, 8-bit UNORM in vanilla; the HDR one
// goes above 1, and the sprite color grows with the luma excess over the threshold times the color itself, so peaks went past 10k
// nits. Each read is clamped to the vanilla range; the rest is the original.

cbuffer CSConstants : register(b0)
{
   float4 pixel_steps; // xy = 1 / color size, zw = depth texels per thread
   float blur_radius;
   float bokeh_threshhold;
   float bokeh_strength;
   float bokeh_scale;
}

struct Sprite
{
   float4 pos;
   float4 color;
};

SamplerState filter_sampler : register(s2);
Texture2D<float4> Depth_tex : register(t1);
Texture2D<float4> Color_tex : register(t2);
AppendStructuredBuffer<Sprite> Out_buff : register(u0);

// Bilinear over the clamped texels, as the UNORM copy clamped before filtering, not after; clamp addressing.
float3 SampleColor(float2 uv, float2 offset)
{
   int2 size;
   Color_tex.GetDimensions(size.x, size.y);
   const float2 texel = (uv + offset * pixel_steps.xy) * size - 0.5;
   const int2 base = int2(floor(texel));
   const float2 f = texel - base;
   const float3 c00 = saturate(Color_tex.Load(int3(clamp(base, 0, size - 1), 0)).rgb);
   const float3 c10 = saturate(Color_tex.Load(int3(clamp(base + int2(1, 0), 0, size - 1), 0)).rgb);
   const float3 c01 = saturate(Color_tex.Load(int3(clamp(base + int2(0, 1), 0, size - 1), 0)).rgb);
   const float3 c11 = saturate(Color_tex.Load(int3(clamp(base + int2(1, 1), 0, size - 1), 0)).rgb);
   return lerp(lerp(c00, c10, f.x), lerp(c01, c11, f.x), f.y);
}

[numthreads(32, 32, 1)] void main(uint3 group_id : SV_GroupID, uint3 thread_id : SV_GroupThreadID) {
   const float2 pixel = float2(group_id.xy * 32 + thread_id.xy + 2);
   const float2 uv = pixel * pixel_steps.xy;
   const float coc = Depth_tex.Load(int3(uint2(pixel * pixel_steps.zw), 0)).x * blur_radius;

   // Inner 2x2 (bilinear, weights 4 2 2 1 = 9) and the ring of 8 around it (weight 2 each, total 25 with the inner)
   const float3 inner = SampleColor(uv, float2(-0.5, -0.5)) * 4.0 + SampleColor(uv, float2(1.0, -0.5)) * 2.0 + SampleColor(uv, float2(-0.5, 1.0)) * 2.0 + SampleColor(uv, float2(1.0, 1.0));
   const float3 all = inner + 2.0 * (SampleColor(uv, float2(-2.0, 1.5)) + SampleColor(uv, float2(-2.0, -0.5)) + SampleColor(uv, float2(-1.5, -2.0)) + SampleColor(uv, float2(0.5, -2.0)) + SampleColor(uv, float2(2.0, -1.5)) + SampleColor(uv, float2(2.0, 0.5)) + SampleColor(uv, float2(-0.5, 2.0)) + SampleColor(uv, float2(1.5, 2.0)));
   const float3 weights = float3(0.21, 0.72, 0.07);
   // Zero (not a skip) past the focus limit, as the original: a negative threshold still spawns black sprites there
   const bool blurred = coc > 1.5;
   const float contrast = blurred ? dot(inner, weights / 9.0) - dot(all, weights) * 0.04 : 0.0;

   if (bokeh_threshhold < contrast)
   {
      const float coc_area = coc * coc;
      const float amount = (contrast - bokeh_threshhold) / coc_area;
      Sprite sprite;
      sprite.pos = float4(uv * float2(2.0, -2.0) + float2(-1.0, 1.0), -0.49, coc_area * bokeh_scale);
      sprite.color = float4((blurred ? inner / 9.0 : 0.0) * (amount * bokeh_strength), max(1.0 - amount * bokeh_strength, 0.0));
      Out_buff.Append(sprite);
   }
}
