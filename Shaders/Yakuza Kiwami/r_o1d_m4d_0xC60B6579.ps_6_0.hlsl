// Yakuza Kiwami material r_o1d_m4d (0xC60B6579, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
// applied by a script: the inline Hable tonemap's saturate goes through
// YKR_MaterialCurve (tangent continuation in HDR), and the light pre-pass buffer t14 through YKR_LightBuffer.

#include "Includes/Common.hlsl"

Texture2D<float4> s0_t : register(t0);

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

SamplerState s7_s : register(s7);

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
    noperspective float4 SV_Position : SV_Position) : SV_Target
{
   float4 SV_Target = 0;
   float4 _41 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _46 = _41.x * _41.x;
   float _47 = _41.y * _41.y;
   float _48 = _41.z * _41.z;
   float4 _49 = s7_t.Sample(s7_s, float2(TEXCOORD.z, TEXCOORD.w));
   float _52 = dot(float3(TEXCOORD_2.x, TEXCOORD_2.y, TEXCOORD_2.z), float3(TEXCOORD_2.x, TEXCOORD_2.y, TEXCOORD_2.z));
   float _53 = rsqrt(_52);
   float _54 = _53 * TEXCOORD_2.x;
   float _55 = _53 * TEXCOORD_2.y;
   float _56 = _53 * TEXCOORD_2.z;
   float _57 = dot(float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z), float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z));
   float _58 = rsqrt(_57);
   float _59 = _58 * TEXCOORD_3.x;
   float _60 = _58 * TEXCOORD_3.y;
   float _61 = _58 * TEXCOORD_3.z;
   float _66 = -0.0f - (c_light_vec[0].x);
   float _67 = -0.0f - (c_light_vec[0].y);
   float _68 = -0.0f - (c_light_vec[0].z);
   float _69 = dot(float3(_54, _55, _56), float3(_66, _67, _68));
   float _73 = c_rt_size.z * SV_Position.x;
   float _74 = c_rt_size.w * SV_Position.y;
   int _77 = c_bool & 8;
   bool _78 = (_77 == 0);
   float _85;
   float _86;
   float _87;
   float _99;
   float _100;
   float _155;
   float _156;
   float _157;
   float _158;
   float _172;
   float _173;
   float _219;
   float _220;
   float _221;
   float _256;
   float _257;
   float _258;
   float _259;
   float _260;
   float _261;
   float _415;
   float _416;
   float _417;
   float _418;
   float _419;
   float _420;
   float _588;
   float _589;
   float _590;
   float _591;
   float _592;
   float _593;
   float _669;
   float _670;
   float _671;
   float _687;
   float _688;
   float _689;
   if (!_78)
   {
      float4 _80 = s13_t.Sample(s13_s, float2(_73, _74));
      _85 = _80.x;
      _86 = _80.y;
      _87 = _80.w;
   }
   else
   {
      _85 = 1.0f;
      _86 = 1.0f;
      _87 = 1.0f;
   }
   if (!_78)
   {
      float _92 = (c_sdw_param[0].x) * _85;
      float _93 = (c_sdw_param[0].z) * _86;
      float _96 = _92 + (c_sdw_param[0].y);
      float _97 = _93 + (c_sdw_param[0].w);
      _99 = _96;
      _100 = _97;
   }
   else
   {
      _99 = _85;
      _100 = _86;
   }
   float _107 = (c_sdw_param[1].x) * _49.y;
   float _109 = _107 + (c_sdw_param[1].y);
   float _110 = _109 * _49.x;
   float _111 = 1.0f - (c_light_col[5].x);
   float _112 = 1.0f - (c_light_col[5].y);
   float _113 = 1.0f - (c_light_col[5].z);
   float _114 = _110 * _111;
   float _115 = _110 * _112;
   float _116 = _110 * _113;
   float _117 = _114 + (c_light_col[5].x);
   float _118 = _115 + (c_light_col[5].y);
   float _119 = _116 + (c_light_col[5].z);
   float _123 = c_rt_size.z * SV_Position.x;
   float _124 = c_rt_size.w * SV_Position.y;
   float4 _125 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_123, _124)));
   int _132 = c_bool & 16384;
   bool _133 = (_132 == 0);
   if (!_133)
   {
      float _135 = _125.x * 2.0f;
      float _136 = _125.y * 2.0f;
      float _137 = _125.z * 2.0f;
      float _138 = _125.w * 2.0f;
      _155 = _135;
      _156 = _136;
      _157 = _137;
      _158 = _138;
   }
   else
   {
      int _140 = c_bool & 8192;
      bool _141 = (_140 == 0);
      if (!_141)
      {
         float _143 = _125.x * 2.0f;
         float _144 = _125.y * 2.0f;
         float _145 = _125.z * 2.0f;
         float _146 = _125.w * 2.0f;
         _155 = _143;
         _156 = _144;
         _157 = _145;
         _158 = _146;
      }
      else
      {
         float _150 = _125.x / c_tone_control.z;
         float _151 = _125.y / c_tone_control.z;
         float _152 = _125.z / c_tone_control.z;
         float _153 = _125.w / c_tone_control.z;
         _155 = _150;
         _156 = _151;
         _157 = _152;
         _158 = _153;
      }
   }
   int _159 = c_bool & 8;
   bool _160 = (_159 == 0);
   if (!_160)
   {
      float _165 = (c_sdw_param[0].x) * _99;
      float _166 = (c_sdw_param[0].z) * _100;
      float _169 = _165 + (c_sdw_param[0].y);
      float _170 = _166 + (c_sdw_param[0].w);
      _172 = _169;
      _173 = _170;
   }
   else
   {
      _172 = _99;
      _173 = _100;
   }
   float _174 = _173 * _172;
   float _179 = 1.0f - (c_light_col[5].x);
   float _180 = 1.0f - (c_light_col[5].y);
   float _181 = 1.0f - (c_light_col[5].z);
   float _182 = _179 * _174;
   float _183 = _180 * _174;
   float _184 = _181 * _174;
   float _185 = _182 + (c_light_col[5].x);
   float _186 = _183 + (c_light_col[5].y);
   float _187 = _184 + (c_light_col[5].z);
   float _188 = _87 * COLOR.x;
   float _189 = _188 * _117;
   float _190 = _87 * COLOR.y;
   float _191 = _190 * _118;
   float _192 = _87 * COLOR.z;
   float _193 = _192 * _119;
   float _194 = _185 * _117;
   float _195 = _186 * _118;
   float _196 = _187 * _119;
   if (!_160)
   {
      float _200 = (c_sdw_param[2].x) * _185;
      float _201 = (c_sdw_param[2].x) * _186;
      float _202 = (c_sdw_param[2].x) * _187;
      float _204 = _200 + (c_sdw_param[2].y);
      float _205 = _201 + (c_sdw_param[2].y);
      float _206 = _202 + (c_sdw_param[2].y);
      float _208 = (c_sdw_param[2].z) * _117;
      float _209 = (c_sdw_param[2].z) * _118;
      float _210 = (c_sdw_param[2].z) * _119;
      float _212 = _208 + (c_sdw_param[2].w);
      float _213 = _209 + (c_sdw_param[2].w);
      float _214 = _210 + (c_sdw_param[2].w);
      float _215 = _212 * _204;
      float _216 = _213 * _205;
      float _217 = _214 * _206;
      _219 = _215;
      _220 = _216;
      _221 = _217;
   }
   else
   {
      _219 = _194;
      _220 = _195;
      _221 = _196;
   }
   float _222 = _87 * _49.x;
   float _223 = _219 * _222;
   float _224 = _220 * _222;
   float _225 = _221 * _222;
   float _236 = c_mat_emissive.x * _46;
   float _237 = _236 * c_emissive_scale;
   float _238 = c_mat_emissive.y * _47;
   float _239 = _238 * c_emissive_scale;
   float _240 = c_mat_emissive.z * _48;
   float _241 = _240 * c_emissive_scale;
   int _242 = c_bool & 2048;
   bool _243 = (_242 == 0);
   if (_243)
   {
      float _245 = _173 * _155;
      float _246 = _173 * _156;
      float _247 = _173 * _157;
      float _248 = _245 + TEXCOORD_4.x;
      float _249 = _246 + TEXCOORD_4.y;
      float _250 = _247 + TEXCOORD_4.z;
      float _251 = _173 * _158;
      float _252 = _251 + TEXCOORD_5.x;
      float _253 = _251 + TEXCOORD_5.y;
      float _254 = _251 + TEXCOORD_5.z;
      _256 = _252;
      _257 = _253;
      _258 = _254;
      _259 = _248;
      _260 = _249;
      _261 = _250;
   }
   else
   {
      _256 = TEXCOORD_5.x;
      _257 = TEXCOORD_5.y;
      _258 = TEXCOORD_5.z;
      _259 = TEXCOORD_4.x;
      _260 = TEXCOORD_4.y;
      _261 = TEXCOORD_4.z;
   }
   float _266 = -0.0f - (c_light_vec[1].x);
   float _267 = -0.0f - (c_light_vec[1].y);
   float _268 = -0.0f - (c_light_vec[1].z);
   float _269 = dot(float3(_54, _55, _56), float3(_266, _267, _268));
   float _270 = saturate(_269);
   float _275 = saturate(_69);
   float _276 = (c_light_col[0].x) * _275;
   float _277 = (c_light_col[0].y) * _275;
   float _278 = (c_light_col[0].z) * _275;
   float _283 = (c_light_col[1].x) * _270;
   float _284 = (c_light_col[1].y) * _270;
   float _285 = (c_light_col[1].z) * _270;
   float _294 = c_diffuse_scale.x * c_mat_diffuse.x;
   float _295 = c_diffuse_scale.y * c_mat_diffuse.y;
   float _296 = c_diffuse_scale.z * c_mat_diffuse.z;
   float _297 = _276 * _185;
   float _298 = _277 * _186;
   float _299 = _278 * _187;
   float _300 = _283 * _173;
   float _301 = _284 * _173;
   float _302 = _285 * _173;
   float _303 = _300 + _297;
   float _304 = _301 + _298;
   float _305 = _302 + _299;
   float _306 = _294 * _303;
   float _307 = _295 * _304;
   float _308 = _296 * _305;
   float _309 = _237 + c_scene_ambient.x;
   float _310 = _309 + _306;
   float _311 = _239 + c_scene_ambient.y;
   float _312 = _311 + _307;
   float _313 = _241 + c_scene_ambient.z;
   float _314 = _313 + _308;
   float _315 = _310 * _189;
   float _316 = _312 * _191;
   float _317 = _314 * _193;
   float _318 = _259 * _223;
   float _319 = _260 * _224;
   float _320 = _261 * _225;
   float _321 = _315 + _318;
   float _322 = _316 + _319;
   float _323 = _317 + _320;
   float _328 = (c_light_vec[0].x) + _59;
   float _329 = (c_light_vec[0].y) + _60;
   float _330 = (c_light_vec[0].z) + _61;
   float _331 = dot(float3(_328, _329, _330), float3(_328, _329, _330));
   float _332 = rsqrt(_331);
   float _333 = _328 * _332;
   float _334 = -0.0f - _333;
   float _335 = _329 * _332;
   float _336 = -0.0f - _335;
   float _337 = _330 * _332;
   float _338 = -0.0f - _337;
   float _339 = dot(float3(_334, _336, _338), float3(_54, _55, _56));
   float _340 = saturate(_339);
   float _341 = _60 + -0.30000001192092896f;
   float _342 = dot(float3(_59, _341, _61), float3(_59, _341, _61));
   float _343 = rsqrt(_342);
   float _344 = _59 * _343;
   float _345 = -0.0f - _344;
   float _346 = _341 * _343;
   float _347 = -0.0f - _346;
   float _348 = _61 * _343;
   float _349 = -0.0f - _348;
   float _350 = dot(float3(_345, _347, _349), float3(_54, _55, _56));
   float _351 = saturate(_350);
   float _354 = log2(_340);
   float _355 = log2(_351);
   float _356 = _354 * c_mat_power;
   float _357 = _355 * c_mat_power;
   float _358 = exp2(_356);
   float _359 = exp2(_357);
   float _362 = _358 * (c_light_col[0].x);
   float _363 = _358 * (c_light_col[0].y);
   float _364 = _358 * (c_light_col[0].z);
   float _365 = _359 * 2.0f;
   float _366 = _365 * (c_light_vec[5].x);
   float _367 = _366 * (c_light_col[1].x);
   float _368 = _366 * (c_light_col[1].y);
   float _369 = _366 * (c_light_col[1].z);
   float _370 = _362 * _185;
   float _371 = _363 * _186;
   float _372 = _364 * _187;
   float _373 = _367 * _270;
   float _374 = _368 * _270;
   float _375 = _369 * _270;
   float _376 = _373 + _370;
   float _377 = _374 + _371;
   float _378 = _375 + _372;
   float _385 = c_mat_specular.x * c_specular_scale.x;
   float _386 = _385 * _189;
   float _387 = _386 * _376;
   float _388 = c_mat_specular.y * c_specular_scale.y;
   float _389 = _388 * _191;
   float _390 = _389 * _377;
   float _391 = c_mat_specular.z * c_specular_scale.z;
   float _392 = _391 * _193;
   float _393 = _392 * _378;
   float _394 = _256 * _223;
   float _395 = _394 * _359;
   float _396 = _257 * _224;
   float _397 = _396 * _359;
   float _398 = _258 * _225;
   float _399 = _398 * _359;
   float _400 = _387 + _395;
   float _401 = _390 + _397;
   float _402 = _393 + _399;
   if (!_243)
   {
      float _404 = _173 * _155;
      float _405 = _173 * _156;
      float _406 = _173 * _157;
      float _407 = _321 + _404;
      float _408 = _322 + _405;
      float _409 = _323 + _406;
      float _410 = _173 * _158;
      float _411 = _400 + _410;
      float _412 = _401 + _410;
      float _413 = _402 + _410;
      _415 = _407;
      _416 = _408;
      _417 = _409;
      _418 = _411;
      _419 = _412;
      _420 = _413;
   }
   else
   {
      _415 = _321;
      _416 = _322;
      _417 = _323;
      _418 = _400;
      _419 = _401;
      _420 = _402;
   }
   int _421 = c_bool & 128;
   bool _422 = (_421 != 0);
   float _423 = select(_422, 0.0f, _418);
   float _424 = select(_422, 0.0f, _419);
   float _425 = select(_422, 0.0f, _420);
   float _426 = _415 * _46;
   float _427 = _416 * _47;
   float _428 = _417 * _48;
   int _429 = c_bool & 32768;
   bool _430 = (_429 == 0);
   if (!_430)
   {
      float _434 = (c_ps_user[11].x) * _46;
      float _435 = (c_ps_user[11].x) * _47;
      float _436 = (c_ps_user[11].x) * _48;
      float _438 = _54 + 1.0f;
      float _439 = _55 + 1.0f;
      float _440 = _56 + 1.0f;
      float _441 = _438 * 0.5f;
      float _442 = _439 * 0.5f;
      float _443 = _440 * 0.5f;
      float _444 = saturate(_441);
      float _445 = saturate(_442);
      float _446 = saturate(_443);
      float _447 = (c_ps_user[11].y) * _444;
      float _448 = _445 * (c_ps_user[11].y);
      float _449 = _446 * (c_ps_user[11].y);
      float _452 = c_mat_power * 0.009999999776482582f;
      float _453 = _452 * (c_ps_user[11].w);
      float _456 = (c_ps_user[12].x) * _185;
      float _457 = (c_ps_user[12].x) * _186;
      float _458 = (c_ps_user[12].x) * _187;
      float _460 = (c_ps_user[12].y) * _189;
      float _461 = (c_ps_user[12].y) * _191;
      float _462 = (c_ps_user[12].y) * _193;
      float _464 = (c_ps_user[12].z) * _223;
      float _465 = (c_ps_user[12].z) * _224;
      float _466 = (c_ps_user[12].z) * _225;
      float _472 = -0.0f - _59;
      float _473 = -0.0f - _60;
      float _474 = -0.0f - _61;
      float _475 = dot(float3(_472, _473, _474), float3(_54, _55, _56));
      float _476 = 1.0f - _475;
      float _477 = _476 * _476;
      float _478 = _476 * 0.9722222089767456f;
      float _479 = _477 * _477;
      float _480 = _479 * _478;
      float _481 = _480 + 0.02777777425944805f;
      float _482 = _481 * (c_ps_user[13].z);
      float _484 = (c_ps_user[13].w) * c_scene_ambient.x;
      float _485 = (c_ps_user[13].w) * c_scene_ambient.y;
      float _486 = (c_ps_user[13].w) * c_scene_ambient.z;
      float _489 = (c_ps_user[14].x) * _237;
      float _490 = (c_ps_user[14].x) * _239;
      float _491 = (c_ps_user[14].x) * _241;
      float _493 = (c_ps_user[14].y) * _276;
      float _494 = (c_ps_user[14].y) * _277;
      float _495 = (c_ps_user[14].y) * _278;
      float _497 = (c_ps_user[14].z) * _283;
      float _498 = (c_ps_user[14].z) * _284;
      float _499 = (c_ps_user[14].z) * _285;
      float _501 = (c_ps_user[14].w) * _259;
      float _502 = (c_ps_user[14].w) * _260;
      float _503 = (c_ps_user[14].w) * _261;
      float _506 = (c_ps_user[15].x) * _362;
      float _507 = (c_ps_user[15].x) * _363;
      float _508 = (c_ps_user[15].x) * _364;
      float _510 = _385 * _367;
      float _511 = _510 * (c_ps_user[15].y);
      float _512 = _388 * _368;
      float _513 = _512 * (c_ps_user[15].y);
      float _514 = _391 * _369;
      float _515 = _514 * (c_ps_user[15].y);
      float _517 = (c_ps_user[15].z) * _256;
      float _518 = (c_ps_user[15].z) * _257;
      float _519 = (c_ps_user[15].z) * _258;
      float _520 = _511 + _517;
      float _521 = _520 * _359;
      float _522 = _493 + (c_ps_user[13].x);
      float _523 = _522 + _497;
      float _524 = c_diffuse_scale.x * _523;
      float _525 = _524 * c_mat_diffuse.x;
      float _526 = (c_ps_user[13].y) + (c_ps_user[11].z);
      float _527 = _526 + _506;
      float _528 = c_mat_specular.x * _527;
      float _529 = _528 * c_specular_scale.x;
      float _530 = _447 + _434;
      float _531 = _530 + _453;
      float _532 = _531 + (c_ps_user[12].w);
      float _533 = _532 + _456;
      float _534 = _533 + _460;
      float _535 = _534 + _464;
      float _536 = _535 + _484;
      float _537 = _536 + _489;
      float _538 = _537 + _501;
      float _539 = _538 + _482;
      float _540 = _539 + _521;
      float _541 = _540 + _525;
      float _542 = _541 + _529;
      float _543 = _513 + _518;
      float _544 = _543 * _359;
      float _545 = _494 + (c_ps_user[13].x);
      float _546 = _545 + _498;
      float _547 = c_diffuse_scale.y * _546;
      float _548 = _547 * c_mat_diffuse.y;
      float _549 = _526 + _507;
      float _550 = c_mat_specular.y * _549;
      float _551 = _550 * c_specular_scale.y;
      float _552 = _448 + _435;
      float _553 = _552 + _453;
      float _554 = _553 + (c_ps_user[12].w);
      float _555 = _554 + _457;
      float _556 = _555 + _461;
      float _557 = _556 + _465;
      float _558 = _557 + _485;
      float _559 = _558 + _490;
      float _560 = _559 + _502;
      float _561 = _560 + _482;
      float _562 = _561 + _544;
      float _563 = _562 + _548;
      float _564 = _563 + _551;
      float _565 = _515 + _519;
      float _566 = _565 * _359;
      float _567 = _495 + (c_ps_user[13].x);
      float _568 = _567 + _499;
      float _569 = c_diffuse_scale.z * _568;
      float _570 = _569 * c_mat_diffuse.z;
      float _571 = _526 + _508;
      float _572 = c_mat_specular.z * _571;
      float _573 = _572 * c_specular_scale.z;
      float _574 = _453 + _436;
      float _575 = _574 + _449;
      float _576 = _575 + (c_ps_user[12].w);
      float _577 = _576 + _458;
      float _578 = _577 + _462;
      float _579 = _578 + _466;
      float _580 = _579 + _486;
      float _581 = _580 + _491;
      float _582 = _581 + _503;
      float _583 = _582 + _482;
      float _584 = _583 + _566;
      float _585 = _584 + _570;
      float _586 = _585 + _573;
      _588 = _542;
      _589 = _564;
      _590 = _586;
      _591 = 0.0f;
      _592 = 0.0f;
      _593 = 0.0f;
   }
   else
   {
      _588 = _426;
      _589 = _427;
      _590 = _428;
      _591 = _423;
      _592 = _424;
      _593 = _425;
   }
   float _594 = _591 + _588;
   float _595 = _592 + _589;
   float _596 = _593 + _590;
   float _601 = c_mat_opacity * _41.w;
   float _602 = _601 * c_global_opacity;
   float _603 = _602 * c_opacity_scale;
   int _604 = c_bool & 16;
   bool _605 = (_604 == 0);
   if (!_605)
   {
      float _609 = c_tone_control.x * _594;
      float _610 = c_tone_control.x * _595;
      float _611 = c_tone_control.x * _596;
      float _613 = log2(_609);
      float _614 = log2(_610);
      float _615 = log2(_611);
      float _616 = _613 * c_tone_control.y;
      float _617 = _614 * c_tone_control.y;
      float _618 = _615 * c_tone_control.y;
      float _619 = exp2(_616);
      float _620 = exp2(_617);
      float _621 = exp2(_618);
      _669 = _619;
      _670 = _620;
      _671 = _621;
   }
   else
   {
      if (_430)
      {
         float _626 = c_tone_control.x * _594;
         float _627 = c_tone_control.x * _595;
         float _628 = c_tone_control.x * _596;
         float _629 = _626 * 2.0f;
         float _630 = _627 * 2.0f;
         float _631 = _628 * 2.0f;
         float _632 = _626 * 0.30000001192092896f;
         float _633 = _627 * 0.30000001192092896f;
         float _634 = _628 * 0.30000001192092896f;
         float _635 = _632 + 0.05000000074505806f;
         float _636 = _633 + 0.05000000074505806f;
         float _637 = _634 + 0.05000000074505806f;
         float _638 = _635 * _629;
         float _639 = _636 * _630;
         float _640 = _637 * _631;
         float _641 = _638 + 0.004000000189989805f;
         float _642 = _639 + 0.004000000189989805f;
         float _643 = _640 + 0.004000000189989805f;
         float _644 = _632 + 0.5f;
         float _645 = _633 + 0.5f;
         float _646 = _634 + 0.5f;
         float _647 = _644 * _629;
         float _648 = _645 * _630;
         float _649 = _646 * _631;
         float _650 = _647 + 0.06000000238418579f;
         float _651 = _648 + 0.06000000238418579f;
         float _652 = _649 + 0.06000000238418579f;
         float _653 = _641 / _650;
         float _654 = _642 / _651;
         float _655 = _643 / _652;
         float _656 = _653 + -0.06666666269302368f;
         float _657 = _654 + -0.06666666269302368f;
         float _658 = _655 + -0.06666666269302368f;
         float _659 = _656 * 1.3790643215179443f;
         float _660 = _657 * 1.3790643215179443f;
         float _661 = _658 * 1.3790643215179443f;
         float _662 = YKR_MaterialCurve(_659, _626);
         float _663 = YKR_MaterialCurve(_660, _627);
         float _664 = YKR_MaterialCurve(_661, _628);
         float _665 = sqrt(_662);
         float _666 = sqrt(_663);
         float _667 = sqrt(_664);
         _669 = _665;
         _670 = _666;
         _671 = _667;
      }
      else
      {
         _669 = _594;
         _670 = _595;
         _671 = _596;
      }
   }
   int _672 = c_bool & 4;
   bool _673 = (_672 == 0);
   if (_673)
   {
      float _675 = saturate(COLOR_1.x);
      float _680 = (c_fog_param[0].x) - _669;
      float _681 = (c_fog_param[0].y) - _670;
      float _682 = (c_fog_param[0].z) - _671;
      float _683 = _680 * _675;
      float _684 = _681 * _675;
      float _685 = _682 * _675;
      _687 = _683;
      _688 = _684;
      _689 = _685;
   }
   else
   {
      _687 = COLOR_1.x;
      _688 = COLOR_1.y;
      _689 = COLOR_1.z;
   }
   float _690 = _687 + _669;
   float _691 = _688 + _670;
   float _692 = _689 + _671;
   float _698 = (c_ps_col_scl[0].x) * _690;
   float _699 = (c_ps_col_scl[0].y) * _691;
   float _700 = (c_ps_col_scl[0].z) * _692;
   float _701 = (c_ps_col_scl[0].w) * _603;
   SV_Target.x = _698;
   SV_Target.y = _699;
   SV_Target.z = _700;
   SV_Target.w = _701;
   int _704 = c_bool & 8;
   bool _705 = (_704 != 0);
   float _706 = select(_705, _690, _698);
   float _707 = select(_705, _691, _699);
   float _708 = select(_705, _692, _700);
   float _709 = select(_705, _603, _701);
   SV_Target.x = _706;
   SV_Target.y = _707;
   SV_Target.z = _708;
   SV_Target.w = _709;
   return SV_Target;
}
