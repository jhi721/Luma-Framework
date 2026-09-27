// DLSS / FSR below native: UE3's filter vertex shader (FilterVertexShader<SAMPLES>, 1 to 8 samples, 0x1C49F98D ... 0xAF68CEAD) for the
// DOF/Bloom gather's blurs at output resolution: its quad covers the render sub-rect's UVs (see Luma_BL_OutputGather.hlsl), scaled to
// the whole target.

#ifndef SAMPLES
#define SAMPLES 2
#endif

cbuffer OutputPost : register(b5)
{
   float2 share;
   float2 inv_share;
};

cbuffer BlurGlobals : register(b0)
{
   float4 sample_offsets[SAMPLES];
};

void blur_vs(float4 position : POSITION, float2 uv : TEXCOORD0, out float4 sample_uvs[SAMPLES] : TEXCOORD0, out float4 out_position : SV_Position)
{
   const float2 full_uv = uv * inv_share;
   [unroll] for (int i = 0; i < SAMPLES; i++)
       sample_uvs[i] = full_uv.xyyx + sample_offsets[i];
   out_position = position;
}
