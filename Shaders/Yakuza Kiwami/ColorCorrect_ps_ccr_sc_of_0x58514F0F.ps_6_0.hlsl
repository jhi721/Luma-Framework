// Yakuza Kiwami ps_ccr_____sc_______of (0x58514F0F, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) and edited by
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
   float _61 = (c_ps_user[7].x) * _16;
   float _62 = (c_ps_user[7].y) * _16;
   float _63 = (c_ps_user[7].z) * _16;
   float _68 = (c_ps_user[11].x) * _17;
   float _69 = (c_ps_user[11].y) * _17;
   float _70 = (c_ps_user[11].z) * _17;
   float _75 = (c_ps_user[15].x) * _18;
   float _76 = (c_ps_user[15].y) * _18;
   float _77 = (c_ps_user[15].z) * _18;
   float _82 = _61 + _54;
   float _83 = _82 + _68;
   float _84 = _83 + _75;
   float _85 = _84 + (c_ps_user[3].x);
   float _86 = _62 + _55;
   float _87 = _86 + _69;
   float _88 = _87 + _76;
   float _89 = _88 + (c_ps_user[3].y);
   float _90 = _63 + _56;
   float _91 = _90 + _70;
   float _92 = _91 + _77;
   float _93 = _92 + (c_ps_user[3].z);
   float _94 = saturate(_85);
   float _95 = saturate(_89);
   float _96 = saturate(_93);
   SV_Target.x = _94;
   SV_Target.y = _95;
   SV_Target.z = _96;
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
