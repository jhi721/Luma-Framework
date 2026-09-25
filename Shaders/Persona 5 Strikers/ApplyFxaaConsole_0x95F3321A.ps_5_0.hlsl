// Katana engine PostEffect3 "ApplyFxaaConsolePS" (0x95F3321A): FXAA 3.11 console: 4 half texel luma taps, then a 2 or 4 tap blend, over the composite's output.
// Luma: the shared tail (radial blur, output power curve in SDR only, fade) is in "Includes/ApplyFxaa.hlsl".
// clang-format off
#include "Includes/Common.hlsl"
#include "Includes/ApplyFxaa.hlsl"
// clang-format on

void main(
    float4 v0 : SV_Position0,
    float2 v1 : TEXCOORD0,
    out float4 o0 : SV_Target0)
{
   float4 r0, r1, r2, r3, r4;
   uint4 bitmask, uiDest;
   float4 fDest;

   r0.x = cmp(0 < g_vFxaaParams.w);
   if (r0.x != 0)
   {
      g_tSceneMap.GetDimensions(0, fDest.x, fDest.y, fDest.z);
      r0.xy = fDest.xy;
      r1.xyzw = float4(1, 1, 1, 1) / r0.xyxy;
      r1.xyzw = r1.xyzw * float4(-0.5, -0.5, 0.5, 0.5) + v1.xyxy;
      r2.xyz = g_tSceneMap.SampleLevel(sampleLinear_s, r1.xy, 0).xyz;
      r0.z = dot(r2.xyz, float3(0.222014993, 0.706655025, 0.0713300034));
      r2.xyz = g_tSceneMap.SampleLevel(sampleLinear_s, r1.xw, 0).xyz;
      r0.w = dot(r2.xyz, float3(0.222014993, 0.706655025, 0.0713300034));
      r2.xyz = g_tSceneMap.SampleLevel(sampleLinear_s, r1.zy, 0).xyz;
      r1.x = dot(r2.xyz, float3(0.222014993, 0.706655025, 0.0713300034));
      r1.yzw = g_tSceneMap.SampleLevel(sampleLinear_s, r1.zw, 0).xyz;
      r1.y = dot(r1.yzw, float3(0.222014993, 0.706655025, 0.0713300034));
      r2.xyz = g_tSceneMap.SampleLevel(sampleLinear_s, v1.xy, 0).xyz;
      r1.z = dot(r2.xyz, float3(0.222014993, 0.706655025, 0.0713300034));
      r1.w = max(r0.z, r0.w);
      r2.w = min(r0.z, r0.w);
      r1.x = 0.00260416674 + r1.x;
      r3.x = max(r1.x, r1.y);
      r3.y = min(r1.x, r1.y);
      r1.w = max(r3.x, r1.w);
      r2.w = min(r3.y, r2.w);
      r3.x = g_vFxaaParams.x * r1.w;
      r3.y = min(r2.w, r1.z);
      r3.x = max(g_vFxaaParams.y, r3.x);
      r1.z = max(r1.w, r1.z);
      r1.z = r1.z + -r3.y;
      r1.z = cmp(r1.z >= r3.x);
      if (r1.z != 0)
      {
         r3.xy = g_vFxaaParams.ww / r0.xy;
         r0.xy = float2(2, 2) / r0.xy;
         r0.w = -r1.x + r0.w;
         r0.z = r1.y + -r0.z;
         r1.x = r0.w + r0.z;
         r1.y = r0.w + -r0.z;
         r0.z = dot(r1.xy, r1.xy);
         r0.z = rsqrt(r0.z);
         r0.zw = r1.xy * r0.zz;
         r1.xy = -r0.zw * r3.xy + v1.xy;
         r1.xyz = g_tSceneMap.SampleLevel(sampleLinear_s, r1.xy, 0).xyz;
         r3.xy = r0.zw * r3.xy + v1.xy;
         r3.xyz = g_tSceneMap.SampleLevel(sampleLinear_s, r3.xy, 0).xyz;
         r3.w = min(abs(r0.z), abs(r0.w));
         r3.w = g_vFxaaParams.z * r3.w;
         r0.zw = r0.zw / r3.ww;
         r0.zw = max(float2(-2, -2), r0.zw);
         r0.zw = min(float2(2, 2), r0.zw);
         r4.xy = -r0.zw * r0.xy + v1.xy;
         r4.xyz = g_tSceneMap.SampleLevel(sampleLinear_s, r4.xy, 0).xyz;
         r0.xy = r0.zw * r0.xy + v1.xy;
         r0.xyz = g_tSceneMap.SampleLevel(sampleLinear_s, r0.xy, 0).xyz;
         r1.xyz = r3.xyz + r1.xyz;
         r0.xyz = r4.xyz + r0.xyz;
         r3.xyz = float3(0.25, 0.25, 0.25) * r1.xyz;
         r0.xyz = r0.xyz * float3(0.25, 0.25, 0.25) + r3.xyz;
         r0.w = dot(r0.xyz, float3(0.222014993, 0.706655025, 0.0713300034));
         r2.w = cmp(r0.w < r2.w);
         r0.w = cmp(r1.w < r0.w);
         r0.w = (int)r0.w | (int)r2.w;
         r1.xyz = float3(0.5, 0.5, 0.5) * r1.xyz;
         r2.xyz = r0.www ? r1.xyz : r0.xyz;
      }
   }
   else
   {
      r0.xy = (int2)v0.xy;
      r0.zw = float2(0, 0);
      r0.xyz = g_tSceneMap.Load(r0.xyz).xyz;
      r2.xyz = min(float3(65024, 65024, 65024), r0.xyz);
   }
   o0.xyz = ApplyRadialBlurGammaFade(r2.xyz, v1.xy);
   o0.w = 1;
   return;
}
