// Yakuza Kiwami material r_o1d2s3t_m4d (0x8C96AF0D, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
// applied by a script: the inline Hable tonemap's saturate goes through
// YKR_MaterialCurve (tangent continuation in HDR), and the light pre-pass buffer t14 through YKR_LightBuffer.

#include "Includes/Common.hlsl"

Texture2D<float4> s0_t : register(t0);

Texture2D<float4> s3_t : register(t3);

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

cbuffer cb3 : register(b3)
{
   float4 c_light_col[6] : packoffset(c000.x);
   float4 c_light_vec[6] : packoffset(c006.x);
};

SamplerState s0_s : register(s0);

SamplerState s3_s : register(s3);

SamplerState s5_s : register(s5);

SamplerState s7_s : register(s7);

SamplerState s13_s : register(s13);

SamplerState s14_s : register(s14);

float4 main(
    linear float4 TEXCOORD : TEXCOORD,
    linear float4 TEXCOORD_1 : TEXCOORD1,
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
   float4 _53 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _58 = _53.x * _53.x;
   float _59 = _53.y * _53.y;
   float _60 = _53.z * _53.z;
   float4 _61 = s3_t.Sample(s3_s, float2(TEXCOORD.z, TEXCOORD.w));
   float4 _65 = s5_t.Sample(s5_s, float2(TEXCOORD_1.x, TEXCOORD_1.y));
   float4 _68 = s7_t.Sample(s7_s, float2(TEXCOORD_1.z, TEXCOORD_1.w));
   float _71 = _65.x * 2.0f;
   float _72 = _65.y * 2.0f;
   float _73 = _71 + -1.0f;
   float _74 = _72 + -1.0f;
   float _75 = dot(float2(_73, _74), float2(_73, _74));
   float _76 = 1.0f - _75;
   float _77 = saturate(_76);
   float _78 = sqrt(_77);
   float _79 = TEXCOORD_2.y * TEXCOORD_6.z;
   float _80 = TEXCOORD_2.z * TEXCOORD_6.y;
   float _81 = _79 - _80;
   float _82 = TEXCOORD_2.z * TEXCOORD_6.x;
   float _83 = TEXCOORD_2.x * TEXCOORD_6.z;
   float _84 = _82 - _83;
   float _85 = TEXCOORD_2.x * TEXCOORD_6.y;
   float _86 = TEXCOORD_2.y * TEXCOORD_6.x;
   float _87 = _85 - _86;
   float _88 = _73 * TEXCOORD_6.x;
   float _89 = _73 * TEXCOORD_6.y;
   float _90 = _73 * TEXCOORD_6.z;
   float _91 = _74 * TEXCOORD_6.w;
   float _92 = _91 * _81;
   float _93 = _91 * _84;
   float _94 = _91 * _87;
   float _95 = _92 + _88;
   float _96 = _93 + _89;
   float _97 = _94 + _90;
   float _98 = _78 * TEXCOORD_2.x;
   float _99 = _78 * TEXCOORD_2.y;
   float _100 = _78 * TEXCOORD_2.z;
   float _101 = _95 + _98;
   float _102 = _96 + _99;
   float _103 = _97 + _100;
   float _104 = dot(float3(_101, _102, _103), float3(_101, _102, _103));
   float _105 = rsqrt(_104);
   float _106 = _101 * _105;
   float _107 = _102 * _105;
   float _108 = _103 * _105;
   float _109 = dot(float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z), float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z));
   float _110 = rsqrt(_109);
   float _111 = _110 * TEXCOORD_3.x;
   float _112 = _110 * TEXCOORD_3.y;
   float _113 = _110 * TEXCOORD_3.z;
   float _118 = -0.0f - (c_light_vec[0].x);
   float _119 = -0.0f - (c_light_vec[0].y);
   float _120 = -0.0f - (c_light_vec[0].z);
   float _121 = dot(float3(_106, _107, _108), float3(_118, _119, _120));
   float _125 = c_rt_size.z * SV_Position.x;
   float _126 = c_rt_size.w * SV_Position.y;
   int _129 = c_bool & 8;
   bool _130 = (_129 == 0);
   float _137;
   float _138;
   float _139;
   float _151;
   float _152;
   float _207;
   float _208;
   float _209;
   float _210;
   float _224;
   float _225;
   float _271;
   float _272;
   float _273;
   float _308;
   float _309;
   float _310;
   float _311;
   float _312;
   float _313;
   float _467;
   float _468;
   float _469;
   float _470;
   float _471;
   float _472;
   float _648;
   float _649;
   float _650;
   float _651;
   float _652;
   float _653;
   float _729;
   float _730;
   float _731;
   float _747;
   float _748;
   float _749;
   if (!_130)
   {
      float4 _132 = s13_t.Sample(s13_s, float2(_125, _126));
      _137 = _132.x;
      _138 = _132.y;
      _139 = _132.w;
   }
   else
   {
      _137 = 1.0f;
      _138 = 1.0f;
      _139 = 1.0f;
   }
   if (!_130)
   {
      float _144 = (c_sdw_param[0].x) * _137;
      float _145 = (c_sdw_param[0].z) * _138;
      float _148 = _144 + (c_sdw_param[0].y);
      float _149 = _145 + (c_sdw_param[0].w);
      _151 = _148;
      _152 = _149;
   }
   else
   {
      _151 = _137;
      _152 = _138;
   }
   float _159 = (c_sdw_param[1].x) * _68.y;
   float _161 = _159 + (c_sdw_param[1].y);
   float _162 = _161 * _68.x;
   float _163 = 1.0f - (c_light_col[5].x);
   float _164 = 1.0f - (c_light_col[5].y);
   float _165 = 1.0f - (c_light_col[5].z);
   float _166 = _162 * _163;
   float _167 = _162 * _164;
   float _168 = _162 * _165;
   float _169 = _166 + (c_light_col[5].x);
   float _170 = _167 + (c_light_col[5].y);
   float _171 = _168 + (c_light_col[5].z);
   float _175 = c_rt_size.z * SV_Position.x;
   float _176 = c_rt_size.w * SV_Position.y;
   float4 _177 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_175, _176)));
   int _184 = c_bool & 16384;
   bool _185 = (_184 == 0);
   if (!_185)
   {
      float _187 = _177.x * 2.0f;
      float _188 = _177.y * 2.0f;
      float _189 = _177.z * 2.0f;
      float _190 = _177.w * 2.0f;
      _207 = _187;
      _208 = _188;
      _209 = _189;
      _210 = _190;
   }
   else
   {
      int _192 = c_bool & 8192;
      bool _193 = (_192 == 0);
      if (!_193)
      {
         float _195 = _177.x * 2.0f;
         float _196 = _177.y * 2.0f;
         float _197 = _177.z * 2.0f;
         float _198 = _177.w * 2.0f;
         _207 = _195;
         _208 = _196;
         _209 = _197;
         _210 = _198;
      }
      else
      {
         float _202 = _177.x / c_tone_control.z;
         float _203 = _177.y / c_tone_control.z;
         float _204 = _177.z / c_tone_control.z;
         float _205 = _177.w / c_tone_control.z;
         _207 = _202;
         _208 = _203;
         _209 = _204;
         _210 = _205;
      }
   }
   int _211 = c_bool & 8;
   bool _212 = (_211 == 0);
   if (!_212)
   {
      float _217 = (c_sdw_param[0].x) * _151;
      float _218 = (c_sdw_param[0].z) * _152;
      float _221 = _217 + (c_sdw_param[0].y);
      float _222 = _218 + (c_sdw_param[0].w);
      _224 = _221;
      _225 = _222;
   }
   else
   {
      _224 = _151;
      _225 = _152;
   }
   float _226 = _225 * _224;
   float _231 = 1.0f - (c_light_col[5].x);
   float _232 = 1.0f - (c_light_col[5].y);
   float _233 = 1.0f - (c_light_col[5].z);
   float _234 = _231 * _226;
   float _235 = _232 * _226;
   float _236 = _233 * _226;
   float _237 = _234 + (c_light_col[5].x);
   float _238 = _235 + (c_light_col[5].y);
   float _239 = _236 + (c_light_col[5].z);
   float _240 = _139 * COLOR.x;
   float _241 = _240 * _169;
   float _242 = _139 * COLOR.y;
   float _243 = _242 * _170;
   float _244 = _139 * COLOR.z;
   float _245 = _244 * _171;
   float _246 = _237 * _169;
   float _247 = _238 * _170;
   float _248 = _239 * _171;
   if (!_212)
   {
      float _252 = (c_sdw_param[2].x) * _237;
      float _253 = (c_sdw_param[2].x) * _238;
      float _254 = (c_sdw_param[2].x) * _239;
      float _256 = _252 + (c_sdw_param[2].y);
      float _257 = _253 + (c_sdw_param[2].y);
      float _258 = _254 + (c_sdw_param[2].y);
      float _260 = (c_sdw_param[2].z) * _169;
      float _261 = (c_sdw_param[2].z) * _170;
      float _262 = (c_sdw_param[2].z) * _171;
      float _264 = _260 + (c_sdw_param[2].w);
      float _265 = _261 + (c_sdw_param[2].w);
      float _266 = _262 + (c_sdw_param[2].w);
      float _267 = _264 * _256;
      float _268 = _265 * _257;
      float _269 = _266 * _258;
      _271 = _267;
      _272 = _268;
      _273 = _269;
   }
   else
   {
      _271 = _246;
      _272 = _247;
      _273 = _248;
   }
   float _274 = _139 * _68.x;
   float _275 = _271 * _274;
   float _276 = _272 * _274;
   float _277 = _273 * _274;
   float _288 = c_mat_emissive.x * _58;
   float _289 = _288 * c_emissive_scale;
   float _290 = c_mat_emissive.y * _59;
   float _291 = _290 * c_emissive_scale;
   float _292 = c_mat_emissive.z * _60;
   float _293 = _292 * c_emissive_scale;
   int _294 = c_bool & 2048;
   bool _295 = (_294 == 0);
   if (_295)
   {
      float _297 = _225 * _207;
      float _298 = _225 * _208;
      float _299 = _225 * _209;
      float _300 = _297 + TEXCOORD_4.x;
      float _301 = _298 + TEXCOORD_4.y;
      float _302 = _299 + TEXCOORD_4.z;
      float _303 = _225 * _210;
      float _304 = _303 + TEXCOORD_5.x;
      float _305 = _303 + TEXCOORD_5.y;
      float _306 = _303 + TEXCOORD_5.z;
      _308 = _304;
      _309 = _305;
      _310 = _306;
      _311 = _300;
      _312 = _301;
      _313 = _302;
   }
   else
   {
      _308 = TEXCOORD_5.x;
      _309 = TEXCOORD_5.y;
      _310 = TEXCOORD_5.z;
      _311 = TEXCOORD_4.x;
      _312 = TEXCOORD_4.y;
      _313 = TEXCOORD_4.z;
   }
   float _318 = -0.0f - (c_light_vec[1].x);
   float _319 = -0.0f - (c_light_vec[1].y);
   float _320 = -0.0f - (c_light_vec[1].z);
   float _321 = dot(float3(_106, _107, _108), float3(_318, _319, _320));
   float _322 = saturate(_321);
   float _327 = saturate(_121);
   float _328 = (c_light_col[0].x) * _327;
   float _329 = (c_light_col[0].y) * _327;
   float _330 = (c_light_col[0].z) * _327;
   float _335 = (c_light_col[1].x) * _322;
   float _336 = (c_light_col[1].y) * _322;
   float _337 = (c_light_col[1].z) * _322;
   float _346 = c_diffuse_scale.x * c_mat_diffuse.x;
   float _347 = c_diffuse_scale.y * c_mat_diffuse.y;
   float _348 = c_diffuse_scale.z * c_mat_diffuse.z;
   float _349 = _328 * _237;
   float _350 = _329 * _238;
   float _351 = _330 * _239;
   float _352 = _335 * _225;
   float _353 = _336 * _225;
   float _354 = _337 * _225;
   float _355 = _352 + _349;
   float _356 = _353 + _350;
   float _357 = _354 + _351;
   float _358 = _346 * _355;
   float _359 = _347 * _356;
   float _360 = _348 * _357;
   float _361 = _289 + c_scene_ambient.x;
   float _362 = _361 + _358;
   float _363 = _291 + c_scene_ambient.y;
   float _364 = _363 + _359;
   float _365 = _293 + c_scene_ambient.z;
   float _366 = _365 + _360;
   float _367 = _362 * _241;
   float _368 = _364 * _243;
   float _369 = _366 * _245;
   float _370 = _311 * _275;
   float _371 = _312 * _276;
   float _372 = _313 * _277;
   float _373 = _367 + _370;
   float _374 = _368 + _371;
   float _375 = _369 + _372;
   float _380 = (c_light_vec[0].x) + _111;
   float _381 = (c_light_vec[0].y) + _112;
   float _382 = (c_light_vec[0].z) + _113;
   float _383 = dot(float3(_380, _381, _382), float3(_380, _381, _382));
   float _384 = rsqrt(_383);
   float _385 = _380 * _384;
   float _386 = -0.0f - _385;
   float _387 = _381 * _384;
   float _388 = -0.0f - _387;
   float _389 = _382 * _384;
   float _390 = -0.0f - _389;
   float _391 = dot(float3(_386, _388, _390), float3(_106, _107, _108));
   float _392 = saturate(_391);
   float _393 = _112 + -0.30000001192092896f;
   float _394 = dot(float3(_111, _393, _113), float3(_111, _393, _113));
   float _395 = rsqrt(_394);
   float _396 = _111 * _395;
   float _397 = -0.0f - _396;
   float _398 = _393 * _395;
   float _399 = -0.0f - _398;
   float _400 = _113 * _395;
   float _401 = -0.0f - _400;
   float _402 = dot(float3(_397, _399, _401), float3(_106, _107, _108));
   float _403 = saturate(_402);
   float _406 = log2(_392);
   float _407 = log2(_403);
   float _408 = _406 * c_mat_power;
   float _409 = _407 * c_mat_power;
   float _410 = exp2(_408);
   float _411 = exp2(_409);
   float _414 = _410 * (c_light_col[0].x);
   float _415 = _410 * (c_light_col[0].y);
   float _416 = _410 * (c_light_col[0].z);
   float _417 = _411 * 2.0f;
   float _418 = _417 * (c_light_vec[5].x);
   float _419 = _418 * (c_light_col[1].x);
   float _420 = _418 * (c_light_col[1].y);
   float _421 = _418 * (c_light_col[1].z);
   float _422 = _414 * _237;
   float _423 = _415 * _238;
   float _424 = _416 * _239;
   float _425 = _419 * _322;
   float _426 = _420 * _322;
   float _427 = _421 * _322;
   float _428 = _425 + _422;
   float _429 = _426 + _423;
   float _430 = _427 + _424;
   float _437 = c_mat_specular.x * c_specular_scale.x;
   float _438 = _437 * _241;
   float _439 = _438 * _428;
   float _440 = c_mat_specular.y * c_specular_scale.y;
   float _441 = _440 * _243;
   float _442 = _441 * _429;
   float _443 = c_mat_specular.z * c_specular_scale.z;
   float _444 = _443 * _245;
   float _445 = _444 * _430;
   float _446 = _308 * _275;
   float _447 = _446 * _411;
   float _448 = _309 * _276;
   float _449 = _448 * _411;
   float _450 = _310 * _277;
   float _451 = _450 * _411;
   float _452 = _439 + _447;
   float _453 = _442 + _449;
   float _454 = _445 + _451;
   if (!_295)
   {
      float _456 = _225 * _207;
      float _457 = _225 * _208;
      float _458 = _225 * _209;
      float _459 = _373 + _456;
      float _460 = _374 + _457;
      float _461 = _375 + _458;
      float _462 = _225 * _210;
      float _463 = _452 + _462;
      float _464 = _453 + _462;
      float _465 = _454 + _462;
      _467 = _459;
      _468 = _460;
      _469 = _461;
      _470 = _463;
      _471 = _464;
      _472 = _465;
   }
   else
   {
      _467 = _373;
      _468 = _374;
      _469 = _375;
      _470 = _452;
      _471 = _453;
      _472 = _454;
   }
   int _473 = c_bool & 128;
   bool _474 = (_473 != 0);
   float _475 = select(_474, 0.0f, _470);
   float _476 = select(_474, 0.0f, _471);
   float _477 = select(_474, 0.0f, _472);
   float _478 = _467 * _58;
   float _479 = _468 * _59;
   float _480 = _469 * _60;
   float _481 = _475 * _61.x;
   float _482 = _476 * _61.y;
   float _483 = _477 * _61.z;
   int _484 = c_bool & 32768;
   bool _485 = (_484 == 0);
   if (!_485)
   {
      float _489 = (c_ps_user[11].x) * _58;
      float _490 = (c_ps_user[11].x) * _59;
      float _491 = (c_ps_user[11].x) * _60;
      float _493 = _106 + 1.0f;
      float _494 = _107 + 1.0f;
      float _495 = _108 + 1.0f;
      float _496 = _493 * 0.5f;
      float _497 = _494 * 0.5f;
      float _498 = _495 * 0.5f;
      float _499 = saturate(_496);
      float _500 = saturate(_497);
      float _501 = saturate(_498);
      float _502 = (c_ps_user[11].y) * _499;
      float _503 = _500 * (c_ps_user[11].y);
      float _504 = _501 * (c_ps_user[11].y);
      float _506 = (c_ps_user[11].z) * _61.x;
      float _507 = (c_ps_user[11].z) * _61.y;
      float _508 = (c_ps_user[11].z) * _61.z;
      float _510 = c_mat_power * 0.009999999776482582f;
      float _511 = _510 * (c_ps_user[11].w);
      float _514 = (c_ps_user[12].x) * _237;
      float _515 = (c_ps_user[12].x) * _238;
      float _516 = (c_ps_user[12].x) * _239;
      float _518 = (c_ps_user[12].y) * _241;
      float _519 = (c_ps_user[12].y) * _243;
      float _520 = (c_ps_user[12].y) * _245;
      float _522 = (c_ps_user[12].z) * _275;
      float _523 = (c_ps_user[12].z) * _276;
      float _524 = (c_ps_user[12].z) * _277;
      float _530 = -0.0f - _111;
      float _531 = -0.0f - _112;
      float _532 = -0.0f - _113;
      float _533 = dot(float3(_530, _531, _532), float3(_106, _107, _108));
      float _534 = 1.0f - _533;
      float _535 = _534 * _534;
      float _536 = _534 * 0.9722222089767456f;
      float _537 = _535 * _535;
      float _538 = _537 * _536;
      float _539 = _538 + 0.02777777425944805f;
      float _540 = _539 * (c_ps_user[13].z);
      float _542 = (c_ps_user[13].w) * c_scene_ambient.x;
      float _543 = (c_ps_user[13].w) * c_scene_ambient.y;
      float _544 = (c_ps_user[13].w) * c_scene_ambient.z;
      float _547 = (c_ps_user[14].x) * _289;
      float _548 = (c_ps_user[14].x) * _291;
      float _549 = (c_ps_user[14].x) * _293;
      float _551 = (c_ps_user[14].y) * _328;
      float _552 = (c_ps_user[14].y) * _329;
      float _553 = (c_ps_user[14].y) * _330;
      float _555 = (c_ps_user[14].z) * _335;
      float _556 = (c_ps_user[14].z) * _336;
      float _557 = (c_ps_user[14].z) * _337;
      float _559 = (c_ps_user[14].w) * _311;
      float _560 = (c_ps_user[14].w) * _312;
      float _561 = (c_ps_user[14].w) * _313;
      float _564 = (c_ps_user[15].x) * _414;
      float _565 = (c_ps_user[15].x) * _415;
      float _566 = (c_ps_user[15].x) * _416;
      float _568 = _437 * _419;
      float _569 = _568 * (c_ps_user[15].y);
      float _570 = _440 * _420;
      float _571 = _570 * (c_ps_user[15].y);
      float _572 = _443 * _421;
      float _573 = _572 * (c_ps_user[15].y);
      float _575 = (c_ps_user[15].z) * _308;
      float _576 = (c_ps_user[15].z) * _309;
      float _577 = (c_ps_user[15].z) * _310;
      float _578 = _569 + _575;
      float _579 = _578 * _411;
      float _580 = _551 + (c_ps_user[13].x);
      float _581 = _580 + _555;
      float _582 = c_diffuse_scale.x * _581;
      float _583 = _582 * c_mat_diffuse.x;
      float _584 = (c_ps_user[13].y) + _506;
      float _585 = _584 + _564;
      float _586 = c_mat_specular.x * _585;
      float _587 = _586 * c_specular_scale.x;
      float _588 = _502 + _489;
      float _589 = _588 + _511;
      float _590 = _589 + (c_ps_user[12].w);
      float _591 = _590 + _514;
      float _592 = _591 + _518;
      float _593 = _592 + _522;
      float _594 = _593 + _542;
      float _595 = _594 + _547;
      float _596 = _595 + _559;
      float _597 = _596 + _540;
      float _598 = _597 + _579;
      float _599 = _598 + _583;
      float _600 = _599 + _587;
      float _601 = _571 + _576;
      float _602 = _601 * _411;
      float _603 = _552 + (c_ps_user[13].x);
      float _604 = _603 + _556;
      float _605 = c_diffuse_scale.y * _604;
      float _606 = _605 * c_mat_diffuse.y;
      float _607 = (c_ps_user[13].y) + _507;
      float _608 = _607 + _565;
      float _609 = c_mat_specular.y * _608;
      float _610 = _609 * c_specular_scale.y;
      float _611 = _503 + _490;
      float _612 = _611 + _511;
      float _613 = _612 + (c_ps_user[12].w);
      float _614 = _613 + _515;
      float _615 = _614 + _519;
      float _616 = _615 + _523;
      float _617 = _616 + _543;
      float _618 = _617 + _548;
      float _619 = _618 + _560;
      float _620 = _619 + _540;
      float _621 = _620 + _602;
      float _622 = _621 + _606;
      float _623 = _622 + _610;
      float _624 = _573 + _577;
      float _625 = _624 * _411;
      float _626 = _553 + (c_ps_user[13].x);
      float _627 = _626 + _557;
      float _628 = c_diffuse_scale.z * _627;
      float _629 = _628 * c_mat_diffuse.z;
      float _630 = (c_ps_user[13].y) + _508;
      float _631 = _630 + _566;
      float _632 = c_mat_specular.z * _631;
      float _633 = _632 * c_specular_scale.z;
      float _634 = _511 + _491;
      float _635 = _634 + _504;
      float _636 = _635 + (c_ps_user[12].w);
      float _637 = _636 + _516;
      float _638 = _637 + _520;
      float _639 = _638 + _524;
      float _640 = _639 + _544;
      float _641 = _640 + _549;
      float _642 = _641 + _561;
      float _643 = _642 + _540;
      float _644 = _643 + _625;
      float _645 = _644 + _629;
      float _646 = _645 + _633;
      _648 = _600;
      _649 = _623;
      _650 = _646;
      _651 = 0.0f;
      _652 = 0.0f;
      _653 = 0.0f;
   }
   else
   {
      _648 = _478;
      _649 = _479;
      _650 = _480;
      _651 = _481;
      _652 = _482;
      _653 = _483;
   }
   float _654 = _651 + _648;
   float _655 = _652 + _649;
   float _656 = _653 + _650;
   float _661 = c_mat_opacity * _53.w;
   float _662 = _661 * c_global_opacity;
   float _663 = _662 * c_opacity_scale;
   int _664 = c_bool & 16;
   bool _665 = (_664 == 0);
   if (!_665)
   {
      float _669 = c_tone_control.x * _654;
      float _670 = c_tone_control.x * _655;
      float _671 = c_tone_control.x * _656;
      float _673 = log2(_669);
      float _674 = log2(_670);
      float _675 = log2(_671);
      float _676 = _673 * c_tone_control.y;
      float _677 = _674 * c_tone_control.y;
      float _678 = _675 * c_tone_control.y;
      float _679 = exp2(_676);
      float _680 = exp2(_677);
      float _681 = exp2(_678);
      _729 = _679;
      _730 = _680;
      _731 = _681;
   }
   else
   {
      if (_485)
      {
         float _686 = c_tone_control.x * _654;
         float _687 = c_tone_control.x * _655;
         float _688 = c_tone_control.x * _656;
         float _689 = _686 * 2.0f;
         float _690 = _687 * 2.0f;
         float _691 = _688 * 2.0f;
         float _692 = _686 * 0.30000001192092896f;
         float _693 = _687 * 0.30000001192092896f;
         float _694 = _688 * 0.30000001192092896f;
         float _695 = _692 + 0.05000000074505806f;
         float _696 = _693 + 0.05000000074505806f;
         float _697 = _694 + 0.05000000074505806f;
         float _698 = _695 * _689;
         float _699 = _696 * _690;
         float _700 = _697 * _691;
         float _701 = _698 + 0.004000000189989805f;
         float _702 = _699 + 0.004000000189989805f;
         float _703 = _700 + 0.004000000189989805f;
         float _704 = _692 + 0.5f;
         float _705 = _693 + 0.5f;
         float _706 = _694 + 0.5f;
         float _707 = _704 * _689;
         float _708 = _705 * _690;
         float _709 = _706 * _691;
         float _710 = _707 + 0.06000000238418579f;
         float _711 = _708 + 0.06000000238418579f;
         float _712 = _709 + 0.06000000238418579f;
         float _713 = _701 / _710;
         float _714 = _702 / _711;
         float _715 = _703 / _712;
         float _716 = _713 + -0.06666666269302368f;
         float _717 = _714 + -0.06666666269302368f;
         float _718 = _715 + -0.06666666269302368f;
         float _719 = _716 * 1.3790643215179443f;
         float _720 = _717 * 1.3790643215179443f;
         float _721 = _718 * 1.3790643215179443f;
         float _722 = YKR_MaterialCurve(_719, _686);
         float _723 = YKR_MaterialCurve(_720, _687);
         float _724 = YKR_MaterialCurve(_721, _688);
         float _725 = sqrt(_722);
         float _726 = sqrt(_723);
         float _727 = sqrt(_724);
         _729 = _725;
         _730 = _726;
         _731 = _727;
      }
      else
      {
         _729 = _654;
         _730 = _655;
         _731 = _656;
      }
   }
   int _732 = c_bool & 4;
   bool _733 = (_732 == 0);
   if (_733)
   {
      float _735 = saturate(COLOR_1.x);
      float _740 = (c_fog_param[0].x) - _729;
      float _741 = (c_fog_param[0].y) - _730;
      float _742 = (c_fog_param[0].z) - _731;
      float _743 = _740 * _735;
      float _744 = _741 * _735;
      float _745 = _742 * _735;
      _747 = _743;
      _748 = _744;
      _749 = _745;
   }
   else
   {
      _747 = COLOR_1.x;
      _748 = COLOR_1.y;
      _749 = COLOR_1.z;
   }
   float _750 = _747 + _729;
   float _751 = _748 + _730;
   float _752 = _749 + _731;
   float _758 = (c_ps_col_scl[0].x) * _750;
   float _759 = (c_ps_col_scl[0].y) * _751;
   float _760 = (c_ps_col_scl[0].z) * _752;
   float _761 = (c_ps_col_scl[0].w) * _663;
   SV_Target.x = _758;
   SV_Target.y = _759;
   SV_Target.z = _760;
   SV_Target.w = _761;
   int _764 = c_bool & 8;
   bool _765 = (_764 != 0);
   float _766 = select(_765, _750, _758);
   float _767 = select(_765, _751, _759);
   float _768 = select(_765, _752, _760);
   float _769 = select(_765, _663, _761);
   SV_Target.x = _766;
   SV_Target.y = _767;
   SV_Target.z = _768;
   SV_Target.w = _769;
   return SV_Target;
}
