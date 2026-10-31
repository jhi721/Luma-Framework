// Yakuza Kiwami ps_render_fb_d93 (0x1C540C83, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) and edited by
// a script: its input goes through YKR_RenderFBInput and its output through YKR_RenderFBOutput (HDR peak, dither).

#include "Includes/PostOutput.hlsl"

Texture2D<float4> s0_t : register(t0);

cbuffer cb13 : register(b13)
{
   uint c_bool : packoffset(c000.x);
};

cbuffer cb_user : register(b9)
{
   float4 c_ps_user[16] : packoffset(c000.x);
};

SamplerState s0_s : register(s0);

float4 main(
    linear float2 TEXCOORD : TEXCOORD) : SV_Target
{
   float4 SV_Target = 0;
   float4 _7 = YKR_RenderFBInput(s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y)));
   float _14 = (c_ps_user[0].x) * _7.x;
   float _15 = (c_ps_user[0].x) * _7.y;
   float _16 = (c_ps_user[0].x) * _7.z;
   float _18 = _14 + (c_ps_user[0].y);
   float _19 = _15 + (c_ps_user[0].y);
   float _20 = _16 + (c_ps_user[0].y);
   float _22 = log2(_18);
   float _23 = log2(_19);
   float _24 = log2(_20);
   float _25 = _22 * (c_ps_user[0].z);
   float _26 = _23 * (c_ps_user[0].z);
   float _27 = _24 * (c_ps_user[0].z);
   float _28 = exp2(_25);
   float _29 = exp2(_26);
   float _30 = exp2(_27);
   float _31 = log2(_28);
   float _32 = log2(_29);
   float _33 = log2(_30);
   float _34 = _31 * 2.200000047683716f;
   float _35 = _32 * 2.200000047683716f;
   float _36 = _33 * 2.200000047683716f;
   float _37 = exp2(_34);
   float _38 = exp2(_35);
   float _39 = exp2(_36);
   int _42 = c_bool & 8;
   bool _43 = (_42 == 0);
   float _65;
   float _66;
   float _67;
   if (!_43)
   {
      float _49 = dot(float3(_37, _38, _39), float3((c_ps_user[1].x), (c_ps_user[1].y), (c_ps_user[1].z)));
      float _54 = dot(float3(_49, _38, _39), float3((c_ps_user[2].x), (c_ps_user[2].y), (c_ps_user[2].z)));
      float _59 = dot(float3(_49, _54, _39), float3((c_ps_user[3].x), (c_ps_user[3].y), (c_ps_user[3].z)));
      _65 = _49;
      _66 = _54;
      _67 = _59;
   }
   else
   {
      float _61 = _39 * 1.0204081535339355f;
      float _62 = _37 * 0.6700000166893005f;
      float _63 = _38 * 0.760869562625885f;
      _65 = _62;
      _66 = _63;
      _67 = _61;
   }
   float _68 = log2(_65);
   float _69 = log2(_66);
   float _70 = log2(_67);
   float _71 = _68 * 0.45454543828964233f;
   float _72 = _69 * 0.45454543828964233f;
   float _73 = _70 * 0.45454543828964233f;
   float _74 = exp2(_71);
   float _75 = exp2(_72);
   float _76 = exp2(_73);
   SV_Target.x = _74;
   SV_Target.y = _75;
   SV_Target.z = _76;
   SV_Target.w = _7.w;
   return YKR_RenderFBOutput(SV_Target, float2(TEXCOORD.x, TEXCOORD.y));
}
