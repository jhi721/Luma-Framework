// XeGTAO adapted for Persona 5 Strikers (Katana engine, D3D11): replaces the game's half resolution SSAO.
// Forked from Luma_SR3_XeGTAO.hlsl (Saints Row: The Third).
// Source: https://github.com/GameTechDev/XeGTAO
//
// P5S specifics (from the native SSAO's disassembly and a capture of its inputs):
// - Only the SSAO calculate (PS 0x63435B03) is replaced: these 4 dispatches run at its target size (R8_UNORM, half res) and are
//   CopyResource'd into it. Its two depth aware blurs (0xDEBA65FD, 0x4D8EC71C, upsampling to full res) and the G-buffer merge
//   (min into gbuf0.a) stay vanilla, so the output is VISIBILITY (1 = open), as the native one.
// - Depth = the calculate's t0 (half res raw D32, reversed Z, sky = 0); normals = its t1 (full res R16G16B16A16_UNORM, octahedral
//   view space normals in .xy), read at each target pixel's full res pixel (NormalInputScaleRT).
// - Depth unpack and uv->view use the calculate's live $Globals (rebound PS -> CS b0), in centimetres:
//   viewZ = 1 / (d * vDepthParam.x + vDepthParam.y), view.xy = (uv * vViewParam.xy + vViewParam.zw) * viewZ, view.z = -viewZ
//   (right handed, y up). XeGTAO's view space only differs by +z forward, so only the normal's z flips.
// - Noise: frozen at 0 without an upscaler (a frame index would make the pattern boil), denoise runs twice. With DLSS/FSR
//   (they accumulate the lit scene the AO feeds) it cycles frame % 64 and denoise runs once, as Intel's XeGTAO.h advises
//   with TAA (NoiseIndexRT, set by main.cpp).

// --- The SSAO calculate's $Globals (main.cpp binds it at CS b0) ---

cbuffer GameGlobals : register(b0)
{
   float4 vResolutionParam; // half res w, h, 1/w, 1/h
   float4 vDepthParam;      // raw depth -> 1 / viewZ scale, offset; max AO distance
   float4 vViewParam;       // uv -> view xy at viewZ 1: mul.xy, add.xy
   float4 vRadiusParam;     // radius^2 (cm^2), -1/radius^2, radius in pixels at viewZ 1, max pixels
   float4 vRayParam;        // directions, steps, 1/directions, horizon bias
   float4 vOccParam;        // strength, max occlusion, noise tiling
}

// --- Luma knobs from main.cpp (power and radius are live DEV/TEST sliders) ---
// b9, not b11: Core's DrawBloom owns b11. Mirrored by gtao_knobs_cb_slot.
cbuffer LumaGTAO : register(b9)
{
   float FinalValuePowerRT;    // primary darkness dial
   float NormalInputScaleRT;   // full res normal pixels per AO target pixel (2 = half res)
   float RadiusOverrideRT;     // > 0 overrides the native radius (centimetres)
   float DebugViewRT;          // DEVELOPMENT debug view (legend in Includes/XeGTAO.hlsl)
   float2 ViewportPixelSizeRT; // 1 / AO target resolution
   float NoiseIndexRT;         // frame % 64 with DLSS/FSR, 0 otherwise (see the header)
   float PaddingRT;
}

#define NORMAL_Z_SIGN -1.0 // the game's view space is -z forward, XeGTAO's +z

// The native world radius (40 cm), unless RadiusOverrideRT > 0
#define EFFECT_RADIUS sqrt(vRadiusParam.x)
// 1 reaches exactly the native radius (the native term drops samples beyond r, weighted 1 - d^2/r^2). EFFECT_FALLOFF_RANGE stays
// Intel's 0.615: the native term also fades with distance (1 / (1 + dist)).
#define RADIUS_MULTIPLIER 1.0

// The native march stops at vRadiusParam.w (108 half res pixels), which shrinks the radius closer than ~5 m (close-ups)
#define XE_GTAO_ADJUST_SCREENSPACE_RADIUS(screenspaceRadius) \
   if (RadiusOverrideRT <= 0.0)                              \
      screenspaceRadius = min(screenspaceRadius, vRadiusParam.w);

#define VIEWPORT_PIXEL_SIZE ViewportPixelSizeRT

// The native calculate's own uv -> view pair: viewPos.xy = (uv * MUL + ADD) * viewZ
#define NDC_TO_VIEW_MUL           vViewParam.xy
#define NDC_TO_VIEW_ADD           vViewParam.zw

#define XE_GTAO_FINAL_OUTPUT_TYPE unorm float // copy source for the game's R8 AO target
#define XE_GTAO_ENCODE_FINAL(v)   (v)
// Visibility, capped at the native maximum occlusion (vOccParam.y, 0.7)
#define XE_GTAO_FINAL_VALUE(v) max(v, 1.0 - vOccParam.y)

// Reversed Z hardware depth -> view z, as the native calculate (near 32 at d = 1, 240000 at the sky's d = 0; centimetres)
float XeGTAO_ScreenSpaceToViewSpaceDepth(const float screenDepth)
{
   return rcp(max(screenDepth * vDepthParam.x + vDepthParam.y, 1e-6));
}

Texture2D<float2> normals : register(t1); // the calculate's t1, full res

// The native calculate's octahedral decode: e = t*2-1, n = (e, 1-|e.x|-|e.y|), the lower hemisphere folded back
// (xy = (1-|yx|) * sign(xy)), normalized, in the same view space as the positions.
float3 XeGTAO_LoadViewspaceNormal(uint2 pixCoord)
{
   const float2 e = normals.Load(int3(pixCoord * uint(NormalInputScaleRT), 0)) * 2.0 - 1.0;
   float3 n = float3(e, 1.0 - abs(e.x) - abs(e.y));
   if (n.z < 0.0)
      n.xy = (1.0 - abs(n.yx)) * (n.xy >= 0.0 ? 1.0 : -1.0);
   n = normalize(n);
   return float3(n.xy, n.z * NORMAL_Z_SIGN);
}

#include "../Includes/XeGTAO.hlsl"
