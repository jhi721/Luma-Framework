// Mass Effect 3 (2012) - solid-colour fill (same hash as ME1 2007's UI_SolidFill): the original writes cb4[8] raw.
// Clamped as the 8-bit UNORM canvas did: colour and alpha from a constant can exceed 1 on the fp16 canvas.
#include "Includes/GameBindings.hlsl"

void main(DGV_MAIN_SIGNATURE_CENTROID)
{
   o0 = saturate(PsConstants[8]);
}
