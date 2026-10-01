#ifndef BL2TPS_SCENE_DEPTH_HLSL
#define BL2TPS_SCENE_DEPTH_HLSL

// The scene's alpha holds Gearbox's EncodeFloatW of the view depth W (Common.usf, "GBX:Zoner"): -(W / 32)^2 up to 4096 units,
// (W / 8192)^2 past it, the sky clamped to 65503 (stored as fp16's 65472 or 65504). The inverse, in game units.
float DecodeFloatW(float encoded)
{
   return sqrt(abs(encoded)) * (encoded > 0.0 ? 8192.0 : 32.0);
}

#endif // BL2TPS_SCENE_DEPTH_HLSL
