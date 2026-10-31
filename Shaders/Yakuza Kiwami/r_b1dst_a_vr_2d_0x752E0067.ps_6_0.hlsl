// Yakuza Kiwami material r_b1dst_a(vr)2d (0x752E0067, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
// applied by a script: the inline Hable tonemap's saturate goes through
// YKR_MaterialCurve (tangent continuation in HDR), and the light pre-pass buffer t14 through YKR_LightBuffer.

#include "Includes/Common.hlsl"

Texture2D<float4> s0_t : register(t0);

Texture2D<float4> s1_t : register(t1);

Texture2D<float4> s3_t : register(t3);

Texture2D<float4> s5_t : register(t5);

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

SamplerState s1_s : register(s1);

SamplerState s3_s : register(s3);

SamplerState s5_s : register(s5);

SamplerState s13_s : register(s13);

SamplerState s14_s : register(s14);

float4 main(
    linear float4 TEXCOORD : TEXCOORD,
    linear float4 COLOR : COLOR,
    linear float4 COLOR_1 : COLOR1,
    linear float3 TEXCOORD_2 : TEXCOORD2,
    linear float3 TEXCOORD_3 : TEXCOORD3,
    linear float3 TEXCOORD_4 : TEXCOORD4,
    linear float3 TEXCOORD_5 : TEXCOORD5,
    linear float4 TEXCOORD_6 : TEXCOORD6,
    noperspective float4 SV_Position : SV_Position) : SV_Target
{
   float4 SV_Target = 0;
   float4 _51 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _56 = _51.x * _51.x;
   float _57 = _51.y * _51.y;
   float _58 = _51.z * _51.z;
   float _62 = (c_attribute_ps[0].x) * TEXCOORD.x;
   float _63 = (c_attribute_ps[0].y) * TEXCOORD.y;
   float4 _64 = s3_t.Sample(s3_s, float2(_62, _63));
   float _71 = (c_attribute_ps[0].z) * TEXCOORD.x;
   float _72 = (c_attribute_ps[0].w) * TEXCOORD.y;
   float4 _73 = s5_t.Sample(s5_s, float2(_71, _72));
   float4 _76 = s1_t.Sample(s1_s, float2(TEXCOORD.z, TEXCOORD.w));
   float _80 = _73.x * 2.0f;
   float _81 = _73.y * 2.0f;
   float _82 = _80 + -1.0f;
   float _83 = _81 + -1.0f;
   float _84 = dot(float2(_82, _83), float2(_82, _83));
   float _85 = 1.0f - _84;
   float _86 = saturate(_85);
   float _87 = sqrt(_86);
   float _88 = TEXCOORD_2.y * TEXCOORD_6.z;
   float _89 = TEXCOORD_2.z * TEXCOORD_6.y;
   float _90 = _88 - _89;
   float _91 = TEXCOORD_2.z * TEXCOORD_6.x;
   float _92 = TEXCOORD_2.x * TEXCOORD_6.z;
   float _93 = _91 - _92;
   float _94 = TEXCOORD_2.x * TEXCOORD_6.y;
   float _95 = TEXCOORD_2.y * TEXCOORD_6.x;
   float _96 = _94 - _95;
   float _97 = _82 * TEXCOORD_6.x;
   float _98 = _82 * TEXCOORD_6.y;
   float _99 = _82 * TEXCOORD_6.z;
   float _100 = _83 * TEXCOORD_6.w;
   float _101 = _100 * _90;
   float _102 = _100 * _93;
   float _103 = _100 * _96;
   float _104 = _101 + _97;
   float _105 = _102 + _98;
   float _106 = _103 + _99;
   float _107 = _87 * TEXCOORD_2.x;
   float _108 = _87 * TEXCOORD_2.y;
   float _109 = _87 * TEXCOORD_2.z;
   float _110 = _104 + _107;
   float _111 = _105 + _108;
   float _112 = _106 + _109;
   float _113 = dot(float3(_110, _111, _112), float3(_110, _111, _112));
   float _114 = rsqrt(_113);
   float _115 = _110 * _114;
   float _116 = _111 * _114;
   float _117 = _112 * _114;
   float _118 = dot(float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z), float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z));
   float _119 = rsqrt(_118);
   float _120 = _119 * TEXCOORD_3.x;
   float _121 = _119 * TEXCOORD_3.y;
   float _122 = _119 * TEXCOORD_3.z;
   float _127 = -0.0f - (c_light_vec[0].x);
   float _128 = -0.0f - (c_light_vec[0].y);
   float _129 = -0.0f - (c_light_vec[0].z);
   float _130 = dot(float3(_115, _116, _117), float3(_127, _128, _129));
   float _134 = c_rt_size.z * SV_Position.x;
   float _135 = c_rt_size.w * SV_Position.y;
   int _138 = c_bool & 8;
   bool _139 = (_138 == 0);
   float _146;
   float _147;
   float _148;
   float _160;
   float _161;
   float _197;
   float _198;
   float _199;
   float _200;
   float _214;
   float _215;
   float _250;
   float _251;
   float _252;
   float _286;
   float _287;
   float _288;
   float _289;
   float _290;
   float _291;
   float _445;
   float _446;
   float _447;
   float _448;
   float _449;
   float _450;
   float _626;
   float _627;
   float _628;
   float _629;
   float _630;
   float _631;
   float _717;
   float _718;
   float _719;
   float _735;
   float _736;
   float _737;
   if (!_139)
   {
      float4 _141 = s13_t.Sample(s13_s, float2(_134, _135));
      _146 = _141.x;
      _147 = _141.y;
      _148 = _141.w;
   }
   else
   {
      _146 = 1.0f;
      _147 = 1.0f;
      _148 = 1.0f;
   }
   if (!_139)
   {
      float _153 = (c_sdw_param[0].x) * _146;
      float _154 = (c_sdw_param[0].z) * _147;
      float _157 = _153 + (c_sdw_param[0].y);
      float _158 = _154 + (c_sdw_param[0].w);
      _160 = _157;
      _161 = _158;
   }
   else
   {
      _160 = _146;
      _161 = _147;
   }
   float _165 = c_rt_size.z * SV_Position.x;
   float _166 = c_rt_size.w * SV_Position.y;
   float4 _167 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_165, _166)));
   int _174 = c_bool & 16384;
   bool _175 = (_174 == 0);
   if (!_175)
   {
      float _177 = _167.x * 2.0f;
      float _178 = _167.y * 2.0f;
      float _179 = _167.z * 2.0f;
      float _180 = _167.w * 2.0f;
      _197 = _177;
      _198 = _178;
      _199 = _179;
      _200 = _180;
   }
   else
   {
      int _182 = c_bool & 8192;
      bool _183 = (_182 == 0);
      if (!_183)
      {
         float _185 = _167.x * 2.0f;
         float _186 = _167.y * 2.0f;
         float _187 = _167.z * 2.0f;
         float _188 = _167.w * 2.0f;
         _197 = _185;
         _198 = _186;
         _199 = _187;
         _200 = _188;
      }
      else
      {
         float _192 = _167.x / c_tone_control.z;
         float _193 = _167.y / c_tone_control.z;
         float _194 = _167.z / c_tone_control.z;
         float _195 = _167.w / c_tone_control.z;
         _197 = _192;
         _198 = _193;
         _199 = _194;
         _200 = _195;
      }
   }
   int _201 = c_bool & 8;
   bool _202 = (_201 == 0);
   if (!_202)
   {
      float _207 = (c_sdw_param[0].x) * _160;
      float _208 = (c_sdw_param[0].z) * _161;
      float _211 = _207 + (c_sdw_param[0].y);
      float _212 = _208 + (c_sdw_param[0].w);
      _214 = _211;
      _215 = _212;
   }
   else
   {
      _214 = _160;
      _215 = _161;
   }
   float _216 = _215 * _214;
   float _221 = 1.0f - (c_light_col[5].x);
   float _222 = 1.0f - (c_light_col[5].y);
   float _223 = 1.0f - (c_light_col[5].z);
   float _224 = _221 * _216;
   float _225 = _222 * _216;
   float _226 = _223 * _216;
   float _227 = _224 + (c_light_col[5].x);
   float _228 = _225 + (c_light_col[5].y);
   float _229 = _226 + (c_light_col[5].z);
   float _230 = _148 * COLOR.x;
   float _231 = _148 * COLOR.y;
   float _232 = _148 * COLOR.z;
   if (!_202)
   {
      float _236 = (c_sdw_param[2].x) * _227;
      float _237 = (c_sdw_param[2].x) * _228;
      float _238 = (c_sdw_param[2].x) * _229;
      float _240 = _236 + (c_sdw_param[2].y);
      float _241 = _237 + (c_sdw_param[2].y);
      float _242 = _238 + (c_sdw_param[2].y);
      float _245 = (c_sdw_param[2].w) + (c_sdw_param[2].z);
      float _246 = _240 * _245;
      float _247 = _241 * _245;
      float _248 = _242 * _245;
      _250 = _246;
      _251 = _247;
      _252 = _248;
   }
   else
   {
      _250 = _227;
      _251 = _228;
      _252 = _229;
   }
   float _253 = _250 * _148;
   float _254 = _251 * _148;
   float _255 = _252 * _148;
   float _266 = c_mat_emissive.x * _56;
   float _267 = _266 * c_emissive_scale;
   float _268 = c_mat_emissive.y * _57;
   float _269 = _268 * c_emissive_scale;
   float _270 = c_mat_emissive.z * _58;
   float _271 = _270 * c_emissive_scale;
   int _272 = c_bool & 2048;
   bool _273 = (_272 == 0);
   if (_273)
   {
      float _275 = _215 * _197;
      float _276 = _215 * _198;
      float _277 = _215 * _199;
      float _278 = _275 + TEXCOORD_4.x;
      float _279 = _276 + TEXCOORD_4.y;
      float _280 = _277 + TEXCOORD_4.z;
      float _281 = _215 * _200;
      float _282 = _281 + TEXCOORD_5.x;
      float _283 = _281 + TEXCOORD_5.y;
      float _284 = _281 + TEXCOORD_5.z;
      _286 = _278;
      _287 = _279;
      _288 = _280;
      _289 = _282;
      _290 = _283;
      _291 = _284;
   }
   else
   {
      _286 = TEXCOORD_4.x;
      _287 = TEXCOORD_4.y;
      _288 = TEXCOORD_4.z;
      _289 = TEXCOORD_5.x;
      _290 = TEXCOORD_5.y;
      _291 = TEXCOORD_5.z;
   }
   float _296 = -0.0f - (c_light_vec[1].x);
   float _297 = -0.0f - (c_light_vec[1].y);
   float _298 = -0.0f - (c_light_vec[1].z);
   float _299 = dot(float3(_115, _116, _117), float3(_296, _297, _298));
   float _300 = saturate(_299);
   float _305 = saturate(_130);
   float _306 = (c_light_col[0].x) * _305;
   float _307 = (c_light_col[0].y) * _305;
   float _308 = (c_light_col[0].z) * _305;
   float _313 = (c_light_col[1].x) * _300;
   float _314 = (c_light_col[1].y) * _300;
   float _315 = (c_light_col[1].z) * _300;
   float _324 = c_diffuse_scale.x * c_mat_diffuse.x;
   float _325 = c_diffuse_scale.y * c_mat_diffuse.y;
   float _326 = c_diffuse_scale.z * c_mat_diffuse.z;
   float _327 = _306 * _227;
   float _328 = _307 * _228;
   float _329 = _308 * _229;
   float _330 = _313 * _215;
   float _331 = _314 * _215;
   float _332 = _315 * _215;
   float _333 = _330 + _327;
   float _334 = _331 + _328;
   float _335 = _332 + _329;
   float _336 = _324 * _333;
   float _337 = _325 * _334;
   float _338 = _326 * _335;
   float _339 = _267 + c_scene_ambient.x;
   float _340 = _339 + _336;
   float _341 = _269 + c_scene_ambient.y;
   float _342 = _341 + _337;
   float _343 = _271 + c_scene_ambient.z;
   float _344 = _343 + _338;
   float _345 = _340 * _230;
   float _346 = _342 * _231;
   float _347 = _344 * _232;
   float _348 = _286 * _253;
   float _349 = _287 * _254;
   float _350 = _288 * _255;
   float _351 = _345 + _348;
   float _352 = _346 + _349;
   float _353 = _347 + _350;
   float _358 = (c_light_vec[0].x) + _120;
   float _359 = (c_light_vec[0].y) + _121;
   float _360 = (c_light_vec[0].z) + _122;
   float _361 = dot(float3(_358, _359, _360), float3(_358, _359, _360));
   float _362 = rsqrt(_361);
   float _363 = _358 * _362;
   float _364 = -0.0f - _363;
   float _365 = _359 * _362;
   float _366 = -0.0f - _365;
   float _367 = _360 * _362;
   float _368 = -0.0f - _367;
   float _369 = dot(float3(_364, _366, _368), float3(_115, _116, _117));
   float _370 = saturate(_369);
   float _371 = _121 + -0.30000001192092896f;
   float _372 = dot(float3(_120, _371, _122), float3(_120, _371, _122));
   float _373 = rsqrt(_372);
   float _374 = _120 * _373;
   float _375 = -0.0f - _374;
   float _376 = _371 * _373;
   float _377 = -0.0f - _376;
   float _378 = _122 * _373;
   float _379 = -0.0f - _378;
   float _380 = dot(float3(_375, _377, _379), float3(_115, _116, _117));
   float _381 = saturate(_380);
   float _384 = log2(_370);
   float _385 = log2(_381);
   float _386 = _384 * c_mat_power;
   float _387 = _385 * c_mat_power;
   float _388 = exp2(_386);
   float _389 = exp2(_387);
   float _392 = _388 * (c_light_col[0].x);
   float _393 = _388 * (c_light_col[0].y);
   float _394 = _388 * (c_light_col[0].z);
   float _395 = _389 * 2.0f;
   float _396 = _395 * (c_light_vec[5].x);
   float _397 = _396 * (c_light_col[1].x);
   float _398 = _396 * (c_light_col[1].y);
   float _399 = _396 * (c_light_col[1].z);
   float _400 = _392 * _227;
   float _401 = _393 * _228;
   float _402 = _394 * _229;
   float _403 = _397 * _300;
   float _404 = _398 * _300;
   float _405 = _399 * _300;
   float _406 = _403 + _400;
   float _407 = _404 + _401;
   float _408 = _405 + _402;
   float _415 = c_mat_specular.x * c_specular_scale.x;
   float _416 = _415 * _230;
   float _417 = _416 * _406;
   float _418 = c_mat_specular.y * c_specular_scale.y;
   float _419 = _418 * _231;
   float _420 = _419 * _407;
   float _421 = c_mat_specular.z * c_specular_scale.z;
   float _422 = _421 * _232;
   float _423 = _422 * _408;
   float _424 = _289 * _253;
   float _425 = _424 * _389;
   float _426 = _290 * _254;
   float _427 = _426 * _389;
   float _428 = _291 * _255;
   float _429 = _428 * _389;
   float _430 = _417 + _425;
   float _431 = _420 + _427;
   float _432 = _423 + _429;
   if (!_273)
   {
      float _434 = _215 * _197;
      float _435 = _215 * _198;
      float _436 = _215 * _199;
      float _437 = _351 + _434;
      float _438 = _352 + _435;
      float _439 = _353 + _436;
      float _440 = _215 * _200;
      float _441 = _430 + _440;
      float _442 = _431 + _440;
      float _443 = _432 + _440;
      _445 = _437;
      _446 = _438;
      _447 = _439;
      _448 = _441;
      _449 = _442;
      _450 = _443;
   }
   else
   {
      _445 = _351;
      _446 = _352;
      _447 = _353;
      _448 = _430;
      _449 = _431;
      _450 = _432;
   }
   int _451 = c_bool & 128;
   bool _452 = (_451 != 0);
   float _453 = select(_452, 0.0f, _448);
   float _454 = select(_452, 0.0f, _449);
   float _455 = select(_452, 0.0f, _450);
   float _456 = _445 * _56;
   float _457 = _446 * _57;
   float _458 = _447 * _58;
   float _459 = _453 * _64.x;
   float _460 = _454 * _64.y;
   float _461 = _455 * _64.z;
   int _462 = c_bool & 32768;
   bool _463 = (_462 == 0);
   if (!_463)
   {
      float _467 = (c_ps_user[11].x) * _56;
      float _468 = (c_ps_user[11].x) * _57;
      float _469 = (c_ps_user[11].x) * _58;
      float _471 = _115 + 1.0f;
      float _472 = _116 + 1.0f;
      float _473 = _117 + 1.0f;
      float _474 = _471 * 0.5f;
      float _475 = _472 * 0.5f;
      float _476 = _473 * 0.5f;
      float _477 = saturate(_474);
      float _478 = saturate(_475);
      float _479 = saturate(_476);
      float _480 = (c_ps_user[11].y) * _477;
      float _481 = _478 * (c_ps_user[11].y);
      float _482 = _479 * (c_ps_user[11].y);
      float _484 = (c_ps_user[11].z) * _64.x;
      float _485 = (c_ps_user[11].z) * _64.y;
      float _486 = (c_ps_user[11].z) * _64.z;
      float _488 = c_mat_power * 0.009999999776482582f;
      float _489 = _488 * (c_ps_user[11].w);
      float _492 = (c_ps_user[12].x) * _227;
      float _493 = (c_ps_user[12].x) * _228;
      float _494 = (c_ps_user[12].x) * _229;
      float _496 = (c_ps_user[12].y) * _230;
      float _497 = (c_ps_user[12].y) * _231;
      float _498 = (c_ps_user[12].y) * _232;
      float _500 = (c_ps_user[12].z) * _253;
      float _501 = (c_ps_user[12].z) * _254;
      float _502 = (c_ps_user[12].z) * _255;
      float _508 = -0.0f - _120;
      float _509 = -0.0f - _121;
      float _510 = -0.0f - _122;
      float _511 = dot(float3(_508, _509, _510), float3(_115, _116, _117));
      float _512 = 1.0f - _511;
      float _513 = _512 * _512;
      float _514 = _512 * 0.9722222089767456f;
      float _515 = _513 * _513;
      float _516 = _515 * _514;
      float _517 = _516 + 0.02777777425944805f;
      float _518 = _517 * (c_ps_user[13].z);
      float _520 = (c_ps_user[13].w) * c_scene_ambient.x;
      float _521 = (c_ps_user[13].w) * c_scene_ambient.y;
      float _522 = (c_ps_user[13].w) * c_scene_ambient.z;
      float _525 = (c_ps_user[14].x) * _267;
      float _526 = (c_ps_user[14].x) * _269;
      float _527 = (c_ps_user[14].x) * _271;
      float _529 = (c_ps_user[14].y) * _306;
      float _530 = (c_ps_user[14].y) * _307;
      float _531 = (c_ps_user[14].y) * _308;
      float _533 = (c_ps_user[14].z) * _313;
      float _534 = (c_ps_user[14].z) * _314;
      float _535 = (c_ps_user[14].z) * _315;
      float _537 = (c_ps_user[14].w) * _286;
      float _538 = (c_ps_user[14].w) * _287;
      float _539 = (c_ps_user[14].w) * _288;
      float _542 = (c_ps_user[15].x) * _392;
      float _543 = (c_ps_user[15].x) * _393;
      float _544 = (c_ps_user[15].x) * _394;
      float _546 = _415 * _397;
      float _547 = _546 * (c_ps_user[15].y);
      float _548 = _418 * _398;
      float _549 = _548 * (c_ps_user[15].y);
      float _550 = _421 * _399;
      float _551 = _550 * (c_ps_user[15].y);
      float _553 = (c_ps_user[15].z) * _289;
      float _554 = (c_ps_user[15].z) * _290;
      float _555 = (c_ps_user[15].z) * _291;
      float _556 = _547 + _553;
      float _557 = _556 * _389;
      float _558 = _529 + (c_ps_user[13].x);
      float _559 = _558 + _533;
      float _560 = c_diffuse_scale.x * _559;
      float _561 = _560 * c_mat_diffuse.x;
      float _562 = (c_ps_user[13].y) + _484;
      float _563 = _562 + _542;
      float _564 = c_mat_specular.x * _563;
      float _565 = _564 * c_specular_scale.x;
      float _566 = _480 + _467;
      float _567 = _566 + _489;
      float _568 = _567 + (c_ps_user[12].w);
      float _569 = _568 + _492;
      float _570 = _569 + _496;
      float _571 = _570 + _500;
      float _572 = _571 + _520;
      float _573 = _572 + _525;
      float _574 = _573 + _537;
      float _575 = _574 + _518;
      float _576 = _575 + _557;
      float _577 = _576 + _561;
      float _578 = _577 + _565;
      float _579 = _549 + _554;
      float _580 = _579 * _389;
      float _581 = _530 + (c_ps_user[13].x);
      float _582 = _581 + _534;
      float _583 = c_diffuse_scale.y * _582;
      float _584 = _583 * c_mat_diffuse.y;
      float _585 = (c_ps_user[13].y) + _485;
      float _586 = _585 + _543;
      float _587 = c_mat_specular.y * _586;
      float _588 = _587 * c_specular_scale.y;
      float _589 = _481 + _468;
      float _590 = _589 + _489;
      float _591 = _590 + (c_ps_user[12].w);
      float _592 = _591 + _493;
      float _593 = _592 + _497;
      float _594 = _593 + _501;
      float _595 = _594 + _521;
      float _596 = _595 + _526;
      float _597 = _596 + _538;
      float _598 = _597 + _518;
      float _599 = _598 + _580;
      float _600 = _599 + _584;
      float _601 = _600 + _588;
      float _602 = _551 + _555;
      float _603 = _602 * _389;
      float _604 = _531 + (c_ps_user[13].x);
      float _605 = _604 + _535;
      float _606 = c_diffuse_scale.z * _605;
      float _607 = _606 * c_mat_diffuse.z;
      float _608 = (c_ps_user[13].y) + _486;
      float _609 = _608 + _544;
      float _610 = c_mat_specular.z * _609;
      float _611 = _610 * c_specular_scale.z;
      float _612 = _489 + _469;
      float _613 = _612 + _482;
      float _614 = _613 + (c_ps_user[12].w);
      float _615 = _614 + _494;
      float _616 = _615 + _498;
      float _617 = _616 + _502;
      float _618 = _617 + _522;
      float _619 = _618 + _527;
      float _620 = _619 + _539;
      float _621 = _620 + _518;
      float _622 = _621 + _603;
      float _623 = _622 + _607;
      float _624 = _623 + _611;
      _626 = _578;
      _627 = _601;
      _628 = _624;
      _629 = 0.0f;
      _630 = 0.0f;
      _631 = 0.0f;
   }
   else
   {
      _626 = _456;
      _627 = _457;
      _628 = _458;
      _629 = _459;
      _630 = _460;
      _631 = _461;
   }
   float _632 = _76.x * _76.x;
   float _633 = _632 * COLOR_1.w;
   float _634 = _76.y * _76.y;
   float _635 = _634 * COLOR_1.w;
   float _636 = _76.z * _76.z;
   float _637 = _636 * COLOR_1.w;
   float _638 = _626 + _633;
   float _639 = _638 + _629;
   float _640 = _627 + _635;
   float _641 = _640 + _630;
   float _642 = _628 + _637;
   float _643 = _642 + _631;
   float _648 = _51.w * COLOR.w;
   float _649 = _648 * c_mat_opacity;
   float _650 = _649 * c_global_opacity;
   float _651 = _650 * c_opacity_scale;
   int _652 = c_bool & 16;
   bool _653 = (_652 == 0);
   if (!_653)
   {
      float _657 = c_tone_control.x * _639;
      float _658 = c_tone_control.x * _641;
      float _659 = c_tone_control.x * _643;
      float _661 = log2(_657);
      float _662 = log2(_658);
      float _663 = log2(_659);
      float _664 = _661 * c_tone_control.y;
      float _665 = _662 * c_tone_control.y;
      float _666 = _663 * c_tone_control.y;
      float _667 = exp2(_664);
      float _668 = exp2(_665);
      float _669 = exp2(_666);
      _717 = _667;
      _718 = _668;
      _719 = _669;
   }
   else
   {
      if (_463)
      {
         float _674 = c_tone_control.x * _639;
         float _675 = c_tone_control.x * _641;
         float _676 = c_tone_control.x * _643;
         float _677 = _674 * 2.0f;
         float _678 = _675 * 2.0f;
         float _679 = _676 * 2.0f;
         float _680 = _674 * 0.30000001192092896f;
         float _681 = _675 * 0.30000001192092896f;
         float _682 = _676 * 0.30000001192092896f;
         float _683 = _680 + 0.05000000074505806f;
         float _684 = _681 + 0.05000000074505806f;
         float _685 = _682 + 0.05000000074505806f;
         float _686 = _683 * _677;
         float _687 = _684 * _678;
         float _688 = _685 * _679;
         float _689 = _686 + 0.004000000189989805f;
         float _690 = _687 + 0.004000000189989805f;
         float _691 = _688 + 0.004000000189989805f;
         float _692 = _680 + 0.5f;
         float _693 = _681 + 0.5f;
         float _694 = _682 + 0.5f;
         float _695 = _692 * _677;
         float _696 = _693 * _678;
         float _697 = _694 * _679;
         float _698 = _695 + 0.06000000238418579f;
         float _699 = _696 + 0.06000000238418579f;
         float _700 = _697 + 0.06000000238418579f;
         float _701 = _689 / _698;
         float _702 = _690 / _699;
         float _703 = _691 / _700;
         float _704 = _701 + -0.06666666269302368f;
         float _705 = _702 + -0.06666666269302368f;
         float _706 = _703 + -0.06666666269302368f;
         float _707 = _704 * 1.3790643215179443f;
         float _708 = _705 * 1.3790643215179443f;
         float _709 = _706 * 1.3790643215179443f;
         float _710 = YKR_MaterialCurve(_707, _674);
         float _711 = YKR_MaterialCurve(_708, _675);
         float _712 = YKR_MaterialCurve(_709, _676);
         float _713 = sqrt(_710);
         float _714 = sqrt(_711);
         float _715 = sqrt(_712);
         _717 = _713;
         _718 = _714;
         _719 = _715;
      }
      else
      {
         _717 = _639;
         _718 = _641;
         _719 = _643;
      }
   }
   int _720 = c_bool & 4;
   bool _721 = (_720 == 0);
   if (_721)
   {
      float _723 = saturate(COLOR_1.x);
      float _728 = (c_fog_param[0].x) - _717;
      float _729 = (c_fog_param[0].y) - _718;
      float _730 = (c_fog_param[0].z) - _719;
      float _731 = _728 * _723;
      float _732 = _729 * _723;
      float _733 = _730 * _723;
      _735 = _731;
      _736 = _732;
      _737 = _733;
   }
   else
   {
      _735 = COLOR_1.x;
      _736 = COLOR_1.y;
      _737 = COLOR_1.z;
   }
   float _738 = _735 + _717;
   float _739 = _736 + _718;
   float _740 = _737 + _719;
   float _746 = (c_ps_col_scl[0].x) * _738;
   float _747 = (c_ps_col_scl[0].y) * _739;
   float _748 = (c_ps_col_scl[0].z) * _740;
   float _749 = (c_ps_col_scl[0].w) * _651;
   SV_Target.x = _746;
   SV_Target.y = _747;
   SV_Target.z = _748;
   SV_Target.w = _749;
   int _752 = c_bool & 8;
   bool _753 = (_752 != 0);
   float _754 = select(_753, _738, _746);
   float _755 = select(_753, _739, _747);
   float _756 = select(_753, _740, _748);
   float _757 = select(_753, _651, _749);
   SV_Target.x = _754;
   SV_Target.y = _755;
   SV_Target.z = _756;
   SV_Target.w = _757;
   return SV_Target;
}
