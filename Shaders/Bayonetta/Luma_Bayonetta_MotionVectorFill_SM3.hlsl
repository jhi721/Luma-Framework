// Scene-end fill of the native D3D9 motion vector path (port of Shaders/Includes/MotionVectorFill.hlsl +
// the former dgVoodoo mod's Luma_Bayonetta_MotionVectorFill.hlsl for ps_3_0). Entry points: vs_quad (a full screen triangle),
// ps_fill (motion target + prepass -> the shared motion / depth textures through MRT) and ps_marker (clears the motion
// target to the marker; D3D9 Clear can't express 65504). Pixels come from VPOS (floor() so a half texel offset of
// VPOS can't matter), sampled point.

#define MOTION_VECTOR_MARKER 65504.0

sampler2D motion_target : register(s0); // G16R16F, render size
sampler2D prepass : register(s1);       // R32F 1 - z/w, render size

// c0..c3: the rows of "reprojection" (previous clip = reprojection * current clip, column vectors); c4: jitter_ndc.xy,
// size.xy
float4 reprojection[4] : register(c0);
float4 jitter_size : register(c4);

void vs_quad(float3 position : POSITION, out float4 clip : POSITION)
{
   clip = float4(position.xy, 0.0, 1.0);
}

float4 ps_marker() : COLOR0
{
   return float4(MOTION_VECTOR_MARKER, MOTION_VECTOR_MARKER, 0.0, 0.0);
}

void ps_fill(float2 vpos : VPOS, out float4 motion_out : COLOR0, out float4 depth_out : COLOR1)
{
   const float2 pixel = floor(vpos);
   const float2 size = jitter_size.zw;
   const float2 uv = (pixel + 0.5) / size;
   // saturate() also drops a NaN
   const float depth = saturate(1.0 - tex2Dlod(prepass, float4(uv, 0.0, 0.0)).x);
   const float2 object_motion = tex2Dlod(motion_target, float4(uv, 0.0, 0.0)).xy;
   float2 motion = object_motion;
   // IsObjectMotion(): below the marker in both components (ordered compares: a NaN fails them)
   if (!all(abs(object_motion) < MOTION_VECTOR_MARKER))
   {
      const float4 current = float4(uv * float2(2.0, -2.0) + float2(-1.0, 1.0) - jitter_size.xy, depth, 1.0);
      const float4 previous = float4(dot(reprojection[0], current), dot(reprojection[1], current), dot(reprojection[2], current), dot(reprojection[3], current));
      const float2 camera = (previous.xy / previous.w - current.xy) * float2(0.5, -0.5);
      // A point the previous camera can't project (w near 0, after a cut) gets none
      motion = all(abs(camera) < MOTION_VECTOR_MARKER) ? camera : float2(0.0, 0.0);
   }
   motion_out = float4(motion, 0.0, 0.0);
   depth_out = float4(depth, 0.0, 0.0, 0.0);
}
