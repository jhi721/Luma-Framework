// Yakuza Kiwami material r_o1ds_m(vr)2d (0xBBDD5570, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
// applied by a script: the inline Hable tonemap's saturate goes through
// YKR_MaterialCurve (tangent continuation in HDR), and the light pre-pass buffer t14 through YKR_LightBuffer.

#include "Includes/Common.hlsl"

Texture2D<float4> s0_t : register(t0);

Texture2D<float4> s1_t : register(t1);

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

SamplerState s1_s : register(s1);

SamplerState s3_s : register(s3);

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
   float4 _44 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _49 = _44.x * _44.x;
   float _50 = _44.y * _44.y;
   float _51 = _44.z * _44.z;
   float _55 = (c_attribute_ps[0].x) * TEXCOORD.x;
   float _56 = (c_attribute_ps[0].y) * TEXCOORD.y;
   float4 _57 = s3_t.Sample(s3_s, float2(_55, _56));
   float4 _61 = s1_t.Sample(s1_s, float2(TEXCOORD.z, TEXCOORD.w));
   float _65 = _61.x * _61.x;
   float _66 = _61.y * _61.y;
   float _67 = _61.z * _61.z;
   float _68 = 1.0f - _65;
   float _69 = 1.0f - _66;
   float _70 = 1.0f - _67;
   float _71 = _68 * COLOR_1.w;
   float _72 = _69 * COLOR_1.w;
   float _73 = _70 * COLOR_1.w;
   float _74 = 1.0f - _71;
   float _75 = 1.0f - _72;
   float _76 = 1.0f - _73;
   float _77 = _49 * _74;
   float _78 = _50 * _75;
   float _79 = _51 * _76;
   float _80 = dot(float3(TEXCOORD_2.x, TEXCOORD_2.y, TEXCOORD_2.z), float3(TEXCOORD_2.x, TEXCOORD_2.y, TEXCOORD_2.z));
   float _81 = rsqrt(_80);
   float _82 = _81 * TEXCOORD_2.x;
   float _83 = _81 * TEXCOORD_2.y;
   float _84 = _81 * TEXCOORD_2.z;
   float _85 = dot(float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z), float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z));
   float _86 = rsqrt(_85);
   float _87 = _86 * TEXCOORD_3.x;
   float _88 = _86 * TEXCOORD_3.y;
   float _89 = _86 * TEXCOORD_3.z;
   float _94 = -0.0f - (c_light_vec[0].x);
   float _95 = -0.0f - (c_light_vec[0].y);
   float _96 = -0.0f - (c_light_vec[0].z);
   float _97 = dot(float3(_82, _83, _84), float3(_94, _95, _96));
   float _101 = c_rt_size.z * SV_Position.x;
   float _102 = c_rt_size.w * SV_Position.y;
   int _105 = c_bool & 8;
   bool _106 = (_105 == 0);
   float _113;
   float _114;
   float _115;
   float _127;
   float _128;
   float _164;
   float _165;
   float _166;
   float _167;
   float _181;
   float _182;
   float _217;
   float _218;
   float _219;
   float _253;
   float _254;
   float _255;
   float _256;
   float _257;
   float _258;
   float _412;
   float _413;
   float _414;
   float _415;
   float _416;
   float _417;
   float _593;
   float _594;
   float _595;
   float _596;
   float _597;
   float _598;
   float _674;
   float _675;
   float _676;
   float _692;
   float _693;
   float _694;
   if (!_106)
   {
      float4 _108 = s13_t.Sample(s13_s, float2(_101, _102));
      _113 = _108.x;
      _114 = _108.y;
      _115 = _108.w;
   }
   else
   {
      _113 = 1.0f;
      _114 = 1.0f;
      _115 = 1.0f;
   }
   if (!_106)
   {
      float _120 = (c_sdw_param[0].x) * _113;
      float _121 = (c_sdw_param[0].z) * _114;
      float _124 = _120 + (c_sdw_param[0].y);
      float _125 = _121 + (c_sdw_param[0].w);
      _127 = _124;
      _128 = _125;
   }
   else
   {
      _127 = _113;
      _128 = _114;
   }
   float _132 = c_rt_size.z * SV_Position.x;
   float _133 = c_rt_size.w * SV_Position.y;
   float4 _134 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_132, _133)));
   int _141 = c_bool & 16384;
   bool _142 = (_141 == 0);
   if (!_142)
   {
      float _144 = _134.x * 2.0f;
      float _145 = _134.y * 2.0f;
      float _146 = _134.z * 2.0f;
      float _147 = _134.w * 2.0f;
      _164 = _144;
      _165 = _145;
      _166 = _146;
      _167 = _147;
   }
   else
   {
      int _149 = c_bool & 8192;
      bool _150 = (_149 == 0);
      if (!_150)
      {
         float _152 = _134.x * 2.0f;
         float _153 = _134.y * 2.0f;
         float _154 = _134.z * 2.0f;
         float _155 = _134.w * 2.0f;
         _164 = _152;
         _165 = _153;
         _166 = _154;
         _167 = _155;
      }
      else
      {
         float _159 = _134.x / c_tone_control.z;
         float _160 = _134.y / c_tone_control.z;
         float _161 = _134.z / c_tone_control.z;
         float _162 = _134.w / c_tone_control.z;
         _164 = _159;
         _165 = _160;
         _166 = _161;
         _167 = _162;
      }
   }
   int _168 = c_bool & 8;
   bool _169 = (_168 == 0);
   if (!_169)
   {
      float _174 = (c_sdw_param[0].x) * _127;
      float _175 = (c_sdw_param[0].z) * _128;
      float _178 = _174 + (c_sdw_param[0].y);
      float _179 = _175 + (c_sdw_param[0].w);
      _181 = _178;
      _182 = _179;
   }
   else
   {
      _181 = _127;
      _182 = _128;
   }
   float _183 = _182 * _181;
   float _188 = 1.0f - (c_light_col[5].x);
   float _189 = 1.0f - (c_light_col[5].y);
   float _190 = 1.0f - (c_light_col[5].z);
   float _191 = _188 * _183;
   float _192 = _189 * _183;
   float _193 = _190 * _183;
   float _194 = _191 + (c_light_col[5].x);
   float _195 = _192 + (c_light_col[5].y);
   float _196 = _193 + (c_light_col[5].z);
   float _197 = _115 * COLOR.x;
   float _198 = _115 * COLOR.y;
   float _199 = _115 * COLOR.z;
   if (!_169)
   {
      float _203 = (c_sdw_param[2].x) * _194;
      float _204 = (c_sdw_param[2].x) * _195;
      float _205 = (c_sdw_param[2].x) * _196;
      float _207 = _203 + (c_sdw_param[2].y);
      float _208 = _204 + (c_sdw_param[2].y);
      float _209 = _205 + (c_sdw_param[2].y);
      float _212 = (c_sdw_param[2].w) + (c_sdw_param[2].z);
      float _213 = _207 * _212;
      float _214 = _208 * _212;
      float _215 = _209 * _212;
      _217 = _213;
      _218 = _214;
      _219 = _215;
   }
   else
   {
      _217 = _194;
      _218 = _195;
      _219 = _196;
   }
   float _220 = _217 * _115;
   float _221 = _218 * _115;
   float _222 = _219 * _115;
   float _233 = c_mat_emissive.x * _77;
   float _234 = _233 * c_emissive_scale;
   float _235 = c_mat_emissive.y * _78;
   float _236 = _235 * c_emissive_scale;
   float _237 = c_mat_emissive.z * _79;
   float _238 = _237 * c_emissive_scale;
   int _239 = c_bool & 2048;
   bool _240 = (_239 == 0);
   if (_240)
   {
      float _242 = _182 * _164;
      float _243 = _182 * _165;
      float _244 = _182 * _166;
      float _245 = _242 + TEXCOORD_4.x;
      float _246 = _243 + TEXCOORD_4.y;
      float _247 = _244 + TEXCOORD_4.z;
      float _248 = _182 * _167;
      float _249 = _248 + TEXCOORD_5.x;
      float _250 = _248 + TEXCOORD_5.y;
      float _251 = _248 + TEXCOORD_5.z;
      _253 = _245;
      _254 = _246;
      _255 = _247;
      _256 = _249;
      _257 = _250;
      _258 = _251;
   }
   else
   {
      _253 = TEXCOORD_4.x;
      _254 = TEXCOORD_4.y;
      _255 = TEXCOORD_4.z;
      _256 = TEXCOORD_5.x;
      _257 = TEXCOORD_5.y;
      _258 = TEXCOORD_5.z;
   }
   float _263 = -0.0f - (c_light_vec[1].x);
   float _264 = -0.0f - (c_light_vec[1].y);
   float _265 = -0.0f - (c_light_vec[1].z);
   float _266 = dot(float3(_82, _83, _84), float3(_263, _264, _265));
   float _267 = saturate(_266);
   float _272 = saturate(_97);
   float _273 = (c_light_col[0].x) * _272;
   float _274 = (c_light_col[0].y) * _272;
   float _275 = (c_light_col[0].z) * _272;
   float _280 = (c_light_col[1].x) * _267;
   float _281 = (c_light_col[1].y) * _267;
   float _282 = (c_light_col[1].z) * _267;
   float _291 = c_diffuse_scale.x * c_mat_diffuse.x;
   float _292 = c_diffuse_scale.y * c_mat_diffuse.y;
   float _293 = c_diffuse_scale.z * c_mat_diffuse.z;
   float _294 = _273 * _194;
   float _295 = _274 * _195;
   float _296 = _275 * _196;
   float _297 = _280 * _182;
   float _298 = _281 * _182;
   float _299 = _282 * _182;
   float _300 = _297 + _294;
   float _301 = _298 + _295;
   float _302 = _299 + _296;
   float _303 = _291 * _300;
   float _304 = _292 * _301;
   float _305 = _293 * _302;
   float _306 = _234 + c_scene_ambient.x;
   float _307 = _306 + _303;
   float _308 = _236 + c_scene_ambient.y;
   float _309 = _308 + _304;
   float _310 = _238 + c_scene_ambient.z;
   float _311 = _310 + _305;
   float _312 = _307 * _197;
   float _313 = _309 * _198;
   float _314 = _311 * _199;
   float _315 = _253 * _220;
   float _316 = _254 * _221;
   float _317 = _255 * _222;
   float _318 = _312 + _315;
   float _319 = _313 + _316;
   float _320 = _314 + _317;
   float _325 = (c_light_vec[0].x) + _87;
   float _326 = (c_light_vec[0].y) + _88;
   float _327 = (c_light_vec[0].z) + _89;
   float _328 = dot(float3(_325, _326, _327), float3(_325, _326, _327));
   float _329 = rsqrt(_328);
   float _330 = _325 * _329;
   float _331 = -0.0f - _330;
   float _332 = _326 * _329;
   float _333 = -0.0f - _332;
   float _334 = _327 * _329;
   float _335 = -0.0f - _334;
   float _336 = dot(float3(_331, _333, _335), float3(_82, _83, _84));
   float _337 = saturate(_336);
   float _338 = _88 + -0.30000001192092896f;
   float _339 = dot(float3(_87, _338, _89), float3(_87, _338, _89));
   float _340 = rsqrt(_339);
   float _341 = _87 * _340;
   float _342 = -0.0f - _341;
   float _343 = _338 * _340;
   float _344 = -0.0f - _343;
   float _345 = _89 * _340;
   float _346 = -0.0f - _345;
   float _347 = dot(float3(_342, _344, _346), float3(_82, _83, _84));
   float _348 = saturate(_347);
   float _351 = log2(_337);
   float _352 = log2(_348);
   float _353 = _351 * c_mat_power;
   float _354 = _352 * c_mat_power;
   float _355 = exp2(_353);
   float _356 = exp2(_354);
   float _359 = _355 * (c_light_col[0].x);
   float _360 = _355 * (c_light_col[0].y);
   float _361 = _355 * (c_light_col[0].z);
   float _362 = _356 * 2.0f;
   float _363 = _362 * (c_light_vec[5].x);
   float _364 = _363 * (c_light_col[1].x);
   float _365 = _363 * (c_light_col[1].y);
   float _366 = _363 * (c_light_col[1].z);
   float _367 = _359 * _194;
   float _368 = _360 * _195;
   float _369 = _361 * _196;
   float _370 = _364 * _267;
   float _371 = _365 * _267;
   float _372 = _366 * _267;
   float _373 = _370 + _367;
   float _374 = _371 + _368;
   float _375 = _372 + _369;
   float _382 = c_mat_specular.x * c_specular_scale.x;
   float _383 = _382 * _197;
   float _384 = _383 * _373;
   float _385 = c_mat_specular.y * c_specular_scale.y;
   float _386 = _385 * _198;
   float _387 = _386 * _374;
   float _388 = c_mat_specular.z * c_specular_scale.z;
   float _389 = _388 * _199;
   float _390 = _389 * _375;
   float _391 = _256 * _220;
   float _392 = _391 * _356;
   float _393 = _257 * _221;
   float _394 = _393 * _356;
   float _395 = _258 * _222;
   float _396 = _395 * _356;
   float _397 = _384 + _392;
   float _398 = _387 + _394;
   float _399 = _390 + _396;
   if (!_240)
   {
      float _401 = _182 * _164;
      float _402 = _182 * _165;
      float _403 = _182 * _166;
      float _404 = _318 + _401;
      float _405 = _319 + _402;
      float _406 = _320 + _403;
      float _407 = _182 * _167;
      float _408 = _397 + _407;
      float _409 = _398 + _407;
      float _410 = _399 + _407;
      _412 = _404;
      _413 = _405;
      _414 = _406;
      _415 = _408;
      _416 = _409;
      _417 = _410;
   }
   else
   {
      _412 = _318;
      _413 = _319;
      _414 = _320;
      _415 = _397;
      _416 = _398;
      _417 = _399;
   }
   int _418 = c_bool & 128;
   bool _419 = (_418 != 0);
   float _420 = select(_419, 0.0f, _415);
   float _421 = select(_419, 0.0f, _416);
   float _422 = select(_419, 0.0f, _417);
   float _423 = _412 * _77;
   float _424 = _413 * _78;
   float _425 = _414 * _79;
   float _426 = _420 * _57.x;
   float _427 = _421 * _57.y;
   float _428 = _422 * _57.z;
   int _429 = c_bool & 32768;
   bool _430 = (_429 == 0);
   if (!_430)
   {
      float _434 = (c_ps_user[11].x) * _77;
      float _435 = (c_ps_user[11].x) * _78;
      float _436 = (c_ps_user[11].x) * _79;
      float _438 = _82 + 1.0f;
      float _439 = _83 + 1.0f;
      float _440 = _84 + 1.0f;
      float _441 = _438 * 0.5f;
      float _442 = _439 * 0.5f;
      float _443 = _440 * 0.5f;
      float _444 = saturate(_441);
      float _445 = saturate(_442);
      float _446 = saturate(_443);
      float _447 = (c_ps_user[11].y) * _444;
      float _448 = _445 * (c_ps_user[11].y);
      float _449 = _446 * (c_ps_user[11].y);
      float _451 = (c_ps_user[11].z) * _57.x;
      float _452 = (c_ps_user[11].z) * _57.y;
      float _453 = (c_ps_user[11].z) * _57.z;
      float _455 = c_mat_power * 0.009999999776482582f;
      float _456 = _455 * (c_ps_user[11].w);
      float _459 = (c_ps_user[12].x) * _194;
      float _460 = (c_ps_user[12].x) * _195;
      float _461 = (c_ps_user[12].x) * _196;
      float _463 = (c_ps_user[12].y) * _197;
      float _464 = (c_ps_user[12].y) * _198;
      float _465 = (c_ps_user[12].y) * _199;
      float _467 = (c_ps_user[12].z) * _220;
      float _468 = (c_ps_user[12].z) * _221;
      float _469 = (c_ps_user[12].z) * _222;
      float _475 = -0.0f - _87;
      float _476 = -0.0f - _88;
      float _477 = -0.0f - _89;
      float _478 = dot(float3(_475, _476, _477), float3(_82, _83, _84));
      float _479 = 1.0f - _478;
      float _480 = _479 * _479;
      float _481 = _479 * 0.9722222089767456f;
      float _482 = _480 * _480;
      float _483 = _482 * _481;
      float _484 = _483 + 0.02777777425944805f;
      float _485 = _484 * (c_ps_user[13].z);
      float _487 = (c_ps_user[13].w) * c_scene_ambient.x;
      float _488 = (c_ps_user[13].w) * c_scene_ambient.y;
      float _489 = (c_ps_user[13].w) * c_scene_ambient.z;
      float _492 = (c_ps_user[14].x) * _234;
      float _493 = (c_ps_user[14].x) * _236;
      float _494 = (c_ps_user[14].x) * _238;
      float _496 = (c_ps_user[14].y) * _273;
      float _497 = (c_ps_user[14].y) * _274;
      float _498 = (c_ps_user[14].y) * _275;
      float _500 = (c_ps_user[14].z) * _280;
      float _501 = (c_ps_user[14].z) * _281;
      float _502 = (c_ps_user[14].z) * _282;
      float _504 = (c_ps_user[14].w) * _253;
      float _505 = (c_ps_user[14].w) * _254;
      float _506 = (c_ps_user[14].w) * _255;
      float _509 = (c_ps_user[15].x) * _359;
      float _510 = (c_ps_user[15].x) * _360;
      float _511 = (c_ps_user[15].x) * _361;
      float _513 = _382 * _364;
      float _514 = _513 * (c_ps_user[15].y);
      float _515 = _385 * _365;
      float _516 = _515 * (c_ps_user[15].y);
      float _517 = _388 * _366;
      float _518 = _517 * (c_ps_user[15].y);
      float _520 = (c_ps_user[15].z) * _256;
      float _521 = (c_ps_user[15].z) * _257;
      float _522 = (c_ps_user[15].z) * _258;
      float _523 = _514 + _520;
      float _524 = _523 * _356;
      float _525 = _496 + (c_ps_user[13].x);
      float _526 = _525 + _500;
      float _527 = c_diffuse_scale.x * _526;
      float _528 = _527 * c_mat_diffuse.x;
      float _529 = (c_ps_user[13].y) + _451;
      float _530 = _529 + _509;
      float _531 = c_mat_specular.x * _530;
      float _532 = _531 * c_specular_scale.x;
      float _533 = _447 + _434;
      float _534 = _533 + _456;
      float _535 = _534 + (c_ps_user[12].w);
      float _536 = _535 + _459;
      float _537 = _536 + _463;
      float _538 = _537 + _467;
      float _539 = _538 + _487;
      float _540 = _539 + _492;
      float _541 = _540 + _504;
      float _542 = _541 + _485;
      float _543 = _542 + _524;
      float _544 = _543 + _528;
      float _545 = _544 + _532;
      float _546 = _516 + _521;
      float _547 = _546 * _356;
      float _548 = _497 + (c_ps_user[13].x);
      float _549 = _548 + _501;
      float _550 = c_diffuse_scale.y * _549;
      float _551 = _550 * c_mat_diffuse.y;
      float _552 = (c_ps_user[13].y) + _452;
      float _553 = _552 + _510;
      float _554 = c_mat_specular.y * _553;
      float _555 = _554 * c_specular_scale.y;
      float _556 = _448 + _435;
      float _557 = _556 + _456;
      float _558 = _557 + (c_ps_user[12].w);
      float _559 = _558 + _460;
      float _560 = _559 + _464;
      float _561 = _560 + _468;
      float _562 = _561 + _488;
      float _563 = _562 + _493;
      float _564 = _563 + _505;
      float _565 = _564 + _485;
      float _566 = _565 + _547;
      float _567 = _566 + _551;
      float _568 = _567 + _555;
      float _569 = _518 + _522;
      float _570 = _569 * _356;
      float _571 = _498 + (c_ps_user[13].x);
      float _572 = _571 + _502;
      float _573 = c_diffuse_scale.z * _572;
      float _574 = _573 * c_mat_diffuse.z;
      float _575 = (c_ps_user[13].y) + _453;
      float _576 = _575 + _511;
      float _577 = c_mat_specular.z * _576;
      float _578 = _577 * c_specular_scale.z;
      float _579 = _456 + _436;
      float _580 = _579 + _449;
      float _581 = _580 + (c_ps_user[12].w);
      float _582 = _581 + _461;
      float _583 = _582 + _465;
      float _584 = _583 + _469;
      float _585 = _584 + _489;
      float _586 = _585 + _494;
      float _587 = _586 + _506;
      float _588 = _587 + _485;
      float _589 = _588 + _570;
      float _590 = _589 + _574;
      float _591 = _590 + _578;
      _593 = _545;
      _594 = _568;
      _595 = _591;
      _596 = 0.0f;
      _597 = 0.0f;
      _598 = 0.0f;
   }
   else
   {
      _593 = _423;
      _594 = _424;
      _595 = _425;
      _596 = _426;
      _597 = _427;
      _598 = _428;
   }
   float _599 = _596 + _593;
   float _600 = _597 + _594;
   float _601 = _598 + _595;
   float _606 = c_mat_opacity * _44.w;
   float _607 = _606 * c_global_opacity;
   float _608 = _607 * c_opacity_scale;
   int _609 = c_bool & 16;
   bool _610 = (_609 == 0);
   if (!_610)
   {
      float _614 = c_tone_control.x * _599;
      float _615 = c_tone_control.x * _600;
      float _616 = c_tone_control.x * _601;
      float _618 = log2(_614);
      float _619 = log2(_615);
      float _620 = log2(_616);
      float _621 = _618 * c_tone_control.y;
      float _622 = _619 * c_tone_control.y;
      float _623 = _620 * c_tone_control.y;
      float _624 = exp2(_621);
      float _625 = exp2(_622);
      float _626 = exp2(_623);
      _674 = _624;
      _675 = _625;
      _676 = _626;
   }
   else
   {
      if (_430)
      {
         float _631 = c_tone_control.x * _599;
         float _632 = c_tone_control.x * _600;
         float _633 = c_tone_control.x * _601;
         float _634 = _631 * 2.0f;
         float _635 = _632 * 2.0f;
         float _636 = _633 * 2.0f;
         float _637 = _631 * 0.30000001192092896f;
         float _638 = _632 * 0.30000001192092896f;
         float _639 = _633 * 0.30000001192092896f;
         float _640 = _637 + 0.05000000074505806f;
         float _641 = _638 + 0.05000000074505806f;
         float _642 = _639 + 0.05000000074505806f;
         float _643 = _640 * _634;
         float _644 = _641 * _635;
         float _645 = _642 * _636;
         float _646 = _643 + 0.004000000189989805f;
         float _647 = _644 + 0.004000000189989805f;
         float _648 = _645 + 0.004000000189989805f;
         float _649 = _637 + 0.5f;
         float _650 = _638 + 0.5f;
         float _651 = _639 + 0.5f;
         float _652 = _649 * _634;
         float _653 = _650 * _635;
         float _654 = _651 * _636;
         float _655 = _652 + 0.06000000238418579f;
         float _656 = _653 + 0.06000000238418579f;
         float _657 = _654 + 0.06000000238418579f;
         float _658 = _646 / _655;
         float _659 = _647 / _656;
         float _660 = _648 / _657;
         float _661 = _658 + -0.06666666269302368f;
         float _662 = _659 + -0.06666666269302368f;
         float _663 = _660 + -0.06666666269302368f;
         float _664 = _661 * 1.3790643215179443f;
         float _665 = _662 * 1.3790643215179443f;
         float _666 = _663 * 1.3790643215179443f;
         float _667 = YKR_MaterialCurve(_664, _631);
         float _668 = YKR_MaterialCurve(_665, _632);
         float _669 = YKR_MaterialCurve(_666, _633);
         float _670 = sqrt(_667);
         float _671 = sqrt(_668);
         float _672 = sqrt(_669);
         _674 = _670;
         _675 = _671;
         _676 = _672;
      }
      else
      {
         _674 = _599;
         _675 = _600;
         _676 = _601;
      }
   }
   int _677 = c_bool & 4;
   bool _678 = (_677 == 0);
   if (_678)
   {
      float _680 = saturate(COLOR_1.x);
      float _685 = (c_fog_param[0].x) - _674;
      float _686 = (c_fog_param[0].y) - _675;
      float _687 = (c_fog_param[0].z) - _676;
      float _688 = _685 * _680;
      float _689 = _686 * _680;
      float _690 = _687 * _680;
      _692 = _688;
      _693 = _689;
      _694 = _690;
   }
   else
   {
      _692 = COLOR_1.x;
      _693 = COLOR_1.y;
      _694 = COLOR_1.z;
   }
   float _695 = _692 + _674;
   float _696 = _693 + _675;
   float _697 = _694 + _676;
   float _703 = (c_ps_col_scl[0].x) * _695;
   float _704 = (c_ps_col_scl[0].y) * _696;
   float _705 = (c_ps_col_scl[0].z) * _697;
   float _706 = (c_ps_col_scl[0].w) * _608;
   SV_Target.x = _703;
   SV_Target.y = _704;
   SV_Target.z = _705;
   SV_Target.w = _706;
   int _709 = c_bool & 8;
   bool _710 = (_709 != 0);
   float _711 = select(_710, _695, _703);
   float _712 = select(_710, _696, _704);
   float _713 = select(_710, _697, _705);
   float _714 = select(_710, _608, _706);
   SV_Target.x = _711;
   SV_Target.y = _712;
   SV_Target.z = _713;
   SV_Target.w = _714;
   return SV_Target;
}
