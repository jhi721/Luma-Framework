// Yakuza Kiwami material s_b1d[ref] (0xD3A18946, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
// applied by a script: the inline Hable tonemap's saturate goes through
// YKR_MaterialCurve (tangent continuation in HDR), and the light pre-pass buffer t14 through YKR_LightBuffer.

#include "Includes/Common.hlsl"

Texture2D<float4> s0_t : register(t0);

TextureCube<float4> s1c_t : register(t1);

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
   float4 _46 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _50 = dot(float3(TEXCOORD_2.x, TEXCOORD_2.y, TEXCOORD_2.z), float3(TEXCOORD_2.x, TEXCOORD_2.y, TEXCOORD_2.z));
   float _51 = rsqrt(_50);
   float _52 = _51 * TEXCOORD_2.x;
   float _53 = _51 * TEXCOORD_2.y;
   float _54 = _51 * TEXCOORD_2.z;
   float _59 = dot(float3(TEXCOORD_1.x, TEXCOORD_1.y, TEXCOORD_1.z), float3(TEXCOORD_1.x, TEXCOORD_1.y, TEXCOORD_1.z));
   float _60 = rsqrt(_59);
   float _61 = _60 * TEXCOORD_1.x;
   float _62 = _60 * TEXCOORD_1.y;
   float _63 = _60 * TEXCOORD_1.z;
   float _64 = _61 + (c_light_vec[0].x);
   float _65 = _62 + (c_light_vec[0].y);
   float _66 = _63 + (c_light_vec[0].z);
   float _67 = dot(float3(_64, _65, _66), float3(_64, _65, _66));
   float _68 = rsqrt(_67);
   float _69 = _64 * _68;
   float _70 = _65 * _68;
   float _71 = _66 * _68;
   float _72 = dot(float3(_61, _62, _63), float3(_52, _53, _54));
   float _73 = _72 * 2.0f;
   float _74 = _73 * _52;
   float _75 = _73 * _53;
   float _76 = _73 * _54;
   float _77 = _61 - _74;
   float _78 = _62 - _75;
   float _79 = _63 - _76;
   float _80 = dot(float3(_77, _78, _79), float3(_77, _78, _79));
   float _81 = rsqrt(_80);
   float _82 = _78 * _81;
   float _83 = _79 * _81;
   float _84 = _81 * _77;
   float _85 = -0.0f - _84;
   float4 _86 = s1c_t.Sample(s1c_s, float3(_85, _82, _83));
   float _91 = max(_86.w, 0.003921568859368563f);
   float _92 = 0.25f / _91;
   float _93 = _92 * _86.x;
   float _94 = _92 * _86.y;
   float _95 = _92 * _86.z;
   float _96 = _93 * _93;
   float _97 = _94 * _94;
   float _98 = _95 * _95;
   float _102 = c_rt_size.z * SV_Position.x;
   float _103 = c_rt_size.w * SV_Position.y;
   int _106 = c_bool & 8;
   bool _107 = (_106 == 0);
   float _113;
   float _114;
   float _126;
   float _127;
   float _307;
   float _308;
   float _309;
   float _310;
   float _311;
   float _312;
   float _345;
   float _346;
   float _347;
   float _348;
   float _667;
   float _668;
   float _669;
   float _745;
   float _746;
   float _747;
   float _763;
   float _764;
   float _765;
   float _790;
   float _791;
   float _792;
   if (!_107)
   {
      float4 _109 = s13_t.Sample(s13_s, float2(_102, _103));
      _113 = _109.x;
      _114 = _109.y;
   }
   else
   {
      _113 = 1.0f;
      _114 = 1.0f;
   }
   if (!_107)
   {
      float _119 = (c_sdw_param_ch[0].x) * _113;
      float _120 = (c_sdw_param_ch[0].z) * _114;
      float _123 = _119 + (c_sdw_param_ch[0].y);
      float _124 = _120 + (c_sdw_param_ch[0].w);
      _126 = _123;
      _127 = _124;
   }
   else
   {
      _126 = _113;
      _127 = _114;
   }
   float _128 = min(_126, _127);
   float _137 = (c_light_vec[1].x) + _61;
   float _138 = (c_light_vec[1].y) + _62;
   float _139 = (c_light_vec[1].z) + _63;
   float _140 = dot(float3(_137, _138, _139), float3(_137, _138, _139));
   float _141 = rsqrt(_140);
   float _142 = _137 * _141;
   float _143 = _138 * _141;
   float _144 = _139 * _141;
   float _145 = -0.0f - (c_light_vec[1].x);
   float _146 = -0.0f - (c_light_vec[1].y);
   float _147 = -0.0f - (c_light_vec[1].z);
   float _148 = -0.0f - (c_light_vec[0].x);
   float _149 = -0.0f - (c_light_vec[0].y);
   float _150 = -0.0f - (c_light_vec[0].z);
   float _151 = dot(float3(_77, _78, _79), float3(_148, _149, _150));
   float _152 = dot(float3(_77, _78, _79), float3(_145, _146, _147));
   float _153 = saturate(_151);
   float _154 = saturate(_152);
   float _155 = log2(_153);
   float _156 = log2(_154);
   float _157 = _155 * c_mat_power;
   float _158 = _156 * c_mat_power;
   float _159 = exp2(_157);
   float _160 = exp2(_158);
   float _165 = c_mat_fresnel * c_tone_control.w;
   float _166 = c_mat_power * 0.03974359855055809f;
   float _167 = _166 + 0.0856831967830658f;
   float _168 = _160 * _167;
   float _169 = dot(float3(_61, _62, _63), float3(_69, _70, _71));
   float _170 = dot(float3(_61, _62, _63), float3(_142, _143, _144));
   float _171 = 1.0f - _169;
   float _172 = 1.0f - _170;
   float _173 = _171 * _171;
   float _174 = _172 * _172;
   float _175 = 1.0f - _165;
   float _176 = _173 * _173;
   float _177 = _176 * _171;
   float _178 = _177 * _175;
   float _179 = _172 * _175;
   float _180 = _174 * _174;
   float _181 = _180 * _179;
   float _182 = _178 + _165;
   float _183 = _181 + _165;
   float _184 = dot(float3(_52, _53, _54), float3(_148, _149, _150));
   float _185 = saturate(_184);
   float _186 = dot(float3(_52, _53, _54), float3(_145, _146, _147));
   float _187 = saturate(_186);
   float _188 = _168 * _187;
   float _189 = _188 * _183;
   float _198 = c_specular_scale.x * c_mat_specular.x;
   float _199 = c_specular_scale.y * c_mat_specular.y;
   float _200 = c_specular_scale.z * c_mat_specular.z;
   float _205 = _128 * _167;
   float _206 = _205 * _159;
   float _207 = _206 * _185;
   float _208 = _207 * _182;
   float _209 = (c_light_col[0].x) * _208;
   float _210 = (c_light_col[0].y) * _208;
   float _211 = (c_light_col[0].z) * _208;
   float _216 = (c_light_col[1].x) * _189;
   float _217 = (c_light_col[1].y) * _189;
   float _218 = (c_light_col[1].z) * _189;
   float _219 = _216 + _209;
   float _220 = _217 + _210;
   float _221 = _218 + _211;
   float _222 = _198 * _219;
   float _223 = _199 * _220;
   float _224 = _200 * _221;
   int _228 = c_bool & 1;
   bool _229 = (_228 == 0);
   if (!_229)
   {
      float _231 = -0.0f - TEXCOORD_4.x;
      float _232 = -0.0f - TEXCOORD_4.y;
      float _233 = -0.0f - TEXCOORD_4.z;
      float _234 = dot(float3(_52, _53, _54), float3(_231, _232, _233));
      float _235 = saturate(_234);
      float _236 = _235 * TEXCOORD_1.w;
      float _237 = _235 * TEXCOORD_2.w;
      float _238 = _235 * TEXCOORD_4.w;
      float _239 = max(0.0f, _236);
      float _240 = max(0.0f, _237);
      float _241 = max(0.0f, _238);
      float _242 = -0.0f - TEXCOORD_5.x;
      float _243 = -0.0f - TEXCOORD_5.y;
      float _244 = -0.0f - TEXCOORD_5.z;
      float _245 = dot(float3(_52, _53, _54), float3(_242, _243, _244));
      float _246 = saturate(_245);
      float _247 = _246 * COLOR.w;
      float _248 = _246 * COLOR_1.w;
      float _249 = _246 * TEXCOORD_5.w;
      float _250 = max(0.0f, _247);
      float _251 = max(0.0f, _248);
      float _252 = max(0.0f, _249);
      float _253 = _250 + _239;
      float _254 = _251 + _240;
      float _255 = _252 + _241;
      float _256 = _61 + TEXCOORD_4.x;
      float _257 = _62 + TEXCOORD_4.y;
      float _258 = _63 + TEXCOORD_4.z;
      float _259 = dot(float3(_256, _257, _258), float3(_256, _257, _258));
      float _260 = rsqrt(_259);
      float _261 = _61 + TEXCOORD_5.x;
      float _262 = _62 + TEXCOORD_5.y;
      float _263 = _63 + TEXCOORD_5.z;
      float _264 = dot(float3(_261, _262, _263), float3(_261, _262, _263));
      float _265 = rsqrt(_264);
      float _266 = _261 * _265;
      float _267 = -0.0f - _266;
      float _268 = _262 * _265;
      float _269 = -0.0f - _268;
      float _270 = _263 * _265;
      float _271 = -0.0f - _270;
      float _272 = _256 * _260;
      float _273 = -0.0f - _272;
      float _274 = _257 * _260;
      float _275 = -0.0f - _274;
      float _276 = _258 * _260;
      float _277 = -0.0f - _276;
      float _278 = dot(float3(_273, _275, _277), float3(_52, _53, _54));
      float _279 = dot(float3(_267, _269, _271), float3(_52, _53, _54));
      float _280 = saturate(_278);
      float _281 = saturate(_279);
      float _282 = log2(_280);
      float _283 = log2(_281);
      float _284 = _282 * c_mat_power;
      float _285 = _283 * c_mat_power;
      float _286 = exp2(_284);
      float _287 = exp2(_285);
      float _288 = _286 * TEXCOORD_1.w;
      float _289 = _286 * TEXCOORD_2.w;
      float _290 = _286 * TEXCOORD_4.w;
      float _291 = _287 * COLOR.w;
      float _292 = _287 * COLOR_1.w;
      float _293 = _287 * TEXCOORD_5.w;
      float _294 = _291 + _288;
      float _295 = _292 + _289;
      float _296 = _293 + _290;
      float _297 = max(0.0f, _294);
      float _298 = max(0.0f, _295);
      float _299 = max(0.0f, _296);
      float _300 = _198 * _297;
      float _301 = _199 * _298;
      float _302 = _200 * _299;
      float _303 = min(_300, 1.0f);
      float _304 = min(_301, 1.0f);
      float _305 = min(_302, 1.0f);
      _307 = _303;
      _308 = _304;
      _309 = _305;
      _310 = _253;
      _311 = _254;
      _312 = _255;
   }
   else
   {
      _307 = 0.0f;
      _308 = 0.0f;
      _309 = 0.0f;
      _310 = 0.0f;
      _311 = 0.0f;
      _312 = 0.0f;
   }
   float _313 = c_rt_size.z * SV_Position.x;
   float _314 = c_rt_size.w * SV_Position.y;
   float4 _315 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_313, _314)));
   int _322 = c_bool & 16384;
   bool _323 = (_322 == 0);
   if (!_323)
   {
      float _325 = _315.x * 4.0f;
      float _326 = _315.y * 4.0f;
      float _327 = _315.z * 4.0f;
      float _328 = _315.w * 4.0f;
      _345 = _325;
      _346 = _326;
      _347 = _327;
      _348 = _328;
   }
   else
   {
      int _330 = c_bool & 8192;
      bool _331 = (_330 == 0);
      if (!_331)
      {
         float _333 = _315.x * 2.0f;
         float _334 = _315.y * 2.0f;
         float _335 = _315.z * 2.0f;
         float _336 = _315.w * 2.0f;
         _345 = _333;
         _346 = _334;
         _347 = _335;
         _348 = _336;
      }
      else
      {
         float _340 = _315.x / c_tone_control.z;
         float _341 = _315.y / c_tone_control.z;
         float _342 = _315.z / c_tone_control.z;
         float _343 = _315.w / c_tone_control.z;
         _345 = _340;
         _346 = _341;
         _347 = _342;
         _348 = _343;
      }
   }
   float _349 = _345 + _310;
   float _350 = _346 + _311;
   float _351 = _347 + _312;
   float _352 = _348 + _307;
   float _353 = _348 + _308;
   float _354 = _348 + _309;
   float _417 = dot(float3(_52, _53, _54), float3(_52, _53, _54));
   float _418 = rsqrt(_417);
   float _419 = _418 * _52;
   float _420 = _418 * _53;
   float _421 = _418 * _54;
   float _422 = dot(float4((c_ps_scratch[80].x), (c_ps_scratch[80].y), (c_ps_scratch[80].z), (c_ps_scratch[80].w)), float4(_419, _420, _421, 1.0f));
   float _423 = dot(float4((c_ps_scratch[81].x), (c_ps_scratch[81].y), (c_ps_scratch[81].z), (c_ps_scratch[81].w)), float4(_419, _420, _421, 1.0f));
   float _424 = dot(float4((c_ps_scratch[82].x), (c_ps_scratch[82].y), (c_ps_scratch[82].z), (c_ps_scratch[82].w)), float4(_419, _420, _421, 1.0f));
   float _425 = dot(float4((c_ps_scratch[83].x), (c_ps_scratch[83].y), (c_ps_scratch[83].z), (c_ps_scratch[83].w)), float4(_419, _420, _421, 1.0f));
   float _426 = dot(float4(_419, _420, _421, 1.0f), float4(_422, _423, _424, _425));
   float _427 = dot(float4((c_ps_scratch[84].x), (c_ps_scratch[84].y), (c_ps_scratch[84].z), (c_ps_scratch[84].w)), float4(_419, _420, _421, 1.0f));
   float _428 = dot(float4((c_ps_scratch[85].x), (c_ps_scratch[85].y), (c_ps_scratch[85].z), (c_ps_scratch[85].w)), float4(_419, _420, _421, 1.0f));
   float _429 = dot(float4((c_ps_scratch[86].x), (c_ps_scratch[86].y), (c_ps_scratch[86].z), (c_ps_scratch[86].w)), float4(_419, _420, _421, 1.0f));
   float _430 = dot(float4((c_ps_scratch[87].x), (c_ps_scratch[87].y), (c_ps_scratch[87].z), (c_ps_scratch[87].w)), float4(_419, _420, _421, 1.0f));
   float _431 = dot(float4(_419, _420, _421, 1.0f), float4(_427, _428, _429, _430));
   float _432 = dot(float4((c_ps_scratch[88].x), (c_ps_scratch[88].y), (c_ps_scratch[88].z), (c_ps_scratch[88].w)), float4(_419, _420, _421, 1.0f));
   float _433 = dot(float4((c_ps_scratch[89].x), (c_ps_scratch[89].y), (c_ps_scratch[89].z), (c_ps_scratch[89].w)), float4(_419, _420, _421, 1.0f));
   float _434 = dot(float4((c_ps_scratch[90].x), (c_ps_scratch[90].y), (c_ps_scratch[90].z), (c_ps_scratch[90].w)), float4(_419, _420, _421, 1.0f));
   float _435 = dot(float4((c_ps_scratch[91].x), (c_ps_scratch[91].y), (c_ps_scratch[91].z), (c_ps_scratch[91].w)), float4(_419, _420, _421, 1.0f));
   float _436 = dot(float4(_419, _420, _421, 1.0f), float4(_432, _433, _434, _435));
   float _437 = max(_426, 0.0625f);
   float _438 = max(_431, 0.0625f);
   float _439 = max(_436, 0.0625f);
   float _440 = _437 * (c_ps_scratch[79].x);
   float _441 = _438 * (c_ps_scratch[79].x);
   float _442 = _439 * (c_ps_scratch[79].x);
   float _447 = -0.0f - (c_light_vec[0].x);
   float _448 = -0.0f - (c_light_vec[0].y);
   float _449 = -0.0f - (c_light_vec[0].z);
   float _450 = dot(float3(_52, _53, _54), float3(_447, _448, _449));
   float _451 = -0.0f - (c_light_vec[1].x);
   float _452 = -0.0f - (c_light_vec[1].y);
   float _453 = -0.0f - (c_light_vec[1].z);
   float _454 = dot(float3(_52, _53, _54), float3(_451, _452, _453));
   float _463 = _53 * 0.5f;
   float _464 = _463 + 0.5f;
   float _465 = (c_light_col[4].x) - (c_light_vec[4].x);
   float _466 = (c_light_col[4].y) - (c_light_vec[4].y);
   float _467 = (c_light_col[4].z) - (c_light_vec[4].z);
   float _468 = _465 * _464;
   float _469 = _466 * _464;
   float _470 = _467 * _464;
   float _471 = saturate(_450);
   float _472 = saturate(_454);
   float _477 = (c_light_col[0].x) * _471;
   float _478 = (c_light_col[0].y) * _471;
   float _479 = (c_light_col[0].z) * _471;
   float _484 = (c_light_col[1].x) * _472;
   float _485 = (c_light_col[1].y) * _472;
   float _486 = (c_light_col[1].z) * _472;
   float _487 = _352 + _222;
   float _488 = _353 + _223;
   float _489 = _354 + _224;
   int _490 = c_bool & 32768;
   bool _491 = (_490 == 0);
   float _500 = (c_light_vec[4].x) + _440;
   float _501 = _500 + _468;
   float _502 = (c_light_vec[4].y) + _441;
   float _503 = _502 + _469;
   float _504 = (c_light_vec[4].z) + _442;
   float _505 = _504 + _470;
   if (!_491)
   {
      float _509 = (c_ps_user[11].x) * _96;
      float _510 = (c_ps_user[11].x) * _97;
      float _511 = (c_ps_user[11].x) * _98;
      float _513 = _52 + 1.0f;
      float _514 = _53 + 1.0f;
      float _515 = _54 + 1.0f;
      float _516 = _513 * 0.5f;
      float _517 = _514 * 0.5f;
      float _518 = _515 * 0.5f;
      float _519 = saturate(_516);
      float _520 = saturate(_517);
      float _521 = saturate(_518);
      float _522 = (c_ps_user[11].y) * _519;
      float _523 = _520 * (c_ps_user[11].y);
      float _524 = _521 * (c_ps_user[11].y);
      float _535 = (c_ps_user[11].w) * _487;
      float _538 = (c_ps_user[12].x) * _128;
      float _544 = -0.0f - _61;
      float _545 = -0.0f - _62;
      float _546 = -0.0f - _63;
      float _547 = dot(float3(_544, _545, _546), float3(_52, _53, _54));
      float _548 = 1.0f - _547;
      float _549 = _548 * _548;
      float _550 = _548 * 0.9722222089767456f;
      float _551 = _549 * _549;
      float _552 = _551 * _550;
      float _553 = _552 + 0.02777777425944805f;
      float _554 = _553 * (c_ps_user[13].z);
      float _556 = (c_ps_user[13].w) * _501;
      float _557 = (c_ps_user[13].w) * _503;
      float _558 = (c_ps_user[13].w) * _505;
      float _561 = (c_ps_user[14].y) * _477;
      float _562 = (c_ps_user[14].y) * _478;
      float _563 = (c_ps_user[14].y) * _479;
      float _565 = (c_ps_user[14].z) * _484;
      float _566 = (c_ps_user[14].z) * _485;
      float _567 = (c_ps_user[14].z) * _486;
      float _569 = (c_ps_user[14].w) * _349;
      float _570 = (c_ps_user[14].w) * _350;
      float _571 = (c_ps_user[14].w) * _351;
      float _576 = (c_ps_user[15].z) * _352;
      float _577 = (c_ps_user[15].z) * _353;
      float _578 = (c_ps_user[15].z) * _354;
      float _579 = (c_ps_user[15].y) + (c_ps_user[15].x);
      float _580 = c_specular_scale.x * _579;
      float _581 = _580 * _487;
      float _582 = _561 + (c_ps_user[13].x);
      float _583 = _582 + _565;
      float _584 = c_diffuse_scale.x * _583;
      float _585 = _584 * c_mat_diffuse.x;
      float _586 = (c_ps_user[13].y) + (c_ps_user[11].z);
      float _587 = _586 * c_specular_scale.x;
      float _588 = _587 + _581;
      float _589 = _588 * c_mat_specular.x;
      float _590 = _522 + _509;
      float _591 = _590 + _535;
      float _592 = _591 + (c_ps_user[12].y);
      float _593 = _592 + _538;
      float _594 = _593 + _556;
      float _595 = _594 + _569;
      float _596 = _595 + _576;
      float _597 = _596 + _554;
      float _598 = _597 + _585;
      float _599 = _598 + _589;
      float _600 = c_specular_scale.y * _579;
      float _601 = _600 * _488;
      float _602 = _562 + (c_ps_user[13].x);
      float _603 = _602 + _566;
      float _604 = c_diffuse_scale.y * _603;
      float _605 = _604 * c_mat_diffuse.y;
      float _606 = _586 * c_specular_scale.y;
      float _607 = _606 + _601;
      float _608 = _607 * c_mat_specular.y;
      float _609 = _535 + _510;
      float _610 = _609 + _523;
      float _611 = _610 + (c_ps_user[12].y);
      float _612 = _611 + _538;
      float _613 = _612 + _557;
      float _614 = _613 + _570;
      float _615 = _614 + _577;
      float _616 = _615 + _554;
      float _617 = _616 + _605;
      float _618 = _617 + _608;
      float _619 = c_specular_scale.z * _579;
      float _620 = _619 * _489;
      float _621 = _563 + (c_ps_user[13].x);
      float _622 = _621 + _567;
      float _623 = c_diffuse_scale.z * _622;
      float _624 = _623 * c_mat_diffuse.z;
      float _625 = _586 * c_specular_scale.z;
      float _626 = _625 + _620;
      float _627 = _626 * c_mat_specular.z;
      float _628 = _535 + _511;
      float _629 = _628 + _524;
      float _630 = _629 + (c_ps_user[12].y);
      float _631 = _630 + _538;
      float _632 = _631 + _558;
      float _633 = _632 + _571;
      float _634 = _633 + _578;
      float _635 = _634 + _554;
      float _636 = _635 + _624;
      float _637 = _636 + _627;
      float _638 = saturate(_599);
      float _639 = saturate(_618);
      float _640 = saturate(_637);
      _667 = _638;
      _668 = _639;
      _669 = _640;
   }
   else
   {
      float _642 = _479 * _128;
      float _643 = _505 + _351;
      float _644 = _643 + _486;
      float _645 = _644 + _642;
      float _646 = _478 * _128;
      float _647 = _503 + _350;
      float _648 = _647 + _485;
      float _649 = _648 + _646;
      float _650 = _477 * _128;
      float _651 = _501 + _349;
      float _652 = _651 + _484;
      float _653 = _652 + _650;
      float _654 = c_diffuse_scale.x * c_mat_diffuse.x;
      float _655 = _654 * _96;
      float _656 = _655 * _653;
      float _657 = c_diffuse_scale.y * c_mat_diffuse.y;
      float _658 = _657 * _97;
      float _659 = _658 * _649;
      float _660 = c_diffuse_scale.z * c_mat_diffuse.z;
      float _661 = _660 * _98;
      float _662 = _661 * _645;
      float _663 = _656 + _487;
      float _664 = _659 + _488;
      float _665 = _662 + _489;
      _667 = _663;
      _668 = _664;
      _669 = _665;
   }
   float _674 = c_mat_opacity * _46.w;
   float _675 = _674 * c_opacity_scale;
   float _676 = _675 * c_global_opacity;
   float _677 = _667 * COLOR.x;
   float _678 = _668 * COLOR.y;
   float _679 = _669 * COLOR.z;
   int _680 = c_bool & 16;
   bool _681 = (_680 == 0);
   if (!_681)
   {
      float _685 = c_tone_control.x * _677;
      float _686 = c_tone_control.x * _678;
      float _687 = c_tone_control.x * _679;
      float _689 = log2(_685);
      float _690 = log2(_686);
      float _691 = log2(_687);
      float _692 = _689 * c_tone_control.y;
      float _693 = _690 * c_tone_control.y;
      float _694 = _691 * c_tone_control.y;
      float _695 = exp2(_692);
      float _696 = exp2(_693);
      float _697 = exp2(_694);
      _745 = _695;
      _746 = _696;
      _747 = _697;
   }
   else
   {
      if (_491)
      {
         float _702 = c_tone_control.x * _677;
         float _703 = c_tone_control.x * _678;
         float _704 = c_tone_control.x * _679;
         float _705 = _702 * 2.0f;
         float _706 = _703 * 2.0f;
         float _707 = _704 * 2.0f;
         float _708 = _702 * 0.30000001192092896f;
         float _709 = _703 * 0.30000001192092896f;
         float _710 = _704 * 0.30000001192092896f;
         float _711 = _708 + 0.05000000074505806f;
         float _712 = _709 + 0.05000000074505806f;
         float _713 = _710 + 0.05000000074505806f;
         float _714 = _711 * _705;
         float _715 = _712 * _706;
         float _716 = _713 * _707;
         float _717 = _714 + 0.004000000189989805f;
         float _718 = _715 + 0.004000000189989805f;
         float _719 = _716 + 0.004000000189989805f;
         float _720 = _708 + 0.5f;
         float _721 = _709 + 0.5f;
         float _722 = _710 + 0.5f;
         float _723 = _720 * _705;
         float _724 = _721 * _706;
         float _725 = _722 * _707;
         float _726 = _723 + 0.06000000238418579f;
         float _727 = _724 + 0.06000000238418579f;
         float _728 = _725 + 0.06000000238418579f;
         float _729 = _717 / _726;
         float _730 = _718 / _727;
         float _731 = _719 / _728;
         float _732 = _729 + -0.06666666269302368f;
         float _733 = _730 + -0.06666666269302368f;
         float _734 = _731 + -0.06666666269302368f;
         float _735 = _732 * 1.3790643215179443f;
         float _736 = _733 * 1.3790643215179443f;
         float _737 = _734 * 1.3790643215179443f;
         float _738 = YKR_MaterialCurve(_735, _702);
         float _739 = YKR_MaterialCurve(_736, _703);
         float _740 = YKR_MaterialCurve(_737, _704);
         float _741 = sqrt(_738);
         float _742 = sqrt(_739);
         float _743 = sqrt(_740);
         _745 = _741;
         _746 = _742;
         _747 = _743;
      }
      else
      {
         _745 = _677;
         _746 = _678;
         _747 = _679;
      }
   }
   int _748 = c_bool & 4;
   bool _749 = (_748 == 0);
   if (_749)
   {
      float _751 = saturate(COLOR_1.x);
      float _756 = (c_fog_param[0].x) - _745;
      float _757 = (c_fog_param[0].y) - _746;
      float _758 = (c_fog_param[0].z) - _747;
      float _759 = _756 * _751;
      float _760 = _757 * _751;
      float _761 = _758 * _751;
      _763 = _759;
      _764 = _760;
      _765 = _761;
   }
   else
   {
      _763 = COLOR_1.x;
      _764 = COLOR_1.y;
      _765 = COLOR_1.z;
   }
   float _766 = _763 + _745;
   float _767 = _764 + _746;
   float _768 = _765 + _747;
   if (!_491)
   {
      float _776 = (c_ps_user[3].x) * (c_ps_user[12].z);
      float _777 = (c_ps_user[3].y) * (c_ps_user[12].z);
      float _778 = (c_ps_user[3].z) * (c_ps_user[12].z);
      float _780 = (c_ps_user[12].w) * COLOR.x;
      float _781 = (c_ps_user[12].w) * COLOR.y;
      float _782 = (c_ps_user[12].w) * COLOR.z;
      float _783 = _780 + _766;
      float _784 = _783 + _776;
      float _785 = _781 + _767;
      float _786 = _785 + _777;
      float _787 = _782 + _768;
      float _788 = _787 + _778;
      _790 = _784;
      _791 = _786;
      _792 = _788;
   }
   else
   {
      _790 = _766;
      _791 = _767;
      _792 = _768;
   }
   float _795 = _676 - c_mat_punchthrough_ref;
   bool _796 = (_795 < 0.0f);
   if (_796)
      discard;
   float _802 = (c_ps_col_scl[0].x) * _790;
   float _803 = (c_ps_col_scl[0].y) * _791;
   float _804 = (c_ps_col_scl[0].z) * _792;
   float _805 = (c_ps_col_scl[0].w) * _676;
   SV_Target.x = _802;
   SV_Target.y = _803;
   SV_Target.z = _804;
   SV_Target.w = _805;
   int _808 = c_bool & 8;
   bool _809 = (_808 != 0);
   float _810 = select(_809, _790, _802);
   float _811 = select(_809, _791, _803);
   float _812 = select(_809, _792, _804);
   float _813 = select(_809, _676, _805);
   SV_Target.x = _810;
   SV_Target.y = _811;
   SV_Target.z = _812;
   SV_Target.w = _813;
   return SV_Target;
}
