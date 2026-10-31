// Yakuza Kiwami material s_o1dst[iris] (0x3DF4EF98, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
// applied by a script: the inline Hable tonemap's saturate goes through
// YKR_MaterialCurve (tangent continuation in HDR), and the light pre-pass buffer t14 through YKR_LightBuffer.

#include "Includes/Common.hlsl"

Texture2D<float4> s0_t : register(t0);

Texture2D<float4> s3_t : register(t3);

Texture2D<float4> s5_t : register(t5);

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

SamplerState s3_s : register(s3);

SamplerState s5_s : register(s5);

SamplerState s13_s : register(s13);

SamplerState s14_s : register(s14);

float4 main(
    linear float4 COLOR : COLOR,
    linear float4 COLOR_1 : COLOR1,
    linear float3 TEXCOORD : TEXCOORD,
    linear float4 TEXCOORD_1 : TEXCOORD1,
    linear float4 TEXCOORD_2 : TEXCOORD2,
    linear float4 TEXCOORD_3 : TEXCOORD3,
    linear float4 TEXCOORD_4 : TEXCOORD4,
    linear float4 TEXCOORD_5 : TEXCOORD5,
    noperspective float4 SV_Position : SV_Position) : SV_Target
{
   float4 SV_Target = 0;
   float4 _53 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _58 = _53.x * _53.x;
   float _59 = _53.y * _53.y;
   float _60 = _53.z * _53.z;
   float4 _61 = s3_t.Sample(s3_s, float2(TEXCOORD.x, TEXCOORD.y));
   float4 _65 = s5_t.Sample(s5_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _70 = _65.x * 2.0f;
   float _71 = _65.y * 2.0f;
   float _72 = _70 + -1.0f;
   float _73 = _71 + -1.0f;
   float _74 = dot(float2(_72, _73), float2(_72, _73));
   float _75 = 1.0f - _74;
   float _76 = saturate(_75);
   float _77 = sqrt(_76);
   float _78 = TEXCOORD_2.y * TEXCOORD_3.z;
   float _79 = TEXCOORD_2.z * TEXCOORD_3.y;
   float _80 = _78 - _79;
   float _81 = TEXCOORD_2.z * TEXCOORD_3.x;
   float _82 = TEXCOORD_2.x * TEXCOORD_3.z;
   float _83 = _81 - _82;
   float _84 = TEXCOORD_2.x * TEXCOORD_3.y;
   float _85 = TEXCOORD_2.y * TEXCOORD_3.x;
   float _86 = _84 - _85;
   float _87 = _72 * TEXCOORD_3.x;
   float _88 = _72 * TEXCOORD_3.y;
   float _89 = _72 * TEXCOORD_3.z;
   float _90 = _73 * TEXCOORD_3.w;
   float _91 = _90 * _80;
   float _92 = _90 * _83;
   float _93 = _90 * _86;
   float _94 = _91 + _87;
   float _95 = _92 + _88;
   float _96 = _93 + _89;
   float _97 = _77 * TEXCOORD_2.x;
   float _98 = _77 * TEXCOORD_2.y;
   float _99 = _77 * TEXCOORD_2.z;
   float _100 = _94 + _97;
   float _101 = _95 + _98;
   float _102 = _96 + _99;
   float _103 = dot(float3(_100, _101, _102), float3(_100, _101, _102));
   float _104 = rsqrt(_103);
   float _105 = _100 * _104;
   float _106 = _101 * _104;
   float _107 = _102 * _104;
   float _112 = dot(float3(TEXCOORD_1.x, TEXCOORD_1.y, TEXCOORD_1.z), float3(TEXCOORD_1.x, TEXCOORD_1.y, TEXCOORD_1.z));
   float _113 = rsqrt(_112);
   float _114 = _113 * TEXCOORD_1.x;
   float _115 = _113 * TEXCOORD_1.y;
   float _116 = _113 * TEXCOORD_1.z;
   float _117 = _114 + (c_light_vec[0].x);
   float _118 = _115 + (c_light_vec[0].y);
   float _119 = _116 + (c_light_vec[0].z);
   float _120 = dot(float3(_117, _118, _119), float3(_117, _118, _119));
   float _121 = rsqrt(_120);
   float _122 = _117 * _121;
   float _123 = _118 * _121;
   float _124 = _119 * _121;
   float _125 = _106 + 0.10000000149011612f;
   float _126 = saturate(_125);
   float _129 = (c_attribute_ps[2].w) + 2.0f;
   float _131 = (c_attribute_ps[2].z) + 2.5f;
   float _132 = -0.0f - _114;
   float _133 = -0.0f - _115;
   float _134 = -0.0f - _116;
   float _135 = dot(float3(_132, _133, _134), float3(TEXCOORD_2.x, TEXCOORD_2.y, TEXCOORD_2.z));
   float _136 = 1.0f - _135;
   float _137 = saturate(_136);
   float _138 = dot(float3(_132, _133, _134), float3(_105, _106, _107));
   float _139 = 1.0f - _138;
   float _140 = saturate(_139);
   float _141 = _140 * _137;
   float _142 = log2(_141);
   float _143 = _142 * _129;
   float _144 = exp2(_143);
   float _145 = _144 * _131;
   int _148 = c_bool & 4096;
   bool _149 = (_148 == 0);
   float _158;
   float _181;
   float _182;
   float _194;
   float _195;
   float _383;
   float _384;
   float _385;
   float _386;
   float _387;
   float _388;
   float _421;
   float _422;
   float _423;
   float _424;
   float _755;
   float _756;
   float _757;
   float _833;
   float _834;
   float _835;
   float _851;
   float _852;
   float _853;
   float _878;
   float _879;
   float _880;
   if (!_149)
   {
      float _154 = (c_ps_user[9].w) * _142;
      float _155 = exp2(_154);
      float _156 = _155 * (c_ps_user[9].z);
      _158 = _156;
   }
   else
   {
      _158 = _145;
   }
   float _159 = _126 * _158;
   float _162 = (c_ps_user[2].x) * _159;
   float _163 = _162 + 1.0f;
   float _164 = _58 * _163;
   float _165 = _59 * _163;
   float _166 = _60 * _163;
   float _170 = c_rt_size.z * SV_Position.x;
   float _171 = c_rt_size.w * SV_Position.y;
   int _174 = c_bool & 8;
   bool _175 = (_174 == 0);
   if (!_175)
   {
      float4 _177 = s13_t.Sample(s13_s, float2(_170, _171));
      _181 = _177.x;
      _182 = _177.y;
   }
   else
   {
      _181 = 1.0f;
      _182 = 1.0f;
   }
   if (!_175)
   {
      float _187 = (c_sdw_param_ch[0].x) * _181;
      float _188 = (c_sdw_param_ch[0].z) * _182;
      float _191 = _187 + (c_sdw_param_ch[0].y);
      float _192 = _188 + (c_sdw_param_ch[0].w);
      _194 = _191;
      _195 = _192;
   }
   else
   {
      _194 = _181;
      _195 = _182;
   }
   float _196 = min(_194, _195);
   float _205 = (c_light_vec[1].x) + _114;
   float _206 = (c_light_vec[1].y) + _115;
   float _207 = (c_light_vec[1].z) + _116;
   float _208 = dot(float3(_205, _206, _207), float3(_205, _206, _207));
   float _209 = rsqrt(_208);
   float _210 = _205 * _209;
   float _211 = _206 * _209;
   float _212 = _207 * _209;
   float _213 = -0.0f - (c_light_vec[1].x);
   float _214 = -0.0f - (c_light_vec[1].y);
   float _215 = -0.0f - (c_light_vec[1].z);
   float _216 = -0.0f - (c_light_vec[0].x);
   float _217 = -0.0f - (c_light_vec[0].y);
   float _218 = -0.0f - (c_light_vec[0].z);
   float _219 = dot(float3(_114, _115, _116), float3(_105, _106, _107));
   float _220 = _219 * 2.0f;
   float _221 = _220 * _105;
   float _222 = _220 * _106;
   float _223 = _220 * _107;
   float _224 = _114 - _221;
   float _225 = _115 - _222;
   float _226 = _116 - _223;
   float _227 = dot(float3(_224, _225, _226), float3(_216, _217, _218));
   float _228 = dot(float3(_224, _225, _226), float3(_213, _214, _215));
   float _229 = saturate(_227);
   float _230 = saturate(_228);
   float _231 = log2(_229);
   float _232 = log2(_230);
   float _233 = _231 * c_mat_power;
   float _234 = _232 * c_mat_power;
   float _235 = exp2(_233);
   float _236 = exp2(_234);
   float _241 = c_mat_fresnel * c_tone_control.w;
   float _242 = c_mat_power * 0.03974359855055809f;
   float _243 = _242 + 0.0856831967830658f;
   float _244 = _236 * _243;
   float _245 = dot(float3(_114, _115, _116), float3(_122, _123, _124));
   float _246 = dot(float3(_114, _115, _116), float3(_210, _211, _212));
   float _247 = 1.0f - _245;
   float _248 = 1.0f - _246;
   float _249 = _247 * _247;
   float _250 = _248 * _248;
   float _251 = 1.0f - _241;
   float _252 = _249 * _249;
   float _253 = _252 * _247;
   float _254 = _253 * _251;
   float _255 = _248 * _251;
   float _256 = _250 * _250;
   float _257 = _256 * _255;
   float _258 = _254 + _241;
   float _259 = _257 + _241;
   float _260 = dot(float3(_105, _106, _107), float3(_216, _217, _218));
   float _261 = saturate(_260);
   float _262 = dot(float3(_105, _106, _107), float3(_213, _214, _215));
   float _263 = saturate(_262);
   float _264 = _244 * _263;
   float _265 = _264 * _259;
   float _274 = c_specular_scale.x * c_mat_specular.x;
   float _275 = c_specular_scale.y * c_mat_specular.y;
   float _276 = c_specular_scale.z * c_mat_specular.z;
   float _281 = _196 * _243;
   float _282 = _281 * _235;
   float _283 = _282 * _261;
   float _284 = _283 * _258;
   float _285 = (c_light_col[0].x) * _284;
   float _286 = (c_light_col[0].y) * _284;
   float _287 = (c_light_col[0].z) * _284;
   float _292 = (c_light_col[1].x) * _265;
   float _293 = (c_light_col[1].y) * _265;
   float _294 = (c_light_col[1].z) * _265;
   float _295 = _292 + _285;
   float _296 = _293 + _286;
   float _297 = _294 + _287;
   float _298 = _274 * _295;
   float _299 = _275 * _296;
   float _300 = _276 * _297;
   int _304 = c_bool & 1;
   bool _305 = (_304 == 0);
   if (!_305)
   {
      float _307 = -0.0f - TEXCOORD_4.x;
      float _308 = -0.0f - TEXCOORD_4.y;
      float _309 = -0.0f - TEXCOORD_4.z;
      float _310 = dot(float3(_105, _106, _107), float3(_307, _308, _309));
      float _311 = saturate(_310);
      float _312 = _311 * TEXCOORD_1.w;
      float _313 = _311 * TEXCOORD_2.w;
      float _314 = _311 * TEXCOORD_4.w;
      float _315 = max(0.0f, _312);
      float _316 = max(0.0f, _313);
      float _317 = max(0.0f, _314);
      float _318 = -0.0f - TEXCOORD_5.x;
      float _319 = -0.0f - TEXCOORD_5.y;
      float _320 = -0.0f - TEXCOORD_5.z;
      float _321 = dot(float3(_105, _106, _107), float3(_318, _319, _320));
      float _322 = saturate(_321);
      float _323 = _322 * COLOR.w;
      float _324 = _322 * COLOR_1.w;
      float _325 = _322 * TEXCOORD_5.w;
      float _326 = max(0.0f, _323);
      float _327 = max(0.0f, _324);
      float _328 = max(0.0f, _325);
      float _329 = _326 + _315;
      float _330 = _327 + _316;
      float _331 = _328 + _317;
      float _332 = _114 + TEXCOORD_4.x;
      float _333 = _115 + TEXCOORD_4.y;
      float _334 = _116 + TEXCOORD_4.z;
      float _335 = dot(float3(_332, _333, _334), float3(_332, _333, _334));
      float _336 = rsqrt(_335);
      float _337 = _114 + TEXCOORD_5.x;
      float _338 = _115 + TEXCOORD_5.y;
      float _339 = _116 + TEXCOORD_5.z;
      float _340 = dot(float3(_337, _338, _339), float3(_337, _338, _339));
      float _341 = rsqrt(_340);
      float _342 = _337 * _341;
      float _343 = -0.0f - _342;
      float _344 = _338 * _341;
      float _345 = -0.0f - _344;
      float _346 = _339 * _341;
      float _347 = -0.0f - _346;
      float _348 = _332 * _336;
      float _349 = -0.0f - _348;
      float _350 = _333 * _336;
      float _351 = -0.0f - _350;
      float _352 = _334 * _336;
      float _353 = -0.0f - _352;
      float _354 = dot(float3(_349, _351, _353), float3(_105, _106, _107));
      float _355 = dot(float3(_343, _345, _347), float3(_105, _106, _107));
      float _356 = saturate(_354);
      float _357 = saturate(_355);
      float _358 = log2(_356);
      float _359 = log2(_357);
      float _360 = _358 * c_mat_power;
      float _361 = _359 * c_mat_power;
      float _362 = exp2(_360);
      float _363 = exp2(_361);
      float _364 = _362 * TEXCOORD_1.w;
      float _365 = _362 * TEXCOORD_2.w;
      float _366 = _362 * TEXCOORD_4.w;
      float _367 = _363 * COLOR.w;
      float _368 = _363 * COLOR_1.w;
      float _369 = _363 * TEXCOORD_5.w;
      float _370 = _367 + _364;
      float _371 = _368 + _365;
      float _372 = _369 + _366;
      float _373 = max(0.0f, _370);
      float _374 = max(0.0f, _371);
      float _375 = max(0.0f, _372);
      float _376 = _274 * _373;
      float _377 = _275 * _374;
      float _378 = _276 * _375;
      float _379 = min(_376, 1.0f);
      float _380 = min(_377, 1.0f);
      float _381 = min(_378, 1.0f);
      _383 = _379;
      _384 = _380;
      _385 = _381;
      _386 = _329;
      _387 = _330;
      _388 = _331;
   }
   else
   {
      _383 = 0.0f;
      _384 = 0.0f;
      _385 = 0.0f;
      _386 = 0.0f;
      _387 = 0.0f;
      _388 = 0.0f;
   }
   float _389 = c_rt_size.z * SV_Position.x;
   float _390 = c_rt_size.w * SV_Position.y;
   float4 _391 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_389, _390)));
   int _398 = c_bool & 16384;
   bool _399 = (_398 == 0);
   if (!_399)
   {
      float _401 = _391.x * 4.0f;
      float _402 = _391.y * 4.0f;
      float _403 = _391.z * 4.0f;
      float _404 = _391.w * 4.0f;
      _421 = _401;
      _422 = _402;
      _423 = _403;
      _424 = _404;
   }
   else
   {
      int _406 = c_bool & 8192;
      bool _407 = (_406 == 0);
      if (!_407)
      {
         float _409 = _391.x * 2.0f;
         float _410 = _391.y * 2.0f;
         float _411 = _391.z * 2.0f;
         float _412 = _391.w * 2.0f;
         _421 = _409;
         _422 = _410;
         _423 = _411;
         _424 = _412;
      }
      else
      {
         float _416 = _391.x / c_tone_control.z;
         float _417 = _391.y / c_tone_control.z;
         float _418 = _391.z / c_tone_control.z;
         float _419 = _391.w / c_tone_control.z;
         _421 = _416;
         _422 = _417;
         _423 = _418;
         _424 = _419;
      }
   }
   float _425 = _421 + _386;
   float _426 = _422 + _387;
   float _427 = _423 + _388;
   float _428 = _424 + _383;
   float _429 = _424 + _384;
   float _430 = _424 + _385;
   float _493 = dot(float3(_105, _106, _107), float3(_105, _106, _107));
   float _494 = rsqrt(_493);
   float _495 = _494 * _105;
   float _496 = _494 * _106;
   float _497 = _494 * _107;
   float _498 = dot(float4((c_ps_scratch[80].x), (c_ps_scratch[80].y), (c_ps_scratch[80].z), (c_ps_scratch[80].w)), float4(_495, _496, _497, 1.0f));
   float _499 = dot(float4((c_ps_scratch[81].x), (c_ps_scratch[81].y), (c_ps_scratch[81].z), (c_ps_scratch[81].w)), float4(_495, _496, _497, 1.0f));
   float _500 = dot(float4((c_ps_scratch[82].x), (c_ps_scratch[82].y), (c_ps_scratch[82].z), (c_ps_scratch[82].w)), float4(_495, _496, _497, 1.0f));
   float _501 = dot(float4((c_ps_scratch[83].x), (c_ps_scratch[83].y), (c_ps_scratch[83].z), (c_ps_scratch[83].w)), float4(_495, _496, _497, 1.0f));
   float _502 = dot(float4(_495, _496, _497, 1.0f), float4(_498, _499, _500, _501));
   float _503 = dot(float4((c_ps_scratch[84].x), (c_ps_scratch[84].y), (c_ps_scratch[84].z), (c_ps_scratch[84].w)), float4(_495, _496, _497, 1.0f));
   float _504 = dot(float4((c_ps_scratch[85].x), (c_ps_scratch[85].y), (c_ps_scratch[85].z), (c_ps_scratch[85].w)), float4(_495, _496, _497, 1.0f));
   float _505 = dot(float4((c_ps_scratch[86].x), (c_ps_scratch[86].y), (c_ps_scratch[86].z), (c_ps_scratch[86].w)), float4(_495, _496, _497, 1.0f));
   float _506 = dot(float4((c_ps_scratch[87].x), (c_ps_scratch[87].y), (c_ps_scratch[87].z), (c_ps_scratch[87].w)), float4(_495, _496, _497, 1.0f));
   float _507 = dot(float4(_495, _496, _497, 1.0f), float4(_503, _504, _505, _506));
   float _508 = dot(float4((c_ps_scratch[88].x), (c_ps_scratch[88].y), (c_ps_scratch[88].z), (c_ps_scratch[88].w)), float4(_495, _496, _497, 1.0f));
   float _509 = dot(float4((c_ps_scratch[89].x), (c_ps_scratch[89].y), (c_ps_scratch[89].z), (c_ps_scratch[89].w)), float4(_495, _496, _497, 1.0f));
   float _510 = dot(float4((c_ps_scratch[90].x), (c_ps_scratch[90].y), (c_ps_scratch[90].z), (c_ps_scratch[90].w)), float4(_495, _496, _497, 1.0f));
   float _511 = dot(float4((c_ps_scratch[91].x), (c_ps_scratch[91].y), (c_ps_scratch[91].z), (c_ps_scratch[91].w)), float4(_495, _496, _497, 1.0f));
   float _512 = dot(float4(_495, _496, _497, 1.0f), float4(_508, _509, _510, _511));
   float _513 = max(_502, 0.0625f);
   float _514 = max(_507, 0.0625f);
   float _515 = max(_512, 0.0625f);
   float _516 = _513 * (c_ps_scratch[79].x);
   float _517 = _514 * (c_ps_scratch[79].x);
   float _518 = _515 * (c_ps_scratch[79].x);
   float _523 = -0.0f - (c_light_vec[0].x);
   float _524 = -0.0f - (c_light_vec[0].y);
   float _525 = -0.0f - (c_light_vec[0].z);
   float _526 = dot(float3(_105, _106, _107), float3(_523, _524, _525));
   float _527 = -0.0f - (c_light_vec[1].x);
   float _528 = -0.0f - (c_light_vec[1].y);
   float _529 = -0.0f - (c_light_vec[1].z);
   float _530 = dot(float3(_105, _106, _107), float3(_527, _528, _529));
   float _539 = _106 * 0.5f;
   float _540 = _539 + 0.5f;
   float _541 = (c_light_col[4].x) - (c_light_vec[4].x);
   float _542 = (c_light_col[4].y) - (c_light_vec[4].y);
   float _543 = (c_light_col[4].z) - (c_light_vec[4].z);
   float _544 = _541 * _540;
   float _545 = _542 * _540;
   float _546 = _543 * _540;
   float _547 = saturate(_526);
   float _548 = saturate(_530);
   float _553 = (c_light_col[0].x) * _547;
   float _554 = (c_light_col[0].y) * _547;
   float _555 = (c_light_col[0].z) * _547;
   float _560 = (c_light_col[1].x) * _548;
   float _561 = (c_light_col[1].y) * _548;
   float _562 = (c_light_col[1].z) * _548;
   float _563 = _428 + _298;
   float _564 = _429 + _299;
   float _565 = _430 + _300;
   int _566 = c_bool & 32768;
   bool _567 = (_566 == 0);
   float _576 = _516 + TEXCOORD.z;
   float _577 = _576 + (c_light_vec[4].x);
   float _578 = _577 + _544;
   float _579 = _517 + TEXCOORD.z;
   float _580 = _579 + (c_light_vec[4].y);
   float _581 = _580 + _545;
   float _582 = _518 + TEXCOORD.z;
   float _583 = _582 + (c_light_vec[4].z);
   float _584 = _583 + _546;
   if (!_567)
   {
      float _588 = (c_ps_user[11].x) * _164;
      float _589 = (c_ps_user[11].x) * _165;
      float _590 = (c_ps_user[11].x) * _166;
      float _592 = _105 + 1.0f;
      float _593 = _106 + 1.0f;
      float _594 = _107 + 1.0f;
      float _595 = _592 * 0.5f;
      float _596 = _593 * 0.5f;
      float _597 = _594 * 0.5f;
      float _598 = saturate(_595);
      float _599 = saturate(_596);
      float _600 = saturate(_597);
      float _601 = (c_ps_user[11].y) * _598;
      float _602 = _599 * (c_ps_user[11].y);
      float _603 = _600 * (c_ps_user[11].y);
      float _613 = (c_ps_user[11].z) * _61.x;
      float _614 = (c_ps_user[11].z) * _61.y;
      float _615 = (c_ps_user[11].z) * _61.z;
      float _617 = (c_ps_user[11].w) * _563;
      float _620 = (c_ps_user[12].x) * _196;
      float _626 = _139 * _139;
      float _627 = _139 * 0.9722222089767456f;
      float _628 = _626 * _626;
      float _629 = _628 * _627;
      float _630 = _629 + 0.02777777425944805f;
      float _631 = (c_ps_user[13].z) * _630;
      float _633 = (c_ps_user[13].w) * _578;
      float _634 = (c_ps_user[13].w) * _581;
      float _635 = (c_ps_user[13].w) * _584;
      float _638 = (c_ps_user[14].y) * _553;
      float _639 = (c_ps_user[14].y) * _554;
      float _640 = (c_ps_user[14].y) * _555;
      float _642 = (c_ps_user[14].z) * _560;
      float _643 = (c_ps_user[14].z) * _561;
      float _644 = (c_ps_user[14].z) * _562;
      float _646 = (c_ps_user[14].w) * _425;
      float _647 = (c_ps_user[14].w) * _426;
      float _648 = (c_ps_user[14].w) * _427;
      float _653 = (c_ps_user[15].z) * _428;
      float _654 = (c_ps_user[15].z) * _429;
      float _655 = (c_ps_user[15].z) * _430;
      float _656 = (c_ps_user[15].y) + (c_ps_user[15].x);
      float _657 = c_specular_scale.x * _656;
      float _658 = _657 * _563;
      float _659 = _638 + (c_ps_user[13].x);
      float _660 = _659 + _642;
      float _661 = c_diffuse_scale.x * _660;
      float _662 = _661 * c_mat_diffuse.x;
      float _663 = (c_ps_user[13].y) + _613;
      float _664 = _663 * c_specular_scale.x;
      float _665 = _664 + _658;
      float _666 = _665 * c_mat_specular.x;
      float _667 = _601 + _588;
      float _668 = _667 + _617;
      float _669 = _668 + (c_ps_user[12].y);
      float _670 = _669 + _620;
      float _671 = _670 + _633;
      float _672 = _671 + _631;
      float _673 = _672 + _646;
      float _674 = _673 + _653;
      float _675 = _674 + _662;
      float _676 = _675 + _666;
      float _677 = c_specular_scale.y * _656;
      float _678 = _677 * _564;
      float _679 = _639 + (c_ps_user[13].x);
      float _680 = _679 + _643;
      float _681 = c_diffuse_scale.y * _680;
      float _682 = _681 * c_mat_diffuse.y;
      float _683 = (c_ps_user[13].y) + _614;
      float _684 = _683 * c_specular_scale.y;
      float _685 = _684 + _678;
      float _686 = _685 * c_mat_specular.y;
      float _687 = _617 + _589;
      float _688 = _687 + _602;
      float _689 = _688 + (c_ps_user[12].y);
      float _690 = _689 + _620;
      float _691 = _690 + _634;
      float _692 = _691 + _631;
      float _693 = _692 + _647;
      float _694 = _693 + _654;
      float _695 = _694 + _682;
      float _696 = _695 + _686;
      float _697 = c_specular_scale.z * _656;
      float _698 = _697 * _565;
      float _699 = _640 + (c_ps_user[13].x);
      float _700 = _699 + _644;
      float _701 = c_diffuse_scale.z * _700;
      float _702 = _701 * c_mat_diffuse.z;
      float _703 = (c_ps_user[13].y) + _615;
      float _704 = _703 * c_specular_scale.z;
      float _705 = _704 + _698;
      float _706 = _705 * c_mat_specular.z;
      float _707 = _617 + _590;
      float _708 = _707 + _603;
      float _709 = _708 + (c_ps_user[12].y);
      float _710 = _709 + _620;
      float _711 = _710 + _635;
      float _712 = _711 + _631;
      float _713 = _712 + _648;
      float _714 = _713 + _655;
      float _715 = _714 + _702;
      float _716 = _715 + _706;
      float _717 = saturate(_676);
      float _718 = saturate(_696);
      float _719 = saturate(_716);
      _755 = _717;
      _756 = _718;
      _757 = _719;
   }
   else
   {
      float _721 = _555 * _196;
      float _722 = _584 + _427;
      float _723 = _722 + _562;
      float _724 = _723 + _721;
      float _725 = _554 * _196;
      float _726 = _581 + _426;
      float _727 = _726 + _561;
      float _728 = _727 + _725;
      float _729 = _553 * _196;
      float _730 = _578 + _425;
      float _731 = _730 + _560;
      float _732 = _731 + _729;
      float _733 = saturate(_732);
      float _734 = saturate(_728);
      float _735 = saturate(_724);
      float _736 = c_diffuse_scale.x * c_mat_diffuse.x;
      float _737 = _736 * _164;
      float _738 = _737 * _733;
      float _739 = c_diffuse_scale.y * c_mat_diffuse.y;
      float _740 = _739 * _165;
      float _741 = _740 * _734;
      float _742 = c_diffuse_scale.z * c_mat_diffuse.z;
      float _743 = _742 * _166;
      float _744 = _743 * _735;
      float _745 = saturate(_563);
      float _746 = saturate(_564);
      float _747 = saturate(_565);
      float _748 = _745 * _61.x;
      float _749 = _746 * _61.y;
      float _750 = _747 * _61.z;
      float _751 = _748 + _738;
      float _752 = _749 + _741;
      float _753 = _750 + _744;
      _755 = _751;
      _756 = _752;
      _757 = _753;
   }
   float _762 = c_mat_opacity * _53.w;
   float _763 = _762 * c_opacity_scale;
   float _764 = _763 * c_global_opacity;
   float _765 = _755 * COLOR.x;
   float _766 = _756 * COLOR.y;
   float _767 = _757 * COLOR.z;
   int _768 = c_bool & 16;
   bool _769 = (_768 == 0);
   if (!_769)
   {
      float _773 = c_tone_control.x * _765;
      float _774 = c_tone_control.x * _766;
      float _775 = c_tone_control.x * _767;
      float _777 = log2(_773);
      float _778 = log2(_774);
      float _779 = log2(_775);
      float _780 = _777 * c_tone_control.y;
      float _781 = _778 * c_tone_control.y;
      float _782 = _779 * c_tone_control.y;
      float _783 = exp2(_780);
      float _784 = exp2(_781);
      float _785 = exp2(_782);
      _833 = _783;
      _834 = _784;
      _835 = _785;
   }
   else
   {
      if (_567)
      {
         float _790 = c_tone_control.x * _765;
         float _791 = c_tone_control.x * _766;
         float _792 = c_tone_control.x * _767;
         float _793 = _790 * 2.0f;
         float _794 = _791 * 2.0f;
         float _795 = _792 * 2.0f;
         float _796 = _790 * 0.30000001192092896f;
         float _797 = _791 * 0.30000001192092896f;
         float _798 = _792 * 0.30000001192092896f;
         float _799 = _796 + 0.05000000074505806f;
         float _800 = _797 + 0.05000000074505806f;
         float _801 = _798 + 0.05000000074505806f;
         float _802 = _799 * _793;
         float _803 = _800 * _794;
         float _804 = _801 * _795;
         float _805 = _802 + 0.004000000189989805f;
         float _806 = _803 + 0.004000000189989805f;
         float _807 = _804 + 0.004000000189989805f;
         float _808 = _796 + 0.5f;
         float _809 = _797 + 0.5f;
         float _810 = _798 + 0.5f;
         float _811 = _808 * _793;
         float _812 = _809 * _794;
         float _813 = _810 * _795;
         float _814 = _811 + 0.06000000238418579f;
         float _815 = _812 + 0.06000000238418579f;
         float _816 = _813 + 0.06000000238418579f;
         float _817 = _805 / _814;
         float _818 = _806 / _815;
         float _819 = _807 / _816;
         float _820 = _817 + -0.06666666269302368f;
         float _821 = _818 + -0.06666666269302368f;
         float _822 = _819 + -0.06666666269302368f;
         float _823 = _820 * 1.3790643215179443f;
         float _824 = _821 * 1.3790643215179443f;
         float _825 = _822 * 1.3790643215179443f;
         float _826 = YKR_MaterialCurve(_823, _790);
         float _827 = YKR_MaterialCurve(_824, _791);
         float _828 = YKR_MaterialCurve(_825, _792);
         float _829 = sqrt(_826);
         float _830 = sqrt(_827);
         float _831 = sqrt(_828);
         _833 = _829;
         _834 = _830;
         _835 = _831;
      }
      else
      {
         _833 = _765;
         _834 = _766;
         _835 = _767;
      }
   }
   int _836 = c_bool & 4;
   bool _837 = (_836 == 0);
   if (_837)
   {
      float _839 = saturate(COLOR_1.x);
      float _844 = (c_fog_param[0].x) - _833;
      float _845 = (c_fog_param[0].y) - _834;
      float _846 = (c_fog_param[0].z) - _835;
      float _847 = _844 * _839;
      float _848 = _845 * _839;
      float _849 = _846 * _839;
      _851 = _847;
      _852 = _848;
      _853 = _849;
   }
   else
   {
      _851 = COLOR_1.x;
      _852 = COLOR_1.y;
      _853 = COLOR_1.z;
   }
   float _854 = _851 + _833;
   float _855 = _852 + _834;
   float _856 = _853 + _835;
   if (!_567)
   {
      float _864 = (c_ps_user[3].x) * (c_ps_user[12].z);
      float _865 = (c_ps_user[3].y) * (c_ps_user[12].z);
      float _866 = (c_ps_user[3].z) * (c_ps_user[12].z);
      float _868 = (c_ps_user[12].w) * COLOR.x;
      float _869 = (c_ps_user[12].w) * COLOR.y;
      float _870 = (c_ps_user[12].w) * COLOR.z;
      float _871 = _868 + _854;
      float _872 = _871 + _864;
      float _873 = _869 + _855;
      float _874 = _873 + _865;
      float _875 = _870 + _856;
      float _876 = _875 + _866;
      _878 = _872;
      _879 = _874;
      _880 = _876;
   }
   else
   {
      _878 = _854;
      _879 = _855;
      _880 = _856;
   }
   float _886 = (c_ps_col_scl[0].x) * _878;
   float _887 = (c_ps_col_scl[0].y) * _879;
   float _888 = (c_ps_col_scl[0].z) * _880;
   float _889 = (c_ps_col_scl[0].w) * _764;
   SV_Target.x = _886;
   SV_Target.y = _887;
   SV_Target.z = _888;
   SV_Target.w = _889;
   int _892 = c_bool & 8;
   bool _893 = (_892 != 0);
   float _894 = select(_893, _878, _886);
   float _895 = select(_893, _879, _887);
   float _896 = select(_893, _880, _888);
   float _897 = select(_893, _764, _889);
   SV_Target.x = _894;
   SV_Target.y = _895;
   SV_Target.z = _896;
   SV_Target.w = _897;
   return SV_Target;
}
