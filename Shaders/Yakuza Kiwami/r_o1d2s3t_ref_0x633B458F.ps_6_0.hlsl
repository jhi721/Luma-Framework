// Yakuza Kiwami material r_o1d2s3t[ref] (0x633B458F, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
// applied by a script: the inline Hable tonemap's saturate goes through
// YKR_MaterialCurve (tangent continuation in HDR), and the light pre-pass buffer t14 through YKR_LightBuffer.

#include "Includes/Common.hlsl"

Texture2D<float4> s0_t : register(t0);

Texture2D<float4> s3_t : register(t3);

Texture2D<float4> s5_t : register(t5);

TextureCube<float4> s6c_t : register(t6);

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

cbuffer cb3 : register(b3)
{
   float4 c_light_col[6] : packoffset(c000.x);
   float4 c_light_vec[6] : packoffset(c006.x);
};

SamplerState s0_s : register(s0);

SamplerState s3_s : register(s3);

SamplerState s5_s : register(s5);

SamplerState s6c_s : register(s6);

SamplerState s13_s : register(s13);

SamplerState s14_s : register(s14);

float4 main(
    linear float4 TEXCOORD : TEXCOORD,
    linear float2 TEXCOORD_1 : TEXCOORD1,
    linear float4 COLOR : COLOR,
    linear float4 COLOR_1 : COLOR1,
    linear float4 TEXCOORD_2 : TEXCOORD2,
    linear float3 TEXCOORD_3 : TEXCOORD3,
    linear float3 TEXCOORD_4 : TEXCOORD4,
    linear float3 TEXCOORD_5 : TEXCOORD5,
    linear float4 TEXCOORD_6 : TEXCOORD6,
    noperspective float4 SV_Position : SV_Position) : SV_Target
{
   float4 SV_Target = 0;
   float4 _52 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _57 = _52.x * _52.x;
   float _58 = _52.y * _52.y;
   float _59 = _52.z * _52.z;
   float4 _60 = s3_t.Sample(s3_s, float2(TEXCOORD.z, TEXCOORD.w));
   float4 _65 = s5_t.Sample(s5_s, float2(TEXCOORD_1.x, TEXCOORD_1.y));
   float _68 = _65.x * 2.0f;
   float _69 = _65.y * 2.0f;
   float _70 = _68 + -1.0f;
   float _71 = _69 + -1.0f;
   float _72 = dot(float2(_70, _71), float2(_70, _71));
   float _73 = 1.0f - _72;
   float _74 = saturate(_73);
   float _75 = sqrt(_74);
   float _76 = TEXCOORD_2.y * TEXCOORD_6.z;
   float _77 = TEXCOORD_2.z * TEXCOORD_6.y;
   float _78 = _76 - _77;
   float _79 = TEXCOORD_2.z * TEXCOORD_6.x;
   float _80 = TEXCOORD_2.x * TEXCOORD_6.z;
   float _81 = _79 - _80;
   float _82 = TEXCOORD_2.x * TEXCOORD_6.y;
   float _83 = TEXCOORD_2.y * TEXCOORD_6.x;
   float _84 = _82 - _83;
   float _85 = _70 * TEXCOORD_6.x;
   float _86 = _70 * TEXCOORD_6.y;
   float _87 = _70 * TEXCOORD_6.z;
   float _88 = _71 * TEXCOORD_6.w;
   float _89 = _88 * _78;
   float _90 = _88 * _81;
   float _91 = _88 * _84;
   float _92 = _75 * TEXCOORD_2.x;
   float _93 = _75 * TEXCOORD_2.y;
   float _94 = _75 * TEXCOORD_2.z;
   float _95 = _92 + _85;
   float _96 = _95 + _89;
   float _97 = _93 + _86;
   float _98 = _97 + _90;
   float _99 = _94 + _87;
   float _100 = _99 + _91;
   float _101 = dot(float3(_96, _98, _100), float3(_96, _98, _100));
   float _102 = rsqrt(_101);
   float _103 = _96 * _102;
   float _104 = _98 * _102;
   float _105 = _100 * _102;
   float _106 = dot(float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z), float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z));
   float _107 = rsqrt(_106);
   float _108 = _107 * TEXCOORD_3.x;
   float _109 = _107 * TEXCOORD_3.y;
   float _110 = _107 * TEXCOORD_3.z;
   float _115 = -0.0f - (c_light_vec[0].x);
   float _116 = -0.0f - (c_light_vec[0].y);
   float _117 = -0.0f - (c_light_vec[0].z);
   float _118 = dot(float3(_103, _104, _105), float3(_115, _116, _117));
   float _122 = c_rt_size.z * SV_Position.x;
   float _123 = c_rt_size.w * SV_Position.y;
   int _126 = c_bool & 8;
   bool _127 = (_126 == 0);
   float _134;
   float _135;
   float _136;
   float _148;
   float _149;
   float _185;
   float _186;
   float _187;
   float _188;
   float _202;
   float _203;
   float _238;
   float _239;
   float _240;
   float _274;
   float _275;
   float _276;
   float _277;
   float _278;
   float _279;
   float _433;
   float _434;
   float _435;
   float _436;
   float _437;
   float _438;
   float _614;
   float _615;
   float _616;
   float _617;
   float _618;
   float _619;
   float _741;
   float _742;
   float _743;
   float _759;
   float _760;
   float _761;
   if (!_127)
   {
      float4 _129 = s13_t.Sample(s13_s, float2(_122, _123));
      _134 = _129.x;
      _135 = _129.y;
      _136 = _129.w;
   }
   else
   {
      _134 = 1.0f;
      _135 = 1.0f;
      _136 = 1.0f;
   }
   if (!_127)
   {
      float _141 = (c_sdw_param[0].x) * _134;
      float _142 = (c_sdw_param[0].z) * _135;
      float _145 = _141 + (c_sdw_param[0].y);
      float _146 = _142 + (c_sdw_param[0].w);
      _148 = _145;
      _149 = _146;
   }
   else
   {
      _148 = _134;
      _149 = _135;
   }
   float _153 = c_rt_size.z * SV_Position.x;
   float _154 = c_rt_size.w * SV_Position.y;
   float4 _155 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_153, _154)));
   int _162 = c_bool & 16384;
   bool _163 = (_162 == 0);
   if (!_163)
   {
      float _165 = _155.x * 2.0f;
      float _166 = _155.y * 2.0f;
      float _167 = _155.z * 2.0f;
      float _168 = _155.w * 2.0f;
      _185 = _165;
      _186 = _166;
      _187 = _167;
      _188 = _168;
   }
   else
   {
      int _170 = c_bool & 8192;
      bool _171 = (_170 == 0);
      if (!_171)
      {
         float _173 = _155.x * 2.0f;
         float _174 = _155.y * 2.0f;
         float _175 = _155.z * 2.0f;
         float _176 = _155.w * 2.0f;
         _185 = _173;
         _186 = _174;
         _187 = _175;
         _188 = _176;
      }
      else
      {
         float _180 = _155.x / c_tone_control.z;
         float _181 = _155.y / c_tone_control.z;
         float _182 = _155.z / c_tone_control.z;
         float _183 = _155.w / c_tone_control.z;
         _185 = _180;
         _186 = _181;
         _187 = _182;
         _188 = _183;
      }
   }
   int _189 = c_bool & 8;
   bool _190 = (_189 == 0);
   if (!_190)
   {
      float _195 = (c_sdw_param[0].x) * _148;
      float _196 = (c_sdw_param[0].z) * _149;
      float _199 = _195 + (c_sdw_param[0].y);
      float _200 = _196 + (c_sdw_param[0].w);
      _202 = _199;
      _203 = _200;
   }
   else
   {
      _202 = _148;
      _203 = _149;
   }
   float _204 = _203 * _202;
   float _209 = 1.0f - (c_light_col[5].x);
   float _210 = 1.0f - (c_light_col[5].y);
   float _211 = 1.0f - (c_light_col[5].z);
   float _212 = _209 * _204;
   float _213 = _210 * _204;
   float _214 = _211 * _204;
   float _215 = _212 + (c_light_col[5].x);
   float _216 = _213 + (c_light_col[5].y);
   float _217 = _214 + (c_light_col[5].z);
   float _218 = _136 * COLOR.x;
   float _219 = _136 * COLOR.y;
   float _220 = _136 * COLOR.z;
   if (!_190)
   {
      float _224 = (c_sdw_param[2].x) * _215;
      float _225 = (c_sdw_param[2].x) * _216;
      float _226 = (c_sdw_param[2].x) * _217;
      float _228 = _224 + (c_sdw_param[2].y);
      float _229 = _225 + (c_sdw_param[2].y);
      float _230 = _226 + (c_sdw_param[2].y);
      float _233 = (c_sdw_param[2].w) + (c_sdw_param[2].z);
      float _234 = _228 * _233;
      float _235 = _229 * _233;
      float _236 = _230 * _233;
      _238 = _234;
      _239 = _235;
      _240 = _236;
   }
   else
   {
      _238 = _215;
      _239 = _216;
      _240 = _217;
   }
   float _241 = _238 * _136;
   float _242 = _239 * _136;
   float _243 = _240 * _136;
   float _254 = c_mat_emissive.x * _57;
   float _255 = _254 * c_emissive_scale;
   float _256 = c_mat_emissive.y * _58;
   float _257 = _256 * c_emissive_scale;
   float _258 = c_mat_emissive.z * _59;
   float _259 = _258 * c_emissive_scale;
   int _260 = c_bool & 2048;
   bool _261 = (_260 == 0);
   if (_261)
   {
      float _263 = _203 * _185;
      float _264 = _203 * _186;
      float _265 = _203 * _187;
      float _266 = _263 + TEXCOORD_4.x;
      float _267 = _264 + TEXCOORD_4.y;
      float _268 = _265 + TEXCOORD_4.z;
      float _269 = _203 * _188;
      float _270 = _269 + TEXCOORD_5.x;
      float _271 = _269 + TEXCOORD_5.y;
      float _272 = _269 + TEXCOORD_5.z;
      _274 = _266;
      _275 = _267;
      _276 = _268;
      _277 = _270;
      _278 = _271;
      _279 = _272;
   }
   else
   {
      _274 = TEXCOORD_4.x;
      _275 = TEXCOORD_4.y;
      _276 = TEXCOORD_4.z;
      _277 = TEXCOORD_5.x;
      _278 = TEXCOORD_5.y;
      _279 = TEXCOORD_5.z;
   }
   float _284 = -0.0f - (c_light_vec[1].x);
   float _285 = -0.0f - (c_light_vec[1].y);
   float _286 = -0.0f - (c_light_vec[1].z);
   float _287 = dot(float3(_103, _104, _105), float3(_284, _285, _286));
   float _288 = saturate(_287);
   float _293 = saturate(_118);
   float _294 = (c_light_col[0].x) * _293;
   float _295 = (c_light_col[0].y) * _293;
   float _296 = (c_light_col[0].z) * _293;
   float _301 = (c_light_col[1].x) * _288;
   float _302 = (c_light_col[1].y) * _288;
   float _303 = (c_light_col[1].z) * _288;
   float _312 = c_diffuse_scale.x * c_mat_diffuse.x;
   float _313 = c_diffuse_scale.y * c_mat_diffuse.y;
   float _314 = c_diffuse_scale.z * c_mat_diffuse.z;
   float _315 = _294 * _215;
   float _316 = _295 * _216;
   float _317 = _296 * _217;
   float _318 = _301 * _203;
   float _319 = _302 * _203;
   float _320 = _303 * _203;
   float _321 = _318 + _315;
   float _322 = _319 + _316;
   float _323 = _320 + _317;
   float _324 = _312 * _321;
   float _325 = _313 * _322;
   float _326 = _314 * _323;
   float _327 = _255 + c_scene_ambient.x;
   float _328 = _327 + _324;
   float _329 = _257 + c_scene_ambient.y;
   float _330 = _329 + _325;
   float _331 = _259 + c_scene_ambient.z;
   float _332 = _331 + _326;
   float _333 = _328 * _218;
   float _334 = _330 * _219;
   float _335 = _332 * _220;
   float _336 = _274 * _241;
   float _337 = _275 * _242;
   float _338 = _276 * _243;
   float _339 = _333 + _336;
   float _340 = _334 + _337;
   float _341 = _335 + _338;
   float _346 = (c_light_vec[0].x) + _108;
   float _347 = (c_light_vec[0].y) + _109;
   float _348 = (c_light_vec[0].z) + _110;
   float _349 = dot(float3(_346, _347, _348), float3(_346, _347, _348));
   float _350 = rsqrt(_349);
   float _351 = _346 * _350;
   float _352 = -0.0f - _351;
   float _353 = _347 * _350;
   float _354 = -0.0f - _353;
   float _355 = _348 * _350;
   float _356 = -0.0f - _355;
   float _357 = dot(float3(_352, _354, _356), float3(_103, _104, _105));
   float _358 = saturate(_357);
   float _359 = _109 + -0.30000001192092896f;
   float _360 = dot(float3(_108, _359, _110), float3(_108, _359, _110));
   float _361 = rsqrt(_360);
   float _362 = _108 * _361;
   float _363 = -0.0f - _362;
   float _364 = _359 * _361;
   float _365 = -0.0f - _364;
   float _366 = _110 * _361;
   float _367 = -0.0f - _366;
   float _368 = dot(float3(_363, _365, _367), float3(_103, _104, _105));
   float _369 = saturate(_368);
   float _372 = log2(_358);
   float _373 = log2(_369);
   float _374 = _372 * c_mat_power;
   float _375 = _373 * c_mat_power;
   float _376 = exp2(_374);
   float _377 = exp2(_375);
   float _380 = _376 * (c_light_col[0].x);
   float _381 = _376 * (c_light_col[0].y);
   float _382 = _376 * (c_light_col[0].z);
   float _383 = _377 * 2.0f;
   float _384 = _383 * (c_light_vec[5].x);
   float _385 = _384 * (c_light_col[1].x);
   float _386 = _384 * (c_light_col[1].y);
   float _387 = _384 * (c_light_col[1].z);
   float _388 = _380 * _215;
   float _389 = _381 * _216;
   float _390 = _382 * _217;
   float _391 = _385 * _288;
   float _392 = _386 * _288;
   float _393 = _387 * _288;
   float _394 = _391 + _388;
   float _395 = _392 + _389;
   float _396 = _393 + _390;
   float _403 = c_mat_specular.x * c_specular_scale.x;
   float _404 = _403 * _218;
   float _405 = _404 * _394;
   float _406 = c_mat_specular.y * c_specular_scale.y;
   float _407 = _406 * _219;
   float _408 = _407 * _395;
   float _409 = c_mat_specular.z * c_specular_scale.z;
   float _410 = _409 * _220;
   float _411 = _410 * _396;
   float _412 = _277 * _241;
   float _413 = _412 * _377;
   float _414 = _278 * _242;
   float _415 = _414 * _377;
   float _416 = _279 * _243;
   float _417 = _416 * _377;
   float _418 = _405 + _413;
   float _419 = _408 + _415;
   float _420 = _411 + _417;
   if (!_261)
   {
      float _422 = _203 * _185;
      float _423 = _203 * _186;
      float _424 = _203 * _187;
      float _425 = _339 + _422;
      float _426 = _340 + _423;
      float _427 = _341 + _424;
      float _428 = _203 * _188;
      float _429 = _418 + _428;
      float _430 = _419 + _428;
      float _431 = _420 + _428;
      _433 = _425;
      _434 = _426;
      _435 = _427;
      _436 = _429;
      _437 = _430;
      _438 = _431;
   }
   else
   {
      _433 = _339;
      _434 = _340;
      _435 = _341;
      _436 = _418;
      _437 = _419;
      _438 = _420;
   }
   int _439 = c_bool & 128;
   bool _440 = (_439 != 0);
   float _441 = select(_440, 0.0f, _436);
   float _442 = select(_440, 0.0f, _437);
   float _443 = select(_440, 0.0f, _438);
   float _444 = _433 * _57;
   float _445 = _434 * _58;
   float _446 = _435 * _59;
   float _447 = _441 * _60.x;
   float _448 = _442 * _60.y;
   float _449 = _443 * _60.z;
   int _450 = c_bool & 32768;
   bool _451 = (_450 == 0);
   if (!_451)
   {
      float _455 = (c_ps_user[11].x) * _57;
      float _456 = (c_ps_user[11].x) * _58;
      float _457 = (c_ps_user[11].x) * _59;
      float _459 = _103 + 1.0f;
      float _460 = _104 + 1.0f;
      float _461 = _105 + 1.0f;
      float _462 = _459 * 0.5f;
      float _463 = _460 * 0.5f;
      float _464 = _461 * 0.5f;
      float _465 = saturate(_462);
      float _466 = saturate(_463);
      float _467 = saturate(_464);
      float _468 = (c_ps_user[11].y) * _465;
      float _469 = _466 * (c_ps_user[11].y);
      float _470 = _467 * (c_ps_user[11].y);
      float _472 = (c_ps_user[11].z) * _60.x;
      float _473 = (c_ps_user[11].z) * _60.y;
      float _474 = (c_ps_user[11].z) * _60.z;
      float _476 = c_mat_power * 0.009999999776482582f;
      float _477 = _476 * (c_ps_user[11].w);
      float _480 = (c_ps_user[12].x) * _215;
      float _481 = (c_ps_user[12].x) * _216;
      float _482 = (c_ps_user[12].x) * _217;
      float _484 = (c_ps_user[12].y) * _218;
      float _485 = (c_ps_user[12].y) * _219;
      float _486 = (c_ps_user[12].y) * _220;
      float _488 = (c_ps_user[12].z) * _241;
      float _489 = (c_ps_user[12].z) * _242;
      float _490 = (c_ps_user[12].z) * _243;
      float _496 = -0.0f - _108;
      float _497 = -0.0f - _109;
      float _498 = -0.0f - _110;
      float _499 = dot(float3(_496, _497, _498), float3(_103, _104, _105));
      float _500 = 1.0f - _499;
      float _501 = _500 * _500;
      float _502 = _500 * 0.9722222089767456f;
      float _503 = _501 * _501;
      float _504 = _503 * _502;
      float _505 = _504 + 0.02777777425944805f;
      float _506 = _505 * (c_ps_user[13].z);
      float _508 = (c_ps_user[13].w) * c_scene_ambient.x;
      float _509 = (c_ps_user[13].w) * c_scene_ambient.y;
      float _510 = (c_ps_user[13].w) * c_scene_ambient.z;
      float _513 = (c_ps_user[14].x) * _255;
      float _514 = (c_ps_user[14].x) * _257;
      float _515 = (c_ps_user[14].x) * _259;
      float _517 = (c_ps_user[14].y) * _294;
      float _518 = (c_ps_user[14].y) * _295;
      float _519 = (c_ps_user[14].y) * _296;
      float _521 = (c_ps_user[14].z) * _301;
      float _522 = (c_ps_user[14].z) * _302;
      float _523 = (c_ps_user[14].z) * _303;
      float _525 = (c_ps_user[14].w) * _274;
      float _526 = (c_ps_user[14].w) * _275;
      float _527 = (c_ps_user[14].w) * _276;
      float _530 = (c_ps_user[15].x) * _380;
      float _531 = (c_ps_user[15].x) * _381;
      float _532 = (c_ps_user[15].x) * _382;
      float _534 = _403 * _385;
      float _535 = _534 * (c_ps_user[15].y);
      float _536 = _406 * _386;
      float _537 = _536 * (c_ps_user[15].y);
      float _538 = _409 * _387;
      float _539 = _538 * (c_ps_user[15].y);
      float _541 = (c_ps_user[15].z) * _277;
      float _542 = (c_ps_user[15].z) * _278;
      float _543 = (c_ps_user[15].z) * _279;
      float _544 = _535 + _541;
      float _545 = _544 * _377;
      float _546 = _517 + (c_ps_user[13].x);
      float _547 = _546 + _521;
      float _548 = c_diffuse_scale.x * _547;
      float _549 = _548 * c_mat_diffuse.x;
      float _550 = (c_ps_user[13].y) + _472;
      float _551 = _550 + _530;
      float _552 = c_mat_specular.x * _551;
      float _553 = _552 * c_specular_scale.x;
      float _554 = _468 + _455;
      float _555 = _554 + _477;
      float _556 = _555 + (c_ps_user[12].w);
      float _557 = _556 + _480;
      float _558 = _557 + _484;
      float _559 = _558 + _488;
      float _560 = _559 + _508;
      float _561 = _560 + _513;
      float _562 = _561 + _525;
      float _563 = _562 + _506;
      float _564 = _563 + _545;
      float _565 = _564 + _549;
      float _566 = _565 + _553;
      float _567 = _537 + _542;
      float _568 = _567 * _377;
      float _569 = _518 + (c_ps_user[13].x);
      float _570 = _569 + _522;
      float _571 = c_diffuse_scale.y * _570;
      float _572 = _571 * c_mat_diffuse.y;
      float _573 = (c_ps_user[13].y) + _473;
      float _574 = _573 + _531;
      float _575 = c_mat_specular.y * _574;
      float _576 = _575 * c_specular_scale.y;
      float _577 = _469 + _456;
      float _578 = _577 + _477;
      float _579 = _578 + (c_ps_user[12].w);
      float _580 = _579 + _481;
      float _581 = _580 + _485;
      float _582 = _581 + _489;
      float _583 = _582 + _509;
      float _584 = _583 + _514;
      float _585 = _584 + _526;
      float _586 = _585 + _506;
      float _587 = _586 + _568;
      float _588 = _587 + _572;
      float _589 = _588 + _576;
      float _590 = _539 + _543;
      float _591 = _590 * _377;
      float _592 = _519 + (c_ps_user[13].x);
      float _593 = _592 + _523;
      float _594 = c_diffuse_scale.z * _593;
      float _595 = _594 * c_mat_diffuse.z;
      float _596 = (c_ps_user[13].y) + _474;
      float _597 = _596 + _532;
      float _598 = c_mat_specular.z * _597;
      float _599 = _598 * c_specular_scale.z;
      float _600 = _477 + _457;
      float _601 = _600 + _470;
      float _602 = _601 + (c_ps_user[12].w);
      float _603 = _602 + _482;
      float _604 = _603 + _486;
      float _605 = _604 + _490;
      float _606 = _605 + _510;
      float _607 = _606 + _515;
      float _608 = _607 + _527;
      float _609 = _608 + _506;
      float _610 = _609 + _591;
      float _611 = _610 + _595;
      float _612 = _611 + _599;
      _614 = _566;
      _615 = _589;
      _616 = _612;
      _617 = 0.0f;
      _618 = 0.0f;
      _619 = 0.0f;
   }
   else
   {
      _614 = _444;
      _615 = _445;
      _616 = _446;
      _617 = _447;
      _618 = _448;
      _619 = _449;
   }
   float _620 = _617 + _614;
   float _621 = _618 + _615;
   float _622 = _619 + _616;
   float _623 = dot(float3(_108, _109, _110), float3(_103, _104, _105));
   float _624 = _623 * 2.0f;
   float _625 = _624 * _103;
   float _626 = _624 * _104;
   float _627 = _624 * _105;
   float _628 = _108 - _625;
   float _629 = _109 - _626;
   float _630 = _110 - _627;
   float _631 = -0.0f - _628;
   float4 _632 = s6c_t.Sample(s6c_s, float3(_631, _629, _630));
   float _639 = _149 * _148;
   float _640 = _118 + 0.75f;
   float _641 = saturate(_640);
   float _642 = _639 * _641;
   float _643 = max(_642, (c_light_vec[5].y));
   float _644 = max(_632.w, 0.003921568859368563f);
   float _645 = 0.25f / _644;
   float _646 = _645 * _632.x;
   float _647 = _645 * _632.y;
   float _648 = _645 * _632.z;
   float _649 = _60.w * TEXCOORD_2.w;
   float _650 = _643 * _649;
   float _651 = _650 * _60.x;
   float _652 = _646 * _646;
   float _653 = _652 * _651;
   float _654 = _650 * _60.y;
   float _655 = _647 * _647;
   float _656 = _655 * _654;
   float _657 = _650 * _60.z;
   float _658 = _648 * _648;
   float _659 = _658 * _657;
   float _660 = _620 + _653;
   float _661 = _621 + _656;
   float _662 = _622 + _659;
   float _669 = c_mat_opacity * _52.w;
   float _670 = _669 * c_opacity_scale;
   float _671 = _670 * c_global_opacity;
   int _674 = c_bool & 16;
   bool _675 = (_674 == 0);
   if (!_675)
   {
      float _679 = c_tone_control.x * _660;
      float _680 = c_tone_control.x * _661;
      float _681 = c_tone_control.x * _662;
      float _683 = log2(_679);
      float _684 = log2(_680);
      float _685 = log2(_681);
      float _686 = _683 * c_tone_control.y;
      float _687 = _684 * c_tone_control.y;
      float _688 = _685 * c_tone_control.y;
      float _689 = exp2(_686);
      float _690 = exp2(_687);
      float _691 = exp2(_688);
      _741 = _689;
      _742 = _690;
      _743 = _691;
   }
   else
   {
      int _693 = c_bool & 32768;
      bool _694 = (_693 == 0);
      if (_694)
      {
         float _698 = c_tone_control.x * _660;
         float _699 = c_tone_control.x * _661;
         float _700 = c_tone_control.x * _662;
         float _701 = _698 * 2.0f;
         float _702 = _699 * 2.0f;
         float _703 = _700 * 2.0f;
         float _704 = _698 * 0.30000001192092896f;
         float _705 = _699 * 0.30000001192092896f;
         float _706 = _700 * 0.30000001192092896f;
         float _707 = _704 + 0.05000000074505806f;
         float _708 = _705 + 0.05000000074505806f;
         float _709 = _706 + 0.05000000074505806f;
         float _710 = _707 * _701;
         float _711 = _708 * _702;
         float _712 = _709 * _703;
         float _713 = _710 + 0.004000000189989805f;
         float _714 = _711 + 0.004000000189989805f;
         float _715 = _712 + 0.004000000189989805f;
         float _716 = _704 + 0.5f;
         float _717 = _705 + 0.5f;
         float _718 = _706 + 0.5f;
         float _719 = _716 * _701;
         float _720 = _717 * _702;
         float _721 = _718 * _703;
         float _722 = _719 + 0.06000000238418579f;
         float _723 = _720 + 0.06000000238418579f;
         float _724 = _721 + 0.06000000238418579f;
         float _725 = _713 / _722;
         float _726 = _714 / _723;
         float _727 = _715 / _724;
         float _728 = _725 + -0.06666666269302368f;
         float _729 = _726 + -0.06666666269302368f;
         float _730 = _727 + -0.06666666269302368f;
         float _731 = _728 * 1.3790643215179443f;
         float _732 = _729 * 1.3790643215179443f;
         float _733 = _730 * 1.3790643215179443f;
         float _734 = YKR_MaterialCurve(_731, _698);
         float _735 = YKR_MaterialCurve(_732, _699);
         float _736 = YKR_MaterialCurve(_733, _700);
         float _737 = sqrt(_734);
         float _738 = sqrt(_735);
         float _739 = sqrt(_736);
         _741 = _737;
         _742 = _738;
         _743 = _739;
      }
      else
      {
         _741 = _660;
         _742 = _661;
         _743 = _662;
      }
   }
   int _744 = c_bool & 4;
   bool _745 = (_744 == 0);
   if (_745)
   {
      float _747 = saturate(COLOR_1.x);
      float _752 = (c_fog_param[0].x) - _741;
      float _753 = (c_fog_param[0].y) - _742;
      float _754 = (c_fog_param[0].z) - _743;
      float _755 = _752 * _747;
      float _756 = _753 * _747;
      float _757 = _754 * _747;
      _759 = _755;
      _760 = _756;
      _761 = _757;
   }
   else
   {
      _759 = COLOR_1.x;
      _760 = COLOR_1.y;
      _761 = COLOR_1.z;
   }
   float _762 = _759 + _741;
   float _763 = _760 + _742;
   float _764 = _761 + _743;
   float _770 = (c_ps_col_scl[0].x) * _762;
   float _771 = (c_ps_col_scl[0].y) * _763;
   float _772 = (c_ps_col_scl[0].z) * _764;
   float _773 = (c_ps_col_scl[0].w) * _671;
   SV_Target.x = _770;
   SV_Target.y = _771;
   SV_Target.z = _772;
   SV_Target.w = _773;
   int _776 = c_bool & 8;
   bool _777 = (_776 != 0);
   float _778 = select(_777, _762, _770);
   float _779 = select(_777, _763, _771);
   float _780 = select(_777, _764, _772);
   float _781 = select(_777, _671, _773);
   SV_Target.x = _778;
   SV_Target.y = _779;
   SV_Target.z = _780;
   SV_Target.w = _781;
   return SV_Target;
}
