// The DOF/Bloom gather's (0xF0D8D818) blur amount for a device depth: needs its "gather_constants" (b0: focus distance, 1 / range,
// exponent; near / far max blur) and "offset_constants" (b2: [6].zw MinZ_MaxZRatio) declared by the includer.
float BlurAmount(float device_depth)
{
   const float view_depth = 1.0 / max(device_depth * offset_constants[6].z - offset_constants[6].w, 0.0);
   const float focus_offset = view_depth - gather_constants[0].x;
   const float max_blur = focus_offset < 0.0 ? gather_constants[1].x : gather_constants[1].y;
   return min(exp2(log2(max(saturate(abs(focus_offset) * gather_constants[0].y), 0.0001)) * gather_constants[0].z), max_blur);
}
