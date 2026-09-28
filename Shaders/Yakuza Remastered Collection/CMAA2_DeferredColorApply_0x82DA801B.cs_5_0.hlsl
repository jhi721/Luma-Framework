// QLOC's CMAA2 "deferred color apply 2x2" (sys_cmaa2_deferredcolorapply2x2), transcribed from the disassembly.
// CMAA2 keeps its blended colors as R11G11B10 floats, so HDR survives until this final store. Vanilla declares the
// output UAV `unorm`, which clamps every channel to [0,1] on the now-fp16 target. The only change here is the float
// declaration.

RWTexture2D<float4> u0 : register(u0);
RWStructuredBuffer<uint> u3 : register(u3);  // candidate pixel positions (x << 16 | y)
RWStructuredBuffer<uint2> u4 : register(u4); // blend items: next index, packed color
RWTexture2D<uint> u5 : register(u5);         // first blend item per pixel
RWByteAddressBuffer u6 : register(u6);       // dispatch counters

[numthreads(4, 32, 1)] void main(uint3 localThreadId : SV_GroupThreadID, uint3 threadId : SV_DispatchThreadID) {
   if (threadId.y >= u6.Load(12))
      return;

   uint packedPos = u3[threadId.y];
   uint2 pos = uint2(packedPos >> 16u, packedPos & 0xFFFFu);
   uint index = u5[pos];

   float4 sum = 0.0;
   for (uint i = 0u; index != 0xFFFFFFFFu && i < 32u; i++)
   {
      uint subPixel = index >> 30u;
      bool isComplexShape = ((index >> 26u) & 1u) != 0u;
      uint2 item = u4[index & 0x03FFFFFFu];
      index = item.x;
      float3 color = float3(f16tof32((item.y & 0x7FFu) << 3u), f16tof32((item.y >> 8u) & 0x3FF8u), f16tof32((item.y >> 18u) & 0x3FF0u));
      float weight = isComplexShape ? 1.8 : 0.8;
      if (subPixel == localThreadId.x)
         sum += float4(color * weight, weight);
   }
   if (sum.w == 0.0)
      return;

   uint2 target = pos * 2u + uint2(localThreadId.x & 1u, localThreadId.x < 2u ? 0u : 1u);
   u0[target] = float4(sum.rgb / sum.w, 0.0);
}
