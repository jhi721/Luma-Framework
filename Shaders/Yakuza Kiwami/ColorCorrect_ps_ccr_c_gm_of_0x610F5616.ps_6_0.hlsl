// Yakuza Kiwami ps_ccr______c_gm____of (0x610F5616, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) and edited by
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
   float _37 = dot(float3((c_ps_user[5].x), (c_ps_user[5].y), (c_ps_user[5].z)), float3(_16, _17, _18));
   float _42 = dot(float3((c_ps_user[9].x), (c_ps_user[9].y), (c_ps_user[9].z)), float3(_16, _17, _18));
   float _47 = dot(float3((c_ps_user[13].x), (c_ps_user[13].y), (c_ps_user[13].z)), float3(_16, _17, _18));
   float _52 = (c_ps_user[1].x) + _37;
   float _53 = (c_ps_user[1].y) + _42;
   float _54 = (c_ps_user[1].z) + _47;
   float _55 = _53 * 0.45454543828964233f;
   float _56 = _54 * 0.45454543828964233f;
   float _57 = log2(_30);
   float _58 = log2(_31);
   float _59 = log2(_32);
   float _60 = _57 * 0.45454543828964233f;
   float _61 = _60 * _52;
   float _62 = _55 * _58;
   float _63 = _56 * _59;
   float _64 = exp2(_61);
   float _65 = exp2(_62);
   float _66 = exp2(_63);
   float _67 = saturate(_64);
   float _68 = saturate(_65);
   float _69 = saturate(_66);
   float _74 = (c_ps_user[7].x) * _16;
   float _75 = (c_ps_user[7].y) * _16;
   float _76 = (c_ps_user[7].z) * _16;
   float _81 = (c_ps_user[11].x) * _17;
   float _82 = (c_ps_user[11].y) * _17;
   float _83 = (c_ps_user[11].z) * _17;
   float _88 = (c_ps_user[15].x) * _18;
   float _89 = (c_ps_user[15].y) * _18;
   float _90 = (c_ps_user[15].z) * _18;
   float _95 = _74 + _67;
   float _96 = _95 + _81;
   float _97 = _96 + _88;
   float _98 = _97 + (c_ps_user[3].x);
   float _99 = _75 + _68;
   float _100 = _99 + _82;
   float _101 = _100 + _89;
   float _102 = _101 + (c_ps_user[3].y);
   float _103 = _76 + _69;
   float _104 = _103 + _83;
   float _105 = _104 + _90;
   float _106 = _105 + (c_ps_user[3].z);
   float _107 = saturate(_98);
   float _108 = saturate(_102);
   float _109 = saturate(_106);
   SV_Target.x = _107;
   SV_Target.y = _108;
   SV_Target.z = _109;
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
