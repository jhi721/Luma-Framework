// Yakuza Kiwami material r_o1dst_m(vr)2ds_m4d (0xB7B52824, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
// applied by a script: the inline Hable tonemap's saturate goes through
// YKR_MaterialCurve (tangent continuation in HDR), and the light pre-pass buffer t14 through YKR_LightBuffer.

#include "Includes/Common.hlsl"

Texture2D<float4> s0_t : register(t0);

Texture2D<float4> s1_t : register(t1);

Texture2D<float4> s3_t : register(t3);

Texture2D<float4> s4_t : register(t4);

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

SamplerState s1_s : register(s1);

SamplerState s3_s : register(s3);

SamplerState s4_s : register(s4);

SamplerState s5_s : register(s5);

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
    linear float4 TEXCOORD_6 : TEXCOORD6,
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
   float _76 = (c_attribute_ps[0].z) * TEXCOORD.x;
   float _77 = (c_attribute_ps[0].w) * TEXCOORD.y;
   float4 _78 = s5_t.Sample(s5_s, float2(_76, _77));
   float4 _81 = s1_t.Sample(s1_s, float2(TEXCOORD.z, TEXCOORD.w));
   float _85 = _81.x * _81.x;
   float _86 = _81.y * _81.y;
   float _87 = _81.z * _81.z;
   float _91 = (c_attribute_ps[1].x) * TEXCOORD.z;
   float _92 = (c_attribute_ps[1].y) * TEXCOORD.w;
   float4 _93 = s4_t.Sample(s4_s, float2(_91, _92));
   float4 _97 = s7_t.Sample(s7_s, float2(TEXCOORD_1.x, TEXCOORD_1.y));
   float _100 = 1.0f - _85;
   float _101 = 1.0f - _86;
   float _102 = 1.0f - _87;
   float _103 = _100 * COLOR_1.w;
   float _104 = _101 * COLOR_1.w;
   float _105 = _102 * COLOR_1.w;
   float _106 = 1.0f - _103;
   float _107 = 1.0f - _104;
   float _108 = 1.0f - _105;
   float _109 = _61 * _106;
   float _110 = _62 * _107;
   float _111 = _63 * _108;
   float _112 = 1.0f - _93.x;
   float _113 = 1.0f - _93.y;
   float _114 = 1.0f - _93.z;
   float _115 = _112 * COLOR_1.w;
   float _116 = _113 * COLOR_1.w;
   float _117 = _114 * COLOR_1.w;
   float _118 = 1.0f - _115;
   float _119 = 1.0f - _116;
   float _120 = 1.0f - _117;
   float _121 = _118 * _69.x;
   float _122 = _119 * _69.y;
   float _123 = _120 * _69.z;
   float _124 = _78.x * 2.0f;
   float _125 = _78.y * 2.0f;
   float _126 = _124 + -1.0f;
   float _127 = _125 + -1.0f;
   float _128 = dot(float2(_126, _127), float2(_126, _127));
   float _129 = 1.0f - _128;
   float _130 = saturate(_129);
   float _131 = sqrt(_130);
   float _132 = TEXCOORD_2.y * TEXCOORD_6.z;
   float _133 = TEXCOORD_2.z * TEXCOORD_6.y;
   float _134 = _132 - _133;
   float _135 = TEXCOORD_2.z * TEXCOORD_6.x;
   float _136 = TEXCOORD_2.x * TEXCOORD_6.z;
   float _137 = _135 - _136;
   float _138 = TEXCOORD_2.x * TEXCOORD_6.y;
   float _139 = TEXCOORD_2.y * TEXCOORD_6.x;
   float _140 = _138 - _139;
   float _141 = _126 * TEXCOORD_6.x;
   float _142 = _126 * TEXCOORD_6.y;
   float _143 = _126 * TEXCOORD_6.z;
   float _144 = _127 * TEXCOORD_6.w;
   float _145 = _144 * _134;
   float _146 = _144 * _137;
   float _147 = _144 * _140;
   float _148 = _145 + _141;
   float _149 = _146 + _142;
   float _150 = _147 + _143;
   float _151 = _131 * TEXCOORD_2.x;
   float _152 = _131 * TEXCOORD_2.y;
   float _153 = _131 * TEXCOORD_2.z;
   float _154 = _148 + _151;
   float _155 = _149 + _152;
   float _156 = _150 + _153;
   float _157 = dot(float3(_154, _155, _156), float3(_154, _155, _156));
   float _158 = rsqrt(_157);
   float _159 = _154 * _158;
   float _160 = _155 * _158;
   float _161 = _156 * _158;
   float _162 = dot(float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z), float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z));
   float _163 = rsqrt(_162);
   float _164 = _163 * TEXCOORD_3.x;
   float _165 = _163 * TEXCOORD_3.y;
   float _166 = _163 * TEXCOORD_3.z;
   float _171 = -0.0f - (c_light_vec[0].x);
   float _172 = -0.0f - (c_light_vec[0].y);
   float _173 = -0.0f - (c_light_vec[0].z);
   float _174 = dot(float3(_159, _160, _161), float3(_171, _172, _173));
   float _178 = c_rt_size.z * SV_Position.x;
   float _179 = c_rt_size.w * SV_Position.y;
   int _182 = c_bool & 8;
   bool _183 = (_182 == 0);
   float _190;
   float _191;
   float _192;
   float _204;
   float _205;
   float _260;
   float _261;
   float _262;
   float _263;
   float _277;
   float _278;
   float _324;
   float _325;
   float _326;
   float _361;
   float _362;
   float _363;
   float _364;
   float _365;
   float _366;
   float _520;
   float _521;
   float _522;
   float _523;
   float _524;
   float _525;
   float _701;
   float _702;
   float _703;
   float _704;
   float _705;
   float _706;
   float _782;
   float _783;
   float _784;
   float _800;
   float _801;
   float _802;
   if (!_183)
   {
      float4 _185 = s13_t.Sample(s13_s, float2(_178, _179));
      _190 = _185.x;
      _191 = _185.y;
      _192 = _185.w;
   }
   else
   {
      _190 = 1.0f;
      _191 = 1.0f;
      _192 = 1.0f;
   }
   if (!_183)
   {
      float _197 = (c_sdw_param[0].x) * _190;
      float _198 = (c_sdw_param[0].z) * _191;
      float _201 = _197 + (c_sdw_param[0].y);
      float _202 = _198 + (c_sdw_param[0].w);
      _204 = _201;
      _205 = _202;
   }
   else
   {
      _204 = _190;
      _205 = _191;
   }
   float _212 = (c_sdw_param[1].x) * _97.y;
   float _214 = _212 + (c_sdw_param[1].y);
   float _215 = _214 * _97.x;
   float _216 = 1.0f - (c_light_col[5].x);
   float _217 = 1.0f - (c_light_col[5].y);
   float _218 = 1.0f - (c_light_col[5].z);
   float _219 = _215 * _216;
   float _220 = _215 * _217;
   float _221 = _215 * _218;
   float _222 = _219 + (c_light_col[5].x);
   float _223 = _220 + (c_light_col[5].y);
   float _224 = _221 + (c_light_col[5].z);
   float _228 = c_rt_size.z * SV_Position.x;
   float _229 = c_rt_size.w * SV_Position.y;
   float4 _230 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_228, _229)));
   int _237 = c_bool & 16384;
   bool _238 = (_237 == 0);
   if (!_238)
   {
      float _240 = _230.x * 2.0f;
      float _241 = _230.y * 2.0f;
      float _242 = _230.z * 2.0f;
      float _243 = _230.w * 2.0f;
      _260 = _240;
      _261 = _241;
      _262 = _242;
      _263 = _243;
   }
   else
   {
      int _245 = c_bool & 8192;
      bool _246 = (_245 == 0);
      if (!_246)
      {
         float _248 = _230.x * 2.0f;
         float _249 = _230.y * 2.0f;
         float _250 = _230.z * 2.0f;
         float _251 = _230.w * 2.0f;
         _260 = _248;
         _261 = _249;
         _262 = _250;
         _263 = _251;
      }
      else
      {
         float _255 = _230.x / c_tone_control.z;
         float _256 = _230.y / c_tone_control.z;
         float _257 = _230.z / c_tone_control.z;
         float _258 = _230.w / c_tone_control.z;
         _260 = _255;
         _261 = _256;
         _262 = _257;
         _263 = _258;
      }
   }
   int _264 = c_bool & 8;
   bool _265 = (_264 == 0);
   if (!_265)
   {
      float _270 = (c_sdw_param[0].x) * _204;
      float _271 = (c_sdw_param[0].z) * _205;
      float _274 = _270 + (c_sdw_param[0].y);
      float _275 = _271 + (c_sdw_param[0].w);
      _277 = _274;
      _278 = _275;
   }
   else
   {
      _277 = _204;
      _278 = _205;
   }
   float _279 = _278 * _277;
   float _284 = 1.0f - (c_light_col[5].x);
   float _285 = 1.0f - (c_light_col[5].y);
   float _286 = 1.0f - (c_light_col[5].z);
   float _287 = _284 * _279;
   float _288 = _285 * _279;
   float _289 = _286 * _279;
   float _290 = _287 + (c_light_col[5].x);
   float _291 = _288 + (c_light_col[5].y);
   float _292 = _289 + (c_light_col[5].z);
   float _293 = _192 * COLOR.x;
   float _294 = _293 * _222;
   float _295 = _192 * COLOR.y;
   float _296 = _295 * _223;
   float _297 = _192 * COLOR.z;
   float _298 = _297 * _224;
   float _299 = _290 * _222;
   float _300 = _291 * _223;
   float _301 = _292 * _224;
   if (!_265)
   {
      float _305 = (c_sdw_param[2].x) * _290;
      float _306 = (c_sdw_param[2].x) * _291;
      float _307 = (c_sdw_param[2].x) * _292;
      float _309 = _305 + (c_sdw_param[2].y);
      float _310 = _306 + (c_sdw_param[2].y);
      float _311 = _307 + (c_sdw_param[2].y);
      float _313 = (c_sdw_param[2].z) * _222;
      float _314 = (c_sdw_param[2].z) * _223;
      float _315 = (c_sdw_param[2].z) * _224;
      float _317 = _313 + (c_sdw_param[2].w);
      float _318 = _314 + (c_sdw_param[2].w);
      float _319 = _315 + (c_sdw_param[2].w);
      float _320 = _317 * _309;
      float _321 = _318 * _310;
      float _322 = _319 * _311;
      _324 = _320;
      _325 = _321;
      _326 = _322;
   }
   else
   {
      _324 = _299;
      _325 = _300;
      _326 = _301;
   }
   float _327 = _192 * _97.x;
   float _328 = _324 * _327;
   float _329 = _325 * _327;
   float _330 = _326 * _327;
   float _341 = c_mat_emissive.x * _109;
   float _342 = _341 * c_emissive_scale;
   float _343 = c_mat_emissive.y * _110;
   float _344 = _343 * c_emissive_scale;
   float _345 = c_mat_emissive.z * _111;
   float _346 = _345 * c_emissive_scale;
   int _347 = c_bool & 2048;
   bool _348 = (_347 == 0);
   if (_348)
   {
      float _350 = _278 * _260;
      float _351 = _278 * _261;
      float _352 = _278 * _262;
      float _353 = _350 + TEXCOORD_4.x;
      float _354 = _351 + TEXCOORD_4.y;
      float _355 = _352 + TEXCOORD_4.z;
      float _356 = _278 * _263;
      float _357 = _356 + TEXCOORD_5.x;
      float _358 = _356 + TEXCOORD_5.y;
      float _359 = _356 + TEXCOORD_5.z;
      _361 = _353;
      _362 = _354;
      _363 = _355;
      _364 = _357;
      _365 = _358;
      _366 = _359;
   }
   else
   {
      _361 = TEXCOORD_4.x;
      _362 = TEXCOORD_4.y;
      _363 = TEXCOORD_4.z;
      _364 = TEXCOORD_5.x;
      _365 = TEXCOORD_5.y;
      _366 = TEXCOORD_5.z;
   }
   float _371 = -0.0f - (c_light_vec[1].x);
   float _372 = -0.0f - (c_light_vec[1].y);
   float _373 = -0.0f - (c_light_vec[1].z);
   float _374 = dot(float3(_159, _160, _161), float3(_371, _372, _373));
   float _375 = saturate(_374);
   float _380 = saturate(_174);
   float _381 = (c_light_col[0].x) * _380;
   float _382 = (c_light_col[0].y) * _380;
   float _383 = (c_light_col[0].z) * _380;
   float _388 = (c_light_col[1].x) * _375;
   float _389 = (c_light_col[1].y) * _375;
   float _390 = (c_light_col[1].z) * _375;
   float _399 = c_diffuse_scale.x * c_mat_diffuse.x;
   float _400 = c_diffuse_scale.y * c_mat_diffuse.y;
   float _401 = c_diffuse_scale.z * c_mat_diffuse.z;
   float _402 = _381 * _290;
   float _403 = _382 * _291;
   float _404 = _383 * _292;
   float _405 = _388 * _278;
   float _406 = _389 * _278;
   float _407 = _390 * _278;
   float _408 = _405 + _402;
   float _409 = _406 + _403;
   float _410 = _407 + _404;
   float _411 = _399 * _408;
   float _412 = _400 * _409;
   float _413 = _401 * _410;
   float _414 = _342 + c_scene_ambient.x;
   float _415 = _414 + _411;
   float _416 = _344 + c_scene_ambient.y;
   float _417 = _416 + _412;
   float _418 = _346 + c_scene_ambient.z;
   float _419 = _418 + _413;
   float _420 = _415 * _294;
   float _421 = _417 * _296;
   float _422 = _419 * _298;
   float _423 = _361 * _328;
   float _424 = _362 * _329;
   float _425 = _363 * _330;
   float _426 = _420 + _423;
   float _427 = _421 + _424;
   float _428 = _422 + _425;
   float _433 = (c_light_vec[0].x) + _164;
   float _434 = (c_light_vec[0].y) + _165;
   float _435 = (c_light_vec[0].z) + _166;
   float _436 = dot(float3(_433, _434, _435), float3(_433, _434, _435));
   float _437 = rsqrt(_436);
   float _438 = _433 * _437;
   float _439 = -0.0f - _438;
   float _440 = _434 * _437;
   float _441 = -0.0f - _440;
   float _442 = _435 * _437;
   float _443 = -0.0f - _442;
   float _444 = dot(float3(_439, _441, _443), float3(_159, _160, _161));
   float _445 = saturate(_444);
   float _446 = _165 + -0.30000001192092896f;
   float _447 = dot(float3(_164, _446, _166), float3(_164, _446, _166));
   float _448 = rsqrt(_447);
   float _449 = _164 * _448;
   float _450 = -0.0f - _449;
   float _451 = _446 * _448;
   float _452 = -0.0f - _451;
   float _453 = _166 * _448;
   float _454 = -0.0f - _453;
   float _455 = dot(float3(_450, _452, _454), float3(_159, _160, _161));
   float _456 = saturate(_455);
   float _459 = log2(_445);
   float _460 = log2(_456);
   float _461 = _459 * c_mat_power;
   float _462 = _460 * c_mat_power;
   float _463 = exp2(_461);
   float _464 = exp2(_462);
   float _467 = _463 * (c_light_col[0].x);
   float _468 = _463 * (c_light_col[0].y);
   float _469 = _463 * (c_light_col[0].z);
   float _470 = _464 * 2.0f;
   float _471 = _470 * (c_light_vec[5].x);
   float _472 = _471 * (c_light_col[1].x);
   float _473 = _471 * (c_light_col[1].y);
   float _474 = _471 * (c_light_col[1].z);
   float _475 = _467 * _290;
   float _476 = _468 * _291;
   float _477 = _469 * _292;
   float _478 = _472 * _375;
   float _479 = _473 * _375;
   float _480 = _474 * _375;
   float _481 = _478 + _475;
   float _482 = _479 + _476;
   float _483 = _480 + _477;
   float _490 = c_mat_specular.x * c_specular_scale.x;
   float _491 = _490 * _294;
   float _492 = _491 * _481;
   float _493 = c_mat_specular.y * c_specular_scale.y;
   float _494 = _493 * _296;
   float _495 = _494 * _482;
   float _496 = c_mat_specular.z * c_specular_scale.z;
   float _497 = _496 * _298;
   float _498 = _497 * _483;
   float _499 = _364 * _328;
   float _500 = _499 * _464;
   float _501 = _365 * _329;
   float _502 = _501 * _464;
   float _503 = _366 * _330;
   float _504 = _503 * _464;
   float _505 = _492 + _500;
   float _506 = _495 + _502;
   float _507 = _498 + _504;
   if (!_348)
   {
      float _509 = _278 * _260;
      float _510 = _278 * _261;
      float _511 = _278 * _262;
      float _512 = _426 + _509;
      float _513 = _427 + _510;
      float _514 = _428 + _511;
      float _515 = _278 * _263;
      float _516 = _505 + _515;
      float _517 = _506 + _515;
      float _518 = _507 + _515;
      _520 = _512;
      _521 = _513;
      _522 = _514;
      _523 = _516;
      _524 = _517;
      _525 = _518;
   }
   else
   {
      _520 = _426;
      _521 = _427;
      _522 = _428;
      _523 = _505;
      _524 = _506;
      _525 = _507;
   }
   int _526 = c_bool & 128;
   bool _527 = (_526 != 0);
   float _528 = select(_527, 0.0f, _523);
   float _529 = select(_527, 0.0f, _524);
   float _530 = select(_527, 0.0f, _525);
   float _531 = _520 * _109;
   float _532 = _521 * _110;
   float _533 = _522 * _111;
   float _534 = _528 * _121;
   float _535 = _529 * _122;
   float _536 = _530 * _123;
   int _537 = c_bool & 32768;
   bool _538 = (_537 == 0);
   if (!_538)
   {
      float _542 = (c_ps_user[11].x) * _109;
      float _543 = (c_ps_user[11].x) * _110;
      float _544 = (c_ps_user[11].x) * _111;
      float _546 = _159 + 1.0f;
      float _547 = _160 + 1.0f;
      float _548 = _161 + 1.0f;
      float _549 = _546 * 0.5f;
      float _550 = _547 * 0.5f;
      float _551 = _548 * 0.5f;
      float _552 = saturate(_549);
      float _553 = saturate(_550);
      float _554 = saturate(_551);
      float _555 = (c_ps_user[11].y) * _552;
      float _556 = _553 * (c_ps_user[11].y);
      float _557 = _554 * (c_ps_user[11].y);
      float _559 = (c_ps_user[11].z) * _121;
      float _560 = (c_ps_user[11].z) * _122;
      float _561 = (c_ps_user[11].z) * _123;
      float _563 = c_mat_power * 0.009999999776482582f;
      float _564 = _563 * (c_ps_user[11].w);
      float _567 = (c_ps_user[12].x) * _290;
      float _568 = (c_ps_user[12].x) * _291;
      float _569 = (c_ps_user[12].x) * _292;
      float _571 = (c_ps_user[12].y) * _294;
      float _572 = (c_ps_user[12].y) * _296;
      float _573 = (c_ps_user[12].y) * _298;
      float _575 = (c_ps_user[12].z) * _328;
      float _576 = (c_ps_user[12].z) * _329;
      float _577 = (c_ps_user[12].z) * _330;
      float _583 = -0.0f - _164;
      float _584 = -0.0f - _165;
      float _585 = -0.0f - _166;
      float _586 = dot(float3(_583, _584, _585), float3(_159, _160, _161));
      float _587 = 1.0f - _586;
      float _588 = _587 * _587;
      float _589 = _587 * 0.9722222089767456f;
      float _590 = _588 * _588;
      float _591 = _590 * _589;
      float _592 = _591 + 0.02777777425944805f;
      float _593 = _592 * (c_ps_user[13].z);
      float _595 = (c_ps_user[13].w) * c_scene_ambient.x;
      float _596 = (c_ps_user[13].w) * c_scene_ambient.y;
      float _597 = (c_ps_user[13].w) * c_scene_ambient.z;
      float _600 = (c_ps_user[14].x) * _342;
      float _601 = (c_ps_user[14].x) * _344;
      float _602 = (c_ps_user[14].x) * _346;
      float _604 = (c_ps_user[14].y) * _381;
      float _605 = (c_ps_user[14].y) * _382;
      float _606 = (c_ps_user[14].y) * _383;
      float _608 = (c_ps_user[14].z) * _388;
      float _609 = (c_ps_user[14].z) * _389;
      float _610 = (c_ps_user[14].z) * _390;
      float _612 = (c_ps_user[14].w) * _361;
      float _613 = (c_ps_user[14].w) * _362;
      float _614 = (c_ps_user[14].w) * _363;
      float _617 = (c_ps_user[15].x) * _467;
      float _618 = (c_ps_user[15].x) * _468;
      float _619 = (c_ps_user[15].x) * _469;
      float _621 = _490 * _472;
      float _622 = _621 * (c_ps_user[15].y);
      float _623 = _493 * _473;
      float _624 = _623 * (c_ps_user[15].y);
      float _625 = _496 * _474;
      float _626 = _625 * (c_ps_user[15].y);
      float _628 = (c_ps_user[15].z) * _364;
      float _629 = (c_ps_user[15].z) * _365;
      float _630 = (c_ps_user[15].z) * _366;
      float _631 = _622 + _628;
      float _632 = _631 * _464;
      float _633 = _604 + (c_ps_user[13].x);
      float _634 = _633 + _608;
      float _635 = c_diffuse_scale.x * _634;
      float _636 = _635 * c_mat_diffuse.x;
      float _637 = (c_ps_user[13].y) + _559;
      float _638 = _637 + _617;
      float _639 = c_mat_specular.x * _638;
      float _640 = _639 * c_specular_scale.x;
      float _641 = _555 + _542;
      float _642 = _641 + _564;
      float _643 = _642 + (c_ps_user[12].w);
      float _644 = _643 + _567;
      float _645 = _644 + _571;
      float _646 = _645 + _575;
      float _647 = _646 + _595;
      float _648 = _647 + _600;
      float _649 = _648 + _612;
      float _650 = _649 + _593;
      float _651 = _650 + _632;
      float _652 = _651 + _636;
      float _653 = _652 + _640;
      float _654 = _624 + _629;
      float _655 = _654 * _464;
      float _656 = _605 + (c_ps_user[13].x);
      float _657 = _656 + _609;
      float _658 = c_diffuse_scale.y * _657;
      float _659 = _658 * c_mat_diffuse.y;
      float _660 = (c_ps_user[13].y) + _560;
      float _661 = _660 + _618;
      float _662 = c_mat_specular.y * _661;
      float _663 = _662 * c_specular_scale.y;
      float _664 = _556 + _543;
      float _665 = _664 + _564;
      float _666 = _665 + (c_ps_user[12].w);
      float _667 = _666 + _568;
      float _668 = _667 + _572;
      float _669 = _668 + _576;
      float _670 = _669 + _596;
      float _671 = _670 + _601;
      float _672 = _671 + _613;
      float _673 = _672 + _593;
      float _674 = _673 + _655;
      float _675 = _674 + _659;
      float _676 = _675 + _663;
      float _677 = _626 + _630;
      float _678 = _677 * _464;
      float _679 = _606 + (c_ps_user[13].x);
      float _680 = _679 + _610;
      float _681 = c_diffuse_scale.z * _680;
      float _682 = _681 * c_mat_diffuse.z;
      float _683 = (c_ps_user[13].y) + _561;
      float _684 = _683 + _619;
      float _685 = c_mat_specular.z * _684;
      float _686 = _685 * c_specular_scale.z;
      float _687 = _564 + _544;
      float _688 = _687 + _557;
      float _689 = _688 + (c_ps_user[12].w);
      float _690 = _689 + _569;
      float _691 = _690 + _573;
      float _692 = _691 + _577;
      float _693 = _692 + _597;
      float _694 = _693 + _602;
      float _695 = _694 + _614;
      float _696 = _695 + _593;
      float _697 = _696 + _678;
      float _698 = _697 + _682;
      float _699 = _698 + _686;
      _701 = _653;
      _702 = _676;
      _703 = _699;
      _704 = 0.0f;
      _705 = 0.0f;
      _706 = 0.0f;
   }
   else
   {
      _701 = _531;
      _702 = _532;
      _703 = _533;
      _704 = _534;
      _705 = _535;
      _706 = _536;
   }
   float _707 = _704 + _701;
   float _708 = _705 + _702;
   float _709 = _706 + _703;
   float _714 = c_mat_opacity * _56.w;
   float _715 = _714 * c_global_opacity;
   float _716 = _715 * c_opacity_scale;
   int _717 = c_bool & 16;
   bool _718 = (_717 == 0);
   if (!_718)
   {
      float _722 = c_tone_control.x * _707;
      float _723 = c_tone_control.x * _708;
      float _724 = c_tone_control.x * _709;
      float _726 = log2(_722);
      float _727 = log2(_723);
      float _728 = log2(_724);
      float _729 = _726 * c_tone_control.y;
      float _730 = _727 * c_tone_control.y;
      float _731 = _728 * c_tone_control.y;
      float _732 = exp2(_729);
      float _733 = exp2(_730);
      float _734 = exp2(_731);
      _782 = _732;
      _783 = _733;
      _784 = _734;
   }
   else
   {
      if (_538)
      {
         float _739 = c_tone_control.x * _707;
         float _740 = c_tone_control.x * _708;
         float _741 = c_tone_control.x * _709;
         float _742 = _739 * 2.0f;
         float _743 = _740 * 2.0f;
         float _744 = _741 * 2.0f;
         float _745 = _739 * 0.30000001192092896f;
         float _746 = _740 * 0.30000001192092896f;
         float _747 = _741 * 0.30000001192092896f;
         float _748 = _745 + 0.05000000074505806f;
         float _749 = _746 + 0.05000000074505806f;
         float _750 = _747 + 0.05000000074505806f;
         float _751 = _748 * _742;
         float _752 = _749 * _743;
         float _753 = _750 * _744;
         float _754 = _751 + 0.004000000189989805f;
         float _755 = _752 + 0.004000000189989805f;
         float _756 = _753 + 0.004000000189989805f;
         float _757 = _745 + 0.5f;
         float _758 = _746 + 0.5f;
         float _759 = _747 + 0.5f;
         float _760 = _757 * _742;
         float _761 = _758 * _743;
         float _762 = _759 * _744;
         float _763 = _760 + 0.06000000238418579f;
         float _764 = _761 + 0.06000000238418579f;
         float _765 = _762 + 0.06000000238418579f;
         float _766 = _754 / _763;
         float _767 = _755 / _764;
         float _768 = _756 / _765;
         float _769 = _766 + -0.06666666269302368f;
         float _770 = _767 + -0.06666666269302368f;
         float _771 = _768 + -0.06666666269302368f;
         float _772 = _769 * 1.3790643215179443f;
         float _773 = _770 * 1.3790643215179443f;
         float _774 = _771 * 1.3790643215179443f;
         float _775 = YKR_MaterialCurve(_772, _739);
         float _776 = YKR_MaterialCurve(_773, _740);
         float _777 = YKR_MaterialCurve(_774, _741);
         float _778 = sqrt(_775);
         float _779 = sqrt(_776);
         float _780 = sqrt(_777);
         _782 = _778;
         _783 = _779;
         _784 = _780;
      }
      else
      {
         _782 = _707;
         _783 = _708;
         _784 = _709;
      }
   }
   int _785 = c_bool & 4;
   bool _786 = (_785 == 0);
   if (_786)
   {
      float _788 = saturate(COLOR_1.x);
      float _793 = (c_fog_param[0].x) - _782;
      float _794 = (c_fog_param[0].y) - _783;
      float _795 = (c_fog_param[0].z) - _784;
      float _796 = _793 * _788;
      float _797 = _794 * _788;
      float _798 = _795 * _788;
      _800 = _796;
      _801 = _797;
      _802 = _798;
   }
   else
   {
      _800 = COLOR_1.x;
      _801 = COLOR_1.y;
      _802 = COLOR_1.z;
   }
   float _803 = _800 + _782;
   float _804 = _801 + _783;
   float _805 = _802 + _784;
   float _811 = (c_ps_col_scl[0].x) * _803;
   float _812 = (c_ps_col_scl[0].y) * _804;
   float _813 = (c_ps_col_scl[0].z) * _805;
   float _814 = (c_ps_col_scl[0].w) * _716;
   SV_Target.x = _811;
   SV_Target.y = _812;
   SV_Target.z = _813;
   SV_Target.w = _814;
   int _817 = c_bool & 8;
   bool _818 = (_817 != 0);
   float _819 = select(_818, _803, _811);
   float _820 = select(_818, _804, _812);
   float _821 = select(_818, _805, _813);
   float _822 = select(_818, _716, _814);
   SV_Target.x = _819;
   SV_Target.y = _820;
   SV_Target.z = _821;
   SV_Target.w = _822;
   return SV_Target;
}
