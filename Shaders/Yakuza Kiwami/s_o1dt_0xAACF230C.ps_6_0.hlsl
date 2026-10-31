// Yakuza Kiwami material s_o1dt (0xAACF230C, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
// applied by a script: the inline Hable tonemap's saturate goes through
// YKR_MaterialCurve (tangent continuation in HDR), and the light pre-pass buffer t14 through YKR_LightBuffer.

#include "Includes/Common.hlsl"

Texture2D<float4> s0_t : register(t0);

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

SamplerState s5_s : register(s5);

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
   float4 _50 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _55 = _50.x * _50.x;
   float _56 = _50.y * _50.y;
   float _57 = _50.z * _50.z;
   float4 _58 = s5_t.Sample(s5_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _63 = _58.x * 2.0f;
   float _64 = _58.y * 2.0f;
   float _65 = _63 + -1.0f;
   float _66 = _64 + -1.0f;
   float _67 = dot(float2(_65, _66), float2(_65, _66));
   float _68 = 1.0f - _67;
   float _69 = saturate(_68);
   float _70 = sqrt(_69);
   float _71 = TEXCOORD_2.y * TEXCOORD_3.z;
   float _72 = TEXCOORD_2.z * TEXCOORD_3.y;
   float _73 = _71 - _72;
   float _74 = TEXCOORD_2.z * TEXCOORD_3.x;
   float _75 = TEXCOORD_2.x * TEXCOORD_3.z;
   float _76 = _74 - _75;
   float _77 = TEXCOORD_2.x * TEXCOORD_3.y;
   float _78 = TEXCOORD_2.y * TEXCOORD_3.x;
   float _79 = _77 - _78;
   float _80 = _65 * TEXCOORD_3.x;
   float _81 = _65 * TEXCOORD_3.y;
   float _82 = _65 * TEXCOORD_3.z;
   float _83 = _66 * TEXCOORD_3.w;
   float _84 = _83 * _73;
   float _85 = _83 * _76;
   float _86 = _83 * _79;
   float _87 = _84 + _80;
   float _88 = _85 + _81;
   float _89 = _86 + _82;
   float _90 = _70 * TEXCOORD_2.x;
   float _91 = _70 * TEXCOORD_2.y;
   float _92 = _70 * TEXCOORD_2.z;
   float _93 = _87 + _90;
   float _94 = _88 + _91;
   float _95 = _89 + _92;
   float _96 = dot(float3(_93, _94, _95), float3(_93, _94, _95));
   float _97 = rsqrt(_96);
   float _98 = _93 * _97;
   float _99 = _94 * _97;
   float _100 = _95 * _97;
   float _105 = dot(float3(TEXCOORD_1.x, TEXCOORD_1.y, TEXCOORD_1.z), float3(TEXCOORD_1.x, TEXCOORD_1.y, TEXCOORD_1.z));
   float _106 = rsqrt(_105);
   float _107 = _106 * TEXCOORD_1.x;
   float _108 = _106 * TEXCOORD_1.y;
   float _109 = _106 * TEXCOORD_1.z;
   float _110 = _107 + (c_light_vec[0].x);
   float _111 = _108 + (c_light_vec[0].y);
   float _112 = _109 + (c_light_vec[0].z);
   float _113 = dot(float3(_110, _111, _112), float3(_110, _111, _112));
   float _114 = rsqrt(_113);
   float _115 = _110 * _114;
   float _116 = _111 * _114;
   float _117 = _112 * _114;
   float _118 = _99 + 0.10000000149011612f;
   float _119 = saturate(_118);
   float _122 = (c_attribute_ps[2].w) + 2.0f;
   float _124 = (c_attribute_ps[2].z) + 2.5f;
   float _125 = -0.0f - _107;
   float _126 = -0.0f - _108;
   float _127 = -0.0f - _109;
   float _128 = dot(float3(_125, _126, _127), float3(TEXCOORD_2.x, TEXCOORD_2.y, TEXCOORD_2.z));
   float _129 = 1.0f - _128;
   float _130 = saturate(_129);
   float _131 = dot(float3(_125, _126, _127), float3(_98, _99, _100));
   float _132 = 1.0f - _131;
   float _133 = saturate(_132);
   float _134 = _133 * _130;
   float _135 = log2(_134);
   float _136 = _135 * _122;
   float _137 = exp2(_136);
   float _138 = _137 * _124;
   int _141 = c_bool & 4096;
   bool _142 = (_141 == 0);
   float _151;
   float _174;
   float _175;
   float _187;
   float _188;
   float _376;
   float _377;
   float _378;
   float _379;
   float _380;
   float _381;
   float _414;
   float _415;
   float _416;
   float _417;
   float _731;
   float _732;
   float _733;
   float _809;
   float _810;
   float _811;
   float _827;
   float _828;
   float _829;
   float _854;
   float _855;
   float _856;
   if (!_142)
   {
      float _147 = (c_ps_user[9].w) * _135;
      float _148 = exp2(_147);
      float _149 = _148 * (c_ps_user[9].z);
      _151 = _149;
   }
   else
   {
      _151 = _138;
   }
   float _152 = _119 * _151;
   float _155 = (c_ps_user[2].x) * _152;
   float _156 = _155 + 1.0f;
   float _157 = _55 * _156;
   float _158 = _56 * _156;
   float _159 = _57 * _156;
   float _163 = c_rt_size.z * SV_Position.x;
   float _164 = c_rt_size.w * SV_Position.y;
   int _167 = c_bool & 8;
   bool _168 = (_167 == 0);
   if (!_168)
   {
      float4 _170 = s13_t.Sample(s13_s, float2(_163, _164));
      _174 = _170.x;
      _175 = _170.y;
   }
   else
   {
      _174 = 1.0f;
      _175 = 1.0f;
   }
   if (!_168)
   {
      float _180 = (c_sdw_param_ch[0].x) * _174;
      float _181 = (c_sdw_param_ch[0].z) * _175;
      float _184 = _180 + (c_sdw_param_ch[0].y);
      float _185 = _181 + (c_sdw_param_ch[0].w);
      _187 = _184;
      _188 = _185;
   }
   else
   {
      _187 = _174;
      _188 = _175;
   }
   float _189 = min(_187, _188);
   float _198 = (c_light_vec[1].x) + _107;
   float _199 = (c_light_vec[1].y) + _108;
   float _200 = (c_light_vec[1].z) + _109;
   float _201 = dot(float3(_198, _199, _200), float3(_198, _199, _200));
   float _202 = rsqrt(_201);
   float _203 = _198 * _202;
   float _204 = _199 * _202;
   float _205 = _200 * _202;
   float _206 = -0.0f - (c_light_vec[1].x);
   float _207 = -0.0f - (c_light_vec[1].y);
   float _208 = -0.0f - (c_light_vec[1].z);
   float _209 = -0.0f - (c_light_vec[0].x);
   float _210 = -0.0f - (c_light_vec[0].y);
   float _211 = -0.0f - (c_light_vec[0].z);
   float _212 = dot(float3(_107, _108, _109), float3(_98, _99, _100));
   float _213 = _212 * 2.0f;
   float _214 = _213 * _98;
   float _215 = _213 * _99;
   float _216 = _213 * _100;
   float _217 = _107 - _214;
   float _218 = _108 - _215;
   float _219 = _109 - _216;
   float _220 = dot(float3(_217, _218, _219), float3(_209, _210, _211));
   float _221 = dot(float3(_217, _218, _219), float3(_206, _207, _208));
   float _222 = saturate(_220);
   float _223 = saturate(_221);
   float _224 = log2(_222);
   float _225 = log2(_223);
   float _226 = _224 * c_mat_power;
   float _227 = _225 * c_mat_power;
   float _228 = exp2(_226);
   float _229 = exp2(_227);
   float _234 = c_mat_fresnel * c_tone_control.w;
   float _235 = c_mat_power * 0.03974359855055809f;
   float _236 = _235 + 0.0856831967830658f;
   float _237 = _229 * _236;
   float _238 = dot(float3(_107, _108, _109), float3(_115, _116, _117));
   float _239 = dot(float3(_107, _108, _109), float3(_203, _204, _205));
   float _240 = 1.0f - _238;
   float _241 = 1.0f - _239;
   float _242 = _240 * _240;
   float _243 = _241 * _241;
   float _244 = 1.0f - _234;
   float _245 = _242 * _242;
   float _246 = _245 * _240;
   float _247 = _246 * _244;
   float _248 = _241 * _244;
   float _249 = _243 * _243;
   float _250 = _249 * _248;
   float _251 = _247 + _234;
   float _252 = _250 + _234;
   float _253 = dot(float3(_98, _99, _100), float3(_209, _210, _211));
   float _254 = saturate(_253);
   float _255 = dot(float3(_98, _99, _100), float3(_206, _207, _208));
   float _256 = saturate(_255);
   float _257 = _237 * _256;
   float _258 = _257 * _252;
   float _267 = c_specular_scale.x * c_mat_specular.x;
   float _268 = c_specular_scale.y * c_mat_specular.y;
   float _269 = c_specular_scale.z * c_mat_specular.z;
   float _274 = _189 * _236;
   float _275 = _274 * _228;
   float _276 = _275 * _254;
   float _277 = _276 * _251;
   float _278 = (c_light_col[0].x) * _277;
   float _279 = (c_light_col[0].y) * _277;
   float _280 = (c_light_col[0].z) * _277;
   float _285 = (c_light_col[1].x) * _258;
   float _286 = (c_light_col[1].y) * _258;
   float _287 = (c_light_col[1].z) * _258;
   float _288 = _285 + _278;
   float _289 = _286 + _279;
   float _290 = _287 + _280;
   float _291 = _267 * _288;
   float _292 = _268 * _289;
   float _293 = _269 * _290;
   int _297 = c_bool & 1;
   bool _298 = (_297 == 0);
   if (!_298)
   {
      float _300 = -0.0f - TEXCOORD_4.x;
      float _301 = -0.0f - TEXCOORD_4.y;
      float _302 = -0.0f - TEXCOORD_4.z;
      float _303 = dot(float3(_98, _99, _100), float3(_300, _301, _302));
      float _304 = saturate(_303);
      float _305 = _304 * TEXCOORD_1.w;
      float _306 = _304 * TEXCOORD_2.w;
      float _307 = _304 * TEXCOORD_4.w;
      float _308 = max(0.0f, _305);
      float _309 = max(0.0f, _306);
      float _310 = max(0.0f, _307);
      float _311 = -0.0f - TEXCOORD_5.x;
      float _312 = -0.0f - TEXCOORD_5.y;
      float _313 = -0.0f - TEXCOORD_5.z;
      float _314 = dot(float3(_98, _99, _100), float3(_311, _312, _313));
      float _315 = saturate(_314);
      float _316 = _315 * COLOR.w;
      float _317 = _315 * COLOR_1.w;
      float _318 = _315 * TEXCOORD_5.w;
      float _319 = max(0.0f, _316);
      float _320 = max(0.0f, _317);
      float _321 = max(0.0f, _318);
      float _322 = _319 + _308;
      float _323 = _320 + _309;
      float _324 = _321 + _310;
      float _325 = _107 + TEXCOORD_4.x;
      float _326 = _108 + TEXCOORD_4.y;
      float _327 = _109 + TEXCOORD_4.z;
      float _328 = dot(float3(_325, _326, _327), float3(_325, _326, _327));
      float _329 = rsqrt(_328);
      float _330 = _107 + TEXCOORD_5.x;
      float _331 = _108 + TEXCOORD_5.y;
      float _332 = _109 + TEXCOORD_5.z;
      float _333 = dot(float3(_330, _331, _332), float3(_330, _331, _332));
      float _334 = rsqrt(_333);
      float _335 = _330 * _334;
      float _336 = -0.0f - _335;
      float _337 = _331 * _334;
      float _338 = -0.0f - _337;
      float _339 = _332 * _334;
      float _340 = -0.0f - _339;
      float _341 = _325 * _329;
      float _342 = -0.0f - _341;
      float _343 = _326 * _329;
      float _344 = -0.0f - _343;
      float _345 = _327 * _329;
      float _346 = -0.0f - _345;
      float _347 = dot(float3(_342, _344, _346), float3(_98, _99, _100));
      float _348 = dot(float3(_336, _338, _340), float3(_98, _99, _100));
      float _349 = saturate(_347);
      float _350 = saturate(_348);
      float _351 = log2(_349);
      float _352 = log2(_350);
      float _353 = _351 * c_mat_power;
      float _354 = _352 * c_mat_power;
      float _355 = exp2(_353);
      float _356 = exp2(_354);
      float _357 = _355 * TEXCOORD_1.w;
      float _358 = _355 * TEXCOORD_2.w;
      float _359 = _355 * TEXCOORD_4.w;
      float _360 = _356 * COLOR.w;
      float _361 = _356 * COLOR_1.w;
      float _362 = _356 * TEXCOORD_5.w;
      float _363 = _360 + _357;
      float _364 = _361 + _358;
      float _365 = _362 + _359;
      float _366 = max(0.0f, _363);
      float _367 = max(0.0f, _364);
      float _368 = max(0.0f, _365);
      float _369 = _267 * _366;
      float _370 = _268 * _367;
      float _371 = _269 * _368;
      float _372 = min(_369, 1.0f);
      float _373 = min(_370, 1.0f);
      float _374 = min(_371, 1.0f);
      _376 = _372;
      _377 = _373;
      _378 = _374;
      _379 = _322;
      _380 = _323;
      _381 = _324;
   }
   else
   {
      _376 = 0.0f;
      _377 = 0.0f;
      _378 = 0.0f;
      _379 = 0.0f;
      _380 = 0.0f;
      _381 = 0.0f;
   }
   float _382 = c_rt_size.z * SV_Position.x;
   float _383 = c_rt_size.w * SV_Position.y;
   float4 _384 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_382, _383)));
   int _391 = c_bool & 16384;
   bool _392 = (_391 == 0);
   if (!_392)
   {
      float _394 = _384.x * 4.0f;
      float _395 = _384.y * 4.0f;
      float _396 = _384.z * 4.0f;
      float _397 = _384.w * 4.0f;
      _414 = _394;
      _415 = _395;
      _416 = _396;
      _417 = _397;
   }
   else
   {
      int _399 = c_bool & 8192;
      bool _400 = (_399 == 0);
      if (!_400)
      {
         float _402 = _384.x * 2.0f;
         float _403 = _384.y * 2.0f;
         float _404 = _384.z * 2.0f;
         float _405 = _384.w * 2.0f;
         _414 = _402;
         _415 = _403;
         _416 = _404;
         _417 = _405;
      }
      else
      {
         float _409 = _384.x / c_tone_control.z;
         float _410 = _384.y / c_tone_control.z;
         float _411 = _384.z / c_tone_control.z;
         float _412 = _384.w / c_tone_control.z;
         _414 = _409;
         _415 = _410;
         _416 = _411;
         _417 = _412;
      }
   }
   float _418 = _414 + _379;
   float _419 = _415 + _380;
   float _420 = _416 + _381;
   float _421 = _417 + _376;
   float _422 = _417 + _377;
   float _423 = _417 + _378;
   float _486 = dot(float3(_98, _99, _100), float3(_98, _99, _100));
   float _487 = rsqrt(_486);
   float _488 = _487 * _98;
   float _489 = _487 * _99;
   float _490 = _487 * _100;
   float _491 = dot(float4((c_ps_scratch[80].x), (c_ps_scratch[80].y), (c_ps_scratch[80].z), (c_ps_scratch[80].w)), float4(_488, _489, _490, 1.0f));
   float _492 = dot(float4((c_ps_scratch[81].x), (c_ps_scratch[81].y), (c_ps_scratch[81].z), (c_ps_scratch[81].w)), float4(_488, _489, _490, 1.0f));
   float _493 = dot(float4((c_ps_scratch[82].x), (c_ps_scratch[82].y), (c_ps_scratch[82].z), (c_ps_scratch[82].w)), float4(_488, _489, _490, 1.0f));
   float _494 = dot(float4((c_ps_scratch[83].x), (c_ps_scratch[83].y), (c_ps_scratch[83].z), (c_ps_scratch[83].w)), float4(_488, _489, _490, 1.0f));
   float _495 = dot(float4(_488, _489, _490, 1.0f), float4(_491, _492, _493, _494));
   float _496 = dot(float4((c_ps_scratch[84].x), (c_ps_scratch[84].y), (c_ps_scratch[84].z), (c_ps_scratch[84].w)), float4(_488, _489, _490, 1.0f));
   float _497 = dot(float4((c_ps_scratch[85].x), (c_ps_scratch[85].y), (c_ps_scratch[85].z), (c_ps_scratch[85].w)), float4(_488, _489, _490, 1.0f));
   float _498 = dot(float4((c_ps_scratch[86].x), (c_ps_scratch[86].y), (c_ps_scratch[86].z), (c_ps_scratch[86].w)), float4(_488, _489, _490, 1.0f));
   float _499 = dot(float4((c_ps_scratch[87].x), (c_ps_scratch[87].y), (c_ps_scratch[87].z), (c_ps_scratch[87].w)), float4(_488, _489, _490, 1.0f));
   float _500 = dot(float4(_488, _489, _490, 1.0f), float4(_496, _497, _498, _499));
   float _501 = dot(float4((c_ps_scratch[88].x), (c_ps_scratch[88].y), (c_ps_scratch[88].z), (c_ps_scratch[88].w)), float4(_488, _489, _490, 1.0f));
   float _502 = dot(float4((c_ps_scratch[89].x), (c_ps_scratch[89].y), (c_ps_scratch[89].z), (c_ps_scratch[89].w)), float4(_488, _489, _490, 1.0f));
   float _503 = dot(float4((c_ps_scratch[90].x), (c_ps_scratch[90].y), (c_ps_scratch[90].z), (c_ps_scratch[90].w)), float4(_488, _489, _490, 1.0f));
   float _504 = dot(float4((c_ps_scratch[91].x), (c_ps_scratch[91].y), (c_ps_scratch[91].z), (c_ps_scratch[91].w)), float4(_488, _489, _490, 1.0f));
   float _505 = dot(float4(_488, _489, _490, 1.0f), float4(_501, _502, _503, _504));
   float _506 = max(_495, 0.0625f);
   float _507 = max(_500, 0.0625f);
   float _508 = max(_505, 0.0625f);
   float _509 = _506 * (c_ps_scratch[79].x);
   float _510 = _507 * (c_ps_scratch[79].x);
   float _511 = _508 * (c_ps_scratch[79].x);
   float _516 = -0.0f - (c_light_vec[0].x);
   float _517 = -0.0f - (c_light_vec[0].y);
   float _518 = -0.0f - (c_light_vec[0].z);
   float _519 = dot(float3(_98, _99, _100), float3(_516, _517, _518));
   float _520 = -0.0f - (c_light_vec[1].x);
   float _521 = -0.0f - (c_light_vec[1].y);
   float _522 = -0.0f - (c_light_vec[1].z);
   float _523 = dot(float3(_98, _99, _100), float3(_520, _521, _522));
   float _532 = _99 * 0.5f;
   float _533 = _532 + 0.5f;
   float _534 = (c_light_col[4].x) - (c_light_vec[4].x);
   float _535 = (c_light_col[4].y) - (c_light_vec[4].y);
   float _536 = (c_light_col[4].z) - (c_light_vec[4].z);
   float _537 = _534 * _533;
   float _538 = _535 * _533;
   float _539 = _536 * _533;
   float _540 = saturate(_519);
   float _541 = saturate(_523);
   float _546 = (c_light_col[0].x) * _540;
   float _547 = (c_light_col[0].y) * _540;
   float _548 = (c_light_col[0].z) * _540;
   float _553 = (c_light_col[1].x) * _541;
   float _554 = (c_light_col[1].y) * _541;
   float _555 = (c_light_col[1].z) * _541;
   float _556 = _421 + _291;
   float _557 = _422 + _292;
   float _558 = _423 + _293;
   int _559 = c_bool & 32768;
   bool _560 = (_559 == 0);
   float _569 = (c_light_vec[4].x) + _509;
   float _570 = _569 + _537;
   float _571 = (c_light_vec[4].y) + _510;
   float _572 = _571 + _538;
   float _573 = (c_light_vec[4].z) + _511;
   float _574 = _573 + _539;
   if (!_560)
   {
      float _578 = (c_ps_user[11].x) * _157;
      float _579 = (c_ps_user[11].x) * _158;
      float _580 = (c_ps_user[11].x) * _159;
      float _582 = _98 + 1.0f;
      float _583 = _99 + 1.0f;
      float _584 = _100 + 1.0f;
      float _585 = _582 * 0.5f;
      float _586 = _583 * 0.5f;
      float _587 = _584 * 0.5f;
      float _588 = saturate(_585);
      float _589 = saturate(_586);
      float _590 = saturate(_587);
      float _591 = (c_ps_user[11].y) * _588;
      float _592 = _589 * (c_ps_user[11].y);
      float _593 = _590 * (c_ps_user[11].y);
      float _604 = (c_ps_user[11].w) * _556;
      float _607 = (c_ps_user[12].x) * _189;
      float _613 = _132 * _132;
      float _614 = _132 * 0.9722222089767456f;
      float _615 = _613 * _613;
      float _616 = _615 * _614;
      float _617 = _616 + 0.02777777425944805f;
      float _618 = (c_ps_user[13].z) * _617;
      float _620 = (c_ps_user[13].w) * _570;
      float _621 = (c_ps_user[13].w) * _572;
      float _622 = (c_ps_user[13].w) * _574;
      float _625 = (c_ps_user[14].y) * _546;
      float _626 = (c_ps_user[14].y) * _547;
      float _627 = (c_ps_user[14].y) * _548;
      float _629 = (c_ps_user[14].z) * _553;
      float _630 = (c_ps_user[14].z) * _554;
      float _631 = (c_ps_user[14].z) * _555;
      float _633 = (c_ps_user[14].w) * _418;
      float _634 = (c_ps_user[14].w) * _419;
      float _635 = (c_ps_user[14].w) * _420;
      float _640 = (c_ps_user[15].z) * _421;
      float _641 = (c_ps_user[15].z) * _422;
      float _642 = (c_ps_user[15].z) * _423;
      float _643 = (c_ps_user[15].y) + (c_ps_user[15].x);
      float _644 = c_specular_scale.x * _643;
      float _645 = _644 * _556;
      float _646 = _625 + (c_ps_user[13].x);
      float _647 = _646 + _629;
      float _648 = c_diffuse_scale.x * _647;
      float _649 = _648 * c_mat_diffuse.x;
      float _650 = (c_ps_user[13].y) + (c_ps_user[11].z);
      float _651 = _650 * c_specular_scale.x;
      float _652 = _651 + _645;
      float _653 = _652 * c_mat_specular.x;
      float _654 = _591 + _578;
      float _655 = _654 + _604;
      float _656 = _655 + (c_ps_user[12].y);
      float _657 = _656 + _607;
      float _658 = _657 + _620;
      float _659 = _658 + _618;
      float _660 = _659 + _633;
      float _661 = _660 + _640;
      float _662 = _661 + _649;
      float _663 = _662 + _653;
      float _664 = c_specular_scale.y * _643;
      float _665 = _664 * _557;
      float _666 = _626 + (c_ps_user[13].x);
      float _667 = _666 + _630;
      float _668 = c_diffuse_scale.y * _667;
      float _669 = _668 * c_mat_diffuse.y;
      float _670 = _650 * c_specular_scale.y;
      float _671 = _670 + _665;
      float _672 = _671 * c_mat_specular.y;
      float _673 = _604 + _579;
      float _674 = _673 + _592;
      float _675 = _674 + (c_ps_user[12].y);
      float _676 = _675 + _607;
      float _677 = _676 + _621;
      float _678 = _677 + _618;
      float _679 = _678 + _634;
      float _680 = _679 + _641;
      float _681 = _680 + _669;
      float _682 = _681 + _672;
      float _683 = c_specular_scale.z * _643;
      float _684 = _683 * _558;
      float _685 = _627 + (c_ps_user[13].x);
      float _686 = _685 + _631;
      float _687 = c_diffuse_scale.z * _686;
      float _688 = _687 * c_mat_diffuse.z;
      float _689 = _650 * c_specular_scale.z;
      float _690 = _689 + _684;
      float _691 = _690 * c_mat_specular.z;
      float _692 = _604 + _580;
      float _693 = _692 + _593;
      float _694 = _693 + (c_ps_user[12].y);
      float _695 = _694 + _607;
      float _696 = _695 + _622;
      float _697 = _696 + _618;
      float _698 = _697 + _635;
      float _699 = _698 + _642;
      float _700 = _699 + _688;
      float _701 = _700 + _691;
      float _702 = saturate(_663);
      float _703 = saturate(_682);
      float _704 = saturate(_701);
      _731 = _702;
      _732 = _703;
      _733 = _704;
   }
   else
   {
      float _706 = _548 * _189;
      float _707 = _574 + _420;
      float _708 = _707 + _555;
      float _709 = _708 + _706;
      float _710 = _547 * _189;
      float _711 = _572 + _419;
      float _712 = _711 + _554;
      float _713 = _712 + _710;
      float _714 = _546 * _189;
      float _715 = _570 + _418;
      float _716 = _715 + _553;
      float _717 = _716 + _714;
      float _718 = c_diffuse_scale.x * c_mat_diffuse.x;
      float _719 = _718 * _157;
      float _720 = _719 * _717;
      float _721 = c_diffuse_scale.y * c_mat_diffuse.y;
      float _722 = _721 * _158;
      float _723 = _722 * _713;
      float _724 = c_diffuse_scale.z * c_mat_diffuse.z;
      float _725 = _724 * _159;
      float _726 = _725 * _709;
      float _727 = _720 + _556;
      float _728 = _723 + _557;
      float _729 = _726 + _558;
      _731 = _727;
      _732 = _728;
      _733 = _729;
   }
   float _738 = c_mat_opacity * _50.w;
   float _739 = _738 * c_opacity_scale;
   float _740 = _739 * c_global_opacity;
   float _741 = _731 * COLOR.x;
   float _742 = _732 * COLOR.y;
   float _743 = _733 * COLOR.z;
   int _744 = c_bool & 16;
   bool _745 = (_744 == 0);
   if (!_745)
   {
      float _749 = c_tone_control.x * _741;
      float _750 = c_tone_control.x * _742;
      float _751 = c_tone_control.x * _743;
      float _753 = log2(_749);
      float _754 = log2(_750);
      float _755 = log2(_751);
      float _756 = _753 * c_tone_control.y;
      float _757 = _754 * c_tone_control.y;
      float _758 = _755 * c_tone_control.y;
      float _759 = exp2(_756);
      float _760 = exp2(_757);
      float _761 = exp2(_758);
      _809 = _759;
      _810 = _760;
      _811 = _761;
   }
   else
   {
      if (_560)
      {
         float _766 = c_tone_control.x * _741;
         float _767 = c_tone_control.x * _742;
         float _768 = c_tone_control.x * _743;
         float _769 = _766 * 2.0f;
         float _770 = _767 * 2.0f;
         float _771 = _768 * 2.0f;
         float _772 = _766 * 0.30000001192092896f;
         float _773 = _767 * 0.30000001192092896f;
         float _774 = _768 * 0.30000001192092896f;
         float _775 = _772 + 0.05000000074505806f;
         float _776 = _773 + 0.05000000074505806f;
         float _777 = _774 + 0.05000000074505806f;
         float _778 = _775 * _769;
         float _779 = _776 * _770;
         float _780 = _777 * _771;
         float _781 = _778 + 0.004000000189989805f;
         float _782 = _779 + 0.004000000189989805f;
         float _783 = _780 + 0.004000000189989805f;
         float _784 = _772 + 0.5f;
         float _785 = _773 + 0.5f;
         float _786 = _774 + 0.5f;
         float _787 = _784 * _769;
         float _788 = _785 * _770;
         float _789 = _786 * _771;
         float _790 = _787 + 0.06000000238418579f;
         float _791 = _788 + 0.06000000238418579f;
         float _792 = _789 + 0.06000000238418579f;
         float _793 = _781 / _790;
         float _794 = _782 / _791;
         float _795 = _783 / _792;
         float _796 = _793 + -0.06666666269302368f;
         float _797 = _794 + -0.06666666269302368f;
         float _798 = _795 + -0.06666666269302368f;
         float _799 = _796 * 1.3790643215179443f;
         float _800 = _797 * 1.3790643215179443f;
         float _801 = _798 * 1.3790643215179443f;
         float _802 = YKR_MaterialCurve(_799, _766);
         float _803 = YKR_MaterialCurve(_800, _767);
         float _804 = YKR_MaterialCurve(_801, _768);
         float _805 = sqrt(_802);
         float _806 = sqrt(_803);
         float _807 = sqrt(_804);
         _809 = _805;
         _810 = _806;
         _811 = _807;
      }
      else
      {
         _809 = _741;
         _810 = _742;
         _811 = _743;
      }
   }
   int _812 = c_bool & 4;
   bool _813 = (_812 == 0);
   if (_813)
   {
      float _815 = saturate(COLOR_1.x);
      float _820 = (c_fog_param[0].x) - _809;
      float _821 = (c_fog_param[0].y) - _810;
      float _822 = (c_fog_param[0].z) - _811;
      float _823 = _820 * _815;
      float _824 = _821 * _815;
      float _825 = _822 * _815;
      _827 = _823;
      _828 = _824;
      _829 = _825;
   }
   else
   {
      _827 = COLOR_1.x;
      _828 = COLOR_1.y;
      _829 = COLOR_1.z;
   }
   float _830 = _827 + _809;
   float _831 = _828 + _810;
   float _832 = _829 + _811;
   if (!_560)
   {
      float _840 = (c_ps_user[3].x) * (c_ps_user[12].z);
      float _841 = (c_ps_user[3].y) * (c_ps_user[12].z);
      float _842 = (c_ps_user[3].z) * (c_ps_user[12].z);
      float _844 = (c_ps_user[12].w) * COLOR.x;
      float _845 = (c_ps_user[12].w) * COLOR.y;
      float _846 = (c_ps_user[12].w) * COLOR.z;
      float _847 = _844 + _830;
      float _848 = _847 + _840;
      float _849 = _845 + _831;
      float _850 = _849 + _841;
      float _851 = _846 + _832;
      float _852 = _851 + _842;
      _854 = _848;
      _855 = _850;
      _856 = _852;
   }
   else
   {
      _854 = _830;
      _855 = _831;
      _856 = _832;
   }
   float _862 = (c_ps_col_scl[0].x) * _854;
   float _863 = (c_ps_col_scl[0].y) * _855;
   float _864 = (c_ps_col_scl[0].z) * _856;
   float _865 = (c_ps_col_scl[0].w) * _740;
   SV_Target.x = _862;
   SV_Target.y = _863;
   SV_Target.z = _864;
   SV_Target.w = _865;
   int _868 = c_bool & 8;
   bool _869 = (_868 != 0);
   float _870 = select(_869, _854, _862);
   float _871 = select(_869, _855, _863);
   float _872 = select(_869, _856, _864);
   float _873 = select(_869, _740, _865);
   SV_Target.x = _870;
   SV_Target.y = _871;
   SV_Target.z = _872;
   SV_Target.w = _873;
   return SV_Target;
}
