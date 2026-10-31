// Yakuza Kiwami material r_o1d_b(vr)2d_b(va)3d[xlt] (0xA4E07A71, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
// applied by a script: the inline Hable tonemap's saturate goes through
// YKR_MaterialCurve (tangent continuation in HDR), and the light pre-pass buffer t14 through YKR_LightBuffer.

#include "Includes/Common.hlsl"

Texture2D<float4> s0_t : register(t0);

Texture2D<float4> s1_t : register(t1);

Texture2D<float4> s7_t : register(t7);

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

SamplerState s1_s : register(s1);

SamplerState s7_s : register(s7);

float4 main(
    linear float4 TEXCOORD : TEXCOORD,
    linear float2 TEXCOORD_1 : TEXCOORD1,
    linear float4 COLOR : COLOR,
    linear float4 COLOR_1 : COLOR1,
    noperspective float4 SV_Position : SV_Position) : SV_Target
{
   float4 SV_Target = 0;
   float4 _26 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _31 = _26.x * _26.x;
   float _32 = _26.y * _26.y;
   float _33 = _26.z * _26.z;
   float4 _34 = s1_t.Sample(s1_s, float2(TEXCOORD.z, TEXCOORD.w));
   float _39 = _34.x * _34.x;
   float _40 = _34.y * _34.y;
   float _41 = _34.z * _34.z;
   float4 _42 = s7_t.Sample(s7_s, float2(TEXCOORD_1.x, TEXCOORD_1.y));
   float _47 = _42.x * _42.x;
   float _48 = _42.y * _42.y;
   float _49 = _42.z * _42.z;
   float _50 = _34.w * COLOR_1.w;
   float _51 = _42.w * COLOR.w;
   float _52 = _39 - _31;
   float _53 = _40 - _32;
   float _54 = _41 - _33;
   float _55 = _52 * _50;
   float _56 = _53 * _50;
   float _57 = _54 * _50;
   float _58 = _55 + _31;
   float _59 = _56 + _32;
   float _60 = _57 + _33;
   float _61 = _47 - _58;
   float _62 = _48 - _59;
   float _63 = _49 - _60;
   float _64 = _61 * _51;
   float _65 = _62 * _51;
   float _66 = _63 * _51;
   float _67 = _64 + _58;
   float _68 = _65 + _59;
   float _69 = _66 + _60;
   float _70 = _67 * COLOR.x;
   float _71 = _68 * COLOR.y;
   float _72 = _69 * COLOR.z;
   float _79 = c_mat_opacity * _26.w;
   float _80 = _79 * c_opacity_scale;
   float _81 = _80 * c_global_opacity;
   int _84 = c_bool & 16;
   bool _85 = (_84 == 0);
   float _151;
   float _152;
   float _153;
   float _169;
   float _170;
   float _171;
   if (!_85)
   {
      float _89 = c_tone_control.x * _70;
      float _90 = c_tone_control.x * _71;
      float _91 = c_tone_control.x * _72;
      float _93 = log2(_89);
      float _94 = log2(_90);
      float _95 = log2(_91);
      float _96 = _93 * c_tone_control.y;
      float _97 = _94 * c_tone_control.y;
      float _98 = _95 * c_tone_control.y;
      float _99 = exp2(_96);
      float _100 = exp2(_97);
      float _101 = exp2(_98);
      _151 = _99;
      _152 = _100;
      _153 = _101;
   }
   else
   {
      int _103 = c_bool & 32768;
      bool _104 = (_103 == 0);
      if (_104)
      {
         float _108 = c_tone_control.x * _70;
         float _109 = c_tone_control.x * _71;
         float _110 = c_tone_control.x * _72;
         float _111 = _108 * 2.0f;
         float _112 = _109 * 2.0f;
         float _113 = _110 * 2.0f;
         float _114 = _108 * 0.30000001192092896f;
         float _115 = _109 * 0.30000001192092896f;
         float _116 = _110 * 0.30000001192092896f;
         float _117 = _114 + 0.05000000074505806f;
         float _118 = _115 + 0.05000000074505806f;
         float _119 = _116 + 0.05000000074505806f;
         float _120 = _117 * _111;
         float _121 = _118 * _112;
         float _122 = _119 * _113;
         float _123 = _120 + 0.004000000189989805f;
         float _124 = _121 + 0.004000000189989805f;
         float _125 = _122 + 0.004000000189989805f;
         float _126 = _114 + 0.5f;
         float _127 = _115 + 0.5f;
         float _128 = _116 + 0.5f;
         float _129 = _126 * _111;
         float _130 = _127 * _112;
         float _131 = _128 * _113;
         float _132 = _129 + 0.06000000238418579f;
         float _133 = _130 + 0.06000000238418579f;
         float _134 = _131 + 0.06000000238418579f;
         float _135 = _123 / _132;
         float _136 = _124 / _133;
         float _137 = _125 / _134;
         float _138 = _135 + -0.06666666269302368f;
         float _139 = _136 + -0.06666666269302368f;
         float _140 = _137 + -0.06666666269302368f;
         float _141 = _138 * 1.3790643215179443f;
         float _142 = _139 * 1.3790643215179443f;
         float _143 = _140 * 1.3790643215179443f;
         float _144 = YKR_MaterialCurve(_141, _108);
         float _145 = YKR_MaterialCurve(_142, _109);
         float _146 = YKR_MaterialCurve(_143, _110);
         float _147 = sqrt(_144);
         float _148 = sqrt(_145);
         float _149 = sqrt(_146);
         _151 = _147;
         _152 = _148;
         _153 = _149;
      }
      else
      {
         _151 = _70;
         _152 = _71;
         _153 = _72;
      }
   }
   int _154 = c_bool & 4;
   bool _155 = (_154 == 0);
   if (_155)
   {
      float _157 = saturate(COLOR_1.x);
      float _162 = (c_fog_param[0].x) - _151;
      float _163 = (c_fog_param[0].y) - _152;
      float _164 = (c_fog_param[0].z) - _153;
      float _165 = _162 * _157;
      float _166 = _163 * _157;
      float _167 = _164 * _157;
      _169 = _165;
      _170 = _166;
      _171 = _167;
   }
   else
   {
      _169 = COLOR_1.x;
      _170 = COLOR_1.y;
      _171 = COLOR_1.z;
   }
   float _172 = _169 + _151;
   float _173 = _170 + _152;
   float _174 = _171 + _153;
   float _180 = (c_ps_col_scl[0].x) * _172;
   float _181 = (c_ps_col_scl[0].y) * _173;
   float _182 = (c_ps_col_scl[0].z) * _174;
   float _183 = (c_ps_col_scl[0].w) * _81;
   SV_Target.x = _180;
   SV_Target.y = _181;
   SV_Target.z = _182;
   SV_Target.w = _183;
   int _186 = c_bool & 8;
   bool _187 = (_186 != 0);
   float _188 = select(_187, _172, _180);
   float _189 = select(_187, _173, _181);
   float _190 = select(_187, _174, _182);
   float _191 = select(_187, _81, _183);
   SV_Target.x = _188;
   SV_Target.y = _189;
   SV_Target.z = _190;
   SV_Target.w = _191;
   return SV_Target;
}
