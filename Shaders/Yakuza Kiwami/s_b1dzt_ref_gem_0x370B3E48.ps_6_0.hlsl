// Yakuza Kiwami material s_b1dzt[ref][gem] (0x370B3E48, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
// applied by a script: the inline Hable tonemap's saturate goes through
// YKR_MaterialCurve (tangent continuation in HDR), and the light pre-pass buffer t14 through YKR_LightBuffer.

#include "Includes/Common.hlsl"

Texture2D<float4> s0_t : register(t0);

TextureCube<float4> s1c_t : register(t1);

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

SamplerState s1c_s : register(s1);

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
   float4 _54 = s2_t.Sample(s2_s, float2(TEXCOORD.x, TEXCOORD.y));
   float4 _59 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _64 = _59.x * _59.x;
   float _65 = _59.y * _59.y;
   float _66 = _59.z * _59.z;
   float4 _67 = s5_t.Sample(s5_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _72 = c_mat_power * _54.z;
   float _73 = max(1.0f, _72);
   float _74 = _67.x * 2.0f;
   float _75 = _67.y * 2.0f;
   float _76 = _74 + -1.0f;
   float _77 = _75 + -1.0f;
   float _78 = dot(float2(_76, _77), float2(_76, _77));
   float _79 = 1.0f - _78;
   float _80 = saturate(_79);
   float _81 = sqrt(_80);
   float _82 = TEXCOORD_2.y * TEXCOORD_3.z;
   float _83 = TEXCOORD_2.z * TEXCOORD_3.y;
   float _84 = _82 - _83;
   float _85 = TEXCOORD_2.z * TEXCOORD_3.x;
   float _86 = TEXCOORD_2.x * TEXCOORD_3.z;
   float _87 = _85 - _86;
   float _88 = TEXCOORD_2.x * TEXCOORD_3.y;
   float _89 = TEXCOORD_2.y * TEXCOORD_3.x;
   float _90 = _88 - _89;
   float _91 = _76 * TEXCOORD_3.x;
   float _92 = _76 * TEXCOORD_3.y;
   float _93 = _76 * TEXCOORD_3.z;
   float _94 = _77 * TEXCOORD_3.w;
   float _95 = _94 * _84;
   float _96 = _94 * _87;
   float _97 = _94 * _90;
   float _98 = _95 + _91;
   float _99 = _96 + _92;
   float _100 = _97 + _93;
   float _101 = _81 * TEXCOORD_2.x;
   float _102 = _81 * TEXCOORD_2.y;
   float _103 = _81 * TEXCOORD_2.z;
   float _104 = _98 + _101;
   float _105 = _99 + _102;
   float _106 = _100 + _103;
   float _107 = dot(float3(_104, _105, _106), float3(_104, _105, _106));
   float _108 = rsqrt(_107);
   float _109 = _104 * _108;
   float _110 = _105 * _108;
   float _111 = _106 * _108;
   float _116 = dot(float3(TEXCOORD_1.x, TEXCOORD_1.y, TEXCOORD_1.z), float3(TEXCOORD_1.x, TEXCOORD_1.y, TEXCOORD_1.z));
   float _117 = rsqrt(_116);
   float _118 = _117 * TEXCOORD_1.x;
   float _119 = _117 * TEXCOORD_1.y;
   float _120 = _117 * TEXCOORD_1.z;
   float _121 = _118 + (c_light_vec[0].x);
   float _122 = _119 + (c_light_vec[0].y);
   float _123 = _120 + (c_light_vec[0].z);
   float _124 = dot(float3(_121, _122, _123), float3(_121, _122, _123));
   float _125 = rsqrt(_124);
   float _126 = _121 * _125;
   float _127 = _122 * _125;
   float _128 = _123 * _125;
   float _129 = dot(float3(_118, _119, _120), float3(_109, _110, _111));
   float _130 = _129 * 2.0f;
   float _131 = _130 * _109;
   float _132 = _130 * _110;
   float _133 = _130 * _111;
   float _134 = _118 - _131;
   float _135 = _119 - _132;
   float _136 = _120 - _133;
   float _137 = dot(float3(_134, _135, _136), float3(_134, _135, _136));
   float _138 = rsqrt(_137);
   float _139 = _135 * _138;
   float _140 = _136 * _138;
   float _141 = _138 * _134;
   float _142 = -0.0f - _141;
   float4 _143 = s1c_t.Sample(s1c_s, float3(_142, _139, _140));
   float _148 = max(_143.w, 0.003921568859368563f);
   float _149 = 0.25f / _148;
   float _150 = _149 * _143.x;
   float _151 = _149 * _143.y;
   float _152 = _149 * _143.z;
   float _153 = _150 * _150;
   float _154 = _151 * _151;
   float _155 = _152 * _152;
   float _156 = _153 - _64;
   float _157 = _154 - _65;
   float _158 = _155 - _66;
   float _159 = _156 * _54.w;
   float _160 = _157 * _54.w;
   float _161 = _158 * _54.w;
   float _162 = _159 + _64;
   float _163 = _160 + _65;
   float _164 = _161 + _66;
   float _168 = c_rt_size.z * SV_Position.x;
   float _169 = c_rt_size.w * SV_Position.y;
   int _172 = c_bool & 8;
   bool _173 = (_172 == 0);
   float _179;
   float _180;
   float _192;
   float _193;
   float _373;
   float _374;
   float _375;
   float _376;
   float _377;
   float _378;
   float _411;
   float _412;
   float _413;
   float _414;
   float _579;
   float _750;
   float _751;
   float _752;
   float _753;
   float _833;
   float _834;
   float _835;
   float _851;
   float _852;
   float _853;
   float _878;
   float _879;
   float _880;
   if (!_173)
   {
      float4 _175 = s13_t.Sample(s13_s, float2(_168, _169));
      _179 = _175.x;
      _180 = _175.y;
   }
   else
   {
      _179 = 1.0f;
      _180 = 1.0f;
   }
   if (!_173)
   {
      float _185 = (c_sdw_param_ch[0].x) * _179;
      float _186 = (c_sdw_param_ch[0].z) * _180;
      float _189 = _185 + (c_sdw_param_ch[0].y);
      float _190 = _186 + (c_sdw_param_ch[0].w);
      _192 = _189;
      _193 = _190;
   }
   else
   {
      _192 = _179;
      _193 = _180;
   }
   float _194 = min(_192, _193);
   float _203 = (c_light_vec[1].x) + _118;
   float _204 = (c_light_vec[1].y) + _119;
   float _205 = (c_light_vec[1].z) + _120;
   float _206 = dot(float3(_203, _204, _205), float3(_203, _204, _205));
   float _207 = rsqrt(_206);
   float _208 = _203 * _207;
   float _209 = _204 * _207;
   float _210 = _205 * _207;
   float _211 = -0.0f - (c_light_vec[1].x);
   float _212 = -0.0f - (c_light_vec[1].y);
   float _213 = -0.0f - (c_light_vec[1].z);
   float _214 = -0.0f - (c_light_vec[0].x);
   float _215 = -0.0f - (c_light_vec[0].y);
   float _216 = -0.0f - (c_light_vec[0].z);
   float _217 = dot(float3(_134, _135, _136), float3(_214, _215, _216));
   float _218 = dot(float3(_134, _135, _136), float3(_211, _212, _213));
   float _219 = saturate(_217);
   float _220 = saturate(_218);
   float _221 = log2(_219);
   float _222 = log2(_220);
   float _223 = _221 * _73;
   float _224 = _222 * _73;
   float _225 = exp2(_223);
   float _226 = exp2(_224);
   float _231 = c_mat_fresnel * c_tone_control.w;
   float _232 = _73 * 0.03974359855055809f;
   float _233 = _232 + 0.0856831967830658f;
   float _234 = _226 * _233;
   float _235 = dot(float3(_118, _119, _120), float3(_126, _127, _128));
   float _236 = dot(float3(_118, _119, _120), float3(_208, _209, _210));
   float _237 = 1.0f - _235;
   float _238 = 1.0f - _236;
   float _239 = _237 * _237;
   float _240 = _238 * _238;
   float _241 = 1.0f - _231;
   float _242 = _239 * _239;
   float _243 = _242 * _237;
   float _244 = _243 * _241;
   float _245 = _238 * _241;
   float _246 = _240 * _240;
   float _247 = _246 * _245;
   float _248 = _244 + _231;
   float _249 = _247 + _231;
   float _250 = dot(float3(_109, _110, _111), float3(_214, _215, _216));
   float _251 = saturate(_250);
   float _252 = dot(float3(_109, _110, _111), float3(_211, _212, _213));
   float _253 = saturate(_252);
   float _254 = _234 * _253;
   float _255 = _254 * _249;
   float _264 = c_specular_scale.x * c_mat_specular.x;
   float _265 = c_specular_scale.y * c_mat_specular.y;
   float _266 = c_specular_scale.z * c_mat_specular.z;
   float _271 = _194 * _233;
   float _272 = _271 * _225;
   float _273 = _272 * _251;
   float _274 = _273 * _248;
   float _275 = (c_light_col[0].x) * _274;
   float _276 = (c_light_col[0].y) * _274;
   float _277 = (c_light_col[0].z) * _274;
   float _282 = (c_light_col[1].x) * _255;
   float _283 = (c_light_col[1].y) * _255;
   float _284 = (c_light_col[1].z) * _255;
   float _285 = _282 + _275;
   float _286 = _283 + _276;
   float _287 = _284 + _277;
   float _288 = _264 * _285;
   float _289 = _265 * _286;
   float _290 = _266 * _287;
   int _294 = c_bool & 1;
   bool _295 = (_294 == 0);
   if (!_295)
   {
      float _297 = -0.0f - TEXCOORD_4.x;
      float _298 = -0.0f - TEXCOORD_4.y;
      float _299 = -0.0f - TEXCOORD_4.z;
      float _300 = dot(float3(_109, _110, _111), float3(_297, _298, _299));
      float _301 = saturate(_300);
      float _302 = _301 * TEXCOORD_1.w;
      float _303 = _301 * TEXCOORD_2.w;
      float _304 = _301 * TEXCOORD_4.w;
      float _305 = max(0.0f, _302);
      float _306 = max(0.0f, _303);
      float _307 = max(0.0f, _304);
      float _308 = -0.0f - TEXCOORD_5.x;
      float _309 = -0.0f - TEXCOORD_5.y;
      float _310 = -0.0f - TEXCOORD_5.z;
      float _311 = dot(float3(_109, _110, _111), float3(_308, _309, _310));
      float _312 = saturate(_311);
      float _313 = _312 * COLOR.w;
      float _314 = _312 * COLOR_1.w;
      float _315 = _312 * TEXCOORD_5.w;
      float _316 = max(0.0f, _313);
      float _317 = max(0.0f, _314);
      float _318 = max(0.0f, _315);
      float _319 = _316 + _305;
      float _320 = _317 + _306;
      float _321 = _318 + _307;
      float _322 = _118 + TEXCOORD_4.x;
      float _323 = _119 + TEXCOORD_4.y;
      float _324 = _120 + TEXCOORD_4.z;
      float _325 = dot(float3(_322, _323, _324), float3(_322, _323, _324));
      float _326 = rsqrt(_325);
      float _327 = _118 + TEXCOORD_5.x;
      float _328 = _119 + TEXCOORD_5.y;
      float _329 = _120 + TEXCOORD_5.z;
      float _330 = dot(float3(_327, _328, _329), float3(_327, _328, _329));
      float _331 = rsqrt(_330);
      float _332 = _327 * _331;
      float _333 = -0.0f - _332;
      float _334 = _328 * _331;
      float _335 = -0.0f - _334;
      float _336 = _329 * _331;
      float _337 = -0.0f - _336;
      float _338 = _322 * _326;
      float _339 = -0.0f - _338;
      float _340 = _323 * _326;
      float _341 = -0.0f - _340;
      float _342 = _324 * _326;
      float _343 = -0.0f - _342;
      float _344 = dot(float3(_339, _341, _343), float3(_109, _110, _111));
      float _345 = dot(float3(_333, _335, _337), float3(_109, _110, _111));
      float _346 = saturate(_344);
      float _347 = saturate(_345);
      float _348 = log2(_346);
      float _349 = log2(_347);
      float _350 = _348 * _73;
      float _351 = _349 * _73;
      float _352 = exp2(_350);
      float _353 = exp2(_351);
      float _354 = _352 * TEXCOORD_1.w;
      float _355 = _352 * TEXCOORD_2.w;
      float _356 = _352 * TEXCOORD_4.w;
      float _357 = _353 * COLOR.w;
      float _358 = _353 * COLOR_1.w;
      float _359 = _353 * TEXCOORD_5.w;
      float _360 = _357 + _354;
      float _361 = _358 + _355;
      float _362 = _359 + _356;
      float _363 = max(0.0f, _360);
      float _364 = max(0.0f, _361);
      float _365 = max(0.0f, _362);
      float _366 = _264 * _363;
      float _367 = _265 * _364;
      float _368 = _266 * _365;
      float _369 = min(_366, 1.0f);
      float _370 = min(_367, 1.0f);
      float _371 = min(_368, 1.0f);
      _373 = _369;
      _374 = _370;
      _375 = _371;
      _376 = _319;
      _377 = _320;
      _378 = _321;
   }
   else
   {
      _373 = 0.0f;
      _374 = 0.0f;
      _375 = 0.0f;
      _376 = 0.0f;
      _377 = 0.0f;
      _378 = 0.0f;
   }
   float _379 = c_rt_size.z * SV_Position.x;
   float _380 = c_rt_size.w * SV_Position.y;
   float4 _381 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_379, _380)));
   int _388 = c_bool & 16384;
   bool _389 = (_388 == 0);
   if (!_389)
   {
      float _391 = _381.x * 4.0f;
      float _392 = _381.y * 4.0f;
      float _393 = _381.z * 4.0f;
      float _394 = _381.w * 4.0f;
      _411 = _391;
      _412 = _392;
      _413 = _393;
      _414 = _394;
   }
   else
   {
      int _396 = c_bool & 8192;
      bool _397 = (_396 == 0);
      if (!_397)
      {
         float _399 = _381.x * 2.0f;
         float _400 = _381.y * 2.0f;
         float _401 = _381.z * 2.0f;
         float _402 = _381.w * 2.0f;
         _411 = _399;
         _412 = _400;
         _413 = _401;
         _414 = _402;
      }
      else
      {
         float _406 = _381.x / c_tone_control.z;
         float _407 = _381.y / c_tone_control.z;
         float _408 = _381.z / c_tone_control.z;
         float _409 = _381.w / c_tone_control.z;
         _411 = _406;
         _412 = _407;
         _413 = _408;
         _414 = _409;
      }
   }
   float _415 = _411 + _376;
   float _416 = _412 + _377;
   float _417 = _413 + _378;
   float _418 = _414 + _373;
   float _419 = _414 + _374;
   float _420 = _414 + _375;
   float _483 = dot(float3(_109, _110, _111), float3(_109, _110, _111));
   float _484 = rsqrt(_483);
   float _485 = _484 * _109;
   float _486 = _484 * _110;
   float _487 = _484 * _111;
   float _488 = dot(float4((c_ps_scratch[80].x), (c_ps_scratch[80].y), (c_ps_scratch[80].z), (c_ps_scratch[80].w)), float4(_485, _486, _487, 1.0f));
   float _489 = dot(float4((c_ps_scratch[81].x), (c_ps_scratch[81].y), (c_ps_scratch[81].z), (c_ps_scratch[81].w)), float4(_485, _486, _487, 1.0f));
   float _490 = dot(float4((c_ps_scratch[82].x), (c_ps_scratch[82].y), (c_ps_scratch[82].z), (c_ps_scratch[82].w)), float4(_485, _486, _487, 1.0f));
   float _491 = dot(float4((c_ps_scratch[83].x), (c_ps_scratch[83].y), (c_ps_scratch[83].z), (c_ps_scratch[83].w)), float4(_485, _486, _487, 1.0f));
   float _492 = dot(float4(_485, _486, _487, 1.0f), float4(_488, _489, _490, _491));
   float _493 = dot(float4((c_ps_scratch[84].x), (c_ps_scratch[84].y), (c_ps_scratch[84].z), (c_ps_scratch[84].w)), float4(_485, _486, _487, 1.0f));
   float _494 = dot(float4((c_ps_scratch[85].x), (c_ps_scratch[85].y), (c_ps_scratch[85].z), (c_ps_scratch[85].w)), float4(_485, _486, _487, 1.0f));
   float _495 = dot(float4((c_ps_scratch[86].x), (c_ps_scratch[86].y), (c_ps_scratch[86].z), (c_ps_scratch[86].w)), float4(_485, _486, _487, 1.0f));
   float _496 = dot(float4((c_ps_scratch[87].x), (c_ps_scratch[87].y), (c_ps_scratch[87].z), (c_ps_scratch[87].w)), float4(_485, _486, _487, 1.0f));
   float _497 = dot(float4(_485, _486, _487, 1.0f), float4(_493, _494, _495, _496));
   float _498 = dot(float4((c_ps_scratch[88].x), (c_ps_scratch[88].y), (c_ps_scratch[88].z), (c_ps_scratch[88].w)), float4(_485, _486, _487, 1.0f));
   float _499 = dot(float4((c_ps_scratch[89].x), (c_ps_scratch[89].y), (c_ps_scratch[89].z), (c_ps_scratch[89].w)), float4(_485, _486, _487, 1.0f));
   float _500 = dot(float4((c_ps_scratch[90].x), (c_ps_scratch[90].y), (c_ps_scratch[90].z), (c_ps_scratch[90].w)), float4(_485, _486, _487, 1.0f));
   float _501 = dot(float4((c_ps_scratch[91].x), (c_ps_scratch[91].y), (c_ps_scratch[91].z), (c_ps_scratch[91].w)), float4(_485, _486, _487, 1.0f));
   float _502 = dot(float4(_485, _486, _487, 1.0f), float4(_498, _499, _500, _501));
   float _503 = max(_492, 0.0625f);
   float _504 = max(_497, 0.0625f);
   float _505 = max(_502, 0.0625f);
   float _506 = _503 * (c_ps_scratch[79].x);
   float _507 = _504 * (c_ps_scratch[79].x);
   float _508 = _505 * (c_ps_scratch[79].x);
   float _513 = -0.0f - (c_light_vec[0].x);
   float _514 = -0.0f - (c_light_vec[0].y);
   float _515 = -0.0f - (c_light_vec[0].z);
   float _516 = dot(float3(_109, _110, _111), float3(_513, _514, _515));
   float _517 = -0.0f - (c_light_vec[1].x);
   float _518 = -0.0f - (c_light_vec[1].y);
   float _519 = -0.0f - (c_light_vec[1].z);
   float _520 = dot(float3(_109, _110, _111), float3(_517, _518, _519));
   float _529 = _110 * 0.5f;
   float _530 = _529 + 0.5f;
   float _531 = (c_light_col[4].x) - (c_light_vec[4].x);
   float _532 = (c_light_col[4].y) - (c_light_vec[4].y);
   float _533 = (c_light_col[4].z) - (c_light_vec[4].z);
   float _534 = _531 * _530;
   float _535 = _532 * _530;
   float _536 = _533 * _530;
   float _537 = saturate(_516);
   float _538 = saturate(_520);
   float _543 = (c_light_col[0].x) * _537;
   float _544 = (c_light_col[0].y) * _537;
   float _545 = (c_light_col[0].z) * _537;
   float _550 = (c_light_col[1].x) * _538;
   float _551 = (c_light_col[1].y) * _538;
   float _552 = (c_light_col[1].z) * _538;
   float _553 = _543 * _194;
   float _554 = _544 * _194;
   float _555 = _545 * _194;
   float _556 = (c_light_vec[4].x) + _506;
   float _557 = _556 + _534;
   float _558 = _557 + _415;
   float _559 = _558 + _550;
   float _560 = _559 + _553;
   float _561 = (c_light_vec[4].y) + _507;
   float _562 = _561 + _535;
   float _563 = _562 + _416;
   float _564 = _563 + _551;
   float _565 = _564 + _554;
   float _566 = (c_light_vec[4].z) + _508;
   float _567 = _566 + _536;
   float _568 = _567 + _417;
   float _569 = _568 + _552;
   float _570 = _569 + _555;
   int _571 = c_bool & 64;
   bool _572 = (_571 == 0);
   if (!_572)
   {
      float _574 = 1.0f - _54.y;
      float _575 = _194 * _574;
      float _576 = _575 * _537;
      float _577 = _576 + _54.y;
      _579 = _577;
   }
   else
   {
      _579 = _54.y;
   }
   float _580 = _418 + _288;
   float _581 = _419 + _289;
   float _582 = _420 + _290;
   int _583 = c_bool & 32768;
   bool _584 = (_583 == 0);
   if (!_584)
   {
      float _596 = (c_ps_user[11].x) * _162;
      float _597 = (c_ps_user[11].x) * _163;
      float _598 = (c_ps_user[11].x) * _164;
      float _600 = _109 + 1.0f;
      float _601 = _110 + 1.0f;
      float _602 = _111 + 1.0f;
      float _603 = _600 * 0.5f;
      float _604 = _601 * 0.5f;
      float _605 = _602 * 0.5f;
      float _606 = saturate(_603);
      float _607 = saturate(_604);
      float _608 = saturate(_605);
      float _609 = (c_ps_user[11].y) * _606;
      float _610 = _607 * (c_ps_user[11].y);
      float _611 = _608 * (c_ps_user[11].y);
      float _621 = (c_ps_user[11].z) * _54.x;
      float _623 = (c_ps_user[11].w) * _580;
      float _626 = (c_ps_user[12].x) * _194;
      float _628 = (c_ps_user[12].y) * _54.y;
      float _633 = -0.0f - _118;
      float _634 = -0.0f - _119;
      float _635 = -0.0f - _120;
      float _636 = dot(float3(_633, _634, _635), float3(_109, _110, _111));
      float _637 = 1.0f - _636;
      float _638 = _637 * _637;
      float _639 = _637 * 0.9722222089767456f;
      float _640 = _638 * _638;
      float _641 = _640 * _639;
      float _642 = _641 + 0.02777777425944805f;
      float _643 = _642 * (c_ps_user[13].z);
      float _645 = (c_ps_user[13].w) * _557;
      float _646 = (c_ps_user[13].w) * _562;
      float _647 = (c_ps_user[13].w) * _567;
      float _650 = (c_ps_user[14].y) * _543;
      float _651 = (c_ps_user[14].y) * _544;
      float _652 = (c_ps_user[14].y) * _545;
      float _654 = (c_ps_user[14].z) * _550;
      float _655 = (c_ps_user[14].z) * _551;
      float _656 = (c_ps_user[14].z) * _552;
      float _658 = (c_ps_user[14].w) * _415;
      float _659 = (c_ps_user[14].w) * _416;
      float _660 = (c_ps_user[14].w) * _417;
      float _665 = (c_ps_user[15].z) * _418;
      float _666 = (c_ps_user[15].z) * _419;
      float _667 = (c_ps_user[15].z) * _420;
      float _668 = (c_ps_user[15].y) + (c_ps_user[15].x);
      float _669 = c_specular_scale.x * _668;
      float _670 = _669 * _580;
      float _671 = _650 + (c_ps_user[13].x);
      float _672 = _671 + _654;
      float _673 = c_diffuse_scale.x * _672;
      float _674 = _673 * c_mat_diffuse.x;
      float _675 = (c_ps_user[13].y) + _621;
      float _676 = _675 * c_specular_scale.x;
      float _677 = _676 + _670;
      float _678 = _677 * c_mat_specular.x;
      float _679 = _609 + _596;
      float _680 = _679 + _623;
      float _681 = _680 + _626;
      float _682 = _681 + _628;
      float _683 = _682 + _645;
      float _684 = _683 + _658;
      float _685 = _684 + _665;
      float _686 = _685 + _643;
      float _687 = _686 + _674;
      float _688 = _687 + _678;
      float _689 = c_specular_scale.y * _668;
      float _690 = _689 * _581;
      float _691 = _651 + (c_ps_user[13].x);
      float _692 = _691 + _655;
      float _693 = c_diffuse_scale.y * _692;
      float _694 = _693 * c_mat_diffuse.y;
      float _695 = _675 * c_specular_scale.y;
      float _696 = _695 + _690;
      float _697 = _696 * c_mat_specular.y;
      float _698 = _623 + _597;
      float _699 = _698 + _610;
      float _700 = _699 + _626;
      float _701 = _700 + _628;
      float _702 = _701 + _646;
      float _703 = _702 + _659;
      float _704 = _703 + _666;
      float _705 = _704 + _643;
      float _706 = _705 + _694;
      float _707 = _706 + _697;
      float _708 = c_specular_scale.z * _668;
      float _709 = _708 * _582;
      float _710 = _652 + (c_ps_user[13].x);
      float _711 = _710 + _656;
      float _712 = c_diffuse_scale.z * _711;
      float _713 = _712 * c_mat_diffuse.z;
      float _714 = _675 * c_specular_scale.z;
      float _715 = _714 + _709;
      float _716 = _715 * c_mat_specular.z;
      float _717 = _623 + _598;
      float _718 = _717 + _611;
      float _719 = _718 + _626;
      float _720 = _719 + _628;
      float _721 = _720 + _647;
      float _722 = _721 + _660;
      float _723 = _722 + _667;
      float _724 = _723 + _643;
      float _725 = _724 + _713;
      float _726 = _725 + _716;
      float _727 = saturate(_688);
      float _728 = saturate(_707);
      float _729 = saturate(_726);
      _750 = _727;
      _751 = _728;
      _752 = _729;
      _753 = 1.0f;
   }
   else
   {
      float _731 = c_diffuse_scale.x * c_mat_diffuse.x;
      float _732 = _731 * _162;
      float _733 = _732 * _560;
      float _734 = _733 * _579;
      float _735 = c_diffuse_scale.y * c_mat_diffuse.y;
      float _736 = _735 * _163;
      float _737 = _736 * _565;
      float _738 = _737 * _579;
      float _739 = c_diffuse_scale.z * c_mat_diffuse.z;
      float _740 = _739 * _164;
      float _741 = _740 * _570;
      float _742 = _741 * _579;
      float _743 = _580 * _54.x;
      float _744 = _581 * _54.x;
      float _745 = _582 * _54.x;
      float _746 = _734 + _743;
      float _747 = _738 + _744;
      float _748 = _742 + _745;
      _750 = _746;
      _751 = _747;
      _752 = _748;
      _753 = _580;
   }
   float _758 = c_mat_opacity * _59.w;
   float _759 = _758 * c_opacity_scale;
   float _760 = _759 * c_global_opacity;
   float _761 = 1.0f - _760;
   float _762 = saturate(_753);
   float _763 = _761 * _762;
   float _764 = _763 + _760;
   float _765 = _750 * COLOR.x;
   float _766 = _751 * COLOR.y;
   float _767 = _752 * COLOR.z;
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
      if (_584)
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
   if (!_584)
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
   float _883 = _764 - c_mat_punchthrough_ref;
   bool _884 = (_883 < 0.0f);
   if (_884)
      discard;
   float _890 = (c_ps_col_scl[0].x) * _878;
   float _891 = (c_ps_col_scl[0].y) * _879;
   float _892 = (c_ps_col_scl[0].z) * _880;
   float _893 = (c_ps_col_scl[0].w) * _764;
   SV_Target.x = _890;
   SV_Target.y = _891;
   SV_Target.z = _892;
   SV_Target.w = _893;
   int _896 = c_bool & 8;
   bool _897 = (_896 != 0);
   float _898 = select(_897, _878, _890);
   float _899 = select(_897, _879, _891);
   float _900 = select(_897, _880, _892);
   float _901 = select(_897, _764, _893);
   SV_Target.x = _898;
   SV_Target.y = _899;
   SV_Target.z = _900;
   SV_Target.w = _901;
   return SV_Target;
}
