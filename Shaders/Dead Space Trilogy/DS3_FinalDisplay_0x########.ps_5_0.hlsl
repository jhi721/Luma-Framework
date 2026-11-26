// Hashes registered from C++ ("redirected_shader_hashes"): originally 0x9AF6BE17.
// clang-format off
#include "Includes/Common.hlsl" // game-local: LumaGameSettings before the shared Settings.hlsl, keep FIRST
#include "Includes/DS_DisplayMap.hlsl"
// clang-format on
#include "../Includes/DgVoodoo.hlsl"

// Dead Space 3: the final display pass (blob 34f102670046; dgVoodoo 2.87.6 -> ps_5_0 0x9AF6BE17), the last draw into the swapchain:
// adaptive saturation, smoothstep S-curve and the brightness slider (Window.Gamma: c0.y gain, c0.z offset, identity at 0.5), all
// saturated. Its math only holds on [0, 1], so above 1 it runs on the colour divided by its max channel and is scaled back (bit exact
// vanilla while max3 <= 1), then the display map.

cbuffer cb3 : register(b3)
{
   float4 cb3[77]; // dgVoodoo state: the texture masks, one (and, or) pair per sampler, s0 at 44/45
}

cbuffer cb4 : register(b4)
{
   float4 cb4[236]; // D3D9 constants at c<N> = cb4[N + 8]: c0 g_brightness_blend_params (y gain, z offset), c1 g_saturation_scurve_params
}

Texture2D<float4> t0 : register(t0); // ColorTexture
// Luma's UI layer (LumaData.CustomData3 != 0, HDR only): the UI drawn after the post chain, premultiplied over transparent black with
// the coverage in alpha (see "ui_layer" in main.cpp). Put over the mapped and pre-scaled scene as its blends did, so it lands at the UI
// paper white. It skips the vanilla saturation, S-curve and brightness, as the UI paper white replaces the latter.
Texture2D<float4> t1 : register(t1);
SamplerState s0_s : register(s0);

// The vanilla saturation and S-curve, before the brightness (its "mad_sat" lerp keeps them in [0, 1])
float4 SaturationSCurve(float4 c)
{
   const float grey = dot(c.rgb, 0.333);
   const float3 residual = c.rgb - grey;
   const float amount = saturate(dot(residual, residual) * 8.0) * cb4[9].x + cb4[9].y;
   // Alpha follows blue, as the translation's swizzle
   const float4 s = saturate(amount * (c.rgbb - grey) + grey);
   return cb4[9].z * (s * s * (3.0 - 2.0 * s) - s) + s;
}

void main(DGV_MAIN_SIGNATURE)
{
   const float4 c = ApplyDgvMask(t0.Sample(s0_s, v5.xy), cb3[44], cb3[45]);
   [branch] if (LumaSettings.DisplayMode != 1)
   {
      o0 = saturate(SaturationSCurve(c) * cb4[8].y + cb4[8].z);
   }
   else
   {
      const float m = max(1.0, max3(c.rgb));
      o0 = SaturationSCurve(float4(c.rgb / m, c.a));
      o0.rgb *= m;
      o0.rgb = o0.rgb * cb4[8].y + cb4[8].z;
      o0.a = saturate(o0.a * cb4[8].y + cb4[8].z);
   }
   uint2 size;
   t0.GetDimensions(size.x, size.y);
   o0.rgb = DisplayMapCanvas(o0.rgb, v0.xy / size);
   [branch] if (LumaData.CustomData3 != 0)
   {
      const float4 ui = t1.Load(int3(v0.xy, 0));
      // The UNORM canvas clipped each UI blend at 1
      o0.rgb = o0.rgb * (1.0 - saturate(ui.a)) + saturate(ui.rgb);
   }
}
