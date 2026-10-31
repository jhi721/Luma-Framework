// Yakuza Kiwami fx_ccr_____sc_gm_gi_of_mask_n (0x9DAF7DB7, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) and edited by
// a script: it becomes YKR_ColorCorrectGrade, each scene sample through YKR_ColorCorrectScene, run per grade (vanilla, HDR) by the main at the end.

#include "Includes/ColorCorrect.hlsl"

Texture2D<float4> s0_t : register(t0);

Texture2D<float4> s2_t : register(t2);

cbuffer cb_user : register(b9)
{
   float4 c_ps_user[16] : packoffset(c000.x);
};

SamplerState s0_s : register(s0);

SamplerState s2_s : register(s2);

float4 YKR_ColorCorrectGrade(float4 COLOR, float4 TEXCOORD)
{
   float4 SV_Target = 0;
   float4 _11 = YKR_ColorCorrectScene(s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y)));
   float _15 = dot(float3(0.29899999499320984f, 0.5870000123977661f, 0.11400000005960464f), float3(_11.x, _11.y, _11.z));
   float _16 = dot(float3(-0.16874000430107117f, -0.33125999569892883f, 0.5f), float3(_11.x, _11.y, _11.z));
   float _17 = dot(float3(0.5f, -0.4186899960041046f, -0.08130999654531479f), float3(_11.x, _11.y, _11.z));
   float _21 = (c_ps_user[0].z) + 1.0f;
   float _22 = _21 * _16;
   float _23 = _21 * _17;
   float _25 = (c_ps_user[0].y) + _15;
   float _26 = dot(float3(1.0f, 0.0f, 1.4019999504089355f), float3(_25, _22, _23));
   float _27 = dot(float3(1.0f, -0.3441399931907654f, -0.714139997959137f), float3(_25, _22, _23));
   float _28 = dot(float3(1.0f, 1.7719999551773071f, 0.0f), float3(_25, _22, _23));
   float _29 = (c_ps_user[0].w) * 2.0f;
   float _30 = 1.0f - _29;
   float _31 = 1.0f / _30;
   float _32 = _31 * _26;
   float _33 = _31 * _27;
   float _34 = _31 * _28;
   float _35 = _32 - (c_ps_user[0].w);
   float _36 = _33 - (c_ps_user[0].w);
   float _37 = _34 - (c_ps_user[0].w);
   float _38 = saturate(_35);
   float _39 = saturate(_36);
   float _40 = saturate(_37);
   float _45 = (c_ps_user[1].x) * 0.45454543828964233f;
   float _46 = (c_ps_user[1].y) * 0.45454543828964233f;
   float _47 = (c_ps_user[1].z) * 0.45454543828964233f;
   float _48 = log2(_38);
   float _49 = log2(_39);
   float _50 = log2(_40);
   float _51 = _45 * _48;
   float _52 = _46 * _49;
   float _53 = _47 * _50;
   float _54 = exp2(_51);
   float _55 = exp2(_52);
   float _56 = exp2(_53);
   float _57 = saturate(_54);
   float _58 = saturate(_55);
   float _59 = saturate(_56);
   float _64 = (c_ps_user[2].x) * _57;
   float _65 = (c_ps_user[2].y) * _58;
   float _66 = (c_ps_user[2].z) * _59;
   float _67 = saturate(_64);
   float _68 = saturate(_65);
   float _69 = saturate(_66);
   float _74 = (c_ps_user[3].x) + _67;
   float _75 = (c_ps_user[3].y) + _68;
   float _76 = (c_ps_user[3].z) + _69;
   float _77 = saturate(_74);
   float _78 = saturate(_75);
   float _79 = saturate(_76);
   float4 _80 = s2_t.Sample(s2_s, float2(TEXCOORD.z, TEXCOORD.w));
   float _84 = (c_ps_user[1].w) * _80.x;
   float _85 = _77 - _11.x;
   float _86 = _78 - _11.y;
   float _87 = _79 - _11.z;
   float _88 = _84 * _85;
   float _89 = _84 * _86;
   float _90 = _84 * _87;
   float _91 = _88 + _11.x;
   float _92 = _89 + _11.y;
   float _93 = _90 + _11.z;
   SV_Target.x = _91;
   SV_Target.y = _92;
   SV_Target.z = _93;
   SV_Target.w = COLOR.w;
   return SV_Target;
}

float4 main(
    linear float4 COLOR : COLOR,
    linear float4 TEXCOORD : TEXCOORD) : SV_Target
{
   const float4 vanilla_graded = YKR_ColorCorrectGrade(COLOR, TEXCOORD);
   float4 hdr_graded = vanilla_graded;
   [branch] if (YKR_ColorCorrectHDR())
   {
      ykr_color_correct_compressed = true;
      hdr_graded = YKR_ColorCorrectGrade(COLOR, TEXCOORD);
   }
   return YKR_ColorCorrectOutput(vanilla_graded, hdr_graded);
}
