// Yakuza Kiwami material s_o1dz (0xB9E198D2, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
// applied by a script: the inline Hable tonemap's saturate goes through
// YKR_MaterialCurve (tangent continuation in HDR), and the light pre-pass buffer t14 through YKR_LightBuffer.

#include "Includes/Common.hlsl"

Texture2D<float4> s0_t : register(t0);

Texture2D<float4> s2_t : register(t2);

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

SamplerState s13_s : register(s13);

SamplerState s14_s : register(s14);

float4 main(
    linear float4 COLOR : COLOR,
    linear float4 COLOR_1 : COLOR1,
    linear float2 TEXCOORD : TEXCOORD,
    linear float4 TEXCOORD_1 : TEXCOORD1,
    linear float4 TEXCOORD_2 : TEXCOORD2,
    linear float4 TEXCOORD_4 : TEXCOORD4,
    linear float4 TEXCOORD_5 : TEXCOORD5,
    noperspective float4 SV_Position : SV_Position) : SV_Target
{
   float4 SV_Target = 0;
   float4 _46 = s2_t.Sample(s2_s, float2(TEXCOORD.x, TEXCOORD.y));
   float4 _50 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _55 = _50.x * _50.x;
   float _56 = _50.y * _50.y;
   float _57 = _50.z * _50.z;
   float _60 = c_mat_power * _46.z;
   float _61 = max(1.0f, _60);
   float _62 = dot(float3(TEXCOORD_2.x, TEXCOORD_2.y, TEXCOORD_2.z), float3(TEXCOORD_2.x, TEXCOORD_2.y, TEXCOORD_2.z));
   float _63 = rsqrt(_62);
   float _64 = _63 * TEXCOORD_2.x;
   float _65 = _63 * TEXCOORD_2.y;
   float _66 = _63 * TEXCOORD_2.z;
   float _71 = dot(float3(TEXCOORD_1.x, TEXCOORD_1.y, TEXCOORD_1.z), float3(TEXCOORD_1.x, TEXCOORD_1.y, TEXCOORD_1.z));
   float _72 = rsqrt(_71);
   float _73 = _72 * TEXCOORD_1.x;
   float _74 = _72 * TEXCOORD_1.y;
   float _75 = _72 * TEXCOORD_1.z;
   float _76 = _73 + (c_light_vec[0].x);
   float _77 = _74 + (c_light_vec[0].y);
   float _78 = _75 + (c_light_vec[0].z);
   float _79 = dot(float3(_76, _77, _78), float3(_76, _77, _78));
   float _80 = rsqrt(_79);
   float _81 = _76 * _80;
   float _82 = _77 * _80;
   float _83 = _78 * _80;
   float _87 = c_rt_size.z * SV_Position.x;
   float _88 = c_rt_size.w * SV_Position.y;
   int _91 = c_bool & 8;
   bool _92 = (_91 == 0);
   float _98;
   float _99;
   float _111;
   float _112;
   float _300;
   float _301;
   float _302;
   float _303;
   float _304;
   float _305;
   float _338;
   float _339;
   float _340;
   float _341;
   float _506;
   float _677;
   float _678;
   float _679;
   float _755;
   float _756;
   float _757;
   float _773;
   float _774;
   float _775;
   float _800;
   float _801;
   float _802;
   if (!_92)
   {
      float4 _94 = s13_t.Sample(s13_s, float2(_87, _88));
      _98 = _94.x;
      _99 = _94.y;
   }
   else
   {
      _98 = 1.0f;
      _99 = 1.0f;
   }
   if (!_92)
   {
      float _104 = (c_sdw_param_ch[0].x) * _98;
      float _105 = (c_sdw_param_ch[0].z) * _99;
      float _108 = _104 + (c_sdw_param_ch[0].y);
      float _109 = _105 + (c_sdw_param_ch[0].w);
      _111 = _108;
      _112 = _109;
   }
   else
   {
      _111 = _98;
      _112 = _99;
   }
   float _113 = min(_111, _112);
   float _122 = (c_light_vec[1].x) + _73;
   float _123 = (c_light_vec[1].y) + _74;
   float _124 = (c_light_vec[1].z) + _75;
   float _125 = dot(float3(_122, _123, _124), float3(_122, _123, _124));
   float _126 = rsqrt(_125);
   float _127 = _122 * _126;
   float _128 = _123 * _126;
   float _129 = _124 * _126;
   float _130 = -0.0f - (c_light_vec[1].x);
   float _131 = -0.0f - (c_light_vec[1].y);
   float _132 = -0.0f - (c_light_vec[1].z);
   float _133 = -0.0f - (c_light_vec[0].x);
   float _134 = -0.0f - (c_light_vec[0].y);
   float _135 = -0.0f - (c_light_vec[0].z);
   float _136 = dot(float3(_73, _74, _75), float3(_64, _65, _66));
   float _137 = _136 * 2.0f;
   float _138 = _137 * _64;
   float _139 = _137 * _65;
   float _140 = _137 * _66;
   float _141 = _73 - _138;
   float _142 = _74 - _139;
   float _143 = _75 - _140;
   float _144 = dot(float3(_141, _142, _143), float3(_133, _134, _135));
   float _145 = dot(float3(_141, _142, _143), float3(_130, _131, _132));
   float _146 = saturate(_144);
   float _147 = saturate(_145);
   float _148 = log2(_146);
   float _149 = log2(_147);
   float _150 = _148 * _61;
   float _151 = _149 * _61;
   float _152 = exp2(_150);
   float _153 = exp2(_151);
   float _158 = c_mat_fresnel * c_tone_control.w;
   float _159 = _61 * 0.03974359855055809f;
   float _160 = _159 + 0.0856831967830658f;
   float _161 = _153 * _160;
   float _162 = dot(float3(_73, _74, _75), float3(_81, _82, _83));
   float _163 = dot(float3(_73, _74, _75), float3(_127, _128, _129));
   float _164 = 1.0f - _162;
   float _165 = 1.0f - _163;
   float _166 = _164 * _164;
   float _167 = _165 * _165;
   float _168 = 1.0f - _158;
   float _169 = _166 * _166;
   float _170 = _169 * _164;
   float _171 = _170 * _168;
   float _172 = _165 * _168;
   float _173 = _167 * _167;
   float _174 = _173 * _172;
   float _175 = _171 + _158;
   float _176 = _174 + _158;
   float _177 = dot(float3(_64, _65, _66), float3(_133, _134, _135));
   float _178 = saturate(_177);
   float _179 = dot(float3(_64, _65, _66), float3(_130, _131, _132));
   float _180 = saturate(_179);
   float _181 = _161 * _180;
   float _182 = _181 * _176;
   float _191 = c_specular_scale.x * c_mat_specular.x;
   float _192 = c_specular_scale.y * c_mat_specular.y;
   float _193 = c_specular_scale.z * c_mat_specular.z;
   float _198 = _113 * _160;
   float _199 = _198 * _152;
   float _200 = _199 * _178;
   float _201 = _200 * _175;
   float _202 = (c_light_col[0].x) * _201;
   float _203 = (c_light_col[0].y) * _201;
   float _204 = (c_light_col[0].z) * _201;
   float _209 = (c_light_col[1].x) * _182;
   float _210 = (c_light_col[1].y) * _182;
   float _211 = (c_light_col[1].z) * _182;
   float _212 = _209 + _202;
   float _213 = _210 + _203;
   float _214 = _211 + _204;
   float _215 = _191 * _212;
   float _216 = _192 * _213;
   float _217 = _193 * _214;
   int _221 = c_bool & 1;
   bool _222 = (_221 == 0);
   if (!_222)
   {
      float _224 = -0.0f - TEXCOORD_4.x;
      float _225 = -0.0f - TEXCOORD_4.y;
      float _226 = -0.0f - TEXCOORD_4.z;
      float _227 = dot(float3(_64, _65, _66), float3(_224, _225, _226));
      float _228 = saturate(_227);
      float _229 = _228 * TEXCOORD_1.w;
      float _230 = _228 * TEXCOORD_2.w;
      float _231 = _228 * TEXCOORD_4.w;
      float _232 = max(0.0f, _229);
      float _233 = max(0.0f, _230);
      float _234 = max(0.0f, _231);
      float _235 = -0.0f - TEXCOORD_5.x;
      float _236 = -0.0f - TEXCOORD_5.y;
      float _237 = -0.0f - TEXCOORD_5.z;
      float _238 = dot(float3(_64, _65, _66), float3(_235, _236, _237));
      float _239 = saturate(_238);
      float _240 = _239 * COLOR.w;
      float _241 = _239 * COLOR_1.w;
      float _242 = _239 * TEXCOORD_5.w;
      float _243 = max(0.0f, _240);
      float _244 = max(0.0f, _241);
      float _245 = max(0.0f, _242);
      float _246 = _243 + _232;
      float _247 = _244 + _233;
      float _248 = _245 + _234;
      float _249 = _73 + TEXCOORD_4.x;
      float _250 = _74 + TEXCOORD_4.y;
      float _251 = _75 + TEXCOORD_4.z;
      float _252 = dot(float3(_249, _250, _251), float3(_249, _250, _251));
      float _253 = rsqrt(_252);
      float _254 = _73 + TEXCOORD_5.x;
      float _255 = _74 + TEXCOORD_5.y;
      float _256 = _75 + TEXCOORD_5.z;
      float _257 = dot(float3(_254, _255, _256), float3(_254, _255, _256));
      float _258 = rsqrt(_257);
      float _259 = _254 * _258;
      float _260 = -0.0f - _259;
      float _261 = _255 * _258;
      float _262 = -0.0f - _261;
      float _263 = _256 * _258;
      float _264 = -0.0f - _263;
      float _265 = _249 * _253;
      float _266 = -0.0f - _265;
      float _267 = _250 * _253;
      float _268 = -0.0f - _267;
      float _269 = _251 * _253;
      float _270 = -0.0f - _269;
      float _271 = dot(float3(_266, _268, _270), float3(_64, _65, _66));
      float _272 = dot(float3(_260, _262, _264), float3(_64, _65, _66));
      float _273 = saturate(_271);
      float _274 = saturate(_272);
      float _275 = log2(_273);
      float _276 = log2(_274);
      float _277 = _275 * _61;
      float _278 = _276 * _61;
      float _279 = exp2(_277);
      float _280 = exp2(_278);
      float _281 = _279 * TEXCOORD_1.w;
      float _282 = _279 * TEXCOORD_2.w;
      float _283 = _279 * TEXCOORD_4.w;
      float _284 = _280 * COLOR.w;
      float _285 = _280 * COLOR_1.w;
      float _286 = _280 * TEXCOORD_5.w;
      float _287 = _284 + _281;
      float _288 = _285 + _282;
      float _289 = _286 + _283;
      float _290 = max(0.0f, _287);
      float _291 = max(0.0f, _288);
      float _292 = max(0.0f, _289);
      float _293 = _191 * _290;
      float _294 = _192 * _291;
      float _295 = _193 * _292;
      float _296 = min(_293, 1.0f);
      float _297 = min(_294, 1.0f);
      float _298 = min(_295, 1.0f);
      _300 = _296;
      _301 = _297;
      _302 = _298;
      _303 = _246;
      _304 = _247;
      _305 = _248;
   }
   else
   {
      _300 = 0.0f;
      _301 = 0.0f;
      _302 = 0.0f;
      _303 = 0.0f;
      _304 = 0.0f;
      _305 = 0.0f;
   }
   float _306 = c_rt_size.z * SV_Position.x;
   float _307 = c_rt_size.w * SV_Position.y;
   float4 _308 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_306, _307)));
   int _315 = c_bool & 16384;
   bool _316 = (_315 == 0);
   if (!_316)
   {
      float _318 = _308.x * 4.0f;
      float _319 = _308.y * 4.0f;
      float _320 = _308.z * 4.0f;
      float _321 = _308.w * 4.0f;
      _338 = _318;
      _339 = _319;
      _340 = _320;
      _341 = _321;
   }
   else
   {
      int _323 = c_bool & 8192;
      bool _324 = (_323 == 0);
      if (!_324)
      {
         float _326 = _308.x * 2.0f;
         float _327 = _308.y * 2.0f;
         float _328 = _308.z * 2.0f;
         float _329 = _308.w * 2.0f;
         _338 = _326;
         _339 = _327;
         _340 = _328;
         _341 = _329;
      }
      else
      {
         float _333 = _308.x / c_tone_control.z;
         float _334 = _308.y / c_tone_control.z;
         float _335 = _308.z / c_tone_control.z;
         float _336 = _308.w / c_tone_control.z;
         _338 = _333;
         _339 = _334;
         _340 = _335;
         _341 = _336;
      }
   }
   float _342 = _338 + _303;
   float _343 = _339 + _304;
   float _344 = _340 + _305;
   float _345 = _341 + _300;
   float _346 = _341 + _301;
   float _347 = _341 + _302;
   float _410 = dot(float3(_64, _65, _66), float3(_64, _65, _66));
   float _411 = rsqrt(_410);
   float _412 = _411 * _64;
   float _413 = _411 * _65;
   float _414 = _411 * _66;
   float _415 = dot(float4((c_ps_scratch[80].x), (c_ps_scratch[80].y), (c_ps_scratch[80].z), (c_ps_scratch[80].w)), float4(_412, _413, _414, 1.0f));
   float _416 = dot(float4((c_ps_scratch[81].x), (c_ps_scratch[81].y), (c_ps_scratch[81].z), (c_ps_scratch[81].w)), float4(_412, _413, _414, 1.0f));
   float _417 = dot(float4((c_ps_scratch[82].x), (c_ps_scratch[82].y), (c_ps_scratch[82].z), (c_ps_scratch[82].w)), float4(_412, _413, _414, 1.0f));
   float _418 = dot(float4((c_ps_scratch[83].x), (c_ps_scratch[83].y), (c_ps_scratch[83].z), (c_ps_scratch[83].w)), float4(_412, _413, _414, 1.0f));
   float _419 = dot(float4(_412, _413, _414, 1.0f), float4(_415, _416, _417, _418));
   float _420 = dot(float4((c_ps_scratch[84].x), (c_ps_scratch[84].y), (c_ps_scratch[84].z), (c_ps_scratch[84].w)), float4(_412, _413, _414, 1.0f));
   float _421 = dot(float4((c_ps_scratch[85].x), (c_ps_scratch[85].y), (c_ps_scratch[85].z), (c_ps_scratch[85].w)), float4(_412, _413, _414, 1.0f));
   float _422 = dot(float4((c_ps_scratch[86].x), (c_ps_scratch[86].y), (c_ps_scratch[86].z), (c_ps_scratch[86].w)), float4(_412, _413, _414, 1.0f));
   float _423 = dot(float4((c_ps_scratch[87].x), (c_ps_scratch[87].y), (c_ps_scratch[87].z), (c_ps_scratch[87].w)), float4(_412, _413, _414, 1.0f));
   float _424 = dot(float4(_412, _413, _414, 1.0f), float4(_420, _421, _422, _423));
   float _425 = dot(float4((c_ps_scratch[88].x), (c_ps_scratch[88].y), (c_ps_scratch[88].z), (c_ps_scratch[88].w)), float4(_412, _413, _414, 1.0f));
   float _426 = dot(float4((c_ps_scratch[89].x), (c_ps_scratch[89].y), (c_ps_scratch[89].z), (c_ps_scratch[89].w)), float4(_412, _413, _414, 1.0f));
   float _427 = dot(float4((c_ps_scratch[90].x), (c_ps_scratch[90].y), (c_ps_scratch[90].z), (c_ps_scratch[90].w)), float4(_412, _413, _414, 1.0f));
   float _428 = dot(float4((c_ps_scratch[91].x), (c_ps_scratch[91].y), (c_ps_scratch[91].z), (c_ps_scratch[91].w)), float4(_412, _413, _414, 1.0f));
   float _429 = dot(float4(_412, _413, _414, 1.0f), float4(_425, _426, _427, _428));
   float _430 = max(_419, 0.0625f);
   float _431 = max(_424, 0.0625f);
   float _432 = max(_429, 0.0625f);
   float _433 = _430 * (c_ps_scratch[79].x);
   float _434 = _431 * (c_ps_scratch[79].x);
   float _435 = _432 * (c_ps_scratch[79].x);
   float _440 = -0.0f - (c_light_vec[0].x);
   float _441 = -0.0f - (c_light_vec[0].y);
   float _442 = -0.0f - (c_light_vec[0].z);
   float _443 = dot(float3(_64, _65, _66), float3(_440, _441, _442));
   float _444 = -0.0f - (c_light_vec[1].x);
   float _445 = -0.0f - (c_light_vec[1].y);
   float _446 = -0.0f - (c_light_vec[1].z);
   float _447 = dot(float3(_64, _65, _66), float3(_444, _445, _446));
   float _456 = _65 * 0.5f;
   float _457 = _456 + 0.5f;
   float _458 = (c_light_col[4].x) - (c_light_vec[4].x);
   float _459 = (c_light_col[4].y) - (c_light_vec[4].y);
   float _460 = (c_light_col[4].z) - (c_light_vec[4].z);
   float _461 = _458 * _457;
   float _462 = _459 * _457;
   float _463 = _460 * _457;
   float _464 = saturate(_443);
   float _465 = saturate(_447);
   float _470 = (c_light_col[0].x) * _464;
   float _471 = (c_light_col[0].y) * _464;
   float _472 = (c_light_col[0].z) * _464;
   float _477 = (c_light_col[1].x) * _465;
   float _478 = (c_light_col[1].y) * _465;
   float _479 = (c_light_col[1].z) * _465;
   float _480 = _470 * _113;
   float _481 = _471 * _113;
   float _482 = _472 * _113;
   float _483 = (c_light_vec[4].x) + _433;
   float _484 = _483 + _461;
   float _485 = _484 + _342;
   float _486 = _485 + _477;
   float _487 = _486 + _480;
   float _488 = (c_light_vec[4].y) + _434;
   float _489 = _488 + _462;
   float _490 = _489 + _343;
   float _491 = _490 + _478;
   float _492 = _491 + _481;
   float _493 = (c_light_vec[4].z) + _435;
   float _494 = _493 + _463;
   float _495 = _494 + _344;
   float _496 = _495 + _479;
   float _497 = _496 + _482;
   int _498 = c_bool & 64;
   bool _499 = (_498 == 0);
   if (!_499)
   {
      float _501 = 1.0f - _46.y;
      float _502 = _113 * _501;
      float _503 = _502 * _464;
      float _504 = _503 + _46.y;
      _506 = _504;
   }
   else
   {
      _506 = _46.y;
   }
   float _507 = _345 + _215;
   float _508 = _346 + _216;
   float _509 = _347 + _217;
   int _510 = c_bool & 32768;
   bool _511 = (_510 == 0);
   if (!_511)
   {
      float _523 = (c_ps_user[11].x) * _55;
      float _524 = (c_ps_user[11].x) * _56;
      float _525 = (c_ps_user[11].x) * _57;
      float _527 = _64 + 1.0f;
      float _528 = _65 + 1.0f;
      float _529 = _66 + 1.0f;
      float _530 = _527 * 0.5f;
      float _531 = _528 * 0.5f;
      float _532 = _529 * 0.5f;
      float _533 = saturate(_530);
      float _534 = saturate(_531);
      float _535 = saturate(_532);
      float _536 = (c_ps_user[11].y) * _533;
      float _537 = _534 * (c_ps_user[11].y);
      float _538 = _535 * (c_ps_user[11].y);
      float _548 = (c_ps_user[11].z) * _46.x;
      float _550 = (c_ps_user[11].w) * _507;
      float _553 = (c_ps_user[12].x) * _113;
      float _555 = (c_ps_user[12].y) * _46.y;
      float _560 = -0.0f - _73;
      float _561 = -0.0f - _74;
      float _562 = -0.0f - _75;
      float _563 = dot(float3(_560, _561, _562), float3(_64, _65, _66));
      float _564 = 1.0f - _563;
      float _565 = _564 * _564;
      float _566 = _564 * 0.9722222089767456f;
      float _567 = _565 * _565;
      float _568 = _567 * _566;
      float _569 = _568 + 0.02777777425944805f;
      float _570 = _569 * (c_ps_user[13].z);
      float _572 = (c_ps_user[13].w) * _484;
      float _573 = (c_ps_user[13].w) * _489;
      float _574 = (c_ps_user[13].w) * _494;
      float _577 = (c_ps_user[14].y) * _470;
      float _578 = (c_ps_user[14].y) * _471;
      float _579 = (c_ps_user[14].y) * _472;
      float _581 = (c_ps_user[14].z) * _477;
      float _582 = (c_ps_user[14].z) * _478;
      float _583 = (c_ps_user[14].z) * _479;
      float _585 = (c_ps_user[14].w) * _342;
      float _586 = (c_ps_user[14].w) * _343;
      float _587 = (c_ps_user[14].w) * _344;
      float _592 = (c_ps_user[15].z) * _345;
      float _593 = (c_ps_user[15].z) * _346;
      float _594 = (c_ps_user[15].z) * _347;
      float _595 = (c_ps_user[15].y) + (c_ps_user[15].x);
      float _596 = c_specular_scale.x * _595;
      float _597 = _596 * _507;
      float _598 = _577 + (c_ps_user[13].x);
      float _599 = _598 + _581;
      float _600 = c_diffuse_scale.x * _599;
      float _601 = _600 * c_mat_diffuse.x;
      float _602 = (c_ps_user[13].y) + _548;
      float _603 = _602 * c_specular_scale.x;
      float _604 = _603 + _597;
      float _605 = _604 * c_mat_specular.x;
      float _606 = _536 + _523;
      float _607 = _606 + _550;
      float _608 = _607 + _553;
      float _609 = _608 + _555;
      float _610 = _609 + _572;
      float _611 = _610 + _585;
      float _612 = _611 + _592;
      float _613 = _612 + _570;
      float _614 = _613 + _601;
      float _615 = _614 + _605;
      float _616 = c_specular_scale.y * _595;
      float _617 = _616 * _508;
      float _618 = _578 + (c_ps_user[13].x);
      float _619 = _618 + _582;
      float _620 = c_diffuse_scale.y * _619;
      float _621 = _620 * c_mat_diffuse.y;
      float _622 = _602 * c_specular_scale.y;
      float _623 = _622 + _617;
      float _624 = _623 * c_mat_specular.y;
      float _625 = _550 + _524;
      float _626 = _625 + _537;
      float _627 = _626 + _553;
      float _628 = _627 + _555;
      float _629 = _628 + _573;
      float _630 = _629 + _586;
      float _631 = _630 + _593;
      float _632 = _631 + _570;
      float _633 = _632 + _621;
      float _634 = _633 + _624;
      float _635 = c_specular_scale.z * _595;
      float _636 = _635 * _509;
      float _637 = _579 + (c_ps_user[13].x);
      float _638 = _637 + _583;
      float _639 = c_diffuse_scale.z * _638;
      float _640 = _639 * c_mat_diffuse.z;
      float _641 = _602 * c_specular_scale.z;
      float _642 = _641 + _636;
      float _643 = _642 * c_mat_specular.z;
      float _644 = _550 + _525;
      float _645 = _644 + _538;
      float _646 = _645 + _553;
      float _647 = _646 + _555;
      float _648 = _647 + _574;
      float _649 = _648 + _587;
      float _650 = _649 + _594;
      float _651 = _650 + _570;
      float _652 = _651 + _640;
      float _653 = _652 + _643;
      float _654 = saturate(_615);
      float _655 = saturate(_634);
      float _656 = saturate(_653);
      _677 = _654;
      _678 = _655;
      _679 = _656;
   }
   else
   {
      float _658 = c_diffuse_scale.x * c_mat_diffuse.x;
      float _659 = _658 * _55;
      float _660 = _659 * _487;
      float _661 = _660 * _506;
      float _662 = c_diffuse_scale.y * c_mat_diffuse.y;
      float _663 = _662 * _56;
      float _664 = _663 * _492;
      float _665 = _664 * _506;
      float _666 = c_diffuse_scale.z * c_mat_diffuse.z;
      float _667 = _666 * _57;
      float _668 = _667 * _497;
      float _669 = _668 * _506;
      float _670 = _507 * _46.x;
      float _671 = _508 * _46.x;
      float _672 = _509 * _46.x;
      float _673 = _661 + _670;
      float _674 = _665 + _671;
      float _675 = _669 + _672;
      _677 = _673;
      _678 = _674;
      _679 = _675;
   }
   float _684 = c_mat_opacity * _50.w;
   float _685 = _684 * c_opacity_scale;
   float _686 = _685 * c_global_opacity;
   float _687 = _677 * COLOR.x;
   float _688 = _678 * COLOR.y;
   float _689 = _679 * COLOR.z;
   int _690 = c_bool & 16;
   bool _691 = (_690 == 0);
   if (!_691)
   {
      float _695 = c_tone_control.x * _687;
      float _696 = c_tone_control.x * _688;
      float _697 = c_tone_control.x * _689;
      float _699 = log2(_695);
      float _700 = log2(_696);
      float _701 = log2(_697);
      float _702 = _699 * c_tone_control.y;
      float _703 = _700 * c_tone_control.y;
      float _704 = _701 * c_tone_control.y;
      float _705 = exp2(_702);
      float _706 = exp2(_703);
      float _707 = exp2(_704);
      _755 = _705;
      _756 = _706;
      _757 = _707;
   }
   else
   {
      if (_511)
      {
         float _712 = c_tone_control.x * _687;
         float _713 = c_tone_control.x * _688;
         float _714 = c_tone_control.x * _689;
         float _715 = _712 * 2.0f;
         float _716 = _713 * 2.0f;
         float _717 = _714 * 2.0f;
         float _718 = _712 * 0.30000001192092896f;
         float _719 = _713 * 0.30000001192092896f;
         float _720 = _714 * 0.30000001192092896f;
         float _721 = _718 + 0.05000000074505806f;
         float _722 = _719 + 0.05000000074505806f;
         float _723 = _720 + 0.05000000074505806f;
         float _724 = _721 * _715;
         float _725 = _722 * _716;
         float _726 = _723 * _717;
         float _727 = _724 + 0.004000000189989805f;
         float _728 = _725 + 0.004000000189989805f;
         float _729 = _726 + 0.004000000189989805f;
         float _730 = _718 + 0.5f;
         float _731 = _719 + 0.5f;
         float _732 = _720 + 0.5f;
         float _733 = _730 * _715;
         float _734 = _731 * _716;
         float _735 = _732 * _717;
         float _736 = _733 + 0.06000000238418579f;
         float _737 = _734 + 0.06000000238418579f;
         float _738 = _735 + 0.06000000238418579f;
         float _739 = _727 / _736;
         float _740 = _728 / _737;
         float _741 = _729 / _738;
         float _742 = _739 + -0.06666666269302368f;
         float _743 = _740 + -0.06666666269302368f;
         float _744 = _741 + -0.06666666269302368f;
         float _745 = _742 * 1.3790643215179443f;
         float _746 = _743 * 1.3790643215179443f;
         float _747 = _744 * 1.3790643215179443f;
         float _748 = YKR_MaterialCurve(_745, _712);
         float _749 = YKR_MaterialCurve(_746, _713);
         float _750 = YKR_MaterialCurve(_747, _714);
         float _751 = sqrt(_748);
         float _752 = sqrt(_749);
         float _753 = sqrt(_750);
         _755 = _751;
         _756 = _752;
         _757 = _753;
      }
      else
      {
         _755 = _687;
         _756 = _688;
         _757 = _689;
      }
   }
   int _758 = c_bool & 4;
   bool _759 = (_758 == 0);
   if (_759)
   {
      float _761 = saturate(COLOR_1.x);
      float _766 = (c_fog_param[0].x) - _755;
      float _767 = (c_fog_param[0].y) - _756;
      float _768 = (c_fog_param[0].z) - _757;
      float _769 = _766 * _761;
      float _770 = _767 * _761;
      float _771 = _768 * _761;
      _773 = _769;
      _774 = _770;
      _775 = _771;
   }
   else
   {
      _773 = COLOR_1.x;
      _774 = COLOR_1.y;
      _775 = COLOR_1.z;
   }
   float _776 = _773 + _755;
   float _777 = _774 + _756;
   float _778 = _775 + _757;
   if (!_511)
   {
      float _786 = (c_ps_user[3].x) * (c_ps_user[12].z);
      float _787 = (c_ps_user[3].y) * (c_ps_user[12].z);
      float _788 = (c_ps_user[3].z) * (c_ps_user[12].z);
      float _790 = (c_ps_user[12].w) * COLOR.x;
      float _791 = (c_ps_user[12].w) * COLOR.y;
      float _792 = (c_ps_user[12].w) * COLOR.z;
      float _793 = _790 + _776;
      float _794 = _793 + _786;
      float _795 = _791 + _777;
      float _796 = _795 + _787;
      float _797 = _792 + _778;
      float _798 = _797 + _788;
      _800 = _794;
      _801 = _796;
      _802 = _798;
   }
   else
   {
      _800 = _776;
      _801 = _777;
      _802 = _778;
   }
   float _808 = (c_ps_col_scl[0].x) * _800;
   float _809 = (c_ps_col_scl[0].y) * _801;
   float _810 = (c_ps_col_scl[0].z) * _802;
   float _811 = (c_ps_col_scl[0].w) * _686;
   SV_Target.x = _808;
   SV_Target.y = _809;
   SV_Target.z = _810;
   SV_Target.w = _811;
   int _814 = c_bool & 8;
   bool _815 = (_814 != 0);
   float _816 = select(_815, _800, _808);
   float _817 = select(_815, _801, _809);
   float _818 = select(_815, _802, _810);
   float _819 = select(_815, _686, _811);
   SV_Target.x = _816;
   SV_Target.y = _817;
   SV_Target.z = _818;
   SV_Target.w = _819;
   return SV_Target;
}
