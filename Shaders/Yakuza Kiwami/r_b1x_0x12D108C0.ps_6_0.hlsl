// Yakuza Kiwami material r_b1x (0x12D108C0, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
// applied by a script: the inline Hable tonemap's saturate goes through
// YKR_MaterialCurve (tangent continuation in HDR), and the light pre-pass buffer t14 through YKR_LightBuffer.

#include "Includes/Common.hlsl"

Texture2D<float4> s0_t : register(t0);

cbuffer cb13 : register(b13)
{
   uint c_bool : packoffset(c000.x);
};

cbuffer cb6 : register(b6)
{
   float3 c_diffuse_scale : packoffset(c000.x);
   float c_global_opacity : packoffset(c000.w);
   float3 c_specular_scale : packoffset(c001.x);
   float c_emissive_scale : packoffset(c001.w);
};

cbuffer cb7 : register(b7)
{
   float4 c_attribute_vs[4] : packoffset(c000.x);
   float4 c_attribute_ps[4] : packoffset(c004.x);
   float c_mat_punchthrough_ref : packoffset(c008.x);
   float c_mat_glare_ref : packoffset(c008.y);
   float c_mat_fresnel : packoffset(c008.z);
   uint c_mat_custom_shader_flags : packoffset(c008.w);
   float3 c_mat_diffuse : packoffset(c009.x);
   float c_mat_opacity : packoffset(c009.w);
   float3 c_mat_specular : packoffset(c010.x);
   float c_mat_power : packoffset(c010.w);
   float3 c_mat_emissive : packoffset(c011.x);
   float c_mat_ambient : packoffset(c011.w);
};

cbuffer cb5 : register(b5)
{
   float2 c_uv0_offset : packoffset(c000.x);
   float2 c_uv1_offset : packoffset(c000.z);
   float2 c_uv2_offset : packoffset(c001.x);
   float c_blend_scale : packoffset(c001.z);
   float c_opacity_scale : packoffset(c001.w);
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

SamplerState s0_s : register(s0);

float4 main(
    linear float2 TEXCOORD : TEXCOORD,
    linear float4 COLOR : COLOR,
    linear float4 COLOR_1 : COLOR1,
    noperspective float4 SV_Position : SV_Position) : SV_Target
{
   float4 SV_Target = 0;
   float4 _17 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _22 = _17.x * _17.x;
   float _23 = _22 * COLOR.x;
   float _24 = _17.y * _17.y;
   float _25 = _24 * COLOR.y;
   float _26 = _17.z * _17.z;
   float _27 = _26 * COLOR.z;
   float _34 = _17.w * COLOR.w;
   float _35 = _34 * c_mat_opacity;
   float _36 = _35 * c_opacity_scale;
   float _37 = _36 * c_global_opacity;
   int _40 = c_bool & 16;
   bool _41 = (_40 == 0);
   float _107;
   float _108;
   float _109;
   float _125;
   float _126;
   float _127;
   if (!_41)
   {
      float _45 = c_tone_control.x * _23;
      float _46 = c_tone_control.x * _25;
      float _47 = c_tone_control.x * _27;
      float _49 = log2(_45);
      float _50 = log2(_46);
      float _51 = log2(_47);
      float _52 = _49 * c_tone_control.y;
      float _53 = _50 * c_tone_control.y;
      float _54 = _51 * c_tone_control.y;
      float _55 = exp2(_52);
      float _56 = exp2(_53);
      float _57 = exp2(_54);
      _107 = _55;
      _108 = _56;
      _109 = _57;
   }
   else
   {
      int _59 = c_bool & 32768;
      bool _60 = (_59 == 0);
      if (_60)
      {
         float _64 = c_tone_control.x * _23;
         float _65 = c_tone_control.x * _25;
         float _66 = c_tone_control.x * _27;
         float _67 = _64 * 2.0f;
         float _68 = _65 * 2.0f;
         float _69 = _66 * 2.0f;
         float _70 = _64 * 0.30000001192092896f;
         float _71 = _65 * 0.30000001192092896f;
         float _72 = _66 * 0.30000001192092896f;
         float _73 = _70 + 0.05000000074505806f;
         float _74 = _71 + 0.05000000074505806f;
         float _75 = _72 + 0.05000000074505806f;
         float _76 = _73 * _67;
         float _77 = _74 * _68;
         float _78 = _75 * _69;
         float _79 = _76 + 0.004000000189989805f;
         float _80 = _77 + 0.004000000189989805f;
         float _81 = _78 + 0.004000000189989805f;
         float _82 = _70 + 0.5f;
         float _83 = _71 + 0.5f;
         float _84 = _72 + 0.5f;
         float _85 = _82 * _67;
         float _86 = _83 * _68;
         float _87 = _84 * _69;
         float _88 = _85 + 0.06000000238418579f;
         float _89 = _86 + 0.06000000238418579f;
         float _90 = _87 + 0.06000000238418579f;
         float _91 = _79 / _88;
         float _92 = _80 / _89;
         float _93 = _81 / _90;
         float _94 = _91 + -0.06666666269302368f;
         float _95 = _92 + -0.06666666269302368f;
         float _96 = _93 + -0.06666666269302368f;
         float _97 = _94 * 1.3790643215179443f;
         float _98 = _95 * 1.3790643215179443f;
         float _99 = _96 * 1.3790643215179443f;
         float _100 = YKR_MaterialCurve(_97, _64);
         float _101 = YKR_MaterialCurve(_98, _65);
         float _102 = YKR_MaterialCurve(_99, _66);
         float _103 = sqrt(_100);
         float _104 = sqrt(_101);
         float _105 = sqrt(_102);
         _107 = _103;
         _108 = _104;
         _109 = _105;
      }
      else
      {
         _107 = _23;
         _108 = _25;
         _109 = _27;
      }
   }
   int _110 = c_bool & 4;
   bool _111 = (_110 == 0);
   if (_111)
   {
      float _113 = saturate(COLOR_1.x);
      float _118 = (c_fog_param[0].x) - _107;
      float _119 = (c_fog_param[0].y) - _108;
      float _120 = (c_fog_param[0].z) - _109;
      float _121 = _118 * _113;
      float _122 = _119 * _113;
      float _123 = _120 * _113;
      _125 = _121;
      _126 = _122;
      _127 = _123;
   }
   else
   {
      _125 = COLOR_1.x;
      _126 = COLOR_1.y;
      _127 = COLOR_1.z;
   }
   float _128 = _125 + _107;
   float _129 = _126 + _108;
   float _130 = _127 + _109;
   float _133 = _37 - c_mat_punchthrough_ref;
   bool _134 = (_133 < 0.0f);
   if (_134)
      discard;
   float _140 = (c_ps_col_scl[0].x) * _128;
   float _141 = (c_ps_col_scl[0].y) * _129;
   float _142 = (c_ps_col_scl[0].z) * _130;
   float _143 = (c_ps_col_scl[0].w) * _37;
   SV_Target.x = _140;
   SV_Target.y = _141;
   SV_Target.z = _142;
   SV_Target.w = _143;
   int _146 = c_bool & 8;
   bool _147 = (_146 != 0);
   float _148 = select(_147, _128, _140);
   float _149 = select(_147, _129, _141);
   float _150 = select(_147, _130, _142);
   float _151 = select(_147, _37, _143);
   SV_Target.x = _148;
   SV_Target.y = _149;
   SV_Target.z = _150;
   SV_Target.w = _151;
   return SV_Target;
}
