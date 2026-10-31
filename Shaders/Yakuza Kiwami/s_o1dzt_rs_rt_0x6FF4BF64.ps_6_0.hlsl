// Yakuza Kiwami material s_o1dzt[rs][rt] (0x6FF4BF64, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
// applied by a script: the inline Hable tonemap's saturate goes through
// YKR_MaterialCurve (tangent continuation in HDR), and the light pre-pass buffer t14 through YKR_LightBuffer.

#include "Includes/Common.hlsl"

Texture2D<float4> s0_t : register(t0);

Texture2D<float4> s2_t : register(t2);

Texture2D<float4> s4_t : register(t4);

Texture2D<float4> s5_t : register(t5);

Texture2D<float4> s6_t : register(t6);

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

SamplerState s4_s : register(s4);

SamplerState s5_s : register(s5);

SamplerState s6_s : register(s6);

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
   float4 _100 = s4_t.Sample(s4_s, float2(TEXCOORD.z, TEXCOORD.w));
   float _102 = 1.0f - _100.x;
   float _103 = _102 * _58.z;
   float _104 = 1.0f - _103;
   float _107 = _98 * 2.0f;
   float _108 = _99 * 2.0f;
   float _109 = _107 + -1.0f;
   float _110 = _108 + -1.0f;
   float _111 = dot(float2(_109, _110), float2(_109, _110));
   float _112 = 1.0f - _111;
   float _113 = saturate(_112);
   float _114 = sqrt(_113);
   float _115 = TEXCOORD_2.y * TEXCOORD_3.z;
   float _116 = TEXCOORD_2.z * TEXCOORD_3.y;
   float _117 = _115 - _116;
   float _118 = TEXCOORD_2.z * TEXCOORD_3.x;
   float _119 = TEXCOORD_2.x * TEXCOORD_3.z;
   float _120 = _118 - _119;
   float _121 = TEXCOORD_2.x * TEXCOORD_3.y;
   float _122 = TEXCOORD_2.y * TEXCOORD_3.x;
   float _123 = _121 - _122;
   float _124 = _109 * TEXCOORD_3.x;
   float _125 = _109 * TEXCOORD_3.y;
   float _126 = _109 * TEXCOORD_3.z;
   float _127 = _110 * TEXCOORD_3.w;
   float _128 = _127 * _117;
   float _129 = _127 * _120;
   float _130 = _127 * _123;
   float _131 = _128 + _124;
   float _132 = _129 + _125;
   float _133 = _130 + _126;
   float _134 = _114 * TEXCOORD_2.x;
   float _135 = _114 * TEXCOORD_2.y;
   float _136 = _114 * TEXCOORD_2.z;
   float _137 = _131 + _134;
   float _138 = _132 + _135;
   float _139 = _133 + _136;
   float _140 = dot(float3(_137, _138, _139), float3(_137, _138, _139));
   float _141 = rsqrt(_140);
   float _142 = _137 * _141;
   float _143 = _138 * _141;
   float _144 = _139 * _141;
   float _149 = dot(float3(TEXCOORD_1.x, TEXCOORD_1.y, TEXCOORD_1.z), float3(TEXCOORD_1.x, TEXCOORD_1.y, TEXCOORD_1.z));
   float _150 = rsqrt(_149);
   float _151 = _150 * TEXCOORD_1.x;
   float _152 = _150 * TEXCOORD_1.y;
   float _153 = _150 * TEXCOORD_1.z;
   float _154 = _151 + (c_light_vec[0].x);
   float _155 = _152 + (c_light_vec[0].y);
   float _156 = _153 + (c_light_vec[0].z);
   float _157 = dot(float3(_154, _155, _156), float3(_154, _155, _156));
   float _158 = rsqrt(_157);
   float _159 = _154 * _158;
   float _160 = _155 * _158;
   float _161 = _156 * _158;
   float _162 = _143 + 0.10000000149011612f;
   float _163 = saturate(_162);
   float _166 = (c_attribute_ps[2].w) + 2.0f;
   float _168 = (c_attribute_ps[2].z) + 2.5f;
   float _169 = -0.0f - _151;
   float _170 = -0.0f - _152;
   float _171 = -0.0f - _153;
   float _172 = dot(float3(_169, _170, _171), float3(TEXCOORD_2.x, TEXCOORD_2.y, TEXCOORD_2.z));
   float _173 = 1.0f - _172;
   float _174 = saturate(_173);
   float _175 = dot(float3(_169, _170, _171), float3(_142, _143, _144));
   float _176 = 1.0f - _175;
   float _177 = saturate(_176);
   float _178 = _177 * _174;
   float _179 = log2(_178);
   float _180 = _179 * _166;
   float _181 = exp2(_180);
   float _182 = _181 * _168;
   int _185 = c_bool & 4096;
   bool _186 = (_185 == 0);
   float _195;
   float _219;
   float _220;
   float _232;
   float _233;
   float _421;
   float _422;
   float _423;
   float _424;
   float _425;
   float _426;
   float _459;
   float _460;
   float _461;
   float _462;
   float _627;
   float _794;
   float _795;
   float _796;
   float _872;
   float _873;
   float _874;
   float _890;
   float _891;
   float _892;
   float _917;
   float _918;
   float _919;
   if (!_186)
   {
      float _191 = (c_ps_user[9].w) * _179;
      float _192 = exp2(_191);
      float _193 = _192 * (c_ps_user[9].z);
      _195 = _193;
   }
   else
   {
      _195 = _182;
   }
   float _196 = _163 * _195;
   float _197 = _58.y * _196;
   float _200 = _197 * (c_ps_user[2].x);
   float _201 = _200 + 1.0f;
   float _202 = _67 * _201;
   float _203 = _68 * _201;
   float _204 = _69 * _201;
   float _208 = c_rt_size.z * SV_Position.x;
   float _209 = c_rt_size.w * SV_Position.y;
   int _212 = c_bool & 8;
   bool _213 = (_212 == 0);
   if (!_213)
   {
      float4 _215 = s13_t.Sample(s13_s, float2(_208, _209));
      _219 = _215.x;
      _220 = _215.y;
   }
   else
   {
      _219 = 1.0f;
      _220 = 1.0f;
   }
   if (!_213)
   {
      float _225 = (c_sdw_param_ch[0].x) * _219;
      float _226 = (c_sdw_param_ch[0].z) * _220;
      float _229 = _225 + (c_sdw_param_ch[0].y);
      float _230 = _226 + (c_sdw_param_ch[0].w);
      _232 = _229;
      _233 = _230;
   }
   else
   {
      _232 = _219;
      _233 = _220;
   }
   float _234 = min(_232, _233);
   float _243 = (c_light_vec[1].x) + _151;
   float _244 = (c_light_vec[1].y) + _152;
   float _245 = (c_light_vec[1].z) + _153;
   float _246 = dot(float3(_243, _244, _245), float3(_243, _244, _245));
   float _247 = rsqrt(_246);
   float _248 = _243 * _247;
   float _249 = _244 * _247;
   float _250 = _245 * _247;
   float _251 = -0.0f - (c_light_vec[1].x);
   float _252 = -0.0f - (c_light_vec[1].y);
   float _253 = -0.0f - (c_light_vec[1].z);
   float _254 = -0.0f - (c_light_vec[0].x);
   float _255 = -0.0f - (c_light_vec[0].y);
   float _256 = -0.0f - (c_light_vec[0].z);
   float _257 = dot(float3(_151, _152, _153), float3(_142, _143, _144));
   float _258 = _257 * 2.0f;
   float _259 = _258 * _142;
   float _260 = _258 * _143;
   float _261 = _258 * _144;
   float _262 = _151 - _259;
   float _263 = _152 - _260;
   float _264 = _153 - _261;
   float _265 = dot(float3(_262, _263, _264), float3(_254, _255, _256));
   float _266 = dot(float3(_262, _263, _264), float3(_251, _252, _253));
   float _267 = saturate(_265);
   float _268 = saturate(_266);
   float _269 = log2(_267);
   float _270 = log2(_268);
   float _271 = _269 * c_mat_power;
   float _272 = _270 * c_mat_power;
   float _273 = exp2(_271);
   float _274 = exp2(_272);
   float _279 = c_mat_fresnel * c_tone_control.w;
   float _280 = c_mat_power * 0.03974359855055809f;
   float _281 = _280 + 0.0856831967830658f;
   float _282 = _274 * _281;
   float _283 = dot(float3(_151, _152, _153), float3(_159, _160, _161));
   float _284 = dot(float3(_151, _152, _153), float3(_248, _249, _250));
   float _285 = 1.0f - _283;
   float _286 = 1.0f - _284;
   float _287 = _285 * _285;
   float _288 = _286 * _286;
   float _289 = 1.0f - _279;
   float _290 = _287 * _287;
   float _291 = _290 * _285;
   float _292 = _291 * _289;
   float _293 = _286 * _289;
   float _294 = _288 * _288;
   float _295 = _294 * _293;
   float _296 = _292 + _279;
   float _297 = _295 + _279;
   float _298 = dot(float3(_142, _143, _144), float3(_254, _255, _256));
   float _299 = saturate(_298);
   float _300 = dot(float3(_142, _143, _144), float3(_251, _252, _253));
   float _301 = saturate(_300);
   float _302 = _282 * _301;
   float _303 = _302 * _297;
   float _312 = c_specular_scale.x * c_mat_specular.x;
   float _313 = c_specular_scale.y * c_mat_specular.y;
   float _314 = c_specular_scale.z * c_mat_specular.z;
   float _319 = _234 * _281;
   float _320 = _319 * _273;
   float _321 = _320 * _299;
   float _322 = _321 * _296;
   float _323 = (c_light_col[0].x) * _322;
   float _324 = (c_light_col[0].y) * _322;
   float _325 = (c_light_col[0].z) * _322;
   float _330 = (c_light_col[1].x) * _303;
   float _331 = (c_light_col[1].y) * _303;
   float _332 = (c_light_col[1].z) * _303;
   float _333 = _330 + _323;
   float _334 = _331 + _324;
   float _335 = _332 + _325;
   float _336 = _312 * _333;
   float _337 = _313 * _334;
   float _338 = _314 * _335;
   int _342 = c_bool & 1;
   bool _343 = (_342 == 0);
   if (!_343)
   {
      float _345 = -0.0f - TEXCOORD_4.x;
      float _346 = -0.0f - TEXCOORD_4.y;
      float _347 = -0.0f - TEXCOORD_4.z;
      float _348 = dot(float3(_142, _143, _144), float3(_345, _346, _347));
      float _349 = saturate(_348);
      float _350 = _349 * TEXCOORD_1.w;
      float _351 = _349 * TEXCOORD_2.w;
      float _352 = _349 * TEXCOORD_4.w;
      float _353 = max(0.0f, _350);
      float _354 = max(0.0f, _351);
      float _355 = max(0.0f, _352);
      float _356 = -0.0f - TEXCOORD_5.x;
      float _357 = -0.0f - TEXCOORD_5.y;
      float _358 = -0.0f - TEXCOORD_5.z;
      float _359 = dot(float3(_142, _143, _144), float3(_356, _357, _358));
      float _360 = saturate(_359);
      float _361 = _360 * COLOR.w;
      float _362 = _360 * COLOR_1.w;
      float _363 = _360 * TEXCOORD_5.w;
      float _364 = max(0.0f, _361);
      float _365 = max(0.0f, _362);
      float _366 = max(0.0f, _363);
      float _367 = _364 + _353;
      float _368 = _365 + _354;
      float _369 = _366 + _355;
      float _370 = _151 + TEXCOORD_4.x;
      float _371 = _152 + TEXCOORD_4.y;
      float _372 = _153 + TEXCOORD_4.z;
      float _373 = dot(float3(_370, _371, _372), float3(_370, _371, _372));
      float _374 = rsqrt(_373);
      float _375 = _151 + TEXCOORD_5.x;
      float _376 = _152 + TEXCOORD_5.y;
      float _377 = _153 + TEXCOORD_5.z;
      float _378 = dot(float3(_375, _376, _377), float3(_375, _376, _377));
      float _379 = rsqrt(_378);
      float _380 = _375 * _379;
      float _381 = -0.0f - _380;
      float _382 = _376 * _379;
      float _383 = -0.0f - _382;
      float _384 = _377 * _379;
      float _385 = -0.0f - _384;
      float _386 = _370 * _374;
      float _387 = -0.0f - _386;
      float _388 = _371 * _374;
      float _389 = -0.0f - _388;
      float _390 = _372 * _374;
      float _391 = -0.0f - _390;
      float _392 = dot(float3(_387, _389, _391), float3(_142, _143, _144));
      float _393 = dot(float3(_381, _383, _385), float3(_142, _143, _144));
      float _394 = saturate(_392);
      float _395 = saturate(_393);
      float _396 = log2(_394);
      float _397 = log2(_395);
      float _398 = _396 * c_mat_power;
      float _399 = _397 * c_mat_power;
      float _400 = exp2(_398);
      float _401 = exp2(_399);
      float _402 = _400 * TEXCOORD_1.w;
      float _403 = _400 * TEXCOORD_2.w;
      float _404 = _400 * TEXCOORD_4.w;
      float _405 = _401 * COLOR.w;
      float _406 = _401 * COLOR_1.w;
      float _407 = _401 * TEXCOORD_5.w;
      float _408 = _405 + _402;
      float _409 = _406 + _403;
      float _410 = _407 + _404;
      float _411 = max(0.0f, _408);
      float _412 = max(0.0f, _409);
      float _413 = max(0.0f, _410);
      float _414 = _312 * _411;
      float _415 = _313 * _412;
      float _416 = _314 * _413;
      float _417 = min(_414, 1.0f);
      float _418 = min(_415, 1.0f);
      float _419 = min(_416, 1.0f);
      _421 = _417;
      _422 = _418;
      _423 = _419;
      _424 = _367;
      _425 = _368;
      _426 = _369;
   }
   else
   {
      _421 = 0.0f;
      _422 = 0.0f;
      _423 = 0.0f;
      _424 = 0.0f;
      _425 = 0.0f;
      _426 = 0.0f;
   }
   float _427 = c_rt_size.z * SV_Position.x;
   float _428 = c_rt_size.w * SV_Position.y;
   float4 _429 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_427, _428)));
   int _436 = c_bool & 16384;
   bool _437 = (_436 == 0);
   if (!_437)
   {
      float _439 = _429.x * 4.0f;
      float _440 = _429.y * 4.0f;
      float _441 = _429.z * 4.0f;
      float _442 = _429.w * 4.0f;
      _459 = _439;
      _460 = _440;
      _461 = _441;
      _462 = _442;
   }
   else
   {
      int _444 = c_bool & 8192;
      bool _445 = (_444 == 0);
      if (!_445)
      {
         float _447 = _429.x * 2.0f;
         float _448 = _429.y * 2.0f;
         float _449 = _429.z * 2.0f;
         float _450 = _429.w * 2.0f;
         _459 = _447;
         _460 = _448;
         _461 = _449;
         _462 = _450;
      }
      else
      {
         float _454 = _429.x / c_tone_control.z;
         float _455 = _429.y / c_tone_control.z;
         float _456 = _429.z / c_tone_control.z;
         float _457 = _429.w / c_tone_control.z;
         _459 = _454;
         _460 = _455;
         _461 = _456;
         _462 = _457;
      }
   }
   float _463 = _459 + _424;
   float _464 = _460 + _425;
   float _465 = _461 + _426;
   float _466 = _462 + _421;
   float _467 = _462 + _422;
   float _468 = _462 + _423;
   float _531 = dot(float3(_142, _143, _144), float3(_142, _143, _144));
   float _532 = rsqrt(_531);
   float _533 = _532 * _142;
   float _534 = _532 * _143;
   float _535 = _532 * _144;
   float _536 = dot(float4((c_ps_scratch[80].x), (c_ps_scratch[80].y), (c_ps_scratch[80].z), (c_ps_scratch[80].w)), float4(_533, _534, _535, 1.0f));
   float _537 = dot(float4((c_ps_scratch[81].x), (c_ps_scratch[81].y), (c_ps_scratch[81].z), (c_ps_scratch[81].w)), float4(_533, _534, _535, 1.0f));
   float _538 = dot(float4((c_ps_scratch[82].x), (c_ps_scratch[82].y), (c_ps_scratch[82].z), (c_ps_scratch[82].w)), float4(_533, _534, _535, 1.0f));
   float _539 = dot(float4((c_ps_scratch[83].x), (c_ps_scratch[83].y), (c_ps_scratch[83].z), (c_ps_scratch[83].w)), float4(_533, _534, _535, 1.0f));
   float _540 = dot(float4(_533, _534, _535, 1.0f), float4(_536, _537, _538, _539));
   float _541 = dot(float4((c_ps_scratch[84].x), (c_ps_scratch[84].y), (c_ps_scratch[84].z), (c_ps_scratch[84].w)), float4(_533, _534, _535, 1.0f));
   float _542 = dot(float4((c_ps_scratch[85].x), (c_ps_scratch[85].y), (c_ps_scratch[85].z), (c_ps_scratch[85].w)), float4(_533, _534, _535, 1.0f));
   float _543 = dot(float4((c_ps_scratch[86].x), (c_ps_scratch[86].y), (c_ps_scratch[86].z), (c_ps_scratch[86].w)), float4(_533, _534, _535, 1.0f));
   float _544 = dot(float4((c_ps_scratch[87].x), (c_ps_scratch[87].y), (c_ps_scratch[87].z), (c_ps_scratch[87].w)), float4(_533, _534, _535, 1.0f));
   float _545 = dot(float4(_533, _534, _535, 1.0f), float4(_541, _542, _543, _544));
   float _546 = dot(float4((c_ps_scratch[88].x), (c_ps_scratch[88].y), (c_ps_scratch[88].z), (c_ps_scratch[88].w)), float4(_533, _534, _535, 1.0f));
   float _547 = dot(float4((c_ps_scratch[89].x), (c_ps_scratch[89].y), (c_ps_scratch[89].z), (c_ps_scratch[89].w)), float4(_533, _534, _535, 1.0f));
   float _548 = dot(float4((c_ps_scratch[90].x), (c_ps_scratch[90].y), (c_ps_scratch[90].z), (c_ps_scratch[90].w)), float4(_533, _534, _535, 1.0f));
   float _549 = dot(float4((c_ps_scratch[91].x), (c_ps_scratch[91].y), (c_ps_scratch[91].z), (c_ps_scratch[91].w)), float4(_533, _534, _535, 1.0f));
   float _550 = dot(float4(_533, _534, _535, 1.0f), float4(_546, _547, _548, _549));
   float _551 = max(_540, 0.0625f);
   float _552 = max(_545, 0.0625f);
   float _553 = max(_550, 0.0625f);
   float _554 = _551 * (c_ps_scratch[79].x);
   float _555 = _552 * (c_ps_scratch[79].x);
   float _556 = _553 * (c_ps_scratch[79].x);
   float _561 = -0.0f - (c_light_vec[0].x);
   float _562 = -0.0f - (c_light_vec[0].y);
   float _563 = -0.0f - (c_light_vec[0].z);
   float _564 = dot(float3(_142, _143, _144), float3(_561, _562, _563));
   float _565 = -0.0f - (c_light_vec[1].x);
   float _566 = -0.0f - (c_light_vec[1].y);
   float _567 = -0.0f - (c_light_vec[1].z);
   float _568 = dot(float3(_142, _143, _144), float3(_565, _566, _567));
   float _577 = _143 * 0.5f;
   float _578 = _577 + 0.5f;
   float _579 = (c_light_col[4].x) - (c_light_vec[4].x);
   float _580 = (c_light_col[4].y) - (c_light_vec[4].y);
   float _581 = (c_light_col[4].z) - (c_light_vec[4].z);
   float _582 = _579 * _578;
   float _583 = _580 * _578;
   float _584 = _581 * _578;
   float _585 = saturate(_564);
   float _586 = saturate(_568);
   float _591 = (c_light_col[0].x) * _585;
   float _592 = (c_light_col[0].y) * _585;
   float _593 = (c_light_col[0].z) * _585;
   float _598 = (c_light_col[1].x) * _586;
   float _599 = (c_light_col[1].y) * _586;
   float _600 = (c_light_col[1].z) * _586;
   float _601 = _591 * _234;
   float _602 = _592 * _234;
   float _603 = _593 * _234;
   float _604 = (c_light_vec[4].x) + _554;
   float _605 = _604 + _582;
   float _606 = _605 + _463;
   float _607 = _606 + _598;
   float _608 = _607 + _601;
   float _609 = (c_light_vec[4].y) + _555;
   float _610 = _609 + _583;
   float _611 = _610 + _464;
   float _612 = _611 + _599;
   float _613 = _612 + _602;
   float _614 = (c_light_vec[4].z) + _556;
   float _615 = _614 + _584;
   float _616 = _615 + _465;
   float _617 = _616 + _600;
   float _618 = _617 + _603;
   int _619 = c_bool & 64;
   bool _620 = (_619 == 0);
   if (!_620)
   {
      float _622 = 1.0f - _58.y;
      float _623 = _234 * _622;
      float _624 = _623 * _585;
      float _625 = _624 + _58.y;
      _627 = _625;
   }
   else
   {
      _627 = _58.y;
   }
   float _628 = _466 + _336;
   float _629 = _467 + _337;
   float _630 = _468 + _338;
   int _631 = c_bool & 32768;
   bool _632 = (_631 == 0);
   if (!_632)
   {
      float _644 = (c_ps_user[11].x) * _202;
      float _645 = (c_ps_user[11].x) * _203;
      float _646 = (c_ps_user[11].x) * _204;
      float _648 = _142 + 1.0f;
      float _649 = _143 + 1.0f;
      float _650 = _144 + 1.0f;
      float _651 = _648 * 0.5f;
      float _652 = _649 * 0.5f;
      float _653 = _650 * 0.5f;
      float _654 = saturate(_651);
      float _655 = saturate(_652);
      float _656 = saturate(_653);
      float _657 = (c_ps_user[11].y) * _654;
      float _658 = _655 * (c_ps_user[11].y);
      float _659 = _656 * (c_ps_user[11].y);
      float _669 = (c_ps_user[11].z) * _58.x;
      float _671 = (c_ps_user[11].w) * _628;
      float _674 = (c_ps_user[12].x) * _234;
      float _676 = (c_ps_user[12].y) * _58.y;
      float _681 = _176 * _176;
      float _682 = _176 * 0.9722222089767456f;
      float _683 = _681 * _681;
      float _684 = _683 * _682;
      float _685 = _684 + 0.02777777425944805f;
      float _686 = (c_ps_user[13].z) * _685;
      float _688 = (c_ps_user[13].w) * _605;
      float _689 = (c_ps_user[13].w) * _610;
      float _690 = (c_ps_user[13].w) * _615;
      float _693 = (c_ps_user[14].y) * _591;
      float _694 = (c_ps_user[14].y) * _592;
      float _695 = (c_ps_user[14].y) * _593;
      float _697 = (c_ps_user[14].z) * _598;
      float _698 = (c_ps_user[14].z) * _599;
      float _699 = (c_ps_user[14].z) * _600;
      float _701 = (c_ps_user[14].w) * _463;
      float _702 = (c_ps_user[14].w) * _464;
      float _703 = (c_ps_user[14].w) * _465;
      float _708 = (c_ps_user[15].z) * _466;
      float _709 = (c_ps_user[15].z) * _467;
      float _710 = (c_ps_user[15].z) * _468;
      float _711 = (c_ps_user[15].y) + (c_ps_user[15].x);
      float _712 = c_specular_scale.x * _711;
      float _713 = _712 * _628;
      float _714 = _693 + (c_ps_user[13].x);
      float _715 = _714 + _697;
      float _716 = c_diffuse_scale.x * _715;
      float _717 = _716 * c_mat_diffuse.x;
      float _718 = (c_ps_user[13].y) + _669;
      float _719 = _718 * c_specular_scale.x;
      float _720 = _719 + _713;
      float _721 = _720 * c_mat_specular.x;
      float _722 = _657 + _644;
      float _723 = _722 + _671;
      float _724 = _723 + _674;
      float _725 = _724 + _676;
      float _726 = _725 + _688;
      float _727 = _726 + _686;
      float _728 = _727 + _701;
      float _729 = _728 + _708;
      float _730 = _729 + _717;
      float _731 = _730 + _721;
      float _732 = c_specular_scale.y * _711;
      float _733 = _732 * _629;
      float _734 = _694 + (c_ps_user[13].x);
      float _735 = _734 + _698;
      float _736 = c_diffuse_scale.y * _735;
      float _737 = _736 * c_mat_diffuse.y;
      float _738 = _718 * c_specular_scale.y;
      float _739 = _738 + _733;
      float _740 = _739 * c_mat_specular.y;
      float _741 = _671 + _645;
      float _742 = _741 + _658;
      float _743 = _742 + _674;
      float _744 = _743 + _676;
      float _745 = _744 + _689;
      float _746 = _745 + _686;
      float _747 = _746 + _702;
      float _748 = _747 + _709;
      float _749 = _748 + _737;
      float _750 = _749 + _740;
      float _751 = c_specular_scale.z * _711;
      float _752 = _751 * _630;
      float _753 = _695 + (c_ps_user[13].x);
      float _754 = _753 + _699;
      float _755 = c_diffuse_scale.z * _754;
      float _756 = _755 * c_mat_diffuse.z;
      float _757 = _718 * c_specular_scale.z;
      float _758 = _757 + _752;
      float _759 = _758 * c_mat_specular.z;
      float _760 = _671 + _646;
      float _761 = _760 + _659;
      float _762 = _761 + _674;
      float _763 = _762 + _676;
      float _764 = _763 + _690;
      float _765 = _764 + _686;
      float _766 = _765 + _703;
      float _767 = _766 + _710;
      float _768 = _767 + _756;
      float _769 = _768 + _759;
      float _770 = saturate(_731);
      float _771 = saturate(_750);
      float _772 = saturate(_769);
      _794 = _770;
      _795 = _771;
      _796 = _772;
   }
   else
   {
      float _774 = _627 * _104;
      float _775 = c_diffuse_scale.x * c_mat_diffuse.x;
      float _776 = _775 * _202;
      float _777 = _776 * _608;
      float _778 = _777 * _774;
      float _779 = c_diffuse_scale.y * c_mat_diffuse.y;
      float _780 = _779 * _203;
      float _781 = _780 * _613;
      float _782 = _781 * _774;
      float _783 = c_diffuse_scale.z * c_mat_diffuse.z;
      float _784 = _783 * _204;
      float _785 = _784 * _618;
      float _786 = _785 * _774;
      float _787 = _628 * _58.x;
      float _788 = _629 * _58.x;
      float _789 = _630 * _58.x;
      float _790 = _778 + _787;
      float _791 = _782 + _788;
      float _792 = _786 + _789;
      _794 = _790;
      _795 = _791;
      _796 = _792;
   }
   float _801 = c_mat_opacity * _62.w;
   float _802 = _801 * c_opacity_scale;
   float _803 = _802 * c_global_opacity;
   float _804 = _794 * COLOR.x;
   float _805 = _795 * COLOR.y;
   float _806 = _796 * COLOR.z;
   int _807 = c_bool & 16;
   bool _808 = (_807 == 0);
   if (!_808)
   {
      float _812 = c_tone_control.x * _804;
      float _813 = c_tone_control.x * _805;
      float _814 = c_tone_control.x * _806;
      float _816 = log2(_812);
      float _817 = log2(_813);
      float _818 = log2(_814);
      float _819 = _816 * c_tone_control.y;
      float _820 = _817 * c_tone_control.y;
      float _821 = _818 * c_tone_control.y;
      float _822 = exp2(_819);
      float _823 = exp2(_820);
      float _824 = exp2(_821);
      _872 = _822;
      _873 = _823;
      _874 = _824;
   }
   else
   {
      if (_632)
      {
         float _829 = c_tone_control.x * _804;
         float _830 = c_tone_control.x * _805;
         float _831 = c_tone_control.x * _806;
         float _832 = _829 * 2.0f;
         float _833 = _830 * 2.0f;
         float _834 = _831 * 2.0f;
         float _835 = _829 * 0.30000001192092896f;
         float _836 = _830 * 0.30000001192092896f;
         float _837 = _831 * 0.30000001192092896f;
         float _838 = _835 + 0.05000000074505806f;
         float _839 = _836 + 0.05000000074505806f;
         float _840 = _837 + 0.05000000074505806f;
         float _841 = _838 * _832;
         float _842 = _839 * _833;
         float _843 = _840 * _834;
         float _844 = _841 + 0.004000000189989805f;
         float _845 = _842 + 0.004000000189989805f;
         float _846 = _843 + 0.004000000189989805f;
         float _847 = _835 + 0.5f;
         float _848 = _836 + 0.5f;
         float _849 = _837 + 0.5f;
         float _850 = _847 * _832;
         float _851 = _848 * _833;
         float _852 = _849 * _834;
         float _853 = _850 + 0.06000000238418579f;
         float _854 = _851 + 0.06000000238418579f;
         float _855 = _852 + 0.06000000238418579f;
         float _856 = _844 / _853;
         float _857 = _845 / _854;
         float _858 = _846 / _855;
         float _859 = _856 + -0.06666666269302368f;
         float _860 = _857 + -0.06666666269302368f;
         float _861 = _858 + -0.06666666269302368f;
         float _862 = _859 * 1.3790643215179443f;
         float _863 = _860 * 1.3790643215179443f;
         float _864 = _861 * 1.3790643215179443f;
         float _865 = YKR_MaterialCurve(_862, _829);
         float _866 = YKR_MaterialCurve(_863, _830);
         float _867 = YKR_MaterialCurve(_864, _831);
         float _868 = sqrt(_865);
         float _869 = sqrt(_866);
         float _870 = sqrt(_867);
         _872 = _868;
         _873 = _869;
         _874 = _870;
      }
      else
      {
         _872 = _804;
         _873 = _805;
         _874 = _806;
      }
   }
   int _875 = c_bool & 4;
   bool _876 = (_875 == 0);
   if (_876)
   {
      float _878 = saturate(COLOR_1.x);
      float _883 = (c_fog_param[0].x) - _872;
      float _884 = (c_fog_param[0].y) - _873;
      float _885 = (c_fog_param[0].z) - _874;
      float _886 = _883 * _878;
      float _887 = _884 * _878;
      float _888 = _885 * _878;
      _890 = _886;
      _891 = _887;
      _892 = _888;
   }
   else
   {
      _890 = COLOR_1.x;
      _891 = COLOR_1.y;
      _892 = COLOR_1.z;
   }
   float _893 = _890 + _872;
   float _894 = _891 + _873;
   float _895 = _892 + _874;
   if (!_632)
   {
      float _903 = (c_ps_user[3].x) * (c_ps_user[12].z);
      float _904 = (c_ps_user[3].y) * (c_ps_user[12].z);
      float _905 = (c_ps_user[3].z) * (c_ps_user[12].z);
      float _907 = (c_ps_user[12].w) * COLOR.x;
      float _908 = (c_ps_user[12].w) * COLOR.y;
      float _909 = (c_ps_user[12].w) * COLOR.z;
      float _910 = _907 + _893;
      float _911 = _910 + _903;
      float _912 = _908 + _894;
      float _913 = _912 + _904;
      float _914 = _909 + _895;
      float _915 = _914 + _905;
      _917 = _911;
      _918 = _913;
      _919 = _915;
   }
   else
   {
      _917 = _893;
      _918 = _894;
      _919 = _895;
   }
   float _925 = (c_ps_col_scl[0].x) * _917;
   float _926 = (c_ps_col_scl[0].y) * _918;
   float _927 = (c_ps_col_scl[0].z) * _919;
   float _928 = (c_ps_col_scl[0].w) * _803;
   SV_Target.x = _925;
   SV_Target.y = _926;
   SV_Target.z = _927;
   SV_Target.w = _928;
   int _931 = c_bool & 8;
   bool _932 = (_931 != 0);
   float _933 = select(_932, _917, _925);
   float _934 = select(_932, _918, _926);
   float _935 = select(_932, _919, _927);
   float _936 = select(_932, _803, _928);
   SV_Target.x = _933;
   SV_Target.y = _934;
   SV_Target.z = _935;
   SV_Target.w = _936;
   return SV_Target;
}
