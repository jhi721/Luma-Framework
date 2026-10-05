// Medal of Honor: Airborne - UE3 camera motion blur (VS 0x82B21E78): 5 taps along each pixel's screen motion, reprojected from the
// linear depth in the scene's alpha. Body transcribed from the dgVoodoo->ps_5_0 disasm of 0x57D9358C.
//
// The one change: a tap that lands on the first person view (weapon, hands, anything UE3 draws in its foreground group) takes the
// pixel's own color instead. Vanilla draws the blur before the foreground, so it never sees it; under DLAA/FSR the blur is replayed
// after the upscaler (main.cpp "ReplayMotionBlur"), on a scene that already holds it, which would otherwise smear the weapon into
// the world around it. The mask is the scene depth after the foreground's depth clear (t1): below 1 on the foreground only, so it
// needs no per weapon knowledge. Without it (vanilla's own draw, the game's 1x1 placeholder or nothing at t1) every tap is kept.

// clang-format off
#include "Includes/GameBindings.hlsl" // b3/b4, the dgVoodoo masks, ApplyDgvMask
// clang-format on

Texture2D<float4> t0 : register(t0); // A copy of the scene (fp16, alpha = linear depth, the sky 65504)
Texture2D<float> t1 : register(t1);  // The foreground mask: the scene depth after the foreground's clear (see above)
SamplerState s0_s : register(s0);

float4 SampleScene(float2 uv)
{
   return ApplyDgvMask(t0.Sample(s0_s, uv), DgvMaskT0, DgvFillT0);
}

// Full 13-entry interpolator layout, declared in order even where unread: linkage is by REGISTER (see Luma_MOHA_Tonemap.hlsl).
// Only TEXCOORD0 is read: .xy the clip space position, .zw the scene UV.
void main(
    float4 v0 : SV_POSITION0,
    float4 v1 : TEXCOORD8,
    float4 v2 : COLOR0,
    float4 v3 : COLOR1,
    float4 v4 : TEXCOORD9,
    float4 v5 : TEXCOORD0,
    float4 v6 : TEXCOORD1,
    float4 v7 : TEXCOORD2,
    float4 v8 : TEXCOORD3,
    float4 v9 : TEXCOORD4,
    float4 v10 : TEXCOORD5,
    float4 v11 : TEXCOORD6,
    float4 v12 : TEXCOORD7,
    out float4 o0 : SV_TARGET0)
{
   const float4 center = SampleScene(v5.zw);
   float depth = min(center.w, 65504.0);
   depth = (depth - 14.0 >= 0.0) ? depth : 65504.0;

   // The pixel's previous clip position (x, y, w) through PsConstants[8..11], then its screen motion
   const float2 screenPosition = depth * v5.xy;
   float3 previous = screenPosition.y * PsConstants[9].xyw;
   previous = PsConstants[8].xyw * screenPosition.x + previous;
   previous = PsConstants[10].xyw * depth + previous;
   previous = previous + PsConstants[11].xyw;
   float2 motion = v5.xy - previous.xy * DgVoodooRcp(previous.z);
   // Scaled to UV and clamped; the step grows with the square of the motion's length relative to the clamp
   motion *= PsConstants[12].xy;
   motion = min(PsConstants[12].zw, max(motion, -PsConstants[12].zw));
   const float2 relative = motion * float2(DgVoodooRcp(PsConstants[12].z), DgVoodooRcp(PsConstants[12].w));
   const float2 step = motion * (dot(relative, relative) + 0.0);

   uint2 scene_size, mask_size;
   t0.GetDimensions(scene_size.x, scene_size.y);
   t1.GetDimensions(mask_size.x, mask_size.y);
   const bool masked = all(mask_size == scene_size);

   float2 uv = step * -0.4 + v5.zw;
   float3 sum = 0.0;
   [loop] for (int i = 0; i < 5; i++)
   {
      const bool foreground = masked && t1.Load(int3(min(uint2(saturate(uv) * mask_size), mask_size - 1), 0)) < 1.0;
      sum += foreground ? center.xyz : SampleScene(uv).xyz;
      uv = step * 0.2 + uv;
   }
   o0 = float4(sum * 0.2, 1.0);
}
