// QLOC's FidelityFX CAS (sharpen only, no scaling; sys_cas_shader_sharpen_only), transcribed from the disassembly.
// Vanilla ends every pixel with mul_sat and alpha 1, which clips the HDR chain back to [0,1]. The only change here is
// that the output keeps its range (max(0) instead of saturate). The sharpening amplitude still comes from the vanilla
// min(min, 2 - max) / max term, so pixels whose neighborhood exceeds 1 get no sharpening, as CAS was designed for SDR.

cbuffer cb0 : register(b0)
{
   uint4 const0;
   float4 const1; // x: peak = -1 / lerp(8, 5, sharpness)
}

Texture2D<float4> t0 : register(t0);
RWTexture2D<float4> u0 : register(u0);

// FFX approximations, bit-exact with the vanilla constants.
float PrxLoRcp(float a)
{
   return asfloat(0x7ef07ebbu - asuint(a));
}
float PrxLoSqrt(float a)
{
   return asfloat((asuint(a) >> 1) + 0x1fbc4639u);
}
float PrxMedRcp(float a)
{
   float b = asfloat(0x7ef19fffu - asuint(a));
   return b * (-b * a + 2.0);
}

void Filter(int2 ip)
{
   float3 a = t0.Load(int3(ip + int2(-1, -1), 0)).rgb;
   float3 b = t0.Load(int3(ip + int2(0, -1), 0)).rgb;
   float3 c = t0.Load(int3(ip + int2(1, -1), 0)).rgb;
   float3 d = t0.Load(int3(ip + int2(-1, 0), 0)).rgb;
   float3 e = t0.Load(int3(ip, 0)).rgb;
   float3 f = t0.Load(int3(ip + int2(1, 0), 0)).rgb;
   float3 g = t0.Load(int3(ip + int2(-1, 1), 0)).rgb;
   float3 h = t0.Load(int3(ip + int2(0, 1), 0)).rgb;
   float3 i = t0.Load(int3(ip + int2(1, 1), 0)).rgb;

   // Green-only amplitude (vanilla: CAS_GO_SLOWER 0).
   float mnG = min(min(min(d.g, e.g), min(f.g, b.g)), h.g);
   float mnG2 = min(mnG, min(min(a.g, c.g), min(g.g, i.g)));
   mnG += mnG2;
   float mxG = max(max(max(d.g, e.g), max(f.g, b.g)), h.g);
   float mxG2 = max(mxG, max(max(a.g, c.g), max(g.g, i.g)));
   mxG += mxG2;

   float ampG = saturate(min(mnG, 2.0 - mxG) * PrxLoRcp(mxG));
   float w = PrxLoSqrt(ampG) * const1.x;
   float rcpWeight = PrxMedRcp(4.0 * w + 1.0);
   float3 pix = (b * w + d * w + e + f * w + h * w) * rcpWeight;
   u0[ip] = float4(max(0.0, pix), 1.0);
}

[numthreads(64, 1, 1)] void main(uint3 localThreadId : SV_GroupThreadID, uint3 workGroupId : SV_GroupID) {
   // ARmp8x8 remap of the 64 threads to an 8x8 quad grid, 4 pixels per thread over a 16x16 tile.
   uint lx = (localThreadId.x >> 1u) & 7u;
   uint ly = ((localThreadId.x >> 3u) & ~1u) | (localThreadId.x & 1u);
   int2 gxy = int2(workGroupId.xy * 16u + uint2(lx, ly));
   Filter(gxy);
   Filter(gxy + int2(8, 0));
   Filter(gxy + int2(8, 8));
   Filter(gxy + int2(0, 8));
}
