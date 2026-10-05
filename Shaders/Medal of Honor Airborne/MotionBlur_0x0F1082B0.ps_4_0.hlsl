// UE3 camera motion blur as translated by dgVoodoo 2.81.3. Register-normalized disassembly is identical to 0x57D9358C apart from
// `div 1.0, x` in place of `rcp x` (and an unread cb5 declaration), so the whole pass comes from that file.
#include "MotionBlur_0x57D9358C.ps_5_0.hlsl"
