// Yakuza Kiwami ps_ccr________gm_gi_of (0xCB74D2A0, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) and edited by
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
   float _23 = dot(float3((c_ps_user[5].x), (c_ps_user[5].y), (c_ps_user[5].z)), float3(_16, _17, _18));
   float _28 = dot(float3((c_ps_user[9].x), (c_ps_user[9].y), (c_ps_user[9].z)), float3(_16, _17, _18));
   float _33 = dot(float3((c_ps_user[13].x), (c_ps_user[13].y), (c_ps_user[13].z)), float3(_16, _17, _18));
   float _38 = (c_ps_user[1].x) + _23;
   float _39 = (c_ps_user[1].y) + _28;
   float _40 = (c_ps_user[1].z) + _33;
   float _41 = _39 * 0.45454543828964233f;
   float _42 = _40 * 0.45454543828964233f;
   float _43 = log2(_6.x);
   float _44 = log2(_6.y);
   float _45 = log2(_6.z);
   float _46 = _43 * 0.45454543828964233f;
   float _47 = _46 * _38;
   float _48 = _41 * _44;
   float _49 = _42 * _45;
   float _50 = exp2(_47);
   float _51 = exp2(_48);
   float _52 = exp2(_49);
   float _53 = saturate(_50);
   float _54 = saturate(_51);
   float _55 = saturate(_52);
   float _60 = (c_ps_user[6].x) * _16;
   float _61 = (c_ps_user[6].y) * _16;
   float _62 = (c_ps_user[6].z) * _16;
   float _63 = 1.0f - _60;
   float _64 = 1.0f - _61;
   float _65 = 1.0f - _62;
   float _70 = (c_ps_user[10].x) * _17;
   float _71 = (c_ps_user[10].y) * _17;
   float _72 = (c_ps_user[10].z) * _17;
   float _73 = 1.0f - _70;
   float _74 = 1.0f - _71;
   float _75 = 1.0f - _72;
   float _80 = (c_ps_user[14].x) * _18;
   float _81 = (c_ps_user[14].y) * _18;
   float _82 = (c_ps_user[14].z) * _18;
   float _83 = 1.0f - _80;
   float _84 = 1.0f - _81;
   float _85 = 1.0f - _82;
   float _90 = _63 * _53;
   float _91 = _90 * (c_ps_user[2].x);
   float _92 = _91 * _73;
   float _93 = _92 * _83;
   float _94 = _64 * _54;
   float _95 = _94 * (c_ps_user[2].y);
   float _96 = _95 * _74;
   float _97 = _96 * _84;
   float _98 = _65 * _55;
   float _99 = _98 * (c_ps_user[2].z);
   float _100 = _99 * _75;
   float _101 = _100 * _85;
   float _102 = saturate(_93);
   float _103 = saturate(_97);
   float _104 = saturate(_101);
   float _109 = (c_ps_user[7].x) * _16;
   float _110 = (c_ps_user[7].y) * _16;
   float _111 = (c_ps_user[7].z) * _16;
   float _116 = (c_ps_user[11].x) * _17;
   float _117 = (c_ps_user[11].y) * _17;
   float _118 = (c_ps_user[11].z) * _17;
   float _123 = (c_ps_user[15].x) * _18;
   float _124 = (c_ps_user[15].y) * _18;
   float _125 = (c_ps_user[15].z) * _18;
   float _130 = _109 + _102;
   float _131 = _130 + _116;
   float _132 = _131 + _123;
   float _133 = _132 + (c_ps_user[3].x);
   float _134 = _110 + _103;
   float _135 = _134 + _117;
   float _136 = _135 + _124;
   float _137 = _136 + (c_ps_user[3].y);
   float _138 = _111 + _104;
   float _139 = _138 + _118;
   float _140 = _139 + _125;
   float _141 = _140 + (c_ps_user[3].z);
   float _142 = saturate(_133);
   float _143 = saturate(_137);
   float _144 = saturate(_141);
   SV_Target.x = _142;
   SV_Target.y = _143;
   SV_Target.z = _144;
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
