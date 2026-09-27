// DEV A/B: DLSS / FSR on a reversible tonemap of the scene, c / (1 + max(c)), undone on the output, c / (1 - max(c)). Tests whether
// the flicker of distant thin geometry against the bright sky comes from the linear HDR contrast.

Texture2D<float4> scene : register(t0);
RWTexture2D<float4> target : register(u0);

[numthreads(8, 8, 1)] void tonemap_cs(uint3 id : SV_DispatchThreadID) {
   const float4 c = scene.Load(int3(id.xy, 0));
   target[id.xy] = float4(c.rgb / (1.0 + max(max(c.r, max(c.g, c.b)), 0.0)), c.a);
}

    [numthreads(8, 8, 1)] void untonemap_cs(uint3 id : SV_DispatchThreadID)
{
   const float4 c = target[id.xy];
   target[id.xy] = float4(c.rgb / (1.0 - min(max(max(c.r, max(c.g, c.b)), 0.0), 0.999)), c.a);
}
