// Camera motion for the motion vector pixels no patched draw wrote (still at the clear value): the scene depth reprojected from the
// current to the previous frame's view projection (row vectors, as the game's "mW2P"). Sky (reversed Z depth 0) gets the rotation only.

cbuffer MotionVectorFill : register(b0)
{
   row_major float4x4 reprojection; // Current clip space to the previous frame's
   float2 jitter_ndc;               // This frame's projection jitter, which the depth has and the motion vectors don't
};

Texture2D<float> depth : register(t0);
RWTexture2D<float2> motion_vectors : register(u0);

[numthreads(8, 8, 1)] void main(uint3 id : SV_DispatchThreadID) {
   uint2 size;
   motion_vectors.GetDimensions(size.x, size.y);
   // FLT_MAX, the clear value
   if (any(id.xy >= size) || motion_vectors[id.xy].x != 3.402823466e+38)
      return;
   const float2 uv = (id.xy + 0.5) / size;
   const float4 current = float4(uv * float2(2.0, -2.0) + float2(-1.0, 1.0) - jitter_ndc, depth.Load(int3(id.xy, 0)), 1.0);
   const float4 previous = mul(current, reprojection);
   // Previous minus current, UV space, like the patched draws
   motion_vectors[id.xy] = (previous.xy / previous.w - current.xy) * float2(0.5, -0.5);
}
