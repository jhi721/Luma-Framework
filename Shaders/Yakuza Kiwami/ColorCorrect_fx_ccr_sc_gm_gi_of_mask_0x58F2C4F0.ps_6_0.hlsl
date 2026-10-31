// Yakuza Kiwami fx_ccr_____sc_gm_gi_of_mask (0x58F2C4F0, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) and edited by
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
   float _15 = dot(float3(_11.x, _11.y, _11.z), float3(0.3333333432674408f, 0.3333333432674408f, 0.3333333432674408f));
   float _16 = 1.0f - _15;
   float _17 = _16 * _16;
   float _18 = _15 * _15;
   float _19 = 1.0f - _18;
   float _20 = _19 - _17;
   float _21 = saturate(_17);
   float _22 = saturate(_20);
   float _23 = saturate(_18);
   float _24 = dot(float3(0.29899999499320984f, 0.5870000123977661f, 0.11400000005960464f), float3(_11.x, _11.y, _11.z));
   float _25 = dot(float3(-0.16874000430107117f, -0.33125999569892883f, 0.5f), float3(_11.x, _11.y, _11.z));
   float _26 = dot(float3(0.5f, -0.4186899960041046f, -0.08130999654531479f), float3(_11.x, _11.y, _11.z));
   float _31 = dot(float3((c_ps_user[4].x), (c_ps_user[4].y), (c_ps_user[4].z)), float3(_21, _22, _23));
   float _36 = dot(float3((c_ps_user[8].x), (c_ps_user[8].y), (c_ps_user[8].z)), float3(_21, _22, _23));
   float _40 = (c_ps_user[0].w) + _36;
   float _41 = _31 + 1.0f;
   float _42 = _41 + (c_ps_user[0].z);
   float _43 = _42 * _25;
   float _44 = _42 * _26;
   float _46 = (c_ps_user[0].y) + _24;
   float _47 = dot(float3(1.0f, 0.0f, 1.4019999504089355f), float3(_46, _43, _44));
   float _48 = dot(float3(1.0f, -0.3441399931907654f, -0.714139997959137f), float3(_46, _43, _44));
   float _49 = dot(float3(1.0f, 1.7719999551773071f, 0.0f), float3(_46, _43, _44));
   float _50 = _40 * 2.0f;
   float _51 = 1.0f - _50;
   float _52 = 1.0f / _51;
   float _53 = _52 * _47;
   float _54 = _52 * _48;
   float _55 = _52 * _49;
   float _56 = _53 - _40;
   float _57 = _54 - _40;
   float _58 = _55 - _40;
   float _59 = saturate(_56);
   float _60 = saturate(_57);
   float _61 = saturate(_58);
   float _66 = dot(float3((c_ps_user[5].x), (c_ps_user[5].y), (c_ps_user[5].z)), float3(_21, _22, _23));
   float _71 = dot(float3((c_ps_user[9].x), (c_ps_user[9].y), (c_ps_user[9].z)), float3(_21, _22, _23));
   float _76 = dot(float3((c_ps_user[13].x), (c_ps_user[13].y), (c_ps_user[13].z)), float3(_21, _22, _23));
   float _81 = (c_ps_user[1].x) + _66;
   float _82 = (c_ps_user[1].y) + _71;
   float _83 = (c_ps_user[1].z) + _76;
   float _84 = _82 * 0.45454543828964233f;
   float _85 = _83 * 0.45454543828964233f;
   float _86 = log2(_59);
   float _87 = log2(_60);
   float _88 = log2(_61);
   float _89 = _86 * 0.45454543828964233f;
   float _90 = _89 * _81;
   float _91 = _84 * _87;
   float _92 = _85 * _88;
   float _93 = exp2(_90);
   float _94 = exp2(_91);
   float _95 = exp2(_92);
   float _96 = saturate(_93);
   float _97 = saturate(_94);
   float _98 = saturate(_95);
   float _103 = (c_ps_user[6].x) * _21;
   float _104 = (c_ps_user[6].y) * _21;
   float _105 = (c_ps_user[6].z) * _21;
   float _106 = 1.0f - _103;
   float _107 = 1.0f - _104;
   float _108 = 1.0f - _105;
   float _113 = (c_ps_user[10].x) * _22;
   float _114 = (c_ps_user[10].y) * _22;
   float _115 = (c_ps_user[10].z) * _22;
   float _116 = 1.0f - _113;
   float _117 = 1.0f - _114;
   float _118 = 1.0f - _115;
   float _123 = (c_ps_user[14].x) * _23;
   float _124 = (c_ps_user[14].y) * _23;
   float _125 = (c_ps_user[14].z) * _23;
   float _126 = 1.0f - _123;
   float _127 = 1.0f - _124;
   float _128 = 1.0f - _125;
   float _133 = _106 * _96;
   float _134 = _133 * (c_ps_user[2].x);
   float _135 = _134 * _116;
   float _136 = _135 * _126;
   float _137 = _107 * _97;
   float _138 = _137 * (c_ps_user[2].y);
   float _139 = _138 * _117;
   float _140 = _139 * _127;
   float _141 = _108 * _98;
   float _142 = _141 * (c_ps_user[2].z);
   float _143 = _142 * _118;
   float _144 = _143 * _128;
   float _145 = saturate(_136);
   float _146 = saturate(_140);
   float _147 = saturate(_144);
   float _152 = (c_ps_user[7].x) * _21;
   float _153 = (c_ps_user[7].y) * _21;
   float _154 = (c_ps_user[7].z) * _21;
   float _159 = (c_ps_user[11].x) * _22;
   float _160 = (c_ps_user[11].y) * _22;
   float _161 = (c_ps_user[11].z) * _22;
   float _166 = (c_ps_user[15].x) * _23;
   float _167 = (c_ps_user[15].y) * _23;
   float _168 = (c_ps_user[15].z) * _23;
   float _173 = _152 + _145;
   float _174 = _173 + _159;
   float _175 = _174 + _166;
   float _176 = _175 + (c_ps_user[3].x);
   float _177 = _153 + _146;
   float _178 = _177 + _160;
   float _179 = _178 + _167;
   float _180 = _179 + (c_ps_user[3].y);
   float _181 = _154 + _147;
   float _182 = _181 + _161;
   float _183 = _182 + _168;
   float _184 = _183 + (c_ps_user[3].z);
   float _185 = saturate(_176);
   float _186 = saturate(_180);
   float _187 = saturate(_184);
   float4 _188 = s2_t.Sample(s2_s, float2(TEXCOORD.z, TEXCOORD.w));
   float _192 = (c_ps_user[1].w) * _188.x;
   float _193 = _185 - _11.x;
   float _194 = _186 - _11.y;
   float _195 = _187 - _11.z;
   float _196 = _192 * _193;
   float _197 = _192 * _194;
   float _198 = _192 * _195;
   float _199 = _196 + _11.x;
   float _200 = _197 + _11.y;
   float _201 = _198 + _11.z;
   SV_Target.x = _199;
   SV_Target.y = _200;
   SV_Target.z = _201;
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
