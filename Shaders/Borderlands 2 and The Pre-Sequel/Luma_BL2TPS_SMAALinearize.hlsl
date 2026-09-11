// Borderlands 2 / The Pre-Sequel — linear-light copy of the post-tonemap LDR, for SMAA's neighborhood blend.
//
// SMAA detects edges on the ENCODED signal, the domain its thresholds are tuned in, but the third pass AVERAGES
// colour and that average only means anything in linear light. It cannot decode after sampling: the blend mixes a
// pixel with its neighbour by sampling at a fractional coordinate and letting the hardware bilinear do the mixing
// (SMAA.hlsl "We exploit bilinear filtering"), so by the time the shader sees a value the average over encoded
// values has already happened. Hence this pass - decode once, up front, into the texture that feeds only that pass.
//
// The encoding is the tonemap's own: plain gamma 2.2, NOT sRGB - which is why no hardware SRGB view can stand in
// for this. It is decoded with the same shared helper the tonemap encodes with (linear_to_gamma there, this its
// inverse), so the exponent is DefaultGamma on both sides and cannot drift if this game ever sets a custom one.
// GCT_POSITIVE is the max(x, 0) the decode needs anyway: the tonemap forces its colour non-negative before
// encoding, but the dither it applies afterwards can undershoot by a sub-LSB at black, and pow() of a negative
// returns NaN. Alpha carries nothing (the tonemap writes o0.w = 0) and is passed through untouched.
#include "../Includes/Color.hlsl"

Texture2D<float4> encoded : register(t0); // post-tonemap LDR snapshot (gamma 2.2)
RWTexture2D<float4> linear_out : register(u0);

[numthreads(8, 8, 1)] void main(uint3 id : SV_DispatchThreadID) {
   const float4 c = encoded.Load(int3(id.xy, 0));
   linear_out[id.xy] = float4(gamma_to_linear(c.rgb, GCT_POSITIVE), c.a);
}
