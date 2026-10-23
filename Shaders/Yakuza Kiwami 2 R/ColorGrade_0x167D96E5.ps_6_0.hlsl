// Yakuza Kiwami 2 R color grade (D3D12, DXIL SM 6.0, hash 0x167D96E5): the HDR replacement of the game's only grade, the pass
// right after the scene shader 0xD0869ED8 that writes t0.
// The vanilla body is transcribed VERBATIM from the decompiled original: temperature and white balance
// matrices, encode (sRGB or pow(c_inv_gamma)), saturate, three blended 16^3 LUTs, contrast curve + YCbCr luma, noise, lerp with
// the linear input by the mask t1.w. t0 is log10(1 + scene), the game's whole tone curve, unbounded and never clamped; the grade
// treats it as display-linear and clips it at 1 (scene 9) with its saturates. Register layout, cbuffer b9, alpha: as the original.
//
// HDR display (LumaSettings.DisplayMode == 1) restores what the saturates clipped without re-authoring the grade: the working
// color (after the matrices) is compressed into [0,1] by ONE max-channel scalar (identity below YK2R_SHOULDER, ReinhardPiecewise),
// the vanilla chain runs verbatim on it, the result is decoded to linear, divided by the same scalar, and contained to the display
// peak with PsychoV31. A scalar can't rotate hue, so no hue restore is added on top (the PsychoV hue reference is the shared
// map's own). Method: Sucker Punch / glowybits 2016 "ColorGradeHdr" (SmoothClamp(max) / max), see "tonemap-lut-bridge" option 3.
// SDR display modes run the vanilla chain unchanged (scale 1, nothing after it). The noise stays vanilla-sized: added in gamma
// space after the recovery, never multiplied by 1/scale. The output stays gamma space, 1 = paper white (Core's composition
// decodes it and applies paper white), values above 1 are allowed.

// clang-format off
// ORDER IS LOAD-BEARING - do not sort. "Common.hlsl" first, it pulls the Luma settings the PsychoV include reads.
#include "../Includes/Common.hlsl"
#include "../Includes/PsychoV31HueReference.hlsl" // PsychoVDisplayMapWithHueReference; its Reinhard.hlsl: Reinhard::ReinhardPiecewise
// clang-format on

// Max channel (after the grade's matrices) below which the compression is the identity (what remains different from vanilla there is the
// PsychoV31 map: <= 4e-4 gamma on moderately saturated pixels in the offline test). The compressed range above it
// (0.5..1) is what the 16^3 LUTs get to resolve, so a higher shoulder spends fewer cells on highlights (see the memory table in
// hdr-maxch-lut-wrap-method: 0.5 -> ~5.3 cells above SDR white, 0.8 -> ~2.7).
#ifndef YK2R_SHOULDER
#define YK2R_SHOULDER 0.5
#endif

Texture2D<float4> s0_t : register(t0);

Texture2D<float4> s1_t : register(t1);

Texture3D<float4> s2v_t : register(t2);

Texture3D<float4> s3v_t : register(t3);

Texture3D<float4> s4v_t : register(t4);

cbuffer cb_user : register(b9)
{
   float4 c_temperature_mtx[4] : packoffset(c000.x);
   float4 c_white_balance_mtx[4] : packoffset(c004.x);
   float c_contrast0 : packoffset(c008.x);
   float c_contrast1 : packoffset(c008.y);
   float c_contrast_mode : packoffset(c008.z);
   uint c_flag : packoffset(c008.w);
   float c_ratio1 : packoffset(c009.x);
   float c_ratio2 : packoffset(c009.y);
   float c_inv_gamma : packoffset(c009.z);
   float c_noise_gain : packoffset(c009.w);
};

SamplerState s0_s : register(s0);

SamplerState s1_s : register(s1);

SamplerState s2v_s : register(s2);

SamplerState s3v_s : register(s3);

SamplerState s4v_s : register(s4);

float4 main(
    linear float2 TEXCOORD : TEXCOORD) : SV_Target
{
   float4 SV_Target = 0;
   float4 _14 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _19 = max(_14.x, 0.0f);
   float _20 = max(_14.y, 0.0f);
   float _21 = max(_14.z, 0.0f);
   float _34 = dot(float3((c_temperature_mtx[0].x), (c_temperature_mtx[0].y), (c_temperature_mtx[0].z)), float3(_19, _20, _21));
   float _35 = dot(float3((c_temperature_mtx[1].x), (c_temperature_mtx[1].y), (c_temperature_mtx[1].z)), float3(_19, _20, _21));
   float _36 = dot(float3((c_temperature_mtx[2].x), (c_temperature_mtx[2].y), (c_temperature_mtx[2].z)), float3(_19, _20, _21));
   float _49 = dot(float3((c_white_balance_mtx[0].x), (c_white_balance_mtx[0].y), (c_white_balance_mtx[0].z)), float3(_34, _35, _36));
   float _50 = dot(float3((c_white_balance_mtx[1].x), (c_white_balance_mtx[1].y), (c_white_balance_mtx[1].z)), float3(_34, _35, _36));
   float _51 = dot(float3((c_white_balance_mtx[2].x), (c_white_balance_mtx[2].y), (c_white_balance_mtx[2].z)), float3(_34, _35, _36));
   const bool hdr = LumaSettings.DisplayMode == 1;
   float hdr_scale = 1.0f;
   if (hdr)
   {
      const float working_max_channel = max3(float3(_49, _50, _51));
      if (working_max_channel > YK2R_SHOULDER)
      {
         hdr_scale = Reinhard::ReinhardPiecewise(working_max_channel, 1.0f, YK2R_SHOULDER) / working_max_channel;
      }
      _49 *= hdr_scale;
      _50 *= hdr_scale;
      _51 *= hdr_scale;
   }
   bool _54 = !(c_inv_gamma <= 0.0f);
   float _55 = log2(_49);
   float _56 = log2(_50);
   float _57 = log2(_51);
   float _85;
   float _86;
   float _87;
   float _180;
   float _181;
   float _182;
   float _253;
   float _254;
   float _255;
   if (!_54)
   {
      float _59 = _49 * 12.920000076293945f;
      float _60 = _50 * 12.920000076293945f;
      float _61 = _51 * 12.920000076293945f;
      float _62 = _55 * 0.4166666567325592f;
      float _63 = _56 * 0.4166666567325592f;
      float _64 = _57 * 0.4166666567325592f;
      float _65 = exp2(_62);
      float _66 = exp2(_63);
      float _67 = exp2(_64);
      float _68 = _65 * 1.0549999475479126f;
      float _69 = _66 * 1.0549999475479126f;
      float _70 = _67 * 1.0549999475479126f;
      float _71 = _68 + -0.054999999701976776f;
      float _72 = _69 + -0.054999999701976776f;
      float _73 = _70 + -0.054999999701976776f;
      float _74 = min(_59, _71);
      float _75 = min(_60, _72);
      float _76 = min(_61, _73);
      _85 = _74;
      _86 = _75;
      _87 = _76;
   }
   else
   {
      float _78 = _55 * c_inv_gamma;
      float _79 = _56 * c_inv_gamma;
      float _80 = _57 * c_inv_gamma;
      float _81 = exp2(_78);
      float _82 = exp2(_79);
      float _83 = exp2(_80);
      _85 = _81;
      _86 = _82;
      _87 = _83;
   }
   int _90 = c_flag & 2;
   bool _91 = (_90 == 0);
   if (!_91)
   {
      float _93 = saturate(_85);
      float _94 = saturate(_86);
      float _95 = saturate(_87);
      uint4 _96 = 0u;
      s2v_t.GetDimensions(0u, _96.x, _96.y, _96.z, _96.w);
      float _98 = float((int)((int)(_96.x)));
      float _100 = float((int)((int)(_96.y)));
      float _102 = float((int)((int)(_96.z)));
      uint4 _103 = 0u;
      s3v_t.GetDimensions(0u, _103.x, _103.y, _103.z, _103.w);
      float _105 = float((int)((int)(_103.x)));
      float _107 = float((int)((int)(_103.y)));
      float _109 = float((int)((int)(_103.z)));
      uint4 _110 = 0u;
      s4v_t.GetDimensions(0u, _110.x, _110.y, _110.z, _110.w);
      float _112 = float((int)((int)(_110.x)));
      float _114 = float((int)((int)(_110.y)));
      float _116 = float((int)((int)(_110.z)));
      float _117 = 1.0f / _98;
      float _118 = 1.0f / _100;
      float _119 = 1.0f / _102;
      float _120 = 0.5f - _93;
      float _121 = 0.5f - _94;
      float _122 = 0.5f - _95;
      float _123 = _117 * _120;
      float _124 = _118 * _121;
      float _125 = _119 * _122;
      float _126 = _123 + _93;
      float _127 = _124 + _94;
      float _128 = _125 + _95;
      float4 _129 = s2v_t.Sample(s2v_s, float3(_126, _127, _128));
      float _133 = 1.0f / _105;
      float _134 = 1.0f / _107;
      float _135 = 1.0f / _109;
      float _136 = _133 * _120;
      float _137 = _134 * _121;
      float _138 = _135 * _122;
      float _139 = _136 + _93;
      float _140 = _137 + _94;
      float _141 = _138 + _95;
      float4 _142 = s3v_t.Sample(s3v_s, float3(_139, _140, _141));
      float _146 = 1.0f / _112;
      float _147 = 1.0f / _114;
      float _148 = 1.0f / _116;
      float _149 = _146 * _120;
      float _150 = _147 * _121;
      float _151 = _148 * _122;
      float _152 = _149 + _93;
      float _153 = _150 + _94;
      float _154 = _151 + _95;
      float4 _155 = s4v_t.Sample(s4v_s, float3(_152, _153, _154));
      float _161 = _142.x - _129.x;
      float _162 = _142.y - _129.y;
      float _163 = _142.z - _129.z;
      float _164 = _161 * c_ratio1;
      float _165 = _162 * c_ratio1;
      float _166 = _163 * c_ratio1;
      float _167 = _164 + _129.x;
      float _168 = _165 + _129.y;
      float _169 = _166 + _129.z;
      float _170 = _155.x - _167;
      float _171 = _155.y - _168;
      float _172 = _155.z - _169;
      float _173 = _170 * c_ratio2;
      float _174 = _171 * c_ratio2;
      float _175 = _172 * c_ratio2;
      float _176 = _173 + _167;
      float _177 = _174 + _168;
      float _178 = _175 + _169;
      _180 = _176;
      _181 = _177;
      _182 = _178;
   }
   else
   {
      _180 = _85;
      _181 = _86;
      _182 = _87;
   }
   int _183 = c_flag & 1;
   bool _184 = (_183 == 0);
   if (!_184)
   {
      float _189 = c_contrast_mode * c_contrast0;
      float _191 = 1.0f - c_contrast_mode;
      float _192 = c_contrast0 * _191;
      float _193 = -0.0f - _192;
      float _194 = c_contrast1 * _191;
      float _195 = -0.0f - _194;
      float _196 = saturate(_180);
      float _197 = saturate(_181);
      float _198 = saturate(_182);
      float _199 = _196 * _196;
      float _200 = _196 + -2.0f;
      float _201 = _199 * _200;
      float _202 = _201 + _196;
      float _203 = _202 * _193;
      float _204 = _199 - _196;
      float _205 = _204 * _196;
      float _206 = _205 * _195;
      float _207 = _203 + _206;
      float _208 = _207 * 3.0f;
      float _209 = _197 * _197;
      float _210 = _197 + -2.0f;
      float _211 = _209 * _210;
      float _212 = _211 + _197;
      float _213 = _212 * _193;
      float _214 = _209 - _197;
      float _215 = _197 * _195;
      float _216 = _215 * _214;
      float _217 = _213 + _216;
      float _218 = _217 * 3.0f;
      float _219 = _198 * _198;
      float _220 = _198 + -2.0f;
      float _221 = _219 * _220;
      float _222 = _221 + _198;
      float _223 = _222 * _193;
      float _224 = _219 - _198;
      float _225 = _198 * _195;
      float _226 = _225 * _224;
      float _227 = _223 + _226;
      float _228 = _227 * 3.0f;
      float _229 = dot(float3(_180, _181, _182), float3(0.29899999499320984f, 0.5870000123977661f, 0.11400000005960464f));
      float _230 = dot(float3(_180, _181, _182), float3(-0.16874000430107117f, -0.33125999569892883f, 0.5f));
      float _231 = dot(float3(_180, _181, _182), float3(0.5f, -0.4186899960041046f, -0.08130999654531479f));
      float _232 = saturate(_229);
      float _233 = _232 * _232;
      float _234 = _232 + -2.0f;
      float _235 = _233 * _234;
      float _236 = _235 + _232;
      float _237 = _189 * _236;
      float _238 = _233 - _232;
      float _239 = c_contrast_mode * c_contrast1;
      float _240 = _239 * _232;
      float _241 = _240 * _238;
      float _242 = -0.0f - _241;
      float _243 = _242 - _237;
      float _244 = _243 * 3.0f;
      float _245 = _244 + _229;
      float _246 = dot(float3(_245, _230, _231), float3(1.0f, 0.0f, 1.4019999504089355f));
      float _247 = dot(float3(_245, _230, _231), float3(1.0f, -0.3441399931907654f, -0.714139997959137f));
      float _248 = dot(float3(_245, _230, _231), float3(1.0f, 1.7719999551773071f, 0.0f));
      float _249 = _246 + _208;
      float _250 = _247 + _218;
      float _251 = _248 + _228;
      _253 = _249;
      _254 = _250;
      _255 = _251;
   }
   else
   {
      _253 = _180;
      _254 = _181;
      _255 = _182;
   }
   float3 graded = float3(_253, _254, _255); // gamma space, graded from the compressed input when hdr
   if (hdr)
   {
      // Exact inverse pair (the helpers' mirrored variants), so a pixel below the shoulder (hdr_scale 1) round trips untouched.
      const bool srgb_encode = c_inv_gamma <= 0.0f;
      float3 recovered_linear = srgb_encode ? gamma_sRGB_to_linear(graded, GCT_MIRROR) : gamma_to_linear(graded, GCT_MIRROR, 1.0f / c_inv_gamma);
      recovered_linear = max(recovered_linear / hdr_scale, 0.0f);
      recovered_linear = PsychoVDisplayMapWithHueReference(recovered_linear, 1.0f, 1.0f, 0.0f); // neutral contrast, saturation, highlights desaturation
      graded = srgb_encode ? linear_to_sRGB_gamma(recovered_linear, GCT_MIRROR) : linear_to_gamma(recovered_linear, GCT_MIRROR, 1.0f / c_inv_gamma);
   }
   float _256 = dot(float2(TEXCOORD.x, TEXCOORD.y), float2(12.989800453186035f, 78.23300170898438f));
   float _257 = _256 + 1763.0f;
   float _258 = sin(_257);
   float _259 = _258 * 43758.546875f;
   float _260 = frac(_259);
   float _262 = _260 + -0.5f;
   float _263 = _262 * c_noise_gain;
   float _264 = _263 + graded.x;
   float _265 = _263 + graded.y;
   float _266 = _263 + graded.z;
   float _267 = max(0.0f, _264);
   float _268 = max(0.0f, _265);
   float _269 = max(0.0f, _266);
   float4 _270 = s1_t.Sample(s1_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _272 = _267 - _14.x;
   float _273 = _268 - _14.y;
   float _274 = _269 - _14.z;
   float _275 = _270.w * _272;
   float _276 = _270.w * _273;
   float _277 = _270.w * _274;
   float _278 = _275 + _14.x;
   float _279 = _276 + _14.y;
   float _280 = _277 + _14.z;
   SV_Target.x = _278;
   SV_Target.y = _279;
   SV_Target.z = _280;
   SV_Target.w = _14.w;
   return SV_Target;
}
