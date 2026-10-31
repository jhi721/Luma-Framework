// Yakuza Kiwami ps_ccr______________of (0xB6C95B59, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) and edited by
// a script: it becomes YKR_ColorCorrectGrade, each scene sample through YKR_ColorCorrectScene, run per grade (vanilla, HDR) by the main at the end.

#include "Includes/ColorCorrect.hlsl"

Texture2D<float4> s0_t : register(t0);

cbuffer cb_user : register(b9)
{
   float4 c_ps_user[16] : packoffset(c000.x);
};

SamplerState s0_s : register(s0);

float4 YKR_ColorCorrectGrade(float2 TEXCOORD)
{
   float4 SV_Target = 0;
   float4 _6 = YKR_ColorCorrectScene(s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y)));
   float _10 = dot(float3(_6.x, _6.y, _6.z), float3(0.3333333432674408f, 0.3333333432674408f, 0.3333333432674408f));
   float _11 = 1.0f - _10;
   float _12 = _11 * _11;
   float _13 = _10 * _10;
   float _14 = 1.0f - _13;
   float _15 = _14 - _12;
   float _16 = saturate(_12);
   float _17 = saturate(_15);
   float _18 = saturate(_13);
   float _23 = (c_ps_user[7].x) * _16;
   float _24 = (c_ps_user[7].y) * _16;
   float _25 = (c_ps_user[7].z) * _16;
   float _30 = (c_ps_user[11].x) * _17;
   float _31 = (c_ps_user[11].y) * _17;
   float _32 = (c_ps_user[11].z) * _17;
   float _37 = (c_ps_user[15].x) * _18;
   float _38 = (c_ps_user[15].y) * _18;
   float _39 = (c_ps_user[15].z) * _18;
   float _44 = _23 + _6.x;
   float _45 = _44 + _30;
   float _46 = _45 + _37;
   float _47 = _46 + (c_ps_user[3].x);
   float _48 = _24 + _6.y;
   float _49 = _48 + _31;
   float _50 = _49 + _38;
   float _51 = _50 + (c_ps_user[3].y);
   float _52 = _25 + _6.z;
   float _53 = _52 + _32;
   float _54 = _53 + _39;
   float _55 = _54 + (c_ps_user[3].z);
   float _56 = saturate(_47);
   float _57 = saturate(_51);
   float _58 = saturate(_55);
   SV_Target.x = _56;
   SV_Target.y = _57;
   SV_Target.z = _58;
   SV_Target.w = 1.0f;
   return SV_Target;
}

float4 main(
    linear float2 TEXCOORD : TEXCOORD) : SV_Target
{
   const float4 vanilla_graded = YKR_ColorCorrectGrade(TEXCOORD);
   float4 hdr_graded = vanilla_graded;
   [branch] if (YKR_ColorCorrectHDR())
   {
      ykr_color_correct_compressed = true;
      hdr_graded = YKR_ColorCorrectGrade(TEXCOORD);
   }
   return YKR_ColorCorrectOutput(vanilla_graded, hdr_graded);
}
