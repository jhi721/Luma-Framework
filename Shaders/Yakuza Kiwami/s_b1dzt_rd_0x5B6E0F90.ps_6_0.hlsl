// Yakuza Kiwami material s_b1dzt[rd] (0x5B6E0F90, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
// applied by a script: the inline Hable tonemap's saturate goes through
// YKR_MaterialCurve (tangent continuation in HDR), and the light pre-pass buffer t14 through YKR_LightBuffer.

#include "Includes/Common.hlsl"

Texture2D<float4> s0_t : register(t0);

Texture2D<float4> s2_t : register(t2);

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

SamplerState s2_s : register(s2);

SamplerState s5_s : register(s5);

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
   float4 _56 = s2_t.Sample(s2_s, float2(TEXCOORD.x, TEXCOORD.y));
   float4 _60 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _65 = _60.x * _60.x;
   float _66 = _60.y * _60.y;
   float _67 = _60.z * _60.z;
   float4 _68 = s5_t.Sample(s5_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _73 = _68.x * 2.0f;
   float _74 = _68.y * 2.0f;
   float _75 = _73 + -1.0f;
   float _76 = _74 + -1.0f;
   float _77 = dot(float2(_75, _76), float2(_75, _76));
   float _78 = 1.0f - _77;
   float _79 = saturate(_78);
   float _80 = sqrt(_79);
   float _81 = TEXCOORD_2.y * TEXCOORD_3.z;
   float _82 = TEXCOORD_2.z * TEXCOORD_3.y;
   float _83 = _81 - _82;
   float _84 = TEXCOORD_2.z * TEXCOORD_3.x;
   float _85 = TEXCOORD_2.x * TEXCOORD_3.z;
   float _86 = _84 - _85;
   float _87 = TEXCOORD_2.x * TEXCOORD_3.y;
   float _88 = TEXCOORD_2.y * TEXCOORD_3.x;
   float _89 = _87 - _88;
   float _90 = _75 * TEXCOORD_3.x;
   float _91 = _75 * TEXCOORD_3.y;
   float _92 = _75 * TEXCOORD_3.z;
   float _93 = _76 * TEXCOORD_3.w;
   float _94 = _93 * _83;
   float _95 = _93 * _86;
   float _96 = _93 * _89;
   float _97 = _94 + _90;
   float _98 = _95 + _91;
   float _99 = _96 + _92;
   float _100 = _80 * TEXCOORD_2.x;
   float _101 = _80 * TEXCOORD_2.y;
   float _102 = _80 * TEXCOORD_2.z;
   float _103 = _97 + _100;
   float _104 = _98 + _101;
   float _105 = _99 + _102;
   float _106 = dot(float3(_103, _104, _105), float3(_103, _104, _105));
   float _107 = rsqrt(_106);
   float _108 = _103 * _107;
   float _109 = _104 * _107;
   float _110 = _105 * _107;
   float _115 = dot(float3(TEXCOORD_1.x, TEXCOORD_1.y, TEXCOORD_1.z), float3(TEXCOORD_1.x, TEXCOORD_1.y, TEXCOORD_1.z));
   float _116 = rsqrt(_115);
   float _117 = _116 * TEXCOORD_1.x;
   float _118 = _116 * TEXCOORD_1.y;
   float _119 = _116 * TEXCOORD_1.z;
   float _120 = _117 + (c_light_vec[0].x);
   float _121 = _118 + (c_light_vec[0].y);
   float _122 = _119 + (c_light_vec[0].z);
   float _123 = dot(float3(_120, _121, _122), float3(_120, _121, _122));
   float _124 = rsqrt(_123);
   float _125 = _120 * _124;
   float _126 = _121 * _124;
   float _127 = _122 * _124;
   float4 _128 = s7_t.Sample(s7_s, float2(TEXCOORD.z, TEXCOORD.w));
   float _133 = _128.x * _128.x;
   float _134 = _128.y * _128.y;
   float _135 = _128.z * _128.z;
   float _136 = 1.0f - _133;
   float _137 = 1.0f - _134;
   float _138 = 1.0f - _135;
   float _139 = 1.0f - _56.z;
   float _140 = 1.0f - _128.w;
   float _141 = _140 * _139;
   float _142 = _141 * _136;
   float _143 = _141 * _137;
   float _144 = _141 * _138;
   float _145 = _142 + _133;
   float _146 = _143 + _134;
   float _147 = _144 + _135;
   float _148 = _65 * _145;
   float _149 = _66 * _146;
   float _150 = _67 * _147;
   float _154 = c_rt_size.z * SV_Position.x;
   float _155 = c_rt_size.w * SV_Position.y;
   int _158 = c_bool & 8;
   bool _159 = (_158 == 0);
   float _165;
   float _166;
   float _178;
   float _179;
   float _367;
   float _368;
   float _369;
   float _370;
   float _371;
   float _372;
   float _405;
   float _406;
   float _407;
   float _408;
   float _573;
   float _744;
   float _745;
   float _746;
   float _822;
   float _823;
   float _824;
   float _840;
   float _841;
   float _842;
   float _867;
   float _868;
   float _869;
   if (!_159)
   {
      float4 _161 = s13_t.Sample(s13_s, float2(_154, _155));
      _165 = _161.x;
      _166 = _161.y;
   }
   else
   {
      _165 = 1.0f;
      _166 = 1.0f;
   }
   if (!_159)
   {
      float _171 = (c_sdw_param_ch[0].x) * _165;
      float _172 = (c_sdw_param_ch[0].z) * _166;
      float _175 = _171 + (c_sdw_param_ch[0].y);
      float _176 = _172 + (c_sdw_param_ch[0].w);
      _178 = _175;
      _179 = _176;
   }
   else
   {
      _178 = _165;
      _179 = _166;
   }
   float _180 = min(_178, _179);
   float _189 = (c_light_vec[1].x) + _117;
   float _190 = (c_light_vec[1].y) + _118;
   float _191 = (c_light_vec[1].z) + _119;
   float _192 = dot(float3(_189, _190, _191), float3(_189, _190, _191));
   float _193 = rsqrt(_192);
   float _194 = _189 * _193;
   float _195 = _190 * _193;
   float _196 = _191 * _193;
   float _197 = -0.0f - (c_light_vec[1].x);
   float _198 = -0.0f - (c_light_vec[1].y);
   float _199 = -0.0f - (c_light_vec[1].z);
   float _200 = -0.0f - (c_light_vec[0].x);
   float _201 = -0.0f - (c_light_vec[0].y);
   float _202 = -0.0f - (c_light_vec[0].z);
   float _203 = dot(float3(_117, _118, _119), float3(_108, _109, _110));
   float _204 = _203 * 2.0f;
   float _205 = _204 * _108;
   float _206 = _204 * _109;
   float _207 = _204 * _110;
   float _208 = _117 - _205;
   float _209 = _118 - _206;
   float _210 = _119 - _207;
   float _211 = dot(float3(_208, _209, _210), float3(_200, _201, _202));
   float _212 = dot(float3(_208, _209, _210), float3(_197, _198, _199));
   float _213 = saturate(_211);
   float _214 = saturate(_212);
   float _215 = log2(_213);
   float _216 = log2(_214);
   float _217 = _215 * c_mat_power;
   float _218 = _216 * c_mat_power;
   float _219 = exp2(_217);
   float _220 = exp2(_218);
   float _225 = c_mat_fresnel * c_tone_control.w;
   float _226 = c_mat_power * 0.03974359855055809f;
   float _227 = _226 + 0.0856831967830658f;
   float _228 = _220 * _227;
   float _229 = dot(float3(_117, _118, _119), float3(_125, _126, _127));
   float _230 = dot(float3(_117, _118, _119), float3(_194, _195, _196));
   float _231 = 1.0f - _229;
   float _232 = 1.0f - _230;
   float _233 = _231 * _231;
   float _234 = _232 * _232;
   float _235 = 1.0f - _225;
   float _236 = _233 * _233;
   float _237 = _236 * _231;
   float _238 = _237 * _235;
   float _239 = _232 * _235;
   float _240 = _234 * _234;
   float _241 = _240 * _239;
   float _242 = _238 + _225;
   float _243 = _241 + _225;
   float _244 = dot(float3(_108, _109, _110), float3(_200, _201, _202));
   float _245 = saturate(_244);
   float _246 = dot(float3(_108, _109, _110), float3(_197, _198, _199));
   float _247 = saturate(_246);
   float _248 = _228 * _247;
   float _249 = _248 * _243;
   float _258 = c_specular_scale.x * c_mat_specular.x;
   float _259 = c_specular_scale.y * c_mat_specular.y;
   float _260 = c_specular_scale.z * c_mat_specular.z;
   float _265 = _180 * _227;
   float _266 = _265 * _219;
   float _267 = _266 * _245;
   float _268 = _267 * _242;
   float _269 = (c_light_col[0].x) * _268;
   float _270 = (c_light_col[0].y) * _268;
   float _271 = (c_light_col[0].z) * _268;
   float _276 = (c_light_col[1].x) * _249;
   float _277 = (c_light_col[1].y) * _249;
   float _278 = (c_light_col[1].z) * _249;
   float _279 = _276 + _269;
   float _280 = _277 + _270;
   float _281 = _278 + _271;
   float _282 = _258 * _279;
   float _283 = _259 * _280;
   float _284 = _260 * _281;
   int _288 = c_bool & 1;
   bool _289 = (_288 == 0);
   if (!_289)
   {
      float _291 = -0.0f - TEXCOORD_4.x;
      float _292 = -0.0f - TEXCOORD_4.y;
      float _293 = -0.0f - TEXCOORD_4.z;
      float _294 = dot(float3(_108, _109, _110), float3(_291, _292, _293));
      float _295 = saturate(_294);
      float _296 = _295 * TEXCOORD_1.w;
      float _297 = _295 * TEXCOORD_2.w;
      float _298 = _295 * TEXCOORD_4.w;
      float _299 = max(0.0f, _296);
      float _300 = max(0.0f, _297);
      float _301 = max(0.0f, _298);
      float _302 = -0.0f - TEXCOORD_5.x;
      float _303 = -0.0f - TEXCOORD_5.y;
      float _304 = -0.0f - TEXCOORD_5.z;
      float _305 = dot(float3(_108, _109, _110), float3(_302, _303, _304));
      float _306 = saturate(_305);
      float _307 = _306 * COLOR.w;
      float _308 = _306 * COLOR_1.w;
      float _309 = _306 * TEXCOORD_5.w;
      float _310 = max(0.0f, _307);
      float _311 = max(0.0f, _308);
      float _312 = max(0.0f, _309);
      float _313 = _310 + _299;
      float _314 = _311 + _300;
      float _315 = _312 + _301;
      float _316 = _117 + TEXCOORD_4.x;
      float _317 = _118 + TEXCOORD_4.y;
      float _318 = _119 + TEXCOORD_4.z;
      float _319 = dot(float3(_316, _317, _318), float3(_316, _317, _318));
      float _320 = rsqrt(_319);
      float _321 = _117 + TEXCOORD_5.x;
      float _322 = _118 + TEXCOORD_5.y;
      float _323 = _119 + TEXCOORD_5.z;
      float _324 = dot(float3(_321, _322, _323), float3(_321, _322, _323));
      float _325 = rsqrt(_324);
      float _326 = _321 * _325;
      float _327 = -0.0f - _326;
      float _328 = _322 * _325;
      float _329 = -0.0f - _328;
      float _330 = _323 * _325;
      float _331 = -0.0f - _330;
      float _332 = _316 * _320;
      float _333 = -0.0f - _332;
      float _334 = _317 * _320;
      float _335 = -0.0f - _334;
      float _336 = _318 * _320;
      float _337 = -0.0f - _336;
      float _338 = dot(float3(_333, _335, _337), float3(_108, _109, _110));
      float _339 = dot(float3(_327, _329, _331), float3(_108, _109, _110));
      float _340 = saturate(_338);
      float _341 = saturate(_339);
      float _342 = log2(_340);
      float _343 = log2(_341);
      float _344 = _342 * c_mat_power;
      float _345 = _343 * c_mat_power;
      float _346 = exp2(_344);
      float _347 = exp2(_345);
      float _348 = _346 * TEXCOORD_1.w;
      float _349 = _346 * TEXCOORD_2.w;
      float _350 = _346 * TEXCOORD_4.w;
      float _351 = _347 * COLOR.w;
      float _352 = _347 * COLOR_1.w;
      float _353 = _347 * TEXCOORD_5.w;
      float _354 = _351 + _348;
      float _355 = _352 + _349;
      float _356 = _353 + _350;
      float _357 = max(0.0f, _354);
      float _358 = max(0.0f, _355);
      float _359 = max(0.0f, _356);
      float _360 = _258 * _357;
      float _361 = _259 * _358;
      float _362 = _260 * _359;
      float _363 = min(_360, 1.0f);
      float _364 = min(_361, 1.0f);
      float _365 = min(_362, 1.0f);
      _367 = _363;
      _368 = _364;
      _369 = _365;
      _370 = _313;
      _371 = _314;
      _372 = _315;
   }
   else
   {
      _367 = 0.0f;
      _368 = 0.0f;
      _369 = 0.0f;
      _370 = 0.0f;
      _371 = 0.0f;
      _372 = 0.0f;
   }
   float _373 = c_rt_size.z * SV_Position.x;
   float _374 = c_rt_size.w * SV_Position.y;
   float4 _375 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_373, _374)));
   int _382 = c_bool & 16384;
   bool _383 = (_382 == 0);
   if (!_383)
   {
      float _385 = _375.x * 4.0f;
      float _386 = _375.y * 4.0f;
      float _387 = _375.z * 4.0f;
      float _388 = _375.w * 4.0f;
      _405 = _385;
      _406 = _386;
      _407 = _387;
      _408 = _388;
   }
   else
   {
      int _390 = c_bool & 8192;
      bool _391 = (_390 == 0);
      if (!_391)
      {
         float _393 = _375.x * 2.0f;
         float _394 = _375.y * 2.0f;
         float _395 = _375.z * 2.0f;
         float _396 = _375.w * 2.0f;
         _405 = _393;
         _406 = _394;
         _407 = _395;
         _408 = _396;
      }
      else
      {
         float _400 = _375.x / c_tone_control.z;
         float _401 = _375.y / c_tone_control.z;
         float _402 = _375.z / c_tone_control.z;
         float _403 = _375.w / c_tone_control.z;
         _405 = _400;
         _406 = _401;
         _407 = _402;
         _408 = _403;
      }
   }
   float _409 = _405 + _370;
   float _410 = _406 + _371;
   float _411 = _407 + _372;
   float _412 = _408 + _367;
   float _413 = _408 + _368;
   float _414 = _408 + _369;
   float _477 = dot(float3(_108, _109, _110), float3(_108, _109, _110));
   float _478 = rsqrt(_477);
   float _479 = _478 * _108;
   float _480 = _478 * _109;
   float _481 = _478 * _110;
   float _482 = dot(float4((c_ps_scratch[80].x), (c_ps_scratch[80].y), (c_ps_scratch[80].z), (c_ps_scratch[80].w)), float4(_479, _480, _481, 1.0f));
   float _483 = dot(float4((c_ps_scratch[81].x), (c_ps_scratch[81].y), (c_ps_scratch[81].z), (c_ps_scratch[81].w)), float4(_479, _480, _481, 1.0f));
   float _484 = dot(float4((c_ps_scratch[82].x), (c_ps_scratch[82].y), (c_ps_scratch[82].z), (c_ps_scratch[82].w)), float4(_479, _480, _481, 1.0f));
   float _485 = dot(float4((c_ps_scratch[83].x), (c_ps_scratch[83].y), (c_ps_scratch[83].z), (c_ps_scratch[83].w)), float4(_479, _480, _481, 1.0f));
   float _486 = dot(float4(_479, _480, _481, 1.0f), float4(_482, _483, _484, _485));
   float _487 = dot(float4((c_ps_scratch[84].x), (c_ps_scratch[84].y), (c_ps_scratch[84].z), (c_ps_scratch[84].w)), float4(_479, _480, _481, 1.0f));
   float _488 = dot(float4((c_ps_scratch[85].x), (c_ps_scratch[85].y), (c_ps_scratch[85].z), (c_ps_scratch[85].w)), float4(_479, _480, _481, 1.0f));
   float _489 = dot(float4((c_ps_scratch[86].x), (c_ps_scratch[86].y), (c_ps_scratch[86].z), (c_ps_scratch[86].w)), float4(_479, _480, _481, 1.0f));
   float _490 = dot(float4((c_ps_scratch[87].x), (c_ps_scratch[87].y), (c_ps_scratch[87].z), (c_ps_scratch[87].w)), float4(_479, _480, _481, 1.0f));
   float _491 = dot(float4(_479, _480, _481, 1.0f), float4(_487, _488, _489, _490));
   float _492 = dot(float4((c_ps_scratch[88].x), (c_ps_scratch[88].y), (c_ps_scratch[88].z), (c_ps_scratch[88].w)), float4(_479, _480, _481, 1.0f));
   float _493 = dot(float4((c_ps_scratch[89].x), (c_ps_scratch[89].y), (c_ps_scratch[89].z), (c_ps_scratch[89].w)), float4(_479, _480, _481, 1.0f));
   float _494 = dot(float4((c_ps_scratch[90].x), (c_ps_scratch[90].y), (c_ps_scratch[90].z), (c_ps_scratch[90].w)), float4(_479, _480, _481, 1.0f));
   float _495 = dot(float4((c_ps_scratch[91].x), (c_ps_scratch[91].y), (c_ps_scratch[91].z), (c_ps_scratch[91].w)), float4(_479, _480, _481, 1.0f));
   float _496 = dot(float4(_479, _480, _481, 1.0f), float4(_492, _493, _494, _495));
   float _497 = max(_486, 0.0625f);
   float _498 = max(_491, 0.0625f);
   float _499 = max(_496, 0.0625f);
   float _500 = _497 * (c_ps_scratch[79].x);
   float _501 = _498 * (c_ps_scratch[79].x);
   float _502 = _499 * (c_ps_scratch[79].x);
   float _507 = -0.0f - (c_light_vec[0].x);
   float _508 = -0.0f - (c_light_vec[0].y);
   float _509 = -0.0f - (c_light_vec[0].z);
   float _510 = dot(float3(_108, _109, _110), float3(_507, _508, _509));
   float _511 = -0.0f - (c_light_vec[1].x);
   float _512 = -0.0f - (c_light_vec[1].y);
   float _513 = -0.0f - (c_light_vec[1].z);
   float _514 = dot(float3(_108, _109, _110), float3(_511, _512, _513));
   float _523 = _109 * 0.5f;
   float _524 = _523 + 0.5f;
   float _525 = (c_light_col[4].x) - (c_light_vec[4].x);
   float _526 = (c_light_col[4].y) - (c_light_vec[4].y);
   float _527 = (c_light_col[4].z) - (c_light_vec[4].z);
   float _528 = _525 * _524;
   float _529 = _526 * _524;
   float _530 = _527 * _524;
   float _531 = saturate(_510);
   float _532 = saturate(_514);
   float _537 = (c_light_col[0].x) * _531;
   float _538 = (c_light_col[0].y) * _531;
   float _539 = (c_light_col[0].z) * _531;
   float _544 = (c_light_col[1].x) * _532;
   float _545 = (c_light_col[1].y) * _532;
   float _546 = (c_light_col[1].z) * _532;
   float _547 = _537 * _180;
   float _548 = _538 * _180;
   float _549 = _539 * _180;
   float _550 = (c_light_vec[4].x) + _500;
   float _551 = _550 + _528;
   float _552 = _551 + _409;
   float _553 = _552 + _544;
   float _554 = _553 + _547;
   float _555 = (c_light_vec[4].y) + _501;
   float _556 = _555 + _529;
   float _557 = _556 + _410;
   float _558 = _557 + _545;
   float _559 = _558 + _548;
   float _560 = (c_light_vec[4].z) + _502;
   float _561 = _560 + _530;
   float _562 = _561 + _411;
   float _563 = _562 + _546;
   float _564 = _563 + _549;
   int _565 = c_bool & 64;
   bool _566 = (_565 == 0);
   if (!_566)
   {
      float _568 = 1.0f - _56.y;
      float _569 = _180 * _568;
      float _570 = _569 * _531;
      float _571 = _570 + _56.y;
      _573 = _571;
   }
   else
   {
      _573 = _56.y;
   }
   float _574 = _412 + _282;
   float _575 = _413 + _283;
   float _576 = _414 + _284;
   int _577 = c_bool & 32768;
   bool _578 = (_577 == 0);
   if (!_578)
   {
      float _590 = (c_ps_user[11].x) * _148;
      float _591 = (c_ps_user[11].x) * _149;
      float _592 = (c_ps_user[11].x) * _150;
      float _594 = _108 + 1.0f;
      float _595 = _109 + 1.0f;
      float _596 = _110 + 1.0f;
      float _597 = _594 * 0.5f;
      float _598 = _595 * 0.5f;
      float _599 = _596 * 0.5f;
      float _600 = saturate(_597);
      float _601 = saturate(_598);
      float _602 = saturate(_599);
      float _603 = (c_ps_user[11].y) * _600;
      float _604 = _601 * (c_ps_user[11].y);
      float _605 = _602 * (c_ps_user[11].y);
      float _615 = (c_ps_user[11].z) * _56.x;
      float _617 = (c_ps_user[11].w) * _574;
      float _620 = (c_ps_user[12].x) * _180;
      float _622 = (c_ps_user[12].y) * _56.y;
      float _627 = -0.0f - _117;
      float _628 = -0.0f - _118;
      float _629 = -0.0f - _119;
      float _630 = dot(float3(_627, _628, _629), float3(_108, _109, _110));
      float _631 = 1.0f - _630;
      float _632 = _631 * _631;
      float _633 = _631 * 0.9722222089767456f;
      float _634 = _632 * _632;
      float _635 = _634 * _633;
      float _636 = _635 + 0.02777777425944805f;
      float _637 = _636 * (c_ps_user[13].z);
      float _639 = (c_ps_user[13].w) * _551;
      float _640 = (c_ps_user[13].w) * _556;
      float _641 = (c_ps_user[13].w) * _561;
      float _644 = (c_ps_user[14].y) * _537;
      float _645 = (c_ps_user[14].y) * _538;
      float _646 = (c_ps_user[14].y) * _539;
      float _648 = (c_ps_user[14].z) * _544;
      float _649 = (c_ps_user[14].z) * _545;
      float _650 = (c_ps_user[14].z) * _546;
      float _652 = (c_ps_user[14].w) * _409;
      float _653 = (c_ps_user[14].w) * _410;
      float _654 = (c_ps_user[14].w) * _411;
      float _659 = (c_ps_user[15].z) * _412;
      float _660 = (c_ps_user[15].z) * _413;
      float _661 = (c_ps_user[15].z) * _414;
      float _662 = (c_ps_user[15].y) + (c_ps_user[15].x);
      float _663 = c_specular_scale.x * _662;
      float _664 = _663 * _574;
      float _665 = _644 + (c_ps_user[13].x);
      float _666 = _665 + _648;
      float _667 = c_diffuse_scale.x * _666;
      float _668 = _667 * c_mat_diffuse.x;
      float _669 = (c_ps_user[13].y) + _615;
      float _670 = _669 * c_specular_scale.x;
      float _671 = _670 + _664;
      float _672 = _671 * c_mat_specular.x;
      float _673 = _603 + _590;
      float _674 = _673 + _617;
      float _675 = _674 + _620;
      float _676 = _675 + _622;
      float _677 = _676 + _639;
      float _678 = _677 + _652;
      float _679 = _678 + _659;
      float _680 = _679 + _637;
      float _681 = _680 + _668;
      float _682 = _681 + _672;
      float _683 = c_specular_scale.y * _662;
      float _684 = _683 * _575;
      float _685 = _645 + (c_ps_user[13].x);
      float _686 = _685 + _649;
      float _687 = c_diffuse_scale.y * _686;
      float _688 = _687 * c_mat_diffuse.y;
      float _689 = _669 * c_specular_scale.y;
      float _690 = _689 + _684;
      float _691 = _690 * c_mat_specular.y;
      float _692 = _617 + _591;
      float _693 = _692 + _604;
      float _694 = _693 + _620;
      float _695 = _694 + _622;
      float _696 = _695 + _640;
      float _697 = _696 + _653;
      float _698 = _697 + _660;
      float _699 = _698 + _637;
      float _700 = _699 + _688;
      float _701 = _700 + _691;
      float _702 = c_specular_scale.z * _662;
      float _703 = _702 * _576;
      float _704 = _646 + (c_ps_user[13].x);
      float _705 = _704 + _650;
      float _706 = c_diffuse_scale.z * _705;
      float _707 = _706 * c_mat_diffuse.z;
      float _708 = _669 * c_specular_scale.z;
      float _709 = _708 + _703;
      float _710 = _709 * c_mat_specular.z;
      float _711 = _617 + _592;
      float _712 = _711 + _605;
      float _713 = _712 + _620;
      float _714 = _713 + _622;
      float _715 = _714 + _641;
      float _716 = _715 + _654;
      float _717 = _716 + _661;
      float _718 = _717 + _637;
      float _719 = _718 + _707;
      float _720 = _719 + _710;
      float _721 = saturate(_682);
      float _722 = saturate(_701);
      float _723 = saturate(_720);
      _744 = _721;
      _745 = _722;
      _746 = _723;
   }
   else
   {
      float _725 = c_diffuse_scale.x * c_mat_diffuse.x;
      float _726 = _725 * _148;
      float _727 = _726 * _554;
      float _728 = _727 * _573;
      float _729 = c_diffuse_scale.y * c_mat_diffuse.y;
      float _730 = _729 * _149;
      float _731 = _730 * _559;
      float _732 = _731 * _573;
      float _733 = c_diffuse_scale.z * c_mat_diffuse.z;
      float _734 = _733 * _150;
      float _735 = _734 * _564;
      float _736 = _735 * _573;
      float _737 = _574 * _56.x;
      float _738 = _575 * _56.x;
      float _739 = _576 * _56.x;
      float _740 = _728 + _737;
      float _741 = _732 + _738;
      float _742 = _736 + _739;
      _744 = _740;
      _745 = _741;
      _746 = _742;
   }
   float _751 = c_mat_opacity * _60.w;
   float _752 = _751 * c_opacity_scale;
   float _753 = _752 * c_global_opacity;
   float _754 = _744 * COLOR.x;
   float _755 = _745 * COLOR.y;
   float _756 = _746 * COLOR.z;
   int _757 = c_bool & 16;
   bool _758 = (_757 == 0);
   if (!_758)
   {
      float _762 = c_tone_control.x * _754;
      float _763 = c_tone_control.x * _755;
      float _764 = c_tone_control.x * _756;
      float _766 = log2(_762);
      float _767 = log2(_763);
      float _768 = log2(_764);
      float _769 = _766 * c_tone_control.y;
      float _770 = _767 * c_tone_control.y;
      float _771 = _768 * c_tone_control.y;
      float _772 = exp2(_769);
      float _773 = exp2(_770);
      float _774 = exp2(_771);
      _822 = _772;
      _823 = _773;
      _824 = _774;
   }
   else
   {
      if (_578)
      {
         float _779 = c_tone_control.x * _754;
         float _780 = c_tone_control.x * _755;
         float _781 = c_tone_control.x * _756;
         float _782 = _779 * 2.0f;
         float _783 = _780 * 2.0f;
         float _784 = _781 * 2.0f;
         float _785 = _779 * 0.30000001192092896f;
         float _786 = _780 * 0.30000001192092896f;
         float _787 = _781 * 0.30000001192092896f;
         float _788 = _785 + 0.05000000074505806f;
         float _789 = _786 + 0.05000000074505806f;
         float _790 = _787 + 0.05000000074505806f;
         float _791 = _788 * _782;
         float _792 = _789 * _783;
         float _793 = _790 * _784;
         float _794 = _791 + 0.004000000189989805f;
         float _795 = _792 + 0.004000000189989805f;
         float _796 = _793 + 0.004000000189989805f;
         float _797 = _785 + 0.5f;
         float _798 = _786 + 0.5f;
         float _799 = _787 + 0.5f;
         float _800 = _797 * _782;
         float _801 = _798 * _783;
         float _802 = _799 * _784;
         float _803 = _800 + 0.06000000238418579f;
         float _804 = _801 + 0.06000000238418579f;
         float _805 = _802 + 0.06000000238418579f;
         float _806 = _794 / _803;
         float _807 = _795 / _804;
         float _808 = _796 / _805;
         float _809 = _806 + -0.06666666269302368f;
         float _810 = _807 + -0.06666666269302368f;
         float _811 = _808 + -0.06666666269302368f;
         float _812 = _809 * 1.3790643215179443f;
         float _813 = _810 * 1.3790643215179443f;
         float _814 = _811 * 1.3790643215179443f;
         float _815 = YKR_MaterialCurve(_812, _779);
         float _816 = YKR_MaterialCurve(_813, _780);
         float _817 = YKR_MaterialCurve(_814, _781);
         float _818 = sqrt(_815);
         float _819 = sqrt(_816);
         float _820 = sqrt(_817);
         _822 = _818;
         _823 = _819;
         _824 = _820;
      }
      else
      {
         _822 = _754;
         _823 = _755;
         _824 = _756;
      }
   }
   int _825 = c_bool & 4;
   bool _826 = (_825 == 0);
   if (_826)
   {
      float _828 = saturate(COLOR_1.x);
      float _833 = (c_fog_param[0].x) - _822;
      float _834 = (c_fog_param[0].y) - _823;
      float _835 = (c_fog_param[0].z) - _824;
      float _836 = _833 * _828;
      float _837 = _834 * _828;
      float _838 = _835 * _828;
      _840 = _836;
      _841 = _837;
      _842 = _838;
   }
   else
   {
      _840 = COLOR_1.x;
      _841 = COLOR_1.y;
      _842 = COLOR_1.z;
   }
   float _843 = _840 + _822;
   float _844 = _841 + _823;
   float _845 = _842 + _824;
   if (!_578)
   {
      float _853 = (c_ps_user[3].x) * (c_ps_user[12].z);
      float _854 = (c_ps_user[3].y) * (c_ps_user[12].z);
      float _855 = (c_ps_user[3].z) * (c_ps_user[12].z);
      float _857 = (c_ps_user[12].w) * COLOR.x;
      float _858 = (c_ps_user[12].w) * COLOR.y;
      float _859 = (c_ps_user[12].w) * COLOR.z;
      float _860 = _857 + _843;
      float _861 = _860 + _853;
      float _862 = _858 + _844;
      float _863 = _862 + _854;
      float _864 = _859 + _845;
      float _865 = _864 + _855;
      _867 = _861;
      _868 = _863;
      _869 = _865;
   }
   else
   {
      _867 = _843;
      _868 = _844;
      _869 = _845;
   }
   float _872 = _753 - c_mat_punchthrough_ref;
   bool _873 = (_872 < 0.0f);
   if (_873)
      discard;
   float _879 = (c_ps_col_scl[0].x) * _867;
   float _880 = (c_ps_col_scl[0].y) * _868;
   float _881 = (c_ps_col_scl[0].z) * _869;
   float _882 = (c_ps_col_scl[0].w) * _753;
   SV_Target.x = _879;
   SV_Target.y = _880;
   SV_Target.z = _881;
   SV_Target.w = _882;
   int _885 = c_bool & 8;
   bool _886 = (_885 != 0);
   float _887 = select(_886, _867, _879);
   float _888 = select(_886, _868, _880);
   float _889 = select(_886, _869, _881);
   float _890 = select(_886, _753, _882);
   SV_Target.x = _887;
   SV_Target.y = _888;
   SV_Target.z = _889;
   SV_Target.w = _890;
   return SV_Target;
}
