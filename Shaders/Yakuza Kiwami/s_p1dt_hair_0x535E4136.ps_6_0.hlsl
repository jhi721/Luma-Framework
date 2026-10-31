// Yakuza Kiwami material s_p1dt[hair] (0x535E4136, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
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
   float _59 = _54.x * _54.x;
   float _60 = _54.y * _54.y;
   float _61 = _54.z * _54.z;
   float4 _62 = s5_t.Sample(s5_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _67 = _62.x * 2.0f;
   float _68 = _62.y * 2.0f;
   float _69 = _67 + -1.0f;
   float _70 = _68 + -1.0f;
   float _71 = dot(float2(_69, _70), float2(_69, _70));
   float _72 = 1.0f - _71;
   float _73 = saturate(_72);
   float _74 = sqrt(_73);
   float _75 = TEXCOORD_2.y * TEXCOORD_3.z;
   float _76 = TEXCOORD_2.z * TEXCOORD_3.y;
   float _77 = _75 - _76;
   float _78 = TEXCOORD_2.z * TEXCOORD_3.x;
   float _79 = TEXCOORD_2.x * TEXCOORD_3.z;
   float _80 = _78 - _79;
   float _81 = TEXCOORD_2.x * TEXCOORD_3.y;
   float _82 = TEXCOORD_2.y * TEXCOORD_3.x;
   float _83 = _81 - _82;
   float _84 = _69 * TEXCOORD_3.x;
   float _85 = _69 * TEXCOORD_3.y;
   float _86 = _69 * TEXCOORD_3.z;
   float _87 = _70 * TEXCOORD_3.w;
   float _88 = _87 * _77;
   float _89 = _87 * _80;
   float _90 = _87 * _83;
   float _91 = _88 + _84;
   float _92 = _89 + _85;
   float _93 = _90 + _86;
   float _94 = _74 * TEXCOORD_2.x;
   float _95 = _74 * TEXCOORD_2.y;
   float _96 = _74 * TEXCOORD_2.z;
   float _97 = _91 + _94;
   float _98 = _92 + _95;
   float _99 = _93 + _96;
   float _100 = dot(float3(_97, _98, _99), float3(_97, _98, _99));
   float _101 = rsqrt(_100);
   float _102 = _97 * _101;
   float _103 = _98 * _101;
   float _104 = _99 * _101;
   float _109 = dot(float3(TEXCOORD_1.x, TEXCOORD_1.y, TEXCOORD_1.z), float3(TEXCOORD_1.x, TEXCOORD_1.y, TEXCOORD_1.z));
   float _110 = rsqrt(_109);
   float _111 = _110 * TEXCOORD_1.x;
   float _112 = _110 * TEXCOORD_1.y;
   float _113 = _110 * TEXCOORD_1.z;
   float _114 = _111 + (c_light_vec[0].x);
   float _115 = _112 + (c_light_vec[0].y);
   float _116 = _113 + (c_light_vec[0].z);
   float _117 = dot(float3(_114, _115, _116), float3(_114, _115, _116));
   float _118 = rsqrt(_117);
   float _119 = _114 * _118;
   float _120 = _115 * _118;
   float _121 = _116 * _118;
   float _122 = _103 + 0.10000000149011612f;
   float _123 = saturate(_122);
   float _126 = (c_attribute_ps[2].w) + 2.0f;
   float _128 = (c_attribute_ps[2].z) + 2.5f;
   float _129 = -0.0f - _111;
   float _130 = -0.0f - _112;
   float _131 = -0.0f - _113;
   float _132 = dot(float3(_129, _130, _131), float3(TEXCOORD_2.x, TEXCOORD_2.y, TEXCOORD_2.z));
   float _133 = 1.0f - _132;
   float _134 = saturate(_133);
   float _135 = dot(float3(_129, _130, _131), float3(_102, _103, _104));
   float _136 = 1.0f - _135;
   float _137 = saturate(_136);
   float _138 = _137 * _134;
   float _139 = log2(_138);
   float _140 = _139 * _126;
   float _141 = exp2(_140);
   float _142 = _141 * _128;
   int _145 = c_bool & 4096;
   bool _146 = (_145 == 0);
   float _155;
   float _178;
   float _179;
   float _191;
   float _192;
   float _357;
   float _358;
   float _359;
   float _388;
   float _389;
   float _390;
   float _691;
   float _692;
   float _693;
   float _769;
   float _770;
   float _771;
   float _787;
   float _788;
   float _789;
   float _814;
   float _815;
   float _816;
   if (!_146)
   {
      float _151 = (c_ps_user[9].w) * _139;
      float _152 = exp2(_151);
      float _153 = _152 * (c_ps_user[9].z);
      _155 = _153;
   }
   else
   {
      _155 = _142;
   }
   float _156 = _123 * _155;
   float _159 = (c_ps_user[2].x) * _156;
   float _160 = _159 + 1.0f;
   float _161 = _59 * _160;
   float _162 = _60 * _160;
   float _163 = _61 * _160;
   float _167 = c_rt_size.z * SV_Position.x;
   float _168 = c_rt_size.w * SV_Position.y;
   int _171 = c_bool & 8;
   bool _172 = (_171 == 0);
   if (!_172)
   {
      float4 _174 = s13_t.Sample(s13_s, float2(_167, _168));
      _178 = _174.x;
      _179 = _174.y;
   }
   else
   {
      _178 = 1.0f;
      _179 = 1.0f;
   }
   if (!_172)
   {
      float _184 = (c_sdw_param_ch[0].x) * _178;
      float _185 = (c_sdw_param_ch[0].z) * _179;
      float _188 = _184 + (c_sdw_param_ch[0].y);
      float _189 = _185 + (c_sdw_param_ch[0].w);
      _191 = _188;
      _192 = _189;
   }
   else
   {
      _191 = _178;
      _192 = _179;
   }
   float _193 = min(_191, _192);
   float _198 = (c_light_vec[1].x) + _111;
   float _199 = (c_light_vec[1].y) + _112;
   float _200 = (c_light_vec[1].z) + _113;
   float _201 = dot(float3(_198, _199, _200), float3(_198, _199, _200));
   float _202 = rsqrt(_201);
   float _203 = _198 * _202;
   float _204 = _199 * _202;
   float _205 = _200 * _202;
   float _206 = _103 * TEXCOORD_3.z;
   float _207 = _104 * TEXCOORD_3.y;
   float _208 = _206 - _207;
   float _209 = _104 * TEXCOORD_3.x;
   float _210 = _102 * TEXCOORD_3.z;
   float _211 = _209 - _210;
   float _212 = _102 * TEXCOORD_3.y;
   float _213 = _103 * TEXCOORD_3.x;
   float _214 = _212 - _213;
   float _215 = _208 * TEXCOORD_3.w;
   float _216 = _211 * TEXCOORD_3.w;
   float _217 = _214 * TEXCOORD_3.w;
   float _218 = _216 * _104;
   float _219 = _217 * _103;
   float _220 = _218 - _219;
   float _221 = _217 * _102;
   float _222 = _215 * _104;
   float _223 = _221 - _222;
   float _224 = _215 * _103;
   float _225 = _216 * _102;
   float _226 = _224 - _225;
   float _227 = -0.0f - _119;
   float _228 = -0.0f - _120;
   float _229 = -0.0f - _121;
   float _230 = dot(float3(_102, _103, _104), float3(_227, _228, _229));
   float _231 = dot(float3(_227, _228, _229), float3(_215, _216, _217));
   float _232 = TEXCOORD.z * 0.20000000298023224f;
   float _233 = _231 * _232;
   float _234 = _233 + 0.5f;
   float4 _235 = s7_t.Sample(s7_s, float2(_230, _234));
   float _237 = dot(float3(_227, _228, _229), float3(_220, _223, _226));
   float _238 = TEXCOORD.w * 0.20000000298023224f;
   float _239 = _237 * _238;
   float _240 = _239 + 0.5f;
   float4 _241 = s7_t.Sample(s7_s, float2(_230, _240));
   float _243 = _241.x * _235.x;
   float _244 = -0.0f - _203;
   float _245 = -0.0f - _204;
   float _246 = -0.0f - _205;
   float _247 = dot(float3(_102, _103, _104), float3(_244, _245, _246));
   float _248 = dot(float3(_244, _245, _246), float3(_215, _216, _217));
   float _249 = _248 * _232;
   float _250 = _249 + 0.5f;
   float4 _251 = s7_t.Sample(s7_s, float2(_247, _250));
   float _253 = dot(float3(_244, _245, _246), float3(_220, _223, _226));
   float _254 = _253 * _238;
   float _255 = _254 + 0.5f;
   float4 _256 = s7_t.Sample(s7_s, float2(_247, _255));
   float _258 = _256.x * _251.x;
   float _259 = log2(_243);
   float _260 = log2(_258);
   float _261 = _259 * c_mat_power;
   float _262 = _260 * c_mat_power;
   float _263 = exp2(_261);
   float _264 = exp2(_262);
   float _269 = c_mat_fresnel * c_tone_control.w;
   float _270 = c_mat_power * 0.03974359855055809f;
   float _271 = _270 + 0.0856831967830658f;
   float _272 = _264 * _271;
   float _273 = dot(float3(_111, _112, _113), float3(_119, _120, _121));
   float _274 = dot(float3(_111, _112, _113), float3(_203, _204, _205));
   float _275 = 1.0f - _273;
   float _276 = 1.0f - _274;
   float _277 = _275 * _275;
   float _278 = _276 * _276;
   float _279 = 1.0f - _269;
   float _280 = _277 * _277;
   float _281 = _280 * _275;
   float _282 = _281 * _279;
   float _283 = _276 * _279;
   float _284 = _278 * _278;
   float _285 = _284 * _283;
   float _286 = _282 + _269;
   float _287 = _285 + _269;
   float _288 = _272 * _287;
   float _297 = c_specular_scale.x * c_mat_specular.x;
   float _298 = c_specular_scale.y * c_mat_specular.y;
   float _299 = c_specular_scale.z * c_mat_specular.z;
   float _304 = _193 * _271;
   float _305 = _304 * _263;
   float _306 = _305 * _286;
   float _307 = _306 * (c_light_col[0].x);
   float _308 = _306 * (c_light_col[0].y);
   float _309 = _306 * (c_light_col[0].z);
   float _314 = _288 * (c_light_col[1].x);
   float _315 = _288 * (c_light_col[1].y);
   float _316 = _288 * (c_light_col[1].z);
   float _317 = _307 + _314;
   float _318 = _308 + _315;
   float _319 = _309 + _316;
   float _320 = _297 * _317;
   float _321 = _298 * _318;
   float _322 = _299 * _319;
   int _328 = c_bool & 1;
   bool _329 = (_328 == 0);
   if (!_329)
   {
      float _331 = -0.0f - TEXCOORD_4.x;
      float _332 = -0.0f - TEXCOORD_4.y;
      float _333 = -0.0f - TEXCOORD_4.z;
      float _334 = dot(float3(_102, _103, _104), float3(_331, _332, _333));
      float _335 = saturate(_334);
      float _336 = _335 * TEXCOORD_1.w;
      float _337 = _335 * TEXCOORD_2.w;
      float _338 = _335 * TEXCOORD_4.w;
      float _339 = max(0.0f, _336);
      float _340 = max(0.0f, _337);
      float _341 = max(0.0f, _338);
      float _342 = -0.0f - TEXCOORD_5.x;
      float _343 = -0.0f - TEXCOORD_5.y;
      float _344 = -0.0f - TEXCOORD_5.z;
      float _345 = dot(float3(_102, _103, _104), float3(_342, _343, _344));
      float _346 = saturate(_345);
      float _347 = _346 * COLOR.w;
      float _348 = _346 * COLOR_1.w;
      float _349 = _346 * TEXCOORD_5.w;
      float _350 = max(0.0f, _347);
      float _351 = max(0.0f, _348);
      float _352 = max(0.0f, _349);
      float _353 = _350 + _339;
      float _354 = _351 + _340;
      float _355 = _352 + _341;
      _357 = _353;
      _358 = _354;
      _359 = _355;
   }
   else
   {
      _357 = 0.0f;
      _358 = 0.0f;
      _359 = 0.0f;
   }
   float _360 = c_rt_size.z * SV_Position.x;
   float _361 = c_rt_size.w * SV_Position.y;
   float4 _362 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_360, _361)));
   int _368 = c_bool & 16384;
   bool _369 = (_368 == 0);
   if (!_369)
   {
      float _371 = _362.x * 4.0f;
      float _372 = _362.y * 4.0f;
      float _373 = _362.z * 4.0f;
      _388 = _371;
      _389 = _372;
      _390 = _373;
   }
   else
   {
      int _375 = c_bool & 8192;
      bool _376 = (_375 == 0);
      if (!_376)
      {
         float _378 = _362.x * 2.0f;
         float _379 = _362.y * 2.0f;
         float _380 = _362.z * 2.0f;
         _388 = _378;
         _389 = _379;
         _390 = _380;
      }
      else
      {
         float _384 = _362.x / c_tone_control.z;
         float _385 = _362.y / c_tone_control.z;
         float _386 = _362.z / c_tone_control.z;
         _388 = _384;
         _389 = _385;
         _390 = _386;
      }
   }
   float _391 = _388 + _357;
   float _392 = _389 + _358;
   float _393 = _390 + _359;
   float _456 = dot(float3(_102, _103, _104), float3(_102, _103, _104));
   float _457 = rsqrt(_456);
   float _458 = _457 * _102;
   float _459 = _457 * _103;
   float _460 = _457 * _104;
   float _461 = dot(float4((c_ps_scratch[80].x), (c_ps_scratch[80].y), (c_ps_scratch[80].z), (c_ps_scratch[80].w)), float4(_458, _459, _460, 1.0f));
   float _462 = dot(float4((c_ps_scratch[81].x), (c_ps_scratch[81].y), (c_ps_scratch[81].z), (c_ps_scratch[81].w)), float4(_458, _459, _460, 1.0f));
   float _463 = dot(float4((c_ps_scratch[82].x), (c_ps_scratch[82].y), (c_ps_scratch[82].z), (c_ps_scratch[82].w)), float4(_458, _459, _460, 1.0f));
   float _464 = dot(float4((c_ps_scratch[83].x), (c_ps_scratch[83].y), (c_ps_scratch[83].z), (c_ps_scratch[83].w)), float4(_458, _459, _460, 1.0f));
   float _465 = dot(float4(_458, _459, _460, 1.0f), float4(_461, _462, _463, _464));
   float _466 = dot(float4((c_ps_scratch[84].x), (c_ps_scratch[84].y), (c_ps_scratch[84].z), (c_ps_scratch[84].w)), float4(_458, _459, _460, 1.0f));
   float _467 = dot(float4((c_ps_scratch[85].x), (c_ps_scratch[85].y), (c_ps_scratch[85].z), (c_ps_scratch[85].w)), float4(_458, _459, _460, 1.0f));
   float _468 = dot(float4((c_ps_scratch[86].x), (c_ps_scratch[86].y), (c_ps_scratch[86].z), (c_ps_scratch[86].w)), float4(_458, _459, _460, 1.0f));
   float _469 = dot(float4((c_ps_scratch[87].x), (c_ps_scratch[87].y), (c_ps_scratch[87].z), (c_ps_scratch[87].w)), float4(_458, _459, _460, 1.0f));
   float _470 = dot(float4(_458, _459, _460, 1.0f), float4(_466, _467, _468, _469));
   float _471 = dot(float4((c_ps_scratch[88].x), (c_ps_scratch[88].y), (c_ps_scratch[88].z), (c_ps_scratch[88].w)), float4(_458, _459, _460, 1.0f));
   float _472 = dot(float4((c_ps_scratch[89].x), (c_ps_scratch[89].y), (c_ps_scratch[89].z), (c_ps_scratch[89].w)), float4(_458, _459, _460, 1.0f));
   float _473 = dot(float4((c_ps_scratch[90].x), (c_ps_scratch[90].y), (c_ps_scratch[90].z), (c_ps_scratch[90].w)), float4(_458, _459, _460, 1.0f));
   float _474 = dot(float4((c_ps_scratch[91].x), (c_ps_scratch[91].y), (c_ps_scratch[91].z), (c_ps_scratch[91].w)), float4(_458, _459, _460, 1.0f));
   float _475 = dot(float4(_458, _459, _460, 1.0f), float4(_471, _472, _473, _474));
   float _476 = max(_465, 0.0625f);
   float _477 = max(_470, 0.0625f);
   float _478 = max(_475, 0.0625f);
   float _479 = _476 * (c_ps_scratch[79].x);
   float _480 = _477 * (c_ps_scratch[79].x);
   float _481 = _478 * (c_ps_scratch[79].x);
   float _486 = -0.0f - (c_light_vec[0].x);
   float _487 = -0.0f - (c_light_vec[0].y);
   float _488 = -0.0f - (c_light_vec[0].z);
   float _489 = dot(float3(_102, _103, _104), float3(_486, _487, _488));
   float _490 = -0.0f - (c_light_vec[1].x);
   float _491 = -0.0f - (c_light_vec[1].y);
   float _492 = -0.0f - (c_light_vec[1].z);
   float _493 = dot(float3(_102, _103, _104), float3(_490, _491, _492));
   float _502 = _103 * 0.5f;
   float _503 = _502 + 0.5f;
   float _504 = (c_light_col[4].x) - (c_light_vec[4].x);
   float _505 = (c_light_col[4].y) - (c_light_vec[4].y);
   float _506 = (c_light_col[4].z) - (c_light_vec[4].z);
   float _507 = _504 * _503;
   float _508 = _505 * _503;
   float _509 = _506 * _503;
   float _510 = saturate(_489);
   float _511 = saturate(_493);
   float _516 = (c_light_col[0].x) * _510;
   float _517 = (c_light_col[0].y) * _510;
   float _518 = (c_light_col[0].z) * _510;
   float _523 = (c_light_col[1].x) * _511;
   float _524 = (c_light_col[1].y) * _511;
   float _525 = (c_light_col[1].z) * _511;
   int _526 = c_bool & 32768;
   bool _527 = (_526 == 0);
   float _536 = (c_light_vec[4].x) + _479;
   float _537 = _536 + _507;
   float _538 = (c_light_vec[4].y) + _480;
   float _539 = _538 + _508;
   float _540 = (c_light_vec[4].z) + _481;
   float _541 = _540 + _509;
   if (!_527)
   {
      float _545 = (c_ps_user[11].x) * _161;
      float _546 = (c_ps_user[11].x) * _162;
      float _547 = (c_ps_user[11].x) * _163;
      float _549 = _102 + 1.0f;
      float _550 = _103 + 1.0f;
      float _551 = _104 + 1.0f;
      float _552 = _549 * 0.5f;
      float _553 = _550 * 0.5f;
      float _554 = _551 * 0.5f;
      float _555 = saturate(_552);
      float _556 = saturate(_553);
      float _557 = saturate(_554);
      float _558 = (c_ps_user[11].y) * _555;
      float _559 = _556 * (c_ps_user[11].y);
      float _560 = _557 * (c_ps_user[11].y);
      float _571 = (c_ps_user[11].w) * _320;
      float _574 = (c_ps_user[12].x) * _193;
      float _580 = _136 * _136;
      float _581 = _136 * 0.9722222089767456f;
      float _582 = _580 * _580;
      float _583 = _582 * _581;
      float _584 = _583 + 0.02777777425944805f;
      float _585 = (c_ps_user[13].z) * _584;
      float _587 = (c_ps_user[13].w) * _537;
      float _588 = (c_ps_user[13].w) * _539;
      float _589 = (c_ps_user[13].w) * _541;
      float _592 = (c_ps_user[14].y) * _516;
      float _593 = (c_ps_user[14].y) * _517;
      float _594 = (c_ps_user[14].y) * _518;
      float _596 = (c_ps_user[14].z) * _523;
      float _597 = (c_ps_user[14].z) * _524;
      float _598 = (c_ps_user[14].z) * _525;
      float _600 = (c_ps_user[14].w) * _391;
      float _601 = (c_ps_user[14].w) * _392;
      float _602 = (c_ps_user[14].w) * _393;
      float _606 = (c_ps_user[15].y) + (c_ps_user[15].x);
      float _607 = c_specular_scale.x * _606;
      float _608 = _607 * _320;
      float _609 = _592 + (c_ps_user[13].x);
      float _610 = _609 + _596;
      float _611 = c_diffuse_scale.x * _610;
      float _612 = _611 * c_mat_diffuse.x;
      float _613 = (c_ps_user[13].y) + (c_ps_user[11].z);
      float _614 = _613 * c_specular_scale.x;
      float _615 = _614 + _608;
      float _616 = _615 * c_mat_specular.x;
      float _617 = _558 + _545;
      float _618 = _617 + _571;
      float _619 = _618 + (c_ps_user[12].y);
      float _620 = _619 + _574;
      float _621 = _620 + _587;
      float _622 = _621 + _585;
      float _623 = _622 + _600;
      float _624 = _623 + _612;
      float _625 = _624 + _616;
      float _626 = c_specular_scale.y * _606;
      float _627 = _626 * _321;
      float _628 = _593 + (c_ps_user[13].x);
      float _629 = _628 + _597;
      float _630 = c_diffuse_scale.y * _629;
      float _631 = _630 * c_mat_diffuse.y;
      float _632 = _613 * c_specular_scale.y;
      float _633 = _632 + _627;
      float _634 = _633 * c_mat_specular.y;
      float _635 = _571 + _546;
      float _636 = _635 + _559;
      float _637 = _636 + (c_ps_user[12].y);
      float _638 = _637 + _574;
      float _639 = _638 + _588;
      float _640 = _639 + _585;
      float _641 = _640 + _601;
      float _642 = _641 + _631;
      float _643 = _642 + _634;
      float _644 = c_specular_scale.z * _606;
      float _645 = _644 * _322;
      float _646 = _594 + (c_ps_user[13].x);
      float _647 = _646 + _598;
      float _648 = c_diffuse_scale.z * _647;
      float _649 = _648 * c_mat_diffuse.z;
      float _650 = _613 * c_specular_scale.z;
      float _651 = _650 + _645;
      float _652 = _651 * c_mat_specular.z;
      float _653 = _571 + _547;
      float _654 = _653 + _560;
      float _655 = _654 + (c_ps_user[12].y);
      float _656 = _655 + _574;
      float _657 = _656 + _589;
      float _658 = _657 + _585;
      float _659 = _658 + _602;
      float _660 = _659 + _649;
      float _661 = _660 + _652;
      float _662 = saturate(_625);
      float _663 = saturate(_643);
      float _664 = saturate(_661);
      _691 = _662;
      _692 = _663;
      _693 = _664;
   }
   else
   {
      float _666 = _518 * _193;
      float _667 = _541 + _393;
      float _668 = _667 + _525;
      float _669 = _668 + _666;
      float _670 = _517 * _193;
      float _671 = _539 + _392;
      float _672 = _671 + _524;
      float _673 = _672 + _670;
      float _674 = _516 * _193;
      float _675 = _537 + _391;
      float _676 = _675 + _523;
      float _677 = _676 + _674;
      float _678 = c_diffuse_scale.x * c_mat_diffuse.x;
      float _679 = _678 * _161;
      float _680 = _679 * _677;
      float _681 = c_diffuse_scale.y * c_mat_diffuse.y;
      float _682 = _681 * _162;
      float _683 = _682 * _673;
      float _684 = c_diffuse_scale.z * c_mat_diffuse.z;
      float _685 = _684 * _163;
      float _686 = _685 * _669;
      float _687 = _680 + _320;
      float _688 = _683 + _321;
      float _689 = _686 + _322;
      _691 = _687;
      _692 = _688;
      _693 = _689;
   }
   float _698 = c_mat_opacity * _54.w;
   float _699 = _698 * c_opacity_scale;
   float _700 = _699 * c_global_opacity;
   float _701 = _691 * COLOR.x;
   float _702 = _692 * COLOR.y;
   float _703 = _693 * COLOR.z;
   int _704 = c_bool & 16;
   bool _705 = (_704 == 0);
   if (!_705)
   {
      float _709 = c_tone_control.x * _701;
      float _710 = c_tone_control.x * _702;
      float _711 = c_tone_control.x * _703;
      float _713 = log2(_709);
      float _714 = log2(_710);
      float _715 = log2(_711);
      float _716 = _713 * c_tone_control.y;
      float _717 = _714 * c_tone_control.y;
      float _718 = _715 * c_tone_control.y;
      float _719 = exp2(_716);
      float _720 = exp2(_717);
      float _721 = exp2(_718);
      _769 = _719;
      _770 = _720;
      _771 = _721;
   }
   else
   {
      if (_527)
      {
         float _726 = c_tone_control.x * _701;
         float _727 = c_tone_control.x * _702;
         float _728 = c_tone_control.x * _703;
         float _729 = _726 * 2.0f;
         float _730 = _727 * 2.0f;
         float _731 = _728 * 2.0f;
         float _732 = _726 * 0.30000001192092896f;
         float _733 = _727 * 0.30000001192092896f;
         float _734 = _728 * 0.30000001192092896f;
         float _735 = _732 + 0.05000000074505806f;
         float _736 = _733 + 0.05000000074505806f;
         float _737 = _734 + 0.05000000074505806f;
         float _738 = _735 * _729;
         float _739 = _736 * _730;
         float _740 = _737 * _731;
         float _741 = _738 + 0.004000000189989805f;
         float _742 = _739 + 0.004000000189989805f;
         float _743 = _740 + 0.004000000189989805f;
         float _744 = _732 + 0.5f;
         float _745 = _733 + 0.5f;
         float _746 = _734 + 0.5f;
         float _747 = _744 * _729;
         float _748 = _745 * _730;
         float _749 = _746 * _731;
         float _750 = _747 + 0.06000000238418579f;
         float _751 = _748 + 0.06000000238418579f;
         float _752 = _749 + 0.06000000238418579f;
         float _753 = _741 / _750;
         float _754 = _742 / _751;
         float _755 = _743 / _752;
         float _756 = _753 + -0.06666666269302368f;
         float _757 = _754 + -0.06666666269302368f;
         float _758 = _755 + -0.06666666269302368f;
         float _759 = _756 * 1.3790643215179443f;
         float _760 = _757 * 1.3790643215179443f;
         float _761 = _758 * 1.3790643215179443f;
         float _762 = YKR_MaterialCurve(_759, _726);
         float _763 = YKR_MaterialCurve(_760, _727);
         float _764 = YKR_MaterialCurve(_761, _728);
         float _765 = sqrt(_762);
         float _766 = sqrt(_763);
         float _767 = sqrt(_764);
         _769 = _765;
         _770 = _766;
         _771 = _767;
      }
      else
      {
         _769 = _701;
         _770 = _702;
         _771 = _703;
      }
   }
   int _772 = c_bool & 4;
   bool _773 = (_772 == 0);
   if (_773)
   {
      float _775 = saturate(COLOR_1.x);
      float _780 = (c_fog_param[0].x) - _769;
      float _781 = (c_fog_param[0].y) - _770;
      float _782 = (c_fog_param[0].z) - _771;
      float _783 = _780 * _775;
      float _784 = _781 * _775;
      float _785 = _782 * _775;
      _787 = _783;
      _788 = _784;
      _789 = _785;
   }
   else
   {
      _787 = COLOR_1.x;
      _788 = COLOR_1.y;
      _789 = COLOR_1.z;
   }
   float _790 = _787 + _769;
   float _791 = _788 + _770;
   float _792 = _789 + _771;
   if (!_527)
   {
      float _800 = (c_ps_user[3].x) * (c_ps_user[12].z);
      float _801 = (c_ps_user[3].y) * (c_ps_user[12].z);
      float _802 = (c_ps_user[3].z) * (c_ps_user[12].z);
      float _804 = (c_ps_user[12].w) * COLOR.x;
      float _805 = (c_ps_user[12].w) * COLOR.y;
      float _806 = (c_ps_user[12].w) * COLOR.z;
      float _807 = _804 + _790;
      float _808 = _807 + _800;
      float _809 = _805 + _791;
      float _810 = _809 + _801;
      float _811 = _806 + _792;
      float _812 = _811 + _802;
      _814 = _808;
      _815 = _810;
      _816 = _812;
   }
   else
   {
      _814 = _790;
      _815 = _791;
      _816 = _792;
   }
   float _819 = _700 - c_mat_punchthrough_ref;
   bool _820 = (_819 < 0.0f);
   if (_820)
      discard;
   float _826 = (c_ps_col_scl[0].x) * _814;
   float _827 = (c_ps_col_scl[0].y) * _815;
   float _828 = (c_ps_col_scl[0].z) * _816;
   float _829 = (c_ps_col_scl[0].w) * _700;
   SV_Target.x = _826;
   SV_Target.y = _827;
   SV_Target.z = _828;
   SV_Target.w = _829;
   int _832 = c_bool & 8;
   bool _833 = (_832 != 0);
   float _834 = select(_833, _814, _826);
   float _835 = select(_833, _815, _827);
   float _836 = select(_833, _816, _828);
   float _837 = select(_833, _700, _829);
   SV_Target.x = _834;
   SV_Target.y = _835;
   SV_Target.z = _836;
   SV_Target.w = _837;
   return SV_Target;
}
