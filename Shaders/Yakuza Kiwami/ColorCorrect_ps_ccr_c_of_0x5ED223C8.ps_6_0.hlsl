// Yakuza Kiwami ps_ccr______c_______of (0x5ED223C8, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) and edited by
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
   float _21 = (c_ps_user[0].w) * 2.0f;
   float _22 = 1.0f - _21;
   float _23 = 1.0f / _22;
   float _24 = _23 * _6.x;
   float _25 = _23 * _6.y;
   float _26 = _23 * _6.z;
   float _27 = _24 - (c_ps_user[0].w);
   float _28 = _25 - (c_ps_user[0].w);
   float _29 = _26 - (c_ps_user[0].w);
   float _30 = saturate(_27);
   float _31 = saturate(_28);
   float _32 = saturate(_29);
   float _37 = (c_ps_user[7].x) * _16;
   float _38 = (c_ps_user[7].y) * _16;
   float _39 = (c_ps_user[7].z) * _16;
   float _44 = (c_ps_user[11].x) * _17;
   float _45 = (c_ps_user[11].y) * _17;
   float _46 = (c_ps_user[11].z) * _17;
   float _51 = (c_ps_user[15].x) * _18;
   float _52 = (c_ps_user[15].y) * _18;
   float _53 = (c_ps_user[15].z) * _18;
   float _58 = _37 + _30;
   float _59 = _58 + _44;
   float _60 = _59 + _51;
   float _61 = _60 + (c_ps_user[3].x);
   float _62 = _38 + _31;
   float _63 = _62 + _45;
   float _64 = _63 + _52;
   float _65 = _64 + (c_ps_user[3].y);
   float _66 = _39 + _32;
   float _67 = _66 + _46;
   float _68 = _67 + _53;
   float _69 = _68 + (c_ps_user[3].z);
   float _70 = saturate(_61);
   float _71 = saturate(_65);
   float _72 = saturate(_69);
   SV_Target.x = _70;
   SV_Target.y = _71;
   SV_Target.z = _72;
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
