// SMAA predication signal from the game's full resolution D32 scene depth (the copy the SSAO downsample reads).
// Port of Saints Row: The Third's Luma_SR3_SMAAPredication.hlsl (itself from Luma_TW2_DepthExtract.hlsl).
//
// SMAA predicates on a plain first difference between adjacent pixels, and on depth that cannot separate a silhouette
// from a surface seen edge-on: a plane's own per-pixel change grows with distance, so no remap and no threshold fixes
// the ratio. Instead this measures the deviation from the local tangent plane: a slope-adjusted second difference with a
// depth-proportional tolerance, the same math as XeGTAO_CalculateEdges.
//
// Depth: 1 / d (reversed Z, sky = 0) is about proportional to linear view depth;
// the relative tolerance cancels the unknown near plane, so no camera constants are needed. Sky clamps at 2^-24.
//
// Output is edge-ness in [0,1] against the LEFT and TOP neighbors only: SMAA compares center-vs-left on one axis and
// center-vs-top on the other, so a one-sided measure jumps 0 -> 1 exactly ACROSS a silhouette, while a symmetric mask
// would read 1 on both sides and difference to 0 where predication must fire. SMAA_PREDICATION_THRESHOLD is then 0.5.

Texture2D<float> depth : register(t0);
RWTexture2D<float> edgeness : register(u0);

// Reversed: the depth clears to 0 (main menu dump), and a closed room reads 0.036-0.52 (Velvet Room dialogue dump)
float LinearDepth(int3 p)
{
   return rcp(max(depth.Load(p), 1.0 / 16777216.0));
}

[numthreads(8, 8, 1)] void main(uint3 id : SV_DispatchThreadID) {
   const int3 p = int3(id.xy, 0);
   const float centerZ = LinearDepth(p);
   // Out-of-bounds Loads return 0; mirroring the center there keeps the border flat instead of a false edge rim.
   const float leftZ = (id.x > 0) ? LinearDepth(p - int3(1, 0, 0)) : centerZ;
   const float rightZ = LinearDepth(p + int3(1, 0, 0));
   const float topZ = (id.y > 0) ? LinearDepth(p - int3(0, 1, 0)) : centerZ;
   const float bottomZ = LinearDepth(p + int3(0, 1, 0));

   // Compare each one-sided delta against the slope implied by the opposite neighbor and keep the smaller: on a plane
   // (any orientation) they agree and this cancels to ~0; at a depth discontinuity neither cancels.
   float4 edgesLRTB = float4(leftZ, rightZ, topZ, bottomZ) - centerZ;
   const float slopeLR = (edgesLRTB.y - edgesLRTB.x) * 0.5;
   const float slopeTB = (edgesLRTB.w - edgesLRTB.z) * 0.5;
   edgesLRTB = min(abs(edgesLRTB), abs(edgesLRTB + float4(slopeLR, -slopeLR, slopeTB, -slopeTB)));

   // Plane deviation counted as a full edge, as a fraction of view depth (TW2's validated 0.02).
   const float tolerance = centerZ * 0.02;
   edgeness[id.xy] = saturate(max(edgesLRTB.x, edgesLRTB.z) / tolerance);
}
