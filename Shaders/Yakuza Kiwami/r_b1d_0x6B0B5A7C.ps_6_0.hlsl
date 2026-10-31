// Yakuza Kiwami material r_b1d (0x6B0B5A7C, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
// applied by a script: the inline Hable tonemap's saturate goes through
// YKR_MaterialCurve (tangent continuation in HDR), and the light pre-pass buffer t14 through YKR_LightBuffer.

#include "Includes/Common.hlsl"

Texture2D<float4> s0_t : register(t0);

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
   float4 _38 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _43 = _38.x * _38.x;
   float _44 = _38.y * _38.y;
   float _45 = _38.z * _38.z;
   float _46 = dot(float3(TEXCOORD_2.x, TEXCOORD_2.y, TEXCOORD_2.z), float3(TEXCOORD_2.x, TEXCOORD_2.y, TEXCOORD_2.z));
   float _47 = rsqrt(_46);
   float _48 = _47 * TEXCOORD_2.x;
   float _49 = _47 * TEXCOORD_2.y;
   float _50 = _47 * TEXCOORD_2.z;
   float _51 = dot(float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z), float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z));
   float _52 = rsqrt(_51);
   float _53 = _52 * TEXCOORD_3.x;
   float _54 = _52 * TEXCOORD_3.y;
   float _55 = _52 * TEXCOORD_3.z;
   float _60 = -0.0f - (c_light_vec[0].x);
   float _61 = -0.0f - (c_light_vec[0].y);
   float _62 = -0.0f - (c_light_vec[0].z);
   float _63 = dot(float3(_48, _49, _50), float3(_60, _61, _62));
   float _67 = c_rt_size.z * SV_Position.x;
   float _68 = c_rt_size.w * SV_Position.y;
   int _71 = c_bool & 8;
   bool _72 = (_71 == 0);
   float _79;
   float _80;
   float _81;
   float _93;
   float _94;
   float _130;
   float _131;
   float _132;
   float _133;
   float _147;
   float _148;
   float _183;
   float _184;
   float _185;
   float _219;
   float _220;
   float _221;
   float _222;
   float _223;
   float _224;
   float _378;
   float _379;
   float _380;
   float _381;
   float _382;
   float _383;
   float _551;
   float _552;
   float _553;
   float _554;
   float _555;
   float _556;
   float _633;
   float _634;
   float _635;
   float _651;
   float _652;
   float _653;
   if (!_72)
   {
      float4 _74 = s13_t.Sample(s13_s, float2(_67, _68));
      _79 = _74.x;
      _80 = _74.y;
      _81 = _74.w;
   }
   else
   {
      _79 = 1.0f;
      _80 = 1.0f;
      _81 = 1.0f;
   }
   if (!_72)
   {
      float _86 = (c_sdw_param[0].x) * _79;
      float _87 = (c_sdw_param[0].z) * _80;
      float _90 = _86 + (c_sdw_param[0].y);
      float _91 = _87 + (c_sdw_param[0].w);
      _93 = _90;
      _94 = _91;
   }
   else
   {
      _93 = _79;
      _94 = _80;
   }
   float _98 = c_rt_size.z * SV_Position.x;
   float _99 = c_rt_size.w * SV_Position.y;
   float4 _100 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_98, _99)));
   int _107 = c_bool & 16384;
   bool _108 = (_107 == 0);
   if (!_108)
   {
      float _110 = _100.x * 2.0f;
      float _111 = _100.y * 2.0f;
      float _112 = _100.z * 2.0f;
      float _113 = _100.w * 2.0f;
      _130 = _110;
      _131 = _111;
      _132 = _112;
      _133 = _113;
   }
   else
   {
      int _115 = c_bool & 8192;
      bool _116 = (_115 == 0);
      if (!_116)
      {
         float _118 = _100.x * 2.0f;
         float _119 = _100.y * 2.0f;
         float _120 = _100.z * 2.0f;
         float _121 = _100.w * 2.0f;
         _130 = _118;
         _131 = _119;
         _132 = _120;
         _133 = _121;
      }
      else
      {
         float _125 = _100.x / c_tone_control.z;
         float _126 = _100.y / c_tone_control.z;
         float _127 = _100.z / c_tone_control.z;
         float _128 = _100.w / c_tone_control.z;
         _130 = _125;
         _131 = _126;
         _132 = _127;
         _133 = _128;
      }
   }
   int _134 = c_bool & 8;
   bool _135 = (_134 == 0);
   if (!_135)
   {
      float _140 = (c_sdw_param[0].x) * _93;
      float _141 = (c_sdw_param[0].z) * _94;
      float _144 = _140 + (c_sdw_param[0].y);
      float _145 = _141 + (c_sdw_param[0].w);
      _147 = _144;
      _148 = _145;
   }
   else
   {
      _147 = _93;
      _148 = _94;
   }
   float _149 = _148 * _147;
   float _154 = 1.0f - (c_light_col[5].x);
   float _155 = 1.0f - (c_light_col[5].y);
   float _156 = 1.0f - (c_light_col[5].z);
   float _157 = _154 * _149;
   float _158 = _155 * _149;
   float _159 = _156 * _149;
   float _160 = _157 + (c_light_col[5].x);
   float _161 = _158 + (c_light_col[5].y);
   float _162 = _159 + (c_light_col[5].z);
   float _163 = _81 * COLOR.x;
   float _164 = _81 * COLOR.y;
   float _165 = _81 * COLOR.z;
   if (!_135)
   {
      float _169 = (c_sdw_param[2].x) * _160;
      float _170 = (c_sdw_param[2].x) * _161;
      float _171 = (c_sdw_param[2].x) * _162;
      float _173 = _169 + (c_sdw_param[2].y);
      float _174 = _170 + (c_sdw_param[2].y);
      float _175 = _171 + (c_sdw_param[2].y);
      float _178 = (c_sdw_param[2].w) + (c_sdw_param[2].z);
      float _179 = _173 * _178;
      float _180 = _174 * _178;
      float _181 = _175 * _178;
      _183 = _179;
      _184 = _180;
      _185 = _181;
   }
   else
   {
      _183 = _160;
      _184 = _161;
      _185 = _162;
   }
   float _186 = _183 * _81;
   float _187 = _184 * _81;
   float _188 = _185 * _81;
   float _199 = c_mat_emissive.x * _43;
   float _200 = _199 * c_emissive_scale;
   float _201 = c_mat_emissive.y * _44;
   float _202 = _201 * c_emissive_scale;
   float _203 = c_mat_emissive.z * _45;
   float _204 = _203 * c_emissive_scale;
   int _205 = c_bool & 2048;
   bool _206 = (_205 == 0);
   if (_206)
   {
      float _208 = _148 * _130;
      float _209 = _148 * _131;
      float _210 = _148 * _132;
      float _211 = _208 + TEXCOORD_4.x;
      float _212 = _209 + TEXCOORD_4.y;
      float _213 = _210 + TEXCOORD_4.z;
      float _214 = _148 * _133;
      float _215 = _214 + TEXCOORD_5.x;
      float _216 = _214 + TEXCOORD_5.y;
      float _217 = _214 + TEXCOORD_5.z;
      _219 = _215;
      _220 = _216;
      _221 = _217;
      _222 = _211;
      _223 = _212;
      _224 = _213;
   }
   else
   {
      _219 = TEXCOORD_5.x;
      _220 = TEXCOORD_5.y;
      _221 = TEXCOORD_5.z;
      _222 = TEXCOORD_4.x;
      _223 = TEXCOORD_4.y;
      _224 = TEXCOORD_4.z;
   }
   float _229 = -0.0f - (c_light_vec[1].x);
   float _230 = -0.0f - (c_light_vec[1].y);
   float _231 = -0.0f - (c_light_vec[1].z);
   float _232 = dot(float3(_48, _49, _50), float3(_229, _230, _231));
   float _233 = saturate(_232);
   float _238 = saturate(_63);
   float _239 = (c_light_col[0].x) * _238;
   float _240 = (c_light_col[0].y) * _238;
   float _241 = (c_light_col[0].z) * _238;
   float _246 = (c_light_col[1].x) * _233;
   float _247 = (c_light_col[1].y) * _233;
   float _248 = (c_light_col[1].z) * _233;
   float _257 = c_diffuse_scale.x * c_mat_diffuse.x;
   float _258 = c_diffuse_scale.y * c_mat_diffuse.y;
   float _259 = c_diffuse_scale.z * c_mat_diffuse.z;
   float _260 = _239 * _160;
   float _261 = _240 * _161;
   float _262 = _241 * _162;
   float _263 = _246 * _148;
   float _264 = _247 * _148;
   float _265 = _248 * _148;
   float _266 = _263 + _260;
   float _267 = _264 + _261;
   float _268 = _265 + _262;
   float _269 = _257 * _266;
   float _270 = _258 * _267;
   float _271 = _259 * _268;
   float _272 = _200 + c_scene_ambient.x;
   float _273 = _272 + _269;
   float _274 = _202 + c_scene_ambient.y;
   float _275 = _274 + _270;
   float _276 = _204 + c_scene_ambient.z;
   float _277 = _276 + _271;
   float _278 = _273 * _163;
   float _279 = _275 * _164;
   float _280 = _277 * _165;
   float _281 = _222 * _186;
   float _282 = _223 * _187;
   float _283 = _224 * _188;
   float _284 = _278 + _281;
   float _285 = _279 + _282;
   float _286 = _280 + _283;
   float _291 = (c_light_vec[0].x) + _53;
   float _292 = (c_light_vec[0].y) + _54;
   float _293 = (c_light_vec[0].z) + _55;
   float _294 = dot(float3(_291, _292, _293), float3(_291, _292, _293));
   float _295 = rsqrt(_294);
   float _296 = _291 * _295;
   float _297 = -0.0f - _296;
   float _298 = _292 * _295;
   float _299 = -0.0f - _298;
   float _300 = _293 * _295;
   float _301 = -0.0f - _300;
   float _302 = dot(float3(_297, _299, _301), float3(_48, _49, _50));
   float _303 = saturate(_302);
   float _304 = _54 + -0.30000001192092896f;
   float _305 = dot(float3(_53, _304, _55), float3(_53, _304, _55));
   float _306 = rsqrt(_305);
   float _307 = _53 * _306;
   float _308 = -0.0f - _307;
   float _309 = _304 * _306;
   float _310 = -0.0f - _309;
   float _311 = _55 * _306;
   float _312 = -0.0f - _311;
   float _313 = dot(float3(_308, _310, _312), float3(_48, _49, _50));
   float _314 = saturate(_313);
   float _317 = log2(_303);
   float _318 = log2(_314);
   float _319 = _317 * c_mat_power;
   float _320 = _318 * c_mat_power;
   float _321 = exp2(_319);
   float _322 = exp2(_320);
   float _325 = _321 * (c_light_col[0].x);
   float _326 = _321 * (c_light_col[0].y);
   float _327 = _321 * (c_light_col[0].z);
   float _328 = _322 * 2.0f;
   float _329 = _328 * (c_light_vec[5].x);
   float _330 = _329 * (c_light_col[1].x);
   float _331 = _329 * (c_light_col[1].y);
   float _332 = _329 * (c_light_col[1].z);
   float _333 = _325 * _160;
   float _334 = _326 * _161;
   float _335 = _327 * _162;
   float _336 = _330 * _233;
   float _337 = _331 * _233;
   float _338 = _332 * _233;
   float _339 = _336 + _333;
   float _340 = _337 + _334;
   float _341 = _338 + _335;
   float _348 = c_mat_specular.x * c_specular_scale.x;
   float _349 = _348 * _163;
   float _350 = _349 * _339;
   float _351 = c_mat_specular.y * c_specular_scale.y;
   float _352 = _351 * _164;
   float _353 = _352 * _340;
   float _354 = c_mat_specular.z * c_specular_scale.z;
   float _355 = _354 * _165;
   float _356 = _355 * _341;
   float _357 = _219 * _186;
   float _358 = _357 * _322;
   float _359 = _220 * _187;
   float _360 = _359 * _322;
   float _361 = _221 * _188;
   float _362 = _361 * _322;
   float _363 = _350 + _358;
   float _364 = _353 + _360;
   float _365 = _356 + _362;
   if (!_206)
   {
      float _367 = _148 * _130;
      float _368 = _148 * _131;
      float _369 = _148 * _132;
      float _370 = _284 + _367;
      float _371 = _285 + _368;
      float _372 = _286 + _369;
      float _373 = _148 * _133;
      float _374 = _363 + _373;
      float _375 = _364 + _373;
      float _376 = _365 + _373;
      _378 = _370;
      _379 = _371;
      _380 = _372;
      _381 = _374;
      _382 = _375;
      _383 = _376;
   }
   else
   {
      _378 = _284;
      _379 = _285;
      _380 = _286;
      _381 = _363;
      _382 = _364;
      _383 = _365;
   }
   int _384 = c_bool & 128;
   bool _385 = (_384 != 0);
   float _386 = select(_385, 0.0f, _381);
   float _387 = select(_385, 0.0f, _382);
   float _388 = select(_385, 0.0f, _383);
   float _389 = _378 * _43;
   float _390 = _379 * _44;
   float _391 = _380 * _45;
   int _392 = c_bool & 32768;
   bool _393 = (_392 == 0);
   if (!_393)
   {
      float _397 = (c_ps_user[11].x) * _43;
      float _398 = (c_ps_user[11].x) * _44;
      float _399 = (c_ps_user[11].x) * _45;
      float _401 = _48 + 1.0f;
      float _402 = _49 + 1.0f;
      float _403 = _50 + 1.0f;
      float _404 = _401 * 0.5f;
      float _405 = _402 * 0.5f;
      float _406 = _403 * 0.5f;
      float _407 = saturate(_404);
      float _408 = saturate(_405);
      float _409 = saturate(_406);
      float _410 = (c_ps_user[11].y) * _407;
      float _411 = _408 * (c_ps_user[11].y);
      float _412 = _409 * (c_ps_user[11].y);
      float _415 = c_mat_power * 0.009999999776482582f;
      float _416 = _415 * (c_ps_user[11].w);
      float _419 = (c_ps_user[12].x) * _160;
      float _420 = (c_ps_user[12].x) * _161;
      float _421 = (c_ps_user[12].x) * _162;
      float _423 = (c_ps_user[12].y) * _163;
      float _424 = (c_ps_user[12].y) * _164;
      float _425 = (c_ps_user[12].y) * _165;
      float _427 = (c_ps_user[12].z) * _186;
      float _428 = (c_ps_user[12].z) * _187;
      float _429 = (c_ps_user[12].z) * _188;
      float _435 = -0.0f - _53;
      float _436 = -0.0f - _54;
      float _437 = -0.0f - _55;
      float _438 = dot(float3(_435, _436, _437), float3(_48, _49, _50));
      float _439 = 1.0f - _438;
      float _440 = _439 * _439;
      float _441 = _439 * 0.9722222089767456f;
      float _442 = _440 * _440;
      float _443 = _442 * _441;
      float _444 = _443 + 0.02777777425944805f;
      float _445 = _444 * (c_ps_user[13].z);
      float _447 = (c_ps_user[13].w) * c_scene_ambient.x;
      float _448 = (c_ps_user[13].w) * c_scene_ambient.y;
      float _449 = (c_ps_user[13].w) * c_scene_ambient.z;
      float _452 = (c_ps_user[14].x) * _200;
      float _453 = (c_ps_user[14].x) * _202;
      float _454 = (c_ps_user[14].x) * _204;
      float _456 = (c_ps_user[14].y) * _239;
      float _457 = (c_ps_user[14].y) * _240;
      float _458 = (c_ps_user[14].y) * _241;
      float _460 = (c_ps_user[14].z) * _246;
      float _461 = (c_ps_user[14].z) * _247;
      float _462 = (c_ps_user[14].z) * _248;
      float _464 = (c_ps_user[14].w) * _222;
      float _465 = (c_ps_user[14].w) * _223;
      float _466 = (c_ps_user[14].w) * _224;
      float _469 = (c_ps_user[15].x) * _325;
      float _470 = (c_ps_user[15].x) * _326;
      float _471 = (c_ps_user[15].x) * _327;
      float _473 = _348 * _330;
      float _474 = _473 * (c_ps_user[15].y);
      float _475 = _351 * _331;
      float _476 = _475 * (c_ps_user[15].y);
      float _477 = _354 * _332;
      float _478 = _477 * (c_ps_user[15].y);
      float _480 = (c_ps_user[15].z) * _219;
      float _481 = (c_ps_user[15].z) * _220;
      float _482 = (c_ps_user[15].z) * _221;
      float _483 = _474 + _480;
      float _484 = _483 * _322;
      float _485 = _456 + (c_ps_user[13].x);
      float _486 = _485 + _460;
      float _487 = c_diffuse_scale.x * _486;
      float _488 = _487 * c_mat_diffuse.x;
      float _489 = (c_ps_user[13].y) + (c_ps_user[11].z);
      float _490 = _489 + _469;
      float _491 = c_mat_specular.x * _490;
      float _492 = _491 * c_specular_scale.x;
      float _493 = _410 + _397;
      float _494 = _493 + _416;
      float _495 = _494 + (c_ps_user[12].w);
      float _496 = _495 + _419;
      float _497 = _496 + _423;
      float _498 = _497 + _427;
      float _499 = _498 + _447;
      float _500 = _499 + _452;
      float _501 = _500 + _464;
      float _502 = _501 + _445;
      float _503 = _502 + _484;
      float _504 = _503 + _488;
      float _505 = _504 + _492;
      float _506 = _476 + _481;
      float _507 = _506 * _322;
      float _508 = _457 + (c_ps_user[13].x);
      float _509 = _508 + _461;
      float _510 = c_diffuse_scale.y * _509;
      float _511 = _510 * c_mat_diffuse.y;
      float _512 = _489 + _470;
      float _513 = c_mat_specular.y * _512;
      float _514 = _513 * c_specular_scale.y;
      float _515 = _411 + _398;
      float _516 = _515 + _416;
      float _517 = _516 + (c_ps_user[12].w);
      float _518 = _517 + _420;
      float _519 = _518 + _424;
      float _520 = _519 + _428;
      float _521 = _520 + _448;
      float _522 = _521 + _453;
      float _523 = _522 + _465;
      float _524 = _523 + _445;
      float _525 = _524 + _507;
      float _526 = _525 + _511;
      float _527 = _526 + _514;
      float _528 = _478 + _482;
      float _529 = _528 * _322;
      float _530 = _458 + (c_ps_user[13].x);
      float _531 = _530 + _462;
      float _532 = c_diffuse_scale.z * _531;
      float _533 = _532 * c_mat_diffuse.z;
      float _534 = _489 + _471;
      float _535 = c_mat_specular.z * _534;
      float _536 = _535 * c_specular_scale.z;
      float _537 = _416 + _399;
      float _538 = _537 + _412;
      float _539 = _538 + (c_ps_user[12].w);
      float _540 = _539 + _421;
      float _541 = _540 + _425;
      float _542 = _541 + _429;
      float _543 = _542 + _449;
      float _544 = _543 + _454;
      float _545 = _544 + _466;
      float _546 = _545 + _445;
      float _547 = _546 + _529;
      float _548 = _547 + _533;
      float _549 = _548 + _536;
      _551 = 0.0f;
      _552 = 0.0f;
      _553 = 0.0f;
      _554 = _505;
      _555 = _527;
      _556 = _549;
   }
   else
   {
      _551 = _386;
      _552 = _387;
      _553 = _388;
      _554 = _389;
      _555 = _390;
      _556 = _391;
   }
   float _557 = _554 + _551;
   float _558 = _555 + _552;
   float _559 = _556 + _553;
   float _564 = _38.w * COLOR.w;
   float _565 = _564 * c_mat_opacity;
   float _566 = _565 * c_global_opacity;
   float _567 = _566 * c_opacity_scale;
   int _568 = c_bool & 16;
   bool _569 = (_568 == 0);
   if (!_569)
   {
      float _573 = c_tone_control.x * _557;
      float _574 = c_tone_control.x * _558;
      float _575 = c_tone_control.x * _559;
      float _577 = log2(_573);
      float _578 = log2(_574);
      float _579 = log2(_575);
      float _580 = _577 * c_tone_control.y;
      float _581 = _578 * c_tone_control.y;
      float _582 = _579 * c_tone_control.y;
      float _583 = exp2(_580);
      float _584 = exp2(_581);
      float _585 = exp2(_582);
      _633 = _583;
      _634 = _584;
      _635 = _585;
   }
   else
   {
      if (_393)
      {
         float _590 = c_tone_control.x * _557;
         float _591 = c_tone_control.x * _558;
         float _592 = c_tone_control.x * _559;
         float _593 = _590 * 2.0f;
         float _594 = _591 * 2.0f;
         float _595 = _592 * 2.0f;
         float _596 = _590 * 0.30000001192092896f;
         float _597 = _591 * 0.30000001192092896f;
         float _598 = _592 * 0.30000001192092896f;
         float _599 = _596 + 0.05000000074505806f;
         float _600 = _597 + 0.05000000074505806f;
         float _601 = _598 + 0.05000000074505806f;
         float _602 = _599 * _593;
         float _603 = _600 * _594;
         float _604 = _601 * _595;
         float _605 = _602 + 0.004000000189989805f;
         float _606 = _603 + 0.004000000189989805f;
         float _607 = _604 + 0.004000000189989805f;
         float _608 = _596 + 0.5f;
         float _609 = _597 + 0.5f;
         float _610 = _598 + 0.5f;
         float _611 = _608 * _593;
         float _612 = _609 * _594;
         float _613 = _610 * _595;
         float _614 = _611 + 0.06000000238418579f;
         float _615 = _612 + 0.06000000238418579f;
         float _616 = _613 + 0.06000000238418579f;
         float _617 = _605 / _614;
         float _618 = _606 / _615;
         float _619 = _607 / _616;
         float _620 = _617 + -0.06666666269302368f;
         float _621 = _618 + -0.06666666269302368f;
         float _622 = _619 + -0.06666666269302368f;
         float _623 = _620 * 1.3790643215179443f;
         float _624 = _621 * 1.3790643215179443f;
         float _625 = _622 * 1.3790643215179443f;
         float _626 = YKR_MaterialCurve(_623, _590);
         float _627 = YKR_MaterialCurve(_624, _591);
         float _628 = YKR_MaterialCurve(_625, _592);
         float _629 = sqrt(_626);
         float _630 = sqrt(_627);
         float _631 = sqrt(_628);
         _633 = _629;
         _634 = _630;
         _635 = _631;
      }
      else
      {
         _633 = _557;
         _634 = _558;
         _635 = _559;
      }
   }
   int _636 = c_bool & 4;
   bool _637 = (_636 == 0);
   if (_637)
   {
      float _639 = saturate(COLOR_1.x);
      float _644 = (c_fog_param[0].x) - _633;
      float _645 = (c_fog_param[0].y) - _634;
      float _646 = (c_fog_param[0].z) - _635;
      float _647 = _644 * _639;
      float _648 = _645 * _639;
      float _649 = _646 * _639;
      _651 = _647;
      _652 = _648;
      _653 = _649;
   }
   else
   {
      _651 = COLOR_1.x;
      _652 = COLOR_1.y;
      _653 = COLOR_1.z;
   }
   float _654 = _651 + _633;
   float _655 = _652 + _634;
   float _656 = _653 + _635;
   float _659 = _567 - c_mat_punchthrough_ref;
   bool _660 = (_659 < 0.0f);
   if (_660)
      discard;
   float _666 = (c_ps_col_scl[0].x) * _654;
   float _667 = (c_ps_col_scl[0].y) * _655;
   float _668 = (c_ps_col_scl[0].z) * _656;
   float _669 = (c_ps_col_scl[0].w) * _567;
   SV_Target.x = _666;
   SV_Target.y = _667;
   SV_Target.z = _668;
   SV_Target.w = _669;
   int _672 = c_bool & 8;
   bool _673 = (_672 != 0);
   float _674 = select(_673, _654, _666);
   float _675 = select(_673, _655, _667);
   float _676 = select(_673, _656, _668);
   float _677 = select(_673, _567, _669);
   SV_Target.x = _674;
   SV_Target.y = _675;
   SV_Target.z = _676;
   SV_Target.w = _677;
   return SV_Target;
}
