// D3D11 output composition. Reads the shared fp16 hand-off texture (linear-light scRGB values: BT.709 primaries, 1.0 = 80 nits,
// after the optional decode_gamma step) and writes it to the flip swapchain in one of three output modes
// (D9N_OUTPUT, PROXY_PARITY P4):
//   0 scrgb : R16G16B16A16_FLOAT, G10 P709. Linear hand-off scaled by paper_white.
//   1 hdr10 : R10G10B10A2_UNORM, G2084 P2020. BT.709 -> BT.2020 on linear light (ITU-R BT.2087), then SMPTE ST 2084 inverse EOTF
//             of absolute nits (scRGB 1.0 = 80 nits), PQ code values.
//   2 srgb  : R8G8B8A8_UNORM, G22 P709. Hand-off 1.0 = reference white, clipped to [0, 1], then the
//             piecewise sRGB encode (IEC 61966-2-1).
// Fullscreen triangle from SV_VertexID, no vertex buffer. D3D11 output draws with the letterbox viewport; the D3D12 presenter
// helper (d9present.cpp) draws the whole back buffer (black outside vp) with ReShade's D3D9 overlay on top (test_flags 4).
// ApplyOutputDithering; LumaSettings (b13) carries only the frame index (d9present.cpp). Built with Bayonetta's defines.
#include "../Includes/Common.hlsl"

Texture2D<float4> hand_off : register(t0);
// Premultiplied ReShade overlay of the D3D9 back buffer (8-bit sR'G'B' code values blended by ImGui, coverage alpha), stretched
// over the whole output.
Texture2D<float4> overlay : register(t1);
// The canvas right after ResetRender (or a movie draw), the floor of Present_0x880A17D3's UI clamp (test_flags 8 / 16).
Texture2D<float4> snapshot : register(t2);

cbuffer Composition : register(b0)
{
   float paper_white;  // scRGB units: 1.0 = 80 nits
   float2 vp_origin;   // letterboxed destination rectangle inside the D3D11 back buffer
   float decode_gamma; // 0 = hand-off is linear; else decode R'G'B' with this power

   float2 vp_size;
   uint output_mode; // 0 scrgb, 1 hdr10, 2 srgb
   uint test_flags;  // 1 = skip the transfer encode, 2 = skip the BT.709 -> BT.2020 matrix (mutation switches),
                     // 4 = overlay at t1, 8 = present clamp (canvas), 16 = snapshot at t2 valid this frame,
                     // 32 = output dither
};

float4 VSMain(uint id : SV_VertexID, out float2 uv : TEXCOORD0) : SV_Position
{
   uv = float2((id << 1) & 2, id & 2);
   return float4(uv * float2(2, -2) + float2(-1, 1), 0, 1);
}

// Linear BT.709 -> linear BT.2020 (ITU-R BT.2087 section 2; rows sum to 1, so white stays white).
static const float3x3 BT709_TO_BT2020 = float3x3(0.6274, 0.3293, 0.0433,
                                                 0.0691, 0.9195, 0.0114,
                                                 0.0164, 0.0880, 0.8956);

// SMPTE ST 2084 inverse EOTF: absolute luminance (nits, 0..10000) -> PQ non-linear signal.
float3 PQInverseEOTF(float3 nits)
{
   const float m1 = 2610.0 / 16384.0, m2 = 2523.0 / 4096.0 * 128.0;
   const float c1 = 3424.0 / 4096.0, c2 = 2413.0 / 4096.0 * 32.0, c3 = 2392.0 / 4096.0 * 32.0;
   const float3 y_m1 = pow(saturate(nits / 10000.0), m1);
   return pow((c1 + c2 * y_m1) / (1.0 + c3 * y_m1), m2);
}

// IEC 61966-2-1 piecewise encode, input clipped to [0, 1].
float3 SRGBEncode(float3 linear_rgb)
{
   const float3 v = saturate(linear_rgb);
   return v <= 0.0031308 ? v * 12.92 : 1.055 * pow(v, 1.0 / 2.4) - 0.055;
}

// IEC 61966-2-1 piecewise decode.
float3 SRGBDecode(float3 encoded)
{
   return encoded <= 0.04045 ? encoded / 12.92 : pow((encoded + 0.055) / 1.055, 2.4);
}

float4 PSMain(float4 position : SV_Position, float2 uv : TEXCOORD0)
    : SV_Target
{
   uint2 size;
   hand_off.GetDimensions(size.x, size.y);
   const float2 vp_uv = (position.xy - vp_origin) / vp_size;
   const int2 texel = clamp(int2(vp_uv * size), 0, int2(size) - 1);
   float3 rgb = all(vp_uv >= 0 && vp_uv < 1) ? hand_off.Load(int3(texel, 0)).rgb : 0;
   // Bayonetta's present clamp, on the encoded canvas: the UI many layers of which blend additively stays within its paper white
   // as the 8-bit canvas kept it, the scene and the movies under it (the snapshot) keep their HDR values; no snapshot: 1 everywhere.
   if (test_flags & 8)
   {
      rgb = min(rgb, max((test_flags & 16) ? snapshot.Load(int3(texel, 0)).rgb : 0, 1));
   }
   // Bayonetta's output dither (Present_0x880A17D3), also on the encoded canvas: one step of the output quantizer, the 8 bit code
   // in SDR, the 10 bit BT.2020 PQ code in HDR at the paper white the composition scales the canvas by
   if (test_flags & 32)
   {
      ApplyOutputDithering(rgb, vp_uv, decode_gamma > 0, output_mode != 2, paper_white * 80.0);
   }
   if (decode_gamma > 0)
   {
      rgb = sign(rgb) * pow(abs(rgb), decode_gamma);
   }
   // rgb is linear-light scRGB from here on.
   if (test_flags & 4)
   {
      // Over in linear light, UI white at the hand-off's reference white (1.0), so paper_white scales it with the scene.
      uint2 ui_size;
      overlay.GetDimensions(ui_size.x, ui_size.y);
      const float4 ui = overlay.Load(int3(min(int2(uv * ui_size), int2(ui_size) - 1), 0));
      rgb = rgb * (1 - ui.a) + SRGBDecode(saturate(ui.rgb / max(ui.a, 1.0 / 255))) * ui.a;
   }
   if (output_mode == 1)
   {
      float3 rgb2020 = (test_flags & 2) ? rgb : mul(BT709_TO_BT2020, rgb);
      rgb2020 = max(rgb2020 * paper_white, 0.0); // negative lobes have no PQ value
      return float4((test_flags & 1) ? saturate(rgb2020 * 80.0 / 10000.0) : PQInverseEOTF(rgb2020 * 80.0), 1);
   }
   if (output_mode == 2)
   {
      // The game's own white (1.0 of the hand-off) is the SDR reference white; paper_white only scales HDR outputs.
      return float4((test_flags & 1) ? saturate(rgb) : SRGBEncode(rgb), 1);
   }
   return float4(rgb * paper_white, 1);
}
