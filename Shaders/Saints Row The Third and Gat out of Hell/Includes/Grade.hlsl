// Saints Row: The Third and Gat out of Hell - the grade every rl_hdr final shares (Gat out of Hell's finals run
// Saints Row: The Third's grade unchanged) and its Luma HDR output. The including tonemap declares
// Lut_sampler_2dTexture / Lut_sampler_2dSampler at its own slots first (SR3 t7/s7, GooH t8/s8).

cbuffer vc4 : register(b4)
{
   float2 Tint_saturation : packoffset(c0);
   float4 Tint_color : packoffset(c1);
}

// Saturation around the game's (0.30, 0.59, 0.11) weighted sum of the linear scene, then tint: the head of every rl_hdr final.
float3 SR_SaturateAndTint(float3 scene)
{
   const float weightedRGB = dot(float3(0.3, 0.59, 0.11), scene);
   return lerp(weightedRGB, scene, Tint_saturation.x) * Tint_color.rgb;
}

// The vanilla cubic shoulder on the 1.49 clip, u = c / 1.49.
float SR_Shoulder(float u)
{
   return 1.5 * u - 0.5 * u * u * u;
}
float3 SR_Shoulder(float3 u)
{
   return 1.5 * u - 0.5 * u * u * u;
}

// The vanilla LUT read, transcribed from the disassembly: slice = floor(b * 31) picks one 32x32 tile in an 8x4 grid,
// red and green address it with a half-texel centre, and the next slice (capped at 31) is lerped by b's fraction.
// `index` must already be in [0, 1]: past it the tile maths walks into the neighbouring slice and row.
float3 SR_SampleLUT(float3 index)
{
   const float slice = floor(index.b * 31.0);
   const float nextSlice = min(slice + 1.0, 31.0);
   const float2 tileUV = (index.rg * 31.0 + 0.5) * float2(1.0 / 256.0, 1.0 / 128.0);
   const float2 uv0 = float2(trunc(frac(slice * 0.125) * 8.0), trunc(slice * 0.125)) * float2(0.125, 0.25) + tileUV;
   const float2 uv1 = float2(trunc(frac(nextSlice * 0.125) * 8.0), trunc(nextSlice * 0.125)) * float2(0.125, 0.25) + tileUV;
   const float3 lut0 = Lut_sampler_2dTexture.Sample(Lut_sampler_2dSampler, uv0).rgb;
   const float3 lut1 = Lut_sampler_2dTexture.Sample(Lut_sampler_2dSampler, uv1).rgb;
   return lerp(lut0, lut1, index.b * 31.0 - slice);
}

// Linear HDR (1.0 = paper white) -> the gamma-encoded post-process space, via DICE to the user's peak.
float3 SR_DisplayMap(float3 hdrLinear, float2 uv)
{
   hdrLinear = IsNaN_Strict(hdrLinear) ? 0.0 : hdrLinear; // NaN -> 0; "x != x" can be compiled away
   hdrLinear = max(hdrLinear, 0.0);
   const float paperWhite = GamePaperWhiteNits / sRGB_WhiteLevelNits;
   const float peakWhite = PeakWhiteNits / sRGB_WhiteLevelNits;
   // User contrast before the display map, so DICE contains whatever it pushes up. Multiplicative around mid-gray (TW2's
   // form); the floored log2 keeps black at 0 for Contrast 0 instead of pow(0, 0) = NaN. Gated so 1 stays bit-exact.
   [branch] if (LumaSettings.GameSettings.Contrast != 1.0)
   {
      hdrLinear = exp2(LumaSettings.GameSettings.Contrast * log2(max(hdrLinear / MidGray, 1e-30))) * MidGray;
   }
   DICESettings settings = DefaultDICESettings(DICE_TYPE_BY_LUMINANCE_PQ_CORRECT_CHANNELS_BEYOND_PEAK_WHITE);
   // Ramps on the max channel between a third of peak and peak, inside DICE's containment: mid-tones stay untouched.
   settings.HighlightsDesaturation = LumaSettings.GameSettings.HighlightDechroma;
   float3 encoded = SR_EncodeOutput(DICETonemap(hdrLinear * paperWhite, peakWhite, settings) / paperWhite);
   // Anti-banding dither, one step of the output quantizer: the 8-bit code in SDR, 10-bit BT.2020 PQ in HDR. The
   // composition scales this image by UIPaperWhite, so that is the PQ scale.
   [branch] if (LumaSettings.GameSettings.Dithering > 0.5)
   {
      if (LumaSettings.DisplayMode == 0)
         ApplyDithering(encoded, uv, true, 1.0, 8u, LumaSettings.FrameIndex, true);
      else
      {
         const float pqScale = max(LumaSettings.UIPaperWhiteNits, 1.0) / HDR10_MaxWhiteNits;
         float3 pq = Linear_to_PQ(BT709_To_BT2020(gamma_to_linear(encoded, GCT_MIRROR) * pqScale), GCT_MIRROR);
         ApplyDithering(pq, uv, true, 1.0, 10u, LumaSettings.FrameIndex, true);
         encoded = linear_to_gamma(BT2020_To_BT709(PQ_to_Linear(pq, GCT_MIRROR)) / pqScale, GCT_MIRROR);
      }
   }
   return encoded;
}

// The pass result: the vanilla SDR reference (TONEMAP_TYPE 0), or the HDR recovery of it through DICE.
float4 SR_Output(float3 vanillaLinear, float recoveryGain, float2 uv, float alpha)
{
#if TONEMAP_TYPE <= 0
   return float4(SR_EncodeOutput(vanillaLinear), alpha);
#else
   return float4(SR_DisplayMap(Saturation(vanillaLinear, LumaSettings.GameSettings.Saturation) * recoveryGain, uv), alpha);
#endif
}

// HDR brightness recovery: how much brighter the clamped vanilla output gets, one scalar for all channels.
// Onset in u, the curve's own input. F'(p) = 1.5(1 - p^2), and the tangent's intercept F(p) - p F'(p) = p^3 is positive
// while F'' = -3u < 0, so every pivot in (0, 1) extends above the curve and the gain is >= 1. 0.35 transfers the
// Mass Effect 2 recovery by its metrics (the onset at 43% of the curve's white gives 0.30, SDR white recovering 1.35x
// gives 0.36): SDR white recovers 1.36x and a scene value of 10 reaches ~8.9x paper white before DICE. MELE / RenoDX
// Hejl-Dawson pivot at mid-gray instead (0.12 here, 80% of a daytime frame) because they feed the continuation through
// an unclamped LUT; this consumer is the Mass Effect 2 one.
float SR_RecoveryGain(float3 u)
{
   const float pivot = 0.35;
   // The hottest channel is the one the per-channel shoulder compresses first; F is monotone, so max3(F(u)) == F(max3(u)).
   const float sourcePeak = max3(u);
   if (sourcePeak <= pivot)
      return 1.0;
   const float extended = SR_Shoulder(pivot) + 1.5 * (1.0 - pivot * pivot) * (sourcePeak - pivot);
   // max(1, ...) holds "never darker than vanilla" through float rounding at the pivot, and turns NaN into vanilla.
   return max(1.0, extended / SR_Shoulder(min(sourcePeak, 1.0)));
}
