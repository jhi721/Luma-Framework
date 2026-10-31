// Yakuza Kiwami material r_o1mt_m4d[ref] (0xCAE2C48A, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
// applied by a script: the inline Hable tonemap's saturate goes through
// YKR_MaterialCurve (tangent continuation in HDR), and the light pre-pass buffer t14 through YKR_LightBuffer.

#include "Includes/Common.hlsl"

Texture2D<float4> s0_t : register(t0);

Texture2D<float4> s5_t : register(t5);

TextureCube<float4> s6c_t : register(t6);

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

SamplerState s5_s : register(s5);

SamplerState s6c_s : register(s6);

SamplerState s7_s : register(s7);

SamplerState s13_s : register(s13);

SamplerState s14_s : register(s14);

float4 main(
    linear float4 TEXCOORD : TEXCOORD,
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
   float4 _50 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _55 = _50.x * _50.x;
   float _56 = _50.y * _50.y;
   float _57 = _50.z * _50.z;
   float _61 = (c_attribute_ps[0].z) * TEXCOORD.x;
   float _62 = (c_attribute_ps[0].w) * TEXCOORD.y;
   float4 _63 = s5_t.Sample(s5_s, float2(_61, _62));
   float4 _66 = s7_t.Sample(s7_s, float2(TEXCOORD.z, TEXCOORD.w));
   float _69 = _63.x * 2.0f;
   float _70 = _63.y * 2.0f;
   float _71 = _69 + -1.0f;
   float _72 = _70 + -1.0f;
   float _73 = dot(float2(_71, _72), float2(_71, _72));
   float _74 = 1.0f - _73;
   float _75 = saturate(_74);
   float _76 = sqrt(_75);
   float _77 = TEXCOORD_2.y * TEXCOORD_6.z;
   float _78 = TEXCOORD_2.z * TEXCOORD_6.y;
   float _79 = _77 - _78;
   float _80 = TEXCOORD_2.z * TEXCOORD_6.x;
   float _81 = TEXCOORD_2.x * TEXCOORD_6.z;
   float _82 = _80 - _81;
   float _83 = TEXCOORD_2.x * TEXCOORD_6.y;
   float _84 = TEXCOORD_2.y * TEXCOORD_6.x;
   float _85 = _83 - _84;
   float _86 = _71 * TEXCOORD_6.x;
   float _87 = _71 * TEXCOORD_6.y;
   float _88 = _71 * TEXCOORD_6.z;
   float _89 = _72 * TEXCOORD_6.w;
   float _90 = _89 * _79;
   float _91 = _89 * _82;
   float _92 = _89 * _85;
   float _93 = _90 + _86;
   float _94 = _91 + _87;
   float _95 = _92 + _88;
   float _96 = _76 * TEXCOORD_2.x;
   float _97 = _76 * TEXCOORD_2.y;
   float _98 = _76 * TEXCOORD_2.z;
   float _99 = _93 + _96;
   float _100 = _94 + _97;
   float _101 = _95 + _98;
   float _102 = dot(float3(_99, _100, _101), float3(_99, _100, _101));
   float _103 = rsqrt(_102);
   float _104 = _99 * _103;
   float _105 = _100 * _103;
   float _106 = _101 * _103;
   float _107 = dot(float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z), float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z));
   float _108 = rsqrt(_107);
   float _109 = _108 * TEXCOORD_3.x;
   float _110 = _108 * TEXCOORD_3.y;
   float _111 = _108 * TEXCOORD_3.z;
   float _116 = -0.0f - (c_light_vec[0].x);
   float _117 = -0.0f - (c_light_vec[0].y);
   float _118 = -0.0f - (c_light_vec[0].z);
   float _119 = dot(float3(_104, _105, _106), float3(_116, _117, _118));
   float _123 = c_rt_size.z * SV_Position.x;
   float _124 = c_rt_size.w * SV_Position.y;
   int _127 = c_bool & 8;
   bool _128 = (_127 == 0);
   float _135;
   float _136;
   float _137;
   float _149;
   float _150;
   float _205;
   float _206;
   float _207;
   float _208;
   float _222;
   float _223;
   float _269;
   float _270;
   float _271;
   float _306;
   float _307;
   float _308;
   float _309;
   float _310;
   float _311;
   float _465;
   float _466;
   float _467;
   float _468;
   float _469;
   float _470;
   float _642;
   float _643;
   float _644;
   float _645;
   float _646;
   float _647;
   float _769;
   float _770;
   float _771;
   float _787;
   float _788;
   float _789;
   if (!_128)
   {
      float4 _130 = s13_t.Sample(s13_s, float2(_123, _124));
      _135 = _130.x;
      _136 = _130.y;
      _137 = _130.w;
   }
   else
   {
      _135 = 1.0f;
      _136 = 1.0f;
      _137 = 1.0f;
   }
   if (!_128)
   {
      float _142 = (c_sdw_param[0].x) * _135;
      float _143 = (c_sdw_param[0].z) * _136;
      float _146 = _142 + (c_sdw_param[0].y);
      float _147 = _143 + (c_sdw_param[0].w);
      _149 = _146;
      _150 = _147;
   }
   else
   {
      _149 = _135;
      _150 = _136;
   }
   float _157 = (c_sdw_param[1].x) * _66.y;
   float _159 = _157 + (c_sdw_param[1].y);
   float _160 = _159 * _66.x;
   float _161 = 1.0f - (c_light_col[5].x);
   float _162 = 1.0f - (c_light_col[5].y);
   float _163 = 1.0f - (c_light_col[5].z);
   float _164 = _160 * _161;
   float _165 = _160 * _162;
   float _166 = _160 * _163;
   float _167 = _164 + (c_light_col[5].x);
   float _168 = _165 + (c_light_col[5].y);
   float _169 = _166 + (c_light_col[5].z);
   float _173 = c_rt_size.z * SV_Position.x;
   float _174 = c_rt_size.w * SV_Position.y;
   float4 _175 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_173, _174)));
   int _182 = c_bool & 16384;
   bool _183 = (_182 == 0);
   if (!_183)
   {
      float _185 = _175.x * 2.0f;
      float _186 = _175.y * 2.0f;
      float _187 = _175.z * 2.0f;
      float _188 = _175.w * 2.0f;
      _205 = _185;
      _206 = _186;
      _207 = _187;
      _208 = _188;
   }
   else
   {
      int _190 = c_bool & 8192;
      bool _191 = (_190 == 0);
      if (!_191)
      {
         float _193 = _175.x * 2.0f;
         float _194 = _175.y * 2.0f;
         float _195 = _175.z * 2.0f;
         float _196 = _175.w * 2.0f;
         _205 = _193;
         _206 = _194;
         _207 = _195;
         _208 = _196;
      }
      else
      {
         float _200 = _175.x / c_tone_control.z;
         float _201 = _175.y / c_tone_control.z;
         float _202 = _175.z / c_tone_control.z;
         float _203 = _175.w / c_tone_control.z;
         _205 = _200;
         _206 = _201;
         _207 = _202;
         _208 = _203;
      }
   }
   int _209 = c_bool & 8;
   bool _210 = (_209 == 0);
   if (!_210)
   {
      float _215 = (c_sdw_param[0].x) * _149;
      float _216 = (c_sdw_param[0].z) * _150;
      float _219 = _215 + (c_sdw_param[0].y);
      float _220 = _216 + (c_sdw_param[0].w);
      _222 = _219;
      _223 = _220;
   }
   else
   {
      _222 = _149;
      _223 = _150;
   }
   float _224 = _223 * _222;
   float _229 = 1.0f - (c_light_col[5].x);
   float _230 = 1.0f - (c_light_col[5].y);
   float _231 = 1.0f - (c_light_col[5].z);
   float _232 = _229 * _224;
   float _233 = _230 * _224;
   float _234 = _231 * _224;
   float _235 = _232 + (c_light_col[5].x);
   float _236 = _233 + (c_light_col[5].y);
   float _237 = _234 + (c_light_col[5].z);
   float _238 = _137 * COLOR.x;
   float _239 = _238 * _167;
   float _240 = _137 * COLOR.y;
   float _241 = _240 * _168;
   float _242 = _137 * COLOR.z;
   float _243 = _242 * _169;
   float _244 = _235 * _167;
   float _245 = _236 * _168;
   float _246 = _237 * _169;
   if (!_210)
   {
      float _250 = (c_sdw_param[2].x) * _235;
      float _251 = (c_sdw_param[2].x) * _236;
      float _252 = (c_sdw_param[2].x) * _237;
      float _254 = _250 + (c_sdw_param[2].y);
      float _255 = _251 + (c_sdw_param[2].y);
      float _256 = _252 + (c_sdw_param[2].y);
      float _258 = (c_sdw_param[2].z) * _167;
      float _259 = (c_sdw_param[2].z) * _168;
      float _260 = (c_sdw_param[2].z) * _169;
      float _262 = _258 + (c_sdw_param[2].w);
      float _263 = _259 + (c_sdw_param[2].w);
      float _264 = _260 + (c_sdw_param[2].w);
      float _265 = _262 * _254;
      float _266 = _263 * _255;
      float _267 = _264 * _256;
      _269 = _265;
      _270 = _266;
      _271 = _267;
   }
   else
   {
      _269 = _244;
      _270 = _245;
      _271 = _246;
   }
   float _272 = _137 * _66.x;
   float _273 = _269 * _272;
   float _274 = _270 * _272;
   float _275 = _271 * _272;
   float _286 = c_mat_emissive.x * _55;
   float _287 = _286 * c_emissive_scale;
   float _288 = c_mat_emissive.y * _56;
   float _289 = _288 * c_emissive_scale;
   float _290 = c_mat_emissive.z * _57;
   float _291 = _290 * c_emissive_scale;
   int _292 = c_bool & 2048;
   bool _293 = (_292 == 0);
   if (_293)
   {
      float _295 = _223 * _205;
      float _296 = _223 * _206;
      float _297 = _223 * _207;
      float _298 = _295 + TEXCOORD_4.x;
      float _299 = _296 + TEXCOORD_4.y;
      float _300 = _297 + TEXCOORD_4.z;
      float _301 = _223 * _208;
      float _302 = _301 + TEXCOORD_5.x;
      float _303 = _301 + TEXCOORD_5.y;
      float _304 = _301 + TEXCOORD_5.z;
      _306 = _298;
      _307 = _299;
      _308 = _300;
      _309 = _302;
      _310 = _303;
      _311 = _304;
   }
   else
   {
      _306 = TEXCOORD_4.x;
      _307 = TEXCOORD_4.y;
      _308 = TEXCOORD_4.z;
      _309 = TEXCOORD_5.x;
      _310 = TEXCOORD_5.y;
      _311 = TEXCOORD_5.z;
   }
   float _316 = -0.0f - (c_light_vec[1].x);
   float _317 = -0.0f - (c_light_vec[1].y);
   float _318 = -0.0f - (c_light_vec[1].z);
   float _319 = dot(float3(_104, _105, _106), float3(_316, _317, _318));
   float _320 = saturate(_319);
   float _325 = saturate(_119);
   float _326 = (c_light_col[0].x) * _325;
   float _327 = (c_light_col[0].y) * _325;
   float _328 = (c_light_col[0].z) * _325;
   float _333 = (c_light_col[1].x) * _320;
   float _334 = (c_light_col[1].y) * _320;
   float _335 = (c_light_col[1].z) * _320;
   float _344 = c_diffuse_scale.x * c_mat_diffuse.x;
   float _345 = c_diffuse_scale.y * c_mat_diffuse.y;
   float _346 = c_diffuse_scale.z * c_mat_diffuse.z;
   float _347 = _326 * _235;
   float _348 = _327 * _236;
   float _349 = _328 * _237;
   float _350 = _333 * _223;
   float _351 = _334 * _223;
   float _352 = _335 * _223;
   float _353 = _350 + _347;
   float _354 = _351 + _348;
   float _355 = _352 + _349;
   float _356 = _344 * _353;
   float _357 = _345 * _354;
   float _358 = _346 * _355;
   float _359 = _287 + c_scene_ambient.x;
   float _360 = _359 + _356;
   float _361 = _289 + c_scene_ambient.y;
   float _362 = _361 + _357;
   float _363 = _291 + c_scene_ambient.z;
   float _364 = _363 + _358;
   float _365 = _360 * _239;
   float _366 = _362 * _241;
   float _367 = _364 * _243;
   float _368 = _306 * _273;
   float _369 = _307 * _274;
   float _370 = _308 * _275;
   float _371 = _365 + _368;
   float _372 = _366 + _369;
   float _373 = _367 + _370;
   float _378 = (c_light_vec[0].x) + _109;
   float _379 = (c_light_vec[0].y) + _110;
   float _380 = (c_light_vec[0].z) + _111;
   float _381 = dot(float3(_378, _379, _380), float3(_378, _379, _380));
   float _382 = rsqrt(_381);
   float _383 = _378 * _382;
   float _384 = -0.0f - _383;
   float _385 = _379 * _382;
   float _386 = -0.0f - _385;
   float _387 = _380 * _382;
   float _388 = -0.0f - _387;
   float _389 = dot(float3(_384, _386, _388), float3(_104, _105, _106));
   float _390 = saturate(_389);
   float _391 = _110 + -0.30000001192092896f;
   float _392 = dot(float3(_109, _391, _111), float3(_109, _391, _111));
   float _393 = rsqrt(_392);
   float _394 = _109 * _393;
   float _395 = -0.0f - _394;
   float _396 = _391 * _393;
   float _397 = -0.0f - _396;
   float _398 = _111 * _393;
   float _399 = -0.0f - _398;
   float _400 = dot(float3(_395, _397, _399), float3(_104, _105, _106));
   float _401 = saturate(_400);
   float _404 = log2(_390);
   float _405 = log2(_401);
   float _406 = _404 * c_mat_power;
   float _407 = _405 * c_mat_power;
   float _408 = exp2(_406);
   float _409 = exp2(_407);
   float _412 = _408 * (c_light_col[0].x);
   float _413 = _408 * (c_light_col[0].y);
   float _414 = _408 * (c_light_col[0].z);
   float _415 = _409 * 2.0f;
   float _416 = _415 * (c_light_vec[5].x);
   float _417 = _416 * (c_light_col[1].x);
   float _418 = _416 * (c_light_col[1].y);
   float _419 = _416 * (c_light_col[1].z);
   float _420 = _412 * _235;
   float _421 = _413 * _236;
   float _422 = _414 * _237;
   float _423 = _417 * _320;
   float _424 = _418 * _320;
   float _425 = _419 * _320;
   float _426 = _423 + _420;
   float _427 = _424 + _421;
   float _428 = _425 + _422;
   float _435 = c_mat_specular.x * c_specular_scale.x;
   float _436 = _435 * _239;
   float _437 = _436 * _426;
   float _438 = c_mat_specular.y * c_specular_scale.y;
   float _439 = _438 * _241;
   float _440 = _439 * _427;
   float _441 = c_mat_specular.z * c_specular_scale.z;
   float _442 = _441 * _243;
   float _443 = _442 * _428;
   float _444 = _309 * _273;
   float _445 = _444 * _409;
   float _446 = _310 * _274;
   float _447 = _446 * _409;
   float _448 = _311 * _275;
   float _449 = _448 * _409;
   float _450 = _437 + _445;
   float _451 = _440 + _447;
   float _452 = _443 + _449;
   if (!_293)
   {
      float _454 = _223 * _205;
      float _455 = _223 * _206;
      float _456 = _223 * _207;
      float _457 = _371 + _454;
      float _458 = _372 + _455;
      float _459 = _373 + _456;
      float _460 = _223 * _208;
      float _461 = _450 + _460;
      float _462 = _451 + _460;
      float _463 = _452 + _460;
      _465 = _457;
      _466 = _458;
      _467 = _459;
      _468 = _461;
      _469 = _462;
      _470 = _463;
   }
   else
   {
      _465 = _371;
      _466 = _372;
      _467 = _373;
      _468 = _450;
      _469 = _451;
      _470 = _452;
   }
   int _471 = c_bool & 128;
   bool _472 = (_471 != 0);
   float _473 = select(_472, 0.0f, _468);
   float _474 = select(_472, 0.0f, _469);
   float _475 = select(_472, 0.0f, _470);
   float _476 = _465 * _55;
   float _477 = _466 * _56;
   float _478 = _467 * _57;
   float _479 = _473 * _50.w;
   float _480 = _474 * _50.w;
   float _481 = _475 * _50.w;
   int _482 = c_bool & 32768;
   bool _483 = (_482 == 0);
   if (!_483)
   {
      float _487 = (c_ps_user[11].x) * _55;
      float _488 = (c_ps_user[11].x) * _56;
      float _489 = (c_ps_user[11].x) * _57;
      float _491 = _104 + 1.0f;
      float _492 = _105 + 1.0f;
      float _493 = _106 + 1.0f;
      float _494 = _491 * 0.5f;
      float _495 = _492 * 0.5f;
      float _496 = _493 * 0.5f;
      float _497 = saturate(_494);
      float _498 = saturate(_495);
      float _499 = saturate(_496);
      float _500 = (c_ps_user[11].y) * _497;
      float _501 = _498 * (c_ps_user[11].y);
      float _502 = _499 * (c_ps_user[11].y);
      float _504 = (c_ps_user[11].z) * _50.w;
      float _506 = c_mat_power * 0.009999999776482582f;
      float _507 = _506 * (c_ps_user[11].w);
      float _510 = (c_ps_user[12].x) * _235;
      float _511 = (c_ps_user[12].x) * _236;
      float _512 = (c_ps_user[12].x) * _237;
      float _514 = (c_ps_user[12].y) * _239;
      float _515 = (c_ps_user[12].y) * _241;
      float _516 = (c_ps_user[12].y) * _243;
      float _518 = (c_ps_user[12].z) * _273;
      float _519 = (c_ps_user[12].z) * _274;
      float _520 = (c_ps_user[12].z) * _275;
      float _526 = -0.0f - _109;
      float _527 = -0.0f - _110;
      float _528 = -0.0f - _111;
      float _529 = dot(float3(_526, _527, _528), float3(_104, _105, _106));
      float _530 = 1.0f - _529;
      float _531 = _530 * _530;
      float _532 = _530 * 0.9722222089767456f;
      float _533 = _531 * _531;
      float _534 = _533 * _532;
      float _535 = _534 + 0.02777777425944805f;
      float _536 = _535 * (c_ps_user[13].z);
      float _538 = (c_ps_user[13].w) * c_scene_ambient.x;
      float _539 = (c_ps_user[13].w) * c_scene_ambient.y;
      float _540 = (c_ps_user[13].w) * c_scene_ambient.z;
      float _543 = (c_ps_user[14].x) * _287;
      float _544 = (c_ps_user[14].x) * _289;
      float _545 = (c_ps_user[14].x) * _291;
      float _547 = (c_ps_user[14].y) * _326;
      float _548 = (c_ps_user[14].y) * _327;
      float _549 = (c_ps_user[14].y) * _328;
      float _551 = (c_ps_user[14].z) * _333;
      float _552 = (c_ps_user[14].z) * _334;
      float _553 = (c_ps_user[14].z) * _335;
      float _555 = (c_ps_user[14].w) * _306;
      float _556 = (c_ps_user[14].w) * _307;
      float _557 = (c_ps_user[14].w) * _308;
      float _560 = (c_ps_user[15].x) * _412;
      float _561 = (c_ps_user[15].x) * _413;
      float _562 = (c_ps_user[15].x) * _414;
      float _564 = _435 * _417;
      float _565 = _564 * (c_ps_user[15].y);
      float _566 = _438 * _418;
      float _567 = _566 * (c_ps_user[15].y);
      float _568 = _441 * _419;
      float _569 = _568 * (c_ps_user[15].y);
      float _571 = (c_ps_user[15].z) * _309;
      float _572 = (c_ps_user[15].z) * _310;
      float _573 = (c_ps_user[15].z) * _311;
      float _574 = _565 + _571;
      float _575 = _574 * _409;
      float _576 = _547 + (c_ps_user[13].x);
      float _577 = _576 + _551;
      float _578 = c_diffuse_scale.x * _577;
      float _579 = _578 * c_mat_diffuse.x;
      float _580 = (c_ps_user[13].y) + _504;
      float _581 = _580 + _560;
      float _582 = c_mat_specular.x * _581;
      float _583 = _582 * c_specular_scale.x;
      float _584 = _500 + _487;
      float _585 = _584 + _507;
      float _586 = _585 + (c_ps_user[12].w);
      float _587 = _586 + _510;
      float _588 = _587 + _514;
      float _589 = _588 + _518;
      float _590 = _589 + _538;
      float _591 = _590 + _543;
      float _592 = _591 + _555;
      float _593 = _592 + _536;
      float _594 = _593 + _575;
      float _595 = _594 + _579;
      float _596 = _595 + _583;
      float _597 = _567 + _572;
      float _598 = _597 * _409;
      float _599 = _548 + (c_ps_user[13].x);
      float _600 = _599 + _552;
      float _601 = c_diffuse_scale.y * _600;
      float _602 = _601 * c_mat_diffuse.y;
      float _603 = _580 + _561;
      float _604 = c_mat_specular.y * _603;
      float _605 = _604 * c_specular_scale.y;
      float _606 = _501 + _488;
      float _607 = _606 + _507;
      float _608 = _607 + (c_ps_user[12].w);
      float _609 = _608 + _511;
      float _610 = _609 + _515;
      float _611 = _610 + _519;
      float _612 = _611 + _539;
      float _613 = _612 + _544;
      float _614 = _613 + _556;
      float _615 = _614 + _536;
      float _616 = _615 + _598;
      float _617 = _616 + _602;
      float _618 = _617 + _605;
      float _619 = _569 + _573;
      float _620 = _619 * _409;
      float _621 = _549 + (c_ps_user[13].x);
      float _622 = _621 + _553;
      float _623 = c_diffuse_scale.z * _622;
      float _624 = _623 * c_mat_diffuse.z;
      float _625 = _580 + _562;
      float _626 = c_mat_specular.z * _625;
      float _627 = _626 * c_specular_scale.z;
      float _628 = _507 + _489;
      float _629 = _628 + _502;
      float _630 = _629 + (c_ps_user[12].w);
      float _631 = _630 + _512;
      float _632 = _631 + _516;
      float _633 = _632 + _520;
      float _634 = _633 + _540;
      float _635 = _634 + _545;
      float _636 = _635 + _557;
      float _637 = _636 + _536;
      float _638 = _637 + _620;
      float _639 = _638 + _624;
      float _640 = _639 + _627;
      _642 = _596;
      _643 = _618;
      _644 = _640;
      _645 = 0.0f;
      _646 = 0.0f;
      _647 = 0.0f;
   }
   else
   {
      _642 = _476;
      _643 = _477;
      _644 = _478;
      _645 = _479;
      _646 = _480;
      _647 = _481;
   }
   float _648 = _645 + _642;
   float _649 = _646 + _643;
   float _650 = _647 + _644;
   float _651 = dot(float3(_109, _110, _111), float3(_104, _105, _106));
   float _652 = _651 * 2.0f;
   float _653 = _652 * _104;
   float _654 = _652 * _105;
   float _655 = _652 * _106;
   float _656 = _109 - _653;
   float _657 = _110 - _654;
   float _658 = _111 - _655;
   float _659 = -0.0f - _656;
   float4 _660 = s6c_t.Sample(s6c_s, float3(_659, _657, _658));
   float _667 = _150 * _149;
   float _668 = _119 + 0.75f;
   float _669 = saturate(_668);
   float _670 = _667 * _669;
   float _671 = max(_670, (c_light_vec[5].y));
   float _672 = max(_660.w, 0.003921568859368563f);
   float _673 = 0.25f / _672;
   float _674 = _673 * _660.x;
   float _675 = _673 * _660.y;
   float _676 = _673 * _660.z;
   float _677 = _50.w * TEXCOORD_2.w;
   float _678 = _677 * _66.x;
   float _679 = _671 * _678;
   float _680 = _679 * _167;
   float _681 = _674 * _674;
   float _682 = _681 * _680;
   float _683 = _679 * _168;
   float _684 = _675 * _675;
   float _685 = _684 * _683;
   float _686 = _679 * _169;
   float _687 = _676 * _676;
   float _688 = _687 * _686;
   float _689 = _648 + _682;
   float _690 = _649 + _685;
   float _691 = _650 + _688;
   float _696 = c_opacity_scale * c_mat_opacity;
   float _699 = _696 * c_global_opacity;
   int _702 = c_bool & 16;
   bool _703 = (_702 == 0);
   if (!_703)
   {
      float _707 = c_tone_control.x * _689;
      float _708 = c_tone_control.x * _690;
      float _709 = c_tone_control.x * _691;
      float _711 = log2(_707);
      float _712 = log2(_708);
      float _713 = log2(_709);
      float _714 = _711 * c_tone_control.y;
      float _715 = _712 * c_tone_control.y;
      float _716 = _713 * c_tone_control.y;
      float _717 = exp2(_714);
      float _718 = exp2(_715);
      float _719 = exp2(_716);
      _769 = _717;
      _770 = _718;
      _771 = _719;
   }
   else
   {
      int _721 = c_bool & 32768;
      bool _722 = (_721 == 0);
      if (_722)
      {
         float _726 = c_tone_control.x * _689;
         float _727 = c_tone_control.x * _690;
         float _728 = c_tone_control.x * _691;
         float _729 = _726 * 2.0f;
         float _730 = _727 * 2.0f;
         float _731 = _728 * 2.0f;
         float _732 = _726 * 0.30000001192092896f;
         float _733 = _727 * 0.30000001192092896f;
         float _734 = _728 * 0.30000001192092896f;
         float _735 = _732 + 0.05000000074505806f;
         float _736 = _733 + 0.05000000074505806f;
         float _737 = _734 + 0.05000000074505806f;
         float _738 = _735 * _729;
         float _739 = _736 * _730;
         float _740 = _737 * _731;
         float _741 = _738 + 0.004000000189989805f;
         float _742 = _739 + 0.004000000189989805f;
         float _743 = _740 + 0.004000000189989805f;
         float _744 = _732 + 0.5f;
         float _745 = _733 + 0.5f;
         float _746 = _734 + 0.5f;
         float _747 = _744 * _729;
         float _748 = _745 * _730;
         float _749 = _746 * _731;
         float _750 = _747 + 0.06000000238418579f;
         float _751 = _748 + 0.06000000238418579f;
         float _752 = _749 + 0.06000000238418579f;
         float _753 = _741 / _750;
         float _754 = _742 / _751;
         float _755 = _743 / _752;
         float _756 = _753 + -0.06666666269302368f;
         float _757 = _754 + -0.06666666269302368f;
         float _758 = _755 + -0.06666666269302368f;
         float _759 = _756 * 1.3790643215179443f;
         float _760 = _757 * 1.3790643215179443f;
         float _761 = _758 * 1.3790643215179443f;
         float _762 = YKR_MaterialCurve(_759, _726);
         float _763 = YKR_MaterialCurve(_760, _727);
         float _764 = YKR_MaterialCurve(_761, _728);
         float _765 = sqrt(_762);
         float _766 = sqrt(_763);
         float _767 = sqrt(_764);
         _769 = _765;
         _770 = _766;
         _771 = _767;
      }
      else
      {
         _769 = _689;
         _770 = _690;
         _771 = _691;
      }
   }
   int _772 = c_bool & 4;
   bool _773 = (_772 == 0);
   if (_773)
   {
      float _775 = saturate(COLOR_1.x);
      float _780 = (c_fog_param[0].x) - _769;
      float _781 = (c_fog_param[0].y) - _770;
      float _782 = (c_fog_param[0].z) - _771;
      float _783 = _780 * _775;
      float _784 = _781 * _775;
      float _785 = _782 * _775;
      _787 = _783;
      _788 = _784;
      _789 = _785;
   }
   else
   {
      _787 = COLOR_1.x;
      _788 = COLOR_1.y;
      _789 = COLOR_1.z;
   }
   float _790 = _787 + _769;
   float _791 = _788 + _770;
   float _792 = _789 + _771;
   float _798 = (c_ps_col_scl[0].x) * _790;
   float _799 = (c_ps_col_scl[0].y) * _791;
   float _800 = (c_ps_col_scl[0].z) * _792;
   float _801 = (c_ps_col_scl[0].w) * _699;
   SV_Target.x = _798;
   SV_Target.y = _799;
   SV_Target.z = _800;
   SV_Target.w = _801;
   int _804 = c_bool & 8;
   bool _805 = (_804 != 0);
   float _806 = select(_805, _790, _798);
   float _807 = select(_805, _791, _799);
   float _808 = select(_805, _792, _800);
   float _809 = select(_805, _699, _801);
   SV_Target.x = _806;
   SV_Target.y = _807;
   SV_Target.z = _808;
   SV_Target.w = _809;
   return SV_Target;
}
