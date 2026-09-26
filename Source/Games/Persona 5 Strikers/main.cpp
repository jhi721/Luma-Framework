#define GAME_PERSONA_5_STRIKERS 1

// Skips the "attach the debugger" popup (Core only checks that it's defined)
#define DISABLE_AUTO_DEBUGGER 1

// Movies play through a separate DX9 device (Media Foundation), same as Nioh
#define CHECK_GRAPHICS_API_COMPATIBILITY 1
// SMAA runs right after the composite, through "original_draw_dispatch_func"
#define ENABLE_POST_DRAW_DISPATCH_CALLBACK 1
#define ENABLE_SMAA 1
// Appends a saturate to the UI shaders (see "PatchShaderBytecodeSync")
#define LUMA_PATCH_BYTECODE_SYNC 1
// The motion vector draw key reads "last_draw_dispatch_data"
#define ENABLE_DRAW_DISPATCH_DATA_CACHE 1

#include "..\..\Core\core.hpp"
#include "..\..\External\WDK\includes\d3d11TokenizedProgramFormat.hpp"
#include "MotionVectorPatches.h"

namespace
{
   // Katana PostEffect3 composite: exposure, lens effects, vignette, baked HDR 3D LUT (tonemap + grade). Once per captured scene
   // (menus, dialogue, hub, field) into the swapchain; the main menu adds one into an off-screen RGBA8 target.
   constexpr uint32_t composite_hash = 0x45A96F2D;
   // FXAA 3 from a swapchain copy (after the UI) back into it
   constexpr uint32_t fxaa_hash = 0xED2D9823;
   // PostEffect3 ApplyFxaa{,Repair,Console,Quality}PS: the engine's FXAA (plus radial blur), from an intermediate composite target into
   // the swapchain. Never captured yet.
   const ShaderHashesList shader_hashes_apply_fxaa = {.pixel_shaders = {0x0B6569A5, 0xC8A7BA1C, 0x95F3321A, 0xED7941FD}};
   // SSAO depth downsample; its t0 (full res D32_FLOAT_S8X24 depth, copied before post) feeds SMAA predication
   constexpr uint32_t ssao_depth_downsample_hash = 0x6E15840A;
   // Native SSAO calculate (half res R8 visibility), replaced by XeGTAO. Its two depth aware upsampling blurs and the G-buffer AO
   // merge (gbuf0.a = min(material AO, SSAO), read by the deferred lighting) stay vanilla.
   constexpr uint32_t ssao_hash = 0x63435B03;
   // UI pixel shaders whose output (texture * vertex color, blend mode, saturation control: grey + k * (color - grey), grey a
   // 0.299/0.587/0.114 weighted RGB sum, k a cb0 scalar) exceeds 0-1: every dumped one ending in that tail, each with one o0.xyzw write and
   // final ret (see "PatchShaderBytecodeSync")
   const std::unordered_set<uint32_t> ui_pixel_shaders = {0x90C6B12E, 0xEE9FC290, 0x07378D54, 0x4A0CB253, 0x8BA60D22, 0xD1BEFD65, 0xF0863953, 0x76C3BC5E, 0xA4DFC750, 0xBEF2C79E};
   // A scene frame's first post pass (scene at t0): DOF (E0DB2D7E), else bloom prefilter, else composite. It ends the scene frame;
   // DLSS/FSR run right before it. The preceding lighting varies by scene (691D080F, or 4376F855 twice).
   const ShaderHashesList post_process_start_shader_hashes = {.pixel_shaders = {0xE0DB2D7E, 0xD65ABD25, 0x3D7CAD40, 0xB6289AC0, composite_hash}};
   // Post passes blending into the scene in place, by UV: DOF merges (5 sample, 9 sample, reduction) and bloom adds (5 and 9 sample,
   // plus sub-rect clamped "ForViewport" twins)
   const ShaderHashesList scene_post_writer_shader_hashes = {.pixel_shaders = {0x409590F7, 0x42D664E0, 0xAA4F82B2, 0x619045C8, 0xB5F3F656, 0x88C4EC12, 0x4CE014A2}};
   // Generic copy: glyphs into their atlas, a 3D layer's alpha into its composite's output, and (render scale below 1) the composite's
   // stretch onto the swapchain
   constexpr uint32_t copy_hash = 0x987DC89C;
   // 3D layers outside the scene (main menu and pause screen characters): the quad vertex shader (texture coordinates from vertices)
   // and the stretch of a layer's render resolution corner over its target
   constexpr uint32_t quad_vertex_shader_hash = 0x2B6CA9A0;
   constexpr uint32_t layer_stretch_hash = 0x99A76DC2;
   constexpr UINT layer_uv_scale_cb_slot = 5; // "register(b5)" in Includes/LayerCorner.hlsl; the quads and sprites only read b0
   // UI sprite vertex shader (also puts 3D layer composites on the swapchain) and the exposure histogram reading a layer
   constexpr uint32_t ui_sprite_vertex_shader_hash = 0x8B19022A;
   constexpr uint32_t exposure_histogram_hash = 0xCC8D4849;

   bool g_hide_ui = false; // Session only, so a restart always has a HUD

   // Overrides the game's "RenderScale" (5 = 50% ... 10 = 100%); 0 keeps the game's option
   int g_render_scale = 0;

   bool g_gtao_enable = true;
   constexpr UINT gtao_knobs_cb_slot = 9; // "register(b9)" in Luma_P5S_XeGTAO.hlsl; b11 is core DrawBloom's
   float g_gtao_final_value_power = 1.f;  // DEV/TEST calibration knobs, not persisted
   float g_gtao_radius_override = 0.f;    // > 0 overrides the native radius (centimetres)

#if DEVELOPMENT
   int g_gtao_debug_view = 0; // 0=off 1=depth gradient 2=normals 3=AO x8 4=edges
   bool g_smaa_predication = true;
   int g_smaa_debug_view = 0; // 0 off, 1 edges, 2 predication
   // "Performance Test" (see "OnPresent"): the mode, its render scale override (0 none) and name; 2 skips the motion vector draws
   int g_perf_test = 0;
   constexpr int perf_test_render_scales[] = {0, 10, 10, 7, 5};
   constexpr const char* perf_test_modes[] = {"Off", "DLAA", "DLAA Without Motion Vector Draws", "Quality (70%)", "Performance (50%)"};
#else
   constexpr int g_gtao_debug_view = 0;
   constexpr bool g_smaa_predication = true;
#endif

   // A Luma shader, null until compiled. The caller holds s_mutex_shader_objects.
   template <typename T>
   typename T::mapped_type FindShader(const T& shaders, uint32_t name)
   {
      const auto it = shaders.find(name);
      return it != shaders.end() ? it->second : typename T::mapped_type{};
   }

   // True when all the named Luma shaders are compiled. The caller holds s_mutex_shader_objects.
   template <typename T, typename... Names>
   bool HasShaders(const T& shaders, Names... names)
   {
      return (bool(FindShader(shaders, names)) && ...);
   }

   // The view's resource, null without a view
   com_ptr<ID3D11Resource> GetViewResource(ID3D11View* view)
   {
      com_ptr<ID3D11Resource> resource;
      if (view)
         view->GetResource(&resource);
      return resource;
   }

   // Writes a dynamic constant buffer, created on first use; false if it can't
   bool WriteConstants(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, com_ptr<ID3D11Buffer>* buffer, const void* data, UINT size)
   {
      if (!*buffer)
      {
         const D3D11_BUFFER_DESC desc = {size, D3D11_USAGE_DYNAMIC, D3D11_BIND_CONSTANT_BUFFER, D3D11_CPU_ACCESS_WRITE};
         native_device->CreateBuffer(&desc, nullptr, &(*buffer));
      }
      D3D11_MAPPED_SUBRESOURCE mapped;
      if (!*buffer || FAILED(native_device_context->Map(buffer->get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
         return false;
      std::memcpy(mapped.pData, data, size);
      native_device_context->Unmap(buffer->get(), 0);
      return true;
   }

   // Draws with a Luma clone of a layer's vertex shader (see "Includes/LayerCorner.hlsl") and uv scale, or as is without one, bypassing
   // Core's state tracking.
   void DrawWithLayerVertexShader(ID3D11DeviceContext* native_device_context, ID3D11VertexShader* vertex_shader, ID3D11Buffer* uv_scale_buffer, const std::function<void()>& draw)
   {
      if (!vertex_shader)
      {
         draw();
         return;
      }
      com_ptr<ID3D11VertexShader> original_vertex_shader;
      com_ptr<ID3D11Buffer> original_uv_scale_buffer;
      native_device_context->VSGetShader(&original_vertex_shader, nullptr, nullptr);
      native_device_context->VSGetConstantBuffers(layer_uv_scale_cb_slot, 1, &original_uv_scale_buffer);
      native_device_context->VSSetConstantBuffers(layer_uv_scale_cb_slot, 1, &uv_scale_buffer);
      native_device_context->VSSetShader(vertex_shader, nullptr, 0);
      draw();
      ID3D11Buffer* const buffer = original_uv_scale_buffer.get();
      native_device_context->VSSetConstantBuffers(layer_uv_scale_cb_slot, 1, &buffer);
      native_device_context->VSSetShader(original_vertex_shader.get(), nullptr, 0);
   }

} // namespace

// Holds the device objects, so they are released with the device.
struct Persona5StrikersGameDeviceData final : public GameDeviceData
{
   // SMAA scratch, recreated on canvas resize: a linear canvas copy (SMAA can't sample the canvas it writes), its gamma encode (edge
   // detection input; with RCAS also SMAA's output for finalize) and the predication edge-ness.
   com_ptr<ID3D11Texture2D> smaa_linear_texture;
   com_ptr<ID3D11ShaderResourceView> smaa_linear_srv;
   com_ptr<ID3D11ShaderResourceView> smaa_gamma_srv;
   com_ptr<ID3D11RenderTargetView> smaa_gamma_rtv;
   com_ptr<ID3D11UnorderedAccessView> smaa_gamma_uav;
   com_ptr<ID3D11ShaderResourceView> smaa_predication_srv;
   com_ptr<ID3D11UnorderedAccessView> smaa_predication_uav;

   // Full res scene depth (SSAO depth downsample's t0) for predication, and for motion vectors when the G-buffer's is unreadable.
   // Kept across frames (the texture persists, rewritten before post): SSAO and post record on different deferred contexts, so per frame
   // resets would race.
   std::mutex smaa_depth_mutex;
   com_ptr<ID3D11ShaderResourceView> smaa_depth_srv;

   // SMAA ran after this frame's composite, or DLSS/FSR before its post: skip the vanilla FXAA
   std::atomic<bool> scene_antialiased = false;
   // A 3D layer outside the scene was drawn (main menu, pause screen), composed by the UI after SMAA: FXAA still runs
   std::atomic<bool> layer_drawn = false;

   // MIN and MAX blends (all channels, alpha only), to clamp the swapchain to 0-1 under a UI draw's own geometry (see "OnDrawOrDispatch")
   com_ptr<ID3D11BlendState> ui_min_blend_states[2];
   com_ptr<ID3D11BlendState> ui_max_blend_states[2];

   // XeGTAO scratch, recreated on SSAO target resize; mutex guarded, as SSAO records on worker threads (deferred contexts)
   std::mutex gtao_mutex;
   com_ptr<ID3D11UnorderedAccessView> gtao_depth_mip_uavs[5]; // R32F view space depth pyramid, 5 mips
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

   // Motion vectors: shaders patched on first use, by original hash (null on failure), and the target
   std::shared_mutex mv_mutex;
   std::unordered_map<uint32_t, com_ptr<ID3D11VertexShader>> mv_vertex_shaders;
   std::unordered_map<uint32_t, com_ptr<ID3D11PixelShader>> mv_pixel_shaders;
   com_ptr<ID3D11Texture2D> mv_texture;
   com_ptr<ID3D11RenderTargetView> mv_rtv;
   // G-buffer blend state copies (independent blending) that also write the MV target, by original, with its description in case the
   // address is reused
   std::unordered_map<ID3D11BlendState*, std::pair<D3D11_BLEND_DESC, com_ptr<ID3D11BlendState>>> mv_blend_states;
   // Byte offsets in each patched vertex shader's $Globals, by original hash: "mW2P" (camera view projection) and "mL2W" (world
   // matrix or bone palette, 3 rows with the translation in .w)
   struct GlobalsLayout
   {
      UINT view_projection = UINT_MAX;
      UINT world = UINT_MAX;
   };
   std::unordered_map<uint32_t, GlobalsLayout> mv_globals_layouts;
   // The scene records on one deferred context: after its first post pass, the next scene depth draw (depth prepass, else G-buffer)
   // starts a frame and clears the target
   bool mv_frame_ended = true;
   com_ptr<ID3D11Resource> mv_scene_depth;          // The G-buffer's depth, which the forward redraws share
   ID3D11DeviceContext* mv_scene_context = nullptr; // Only compared

   // CPU copies of the $Globals buffers patched draws bind (mapped with discard before nearly every draw; some draws reuse the last
   // contents): each Map's pointer, copied at Unmap
   std::mutex mv_globals_mutex;
   std::unordered_set<uint64_t> mv_globals_buffers;
   std::unordered_map<uint64_t, void*> mv_mapped_globals;
   std::unordered_map<uint64_t, std::vector<uint8_t>> mv_globals_copies;
   // Scene context only: the previous frame's $Globals uploads, one dynamic buffer per size
   std::unordered_map<UINT, com_ptr<ID3D11Buffer>> mv_previous_globals_buffers;
   // Scene context only: this and last frame's copies of each resource the patched vertex shaders read (wind and interaction buffers,
   // ocean maps), taken at first use in a frame; the latter feeds the vertex shader's second run. Held until a frame passes without it,
   // so its address can't be reused.
   struct PreviousResource
   {
      com_ptr<ID3D11Resource> resource;
      com_ptr<ID3D11Resource> copies[2];          // This frame's, the previous frame's
      com_ptr<ID3D11ShaderResourceView> views[2]; // Of the copies, created when read
      D3D11_SHADER_RESOURCE_VIEW_DESC view_descs[2] = {};
      uint32_t frame = 0;
      bool has_previous = false;
   };
   std::unordered_map<ID3D11Resource*, PreviousResource> mv_previous_resources;
   // Patched draws by draw key (shaders, buffers, arguments), with translation and $Globals. A draw takes the previous frame's
   // $Globals of its key's nearest draw (same object, a frame earlier).
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
   // Camera motion fill of pixels no patched draw wrote (see "Luma_P5S_MotionVectorFill.hlsl"; frame start marks them FLT_MAX): the
   // target's UAV (if the GPU loads R32G32_FLOAT from UAVs), its constants, and the frame's depth (also the upscaler's)
   com_ptr<ID3D11UnorderedAccessView> mv_uav;
   com_ptr<ID3D11Buffer> mv_fill_buffer;
   bool mv_fill_pending = false;
   com_ptr<ID3D11ShaderResourceView> mv_scene_depth_srv;
   com_ptr<ID3D11Resource> mv_frame_depth;
   std::array<float, 16> mv_view_projection = {};
   std::array<float, 16> mv_previous_view_projection = {};
   bool mv_view_projection_valid = false;
   bool mv_previous_view_projection_valid = false;
   uint32_t mv_frame_present = 0;
   std::atomic<uint32_t> mv_presents = 0;

#if DEVELOPMENT
   // "Performance Test": GPU timestamps from the scene frame's start (in the game's command list) to the split and around DLSS/FSR, in a
   // ring read back a few frames later without waiting (the game caps at 60 fps, so frame times say nothing), and the CPU time in the
   // motion vector hooks
   struct PerfQueries
   {
      com_ptr<ID3D11Query> disjoint, scene_start, sr_start, sr_end;
      // Issued, until read back. A set whose split never ran is reused when the ring comes back to it (8 scene frames later).
      bool pending = false;
   };
   struct PerfStats
   {
      double scene_ms = 0.0, scene_max_ms = 0.0, sr_ms = 0.0, sr_max_ms = 0.0;
      uint32_t samples = 0, disjoint = 0, frames = 0;
   };
#endif

   // DLSS/FSR run on the immediate context but the scene records on a deferred one, so its command list is split before post; the
   // first part runs before the upscaler when the game executes the rest (see "OnExecuteSecondaryCommandList").
   struct SRSplit
   {
#if DEVELOPMENT
      PerfQueries* perf_queries = nullptr;
#endif
      com_ptr<ID3D11CommandList> partial;
      com_ptr<ID3D11CommandList> remainder;  // Held so its address (the pending split's key) can't be reused
      com_ptr<ID3D11Texture2D> source_color; // Also the output at native resolution (DLAA), read by the post process
      com_ptr<ID3D11Texture2D> output_color; // Upscaling only (render scale below 1): the output, read by the post process
      com_ptr<ID3D11Resource> depth;
      std::array<float, 2> jitter;
      float vertical_fov = 0.7330383f; // Radians (FSR needs it), 42 degrees as measured in dialogue until the frame's camera is known
   };
   std::mutex sr_mutex;
   bool sr_split_ready = false;                                // Scene context only: this frame's G-buffer drew, not split yet
   uint64_t sr_split_context = 0;                              // The context split last, until the game finishes its command list
   SRSplit sr_split;                                           // The last split, likewise
   std::unordered_map<uint64_t, SRSplit> sr_pending_splits;    // By the game's command list (the remainder), until executed
   std::vector<com_ptr<ID3D11Buffer>> sr_no_overwrite_buffers; // Dynamic buffers the game appends to ("D3D11_MAP_WRITE_NO_OVERWRITE")
#if DEVELOPMENT
   std::mutex perf_mutex;
   std::array<PerfQueries, 8> perf_queries;
   size_t perf_query_index = 0;
   PerfQueries* perf_frame_queries = nullptr; // Scene context only: this frame's, until its split takes it
   PerfStats perf_stats;                      // This log window's
   int perf_settle_frames = 0;                // Frames skipped after a mode change (targets rebuilt, history reset)
   std::atomic<int64_t> perf_hook_ns = 0;     // This log window's
#endif
   // Upscaling: the game renders scene and post (composite included) at render scale, then stretches onto the swapchain. Instead the
   // upscaler writes the output resolution, scene blending post passes also blend into it, and the composite draws from it into an
   // output sized canvas that the stretch copies 1:1.
   ID3D11DeviceContext* sr_upscaling_context = nullptr; // This frame's scene context, once split for upscaling (only compared)
   com_ptr<ID3D11Texture2D> sr_upscaled_output;
   com_ptr<ID3D11RenderTargetView> sr_upscaled_output_rtv;
   com_ptr<ID3D11ShaderResourceView> sr_upscaled_output_srv;
   com_ptr<ID3D11RenderTargetView> sr_upscaled_canvas_rtv;
   com_ptr<ID3D11ShaderResourceView> sr_upscaled_canvas_srv;
   com_ptr<ID3D11Resource> composite_target; // This frame's composite target when it isn't the swapchain (render scales below 1)
   // The game's "RenderScale" in memory (see "FindRenderScaleSetting"), null if not found. Present thread only.
   int32_t* render_scale_setting = nullptr;
   bool render_scale_searched = false;
   int32_t render_scale_game = 0;            // The game's own option, restored without an override
   int32_t render_scale_applied = 0;         // The value the game last rebuilt its targets at (as known)
   int32_t render_scale_memory = 0;          // The value Luma last left in the setting
   int render_scale_restore_presents = 0;    // Until a temporary value (the main menu's 100%) is replaced by the kept one
   bool render_scale_menu_reapplied = false; // The main menu's 100% was reapplied since it opened (see "UpdateRenderScale")
   // The main menu: presents without a scene frame whose composite draws into a render resolution target (its background); the
   // pause screen has neither. Reset by any scene frame.
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
   // Layer stretch targets, then the composites made from them (only compared): the pause screen's composite reads its stretch target
   // whole; a UI sprite scales up, and the exposure reads, its output's render resolution corner.
   struct LayerOutput
   {
      float scale = 1.f; // Render resolution / output resolution
      uint32_t frame_index = 0;
   };
   std::unordered_map<ID3D11Resource*, LayerOutput> layer_outputs;
   std::unordered_map<ID3D11Resource*, LayerOutput> layer_composed;
   com_ptr<ID3D11RenderTargetView> layer_histogram_rtv; // The stretch target downscaled to render resolution, for the exposure
   com_ptr<ID3D11ShaderResourceView> layer_histogram_srv;
   std::atomic<uint32_t> sr_render_height = 1;
   std::atomic<uint32_t> sr_output_height = 1;
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

#if DEVELOPMENT
   // "Performance Test": adds the scope's CPU time to the motion vector hooks' total, while the test runs
   struct PerfHookTimer
   {
      std::atomic<int64_t>& total_ns;
      const bool enabled = g_perf_test != 0;
      const std::chrono::steady_clock::time_point start = enabled ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
      ~PerfHookTimer()
      {
         if (enabled)
            total_ns += std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count();
      }
   };
#endif

   static DeviceData* GetDeviceData(reshade::api::device* device)
   {
      auto* const device_data = device->get_private_data<DeviceData>();
      return device_data && device_data->game ? device_data : nullptr;
   }

   // Motion vectors: the mapped pointer of a $Globals buffer a patched draw binds, copied at Unmap
   static void OnMapBufferRegion(reshade::api::device* device, reshade::api::resource resource, uint64_t offset, uint64_t size, reshade::api::map_access access, void** data)
   {
      DeviceData* const device_data = GetDeviceData(device);
      if (!device_data || !IsSRActive(*device_data))
         return;
      auto& game_device_data = GetGameDeviceData(*device_data);
#if DEVELOPMENT
      const PerfHookTimer timer{game_device_data.perf_hook_ns};
#endif
      if (access == reshade::api::map_access::write_discard && data && *data)
      {
         const std::lock_guard lock(game_device_data.mv_globals_mutex);
         if (game_device_data.mv_globals_buffers.contains(resource.handle))
            game_device_data.mv_mapped_globals[resource.handle] = *data;
      }
      // A deferred context's first map of a dynamic buffer in a command list must discard, so the split discards these (see "sr_no_overwrite_buffers")
      else if (access == reshade::api::map_access::write_only)
      {
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
   }

   // Motion vectors: copies a written $Globals buffer's mapped memory to the CPU at Unmap
   static void OnUnmapBufferRegion(reshade::api::device* device, reshade::api::resource resource)
   {
      DeviceData* const device_data = GetDeviceData(device);
      auto* const game_device_data = device_data && IsSRActive(*device_data) ? &GetGameDeviceData(*device_data) : nullptr;
      if (!game_device_data)
         return;
#if DEVELOPMENT
      const PerfHookTimer timer{game_device_data->perf_hook_ns};
#endif
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

   // A scene frame starts at its first draw into the scene depth (depth prepass, else G-buffer): clears the motion vectors, moves the
   // object history to the previous frame, and sets the jitter
   static void StartMotionVectorFrame(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, DeviceData& device_data, const uint4& depth_size)
   {
      auto& game_device_data = GetGameDeviceData(device_data);
#if DEVELOPMENT
      // "Performance Test": the scene's GPU time starts here, a timestamp recorded into the game's command list
      {
         const std::lock_guard lock(game_device_data.perf_mutex);
         game_device_data.perf_frame_queries = nullptr; // The last frame's if it never split
         auto& queries = game_device_data.perf_queries[game_device_data.perf_query_index];
         // Skipped while the ring's next set is still unread
         if (g_perf_test != 0 && !queries.pending)
         {
            const D3D11_QUERY_DESC disjoint_desc = {D3D11_QUERY_TIMESTAMP_DISJOINT}, timestamp_desc = {D3D11_QUERY_TIMESTAMP};
            if (!queries.disjoint)
            {
               native_device->CreateQuery(&disjoint_desc, &queries.disjoint);
               native_device->CreateQuery(&timestamp_desc, &queries.scene_start);
               native_device->CreateQuery(&timestamp_desc, &queries.sr_start);
               native_device->CreateQuery(&timestamp_desc, &queries.sr_end);
            }
            if (queries.disjoint && queries.scene_start && queries.sr_start && queries.sr_end)
            {
               game_device_data.perf_frame_queries = &queries;
               game_device_data.perf_query_index = (game_device_data.perf_query_index + 1) % game_device_data.perf_queries.size();
               native_device_context->End(queries.scene_start.get());
            }
         }
      }
#endif
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
      // Last frame's camera is the previous one, unless scene-less frames (menus) came between
      game_device_data.mv_previous_view_projection = game_device_data.mv_view_projection;
      game_device_data.mv_previous_view_projection_valid = game_device_data.mv_view_projection_valid && game_device_data.mv_presents - game_device_data.mv_frame_present <= 1;
      game_device_data.mv_view_projection_valid = false;
      game_device_data.mv_frame_present = game_device_data.mv_presents;
      game_device_data.mv_previous_objects = std::move(game_device_data.mv_objects);
      game_device_data.mv_objects.clear();
      game_device_data.mv_objects.reserve(game_device_data.mv_previous_objects.size());
      if (!game_device_data.mv_previous_view_projection_valid)
         game_device_data.mv_previous_objects.clear();
      std::erase_if(game_device_data.mv_previous_resources, [&](const auto& entry)
         { return entry.second.frame + 1 < game_device_data.mv_frame_present; });

      // Halton (2, 3) over the upscaler's phase count (from its last settings; more at lower render scales)
      const bool jitter = IsSRActive(device_data);
      const SR::InstanceData* const sr_instance_data = IsSRActive(device_data) ? device_data.GetSRInstanceData() : nullptr;
      const int phases = sr_instance_data ? (std::max)(sr_implementations[device_data.sr_type]->GetJitterPhases(sr_instance_data), 1) : SR::GetDefaultJitterPhases();
      game_device_data.mv_jitter = jitter ? std::array<float, 2>{SR::HaltonSequence(cb_luma_global_settings.FrameIndex % phases, 2), SR::HaltonSequence(cb_luma_global_settings.FrameIndex % phases, 3)} : std::array<float, 2>{};
      // Pixels to NDC (y up)
      const float ndc_jitter[4] = {game_device_data.mv_jitter[0] * 2.f / float(depth_size.x), game_device_data.mv_jitter[1] * -2.f / float(depth_size.y), 0.f, 0.f};
      WriteConstants(native_device, native_device_context, std::addressof(game_device_data.mv_jitter_buffer), ndc_jitter, sizeof(ndc_jitter));
   }

   // A patched vertex shader's $Globals byte offsets, by original hash (none until patched)
   static Persona5StrikersGameDeviceData::GlobalsLayout GetGlobalsLayout(Persona5StrikersGameDeviceData* game_device_data, uint32_t hash)
   {
      const std::shared_lock lock(game_device_data->mv_mutex);
      const auto it = game_device_data->mv_globals_layouts.find(hash);
      return it != game_device_data->mv_globals_layouts.end() ? it->second : Persona5StrikersGameDeviceData::GlobalsLayout{};
   }

   // A $Globals buffer's CPU copy (empty until its first Unmap); registers it for a copy at every Unmap
   static std::vector<uint8_t> GetGlobalsCopy(Persona5StrikersGameDeviceData* game_device_data, ID3D11Buffer* buffer)
   {
      const std::lock_guard lock(game_device_data->mv_globals_mutex);
      const uint64_t handle = reinterpret_cast<uint64_t>(buffer);
      game_device_data->mv_globals_buffers.insert(handle);
      const auto copy = game_device_data->mv_globals_copies.find(handle);
      return copy != game_device_data->mv_globals_copies.end() ? copy->second : std::vector<uint8_t>{};
   }

   // A scene frame ends at its first post pass: picks the frame's depth (the upscaler's) and fills camera motion where no patched draw wrote
   static void EndMotionVectorFrame(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, DeviceData& device_data)
   {
      auto& game_device_data = GetGameDeviceData(device_data);
      game_device_data.mv_frame_ended = true;

      // The G-buffer's depth if it's a shader resource, else the game's pre-post copy
      com_ptr<ID3D11ShaderResourceView> depth_srv;
      D3D11_TEXTURE2D_DESC depth_desc = {};
      if (com_ptr<ID3D11Texture2D> depth_texture; game_device_data.mv_scene_depth && SUCCEEDED(game_device_data.mv_scene_depth->QueryInterface(&depth_texture)))
         depth_texture->GetDesc(&depth_desc);
      if ((depth_desc.BindFlags & D3D11_BIND_SHADER_RESOURCE) != 0)
      {
         if (GetViewResource(game_device_data.mv_scene_depth_srv.get()) != game_device_data.mv_scene_depth)
         {
            game_device_data.mv_scene_depth_srv.reset();
            // The depth formats' depth channel
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
      game_device_data.mv_frame_depth = GetViewResource(depth_srv.get());

      if (game_device_data.mv_fill_pending)
      {
         game_device_data.mv_fill_pending = false;
         // Current clip space to the previous frame's, for row vectors: inverse(current) * previous, in double so the world translation
         // (centimetres) cancels out.
         double reprojection[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
         if (game_device_data.mv_view_projection_valid && game_device_data.mv_previous_view_projection_valid)
         {
            // Gauss-Jordan with partial pivoting on [current | identity]
            double a[4][8] = {};
            for (int row = 0; row < 4; row++)
            {
               for (int column = 0; column < 4; column++)
                  a[row][column] = game_device_data.mv_view_projection[row * 4 + column];
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
                     reprojection[row * 4 + column] += a[row][4 + k] * double(game_device_data.mv_previous_view_projection[k * 4 + column]);
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
         const std::shared_lock lock_shader_objects(s_mutex_shader_objects);
         const com_ptr<ID3D11ComputeShader> fill_shader = FindShader(device_data.native_compute_shaders, "P5S Motion Vector Fill CS"_h);
         // ponytail: a shader reload between the clear and here (DEV) leaves the FLT_MAX marker for a frame
         if (depth_srv && fill_shader && WriteConstants(native_device, native_device_context, std::addressof(game_device_data.mv_fill_buffer), constants, sizeof(constants)))
         {
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
            native_device_context->CSSetShader(fill_shader.get(), nullptr, 0);
            native_device_context->Dispatch((mv_desc.Width + 7) / 8, (mv_desc.Height + 7) / 8, 1);
            compute_state.Restore(native_device_context);
            graphics_state.Restore(native_device_context);
         }
      }
   }

   // Jitter for scene draws without motion vectors (patched vertex shader, game pixel shader): the depth prepass, which the G-buffer
   // tests with GREATER_EQUAL (unjittered, sloped floors lost pixels to the jittered G-buffer: black flicker), and depth tested geometry
   // not writing depth. False if it can't (the draw runs untouched).
   static bool DrawWithJitter(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, CommandListData& cmd_list_data, DeviceData& device_data, const ShaderHashesList<OneShaderPerPipeline>& original_shader_hashes, ID3D11DepthStencilView* dsv, bool depth_prepass, const std::function<void()>& draw)
   {
      auto& game_device_data = GetGameDeviceData(device_data);
      const com_ptr<ID3D11Resource> depth = GetViewResource(dsv);
      // Into the last G-buffer's depth; only a depth prepass starts a frame, others join it
      if (!depth || depth != game_device_data.mv_scene_depth || !game_device_data.mv_rtv || (game_device_data.mv_frame_ended ? !depth_prepass : native_device_context != game_device_data.mv_scene_context))
         return false;
      const com_ptr<ID3D11VertexShader> vertex_shader = GetMotionVectorShader(native_device, device_data, &game_device_data.mv_vertex_shaders, original_shader_hashes.vertex_shaders[0], cmd_list_data.pipeline_state_original_vertex_shader);
      if (!vertex_shader)
         return false;
      // Objects only: full screen passes have no camera
      if (GetGlobalsLayout(&game_device_data, original_shader_hashes.vertex_shaders[0]).view_projection == UINT_MAX)
         return false;
      if (game_device_data.mv_frame_ended)
      {
         uint4 depth_size;
         DXGI_FORMAT depth_format;
         GetResourceInfo(depth.get(), depth_size, depth_format);
         StartMotionVectorFrame(native_device, native_device_context, device_data, depth_size);
      }
#if DEVELOPMENT
      // "Performance Test" without motion vector draws: the frame and its split still happen, the draws run untouched
      if (g_perf_test == 2)
         return false;
#endif

      com_ptr<ID3D11VertexShader> original_vertex_shader;
      native_device_context->VSGetShader(&original_vertex_shader, nullptr, nullptr);
      com_ptr<ID3D11Buffer> original_jitter;
      native_device_context->VSGetConstantBuffers(MotionVectorPatches::jitter_slot, 1, &original_jitter);
      ID3D11Buffer* const jitter = game_device_data.mv_jitter_buffer.get();
      native_device_context->VSSetConstantBuffers(MotionVectorPatches::jitter_slot, 1, &jitter);
      native_device_context->VSSetShader(vertex_shader.get(), nullptr, 0);
      draw();
      // Set directly, bypassing Core's state tracking: restore the game's
      native_device_context->VSSetShader(original_vertex_shader.get(), nullptr, 0);
      ID3D11Buffer* const restored_jitter = original_jitter.get();
      native_device_context->VSSetConstantBuffers(MotionVectorPatches::jitter_slot, 1, &restored_jitter);
      return true;
   }

   // Draws a G-buffer draw, or a forward scene redraw into its depth (outlines, sky), with the patched shaders, adding the motion vector
   // target ("target_slot", past the game's) and the previous frame's $Globals ("previous_globals_slot"). False if it can't (the draw
   // runs untouched).
   static bool DrawWithMotionVectors(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, CommandListData& cmd_list_data, DeviceData& device_data, const ShaderHashesList<OneShaderPerPipeline>& original_shader_hashes, ID3D11RenderTargetView* const (&rtvs)[8], ID3D11DepthStencilView* dsv, bool gbuffer, const std::function<void()>& draw)
   {
      auto& game_device_data = GetGameDeviceData(device_data);
      const com_ptr<ID3D11Resource> depth = GetViewResource(dsv);
      // Forward draws: into this frame's G-buffer depth, before its post
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
      // Without independent blending the MV target takes RT0's blend, which must be off; with it, a state copy writes it unblended
      if ((!blend_desc.IndependentBlendEnable && blend_desc.RenderTarget[0].BlendEnable) || !vertex_shader || !pixel_shader || !dsv)
         return false;
      // Forward full screen passes have no camera
      const Persona5StrikersGameDeviceData::GlobalsLayout layout = GetGlobalsLayout(&game_device_data, original_shader_hashes.vertex_shaders[0]);
      if (!gbuffer && layout.view_projection == UINT_MAX)
         return false;
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
            return false;
      }

      // The G-buffer owns the target (scene depth sized) and starts frames; forward draws only add to it
      if (gbuffer)
      {
         uint4 depth_size = {};
         // The target already matches the same depth, and only a frame start needs the size
         if (depth != game_device_data.mv_scene_depth || !game_device_data.mv_rtv || game_device_data.mv_frame_ended)
         {
            DXGI_FORMAT depth_format;
            GetResourceInfo(depth.get(), depth_size, depth_format);
            game_device_data.mv_scene_depth = depth;
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
#if DEVELOPMENT
      if (g_perf_test == 2)
         return false;
#endif

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
      // The previous frame's $Globals: the same object's from last frame, else this draw's with last frame's camera (no object motion).
      // Draws of a buffer without a CPU copy yet get the current data (zero motion).
      std::vector<uint8_t> previous_globals_data = GetGlobalsCopy(&game_device_data, globals.get());
      ID3D11Buffer* previous_globals = globals.get();
      if (!previous_globals_data.empty() && layout.view_projection != UINT_MAX && layout.view_projection + 64 <= previous_globals_data.size())
      {
         uint8_t* const view_projection = previous_globals_data.data() + layout.view_projection;
         // The frame's first draw has the camera
         if (!game_device_data.mv_view_projection_valid)
         {
            std::memcpy(game_device_data.mv_view_projection.data(), view_projection, sizeof(game_device_data.mv_view_projection));
            game_device_data.mv_view_projection_valid = true;
         }

         // Draw key: same mesh, same shaders. Objects sharing it (e.g. props) are told apart by translation.
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

         // ponytail: linear search among the key's candidates (a handful at most); a spatial lookup if big crowds share a mesh
         const Persona5StrikersGameDeviceData::MotionVectorObject* match = nullptr;
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
            }
         }
         // Kept as drawn for the next frame (moved when the match's is uploaded)
         game_device_data.mv_objects[key].push_back({translation, match ? std::move(previous_globals_data) : previous_globals_data});
         // Not found: last frame's camera, if this is the frame's view projection (others are left as is)
         if (!match && game_device_data.mv_previous_view_projection_valid && std::memcmp(view_projection, game_device_data.mv_view_projection.data(), sizeof(game_device_data.mv_view_projection)) == 0)
            std::memcpy(view_projection, game_device_data.mv_previous_view_projection.data(), sizeof(game_device_data.mv_previous_view_projection));

         const std::vector<uint8_t>& upload_data = match ? match->globals : previous_globals_data;
         com_ptr<ID3D11Buffer>& upload = game_device_data.mv_previous_globals_buffers[UINT(upload_data.size())];
         if (WriteConstants(native_device, native_device_context, std::addressof(upload), upload_data.data(), UINT(upload_data.size())))
            previous_globals = upload.get();
      }
      native_device_context->VSSetConstantBuffers(MotionVectorPatches::previous_globals_slot, 1, &previous_globals);
      // Second run resources: the previous frame's copies, else the current ones (immutable, or no previous copy)
      com_ptr<ID3D11ShaderResourceView> srvs[MotionVectorPatches::resource_slots];
      native_device_context->VSGetShaderResources(0, MotionVectorPatches::resource_slots, &srvs[0]);
      ID3D11ShaderResourceView* previous_srvs[MotionVectorPatches::resource_slots] = {};
      for (UINT slot = 0; slot < MotionVectorPatches::resource_slots; slot++)
      {
         previous_srvs[slot] = srvs[slot].get();
         const com_ptr<ID3D11Resource> resource = GetViewResource(srvs[slot].get());
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
         // Immutable resources (or none) never change
         if (usage == D3D11_USAGE_IMMUTABLE)
            continue;

         auto& entry = game_device_data.mv_previous_resources[resource.get()];
         if (entry.frame != game_device_data.mv_frame_present || !entry.copies[0])
         {
            entry.resource = resource;
            entry.has_previous = entry.copies[0] && entry.frame + 1 == game_device_data.mv_frame_present;
            std::swap(entry.copies[0], entry.copies[1]);
            std::swap(entry.views[0], entry.views[1]);
            std::swap(entry.view_descs[0], entry.view_descs[1]);
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
               native_device_context->CopyResource(entry.copies[0].get(), resource.get());
         }
         if (!entry.has_previous || !entry.copies[1])
            continue;
         D3D11_SHADER_RESOURCE_VIEW_DESC view_desc;
         srvs[slot]->GetDesc(&view_desc);
         if (!entry.views[1] || std::memcmp(&view_desc, &entry.view_descs[1], sizeof(view_desc)) != 0)
         {
            entry.views[1].reset();
            entry.view_descs[1] = view_desc;
            native_device->CreateShaderResourceView(entry.copies[1].get(), &view_desc, &entry.views[1]);
         }
         if (entry.views[1])
            previous_srvs[slot] = entry.views[1].get();
      }
      native_device_context->VSSetShaderResources(MotionVectorPatches::previous_resources_slot, MotionVectorPatches::resource_slots, previous_srvs);
      native_device_context->VSSetShader(vertex_shader.get(), nullptr, 0);
      native_device_context->PSSetShader(pixel_shader.get(), nullptr, 0);
      if (motion_vector_blend_state)
         native_device_context->OMSetBlendState(motion_vector_blend_state.get(), blend_factor, sample_mask);
      draw();
      if (motion_vector_blend_state)
         native_device_context->OMSetBlendState(blend_state.get(), blend_factor, sample_mask);
      // Set directly, bypassing Core's state tracking: restore the game's
      native_device_context->VSSetShader(original_vertex_shader.get(), nullptr, 0);
      native_device_context->PSSetShader(original_pixel_shader.get(), nullptr, 0);
      ID3D11Buffer* const restored_globals = original_previous_globals.get();
      native_device_context->VSSetConstantBuffers(MotionVectorPatches::previous_globals_slot, 1, &restored_globals);
      ID3D11Buffer* const restored_jitter = original_jitter.get();
      native_device_context->VSSetConstantBuffers(MotionVectorPatches::jitter_slot, 1, &restored_jitter);
      ID3D11ShaderResourceView* const null_srvs[MotionVectorPatches::resource_slots] = {};
      native_device_context->VSSetShaderResources(MotionVectorPatches::previous_resources_slot, MotionVectorPatches::resource_slots, null_srvs);
      native_device_context->OMSetRenderTargets(8, rtvs, dsv);
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

   // "mov_sat o0.xyzw, o0.xyzw" before the final ret: the clamp the vanilla UNORM swapchain applied to the UI
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
      GetShaderDefineData(UI_DRAW_TYPE_HASH).SetDefaultValue('2'); // The UI blends onto the swapchain after the composite

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
      native_shaders_definitions.emplace(CompileTimeStringHash("P5S Layer Quad VS"), ShaderDefinition{"Luma_P5S_LayerQuad", reshade::api::pipeline_subobject_type::vertex_shader});
      native_shaders_definitions.emplace(CompileTimeStringHash("P5S Layer Sprite VS"), ShaderDefinition{"Luma_P5S_LayerSprite", reshade::api::pipeline_subobject_type::vertex_shader});
      // XeGTAO passes (Luma_P5S_XeGTAO.hlsl); the two denoisers differ only by XE_GTAO_FINAL_APPLY.
      native_shaders_definitions.emplace(CompileTimeStringHash("P5S XeGTAO Prefilter Depths CS"), ShaderDefinition{"Luma_P5S_XeGTAO", reshade::api::pipeline_subobject_type::compute_shader, nullptr, "prefilter_depths16x16_cs"});
      native_shaders_definitions.emplace(CompileTimeStringHash("P5S XeGTAO Main Pass CS"), ShaderDefinition{"Luma_P5S_XeGTAO", reshade::api::pipeline_subobject_type::compute_shader, nullptr, "main_pass_cs"});
      native_shaders_definitions.emplace(CompileTimeStringHash("P5S XeGTAO Denoise Pass 1 CS"), ShaderDefinition{"Luma_P5S_XeGTAO", reshade::api::pipeline_subobject_type::compute_shader, nullptr, "denoise_pass_cs", {{"XE_GTAO_FINAL_APPLY", "0"}}});
      native_shaders_definitions.emplace(CompileTimeStringHash("P5S XeGTAO Denoise Pass 2 CS"), ShaderDefinition{"Luma_P5S_XeGTAO", reshade::api::pipeline_subobject_type::compute_shader, nullptr, "denoise_pass_cs", {{"XE_GTAO_FINAL_APPLY", "1"}}});

      reshade::register_event<reshade::addon_event::map_buffer_region>(OnMapBufferRegion);
      reshade::register_event<reshade::addon_event::unmap_buffer_region>(OnUnmapBufferRegion);
      reshade::register_event<reshade::addon_event::execute_secondary_command_list>(OnExecuteSecondaryCommandList);
   }

   static void UnregisterEvents()
   {
      reshade::unregister_event<reshade::addon_event::map_buffer_region>(OnMapBufferRegion);
      reshade::unregister_event<reshade::addon_event::unmap_buffer_region>(OnUnmapBufferRegion);
      reshade::unregister_event<reshade::addon_event::execute_secondary_command_list>(OnExecuteSecondaryCommandList);
   }

   // Draws the composite, then SMAA on its canvas (swapchain, upscaled canvas, or without DLSS/FSR below render scale 1 its own target,
   // which the game stretches onto the swapchain) before the UI: copy, gamma encode, predication, SMAA
   // into the gamma copy, finalize (RCAS, decode, dither) into the canvas; without RCAS, SMAA writes the canvas. With "smaa" false
   // (DLSS/FSR antialiased) only RCAS: copy, gamma encode, finalize. If anything is missing (shaders compiling, unexpected target) the
   // composite is left alone and the vanilla FXAA runs (not after DLSS/FSR).
   static DrawOrDispatchOverrideType DrawCompositeWithSMAAAndRCAS(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, CommandListData& cmd_list_data, DeviceData& device_data, bool* updated_cbuffers, const std::function<void()>& original_draw_dispatch_func, bool smaa)
   {
      auto& game_device_data = GetGameDeviceData(device_data);
      com_ptr<ID3D11RenderTargetView> canvas_rtv;
      native_device_context->OMGetRenderTargets(1, &canvas_rtv, nullptr);
      const com_ptr<ID3D11Resource> canvas_resource = GetViewResource(canvas_rtv.get());
      com_ptr<ID3D11Texture2D> canvas_texture;
      if (!canvas_resource || FAILED(canvas_resource->QueryInterface(&canvas_texture)))
         return DrawOrDispatchOverrideType::None;
      D3D11_TEXTURE2D_DESC canvas_desc;
      canvas_texture->GetDesc(&canvas_desc);
      // Not the main menu's second, off-screen composite (RGBA8, see the format check)
      if (!IsBackBuffer(&device_data, canvas_resource.get()) && canvas_rtv != game_device_data.sr_upscaled_canvas_rtv && (IsSRActive(device_data) || canvas_desc.Width >= uint32_t(device_data.output_resolution.x + 0.5f)))
         return DrawOrDispatchOverrideType::None;
      // The upgraded (linear fp16) swapchain, the SMAA copies' format
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
         // Core's "DrawSMAA" sizes its intermediates from its first target and rebuilds them only on swapchain init (as in MEA/MELE)
         auto& managed_resources = device_data.managed_resources;
         managed_resources.depth_stencil_views["smaa_dsv"_h].reset();
         managed_resources.render_target_views["smaa_edge_detection"_h].reset();
         managed_resources.render_target_views["smaa_blending_weight_calculation"_h].reset();
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

      // Predication depth, if captured and canvas sized; else plain ULTRA.
      com_ptr<ID3D11ShaderResourceView> depth_srv;
      if (smaa && g_smaa_predication && game_device_data.smaa_predication_uav && HasShaders(device_data.native_compute_shaders, "P5S SMAA Predication CS"_h))
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

      // The replaced composite reads the Luma cbuffers, which Core binds only after this callback; they stay bound for SMAA and finalize.
      // The data carries the canvas size (CustomData1/2: the swapchain's, or the render resolution) and the predication scale
      // (CustomData3), never 0 here, which also defers the composite's dither to the chain's end so RCAS doesn't sharpen it.
      SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, reshade::api::shader_stage::vertex | reshade::api::shader_stage::pixel, LumaConstantBufferType::LumaSettings);
      SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, reshade::api::shader_stage::vertex | reshade::api::shader_stage::pixel, LumaConstantBufferType::LumaData, canvas_desc.Width, canvas_desc.Height, depth_srv ? 2.f : 1.f);
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

      // Without RCAS, SMAA writes (and dithers) the canvas directly, only sampling the linear copy; finalize would only decode
      const bool sharpen = cb_luma_global_settings.GameSettings.RCASSharpness > 0.f;
      if (smaa)
         DrawSMAA(native_device, native_device_context, device_data, sharpen ? game_device_data.smaa_gamma_rtv.get() : canvas_rtv.get(), game_device_data.smaa_linear_srv.get(), game_device_data.smaa_gamma_srv.get(), depth_srv ? game_device_data.smaa_predication_srv.get() : nullptr);

      // Development builds may also draw a debug view
      if (sharpen || DEVELOPMENT)
      {
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
      }

      game_device_data.scene_antialiased = true;
      return DrawOrDispatchOverrideType::Replaced;
   }

   // XeGTAO in place of the SSAO calculate draw: prefilter, main pass and two denoisers on its inputs (t0 half res depth, t1 full res
   // normals, b0 $Globals with this frame's projection and radius), then a copy into its render target. False (the native draw runs) if
   // an input, shader or scratch is missing.
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
      const com_ptr<ID3D11Resource> target = GetViewResource(target_rtv.get());
      uint4 target_size, depth_size, normals_size;
      DXGI_FORMAT target_format, unused_format;
      GetResourceInfo(target.get(), target_size, target_format);
      GetResourceInfo(depth_srv.get(), depth_size, unused_format);
      GetResourceInfo(normals_srv.get(), normals_size, unused_format);
      const uint32_t width = target_size.x;
      const uint32_t height = target_size.y;
      // The final denoiser's R8_UNORM output is copied into the target, so the formats must match
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
         com_ptr<ID3D11Texture2D> depth_mips_texture;
         bool ok = SUCCEEDED(native_device->CreateTexture2D(&desc, nullptr, &depth_mips_texture)) && SUCCEEDED(native_device->CreateShaderResourceView(depth_mips_texture.get(), nullptr, &game_device_data.gtao_depth_mips_srv));
         D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc = {};
         uav_desc.Format = desc.Format;
         uav_desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
         for (UINT mip = 0; ok && mip < 5; mip++)
         {
            uav_desc.Texture2D.MipSlice = mip;
            ok = SUCCEEDED(native_device->CreateUnorderedAccessView(depth_mips_texture.get(), &uav_desc, &game_device_data.gtao_depth_mip_uavs[mip]));
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

      const float knobs[8] = {g_gtao_final_value_power, float(normal_input_scale), g_gtao_radius_override, float(g_gtao_debug_view), 1.f / float(width), 1.f / float(height), 0.f, 0.f};
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
      // $Globals may be a range of a bigger buffer; a zero count means a plain binding
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

      // The target is still the draw's render target, and the runtime doesn't resolve copies into OM bound resources: unbind around the
      // copy.
      native_device_context->OMSetRenderTargets(0, nullptr, nullptr);
      native_device_context->CopyResource(target.get(), game_device_data.gtao_final_texture.get());
      ID3D11RenderTargetView* const rtv = target_rtv.get();
      native_device_context->OMSetRenderTargets(1, &rtv, target_dsv.get());
      return true;
   }

   // For the game's "FinishCommandList" (command list, then deferred context) and "ExecuteCommandList" (immediate context, then command
   // list, before it runs). A list finished after a split is the scene's remainder: on its execution the split's first part runs, then
   // DLSS/FSR writes the scene its post process reads.
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
      com_ptr<ID3D11Device> native_device;
      native_device_context->GetDevice(&native_device);
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
#if DEVELOPMENT
      // The scene's start timestamp already ran with the game's earlier command lists; the disjoint query only covers the rest
      auto* const perf_queries = split.perf_queries;
      if (perf_queries)
         native_device_context->Begin(perf_queries->disjoint.get());
#endif
      // Always, even if the upscaler went away meanwhile: it starts the game's frame
      native_device_context->ExecuteCommandList(split.partial.get(), FALSE);
#if DEVELOPMENT
      if (perf_queries)
         native_device_context->End(perf_queries->sr_start.get());
#endif

      D3D11_TEXTURE2D_DESC desc;
      split.source_color->GetDesc(&desc);
      if (IsSRActive(*device_data) && game_device_data.mv_texture)
      {
         // Written as a UAV. Upscaling: into the split's output, read by the post process; DLAA: copied back into the scene.
         D3D11_TEXTURE2D_DESC output_desc = {};
         if (!split.output_color)
         {
            if (device_data->sr_output_color)
               device_data->sr_output_color->GetDesc(&output_desc);
            if (output_desc.Width != desc.Width || output_desc.Height != desc.Height)
            {
               device_data->sr_output_color.reset();
               output_desc = {desc.Width, desc.Height, 1, 1, DXGI_FORMAT_R16G16B16A16_FLOAT, {1, 0}, D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS};
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
         // As applied (pixels, +y down): the opposite sign shakes upscaled frames
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
               // Also scaled down into the scene, else post passes reading it at render resolution (DOF, bloom, exposure) see the raw jittered
               // frame and the DOF merge blends a shaking blur into the output
               com_ptr<ID3D11ShaderResourceView> output_srv;
               com_ptr<ID3D11RenderTargetView> scene_rtv;
               const D3D11_RENDER_TARGET_VIEW_DESC scene_rtv_desc = {DXGI_FORMAT_R16G16B16A16_FLOAT, D3D11_RTV_DIMENSION_TEXTURE2D};
               const std::shared_lock lock_shader_objects(s_mutex_shader_objects);
               const com_ptr<ID3D11VertexShader> scale_vertex_shader = FindShader(device_data->native_vertex_shaders, "Scale VS"_h);
               const com_ptr<ID3D11PixelShader> scale_pixel_shader = FindShader(device_data->native_pixel_shaders, "Scale PS"_h);
               if (scale_vertex_shader && scale_pixel_shader && SUCCEEDED(native_device->CreateShaderResourceView(output_color, nullptr, &output_srv)) && SUCCEEDED(native_device->CreateRenderTargetView(split.source_color.get(), &scene_rtv_desc, &scene_rtv)))
                  DrawCustomPixelShader(native_device_context.get(), device_data->default_depth_stencil_state.get(), device_data->default_blend_state.get(), device_data->sampler_state_linear.get(), scale_vertex_shader.get(), scale_pixel_shader.get(), output_srv.get(), scene_rtv.get(), desc.Width, desc.Height);
            }
            device_data->has_drawn_sr = true;
         }
         else
         {
            // Back to SMAA until the upscaler is picked again
            device_data->sr_suppressed = true;
         }
      }
#if DEVELOPMENT
      if (perf_queries)
      {
         native_device_context->End(perf_queries->sr_end.get());
         native_device_context->End(perf_queries->disjoint.get());
         const std::lock_guard lock(game_device_data.perf_mutex);
         perf_queries->pending = true;
      }
#endif
      draw_state.Restore(native_device_context.get());
      compute_state.Restore(native_device_context.get());
   }

   // Byte offset of "fClstScl" in a pixel shader's $Globals (b0) by original hash, UINT_MAX if none
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

   // Render scale of a layer output from this frame (a deferred context can record just past the present), else 0 (e.g. stale from a
   // previous pause screen). The caller holds "layer_mutex".
   static float GetRecentLayerScale(const std::unordered_map<ID3D11Resource*, Persona5StrikersGameDeviceData::LayerOutput>& outputs, ID3D11Resource* resource)
   {
      const auto it = outputs.find(resource);
      return it != outputs.end() && cb_luma_global_settings.FrameIndex - it->second.frame_index <= 1 ? it->second.scale : 0.f;
   }

   // The layer vertex shaders' uv scale (1 / render scale) at "layer_uv_scale_cb_slot" (see "Includes/LayerCorner.hlsl")
   static com_ptr<ID3D11Buffer> GetLayerUVScaleBuffer(ID3D11Device* native_device, Persona5StrikersGameDeviceData* game_device_data, const float scale[2])
   {
      const std::lock_guard lock(game_device_data->layer_mutex);
      if (!game_device_data->layer_uv_scale_buffer || game_device_data->layer_uv_scale[0] != scale[0] || game_device_data->layer_uv_scale[1] != scale[1])
      {
         const float uv_scale[4] = {1.f / scale[0], 1.f / scale[1], 0.f, 0.f};
         const D3D11_BUFFER_DESC desc = {sizeof(uv_scale), D3D11_USAGE_IMMUTABLE, D3D11_BIND_CONSTANT_BUFFER};
         const D3D11_SUBRESOURCE_DATA data = {uv_scale};
         game_device_data->layer_uv_scale_buffer.reset();
         if (FAILED(native_device->CreateBuffer(&desc, &data, &game_device_data->layer_uv_scale_buffer)))
            return nullptr;
         std::copy_n(scale, 2, game_device_data->layer_uv_scale);
      }
      return game_device_data->layer_uv_scale_buffer;
   }

   // Upscaling: 3D layers outside the scene frame (main menu and pause screen characters, outlines, translucents) draw through a render
   // resolution viewport into output sized targets, then a stretch (0x99A76DC2) scales that corner over the target, bypassing DLSS/FSR.
   // Their draws get the whole target instead, and the stretch copies 1:1. Rescaled to match: the quads' texture coordinates (from their
   // vertices) and the lit pixel shaders' light cluster index (pixel position * "fClstScl" / 64). False if not such a draw (it then runs
   // untouched).
   static bool DrawLayerAtOutputResolution(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, CommandListData& cmd_list_data, DeviceData& device_data, const ShaderHashesList<OneShaderPerPipeline>& original_shader_hashes, const std::function<void()>& draw)
   {
      auto& game_device_data = GetGameDeviceData(device_data);
      const bool stretch = original_shader_hashes.Contains(layer_stretch_hash, reshade::api::shader_stage::pixel) && original_shader_hashes.Contains(quad_vertex_shader_hash, reshade::api::shader_stage::vertex);
      D3D11_VIEWPORT viewport = {};
      UINT viewports = 1;
      native_device_context->RSGetViewports(&viewports, &viewport);
      const uint2 output_size = {uint32_t(device_data.output_resolution.x + 0.5f), uint32_t(device_data.output_resolution.y + 0.5f)};
      // Most draws (the scene's included) stop here. With an output sized target this is the render scale, equal in both axes (not a panel).
      float scale[2] = {viewport.Width / float(output_size.x), viewport.Height / float(output_size.y)};
      if (!stretch && (viewports == 0 || viewport.TopLeftX != 0.f || viewport.TopLeftY != 0.f || viewport.Width >= float(output_size.x) || scale[0] <= 0.f || std::abs(scale[0] - scale[1]) > 0.01f))
         return false;
      D3D11_TEXTURE2D_DESC target_desc = {};
      com_ptr<ID3D11Resource> target;
      if (stretch)
      {
         // From a target the layer drew whole
         com_ptr<ID3D11ShaderResourceView> srv;
         native_device_context->PSGetShaderResources(0, 1, &srv);
         const com_ptr<ID3D11Resource> source = GetViewResource(srv.get());
         const std::lock_guard lock(game_device_data.layer_mutex);
         const auto it = game_device_data.layer_frames.find(native_device_context);
         if (!source || it == game_device_data.layer_frames.end() || std::ranges::find(it->second.targets, source.get()) == it->second.targets.end())
            return false;
         std::copy_n(it->second.scale, 2, scale);
         game_device_data.layer_frames.erase(it);
         com_ptr<ID3D11RenderTargetView> rtv;
         native_device_context->OMGetRenderTargets(1, &rtv, nullptr);
         if (const com_ptr<ID3D11Resource> stretch_target = GetViewResource(rtv.get()))
            game_device_data.layer_outputs[stretch_target.get()] = {scale[0], cb_luma_global_settings.FrameIndex};
      }
      else
      {
         com_ptr<ID3D11RenderTargetView> rtv;
         com_ptr<ID3D11DepthStencilView> dsv;
         native_device_context->OMGetRenderTargets(1, &rtv, &dsv);
         target = GetViewResource(rtv.get());
         if (com_ptr<ID3D11Texture2D> texture; target && SUCCEEDED(target->QueryInterface(&texture)))
            texture->GetDesc(&target_desc);
         // An output sized off-screen target, with the viewport at its top left corner
         if (target_desc.Width != output_size.x || target_desc.Height != output_size.y || IsBackBuffer(&device_data, target.get()))
            return false;
         if (dsv)
         {
            uint4 depth_size;
            DXGI_FORMAT depth_format;
            GetResourceInfo(GetViewResource(dsv.get()).get(), depth_size, depth_format);
            if (depth_size.x != target_desc.Width || depth_size.y != target_desc.Height)
               return false;
         }
      }

      // All or nothing: otherwise the layer's quads, swapchain sprite and exposure would read a corner of what it drew
      com_ptr<ID3D11VertexShader> quad_vertex_shader;
      {
         const std::shared_lock lock_shader_objects(s_mutex_shader_objects);
         quad_vertex_shader = FindShader(device_data.native_vertex_shaders, "P5S Layer Quad VS"_h);
         if (!quad_vertex_shader || !HasShaders(device_data.native_vertex_shaders, "P5S Layer Sprite VS"_h, "Scale VS"_h) || !HasShaders(device_data.native_pixel_shaders, "Scale PS"_h))
            return false;
         if (!original_shader_hashes.Contains(quad_vertex_shader_hash, reshade::api::shader_stage::vertex))
            quad_vertex_shader.reset();
      }
      com_ptr<ID3D11Buffer> uv_scale_buffer;
      if (quad_vertex_shader)
      {
         uv_scale_buffer = GetLayerUVScaleBuffer(native_device, &game_device_data, scale);
         if (!uv_scale_buffer)
            return false;
      }

      // The lit pixel shaders' $Globals with the cluster scale matching the pixel positions. Until the buffer's first CPU copy the game's is
      // kept, so point lights use the wrong clusters for a frame.
      com_ptr<ID3D11Buffer> original_globals;
      com_ptr<ID3D11Buffer> patched_globals;
      const UINT cluster_scale_offset = stretch ? UINT_MAX : GetClusterScaleOffset(device_data, original_shader_hashes.pixel_shaders[0], cmd_list_data.pipeline_state_original_pixel_shader);
      if (cluster_scale_offset != UINT_MAX)
      {
         native_device_context->PSGetConstantBuffers(0, 1, &original_globals);
         std::vector<uint8_t> globals_data = original_globals ? GetGlobalsCopy(&game_device_data, original_globals.get()) : std::vector<uint8_t>{};
         if (cluster_scale_offset + sizeof(float) <= globals_data.size())
         {
            float cluster_scale;
            std::memcpy(&cluster_scale, globals_data.data() + cluster_scale_offset, sizeof(float));
            cluster_scale *= scale[0];
            std::memcpy(globals_data.data() + cluster_scale_offset, &cluster_scale, sizeof(float));
            const std::lock_guard lock(game_device_data.layer_mutex);
            com_ptr<ID3D11Buffer>& upload = game_device_data.layer_globals_buffers[UINT(globals_data.size())];
            if (WriteConstants(native_device, native_device_context, std::addressof(upload), globals_data.data(), UINT(globals_data.size())))
               patched_globals = upload;
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

      // Set directly (bypassing Core's state tracking), then restored
      D3D11_RECT scissor = {};
      UINT scissors = 1;
      native_device_context->RSGetScissorRects(&scissors, &scissor);
      if (!stretch)
      {
         const D3D11_VIEWPORT full_viewport = {0.f, 0.f, float(target_desc.Width), float(target_desc.Height), viewport.MinDepth, viewport.MaxDepth};
         const D3D11_RECT full_scissor = {0, 0, LONG(target_desc.Width), LONG(target_desc.Height)};
         native_device_context->RSSetViewports(1, &full_viewport);
         native_device_context->RSSetScissorRects(1, &full_scissor);
      }
      if (patched_globals)
      {
         ID3D11Buffer* const buffer = patched_globals.get();
         native_device_context->PSSetConstantBuffers(0, 1, &buffer);
      }
      DrawWithLayerVertexShader(native_device_context, quad_vertex_shader.get(), uv_scale_buffer.get(), draw);
      if (!stretch)
      {
         native_device_context->RSSetViewports(1, &viewport);
         native_device_context->RSSetScissorRects(scissors, &scissor);
      }
      if (patched_globals)
      {
         ID3D11Buffer* const buffer = original_globals.get();
         native_device_context->PSSetConstantBuffers(0, 1, &buffer);
      }
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

      if (IsSRActive(device_data) && (stages & reshade::api::shader_stage::vertex) == reshade::api::shader_stage::vertex && original_draw_dispatch_func && *original_draw_dispatch_func)
      {
#if DEVELOPMENT
         // Includes recording the draw itself (patched or not)
         const PerfHookTimer timer{game_device_data.perf_hook_ns};
#endif
         if (original_shader_hashes.Contains(post_process_start_shader_hashes))
         {
            if (native_device_context == game_device_data.mv_scene_context && !game_device_data.mv_frame_ended)
               EndMotionVectorFrame(native_device, native_device_context, device_data);
         }
         else
         {
            com_ptr<ID3D11RenderTargetView> rtvs[8];
            com_ptr<ID3D11DepthStencilView> dsv;
            native_device_context->OMGetRenderTargets(8, &rtvs[0], &dsv);
            // The G-buffer (5 targets), or an opaque forward mesh draw into the scene (outlines, sky: 1 target, depth written)
            const bool gbuffer = std::all_of(rtvs, rtvs + 5, [](const auto& rtv)
               { return rtv.get() != nullptr; });
            D3D11_DEPTH_STENCIL_DESC depth_desc = {};
            com_ptr<ID3D11Buffer> vertex_buffer;
            // Forward draws join a started frame on the scene context; outside a frame only a depth prepass (no targets) starts one
            const bool scene_draw = game_device_data.mv_frame_ended ? !rtvs[0] : native_device_context == game_device_data.mv_scene_context;
            if (!gbuffer && dsv && scene_draw)
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
            // Any other mesh depth tested against the scene: the depth prepass (no targets), or depth test only geometry
            const bool depth_prepass = !rtvs[0] && depth_write;
            if (!gbuffer && depth_desc.DepthEnable && vertex_buffer && DrawWithJitter(native_device, native_device_context, cmd_list_data, device_data, original_shader_hashes, dsv.get(), depth_prepass, *original_draw_dispatch_func))
               return DrawOrDispatchOverrideType::Replaced;
         }
      }

      if (original_shader_hashes.Contains(layer_stretch_hash, reshade::api::shader_stage::pixel))
         game_device_data.layer_drawn = true;
      if (IsSRActive(device_data) && (stages & reshade::api::shader_stage::vertex) == reshade::api::shader_stage::vertex && original_draw_dispatch_func && *original_draw_dispatch_func && DrawLayerAtOutputResolution(native_device, native_device_context, cmd_list_data, device_data, original_shader_hashes, *original_draw_dispatch_func))
         return DrawOrDispatchOverrideType::Replaced;

      // The exposure histogram reads a layer stretch target's render resolution corner by pixel: give it the target downscaled to render
      // resolution
      if (original_shader_hashes.Contains(exposure_histogram_hash, reshade::api::shader_stage::compute) && original_draw_dispatch_func && *original_draw_dispatch_func)
      {
         com_ptr<ID3D11ShaderResourceView> layer_srv;
         native_device_context->CSGetShaderResources(0, 1, &layer_srv);
         const com_ptr<ID3D11Resource> layer = GetViewResource(layer_srv.get());
         com_ptr<ID3D11RenderTargetView> histogram_rtv;
         com_ptr<ID3D11ShaderResourceView> histogram_srv;
         UINT width = 0;
         UINT height = 0;
         if (layer)
         {
            const std::lock_guard lock(game_device_data.layer_mutex);
            if (const float scale = GetRecentLayerScale(game_device_data.layer_outputs, layer.get()); scale > 0.f)
            {
               uint4 layer_size;
               DXGI_FORMAT layer_format;
               GetResourceInfo(layer.get(), layer_size, layer_format);
               width = UINT(float(layer_size.x) * scale + 0.5f);
               height = UINT(float(layer_size.y) * scale + 0.5f);
               uint4 histogram_size = {};
               DXGI_FORMAT histogram_format;
               if (game_device_data.layer_histogram_srv)
                  GetResourceInfo(game_device_data.layer_histogram_srv.get(), histogram_size, histogram_format);
               if (histogram_size.x != width || histogram_size.y != height)
               {
                  game_device_data.layer_histogram_rtv.reset();
                  game_device_data.layer_histogram_srv.reset();
                  const D3D11_TEXTURE2D_DESC desc = {width, height, 1, 1, DXGI_FORMAT_R16G16B16A16_FLOAT, {1, 0}, D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET};
                  com_ptr<ID3D11Texture2D> texture;
                  if (FAILED(native_device->CreateTexture2D(&desc, nullptr, &texture)) || FAILED(native_device->CreateRenderTargetView(texture.get(), nullptr, &game_device_data.layer_histogram_rtv)) || FAILED(native_device->CreateShaderResourceView(texture.get(), nullptr, &game_device_data.layer_histogram_srv)))
                  {
                     game_device_data.layer_histogram_rtv.reset();
                     game_device_data.layer_histogram_srv.reset();
                  }
               }
               histogram_rtv = game_device_data.layer_histogram_rtv;
               histogram_srv = game_device_data.layer_histogram_srv;
            }
         }
         com_ptr<ID3D11VertexShader> downsample_vertex_shader;
         com_ptr<ID3D11PixelShader> downsample_pixel_shader;
         if (histogram_srv)
         {
            const std::shared_lock lock_shader_objects(s_mutex_shader_objects);
            downsample_vertex_shader = FindShader(device_data.native_vertex_shaders, "Scale VS"_h);
            downsample_pixel_shader = FindShader(device_data.native_pixel_shaders, "Scale PS"_h);
         }
         if (downsample_vertex_shader && downsample_pixel_shader)
         {
            {
               DrawStateStack<DrawStateStackType::FullGraphics> state;
               state.Cache(native_device_context, device_data.uav_max_count);
               DrawCustomPixelShader(native_device_context, device_data.default_depth_stencil_state.get(), device_data.default_blend_state.get(), device_data.sampler_state_linear.get(), downsample_vertex_shader.get(), downsample_pixel_shader.get(), layer_srv.get(), histogram_rtv.get(), width, height);
               state.Restore(native_device_context);
            }
            ID3D11ShaderResourceView* srv = histogram_srv.get();
            native_device_context->CSSetShaderResources(0, 1, &srv);
            (*original_draw_dispatch_func)();
            srv = layer_srv.get();
            native_device_context->CSSetShaderResources(0, 1, &srv);
            return DrawOrDispatchOverrideType::Replaced;
         }
      }

      if (g_gtao_enable && original_shader_hashes.Contains(ssao_hash, reshade::api::shader_stage::pixel))
         return RunXeGTAO(native_device, native_device_context, device_data) ? DrawOrDispatchOverrideType::Replaced : DrawOrDispatchOverrideType::None;

      // DLSS/FSR before the scene's first post pass: split its command list here, the upscaler runs between the parts
      if (game_device_data.sr_split_ready && native_device_context == game_device_data.mv_scene_context && IsSRActive(device_data) && original_shader_hashes.Contains(post_process_start_shader_hashes))
      {
         game_device_data.sr_split_ready = false;
         com_ptr<ID3D11ShaderResourceView> scene_srv;
         native_device_context->PSGetShaderResources(0, 1, &scene_srv);
         const com_ptr<ID3D11Resource> scene = GetViewResource(scene_srv.get());
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
            const auto& view_projection = game_device_data.mv_view_projection;
            split.vertical_fov = 2.f * std::atan(1.f / std::sqrt(view_projection[1] * view_projection[1] + view_projection[5] * view_projection[5] + view_projection[9] * view_projection[9]));
         }
         // Upscaling when the game renders below output resolution (its render scale option): output and canvas at output resolution
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
               game_device_data.sr_upscaled_canvas_rtv.reset();
               game_device_data.sr_upscaled_canvas_srv.reset();
               output_desc = {output_size.x, output_size.y, 1, 1, DXGI_FORMAT_R16G16B16A16_FLOAT, {1, 0}, D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET | D3D11_BIND_UNORDERED_ACCESS};
               bool created = SUCCEEDED(native_device->CreateTexture2D(&output_desc, nullptr, &game_device_data.sr_upscaled_output)) && SUCCEEDED(native_device->CreateRenderTargetView(game_device_data.sr_upscaled_output.get(), nullptr, &game_device_data.sr_upscaled_output_rtv)) && SUCCEEDED(native_device->CreateShaderResourceView(game_device_data.sr_upscaled_output.get(), nullptr, &game_device_data.sr_upscaled_output_srv));
               output_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
               com_ptr<ID3D11Texture2D> canvas;
               created = created && SUCCEEDED(native_device->CreateTexture2D(&output_desc, nullptr, &canvas)) && SUCCEEDED(native_device->CreateRenderTargetView(canvas.get(), nullptr, &game_device_data.sr_upscaled_canvas_rtv)) && SUCCEEDED(native_device->CreateShaderResourceView(canvas.get(), nullptr, &game_device_data.sr_upscaled_canvas_srv));
               // Retried next frame (the size check sees no output); meanwhile the upscaler runs at render resolution and the game stretches it
               if (!created)
               {
                  game_device_data.sr_upscaled_output.reset();
                  game_device_data.sr_upscaled_canvas_srv.reset();
               }
#if DEVELOPMENT
               reshade::log::message(created ? reshade::log::level::info : reshade::log::level::warning, std::format("[P5S SR] upscaling {}x{} -> {}x{}{}", scene_desc.Width, scene_desc.Height, output_size.x, output_size.y, created ? "" : " failed").c_str());
#endif
            }
            if (game_device_data.sr_upscaled_canvas_srv)
               split.output_color = game_device_data.sr_upscaled_output;
         }
         // The upgraded scene: motion vector sized, not multisampled
         if (split.depth && native_device_context->GetType() == D3D11_DEVICE_CONTEXT_DEFERRED && scene_desc.Width == mv_size.x && scene_desc.Height == mv_size.y && scene_desc.SampleDesc.Count == 1 && (scene_desc.Format == DXGI_FORMAT_R16G16B16A16_FLOAT || scene_desc.Format == DXGI_FORMAT_R16G16B16A16_TYPELESS) && SUCCEEDED(native_device_context->FinishCommandList(TRUE, &split.partial)))
         {
            const std::lock_guard lock(game_device_data.sr_mutex);
            // The game doesn't know its command list restarted: its next "D3D11_MAP_WRITE_NO_OVERWRITE" map fails without a discard first
            // (dialogue text went missing). Data appended before the split is lost, as in other mods that split command lists.
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
#if DEVELOPMENT
            {
               const std::lock_guard perf_lock(game_device_data.perf_mutex);
               split.perf_queries = std::exchange(game_device_data.perf_frame_queries, nullptr);
            }
#endif
            game_device_data.sr_split = std::move(split);
            game_device_data.sr_split_context = reinterpret_cast<uint64_t>(native_device_context);
            game_device_data.scene_antialiased = true;
         }
      }

      // Upscaled: scene blending post passes also blend into the upscaler's output. The render resolution scene stays vanilla for later
      // readers (bloom prefilter, exposure histogram by pixel).
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
         // Below render scale 1 the composite draws into its own target, which the game stretches onto the swapchain
         {
            com_ptr<ID3D11RenderTargetView> rtv;
            native_device_context->OMGetRenderTargets(1, &rtv, nullptr);
            const com_ptr<ID3D11Resource> target = GetViewResource(rtv.get());
            game_device_data.composite_target.reset();
            if (target && !IsBackBuffer(&device_data, target.get()))
            {
               game_device_data.composite_target = target;
               uint4 target_size;
               DXGI_FORMAT target_format;
               GetResourceInfo(target.get(), target_size, target_format);
               if (target_size.x < uint32_t(device_data.output_resolution.x + 0.5f))
                  game_device_data.render_resolution_composite = true;

               // A 3D layer's composite, from its stretch target: see "layer_composed"
               com_ptr<ID3D11ShaderResourceView> scene_srv;
               native_device_context->PSGetShaderResources(0, 1, &scene_srv);
               float layer_scale = 0.f;
               if (const com_ptr<ID3D11Resource> scene = GetViewResource(scene_srv.get()))
               {
                  const std::lock_guard lock(game_device_data.layer_mutex);
                  layer_scale = GetRecentLayerScale(game_device_data.layer_outputs, scene.get());
                  if (layer_scale > 0.f)
                     game_device_data.layer_composed[target.get()] = {layer_scale, cb_luma_global_settings.FrameIndex};
               }
               // It draws the render resolution corner (for the sprite to scale up), cut by scissor or viewport: draw the whole target from the
               // whole scene instead, rescaling a corner viewport's scene coordinates by "LumaData.CustomData4" (scene samples only).
               D3D11_VIEWPORT viewport = {};
               UINT viewports = 1;
               native_device_context->RSGetViewports(&viewports, &viewport);
               D3D11_RECT scissor = {};
               UINT scissors = 1;
               native_device_context->RSGetScissorRects(&scissors, &scissor);
               if (layer_scale > 0.f && viewports != 0 && viewport.Width > 0.f && (viewport.Width < float(target_size.x) || (scissors != 0 && scissor.right < LONG(target_size.x))))
               {
                  const D3D11_VIEWPORT full_viewport = {0.f, 0.f, float(target_size.x), float(target_size.y), viewport.MinDepth, viewport.MaxDepth};
                  const D3D11_RECT full_scissor = {0, 0, LONG(target_size.x), LONG(target_size.y)};
                  native_device_context->RSSetViewports(1, &full_viewport);
                  native_device_context->RSSetScissorRects(1, &full_scissor);
                  SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, reshade::api::shader_stage::pixel, LumaConstantBufferType::LumaSettings);
                  SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, reshade::api::shader_stage::pixel, LumaConstantBufferType::LumaData, 0, 0, 0.f, float(target_size.x) / viewport.Width);
                  updated_cbuffers = true;
                  (*original_draw_dispatch_func)();
                  native_device_context->RSSetViewports(1, &viewport);
                  native_device_context->RSSetScissorRects(scissors, &scissor);
                  return DrawOrDispatchOverrideType::Replaced;
               }
            }
         }
         // Upscaled: from the upscaler's output into the output resolution canvas (RCAS included), which the stretch copies 1:1
         if (native_device_context == game_device_data.sr_upscaling_context)
         {
            DrawStateStack<DrawStateStackType::FullGraphics> state;
            state.Cache(native_device_context, device_data.uav_max_count);
            D3D11_TEXTURE2D_DESC desc;
            game_device_data.sr_upscaled_output->GetDesc(&desc); // The canvas' size
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
            // Also into the game's own target, which the pause screen freezes as its background (else a stale frame)
            SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, reshade::api::shader_stage::pixel, LumaConstantBufferType::LumaSettings);
            SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, reshade::api::shader_stage::pixel, LumaConstantBufferType::LumaData);
            (*original_draw_dispatch_func)();
            return DrawOrDispatchOverrideType::Replaced;
         }
         // SMAA not with DLSS/FSR (not even on composites they skip, like the pause screen's), RCAS after either
         if (cb_luma_global_settings.GameSettings.SMAAEnable > 0.5f && !IsSRActive(device_data) && !game_device_data.scene_antialiased)
            return DrawCompositeWithSMAAAndRCAS(native_device, native_device_context, cmd_list_data, device_data, &updated_cbuffers, *original_draw_dispatch_func, true);
         if (game_device_data.scene_antialiased && cb_luma_global_settings.GameSettings.RCASSharpness > 0.f)
            return DrawCompositeWithSMAAAndRCAS(native_device, native_device_context, cmd_list_data, device_data, &updated_cbuffers, *original_draw_dispatch_func, false);
         return DrawOrDispatchOverrideType::None;
      }

      // The copy of a 3D layer's alpha into its (pause screen) composite's output: its quad covers the render resolution corner (full
      // viewport, corner positions and texture coordinates). Drawn over the whole target instead, like the composite.
      if (original_shader_hashes.Contains(copy_hash, reshade::api::shader_stage::pixel) && original_shader_hashes.Contains(quad_vertex_shader_hash, reshade::api::shader_stage::vertex) && original_draw_dispatch_func && *original_draw_dispatch_func)
      {
         com_ptr<ID3D11RenderTargetView> rtv;
         native_device_context->OMGetRenderTargets(1, &rtv, nullptr);
         const com_ptr<ID3D11Resource> target = GetViewResource(rtv.get());
         float scale[2] = {};
         if (target)
         {
            const std::lock_guard lock(game_device_data.layer_mutex);
            scale[0] = scale[1] = GetRecentLayerScale(game_device_data.layer_composed, target.get());
         }
         com_ptr<ID3D11VertexShader> quad_vertex_shader;
         if (scale[0] > 0.f)
         {
            const std::shared_lock lock_shader_objects(s_mutex_shader_objects);
            quad_vertex_shader = FindShader(device_data.native_vertex_shaders, "P5S Layer Quad VS"_h);
         }
         const com_ptr<ID3D11Buffer> uv_scale_buffer = quad_vertex_shader ? GetLayerUVScaleBuffer(native_device, &game_device_data, scale) : nullptr;
         D3D11_VIEWPORT viewport = {};
         UINT viewports = 1;
         native_device_context->RSGetViewports(&viewports, &viewport);
         if (uv_scale_buffer && viewports != 0)
         {
            uint4 target_size;
            DXGI_FORMAT target_format;
            GetResourceInfo(target.get(), target_size, target_format);
            D3D11_RECT scissor = {};
            UINT scissors = 1;
            native_device_context->RSGetScissorRects(&scissors, &scissor);
            const D3D11_VIEWPORT whole_viewport = {viewport.TopLeftX / scale[0], viewport.TopLeftY / scale[1], viewport.Width / scale[0], viewport.Height / scale[1], viewport.MinDepth, viewport.MaxDepth};
            const D3D11_RECT whole_scissor = {0, 0, LONG(target_size.x), LONG(target_size.y)};
            native_device_context->RSSetViewports(1, &whole_viewport);
            native_device_context->RSSetScissorRects(1, &whole_scissor);
            DrawWithLayerVertexShader(native_device_context, quad_vertex_shader.get(), uv_scale_buffer.get(), *original_draw_dispatch_func);
            native_device_context->RSSetViewports(1, &viewport);
            native_device_context->RSSetScissorRects(scissors, &scissor);
            return DrawOrDispatchOverrideType::Replaced;
         }
      }

      // The game's stretch of the composite target onto the swapchain (render scales below 1) is scene, not UI. Upscaled, it copies the canvas 1:1.
      if (game_device_data.composite_target && original_shader_hashes.Contains(copy_hash, reshade::api::shader_stage::pixel) && original_draw_dispatch_func && *original_draw_dispatch_func)
      {
         com_ptr<ID3D11ShaderResourceView> game_srv;
         native_device_context->PSGetShaderResources(0, 1, &game_srv);
         if (const com_ptr<ID3D11Resource> source = GetViewResource(game_srv.get()); source && source == game_device_data.composite_target)
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

      // Everything drawn onto the swapchain after the composite is UI (HUD, menus, dialogue boxes, fades), except FXAA. Checked after the
      // composite, so a stale flag can't stop the scene drawing.
      if (device_data.has_drawn_main_post_processing && (stages & reshade::api::shader_stage::pixel) == reshade::api::shader_stage::pixel && !original_shader_hashes.Contains(fxaa_hash, reshade::api::shader_stage::pixel) && !original_shader_hashes.Contains(shader_hashes_apply_fxaa))
      {
         com_ptr<ID3D11RenderTargetView> rtv;
         native_device_context->OMGetRenderTargets(1, &rtv, nullptr);
         if (const com_ptr<ID3D11Resource> rtv_resource = GetViewResource(rtv.get()); rtv_resource && IsBackBuffer(&device_data, rtv_resource.get()))
         {
            if (g_hide_ui)
               return DrawOrDispatchOverrideType::Skip;

            // The sprite putting a 3D layer's composite on the swapchain scales up its render resolution corner: draw all of it instead
            if (original_shader_hashes.Contains(ui_sprite_vertex_shader_hash, reshade::api::shader_stage::vertex) && original_draw_dispatch_func && *original_draw_dispatch_func)
            {
               com_ptr<ID3D11ShaderResourceView> layer_srv;
               native_device_context->PSGetShaderResources(0, 1, &layer_srv);
               float scale[2] = {};
               if (const com_ptr<ID3D11Resource> layer = GetViewResource(layer_srv.get()))
               {
                  const std::lock_guard lock(game_device_data.layer_mutex);
                  scale[0] = scale[1] = GetRecentLayerScale(game_device_data.layer_composed, layer.get());
               }
               com_ptr<ID3D11VertexShader> sprite_vertex_shader;
               if (scale[0] > 0.f)
               {
                  const std::shared_lock lock_shader_objects(s_mutex_shader_objects);
                  sprite_vertex_shader = FindShader(device_data.native_vertex_shaders, "P5S Layer Sprite VS"_h);
               }
               const com_ptr<ID3D11Buffer> uv_scale_buffer = sprite_vertex_shader ? GetLayerUVScaleBuffer(native_device, &game_device_data, scale) : nullptr;
               if (uv_scale_buffer)
               {
                  DrawWithLayerVertexShader(native_device_context, sprite_vertex_shader.get(), uv_scale_buffer.get(), *original_draw_dispatch_func);
                  return DrawOrDispatchOverrideType::Replaced;
               }
            }

            // UI blends reading the swapchain saw it clamped to 0-1 by the vanilla UNORM target; in fp16, earlier additive UI (e.g. the menu
            // cursor's RGB cards, alpha 1 + 1 + 1) exceeds 1: the cursor's reverse subtracted text vanished and destination alpha masks (HUD,
            // main menu) read alphas up to 2. So clamp first with the same draw (own geometry and stencil), a white pixel shader and a MIN blend:
            // all channels before a color subtract, else only the never displayed alpha, keeping the HDR scene's color range. Subtracts are floored
            // after (dialogue bubbles reached -1.5) with a black pixel shader and a MAX blend; additive UI is clamped after to the display's peak
            // (vanilla clipped at 1), likewise.
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
                     white_pixel_shader = FindShader(device_data.native_pixel_shaders, "P5S Draw White PS"_h);
                     black_pixel_shader = FindShader(device_data.native_pixel_shaders, "P5S Draw Black PS"_h);
                     peak_pixel_shader = FindShader(device_data.native_pixel_shaders, "P5S UI Peak Clamp PS"_h);
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

      // SMAA already antialiased the scene, unless the UI then composed a 3D layer (main menu, pause screen): the vanilla FXAA then runs,
      // suiting its thin line art better (SMAA on the finished frame left more stairs; on the layer before its tone curve, fringes on
      // semi-transparent edges). Bitwise "&" so both flags reset every frame.
      if (original_shader_hashes.Contains(fxaa_hash, reshade::api::shader_stage::pixel) && (game_device_data.scene_antialiased.exchange(false) & !game_device_data.layer_drawn.exchange(false)))
         return DrawOrDispatchOverrideType::Skip;

      return DrawOrDispatchOverrideType::None;
   }

   // The game's "RenderScale" in memory: its settings block is a run of int32 in config.xml's order in game.exe's writable data,
   // filled from config.xml before add-ons load. Found by the Resolution, RenderScale, FPS, VSync run; null unless exactly one matches.
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

   // Keeps the game's render scale at the override (or its own option), live: after writing the setting, a WM_SIZE for the current size
   // makes the game rebuild its targets now (it does on any resize, e.g. alt-tab). Changing the option in the game's menu (the same
   // setting) drops the override, also live. Without DLSS/FSR the game stretches its composite (SMAA runs before). With them the main
   // menu ("menu") renders at 100% (no DLSS/FSR there, so the background would be stretched), written only for the rebuild and then
   // replaced by the kept value, so the options menu shows and saves the real one. An alt-tab there rebuilds at the kept value
   // ("menu_rebuilt_low"), so 100% is reapplied, once per menu stay.
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
      // The game's menu wrote it (never during a temporary value)
      if (game_device_data->render_scale_restore_presents == 0 && current != game_device_data->render_scale_memory)
      {
         game_device_data->render_scale_game = game_device_data->render_scale_memory = current;
         if (g_render_scale != 0)
         {
            g_render_scale = 0;
            reshade::set_config_value(nullptr, NAME, "RenderScale", g_render_scale);
         }
      }
#if DEVELOPMENT
      const int32_t render_scale = g_perf_test != 0 ? perf_test_render_scales[g_perf_test] : g_render_scale;
#else
      const int32_t render_scale = g_render_scale;
#endif
      const int32_t kept = render_scale != 0 ? render_scale : game_device_data->render_scale_game;
      const int32_t wanted = menu ? 10 : kept;
      // Once per menu stay: P5StrikersFix rebuilds at the kept value right after it's restored, which would loop the rebuilds (and
      // ReShade's effect reloads) every few presents; the menu then stays at the kept value, as in the game
      if (!menu)
         game_device_data->render_scale_menu_reapplied = false;
      if (menu_rebuilt_low && !game_device_data->render_scale_menu_reapplied && game_device_data->render_scale_restore_presents == 0 && game_device_data->render_scale_applied == wanted)
      {
         game_device_data->render_scale_applied = kept;
         game_device_data->render_scale_menu_reapplied = true;
      }
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
      // The override changed while the targets are already at "wanted" (main menu)
      else if (current != kept)
      {
         *setting = game_device_data->render_scale_memory = kept;
      }
   }

   void OnPresent(ID3D11Device* native_device, DeviceData& device_data) override
   {
      // Set by the composite; Core copies it into "has_drawn_main_post_processing_previous" before this, but never clears it
      device_data.has_drawn_main_post_processing = false;
      // The upscaler's history restarts after any frame it didn't draw (menus, loading, just picked)
      device_data.force_reset_sr = !device_data.has_drawn_sr;
      device_data.has_drawn_sr = false;
      auto& game_device_data = GetGameDeviceData(device_data);
      game_device_data.mv_presents++;
      {
         const std::lock_guard lock(game_device_data.layer_mutex);
         const auto stale = [](const auto& entry)
         { return cb_luma_global_settings.FrameIndex - entry.second.frame_index > 1; };
         std::erase_if(game_device_data.layer_outputs, stale);
         std::erase_if(game_device_data.layer_composed, stale);
      }
      {
         // The main menu after 30 presents (loading screens and fades between keep the game's scale), until the scene draws again. Scene
         // frames are only seen with DLSS/FSR (motion vectors start them), the only time 100% matters.
         const bool render_resolution_composite = game_device_data.render_resolution_composite.exchange(false);
         if (game_device_data.scene_drawn.exchange(false) || !IsSRActive(device_data))
            game_device_data.menu_presents = 0;
         else if (render_resolution_composite && game_device_data.menu_presents < 30)
            game_device_data.menu_presents++;
         const bool menu = game_device_data.menu_presents >= 30;
         UpdateRenderScale(&game_device_data, menu, menu && render_resolution_composite);
      }
      // A mip sharper under DLSS/FSR, which resolves the detail over its jittered frames (Core applies it to anisotropic samplers)
      if (!custom_texture_mip_lod_bias_offset)
      {
         const std::unique_lock lock(s_mutex_samplers);
         device_data.texture_mip_lod_bias_offset = IsSRActive(device_data) ? SR::GetMipLODBias(game_device_data.sr_render_height.load(), game_device_data.sr_output_height.load()) : 0.f;
      }
#if DEVELOPMENT
      // "Performance Test": the finished timestamp sets, averaged into a log line every 120 frames (the first 60 after a mode change skipped)
      if (g_perf_test != 0)
      {
         com_ptr<ID3D11DeviceContext> immediate_context;
         native_device->GetImmediateContext(&immediate_context);
         const std::lock_guard lock(game_device_data.perf_mutex);
         const bool measuring = game_device_data.perf_settle_frames <= 0;
         auto& stats = game_device_data.perf_stats;
         for (auto& queries : game_device_data.perf_queries)
         {
            D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjoint;
            UINT64 scene_start, sr_start, sr_end;
            if (!queries.pending || immediate_context->GetData(queries.disjoint.get(), &disjoint, sizeof(disjoint), D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK)
               continue;
            queries.pending = false;
            if (!measuring || immediate_context->GetData(queries.scene_start.get(), &scene_start, sizeof(scene_start), D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK || immediate_context->GetData(queries.sr_start.get(), &sr_start, sizeof(sr_start), D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK || immediate_context->GetData(queries.sr_end.get(), &sr_end, sizeof(sr_end), D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK)
               continue;
            if (disjoint.Disjoint || disjoint.Frequency == 0)
            {
               stats.disjoint++;
               continue;
            }
            const double scene_ms = 1000.0 * double(sr_start - scene_start) / double(disjoint.Frequency);
            const double sr_ms = 1000.0 * double(sr_end - sr_start) / double(disjoint.Frequency);
            stats.scene_ms += scene_ms;
            stats.scene_max_ms = (std::max)(stats.scene_max_ms, scene_ms);
            stats.sr_ms += sr_ms;
            stats.sr_max_ms = (std::max)(stats.sr_max_ms, sr_ms);
            stats.samples++;
         }
         if (!measuring)
         {
            game_device_data.perf_settle_frames--;
            stats = {};
            game_device_data.perf_hook_ns = 0;
         }
         else if (++stats.frames >= 120)
         {
            const uint32_t samples = (std::max)(stats.samples, 1u);
            reshade::log::message(reshade::log::level::info, std::format("[P5S Perf] mode=\"{}\" sr={} render={}p output={}p gpu scene avg/max={:.3f}/{:.3f} ms gpu sr avg/max={:.3f}/{:.3f} ms cpu hooks={:.3f} ms/frame samples={}/{} disjoint={}", perf_test_modes[g_perf_test], device_data.sr_type == SR::Type::FSR ? "FSR" : "DLSS", game_device_data.sr_render_height.load(), game_device_data.sr_output_height.load(), stats.scene_ms / samples, stats.scene_max_ms, stats.sr_ms / samples, stats.sr_max_ms, double(game_device_data.perf_hook_ns.exchange(0)) / 1e6 / stats.frames, stats.samples, stats.frames, stats.disjoint).c_str());
            stats = {};
         }
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
   }

   void DrawImGuiSettings(DeviceData& device_data) override
   {
      auto& settings = cb_luma_global_settings.GameSettings;

      // Persisted GameSettings checkbox (0/1) with tooltip and reset button
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
         // The game's render scale (its option steps by 10%), overridden live (see "UpdateRenderScale"), at the upscaler modes' steps. A saved
         // value between them shows as a percentage.
         const auto& game_device_data = GetGameDeviceData(device_data);
         ImGui::BeginDisabled(!game_device_data.render_scale_setting);
         constexpr std::pair<const char*, int> presets[] = {{"Game Setting", 0}, {"Native", 10}, {"Quality", 7}, {"Balanced", 6}, {"Performance", 5}};
         const auto current = std::ranges::find(presets, g_render_scale, &std::pair<const char*, int>::second);
         if (ImGui::BeginCombo("Render Scale", current != std::end(presets) ? current->first : std::format("{}%", g_render_scale * 10).c_str()))
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
         if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip("The resolution the game renders at, upscaled by DLSS/FSR or stretched by the game.\nOverrides the game's own option until you change it in the game.");
         DrawResetButton(g_render_scale, 0, "RenderScale");
         ImGui::EndDisabled();
      }
      constexpr const char* smaa_tooltip = "Replaces the game's FXAA with SMAA (works with the game's anti-aliasing setting on or off; not used with DLSS/FSR).";
      bool smaa = false;
      // DLSS/FSR replace SMAA: shown off, the saved choice kept for when they're turned off
      if (IsSRActive(device_data))
      {
         ImGui::BeginDisabled();
         ImGui::Checkbox("SMAA Enable", &smaa);
         ImGui::EndDisabled();
         if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip("%s", smaa_tooltip);
      }
      else
      {
         smaa = settings_toggle("SMAA Enable", "SMAAEnable", &settings.SMAAEnable, default_luma_global_game_settings.SMAAEnable, smaa_tooltip);
      }
      ImGui::BeginDisabled(!smaa && !IsSRActive(device_data));
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
      DrawResetButton(g_gtao_enable, true, "GTAOEnable");
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

      ImGui::SeparatorText("Performance");
      ImGui::BeginDisabled(!IsSRActive(device_data));
      const int previous_perf_test = g_perf_test;
      if (ImGui::Combo("Performance Test", &g_perf_test, perf_test_modes, int(std::size(perf_test_modes))) && g_perf_test != previous_perf_test)
      {
         auto& game_device_data = GetGameDeviceData(device_data);
         const std::lock_guard lock(game_device_data.perf_mutex);
         game_device_data.perf_settle_frames = 60;
      }
      ImGui::EndDisabled();
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
         ImGui::SetTooltip("Sets the render scale for the mode and logs \"[P5S Perf]\" to ReShade.log every 120 frames: the GPU time of the scene (from its first depth draw to the split,\nthe motion vector draws included, and any GPU idle between the game's command lists) and of DLSS/FSR (timestamps, so the 60 fps cap doesn't matter), and the CPU time in the motion vector hooks.\nThe difference between DLAA and \"DLAA Without Motion Vector Draws\" is their cost (that mode's image is unjittered, with camera-only motion vectors).\nNeeds DLSS or FSR. Keep the camera still, set the GPU to maximum performance in the driver (a capped GPU downclocks). Not saved.");
   }
#endif

   void PrintImGuiAbout() override
   {
      ImGui::PushTextWrapPos(0.f);
      ImGui::Text(
         "Luma for \"Persona 5 Strikers\" is developed by DristoforColumb and is open source and free.\n"
         "It adds HDR, DLSS or FSR 3 upscaling and native anti-aliasing (DLAA), and replaces the game's FXAA with SMAA and its SSAO with XeGTAO.\n"
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
                  "\nAMD FidelityFX (RCAS + FSR 3)"
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
      // FXAA reads a BGRA8 swapchain copy (after UI), which must hold HDR too. R11G11B10_FLOAT is upgraded for precision as in Nioh: the
      // HDR scene (deferred lighting, refraction grab copy) and the bloom and flare mips (swapchain aspect ratio); the bloom chain
      // requantizes through 11 passes, and the 5 bit blue mantissa tints the halos. Arrays (the R11G11B10 G-buffer) are never upgraded.
      // Every other swapchain aspect ratio BGRA8 target (e.g. G-buffer albedo) is upgraded too; upgrading only the FXAA copy would save VRAM.
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
