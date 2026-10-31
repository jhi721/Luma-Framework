// Yakuza Kiwami material s_o1dt[rd][ag] (0x70ABBD35, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
// applied by a script: the inline Hable tonemap's saturate goes through
// YKR_MaterialCurve (tangent continuation in HDR), and the light pre-pass buffer t14 through YKR_LightBuffer.

#include "Includes/Common.hlsl"

Texture2D<float4> s0_t : register(t0);

Texture2D<float4> s5_t : register(t5);

Texture2D<float4> s7_t : register(t7);

Texture2D<float4> s13_t : register(t13);

Texture2D<float4> s14_t : register(t14);

cbuffer cb12 : register(b12)
{
   float4 c_matP[4] : packoffset(c000.x);
   float4 c_matV[4] : packoffset(c004.x);
   float4 c_matVP[4] : packoffset(c008.x);
   float4 c_matT[4][4] : packoffset(c012.x);
   float4 c_rvtx_param : packoffset(c028.x);
   float4 c_rt_size : packoffset(c029.x);
   float3 c_location : packoffset(c030.x);
};

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

cbuffer cb2 : register(b2)
{
   float4 c_ps_scratch[92] : packoffset(c000.x);
};

cbuffer cb3 : register(b3)
{
   float4 c_light_col[6] : packoffset(c000.x);
   float4 c_light_vec[6] : packoffset(c006.x);
};

SamplerState s0_s : register(s0);

SamplerState s5_s : register(s5);

SamplerState s7_s : register(s7);

SamplerState s13_s : register(s13);

SamplerState s14_s : register(s14);

float4 main(
    linear float4 COLOR : COLOR,
    linear float4 COLOR_1 : COLOR1,
    linear float4 TEXCOORD : TEXCOORD,
    linear float4 TEXCOORD_1 : TEXCOORD1,
    linear float4 TEXCOORD_2 : TEXCOORD2,
    linear float4 TEXCOORD_3 : TEXCOORD3,
    linear float4 TEXCOORD_4 : TEXCOORD4,
    linear float4 TEXCOORD_5 : TEXCOORD5,
    noperspective float4 SV_Position : SV_Position) : SV_Target
{
   float4 SV_Target = 0;
   float4 _54 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _59 = _54.x * _54.x;
   float _60 = _54.y * _54.y;
   float _61 = _54.z * _54.z;
   float4 _62 = s5_t.Sample(s5_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _67 = _62.x * 2.0f;
   float _68 = _62.y * 2.0f;
   float _69 = _67 + -1.0f;
   float _70 = _68 + -1.0f;
   float _71 = dot(float2(_69, _70), float2(_69, _70));
   float _72 = 1.0f - _71;
   float _73 = saturate(_72);
   float _74 = sqrt(_73);
   float _75 = TEXCOORD_2.y * TEXCOORD_3.z;
   float _76 = TEXCOORD_2.z * TEXCOORD_3.y;
   float _77 = _75 - _76;
   float _78 = TEXCOORD_2.z * TEXCOORD_3.x;
   float _79 = TEXCOORD_2.x * TEXCOORD_3.z;
   float _80 = _78 - _79;
   float _81 = TEXCOORD_2.x * TEXCOORD_3.y;
   float _82 = TEXCOORD_2.y * TEXCOORD_3.x;
   float _83 = _81 - _82;
   float _84 = _69 * TEXCOORD_3.x;
   float _85 = _69 * TEXCOORD_3.y;
   float _86 = _69 * TEXCOORD_3.z;
   float _87 = _70 * TEXCOORD_3.w;
   float _88 = _87 * _77;
   float _89 = _87 * _80;
   float _90 = _87 * _83;
   float _91 = _88 + _84;
   float _92 = _89 + _85;
   float _93 = _90 + _86;
   float _94 = _74 * TEXCOORD_2.x;
   float _95 = _74 * TEXCOORD_2.y;
   float _96 = _74 * TEXCOORD_2.z;
   float _97 = _91 + _94;
   float _98 = _92 + _95;
   float _99 = _93 + _96;
   float _100 = dot(float3(_97, _98, _99), float3(_97, _98, _99));
   float _101 = rsqrt(_100);
   float _102 = _97 * _101;
   float _103 = _98 * _101;
   float _104 = _99 * _101;
   float _109 = dot(float3(TEXCOORD_1.x, TEXCOORD_1.y, TEXCOORD_1.z), float3(TEXCOORD_1.x, TEXCOORD_1.y, TEXCOORD_1.z));
   float _110 = rsqrt(_109);
   float _111 = _110 * TEXCOORD_1.x;
   float _112 = _110 * TEXCOORD_1.y;
   float _113 = _110 * TEXCOORD_1.z;
   float4 _114 = s7_t.Sample(s7_s, float2(TEXCOORD.z, TEXCOORD.w));
   float _119 = _114.x * _114.x;
   float _120 = _114.y * _114.y;
   float _121 = _114.z * _114.z;
   float _122 = 1.0f - _119;
   float _123 = 1.0f - _120;
   float _124 = 1.0f - _121;
   float _125 = 1.0f - _114.w;
   float _126 = _122 * _125;
   float _127 = _123 * _125;
   float _128 = _124 * _125;
   float _129 = _126 + _119;
   float _130 = _127 + _120;
   float _131 = _128 + _121;
   float _132 = _59 * _129;
   float _133 = _60 * _130;
   float _134 = _61 * _131;
   float _138 = c_rt_size.z * SV_Position.x;
   float _139 = c_rt_size.w * SV_Position.y;
   int _142 = c_bool & 8;
   bool _143 = (_142 == 0);
   float _149;
   float _150;
   float _162;
   float _163;
   float _258;
   float _259;
   float _260;
   float _261;
   float _262;
   float _263;
   float _296;
   float _297;
   float _298;
   float _299;
   float _566;
   float _567;
   float _568;
   float _644;
   float _645;
   float _646;
   float _662;
   float _663;
   float _664;
   float _689;
   float _690;
   float _691;
   if (!_143)
   {
      float4 _145 = s13_t.Sample(s13_s, float2(_138, _139));
      _149 = _145.x;
      _150 = _145.y;
   }
   else
   {
      _149 = 1.0f;
      _150 = 1.0f;
   }
   if (!_143)
   {
      float _155 = (c_sdw_param_ch[0].x) * _149;
      float _156 = (c_sdw_param_ch[0].z) * _150;
      float _159 = _155 + (c_sdw_param_ch[0].y);
      float _160 = _156 + (c_sdw_param_ch[0].w);
      _162 = _159;
      _163 = _160;
   }
   else
   {
      _162 = _149;
      _163 = _150;
   }
   float _164 = min(_162, _163);
   int _168 = c_bool & 1;
   bool _169 = (_168 == 0);
   if (!_169)
   {
      float _171 = -0.0f - TEXCOORD_4.x;
      float _172 = -0.0f - TEXCOORD_4.y;
      float _173 = -0.0f - TEXCOORD_4.z;
      float _174 = dot(float3(_102, _103, _104), float3(_171, _172, _173));
      float _175 = saturate(_174);
      float _176 = _175 * TEXCOORD_1.w;
      float _177 = _175 * TEXCOORD_2.w;
      float _178 = _175 * TEXCOORD_4.w;
      float _179 = max(0.0f, _176);
      float _180 = max(0.0f, _177);
      float _181 = max(0.0f, _178);
      float _182 = -0.0f - TEXCOORD_5.x;
      float _183 = -0.0f - TEXCOORD_5.y;
      float _184 = -0.0f - TEXCOORD_5.z;
      float _185 = dot(float3(_102, _103, _104), float3(_182, _183, _184));
      float _186 = saturate(_185);
      float _187 = _186 * COLOR.w;
      float _188 = _186 * COLOR_1.w;
      float _189 = _186 * TEXCOORD_5.w;
      float _190 = max(0.0f, _187);
      float _191 = max(0.0f, _188);
      float _192 = max(0.0f, _189);
      float _193 = _190 + _179;
      float _194 = _191 + _180;
      float _195 = _192 + _181;
      float _196 = _111 + TEXCOORD_4.x;
      float _197 = _112 + TEXCOORD_4.y;
      float _198 = _113 + TEXCOORD_4.z;
      float _199 = dot(float3(_196, _197, _198), float3(_196, _197, _198));
      float _200 = rsqrt(_199);
      float _201 = _111 + TEXCOORD_5.x;
      float _202 = _112 + TEXCOORD_5.y;
      float _203 = _113 + TEXCOORD_5.z;
      float _204 = dot(float3(_201, _202, _203), float3(_201, _202, _203));
      float _205 = rsqrt(_204);
      float _206 = _201 * _205;
      float _207 = -0.0f - _206;
      float _208 = _202 * _205;
      float _209 = -0.0f - _208;
      float _210 = _203 * _205;
      float _211 = -0.0f - _210;
      float _212 = _196 * _200;
      float _213 = -0.0f - _212;
      float _214 = _197 * _200;
      float _215 = -0.0f - _214;
      float _216 = _198 * _200;
      float _217 = -0.0f - _216;
      float _218 = dot(float3(_213, _215, _217), float3(_102, _103, _104));
      float _219 = dot(float3(_207, _209, _211), float3(_102, _103, _104));
      float _220 = saturate(_218);
      float _221 = saturate(_219);
      float _222 = log2(_220);
      float _223 = log2(_221);
      float _224 = _222 * c_mat_power;
      float _225 = _223 * c_mat_power;
      float _226 = exp2(_224);
      float _227 = exp2(_225);
      float _228 = _226 * TEXCOORD_1.w;
      float _229 = _226 * TEXCOORD_2.w;
      float _230 = _226 * TEXCOORD_4.w;
      float _231 = _227 * COLOR.w;
      float _232 = _227 * COLOR_1.w;
      float _233 = _227 * TEXCOORD_5.w;
      float _234 = _231 + _228;
      float _235 = _232 + _229;
      float _236 = _233 + _230;
      float _237 = max(0.0f, _234);
      float _238 = max(0.0f, _235);
      float _239 = max(0.0f, _236);
      float _248 = c_mat_specular.x * _237;
      float _249 = _248 * c_specular_scale.x;
      float _250 = c_mat_specular.y * _238;
      float _251 = _250 * c_specular_scale.y;
      float _252 = c_mat_specular.z * _239;
      float _253 = _252 * c_specular_scale.z;
      float _254 = min(_249, 1.0f);
      float _255 = min(_251, 1.0f);
      float _256 = min(_253, 1.0f);
      _258 = _254;
      _259 = _255;
      _260 = _256;
      _261 = _193;
      _262 = _194;
      _263 = _195;
   }
   else
   {
      _258 = 0.0f;
      _259 = 0.0f;
      _260 = 0.0f;
      _261 = 0.0f;
      _262 = 0.0f;
      _263 = 0.0f;
   }
   float _264 = c_rt_size.z * SV_Position.x;
   float _265 = c_rt_size.w * SV_Position.y;
   float4 _266 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_264, _265)));
   int _273 = c_bool & 16384;
   bool _274 = (_273 == 0);
   if (!_274)
   {
      float _276 = _266.x * 4.0f;
      float _277 = _266.y * 4.0f;
      float _278 = _266.z * 4.0f;
      float _279 = _266.w * 4.0f;
      _296 = _276;
      _297 = _277;
      _298 = _278;
      _299 = _279;
   }
   else
   {
      int _281 = c_bool & 8192;
      bool _282 = (_281 == 0);
      if (!_282)
      {
         float _284 = _266.x * 2.0f;
         float _285 = _266.y * 2.0f;
         float _286 = _266.z * 2.0f;
         float _287 = _266.w * 2.0f;
         _296 = _284;
         _297 = _285;
         _298 = _286;
         _299 = _287;
      }
      else
      {
         float _291 = _266.x / c_tone_control.z;
         float _292 = _266.y / c_tone_control.z;
         float _293 = _266.z / c_tone_control.z;
         float _294 = _266.w / c_tone_control.z;
         _296 = _291;
         _297 = _292;
         _298 = _293;
         _299 = _294;
      }
   }
   float _300 = _296 + _261;
   float _301 = _297 + _262;
   float _302 = _298 + _263;
   float _365 = dot(float3(_102, _103, _104), float3(_102, _103, _104));
   float _366 = rsqrt(_365);
   float _367 = _366 * _102;
   float _368 = _366 * _103;
   float _369 = _366 * _104;
   float _370 = dot(float4((c_ps_scratch[80].x), (c_ps_scratch[80].y), (c_ps_scratch[80].z), (c_ps_scratch[80].w)), float4(_367, _368, _369, 1.0f));
   float _371 = dot(float4((c_ps_scratch[81].x), (c_ps_scratch[81].y), (c_ps_scratch[81].z), (c_ps_scratch[81].w)), float4(_367, _368, _369, 1.0f));
   float _372 = dot(float4((c_ps_scratch[82].x), (c_ps_scratch[82].y), (c_ps_scratch[82].z), (c_ps_scratch[82].w)), float4(_367, _368, _369, 1.0f));
   float _373 = dot(float4((c_ps_scratch[83].x), (c_ps_scratch[83].y), (c_ps_scratch[83].z), (c_ps_scratch[83].w)), float4(_367, _368, _369, 1.0f));
   float _374 = dot(float4(_367, _368, _369, 1.0f), float4(_370, _371, _372, _373));
   float _375 = dot(float4((c_ps_scratch[84].x), (c_ps_scratch[84].y), (c_ps_scratch[84].z), (c_ps_scratch[84].w)), float4(_367, _368, _369, 1.0f));
   float _376 = dot(float4((c_ps_scratch[85].x), (c_ps_scratch[85].y), (c_ps_scratch[85].z), (c_ps_scratch[85].w)), float4(_367, _368, _369, 1.0f));
   float _377 = dot(float4((c_ps_scratch[86].x), (c_ps_scratch[86].y), (c_ps_scratch[86].z), (c_ps_scratch[86].w)), float4(_367, _368, _369, 1.0f));
   float _378 = dot(float4((c_ps_scratch[87].x), (c_ps_scratch[87].y), (c_ps_scratch[87].z), (c_ps_scratch[87].w)), float4(_367, _368, _369, 1.0f));
   float _379 = dot(float4(_367, _368, _369, 1.0f), float4(_375, _376, _377, _378));
   float _380 = dot(float4((c_ps_scratch[88].x), (c_ps_scratch[88].y), (c_ps_scratch[88].z), (c_ps_scratch[88].w)), float4(_367, _368, _369, 1.0f));
   float _381 = dot(float4((c_ps_scratch[89].x), (c_ps_scratch[89].y), (c_ps_scratch[89].z), (c_ps_scratch[89].w)), float4(_367, _368, _369, 1.0f));
   float _382 = dot(float4((c_ps_scratch[90].x), (c_ps_scratch[90].y), (c_ps_scratch[90].z), (c_ps_scratch[90].w)), float4(_367, _368, _369, 1.0f));
   float _383 = dot(float4((c_ps_scratch[91].x), (c_ps_scratch[91].y), (c_ps_scratch[91].z), (c_ps_scratch[91].w)), float4(_367, _368, _369, 1.0f));
   float _384 = dot(float4(_367, _368, _369, 1.0f), float4(_380, _381, _382, _383));
   float _385 = max(_374, 0.0625f);
   float _386 = max(_379, 0.0625f);
   float _387 = max(_384, 0.0625f);
   float _388 = _385 * (c_ps_scratch[79].x);
   float _389 = _386 * (c_ps_scratch[79].x);
   float _390 = _387 * (c_ps_scratch[79].x);
   float _395 = -0.0f - (c_light_vec[0].x);
   float _396 = -0.0f - (c_light_vec[0].y);
   float _397 = -0.0f - (c_light_vec[0].z);
   float _398 = dot(float3(_102, _103, _104), float3(_395, _396, _397));
   float _399 = -0.0f - (c_light_vec[1].x);
   float _400 = -0.0f - (c_light_vec[1].y);
   float _401 = -0.0f - (c_light_vec[1].z);
   float _402 = dot(float3(_102, _103, _104), float3(_399, _400, _401));
   float _411 = _103 * 0.5f;
   float _412 = _411 + 0.5f;
   float _413 = (c_light_col[4].x) - (c_light_vec[4].x);
   float _414 = (c_light_col[4].y) - (c_light_vec[4].y);
   float _415 = (c_light_col[4].z) - (c_light_vec[4].z);
   float _416 = _413 * _412;
   float _417 = _414 * _412;
   float _418 = _415 * _412;
   float _419 = saturate(_398);
   float _420 = saturate(_402);
   float _425 = (c_light_col[0].x) * _419;
   float _426 = (c_light_col[0].y) * _419;
   float _427 = (c_light_col[0].z) * _419;
   float _432 = (c_light_col[1].x) * _420;
   float _433 = (c_light_col[1].y) * _420;
   float _434 = (c_light_col[1].z) * _420;
   int _435 = c_bool & 32768;
   bool _436 = (_435 == 0);
   float _445 = (c_light_vec[4].x) + _388;
   float _446 = _445 + _416;
   float _447 = (c_light_vec[4].y) + _389;
   float _448 = _447 + _417;
   float _449 = (c_light_vec[4].z) + _390;
   float _450 = _449 + _418;
   if (!_436)
   {
      float _454 = (c_ps_user[11].x) * _132;
      float _455 = (c_ps_user[11].x) * _133;
      float _456 = (c_ps_user[11].x) * _134;
      float _458 = _102 + 1.0f;
      float _459 = _103 + 1.0f;
      float _460 = _104 + 1.0f;
      float _461 = _458 * 0.5f;
      float _462 = _459 * 0.5f;
      float _463 = _460 * 0.5f;
      float _464 = saturate(_461);
      float _465 = saturate(_462);
      float _466 = saturate(_463);
      float _467 = (c_ps_user[11].y) * _464;
      float _468 = _465 * (c_ps_user[11].y);
      float _469 = _466 * (c_ps_user[11].y);
      float _472 = (c_ps_user[12].x) * _164;
      float _477 = -0.0f - _111;
      float _478 = -0.0f - _112;
      float _479 = -0.0f - _113;
      float _480 = dot(float3(_477, _478, _479), float3(_102, _103, _104));
      float _481 = 1.0f - _480;
      float _482 = _481 * _481;
      float _483 = _481 * 0.9722222089767456f;
      float _484 = _482 * _482;
      float _485 = _484 * _483;
      float _486 = _485 + 0.02777777425944805f;
      float _487 = _486 * (c_ps_user[13].z);
      float _489 = (c_ps_user[13].w) * _446;
      float _490 = (c_ps_user[13].w) * _448;
      float _491 = (c_ps_user[13].w) * _450;
      float _494 = (c_ps_user[14].y) * _425;
      float _495 = (c_ps_user[14].y) * _426;
      float _496 = (c_ps_user[14].y) * _427;
      float _498 = (c_ps_user[14].z) * _432;
      float _499 = (c_ps_user[14].z) * _433;
      float _500 = (c_ps_user[14].z) * _434;
      float _502 = (c_ps_user[14].w) * _300;
      float _503 = (c_ps_user[14].w) * _301;
      float _504 = (c_ps_user[14].w) * _302;
      float _505 = _472 + (c_ps_user[12].y);
      float _506 = _494 + (c_ps_user[13].x);
      float _507 = _506 + _498;
      float _508 = c_diffuse_scale.x * _507;
      float _509 = _508 * c_mat_diffuse.x;
      float _510 = _487 + _505;
      float _511 = _510 + _454;
      float _512 = _511 + _467;
      float _513 = _512 + _489;
      float _514 = _513 + _502;
      float _515 = _514 + _509;
      float _516 = _495 + (c_ps_user[13].x);
      float _517 = _516 + _499;
      float _518 = c_diffuse_scale.y * _517;
      float _519 = _518 * c_mat_diffuse.y;
      float _520 = _510 + _455;
      float _521 = _520 + _468;
      float _522 = _521 + _490;
      float _523 = _522 + _503;
      float _524 = _523 + _519;
      float _525 = _496 + (c_ps_user[13].x);
      float _526 = _525 + _500;
      float _527 = c_diffuse_scale.z * _526;
      float _528 = _527 * c_mat_diffuse.z;
      float _529 = _510 + _456;
      float _530 = _529 + _469;
      float _531 = _530 + _491;
      float _532 = _531 + _504;
      float _533 = _532 + _528;
      float _534 = saturate(_515);
      float _535 = saturate(_524);
      float _536 = saturate(_533);
      _566 = _534;
      _567 = _535;
      _568 = _536;
   }
   else
   {
      float _538 = _427 * _164;
      float _539 = _450 + _302;
      float _540 = _539 + _434;
      float _541 = _540 + _538;
      float _542 = _426 * _164;
      float _543 = _448 + _301;
      float _544 = _543 + _433;
      float _545 = _544 + _542;
      float _546 = _425 * _164;
      float _547 = _446 + _300;
      float _548 = _547 + _432;
      float _549 = _548 + _546;
      float _550 = _299 + _260;
      float _551 = _299 + _259;
      float _552 = _299 + _258;
      float _553 = c_diffuse_scale.x * c_mat_diffuse.x;
      float _554 = _553 * _132;
      float _555 = _554 * _549;
      float _556 = c_diffuse_scale.y * c_mat_diffuse.y;
      float _557 = _556 * _133;
      float _558 = _557 * _545;
      float _559 = c_diffuse_scale.z * c_mat_diffuse.z;
      float _560 = _559 * _134;
      float _561 = _560 * _541;
      float _562 = _552 + _555;
      float _563 = _551 + _558;
      float _564 = _550 + _561;
      _566 = _562;
      _567 = _563;
      _568 = _564;
   }
   float _573 = c_mat_opacity * _54.w;
   float _574 = _573 * c_opacity_scale;
   float _575 = _574 * c_global_opacity;
   float _576 = _566 * COLOR.x;
   float _577 = _567 * COLOR.y;
   float _578 = _568 * COLOR.z;
   int _579 = c_bool & 16;
   bool _580 = (_579 == 0);
   if (!_580)
   {
      float _584 = c_tone_control.x * _576;
      float _585 = c_tone_control.x * _577;
      float _586 = c_tone_control.x * _578;
      float _588 = log2(_584);
      float _589 = log2(_585);
      float _590 = log2(_586);
      float _591 = _588 * c_tone_control.y;
      float _592 = _589 * c_tone_control.y;
      float _593 = _590 * c_tone_control.y;
      float _594 = exp2(_591);
      float _595 = exp2(_592);
      float _596 = exp2(_593);
      _644 = _594;
      _645 = _595;
      _646 = _596;
   }
   else
   {
      if (_436)
      {
         float _601 = c_tone_control.x * _576;
         float _602 = c_tone_control.x * _577;
         float _603 = c_tone_control.x * _578;
         float _604 = _601 * 2.0f;
         float _605 = _602 * 2.0f;
         float _606 = _603 * 2.0f;
         float _607 = _601 * 0.30000001192092896f;
         float _608 = _602 * 0.30000001192092896f;
         float _609 = _603 * 0.30000001192092896f;
         float _610 = _607 + 0.05000000074505806f;
         float _611 = _608 + 0.05000000074505806f;
         float _612 = _609 + 0.05000000074505806f;
         float _613 = _610 * _604;
         float _614 = _611 * _605;
         float _615 = _612 * _606;
         float _616 = _613 + 0.004000000189989805f;
         float _617 = _614 + 0.004000000189989805f;
         float _618 = _615 + 0.004000000189989805f;
         float _619 = _607 + 0.5f;
         float _620 = _608 + 0.5f;
         float _621 = _609 + 0.5f;
         float _622 = _619 * _604;
         float _623 = _620 * _605;
         float _624 = _621 * _606;
         float _625 = _622 + 0.06000000238418579f;
         float _626 = _623 + 0.06000000238418579f;
         float _627 = _624 + 0.06000000238418579f;
         float _628 = _616 / _625;
         float _629 = _617 / _626;
         float _630 = _618 / _627;
         float _631 = _628 + -0.06666666269302368f;
         float _632 = _629 + -0.06666666269302368f;
         float _633 = _630 + -0.06666666269302368f;
         float _634 = _631 * 1.3790643215179443f;
         float _635 = _632 * 1.3790643215179443f;
         float _636 = _633 * 1.3790643215179443f;
         float _637 = YKR_MaterialCurve(_634, _601);
         float _638 = YKR_MaterialCurve(_635, _602);
         float _639 = YKR_MaterialCurve(_636, _603);
         float _640 = sqrt(_637);
         float _641 = sqrt(_638);
         float _642 = sqrt(_639);
         _644 = _640;
         _645 = _641;
         _646 = _642;
      }
      else
      {
         _644 = _576;
         _645 = _577;
         _646 = _578;
      }
   }
   int _647 = c_bool & 4;
   bool _648 = (_647 == 0);
   if (_648)
   {
      float _650 = saturate(COLOR_1.x);
      float _655 = (c_fog_param[0].x) - _644;
      float _656 = (c_fog_param[0].y) - _645;
      float _657 = (c_fog_param[0].z) - _646;
      float _658 = _655 * _650;
      float _659 = _656 * _650;
      float _660 = _657 * _650;
      _662 = _658;
      _663 = _659;
      _664 = _660;
   }
   else
   {
      _662 = COLOR_1.x;
      _663 = COLOR_1.y;
      _664 = COLOR_1.z;
   }
   float _665 = _662 + _644;
   float _666 = _663 + _645;
   float _667 = _664 + _646;
   if (!_436)
   {
      float _675 = (c_ps_user[3].x) * (c_ps_user[12].z);
      float _676 = (c_ps_user[3].y) * (c_ps_user[12].z);
      float _677 = (c_ps_user[3].z) * (c_ps_user[12].z);
      float _679 = (c_ps_user[12].w) * COLOR.x;
      float _680 = (c_ps_user[12].w) * COLOR.y;
      float _681 = (c_ps_user[12].w) * COLOR.z;
      float _682 = _679 + _665;
      float _683 = _682 + _675;
      float _684 = _680 + _666;
      float _685 = _684 + _676;
      float _686 = _681 + _667;
      float _687 = _686 + _677;
      _689 = _683;
      _690 = _685;
      _691 = _687;
   }
   else
   {
      _689 = _665;
      _690 = _666;
      _691 = _667;
   }
   float _697 = (c_ps_col_scl[0].x) * _689;
   float _698 = (c_ps_col_scl[0].y) * _690;
   float _699 = (c_ps_col_scl[0].z) * _691;
   float _700 = (c_ps_col_scl[0].w) * _575;
   SV_Target.x = _697;
   SV_Target.y = _698;
   SV_Target.z = _699;
   SV_Target.w = _700;
   int _703 = c_bool & 8;
   bool _704 = (_703 != 0);
   float _705 = select(_704, _689, _697);
   float _706 = select(_704, _690, _698);
   float _707 = select(_704, _691, _699);
   float _708 = select(_704, _575, _700);
   SV_Target.x = _705;
   SV_Target.y = _706;
   SV_Target.z = _707;
   SV_Target.w = _708;
   return SV_Target;
}
