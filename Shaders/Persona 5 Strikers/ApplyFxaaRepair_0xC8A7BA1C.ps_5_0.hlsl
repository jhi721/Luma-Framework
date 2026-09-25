// Katana engine PostEffect3 "ApplyFxaaRepairPS" (0xC8A7BA1C): ApplyFxaaPS with the blend taps bilinearly sampled instead of loaded, over the composite's output.
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
   float4 r0, r1, r2, r3, r4, r5;
   uint4 bitmask, uiDest;
   float4 fDest;

   r0.x = cmp(0 < g_vFxaaParams.w);
   if (r0.x != 0)
   {
      r0.xy = (int2)v0.xy;
      r0.zw = float2(0, 0);
      r1.xyz = g_tSceneMap.Load(r0.xyw, int2(0, 0)).xyz;
      r2.xy = g_tSceneMap.Load(r0.xyw, int2(-1, -1)).xy;
      r2.zw = g_tSceneMap.Load(r0.xyw, int2(1, -1)).xy;
      r3.xy = g_tSceneMap.Load(r0.xyw, int2(-1, 1)).xy;
      r0.xy = g_tSceneMap.Load(r0.xyz, int2(1, 1)).xy;
      r0.z = r1.y * 3.18291569 + r1.x;
      r4.z = r2.y * 3.18291569 + r2.x;
      r4.w = r2.w * 3.18291569 + r2.z;
      r4.y = r3.y * 3.18291569 + r3.x;
      r4.x = r0.y * 3.18291569 + r0.x;
      r0.xy = min(r4.zy, r4.wx);
      r0.x = min(r0.x, r0.y);
      r0.x = min(r0.z, r0.x);
      r0.yw = max(r4.zy, r4.wx);
      r0.y = max(r0.y, r0.w);
      r0.y = max(r0.z, r0.y);
      r0.z = r0.y + -r0.x;
      r0.w = g_vFxaaParams.x * r0.y;
      r0.w = max(g_vFxaaParams.y, r0.w);
      r0.z = cmp(r0.z < r0.w);
      if (r0.z == 0)
      {
         g_tSceneMap.GetDimensions(0, fDest.x, fDest.y, fDest.z);
         r0.zw = fDest.xy;
         r2.xyzw = r4.yzzw + r4.xwyx;
         r2.xy = r2.xz + -r2.yw;
         r1.w = dot(r2.xy, r2.xy);
         r1.w = max(1.00000001e-007, r1.w);
         r1.w = sqrt(r1.w);
         r2.xy = r2.xy / r1.ww;
         r1.w = min(abs(r2.x), abs(r2.y));
         r1.w = r1.w * g_vFxaaParams.z + 0.00100000005;
         r3.xy = r2.xy / r1.ww;
         r3.xy = max(-g_vFxaaParams.ww, r3.xy);
         r2.zw = min(g_vFxaaParams.ww, r3.xy);
         r3.x = 0.5;
         r3.z = g_vFxaaParams.w;
         r4.xyzw = -r2.xyzw * r3.xxzz + v0.xyxy;
         r4.xyzw = r4.xyzw / r0.zwzw;
         r2.xyzw = r2.xyzw * r3.xxzz + v0.xyxy;
         r2.xyzw = r2.xyzw / r0.zwzw;
         r3.xyz = g_tSceneMap.SampleLevel(sampleLinear_s, r4.xy, 0).xyz;
         r5.xyz = g_tSceneMap.SampleLevel(sampleLinear_s, r2.xy, 0).xyz;
         r4.xyz = g_tSceneMap.SampleLevel(sampleLinear_s, r4.zw, 0).xyz;
         r2.xyz = g_tSceneMap.SampleLevel(sampleLinear_s, r2.zw, 0).xyz;
         r3.xyz = r5.xyz + r3.xyz;
         r5.xyz = float3(0.5, 0.5, 0.5) * r3.xyz;
         r2.xyz = r4.xyz + r2.xyz;
         r2.xyz = float3(0.25, 0.25, 0.25) * r2.xyz;
         r2.xyz = r3.xyz * float3(0.25, 0.25, 0.25) + r2.xyz;
         r0.z = r2.y * 3.18291569 + r2.x;
         r0.x = cmp(r0.z < r0.x);
         r0.y = cmp(r0.y < r0.z);
         r0.x = (int)r0.y | (int)r0.x;
         r1.xyz = r0.xxx ? r5.xyz : r2.xyz;
      }
   }
   else
   {
      r0.xy = (int2)v0.xy;
      r0.zw = float2(0, 0);
      r0.xyz = g_tSceneMap.Load(r0.xyz).xyz;
      r1.xyz = min(float3(65024, 65024, 65024), r0.xyz);
   }
   o0.xyz = ApplyRadialBlurGammaFade(r1.xyz, v1.xy);
   o0.w = 1;
   return;
}
