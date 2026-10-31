// Yakuza Kiwami material s_o1dszt[rd] (0xB2AC46D5, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
// applied by a script: the inline Hable tonemap's saturate goes through
// YKR_MaterialCurve (tangent continuation in HDR), and the light pre-pass buffer t14 through YKR_LightBuffer.

#include "Includes/Common.hlsl"

Texture2D<float4> s0_t : register(t0);

Texture2D<float4> s2_t : register(t2);

Texture2D<float4> s3_t : register(t3);

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

SamplerState s3_s : register(s3);

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
   float4 _58 = s2_t.Sample(s2_s, float2(TEXCOORD.x, TEXCOORD.y));
   float4 _61 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _66 = _61.x * _61.x;
   float _67 = _61.y * _61.y;
   float _68 = _61.z * _61.z;
   float4 _69 = s3_t.Sample(s3_s, float2(TEXCOORD.x, TEXCOORD.y));
   float4 _73 = s5_t.Sample(s5_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _78 = _73.x * 2.0f;
   float _79 = _73.y * 2.0f;
   float _80 = _78 + -1.0f;
   float _81 = _79 + -1.0f;
   float _82 = dot(float2(_80, _81), float2(_80, _81));
   float _83 = 1.0f - _82;
   float _84 = saturate(_83);
   float _85 = sqrt(_84);
   float _86 = TEXCOORD_2.y * TEXCOORD_3.z;
   float _87 = TEXCOORD_2.z * TEXCOORD_3.y;
   float _88 = _86 - _87;
   float _89 = TEXCOORD_2.z * TEXCOORD_3.x;
   float _90 = TEXCOORD_2.x * TEXCOORD_3.z;
   float _91 = _89 - _90;
   float _92 = TEXCOORD_2.x * TEXCOORD_3.y;
   float _93 = TEXCOORD_2.y * TEXCOORD_3.x;
   float _94 = _92 - _93;
   float _95 = _80 * TEXCOORD_3.x;
   float _96 = _80 * TEXCOORD_3.y;
   float _97 = _80 * TEXCOORD_3.z;
   float _98 = _81 * TEXCOORD_3.w;
   float _99 = _98 * _88;
   float _100 = _98 * _91;
   float _101 = _98 * _94;
   float _102 = _99 + _95;
   float _103 = _100 + _96;
   float _104 = _101 + _97;
   float _105 = _85 * TEXCOORD_2.x;
   float _106 = _85 * TEXCOORD_2.y;
   float _107 = _85 * TEXCOORD_2.z;
   float _108 = _102 + _105;
   float _109 = _103 + _106;
   float _110 = _104 + _107;
   float _111 = dot(float3(_108, _109, _110), float3(_108, _109, _110));
   float _112 = rsqrt(_111);
   float _113 = _108 * _112;
   float _114 = _109 * _112;
   float _115 = _110 * _112;
   float _120 = dot(float3(TEXCOORD_1.x, TEXCOORD_1.y, TEXCOORD_1.z), float3(TEXCOORD_1.x, TEXCOORD_1.y, TEXCOORD_1.z));
   float _121 = rsqrt(_120);
   float _122 = _121 * TEXCOORD_1.x;
   float _123 = _121 * TEXCOORD_1.y;
   float _124 = _121 * TEXCOORD_1.z;
   float _125 = _122 + (c_light_vec[0].x);
   float _126 = _123 + (c_light_vec[0].y);
   float _127 = _124 + (c_light_vec[0].z);
   float _128 = dot(float3(_125, _126, _127), float3(_125, _126, _127));
   float _129 = rsqrt(_128);
   float _130 = _125 * _129;
   float _131 = _126 * _129;
   float _132 = _127 * _129;
   float4 _133 = s7_t.Sample(s7_s, float2(TEXCOORD.z, TEXCOORD.w));
   float _138 = _133.x * _133.x;
   float _139 = _133.y * _133.y;
   float _140 = _133.z * _133.z;
   float _141 = 1.0f - _138;
   float _142 = 1.0f - _139;
   float _143 = 1.0f - _140;
   float _144 = 1.0f - _58.z;
   float _145 = 1.0f - _133.w;
   float _146 = _145 * _144;
   float _147 = _146 * _141;
   float _148 = _146 * _142;
   float _149 = _146 * _143;
   float _150 = _147 + _138;
   float _151 = _148 + _139;
   float _152 = _149 + _140;
   float _153 = _66 * _150;
   float _154 = _67 * _151;
   float _155 = _68 * _152;
   float _156 = _114 + 0.10000000149011612f;
   float _157 = saturate(_156);
   float _160 = (c_attribute_ps[2].w) + 2.0f;
   float _162 = (c_attribute_ps[2].z) + 2.5f;
   float _163 = -0.0f - _122;
   float _164 = -0.0f - _123;
   float _165 = -0.0f - _124;
   float _166 = dot(float3(_163, _164, _165), float3(TEXCOORD_2.x, TEXCOORD_2.y, TEXCOORD_2.z));
   float _167 = 1.0f - _166;
   float _168 = saturate(_167);
   float _169 = dot(float3(_163, _164, _165), float3(_113, _114, _115));
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
   float _791;
   float _792;
   float _793;
   float _869;
   float _870;
   float _871;
   float _887;
   float _888;
   float _889;
   float _914;
   float _915;
   float _916;
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
   float _191 = _58.y * _190;
   float _194 = _191 * (c_ps_user[2].x);
   float _195 = _194 + 1.0f;
   float _196 = _153 * _195;
   float _197 = _154 * _195;
   float _198 = _155 * _195;
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
   float _237 = (c_light_vec[1].x) + _122;
   float _238 = (c_light_vec[1].y) + _123;
   float _239 = (c_light_vec[1].z) + _124;
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
   float _251 = dot(float3(_122, _123, _124), float3(_113, _114, _115));
   float _252 = _251 * 2.0f;
   float _253 = _252 * _113;
   float _254 = _252 * _114;
   float _255 = _252 * _115;
   float _256 = _122 - _253;
   float _257 = _123 - _254;
   float _258 = _124 - _255;
   float _259 = dot(float3(_256, _257, _258), float3(_248, _249, _250));
   float _260 = dot(float3(_256, _257, _258), float3(_245, _246, _247));
   float _261 = saturate(_259);
   float _262 = saturate(_260);
   float _263 = log2(_261);
   float _264 = log2(_262);
   float _265 = _263 * c_mat_power;
   float _266 = _264 * c_mat_power;
   float _267 = exp2(_265);
   float _268 = exp2(_266);
   float _273 = c_mat_fresnel * c_tone_control.w;
   float _274 = c_mat_power * 0.03974359855055809f;
   float _275 = _274 + 0.0856831967830658f;
   float _276 = _268 * _275;
   float _277 = dot(float3(_122, _123, _124), float3(_130, _131, _132));
   float _278 = dot(float3(_122, _123, _124), float3(_242, _243, _244));
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
   float _292 = dot(float3(_113, _114, _115), float3(_248, _249, _250));
   float _293 = saturate(_292);
   float _294 = dot(float3(_113, _114, _115), float3(_245, _246, _247));
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
      float _342 = dot(float3(_113, _114, _115), float3(_339, _340, _341));
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
      float _353 = dot(float3(_113, _114, _115), float3(_350, _351, _352));
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
      float _364 = _122 + TEXCOORD_4.x;
      float _365 = _123 + TEXCOORD_4.y;
      float _366 = _124 + TEXCOORD_4.z;
      float _367 = dot(float3(_364, _365, _366), float3(_364, _365, _366));
      float _368 = rsqrt(_367);
      float _369 = _122 + TEXCOORD_5.x;
      float _370 = _123 + TEXCOORD_5.y;
      float _371 = _124 + TEXCOORD_5.z;
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
      float _386 = dot(float3(_381, _383, _385), float3(_113, _114, _115));
      float _387 = dot(float3(_375, _377, _379), float3(_113, _114, _115));
      float _388 = saturate(_386);
      float _389 = saturate(_387);
      float _390 = log2(_388);
      float _391 = log2(_389);
      float _392 = _390 * c_mat_power;
      float _393 = _391 * c_mat_power;
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
   float _525 = dot(float3(_113, _114, _115), float3(_113, _114, _115));
   float _526 = rsqrt(_525);
   float _527 = _526 * _113;
   float _528 = _526 * _114;
   float _529 = _526 * _115;
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
   float _558 = dot(float3(_113, _114, _115), float3(_555, _556, _557));
   float _559 = -0.0f - (c_light_vec[1].x);
   float _560 = -0.0f - (c_light_vec[1].y);
   float _561 = -0.0f - (c_light_vec[1].z);
   float _562 = dot(float3(_113, _114, _115), float3(_559, _560, _561));
   float _571 = _114 * 0.5f;
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
      float _616 = 1.0f - _58.y;
      float _617 = _228 * _616;
      float _618 = _617 * _579;
      float _619 = _618 + _58.y;
      _621 = _619;
   }
   else
   {
      _621 = _58.y;
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
      float _642 = _113 + 1.0f;
      float _643 = _114 + 1.0f;
      float _644 = _115 + 1.0f;
      float _645 = _642 * 0.5f;
      float _646 = _643 * 0.5f;
      float _647 = _644 * 0.5f;
      float _648 = saturate(_645);
      float _649 = saturate(_646);
      float _650 = saturate(_647);
      float _651 = (c_ps_user[11].y) * _648;
      float _652 = _649 * (c_ps_user[11].y);
      float _653 = _650 * (c_ps_user[11].y);
      float _663 = (c_ps_user[11].z) * _69.x;
      float _664 = (c_ps_user[11].z) * _69.y;
      float _665 = (c_ps_user[11].z) * _69.z;
      float _667 = (c_ps_user[11].w) * _622;
      float _670 = (c_ps_user[12].x) * _228;
      float _672 = (c_ps_user[12].y) * _58.y;
      float _677 = _170 * _170;
      float _678 = _170 * 0.9722222089767456f;
      float _679 = _677 * _677;
      float _680 = _679 * _678;
      float _681 = _680 + 0.02777777425944805f;
      float _682 = (c_ps_user[13].z) * _681;
      float _684 = (c_ps_user[13].w) * _599;
      float _685 = (c_ps_user[13].w) * _604;
      float _686 = (c_ps_user[13].w) * _609;
      float _689 = (c_ps_user[14].y) * _585;
      float _690 = (c_ps_user[14].y) * _586;
      float _691 = (c_ps_user[14].y) * _587;
      float _693 = (c_ps_user[14].z) * _592;
      float _694 = (c_ps_user[14].z) * _593;
      float _695 = (c_ps_user[14].z) * _594;
      float _697 = (c_ps_user[14].w) * _457;
      float _698 = (c_ps_user[14].w) * _458;
      float _699 = (c_ps_user[14].w) * _459;
      float _704 = (c_ps_user[15].z) * _460;
      float _705 = (c_ps_user[15].z) * _461;
      float _706 = (c_ps_user[15].z) * _462;
      float _707 = (c_ps_user[15].y) + (c_ps_user[15].x);
      float _708 = c_specular_scale.x * _707;
      float _709 = _708 * _622;
      float _710 = _689 + (c_ps_user[13].x);
      float _711 = _710 + _693;
      float _712 = c_diffuse_scale.x * _711;
      float _713 = _712 * c_mat_diffuse.x;
      float _714 = (c_ps_user[13].y) + _663;
      float _715 = _714 * c_specular_scale.x;
      float _716 = _715 + _709;
      float _717 = _716 * c_mat_specular.x;
      float _718 = _651 + _638;
      float _719 = _718 + _667;
      float _720 = _719 + _670;
      float _721 = _720 + _672;
      float _722 = _721 + _684;
      float _723 = _722 + _682;
      float _724 = _723 + _697;
      float _725 = _724 + _704;
      float _726 = _725 + _713;
      float _727 = _726 + _717;
      float _728 = c_specular_scale.y * _707;
      float _729 = _728 * _623;
      float _730 = _690 + (c_ps_user[13].x);
      float _731 = _730 + _694;
      float _732 = c_diffuse_scale.y * _731;
      float _733 = _732 * c_mat_diffuse.y;
      float _734 = (c_ps_user[13].y) + _664;
      float _735 = _734 * c_specular_scale.y;
      float _736 = _735 + _729;
      float _737 = _736 * c_mat_specular.y;
      float _738 = _667 + _639;
      float _739 = _738 + _652;
      float _740 = _739 + _670;
      float _741 = _740 + _672;
      float _742 = _741 + _685;
      float _743 = _742 + _682;
      float _744 = _743 + _698;
      float _745 = _744 + _705;
      float _746 = _745 + _733;
      float _747 = _746 + _737;
      float _748 = c_specular_scale.z * _707;
      float _749 = _748 * _624;
      float _750 = _691 + (c_ps_user[13].x);
      float _751 = _750 + _695;
      float _752 = c_diffuse_scale.z * _751;
      float _753 = _752 * c_mat_diffuse.z;
      float _754 = (c_ps_user[13].y) + _665;
      float _755 = _754 * c_specular_scale.z;
      float _756 = _755 + _749;
      float _757 = _756 * c_mat_specular.z;
      float _758 = _667 + _640;
      float _759 = _758 + _653;
      float _760 = _759 + _670;
      float _761 = _760 + _672;
      float _762 = _761 + _686;
      float _763 = _762 + _682;
      float _764 = _763 + _699;
      float _765 = _764 + _706;
      float _766 = _765 + _753;
      float _767 = _766 + _757;
      float _768 = saturate(_727);
      float _769 = saturate(_747);
      float _770 = saturate(_767);
      _791 = _768;
      _792 = _769;
      _793 = _770;
   }
   else
   {
      float _772 = c_diffuse_scale.x * c_mat_diffuse.x;
      float _773 = _772 * _196;
      float _774 = _773 * _602;
      float _775 = _774 * _621;
      float _776 = c_diffuse_scale.y * c_mat_diffuse.y;
      float _777 = _776 * _197;
      float _778 = _777 * _607;
      float _779 = _778 * _621;
      float _780 = c_diffuse_scale.z * c_mat_diffuse.z;
      float _781 = _780 * _198;
      float _782 = _781 * _612;
      float _783 = _782 * _621;
      float _784 = _622 * _69.x;
      float _785 = _623 * _69.y;
      float _786 = _624 * _69.z;
      float _787 = _775 + _784;
      float _788 = _779 + _785;
      float _789 = _783 + _786;
      _791 = _787;
      _792 = _788;
      _793 = _789;
   }
   float _798 = c_mat_opacity * _61.w;
   float _799 = _798 * c_opacity_scale;
   float _800 = _799 * c_global_opacity;
   float _801 = _791 * COLOR.x;
   float _802 = _792 * COLOR.y;
   float _803 = _793 * COLOR.z;
   int _804 = c_bool & 16;
   bool _805 = (_804 == 0);
   if (!_805)
   {
      float _809 = c_tone_control.x * _801;
      float _810 = c_tone_control.x * _802;
      float _811 = c_tone_control.x * _803;
      float _813 = log2(_809);
      float _814 = log2(_810);
      float _815 = log2(_811);
      float _816 = _813 * c_tone_control.y;
      float _817 = _814 * c_tone_control.y;
      float _818 = _815 * c_tone_control.y;
      float _819 = exp2(_816);
      float _820 = exp2(_817);
      float _821 = exp2(_818);
      _869 = _819;
      _870 = _820;
      _871 = _821;
   }
   else
   {
      if (_626)
      {
         float _826 = c_tone_control.x * _801;
         float _827 = c_tone_control.x * _802;
         float _828 = c_tone_control.x * _803;
         float _829 = _826 * 2.0f;
         float _830 = _827 * 2.0f;
         float _831 = _828 * 2.0f;
         float _832 = _826 * 0.30000001192092896f;
         float _833 = _827 * 0.30000001192092896f;
         float _834 = _828 * 0.30000001192092896f;
         float _835 = _832 + 0.05000000074505806f;
         float _836 = _833 + 0.05000000074505806f;
         float _837 = _834 + 0.05000000074505806f;
         float _838 = _835 * _829;
         float _839 = _836 * _830;
         float _840 = _837 * _831;
         float _841 = _838 + 0.004000000189989805f;
         float _842 = _839 + 0.004000000189989805f;
         float _843 = _840 + 0.004000000189989805f;
         float _844 = _832 + 0.5f;
         float _845 = _833 + 0.5f;
         float _846 = _834 + 0.5f;
         float _847 = _844 * _829;
         float _848 = _845 * _830;
         float _849 = _846 * _831;
         float _850 = _847 + 0.06000000238418579f;
         float _851 = _848 + 0.06000000238418579f;
         float _852 = _849 + 0.06000000238418579f;
         float _853 = _841 / _850;
         float _854 = _842 / _851;
         float _855 = _843 / _852;
         float _856 = _853 + -0.06666666269302368f;
         float _857 = _854 + -0.06666666269302368f;
         float _858 = _855 + -0.06666666269302368f;
         float _859 = _856 * 1.3790643215179443f;
         float _860 = _857 * 1.3790643215179443f;
         float _861 = _858 * 1.3790643215179443f;
         float _862 = YKR_MaterialCurve(_859, _826);
         float _863 = YKR_MaterialCurve(_860, _827);
         float _864 = YKR_MaterialCurve(_861, _828);
         float _865 = sqrt(_862);
         float _866 = sqrt(_863);
         float _867 = sqrt(_864);
         _869 = _865;
         _870 = _866;
         _871 = _867;
      }
      else
      {
         _869 = _801;
         _870 = _802;
         _871 = _803;
      }
   }
   int _872 = c_bool & 4;
   bool _873 = (_872 == 0);
   if (_873)
   {
      float _875 = saturate(COLOR_1.x);
      float _880 = (c_fog_param[0].x) - _869;
      float _881 = (c_fog_param[0].y) - _870;
      float _882 = (c_fog_param[0].z) - _871;
      float _883 = _880 * _875;
      float _884 = _881 * _875;
      float _885 = _882 * _875;
      _887 = _883;
      _888 = _884;
      _889 = _885;
   }
   else
   {
      _887 = COLOR_1.x;
      _888 = COLOR_1.y;
      _889 = COLOR_1.z;
   }
   float _890 = _887 + _869;
   float _891 = _888 + _870;
   float _892 = _889 + _871;
   if (!_626)
   {
      float _900 = (c_ps_user[3].x) * (c_ps_user[12].z);
      float _901 = (c_ps_user[3].y) * (c_ps_user[12].z);
      float _902 = (c_ps_user[3].z) * (c_ps_user[12].z);
      float _904 = (c_ps_user[12].w) * COLOR.x;
      float _905 = (c_ps_user[12].w) * COLOR.y;
      float _906 = (c_ps_user[12].w) * COLOR.z;
      float _907 = _904 + _890;
      float _908 = _907 + _900;
      float _909 = _905 + _891;
      float _910 = _909 + _901;
      float _911 = _906 + _892;
      float _912 = _911 + _902;
      _914 = _908;
      _915 = _910;
      _916 = _912;
   }
   else
   {
      _914 = _890;
      _915 = _891;
      _916 = _892;
   }
   float _922 = (c_ps_col_scl[0].x) * _914;
   float _923 = (c_ps_col_scl[0].y) * _915;
   float _924 = (c_ps_col_scl[0].z) * _916;
   float _925 = (c_ps_col_scl[0].w) * _800;
   SV_Target.x = _922;
   SV_Target.y = _923;
   SV_Target.z = _924;
   SV_Target.w = _925;
   int _928 = c_bool & 8;
   bool _929 = (_928 != 0);
   float _930 = select(_929, _914, _922);
   float _931 = select(_929, _915, _923);
   float _932 = select(_929, _916, _924);
   float _933 = select(_929, _800, _925);
   SV_Target.x = _930;
   SV_Target.y = _931;
   SV_Target.z = _932;
   SV_Target.w = _933;
   return SV_Target;
}
