// Medal of Honor (2010) — build the SMAA predication signal from the game's depth.
//
// Port of the shipped Airborne / Witcher 2 pass with ONE substantive change: the depth has to be linearised
// first. UE3 packs scene depth into the ALPHA of the fp16 scene colour buffer, which is already bound at t0 on
// the draw the mod hooks (the tonemap), so nothing has to be captured at another pass — the donor's fragile
// part. But where Airborne's alpha IS linear distance in Unreal units, this game's is not: the motion-blur block
// of the tonemap reciprocates it,
//     linearZ = 1 / (alpha * cb4[10].z - cb4[10].w)
// and every measurement below is meaningless on the raw value. The dispatch runs from the tonemap draw's own
// post-draw callback, where that cb4 is still bound, so the game's buffer is handed straight to this pass at b1
// instead of being round-tripped through a staging copy — same frame, no staleness, nothing to invalidate.
//
// WHY THIS IS NOT JUST A RESCALE. SMAA's predication compares the buffer between ADJACENT pixels against a
// constant (SMAA.hlsl: delta = abs(neighbours.xx - neighbours.yz); edges = step(THRESHOLD, delta)) — a plain
// first difference. On LINEAR depth that test cannot separate silhouettes from a surface seen edge-on: a plane's
// own per-pixel change grows as z^2 (dz ~= z^2 * theta_pix / d, d = perpendicular eye-to-plane distance), so a
// distant floor legitimately moves more per pixel than a nearby silhouette does. No monotonic remap of z and no
// choice of threshold fixes that ratio.
//
// So this measures deviation from the local tangent plane instead: a SLOPE-ADJUSTED SECOND DIFFERENCE with a
// depth-proportional tolerance, the same math as the repository's XeGTAO_CalculateEdges. Formalized by SVGF
// (Schied et al. 2017); prescribed for this use by Emil Persson (2009); the same principle as D3D's
// SlopeScaledDepthBias.
//
// Output = edge-ness in [0,1] against the LEFT and TOP neighbours only (never right/bottom): SMAA compares
// centre-vs-left on one axis and centre-vs-top on the other, so a one-sided measure makes the value jump 0 -> 1
// exactly ACROSS a silhouette. A symmetric mask would read 1 on both sides and its first difference would be 0
// right where predication must fire. Single-channel because the predication Gather reads only R.
// Because the result is normalized by centreZ it is a unitless edge-ness signal, so SMAA_PREDICATION_THRESHOLD is
// simply 0.5 — independent of scene scale, units, camera height, FOV and resolution. Tune P.x, never that.

Texture2D<float4> scene : register(t0); // fp16 scene colour; .w carries NON-linear depth (see the header)
RWTexture2D<float> uav : register(u0);  // R16_FLOAT predication signal (0 = on the local plane, 1 = edge)

cbuffer PredCB : register(b0)
{
   float4 P; // x = relative tolerance (plane deviation counted as a full edge, as a fraction of view depth)
}

// The game's own cb4, forwarded by main.cpp from the tonemap draw. Declared at the original's size because the
// buffer IS the original: only row 10 is read, and only the motion-blur permutations declare it, which is why the
// caller gates the whole dispatch on one of those running.
cbuffer PixelShaderConstants : register(b1)
{
   float4 PsConstants[236] : packoffset(c0);
}
#define DepthParams PsConstants[10] // .z/.w linearise the depth the scene alpha carries

// The game's own linearisation. The tonemap reaches this as rcp() behind dgVoodoo's zero test; here the
// denominator is floored instead, which caps the far plane at a large FINITE depth. Finite matters: an infinity
// would turn the sky's own neighbour differences into NaN. Every sky pixel then shares one value, so the sky is
// internally flat and only the horizon reads as a step — a genuine silhouette, correctly. It is also the first
// place to look if a halo appears along the skyline, and the lever for that is P.x.
float LinearDepth(float raw)
{
   return 1.0 / max(raw * DepthParams.z - DepthParams.w, 1e-6);
}

[numthreads(8, 8, 1)] void main(uint3 id : SV_DispatchThreadID) {
   const int3 p = int3(id.xy, 0);
   const float centerZ = LinearDepth(scene.Load(p).w);
   // Out-of-bounds Loads return 0 (D3D11-defined), which linearises to a bogus depth, so the border mirrors the
   // centre instead — a flat border rather than a false edge along the screen edges.
   const float leftZ = (id.x > 0) ? LinearDepth(scene.Load(p - int3(1, 0, 0)).w) : centerZ;
   const float rightZ = LinearDepth(scene.Load(p + int3(1, 0, 0)).w);
   const float topZ = (id.y > 0) ? LinearDepth(scene.Load(p - int3(0, 1, 0)).w) : centerZ;
   const float bottomZ = LinearDepth(scene.Load(p + int3(0, 1, 0)).w);

   // Deviation from the local plane: compare each one-sided delta against the slope implied by the opposite
   // neighbour, and keep whichever is smaller. On a plane (any orientation) the two agree and this cancels to
   // ~0; at a depth discontinuity neither cancels.
   float4 edgesLRTB = float4(leftZ, rightZ, topZ, bottomZ) - centerZ;
   const float slopeLR = (edgesLRTB.y - edgesLRTB.x) * 0.5;
   const float slopeTB = (edgesLRTB.w - edgesLRTB.z) * 0.5;
   const float4 edgesLRTBSlopeAdjusted = edgesLRTB + float4(slopeLR, -slopeLR, slopeTB, -slopeTB);
   edgesLRTB = min(abs(edgesLRTB), abs(edgesLRTBSlopeAdjusted));

   // Depth-proportional tolerance: the deviation that matters scales with distance (a step that is a silhouette
   // up close is noise far away). Both terms are required — the slope adjustment alone still degrades toward the
   // vanishing point, and a depth-proportional threshold alone cannot reject a grazing plane at all.
   const float tolerance = max(centerZ, 1e-3) * max(P.x, 1e-4);
   // Left and top only (see the header): this is the axis pairing SMAA's predication actually compares.
   uav[id.xy] = saturate(max(edgesLRTB.x, edgesLRTB.z) / tolerance);
}
