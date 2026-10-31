// Yakuza Kiwami material r_o1dst[ref] (0x09E5EABE, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
// applied by a script: the inline Hable tonemap's saturate goes through
// YKR_MaterialCurve (tangent continuation in HDR), and the light pre-pass buffer t14 through YKR_LightBuffer.

#include "Includes/Common.hlsl"

Texture2D<float4> s0_t : register(t0);

Texture2D<float4> s3_t : register(t3);

Texture2D<float4> s5_t : register(t5);

TextureCube<float4> s6c_t : register(t6);

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

cbuffer cb3 : register(b3)
{
   float4 c_light_col[6] : packoffset(c000.x);
   float4 c_light_vec[6] : packoffset(c006.x);
};

SamplerState s0_s : register(s0);

SamplerState s3_s : register(s3);

SamplerState s5_s : register(s5);

SamplerState s6c_s : register(s6);

SamplerState s13_s : register(s13);

SamplerState s14_s : register(s14);

float4 main(
    linear float2 TEXCOORD : TEXCOORD,
    linear float4 COLOR : COLOR,
    linear float4 COLOR_1 : COLOR1,
    linear float4 TEXCOORD_2 : TEXCOORD2,
    linear float3 TEXCOORD_3 : TEXCOORD3,
    linear float3 TEXCOORD_4 : TEXCOORD4,
    linear float3 TEXCOORD_5 : TEXCOORD5,
    linear float4 TEXCOORD_6 : TEXCOORD6,
    noperspective float4 SV_Position : SV_Position) : SV_Target
{
   float4 SV_Target = 0;
   float4 _48 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _53 = _48.x * _48.x;
   float _54 = _48.y * _48.y;
   float _55 = _48.z * _48.z;
   float _59 = (c_attribute_ps[0].x) * TEXCOORD.x;
   float _60 = (c_attribute_ps[0].y) * TEXCOORD.y;
   float4 _61 = s3_t.Sample(s3_s, float2(_59, _60));
   float _69 = (c_attribute_ps[0].z) * TEXCOORD.x;
   float _70 = (c_attribute_ps[0].w) * TEXCOORD.y;
   float4 _71 = s5_t.Sample(s5_s, float2(_69, _70));
   float _74 = _71.x * 2.0f;
   float _75 = _71.y * 2.0f;
   float _76 = _74 + -1.0f;
   float _77 = _75 + -1.0f;
   float _78 = dot(float2(_76, _77), float2(_76, _77));
   float _79 = 1.0f - _78;
   float _80 = saturate(_79);
   float _81 = sqrt(_80);
   float _82 = TEXCOORD_2.y * TEXCOORD_6.z;
   float _83 = TEXCOORD_2.z * TEXCOORD_6.y;
   float _84 = _82 - _83;
   float _85 = TEXCOORD_2.z * TEXCOORD_6.x;
   float _86 = TEXCOORD_2.x * TEXCOORD_6.z;
   float _87 = _85 - _86;
   float _88 = TEXCOORD_2.x * TEXCOORD_6.y;
   float _89 = TEXCOORD_2.y * TEXCOORD_6.x;
   float _90 = _88 - _89;
   float _91 = _76 * TEXCOORD_6.x;
   float _92 = _76 * TEXCOORD_6.y;
   float _93 = _76 * TEXCOORD_6.z;
   float _94 = _77 * TEXCOORD_6.w;
   float _95 = _94 * _84;
   float _96 = _94 * _87;
   float _97 = _94 * _90;
   float _98 = _81 * TEXCOORD_2.x;
   float _99 = _81 * TEXCOORD_2.y;
   float _100 = _81 * TEXCOORD_2.z;
   float _101 = _98 + _91;
   float _102 = _101 + _95;
   float _103 = _99 + _92;
   float _104 = _103 + _96;
   float _105 = _100 + _93;
   float _106 = _105 + _97;
   float _107 = dot(float3(_102, _104, _106), float3(_102, _104, _106));
   float _108 = rsqrt(_107);
   float _109 = _102 * _108;
   float _110 = _104 * _108;
   float _111 = _106 * _108;
   float _112 = dot(float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z), float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z));
   float _113 = rsqrt(_112);
   float _114 = _113 * TEXCOORD_3.x;
   float _115 = _113 * TEXCOORD_3.y;
   float _116 = _113 * TEXCOORD_3.z;
   float _121 = -0.0f - (c_light_vec[0].x);
   float _122 = -0.0f - (c_light_vec[0].y);
   float _123 = -0.0f - (c_light_vec[0].z);
   float _124 = dot(float3(_109, _110, _111), float3(_121, _122, _123));
   float _128 = c_rt_size.z * SV_Position.x;
   float _129 = c_rt_size.w * SV_Position.y;
   int _132 = c_bool & 8;
   bool _133 = (_132 == 0);
   float _140;
   float _141;
   float _142;
   float _154;
   float _155;
   float _191;
   float _192;
   float _193;
   float _194;
   float _208;
   float _209;
   float _244;
   float _245;
   float _246;
   float _280;
   float _281;
   float _282;
   float _283;
   float _284;
   float _285;
   float _439;
   float _440;
   float _441;
   float _442;
   float _443;
   float _444;
   float _620;
   float _621;
   float _622;
   float _623;
   float _624;
   float _625;
   float _747;
   float _748;
   float _749;
   float _765;
   float _766;
   float _767;
   if (!_133)
   {
      float4 _135 = s13_t.Sample(s13_s, float2(_128, _129));
      _140 = _135.x;
      _141 = _135.y;
      _142 = _135.w;
   }
   else
   {
      _140 = 1.0f;
      _141 = 1.0f;
      _142 = 1.0f;
   }
   if (!_133)
   {
      float _147 = (c_sdw_param[0].x) * _140;
      float _148 = (c_sdw_param[0].z) * _141;
      float _151 = _147 + (c_sdw_param[0].y);
      float _152 = _148 + (c_sdw_param[0].w);
      _154 = _151;
      _155 = _152;
   }
   else
   {
      _154 = _140;
      _155 = _141;
   }
   float _159 = c_rt_size.z * SV_Position.x;
   float _160 = c_rt_size.w * SV_Position.y;
   float4 _161 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_159, _160)));
   int _168 = c_bool & 16384;
   bool _169 = (_168 == 0);
   if (!_169)
   {
      float _171 = _161.x * 2.0f;
      float _172 = _161.y * 2.0f;
      float _173 = _161.z * 2.0f;
      float _174 = _161.w * 2.0f;
      _191 = _171;
      _192 = _172;
      _193 = _173;
      _194 = _174;
   }
   else
   {
      int _176 = c_bool & 8192;
      bool _177 = (_176 == 0);
      if (!_177)
      {
         float _179 = _161.x * 2.0f;
         float _180 = _161.y * 2.0f;
         float _181 = _161.z * 2.0f;
         float _182 = _161.w * 2.0f;
         _191 = _179;
         _192 = _180;
         _193 = _181;
         _194 = _182;
      }
      else
      {
         float _186 = _161.x / c_tone_control.z;
         float _187 = _161.y / c_tone_control.z;
         float _188 = _161.z / c_tone_control.z;
         float _189 = _161.w / c_tone_control.z;
         _191 = _186;
         _192 = _187;
         _193 = _188;
         _194 = _189;
      }
   }
   int _195 = c_bool & 8;
   bool _196 = (_195 == 0);
   if (!_196)
   {
      float _201 = (c_sdw_param[0].x) * _154;
      float _202 = (c_sdw_param[0].z) * _155;
      float _205 = _201 + (c_sdw_param[0].y);
      float _206 = _202 + (c_sdw_param[0].w);
      _208 = _205;
      _209 = _206;
   }
   else
   {
      _208 = _154;
      _209 = _155;
   }
   float _210 = _209 * _208;
   float _215 = 1.0f - (c_light_col[5].x);
   float _216 = 1.0f - (c_light_col[5].y);
   float _217 = 1.0f - (c_light_col[5].z);
   float _218 = _215 * _210;
   float _219 = _216 * _210;
   float _220 = _217 * _210;
   float _221 = _218 + (c_light_col[5].x);
   float _222 = _219 + (c_light_col[5].y);
   float _223 = _220 + (c_light_col[5].z);
   float _224 = _142 * COLOR.x;
   float _225 = _142 * COLOR.y;
   float _226 = _142 * COLOR.z;
   if (!_196)
   {
      float _230 = (c_sdw_param[2].x) * _221;
      float _231 = (c_sdw_param[2].x) * _222;
      float _232 = (c_sdw_param[2].x) * _223;
      float _234 = _230 + (c_sdw_param[2].y);
      float _235 = _231 + (c_sdw_param[2].y);
      float _236 = _232 + (c_sdw_param[2].y);
      float _239 = (c_sdw_param[2].w) + (c_sdw_param[2].z);
      float _240 = _234 * _239;
      float _241 = _235 * _239;
      float _242 = _236 * _239;
      _244 = _240;
      _245 = _241;
      _246 = _242;
   }
   else
   {
      _244 = _221;
      _245 = _222;
      _246 = _223;
   }
   float _247 = _244 * _142;
   float _248 = _245 * _142;
   float _249 = _246 * _142;
   float _260 = c_mat_emissive.x * _53;
   float _261 = _260 * c_emissive_scale;
   float _262 = c_mat_emissive.y * _54;
   float _263 = _262 * c_emissive_scale;
   float _264 = c_mat_emissive.z * _55;
   float _265 = _264 * c_emissive_scale;
   int _266 = c_bool & 2048;
   bool _267 = (_266 == 0);
   if (_267)
   {
      float _269 = _209 * _191;
      float _270 = _209 * _192;
      float _271 = _209 * _193;
      float _272 = _269 + TEXCOORD_4.x;
      float _273 = _270 + TEXCOORD_4.y;
      float _274 = _271 + TEXCOORD_4.z;
      float _275 = _209 * _194;
      float _276 = _275 + TEXCOORD_5.x;
      float _277 = _275 + TEXCOORD_5.y;
      float _278 = _275 + TEXCOORD_5.z;
      _280 = _272;
      _281 = _273;
      _282 = _274;
      _283 = _276;
      _284 = _277;
      _285 = _278;
   }
   else
   {
      _280 = TEXCOORD_4.x;
      _281 = TEXCOORD_4.y;
      _282 = TEXCOORD_4.z;
      _283 = TEXCOORD_5.x;
      _284 = TEXCOORD_5.y;
      _285 = TEXCOORD_5.z;
   }
   float _290 = -0.0f - (c_light_vec[1].x);
   float _291 = -0.0f - (c_light_vec[1].y);
   float _292 = -0.0f - (c_light_vec[1].z);
   float _293 = dot(float3(_109, _110, _111), float3(_290, _291, _292));
   float _294 = saturate(_293);
   float _299 = saturate(_124);
   float _300 = (c_light_col[0].x) * _299;
   float _301 = (c_light_col[0].y) * _299;
   float _302 = (c_light_col[0].z) * _299;
   float _307 = (c_light_col[1].x) * _294;
   float _308 = (c_light_col[1].y) * _294;
   float _309 = (c_light_col[1].z) * _294;
   float _318 = c_diffuse_scale.x * c_mat_diffuse.x;
   float _319 = c_diffuse_scale.y * c_mat_diffuse.y;
   float _320 = c_diffuse_scale.z * c_mat_diffuse.z;
   float _321 = _300 * _221;
   float _322 = _301 * _222;
   float _323 = _302 * _223;
   float _324 = _307 * _209;
   float _325 = _308 * _209;
   float _326 = _309 * _209;
   float _327 = _324 + _321;
   float _328 = _325 + _322;
   float _329 = _326 + _323;
   float _330 = _318 * _327;
   float _331 = _319 * _328;
   float _332 = _320 * _329;
   float _333 = _261 + c_scene_ambient.x;
   float _334 = _333 + _330;
   float _335 = _263 + c_scene_ambient.y;
   float _336 = _335 + _331;
   float _337 = _265 + c_scene_ambient.z;
   float _338 = _337 + _332;
   float _339 = _334 * _224;
   float _340 = _336 * _225;
   float _341 = _338 * _226;
   float _342 = _280 * _247;
   float _343 = _281 * _248;
   float _344 = _282 * _249;
   float _345 = _339 + _342;
   float _346 = _340 + _343;
   float _347 = _341 + _344;
   float _352 = (c_light_vec[0].x) + _114;
   float _353 = (c_light_vec[0].y) + _115;
   float _354 = (c_light_vec[0].z) + _116;
   float _355 = dot(float3(_352, _353, _354), float3(_352, _353, _354));
   float _356 = rsqrt(_355);
   float _357 = _352 * _356;
   float _358 = -0.0f - _357;
   float _359 = _353 * _356;
   float _360 = -0.0f - _359;
   float _361 = _354 * _356;
   float _362 = -0.0f - _361;
   float _363 = dot(float3(_358, _360, _362), float3(_109, _110, _111));
   float _364 = saturate(_363);
   float _365 = _115 + -0.30000001192092896f;
   float _366 = dot(float3(_114, _365, _116), float3(_114, _365, _116));
   float _367 = rsqrt(_366);
   float _368 = _114 * _367;
   float _369 = -0.0f - _368;
   float _370 = _365 * _367;
   float _371 = -0.0f - _370;
   float _372 = _116 * _367;
   float _373 = -0.0f - _372;
   float _374 = dot(float3(_369, _371, _373), float3(_109, _110, _111));
   float _375 = saturate(_374);
   float _378 = log2(_364);
   float _379 = log2(_375);
   float _380 = _378 * c_mat_power;
   float _381 = _379 * c_mat_power;
   float _382 = exp2(_380);
   float _383 = exp2(_381);
   float _386 = _382 * (c_light_col[0].x);
   float _387 = _382 * (c_light_col[0].y);
   float _388 = _382 * (c_light_col[0].z);
   float _389 = _383 * 2.0f;
   float _390 = _389 * (c_light_vec[5].x);
   float _391 = _390 * (c_light_col[1].x);
   float _392 = _390 * (c_light_col[1].y);
   float _393 = _390 * (c_light_col[1].z);
   float _394 = _386 * _221;
   float _395 = _387 * _222;
   float _396 = _388 * _223;
   float _397 = _391 * _294;
   float _398 = _392 * _294;
   float _399 = _393 * _294;
   float _400 = _397 + _394;
   float _401 = _398 + _395;
   float _402 = _399 + _396;
   float _409 = c_mat_specular.x * c_specular_scale.x;
   float _410 = _409 * _224;
   float _411 = _410 * _400;
   float _412 = c_mat_specular.y * c_specular_scale.y;
   float _413 = _412 * _225;
   float _414 = _413 * _401;
   float _415 = c_mat_specular.z * c_specular_scale.z;
   float _416 = _415 * _226;
   float _417 = _416 * _402;
   float _418 = _283 * _247;
   float _419 = _418 * _383;
   float _420 = _284 * _248;
   float _421 = _420 * _383;
   float _422 = _285 * _249;
   float _423 = _422 * _383;
   float _424 = _411 + _419;
   float _425 = _414 + _421;
   float _426 = _417 + _423;
   if (!_267)
   {
      float _428 = _209 * _191;
      float _429 = _209 * _192;
      float _430 = _209 * _193;
      float _431 = _345 + _428;
      float _432 = _346 + _429;
      float _433 = _347 + _430;
      float _434 = _209 * _194;
      float _435 = _424 + _434;
      float _436 = _425 + _434;
      float _437 = _426 + _434;
      _439 = _431;
      _440 = _432;
      _441 = _433;
      _442 = _435;
      _443 = _436;
      _444 = _437;
   }
   else
   {
      _439 = _345;
      _440 = _346;
      _441 = _347;
      _442 = _424;
      _443 = _425;
      _444 = _426;
   }
   int _445 = c_bool & 128;
   bool _446 = (_445 != 0);
   float _447 = select(_446, 0.0f, _442);
   float _448 = select(_446, 0.0f, _443);
   float _449 = select(_446, 0.0f, _444);
   float _450 = _439 * _53;
   float _451 = _440 * _54;
   float _452 = _441 * _55;
   float _453 = _447 * _61.x;
   float _454 = _448 * _61.y;
   float _455 = _449 * _61.z;
   int _456 = c_bool & 32768;
   bool _457 = (_456 == 0);
   if (!_457)
   {
      float _461 = (c_ps_user[11].x) * _53;
      float _462 = (c_ps_user[11].x) * _54;
      float _463 = (c_ps_user[11].x) * _55;
      float _465 = _109 + 1.0f;
      float _466 = _110 + 1.0f;
      float _467 = _111 + 1.0f;
      float _468 = _465 * 0.5f;
      float _469 = _466 * 0.5f;
      float _470 = _467 * 0.5f;
      float _471 = saturate(_468);
      float _472 = saturate(_469);
      float _473 = saturate(_470);
      float _474 = (c_ps_user[11].y) * _471;
      float _475 = _472 * (c_ps_user[11].y);
      float _476 = _473 * (c_ps_user[11].y);
      float _478 = (c_ps_user[11].z) * _61.x;
      float _479 = (c_ps_user[11].z) * _61.y;
      float _480 = (c_ps_user[11].z) * _61.z;
      float _482 = c_mat_power * 0.009999999776482582f;
      float _483 = _482 * (c_ps_user[11].w);
      float _486 = (c_ps_user[12].x) * _221;
      float _487 = (c_ps_user[12].x) * _222;
      float _488 = (c_ps_user[12].x) * _223;
      float _490 = (c_ps_user[12].y) * _224;
      float _491 = (c_ps_user[12].y) * _225;
      float _492 = (c_ps_user[12].y) * _226;
      float _494 = (c_ps_user[12].z) * _247;
      float _495 = (c_ps_user[12].z) * _248;
      float _496 = (c_ps_user[12].z) * _249;
      float _502 = -0.0f - _114;
      float _503 = -0.0f - _115;
      float _504 = -0.0f - _116;
      float _505 = dot(float3(_502, _503, _504), float3(_109, _110, _111));
      float _506 = 1.0f - _505;
      float _507 = _506 * _506;
      float _508 = _506 * 0.9722222089767456f;
      float _509 = _507 * _507;
      float _510 = _509 * _508;
      float _511 = _510 + 0.02777777425944805f;
      float _512 = _511 * (c_ps_user[13].z);
      float _514 = (c_ps_user[13].w) * c_scene_ambient.x;
      float _515 = (c_ps_user[13].w) * c_scene_ambient.y;
      float _516 = (c_ps_user[13].w) * c_scene_ambient.z;
      float _519 = (c_ps_user[14].x) * _261;
      float _520 = (c_ps_user[14].x) * _263;
      float _521 = (c_ps_user[14].x) * _265;
      float _523 = (c_ps_user[14].y) * _300;
      float _524 = (c_ps_user[14].y) * _301;
      float _525 = (c_ps_user[14].y) * _302;
      float _527 = (c_ps_user[14].z) * _307;
      float _528 = (c_ps_user[14].z) * _308;
      float _529 = (c_ps_user[14].z) * _309;
      float _531 = (c_ps_user[14].w) * _280;
      float _532 = (c_ps_user[14].w) * _281;
      float _533 = (c_ps_user[14].w) * _282;
      float _536 = (c_ps_user[15].x) * _386;
      float _537 = (c_ps_user[15].x) * _387;
      float _538 = (c_ps_user[15].x) * _388;
      float _540 = _409 * _391;
      float _541 = _540 * (c_ps_user[15].y);
      float _542 = _412 * _392;
      float _543 = _542 * (c_ps_user[15].y);
      float _544 = _415 * _393;
      float _545 = _544 * (c_ps_user[15].y);
      float _547 = (c_ps_user[15].z) * _283;
      float _548 = (c_ps_user[15].z) * _284;
      float _549 = (c_ps_user[15].z) * _285;
      float _550 = _541 + _547;
      float _551 = _550 * _383;
      float _552 = _523 + (c_ps_user[13].x);
      float _553 = _552 + _527;
      float _554 = c_diffuse_scale.x * _553;
      float _555 = _554 * c_mat_diffuse.x;
      float _556 = (c_ps_user[13].y) + _478;
      float _557 = _556 + _536;
      float _558 = c_mat_specular.x * _557;
      float _559 = _558 * c_specular_scale.x;
      float _560 = _474 + _461;
      float _561 = _560 + _483;
      float _562 = _561 + (c_ps_user[12].w);
      float _563 = _562 + _486;
      float _564 = _563 + _490;
      float _565 = _564 + _494;
      float _566 = _565 + _514;
      float _567 = _566 + _519;
      float _568 = _567 + _531;
      float _569 = _568 + _512;
      float _570 = _569 + _551;
      float _571 = _570 + _555;
      float _572 = _571 + _559;
      float _573 = _543 + _548;
      float _574 = _573 * _383;
      float _575 = _524 + (c_ps_user[13].x);
      float _576 = _575 + _528;
      float _577 = c_diffuse_scale.y * _576;
      float _578 = _577 * c_mat_diffuse.y;
      float _579 = (c_ps_user[13].y) + _479;
      float _580 = _579 + _537;
      float _581 = c_mat_specular.y * _580;
      float _582 = _581 * c_specular_scale.y;
      float _583 = _475 + _462;
      float _584 = _583 + _483;
      float _585 = _584 + (c_ps_user[12].w);
      float _586 = _585 + _487;
      float _587 = _586 + _491;
      float _588 = _587 + _495;
      float _589 = _588 + _515;
      float _590 = _589 + _520;
      float _591 = _590 + _532;
      float _592 = _591 + _512;
      float _593 = _592 + _574;
      float _594 = _593 + _578;
      float _595 = _594 + _582;
      float _596 = _545 + _549;
      float _597 = _596 * _383;
      float _598 = _525 + (c_ps_user[13].x);
      float _599 = _598 + _529;
      float _600 = c_diffuse_scale.z * _599;
      float _601 = _600 * c_mat_diffuse.z;
      float _602 = (c_ps_user[13].y) + _480;
      float _603 = _602 + _538;
      float _604 = c_mat_specular.z * _603;
      float _605 = _604 * c_specular_scale.z;
      float _606 = _483 + _463;
      float _607 = _606 + _476;
      float _608 = _607 + (c_ps_user[12].w);
      float _609 = _608 + _488;
      float _610 = _609 + _492;
      float _611 = _610 + _496;
      float _612 = _611 + _516;
      float _613 = _612 + _521;
      float _614 = _613 + _533;
      float _615 = _614 + _512;
      float _616 = _615 + _597;
      float _617 = _616 + _601;
      float _618 = _617 + _605;
      _620 = _572;
      _621 = _595;
      _622 = _618;
      _623 = 0.0f;
      _624 = 0.0f;
      _625 = 0.0f;
   }
   else
   {
      _620 = _450;
      _621 = _451;
      _622 = _452;
      _623 = _453;
      _624 = _454;
      _625 = _455;
   }
   float _626 = _623 + _620;
   float _627 = _624 + _621;
   float _628 = _625 + _622;
   float _629 = dot(float3(_114, _115, _116), float3(_109, _110, _111));
   float _630 = _629 * 2.0f;
   float _631 = _630 * _109;
   float _632 = _630 * _110;
   float _633 = _630 * _111;
   float _634 = _114 - _631;
   float _635 = _115 - _632;
   float _636 = _116 - _633;
   float _637 = -0.0f - _634;
   float4 _638 = s6c_t.Sample(s6c_s, float3(_637, _635, _636));
   float _645 = _155 * _154;
   float _646 = _124 + 0.75f;
   float _647 = saturate(_646);
   float _648 = _645 * _647;
   float _649 = max(_648, (c_light_vec[5].y));
   float _650 = max(_638.w, 0.003921568859368563f);
   float _651 = 0.25f / _650;
   float _652 = _651 * _638.x;
   float _653 = _651 * _638.y;
   float _654 = _651 * _638.z;
   float _655 = _61.w * TEXCOORD_2.w;
   float _656 = _649 * _655;
   float _657 = _656 * _61.x;
   float _658 = _652 * _652;
   float _659 = _658 * _657;
   float _660 = _656 * _61.y;
   float _661 = _653 * _653;
   float _662 = _661 * _660;
   float _663 = _656 * _61.z;
   float _664 = _654 * _654;
   float _665 = _664 * _663;
   float _666 = _626 + _659;
   float _667 = _627 + _662;
   float _668 = _628 + _665;
   float _675 = c_mat_opacity * _48.w;
   float _676 = _675 * c_opacity_scale;
   float _677 = _676 * c_global_opacity;
   int _680 = c_bool & 16;
   bool _681 = (_680 == 0);
   if (!_681)
   {
      float _685 = c_tone_control.x * _666;
      float _686 = c_tone_control.x * _667;
      float _687 = c_tone_control.x * _668;
      float _689 = log2(_685);
      float _690 = log2(_686);
      float _691 = log2(_687);
      float _692 = _689 * c_tone_control.y;
      float _693 = _690 * c_tone_control.y;
      float _694 = _691 * c_tone_control.y;
      float _695 = exp2(_692);
      float _696 = exp2(_693);
      float _697 = exp2(_694);
      _747 = _695;
      _748 = _696;
      _749 = _697;
   }
   else
   {
      int _699 = c_bool & 32768;
      bool _700 = (_699 == 0);
      if (_700)
      {
         float _704 = c_tone_control.x * _666;
         float _705 = c_tone_control.x * _667;
         float _706 = c_tone_control.x * _668;
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
         _747 = _666;
         _748 = _667;
         _749 = _668;
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
   float _776 = (c_ps_col_scl[0].x) * _768;
   float _777 = (c_ps_col_scl[0].y) * _769;
   float _778 = (c_ps_col_scl[0].z) * _770;
   float _779 = (c_ps_col_scl[0].w) * _677;
   SV_Target.x = _776;
   SV_Target.y = _777;
   SV_Target.z = _778;
   SV_Target.w = _779;
   int _782 = c_bool & 8;
   bool _783 = (_782 != 0);
   float _784 = select(_783, _768, _776);
   float _785 = select(_783, _769, _777);
   float _786 = select(_783, _770, _778);
   float _787 = select(_783, _677, _779);
   SV_Target.x = _784;
   SV_Target.y = _785;
   SV_Target.z = _786;
   SV_Target.w = _787;
   return SV_Target;
}
