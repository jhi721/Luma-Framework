// Yakuza Kiwami material r_o1m_b(vr)2m_m4d (0x8A60448E, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
// applied by a script: the inline Hable tonemap's saturate goes through
// YKR_MaterialCurve (tangent continuation in HDR), and the light pre-pass buffer t14 through YKR_LightBuffer.

#include "Includes/Common.hlsl"

Texture2D<float4> s0_t : register(t0);

Texture2D<float4> s1_t : register(t1);

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

SamplerState s1_s : register(s1);

SamplerState s7_s : register(s7);

SamplerState s13_s : register(s13);

SamplerState s14_s : register(s14);

float4 main(
    linear float4 TEXCOORD : TEXCOORD,
    linear float2 TEXCOORD_1 : TEXCOORD1,
    linear float4 COLOR : COLOR,
    linear float4 COLOR_1 : COLOR1,
    linear float3 TEXCOORD_2 : TEXCOORD2,
    linear float3 TEXCOORD_3 : TEXCOORD3,
    linear float3 TEXCOORD_4 : TEXCOORD4,
    linear float3 TEXCOORD_5 : TEXCOORD5,
    noperspective float4 SV_Position : SV_Position) : SV_Target
{
   float4 SV_Target = 0;
   float4 _46 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _51 = _46.x * _46.x;
   float _52 = _46.y * _46.y;
   float _53 = _46.z * _46.z;
   float4 _54 = s1_t.Sample(s1_s, float2(TEXCOORD.z, TEXCOORD.w));
   float _59 = _54.x * _54.x;
   float _60 = _54.y * _54.y;
   float _61 = _54.z * _54.z;
   float4 _62 = s7_t.Sample(s7_s, float2(TEXCOORD_1.x, TEXCOORD_1.y));
   float _65 = _59 - _51;
   float _66 = _60 - _52;
   float _67 = _61 - _53;
   float _68 = _65 * COLOR_1.w;
   float _69 = _66 * COLOR_1.w;
   float _70 = _67 * COLOR_1.w;
   float _71 = _68 + _51;
   float _72 = _69 + _52;
   float _73 = _70 + _53;
   float _74 = _54.w - _46.w;
   float _75 = _74 * COLOR_1.w;
   float _76 = _75 + _46.w;
   float _77 = dot(float3(TEXCOORD_2.x, TEXCOORD_2.y, TEXCOORD_2.z), float3(TEXCOORD_2.x, TEXCOORD_2.y, TEXCOORD_2.z));
   float _78 = rsqrt(_77);
   float _79 = _78 * TEXCOORD_2.x;
   float _80 = _78 * TEXCOORD_2.y;
   float _81 = _78 * TEXCOORD_2.z;
   float _82 = dot(float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z), float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z));
   float _83 = rsqrt(_82);
   float _84 = _83 * TEXCOORD_3.x;
   float _85 = _83 * TEXCOORD_3.y;
   float _86 = _83 * TEXCOORD_3.z;
   float _91 = -0.0f - (c_light_vec[0].x);
   float _92 = -0.0f - (c_light_vec[0].y);
   float _93 = -0.0f - (c_light_vec[0].z);
   float _94 = dot(float3(_79, _80, _81), float3(_91, _92, _93));
   float _98 = c_rt_size.z * SV_Position.x;
   float _99 = c_rt_size.w * SV_Position.y;
   int _102 = c_bool & 8;
   bool _103 = (_102 == 0);
   float _110;
   float _111;
   float _112;
   float _124;
   float _125;
   float _180;
   float _181;
   float _182;
   float _183;
   float _197;
   float _198;
   float _244;
   float _245;
   float _246;
   float _281;
   float _282;
   float _283;
   float _284;
   float _285;
   float _286;
   float _440;
   float _441;
   float _442;
   float _443;
   float _444;
   float _445;
   float _617;
   float _618;
   float _619;
   float _620;
   float _621;
   float _622;
   float _697;
   float _698;
   float _699;
   float _715;
   float _716;
   float _717;
   if (!_103)
   {
      float4 _105 = s13_t.Sample(s13_s, float2(_98, _99));
      _110 = _105.x;
      _111 = _105.y;
      _112 = _105.w;
   }
   else
   {
      _110 = 1.0f;
      _111 = 1.0f;
      _112 = 1.0f;
   }
   if (!_103)
   {
      float _117 = (c_sdw_param[0].x) * _110;
      float _118 = (c_sdw_param[0].z) * _111;
      float _121 = _117 + (c_sdw_param[0].y);
      float _122 = _118 + (c_sdw_param[0].w);
      _124 = _121;
      _125 = _122;
   }
   else
   {
      _124 = _110;
      _125 = _111;
   }
   float _132 = (c_sdw_param[1].x) * _62.y;
   float _134 = _132 + (c_sdw_param[1].y);
   float _135 = _134 * _62.x;
   float _136 = 1.0f - (c_light_col[5].x);
   float _137 = 1.0f - (c_light_col[5].y);
   float _138 = 1.0f - (c_light_col[5].z);
   float _139 = _135 * _136;
   float _140 = _135 * _137;
   float _141 = _135 * _138;
   float _142 = _139 + (c_light_col[5].x);
   float _143 = _140 + (c_light_col[5].y);
   float _144 = _141 + (c_light_col[5].z);
   float _148 = c_rt_size.z * SV_Position.x;
   float _149 = c_rt_size.w * SV_Position.y;
   float4 _150 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_148, _149)));
   int _157 = c_bool & 16384;
   bool _158 = (_157 == 0);
   if (!_158)
   {
      float _160 = _150.x * 2.0f;
      float _161 = _150.y * 2.0f;
      float _162 = _150.z * 2.0f;
      float _163 = _150.w * 2.0f;
      _180 = _160;
      _181 = _161;
      _182 = _162;
      _183 = _163;
   }
   else
   {
      int _165 = c_bool & 8192;
      bool _166 = (_165 == 0);
      if (!_166)
      {
         float _168 = _150.x * 2.0f;
         float _169 = _150.y * 2.0f;
         float _170 = _150.z * 2.0f;
         float _171 = _150.w * 2.0f;
         _180 = _168;
         _181 = _169;
         _182 = _170;
         _183 = _171;
      }
      else
      {
         float _175 = _150.x / c_tone_control.z;
         float _176 = _150.y / c_tone_control.z;
         float _177 = _150.z / c_tone_control.z;
         float _178 = _150.w / c_tone_control.z;
         _180 = _175;
         _181 = _176;
         _182 = _177;
         _183 = _178;
      }
   }
   int _184 = c_bool & 8;
   bool _185 = (_184 == 0);
   if (!_185)
   {
      float _190 = (c_sdw_param[0].x) * _124;
      float _191 = (c_sdw_param[0].z) * _125;
      float _194 = _190 + (c_sdw_param[0].y);
      float _195 = _191 + (c_sdw_param[0].w);
      _197 = _194;
      _198 = _195;
   }
   else
   {
      _197 = _124;
      _198 = _125;
   }
   float _199 = _198 * _197;
   float _204 = 1.0f - (c_light_col[5].x);
   float _205 = 1.0f - (c_light_col[5].y);
   float _206 = 1.0f - (c_light_col[5].z);
   float _207 = _204 * _199;
   float _208 = _205 * _199;
   float _209 = _206 * _199;
   float _210 = _207 + (c_light_col[5].x);
   float _211 = _208 + (c_light_col[5].y);
   float _212 = _209 + (c_light_col[5].z);
   float _213 = _112 * COLOR.x;
   float _214 = _213 * _142;
   float _215 = _112 * COLOR.y;
   float _216 = _215 * _143;
   float _217 = _112 * COLOR.z;
   float _218 = _217 * _144;
   float _219 = _210 * _142;
   float _220 = _211 * _143;
   float _221 = _212 * _144;
   if (!_185)
   {
      float _225 = (c_sdw_param[2].x) * _210;
      float _226 = (c_sdw_param[2].x) * _211;
      float _227 = (c_sdw_param[2].x) * _212;
      float _229 = _225 + (c_sdw_param[2].y);
      float _230 = _226 + (c_sdw_param[2].y);
      float _231 = _227 + (c_sdw_param[2].y);
      float _233 = (c_sdw_param[2].z) * _142;
      float _234 = (c_sdw_param[2].z) * _143;
      float _235 = (c_sdw_param[2].z) * _144;
      float _237 = _233 + (c_sdw_param[2].w);
      float _238 = _234 + (c_sdw_param[2].w);
      float _239 = _235 + (c_sdw_param[2].w);
      float _240 = _237 * _229;
      float _241 = _238 * _230;
      float _242 = _239 * _231;
      _244 = _240;
      _245 = _241;
      _246 = _242;
   }
   else
   {
      _244 = _219;
      _245 = _220;
      _246 = _221;
   }
   float _247 = _112 * _62.x;
   float _248 = _244 * _247;
   float _249 = _245 * _247;
   float _250 = _246 * _247;
   float _261 = c_mat_emissive.x * _71;
   float _262 = _261 * c_emissive_scale;
   float _263 = c_mat_emissive.y * _72;
   float _264 = _263 * c_emissive_scale;
   float _265 = c_mat_emissive.z * _73;
   float _266 = _265 * c_emissive_scale;
   int _267 = c_bool & 2048;
   bool _268 = (_267 == 0);
   if (_268)
   {
      float _270 = _198 * _180;
      float _271 = _198 * _181;
      float _272 = _198 * _182;
      float _273 = _270 + TEXCOORD_4.x;
      float _274 = _271 + TEXCOORD_4.y;
      float _275 = _272 + TEXCOORD_4.z;
      float _276 = _198 * _183;
      float _277 = _276 + TEXCOORD_5.x;
      float _278 = _276 + TEXCOORD_5.y;
      float _279 = _276 + TEXCOORD_5.z;
      _281 = _273;
      _282 = _274;
      _283 = _275;
      _284 = _277;
      _285 = _278;
      _286 = _279;
   }
   else
   {
      _281 = TEXCOORD_4.x;
      _282 = TEXCOORD_4.y;
      _283 = TEXCOORD_4.z;
      _284 = TEXCOORD_5.x;
      _285 = TEXCOORD_5.y;
      _286 = TEXCOORD_5.z;
   }
   float _291 = -0.0f - (c_light_vec[1].x);
   float _292 = -0.0f - (c_light_vec[1].y);
   float _293 = -0.0f - (c_light_vec[1].z);
   float _294 = dot(float3(_79, _80, _81), float3(_291, _292, _293));
   float _295 = saturate(_294);
   float _300 = saturate(_94);
   float _301 = (c_light_col[0].x) * _300;
   float _302 = (c_light_col[0].y) * _300;
   float _303 = (c_light_col[0].z) * _300;
   float _308 = (c_light_col[1].x) * _295;
   float _309 = (c_light_col[1].y) * _295;
   float _310 = (c_light_col[1].z) * _295;
   float _319 = c_diffuse_scale.x * c_mat_diffuse.x;
   float _320 = c_diffuse_scale.y * c_mat_diffuse.y;
   float _321 = c_diffuse_scale.z * c_mat_diffuse.z;
   float _322 = _301 * _210;
   float _323 = _302 * _211;
   float _324 = _303 * _212;
   float _325 = _308 * _198;
   float _326 = _309 * _198;
   float _327 = _310 * _198;
   float _328 = _325 + _322;
   float _329 = _326 + _323;
   float _330 = _327 + _324;
   float _331 = _319 * _328;
   float _332 = _320 * _329;
   float _333 = _321 * _330;
   float _334 = _262 + c_scene_ambient.x;
   float _335 = _334 + _331;
   float _336 = _264 + c_scene_ambient.y;
   float _337 = _336 + _332;
   float _338 = _266 + c_scene_ambient.z;
   float _339 = _338 + _333;
   float _340 = _335 * _214;
   float _341 = _337 * _216;
   float _342 = _339 * _218;
   float _343 = _281 * _248;
   float _344 = _282 * _249;
   float _345 = _283 * _250;
   float _346 = _340 + _343;
   float _347 = _341 + _344;
   float _348 = _342 + _345;
   float _353 = (c_light_vec[0].x) + _84;
   float _354 = (c_light_vec[0].y) + _85;
   float _355 = (c_light_vec[0].z) + _86;
   float _356 = dot(float3(_353, _354, _355), float3(_353, _354, _355));
   float _357 = rsqrt(_356);
   float _358 = _353 * _357;
   float _359 = -0.0f - _358;
   float _360 = _354 * _357;
   float _361 = -0.0f - _360;
   float _362 = _355 * _357;
   float _363 = -0.0f - _362;
   float _364 = dot(float3(_359, _361, _363), float3(_79, _80, _81));
   float _365 = saturate(_364);
   float _366 = _85 + -0.30000001192092896f;
   float _367 = dot(float3(_84, _366, _86), float3(_84, _366, _86));
   float _368 = rsqrt(_367);
   float _369 = _84 * _368;
   float _370 = -0.0f - _369;
   float _371 = _366 * _368;
   float _372 = -0.0f - _371;
   float _373 = _86 * _368;
   float _374 = -0.0f - _373;
   float _375 = dot(float3(_370, _372, _374), float3(_79, _80, _81));
   float _376 = saturate(_375);
   float _379 = log2(_365);
   float _380 = log2(_376);
   float _381 = _379 * c_mat_power;
   float _382 = _380 * c_mat_power;
   float _383 = exp2(_381);
   float _384 = exp2(_382);
   float _387 = _383 * (c_light_col[0].x);
   float _388 = _383 * (c_light_col[0].y);
   float _389 = _383 * (c_light_col[0].z);
   float _390 = _384 * 2.0f;
   float _391 = _390 * (c_light_vec[5].x);
   float _392 = _391 * (c_light_col[1].x);
   float _393 = _391 * (c_light_col[1].y);
   float _394 = _391 * (c_light_col[1].z);
   float _395 = _387 * _210;
   float _396 = _388 * _211;
   float _397 = _389 * _212;
   float _398 = _392 * _295;
   float _399 = _393 * _295;
   float _400 = _394 * _295;
   float _401 = _398 + _395;
   float _402 = _399 + _396;
   float _403 = _400 + _397;
   float _410 = c_mat_specular.x * c_specular_scale.x;
   float _411 = _410 * _214;
   float _412 = _411 * _401;
   float _413 = c_mat_specular.y * c_specular_scale.y;
   float _414 = _413 * _216;
   float _415 = _414 * _402;
   float _416 = c_mat_specular.z * c_specular_scale.z;
   float _417 = _416 * _218;
   float _418 = _417 * _403;
   float _419 = _284 * _248;
   float _420 = _419 * _384;
   float _421 = _285 * _249;
   float _422 = _421 * _384;
   float _423 = _286 * _250;
   float _424 = _423 * _384;
   float _425 = _412 + _420;
   float _426 = _415 + _422;
   float _427 = _418 + _424;
   if (!_268)
   {
      float _429 = _198 * _180;
      float _430 = _198 * _181;
      float _431 = _198 * _182;
      float _432 = _346 + _429;
      float _433 = _347 + _430;
      float _434 = _348 + _431;
      float _435 = _198 * _183;
      float _436 = _425 + _435;
      float _437 = _426 + _435;
      float _438 = _427 + _435;
      _440 = _432;
      _441 = _433;
      _442 = _434;
      _443 = _436;
      _444 = _437;
      _445 = _438;
   }
   else
   {
      _440 = _346;
      _441 = _347;
      _442 = _348;
      _443 = _425;
      _444 = _426;
      _445 = _427;
   }
   int _446 = c_bool & 128;
   bool _447 = (_446 != 0);
   float _448 = select(_447, 0.0f, _443);
   float _449 = select(_447, 0.0f, _444);
   float _450 = select(_447, 0.0f, _445);
   float _451 = _440 * _71;
   float _452 = _441 * _72;
   float _453 = _442 * _73;
   float _454 = _448 * _76;
   float _455 = _449 * _76;
   float _456 = _450 * _76;
   int _457 = c_bool & 32768;
   bool _458 = (_457 == 0);
   if (!_458)
   {
      float _462 = (c_ps_user[11].x) * _71;
      float _463 = (c_ps_user[11].x) * _72;
      float _464 = (c_ps_user[11].x) * _73;
      float _466 = _79 + 1.0f;
      float _467 = _80 + 1.0f;
      float _468 = _81 + 1.0f;
      float _469 = _466 * 0.5f;
      float _470 = _467 * 0.5f;
      float _471 = _468 * 0.5f;
      float _472 = saturate(_469);
      float _473 = saturate(_470);
      float _474 = saturate(_471);
      float _475 = (c_ps_user[11].y) * _472;
      float _476 = _473 * (c_ps_user[11].y);
      float _477 = _474 * (c_ps_user[11].y);
      float _479 = (c_ps_user[11].z) * _76;
      float _481 = c_mat_power * 0.009999999776482582f;
      float _482 = _481 * (c_ps_user[11].w);
      float _485 = (c_ps_user[12].x) * _210;
      float _486 = (c_ps_user[12].x) * _211;
      float _487 = (c_ps_user[12].x) * _212;
      float _489 = (c_ps_user[12].y) * _214;
      float _490 = (c_ps_user[12].y) * _216;
      float _491 = (c_ps_user[12].y) * _218;
      float _493 = (c_ps_user[12].z) * _248;
      float _494 = (c_ps_user[12].z) * _249;
      float _495 = (c_ps_user[12].z) * _250;
      float _501 = -0.0f - _84;
      float _502 = -0.0f - _85;
      float _503 = -0.0f - _86;
      float _504 = dot(float3(_501, _502, _503), float3(_79, _80, _81));
      float _505 = 1.0f - _504;
      float _506 = _505 * _505;
      float _507 = _505 * 0.9722222089767456f;
      float _508 = _506 * _506;
      float _509 = _508 * _507;
      float _510 = _509 + 0.02777777425944805f;
      float _511 = _510 * (c_ps_user[13].z);
      float _513 = (c_ps_user[13].w) * c_scene_ambient.x;
      float _514 = (c_ps_user[13].w) * c_scene_ambient.y;
      float _515 = (c_ps_user[13].w) * c_scene_ambient.z;
      float _518 = (c_ps_user[14].x) * _262;
      float _519 = (c_ps_user[14].x) * _264;
      float _520 = (c_ps_user[14].x) * _266;
      float _522 = (c_ps_user[14].y) * _301;
      float _523 = (c_ps_user[14].y) * _302;
      float _524 = (c_ps_user[14].y) * _303;
      float _526 = (c_ps_user[14].z) * _308;
      float _527 = (c_ps_user[14].z) * _309;
      float _528 = (c_ps_user[14].z) * _310;
      float _530 = (c_ps_user[14].w) * _281;
      float _531 = (c_ps_user[14].w) * _282;
      float _532 = (c_ps_user[14].w) * _283;
      float _535 = (c_ps_user[15].x) * _387;
      float _536 = (c_ps_user[15].x) * _388;
      float _537 = (c_ps_user[15].x) * _389;
      float _539 = _410 * _392;
      float _540 = _539 * (c_ps_user[15].y);
      float _541 = _413 * _393;
      float _542 = _541 * (c_ps_user[15].y);
      float _543 = _416 * _394;
      float _544 = _543 * (c_ps_user[15].y);
      float _546 = (c_ps_user[15].z) * _284;
      float _547 = (c_ps_user[15].z) * _285;
      float _548 = (c_ps_user[15].z) * _286;
      float _549 = _540 + _546;
      float _550 = _549 * _384;
      float _551 = _522 + (c_ps_user[13].x);
      float _552 = _551 + _526;
      float _553 = c_diffuse_scale.x * _552;
      float _554 = _553 * c_mat_diffuse.x;
      float _555 = (c_ps_user[13].y) + _479;
      float _556 = _555 + _535;
      float _557 = c_mat_specular.x * _556;
      float _558 = _557 * c_specular_scale.x;
      float _559 = _475 + _462;
      float _560 = _559 + _482;
      float _561 = _560 + (c_ps_user[12].w);
      float _562 = _561 + _485;
      float _563 = _562 + _489;
      float _564 = _563 + _493;
      float _565 = _564 + _513;
      float _566 = _565 + _518;
      float _567 = _566 + _530;
      float _568 = _567 + _511;
      float _569 = _568 + _550;
      float _570 = _569 + _554;
      float _571 = _570 + _558;
      float _572 = _542 + _547;
      float _573 = _572 * _384;
      float _574 = _523 + (c_ps_user[13].x);
      float _575 = _574 + _527;
      float _576 = c_diffuse_scale.y * _575;
      float _577 = _576 * c_mat_diffuse.y;
      float _578 = _555 + _536;
      float _579 = c_mat_specular.y * _578;
      float _580 = _579 * c_specular_scale.y;
      float _581 = _476 + _463;
      float _582 = _581 + _482;
      float _583 = _582 + (c_ps_user[12].w);
      float _584 = _583 + _486;
      float _585 = _584 + _490;
      float _586 = _585 + _494;
      float _587 = _586 + _514;
      float _588 = _587 + _519;
      float _589 = _588 + _531;
      float _590 = _589 + _511;
      float _591 = _590 + _573;
      float _592 = _591 + _577;
      float _593 = _592 + _580;
      float _594 = _544 + _548;
      float _595 = _594 * _384;
      float _596 = _524 + (c_ps_user[13].x);
      float _597 = _596 + _528;
      float _598 = c_diffuse_scale.z * _597;
      float _599 = _598 * c_mat_diffuse.z;
      float _600 = _555 + _537;
      float _601 = c_mat_specular.z * _600;
      float _602 = _601 * c_specular_scale.z;
      float _603 = _482 + _464;
      float _604 = _603 + _477;
      float _605 = _604 + (c_ps_user[12].w);
      float _606 = _605 + _487;
      float _607 = _606 + _491;
      float _608 = _607 + _495;
      float _609 = _608 + _515;
      float _610 = _609 + _520;
      float _611 = _610 + _532;
      float _612 = _611 + _511;
      float _613 = _612 + _595;
      float _614 = _613 + _599;
      float _615 = _614 + _602;
      _617 = _571;
      _618 = _593;
      _619 = _615;
      _620 = 0.0f;
      _621 = 0.0f;
      _622 = 0.0f;
   }
   else
   {
      _617 = _451;
      _618 = _452;
      _619 = _453;
      _620 = _454;
      _621 = _455;
      _622 = _456;
   }
   float _623 = _620 + _617;
   float _624 = _621 + _618;
   float _625 = _622 + _619;
   float _630 = c_global_opacity * c_mat_opacity;
   float _631 = _630 * c_opacity_scale;
   int _632 = c_bool & 16;
   bool _633 = (_632 == 0);
   if (!_633)
   {
      float _637 = c_tone_control.x * _623;
      float _638 = c_tone_control.x * _624;
      float _639 = c_tone_control.x * _625;
      float _641 = log2(_637);
      float _642 = log2(_638);
      float _643 = log2(_639);
      float _644 = _641 * c_tone_control.y;
      float _645 = _642 * c_tone_control.y;
      float _646 = _643 * c_tone_control.y;
      float _647 = exp2(_644);
      float _648 = exp2(_645);
      float _649 = exp2(_646);
      _697 = _647;
      _698 = _648;
      _699 = _649;
   }
   else
   {
      if (_458)
      {
         float _654 = c_tone_control.x * _623;
         float _655 = c_tone_control.x * _624;
         float _656 = c_tone_control.x * _625;
         float _657 = _654 * 2.0f;
         float _658 = _655 * 2.0f;
         float _659 = _656 * 2.0f;
         float _660 = _654 * 0.30000001192092896f;
         float _661 = _655 * 0.30000001192092896f;
         float _662 = _656 * 0.30000001192092896f;
         float _663 = _660 + 0.05000000074505806f;
         float _664 = _661 + 0.05000000074505806f;
         float _665 = _662 + 0.05000000074505806f;
         float _666 = _663 * _657;
         float _667 = _664 * _658;
         float _668 = _665 * _659;
         float _669 = _666 + 0.004000000189989805f;
         float _670 = _667 + 0.004000000189989805f;
         float _671 = _668 + 0.004000000189989805f;
         float _672 = _660 + 0.5f;
         float _673 = _661 + 0.5f;
         float _674 = _662 + 0.5f;
         float _675 = _672 * _657;
         float _676 = _673 * _658;
         float _677 = _674 * _659;
         float _678 = _675 + 0.06000000238418579f;
         float _679 = _676 + 0.06000000238418579f;
         float _680 = _677 + 0.06000000238418579f;
         float _681 = _669 / _678;
         float _682 = _670 / _679;
         float _683 = _671 / _680;
         float _684 = _681 + -0.06666666269302368f;
         float _685 = _682 + -0.06666666269302368f;
         float _686 = _683 + -0.06666666269302368f;
         float _687 = _684 * 1.3790643215179443f;
         float _688 = _685 * 1.3790643215179443f;
         float _689 = _686 * 1.3790643215179443f;
         float _690 = YKR_MaterialCurve(_687, _654);
         float _691 = YKR_MaterialCurve(_688, _655);
         float _692 = YKR_MaterialCurve(_689, _656);
         float _693 = sqrt(_690);
         float _694 = sqrt(_691);
         float _695 = sqrt(_692);
         _697 = _693;
         _698 = _694;
         _699 = _695;
      }
      else
      {
         _697 = _623;
         _698 = _624;
         _699 = _625;
      }
   }
   int _700 = c_bool & 4;
   bool _701 = (_700 == 0);
   if (_701)
   {
      float _703 = saturate(COLOR_1.x);
      float _708 = (c_fog_param[0].x) - _697;
      float _709 = (c_fog_param[0].y) - _698;
      float _710 = (c_fog_param[0].z) - _699;
      float _711 = _708 * _703;
      float _712 = _709 * _703;
      float _713 = _710 * _703;
      _715 = _711;
      _716 = _712;
      _717 = _713;
   }
   else
   {
      _715 = COLOR_1.x;
      _716 = COLOR_1.y;
      _717 = COLOR_1.z;
   }
   float _718 = _715 + _697;
   float _719 = _716 + _698;
   float _720 = _717 + _699;
   float _726 = (c_ps_col_scl[0].x) * _718;
   float _727 = (c_ps_col_scl[0].y) * _719;
   float _728 = (c_ps_col_scl[0].z) * _720;
   float _729 = (c_ps_col_scl[0].w) * _631;
   SV_Target.x = _726;
   SV_Target.y = _727;
   SV_Target.z = _728;
   SV_Target.w = _729;
   int _732 = c_bool & 8;
   bool _733 = (_732 != 0);
   float _734 = select(_733, _718, _726);
   float _735 = select(_733, _719, _727);
   float _736 = select(_733, _720, _728);
   float _737 = select(_733, _631, _729);
   SV_Target.x = _734;
   SV_Target.y = _735;
   SV_Target.z = _736;
   SV_Target.w = _737;
   return SV_Target;
}
