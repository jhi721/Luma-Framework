#define GAME_SAINTS_ROW_IV 1

#define DISABLE_AUTO_DEBUGGER 1
// Alpha to coverage wraps the game's own alpha-tested draws, so it needs "original_draw_dispatch_func".
#define ENABLE_POST_DRAW_DISPATCH_CALLBACK 1
// Alpha to coverage also patches the alpha those draws output, into a clone that OnDrawOrDispatch swaps in per draw.
#define LUMA_PATCH_BYTECODE_SYNC 1
#define LUMA_PATCH_SYNC_MODE_CLONE 1
// SMAA runs after the rl_hdr final composite, through the post-draw callback above.
#define ENABLE_SMAA 1
#define ENABLE_BLOOM 1

#include "..\..\Core\core.hpp"
#include "..\..\External\WDK\includes\d3d11TokenizedProgramFormat.hpp"

// Saints Row IV (Volition CTG engine, the 2022 Re-Elected build sr_hv.exe), native D3D11, 64-bit.
// Same engine and post chain as Saints Row: The Third; the final composite also carries distortion and DoF.
//
// Frame (DevKit): FP16 scene (MSAA from display.ini, resolved before the tonemap) -> rl_hdr final composite straight into
// the r8g8b8a8 swapchain -> [PostProcess 2: diffusion DoF on an 8-bit copy of it] -> rl_prim_2d UI, all on
// display-encoded data.

namespace
{
   // User settings, persisted in the [Luma] config section.
   bool g_luma_msaa_enable = true; // HDR-aware MSAA resolve + alpha to coverage on alpha-tested materials
   bool g_smaa_enable = true;
   float g_rcas_sharpness = 0.f;
   bool g_luma_bloom_enable = true;
   bool g_gtao_enable = true;
   bool g_hide_ui = false; // Session-only, so a restart never comes back without a HUD.

   // The Volition "vint" UI draws through these rl_prim_2d / rl_prim_2d_tex pixel shaders (both variants of each); the
   // Bink video has its own rl_prim_2d_bink shaders and stays visible.
   const std::unordered_set<uint32_t> ui_pixel_shaders = {0xDF5FED78, 0x1606534E, 0x901E0D91, 0x079F6BD4};

   // The rl_hdr final composites (exe technique names in brackets), which write the swapchain: SMAA runs right after them.
   constexpr uint32_t tonemap_no_post_pixel_shader = 0xC235DDDD; // PostProcess 0, no bloom input
   constexpr std::pair<uint32_t, const char*> tonemap_pixel_shaders[] = {
      {0xED6DDA24, "rl_hdr_09 [final]"}, // PostProcess 1/2
      {0xD742C62C, "rl_hdr_10 [final_no_lut]"},
      {0x966367E4, "rl_hdr_08 [final_diffracted]"},
      {0x9F1F6557, "rl_hdr_06 [final_no_lut_diffracted]"},
      {tonemap_no_post_pixel_shader, "rl_hdr_05 [no_tonemapping] (PostProcess 0)"},
   };

   // Luma bloom pyramid (MELE's widths). No energy constant: the native combine sums three levels and the finals divide
   // bloom by 3, so vanilla shows their mean, and the pyramid's mips are energy-preserving means too.
   constexpr int kBloomMips = 6;
   constexpr float kBloomSigmas[kBloomMips] = {1.5f, 2.f, 2.f, 2.f, 2.f, 1.f};

   // The native bloom passes whose constants the Luma prefilter replays (Luma_Bloom_impl.hlsl), after the source
   // downsample below.
   constexpr uint32_t bloom_brightpass_pixel_shader = 0x12F9FD30; // rl_hdr_prep_08: vc0 c22 Bloom_curve_values, vc4 c1 Tint_color
   constexpr uint32_t bloom_combine_pixel_shader = 0x52563AB4;    // rl_hdr_prep_07: vc4 c1 Tint_color

   // A Luma shader is usable only once compiled; true when all the named ones are. The caller holds s_mutex_shader_objects.
   template <typename T, typename... Names>
   bool HasShaders(const T& shaders, Names... names)
   {
      const auto has = [&](uint32_t name)
      {
         const auto it = shaders.find(name);
         return it != shaders.end() && it->second;
      };
      return (has(names) && ...);
   }

   // Snapshots the pixel shader cbuffer bound at `slot` into `copy`, on the GPU. The game re-uploads the same vc0 / vc4
   // buffers for every pass, so a later pass sees an earlier pass's constants only through a copy.
   void CopyBoundPSConstantBuffer(ID3D11Device* device, ID3D11DeviceContext* device_context, UINT slot, com_ptr<ID3D11Buffer>& copy)
   {
      com_ptr<ID3D11Buffer> cb;
      device_context->PSGetConstantBuffers(slot, 1, &cb);
      if (!cb)
         return;
      D3D11_BUFFER_DESC cb_desc;
      cb->GetDesc(&cb_desc);
      D3D11_BUFFER_DESC copy_desc = {};
      if (copy)
         copy->GetDesc(&copy_desc);
      if (copy_desc.ByteWidth != cb_desc.ByteWidth)
      {
         copy.reset();
         copy_desc = cb_desc;
         copy_desc.Usage = D3D11_USAGE_DEFAULT;
         copy_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
         copy_desc.CPUAccessFlags = 0;
         copy_desc.MiscFlags = 0;
         copy_desc.StructureByteStride = 0;
         device->CreateBuffer(&copy_desc, nullptr, &copy);
      }
      if (copy)
         device_context->CopyResource(copy.get(), cb.get());
   }

   // The frame's first rl_downsample_02 is the quarter-res brightpass source (16-tap box * its vc4 Tint_color, b6).
   constexpr uint32_t downsample_pixel_shader = 0x27064754;
   constexpr uint32_t blur_pixel_shader = 0x378BA268; // rl_gaussian_blur_01, the bloom levels' separable blur

   // XeGTAO over rl_ssao_singleframe_calculate (SSAO_Level 2/3). Its 4 draws (one AO channel each, into a half-res target:
   // r8g8b8a8_unorm, r16g16b16a16_float once Luma's format upgrade reaches it) become one run of the 4 compute passes at the
   // target's size, copied into the target (created without UAV bind); the native blur and apply stay. Level 1
   // (multiframe) stays native.
   constexpr uint32_t ssao_singleframe_calculate_pixel_shader = 0x624BF56D;
   constexpr UINT gtao_knobs_cb_slot = 9;   // "register(b9)" in Luma_SR4_XeGTAO.hlsl; b11 is core DrawBloom's
   constexpr UINT gtao_depth_mip_count = 5; // XE_GTAO_DEPTH_MIP_LEVELS in Luma_SR4_XeGTAO.hlsl
   float g_gtao_final_value_power = 2.2f;   // DEV/TEST calibration knobs, not persisted
   float g_gtao_radius_override = 0.f;      // > 0 overrides the shader's EFFECT_RADIUS (metres)
#if DEVELOPMENT
   int g_gtao_debug_view = 0; // 0=off 1=depth gradient 2=normals 3=AO x8 4=edges
#endif

   // The material-pass pixel shaders that alpha test (discard on "Alpha_Threshold", one render target: the MSAA scene) and end
   // in "mul o0.xyzw, rX.xyzw, cb4[1].xyzw", from a census of every shader in the game's packfile (the same census
   // reproduces Saints Row: The Third's list exactly): grass, tree cards, billboards, windows, decals, cloth. 53 are
   // shared with Saints Row: The Third, 34 are the same materials recompiled; all 87 keep Alpha_Threshold at cb4[8].x.
   // Stipple-only discards (inferred-lighting translucency), the G-buffer pass and the alpha tests that output black are
   // left out.
   const std::unordered_set<uint32_t> alpha_test_material_pixel_shaders = {
      0x0699750F,
      0x082A8803,
      0x09BDCB94,
      0x0A6A298B,
      0x0B0E36DF,
      0x0DAC0BB0,
      0x186DE30B,
      0x187B6D2D,
      0x1D3989CD,
      0x2329F8C9,
      0x28E33A73,
      0x348A19F8,
      0x34DD8FFA,
      0x36361FCD,
      0x3926C52C,
      0x3927F339,
      0x3D396FFC,
      0x43CFBFEB,
      0x45B82576,
      0x488AB704,
      0x4EAF19BE,
      0x50F49B4B,
      0x50FFA155,
      0x54C8D58A,
      0x5EAE4FD3,
      0x5F2800D7,
      0x61DF4BD4,
      0x64F26798,
      0x65B338A4,
      0x672D46C7,
      0x6B2E5D5B,
      0x6F46C2D3,
      0x71FC661A,
      0x72517A56,
      0x74270DA5,
      0x742F80EA,
      0x77724E60,
      0x780F8B04,
      0x79E37859,
      0x7EFB7AFD,
      0x80D33059,
      0x833CD6A5,
      0x85EC8716,
      0x8A0C53E5,
      0x8DC4AF69,
      0x91CAD2FE,
      0x923D49AD,
      0x92BC1987,
      0x940E7C28,
      0x98A636E4,
      0x9A0A0553,
      0xA020BA98,
      0xA41B183F,
      0xA54A6B81,
      0xA6A30C1E,
      0xA902D17F,
      0xAD7FF76B,
      0xAF992C34,
      0xAFB33818,
      0xB6D050A7,
      0xB8C52156,
      0xBF0DAE37,
      0xC69A47EA,
      0xC791FDC5,
      0xC9C139B8,
      0xCDEACE0D,
      0xCE4B50AE,
      0xD04D05A1,
      0xD1C75010,
      0xD3611545,
      0xD79D6A07,
      0xD8650F52,
      0xDB03C7EE,
      0xDFD02DBF,
      0xE07333BF,
      0xE22569C5,
      0xE833B238,
      0xE84D2FA2,
      0xE84FE71D,
      0xEA0D8390,
      0xF2EFDC51,
      0xF355F7F4,
      0xF46060FB,
      0xF4759E94,
      0xF8041F13,
      0xF88CA20F,
      0xFC5E882F,
   };
#if DEVELOPMENT
   uint32_t g_msaa_resolves_this_frame = 0;
   uint32_t g_msaa_resolves_last_frame = 0;

   bool g_smaa_predication = true;
   bool g_smaa_edges_debug = false; // show SMAA's edges instead of the frame
   // Which rl_hdr final the game drew last, and how many finals the last frame contained.
   uint32_t g_final_perm = 0;
   uint32_t g_finals_this_frame = 0;
   uint32_t g_finals_last_frame = 0;
   // Native bloom constants readout: true inside the brightpass-to-combine window, where the blur / downsample vc4 are
   // copied. The staging copies live in bloom_readouts (read a frame late with DO_NOT_WAIT, so it never stalls).
   bool g_in_bloom_chain = false;

   // Native SSAO constants readout (XeGTAO calibration): vc0 of the frame's first calculate draw, read a frame late through
   // a staging copy and formatted into g_ssao_readout. Layout: c0.x ssao_fade_parameter, c1-c4 ssao_inv_proj (rows, applied as
   // ndc.x*c1 + ndc.y*c2 + depth*c3 + c4), c5.xy ssao_projection_scales, then ssao_sample_radius_reference at c18.x
   // (singleframe) or c14.x with ssao_temporal_falloff at c19.xy (multiframe).
   constexpr uint32_t ssao_multiframe_calculate_pixel_shader = 0x1D8BB773; // SSAO_Level 1

   uint32_t g_ssao_perm_this_frame = 0; // calculate shader whose vc0 was copied this frame
   uint32_t g_ssao_staging_perm = 0;    // ... and the one the staging copy holds
   uint32_t g_ssao_perm = 0;            // ... and the one "g_ssao_vc0" was read from
   uint32_t g_ssao_calculate_draws_this_frame = 0;
   uint32_t g_ssao_calculate_draws_last_frame = 0;
   float g_ssao_vc0[20][4] = {};
   float g_ssao_vc0_logged[20][4] = {};
   char g_ssao_readout[2048] = {}; // the formatted g_ssao_vc0, shown in the DEV panel and logged
   uint32_t g_ssao_frames_since_log = 0;
#endif
} // namespace

// Everything that holds a device object, so it is released with its device.
struct SaintsRowIVGameDeviceData final : public GameDeviceData
{
   // SMAA inputs: a snapshot of the gamma canvas (SMAA writes the canvas, so it cannot also sample it) and its
   // linear-light decode. Recreated when the canvas size or format changes. With RCAS on, SMAA writes its output into the
   // snapshot (only edge detection reads it, before that) and RCAS sharpens it back into the canvas.
   com_ptr<ID3D11Texture2D> smaa_gamma_texture;
   com_ptr<ID3D11ShaderResourceView> smaa_gamma_srv;
   com_ptr<ID3D11RenderTargetView> smaa_gamma_rtv;
   com_ptr<ID3D11UnorderedAccessView> smaa_linear_uav;
   com_ptr<ID3D11ShaderResourceView> smaa_linear_srv;
   com_ptr<ID3D11UnorderedAccessView> smaa_predication_uav;
   com_ptr<ID3D11ShaderResourceView> smaa_predication_srv;

   // GPU copy of the final composite's vc4 (Tint_saturation c0, Tint_color c1). The game's eye-adaptation exposure lives in
   // Tint_color, applied only in the composite, so the MSAA scene is unexposed; the weighted resolve reads last frame's copy
   // to weight samples in the displayed exposure. Copied only while Luma MSAA is on; null until then (the resolve shader
   // falls back to a box resolve on the zeroed binding).
   com_ptr<ID3D11Buffer> composite_tint_cb;

   // GPU copies of the native bloom's constants, taken at their own draws earlier in the same frame: the brightpass vc0
   // (bound at b0 for the prefilter) and vc4 (b4), the combine vc4 (b5) and the source downsample vc4 (b6). Copied only
   // while Luma bloom is on (always in DEVELOPMENT, for the readout); null until those passes first draw.
   com_ptr<ID3D11Buffer> bloom_brightpass_vc0_cb;
   com_ptr<ID3D11Buffer> bloom_brightpass_vc4_cb;
   com_ptr<ID3D11Buffer> bloom_combine_vc4_cb;
   com_ptr<ID3D11Buffer> bloom_source_downsample_vc4_cb;
   bool bloom_source_downsampled = false;

   // XeGTAO scratch.
   bool gtao_tried_this_frame = false;               // the frame's first calculate draw has run
   bool gtao_ran_this_frame = false;                 // ... and XeGTAO wrote all four channels, skip the other three
   com_ptr<ID3D11Texture2D> gtao_depth_mips_texture; // R32F view-space depth pyramid, 5 mips
   com_ptr<ID3D11UnorderedAccessView> gtao_depth_mip_uavs[gtao_depth_mip_count];
   com_ptr<ID3D11ShaderResourceView> gtao_depth_mips_srv;
   com_ptr<ID3D11UnorderedAccessView> gtao_working_uavs[2]; // R8G8_UNORM AO + edges ping-pong
   com_ptr<ID3D11ShaderResourceView> gtao_working_srvs[2];
   com_ptr<ID3D11Texture2D> gtao_final_texture; // copy source for the game's target, in the target's format
   com_ptr<ID3D11UnorderedAccessView> gtao_final_uav;
   // The size and format the set was built for, kept even when the allocation failed: a null set then means "failed",
   // and it is not retried every frame.
   uint32_t gtao_width = 0;
   uint32_t gtao_height = 0;
   DXGI_FORMAT gtao_format = DXGI_FORMAT_UNKNOWN;
   com_ptr<ID3D11Buffer> gtao_knobs_cb; // immutable, recreated when a knob changes
   float gtao_knobs[8] = {};

   void ReleaseGTAOScratch()
   {
      gtao_depth_mips_texture.reset();
      for (auto& uav : gtao_depth_mip_uavs)
         uav.reset();
      gtao_depth_mips_srv.reset();
      for (auto& uav : gtao_working_uavs)
         uav.reset();
      for (auto& srv : gtao_working_srvs)
         srv.reset();
      gtao_final_texture.reset();
      gtao_final_uav.reset();
      gtao_knobs_cb.reset();
      gtao_width = 0;
      gtao_height = 0;
      gtao_format = DXGI_FORMAT_UNKNOWN;
   }

#if DEVELOPMENT
   com_ptr<ID3D11Buffer> bloom_blur_vc4_cb;
   com_ptr<ID3D11Buffer> bloom_downsample_vc4_cb;
   struct BloomConstantReadout
   {
      const char* name;
      com_ptr<ID3D11Buffer> SaintsRowIVGameDeviceData::* source;
      UINT register_index;
      com_ptr<ID3D11Buffer> staging;
      float value[4];
   };
   BloomConstantReadout bloom_readouts[6] = {
      {"Source downsample Tint_color (vc4 c1)", &SaintsRowIVGameDeviceData::bloom_source_downsample_vc4_cb, 1},
      {"Brightpass Bloom_curve_values (vc0 c22)", &SaintsRowIVGameDeviceData::bloom_brightpass_vc0_cb, 22},
      {"Brightpass Tint_color (vc4 c1)", &SaintsRowIVGameDeviceData::bloom_brightpass_vc4_cb, 1},
      {"Blur Tint_color (vc4 c1)", &SaintsRowIVGameDeviceData::bloom_blur_vc4_cb, 1},
      {"Downsample Tint_color (vc4 c1)", &SaintsRowIVGameDeviceData::bloom_downsample_vc4_cb, 1},
      {"Combine Tint_color (vc4 c1)", &SaintsRowIVGameDeviceData::bloom_combine_vc4_cb, 1},
   };

   com_ptr<ID3D11Buffer> ssao_vc0_cb;
   com_ptr<ID3D11Buffer> ssao_vc0_staging;
#endif
};

class SaintsRowIV final : public Game
{
   static SaintsRowIVGameDeviceData& GetGameDeviceData(DeviceData& device_data)
   {
      return *static_cast<SaintsRowIVGameDeviceData*>(device_data.game);
   }

public:
   void OnCreateDevice(ID3D11Device* native_device, DeviceData& device_data) override
   {
      device_data.game = new SaintsRowIVGameDeviceData;
   }

   void OnDestroyDeviceData(DeviceData& device_data) override
   {
      // GameDeviceData lacks a virtual destructor; delete through the concrete type to release derived members.
      delete static_cast<SaintsRowIVGameDeviceData*>(device_data.game);
      device_data.game = nullptr;
   }

   // Sharpened alpha (Golus) for alpha to coverage: "o0.a" becomes saturate((a - Alpha_Threshold) / fwidth(a) + 0.5), so
   // coverage is full inside a cut-out and only ramps across its edge pixel; the plain alpha would thin foliage out, since
   // vanilla draws everything above the threshold opaque. In every listed shader rX.w is the tested alpha (the one compared
   // against Alpha_Threshold, cb4[8].x) and rX is dead after the final mul, so the patch reuses it before the ret:
   //   add rX.w, rX.w, -cb4[8].x | deriv_rtx rX.x, rX.w | deriv_rty rX.y, rX.w | add rX.x, |rX.x|, |rX.y|
   //   max rX.x, rX.x, l(0.0001) | div rX.w, rX.w, rX.x | add_sat o0.w, rX.w, l(0.5)
   // The discard stays, so coverage runs 0.5..1 across the edge and the silhouette never grows past vanilla.
   std::unique_ptr<std::byte[]> PatchShaderBytecodeSync(const std::byte* code, size_t& size, reshade::api::pipeline_subobject_type type, uint64_t shader_hash, const std::byte* shader_object, size_t shader_object_size) override
   {
      constexpr size_t tail_tokens = 9; // mul (8) + ret (1)
      if (type != reshade::api::pipeline_subobject_type::pixel_shader || !alpha_test_material_pixel_shaders.contains(uint32_t(shader_hash)) || size % sizeof(uint32_t) != 0 || size < tail_tokens * sizeof(uint32_t))
         return nullptr;
      const uint32_t* tail = reinterpret_cast<const uint32_t*>(code) + size / sizeof(uint32_t) - tail_tokens;

      const uint32_t operand_4_component_1d = ENCODE_D3D10_SB_OPERAND_NUM_COMPONENTS(D3D10_SB_OPERAND_4_COMPONENT) | ENCODE_D3D10_SB_OPERAND_INDEX_DIMENSION(D3D10_SB_OPERAND_INDEX_1D) | ENCODE_D3D10_SB_OPERAND_INDEX_REPRESENTATION(0, D3D10_SB_OPERAND_INDEX_IMMEDIATE32);
      const uint32_t swizzle_mode = ENCODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(D3D10_SB_OPERAND_4_COMPONENT_SWIZZLE_MODE);
      const bool tail_matches =
         tail[0] == (ENCODE_D3D10_SB_OPCODE_TYPE(D3D10_SB_OPCODE_MUL) | ENCODE_D3D10_SB_TOKENIZED_INSTRUCTION_LENGTH(8)) &&
         tail[1] == (operand_4_component_1d | ENCODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(D3D10_SB_OPERAND_4_COMPONENT_MASK_MODE) | D3D10_SB_OPERAND_4_COMPONENT_MASK_ALL | ENCODE_D3D10_SB_OPERAND_TYPE(D3D10_SB_OPERAND_TYPE_OUTPUT)) && tail[2] == 0 &&
         (tail[3] & ~D3D10_SB_OPERAND_4_COMPONENT_SWIZZLE_MASK) == (operand_4_component_1d | swizzle_mode | ENCODE_D3D10_SB_OPERAND_TYPE(D3D10_SB_OPERAND_TYPE_TEMP)) &&
         (tail[5] & ~D3D10_SB_OPERAND_4_COMPONENT_SWIZZLE_MASK) == (ENCODE_D3D10_SB_OPERAND_NUM_COMPONENTS(D3D10_SB_OPERAND_4_COMPONENT) | ENCODE_D3D10_SB_OPERAND_INDEX_DIMENSION(D3D10_SB_OPERAND_INDEX_2D) | swizzle_mode | ENCODE_D3D10_SB_OPERAND_TYPE(D3D10_SB_OPERAND_TYPE_CONSTANT_BUFFER)) && tail[6] == 4 && tail[7] == 1 &&
         tail[8] == (ENCODE_D3D10_SB_OPCODE_TYPE(D3D10_SB_OPCODE_RET) | ENCODE_D3D10_SB_TOKENIZED_INSTRUCTION_LENGTH(1));
      ASSERT_ONCE(tail_matches);
      if (!tail_matches)
         return nullptr;
      const uint32_t r = tail[4];

      std::vector<uint32_t> patch;
      const auto dest = [&](D3D10_SB_OPERAND_TYPE operand_type, uint32_t index, uint32_t component)
      {
         patch.insert(patch.end(), {operand_4_component_1d | ENCODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(D3D10_SB_OPERAND_4_COMPONENT_MASK_MODE) | (D3D10_SB_OPERAND_4_COMPONENT_MASK_X << component) | ENCODE_D3D10_SB_OPERAND_TYPE(operand_type), index});
      };
      const auto src = [&](uint32_t component, D3D10_SB_OPERAND_MODIFIER modifier = D3D10_SB_OPERAND_MODIFIER_NONE)
      {
         const uint32_t token = operand_4_component_1d | ENCODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(D3D10_SB_OPERAND_4_COMPONENT_SELECT_1_MODE) | ENCODE_D3D10_SB_OPERAND_4_COMPONENT_SELECT_1(component) | ENCODE_D3D10_SB_OPERAND_TYPE(D3D10_SB_OPERAND_TYPE_TEMP);
         if (modifier == D3D10_SB_OPERAND_MODIFIER_NONE)
            patch.insert(patch.end(), {token, r});
         else
            patch.insert(patch.end(), {token | ENCODE_D3D10_SB_OPERAND_EXTENDED(1), ENCODE_D3D10_SB_EXTENDED_OPERAND_TYPE(D3D10_SB_EXTENDED_OPERAND_MODIFIER) | ENCODE_D3D10_SB_EXTENDED_OPERAND_MODIFIER(modifier), r});
      };
      const auto immediate = [&](float value)
      {
         patch.insert(patch.end(), {ENCODE_D3D10_SB_OPERAND_NUM_COMPONENTS(D3D10_SB_OPERAND_1_COMPONENT) | ENCODE_D3D10_SB_OPERAND_TYPE(D3D10_SB_OPERAND_TYPE_IMMEDIATE32), std::bit_cast<uint32_t>(value)});
      };
      // Emits the opcode token, then fills in its length once the operands are in.
      const auto instruction = [&](D3D10_SB_OPCODE_TYPE opcode, auto&& operands, bool saturate = false)
      {
         const size_t start = patch.size();
         patch.push_back(0);
         operands();
         patch[start] = ENCODE_D3D10_SB_OPCODE_TYPE(opcode) | ENCODE_D3D10_SB_TOKENIZED_INSTRUCTION_LENGTH(uint32_t(patch.size() - start)) | ENCODE_D3D10_SB_INSTRUCTION_SATURATE(saturate);
      };
      instruction(D3D10_SB_OPCODE_ADD, [&]
         {
         dest(D3D10_SB_OPERAND_TYPE_TEMP, r, D3D10_SB_4_COMPONENT_W);
         src(D3D10_SB_4_COMPONENT_W);
         // -cb4[8].x (Alpha_Threshold)
         patch.insert(patch.end(), {ENCODE_D3D10_SB_OPERAND_NUM_COMPONENTS(D3D10_SB_OPERAND_4_COMPONENT) | ENCODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(D3D10_SB_OPERAND_4_COMPONENT_SELECT_1_MODE) | ENCODE_D3D10_SB_OPERAND_4_COMPONENT_SELECT_1(D3D10_SB_4_COMPONENT_X) | ENCODE_D3D10_SB_OPERAND_TYPE(D3D10_SB_OPERAND_TYPE_CONSTANT_BUFFER) | ENCODE_D3D10_SB_OPERAND_INDEX_DIMENSION(D3D10_SB_OPERAND_INDEX_2D) | ENCODE_D3D10_SB_OPERAND_EXTENDED(1),
            ENCODE_D3D10_SB_EXTENDED_OPERAND_TYPE(D3D10_SB_EXTENDED_OPERAND_MODIFIER) | ENCODE_D3D10_SB_EXTENDED_OPERAND_MODIFIER(D3D10_SB_OPERAND_MODIFIER_NEG), 4, 8}); });
      instruction(D3D10_SB_OPCODE_DERIV_RTX, [&]
         { dest(D3D10_SB_OPERAND_TYPE_TEMP, r, D3D10_SB_4_COMPONENT_X); src(D3D10_SB_4_COMPONENT_W); });
      instruction(D3D10_SB_OPCODE_DERIV_RTY, [&]
         { dest(D3D10_SB_OPERAND_TYPE_TEMP, r, D3D10_SB_4_COMPONENT_Y); src(D3D10_SB_4_COMPONENT_W); });
      instruction(D3D10_SB_OPCODE_ADD, [&]
         { dest(D3D10_SB_OPERAND_TYPE_TEMP, r, D3D10_SB_4_COMPONENT_X); src(D3D10_SB_4_COMPONENT_X, D3D10_SB_OPERAND_MODIFIER_ABS); src(D3D10_SB_4_COMPONENT_Y, D3D10_SB_OPERAND_MODIFIER_ABS); });
      instruction(D3D10_SB_OPCODE_MAX, [&]
         { dest(D3D10_SB_OPERAND_TYPE_TEMP, r, D3D10_SB_4_COMPONENT_X); src(D3D10_SB_4_COMPONENT_X); immediate(0.0001f); });
      instruction(D3D10_SB_OPCODE_DIV, [&]
         { dest(D3D10_SB_OPERAND_TYPE_TEMP, r, D3D10_SB_4_COMPONENT_W); src(D3D10_SB_4_COMPONENT_W); src(D3D10_SB_4_COMPONENT_X); });
      instruction(D3D10_SB_OPCODE_ADD, [&]
         { dest(D3D10_SB_OPERAND_TYPE_OUTPUT, 0, D3D10_SB_4_COMPONENT_W); src(D3D10_SB_4_COMPONENT_W); immediate(0.5f); }, true);
#if DEVELOPMENT
      // The same patch for r3, assembled offline and checked with fxc /dumpbin.
      if (r == 3)
      {
         constexpr uint32_t expected[] = {0x09000000, 0x00100082, 0x00000003, 0x0010003A, 0x00000003, 0x8020800A, 0x00000041, 0x00000004, 0x00000008, 0x0500000B, 0x00100012, 0x00000003, 0x0010003A, 0x00000003, 0x0500000C, 0x00100022, 0x00000003, 0x0010003A, 0x00000003, 0x09000000, 0x00100012, 0x00000003, 0x8010000A, 0x00000081, 0x00000003, 0x8010001A, 0x00000081, 0x00000003, 0x07000034, 0x00100012, 0x00000003, 0x0010000A, 0x00000003, 0x00004001, 0x38D1B717, 0x0700000E, 0x00100082, 0x00000003, 0x0010003A, 0x00000003, 0x0010000A, 0x00000003, 0x07002000, 0x00102082, 0x00000000, 0x0010003A, 0x00000003, 0x00004001, 0x3F000000};
         ASSERT_ONCE(patch.size() == std::size(expected) && std::equal(patch.begin(), patch.end(), std::begin(expected)));
      }
#endif

      const size_t patch_size = patch.size() * sizeof(uint32_t);
      const size_t ret_offset = size - sizeof(uint32_t);
      auto new_code = std::make_unique<std::byte[]>(size + patch_size);
      std::memcpy(new_code.get(), code, ret_offset);
      std::memcpy(new_code.get() + ret_offset, patch.data(), patch_size);
      std::memcpy(new_code.get() + ret_offset + patch_size, code + ret_offset, sizeof(uint32_t));
      size += patch_size;
      return new_code;
   }

   // The patched variant is swapped in per draw (OnDrawOrDispatch), only where it is safe.
   bool OnBindPatchedShader(DeviceData& device_data, uint32_t shader_hash, reshade::api::pipeline_subobject_type type) override
   {
      return false;
   }

   // Draws the final composite, then SMAA on the canvas it wrote (the swapchain), before DoF and the UI read it.
   // Anything missing (shaders still compiling, an unexpected target) leaves the composite alone and skips SMAA.
   DrawOrDispatchOverrideType DrawTonemapWithSMAA(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, CommandListData& cmd_list_data, DeviceData& device_data, bool& updated_cbuffers, const std::function<void()>& original_draw_dispatch_func)
   {
      com_ptr<ID3D11RenderTargetView> canvas_rtv;
      native_device_context->OMGetRenderTargets(1, &canvas_rtv, nullptr);
      com_ptr<ID3D11Resource> canvas_resource;
      if (canvas_rtv)
         canvas_rtv->GetResource(&canvas_resource);
      com_ptr<ID3D11Texture2D> canvas_texture;
      if (!canvas_resource || FAILED(canvas_resource->QueryInterface(&canvas_texture)))
         return DrawOrDispatchOverrideType::None;
      D3D11_TEXTURE2D_DESC canvas_desc;
      canvas_texture->GetDesc(&canvas_desc);
      if (canvas_desc.SampleDesc.Count != 1 || canvas_desc.ArraySize != 1)
         return DrawOrDispatchOverrideType::None;
      auto& game_device_data = GetGameDeviceData(device_data);

      // Held through SMAA so a shader reload cannot release them mid-use; "DrawSMAA" looks its shaders up with "at".
      const std::shared_lock lock_shader_objects(s_mutex_shader_objects);
      if (!HasShaders(device_data.native_vertex_shaders, "SMAA Edge Detection VS"_h, "SMAA Blending Weight Calculation VS"_h, "SMAA Neighborhood Blending VS"_h) || !HasShaders(device_data.native_pixel_shaders, "SMAA Edge Detection PS"_h, "SMAA Blending Weight Calculation PS"_h, "SMAA Neighborhood Blending PS"_h) || !HasShaders(device_data.native_compute_shaders, "SR4 SMAA Linearize CS"_h))
         return DrawOrDispatchOverrideType::None;

      D3D11_TEXTURE2D_DESC snapshot_desc = {};
      if (game_device_data.smaa_gamma_texture)
         game_device_data.smaa_gamma_texture->GetDesc(&snapshot_desc);
      if (snapshot_desc.Width != canvas_desc.Width || snapshot_desc.Height != canvas_desc.Height || snapshot_desc.Format != canvas_desc.Format)
      {
         game_device_data.smaa_gamma_texture.reset();
         game_device_data.smaa_gamma_srv.reset();
         game_device_data.smaa_gamma_rtv.reset();
         game_device_data.smaa_linear_uav.reset();
         game_device_data.smaa_linear_srv.reset();
         game_device_data.smaa_predication_uav.reset();
         game_device_data.smaa_predication_srv.reset();
         D3D11_TEXTURE2D_DESC desc = canvas_desc;
         desc.MipLevels = 1;
         desc.Usage = D3D11_USAGE_DEFAULT;
         desc.CPUAccessFlags = 0;
         desc.MiscFlags = 0;
         desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
         if (FAILED(native_device->CreateTexture2D(&desc, nullptr, &game_device_data.smaa_gamma_texture)) || FAILED(native_device->CreateShaderResourceView(game_device_data.smaa_gamma_texture.get(), nullptr, &game_device_data.smaa_gamma_srv)) || FAILED(native_device->CreateRenderTargetView(game_device_data.smaa_gamma_texture.get(), nullptr, &game_device_data.smaa_gamma_rtv)))
         {
            game_device_data.smaa_gamma_texture.reset();
            return DrawOrDispatchOverrideType::None;
         }
         desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
         desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
         com_ptr<ID3D11Texture2D> linear_texture;
         if (FAILED(native_device->CreateTexture2D(&desc, nullptr, &linear_texture)) || FAILED(native_device->CreateUnorderedAccessView(linear_texture.get(), nullptr, &game_device_data.smaa_linear_uav)) || FAILED(native_device->CreateShaderResourceView(linear_texture.get(), nullptr, &game_device_data.smaa_linear_srv)))
         {
            game_device_data.smaa_gamma_texture.reset();
            return DrawOrDispatchOverrideType::None;
         }
         // Without it SMAA simply runs unpredicated.
         desc.Format = DXGI_FORMAT_R16_FLOAT;
         com_ptr<ID3D11Texture2D> predication_texture;
         if (FAILED(native_device->CreateTexture2D(&desc, nullptr, &predication_texture)) || FAILED(native_device->CreateUnorderedAccessView(predication_texture.get(), nullptr, &game_device_data.smaa_predication_uav)) || FAILED(native_device->CreateShaderResourceView(predication_texture.get(), nullptr, &game_device_data.smaa_predication_srv)))
         {
            game_device_data.smaa_predication_uav.reset();
            game_device_data.smaa_predication_srv.reset();
         }
      }

      // Predication depth: the final composite's own depth input (t5, the R24 main-pass depth the DoF weight reads).
      // Anything else (another format or size, or nothing bound) falls back to plain ULTRA.
      com_ptr<ID3D11ShaderResourceView> depth_srv;
      bool predication_available = game_device_data.smaa_predication_uav && HasShaders(device_data.native_compute_shaders, "SR4 SMAA Predication CS"_h);
#if DEVELOPMENT
      predication_available = predication_available && g_smaa_predication;
#endif
      if (predication_available)
         native_device_context->PSGetShaderResources(5, 1, &depth_srv);
      if (depth_srv)
      {
         D3D11_SHADER_RESOURCE_VIEW_DESC depth_srv_desc;
         depth_srv->GetDesc(&depth_srv_desc);
         uint4 depth_size;
         DXGI_FORMAT depth_format;
         GetResourceInfo(depth_srv.get(), depth_size, depth_format);
         if (depth_srv_desc.Format != DXGI_FORMAT_R24_UNORM_X8_TYPELESS || depth_srv_desc.ViewDimension != D3D11_SRV_DIMENSION_TEXTURE2D || depth_size.x != canvas_desc.Width || depth_size.y != canvas_desc.Height)
            depth_srv.reset();
      }

      original_draw_dispatch_func();
      native_device_context->CopyResource(game_device_data.smaa_gamma_texture.get(), canvas_resource.get());

      {
         DrawStateStack<DrawStateStackType::Compute> linearize_state;
         linearize_state.Cache(native_device_context, device_data.uav_max_count);
         ID3D11UnorderedAccessView* const linear_uav = game_device_data.smaa_linear_uav.get();
         ID3D11ShaderResourceView* const gamma_srv = game_device_data.smaa_gamma_srv.get();
         native_device_context->CSSetUnorderedAccessViews(0, 1, &linear_uav, nullptr);
         native_device_context->CSSetShaderResources(0, 1, &gamma_srv);
         native_device_context->CSSetShader(device_data.native_compute_shaders.at("SR4 SMAA Linearize CS"_h).get(), nullptr, 0);
         native_device_context->Dispatch((canvas_desc.Width + 7) / 8, (canvas_desc.Height + 7) / 8, 1);
         if (depth_srv)
         {
            ID3D11UnorderedAccessView* const predication_uav = game_device_data.smaa_predication_uav.get();
            ID3D11ShaderResourceView* const raw_depth_srv = depth_srv.get();
            native_device_context->CSSetUnorderedAccessViews(0, 1, &predication_uav, nullptr);
            native_device_context->CSSetShaderResources(0, 1, &raw_depth_srv);
            native_device_context->CSSetShader(device_data.native_compute_shaders.at("SR4 SMAA Predication CS"_h).get(), nullptr, 0);
            native_device_context->Dispatch((canvas_desc.Width + 7) / 8, (canvas_desc.Height + 7) / 8, 1);
         }
         linearize_state.Restore(native_device_context);
      }

      // The SMAA shaders read the canvas size from the Luma settings and the predication scale from the Luma data, in both stages.
      SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, reshade::api::shader_stage::vertex | reshade::api::shader_stage::pixel, LumaConstantBufferType::LumaSettings);
      SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, reshade::api::shader_stage::vertex | reshade::api::shader_stage::pixel, LumaConstantBufferType::LumaData, 0, 0, depth_srv ? 2.f : 1.f);
      updated_cbuffers = true;
      const bool sharpen = g_rcas_sharpness > 0.f && HasShaders(device_data.native_vertex_shaders, "Copy VS"_h) && HasShaders(device_data.native_pixel_shaders, "SR4 Sharpen PS"_h);
      DrawSMAA(native_device, native_device_context, device_data, sharpen ? game_device_data.smaa_gamma_rtv.get() : canvas_rtv.get(), game_device_data.smaa_linear_srv.get(), game_device_data.smaa_gamma_srv.get(), depth_srv ? game_device_data.smaa_predication_srv.get() : nullptr);
#if DEVELOPMENT
      // Calibration aid: SMAA's edge texture (red = horizontal, green = vertical edges) replaces the frame.
      if (g_smaa_edges_debug && HasShaders(device_data.native_vertex_shaders, "Copy VS"_h) && HasShaders(device_data.native_pixel_shaders, "Copy PS"_h))
      {
         DrawStateStack<DrawStateStackType::FullGraphics> debug_state;
         debug_state.Cache(native_device_context, device_data.uav_max_count);
         DrawCustomPixelShader(native_device_context, device_data.default_depth_stencil_state.get(), device_data.default_blend_state.get(), nullptr, device_data.native_vertex_shaders.at("Copy VS"_h).get(), device_data.native_pixel_shaders.at("Copy PS"_h).get(), device_data.managed_resources.shader_resource_views["smaa_edge_detection"_h].get(), canvas_rtv.get(), canvas_desc.Width, canvas_desc.Height, false);
         debug_state.Restore(native_device_context);
         return DrawOrDispatchOverrideType::Replaced;
      }
#endif
      if (sharpen)
      {
         DrawStateStack<DrawStateStackType::FullGraphics> sharpen_state;
         sharpen_state.Cache(native_device_context, device_data.uav_max_count);
         SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, reshade::api::shader_stage::pixel, LumaConstantBufferType::LumaData, 0, 0, g_rcas_sharpness);
         DrawCustomPixelShader(native_device_context, device_data.default_depth_stencil_state.get(), device_data.default_blend_state.get(), nullptr, device_data.native_vertex_shaders.at("Copy VS"_h).get(), device_data.native_pixel_shaders.at("SR4 Sharpen PS"_h).get(), game_device_data.smaa_gamma_srv.get(), canvas_rtv.get(), canvas_desc.Width, canvas_desc.Height, false);
         sharpen_state.Restore(native_device_context);
      }
      return DrawOrDispatchOverrideType::Replaced;
   }

   // XeGTAO in place of the first singleframe calculate draw: prefilter, main pass and two denoisers on the draw's own
   // inputs (t14 depth, t13 normals, vc0 at b0 for this frame's projection), then a copy into its render target. Returns
   // false, and the native draw runs, when an input, a shader or the scratch is missing.
   bool RunXeGTAO(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, DeviceData& device_data)
   {
      // Held through the dispatches so a shader reload cannot release them mid-use.
      const std::shared_lock lock_shader_objects(s_mutex_shader_objects);
      const auto& shaders = device_data.native_compute_shaders;
      if (!HasShaders(shaders, "SR4 XeGTAO Prefilter Depths CS"_h, "SR4 XeGTAO Main Pass CS"_h, "SR4 XeGTAO Denoise Pass 1 CS"_h, "SR4 XeGTAO Denoise Pass 2 CS"_h))
         return false;

      com_ptr<ID3D11ShaderResourceView> normals_srv;
      com_ptr<ID3D11ShaderResourceView> depth_srv;
      com_ptr<ID3D11Buffer> vc0;
      com_ptr<ID3D11RenderTargetView> target_rtv;
      com_ptr<ID3D11DepthStencilView> target_dsv;
      native_device_context->PSGetShaderResources(13, 1, &normals_srv);
      native_device_context->PSGetShaderResources(14, 1, &depth_srv);
      native_device_context->PSGetConstantBuffers(0, 1, &vc0);
      native_device_context->OMGetRenderTargets(1, &target_rtv, &target_dsv);
      if (!normals_srv || !depth_srv || !vc0 || !target_rtv)
         return false;
      com_ptr<ID3D11Resource> target;
      target_rtv->GetResource(&target);
      D3D11_RENDER_TARGET_VIEW_DESC target_rtv_desc;
      target_rtv->GetDesc(&target_rtv_desc);
      uint4 target_size, depth_size, normals_size;
      DXGI_FORMAT unused_format;
      GetResourceInfo(target.get(), target_size, unused_format);
      GetResourceInfo(depth_srv.get(), depth_size, unused_format);
      GetResourceInfo(normals_srv.get(), normals_size, unused_format);
      const uint32_t width = target_size.x;
      const uint32_t height = target_size.y;
      // The final denoiser stores through a typed UAV in the target's own view format, so the copy stays within its format group.
      if (width == 0 || height == 0 || (target_rtv_desc.Format != DXGI_FORMAT_R8G8B8A8_UNORM && target_rtv_desc.Format != DXGI_FORMAT_R16G16B16A16_FLOAT))
         return false;
      auto& game_device_data = GetGameDeviceData(device_data);
      // Full-res depth and normals per target pixel (2 at every level; rounded, as a half-res target of an odd size rounds).
      const uint32_t input_scale = (depth_size.x + width / 2) / width;
      if (input_scale == 0 || (depth_size.y + height / 2) / height != input_scale || normals_size.x != depth_size.x || normals_size.y != depth_size.y)
         return false;

      if (game_device_data.gtao_width != width || game_device_data.gtao_height != height || game_device_data.gtao_format != target_rtv_desc.Format)
      {
         game_device_data.ReleaseGTAOScratch();
         D3D11_TEXTURE2D_DESC desc = {};
         desc.Width = width;
         desc.Height = height;
         desc.MipLevels = gtao_depth_mip_count;
         desc.ArraySize = 1;
         desc.Format = DXGI_FORMAT_R32_FLOAT;
         desc.SampleDesc.Count = 1;
         desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
         bool ok = SUCCEEDED(native_device->CreateTexture2D(&desc, nullptr, &game_device_data.gtao_depth_mips_texture)) && SUCCEEDED(native_device->CreateShaderResourceView(game_device_data.gtao_depth_mips_texture.get(), nullptr, &game_device_data.gtao_depth_mips_srv));
         D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc = {};
         uav_desc.Format = desc.Format;
         uav_desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
         for (UINT mip = 0; ok && mip < gtao_depth_mip_count; mip++)
         {
            uav_desc.Texture2D.MipSlice = mip;
            ok = SUCCEEDED(native_device->CreateUnorderedAccessView(game_device_data.gtao_depth_mips_texture.get(), &uav_desc, &game_device_data.gtao_depth_mip_uavs[mip]));
         }
         desc.MipLevels = 1;
         desc.Format = DXGI_FORMAT_R8G8_UNORM;
         for (int i = 0; ok && i < 2; i++)
         {
            com_ptr<ID3D11Texture2D> working_texture;
            ok = SUCCEEDED(native_device->CreateTexture2D(&desc, nullptr, &working_texture)) && SUCCEEDED(native_device->CreateUnorderedAccessView(working_texture.get(), nullptr, &game_device_data.gtao_working_uavs[i])) && SUCCEEDED(native_device->CreateShaderResourceView(working_texture.get(), nullptr, &game_device_data.gtao_working_srvs[i]));
         }
         desc.Format = target_rtv_desc.Format;
         ok = ok && SUCCEEDED(native_device->CreateTexture2D(&desc, nullptr, &game_device_data.gtao_final_texture)) && SUCCEEDED(native_device->CreateUnorderedAccessView(game_device_data.gtao_final_texture.get(), nullptr, &game_device_data.gtao_final_uav));
         if (!ok)
            game_device_data.ReleaseGTAOScratch();
         game_device_data.gtao_width = width;
         game_device_data.gtao_height = height;
         game_device_data.gtao_format = target_rtv_desc.Format;
      }
      if (!game_device_data.gtao_final_uav)
         return false;

#if DEVELOPMENT
      const float debug_view = float(g_gtao_debug_view);
#else
      const float debug_view = 0.f;
#endif
      const float knobs[8] = {g_gtao_final_value_power, float(input_scale), g_gtao_radius_override, debug_view, 1.f / float(width), 1.f / float(height), 0.f, 0.f};
      if (!game_device_data.gtao_knobs_cb || std::memcmp(game_device_data.gtao_knobs, knobs, sizeof(knobs)) != 0)
      {
         game_device_data.gtao_knobs_cb.reset();
         D3D11_BUFFER_DESC cb_desc = {};
         cb_desc.ByteWidth = sizeof(knobs);
         cb_desc.Usage = D3D11_USAGE_IMMUTABLE;
         cb_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
         const D3D11_SUBRESOURCE_DATA cb_data = {knobs};
         if (FAILED(native_device->CreateBuffer(&cb_desc, &cb_data, &game_device_data.gtao_knobs_cb)))
            return false;
         std::memcpy(game_device_data.gtao_knobs, knobs, sizeof(knobs));
      }

      DrawStateStack<DrawStateStackType::Compute> compute_state;
      compute_state.Cache(native_device_context, device_data.uav_max_count);
      ID3D11Buffer* const cbs[] = {vc0.get(), game_device_data.gtao_knobs_cb.get()};
      native_device_context->CSSetConstantBuffers(0, 1, &cbs[0]);
      native_device_context->CSSetConstantBuffers(gtao_knobs_cb_slot, 1, &cbs[1]);
      ID3D11SamplerState* const point_sampler = device_data.sampler_state_point.get();
      native_device_context->CSSetSamplers(0, 1, &point_sampler);

      // Each pass binds its destination UAV before its source SRVs: D3D11 otherwise nulls an SRV that still aliases the
      // previous pass's bound UAV.
      ID3D11ShaderResourceView* const null_srvs[2] = {};
      ID3D11UnorderedAccessView* const null_uavs[gtao_depth_mip_count] = {};
      const auto pass = [&](uint32_t shader_name_hash, UINT uav_count, ID3D11UnorderedAccessView* const* uavs, ID3D11ShaderResourceView* const(&srvs)[2], UINT groups_x, UINT groups_y)
      {
         native_device_context->CSSetShaderResources(0, 2, null_srvs);
         native_device_context->CSSetUnorderedAccessViews(0, uav_count, uavs, nullptr);
         native_device_context->CSSetShaderResources(0, 2, srvs);
         native_device_context->CSSetShader(shaders.at(shader_name_hash).get(), nullptr, 0);
         native_device_context->Dispatch(groups_x, groups_y, 1);
         native_device_context->CSSetUnorderedAccessViews(0, uav_count, null_uavs, nullptr);
      };
      ID3D11UnorderedAccessView* const mip_uavs[gtao_depth_mip_count] = {game_device_data.gtao_depth_mip_uavs[0].get(), game_device_data.gtao_depth_mip_uavs[1].get(), game_device_data.gtao_depth_mip_uavs[2].get(), game_device_data.gtao_depth_mip_uavs[3].get(), game_device_data.gtao_depth_mip_uavs[4].get()};
      ID3D11UnorderedAccessView* const working_uavs[2] = {game_device_data.gtao_working_uavs[0].get(), game_device_data.gtao_working_uavs[1].get()};
      ID3D11UnorderedAccessView* const final_uav = game_device_data.gtao_final_uav.get();
      pass("SR4 XeGTAO Prefilter Depths CS"_h, gtao_depth_mip_count, mip_uavs, {depth_srv.get(), nullptr}, (width + 15) / 16, (height + 15) / 16);
      pass("SR4 XeGTAO Main Pass CS"_h, 1, &working_uavs[0], {game_device_data.gtao_depth_mips_srv.get(), normals_srv.get()}, (width + 7) / 8, (height + 7) / 8);
      pass("SR4 XeGTAO Denoise Pass 1 CS"_h, 1, &working_uavs[1], {game_device_data.gtao_working_srvs[0].get(), nullptr}, (width + 15) / 16, (height + 7) / 8);
      pass("SR4 XeGTAO Denoise Pass 2 CS"_h, 1, &final_uav, {game_device_data.gtao_working_srvs[1].get(), nullptr}, (width + 15) / 16, (height + 7) / 8);
      compute_state.Restore(native_device_context);

      // The target is still bound as the draw's render target, and a copy into an OM-bound resource is a hazard the runtime
      // does not resolve: unbind around the copy, then give the game its binding back.
      native_device_context->OMSetRenderTargets(0, nullptr, nullptr);
      native_device_context->CopyResource(target.get(), game_device_data.gtao_final_texture.get());
      ID3D11RenderTargetView* const rtv = target_rtv.get();
      native_device_context->OMSetRenderTargets(1, &rtv, target_dsv.get());
      return true;
   }

   DrawOrDispatchOverrideType OnDrawOrDispatch(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, CommandListData& cmd_list_data, DeviceData& device_data, reshade::api::shader_stage stages, const ShaderHashesList<OneShaderPerPipeline>& original_shader_hashes, bool is_custom_pass, bool& updated_cbuffers, std::function<void()>* original_draw_dispatch_func) override
   {
      auto& game_device_data = GetGameDeviceData(device_data);
      const uint32_t pixel_shader_hash = uint32_t(original_shader_hashes.pixel_shaders[0]);
      // The native bloom constants only feed Luma bloom (and the DEV readout).
      const bool copy_bloom_constants = g_luma_bloom_enable || DEVELOPMENT;
      if (copy_bloom_constants)
      {
         if (pixel_shader_hash == bloom_brightpass_pixel_shader)
         {
            CopyBoundPSConstantBuffer(native_device, native_device_context, 0, game_device_data.bloom_brightpass_vc0_cb);
            CopyBoundPSConstantBuffer(native_device, native_device_context, 4, game_device_data.bloom_brightpass_vc4_cb);
#if DEVELOPMENT
            g_in_bloom_chain = true;
#endif
            return DrawOrDispatchOverrideType::None;
         }
         if (pixel_shader_hash == bloom_combine_pixel_shader)
         {
            CopyBoundPSConstantBuffer(native_device, native_device_context, 4, game_device_data.bloom_combine_vc4_cb);
#if DEVELOPMENT
            g_in_bloom_chain = false;
#endif
            return DrawOrDispatchOverrideType::None;
         }
         if (!game_device_data.bloom_source_downsampled && pixel_shader_hash == downsample_pixel_shader)
         {
            game_device_data.bloom_source_downsampled = true;
            CopyBoundPSConstantBuffer(native_device, native_device_context, 4, game_device_data.bloom_source_downsample_vc4_cb);
            return DrawOrDispatchOverrideType::None;
         }
      }
#if DEVELOPMENT
      if (pixel_shader_hash == ssao_singleframe_calculate_pixel_shader || pixel_shader_hash == ssao_multiframe_calculate_pixel_shader)
      {
         if (g_ssao_calculate_draws_this_frame++ == 0)
         {
            g_ssao_perm_this_frame = pixel_shader_hash;
            CopyBoundPSConstantBuffer(native_device, native_device_context, 0, game_device_data.ssao_vc0_cb);
         }
      }
      // The blur and downsample inside the bloom chain, for the constants readout only (the last of each wins).
      if (g_in_bloom_chain && (pixel_shader_hash == blur_pixel_shader || pixel_shader_hash == downsample_pixel_shader))
      {
         CopyBoundPSConstantBuffer(native_device, native_device_context, 4, pixel_shader_hash == blur_pixel_shader ? game_device_data.bloom_blur_vc4_cb : game_device_data.bloom_downsample_vc4_cb);
         return DrawOrDispatchOverrideType::None;
      }
#endif
      if (g_gtao_enable && pixel_shader_hash == ssao_singleframe_calculate_pixel_shader)
      {
         // Decided on the frame's first draw, so the four AO channels never mix XeGTAO and native.
         if (std::exchange(game_device_data.gtao_tried_this_frame, true))
            return game_device_data.gtao_ran_this_frame ? DrawOrDispatchOverrideType::Skip : DrawOrDispatchOverrideType::None;
         game_device_data.gtao_ran_this_frame = RunXeGTAO(native_device, native_device_context, device_data);
         return game_device_data.gtao_ran_this_frame ? DrawOrDispatchOverrideType::Replaced : DrawOrDispatchOverrideType::None;
      }
      if (std::ranges::contains(tonemap_pixel_shaders, pixel_shader_hash, &std::pair<uint32_t, const char*>::first))
      {
#if DEVELOPMENT
         g_final_perm = pixel_shader_hash;
         g_finals_this_frame++;
#endif
         if (g_luma_msaa_enable) // only the weighted resolve reads it
            CopyBoundPSConstantBuffer(native_device, native_device_context, 4, game_device_data.composite_tint_cb);

         // Luma bloom from the final's own scene input (t0), bound over the game's Final_bloom (t6). The native chain
         // still runs and is simply not read. The prefilter replays the native brightpass and combine on their own
         // constants (copies bound at b0/b4/b5/b6; DrawBloom itself binds only b11). The state stack gives the final back
         // its cbuffers, RT and SRVs.
         if (g_luma_bloom_enable && pixel_shader_hash != tonemap_no_post_pixel_shader && game_device_data.bloom_source_downsample_vc4_cb && game_device_data.bloom_brightpass_vc0_cb && game_device_data.bloom_brightpass_vc4_cb && game_device_data.bloom_combine_vc4_cb)
         {
            com_ptr<ID3D11ShaderResourceView> scene_srv;
            native_device_context->PSGetShaderResources(0, 1, &scene_srv);
            const std::shared_lock lock_shader_objects(s_mutex_shader_objects);
            if (scene_srv && HasShaders(device_data.native_vertex_shaders, "Bloom VS"_h) && HasShaders(device_data.native_pixel_shaders, "Bloom Prefilter PS"_h, "Bloom Downsample PS"_h, "Bloom Upsample PS"_h))
            {
               DrawStateStack<DrawStateStackType::FullGraphics> bloom_state;
               bloom_state.Cache(native_device_context, device_data.uav_max_count);
               ID3D11Buffer* const brightpass_vc0 = game_device_data.bloom_brightpass_vc0_cb.get();
               ID3D11Buffer* const tints_vc4[3] = {game_device_data.bloom_brightpass_vc4_cb.get(), game_device_data.bloom_combine_vc4_cb.get(), game_device_data.bloom_source_downsample_vc4_cb.get()};
               native_device_context->PSSetConstantBuffers(0, 1, &brightpass_vc0);
               native_device_context->PSSetConstantBuffers(4, 3, tints_vc4);
               com_ptr<ID3D11ShaderResourceView> bloom_srv;
               // No Karis average: the native source is a plain box, and its 1 / (1 + luminance) weight on this unexposed scene
               // would dim small bright sources about 4x harder than on screen.
               DrawBloom(native_device, native_device_context, device_data, scene_srv.get(), kBloomMips, kBloomSigmas, &bloom_srv);
               bloom_state.Restore(native_device_context);
               if (bloom_srv)
               {
                  ID3D11ShaderResourceView* const bloom = bloom_srv.get();
                  native_device_context->PSSetShaderResources(6, 1, &bloom);
               }
            }
         }

         if (g_smaa_enable && original_draw_dispatch_func != nullptr)
            return DrawTonemapWithSMAA(native_device, native_device_context, cmd_list_data, device_data, updated_cbuffers, *original_draw_dispatch_func);
         return DrawOrDispatchOverrideType::None;
      }
      // Hide the UI: drop its draws, but only those onto the swapchain, so any off-screen use of the same shaders survives.
      if (g_hide_ui && ui_pixel_shaders.contains(pixel_shader_hash))
      {
         com_ptr<ID3D11RenderTargetView> rtv;
         native_device_context->OMGetRenderTargets(1, &rtv, nullptr);
         com_ptr<ID3D11Resource> rtv_resource;
         if (rtv)
            rtv->GetResource(&rtv_resource);
         if (rtv_resource)
         {
            const std::shared_lock lock(device_data.mutex);
            if (device_data.back_buffers.contains(reinterpret_cast<uint64_t>(rtv_resource.get())))
               return DrawOrDispatchOverrideType::Replaced;
         }
         return DrawOrDispatchOverrideType::None;
      }

      // Alpha-tested material draws into the MSAA scene get alpha to coverage, with the sharpened alpha above. Only opaque ones
      // (blending off or One/Zero, as foliage): the same shaders also draw alpha-blended decals and windows, whose alpha is their
      // opacity. The game's blend state and shader are handed back after the draw.
      if (!g_luma_msaa_enable || is_custom_pass || original_draw_dispatch_func == nullptr || (stages & reshade::api::shader_stage::pixel) == 0 || !alpha_test_material_pixel_shaders.contains(pixel_shader_hash))
         return DrawOrDispatchOverrideType::None;
      // Without the sharpened-alpha clone (patch rejected or unloaded), coverage from the raw alpha would thin foliage out.
      if (!cmd_list_data.patch_clone_handles.contains(pixel_shader_hash))
         return DrawOrDispatchOverrideType::None;

      com_ptr<ID3D11RenderTargetView> rtv;
      native_device_context->OMGetRenderTargets(1, &rtv, nullptr);
      if (!rtv)
         return DrawOrDispatchOverrideType::None;
      // 4x+ only: 2x gives just two coverage levels, a coarser edge than vanilla's alpha test.
      com_ptr<ID3D11Resource> rtv_resource;
      rtv->GetResource(&rtv_resource);
      com_ptr<ID3D11Texture2D> rtv_texture;
      if (FAILED(rtv_resource->QueryInterface(&rtv_texture)))
         return DrawOrDispatchOverrideType::None;
      D3D11_TEXTURE2D_DESC rtv_texture_desc;
      rtv_texture->GetDesc(&rtv_texture_desc);
      if (rtv_texture_desc.SampleDesc.Count < 4)
         return DrawOrDispatchOverrideType::None;

      com_ptr<ID3D11BlendState> blend_state;
      FLOAT blend_factor[4];
      UINT sample_mask;
      native_device_context->OMGetBlendState(&blend_state, blend_factor, &sample_mask);
      D3D11_BLEND_DESC blend_desc = CD3D11_BLEND_DESC(D3D11_DEFAULT);
      if (blend_state)
         blend_state->GetDesc(&blend_desc);
      if (!IsRTBlendDisabled(blend_desc.RenderTarget[0]))
         return DrawOrDispatchOverrideType::None;
      blend_desc.AlphaToCoverageEnable = TRUE;
      // D3D11 hands back the existing object for a desc it has already seen, so this is a lookup after the first time.
      com_ptr<ID3D11BlendState> a2c_blend_state;
      if (FAILED(native_device->CreateBlendState(&blend_desc, &a2c_blend_state)))
         return DrawOrDispatchOverrideType::None;

      cmd_list_data.UseShaderVariant(native_device_context, pixel_shader_hash, reshade::api::shader_stage::pixel, Shader::ShaderVariant::Patched);
      native_device_context->OMSetBlendState(a2c_blend_state.get(), blend_factor, sample_mask);
      (*original_draw_dispatch_func)();
      native_device_context->OMSetBlendState(blend_state.get(), blend_factor, sample_mask);
      cmd_list_data.UseShaderVariant(native_device_context, pixel_shader_hash, reshade::api::shader_stage::pixel, Shader::ShaderVariant::Original);
      return DrawOrDispatchOverrideType::Replaced;
   }

   // The weighted resolve reads the samples of the FP16 MSAA scene target, which the game only ever resolves and may
   // have created without a shader resource bind.
   static bool OnCreateResource(reshade::api::device* device, reshade::api::resource_desc& desc, reshade::api::subresource_data* initial_data, reshade::api::resource_usage initial_state)
   {
      if (desc.type != reshade::api::resource_type::texture_2d || desc.texture.samples <= 1 || (desc.usage & reshade::api::resource_usage::render_target) == 0)
         return false;
      if (desc.texture.format != reshade::api::format::r16g16b16a16_float && desc.texture.format != reshade::api::format::r16g16b16a16_typeless)
         return false;
      desc.usage |= reshade::api::resource_usage::shader_resource;
      return true;
   }

   // The game's one ResolveSubresource (grab_scene_color) resolves the FP16 MSAA scene before the tonemap: draw
   // Luma_SR4_MSAAResolve instead. Anything else, or a target we cannot bind, keeps the hardware resolve.
   static bool OnResolveTextureRegion(reshade::api::command_list* cmd_list, reshade::api::resource source, uint32_t source_subresource, const reshade::api::subresource_box* source_box, reshade::api::resource dest, uint32_t dest_subresource, uint32_t dest_x, uint32_t dest_y, uint32_t dest_z, reshade::api::format format)
   {
      if (!g_luma_msaa_enable || format != reshade::api::format::r16g16b16a16_float || source_subresource != 0 || dest_subresource != 0 || source_box != nullptr || dest_x != 0 || dest_y != 0 || dest_z != 0)
         return false;

      com_ptr<ID3D11Texture2D> source_texture;
      com_ptr<ID3D11Texture2D> dest_texture;
      if (FAILED(reinterpret_cast<ID3D11Resource*>(source.handle)->QueryInterface(&source_texture)) || FAILED(reinterpret_cast<ID3D11Resource*>(dest.handle)->QueryInterface(&dest_texture)))
         return false;
      D3D11_TEXTURE2D_DESC source_desc;
      source_texture->GetDesc(&source_desc);
      D3D11_TEXTURE2D_DESC dest_desc;
      dest_texture->GetDesc(&dest_desc);
      if (source_desc.SampleDesc.Count <= 1 || source_desc.ArraySize != 1 || (source_desc.BindFlags & D3D11_BIND_SHADER_RESOURCE) == 0 || (dest_desc.BindFlags & D3D11_BIND_RENDER_TARGET) == 0)
         return false;

      DeviceData& device_data = *cmd_list->get_device()->get_private_data<DeviceData>();
      auto& game_device_data = GetGameDeviceData(device_data);
      // Held through the draw so a shader reload cannot release them mid-use.
      const std::shared_lock lock_shader_objects(s_mutex_shader_objects);
      if (!HasShaders(device_data.native_vertex_shaders, "Copy VS"_h) || !HasShaders(device_data.native_pixel_shaders, "SR4 MSAA Resolve PS"_h))
         return false;

      ID3D11Device* native_device = (ID3D11Device*)(cmd_list->get_device()->get_native());
      ID3D11DeviceContext* native_device_context = (ID3D11DeviceContext*)(cmd_list->get_native());

      // ponytail: views are created per resolve (one or two a frame); cache them per resource if it ever shows in a profile.
      D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
      srv_desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
      srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DMS;
      D3D11_RENDER_TARGET_VIEW_DESC rtv_desc = {};
      rtv_desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
      rtv_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
      com_ptr<ID3D11ShaderResourceView> source_srv;
      com_ptr<ID3D11RenderTargetView> dest_rtv;
      if (FAILED(native_device->CreateShaderResourceView(source_texture.get(), &srv_desc, &source_srv)) || FAILED(native_device->CreateRenderTargetView(dest_texture.get(), &rtv_desc, &dest_rtv)))
         return false;

      DrawStateStack<DrawStateStackType::FullGraphics> draw_state_stack;
      draw_state_stack.Cache(native_device_context, device_data.uav_max_count);
      // The game resolves with the MSAA target still bound for output, and DX11 refuses to also bind it as a shader resource.
      native_device_context->OMSetRenderTargetsAndUnorderedAccessViews(0, nullptr, nullptr, 0, 0, nullptr, nullptr);
      ID3D11Buffer* const tint_cb = game_device_data.composite_tint_cb.get();
      native_device_context->PSSetConstantBuffers(4, 1, &tint_cb);
      DrawCustomPixelShader(native_device_context, device_data.default_depth_stencil_state.get(), device_data.default_blend_state.get(), nullptr, device_data.native_vertex_shaders.at("Copy VS"_h).get(), device_data.native_pixel_shaders.at("SR4 MSAA Resolve PS"_h).get(), source_srv.get(), dest_rtv.get(), dest_desc.Width, dest_desc.Height);
      draw_state_stack.Restore(native_device_context);
#if DEVELOPMENT
      g_msaa_resolves_this_frame++;
#endif
      return true;
   }

   void OnInit(bool async) override
   {
      std::vector<ShaderDefineData> game_shader_defines_data = {
         {"TONEMAP_TYPE", '1', true, false, "0 - Vanilla SDR\n1 - Luma HDR (Vanilla+)", 1},
         {"XE_GTAO_QUALITY", '3', true, false, "XeGTAO quality (slice count)\n0 - Low\n1 - Medium\n2 - High\n3 - Very High\n4 - Ultra", 4},
      };
      shader_defines_data.append_range(game_shader_defines_data);
      assert(shader_defines_data.size() < MAX_SHADER_DEFINES);

      // The tonemap writes gamma-encoded output and DoF and UI keep running on it after,
      // so stay in gamma space on float buffers and linearize in the final composition.
      GetShaderDefineData(POST_PROCESS_SPACE_TYPE_HASH).SetDefaultValue('0');
      GetShaderDefineData(EARLY_DISPLAY_ENCODING_HASH).SetDefaultValue('0');
      // The no-LUT finals end in pow(1/2.2).
      GetShaderDefineData(VANILLA_ENCODING_TYPE_HASH).SetDefaultValue('1');
      GetShaderDefineData(GAMMA_CORRECTION_TYPE_HASH).SetDefaultValue('1');
      // The rl_hdr finals and the Bink video pre-scale the scene by GamePaperWhite/UIPaperWhite, the rl_prim_2d UI stays vanilla.
      GetShaderDefineData(UI_DRAW_TYPE_HASH).SetDefaultValue('2');

      // The game binds cb0-cb8.
      luma_settings_cbuffer_index = 13;
      luma_data_cbuffer_index = 12;

      // Manual paper-white sliders, not the OS reference level. Core gates the UI one on UI_DRAW_TYPE >= 1.
      use_os_reference_white_level = false;

      default_luma_global_game_settings.Dithering = 1.f;
      default_luma_global_game_settings.VideoAutoHDREnable = 1.f;
      default_luma_global_game_settings.VideoAutoHDRBoost = 0.5f; // peak ~165 nits
      default_luma_global_game_settings.BloomIntensity = 1.f;
      default_luma_global_game_settings.Exposure = 1.f;
      default_luma_global_game_settings.Saturation = 1.f;
      default_luma_global_game_settings.ColorGradingIntensity = 1.f;
      default_luma_global_game_settings.Contrast = 1.f;
      default_luma_global_game_settings.HighlightDechroma = 0.f;
      cb_luma_global_settings.GameSettings = default_luma_global_game_settings;

      native_shaders_definitions.emplace(CompileTimeStringHash("SR4 MSAA Resolve PS"), ShaderDefinition{"Luma_SR4_MSAAResolve", reshade::api::pipeline_subobject_type::pixel_shader});
      native_shaders_definitions.emplace(CompileTimeStringHash("SR4 SMAA Linearize CS"), ShaderDefinition{"Luma_SR4_SMAALinearize", reshade::api::pipeline_subobject_type::compute_shader});
      native_shaders_definitions.emplace(CompileTimeStringHash("SR4 SMAA Predication CS"), ShaderDefinition{"Luma_SR4_SMAAPredication", reshade::api::pipeline_subobject_type::compute_shader});
      native_shaders_definitions.emplace(CompileTimeStringHash("SR4 Sharpen PS"), ShaderDefinition{"Luma_SR4_Sharpen", reshade::api::pipeline_subobject_type::pixel_shader, nullptr, "sharpen_ps"});
      // XeGTAO passes (Luma_SR4_XeGTAO.hlsl); the two denoisers differ only by XE_GTAO_FINAL_APPLY.
      native_shaders_definitions.emplace(CompileTimeStringHash("SR4 XeGTAO Prefilter Depths CS"), ShaderDefinition{"Luma_SR4_XeGTAO", reshade::api::pipeline_subobject_type::compute_shader, nullptr, "prefilter_depths16x16_cs"});
      native_shaders_definitions.emplace(CompileTimeStringHash("SR4 XeGTAO Main Pass CS"), ShaderDefinition{"Luma_SR4_XeGTAO", reshade::api::pipeline_subobject_type::compute_shader, nullptr, "main_pass_cs"});
      native_shaders_definitions.emplace(CompileTimeStringHash("SR4 XeGTAO Denoise Pass 1 CS"), ShaderDefinition{"Luma_SR4_XeGTAO", reshade::api::pipeline_subobject_type::compute_shader, nullptr, "denoise_pass_cs", {{"XE_GTAO_FINAL_APPLY", "0"}}});
      native_shaders_definitions.emplace(CompileTimeStringHash("SR4 XeGTAO Denoise Pass 2 CS"), ShaderDefinition{"Luma_SR4_XeGTAO", reshade::api::pipeline_subobject_type::compute_shader, nullptr, "denoise_pass_cs", {{"XE_GTAO_FINAL_APPLY", "1"}}});
      reshade::register_event<reshade::addon_event::create_resource>(OnCreateResource);
      reshade::register_event<reshade::addon_event::resolve_texture_region>(OnResolveTextureRegion);
   }

   void LoadConfigs() override
   {
      reshade::get_config_value(nullptr, NAME, "MSAAResolveEnable", g_luma_msaa_enable);
      reshade::get_config_value(nullptr, NAME, "SMAAEnable", g_smaa_enable);
      reshade::get_config_value(nullptr, NAME, "RCASSharpness", g_rcas_sharpness);
      reshade::get_config_value(nullptr, NAME, "BloomEnable", g_luma_bloom_enable);
      reshade::get_config_value(nullptr, NAME, "GTAOEnable", g_gtao_enable);
      auto& gs = cb_luma_global_settings.GameSettings;
      reshade::get_config_value(nullptr, NAME, "Dithering", gs.Dithering);
      reshade::get_config_value(nullptr, NAME, "VideoAutoHDREnable", gs.VideoAutoHDREnable);
      reshade::get_config_value(nullptr, NAME, "VideoAutoHDRBoost", gs.VideoAutoHDRBoost);
      reshade::get_config_value(nullptr, NAME, "BloomIntensity", gs.BloomIntensity);
      reshade::get_config_value(nullptr, NAME, "Exposure", gs.Exposure);
      reshade::get_config_value(nullptr, NAME, "Saturation", gs.Saturation);
      reshade::get_config_value(nullptr, NAME, "ColorGradingIntensity", gs.ColorGradingIntensity);
      reshade::get_config_value(nullptr, NAME, "Contrast", gs.Contrast);
      reshade::get_config_value(nullptr, NAME, "HighlightsDesaturation", gs.HighlightDechroma);
   }

   void DrawImGuiSettings(DeviceData& device_data) override
   {
      auto& gs = cb_luma_global_settings.GameSettings;

      // Persisted controls with their tooltip. The GameSettings ones (toggle, slider) also mark the Luma cbuffer for upload.
      const auto config_checkbox = [](const char* label, const char* key, bool* value, const char* tooltip)
      {
         if (ImGui::Checkbox(label, value))
            reshade::set_config_value(nullptr, NAME, key, *value);
         if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", tooltip);
      };
      const auto settings_toggle = [&](const char* label, const char* key, float* value, const char* tooltip)
      {
         bool enabled = *value > 0.5f;
         if (ImGui::Checkbox(label, &enabled))
         {
            *value = enabled ? 1.f : 0.f;
            reshade::set_config_value(nullptr, NAME, key, *value);
            device_data.cb_luma_global_settings_dirty = true;
         }
         if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", tooltip);
         return enabled;
      };
      const auto slider = [&](const char* label, const char* key, float* value, float default_value, float max_value, const char* tooltip)
      {
         if (ImGui::SliderFloat(label, value, 0.f, max_value))
            device_data.cb_luma_global_settings_dirty = true;
         if (ImGui::IsItemDeactivatedAfterEdit())
            reshade::set_config_value(nullptr, NAME, key, *value);
         if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip("%s", tooltip);
         if (DrawResetButton(*value, default_value, key))
            device_data.cb_luma_global_settings_dirty = true;
      };

      ImGui::SeparatorText("Anti-Aliasing");

      config_checkbox("Luma MSAA Enable", "MSAAResolveEnable", &g_luma_msaa_enable, "Replaces the game's MSAA with an HDR-aware one (smoother edges on bright lights, sky, grass, foliage and fences; requires Anti-Aliasing enabled in the game's display settings).");
      config_checkbox("SMAA Enable", "SMAAEnable", &g_smaa_enable, "Adds SMAA anti-aliasing on top of the game's own (most noticeable with Anti-Aliasing off or low in the game's display settings).");

      ImGui::BeginDisabled(!g_smaa_enable);
      slider("RCAS Sharpness", "RCASSharpness", &g_rcas_sharpness, 0.f, 1.f, "Sharpening applied on top of SMAA (0 = off).");
#if DEVELOPMENT
      ImGui::Checkbox("SMAA Predication", &g_smaa_predication);
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Finds edges by geometry (the game's scene depth) as well as by brightness.\nKeeps textures sharp while still antialiasing real silhouettes.");
      ImGui::Checkbox("SMAA Edges Debug View", &g_smaa_edges_debug);
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Show the edges SMAA smooths (red = horizontal, green = vertical) instead of the frame.\nToggle SMAA Predication to compare: texture detail should lose edges, silhouettes keep them.");
#endif
      ImGui::EndDisabled();

      ImGui::SeparatorText("Grade");

      slider("Exposure", "Exposure", &gs.Exposure, default_luma_global_game_settings.Exposure, 2.f, "Overall image brightness (1 = vanilla).");
      slider("Contrast", "Contrast", &gs.Contrast, default_luma_global_game_settings.Contrast, 2.f, "Overall image contrast, HDR only (1 = vanilla).");
      slider("Saturation", "Saturation", &gs.Saturation, default_luma_global_game_settings.Saturation, 2.f, "Color saturation, HDR only (1 = vanilla).");
      slider("Highlights Desaturation", "HighlightsDesaturation", &gs.HighlightDechroma, default_luma_global_game_settings.HighlightDechroma, 1.f, "How far the brightest sources fade to neutral white, HDR only (0 = keep color at any brightness).");
      slider("Color Grading Intensity", "ColorGradingIntensity", &gs.ColorGradingIntensity, default_luma_global_game_settings.ColorGradingIntensity, 1.f, "Strength of the game's own color grading (1 = vanilla, 0 = neutral).");

      ImGui::SeparatorText("Bloom");

      config_checkbox("Luma Bloom Enable", "BloomEnable", &g_luma_bloom_enable, "Replaces the game's bloom with a wider, softer HDR bloom.");
      slider("Bloom Intensity", "BloomIntensity", &gs.BloomIntensity, default_luma_global_game_settings.BloomIntensity, 2.f, "Bloom strength (1 = vanilla, 0 = none).");

      ImGui::SeparatorText("Ambient Occlusion");

      config_checkbox("XeGTAO Enable", "GTAOEnable", &g_gtao_enable, "Replaces the game's SSAO with XeGTAO (cleaner, more accurate ambient occlusion; requires Ambient Occlusion set to Medium or High in the game's display settings).");
#if DEVELOPMENT || TEST
      ImGui::BeginDisabled(!g_gtao_enable);
      ImGui::SliderFloat("GTAO Final Value Power", &g_gtao_final_value_power, 0.3f, 4.5f, "%.2f");
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
         ImGui::SetTooltip("Primary darkness dial (higher = darker AO). 2.2 matches the native SSAO's coverage and mean darkening.");
      ImGui::SliderFloat("GTAO Radius Override", &g_gtao_radius_override, 0.f, 5.f, "%.3f");
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
         ImGui::SetTooltip("0 = the shader's EFFECT_RADIUS (0.5 m); > 0 overrides it, in metres.");
#if DEVELOPMENT // the shader's debug blocks exist in DEVELOPMENT only
      ImGui::Combo("GTAO Debug View", &g_gtao_debug_view, "Off\0Depth gradient\0Normals\0AO x8\0Edges\0");
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
         ImGui::SetTooltip("Draws diagnostics through the game's SSAO apply (multiplied into the lighting). Depth gradient flat or blocky = wrong input;\nNormals: camera-facing surfaces bright, black everywhere = NORMAL_Z_SIGN inverted; AO x8 = spot broad over-occlusion.");
#endif
      ImGui::EndDisabled();
#endif

      ImGui::SeparatorText("Effects");

      ImGui::BeginDisabled(!settings_toggle("Video AutoHDR", "VideoAutoHDREnable", &gs.VideoAutoHDREnable, "Adds HDR highlights to pre-rendered videos (HDR only)."));
      slider("Video HDR Boost", "VideoAutoHDRBoost", &gs.VideoAutoHDRBoost, default_luma_global_game_settings.VideoAutoHDRBoost, 1.f, "Video highlight strength (0 = off).");
      ImGui::EndDisabled();

      settings_toggle("Dithering", "Dithering", &gs.Dithering, "Reduces gradient banding.");

      ImGui::SeparatorText("UI");

      ImGui::Checkbox("Hide Gameplay UI", &g_hide_ui);
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Disables the in-game UI.");
   }

   void OnPresent(ID3D11Device* native_device, DeviceData& device_data) override
   {
      auto& game_device_data = GetGameDeviceData(device_data);
      game_device_data.bloom_source_downsampled = false;
      game_device_data.gtao_tried_this_frame = false;
      game_device_data.gtao_ran_this_frame = false;
      // Turning XeGTAO off gives its scratch back; it is rebuilt on demand.
      if (!g_gtao_enable && game_device_data.gtao_width != 0)
         game_device_data.ReleaseGTAOScratch();
#if DEVELOPMENT
      g_msaa_resolves_last_frame = std::exchange(g_msaa_resolves_this_frame, 0);
      g_finals_last_frame = std::exchange(g_finals_this_frame, 0);

      com_ptr<ID3D11DeviceContext> native_device_context;
      native_device->GetImmediateContext(&native_device_context);
      for (auto& readout : game_device_data.bloom_readouts)
      {
         D3D11_MAPPED_SUBRESOURCE mapped;
         if (readout.staging && SUCCEEDED(native_device_context->Map(readout.staging.get(), 0, D3D11_MAP_READ, D3D11_MAP_FLAG_DO_NOT_WAIT, &mapped)))
         {
            std::memcpy(readout.value, static_cast<const float*>(mapped.pData) + readout.register_index * 4, sizeof(readout.value));
            native_device_context->Unmap(readout.staging.get(), 0);
         }
         const com_ptr<ID3D11Buffer>& source = game_device_data.*readout.source;
         if (!source)
            continue;
         D3D11_BUFFER_DESC desc;
         source->GetDesc(&desc);
         if (desc.ByteWidth < (readout.register_index + 1) * 16)
            continue;
         if (!readout.staging)
         {
            desc.Usage = D3D11_USAGE_STAGING;
            desc.BindFlags = 0;
            desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            native_device->CreateBuffer(&desc, nullptr, &readout.staging);
         }
         if (readout.staging)
            native_device_context->CopyResource(readout.staging.get(), source.get());
      }

      g_ssao_calculate_draws_last_frame = std::exchange(g_ssao_calculate_draws_this_frame, 0);
      g_ssao_frames_since_log++;
      D3D11_MAPPED_SUBRESOURCE mapped;
      if (g_ssao_staging_perm != 0 && SUCCEEDED(native_device_context->Map(game_device_data.ssao_vc0_staging.get(), 0, D3D11_MAP_READ, D3D11_MAP_FLAG_DO_NOT_WAIT, &mapped)))
      {
         D3D11_BUFFER_DESC desc;
         game_device_data.ssao_vc0_staging->GetDesc(&desc);
         std::memset(g_ssao_vc0, 0, sizeof(g_ssao_vc0));
         std::memcpy(g_ssao_vc0, mapped.pData, (std::min)(size_t(desc.ByteWidth), sizeof(g_ssao_vc0)));
         native_device_context->Unmap(game_device_data.ssao_vc0_staging.get(), 0);
         g_ssao_perm = std::exchange(g_ssao_staging_perm, 0);

         const auto& c = g_ssao_vc0;
         const bool multiframe = g_ssao_perm == ssao_multiframe_calculate_pixel_shader;
         int length = std::snprintf(g_ssao_readout, sizeof(g_ssao_readout),
            "SR4 SSAO vc0 (%s 0x%08X): fade %g, radius_reference %g, projection_scales %g %g, temporal_falloff %g %g, hFOV %.3f vFOV %.3f deg, view z at depth 0 / 1: %g / %g\n"
            "  inv_proj c1 %g %g %g %g | c2 %g %g %g %g | c3 %g %g %g %g | c4 %g %g %g %g",
            multiframe ? "multiframe" : "singleframe", g_ssao_perm, c[0][0], multiframe ? c[14][0] : c[18][0], c[5][0], c[5][1], multiframe ? c[19][0] : 0.f, multiframe ? c[19][1] : 0.f,
            2.f * std::atan(c[1][0]) * 57.29578f, 2.f * std::atan(c[2][1]) * 57.29578f, c[4][2] / c[4][3], (c[3][2] + c[4][2]) / (c[3][3] + c[4][3]),
            c[1][0], c[1][1], c[1][2], c[1][3], c[2][0], c[2][1], c[2][2], c[2][3], c[3][0], c[3][1], c[3][2], c[3][3], c[4][0], c[4][1], c[4][2], c[4][3]);
         // Sample offsets, .xy used (UV = offset * radius): interleave c6-c9 (this is the first draw's set), blue noise c10+.
         for (int i = 6; i < (multiframe ? 14 : 18); i++)
            length += std::snprintf(g_ssao_readout + length, sizeof(g_ssao_readout) - length, "%s%s c%d %g %g (|%g|)", i == 6 || i == 10 ? "\n  " : "", i == 6 ? "interleave" : (i == 10 ? "blue_noise" : ""), i, c[i][0], c[i][1], std::sqrt(c[i][0] * c[i][0] + c[i][1] * c[i][1]));

         // Logged on change only, at most every 30 frames: the FOV animates and the multiframe offsets alternate per frame,
         // so this samples the values rather than logging every change.
         if (g_ssao_frames_since_log >= 30 && std::memcmp(g_ssao_vc0, g_ssao_vc0_logged, sizeof(g_ssao_vc0)) != 0)
         {
            std::memcpy(g_ssao_vc0_logged, g_ssao_vc0, sizeof(g_ssao_vc0));
            g_ssao_frames_since_log = 0;
            reshade::log::message(reshade::log::level::info, g_ssao_readout);
         }
      }
      const uint32_t ssao_perm_copied = std::exchange(g_ssao_perm_this_frame, 0);
      if (ssao_perm_copied != 0 && game_device_data.ssao_vc0_cb)
      {
         D3D11_BUFFER_DESC desc;
         game_device_data.ssao_vc0_cb->GetDesc(&desc);
         D3D11_BUFFER_DESC staging_desc = {};
         if (game_device_data.ssao_vc0_staging)
            game_device_data.ssao_vc0_staging->GetDesc(&staging_desc);
         if (staging_desc.ByteWidth != desc.ByteWidth)
         {
            game_device_data.ssao_vc0_staging.reset();
            desc.Usage = D3D11_USAGE_STAGING;
            desc.BindFlags = 0;
            desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            native_device->CreateBuffer(&desc, nullptr, &game_device_data.ssao_vc0_staging);
         }
         if (game_device_data.ssao_vc0_staging)
         {
            native_device_context->CopyResource(game_device_data.ssao_vc0_staging.get(), game_device_data.ssao_vc0_cb.get());
            g_ssao_staging_perm = ssao_perm_copied;
         }
      }
#endif
   }

#if DEVELOPMENT
   void DrawImGuiDevSettings(DeviceData& device_data) override
   {
      auto& game_device_data = GetGameDeviceData(device_data);
      ImGui::Text("Luma MSAA resolves last frame: %u", g_msaa_resolves_last_frame);

      ImGui::SeparatorText("Final composite DEV readout");
      if (g_final_perm == 0)
         ImGui::Text("no rl_hdr final seen yet");
      else
      {
         ImGui::Text("perm 0x%08X %s  (draws last frame: %u)", g_final_perm, std::ranges::find(tonemap_pixel_shaders, g_final_perm, &std::pair<uint32_t, const char*>::first)->second, g_finals_last_frame);
         if (ImGui::IsItemHovered())
            ImGui::SetTooltip("The rl_hdr final the game drew last; its HDR path is in the matching Tonemap*_0x<hash> shader. Two draws in a frame mean two finals (e.g. a menu over the scene).");
      }

      ImGui::SeparatorText("Bloom DEV readout");
      for (const auto& readout : game_device_data.bloom_readouts)
         ImGui::Text("%s: %.4f %.4f %.4f %.4f", readout.name, readout.value[0], readout.value[1], readout.value[2], readout.value[3]);

      ImGui::SeparatorText("SSAO DEV readout");
      ImGui::Text("calculate draws last frame: %u", g_ssao_calculate_draws_last_frame);
      ImGui::TextUnformatted(g_ssao_perm == 0 ? "no SSAO calculate seen yet" : g_ssao_readout);
   }
#endif

   void PrintImGuiAbout() override
   {
      ImGui::PushTextWrapPos(0.f);
      ImGui::Text(
         "Luma for \"Saints Row IV: Re-Elected\" is developed by DristoforColumb and is open source and free.\n"
         "It adds HDR, HDR bloom and SMAA anti-aliasing, and replaces the game's SSAO with XeGTAO.\n"
         "Set Ambient Occlusion to Medium or High in the game's display settings for XeGTAO to apply; SMAA works either way.\n"
         "Do NOT run another HDR mod (e.g. RenoDX) alongside it.\n"
         "Thanks to the Luma team and contributors.\n"
         "If you enjoy it, consider donating.");
      ImGui::PopTextWrapPos();

      ImGui::NewLine();
      ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(70, 134, 0, 255));
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(70 + 9, 134 + 9, 0, 255));
      ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(70 + 18, 134 + 18, 0, 255));
      static const std::string donation_link = std::string("Buy DristoforColumb a Coffee on ko-fi ") + std::string(ICON_FK_OK);
      if (ImGui::Button(donation_link.c_str()))
         ShellExecuteA(nullptr, "open", "https://ko-fi.com/dristoforcolumb", nullptr, nullptr, SW_SHOWNORMAL);
      ImGui::PopStyleColor(3);

      ImGui::NewLine();
      static const std::string social_link = std::string("Join our \"HDR Den\" Discord ") + std::string(ICON_FK_SEARCH);
      if (ImGui::Button(social_link.c_str()))
      {
         // Unique link for Luma's HDR Den (tracks the origin of people joining); do not share for other purposes.
         static const std::string discord_link = std::string("https://discord.gg/J9fM") + std::string("3EVuEZ");
         ShellExecuteA(nullptr, "open", discord_link.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
      }
      static const std::string contributing_link = std::string("Contribute on Github ") + std::string(ICON_FK_FILE_CODE);
      if (ImGui::Button(contributing_link.c_str()))
         ShellExecuteA(nullptr, "open", "https://github.com/Filoppi/Luma-Framework", nullptr, nullptr, SW_SHOWNORMAL);

      ImGui::NewLine();
      ImGui::Text("Build Date: %s %s", __DATE__, __TIME__);

      ImGui::NewLine();
      ImGui::Text("Credits:"
                  "\n\nMain:"
                  "\nDristoforColumb"
                  "\n\nThird Party:"
                  "\nReShade"
                  "\nImGui"
                  "\nRenoDX (HDR tonemap method)"
                  "\nDICE (HDR tonemapper)"
                  "\nSMAA (Iryoku)"
                  "\nXeGTAO (Intel)"
                  "\nAMD FidelityFX (RCAS)");
   }
};

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
   if (ul_reason_for_call == DLL_PROCESS_ATTACH)
   {
      Globals::SetGlobals(PROJECT_NAME, "Saints Row IV: Re-Elected Luma mod", "", 1);
      Globals::DEVELOPMENT_STATE = Globals::ModDevelopmentState::Finished;

      // display.ini "Fullscreen = true" is exclusive fullscreen; Core blocks FSE, this makes the window borderless instead.
      force_borderless = true;

      swapchain_format_upgrade_type = TextureFormatUpgradesType::AllowedEnabled;
      swapchain_upgrade_type = SwapchainUpgradeType::scRGB;
      texture_format_upgrades_type = TextureFormatUpgradesType::AllowedEnabled;
      // The swapchain copy the PostProcess 2 diffusion DoF reads (swapchain-sized r8g8b8a8_unorm).
      texture_upgrade_formats = {
         reshade::api::format::r8g8b8a8_unorm,
      };
      // "No1Px": the game presents at 1x1 for a few frames while booting, which also matches the aspect ratio filter.
      texture_format_upgrades_2d_size_filters = 0 | (uint32_t)TextureFormatUpgrades2DSizeFilters::SwapchainResolution | (uint32_t)TextureFormatUpgrades2DSizeFilters::SwapchainAspectRatio | (uint32_t)TextureFormatUpgrades2DSizeFilters::No1Px;

      game = new SaintsRowIV();
   }

   else if (ul_reason_for_call == DLL_PROCESS_DETACH)
   {
      reshade::unregister_event<reshade::addon_event::create_resource>(SaintsRowIV::OnCreateResource);
      reshade::unregister_event<reshade::addon_event::resolve_texture_region>(SaintsRowIV::OnResolveTextureRegion);
   }

   CoreMain(hModule, ul_reason_for_call, lpReserved);

   return TRUE;
}
