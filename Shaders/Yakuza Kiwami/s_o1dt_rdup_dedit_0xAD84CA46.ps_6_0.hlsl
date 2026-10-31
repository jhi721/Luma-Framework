// Yakuza Kiwami material s_o1dt[rdup][dedit] (0xAD84CA46, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
// applied by a script: the inline Hable tonemap's saturate goes through
// YKR_MaterialCurve (tangent continuation in HDR), and the light pre-pass buffer t14 through YKR_LightBuffer.

#include "Includes/Common.hlsl"

Texture2D<float4> s0_t : register(t0);

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

SamplerState s5_s : register(s5);

SamplerState s7_s : register(s7);

SamplerState s13_s : register(s13);

SamplerState s14_s : register(s14);

float4 main(
    linear float4 COLOR : COLOR,
    linear float4 COLOR_1 : COLOR1,
    linear float2 TEXCOORD : TEXCOORD,
    linear float4 TEXCOORD_1 : TEXCOORD1,
    linear float4 TEXCOORD_2 : TEXCOORD2,
    linear float4 TEXCOORD_3 : TEXCOORD3,
    linear float4 TEXCOORD_4 : TEXCOORD4,
    linear float4 TEXCOORD_5 : TEXCOORD5,
    noperspective float4 SV_Position : SV_Position) : SV_Target
{
   float4 SV_Target = 0;
   float4 _52 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _57 = _52.x * _52.x;
   float _58 = _52.y * _52.y;
   float _59 = _52.z * _52.z;
   float4 _60 = s5_t.Sample(s5_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _67 = _57 * (c_ps_user[3].x);
   float _68 = _58 * (c_ps_user[3].y);
   float _69 = _59 * (c_ps_user[3].z);
   float _72 = _60.x * 2.0f;
   float _73 = _60.y * 2.0f;
   float _74 = _72 + -1.0f;
   float _75 = _73 + -1.0f;
   float _76 = dot(float2(_74, _75), float2(_74, _75));
   float _77 = 1.0f - _76;
   float _78 = saturate(_77);
   float _79 = sqrt(_78);
   float _80 = TEXCOORD_2.y * TEXCOORD_3.z;
   float _81 = TEXCOORD_2.z * TEXCOORD_3.y;
   float _82 = _80 - _81;
   float _83 = TEXCOORD_2.z * TEXCOORD_3.x;
   float _84 = TEXCOORD_2.x * TEXCOORD_3.z;
   float _85 = _83 - _84;
   float _86 = TEXCOORD_2.x * TEXCOORD_3.y;
   float _87 = TEXCOORD_2.y * TEXCOORD_3.x;
   float _88 = _86 - _87;
   float _89 = _74 * TEXCOORD_3.x;
   float _90 = _74 * TEXCOORD_3.y;
   float _91 = _74 * TEXCOORD_3.z;
   float _92 = _75 * TEXCOORD_3.w;
   float _93 = _92 * _82;
   float _94 = _92 * _85;
   float _95 = _92 * _88;
   float _96 = _93 + _89;
   float _97 = _94 + _90;
   float _98 = _95 + _91;
   float _99 = _79 * TEXCOORD_2.x;
   float _100 = _79 * TEXCOORD_2.y;
   float _101 = _79 * TEXCOORD_2.z;
   float _102 = _96 + _99;
   float _103 = _97 + _100;
   float _104 = _98 + _101;
   float _105 = dot(float3(_102, _103, _104), float3(_102, _103, _104));
   float _106 = rsqrt(_105);
   float _107 = _102 * _106;
   float _108 = _103 * _106;
   float _109 = _104 * _106;
   float _114 = dot(float3(TEXCOORD_1.x, TEXCOORD_1.y, TEXCOORD_1.z), float3(TEXCOORD_1.x, TEXCOORD_1.y, TEXCOORD_1.z));
   float _115 = rsqrt(_114);
   float _116 = _115 * TEXCOORD_1.x;
   float _117 = _115 * TEXCOORD_1.y;
   float _118 = _115 * TEXCOORD_1.z;
   float _119 = _116 + (c_light_vec[0].x);
   float _120 = _117 + (c_light_vec[0].y);
   float _121 = _118 + (c_light_vec[0].z);
   float _122 = dot(float3(_119, _120, _121), float3(_119, _120, _121));
   float _123 = rsqrt(_122);
   float _124 = _119 * _123;
   float _125 = _120 * _123;
   float _126 = _121 * _123;
   float _128 = (c_ps_user[3].w) * TEXCOORD.x;
   float _129 = (c_ps_user[3].w) * TEXCOORD.y;
   float4 _130 = s7_t.Sample(s7_s, float2(_128, _129));
   float _135 = _130.x * _130.x;
   float _136 = _130.y * _130.y;
   float _137 = _130.z * _130.z;
   float _138 = 1.0f - _135;
   float _139 = 1.0f - _136;
   float _140 = 1.0f - _137;
   float _141 = 1.0f - _130.w;
   float _142 = _138 * _141;
   float _143 = _139 * _141;
   float _144 = _140 * _141;
   float _145 = _142 + _135;
   float _146 = _143 + _136;
   float _147 = _144 + _137;
   float _148 = _67 * _145;
   float _149 = _68 * _146;
   float _150 = _69 * _147;
   float _151 = _108 + 0.10000000149011612f;
   float _152 = saturate(_151);
   float _155 = (c_attribute_ps[2].w) + 2.0f;
   float _157 = (c_attribute_ps[2].z) + 2.5f;
   float _158 = -0.0f - _116;
   float _159 = -0.0f - _117;
   float _160 = -0.0f - _118;
   float _161 = dot(float3(_158, _159, _160), float3(TEXCOORD_2.x, TEXCOORD_2.y, TEXCOORD_2.z));
   float _162 = 1.0f - _161;
   float _163 = saturate(_162);
   float _164 = dot(float3(_158, _159, _160), float3(_107, _108, _109));
   float _165 = 1.0f - _164;
   float _166 = saturate(_165);
   float _167 = _166 * _163;
   float _168 = log2(_167);
   float _169 = _168 * _155;
   float _170 = exp2(_169);
   float _171 = _170 * _157;
   int _174 = c_bool & 4096;
   bool _175 = (_174 == 0);
   float _184;
   float _207;
   float _208;
   float _220;
   float _221;
   float _409;
   float _410;
   float _411;
   float _412;
   float _413;
   float _414;
   float _447;
   float _448;
   float _449;
   float _450;
   float _764;
   float _765;
   float _766;
   float _842;
   float _843;
   float _844;
   float _860;
   float _861;
   float _862;
   float _887;
   float _888;
   float _889;
   if (!_175)
   {
      float _180 = (c_ps_user[9].w) * _168;
      float _181 = exp2(_180);
      float _182 = _181 * (c_ps_user[9].z);
      _184 = _182;
   }
   else
   {
      _184 = _171;
   }
   float _185 = _152 * _184;
   float _188 = (c_ps_user[2].x) * _185;
   float _189 = _188 + 1.0f;
   float _190 = _148 * _189;
   float _191 = _149 * _189;
   float _192 = _150 * _189;
   float _196 = c_rt_size.z * SV_Position.x;
   float _197 = c_rt_size.w * SV_Position.y;
   int _200 = c_bool & 8;
   bool _201 = (_200 == 0);
   if (!_201)
   {
      float4 _203 = s13_t.Sample(s13_s, float2(_196, _197));
      _207 = _203.x;
      _208 = _203.y;
   }
   else
   {
      _207 = 1.0f;
      _208 = 1.0f;
   }
   if (!_201)
   {
      float _213 = (c_sdw_param_ch[0].x) * _207;
      float _214 = (c_sdw_param_ch[0].z) * _208;
      float _217 = _213 + (c_sdw_param_ch[0].y);
      float _218 = _214 + (c_sdw_param_ch[0].w);
      _220 = _217;
      _221 = _218;
   }
   else
   {
      _220 = _207;
      _221 = _208;
   }
   float _222 = min(_220, _221);
   float _231 = (c_light_vec[1].x) + _116;
   float _232 = (c_light_vec[1].y) + _117;
   float _233 = (c_light_vec[1].z) + _118;
   float _234 = dot(float3(_231, _232, _233), float3(_231, _232, _233));
   float _235 = rsqrt(_234);
   float _236 = _231 * _235;
   float _237 = _232 * _235;
   float _238 = _233 * _235;
   float _239 = -0.0f - (c_light_vec[1].x);
   float _240 = -0.0f - (c_light_vec[1].y);
   float _241 = -0.0f - (c_light_vec[1].z);
   float _242 = -0.0f - (c_light_vec[0].x);
   float _243 = -0.0f - (c_light_vec[0].y);
   float _244 = -0.0f - (c_light_vec[0].z);
   float _245 = dot(float3(_116, _117, _118), float3(_107, _108, _109));
   float _246 = _245 * 2.0f;
   float _247 = _246 * _107;
   float _248 = _246 * _108;
   float _249 = _246 * _109;
   float _250 = _116 - _247;
   float _251 = _117 - _248;
   float _252 = _118 - _249;
   float _253 = dot(float3(_250, _251, _252), float3(_242, _243, _244));
   float _254 = dot(float3(_250, _251, _252), float3(_239, _240, _241));
   float _255 = saturate(_253);
   float _256 = saturate(_254);
   float _257 = log2(_255);
   float _258 = log2(_256);
   float _259 = _257 * c_mat_power;
   float _260 = _258 * c_mat_power;
   float _261 = exp2(_259);
   float _262 = exp2(_260);
   float _267 = c_mat_fresnel * c_tone_control.w;
   float _268 = c_mat_power * 0.03974359855055809f;
   float _269 = _268 + 0.0856831967830658f;
   float _270 = _262 * _269;
   float _271 = dot(float3(_116, _117, _118), float3(_124, _125, _126));
   float _272 = dot(float3(_116, _117, _118), float3(_236, _237, _238));
   float _273 = 1.0f - _271;
   float _274 = 1.0f - _272;
   float _275 = _273 * _273;
   float _276 = _274 * _274;
   float _277 = 1.0f - _267;
   float _278 = _275 * _275;
   float _279 = _278 * _273;
   float _280 = _279 * _277;
   float _281 = _274 * _277;
   float _282 = _276 * _276;
   float _283 = _282 * _281;
   float _284 = _280 + _267;
   float _285 = _283 + _267;
   float _286 = dot(float3(_107, _108, _109), float3(_242, _243, _244));
   float _287 = saturate(_286);
   float _288 = dot(float3(_107, _108, _109), float3(_239, _240, _241));
   float _289 = saturate(_288);
   float _290 = _270 * _289;
   float _291 = _290 * _285;
   float _300 = c_specular_scale.x * c_mat_specular.x;
   float _301 = c_specular_scale.y * c_mat_specular.y;
   float _302 = c_specular_scale.z * c_mat_specular.z;
   float _307 = _222 * _269;
   float _308 = _307 * _261;
   float _309 = _308 * _287;
   float _310 = _309 * _284;
   float _311 = (c_light_col[0].x) * _310;
   float _312 = (c_light_col[0].y) * _310;
   float _313 = (c_light_col[0].z) * _310;
   float _318 = (c_light_col[1].x) * _291;
   float _319 = (c_light_col[1].y) * _291;
   float _320 = (c_light_col[1].z) * _291;
   float _321 = _318 + _311;
   float _322 = _319 + _312;
   float _323 = _320 + _313;
   float _324 = _300 * _321;
   float _325 = _301 * _322;
   float _326 = _302 * _323;
   int _330 = c_bool & 1;
   bool _331 = (_330 == 0);
   if (!_331)
   {
      float _333 = -0.0f - TEXCOORD_4.x;
      float _334 = -0.0f - TEXCOORD_4.y;
      float _335 = -0.0f - TEXCOORD_4.z;
      float _336 = dot(float3(_107, _108, _109), float3(_333, _334, _335));
      float _337 = saturate(_336);
      float _338 = _337 * TEXCOORD_1.w;
      float _339 = _337 * TEXCOORD_2.w;
      float _340 = _337 * TEXCOORD_4.w;
      float _341 = max(0.0f, _338);
      float _342 = max(0.0f, _339);
      float _343 = max(0.0f, _340);
      float _344 = -0.0f - TEXCOORD_5.x;
      float _345 = -0.0f - TEXCOORD_5.y;
      float _346 = -0.0f - TEXCOORD_5.z;
      float _347 = dot(float3(_107, _108, _109), float3(_344, _345, _346));
      float _348 = saturate(_347);
      float _349 = _348 * COLOR.w;
      float _350 = _348 * COLOR_1.w;
      float _351 = _348 * TEXCOORD_5.w;
      float _352 = max(0.0f, _349);
      float _353 = max(0.0f, _350);
      float _354 = max(0.0f, _351);
      float _355 = _352 + _341;
      float _356 = _353 + _342;
      float _357 = _354 + _343;
      float _358 = _116 + TEXCOORD_4.x;
      float _359 = _117 + TEXCOORD_4.y;
      float _360 = _118 + TEXCOORD_4.z;
      float _361 = dot(float3(_358, _359, _360), float3(_358, _359, _360));
      float _362 = rsqrt(_361);
      float _363 = _116 + TEXCOORD_5.x;
      float _364 = _117 + TEXCOORD_5.y;
      float _365 = _118 + TEXCOORD_5.z;
      float _366 = dot(float3(_363, _364, _365), float3(_363, _364, _365));
      float _367 = rsqrt(_366);
      float _368 = _363 * _367;
      float _369 = -0.0f - _368;
      float _370 = _364 * _367;
      float _371 = -0.0f - _370;
      float _372 = _365 * _367;
      float _373 = -0.0f - _372;
      float _374 = _358 * _362;
      float _375 = -0.0f - _374;
      float _376 = _359 * _362;
      float _377 = -0.0f - _376;
      float _378 = _360 * _362;
      float _379 = -0.0f - _378;
      float _380 = dot(float3(_375, _377, _379), float3(_107, _108, _109));
      float _381 = dot(float3(_369, _371, _373), float3(_107, _108, _109));
      float _382 = saturate(_380);
      float _383 = saturate(_381);
      float _384 = log2(_382);
      float _385 = log2(_383);
      float _386 = _384 * c_mat_power;
      float _387 = _385 * c_mat_power;
      float _388 = exp2(_386);
      float _389 = exp2(_387);
      float _390 = _388 * TEXCOORD_1.w;
      float _391 = _388 * TEXCOORD_2.w;
      float _392 = _388 * TEXCOORD_4.w;
      float _393 = _389 * COLOR.w;
      float _394 = _389 * COLOR_1.w;
      float _395 = _389 * TEXCOORD_5.w;
      float _396 = _393 + _390;
      float _397 = _394 + _391;
      float _398 = _395 + _392;
      float _399 = max(0.0f, _396);
      float _400 = max(0.0f, _397);
      float _401 = max(0.0f, _398);
      float _402 = _300 * _399;
      float _403 = _301 * _400;
      float _404 = _302 * _401;
      float _405 = min(_402, 1.0f);
      float _406 = min(_403, 1.0f);
      float _407 = min(_404, 1.0f);
      _409 = _405;
      _410 = _406;
      _411 = _407;
      _412 = _355;
      _413 = _356;
      _414 = _357;
   }
   else
   {
      _409 = 0.0f;
      _410 = 0.0f;
      _411 = 0.0f;
      _412 = 0.0f;
      _413 = 0.0f;
      _414 = 0.0f;
   }
   float _415 = c_rt_size.z * SV_Position.x;
   float _416 = c_rt_size.w * SV_Position.y;
   float4 _417 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_415, _416)));
   int _424 = c_bool & 16384;
   bool _425 = (_424 == 0);
   if (!_425)
   {
      float _427 = _417.x * 4.0f;
      float _428 = _417.y * 4.0f;
      float _429 = _417.z * 4.0f;
      float _430 = _417.w * 4.0f;
      _447 = _427;
      _448 = _428;
      _449 = _429;
      _450 = _430;
   }
   else
   {
      int _432 = c_bool & 8192;
      bool _433 = (_432 == 0);
      if (!_433)
      {
         float _435 = _417.x * 2.0f;
         float _436 = _417.y * 2.0f;
         float _437 = _417.z * 2.0f;
         float _438 = _417.w * 2.0f;
         _447 = _435;
         _448 = _436;
         _449 = _437;
         _450 = _438;
      }
      else
      {
         float _442 = _417.x / c_tone_control.z;
         float _443 = _417.y / c_tone_control.z;
         float _444 = _417.z / c_tone_control.z;
         float _445 = _417.w / c_tone_control.z;
         _447 = _442;
         _448 = _443;
         _449 = _444;
         _450 = _445;
      }
   }
   float _451 = _447 + _412;
   float _452 = _448 + _413;
   float _453 = _449 + _414;
   float _454 = _450 + _409;
   float _455 = _450 + _410;
   float _456 = _450 + _411;
   float _519 = dot(float3(_107, _108, _109), float3(_107, _108, _109));
   float _520 = rsqrt(_519);
   float _521 = _520 * _107;
   float _522 = _520 * _108;
   float _523 = _520 * _109;
   float _524 = dot(float4((c_ps_scratch[80].x), (c_ps_scratch[80].y), (c_ps_scratch[80].z), (c_ps_scratch[80].w)), float4(_521, _522, _523, 1.0f));
   float _525 = dot(float4((c_ps_scratch[81].x), (c_ps_scratch[81].y), (c_ps_scratch[81].z), (c_ps_scratch[81].w)), float4(_521, _522, _523, 1.0f));
   float _526 = dot(float4((c_ps_scratch[82].x), (c_ps_scratch[82].y), (c_ps_scratch[82].z), (c_ps_scratch[82].w)), float4(_521, _522, _523, 1.0f));
   float _527 = dot(float4((c_ps_scratch[83].x), (c_ps_scratch[83].y), (c_ps_scratch[83].z), (c_ps_scratch[83].w)), float4(_521, _522, _523, 1.0f));
   float _528 = dot(float4(_521, _522, _523, 1.0f), float4(_524, _525, _526, _527));
   float _529 = dot(float4((c_ps_scratch[84].x), (c_ps_scratch[84].y), (c_ps_scratch[84].z), (c_ps_scratch[84].w)), float4(_521, _522, _523, 1.0f));
   float _530 = dot(float4((c_ps_scratch[85].x), (c_ps_scratch[85].y), (c_ps_scratch[85].z), (c_ps_scratch[85].w)), float4(_521, _522, _523, 1.0f));
   float _531 = dot(float4((c_ps_scratch[86].x), (c_ps_scratch[86].y), (c_ps_scratch[86].z), (c_ps_scratch[86].w)), float4(_521, _522, _523, 1.0f));
   float _532 = dot(float4((c_ps_scratch[87].x), (c_ps_scratch[87].y), (c_ps_scratch[87].z), (c_ps_scratch[87].w)), float4(_521, _522, _523, 1.0f));
   float _533 = dot(float4(_521, _522, _523, 1.0f), float4(_529, _530, _531, _532));
   float _534 = dot(float4((c_ps_scratch[88].x), (c_ps_scratch[88].y), (c_ps_scratch[88].z), (c_ps_scratch[88].w)), float4(_521, _522, _523, 1.0f));
   float _535 = dot(float4((c_ps_scratch[89].x), (c_ps_scratch[89].y), (c_ps_scratch[89].z), (c_ps_scratch[89].w)), float4(_521, _522, _523, 1.0f));
   float _536 = dot(float4((c_ps_scratch[90].x), (c_ps_scratch[90].y), (c_ps_scratch[90].z), (c_ps_scratch[90].w)), float4(_521, _522, _523, 1.0f));
   float _537 = dot(float4((c_ps_scratch[91].x), (c_ps_scratch[91].y), (c_ps_scratch[91].z), (c_ps_scratch[91].w)), float4(_521, _522, _523, 1.0f));
   float _538 = dot(float4(_521, _522, _523, 1.0f), float4(_534, _535, _536, _537));
   float _539 = max(_528, 0.0625f);
   float _540 = max(_533, 0.0625f);
   float _541 = max(_538, 0.0625f);
   float _542 = _539 * (c_ps_scratch[79].x);
   float _543 = _540 * (c_ps_scratch[79].x);
   float _544 = _541 * (c_ps_scratch[79].x);
   float _549 = -0.0f - (c_light_vec[0].x);
   float _550 = -0.0f - (c_light_vec[0].y);
   float _551 = -0.0f - (c_light_vec[0].z);
   float _552 = dot(float3(_107, _108, _109), float3(_549, _550, _551));
   float _553 = -0.0f - (c_light_vec[1].x);
   float _554 = -0.0f - (c_light_vec[1].y);
   float _555 = -0.0f - (c_light_vec[1].z);
   float _556 = dot(float3(_107, _108, _109), float3(_553, _554, _555));
   float _565 = _108 * 0.5f;
   float _566 = _565 + 0.5f;
   float _567 = (c_light_col[4].x) - (c_light_vec[4].x);
   float _568 = (c_light_col[4].y) - (c_light_vec[4].y);
   float _569 = (c_light_col[4].z) - (c_light_vec[4].z);
   float _570 = _567 * _566;
   float _571 = _568 * _566;
   float _572 = _569 * _566;
   float _573 = saturate(_552);
   float _574 = saturate(_556);
   float _579 = (c_light_col[0].x) * _573;
   float _580 = (c_light_col[0].y) * _573;
   float _581 = (c_light_col[0].z) * _573;
   float _586 = (c_light_col[1].x) * _574;
   float _587 = (c_light_col[1].y) * _574;
   float _588 = (c_light_col[1].z) * _574;
   float _589 = _454 + _324;
   float _590 = _455 + _325;
   float _591 = _456 + _326;
   int _592 = c_bool & 32768;
   bool _593 = (_592 == 0);
   float _602 = (c_light_vec[4].x) + _542;
   float _603 = _602 + _570;
   float _604 = (c_light_vec[4].y) + _543;
   float _605 = _604 + _571;
   float _606 = (c_light_vec[4].z) + _544;
   float _607 = _606 + _572;
   if (!_593)
   {
      float _611 = (c_ps_user[11].x) * _190;
      float _612 = (c_ps_user[11].x) * _191;
      float _613 = (c_ps_user[11].x) * _192;
      float _615 = _107 + 1.0f;
      float _616 = _108 + 1.0f;
      float _617 = _109 + 1.0f;
      float _618 = _615 * 0.5f;
      float _619 = _616 * 0.5f;
      float _620 = _617 * 0.5f;
      float _621 = saturate(_618);
      float _622 = saturate(_619);
      float _623 = saturate(_620);
      float _624 = (c_ps_user[11].y) * _621;
      float _625 = _622 * (c_ps_user[11].y);
      float _626 = _623 * (c_ps_user[11].y);
      float _637 = (c_ps_user[11].w) * _589;
      float _640 = (c_ps_user[12].x) * _222;
      float _646 = _165 * _165;
      float _647 = _165 * 0.9722222089767456f;
      float _648 = _646 * _646;
      float _649 = _648 * _647;
      float _650 = _649 + 0.02777777425944805f;
      float _651 = (c_ps_user[13].z) * _650;
      float _653 = (c_ps_user[13].w) * _603;
      float _654 = (c_ps_user[13].w) * _605;
      float _655 = (c_ps_user[13].w) * _607;
      float _658 = (c_ps_user[14].y) * _579;
      float _659 = (c_ps_user[14].y) * _580;
      float _660 = (c_ps_user[14].y) * _581;
      float _662 = (c_ps_user[14].z) * _586;
      float _663 = (c_ps_user[14].z) * _587;
      float _664 = (c_ps_user[14].z) * _588;
      float _666 = (c_ps_user[14].w) * _451;
      float _667 = (c_ps_user[14].w) * _452;
      float _668 = (c_ps_user[14].w) * _453;
      float _673 = (c_ps_user[15].z) * _454;
      float _674 = (c_ps_user[15].z) * _455;
      float _675 = (c_ps_user[15].z) * _456;
      float _676 = (c_ps_user[15].y) + (c_ps_user[15].x);
      float _677 = c_specular_scale.x * _676;
      float _678 = _677 * _589;
      float _679 = _658 + (c_ps_user[13].x);
      float _680 = _679 + _662;
      float _681 = c_diffuse_scale.x * _680;
      float _682 = _681 * c_mat_diffuse.x;
      float _683 = (c_ps_user[13].y) + (c_ps_user[11].z);
      float _684 = _683 * c_specular_scale.x;
      float _685 = _684 + _678;
      float _686 = _685 * c_mat_specular.x;
      float _687 = _624 + _611;
      float _688 = _687 + _637;
      float _689 = _688 + (c_ps_user[12].y);
      float _690 = _689 + _640;
      float _691 = _690 + _653;
      float _692 = _691 + _651;
      float _693 = _692 + _666;
      float _694 = _693 + _673;
      float _695 = _694 + _682;
      float _696 = _695 + _686;
      float _697 = c_specular_scale.y * _676;
      float _698 = _697 * _590;
      float _699 = _659 + (c_ps_user[13].x);
      float _700 = _699 + _663;
      float _701 = c_diffuse_scale.y * _700;
      float _702 = _701 * c_mat_diffuse.y;
      float _703 = _683 * c_specular_scale.y;
      float _704 = _703 + _698;
      float _705 = _704 * c_mat_specular.y;
      float _706 = _637 + _612;
      float _707 = _706 + _625;
      float _708 = _707 + (c_ps_user[12].y);
      float _709 = _708 + _640;
      float _710 = _709 + _654;
      float _711 = _710 + _651;
      float _712 = _711 + _667;
      float _713 = _712 + _674;
      float _714 = _713 + _702;
      float _715 = _714 + _705;
      float _716 = c_specular_scale.z * _676;
      float _717 = _716 * _591;
      float _718 = _660 + (c_ps_user[13].x);
      float _719 = _718 + _664;
      float _720 = c_diffuse_scale.z * _719;
      float _721 = _720 * c_mat_diffuse.z;
      float _722 = _683 * c_specular_scale.z;
      float _723 = _722 + _717;
      float _724 = _723 * c_mat_specular.z;
      float _725 = _637 + _613;
      float _726 = _725 + _626;
      float _727 = _726 + (c_ps_user[12].y);
      float _728 = _727 + _640;
      float _729 = _728 + _655;
      float _730 = _729 + _651;
      float _731 = _730 + _668;
      float _732 = _731 + _675;
      float _733 = _732 + _721;
      float _734 = _733 + _724;
      float _735 = saturate(_696);
      float _736 = saturate(_715);
      float _737 = saturate(_734);
      _764 = _735;
      _765 = _736;
      _766 = _737;
   }
   else
   {
      float _739 = _581 * _222;
      float _740 = _607 + _453;
      float _741 = _740 + _588;
      float _742 = _741 + _739;
      float _743 = _580 * _222;
      float _744 = _605 + _452;
      float _745 = _744 + _587;
      float _746 = _745 + _743;
      float _747 = _579 * _222;
      float _748 = _603 + _451;
      float _749 = _748 + _586;
      float _750 = _749 + _747;
      float _751 = c_diffuse_scale.x * c_mat_diffuse.x;
      float _752 = _751 * _190;
      float _753 = _752 * _750;
      float _754 = c_diffuse_scale.y * c_mat_diffuse.y;
      float _755 = _754 * _191;
      float _756 = _755 * _746;
      float _757 = c_diffuse_scale.z * c_mat_diffuse.z;
      float _758 = _757 * _192;
      float _759 = _758 * _742;
      float _760 = _753 + _589;
      float _761 = _756 + _590;
      float _762 = _759 + _591;
      _764 = _760;
      _765 = _761;
      _766 = _762;
   }
   float _771 = c_mat_opacity * _52.w;
   float _772 = _771 * c_opacity_scale;
   float _773 = _772 * c_global_opacity;
   float _774 = _764 * COLOR.x;
   float _775 = _765 * COLOR.y;
   float _776 = _766 * COLOR.z;
   int _777 = c_bool & 16;
   bool _778 = (_777 == 0);
   if (!_778)
   {
      float _782 = c_tone_control.x * _774;
      float _783 = c_tone_control.x * _775;
      float _784 = c_tone_control.x * _776;
      float _786 = log2(_782);
      float _787 = log2(_783);
      float _788 = log2(_784);
      float _789 = _786 * c_tone_control.y;
      float _790 = _787 * c_tone_control.y;
      float _791 = _788 * c_tone_control.y;
      float _792 = exp2(_789);
      float _793 = exp2(_790);
      float _794 = exp2(_791);
      _842 = _792;
      _843 = _793;
      _844 = _794;
   }
   else
   {
      if (_593)
      {
         float _799 = c_tone_control.x * _774;
         float _800 = c_tone_control.x * _775;
         float _801 = c_tone_control.x * _776;
         float _802 = _799 * 2.0f;
         float _803 = _800 * 2.0f;
         float _804 = _801 * 2.0f;
         float _805 = _799 * 0.30000001192092896f;
         float _806 = _800 * 0.30000001192092896f;
         float _807 = _801 * 0.30000001192092896f;
         float _808 = _805 + 0.05000000074505806f;
         float _809 = _806 + 0.05000000074505806f;
         float _810 = _807 + 0.05000000074505806f;
         float _811 = _808 * _802;
         float _812 = _809 * _803;
         float _813 = _810 * _804;
         float _814 = _811 + 0.004000000189989805f;
         float _815 = _812 + 0.004000000189989805f;
         float _816 = _813 + 0.004000000189989805f;
         float _817 = _805 + 0.5f;
         float _818 = _806 + 0.5f;
         float _819 = _807 + 0.5f;
         float _820 = _817 * _802;
         float _821 = _818 * _803;
         float _822 = _819 * _804;
         float _823 = _820 + 0.06000000238418579f;
         float _824 = _821 + 0.06000000238418579f;
         float _825 = _822 + 0.06000000238418579f;
         float _826 = _814 / _823;
         float _827 = _815 / _824;
         float _828 = _816 / _825;
         float _829 = _826 + -0.06666666269302368f;
         float _830 = _827 + -0.06666666269302368f;
         float _831 = _828 + -0.06666666269302368f;
         float _832 = _829 * 1.3790643215179443f;
         float _833 = _830 * 1.3790643215179443f;
         float _834 = _831 * 1.3790643215179443f;
         float _835 = YKR_MaterialCurve(_832, _799);
         float _836 = YKR_MaterialCurve(_833, _800);
         float _837 = YKR_MaterialCurve(_834, _801);
         float _838 = sqrt(_835);
         float _839 = sqrt(_836);
         float _840 = sqrt(_837);
         _842 = _838;
         _843 = _839;
         _844 = _840;
      }
      else
      {
         _842 = _774;
         _843 = _775;
         _844 = _776;
      }
   }
   int _845 = c_bool & 4;
   bool _846 = (_845 == 0);
   if (_846)
   {
      float _848 = saturate(COLOR_1.x);
      float _853 = (c_fog_param[0].x) - _842;
      float _854 = (c_fog_param[0].y) - _843;
      float _855 = (c_fog_param[0].z) - _844;
      float _856 = _853 * _848;
      float _857 = _854 * _848;
      float _858 = _855 * _848;
      _860 = _856;
      _861 = _857;
      _862 = _858;
   }
   else
   {
      _860 = COLOR_1.x;
      _861 = COLOR_1.y;
      _862 = COLOR_1.z;
   }
   float _863 = _860 + _842;
   float _864 = _861 + _843;
   float _865 = _862 + _844;
   if (!_593)
   {
      float _873 = (c_ps_user[3].x) * (c_ps_user[12].z);
      float _874 = (c_ps_user[3].y) * (c_ps_user[12].z);
      float _875 = (c_ps_user[3].z) * (c_ps_user[12].z);
      float _877 = (c_ps_user[12].w) * COLOR.x;
      float _878 = (c_ps_user[12].w) * COLOR.y;
      float _879 = (c_ps_user[12].w) * COLOR.z;
      float _880 = _877 + _863;
      float _881 = _880 + _873;
      float _882 = _878 + _864;
      float _883 = _882 + _874;
      float _884 = _879 + _865;
      float _885 = _884 + _875;
      _887 = _881;
      _888 = _883;
      _889 = _885;
   }
   else
   {
      _887 = _863;
      _888 = _864;
      _889 = _865;
   }
   float _895 = (c_ps_col_scl[0].x) * _887;
   float _896 = (c_ps_col_scl[0].y) * _888;
   float _897 = (c_ps_col_scl[0].z) * _889;
   float _898 = (c_ps_col_scl[0].w) * _773;
   SV_Target.x = _895;
   SV_Target.y = _896;
   SV_Target.z = _897;
   SV_Target.w = _898;
   int _901 = c_bool & 8;
   bool _902 = (_901 != 0);
   float _903 = select(_902, _887, _895);
   float _904 = select(_902, _888, _896);
   float _905 = select(_902, _889, _897);
   float _906 = select(_902, _773, _898);
   SV_Target.x = _903;
   SV_Target.y = _904;
   SV_Target.z = _905;
   SV_Target.w = _906;
   return SV_Target;
}
