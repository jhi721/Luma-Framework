#ifndef SRC_DEPTH_EDGES_HLSL
#define SRC_DEPTH_EDGES_HLSL

// SMAA predication signal from depth: deviation from the local tangent plane, a slope-adjusted second difference with a
// depth-proportional tolerance (the math of XeGTAO_CalculateEdges; SVGF, Schied et al. 2017; Emil Persson 2009).
// A plain depth difference cannot work: a plane's own per-pixel change grows with distance.
//
// Each one-sided delta is compared against the slope implied by the opposite neighbour and the smaller is kept: on a plane
// (any orientation) the two agree and cancel to ~0, at a depth discontinuity neither does. Taking the min also means a
// garbage opposite tap (off the texture) can only shrink the result, never invent an edge.
//
// Returns edge-ness in [0,1] against the LEFT and TOP neighbours only: SMAA compares centre-vs-left and centre-vs-top, so a
// one-sided measure jumps 0 -> 1 exactly across a silhouette (a symmetric one would difference to 0 there), and
// SMAA_PREDICATION_THRESHOLD is 0.5. "tolerance" is the plane deviation counted as a full edge, in the depth's units
// (normally a fraction of the centre's view depth).
float PlaneDeviationEdgeness(float center, float left, float right, float top, float bottom, float tolerance)
{
   float4 edgesLRTB = float4(left, right, top, bottom) - center;
   const float slopeLR = (edgesLRTB.y - edgesLRTB.x) * 0.5;
   const float slopeTB = (edgesLRTB.w - edgesLRTB.z) * 0.5;
   edgesLRTB = min(abs(edgesLRTB), abs(edgesLRTB + float4(slopeLR, -slopeLR, slopeTB, -slopeTB)));
   return saturate(max(edgesLRTB.x, edgesLRTB.z) / tolerance);
}

// A value proportional to linear view depth from a device depth d with a standard (1 - d ~ near / z) or reversed
// (d ~ near / z) projection: the relative tolerance cancels the unknown near plane, so no camera constants are needed.
// The sky (0 after the flip) clamps at 2^-24.
float RelativeViewDepth(float deviceDepth, bool reversedZ)
{
   return rcp(max(reversedZ ? deviceDepth : 1.0 - deviceDepth, 1.0 / 16777216.0));
}

#endif // SRC_DEPTH_EDGES_HLSL
