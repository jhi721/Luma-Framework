// DLSS/FSR inputs from the scene's alpha, where the base pass writes Gearbox's EncodeFloatW of the view depth W (Common.usf:
// -(W / 32)^2 up to 4096 units, (W / 8192)^2 past it, the sky clamped to 65503): the device depth, and the camera motion for the
// motion vector pixels no patched draw wrote (still the marker 65504 from the frame start, the largest float16: sky, unpatched
// draws), that depth reprojected from the current to the previous frame's camera (vc4 view projection, turned into column vectors
// on the CPU, PreViewTranslation change included). Depth isn't reversed. The game has no depth view to read (ME1's fill).

cbuffer MotionVectorFill : register(b0)
{
   row_major float4x4 reprojection; // Current clip space to the previous frame's
   float2 jitter_ndc;               // This frame's projection jitter: in the depth, not in the motion vectors
   float2 depth_from_view;          // The projection's depth row: device depth = x + y / view depth
};

Texture2D<float4> scene : register(t0);
RWTexture2D<float2> motion_vectors : register(u0);
RWTexture2D<float> device_depth : register(u1);

// Common.usf DecodeFloatW
float DecodeFloatW(float encoded)
{
   return sqrt(abs(encoded)) * (encoded > 0.0 ? 8192.0 : 32.0);
}

[numthreads(8, 8, 1)] void main(uint3 id : SV_DispatchThreadID) {
   uint2 size;
   motion_vectors.GetDimensions(size.x, size.y);
   if (any(id.xy >= size))
      return;
   // max() also drops a NaN; a cleared (0) alpha lands on the near plane
   const float depth = saturate(depth_from_view.x + depth_from_view.y / max(DecodeFloatW(scene.Load(int3(id.xy, 0)).a), 1e-4));
   device_depth[id.xy] = depth;
   if (motion_vectors[id.xy].x < 65504.0)
      return;
   const float4 current = float4((id.xy + 0.5) / size * float2(2.0, -2.0) + float2(-1.0, 1.0) - jitter_ndc, depth, 1.0);
   const float4 previous = mul(reprojection, current);
   // Previous minus current, UV space, like the patched draws
   motion_vectors[id.xy] = (previous.xy / previous.w - current.xy) * float2(0.5, -0.5);
}
