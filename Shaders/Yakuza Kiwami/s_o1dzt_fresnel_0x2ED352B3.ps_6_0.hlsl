// Yakuza Kiwami material s_o1dzt[fresnel] (0x2ED352B3, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
// applied by a script: the inline Hable tonemap's saturate goes through
// YKR_MaterialCurve (tangent continuation in HDR), and the light pre-pass buffer t14 through YKR_LightBuffer.

#include "Includes/Common.hlsl"

Texture2D<float4> s0_t : register(t0);

Texture2D<float4> s2_t : register(t2);

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

SamplerState s2_s : register(s2);

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
   float4 _52 = s2_t.Sample(s2_s, float2(TEXCOORD.x, TEXCOORD.y));
   float4 _56 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _61 = _56.x * _56.x;
   float _62 = _56.y * _56.y;
   float _63 = _56.z * _56.z;
   float4 _64 = s5_t.Sample(s5_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _69 = c_mat_power * _52.z;
   float _70 = max(1.0f, _69);
   float _71 = _64.x * 2.0f;
   float _72 = _64.y * 2.0f;
   float _73 = _71 + -1.0f;
   float _74 = _72 + -1.0f;
   float _75 = dot(float2(_73, _74), float2(_73, _74));
   float _76 = 1.0f - _75;
   float _77 = saturate(_76);
   float _78 = sqrt(_77);
   float _79 = TEXCOORD_2.y * TEXCOORD_3.z;
   float _80 = TEXCOORD_2.z * TEXCOORD_3.y;
   float _81 = _79 - _80;
   float _82 = TEXCOORD_2.z * TEXCOORD_3.x;
   float _83 = TEXCOORD_2.x * TEXCOORD_3.z;
   float _84 = _82 - _83;
   float _85 = TEXCOORD_2.x * TEXCOORD_3.y;
   float _86 = TEXCOORD_2.y * TEXCOORD_3.x;
   float _87 = _85 - _86;
   float _88 = _73 * TEXCOORD_3.x;
   float _89 = _73 * TEXCOORD_3.y;
   float _90 = _73 * TEXCOORD_3.z;
   float _91 = _74 * TEXCOORD_3.w;
   float _92 = _91 * _81;
   float _93 = _91 * _84;
   float _94 = _91 * _87;
   float _95 = _92 + _88;
   float _96 = _93 + _89;
   float _97 = _94 + _90;
   float _98 = _78 * TEXCOORD_2.x;
   float _99 = _78 * TEXCOORD_2.y;
   float _100 = _78 * TEXCOORD_2.z;
   float _101 = _95 + _98;
   float _102 = _96 + _99;
   float _103 = _97 + _100;
   float _104 = dot(float3(_101, _102, _103), float3(_101, _102, _103));
   float _105 = rsqrt(_104);
   float _106 = _101 * _105;
   float _107 = _102 * _105;
   float _108 = _103 * _105;
   float _113 = dot(float3(TEXCOORD_1.x, TEXCOORD_1.y, TEXCOORD_1.z), float3(TEXCOORD_1.x, TEXCOORD_1.y, TEXCOORD_1.z));
   float _114 = rsqrt(_113);
   float _115 = _114 * TEXCOORD_1.x;
   float _116 = _114 * TEXCOORD_1.y;
   float _117 = _114 * TEXCOORD_1.z;
   float _118 = _115 + (c_light_vec[0].x);
   float _119 = _116 + (c_light_vec[0].y);
   float _120 = _117 + (c_light_vec[0].z);
   float _121 = dot(float3(_118, _119, _120), float3(_118, _119, _120));
   float _122 = rsqrt(_121);
   float _123 = _107 + 0.10000000149011612f;
   float _124 = saturate(_123);
   float _127 = (c_attribute_ps[2].w) + 2.0f;
   float _129 = (c_attribute_ps[2].z) + 2.5f;
   float _130 = -0.0f - _115;
   float _131 = -0.0f - _116;
   float _132 = -0.0f - _117;
   float _133 = dot(float3(_130, _131, _132), float3(TEXCOORD_2.x, TEXCOORD_2.y, TEXCOORD_2.z));
   float _134 = 1.0f - _133;
   float _135 = saturate(_134);
   float _136 = dot(float3(_130, _131, _132), float3(_106, _107, _108));
   float _137 = 1.0f - _136;
   float _138 = saturate(_137);
   float _139 = _138 * _135;
   float _140 = log2(_139);
   float _141 = _140 * _127;
   float _142 = exp2(_141);
   float _143 = _142 * _129;
   int _146 = c_bool & 4096;
   bool _147 = (_146 == 0);
   float _156;
   float _180;
   float _181;
   float _193;
   float _194;
   float _317;
   float _318;
   float _319;
   float _320;
   float _321;
   float _322;
   float _355;
   float _356;
   float _357;
   float _358;
   float _523;
   float _689;
   float _690;
   float _691;
   float _767;
   float _768;
   float _769;
   float _785;
   float _786;
   float _787;
   float _812;
   float _813;
   float _814;
   if (!_147)
   {
      float _152 = (c_ps_user[9].w) * _140;
      float _153 = exp2(_152);
      float _154 = _153 * (c_ps_user[9].z);
      _156 = _154;
   }
   else
   {
      _156 = _143;
   }
   float _157 = _124 * _156;
   float _158 = _52.y * _157;
   float _161 = _158 * (c_ps_user[2].x);
   float _162 = _161 + 1.0f;
   float _163 = _61 * _162;
   float _164 = _62 * _162;
   float _165 = _63 * _162;
   float _169 = c_rt_size.z * SV_Position.x;
   float _170 = c_rt_size.w * SV_Position.y;
   int _173 = c_bool & 8;
   bool _174 = (_173 == 0);
   if (!_174)
   {
      float4 _176 = s13_t.Sample(s13_s, float2(_169, _170));
      _180 = _176.x;
      _181 = _176.y;
   }
   else
   {
      _180 = 1.0f;
      _181 = 1.0f;
   }
   if (!_174)
   {
      float _186 = (c_sdw_param_ch[0].x) * _180;
      float _187 = (c_sdw_param_ch[0].z) * _181;
      float _190 = _186 + (c_sdw_param_ch[0].y);
      float _191 = _187 + (c_sdw_param_ch[0].w);
      _193 = _190;
      _194 = _191;
   }
   else
   {
      _193 = _180;
      _194 = _181;
   }
   float _195 = min(_193, _194);
   float _196 = -0.0f - _122;
   float _197 = _118 * _196;
   float _198 = _119 * _196;
   float _199 = _120 * _196;
   float _200 = dot(float3(_197, _198, _199), float3(_106, _107, _108));
   float _201 = saturate(_200);
   float _202 = log2(_201);
   float _203 = _202 * _70;
   float _204 = exp2(_203);
   float _217 = _204 * _195;
   float _218 = c_specular_scale.x * c_mat_specular.x;
   float _219 = _218 * _217;
   float _220 = _219 * (c_light_col[0].x);
   float _221 = c_specular_scale.y * c_mat_specular.y;
   float _222 = _221 * _217;
   float _223 = _222 * (c_light_col[0].y);
   float _224 = c_specular_scale.z * c_mat_specular.z;
   float _225 = _224 * _217;
   float _226 = _225 * (c_light_col[0].z);
   float _227 = max(_200, 0.0f);
   float _228 = saturate(_136);
   float _229 = 1.0f - _228;
   float _230 = _229 * _229;
   float _231 = _230 * _227;
   float _232 = _231 + _220;
   float _233 = _231 + _223;
   float _234 = _231 + _226;
   int _238 = c_bool & 1;
   bool _239 = (_238 == 0);
   if (!_239)
   {
      float _241 = -0.0f - TEXCOORD_4.x;
      float _242 = -0.0f - TEXCOORD_4.y;
      float _243 = -0.0f - TEXCOORD_4.z;
      float _244 = dot(float3(_106, _107, _108), float3(_241, _242, _243));
      float _245 = saturate(_244);
      float _246 = _245 * TEXCOORD_1.w;
      float _247 = _245 * TEXCOORD_2.w;
      float _248 = _245 * TEXCOORD_4.w;
      float _249 = max(0.0f, _246);
      float _250 = max(0.0f, _247);
      float _251 = max(0.0f, _248);
      float _252 = -0.0f - TEXCOORD_5.x;
      float _253 = -0.0f - TEXCOORD_5.y;
      float _254 = -0.0f - TEXCOORD_5.z;
      float _255 = dot(float3(_106, _107, _108), float3(_252, _253, _254));
      float _256 = saturate(_255);
      float _257 = _256 * COLOR.w;
      float _258 = _256 * COLOR_1.w;
      float _259 = _256 * TEXCOORD_5.w;
      float _260 = max(0.0f, _257);
      float _261 = max(0.0f, _258);
      float _262 = max(0.0f, _259);
      float _263 = _260 + _249;
      float _264 = _261 + _250;
      float _265 = _262 + _251;
      float _266 = _115 + TEXCOORD_4.x;
      float _267 = _116 + TEXCOORD_4.y;
      float _268 = _117 + TEXCOORD_4.z;
      float _269 = dot(float3(_266, _267, _268), float3(_266, _267, _268));
      float _270 = rsqrt(_269);
      float _271 = _115 + TEXCOORD_5.x;
      float _272 = _116 + TEXCOORD_5.y;
      float _273 = _117 + TEXCOORD_5.z;
      float _274 = dot(float3(_271, _272, _273), float3(_271, _272, _273));
      float _275 = rsqrt(_274);
      float _276 = _271 * _275;
      float _277 = -0.0f - _276;
      float _278 = _272 * _275;
      float _279 = -0.0f - _278;
      float _280 = _273 * _275;
      float _281 = -0.0f - _280;
      float _282 = _266 * _270;
      float _283 = -0.0f - _282;
      float _284 = _267 * _270;
      float _285 = -0.0f - _284;
      float _286 = _268 * _270;
      float _287 = -0.0f - _286;
      float _288 = dot(float3(_283, _285, _287), float3(_106, _107, _108));
      float _289 = dot(float3(_277, _279, _281), float3(_106, _107, _108));
      float _290 = saturate(_288);
      float _291 = saturate(_289);
      float _292 = log2(_290);
      float _293 = log2(_291);
      float _294 = _292 * _70;
      float _295 = _293 * _70;
      float _296 = exp2(_294);
      float _297 = exp2(_295);
      float _298 = _296 * TEXCOORD_1.w;
      float _299 = _296 * TEXCOORD_2.w;
      float _300 = _296 * TEXCOORD_4.w;
      float _301 = _297 * COLOR.w;
      float _302 = _297 * COLOR_1.w;
      float _303 = _297 * TEXCOORD_5.w;
      float _304 = _301 + _298;
      float _305 = _302 + _299;
      float _306 = _303 + _300;
      float _307 = max(0.0f, _304);
      float _308 = max(0.0f, _305);
      float _309 = max(0.0f, _306);
      float _310 = _218 * _307;
      float _311 = _221 * _308;
      float _312 = _224 * _309;
      float _313 = min(_310, 1.0f);
      float _314 = min(_311, 1.0f);
      float _315 = min(_312, 1.0f);
      _317 = _313;
      _318 = _314;
      _319 = _315;
      _320 = _263;
      _321 = _264;
      _322 = _265;
   }
   else
   {
      _317 = 0.0f;
      _318 = 0.0f;
      _319 = 0.0f;
      _320 = 0.0f;
      _321 = 0.0f;
      _322 = 0.0f;
   }
   float _323 = c_rt_size.z * SV_Position.x;
   float _324 = c_rt_size.w * SV_Position.y;
   float4 _325 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_323, _324)));
   int _332 = c_bool & 16384;
   bool _333 = (_332 == 0);
   if (!_333)
   {
      float _335 = _325.x * 4.0f;
      float _336 = _325.y * 4.0f;
      float _337 = _325.z * 4.0f;
      float _338 = _325.w * 4.0f;
      _355 = _335;
      _356 = _336;
      _357 = _337;
      _358 = _338;
   }
   else
   {
      int _340 = c_bool & 8192;
      bool _341 = (_340 == 0);
      if (!_341)
      {
         float _343 = _325.x * 2.0f;
         float _344 = _325.y * 2.0f;
         float _345 = _325.z * 2.0f;
         float _346 = _325.w * 2.0f;
         _355 = _343;
         _356 = _344;
         _357 = _345;
         _358 = _346;
      }
      else
      {
         float _350 = _325.x / c_tone_control.z;
         float _351 = _325.y / c_tone_control.z;
         float _352 = _325.z / c_tone_control.z;
         float _353 = _325.w / c_tone_control.z;
         _355 = _350;
         _356 = _351;
         _357 = _352;
         _358 = _353;
      }
   }
   float _359 = _355 + _320;
   float _360 = _356 + _321;
   float _361 = _357 + _322;
   float _362 = _358 + _317;
   float _363 = _358 + _318;
   float _364 = _358 + _319;
   float _427 = dot(float3(_106, _107, _108), float3(_106, _107, _108));
   float _428 = rsqrt(_427);
   float _429 = _428 * _106;
   float _430 = _428 * _107;
   float _431 = _428 * _108;
   float _432 = dot(float4((c_ps_scratch[80].x), (c_ps_scratch[80].y), (c_ps_scratch[80].z), (c_ps_scratch[80].w)), float4(_429, _430, _431, 1.0f));
   float _433 = dot(float4((c_ps_scratch[81].x), (c_ps_scratch[81].y), (c_ps_scratch[81].z), (c_ps_scratch[81].w)), float4(_429, _430, _431, 1.0f));
   float _434 = dot(float4((c_ps_scratch[82].x), (c_ps_scratch[82].y), (c_ps_scratch[82].z), (c_ps_scratch[82].w)), float4(_429, _430, _431, 1.0f));
   float _435 = dot(float4((c_ps_scratch[83].x), (c_ps_scratch[83].y), (c_ps_scratch[83].z), (c_ps_scratch[83].w)), float4(_429, _430, _431, 1.0f));
   float _436 = dot(float4(_429, _430, _431, 1.0f), float4(_432, _433, _434, _435));
   float _437 = dot(float4((c_ps_scratch[84].x), (c_ps_scratch[84].y), (c_ps_scratch[84].z), (c_ps_scratch[84].w)), float4(_429, _430, _431, 1.0f));
   float _438 = dot(float4((c_ps_scratch[85].x), (c_ps_scratch[85].y), (c_ps_scratch[85].z), (c_ps_scratch[85].w)), float4(_429, _430, _431, 1.0f));
   float _439 = dot(float4((c_ps_scratch[86].x), (c_ps_scratch[86].y), (c_ps_scratch[86].z), (c_ps_scratch[86].w)), float4(_429, _430, _431, 1.0f));
   float _440 = dot(float4((c_ps_scratch[87].x), (c_ps_scratch[87].y), (c_ps_scratch[87].z), (c_ps_scratch[87].w)), float4(_429, _430, _431, 1.0f));
   float _441 = dot(float4(_429, _430, _431, 1.0f), float4(_437, _438, _439, _440));
   float _442 = dot(float4((c_ps_scratch[88].x), (c_ps_scratch[88].y), (c_ps_scratch[88].z), (c_ps_scratch[88].w)), float4(_429, _430, _431, 1.0f));
   float _443 = dot(float4((c_ps_scratch[89].x), (c_ps_scratch[89].y), (c_ps_scratch[89].z), (c_ps_scratch[89].w)), float4(_429, _430, _431, 1.0f));
   float _444 = dot(float4((c_ps_scratch[90].x), (c_ps_scratch[90].y), (c_ps_scratch[90].z), (c_ps_scratch[90].w)), float4(_429, _430, _431, 1.0f));
   float _445 = dot(float4((c_ps_scratch[91].x), (c_ps_scratch[91].y), (c_ps_scratch[91].z), (c_ps_scratch[91].w)), float4(_429, _430, _431, 1.0f));
   float _446 = dot(float4(_429, _430, _431, 1.0f), float4(_442, _443, _444, _445));
   float _447 = max(_436, 0.0625f);
   float _448 = max(_441, 0.0625f);
   float _449 = max(_446, 0.0625f);
   float _450 = _447 * (c_ps_scratch[79].x);
   float _451 = _448 * (c_ps_scratch[79].x);
   float _452 = _449 * (c_ps_scratch[79].x);
   float _457 = -0.0f - (c_light_vec[0].x);
   float _458 = -0.0f - (c_light_vec[0].y);
   float _459 = -0.0f - (c_light_vec[0].z);
   float _460 = dot(float3(_106, _107, _108), float3(_457, _458, _459));
   float _461 = -0.0f - (c_light_vec[1].x);
   float _462 = -0.0f - (c_light_vec[1].y);
   float _463 = -0.0f - (c_light_vec[1].z);
   float _464 = dot(float3(_106, _107, _108), float3(_461, _462, _463));
   float _473 = _107 * 0.5f;
   float _474 = _473 + 0.5f;
   float _475 = (c_light_col[4].x) - (c_light_vec[4].x);
   float _476 = (c_light_col[4].y) - (c_light_vec[4].y);
   float _477 = (c_light_col[4].z) - (c_light_vec[4].z);
   float _478 = _475 * _474;
   float _479 = _476 * _474;
   float _480 = _477 * _474;
   float _481 = saturate(_460);
   float _482 = saturate(_464);
   float _487 = (c_light_col[0].x) * _481;
   float _488 = (c_light_col[0].y) * _481;
   float _489 = (c_light_col[0].z) * _481;
   float _494 = (c_light_col[1].x) * _482;
   float _495 = (c_light_col[1].y) * _482;
   float _496 = (c_light_col[1].z) * _482;
   float _497 = _487 * _195;
   float _498 = _488 * _195;
   float _499 = _489 * _195;
   float _500 = (c_light_vec[4].x) + _450;
   float _501 = _500 + _478;
   float _502 = _501 + _359;
   float _503 = _502 + _494;
   float _504 = _503 + _497;
   float _505 = (c_light_vec[4].y) + _451;
   float _506 = _505 + _479;
   float _507 = _506 + _360;
   float _508 = _507 + _495;
   float _509 = _508 + _498;
   float _510 = (c_light_vec[4].z) + _452;
   float _511 = _510 + _480;
   float _512 = _511 + _361;
   float _513 = _512 + _496;
   float _514 = _513 + _499;
   int _515 = c_bool & 64;
   bool _516 = (_515 == 0);
   if (!_516)
   {
      float _518 = 1.0f - _52.y;
      float _519 = _195 * _518;
      float _520 = _519 * _481;
      float _521 = _520 + _52.y;
      _523 = _521;
   }
   else
   {
      _523 = _52.y;
   }
   float _524 = _232 + _362;
   float _525 = _233 + _363;
   float _526 = _234 + _364;
   int _527 = c_bool & 32768;
   bool _528 = (_527 == 0);
   if (!_528)
   {
      float _540 = (c_ps_user[11].x) * _163;
      float _541 = (c_ps_user[11].x) * _164;
      float _542 = (c_ps_user[11].x) * _165;
      float _544 = _106 + 1.0f;
      float _545 = _107 + 1.0f;
      float _546 = _108 + 1.0f;
      float _547 = _544 * 0.5f;
      float _548 = _545 * 0.5f;
      float _549 = _546 * 0.5f;
      float _550 = saturate(_547);
      float _551 = saturate(_548);
      float _552 = saturate(_549);
      float _553 = (c_ps_user[11].y) * _550;
      float _554 = _551 * (c_ps_user[11].y);
      float _555 = _552 * (c_ps_user[11].y);
      float _565 = (c_ps_user[11].z) * _52.x;
      float _567 = (c_ps_user[11].w) * _524;
      float _570 = (c_ps_user[12].x) * _195;
      float _572 = (c_ps_user[12].y) * _52.y;
      float _577 = _137 * _137;
      float _578 = _137 * 0.9722222089767456f;
      float _579 = _577 * _577;
      float _580 = _579 * _578;
      float _581 = _580 + 0.02777777425944805f;
      float _582 = (c_ps_user[13].z) * _581;
      float _584 = (c_ps_user[13].w) * _501;
      float _585 = (c_ps_user[13].w) * _506;
      float _586 = (c_ps_user[13].w) * _511;
      float _589 = (c_ps_user[14].y) * _487;
      float _590 = (c_ps_user[14].y) * _488;
      float _591 = (c_ps_user[14].y) * _489;
      float _593 = (c_ps_user[14].z) * _494;
      float _594 = (c_ps_user[14].z) * _495;
      float _595 = (c_ps_user[14].z) * _496;
      float _597 = (c_ps_user[14].w) * _359;
      float _598 = (c_ps_user[14].w) * _360;
      float _599 = (c_ps_user[14].w) * _361;
      float _604 = (c_ps_user[15].z) * _362;
      float _605 = (c_ps_user[15].z) * _363;
      float _606 = (c_ps_user[15].z) * _364;
      float _607 = (c_ps_user[15].y) + (c_ps_user[15].x);
      float _608 = c_specular_scale.x * _607;
      float _609 = _608 * _524;
      float _610 = _589 + (c_ps_user[13].x);
      float _611 = _610 + _593;
      float _612 = c_diffuse_scale.x * _611;
      float _613 = _612 * c_mat_diffuse.x;
      float _614 = (c_ps_user[13].y) + _565;
      float _615 = _614 * c_specular_scale.x;
      float _616 = _615 + _609;
      float _617 = _616 * c_mat_specular.x;
      float _618 = _553 + _540;
      float _619 = _618 + _567;
      float _620 = _619 + _570;
      float _621 = _620 + _572;
      float _622 = _621 + _584;
      float _623 = _622 + _582;
      float _624 = _623 + _597;
      float _625 = _624 + _604;
      float _626 = _625 + _613;
      float _627 = _626 + _617;
      float _628 = c_specular_scale.y * _607;
      float _629 = _628 * _525;
      float _630 = _590 + (c_ps_user[13].x);
      float _631 = _630 + _594;
      float _632 = c_diffuse_scale.y * _631;
      float _633 = _632 * c_mat_diffuse.y;
      float _634 = _614 * c_specular_scale.y;
      float _635 = _634 + _629;
      float _636 = _635 * c_mat_specular.y;
      float _637 = _567 + _541;
      float _638 = _637 + _554;
      float _639 = _638 + _570;
      float _640 = _639 + _572;
      float _641 = _640 + _585;
      float _642 = _641 + _582;
      float _643 = _642 + _598;
      float _644 = _643 + _605;
      float _645 = _644 + _633;
      float _646 = _645 + _636;
      float _647 = c_specular_scale.z * _607;
      float _648 = _647 * _526;
      float _649 = _591 + (c_ps_user[13].x);
      float _650 = _649 + _595;
      float _651 = c_diffuse_scale.z * _650;
      float _652 = _651 * c_mat_diffuse.z;
      float _653 = _614 * c_specular_scale.z;
      float _654 = _653 + _648;
      float _655 = _654 * c_mat_specular.z;
      float _656 = _567 + _542;
      float _657 = _656 + _555;
      float _658 = _657 + _570;
      float _659 = _658 + _572;
      float _660 = _659 + _586;
      float _661 = _660 + _582;
      float _662 = _661 + _599;
      float _663 = _662 + _606;
      float _664 = _663 + _652;
      float _665 = _664 + _655;
      float _666 = saturate(_627);
      float _667 = saturate(_646);
      float _668 = saturate(_665);
      _689 = _666;
      _690 = _667;
      _691 = _668;
   }
   else
   {
      float _670 = c_diffuse_scale.x * c_mat_diffuse.x;
      float _671 = _670 * _163;
      float _672 = _671 * _504;
      float _673 = _672 * _523;
      float _674 = c_diffuse_scale.y * c_mat_diffuse.y;
      float _675 = _674 * _164;
      float _676 = _675 * _509;
      float _677 = _676 * _523;
      float _678 = c_diffuse_scale.z * c_mat_diffuse.z;
      float _679 = _678 * _165;
      float _680 = _679 * _514;
      float _681 = _680 * _523;
      float _682 = _524 * _52.x;
      float _683 = _525 * _52.x;
      float _684 = _526 * _52.x;
      float _685 = _673 + _682;
      float _686 = _677 + _683;
      float _687 = _681 + _684;
      _689 = _685;
      _690 = _686;
      _691 = _687;
   }
   float _696 = c_mat_opacity * _56.w;
   float _697 = _696 * c_opacity_scale;
   float _698 = _697 * c_global_opacity;
   float _699 = _689 * COLOR.x;
   float _700 = _690 * COLOR.y;
   float _701 = _691 * COLOR.z;
   int _702 = c_bool & 16;
   bool _703 = (_702 == 0);
   if (!_703)
   {
      float _707 = c_tone_control.x * _699;
      float _708 = c_tone_control.x * _700;
      float _709 = c_tone_control.x * _701;
      float _711 = log2(_707);
      float _712 = log2(_708);
      float _713 = log2(_709);
      float _714 = _711 * c_tone_control.y;
      float _715 = _712 * c_tone_control.y;
      float _716 = _713 * c_tone_control.y;
      float _717 = exp2(_714);
      float _718 = exp2(_715);
      float _719 = exp2(_716);
      _767 = _717;
      _768 = _718;
      _769 = _719;
   }
   else
   {
      if (_528)
      {
         float _724 = c_tone_control.x * _699;
         float _725 = c_tone_control.x * _700;
         float _726 = c_tone_control.x * _701;
         float _727 = _724 * 2.0f;
         float _728 = _725 * 2.0f;
         float _729 = _726 * 2.0f;
         float _730 = _724 * 0.30000001192092896f;
         float _731 = _725 * 0.30000001192092896f;
         float _732 = _726 * 0.30000001192092896f;
         float _733 = _730 + 0.05000000074505806f;
         float _734 = _731 + 0.05000000074505806f;
         float _735 = _732 + 0.05000000074505806f;
         float _736 = _733 * _727;
         float _737 = _734 * _728;
         float _738 = _735 * _729;
         float _739 = _736 + 0.004000000189989805f;
         float _740 = _737 + 0.004000000189989805f;
         float _741 = _738 + 0.004000000189989805f;
         float _742 = _730 + 0.5f;
         float _743 = _731 + 0.5f;
         float _744 = _732 + 0.5f;
         float _745 = _742 * _727;
         float _746 = _743 * _728;
         float _747 = _744 * _729;
         float _748 = _745 + 0.06000000238418579f;
         float _749 = _746 + 0.06000000238418579f;
         float _750 = _747 + 0.06000000238418579f;
         float _751 = _739 / _748;
         float _752 = _740 / _749;
         float _753 = _741 / _750;
         float _754 = _751 + -0.06666666269302368f;
         float _755 = _752 + -0.06666666269302368f;
         float _756 = _753 + -0.06666666269302368f;
         float _757 = _754 * 1.3790643215179443f;
         float _758 = _755 * 1.3790643215179443f;
         float _759 = _756 * 1.3790643215179443f;
         float _760 = YKR_MaterialCurve(_757, _724);
         float _761 = YKR_MaterialCurve(_758, _725);
         float _762 = YKR_MaterialCurve(_759, _726);
         float _763 = sqrt(_760);
         float _764 = sqrt(_761);
         float _765 = sqrt(_762);
         _767 = _763;
         _768 = _764;
         _769 = _765;
      }
      else
      {
         _767 = _699;
         _768 = _700;
         _769 = _701;
      }
   }
   int _770 = c_bool & 4;
   bool _771 = (_770 == 0);
   if (_771)
   {
      float _773 = saturate(COLOR_1.x);
      float _778 = (c_fog_param[0].x) - _767;
      float _779 = (c_fog_param[0].y) - _768;
      float _780 = (c_fog_param[0].z) - _769;
      float _781 = _778 * _773;
      float _782 = _779 * _773;
      float _783 = _780 * _773;
      _785 = _781;
      _786 = _782;
      _787 = _783;
   }
   else
   {
      _785 = COLOR_1.x;
      _786 = COLOR_1.y;
      _787 = COLOR_1.z;
   }
   float _788 = _785 + _767;
   float _789 = _786 + _768;
   float _790 = _787 + _769;
   if (!_528)
   {
      float _798 = (c_ps_user[3].x) * (c_ps_user[12].z);
      float _799 = (c_ps_user[3].y) * (c_ps_user[12].z);
      float _800 = (c_ps_user[3].z) * (c_ps_user[12].z);
      float _802 = (c_ps_user[12].w) * COLOR.x;
      float _803 = (c_ps_user[12].w) * COLOR.y;
      float _804 = (c_ps_user[12].w) * COLOR.z;
      float _805 = _802 + _788;
      float _806 = _805 + _798;
      float _807 = _803 + _789;
      float _808 = _807 + _799;
      float _809 = _804 + _790;
      float _810 = _809 + _800;
      _812 = _806;
      _813 = _808;
      _814 = _810;
   }
   else
   {
      _812 = _788;
      _813 = _789;
      _814 = _790;
   }
   float _820 = (c_ps_col_scl[0].x) * _812;
   float _821 = (c_ps_col_scl[0].y) * _813;
   float _822 = (c_ps_col_scl[0].z) * _814;
   float _823 = (c_ps_col_scl[0].w) * _698;
   SV_Target.x = _820;
   SV_Target.y = _821;
   SV_Target.z = _822;
   SV_Target.w = _823;
   int _826 = c_bool & 8;
   bool _827 = (_826 != 0);
   float _828 = select(_827, _812, _820);
   float _829 = select(_827, _813, _821);
   float _830 = select(_827, _814, _822);
   float _831 = select(_827, _698, _823);
   SV_Target.x = _828;
   SV_Target.y = _829;
   SV_Target.z = _830;
   SV_Target.w = _831;
   return SV_Target;
}
