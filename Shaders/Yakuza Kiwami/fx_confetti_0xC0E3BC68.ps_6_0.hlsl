// Yakuza Kiwami material fx_confetti (0xC0E3BC68, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
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
    linear float3 TEXCOORD_2 : TEXCOORD2) : SV_Target
{
   float4 SV_Target = 0;
   float4 _17 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _22 = _17.x * _17.x;
   float _23 = _17.y * _17.y;
   float _24 = _17.z * _17.z;
   int _27 = c_bool & 8;
   bool _28 = (_27 == 0);
   float _36;
   float _37;
   float _49;
   float _50;
   float _67;
   float _68;
   float _69;
   float _150;
   float _151;
   float _152;
   if (!_28)
   {
      float _30 = TEXCOORD_1.x / TEXCOORD_1.w;
      float _31 = TEXCOORD_1.y / TEXCOORD_1.w;
      float4 _32 = s13_t.Sample(s13_s, float2(_30, _31));
      _36 = _32.x;
      _37 = _32.y;
   }
   else
   {
      _36 = 1.0f;
      _37 = 1.0f;
   }
   if (!_28)
   {
      float _42 = (c_sdw_param[0].x) * _36;
      float _43 = (c_sdw_param[0].z) * _37;
      float _46 = _42 + (c_sdw_param[0].y);
      float _47 = _43 + (c_sdw_param[0].w);
      _49 = _46;
      _50 = _47;
   }
   else
   {
      _49 = _36;
      _50 = _37;
   }
   if (!_28)
   {
      float _52 = _50 * _49;
      float _57 = 1.0f - (c_light_col[5].x);
      float _58 = 1.0f - (c_light_col[5].y);
      float _59 = 1.0f - (c_light_col[5].z);
      float _60 = _57 * _52;
      float _61 = _58 * _52;
      float _62 = _59 * _52;
      float _63 = _60 + (c_light_col[5].x);
      float _64 = _61 + (c_light_col[5].y);
      float _65 = _62 + (c_light_col[5].z);
      _67 = _63;
      _68 = _64;
      _69 = _65;
   }
   else
   {
      _67 = 1.0f;
      _68 = 1.0f;
      _69 = 1.0f;
   }
   float _74 = _67 * TEXCOORD_2.x;
   float _75 = _68 * TEXCOORD_2.y;
   float _76 = _69 * TEXCOORD_2.z;
   float _77 = (c_ps_user[0].x) + _74;
   float _78 = _22 * _77;
   float _79 = (c_ps_user[0].y) + _75;
   float _80 = _23 * _79;
   float _81 = (c_ps_user[0].z) + _76;
   float _82 = _24 * _81;
   int _83 = c_bool & 16;
   bool _84 = (_83 == 0);
   if (!_84)
   {
      float _88 = c_tone_control.x * _78;
      float _89 = c_tone_control.x * _80;
      float _90 = c_tone_control.x * _82;
      float _92 = log2(_88);
      float _93 = log2(_89);
      float _94 = log2(_90);
      float _95 = _92 * c_tone_control.y;
      float _96 = _93 * c_tone_control.y;
      float _97 = _94 * c_tone_control.y;
      float _98 = exp2(_95);
      float _99 = exp2(_96);
      float _100 = exp2(_97);
      _150 = _98;
      _151 = _99;
      _152 = _100;
   }
   else
   {
      int _102 = c_bool & 32768;
      bool _103 = (_102 == 0);
      if (_103)
      {
         float _107 = c_tone_control.x * _78;
         float _108 = c_tone_control.x * _80;
         float _109 = c_tone_control.x * _82;
         float _110 = _107 * 2.0f;
         float _111 = _108 * 2.0f;
         float _112 = _109 * 2.0f;
         float _113 = _107 * 0.30000001192092896f;
         float _114 = _108 * 0.30000001192092896f;
         float _115 = _109 * 0.30000001192092896f;
         float _116 = _113 + 0.05000000074505806f;
         float _117 = _114 + 0.05000000074505806f;
         float _118 = _115 + 0.05000000074505806f;
         float _119 = _116 * _110;
         float _120 = _117 * _111;
         float _121 = _118 * _112;
         float _122 = _119 + 0.004000000189989805f;
         float _123 = _120 + 0.004000000189989805f;
         float _124 = _121 + 0.004000000189989805f;
         float _125 = _113 + 0.5f;
         float _126 = _114 + 0.5f;
         float _127 = _115 + 0.5f;
         float _128 = _125 * _110;
         float _129 = _126 * _111;
         float _130 = _127 * _112;
         float _131 = _128 + 0.06000000238418579f;
         float _132 = _129 + 0.06000000238418579f;
         float _133 = _130 + 0.06000000238418579f;
         float _134 = _122 / _131;
         float _135 = _123 / _132;
         float _136 = _124 / _133;
         float _137 = _134 + -0.06666666269302368f;
         float _138 = _135 + -0.06666666269302368f;
         float _139 = _136 + -0.06666666269302368f;
         float _140 = _137 * 1.3790643215179443f;
         float _141 = _138 * 1.3790643215179443f;
         float _142 = _139 * 1.3790643215179443f;
         float _143 = YKR_MaterialCurve(_140, _107);
         float _144 = YKR_MaterialCurve(_141, _108);
         float _145 = YKR_MaterialCurve(_142, _109);
         float _146 = sqrt(_143);
         float _147 = sqrt(_144);
         float _148 = sqrt(_145);
         _150 = _146;
         _151 = _147;
         _152 = _148;
      }
      else
      {
         _150 = _78;
         _151 = _80;
         _152 = _82;
      }
   }
   float _158 = (c_ps_col_scl[0].x) * _150;
   float _159 = (c_ps_col_scl[0].y) * _151;
   float _160 = (c_ps_col_scl[0].z) * _152;
   float _161 = (c_ps_col_scl[0].w) * _17.w;
   SV_Target.x = _158;
   SV_Target.y = _159;
   SV_Target.z = _160;
   SV_Target.w = _161;
   int _164 = c_bool & 8;
   bool _165 = (_164 != 0);
   float _166 = select(_165, _150, _158);
   float _167 = select(_165, _151, _159);
   float _168 = select(_165, _152, _160);
   float _169 = select(_165, _17.w, _161);
   SV_Target.x = _166;
   SV_Target.y = _167;
   SV_Target.z = _168;
   SV_Target.w = _169;
   return SV_Target;
}
