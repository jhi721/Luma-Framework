// Hashes registered from C++ ("redirected_shader_hashes"): originally 0x97C61073.
// Dead Space (2008): dgVoodoo's present blit with its colour adjustment and 256 entry gamma ramp table (cb2), taken while the game's
// ramp isn't identity (the same bytecode in 2.87.6 and 2.81.3). Both are dropped: the game's ramp reaches the display map as
// "GammaRampExponent" (the SetGammaRamp hook in main.cpp keeps it off dgVoodoo).
#include "Includes/DS_Present.hlsl"
