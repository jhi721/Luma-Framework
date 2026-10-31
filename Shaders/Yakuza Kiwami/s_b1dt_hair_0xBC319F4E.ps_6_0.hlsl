// Yakuza Kiwami material s_b1dt[hair] (0xBC319F4E, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
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
   float _114 = _111 + (c_light_vec[0].x);
   float _115 = _112 + (c_light_vec[0].y);
   float _116 = _113 + (c_light_vec[0].z);
   float _117 = dot(float3(_114, _115, _116), float3(_114, _115, _116));
   float _118 = rsqrt(_117);
   float _119 = _114 * _118;
   float _120 = _115 * _118;
   float _121 = _116 * _118;
   float _125 = c_rt_size.z * SV_Position.x;
   float _126 = c_rt_size.w * SV_Position.y;
   int _129 = c_bool & 8;
   bool _130 = (_129 == 0);
   float _136;
   float _137;
   float _149;
   float _150;
   float _315;
   float _316;
   float _317;
   float _346;
   float _347;
   float _348;
   float _654;
   float _655;
   float _656;
   float _732;
   float _733;
   float _734;
   float _750;
   float _751;
   float _752;
   float _777;
   float _778;
   float _779;
   if (!_130)
   {
      float4 _132 = s13_t.Sample(s13_s, float2(_125, _126));
      _136 = _132.x;
      _137 = _132.y;
   }
   else
   {
      _136 = 1.0f;
      _137 = 1.0f;
   }
   if (!_130)
   {
      float _142 = (c_sdw_param_ch[0].x) * _136;
      float _143 = (c_sdw_param_ch[0].z) * _137;
      float _146 = _142 + (c_sdw_param_ch[0].y);
      float _147 = _143 + (c_sdw_param_ch[0].w);
      _149 = _146;
      _150 = _147;
   }
   else
   {
      _149 = _136;
      _150 = _137;
   }
   float _151 = min(_149, _150);
   float _156 = (c_light_vec[1].x) + _111;
   float _157 = (c_light_vec[1].y) + _112;
   float _158 = (c_light_vec[1].z) + _113;
   float _159 = dot(float3(_156, _157, _158), float3(_156, _157, _158));
   float _160 = rsqrt(_159);
   float _161 = _156 * _160;
   float _162 = _157 * _160;
   float _163 = _158 * _160;
   float _164 = _103 * TEXCOORD_3.z;
   float _165 = _104 * TEXCOORD_3.y;
   float _166 = _164 - _165;
   float _167 = _104 * TEXCOORD_3.x;
   float _168 = _102 * TEXCOORD_3.z;
   float _169 = _167 - _168;
   float _170 = _102 * TEXCOORD_3.y;
   float _171 = _103 * TEXCOORD_3.x;
   float _172 = _170 - _171;
   float _173 = _166 * TEXCOORD_3.w;
   float _174 = _169 * TEXCOORD_3.w;
   float _175 = _172 * TEXCOORD_3.w;
   float _176 = _174 * _104;
   float _177 = _175 * _103;
   float _178 = _176 - _177;
   float _179 = _175 * _102;
   float _180 = _173 * _104;
   float _181 = _179 - _180;
   float _182 = _173 * _103;
   float _183 = _174 * _102;
   float _184 = _182 - _183;
   float _185 = -0.0f - _119;
   float _186 = -0.0f - _120;
   float _187 = -0.0f - _121;
   float _188 = dot(float3(_102, _103, _104), float3(_185, _186, _187));
   float _189 = dot(float3(_185, _186, _187), float3(_173, _174, _175));
   float _190 = TEXCOORD.z * 0.20000000298023224f;
   float _191 = _189 * _190;
   float _192 = _191 + 0.5f;
   float4 _193 = s7_t.Sample(s7_s, float2(_188, _192));
   float _195 = dot(float3(_185, _186, _187), float3(_178, _181, _184));
   float _196 = TEXCOORD.w * 0.20000000298023224f;
   float _197 = _195 * _196;
   float _198 = _197 + 0.5f;
   float4 _199 = s7_t.Sample(s7_s, float2(_188, _198));
   float _201 = _199.x * _193.x;
   float _202 = -0.0f - _161;
   float _203 = -0.0f - _162;
   float _204 = -0.0f - _163;
   float _205 = dot(float3(_102, _103, _104), float3(_202, _203, _204));
   float _206 = dot(float3(_202, _203, _204), float3(_173, _174, _175));
   float _207 = _206 * _190;
   float _208 = _207 + 0.5f;
   float4 _209 = s7_t.Sample(s7_s, float2(_205, _208));
   float _211 = dot(float3(_202, _203, _204), float3(_178, _181, _184));
   float _212 = _211 * _196;
   float _213 = _212 + 0.5f;
   float4 _214 = s7_t.Sample(s7_s, float2(_205, _213));
   float _216 = _214.x * _209.x;
   float _217 = log2(_201);
   float _218 = log2(_216);
   float _219 = _217 * c_mat_power;
   float _220 = _218 * c_mat_power;
   float _221 = exp2(_219);
   float _222 = exp2(_220);
   float _227 = c_mat_fresnel * c_tone_control.w;
   float _228 = c_mat_power * 0.03974359855055809f;
   float _229 = _228 + 0.0856831967830658f;
   float _230 = _222 * _229;
   float _231 = dot(float3(_111, _112, _113), float3(_119, _120, _121));
   float _232 = dot(float3(_111, _112, _113), float3(_161, _162, _163));
   float _233 = 1.0f - _231;
   float _234 = 1.0f - _232;
   float _235 = _233 * _233;
   float _236 = _234 * _234;
   float _237 = 1.0f - _227;
   float _238 = _235 * _235;
   float _239 = _238 * _233;
   float _240 = _239 * _237;
   float _241 = _234 * _237;
   float _242 = _236 * _236;
   float _243 = _242 * _241;
   float _244 = _240 + _227;
   float _245 = _243 + _227;
   float _246 = _230 * _245;
   float _255 = c_specular_scale.x * c_mat_specular.x;
   float _256 = c_specular_scale.y * c_mat_specular.y;
   float _257 = c_specular_scale.z * c_mat_specular.z;
   float _262 = _151 * _229;
   float _263 = _262 * _221;
   float _264 = _263 * _244;
   float _265 = _264 * (c_light_col[0].x);
   float _266 = _264 * (c_light_col[0].y);
   float _267 = _264 * (c_light_col[0].z);
   float _272 = _246 * (c_light_col[1].x);
   float _273 = _246 * (c_light_col[1].y);
   float _274 = _246 * (c_light_col[1].z);
   float _275 = _265 + _272;
   float _276 = _266 + _273;
   float _277 = _267 + _274;
   float _278 = _255 * _275;
   float _279 = _256 * _276;
   float _280 = _257 * _277;
   int _286 = c_bool & 1;
   bool _287 = (_286 == 0);
   if (!_287)
   {
      float _289 = -0.0f - TEXCOORD_4.x;
      float _290 = -0.0f - TEXCOORD_4.y;
      float _291 = -0.0f - TEXCOORD_4.z;
      float _292 = dot(float3(_102, _103, _104), float3(_289, _290, _291));
      float _293 = saturate(_292);
      float _294 = _293 * TEXCOORD_1.w;
      float _295 = _293 * TEXCOORD_2.w;
      float _296 = _293 * TEXCOORD_4.w;
      float _297 = max(0.0f, _294);
      float _298 = max(0.0f, _295);
      float _299 = max(0.0f, _296);
      float _300 = -0.0f - TEXCOORD_5.x;
      float _301 = -0.0f - TEXCOORD_5.y;
      float _302 = -0.0f - TEXCOORD_5.z;
      float _303 = dot(float3(_102, _103, _104), float3(_300, _301, _302));
      float _304 = saturate(_303);
      float _305 = _304 * COLOR.w;
      float _306 = _304 * COLOR_1.w;
      float _307 = _304 * TEXCOORD_5.w;
      float _308 = max(0.0f, _305);
      float _309 = max(0.0f, _306);
      float _310 = max(0.0f, _307);
      float _311 = _308 + _297;
      float _312 = _309 + _298;
      float _313 = _310 + _299;
      _315 = _311;
      _316 = _312;
      _317 = _313;
   }
   else
   {
      _315 = 0.0f;
      _316 = 0.0f;
      _317 = 0.0f;
   }
   float _318 = c_rt_size.z * SV_Position.x;
   float _319 = c_rt_size.w * SV_Position.y;
   float4 _320 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_318, _319)));
   int _326 = c_bool & 16384;
   bool _327 = (_326 == 0);
   if (!_327)
   {
      float _329 = _320.x * 4.0f;
      float _330 = _320.y * 4.0f;
      float _331 = _320.z * 4.0f;
      _346 = _329;
      _347 = _330;
      _348 = _331;
   }
   else
   {
      int _333 = c_bool & 8192;
      bool _334 = (_333 == 0);
      if (!_334)
      {
         float _336 = _320.x * 2.0f;
         float _337 = _320.y * 2.0f;
         float _338 = _320.z * 2.0f;
         _346 = _336;
         _347 = _337;
         _348 = _338;
      }
      else
      {
         float _342 = _320.x / c_tone_control.z;
         float _343 = _320.y / c_tone_control.z;
         float _344 = _320.z / c_tone_control.z;
         _346 = _342;
         _347 = _343;
         _348 = _344;
      }
   }
   float _349 = _346 + _315;
   float _350 = _347 + _316;
   float _351 = _348 + _317;
   float _414 = dot(float3(_102, _103, _104), float3(_102, _103, _104));
   float _415 = rsqrt(_414);
   float _416 = _415 * _102;
   float _417 = _415 * _103;
   float _418 = _415 * _104;
   float _419 = dot(float4((c_ps_scratch[80].x), (c_ps_scratch[80].y), (c_ps_scratch[80].z), (c_ps_scratch[80].w)), float4(_416, _417, _418, 1.0f));
   float _420 = dot(float4((c_ps_scratch[81].x), (c_ps_scratch[81].y), (c_ps_scratch[81].z), (c_ps_scratch[81].w)), float4(_416, _417, _418, 1.0f));
   float _421 = dot(float4((c_ps_scratch[82].x), (c_ps_scratch[82].y), (c_ps_scratch[82].z), (c_ps_scratch[82].w)), float4(_416, _417, _418, 1.0f));
   float _422 = dot(float4((c_ps_scratch[83].x), (c_ps_scratch[83].y), (c_ps_scratch[83].z), (c_ps_scratch[83].w)), float4(_416, _417, _418, 1.0f));
   float _423 = dot(float4(_416, _417, _418, 1.0f), float4(_419, _420, _421, _422));
   float _424 = dot(float4((c_ps_scratch[84].x), (c_ps_scratch[84].y), (c_ps_scratch[84].z), (c_ps_scratch[84].w)), float4(_416, _417, _418, 1.0f));
   float _425 = dot(float4((c_ps_scratch[85].x), (c_ps_scratch[85].y), (c_ps_scratch[85].z), (c_ps_scratch[85].w)), float4(_416, _417, _418, 1.0f));
   float _426 = dot(float4((c_ps_scratch[86].x), (c_ps_scratch[86].y), (c_ps_scratch[86].z), (c_ps_scratch[86].w)), float4(_416, _417, _418, 1.0f));
   float _427 = dot(float4((c_ps_scratch[87].x), (c_ps_scratch[87].y), (c_ps_scratch[87].z), (c_ps_scratch[87].w)), float4(_416, _417, _418, 1.0f));
   float _428 = dot(float4(_416, _417, _418, 1.0f), float4(_424, _425, _426, _427));
   float _429 = dot(float4((c_ps_scratch[88].x), (c_ps_scratch[88].y), (c_ps_scratch[88].z), (c_ps_scratch[88].w)), float4(_416, _417, _418, 1.0f));
   float _430 = dot(float4((c_ps_scratch[89].x), (c_ps_scratch[89].y), (c_ps_scratch[89].z), (c_ps_scratch[89].w)), float4(_416, _417, _418, 1.0f));
   float _431 = dot(float4((c_ps_scratch[90].x), (c_ps_scratch[90].y), (c_ps_scratch[90].z), (c_ps_scratch[90].w)), float4(_416, _417, _418, 1.0f));
   float _432 = dot(float4((c_ps_scratch[91].x), (c_ps_scratch[91].y), (c_ps_scratch[91].z), (c_ps_scratch[91].w)), float4(_416, _417, _418, 1.0f));
   float _433 = dot(float4(_416, _417, _418, 1.0f), float4(_429, _430, _431, _432));
   float _434 = max(_423, 0.0625f);
   float _435 = max(_428, 0.0625f);
   float _436 = max(_433, 0.0625f);
   float _437 = _434 * (c_ps_scratch[79].x);
   float _438 = _435 * (c_ps_scratch[79].x);
   float _439 = _436 * (c_ps_scratch[79].x);
   float _444 = -0.0f - (c_light_vec[0].x);
   float _445 = -0.0f - (c_light_vec[0].y);
   float _446 = -0.0f - (c_light_vec[0].z);
   float _447 = dot(float3(_102, _103, _104), float3(_444, _445, _446));
   float _448 = -0.0f - (c_light_vec[1].x);
   float _449 = -0.0f - (c_light_vec[1].y);
   float _450 = -0.0f - (c_light_vec[1].z);
   float _451 = dot(float3(_102, _103, _104), float3(_448, _449, _450));
   float _460 = _103 * 0.5f;
   float _461 = _460 + 0.5f;
   float _462 = (c_light_col[4].x) - (c_light_vec[4].x);
   float _463 = (c_light_col[4].y) - (c_light_vec[4].y);
   float _464 = (c_light_col[4].z) - (c_light_vec[4].z);
   float _465 = _462 * _461;
   float _466 = _463 * _461;
   float _467 = _464 * _461;
   float _468 = saturate(_447);
   float _469 = saturate(_451);
   float _474 = (c_light_col[0].x) * _468;
   float _475 = (c_light_col[0].y) * _468;
   float _476 = (c_light_col[0].z) * _468;
   float _481 = (c_light_col[1].x) * _469;
   float _482 = (c_light_col[1].y) * _469;
   float _483 = (c_light_col[1].z) * _469;
   int _484 = c_bool & 32768;
   bool _485 = (_484 == 0);
   float _494 = (c_light_vec[4].x) + _437;
   float _495 = _494 + _465;
   float _496 = (c_light_vec[4].y) + _438;
   float _497 = _496 + _466;
   float _498 = (c_light_vec[4].z) + _439;
   float _499 = _498 + _467;
   if (!_485)
   {
      float _503 = (c_ps_user[11].x) * _59;
      float _504 = (c_ps_user[11].x) * _60;
      float _505 = (c_ps_user[11].x) * _61;
      float _507 = _102 + 1.0f;
      float _508 = _103 + 1.0f;
      float _509 = _104 + 1.0f;
      float _510 = _507 * 0.5f;
      float _511 = _508 * 0.5f;
      float _512 = _509 * 0.5f;
      float _513 = saturate(_510);
      float _514 = saturate(_511);
      float _515 = saturate(_512);
      float _516 = (c_ps_user[11].y) * _513;
      float _517 = _514 * (c_ps_user[11].y);
      float _518 = _515 * (c_ps_user[11].y);
      float _529 = (c_ps_user[11].w) * _278;
      float _532 = (c_ps_user[12].x) * _151;
      float _538 = -0.0f - _111;
      float _539 = -0.0f - _112;
      float _540 = -0.0f - _113;
      float _541 = dot(float3(_538, _539, _540), float3(_102, _103, _104));
      float _542 = 1.0f - _541;
      float _543 = _542 * _542;
      float _544 = _542 * 0.9722222089767456f;
      float _545 = _543 * _543;
      float _546 = _545 * _544;
      float _547 = _546 + 0.02777777425944805f;
      float _548 = _547 * (c_ps_user[13].z);
      float _550 = (c_ps_user[13].w) * _495;
      float _551 = (c_ps_user[13].w) * _497;
      float _552 = (c_ps_user[13].w) * _499;
      float _555 = (c_ps_user[14].y) * _474;
      float _556 = (c_ps_user[14].y) * _475;
      float _557 = (c_ps_user[14].y) * _476;
      float _559 = (c_ps_user[14].z) * _481;
      float _560 = (c_ps_user[14].z) * _482;
      float _561 = (c_ps_user[14].z) * _483;
      float _563 = (c_ps_user[14].w) * _349;
      float _564 = (c_ps_user[14].w) * _350;
      float _565 = (c_ps_user[14].w) * _351;
      float _569 = (c_ps_user[15].y) + (c_ps_user[15].x);
      float _570 = c_specular_scale.x * _569;
      float _571 = _570 * _278;
      float _572 = _555 + (c_ps_user[13].x);
      float _573 = _572 + _559;
      float _574 = c_diffuse_scale.x * _573;
      float _575 = _574 * c_mat_diffuse.x;
      float _576 = (c_ps_user[13].y) + (c_ps_user[11].z);
      float _577 = _576 * c_specular_scale.x;
      float _578 = _577 + _571;
      float _579 = _578 * c_mat_specular.x;
      float _580 = _516 + _503;
      float _581 = _580 + _529;
      float _582 = _581 + (c_ps_user[12].y);
      float _583 = _582 + _532;
      float _584 = _583 + _550;
      float _585 = _584 + _563;
      float _586 = _585 + _548;
      float _587 = _586 + _575;
      float _588 = _587 + _579;
      float _589 = c_specular_scale.y * _569;
      float _590 = _589 * _279;
      float _591 = _556 + (c_ps_user[13].x);
      float _592 = _591 + _560;
      float _593 = c_diffuse_scale.y * _592;
      float _594 = _593 * c_mat_diffuse.y;
      float _595 = _576 * c_specular_scale.y;
      float _596 = _595 + _590;
      float _597 = _596 * c_mat_specular.y;
      float _598 = _529 + _504;
      float _599 = _598 + _517;
      float _600 = _599 + (c_ps_user[12].y);
      float _601 = _600 + _532;
      float _602 = _601 + _551;
      float _603 = _602 + _564;
      float _604 = _603 + _548;
      float _605 = _604 + _594;
      float _606 = _605 + _597;
      float _607 = c_specular_scale.z * _569;
      float _608 = _607 * _280;
      float _609 = _557 + (c_ps_user[13].x);
      float _610 = _609 + _561;
      float _611 = c_diffuse_scale.z * _610;
      float _612 = _611 * c_mat_diffuse.z;
      float _613 = _576 * c_specular_scale.z;
      float _614 = _613 + _608;
      float _615 = _614 * c_mat_specular.z;
      float _616 = _529 + _505;
      float _617 = _616 + _518;
      float _618 = _617 + (c_ps_user[12].y);
      float _619 = _618 + _532;
      float _620 = _619 + _552;
      float _621 = _620 + _565;
      float _622 = _621 + _548;
      float _623 = _622 + _612;
      float _624 = _623 + _615;
      float _625 = saturate(_588);
      float _626 = saturate(_606);
      float _627 = saturate(_624);
      _654 = _625;
      _655 = _626;
      _656 = _627;
   }
   else
   {
      float _629 = _476 * _151;
      float _630 = _499 + _351;
      float _631 = _630 + _483;
      float _632 = _631 + _629;
      float _633 = _475 * _151;
      float _634 = _497 + _350;
      float _635 = _634 + _482;
      float _636 = _635 + _633;
      float _637 = _474 * _151;
      float _638 = _495 + _349;
      float _639 = _638 + _481;
      float _640 = _639 + _637;
      float _641 = c_diffuse_scale.x * c_mat_diffuse.x;
      float _642 = _641 * _59;
      float _643 = _642 * _640;
      float _644 = c_diffuse_scale.y * c_mat_diffuse.y;
      float _645 = _644 * _60;
      float _646 = _645 * _636;
      float _647 = c_diffuse_scale.z * c_mat_diffuse.z;
      float _648 = _647 * _61;
      float _649 = _648 * _632;
      float _650 = _643 + _278;
      float _651 = _646 + _279;
      float _652 = _649 + _280;
      _654 = _650;
      _655 = _651;
      _656 = _652;
   }
   float _661 = c_mat_opacity * _54.w;
   float _662 = _661 * c_opacity_scale;
   float _663 = _662 * c_global_opacity;
   float _664 = _654 * COLOR.x;
   float _665 = _655 * COLOR.y;
   float _666 = _656 * COLOR.z;
   int _667 = c_bool & 16;
   bool _668 = (_667 == 0);
   if (!_668)
   {
      float _672 = c_tone_control.x * _664;
      float _673 = c_tone_control.x * _665;
      float _674 = c_tone_control.x * _666;
      float _676 = log2(_672);
      float _677 = log2(_673);
      float _678 = log2(_674);
      float _679 = _676 * c_tone_control.y;
      float _680 = _677 * c_tone_control.y;
      float _681 = _678 * c_tone_control.y;
      float _682 = exp2(_679);
      float _683 = exp2(_680);
      float _684 = exp2(_681);
      _732 = _682;
      _733 = _683;
      _734 = _684;
   }
   else
   {
      if (_485)
      {
         float _689 = c_tone_control.x * _664;
         float _690 = c_tone_control.x * _665;
         float _691 = c_tone_control.x * _666;
         float _692 = _689 * 2.0f;
         float _693 = _690 * 2.0f;
         float _694 = _691 * 2.0f;
         float _695 = _689 * 0.30000001192092896f;
         float _696 = _690 * 0.30000001192092896f;
         float _697 = _691 * 0.30000001192092896f;
         float _698 = _695 + 0.05000000074505806f;
         float _699 = _696 + 0.05000000074505806f;
         float _700 = _697 + 0.05000000074505806f;
         float _701 = _698 * _692;
         float _702 = _699 * _693;
         float _703 = _700 * _694;
         float _704 = _701 + 0.004000000189989805f;
         float _705 = _702 + 0.004000000189989805f;
         float _706 = _703 + 0.004000000189989805f;
         float _707 = _695 + 0.5f;
         float _708 = _696 + 0.5f;
         float _709 = _697 + 0.5f;
         float _710 = _707 * _692;
         float _711 = _708 * _693;
         float _712 = _709 * _694;
         float _713 = _710 + 0.06000000238418579f;
         float _714 = _711 + 0.06000000238418579f;
         float _715 = _712 + 0.06000000238418579f;
         float _716 = _704 / _713;
         float _717 = _705 / _714;
         float _718 = _706 / _715;
         float _719 = _716 + -0.06666666269302368f;
         float _720 = _717 + -0.06666666269302368f;
         float _721 = _718 + -0.06666666269302368f;
         float _722 = _719 * 1.3790643215179443f;
         float _723 = _720 * 1.3790643215179443f;
         float _724 = _721 * 1.3790643215179443f;
         float _725 = YKR_MaterialCurve(_722, _689);
         float _726 = YKR_MaterialCurve(_723, _690);
         float _727 = YKR_MaterialCurve(_724, _691);
         float _728 = sqrt(_725);
         float _729 = sqrt(_726);
         float _730 = sqrt(_727);
         _732 = _728;
         _733 = _729;
         _734 = _730;
      }
      else
      {
         _732 = _664;
         _733 = _665;
         _734 = _666;
      }
   }
   int _735 = c_bool & 4;
   bool _736 = (_735 == 0);
   if (_736)
   {
      float _738 = saturate(COLOR_1.x);
      float _743 = (c_fog_param[0].x) - _732;
      float _744 = (c_fog_param[0].y) - _733;
      float _745 = (c_fog_param[0].z) - _734;
      float _746 = _743 * _738;
      float _747 = _744 * _738;
      float _748 = _745 * _738;
      _750 = _746;
      _751 = _747;
      _752 = _748;
   }
   else
   {
      _750 = COLOR_1.x;
      _751 = COLOR_1.y;
      _752 = COLOR_1.z;
   }
   float _753 = _750 + _732;
   float _754 = _751 + _733;
   float _755 = _752 + _734;
   if (!_485)
   {
      float _763 = (c_ps_user[3].x) * (c_ps_user[12].z);
      float _764 = (c_ps_user[3].y) * (c_ps_user[12].z);
      float _765 = (c_ps_user[3].z) * (c_ps_user[12].z);
      float _767 = (c_ps_user[12].w) * COLOR.x;
      float _768 = (c_ps_user[12].w) * COLOR.y;
      float _769 = (c_ps_user[12].w) * COLOR.z;
      float _770 = _767 + _753;
      float _771 = _770 + _763;
      float _772 = _768 + _754;
      float _773 = _772 + _764;
      float _774 = _769 + _755;
      float _775 = _774 + _765;
      _777 = _771;
      _778 = _773;
      _779 = _775;
   }
   else
   {
      _777 = _753;
      _778 = _754;
      _779 = _755;
   }
   float _782 = _663 - c_mat_punchthrough_ref;
   bool _783 = (_782 < 0.0f);
   if (_783)
      discard;
   float _789 = (c_ps_col_scl[0].x) * _777;
   float _790 = (c_ps_col_scl[0].y) * _778;
   float _791 = (c_ps_col_scl[0].z) * _779;
   float _792 = (c_ps_col_scl[0].w) * _663;
   SV_Target.x = _789;
   SV_Target.y = _790;
   SV_Target.z = _791;
   SV_Target.w = _792;
   int _795 = c_bool & 8;
   bool _796 = (_795 != 0);
   float _797 = select(_796, _777, _789);
   float _798 = select(_796, _778, _790);
   float _799 = select(_796, _779, _791);
   float _800 = select(_796, _663, _792);
   SV_Target.x = _797;
   SV_Target.y = _798;
   SV_Target.z = _799;
   SV_Target.w = _800;
   return SV_Target;
}
