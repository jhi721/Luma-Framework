// Yakuza Kiwami material s_o1dzt[rdup][rt][dedit] (0x20360774, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
// applied by a script: the inline Hable tonemap's saturate goes through
// YKR_MaterialCurve (tangent continuation in HDR), and the light pre-pass buffer t14 through YKR_LightBuffer.

#include "Includes/Common.hlsl"

Texture2D<float4> s0_t : register(t0);

Texture2D<float4> s2_t : register(t2);

Texture2D<float4> s5_t : register(t5);

Texture2D<float4> s6_t : register(t6);

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

cbuffer cb2 : register(b2)
{
   float4 c_ps_scratch[92] : packoffset(c000.x);
};

cbuffer cb3 : register(b3)
{
   float4 c_light_col[6] : packoffset(c000.x);
   float4 c_light_vec[6] : packoffset(c006.x);
};

SamplerState s0_s : register(s0);

SamplerState s2_s : register(s2);

SamplerState s5_s : register(s5);

SamplerState s6_s : register(s6);

SamplerState s7_s : register(s7);

SamplerState s13_s : register(s13);

SamplerState s14_s : register(s14);

float4 main(
    linear float4 COLOR : COLOR,
    linear float4 COLOR_1 : COLOR1,
    linear float4 TEXCOORD : TEXCOORD,
    linear float4 TEXCOORD_1 : TEXCOORD1,
    linear float4 TEXCOORD_2 : TEXCOORD2,
    linear float4 TEXCOORD_3 : TEXCOORD3,
    linear float4 TEXCOORD_4 : TEXCOORD4,
    linear float4 TEXCOORD_5 : TEXCOORD5,
    noperspective float4 SV_Position : SV_Position) : SV_Target
{
   float4 SV_Target = 0;
   float4 _58 = s2_t.Sample(s2_s, float2(TEXCOORD.x, TEXCOORD.y));
   float4 _62 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _67 = _62.x * _62.x;
   float _68 = _62.y * _62.y;
   float _69 = _62.z * _62.z;
   float4 _70 = s5_t.Sample(s5_s, float2(TEXCOORD.x, TEXCOORD.y));
   float4 _73 = s6_t.Sample(s6_s, float2(TEXCOORD.z, TEXCOORD.w));
   float _76 = _73.x + -0.5f;
   float _77 = _73.y + -0.5f;
   float _78 = _76 * _58.z;
   float _79 = _77 * _58.z;
   float _80 = _78 + 0.5f;
   float _81 = _79 + 0.5f;
   bool _82 = (_70.x < 0.5f);
   bool _83 = (_70.y < 0.5f);
   float _84 = _70.x * 2.0f;
   float _85 = _84 * _80;
   float _86 = _70.y * 2.0f;
   float _87 = _86 * _81;
   float _88 = 1.0f - _70.x;
   float _89 = 1.0f - _70.y;
   float _90 = _88 * 2.0f;
   float _91 = _89 * 2.0f;
   float _92 = 0.5f - _78;
   float _93 = 0.5f - _79;
   float _94 = _90 * _92;
   float _95 = _91 * _93;
   float _96 = 1.0f - _94;
   float _97 = 1.0f - _95;
   float _98 = select(_82, _85, _96);
   float _99 = select(_83, _87, _97);
   float _104 = _67 * (c_ps_user[3].x);
   float _105 = _68 * (c_ps_user[3].y);
   float _106 = _69 * (c_ps_user[3].z);
   float _109 = _98 * 2.0f;
   float _110 = _99 * 2.0f;
   float _111 = _109 + -1.0f;
   float _112 = _110 + -1.0f;
   float _113 = dot(float2(_111, _112), float2(_111, _112));
   float _114 = 1.0f - _113;
   float _115 = saturate(_114);
   float _116 = sqrt(_115);
   float _117 = TEXCOORD_2.y * TEXCOORD_3.z;
   float _118 = TEXCOORD_2.z * TEXCOORD_3.y;
   float _119 = _117 - _118;
   float _120 = TEXCOORD_2.z * TEXCOORD_3.x;
   float _121 = TEXCOORD_2.x * TEXCOORD_3.z;
   float _122 = _120 - _121;
   float _123 = TEXCOORD_2.x * TEXCOORD_3.y;
   float _124 = TEXCOORD_2.y * TEXCOORD_3.x;
   float _125 = _123 - _124;
   float _126 = _111 * TEXCOORD_3.x;
   float _127 = _111 * TEXCOORD_3.y;
   float _128 = _111 * TEXCOORD_3.z;
   float _129 = _112 * TEXCOORD_3.w;
   float _130 = _129 * _119;
   float _131 = _129 * _122;
   float _132 = _129 * _125;
   float _133 = _116 * TEXCOORD_2.x;
   float _134 = _116 * TEXCOORD_2.y;
   float _135 = _116 * TEXCOORD_2.z;
   float _136 = _126 + _133;
   float _137 = _136 + _130;
   float _138 = _127 + _134;
   float _139 = _138 + _131;
   float _140 = _128 + _135;
   float _141 = _140 + _132;
   float _142 = dot(float3(_137, _139, _141), float3(_137, _139, _141));
   float _143 = rsqrt(_142);
   float _144 = _137 * _143;
   float _145 = _139 * _143;
   float _146 = _141 * _143;
   float _151 = dot(float3(TEXCOORD_1.x, TEXCOORD_1.y, TEXCOORD_1.z), float3(TEXCOORD_1.x, TEXCOORD_1.y, TEXCOORD_1.z));
   float _152 = rsqrt(_151);
   float _153 = _152 * TEXCOORD_1.x;
   float _154 = _152 * TEXCOORD_1.y;
   float _155 = _152 * TEXCOORD_1.z;
   float _156 = _153 + (c_light_vec[0].x);
   float _157 = _154 + (c_light_vec[0].y);
   float _158 = _155 + (c_light_vec[0].z);
   float _159 = dot(float3(_156, _157, _158), float3(_156, _157, _158));
   float _160 = rsqrt(_159);
   float _161 = _156 * _160;
   float _162 = _157 * _160;
   float _163 = _158 * _160;
   float _165 = (c_ps_user[3].w) * TEXCOORD.x;
   float _166 = (c_ps_user[3].w) * TEXCOORD.y;
   float4 _167 = s7_t.Sample(s7_s, float2(_165, _166));
   float _172 = _167.x * _167.x;
   float _173 = _167.y * _167.y;
   float _174 = _167.z * _167.z;
   float _175 = 1.0f - _172;
   float _176 = 1.0f - _173;
   float _177 = 1.0f - _174;
   float _178 = 1.0f - _58.z;
   float _179 = 1.0f - _167.w;
   float _180 = _179 * _178;
   float _181 = _180 * _175;
   float _182 = _180 * _176;
   float _183 = _180 * _177;
   float _184 = _181 + _172;
   float _185 = _182 + _173;
   float _186 = _183 + _174;
   float _187 = _104 * _184;
   float _188 = _105 * _185;
   float _189 = _106 * _186;
   float _190 = _145 + 0.10000000149011612f;
   float _191 = saturate(_190);
   float _194 = (c_attribute_ps[2].w) + 2.0f;
   float _196 = (c_attribute_ps[2].z) + 2.5f;
   float _197 = -0.0f - _153;
   float _198 = -0.0f - _154;
   float _199 = -0.0f - _155;
   float _200 = dot(float3(_197, _198, _199), float3(TEXCOORD_2.x, TEXCOORD_2.y, TEXCOORD_2.z));
   float _201 = 1.0f - _200;
   float _202 = saturate(_201);
   float _203 = dot(float3(_197, _198, _199), float3(_144, _145, _146));
   float _204 = 1.0f - _203;
   float _205 = saturate(_204);
   float _206 = _205 * _202;
   float _207 = log2(_206);
   float _208 = _207 * _194;
   float _209 = exp2(_208);
   float _210 = _209 * _196;
   int _213 = c_bool & 4096;
   bool _214 = (_213 == 0);
   float _223;
   float _247;
   float _248;
   float _260;
   float _261;
   float _449;
   float _450;
   float _451;
   float _452;
   float _453;
   float _454;
   float _487;
   float _488;
   float _489;
   float _490;
   float _655;
   float _821;
   float _822;
   float _823;
   float _899;
   float _900;
   float _901;
   float _917;
   float _918;
   float _919;
   float _944;
   float _945;
   float _946;
   if (!_214)
   {
      float _219 = (c_ps_user[9].w) * _207;
      float _220 = exp2(_219);
      float _221 = _220 * (c_ps_user[9].z);
      _223 = _221;
   }
   else
   {
      _223 = _210;
   }
   float _224 = _191 * _223;
   float _225 = _58.y * _224;
   float _228 = _225 * (c_ps_user[2].x);
   float _229 = _228 + 1.0f;
   float _230 = _187 * _229;
   float _231 = _188 * _229;
   float _232 = _189 * _229;
   float _236 = c_rt_size.z * SV_Position.x;
   float _237 = c_rt_size.w * SV_Position.y;
   int _240 = c_bool & 8;
   bool _241 = (_240 == 0);
   if (!_241)
   {
      float4 _243 = s13_t.Sample(s13_s, float2(_236, _237));
      _247 = _243.x;
      _248 = _243.y;
   }
   else
   {
      _247 = 1.0f;
      _248 = 1.0f;
   }
   if (!_241)
   {
      float _253 = (c_sdw_param_ch[0].x) * _247;
      float _254 = (c_sdw_param_ch[0].z) * _248;
      float _257 = _253 + (c_sdw_param_ch[0].y);
      float _258 = _254 + (c_sdw_param_ch[0].w);
      _260 = _257;
      _261 = _258;
   }
   else
   {
      _260 = _247;
      _261 = _248;
   }
   float _262 = min(_260, _261);
   float _271 = (c_light_vec[1].x) + _153;
   float _272 = (c_light_vec[1].y) + _154;
   float _273 = (c_light_vec[1].z) + _155;
   float _274 = dot(float3(_271, _272, _273), float3(_271, _272, _273));
   float _275 = rsqrt(_274);
   float _276 = _271 * _275;
   float _277 = _272 * _275;
   float _278 = _273 * _275;
   float _279 = -0.0f - (c_light_vec[1].x);
   float _280 = -0.0f - (c_light_vec[1].y);
   float _281 = -0.0f - (c_light_vec[1].z);
   float _282 = -0.0f - (c_light_vec[0].x);
   float _283 = -0.0f - (c_light_vec[0].y);
   float _284 = -0.0f - (c_light_vec[0].z);
   float _285 = dot(float3(_153, _154, _155), float3(_144, _145, _146));
   float _286 = _285 * 2.0f;
   float _287 = _286 * _144;
   float _288 = _286 * _145;
   float _289 = _286 * _146;
   float _290 = _153 - _287;
   float _291 = _154 - _288;
   float _292 = _155 - _289;
   float _293 = dot(float3(_290, _291, _292), float3(_282, _283, _284));
   float _294 = dot(float3(_290, _291, _292), float3(_279, _280, _281));
   float _295 = saturate(_293);
   float _296 = saturate(_294);
   float _297 = log2(_295);
   float _298 = log2(_296);
   float _299 = _297 * c_mat_power;
   float _300 = _298 * c_mat_power;
   float _301 = exp2(_299);
   float _302 = exp2(_300);
   float _307 = c_mat_fresnel * c_tone_control.w;
   float _308 = c_mat_power * 0.03974359855055809f;
   float _309 = _308 + 0.0856831967830658f;
   float _310 = _302 * _309;
   float _311 = dot(float3(_153, _154, _155), float3(_161, _162, _163));
   float _312 = dot(float3(_153, _154, _155), float3(_276, _277, _278));
   float _313 = 1.0f - _311;
   float _314 = 1.0f - _312;
   float _315 = _313 * _313;
   float _316 = _314 * _314;
   float _317 = 1.0f - _307;
   float _318 = _315 * _315;
   float _319 = _318 * _313;
   float _320 = _319 * _317;
   float _321 = _314 * _317;
   float _322 = _316 * _316;
   float _323 = _322 * _321;
   float _324 = _320 + _307;
   float _325 = _323 + _307;
   float _326 = dot(float3(_144, _145, _146), float3(_282, _283, _284));
   float _327 = saturate(_326);
   float _328 = dot(float3(_144, _145, _146), float3(_279, _280, _281));
   float _329 = saturate(_328);
   float _330 = _310 * _329;
   float _331 = _330 * _325;
   float _340 = c_specular_scale.x * c_mat_specular.x;
   float _341 = c_specular_scale.y * c_mat_specular.y;
   float _342 = c_specular_scale.z * c_mat_specular.z;
   float _347 = _262 * _309;
   float _348 = _347 * _301;
   float _349 = _348 * _327;
   float _350 = _349 * _324;
   float _351 = (c_light_col[0].x) * _350;
   float _352 = (c_light_col[0].y) * _350;
   float _353 = (c_light_col[0].z) * _350;
   float _358 = (c_light_col[1].x) * _331;
   float _359 = (c_light_col[1].y) * _331;
   float _360 = (c_light_col[1].z) * _331;
   float _361 = _358 + _351;
   float _362 = _359 + _352;
   float _363 = _360 + _353;
   float _364 = _340 * _361;
   float _365 = _341 * _362;
   float _366 = _342 * _363;
   int _370 = c_bool & 1;
   bool _371 = (_370 == 0);
   if (!_371)
   {
      float _373 = -0.0f - TEXCOORD_4.x;
      float _374 = -0.0f - TEXCOORD_4.y;
      float _375 = -0.0f - TEXCOORD_4.z;
      float _376 = dot(float3(_144, _145, _146), float3(_373, _374, _375));
      float _377 = saturate(_376);
      float _378 = _377 * TEXCOORD_1.w;
      float _379 = _377 * TEXCOORD_2.w;
      float _380 = _377 * TEXCOORD_4.w;
      float _381 = max(0.0f, _378);
      float _382 = max(0.0f, _379);
      float _383 = max(0.0f, _380);
      float _384 = -0.0f - TEXCOORD_5.x;
      float _385 = -0.0f - TEXCOORD_5.y;
      float _386 = -0.0f - TEXCOORD_5.z;
      float _387 = dot(float3(_144, _145, _146), float3(_384, _385, _386));
      float _388 = saturate(_387);
      float _389 = _388 * COLOR.w;
      float _390 = _388 * COLOR_1.w;
      float _391 = _388 * TEXCOORD_5.w;
      float _392 = max(0.0f, _389);
      float _393 = max(0.0f, _390);
      float _394 = max(0.0f, _391);
      float _395 = _392 + _381;
      float _396 = _393 + _382;
      float _397 = _394 + _383;
      float _398 = _153 + TEXCOORD_4.x;
      float _399 = _154 + TEXCOORD_4.y;
      float _400 = _155 + TEXCOORD_4.z;
      float _401 = dot(float3(_398, _399, _400), float3(_398, _399, _400));
      float _402 = rsqrt(_401);
      float _403 = _153 + TEXCOORD_5.x;
      float _404 = _154 + TEXCOORD_5.y;
      float _405 = _155 + TEXCOORD_5.z;
      float _406 = dot(float3(_403, _404, _405), float3(_403, _404, _405));
      float _407 = rsqrt(_406);
      float _408 = _403 * _407;
      float _409 = -0.0f - _408;
      float _410 = _404 * _407;
      float _411 = -0.0f - _410;
      float _412 = _405 * _407;
      float _413 = -0.0f - _412;
      float _414 = _398 * _402;
      float _415 = -0.0f - _414;
      float _416 = _399 * _402;
      float _417 = -0.0f - _416;
      float _418 = _400 * _402;
      float _419 = -0.0f - _418;
      float _420 = dot(float3(_415, _417, _419), float3(_144, _145, _146));
      float _421 = dot(float3(_409, _411, _413), float3(_144, _145, _146));
      float _422 = saturate(_420);
      float _423 = saturate(_421);
      float _424 = log2(_422);
      float _425 = log2(_423);
      float _426 = _424 * c_mat_power;
      float _427 = _425 * c_mat_power;
      float _428 = exp2(_426);
      float _429 = exp2(_427);
      float _430 = _428 * TEXCOORD_1.w;
      float _431 = _428 * TEXCOORD_2.w;
      float _432 = _428 * TEXCOORD_4.w;
      float _433 = _429 * COLOR.w;
      float _434 = _429 * COLOR_1.w;
      float _435 = _429 * TEXCOORD_5.w;
      float _436 = _433 + _430;
      float _437 = _434 + _431;
      float _438 = _435 + _432;
      float _439 = max(0.0f, _436);
      float _440 = max(0.0f, _437);
      float _441 = max(0.0f, _438);
      float _442 = _340 * _439;
      float _443 = _341 * _440;
      float _444 = _342 * _441;
      float _445 = min(_442, 1.0f);
      float _446 = min(_443, 1.0f);
      float _447 = min(_444, 1.0f);
      _449 = _445;
      _450 = _446;
      _451 = _447;
      _452 = _395;
      _453 = _396;
      _454 = _397;
   }
   else
   {
      _449 = 0.0f;
      _450 = 0.0f;
      _451 = 0.0f;
      _452 = 0.0f;
      _453 = 0.0f;
      _454 = 0.0f;
   }
   float _455 = c_rt_size.z * SV_Position.x;
   float _456 = c_rt_size.w * SV_Position.y;
   float4 _457 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_455, _456)));
   int _464 = c_bool & 16384;
   bool _465 = (_464 == 0);
   if (!_465)
   {
      float _467 = _457.x * 4.0f;
      float _468 = _457.y * 4.0f;
      float _469 = _457.z * 4.0f;
      float _470 = _457.w * 4.0f;
      _487 = _467;
      _488 = _468;
      _489 = _469;
      _490 = _470;
   }
   else
   {
      int _472 = c_bool & 8192;
      bool _473 = (_472 == 0);
      if (!_473)
      {
         float _475 = _457.x * 2.0f;
         float _476 = _457.y * 2.0f;
         float _477 = _457.z * 2.0f;
         float _478 = _457.w * 2.0f;
         _487 = _475;
         _488 = _476;
         _489 = _477;
         _490 = _478;
      }
      else
      {
         float _482 = _457.x / c_tone_control.z;
         float _483 = _457.y / c_tone_control.z;
         float _484 = _457.z / c_tone_control.z;
         float _485 = _457.w / c_tone_control.z;
         _487 = _482;
         _488 = _483;
         _489 = _484;
         _490 = _485;
      }
   }
   float _491 = _487 + _452;
   float _492 = _488 + _453;
   float _493 = _489 + _454;
   float _494 = _490 + _449;
   float _495 = _490 + _450;
   float _496 = _490 + _451;
   float _559 = dot(float3(_144, _145, _146), float3(_144, _145, _146));
   float _560 = rsqrt(_559);
   float _561 = _560 * _144;
   float _562 = _560 * _145;
   float _563 = _560 * _146;
   float _564 = dot(float4((c_ps_scratch[80].x), (c_ps_scratch[80].y), (c_ps_scratch[80].z), (c_ps_scratch[80].w)), float4(_561, _562, _563, 1.0f));
   float _565 = dot(float4((c_ps_scratch[81].x), (c_ps_scratch[81].y), (c_ps_scratch[81].z), (c_ps_scratch[81].w)), float4(_561, _562, _563, 1.0f));
   float _566 = dot(float4((c_ps_scratch[82].x), (c_ps_scratch[82].y), (c_ps_scratch[82].z), (c_ps_scratch[82].w)), float4(_561, _562, _563, 1.0f));
   float _567 = dot(float4((c_ps_scratch[83].x), (c_ps_scratch[83].y), (c_ps_scratch[83].z), (c_ps_scratch[83].w)), float4(_561, _562, _563, 1.0f));
   float _568 = dot(float4(_561, _562, _563, 1.0f), float4(_564, _565, _566, _567));
   float _569 = dot(float4((c_ps_scratch[84].x), (c_ps_scratch[84].y), (c_ps_scratch[84].z), (c_ps_scratch[84].w)), float4(_561, _562, _563, 1.0f));
   float _570 = dot(float4((c_ps_scratch[85].x), (c_ps_scratch[85].y), (c_ps_scratch[85].z), (c_ps_scratch[85].w)), float4(_561, _562, _563, 1.0f));
   float _571 = dot(float4((c_ps_scratch[86].x), (c_ps_scratch[86].y), (c_ps_scratch[86].z), (c_ps_scratch[86].w)), float4(_561, _562, _563, 1.0f));
   float _572 = dot(float4((c_ps_scratch[87].x), (c_ps_scratch[87].y), (c_ps_scratch[87].z), (c_ps_scratch[87].w)), float4(_561, _562, _563, 1.0f));
   float _573 = dot(float4(_561, _562, _563, 1.0f), float4(_569, _570, _571, _572));
   float _574 = dot(float4((c_ps_scratch[88].x), (c_ps_scratch[88].y), (c_ps_scratch[88].z), (c_ps_scratch[88].w)), float4(_561, _562, _563, 1.0f));
   float _575 = dot(float4((c_ps_scratch[89].x), (c_ps_scratch[89].y), (c_ps_scratch[89].z), (c_ps_scratch[89].w)), float4(_561, _562, _563, 1.0f));
   float _576 = dot(float4((c_ps_scratch[90].x), (c_ps_scratch[90].y), (c_ps_scratch[90].z), (c_ps_scratch[90].w)), float4(_561, _562, _563, 1.0f));
   float _577 = dot(float4((c_ps_scratch[91].x), (c_ps_scratch[91].y), (c_ps_scratch[91].z), (c_ps_scratch[91].w)), float4(_561, _562, _563, 1.0f));
   float _578 = dot(float4(_561, _562, _563, 1.0f), float4(_574, _575, _576, _577));
   float _579 = max(_568, 0.0625f);
   float _580 = max(_573, 0.0625f);
   float _581 = max(_578, 0.0625f);
   float _582 = _579 * (c_ps_scratch[79].x);
   float _583 = _580 * (c_ps_scratch[79].x);
   float _584 = _581 * (c_ps_scratch[79].x);
   float _589 = -0.0f - (c_light_vec[0].x);
   float _590 = -0.0f - (c_light_vec[0].y);
   float _591 = -0.0f - (c_light_vec[0].z);
   float _592 = dot(float3(_144, _145, _146), float3(_589, _590, _591));
   float _593 = -0.0f - (c_light_vec[1].x);
   float _594 = -0.0f - (c_light_vec[1].y);
   float _595 = -0.0f - (c_light_vec[1].z);
   float _596 = dot(float3(_144, _145, _146), float3(_593, _594, _595));
   float _605 = _145 * 0.5f;
   float _606 = _605 + 0.5f;
   float _607 = (c_light_col[4].x) - (c_light_vec[4].x);
   float _608 = (c_light_col[4].y) - (c_light_vec[4].y);
   float _609 = (c_light_col[4].z) - (c_light_vec[4].z);
   float _610 = _607 * _606;
   float _611 = _608 * _606;
   float _612 = _609 * _606;
   float _613 = saturate(_592);
   float _614 = saturate(_596);
   float _619 = (c_light_col[0].x) * _613;
   float _620 = (c_light_col[0].y) * _613;
   float _621 = (c_light_col[0].z) * _613;
   float _626 = (c_light_col[1].x) * _614;
   float _627 = (c_light_col[1].y) * _614;
   float _628 = (c_light_col[1].z) * _614;
   float _629 = _619 * _262;
   float _630 = _620 * _262;
   float _631 = _621 * _262;
   float _632 = (c_light_vec[4].x) + _582;
   float _633 = _632 + _610;
   float _634 = _633 + _491;
   float _635 = _634 + _626;
   float _636 = _635 + _629;
   float _637 = (c_light_vec[4].y) + _583;
   float _638 = _637 + _611;
   float _639 = _638 + _492;
   float _640 = _639 + _627;
   float _641 = _640 + _630;
   float _642 = (c_light_vec[4].z) + _584;
   float _643 = _642 + _612;
   float _644 = _643 + _493;
   float _645 = _644 + _628;
   float _646 = _645 + _631;
   int _647 = c_bool & 64;
   bool _648 = (_647 == 0);
   if (!_648)
   {
      float _650 = 1.0f - _58.y;
      float _651 = _262 * _650;
      float _652 = _651 * _613;
      float _653 = _652 + _58.y;
      _655 = _653;
   }
   else
   {
      _655 = _58.y;
   }
   float _656 = _494 + _364;
   float _657 = _495 + _365;
   float _658 = _496 + _366;
   int _659 = c_bool & 32768;
   bool _660 = (_659 == 0);
   if (!_660)
   {
      float _672 = (c_ps_user[11].x) * _230;
      float _673 = (c_ps_user[11].x) * _231;
      float _674 = (c_ps_user[11].x) * _232;
      float _676 = _144 + 1.0f;
      float _677 = _145 + 1.0f;
      float _678 = _146 + 1.0f;
      float _679 = _676 * 0.5f;
      float _680 = _677 * 0.5f;
      float _681 = _678 * 0.5f;
      float _682 = saturate(_679);
      float _683 = saturate(_680);
      float _684 = saturate(_681);
      float _685 = (c_ps_user[11].y) * _682;
      float _686 = _683 * (c_ps_user[11].y);
      float _687 = _684 * (c_ps_user[11].y);
      float _697 = (c_ps_user[11].z) * _58.x;
      float _699 = (c_ps_user[11].w) * _656;
      float _702 = (c_ps_user[12].x) * _262;
      float _704 = (c_ps_user[12].y) * _58.y;
      float _709 = _204 * _204;
      float _710 = _204 * 0.9722222089767456f;
      float _711 = _709 * _709;
      float _712 = _711 * _710;
      float _713 = _712 + 0.02777777425944805f;
      float _714 = (c_ps_user[13].z) * _713;
      float _716 = (c_ps_user[13].w) * _633;
      float _717 = (c_ps_user[13].w) * _638;
      float _718 = (c_ps_user[13].w) * _643;
      float _721 = (c_ps_user[14].y) * _619;
      float _722 = (c_ps_user[14].y) * _620;
      float _723 = (c_ps_user[14].y) * _621;
      float _725 = (c_ps_user[14].z) * _626;
      float _726 = (c_ps_user[14].z) * _627;
      float _727 = (c_ps_user[14].z) * _628;
      float _729 = (c_ps_user[14].w) * _491;
      float _730 = (c_ps_user[14].w) * _492;
      float _731 = (c_ps_user[14].w) * _493;
      float _736 = (c_ps_user[15].z) * _494;
      float _737 = (c_ps_user[15].z) * _495;
      float _738 = (c_ps_user[15].z) * _496;
      float _739 = (c_ps_user[15].y) + (c_ps_user[15].x);
      float _740 = c_specular_scale.x * _739;
      float _741 = _740 * _656;
      float _742 = _721 + (c_ps_user[13].x);
      float _743 = _742 + _725;
      float _744 = c_diffuse_scale.x * _743;
      float _745 = _744 * c_mat_diffuse.x;
      float _746 = (c_ps_user[13].y) + _697;
      float _747 = _746 * c_specular_scale.x;
      float _748 = _747 + _741;
      float _749 = _748 * c_mat_specular.x;
      float _750 = _685 + _672;
      float _751 = _750 + _699;
      float _752 = _751 + _702;
      float _753 = _752 + _704;
      float _754 = _753 + _716;
      float _755 = _754 + _714;
      float _756 = _755 + _729;
      float _757 = _756 + _736;
      float _758 = _757 + _745;
      float _759 = _758 + _749;
      float _760 = c_specular_scale.y * _739;
      float _761 = _760 * _657;
      float _762 = _722 + (c_ps_user[13].x);
      float _763 = _762 + _726;
      float _764 = c_diffuse_scale.y * _763;
      float _765 = _764 * c_mat_diffuse.y;
      float _766 = _746 * c_specular_scale.y;
      float _767 = _766 + _761;
      float _768 = _767 * c_mat_specular.y;
      float _769 = _699 + _673;
      float _770 = _769 + _686;
      float _771 = _770 + _702;
      float _772 = _771 + _704;
      float _773 = _772 + _717;
      float _774 = _773 + _714;
      float _775 = _774 + _730;
      float _776 = _775 + _737;
      float _777 = _776 + _765;
      float _778 = _777 + _768;
      float _779 = c_specular_scale.z * _739;
      float _780 = _779 * _658;
      float _781 = _723 + (c_ps_user[13].x);
      float _782 = _781 + _727;
      float _783 = c_diffuse_scale.z * _782;
      float _784 = _783 * c_mat_diffuse.z;
      float _785 = _746 * c_specular_scale.z;
      float _786 = _785 + _780;
      float _787 = _786 * c_mat_specular.z;
      float _788 = _699 + _674;
      float _789 = _788 + _687;
      float _790 = _789 + _702;
      float _791 = _790 + _704;
      float _792 = _791 + _718;
      float _793 = _792 + _714;
      float _794 = _793 + _731;
      float _795 = _794 + _738;
      float _796 = _795 + _784;
      float _797 = _796 + _787;
      float _798 = saturate(_759);
      float _799 = saturate(_778);
      float _800 = saturate(_797);
      _821 = _798;
      _822 = _799;
      _823 = _800;
   }
   else
   {
      float _802 = c_diffuse_scale.x * c_mat_diffuse.x;
      float _803 = _802 * _230;
      float _804 = _803 * _636;
      float _805 = _804 * _655;
      float _806 = c_diffuse_scale.y * c_mat_diffuse.y;
      float _807 = _806 * _231;
      float _808 = _807 * _641;
      float _809 = _808 * _655;
      float _810 = c_diffuse_scale.z * c_mat_diffuse.z;
      float _811 = _810 * _232;
      float _812 = _811 * _646;
      float _813 = _812 * _655;
      float _814 = _656 * _58.x;
      float _815 = _657 * _58.x;
      float _816 = _658 * _58.x;
      float _817 = _805 + _814;
      float _818 = _809 + _815;
      float _819 = _813 + _816;
      _821 = _817;
      _822 = _818;
      _823 = _819;
   }
   float _828 = c_mat_opacity * _62.w;
   float _829 = _828 * c_opacity_scale;
   float _830 = _829 * c_global_opacity;
   float _831 = _821 * COLOR.x;
   float _832 = _822 * COLOR.y;
   float _833 = _823 * COLOR.z;
   int _834 = c_bool & 16;
   bool _835 = (_834 == 0);
   if (!_835)
   {
      float _839 = c_tone_control.x * _831;
      float _840 = c_tone_control.x * _832;
      float _841 = c_tone_control.x * _833;
      float _843 = log2(_839);
      float _844 = log2(_840);
      float _845 = log2(_841);
      float _846 = _843 * c_tone_control.y;
      float _847 = _844 * c_tone_control.y;
      float _848 = _845 * c_tone_control.y;
      float _849 = exp2(_846);
      float _850 = exp2(_847);
      float _851 = exp2(_848);
      _899 = _849;
      _900 = _850;
      _901 = _851;
   }
   else
   {
      if (_660)
      {
         float _856 = c_tone_control.x * _831;
         float _857 = c_tone_control.x * _832;
         float _858 = c_tone_control.x * _833;
         float _859 = _856 * 2.0f;
         float _860 = _857 * 2.0f;
         float _861 = _858 * 2.0f;
         float _862 = _856 * 0.30000001192092896f;
         float _863 = _857 * 0.30000001192092896f;
         float _864 = _858 * 0.30000001192092896f;
         float _865 = _862 + 0.05000000074505806f;
         float _866 = _863 + 0.05000000074505806f;
         float _867 = _864 + 0.05000000074505806f;
         float _868 = _865 * _859;
         float _869 = _866 * _860;
         float _870 = _867 * _861;
         float _871 = _868 + 0.004000000189989805f;
         float _872 = _869 + 0.004000000189989805f;
         float _873 = _870 + 0.004000000189989805f;
         float _874 = _862 + 0.5f;
         float _875 = _863 + 0.5f;
         float _876 = _864 + 0.5f;
         float _877 = _874 * _859;
         float _878 = _875 * _860;
         float _879 = _876 * _861;
         float _880 = _877 + 0.06000000238418579f;
         float _881 = _878 + 0.06000000238418579f;
         float _882 = _879 + 0.06000000238418579f;
         float _883 = _871 / _880;
         float _884 = _872 / _881;
         float _885 = _873 / _882;
         float _886 = _883 + -0.06666666269302368f;
         float _887 = _884 + -0.06666666269302368f;
         float _888 = _885 + -0.06666666269302368f;
         float _889 = _886 * 1.3790643215179443f;
         float _890 = _887 * 1.3790643215179443f;
         float _891 = _888 * 1.3790643215179443f;
         float _892 = YKR_MaterialCurve(_889, _856);
         float _893 = YKR_MaterialCurve(_890, _857);
         float _894 = YKR_MaterialCurve(_891, _858);
         float _895 = sqrt(_892);
         float _896 = sqrt(_893);
         float _897 = sqrt(_894);
         _899 = _895;
         _900 = _896;
         _901 = _897;
      }
      else
      {
         _899 = _831;
         _900 = _832;
         _901 = _833;
      }
   }
   int _902 = c_bool & 4;
   bool _903 = (_902 == 0);
   if (_903)
   {
      float _905 = saturate(COLOR_1.x);
      float _910 = (c_fog_param[0].x) - _899;
      float _911 = (c_fog_param[0].y) - _900;
      float _912 = (c_fog_param[0].z) - _901;
      float _913 = _910 * _905;
      float _914 = _911 * _905;
      float _915 = _912 * _905;
      _917 = _913;
      _918 = _914;
      _919 = _915;
   }
   else
   {
      _917 = COLOR_1.x;
      _918 = COLOR_1.y;
      _919 = COLOR_1.z;
   }
   float _920 = _917 + _899;
   float _921 = _918 + _900;
   float _922 = _919 + _901;
   if (!_660)
   {
      float _930 = (c_ps_user[3].x) * (c_ps_user[12].z);
      float _931 = (c_ps_user[3].y) * (c_ps_user[12].z);
      float _932 = (c_ps_user[3].z) * (c_ps_user[12].z);
      float _934 = (c_ps_user[12].w) * COLOR.x;
      float _935 = (c_ps_user[12].w) * COLOR.y;
      float _936 = (c_ps_user[12].w) * COLOR.z;
      float _937 = _934 + _920;
      float _938 = _937 + _930;
      float _939 = _935 + _921;
      float _940 = _939 + _931;
      float _941 = _936 + _922;
      float _942 = _941 + _932;
      _944 = _938;
      _945 = _940;
      _946 = _942;
   }
   else
   {
      _944 = _920;
      _945 = _921;
      _946 = _922;
   }
   float _952 = (c_ps_col_scl[0].x) * _944;
   float _953 = (c_ps_col_scl[0].y) * _945;
   float _954 = (c_ps_col_scl[0].z) * _946;
   float _955 = (c_ps_col_scl[0].w) * _830;
   SV_Target.x = _952;
   SV_Target.y = _953;
   SV_Target.z = _954;
   SV_Target.w = _955;
   int _958 = c_bool & 8;
   bool _959 = (_958 != 0);
   float _960 = select(_959, _944, _952);
   float _961 = select(_959, _945, _953);
   float _962 = select(_959, _946, _954);
   float _963 = select(_959, _830, _955);
   SV_Target.x = _960;
   SV_Target.y = _961;
   SV_Target.z = _962;
   SV_Target.w = _963;
   return SV_Target;
}
