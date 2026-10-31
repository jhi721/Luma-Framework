// Yakuza Kiwami material s_b1dzt[hair][offset][makeup] (0x4BE63835, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
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
    linear float4 TEXCOORD : TEXCOORD,
    linear float4 TEXCOORD_1 : TEXCOORD1,
    linear float4 TEXCOORD_2 : TEXCOORD2,
    linear float4 TEXCOORD_3 : TEXCOORD3,
    linear float4 TEXCOORD_4 : TEXCOORD4,
    linear float4 TEXCOORD_5 : TEXCOORD5,
    linear float TEXCOORD_6 : TEXCOORD6,
    noperspective float4 SV_Position : SV_Position) : SV_Target
{
   float4 SV_Target = 0;
   float4 _56 = s2_t.Sample(s2_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _60 = _56.x + -0.49803921580314636f;
   float4 _61 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _66 = _61.x * _61.x;
   float _67 = _61.y * _61.y;
   float _68 = _61.z * _61.z;
   float4 _69 = s5_t.Sample(s5_s, float2(TEXCOORD.x, TEXCOORD.y));
   bool _72 = (_69.y < 0.5f);
   float _73 = _60 * 0.25f;
   float _74 = _73 + 0.9980392456054688f;
   float _75 = _69.y * _74;
   float _76 = 1.0f - _69.y;
   float _77 = 1.0019607543945312f - _73;
   float _78 = _76 * _77;
   float _79 = 1.0f - _78;
   float _80 = select(_72, _75, _79);
   float _83 = c_mat_power * _56.z;
   float _84 = max(1.0f, _83);
   float _85 = _69.x * 2.0f;
   float _86 = _80 * 2.0f;
   float _87 = _85 + -1.0f;
   float _88 = _86 + -1.0f;
   float _89 = dot(float2(_87, _88), float2(_87, _88));
   float _90 = 1.0f - _89;
   float _91 = saturate(_90);
   float _92 = sqrt(_91);
   float _93 = TEXCOORD_2.y * TEXCOORD_3.z;
   float _94 = TEXCOORD_2.z * TEXCOORD_3.y;
   float _95 = _93 - _94;
   float _96 = TEXCOORD_2.z * TEXCOORD_3.x;
   float _97 = TEXCOORD_2.x * TEXCOORD_3.z;
   float _98 = _96 - _97;
   float _99 = TEXCOORD_2.x * TEXCOORD_3.y;
   float _100 = TEXCOORD_2.y * TEXCOORD_3.x;
   float _101 = _99 - _100;
   float _102 = _87 * TEXCOORD_3.x;
   float _103 = _87 * TEXCOORD_3.y;
   float _104 = _87 * TEXCOORD_3.z;
   float _105 = _88 * TEXCOORD_3.w;
   float _106 = _105 * _95;
   float _107 = _105 * _98;
   float _108 = _105 * _101;
   float _109 = _92 * TEXCOORD_2.x;
   float _110 = _92 * TEXCOORD_2.y;
   float _111 = _92 * TEXCOORD_2.z;
   float _112 = _109 + _102;
   float _113 = _112 + _106;
   float _114 = _110 + _103;
   float _115 = _114 + _107;
   float _116 = _111 + _104;
   float _117 = _116 + _108;
   float _118 = dot(float3(_113, _115, _117), float3(_113, _115, _117));
   float _119 = rsqrt(_118);
   float _120 = _113 * _119;
   float _121 = _115 * _119;
   float _122 = _117 * _119;
   float _127 = dot(float3(TEXCOORD_1.x, TEXCOORD_1.y, TEXCOORD_1.z), float3(TEXCOORD_1.x, TEXCOORD_1.y, TEXCOORD_1.z));
   float _128 = rsqrt(_127);
   float _129 = _128 * TEXCOORD_1.x;
   float _130 = _128 * TEXCOORD_1.y;
   float _131 = _128 * TEXCOORD_1.z;
   float _132 = _129 + (c_light_vec[0].x);
   float _133 = _130 + (c_light_vec[0].y);
   float _134 = _131 + (c_light_vec[0].z);
   float _135 = dot(float3(_132, _133, _134), float3(_132, _133, _134));
   float _136 = rsqrt(_135);
   float _137 = _132 * _136;
   float _138 = _133 * _136;
   float _139 = _134 * _136;
   float _143 = c_rt_size.z * SV_Position.x;
   float _144 = c_rt_size.w * SV_Position.y;
   int _147 = c_bool & 8;
   bool _148 = (_147 == 0);
   float _154;
   float _155;
   float _167;
   float _168;
   float _334;
   float _356;
   float _376;
   float _396;
   float _397;
   float _398;
   float _433;
   float _434;
   float _435;
   float _464;
   float _465;
   float _466;
   float _776;
   float _777;
   float _778;
   float _855;
   float _856;
   float _857;
   float _873;
   float _874;
   float _875;
   float _900;
   float _901;
   float _902;
   if (!_148)
   {
      float4 _150 = s13_t.Sample(s13_s, float2(_143, _144));
      _154 = _150.x;
      _155 = _150.y;
   }
   else
   {
      _154 = 1.0f;
      _155 = 1.0f;
   }
   if (!_148)
   {
      float _160 = (c_sdw_param_ch[0].x) * _154;
      float _161 = (c_sdw_param_ch[0].z) * _155;
      float _164 = _160 + (c_sdw_param_ch[0].y);
      float _165 = _161 + (c_sdw_param_ch[0].w);
      _167 = _164;
      _168 = _165;
   }
   else
   {
      _167 = _154;
      _168 = _155;
   }
   float _169 = min(_167, _168);
   float _174 = (c_light_vec[1].x) + _129;
   float _175 = (c_light_vec[1].y) + _130;
   float _176 = (c_light_vec[1].z) + _131;
   float _177 = dot(float3(_174, _175, _176), float3(_174, _175, _176));
   float _178 = rsqrt(_177);
   float _179 = _174 * _178;
   float _180 = _175 * _178;
   float _181 = _176 * _178;
   float _182 = _121 * TEXCOORD_3.z;
   float _183 = _122 * TEXCOORD_3.y;
   float _184 = _182 - _183;
   float _185 = _122 * TEXCOORD_3.x;
   float _186 = _120 * TEXCOORD_3.z;
   float _187 = _185 - _186;
   float _188 = _120 * TEXCOORD_3.y;
   float _189 = _121 * TEXCOORD_3.x;
   float _190 = _188 - _189;
   float _191 = _184 * TEXCOORD_3.w;
   float _192 = _187 * TEXCOORD_3.w;
   float _193 = _190 * TEXCOORD_3.w;
   float _194 = _192 * _122;
   float _195 = _193 * _121;
   float _196 = _194 - _195;
   float _197 = _193 * _120;
   float _198 = _191 * _122;
   float _199 = _197 - _198;
   float _200 = _191 * _121;
   float _201 = _192 * _120;
   float _202 = _200 - _201;
   float _203 = -0.0f - _137;
   float _204 = -0.0f - _138;
   float _205 = -0.0f - _139;
   float _206 = dot(float3(_120, _121, _122), float3(_203, _204, _205));
   float _207 = dot(float3(_203, _204, _205), float3(_191, _192, _193));
   float _208 = TEXCOORD.z * 0.20000000298023224f;
   float _209 = _207 * _208;
   float _210 = _209 + 0.5f;
   float4 _211 = s7_t.Sample(s7_s, float2(_206, _210));
   float _213 = dot(float3(_203, _204, _205), float3(_196, _199, _202));
   float _214 = TEXCOORD.w * 0.20000000298023224f;
   float _215 = _213 * _214;
   float _216 = _215 + 0.5f;
   float4 _217 = s7_t.Sample(s7_s, float2(_206, _216));
   float _219 = _217.x * _211.x;
   float _220 = -0.0f - _179;
   float _221 = -0.0f - _180;
   float _222 = -0.0f - _181;
   float _223 = dot(float3(_120, _121, _122), float3(_220, _221, _222));
   float _224 = dot(float3(_220, _221, _222), float3(_191, _192, _193));
   float _225 = _224 * _208;
   float _226 = _225 + 0.5f;
   float4 _227 = s7_t.Sample(s7_s, float2(_223, _226));
   float _229 = dot(float3(_220, _221, _222), float3(_196, _199, _202));
   float _230 = _229 * _214;
   float _231 = _230 + 0.5f;
   float4 _232 = s7_t.Sample(s7_s, float2(_223, _231));
   float _234 = _232.x * _227.x;
   float _235 = log2(_219);
   float _236 = log2(_234);
   float _237 = _235 * _84;
   float _238 = _236 * _84;
   float _239 = exp2(_237);
   float _240 = exp2(_238);
   float _245 = c_mat_fresnel * c_tone_control.w;
   float _246 = _84 * 0.03974359855055809f;
   float _247 = _246 + 0.0856831967830658f;
   float _248 = _240 * _247;
   float _249 = dot(float3(_129, _130, _131), float3(_137, _138, _139));
   float _250 = dot(float3(_129, _130, _131), float3(_179, _180, _181));
   float _251 = 1.0f - _249;
   float _252 = 1.0f - _250;
   float _253 = _251 * _251;
   float _254 = _252 * _252;
   float _255 = 1.0f - _245;
   float _256 = _253 * _253;
   float _257 = _256 * _251;
   float _258 = _257 * _255;
   float _259 = _252 * _255;
   float _260 = _254 * _254;
   float _261 = _260 * _259;
   float _262 = _258 + _245;
   float _263 = _261 + _245;
   float _264 = _248 * _263;
   float _273 = c_specular_scale.x * c_mat_specular.x;
   float _274 = c_specular_scale.y * c_mat_specular.y;
   float _275 = c_specular_scale.z * c_mat_specular.z;
   float _280 = _169 * _247;
   float _281 = _280 * _239;
   float _282 = _281 * _262;
   float _283 = _282 * (c_light_col[0].x);
   float _284 = _282 * (c_light_col[0].y);
   float _285 = _282 * (c_light_col[0].z);
   float _290 = _264 * (c_light_col[1].x);
   float _291 = _264 * (c_light_col[1].y);
   float _292 = _264 * (c_light_col[1].z);
   float _293 = _283 + _290;
   float _294 = _284 + _291;
   float _295 = _285 + _292;
   float _296 = _273 * _293;
   float _297 = _274 * _294;
   float _298 = _275 * _295;
   float _304 = max(_66, _67);
   float _305 = max(_304, _68);
   float _306 = min(_66, _67);
   float _307 = min(_306, _68);
   float _308 = _307 + _305;
   float _309 = _308 * 0.5f;
   float _310 = (c_ps_scratch[10].z) * 2.0f;
   float _311 = _310 + -1.0f;
   bool _312 = (_311 > 0.0f);
   float _313 = float((bool)_312);
   float _314 = 1.0f - _309;
   float _315 = 2.0f - _310;
   float _316 = _314 * _315;
   float _317 = 1.0f - _316;
   float _318 = _317 * _313;
   float _319 = _309 * _310;
   float _320 = 1.0f - _313;
   float _321 = _319 * _320;
   float _322 = _318 + _321;
   bool _323 = ((c_ps_scratch[10].y) > 0.0f);
   if (_323)
   {
      bool _325 = (_322 < 0.5f);
      if (_325)
      {
         float _327 = (c_ps_scratch[10].y) + 1.0f;
         float _328 = _322 * _327;
         _334 = _328;
      }
      else
      {
         float _330 = _322 + (c_ps_scratch[10].y);
         float _331 = _322 * (c_ps_scratch[10].y);
         float _332 = _330 - _331;
         _334 = _332;
      }
      float _335 = _322 * 2.0f;
      float _336 = _335 - _334;
      float _337 = (c_ps_scratch[10].x) + 1.3333333730697632f;
      float _338 = frac(_337);
      bool _339 = (_338 < 0.1666666716337204f);
      if (_339)
      {
         float _341 = _334 - _336;
         float _342 = _338 * 6.0f;
         float _343 = _342 * _341;
         float _344 = _343 + _336;
         _356 = _344;
      }
      else
      {
         bool _346 = (_338 < 0.5f);
         if (!_346)
         {
            bool _348 = (_338 < 0.6666666865348816f);
            if (_348)
            {
               float _350 = _334 - _336;
               float _351 = 0.6666666865348816f - _338;
               float _352 = _351 * 6.0f;
               float _353 = _352 * _350;
               float _354 = _353 + _336;
               _356 = _354;
            }
            else
            {
               _356 = _336;
            }
         }
         else
         {
            _356 = _334;
         }
      }
      float _357 = (c_ps_scratch[10].x) + 1.0f;
      float _358 = frac(_357);
      bool _359 = (_358 < 0.1666666716337204f);
      if (_359)
      {
         float _361 = _334 - _336;
         float _362 = _361 * 6.0f;
         float _363 = _362 * _358;
         float _364 = _363 + _336;
         _376 = _364;
      }
      else
      {
         bool _366 = (_358 < 0.5f);
         if (!_366)
         {
            bool _368 = (_358 < 0.6666666865348816f);
            if (_368)
            {
               float _370 = _334 - _336;
               float _371 = 0.6666666865348816f - _358;
               float _372 = _370 * 6.0f;
               float _373 = _372 * _371;
               float _374 = _373 + _336;
               _376 = _374;
            }
            else
            {
               _376 = _336;
            }
         }
         else
         {
            _376 = _334;
         }
      }
      float _377 = (c_ps_scratch[10].x) + 0.6666666269302368f;
      float _378 = frac(_377);
      bool _379 = (_378 < 0.1666666716337204f);
      if (_379)
      {
         float _381 = _334 - _336;
         float _382 = _381 * 6.0f;
         float _383 = _382 * _378;
         float _384 = _383 + _336;
         _396 = _356;
         _397 = _376;
         _398 = _384;
      }
      else
      {
         bool _386 = (_378 < 0.5f);
         if (!_386)
         {
            bool _388 = (_378 < 0.6666666865348816f);
            if (_388)
            {
               float _390 = _334 - _336;
               float _391 = 0.6666666865348816f - _378;
               float _392 = _390 * 6.0f;
               float _393 = _392 * _391;
               float _394 = _393 + _336;
               _396 = _356;
               _397 = _376;
               _398 = _394;
            }
            else
            {
               _396 = _356;
               _397 = _376;
               _398 = _336;
            }
         }
         else
         {
            _396 = _356;
            _397 = _376;
            _398 = _334;
         }
      }
   }
   else
   {
      _396 = _322;
      _397 = _322;
      _398 = _322;
   }
   int _404 = c_bool & 1;
   bool _405 = (_404 == 0);
   if (!_405)
   {
      float _407 = -0.0f - TEXCOORD_4.x;
      float _408 = -0.0f - TEXCOORD_4.y;
      float _409 = -0.0f - TEXCOORD_4.z;
      float _410 = dot(float3(_120, _121, _122), float3(_407, _408, _409));
      float _411 = saturate(_410);
      float _412 = _411 * TEXCOORD_1.w;
      float _413 = _411 * TEXCOORD_2.w;
      float _414 = _411 * TEXCOORD_4.w;
      float _415 = max(0.0f, _412);
      float _416 = max(0.0f, _413);
      float _417 = max(0.0f, _414);
      float _418 = -0.0f - TEXCOORD_5.x;
      float _419 = -0.0f - TEXCOORD_5.y;
      float _420 = -0.0f - TEXCOORD_5.z;
      float _421 = dot(float3(_120, _121, _122), float3(_418, _419, _420));
      float _422 = saturate(_421);
      float _423 = _422 * COLOR.w;
      float _424 = _422 * COLOR_1.w;
      float _425 = _422 * TEXCOORD_5.w;
      float _426 = max(0.0f, _423);
      float _427 = max(0.0f, _424);
      float _428 = max(0.0f, _425);
      float _429 = _426 + _415;
      float _430 = _427 + _416;
      float _431 = _428 + _417;
      _433 = _429;
      _434 = _430;
      _435 = _431;
   }
   else
   {
      _433 = 0.0f;
      _434 = 0.0f;
      _435 = 0.0f;
   }
   float _436 = c_rt_size.z * SV_Position.x;
   float _437 = c_rt_size.w * SV_Position.y;
   float4 _438 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_436, _437)));
   int _444 = c_bool & 16384;
   bool _445 = (_444 == 0);
   if (!_445)
   {
      float _447 = _438.x * 4.0f;
      float _448 = _438.y * 4.0f;
      float _449 = _438.z * 4.0f;
      _464 = _447;
      _465 = _448;
      _466 = _449;
   }
   else
   {
      int _451 = c_bool & 8192;
      bool _452 = (_451 == 0);
      if (!_452)
      {
         float _454 = _438.x * 2.0f;
         float _455 = _438.y * 2.0f;
         float _456 = _438.z * 2.0f;
         _464 = _454;
         _465 = _455;
         _466 = _456;
      }
      else
      {
         float _460 = _438.x / c_tone_control.z;
         float _461 = _438.y / c_tone_control.z;
         float _462 = _438.z / c_tone_control.z;
         _464 = _460;
         _465 = _461;
         _466 = _462;
      }
   }
   float _467 = _464 + _433;
   float _468 = _465 + _434;
   float _469 = _466 + _435;
   float _532 = dot(float3(_120, _121, _122), float3(_120, _121, _122));
   float _533 = rsqrt(_532);
   float _534 = _533 * _120;
   float _535 = _533 * _121;
   float _536 = _533 * _122;
   float _537 = dot(float4((c_ps_scratch[80].x), (c_ps_scratch[80].y), (c_ps_scratch[80].z), (c_ps_scratch[80].w)), float4(_534, _535, _536, 1.0f));
   float _538 = dot(float4((c_ps_scratch[81].x), (c_ps_scratch[81].y), (c_ps_scratch[81].z), (c_ps_scratch[81].w)), float4(_534, _535, _536, 1.0f));
   float _539 = dot(float4((c_ps_scratch[82].x), (c_ps_scratch[82].y), (c_ps_scratch[82].z), (c_ps_scratch[82].w)), float4(_534, _535, _536, 1.0f));
   float _540 = dot(float4((c_ps_scratch[83].x), (c_ps_scratch[83].y), (c_ps_scratch[83].z), (c_ps_scratch[83].w)), float4(_534, _535, _536, 1.0f));
   float _541 = dot(float4(_534, _535, _536, 1.0f), float4(_537, _538, _539, _540));
   float _542 = dot(float4((c_ps_scratch[84].x), (c_ps_scratch[84].y), (c_ps_scratch[84].z), (c_ps_scratch[84].w)), float4(_534, _535, _536, 1.0f));
   float _543 = dot(float4((c_ps_scratch[85].x), (c_ps_scratch[85].y), (c_ps_scratch[85].z), (c_ps_scratch[85].w)), float4(_534, _535, _536, 1.0f));
   float _544 = dot(float4((c_ps_scratch[86].x), (c_ps_scratch[86].y), (c_ps_scratch[86].z), (c_ps_scratch[86].w)), float4(_534, _535, _536, 1.0f));
   float _545 = dot(float4((c_ps_scratch[87].x), (c_ps_scratch[87].y), (c_ps_scratch[87].z), (c_ps_scratch[87].w)), float4(_534, _535, _536, 1.0f));
   float _546 = dot(float4(_534, _535, _536, 1.0f), float4(_542, _543, _544, _545));
   float _547 = dot(float4((c_ps_scratch[88].x), (c_ps_scratch[88].y), (c_ps_scratch[88].z), (c_ps_scratch[88].w)), float4(_534, _535, _536, 1.0f));
   float _548 = dot(float4((c_ps_scratch[89].x), (c_ps_scratch[89].y), (c_ps_scratch[89].z), (c_ps_scratch[89].w)), float4(_534, _535, _536, 1.0f));
   float _549 = dot(float4((c_ps_scratch[90].x), (c_ps_scratch[90].y), (c_ps_scratch[90].z), (c_ps_scratch[90].w)), float4(_534, _535, _536, 1.0f));
   float _550 = dot(float4((c_ps_scratch[91].x), (c_ps_scratch[91].y), (c_ps_scratch[91].z), (c_ps_scratch[91].w)), float4(_534, _535, _536, 1.0f));
   float _551 = dot(float4(_534, _535, _536, 1.0f), float4(_547, _548, _549, _550));
   float _552 = max(_541, 0.0625f);
   float _553 = max(_546, 0.0625f);
   float _554 = max(_551, 0.0625f);
   float _555 = _552 * (c_ps_scratch[79].x);
   float _556 = _553 * (c_ps_scratch[79].x);
   float _557 = _554 * (c_ps_scratch[79].x);
   float _562 = -0.0f - (c_light_vec[0].x);
   float _563 = -0.0f - (c_light_vec[0].y);
   float _564 = -0.0f - (c_light_vec[0].z);
   float _565 = dot(float3(_120, _121, _122), float3(_562, _563, _564));
   float _566 = -0.0f - (c_light_vec[1].x);
   float _567 = -0.0f - (c_light_vec[1].y);
   float _568 = -0.0f - (c_light_vec[1].z);
   float _569 = dot(float3(_120, _121, _122), float3(_566, _567, _568));
   float _578 = _121 * 0.5f;
   float _579 = _578 + 0.5f;
   float _580 = (c_light_col[4].x) - (c_light_vec[4].x);
   float _581 = (c_light_col[4].y) - (c_light_vec[4].y);
   float _582 = (c_light_col[4].z) - (c_light_vec[4].z);
   float _583 = _580 * _579;
   float _584 = _581 * _579;
   float _585 = _582 * _579;
   float _586 = saturate(_565);
   float _587 = saturate(_569);
   float _592 = (c_light_col[0].x) * _586;
   float _593 = (c_light_col[0].y) * _586;
   float _594 = (c_light_col[0].z) * _586;
   float _599 = (c_light_col[1].x) * _587;
   float _600 = (c_light_col[1].y) * _587;
   float _601 = (c_light_col[1].z) * _587;
   int _602 = c_bool & 32768;
   bool _603 = (_602 == 0);
   float _612 = (c_light_vec[4].x) + _555;
   float _613 = _612 + _583;
   float _614 = (c_light_vec[4].y) + _556;
   float _615 = _614 + _584;
   float _616 = (c_light_vec[4].z) + _557;
   float _617 = _616 + _585;
   if (!_603)
   {
      float _621 = (c_ps_user[11].x) * _396;
      float _622 = (c_ps_user[11].x) * _397;
      float _623 = (c_ps_user[11].x) * _398;
      float _625 = _120 + 1.0f;
      float _626 = _121 + 1.0f;
      float _627 = _122 + 1.0f;
      float _628 = _625 * 0.5f;
      float _629 = _626 * 0.5f;
      float _630 = _627 * 0.5f;
      float _631 = saturate(_628);
      float _632 = saturate(_629);
      float _633 = saturate(_630);
      float _634 = (c_ps_user[11].y) * _631;
      float _635 = _632 * (c_ps_user[11].y);
      float _636 = _633 * (c_ps_user[11].y);
      float _646 = (c_ps_user[11].z) * _56.y;
      float _648 = (c_ps_user[11].w) * _296;
      float _651 = (c_ps_user[12].x) * _169;
      float _657 = -0.0f - _129;
      float _658 = -0.0f - _130;
      float _659 = -0.0f - _131;
      float _660 = dot(float3(_657, _658, _659), float3(_120, _121, _122));
      float _661 = 1.0f - _660;
      float _662 = _661 * _661;
      float _663 = _661 * 0.9722222089767456f;
      float _664 = _662 * _662;
      float _665 = _664 * _663;
      float _666 = _665 + 0.02777777425944805f;
      float _667 = _666 * (c_ps_user[13].z);
      float _669 = (c_ps_user[13].w) * _613;
      float _670 = (c_ps_user[13].w) * _615;
      float _671 = (c_ps_user[13].w) * _617;
      float _674 = (c_ps_user[14].y) * _592;
      float _675 = (c_ps_user[14].y) * _593;
      float _676 = (c_ps_user[14].y) * _594;
      float _678 = (c_ps_user[14].z) * _599;
      float _679 = (c_ps_user[14].z) * _600;
      float _680 = (c_ps_user[14].z) * _601;
      float _682 = (c_ps_user[14].w) * _467;
      float _683 = (c_ps_user[14].w) * _468;
      float _684 = (c_ps_user[14].w) * _469;
      float _688 = (c_ps_user[15].y) + (c_ps_user[15].x);
      float _689 = c_specular_scale.x * _688;
      float _690 = _689 * _296;
      float _691 = _674 + (c_ps_user[13].x);
      float _692 = _691 + _678;
      float _693 = c_diffuse_scale.x * _692;
      float _694 = _693 * c_mat_diffuse.x;
      float _695 = (c_ps_user[13].y) + _646;
      float _696 = _695 * c_specular_scale.x;
      float _697 = _696 + _690;
      float _698 = _697 * c_mat_specular.x;
      float _699 = _634 + _621;
      float _700 = _699 + _648;
      float _701 = _700 + (c_ps_user[12].y);
      float _702 = _701 + _651;
      float _703 = _702 + _669;
      float _704 = _703 + _682;
      float _705 = _704 + _667;
      float _706 = _705 + _694;
      float _707 = _706 + _698;
      float _708 = c_specular_scale.y * _688;
      float _709 = _708 * _297;
      float _710 = _675 + (c_ps_user[13].x);
      float _711 = _710 + _679;
      float _712 = c_diffuse_scale.y * _711;
      float _713 = _712 * c_mat_diffuse.y;
      float _714 = _695 * c_specular_scale.y;
      float _715 = _714 + _709;
      float _716 = _715 * c_mat_specular.y;
      float _717 = _648 + _622;
      float _718 = _717 + _635;
      float _719 = _718 + (c_ps_user[12].y);
      float _720 = _719 + _651;
      float _721 = _720 + _670;
      float _722 = _721 + _683;
      float _723 = _722 + _667;
      float _724 = _723 + _713;
      float _725 = _724 + _716;
      float _726 = c_specular_scale.z * _688;
      float _727 = _726 * _298;
      float _728 = _676 + (c_ps_user[13].x);
      float _729 = _728 + _680;
      float _730 = c_diffuse_scale.z * _729;
      float _731 = _730 * c_mat_diffuse.z;
      float _732 = _695 * c_specular_scale.z;
      float _733 = _732 + _727;
      float _734 = _733 * c_mat_specular.z;
      float _735 = _648 + _623;
      float _736 = _735 + _636;
      float _737 = _736 + (c_ps_user[12].y);
      float _738 = _737 + _651;
      float _739 = _738 + _671;
      float _740 = _739 + _684;
      float _741 = _740 + _667;
      float _742 = _741 + _731;
      float _743 = _742 + _734;
      float _744 = saturate(_707);
      float _745 = saturate(_725);
      float _746 = saturate(_743);
      _776 = _744;
      _777 = _745;
      _778 = _746;
   }
   else
   {
      float _748 = _594 * _169;
      float _749 = _617 + _469;
      float _750 = _749 + _601;
      float _751 = _750 + _748;
      float _752 = _593 * _169;
      float _753 = _615 + _468;
      float _754 = _753 + _600;
      float _755 = _754 + _752;
      float _756 = _592 * _169;
      float _757 = _613 + _467;
      float _758 = _757 + _599;
      float _759 = _758 + _756;
      float _760 = c_diffuse_scale.x * c_mat_diffuse.x;
      float _761 = _760 * _396;
      float _762 = _761 * _759;
      float _763 = c_diffuse_scale.y * c_mat_diffuse.y;
      float _764 = _763 * _397;
      float _765 = _764 * _755;
      float _766 = c_diffuse_scale.z * c_mat_diffuse.z;
      float _767 = _766 * _398;
      float _768 = _767 * _751;
      float _769 = _296 * _56.y;
      float _770 = _297 * _56.y;
      float _771 = _298 * _56.y;
      float _772 = _762 + _769;
      float _773 = _765 + _770;
      float _774 = _768 + _771;
      _776 = _772;
      _777 = _773;
      _778 = _774;
   }
   float _783 = (c_ps_scratch[10].w) * _61.w;
   float _784 = _783 * c_mat_opacity;
   float _785 = _784 * c_opacity_scale;
   float _786 = _785 * c_global_opacity;
   float _787 = _776 * COLOR.x;
   float _788 = _777 * COLOR.y;
   float _789 = _778 * COLOR.z;
   int _790 = c_bool & 16;
   bool _791 = (_790 == 0);
   if (!_791)
   {
      float _795 = c_tone_control.x * _787;
      float _796 = c_tone_control.x * _788;
      float _797 = c_tone_control.x * _789;
      float _799 = log2(_795);
      float _800 = log2(_796);
      float _801 = log2(_797);
      float _802 = _799 * c_tone_control.y;
      float _803 = _800 * c_tone_control.y;
      float _804 = _801 * c_tone_control.y;
      float _805 = exp2(_802);
      float _806 = exp2(_803);
      float _807 = exp2(_804);
      _855 = _805;
      _856 = _806;
      _857 = _807;
   }
   else
   {
      if (_603)
      {
         float _812 = c_tone_control.x * _787;
         float _813 = c_tone_control.x * _788;
         float _814 = c_tone_control.x * _789;
         float _815 = _812 * 2.0f;
         float _816 = _813 * 2.0f;
         float _817 = _814 * 2.0f;
         float _818 = _812 * 0.30000001192092896f;
         float _819 = _813 * 0.30000001192092896f;
         float _820 = _814 * 0.30000001192092896f;
         float _821 = _818 + 0.05000000074505806f;
         float _822 = _819 + 0.05000000074505806f;
         float _823 = _820 + 0.05000000074505806f;
         float _824 = _821 * _815;
         float _825 = _822 * _816;
         float _826 = _823 * _817;
         float _827 = _824 + 0.004000000189989805f;
         float _828 = _825 + 0.004000000189989805f;
         float _829 = _826 + 0.004000000189989805f;
         float _830 = _818 + 0.5f;
         float _831 = _819 + 0.5f;
         float _832 = _820 + 0.5f;
         float _833 = _830 * _815;
         float _834 = _831 * _816;
         float _835 = _832 * _817;
         float _836 = _833 + 0.06000000238418579f;
         float _837 = _834 + 0.06000000238418579f;
         float _838 = _835 + 0.06000000238418579f;
         float _839 = _827 / _836;
         float _840 = _828 / _837;
         float _841 = _829 / _838;
         float _842 = _839 + -0.06666666269302368f;
         float _843 = _840 + -0.06666666269302368f;
         float _844 = _841 + -0.06666666269302368f;
         float _845 = _842 * 1.3790643215179443f;
         float _846 = _843 * 1.3790643215179443f;
         float _847 = _844 * 1.3790643215179443f;
         float _848 = YKR_MaterialCurve(_845, _812);
         float _849 = YKR_MaterialCurve(_846, _813);
         float _850 = YKR_MaterialCurve(_847, _814);
         float _851 = sqrt(_848);
         float _852 = sqrt(_849);
         float _853 = sqrt(_850);
         _855 = _851;
         _856 = _852;
         _857 = _853;
      }
      else
      {
         _855 = _787;
         _856 = _788;
         _857 = _789;
      }
   }
   int _858 = c_bool & 4;
   bool _859 = (_858 == 0);
   if (_859)
   {
      float _861 = saturate(COLOR_1.x);
      float _866 = (c_fog_param[0].x) - _855;
      float _867 = (c_fog_param[0].y) - _856;
      float _868 = (c_fog_param[0].z) - _857;
      float _869 = _866 * _861;
      float _870 = _867 * _861;
      float _871 = _868 * _861;
      _873 = _869;
      _874 = _870;
      _875 = _871;
   }
   else
   {
      _873 = COLOR_1.x;
      _874 = COLOR_1.y;
      _875 = COLOR_1.z;
   }
   float _876 = _873 + _855;
   float _877 = _874 + _856;
   float _878 = _875 + _857;
   if (!_603)
   {
      float _886 = (c_ps_user[3].x) * (c_ps_user[12].z);
      float _887 = (c_ps_user[3].y) * (c_ps_user[12].z);
      float _888 = (c_ps_user[3].z) * (c_ps_user[12].z);
      float _890 = (c_ps_user[12].w) * COLOR.x;
      float _891 = (c_ps_user[12].w) * COLOR.y;
      float _892 = (c_ps_user[12].w) * COLOR.z;
      float _893 = _890 + _876;
      float _894 = _893 + _886;
      float _895 = _891 + _877;
      float _896 = _895 + _887;
      float _897 = _892 + _878;
      float _898 = _897 + _888;
      _900 = _894;
      _901 = _896;
      _902 = _898;
   }
   else
   {
      _900 = _876;
      _901 = _877;
      _902 = _878;
   }
   float _905 = _786 - c_mat_punchthrough_ref;
   bool _906 = (_905 < 0.0f);
   if (_906)
      discard;
   float _912 = (c_ps_col_scl[0].x) * _900;
   float _913 = (c_ps_col_scl[0].y) * _901;
   float _914 = (c_ps_col_scl[0].z) * _902;
   float _915 = (c_ps_col_scl[0].w) * _786;
   SV_Target.x = _912;
   SV_Target.y = _913;
   SV_Target.z = _914;
   SV_Target.w = _915;
   int _918 = c_bool & 8;
   bool _919 = (_918 != 0);
   float _920 = select(_919, _900, _912);
   float _921 = select(_919, _901, _913);
   float _922 = select(_919, _902, _914);
   float _923 = select(_919, _786, _915);
   SV_Target.x = _920;
   SV_Target.y = _921;
   SV_Target.z = _922;
   SV_Target.w = _923;
   return SV_Target;
}
