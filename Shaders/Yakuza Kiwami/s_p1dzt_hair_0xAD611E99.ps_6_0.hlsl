// Yakuza Kiwami material s_p1dzt[hair] (0xAD611E99, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
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
   float _73 = c_mat_power * _56.z;
   float _74 = max(1.0f, _73);
   float _75 = _68.x * 2.0f;
   float _76 = _68.y * 2.0f;
   float _77 = _75 + -1.0f;
   float _78 = _76 + -1.0f;
   float _79 = dot(float2(_77, _78), float2(_77, _78));
   float _80 = 1.0f - _79;
   float _81 = saturate(_80);
   float _82 = sqrt(_81);
   float _83 = TEXCOORD_2.y * TEXCOORD_3.z;
   float _84 = TEXCOORD_2.z * TEXCOORD_3.y;
   float _85 = _83 - _84;
   float _86 = TEXCOORD_2.z * TEXCOORD_3.x;
   float _87 = TEXCOORD_2.x * TEXCOORD_3.z;
   float _88 = _86 - _87;
   float _89 = TEXCOORD_2.x * TEXCOORD_3.y;
   float _90 = TEXCOORD_2.y * TEXCOORD_3.x;
   float _91 = _89 - _90;
   float _92 = _77 * TEXCOORD_3.x;
   float _93 = _77 * TEXCOORD_3.y;
   float _94 = _77 * TEXCOORD_3.z;
   float _95 = _78 * TEXCOORD_3.w;
   float _96 = _95 * _85;
   float _97 = _95 * _88;
   float _98 = _95 * _91;
   float _99 = _96 + _92;
   float _100 = _97 + _93;
   float _101 = _98 + _94;
   float _102 = _82 * TEXCOORD_2.x;
   float _103 = _82 * TEXCOORD_2.y;
   float _104 = _82 * TEXCOORD_2.z;
   float _105 = _99 + _102;
   float _106 = _100 + _103;
   float _107 = _101 + _104;
   float _108 = dot(float3(_105, _106, _107), float3(_105, _106, _107));
   float _109 = rsqrt(_108);
   float _110 = _105 * _109;
   float _111 = _106 * _109;
   float _112 = _107 * _109;
   float _117 = dot(float3(TEXCOORD_1.x, TEXCOORD_1.y, TEXCOORD_1.z), float3(TEXCOORD_1.x, TEXCOORD_1.y, TEXCOORD_1.z));
   float _118 = rsqrt(_117);
   float _119 = _118 * TEXCOORD_1.x;
   float _120 = _118 * TEXCOORD_1.y;
   float _121 = _118 * TEXCOORD_1.z;
   float _122 = _119 + (c_light_vec[0].x);
   float _123 = _120 + (c_light_vec[0].y);
   float _124 = _121 + (c_light_vec[0].z);
   float _125 = dot(float3(_122, _123, _124), float3(_122, _123, _124));
   float _126 = rsqrt(_125);
   float _127 = _122 * _126;
   float _128 = _123 * _126;
   float _129 = _124 * _126;
   float _130 = _111 + 0.10000000149011612f;
   float _131 = saturate(_130);
   float _134 = (c_attribute_ps[2].w) + 2.0f;
   float _136 = (c_attribute_ps[2].z) + 2.5f;
   float _137 = -0.0f - _119;
   float _138 = -0.0f - _120;
   float _139 = -0.0f - _121;
   float _140 = dot(float3(_137, _138, _139), float3(TEXCOORD_2.x, TEXCOORD_2.y, TEXCOORD_2.z));
   float _141 = 1.0f - _140;
   float _142 = saturate(_141);
   float _143 = dot(float3(_137, _138, _139), float3(_110, _111, _112));
   float _144 = 1.0f - _143;
   float _145 = saturate(_144);
   float _146 = _145 * _142;
   float _147 = log2(_146);
   float _148 = _147 * _134;
   float _149 = exp2(_148);
   float _150 = _149 * _136;
   int _153 = c_bool & 4096;
   bool _154 = (_153 == 0);
   float _163;
   float _187;
   float _188;
   float _200;
   float _201;
   float _366;
   float _367;
   float _368;
   float _397;
   float _398;
   float _399;
   float _561;
   float _717;
   float _718;
   float _719;
   float _795;
   float _796;
   float _797;
   float _813;
   float _814;
   float _815;
   float _840;
   float _841;
   float _842;
   if (!_154)
   {
      float _159 = (c_ps_user[9].w) * _147;
      float _160 = exp2(_159);
      float _161 = _160 * (c_ps_user[9].z);
      _163 = _161;
   }
   else
   {
      _163 = _150;
   }
   float _164 = _131 * _163;
   float _165 = _56.y * _164;
   float _168 = _165 * (c_ps_user[2].x);
   float _169 = _168 + 1.0f;
   float _170 = _65 * _169;
   float _171 = _66 * _169;
   float _172 = _67 * _169;
   float _176 = c_rt_size.z * SV_Position.x;
   float _177 = c_rt_size.w * SV_Position.y;
   int _180 = c_bool & 8;
   bool _181 = (_180 == 0);
   if (!_181)
   {
      float4 _183 = s13_t.Sample(s13_s, float2(_176, _177));
      _187 = _183.x;
      _188 = _183.y;
   }
   else
   {
      _187 = 1.0f;
      _188 = 1.0f;
   }
   if (!_181)
   {
      float _193 = (c_sdw_param_ch[0].x) * _187;
      float _194 = (c_sdw_param_ch[0].z) * _188;
      float _197 = _193 + (c_sdw_param_ch[0].y);
      float _198 = _194 + (c_sdw_param_ch[0].w);
      _200 = _197;
      _201 = _198;
   }
   else
   {
      _200 = _187;
      _201 = _188;
   }
   float _202 = min(_200, _201);
   float _207 = (c_light_vec[1].x) + _119;
   float _208 = (c_light_vec[1].y) + _120;
   float _209 = (c_light_vec[1].z) + _121;
   float _210 = dot(float3(_207, _208, _209), float3(_207, _208, _209));
   float _211 = rsqrt(_210);
   float _212 = _207 * _211;
   float _213 = _208 * _211;
   float _214 = _209 * _211;
   float _215 = _111 * TEXCOORD_3.z;
   float _216 = _112 * TEXCOORD_3.y;
   float _217 = _215 - _216;
   float _218 = _112 * TEXCOORD_3.x;
   float _219 = _110 * TEXCOORD_3.z;
   float _220 = _218 - _219;
   float _221 = _110 * TEXCOORD_3.y;
   float _222 = _111 * TEXCOORD_3.x;
   float _223 = _221 - _222;
   float _224 = _217 * TEXCOORD_3.w;
   float _225 = _220 * TEXCOORD_3.w;
   float _226 = _223 * TEXCOORD_3.w;
   float _227 = _225 * _112;
   float _228 = _226 * _111;
   float _229 = _227 - _228;
   float _230 = _226 * _110;
   float _231 = _224 * _112;
   float _232 = _230 - _231;
   float _233 = _224 * _111;
   float _234 = _225 * _110;
   float _235 = _233 - _234;
   float _236 = -0.0f - _127;
   float _237 = -0.0f - _128;
   float _238 = -0.0f - _129;
   float _239 = dot(float3(_110, _111, _112), float3(_236, _237, _238));
   float _240 = dot(float3(_236, _237, _238), float3(_224, _225, _226));
   float _241 = TEXCOORD.z * 0.20000000298023224f;
   float _242 = _240 * _241;
   float _243 = _242 + 0.5f;
   float4 _244 = s7_t.Sample(s7_s, float2(_239, _243));
   float _246 = dot(float3(_236, _237, _238), float3(_229, _232, _235));
   float _247 = TEXCOORD.w * 0.20000000298023224f;
   float _248 = _246 * _247;
   float _249 = _248 + 0.5f;
   float4 _250 = s7_t.Sample(s7_s, float2(_239, _249));
   float _252 = _250.x * _244.x;
   float _253 = -0.0f - _212;
   float _254 = -0.0f - _213;
   float _255 = -0.0f - _214;
   float _256 = dot(float3(_110, _111, _112), float3(_253, _254, _255));
   float _257 = dot(float3(_253, _254, _255), float3(_224, _225, _226));
   float _258 = _257 * _241;
   float _259 = _258 + 0.5f;
   float4 _260 = s7_t.Sample(s7_s, float2(_256, _259));
   float _262 = dot(float3(_253, _254, _255), float3(_229, _232, _235));
   float _263 = _262 * _247;
   float _264 = _263 + 0.5f;
   float4 _265 = s7_t.Sample(s7_s, float2(_256, _264));
   float _267 = _265.x * _260.x;
   float _268 = log2(_252);
   float _269 = log2(_267);
   float _270 = _268 * _74;
   float _271 = _269 * _74;
   float _272 = exp2(_270);
   float _273 = exp2(_271);
   float _278 = c_mat_fresnel * c_tone_control.w;
   float _279 = _74 * 0.03974359855055809f;
   float _280 = _279 + 0.0856831967830658f;
   float _281 = _273 * _280;
   float _282 = dot(float3(_119, _120, _121), float3(_127, _128, _129));
   float _283 = dot(float3(_119, _120, _121), float3(_212, _213, _214));
   float _284 = 1.0f - _282;
   float _285 = 1.0f - _283;
   float _286 = _284 * _284;
   float _287 = _285 * _285;
   float _288 = 1.0f - _278;
   float _289 = _286 * _286;
   float _290 = _289 * _284;
   float _291 = _290 * _288;
   float _292 = _285 * _288;
   float _293 = _287 * _287;
   float _294 = _293 * _292;
   float _295 = _291 + _278;
   float _296 = _294 + _278;
   float _297 = _281 * _296;
   float _306 = c_specular_scale.x * c_mat_specular.x;
   float _307 = c_specular_scale.y * c_mat_specular.y;
   float _308 = c_specular_scale.z * c_mat_specular.z;
   float _313 = _202 * _280;
   float _314 = _313 * _272;
   float _315 = _314 * _295;
   float _316 = _315 * (c_light_col[0].x);
   float _317 = _315 * (c_light_col[0].y);
   float _318 = _315 * (c_light_col[0].z);
   float _323 = _297 * (c_light_col[1].x);
   float _324 = _297 * (c_light_col[1].y);
   float _325 = _297 * (c_light_col[1].z);
   float _326 = _316 + _323;
   float _327 = _317 + _324;
   float _328 = _318 + _325;
   float _329 = _306 * _326;
   float _330 = _307 * _327;
   float _331 = _308 * _328;
   int _337 = c_bool & 1;
   bool _338 = (_337 == 0);
   if (!_338)
   {
      float _340 = -0.0f - TEXCOORD_4.x;
      float _341 = -0.0f - TEXCOORD_4.y;
      float _342 = -0.0f - TEXCOORD_4.z;
      float _343 = dot(float3(_110, _111, _112), float3(_340, _341, _342));
      float _344 = saturate(_343);
      float _345 = _344 * TEXCOORD_1.w;
      float _346 = _344 * TEXCOORD_2.w;
      float _347 = _344 * TEXCOORD_4.w;
      float _348 = max(0.0f, _345);
      float _349 = max(0.0f, _346);
      float _350 = max(0.0f, _347);
      float _351 = -0.0f - TEXCOORD_5.x;
      float _352 = -0.0f - TEXCOORD_5.y;
      float _353 = -0.0f - TEXCOORD_5.z;
      float _354 = dot(float3(_110, _111, _112), float3(_351, _352, _353));
      float _355 = saturate(_354);
      float _356 = _355 * COLOR.w;
      float _357 = _355 * COLOR_1.w;
      float _358 = _355 * TEXCOORD_5.w;
      float _359 = max(0.0f, _356);
      float _360 = max(0.0f, _357);
      float _361 = max(0.0f, _358);
      float _362 = _359 + _348;
      float _363 = _360 + _349;
      float _364 = _361 + _350;
      _366 = _362;
      _367 = _363;
      _368 = _364;
   }
   else
   {
      _366 = 0.0f;
      _367 = 0.0f;
      _368 = 0.0f;
   }
   float _369 = c_rt_size.z * SV_Position.x;
   float _370 = c_rt_size.w * SV_Position.y;
   float4 _371 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_369, _370)));
   int _377 = c_bool & 16384;
   bool _378 = (_377 == 0);
   if (!_378)
   {
      float _380 = _371.x * 4.0f;
      float _381 = _371.y * 4.0f;
      float _382 = _371.z * 4.0f;
      _397 = _380;
      _398 = _381;
      _399 = _382;
   }
   else
   {
      int _384 = c_bool & 8192;
      bool _385 = (_384 == 0);
      if (!_385)
      {
         float _387 = _371.x * 2.0f;
         float _388 = _371.y * 2.0f;
         float _389 = _371.z * 2.0f;
         _397 = _387;
         _398 = _388;
         _399 = _389;
      }
      else
      {
         float _393 = _371.x / c_tone_control.z;
         float _394 = _371.y / c_tone_control.z;
         float _395 = _371.z / c_tone_control.z;
         _397 = _393;
         _398 = _394;
         _399 = _395;
      }
   }
   float _400 = _397 + _366;
   float _401 = _398 + _367;
   float _402 = _399 + _368;
   float _465 = dot(float3(_110, _111, _112), float3(_110, _111, _112));
   float _466 = rsqrt(_465);
   float _467 = _466 * _110;
   float _468 = _466 * _111;
   float _469 = _466 * _112;
   float _470 = dot(float4((c_ps_scratch[80].x), (c_ps_scratch[80].y), (c_ps_scratch[80].z), (c_ps_scratch[80].w)), float4(_467, _468, _469, 1.0f));
   float _471 = dot(float4((c_ps_scratch[81].x), (c_ps_scratch[81].y), (c_ps_scratch[81].z), (c_ps_scratch[81].w)), float4(_467, _468, _469, 1.0f));
   float _472 = dot(float4((c_ps_scratch[82].x), (c_ps_scratch[82].y), (c_ps_scratch[82].z), (c_ps_scratch[82].w)), float4(_467, _468, _469, 1.0f));
   float _473 = dot(float4((c_ps_scratch[83].x), (c_ps_scratch[83].y), (c_ps_scratch[83].z), (c_ps_scratch[83].w)), float4(_467, _468, _469, 1.0f));
   float _474 = dot(float4(_467, _468, _469, 1.0f), float4(_470, _471, _472, _473));
   float _475 = dot(float4((c_ps_scratch[84].x), (c_ps_scratch[84].y), (c_ps_scratch[84].z), (c_ps_scratch[84].w)), float4(_467, _468, _469, 1.0f));
   float _476 = dot(float4((c_ps_scratch[85].x), (c_ps_scratch[85].y), (c_ps_scratch[85].z), (c_ps_scratch[85].w)), float4(_467, _468, _469, 1.0f));
   float _477 = dot(float4((c_ps_scratch[86].x), (c_ps_scratch[86].y), (c_ps_scratch[86].z), (c_ps_scratch[86].w)), float4(_467, _468, _469, 1.0f));
   float _478 = dot(float4((c_ps_scratch[87].x), (c_ps_scratch[87].y), (c_ps_scratch[87].z), (c_ps_scratch[87].w)), float4(_467, _468, _469, 1.0f));
   float _479 = dot(float4(_467, _468, _469, 1.0f), float4(_475, _476, _477, _478));
   float _480 = dot(float4((c_ps_scratch[88].x), (c_ps_scratch[88].y), (c_ps_scratch[88].z), (c_ps_scratch[88].w)), float4(_467, _468, _469, 1.0f));
   float _481 = dot(float4((c_ps_scratch[89].x), (c_ps_scratch[89].y), (c_ps_scratch[89].z), (c_ps_scratch[89].w)), float4(_467, _468, _469, 1.0f));
   float _482 = dot(float4((c_ps_scratch[90].x), (c_ps_scratch[90].y), (c_ps_scratch[90].z), (c_ps_scratch[90].w)), float4(_467, _468, _469, 1.0f));
   float _483 = dot(float4((c_ps_scratch[91].x), (c_ps_scratch[91].y), (c_ps_scratch[91].z), (c_ps_scratch[91].w)), float4(_467, _468, _469, 1.0f));
   float _484 = dot(float4(_467, _468, _469, 1.0f), float4(_480, _481, _482, _483));
   float _485 = max(_474, 0.0625f);
   float _486 = max(_479, 0.0625f);
   float _487 = max(_484, 0.0625f);
   float _488 = _485 * (c_ps_scratch[79].x);
   float _489 = _486 * (c_ps_scratch[79].x);
   float _490 = _487 * (c_ps_scratch[79].x);
   float _495 = -0.0f - (c_light_vec[0].x);
   float _496 = -0.0f - (c_light_vec[0].y);
   float _497 = -0.0f - (c_light_vec[0].z);
   float _498 = dot(float3(_110, _111, _112), float3(_495, _496, _497));
   float _499 = -0.0f - (c_light_vec[1].x);
   float _500 = -0.0f - (c_light_vec[1].y);
   float _501 = -0.0f - (c_light_vec[1].z);
   float _502 = dot(float3(_110, _111, _112), float3(_499, _500, _501));
   float _511 = _111 * 0.5f;
   float _512 = _511 + 0.5f;
   float _513 = (c_light_col[4].x) - (c_light_vec[4].x);
   float _514 = (c_light_col[4].y) - (c_light_vec[4].y);
   float _515 = (c_light_col[4].z) - (c_light_vec[4].z);
   float _516 = _513 * _512;
   float _517 = _514 * _512;
   float _518 = _515 * _512;
   float _519 = saturate(_498);
   float _520 = saturate(_502);
   float _525 = (c_light_col[0].x) * _519;
   float _526 = (c_light_col[0].y) * _519;
   float _527 = (c_light_col[0].z) * _519;
   float _532 = (c_light_col[1].x) * _520;
   float _533 = (c_light_col[1].y) * _520;
   float _534 = (c_light_col[1].z) * _520;
   float _535 = _525 * _202;
   float _536 = _526 * _202;
   float _537 = _527 * _202;
   float _538 = (c_light_vec[4].x) + _488;
   float _539 = _538 + _516;
   float _540 = _539 + _400;
   float _541 = _540 + _532;
   float _542 = _541 + _535;
   float _543 = (c_light_vec[4].y) + _489;
   float _544 = _543 + _517;
   float _545 = _544 + _401;
   float _546 = _545 + _533;
   float _547 = _546 + _536;
   float _548 = (c_light_vec[4].z) + _490;
   float _549 = _548 + _518;
   float _550 = _549 + _402;
   float _551 = _550 + _534;
   float _552 = _551 + _537;
   int _553 = c_bool & 64;
   bool _554 = (_553 == 0);
   if (!_554)
   {
      float _556 = 1.0f - _56.y;
      float _557 = _202 * _556;
      float _558 = _557 * _519;
      float _559 = _558 + _56.y;
      _561 = _559;
   }
   else
   {
      _561 = _56.y;
   }
   int _562 = c_bool & 32768;
   bool _563 = (_562 == 0);
   if (!_563)
   {
      float _575 = (c_ps_user[11].x) * _170;
      float _576 = (c_ps_user[11].x) * _171;
      float _577 = (c_ps_user[11].x) * _172;
      float _579 = _110 + 1.0f;
      float _580 = _111 + 1.0f;
      float _581 = _112 + 1.0f;
      float _582 = _579 * 0.5f;
      float _583 = _580 * 0.5f;
      float _584 = _581 * 0.5f;
      float _585 = saturate(_582);
      float _586 = saturate(_583);
      float _587 = saturate(_584);
      float _588 = (c_ps_user[11].y) * _585;
      float _589 = _586 * (c_ps_user[11].y);
      float _590 = _587 * (c_ps_user[11].y);
      float _600 = (c_ps_user[11].z) * _56.x;
      float _602 = (c_ps_user[11].w) * _329;
      float _605 = (c_ps_user[12].x) * _202;
      float _607 = (c_ps_user[12].y) * _56.y;
      float _612 = _144 * _144;
      float _613 = _144 * 0.9722222089767456f;
      float _614 = _612 * _612;
      float _615 = _614 * _613;
      float _616 = _615 + 0.02777777425944805f;
      float _617 = (c_ps_user[13].z) * _616;
      float _619 = (c_ps_user[13].w) * _539;
      float _620 = (c_ps_user[13].w) * _544;
      float _621 = (c_ps_user[13].w) * _549;
      float _624 = (c_ps_user[14].y) * _525;
      float _625 = (c_ps_user[14].y) * _526;
      float _626 = (c_ps_user[14].y) * _527;
      float _628 = (c_ps_user[14].z) * _532;
      float _629 = (c_ps_user[14].z) * _533;
      float _630 = (c_ps_user[14].z) * _534;
      float _632 = (c_ps_user[14].w) * _400;
      float _633 = (c_ps_user[14].w) * _401;
      float _634 = (c_ps_user[14].w) * _402;
      float _638 = (c_ps_user[15].y) + (c_ps_user[15].x);
      float _639 = c_specular_scale.x * _638;
      float _640 = _639 * _329;
      float _641 = _624 + (c_ps_user[13].x);
      float _642 = _641 + _628;
      float _643 = c_diffuse_scale.x * _642;
      float _644 = _643 * c_mat_diffuse.x;
      float _645 = (c_ps_user[13].y) + _600;
      float _646 = _645 * c_specular_scale.x;
      float _647 = _646 + _640;
      float _648 = _647 * c_mat_specular.x;
      float _649 = _588 + _575;
      float _650 = _649 + _602;
      float _651 = _650 + _605;
      float _652 = _651 + _607;
      float _653 = _652 + _619;
      float _654 = _653 + _617;
      float _655 = _654 + _632;
      float _656 = _655 + _644;
      float _657 = _656 + _648;
      float _658 = c_specular_scale.y * _638;
      float _659 = _658 * _330;
      float _660 = _625 + (c_ps_user[13].x);
      float _661 = _660 + _629;
      float _662 = c_diffuse_scale.y * _661;
      float _663 = _662 * c_mat_diffuse.y;
      float _664 = _645 * c_specular_scale.y;
      float _665 = _664 + _659;
      float _666 = _665 * c_mat_specular.y;
      float _667 = _602 + _576;
      float _668 = _667 + _589;
      float _669 = _668 + _605;
      float _670 = _669 + _607;
      float _671 = _670 + _620;
      float _672 = _671 + _617;
      float _673 = _672 + _633;
      float _674 = _673 + _663;
      float _675 = _674 + _666;
      float _676 = c_specular_scale.z * _638;
      float _677 = _676 * _331;
      float _678 = _626 + (c_ps_user[13].x);
      float _679 = _678 + _630;
      float _680 = c_diffuse_scale.z * _679;
      float _681 = _680 * c_mat_diffuse.z;
      float _682 = _645 * c_specular_scale.z;
      float _683 = _682 + _677;
      float _684 = _683 * c_mat_specular.z;
      float _685 = _602 + _577;
      float _686 = _685 + _590;
      float _687 = _686 + _605;
      float _688 = _687 + _607;
      float _689 = _688 + _621;
      float _690 = _689 + _617;
      float _691 = _690 + _634;
      float _692 = _691 + _681;
      float _693 = _692 + _684;
      float _694 = saturate(_657);
      float _695 = saturate(_675);
      float _696 = saturate(_693);
      _717 = _694;
      _718 = _695;
      _719 = _696;
   }
   else
   {
      float _698 = c_diffuse_scale.x * c_mat_diffuse.x;
      float _699 = _698 * _170;
      float _700 = _699 * _542;
      float _701 = _700 * _561;
      float _702 = c_diffuse_scale.y * c_mat_diffuse.y;
      float _703 = _702 * _171;
      float _704 = _703 * _547;
      float _705 = _704 * _561;
      float _706 = c_diffuse_scale.z * c_mat_diffuse.z;
      float _707 = _706 * _172;
      float _708 = _707 * _552;
      float _709 = _708 * _561;
      float _710 = _329 * _56.x;
      float _711 = _330 * _56.x;
      float _712 = _331 * _56.x;
      float _713 = _701 + _710;
      float _714 = _705 + _711;
      float _715 = _709 + _712;
      _717 = _713;
      _718 = _714;
      _719 = _715;
   }
   float _724 = c_mat_opacity * _60.w;
   float _725 = _724 * c_opacity_scale;
   float _726 = _725 * c_global_opacity;
   float _727 = _717 * COLOR.x;
   float _728 = _718 * COLOR.y;
   float _729 = _719 * COLOR.z;
   int _730 = c_bool & 16;
   bool _731 = (_730 == 0);
   if (!_731)
   {
      float _735 = c_tone_control.x * _727;
      float _736 = c_tone_control.x * _728;
      float _737 = c_tone_control.x * _729;
      float _739 = log2(_735);
      float _740 = log2(_736);
      float _741 = log2(_737);
      float _742 = _739 * c_tone_control.y;
      float _743 = _740 * c_tone_control.y;
      float _744 = _741 * c_tone_control.y;
      float _745 = exp2(_742);
      float _746 = exp2(_743);
      float _747 = exp2(_744);
      _795 = _745;
      _796 = _746;
      _797 = _747;
   }
   else
   {
      if (_563)
      {
         float _752 = c_tone_control.x * _727;
         float _753 = c_tone_control.x * _728;
         float _754 = c_tone_control.x * _729;
         float _755 = _752 * 2.0f;
         float _756 = _753 * 2.0f;
         float _757 = _754 * 2.0f;
         float _758 = _752 * 0.30000001192092896f;
         float _759 = _753 * 0.30000001192092896f;
         float _760 = _754 * 0.30000001192092896f;
         float _761 = _758 + 0.05000000074505806f;
         float _762 = _759 + 0.05000000074505806f;
         float _763 = _760 + 0.05000000074505806f;
         float _764 = _761 * _755;
         float _765 = _762 * _756;
         float _766 = _763 * _757;
         float _767 = _764 + 0.004000000189989805f;
         float _768 = _765 + 0.004000000189989805f;
         float _769 = _766 + 0.004000000189989805f;
         float _770 = _758 + 0.5f;
         float _771 = _759 + 0.5f;
         float _772 = _760 + 0.5f;
         float _773 = _770 * _755;
         float _774 = _771 * _756;
         float _775 = _772 * _757;
         float _776 = _773 + 0.06000000238418579f;
         float _777 = _774 + 0.06000000238418579f;
         float _778 = _775 + 0.06000000238418579f;
         float _779 = _767 / _776;
         float _780 = _768 / _777;
         float _781 = _769 / _778;
         float _782 = _779 + -0.06666666269302368f;
         float _783 = _780 + -0.06666666269302368f;
         float _784 = _781 + -0.06666666269302368f;
         float _785 = _782 * 1.3790643215179443f;
         float _786 = _783 * 1.3790643215179443f;
         float _787 = _784 * 1.3790643215179443f;
         float _788 = YKR_MaterialCurve(_785, _752);
         float _789 = YKR_MaterialCurve(_786, _753);
         float _790 = YKR_MaterialCurve(_787, _754);
         float _791 = sqrt(_788);
         float _792 = sqrt(_789);
         float _793 = sqrt(_790);
         _795 = _791;
         _796 = _792;
         _797 = _793;
      }
      else
      {
         _795 = _727;
         _796 = _728;
         _797 = _729;
      }
   }
   int _798 = c_bool & 4;
   bool _799 = (_798 == 0);
   if (_799)
   {
      float _801 = saturate(COLOR_1.x);
      float _806 = (c_fog_param[0].x) - _795;
      float _807 = (c_fog_param[0].y) - _796;
      float _808 = (c_fog_param[0].z) - _797;
      float _809 = _806 * _801;
      float _810 = _807 * _801;
      float _811 = _808 * _801;
      _813 = _809;
      _814 = _810;
      _815 = _811;
   }
   else
   {
      _813 = COLOR_1.x;
      _814 = COLOR_1.y;
      _815 = COLOR_1.z;
   }
   float _816 = _813 + _795;
   float _817 = _814 + _796;
   float _818 = _815 + _797;
   if (!_563)
   {
      float _826 = (c_ps_user[3].x) * (c_ps_user[12].z);
      float _827 = (c_ps_user[3].y) * (c_ps_user[12].z);
      float _828 = (c_ps_user[3].z) * (c_ps_user[12].z);
      float _830 = (c_ps_user[12].w) * COLOR.x;
      float _831 = (c_ps_user[12].w) * COLOR.y;
      float _832 = (c_ps_user[12].w) * COLOR.z;
      float _833 = _830 + _816;
      float _834 = _833 + _826;
      float _835 = _831 + _817;
      float _836 = _835 + _827;
      float _837 = _832 + _818;
      float _838 = _837 + _828;
      _840 = _834;
      _841 = _836;
      _842 = _838;
   }
   else
   {
      _840 = _816;
      _841 = _817;
      _842 = _818;
   }
   float _845 = _726 - c_mat_punchthrough_ref;
   bool _846 = (_845 < 0.0f);
   if (_846)
      discard;
   float _852 = (c_ps_col_scl[0].x) * _840;
   float _853 = (c_ps_col_scl[0].y) * _841;
   float _854 = (c_ps_col_scl[0].z) * _842;
   float _855 = (c_ps_col_scl[0].w) * _726;
   SV_Target.x = _852;
   SV_Target.y = _853;
   SV_Target.z = _854;
   SV_Target.w = _855;
   int _858 = c_bool & 8;
   bool _859 = (_858 != 0);
   float _860 = select(_859, _840, _852);
   float _861 = select(_859, _841, _853);
   float _862 = select(_859, _842, _854);
   float _863 = select(_859, _726, _855);
   SV_Target.x = _860;
   SV_Target.y = _861;
   SV_Target.z = _862;
   SV_Target.w = _863;
   return SV_Target;
}
