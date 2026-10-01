// Borderlands: The Pre-Sequel — tonemap / HDR injection point (dgVoodoo 2.87.3 -> ps_5_0, hash 0xFCFE623E).
// Same UE3 uber-post grade as BL2, but the native shader (tps_tonemap_0xF8997849) inserts a LightShaftTexture at
// sampler 1, shifting bloom/vignette/LUT/DOF down one slot; dgVoodoo maps sN 1:1 onto tN, so the slot map below
// rebinds them and the injected Luma bloom moves to t8 (t5 is TPS's native DOF). Body shared via Luma_BL2TPS_Tonemap.hlsl.
#define TM_HAS_LIGHTSHAFT 1
#define TM_T_LIGHTSHAFT   t1 // LightShaftTexture (god rays) — TPS-only, inserted at slot 1
#define TM_T_BLOOM        t2 // FilterColor1Texture (screen-blend bloom)
#define TM_T_VIGNETTE     t3 // VignetteTexture
#define TM_T_LUT          t4 // ColorGradingLUT (256x16, 16-slice)
#define TM_T_DOF          t5 // LowResPostProcessBuffer (half-res DOF)
#define TM_T_LUMABLOOM    t8 // injected Luma HDR bloom (t5 is the native DOF on TPS — bind higher to avoid the clash)
#define TM_S_LIGHTSHAFT   s1 // the light-shaft texture's own sampler (native: sample t1, s1)
#define TM_S_BLOOM        s2
#define TM_S_VIGNETTE     s3
#define TM_S_LUT          s4
#define TM_S_DOF          s5
#include "Luma_BL2TPS_Tonemap.hlsl"
