// Yakuza Kiwami material r_o1m_m(vr)2m (0x8C7C318E, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
// applied by a script: the inline Hable tonemap's saturate goes through
// YKR_MaterialCurve (tangent continuation in HDR), and the light pre-pass buffer t14 through YKR_LightBuffer.

#include "Includes/Common.hlsl"

Texture2D<float4> s0_t : register(t0);

Texture2D<float4> s1_t : register(t1);

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
   float4 _42 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _47 = _42.x * _42.x;
   float _48 = _42.y * _42.y;
   float _49 = _42.z * _42.z;
   float4 _50 = s1_t.Sample(s1_s, float2(TEXCOORD.z, TEXCOORD.w));
   float _55 = _50.x * _50.x;
   float _56 = _50.y * _50.y;
   float _57 = _50.z * _50.z;
   float _58 = 1.0f - _55;
   float _59 = 1.0f - _56;
   float _60 = 1.0f - _57;
   float _61 = _58 * COLOR_1.w;
   float _62 = _59 * COLOR_1.w;
   float _63 = _60 * COLOR_1.w;
   float _64 = 1.0f - _61;
   float _65 = 1.0f - _62;
   float _66 = 1.0f - _63;
   float _67 = _47 * _64;
   float _68 = _48 * _65;
   float _69 = _49 * _66;
   float _70 = 1.0f - _50.w;
   float _71 = _70 * COLOR_1.w;
   float _72 = 1.0f - _71;
   float _73 = _72 * _42.w;
   float _74 = dot(float3(TEXCOORD_2.x, TEXCOORD_2.y, TEXCOORD_2.z), float3(TEXCOORD_2.x, TEXCOORD_2.y, TEXCOORD_2.z));
   float _75 = rsqrt(_74);
   float _76 = _75 * TEXCOORD_2.x;
   float _77 = _75 * TEXCOORD_2.y;
   float _78 = _75 * TEXCOORD_2.z;
   float _79 = dot(float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z), float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z));
   float _80 = rsqrt(_79);
   float _81 = _80 * TEXCOORD_3.x;
   float _82 = _80 * TEXCOORD_3.y;
   float _83 = _80 * TEXCOORD_3.z;
   float _88 = -0.0f - (c_light_vec[0].x);
   float _89 = -0.0f - (c_light_vec[0].y);
   float _90 = -0.0f - (c_light_vec[0].z);
   float _91 = dot(float3(_76, _77, _78), float3(_88, _89, _90));
   float _95 = c_rt_size.z * SV_Position.x;
   float _96 = c_rt_size.w * SV_Position.y;
   int _99 = c_bool & 8;
   bool _100 = (_99 == 0);
   float _107;
   float _108;
   float _109;
   float _121;
   float _122;
   float _158;
   float _159;
   float _160;
   float _161;
   float _175;
   float _176;
   float _211;
   float _212;
   float _213;
   float _247;
   float _248;
   float _249;
   float _250;
   float _251;
   float _252;
   float _406;
   float _407;
   float _408;
   float _409;
   float _410;
   float _411;
   float _583;
   float _584;
   float _585;
   float _586;
   float _587;
   float _588;
   float _663;
   float _664;
   float _665;
   float _681;
   float _682;
   float _683;
   if (!_100)
   {
      float4 _102 = s13_t.Sample(s13_s, float2(_95, _96));
      _107 = _102.x;
      _108 = _102.y;
      _109 = _102.w;
   }
   else
   {
      _107 = 1.0f;
      _108 = 1.0f;
      _109 = 1.0f;
   }
   if (!_100)
   {
      float _114 = (c_sdw_param[0].x) * _107;
      float _115 = (c_sdw_param[0].z) * _108;
      float _118 = _114 + (c_sdw_param[0].y);
      float _119 = _115 + (c_sdw_param[0].w);
      _121 = _118;
      _122 = _119;
   }
   else
   {
      _121 = _107;
      _122 = _108;
   }
   float _126 = c_rt_size.z * SV_Position.x;
   float _127 = c_rt_size.w * SV_Position.y;
   float4 _128 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_126, _127)));
   int _135 = c_bool & 16384;
   bool _136 = (_135 == 0);
   if (!_136)
   {
      float _138 = _128.x * 2.0f;
      float _139 = _128.y * 2.0f;
      float _140 = _128.z * 2.0f;
      float _141 = _128.w * 2.0f;
      _158 = _138;
      _159 = _139;
      _160 = _140;
      _161 = _141;
   }
   else
   {
      int _143 = c_bool & 8192;
      bool _144 = (_143 == 0);
      if (!_144)
      {
         float _146 = _128.x * 2.0f;
         float _147 = _128.y * 2.0f;
         float _148 = _128.z * 2.0f;
         float _149 = _128.w * 2.0f;
         _158 = _146;
         _159 = _147;
         _160 = _148;
         _161 = _149;
      }
      else
      {
         float _153 = _128.x / c_tone_control.z;
         float _154 = _128.y / c_tone_control.z;
         float _155 = _128.z / c_tone_control.z;
         float _156 = _128.w / c_tone_control.z;
         _158 = _153;
         _159 = _154;
         _160 = _155;
         _161 = _156;
      }
   }
   int _162 = c_bool & 8;
   bool _163 = (_162 == 0);
   if (!_163)
   {
      float _168 = (c_sdw_param[0].x) * _121;
      float _169 = (c_sdw_param[0].z) * _122;
      float _172 = _168 + (c_sdw_param[0].y);
      float _173 = _169 + (c_sdw_param[0].w);
      _175 = _172;
      _176 = _173;
   }
   else
   {
      _175 = _121;
      _176 = _122;
   }
   float _177 = _176 * _175;
   float _182 = 1.0f - (c_light_col[5].x);
   float _183 = 1.0f - (c_light_col[5].y);
   float _184 = 1.0f - (c_light_col[5].z);
   float _185 = _182 * _177;
   float _186 = _183 * _177;
   float _187 = _184 * _177;
   float _188 = _185 + (c_light_col[5].x);
   float _189 = _186 + (c_light_col[5].y);
   float _190 = _187 + (c_light_col[5].z);
   float _191 = _109 * COLOR.x;
   float _192 = _109 * COLOR.y;
   float _193 = _109 * COLOR.z;
   if (!_163)
   {
      float _197 = (c_sdw_param[2].x) * _188;
      float _198 = (c_sdw_param[2].x) * _189;
      float _199 = (c_sdw_param[2].x) * _190;
      float _201 = _197 + (c_sdw_param[2].y);
      float _202 = _198 + (c_sdw_param[2].y);
      float _203 = _199 + (c_sdw_param[2].y);
      float _206 = (c_sdw_param[2].w) + (c_sdw_param[2].z);
      float _207 = _201 * _206;
      float _208 = _202 * _206;
      float _209 = _203 * _206;
      _211 = _207;
      _212 = _208;
      _213 = _209;
   }
   else
   {
      _211 = _188;
      _212 = _189;
      _213 = _190;
   }
   float _214 = _211 * _109;
   float _215 = _212 * _109;
   float _216 = _213 * _109;
   float _227 = c_mat_emissive.x * _67;
   float _228 = _227 * c_emissive_scale;
   float _229 = c_mat_emissive.y * _68;
   float _230 = _229 * c_emissive_scale;
   float _231 = c_mat_emissive.z * _69;
   float _232 = _231 * c_emissive_scale;
   int _233 = c_bool & 2048;
   bool _234 = (_233 == 0);
   if (_234)
   {
      float _236 = _176 * _158;
      float _237 = _176 * _159;
      float _238 = _176 * _160;
      float _239 = _236 + TEXCOORD_4.x;
      float _240 = _237 + TEXCOORD_4.y;
      float _241 = _238 + TEXCOORD_4.z;
      float _242 = _176 * _161;
      float _243 = _242 + TEXCOORD_5.x;
      float _244 = _242 + TEXCOORD_5.y;
      float _245 = _242 + TEXCOORD_5.z;
      _247 = _243;
      _248 = _244;
      _249 = _245;
      _250 = _239;
      _251 = _240;
      _252 = _241;
   }
   else
   {
      _247 = TEXCOORD_5.x;
      _248 = TEXCOORD_5.y;
      _249 = TEXCOORD_5.z;
      _250 = TEXCOORD_4.x;
      _251 = TEXCOORD_4.y;
      _252 = TEXCOORD_4.z;
   }
   float _257 = -0.0f - (c_light_vec[1].x);
   float _258 = -0.0f - (c_light_vec[1].y);
   float _259 = -0.0f - (c_light_vec[1].z);
   float _260 = dot(float3(_76, _77, _78), float3(_257, _258, _259));
   float _261 = saturate(_260);
   float _266 = saturate(_91);
   float _267 = (c_light_col[0].x) * _266;
   float _268 = (c_light_col[0].y) * _266;
   float _269 = (c_light_col[0].z) * _266;
   float _274 = (c_light_col[1].x) * _261;
   float _275 = (c_light_col[1].y) * _261;
   float _276 = (c_light_col[1].z) * _261;
   float _285 = c_diffuse_scale.x * c_mat_diffuse.x;
   float _286 = c_diffuse_scale.y * c_mat_diffuse.y;
   float _287 = c_diffuse_scale.z * c_mat_diffuse.z;
   float _288 = _267 * _188;
   float _289 = _268 * _189;
   float _290 = _269 * _190;
   float _291 = _274 * _176;
   float _292 = _275 * _176;
   float _293 = _276 * _176;
   float _294 = _291 + _288;
   float _295 = _292 + _289;
   float _296 = _293 + _290;
   float _297 = _285 * _294;
   float _298 = _286 * _295;
   float _299 = _287 * _296;
   float _300 = _228 + c_scene_ambient.x;
   float _301 = _300 + _297;
   float _302 = _230 + c_scene_ambient.y;
   float _303 = _302 + _298;
   float _304 = _232 + c_scene_ambient.z;
   float _305 = _304 + _299;
   float _306 = _301 * _191;
   float _307 = _303 * _192;
   float _308 = _305 * _193;
   float _309 = _250 * _214;
   float _310 = _251 * _215;
   float _311 = _252 * _216;
   float _312 = _306 + _309;
   float _313 = _307 + _310;
   float _314 = _308 + _311;
   float _319 = (c_light_vec[0].x) + _81;
   float _320 = (c_light_vec[0].y) + _82;
   float _321 = (c_light_vec[0].z) + _83;
   float _322 = dot(float3(_319, _320, _321), float3(_319, _320, _321));
   float _323 = rsqrt(_322);
   float _324 = _319 * _323;
   float _325 = -0.0f - _324;
   float _326 = _320 * _323;
   float _327 = -0.0f - _326;
   float _328 = _321 * _323;
   float _329 = -0.0f - _328;
   float _330 = dot(float3(_325, _327, _329), float3(_76, _77, _78));
   float _331 = saturate(_330);
   float _332 = _82 + -0.30000001192092896f;
   float _333 = dot(float3(_81, _332, _83), float3(_81, _332, _83));
   float _334 = rsqrt(_333);
   float _335 = _81 * _334;
   float _336 = -0.0f - _335;
   float _337 = _332 * _334;
   float _338 = -0.0f - _337;
   float _339 = _83 * _334;
   float _340 = -0.0f - _339;
   float _341 = dot(float3(_336, _338, _340), float3(_76, _77, _78));
   float _342 = saturate(_341);
   float _345 = log2(_331);
   float _346 = log2(_342);
   float _347 = _345 * c_mat_power;
   float _348 = _346 * c_mat_power;
   float _349 = exp2(_347);
   float _350 = exp2(_348);
   float _353 = _349 * (c_light_col[0].x);
   float _354 = _349 * (c_light_col[0].y);
   float _355 = _349 * (c_light_col[0].z);
   float _356 = _350 * 2.0f;
   float _357 = _356 * (c_light_vec[5].x);
   float _358 = _357 * (c_light_col[1].x);
   float _359 = _357 * (c_light_col[1].y);
   float _360 = _357 * (c_light_col[1].z);
   float _361 = _353 * _188;
   float _362 = _354 * _189;
   float _363 = _355 * _190;
   float _364 = _358 * _261;
   float _365 = _359 * _261;
   float _366 = _360 * _261;
   float _367 = _364 + _361;
   float _368 = _365 + _362;
   float _369 = _366 + _363;
   float _376 = c_mat_specular.x * c_specular_scale.x;
   float _377 = _376 * _191;
   float _378 = _377 * _367;
   float _379 = c_mat_specular.y * c_specular_scale.y;
   float _380 = _379 * _192;
   float _381 = _380 * _368;
   float _382 = c_mat_specular.z * c_specular_scale.z;
   float _383 = _382 * _193;
   float _384 = _383 * _369;
   float _385 = _247 * _214;
   float _386 = _385 * _350;
   float _387 = _248 * _215;
   float _388 = _387 * _350;
   float _389 = _249 * _216;
   float _390 = _389 * _350;
   float _391 = _378 + _386;
   float _392 = _381 + _388;
   float _393 = _384 + _390;
   if (!_234)
   {
      float _395 = _176 * _158;
      float _396 = _176 * _159;
      float _397 = _176 * _160;
      float _398 = _312 + _395;
      float _399 = _313 + _396;
      float _400 = _314 + _397;
      float _401 = _176 * _161;
      float _402 = _391 + _401;
      float _403 = _392 + _401;
      float _404 = _393 + _401;
      _406 = _398;
      _407 = _399;
      _408 = _400;
      _409 = _402;
      _410 = _403;
      _411 = _404;
   }
   else
   {
      _406 = _312;
      _407 = _313;
      _408 = _314;
      _409 = _391;
      _410 = _392;
      _411 = _393;
   }
   int _412 = c_bool & 128;
   bool _413 = (_412 != 0);
   float _414 = select(_413, 0.0f, _409);
   float _415 = select(_413, 0.0f, _410);
   float _416 = select(_413, 0.0f, _411);
   float _417 = _406 * _67;
   float _418 = _407 * _68;
   float _419 = _408 * _69;
   float _420 = _414 * _73;
   float _421 = _415 * _73;
   float _422 = _416 * _73;
   int _423 = c_bool & 32768;
   bool _424 = (_423 == 0);
   if (!_424)
   {
      float _428 = (c_ps_user[11].x) * _67;
      float _429 = (c_ps_user[11].x) * _68;
      float _430 = (c_ps_user[11].x) * _69;
      float _432 = _76 + 1.0f;
      float _433 = _77 + 1.0f;
      float _434 = _78 + 1.0f;
      float _435 = _432 * 0.5f;
      float _436 = _433 * 0.5f;
      float _437 = _434 * 0.5f;
      float _438 = saturate(_435);
      float _439 = saturate(_436);
      float _440 = saturate(_437);
      float _441 = (c_ps_user[11].y) * _438;
      float _442 = _439 * (c_ps_user[11].y);
      float _443 = _440 * (c_ps_user[11].y);
      float _445 = (c_ps_user[11].z) * _73;
      float _447 = c_mat_power * 0.009999999776482582f;
      float _448 = _447 * (c_ps_user[11].w);
      float _451 = (c_ps_user[12].x) * _188;
      float _452 = (c_ps_user[12].x) * _189;
      float _453 = (c_ps_user[12].x) * _190;
      float _455 = (c_ps_user[12].y) * _191;
      float _456 = (c_ps_user[12].y) * _192;
      float _457 = (c_ps_user[12].y) * _193;
      float _459 = (c_ps_user[12].z) * _214;
      float _460 = (c_ps_user[12].z) * _215;
      float _461 = (c_ps_user[12].z) * _216;
      float _467 = -0.0f - _81;
      float _468 = -0.0f - _82;
      float _469 = -0.0f - _83;
      float _470 = dot(float3(_467, _468, _469), float3(_76, _77, _78));
      float _471 = 1.0f - _470;
      float _472 = _471 * _471;
      float _473 = _471 * 0.9722222089767456f;
      float _474 = _472 * _472;
      float _475 = _474 * _473;
      float _476 = _475 + 0.02777777425944805f;
      float _477 = _476 * (c_ps_user[13].z);
      float _479 = (c_ps_user[13].w) * c_scene_ambient.x;
      float _480 = (c_ps_user[13].w) * c_scene_ambient.y;
      float _481 = (c_ps_user[13].w) * c_scene_ambient.z;
      float _484 = (c_ps_user[14].x) * _228;
      float _485 = (c_ps_user[14].x) * _230;
      float _486 = (c_ps_user[14].x) * _232;
      float _488 = (c_ps_user[14].y) * _267;
      float _489 = (c_ps_user[14].y) * _268;
      float _490 = (c_ps_user[14].y) * _269;
      float _492 = (c_ps_user[14].z) * _274;
      float _493 = (c_ps_user[14].z) * _275;
      float _494 = (c_ps_user[14].z) * _276;
      float _496 = (c_ps_user[14].w) * _250;
      float _497 = (c_ps_user[14].w) * _251;
      float _498 = (c_ps_user[14].w) * _252;
      float _501 = (c_ps_user[15].x) * _353;
      float _502 = (c_ps_user[15].x) * _354;
      float _503 = (c_ps_user[15].x) * _355;
      float _505 = _376 * _358;
      float _506 = _505 * (c_ps_user[15].y);
      float _507 = _379 * _359;
      float _508 = _507 * (c_ps_user[15].y);
      float _509 = _382 * _360;
      float _510 = _509 * (c_ps_user[15].y);
      float _512 = (c_ps_user[15].z) * _247;
      float _513 = (c_ps_user[15].z) * _248;
      float _514 = (c_ps_user[15].z) * _249;
      float _515 = _506 + _512;
      float _516 = _515 * _350;
      float _517 = _488 + (c_ps_user[13].x);
      float _518 = _517 + _492;
      float _519 = c_diffuse_scale.x * _518;
      float _520 = _519 * c_mat_diffuse.x;
      float _521 = (c_ps_user[13].y) + _445;
      float _522 = _521 + _501;
      float _523 = c_mat_specular.x * _522;
      float _524 = _523 * c_specular_scale.x;
      float _525 = _441 + _428;
      float _526 = _525 + _448;
      float _527 = _526 + (c_ps_user[12].w);
      float _528 = _527 + _451;
      float _529 = _528 + _455;
      float _530 = _529 + _459;
      float _531 = _530 + _479;
      float _532 = _531 + _484;
      float _533 = _532 + _496;
      float _534 = _533 + _477;
      float _535 = _534 + _516;
      float _536 = _535 + _520;
      float _537 = _536 + _524;
      float _538 = _508 + _513;
      float _539 = _538 * _350;
      float _540 = _489 + (c_ps_user[13].x);
      float _541 = _540 + _493;
      float _542 = c_diffuse_scale.y * _541;
      float _543 = _542 * c_mat_diffuse.y;
      float _544 = _521 + _502;
      float _545 = c_mat_specular.y * _544;
      float _546 = _545 * c_specular_scale.y;
      float _547 = _442 + _429;
      float _548 = _547 + _448;
      float _549 = _548 + (c_ps_user[12].w);
      float _550 = _549 + _452;
      float _551 = _550 + _456;
      float _552 = _551 + _460;
      float _553 = _552 + _480;
      float _554 = _553 + _485;
      float _555 = _554 + _497;
      float _556 = _555 + _477;
      float _557 = _556 + _539;
      float _558 = _557 + _543;
      float _559 = _558 + _546;
      float _560 = _510 + _514;
      float _561 = _560 * _350;
      float _562 = _490 + (c_ps_user[13].x);
      float _563 = _562 + _494;
      float _564 = c_diffuse_scale.z * _563;
      float _565 = _564 * c_mat_diffuse.z;
      float _566 = _521 + _503;
      float _567 = c_mat_specular.z * _566;
      float _568 = _567 * c_specular_scale.z;
      float _569 = _448 + _430;
      float _570 = _569 + _443;
      float _571 = _570 + (c_ps_user[12].w);
      float _572 = _571 + _453;
      float _573 = _572 + _457;
      float _574 = _573 + _461;
      float _575 = _574 + _481;
      float _576 = _575 + _486;
      float _577 = _576 + _498;
      float _578 = _577 + _477;
      float _579 = _578 + _561;
      float _580 = _579 + _565;
      float _581 = _580 + _568;
      _583 = _537;
      _584 = _559;
      _585 = _581;
      _586 = 0.0f;
      _587 = 0.0f;
      _588 = 0.0f;
   }
   else
   {
      _583 = _417;
      _584 = _418;
      _585 = _419;
      _586 = _420;
      _587 = _421;
      _588 = _422;
   }
   float _589 = _586 + _583;
   float _590 = _587 + _584;
   float _591 = _588 + _585;
   float _596 = c_global_opacity * c_mat_opacity;
   float _597 = _596 * c_opacity_scale;
   int _598 = c_bool & 16;
   bool _599 = (_598 == 0);
   if (!_599)
   {
      float _603 = c_tone_control.x * _589;
      float _604 = c_tone_control.x * _590;
      float _605 = c_tone_control.x * _591;
      float _607 = log2(_603);
      float _608 = log2(_604);
      float _609 = log2(_605);
      float _610 = _607 * c_tone_control.y;
      float _611 = _608 * c_tone_control.y;
      float _612 = _609 * c_tone_control.y;
      float _613 = exp2(_610);
      float _614 = exp2(_611);
      float _615 = exp2(_612);
      _663 = _613;
      _664 = _614;
      _665 = _615;
   }
   else
   {
      if (_424)
      {
         float _620 = c_tone_control.x * _589;
         float _621 = c_tone_control.x * _590;
         float _622 = c_tone_control.x * _591;
         float _623 = _620 * 2.0f;
         float _624 = _621 * 2.0f;
         float _625 = _622 * 2.0f;
         float _626 = _620 * 0.30000001192092896f;
         float _627 = _621 * 0.30000001192092896f;
         float _628 = _622 * 0.30000001192092896f;
         float _629 = _626 + 0.05000000074505806f;
         float _630 = _627 + 0.05000000074505806f;
         float _631 = _628 + 0.05000000074505806f;
         float _632 = _629 * _623;
         float _633 = _630 * _624;
         float _634 = _631 * _625;
         float _635 = _632 + 0.004000000189989805f;
         float _636 = _633 + 0.004000000189989805f;
         float _637 = _634 + 0.004000000189989805f;
         float _638 = _626 + 0.5f;
         float _639 = _627 + 0.5f;
         float _640 = _628 + 0.5f;
         float _641 = _638 * _623;
         float _642 = _639 * _624;
         float _643 = _640 * _625;
         float _644 = _641 + 0.06000000238418579f;
         float _645 = _642 + 0.06000000238418579f;
         float _646 = _643 + 0.06000000238418579f;
         float _647 = _635 / _644;
         float _648 = _636 / _645;
         float _649 = _637 / _646;
         float _650 = _647 + -0.06666666269302368f;
         float _651 = _648 + -0.06666666269302368f;
         float _652 = _649 + -0.06666666269302368f;
         float _653 = _650 * 1.3790643215179443f;
         float _654 = _651 * 1.3790643215179443f;
         float _655 = _652 * 1.3790643215179443f;
         float _656 = YKR_MaterialCurve(_653, _620);
         float _657 = YKR_MaterialCurve(_654, _621);
         float _658 = YKR_MaterialCurve(_655, _622);
         float _659 = sqrt(_656);
         float _660 = sqrt(_657);
         float _661 = sqrt(_658);
         _663 = _659;
         _664 = _660;
         _665 = _661;
      }
      else
      {
         _663 = _589;
         _664 = _590;
         _665 = _591;
      }
   }
   int _666 = c_bool & 4;
   bool _667 = (_666 == 0);
   if (_667)
   {
      float _669 = saturate(COLOR_1.x);
      float _674 = (c_fog_param[0].x) - _663;
      float _675 = (c_fog_param[0].y) - _664;
      float _676 = (c_fog_param[0].z) - _665;
      float _677 = _674 * _669;
      float _678 = _675 * _669;
      float _679 = _676 * _669;
      _681 = _677;
      _682 = _678;
      _683 = _679;
   }
   else
   {
      _681 = COLOR_1.x;
      _682 = COLOR_1.y;
      _683 = COLOR_1.z;
   }
   float _684 = _681 + _663;
   float _685 = _682 + _664;
   float _686 = _683 + _665;
   float _692 = (c_ps_col_scl[0].x) * _684;
   float _693 = (c_ps_col_scl[0].y) * _685;
   float _694 = (c_ps_col_scl[0].z) * _686;
   float _695 = (c_ps_col_scl[0].w) * _597;
   SV_Target.x = _692;
   SV_Target.y = _693;
   SV_Target.z = _694;
   SV_Target.w = _695;
   int _698 = c_bool & 8;
   bool _699 = (_698 != 0);
   float _700 = select(_699, _684, _692);
   float _701 = select(_699, _685, _693);
   float _702 = select(_699, _686, _694);
   float _703 = select(_699, _597, _695);
   SV_Target.x = _700;
   SV_Target.y = _701;
   SV_Target.z = _702;
   SV_Target.w = _703;
   return SV_Target;
}
