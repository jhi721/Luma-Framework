// The upscaler's device depth (a sharable R32_FLOAT copy of the scene's D24S8, see SR bridge), and the camera motion for the motion
// vector pixels no patched draw wrote (still the marker 65504 from the frame start, the largest float16: sky, unpatched draws): the
// scene depth reprojected from the current to the previous frame's camera (the view projection derived from the draws' constants,
// column vectors). Depth isn't reversed. BL GOTY's fill, with the depth written out as ME1's.

cbuffer MotionVectorFill : register(b0)
{
   row_major float4x4 reprojection; // Current clip space to the previous frame's
   float2 jitter_ndc;               // This frame's projection jitter: in the depth, not in the motion vectors
   float2 padding;
};

Texture2D<float> depth : register(t0);
RWTexture2D<float2> motion_vectors : register(u0);
RWTexture2D<float> device_depth : register(u1);

[numthreads(8, 8, 1)] void main(uint3 id : SV_DispatchThreadID) {
   uint2 size;
   motion_vectors.GetDimensions(size.x, size.y);
   if (any(id.xy >= size))
      return;
   const float scene_depth = depth.Load(int3(id.xy, 0));
   device_depth[id.xy] = scene_depth;
   if (motion_vectors[id.xy].x < 65504.0)
      return;
   const float4 current = float4((id.xy + 0.5) / size * float2(2.0, -2.0) + float2(-1.0, 1.0) - jitter_ndc, scene_depth, 1.0);
   const float4 previous = mul(reprojection, current);
   // Previous minus current, UV space, like the patched draws
   motion_vectors[id.xy] = (previous.xy / previous.w - current.xy) * float2(0.5, -0.5);
}
