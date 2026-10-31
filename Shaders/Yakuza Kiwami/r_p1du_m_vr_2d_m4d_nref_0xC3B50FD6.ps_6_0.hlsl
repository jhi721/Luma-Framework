// Yakuza Kiwami material r_p1du_m(vr)2d_m4d[nref] (0xC3B50FD6, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
// applied by a script: the inline Hable tonemap's saturate goes through
// YKR_MaterialCurve (tangent continuation in HDR), and the light pre-pass buffer t14 through YKR_LightBuffer.

#include "Includes/Common.hlsl"

Texture2D<float4> s0_t : register(t0);

Texture2D<float4> s1_t : register(t1);

Texture2D<float4> s3_t : register(t3);

TextureCube<float4> s6c_t : register(t6);

Texture2D<float4> s7_t : register(t7);

Texture3D<float4> s12v_t : register(t12);

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

SamplerState s6c_s : register(s6);

SamplerState s7_s : register(s7);

SamplerState s12v_s : register(s12);

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
    linear float3 TEXCOORD_6 : TEXCOORD6,
    noperspective float4 SV_Position : SV_Position) : SV_Target
{
   float4 SV_Target = 0;
   float4 _56 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _61 = _56.x * _56.x;
   float _62 = _56.y * _56.y;
   float _63 = _56.z * _56.z;
   float _67 = (c_attribute_ps[0].x) * TEXCOORD.x;
   float _68 = (c_attribute_ps[0].y) * TEXCOORD.y;
   float4 _69 = s3_t.Sample(s3_s, float2(_67, _68));
   float4 _72 = s1_t.Sample(s1_s, float2(TEXCOORD.z, TEXCOORD.w));
   float _76 = _72.x * _72.x;
   float _77 = _72.y * _72.y;
   float _78 = _72.z * _72.z;
   float4 _79 = s7_t.Sample(s7_s, float2(TEXCOORD_1.x, TEXCOORD_1.y));
   float _82 = 1.0f - _76;
   float _83 = 1.0f - _77;
   float _84 = 1.0f - _78;
   float _85 = _82 * COLOR_1.w;
   float _86 = _83 * COLOR_1.w;
   float _87 = _84 * COLOR_1.w;
   float _88 = 1.0f - _85;
   float _89 = 1.0f - _86;
   float _90 = 1.0f - _87;
   float _91 = _61 * _88;
   float _92 = _62 * _89;
   float _93 = _63 * _90;
   float _94 = dot(float3(TEXCOORD_2.x, TEXCOORD_2.y, TEXCOORD_2.z), float3(TEXCOORD_2.x, TEXCOORD_2.y, TEXCOORD_2.z));
   float _95 = rsqrt(_94);
   float _96 = _95 * TEXCOORD_2.x;
   float _97 = _95 * TEXCOORD_2.y;
   float _98 = _95 * TEXCOORD_2.z;
   float _99 = dot(float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z), float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z));
   float _100 = rsqrt(_99);
   float _101 = _100 * TEXCOORD_3.x;
   float _102 = _100 * TEXCOORD_3.y;
   float _103 = _100 * TEXCOORD_3.z;
   float _108 = -0.0f - (c_light_vec[0].x);
   float _109 = -0.0f - (c_light_vec[0].y);
   float _110 = -0.0f - (c_light_vec[0].z);
   float _111 = dot(float3(_96, _97, _98), float3(_108, _109, _110));
   float _115 = c_rt_size.z * SV_Position.x;
   float _116 = c_rt_size.w * SV_Position.y;
   int _119 = c_bool & 8;
   bool _120 = (_119 == 0);
   float _127;
   float _128;
   float _129;
   float _141;
   float _142;
   float _197;
   float _198;
   float _199;
   float _200;
   float _214;
   float _215;
   float _261;
   float _262;
   float _263;
   float _298;
   float _299;
   float _300;
   float _301;
   float _302;
   float _303;
   float _457;
   float _458;
   float _459;
   float _460;
   float _461;
   float _462;
   float _634;
   float _635;
   float _636;
   float _637;
   float _638;
   float _639;
   float _795;
   float _796;
   float _797;
   float _813;
   float _814;
   float _815;
   if (!_120)
   {
      float4 _122 = s13_t.Sample(s13_s, float2(_115, _116));
      _127 = _122.x;
      _128 = _122.y;
      _129 = _122.w;
   }
   else
   {
      _127 = 1.0f;
      _128 = 1.0f;
      _129 = 1.0f;
   }
   if (!_120)
   {
      float _134 = (c_sdw_param[0].x) * _127;
      float _135 = (c_sdw_param[0].z) * _128;
      float _138 = _134 + (c_sdw_param[0].y);
      float _139 = _135 + (c_sdw_param[0].w);
      _141 = _138;
      _142 = _139;
   }
   else
   {
      _141 = _127;
      _142 = _128;
   }
   float _149 = (c_sdw_param[1].x) * _79.y;
   float _151 = _149 + (c_sdw_param[1].y);
   float _152 = _151 * _79.x;
   float _153 = 1.0f - (c_light_col[5].x);
   float _154 = 1.0f - (c_light_col[5].y);
   float _155 = 1.0f - (c_light_col[5].z);
   float _156 = _152 * _153;
   float _157 = _152 * _154;
   float _158 = _152 * _155;
   float _159 = _156 + (c_light_col[5].x);
   float _160 = _157 + (c_light_col[5].y);
   float _161 = _158 + (c_light_col[5].z);
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
      float _207 = (c_sdw_param[0].x) * _141;
      float _208 = (c_sdw_param[0].z) * _142;
      float _211 = _207 + (c_sdw_param[0].y);
      float _212 = _208 + (c_sdw_param[0].w);
      _214 = _211;
      _215 = _212;
   }
   else
   {
      _214 = _141;
      _215 = _142;
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
   float _230 = _129 * COLOR.x;
   float _231 = _230 * _159;
   float _232 = _129 * COLOR.y;
   float _233 = _232 * _160;
   float _234 = _129 * COLOR.z;
   float _235 = _234 * _161;
   float _236 = _227 * _159;
   float _237 = _228 * _160;
   float _238 = _229 * _161;
   if (!_202)
   {
      float _242 = (c_sdw_param[2].x) * _227;
      float _243 = (c_sdw_param[2].x) * _228;
      float _244 = (c_sdw_param[2].x) * _229;
      float _246 = _242 + (c_sdw_param[2].y);
      float _247 = _243 + (c_sdw_param[2].y);
      float _248 = _244 + (c_sdw_param[2].y);
      float _250 = (c_sdw_param[2].z) * _159;
      float _251 = (c_sdw_param[2].z) * _160;
      float _252 = (c_sdw_param[2].z) * _161;
      float _254 = _250 + (c_sdw_param[2].w);
      float _255 = _251 + (c_sdw_param[2].w);
      float _256 = _252 + (c_sdw_param[2].w);
      float _257 = _254 * _246;
      float _258 = _255 * _247;
      float _259 = _256 * _248;
      _261 = _257;
      _262 = _258;
      _263 = _259;
   }
   else
   {
      _261 = _236;
      _262 = _237;
      _263 = _238;
   }
   float _264 = _129 * _79.x;
   float _265 = _261 * _264;
   float _266 = _262 * _264;
   float _267 = _263 * _264;
   float _278 = c_mat_emissive.x * _91;
   float _279 = _278 * c_emissive_scale;
   float _280 = c_mat_emissive.y * _92;
   float _281 = _280 * c_emissive_scale;
   float _282 = c_mat_emissive.z * _93;
   float _283 = _282 * c_emissive_scale;
   int _284 = c_bool & 2048;
   bool _285 = (_284 == 0);
   if (_285)
   {
      float _287 = _215 * _197;
      float _288 = _215 * _198;
      float _289 = _215 * _199;
      float _290 = _287 + TEXCOORD_4.x;
      float _291 = _288 + TEXCOORD_4.y;
      float _292 = _289 + TEXCOORD_4.z;
      float _293 = _215 * _200;
      float _294 = _293 + TEXCOORD_5.x;
      float _295 = _293 + TEXCOORD_5.y;
      float _296 = _293 + TEXCOORD_5.z;
      _298 = _294;
      _299 = _295;
      _300 = _296;
      _301 = _290;
      _302 = _291;
      _303 = _292;
   }
   else
   {
      _298 = TEXCOORD_5.x;
      _299 = TEXCOORD_5.y;
      _300 = TEXCOORD_5.z;
      _301 = TEXCOORD_4.x;
      _302 = TEXCOORD_4.y;
      _303 = TEXCOORD_4.z;
   }
   float _308 = -0.0f - (c_light_vec[1].x);
   float _309 = -0.0f - (c_light_vec[1].y);
   float _310 = -0.0f - (c_light_vec[1].z);
   float _311 = dot(float3(_96, _97, _98), float3(_308, _309, _310));
   float _312 = saturate(_311);
   float _317 = saturate(_111);
   float _318 = (c_light_col[0].x) * _317;
   float _319 = (c_light_col[0].y) * _317;
   float _320 = (c_light_col[0].z) * _317;
   float _325 = (c_light_col[1].x) * _312;
   float _326 = (c_light_col[1].y) * _312;
   float _327 = (c_light_col[1].z) * _312;
   float _336 = c_diffuse_scale.x * c_mat_diffuse.x;
   float _337 = c_diffuse_scale.y * c_mat_diffuse.y;
   float _338 = c_diffuse_scale.z * c_mat_diffuse.z;
   float _339 = _318 * _227;
   float _340 = _319 * _228;
   float _341 = _320 * _229;
   float _342 = _325 * _215;
   float _343 = _326 * _215;
   float _344 = _327 * _215;
   float _345 = _342 + _339;
   float _346 = _343 + _340;
   float _347 = _344 + _341;
   float _348 = _336 * _345;
   float _349 = _337 * _346;
   float _350 = _338 * _347;
   float _351 = _279 + c_scene_ambient.x;
   float _352 = _351 + _348;
   float _353 = _281 + c_scene_ambient.y;
   float _354 = _353 + _349;
   float _355 = _283 + c_scene_ambient.z;
   float _356 = _355 + _350;
   float _357 = _352 * _231;
   float _358 = _354 * _233;
   float _359 = _356 * _235;
   float _360 = _301 * _265;
   float _361 = _302 * _266;
   float _362 = _303 * _267;
   float _363 = _357 + _360;
   float _364 = _358 + _361;
   float _365 = _359 + _362;
   float _370 = (c_light_vec[0].x) + _101;
   float _371 = (c_light_vec[0].y) + _102;
   float _372 = (c_light_vec[0].z) + _103;
   float _373 = dot(float3(_370, _371, _372), float3(_370, _371, _372));
   float _374 = rsqrt(_373);
   float _375 = _370 * _374;
   float _376 = -0.0f - _375;
   float _377 = _371 * _374;
   float _378 = -0.0f - _377;
   float _379 = _372 * _374;
   float _380 = -0.0f - _379;
   float _381 = dot(float3(_376, _378, _380), float3(_96, _97, _98));
   float _382 = saturate(_381);
   float _383 = _102 + -0.30000001192092896f;
   float _384 = dot(float3(_101, _383, _103), float3(_101, _383, _103));
   float _385 = rsqrt(_384);
   float _386 = _101 * _385;
   float _387 = -0.0f - _386;
   float _388 = _383 * _385;
   float _389 = -0.0f - _388;
   float _390 = _103 * _385;
   float _391 = -0.0f - _390;
   float _392 = dot(float3(_387, _389, _391), float3(_96, _97, _98));
   float _393 = saturate(_392);
   float _396 = log2(_382);
   float _397 = log2(_393);
   float _398 = _396 * c_mat_power;
   float _399 = _397 * c_mat_power;
   float _400 = exp2(_398);
   float _401 = exp2(_399);
   float _404 = _400 * (c_light_col[0].x);
   float _405 = _400 * (c_light_col[0].y);
   float _406 = _400 * (c_light_col[0].z);
   float _407 = _401 * 2.0f;
   float _408 = _407 * (c_light_vec[5].x);
   float _409 = _408 * (c_light_col[1].x);
   float _410 = _408 * (c_light_col[1].y);
   float _411 = _408 * (c_light_col[1].z);
   float _412 = _404 * _227;
   float _413 = _405 * _228;
   float _414 = _406 * _229;
   float _415 = _409 * _312;
   float _416 = _410 * _312;
   float _417 = _411 * _312;
   float _418 = _415 + _412;
   float _419 = _416 + _413;
   float _420 = _417 + _414;
   float _427 = c_mat_specular.x * c_specular_scale.x;
   float _428 = _427 * _231;
   float _429 = _428 * _418;
   float _430 = c_mat_specular.y * c_specular_scale.y;
   float _431 = _430 * _233;
   float _432 = _431 * _419;
   float _433 = c_mat_specular.z * c_specular_scale.z;
   float _434 = _433 * _235;
   float _435 = _434 * _420;
   float _436 = _298 * _265;
   float _437 = _436 * _401;
   float _438 = _299 * _266;
   float _439 = _438 * _401;
   float _440 = _300 * _267;
   float _441 = _440 * _401;
   float _442 = _429 + _437;
   float _443 = _432 + _439;
   float _444 = _435 + _441;
   if (!_285)
   {
      float _446 = _215 * _197;
      float _447 = _215 * _198;
      float _448 = _215 * _199;
      float _449 = _363 + _446;
      float _450 = _364 + _447;
      float _451 = _365 + _448;
      float _452 = _215 * _200;
      float _453 = _442 + _452;
      float _454 = _443 + _452;
      float _455 = _444 + _452;
      _457 = _449;
      _458 = _450;
      _459 = _451;
      _460 = _453;
      _461 = _454;
      _462 = _455;
   }
   else
   {
      _457 = _363;
      _458 = _364;
      _459 = _365;
      _460 = _442;
      _461 = _443;
      _462 = _444;
   }
   int _463 = c_bool & 128;
   bool _464 = (_463 != 0);
   float _465 = select(_464, 0.0f, _460);
   float _466 = select(_464, 0.0f, _461);
   float _467 = select(_464, 0.0f, _462);
   float _468 = _457 * _91;
   float _469 = _458 * _92;
   float _470 = _459 * _93;
   float _471 = _465 * _69.y;
   float _472 = _466 * _69.y;
   float _473 = _467 * _69.y;
   int _474 = c_bool & 32768;
   bool _475 = (_474 == 0);
   if (!_475)
   {
      float _479 = (c_ps_user[11].x) * _91;
      float _480 = (c_ps_user[11].x) * _92;
      float _481 = (c_ps_user[11].x) * _93;
      float _483 = _96 + 1.0f;
      float _484 = _97 + 1.0f;
      float _485 = _98 + 1.0f;
      float _486 = _483 * 0.5f;
      float _487 = _484 * 0.5f;
      float _488 = _485 * 0.5f;
      float _489 = saturate(_486);
      float _490 = saturate(_487);
      float _491 = saturate(_488);
      float _492 = (c_ps_user[11].y) * _489;
      float _493 = _490 * (c_ps_user[11].y);
      float _494 = _491 * (c_ps_user[11].y);
      float _496 = (c_ps_user[11].z) * _69.y;
      float _498 = c_mat_power * 0.009999999776482582f;
      float _499 = _498 * (c_ps_user[11].w);
      float _502 = (c_ps_user[12].x) * _227;
      float _503 = (c_ps_user[12].x) * _228;
      float _504 = (c_ps_user[12].x) * _229;
      float _506 = (c_ps_user[12].y) * _231;
      float _507 = (c_ps_user[12].y) * _233;
      float _508 = (c_ps_user[12].y) * _235;
      float _510 = (c_ps_user[12].z) * _265;
      float _511 = (c_ps_user[12].z) * _266;
      float _512 = (c_ps_user[12].z) * _267;
      float _518 = -0.0f - _101;
      float _519 = -0.0f - _102;
      float _520 = -0.0f - _103;
      float _521 = dot(float3(_518, _519, _520), float3(_96, _97, _98));
      float _522 = 1.0f - _521;
      float _523 = _522 * _522;
      float _524 = _522 * 0.9722222089767456f;
      float _525 = _523 * _523;
      float _526 = _525 * _524;
      float _527 = _526 + 0.02777777425944805f;
      float _528 = _527 * (c_ps_user[13].z);
      float _530 = (c_ps_user[13].w) * c_scene_ambient.x;
      float _531 = (c_ps_user[13].w) * c_scene_ambient.y;
      float _532 = (c_ps_user[13].w) * c_scene_ambient.z;
      float _535 = (c_ps_user[14].x) * _279;
      float _536 = (c_ps_user[14].x) * _281;
      float _537 = (c_ps_user[14].x) * _283;
      float _539 = (c_ps_user[14].y) * _318;
      float _540 = (c_ps_user[14].y) * _319;
      float _541 = (c_ps_user[14].y) * _320;
      float _543 = (c_ps_user[14].z) * _325;
      float _544 = (c_ps_user[14].z) * _326;
      float _545 = (c_ps_user[14].z) * _327;
      float _547 = (c_ps_user[14].w) * _301;
      float _548 = (c_ps_user[14].w) * _302;
      float _549 = (c_ps_user[14].w) * _303;
      float _552 = (c_ps_user[15].x) * _404;
      float _553 = (c_ps_user[15].x) * _405;
      float _554 = (c_ps_user[15].x) * _406;
      float _556 = _427 * _409;
      float _557 = _556 * (c_ps_user[15].y);
      float _558 = _430 * _410;
      float _559 = _558 * (c_ps_user[15].y);
      float _560 = _433 * _411;
      float _561 = _560 * (c_ps_user[15].y);
      float _563 = (c_ps_user[15].z) * _298;
      float _564 = (c_ps_user[15].z) * _299;
      float _565 = (c_ps_user[15].z) * _300;
      float _566 = _557 + _563;
      float _567 = _566 * _401;
      float _568 = _539 + (c_ps_user[13].x);
      float _569 = _568 + _543;
      float _570 = c_diffuse_scale.x * _569;
      float _571 = _570 * c_mat_diffuse.x;
      float _572 = (c_ps_user[13].y) + _496;
      float _573 = _572 + _552;
      float _574 = c_mat_specular.x * _573;
      float _575 = _574 * c_specular_scale.x;
      float _576 = _492 + _479;
      float _577 = _576 + _499;
      float _578 = _577 + (c_ps_user[12].w);
      float _579 = _578 + _502;
      float _580 = _579 + _506;
      float _581 = _580 + _510;
      float _582 = _581 + _530;
      float _583 = _582 + _535;
      float _584 = _583 + _547;
      float _585 = _584 + _528;
      float _586 = _585 + _567;
      float _587 = _586 + _571;
      float _588 = _587 + _575;
      float _589 = _559 + _564;
      float _590 = _589 * _401;
      float _591 = _540 + (c_ps_user[13].x);
      float _592 = _591 + _544;
      float _593 = c_diffuse_scale.y * _592;
      float _594 = _593 * c_mat_diffuse.y;
      float _595 = _572 + _553;
      float _596 = c_mat_specular.y * _595;
      float _597 = _596 * c_specular_scale.y;
      float _598 = _493 + _480;
      float _599 = _598 + _499;
      float _600 = _599 + (c_ps_user[12].w);
      float _601 = _600 + _503;
      float _602 = _601 + _507;
      float _603 = _602 + _511;
      float _604 = _603 + _531;
      float _605 = _604 + _536;
      float _606 = _605 + _548;
      float _607 = _606 + _528;
      float _608 = _607 + _590;
      float _609 = _608 + _594;
      float _610 = _609 + _597;
      float _611 = _561 + _565;
      float _612 = _611 * _401;
      float _613 = _541 + (c_ps_user[13].x);
      float _614 = _613 + _545;
      float _615 = c_diffuse_scale.z * _614;
      float _616 = _615 * c_mat_diffuse.z;
      float _617 = _572 + _554;
      float _618 = c_mat_specular.z * _617;
      float _619 = _618 * c_specular_scale.z;
      float _620 = _499 + _481;
      float _621 = _620 + _494;
      float _622 = _621 + (c_ps_user[12].w);
      float _623 = _622 + _504;
      float _624 = _623 + _508;
      float _625 = _624 + _512;
      float _626 = _625 + _532;
      float _627 = _626 + _537;
      float _628 = _627 + _549;
      float _629 = _628 + _528;
      float _630 = _629 + _612;
      float _631 = _630 + _616;
      float _632 = _631 + _619;
      _634 = 0.0f;
      _635 = 0.0f;
      _636 = 0.0f;
      _637 = _588;
      _638 = _610;
      _639 = _632;
   }
   else
   {
      _634 = _471;
      _635 = _472;
      _636 = _473;
      _637 = _468;
      _638 = _469;
      _639 = _470;
   }
   float _640 = _637 + _634;
   float _641 = _638 + _635;
   float _642 = _639 + _636;
   float _646 = (c_attribute_ps[2].x) * TEXCOORD_6.x;
   float _647 = (c_attribute_ps[2].x) * TEXCOORD_6.y;
   float _648 = (c_attribute_ps[2].x) * TEXCOORD_6.z;
   float4 _649 = s12v_t.Sample(s12v_s, float3(_646, _647, _648));
   float _654 = _649.x * 2.0f;
   float _655 = _649.y * 2.0f;
   float _656 = _649.z * 2.0f;
   float _657 = _649.w * 2.0f;
   float _658 = _654 + -1.0f;
   float _659 = _655 + -1.0f;
   float _660 = _656 + -1.0f;
   float _661 = _657 + -1.0f;
   float _662 = _658 - _661;
   float _663 = _659 - _661;
   float _664 = _660 - _661;
   float _665 = _662 * (c_attribute_ps[2].y);
   float _666 = _663 * (c_attribute_ps[2].y);
   float _667 = _664 * (c_attribute_ps[2].y);
   float _668 = _665 + _96;
   float _669 = _666 + _97;
   float _670 = _667 + _98;
   float _671 = dot(float3(_668, _669, _670), float3(_668, _669, _670));
   float _672 = rsqrt(_671);
   float _673 = _668 * _672;
   float _674 = _669 * _672;
   float _675 = _670 * _672;
   float _676 = dot(float3(_101, _102, _103), float3(_673, _674, _675));
   float _677 = _676 * 2.0f;
   float _678 = _673 * _677;
   float _679 = _674 * _677;
   float _680 = _675 * _677;
   float _681 = _101 - _678;
   float _682 = _102 - _679;
   float _683 = _103 - _680;
   float _684 = -0.0f - _681;
   float4 _685 = s6c_t.Sample(s6c_s, float3(_684, _682, _683));
   float _692 = _142 * _141;
   float _693 = _111 + 0.75f;
   float _694 = saturate(_693);
   float _695 = _692 * _694;
   float _696 = max(_695, (c_light_vec[5].y));
   float _697 = max(_685.w, 0.003921568859368563f);
   float _698 = 0.25f / _697;
   float _699 = _698 * _685.x;
   float _700 = _698 * _685.y;
   float _701 = _698 * _685.z;
   float _702 = _69.x * TEXCOORD_2.w;
   float _703 = _702 * _79.x;
   float _704 = _696 * _703;
   float _705 = _704 * _159;
   float _706 = _699 * _699;
   float _707 = _706 * _705;
   float _708 = _704 * _160;
   float _709 = _700 * _700;
   float _710 = _709 * _708;
   float _711 = _704 * _161;
   float _712 = _701 * _701;
   float _713 = _712 * _711;
   float _714 = _640 + _707;
   float _715 = _641 + _710;
   float _716 = _642 + _713;
   float _723 = c_mat_opacity * _56.w;
   float _724 = _723 * c_opacity_scale;
   float _725 = _724 * c_global_opacity;
   int _728 = c_bool & 16;
   bool _729 = (_728 == 0);
   if (!_729)
   {
      float _733 = c_tone_control.x * _714;
      float _734 = c_tone_control.x * _715;
      float _735 = c_tone_control.x * _716;
      float _737 = log2(_733);
      float _738 = log2(_734);
      float _739 = log2(_735);
      float _740 = _737 * c_tone_control.y;
      float _741 = _738 * c_tone_control.y;
      float _742 = _739 * c_tone_control.y;
      float _743 = exp2(_740);
      float _744 = exp2(_741);
      float _745 = exp2(_742);
      _795 = _743;
      _796 = _744;
      _797 = _745;
   }
   else
   {
      int _747 = c_bool & 32768;
      bool _748 = (_747 == 0);
      if (_748)
      {
         float _752 = c_tone_control.x * _714;
         float _753 = c_tone_control.x * _715;
         float _754 = c_tone_control.x * _716;
         float _755 = _752 * 2.0f;
         float _756 = _753 * 2.0f;
         float _757 = _754 * 2.0f;
         float _758 = _752 * 0.30000001192092896f;
         float _759 = _753 * 0.30000001192092896f;
         float _760 = _754 * 0.30000001192092896f;
         float _761 = _758 + 0.05000000074505806f;
         float _762 = _759 + 0.05000000074505806f;
         float _763 = _760 + 0.05000000074505806f;
         float _764 = _761 * _755;
         float _765 = _762 * _756;
         float _766 = _763 * _757;
         float _767 = _764 + 0.004000000189989805f;
         float _768 = _765 + 0.004000000189989805f;
         float _769 = _766 + 0.004000000189989805f;
         float _770 = _758 + 0.5f;
         float _771 = _759 + 0.5f;
         float _772 = _760 + 0.5f;
         float _773 = _770 * _755;
         float _774 = _771 * _756;
         float _775 = _772 * _757;
         float _776 = _773 + 0.06000000238418579f;
         float _777 = _774 + 0.06000000238418579f;
         float _778 = _775 + 0.06000000238418579f;
         float _779 = _767 / _776;
         float _780 = _768 / _777;
         float _781 = _769 / _778;
         float _782 = _779 + -0.06666666269302368f;
         float _783 = _780 + -0.06666666269302368f;
         float _784 = _781 + -0.06666666269302368f;
         float _785 = _782 * 1.3790643215179443f;
         float _786 = _783 * 1.3790643215179443f;
         float _787 = _784 * 1.3790643215179443f;
         float _788 = YKR_MaterialCurve(_785, _752);
         float _789 = YKR_MaterialCurve(_786, _753);
         float _790 = YKR_MaterialCurve(_787, _754);
         float _791 = sqrt(_788);
         float _792 = sqrt(_789);
         float _793 = sqrt(_790);
         _795 = _791;
         _796 = _792;
         _797 = _793;
      }
      else
      {
         _795 = _714;
         _796 = _715;
         _797 = _716;
      }
   }
   int _798 = c_bool & 4;
   bool _799 = (_798 == 0);
   if (_799)
   {
      float _801 = saturate(COLOR_1.x);
      float _806 = (c_fog_param[0].x) - _795;
      float _807 = (c_fog_param[0].y) - _796;
      float _808 = (c_fog_param[0].z) - _797;
      float _809 = _806 * _801;
      float _810 = _807 * _801;
      float _811 = _808 * _801;
      _813 = _809;
      _814 = _810;
      _815 = _811;
   }
   else
   {
      _813 = COLOR_1.x;
      _814 = COLOR_1.y;
      _815 = COLOR_1.z;
   }
   float _816 = _813 + _795;
   float _817 = _814 + _796;
   float _818 = _815 + _797;
   float _821 = _725 - c_mat_punchthrough_ref;
   bool _822 = (_821 < 0.0f);
   if (_822)
      discard;
   float _828 = (c_ps_col_scl[0].x) * _816;
   float _829 = (c_ps_col_scl[0].y) * _817;
   float _830 = (c_ps_col_scl[0].z) * _818;
   float _831 = (c_ps_col_scl[0].w) * _725;
   SV_Target.x = _828;
   SV_Target.y = _829;
   SV_Target.z = _830;
   SV_Target.w = _831;
   int _834 = c_bool & 8;
   bool _835 = (_834 != 0);
   float _836 = select(_835, _816, _828);
   float _837 = select(_835, _817, _829);
   float _838 = select(_835, _818, _830);
   float _839 = select(_835, _725, _831);
   SV_Target.x = _836;
   SV_Target.y = _837;
   SV_Target.z = _838;
   SV_Target.w = _839;
   return SV_Target;
}
