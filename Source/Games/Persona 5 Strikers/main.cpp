#define GAME_PERSONA_5_STRIKERS 1

// Core only checks whether this is defined (any value skips the "attach the debugger" popup)
#define DISABLE_AUTO_DEBUGGER 1

// Movies play through a separate DX9 device (Media Foundation), same as Nioh
#define CHECK_GRAPHICS_API_COMPATIBILITY 1
// SMAA runs right after the composite, through "original_draw_dispatch_func"
#define ENABLE_POST_DRAW_DISPATCH_CALLBACK 1
#define ENABLE_SMAA 1
// The UI shaders get a saturate appended in place (see "PatchShaderBytecodeSync")
#define LUMA_PATCH_BYTECODE_SYNC 1
// The motion vector draw key reads the draw arguments ("last_draw_dispatch_data")
#define ENABLE_DRAW_DISPATCH_DATA_CACHE 1

#include "..\..\Core\core.hpp"
#include "..\..\External\WDK\includes\d3d11TokenizedProgramFormat.hpp"
#include "MotionVectorPatches.h"

namespace
{
   // Katana engine PostEffect3 composite: exposure, lens effects, vignette, then the baked HDR 3D LUT (tonemap + grade).
   // Every captured scene (menus, dialogue, hub, field) runs it once, writing straight into the swapchain.
   // The main menu runs it a second time into an off-screen RGBA8 target.
   constexpr uint32_t composite_hash = 0x45A96F2D;
   // FXAA 3: the game copies the swapchain (after the UI) and FXAAs the copy back into it
   constexpr uint32_t fxaa_hash = 0xED2D9823;
   // PostEffect3 ApplyFxaa{,Repair,Console,Quality}PS: the engine's own FXAA, which also carries the radial blur. When one of them runs, the
   // composite goes to an intermediate target and this pass draws it into the swapchain. Never seen in a capture yet.
   const ShaderHashesList shader_hashes_apply_fxaa = {.pixel_shaders = {0x0B6569A5, 0xC8A7BA1C, 0x95F3321A, 0xED7941FD}};
   // The SSAO depth downsample, whose t0 is the full res D32_FLOAT_S8X24 depth (a copy the game makes right before post): the SMAA predication input
   constexpr uint32_t ssao_depth_downsample_hash = 0x6E15840A;
   // The native SSAO calculate (half res R8 visibility), replaced by XeGTAO. Its two depth aware blurs, which upsample to full
   // res, and the merge into the G-buffer AO (gbuf0.a = min(material AO, SSAO), read by the deferred lighting) stay vanilla.
   constexpr uint32_t ssao_hash = 0x63435B03;
   // The UI pixel shaders, drawing into the swapchain (vanilla BGRA8 UNORM), which clamped their output before blending. Their texture
   // times vertex color, blend mode and saturation control (grey + k * (color - grey), grey a 0.299/0.587/0.114 weighted RGB sum, k a
   // cb0 scalar) go outside 0-1, which the fp16 swapchain no longer clamps. The whole family ends in that saturation tail; found by
   // disassembling all the dumped pixel shaders.
   // Each has a single o0.xyzw write and a single final ret.
   const std::unordered_set<uint32_t> ui_pixel_shaders = {0x90C6B12E, 0xEE9FC290, 0x07378D54, 0x4A0CB253, 0x8BA60D22, 0xD1BEFD65, 0xF0863953, 0x76C3BC5E, 0xA4DFC750, 0xBEF2C79E};
   // The first post process pass of a scene frame, each reading the scene at t0: DOF (E0DB2D7E), else the bloom prefilter, else the
   // composite. It ends the scene frame (the next G-buffer draw starts a new one), and DLSS runs right before it. The lighting before it
   // varies by scene (691D080F, or 4376F855 twice).
   const ShaderHashesList post_process_start_shader_hashes = {.pixel_shaders = {0xE0DB2D7E, 0xD65ABD25, 0x3D7CAD40, 0xB6289AC0, composite_hash}};
   // The post passes that blend into the scene in place, all sampling their inputs by UV: the DOF merges (5 sample, 9 sample,
   // reduction) and the bloom adds (5 and 9 sample, and their sub-rect clamped "ForViewport" twins)
   const ShaderHashesList scene_post_writer_shader_hashes = {.pixel_shaders = {0x409590F7, 0x42D664E0, 0xAA4F82B2, 0x619045C8, 0xB5F3F656, 0x88C4EC12, 0x4CE014A2}};
   // A generic copy: text glyphs into their atlas, and at render scales below 1, the composite's output stretched onto the swapchain
   constexpr uint32_t copy_hash = 0x987DC89C;
   // The 3D layers outside the scene (main menu and pause screen characters): the quad vertex shader, which takes its texture
   // coordinates from its vertices, and the stretch of a layer's render resolution corner over its whole target
   constexpr uint32_t quad_vertex_shader_hash = 0x2B6CA9A0;
   constexpr uint32_t layer_stretch_hash = 0x99A76DC2;
   constexpr UINT layer_uv_scale_cb_slot = 5; // "register(b5)" in Luma_P5S_LayerQuad.hlsl; the quads only read b0

   bool g_hide_ui = false; // Session only, so a restart never comes back without a HUD

   // The game's render scale (its "RenderScale" setting, 5 = 50% ... 10 = 100%), overriding the game's own option; 0 leaves it to the game
   int g_render_scale = 0;
   bool g_render_scale_custom = false; // A 10% step slider instead of the presets

   bool g_gtao_enable = true;
   constexpr UINT gtao_knobs_cb_slot = 9; // "register(b9)" in Luma_P5S_XeGTAO.hlsl; b11 is core DrawBloom's
   float g_gtao_final_value_power = 1.f;  // DEV/TEST calibration knobs, not persisted
   float g_gtao_radius_override = 0.f;    // > 0 overrides the native radius (centimetres)
#if DEVELOPMENT
   int g_gtao_debug_view = 0; // 0=off 1=depth gradient 2=normals 3=AO x8 4=edges
#endif

#if DEVELOPMENT
   bool g_smaa_predication = true;
   int g_smaa_debug_view = 0; // 0 off, 1 edges, 2 predication
   // Motion vector research (the game renders none): how the G-buffer draws get their VS $Globals (cb0: view projection, world matrix
   // or bone palette). Consumed at the next present.
   std::atomic<bool> g_mv_probe = false;

   // Motion vectors (see "DrawWithMotionVectors") without an upscaler, to check them
   bool g_mv_enable = false;
   bool g_mv_debug_view = false;
   bool g_mv_force_jitter = false;                    // The upscaler's jitter without an upscaler, to check it: the image shakes, the motion vectors don't
   constexpr uint32_t mv_probe_max_draw_lines = 8000; // Field gameplay has ~3000 draws
#else
   constexpr bool g_mv_enable = false;
   constexpr bool g_mv_force_jitter = false;
#endif

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

} // namespace

// Everything that holds a device object, so it is released with its device.
struct Persona5StrikersGameDeviceData final : public GameDeviceData
{
   // SMAA scratch, recreated when the canvas size changes: a linear copy of the canvas (SMAA writes the canvas, so it cannot
   // also sample it), its gamma encode (the edge detection input; with RCAS also SMAA's output, read by the finalize pass) and
   // the predication edge-ness.
   com_ptr<ID3D11Texture2D> smaa_linear_texture;
   com_ptr<ID3D11ShaderResourceView> smaa_linear_srv;
   com_ptr<ID3D11ShaderResourceView> smaa_gamma_srv;
   com_ptr<ID3D11RenderTargetView> smaa_gamma_rtv;
   com_ptr<ID3D11UnorderedAccessView> smaa_gamma_uav;
   com_ptr<ID3D11ShaderResourceView> smaa_predication_srv;
   com_ptr<ID3D11UnorderedAccessView> smaa_predication_uav;

   // The scene depth for predication, from the SSAO depth downsample. The texture is persistent and written by the game before post
   // every frame, so the view is kept (the SSAO and post passes record on different deferred contexts, so per frame resets would race).
   std::mutex smaa_depth_mutex;
   com_ptr<ID3D11ShaderResourceView> smaa_depth_srv;

   // Set when SMAA ran after this frame's composite, or DLSS/FSR before its post process, so the vanilla FXAA is skipped
   std::atomic<bool> scene_antialiased = false;
   // Set when a 3D layer outside the scene was drawn (main menu, pause screen): the UI composes it after the scene's SMAA, so the
   // vanilla FXAA still runs
   std::atomic<bool> layer_drawn = false;

   // MIN and MAX blends (all channels, alpha only), to clamp the swapchain to 0-1 under a UI draw's own geometry (see "OnDrawOrDispatch")
   com_ptr<ID3D11BlendState> ui_min_blend_states[2];
   com_ptr<ID3D11BlendState> ui_max_blend_states[2];

   // XeGTAO scratch, recreated when the SSAO target size changes. The SSAO records on worker threads (deferred contexts), so
   // everything below is guarded by the mutex.
   std::mutex gtao_mutex;
   com_ptr<ID3D11Texture2D> gtao_depth_mips_texture; // R32F view space depth pyramid, 5 mips
   com_ptr<ID3D11UnorderedAccessView> gtao_depth_mip_uavs[5];
   com_ptr<ID3D11ShaderResourceView> gtao_depth_mips_srv;
   com_ptr<ID3D11UnorderedAccessView> gtao_working_uavs[2]; // R8G8_UNORM AO + edges ping-pong
   com_ptr<ID3D11ShaderResourceView> gtao_working_srvs[2];
   com_ptr<ID3D11Texture2D> gtao_final_texture; // R8_UNORM copy source for the game's target
   com_ptr<ID3D11UnorderedAccessView> gtao_final_uav;
   uint32_t gtao_width = 0;
   uint32_t gtao_height = 0;
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
      gtao_width = 0;
      gtao_height = 0;
   }

#if DEVELOPMENT
   // "Log MV Probe": every constant buffer mapped or updated in the probed frame, and the G-buffer draws that bind it as VS cb0
   struct MVProbeBuffer
   {
      uint32_t maps[5] = {}; // By reshade::api::map_access (1 read_only ... 4 write_discard)
      uint32_t device_updates = 0;
      uint32_t command_updates = 0;
      uint64_t update_bytes = 0;
      uint32_t gbuffer_draws = 0;
      uint32_t other_draws = 0;
      std::unordered_set<uint32_t> first_constants; // Distinct VSSetConstantBuffers1 offsets, a ring buffer if many
      D3D11_BUFFER_DESC desc = {};
   };
#endif

   // Motion vectors: the patched shaders by original hash (null when the patch failed), patched on first use, and the target
   std::shared_mutex mv_mutex;
   std::unordered_map<uint32_t, com_ptr<ID3D11VertexShader>> mv_vertex_shaders;
   std::unordered_map<uint32_t, com_ptr<ID3D11PixelShader>> mv_pixel_shaders;
   com_ptr<ID3D11Texture2D> mv_texture;
   com_ptr<ID3D11RenderTargetView> mv_rtv;
   // The game's G-buffer blend states blend each target independently: copies that also write the MV target, by original (and its
   // description, in case the address gets reused)
   std::unordered_map<ID3D11BlendState*, std::pair<D3D11_BLEND_DESC, com_ptr<ID3D11BlendState>>> mv_blend_states;
   // Byte offsets in each patched vertex shader's $Globals, by original hash: "mW2P" (the camera's view projection) and "mL2W" (the
   // world matrix, or the bone palette), 3 rows with the translation in .w
   struct GlobalsLayout
   {
      UINT view_projection = UINT_MAX;
      UINT world = UINT_MAX;
   };
   std::unordered_map<uint32_t, GlobalsLayout> mv_globals_layouts;
   // The scene records on a single deferred context: after its first post process pass, the next G-buffer draw starts a frame (clears the target)
   bool mv_frame_ended = true;
   com_ptr<ID3D11Resource> mv_scene_depth;          // The G-buffer's depth, which the forward redraws share
   ID3D11DeviceContext* mv_scene_context = nullptr; // Only compared

   // CPU copies of the $Globals buffers the patched draws bind (the game maps them with discard before about every draw, and some draws
   // reuse the last contents): the pointer from each Map, copied at its Unmap
   std::mutex mv_globals_mutex;
   std::unordered_set<uint64_t> mv_globals_buffers;
   std::unordered_map<uint64_t, void*> mv_mapped_globals;
   std::unordered_map<uint64_t, std::vector<uint8_t>> mv_globals_copies;
   // Scene context only: the previous frame's $Globals uploads, one dynamic buffer per size, and the camera's view projection
   std::unordered_map<UINT, com_ptr<ID3D11Buffer>> mv_previous_globals_buffers;
   // Scene context only: two copies of each resource the patched vertex shaders read (wind and interaction buffers, ocean maps), taken
   // at its first use in a frame, by resource (held, so its address can't be reused, until a frame goes by without it). The one from
   // the previous frame goes to the vertex shader's second run.
   struct PreviousResource
   {
      com_ptr<ID3D11Resource> resource;
      com_ptr<ID3D11Resource> copies[2]; // This frame's, the previous frame's
      com_ptr<ID3D11ShaderResourceView> previous_view;
      D3D11_SHADER_RESOURCE_VIEW_DESC previous_view_desc = {};
      uint32_t frame = 0;
      bool has_previous = false;
   };
   std::unordered_map<ID3D11Resource*, PreviousResource> mv_previous_resources;
   // Every patched draw of a frame, by draw key (shaders, buffers, arguments), with its translation and $Globals. A draw takes the
   // previous frame's $Globals of the draw with its key nearest to it (same object, a frame earlier).
   struct MotionVectorObject
   {
      std::array<float, 3> translation;
      std::vector<uint8_t> globals;
   };
   std::unordered_map<uint64_t, std::vector<MotionVectorObject>> mv_objects;
   std::unordered_map<uint64_t, std::vector<MotionVectorObject>> mv_previous_objects;
   // This frame's projection jitter (pixels, +y down) and its VS cbuffer ("MotionVectorPatches::jitter_slot": NDC offset)
   std::array<float, 2> mv_jitter = {};
   com_ptr<ID3D11Buffer> mv_jitter_buffer;
   // The camera motion fill of the pixels no patched draw wrote (see "Luma_P5S_MotionVectorFill.hlsl"), which clears the target to FLT_MAX:
   // the target's UAV (if the GPU loads R32G32_FLOAT from UAVs), its constants, and the frame's depth (also the upscaler's)
   com_ptr<ID3D11UnorderedAccessView> mv_uav;
   com_ptr<ID3D11Buffer> mv_fill_buffer;
   bool mv_fill_pending = false;
   com_ptr<ID3D11ShaderResourceView> mv_scene_depth_srv;
   com_ptr<ID3D11Resource> mv_frame_depth;
   std::array<uint8_t, 64> mv_view_projection = {};
   std::array<uint8_t, 64> mv_previous_view_projection = {};
   bool mv_view_projection_valid = false;
   bool mv_previous_view_projection_valid = false;
   uint32_t mv_frame_present = 0;
   std::atomic<uint32_t> mv_presents = 0;

   std::atomic<uint32_t> mv_patched_draws = 0;
   std::atomic<uint32_t> mv_skipped_draws = 0;
   std::atomic<uint32_t> mv_uncopied_draws = 0;     // No CPU copy of its $Globals yet
   std::atomic<uint32_t> mv_other_camera_draws = 0; // A view projection other than the frame's camera, left as is
   std::atomic<uint32_t> mv_matched_draws = 0;      // Found last frame's own $Globals
   std::atomic<uint32_t> mv_ambiguous_draws = 0;    // Matched among several candidates
   std::atomic<uint32_t> mv_forward_draws = 0;      // Of the patched draws, the forward redraws (outlines, sky)
   std::atomic<uint32_t> mv_jitter_only_draws = 0;  // Scene draws with the jitter but no motion vectors (depth prepass, depth tested geometry)
   std::atomic<uint32_t> mv_resource_copies = 0;    // Of the resources the patched vertex shaders read, for their previous frame
   std::atomic<uint32_t> mv_fills = 0;

   // DLSS/FSR run on the immediate context, but the scene records on a deferred one: its command list is split right before the post
   // process, and the first part is executed before the upscaler when the game executes the rest (see "OnExecuteSecondaryCommandList").
   struct SRSplit
   {
      com_ptr<ID3D11CommandList> partial;
      com_ptr<ID3D11CommandList> remainder;  // Held, so its address (the pending split's key) can't be reused by another command list
      com_ptr<ID3D11Texture2D> source_color; // Also the output at native resolution (DLAA): the post process reads it
      com_ptr<ID3D11Texture2D> output_color; // Upscaling (the game's render scale below 1) only: the output, which the post process then reads
      com_ptr<ID3D11Resource> depth;
      std::array<float, 2> jitter;
      float vertical_fov = 0.7330383f; // Radians (FSR needs it), 42 degrees as measured in dialogue until the frame's camera is known
   };
   std::mutex sr_mutex;
   bool sr_split_ready = false;                                // Scene context only: this frame's G-buffer drew and the command list isn't split yet
   uint64_t sr_split_context = 0;                              // The context split last, until the game finishes its command list
   SRSplit sr_split;                                           // Split last, until the game finishes its command list
   std::unordered_map<uint64_t, SRSplit> sr_pending_splits;    // By the game's command list, the remainder, until it's executed
   std::vector<com_ptr<ID3D11Buffer>> sr_no_overwrite_buffers; // The dynamic buffers the game appends to with "D3D11_MAP_WRITE_NO_OVERWRITE"
   // Upscaling: the game renders the scene and its post process at its render scale into their own textures, the composite included,
   // then stretches that onto the swapchain. The upscaler writes the output resolution instead, the post passes that blend into the
   // scene also blend into it, and the composite draws from it at the output resolution into the canvas, which the stretch copies 1:1.
   ID3D11DeviceContext* sr_upscaling_context = nullptr; // This frame's scene context, once split for upscaling (only compared)
   com_ptr<ID3D11Texture2D> sr_upscaled_output;
   com_ptr<ID3D11RenderTargetView> sr_upscaled_output_rtv;
   com_ptr<ID3D11ShaderResourceView> sr_upscaled_output_srv;
   com_ptr<ID3D11Texture2D> sr_upscaled_canvas;
   com_ptr<ID3D11RenderTargetView> sr_upscaled_canvas_rtv;
   com_ptr<ID3D11ShaderResourceView> sr_upscaled_canvas_srv;
   com_ptr<ID3D11Resource> composite_target; // This frame's composite target when it isn't the swapchain (render scales below 1)
   // The game's render scale setting in its settings block (see "FindRenderScaleSetting"), null if not found. Present thread only.
   int32_t* render_scale_setting = nullptr;
   bool render_scale_searched = false;
   int32_t render_scale_game = 0;         // The game's own value (its option), restored without an override
   int32_t render_scale_applied = 0;      // The value the game last rebuilt its targets at, as far as known
   int32_t render_scale_memory = 0;       // The value Luma last left in the setting
   int render_scale_restore_presents = 0; // Until a temporary value (the main menu's 100%) is replaced by the kept one
   // The main menu: presents without a scene frame whose composite draws into a render resolution target (its background). The
   // pause screen has neither, so it never counts. Reset by any scene frame.
   std::atomic<bool> scene_drawn = false;
   std::atomic<bool> render_resolution_composite = false;
   uint32_t menu_presents = 0;
   // Upscaling: the 3D layers outside the scene draw at the output resolution (see "DrawLayerAtOutputResolution")
   struct LayerFrame
   {
      std::vector<ID3D11Resource*> targets; // Only compared
      float scale[2] = {1.f, 1.f};          // Render resolution / output resolution
   };
   std::mutex layer_mutex;
   std::unordered_map<ID3D11DeviceContext*, LayerFrame> layer_frames;     // By context, until its stretch
   std::unordered_map<uint32_t, UINT> layer_cluster_scale_offsets;        // "fClstScl" in each pixel shader's $Globals, by original hash (UINT_MAX if none)
   std::unordered_map<UINT, com_ptr<ID3D11Buffer>> layer_globals_buffers; // The patched $Globals uploads, one dynamic buffer per size
   com_ptr<ID3D11Buffer> layer_uv_scale_buffer;
   float layer_uv_scale[2] = {};
   std::atomic<uint32_t> layer_draws = 0;
   std::atomic<uint32_t> layer_uncopied_draws = 0; // No CPU copy of their $Globals yet: light clusters at the render resolution's scale
   std::atomic<uint32_t> sr_render_height = 1;
   std::atomic<uint32_t> sr_output_height = 1;
   std::atomic<uint32_t> sr_splits = 0;
   std::atomic<uint32_t> sr_draws = 0;
   std::atomic<uint32_t> sr_no_overwrite_maps = 0;

#if DEVELOPMENT
   std::atomic<bool> mv_probe_logging = false; // The probed frame
   std::mutex mv_probe_mutex;
   std::unordered_map<uint64_t, MVProbeBuffer> mv_probe_buffers;
   std::unordered_map<ID3D11DeviceContext*, uint32_t> mv_probe_context_gbuffer_draws;
   uint32_t mv_probe_draws = 0;
   uint32_t mv_probe_gbuffer_draws = 0;
   // Every buffer event received, constant buffer or not: 0 means this ReShade build doesn't send them (maps need RESHADE_ADDON >= 2)
   uint32_t mv_probe_map_events = 0;
   uint32_t mv_probe_update_events = 0;

#endif
};

class Persona5Strikers final : public Game
{
   static Persona5StrikersGameDeviceData& GetGameDeviceData(DeviceData& device_data)
   {
      return *static_cast<Persona5StrikersGameDeviceData*>(device_data.game);
   }

   static bool IsBackBuffer(DeviceData* device_data, ID3D11Resource* resource)
   {
      const std::shared_lock lock(device_data->mutex);
      return device_data->back_buffers.contains(reinterpret_cast<uint64_t>(resource));
   }

   static bool IsSRActive(const DeviceData& device_data)
   {
      return device_data.sr_type != SR::Type::None && !device_data.sr_suppressed;
   }

   // The scene draws with motion vectors (and jitter) for the upscaler
   static bool AreMotionVectorsEnabled(const DeviceData& device_data)
   {
      return g_mv_enable || IsSRActive(device_data);
   }

#if DEVELOPMENT
   // The probed frame's entry for a constant buffer, null for other resources. The caller holds the mutex from "GetMVProbeMutex".
   static Persona5StrikersGameDeviceData::MVProbeBuffer* GetMVProbeBuffer(reshade::api::device* device, reshade::api::resource resource, bool map)
   {
      auto* const game_device_data = static_cast<Persona5StrikersGameDeviceData*>(device->get_private_data<DeviceData>()->game);
      (map ? game_device_data->mv_probe_map_events : game_device_data->mv_probe_update_events)++;
      if ((device->get_resource_desc(resource).usage & reshade::api::resource_usage::constant_buffer) == 0)
         return nullptr;
      return &game_device_data->mv_probe_buffers[resource.handle];
   }

   // Null outside the probed frame, so the buffer events cost nothing then
   static std::mutex* GetMVProbeMutex(reshade::api::device* device)
   {
      auto* const device_data = device->get_private_data<DeviceData>();
      auto* const game_device_data = device_data ? static_cast<Persona5StrikersGameDeviceData*>(device_data->game) : nullptr;
      return game_device_data && game_device_data->mv_probe_logging ? &game_device_data->mv_probe_mutex : nullptr;
   }

#endif

   static DeviceData* GetDeviceData(reshade::api::device* device)
   {
      auto* const device_data = device->get_private_data<DeviceData>();
      return device_data && device_data->game ? device_data : nullptr;
   }

   // Motion vectors: remembers where the game writes a $Globals buffer a patched draw binds, to copy it at its Unmap
   static void OnMapBufferRegion(reshade::api::device* device, reshade::api::resource resource, uint64_t offset, uint64_t size, reshade::api::map_access access, void** data)
   {
      DeviceData* const device_data = GetDeviceData(device);
      if (!device_data || !AreMotionVectorsEnabled(*device_data))
         return;
      auto& game_device_data = GetGameDeviceData(*device_data);
      if (access == reshade::api::map_access::write_discard && data && *data)
      {
         const std::lock_guard lock(game_device_data.mv_globals_mutex);
         if (game_device_data.mv_globals_buffers.contains(resource.handle))
            game_device_data.mv_mapped_globals[resource.handle] = *data;
      }
      // A deferred context's first map of a dynamic buffer in a command list must discard, so the split discards these (see "sr_no_overwrite_buffers")
      else if (access == reshade::api::map_access::write_only)
      {
         game_device_data.sr_no_overwrite_maps++;
         auto* const buffer = reinterpret_cast<ID3D11Buffer*>(resource.handle);
         // "D3D11_MAP_WRITE" (staging) is reported as write only too, it can't be discarded
         D3D11_BUFFER_DESC desc;
         buffer->GetDesc(&desc);
         const std::lock_guard lock(game_device_data.sr_mutex);
         if (desc.Usage == D3D11_USAGE_DYNAMIC && std::ranges::find(game_device_data.sr_no_overwrite_buffers, buffer, [](const auto& known)
                                                     { return known.get(); }) == game_device_data.sr_no_overwrite_buffers.end())
         {
            // ponytail: cap, the game has a few (dialogue text vertices and indices); a released buffer stays referenced until the cap clears it
            if (game_device_data.sr_no_overwrite_buffers.size() >= 16)
               game_device_data.sr_no_overwrite_buffers.clear();
            game_device_data.sr_no_overwrite_buffers.emplace_back(buffer);
         }
      }

#if DEVELOPMENT
      std::mutex* const mutex = GetMVProbeMutex(device);
      if (!mutex)
         return;
      const std::lock_guard lock(*mutex);
      if (auto* const buffer = GetMVProbeBuffer(device, resource, true))
         buffer->maps[std::clamp(int(access), 0, 4)]++;
#endif
   }

   // Motion vectors: the CPU copy of a $Globals buffer, before its Unmap (the game has written it). Reads the mapped memory back.
   static void OnUnmapBufferRegion(reshade::api::device* device, reshade::api::resource resource)
   {
      DeviceData* const device_data = GetDeviceData(device);
      auto* const game_device_data = device_data && AreMotionVectorsEnabled(*device_data) ? &GetGameDeviceData(*device_data) : nullptr;
      if (!game_device_data)
         return;
      const std::lock_guard lock(game_device_data->mv_globals_mutex);
      const auto mapped = game_device_data->mv_mapped_globals.find(resource.handle);
      if (mapped == game_device_data->mv_mapped_globals.end())
         return;
      D3D11_BUFFER_DESC desc;
      reinterpret_cast<ID3D11Buffer*>(resource.handle)->GetDesc(&desc);
      auto& copy = game_device_data->mv_globals_copies[resource.handle];
      copy.resize(desc.ByteWidth);
      std::memcpy(copy.data(), mapped->second, desc.ByteWidth);
      game_device_data->mv_mapped_globals.erase(mapped);
   }

#if DEVELOPMENT

   static bool OnUpdateBufferRegion(reshade::api::device* device, const void* data, reshade::api::resource dest, uint64_t dest_offset, uint64_t size)
   {
      std::mutex* const mutex = GetMVProbeMutex(device);
      if (!mutex)
         return false;
      const std::lock_guard lock(*mutex);
      if (auto* const buffer = GetMVProbeBuffer(device, dest, false))
      {
         buffer->device_updates++;
         buffer->update_bytes += size;
      }
      return false;
   }

   static bool OnUpdateBufferRegionCommand(reshade::api::command_list* cmd_list, const void* data, reshade::api::resource dest, uint64_t dest_offset, uint64_t size)
   {
      reshade::api::device* const device = cmd_list->get_device();
      std::mutex* const mutex = GetMVProbeMutex(device);
      if (!mutex)
         return false;
      const std::lock_guard lock(*mutex);
      if (auto* const buffer = GetMVProbeBuffer(device, dest, false))
      {
         buffer->command_updates++;
         buffer->update_bytes += size;
      }
      return false;
   }

   // Logs every graphics draw of the probed frame: its pass (targets, depth and blend state, viewport), VS cb0 binding, the other VS
   // inputs (vertex buffers, SRVs, cbuffer slots: instancing or skinning data outside cb0) and arguments. G-buffer draws (5 render
   // targets, 6 for water) are also counted per cb0 buffer; outlines, a depth prepass or forward opaques show up as the other passes.
   static void LogMVProbeDraw(ID3D11DeviceContext* native_device_context, Persona5StrikersGameDeviceData* game_device_data, const ShaderHashesList<OneShaderPerPipeline>& original_shader_hashes)
   {
      com_ptr<ID3D11DeviceContext1> native_device_context1;
      if (FAILED(native_device_context->QueryInterface(&native_device_context1)))
         return;
      com_ptr<ID3D11Buffer> cb;
      UINT first = 0, count = 0;
      native_device_context1->VSGetConstantBuffers1(0, 1, &cb, &first, &count);
      com_ptr<ID3D11RenderTargetView> rtvs[8];
      com_ptr<ID3D11DepthStencilView> dsv;
      native_device_context->OMGetRenderTargets(8, &rtvs[0], &dsv);
      const UINT rt_count = UINT(std::ranges::count_if(rtvs, [](const auto& rtv)
         { return rtv.get() != nullptr; }));
      const bool gbuffer = rt_count >= 5;

      uint32_t line_index = 0;
      {
         const std::lock_guard lock(game_device_data->mv_probe_mutex);
         line_index = game_device_data->mv_probe_draws++;
         if (gbuffer)
         {
            game_device_data->mv_probe_gbuffer_draws++;
            game_device_data->mv_probe_context_gbuffer_draws[native_device_context]++;
         }
         if (cb)
         {
            auto& buffer = game_device_data->mv_probe_buffers[reinterpret_cast<uint64_t>(cb.get())];
            if (buffer.desc.ByteWidth == 0)
               cb->GetDesc(&buffer.desc);
            (gbuffer ? buffer.gbuffer_draws : buffer.other_draws)++;
            if (gbuffer)
               buffer.first_constants.insert(first);
         }
      }
      if (line_index >= mv_probe_max_draw_lines)
         return;

      DXGI_FORMAT rt0_format = DXGI_FORMAT_UNKNOWN;
      if (rtvs[0])
      {
         D3D11_RENDER_TARGET_VIEW_DESC rtv_desc;
         rtvs[0]->GetDesc(&rtv_desc);
         rt0_format = rtv_desc.Format;
      }
      com_ptr<ID3D11DepthStencilState> depth_stencil_state;
      UINT stencil_ref = 0;
      native_device_context->OMGetDepthStencilState(&depth_stencil_state, &stencil_ref);
      D3D11_DEPTH_STENCIL_DESC depth_desc = {};
      if (depth_stencil_state)
         depth_stencil_state->GetDesc(&depth_desc);
      com_ptr<ID3D11BlendState> blend_state;
      FLOAT blend_factor[4];
      UINT sample_mask = 0;
      native_device_context->OMGetBlendState(&blend_state, blend_factor, &sample_mask);
      D3D11_BLEND_DESC blend_desc = {};
      if (blend_state)
         blend_state->GetDesc(&blend_desc);
      D3D11_VIEWPORT viewport = {};
      UINT viewports = 1;
      native_device_context->RSGetViewports(&viewports, &viewport);

      com_ptr<ID3D11Buffer> vbs[4];
      UINT vb_strides[4] = {}, vb_offsets[4] = {};
      native_device_context->IAGetVertexBuffers(0, 4, &vbs[0], vb_strides, vb_offsets);
      com_ptr<ID3D11Buffer> ib;
      DXGI_FORMAT ib_format = DXGI_FORMAT_UNKNOWN;
      UINT ib_offset = 0;
      native_device_context->IAGetIndexBuffer(&ib, &ib_format, &ib_offset);
      // Bound slot masks: VS SRVs t0-t15 and cbuffers b0-b13
      com_ptr<ID3D11ShaderResourceView> vs_srvs[16];
      native_device_context->VSGetShaderResources(0, 16, &vs_srvs[0]);
      com_ptr<ID3D11Buffer> vs_cbs[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
      native_device_context->VSGetConstantBuffers(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, &vs_cbs[0]);
      uint32_t vs_srv_mask = 0, vs_cb_mask = 0;
      for (uint32_t i = 0; i < 16; i++)
         vs_srv_mask |= vs_srvs[i] ? (1u << i) : 0u;
      for (uint32_t i = 0; i < D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT; i++)
         vs_cb_mask |= vs_cbs[i] ? (1u << i) : 0u;

      const DrawDispatchData& draw = last_draw_dispatch_data;
      reshade::log::message(reshade::log::level::info, std::format("[P5S MV] frame={} draw={} ctx={} deferred={} vs=0x{:08X} ps=0x{:08X} rts={} rt0_format={} dsv={} depth={} depth_write={} depth_func={} stencil={} blend0={} viewport={}x{} cb0={} first={} count={} vs_cbs=0x{:X} vs_srvs=0x{:X} vbs={}/{}/{}/{} strides={}/{}/{}/{} ib={} ib_format={} indexed={} indices={} first_index={} vertex_offset={} vertices={} first_vertex={} instances={}",
                                                          cb_luma_global_settings.FrameIndex, line_index, static_cast<void*>(native_device_context), native_device_context->GetType() == D3D11_DEVICE_CONTEXT_DEFERRED,
                                                          original_shader_hashes.vertex_shaders[0], original_shader_hashes.pixel_shaders[0], rt_count, int(rt0_format), static_cast<void*>(dsv.get()),
                                                          bool(depth_desc.DepthEnable), depth_desc.DepthWriteMask == D3D11_DEPTH_WRITE_MASK_ALL, int(depth_desc.DepthFunc), bool(depth_desc.StencilEnable), bool(blend_desc.RenderTarget[0].BlendEnable), viewport.Width, viewport.Height,
                                                          static_cast<void*>(cb.get()), first, count, vs_cb_mask, vs_srv_mask,
                                                          static_cast<void*>(vbs[0].get()), static_cast<void*>(vbs[1].get()), static_cast<void*>(vbs[2].get()), static_cast<void*>(vbs[3].get()), vb_strides[0], vb_strides[1], vb_strides[2], vb_strides[3],
                                                          static_cast<void*>(ib.get()), int(ib_format), draw.indexed, draw.index_count, draw.first_index, draw.vertex_offset, draw.vertex_count, draw.first_vertex, draw.instance_count)
                                                          .c_str());

      // Which textures the draw writes and reads: resource, size and format of RT0 and PS t0-t3
      const auto describe = [](ID3D11View* view)
      {
         com_ptr<ID3D11Resource> resource;
         if (view)
            view->GetResource(&resource);
         com_ptr<ID3D11Texture2D> texture;
         D3D11_TEXTURE2D_DESC desc = {};
         if (resource && SUCCEEDED(resource->QueryInterface(&texture)))
            texture->GetDesc(&desc);
         return std::format("{}({}x{} f{})", static_cast<void*>(resource.get()), desc.Width, desc.Height, int(desc.Format));
      };
      com_ptr<ID3D11ShaderResourceView> ps_srvs[4];
      native_device_context->PSGetShaderResources(0, 4, &ps_srvs[0]);
      reshade::log::message(reshade::log::level::info, std::format("[P5S MV] frame={} draw={} resources: rt0={} ps_t0={} ps_t1={} ps_t2={} ps_t3={}", cb_luma_global_settings.FrameIndex, line_index, describe(rtvs[0].get()), describe(ps_srvs[0].get()), describe(ps_srvs[1].get()), describe(ps_srvs[2].get()), describe(ps_srvs[3].get())).c_str());
   }

   // Per buffer: how the game fills the constant buffers its G-buffer draws read as VS cb0, then clears the probe
   static void LogMVProbeSummary(Persona5StrikersGameDeviceData* game_device_data)
   {
      const std::lock_guard lock(game_device_data->mv_probe_mutex);
      const auto log = [](const std::string& line)
      { reshade::log::message(reshade::log::level::info, line.c_str()); };
      uint32_t gbuffer_buffers = 0;
      for (const auto& [handle, buffer] : game_device_data->mv_probe_buffers)
      {
         if (buffer.gbuffer_draws == 0)
            continue;
         gbuffer_buffers++;
         log(std::format("[P5S MV] cb0 buffer={} bytes={} usage={} cpu_access=0x{:X} misc=0x{:X} gbuffer_draws={} other_draws={} distinct_first={} maps(ro/wo/rw/discard)={}/{}/{}/{} device_updates={} command_updates={} update_bytes={}",
            reinterpret_cast<void*>(handle), buffer.desc.ByteWidth, int(buffer.desc.Usage), buffer.desc.CPUAccessFlags, buffer.desc.MiscFlags, buffer.gbuffer_draws, buffer.other_draws, buffer.first_constants.size(),
            buffer.maps[1], buffer.maps[2], buffer.maps[3], buffer.maps[4], buffer.device_updates, buffer.command_updates, buffer.update_bytes));
      }
      log(std::format("[P5S MV] summary: draws={} gbuffer_draws={} gbuffer_cb0_buffers={} constant_buffers_touched={} map_events={} update_events={}", game_device_data->mv_probe_draws, game_device_data->mv_probe_gbuffer_draws, gbuffer_buffers, game_device_data->mv_probe_buffers.size(), game_device_data->mv_probe_map_events, game_device_data->mv_probe_update_events));
      for (const auto& [context, draws] : game_device_data->mv_probe_context_gbuffer_draws)
         log(std::format("[P5S MV] gbuffer context={} draws={}", static_cast<void*>(context), draws));
      game_device_data->mv_probe_buffers.clear();
      game_device_data->mv_probe_context_gbuffer_draws.clear();
      game_device_data->mv_probe_draws = 0;
      game_device_data->mv_probe_gbuffer_draws = 0;
      game_device_data->mv_probe_map_events = 0;
      game_device_data->mv_probe_update_events = 0;
   }

#endif

   // The motion vector version of the bound shader, patched from Core's copy of its bytecode on first use (null if it can't be)
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
      Persona5StrikersGameDeviceData::GlobalsLayout layout;
      {
         const std::shared_lock lock(s_mutex_generic);
         if (const auto it = device_data.pipeline_cache_by_pipeline_handle.find(pipeline.handle); it != device_data.pipeline_cache_by_pipeline_handle.end() && it->second->subobjects_cache)
         {
            const auto* desc = static_cast<const reshade::api::shader_desc*>(it->second->subobjects_cache[0].data);
            const auto* code = static_cast<const uint8_t*>(desc->code);
            patched = vertex ? MotionVectorPatches::PatchVertexShader(code, desc->code_size, &error) : MotionVectorPatches::PatchPixelShader(code, desc->code_size, &error);
            com_ptr<ID3D11ShaderReflection> reflection;
            if (vertex && Shader::d3d_reflect && SUCCEEDED(Shader::d3d_reflect(code, desc->code_size, IID_PPV_ARGS(&reflection))))
            {
               // Missing names give dummy reflection objects whose GetDesc fails
               ID3D11ShaderReflectionConstantBuffer* const globals = reflection->GetConstantBufferByName("$Globals");
               D3D11_SHADER_VARIABLE_DESC variable;
               if (SUCCEEDED(globals->GetVariableByName("mW2P")->GetDesc(&variable)) && variable.Size == 64)
                  layout.view_projection = variable.StartOffset;
               if (SUCCEEDED(globals->GetVariableByName("mL2W")->GetDesc(&variable)) && variable.Size >= 48)
                  layout.world = variable.StartOffset;
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
         reshade::log::message(shader ? reshade::log::level::info : reshade::log::level::warning, std::format("[P5S MV] {} 0x{:08X} {}", vertex ? "VS" : "PS", hash, shader ? "patched" : error).c_str());
      const std::unique_lock lock(game_device_data.mv_mutex);
      if (vertex)
         game_device_data.mv_globals_layouts.try_emplace(hash, layout);
      return shaders->try_emplace(hash, shader).first->second;
   }

   // A scene frame starts at its first draw into the scene depth (a depth prepass, else the G-buffer): cleared motion vectors, the object
   // history moved to the previous frame, and this frame's jitter
   static void StartMotionVectorFrame(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, DeviceData& device_data, const uint4& depth_size)
   {
      auto& game_device_data = GetGameDeviceData(device_data);
      {
         const std::shared_lock lock_shader_objects(s_mutex_shader_objects);
         game_device_data.mv_fill_pending = game_device_data.mv_uav && HasShaders(device_data.native_compute_shaders, "P5S Motion Vector Fill CS"_h);
      }
      const FLOAT clear_value = game_device_data.mv_fill_pending ? FLT_MAX : 0.f;
      const FLOAT clear[4] = {clear_value, clear_value, 0.f, 0.f};
      native_device_context->ClearRenderTargetView(game_device_data.mv_rtv.get(), clear);
      game_device_data.mv_frame_ended = false;
      game_device_data.mv_scene_context = native_device_context;
      game_device_data.sr_split_ready = true;
      game_device_data.scene_drawn = true;
      game_device_data.sr_upscaling_context = nullptr; // Until this frame's split decides
      game_device_data.composite_target.reset();
      // Last frame's camera is the previous one, unless frames without a scene (menus) came in between
      game_device_data.mv_previous_view_projection = game_device_data.mv_view_projection;
      game_device_data.mv_previous_view_projection_valid = game_device_data.mv_view_projection_valid && game_device_data.mv_presents - game_device_data.mv_frame_present <= 1;
      game_device_data.mv_view_projection_valid = false;
      game_device_data.mv_frame_present = game_device_data.mv_presents;
      game_device_data.mv_previous_objects = std::move(game_device_data.mv_objects);
      game_device_data.mv_objects.clear();
      if (!game_device_data.mv_previous_view_projection_valid)
         game_device_data.mv_previous_objects.clear();
      std::erase_if(game_device_data.mv_previous_resources, [&](const auto& entry)
         { return entry.second.frame + 1 < game_device_data.mv_frame_present; });

      // Halton (2, 3) over the upscaler's phases
      const bool jitter = g_mv_force_jitter || IsSRActive(device_data);
      // More phases the lower the render scale (the upscaler's own count, from its last settings)
      const SR::InstanceData* const sr_instance_data = IsSRActive(device_data) ? device_data.GetSRInstanceData() : nullptr;
      const int phases = sr_instance_data ? (std::max)(sr_implementations[device_data.sr_type]->GetJitterPhases(sr_instance_data), 1) : SR::GetDefaultJitterPhases();
      game_device_data.mv_jitter = jitter ? std::array<float, 2>{SR::HaltonSequence(cb_luma_global_settings.FrameIndex % phases, 2), SR::HaltonSequence(cb_luma_global_settings.FrameIndex % phases, 3)} : std::array<float, 2>{};
      if (!game_device_data.mv_jitter_buffer)
      {
         const D3D11_BUFFER_DESC desc = {16, D3D11_USAGE_DYNAMIC, D3D11_BIND_CONSTANT_BUFFER, D3D11_CPU_ACCESS_WRITE};
         native_device->CreateBuffer(&desc, nullptr, &game_device_data.mv_jitter_buffer);
      }
      D3D11_MAPPED_SUBRESOURCE mapped;
      if (game_device_data.mv_jitter_buffer && SUCCEEDED(native_device_context->Map(game_device_data.mv_jitter_buffer.get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
      {
         // Pixels to NDC (y up)
         const float ndc_jitter[4] = {game_device_data.mv_jitter[0] * 2.f / float(depth_size.x), game_device_data.mv_jitter[1] * -2.f / float(depth_size.y), 0.f, 0.f};
         std::memcpy(mapped.pData, ndc_jitter, sizeof(ndc_jitter));
         native_device_context->Unmap(game_device_data.mv_jitter_buffer.get(), 0);
      }
   }

   // Scene draws the motion vector paths don't take still need the jitter (patched vertex shader, game pixel shader, no motion vectors):
   // the depth prepass, which the G-buffer then depth tests with GREATER_EQUAL (unjittered, sloped floors lost pixels to the jittered
   // G-buffer: black flicker), and depth tested geometry that doesn't write depth. False if it can't (the draw then goes ahead untouched).
   static bool DrawWithJitter(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, CommandListData& cmd_list_data, DeviceData& device_data, const ShaderHashesList<OneShaderPerPipeline>& original_shader_hashes, ID3D11DepthStencilView* dsv, bool depth_prepass, const std::function<void()>& draw)
   {
      auto& game_device_data = GetGameDeviceData(device_data);
      com_ptr<ID3D11Resource> depth;
      dsv->GetResource(&depth);
      // Into the last G-buffer's depth; only a depth prepass starts a frame, other draws join the started one
      if (!depth || depth != game_device_data.mv_scene_depth || !game_device_data.mv_rtv || (game_device_data.mv_frame_ended ? !depth_prepass : native_device_context != game_device_data.mv_scene_context))
         return false;
      const com_ptr<ID3D11VertexShader> vertex_shader = GetMotionVectorShader(native_device, device_data, &game_device_data.mv_vertex_shaders, original_shader_hashes.vertex_shaders[0], cmd_list_data.pipeline_state_original_vertex_shader);
      if (!vertex_shader)
         return false;
      // Objects only: full screen passes have no camera
      {
         const std::shared_lock lock(game_device_data.mv_mutex);
         if (const auto it = game_device_data.mv_globals_layouts.find(original_shader_hashes.vertex_shaders[0]); it == game_device_data.mv_globals_layouts.end() || it->second.view_projection == UINT_MAX)
            return false;
      }
      if (game_device_data.mv_frame_ended)
      {
         uint4 depth_size;
         DXGI_FORMAT depth_format;
         GetResourceInfo(depth.get(), depth_size, depth_format);
         StartMotionVectorFrame(native_device, native_device_context, device_data, depth_size);
      }

      com_ptr<ID3D11VertexShader> original_vertex_shader;
      native_device_context->VSGetShader(&original_vertex_shader, nullptr, nullptr);
      com_ptr<ID3D11Buffer> original_jitter;
      native_device_context->VSGetConstantBuffers(MotionVectorPatches::jitter_slot, 1, &original_jitter);
      ID3D11Buffer* const jitter = game_device_data.mv_jitter_buffer.get();
      native_device_context->VSSetConstantBuffers(MotionVectorPatches::jitter_slot, 1, &jitter);
      native_device_context->VSSetShader(vertex_shader.get(), nullptr, 0);
      draw();
      // Set directly, so Core's tracking of the bound state never sees them: put the game's back
      native_device_context->VSSetShader(original_vertex_shader.get(), nullptr, 0);
      ID3D11Buffer* const restored_jitter = original_jitter.get();
      native_device_context->VSSetConstantBuffers(MotionVectorPatches::jitter_slot, 1, &restored_jitter);
      game_device_data.mv_jitter_only_draws++;
      return true;
   }

   // Draws a G-buffer draw, or a forward redraw of the scene into the same depth (outlines, sky), with the patched shaders, also into
   // the motion vector target (at "target_slot", past the game's targets), with the previous frame's $Globals at "previous_globals_slot".
   // False if it can't (the draw then goes ahead untouched).
   static bool DrawWithMotionVectors(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, CommandListData& cmd_list_data, DeviceData& device_data, const ShaderHashesList<OneShaderPerPipeline>& original_shader_hashes, ID3D11RenderTargetView* const (&rtvs)[8], ID3D11DepthStencilView* dsv, bool gbuffer, const std::function<void()>& draw)
   {
      auto& game_device_data = GetGameDeviceData(device_data);
      com_ptr<ID3D11Resource> depth;
      if (dsv)
         dsv->GetResource(&depth);
      // Forward draws: after this frame's G-buffer (and before its post process), into its depth
      if (!gbuffer && (game_device_data.mv_frame_ended || native_device_context != game_device_data.mv_scene_context || !depth || depth != game_device_data.mv_scene_depth || !game_device_data.mv_rtv))
         return false;
      com_ptr<ID3D11BlendState> blend_state;
      FLOAT blend_factor[4];
      UINT sample_mask;
      native_device_context->OMGetBlendState(&blend_state, blend_factor, &sample_mask);
      D3D11_BLEND_DESC blend_desc = {};
      if (blend_state)
         blend_state->GetDesc(&blend_desc);
      const com_ptr<ID3D11VertexShader> vertex_shader = GetMotionVectorShader(native_device, device_data, &game_device_data.mv_vertex_shaders, original_shader_hashes.vertex_shaders[0], cmd_list_data.pipeline_state_original_vertex_shader);
      const com_ptr<ID3D11PixelShader> pixel_shader = GetMotionVectorShader(native_device, device_data, &game_device_data.mv_pixel_shaders, original_shader_hashes.pixel_shaders[0], cmd_list_data.pipeline_state_original_pixel_shader);
      // Without independent blending the MV target takes RT0's blend, which must be off; with it, a copy of the state writes it unblended
      if ((!blend_desc.IndependentBlendEnable && blend_desc.RenderTarget[0].BlendEnable) || !vertex_shader || !pixel_shader || !dsv)
      {
         game_device_data.mv_skipped_draws++;
         return false;
      }
      // Forward draws that aren't objects (full screen passes) have no camera
      if (!gbuffer)
      {
         const std::shared_lock lock(game_device_data.mv_mutex);
         if (const auto it = game_device_data.mv_globals_layouts.find(original_shader_hashes.vertex_shaders[0]); it == game_device_data.mv_globals_layouts.end() || it->second.view_projection == UINT_MAX)
            return false;
      }
      com_ptr<ID3D11BlendState> motion_vector_blend_state;
      if (blend_desc.IndependentBlendEnable)
      {
         const std::unique_lock lock(game_device_data.mv_mutex);
         auto& [cached_desc, cached_state] = game_device_data.mv_blend_states[blend_state.get()];
         if (!cached_state || std::memcmp(&cached_desc, &blend_desc, sizeof(blend_desc)) != 0)
         {
            cached_desc = blend_desc;
            D3D11_BLEND_DESC desc = blend_desc;
            desc.RenderTarget[MotionVectorPatches::target_slot] = {FALSE, D3D11_BLEND_ONE, D3D11_BLEND_ZERO, D3D11_BLEND_OP_ADD, D3D11_BLEND_ONE, D3D11_BLEND_ZERO, D3D11_BLEND_OP_ADD, D3D11_COLOR_WRITE_ENABLE_ALL};
            cached_state.reset();
            native_device->CreateBlendState(&desc, &cached_state);
         }
         motion_vector_blend_state = cached_state;
         if (!motion_vector_blend_state)
         {
            game_device_data.mv_skipped_draws++;
            return false;
         }
      }

      // The G-buffer owns the target (sized like the scene depth) and starts the frames; forward draws only add to it
      if (gbuffer)
      {
         uint4 depth_size;
         DXGI_FORMAT depth_format;
         GetResourceInfo(depth.get(), depth_size, depth_format);
         game_device_data.mv_scene_depth = depth;
         {
            const std::unique_lock lock(game_device_data.mv_mutex);
            D3D11_TEXTURE2D_DESC desc = {};
            if (game_device_data.mv_texture)
               game_device_data.mv_texture->GetDesc(&desc);
            if (desc.Width != depth_size.x || desc.Height != depth_size.y)
            {
               game_device_data.mv_texture.reset();
               game_device_data.mv_rtv.reset();
               game_device_data.mv_uav.reset();
               D3D11_FEATURE_DATA_FORMAT_SUPPORT2 support = {DXGI_FORMAT_R32G32_FLOAT};
               const bool typed_uav_load = SUCCEEDED(native_device->CheckFeatureSupport(D3D11_FEATURE_FORMAT_SUPPORT2, &support, sizeof(support))) && (support.OutFormatSupport2 & D3D11_FORMAT_SUPPORT2_UAV_TYPED_LOAD) != 0;
               desc = {depth_size.x, depth_size.y, 1, 1, DXGI_FORMAT_R32G32_FLOAT, {1, 0}, D3D11_USAGE_DEFAULT, D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | (typed_uav_load ? D3D11_BIND_UNORDERED_ACCESS : 0u)};
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
         if (game_device_data.mv_frame_ended)
            StartMotionVectorFrame(native_device, native_device_context, device_data, depth_size);
      }

      com_ptr<ID3D11VertexShader> original_vertex_shader;
      com_ptr<ID3D11PixelShader> original_pixel_shader;
      native_device_context->VSGetShader(&original_vertex_shader, nullptr, nullptr);
      native_device_context->PSGetShader(&original_pixel_shader, nullptr, nullptr);
      com_ptr<ID3D11Buffer> globals;
      com_ptr<ID3D11Buffer> original_previous_globals;
      native_device_context->VSGetConstantBuffers(0, 1, &globals);
      native_device_context->VSGetConstantBuffers(MotionVectorPatches::previous_globals_slot, 1, &original_previous_globals);
      com_ptr<ID3D11Buffer> original_jitter;
      native_device_context->VSGetConstantBuffers(MotionVectorPatches::jitter_slot, 1, &original_jitter);
      ID3D11Buffer* const jitter = game_device_data.mv_jitter_buffer.get();
      native_device_context->VSSetConstantBuffers(MotionVectorPatches::jitter_slot, 1, &jitter);

      ID3D11RenderTargetView* targets[MotionVectorPatches::target_slot + 1] = {};
      std::copy_n(rtvs, MotionVectorPatches::target_slot, targets);
      targets[MotionVectorPatches::target_slot] = game_device_data.mv_rtv.get();
      native_device_context->OMSetRenderTargets(MotionVectorPatches::target_slot + 1, targets, dsv);
      // The previous frame's $Globals: last frame's copy of the same object's if found, otherwise this draw's with last frame's camera
      // (no object motion). Until a buffer has a CPU copy, its draws get the current data (zero motion).
      std::vector<uint8_t> previous_globals_data;
      {
         const std::lock_guard lock(game_device_data.mv_globals_mutex);
         const uint64_t handle = reinterpret_cast<uint64_t>(globals.get());
         game_device_data.mv_globals_buffers.insert(handle);
         if (const auto copy = game_device_data.mv_globals_copies.find(handle); copy != game_device_data.mv_globals_copies.end())
            previous_globals_data = copy->second;
      }
      Persona5StrikersGameDeviceData::GlobalsLayout layout;
      {
         const std::shared_lock lock(game_device_data.mv_mutex);
         if (const auto it = game_device_data.mv_globals_layouts.find(original_shader_hashes.vertex_shaders[0]); it != game_device_data.mv_globals_layouts.end())
            layout = it->second;
      }
      ID3D11Buffer* previous_globals = globals.get();
      if (!previous_globals_data.empty() && layout.view_projection != UINT_MAX && layout.view_projection + 64 <= previous_globals_data.size())
      {
         uint8_t* const view_projection = previous_globals_data.data() + layout.view_projection;
         // The frame's first draw has the camera
         if (!game_device_data.mv_view_projection_valid)
         {
            std::memcpy(game_device_data.mv_view_projection.data(), view_projection, 64);
            game_device_data.mv_view_projection_valid = true;
         }

         // The draw key: the same mesh drawn by the same shaders. Several objects can share it (e.g. props), the translation tells them apart.
         com_ptr<ID3D11Buffer> vertex_buffer;
         UINT vertex_stride = 0, vertex_offset = 0;
         native_device_context->IAGetVertexBuffers(0, 1, &vertex_buffer, &vertex_stride, &vertex_offset);
         com_ptr<ID3D11Buffer> index_buffer;
         DXGI_FORMAT index_format;
         UINT index_offset = 0;
         native_device_context->IAGetIndexBuffer(&index_buffer, &index_format, &index_offset);
         const DrawDispatchData& draw_data = last_draw_dispatch_data;
         uint64_t key = 14695981039346656037ull; // FNV-1a over the key's values
         for (const uint64_t value : {uint64_t(original_shader_hashes.vertex_shaders[0]), uint64_t(original_shader_hashes.pixel_shaders[0]), reinterpret_cast<uint64_t>(vertex_buffer.get()), uint64_t(vertex_offset), reinterpret_cast<uint64_t>(index_buffer.get()), uint64_t(index_offset), uint64_t(draw_data.index_count), uint64_t(draw_data.first_index), uint64_t(uint32_t(draw_data.vertex_offset)), uint64_t(draw_data.vertex_count), uint64_t(draw_data.first_vertex)})
            key = (key ^ value) * 1099511628211ull;
         std::array<float, 3> translation = {};
         if (layout.world != UINT_MAX && layout.world + 48 <= previous_globals_data.size())
         {
            const float* const world = reinterpret_cast<const float*>(previous_globals_data.data() + layout.world);
            translation = {world[3], world[7], world[11]};
         }
         game_device_data.mv_objects[key].push_back({translation, previous_globals_data});

         // ponytail: linear search among the key's candidates (a handful at most); a spatial lookup if big crowds share a mesh
         const Persona5StrikersGameDeviceData::MotionVectorObject* match = nullptr;
         size_t candidates = 0;
         if (const auto previous = game_device_data.mv_previous_objects.find(key); previous != game_device_data.mv_previous_objects.end())
         {
            float nearest = FLT_MAX;
            for (const auto& object : previous->second)
            {
               const float dx = object.translation[0] - translation[0], dy = object.translation[1] - translation[1], dz = object.translation[2] - translation[2];
               const float distance = dx * dx + dy * dy + dz * dz;
               if (object.globals.size() == previous_globals_data.size() && distance < nearest)
               {
                  nearest = distance;
                  match = &object;
               }
               candidates++;
            }
         }
         if (match)
         {
            previous_globals_data = match->globals;
            game_device_data.mv_matched_draws++;
            if (candidates > 1)
               game_device_data.mv_ambiguous_draws++;
         }
         else if (std::memcmp(view_projection, game_device_data.mv_view_projection.data(), 64) != 0)
         {
            game_device_data.mv_other_camera_draws++;
         }
         else if (game_device_data.mv_previous_view_projection_valid)
         {
            std::memcpy(view_projection, game_device_data.mv_previous_view_projection.data(), 64);
         }

         com_ptr<ID3D11Buffer>& upload = game_device_data.mv_previous_globals_buffers[UINT(previous_globals_data.size())];
         if (!upload)
         {
            const D3D11_BUFFER_DESC desc = {UINT(previous_globals_data.size()), D3D11_USAGE_DYNAMIC, D3D11_BIND_CONSTANT_BUFFER, D3D11_CPU_ACCESS_WRITE};
            native_device->CreateBuffer(&desc, nullptr, &upload);
         }
         D3D11_MAPPED_SUBRESOURCE mapped;
         if (upload && SUCCEEDED(native_device_context->Map(upload.get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
         {
            std::memcpy(mapped.pData, previous_globals_data.data(), previous_globals_data.size());
            native_device_context->Unmap(upload.get(), 0);
            previous_globals = upload.get();
         }
      }
      else
      {
         game_device_data.mv_uncopied_draws++;
      }
      native_device_context->VSSetConstantBuffers(MotionVectorPatches::previous_globals_slot, 1, &previous_globals);
      // The vertex shader's resources for its second run: the previous frame's copies, or the current ones (immutable, or nothing from
      // the previous frame)
      com_ptr<ID3D11ShaderResourceView> srvs[MotionVectorPatches::resource_slots];
      native_device_context->VSGetShaderResources(0, MotionVectorPatches::resource_slots, &srvs[0]);
      ID3D11ShaderResourceView* previous_srvs[MotionVectorPatches::resource_slots] = {};
      for (UINT slot = 0; slot < MotionVectorPatches::resource_slots; slot++)
      {
         previous_srvs[slot] = srvs[slot].get();
         com_ptr<ID3D11Resource> resource;
         if (srvs[slot])
            srvs[slot]->GetResource(&resource);
         D3D11_RESOURCE_DIMENSION dimension = D3D11_RESOURCE_DIMENSION_UNKNOWN;
         if (resource)
            resource->GetType(&dimension);
         D3D11_BUFFER_DESC buffer_desc = {};
         D3D11_TEXTURE1D_DESC texture_1d_desc = {};
         D3D11_TEXTURE2D_DESC texture_2d_desc = {};
         D3D11_TEXTURE3D_DESC texture_3d_desc = {};
         D3D11_USAGE usage = D3D11_USAGE_IMMUTABLE;
         switch (dimension)
         {
         case D3D11_RESOURCE_DIMENSION_BUFFER:
            static_cast<ID3D11Buffer*>(resource.get())->GetDesc(&buffer_desc);
            usage = buffer_desc.Usage;
            break;
         case D3D11_RESOURCE_DIMENSION_TEXTURE1D:
            static_cast<ID3D11Texture1D*>(resource.get())->GetDesc(&texture_1d_desc);
            usage = texture_1d_desc.Usage;
            break;
         case D3D11_RESOURCE_DIMENSION_TEXTURE2D:
            static_cast<ID3D11Texture2D*>(resource.get())->GetDesc(&texture_2d_desc);
            usage = texture_2d_desc.Usage;
            break;
         case D3D11_RESOURCE_DIMENSION_TEXTURE3D:
            static_cast<ID3D11Texture3D*>(resource.get())->GetDesc(&texture_3d_desc);
            usage = texture_3d_desc.Usage;
            break;
         }
         // Immutable resources (or none) are the same every frame
         if (usage == D3D11_USAGE_IMMUTABLE)
            continue;

         auto& entry = game_device_data.mv_previous_resources[resource.get()];
         if (entry.frame != game_device_data.mv_frame_present || !entry.copies[0])
         {
            entry.resource = resource;
            entry.has_previous = entry.copies[0] && entry.frame + 1 == game_device_data.mv_frame_present;
            std::swap(entry.copies[0], entry.copies[1]);
            entry.previous_view.reset();
            entry.frame = game_device_data.mv_frame_present;
            // Plain GPU resources (a dynamic one can't be a copy destination)
            if (!entry.copies[0])
            {
               com_ptr<ID3D11Buffer> buffer;
               com_ptr<ID3D11Texture1D> texture_1d;
               com_ptr<ID3D11Texture2D> texture_2d;
               com_ptr<ID3D11Texture3D> texture_3d;
               switch (dimension)
               {
               case D3D11_RESOURCE_DIMENSION_BUFFER:
                  buffer_desc = {buffer_desc.ByteWidth, D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, buffer_desc.MiscFlags & (D3D11_RESOURCE_MISC_BUFFER_STRUCTURED | D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS), buffer_desc.StructureByteStride};
                  if (SUCCEEDED(native_device->CreateBuffer(&buffer_desc, nullptr, &buffer)))
                     buffer->QueryInterface(&entry.copies[0]);
                  break;
               case D3D11_RESOURCE_DIMENSION_TEXTURE1D:
                  texture_1d_desc = {texture_1d_desc.Width, texture_1d_desc.MipLevels, texture_1d_desc.ArraySize, texture_1d_desc.Format, D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE};
                  if (SUCCEEDED(native_device->CreateTexture1D(&texture_1d_desc, nullptr, &texture_1d)))
                     texture_1d->QueryInterface(&entry.copies[0]);
                  break;
               case D3D11_RESOURCE_DIMENSION_TEXTURE2D:
                  texture_2d_desc = {texture_2d_desc.Width, texture_2d_desc.Height, texture_2d_desc.MipLevels, texture_2d_desc.ArraySize, texture_2d_desc.Format, texture_2d_desc.SampleDesc, D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, texture_2d_desc.MiscFlags & D3D11_RESOURCE_MISC_TEXTURECUBE};
                  if (SUCCEEDED(native_device->CreateTexture2D(&texture_2d_desc, nullptr, &texture_2d)))
                     texture_2d->QueryInterface(&entry.copies[0]);
                  break;
               case D3D11_RESOURCE_DIMENSION_TEXTURE3D:
                  texture_3d_desc = {texture_3d_desc.Width, texture_3d_desc.Height, texture_3d_desc.Depth, texture_3d_desc.MipLevels, texture_3d_desc.Format, D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE};
                  if (SUCCEEDED(native_device->CreateTexture3D(&texture_3d_desc, nullptr, &texture_3d)))
                     texture_3d->QueryInterface(&entry.copies[0]);
                  break;
               }
            }
            if (entry.copies[0])
            {
               native_device_context->CopyResource(entry.copies[0].get(), resource.get());
               game_device_data.mv_resource_copies++;
            }
         }
         if (!entry.has_previous || !entry.copies[1])
            continue;
         D3D11_SHADER_RESOURCE_VIEW_DESC view_desc;
         srvs[slot]->GetDesc(&view_desc);
         if (!entry.previous_view || std::memcmp(&view_desc, &entry.previous_view_desc, sizeof(view_desc)) != 0)
         {
            entry.previous_view.reset();
            entry.previous_view_desc = view_desc;
            native_device->CreateShaderResourceView(entry.copies[1].get(), &view_desc, &entry.previous_view);
         }
         if (entry.previous_view)
            previous_srvs[slot] = entry.previous_view.get();
      }
      native_device_context->VSSetShaderResources(MotionVectorPatches::previous_resources_slot, MotionVectorPatches::resource_slots, previous_srvs);
      native_device_context->VSSetShader(vertex_shader.get(), nullptr, 0);
      native_device_context->PSSetShader(pixel_shader.get(), nullptr, 0);
      if (motion_vector_blend_state)
         native_device_context->OMSetBlendState(motion_vector_blend_state.get(), blend_factor, sample_mask);
      draw();
      if (motion_vector_blend_state)
         native_device_context->OMSetBlendState(blend_state.get(), blend_factor, sample_mask);
      // Set directly, so Core's tracking of the bound state never sees them: put the game's back
      native_device_context->VSSetShader(original_vertex_shader.get(), nullptr, 0);
      native_device_context->PSSetShader(original_pixel_shader.get(), nullptr, 0);
      ID3D11Buffer* const restored_globals = original_previous_globals.get();
      native_device_context->VSSetConstantBuffers(MotionVectorPatches::previous_globals_slot, 1, &restored_globals);
      ID3D11Buffer* const restored_jitter = original_jitter.get();
      native_device_context->VSSetConstantBuffers(MotionVectorPatches::jitter_slot, 1, &restored_jitter);
      ID3D11ShaderResourceView* const null_srvs[MotionVectorPatches::resource_slots] = {};
      native_device_context->VSSetShaderResources(MotionVectorPatches::previous_resources_slot, MotionVectorPatches::resource_slots, null_srvs);
      native_device_context->OMSetRenderTargets(8, rtvs, dsv);
      game_device_data.mv_patched_draws++;
      if (!gbuffer)
         game_device_data.mv_forward_draws++;
      return true;
   }

public:
   void OnCreateDevice(ID3D11Device* native_device, DeviceData& device_data) override
   {
      auto* game_device_data = new Persona5StrikersGameDeviceData;
      device_data.game = game_device_data;
      // No TAA to replace, but Core's upscaler UI checks for it
      device_data.taa_detected = true;
      for (int i = 0; i < 2; i++)
      {
         const UINT8 write_mask = i == 0 ? D3D11_COLOR_WRITE_ENABLE_ALL : D3D11_COLOR_WRITE_ENABLE_ALPHA;
         D3D11_BLEND_DESC blend_desc = {};
         blend_desc.RenderTarget[0] = {TRUE, D3D11_BLEND_ONE, D3D11_BLEND_ONE, D3D11_BLEND_OP_MIN, D3D11_BLEND_ONE, D3D11_BLEND_ONE, D3D11_BLEND_OP_MIN, write_mask};
         native_device->CreateBlendState(&blend_desc, &game_device_data->ui_min_blend_states[i]);
         blend_desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_MAX;
         blend_desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_MAX;
         native_device->CreateBlendState(&blend_desc, &game_device_data->ui_max_blend_states[i]);
      }
   }

   // "mov_sat o0.xyzw, o0.xyzw" before the final ret: the clamp the vanilla UNORM swapchain applied to the UI (as in Yakuza 3 Remastered)
   std::unique_ptr<std::byte[]> PatchShaderBytecodeSync(const std::byte* code, size_t& size, reshade::api::pipeline_subobject_type type, uint64_t shader_hash, const std::byte* shader_object, size_t shader_object_size) override
   {
      constexpr uint32_t ret_token = ENCODE_D3D10_SB_OPCODE_TYPE(D3D10_SB_OPCODE_RET) | ENCODE_D3D10_SB_TOKENIZED_INSTRUCTION_LENGTH(1);
      if (type != reshade::api::pipeline_subobject_type::pixel_shader || !ui_pixel_shaders.contains(uint32_t(shader_hash)) || size % sizeof(uint32_t) != 0 || size < sizeof(uint32_t) || reinterpret_cast<const uint32_t*>(code)[size / sizeof(uint32_t) - 1] != ret_token)
         return nullptr;
      // Encoded by hand rather than with ShaderPatching::GetSatInstruction, whose source operand uses the mask selection mode;
      // fxc encodes sources as swizzles.
      constexpr uint32_t operand_o0 = ENCODE_D3D10_SB_OPERAND_NUM_COMPONENTS(D3D10_SB_OPERAND_4_COMPONENT) | ENCODE_D3D10_SB_OPERAND_TYPE(D3D10_SB_OPERAND_TYPE_OUTPUT) | ENCODE_D3D10_SB_OPERAND_INDEX_DIMENSION(D3D10_SB_OPERAND_INDEX_1D) | ENCODE_D3D10_SB_OPERAND_INDEX_REPRESENTATION(0, D3D10_SB_OPERAND_INDEX_IMMEDIATE32);
      constexpr uint32_t patch[] = {
         ENCODE_D3D10_SB_OPCODE_TYPE(D3D10_SB_OPCODE_MOV) | ENCODE_D3D10_SB_TOKENIZED_INSTRUCTION_LENGTH(5) | ENCODE_D3D10_SB_INSTRUCTION_SATURATE(true),
         operand_o0 | ENCODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(D3D10_SB_OPERAND_4_COMPONENT_MASK_MODE) | D3D10_SB_OPERAND_4_COMPONENT_MASK_ALL, 0,
         operand_o0 | ENCODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(D3D10_SB_OPERAND_4_COMPONENT_SWIZZLE_MODE) | D3D10_SB_OPERAND_4_COMPONENT_NOSWIZZLE, 0};
      static_assert(patch[0] == 0x05002036 && patch[1] == 0x001020F2 && patch[3] == 0x00102E46);
      const size_t ret_offset = size - sizeof(uint32_t);
      auto new_code = std::make_unique<std::byte[]>(size + sizeof(patch));
      std::memcpy(new_code.get(), code, ret_offset);
      std::memcpy(new_code.get() + ret_offset, patch, sizeof(patch));
      std::memcpy(new_code.get() + ret_offset + sizeof(patch), code + ret_offset, sizeof(uint32_t));
      size += sizeof(patch);
      return new_code;
   }

   void OnDestroyDeviceData(DeviceData& device_data) override
   {
      // GameDeviceData lacks a virtual destructor; delete through the concrete type to release derived members.
      delete static_cast<Persona5StrikersGameDeviceData*>(device_data.game);
      device_data.game = nullptr;
   }

   void OnInit(bool async) override
   {
      std::vector<ShaderDefineData> game_shader_defines_data = {
         {"TONEMAP_TYPE", '1', true, false, "0 - SDR: Vanilla (reference)\n1 - HDR: LUT grade extended above mid gray + DICE display map", 1},
         {"XE_GTAO_QUALITY", '3', true, false, "XeGTAO quality (slice count)\n0 - Low\n1 - Medium\n2 - High\n3 - Very High\n4 - Ultra", 4},
      };
      shader_defines_data.append_range(game_shader_defines_data);
      assert(shader_defines_data.size() < MAX_SHADER_DEFINES);

      GetShaderDefineData(POST_PROCESS_SPACE_TYPE_HASH).SetDefaultValue('1'); // The composite, UI and FXAA write the swapchain through sRGB views, so in linear
      GetShaderDefineData(EARLY_DISPLAY_ENCODING_HASH).SetDefaultValue('0');
      GetShaderDefineData(VANILLA_ENCODING_TYPE_HASH).SetDefaultValue('0'); // sRGB (implicit, through the swapchain views)
      GetShaderDefineData(GAMMA_CORRECTION_TYPE_HASH).SetDefaultValue('1');
      GetShaderDefineData(UI_DRAW_TYPE_HASH).SetDefaultValue('2'); // The UI blends straight onto the swapchain after the composite

      // The game binds b0-b4
      luma_settings_cbuffer_index = 13;
      luma_data_cbuffer_index = 12;

      default_luma_global_game_settings.Dithering = 1.f;
      default_luma_global_game_settings.SMAAEnable = 1.f;
      default_luma_global_game_settings.VignetteIntensity = 1.f;
      default_luma_global_game_settings.LensFlareIntensity = 1.f;
      default_luma_global_game_settings.Exposure = 1.f;
      default_luma_global_game_settings.ColorGradingIntensity = 1.f;
      default_luma_global_game_settings.Saturation = 1.f;
      default_luma_global_game_settings.BloomIntensity = 1.f;
      cb_luma_global_settings.GameSettings = default_luma_global_game_settings;

      native_shaders_definitions.emplace(CompileTimeStringHash("P5S SMAA Encode CS"), ShaderDefinition{"Luma_P5S_SMAAEncode", reshade::api::pipeline_subobject_type::compute_shader});
      native_shaders_definitions.emplace(CompileTimeStringHash("P5S Motion Vector Fill CS"), ShaderDefinition{"Luma_P5S_MotionVectorFill", reshade::api::pipeline_subobject_type::compute_shader});
      native_shaders_definitions.emplace(CompileTimeStringHash("P5S SMAA Predication CS"), ShaderDefinition{"Luma_P5S_SMAAPredication", reshade::api::pipeline_subobject_type::compute_shader});
      native_shaders_definitions.emplace(CompileTimeStringHash("P5S SMAA Finalize PS"), ShaderDefinition{"Luma_P5S_SMAAFinalize", reshade::api::pipeline_subobject_type::pixel_shader});
      native_shaders_definitions.emplace(CompileTimeStringHash("P5S Draw White PS"), ShaderDefinition{"Luma_DrawColor_PS", reshade::api::pipeline_subobject_type::pixel_shader, nullptr, nullptr, {{"COLOR", "float4(1.0, 1.0, 1.0, 1.0)"}}});
      native_shaders_definitions.emplace(CompileTimeStringHash("P5S Draw Black PS"), ShaderDefinition{"Luma_DrawColor_PS", reshade::api::pipeline_subobject_type::pixel_shader, nullptr, nullptr, {{"COLOR", "float4(0.0, 0.0, 0.0, 0.0)"}}});
      native_shaders_definitions.emplace(CompileTimeStringHash("P5S UI Peak Clamp PS"), ShaderDefinition{"Luma_P5S_UIPeakClamp", reshade::api::pipeline_subobject_type::pixel_shader});
      native_shaders_definitions.emplace(CompileTimeStringHash("P5S Downsample VS"), ShaderDefinition{"Luma_P5S_Downsample", reshade::api::pipeline_subobject_type::vertex_shader, nullptr, "vs_main"});
      native_shaders_definitions.emplace(CompileTimeStringHash("P5S Downsample PS"), ShaderDefinition{"Luma_P5S_Downsample", reshade::api::pipeline_subobject_type::pixel_shader, nullptr, "ps_main"});
      native_shaders_definitions.emplace(CompileTimeStringHash("P5S Layer Quad VS"), ShaderDefinition{"Luma_P5S_LayerQuad", reshade::api::pipeline_subobject_type::vertex_shader});
      // XeGTAO passes (Luma_P5S_XeGTAO.hlsl); the two denoisers differ only by XE_GTAO_FINAL_APPLY.
      native_shaders_definitions.emplace(CompileTimeStringHash("P5S XeGTAO Prefilter Depths CS"), ShaderDefinition{"Luma_P5S_XeGTAO", reshade::api::pipeline_subobject_type::compute_shader, nullptr, "prefilter_depths16x16_cs"});
      native_shaders_definitions.emplace(CompileTimeStringHash("P5S XeGTAO Main Pass CS"), ShaderDefinition{"Luma_P5S_XeGTAO", reshade::api::pipeline_subobject_type::compute_shader, nullptr, "main_pass_cs"});
      native_shaders_definitions.emplace(CompileTimeStringHash("P5S XeGTAO Denoise Pass 1 CS"), ShaderDefinition{"Luma_P5S_XeGTAO", reshade::api::pipeline_subobject_type::compute_shader, nullptr, "denoise_pass_cs", {{"XE_GTAO_FINAL_APPLY", "0"}}});
      native_shaders_definitions.emplace(CompileTimeStringHash("P5S XeGTAO Denoise Pass 2 CS"), ShaderDefinition{"Luma_P5S_XeGTAO", reshade::api::pipeline_subobject_type::compute_shader, nullptr, "denoise_pass_cs", {{"XE_GTAO_FINAL_APPLY", "1"}}});

      reshade::register_event<reshade::addon_event::map_buffer_region>(OnMapBufferRegion);
      reshade::register_event<reshade::addon_event::unmap_buffer_region>(OnUnmapBufferRegion);
      reshade::register_event<reshade::addon_event::execute_secondary_command_list>(OnExecuteSecondaryCommandList);
#if DEVELOPMENT
      reshade::register_event<reshade::addon_event::update_buffer_region>(OnUpdateBufferRegion);
      reshade::register_event<reshade::addon_event::update_buffer_region_command>(OnUpdateBufferRegionCommand);
#endif
   }

   static void UnregisterEvents()
   {
      reshade::unregister_event<reshade::addon_event::map_buffer_region>(OnMapBufferRegion);
      reshade::unregister_event<reshade::addon_event::unmap_buffer_region>(OnUnmapBufferRegion);
      reshade::unregister_event<reshade::addon_event::execute_secondary_command_list>(OnExecuteSecondaryCommandList);
#if DEVELOPMENT
      reshade::unregister_event<reshade::addon_event::update_buffer_region>(OnUpdateBufferRegion);
      reshade::unregister_event<reshade::addon_event::update_buffer_region_command>(OnUpdateBufferRegionCommand);
#endif
   }

   // Draws the composite, then SMAA on the canvas it wrote (the swapchain), before the UI: copy, gamma encode, predication,
   // SMAA into the gamma copy, then the finalize pass (RCAS, decode, dither) back into the canvas; without RCAS, SMAA straight into the canvas.
   // Without "smaa" (DLSS/FSR already antialiased the scene) only RCAS runs: copy, gamma encode, finalize.
   // Anything missing (shaders still compiling, an unexpected target) leaves the composite alone, and the vanilla FXAA runs (not after DLSS/FSR).
   static DrawOrDispatchOverrideType DrawCompositeWithSMAAAndRCAS(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, CommandListData& cmd_list_data, DeviceData& device_data, bool* updated_cbuffers, const std::function<void()>& original_draw_dispatch_func, bool smaa)
   {
      com_ptr<ID3D11RenderTargetView> canvas_rtv;
      native_device_context->OMGetRenderTargets(1, &canvas_rtv, nullptr);
      com_ptr<ID3D11Resource> canvas_resource;
      if (canvas_rtv)
         canvas_rtv->GetResource(&canvas_resource);
      com_ptr<ID3D11Texture2D> canvas_texture;
      // Not the main menu's second composite, into an off-screen target
      if (!canvas_resource || FAILED(canvas_resource->QueryInterface(&canvas_texture)) || !IsBackBuffer(&device_data, canvas_resource.get()))
         return DrawOrDispatchOverrideType::None;
      auto& game_device_data = GetGameDeviceData(device_data);
      D3D11_TEXTURE2D_DESC canvas_desc;
      canvas_texture->GetDesc(&canvas_desc);
      // The upgraded (linear fp16) swapchain, which the SMAA copies are in
      if (canvas_desc.SampleDesc.Count != 1 || canvas_desc.ArraySize != 1 || (canvas_desc.Format != DXGI_FORMAT_R16G16B16A16_FLOAT && canvas_desc.Format != DXGI_FORMAT_R16G16B16A16_TYPELESS))
         return DrawOrDispatchOverrideType::None;

      // Held through SMAA so a shader reload cannot release them mid-use; "DrawSMAA" looks its shaders up with "at".
      const std::shared_lock lock_shader_objects(s_mutex_shader_objects);
      if (!HasShaders(device_data.native_vertex_shaders, "Copy VS"_h) || !HasShaders(device_data.native_pixel_shaders, "P5S SMAA Finalize PS"_h) || !HasShaders(device_data.native_compute_shaders, "P5S SMAA Encode CS"_h))
         return DrawOrDispatchOverrideType::None;
      if (smaa && (!HasShaders(device_data.native_vertex_shaders, "SMAA Edge Detection VS"_h, "SMAA Blending Weight Calculation VS"_h, "SMAA Neighborhood Blending VS"_h) || !HasShaders(device_data.native_pixel_shaders, "SMAA Edge Detection PS"_h, "SMAA Blending Weight Calculation PS"_h, "SMAA Neighborhood Blending PS"_h)))
         return DrawOrDispatchOverrideType::None;

      D3D11_TEXTURE2D_DESC scratch_desc = {};
      if (game_device_data.smaa_linear_texture)
         game_device_data.smaa_linear_texture->GetDesc(&scratch_desc);
      if (scratch_desc.Width != canvas_desc.Width || scratch_desc.Height != canvas_desc.Height)
      {
         game_device_data.smaa_linear_texture.reset();
         game_device_data.smaa_linear_srv.reset();
         game_device_data.smaa_gamma_srv.reset();
         game_device_data.smaa_gamma_rtv.reset();
         game_device_data.smaa_gamma_uav.reset();
         game_device_data.smaa_predication_srv.reset();
         game_device_data.smaa_predication_uav.reset();
         D3D11_TEXTURE2D_DESC desc = canvas_desc;
         desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
         desc.MipLevels = 1;
         desc.Usage = D3D11_USAGE_DEFAULT;
         desc.CPUAccessFlags = 0;
         desc.MiscFlags = 0;
         desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
         bool ok = SUCCEEDED(native_device->CreateTexture2D(&desc, nullptr, &game_device_data.smaa_linear_texture)) && SUCCEEDED(native_device->CreateShaderResourceView(game_device_data.smaa_linear_texture.get(), nullptr, &game_device_data.smaa_linear_srv));
         desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET | D3D11_BIND_UNORDERED_ACCESS;
         com_ptr<ID3D11Texture2D> gamma_texture;
         ok = ok && SUCCEEDED(native_device->CreateTexture2D(&desc, nullptr, &gamma_texture)) && SUCCEEDED(native_device->CreateShaderResourceView(gamma_texture.get(), nullptr, &game_device_data.smaa_gamma_srv)) && SUCCEEDED(native_device->CreateRenderTargetView(gamma_texture.get(), nullptr, &game_device_data.smaa_gamma_rtv)) && SUCCEEDED(native_device->CreateUnorderedAccessView(gamma_texture.get(), nullptr, &game_device_data.smaa_gamma_uav));
         if (!ok)
         {
            // Retried next frame (the size check above sees no texture)
            game_device_data.smaa_linear_texture.reset();
            return DrawOrDispatchOverrideType::None;
         }
         // Without it SMAA simply runs unpredicated.
         desc.Format = DXGI_FORMAT_R16_FLOAT;
         desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
         com_ptr<ID3D11Texture2D> predication_texture;
         if (FAILED(native_device->CreateTexture2D(&desc, nullptr, &predication_texture)) || FAILED(native_device->CreateUnorderedAccessView(predication_texture.get(), nullptr, &game_device_data.smaa_predication_uav)) || FAILED(native_device->CreateShaderResourceView(predication_texture.get(), nullptr, &game_device_data.smaa_predication_srv)))
         {
            game_device_data.smaa_predication_uav.reset();
            game_device_data.smaa_predication_srv.reset();
         }
      }

      // Predication depth, if captured and canvas sized. Anything else falls back to plain ULTRA.
      com_ptr<ID3D11ShaderResourceView> depth_srv;
      bool predication_available = smaa && game_device_data.smaa_predication_uav && HasShaders(device_data.native_compute_shaders, "P5S SMAA Predication CS"_h);
#if DEVELOPMENT
      predication_available = predication_available && g_smaa_predication;
#endif
      if (predication_available)
      {
         const std::lock_guard lock(game_device_data.smaa_depth_mutex);
         depth_srv = game_device_data.smaa_depth_srv;
      }
      if (depth_srv)
      {
         uint4 depth_size;
         DXGI_FORMAT unused_format;
         GetResourceInfo(depth_srv.get(), depth_size, unused_format);
         if (depth_size.x != canvas_desc.Width || depth_size.y != canvas_desc.Height)
            depth_srv.reset();
      }

      // The replaced composite reads the Luma cbuffers, which Core only binds after this callback returns. They stay bound for
      // SMAA and the finalize pass: the settings for the canvas size, the data for the predication scale (CustomData3, never 0
      // here, which also tells the composite to leave the dither to the end of the chain, so RCAS doesn't sharpen it).
      SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, reshade::api::shader_stage::vertex | reshade::api::shader_stage::pixel, LumaConstantBufferType::LumaSettings);
      SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, reshade::api::shader_stage::vertex | reshade::api::shader_stage::pixel, LumaConstantBufferType::LumaData, 0, 0, depth_srv ? 2.f : 1.f);
      *updated_cbuffers = true;
      original_draw_dispatch_func();
      native_device_context->CopyResource(game_device_data.smaa_linear_texture.get(), canvas_resource.get());

      {
         DrawStateStack<DrawStateStackType::Compute> compute_state;
         compute_state.Cache(native_device_context, device_data.uav_max_count);
         ID3D11UnorderedAccessView* const gamma_uav = game_device_data.smaa_gamma_uav.get();
         ID3D11ShaderResourceView* const linear_srv = game_device_data.smaa_linear_srv.get();
         native_device_context->CSSetUnorderedAccessViews(0, 1, &gamma_uav, nullptr);
         native_device_context->CSSetShaderResources(0, 1, &linear_srv);
         native_device_context->CSSetShader(device_data.native_compute_shaders.at("P5S SMAA Encode CS"_h).get(), nullptr, 0);
         native_device_context->Dispatch((canvas_desc.Width + 7) / 8, (canvas_desc.Height + 7) / 8, 1);
         if (depth_srv)
         {
            ID3D11UnorderedAccessView* const predication_uav = game_device_data.smaa_predication_uav.get();
            ID3D11ShaderResourceView* const raw_depth_srv = depth_srv.get();
            native_device_context->CSSetUnorderedAccessViews(0, 1, &predication_uav, nullptr);
            native_device_context->CSSetShaderResources(0, 1, &raw_depth_srv);
            native_device_context->CSSetShader(device_data.native_compute_shaders.at("P5S SMAA Predication CS"_h).get(), nullptr, 0);
            native_device_context->Dispatch((canvas_desc.Width + 7) / 8, (canvas_desc.Height + 7) / 8, 1);
         }
         compute_state.Restore(native_device_context);
      }

      // Without RCAS, SMAA writes (and dithers) the canvas directly: it only samples the linear copy, and the finalize pass would just decode
      const bool sharpen = cb_luma_global_settings.GameSettings.RCASSharpness > 0.f;
      if (smaa)
         DrawSMAA(native_device, native_device_context, device_data, sharpen ? game_device_data.smaa_gamma_rtv.get() : canvas_rtv.get(), game_device_data.smaa_linear_srv.get(), game_device_data.smaa_gamma_srv.get(), depth_srv ? game_device_data.smaa_predication_srv.get() : nullptr);

      DrawStateStack<DrawStateStackType::FullGraphics> finalize_state;
      finalize_state.Cache(native_device_context, device_data.uav_max_count);
      if (sharpen)
         DrawCustomPixelShader(native_device_context, device_data.default_depth_stencil_state.get(), device_data.default_blend_state.get(), nullptr, device_data.native_vertex_shaders.at("Copy VS"_h).get(), device_data.native_pixel_shaders.at("P5S SMAA Finalize PS"_h).get(), game_device_data.smaa_gamma_srv.get(), canvas_rtv.get(), canvas_desc.Width, canvas_desc.Height, false);
#if DEVELOPMENT
      // Calibration aid: SMAA's edges (red = horizontal, green = vertical) or the predication edge-ness (red) replace the frame
      ID3D11ShaderResourceView* const edges_srv = device_data.managed_resources.shader_resource_views["smaa_edge_detection"_h].get();
      ID3D11ShaderResourceView* const debug_srv = g_smaa_debug_view == 1 && smaa ? edges_srv : (g_smaa_debug_view == 2 && depth_srv ? game_device_data.smaa_predication_srv.get() : nullptr);
      if (debug_srv && HasShaders(device_data.native_pixel_shaders, "Copy PS"_h))
         DrawCustomPixelShader(native_device_context, device_data.default_depth_stencil_state.get(), device_data.default_blend_state.get(), nullptr, device_data.native_vertex_shaders.at("Copy VS"_h).get(), device_data.native_pixel_shaders.at("Copy PS"_h).get(), debug_srv, canvas_rtv.get(), canvas_desc.Width, canvas_desc.Height, false);
#endif
      finalize_state.Restore(native_device_context);

      game_device_data.scene_antialiased = true;
      return DrawOrDispatchOverrideType::Replaced;
   }

   // XeGTAO in place of the SSAO calculate draw: prefilter, main pass and two denoisers on the draw's own inputs (t0 half res
   // depth, t1 full res normals, its $Globals at b0 for this frame's projection and radius), then a copy into its render target.
   // Returns false, and the native draw runs, when an input, a shader or the scratch is missing.
   static bool RunXeGTAO(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, DeviceData& device_data)
   {
      // Held through the dispatches so a shader reload cannot release them mid-use.
      const std::shared_lock lock_shader_objects(s_mutex_shader_objects);
      const auto& shaders = device_data.native_compute_shaders;
      if (!HasShaders(shaders, "P5S XeGTAO Prefilter Depths CS"_h, "P5S XeGTAO Main Pass CS"_h, "P5S XeGTAO Denoise Pass 1 CS"_h, "P5S XeGTAO Denoise Pass 2 CS"_h))
         return false;
      com_ptr<ID3D11DeviceContext1> native_device_context1;
      if (FAILED(native_device_context->QueryInterface(&native_device_context1)))
         return false;

      com_ptr<ID3D11ShaderResourceView> depth_srv;
      com_ptr<ID3D11ShaderResourceView> normals_srv;
      com_ptr<ID3D11Buffer> globals_cb;
      UINT globals_first = 0, globals_count = 0;
      com_ptr<ID3D11RenderTargetView> target_rtv;
      com_ptr<ID3D11DepthStencilView> target_dsv;
      native_device_context->PSGetShaderResources(0, 1, &depth_srv);
      native_device_context->PSGetShaderResources(1, 1, &normals_srv);
      native_device_context1->PSGetConstantBuffers1(0, 1, &globals_cb, &globals_first, &globals_count);
      native_device_context->OMGetRenderTargets(1, &target_rtv, &target_dsv);
      if (!depth_srv || !normals_srv || !globals_cb || !target_rtv)
         return false;
      com_ptr<ID3D11Resource> target;
      target_rtv->GetResource(&target);
      uint4 target_size, depth_size, normals_size;
      DXGI_FORMAT target_format, unused_format;
      GetResourceInfo(target.get(), target_size, target_format);
      GetResourceInfo(depth_srv.get(), depth_size, unused_format);
      GetResourceInfo(normals_srv.get(), normals_size, unused_format);
      const uint32_t width = target_size.x;
      const uint32_t height = target_size.y;
      // The final denoiser's R8_UNORM output is copied into the target, so its format must match
      if (width == 0 || height == 0 || target_format != DXGI_FORMAT_R8_UNORM || depth_size.x != width || depth_size.y != height)
         return false;
      // Full res normals per target pixel (2, the SSAO is half res)
      const uint32_t normal_input_scale = (normals_size.x + width / 2) / width;
      if (normal_input_scale == 0 || (normals_size.y + height / 2) / height != normal_input_scale)
         return false;

      auto& game_device_data = GetGameDeviceData(device_data);
      const std::lock_guard lock(game_device_data.gtao_mutex);
      if (game_device_data.gtao_width != width || game_device_data.gtao_height != height)
      {
         game_device_data.ReleaseGTAOScratch();
         D3D11_TEXTURE2D_DESC desc = {};
         desc.Width = width;
         desc.Height = height;
         desc.MipLevels = 5; // XE_GTAO_DEPTH_MIP_LEVELS
         desc.ArraySize = 1;
         desc.Format = DXGI_FORMAT_R32_FLOAT;
         desc.SampleDesc.Count = 1;
         desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
         bool ok = SUCCEEDED(native_device->CreateTexture2D(&desc, nullptr, &game_device_data.gtao_depth_mips_texture)) && SUCCEEDED(native_device->CreateShaderResourceView(game_device_data.gtao_depth_mips_texture.get(), nullptr, &game_device_data.gtao_depth_mips_srv));
         D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc = {};
         uav_desc.Format = desc.Format;
         uav_desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
         for (UINT mip = 0; ok && mip < 5; mip++)
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
         desc.Format = DXGI_FORMAT_R8_UNORM;
         ok = ok && SUCCEEDED(native_device->CreateTexture2D(&desc, nullptr, &game_device_data.gtao_final_texture)) && SUCCEEDED(native_device->CreateUnorderedAccessView(game_device_data.gtao_final_texture.get(), nullptr, &game_device_data.gtao_final_uav));
         if (!ok)
            game_device_data.ReleaseGTAOScratch();
         game_device_data.gtao_width = width;
         game_device_data.gtao_height = height;
      }
      if (!game_device_data.gtao_final_uav)
         return false;

#if DEVELOPMENT
      const float debug_view = float(g_gtao_debug_view);
#else
      const float debug_view = 0.f;
#endif
      const float knobs[8] = {g_gtao_final_value_power, float(normal_input_scale), g_gtao_radius_override, debug_view, 1.f / float(width), 1.f / float(height), 0.f, 0.f};
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
      // The game may bind $Globals as a range of a bigger buffer; a zero count means a plain binding
      ID3D11Buffer* const cbs[] = {globals_cb.get(), game_device_data.gtao_knobs_cb.get()};
      if (globals_count != 0)
         native_device_context1->CSSetConstantBuffers1(0, 1, &cbs[0], &globals_first, &globals_count);
      else
         native_device_context->CSSetConstantBuffers(0, 1, &cbs[0]);
      native_device_context->CSSetConstantBuffers(gtao_knobs_cb_slot, 1, &cbs[1]);
      ID3D11SamplerState* const point_sampler = device_data.sampler_state_point.get();
      native_device_context->CSSetSamplers(0, 1, &point_sampler);

      // Each pass binds its destination UAV before its source SRVs: D3D11 otherwise nulls an SRV that still aliases the
      // previous pass's bound UAV.
      ID3D11ShaderResourceView* const null_srvs[2] = {};
      ID3D11UnorderedAccessView* const null_uavs[5] = {};
      const auto pass = [&](uint32_t shader_name_hash, UINT uav_count, ID3D11UnorderedAccessView* const* uavs, ID3D11ShaderResourceView* const(&srvs)[2], UINT groups_x, UINT groups_y)
      {
         native_device_context->CSSetShaderResources(0, 2, null_srvs);
         native_device_context->CSSetUnorderedAccessViews(0, uav_count, uavs, nullptr);
         native_device_context->CSSetShaderResources(0, 2, srvs);
         native_device_context->CSSetShader(shaders.at(shader_name_hash).get(), nullptr, 0);
         native_device_context->Dispatch(groups_x, groups_y, 1);
         native_device_context->CSSetUnorderedAccessViews(0, uav_count, null_uavs, nullptr);
      };
      ID3D11UnorderedAccessView* const mip_uavs[5] = {game_device_data.gtao_depth_mip_uavs[0].get(), game_device_data.gtao_depth_mip_uavs[1].get(), game_device_data.gtao_depth_mip_uavs[2].get(), game_device_data.gtao_depth_mip_uavs[3].get(), game_device_data.gtao_depth_mip_uavs[4].get()};
      ID3D11UnorderedAccessView* const working_uavs[2] = {game_device_data.gtao_working_uavs[0].get(), game_device_data.gtao_working_uavs[1].get()};
      ID3D11UnorderedAccessView* const final_uav = game_device_data.gtao_final_uav.get();
      pass("P5S XeGTAO Prefilter Depths CS"_h, 5, mip_uavs, {depth_srv.get(), nullptr}, (width + 15) / 16, (height + 15) / 16);
      pass("P5S XeGTAO Main Pass CS"_h, 1, &working_uavs[0], {game_device_data.gtao_depth_mips_srv.get(), normals_srv.get()}, (width + 7) / 8, (height + 7) / 8);
      pass("P5S XeGTAO Denoise Pass 1 CS"_h, 1, &working_uavs[1], {game_device_data.gtao_working_srvs[0].get(), nullptr}, (width + 15) / 16, (height + 7) / 8);
      pass("P5S XeGTAO Denoise Pass 2 CS"_h, 1, &final_uav, {game_device_data.gtao_working_srvs[1].get(), nullptr}, (width + 15) / 16, (height + 7) / 8);
      compute_state.Restore(native_device_context);

      // The target is still bound as the draw's render target, and a copy into an OM bound resource is a hazard the runtime
      // does not resolve: unbind around the copy, then give the game its binding back.
      native_device_context->OMSetRenderTargets(0, nullptr, nullptr);
      native_device_context->CopyResource(target.get(), game_device_data.gtao_final_texture.get());
      ID3D11RenderTargetView* const rtv = target_rtv.get();
      native_device_context->OMSetRenderTargets(1, &rtv, target_dsv.get());
      return true;
   }

   // For the game's "FinishCommandList" (a command list, then its deferred context) and "ExecuteCommandList" (the immediate context,
   // then the command list; before it executes). A command list finished after a split is the scene's remainder: when it's executed, the
   // split's first part is executed first, then DLSS/FSR writes the scene the remainder's post process reads.
   static void OnExecuteSecondaryCommandList(reshade::api::command_list* cmd_list, reshade::api::command_list* secondary_cmd_list)
   {
      DeviceData* const device_data = GetDeviceData(cmd_list->get_device());
      if (!device_data)
         return;
      auto& game_device_data = GetGameDeviceData(*device_data);
      auto* const native = reinterpret_cast<ID3D11DeviceChild*>(cmd_list->get_native());
      if (com_ptr<ID3D11CommandList> finished; SUCCEEDED(native->QueryInterface(&finished)))
      {
         const std::lock_guard lock(game_device_data.sr_mutex);
         if (game_device_data.sr_split.partial && secondary_cmd_list->get_native() == game_device_data.sr_split_context)
         {
            // ponytail: cap, in case the game drops a command list without executing it; a pending split per context if one ever lingers
            if (game_device_data.sr_pending_splits.size() >= 4)
               game_device_data.sr_pending_splits.clear();
            game_device_data.sr_split.remainder = finished;
            game_device_data.sr_pending_splits[reinterpret_cast<uint64_t>(finished.get())] = std::move(game_device_data.sr_split);
            game_device_data.sr_split = {};
            game_device_data.sr_split_context = 0;
         }
         return;
      }
      com_ptr<ID3D11DeviceContext> native_device_context;
      if (FAILED(native->QueryInterface(&native_device_context)))
         return;
      Persona5StrikersGameDeviceData::SRSplit split;
      {
         const std::lock_guard lock(game_device_data.sr_mutex);
         const auto it = game_device_data.sr_pending_splits.find(secondary_cmd_list->get_native());
         if (it == game_device_data.sr_pending_splits.end())
            return;
         split = std::move(it->second);
         game_device_data.sr_pending_splits.erase(it);
      }

      DrawStateStack<DrawStateStackType::FullGraphics> draw_state;
      DrawStateStack<DrawStateStackType::Compute> compute_state;
      draw_state.Cache(native_device_context.get(), device_data->uav_max_count);
      compute_state.Cache(native_device_context.get(), device_data->uav_max_count);
      // Always, even with the upscaler gone meanwhile: it's the start of the game's frame
      native_device_context->ExecuteCommandList(split.partial.get(), FALSE);

      D3D11_TEXTURE2D_DESC desc;
      split.source_color->GetDesc(&desc);
      if (IsSRActive(*device_data) && game_device_data.mv_texture)
      {
         // The output is written as a UAV. Upscaling: into the split's output, which the post process reads; else (DLAA) copied back into the scene.
         D3D11_TEXTURE2D_DESC output_desc = {};
         if (!split.output_color)
         {
            if (device_data->sr_output_color)
               device_data->sr_output_color->GetDesc(&output_desc);
            if (output_desc.Width != desc.Width || output_desc.Height != desc.Height)
            {
               device_data->sr_output_color.reset();
               output_desc = {desc.Width, desc.Height, 1, 1, DXGI_FORMAT_R16G16B16A16_FLOAT, {1, 0}, D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS};
               com_ptr<ID3D11Device> native_device;
               native_device_context->GetDevice(&native_device);
               native_device->CreateTexture2D(&output_desc, nullptr, &device_data->sr_output_color);
            }
         }
         ID3D11Texture2D* const output_color = split.output_color ? split.output_color.get() : device_data->sr_output_color.get();
         if (output_color)
            output_color->GetDesc(&output_desc);

         auto* const sr_instance_data = device_data->GetSRInstanceData();
         SR::SettingsData settings_data;
         settings_data.output_width = output_desc.Width;
         settings_data.output_height = output_desc.Height;
         settings_data.render_width = desc.Width;
         settings_data.render_height = desc.Height;
         settings_data.dynamic_resolution = false;
         settings_data.hdr = true;
         settings_data.inverted_depth = true;
         settings_data.mvs_jittered = false;
         // The motion vectors are UV deltas, previous minus current
         settings_data.mvs_x_scale = float(desc.Width);
         settings_data.mvs_y_scale = float(desc.Height);
         settings_data.auto_exposure = true;
         settings_data.render_preset = dlss_render_preset;
         sr_implementations[device_data->sr_type]->UpdateSettings(sr_instance_data, native_device_context.get(), settings_data);

         SR::SuperResolutionImpl::DrawData draw_data;
         draw_data.source_color = split.source_color.get();
         draw_data.output_color = output_color;
         draw_data.motion_vectors = game_device_data.mv_texture.get();
         draw_data.depth_buffer = split.depth.get();
         draw_data.render_width = desc.Width;
         draw_data.render_height = desc.Height;
         // As applied (pixels, +y down): found with the developer DLSS's jitter configs, the opposite shook upscaled frames
         draw_data.jitter_x = split.jitter[0];
         draw_data.jitter_y = split.jitter[1];
         draw_data.vert_fov = split.vertical_fov;
         // Meters, from the SSAO's depth linearization (reversed Z, near 32 cm, far 2400 m)
         draw_data.near_plane = 0.32f;
         draw_data.far_plane = 2400.f;
         draw_data.reset = device_data->force_reset_sr;
         if (output_color && sr_implementations[device_data->sr_type]->Draw(sr_instance_data, native_device_context.get(), draw_data))
         {
            if (!split.output_color)
            {
               native_device_context->CopySubresourceRegion(split.source_color.get(), 0, 0, 0, 0, output_color, 0, nullptr);
            }
            else
            {
               // Also scaled back down into the scene: the post passes that read it at the render resolution (DOF, bloom, exposure) would
               // otherwise see the raw jittered frame, and the DOF merge would blend a shaking blur into the output (blurry and shaking)
               com_ptr<ID3D11Device> native_device;
               native_device_context->GetDevice(&native_device);
               com_ptr<ID3D11ShaderResourceView> output_srv;
               com_ptr<ID3D11RenderTargetView> scene_rtv;
               const D3D11_RENDER_TARGET_VIEW_DESC scene_rtv_desc = {DXGI_FORMAT_R16G16B16A16_FLOAT, D3D11_RTV_DIMENSION_TEXTURE2D};
               const std::shared_lock lock_shader_objects(s_mutex_shader_objects);
               if (HasShaders(device_data->native_vertex_shaders, "P5S Downsample VS"_h) && HasShaders(device_data->native_pixel_shaders, "P5S Downsample PS"_h) && SUCCEEDED(native_device->CreateShaderResourceView(output_color, nullptr, &output_srv)) && SUCCEEDED(native_device->CreateRenderTargetView(split.source_color.get(), &scene_rtv_desc, &scene_rtv)))
                  DrawCustomPixelShader(native_device_context.get(), device_data->default_depth_stencil_state.get(), device_data->default_blend_state.get(), device_data->sampler_state_linear.get(), device_data->native_vertex_shaders.at("P5S Downsample VS"_h).get(), device_data->native_pixel_shaders.at("P5S Downsample PS"_h).get(), output_srv.get(), scene_rtv.get(), desc.Width, desc.Height);
            }
            device_data->has_drawn_sr = true;
            game_device_data.sr_draws++;
         }
         else
         {
            // Back to SMAA until the upscaler is picked again
            device_data->sr_suppressed = true;
         }
      }
      draw_state.Restore(native_device_context.get());
      compute_state.Restore(native_device_context.get());
   }

   // Byte offset of "fClstScl" in a pixel shader's $Globals (b0), UINT_MAX if it has none, by original hash
   static UINT GetClusterScaleOffset(DeviceData& device_data, uint32_t hash, reshade::api::pipeline pipeline)
   {
      auto& game_device_data = GetGameDeviceData(device_data);
      {
         const std::lock_guard lock(game_device_data.layer_mutex);
         if (const auto it = game_device_data.layer_cluster_scale_offsets.find(hash); it != game_device_data.layer_cluster_scale_offsets.end())
            return it->second;
      }
      UINT offset = UINT_MAX;
      {
         const std::shared_lock lock(s_mutex_generic);
         if (const auto it = device_data.pipeline_cache_by_pipeline_handle.find(pipeline.handle); it != device_data.pipeline_cache_by_pipeline_handle.end() && it->second->subobjects_cache)
         {
            const auto* desc = static_cast<const reshade::api::shader_desc*>(it->second->subobjects_cache[0].data);
            com_ptr<ID3D11ShaderReflection> reflection;
            D3D11_SHADER_INPUT_BIND_DESC bind;
            D3D11_SHADER_VARIABLE_DESC variable;
            // Missing names give dummy reflection objects whose GetDesc fails
            if (Shader::d3d_reflect && SUCCEEDED(Shader::d3d_reflect(desc->code, desc->code_size, IID_PPV_ARGS(&reflection))) && SUCCEEDED(reflection->GetResourceBindingDescByName("$Globals", &bind)) && bind.BindPoint == 0 && SUCCEEDED(reflection->GetConstantBufferByName("$Globals")->GetVariableByName("fClstScl")->GetDesc(&variable)) && variable.Size == sizeof(float))
               offset = variable.StartOffset;
         }
      }
      const std::lock_guard lock(game_device_data.layer_mutex);
      return game_device_data.layer_cluster_scale_offsets.try_emplace(hash, offset).first->second;
   }

   // Upscaling: the 3D layers the game draws outside the scene frame (the main menu and pause screen characters, their outlines and
   // translucents) go into output sized targets through a render resolution viewport, then a stretch (0x99A76DC2) scales that corner
   // over the whole target, so DLSS/FSR never sees them. Their draws get the whole target instead, and the stretch copies it 1:1. What
   // depends on the render resolution is scaled back: the quads' texture coordinates (from their vertices), and the light cluster index
   // of the lit pixel shaders (pixel position * "fClstScl" / 64). False if the draw isn't one (it then goes ahead untouched).
   static bool DrawLayerAtOutputResolution(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, CommandListData& cmd_list_data, DeviceData& device_data, const ShaderHashesList<OneShaderPerPipeline>& original_shader_hashes, const std::function<void()>& draw)
   {
      auto& game_device_data = GetGameDeviceData(device_data);
      const bool stretch = original_shader_hashes.Contains(layer_stretch_hash, reshade::api::shader_stage::pixel) && original_shader_hashes.Contains(quad_vertex_shader_hash, reshade::api::shader_stage::vertex);
      D3D11_VIEWPORT viewport = {};
      UINT viewports = 1;
      native_device_context->RSGetViewports(&viewports, &viewport);
      const uint2 output_size = {uint32_t(device_data.output_resolution.x + 0.5f), uint32_t(device_data.output_resolution.y + 0.5f)};
      // Most draws (the scene's included) stop here
      if (!stretch && (viewports == 0 || viewport.TopLeftX != 0.f || viewport.TopLeftY != 0.f || viewport.Width >= float(output_size.x)))
         return false;
      D3D11_TEXTURE2D_DESC target_desc = {};
      com_ptr<ID3D11Resource> target;
      float scale[2];
      if (stretch)
      {
         // From a target the layer drew whole
         com_ptr<ID3D11ShaderResourceView> srv;
         native_device_context->PSGetShaderResources(0, 1, &srv);
         com_ptr<ID3D11Resource> source;
         if (srv)
            srv->GetResource(&source);
         const std::lock_guard lock(game_device_data.layer_mutex);
         const auto it = game_device_data.layer_frames.find(native_device_context);
         if (!source || it == game_device_data.layer_frames.end() || std::ranges::find(it->second.targets, source.get()) == it->second.targets.end())
            return false;
         std::copy_n(it->second.scale, 2, scale);
         game_device_data.layer_frames.erase(it);
      }
      else
      {
         com_ptr<ID3D11RenderTargetView> rtv;
         com_ptr<ID3D11DepthStencilView> dsv;
         native_device_context->OMGetRenderTargets(1, &rtv, &dsv);
         if (rtv)
            rtv->GetResource(&target);
         if (com_ptr<ID3D11Texture2D> texture; target && SUCCEEDED(target->QueryInterface(&texture)))
            texture->GetDesc(&target_desc);
         // An output sized off-screen target, with the viewport at its top left corner smaller in both axes by the same ratio (not a panel)
         if (target_desc.Width != output_size.x || target_desc.Height != output_size.y || IsBackBuffer(&device_data, target.get()))
            return false;
         scale[0] = viewport.Width / float(target_desc.Width);
         scale[1] = viewport.Height / float(target_desc.Height);
         if (scale[0] <= 0.f || std::abs(scale[0] - scale[1]) > 0.01f)
            return false;
         if (dsv)
         {
            com_ptr<ID3D11Resource> depth;
            dsv->GetResource(&depth);
            uint4 depth_size;
            DXGI_FORMAT depth_format;
            GetResourceInfo(depth.get(), depth_size, depth_format);
            if (depth_size.x != target_desc.Width || depth_size.y != target_desc.Height)
               return false;
         }
      }

      // All or nothing: without it, the layer's quads would read a corner of what it drew
      com_ptr<ID3D11VertexShader> quad_vertex_shader;
      {
         const std::shared_lock lock_shader_objects(s_mutex_shader_objects);
         if (!HasShaders(device_data.native_vertex_shaders, "P5S Layer Quad VS"_h))
            return false;
         if (original_shader_hashes.Contains(quad_vertex_shader_hash, reshade::api::shader_stage::vertex))
            quad_vertex_shader = device_data.native_vertex_shaders.at("P5S Layer Quad VS"_h);
      }
      com_ptr<ID3D11Buffer> uv_scale_buffer;
      if (quad_vertex_shader)
      {
         const std::lock_guard lock(game_device_data.layer_mutex);
         if (!game_device_data.layer_uv_scale_buffer || game_device_data.layer_uv_scale[0] != scale[0] || game_device_data.layer_uv_scale[1] != scale[1])
         {
            const float uv_scale[4] = {1.f / scale[0], 1.f / scale[1], 0.f, 0.f};
            const D3D11_BUFFER_DESC desc = {sizeof(uv_scale), D3D11_USAGE_IMMUTABLE, D3D11_BIND_CONSTANT_BUFFER};
            const D3D11_SUBRESOURCE_DATA data = {uv_scale};
            game_device_data.layer_uv_scale_buffer.reset();
            if (FAILED(native_device->CreateBuffer(&desc, &data, &game_device_data.layer_uv_scale_buffer)))
               return false;
            std::copy_n(scale, 2, game_device_data.layer_uv_scale);
         }
         uv_scale_buffer = game_device_data.layer_uv_scale_buffer;
      }

      // The lit pixel shaders' $Globals with the cluster scale matching the pixel positions. Until the buffer has a CPU copy (its first
      // draw), the draw keeps the game's, so its point lights come from the wrong clusters for a frame.
      com_ptr<ID3D11Buffer> original_globals;
      com_ptr<ID3D11Buffer> patched_globals;
      const UINT cluster_scale_offset = stretch ? UINT_MAX : GetClusterScaleOffset(device_data, original_shader_hashes.pixel_shaders[0], cmd_list_data.pipeline_state_original_pixel_shader);
      if (cluster_scale_offset != UINT_MAX)
      {
         native_device_context->PSGetConstantBuffers(0, 1, &original_globals);
         std::vector<uint8_t> globals_data;
         if (original_globals)
         {
            const std::lock_guard lock(game_device_data.mv_globals_mutex);
            const uint64_t handle = reinterpret_cast<uint64_t>(original_globals.get());
            game_device_data.mv_globals_buffers.insert(handle);
            if (const auto copy = game_device_data.mv_globals_copies.find(handle); copy != game_device_data.mv_globals_copies.end())
               globals_data = copy->second;
         }
         if (cluster_scale_offset + sizeof(float) <= globals_data.size())
         {
            float cluster_scale;
            std::memcpy(&cluster_scale, globals_data.data() + cluster_scale_offset, sizeof(float));
            cluster_scale *= scale[0];
            std::memcpy(globals_data.data() + cluster_scale_offset, &cluster_scale, sizeof(float));
            {
               const std::lock_guard lock(game_device_data.layer_mutex);
               com_ptr<ID3D11Buffer>& upload = game_device_data.layer_globals_buffers[UINT(globals_data.size())];
               if (!upload)
               {
                  const D3D11_BUFFER_DESC desc = {UINT(globals_data.size()), D3D11_USAGE_DYNAMIC, D3D11_BIND_CONSTANT_BUFFER, D3D11_CPU_ACCESS_WRITE};
                  native_device->CreateBuffer(&desc, nullptr, &upload);
               }
               patched_globals = upload;
            }
            D3D11_MAPPED_SUBRESOURCE mapped;
            if (patched_globals && SUCCEEDED(native_device_context->Map(patched_globals.get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
            {
               std::memcpy(mapped.pData, globals_data.data(), globals_data.size());
               native_device_context->Unmap(patched_globals.get(), 0);
            }
            else
            {
               patched_globals.reset();
            }
         }
         else
         {
            game_device_data.layer_uncopied_draws++;
         }
      }

      if (!stretch)
      {
         const std::lock_guard lock(game_device_data.layer_mutex);
         auto& frame = game_device_data.layer_frames[native_device_context];
         if (std::ranges::find(frame.targets, target.get()) == frame.targets.end())
            frame.targets.push_back(target.get());
         std::copy_n(scale, 2, frame.scale);
      }

      // Set directly, so Core's tracking of the bound state never sees them: the game's are put back after
      D3D11_RECT scissor = {};
      UINT scissors = 1;
      native_device_context->RSGetScissorRects(&scissors, &scissor);
      com_ptr<ID3D11VertexShader> original_vertex_shader;
      com_ptr<ID3D11Buffer> original_uv_scale_buffer;
      if (!stretch)
      {
         const D3D11_VIEWPORT full_viewport = {0.f, 0.f, float(target_desc.Width), float(target_desc.Height), viewport.MinDepth, viewport.MaxDepth};
         const D3D11_RECT full_scissor = {0, 0, LONG(target_desc.Width), LONG(target_desc.Height)};
         native_device_context->RSSetViewports(1, &full_viewport);
         native_device_context->RSSetScissorRects(1, &full_scissor);
      }
      if (quad_vertex_shader)
      {
         native_device_context->VSGetShader(&original_vertex_shader, nullptr, nullptr);
         native_device_context->VSGetConstantBuffers(layer_uv_scale_cb_slot, 1, &original_uv_scale_buffer);
         ID3D11Buffer* const buffer = uv_scale_buffer.get();
         native_device_context->VSSetConstantBuffers(layer_uv_scale_cb_slot, 1, &buffer);
         native_device_context->VSSetShader(quad_vertex_shader.get(), nullptr, 0);
      }
      if (patched_globals)
      {
         ID3D11Buffer* const buffer = patched_globals.get();
         native_device_context->PSSetConstantBuffers(0, 1, &buffer);
      }
      draw();
      if (!stretch)
      {
         native_device_context->RSSetViewports(1, &viewport);
         native_device_context->RSSetScissorRects(scissors, &scissor);
      }
      if (quad_vertex_shader)
      {
         ID3D11Buffer* const buffer = original_uv_scale_buffer.get();
         native_device_context->VSSetConstantBuffers(layer_uv_scale_cb_slot, 1, &buffer);
         native_device_context->VSSetShader(original_vertex_shader.get(), nullptr, 0);
      }
      if (patched_globals)
      {
         ID3D11Buffer* const buffer = original_globals.get();
         native_device_context->PSSetConstantBuffers(0, 1, &buffer);
      }
      game_device_data.layer_draws++;
      return true;
   }

   DrawOrDispatchOverrideType OnDrawOrDispatch(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, CommandListData& cmd_list_data, DeviceData& device_data, reshade::api::shader_stage stages, const ShaderHashesList<OneShaderPerPipeline>& original_shader_hashes, bool is_custom_pass, bool& updated_cbuffers, std::function<void()>* original_draw_dispatch_func) override
   {
      auto& game_device_data = GetGameDeviceData(device_data);

      if (original_shader_hashes.Contains(ssao_depth_downsample_hash, reshade::api::shader_stage::pixel))
      {
         com_ptr<ID3D11ShaderResourceView> depth_srv;
         native_device_context->PSGetShaderResources(0, 1, &depth_srv);
         D3D11_SHADER_RESOURCE_VIEW_DESC depth_srv_desc;
         if (depth_srv)
            depth_srv->GetDesc(&depth_srv_desc);
         if (depth_srv && depth_srv_desc.Format == DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS && depth_srv_desc.ViewDimension == D3D11_SRV_DIMENSION_TEXTURE2D)
         {
            const std::lock_guard lock(game_device_data.smaa_depth_mutex);
            game_device_data.smaa_depth_srv = depth_srv;
         }
      }

#if DEVELOPMENT
      if (game_device_data.mv_probe_logging && (stages & reshade::api::shader_stage::vertex) == reshade::api::shader_stage::vertex)
         LogMVProbeDraw(native_device_context, &game_device_data, original_shader_hashes);
#endif

      if (AreMotionVectorsEnabled(device_data) && (stages & reshade::api::shader_stage::vertex) == reshade::api::shader_stage::vertex && original_draw_dispatch_func && *original_draw_dispatch_func)
      {
         if (original_shader_hashes.Contains(post_process_start_shader_hashes))
         {
            if (native_device_context == game_device_data.mv_scene_context && !game_device_data.mv_frame_ended)
            {
               game_device_data.mv_frame_ended = true;

               // The G-buffer's depth, if it can be read (a shader resource), otherwise the copy the game makes before post
               com_ptr<ID3D11ShaderResourceView> depth_srv;
               D3D11_TEXTURE2D_DESC depth_desc = {};
               if (com_ptr<ID3D11Texture2D> depth_texture; game_device_data.mv_scene_depth && SUCCEEDED(game_device_data.mv_scene_depth->QueryInterface(&depth_texture)))
                  depth_texture->GetDesc(&depth_desc);
               if ((depth_desc.BindFlags & D3D11_BIND_SHADER_RESOURCE) != 0)
               {
                  com_ptr<ID3D11Resource> view_resource;
                  if (game_device_data.mv_scene_depth_srv)
                     game_device_data.mv_scene_depth_srv->GetResource(&view_resource);
                  if (view_resource != game_device_data.mv_scene_depth)
                  {
                     game_device_data.mv_scene_depth_srv.reset();
                     // The depth channel of the depth formats
                     const DXGI_FORMAT format = depth_desc.Format == DXGI_FORMAT_R32G8X24_TYPELESS ? DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS : (depth_desc.Format == DXGI_FORMAT_R32_TYPELESS ? DXGI_FORMAT_R32_FLOAT : (depth_desc.Format == DXGI_FORMAT_R24G8_TYPELESS ? DXGI_FORMAT_R24_UNORM_X8_TYPELESS : depth_desc.Format));
                     D3D11_SHADER_RESOURCE_VIEW_DESC view_desc = {format, D3D11_SRV_DIMENSION_TEXTURE2D};
                     view_desc.Texture2D.MipLevels = 1;
                     native_device->CreateShaderResourceView(game_device_data.mv_scene_depth.get(), &view_desc, &game_device_data.mv_scene_depth_srv);
                  }
                  depth_srv = game_device_data.mv_scene_depth_srv;
               }
               else
               {
                  const std::lock_guard lock(game_device_data.smaa_depth_mutex);
                  depth_srv = game_device_data.smaa_depth_srv;
               }
               game_device_data.mv_frame_depth.reset();
               if (depth_srv)
                  depth_srv->GetResource(&game_device_data.mv_frame_depth);

               // Camera motion where no patched draw wrote
               if (game_device_data.mv_fill_pending)
               {
                  game_device_data.mv_fill_pending = false;
                  // Current clip space to the previous frame's, for row vectors: inverse(current) * previous. In double, the world
                  // translation (centimetres) cancels out between the two.
                  double reprojection[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
                  if (game_device_data.mv_view_projection_valid && game_device_data.mv_previous_view_projection_valid)
                  {
                     float current_float[16], previous_float[16];
                     std::memcpy(current_float, game_device_data.mv_view_projection.data(), sizeof(current_float));
                     std::memcpy(previous_float, game_device_data.mv_previous_view_projection.data(), sizeof(previous_float));
                     // Gauss-Jordan with partial pivoting on [current | identity]
                     double a[4][8] = {};
                     for (int row = 0; row < 4; row++)
                     {
                        for (int column = 0; column < 4; column++)
                           a[row][column] = current_float[row * 4 + column];
                        a[row][4 + row] = 1.0;
                     }
                     bool invertible = true;
                     for (int column = 0; column < 4 && invertible; column++)
                     {
                        int pivot = column;
                        for (int row = column + 1; row < 4; row++)
                        {
                           if (std::abs(a[row][column]) > std::abs(a[pivot][column]))
                              pivot = row;
                        }
                        invertible = std::abs(a[pivot][column]) > 1e-30;
                        std::swap(a[column], a[pivot]);
                        const double scale = invertible ? 1.0 / a[column][column] : 0.0;
                        for (double& value : a[column])
                           value *= scale;
                        for (int row = 0; row < 4; row++)
                        {
                           const double factor = row == column ? 0.0 : a[row][column];
                           for (int i = 0; i < 8; i++)
                              a[row][i] -= factor * a[column][i];
                        }
                     }
                     for (int row = 0; row < 4 && invertible; row++)
                     {
                        for (int column = 0; column < 4; column++)
                        {
                           reprojection[row * 4 + column] = 0.0;
                           for (int k = 0; k < 4; k++)
                              reprojection[row * 4 + column] += a[row][4 + k] * double(previous_float[k * 4 + column]);
                        }
                     }
                  }
                  D3D11_TEXTURE2D_DESC mv_desc;
                  game_device_data.mv_texture->GetDesc(&mv_desc);
                  float constants[20] = {};
                  for (int i = 0; i < 16; i++)
                     constants[i] = float(reprojection[i]);
                  constants[16] = game_device_data.mv_jitter[0] * 2.f / float(mv_desc.Width);
                  constants[17] = game_device_data.mv_jitter[1] * -2.f / float(mv_desc.Height);
                  if (!game_device_data.mv_fill_buffer)
                  {
                     const D3D11_BUFFER_DESC desc = {sizeof(constants), D3D11_USAGE_DYNAMIC, D3D11_BIND_CONSTANT_BUFFER, D3D11_CPU_ACCESS_WRITE};
                     native_device->CreateBuffer(&desc, nullptr, &game_device_data.mv_fill_buffer);
                  }
                  D3D11_MAPPED_SUBRESOURCE mapped;
                  const std::shared_lock lock_shader_objects(s_mutex_shader_objects);
                  // ponytail: a shader reload between the clear and here (DEV) leaves the FLT_MAX marker for a frame
                  if (depth_srv && game_device_data.mv_fill_buffer && HasShaders(device_data.native_compute_shaders, "P5S Motion Vector Fill CS"_h) && SUCCEEDED(native_device_context->Map(game_device_data.mv_fill_buffer.get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
                  {
                     std::memcpy(mapped.pData, constants, sizeof(constants));
                     native_device_context->Unmap(game_device_data.mv_fill_buffer.get(), 0);
                     DrawStateStack<DrawStateStackType::FullGraphics> graphics_state;
                     DrawStateStack<DrawStateStackType::Compute> compute_state;
                     graphics_state.Cache(native_device_context, device_data.uav_max_count);
                     compute_state.Cache(native_device_context, device_data.uav_max_count);
                     // The depth may be bound as the depth target, and the motion vectors as a render target
                     native_device_context->OMSetRenderTargets(0, nullptr, nullptr);
                     ID3D11Buffer* const buffer = game_device_data.mv_fill_buffer.get();
                     ID3D11ShaderResourceView* const srv = depth_srv.get();
                     ID3D11UnorderedAccessView* const uav = game_device_data.mv_uav.get();
                     native_device_context->CSSetConstantBuffers(0, 1, &buffer);
                     native_device_context->CSSetShaderResources(0, 1, &srv);
                     native_device_context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);
                     native_device_context->CSSetShader(device_data.native_compute_shaders.at("P5S Motion Vector Fill CS"_h).get(), nullptr, 0);
                     native_device_context->Dispatch((mv_desc.Width + 7) / 8, (mv_desc.Height + 7) / 8, 1);
                     compute_state.Restore(native_device_context);
                     graphics_state.Restore(native_device_context);
                     game_device_data.mv_fills++;
                  }
               }
            }
         }
         else
         {
            com_ptr<ID3D11RenderTargetView> rtvs[8];
            com_ptr<ID3D11DepthStencilView> dsv;
            native_device_context->OMGetRenderTargets(8, &rtvs[0], &dsv);
            // The G-buffer (5 targets), or an opaque forward draw of a mesh into the scene (outlines, sky: 1 target, depth written)
            const bool gbuffer = std::all_of(rtvs, rtvs + 5, [](const auto& rtv)
               { return rtv.get() != nullptr; });
            D3D11_DEPTH_STENCIL_DESC depth_desc = {};
            com_ptr<ID3D11Buffer> vertex_buffer;
            if (!gbuffer && dsv)
            {
               com_ptr<ID3D11DepthStencilState> depth_stencil_state;
               UINT stencil_ref;
               native_device_context->OMGetDepthStencilState(&depth_stencil_state, &stencil_ref);
               if (depth_stencil_state)
                  depth_stencil_state->GetDesc(&depth_desc);
               UINT stride, offset;
               native_device_context->IAGetVertexBuffers(0, 1, &vertex_buffer, &stride, &offset);
            }
            const bool depth_write = depth_desc.DepthEnable && depth_desc.DepthWriteMask == D3D11_DEPTH_WRITE_MASK_ALL;
            const bool forward = !gbuffer && rtvs[0] && !rtvs[1] && depth_write && vertex_buffer;
            if (gbuffer || forward)
            {
               ID3D11RenderTargetView* const bound_rtvs[8] = {rtvs[0].get(), rtvs[1].get(), rtvs[2].get(), rtvs[3].get(), rtvs[4].get(), rtvs[5].get(), rtvs[6].get(), rtvs[7].get()};
               if (DrawWithMotionVectors(native_device, native_device_context, cmd_list_data, device_data, original_shader_hashes, bound_rtvs, dsv.get(), gbuffer, *original_draw_dispatch_func))
                  return DrawOrDispatchOverrideType::Replaced;
            }
            // Any other mesh depth tested against the scene: the depth prepass (no targets), or geometry that only tests depth
            const bool depth_prepass = !rtvs[0] && depth_write;
            if (!gbuffer && depth_desc.DepthEnable && vertex_buffer && DrawWithJitter(native_device, native_device_context, cmd_list_data, device_data, original_shader_hashes, dsv.get(), depth_prepass, *original_draw_dispatch_func))
               return DrawOrDispatchOverrideType::Replaced;
         }
      }

      if (original_shader_hashes.Contains(layer_stretch_hash, reshade::api::shader_stage::pixel))
         game_device_data.layer_drawn = true;
      if (IsSRActive(device_data) && (stages & reshade::api::shader_stage::vertex) == reshade::api::shader_stage::vertex && original_draw_dispatch_func && *original_draw_dispatch_func && DrawLayerAtOutputResolution(native_device, native_device_context, cmd_list_data, device_data, original_shader_hashes, *original_draw_dispatch_func))
         return DrawOrDispatchOverrideType::Replaced;

      if (g_gtao_enable && original_shader_hashes.Contains(ssao_hash, reshade::api::shader_stage::pixel))
         return RunXeGTAO(native_device, native_device_context, device_data) ? DrawOrDispatchOverrideType::Replaced : DrawOrDispatchOverrideType::None;

      // DLSS/FSR before the scene's first post process pass: split its command list here, the upscaler runs between the two parts
      if (game_device_data.sr_split_ready && native_device_context == game_device_data.mv_scene_context && IsSRActive(device_data) && original_shader_hashes.Contains(post_process_start_shader_hashes))
      {
         game_device_data.sr_split_ready = false;
         com_ptr<ID3D11ShaderResourceView> scene_srv;
         native_device_context->PSGetShaderResources(0, 1, &scene_srv);
         com_ptr<ID3D11Resource> scene;
         if (scene_srv)
            scene_srv->GetResource(&scene);
         Persona5StrikersGameDeviceData::SRSplit split;
         D3D11_TEXTURE2D_DESC scene_desc = {};
         if (scene && SUCCEEDED(scene->QueryInterface(&split.source_color)))
            split.source_color->GetDesc(&scene_desc);
         uint4 mv_size = {};
         DXGI_FORMAT mv_format;
         if (game_device_data.mv_texture)
            GetResourceInfo(game_device_data.mv_texture.get(), mv_size, mv_format);
         split.depth = game_device_data.mv_frame_depth;
         split.jitter = game_device_data.mv_jitter;
         // "mW2P" is row major and multiplies row vectors: its column 1 (xyz) is the view's up axis times 1 / tan(fov / 2)
         if (game_device_data.mv_view_projection_valid)
         {
            float view_projection[16];
            std::memcpy(view_projection, game_device_data.mv_view_projection.data(), sizeof(view_projection));
            split.vertical_fov = 2.f * std::atan(1.f / std::sqrt(view_projection[1] * view_projection[1] + view_projection[5] * view_projection[5] + view_projection[9] * view_projection[9]));
         }
         // Upscaling when the game renders below the output resolution (its render scale option): the output and the canvas at the output resolution
         const uint2 output_size = {uint32_t(device_data.output_resolution.x + 0.5f), uint32_t(device_data.output_resolution.y + 0.5f)};
         if (scene_desc.Width < output_size.x || scene_desc.Height < output_size.y)
         {
            D3D11_TEXTURE2D_DESC output_desc = {};
            if (game_device_data.sr_upscaled_output)
               game_device_data.sr_upscaled_output->GetDesc(&output_desc);
            if (output_desc.Width != output_size.x || output_desc.Height != output_size.y)
            {
               game_device_data.sr_upscaled_output.reset();
               game_device_data.sr_upscaled_output_rtv.reset();
               game_device_data.sr_upscaled_output_srv.reset();
               game_device_data.sr_upscaled_canvas.reset();
               game_device_data.sr_upscaled_canvas_rtv.reset();
               game_device_data.sr_upscaled_canvas_srv.reset();
               output_desc = {output_size.x, output_size.y, 1, 1, DXGI_FORMAT_R16G16B16A16_FLOAT, {1, 0}, D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET | D3D11_BIND_UNORDERED_ACCESS};
               bool created = SUCCEEDED(native_device->CreateTexture2D(&output_desc, nullptr, &game_device_data.sr_upscaled_output)) && SUCCEEDED(native_device->CreateRenderTargetView(game_device_data.sr_upscaled_output.get(), nullptr, &game_device_data.sr_upscaled_output_rtv)) && SUCCEEDED(native_device->CreateShaderResourceView(game_device_data.sr_upscaled_output.get(), nullptr, &game_device_data.sr_upscaled_output_srv));
               output_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
               created = created && SUCCEEDED(native_device->CreateTexture2D(&output_desc, nullptr, &game_device_data.sr_upscaled_canvas)) && SUCCEEDED(native_device->CreateRenderTargetView(game_device_data.sr_upscaled_canvas.get(), nullptr, &game_device_data.sr_upscaled_canvas_rtv)) && SUCCEEDED(native_device->CreateShaderResourceView(game_device_data.sr_upscaled_canvas.get(), nullptr, &game_device_data.sr_upscaled_canvas_srv));
               // Retried next frame; meanwhile the upscaler runs at the render resolution, which the game stretches
               if (!created)
                  game_device_data.sr_upscaled_canvas_srv.reset();
#if DEVELOPMENT
               reshade::log::message(created ? reshade::log::level::info : reshade::log::level::warning, std::format("[P5S SR] upscaling {}x{} -> {}x{}{}", scene_desc.Width, scene_desc.Height, output_size.x, output_size.y, created ? "" : " failed").c_str());
#endif
            }
            if (game_device_data.sr_upscaled_canvas_srv)
               split.output_color = game_device_data.sr_upscaled_output;
         }
         // The upgraded scene, the motion vectors' size, not multisampled
         if (split.depth && native_device_context->GetType() == D3D11_DEVICE_CONTEXT_DEFERRED && scene_desc.Width == mv_size.x && scene_desc.Height == mv_size.y && scene_desc.SampleDesc.Count == 1 && (scene_desc.Format == DXGI_FORMAT_R16G16B16A16_FLOAT || scene_desc.Format == DXGI_FORMAT_R16G16B16A16_TYPELESS) && SUCCEEDED(native_device_context->FinishCommandList(TRUE, &split.partial)))
         {
            const std::lock_guard lock(game_device_data.sr_mutex);
            // The game doesn't know its command list restarted: its next "D3D11_MAP_WRITE_NO_OVERWRITE" map would fail without a discard
            // first (the dialogue text went missing). Data it appended before the split isn't carried over, like in the sibling mods.
            for (const auto& buffer : game_device_data.sr_no_overwrite_buffers)
            {
               D3D11_MAPPED_SUBRESOURCE mapped;
               if (SUCCEEDED(native_device_context->Map(buffer.get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
                  native_device_context->Unmap(buffer.get(), 0);
            }
            game_device_data.sr_upscaling_context = split.output_color ? native_device_context : nullptr;
            game_device_data.sr_render_height = scene_desc.Height;
            device_data.render_resolution = {float(scene_desc.Width), float(scene_desc.Height)}; // Shown by Core
            game_device_data.sr_output_height = split.output_color ? output_size.y : scene_desc.Height;
            game_device_data.sr_split = std::move(split);
            game_device_data.sr_split_context = reinterpret_cast<uint64_t>(native_device_context);
            game_device_data.scene_antialiased = true;
            game_device_data.sr_splits++;
         }
      }

      // Upscaled: the post passes that blend into the scene also blend into the upscaler's output, at the output resolution. The render
      // resolution scene stays vanilla for the passes that read it after them (the bloom prefilter, the exposure histogram by pixel).
      if (native_device_context == game_device_data.sr_upscaling_context && original_shader_hashes.Contains(scene_post_writer_shader_hashes) && original_draw_dispatch_func && *original_draw_dispatch_func)
      {
         // The replaced bloom add reads its intensity
         SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, reshade::api::shader_stage::pixel, LumaConstantBufferType::LumaSettings);
         updated_cbuffers = true;
         (*original_draw_dispatch_func)();
         DrawStateStack<DrawStateStackType::FullGraphics> state;
         state.Cache(native_device_context, device_data.uav_max_count);
         D3D11_TEXTURE2D_DESC desc;
         game_device_data.sr_upscaled_output->GetDesc(&desc);
         // The game scissors its passes to the render resolution
         const D3D11_VIEWPORT viewport = {0.f, 0.f, float(desc.Width), float(desc.Height), 0.f, 1.f};
         const D3D11_RECT scissor = {0, 0, LONG(desc.Width), LONG(desc.Height)};
         ID3D11RenderTargetView* const rtv = game_device_data.sr_upscaled_output_rtv.get();
         native_device_context->OMSetRenderTargets(1, &rtv, nullptr);
         native_device_context->RSSetViewports(1, &viewport);
         native_device_context->RSSetScissorRects(1, &scissor);
         (*original_draw_dispatch_func)();
         state.Restore(native_device_context);
         return DrawOrDispatchOverrideType::Replaced;
      }

      if (original_shader_hashes.Contains(composite_hash, reshade::api::shader_stage::pixel))
      {
         device_data.has_drawn_main_post_processing = true;
         if (!original_draw_dispatch_func || !*original_draw_dispatch_func)
            return DrawOrDispatchOverrideType::None;
         // Below render scale 1 the composite draws into its own target, which the game then stretches onto the swapchain
         {
            com_ptr<ID3D11RenderTargetView> rtv;
            native_device_context->OMGetRenderTargets(1, &rtv, nullptr);
            com_ptr<ID3D11Resource> target;
            if (rtv)
               rtv->GetResource(&target);
            game_device_data.composite_target.reset();
            if (target && !IsBackBuffer(&device_data, target.get()))
            {
               game_device_data.composite_target = target;
               uint4 target_size;
               DXGI_FORMAT target_format;
               GetResourceInfo(target.get(), target_size, target_format);
               if (target_size.x < uint32_t(device_data.output_resolution.x + 0.5f))
                  game_device_data.render_resolution_composite = true;
            }
         }
         // Upscaled: from the upscaler's output, at the output resolution, into the canvas (RCAS included), which the stretch then copies 1:1
         if (native_device_context == game_device_data.sr_upscaling_context)
         {
            DrawStateStack<DrawStateStackType::FullGraphics> state;
            state.Cache(native_device_context, device_data.uav_max_count);
            D3D11_TEXTURE2D_DESC desc;
            game_device_data.sr_upscaled_canvas->GetDesc(&desc);
            const D3D11_VIEWPORT viewport = {0.f, 0.f, float(desc.Width), float(desc.Height), 0.f, 1.f};
            const D3D11_RECT scissor = {0, 0, LONG(desc.Width), LONG(desc.Height)};
            ID3D11ShaderResourceView* const scene_srv = game_device_data.sr_upscaled_output_srv.get();
            ID3D11RenderTargetView* const canvas_rtv = game_device_data.sr_upscaled_canvas_rtv.get();
            native_device_context->PSSetShaderResources(0, 1, &scene_srv);
            native_device_context->OMSetRenderTargets(1, &canvas_rtv, nullptr);
            native_device_context->RSSetViewports(1, &viewport);
            native_device_context->RSSetScissorRects(1, &scissor);
            if (cb_luma_global_settings.GameSettings.RCASSharpness <= 0.f || DrawCompositeWithSMAAAndRCAS(native_device, native_device_context, cmd_list_data, device_data, &updated_cbuffers, *original_draw_dispatch_func, false) == DrawOrDispatchOverrideType::None)
            {
               SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, reshade::api::shader_stage::pixel, LumaConstantBufferType::LumaSettings);
               SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, reshade::api::shader_stage::pixel, LumaConstantBufferType::LumaData);
               updated_cbuffers = true;
               (*original_draw_dispatch_func)();
            }
            state.Restore(native_device_context);
            return DrawOrDispatchOverrideType::Replaced;
         }
         // SMAA not after DLSS/FSR (split this frame), RCAS after either
         if (cb_luma_global_settings.GameSettings.SMAAEnable > 0.5f && !game_device_data.scene_antialiased)
            return DrawCompositeWithSMAAAndRCAS(native_device, native_device_context, cmd_list_data, device_data, &updated_cbuffers, *original_draw_dispatch_func, true);
         if (game_device_data.scene_antialiased && cb_luma_global_settings.GameSettings.RCASSharpness > 0.f)
            return DrawCompositeWithSMAAAndRCAS(native_device, native_device_context, cmd_list_data, device_data, &updated_cbuffers, *original_draw_dispatch_func, false);
         return DrawOrDispatchOverrideType::None;
      }

      // The game's stretch of the composite's target onto the swapchain (render scales below 1) is the scene, never UI. Upscaled, it copies the canvas 1:1.
      if (game_device_data.composite_target && original_shader_hashes.Contains(copy_hash, reshade::api::shader_stage::pixel) && original_draw_dispatch_func && *original_draw_dispatch_func)
      {
         com_ptr<ID3D11ShaderResourceView> game_srv;
         native_device_context->PSGetShaderResources(0, 1, &game_srv);
         com_ptr<ID3D11Resource> source;
         if (game_srv)
            game_srv->GetResource(&source);
         if (source && source == game_device_data.composite_target)
         {
            if (native_device_context != game_device_data.sr_upscaling_context)
               return DrawOrDispatchOverrideType::None;
            ID3D11ShaderResourceView* srv = game_device_data.sr_upscaled_canvas_srv.get();
            native_device_context->PSSetShaderResources(0, 1, &srv);
            (*original_draw_dispatch_func)();
            srv = game_srv.get();
            native_device_context->PSSetShaderResources(0, 1, &srv);
            // The frame's post process is done
            game_device_data.sr_upscaling_context = nullptr;
            return DrawOrDispatchOverrideType::Replaced;
         }
      }

      // Everything the game draws onto the swapchain after the composite is UI (HUD, menus, dialogue boxes, fades), except the FXAA passes.
      // Checked after the composite, so a flag left over from the previous frame can never stop the scene from drawing.
      if (device_data.has_drawn_main_post_processing && (stages & reshade::api::shader_stage::pixel) == reshade::api::shader_stage::pixel && !original_shader_hashes.Contains(fxaa_hash, reshade::api::shader_stage::pixel) && !original_shader_hashes.Contains(shader_hashes_apply_fxaa))
      {
         com_ptr<ID3D11RenderTargetView> rtv;
         native_device_context->OMGetRenderTargets(1, &rtv, nullptr);
         com_ptr<ID3D11Resource> rtv_resource;
         if (rtv)
            rtv->GetResource(&rtv_resource);
         if (rtv_resource && IsBackBuffer(&device_data, rtv_resource.get()))
         {
            if (g_hide_ui)
               return DrawOrDispatchOverrideType::Skip;

            // UI draws that depend on the swapchain's magnitude through their blend saw it clamped to 0-1 by the vanilla UNORM target, while
            // the additive UI before them (e.g. the menu cursor's RGB cards, alpha 1 + 1 + 1) now accumulates above 1 on the fp16 one: the
            // cursor's reverse subtracted option text vanished, and destination alpha masks (HUD, main menu) read alphas up to 2. Clamp first,
            // under the draw's own geometry and stencil: the same draw with a white pixel shader and a MIN blend. Colors only before a color
            // subtract; destination alpha factors only need the (never displayed) alpha, and destination color factors are left alone, so the
            // HDR scene under the UI keeps its range. A subtract's own result went below 0 too (the dialogue bubbles, -1.5), so it's floored
            // after, likewise with a black pixel shader and a MAX blend. Additive UI over a bright scene went beyond the display's peak
            // (vanilla clipped it at 1), so it's clamped after to the peak, likewise.
            bool drawn = false;
            if (original_draw_dispatch_func && *original_draw_dispatch_func)
            {
               com_ptr<ID3D11BlendState> blend_state;
               FLOAT blend_factor[4];
               UINT sample_mask;
               native_device_context->OMGetBlendState(&blend_state, blend_factor, &sample_mask);
               D3D11_BLEND_DESC blend_desc = {};
               if (blend_state)
                  blend_state->GetDesc(&blend_desc);
               const D3D11_RENDER_TARGET_BLEND_DESC& rt_blend = blend_desc.RenderTarget[0];
               const auto subtracts = [](D3D11_BLEND_OP op)
               { return op == D3D11_BLEND_OP_SUBTRACT || op == D3D11_BLEND_OP_REV_SUBTRACT; };
               const auto reads_destination_alpha = [](D3D11_BLEND blend)
               { return blend == D3D11_BLEND_DEST_ALPHA || blend == D3D11_BLEND_INV_DEST_ALPHA || blend == D3D11_BLEND_SRC_ALPHA_SAT; };
               const bool subtracts_colors = rt_blend.BlendEnable && subtracts(rt_blend.BlendOp);
               const bool subtracts_alpha = rt_blend.BlendEnable && subtracts(rt_blend.BlendOpAlpha);
               const bool clamp_alpha = subtracts_alpha || (rt_blend.BlendEnable && (reads_destination_alpha(rt_blend.SrcBlend) || reads_destination_alpha(rt_blend.DestBlend) || reads_destination_alpha(rt_blend.SrcBlendAlpha) || reads_destination_alpha(rt_blend.DestBlendAlpha)));
               const bool adds_colors = rt_blend.BlendEnable && rt_blend.BlendOp == D3D11_BLEND_OP_ADD && rt_blend.DestBlend == D3D11_BLEND_ONE;
               if (subtracts_colors || clamp_alpha || adds_colors)
               {
                  com_ptr<ID3D11PixelShader> white_pixel_shader;
                  com_ptr<ID3D11PixelShader> black_pixel_shader;
                  com_ptr<ID3D11PixelShader> peak_pixel_shader;
                  {
                     const std::shared_lock lock_shader_objects(s_mutex_shader_objects);
                     if (HasShaders(device_data.native_pixel_shaders, "P5S Draw White PS"_h, "P5S Draw Black PS"_h, "P5S UI Peak Clamp PS"_h))
                     {
                        white_pixel_shader = device_data.native_pixel_shaders.at("P5S Draw White PS"_h);
                        black_pixel_shader = device_data.native_pixel_shaders.at("P5S Draw Black PS"_h);
                        peak_pixel_shader = device_data.native_pixel_shaders.at("P5S UI Peak Clamp PS"_h);
                     }
                  }
                  // Index 0 all channels, 1 alpha only
                  const int channels = subtracts_colors ? 0 : 1;
                  if (white_pixel_shader && black_pixel_shader && peak_pixel_shader && game_device_data.ui_min_blend_states[0] && game_device_data.ui_min_blend_states[1] && game_device_data.ui_max_blend_states[channels])
                  {
                     com_ptr<ID3D11PixelShader> pixel_shader;
                     native_device_context->PSGetShader(&pixel_shader, nullptr, nullptr);
                     const auto draw = [&](ID3D11PixelShader* draw_pixel_shader, ID3D11BlendState* draw_blend_state)
                     {
                        native_device_context->PSSetShader(draw_pixel_shader, nullptr, 0);
                        native_device_context->OMSetBlendState(draw_blend_state, blend_factor, sample_mask);
                        (*original_draw_dispatch_func)();
                     };
                     if (subtracts_colors || clamp_alpha)
                        draw(white_pixel_shader.get(), game_device_data.ui_min_blend_states[channels].get());
                     draw(pixel_shader.get(), blend_state.get());
                     if (subtracts_colors || subtracts_alpha)
                        draw(black_pixel_shader.get(), game_device_data.ui_max_blend_states[channels].get());
                     if (adds_colors)
                     {
                        SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, reshade::api::shader_stage::pixel, LumaConstantBufferType::LumaSettings);
                        updated_cbuffers = true;
                        draw(peak_pixel_shader.get(), game_device_data.ui_min_blend_states[0].get());
                     }
                     native_device_context->PSSetShader(pixel_shader.get(), nullptr, 0);
                     native_device_context->OMSetBlendState(blend_state.get(), blend_factor, sample_mask);
                     drawn = true;
                  }
               }
            }

            if (drawn)
               return DrawOrDispatchOverrideType::Replaced;
         }
      }

      // SMAA already antialiased the scene, before the UI. Unless a 3D layer (main menu, pause screen) was composed in the UI after it:
      // the vanilla FXAA then runs, which suits its thin line art better than SMAA (tried on the finished frame: more visible stairs),
      // and SMAA on the layer itself, before its composite's tone curve, left fringes on its semi-transparent edges
      if (original_shader_hashes.Contains(fxaa_hash, reshade::api::shader_stage::pixel) && (game_device_data.scene_antialiased.exchange(false) & !game_device_data.layer_drawn.exchange(false)))
         return DrawOrDispatchOverrideType::Skip;

      return DrawOrDispatchOverrideType::None;
   }

   // The game's "RenderScale" setting in memory: its settings block is a run of int32 in config.xml's order, in game.exe's writable
   // data, which the game fills from config.xml before add-ons load. Found by the Resolution, RenderScale, FPS and VSync run; null
   // unless exactly one matches.
   static int32_t* FindRenderScaleSetting()
   {
      const wchar_t* const app_data = _wgetenv(L"APPDATA");
      if (!app_data)
         return nullptr;
      std::ifstream file(std::filesystem::path(app_data) / L"Sega" / L"Steam" / L"P5S" / L"config.xml");
      const std::string xml{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
      int32_t run[4];
      const char* const tags[4] = {"Resolution", "RenderScale", "FPS", "VSync"};
      for (int i = 0; i < 4; i++)
      {
         const std::string open = std::format("<{}>", tags[i]);
         const size_t start = xml.find(open);
         if (start == std::string::npos)
            return nullptr;
         run[i] = std::atoi(xml.c_str() + start + open.size());
      }

      const auto* const module = reinterpret_cast<const uint8_t*>(GetModuleHandleW(nullptr));
      const auto* const nt_headers = reinterpret_cast<const IMAGE_NT_HEADERS*>(module + reinterpret_cast<const IMAGE_DOS_HEADER*>(module)->e_lfanew);
      const uint8_t* const module_end = module + nt_headers->OptionalHeader.SizeOfImage;
      int32_t* found = nullptr;
      size_t matches = 0;
      MEMORY_BASIC_INFORMATION info;
      for (const uint8_t* address = module; address < module_end && VirtualQuery(address, &info, sizeof(info)); address = static_cast<const uint8_t*>(info.BaseAddress) + info.RegionSize)
      {
         constexpr DWORD writable = PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
         if (info.State != MEM_COMMIT || (info.Protect & writable) == 0 || (info.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0)
            continue;
         auto* const region = static_cast<int32_t*>(info.BaseAddress);
         const size_t count = (std::min)(info.RegionSize, size_t(module_end - static_cast<const uint8_t*>(info.BaseAddress))) / sizeof(int32_t);
         for (size_t i = 0; i + 4 <= count; i++)
         {
            if (std::memcmp(region + i, run, sizeof(run)) == 0)
            {
               found = region + i + 1;
               matches++;
            }
         }
      }
      reshade::log::message(matches == 1 ? reshade::log::level::info : reshade::log::level::warning, std::format("[P5S] RenderScale setting {} (config.xml {}, {} matches)", matches == 1 ? "found" : "not found", run[1], matches).c_str());
      return matches == 1 ? found : nullptr;
   }

   // Keeps the game's render scale at the override (or the game's own option), live: the game rebuilds its targets at its setting
   // whenever its window is resized (e.g. alt-tab), so after writing it a WM_SIZE for the current size makes it apply now. Changing
   // the option in the game's menu (which writes the same setting) drops the override, and applies live too.
   // In the main menu ("menu") it renders at 100%: DLSS/FSR never run there, and the background would be a stretched render resolution
   // image. That value is only written for the rebuild, then the kept one is put back, so the game's options menu shows (and would save
   // to config.xml) the real one. An alt-tab there rebuilds at the kept value, which "menu_rebuilt_low" reports, so 100% is applied again.
   static void UpdateRenderScale(Persona5StrikersGameDeviceData* game_device_data, bool menu, bool menu_rebuilt_low)
   {
      if (!game_device_data->render_scale_searched)
      {
         game_device_data->render_scale_searched = true;
         game_device_data->render_scale_setting = FindRenderScaleSetting();
         if (game_device_data->render_scale_setting)
            game_device_data->render_scale_game = game_device_data->render_scale_applied = game_device_data->render_scale_memory = *game_device_data->render_scale_setting;
      }
      int32_t* const setting = game_device_data->render_scale_setting;
      if (!setting)
         return;
      const int32_t current = *setting;
      // The game's menu wrote it (never while a temporary value is in)
      if (game_device_data->render_scale_restore_presents == 0 && current != game_device_data->render_scale_memory)
      {
         game_device_data->render_scale_game = game_device_data->render_scale_memory = current;
         if (g_render_scale != 0)
         {
            g_render_scale = 0;
            reshade::set_config_value(nullptr, NAME, "RenderScale", g_render_scale);
         }
      }
      const int32_t kept = g_render_scale != 0 ? g_render_scale : game_device_data->render_scale_game;
      const int32_t wanted = menu ? 10 : kept;
      if (menu_rebuilt_low && game_device_data->render_scale_restore_presents == 0 && game_device_data->render_scale_applied == wanted)
         game_device_data->render_scale_applied = kept;
      if (wanted != game_device_data->render_scale_applied)
      {
         *setting = game_device_data->render_scale_memory = game_device_data->render_scale_applied = wanted;
         // ponytail: a fixed wait for the rebuild (it follows the message within a frame or two)
         game_device_data->render_scale_restore_presents = wanted != kept ? 10 : 0;
         RECT client;
         if (game_window && GetClientRect(game_window, &client))
            PostMessageW(game_window, WM_SIZE, SIZE_RESTORED, MAKELPARAM(client.right - client.left, client.bottom - client.top));
      }
      else if (game_device_data->render_scale_restore_presents > 0)
      {
         if (--game_device_data->render_scale_restore_presents == 0)
            *setting = game_device_data->render_scale_memory = kept;
      }
      // The override changed while the targets already are at "wanted" (the main menu)
      else if (current != kept)
      {
         *setting = game_device_data->render_scale_memory = kept;
      }
   }

   void OnPresent(ID3D11Device* native_device, DeviceData& device_data) override
   {
      // Set by the composite; Core copies it into "has_drawn_main_post_processing_previous" before this, but never clears it
      device_data.has_drawn_main_post_processing = false;
      // The upscaler's history restarts after any frame it didn't draw (menus, loading, the upscaler just picked)
      device_data.force_reset_sr = !device_data.has_drawn_sr;
      device_data.has_drawn_sr = false;
      auto& game_device_data = GetGameDeviceData(device_data);
      game_device_data.mv_presents++;
      {
         // The main menu from 30 presents on (loading screens and fades in between stay at the game's scale), until the scene draws again.
         // Scene frames are only seen with DLSS/FSR (the motion vectors start them), which is also when 100% matters.
         const bool render_resolution_composite = game_device_data.render_resolution_composite.exchange(false);
         if (game_device_data.scene_drawn.exchange(false) || !IsSRActive(device_data))
            game_device_data.menu_presents = 0;
         else if (render_resolution_composite && game_device_data.menu_presents < 30)
            game_device_data.menu_presents++;
         const bool menu = game_device_data.menu_presents >= 30;
         UpdateRenderScale(&game_device_data, menu, menu && render_resolution_composite);
      }
      // Textures a mip sharper under DLSS/FSR, which resolves the extra detail over its jittered frames (Core applies it to the anisotropic samplers)
      if (!custom_texture_mip_lod_bias_offset)
      {
         const std::unique_lock lock(s_mutex_samplers);
         device_data.texture_mip_lod_bias_offset = IsSRActive(device_data) ? SR::GetMipLODBias(game_device_data.sr_render_height.load(), game_device_data.sr_output_height.load()) : 0.f;
      }
#if DEVELOPMENT
      // "Log MV Probe" logs the frame between this present and the next, and summarizes it at that next present
      if (game_device_data.mv_probe_logging.exchange(g_mv_probe.exchange(false)))
         LogMVProbeSummary(&game_device_data);
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
      if (AreMotionVectorsEnabled(device_data) && cb_luma_global_settings.FrameIndex % 60 == 0)
      {
         // The game's samplers Core upgrades (anisotropic ones), which the mip bias applies to
         size_t samplers = 0, anisotropic_samplers = 0;
         {
            const std::shared_lock lock(s_mutex_samplers);
            for (const auto& [handle, custom_samplers] : device_data.custom_sampler_by_original_sampler)
            {
               D3D11_SAMPLER_DESC desc;
               reinterpret_cast<ID3D11SamplerState*>(handle)->GetDesc(&desc);
               samplers++;
               anisotropic_samplers += desc.Filter == D3D11_FILTER_ANISOTROPIC || desc.Filter == D3D11_FILTER_COMPARISON_ANISOTROPIC;
            }
         }
         reshade::log::message(reshade::log::level::info, std::format("[P5S MV] frame={} per 60 frames: patched_draws={} forward_draws={} jitter_only_draws={} skipped_draws={} uncopied_draws={} matched_draws={} ambiguous_draws={} other_camera_draws={} sr_splits={} sr_draws={} no_overwrite_maps={} samplers={} anisotropic_samplers={} mip_bias={} resource_copies={} fills={} layer_draws={} layer_uncopied_draws={}", cb_luma_global_settings.FrameIndex, game_device_data.mv_patched_draws.exchange(0), game_device_data.mv_forward_draws.exchange(0), game_device_data.mv_jitter_only_draws.exchange(0), game_device_data.mv_skipped_draws.exchange(0), game_device_data.mv_uncopied_draws.exchange(0), game_device_data.mv_matched_draws.exchange(0), game_device_data.mv_ambiguous_draws.exchange(0), game_device_data.mv_other_camera_draws.exchange(0), game_device_data.sr_splits.exchange(0), game_device_data.sr_draws.exchange(0), game_device_data.sr_no_overwrite_maps.exchange(0), samplers, anisotropic_samplers, float(device_data.texture_mip_lod_bias_offset), game_device_data.mv_resource_copies.exchange(0), game_device_data.mv_fills.exchange(0), game_device_data.layer_draws.exchange(0), game_device_data.layer_uncopied_draws.exchange(0)).c_str());
      }
#endif
   }

   void LoadConfigs() override
   {
      auto& settings = cb_luma_global_settings.GameSettings;
      reshade::get_config_value(nullptr, NAME, "SMAAEnable", settings.SMAAEnable);
      reshade::get_config_value(nullptr, NAME, "RCASSharpness", settings.RCASSharpness);
      reshade::get_config_value(nullptr, NAME, "VignetteIntensity", settings.VignetteIntensity);
      reshade::get_config_value(nullptr, NAME, "LensFlareIntensity", settings.LensFlareIntensity);
      reshade::get_config_value(nullptr, NAME, "Exposure", settings.Exposure);
      reshade::get_config_value(nullptr, NAME, "ColorGradingIntensity", settings.ColorGradingIntensity);
      reshade::get_config_value(nullptr, NAME, "Saturation", settings.Saturation);
      reshade::get_config_value(nullptr, NAME, "HighlightsDesaturation", settings.HighlightsDesaturation);
      reshade::get_config_value(nullptr, NAME, "BloomIntensity", settings.BloomIntensity);
      reshade::get_config_value(nullptr, NAME, "Dithering", settings.Dithering);
      reshade::get_config_value(nullptr, NAME, "GTAOEnable", g_gtao_enable);
      reshade::get_config_value(nullptr, NAME, "RenderScale", g_render_scale);
      if (g_render_scale != 0)
         g_render_scale = std::clamp(g_render_scale, 5, 10);
      reshade::get_config_value(nullptr, NAME, "RenderScaleCustom", g_render_scale_custom);
   }

   void DrawImGuiSettings(DeviceData& device_data) override
   {
      auto& settings = cb_luma_global_settings.GameSettings;

      // Persisted GameSettings checkbox (0/1) with its tooltip and reset button
      const auto settings_toggle = [&](const char* label, const char* key, float* value, float default_value, const char* tooltip)
      {
         bool enabled = *value > 0.5f;
         if (ImGui::Checkbox(label, &enabled))
         {
            *value = enabled ? 1.f : 0.f;
            reshade::set_config_value(nullptr, NAME, key, *value);
            device_data.cb_luma_global_settings_dirty = true;
         }
         if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip("%s", tooltip);
         if (DrawResetButton(*value, default_value, key))
            device_data.cb_luma_global_settings_dirty = true;
         return *value > 0.5f;
      };
      // Persisted GameSettings slider, saved once the edit ends
      const auto settings_slider = [&](const char* label, const char* key, float* value, float default_value, float max_value, const char* tooltip)
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
      {
         // The game's render scale, overridden live (see "UpdateRenderScale"): presets named after the upscaler modes (the game's
         // steps are 10%), or any step with the custom slider. Shows the game's own option until overridden.
         const auto& game_device_data = GetGameDeviceData(device_data);
         ImGui::BeginDisabled(!game_device_data.render_scale_setting);
         if (g_render_scale_custom)
         {
            // The game's own steps: its setting (5-10), shown as a percentage ("%d0")
            int scale = g_render_scale != 0 ? g_render_scale : game_device_data.render_scale_game;
            if (ImGui::SliderInt("Render Scale", &scale, 5, 10, g_render_scale != 0 ? "%d0%%" : "%d0%% (game)", ImGuiSliderFlags_AlwaysClamp))
            {
               g_render_scale = scale;
               reshade::set_config_value(nullptr, NAME, "RenderScale", g_render_scale);
            }
         }
         else
         {
            constexpr std::pair<const char*, int> presets[] = {{"Game Setting", 0}, {"DLAA (100%)", 10}, {"Quality (70%)", 7}, {"Balanced (60%)", 6}, {"Performance (50%)", 5}};
            const auto current = std::ranges::find(presets, g_render_scale, &std::pair<const char*, int>::second);
            const std::string preview = g_render_scale == 0 ? std::format("Game Setting ({}%)", game_device_data.render_scale_game * 10) : (current != std::end(presets) ? std::string(current->first) : std::format("{}%", g_render_scale * 10));
            if (ImGui::BeginCombo("Render Scale", preview.c_str()))
            {
               for (const auto& [label, value] : presets)
               {
                  if (ImGui::Selectable(label, value == g_render_scale))
                  {
                     g_render_scale = value;
                     reshade::set_config_value(nullptr, NAME, "RenderScale", g_render_scale);
                  }
               }
               ImGui::EndCombo();
            }
         }
         if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip("The resolution the game renders at, which DLSS/FSR upscale to the output resolution.\nApplies immediately and overrides the game's own option; changing that option in the game drops the override.");
         DrawResetButton(g_render_scale, 0, "RenderScale");
         if (ImGui::Checkbox("Custom Render Scale", &g_render_scale_custom))
            reshade::set_config_value(nullptr, NAME, "RenderScaleCustom", g_render_scale_custom);
         if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip("Picks the render scale in 10%% steps instead of the presets.");
         ImGui::EndDisabled();
      }
      ImGui::BeginDisabled(!settings_toggle("SMAA Enable", "SMAAEnable", &settings.SMAAEnable, default_luma_global_game_settings.SMAAEnable, "Replaces the game's FXAA with SMAA (works with the game's anti-aliasing setting on or off).") && !IsSRActive(device_data));
      settings_slider("RCAS Sharpness", "RCASSharpness", &settings.RCASSharpness, default_luma_global_game_settings.RCASSharpness, 1.f, "Sharpening applied on top of SMAA or DLSS/FSR (0 = off).");
      ImGui::EndDisabled();

      ImGui::SeparatorText("Grade");
      settings_slider("Exposure", "Exposure", &settings.Exposure, default_luma_global_game_settings.Exposure, 2.f, "Overall image brightness (1 = vanilla).");
      settings_slider("Saturation", "Saturation", &settings.Saturation, default_luma_global_game_settings.Saturation, 2.f, "Color saturation, HDR only (1 = vanilla).");
      settings_slider("Highlights Desaturation", "HighlightsDesaturation", &settings.HighlightsDesaturation, default_luma_global_game_settings.HighlightsDesaturation, 1.f, "How far the brightest sources fade to neutral white, HDR only (0 = keep color at any brightness).");
      settings_slider("Color Grading Intensity", "ColorGradingIntensity", &settings.ColorGradingIntensity, default_luma_global_game_settings.ColorGradingIntensity, 1.f, "Strength of the game's own color grading (1 = vanilla, 0 = neutral).");

      ImGui::SeparatorText("Bloom");
      settings_slider("Bloom Intensity", "BloomIntensity", &settings.BloomIntensity, default_luma_global_game_settings.BloomIntensity, 2.f, "Bloom strength (1 = vanilla, 0 = none).");

      ImGui::SeparatorText("Ambient Occlusion");
      if (ImGui::Checkbox("XeGTAO Enable", &g_gtao_enable))
         reshade::set_config_value(nullptr, NAME, "GTAOEnable", g_gtao_enable);
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Replaces the game's SSAO with XeGTAO (cleaner, more accurate ambient occlusion; requires Ambient Occlusion enabled in the game's graphic settings).");
#if DEVELOPMENT || TEST
      ImGui::BeginDisabled(!g_gtao_enable);
      ImGui::SliderFloat("GTAO Final Value Power", &g_gtao_final_value_power, 0.3f, 4.5f, "%.2f");
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
         ImGui::SetTooltip("Primary darkness dial (higher = darker AO). Not saved.");
      ImGui::SliderFloat("GTAO Radius Override", &g_gtao_radius_override, 0.f, 200.f, "%.1f");
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
         ImGui::SetTooltip("0 = the game's SSAO radius (40 cm, capped at 108 half res pixels up close); > 0 overrides it, in centimetres, uncapped. Not saved.");
#if DEVELOPMENT // the shader's debug blocks exist in DEVELOPMENT only
      ImGui::Combo("GTAO Debug View", &g_gtao_debug_view, "Off\0Depth gradient\0Normals\0AO x8\0Edges\0");
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
         ImGui::SetTooltip("Draws diagnostics through the game's SSAO blurs into the G-buffer AO (darkens the ambient lighting only).\nDepth gradient flat or blocky = wrong input; Normals: camera-facing surfaces bright, black everywhere = NORMAL_Z_SIGN inverted;\nAO x8 = spot broad over-occlusion.");
#endif
      ImGui::EndDisabled();
#endif

      ImGui::SeparatorText("Effects");
      settings_slider("Vignette Intensity", "VignetteIntensity", &settings.VignetteIntensity, default_luma_global_game_settings.VignetteIntensity, 1.f, "Scales the game's vignette darkening (1 = vanilla, 0 = none).");
      settings_slider("Lens Flare Intensity", "LensFlareIntensity", &settings.LensFlareIntensity, default_luma_global_game_settings.LensFlareIntensity, 2.f, "Lens-flare / glare strength (1 = vanilla, 0 = off).");
      settings_toggle("Dithering", "Dithering", &settings.Dithering, default_luma_global_game_settings.Dithering, "Reduces gradient banding.");

      ImGui::SeparatorText("UI");
      ImGui::Checkbox("Hide Gameplay UI", &g_hide_ui);
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Disables the in-game UI.");
   }

#if DEVELOPMENT
   void DrawImGuiDevSettings(DeviceData& device_data) override
   {
      ImGui::SeparatorText("SMAA");
      ImGui::Checkbox("SMAA Predication", &g_smaa_predication);
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Finds edges by geometry (plane deviation of the scene depth) as well as by color, so texture detail stays sharp while silhouettes are antialiased. Not saved.");
      ImGui::Combo("SMAA Predication Debug View", &g_smaa_debug_view, "Off\0Edges\0Predication\0");
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Replaces the frame with SMAA's edges (red = horizontal, green = vertical) or the predication edge-ness (red).\nToggle SMAA Predication to compare: texture detail should lose edges, silhouettes keep them.");

      ImGui::SeparatorText("Motion Vectors");
      ImGui::Checkbox("MV Enable", &g_mv_enable);
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Runs the G-buffer with the motion vector shaders without DLSS/FSR (camera and object motion; particles have none).\nReShade.log gets the patched/refused shaders and per 60 frames the draw counters (matched = own previous frame found). Not saved.");
      ImGui::Checkbox("MV Force Jitter", &g_mv_force_jitter);
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Jitters the motion vector draws like for DLSS (Halton, 8 phases, +-0.5 px) without it: the image shakes,\nthe motion vectors must not (debug view black with a static camera). Needs MV Enable.");
      ImGui::Checkbox("MV Debug View", &g_mv_debug_view);
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Shows the motion vector target (absolute, in pixels) through Core's debug draw.");
      if (ImGui::Button("Log MV Probe"))
         g_mv_probe = true;
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Logs the next frame's draws to ReShade.log (pass state, VS cb0 binding and other VS inputs, draw arguments, up to %u),\nthen per G-buffer cb0 buffer how the game fills it (maps by type, updates, distinct binding offsets). Use it in gameplay.", mv_probe_max_draw_lines);
   }
#endif

   void PrintImGuiAbout() override
   {
      ImGui::PushTextWrapPos(0.f);
      ImGui::Text(
         "Luma for \"Persona 5 Strikers\" is developed by DristoforColumb and is open source and free.\n"
         "It adds HDR and DLAA or FSR 3 native anti-aliasing, and replaces the game's FXAA with SMAA and its SSAO with XeGTAO.\n"
         "Enable Ambient Occlusion in the game's graphic settings for XeGTAO to apply; SMAA works either way.\n"
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
                  "\nAMD FidelityFX (RCAS + FSR Native AA)"
                  "\nNVIDIA NGX (DLSS)");
   }
};

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
   if (ul_reason_for_call == DLL_PROCESS_ATTACH)
   {
      Globals::SetGlobals(PROJECT_NAME, "Persona 5 Strikers Luma mod", "", 1);

      // The composite, UI and FXAA all write the swapchain
      swapchain_format_upgrade_type = TextureFormatUpgradesType::AllowedEnabled;
      swapchain_upgrade_type = SwapchainUpgradeType::scRGB;
      // FXAA reads a BGRA8 copy of the swapchain (after UI), which has to hold HDR too.
      // The HDR scene (deferred lighting, the refraction grab copy) and the bloom and flare mips (swapchain aspect ratio) are R11G11B10_FLOAT, upgraded for
      // precision as in Nioh: the bloom chain requantizes through 11 passes, and R11G11B10's 5 bit blue mantissa tints the halos.
      // Arrays are never upgraded, so the R11G11B10 G-buffer array stays.
      // This also upgrades every other swapchain aspect ratio BGRA8 target (e.g. the G-buffer albedo); upgrading only the FXAA copy would save VRAM.
      texture_format_upgrades_type = TextureFormatUpgradesType::AllowedEnabled;
      texture_upgrade_formats = {reshade::api::format::b8g8r8a8_typeless, reshade::api::format::r11g11b10_float};
      texture_format_upgrades_2d_size_filters = (uint32_t)TextureFormatUpgrades2DSizeFilters::SwapchainAspectRatio | (uint32_t)TextureFormatUpgrades2DSizeFilters::No1Px;
      // For the DLSS/FSR mip bias (see "OnPresent")
      enable_samplers_upgrade = true;

#if DEVELOPMENT
      forced_shader_names.emplace(composite_hash, "Composite");
      forced_shader_names.emplace(0x90C6B12E, "UI");
      forced_shader_names.emplace(fxaa_hash, "FXAA");
      for (const uint32_t hash : shader_hashes_apply_fxaa.pixel_shaders)
         forced_shader_names.emplace(hash, "Apply FXAA");
      forced_shader_names.emplace(0x619045C8, "Bloom Combine");
      forced_shader_names.emplace(0x88C4EC12, "Bloom Combine");
      forced_shader_names.emplace(0xB5F3F656, "Bloom Combine");
      forced_shader_names.emplace(0x4CE014A2, "Bloom Combine");
      forced_shader_names.emplace(0xB6289AC0, "Bloom Prefilter");
      forced_shader_names.emplace(ssao_depth_downsample_hash, "SSAO Depth Downsample");
      forced_shader_names.emplace(ssao_hash, "SSAO");
      forced_shader_names.emplace(0x691D080F, "Deferred Lighting");
#endif

      game = new Persona5Strikers();
   }
   else if (ul_reason_for_call == DLL_PROCESS_DETACH)
   {
      Persona5Strikers::UnregisterEvents();
   }

   CoreMain(hModule, ul_reason_for_call, lpReserved);

   return TRUE;
}
