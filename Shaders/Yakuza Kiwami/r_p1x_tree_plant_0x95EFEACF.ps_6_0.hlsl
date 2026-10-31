// Yakuza Kiwami material r_p1x[tree][plant] (0x95EFEACF, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
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
   float4 _16 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _21 = _16.x * _16.x;
   float _22 = _21 * COLOR.x;
   float _23 = _16.y * _16.y;
   float _24 = _23 * COLOR.y;
   float _25 = _16.z * _16.z;
   float _26 = _25 * COLOR.z;
   float _33 = c_mat_opacity * _16.w;
   float _34 = _33 * c_opacity_scale;
   float _35 = _34 * c_global_opacity;
   int _38 = c_bool & 16;
   bool _39 = (_38 == 0);
   float _105;
   float _106;
   float _107;
   float _123;
   float _124;
   float _125;
   if (!_39)
   {
      float _43 = c_tone_control.x * _22;
      float _44 = c_tone_control.x * _24;
      float _45 = c_tone_control.x * _26;
      float _47 = log2(_43);
      float _48 = log2(_44);
      float _49 = log2(_45);
      float _50 = _47 * c_tone_control.y;
      float _51 = _48 * c_tone_control.y;
      float _52 = _49 * c_tone_control.y;
      float _53 = exp2(_50);
      float _54 = exp2(_51);
      float _55 = exp2(_52);
      _105 = _53;
      _106 = _54;
      _107 = _55;
   }
   else
   {
      int _57 = c_bool & 32768;
      bool _58 = (_57 == 0);
      if (_58)
      {
         float _62 = c_tone_control.x * _22;
         float _63 = c_tone_control.x * _24;
         float _64 = c_tone_control.x * _26;
         float _65 = _62 * 2.0f;
         float _66 = _63 * 2.0f;
         float _67 = _64 * 2.0f;
         float _68 = _62 * 0.30000001192092896f;
         float _69 = _63 * 0.30000001192092896f;
         float _70 = _64 * 0.30000001192092896f;
         float _71 = _68 + 0.05000000074505806f;
         float _72 = _69 + 0.05000000074505806f;
         float _73 = _70 + 0.05000000074505806f;
         float _74 = _71 * _65;
         float _75 = _72 * _66;
         float _76 = _73 * _67;
         float _77 = _74 + 0.004000000189989805f;
         float _78 = _75 + 0.004000000189989805f;
         float _79 = _76 + 0.004000000189989805f;
         float _80 = _68 + 0.5f;
         float _81 = _69 + 0.5f;
         float _82 = _70 + 0.5f;
         float _83 = _80 * _65;
         float _84 = _81 * _66;
         float _85 = _82 * _67;
         float _86 = _83 + 0.06000000238418579f;
         float _87 = _84 + 0.06000000238418579f;
         float _88 = _85 + 0.06000000238418579f;
         float _89 = _77 / _86;
         float _90 = _78 / _87;
         float _91 = _79 / _88;
         float _92 = _89 + -0.06666666269302368f;
         float _93 = _90 + -0.06666666269302368f;
         float _94 = _91 + -0.06666666269302368f;
         float _95 = _92 * 1.3790643215179443f;
         float _96 = _93 * 1.3790643215179443f;
         float _97 = _94 * 1.3790643215179443f;
         float _98 = YKR_MaterialCurve(_95, _62);
         float _99 = YKR_MaterialCurve(_96, _63);
         float _100 = YKR_MaterialCurve(_97, _64);
         float _101 = sqrt(_98);
         float _102 = sqrt(_99);
         float _103 = sqrt(_100);
         _105 = _101;
         _106 = _102;
         _107 = _103;
      }
      else
      {
         _105 = _22;
         _106 = _24;
         _107 = _26;
      }
   }
   int _108 = c_bool & 4;
   bool _109 = (_108 == 0);
   if (_109)
   {
      float _111 = saturate(COLOR_1.x);
      float _116 = (c_fog_param[0].x) - _105;
      float _117 = (c_fog_param[0].y) - _106;
      float _118 = (c_fog_param[0].z) - _107;
      float _119 = _116 * _111;
      float _120 = _117 * _111;
      float _121 = _118 * _111;
      _123 = _119;
      _124 = _120;
      _125 = _121;
   }
   else
   {
      _123 = COLOR_1.x;
      _124 = COLOR_1.y;
      _125 = COLOR_1.z;
   }
   float _126 = _123 + _105;
   float _127 = _124 + _106;
   float _128 = _125 + _107;
   float _131 = _16.w - c_mat_punchthrough_ref;
   bool _132 = (_131 < 0.0f);
   if (_132)
      discard;
   float _138 = (c_ps_col_scl[0].x) * _126;
   float _139 = (c_ps_col_scl[0].y) * _127;
   float _140 = (c_ps_col_scl[0].z) * _128;
   float _141 = (c_ps_col_scl[0].w) * _35;
   SV_Target.x = _138;
   SV_Target.y = _139;
   SV_Target.z = _140;
   SV_Target.w = _141;
   int _144 = c_bool & 8;
   bool _145 = (_144 != 0);
   float _146 = select(_145, _126, _138);
   float _147 = select(_145, _127, _139);
   float _148 = select(_145, _128, _140);
   float _149 = select(_145, _35, _141);
   SV_Target.x = _146;
   SV_Target.y = _147;
   SV_Target.z = _148;
   SV_Target.w = _149;
   return SV_Target;
}
