// D3D11 output composition. Reads the shared fp16 hand-off texture (linear-light scRGB values: BT.709 primaries, 1.0 = 80 nits,
// after decode_gamma decodes a transfer-encoded R'G'B' hand-off, when set) and writes it to the flip swapchain in one of three output modes
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
   float decode_gamma; // 0 = hand-off is linear; < 0: decode R'G'B' with the sRGB transfer function, else with this power

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
      ApplyOutputDithering(rgb, vp_uv, decode_gamma != 0, output_mode != 2, paper_white * 80.0);
   }
   if (decode_gamma < 0)
   {
      rgb = gamma_sRGB_to_linear(rgb, GCT_MIRROR);
   }
   else if (decode_gamma > 0)
   {
      rgb = gamma_to_linear(rgb, GCT_MIRROR, decode_gamma);
   }
   // rgb is linear-light scRGB from here on.
   if (test_flags & 4)
   {
      // Over in linear light, UI white at the hand-off's reference white (1.0), so paper_white scales it with the scene.
      uint2 ui_size;
      overlay.GetDimensions(ui_size.x, ui_size.y);
      const float4 ui = overlay.Load(int3(min(int2(uv * ui_size), int2(ui_size) - 1), 0));
      rgb = rgb * (1 - ui.a) + gamma_sRGB_to_linear(ui.rgb / max(ui.a, 1.0 / 255), GCT_SATURATE) * ui.a;
   }
   if (output_mode == 1)
   {
      float3 rgb2020 = (test_flags & 2) ? rgb : mul(BT709_TO_BT2020, rgb);
      rgb2020 = max(rgb2020 * paper_white, 0.0); // negative lobes have no PQ value
      return float4((test_flags & 1) ? saturate(rgb2020 * 80.0 / 10000.0) : Linear_to_PQ(rgb2020 * 80.0 / 10000.0, GCT_SATURATE), 1);
   }
   if (output_mode == 2)
   {
      // The game's own white (1.0 of the hand-off) is the SDR reference white; paper_white only scales HDR outputs.
      return float4((test_flags & 1) ? saturate(rgb) : linear_to_sRGB_gamma(rgb, GCT_SATURATE), 1);
   }
   return float4(rgb * paper_white, 1);
}
