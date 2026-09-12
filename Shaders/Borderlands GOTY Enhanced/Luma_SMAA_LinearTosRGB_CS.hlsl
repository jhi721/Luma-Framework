#include "../Includes/Common.hlsl"

Texture2D tex0 : register(t0);
RWTexture2D<float4> uav0 : register(u0);
RWTexture2D<float4> uav1 : register(u1);

[numthreads(8,8,1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
	// tex0 - the game's post-process buffer; under POST_PROCESS_SPACE_TYPE 0 (Borderlands GOTY) already gamma-encoded, fp16, 1.0 = paper white
	// uav0 - pass-through copy (SMAA neighborhood blending reads this)
	// uav1 - sRGB encode applied on top of that stored gamma (SMAA edge detection reads this)

	float4 color = tex0.Load(int3(dtid.xy, 0));
	uav0[dtid.xy] = color;
	color.rgb = linear_to_sRGB_gamma(color.rgb);
	uav1[dtid.xy] = color;
}
