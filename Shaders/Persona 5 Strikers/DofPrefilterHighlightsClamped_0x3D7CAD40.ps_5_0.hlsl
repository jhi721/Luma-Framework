// DOF prefilter with a bokeh highlight mask (0x3D7CAD40): 0xD65ABD25 with the luminance floored before its log, the log level
// saturated and a fixed CoC threshold constant. See 0xE0DB2D7E.

#define P5S_DOF_PREFILTER_HIGHLIGHTS 2
#include "DofPrefilter_0xE0DB2D7E.ps_5_0.hlsl"
