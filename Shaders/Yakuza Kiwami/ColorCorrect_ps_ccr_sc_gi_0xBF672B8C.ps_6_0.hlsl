// Yakuza Kiwami ps_ccr_____sc____gi___ (0xBF672B8C, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) and edited by
// a script: it becomes YKR_ColorCorrectGrade, each scene sample through YKR_ColorCorrectScene, run per grade (vanilla, HDR) by the main at the end.

#include "Includes/ColorCorrect.hlsl"

Texture2D<float4> s0_t : register(t0);

cbuffer cb_user : register(b9)
{
   float4 c_ps_user[16] : packoffset(c000.x);
};

SamplerState s0_s : register(s0);

float4 YKR_ColorCorrectGrade(float2 TEXCOORD)
{
   float4 SV_Target = 0;
   float4 _6 = YKR_ColorCorrectScene(s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y)));
   float _10 = dot(float3(_6.x, _6.y, _6.z), float3(0.3333333432674408f, 0.3333333432674408f, 0.3333333432674408f));
   float _11 = 1.0f - _10;
   float _12 = _11 * _11;
   float _13 = _10 * _10;
   float _14 = 1.0f - _13;
   float _15 = _14 - _12;
   float _16 = saturate(_12);
   float _17 = saturate(_15);
   float _18 = saturate(_13);
   float _19 = dot(float3(0.29899999499320984f, 0.5870000123977661f, 0.11400000005960464f), float3(_6.x, _6.y, _6.z));
   float _20 = dot(float3(-0.16874000430107117f, -0.33125999569892883f, 0.5f), float3(_6.x, _6.y, _6.z));
   float _21 = dot(float3(0.5f, -0.4186899960041046f, -0.08130999654531479f), float3(_6.x, _6.y, _6.z));
   float _26 = dot(float3((c_ps_user[4].x), (c_ps_user[4].y), (c_ps_user[4].z)), float3(_16, _17, _18));
   float _31 = dot(float3((c_ps_user[8].x), (c_ps_user[8].y), (c_ps_user[8].z)), float3(_16, _17, _18));
   float _35 = (c_ps_user[0].w) + _31;
   float _36 = _26 + 1.0f;
   float _37 = _36 + (c_ps_user[0].z);
   float _38 = _37 * _20;
   float _39 = _37 * _21;
   float _41 = (c_ps_user[0].y) + _19;
   float _42 = dot(float3(1.0f, 0.0f, 1.4019999504089355f), float3(_41, _38, _39));
   float _43 = dot(float3(1.0f, -0.3441399931907654f, -0.714139997959137f), float3(_41, _38, _39));
   float _44 = dot(float3(1.0f, 1.7719999551773071f, 0.0f), float3(_41, _38, _39));
   float _45 = _35 * 2.0f;
   float _46 = 1.0f - _45;
   float _47 = 1.0f / _46;
   float _48 = _47 * _42;
   float _49 = _47 * _43;
   float _50 = _47 * _44;
   float _51 = _48 - _35;
   float _52 = _49 - _35;
   float _53 = _50 - _35;
   float _54 = saturate(_51);
   float _55 = saturate(_52);
   float _56 = saturate(_53);
   float _61 = (c_ps_user[6].x) * _16;
   float _62 = (c_ps_user[6].y) * _16;
   float _63 = (c_ps_user[6].z) * _16;
   float _64 = 1.0f - _61;
   float _65 = 1.0f - _62;
   float _66 = 1.0f - _63;
   float _71 = (c_ps_user[10].x) * _17;
   float _72 = (c_ps_user[10].y) * _17;
   float _73 = (c_ps_user[10].z) * _17;
   float _74 = 1.0f - _71;
   float _75 = 1.0f - _72;
   float _76 = 1.0f - _73;
   float _81 = (c_ps_user[14].x) * _18;
   float _82 = (c_ps_user[14].y) * _18;
   float _83 = (c_ps_user[14].z) * _18;
   float _84 = 1.0f - _81;
   float _85 = 1.0f - _82;
   float _86 = 1.0f - _83;
   float _91 = _64 * _54;
   float _92 = _91 * (c_ps_user[2].x);
   float _93 = _92 * _74;
   float _94 = _93 * _84;
   float _95 = _65 * _55;
   float _96 = _95 * (c_ps_user[2].y);
   float _97 = _96 * _75;
   float _98 = _97 * _85;
   float _99 = _66 * _56;
   float _100 = _99 * (c_ps_user[2].z);
   float _101 = _100 * _76;
   float _102 = _101 * _86;
   float _103 = saturate(_94);
   float _104 = saturate(_98);
   float _105 = saturate(_102);
   SV_Target.x = _103;
   SV_Target.y = _104;
   SV_Target.z = _105;
   SV_Target.w = 1.0f;
   return SV_Target;
}

float4 main(
    linear float2 TEXCOORD : TEXCOORD) : SV_Target
{
   const float4 vanilla_graded = YKR_ColorCorrectGrade(TEXCOORD);
   float4 hdr_graded = vanilla_graded;
   [branch] if (YKR_ColorCorrectHDR())
   {
      ykr_color_correct_compressed = true;
      hdr_graded = YKR_ColorCorrectGrade(TEXCOORD);
   }
   return YKR_ColorCorrectOutput(vanilla_graded, hdr_graded);
}
