// Yakuza Kiwami material r_o1dut[lantern] (0x4975CA08, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
// applied by a script: the inline Hable tonemap's saturate goes through
// YKR_MaterialCurve (tangent continuation in HDR), and the light pre-pass buffer t14 through YKR_LightBuffer.

#include "Includes/Common.hlsl"

Texture2D<float4> s0_t : register(t0);

Texture2D<float4> s3_t : register(t3);

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

cbuffer cb3 : register(b3)
{
   float4 c_light_col[6] : packoffset(c000.x);
   float4 c_light_vec[6] : packoffset(c006.x);
};

SamplerState s0_s : register(s0);

SamplerState s3_s : register(s3);

SamplerState s5_s : register(s5);

SamplerState s6_s : register(s6);

SamplerState s13_s : register(s13);

SamplerState s14_s : register(s14);

float4 main(
    linear float4 TEXCOORD : TEXCOORD,
    linear float4 COLOR : COLOR,
    linear float4 COLOR_1 : COLOR1,
    linear float3 TEXCOORD_2 : TEXCOORD2,
    linear float3 TEXCOORD_3 : TEXCOORD3,
    linear float3 TEXCOORD_4 : TEXCOORD4,
    linear float3 TEXCOORD_5 : TEXCOORD5,
    linear float4 TEXCOORD_6 : TEXCOORD6,
    linear float4 TEXCOORD_1 : TEXCOORD1,
    noperspective float4 SV_Position : SV_Position) : SV_Target
{
   float4 SV_Target = 0;
   float4 _51 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _56 = _51.x * _51.x;
   float _57 = _51.y * _51.y;
   float _58 = _51.z * _51.z;
   float _62 = (c_attribute_ps[0].x) * TEXCOORD.x;
   float _63 = (c_attribute_ps[0].y) * TEXCOORD.y;
   float4 _64 = s3_t.Sample(s3_s, float2(_62, _63));
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
   float _616;
   float _617;
   float _618;
   float _619;
   float _620;
   float _621;
   float _697;
   float _698;
   float _699;
   float _715;
   float _716;
   float _717;
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
   float _260 = c_mat_emissive.x * _56;
   float _261 = _260 * c_emissive_scale;
   float _262 = c_mat_emissive.y * _57;
   float _263 = _262 * c_emissive_scale;
   float _264 = c_mat_emissive.z * _58;
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
      _280 = _276;
      _281 = _277;
      _282 = _278;
      _283 = _272;
      _284 = _273;
      _285 = _274;
   }
   else
   {
      _280 = TEXCOORD_5.x;
      _281 = TEXCOORD_5.y;
      _282 = TEXCOORD_5.z;
      _283 = TEXCOORD_4.x;
      _284 = TEXCOORD_4.y;
      _285 = TEXCOORD_4.z;
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
   float _342 = _283 * _247;
   float _343 = _284 * _248;
   float _344 = _285 * _249;
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
   float _418 = _280 * _247;
   float _419 = _418 * _383;
   float _420 = _281 * _248;
   float _421 = _420 * _383;
   float _422 = _282 * _249;
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
   float _450 = _439 * _56;
   float _451 = _440 * _57;
   float _452 = _441 * _58;
   float _453 = _447 * _64.y;
   float _454 = _448 * _64.y;
   float _455 = _449 * _64.y;
   int _456 = c_bool & 32768;
   bool _457 = (_456 == 0);
   if (!_457)
   {
      float _461 = (c_ps_user[11].x) * _56;
      float _462 = (c_ps_user[11].x) * _57;
      float _463 = (c_ps_user[11].x) * _58;
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
      float _478 = (c_ps_user[11].z) * _64.y;
      float _480 = c_mat_power * 0.009999999776482582f;
      float _481 = _480 * (c_ps_user[11].w);
      float _484 = (c_ps_user[12].x) * _221;
      float _485 = (c_ps_user[12].x) * _222;
      float _486 = (c_ps_user[12].x) * _223;
      float _488 = (c_ps_user[12].y) * _224;
      float _489 = (c_ps_user[12].y) * _225;
      float _490 = (c_ps_user[12].y) * _226;
      float _492 = (c_ps_user[12].z) * _247;
      float _493 = (c_ps_user[12].z) * _248;
      float _494 = (c_ps_user[12].z) * _249;
      float _500 = -0.0f - _114;
      float _501 = -0.0f - _115;
      float _502 = -0.0f - _116;
      float _503 = dot(float3(_500, _501, _502), float3(_109, _110, _111));
      float _504 = 1.0f - _503;
      float _505 = _504 * _504;
      float _506 = _504 * 0.9722222089767456f;
      float _507 = _505 * _505;
      float _508 = _507 * _506;
      float _509 = _508 + 0.02777777425944805f;
      float _510 = _509 * (c_ps_user[13].z);
      float _512 = (c_ps_user[13].w) * c_scene_ambient.x;
      float _513 = (c_ps_user[13].w) * c_scene_ambient.y;
      float _514 = (c_ps_user[13].w) * c_scene_ambient.z;
      float _517 = (c_ps_user[14].x) * _261;
      float _518 = (c_ps_user[14].x) * _263;
      float _519 = (c_ps_user[14].x) * _265;
      float _521 = (c_ps_user[14].y) * _300;
      float _522 = (c_ps_user[14].y) * _301;
      float _523 = (c_ps_user[14].y) * _302;
      float _525 = (c_ps_user[14].z) * _307;
      float _526 = (c_ps_user[14].z) * _308;
      float _527 = (c_ps_user[14].z) * _309;
      float _529 = (c_ps_user[14].w) * _283;
      float _530 = (c_ps_user[14].w) * _284;
      float _531 = (c_ps_user[14].w) * _285;
      float _534 = (c_ps_user[15].x) * _386;
      float _535 = (c_ps_user[15].x) * _387;
      float _536 = (c_ps_user[15].x) * _388;
      float _538 = _409 * _391;
      float _539 = _538 * (c_ps_user[15].y);
      float _540 = _412 * _392;
      float _541 = _540 * (c_ps_user[15].y);
      float _542 = _415 * _393;
      float _543 = _542 * (c_ps_user[15].y);
      float _545 = (c_ps_user[15].z) * _280;
      float _546 = (c_ps_user[15].z) * _281;
      float _547 = (c_ps_user[15].z) * _282;
      float _548 = _539 + _545;
      float _549 = _548 * _383;
      float _550 = _521 + (c_ps_user[13].x);
      float _551 = _550 + _525;
      float _552 = c_diffuse_scale.x * _551;
      float _553 = _552 * c_mat_diffuse.x;
      float _554 = (c_ps_user[13].y) + _478;
      float _555 = _554 + _534;
      float _556 = c_mat_specular.x * _555;
      float _557 = _556 * c_specular_scale.x;
      float _558 = _474 + _461;
      float _559 = _558 + _481;
      float _560 = _559 + (c_ps_user[12].w);
      float _561 = _560 + _484;
      float _562 = _561 + _488;
      float _563 = _562 + _492;
      float _564 = _563 + _512;
      float _565 = _564 + _517;
      float _566 = _565 + _529;
      float _567 = _566 + _510;
      float _568 = _567 + _549;
      float _569 = _568 + _553;
      float _570 = _569 + _557;
      float _571 = _541 + _546;
      float _572 = _571 * _383;
      float _573 = _522 + (c_ps_user[13].x);
      float _574 = _573 + _526;
      float _575 = c_diffuse_scale.y * _574;
      float _576 = _575 * c_mat_diffuse.y;
      float _577 = _554 + _535;
      float _578 = c_mat_specular.y * _577;
      float _579 = _578 * c_specular_scale.y;
      float _580 = _475 + _462;
      float _581 = _580 + _481;
      float _582 = _581 + (c_ps_user[12].w);
      float _583 = _582 + _485;
      float _584 = _583 + _489;
      float _585 = _584 + _493;
      float _586 = _585 + _513;
      float _587 = _586 + _518;
      float _588 = _587 + _530;
      float _589 = _588 + _510;
      float _590 = _589 + _572;
      float _591 = _590 + _576;
      float _592 = _591 + _579;
      float _593 = _543 + _547;
      float _594 = _593 * _383;
      float _595 = _523 + (c_ps_user[13].x);
      float _596 = _595 + _527;
      float _597 = c_diffuse_scale.z * _596;
      float _598 = _597 * c_mat_diffuse.z;
      float _599 = _554 + _536;
      float _600 = c_mat_specular.z * _599;
      float _601 = _600 * c_specular_scale.z;
      float _602 = _481 + _463;
      float _603 = _602 + _476;
      float _604 = _603 + (c_ps_user[12].w);
      float _605 = _604 + _486;
      float _606 = _605 + _490;
      float _607 = _606 + _494;
      float _608 = _607 + _514;
      float _609 = _608 + _519;
      float _610 = _609 + _531;
      float _611 = _610 + _510;
      float _612 = _611 + _594;
      float _613 = _612 + _598;
      float _614 = _613 + _601;
      _616 = _570;
      _617 = _592;
      _618 = _614;
      _619 = 0.0f;
      _620 = 0.0f;
      _621 = 0.0f;
   }
   else
   {
      _616 = _450;
      _617 = _451;
      _618 = _452;
      _619 = _453;
      _620 = _454;
      _621 = _455;
   }
   float _622 = _619 + _616;
   float _623 = _620 + _617;
   float _624 = _621 + _618;
   float _629 = c_mat_opacity * _51.w;
   float _630 = _629 * c_global_opacity;
   float _631 = _630 * c_opacity_scale;
   int _632 = c_bool & 16;
   bool _633 = (_632 == 0);
   if (!_633)
   {
      float _637 = c_tone_control.x * _622;
      float _638 = c_tone_control.x * _623;
      float _639 = c_tone_control.x * _624;
      float _641 = log2(_637);
      float _642 = log2(_638);
      float _643 = log2(_639);
      float _644 = _641 * c_tone_control.y;
      float _645 = _642 * c_tone_control.y;
      float _646 = _643 * c_tone_control.y;
      float _647 = exp2(_644);
      float _648 = exp2(_645);
      float _649 = exp2(_646);
      _697 = _647;
      _698 = _648;
      _699 = _649;
   }
   else
   {
      if (_457)
      {
         float _654 = c_tone_control.x * _622;
         float _655 = c_tone_control.x * _623;
         float _656 = c_tone_control.x * _624;
         float _657 = _654 * 2.0f;
         float _658 = _655 * 2.0f;
         float _659 = _656 * 2.0f;
         float _660 = _654 * 0.30000001192092896f;
         float _661 = _655 * 0.30000001192092896f;
         float _662 = _656 * 0.30000001192092896f;
         float _663 = _660 + 0.05000000074505806f;
         float _664 = _661 + 0.05000000074505806f;
         float _665 = _662 + 0.05000000074505806f;
         float _666 = _663 * _657;
         float _667 = _664 * _658;
         float _668 = _665 * _659;
         float _669 = _666 + 0.004000000189989805f;
         float _670 = _667 + 0.004000000189989805f;
         float _671 = _668 + 0.004000000189989805f;
         float _672 = _660 + 0.5f;
         float _673 = _661 + 0.5f;
         float _674 = _662 + 0.5f;
         float _675 = _672 * _657;
         float _676 = _673 * _658;
         float _677 = _674 * _659;
         float _678 = _675 + 0.06000000238418579f;
         float _679 = _676 + 0.06000000238418579f;
         float _680 = _677 + 0.06000000238418579f;
         float _681 = _669 / _678;
         float _682 = _670 / _679;
         float _683 = _671 / _680;
         float _684 = _681 + -0.06666666269302368f;
         float _685 = _682 + -0.06666666269302368f;
         float _686 = _683 + -0.06666666269302368f;
         float _687 = _684 * 1.3790643215179443f;
         float _688 = _685 * 1.3790643215179443f;
         float _689 = _686 * 1.3790643215179443f;
         float _690 = YKR_MaterialCurve(_687, _654);
         float _691 = YKR_MaterialCurve(_688, _655);
         float _692 = YKR_MaterialCurve(_689, _656);
         float _693 = sqrt(_690);
         float _694 = sqrt(_691);
         float _695 = sqrt(_692);
         _697 = _693;
         _698 = _694;
         _699 = _695;
      }
      else
      {
         _697 = _622;
         _698 = _623;
         _699 = _624;
      }
   }
   int _700 = c_bool & 4;
   bool _701 = (_700 == 0);
   if (_701)
   {
      float _703 = saturate(COLOR_1.x);
      float _708 = (c_fog_param[0].x) - _697;
      float _709 = (c_fog_param[0].y) - _698;
      float _710 = (c_fog_param[0].z) - _699;
      float _711 = _708 * _703;
      float _712 = _709 * _703;
      float _713 = _710 * _703;
      _715 = _711;
      _716 = _712;
      _717 = _713;
   }
   else
   {
      _715 = COLOR_1.x;
      _716 = COLOR_1.y;
      _717 = COLOR_1.z;
   }
   float _718 = _715 + _697;
   float _719 = _716 + _698;
   float _720 = _717 + _699;
   float _721 = dot(float3(TEXCOORD_2.x, TEXCOORD_2.y, TEXCOORD_2.z), float3(TEXCOORD_2.x, TEXCOORD_2.y, TEXCOORD_2.z));
   float _722 = rsqrt(_721);
   float _723 = _722 * TEXCOORD_2.x;
   float _724 = _722 * TEXCOORD_2.y;
   float _725 = _722 * TEXCOORD_2.z;
   float _726 = dot(float3(_114, _115, _116), float3(_723, _724, _725));
   float _727 = TEXCOORD.z - _726;
   float4 _728 = s6_t.Sample(s6_s, float2(_727, TEXCOORD.w));
   float _732 = log2(_56);
   float _733 = log2(_57);
   float _734 = log2(_58);
   float _735 = _732 * TEXCOORD_1.y;
   float _736 = _733 * TEXCOORD_1.y;
   float _737 = _734 * TEXCOORD_1.y;
   float _738 = exp2(_735);
   float _739 = exp2(_736);
   float _740 = exp2(_737);
   float _741 = _51.w * TEXCOORD_1.x;
   float _742 = _728.x * _741;
   float _743 = _742 * _738;
   float _744 = _728.y * _741;
   float _745 = _744 * _739;
   float _746 = _728.z * _741;
   float _747 = _746 * _740;
   float _748 = _718 + _743;
   float _749 = _719 + _745;
   float _750 = _720 + _747;
   float _756 = (c_ps_col_scl[0].x) * _748;
   float _757 = (c_ps_col_scl[0].y) * _749;
   float _758 = _750 * (c_ps_col_scl[0].z);
   float _759 = (c_ps_col_scl[0].w) * _631;
   SV_Target.x = _756;
   SV_Target.y = _757;
   SV_Target.z = _758;
   SV_Target.w = _759;
   int _762 = c_bool & 8;
   bool _763 = (_762 != 0);
   float _764 = select(_763, _748, _756);
   float _765 = select(_763, _749, _757);
   float _766 = select(_763, _750, _758);
   float _767 = select(_763, _631, _759);
   SV_Target.x = _764;
   SV_Target.y = _765;
   SV_Target.z = _766;
   SV_Target.w = _767;
   return SV_Target;
}
