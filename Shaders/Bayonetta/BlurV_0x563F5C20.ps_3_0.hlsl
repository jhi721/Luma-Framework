// Bayonetta bloom blur "filter02" (V), raw ps_3_0 CRC32 0x563F5C20
// (dgVoodoo 2.87.5 0x91CC01D3). See Includes/BloomBlur_sm3.hlsli.
#include "Includes/BloomBlur_sm3.hlsli"

float4 main(float2 uv : TEXCOORD0) : COLOR0
{
   return BloomBlur(uv, float2(0.0, 1.0));
}
