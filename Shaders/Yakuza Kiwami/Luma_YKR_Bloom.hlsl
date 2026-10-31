// Luma Bloom around the shared pyramid (Luma_Bloom_impl.hlsl), drawn by main.cpp with "DrawFullscreenPassD3D12()" (b0: the target's width and
// height, then the constants below) at glow_pass2 0x1EE5A3E2, whose own levels (glow_pass1 0x4050FD15 x8) are skipped.
// Vanilla glow (FUN_1402e7c70): glow_pass0 writes level L (1024x512 B8G8R8A8: rgb gains, alpha = luma gain, each saturated); glow_pass1 blurs it down
// to the 4 levels m0..m3 (512x256 to 64x32, 8-bit); glow_pass2 sums L and m0..m3 at 0.2 each, rgb x c_ps_user[0].yzw + alpha x c_ps_user[0].x,
// saturated, alpha = the max channel, into a 1024x512 B8G8R8A8 target. Everything after reads that target, unchanged here.
// The weights are the stage's (not constant): main.cpp reads them from the engine's constant mirror at the draw.
// That target stays vanilla (the exposure meter and the water read it too): "Bloom Intensity" scales the glow where it is added to the scene.

// For "YKR_VanillaSceneHDR" only: Luma's cbuffers aren't in these passes' root signature, nothing here reads them
#include "Includes/Common.hlsl"

cbuffer GlowConstants : register(b0)
{
   float2 target_size;
   float weight_scale;    // The largest weight: the folded level is divided by it, so its 8-bit tail fade stays in level units
   float bloom_intensity; // 1 = vanilla, "apply_ps" only
   float4 weights;        // glow_pass2's c_ps_user[0]: alpha weight, then the rgb ones
}

Texture2D<float4> level : register(t0);
Texture2D<float4> bloom : register(t1); // The scene copy in "apply_ps"
SamplerState linear_sampler : register(s0);

// L into the fp16 bloom input: pass2 is linear up to its saturate, so the weights fold in before the blur
float4 fold_ps(float4 pos : SV_Position) : SV_Target
{
   const float4 l = level.Load(int3(pos.xy, 0));
   return float4((l.rgb * weights.yzw + l.a * weights.x) / weight_scale, 1.0);
}

// t0 = the folded L, t1 = DrawBloom's levels (its accumulations a_k = 0.5 * m_k + 0.5 * a_k+1), so m0 + m1 + m2 + m3 = 2 * a0 + a1 + a2.
// Vanilla m1..m3 are 8-bit: their faint tails round to 0, so a1 and a2 fade out below 1.5/255 (from 0.75/255), as the Remastered Collection's
// composite does (modelled offline).
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
// rgb and alpha), drawn opaque over the scene: t0 = the glow target, bilinear over the scene as the game samples it, t1 = a copy of the scene.
// The vanilla 8-bit scene clipped that sum at 1. The fp16 scene doesn't, and the post chain reads anything above the materials' pivot as their
// HDR extension, so glow on an already bright surface (rim lights on skin) blew out; clipping at 1 instead left a flat, hue shifted plateau at
// an HDR level. So the glow adds the display light (scene encoding: display = x^2.2) it added in vanilla, clip included, to the pixel's own:
// vanilla below the pivot, where both scenes are equal, and in SDR display modes. "weight_scale" is 1 for the HDR display mode here.
float4 apply_ps(float4 pos : SV_Position) : SV_Target
{
   const float4 glow = level.SampleLevel(linear_sampler, pos.xy / target_size, 0.0) * bloom_intensity;
   const float4 scene = bloom.Load(int3(pos.xy, 0));
   float4 vanilla = saturate(scene);
   if (weight_scale != 0.0)
   {
      vanilla.rgb = float3(YKR_VanillaSceneHDR(scene.r), YKR_VanillaSceneHDR(scene.g), YKR_VanillaSceneHDR(scene.b));
   }
   const float3 added_display = pow(saturate(vanilla.rgb + glow.rgb), 2.2) - pow(vanilla.rgb, 2.2);
   const float3 lit = max(scene.rgb, 0.0);
   // Added to the scene, not written over it: a negative channel (DoF leaves some) stays as it was where no glow lands
   return float4(scene.rgb + pow(pow(lit, 2.2) + added_display, 1.0 / 2.2) - lit, scene.a + saturate(vanilla.a + glow.a) - vanilla.a);
}
