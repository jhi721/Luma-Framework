// Katana engine PostEffect3 "cbComposite" (b2), shared by the composite, the bloom combine and the ApplyFxaa passes

cbuffer cbComposite : register(b2)
{
   float4 g_vSceneTexSize : packoffset(c0);
   float4 g_vCompositeInfo : packoffset(c1);
   float4 g_vSun2dInfo : packoffset(c2);
   float4 g_vEtcEffect : packoffset(c3);
   float4 g_vBloomInfo : packoffset(c4);
   float4 g_vLimbDarkenningInfo : packoffset(c5);
   float4 g_vFxaaParams : packoffset(c6);
   float4 g_vGammaCorrection : packoffset(c7);
   float4 g_vRadialBlurCenter : packoffset(c8);
   float4 g_vRadialBlurInfo : packoffset(c9);
   float4 g_vFxaaQualityParams : packoffset(c10);
   float4 g_vCompositeLastViewport : packoffset(c11);
   float4 g_vMaxUV : packoffset(c12);
}
