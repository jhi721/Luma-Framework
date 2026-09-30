#define GAME_SAINTS_ROW_IV 1

#define DISABLE_AUTO_DEBUGGER 1
// Alpha to coverage, the motion vector draws and SMAA wrap the game's own draws, so they need "original_draw_dispatch_func".
#define ENABLE_POST_DRAW_DISPATCH_CALLBACK 1
// Alpha to coverage also patches the alpha those draws output, into a clone that OnDrawOrDispatch swaps in per draw.
#define LUMA_PATCH_BYTECODE_SYNC 1
#define LUMA_PATCH_SYNC_MODE_CLONE 1
// SMAA runs right after the rl_hdr final composite.
#define ENABLE_SMAA 1
#define ENABLE_BLOOM 1
// The motion vector draw key reads the draw's arguments ("last_draw_dispatch_data")
#define ENABLE_DRAW_DISPATCH_DATA_CACHE 1

#include "..\..\Core\core.hpp"
#include "..\..\External\WDK\includes\d3d11TokenizedProgramFormat.hpp"
#include "MotionVectorPatches.h"
#include "..\..\Core\includes\patched_draws.h"

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
   // rl_distortion_01, replaced: it reads the distortion map through "SR4_SampleDistortionMap" like the finals (see
   // "Distortion_0xE9E18958.ps_4_0.hlsl")
   constexpr uint32_t distortion_pixel_shader = 0xE9E18958;
   // rl_bokeh_sprite_01: the aiming depth of field's bokeh sprites, added onto the output after the finals (see "DrawBlendLimited")
   constexpr uint32_t bokeh_sprite_pixel_shader = 0x4DDED58A;

   // How a draw onto the output combines with it, for "DrawBlendLimited": the vint UI's render modes additive / additive_alpha
   // (dest + src), subtractive (dest - src); multiply and plain alpha blending stay within the target's range.
   enum class OutputBlend
   {
      Other,
      Additive,
      Subtractive,
   };
   constexpr OutputBlend GetOutputBlend(const D3D11_RENDER_TARGET_BLEND_DESC& rt)
   {
      if (!rt.BlendEnable || (rt.RenderTargetWriteMask & (D3D11_COLOR_WRITE_ENABLE_RED | D3D11_COLOR_WRITE_ENABLE_GREEN | D3D11_COLOR_WRITE_ENABLE_BLUE)) == 0 || rt.DestBlend != D3D11_BLEND_ONE)
         return OutputBlend::Other;
      if (rt.BlendOp == D3D11_BLEND_OP_ADD)
         return OutputBlend::Additive;
      return rt.BlendOp == D3D11_BLEND_OP_REV_SUBTRACT ? OutputBlend::Subtractive : OutputBlend::Other;
   }
   static_assert(GetOutputBlend({TRUE, D3D11_BLEND_ONE, D3D11_BLEND_ONE, D3D11_BLEND_OP_ADD, D3D11_BLEND_ONE, D3D11_BLEND_ONE, D3D11_BLEND_OP_ADD, D3D11_COLOR_WRITE_ENABLE_ALL}) == OutputBlend::Additive);
   static_assert(GetOutputBlend({TRUE, D3D11_BLEND_SRC_ALPHA, D3D11_BLEND_ONE, D3D11_BLEND_OP_ADD, D3D11_BLEND_ONE, D3D11_BLEND_ONE, D3D11_BLEND_OP_ADD, D3D11_COLOR_WRITE_ENABLE_ALL}) == OutputBlend::Additive);
   static_assert(GetOutputBlend({TRUE, D3D11_BLEND_SRC_ALPHA, D3D11_BLEND_ONE, D3D11_BLEND_OP_REV_SUBTRACT, D3D11_BLEND_ONE, D3D11_BLEND_ONE, D3D11_BLEND_OP_ADD, D3D11_COLOR_WRITE_ENABLE_ALL}) == OutputBlend::Subtractive);
   static_assert(GetOutputBlend({TRUE, D3D11_BLEND_SRC_ALPHA, D3D11_BLEND_INV_SRC_ALPHA, D3D11_BLEND_OP_ADD, D3D11_BLEND_ONE, D3D11_BLEND_ONE, D3D11_BLEND_OP_ADD, D3D11_COLOR_WRITE_ENABLE_ALL}) == OutputBlend::Other);
   static_assert(GetOutputBlend({TRUE, D3D11_BLEND_DEST_COLOR, D3D11_BLEND_ZERO, D3D11_BLEND_OP_ADD, D3D11_BLEND_ONE, D3D11_BLEND_ONE, D3D11_BLEND_OP_ADD, D3D11_COLOR_WRITE_ENABLE_ALL}) == OutputBlend::Other);
   static_assert(GetOutputBlend({FALSE, D3D11_BLEND_ONE, D3D11_BLEND_ONE, D3D11_BLEND_OP_ADD, D3D11_BLEND_ONE, D3D11_BLEND_ONE, D3D11_BLEND_OP_ADD, D3D11_COLOR_WRITE_ENABLE_ALL}) == OutputBlend::Other);
   static_assert(GetOutputBlend({TRUE, D3D11_BLEND_ONE, D3D11_BLEND_ONE, D3D11_BLEND_OP_ADD, D3D11_BLEND_ONE, D3D11_BLEND_ONE, D3D11_BLEND_OP_ADD, D3D11_COLOR_WRITE_ENABLE_ALPHA}) == OutputBlend::Other);

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

   // Snapshots the pixel shader cbuffer bound at `slot` into `*copy`, on the GPU. The game re-uploads the same vc0 / vc4
   // buffers for every pass, so a later pass sees an earlier pass's constants only through a copy.
   void CopyBoundPSConstantBuffer(ID3D11Device* device, ID3D11DeviceContext* device_context, UINT slot, com_ptr<ID3D11Buffer>* copy)
   {
      com_ptr<ID3D11Buffer> cb;
      device_context->PSGetConstantBuffers(slot, 1, &cb);
      if (!cb)
         return;
      D3D11_BUFFER_DESC cb_desc;
      cb->GetDesc(&cb_desc);
      D3D11_BUFFER_DESC copy_desc = {};
      if (*copy)
         (*copy)->GetDesc(&copy_desc);
      if (copy_desc.ByteWidth != cb_desc.ByteWidth)
      {
         copy->reset();
         copy_desc = cb_desc;
         copy_desc.Usage = D3D11_USAGE_DEFAULT;
         copy_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
         copy_desc.CPUAccessFlags = 0;
         copy_desc.MiscFlags = 0;
         copy_desc.StructureByteStride = 0;
         device->CreateBuffer(&copy_desc, nullptr, &*copy);
      }
      if (*copy)
         device_context->CopyResource(copy->get(), cb.get());
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

   // Motion vectors for DLSS / FSR (the game renders none, see MotionVectorPatches.h): the main material pass draws every object with
   // patched shaders that also write an extra target, the vertex shader's second run reading the object's previous frame vc2 / vc3.
#if DEVELOPMENT
   bool g_mv_enable = false;
   bool g_mv_debug_view = false;
   bool g_mv_force_jitter = false;   // The projection jitter without an upscaler
   bool g_mv_disable_jitter = false; // No projection jitter under the upscaler (A/B of jitter-dependent artifacts)
   // A/B of the motion vector path's CPU and GPU savings (see "MayBeRegisteredBuffer", "NewConstantsCopy", "mv_vertex_shader_read_sizes",
   // "mv_texture")
   bool g_mv_buffer_filter = true;
   bool g_mv_constants_pool = true;
   bool g_mv_read_sizes = true;
   bool g_mv_half_float = true;
   // "Performance Test" (see "OnPresent"): the mode, and the anti-aliasing, render scale and GTAO resolution it sets while it runs (the
   // user's are restored on "Off" or "Current Settings", and never saved)
   int g_perf_test = 0;
   bool g_perf_hook_timers = true; // The hooks' CPU time (two clock reads per hooked draw, themselves a cost to measure)
   struct PerfTestMode
   {
      const char* name;
      bool set_aa = false; // Else the current settings (the fields below too)
      SR::Type sr_type = SR::Type::None;
      unsigned int dlss_preset = 0; // NVSDK_NGX_DLSS_Hint_Render_Preset (5 = E, 11 = K, ...)
      bool smaa = false;
      int motion_vector_draws = 2; // 2 patched (motion vectors and jitter), 1 jitter only, 0 untouched
      float render_scale = 0.f;    // > 0: "Render Scale (%)" while it runs, else the current one
      int gtao_full_res = -1;      // >= 0: "GTAO Full Resolution" while it runs, else the current one
   };
   constexpr PerfTestMode perf_test_modes[] = {
      {.name = "Off"},
      {.name = "Current Settings"},
      {.name = "DLSS K", .set_aa = true, .sr_type = SR::Type::DLSS, .dlss_preset = 11},
      {.name = "DLSS K 100%", .set_aa = true, .sr_type = SR::Type::DLSS, .dlss_preset = 11, .render_scale = 1.f, .gtao_full_res = 0},
      {.name = "DLSS K 67%", .set_aa = true, .sr_type = SR::Type::DLSS, .dlss_preset = 11, .render_scale = 0.67f, .gtao_full_res = 0},
      {.name = "DLSS K 50%", .set_aa = true, .sr_type = SR::Type::DLSS, .dlss_preset = 11, .render_scale = 0.5f, .gtao_full_res = 0},
      {.name = "DLSS K 100% GTAO Half Res", .set_aa = true, .sr_type = SR::Type::DLSS, .dlss_preset = 11, .render_scale = 1.f, .gtao_full_res = 1},
      {.name = "DLSS K 100% Jitter Only", .set_aa = true, .sr_type = SR::Type::DLSS, .dlss_preset = 11, .motion_vector_draws = 1, .render_scale = 1.f, .gtao_full_res = 0},
      {.name = "DLSS K 100% Without Motion Vector Draws", .set_aa = true, .sr_type = SR::Type::DLSS, .dlss_preset = 11, .motion_vector_draws = 0, .render_scale = 1.f, .gtao_full_res = 0},
      {.name = "DLSS L", .set_aa = true, .sr_type = SR::Type::DLSS, .dlss_preset = 12},
      {.name = "DLSS M", .set_aa = true, .sr_type = SR::Type::DLSS, .dlss_preset = 13},
      {.name = "DLSS E (CNN)", .set_aa = true, .sr_type = SR::Type::DLSS, .dlss_preset = 5},
      {.name = "FSR 3", .set_aa = true, .sr_type = SR::Type::FSR},
      {.name = "SMAA", .set_aa = true, .smaa = true},
      {.name = "No AA", .set_aa = true},
   };
   // "Sweep": these modes in turn, a few log windows each, over several rounds (interleaved, so the scene's drift averages out), then
   // a median per mode
   bool g_perf_sweep = false;
   constexpr int perf_sweep_modes[] = {3, 4, 5, 6, 7, 8, 14};
   static_assert(std::string_view(perf_test_modes[perf_sweep_modes[0]].name) == "DLSS K 100%" && std::string_view(perf_test_modes[perf_sweep_modes[std::size(perf_sweep_modes) - 1]].name) == "No AA");
   static_assert(std::size(perf_test_modes) <= 32); // 5 bits in "perf_settings"
   constexpr int perf_sweep_rounds = 5;
   constexpr int perf_sweep_windows = 3; // Per mode and round
   uint32_t g_pixel_steps_overrides_this_frame = 0;
   uint32_t g_pixel_steps_overrides_last_frame = 0;
   uint32_t g_sub_rect_draws_this_frame = 0;
   uint32_t g_sub_rect_draws_last_frame = 0;
#else
   constexpr bool g_mv_enable = false;
   constexpr bool g_mv_force_jitter = false;
   constexpr bool g_mv_disable_jitter = false;
   constexpr bool g_mv_buffer_filter = true;
   constexpr bool g_mv_constants_pool = true;
   constexpr bool g_mv_read_sizes = true;
   constexpr bool g_mv_half_float = true;
#endif
   // SMAA's (and the snapshot's) textures go after this many frames without SMAA (and RCAS): the upscaler replaces it
   constexpr uint32_t smaa_idle_release_frames = 600;
   // DLSS/FSR render scale (docs/SaintsRow4-DLSS-Upscaling-Research.md, method A; the game has none): while motion vectors run
   // (DLSS/FSR, or MV Enable in development), the scene window's output sized viewports and scissors shrink to a top-left sub-rect of
   // this scale, the scene's screen texture reads follow it ("IR_Pixel_Steps", "DrawSubRect"), DLSS/FSR upscale the sub-rect and the
   // depths post reads are resampled to the full target.
   float g_render_scale = 1.f;
   constexpr float min_render_scale = 0.5f;
   // The scene's screen space quads (UVs over the full target) and their sub-rect versions ("Luma_SR4_SubRectQuad.hlsl")
   constexpr std::pair<uint32_t, uint32_t> sub_rect_quad_vertex_shaders[] = {
      {0x0FFC4B94, CompileTimeStringHash("SR4 Sub Rect Light Unit Z VS")}, // Deferred lights
      {0x086E02B3, CompileTimeStringHash("SR4 Sub Rect Light VS")},
      {0x58DBDDA3, CompileTimeStringHash("SR4 Sub Rect Quad VS")}, // Full screen: rl_restore_depth, the particle depth downsample, the SSAO blur (also post)
      {0x9669662B, CompileTimeStringHash("SR4 Sub Rect SSAO VS")}, // SSAO apply
   };
   // The scene's pixel shaders that make their screen UV from NDC themselves (the sun shadow term family, reading Depth_map), patched to
   // scale it, and those that also rebuild the position from that NDC get the projection jitter out of it (see
   // "MotionVectorPatches::PatchScreenUVPixelShader"). From a census of every PS in the packfile.
   constexpr uint32_t sub_rect_uv_pixel_shaders[] = {0x1651758C, 0x5F2068F4, 0x71CC87AA, 0xB2A84B7A, 0xC4A32DC0};
   // The soft particles: every pixel shader reading "Depth_buffer" (always at t1), at a screen UV their vertex shader makes. Patched to
   // scale their t1 reads, drawn with the jitter (see "DrawWithJitter").
   constexpr uint32_t sub_rect_depth_pixel_shaders[] = {0x1E62F620, 0x287D9D19, 0x3670CDF3, 0x4EA3C638, 0x6D36434B, 0x6E727E4E, 0x704041AF, 0x916248BC, 0x97CED958, 0x98B323DC, 0x9EABB402, 0xA38F8857, 0xAE830511, 0xC26C7BF1, 0xC5593CB5, 0xD82578B1, 0xD89A814D, 0xDD59978F, 0xDDE30B96, 0xE1ACFE95, 0xE20C3CAF, 0xE6A3553A, 0xFEB7D879};
   constexpr uint32_t soft_particle_depth_slot = 1;
   // The scene's translucent materials that read the G-buffer depth (t14, soft fade) and normals (t13) at the viewport UV they make from
   // NDC themselves ((ndc + 1) * 0.5, the VFX masks and ice plus half a texel of "Target_dimensions"): the VFX masks (super sprint),
   // screen space decals and blood pools, ice, water. Patched to scale those reads. From a census of every PS in the packfile.
   constexpr uint32_t sub_rect_gbuffer_pixel_shaders[] = {0x0A0667FF, 0x0C8CA3C7, 0x361EB62F, 0x3F7B8610, 0x5D07F0FA, 0x940CFE19, 0x969065BB, 0x984B90D0, 0xB6D774D4, 0xBF6D0B8A, 0xD4FBFD7A, 0xD8A1A3C4, 0xF6B4D96E};
   constexpr uint32_t gbuffer_texture_slots = (1u << 13) | (1u << 14);
   // The full screen quads' pixel shaders that also rebuild NDC from the quad UV (the native SSAO calculates, singleframe and multiframe,
   // and the AO volumes rl_rao_calculate_box / _ellipsoid): drawn with the game's VS (UV over the viewport), their screen texture reads
   // scaled instead, by slot (the multiframe SSAO also reads its previous result, t0)
   constexpr std::pair<uint32_t, uint32_t> sub_rect_quad_ndc_pixel_shaders[] = {{0x624BF56D, gbuffer_texture_slots}, {0x1D8BB773, gbuffer_texture_slots | 1u}, {0xDA63305D, gbuffer_texture_slots}, {0x30983822, gbuffer_texture_slots}};
   // The post passes Luma doesn't replace that read the distortion map (drawn into the sub-rect with the scene) at the screen UV, with
   // its slot: rl_distortion_02, and the rl_hdr finals outside "tonemap_pixel_shaders" (those and rl_distortion_01 scale it in
   // "SR4_SampleDistortionMap")
   constexpr std::pair<uint32_t, uint32_t> sub_rect_distortion_pixel_shaders[] = {{0x8957630A, 1}, {0x7DCC8A34, 4}, {0x7FC325C0, 4}, {0x8D1F6BBB, 4}, {0xD501C191, 4}, {0xE9664111, 4}, {0xFEE7D6DC, 4}};
   // The native SSAO passes drawing into its half res targets under the sub-rect: the calculates (multiframe, and singleframe when
   // XeGTAO doesn't run) and the singleframe blurs. Past the share's edge the next pass reads (the apply's bilinear, the blur's taps,
   // the multiframe history), so the edge is repeated there after each (see "DrawSubRect").
   constexpr uint32_t sub_rect_ssao_pixel_shaders[] = {0x1D8BB773, 0x624BF56D, 0x8D425B02, 0x77A123E3};
   constexpr uint32_t sub_rect_ssao_guard_texels = 8;

   // XeGTAO over rl_ssao_singleframe_calculate (SSAO_Level 2/3). Its 4 draws (one AO channel each, into a half-res target:
   // r8g8b8a8_unorm, r16g16b16a16_float once Luma's format upgrade reaches it) become one XeGTAO run at the first draw (at the
   // target's size, the depth's with DLSS/FSR, see "RunXeGTAO"), copied into the target (created without UAV bind); the native
   // blur and apply stay. Level 1 (multiframe) stays native.
   constexpr uint32_t ssao_singleframe_calculate_pixel_shader = 0x624BF56D;
   constexpr UINT gtao_knobs_cb_slot = 9;     // "register(b9)" in Luma_SR4_XeGTAO.hlsl; b11 is core DrawBloom's
   constexpr UINT gtao_depth_mip_count = 5;   // XE_GTAO_DEPTH_MIP_LEVELS in Includes/XeGTAO.hlsl
   float g_gtao_final_value_power = 2.2f;     // DEV/TEST calibration knobs, not persisted
   float g_gtao_radius_override = 0.f;        // > 0 overrides the shader's EFFECT_RADIUS (metres)
   float g_gtao_thin_occluder_override = 0.f; // > 0 overrides the shader's THIN_OCCLUDER_COMPENSATION
#if DEVELOPMENT
   int g_gtao_debug_view = 0; // 0=off 1=depth gradient 2=normals 3=AO x8 4=edges
   // A/B of the DLSS/FSR mode (see "RunXeGTAO"): 0 = auto (with the upscaler), 1 = off, 2 = on
   int g_gtao_noise_per_frame = 0;
   int g_gtao_single_denoise = 0;
   int g_gtao_full_res = 0;
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
   uint32_t g_mv_tiebreak_collisions_last_frame = 0; // See "PatchedDraws::CountTieBreakCollisions"
#endif

   // A patched pixel shader's kind, each cached apart (see "GetMotionVectorShader")
   enum class PixelShaderPatch : uint8_t
   {
      MotionVectors,
      ScreenUV,              // Render scale (and the sun shadow terms' jitter), see the "sub_rect_*_pixel_shaders"
      MotionVectorsScreenUV, // The same on top of the motion vector patch
   };
} // namespace

// Everything that holds a device object, so it is released with its device.
struct SaintsRowIVGameDeviceData final : public GameDeviceData
{
   // SMAA and RCAS input: a snapshot of the gamma canvas (SMAA writes the canvas, so it cannot also sample it; its blend filters the
   // snapshot in linear light itself). Recreated when the canvas size or format changes. With RCAS on after SMAA, the snapshot is
   // retaken from SMAA's output and RCAS sharpens it back into the canvas.
   com_ptr<ID3D11Texture2D> smaa_gamma_texture;
   com_ptr<ID3D11ShaderResourceView> smaa_gamma_srv;
   com_ptr<ID3D11UnorderedAccessView> smaa_predication_uav;
   com_ptr<ID3D11ShaderResourceView> smaa_predication_srv;
   // The frames SMAA and the snapshot's users (SMAA or RCAS) last ran, for "smaa_idle_release_frames"
   uint32_t smaa_frame = 0;
   uint32_t snapshot_frame = 0;

   // The output before an additive or subtractive draw, and the blend op MIN / MAX states that limit it (see "DrawBlendLimited")
   com_ptr<ID3D11Texture2D> blend_limit_base_texture;
   com_ptr<ID3D11ShaderResourceView> blend_limit_base_srv;
   com_ptr<ID3D11BlendState> blend_limit_states[2];

   // GPU copy of the final composite's vc4 (Tint_saturation c0, Tint_color c1). The game's eye-adaptation exposure lives in
   // Tint_color, applied only in the composite, so the MSAA scene is unexposed; the weighted resolve reads last frame's copy
   // to weight samples in the displayed exposure. Copied only while Luma MSAA is on; null until then (the resolve shader
   // falls back to a box resolve on the zeroed binding).
   com_ptr<ID3D11Buffer> composite_tint_cb;
   // The game resolved an MSAA scene this frame / last frame (its Anti-Aliasing display setting)
   bool msaa_scene_resolved = false;
   bool msaa_scene = false;
   // "DrawUpscaler" made a new upscaler output texture (see there)
   bool sr_output_recreated = false;
   // "IsSRActive", taken at present (see there)
   bool sr_active = false;
   // None was picked ("CleanExtraSRResources", from the overlay): the motion vector and render scale resources go at the next present
   std::atomic<bool> release_sr_resources = false;

   // GPU copies of the native bloom's constants, taken at their own draws earlier in the same frame: the brightpass vc0
   // (bound at b0 for the prefilter) and vc4 (b4), the combine vc4 (b5) and the source downsample vc4 (b6). Copied only
   // while Luma bloom is on; null until those passes first draw.
   com_ptr<ID3D11Buffer> bloom_brightpass_vc0_cb;
   com_ptr<ID3D11Buffer> bloom_brightpass_vc4_cb;
   com_ptr<ID3D11Buffer> bloom_combine_vc4_cb;
   com_ptr<ID3D11Buffer> bloom_source_downsample_vc4_cb;
   bool bloom_source_downsampled = false;

   // XeGTAO scratch.
   bool gtao_tried_this_frame = false;               // the frame's first calculate draw decided XeGTAO or native
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
   com_ptr<ID3D11Buffer> gtao_knobs_cb; // dynamic, rewritten every run (the noise index changes per frame with DLSS/FSR)

   // Motion vectors: shaders patched on first use, by original hash (null on failure), and the target (sized like the scene; every
   // blend state writes it unblended, see "OnCreateBlendState")
   std::shared_mutex mv_mutex;
   std::unordered_map<uint32_t, com_ptr<ID3D11VertexShader>> mv_vertex_shaders;
   std::array<std::unordered_map<uint32_t, com_ptr<ID3D11PixelShader>>, 3> mv_pixel_shaders; // By "PixelShaderPatch"
   com_ptr<ID3D11Texture2D> mv_texture;
   com_ptr<ID3D11RenderTargetView> mv_rtv;
   com_ptr<ID3D11UnorderedAccessView> mv_uav; // Null without typed UAV loads of its format (then no fill)
   // The frame's G-buffer depth (taken at the frame start): the upscaler's depth and the camera motion fill's (see
   // "Luma_SR4_MotionVectorFill.hlsl"; when the fill will run, the frame start marks the target with the fill's marker)
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
   // The last resource copy since the G-buffer (not referenced, only compared)
   uint64_t mv_scene_copy_source = 0;
   uint64_t mv_scene_copy_dest = 0;
   bool mv_scene_copied = false; // This frame's material target copy happened
   // The projection jitter (pixels, +y down), chosen when the G-buffer opens the scene; its NDC offset is at VS
   // "MotionVectorPatches::jitter_slot" of every mesh draw depth tested against the scene until the first post pass
   std::array<float, 2> mv_jitter = {};
   std::array<float, 2> mv_jitter_ndc = {}; // The same offset in NDC (y up), as the jitter buffer holds it
   com_ptr<ID3D11Buffer> mv_jitter_buffer;
   std::array<uint32_t, 2> mv_render_size = {}; // The scene's pixels: the render scale's sub-rect, else the whole target
   // Per-draw lookups kept for the next draw. Views and states (reset when the scene opens, they can be recreated between scenes):
   // the jitter path's last depth view and whether it's output sized, and its last depth stencil state's depth test; the motion
   // vector path's last accepted scene targets (the motion vector target is built for them) and the last blend state's opacity
   // (null = the default state, opaque). Shaders: the last vertex and pixel shader's patched versions, the jitter path's pixel
   // shader apart (owned by "mv_vertex_shaders" / "mv_pixel_shaders", never erased), and the vertex shader's
   // "mv_bone_vertex_shaders" and "mv_resourceless_vertex_shaders" membership.
   ID3D11DepthStencilView* jitter_dsv = nullptr;
   bool jitter_dsv_scene_sized = false;
   ID3D11DepthStencilState* jitter_depth_stencil_state = nullptr;
   bool jitter_depth_test = true;
   ID3D11RenderTargetView* mv_accepted_rtv = nullptr;
   ID3D11DepthStencilView* mv_accepted_dsv = nullptr;
   ID3D11BlendState* mv_blend_state = nullptr;
   bool mv_blend_opaque = true;
   uint32_t mv_last_vertex_shader_hash = 0;
   ID3D11VertexShader* mv_last_vertex_shader = nullptr;
   bool mv_last_vertex_shader_skinned = false;
   bool mv_last_vertex_shader_resourceless = false;
   uint32_t mv_last_pixel_shader_hash = 0;
   PixelShaderPatch mv_last_pixel_shader_patch = PixelShaderPatch::MotionVectors;
   ID3D11PixelShader* mv_last_pixel_shader = nullptr;
   uint32_t jitter_last_pixel_shader_hash = 0;
   ID3D11PixelShader* jitter_last_pixel_shader = nullptr;
   PatchedDraws::BoundShader<ID3D11VertexShader> mv_bound_vertex_shader;
   PatchedDraws::BoundShader<ID3D11PixelShader> mv_bound_pixel_shader;
   // The patched vertex shaders that read vc3 (skinned), and those shown to read no resource (the others' second run needs them at
   // "MotionVectorPatches::previous_resources_slot"), by original hash
   std::unordered_set<uint32_t> mv_bone_vertex_shaders;
   std::unordered_set<uint32_t> mv_resourceless_vertex_shaders;
   // The bytes of vc2 / vc3 each patched vertex shader reads ("DXBC::ConstantBufferBytes": the previous frame's copies upload only
   // those), by original hash, and the last one's (see "mv_last_vertex_shader_hash")
   std::unordered_map<uint32_t, std::array<UINT, std::size(MotionVectorPatches::previous_slots)>> mv_vertex_shader_read_sizes;
   std::array<UINT, std::size(MotionVectorPatches::previous_slots)> mv_last_vertex_shader_read_sizes = {};

   // CPU copies of the vc2 / vc3 buffers the motion vector draws bind (one shared buffer each, mapped with discard every few draws;
   // draws in between reuse the last contents), by buffer (an entry registers it, null until its first Unmap): each Map's pointer,
   // copied at Unmap into a new snapshot the draws share
   using ConstantsCopy = std::shared_ptr<const std::vector<uint8_t>>;
   std::shared_mutex mv_constants_mutex;
   std::unordered_map<uint64_t, void*> mv_mapped_constants;
   std::unordered_map<uint64_t, ConstantsCopy> mv_constants_copies;
   // Render scale: the scene draws' PS vc4 buffers, whose "IR_Pixel_Steps" (c9) is rewritten at Unmap, and the last one registered
   // (read without the lock, cleared when that buffer is destroyed)
   std::unordered_set<uint64_t> sub_rect_vc4_buffers;
   std::atomic<ID3D11Buffer*> sub_rect_last_vc4 = nullptr;
   // The first registered buffers (vc2 / vc3 copies, render scale vc4) and their sizes again, for the Map hooks to skip the game's
   // other buffers without the lock and the lookups (see "MayBeRegisteredBuffer"). Written under "mv_constants_mutex"; with more
   // registered, every buffer takes the lock.
   static constexpr uint32_t max_filtered_buffers = 32;
   static constexpr uint64_t destroyed_buffer_slot = 1; // A destroyed buffer's slot, reused by the next registration (never a handle)
   std::array<std::atomic<uint64_t>, max_filtered_buffers> mv_filtered_buffers = {};
   std::array<std::atomic<UINT>, max_filtered_buffers> mv_filtered_buffer_sizes = {};
   std::atomic<uint32_t> mv_filtered_buffer_count = 0; // Every registered buffer, past "max_filtered_buffers" too
   // Every copy made, and by size those nobody held anymore at the last present (the next copies take them, see "NewConstantsCopy").
   // Every copy is in the pool, so one with a use count of 2 is held by "mv_constants_copies" and the pool alone. Under
   // "mv_constants_mutex".
   std::vector<std::shared_ptr<std::vector<uint8_t>>> mv_constants_pool;
   std::unordered_map<size_t, std::vector<uint32_t>> mv_constants_pool_free;
   size_t mv_constants_made = 0;          // Copies asked for since the last present
   std::vector<uint8_t> mv_camera_upload; // Scratch: an unmatched draw's vc2 with last frame's camera
   // Previous frame constants of the motion vector draws (see "PatchedDraws::PreviousConstants")
   PatchedDraws::PreviousConstants mv_previous_constants;
   // Motion vector draws by draw key (shaders, buffers, arguments), with the objTM translation and vc2 / vc3. A draw takes the
   // previous frame's constants of its key's nearest draw (same object, a frame earlier).
   struct MotionVectorObject
   {
      std::array<float, 3> translation;
      ConstantsCopy object; // vc2
      ConstantsCopy bones;  // vc3, skinned draws only
   };
   std::unordered_map<uint64_t, std::vector<MotionVectorObject>> mv_objects;
   std::unordered_map<uint64_t, std::vector<MotionVectorObject>> mv_previous_objects;

   // Render scale (see "g_render_scale"): whether the scaled viewport and scissor are still bound (until the game binds its own, see
   // "OnBindViewports"; a value compare can't tell them apart, the half res SSAO viewport equals the sub-rect at 0.5) and the game's
   // they replaced (put back when the scene ends)
   D3D11_VIEWPORT sub_rect_game_viewport = {};
   D3D11_RECT sub_rect_game_scissor = {};
   bool sub_rect_bound = false;
   bool sub_rect_scissor_bound = false;
   // The frame depth post reads ("mv_frame_depth": the final composite's t5, DoF, god rays) resampled over the full target after the
   // upscaler: a copy of it to read, a view to write it, and the state that writes it unconditionally
   com_ptr<ID3D11Texture2D> sub_rect_depth_copy;
   com_ptr<ID3D11ShaderResourceView> sub_rect_depth_copy_srv;
   com_ptr<ID3D11DepthStencilView> sub_rect_depth_dsv;
   com_ptr<ID3D11DepthStencilState> sub_rect_depth_write_state;
   // A frame without the upscaler under the sub-rect (see "ResolveScene"): a copy of the scene to read and a view to write it
   com_ptr<ID3D11Texture2D> sub_rect_color_copy;
   com_ptr<ID3D11ShaderResourceView> sub_rect_color_copy_srv;
   com_ptr<ID3D11RenderTargetView> sub_rect_scene_rtv;
   // The frame's camera (vc2 projTM of its first draw) and the previous frame's
   std::array<float, 16> mv_camera = {};
   std::array<float, 16> mv_previous_camera = {};
   bool mv_camera_valid = false;
   bool mv_previous_camera_valid = false;
   uint32_t mv_frame_index = 0; // The Luma frame index of the last motion vector frame

#if DEVELOPMENT
   // "Performance Test": GPU timestamps per frame (present to present, the scene from the G-buffer to its first post pass, the
   // upscaler), all on the immediate context, in a ring read back a few frames later without waiting; and the CPU time in the
   // motion vector hooks
   struct PerfQueries
   {
      com_ptr<ID3D11Query> disjoint, frame_start, scene_start, scene_end, sr_end, frame_end;
      bool scene_started = false; // scene_start issued
      bool scene = false;         // ... and scene_end
      bool sr = false;            // ... and sr_end
      bool pending = false;
   };
   struct PerfStats
   {
      double frame_ms = 0.0, frame_max_ms = 0.0, scene_ms = 0.0, scene_max_ms = 0.0, sr_ms = 0.0, sr_max_ms = 0.0, cpu_frame_ms = 0.0;
      uint32_t samples = 0, scene_samples = 0, sr_samples = 0, disjoint = 0, frames = 0;
   };
   std::array<PerfQueries, 8> perf_queries;
   size_t perf_query_index = 0;
   PerfQueries* perf_frame_queries = nullptr; // This frame's, from present to present
   PerfStats perf_stats;                      // This log window's
   int perf_settle_frames = 0;                // Frames skipped after a change (targets rebuilt, history reset)
   uint32_t perf_settings = 0;                // The measured settings, to restart the settle on a change
   std::chrono::steady_clock::time_point perf_last_present;
   std::atomic<int64_t> perf_hook_ns = 0; // This log window's
   // The user's anti-aliasing, render scale and GTAO resolution, while a mode that sets its own runs
   SR::Type perf_user_sr_type = SR::Type::None;
   unsigned int perf_user_dlss_preset = 0;
   bool perf_user_smaa = false;
   float perf_user_render_scale = 1.f;
   int perf_user_gtao_full_res = 0;
   // "Sweep": the step over all rounds, the log windows done in it, and per mode each window's frame, scene, SR and hook times
   int perf_sweep_step = 0;
   int perf_sweep_windows_done = 0;
   std::vector<std::array<double, 4>> perf_sweep_results[std::size(perf_test_modes)];
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
};

class SaintsRowIV final : public Game
{
   static SaintsRowIVGameDeviceData& GetGameDeviceData(DeviceData& device_data)
   {
      return *static_cast<SaintsRowIVGameDeviceData*>(device_data.game);
   }

#if DEVELOPMENT
   // "Performance Test": adds the scope's CPU time to the motion vector hooks' total, while the test runs
   struct PerfHookTimer
   {
      std::atomic<int64_t>& total_ns;
      const bool enabled = g_perf_test != 0 && g_perf_hook_timers;
      const std::chrono::steady_clock::time_point start = (enabled ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{});
      ~PerfHookTimer()
      {
         if (enabled)
            total_ns += std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count();
      }
   };
#endif

#if DEVELOPMENT
   // A mode that sets an upscaler this GPU doesn't have can't run
   static bool IsPerfTestModeAvailable(const DeviceData& device_data, const PerfTestMode& mode)
   {
      return !mode.set_aa || mode.sr_type == SR::Type::None || device_data.sr_implementations_instances.contains(mode.sr_type);
   }
   // "Performance Test": switches to a mode, setting its anti-aliasing as Core's "Super Resolution" and "DLSS Preset" selection do, and
   // its render scale and GTAO resolution (without saving), keeping the user's while any mode that sets its own runs and restoring them after
   static void ApplyPerfTestMode(DeviceData& device_data, int mode_index)
   {
      auto& game_device_data = GetGameDeviceData(device_data);
      const PerfTestMode& mode = perf_test_modes[mode_index];
      const PerfTestMode& previous_mode = perf_test_modes[g_perf_test];
      if (!previous_mode.set_aa && mode.set_aa)
      {
         game_device_data.perf_user_sr_type = device_data.sr_type;
         game_device_data.perf_user_dlss_preset = dlss_render_preset;
         game_device_data.perf_user_smaa = g_smaa_enable;
         game_device_data.perf_user_render_scale = g_render_scale;
         game_device_data.perf_user_gtao_full_res = g_gtao_full_res;
      }
      if (mode.set_aa || previous_mode.set_aa)
      {
         SetSRType(device_data, mode.set_aa ? mode.sr_type : game_device_data.perf_user_sr_type);
         device_data.sr_suppressed = false;
         dlss_render_preset = ((mode.set_aa && mode.sr_type == SR::Type::DLSS) ? mode.dlss_preset : game_device_data.perf_user_dlss_preset);
         g_smaa_enable = (mode.set_aa ? mode.smaa : game_device_data.perf_user_smaa);
         g_render_scale = (mode.render_scale > 0.f ? mode.render_scale : game_device_data.perf_user_render_scale);
         g_gtao_full_res = (mode.gtao_full_res >= 0 ? mode.gtao_full_res : game_device_data.perf_user_gtao_full_res);
      }
      g_perf_test = mode_index;
   }
#endif

   // An upscaler is picked, hasn't failed (it then gives way to SMAA until picked again) and the scene isn't the game's MSAA one (no
   // motion vectors there: the upscaler steps aside while it lasts). Fixed for the whole frame (see "OnPresent"): a selection made
   // after the render scale and the motion vector state were set would otherwise run the upscaler, XeGTAO and post on mixed state
   static bool IsSRActive(DeviceData& device_data)
   {
      return GetGameDeviceData(device_data).sr_active;
   }

   // The render scale, 1 when it's off (see "g_render_scale")
   static float GetSubRectScale(DeviceData& device_data)
   {
      return (g_render_scale < 1.f && GetGameDeviceData(device_data).mv_active) ? g_render_scale : 1.f;
   }

   // Registers a buffer for the Map hooks' lock free filter ("MayBeRegisteredBuffer"), with its size. Under "mv_constants_mutex".
   static void RegisterFilteredBuffer(SaintsRowIVGameDeviceData* game_device_data, ID3D11Buffer* buffer)
   {
      // A destroyed buffer's slot, else the next one (the size before the handle, which the hooks read first)
      const uint32_t count = game_device_data->mv_filtered_buffer_count.load(std::memory_order_relaxed);
      uint32_t slot = 0;
      while (slot < (std::min)(count, SaintsRowIVGameDeviceData::max_filtered_buffers) && game_device_data->mv_filtered_buffers[slot].load(std::memory_order_relaxed) != SaintsRowIVGameDeviceData::destroyed_buffer_slot)
         slot++;
      if (slot < SaintsRowIVGameDeviceData::max_filtered_buffers)
      {
         D3D11_BUFFER_DESC desc;
         buffer->GetDesc(&desc);
         game_device_data->mv_filtered_buffer_sizes[slot].store(desc.ByteWidth, std::memory_order_relaxed);
         game_device_data->mv_filtered_buffers[slot].store(reinterpret_cast<uint64_t>(buffer), std::memory_order_release);
      }
      // Past the list, the count still grows: every buffer then takes the lock
      if (slot >= count)
      {
         game_device_data->mv_filtered_buffer_count.store(count + 1, std::memory_order_release);
      }
   }

   // False if the buffer surely isn't a registered one; "size" its size if known (else 0). Lock free: the Map hooks see every Map of
   // the game (vc0-vc4 are mapped with discard many times a frame, most of them other buffers).
   static bool MayBeRegisteredBuffer(const SaintsRowIVGameDeviceData& game_device_data, uint64_t handle, UINT* size)
   {
      *size = 0;
      const uint32_t count = game_device_data.mv_filtered_buffer_count.load(std::memory_order_acquire);
      if (!g_mv_buffer_filter || count > SaintsRowIVGameDeviceData::max_filtered_buffers)
         return true;
      for (uint32_t i = 0; i < count; i++)
      {
         if (game_device_data.mv_filtered_buffers[i].load(std::memory_order_acquire) == handle)
         {
            *size = game_device_data.mv_filtered_buffer_sizes[i].load(std::memory_order_relaxed);
            return true;
         }
      }
      return false;
   }

   // A vc2 / vc3 buffer's CPU copy (null until its first Unmap); registers it for a copy at every Unmap
   static SaintsRowIVGameDeviceData::ConstantsCopy GetConstantsCopy(SaintsRowIVGameDeviceData* game_device_data, ID3D11Buffer* buffer)
   {
      if (!buffer)
         return nullptr;
      const std::lock_guard lock(game_device_data->mv_constants_mutex);
      const auto [entry, registered] = game_device_data->mv_constants_copies.try_emplace(reinterpret_cast<uint64_t>(buffer));
      if (registered && !game_device_data->sub_rect_vc4_buffers.contains(reinterpret_cast<uint64_t>(buffer)))
      {
         RegisterFilteredBuffer(game_device_data, buffer);
      }
      return entry->second;
   }

   // A vc2 / vc3 copy of "size" bytes from "bytes": one of that size nobody held anymore at the last present, else a new one. Under
   // "mv_constants_mutex".
   static std::shared_ptr<std::vector<uint8_t>> NewConstantsCopy(SaintsRowIVGameDeviceData* game_device_data, const uint8_t* bytes, size_t size)
   {
      game_device_data->mv_constants_made++;
      if (g_mv_constants_pool)
      {
         if (const auto free_copies = game_device_data->mv_constants_pool_free.find(size); free_copies != game_device_data->mv_constants_pool_free.end() && !free_copies->second.empty())
         {
            auto copy = game_device_data->mv_constants_pool[free_copies->second.back()];
            free_copies->second.pop_back();
            std::memcpy(copy->data(), bytes, size);
            return copy;
         }
      }
      auto copy = std::make_shared<std::vector<uint8_t>>(bytes, bytes + size);
      // Pooled even without "g_mv_constants_pool", which only gates the reuse: the in-place rewrite relies on it (see "mv_constants_pool")
      game_device_data->mv_constants_pool.push_back(copy);
      return copy;
   }

   static void OnMapBufferRegion(reshade::api::device* device, reshade::api::resource resource, uint64_t offset, uint64_t size, reshade::api::map_access access, void** data)
   {
      DeviceData* const device_data = device->get_private_data<DeviceData>();
      if (device_data && device_data->game && GetGameDeviceData(*device_data).mv_active && access == reshade::api::map_access::write_discard && offset == 0 && data && *data)
      {
         auto& game_device_data = GetGameDeviceData(*device_data);
#if DEVELOPMENT
         const PerfHookTimer timer{game_device_data.perf_hook_ns};
#endif
         UINT unused_size = 0;
         if (!MayBeRegisteredBuffer(game_device_data, resource.handle, &unused_size))
            return;
         const std::lock_guard lock(game_device_data.mv_constants_mutex);
         if (game_device_data.mv_constants_copies.contains(resource.handle) || game_device_data.sub_rect_vc4_buffers.contains(resource.handle))
            game_device_data.mv_mapped_constants[resource.handle] = *data;
      }
   }

   // Before a registered buffer's Unmap (the game has written it): the render scale's vc4 rewrite, and the motion vectors' CPU copy
   // of a vc2 / vc3 buffer, read back from the mapped memory.
   static void OnUnmapBufferRegion(reshade::api::device* device, reshade::api::resource resource)
   {
      DeviceData* const device_data = device->get_private_data<DeviceData>();
      if (device_data && device_data->game && GetGameDeviceData(*device_data).mv_active)
      {
         auto& game_device_data = GetGameDeviceData(*device_data);
#if DEVELOPMENT
         const PerfHookTimer timer{game_device_data.perf_hook_ns};
#endif
         UINT buffer_size = 0;
         if (!MayBeRegisteredBuffer(game_device_data, resource.handle, &buffer_size))
            return;
         const std::lock_guard lock(game_device_data.mv_constants_mutex);
         if (const auto mapped = game_device_data.mv_mapped_constants.find(resource.handle); mapped != game_device_data.mv_mapped_constants.end())
         {
            if (buffer_size == 0)
            {
               D3D11_BUFFER_DESC desc;
               reinterpret_cast<ID3D11Buffer*>(resource.handle)->GetDesc(&desc);
               buffer_size = desc.ByteWidth;
            }
            auto* const mapped_data = static_cast<uint8_t*>(mapped->second);
            // Render scale: the material pass maps its screen texel to UV as ((ndc + 1) / 2 * c9.zw + 0.5) * c9.xy (y flipped), so
            // zw = the render size puts it in the sub-rect, xy stays 1 / the full texture size. Only the output sized steps (reflections
            // have their own). The G-buffer's stipple (LOD fades, translucents) samples its pattern at the viewport UV times
            // "IR_Stipple_Repeat_Info" (c12.xy, pattern repeats over the target): scaled alike, so its pixels keep the parity the material
            // pass's DSF expects.
            if (const float scale = GetSubRectScale(*device_data); scale < 1.f && buffer_size >= 10 * 16 && game_device_data.sub_rect_vc4_buffers.contains(resource.handle))
            {
               float pixel_steps[4];
               std::memcpy(pixel_steps, mapped_data + 9 * 16, sizeof(pixel_steps));
               if (pixel_steps[2] == device_data->output_resolution.x && pixel_steps[3] == device_data->output_resolution.y)
               {
                  pixel_steps[2] = std::round(pixel_steps[2] * scale);
                  pixel_steps[3] = std::round(pixel_steps[3] * scale);
                  std::memcpy(mapped_data + 9 * 16, pixel_steps, sizeof(pixel_steps));
#if DEVELOPMENT
                  g_pixel_steps_overrides_this_frame++;
#endif
                  if (buffer_size >= 13 * 16)
                  {
                     float stipple_repeat[2];
                     std::memcpy(stipple_repeat, mapped_data + 12 * 16, sizeof(stipple_repeat));
                     stipple_repeat[0] *= pixel_steps[2] / device_data->output_resolution.x;
                     stipple_repeat[1] *= pixel_steps[3] / device_data->output_resolution.y;
                     std::memcpy(mapped_data + 12 * 16, stipple_repeat, sizeof(stipple_repeat));
                  }
               }
            }
            if (const auto copy = game_device_data.mv_constants_copies.find(resource.handle); copy != game_device_data.mv_constants_copies.end())
            {
               // The buffer's last copy, rewritten in place when no draw or object holds it (only the map and the pool do): most Maps go
               // to buffers no motion vector draw read since. Made non-const ("NewConstantsCopy"), so writing it is defined.
               if (g_mv_constants_pool && copy->second && copy->second.use_count() == 2 && copy->second->size() == buffer_size)
               {
                  std::memcpy(const_cast<uint8_t*>(copy->second->data()), mapped_data, buffer_size);
               }
               else
               {
                  copy->second = NewConstantsCopy(&game_device_data, mapped_data, buffer_size);
               }
            }
            game_device_data.mv_mapped_constants.erase(mapped);
         }
      }
   }

   // Motion vectors, render scale: a destroyed registered buffer leaves the registry and the filter (its address can come back as
   // another buffer, of another size)
   static void OnDestroyResource(reshade::api::device* device, reshade::api::resource resource)
   {
      DeviceData* const device_data = device->get_private_data<DeviceData>();
      if (!device_data || !device_data->game)
         return;
      auto& game_device_data = GetGameDeviceData(*device_data);
      UINT unused_size = 0;
      if (!MayBeRegisteredBuffer(game_device_data, resource.handle, &unused_size))
         return;
      const std::lock_guard lock(game_device_data.mv_constants_mutex);
      const bool registered = (game_device_data.mv_constants_copies.erase(resource.handle) + game_device_data.sub_rect_vc4_buffers.erase(resource.handle)) != 0;
      game_device_data.mv_mapped_constants.erase(resource.handle);
      if (!registered)
         return;
      ID3D11Buffer* destroyed = reinterpret_cast<ID3D11Buffer*>(resource.handle);
      game_device_data.sub_rect_last_vc4.compare_exchange_strong(destroyed, nullptr, std::memory_order_relaxed);
      const uint32_t count = (std::min)(game_device_data.mv_filtered_buffer_count.load(std::memory_order_relaxed), SaintsRowIVGameDeviceData::max_filtered_buffers);
      for (uint32_t i = 0; i < count; i++)
      {
         if (game_device_data.mv_filtered_buffers[i].load(std::memory_order_relaxed) == resource.handle)
         {
            game_device_data.mv_filtered_buffers[i].store(SaintsRowIVGameDeviceData::destroyed_buffer_slot, std::memory_order_release);
            break;
         }
      }
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

   // The bound shader's motion vector version (a pixel shader's: of "pixel_shader_patch"), patched from Core's bytecode copy on
   // first use (null if it can't be)
   template <typename T>
   static com_ptr<T> GetMotionVectorShader(ID3D11Device* native_device, DeviceData& device_data, uint32_t hash, reshade::api::pipeline pipeline, PixelShaderPatch pixel_shader_patch = PixelShaderPatch::MotionVectors)
   {
      constexpr bool vertex = std::is_same_v<T, ID3D11VertexShader>;
      auto& game_device_data = GetGameDeviceData(device_data);
      auto& shaders = [&]() -> std::unordered_map<uint32_t, com_ptr<T>>&
      {
         if constexpr (vertex)
            return game_device_data.mv_vertex_shaders;
         else
            return game_device_data.mv_pixel_shaders[size_t(pixel_shader_patch)];
      }();
      {
         const std::shared_lock lock(game_device_data.mv_mutex);
         if (const auto it = shaders.find(hash); it != shaders.end())
            return it->second;
      }
      std::vector<uint8_t> patched;
      std::string error = "no bytecode";
      bool screen_space = false;
      bool screen_uv_miss = false;
      {
         const std::shared_lock lock(s_mutex_generic);
         if (const auto it = device_data.pipeline_cache_by_pipeline_handle.find(pipeline.handle); it != device_data.pipeline_cache_by_pipeline_handle.end() && it->second->subobjects_cache)
         {
            const auto* desc = static_cast<const reshade::api::shader_desc*>(it->second->subobjects_cache[0].data);
            const auto* code = static_cast<const uint8_t*>(desc->code);
            if constexpr (vertex)
            {
               patched = MotionVectorPatch::PatchVertexShader(code, desc->code_size, MotionVectorPatches::layout, &error);
            }
            else
            {
               // The screen textures read over the full target, by slot (see the "sub_rect_*_pixel_shaders")
               const auto distortion = std::ranges::find(sub_rect_distortion_pixel_shaders, hash, &std::pair<uint32_t, uint32_t>::first);
               const auto quad_ndc = std::ranges::find(sub_rect_quad_ndc_pixel_shaders, hash, &std::pair<uint32_t, uint32_t>::first);
               uint32_t texture_slots = MotionVectorPatches::no_texture;
               if (std::ranges::contains(sub_rect_depth_pixel_shaders, hash))
                  texture_slots = 1u << soft_particle_depth_slot;
               else if (distortion != std::end(sub_rect_distortion_pixel_shaders))
                  texture_slots = 1u << distortion->second;
               else if (quad_ndc != std::end(sub_rect_quad_ndc_pixel_shaders))
                  texture_slots = quad_ndc->second;
               else if (std::ranges::contains(sub_rect_gbuffer_pixel_shaders, hash))
                  texture_slots = gbuffer_texture_slots;
               if (pixel_shader_patch == PixelShaderPatch::ScreenUV)
               {
                  patched = MotionVectorPatches::PatchScreenUVPixelShader(code, desc->code_size, texture_slots, &error);
               }
               else
               {
                  patched = MotionVectorPatch::PatchPixelShader(code, desc->code_size, MotionVectorPatches::layout, &error);
                  if (pixel_shader_patch == PixelShaderPatch::MotionVectorsScreenUV && !patched.empty())
                     patched = MotionVectorPatches::PatchScreenUVPixelShader(patched.data(), patched.size(), texture_slots, &error);
               }
            }
            // Most shaders read no screen texture over the full target: the game's (or the plain motion vector) shader goes on
            screen_uv_miss = patched.empty() && error == "no screen uv";
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
               else
               {
                  const bool skinned = SUCCEEDED(reflection->GetResourceBindingDescByName("vc3", &bind_desc)) && bind_desc.BindPoint == MotionVectorPatches::previous_slots[1].first;
                  D3D11_SHADER_DESC shader_desc;
                  bool resourceless = SUCCEEDED(reflection->GetDesc(&shader_desc));
                  for (UINT i = 0; resourceless && i < shader_desc.BoundResources; i++)
                     resourceless = SUCCEEDED(reflection->GetResourceBindingDesc(i, &bind_desc)) && (bind_desc.Type == D3D_SIT_CBUFFER || bind_desc.Type == D3D_SIT_SAMPLER);
                  std::array<UINT, std::size(MotionVectorPatches::previous_slots)> read_sizes;
                  for (size_t i = 0; i < read_sizes.size(); i++)
                     read_sizes[i] = DXBC::ConstantBufferBytes(code, desc->code_size, MotionVectorPatches::previous_slots[i].first);
                  const std::unique_lock lock(game_device_data.mv_mutex);
                  if (skinned)
                     game_device_data.mv_bone_vertex_shaders.insert(hash);
                  if (resourceless)
                     game_device_data.mv_resourceless_vertex_shaders.insert(hash);
                  game_device_data.mv_vertex_shader_read_sizes[hash] = read_sizes;
               }
            }
         }
      }
      com_ptr<T> shader;
      if constexpr (!vertex)
      {
         if (screen_uv_miss && pixel_shader_patch == PixelShaderPatch::MotionVectorsScreenUV)
            shader = GetMotionVectorShader<T>(native_device, device_data, hash, pipeline);
      }
      if (!patched.empty())
      {
         HRESULT hr = E_FAIL;
         if constexpr (vertex)
            hr = native_device->CreateVertexShader(patched.data(), patched.size(), nullptr, &shader);
         else
            hr = native_device->CreatePixelShader(patched.data(), patched.size(), nullptr, &shader);
         if (FAILED(hr))
            error = std::format("create 0x{:08X}", uint32_t(hr));
      }
      // Failures in every build (bug reports), every patched shader only in development
      if (!screen_uv_miss && (DEVELOPMENT || !shader))
         reshade::log::message((shader || screen_space) ? reshade::log::level::info : reshade::log::level::warning, std::format("[SR4 MV] {} 0x{:08X} {}", vertex ? "VS" : "PS", hash, shader ? "patched" : error).c_str());
      const std::unique_lock lock(game_device_data.mv_mutex);
      return shaders.try_emplace(hash, shader).first->second;
   }

   // The bound vertex shader's patched version (null if refused), looked up again only when the game's changes
   static ID3D11VertexShader* GetPatchedVertexShader(ID3D11Device* native_device, CommandListData& cmd_list_data, DeviceData& device_data, uint32_t hash)
   {
      auto& game_device_data = GetGameDeviceData(device_data);
      if (hash != game_device_data.mv_last_vertex_shader_hash)
      {
         game_device_data.mv_last_vertex_shader = GetMotionVectorShader<ID3D11VertexShader>(native_device, device_data, hash, cmd_list_data.pipeline_state_original_vertex_shader).get();
         const std::shared_lock lock(game_device_data.mv_mutex);
         game_device_data.mv_last_vertex_shader_skinned = game_device_data.mv_bone_vertex_shaders.contains(hash);
         game_device_data.mv_last_vertex_shader_resourceless = game_device_data.mv_resourceless_vertex_shaders.contains(hash);
         const auto read_sizes = game_device_data.mv_vertex_shader_read_sizes.find(hash);
         game_device_data.mv_last_vertex_shader_read_sizes = (read_sizes != game_device_data.mv_vertex_shader_read_sizes.end() ? read_sizes->second : std::array<UINT, std::size(MotionVectorPatches::previous_slots)>{});
         game_device_data.mv_last_vertex_shader_hash = hash;
      }
      return game_device_data.mv_last_vertex_shader;
   }

   // Render scale (see "g_render_scale"): a scene draw's output or half output sized viewport (the half res SSAO and particle
   // targets) shrinks to the top-left render sub-rect's share of it, its scissor with it, both left bound after the draw like the patched
   // shaders; other viewports (shadows, reflections) stay. Its PS vc4 gets "IR_Pixel_Steps" rewritten from its next Map on (see
   // "OnUnmapBufferRegion"). True if the draw has the scaled viewport.
   static bool ScaleSceneViewport(ID3D11DeviceContext* native_device_context, DeviceData& device_data, const ShaderHashesList<OneShaderPerPipeline>& shader_hashes)
   {
      const float scale = GetSubRectScale(device_data);
      if (scale >= 1.f)
         return false;
      auto& game_device_data = GetGameDeviceData(device_data);
      if (!game_device_data.sub_rect_bound)
      {
         D3D11_VIEWPORT viewport;
         UINT count = 1;
         native_device_context->RSGetViewports(&count, &viewport);
         const bool full = viewport.Width == device_data.output_resolution.x && viewport.Height == device_data.output_resolution.y;
         const bool half = viewport.Width * 2.f == device_data.output_resolution.x && viewport.Height * 2.f == device_data.output_resolution.y;
         if (count != 1 || viewport.TopLeftX != 0.f || viewport.TopLeftY != 0.f || (!full && !half))
         {
#if DEVELOPMENT
            // Scene draws left unscaled, once per viewport and target size (with the shaders of the first)
            {
               com_ptr<ID3D11RenderTargetView> rtv;
               native_device_context->OMGetRenderTargets(1, &rtv, nullptr);
               uint4 target_size = {};
               DXGI_FORMAT target_format = DXGI_FORMAT_UNKNOWN;
               if (rtv)
                  GetResourceInfo(rtv.get(), target_size, target_format);
               static std::set<std::array<uint32_t, 6>> logged;
               if (logged.insert({uint32_t(viewport.TopLeftX), uint32_t(viewport.TopLeftY), uint32_t(viewport.Width), uint32_t(viewport.Height), target_size.x, target_size.y}).second)
                  reshade::log::message(reshade::log::level::info, std::format("[SR4 SubRect] unscaled scene viewport {},{} {}x{} into {}x{} (format {}), vs 0x{:08X} ps 0x{:08X}", viewport.TopLeftX, viewport.TopLeftY, viewport.Width, viewport.Height, target_size.x, target_size.y, int(target_format), shader_hashes.vertex_shaders[0], shader_hashes.pixel_shaders[0]).c_str());
            }
#endif
            // A draw at another size (shadows, reflections) gets the game's scissor back
            if (std::exchange(game_device_data.sub_rect_scissor_bound, false))
               native_device_context->RSSetScissorRects(1, &game_device_data.sub_rect_game_scissor);
            return false;
         }
         game_device_data.sub_rect_game_viewport = viewport;
         // The render size's share, exactly (half a pixel at half res), so the scaled UVs ("uv_scale") land on the same texels
         viewport.Width = float(game_device_data.mv_render_size[0]) * (viewport.Width / device_data.output_resolution.x);
         viewport.Height = float(game_device_data.mv_render_size[1]) * (viewport.Height / device_data.output_resolution.y);
         native_device_context->RSSetViewports(1, &viewport);
         game_device_data.sub_rect_bound = true;
      }
      if (!game_device_data.sub_rect_scissor_bound)
      {
         D3D11_RECT scissor = {};
         UINT count = 1;
         native_device_context->RSGetScissorRects(&count, &scissor);
         game_device_data.sub_rect_game_scissor = scissor;
         scissor = {LONG(std::floor(float(scissor.left) * scale)), LONG(std::floor(float(scissor.top) * scale)), LONG(std::ceil(float(scissor.right) * scale)), LONG(std::ceil(float(scissor.bottom) * scale))};
         native_device_context->RSSetScissorRects(1, &scissor);
         game_device_data.sub_rect_scissor_bound = true;
      }
      com_ptr<ID3D11Buffer> vc4;
      native_device_context->PSGetConstantBuffers(4, 1, &vc4);
      if (vc4 && vc4.get() != game_device_data.sub_rect_last_vc4.load(std::memory_order_relaxed))
      {
         const std::lock_guard lock(game_device_data.mv_constants_mutex);
         game_device_data.sub_rect_last_vc4.store(vc4.get(), std::memory_order_relaxed);
         if (game_device_data.sub_rect_vc4_buffers.insert(reinterpret_cast<uint64_t>(vc4.get())).second && !game_device_data.mv_constants_copies.contains(reinterpret_cast<uint64_t>(vc4.get())))
         {
            RegisterFilteredBuffer(&game_device_data, vc4.get());
         }
      }
      return true;
   }

   // Render scale: the game's viewport and scissor back where the scaled ones are still bound (the scene ended)
   static void RestoreSceneViewport(ID3D11DeviceContext* native_device_context, SaintsRowIVGameDeviceData* game_device_data)
   {
      if (std::exchange(game_device_data->sub_rect_bound, false))
         native_device_context->RSSetViewports(1, &game_device_data->sub_rect_game_viewport);
      if (std::exchange(game_device_data->sub_rect_scissor_bound, false))
         native_device_context->RSSetScissorRects(1, &game_device_data->sub_rect_game_scissor);
   }

   // Render scale: the game binding its own viewports or scissors replaces the scaled ones (ours are set on the native context, so
   // they send no event)
   static void OnBindViewports(reshade::api::command_list* cmd_list, uint32_t first, uint32_t count, const reshade::api::viewport* viewports)
   {
      if (DeviceData* const device_data = cmd_list->get_device()->get_private_data<DeviceData>(); device_data && device_data->game)
         GetGameDeviceData(*device_data).sub_rect_bound = false;
   }
   static void OnBindScissorRects(reshade::api::command_list* cmd_list, uint32_t first, uint32_t count, const reshade::api::rect* rects)
   {
      if (DeviceData* const device_data = cmd_list->get_device()->get_private_data<DeviceData>(); device_data && device_data->game)
         GetGameDeviceData(*device_data).sub_rect_scissor_bound = false;
   }

   // Render scale: a scene draw that reads screen textures at UVs over the full target, into the sub-rect: the screen space quads
   // ("sub_rect_quad_vertex_shaders") with their sub-rect VS, or the patched screen UV pixel shaders ("sub_rect_uv_pixel_shaders", and
   // the quads rebuilding NDC from their UV, "sub_rect_quad_ndc_pixel_shaders", with the game's VS), both left bound like the motion
   // vector shaders, with the jitter buffer for the UV scale (and the sun shadow terms' jitter, at every render scale). False if it's
   // neither (or the shader is missing).
   static bool DrawSubRect(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, CommandListData& cmd_list_data, DeviceData& device_data, const ShaderHashesList<OneShaderPerPipeline>& original_shader_hashes, const std::function<void()>& draw)
   {
      auto& game_device_data = GetGameDeviceData(device_data);
      if (!game_device_data.mv_jitter_buffer)
         return false;
      ID3D11Buffer* const sub_rect = game_device_data.mv_jitter_buffer.get();
      const uint32_t pixel_shader_hash = original_shader_hashes.pixel_shaders[0];
      if (std::ranges::contains(sub_rect_uv_pixel_shaders, pixel_shader_hash) || std::ranges::contains(sub_rect_quad_ndc_pixel_shaders, pixel_shader_hash, &std::pair<uint32_t, uint32_t>::first))
      {
         const com_ptr<ID3D11PixelShader> pixel_shader = GetMotionVectorShader<ID3D11PixelShader>(native_device, device_data, pixel_shader_hash, cmd_list_data.pipeline_state_original_pixel_shader, PixelShaderPatch::ScreenUV);
         if (!pixel_shader)
            return false;
         PatchedDraws::RestoreGameShader(native_device_context, &game_device_data.mv_bound_vertex_shader);
         PatchedDraws::BindPatchedShader(native_device_context, pixel_shader.get(), &game_device_data.mv_bound_pixel_shader);
         native_device_context->PSSetConstantBuffers(MotionVectorPatches::jitter_slot, 1, &sub_rect);
      }
      else
      {
         const auto quad = std::ranges::find(sub_rect_quad_vertex_shaders, original_shader_hashes.vertex_shaders[0], &std::pair<uint32_t, uint32_t>::first);
         if (quad == std::end(sub_rect_quad_vertex_shaders))
            return false;
         const std::shared_lock lock_shader_objects(s_mutex_shader_objects);
         const auto vertex_shader = device_data.native_vertex_shaders.find(quad->second);
         if (vertex_shader == device_data.native_vertex_shaders.end() || !vertex_shader->second)
            return false;
         PatchedDraws::BindPatchedShader(native_device_context, vertex_shader->second.get(), &game_device_data.mv_bound_vertex_shader);
         PatchedDraws::RestoreGameShader(native_device_context, &game_device_data.mv_bound_pixel_shader);
         native_device_context->VSSetConstantBuffers(MotionVectorPatches::jitter_slot, 1, &sub_rect);
      }
      draw();
      // A native SSAO pass (see "sub_rect_ssao_pixel_shaders"): its share's last column and row repeated into a band past it (the
      // texture's clamp at full scale); the share ends at the scaled viewport's last covered pixel
      if (GetSubRectScale(device_data) < 1.f && std::ranges::contains(sub_rect_ssao_pixel_shaders, pixel_shader_hash))
      {
         com_ptr<ID3D11RenderTargetView> rtvs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
         com_ptr<ID3D11DepthStencilView> dsv;
         native_device_context->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, &rtvs[0], &dsv);
         com_ptr<ID3D11Resource> resource;
         com_ptr<ID3D11Texture2D> texture;
         if (rtvs[0])
            rtvs[0]->GetResource(&resource);
         D3D11_VIEWPORT viewport;
         UINT viewport_count = 1;
         native_device_context->RSGetViewports(&viewport_count, &viewport);
         D3D11_TEXTURE2D_DESC desc;
         if (resource && SUCCEEDED(resource->QueryInterface(&texture)) && viewport_count == 1)
         {
            texture->GetDesc(&desc);
            const UINT last_x = UINT(std::ceil(viewport.Width - 0.5f)) - 1, last_y = UINT(std::ceil(viewport.Height - 0.5f)) - 1;
            if (desc.SampleDesc.Count == 1 && last_x + 1 < desc.Width && last_y + 1 < desc.Height)
            {
               // A copy into a resource bound for output is a hazard the runtime does not resolve: unbound around the copies
               native_device_context->OMSetRenderTargets(0, nullptr, nullptr);
               const UINT band_x = (std::min)(desc.Width - 1 - last_x, sub_rect_ssao_guard_texels), band_y = (std::min)(desc.Height - 1 - last_y, sub_rect_ssao_guard_texels);
               const D3D11_BOX column = {.left = last_x, .top = 0, .front = 0, .right = last_x + 1, .bottom = last_y + 1, .back = 1};
               for (UINT i = 1; i <= band_x; i++)
                  native_device_context->CopySubresourceRegion(texture.get(), 0, last_x + i, 0, 0, texture.get(), 0, &column);
               const D3D11_BOX row = {.left = 0, .top = last_y, .front = 0, .right = last_x + 1 + band_x, .bottom = last_y + 1, .back = 1};
               for (UINT i = 1; i <= band_y; i++)
                  native_device_context->CopySubresourceRegion(texture.get(), 0, 0, last_y + i, 0, texture.get(), 0, &row);
               ID3D11RenderTargetView* targets[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
               for (UINT i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
                  targets[i] = rtvs[i].get();
               native_device_context->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, targets, dsv.get());
            }
         }
      }
#if DEVELOPMENT
      g_sub_rect_draws_this_frame++;
#endif
      return true;
   }

   // Draws an opaque main material pass draw (the fp16 scene alone, output sized, with depth) with the patched shaders, adding the
   // motion vector target ("target_slot", past the game's) and the previous frame's vc2 / vc3 ("previous_slots"). False if it can't
   // (the draw then goes to "DrawWithJitter").
   static bool DrawWithMotionVectors(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, CommandListData& cmd_list_data, DeviceData& device_data, const ShaderHashesList<OneShaderPerPipeline>& original_shader_hashes, const std::function<void()>& draw, const com_ptr<ID3D11RenderTargetView> (&rtvs)[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT], ID3D11DepthStencilView* dsv)
   {
      auto& game_device_data = GetGameDeviceData(device_data);
      // The G-buffer (2-3 targets, output sized depth) opens the scene; post passes with 2 targets must not reopen it
      if (rtvs[0] && rtvs[1])
      {
         if (!game_device_data.mv_scene_open && dsv)
         {
            uint4 gbuffer_depth_size;
            DXGI_FORMAT unused_gbuffer_format;
            GetResourceInfo(dsv, gbuffer_depth_size, unused_gbuffer_format);
            if (gbuffer_depth_size.x == device_data.output_resolution.x && gbuffer_depth_size.y == device_data.output_resolution.y)
            {
               game_device_data.mv_scene_open = true;
#if DEVELOPMENT
               if (auto* const perf_queries = game_device_data.perf_frame_queries; perf_queries && !std::exchange(perf_queries->scene_started, true))
                  native_device_context->End(perf_queries->scene_start.get());
#endif
               game_device_data.mv_scene_copied = false;
               game_device_data.mv_scene_copy_source = 0;
               game_device_data.mv_scene_copy_dest = 0;
               game_device_data.jitter_dsv = nullptr;
               game_device_data.jitter_depth_stencil_state = nullptr;
               game_device_data.jitter_depth_test = true;
               game_device_data.mv_accepted_rtv = nullptr;
               game_device_data.mv_accepted_dsv = nullptr;
               game_device_data.mv_blend_state = nullptr;
               game_device_data.mv_blend_opaque = true;
               // Halton (2, 3) over the upscaler's phase count; pixels to NDC (y up)
               const SR::InstanceData* const sr_instance_data = (IsSRActive(device_data) ? device_data.GetSRInstanceData() : nullptr);
               const int phases = (sr_instance_data ? (std::max)(sr_implementations[device_data.sr_type]->GetJitterPhases(sr_instance_data), 1) : SR::GetDefaultJitterPhases());
               const unsigned int phase = cb_luma_global_settings.FrameIndex % phases;
               game_device_data.mv_jitter = (((sr_instance_data || g_mv_force_jitter) && !g_mv_disable_jitter) ? std::array<float, 2>{SR::HaltonSequence(phase, 2), SR::HaltonSequence(phase, 3)} : std::array<float, 2>{});
               // The render size: the render scale's sub-rect (see "ScaleSceneViewport"), else the scene's
               const float scale = GetSubRectScale(device_data);
               const float render_width = std::round(float(gbuffer_depth_size.x) * scale), render_height = std::round(float(gbuffer_depth_size.y) * scale);
               game_device_data.mv_jitter_ndc = {game_device_data.mv_jitter[0] * 2.f / render_width, game_device_data.mv_jitter[1] * -2.f / render_height};
               // zw: the sub-rect's share of the target, for "Luma_SR4_SubRectQuad.hlsl"
               // c1.xy: the UV of the share's last texel center, where the patched screen texture reads clamp (see
               // "MotionVectorPatches::PatchScreenUVPixelShader")
               const float ndc_jitter[8] = {game_device_data.mv_jitter_ndc[0], game_device_data.mv_jitter_ndc[1], render_width / float(gbuffer_depth_size.x), render_height / float(gbuffer_depth_size.y), (render_width - 0.5f) / float(gbuffer_depth_size.x), (render_height - 0.5f) / float(gbuffer_depth_size.y), 0.f, 0.f};
               game_device_data.mv_render_size = {uint32_t(render_width), uint32_t(render_height)};
               if (!PatchedDraws::WriteDynamicConstants(native_device, native_device_context, std::addressof(game_device_data.mv_jitter_buffer), ndc_jitter, sizeof(ndc_jitter)))
               {
                  // No stale jitter on the material draws either: no motion vectors this frame
                  game_device_data.mv_jitter = {};
                  game_device_data.mv_jitter_ndc = {};
                  game_device_data.mv_jitter_buffer.reset();
               }
            }
         }
         game_device_data.mv_scene_color_wanted |= game_device_data.mv_scene_open;
         return false;
      }
      // The scene target alone, plus the motion vector target the last motion vector draw left bound
      for (UINT slot = 1; slot < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; slot++)
      {
         if (rtvs[slot] && (slot != MotionVectorPatches::target_slot || rtvs[slot] != game_device_data.mv_rtv))
            return false;
      }
      if (!rtvs[0] || !dsv || !game_device_data.mv_scene_open)
         return false;
      // Known targets: checked, and the motion vector target built for them
      if (rtvs[0].get() != game_device_data.mv_accepted_rtv || dsv != game_device_data.mv_accepted_dsv)
      {
         com_ptr<ID3D11Resource> color;
         rtvs[0]->GetResource(&color);
         if (!color || color != game_device_data.mv_scene_color)
            return false;
         D3D11_RENDER_TARGET_VIEW_DESC rtv_desc;
         rtvs[0]->GetDesc(&rtv_desc);
         // The motion vector target is single sampled; an MSAA scene (display.ini MSAA_Level) turns the upscaler off (see "IsSRActive")
         if (rtv_desc.Format != DXGI_FORMAT_R16G16B16A16_FLOAT || rtv_desc.ViewDimension != D3D11_RTV_DIMENSION_TEXTURE2D)
            return false;
         uint4 size, depth_size;
         DXGI_FORMAT unused_format;
         GetResourceInfo(rtvs[0].get(), size, unused_format);
         GetResourceInfo(dsv, depth_size, unused_format);
         // The main scene only (reflections and cube faces are smaller)
         if (size.x != device_data.output_resolution.x || size.y != device_data.output_resolution.y || depth_size.x != size.x || depth_size.y != size.y)
            return false;
         const std::unique_lock lock(game_device_data.mv_mutex);
         D3D11_TEXTURE2D_DESC desc = {};
         if (game_device_data.mv_texture)
            game_device_data.mv_texture->GetDesc(&desc);
         // R16G16_FLOAT: the patched draws' motion is analytic, and float16 keeps it within 0.05 % at half the bandwidth (the draws, the
         // fill, the upscaler)
         const DXGI_FORMAT mv_format = (g_mv_half_float ? DXGI_FORMAT_R16G16_FLOAT : DXGI_FORMAT_R32G32_FLOAT);
         if (desc.Width != size.x || desc.Height != size.y || desc.Format != mv_format)
         {
            game_device_data.mv_texture.reset();
            game_device_data.mv_rtv.reset();
            game_device_data.mv_uav.reset();
            // The fill reads the target back through its UAV
            D3D11_FEATURE_DATA_FORMAT_SUPPORT2 support = {mv_format};
            const bool typed_uav_load = SUCCEEDED(native_device->CheckFeatureSupport(D3D11_FEATURE_FORMAT_SUPPORT2, &support, sizeof(support))) && (support.OutFormatSupport2 & D3D11_FORMAT_SUPPORT2_UAV_TYPED_LOAD) != 0;
            desc = {.Width = size.x, .Height = size.y, .MipLevels = 1, .ArraySize = 1, .Format = mv_format, .SampleDesc = {.Count = 1}, .Usage = D3D11_USAGE_DEFAULT, .BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | (typed_uav_load ? D3D11_BIND_UNORDERED_ACCESS : 0u)};
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
         game_device_data.mv_accepted_rtv = rtvs[0].get();
         game_device_data.mv_accepted_dsv = dsv;
      }
      com_ptr<ID3D11BlendState> blend_state;
      native_device_context->OMGetBlendState(&blend_state, nullptr, nullptr);
      if (blend_state.get() != game_device_data.mv_blend_state)
      {
         D3D11_BLEND_DESC blend_desc = CD3D11_BLEND_DESC(D3D11_DEFAULT);
         if (blend_state)
            blend_state->GetDesc(&blend_desc);
         // Translucent draws keep the motion vectors of what's behind them, and so do draws that write no color (every blend state
         // writes the motion vector target, see "OnCreateBlendState"). The material pass blends One/Zero (an opaque write).
         const D3D11_RENDER_TARGET_BLEND_DESC& rt0_blend = blend_desc.RenderTarget[0];
         game_device_data.mv_blend_opaque = rt0_blend.RenderTargetWriteMask != 0 && (!rt0_blend.BlendEnable || (rt0_blend.SrcBlend == D3D11_BLEND_ONE && rt0_blend.DestBlend == D3D11_BLEND_ZERO && rt0_blend.BlendOp == D3D11_BLEND_OP_ADD));
         game_device_data.mv_blend_state = blend_state.get();
      }
      if (!game_device_data.mv_blend_opaque)
         return false;

      ID3D11VertexShader* const vertex_shader = GetPatchedVertexShader(native_device, cmd_list_data, device_data, original_shader_hashes.vertex_shaders[0]);
      // Under the render scale's sub-rect, with the stipple DSF moved to the full texture (see "MotionVectorPatches::PatchScreenUVPixelShader")
      const PixelShaderPatch pixel_shader_patch = (GetSubRectScale(device_data) < 1.f ? PixelShaderPatch::MotionVectorsScreenUV : PixelShaderPatch::MotionVectors);
      if (const uint32_t pixel_shader_hash = original_shader_hashes.pixel_shaders[0]; pixel_shader_hash != game_device_data.mv_last_pixel_shader_hash || pixel_shader_patch != game_device_data.mv_last_pixel_shader_patch)
      {
         game_device_data.mv_last_pixel_shader = GetMotionVectorShader<ID3D11PixelShader>(native_device, device_data, pixel_shader_hash, cmd_list_data.pipeline_state_original_pixel_shader, pixel_shader_patch).get();
         game_device_data.mv_last_pixel_shader_hash = pixel_shader_hash;
         game_device_data.mv_last_pixel_shader_patch = pixel_shader_patch;
      }
      ID3D11PixelShader* const pixel_shader = game_device_data.mv_last_pixel_shader;
      if (!vertex_shader || !pixel_shader || !game_device_data.mv_jitter_buffer)
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
            if (depth_desc.Format != DXGI_FORMAT_R24_UNORM_X8_TYPELESS || depth_desc.ViewDimension != D3D11_SRV_DIMENSION_TEXTURE2D || frame_depth_size.x != device_data.output_resolution.x || frame_depth_size.y != device_data.output_resolution.y)
               game_device_data.mv_frame_depth.reset();
         }
         {
            const std::shared_lock lock_shader_objects(s_mutex_shader_objects);
            game_device_data.mv_fill_pending = game_device_data.mv_frame_depth && game_device_data.mv_uav && HasShaders(device_data.native_compute_shaders, "SR4 Motion Vector Fill CS"_h);
         }
         // The fill's marker: the largest float16, exact in both formats (see "Luma_SR4_MotionVectorFill.hlsl")
         const FLOAT clear_value = (game_device_data.mv_fill_pending ? 65504.f : 0.f);
         const FLOAT clear[4] = {clear_value, clear_value, 0.f, 0.f};
         native_device_context->ClearRenderTargetView(game_device_data.mv_rtv.get(), clear);
         // Last frame's camera and objects are the previous ones, unless frames without a scene (menus, videos) came between
         game_device_data.mv_previous_camera = game_device_data.mv_camera;
         game_device_data.mv_previous_camera_valid = game_device_data.mv_camera_valid && cb_luma_global_settings.FrameIndex - game_device_data.mv_frame_index <= 1;
         game_device_data.mv_camera_valid = false;
         game_device_data.mv_frame_index = cb_luma_global_settings.FrameIndex;
         // Swapped, not rebuilt: the lists keep their nodes and capacity (an empty list matches nothing); keys drawn in neither of the last
         // two frames go
         game_device_data.mv_previous_objects.swap(game_device_data.mv_objects);
#if DEVELOPMENT
         g_mv_tiebreak_collisions_last_frame = PatchedDraws::CountTieBreakCollisions(game_device_data.mv_previous_objects, [](const auto& a, const auto& b)
            { return PatchedDraws::SameBytes(a.object, b.object) && PatchedDraws::SameBytes(a.bones, b.bones); });
#endif
         std::erase_if(game_device_data.mv_objects, [](const auto& entry)
            { return entry.second.empty(); });
         for (auto& entry : game_device_data.mv_objects)
            entry.second.clear();
         if (!game_device_data.mv_previous_camera_valid)
            game_device_data.mv_previous_objects.clear();
      }

      // The game's vc2 / vc3. The slots added past them stay bound after the draw: no game shader reads a constant buffer at slot 9 or above.
      static_assert(MotionVectorPatches::previous_slots[0].first == MotionVectorPatches::object_slot && MotionVectorPatches::previous_slots[1].first == MotionVectorPatches::object_slot + 1);
      com_ptr<ID3D11Buffer> game_cbs[std::size(MotionVectorPatches::previous_slots)];
      native_device_context->VSGetConstantBuffers(MotionVectorPatches::object_slot, UINT(std::size(game_cbs)), &game_cbs[0]);
      const bool skinned = game_device_data.mv_last_vertex_shader_skinned;
      const bool resourceless = game_device_data.mv_last_vertex_shader_resourceless;
      const SaintsRowIVGameDeviceData::ConstantsCopy object = GetConstantsCopy(&game_device_data, game_cbs[0].get());
      const SaintsRowIVGameDeviceData::ConstantsCopy bones = (skinned ? GetConstantsCopy(&game_device_data, game_cbs[1].get()) : nullptr);
      const auto copy_size = [](const SaintsRowIVGameDeviceData::ConstantsCopy& copy)
      { return copy ? copy->size() : size_t(0); };
      // The previous frame's vc2 / vc3: the same object's from last frame, else this draw's with last frame's camera (no object motion).
      // None (no CPU copy yet): the current ones (zero motion).
      const std::vector<uint8_t>* uploads[std::size(MotionVectorPatches::previous_slots)] = {};
      // vc2: projTM c0-c3 (the camera), objTM c16-c18 (translation in .w)
      constexpr size_t camera_size = sizeof(game_device_data.mv_camera);
      if (copy_size(object) >= 19 * 16 && (!skinned || copy_size(bones) != 0))
      {
         if (!game_device_data.mv_camera_valid)
         {
            std::memcpy(game_device_data.mv_camera.data(), object->data(), camera_size);
            game_device_data.mv_camera_valid = true;
         }
#if DEVELOPMENT
         // "Performance Test" without motion vector draws: the frame (camera, target clear, camera fill, upscaler) still happens, the
         // draws run jittered only, or untouched
         if (perf_test_modes[g_perf_test].motion_vector_draws < 2)
            return false;
#endif

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
         const float* const values = reinterpret_cast<const float*>(object->data());
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
               if (copy_size(candidate.object) == object->size() && copy_size(candidate.bones) == copy_size(bones) && distance < nearest)
               {
                  nearest = distance;
                  match = &candidate;
               }
            }
         }
         if (match)
         {
            // Last frame's list outlives the draw ("mv_previous_objects" only changes at the next frame start)
            uploads[0] = match->object.get();
            if (skinned)
               uploads[1] = match->bones.get();
            // Kept as drawn for the next frame
            game_device_data.mv_objects[key].push_back({translation, object, bones});
         }
         // Draws with another projTM than the frame's camera (sky, windows at infinity) are left as is and not tracked
         else if (std::memcmp(object->data(), game_device_data.mv_camera.data(), camera_size) == 0)
         {
            // Not found: its own constants with last frame's camera (camera motion only)
            if (game_device_data.mv_previous_camera_valid)
            {
               auto& upload = game_device_data.mv_camera_upload;
               upload.assign(object->begin(), object->end());
               std::memcpy(upload.data(), game_device_data.mv_previous_camera.data(), camera_size);
               uploads[0] = &upload;
            }
            game_device_data.mv_objects[key].push_back({translation, object, bones});
         }
      }
      ID3D11Buffer* const current[] = {game_cbs[0].get(), game_cbs[1].get()};
      const std::span<const UINT> read_sizes = (g_mv_read_sizes ? std::span<const UINT>(game_device_data.mv_last_vertex_shader_read_sizes) : std::span<const UINT>());
      game_device_data.mv_previous_constants.Bind(native_device, native_device_context, MotionVectorPatches::previous_slots, uploads, current, "SR4", read_sizes);
      ID3D11Buffer* const jitter = game_device_data.mv_jitter_buffer.get();
      native_device_context->VSSetConstantBuffers(MotionVectorPatches::jitter_slot, 1, &jitter);
      if (pixel_shader_patch == PixelShaderPatch::MotionVectorsScreenUV)
         native_device_context->PSSetConstantBuffers(MotionVectorPatches::jitter_slot, 1, &jitter);
      // The second run's resources: the current ones (no copies kept, see "MotionVectorPatches::previous_resources_slot")
      if (!resourceless)
      {
         com_ptr<ID3D11ShaderResourceView> resources[MotionVectorPatches::resource_slots];
         native_device_context->VSGetShaderResources(0, MotionVectorPatches::resource_slots, &resources[0]);
         ID3D11ShaderResourceView* previous_resources[MotionVectorPatches::resource_slots] = {};
         for (UINT slot = 0; slot < MotionVectorPatches::resource_slots; slot++)
            previous_resources[slot] = resources[slot].get();
         native_device_context->VSSetShaderResources(MotionVectorPatches::previous_resources_slot, MotionVectorPatches::resource_slots, previous_resources);
      }
      // Left bound after the draw (set directly, bypassing Core's state tracking): the game's next draws either bind their own
      // targets and shaders, or are motion vector draws too. The draws in between write no "o4" (no game pixel shader declares a
      // fifth target), so the motion vector target keeps its contents.
      if (rtvs[MotionVectorPatches::target_slot] != game_device_data.mv_rtv)
      {
         ID3D11RenderTargetView* targets[MotionVectorPatches::target_slot + 1] = {rtvs[0].get()};
         targets[MotionVectorPatches::target_slot] = game_device_data.mv_rtv.get();
         native_device_context->OMSetRenderTargets(MotionVectorPatches::target_slot + 1, targets, dsv);
      }
      PatchedDraws::BindPatchedShader(native_device_context, vertex_shader, &game_device_data.mv_bound_vertex_shader);
      PatchedDraws::BindPatchedShader(native_device_context, pixel_shader, &game_device_data.mv_bound_pixel_shader);

      draw();

      if (!resourceless)
      {
         ID3D11ShaderResourceView* const null_resources[MotionVectorPatches::resource_slots] = {};
         native_device_context->VSSetShaderResources(MotionVectorPatches::previous_resources_slot, MotionVectorPatches::resource_slots, null_resources);
      }
      return true;
   }

   // DLSS / FSR on the jittered scene (the post scene, see "ResolveScene"; its render scale sub-rect, else all of it), the G-buffer
   // depth and the motion vectors; the full size result goes back into the scene. False if it didn't draw (missing input, or the upscaler failed).
   static bool DrawUpscaler(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, DeviceData& device_data, ID3D11Texture2D* scene, const D3D11_TEXTURE2D_DESC& scene_desc, ID3D11Resource* depth, uint32_t render_width, uint32_t render_height)
   {
      auto& game_device_data = GetGameDeviceData(device_data);
      if (!game_device_data.mv_texture || !game_device_data.mv_camera_valid)
         return false;
      D3D11_TEXTURE2D_DESC mv_desc;
      game_device_data.mv_texture->GetDesc(&mv_desc);
      if (scene_desc.Width != mv_desc.Width || scene_desc.Height != mv_desc.Height)
         return false;
      // None when "Super Resolution" changed after present (see "IsSRActive"): no output texture is made for it
      SR::InstanceData* const sr_instance_data = device_data.GetSRInstanceData();
      if (!sr_instance_data)
         return false;

      D3D11_TEXTURE2D_DESC output_desc = {};
      if (device_data.sr_output_color)
         device_data.sr_output_color->GetDesc(&output_desc);
      if (output_desc.Width != scene_desc.Width || output_desc.Height != scene_desc.Height)
      {
         device_data.sr_output_color.reset();
         output_desc = {.Width = scene_desc.Width, .Height = scene_desc.Height, .MipLevels = 1, .ArraySize = 1, .Format = DXGI_FORMAT_R16G16B16A16_FLOAT, .SampleDesc = {.Count = 1}, .Usage = D3D11_USAGE_DEFAULT, .BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS};
         native_device->CreateTexture2D(&output_desc, nullptr, &device_data.sr_output_color);
         game_device_data.sr_output_recreated = true;
      }
      if (!device_data.sr_output_color)
      {
         // Back to SMAA until the upscaler is picked again
         device_data.sr_suppressed = true;
         return false;
      }

      // FSR needs the camera (DLSS ignores it). projTM (vc2 c0-c3) is a column vector view projection with absolute world
      // translation: row 1 = the up axis / tan(fov / 2), row 3 = the view depth axis, row 2 = A * row 3 + B (w), with
      // A = far / (far - near) and B = -near * A. In double: the translations are large.
      const auto row = [&](int i)
      { return std::array<double, 4>{game_device_data.mv_camera[i * 4], game_device_data.mv_camera[i * 4 + 1], game_device_data.mv_camera[i * 4 + 2], game_device_data.mv_camera[i * 4 + 3]}; };
      const auto length3 = [](const std::array<double, 4>& v)
      { return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]); };
      const std::array<double, 4> up = row(1), z = row(2), w = row(3);
      const double a = (length3(w) > 0.0 ? (length3(z) / length3(w)) : 0.0);
      const double b = z[3] - a * w[3];
      const double near_plane = (a > 0.0 ? (-b / a) : 0.0);
      // far = B / (1 - A), finite for A > 1 (A ~= 1.00001 at near 0.15: ~15 km). ponytail: 100 km for A <= 1 (infinite); Core's FSR
      // sets FFX_FSR3_ENABLE_DEPTH_INFINITE only with inverted depth, a separate infinite far flag in SR::SettingsData would drop it
      const double far_plane = ((a > 1.0 && b / (1.0 - a) > near_plane) ? (b / (1.0 - a)) : 100000.0);
      const double vert_fov = (length3(up) > 0.0 ? (2.0 * std::atan(1.0 / length3(up))) : 0.0);

      SR::SettingsData settings_data;
      settings_data.output_width = scene_desc.Width;
      settings_data.output_height = scene_desc.Height;
      settings_data.render_width = render_width;
      settings_data.render_height = render_height;
      settings_data.dynamic_resolution = false;
      settings_data.hdr = true;
      settings_data.inverted_depth = false;
      settings_data.mvs_jittered = false;
      // The motion vectors are UV deltas of the rendered area (the viewport), previous minus current
      settings_data.mvs_x_scale = float(render_width);
      settings_data.mvs_y_scale = float(render_height);
      settings_data.auto_exposure = (device_data.sr_type != SR::Type::FSR);
      settings_data.render_preset = dlss_render_preset;
      sr_implementations[device_data.sr_type]->UpdateSettings(sr_instance_data, native_device_context, settings_data);

      SR::SuperResolutionImpl::DrawData draw_data;
      draw_data.source_color = scene;
      draw_data.output_color = device_data.sr_output_color.get();
      draw_data.motion_vectors = game_device_data.mv_texture.get();
      draw_data.depth_buffer = depth;
      draw_data.render_width = render_width;
      draw_data.render_height = render_height;
      // As applied (pixels, +y down)
      draw_data.jitter_x = game_device_data.mv_jitter[0];
      draw_data.jitter_y = game_device_data.mv_jitter[1];
      draw_data.reset = device_data.force_reset_sr;
      // FSR needs one on every frame: 60 degrees when projTM gave none (as SR3R)
      draw_data.vert_fov = float(vert_fov > 0.0 ? vert_fov : (60.0 * M_PI / 180.0));
      if (near_plane > 0.0)
      {
         draw_data.near_plane = float(near_plane);
         draw_data.far_plane = float(far_plane);
      }
      if (!sr_implementations[device_data.sr_type]->Draw(sr_instance_data, native_device_context, draw_data))
      {
         // Back to SMAA until the upscaler is picked again
         device_data.sr_suppressed = true;
         return false;
      }
      native_device_context->CopySubresourceRegion(scene, 0, 0, 0, 0, device_data.sr_output_color.get(), 0, nullptr);
      // DLSS draws nothing into a new output texture (the session's first, or one made after "None", which Core frees): the frame shows
      // the texture's stale memory until its feature is created again after a draw (a preset change fixed it, a new feature before the
      // first draw didn't). Settings changed once here force that at the next frame's "UpdateSettings".
      if (std::exchange(game_device_data.sr_output_recreated, false) && device_data.sr_type == SR::Type::DLSS)
      {
         SR::SettingsData throwaway_settings_data = settings_data;
         throwaway_settings_data.mvs_jittered = !throwaway_settings_data.mvs_jittered;
         sr_implementations[device_data.sr_type]->UpdateSettings(sr_instance_data, native_device_context, throwaway_settings_data);
      }
      device_data.has_drawn_sr = true;
      device_data.render_resolution = {float(render_width), float(render_height)};
      return true;
   }

   // The scene post reads, before its first pass (immediate context): that pass's t0 (the material target's copy, with the forward
   // draws) upscaled by DLSS / FSR ("DrawUpscaler"); under the render scale's sub-rect without it (the upscaler skipped this frame, or
   // "MV Enable" without one), stretched over the target instead. Then the depths post reads, resampled over the target.
   static void ResolveScene(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, DeviceData& device_data)
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
      if (!scene_resource || FAILED(scene_resource->QueryInterface(&scene)))
         return;
      D3D11_TEXTURE2D_DESC scene_desc;
      scene->GetDesc(&scene_desc);
      // The top-left render sub-rect under the render scale, else the whole scene
      const uint32_t render_width = (std::min)(game_device_data.mv_render_size[0], scene_desc.Width);
      const uint32_t render_height = (std::min)(game_device_data.mv_render_size[1], scene_desc.Height);
      if ((scene_desc.Format != DXGI_FORMAT_R16G16B16A16_FLOAT && scene_desc.Format != DXGI_FORMAT_R16G16B16A16_TYPELESS) || scene_desc.SampleDesc.Count != 1 || render_width == 0 || render_height == 0)
         return;
      com_ptr<ID3D11Resource> depth;
      if (game_device_data.mv_frame_depth)
         game_device_data.mv_frame_depth->GetResource(&depth);

      DrawStateStack<DrawStateStackType::FullGraphics> graphics_state;
      DrawStateStack<DrawStateStackType::Compute> compute_state;
      graphics_state.Cache(native_device_context, device_data.uav_max_count);
      compute_state.Cache(native_device_context, device_data.uav_max_count);

      const bool upscaled = IsSRActive(device_data) && depth && DrawUpscaler(native_device, native_device_context, device_data, scene.get(), scene_desc, depth.get(), render_width, render_height);
      const std::shared_lock lock_shader_objects(s_mutex_shader_objects);
      if ((render_width != scene_desc.Width || render_height != scene_desc.Height) && game_device_data.mv_jitter_buffer && HasShaders(device_data.native_vertex_shaders, "Copy VS"_h))
      {
         ID3D11Buffer* const sub_rect = game_device_data.mv_jitter_buffer.get();
         const D3D11_VIEWPORT viewport = {.TopLeftX = 0.f, .TopLeftY = 0.f, .Width = float(scene_desc.Width), .Height = float(scene_desc.Height), .MinDepth = 0.f, .MaxDepth = 1.f};
         // No upscaled scene: the sub-rect stretched (bilinear) over the target from a copy of it, so post never shows it in the corner
         if (!upscaled && HasShaders(device_data.native_pixel_shaders, "SR4 Sub Rect Color Upscale PS"_h))
         {
            D3D11_TEXTURE2D_DESC copy_desc = {};
            if (game_device_data.sub_rect_color_copy)
               game_device_data.sub_rect_color_copy->GetDesc(&copy_desc);
            if (copy_desc.Width != scene_desc.Width || copy_desc.Height != scene_desc.Height || copy_desc.Format != scene_desc.Format)
            {
               game_device_data.sub_rect_color_copy.reset();
               game_device_data.sub_rect_color_copy_srv.reset();
               copy_desc = {.Width = scene_desc.Width, .Height = scene_desc.Height, .MipLevels = 1, .ArraySize = 1, .Format = scene_desc.Format, .SampleDesc = {.Count = 1}, .Usage = D3D11_USAGE_DEFAULT, .BindFlags = D3D11_BIND_SHADER_RESOURCE};
               const CD3D11_SHADER_RESOURCE_VIEW_DESC srv_desc(D3D11_SRV_DIMENSION_TEXTURE2D, DXGI_FORMAT_R16G16B16A16_FLOAT);
               if (FAILED(native_device->CreateTexture2D(&copy_desc, nullptr, &game_device_data.sub_rect_color_copy)) || FAILED(native_device->CreateShaderResourceView(game_device_data.sub_rect_color_copy.get(), &srv_desc, &game_device_data.sub_rect_color_copy_srv)))
                  game_device_data.sub_rect_color_copy.reset();
            }
            com_ptr<ID3D11Resource> rtv_resource;
            if (game_device_data.sub_rect_scene_rtv)
               game_device_data.sub_rect_scene_rtv->GetResource(&rtv_resource);
            if (rtv_resource != scene_resource)
            {
               game_device_data.sub_rect_scene_rtv.reset();
               const CD3D11_RENDER_TARGET_VIEW_DESC rtv_desc(D3D11_RTV_DIMENSION_TEXTURE2D, DXGI_FORMAT_R16G16B16A16_FLOAT);
               native_device->CreateRenderTargetView(scene.get(), &rtv_desc, &game_device_data.sub_rect_scene_rtv);
            }
            if (game_device_data.sub_rect_color_copy && game_device_data.sub_rect_scene_rtv)
            {
               // The scene is still bound for reading
               ID3D11ShaderResourceView* const null_srv = nullptr;
               native_device_context->PSSetShaderResources(0, 1, &null_srv);
               native_device_context->CopyResource(game_device_data.sub_rect_color_copy.get(), scene.get());
               native_device_context->PSSetConstantBuffers(MotionVectorPatches::jitter_slot, 1, &sub_rect);
               DrawCustomPixelShader(native_device_context, device_data.default_depth_stencil_state.get(), device_data.default_blend_state.get(), nullptr, device_data.native_vertex_shaders.at("Copy VS"_h).get(), device_data.native_pixel_shaders.at("SR4 Sub Rect Color Upscale PS"_h).get(), game_device_data.sub_rect_color_copy_srv.get(), game_device_data.sub_rect_scene_rtv.get(), scene_desc.Width, scene_desc.Height);
            }
         }
         // Post (the final composite's DoF weight, t5) reads the frame depth over the full target, the scene drew it into the sub-rect:
         // resampled (nearest) from a copy of it. The state stack below gives the game its state back.
         if (depth && HasShaders(device_data.native_pixel_shaders, "SR4 Sub Rect Depth Upscale PS"_h))
         {
            com_ptr<ID3D11Texture2D> depth_texture;
            D3D11_TEXTURE2D_DESC depth_desc = {}, copy_desc = {};
            if (SUCCEEDED(depth->QueryInterface(&depth_texture)))
               depth_texture->GetDesc(&depth_desc);
            if (game_device_data.sub_rect_depth_copy)
               game_device_data.sub_rect_depth_copy->GetDesc(&copy_desc);
            if (depth_desc.Format == DXGI_FORMAT_R24G8_TYPELESS && (copy_desc.Width != depth_desc.Width || copy_desc.Height != depth_desc.Height))
            {
               game_device_data.sub_rect_depth_copy.reset();
               game_device_data.sub_rect_depth_copy_srv.reset();
               copy_desc = {.Width = depth_desc.Width, .Height = depth_desc.Height, .MipLevels = 1, .ArraySize = 1, .Format = DXGI_FORMAT_R24G8_TYPELESS, .SampleDesc = {.Count = 1}, .Usage = D3D11_USAGE_DEFAULT, .BindFlags = D3D11_BIND_SHADER_RESOURCE};
               const CD3D11_SHADER_RESOURCE_VIEW_DESC srv_desc(D3D11_SRV_DIMENSION_TEXTURE2D, DXGI_FORMAT_R24_UNORM_X8_TYPELESS);
               if (FAILED(native_device->CreateTexture2D(&copy_desc, nullptr, &game_device_data.sub_rect_depth_copy)) || FAILED(native_device->CreateShaderResourceView(game_device_data.sub_rect_depth_copy.get(), &srv_desc, &game_device_data.sub_rect_depth_copy_srv)))
                  game_device_data.sub_rect_depth_copy.reset();
            }
            com_ptr<ID3D11Resource> dsv_resource;
            if (game_device_data.sub_rect_depth_dsv)
               game_device_data.sub_rect_depth_dsv->GetResource(&dsv_resource);
            if (depth_desc.Format == DXGI_FORMAT_R24G8_TYPELESS && dsv_resource != depth)
            {
               game_device_data.sub_rect_depth_dsv.reset();
               const CD3D11_DEPTH_STENCIL_VIEW_DESC dsv_desc(D3D11_DSV_DIMENSION_TEXTURE2D, DXGI_FORMAT_D24_UNORM_S8_UINT);
               native_device->CreateDepthStencilView(depth.get(), &dsv_desc, &game_device_data.sub_rect_depth_dsv);
            }
            if (!game_device_data.sub_rect_depth_write_state)
            {
               D3D11_DEPTH_STENCIL_DESC state_desc = CD3D11_DEPTH_STENCIL_DESC(D3D11_DEFAULT);
               state_desc.DepthFunc = D3D11_COMPARISON_ALWAYS;
               native_device->CreateDepthStencilState(&state_desc, &game_device_data.sub_rect_depth_write_state);
            }
            if (depth_desc.Format == DXGI_FORMAT_R24G8_TYPELESS && game_device_data.sub_rect_depth_copy && game_device_data.sub_rect_depth_dsv && game_device_data.sub_rect_depth_write_state)
            {
               // The frame depth may still be bound for reading
               ID3D11ShaderResourceView* const null_srv = nullptr;
               native_device_context->PSSetShaderResources(0, 1, &null_srv);
               native_device_context->OMSetRenderTargets(0, nullptr, nullptr);
               native_device_context->CopyResource(game_device_data.sub_rect_depth_copy.get(), depth.get());
               ID3D11ShaderResourceView* const depth_copy = game_device_data.sub_rect_depth_copy_srv.get();
               native_device_context->OMSetRenderTargets(0, nullptr, game_device_data.sub_rect_depth_dsv.get());
               native_device_context->OMSetDepthStencilState(game_device_data.sub_rect_depth_write_state.get(), 0);
               native_device_context->OMSetBlendState(nullptr, nullptr, UINT_MAX);
               native_device_context->RSSetState(nullptr);
               native_device_context->RSSetViewports(1, &viewport);
               native_device_context->IASetInputLayout(nullptr);
               native_device_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
               native_device_context->VSSetShader(device_data.native_vertex_shaders.at("Copy VS"_h).get(), nullptr, 0);
               native_device_context->PSSetShader(device_data.native_pixel_shaders.at("SR4 Sub Rect Depth Upscale PS"_h).get(), nullptr, 0);
               native_device_context->PSSetShaderResources(0, 1, &depth_copy);
               native_device_context->PSSetConstantBuffers(MotionVectorPatches::jitter_slot, 1, &sub_rect);
               native_device_context->Draw(4, 0);
               // The main pass depth too (rl_restore_depth's target, another resource): the effects drawn after the final composite
               // (the simulation glitch 0x2B7780A4) test against it
               com_ptr<ID3D11Resource> main_depth;
               if (game_device_data.mv_accepted_dsv)
                  game_device_data.mv_accepted_dsv->GetResource(&main_depth);
               com_ptr<ID3D11Texture2D> main_depth_texture;
               D3D11_TEXTURE2D_DESC main_depth_desc = {};
               if (main_depth && main_depth != depth && SUCCEEDED(main_depth->QueryInterface(&main_depth_texture)))
                  main_depth_texture->GetDesc(&main_depth_desc);
               if ((main_depth_desc.Format == DXGI_FORMAT_R24G8_TYPELESS || main_depth_desc.Format == DXGI_FORMAT_D24_UNORM_S8_UINT) && main_depth_desc.Width == copy_desc.Width && main_depth_desc.Height == copy_desc.Height && main_depth_desc.MipLevels == 1 && main_depth_desc.ArraySize == 1 && main_depth_desc.SampleDesc.Count == 1)
               {
                  native_device_context->PSSetShaderResources(0, 1, &null_srv);
                  native_device_context->OMSetRenderTargets(0, nullptr, nullptr);
                  native_device_context->CopyResource(game_device_data.sub_rect_depth_copy.get(), main_depth.get());
                  native_device_context->OMSetRenderTargets(0, nullptr, game_device_data.mv_accepted_dsv);
                  native_device_context->PSSetShaderResources(0, 1, &depth_copy);
                  native_device_context->Draw(4, 0);
               }
            }
         }
      }
      compute_state.Restore(native_device_context);
      graphics_state.Restore(native_device_context);
   }

   // Jitter for the scene's mesh draws without motion vectors (patched vertex shader): the G-buffer, light volumes, depth only and
   // forward draws. Every draw depth tested against the scene takes the same jitter, or jittered and unjittered depths of the same
   // surface fail each other's test. False if it can't (the draw runs untouched).
   static bool DrawWithJitter(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, CommandListData& cmd_list_data, DeviceData& device_data, const ShaderHashesList<OneShaderPerPipeline>& original_shader_hashes, const std::function<void()>& draw, ID3D11DepthStencilView* dsv)
   {
      auto& game_device_data = GetGameDeviceData(device_data);
      if (game_device_data.mv_jitter == std::array<float, 2>{} || !game_device_data.mv_jitter_buffer)
         return false;
      if (!game_device_data.mv_scene_open)
         return false;
#if DEVELOPMENT
      // "Performance Test" without motion vector draws: the whole frame unjittered, so its depth tests stay consistent
      if (perf_test_modes[g_perf_test].motion_vector_draws < 1)
         return false;
#endif
      // Meshes only (full screen passes have no vertex buffer or no depth test), into output sized depth (not shadows or reflections)
      if (!dsv)
         return false;
      if (dsv != game_device_data.jitter_dsv)
      {
         uint4 depth_size;
         DXGI_FORMAT unused_format;
         GetResourceInfo(dsv, depth_size, unused_format);
         game_device_data.jitter_dsv = dsv;
         game_device_data.jitter_dsv_scene_sized = depth_size.x == device_data.output_resolution.x && depth_size.y == device_data.output_resolution.y;
      }
      if (!game_device_data.jitter_dsv_scene_sized)
         return false;
      com_ptr<ID3D11DepthStencilState> depth_stencil_state;
      native_device_context->OMGetDepthStencilState(&depth_stencil_state, nullptr);
      if (depth_stencil_state.get() != game_device_data.jitter_depth_stencil_state)
      {
         D3D11_DEPTH_STENCIL_DESC depth_desc = CD3D11_DEPTH_STENCIL_DESC(D3D11_DEFAULT);
         if (depth_stencil_state)
            depth_stencil_state->GetDesc(&depth_desc);
         game_device_data.jitter_depth_test = depth_desc.DepthEnable;
         game_device_data.jitter_depth_stencil_state = depth_stencil_state.get();
      }
      if (!game_device_data.jitter_depth_test)
         return false;
      com_ptr<ID3D11Buffer> vertex_buffer;
      UINT vertex_stride, vertex_offset;
      native_device_context->IAGetVertexBuffers(0, 1, &vertex_buffer, &vertex_stride, &vertex_offset);
      if (!vertex_buffer)
         return false;
      ID3D11VertexShader* const vertex_shader = GetPatchedVertexShader(native_device, cmd_list_data, device_data, original_shader_hashes.vertex_shaders[0]);
      if (!vertex_shader)
         return false;

      // The patched vertex shader and the jitter stay bound after the draw (see "DrawWithMotionVectors"), with the game's pixel shader
      // (a motion vector draw's is put back), or under the render scale's sub-rect a patched one reading screen textures (soft
      // particles, the stipple DSF)
      PatchedDraws::BindPatchedShader(native_device_context, vertex_shader, &game_device_data.mv_bound_vertex_shader);
      ID3D11Buffer* const jitter = game_device_data.mv_jitter_buffer.get();
      native_device_context->VSSetConstantBuffers(MotionVectorPatches::jitter_slot, 1, &jitter);
      ID3D11PixelShader* pixel_shader = nullptr;
      if (GetSubRectScale(device_data) < 1.f && cmd_list_data.pipeline_state_original_pixel_shader.handle != 0)
      {
         if (const uint32_t pixel_shader_hash = original_shader_hashes.pixel_shaders[0]; pixel_shader_hash != game_device_data.jitter_last_pixel_shader_hash)
         {
            game_device_data.jitter_last_pixel_shader = GetMotionVectorShader<ID3D11PixelShader>(native_device, device_data, pixel_shader_hash, cmd_list_data.pipeline_state_original_pixel_shader, PixelShaderPatch::ScreenUV).get();
            game_device_data.jitter_last_pixel_shader_hash = pixel_shader_hash;
         }
         pixel_shader = game_device_data.jitter_last_pixel_shader;
      }
      if (pixel_shader)
      {
         PatchedDraws::BindPatchedShader(native_device_context, pixel_shader, &game_device_data.mv_bound_pixel_shader);
         native_device_context->PSSetConstantBuffers(MotionVectorPatches::jitter_slot, 1, &jitter);
      }
      else
      {
         PatchedDraws::RestoreGameShader(native_device_context, &game_device_data.mv_bound_pixel_shader);
      }
      draw();
      return true;
   }

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

      using DXBC::Destination, DXBC::RegisterOperand;
      const uint32_t swizzle_mode = ENCODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(D3D10_SB_OPERAND_4_COMPONENT_SWIZZLE_MODE);
      const bool tail_matches =
         tail[0] == (ENCODE_D3D10_SB_OPCODE_TYPE(D3D10_SB_OPCODE_MUL) | ENCODE_D3D10_SB_TOKENIZED_INSTRUCTION_LENGTH(8)) &&
         tail[1] == Destination(D3D10_SB_OPERAND_TYPE_OUTPUT, D3D10_SB_OPERAND_4_COMPONENT_MASK_ALL) && tail[2] == 0 &&
         (tail[3] & ~D3D10_SB_OPERAND_4_COMPONENT_SWIZZLE_MASK) == (RegisterOperand(D3D10_SB_OPERAND_TYPE_TEMP) | swizzle_mode) &&
         (tail[5] & ~D3D10_SB_OPERAND_4_COMPONENT_SWIZZLE_MASK) == (ENCODE_D3D10_SB_OPERAND_NUM_COMPONENTS(D3D10_SB_OPERAND_4_COMPONENT) | ENCODE_D3D10_SB_OPERAND_INDEX_DIMENSION(D3D10_SB_OPERAND_INDEX_2D) | swizzle_mode | ENCODE_D3D10_SB_OPERAND_TYPE(D3D10_SB_OPERAND_TYPE_CONSTANT_BUFFER)) && tail[6] == 4 && tail[7] == 1 &&
         tail[8] == (ENCODE_D3D10_SB_OPCODE_TYPE(D3D10_SB_OPCODE_RET) | ENCODE_D3D10_SB_TOKENIZED_INSTRUCTION_LENGTH(1));
      ASSERT_ONCE(tail_matches);
      if (!tail_matches)
         return nullptr;
      const uint32_t r = tail[4];

      std::vector<uint32_t> patch;
      const auto dest = [&](D3D10_SB_OPERAND_TYPE operand_type, uint32_t index, uint32_t component)
      {
         patch.insert(patch.end(), {Destination(operand_type, D3D10_SB_OPERAND_4_COMPONENT_MASK_X << component), index});
      };
      const auto src = [&](uint32_t component, D3D10_SB_OPERAND_MODIFIER modifier = D3D10_SB_OPERAND_MODIFIER_NONE)
      {
         const uint32_t token = RegisterOperand(D3D10_SB_OPERAND_TYPE_TEMP) | ENCODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(D3D10_SB_OPERAND_4_COMPONENT_SELECT_1_MODE) | ENCODE_D3D10_SB_OPERAND_4_COMPONENT_SELECT_1(component);
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

   // An additive or subtractive draw onto the gamma-encoded HDR output (the vint UI's additive / subtractive render modes, the
   // bokeh sprites). Vanilla's UNORM swapchain clipped its result to [0,1]; here it went past the peak over highlights, or negative.
   // The game's draw, then "SR4 Additive Limit PS" / "SR4 Subtractive Limit PS" with blend op MIN / MAX from a copy of the target
   // taken before it (see "Luma_SR4_BlendLimit.hlsl"). False (the game's draw runs alone) when the target or the copy is unusable.
   bool DrawBlendLimited(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, DeviceData& device_data, const std::function<void()>& original_draw_dispatch_func, ID3D11RenderTargetView* target_rtv, ID3D11Resource* target_resource, OutputBlend blend)
   {
      com_ptr<ID3D11Texture2D> target_texture;
      if (FAILED(target_resource->QueryInterface(&target_texture)))
         return false;
      D3D11_TEXTURE2D_DESC desc;
      target_texture->GetDesc(&desc);
      if (desc.SampleDesc.Count != 1 || desc.ArraySize != 1)
         return false;
      auto& game_device_data = GetGameDeviceData(device_data);
      const bool subtractive = blend == OutputBlend::Subtractive;
      const uint32_t limit_pixel_shader = (subtractive ? "SR4 Subtractive Limit PS"_h : "SR4 Additive Limit PS"_h);

      D3D11_TEXTURE2D_DESC base_desc = {};
      if (game_device_data.blend_limit_base_texture)
         game_device_data.blend_limit_base_texture->GetDesc(&base_desc);
      if (base_desc.Width != desc.Width || base_desc.Height != desc.Height || base_desc.Format != desc.Format || base_desc.MipLevels != desc.MipLevels)
      {
         game_device_data.blend_limit_base_srv.reset();
         game_device_data.blend_limit_base_texture.reset();
         desc.Usage = D3D11_USAGE_DEFAULT;
         desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
         desc.CPUAccessFlags = 0;
         desc.MiscFlags = 0;
         if (FAILED(native_device->CreateTexture2D(&desc, nullptr, &game_device_data.blend_limit_base_texture)) || FAILED(native_device->CreateShaderResourceView(game_device_data.blend_limit_base_texture.get(), nullptr, &game_device_data.blend_limit_base_srv)))
         {
            game_device_data.blend_limit_base_srv.reset();
            game_device_data.blend_limit_base_texture.reset();
            return false;
         }
      }
      com_ptr<ID3D11BlendState>& blend_state = game_device_data.blend_limit_states[subtractive ? 1 : 0];
      if (!blend_state)
      {
         D3D11_BLEND_DESC blend_desc = {};
         auto& rt = blend_desc.RenderTarget[0];
         rt.BlendEnable = TRUE;
         rt.SrcBlend = rt.DestBlend = rt.SrcBlendAlpha = rt.DestBlendAlpha = D3D11_BLEND_ONE;
         rt.BlendOp = rt.BlendOpAlpha = (subtractive ? D3D11_BLEND_OP_MAX : D3D11_BLEND_OP_MIN);
         rt.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_RED | D3D11_COLOR_WRITE_ENABLE_GREEN | D3D11_COLOR_WRITE_ENABLE_BLUE;
         if (FAILED(native_device->CreateBlendState(&blend_desc, &blend_state)))
            return false;
      }

      native_device_context->CopyResource(game_device_data.blend_limit_base_texture.get(), target_resource);
      original_draw_dispatch_func();
      // The shaders are looked up after the game's draw, which runs outside Luma's locks: without them (still compiling, a reload) the
      // draw stays unlimited
      const std::shared_lock lock_shader_objects(s_mutex_shader_objects);
      if (!HasShaders(device_data.native_vertex_shaders, "Copy VS"_h) || !HasShaders(device_data.native_pixel_shaders, limit_pixel_shader))
         return true;
      DrawStateStack<DrawStateStackType::FullGraphics> limit_state;
      limit_state.Cache(native_device_context, device_data.uav_max_count);
      DrawCustomPixelShader(native_device_context, device_data.default_depth_stencil_state.get(), blend_state.get(), nullptr, device_data.native_vertex_shaders.at("Copy VS"_h).get(), device_data.native_pixel_shaders.at(limit_pixel_shader).get(), game_device_data.blend_limit_base_srv.get(), target_rtv, desc.Width, desc.Height);
      limit_state.Restore(native_device_context);
      return true;
   }

   // Draws the final composite, then SMAA and/or RCAS on the canvas it wrote (the swapchain), before DoF and the UI read it.
   // Anything missing (shaders still compiling, an unexpected target) leaves the composite as the game draws it.
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

      // Held through SMAA so a shader reload cannot release them mid-use ("DrawSMAA" looks its shaders up with "at"), except over the
      // game's draw, which runs outside Luma's locks: checked again after it. Without "smaa" (the upscaler already antialiased the
      // scene) only RCAS runs.
      std::shared_lock lock_shader_objects(s_mutex_shader_objects);
      const auto has_sharpen_shaders = [&]
      { return HasShaders(device_data.native_vertex_shaders, "Copy VS"_h) && HasShaders(device_data.native_pixel_shaders, "SR4 Sharpen PS"_h); };
      const auto has_smaa_shaders = [&]
      { return HasShaders(device_data.native_vertex_shaders, "SMAA Edge Detection VS"_h, "SMAA Blending Weight Calculation VS"_h, "SMAA Neighborhood Blending VS"_h) && HasShaders(device_data.native_pixel_shaders, "SMAA Edge Detection PS"_h, "SMAA Blending Weight Calculation PS"_h, "SMAA Neighborhood Blending PS"_h); };
      const bool sharpen = g_rcas_sharpness > 0.f && has_sharpen_shaders();
      if (!smaa && !sharpen)
         return DrawOrDispatchOverrideType::None;
      if (smaa && !has_smaa_shaders())
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
         game_device_data.smaa_predication_uav.reset();
         game_device_data.smaa_predication_srv.reset();
         desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
         if (FAILED(native_device->CreateTexture2D(&desc, nullptr, &game_device_data.smaa_gamma_texture)) || FAILED(native_device->CreateShaderResourceView(game_device_data.smaa_gamma_texture.get(), nullptr, &game_device_data.smaa_gamma_srv)))
         {
            game_device_data.smaa_gamma_texture.reset();
            return DrawOrDispatchOverrideType::None;
         }
      }
      // Without it SMAA simply runs unpredicated
      if (smaa && !game_device_data.smaa_predication_srv)
      {
         desc.Format = DXGI_FORMAT_R16_FLOAT;
         desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
         com_ptr<ID3D11Texture2D> predication_texture;
         if (FAILED(native_device->CreateTexture2D(&desc, nullptr, &predication_texture)) || FAILED(native_device->CreateUnorderedAccessView(predication_texture.get(), nullptr, &game_device_data.smaa_predication_uav)) || FAILED(native_device->CreateShaderResourceView(predication_texture.get(), nullptr, &game_device_data.smaa_predication_srv)))
         {
            game_device_data.smaa_predication_uav.reset();
            game_device_data.smaa_predication_srv.reset();
         }
      }

      // Predication depth: the final composite's own depth input (t5, the frame depth the DoF weight reads, R24).
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

      lock_shader_objects.unlock();
      original_draw_dispatch_func();
      lock_shader_objects.lock();
      // A shader reload during the game's draw: the composite stays as drawn this frame
      if ((smaa && !has_smaa_shaders()) || (sharpen && !has_sharpen_shaders()) || (depth_srv && !HasShaders(device_data.native_compute_shaders, "SR4 SMAA Predication CS"_h)))
         return DrawOrDispatchOverrideType::Replaced;
      native_device_context->CopyResource(game_device_data.smaa_gamma_texture.get(), canvas_resource.get());
      game_device_data.snapshot_frame = cb_luma_global_settings.FrameIndex;

      if (smaa)
      {
         game_device_data.smaa_frame = cb_luma_global_settings.FrameIndex;
         if (depth_srv)
         {
            DrawStateStack<DrawStateStackType::Compute> predication_state;
            predication_state.Cache(native_device_context, device_data.uav_max_count);
            ID3D11UnorderedAccessView* const predication_uav = game_device_data.smaa_predication_uav.get();
            ID3D11ShaderResourceView* const raw_depth_srv = depth_srv.get();
            native_device_context->CSSetUnorderedAccessViews(0, 1, &predication_uav, nullptr);
            native_device_context->CSSetShaderResources(0, 1, &raw_depth_srv);
            native_device_context->CSSetShader(device_data.native_compute_shaders.at("SR4 SMAA Predication CS"_h).get(), nullptr, 0);
            native_device_context->Dispatch((canvas_desc.Width + 7) / 8, (canvas_desc.Height + 7) / 8, 1);
            predication_state.Restore(native_device_context);
         }

         // The SMAA shaders read the canvas size from the Luma settings and the predication scale from the Luma data, in both stages.
         SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, reshade::api::shader_stage::vertex | reshade::api::shader_stage::pixel, LumaConstantBufferType::LumaSettings);
         SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, reshade::api::shader_stage::vertex | reshade::api::shader_stage::pixel, LumaConstantBufferType::LumaData, 0, 0, depth_srv ? 2.f : 1.f);
         DrawSMAA(native_device, native_device_context, device_data, canvas_rtv.get(), game_device_data.smaa_gamma_srv.get(), game_device_data.smaa_gamma_srv.get(), depth_srv ? game_device_data.smaa_predication_srv.get() : nullptr);
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
         // RCAS sharpens SMAA's output
         if (smaa)
         {
            native_device_context->CopyResource(game_device_data.smaa_gamma_texture.get(), canvas_resource.get());
         }
         DrawStateStack<DrawStateStackType::FullGraphics> sharpen_state;
         sharpen_state.Cache(native_device_context, device_data.uav_max_count);
         SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, reshade::api::shader_stage::pixel, LumaConstantBufferType::LumaData, 0, 0, g_rcas_sharpness);
         DrawCustomPixelShader(native_device_context, device_data.default_depth_stencil_state.get(), device_data.default_blend_state.get(), nullptr, device_data.native_vertex_shaders.at("Copy VS"_h).get(), device_data.native_pixel_shaders.at("SR4 Sharpen PS"_h).get(), game_device_data.smaa_gamma_srv.get(), canvas_rtv.get(), canvas_desc.Width, canvas_desc.Height, false);
         sharpen_state.Restore(native_device_context);
      }
      return DrawOrDispatchOverrideType::Replaced;
   }

   // XeGTAO in place of the first singleframe calculate draw: prefilter, main pass and denoise (two passes, one under DLSS/FSR) on
   // the draw's own inputs (t14 depth, t13 normals, vc0 at b0 for this frame's projection), then a copy into its render target.
   // Returns false, and the native draw runs, when an input, a shader or the scratch is missing.
   bool RunXeGTAO(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, DeviceData& device_data)
   {
      // Held through the dispatches so a shader reload cannot release them mid-use.
      const std::shared_lock lock_shader_objects(s_mutex_shader_objects);
      const auto& shaders = device_data.native_compute_shaders;
      if (!HasShaders(shaders, "SR4 XeGTAO Prefilter Depths CS"_h, "SR4 XeGTAO Main Pass CS"_h, "SR4 XeGTAO Denoise Pass 1 CS"_h, "SR4 XeGTAO Denoise Pass 2 CS"_h, "SR4 XeGTAO Downsample CS"_h))
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
      // DLSS/FSR accumulate the lit scene the AO multiplies into: cycle the noise and denoise once (Intel's XeGTAO.h with TAA);
      // without them a moving pattern would boil, so it stays frozen and denoises twice. Full resolution mode (with DLSS/FSR only,
      // +0.55 ms at 4K with DLSS K in the "Performance Test" sweep): every pass at the depth's size, averaged into the target at the end
      // ("downsample_cs"). At half res the upscaler's jitter flips a target pixel's one depth texel between grass blades and the
      // ground: the AO boils.
      const bool temporal = IsSRActive(device_data);
#if DEVELOPMENT
      const bool full_res = (g_gtao_full_res ? (g_gtao_full_res == 2) : temporal);
      const bool noise_per_frame = (g_gtao_noise_per_frame ? (g_gtao_noise_per_frame == 2) : temporal);
      const bool single_denoise = (g_gtao_single_denoise ? (g_gtao_single_denoise == 2) : temporal);
#else
      const bool full_res = temporal, noise_per_frame = temporal, single_denoise = temporal;
#endif
      const uint32_t work_width = (full_res ? depth_size.x : width), work_height = (full_res ? depth_size.y : height);

      if (game_device_data.gtao_width != work_width || game_device_data.gtao_height != work_height || game_device_data.gtao_format != target_rtv_desc.Format)
      {
         game_device_data.ReleaseGTAOScratch();
         D3D11_TEXTURE2D_DESC desc = {};
         desc.Width = work_width;
         desc.Height = work_height;
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
         desc.Width = width;
         desc.Height = height;
         desc.Format = target_rtv_desc.Format;
         ok = ok && SUCCEEDED(native_device->CreateTexture2D(&desc, nullptr, &game_device_data.gtao_final_texture)) && SUCCEEDED(native_device->CreateUnorderedAccessView(game_device_data.gtao_final_texture.get(), nullptr, &game_device_data.gtao_final_uav));
         if (!ok)
            game_device_data.ReleaseGTAOScratch();
         game_device_data.gtao_width = work_width;
         game_device_data.gtao_height = work_height;
         game_device_data.gtao_format = target_rtv_desc.Format;
      }
      if (!game_device_data.gtao_final_uav)
         return false;

#if DEVELOPMENT
      const float debug_view = float(g_gtao_debug_view);
#else
      const float debug_view = 0.f;
#endif
      // Render scale: the scene fills the target's top-left share (see "g_render_scale"): the shader's ndc follow it, the main
      // pass and the denoise run over it only (the depth prefilter covers the whole target, so samples past the edge read cleared depth)
      const bool sub_rect = GetSubRectScale(device_data) < 1.f;
      const float sub_rect_scale[2] = {(sub_rect ? (float(game_device_data.mv_render_size[0]) / device_data.output_resolution.x) : 1.f), (sub_rect ? (float(game_device_data.mv_render_size[1]) / device_data.output_resolution.y) : 1.f)};
      const UINT ao_width = (std::min)(work_width, UINT(std::ceil(float(work_width) * sub_rect_scale[0]))), ao_height = (std::min)(work_height, UINT(std::ceil(float(work_height) * sub_rect_scale[1])));
      const float knobs[12] = {g_gtao_final_value_power, (full_res ? 1.f : float(input_scale)), g_gtao_radius_override, debug_view, 1.f / float(work_width), 1.f / float(work_height), (noise_per_frame ? float(cb_luma_global_settings.FrameIndex % 64) : 0.f), float(input_scale), sub_rect_scale[0], sub_rect_scale[1], g_gtao_thin_occluder_override, 0.f};
      if (!PatchedDraws::WriteDynamicConstants(native_device, native_device_context, std::addressof(game_device_data.gtao_knobs_cb), knobs, sizeof(knobs)))
         return false;

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
      ID3D11UnorderedAccessView* mip_uavs[gtao_depth_mip_count];
      for (UINT mip = 0; mip < gtao_depth_mip_count; mip++)
         mip_uavs[mip] = game_device_data.gtao_depth_mip_uavs[mip].get();
      ID3D11UnorderedAccessView* const working_uavs[2] = {game_device_data.gtao_working_uavs[0].get(), game_device_data.gtao_working_uavs[1].get()};
      ID3D11UnorderedAccessView* const final_uav = game_device_data.gtao_final_uav.get();
      pass("SR4 XeGTAO Prefilter Depths CS"_h, gtao_depth_mip_count, mip_uavs, {depth_srv.get(), nullptr}, (work_width + 15) / 16, (work_height + 15) / 16);
      pass("SR4 XeGTAO Main Pass CS"_h, 1, &working_uavs[0], {game_device_data.gtao_depth_mips_srv.get(), normals_srv.get()}, (ao_width + 7) / 8, (ao_height + 7) / 8);
      if (full_res)
      {
         // Both denoise passes stay at full res (working 0 -> 1 -> 0), then the average into the whole target: past the render scale's
         // share it repeats the share's edge, which the game's bilinear apply and its blur read
         pass("SR4 XeGTAO Denoise Pass 1 CS"_h, 1, &working_uavs[1], {game_device_data.gtao_working_srvs[0].get(), nullptr}, (ao_width + 15) / 16, (ao_height + 7) / 8);
         if (!single_denoise)
            pass("SR4 XeGTAO Denoise Pass 1 CS"_h, 1, &working_uavs[0], {game_device_data.gtao_working_srvs[1].get(), nullptr}, (ao_width + 15) / 16, (ao_height + 7) / 8);
         pass("SR4 XeGTAO Downsample CS"_h, 1, &final_uav, {game_device_data.gtao_working_srvs[single_denoise ? 1 : 0].get(), nullptr}, (width + 7) / 8, (height + 7) / 8);
      }
      else
      {
         if (!single_denoise)
            pass("SR4 XeGTAO Denoise Pass 1 CS"_h, 1, &working_uavs[1], {game_device_data.gtao_working_srvs[0].get(), nullptr}, (ao_width + 15) / 16, (ao_height + 7) / 8);
         pass("SR4 XeGTAO Denoise Pass 2 CS"_h, 1, &final_uav, {game_device_data.gtao_working_srvs[single_denoise ? 0 : 1].get(), nullptr}, (ao_width + 15) / 16, (ao_height + 7) / 8);
      }
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
      // XeGTAO in place of the SSAO calculate (it follows the render scale's sub-rect itself), decided on the frame's first draw so the
      // four AO channels never mix XeGTAO and native. Off or failed, the native draws go on like any other scene draw ("DrawSubRect").
      if (g_gtao_enable && pixel_shader_hash == ssao_singleframe_calculate_pixel_shader)
      {
         const bool first_draw = !std::exchange(game_device_data.gtao_tried_this_frame, true);
         if (first_draw)
         {
            game_device_data.gtao_ran_this_frame = RunXeGTAO(native_device, native_device_context, device_data);
         }
         if (game_device_data.gtao_ran_this_frame)
            return (first_draw ? DrawOrDispatchOverrideType::Replaced : DrawOrDispatchOverrideType::Skip);
      }
      if (game_device_data.mv_active && !is_custom_pass && original_draw_dispatch_func && *original_draw_dispatch_func && (stages & reshade::api::shader_stage::vertex) == reshade::api::shader_stage::vertex)
      {
         if (pixel_shader_hash == downsample_pixel_shader || pixel_shader_hash == god_rays_mask_pixel_shader || (game_device_data.mv_scene_copied && post_process_pixel_shaders.contains(pixel_shader_hash)))
         {
            // The first downsample reads a copy of the material target; its source is kept (see "mv_scene_color")
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
               constants[18] = float(game_device_data.mv_render_size[0]);
               constants[19] = float(game_device_data.mv_render_size[1]);
               const std::shared_lock lock_shader_objects(s_mutex_shader_objects);
               // ponytail: a shader reload between the frame start and here (DEV) leaves the fill's marker for a frame
               if (HasShaders(device_data.native_compute_shaders, "SR4 Motion Vector Fill CS"_h) && PatchedDraws::WriteDynamicConstants(native_device, native_device_context, std::addressof(game_device_data.mv_fill_buffer), constants, sizeof(constants)))
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
                  native_device_context->Dispatch((game_device_data.mv_render_size[0] + 7) / 8, (game_device_data.mv_render_size[1] + 7) / 8, 1);
                  compute_state.Restore(native_device_context);
                  graphics_state.Restore(native_device_context);
               }
            }
            if (!game_device_data.mv_frame_ended)
            {
#if DEVELOPMENT
               auto* const perf_queries = game_device_data.perf_frame_queries;
               if (perf_queries && perf_queries->scene_started && !perf_queries->scene)
               {
                  native_device_context->End(perf_queries->scene_end.get());
                  perf_queries->scene = true;
               }
#endif
               if (IsSRActive(device_data) || GetSubRectScale(device_data) < 1.f)
                  ResolveScene(native_device, native_device_context, device_data);
#if DEVELOPMENT
               if (perf_queries && perf_queries->scene && !perf_queries->sr && device_data.has_drawn_sr)
               {
                  native_device_context->End(perf_queries->sr_end.get());
                  perf_queries->sr = true;
               }
#endif
            }
            game_device_data.mv_frame_ended = true;
            game_device_data.mv_scene_open = false;
            RestoreSceneViewport(native_device_context, &game_device_data);
         }
         else
         {
#if DEVELOPMENT
            const PerfHookTimer timer{game_device_data.perf_hook_ns}; // The draw's own submission included
#endif
            com_ptr<ID3D11RenderTargetView> rtvs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
            com_ptr<ID3D11DepthStencilView> dsv;
            native_device_context->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, &rtvs[0], &dsv);
            const bool scene_was_open = game_device_data.mv_scene_open;
            // The sun shadow terms take the jitter out of the NDC they rebuild the position from at every render scale (see
            // "MotionVectorPatches::PatchScreenUVPixelShader"), the other screen texture draws only under the sub-rect
            if (scene_was_open && (ScaleSceneViewport(native_device_context, device_data, original_shader_hashes) || (GetSubRectScale(device_data) >= 1.f && std::ranges::contains(sub_rect_uv_pixel_shaders, pixel_shader_hash))) && DrawSubRect(native_device, native_device_context, cmd_list_data, device_data, original_shader_hashes, *original_draw_dispatch_func))
               return DrawOrDispatchOverrideType::Replaced;
            const bool motion_vectors = DrawWithMotionVectors(native_device, native_device_context, cmd_list_data, device_data, original_shader_hashes, *original_draw_dispatch_func, rtvs, dsv.get());
            // The G-buffer draw that opens the scene (never drawn above)
            if (!scene_was_open && game_device_data.mv_scene_open)
               ScaleSceneViewport(native_device_context, device_data, original_shader_hashes);
            const bool jittered = motion_vectors || DrawWithJitter(native_device, native_device_context, cmd_list_data, device_data, original_shader_hashes, *original_draw_dispatch_func, dsv.get());
#if DEVELOPMENT
            Mcp::Annotate(cmd_list_data, motion_vectors ? "mv" : (jittered ? "jitter" : "unpatched"));
#endif
            if (jittered)
               return DrawOrDispatchOverrideType::Replaced;
         }
      }
      // Every draw without a patched shader: the game's own, if the last patched draw's are still bound
      PatchedDraws::RestoreGameShader(native_device_context, &game_device_data.mv_bound_vertex_shader);
      PatchedDraws::RestoreGameShader(native_device_context, &game_device_data.mv_bound_pixel_shader);
      // Render scale: a post pass reading the distortion map, which the scene drew into the sub-rect, at its share of the UV
      // (patched, left bound like the motion vector shaders, the jitter buffer's zw)
      if (GetSubRectScale(device_data) < 1.f && game_device_data.mv_jitter_buffer && std::ranges::contains(sub_rect_distortion_pixel_shaders, pixel_shader_hash, &std::pair<uint32_t, uint32_t>::first))
      {
         if (const com_ptr<ID3D11PixelShader> pixel_shader = GetMotionVectorShader<ID3D11PixelShader>(native_device, device_data, pixel_shader_hash, cmd_list_data.pipeline_state_original_pixel_shader, PixelShaderPatch::ScreenUV))
         {
            PatchedDraws::BindPatchedShader(native_device_context, pixel_shader.get(), &game_device_data.mv_bound_pixel_shader);
            ID3D11Buffer* const sub_rect = game_device_data.mv_jitter_buffer.get();
            native_device_context->PSSetConstantBuffers(MotionVectorPatches::jitter_slot, 1, &sub_rect);
         }
      }
      // The native bloom constants only feed Luma bloom.
      if (g_luma_bloom_enable)
      {
         if (pixel_shader_hash == bloom_brightpass_pixel_shader)
         {
            CopyBoundPSConstantBuffer(native_device, native_device_context, 0, std::addressof(game_device_data.bloom_brightpass_vc0_cb));
            CopyBoundPSConstantBuffer(native_device, native_device_context, 4, std::addressof(game_device_data.bloom_brightpass_vc4_cb));
            return DrawOrDispatchOverrideType::None;
         }
         if (pixel_shader_hash == bloom_combine_pixel_shader)
         {
            CopyBoundPSConstantBuffer(native_device, native_device_context, 4, std::addressof(game_device_data.bloom_combine_vc4_cb));
            return DrawOrDispatchOverrideType::None;
         }
         if (!game_device_data.bloom_source_downsampled && pixel_shader_hash == downsample_pixel_shader)
         {
            game_device_data.bloom_source_downsampled = true;
            CopyBoundPSConstantBuffer(native_device, native_device_context, 4, std::addressof(game_device_data.bloom_source_downsample_vc4_cb));
            return DrawOrDispatchOverrideType::None;
         }
      }
      // The distortion map's share of the target for the replacements that read it (render scale, 0 = the whole target), set at
      // each of their draws so they never read the SMAA/RCAS values left in LumaData
      const auto set_distortion_map_share = [&]
      {
         const bool sub_rect = GetSubRectScale(device_data) < 1.f;
         SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, reshade::api::shader_stage::pixel, LumaConstantBufferType::LumaSettings);
         SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, reshade::api::shader_stage::pixel, LumaConstantBufferType::LumaData, 0, 0, sub_rect ? float(game_device_data.mv_render_size[0]) / device_data.output_resolution.x : 0.f, sub_rect ? float(game_device_data.mv_render_size[1]) / device_data.output_resolution.y : 0.f);
         updated_cbuffers = true;
      };
      if (pixel_shader_hash == distortion_pixel_shader)
      {
         set_distortion_map_share();
         return DrawOrDispatchOverrideType::None;
      }
      if (std::ranges::contains(tonemap_pixel_shaders, pixel_shader_hash, &std::pair<uint32_t, const char*>::first))
      {
         // A 3D scene was composited this frame: Core then shows whether DLSS/FSR ran (the tick next to "Super Resolution"). No
         // UI separation, UI shader replacements or UI background tonemap here, so nothing else reads it.
         device_data.has_drawn_main_post_processing = true;
         set_distortion_map_share();
#if DEVELOPMENT
         g_final_perm = pixel_shader_hash;
         g_finals_this_frame++;
#endif
         if (g_luma_msaa_enable) // only the weighted resolve reads it
            CopyBoundPSConstantBuffer(native_device, native_device_context, 4, std::addressof(game_device_data.composite_tint_cb));

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
      // The UI and the bokeh sprites onto the swapchain (any off-screen use of the same shaders is left alone): the UI dropped with
      // "Hide Gameplay UI", and an additive or subtractive draw limited to the vanilla UNORM range (see "DrawBlendLimited").
      const bool ui = ui_pixel_shaders.contains(pixel_shader_hash);
      if (ui || pixel_shader_hash == bokeh_sprite_pixel_shader)
      {
         com_ptr<ID3D11RenderTargetView> rtv;
         native_device_context->OMGetRenderTargets(1, &rtv, nullptr);
         com_ptr<ID3D11Resource> rtv_resource;
         if (rtv)
            rtv->GetResource(&rtv_resource);
         bool back_buffer = false;
         if (rtv_resource)
         {
            const std::shared_lock lock(device_data.mutex);
            back_buffer = device_data.back_buffers.contains(reinterpret_cast<uint64_t>(rtv_resource.get()));
         }
         if (!back_buffer)
            return DrawOrDispatchOverrideType::None;
         if (ui && g_hide_ui)
            return DrawOrDispatchOverrideType::Replaced;
         com_ptr<ID3D11BlendState> blend_state;
         native_device_context->OMGetBlendState(&blend_state, nullptr, nullptr);
         D3D11_BLEND_DESC blend_desc = {};
         if (blend_state)
            blend_state->GetDesc(&blend_desc);
         const OutputBlend blend = GetOutputBlend(blend_desc.RenderTarget[0]);
         if (blend != OutputBlend::Other && original_draw_dispatch_func && *original_draw_dispatch_func && DrawBlendLimited(native_device, native_device_context, device_data, *original_draw_dispatch_func, rtv.get(), rtv_resource.get(), blend))
            return DrawOrDispatchOverrideType::Replaced;
         return DrawOrDispatchOverrideType::None;
      }

      // Alpha-tested material draws into the MSAA scene get alpha to coverage, with the sharpened alpha (see "PatchShaderBytecodeSync").
      // Only opaque ones (blending off or One/Zero, as foliage): the same shaders also draw alpha-blended decals and windows, whose
      // alpha is their opacity. The game's blend state and shader are handed back after the draw.
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
#if DEVELOPMENT
      Mcp::Annotate(cmd_list_data, "a2c");
#endif
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

   // Every blend state writes the motion vector target ("MotionVectorPatches::target_slot", bound only by the motion vector draws)
   // unblended: no per-draw copy of the game's state. ReShade turns independent blending on only when a target now differs.
   static bool OnCreateBlendState(reshade::api::device* device, reshade::api::pipeline_layout layout, uint32_t subobject_count, const reshade::api::pipeline_subobject* subobjects)
   {
      for (uint32_t i = 0; i < subobject_count; i++)
      {
         if (subobjects[i].type != reshade::api::pipeline_subobject_type::blend_state)
            continue;
         auto& desc = *static_cast<reshade::api::blend_desc*>(subobjects[i].data);
         desc.blend_enable[MotionVectorPatches::target_slot] = false;
         desc.render_target_write_mask[MotionVectorPatches::target_slot] = 0xF;
         return true;
      }
      return false;
   }

   // The game's one ResolveSubresource (grab_scene_color) resolves the FP16 MSAA scene before the tonemap: draw
   // Luma_SR4_MSAAResolve instead. Anything else, or a target we cannot bind, keeps the hardware resolve.
   static bool OnResolveTextureRegion(reshade::api::command_list* cmd_list, reshade::api::resource source, uint32_t source_subresource, const reshade::api::subresource_box* source_box, reshade::api::resource dest, uint32_t dest_subresource, uint32_t dest_x, uint32_t dest_y, uint32_t dest_z, reshade::api::format format)
   {
      DeviceData* const device_data_pointer = cmd_list->get_device()->get_private_data<DeviceData>();
      if (!device_data_pointer || !device_data_pointer->game || format != reshade::api::format::r16g16b16a16_float)
         return false;
      DeviceData& device_data = *device_data_pointer;
      auto& game_device_data = GetGameDeviceData(device_data);
      game_device_data.msaa_scene_resolved = true;
      if (!g_luma_msaa_enable || source_subresource != 0 || dest_subresource != 0 || source_box != nullptr || dest_x != 0 || dest_y != 0 || dest_z != 0)
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
#if DEVELOPMENT
      // For the MCP "luma_dev_values" tool (the counters are the last complete frame's)
      Mcp::RegisterToggles({{"mv_enable", &g_mv_enable}, {"mv_debug_view", &g_mv_debug_view}, {"mv_force_jitter", &g_mv_force_jitter}, {"mv_disable_jitter", &g_mv_disable_jitter},
         {"mv_buffer_filter", &g_mv_buffer_filter}, {"mv_constants_pool", &g_mv_constants_pool}, {"mv_read_sizes", &g_mv_read_sizes}, {"mv_half_float", &g_mv_half_float}});
      Mcp::RegisterCounters({{"msaa_resolves", &g_msaa_resolves_last_frame}, {"finals", &g_finals_last_frame}, {"pixel_steps_overrides", &g_pixel_steps_overrides_last_frame},
         {"sub_rect_draws", &g_sub_rect_draws_last_frame}, {"mv.tiebreak_collisions", &g_mv_tiebreak_collisions_last_frame}});
      Mcp::RegisterToggles({{"luma_msaa_enable", &g_luma_msaa_enable}, {"smaa_enable", &g_smaa_enable}, {"smaa_predication", &g_smaa_predication}, {"smaa_edges_debug", &g_smaa_edges_debug},
         {"luma_bloom_enable", &g_luma_bloom_enable}, {"gtao_enable", &g_gtao_enable}, {"hide_ui", &g_hide_ui}, {"perf_hook_timers", &g_perf_hook_timers}});
      Mcp::RegisterValues({{"rcas_sharpness", &g_rcas_sharpness, 0.f, 1.f}, {"render_scale", &g_render_scale, min_render_scale, 1.f}, {"gtao_final_value_power", &g_gtao_final_value_power, 0.3f, 4.5f},
         {"gtao_radius_override", &g_gtao_radius_override, 0.f, 5.f}, {"gtao_thin_occluder_override", &g_gtao_thin_occluder_override, 0.f, 1.f}});
      Mcp::RegisterInts({{"gtao_debug_view", &g_gtao_debug_view, 0, 4}, {"gtao_noise_per_frame", &g_gtao_noise_per_frame, 0, 2}, {"gtao_single_denoise", &g_gtao_single_denoise, 0, 2},
         {"gtao_full_res", &g_gtao_full_res, 0, 2},
         {"perf_test", &g_perf_test, 0, int(std::size(perf_test_modes)) - 1, [](DeviceData& device_data, double value)
            {
               // As the "Performance Test" combo
               const PerfTestMode& mode = perf_test_modes[int(value)];
               if (!IsPerfTestModeAvailable(device_data, mode))
                  return std::format("{} needs an upscaler this GPU doesn't have", mode.name);
               g_perf_sweep = false;
               ApplyPerfTestMode(device_data, int(value));
               return std::string();
            }}});
      Mcp::RegisterTextures({MCP_GAME_TEXTURE("smaa.input", smaa_gamma_texture),
         MCP_GAME_TEXTURE("smaa.pred_mask", smaa_predication_srv),
         MCP_GAME_TEXTURE("blend_limit.base", blend_limit_base_texture),
         MCP_GAME_TEXTURE("gtao.depth_mips", gtao_depth_mips_texture),
         MCP_GAME_TEXTURE("gtao.output", gtao_final_texture),
         MCP_GAME_TEXTURE("mv.velocity", mv_texture),
         MCP_GAME_TEXTURE("mv.depth", mv_frame_depth),
         MCP_GAME_TEXTURE("render_scale.depth_copy", sub_rect_depth_copy),
         MCP_GAME_TEXTURE("render_scale.color_copy", sub_rect_color_copy)});
#endif
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
      native_shaders_definitions.emplace(CompileTimeStringHash("SR4 Sub Rect Quad VS"), ShaderDefinition{"Luma_SR4_SubRectQuad", reshade::api::pipeline_subobject_type::vertex_shader, nullptr, "quad_vs"});
      native_shaders_definitions.emplace(CompileTimeStringHash("SR4 Sub Rect Light VS"), ShaderDefinition{"Luma_SR4_SubRectQuad", reshade::api::pipeline_subobject_type::vertex_shader, nullptr, "light_vs", {{"LIGHT_VIEW_RAY_UNIT_Z", "0"}}});
      native_shaders_definitions.emplace(CompileTimeStringHash("SR4 Sub Rect Light Unit Z VS"), ShaderDefinition{"Luma_SR4_SubRectQuad", reshade::api::pipeline_subobject_type::vertex_shader, nullptr, "light_vs", {{"LIGHT_VIEW_RAY_UNIT_Z", "1"}}});
      native_shaders_definitions.emplace(CompileTimeStringHash("SR4 Sub Rect SSAO VS"), ShaderDefinition{"Luma_SR4_SubRectQuad", reshade::api::pipeline_subobject_type::vertex_shader, nullptr, "ssao_vs"});
      native_shaders_definitions.emplace(CompileTimeStringHash("SR4 Sub Rect Depth Upscale PS"), ShaderDefinition{"Luma_SR4_SubRectQuad", reshade::api::pipeline_subobject_type::pixel_shader, nullptr, "depth_upscale_ps"});
      native_shaders_definitions.emplace(CompileTimeStringHash("SR4 Sub Rect Color Upscale PS"), ShaderDefinition{"Luma_SR4_SubRectQuad", reshade::api::pipeline_subobject_type::pixel_shader, nullptr, "color_upscale_ps"});
      native_shaders_definitions.emplace(CompileTimeStringHash("SR4 SMAA Predication CS"), ShaderDefinition{"Luma_SR4_SMAAPredication", reshade::api::pipeline_subobject_type::compute_shader});
      native_shaders_definitions.emplace(CompileTimeStringHash("SR4 Additive Limit PS"), ShaderDefinition{"Luma_SR4_BlendLimit", reshade::api::pipeline_subobject_type::pixel_shader, nullptr, "additive_limit_ps"});
      native_shaders_definitions.emplace(CompileTimeStringHash("SR4 Subtractive Limit PS"), ShaderDefinition{"Luma_SR4_BlendLimit", reshade::api::pipeline_subobject_type::pixel_shader, nullptr, "subtractive_limit_ps"});
      native_shaders_definitions.emplace(CompileTimeStringHash("SR4 Sharpen PS"), ShaderDefinition{"Luma_SR4_Sharpen", reshade::api::pipeline_subobject_type::pixel_shader, nullptr, "sharpen_ps"});
      // XeGTAO passes (Luma_SR4_XeGTAO.hlsl); the two denoisers differ only by XE_GTAO_FINAL_APPLY.
      native_shaders_definitions.emplace(CompileTimeStringHash("SR4 XeGTAO Prefilter Depths CS"), ShaderDefinition{"Luma_SR4_XeGTAO", reshade::api::pipeline_subobject_type::compute_shader, nullptr, "prefilter_depths16x16_cs"});
      native_shaders_definitions.emplace(CompileTimeStringHash("SR4 XeGTAO Main Pass CS"), ShaderDefinition{"Luma_SR4_XeGTAO", reshade::api::pipeline_subobject_type::compute_shader, nullptr, "main_pass_cs"});
      native_shaders_definitions.emplace(CompileTimeStringHash("SR4 XeGTAO Denoise Pass 1 CS"), ShaderDefinition{"Luma_SR4_XeGTAO", reshade::api::pipeline_subobject_type::compute_shader, nullptr, "denoise_pass_cs", {{"XE_GTAO_FINAL_APPLY", "0"}}});
      native_shaders_definitions.emplace(CompileTimeStringHash("SR4 XeGTAO Denoise Pass 2 CS"), ShaderDefinition{"Luma_SR4_XeGTAO", reshade::api::pipeline_subobject_type::compute_shader, nullptr, "denoise_pass_cs", {{"XE_GTAO_FINAL_APPLY", "1"}}});
      native_shaders_definitions.emplace(CompileTimeStringHash("SR4 XeGTAO Downsample CS"), ShaderDefinition{"Luma_SR4_XeGTAO", reshade::api::pipeline_subobject_type::compute_shader, nullptr, "downsample_cs", {{"XE_GTAO_FINAL_APPLY", "1"}}});
      reshade::register_event<reshade::addon_event::create_resource>(OnCreateResource);
      reshade::register_event<reshade::addon_event::create_pipeline>(OnCreateBlendState);
      reshade::register_event<reshade::addon_event::resolve_texture_region>(OnResolveTextureRegion);
      reshade::register_event<reshade::addon_event::map_buffer_region>(OnMapBufferRegion);
      reshade::register_event<reshade::addon_event::unmap_buffer_region>(OnUnmapBufferRegion);
      reshade::register_event<reshade::addon_event::destroy_resource>(OnDestroyResource);
      reshade::register_event<reshade::addon_event::bind_viewports>(OnBindViewports);
      reshade::register_event<reshade::addon_event::bind_scissor_rects>(OnBindScissorRects);
   }

   static void UnregisterEvents()
   {
      reshade::unregister_event<reshade::addon_event::create_resource>(OnCreateResource);
      reshade::unregister_event<reshade::addon_event::create_pipeline>(OnCreateBlendState);
      reshade::unregister_event<reshade::addon_event::resolve_texture_region>(OnResolveTextureRegion);
      reshade::unregister_event<reshade::addon_event::map_buffer_region>(OnMapBufferRegion);
      reshade::unregister_event<reshade::addon_event::unmap_buffer_region>(OnUnmapBufferRegion);
      reshade::unregister_event<reshade::addon_event::destroy_resource>(OnDestroyResource);
      reshade::unregister_event<reshade::addon_event::bind_viewports>(OnBindViewports);
      reshade::unregister_event<reshade::addon_event::bind_scissor_rects>(OnBindScissorRects);
   }

   void LoadConfigs() override
   {
      reshade::get_config_value(nullptr, NAME, "MSAAResolveEnable", g_luma_msaa_enable);
      reshade::get_config_value(nullptr, NAME, "SMAAEnable", g_smaa_enable);
      reshade::get_config_value(nullptr, NAME, "RCASSharpness", g_rcas_sharpness);
      reshade::get_config_value(nullptr, NAME, "BloomEnable", g_luma_bloom_enable);
      reshade::get_config_value(nullptr, NAME, "GTAOEnable", g_gtao_enable);
      reshade::get_config_value(nullptr, NAME, "RenderScale", g_render_scale);
      g_render_scale = std::clamp(g_render_scale, min_render_scale, 1.f);
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
            *value = (enabled ? 1.f : 0.f);
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

      const bool sr_active = IsSRActive(device_data);
      ImGui::BeginDisabled(!sr_active);
      // Applied on release: every render size recreates the DLSS/FSR feature, a hitch per 1% step while dragging
      static int held_render_scale = 0; // The slider's value while it's held, else 0
      int render_scale = (held_render_scale != 0 ? held_render_scale : int(std::round(g_render_scale * 100.f)));
      ImGui::SliderInt("Render Scale (%)", &render_scale, int(min_render_scale * 100.f), 100, "%d%%", ImGuiSliderFlags_AlwaysClamp);
      held_render_scale = (ImGui::IsItemActive() ? render_scale : 0);
      if (ImGui::IsItemDeactivatedAfterEdit())
      {
         g_render_scale = float(render_scale) / 100.f;
         reshade::set_config_value(nullptr, NAME, "RenderScale", g_render_scale);
      }
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
         ImGui::SetTooltip("The resolution the game renders at, upscaled by DLSS/FSR.");
      DrawResetButton(g_render_scale, 1.f, "RenderScale");
      ImGui::EndDisabled();

      // The upscaler (Super Resolution, in the Settings tab) replaces MSAA and SMAA: shown off, the saved choices are kept
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
         ImGui::TextColored(ImVec4(1.f, 0.6f, 0.f, 1.f), "DLSS/FSR inactive: turn Anti-Aliasing off in the game's display settings.");

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
      ImGui::SliderFloat("GTAO Thin Occluder Compensation", &g_gtao_thin_occluder_override, 0.f, 1.f, "%.2f");
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
         ImGui::SetTooltip("0 = the shader's THIN_OCCLUDER_COMPENSATION (0); > 0 overrides it (Intel: 0-0.7). Higher = samples behind the center\nstop occluding sooner: thin occluders (grass, poles) darken what is behind them less.");
#if DEVELOPMENT // the shader's debug blocks exist in DEVELOPMENT only
      ImGui::Combo("GTAO Debug View", &g_gtao_debug_view, "Off\0Depth gradient\0Normals\0AO x8\0Edges\0");
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
         ImGui::SetTooltip("Draws diagnostics through the game's SSAO apply (multiplied into the lighting). Depth gradient flat or blocky = wrong input;\nNormals: camera-facing surfaces bright, black everywhere = NORMAL_Z_SIGN inverted; AO x8 = spot broad over-occlusion.");
      ImGui::Combo("GTAO Noise Per Frame", &g_gtao_noise_per_frame, "Auto (DLSS/FSR)\0Off (frozen)\0On (frame % 64)\0");
      ImGui::Combo("GTAO Single Denoise", &g_gtao_single_denoise, "Auto (DLSS/FSR)\0Off (2 passes)\0On (1 pass)\0");
      ImGui::Combo("GTAO Full Resolution", &g_gtao_full_res, "Auto (DLSS/FSR)\0Off (half res)\0On\0");
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

   void CleanExtraSRResources(DeviceData& device_data) override
   {
      GetGameDeviceData(device_data).release_sr_resources = true;
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
      device_data.has_drawn_main_post_processing = false;
      game_device_data.msaa_scene = std::exchange(game_device_data.msaa_scene_resolved, false);
      game_device_data.sr_active = device_data.sr_type != SR::Type::None && !device_data.sr_suppressed && !game_device_data.msaa_scene;
      const bool mv_was_active = game_device_data.mv_active.exchange(IsSRActive(device_data) || g_mv_enable);
      if (mv_was_active && !game_device_data.mv_active)
      {
         // The copies stop following the Maps: a draw after the upscaler comes back must find none (zero motion) until its buffer is
         // mapped again, not a stale one
         {
            const std::lock_guard lock(game_device_data.mv_constants_mutex);
            for (auto& [buffer, copy] : game_device_data.mv_constants_copies)
               copy.reset();
            game_device_data.mv_mapped_constants.clear();
         }
         game_device_data.mv_objects.clear();
         game_device_data.mv_previous_objects.clear();
         game_device_data.mv_camera_valid = false;
         game_device_data.mv_previous_camera_valid = false;
      }
      // None picked: Core freed its upscaler resources and output, ours go too (recreated when an upscaler is picked again)
      if (device_data.sr_type == SR::Type::None && game_device_data.release_sr_resources.exchange(false) && !g_mv_enable)
      {
         {
            const std::unique_lock lock(game_device_data.mv_mutex);
            game_device_data.mv_texture.reset();
            game_device_data.mv_rtv.reset();
            game_device_data.mv_uav.reset();
         }
         game_device_data.mv_frame_depth.reset();
         game_device_data.mv_fill_buffer.reset();
         game_device_data.mv_jitter_buffer.reset();
         game_device_data.mv_previous_constants = {};
         game_device_data.sub_rect_depth_copy.reset();
         game_device_data.sub_rect_depth_copy_srv.reset();
         game_device_data.sub_rect_depth_dsv.reset();
         game_device_data.sub_rect_depth_write_state.reset();
         game_device_data.sub_rect_color_copy.reset();
         game_device_data.sub_rect_color_copy_srv.reset();
         game_device_data.sub_rect_scene_rtv.reset();
      }
      // SMAA's resources go once it stopped running (the upscaler antialiases, or it's off), the snapshot too once RCAS stopped as well
      if (cb_luma_global_settings.FrameIndex - game_device_data.smaa_frame > smaa_idle_release_frames)
      {
         game_device_data.smaa_predication_uav.reset();
         game_device_data.smaa_predication_srv.reset();
         ReleaseSMAA(device_data);
      }
      if (cb_luma_global_settings.FrameIndex - game_device_data.snapshot_frame > smaa_idle_release_frames)
      {
         game_device_data.smaa_gamma_texture.reset();
         game_device_data.smaa_gamma_srv.reset();
      }
      // The pooled constant copies only the pool holds (superseded, no object keeps them) are free for the next ones, as many as the
      // last frame asked for: a burst between two presents (loading screens) would keep its peak otherwise. Without motion vectors, none.
      {
         const std::lock_guard lock(game_device_data.mv_constants_mutex);
         size_t kept_free = std::exchange(game_device_data.mv_constants_made, 0);
         if (!game_device_data.mv_active)
         {
            kept_free = 0;
         }
         std::erase_if(game_device_data.mv_constants_pool, [&](const auto& copy)
            {
               if (copy.use_count() != 1)
                  return false;
               if (kept_free == 0)
                  return true;
               kept_free--;
               return false; });
         for (auto& [size, free_copies] : game_device_data.mv_constants_pool_free)
            free_copies.clear();
         for (uint32_t i = 0; i < uint32_t(game_device_data.mv_constants_pool.size()); i++)
         {
            if (game_device_data.mv_constants_pool[i].use_count() == 1)
            {
               game_device_data.mv_constants_pool_free[game_device_data.mv_constants_pool[i]->size()].push_back(i);
            }
         }
      }
      // A scene no post pass ended ends here: its jitter must not reach the next frame's draws before the G-buffer
      game_device_data.mv_scene_open = false;
      RestoreSceneViewport(native_device_context.get(), &game_device_data);
      game_device_data.mv_frame_ended = true;
      game_device_data.mv_scene_color_wanted = false;
      game_device_data.mv_fill_pending = false;
      if (!custom_texture_mip_lod_bias_offset)
      {
         const std::unique_lock lock(s_mutex_samplers);
         // -1 at native resolution, lower below it (the render scale)
         device_data.texture_mip_lod_bias_offset = (IsSRActive(device_data) ? SR::GetMipLODBias(std::round(device_data.output_resolution.y * GetSubRectScale(device_data)), device_data.output_resolution.y) : 0.f);
      }
      // Turning XeGTAO off gives its scratch back; it is rebuilt on demand.
      if (!g_gtao_enable && game_device_data.gtao_width != 0)
         game_device_data.ReleaseGTAOScratch();
#if DEVELOPMENT
      // "Performance Test" (see its tooltip): closes this frame's timestamp set, reads back the finished ones, opens the next frame's
      if (auto* const queries = std::exchange(game_device_data.perf_frame_queries, nullptr))
      {
         native_device_context->End(queries->frame_end.get());
         native_device_context->End(queries->disjoint.get());
         queries->pending = true;
      }
      if (g_perf_test != 0)
      {
         const auto now = std::chrono::steady_clock::now();
         const uint32_t settings = uint32_t(g_perf_test) | (uint32_t(int(device_data.sr_type) + 1) << 5) | (dlss_render_preset << 8) | (uint32_t(g_smaa_enable) << 16) | (uint32_t(g_gtao_enable) << 17) | (uint32_t(game_device_data.msaa_scene) << 18) | (uint32_t(g_perf_hook_timers) << 19) | (uint32_t(g_gtao_full_res) << 20) | (uint32_t(std::lround(g_render_scale * 100.f)) << 22);
         // Also after a pause (the game stops presenting while unfocused): the upscaler history and the clocks restart
         if (std::exchange(game_device_data.perf_settings, settings) != settings || now - game_device_data.perf_last_present > std::chrono::milliseconds(250))
            game_device_data.perf_settle_frames = 60;
         const bool measuring = game_device_data.perf_settle_frames <= 0;
         auto& stats = game_device_data.perf_stats;
         for (auto& queries : game_device_data.perf_queries)
         {
            D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjoint;
            if (!queries.pending || native_device_context->GetData(queries.disjoint.get(), &disjoint, sizeof(disjoint), D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK)
               continue;
            queries.pending = false;
            if (!measuring)
               continue;
            if (disjoint.Disjoint || disjoint.Frequency == 0)
            {
               stats.disjoint++;
               continue;
            }
            const auto read = [&](const com_ptr<ID3D11Query>& query, UINT64* ticks)
            { return native_device_context->GetData(query.get(), ticks, sizeof(*ticks), D3D11_ASYNC_GETDATA_DONOTFLUSH) == S_OK; };
            const auto add = [&](UINT64 start, UINT64 end, double* total, double* max_ms, uint32_t* samples)
            {
               const double ms = 1000.0 * double(end - start) / double(disjoint.Frequency);
               *total += ms;
               *max_ms = (std::max)(*max_ms, ms);
               ++*samples;
            };
            UINT64 frame_start = 0, frame_end = 0, scene_start = 0, scene_end = 0, sr_end = 0;
            if (!read(queries.frame_start, &frame_start) || !read(queries.frame_end, &frame_end))
               continue;
            add(frame_start, frame_end, &stats.frame_ms, &stats.frame_max_ms, &stats.samples);
            if (queries.scene && read(queries.scene_start, &scene_start) && read(queries.scene_end, &scene_end))
            {
               add(scene_start, scene_end, &stats.scene_ms, &stats.scene_max_ms, &stats.scene_samples);
               if (queries.sr && read(queries.sr_end, &sr_end))
                  add(scene_end, sr_end, &stats.sr_ms, &stats.sr_max_ms, &stats.sr_samples);
            }
         }
         if (!measuring)
         {
            game_device_data.perf_settle_frames--;
            stats = {};
            game_device_data.perf_hook_ns = 0;
         }
         else
         {
            stats.cpu_frame_ms += std::chrono::duration<double, std::milli>(now - game_device_data.perf_last_present).count();
            if (++stats.frames >= 120)
            {
               const auto average = [](double total, uint32_t samples)
               { return samples != 0 ? total / samples : 0.0; };
               std::string aa = (g_smaa_enable ? "SMAA" : "None");
               if (IsSRActive(device_data))
                  aa = (device_data.sr_type == SR::Type::FSR ? "FSR" : (dlss_render_preset != 0 ? std::format("DLSS_{}", char('A' + dlss_render_preset - 1)) : "DLSS_Default"));
               const std::array<double, 4> window = {average(stats.frame_ms, stats.samples), average(stats.scene_ms, stats.scene_samples), average(stats.sr_ms, stats.sr_samples), double(game_device_data.perf_hook_ns.exchange(0)) / 1e6 / stats.frames};
               reshade::log::message(reshade::log::level::info, std::format("[SR4 Perf] mode=\"{}\" aa={} hook_timers={} msaa={} gtao={} gtao_full_res={} render_scale={:.2f} rcas={:.2f} output={}x{} gpu frame avg/max={:.3f}/{:.3f} ms scene avg/max={:.3f}/{:.3f} ms ({}) sr avg/max={:.3f}/{:.3f} ms ({}) cpu frame avg={:.3f} ms cpu hooks={:.3f} ms/frame samples={}/{} disjoint={}", perf_test_modes[g_perf_test].name, aa, g_perf_hook_timers, game_device_data.msaa_scene, g_gtao_enable, g_gtao_full_res, GetSubRectScale(device_data), g_rcas_sharpness, uint32_t(device_data.output_resolution.x), uint32_t(device_data.output_resolution.y), window[0], stats.frame_max_ms, window[1], stats.scene_max_ms, stats.scene_samples, window[2], stats.sr_max_ms, stats.sr_samples, stats.cpu_frame_ms / stats.frames, window[3], stats.samples, stats.frames, stats.disjoint).c_str());
               stats = {};

               if (g_perf_sweep)
               {
                  game_device_data.perf_sweep_results[g_perf_test].push_back(window);
                  if (++game_device_data.perf_sweep_windows_done >= perf_sweep_windows)
                  {
                     game_device_data.perf_sweep_windows_done = 0;
                     const int step = ++game_device_data.perf_sweep_step;
                     if (step < perf_sweep_rounds * int(std::size(perf_sweep_modes)))
                     {
                        ApplyPerfTestMode(device_data, perf_sweep_modes[step % std::size(perf_sweep_modes)]);
                     }
                     else
                     {
                        // Per mode: the median window (and the frame's range), and the frame against the last mode's (No AA)
                        const auto median = [&](int mode, size_t column)
                        {
                           std::vector<double> values;
                           for (const auto& result : game_device_data.perf_sweep_results[mode])
                              values.push_back(result[column]);
                           std::nth_element(values.begin(), values.begin() + values.size() / 2, values.end());
                           return values.empty() ? 0.0 : values[values.size() / 2];
                        };
                        const double baseline = median(perf_sweep_modes[std::size(perf_sweep_modes) - 1], 0);
                        for (const int mode : perf_sweep_modes)
                        {
                           const auto& results = game_device_data.perf_sweep_results[mode];
                           const auto [min_frame, max_frame] = std::minmax_element(results.begin(), results.end(), [](const auto& a, const auto& b)
                              { return a[0] < b[0]; });
                           reshade::log::message(reshade::log::level::info, std::format("[SR4 Perf] sweep mode=\"{}\" hook_timers={} windows={} gpu frame median={:.3f} ms (min/max {:.3f}/{:.3f}, {:+.3f} vs \"{}\") scene median={:.3f} ms sr median={:.3f} ms cpu hooks median={:.3f} ms/frame", perf_test_modes[mode].name, g_perf_hook_timers, results.size(), median(mode, 0), results.empty() ? 0.0 : (*min_frame)[0], results.empty() ? 0.0 : (*max_frame)[0], median(mode, 0) - baseline, perf_test_modes[perf_sweep_modes[std::size(perf_sweep_modes) - 1]].name, median(mode, 1), median(mode, 2), median(mode, 3)).c_str());
                        }
                        g_perf_sweep = false;
                        ApplyPerfTestMode(device_data, 0);
                     }
                  }
               }
            }
         }
         game_device_data.perf_last_present = now;

         auto& queries = game_device_data.perf_queries[game_device_data.perf_query_index];
         if (!queries.pending)
         {
            if (!queries.disjoint)
            {
               const D3D11_QUERY_DESC disjoint_desc = {D3D11_QUERY_TIMESTAMP_DISJOINT}, timestamp_desc = {D3D11_QUERY_TIMESTAMP};
               native_device->CreateQuery(&disjoint_desc, &queries.disjoint);
               for (auto* const query : {&queries.frame_start, &queries.scene_start, &queries.scene_end, &queries.sr_end, &queries.frame_end})
                  native_device->CreateQuery(&timestamp_desc, &*query);
            }
            if (queries.disjoint && queries.frame_start && queries.scene_start && queries.scene_end && queries.sr_end && queries.frame_end)
            {
               native_device_context->Begin(queries.disjoint.get());
               native_device_context->End(queries.frame_start.get());
               queries.scene_started = queries.scene = queries.sr = false;
               game_device_data.perf_frame_queries = &queries;
               game_device_data.perf_query_index = (game_device_data.perf_query_index + 1) % game_device_data.perf_queries.size();
            }
         }
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

      g_msaa_resolves_last_frame = std::exchange(g_msaa_resolves_this_frame, 0);
      g_finals_last_frame = std::exchange(g_finals_this_frame, 0);
      g_pixel_steps_overrides_last_frame = std::exchange(g_pixel_steps_overrides_this_frame, 0);
      g_sub_rect_draws_last_frame = std::exchange(g_sub_rect_draws_this_frame, 0);
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

      ImGui::SeparatorText("Motion vector research");
      ImGui::Checkbox("MV Enable", &g_mv_enable);
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Redraws the opaque main material pass with the motion vector shaders without DLSS/FSR: camera and object motion\n(each draw finds its own previous frame vc2/vc3). The image must not change; the debug view is black with a static camera\nand lights up only what moves. ReShade.log: patched/refused shaders. Not saved.");
      ImGui::Checkbox("MV Force Jitter", &g_mv_force_jitter);
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Jitters the scene (Halton 2/3, 8 phases) without an upscaler, with MV Enable. The image shakes by a subpixel; nothing\nmay flicker or lose pixels, and the debug view stays black with a static camera. Not saved.");
      ImGui::Checkbox("MV Disable Jitter", &g_mv_disable_jitter);
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("No projection jitter under DLSS/FSR (the upscaler gets zero jitter): isolates artifacts that come from the jitter. Not saved.");
      ImGui::Checkbox("MV Debug View", &g_mv_debug_view);
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Shows the motion vector target (absolute, in pixels) through Core's debug draw.");
      ImGui::Text("IR_Pixel_Steps overrides last frame: %u, sub-rect draws: %u", g_pixel_steps_overrides_last_frame, g_sub_rect_draws_last_frame);
      ImGui::Checkbox("MV Buffer Filter", &g_mv_buffer_filter);
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("The Map hooks skip unregistered buffers without the lock and the lookups (A/B of the CPU saving). Not saved.");
      ImGui::Checkbox("MV Constants Pool", &g_mv_constants_pool);
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("The vc2/vc3 copies are rewritten in place or reused from a pool instead of allocated per Unmap (A/B of the CPU saving). Not saved.");
      ImGui::Checkbox("MV Read Sizes", &g_mv_read_sizes);
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("The previous frame's vc2/vc3 upload only the bytes the vertex shader reads (A/B of the CPU saving). Not saved.");
      ImGui::Checkbox("MV Half Float", &g_mv_half_float);
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Motion vector target R16G16_FLOAT instead of R32G32_FLOAT (A/B of the GPU saving; the target is recreated). Not saved.");
      {
         const std::lock_guard lock(game_device_data.mv_constants_mutex);
         ImGui::Text("Filtered buffers: %u, pooled constant copies: %zu", game_device_data.mv_filtered_buffer_count.load(std::memory_order_relaxed), game_device_data.mv_constants_pool.size());
      }

      ImGui::SeparatorText("Performance");
      const std::string sweep_label = std::format("Sweep ({}/{})", game_device_data.perf_sweep_step + 1, perf_sweep_rounds * std::size(perf_sweep_modes));
      if (ImGui::BeginCombo("Performance Test", g_perf_sweep ? sweep_label.c_str() : perf_test_modes[g_perf_test].name))
      {
         for (int i = 0; i < int(std::size(perf_test_modes)); i++)
         {
            const PerfTestMode& mode = perf_test_modes[i];
            ImGui::BeginDisabled(!IsPerfTestModeAvailable(device_data, mode));
            if (ImGui::Selectable(mode.name, !g_perf_sweep && g_perf_test == i) && (g_perf_sweep || g_perf_test != i))
            {
               g_perf_sweep = false;
               ApplyPerfTestMode(device_data, i);
            }
            ImGui::EndDisabled();
         }
         ImGui::BeginDisabled(!device_data.sr_implementations_instances.contains(SR::Type::DLSS));
         if (ImGui::Selectable("Sweep", g_perf_sweep) && !g_perf_sweep)
         {
            for (auto& results : game_device_data.perf_sweep_results)
               results.clear();
            game_device_data.perf_sweep_step = 0;
            game_device_data.perf_sweep_windows_done = 0;
            g_perf_sweep = true;
            ApplyPerfTestMode(device_data, perf_sweep_modes[0]);
         }
         ImGui::EndDisabled();
         ImGui::EndCombo();
      }
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Logs \"[SR4 Perf]\" to ReShade.log every 120 frames (the first 60 after a change of mode or AA settings, or a pause, skipped): the GPU\ntime of the whole frame (present to present), of the scene (G-buffer to its first post pass: motion vector, jitter and camera\nfill included) and of DLSS/FSR (timestamps), the CPU frame time and the CPU time in the motion vector hooks.\nThe modes set the anti-aliasing, DLSS preset, render scale and GTAO resolution themselves (the user's come back on \"Off\" or\n\"Current Settings\", nothing is saved). \"Jitter Only\" draws the material pass jittered but without motion vectors, \"Without\nMotion Vector Draws\" leaves every draw untouched (unjittered): the differences to \"DLSS K 100%\" are their costs. Keep the camera still and the game focused, 10 s per mode.\n\"Sweep\" runs the DLSS K render scale, GTAO and motion vector modes, then No AA, in turn, %d windows each, %d rounds, then logs\n\"[SR4 Perf] sweep\" lines: each mode's median window and its frame time against No AA; picking another mode stops it.", perf_sweep_windows, perf_sweep_rounds);
      ImGui::Checkbox("Hook Timers", &g_perf_hook_timers);
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Times the motion vector hooks for \"cpu hooks\" (two clock reads per hooked draw, ~9000 a frame in a dense view).\nRun a Sweep with it off to see their own cost in the frame times.");
   }
#endif

   void PrintImGuiAbout() override
   {
      ImGui::PushTextWrapPos(0.f);
      ImGui::Text(
         "Luma for \"Saints Row IV: Re-Elected\" is developed by DristoforColumb and is open source and free.\n"
         "It adds HDR, DLSS or FSR 3 upscaling, HDR bloom and SMAA anti-aliasing, and replaces the game's SSAO with XeGTAO.\n"
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
                  "\nAMD FidelityFX (RCAS + FSR 3)"
                  "\nNVIDIA NGX (DLSS)");
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
