// Luma Bloom around the shared pyramid (Luma_Bloom_impl.hlsl), drawn by main.cpp with "DrawFullscreenPassD3D12()" (b0: the target's width and
// height, then the constants below) at glow_pass2 0x1EE5A3E2, whose own levels (glow_pass1 0x4050FD15 x8) are skipped.
// Vanilla glow (FUN_1402e7c70): glow_pass0 writes level L (1024x512 B8G8R8A8: rgb gains, alpha = luma gain, each saturated); glow_pass1 blurs it down
// to the 4 levels m0..m3 (512x256 to 64x32, 8-bit); glow_pass2 sums L and m0..m3 at 0.2 each, rgb x c_ps_user[0].yzw + alpha x c_ps_user[0].x,
// saturated, alpha = the max channel, into a 1024x512 B8G8R8A8 target. Everything after reads that target, unchanged here.
// The weights are the stage's (not constant): main.cpp reads them from the engine's constant mirror at the draw.
// That target stays vanilla (the exposure meter and the water read it too): "Bloom Intensity" scales the glow where it is added to the scene.

cbuffer GlowConstants : register(b0)
{
   float2 target_size;
   float weight_scale;    // The largest weight: the folded level is divided by it, so its 8-bit tail fade stays in level units
   float bloom_intensity; // 1 = vanilla, "apply_ps" only
   float4 weights;        // glow_pass2's c_ps_user[0]: alpha weight, then the rgb ones
}

Texture2D<float4> level : register(t0);
Texture2D<float4> bloom : register(t1);
SamplerState linear_sampler : register(s0);

// L into the fp16 bloom input: pass2 is linear up to its saturate, so the weights fold in before the blur
float4 fold_ps(float4 pos : SV_Position) : SV_Target
{
   const float4 l = level.Load(int3(pos.xy, 0));
   return float4((l.rgb * weights.yzw + l.a * weights.x) / weight_scale, 1.0);
}

// t0 = the folded L, t1 = DrawBloom's levels (its accumulations a_k = 0.5 * m_k + 0.5 * a_k+1), so m0 + m1 + m2 + m3 = 2 * a0 + a1 + a2.
// Vanilla m1..m3 are 8-bit: their faint tails round to 0, so a1 and a2 fade out below 1.5/255 (from 0.75/255), as the Remastered Collection's
// composite (Luma_YRC_Bloom.hlsl, modelled offline there).
float4 composite_ps(float4 pos : SV_Position) : SV_Target
{
   const float2 uv = pos.xy / target_size;
   float3 s = level.Load(int3(pos.xy, 0)).rgb + 2.0 * bloom.SampleLevel(linear_sampler, uv, 0.0).rgb;
   for (int mip = 1; mip < 3; mip++)
   {
      const float3 m = bloom.SampleLevel(linear_sampler, uv, mip).rgb;
      s += m * saturate(m * (2.0 * 255.0 / 1.5) - 1.0);
   }
   const float3 c = saturate(s * (0.2 * weight_scale));
   return float4(c, max(c.r, max(c.g, c.b)));
}

// In place of the glow composite (FUN_1402e8840: ps_flickerfree_fast 0xBA1E66BE adding the glow target onto the scene, blend ONE/ONE on
// rgb and alpha), drawn with the same blend: t0 = the glow target, bilinear over the scene as the game samples it
float4 apply_ps(float4 pos : SV_Position) : SV_Target
{
   return level.SampleLevel(linear_sampler, pos.xy / target_size, 0.0) * bloom_intensity;
}
