// ME1LE stage-1 tonemap: analytic scene grade; no LUT, white point, or vignette. The dumped CSO clamps to 1 after the
// native gamma curve and goes straight to the metering dot product and o0, without the 0.832050323/3.25/pow-100
// radial vignette constants of the ME1LE LUT permutations.
#define TM_VIGNETTE_TYPE 0
#include "Tonemap_ME_Analytic_Body.hlsl"
