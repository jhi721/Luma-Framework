// Yakuza Kiwami material fx_confetti_blend (0xE95A10D7, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
// applied by a script: the inline Hable tonemap's saturate goes through
// YKR_MaterialCurve (tangent continuation in HDR), and the light pre-pass buffer t14 through YKR_LightBuffer.

#include "Includes/Common.hlsl"

Texture2D<float4> s0_t : register(t0);

Texture2D<float4> s13_t : register(t13);

cbuffer cb13 : register(b13)
{
   uint c_bool : packoffset(c000.x);
};

cbuffer cb_user : register(b9)
{
   float4 c_ps_user[16] : packoffset(c000.x);
};

cbuffer cb1 : register(b1)
{
   float4 c_tone_control : packoffset(c000.x);
   float4 c_fog_param[2] : packoffset(c001.x);
   float4 c_ps_col_scl[1] : packoffset(c003.x);
   float4 c_ls_param[4] : packoffset(c004.x);
   float4 c_shadowmap_offset[1] : packoffset(c008.x);
   float4 c_sdw_param[3] : packoffset(c009.x);
   float4 c_sdw_param_ch[1] : packoffset(c012.x);
   float4 c_scene_ambient : packoffset(c013.x);
};

cbuffer cb3 : register(b3)
{
   float4 c_light_col[6] : packoffset(c000.x);
   float4 c_light_vec[6] : packoffset(c006.x);
};

SamplerState s0_s : register(s0);

SamplerState s13_s : register(s13);

float4 main(
    noperspective float4 SV_Position : SV_Position,
    linear float2 TEXCOORD : TEXCOORD,
    linear float4 TEXCOORD_1 : TEXCOORD1,
    linear float4 TEXCOORD_2 : TEXCOORD2) : SV_Target
{
   float4 SV_Target = 0;
   float4 _18 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _23 = _18.x * _18.x;
   float _24 = _18.y * _18.y;
   float _25 = _18.z * _18.z;
   int _28 = c_bool & 8;
   bool _29 = (_28 == 0);
   float _37;
   float _38;
   float _50;
   float _51;
   float _68;
   float _69;
   float _70;
   float _152;
   float _153;
   float _154;
   if (!_29)
   {
      float _31 = TEXCOORD_1.x / TEXCOORD_1.w;
      float _32 = TEXCOORD_1.y / TEXCOORD_1.w;
      float4 _33 = s13_t.Sample(s13_s, float2(_31, _32));
      _37 = _33.x;
      _38 = _33.y;
   }
   else
   {
      _37 = 1.0f;
      _38 = 1.0f;
   }
   if (!_29)
   {
      float _43 = (c_sdw_param[0].x) * _37;
      float _44 = (c_sdw_param[0].z) * _38;
      float _47 = _43 + (c_sdw_param[0].y);
      float _48 = _44 + (c_sdw_param[0].w);
      _50 = _47;
      _51 = _48;
   }
   else
   {
      _50 = _37;
      _51 = _38;
   }
   if (!_29)
   {
      float _53 = _51 * _50;
      float _58 = 1.0f - (c_light_col[5].x);
      float _59 = 1.0f - (c_light_col[5].y);
      float _60 = 1.0f - (c_light_col[5].z);
      float _61 = _58 * _53;
      float _62 = _59 * _53;
      float _63 = _60 * _53;
      float _64 = _61 + (c_light_col[5].x);
      float _65 = _62 + (c_light_col[5].y);
      float _66 = _63 + (c_light_col[5].z);
      _68 = _64;
      _69 = _65;
      _70 = _66;
   }
   else
   {
      _68 = 1.0f;
      _69 = 1.0f;
      _70 = 1.0f;
   }
   float _75 = _68 * TEXCOORD_2.x;
   float _76 = _69 * TEXCOORD_2.y;
   float _77 = _70 * TEXCOORD_2.z;
   float _78 = (c_ps_user[0].x) + _75;
   float _79 = _23 * _78;
   float _80 = (c_ps_user[0].y) + _76;
   float _81 = _24 * _80;
   float _82 = (c_ps_user[0].z) + _77;
   float _83 = _25 * _82;
   float _84 = _18.w * TEXCOORD_2.w;
   int _85 = c_bool & 16;
   bool _86 = (_85 == 0);
   if (!_86)
   {
      float _90 = c_tone_control.x * _79;
      float _91 = c_tone_control.x * _81;
      float _92 = c_tone_control.x * _83;
      float _94 = log2(_90);
      float _95 = log2(_91);
      float _96 = log2(_92);
      float _97 = _94 * c_tone_control.y;
      float _98 = _95 * c_tone_control.y;
      float _99 = _96 * c_tone_control.y;
      float _100 = exp2(_97);
      float _101 = exp2(_98);
      float _102 = exp2(_99);
      _152 = _100;
      _153 = _101;
      _154 = _102;
   }
   else
   {
      int _104 = c_bool & 32768;
      bool _105 = (_104 == 0);
      if (_105)
      {
         float _109 = c_tone_control.x * _79;
         float _110 = c_tone_control.x * _81;
         float _111 = c_tone_control.x * _83;
         float _112 = _109 * 2.0f;
         float _113 = _110 * 2.0f;
         float _114 = _111 * 2.0f;
         float _115 = _109 * 0.30000001192092896f;
         float _116 = _110 * 0.30000001192092896f;
         float _117 = _111 * 0.30000001192092896f;
         float _118 = _115 + 0.05000000074505806f;
         float _119 = _116 + 0.05000000074505806f;
         float _120 = _117 + 0.05000000074505806f;
         float _121 = _118 * _112;
         float _122 = _119 * _113;
         float _123 = _120 * _114;
         float _124 = _121 + 0.004000000189989805f;
         float _125 = _122 + 0.004000000189989805f;
         float _126 = _123 + 0.004000000189989805f;
         float _127 = _115 + 0.5f;
         float _128 = _116 + 0.5f;
         float _129 = _117 + 0.5f;
         float _130 = _127 * _112;
         float _131 = _128 * _113;
         float _132 = _129 * _114;
         float _133 = _130 + 0.06000000238418579f;
         float _134 = _131 + 0.06000000238418579f;
         float _135 = _132 + 0.06000000238418579f;
         float _136 = _124 / _133;
         float _137 = _125 / _134;
         float _138 = _126 / _135;
         float _139 = _136 + -0.06666666269302368f;
         float _140 = _137 + -0.06666666269302368f;
         float _141 = _138 + -0.06666666269302368f;
         float _142 = _139 * 1.3790643215179443f;
         float _143 = _140 * 1.3790643215179443f;
         float _144 = _141 * 1.3790643215179443f;
         float _145 = YKR_MaterialCurve(_142, _109);
         float _146 = YKR_MaterialCurve(_143, _110);
         float _147 = YKR_MaterialCurve(_144, _111);
         float _148 = sqrt(_145);
         float _149 = sqrt(_146);
         float _150 = sqrt(_147);
         _152 = _148;
         _153 = _149;
         _154 = _150;
      }
      else
      {
         _152 = _79;
         _153 = _81;
         _154 = _83;
      }
   }
   float _160 = (c_ps_col_scl[0].x) * _152;
   float _161 = (c_ps_col_scl[0].y) * _153;
   float _162 = (c_ps_col_scl[0].z) * _154;
   float _163 = (c_ps_col_scl[0].w) * _84;
   SV_Target.x = _160;
   SV_Target.y = _161;
   SV_Target.z = _162;
   SV_Target.w = _163;
   int _166 = c_bool & 8;
   bool _167 = (_166 != 0);
   float _168 = select(_167, _152, _160);
   float _169 = select(_167, _153, _161);
   float _170 = select(_167, _154, _162);
   float _171 = select(_167, _84, _163);
   SV_Target.x = _168;
   SV_Target.y = _169;
   SV_Target.z = _170;
   SV_Target.w = _171;
   return SV_Target;
}
