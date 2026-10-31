// Yakuza Kiwami material s_b1ht[hair][offset][dedit] (0xB7F53775, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
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
    linear float4 TEXCOORD : TEXCOORD,
    linear float4 TEXCOORD_1 : TEXCOORD1,
    linear float4 TEXCOORD_2 : TEXCOORD2,
    linear float4 TEXCOORD_3 : TEXCOORD3,
    linear float4 TEXCOORD_4 : TEXCOORD4,
    linear float4 TEXCOORD_5 : TEXCOORD5,
    noperspective float4 SV_Position : SV_Position) : SV_Target
{
   float4 SV_Target = 0;
   float4 _54 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _56 = _54.y * _54.y;
   float4 _57 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _60 = _57.x + -0.49803921580314636f;
   float4 _61 = s5_t.Sample(s5_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _68 = (c_ps_user[3].x) * _56;
   float _69 = (c_ps_user[3].y) * _56;
   float _70 = (c_ps_user[3].z) * _56;
   bool _71 = (_61.y < 0.5f);
   float _72 = _60 * 0.25f;
   float _73 = _72 + 0.9980392456054688f;
   float _74 = _61.y * _73;
   float _75 = 1.0f - _61.y;
   float _76 = 1.0019607543945312f - _72;
   float _77 = _75 * _76;
   float _78 = 1.0f - _77;
   float _79 = select(_71, _74, _78);
   float _82 = _61.x * 2.0f;
   float _83 = _79 * 2.0f;
   float _84 = _82 + -1.0f;
   float _85 = _83 + -1.0f;
   float _86 = dot(float2(_84, _85), float2(_84, _85));
   float _87 = 1.0f - _86;
   float _88 = saturate(_87);
   float _89 = sqrt(_88);
   float _90 = TEXCOORD_2.y * TEXCOORD_3.z;
   float _91 = TEXCOORD_2.z * TEXCOORD_3.y;
   float _92 = _90 - _91;
   float _93 = TEXCOORD_2.z * TEXCOORD_3.x;
   float _94 = TEXCOORD_2.x * TEXCOORD_3.z;
   float _95 = _93 - _94;
   float _96 = TEXCOORD_2.x * TEXCOORD_3.y;
   float _97 = TEXCOORD_2.y * TEXCOORD_3.x;
   float _98 = _96 - _97;
   float _99 = _84 * TEXCOORD_3.x;
   float _100 = _84 * TEXCOORD_3.y;
   float _101 = _84 * TEXCOORD_3.z;
   float _102 = _85 * TEXCOORD_3.w;
   float _103 = _102 * _92;
   float _104 = _102 * _95;
   float _105 = _102 * _98;
   float _106 = _89 * TEXCOORD_2.x;
   float _107 = _89 * TEXCOORD_2.y;
   float _108 = _89 * TEXCOORD_2.z;
   float _109 = _106 + _99;
   float _110 = _109 + _103;
   float _111 = _107 + _100;
   float _112 = _111 + _104;
   float _113 = _108 + _101;
   float _114 = _113 + _105;
   float _115 = dot(float3(_110, _112, _114), float3(_110, _112, _114));
   float _116 = rsqrt(_115);
   float _117 = _110 * _116;
   float _118 = _112 * _116;
   float _119 = _114 * _116;
   float _124 = dot(float3(TEXCOORD_1.x, TEXCOORD_1.y, TEXCOORD_1.z), float3(TEXCOORD_1.x, TEXCOORD_1.y, TEXCOORD_1.z));
   float _125 = rsqrt(_124);
   float _126 = _125 * TEXCOORD_1.x;
   float _127 = _125 * TEXCOORD_1.y;
   float _128 = _125 * TEXCOORD_1.z;
   float _129 = _126 + (c_light_vec[0].x);
   float _130 = _127 + (c_light_vec[0].y);
   float _131 = _128 + (c_light_vec[0].z);
   float _132 = dot(float3(_129, _130, _131), float3(_129, _130, _131));
   float _133 = rsqrt(_132);
   float _134 = _129 * _133;
   float _135 = _130 * _133;
   float _136 = _131 * _133;
   float _140 = c_rt_size.z * SV_Position.x;
   float _141 = c_rt_size.w * SV_Position.y;
   int _144 = c_bool & 8;
   bool _145 = (_144 == 0);
   float _151;
   float _152;
   float _164;
   float _165;
   float _330;
   float _331;
   float _332;
   float _361;
   float _362;
   float _363;
   float _669;
   float _670;
   float _671;
   float _747;
   float _748;
   float _749;
   float _765;
   float _766;
   float _767;
   float _792;
   float _793;
   float _794;
   if (!_145)
   {
      float4 _147 = s13_t.Sample(s13_s, float2(_140, _141));
      _151 = _147.x;
      _152 = _147.y;
   }
   else
   {
      _151 = 1.0f;
      _152 = 1.0f;
   }
   if (!_145)
   {
      float _157 = (c_sdw_param_ch[0].x) * _151;
      float _158 = (c_sdw_param_ch[0].z) * _152;
      float _161 = _157 + (c_sdw_param_ch[0].y);
      float _162 = _158 + (c_sdw_param_ch[0].w);
      _164 = _161;
      _165 = _162;
   }
   else
   {
      _164 = _151;
      _165 = _152;
   }
   float _166 = min(_164, _165);
   float _171 = (c_light_vec[1].x) + _126;
   float _172 = (c_light_vec[1].y) + _127;
   float _173 = (c_light_vec[1].z) + _128;
   float _174 = dot(float3(_171, _172, _173), float3(_171, _172, _173));
   float _175 = rsqrt(_174);
   float _176 = _171 * _175;
   float _177 = _172 * _175;
   float _178 = _173 * _175;
   float _179 = _118 * TEXCOORD_3.z;
   float _180 = _119 * TEXCOORD_3.y;
   float _181 = _179 - _180;
   float _182 = _119 * TEXCOORD_3.x;
   float _183 = _117 * TEXCOORD_3.z;
   float _184 = _182 - _183;
   float _185 = _117 * TEXCOORD_3.y;
   float _186 = _118 * TEXCOORD_3.x;
   float _187 = _185 - _186;
   float _188 = _181 * TEXCOORD_3.w;
   float _189 = _184 * TEXCOORD_3.w;
   float _190 = _187 * TEXCOORD_3.w;
   float _191 = _189 * _119;
   float _192 = _190 * _118;
   float _193 = _191 - _192;
   float _194 = _190 * _117;
   float _195 = _188 * _119;
   float _196 = _194 - _195;
   float _197 = _188 * _118;
   float _198 = _189 * _117;
   float _199 = _197 - _198;
   float _200 = -0.0f - _134;
   float _201 = -0.0f - _135;
   float _202 = -0.0f - _136;
   float _203 = dot(float3(_117, _118, _119), float3(_200, _201, _202));
   float _204 = dot(float3(_200, _201, _202), float3(_188, _189, _190));
   float _205 = TEXCOORD.z * 0.20000000298023224f;
   float _206 = _204 * _205;
   float _207 = _206 + 0.5f;
   float4 _208 = s7_t.Sample(s7_s, float2(_203, _207));
   float _210 = dot(float3(_200, _201, _202), float3(_193, _196, _199));
   float _211 = TEXCOORD.w * 0.20000000298023224f;
   float _212 = _210 * _211;
   float _213 = _212 + 0.5f;
   float4 _214 = s7_t.Sample(s7_s, float2(_203, _213));
   float _216 = _214.x * _208.x;
   float _217 = -0.0f - _176;
   float _218 = -0.0f - _177;
   float _219 = -0.0f - _178;
   float _220 = dot(float3(_117, _118, _119), float3(_217, _218, _219));
   float _221 = dot(float3(_217, _218, _219), float3(_188, _189, _190));
   float _222 = _221 * _205;
   float _223 = _222 + 0.5f;
   float4 _224 = s7_t.Sample(s7_s, float2(_220, _223));
   float _226 = dot(float3(_217, _218, _219), float3(_193, _196, _199));
   float _227 = _226 * _211;
   float _228 = _227 + 0.5f;
   float4 _229 = s7_t.Sample(s7_s, float2(_220, _228));
   float _231 = _229.x * _224.x;
   float _232 = log2(_216);
   float _233 = log2(_231);
   float _234 = _232 * c_mat_power;
   float _235 = _233 * c_mat_power;
   float _236 = exp2(_234);
   float _237 = exp2(_235);
   float _242 = c_mat_fresnel * c_tone_control.w;
   float _243 = c_mat_power * 0.03974359855055809f;
   float _244 = _243 + 0.0856831967830658f;
   float _245 = _237 * _244;
   float _246 = dot(float3(_126, _127, _128), float3(_134, _135, _136));
   float _247 = dot(float3(_126, _127, _128), float3(_176, _177, _178));
   float _248 = 1.0f - _246;
   float _249 = 1.0f - _247;
   float _250 = _248 * _248;
   float _251 = _249 * _249;
   float _252 = 1.0f - _242;
   float _253 = _250 * _250;
   float _254 = _253 * _248;
   float _255 = _254 * _252;
   float _256 = _249 * _252;
   float _257 = _251 * _251;
   float _258 = _257 * _256;
   float _259 = _255 + _242;
   float _260 = _258 + _242;
   float _261 = _245 * _260;
   float _270 = c_specular_scale.x * c_mat_specular.x;
   float _271 = c_specular_scale.y * c_mat_specular.y;
   float _272 = c_specular_scale.z * c_mat_specular.z;
   float _277 = _166 * _244;
   float _278 = _277 * _236;
   float _279 = _278 * _259;
   float _280 = _279 * (c_light_col[0].x);
   float _281 = _279 * (c_light_col[0].y);
   float _282 = _279 * (c_light_col[0].z);
   float _287 = _261 * (c_light_col[1].x);
   float _288 = _261 * (c_light_col[1].y);
   float _289 = _261 * (c_light_col[1].z);
   float _290 = _280 + _287;
   float _291 = _281 + _288;
   float _292 = _282 + _289;
   float _293 = _270 * _290;
   float _294 = _271 * _291;
   float _295 = _272 * _292;
   int _301 = c_bool & 1;
   bool _302 = (_301 == 0);
   if (!_302)
   {
      float _304 = -0.0f - TEXCOORD_4.x;
      float _305 = -0.0f - TEXCOORD_4.y;
      float _306 = -0.0f - TEXCOORD_4.z;
      float _307 = dot(float3(_117, _118, _119), float3(_304, _305, _306));
      float _308 = saturate(_307);
      float _309 = _308 * TEXCOORD_1.w;
      float _310 = _308 * TEXCOORD_2.w;
      float _311 = _308 * TEXCOORD_4.w;
      float _312 = max(0.0f, _309);
      float _313 = max(0.0f, _310);
      float _314 = max(0.0f, _311);
      float _315 = -0.0f - TEXCOORD_5.x;
      float _316 = -0.0f - TEXCOORD_5.y;
      float _317 = -0.0f - TEXCOORD_5.z;
      float _318 = dot(float3(_117, _118, _119), float3(_315, _316, _317));
      float _319 = saturate(_318);
      float _320 = _319 * COLOR.w;
      float _321 = _319 * COLOR_1.w;
      float _322 = _319 * TEXCOORD_5.w;
      float _323 = max(0.0f, _320);
      float _324 = max(0.0f, _321);
      float _325 = max(0.0f, _322);
      float _326 = _323 + _312;
      float _327 = _324 + _313;
      float _328 = _325 + _314;
      _330 = _326;
      _331 = _327;
      _332 = _328;
   }
   else
   {
      _330 = 0.0f;
      _331 = 0.0f;
      _332 = 0.0f;
   }
   float _333 = c_rt_size.z * SV_Position.x;
   float _334 = c_rt_size.w * SV_Position.y;
   float4 _335 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_333, _334)));
   int _341 = c_bool & 16384;
   bool _342 = (_341 == 0);
   if (!_342)
   {
      float _344 = _335.x * 4.0f;
      float _345 = _335.y * 4.0f;
      float _346 = _335.z * 4.0f;
      _361 = _344;
      _362 = _345;
      _363 = _346;
   }
   else
   {
      int _348 = c_bool & 8192;
      bool _349 = (_348 == 0);
      if (!_349)
      {
         float _351 = _335.x * 2.0f;
         float _352 = _335.y * 2.0f;
         float _353 = _335.z * 2.0f;
         _361 = _351;
         _362 = _352;
         _363 = _353;
      }
      else
      {
         float _357 = _335.x / c_tone_control.z;
         float _358 = _335.y / c_tone_control.z;
         float _359 = _335.z / c_tone_control.z;
         _361 = _357;
         _362 = _358;
         _363 = _359;
      }
   }
   float _364 = _361 + _330;
   float _365 = _362 + _331;
   float _366 = _363 + _332;
   float _429 = dot(float3(_117, _118, _119), float3(_117, _118, _119));
   float _430 = rsqrt(_429);
   float _431 = _430 * _117;
   float _432 = _430 * _118;
   float _433 = _430 * _119;
   float _434 = dot(float4((c_ps_scratch[80].x), (c_ps_scratch[80].y), (c_ps_scratch[80].z), (c_ps_scratch[80].w)), float4(_431, _432, _433, 1.0f));
   float _435 = dot(float4((c_ps_scratch[81].x), (c_ps_scratch[81].y), (c_ps_scratch[81].z), (c_ps_scratch[81].w)), float4(_431, _432, _433, 1.0f));
   float _436 = dot(float4((c_ps_scratch[82].x), (c_ps_scratch[82].y), (c_ps_scratch[82].z), (c_ps_scratch[82].w)), float4(_431, _432, _433, 1.0f));
   float _437 = dot(float4((c_ps_scratch[83].x), (c_ps_scratch[83].y), (c_ps_scratch[83].z), (c_ps_scratch[83].w)), float4(_431, _432, _433, 1.0f));
   float _438 = dot(float4(_431, _432, _433, 1.0f), float4(_434, _435, _436, _437));
   float _439 = dot(float4((c_ps_scratch[84].x), (c_ps_scratch[84].y), (c_ps_scratch[84].z), (c_ps_scratch[84].w)), float4(_431, _432, _433, 1.0f));
   float _440 = dot(float4((c_ps_scratch[85].x), (c_ps_scratch[85].y), (c_ps_scratch[85].z), (c_ps_scratch[85].w)), float4(_431, _432, _433, 1.0f));
   float _441 = dot(float4((c_ps_scratch[86].x), (c_ps_scratch[86].y), (c_ps_scratch[86].z), (c_ps_scratch[86].w)), float4(_431, _432, _433, 1.0f));
   float _442 = dot(float4((c_ps_scratch[87].x), (c_ps_scratch[87].y), (c_ps_scratch[87].z), (c_ps_scratch[87].w)), float4(_431, _432, _433, 1.0f));
   float _443 = dot(float4(_431, _432, _433, 1.0f), float4(_439, _440, _441, _442));
   float _444 = dot(float4((c_ps_scratch[88].x), (c_ps_scratch[88].y), (c_ps_scratch[88].z), (c_ps_scratch[88].w)), float4(_431, _432, _433, 1.0f));
   float _445 = dot(float4((c_ps_scratch[89].x), (c_ps_scratch[89].y), (c_ps_scratch[89].z), (c_ps_scratch[89].w)), float4(_431, _432, _433, 1.0f));
   float _446 = dot(float4((c_ps_scratch[90].x), (c_ps_scratch[90].y), (c_ps_scratch[90].z), (c_ps_scratch[90].w)), float4(_431, _432, _433, 1.0f));
   float _447 = dot(float4((c_ps_scratch[91].x), (c_ps_scratch[91].y), (c_ps_scratch[91].z), (c_ps_scratch[91].w)), float4(_431, _432, _433, 1.0f));
   float _448 = dot(float4(_431, _432, _433, 1.0f), float4(_444, _445, _446, _447));
   float _449 = max(_438, 0.0625f);
   float _450 = max(_443, 0.0625f);
   float _451 = max(_448, 0.0625f);
   float _452 = _449 * (c_ps_scratch[79].x);
   float _453 = _450 * (c_ps_scratch[79].x);
   float _454 = _451 * (c_ps_scratch[79].x);
   float _459 = -0.0f - (c_light_vec[0].x);
   float _460 = -0.0f - (c_light_vec[0].y);
   float _461 = -0.0f - (c_light_vec[0].z);
   float _462 = dot(float3(_117, _118, _119), float3(_459, _460, _461));
   float _463 = -0.0f - (c_light_vec[1].x);
   float _464 = -0.0f - (c_light_vec[1].y);
   float _465 = -0.0f - (c_light_vec[1].z);
   float _466 = dot(float3(_117, _118, _119), float3(_463, _464, _465));
   float _475 = _118 * 0.5f;
   float _476 = _475 + 0.5f;
   float _477 = (c_light_col[4].x) - (c_light_vec[4].x);
   float _478 = (c_light_col[4].y) - (c_light_vec[4].y);
   float _479 = (c_light_col[4].z) - (c_light_vec[4].z);
   float _480 = _477 * _476;
   float _481 = _478 * _476;
   float _482 = _479 * _476;
   float _483 = saturate(_462);
   float _484 = saturate(_466);
   float _489 = (c_light_col[0].x) * _483;
   float _490 = (c_light_col[0].y) * _483;
   float _491 = (c_light_col[0].z) * _483;
   float _496 = (c_light_col[1].x) * _484;
   float _497 = (c_light_col[1].y) * _484;
   float _498 = (c_light_col[1].z) * _484;
   int _499 = c_bool & 32768;
   bool _500 = (_499 == 0);
   float _509 = (c_light_vec[4].x) + _452;
   float _510 = _509 + _480;
   float _511 = (c_light_vec[4].y) + _453;
   float _512 = _511 + _481;
   float _513 = (c_light_vec[4].z) + _454;
   float _514 = _513 + _482;
   if (!_500)
   {
      float _518 = (c_ps_user[11].x) * _68;
      float _519 = (c_ps_user[11].x) * _69;
      float _520 = (c_ps_user[11].x) * _70;
      float _522 = _117 + 1.0f;
      float _523 = _118 + 1.0f;
      float _524 = _119 + 1.0f;
      float _525 = _522 * 0.5f;
      float _526 = _523 * 0.5f;
      float _527 = _524 * 0.5f;
      float _528 = saturate(_525);
      float _529 = saturate(_526);
      float _530 = saturate(_527);
      float _531 = (c_ps_user[11].y) * _528;
      float _532 = _529 * (c_ps_user[11].y);
      float _533 = _530 * (c_ps_user[11].y);
      float _544 = (c_ps_user[11].w) * _293;
      float _547 = (c_ps_user[12].x) * _166;
      float _553 = -0.0f - _126;
      float _554 = -0.0f - _127;
      float _555 = -0.0f - _128;
      float _556 = dot(float3(_553, _554, _555), float3(_117, _118, _119));
      float _557 = 1.0f - _556;
      float _558 = _557 * _557;
      float _559 = _557 * 0.9722222089767456f;
      float _560 = _558 * _558;
      float _561 = _560 * _559;
      float _562 = _561 + 0.02777777425944805f;
      float _563 = _562 * (c_ps_user[13].z);
      float _565 = (c_ps_user[13].w) * _510;
      float _566 = (c_ps_user[13].w) * _512;
      float _567 = (c_ps_user[13].w) * _514;
      float _570 = (c_ps_user[14].y) * _489;
      float _571 = (c_ps_user[14].y) * _490;
      float _572 = (c_ps_user[14].y) * _491;
      float _574 = (c_ps_user[14].z) * _496;
      float _575 = (c_ps_user[14].z) * _497;
      float _576 = (c_ps_user[14].z) * _498;
      float _578 = (c_ps_user[14].w) * _364;
      float _579 = (c_ps_user[14].w) * _365;
      float _580 = (c_ps_user[14].w) * _366;
      float _584 = (c_ps_user[15].y) + (c_ps_user[15].x);
      float _585 = c_specular_scale.x * _584;
      float _586 = _585 * _293;
      float _587 = _570 + (c_ps_user[13].x);
      float _588 = _587 + _574;
      float _589 = c_diffuse_scale.x * _588;
      float _590 = _589 * c_mat_diffuse.x;
      float _591 = (c_ps_user[13].y) + (c_ps_user[11].z);
      float _592 = _591 * c_specular_scale.x;
      float _593 = _592 + _586;
      float _594 = _593 * c_mat_specular.x;
      float _595 = _531 + _518;
      float _596 = _595 + _544;
      float _597 = _596 + (c_ps_user[12].y);
      float _598 = _597 + _547;
      float _599 = _598 + _565;
      float _600 = _599 + _578;
      float _601 = _600 + _563;
      float _602 = _601 + _590;
      float _603 = _602 + _594;
      float _604 = c_specular_scale.y * _584;
      float _605 = _604 * _294;
      float _606 = _571 + (c_ps_user[13].x);
      float _607 = _606 + _575;
      float _608 = c_diffuse_scale.y * _607;
      float _609 = _608 * c_mat_diffuse.y;
      float _610 = _591 * c_specular_scale.y;
      float _611 = _610 + _605;
      float _612 = _611 * c_mat_specular.y;
      float _613 = _544 + _519;
      float _614 = _613 + _532;
      float _615 = _614 + (c_ps_user[12].y);
      float _616 = _615 + _547;
      float _617 = _616 + _566;
      float _618 = _617 + _579;
      float _619 = _618 + _563;
      float _620 = _619 + _609;
      float _621 = _620 + _612;
      float _622 = c_specular_scale.z * _584;
      float _623 = _622 * _295;
      float _624 = _572 + (c_ps_user[13].x);
      float _625 = _624 + _576;
      float _626 = c_diffuse_scale.z * _625;
      float _627 = _626 * c_mat_diffuse.z;
      float _628 = _591 * c_specular_scale.z;
      float _629 = _628 + _623;
      float _630 = _629 * c_mat_specular.z;
      float _631 = _544 + _520;
      float _632 = _631 + _533;
      float _633 = _632 + (c_ps_user[12].y);
      float _634 = _633 + _547;
      float _635 = _634 + _567;
      float _636 = _635 + _580;
      float _637 = _636 + _563;
      float _638 = _637 + _627;
      float _639 = _638 + _630;
      float _640 = saturate(_603);
      float _641 = saturate(_621);
      float _642 = saturate(_639);
      _669 = _640;
      _670 = _641;
      _671 = _642;
   }
   else
   {
      float _644 = _491 * _166;
      float _645 = _514 + _366;
      float _646 = _645 + _498;
      float _647 = _646 + _644;
      float _648 = _490 * _166;
      float _649 = _512 + _365;
      float _650 = _649 + _497;
      float _651 = _650 + _648;
      float _652 = _489 * _166;
      float _653 = _510 + _364;
      float _654 = _653 + _496;
      float _655 = _654 + _652;
      float _656 = c_diffuse_scale.x * c_mat_diffuse.x;
      float _657 = _656 * _68;
      float _658 = _657 * _655;
      float _659 = c_diffuse_scale.y * c_mat_diffuse.y;
      float _660 = _659 * _69;
      float _661 = _660 * _651;
      float _662 = c_diffuse_scale.z * c_mat_diffuse.z;
      float _663 = _662 * _70;
      float _664 = _663 * _647;
      float _665 = _658 + _293;
      float _666 = _661 + _294;
      float _667 = _664 + _295;
      _669 = _665;
      _670 = _666;
      _671 = _667;
   }
   float _676 = c_mat_opacity * _57.w;
   float _677 = _676 * c_opacity_scale;
   float _678 = _677 * c_global_opacity;
   float _679 = _669 * COLOR.x;
   float _680 = _670 * COLOR.y;
   float _681 = _671 * COLOR.z;
   int _682 = c_bool & 16;
   bool _683 = (_682 == 0);
   if (!_683)
   {
      float _687 = c_tone_control.x * _679;
      float _688 = c_tone_control.x * _680;
      float _689 = c_tone_control.x * _681;
      float _691 = log2(_687);
      float _692 = log2(_688);
      float _693 = log2(_689);
      float _694 = _691 * c_tone_control.y;
      float _695 = _692 * c_tone_control.y;
      float _696 = _693 * c_tone_control.y;
      float _697 = exp2(_694);
      float _698 = exp2(_695);
      float _699 = exp2(_696);
      _747 = _697;
      _748 = _698;
      _749 = _699;
   }
   else
   {
      if (_500)
      {
         float _704 = c_tone_control.x * _679;
         float _705 = c_tone_control.x * _680;
         float _706 = c_tone_control.x * _681;
         float _707 = _704 * 2.0f;
         float _708 = _705 * 2.0f;
         float _709 = _706 * 2.0f;
         float _710 = _704 * 0.30000001192092896f;
         float _711 = _705 * 0.30000001192092896f;
         float _712 = _706 * 0.30000001192092896f;
         float _713 = _710 + 0.05000000074505806f;
         float _714 = _711 + 0.05000000074505806f;
         float _715 = _712 + 0.05000000074505806f;
         float _716 = _713 * _707;
         float _717 = _714 * _708;
         float _718 = _715 * _709;
         float _719 = _716 + 0.004000000189989805f;
         float _720 = _717 + 0.004000000189989805f;
         float _721 = _718 + 0.004000000189989805f;
         float _722 = _710 + 0.5f;
         float _723 = _711 + 0.5f;
         float _724 = _712 + 0.5f;
         float _725 = _722 * _707;
         float _726 = _723 * _708;
         float _727 = _724 * _709;
         float _728 = _725 + 0.06000000238418579f;
         float _729 = _726 + 0.06000000238418579f;
         float _730 = _727 + 0.06000000238418579f;
         float _731 = _719 / _728;
         float _732 = _720 / _729;
         float _733 = _721 / _730;
         float _734 = _731 + -0.06666666269302368f;
         float _735 = _732 + -0.06666666269302368f;
         float _736 = _733 + -0.06666666269302368f;
         float _737 = _734 * 1.3790643215179443f;
         float _738 = _735 * 1.3790643215179443f;
         float _739 = _736 * 1.3790643215179443f;
         float _740 = YKR_MaterialCurve(_737, _704);
         float _741 = YKR_MaterialCurve(_738, _705);
         float _742 = YKR_MaterialCurve(_739, _706);
         float _743 = sqrt(_740);
         float _744 = sqrt(_741);
         float _745 = sqrt(_742);
         _747 = _743;
         _748 = _744;
         _749 = _745;
      }
      else
      {
         _747 = _679;
         _748 = _680;
         _749 = _681;
      }
   }
   int _750 = c_bool & 4;
   bool _751 = (_750 == 0);
   if (_751)
   {
      float _753 = saturate(COLOR_1.x);
      float _758 = (c_fog_param[0].x) - _747;
      float _759 = (c_fog_param[0].y) - _748;
      float _760 = (c_fog_param[0].z) - _749;
      float _761 = _758 * _753;
      float _762 = _759 * _753;
      float _763 = _760 * _753;
      _765 = _761;
      _766 = _762;
      _767 = _763;
   }
   else
   {
      _765 = COLOR_1.x;
      _766 = COLOR_1.y;
      _767 = COLOR_1.z;
   }
   float _768 = _765 + _747;
   float _769 = _766 + _748;
   float _770 = _767 + _749;
   if (!_500)
   {
      float _778 = (c_ps_user[3].x) * (c_ps_user[12].z);
      float _779 = (c_ps_user[3].y) * (c_ps_user[12].z);
      float _780 = (c_ps_user[3].z) * (c_ps_user[12].z);
      float _782 = (c_ps_user[12].w) * COLOR.x;
      float _783 = (c_ps_user[12].w) * COLOR.y;
      float _784 = (c_ps_user[12].w) * COLOR.z;
      float _785 = _782 + _768;
      float _786 = _785 + _778;
      float _787 = _783 + _769;
      float _788 = _787 + _779;
      float _789 = _784 + _770;
      float _790 = _789 + _780;
      _792 = _786;
      _793 = _788;
      _794 = _790;
   }
   else
   {
      _792 = _768;
      _793 = _769;
      _794 = _770;
   }
   float _797 = _678 - c_mat_punchthrough_ref;
   bool _798 = (_797 < 0.0f);
   if (_798)
      discard;
   float _804 = (c_ps_col_scl[0].x) * _792;
   float _805 = (c_ps_col_scl[0].y) * _793;
   float _806 = (c_ps_col_scl[0].z) * _794;
   float _807 = (c_ps_col_scl[0].w) * _678;
   SV_Target.x = _804;
   SV_Target.y = _805;
   SV_Target.z = _806;
   SV_Target.w = _807;
   int _810 = c_bool & 8;
   bool _811 = (_810 != 0);
   float _812 = select(_811, _792, _804);
   float _813 = select(_811, _793, _805);
   float _814 = select(_811, _794, _806);
   float _815 = select(_811, _678, _807);
   SV_Target.x = _812;
   SV_Target.y = _813;
   SV_Target.z = _814;
   SV_Target.w = _815;
   return SV_Target;
}
