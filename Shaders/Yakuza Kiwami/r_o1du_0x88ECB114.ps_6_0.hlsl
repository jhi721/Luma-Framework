// Yakuza Kiwami material r_o1du (0x88ECB114, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
// applied by a script: the inline Hable tonemap's saturate goes through
// YKR_MaterialCurve (tangent continuation in HDR), and the light pre-pass buffer t14 through YKR_LightBuffer.

#include "Includes/Common.hlsl"

Texture2D<float4> s0_t : register(t0);

Texture2D<float4> s3_t : register(t3);

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

SamplerState s13_s : register(s13);

SamplerState s14_s : register(s14);

float4 main(
    linear float2 TEXCOORD : TEXCOORD,
    linear float4 COLOR : COLOR,
    linear float4 COLOR_1 : COLOR1,
    linear float3 TEXCOORD_2 : TEXCOORD2,
    linear float3 TEXCOORD_3 : TEXCOORD3,
    linear float3 TEXCOORD_4 : TEXCOORD4,
    linear float3 TEXCOORD_5 : TEXCOORD5,
    noperspective float4 SV_Position : SV_Position) : SV_Target
{
   float4 SV_Target = 0;
   float4 _39 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _44 = _39.x * _39.x;
   float _45 = _39.y * _39.y;
   float _46 = _39.z * _39.z;
   float _50 = (c_attribute_ps[0].x) * TEXCOORD.x;
   float _51 = (c_attribute_ps[0].y) * TEXCOORD.y;
   float4 _52 = s3_t.Sample(s3_s, float2(_50, _51));
   float _54 = dot(float3(TEXCOORD_2.x, TEXCOORD_2.y, TEXCOORD_2.z), float3(TEXCOORD_2.x, TEXCOORD_2.y, TEXCOORD_2.z));
   float _55 = rsqrt(_54);
   float _56 = _55 * TEXCOORD_2.x;
   float _57 = _55 * TEXCOORD_2.y;
   float _58 = _55 * TEXCOORD_2.z;
   float _59 = dot(float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z), float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z));
   float _60 = rsqrt(_59);
   float _61 = _60 * TEXCOORD_3.x;
   float _62 = _60 * TEXCOORD_3.y;
   float _63 = _60 * TEXCOORD_3.z;
   float _68 = -0.0f - (c_light_vec[0].x);
   float _69 = -0.0f - (c_light_vec[0].y);
   float _70 = -0.0f - (c_light_vec[0].z);
   float _71 = dot(float3(_56, _57, _58), float3(_68, _69, _70));
   float _75 = c_rt_size.z * SV_Position.x;
   float _76 = c_rt_size.w * SV_Position.y;
   int _79 = c_bool & 8;
   bool _80 = (_79 == 0);
   float _87;
   float _88;
   float _89;
   float _101;
   float _102;
   float _138;
   float _139;
   float _140;
   float _141;
   float _155;
   float _156;
   float _191;
   float _192;
   float _193;
   float _227;
   float _228;
   float _229;
   float _230;
   float _231;
   float _232;
   float _386;
   float _387;
   float _388;
   float _389;
   float _390;
   float _391;
   float _563;
   float _564;
   float _565;
   float _566;
   float _567;
   float _568;
   float _644;
   float _645;
   float _646;
   float _662;
   float _663;
   float _664;
   if (!_80)
   {
      float4 _82 = s13_t.Sample(s13_s, float2(_75, _76));
      _87 = _82.x;
      _88 = _82.y;
      _89 = _82.w;
   }
   else
   {
      _87 = 1.0f;
      _88 = 1.0f;
      _89 = 1.0f;
   }
   if (!_80)
   {
      float _94 = (c_sdw_param[0].x) * _87;
      float _95 = (c_sdw_param[0].z) * _88;
      float _98 = _94 + (c_sdw_param[0].y);
      float _99 = _95 + (c_sdw_param[0].w);
      _101 = _98;
      _102 = _99;
   }
   else
   {
      _101 = _87;
      _102 = _88;
   }
   float _106 = c_rt_size.z * SV_Position.x;
   float _107 = c_rt_size.w * SV_Position.y;
   float4 _108 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_106, _107)));
   int _115 = c_bool & 16384;
   bool _116 = (_115 == 0);
   if (!_116)
   {
      float _118 = _108.x * 2.0f;
      float _119 = _108.y * 2.0f;
      float _120 = _108.z * 2.0f;
      float _121 = _108.w * 2.0f;
      _138 = _118;
      _139 = _119;
      _140 = _120;
      _141 = _121;
   }
   else
   {
      int _123 = c_bool & 8192;
      bool _124 = (_123 == 0);
      if (!_124)
      {
         float _126 = _108.x * 2.0f;
         float _127 = _108.y * 2.0f;
         float _128 = _108.z * 2.0f;
         float _129 = _108.w * 2.0f;
         _138 = _126;
         _139 = _127;
         _140 = _128;
         _141 = _129;
      }
      else
      {
         float _133 = _108.x / c_tone_control.z;
         float _134 = _108.y / c_tone_control.z;
         float _135 = _108.z / c_tone_control.z;
         float _136 = _108.w / c_tone_control.z;
         _138 = _133;
         _139 = _134;
         _140 = _135;
         _141 = _136;
      }
   }
   int _142 = c_bool & 8;
   bool _143 = (_142 == 0);
   if (!_143)
   {
      float _148 = (c_sdw_param[0].x) * _101;
      float _149 = (c_sdw_param[0].z) * _102;
      float _152 = _148 + (c_sdw_param[0].y);
      float _153 = _149 + (c_sdw_param[0].w);
      _155 = _152;
      _156 = _153;
   }
   else
   {
      _155 = _101;
      _156 = _102;
   }
   float _157 = _156 * _155;
   float _162 = 1.0f - (c_light_col[5].x);
   float _163 = 1.0f - (c_light_col[5].y);
   float _164 = 1.0f - (c_light_col[5].z);
   float _165 = _162 * _157;
   float _166 = _163 * _157;
   float _167 = _164 * _157;
   float _168 = _165 + (c_light_col[5].x);
   float _169 = _166 + (c_light_col[5].y);
   float _170 = _167 + (c_light_col[5].z);
   float _171 = _89 * COLOR.x;
   float _172 = _89 * COLOR.y;
   float _173 = _89 * COLOR.z;
   if (!_143)
   {
      float _177 = (c_sdw_param[2].x) * _168;
      float _178 = (c_sdw_param[2].x) * _169;
      float _179 = (c_sdw_param[2].x) * _170;
      float _181 = _177 + (c_sdw_param[2].y);
      float _182 = _178 + (c_sdw_param[2].y);
      float _183 = _179 + (c_sdw_param[2].y);
      float _186 = (c_sdw_param[2].w) + (c_sdw_param[2].z);
      float _187 = _181 * _186;
      float _188 = _182 * _186;
      float _189 = _183 * _186;
      _191 = _187;
      _192 = _188;
      _193 = _189;
   }
   else
   {
      _191 = _168;
      _192 = _169;
      _193 = _170;
   }
   float _194 = _191 * _89;
   float _195 = _192 * _89;
   float _196 = _193 * _89;
   float _207 = c_mat_emissive.x * _44;
   float _208 = _207 * c_emissive_scale;
   float _209 = c_mat_emissive.y * _45;
   float _210 = _209 * c_emissive_scale;
   float _211 = c_mat_emissive.z * _46;
   float _212 = _211 * c_emissive_scale;
   int _213 = c_bool & 2048;
   bool _214 = (_213 == 0);
   if (_214)
   {
      float _216 = _156 * _138;
      float _217 = _156 * _139;
      float _218 = _156 * _140;
      float _219 = _216 + TEXCOORD_4.x;
      float _220 = _217 + TEXCOORD_4.y;
      float _221 = _218 + TEXCOORD_4.z;
      float _222 = _156 * _141;
      float _223 = _222 + TEXCOORD_5.x;
      float _224 = _222 + TEXCOORD_5.y;
      float _225 = _222 + TEXCOORD_5.z;
      _227 = _223;
      _228 = _224;
      _229 = _225;
      _230 = _219;
      _231 = _220;
      _232 = _221;
   }
   else
   {
      _227 = TEXCOORD_5.x;
      _228 = TEXCOORD_5.y;
      _229 = TEXCOORD_5.z;
      _230 = TEXCOORD_4.x;
      _231 = TEXCOORD_4.y;
      _232 = TEXCOORD_4.z;
   }
   float _237 = -0.0f - (c_light_vec[1].x);
   float _238 = -0.0f - (c_light_vec[1].y);
   float _239 = -0.0f - (c_light_vec[1].z);
   float _240 = dot(float3(_56, _57, _58), float3(_237, _238, _239));
   float _241 = saturate(_240);
   float _246 = saturate(_71);
   float _247 = (c_light_col[0].x) * _246;
   float _248 = (c_light_col[0].y) * _246;
   float _249 = (c_light_col[0].z) * _246;
   float _254 = (c_light_col[1].x) * _241;
   float _255 = (c_light_col[1].y) * _241;
   float _256 = (c_light_col[1].z) * _241;
   float _265 = c_diffuse_scale.x * c_mat_diffuse.x;
   float _266 = c_diffuse_scale.y * c_mat_diffuse.y;
   float _267 = c_diffuse_scale.z * c_mat_diffuse.z;
   float _268 = _247 * _168;
   float _269 = _248 * _169;
   float _270 = _249 * _170;
   float _271 = _254 * _156;
   float _272 = _255 * _156;
   float _273 = _256 * _156;
   float _274 = _271 + _268;
   float _275 = _272 + _269;
   float _276 = _273 + _270;
   float _277 = _265 * _274;
   float _278 = _266 * _275;
   float _279 = _267 * _276;
   float _280 = _208 + c_scene_ambient.x;
   float _281 = _280 + _277;
   float _282 = _210 + c_scene_ambient.y;
   float _283 = _282 + _278;
   float _284 = _212 + c_scene_ambient.z;
   float _285 = _284 + _279;
   float _286 = _281 * _171;
   float _287 = _283 * _172;
   float _288 = _285 * _173;
   float _289 = _230 * _194;
   float _290 = _231 * _195;
   float _291 = _232 * _196;
   float _292 = _286 + _289;
   float _293 = _287 + _290;
   float _294 = _288 + _291;
   float _299 = (c_light_vec[0].x) + _61;
   float _300 = (c_light_vec[0].y) + _62;
   float _301 = (c_light_vec[0].z) + _63;
   float _302 = dot(float3(_299, _300, _301), float3(_299, _300, _301));
   float _303 = rsqrt(_302);
   float _304 = _299 * _303;
   float _305 = -0.0f - _304;
   float _306 = _300 * _303;
   float _307 = -0.0f - _306;
   float _308 = _301 * _303;
   float _309 = -0.0f - _308;
   float _310 = dot(float3(_305, _307, _309), float3(_56, _57, _58));
   float _311 = saturate(_310);
   float _312 = _62 + -0.30000001192092896f;
   float _313 = dot(float3(_61, _312, _63), float3(_61, _312, _63));
   float _314 = rsqrt(_313);
   float _315 = _61 * _314;
   float _316 = -0.0f - _315;
   float _317 = _312 * _314;
   float _318 = -0.0f - _317;
   float _319 = _63 * _314;
   float _320 = -0.0f - _319;
   float _321 = dot(float3(_316, _318, _320), float3(_56, _57, _58));
   float _322 = saturate(_321);
   float _325 = log2(_311);
   float _326 = log2(_322);
   float _327 = _325 * c_mat_power;
   float _328 = _326 * c_mat_power;
   float _329 = exp2(_327);
   float _330 = exp2(_328);
   float _333 = _329 * (c_light_col[0].x);
   float _334 = _329 * (c_light_col[0].y);
   float _335 = _329 * (c_light_col[0].z);
   float _336 = _330 * 2.0f;
   float _337 = _336 * (c_light_vec[5].x);
   float _338 = _337 * (c_light_col[1].x);
   float _339 = _337 * (c_light_col[1].y);
   float _340 = _337 * (c_light_col[1].z);
   float _341 = _333 * _168;
   float _342 = _334 * _169;
   float _343 = _335 * _170;
   float _344 = _338 * _241;
   float _345 = _339 * _241;
   float _346 = _340 * _241;
   float _347 = _344 + _341;
   float _348 = _345 + _342;
   float _349 = _346 + _343;
   float _356 = c_mat_specular.x * c_specular_scale.x;
   float _357 = _356 * _171;
   float _358 = _357 * _347;
   float _359 = c_mat_specular.y * c_specular_scale.y;
   float _360 = _359 * _172;
   float _361 = _360 * _348;
   float _362 = c_mat_specular.z * c_specular_scale.z;
   float _363 = _362 * _173;
   float _364 = _363 * _349;
   float _365 = _227 * _194;
   float _366 = _365 * _330;
   float _367 = _228 * _195;
   float _368 = _367 * _330;
   float _369 = _229 * _196;
   float _370 = _369 * _330;
   float _371 = _358 + _366;
   float _372 = _361 + _368;
   float _373 = _364 + _370;
   if (!_214)
   {
      float _375 = _156 * _138;
      float _376 = _156 * _139;
      float _377 = _156 * _140;
      float _378 = _292 + _375;
      float _379 = _293 + _376;
      float _380 = _294 + _377;
      float _381 = _156 * _141;
      float _382 = _371 + _381;
      float _383 = _372 + _381;
      float _384 = _373 + _381;
      _386 = _378;
      _387 = _379;
      _388 = _380;
      _389 = _382;
      _390 = _383;
      _391 = _384;
   }
   else
   {
      _386 = _292;
      _387 = _293;
      _388 = _294;
      _389 = _371;
      _390 = _372;
      _391 = _373;
   }
   int _392 = c_bool & 128;
   bool _393 = (_392 != 0);
   float _394 = select(_393, 0.0f, _389);
   float _395 = select(_393, 0.0f, _390);
   float _396 = select(_393, 0.0f, _391);
   float _397 = _386 * _44;
   float _398 = _387 * _45;
   float _399 = _388 * _46;
   float _400 = _394 * _52.y;
   float _401 = _395 * _52.y;
   float _402 = _396 * _52.y;
   int _403 = c_bool & 32768;
   bool _404 = (_403 == 0);
   if (!_404)
   {
      float _408 = (c_ps_user[11].x) * _44;
      float _409 = (c_ps_user[11].x) * _45;
      float _410 = (c_ps_user[11].x) * _46;
      float _412 = _56 + 1.0f;
      float _413 = _57 + 1.0f;
      float _414 = _58 + 1.0f;
      float _415 = _412 * 0.5f;
      float _416 = _413 * 0.5f;
      float _417 = _414 * 0.5f;
      float _418 = saturate(_415);
      float _419 = saturate(_416);
      float _420 = saturate(_417);
      float _421 = (c_ps_user[11].y) * _418;
      float _422 = _419 * (c_ps_user[11].y);
      float _423 = _420 * (c_ps_user[11].y);
      float _425 = (c_ps_user[11].z) * _52.y;
      float _427 = c_mat_power * 0.009999999776482582f;
      float _428 = _427 * (c_ps_user[11].w);
      float _431 = (c_ps_user[12].x) * _168;
      float _432 = (c_ps_user[12].x) * _169;
      float _433 = (c_ps_user[12].x) * _170;
      float _435 = (c_ps_user[12].y) * _171;
      float _436 = (c_ps_user[12].y) * _172;
      float _437 = (c_ps_user[12].y) * _173;
      float _439 = (c_ps_user[12].z) * _194;
      float _440 = (c_ps_user[12].z) * _195;
      float _441 = (c_ps_user[12].z) * _196;
      float _447 = -0.0f - _61;
      float _448 = -0.0f - _62;
      float _449 = -0.0f - _63;
      float _450 = dot(float3(_447, _448, _449), float3(_56, _57, _58));
      float _451 = 1.0f - _450;
      float _452 = _451 * _451;
      float _453 = _451 * 0.9722222089767456f;
      float _454 = _452 * _452;
      float _455 = _454 * _453;
      float _456 = _455 + 0.02777777425944805f;
      float _457 = _456 * (c_ps_user[13].z);
      float _459 = (c_ps_user[13].w) * c_scene_ambient.x;
      float _460 = (c_ps_user[13].w) * c_scene_ambient.y;
      float _461 = (c_ps_user[13].w) * c_scene_ambient.z;
      float _464 = (c_ps_user[14].x) * _208;
      float _465 = (c_ps_user[14].x) * _210;
      float _466 = (c_ps_user[14].x) * _212;
      float _468 = (c_ps_user[14].y) * _247;
      float _469 = (c_ps_user[14].y) * _248;
      float _470 = (c_ps_user[14].y) * _249;
      float _472 = (c_ps_user[14].z) * _254;
      float _473 = (c_ps_user[14].z) * _255;
      float _474 = (c_ps_user[14].z) * _256;
      float _476 = (c_ps_user[14].w) * _230;
      float _477 = (c_ps_user[14].w) * _231;
      float _478 = (c_ps_user[14].w) * _232;
      float _481 = (c_ps_user[15].x) * _333;
      float _482 = (c_ps_user[15].x) * _334;
      float _483 = (c_ps_user[15].x) * _335;
      float _485 = _356 * _338;
      float _486 = _485 * (c_ps_user[15].y);
      float _487 = _359 * _339;
      float _488 = _487 * (c_ps_user[15].y);
      float _489 = _362 * _340;
      float _490 = _489 * (c_ps_user[15].y);
      float _492 = (c_ps_user[15].z) * _227;
      float _493 = (c_ps_user[15].z) * _228;
      float _494 = (c_ps_user[15].z) * _229;
      float _495 = _486 + _492;
      float _496 = _495 * _330;
      float _497 = _468 + (c_ps_user[13].x);
      float _498 = _497 + _472;
      float _499 = c_diffuse_scale.x * _498;
      float _500 = _499 * c_mat_diffuse.x;
      float _501 = (c_ps_user[13].y) + _425;
      float _502 = _501 + _481;
      float _503 = c_mat_specular.x * _502;
      float _504 = _503 * c_specular_scale.x;
      float _505 = _421 + _408;
      float _506 = _505 + _428;
      float _507 = _506 + (c_ps_user[12].w);
      float _508 = _507 + _431;
      float _509 = _508 + _435;
      float _510 = _509 + _439;
      float _511 = _510 + _459;
      float _512 = _511 + _464;
      float _513 = _512 + _476;
      float _514 = _513 + _457;
      float _515 = _514 + _496;
      float _516 = _515 + _500;
      float _517 = _516 + _504;
      float _518 = _488 + _493;
      float _519 = _518 * _330;
      float _520 = _469 + (c_ps_user[13].x);
      float _521 = _520 + _473;
      float _522 = c_diffuse_scale.y * _521;
      float _523 = _522 * c_mat_diffuse.y;
      float _524 = _501 + _482;
      float _525 = c_mat_specular.y * _524;
      float _526 = _525 * c_specular_scale.y;
      float _527 = _422 + _409;
      float _528 = _527 + _428;
      float _529 = _528 + (c_ps_user[12].w);
      float _530 = _529 + _432;
      float _531 = _530 + _436;
      float _532 = _531 + _440;
      float _533 = _532 + _460;
      float _534 = _533 + _465;
      float _535 = _534 + _477;
      float _536 = _535 + _457;
      float _537 = _536 + _519;
      float _538 = _537 + _523;
      float _539 = _538 + _526;
      float _540 = _490 + _494;
      float _541 = _540 * _330;
      float _542 = _470 + (c_ps_user[13].x);
      float _543 = _542 + _474;
      float _544 = c_diffuse_scale.z * _543;
      float _545 = _544 * c_mat_diffuse.z;
      float _546 = _501 + _483;
      float _547 = c_mat_specular.z * _546;
      float _548 = _547 * c_specular_scale.z;
      float _549 = _428 + _410;
      float _550 = _549 + _423;
      float _551 = _550 + (c_ps_user[12].w);
      float _552 = _551 + _433;
      float _553 = _552 + _437;
      float _554 = _553 + _441;
      float _555 = _554 + _461;
      float _556 = _555 + _466;
      float _557 = _556 + _478;
      float _558 = _557 + _457;
      float _559 = _558 + _541;
      float _560 = _559 + _545;
      float _561 = _560 + _548;
      _563 = _517;
      _564 = _539;
      _565 = _561;
      _566 = 0.0f;
      _567 = 0.0f;
      _568 = 0.0f;
   }
   else
   {
      _563 = _397;
      _564 = _398;
      _565 = _399;
      _566 = _400;
      _567 = _401;
      _568 = _402;
   }
   float _569 = _566 + _563;
   float _570 = _567 + _564;
   float _571 = _568 + _565;
   float _576 = c_mat_opacity * _39.w;
   float _577 = _576 * c_global_opacity;
   float _578 = _577 * c_opacity_scale;
   int _579 = c_bool & 16;
   bool _580 = (_579 == 0);
   if (!_580)
   {
      float _584 = c_tone_control.x * _569;
      float _585 = c_tone_control.x * _570;
      float _586 = c_tone_control.x * _571;
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
      if (_404)
      {
         float _601 = c_tone_control.x * _569;
         float _602 = c_tone_control.x * _570;
         float _603 = c_tone_control.x * _571;
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
         _644 = _569;
         _645 = _570;
         _646 = _571;
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
   float _673 = (c_ps_col_scl[0].x) * _665;
   float _674 = (c_ps_col_scl[0].y) * _666;
   float _675 = (c_ps_col_scl[0].z) * _667;
   float _676 = (c_ps_col_scl[0].w) * _578;
   SV_Target.x = _673;
   SV_Target.y = _674;
   SV_Target.z = _675;
   SV_Target.w = _676;
   int _679 = c_bool & 8;
   bool _680 = (_679 != 0);
   float _681 = select(_680, _665, _673);
   float _682 = select(_680, _666, _674);
   float _683 = select(_680, _667, _675);
   float _684 = select(_680, _578, _676);
   SV_Target.x = _681;
   SV_Target.y = _682;
   SV_Target.z = _683;
   SV_Target.w = _684;
   return SV_Target;
}
