// Bayonetta bloom blur "filter01" (H), raw ps_3_0 CRC32 0x0F24B7EF
// (dgVoodoo 2.87.5 0xCE62BA4B). See Includes/BloomBlur_sm3.hlsli.
#include "Includes/BloomBlur_sm3.hlsli"

float4 main(float2 uv : TEXCOORD0) : COLOR0
{
   return BloomBlur(uv, float2(1.0, 0.0));
}
