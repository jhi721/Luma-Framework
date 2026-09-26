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
// The motion vector draw key reads the draw's arguments ("last_draw_dispatch_data")
#define ENABLE_DRAW_DISPATCH_DATA_CACHE 1

#include "..\..\Core\core.hpp"
#include "..\..\External\WDK\includes\d3d11TokenizedProgramFormat.hpp"
#include "MotionVectorPatches.h"

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
   // The scene's first post passes: god rays (their mask, when drawn), else that downsample. They end a motion vector frame.
   constexpr uint32_t god_rays_mask_pixel_shader = 0xDD8853F9;
   // Every pixel shader of the packfile's post process families (rl_hdr_prep, rl_god_rays, rl_distortion, rl_motion_blur,
   // rl_depth_of_field, rl_diffusion_depth_of_field, rl_downsample*, rl_filter_invalid_hdr, rl_custom_screen_fx, rl_lut_blend,
   // rl_hdr, rl_hdr_prince): whichever comes first after the material target's copy into the post scene ends the motion vector
   // frame (the upscaler then runs before it). Some also run mid scene (the downsamples), hence the copy condition.
   const std::unordered_set<uint32_t> post_process_pixel_shaders = {
      0x07BE3368,
      0xAB9CD83F,
      0x47FE8484,
      0xFC430199,
      0x5F9A6C58,
      0x8EFF90D5, // rl_custom_screen_fx
      0x6C8C1B18,
      0x9D995971, // rl_depth_of_field
      0x80CC0BE3,
      0xE68D094B,
      0xDF6B7D0A,
      0xCBF94F4E,
      0x3E6FB3F8,
      0xF76FA615, // rl_diffusion_depth_of_field
      0xF9A739DF,
      0x47272E90,
      0x54F83BC2,
      0xB8182724,
      0xB0FA89F4,
      0xE9E18958,
      0x8957630A, // rl_distortion
      0x7426FCE6,
      0x27064754,
      0x14B55F49,
      0x0EBB3B4F, // rl_downsample, _2x2, _fast
      0x6C9D7D5C, // rl_filter_invalid_hdr
      0x461390F2,
      0xDD8853F9, // rl_god_rays
      0xC235DDDD,
      0x9F1F6557,
      0xFEE7D6DC,
      0x966367E4,
      0xED6DDA24,
      0xD742C62C, // rl_hdr
      0xF03A6DC3,
      0x9113BBFC,
      0x3F43F01B,
      0xFFD2B4DB,
      0x52563AB4,
      0x12F9FD30, // rl_hdr_prep
      0x8ED48FDF,
      0x7DCC8A34,
      0xD501C191,
      0x8D1F6BBB,
      0x7FC325C0,
      0xE9664111, // rl_hdr_prince
      0xCD584570, // rl_lut_blend
      0xAE7986CC,
      0xA05B432B,
      0xA0ACCB08,
      0x36BEC2DE,
      0x4608446A, // rl_motion_blur
   };

   // Motion vectors for DLSS / FSR (the game renders none, see MotionVectorPatches.h): the main material pass redraws every object with
   // patched shaders into an extra target, the second run reading the object's previous frame vc2 / vc3.
#if DEVELOPMENT
   bool g_mv_enable = false;
   bool g_mv_debug_view = false;
   bool g_mv_force_jitter = false; // The projection jitter without an upscaler
#else
   constexpr bool g_mv_enable = false;
   constexpr bool g_mv_force_jitter = false;
#endif
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

   // Motion vector research (the game renders none): the frame probe logs the next frame's draws (pass state, the per-draw VS
   // constants vc2 / vc3 and a few vc2 values, vertex buffers, draw arguments), then how every buffer those draws read was filled.
   // Consumed at the next present.
   enum class ProbeRequest : int
   {
      None,
      Log,
      LogAndDump, // also writes every draw's vc2 / vc3 contents to a CSV next to the executable
   };
   std::atomic<ProbeRequest> g_probe_request = ProbeRequest::None;
   constexpr uint32_t probe_max_draw_lines = 12000;
   // The VS slots of the per-draw constants: vc2 (projTM c0-3, eyePos c4, objTM c16-18), vc3 (Bone_weights)
   constexpr UINT object_constants_slot = MotionVectorPatches::object_slot;
   constexpr UINT bone_constants_slot = MotionVectorPatches::previous_slots[1].first;
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
   // The game resolved an MSAA scene this frame / last frame (display.ini MSAA_Level, applied at the game's start)
   bool msaa_scene_resolved = false;
   bool msaa_scene = false;

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

   // Motion vectors: shaders patched on first use, by original hash (null on failure), the target (sized like the scene), and copies
   // of the material pass's independent blend states that also write it
   std::shared_mutex mv_mutex;
   std::unordered_map<uint32_t, com_ptr<ID3D11VertexShader>> mv_vertex_shaders;
   std::unordered_map<uint32_t, com_ptr<ID3D11PixelShader>> mv_pixel_shaders;
   com_ptr<ID3D11Texture2D> mv_texture;
   com_ptr<ID3D11RenderTargetView> mv_rtv;
   com_ptr<ID3D11UnorderedAccessView> mv_uav; // Null without typed UAV loads of R32G32_FLOAT (then no fill)
   // The frame's G-buffer depth (taken at the frame start): the upscaler's depth and the camera motion fill's (see
   // "Luma_SR4_MotionVectorFill.hlsl"; when the fill will run, the frame start marks the target FLT_MAX)
   com_ptr<ID3D11ShaderResourceView> mv_frame_depth;
   bool mv_fill_pending = false;
   com_ptr<ID3D11Buffer> mv_fill_buffer;
   // A frame starts at its first material draw (clearing the target) and ends at the first post pass. Only material draws after a
   // G-buffer draw and before that post pass qualify: later full-size opaque draws (3D HUD onto the fp16 swapchain) would restart it.
   bool mv_frame_ended = true;
   bool mv_scene_open = false;
   std::atomic<bool> mv_active = false; // Motion vectors and jitter this frame: an upscaler is active, or the DEV toggle (set at present)
   // The material pass target, from the previous frame: only draws into it get motion vectors. The lighting pass (light volume
   // meshes, 0x0FFC4B94) draws into a buffer of the same size and format. The game copies the material target into the first post
   // pass's t0 (forward draws go there), so it's that copy's source.
   com_ptr<ID3D11Resource> mv_scene_color;
   bool mv_scene_color_wanted = false; // Since the last G-buffer draw, until the first downsample
   // The last CopyResource since the G-buffer (not referenced, only compared)
   uint64_t mv_scene_copy_source = 0;
   uint64_t mv_scene_copy_dest = 0;
   bool mv_scene_copied = false; // This frame's material target copy happened
   // The projection jitter (pixels, +y down), chosen when the G-buffer opens the scene; its NDC offset is at VS
   // "MotionVectorPatches::jitter_slot" of every mesh draw depth tested against the scene until the first post pass
   std::array<float, 2> mv_jitter = {};
   std::array<float, 2> mv_jitter_ndc = {}; // The same offset in NDC (y up), as the jitter buffer holds it
   com_ptr<ID3D11Buffer> mv_jitter_buffer;
   // The patched vertex shaders that read vc3 (skinned), by original hash
   std::unordered_set<uint32_t> mv_bone_vertex_shaders;

   // CPU copies of the vc2 / vc3 buffers the motion vector draws bind (one shared buffer each, mapped with discard every few draws;
   // draws in between reuse the last contents), by buffer (an entry registers it, empty until its first Unmap): each Map's pointer,
   // copied at Unmap
   std::mutex mv_constants_mutex;
   std::unordered_map<uint64_t, void*> mv_mapped_constants;
   std::unordered_map<uint64_t, std::vector<uint8_t>> mv_constants_copies;
   std::vector<uint8_t> mv_camera_upload; // Scratch: an unmatched draw's vc2 with last frame's camera
   // The previous frame's vc2 / vc3 uploads (dynamic), by "MotionVectorPatches::previous_slots" index
   com_ptr<ID3D11Buffer> mv_previous_buffers[std::size(MotionVectorPatches::previous_slots)];
   // Motion vector draws by draw key (shaders, buffers, arguments), with the objTM translation and vc2 / vc3. A draw takes the
   // previous frame's constants of its key's nearest draw (same object, a frame earlier).
   struct MotionVectorObject
   {
      std::array<float, 3> translation;
      std::vector<uint8_t> object; // vc2
      std::vector<uint8_t> bones;  // vc3, skinned draws only
   };
   std::unordered_map<uint64_t, std::vector<MotionVectorObject>> mv_objects;
   std::unordered_map<uint64_t, std::vector<MotionVectorObject>> mv_previous_objects;
   // The frame's camera (vc2 projTM of its first draw) and the previous frame's
   std::array<float, 16> mv_camera = {};
   std::array<float, 16> mv_previous_camera = {};
   bool mv_camera_valid = false;
   bool mv_previous_camera_valid = false;
   uint32_t mv_frame_index = 0; // The Luma frame index of the last motion vector frame

#if DEVELOPMENT
   std::atomic<uint32_t> mv_draws = 0;
   std::atomic<uint32_t> mv_skipped_draws = 0;      // Material draws that couldn't get motion vectors (patch refused)
   std::atomic<uint32_t> mv_matched_draws = 0;      // Found last frame's own constants
   std::atomic<uint32_t> mv_other_camera_draws = 0; // Not matched, with a projTM other than the frame's camera: left as is
   std::atomic<uint32_t> mv_off_camera_draws = 0;   // Any, with a projTM other than the frame's camera
   std::atomic<uint32_t> mv_jitter_only_draws = 0;  // See "DrawWithJitter"
   std::atomic<uint32_t> mv_sr_draws = 0;           // Upscaler runs
   std::atomic<uint32_t> mv_sr_failures = 0;        // Upscaler failures (it's then suppressed until picked again)
   std::atomic<uint32_t> mv_sr_skips = 0;           // Upscaler frames skipped (no usable scene, depth or camera)
   uint32_t mv_frame_end_shader = 0;                // The post pass that ended the last motion vector frame
   // Depth tested scene sized draws left unjittered: scene closed (before the G-buffer or after post), no vertex buffer, VS refused
   static constexpr const char* mv_jitter_skip_names[] = {"scene_closed", "no_vertex_buffer", "vs_refused"};
   std::atomic<uint32_t> mv_jitter_skips[std::size(mv_jitter_skip_names)] = {};
   std::atomic<uint64_t> mv_jitter_skip_examples[std::size(mv_jitter_skip_names)] = {};
   uint64_t mv_camera_draw = 0;                 // VS << 32 | PS of the draw that set the frame's camera
   std::atomic<uint32_t> mv_uncopied_draws = 0; // No CPU copy of vc2 yet: zero motion
   // Draws that reached the motion vector path, and why the others were left untouched, per 60 frames, with the last VS/PS seen
   // for each reason
   std::atomic<uint32_t> mv_candidate_draws = 0;
   std::atomic<uint32_t> mv_frames = 0;          // Motion vector frames started
   std::atomic<uint32_t> mv_constant_copies = 0; // vc2 / vc3 CPU copies taken at Unmap
   std::atomic<uint32_t> mv_moved_draws = 0;     // Matched draws whose previous vc2 differs from the current one
   // A sample matched draw per 60 frames: largest |previous - current| over projTM and over objTM
   float mv_sample_camera_delta = -1.f;
   float mv_sample_object_delta = -1.f;
   // Largest |previous - current| over the frame camera's projTM in the last 60 frames
   float mv_camera_delta = 0.f;
   // The whole target, read back a frame late: largest |motion vector| (UV) and the share of non zero pixels
   com_ptr<ID3D11Texture2D> mv_readback;
   bool mv_readback_pending = false;
   static constexpr const char* mv_reject_names[] = {"targets", "format", "size", "depth_size", "blend", "msaa", "outside_scene", "not_scene_color"};
   std::atomic<uint32_t> mv_rejects[std::size(mv_reject_names)] = {};
   std::atomic<uint64_t> mv_reject_examples[std::size(mv_reject_names)] = {};
   uint64_t mv_rejected_scene_target = 0;
#endif

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

   // Frame probe: every buffer mapped or updated in the probed frame, and the draws that read it as vc2, vc3 or an instance stream
   struct ProbeBuffer
   {
      uint32_t maps[5] = {}; // By reshade::api::map_access (1 read_only, 2 write_only = NO_OVERWRITE or WRITE, 3 read_write, 4 write_discard)
      uint32_t updates = 0;  // UpdateSubresource
      uint32_t object_draws = 0;
      uint32_t bone_draws = 0;
      uint32_t instance_draws = 0; // Bound at a vertex buffer slot >= 1
      D3D11_BUFFER_DESC desc = {};
      uint8_t* mapped = nullptr;
      std::vector<uint8_t> shadow; // Constant buffers: CPU copy of the last write (Unmap or update)
   };
   std::atomic<ProbeRequest> probe_mode = ProbeRequest::None; // The probed frame
   std::mutex probe_mutex;
   std::unordered_map<uint64_t, ProbeBuffer> probe_buffers;
   uint32_t probe_draws = 0;
   uint32_t probe_map_events = 0; // Every buffer event: 0 means this ReShade build doesn't send them
   uint32_t probe_update_events = 0;
   std::unordered_map<std::string_view, uint32_t> probe_pass_draws;
   std::ofstream probe_dump;
#endif
};

class SaintsRowIV final : public Game
{
   static SaintsRowIVGameDeviceData& GetGameDeviceData(DeviceData& device_data)
   {
      return *static_cast<SaintsRowIVGameDeviceData*>(device_data.game);
   }

   // Writes a dynamic constant buffer, (re)created at the data's size; false if it can't
   static bool WriteConstants(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, com_ptr<ID3D11Buffer>* buffer, const void* data, UINT size)
   {
      D3D11_BUFFER_DESC desc = {};
      if (*buffer)
         (*buffer)->GetDesc(&desc);
      if (desc.ByteWidth != size)
      {
         buffer->reset();
         desc = {size, D3D11_USAGE_DYNAMIC, D3D11_BIND_CONSTANT_BUFFER, D3D11_CPU_ACCESS_WRITE};
         native_device->CreateBuffer(&desc, nullptr, &(*buffer));
      }
      D3D11_MAPPED_SUBRESOURCE mapped;
      if (!*buffer || FAILED(native_device_context->Map(buffer->get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
         return false;
      std::memcpy(mapped.pData, data, size);
      native_device_context->Unmap(buffer->get(), 0);
      return true;
   }

   // An upscaler is picked, hasn't failed (it then gives way to SMAA until picked again) and the scene isn't the game's MSAA one (no
   // motion vectors there: the upscaler steps aside while it lasts)
   static bool IsSRActive(DeviceData& device_data)
   {
      return device_data.sr_type != SR::Type::None && !device_data.sr_suppressed && !GetGameDeviceData(device_data).msaa_scene;
   }

   // A vc2 / vc3 buffer's CPU copy (empty until its first Unmap); registers it for a copy at every Unmap
   static std::vector<uint8_t> GetConstantsCopy(SaintsRowIVGameDeviceData* game_device_data, ID3D11Buffer* buffer)
   {
      if (!buffer)
         return {};
      const std::lock_guard lock(game_device_data->mv_constants_mutex);
      return game_device_data->mv_constants_copies[reinterpret_cast<uint64_t>(buffer)];
   }

   static void OnMapBufferRegion(reshade::api::device* device, reshade::api::resource resource, uint64_t offset, uint64_t size, reshade::api::map_access access, void** data)
   {
      DeviceData* const device_data = device->get_private_data<DeviceData>();
      if (device_data && device_data->game && GetGameDeviceData(*device_data).mv_active && access == reshade::api::map_access::write_discard && offset == 0 && data && *data)
      {
         auto& game_device_data = GetGameDeviceData(*device_data);
         const std::lock_guard lock(game_device_data.mv_constants_mutex);
         if (game_device_data.mv_constants_copies.contains(resource.handle))
            game_device_data.mv_mapped_constants[resource.handle] = *data;
      }
#if DEVELOPMENT
      OnProbeMapBufferRegion(device, resource, offset, size, access, data);
#endif
   }

   // Motion vectors: the CPU copy of a vc2 / vc3 buffer, before its Unmap (the game has written it). Reads the mapped memory back.
   static void OnUnmapBufferRegion(reshade::api::device* device, reshade::api::resource resource)
   {
      DeviceData* const device_data = device->get_private_data<DeviceData>();
      if (device_data && device_data->game && GetGameDeviceData(*device_data).mv_active)
      {
         auto& game_device_data = GetGameDeviceData(*device_data);
         const std::lock_guard lock(game_device_data.mv_constants_mutex);
         if (const auto mapped = game_device_data.mv_mapped_constants.find(resource.handle); mapped != game_device_data.mv_mapped_constants.end())
         {
            D3D11_BUFFER_DESC desc;
            reinterpret_cast<ID3D11Buffer*>(resource.handle)->GetDesc(&desc);
            auto& copy = game_device_data.mv_constants_copies[resource.handle];
            copy.resize(desc.ByteWidth);
            std::memcpy(copy.data(), mapped->second, desc.ByteWidth);
            game_device_data.mv_mapped_constants.erase(mapped);
#if DEVELOPMENT
            game_device_data.mv_constant_copies++;
#endif
         }
      }
#if DEVELOPMENT
      OnProbeUnmapBufferRegion(device, resource);
#endif
   }

   // Motion vectors: the copy of the material target into the post scene (see "mv_scene_color")
   bool OverrideCopyResource(ID3D11Device* native_device, DeviceData& device_data, uint64_t& dst_resource, uint64_t& src_resource) override
   {
      auto& game_device_data = GetGameDeviceData(device_data);
      if (game_device_data.mv_active && game_device_data.mv_scene_color_wanted)
      {
         game_device_data.mv_scene_copy_source = src_resource;
         game_device_data.mv_scene_copy_dest = dst_resource;
         game_device_data.mv_scene_copied |= src_resource == uint64_t(game_device_data.mv_scene_color.get());
      }
      return false;
   }
   bool OverrideCopyTextureRegion(ID3D11Device* native_device, DeviceData& device_data, uint64_t& dst_resource, uint32_t dst_subresource, const D3D11_BOX* dst_box, uint64_t& src_resource, uint32_t src_subresource, const D3D11_BOX* src_box) override
   {
      return OverrideCopyResource(native_device, device_data, dst_resource, src_resource);
   }

   // The bound shader's motion vector version, patched from Core's bytecode copy on first use (null if it can't be)
   template <typename T>
   static com_ptr<T> GetMotionVectorShader(ID3D11Device* native_device, DeviceData& device_data, std::unordered_map<uint32_t, com_ptr<T>>* shaders, uint32_t hash, reshade::api::pipeline pipeline)
   {
      constexpr bool vertex = std::is_same_v<T, ID3D11VertexShader>;
      auto& game_device_data = GetGameDeviceData(device_data);
      {
         const std::shared_lock lock(game_device_data.mv_mutex);
         if (const auto it = shaders->find(hash); it != shaders->end())
            return it->second;
      }
      std::vector<uint8_t> patched;
      std::string error = "no bytecode";
      bool screen_space = false;
      {
         const std::shared_lock lock(s_mutex_generic);
         if (const auto it = device_data.pipeline_cache_by_pipeline_handle.find(pipeline.handle); it != device_data.pipeline_cache_by_pipeline_handle.end() && it->second->subobjects_cache)
         {
            const auto* desc = static_cast<const reshade::api::shader_desc*>(it->second->subobjects_cache[0].data);
            const auto* code = static_cast<const uint8_t*>(desc->code);
            patched = vertex ? MotionVectorPatches::PatchVertexShader(code, desc->code_size, &error) : MotionVectorPatches::PatchPixelShader(code, desc->code_size, &error);
            // Skinned: the bone palette vc3 ("Bone_weights") needs its own previous copy. Screen space quads (deferred lights, decals, 2D)
            // bind vc2 without placing vertices with its projTM: jittered, they'd shift against their own UVs and read the G-buffer off
            com_ptr<ID3D11ShaderReflection> reflection;
            if (vertex && !patched.empty() && Shader::d3d_reflect && SUCCEEDED(Shader::d3d_reflect(code, desc->code_size, IID_PPV_ARGS(&reflection))))
            {
               D3D11_SHADER_VARIABLE_DESC projection_desc;
               D3D11_SHADER_INPUT_BIND_DESC bind_desc;
               if (FAILED(reflection->GetVariableByName("projTM")->GetDesc(&projection_desc)) || (projection_desc.uFlags & D3D_SVF_USED) == 0)
               {
                  patched.clear();
                  error = "screen space (no vc2 projTM)";
                  screen_space = true;
               }
               else if (SUCCEEDED(reflection->GetResourceBindingDescByName("vc3", &bind_desc)) && bind_desc.BindPoint == MotionVectorPatches::previous_slots[1].first)
               {
                  const std::unique_lock lock(game_device_data.mv_mutex);
                  game_device_data.mv_bone_vertex_shaders.insert(hash);
               }
            }
         }
      }
      com_ptr<T> shader;
      if (!patched.empty())
      {
         HRESULT hr;
         if constexpr (vertex)
            hr = native_device->CreateVertexShader(patched.data(), patched.size(), nullptr, &shader);
         else
            hr = native_device->CreatePixelShader(patched.data(), patched.size(), nullptr, &shader);
         if (FAILED(hr))
            error = std::format("create 0x{:08X}", uint32_t(hr));
      }
      // Failures in every build (bug reports), every patched shader only in development
      if (DEVELOPMENT || !shader)
         reshade::log::message((shader || screen_space) ? reshade::log::level::info : reshade::log::level::warning, std::format("[SR4 MV] {} 0x{:08X} {}", vertex ? "VS" : "PS", hash, shader ? "patched" : error).c_str());
      const std::unique_lock lock(game_device_data.mv_mutex);
      return shaders->try_emplace(hash, shader).first->second;
   }

   // Draws an opaque main material pass draw (the fp16 scene alone, output sized, with depth) with the patched shaders, adding the
   // motion vector target ("target_slot", past the game's) and the previous frame's vc2 / vc3 ("previous_slots"). False if it can't
   // (the draw then runs untouched).
   static bool DrawWithMotionVectors(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, CommandListData& cmd_list_data, DeviceData& device_data, const ShaderHashesList<OneShaderPerPipeline>& original_shader_hashes, const std::function<void()>& draw)
   {
      auto& game_device_data = GetGameDeviceData(device_data);
#if DEVELOPMENT
      game_device_data.mv_candidate_draws++;
      const auto reject = [&](size_t reason)
      {
         game_device_data.mv_rejects[reason]++;
         game_device_data.mv_reject_examples[reason] = (uint64_t(original_shader_hashes.vertex_shaders[0]) << 32) | uint32_t(original_shader_hashes.pixel_shaders[0]);
         return false;
      };
#else
      const auto reject = [](size_t)
      { return false; };
#endif
      com_ptr<ID3D11RenderTargetView> rtvs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
      com_ptr<ID3D11DepthStencilView> dsv;
      native_device_context->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, &rtvs[0], &dsv);
      // The G-buffer (2-3 targets, output sized depth) opens the scene; post passes with 2 targets must not reopen it
      if (rtvs[0] && rtvs[1])
      {
         if (!game_device_data.mv_scene_open && dsv)
         {
            uint4 gbuffer_depth_size;
            DXGI_FORMAT unused_gbuffer_format;
            GetResourceInfo(dsv.get(), gbuffer_depth_size, unused_gbuffer_format);
            if (gbuffer_depth_size.x == device_data.output_resolution.x && gbuffer_depth_size.y == device_data.output_resolution.y)
            {
               game_device_data.mv_scene_open = true;
               game_device_data.mv_scene_copied = false;
               game_device_data.mv_scene_copy_source = 0;
               game_device_data.mv_scene_copy_dest = 0;
               // Halton (2, 3) over the upscaler's phase count; pixels to NDC (y up)
               const SR::InstanceData* const sr_instance_data = IsSRActive(device_data) ? device_data.GetSRInstanceData() : nullptr;
               const int phases = sr_instance_data ? (std::max)(sr_implementations[device_data.sr_type]->GetJitterPhases(sr_instance_data), 1) : SR::GetDefaultJitterPhases();
               const unsigned int phase = cb_luma_global_settings.FrameIndex % phases;
               game_device_data.mv_jitter = (sr_instance_data || g_mv_force_jitter) ? std::array<float, 2>{SR::HaltonSequence(phase, 2), SR::HaltonSequence(phase, 3)} : std::array<float, 2>{};
               game_device_data.mv_jitter_ndc = {game_device_data.mv_jitter[0] * 2.f / float(gbuffer_depth_size.x), game_device_data.mv_jitter[1] * -2.f / float(gbuffer_depth_size.y)};
               const float ndc_jitter[4] = {game_device_data.mv_jitter_ndc[0], game_device_data.mv_jitter_ndc[1], 0.f, 0.f};
               if (!WriteConstants(native_device, native_device_context, std::addressof(game_device_data.mv_jitter_buffer), ndc_jitter, sizeof(ndc_jitter)))
               {
                  // No stale jitter on the material draws either: no motion vectors this frame
                  game_device_data.mv_jitter = {};
                  game_device_data.mv_jitter_ndc = {};
                  game_device_data.mv_jitter_buffer.reset();
               }
            }
         }
         game_device_data.mv_scene_color_wanted |= game_device_data.mv_scene_open;
         return reject(0);
      }
      if (!rtvs[0] || !dsv || std::any_of(std::begin(rtvs) + 1, std::end(rtvs), [](const auto& rtv)
                                 { return rtv.get() != nullptr; }))
         return reject(0);
      if (!game_device_data.mv_scene_open)
         return reject(6);
      {
         com_ptr<ID3D11Resource> color;
         rtvs[0]->GetResource(&color);
         if (!color || color != game_device_data.mv_scene_color)
         {
#if DEVELOPMENT
            game_device_data.mv_rejected_scene_target = uint64_t(color.get());
#endif
            return reject(7);
         }
      }
      D3D11_RENDER_TARGET_VIEW_DESC rtv_desc;
      rtvs[0]->GetDesc(&rtv_desc);
      if (rtv_desc.Format != DXGI_FORMAT_R16G16B16A16_FLOAT)
         return reject(1);
      // The target must match the scene's sample count; MSAA (display.ini MSAA_Level) is off with DLAA
      if (rtv_desc.ViewDimension != D3D11_RTV_DIMENSION_TEXTURE2D)
         return reject(5);
      uint4 size, depth_size;
      DXGI_FORMAT unused_format;
      GetResourceInfo(rtvs[0].get(), size, unused_format);
      GetResourceInfo(dsv.get(), depth_size, unused_format);
      // The main scene only (reflections and cube faces are smaller)
      if (size.x != device_data.output_resolution.x || size.y != device_data.output_resolution.y)
         return reject(2);
      if (depth_size.x != size.x || depth_size.y != size.y)
         return reject(3);
      com_ptr<ID3D11BlendState> blend_state;
      FLOAT blend_factor[4];
      UINT sample_mask;
      native_device_context->OMGetBlendState(&blend_state, blend_factor, &sample_mask);
      D3D11_BLEND_DESC blend_desc = CD3D11_BLEND_DESC(D3D11_DEFAULT);
      if (blend_state)
         blend_state->GetDesc(&blend_desc);
      // Translucent draws keep the motion vectors of what's behind them. The material pass blends One/Zero (an opaque write).
      const D3D11_RENDER_TARGET_BLEND_DESC& rt0_blend = blend_desc.RenderTarget[0];
      const bool opaque = !rt0_blend.BlendEnable || (rt0_blend.SrcBlend == D3D11_BLEND_ONE && rt0_blend.DestBlend == D3D11_BLEND_ZERO && rt0_blend.BlendOp == D3D11_BLEND_OP_ADD);
      if (!opaque)
         return reject(4);

      const com_ptr<ID3D11VertexShader> vertex_shader = GetMotionVectorShader(native_device, device_data, &game_device_data.mv_vertex_shaders, original_shader_hashes.vertex_shaders[0], cmd_list_data.pipeline_state_original_vertex_shader);
      const com_ptr<ID3D11PixelShader> pixel_shader = GetMotionVectorShader(native_device, device_data, &game_device_data.mv_pixel_shaders, original_shader_hashes.pixel_shaders[0], cmd_list_data.pipeline_state_original_pixel_shader);
      if (!vertex_shader || !pixel_shader)
      {
#if DEVELOPMENT
         game_device_data.mv_skipped_draws++;
#endif
         return false;
      }
      // With independent blending, a copy of the state writes the target unblended (D3D11 hands back the existing object for a
      // description it has seen); without, it takes RT0's (off, checked above)
      com_ptr<ID3D11BlendState> motion_vector_blend_state = blend_state;
      if (blend_desc.IndependentBlendEnable)
      {
         D3D11_BLEND_DESC desc = blend_desc;
         desc.RenderTarget[MotionVectorPatches::target_slot] = {FALSE, D3D11_BLEND_ONE, D3D11_BLEND_ZERO, D3D11_BLEND_OP_ADD, D3D11_BLEND_ONE, D3D11_BLEND_ZERO, D3D11_BLEND_OP_ADD, D3D11_COLOR_WRITE_ENABLE_ALL};
         motion_vector_blend_state.reset();
         if (FAILED(native_device->CreateBlendState(&desc, &motion_vector_blend_state)))
            return false;
      }

      {
         const std::unique_lock lock(game_device_data.mv_mutex);
         D3D11_TEXTURE2D_DESC desc = {};
         if (game_device_data.mv_texture)
            game_device_data.mv_texture->GetDesc(&desc);
         if (desc.Width != size.x || desc.Height != size.y)
         {
            game_device_data.mv_texture.reset();
            game_device_data.mv_rtv.reset();
            game_device_data.mv_uav.reset();
            // The fill reads the target back through its UAV
            D3D11_FEATURE_DATA_FORMAT_SUPPORT2 support = {DXGI_FORMAT_R32G32_FLOAT};
            const bool typed_uav_load = SUCCEEDED(native_device->CheckFeatureSupport(D3D11_FEATURE_FORMAT_SUPPORT2, &support, sizeof(support))) && (support.OutFormatSupport2 & D3D11_FORMAT_SUPPORT2_UAV_TYPED_LOAD) != 0;
            desc = {size.x, size.y, 1, 1, DXGI_FORMAT_R32G32_FLOAT, {1, 0}, D3D11_USAGE_DEFAULT, D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | (typed_uav_load ? D3D11_BIND_UNORDERED_ACCESS : 0u)};
            if (FAILED(native_device->CreateTexture2D(&desc, nullptr, &game_device_data.mv_texture)) || FAILED(native_device->CreateRenderTargetView(game_device_data.mv_texture.get(), nullptr, &game_device_data.mv_rtv)))
            {
               game_device_data.mv_texture.reset();
               game_device_data.mv_rtv.reset();
               return false;
            }
            if (typed_uav_load)
               native_device->CreateUnorderedAccessView(game_device_data.mv_texture.get(), nullptr, &game_device_data.mv_uav);
            game_device_data.mv_frame_ended = true;
         }
      }
      if (!game_device_data.mv_jitter_buffer)
         return false;
      if (std::exchange(game_device_data.mv_frame_ended, false))
      {
         // The material draws bind the G-buffer depth (IR_GBuffer_Depth) at PS t14
         game_device_data.mv_frame_depth.reset();
         native_device_context->PSGetShaderResources(14, 1, &game_device_data.mv_frame_depth);
         if (game_device_data.mv_frame_depth)
         {
            D3D11_SHADER_RESOURCE_VIEW_DESC depth_desc;
            game_device_data.mv_frame_depth->GetDesc(&depth_desc);
            uint4 frame_depth_size;
            DXGI_FORMAT unused_depth_format;
            GetResourceInfo(game_device_data.mv_frame_depth.get(), frame_depth_size, unused_depth_format);
            if (depth_desc.Format != DXGI_FORMAT_R24_UNORM_X8_TYPELESS || depth_desc.ViewDimension != D3D11_SRV_DIMENSION_TEXTURE2D || frame_depth_size.x != size.x || frame_depth_size.y != size.y)
               game_device_data.mv_frame_depth.reset();
         }
         {
            const std::shared_lock lock_shader_objects(s_mutex_shader_objects);
            game_device_data.mv_fill_pending = game_device_data.mv_frame_depth && game_device_data.mv_uav && HasShaders(device_data.native_compute_shaders, "SR4 Motion Vector Fill CS"_h);
         }
         const FLOAT clear_value = game_device_data.mv_fill_pending ? FLT_MAX : 0.f;
         const FLOAT clear[4] = {clear_value, clear_value, 0.f, 0.f};
         native_device_context->ClearRenderTargetView(game_device_data.mv_rtv.get(), clear);
         // Last frame's camera and objects are the previous ones, unless frames without a scene (menus, videos) came between
         game_device_data.mv_previous_camera = game_device_data.mv_camera;
         game_device_data.mv_previous_camera_valid = game_device_data.mv_camera_valid && cb_luma_global_settings.FrameIndex - game_device_data.mv_frame_index <= 1;
         game_device_data.mv_camera_valid = false;
         game_device_data.mv_frame_index = cb_luma_global_settings.FrameIndex;
         game_device_data.mv_previous_objects = std::move(game_device_data.mv_objects);
         game_device_data.mv_objects.clear();
         game_device_data.mv_objects.reserve(game_device_data.mv_previous_objects.size());
         if (!game_device_data.mv_previous_camera_valid)
            game_device_data.mv_previous_objects.clear();
#if DEVELOPMENT
         game_device_data.mv_frames++;
#endif
      }

      // Set directly, bypassing Core's state tracking: the game's state is restored after the draw
      com_ptr<ID3D11VertexShader> original_vertex_shader;
      com_ptr<ID3D11PixelShader> original_pixel_shader;
      native_device_context->VSGetShader(&original_vertex_shader, nullptr, nullptr);
      native_device_context->PSGetShader(&original_pixel_shader, nullptr, nullptr);
      // One read: the game's vc2 / vc3 and the slots added past them (restored after the draw)
      static_assert(MotionVectorPatches::previous_slots[0].second == MotionVectorPatches::jitter_slot + 1 && MotionVectorPatches::previous_slots[1].second == MotionVectorPatches::jitter_slot + 2);
      com_ptr<ID3D11Buffer> original_cbs[MotionVectorPatches::jitter_slot + std::size(MotionVectorPatches::previous_slots) + 1];
      constexpr UINT first_added_slot = MotionVectorPatches::jitter_slot;
      native_device_context->VSGetConstantBuffers(0, UINT(std::size(original_cbs)), &original_cbs[0]);
      // The previous frame's vc2 / vc3: the same object's from last frame, else this draw's with last frame's camera (no object motion).
      // Until a buffer has a CPU copy, the current ones (zero motion).
      ID3D11Buffer* previous_buffers[std::size(MotionVectorPatches::previous_slots)] = {};
      for (size_t i = 0; i < std::size(previous_buffers); i++)
         previous_buffers[i] = original_cbs[MotionVectorPatches::previous_slots[i].first].get();
      bool skinned;
      {
         const std::shared_lock lock(game_device_data.mv_mutex);
         skinned = game_device_data.mv_bone_vertex_shaders.contains(original_shader_hashes.vertex_shaders[0]);
      }
      std::vector<uint8_t> object = GetConstantsCopy(&game_device_data, previous_buffers[0]);
      std::vector<uint8_t> bones = skinned ? GetConstantsCopy(&game_device_data, previous_buffers[1]) : std::vector<uint8_t>{};
      // vc2: projTM c0-c3 (the camera), objTM c16-c18 (translation in .w)
      constexpr size_t camera_size = sizeof(game_device_data.mv_camera);
      if (object.size() >= 19 * 16 && (!skinned || !bones.empty()))
      {
         if (!game_device_data.mv_camera_valid)
         {
            std::memcpy(game_device_data.mv_camera.data(), object.data(), camera_size);
            game_device_data.mv_camera_valid = true;
#if DEVELOPMENT
            game_device_data.mv_camera_draw = (uint64_t(original_shader_hashes.vertex_shaders[0]) << 32) | uint32_t(original_shader_hashes.pixel_shaders[0]);
            if (game_device_data.mv_previous_camera_valid)
            {
               for (int i = 0; i < 16; i++)
                  game_device_data.mv_camera_delta = (std::max)(game_device_data.mv_camera_delta, std::abs(game_device_data.mv_camera[i] - game_device_data.mv_previous_camera[i]));
            }
#endif
         }

         // Draw key: same mesh, same shaders. Objects sharing it (props) are told apart by translation. No instance count: frustum culling
         // changes it for the instanced statics as the camera turns.
         com_ptr<ID3D11Buffer> vertex_buffer;
         UINT vertex_stride = 0, vertex_offset = 0;
         native_device_context->IAGetVertexBuffers(0, 1, &vertex_buffer, &vertex_stride, &vertex_offset);
         com_ptr<ID3D11Buffer> index_buffer;
         DXGI_FORMAT index_format;
         UINT index_offset = 0;
         native_device_context->IAGetIndexBuffer(&index_buffer, &index_format, &index_offset);
         const DrawDispatchData& draw_data = last_draw_dispatch_data;
         uint64_t key = 0;
         for (const uint64_t value : {uint64_t(original_shader_hashes.vertex_shaders[0]), uint64_t(original_shader_hashes.pixel_shaders[0]), reinterpret_cast<uint64_t>(vertex_buffer.get()), uint64_t(vertex_offset), reinterpret_cast<uint64_t>(index_buffer.get()), uint64_t(index_offset), uint64_t(draw_data.index_count), uint64_t(draw_data.first_index), uint64_t(uint32_t(draw_data.vertex_offset)), uint64_t(draw_data.vertex_count), uint64_t(draw_data.first_vertex)})
            HashCombine(key, value);
         const float* const values = reinterpret_cast<const float*>(object.data());
         const std::array<float, 3> translation = {values[16 * 4 + 3], values[17 * 4 + 3], values[18 * 4 + 3]};

         // ponytail: linear search among the key's candidates (a handful at most); a spatial lookup if big crowds share a mesh
         const SaintsRowIVGameDeviceData::MotionVectorObject* match = nullptr;
         if (const auto previous = game_device_data.mv_previous_objects.find(key); previous != game_device_data.mv_previous_objects.end())
         {
            float nearest = FLT_MAX;
            for (const auto& candidate : previous->second)
            {
               const float dx = candidate.translation[0] - translation[0], dy = candidate.translation[1] - translation[1], dz = candidate.translation[2] - translation[2];
               const float distance = dx * dx + dy * dy + dz * dz;
               if (candidate.object.size() == object.size() && candidate.bones.size() == bones.size() && distance < nearest)
               {
                  nearest = distance;
                  match = &candidate;
               }
            }
         }
         // Draws with another projTM than the frame's camera (sky, windows at infinity) are left as is and not tracked
         const bool frame_camera = std::memcmp(object.data(), game_device_data.mv_camera.data(), camera_size) == 0;
#if DEVELOPMENT
         game_device_data.mv_off_camera_draws += !frame_camera;
#endif
         if (match)
         {
#if DEVELOPMENT
            game_device_data.mv_matched_draws++;
            game_device_data.mv_moved_draws += match->object != object;
            if (game_device_data.mv_sample_camera_delta < 0.f)
            {
               const float* const previous_values = reinterpret_cast<const float*>(match->object.data());
               float camera_delta = 0.f, object_delta = 0.f;
               for (int i = 0; i < 16; i++)
                  camera_delta = (std::max)(camera_delta, std::abs(previous_values[i] - values[i]));
               for (int i = 16 * 4; i < 19 * 4; i++)
                  object_delta = (std::max)(object_delta, std::abs(previous_values[i] - values[i]));
               game_device_data.mv_sample_camera_delta = camera_delta;
               game_device_data.mv_sample_object_delta = object_delta;
            }
#endif
            if (WriteConstants(native_device, native_device_context, std::addressof(game_device_data.mv_previous_buffers[0]), match->object.data(), UINT(match->object.size())))
               previous_buffers[0] = game_device_data.mv_previous_buffers[0].get();
            if (skinned && WriteConstants(native_device, native_device_context, std::addressof(game_device_data.mv_previous_buffers[1]), match->bones.data(), UINT(match->bones.size())))
               previous_buffers[1] = game_device_data.mv_previous_buffers[1].get();
            // Kept as drawn for the next frame (after the upload: "match" points into last frame's list)
            game_device_data.mv_objects[key].push_back({translation, std::move(object), std::move(bones)});
         }
         else if (frame_camera)
         {
            // Not found: its own constants with last frame's camera (camera motion only)
            if (game_device_data.mv_previous_camera_valid)
            {
               auto& upload = game_device_data.mv_camera_upload;
               upload.assign(object.begin(), object.end());
               std::memcpy(upload.data(), game_device_data.mv_previous_camera.data(), camera_size);
               if (WriteConstants(native_device, native_device_context, std::addressof(game_device_data.mv_previous_buffers[0]), upload.data(), UINT(upload.size())))
                  previous_buffers[0] = game_device_data.mv_previous_buffers[0].get();
            }
            game_device_data.mv_objects[key].push_back({translation, std::move(object), std::move(bones)});
         }
#if DEVELOPMENT
         else
         {
            game_device_data.mv_other_camera_draws++;
         }
#endif
      }
#if DEVELOPMENT
      else
      {
         game_device_data.mv_uncopied_draws++;
      }
#endif
      for (size_t i = 0; i < std::size(previous_buffers); i++)
         native_device_context->VSSetConstantBuffers(MotionVectorPatches::previous_slots[i].second, 1, &previous_buffers[i]);
      ID3D11Buffer* const jitter = game_device_data.mv_jitter_buffer.get();
      native_device_context->VSSetConstantBuffers(MotionVectorPatches::jitter_slot, 1, &jitter);
      // The second run's resources: the current ones (no copies kept, see "MotionVectorPatches::previous_resources_slot")
      com_ptr<ID3D11ShaderResourceView> resources[MotionVectorPatches::resource_slots];
      native_device_context->VSGetShaderResources(0, MotionVectorPatches::resource_slots, &resources[0]);
      ID3D11ShaderResourceView* previous_resources[MotionVectorPatches::resource_slots] = {};
      for (UINT slot = 0; slot < MotionVectorPatches::resource_slots; slot++)
         previous_resources[slot] = resources[slot].get();
      native_device_context->VSSetShaderResources(MotionVectorPatches::previous_resources_slot, MotionVectorPatches::resource_slots, previous_resources);
      ID3D11RenderTargetView* targets[MotionVectorPatches::target_slot + 1] = {rtvs[0].get()};
      targets[MotionVectorPatches::target_slot] = game_device_data.mv_rtv.get();
      native_device_context->OMSetRenderTargets(MotionVectorPatches::target_slot + 1, targets, dsv.get());
      if (motion_vector_blend_state != blend_state)
         native_device_context->OMSetBlendState(motion_vector_blend_state.get(), blend_factor, sample_mask);
      native_device_context->VSSetShader(vertex_shader.get(), nullptr, 0);
      native_device_context->PSSetShader(pixel_shader.get(), nullptr, 0);

      draw();

      native_device_context->VSSetShader(original_vertex_shader.get(), nullptr, 0);
      native_device_context->PSSetShader(original_pixel_shader.get(), nullptr, 0);
      if (motion_vector_blend_state != blend_state)
         native_device_context->OMSetBlendState(blend_state.get(), blend_factor, sample_mask);
      ID3D11RenderTargetView* const original_rtv = rtvs[0].get();
      native_device_context->OMSetRenderTargets(1, &original_rtv, dsv.get());
      ID3D11Buffer* restored_cbs[std::size(original_cbs)] = {};
      for (size_t i = first_added_slot; i < std::size(original_cbs); i++)
         restored_cbs[i] = original_cbs[i].get();
      native_device_context->VSSetConstantBuffers(first_added_slot, UINT(std::size(original_cbs)) - first_added_slot, &restored_cbs[first_added_slot]);
      ID3D11ShaderResourceView* const null_resources[MotionVectorPatches::resource_slots] = {};
      native_device_context->VSSetShaderResources(MotionVectorPatches::previous_resources_slot, MotionVectorPatches::resource_slots, null_resources);
#if DEVELOPMENT
      game_device_data.mv_draws++;
#endif
      return true;
   }

   // DLSS / FSR at native resolution on the jittered scene, before its first post pass (immediate context): that pass's t0 (the
   // material target's copy, with the forward draws), the G-buffer depth and the motion vectors. The result goes back into t0.
   static void DrawSuperResolution(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, DeviceData& device_data)
   {
      auto& game_device_data = GetGameDeviceData(device_data);
      // The post scene: this frame's copy of the material target, else the pass's t0 (the downsample reads the scene there, the god
      // rays mask, first when the sun is in view, reads the depth)
      com_ptr<ID3D11Resource> scene_resource;
      if (game_device_data.mv_scene_copy_dest && game_device_data.mv_scene_copy_source == uint64_t(game_device_data.mv_scene_color.get()))
      {
         scene_resource = reinterpret_cast<ID3D11Resource*>(game_device_data.mv_scene_copy_dest);
      }
      else
      {
         com_ptr<ID3D11ShaderResourceView> scene_srv;
         native_device_context->PSGetShaderResources(0, 1, &scene_srv);
         if (scene_srv)
            scene_srv->GetResource(&scene_resource);
      }
      com_ptr<ID3D11Texture2D> scene;
#if DEVELOPMENT
      const auto skip = [&]()
      { game_device_data.mv_sr_skips++; };
#else
      const auto skip = []() {};
#endif
      if (!scene_resource || FAILED(scene_resource->QueryInterface(&scene)) || !game_device_data.mv_texture || !game_device_data.mv_frame_depth || !game_device_data.mv_camera_valid)
         return skip();
      D3D11_TEXTURE2D_DESC scene_desc, mv_desc;
      scene->GetDesc(&scene_desc);
      game_device_data.mv_texture->GetDesc(&mv_desc);
      if ((scene_desc.Format != DXGI_FORMAT_R16G16B16A16_FLOAT && scene_desc.Format != DXGI_FORMAT_R16G16B16A16_TYPELESS) || scene_desc.SampleDesc.Count != 1 || scene_desc.Width != mv_desc.Width || scene_desc.Height != mv_desc.Height)
         return skip();
      com_ptr<ID3D11Resource> depth;
      game_device_data.mv_frame_depth->GetResource(&depth);

      D3D11_TEXTURE2D_DESC output_desc = {};
      if (device_data.sr_output_color)
         device_data.sr_output_color->GetDesc(&output_desc);
      if (output_desc.Width != scene_desc.Width || output_desc.Height != scene_desc.Height)
      {
         device_data.sr_output_color.reset();
         output_desc = {scene_desc.Width, scene_desc.Height, 1, 1, DXGI_FORMAT_R16G16B16A16_FLOAT, {1, 0}, D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS};
         native_device->CreateTexture2D(&output_desc, nullptr, &device_data.sr_output_color);
      }
      SR::InstanceData* const sr_instance_data = device_data.GetSRInstanceData();
      if (!device_data.sr_output_color || !sr_instance_data)
         return skip();

      // FSR needs the camera (DLSS ignores it). projTM (vc2 c0-c3) is a column vector view projection with absolute world
      // translation: row 1 = the up axis / tan(fov / 2), row 3 = the view depth axis, row 2 = A * row 3 + B (w), with
      // A = far / (far - near) and B = -near * A. In double: the translations are large.
      const auto row = [&](int i)
      { return std::array<double, 4>{game_device_data.mv_camera[i * 4], game_device_data.mv_camera[i * 4 + 1], game_device_data.mv_camera[i * 4 + 2], game_device_data.mv_camera[i * 4 + 3]}; };
      const auto length3 = [](const std::array<double, 4>& v)
      { return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]); };
      const std::array<double, 4> up = row(1), z = row(2), w = row(3);
      const double a = length3(w) > 0.0 ? length3(z) / length3(w) : 0.0;
      const double b = z[3] - a * w[3];
      const double near_plane = a > 0.0 ? -b / a : 0.0;
      // far = B / (1 - A), finite for A > 1 (A ~= 1.00001 at near 0.15: ~15 km). ponytail: 100 km for A <= 1 (infinite); Core's FSR
      // sets FFX_FSR3_ENABLE_DEPTH_INFINITE only with inverted depth, a separate infinite far flag in SR::SettingsData would drop it
      const double far_plane = (a > 1.0 && b / (1.0 - a) > near_plane) ? b / (1.0 - a) : 100000.0;
      const double vert_fov = length3(up) > 0.0 ? 2.0 * std::atan(1.0 / length3(up)) : 0.0;

      DrawStateStack<DrawStateStackType::FullGraphics> graphics_state;
      DrawStateStack<DrawStateStackType::Compute> compute_state;
      graphics_state.Cache(native_device_context, device_data.uav_max_count);
      compute_state.Cache(native_device_context, device_data.uav_max_count);

      SR::SettingsData settings_data;
      settings_data.output_width = scene_desc.Width;
      settings_data.output_height = scene_desc.Height;
      settings_data.render_width = scene_desc.Width;
      settings_data.render_height = scene_desc.Height;
      settings_data.dynamic_resolution = false;
      settings_data.hdr = true;
      settings_data.inverted_depth = false;
      settings_data.mvs_jittered = false;
      // The motion vectors are UV deltas, previous minus current
      settings_data.mvs_x_scale = float(scene_desc.Width);
      settings_data.mvs_y_scale = float(scene_desc.Height);
      settings_data.auto_exposure = true;
      settings_data.render_preset = dlss_render_preset;
      sr_implementations[device_data.sr_type]->UpdateSettings(sr_instance_data, native_device_context, settings_data);

      SR::SuperResolutionImpl::DrawData draw_data;
      draw_data.source_color = scene.get();
      draw_data.output_color = device_data.sr_output_color.get();
      draw_data.motion_vectors = game_device_data.mv_texture.get();
      draw_data.depth_buffer = depth.get();
      draw_data.render_width = scene_desc.Width;
      draw_data.render_height = scene_desc.Height;
      // As applied (pixels, +y down)
      draw_data.jitter_x = game_device_data.mv_jitter[0];
      draw_data.jitter_y = game_device_data.mv_jitter[1];
      draw_data.reset = device_data.force_reset_sr;
      if (vert_fov > 0.0)
         draw_data.vert_fov = float(vert_fov);
      if (near_plane > 0.0)
      {
         draw_data.near_plane = float(near_plane);
         draw_data.far_plane = float(far_plane);
      }
      if (sr_implementations[device_data.sr_type]->Draw(sr_instance_data, native_device_context, draw_data))
      {
         native_device_context->CopySubresourceRegion(scene.get(), 0, 0, 0, 0, device_data.sr_output_color.get(), 0, nullptr);
         device_data.has_drawn_sr = true;
         device_data.render_resolution = {float(scene_desc.Width), float(scene_desc.Height)};
#if DEVELOPMENT
         game_device_data.mv_sr_draws++;
#endif
      }
      else
      {
         // Back to SMAA until the upscaler is picked again
         device_data.sr_suppressed = true;
#if DEVELOPMENT
         game_device_data.mv_sr_failures++;
#endif
      }
#if DEVELOPMENT
      if (cb_luma_global_settings.FrameIndex % 600 == 0)
         reshade::log::message(reshade::log::level::info, std::format("[SR4 SR] vert_fov {:.4g} deg, near {:.4g}, far {:.6g} (A {:.9g}), jitter {:.3f},{:.3f}, reset {}", vert_fov * 180.0 / 3.14159265358979, near_plane, far_plane, a, draw_data.jitter_x, draw_data.jitter_y, draw_data.reset).c_str());
#endif
      compute_state.Restore(native_device_context);
      graphics_state.Restore(native_device_context);
   }

   // Jitter for the scene's mesh draws without motion vectors (patched vertex shader, game pixel shader): the G-buffer, light volumes,
   // depth only and forward draws. Every draw depth tested against the scene takes the same jitter, or jittered and unjittered depths
   // of the same surface fail each other's test. False if it can't (the draw runs untouched).
   static bool DrawWithJitter(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, CommandListData& cmd_list_data, DeviceData& device_data, const ShaderHashesList<OneShaderPerPipeline>& original_shader_hashes, const std::function<void()>& draw)
   {
      auto& game_device_data = GetGameDeviceData(device_data);
      if (game_device_data.mv_jitter == std::array<float, 2>{} || !game_device_data.mv_jitter_buffer)
         return false;
#if !DEVELOPMENT // Development counts the scene sized draws outside the scene below
      if (!game_device_data.mv_scene_open)
         return false;
#endif
      com_ptr<ID3D11DepthStencilView> dsv;
      native_device_context->OMGetRenderTargets(0, nullptr, &dsv);
      com_ptr<ID3D11DepthStencilState> depth_stencil_state;
      UINT stencil_ref;
      native_device_context->OMGetDepthStencilState(&depth_stencil_state, &stencil_ref);
      D3D11_DEPTH_STENCIL_DESC depth_desc = CD3D11_DEPTH_STENCIL_DESC(D3D11_DEFAULT);
      if (depth_stencil_state)
         depth_stencil_state->GetDesc(&depth_desc);
      com_ptr<ID3D11Buffer> vertex_buffer;
      UINT vertex_stride, vertex_offset;
      native_device_context->IAGetVertexBuffers(0, 1, &vertex_buffer, &vertex_stride, &vertex_offset);
      // Meshes only (full screen passes have no vertex buffer or no depth test), into output sized depth (not shadows or reflections)
      if (!dsv || !depth_desc.DepthEnable)
         return false;
      uint4 depth_size;
      DXGI_FORMAT unused_format;
      GetResourceInfo(dsv.get(), depth_size, unused_format);
      if (depth_size.x != device_data.output_resolution.x || depth_size.y != device_data.output_resolution.y)
         return false;
#if DEVELOPMENT
      const auto skip = [&](size_t reason)
      {
         game_device_data.mv_jitter_skips[reason]++;
         game_device_data.mv_jitter_skip_examples[reason] = (uint64_t(original_shader_hashes.vertex_shaders[0]) << 32) | uint32_t(original_shader_hashes.pixel_shaders[0]);
         return false;
      };
#else
      const auto skip = [](size_t)
      { return false; };
#endif
      if (!game_device_data.mv_scene_open)
         return skip(0);
      if (!vertex_buffer)
         return skip(1);
      const com_ptr<ID3D11VertexShader> vertex_shader = GetMotionVectorShader(native_device, device_data, &game_device_data.mv_vertex_shaders, original_shader_hashes.vertex_shaders[0], cmd_list_data.pipeline_state_original_vertex_shader);
      if (!vertex_shader)
         return skip(2);

      // Set directly, bypassing Core's state tracking: the game's state is restored after the draw
      com_ptr<ID3D11VertexShader> original_vertex_shader;
      native_device_context->VSGetShader(&original_vertex_shader, nullptr, nullptr);
      com_ptr<ID3D11Buffer> original_jitter;
      native_device_context->VSGetConstantBuffers(MotionVectorPatches::jitter_slot, 1, &original_jitter);
      ID3D11Buffer* const jitter = game_device_data.mv_jitter_buffer.get();
      native_device_context->VSSetConstantBuffers(MotionVectorPatches::jitter_slot, 1, &jitter);
      native_device_context->VSSetShader(vertex_shader.get(), nullptr, 0);
      draw();
      native_device_context->VSSetShader(original_vertex_shader.get(), nullptr, 0);
      ID3D11Buffer* const restored_jitter = original_jitter.get();
      native_device_context->VSSetConstantBuffers(MotionVectorPatches::jitter_slot, 1, &restored_jitter);
#if DEVELOPMENT
      game_device_data.mv_jitter_only_draws++;
#endif
      return true;
   }

#if DEVELOPMENT
   // The probed frame's game data, null otherwise (the buffer events then cost nothing)
   static SaintsRowIVGameDeviceData* GetProbingGameDeviceData(reshade::api::device* device)
   {
      DeviceData* const device_data = device->get_private_data<DeviceData>();
      auto* const game_device_data = device_data && device_data->game ? &GetGameDeviceData(*device_data) : nullptr;
      return game_device_data && game_device_data->probe_mode != ProbeRequest::None ? game_device_data : nullptr;
   }

   // The probed frame's entry for a buffer. The caller holds probe_mutex.
   static SaintsRowIVGameDeviceData::ProbeBuffer& GetProbeBuffer(SaintsRowIVGameDeviceData* game_device_data, ID3D11Buffer* buffer)
   {
      auto& entry = game_device_data->probe_buffers[reinterpret_cast<uint64_t>(buffer)];
      if (entry.desc.ByteWidth == 0)
         buffer->GetDesc(&entry.desc);
      return entry;
   }

   static void OnProbeMapBufferRegion(reshade::api::device* device, reshade::api::resource resource, uint64_t offset, uint64_t size, reshade::api::map_access access, void** data)
   {
      auto* const game_device_data = GetProbingGameDeviceData(device);
      if (!game_device_data)
         return;
      const std::lock_guard lock(game_device_data->probe_mutex);
      game_device_data->probe_map_events++;
      auto& entry = GetProbeBuffer(game_device_data, reinterpret_cast<ID3D11Buffer*>(resource.handle));
      entry.maps[std::clamp(int(access), 0, 4)]++;
      entry.mapped = access != reshade::api::map_access::read_only && data && *data && offset == 0 ? static_cast<uint8_t*>(*data) : nullptr;
   }

   // Reads the mapped memory back (write combined, slow): DEVELOPMENT only, for the probed frame
   static void OnProbeUnmapBufferRegion(reshade::api::device* device, reshade::api::resource resource)
   {
      auto* const game_device_data = GetProbingGameDeviceData(device);
      if (!game_device_data)
         return;
      const std::lock_guard lock(game_device_data->probe_mutex);
      const auto it = game_device_data->probe_buffers.find(resource.handle);
      if (it == game_device_data->probe_buffers.end() || !it->second.mapped)
         return;
      auto& entry = it->second;
      if (entry.desc.BindFlags & D3D11_BIND_CONSTANT_BUFFER)
         entry.shadow.assign(entry.mapped, entry.mapped + entry.desc.ByteWidth);
      entry.mapped = nullptr;
   }

   static void RecordProbeUpdate(reshade::api::device* device, const void* data, reshade::api::resource dest, uint64_t dest_offset, uint64_t size)
   {
      auto* const game_device_data = GetProbingGameDeviceData(device);
      if (!game_device_data)
         return;
      const std::lock_guard lock(game_device_data->probe_mutex);
      game_device_data->probe_update_events++;
      auto& entry = GetProbeBuffer(game_device_data, reinterpret_cast<ID3D11Buffer*>(dest.handle));
      entry.updates++;
      if ((entry.desc.BindFlags & D3D11_BIND_CONSTANT_BUFFER) && data && dest_offset + size <= entry.desc.ByteWidth)
      {
         entry.shadow.resize(entry.desc.ByteWidth);
         std::memcpy(entry.shadow.data() + dest_offset, data, size_t(size));
      }
   }
   static bool OnProbeUpdateBufferRegion(reshade::api::device* device, const void* data, reshade::api::resource dest, uint64_t dest_offset, uint64_t size)
   {
      RecordProbeUpdate(device, data, dest, dest_offset, size);
      return false;
   }
   static bool OnProbeUpdateBufferRegionCommand(reshade::api::command_list* cmd_list, const void* data, reshade::api::resource dest, uint64_t dest_offset, uint64_t size)
   {
      RecordProbeUpdate(cmd_list->get_device(), data, dest, dest_offset, size);
      return false;
   }

   // One ReShade.log line per draw of the probed frame, plus the vc2 values that tell the transform model (world absolute or
   // camera relative, per-draw or shared camera), and with a dump, the CSV rows of vc2 / vc3.
   static void LogProbeDraw(ID3D11DeviceContext* native_device_context, SaintsRowIVGameDeviceData* game_device_data, const ShaderHashesList<OneShaderPerPipeline>& original_shader_hashes)
   {
      com_ptr<ID3D11RenderTargetView> rtvs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
      com_ptr<ID3D11DepthStencilView> dsv;
      native_device_context->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, &rtvs[0], &dsv);
      const UINT rt_count = UINT(std::ranges::count_if(rtvs, [](const auto& rtv)
         { return rtv.get() != nullptr; }));
      com_ptr<ID3D11DepthStencilState> depth_stencil_state;
      UINT stencil_ref = 0;
      native_device_context->OMGetDepthStencilState(&depth_stencil_state, &stencil_ref);
      D3D11_DEPTH_STENCIL_DESC depth_desc = {};
      if (depth_stencil_state)
         depth_stencil_state->GetDesc(&depth_desc);
      const bool depth_write = depth_desc.DepthEnable && depth_desc.DepthWriteMask == D3D11_DEPTH_WRITE_MASK_ALL;
      D3D11_RENDER_TARGET_VIEW_DESC rt0_desc = {};
      uint4 rt0_size = {};
      if (rtvs[0])
      {
         rtvs[0]->GetDesc(&rt0_desc);
         DXGI_FORMAT unused_format;
         GetResourceInfo(rtvs[0].get(), rt0_size, unused_format);
      }
      // G-buffer: 2-3 targets (normals, DSF, lighting); material pass: the fp16 scene alone
      const std::string_view pass = rt_count >= 2 ? "gbuffer" : (rt_count == 1 && dsv && rt0_desc.Format == DXGI_FORMAT_R16G16B16A16_FLOAT ? "scene" : (rt_count == 0 && depth_write ? "depth" : "other"));

      com_ptr<ID3D11Buffer> vs_cbs[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
      native_device_context->VSGetConstantBuffers(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, &vs_cbs[0]);
      uint32_t vs_cb_mask = 0;
      for (uint32_t i = 0; i < D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT; i++)
         vs_cb_mask |= vs_cbs[i] ? (1u << i) : 0u;
      com_ptr<ID3D11Buffer> vbs[4];
      UINT vb_strides[4] = {}, vb_offsets[4] = {};
      native_device_context->IAGetVertexBuffers(0, 4, &vbs[0], vb_strides, vb_offsets);
      com_ptr<ID3D11Buffer> ib;
      DXGI_FORMAT ib_format = DXGI_FORMAT_UNKNOWN;
      UINT ib_offset = 0;
      native_device_context->IAGetIndexBuffer(&ib, &ib_format, &ib_offset);
      D3D11_VIEWPORT viewport = {};
      UINT viewports = 1;
      native_device_context->RSGetViewports(&viewports, &viewport);
      // The motion vector target's blend slot: independent blending, or RT0 unblended
      com_ptr<ID3D11BlendState> blend_state;
      FLOAT blend_factor[4];
      UINT sample_mask = 0;
      native_device_context->OMGetBlendState(&blend_state, blend_factor, &sample_mask);
      D3D11_BLEND_DESC blend_desc = CD3D11_BLEND_DESC(D3D11_DEFAULT);
      if (blend_state)
         blend_state->GetDesc(&blend_desc);

      uint32_t line_index;
      std::vector<uint8_t> object_constants, bone_constants;
      {
         const std::lock_guard lock(game_device_data->probe_mutex);
         line_index = game_device_data->probe_draws++;
         game_device_data->probe_pass_draws[pass]++;
         if (vs_cbs[object_constants_slot])
         {
            auto& entry = GetProbeBuffer(game_device_data, vs_cbs[object_constants_slot].get());
            entry.object_draws++;
            object_constants = entry.shadow;
         }
         if (vs_cbs[bone_constants_slot])
         {
            auto& entry = GetProbeBuffer(game_device_data, vs_cbs[bone_constants_slot].get());
            entry.bone_draws++;
            bone_constants = entry.shadow;
         }
         for (int i = 1; i < 4; i++)
         {
            if (vbs[i])
               GetProbeBuffer(game_device_data, vbs[i].get()).instance_draws++;
         }
      }
      if (line_index >= probe_max_draw_lines)
         return;

      const DrawDispatchData& draw = last_draw_dispatch_data;
      reshade::log::message(reshade::log::level::info, std::format("[SR4 Probe] frame={} draw={} ctx={} deferred={} pass={} vs=0x{:08X} ps=0x{:08X} rts={} rt0_format={} rt0={}x{} dsv={} depth={} depth_write={} depth_func={} stencil={} blend0={} independent_blend={} a2c={} viewport={}x{} vs_cbs=0x{:X} vc2={} vc3={} vbs={}/{}/{}/{} strides={}/{}/{}/{} ib={} indexed={} indices={} vertices={} instances={} first_instance={}",
                                                          cb_luma_global_settings.FrameIndex, line_index, static_cast<void*>(native_device_context), native_device_context->GetType() == D3D11_DEVICE_CONTEXT_DEFERRED, pass,
                                                          original_shader_hashes.vertex_shaders[0], original_shader_hashes.pixel_shaders[0], rt_count, int(rt0_desc.Format), rt0_size.x, rt0_size.y, static_cast<void*>(dsv.get()),
                                                          bool(depth_desc.DepthEnable), depth_write, int(depth_desc.DepthFunc), bool(depth_desc.StencilEnable), bool(blend_desc.RenderTarget[0].BlendEnable), bool(blend_desc.IndependentBlendEnable), bool(blend_desc.AlphaToCoverageEnable), viewport.Width, viewport.Height, vs_cb_mask,
                                                          static_cast<void*>(vs_cbs[object_constants_slot].get()), static_cast<void*>(vs_cbs[bone_constants_slot].get()),
                                                          static_cast<void*>(vbs[0].get()), static_cast<void*>(vbs[1].get()), static_cast<void*>(vbs[2].get()), static_cast<void*>(vbs[3].get()), vb_strides[0], vb_strides[1], vb_strides[2], vb_strides[3],
                                                          static_cast<void*>(ib.get()), draw.indexed, draw.index_count, draw.vertex_count, draw.instance_count, draw.first_instance)
                                                          .c_str());

      // vc2: projTM rows c0-c3, eyePos c4, objTM rows c16-c18 (translation in .w)
      if (object_constants.size() >= 19 * 16)
      {
         const auto* c = reinterpret_cast<const float (*)[4]>(object_constants.data());
         reshade::log::message(reshade::log::level::info, std::format("[SR4 Probe] frame={} draw={} vc2: proj c0=({:.5g} {:.5g} {:.5g} {:.5g}) c1=({:.5g} {:.5g} {:.5g} {:.5g}) c2=({:.5g} {:.5g} {:.5g} {:.5g}) c3=({:.5g} {:.5g} {:.5g} {:.5g}) eye=({:.6g} {:.6g} {:.6g}) obj_t=({:.6g} {:.6g} {:.6g})",
                                                             cb_luma_global_settings.FrameIndex, line_index, c[0][0], c[0][1], c[0][2], c[0][3], c[1][0], c[1][1], c[1][2], c[1][3], c[2][0], c[2][1], c[2][2], c[2][3], c[3][0], c[3][1], c[3][2], c[3][3],
                                                             c[4][0], c[4][1], c[4][2], c[16][3], c[17][3], c[18][3])
                                                             .c_str());
      }
      else if (vs_cbs[object_constants_slot])
      {
         reshade::log::message(reshade::log::level::info, std::format("[SR4 Probe] frame={} draw={} vc2: no CPU copy (not written in the probed frame)", cb_luma_global_settings.FrameIndex, line_index).c_str());
      }

      if (game_device_data->probe_dump.is_open())
      {
         const auto dump = [&](UINT slot, const std::vector<uint8_t>& constants)
         {
            if (constants.empty())
               return;
            game_device_data->probe_dump << line_index << ",0x" << std::format("{:08X}", original_shader_hashes.vertex_shaders[0]) << ",0x" << std::format("{:08X}", original_shader_hashes.pixel_shaders[0]) << ',' << pass << ",vc" << slot;
            const float* values = reinterpret_cast<const float*>(constants.data());
            for (size_t i = 0; i < constants.size() / sizeof(float); i++)
               game_device_data->probe_dump << ',' << std::format("{:.7g}", values[i]);
            game_device_data->probe_dump << '\n';
         };
         dump(object_constants_slot, object_constants);
         dump(bone_constants_slot, bone_constants);
      }
   }

   // Per buffer read as vc2, vc3 or an instance stream in the probed frame: how the game fills it. Then clears the probe.
   static void LogProbeSummary(SaintsRowIVGameDeviceData* game_device_data)
   {
      const std::lock_guard lock(game_device_data->probe_mutex);
      const auto log = [](const std::string& line)
      { reshade::log::message(reshade::log::level::info, line.c_str()); };
      uint32_t object_buffers = 0, bone_buffers = 0, instance_buffers = 0, rewritten_instance_buffers = 0;
      for (const auto& [handle, buffer] : game_device_data->probe_buffers)
      {
         if (buffer.object_draws == 0 && buffer.bone_draws == 0 && buffer.instance_draws == 0)
            continue;
         object_buffers += buffer.object_draws != 0;
         bone_buffers += buffer.bone_draws != 0;
         instance_buffers += buffer.instance_draws != 0;
         const uint32_t writes = buffer.maps[2] + buffer.maps[3] + buffer.maps[4] + buffer.updates;
         rewritten_instance_buffers += buffer.instance_draws != 0 && writes != 0;
         // Static instance streams are many and identical in shape: only the rewritten ones get a line
         if (buffer.object_draws == 0 && buffer.bone_draws == 0 && writes == 0)
            continue;
         log(std::format("[SR4 Probe] buffer={} bytes={} usage={} cpu_access=0x{:X} bind=0x{:X} vc2_draws={} vc3_draws={} instance_draws={} maps(ro/wo/rw/discard)={}/{}/{}/{} updates={} cpu_copy={}",
            reinterpret_cast<void*>(handle), buffer.desc.ByteWidth, int(buffer.desc.Usage), buffer.desc.CPUAccessFlags, buffer.desc.BindFlags, buffer.object_draws, buffer.bone_draws, buffer.instance_draws,
            buffer.maps[1], buffer.maps[2], buffer.maps[3], buffer.maps[4], buffer.updates, !buffer.shadow.empty()));
      }
      std::string passes;
      for (const auto& [pass, draws] : game_device_data->probe_pass_draws)
         passes += std::format(" {}={}", pass, draws);
      log(std::format("[SR4 Probe] summary: draws={}{} vc2_buffers={} vc3_buffers={} instance_buffers={} rewritten_instance_buffers={} buffers_touched={} map_events={} update_events={}",
         game_device_data->probe_draws, passes, object_buffers, bone_buffers, instance_buffers, rewritten_instance_buffers, game_device_data->probe_buffers.size(), game_device_data->probe_map_events, game_device_data->probe_update_events));
      game_device_data->probe_buffers.clear();
      game_device_data->probe_pass_draws.clear();
      game_device_data->probe_draws = 0;
      game_device_data->probe_map_events = 0;
      game_device_data->probe_update_events = 0;
      game_device_data->probe_dump.close();
   }
#endif

public:
   void OnCreateDevice(ID3D11Device* native_device, DeviceData& device_data) override
   {
      device_data.game = new SaintsRowIVGameDeviceData;
      device_data.taa_detected = true; // No TAA to replace, but Core's upscaler UI checks for it
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
   DrawOrDispatchOverrideType DrawTonemapWithSMAA(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, CommandListData& cmd_list_data, DeviceData& device_data, const std::function<void()>& original_draw_dispatch_func, bool smaa)
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
      // Without "smaa" (the upscaler already antialiased the scene) only RCAS runs.
      const std::shared_lock lock_shader_objects(s_mutex_shader_objects);
      const bool sharpen = g_rcas_sharpness > 0.f && HasShaders(device_data.native_vertex_shaders, "Copy VS"_h) && HasShaders(device_data.native_pixel_shaders, "SR4 Sharpen PS"_h);
      if (!smaa && !sharpen)
         return DrawOrDispatchOverrideType::None;
      if (smaa && (!HasShaders(device_data.native_vertex_shaders, "SMAA Edge Detection VS"_h, "SMAA Blending Weight Calculation VS"_h, "SMAA Neighborhood Blending VS"_h) || !HasShaders(device_data.native_pixel_shaders, "SMAA Edge Detection PS"_h, "SMAA Blending Weight Calculation PS"_h, "SMAA Neighborhood Blending PS"_h) || !HasShaders(device_data.native_compute_shaders, "SR4 SMAA Linearize CS"_h)))
         return DrawOrDispatchOverrideType::None;

      // The scratch textures: canvas sized, single mip
      D3D11_TEXTURE2D_DESC desc = canvas_desc;
      desc.MipLevels = 1;
      desc.Usage = D3D11_USAGE_DEFAULT;
      desc.CPUAccessFlags = 0;
      desc.MiscFlags = 0;
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
         desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
         if (FAILED(native_device->CreateTexture2D(&desc, nullptr, &game_device_data.smaa_gamma_texture)) || FAILED(native_device->CreateShaderResourceView(game_device_data.smaa_gamma_texture.get(), nullptr, &game_device_data.smaa_gamma_srv)) || FAILED(native_device->CreateRenderTargetView(game_device_data.smaa_gamma_texture.get(), nullptr, &game_device_data.smaa_gamma_rtv)))
         {
            game_device_data.smaa_gamma_texture.reset();
            return DrawOrDispatchOverrideType::None;
         }
      }
      if (smaa && !game_device_data.smaa_linear_srv)
      {
         desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
         desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
         com_ptr<ID3D11Texture2D> linear_texture;
         if (FAILED(native_device->CreateTexture2D(&desc, nullptr, &linear_texture)) || FAILED(native_device->CreateUnorderedAccessView(linear_texture.get(), nullptr, &game_device_data.smaa_linear_uav)) || FAILED(native_device->CreateShaderResourceView(linear_texture.get(), nullptr, &game_device_data.smaa_linear_srv)))
         {
            game_device_data.smaa_linear_uav.reset();
            game_device_data.smaa_linear_srv.reset();
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
      bool predication_available = smaa && game_device_data.smaa_predication_uav && HasShaders(device_data.native_compute_shaders, "SR4 SMAA Predication CS"_h);
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

      if (smaa)
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

         // The SMAA shaders read the canvas size from the Luma settings and the predication scale from the Luma data, in both stages.
         SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, reshade::api::shader_stage::vertex | reshade::api::shader_stage::pixel, LumaConstantBufferType::LumaSettings);
         SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, reshade::api::shader_stage::vertex | reshade::api::shader_stage::pixel, LumaConstantBufferType::LumaData, 0, 0, depth_srv ? 2.f : 1.f);
         DrawSMAA(native_device, native_device_context, device_data, sharpen ? game_device_data.smaa_gamma_rtv.get() : canvas_rtv.get(), game_device_data.smaa_linear_srv.get(), game_device_data.smaa_gamma_srv.get(), depth_srv ? game_device_data.smaa_predication_srv.get() : nullptr);
      }
#if DEVELOPMENT
      // Calibration aid: SMAA's edge texture (red = horizontal, green = vertical edges) replaces the frame.
      if (smaa && g_smaa_edges_debug && HasShaders(device_data.native_vertex_shaders, "Copy VS"_h) && HasShaders(device_data.native_pixel_shaders, "Copy PS"_h))
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
#if DEVELOPMENT
      if (game_device_data.probe_mode != ProbeRequest::None && (stages & reshade::api::shader_stage::vertex) == reshade::api::shader_stage::vertex)
         LogProbeDraw(native_device_context, &game_device_data, original_shader_hashes);
#endif
      // The alpha test materials count as custom (Core flags their A2C patch clone), but draw with the game's shader outside of MSAA
      if (game_device_data.mv_active && (!is_custom_pass || alpha_test_material_pixel_shaders.contains(pixel_shader_hash)) && original_draw_dispatch_func && *original_draw_dispatch_func && (stages & reshade::api::shader_stage::vertex) == reshade::api::shader_stage::vertex)
      {
         if (pixel_shader_hash == downsample_pixel_shader || pixel_shader_hash == god_rays_mask_pixel_shader || (game_device_data.mv_scene_copied && post_process_pixel_shaders.contains(pixel_shader_hash)))
         {
            // The first downsample reads the finished scene, a copy of the material target
            if (pixel_shader_hash == downsample_pixel_shader && std::exchange(game_device_data.mv_scene_color_wanted, false))
            {
               com_ptr<ID3D11ShaderResourceView> scene_srv;
               native_device_context->PSGetShaderResources(0, 1, &scene_srv);
               game_device_data.mv_scene_color.reset();
               if (scene_srv)
                  scene_srv->GetResource(&game_device_data.mv_scene_color);
               if (game_device_data.mv_scene_color && uint64_t(game_device_data.mv_scene_color.get()) == game_device_data.mv_scene_copy_dest)
                  game_device_data.mv_scene_color = reinterpret_cast<ID3D11Resource*>(game_device_data.mv_scene_copy_source);
            }
            if (std::exchange(game_device_data.mv_fill_pending, false))
            {
               // Current clip space to the previous frame's: previous projTM * inverse(current), in double (absolute world translation)
               Math::Matrix44D current, previous;
               current.SetIdentity();
               previous.SetIdentity();
               if (game_device_data.mv_camera_valid && game_device_data.mv_previous_camera_valid)
               {
                  for (int i = 0; i < 16; i++)
                  {
                     current.GetData()[i] = game_device_data.mv_camera[i];
                     previous.GetData()[i] = game_device_data.mv_previous_camera[i];
                  }
                  current.Invert();
               }
               const Math::Matrix44D reprojection = previous * current;
               D3D11_TEXTURE2D_DESC mv_desc;
               game_device_data.mv_texture->GetDesc(&mv_desc);
               float constants[20] = {};
               for (int i = 0; i < 16; i++)
                  constants[i] = float(reprojection.GetData()[i]);
               constants[16] = game_device_data.mv_jitter_ndc[0];
               constants[17] = game_device_data.mv_jitter_ndc[1];
               const std::shared_lock lock_shader_objects(s_mutex_shader_objects);
               // ponytail: a shader reload between the frame start and here (DEV) leaves the FLT_MAX marker for a frame
               if (HasShaders(device_data.native_compute_shaders, "SR4 Motion Vector Fill CS"_h) && WriteConstants(native_device, native_device_context, std::addressof(game_device_data.mv_fill_buffer), constants, sizeof(constants)))
               {
                  DrawStateStack<DrawStateStackType::FullGraphics> graphics_state;
                  DrawStateStack<DrawStateStackType::Compute> compute_state;
                  graphics_state.Cache(native_device_context, device_data.uav_max_count);
                  compute_state.Cache(native_device_context, device_data.uav_max_count);
                  // The depth may be bound as the depth target, and the motion vectors as a render target
                  native_device_context->OMSetRenderTargets(0, nullptr, nullptr);
                  ID3D11Buffer* const buffer = game_device_data.mv_fill_buffer.get();
                  ID3D11ShaderResourceView* const srv = game_device_data.mv_frame_depth.get();
                  ID3D11UnorderedAccessView* const uav = game_device_data.mv_uav.get();
                  native_device_context->CSSetConstantBuffers(0, 1, &buffer);
                  native_device_context->CSSetShaderResources(0, 1, &srv);
                  native_device_context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);
                  native_device_context->CSSetShader(device_data.native_compute_shaders.at("SR4 Motion Vector Fill CS"_h).get(), nullptr, 0);
                  native_device_context->Dispatch((mv_desc.Width + 7) / 8, (mv_desc.Height + 7) / 8, 1);
                  compute_state.Restore(native_device_context);
                  graphics_state.Restore(native_device_context);
               }
            }
            if (!game_device_data.mv_frame_ended)
            {
#if DEVELOPMENT
               game_device_data.mv_frame_end_shader = pixel_shader_hash;
#endif
               if (IsSRActive(device_data))
                  DrawSuperResolution(native_device, native_device_context, device_data);
            }
            game_device_data.mv_frame_ended = true;
            game_device_data.mv_scene_open = false;
         }
         else
         {
            const bool motion_vectors = DrawWithMotionVectors(native_device, native_device_context, cmd_list_data, device_data, original_shader_hashes, *original_draw_dispatch_func);
            const bool jittered = motion_vectors || DrawWithJitter(native_device, native_device_context, cmd_list_data, device_data, original_shader_hashes, *original_draw_dispatch_func);
#if DEVELOPMENT
            // The path the probed draw (the line logged just before) actually took
            if (game_device_data.probe_mode != ProbeRequest::None && game_device_data.probe_draws <= probe_max_draw_lines)
               reshade::log::message(reshade::log::level::info, std::format("[SR4 Probe] draw={} path={} scene_open={} jitter={},{}", game_device_data.probe_draws - 1, motion_vectors ? "mv" : (jittered ? "jitter" : "none"), game_device_data.mv_scene_open, game_device_data.mv_jitter[0], game_device_data.mv_jitter[1]).c_str());
#endif
            if (jittered)
               return DrawOrDispatchOverrideType::Replaced;
         }
      }
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

         // The upscaler replaces SMAA (RCAS still sharpens its output)
         if ((g_smaa_enable || device_data.has_drawn_sr) && original_draw_dispatch_func != nullptr)
            return DrawTonemapWithSMAA(native_device, native_device_context, cmd_list_data, device_data, *original_draw_dispatch_func, !device_data.has_drawn_sr);
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
      if (DeviceData* const device_data = cmd_list->get_device()->get_private_data<DeviceData>(); device_data && device_data->game && format == reshade::api::format::r16g16b16a16_float)
         GetGameDeviceData(*device_data).msaa_scene_resolved = true;
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
      default_luma_global_game_settings.VignetteIntensity = 1.f;
      default_luma_global_game_settings.FilmGrainIntensity = 1.f;
      cb_luma_global_settings.GameSettings = default_luma_global_game_settings;

      native_shaders_definitions.emplace(CompileTimeStringHash("SR4 MSAA Resolve PS"), ShaderDefinition{"Luma_SR4_MSAAResolve", reshade::api::pipeline_subobject_type::pixel_shader});
      native_shaders_definitions.emplace(CompileTimeStringHash("SR4 Motion Vector Fill CS"), ShaderDefinition{"Luma_SR4_MotionVectorFill", reshade::api::pipeline_subobject_type::compute_shader});
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
      reshade::register_event<reshade::addon_event::map_buffer_region>(OnMapBufferRegion);
      reshade::register_event<reshade::addon_event::unmap_buffer_region>(OnUnmapBufferRegion);
#if DEVELOPMENT
      reshade::register_event<reshade::addon_event::update_buffer_region>(OnProbeUpdateBufferRegion);
      reshade::register_event<reshade::addon_event::update_buffer_region_command>(OnProbeUpdateBufferRegionCommand);
#endif
   }

   static void UnregisterEvents()
   {
      reshade::unregister_event<reshade::addon_event::create_resource>(OnCreateResource);
      reshade::unregister_event<reshade::addon_event::resolve_texture_region>(OnResolveTextureRegion);
      reshade::unregister_event<reshade::addon_event::map_buffer_region>(OnMapBufferRegion);
      reshade::unregister_event<reshade::addon_event::unmap_buffer_region>(OnUnmapBufferRegion);
#if DEVELOPMENT
      reshade::unregister_event<reshade::addon_event::update_buffer_region>(OnProbeUpdateBufferRegion);
      reshade::unregister_event<reshade::addon_event::update_buffer_region_command>(OnProbeUpdateBufferRegionCommand);
#endif
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
      reshade::get_config_value(nullptr, NAME, "VignetteIntensity", gs.VignetteIntensity);
      reshade::get_config_value(nullptr, NAME, "FilmGrainIntensity", gs.FilmGrainIntensity);
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

      // The upscaler (Super Resolution, in the Settings tab) replaces MSAA and SMAA: shown off, the saved choices are kept
      const bool sr_active = IsSRActive(device_data);
      ImGui::BeginDisabled(sr_active);
      bool msaa_shown = g_luma_msaa_enable && !sr_active;
      if (ImGui::Checkbox("Luma MSAA Enable", sr_active ? &msaa_shown : &g_luma_msaa_enable))
         reshade::set_config_value(nullptr, NAME, "MSAAResolveEnable", g_luma_msaa_enable);
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
         ImGui::SetTooltip("Replaces the game's MSAA with an HDR-aware one (smoother edges on bright lights, sky, grass, foliage and fences; requires Anti-Aliasing enabled in the game's display settings; not used with DLSS/FSR).");
      bool smaa_shown = g_smaa_enable && !sr_active;
      if (ImGui::Checkbox("SMAA Enable", sr_active ? &smaa_shown : &g_smaa_enable))
         reshade::set_config_value(nullptr, NAME, "SMAAEnable", g_smaa_enable);
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
         ImGui::SetTooltip("Adds SMAA anti-aliasing on top of the game's own (most noticeable with Anti-Aliasing off or low in the game's display settings; not used with DLSS/FSR).");
      ImGui::EndDisabled();
      if (device_data.sr_type != SR::Type::None && GetGameDeviceData(device_data).msaa_scene)
         ImGui::TextColored(ImVec4(1.f, 0.6f, 0.f, 1.f), "DLSS/FSR inactive: turn Anti-Aliasing off in the game's display settings and restart the game.");

      ImGui::BeginDisabled(!g_smaa_enable && !sr_active);
      slider("RCAS Sharpness", "RCASSharpness", &g_rcas_sharpness, 0.f, 1.f, "Sharpening applied on top of SMAA or DLSS/FSR (0 = off).");
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

      slider("Vignette Intensity", "VignetteIntensity", &gs.VignetteIntensity, default_luma_global_game_settings.VignetteIntensity, 1.f, "Scales the game's vignette darkening (1 = vanilla, 0 = none).");
      slider("Film Grain Intensity", "FilmGrainIntensity", &gs.FilmGrainIntensity, default_luma_global_game_settings.FilmGrainIntensity, 1.f, "Scales the game's film grain (1 = vanilla, 0 = off).");
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
      com_ptr<ID3D11DeviceContext> native_device_context;
      native_device->GetImmediateContext(&native_device_context);
      game_device_data.bloom_source_downsampled = false;
      game_device_data.gtao_tried_this_frame = false;
      game_device_data.gtao_ran_this_frame = false;
      // The upscaler's history restarts after any frame it didn't draw (menus, loading, just picked)
      device_data.force_reset_sr = !device_data.has_drawn_sr;
      device_data.has_drawn_sr = false;
      game_device_data.msaa_scene = std::exchange(game_device_data.msaa_scene_resolved, false);
      game_device_data.mv_active = IsSRActive(device_data) || g_mv_enable;
      // A scene no post pass ended ends here: its jitter must not reach the next frame's draws before the G-buffer
      game_device_data.mv_scene_open = false;
      game_device_data.mv_frame_ended = true;
      game_device_data.mv_scene_color_wanted = false;
      game_device_data.mv_fill_pending = false;
      if (!custom_texture_mip_lod_bias_offset)
      {
         const std::unique_lock lock(s_mutex_samplers);
         // -1 at native resolution
         device_data.texture_mip_lod_bias_offset = IsSRActive(device_data) ? SR::GetMipLODBias(device_data.output_resolution.y, device_data.output_resolution.y) : 0.f;
      }
      // Turning XeGTAO off gives its scratch back; it is rebuilt on demand.
      if (!g_gtao_enable && game_device_data.gtao_width != 0)
         game_device_data.ReleaseGTAOScratch();
#if DEVELOPMENT
      // The probe covers the draws between this present and the next, summarized at that next present
      const ProbeRequest probe_request = g_probe_request.exchange(ProbeRequest::None);
      if (game_device_data.probe_mode.exchange(probe_request) != ProbeRequest::None)
         LogProbeSummary(&game_device_data);
      if (probe_request == ProbeRequest::LogAndDump)
      {
         const std::filesystem::path dump_path = System::GetModulePath().parent_path() / std::format("Luma_SR4_Probe_{}.csv", cb_luma_global_settings.FrameIndex);
         const std::lock_guard lock(game_device_data.probe_mutex);
         game_device_data.probe_dump.open(dump_path);
         game_device_data.probe_dump << "draw,vs,ps,pass,slot,c0.x...\n";
         reshade::log::message(game_device_data.probe_dump.is_open() ? reshade::log::level::info : reshade::log::level::warning, std::format("[SR4 Probe] constants dump {}", dump_path.string()).c_str());
      }

      // "MV Debug View": Core's debug draw of the target, absolute values in pixels
      {
         const std::shared_lock lock(game_device_data.mv_mutex);
         if (g_mv_debug_view && game_device_data.mv_texture)
         {
            D3D11_TEXTURE2D_DESC desc;
            game_device_data.mv_texture->GetDesc(&desc);
            debug_draw_auto_clear_texture = false;
            debug_draw_options |= (uint32_t)DebugDrawTextureOptionsMask::Abs | (uint32_t)DebugDrawTextureOptionsMask::UVToPixelSpace;
            device_data.debug_draw_texture = game_device_data.mv_texture.get();
            device_data.debug_draw_texture_format = desc.Format;
            device_data.debug_draw_texture_size = {desc.Width, desc.Height, 1, 1};
         }
         else if (device_data.debug_draw_texture && device_data.debug_draw_texture.get() == game_device_data.mv_texture.get())
         {
            device_data.debug_draw_texture = nullptr;
         }
      }
      // The whole motion vector target: copied at a 60th frame, read at the next present
      float mv_max = -1.f;
      double mv_nonzero = 0.0;
      if (game_device_data.mv_readback_pending)
      {
         game_device_data.mv_readback_pending = false;
         D3D11_TEXTURE2D_DESC desc;
         game_device_data.mv_readback->GetDesc(&desc);
         D3D11_MAPPED_SUBRESOURCE mapped;
         if (SUCCEEDED(native_device_context->Map(game_device_data.mv_readback.get(), 0, D3D11_MAP_READ, 0, &mapped)))
         {
            mv_max = 0.f;
            size_t nonzero = 0;
            for (UINT y = 0; y < desc.Height; y++)
            {
               const float* const row = reinterpret_cast<const float*>(static_cast<const uint8_t*>(mapped.pData) + y * mapped.RowPitch);
               for (UINT x = 0; x < desc.Width; x++)
               {
                  const float largest = (std::max)(std::abs(row[x * 2]), std::abs(row[x * 2 + 1]));
                  mv_max = (std::max)(mv_max, largest);
                  // Not "> 0": the patched shader's two runs differ by rounding (~1e-7) with identical constants
                  nonzero += largest > 0.1f / float(desc.Width);
               }
            }
            mv_nonzero = double(nonzero) / (double(desc.Width) * desc.Height);
            native_device_context->Unmap(game_device_data.mv_readback.get(), 0);
         }
      }
      if (game_device_data.mv_active && cb_luma_global_settings.FrameIndex % 60 == 59)
      {
         const std::shared_lock lock(game_device_data.mv_mutex);
         if (game_device_data.mv_texture)
         {
            D3D11_TEXTURE2D_DESC desc;
            game_device_data.mv_texture->GetDesc(&desc);
            D3D11_TEXTURE2D_DESC readback_desc = {};
            if (game_device_data.mv_readback)
               game_device_data.mv_readback->GetDesc(&readback_desc);
            if (readback_desc.Width != desc.Width || readback_desc.Height != desc.Height)
            {
               game_device_data.mv_readback.reset();
               readback_desc = {desc.Width, desc.Height, 1, 1, DXGI_FORMAT_R32G32_FLOAT, {1, 0}, D3D11_USAGE_STAGING, 0, D3D11_CPU_ACCESS_READ};
               native_device->CreateTexture2D(&readback_desc, nullptr, &game_device_data.mv_readback);
            }
            if (game_device_data.mv_readback)
            {
               native_device_context->CopyResource(game_device_data.mv_readback.get(), game_device_data.mv_texture.get());
               game_device_data.mv_readback_pending = true;
            }
         }
      }
      if (game_device_data.mv_active && cb_luma_global_settings.FrameIndex % 60 == 0)
      {
         reshade::log::message(reshade::log::level::info, std::format("[SR4 MV] sample: frame camera delta {:.6g}, matched draw projTM delta {:.6g} objTM delta {:.6g}, target max |mv| {:.6g} (UV), moving (> 0.1 px) {:.1f}%, scene color 0x{:X} (copy 0x{:X} -> 0x{:X}), last rejected target 0x{:X}", game_device_data.mv_camera_delta, game_device_data.mv_sample_camera_delta, game_device_data.mv_sample_object_delta, mv_max, mv_nonzero * 100.0, uint64_t(game_device_data.mv_scene_color.get()), game_device_data.mv_scene_copy_source, game_device_data.mv_scene_copy_dest, game_device_data.mv_rejected_scene_target).c_str());
         game_device_data.mv_camera_delta = 0.f;
         game_device_data.mv_sample_camera_delta = -1.f;
         game_device_data.mv_sample_object_delta = -1.f;
         std::string rejects;
         for (size_t i = 0; i < std::size(game_device_data.mv_rejects); i++)
         {
            const uint64_t example = game_device_data.mv_reject_examples[i];
            rejects += std::format(" {}={} (vs 0x{:08X} ps 0x{:08X})", SaintsRowIVGameDeviceData::mv_reject_names[i], game_device_data.mv_rejects[i].exchange(0), uint32_t(example >> 32), uint32_t(example));
         }
         rejects += " unjittered:";
         for (size_t i = 0; i < std::size(game_device_data.mv_jitter_skips); i++)
         {
            const uint64_t example = game_device_data.mv_jitter_skip_examples[i];
            rejects += std::format(" {}={} (vs 0x{:08X} ps 0x{:08X})", SaintsRowIVGameDeviceData::mv_jitter_skip_names[i], game_device_data.mv_jitter_skips[i].exchange(0), uint32_t(example >> 32), uint32_t(example));
         }
         reshade::log::message(reshade::log::level::info, std::format("[SR4 MV] frame={} per 60 frames: mv_frames={} constant_copies={} moved={} candidates={} mv_draws={} matched={} other_camera={} off_camera={} jitter_only={} sr_draws={} sr_failures={} sr_skips={} ended_by=0x{:08X} (camera from vs 0x{:08X} ps 0x{:08X}) uncopied={} skipped_draws={} rejected:{}", cb_luma_global_settings.FrameIndex, game_device_data.mv_frames.exchange(0), game_device_data.mv_constant_copies.exchange(0), game_device_data.mv_moved_draws.exchange(0), game_device_data.mv_candidate_draws.exchange(0), game_device_data.mv_draws.exchange(0), game_device_data.mv_matched_draws.exchange(0), game_device_data.mv_other_camera_draws.exchange(0), game_device_data.mv_off_camera_draws.exchange(0), game_device_data.mv_jitter_only_draws.exchange(0), game_device_data.mv_sr_draws.exchange(0), game_device_data.mv_sr_failures.exchange(0), game_device_data.mv_sr_skips.exchange(0), game_device_data.mv_frame_end_shader, uint32_t(game_device_data.mv_camera_draw >> 32), uint32_t(game_device_data.mv_camera_draw), game_device_data.mv_uncopied_draws.exchange(0), game_device_data.mv_skipped_draws.exchange(0), rejects).c_str());
      }

      g_msaa_resolves_last_frame = std::exchange(g_msaa_resolves_this_frame, 0);
      g_finals_last_frame = std::exchange(g_finals_this_frame, 0);

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

      ImGui::SeparatorText("Motion vector research");
      ImGui::Checkbox("MV Enable", &g_mv_enable);
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Redraws the opaque main material pass with the motion vector shaders without DLSS/FSR: camera and object motion\n(each draw finds its own previous frame vc2/vc3). The image must not change; the debug view is black with a static camera\nand lights up only what moves. ReShade.log: patched/refused shaders, and per 60 frames mv_draws / matched / other_camera /\nuncopied and the rejection reasons. Not saved.");
      ImGui::Checkbox("MV Force Jitter", &g_mv_force_jitter);
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Jitters the scene (Halton 2/3, 8 phases) without an upscaler, with MV Enable. The image shakes by a subpixel; nothing\nmay flicker or lose pixels, and the debug view stays black with a static camera. Not saved.");
      ImGui::Checkbox("MV Debug View", &g_mv_debug_view);
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Shows the motion vector target (absolute, in pixels) through Core's debug draw.");
      if (ImGui::Button("Log Frame Probe"))
         g_probe_request = ProbeRequest::Log;
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Logs the next frame's draws to ReShade.log (pass, targets, depth and blend state, VS vc2/vc3 bindings with projTM,\neyePos and objTM translation, vertex buffers, draw arguments; up to %u lines), then per vc2/vc3/instance buffer\nhow the game fills it (maps by type, updates). Use it in gameplay.", probe_max_draw_lines);
      ImGui::SameLine();
      if (ImGui::Button("Log Frame Probe + Dump Constants"))
         g_probe_request = ProbeRequest::LogAndDump;
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("The same, and every draw's full vc2 / vc3 contents (as written this frame) to Luma_SR4_Probe_<frame>.csv\nnext to the executable (one row per draw and buffer).");
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
      enable_samplers_upgrade = true; // For the DLSS / FSR mip bias (see "OnPresent")

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
      SaintsRowIV::UnregisterEvents();
   }

   CoreMain(hModule, ul_reason_for_call, lpReserved);

   return TRUE;
}
