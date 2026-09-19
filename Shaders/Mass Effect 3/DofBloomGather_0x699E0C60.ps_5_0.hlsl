// Mass Effect 3 (2012) - UE3 DOFAndBloomGather, the full-res DoF perms' quarter-res source (962x542 at 4K): DoF blur
// and bloom summed into ONE target, x0.25 (the uber reads it back x4). Transcribed from the dgVoodoo 2.87.3 disasm.
// Replaced for ONE purpose: to switch the game's own bloom off at its source when the Luma pyramid replaces it; the
// DoF half is vanilla. The half-res perms' downsample (0xC6215545) is not replaced: their uber stops reading it.
// GATHER_TAPS 16 here, 4 in DofBloomGather4_0xA8595DEF.

// clang-format off
// ORDER IS LOAD-BEARING - game-local Common.hlsl first, so LUMA_GAME_CB_STRUCTS is defined before Settings.hlsl.
#include "Includes/Common.hlsl"       // game-local: LumaSettings.GameSettings.LumaBloomEnable
#include "Includes/GameBindings.hlsl" // b3/b4, ApplyDgvMask
// clang-format on

#ifndef GATHER_TAPS
#define GATHER_TAPS 16
#endif

SamplerState SceneColorTextureSampler_s : register(s0);
Texture2D<float4> SceneColorTexture : register(t0); // fp16 scene colour; .w carries DEVICE Z, not alpha

// cb4 is dgVoodoo's shared mirror, so a row means different things per pass: named for THIS pass.
#define BloomScaleAndThreshold PsConstants[8]  // .x bloom scale (per post-process volume), .y bright-pass threshold
#define MinZ_MaxZRatio         PsConstants[10] // device Z -> view depth
#define DoFParams              PsConstants[11] // .x focus distance, .y 1/range, .z falloff exponent
#define DoFMaxBlur             PsConstants[12] // .x max blur near, .y max blur far

// One tap: colour + view depth accumulate for the average, the soft bright pass accumulates separately.
void GatherTap(float2 uv, inout float4 sceneSum, inout float3 bloomSum)
{
   float4 s = ApplyDgvMask(SceneColorTexture.Sample(SceneColorTextureSampler_s, uv), 0);
   bloomSum += s.xyz * saturate((max3(s.xyz) - BloomScaleAndThreshold.y) * 0.5);
   const float z = min(s.w, 65504.0) * MinZ_MaxZRatio.z - MinZ_MaxZRatio.w;
   s.w = abs(z) > 0.0 ? rcp(z) : 1e37;
   sceneSum += s;
}

// Tap pairs are (vN.xy, vN.wz) for N = 5..12.
void main(DGV_MAIN_SIGNATURE)
{
   float4 sceneSum = 0.0;
   float3 bloomSum = 0.0;
   const float4 taps[8] = {v5, v6, v7, v8, v9, v10, v11, v12};
   [unroll] for (uint i = 0; i < GATHER_TAPS / 2; i++)
   {
      GatherTap(taps[i].xy, sceneSum, bloomSum);
      GatherTap(taps[i].wz, sceneSum, bloomSum);
   }

   const float4 avg = sceneSum * (1.0 / GATHER_TAPS);

   // Vanilla DoF weight from the AVERAGE depth.
   const float blur = ME3_DoFBlur(avg.w, DoFParams.xyz, DoFMaxBlur.xy);

   // No vanilla glow when the Luma pyramid owns it.
   const float3 bloom = LumaSettings.GameSettings.LumaBloomEnable > 0.5 ? 0.0 : bloomSum * BloomScaleAndThreshold.x * (1.0 / GATHER_TAPS);
   o0 = float4(avg.xyz * blur + bloom, blur) * 0.25;
}
