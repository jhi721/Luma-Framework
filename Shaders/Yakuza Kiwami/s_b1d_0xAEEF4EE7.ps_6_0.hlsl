// Yakuza Kiwami material s_b1d (0xAEEF4EE7, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
// applied by a script: the inline Hable tonemap's saturate goes through
// YKR_MaterialCurve (tangent continuation in HDR), and the light pre-pass buffer t14 through YKR_LightBuffer.

#include "Includes/Common.hlsl"

Texture2D<float4> s0_t : register(t0);

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
   float4 _44 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _49 = _44.x * _44.x;
   float _50 = _44.y * _44.y;
   float _51 = _44.z * _44.z;
   float _54 = dot(float3(TEXCOORD_2.x, TEXCOORD_2.y, TEXCOORD_2.z), float3(TEXCOORD_2.x, TEXCOORD_2.y, TEXCOORD_2.z));
   float _55 = rsqrt(_54);
   float _56 = _55 * TEXCOORD_2.x;
   float _57 = _55 * TEXCOORD_2.y;
   float _58 = _55 * TEXCOORD_2.z;
   float _63 = dot(float3(TEXCOORD_1.x, TEXCOORD_1.y, TEXCOORD_1.z), float3(TEXCOORD_1.x, TEXCOORD_1.y, TEXCOORD_1.z));
   float _64 = rsqrt(_63);
   float _65 = _64 * TEXCOORD_1.x;
   float _66 = _64 * TEXCOORD_1.y;
   float _67 = _64 * TEXCOORD_1.z;
   float _68 = _65 + (c_light_vec[0].x);
   float _69 = _66 + (c_light_vec[0].y);
   float _70 = _67 + (c_light_vec[0].z);
   float _71 = dot(float3(_68, _69, _70), float3(_68, _69, _70));
   float _72 = rsqrt(_71);
   float _73 = _68 * _72;
   float _74 = _69 * _72;
   float _75 = _70 * _72;
   float _79 = c_rt_size.z * SV_Position.x;
   float _80 = c_rt_size.w * SV_Position.y;
   int _83 = c_bool & 8;
   bool _84 = (_83 == 0);
   float _90;
   float _91;
   float _103;
   float _104;
   float _292;
   float _293;
   float _294;
   float _295;
   float _296;
   float _297;
   float _330;
   float _331;
   float _332;
   float _333;
   float _652;
   float _653;
   float _654;
   float _730;
   float _731;
   float _732;
   float _748;
   float _749;
   float _750;
   float _775;
   float _776;
   float _777;
   if (!_84)
   {
      float4 _86 = s13_t.Sample(s13_s, float2(_79, _80));
      _90 = _86.x;
      _91 = _86.y;
   }
   else
   {
      _90 = 1.0f;
      _91 = 1.0f;
   }
   if (!_84)
   {
      float _96 = (c_sdw_param_ch[0].x) * _90;
      float _97 = (c_sdw_param_ch[0].z) * _91;
      float _100 = _96 + (c_sdw_param_ch[0].y);
      float _101 = _97 + (c_sdw_param_ch[0].w);
      _103 = _100;
      _104 = _101;
   }
   else
   {
      _103 = _90;
      _104 = _91;
   }
   float _105 = min(_103, _104);
   float _114 = (c_light_vec[1].x) + _65;
   float _115 = (c_light_vec[1].y) + _66;
   float _116 = (c_light_vec[1].z) + _67;
   float _117 = dot(float3(_114, _115, _116), float3(_114, _115, _116));
   float _118 = rsqrt(_117);
   float _119 = _114 * _118;
   float _120 = _115 * _118;
   float _121 = _116 * _118;
   float _122 = -0.0f - (c_light_vec[1].x);
   float _123 = -0.0f - (c_light_vec[1].y);
   float _124 = -0.0f - (c_light_vec[1].z);
   float _125 = -0.0f - (c_light_vec[0].x);
   float _126 = -0.0f - (c_light_vec[0].y);
   float _127 = -0.0f - (c_light_vec[0].z);
   float _128 = dot(float3(_65, _66, _67), float3(_56, _57, _58));
   float _129 = _128 * 2.0f;
   float _130 = _129 * _56;
   float _131 = _129 * _57;
   float _132 = _129 * _58;
   float _133 = _65 - _130;
   float _134 = _66 - _131;
   float _135 = _67 - _132;
   float _136 = dot(float3(_133, _134, _135), float3(_125, _126, _127));
   float _137 = dot(float3(_133, _134, _135), float3(_122, _123, _124));
   float _138 = saturate(_136);
   float _139 = saturate(_137);
   float _140 = log2(_138);
   float _141 = log2(_139);
   float _142 = _140 * c_mat_power;
   float _143 = _141 * c_mat_power;
   float _144 = exp2(_142);
   float _145 = exp2(_143);
   float _150 = c_mat_fresnel * c_tone_control.w;
   float _151 = c_mat_power * 0.03974359855055809f;
   float _152 = _151 + 0.0856831967830658f;
   float _153 = _145 * _152;
   float _154 = dot(float3(_65, _66, _67), float3(_73, _74, _75));
   float _155 = dot(float3(_65, _66, _67), float3(_119, _120, _121));
   float _156 = 1.0f - _154;
   float _157 = 1.0f - _155;
   float _158 = _156 * _156;
   float _159 = _157 * _157;
   float _160 = 1.0f - _150;
   float _161 = _158 * _158;
   float _162 = _161 * _156;
   float _163 = _162 * _160;
   float _164 = _157 * _160;
   float _165 = _159 * _159;
   float _166 = _165 * _164;
   float _167 = _163 + _150;
   float _168 = _166 + _150;
   float _169 = dot(float3(_56, _57, _58), float3(_125, _126, _127));
   float _170 = saturate(_169);
   float _171 = dot(float3(_56, _57, _58), float3(_122, _123, _124));
   float _172 = saturate(_171);
   float _173 = _153 * _172;
   float _174 = _173 * _168;
   float _183 = c_specular_scale.x * c_mat_specular.x;
   float _184 = c_specular_scale.y * c_mat_specular.y;
   float _185 = c_specular_scale.z * c_mat_specular.z;
   float _190 = _105 * _152;
   float _191 = _190 * _144;
   float _192 = _191 * _170;
   float _193 = _192 * _167;
   float _194 = (c_light_col[0].x) * _193;
   float _195 = (c_light_col[0].y) * _193;
   float _196 = (c_light_col[0].z) * _193;
   float _201 = (c_light_col[1].x) * _174;
   float _202 = (c_light_col[1].y) * _174;
   float _203 = (c_light_col[1].z) * _174;
   float _204 = _201 + _194;
   float _205 = _202 + _195;
   float _206 = _203 + _196;
   float _207 = _183 * _204;
   float _208 = _184 * _205;
   float _209 = _185 * _206;
   int _213 = c_bool & 1;
   bool _214 = (_213 == 0);
   if (!_214)
   {
      float _216 = -0.0f - TEXCOORD_4.x;
      float _217 = -0.0f - TEXCOORD_4.y;
      float _218 = -0.0f - TEXCOORD_4.z;
      float _219 = dot(float3(_56, _57, _58), float3(_216, _217, _218));
      float _220 = saturate(_219);
      float _221 = _220 * TEXCOORD_1.w;
      float _222 = _220 * TEXCOORD_2.w;
      float _223 = _220 * TEXCOORD_4.w;
      float _224 = max(0.0f, _221);
      float _225 = max(0.0f, _222);
      float _226 = max(0.0f, _223);
      float _227 = -0.0f - TEXCOORD_5.x;
      float _228 = -0.0f - TEXCOORD_5.y;
      float _229 = -0.0f - TEXCOORD_5.z;
      float _230 = dot(float3(_56, _57, _58), float3(_227, _228, _229));
      float _231 = saturate(_230);
      float _232 = _231 * COLOR.w;
      float _233 = _231 * COLOR_1.w;
      float _234 = _231 * TEXCOORD_5.w;
      float _235 = max(0.0f, _232);
      float _236 = max(0.0f, _233);
      float _237 = max(0.0f, _234);
      float _238 = _235 + _224;
      float _239 = _236 + _225;
      float _240 = _237 + _226;
      float _241 = _65 + TEXCOORD_4.x;
      float _242 = _66 + TEXCOORD_4.y;
      float _243 = _67 + TEXCOORD_4.z;
      float _244 = dot(float3(_241, _242, _243), float3(_241, _242, _243));
      float _245 = rsqrt(_244);
      float _246 = _65 + TEXCOORD_5.x;
      float _247 = _66 + TEXCOORD_5.y;
      float _248 = _67 + TEXCOORD_5.z;
      float _249 = dot(float3(_246, _247, _248), float3(_246, _247, _248));
      float _250 = rsqrt(_249);
      float _251 = _246 * _250;
      float _252 = -0.0f - _251;
      float _253 = _247 * _250;
      float _254 = -0.0f - _253;
      float _255 = _248 * _250;
      float _256 = -0.0f - _255;
      float _257 = _241 * _245;
      float _258 = -0.0f - _257;
      float _259 = _242 * _245;
      float _260 = -0.0f - _259;
      float _261 = _243 * _245;
      float _262 = -0.0f - _261;
      float _263 = dot(float3(_258, _260, _262), float3(_56, _57, _58));
      float _264 = dot(float3(_252, _254, _256), float3(_56, _57, _58));
      float _265 = saturate(_263);
      float _266 = saturate(_264);
      float _267 = log2(_265);
      float _268 = log2(_266);
      float _269 = _267 * c_mat_power;
      float _270 = _268 * c_mat_power;
      float _271 = exp2(_269);
      float _272 = exp2(_270);
      float _273 = _271 * TEXCOORD_1.w;
      float _274 = _271 * TEXCOORD_2.w;
      float _275 = _271 * TEXCOORD_4.w;
      float _276 = _272 * COLOR.w;
      float _277 = _272 * COLOR_1.w;
      float _278 = _272 * TEXCOORD_5.w;
      float _279 = _276 + _273;
      float _280 = _277 + _274;
      float _281 = _278 + _275;
      float _282 = max(0.0f, _279);
      float _283 = max(0.0f, _280);
      float _284 = max(0.0f, _281);
      float _285 = _183 * _282;
      float _286 = _184 * _283;
      float _287 = _185 * _284;
      float _288 = min(_285, 1.0f);
      float _289 = min(_286, 1.0f);
      float _290 = min(_287, 1.0f);
      _292 = _288;
      _293 = _289;
      _294 = _290;
      _295 = _238;
      _296 = _239;
      _297 = _240;
   }
   else
   {
      _292 = 0.0f;
      _293 = 0.0f;
      _294 = 0.0f;
      _295 = 0.0f;
      _296 = 0.0f;
      _297 = 0.0f;
   }
   float _298 = c_rt_size.z * SV_Position.x;
   float _299 = c_rt_size.w * SV_Position.y;
   float4 _300 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_298, _299)));
   int _307 = c_bool & 16384;
   bool _308 = (_307 == 0);
   if (!_308)
   {
      float _310 = _300.x * 4.0f;
      float _311 = _300.y * 4.0f;
      float _312 = _300.z * 4.0f;
      float _313 = _300.w * 4.0f;
      _330 = _310;
      _331 = _311;
      _332 = _312;
      _333 = _313;
   }
   else
   {
      int _315 = c_bool & 8192;
      bool _316 = (_315 == 0);
      if (!_316)
      {
         float _318 = _300.x * 2.0f;
         float _319 = _300.y * 2.0f;
         float _320 = _300.z * 2.0f;
         float _321 = _300.w * 2.0f;
         _330 = _318;
         _331 = _319;
         _332 = _320;
         _333 = _321;
      }
      else
      {
         float _325 = _300.x / c_tone_control.z;
         float _326 = _300.y / c_tone_control.z;
         float _327 = _300.z / c_tone_control.z;
         float _328 = _300.w / c_tone_control.z;
         _330 = _325;
         _331 = _326;
         _332 = _327;
         _333 = _328;
      }
   }
   float _334 = _330 + _295;
   float _335 = _331 + _296;
   float _336 = _332 + _297;
   float _337 = _333 + _292;
   float _338 = _333 + _293;
   float _339 = _333 + _294;
   float _402 = dot(float3(_56, _57, _58), float3(_56, _57, _58));
   float _403 = rsqrt(_402);
   float _404 = _403 * _56;
   float _405 = _403 * _57;
   float _406 = _403 * _58;
   float _407 = dot(float4((c_ps_scratch[80].x), (c_ps_scratch[80].y), (c_ps_scratch[80].z), (c_ps_scratch[80].w)), float4(_404, _405, _406, 1.0f));
   float _408 = dot(float4((c_ps_scratch[81].x), (c_ps_scratch[81].y), (c_ps_scratch[81].z), (c_ps_scratch[81].w)), float4(_404, _405, _406, 1.0f));
   float _409 = dot(float4((c_ps_scratch[82].x), (c_ps_scratch[82].y), (c_ps_scratch[82].z), (c_ps_scratch[82].w)), float4(_404, _405, _406, 1.0f));
   float _410 = dot(float4((c_ps_scratch[83].x), (c_ps_scratch[83].y), (c_ps_scratch[83].z), (c_ps_scratch[83].w)), float4(_404, _405, _406, 1.0f));
   float _411 = dot(float4(_404, _405, _406, 1.0f), float4(_407, _408, _409, _410));
   float _412 = dot(float4((c_ps_scratch[84].x), (c_ps_scratch[84].y), (c_ps_scratch[84].z), (c_ps_scratch[84].w)), float4(_404, _405, _406, 1.0f));
   float _413 = dot(float4((c_ps_scratch[85].x), (c_ps_scratch[85].y), (c_ps_scratch[85].z), (c_ps_scratch[85].w)), float4(_404, _405, _406, 1.0f));
   float _414 = dot(float4((c_ps_scratch[86].x), (c_ps_scratch[86].y), (c_ps_scratch[86].z), (c_ps_scratch[86].w)), float4(_404, _405, _406, 1.0f));
   float _415 = dot(float4((c_ps_scratch[87].x), (c_ps_scratch[87].y), (c_ps_scratch[87].z), (c_ps_scratch[87].w)), float4(_404, _405, _406, 1.0f));
   float _416 = dot(float4(_404, _405, _406, 1.0f), float4(_412, _413, _414, _415));
   float _417 = dot(float4((c_ps_scratch[88].x), (c_ps_scratch[88].y), (c_ps_scratch[88].z), (c_ps_scratch[88].w)), float4(_404, _405, _406, 1.0f));
   float _418 = dot(float4((c_ps_scratch[89].x), (c_ps_scratch[89].y), (c_ps_scratch[89].z), (c_ps_scratch[89].w)), float4(_404, _405, _406, 1.0f));
   float _419 = dot(float4((c_ps_scratch[90].x), (c_ps_scratch[90].y), (c_ps_scratch[90].z), (c_ps_scratch[90].w)), float4(_404, _405, _406, 1.0f));
   float _420 = dot(float4((c_ps_scratch[91].x), (c_ps_scratch[91].y), (c_ps_scratch[91].z), (c_ps_scratch[91].w)), float4(_404, _405, _406, 1.0f));
   float _421 = dot(float4(_404, _405, _406, 1.0f), float4(_417, _418, _419, _420));
   float _422 = max(_411, 0.0625f);
   float _423 = max(_416, 0.0625f);
   float _424 = max(_421, 0.0625f);
   float _425 = _422 * (c_ps_scratch[79].x);
   float _426 = _423 * (c_ps_scratch[79].x);
   float _427 = _424 * (c_ps_scratch[79].x);
   float _432 = -0.0f - (c_light_vec[0].x);
   float _433 = -0.0f - (c_light_vec[0].y);
   float _434 = -0.0f - (c_light_vec[0].z);
   float _435 = dot(float3(_56, _57, _58), float3(_432, _433, _434));
   float _436 = -0.0f - (c_light_vec[1].x);
   float _437 = -0.0f - (c_light_vec[1].y);
   float _438 = -0.0f - (c_light_vec[1].z);
   float _439 = dot(float3(_56, _57, _58), float3(_436, _437, _438));
   float _448 = _57 * 0.5f;
   float _449 = _448 + 0.5f;
   float _450 = (c_light_col[4].x) - (c_light_vec[4].x);
   float _451 = (c_light_col[4].y) - (c_light_vec[4].y);
   float _452 = (c_light_col[4].z) - (c_light_vec[4].z);
   float _453 = _450 * _449;
   float _454 = _451 * _449;
   float _455 = _452 * _449;
   float _456 = saturate(_435);
   float _457 = saturate(_439);
   float _462 = (c_light_col[0].x) * _456;
   float _463 = (c_light_col[0].y) * _456;
   float _464 = (c_light_col[0].z) * _456;
   float _469 = (c_light_col[1].x) * _457;
   float _470 = (c_light_col[1].y) * _457;
   float _471 = (c_light_col[1].z) * _457;
   float _472 = _337 + _207;
   float _473 = _338 + _208;
   float _474 = _339 + _209;
   int _475 = c_bool & 32768;
   bool _476 = (_475 == 0);
   float _485 = (c_light_vec[4].x) + _425;
   float _486 = _485 + _453;
   float _487 = (c_light_vec[4].y) + _426;
   float _488 = _487 + _454;
   float _489 = (c_light_vec[4].z) + _427;
   float _490 = _489 + _455;
   if (!_476)
   {
      float _494 = (c_ps_user[11].x) * _49;
      float _495 = (c_ps_user[11].x) * _50;
      float _496 = (c_ps_user[11].x) * _51;
      float _498 = _56 + 1.0f;
      float _499 = _57 + 1.0f;
      float _500 = _58 + 1.0f;
      float _501 = _498 * 0.5f;
      float _502 = _499 * 0.5f;
      float _503 = _500 * 0.5f;
      float _504 = saturate(_501);
      float _505 = saturate(_502);
      float _506 = saturate(_503);
      float _507 = (c_ps_user[11].y) * _504;
      float _508 = _505 * (c_ps_user[11].y);
      float _509 = _506 * (c_ps_user[11].y);
      float _520 = (c_ps_user[11].w) * _472;
      float _523 = (c_ps_user[12].x) * _105;
      float _529 = -0.0f - _65;
      float _530 = -0.0f - _66;
      float _531 = -0.0f - _67;
      float _532 = dot(float3(_529, _530, _531), float3(_56, _57, _58));
      float _533 = 1.0f - _532;
      float _534 = _533 * _533;
      float _535 = _533 * 0.9722222089767456f;
      float _536 = _534 * _534;
      float _537 = _536 * _535;
      float _538 = _537 + 0.02777777425944805f;
      float _539 = _538 * (c_ps_user[13].z);
      float _541 = (c_ps_user[13].w) * _486;
      float _542 = (c_ps_user[13].w) * _488;
      float _543 = (c_ps_user[13].w) * _490;
      float _546 = (c_ps_user[14].y) * _462;
      float _547 = (c_ps_user[14].y) * _463;
      float _548 = (c_ps_user[14].y) * _464;
      float _550 = (c_ps_user[14].z) * _469;
      float _551 = (c_ps_user[14].z) * _470;
      float _552 = (c_ps_user[14].z) * _471;
      float _554 = (c_ps_user[14].w) * _334;
      float _555 = (c_ps_user[14].w) * _335;
      float _556 = (c_ps_user[14].w) * _336;
      float _561 = (c_ps_user[15].z) * _337;
      float _562 = (c_ps_user[15].z) * _338;
      float _563 = (c_ps_user[15].z) * _339;
      float _564 = (c_ps_user[15].y) + (c_ps_user[15].x);
      float _565 = c_specular_scale.x * _564;
      float _566 = _565 * _472;
      float _567 = _546 + (c_ps_user[13].x);
      float _568 = _567 + _550;
      float _569 = c_diffuse_scale.x * _568;
      float _570 = _569 * c_mat_diffuse.x;
      float _571 = (c_ps_user[13].y) + (c_ps_user[11].z);
      float _572 = _571 * c_specular_scale.x;
      float _573 = _572 + _566;
      float _574 = _573 * c_mat_specular.x;
      float _575 = _507 + _494;
      float _576 = _575 + _520;
      float _577 = _576 + (c_ps_user[12].y);
      float _578 = _577 + _523;
      float _579 = _578 + _541;
      float _580 = _579 + _554;
      float _581 = _580 + _561;
      float _582 = _581 + _539;
      float _583 = _582 + _570;
      float _584 = _583 + _574;
      float _585 = c_specular_scale.y * _564;
      float _586 = _585 * _473;
      float _587 = _547 + (c_ps_user[13].x);
      float _588 = _587 + _551;
      float _589 = c_diffuse_scale.y * _588;
      float _590 = _589 * c_mat_diffuse.y;
      float _591 = _571 * c_specular_scale.y;
      float _592 = _591 + _586;
      float _593 = _592 * c_mat_specular.y;
      float _594 = _520 + _495;
      float _595 = _594 + _508;
      float _596 = _595 + (c_ps_user[12].y);
      float _597 = _596 + _523;
      float _598 = _597 + _542;
      float _599 = _598 + _555;
      float _600 = _599 + _562;
      float _601 = _600 + _539;
      float _602 = _601 + _590;
      float _603 = _602 + _593;
      float _604 = c_specular_scale.z * _564;
      float _605 = _604 * _474;
      float _606 = _548 + (c_ps_user[13].x);
      float _607 = _606 + _552;
      float _608 = c_diffuse_scale.z * _607;
      float _609 = _608 * c_mat_diffuse.z;
      float _610 = _571 * c_specular_scale.z;
      float _611 = _610 + _605;
      float _612 = _611 * c_mat_specular.z;
      float _613 = _520 + _496;
      float _614 = _613 + _509;
      float _615 = _614 + (c_ps_user[12].y);
      float _616 = _615 + _523;
      float _617 = _616 + _543;
      float _618 = _617 + _556;
      float _619 = _618 + _563;
      float _620 = _619 + _539;
      float _621 = _620 + _609;
      float _622 = _621 + _612;
      float _623 = saturate(_584);
      float _624 = saturate(_603);
      float _625 = saturate(_622);
      _652 = _623;
      _653 = _624;
      _654 = _625;
   }
   else
   {
      float _627 = _464 * _105;
      float _628 = _490 + _336;
      float _629 = _628 + _471;
      float _630 = _629 + _627;
      float _631 = _463 * _105;
      float _632 = _488 + _335;
      float _633 = _632 + _470;
      float _634 = _633 + _631;
      float _635 = _462 * _105;
      float _636 = _486 + _334;
      float _637 = _636 + _469;
      float _638 = _637 + _635;
      float _639 = c_diffuse_scale.x * c_mat_diffuse.x;
      float _640 = _639 * _49;
      float _641 = _640 * _638;
      float _642 = c_diffuse_scale.y * c_mat_diffuse.y;
      float _643 = _642 * _50;
      float _644 = _643 * _634;
      float _645 = c_diffuse_scale.z * c_mat_diffuse.z;
      float _646 = _645 * _51;
      float _647 = _646 * _630;
      float _648 = _641 + _472;
      float _649 = _644 + _473;
      float _650 = _647 + _474;
      _652 = _648;
      _653 = _649;
      _654 = _650;
   }
   float _659 = c_mat_opacity * _44.w;
   float _660 = _659 * c_opacity_scale;
   float _661 = _660 * c_global_opacity;
   float _662 = _652 * COLOR.x;
   float _663 = _653 * COLOR.y;
   float _664 = _654 * COLOR.z;
   int _665 = c_bool & 16;
   bool _666 = (_665 == 0);
   if (!_666)
   {
      float _670 = c_tone_control.x * _662;
      float _671 = c_tone_control.x * _663;
      float _672 = c_tone_control.x * _664;
      float _674 = log2(_670);
      float _675 = log2(_671);
      float _676 = log2(_672);
      float _677 = _674 * c_tone_control.y;
      float _678 = _675 * c_tone_control.y;
      float _679 = _676 * c_tone_control.y;
      float _680 = exp2(_677);
      float _681 = exp2(_678);
      float _682 = exp2(_679);
      _730 = _680;
      _731 = _681;
      _732 = _682;
   }
   else
   {
      if (_476)
      {
         float _687 = c_tone_control.x * _662;
         float _688 = c_tone_control.x * _663;
         float _689 = c_tone_control.x * _664;
         float _690 = _687 * 2.0f;
         float _691 = _688 * 2.0f;
         float _692 = _689 * 2.0f;
         float _693 = _687 * 0.30000001192092896f;
         float _694 = _688 * 0.30000001192092896f;
         float _695 = _689 * 0.30000001192092896f;
         float _696 = _693 + 0.05000000074505806f;
         float _697 = _694 + 0.05000000074505806f;
         float _698 = _695 + 0.05000000074505806f;
         float _699 = _696 * _690;
         float _700 = _697 * _691;
         float _701 = _698 * _692;
         float _702 = _699 + 0.004000000189989805f;
         float _703 = _700 + 0.004000000189989805f;
         float _704 = _701 + 0.004000000189989805f;
         float _705 = _693 + 0.5f;
         float _706 = _694 + 0.5f;
         float _707 = _695 + 0.5f;
         float _708 = _705 * _690;
         float _709 = _706 * _691;
         float _710 = _707 * _692;
         float _711 = _708 + 0.06000000238418579f;
         float _712 = _709 + 0.06000000238418579f;
         float _713 = _710 + 0.06000000238418579f;
         float _714 = _702 / _711;
         float _715 = _703 / _712;
         float _716 = _704 / _713;
         float _717 = _714 + -0.06666666269302368f;
         float _718 = _715 + -0.06666666269302368f;
         float _719 = _716 + -0.06666666269302368f;
         float _720 = _717 * 1.3790643215179443f;
         float _721 = _718 * 1.3790643215179443f;
         float _722 = _719 * 1.3790643215179443f;
         float _723 = YKR_MaterialCurve(_720, _687);
         float _724 = YKR_MaterialCurve(_721, _688);
         float _725 = YKR_MaterialCurve(_722, _689);
         float _726 = sqrt(_723);
         float _727 = sqrt(_724);
         float _728 = sqrt(_725);
         _730 = _726;
         _731 = _727;
         _732 = _728;
      }
      else
      {
         _730 = _662;
         _731 = _663;
         _732 = _664;
      }
   }
   int _733 = c_bool & 4;
   bool _734 = (_733 == 0);
   if (_734)
   {
      float _736 = saturate(COLOR_1.x);
      float _741 = (c_fog_param[0].x) - _730;
      float _742 = (c_fog_param[0].y) - _731;
      float _743 = (c_fog_param[0].z) - _732;
      float _744 = _741 * _736;
      float _745 = _742 * _736;
      float _746 = _743 * _736;
      _748 = _744;
      _749 = _745;
      _750 = _746;
   }
   else
   {
      _748 = COLOR_1.x;
      _749 = COLOR_1.y;
      _750 = COLOR_1.z;
   }
   float _751 = _748 + _730;
   float _752 = _749 + _731;
   float _753 = _750 + _732;
   if (!_476)
   {
      float _761 = (c_ps_user[3].x) * (c_ps_user[12].z);
      float _762 = (c_ps_user[3].y) * (c_ps_user[12].z);
      float _763 = (c_ps_user[3].z) * (c_ps_user[12].z);
      float _765 = (c_ps_user[12].w) * COLOR.x;
      float _766 = (c_ps_user[12].w) * COLOR.y;
      float _767 = (c_ps_user[12].w) * COLOR.z;
      float _768 = _765 + _751;
      float _769 = _768 + _761;
      float _770 = _766 + _752;
      float _771 = _770 + _762;
      float _772 = _767 + _753;
      float _773 = _772 + _763;
      _775 = _769;
      _776 = _771;
      _777 = _773;
   }
   else
   {
      _775 = _751;
      _776 = _752;
      _777 = _753;
   }
   float _780 = _661 - c_mat_punchthrough_ref;
   bool _781 = (_780 < 0.0f);
   if (_781)
      discard;
   float _787 = (c_ps_col_scl[0].x) * _775;
   float _788 = (c_ps_col_scl[0].y) * _776;
   float _789 = (c_ps_col_scl[0].z) * _777;
   float _790 = (c_ps_col_scl[0].w) * _661;
   SV_Target.x = _787;
   SV_Target.y = _788;
   SV_Target.z = _789;
   SV_Target.w = _790;
   int _793 = c_bool & 8;
   bool _794 = (_793 != 0);
   float _795 = select(_794, _775, _787);
   float _796 = select(_794, _776, _788);
   float _797 = select(_794, _777, _789);
   float _798 = select(_794, _661, _790);
   SV_Target.x = _795;
   SV_Target.y = _796;
   SV_Target.z = _797;
   SV_Target.w = _798;
   return SV_Target;
}
