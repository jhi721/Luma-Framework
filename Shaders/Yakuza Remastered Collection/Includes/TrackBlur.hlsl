// fx_track_blur, the same math in every game (Y5R reorders the inputs): 12-tap motion trail over the scene. Vanilla
// ends with add_sat(+0.2), which clips the HDR scene inside the trail. Verbatim except max(0) there.

cbuffer cb11 : register(b11)
{
   uint4 cb11[1];
}

SamplerState s0_s : register(s0);
SamplerState s1_s : register(s1);
Texture2D<float4> t0 : register(t0); // scene
Texture2D<float4> t1 : register(t1); // trail mask (alpha)

float4 TrackBlur(float alphaScale, float4 taps[6], float2 maskUV)
{
   float4 result;
   float alpha = t1.Sample(s1_s, maskUV).w * alphaScale;
   result.w = alpha;
   if (cb11[0].z > 0u && (alpha - float(cb11[0].z) * 0.00392156886) < 0.0)
      discard;

   float3 c = t0.Sample(s0_s, taps[0].zw).xyz * 0.133;
   c = t0.Sample(s0_s, taps[0].xy).xyz * 0.143 + c;
   c = t0.Sample(s0_s, taps[1].xy).xyz * 0.123 + c;
   c = t0.Sample(s0_s, taps[1].zw).xyz * 0.113 + c;
   c = t0.Sample(s0_s, taps[2].xy).xyz * 0.103 + c;
   c = t0.Sample(s0_s, taps[2].zw).xyz * 0.093 + c;
   c = t0.Sample(s0_s, taps[3].xy).xyz * 0.073 + c;
   c = t0.Sample(s0_s, taps[3].zw).xyz * 0.063 + c;
   c = t0.Sample(s0_s, taps[4].xy).xyz * 0.053 + c;
   c = t0.Sample(s0_s, taps[4].zw).xyz * 0.043 + c;
   c = t0.Sample(s0_s, taps[5].xy).xyz * 0.033 + c;
   c = t0.Sample(s0_s, taps[5].zw).xyz * 0.023 + c;
   result.xyz = max(0.0, c + 0.2);
   return result;
}
