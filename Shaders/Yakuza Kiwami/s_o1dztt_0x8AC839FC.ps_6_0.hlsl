// Yakuza Kiwami material s_o1dztt (0x8AC839FC, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
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
    linear float3 TEXCOORD : TEXCOORD,
    linear float4 TEXCOORD_1 : TEXCOORD1,
    linear float4 TEXCOORD_2 : TEXCOORD2,
    linear float4 TEXCOORD_3 : TEXCOORD3,
    linear float4 TEXCOORD_4 : TEXCOORD4,
    linear float4 TEXCOORD_5 : TEXCOORD5,
    noperspective float4 SV_Position : SV_Position) : SV_Target
{
   float4 SV_Target = 0;
   float4 _55 = s2_t.Sample(s2_s, float2(TEXCOORD.x, TEXCOORD.y));
   float4 _59 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _64 = _59.x * _59.x;
   float _65 = _59.y * _59.y;
   float _66 = _59.z * _59.z;
   float4 _67 = s5_t.Sample(s5_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _72 = c_mat_power * _55.z;
   float _73 = max(1.0f, _72);
   float4 _74 = s7_t.Sample(s7_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _77 = _74.x + -0.5f;
   float _78 = _74.y + -0.5f;
   float _79 = _77 * TEXCOORD.z;
   float _80 = _78 * TEXCOORD.z;
   float _81 = _79 + 0.5f;
   float _82 = _80 + 0.5f;
   bool _83 = (_67.x < 0.5f);
   bool _84 = (_67.y < 0.5f);
   float _85 = _67.x * 2.0f;
   float _86 = _85 * _81;
   float _87 = _67.y * 2.0f;
   float _88 = _87 * _82;
   float _89 = 1.0f - _67.x;
   float _90 = 1.0f - _67.y;
   float _91 = _89 * 2.0f;
   float _92 = _90 * 2.0f;
   float _93 = 0.5f - _79;
   float _94 = 0.5f - _80;
   float _95 = _91 * _93;
   float _96 = _92 * _94;
   float _97 = 1.0f - _95;
   float _98 = 1.0f - _96;
   float _99 = select(_83, _86, _97);
   float _100 = select(_84, _88, _98);
   float _101 = _99 * 2.0f;
   float _102 = _100 * 2.0f;
   float _103 = _101 + -1.0f;
   float _104 = _102 + -1.0f;
   float _105 = dot(float2(_103, _104), float2(_103, _104));
   float _106 = 1.0f - _105;
   float _107 = saturate(_106);
   float _108 = sqrt(_107);
   float _109 = TEXCOORD_2.y * TEXCOORD_3.z;
   float _110 = TEXCOORD_2.z * TEXCOORD_3.y;
   float _111 = _109 - _110;
   float _112 = TEXCOORD_2.z * TEXCOORD_3.x;
   float _113 = TEXCOORD_2.x * TEXCOORD_3.z;
   float _114 = _112 - _113;
   float _115 = TEXCOORD_2.x * TEXCOORD_3.y;
   float _116 = TEXCOORD_2.y * TEXCOORD_3.x;
   float _117 = _115 - _116;
   float _118 = _103 * TEXCOORD_3.x;
   float _119 = _103 * TEXCOORD_3.y;
   float _120 = _103 * TEXCOORD_3.z;
   float _121 = _104 * TEXCOORD_3.w;
   float _122 = _121 * _111;
   float _123 = _121 * _114;
   float _124 = _121 * _117;
   float _125 = _108 * TEXCOORD_2.x;
   float _126 = _108 * TEXCOORD_2.y;
   float _127 = _108 * TEXCOORD_2.z;
   float _128 = _118 + _125;
   float _129 = _128 + _122;
   float _130 = _119 + _126;
   float _131 = _130 + _123;
   float _132 = _120 + _127;
   float _133 = _132 + _124;
   float _134 = dot(float3(_129, _131, _133), float3(_129, _131, _133));
   float _135 = rsqrt(_134);
   float _136 = _129 * _135;
   float _137 = _131 * _135;
   float _138 = _133 * _135;
   float _143 = dot(float3(TEXCOORD_1.x, TEXCOORD_1.y, TEXCOORD_1.z), float3(TEXCOORD_1.x, TEXCOORD_1.y, TEXCOORD_1.z));
   float _144 = rsqrt(_143);
   float _145 = _144 * TEXCOORD_1.x;
   float _146 = _144 * TEXCOORD_1.y;
   float _147 = _144 * TEXCOORD_1.z;
   float _148 = _145 + (c_light_vec[0].x);
   float _149 = _146 + (c_light_vec[0].y);
   float _150 = _147 + (c_light_vec[0].z);
   float _151 = dot(float3(_148, _149, _150), float3(_148, _149, _150));
   float _152 = rsqrt(_151);
   float _153 = _148 * _152;
   float _154 = _149 * _152;
   float _155 = _150 * _152;
   float _156 = _137 + 0.10000000149011612f;
   float _157 = saturate(_156);
   float _160 = (c_attribute_ps[2].w) + 2.0f;
   float _162 = (c_attribute_ps[2].z) + 2.5f;
   float _163 = -0.0f - _145;
   float _164 = -0.0f - _146;
   float _165 = -0.0f - _147;
   float _166 = dot(float3(_163, _164, _165), float3(TEXCOORD_2.x, TEXCOORD_2.y, TEXCOORD_2.z));
   float _167 = 1.0f - _166;
   float _168 = saturate(_167);
   float _169 = dot(float3(_163, _164, _165), float3(_136, _137, _138));
   float _170 = 1.0f - _169;
   float _171 = saturate(_170);
   float _172 = _171 * _168;
   float _173 = log2(_172);
   float _174 = _173 * _160;
   float _175 = exp2(_174);
   float _176 = _175 * _162;
   int _179 = c_bool & 4096;
   bool _180 = (_179 == 0);
   float _189;
   float _213;
   float _214;
   float _226;
   float _227;
   float _415;
   float _416;
   float _417;
   float _418;
   float _419;
   float _420;
   float _453;
   float _454;
   float _455;
   float _456;
   float _621;
   float _787;
   float _788;
   float _789;
   float _865;
   float _866;
   float _867;
   float _883;
   float _884;
   float _885;
   float _910;
   float _911;
   float _912;
   if (!_180)
   {
      float _185 = (c_ps_user[9].w) * _173;
      float _186 = exp2(_185);
      float _187 = _186 * (c_ps_user[9].z);
      _189 = _187;
   }
   else
   {
      _189 = _176;
   }
   float _190 = _157 * _189;
   float _191 = _55.y * _190;
   float _194 = _191 * (c_ps_user[2].x);
   float _195 = _194 + 1.0f;
   float _196 = _64 * _195;
   float _197 = _65 * _195;
   float _198 = _66 * _195;
   float _202 = c_rt_size.z * SV_Position.x;
   float _203 = c_rt_size.w * SV_Position.y;
   int _206 = c_bool & 8;
   bool _207 = (_206 == 0);
   if (!_207)
   {
      float4 _209 = s13_t.Sample(s13_s, float2(_202, _203));
      _213 = _209.x;
      _214 = _209.y;
   }
   else
   {
      _213 = 1.0f;
      _214 = 1.0f;
   }
   if (!_207)
   {
      float _219 = (c_sdw_param_ch[0].x) * _213;
      float _220 = (c_sdw_param_ch[0].z) * _214;
      float _223 = _219 + (c_sdw_param_ch[0].y);
      float _224 = _220 + (c_sdw_param_ch[0].w);
      _226 = _223;
      _227 = _224;
   }
   else
   {
      _226 = _213;
      _227 = _214;
   }
   float _228 = min(_226, _227);
   float _237 = (c_light_vec[1].x) + _145;
   float _238 = (c_light_vec[1].y) + _146;
   float _239 = (c_light_vec[1].z) + _147;
   float _240 = dot(float3(_237, _238, _239), float3(_237, _238, _239));
   float _241 = rsqrt(_240);
   float _242 = _237 * _241;
   float _243 = _238 * _241;
   float _244 = _239 * _241;
   float _245 = -0.0f - (c_light_vec[1].x);
   float _246 = -0.0f - (c_light_vec[1].y);
   float _247 = -0.0f - (c_light_vec[1].z);
   float _248 = -0.0f - (c_light_vec[0].x);
   float _249 = -0.0f - (c_light_vec[0].y);
   float _250 = -0.0f - (c_light_vec[0].z);
   float _251 = dot(float3(_145, _146, _147), float3(_136, _137, _138));
   float _252 = _251 * 2.0f;
   float _253 = _252 * _136;
   float _254 = _252 * _137;
   float _255 = _252 * _138;
   float _256 = _145 - _253;
   float _257 = _146 - _254;
   float _258 = _147 - _255;
   float _259 = dot(float3(_256, _257, _258), float3(_248, _249, _250));
   float _260 = dot(float3(_256, _257, _258), float3(_245, _246, _247));
   float _261 = saturate(_259);
   float _262 = saturate(_260);
   float _263 = log2(_261);
   float _264 = log2(_262);
   float _265 = _263 * _73;
   float _266 = _264 * _73;
   float _267 = exp2(_265);
   float _268 = exp2(_266);
   float _273 = c_mat_fresnel * c_tone_control.w;
   float _274 = _73 * 0.03974359855055809f;
   float _275 = _274 + 0.0856831967830658f;
   float _276 = _268 * _275;
   float _277 = dot(float3(_145, _146, _147), float3(_153, _154, _155));
   float _278 = dot(float3(_145, _146, _147), float3(_242, _243, _244));
   float _279 = 1.0f - _277;
   float _280 = 1.0f - _278;
   float _281 = _279 * _279;
   float _282 = _280 * _280;
   float _283 = 1.0f - _273;
   float _284 = _281 * _281;
   float _285 = _284 * _279;
   float _286 = _285 * _283;
   float _287 = _280 * _283;
   float _288 = _282 * _282;
   float _289 = _288 * _287;
   float _290 = _286 + _273;
   float _291 = _289 + _273;
   float _292 = dot(float3(_136, _137, _138), float3(_248, _249, _250));
   float _293 = saturate(_292);
   float _294 = dot(float3(_136, _137, _138), float3(_245, _246, _247));
   float _295 = saturate(_294);
   float _296 = _276 * _295;
   float _297 = _296 * _291;
   float _306 = c_specular_scale.x * c_mat_specular.x;
   float _307 = c_specular_scale.y * c_mat_specular.y;
   float _308 = c_specular_scale.z * c_mat_specular.z;
   float _313 = _228 * _275;
   float _314 = _313 * _267;
   float _315 = _314 * _293;
   float _316 = _315 * _290;
   float _317 = (c_light_col[0].x) * _316;
   float _318 = (c_light_col[0].y) * _316;
   float _319 = (c_light_col[0].z) * _316;
   float _324 = (c_light_col[1].x) * _297;
   float _325 = (c_light_col[1].y) * _297;
   float _326 = (c_light_col[1].z) * _297;
   float _327 = _324 + _317;
   float _328 = _325 + _318;
   float _329 = _326 + _319;
   float _330 = _306 * _327;
   float _331 = _307 * _328;
   float _332 = _308 * _329;
   int _336 = c_bool & 1;
   bool _337 = (_336 == 0);
   if (!_337)
   {
      float _339 = -0.0f - TEXCOORD_4.x;
      float _340 = -0.0f - TEXCOORD_4.y;
      float _341 = -0.0f - TEXCOORD_4.z;
      float _342 = dot(float3(_136, _137, _138), float3(_339, _340, _341));
      float _343 = saturate(_342);
      float _344 = _343 * TEXCOORD_1.w;
      float _345 = _343 * TEXCOORD_2.w;
      float _346 = _343 * TEXCOORD_4.w;
      float _347 = max(0.0f, _344);
      float _348 = max(0.0f, _345);
      float _349 = max(0.0f, _346);
      float _350 = -0.0f - TEXCOORD_5.x;
      float _351 = -0.0f - TEXCOORD_5.y;
      float _352 = -0.0f - TEXCOORD_5.z;
      float _353 = dot(float3(_136, _137, _138), float3(_350, _351, _352));
      float _354 = saturate(_353);
      float _355 = _354 * COLOR.w;
      float _356 = _354 * COLOR_1.w;
      float _357 = _354 * TEXCOORD_5.w;
      float _358 = max(0.0f, _355);
      float _359 = max(0.0f, _356);
      float _360 = max(0.0f, _357);
      float _361 = _358 + _347;
      float _362 = _359 + _348;
      float _363 = _360 + _349;
      float _364 = _145 + TEXCOORD_4.x;
      float _365 = _146 + TEXCOORD_4.y;
      float _366 = _147 + TEXCOORD_4.z;
      float _367 = dot(float3(_364, _365, _366), float3(_364, _365, _366));
      float _368 = rsqrt(_367);
      float _369 = _145 + TEXCOORD_5.x;
      float _370 = _146 + TEXCOORD_5.y;
      float _371 = _147 + TEXCOORD_5.z;
      float _372 = dot(float3(_369, _370, _371), float3(_369, _370, _371));
      float _373 = rsqrt(_372);
      float _374 = _369 * _373;
      float _375 = -0.0f - _374;
      float _376 = _370 * _373;
      float _377 = -0.0f - _376;
      float _378 = _371 * _373;
      float _379 = -0.0f - _378;
      float _380 = _364 * _368;
      float _381 = -0.0f - _380;
      float _382 = _365 * _368;
      float _383 = -0.0f - _382;
      float _384 = _366 * _368;
      float _385 = -0.0f - _384;
      float _386 = dot(float3(_381, _383, _385), float3(_136, _137, _138));
      float _387 = dot(float3(_375, _377, _379), float3(_136, _137, _138));
      float _388 = saturate(_386);
      float _389 = saturate(_387);
      float _390 = log2(_388);
      float _391 = log2(_389);
      float _392 = _390 * _73;
      float _393 = _391 * _73;
      float _394 = exp2(_392);
      float _395 = exp2(_393);
      float _396 = _394 * TEXCOORD_1.w;
      float _397 = _394 * TEXCOORD_2.w;
      float _398 = _394 * TEXCOORD_4.w;
      float _399 = _395 * COLOR.w;
      float _400 = _395 * COLOR_1.w;
      float _401 = _395 * TEXCOORD_5.w;
      float _402 = _399 + _396;
      float _403 = _400 + _397;
      float _404 = _401 + _398;
      float _405 = max(0.0f, _402);
      float _406 = max(0.0f, _403);
      float _407 = max(0.0f, _404);
      float _408 = _306 * _405;
      float _409 = _307 * _406;
      float _410 = _308 * _407;
      float _411 = min(_408, 1.0f);
      float _412 = min(_409, 1.0f);
      float _413 = min(_410, 1.0f);
      _415 = _411;
      _416 = _412;
      _417 = _413;
      _418 = _361;
      _419 = _362;
      _420 = _363;
   }
   else
   {
      _415 = 0.0f;
      _416 = 0.0f;
      _417 = 0.0f;
      _418 = 0.0f;
      _419 = 0.0f;
      _420 = 0.0f;
   }
   float _421 = c_rt_size.z * SV_Position.x;
   float _422 = c_rt_size.w * SV_Position.y;
   float4 _423 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_421, _422)));
   int _430 = c_bool & 16384;
   bool _431 = (_430 == 0);
   if (!_431)
   {
      float _433 = _423.x * 4.0f;
      float _434 = _423.y * 4.0f;
      float _435 = _423.z * 4.0f;
      float _436 = _423.w * 4.0f;
      _453 = _433;
      _454 = _434;
      _455 = _435;
      _456 = _436;
   }
   else
   {
      int _438 = c_bool & 8192;
      bool _439 = (_438 == 0);
      if (!_439)
      {
         float _441 = _423.x * 2.0f;
         float _442 = _423.y * 2.0f;
         float _443 = _423.z * 2.0f;
         float _444 = _423.w * 2.0f;
         _453 = _441;
         _454 = _442;
         _455 = _443;
         _456 = _444;
      }
      else
      {
         float _448 = _423.x / c_tone_control.z;
         float _449 = _423.y / c_tone_control.z;
         float _450 = _423.z / c_tone_control.z;
         float _451 = _423.w / c_tone_control.z;
         _453 = _448;
         _454 = _449;
         _455 = _450;
         _456 = _451;
      }
   }
   float _457 = _453 + _418;
   float _458 = _454 + _419;
   float _459 = _455 + _420;
   float _460 = _456 + _415;
   float _461 = _456 + _416;
   float _462 = _456 + _417;
   float _525 = dot(float3(_136, _137, _138), float3(_136, _137, _138));
   float _526 = rsqrt(_525);
   float _527 = _526 * _136;
   float _528 = _526 * _137;
   float _529 = _526 * _138;
   float _530 = dot(float4((c_ps_scratch[80].x), (c_ps_scratch[80].y), (c_ps_scratch[80].z), (c_ps_scratch[80].w)), float4(_527, _528, _529, 1.0f));
   float _531 = dot(float4((c_ps_scratch[81].x), (c_ps_scratch[81].y), (c_ps_scratch[81].z), (c_ps_scratch[81].w)), float4(_527, _528, _529, 1.0f));
   float _532 = dot(float4((c_ps_scratch[82].x), (c_ps_scratch[82].y), (c_ps_scratch[82].z), (c_ps_scratch[82].w)), float4(_527, _528, _529, 1.0f));
   float _533 = dot(float4((c_ps_scratch[83].x), (c_ps_scratch[83].y), (c_ps_scratch[83].z), (c_ps_scratch[83].w)), float4(_527, _528, _529, 1.0f));
   float _534 = dot(float4(_527, _528, _529, 1.0f), float4(_530, _531, _532, _533));
   float _535 = dot(float4((c_ps_scratch[84].x), (c_ps_scratch[84].y), (c_ps_scratch[84].z), (c_ps_scratch[84].w)), float4(_527, _528, _529, 1.0f));
   float _536 = dot(float4((c_ps_scratch[85].x), (c_ps_scratch[85].y), (c_ps_scratch[85].z), (c_ps_scratch[85].w)), float4(_527, _528, _529, 1.0f));
   float _537 = dot(float4((c_ps_scratch[86].x), (c_ps_scratch[86].y), (c_ps_scratch[86].z), (c_ps_scratch[86].w)), float4(_527, _528, _529, 1.0f));
   float _538 = dot(float4((c_ps_scratch[87].x), (c_ps_scratch[87].y), (c_ps_scratch[87].z), (c_ps_scratch[87].w)), float4(_527, _528, _529, 1.0f));
   float _539 = dot(float4(_527, _528, _529, 1.0f), float4(_535, _536, _537, _538));
   float _540 = dot(float4((c_ps_scratch[88].x), (c_ps_scratch[88].y), (c_ps_scratch[88].z), (c_ps_scratch[88].w)), float4(_527, _528, _529, 1.0f));
   float _541 = dot(float4((c_ps_scratch[89].x), (c_ps_scratch[89].y), (c_ps_scratch[89].z), (c_ps_scratch[89].w)), float4(_527, _528, _529, 1.0f));
   float _542 = dot(float4((c_ps_scratch[90].x), (c_ps_scratch[90].y), (c_ps_scratch[90].z), (c_ps_scratch[90].w)), float4(_527, _528, _529, 1.0f));
   float _543 = dot(float4((c_ps_scratch[91].x), (c_ps_scratch[91].y), (c_ps_scratch[91].z), (c_ps_scratch[91].w)), float4(_527, _528, _529, 1.0f));
   float _544 = dot(float4(_527, _528, _529, 1.0f), float4(_540, _541, _542, _543));
   float _545 = max(_534, 0.0625f);
   float _546 = max(_539, 0.0625f);
   float _547 = max(_544, 0.0625f);
   float _548 = _545 * (c_ps_scratch[79].x);
   float _549 = _546 * (c_ps_scratch[79].x);
   float _550 = _547 * (c_ps_scratch[79].x);
   float _555 = -0.0f - (c_light_vec[0].x);
   float _556 = -0.0f - (c_light_vec[0].y);
   float _557 = -0.0f - (c_light_vec[0].z);
   float _558 = dot(float3(_136, _137, _138), float3(_555, _556, _557));
   float _559 = -0.0f - (c_light_vec[1].x);
   float _560 = -0.0f - (c_light_vec[1].y);
   float _561 = -0.0f - (c_light_vec[1].z);
   float _562 = dot(float3(_136, _137, _138), float3(_559, _560, _561));
   float _571 = _137 * 0.5f;
   float _572 = _571 + 0.5f;
   float _573 = (c_light_col[4].x) - (c_light_vec[4].x);
   float _574 = (c_light_col[4].y) - (c_light_vec[4].y);
   float _575 = (c_light_col[4].z) - (c_light_vec[4].z);
   float _576 = _573 * _572;
   float _577 = _574 * _572;
   float _578 = _575 * _572;
   float _579 = saturate(_558);
   float _580 = saturate(_562);
   float _585 = (c_light_col[0].x) * _579;
   float _586 = (c_light_col[0].y) * _579;
   float _587 = (c_light_col[0].z) * _579;
   float _592 = (c_light_col[1].x) * _580;
   float _593 = (c_light_col[1].y) * _580;
   float _594 = (c_light_col[1].z) * _580;
   float _595 = _585 * _228;
   float _596 = _586 * _228;
   float _597 = _587 * _228;
   float _598 = (c_light_vec[4].x) + _548;
   float _599 = _598 + _576;
   float _600 = _599 + _457;
   float _601 = _600 + _592;
   float _602 = _601 + _595;
   float _603 = (c_light_vec[4].y) + _549;
   float _604 = _603 + _577;
   float _605 = _604 + _458;
   float _606 = _605 + _593;
   float _607 = _606 + _596;
   float _608 = (c_light_vec[4].z) + _550;
   float _609 = _608 + _578;
   float _610 = _609 + _459;
   float _611 = _610 + _594;
   float _612 = _611 + _597;
   int _613 = c_bool & 64;
   bool _614 = (_613 == 0);
   if (!_614)
   {
      float _616 = 1.0f - _55.y;
      float _617 = _228 * _616;
      float _618 = _617 * _579;
      float _619 = _618 + _55.y;
      _621 = _619;
   }
   else
   {
      _621 = _55.y;
   }
   float _622 = _460 + _330;
   float _623 = _461 + _331;
   float _624 = _462 + _332;
   int _625 = c_bool & 32768;
   bool _626 = (_625 == 0);
   if (!_626)
   {
      float _638 = (c_ps_user[11].x) * _196;
      float _639 = (c_ps_user[11].x) * _197;
      float _640 = (c_ps_user[11].x) * _198;
      float _642 = _136 + 1.0f;
      float _643 = _137 + 1.0f;
      float _644 = _138 + 1.0f;
      float _645 = _642 * 0.5f;
      float _646 = _643 * 0.5f;
      float _647 = _644 * 0.5f;
      float _648 = saturate(_645);
      float _649 = saturate(_646);
      float _650 = saturate(_647);
      float _651 = (c_ps_user[11].y) * _648;
      float _652 = _649 * (c_ps_user[11].y);
      float _653 = _650 * (c_ps_user[11].y);
      float _663 = (c_ps_user[11].z) * _55.x;
      float _665 = (c_ps_user[11].w) * _622;
      float _668 = (c_ps_user[12].x) * _228;
      float _670 = (c_ps_user[12].y) * _55.y;
      float _675 = _170 * _170;
      float _676 = _170 * 0.9722222089767456f;
      float _677 = _675 * _675;
      float _678 = _677 * _676;
      float _679 = _678 + 0.02777777425944805f;
      float _680 = (c_ps_user[13].z) * _679;
      float _682 = (c_ps_user[13].w) * _599;
      float _683 = (c_ps_user[13].w) * _604;
      float _684 = (c_ps_user[13].w) * _609;
      float _687 = (c_ps_user[14].y) * _585;
      float _688 = (c_ps_user[14].y) * _586;
      float _689 = (c_ps_user[14].y) * _587;
      float _691 = (c_ps_user[14].z) * _592;
      float _692 = (c_ps_user[14].z) * _593;
      float _693 = (c_ps_user[14].z) * _594;
      float _695 = (c_ps_user[14].w) * _457;
      float _696 = (c_ps_user[14].w) * _458;
      float _697 = (c_ps_user[14].w) * _459;
      float _702 = (c_ps_user[15].z) * _460;
      float _703 = (c_ps_user[15].z) * _461;
      float _704 = (c_ps_user[15].z) * _462;
      float _705 = (c_ps_user[15].y) + (c_ps_user[15].x);
      float _706 = c_specular_scale.x * _705;
      float _707 = _706 * _622;
      float _708 = _687 + (c_ps_user[13].x);
      float _709 = _708 + _691;
      float _710 = c_diffuse_scale.x * _709;
      float _711 = _710 * c_mat_diffuse.x;
      float _712 = (c_ps_user[13].y) + _663;
      float _713 = _712 * c_specular_scale.x;
      float _714 = _713 + _707;
      float _715 = _714 * c_mat_specular.x;
      float _716 = _651 + _638;
      float _717 = _716 + _665;
      float _718 = _717 + _668;
      float _719 = _718 + _670;
      float _720 = _719 + _682;
      float _721 = _720 + _680;
      float _722 = _721 + _695;
      float _723 = _722 + _702;
      float _724 = _723 + _711;
      float _725 = _724 + _715;
      float _726 = c_specular_scale.y * _705;
      float _727 = _726 * _623;
      float _728 = _688 + (c_ps_user[13].x);
      float _729 = _728 + _692;
      float _730 = c_diffuse_scale.y * _729;
      float _731 = _730 * c_mat_diffuse.y;
      float _732 = _712 * c_specular_scale.y;
      float _733 = _732 + _727;
      float _734 = _733 * c_mat_specular.y;
      float _735 = _665 + _639;
      float _736 = _735 + _652;
      float _737 = _736 + _668;
      float _738 = _737 + _670;
      float _739 = _738 + _683;
      float _740 = _739 + _680;
      float _741 = _740 + _696;
      float _742 = _741 + _703;
      float _743 = _742 + _731;
      float _744 = _743 + _734;
      float _745 = c_specular_scale.z * _705;
      float _746 = _745 * _624;
      float _747 = _689 + (c_ps_user[13].x);
      float _748 = _747 + _693;
      float _749 = c_diffuse_scale.z * _748;
      float _750 = _749 * c_mat_diffuse.z;
      float _751 = _712 * c_specular_scale.z;
      float _752 = _751 + _746;
      float _753 = _752 * c_mat_specular.z;
      float _754 = _665 + _640;
      float _755 = _754 + _653;
      float _756 = _755 + _668;
      float _757 = _756 + _670;
      float _758 = _757 + _684;
      float _759 = _758 + _680;
      float _760 = _759 + _697;
      float _761 = _760 + _704;
      float _762 = _761 + _750;
      float _763 = _762 + _753;
      float _764 = saturate(_725);
      float _765 = saturate(_744);
      float _766 = saturate(_763);
      _787 = _764;
      _788 = _765;
      _789 = _766;
   }
   else
   {
      float _768 = c_diffuse_scale.x * c_mat_diffuse.x;
      float _769 = _768 * _196;
      float _770 = _769 * _602;
      float _771 = _770 * _621;
      float _772 = c_diffuse_scale.y * c_mat_diffuse.y;
      float _773 = _772 * _197;
      float _774 = _773 * _607;
      float _775 = _774 * _621;
      float _776 = c_diffuse_scale.z * c_mat_diffuse.z;
      float _777 = _776 * _198;
      float _778 = _777 * _612;
      float _779 = _778 * _621;
      float _780 = _622 * _55.x;
      float _781 = _623 * _55.x;
      float _782 = _624 * _55.x;
      float _783 = _771 + _780;
      float _784 = _775 + _781;
      float _785 = _779 + _782;
      _787 = _783;
      _788 = _784;
      _789 = _785;
   }
   float _794 = c_mat_opacity * _59.w;
   float _795 = _794 * c_opacity_scale;
   float _796 = _795 * c_global_opacity;
   float _797 = _787 * COLOR.x;
   float _798 = _788 * COLOR.y;
   float _799 = _789 * COLOR.z;
   int _800 = c_bool & 16;
   bool _801 = (_800 == 0);
   if (!_801)
   {
      float _805 = c_tone_control.x * _797;
      float _806 = c_tone_control.x * _798;
      float _807 = c_tone_control.x * _799;
      float _809 = log2(_805);
      float _810 = log2(_806);
      float _811 = log2(_807);
      float _812 = _809 * c_tone_control.y;
      float _813 = _810 * c_tone_control.y;
      float _814 = _811 * c_tone_control.y;
      float _815 = exp2(_812);
      float _816 = exp2(_813);
      float _817 = exp2(_814);
      _865 = _815;
      _866 = _816;
      _867 = _817;
   }
   else
   {
      if (_626)
      {
         float _822 = c_tone_control.x * _797;
         float _823 = c_tone_control.x * _798;
         float _824 = c_tone_control.x * _799;
         float _825 = _822 * 2.0f;
         float _826 = _823 * 2.0f;
         float _827 = _824 * 2.0f;
         float _828 = _822 * 0.30000001192092896f;
         float _829 = _823 * 0.30000001192092896f;
         float _830 = _824 * 0.30000001192092896f;
         float _831 = _828 + 0.05000000074505806f;
         float _832 = _829 + 0.05000000074505806f;
         float _833 = _830 + 0.05000000074505806f;
         float _834 = _831 * _825;
         float _835 = _832 * _826;
         float _836 = _833 * _827;
         float _837 = _834 + 0.004000000189989805f;
         float _838 = _835 + 0.004000000189989805f;
         float _839 = _836 + 0.004000000189989805f;
         float _840 = _828 + 0.5f;
         float _841 = _829 + 0.5f;
         float _842 = _830 + 0.5f;
         float _843 = _840 * _825;
         float _844 = _841 * _826;
         float _845 = _842 * _827;
         float _846 = _843 + 0.06000000238418579f;
         float _847 = _844 + 0.06000000238418579f;
         float _848 = _845 + 0.06000000238418579f;
         float _849 = _837 / _846;
         float _850 = _838 / _847;
         float _851 = _839 / _848;
         float _852 = _849 + -0.06666666269302368f;
         float _853 = _850 + -0.06666666269302368f;
         float _854 = _851 + -0.06666666269302368f;
         float _855 = _852 * 1.3790643215179443f;
         float _856 = _853 * 1.3790643215179443f;
         float _857 = _854 * 1.3790643215179443f;
         float _858 = YKR_MaterialCurve(_855, _822);
         float _859 = YKR_MaterialCurve(_856, _823);
         float _860 = YKR_MaterialCurve(_857, _824);
         float _861 = sqrt(_858);
         float _862 = sqrt(_859);
         float _863 = sqrt(_860);
         _865 = _861;
         _866 = _862;
         _867 = _863;
      }
      else
      {
         _865 = _797;
         _866 = _798;
         _867 = _799;
      }
   }
   int _868 = c_bool & 4;
   bool _869 = (_868 == 0);
   if (_869)
   {
      float _871 = saturate(COLOR_1.x);
      float _876 = (c_fog_param[0].x) - _865;
      float _877 = (c_fog_param[0].y) - _866;
      float _878 = (c_fog_param[0].z) - _867;
      float _879 = _876 * _871;
      float _880 = _877 * _871;
      float _881 = _878 * _871;
      _883 = _879;
      _884 = _880;
      _885 = _881;
   }
   else
   {
      _883 = COLOR_1.x;
      _884 = COLOR_1.y;
      _885 = COLOR_1.z;
   }
   float _886 = _883 + _865;
   float _887 = _884 + _866;
   float _888 = _885 + _867;
   if (!_626)
   {
      float _896 = (c_ps_user[3].x) * (c_ps_user[12].z);
      float _897 = (c_ps_user[3].y) * (c_ps_user[12].z);
      float _898 = (c_ps_user[3].z) * (c_ps_user[12].z);
      float _900 = (c_ps_user[12].w) * COLOR.x;
      float _901 = (c_ps_user[12].w) * COLOR.y;
      float _902 = (c_ps_user[12].w) * COLOR.z;
      float _903 = _900 + _886;
      float _904 = _903 + _896;
      float _905 = _901 + _887;
      float _906 = _905 + _897;
      float _907 = _902 + _888;
      float _908 = _907 + _898;
      _910 = _904;
      _911 = _906;
      _912 = _908;
   }
   else
   {
      _910 = _886;
      _911 = _887;
      _912 = _888;
   }
   float _918 = (c_ps_col_scl[0].x) * _910;
   float _919 = (c_ps_col_scl[0].y) * _911;
   float _920 = (c_ps_col_scl[0].z) * _912;
   float _921 = (c_ps_col_scl[0].w) * _796;
   SV_Target.x = _918;
   SV_Target.y = _919;
   SV_Target.z = _920;
   SV_Target.w = _921;
   int _924 = c_bool & 8;
   bool _925 = (_924 != 0);
   float _926 = select(_925, _910, _918);
   float _927 = select(_925, _911, _919);
   float _928 = select(_925, _912, _920);
   float _929 = select(_925, _796, _921);
   SV_Target.x = _926;
   SV_Target.y = _927;
   SV_Target.z = _928;
   SV_Target.w = _929;
   return SV_Target;
}
