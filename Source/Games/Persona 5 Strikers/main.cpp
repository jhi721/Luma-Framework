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
#include "..\..\Core\includes\patched_draws.h"
#if DEVELOPMENT
#include "..\..\Core\includes\perf_test.h"
#endif

namespace
{
   // Katana PostEffect3 composite: exposure, lens effects, vignette, baked HDR 3D LUT (tonemap + grade). Once per captured scene
   // (menus, dialogue, hub, field) into the swapchain; the main menu adds one into an off-screen RGBA8 target.
   constexpr uint32_t composite_hash = 0x45A96F2D;
   // FXAA 3 from a swapchain copy (after the UI) back into it
   constexpr uint32_t fxaa_hash = 0xED2D9823;
   // SMAA's scratch is freed after this many frames without SMAA (DLSS/FSR antialias), the copies after as many without SMAA or RCAS
   // (~10 s at the game's 60 fps cap)
   constexpr uint32_t smaa_idle_release_frames = 600;
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
   // A scene frame's first post pass (scene at t0): DOF prefilter (E0DB2D7E or its two highlight mask variants), else bloom prefilter,
   // else composite. It ends the scene frame; DLSS/FSR run right before it. The preceding lighting varies by scene (691D080F, or
   // 4376F855 twice).
   const ShaderHashesList post_process_start_shader_hashes = {.pixel_shaders = {0xE0DB2D7E, 0xD65ABD25, 0x3D7CAD40, 0xB6289AC0, composite_hash}};
   // Bloom upsamples (3x3 tent into the next mip up, and its g_vMaxUV clamped twin): the first of a frame adds the iterations the game
   // skips below the output resolution (see "ExtendBloom")
   const ShaderHashesList bloom_upsample_shader_hashes = {.pixel_shaders = {0x18E4283B, 0xE40794E8}};
   constexpr UINT bloom_loop_cb_slot = 3; // "cbBloomLoop", g_vSampleScale.x (the tent scale) at byte 16
   constexpr UINT post_srv_count = 16;    // The texture slots a redirected post pass gets (the game's use t0-t4)
#if DEVELOPMENT
   // Post passes blending into the scene in place, by UV: DOF merges (5 sample, 9 sample, reduction) and bloom adds (5 and 9 sample,
   // plus sub-rect clamped "ForViewport" twins). The previous way (A/B) draws them a second time into the upscaler's output.
   const ShaderHashesList scene_post_writer_shader_hashes = {.pixel_shaders = {0x409590F7, 0x42D664E0, 0xAA4F82B2, 0x619045C8, 0xB5F3F656, 0x88C4EC12, 0x4CE014A2}};
#endif
   // Generic copy: glyphs into their atlas, a 3D layer's alpha into its composite's output, and (render scale below 1) the composite's
   // stretch onto the swapchain
   constexpr uint32_t copy_hash = 0x987DC89C;
   // Fullscreen at a custom resolution (P5StrikersFix): the game draws the whole frame (composite, UI, FXAA) into its own target of that
   // size, then this (exp(log(color) * gamma), gamma 1) puts it on the swapchain within black bars (92F1307C). That target then is what
   // "swapchain" means in this file (see "IsGameBackBuffer").
   constexpr uint32_t letterbox_hash = 0xEC5254F6;
   // 3D layers outside the scene (main menu and pause screen characters): the quad vertex shader (texture coordinates from vertices)
   // and the stretch of a layer's render resolution corner over its target
   constexpr uint32_t quad_vertex_shader_hash = 0x2B6CA9A0;
   constexpr uint32_t layer_stretch_hash = 0x99A76DC2;
   constexpr UINT layer_uv_scale_cb_slot = 5; // "register(b5)" in Includes/LayerCorner.hlsl; the quads and sprites only read b0
   // UI sprite vertex shader (also puts 3D layer composites on the swapchain) and the exposure histogram (reading the scene or a layer)
   constexpr uint32_t ui_sprite_vertex_shader_hash = 0x8B19022A;
   constexpr uint32_t exposure_histogram_hash = 0xCC8D4849;

   bool g_hide_ui = false; // Session only, so a restart always has a HUD

   // Forced over the game's "RenderScale" option (5 = 50% ... 10 = 100%)
   int g_render_scale = 10;

   bool g_gtao_enable = true;
   constexpr UINT gtao_knobs_cb_slot = 9; // "register(b9)" in Luma_P5S_XeGTAO.hlsl; b11 is core DrawBloom's
   float g_gtao_final_value_power = 1.f;  // DEV/TEST calibration knobs, not persisted
   float g_gtao_radius_override = 0.f;    // > 0 overrides the native radius (centimetres)

#if DEVELOPMENT
   int g_gtao_debug_view = 0; // 0=off 1=depth gradient 2=normals 3=AO x8 4=edges
   // See "PatchedDraws::CountTieBreakCollisions"
   uint32_t g_mv_tiebreak_collisions_last_frame = 0;
   // Per scene frame (published at the next one's start): the depth tested draws into the scene depth beside the G-buffer that got the
   // jitter, or didn't for lack of a vertex buffer or of a motion vector patch (no camera in $Globals, patch failed). Each skipped pixel
   // shader is logged once, with its reason.
   std::atomic<uint32_t> g_jitter_draws = 0;
   std::atomic<uint32_t> g_jitter_skipped_no_vb = 0;
   std::atomic<uint32_t> g_jitter_skipped_unpatched = 0;
   uint32_t g_jitter_draws_last_frame = 0;
   uint32_t g_jitter_skipped_no_vb_last_frame = 0;
   uint32_t g_jitter_skipped_unpatched_last_frame = 0;
   std::shared_mutex g_jitter_skipped_mutex;
   std::unordered_set<uint64_t> g_jitter_skipped_logged;
   bool g_smaa_predication = true;
   int g_smaa_debug_view = 0; // 0 off, 1 edges, 2 predication
   // Upscaling A/B: the post process at the output resolution (see "RedirectPostDraw"), else the previous way (see
   // "scene_post_writer_shader_hashes")
   bool g_post_output_resolution = true;
   // "Performance Test" (see "OnPresent"): the mode ("Perf::g_test") render scale override (0 none) and name; 2 skips the motion
   // vector draws
   constexpr int perf_test_render_scales[] = {0, 10, 10, 7, 5};
   constexpr const char* perf_test_modes[] = {"Off", "DLAA", "DLAA Without Motion Vector Draws", "Quality (70%)", "Performance (50%)"};
   // A/B for the buffer hooks' lock free $Globals filter (see "MayBeGlobalsBuffer")
   bool g_mv_globals_filter = true;
   // A/B for the motion vector target's format: R16G16_FLOAT, else R32G32_FLOAT
   bool g_mv_half_float = true;
#else
   constexpr int g_gtao_debug_view = 0;
   constexpr bool g_mv_globals_filter = true;
   constexpr bool g_mv_half_float = true;
   constexpr bool g_smaa_predication = true;
   constexpr bool g_post_output_resolution = true;
#endif

   // The view's resource, null without a view
   com_ptr<ID3D11Resource> GetViewResource(ID3D11View* view)
   {
      com_ptr<ID3D11Resource> resource;
      if (view)
      {
         view->GetResource(&resource);
      }
      return resource;
   }

   // Draws with a Luma vertex shader (a layer's clone, see "Includes/LayerCorner.hlsl", or a motion vector patch) and one of its
   // constant buffers (the layer's uv scale, the jitter), or as is without one, bypassing Core's state tracking: restores the game's.
   void DrawWithVertexShader(ID3D11DeviceContext* native_device_context, ID3D11VertexShader* vertex_shader, UINT cb_slot, ID3D11Buffer* buffer, const std::function<void()>& draw)
   {
      if (!vertex_shader)
      {
         draw();
         return;
      }
      com_ptr<ID3D11VertexShader> original_vertex_shader;
      com_ptr<ID3D11Buffer> original_buffer;
      native_device_context->VSGetShader(&original_vertex_shader, nullptr, nullptr);
      native_device_context->VSGetConstantBuffers(cb_slot, 1, &original_buffer);
      native_device_context->VSSetConstantBuffers(cb_slot, 1, &buffer);
      native_device_context->VSSetShader(vertex_shader, nullptr, 0);
      draw();
      ID3D11Buffer* const restored_buffer = original_buffer.get();
      native_device_context->VSSetConstantBuffers(cb_slot, 1, &restored_buffer);
      native_device_context->VSSetShader(original_vertex_shader.get(), nullptr, 0);
   }

   // Viewport and scissor over a whole target (the game scissors its passes)
   void SetFullTargetViewport(ID3D11DeviceContext* native_device_context, UINT width, UINT height, float min_depth = 0.f, float max_depth = 1.f)
   {
      const D3D11_VIEWPORT viewport = {.TopLeftX = 0.f, .TopLeftY = 0.f, .Width = float(width), .Height = float(height), .MinDepth = min_depth, .MaxDepth = max_depth};
      const D3D11_RECT scissor = {.left = 0, .top = 0, .right = LONG(width), .bottom = LONG(height)};
      native_device_context->RSSetViewports(1, &viewport);
      native_device_context->RSSetScissorRects(1, &scissor);
   }

} // namespace

// Holds the device objects, so they are released with the device.
struct Persona5StrikersGameDeviceData final : public GameDeviceData
{
   // SMAA scratch, recreated on canvas resize: a linear canvas copy (SMAA can't sample the canvas it writes), its gamma encode (edge
   // detection input; with RCAS also SMAA's output for finalize) and the predication edge-ness. Guarded (with Core's SMAA intermediates):
   // the main menu records its two composites on two deferred contexts, one of them may release it idle while the other draws.
   std::shared_mutex smaa_mutex;
   com_ptr<ID3D11Texture2D> smaa_linear_texture;
   com_ptr<ID3D11ShaderResourceView> smaa_linear_srv;
   com_ptr<ID3D11ShaderResourceView> smaa_gamma_srv;
   com_ptr<ID3D11RenderTargetView> smaa_gamma_rtv;
   com_ptr<ID3D11UnorderedAccessView> smaa_gamma_uav;
   com_ptr<ID3D11ShaderResourceView> smaa_predication_srv;
   com_ptr<ID3D11UnorderedAccessView> smaa_predication_uav;
   // Composite only: the frame SMAA last ran, and SMAA or RCAS (both use the copies), for "smaa_idle_release_frames"
   uint32_t smaa_frame = 0;
   uint32_t smaa_copies_frame = 0;

   void ReleaseSMAACopies()
   {
      smaa_linear_texture.reset();
      smaa_linear_srv.reset();
      smaa_gamma_srv.reset();
      smaa_gamma_rtv.reset();
      smaa_gamma_uav.reset();
   }

   // Full res scene depth (SSAO depth downsample's t0) for predication, and for motion vectors when the G-buffer's is unreadable.
   // Kept across frames (the texture persists, rewritten before post): SSAO and post record on different deferred contexts, so per frame
   // resets would race.
   std::shared_mutex smaa_depth_mutex;
   com_ptr<ID3D11ShaderResourceView> smaa_depth_srv;

   // SMAA ran after this frame's composite, or DLSS/FSR before its post: skip the vanilla FXAA
   std::atomic<bool> scene_antialiased = false;
   // A 3D layer outside the scene was drawn (main menu, pause screen), composed by the UI after SMAA: FXAA still runs
   std::atomic<bool> layer_drawn = false;

   // MIN and MAX blends (all channels, alpha only), to clamp the swapchain to 0-1 under a UI draw's own geometry (see "OnDrawOrDispatch")
   com_ptr<ID3D11BlendState> ui_min_blend_states[2];
   com_ptr<ID3D11BlendState> ui_max_blend_states[2];

   // XeGTAO scratch, recreated on SSAO target resize; mutex guarded, as SSAO records on worker threads (deferred contexts)
   std::shared_mutex gtao_mutex;
   com_ptr<ID3D11UnorderedAccessView> gtao_depth_mip_uavs[5]; // R32F view space depth pyramid, 5 mips
   com_ptr<ID3D11ShaderResourceView> gtao_depth_mips_srv;
   com_ptr<ID3D11UnorderedAccessView> gtao_working_uavs[2]; // R8G8_UNORM AO + edges ping-pong
   com_ptr<ID3D11ShaderResourceView> gtao_working_srvs[2];
   com_ptr<ID3D11Texture2D> gtao_final_texture; // R8_UNORM copy source for the game's target
   com_ptr<ID3D11UnorderedAccessView> gtao_final_uav;
   uint32_t gtao_width = 0;
   uint32_t gtao_height = 0;
   com_ptr<ID3D11Buffer> gtao_knobs_cb; // dynamic, rewritten every run (the noise index changes per frame with DLSS/FSR)

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
      uint32_t resource_slots = (1u << MotionVectorPatches::resource_slots) - 1; // A bit per t-slot the shader reads (all without reflection)
   };
   std::unordered_map<uint32_t, GlobalsLayout> mv_globals_layouts;
   // The scene records on one deferred context: after its first post pass, the next scene depth draw (depth prepass, else G-buffer)
   // starts a frame and clears the target
   // Atomics: every context's depth draws check them (shadows, other passes) before touching the scene's state
   std::atomic<bool> mv_frame_ended = true;
   com_ptr<ID3D11Resource> mv_scene_depth;                       // The G-buffer's depth, which the forward redraws share
   std::atomic<ID3D11Resource*> mv_scene_depth_id = nullptr;     // "mv_scene_depth", for other contexts to compare
   std::atomic<ID3D11DeviceContext*> mv_scene_context = nullptr; // Only compared

   // The $Globals buffers patched draws bind (mapped with discard before nearly every draw; some draws reuse the last contents): each
   // Map's pointer, and at Unmap a CPU snapshot of it that the draws (and the object history) share
   using GlobalsCopy = std::shared_ptr<const std::vector<uint8_t>>;
   struct GlobalsBuffer
   {
      void* mapped = nullptr; // Between a Map and its Unmap
      UINT size = 0;
      GlobalsCopy copy; // Null until the first Unmap after the buffer's registration
   };
   std::shared_mutex mv_globals_mutex;
   std::unordered_map<uint64_t, GlobalsBuffer> mv_globals_buffers;
   // "mv_globals_buffers" again, for the buffer hooks to skip the game's other buffers without the lock (see "MayBeGlobalsBuffer"): an
   // open addressing set (0 = empty slot), filled under "mv_globals_mutex" up to 3/4, then a count above that lets every buffer through.
   // A destroyed buffer's slot becomes a marker that lookups step over and insertions reuse (not a pointer: those are aligned); when
   // they fill it, it's rebuilt from the registered buffers, "mv_filtered_globals_rebuilds" odd meanwhile (a sequence lock: lookups
   // that overlap one match anything).
   static constexpr uint32_t filtered_globals_buffer_slots = 256; // A power of two
   static constexpr uint32_t max_filtered_globals_buffers = filtered_globals_buffer_slots / 4 * 3;
   static constexpr uint64_t destroyed_globals_buffer_slot = 1;
   std::array<std::atomic<uint64_t>, filtered_globals_buffer_slots> mv_filtered_globals_buffers = {};
   std::atomic<uint32_t> mv_filtered_globals_buffer_count = 0; // Occupied slots, markers included
   std::atomic<uint32_t> mv_filtered_globals_rebuilds = 0;
   // Scene context only: the previous frame's $Globals uploads, one dynamic buffer per size, and a draw's $Globals with last frame's
   // camera before its upload
   std::unordered_map<UINT, com_ptr<ID3D11Buffer>> mv_previous_globals_buffers;
   std::vector<uint8_t> mv_globals_scratch;
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
   // Patched draws by draw key (shaders, buffers, arguments), with the world matrix and $Globals. A draw takes the previous frame's
   // $Globals of its key's nearest draw (same object, a frame earlier).
   struct MotionVectorObject
   {
      PatchedDraws::ObjectTransform transform;
      GlobalsCopy globals; // Never null
   };
   std::unordered_map<uint64_t, std::vector<MotionVectorObject>> mv_objects;
   std::unordered_map<uint64_t, std::vector<MotionVectorObject>> mv_previous_objects;
   // This frame's projection jitter (pixels, +y down) and its VS cbuffer ("MotionVectorPatches::jitter_slot": NDC offset)
   std::array<float, 2> mv_jitter = {};
   com_ptr<ID3D11Buffer> mv_jitter_buffer;
   // Camera motion fill of pixels no patched draw wrote (see "Luma_P5S_MotionVectorFill.hlsl"; frame start marks them 65504, the largest
   // float16): the target's UAV (if the GPU loads its format from UAVs), its constants, and the frame's depth (also the upscaler's)
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
   // The upscaled frame's second composite, into the game's own target (after the split, so timed on its own)
   struct PerfDrawQueries
   {
      com_ptr<ID3D11Query> disjoint, start, end;
      bool pending = false;
   };
   struct PerfStats
   {
      Perf::Stat scene, sr, composite;
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
      com_ptr<ID3D11Texture2D> motion_vectors;
      com_ptr<ID3D11Resource> depth;
      std::array<float, 2> jitter;
      float vertical_fov = 0.7330383f; // Radians (FSR needs it), 42 degrees as measured in dialogue until the frame's camera is known
   };
   std::shared_mutex sr_mutex;
   bool sr_split_ready = false;                                // Scene context only: this frame's G-buffer drew, not split yet
   uint64_t sr_split_context = 0;                              // The context split last, until the game finishes its command list
   SRSplit sr_split;                                           // The last split, likewise
   std::unordered_map<uint64_t, SRSplit> sr_pending_splits;    // By the game's command list (the remainder), until executed
   std::vector<com_ptr<ID3D11Buffer>> sr_no_overwrite_buffers; // Dynamic buffers the game appends to ("D3D11_MAP_WRITE_NO_OVERWRITE")
#if DEVELOPMENT
   std::shared_mutex perf_mutex;
   std::array<PerfQueries, 8> perf_queries;
   size_t perf_query_index = 0;
   std::array<PerfDrawQueries, 8> perf_composite_queries;
   size_t perf_composite_query_index = 0;
   PerfQueries* perf_frame_queries = nullptr; // Scene context only: this frame's, until its split takes it
   Perf::Window<PerfStats> perf_window;
#endif
   // Upscaling: the game renders scene and post (composite included) at render scale, then stretches onto the swapchain. Instead the
   // upscaler writes the output resolution, the post process up to the composite runs at the output resolution on it (see
   // "RedirectPostDraw"), and the composite draws from it into an output sized canvas that the stretch copies 1:1.
   std::atomic<ID3D11DeviceContext*> sr_upscaling_context = nullptr; // This frame's scene context, once split for upscaling (only compared)
   // "IsSRActive", taken at present: Core's "Super Resolution" selection changes after it, mid frame for the scene's command lists
   std::atomic<bool> sr_active = false;
   // The upscaler drew the last presented frame: the jitter only goes where it gets resolved (a scene the split refuses, e.g. an
   // unexpected size or format, would otherwise just shake)
   std::atomic<bool> sr_drew = false;
   // The reasons a scene frame wasn't split for the upscaler, each logged once (bits as in the "[P5S SR]" warning's legend)
   std::atomic<uint32_t> sr_split_refusals_logged = 0;
   com_ptr<ID3D11Texture2D> sr_upscaled_output;
   // Immediate context only: the upscaled output DLSS/FSR last drew into, null after a DLAA frame (see "output_recreated")
   com_ptr<ID3D11Texture2D> sr_drawn_output;
   com_ptr<ID3D11RenderTargetView> sr_upscaled_output_rtv;
   com_ptr<ID3D11ShaderResourceView> sr_upscaled_output_srv;
   com_ptr<ID3D11RenderTargetView> sr_upscaled_canvas_rtv;
   com_ptr<ID3D11ShaderResourceView> sr_upscaled_canvas_srv;
   // Scene context only, from the split to the composite: the game's render resolution scene (the upscaler's input), with a view to
   // downscale the output into it for its readers outside the redirection, and whether it holds the output (see "DownscalePostScene")
   bool post_redirect = false;
   com_ptr<ID3D11Resource> post_scene;
   com_ptr<ID3D11RenderTargetView> post_scene_rtv;
   bool post_scene_current = false;
   // Output resolution copies of the post process targets, by the game's texture (held, so its address can't be reused), with their
   // views by the game's view; made for "post_sizes" (render, output), cleared when those change
   struct PostTarget
   {
      com_ptr<ID3D11Resource> original;
      com_ptr<ID3D11Texture2D> texture;
      std::vector<com_ptr<ID3D11ShaderResourceView>> mip_srvs; // One level views, for the bloom iterations Luma adds
      std::vector<com_ptr<ID3D11RenderTargetView>> mip_rtvs;
   };
   std::unordered_map<ID3D11Resource*, PostTarget> post_targets;
   struct PostView
   {
      com_ptr<ID3D11View> original; // Held, so its address can't be reused
      com_ptr<ID3D11View> replacement;
   };
   std::unordered_map<ID3D11View*, PostView> post_views;
   std::array<uint2, 2> post_sizes = {};
   // The tent scale of the frame's bloom upsamples with the iterations the game skips below the output resolution ("ExtendBloom",
   // "LumaData.CustomData3"): 0 for the game's, < 0 until the first upsample
   float bloom_sample_scale = -1.f;
   // This frame's composite target when it isn't the swapchain (render scales below 1): reset at the scene's start, set by the composite,
   // read by the stretch, which may record on different contexts
   std::shared_mutex composite_target_mutex;
   com_ptr<ID3D11Resource> composite_target;
   // The target the game letterboxes onto the swapchain (see "letterbox_hash"): set by that draw, dropped at the first present without
   // one. Its aspect ratio is upgraded as the swapchain's, from the rebuild it asks for.
   struct Letterbox
   {
      std::shared_mutex mutex;
      com_ptr<ID3D11Resource> target;
      uint2 size = {};
      bool drawn = false;
   };
   Letterbox letterbox;
   // Present thread only: the next "UpdateRenderScale" rebuilds the game's targets even at the same render scale
   bool targets_rebuild = false;
   // The game's "RenderScale" in memory (see "FindRenderScaleSetting"), null if not found. Present thread only.
   int32_t* render_scale_setting = nullptr;
   bool render_scale_searched = false;
   // The main menu: presents without a scene frame whose composite draws into a render resolution target (its background); the
   // pause screen has neither. Reset by any scene frame.
   std::atomic<bool> scene_drawn = false;
   std::atomic<bool> render_resolution_composite = false;
   uint32_t menu_presents = 0;
   // The 3D layers' targets (shared by the main and pause menus) are made at the render resolution of their first use and kept through
   // any rebuild or resize: until a layer drew the game counts as in the main menu (100%), or they stay render sized (stretched) for the
   // whole run. Then the title stays at 100% until a scene frame, as a later main menu does.
   std::atomic<bool> layer_targets_created = false;
   // Upscaling: the 3D layers outside the scene draw at the output resolution (see "DrawLayerAtOutputResolution")
   struct LayerFrame
   {
      std::vector<ID3D11Resource*> targets; // Only compared
      float scale[2] = {1.f, 1.f};          // Render resolution / output resolution
   };
   std::shared_mutex layer_mutex;
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
   // The last split's heights, for the mip bias at present
   struct SRHeights
   {
      uint32_t render = 1;
      uint32_t output = 1;
   };
   std::atomic<SRHeights> sr_heights = SRHeights{};
};

class Persona5Strikers final : public Game
{
   static Persona5StrikersGameDeviceData& GetGameDeviceData(DeviceData& device_data)
   {
      return *static_cast<Persona5StrikersGameDeviceData*>(device_data.game);
   }

   static bool IsSwapchainBackBuffer(DeviceData* device_data, ID3D11Resource* resource)
   {
      const std::shared_lock lock(device_data->mutex);
      return device_data->back_buffers.contains(reinterpret_cast<uint64_t>(resource));
   }

   // What the game draws its frame into: the swapchain's back buffer, or the target it letterboxes onto it
   static bool IsGameBackBuffer(DeviceData* device_data, ID3D11Resource* resource)
   {
      {
         auto& letterbox = GetGameDeviceData(*device_data).letterbox;
         const std::shared_lock lock(letterbox.mutex);
         if (letterbox.target)
            return letterbox.target.get() == resource;
      }
      return IsSwapchainBackBuffer(device_data, resource);
   }

   // The game's back buffer size: the output resolution of the upscaling, the layers and the composite
   static uint2 GetOutputSize(DeviceData& device_data)
   {
      {
         auto& letterbox = GetGameDeviceData(device_data).letterbox;
         const std::shared_lock lock(letterbox.mutex);
         if (letterbox.target)
            return letterbox.size;
      }
      return {uint32_t(device_data.output_resolution.x + 0.5f), uint32_t(device_data.output_resolution.y + 0.5f)};
   }

   static bool IsSRActive(const DeviceData& device_data)
   {
      return device_data.game && static_cast<const Persona5StrikersGameDeviceData*>(device_data.game)->sr_active;
   }

   static DeviceData* GetDeviceData(reshade::api::device* device)
   {
      auto* const device_data = device->get_private_data<DeviceData>();
      return device_data && device_data->game ? device_data : nullptr;
   }

   // A buffer's first slot in "mv_filtered_globals_buffers" (Fibonacci hashing of the pointer, whose low bits are alignment)
   static uint32_t FilteredGlobalsBufferSlot(uint64_t handle)
   {
      return uint32_t(((handle >> 4) * 0x9E3779B97F4A7C15ull) >> 56) & (Persona5StrikersGameDeviceData::filtered_globals_buffer_slots - 1);
   }

   // False if the buffer surely isn't one of "mv_globals_buffers". Lock free: the buffer hooks see every Map/Unmap of the game.
   static bool MayBeGlobalsBuffer(const Persona5StrikersGameDeviceData& game_device_data, uint64_t handle)
   {
      const uint32_t rebuilds = game_device_data.mv_filtered_globals_rebuilds.load(std::memory_order_acquire);
      if (!g_mv_globals_filter || (rebuilds & 1) != 0 || game_device_data.mv_filtered_globals_buffer_count.load(std::memory_order_relaxed) > Persona5StrikersGameDeviceData::max_filtered_globals_buffers)
         return true;
      // Ends at an empty slot: the set is never more than 3/4 full
      for (uint32_t i = FilteredGlobalsBufferSlot(handle);; i = (i + 1) & (Persona5StrikersGameDeviceData::filtered_globals_buffer_slots - 1))
      {
         const uint64_t slot = game_device_data.mv_filtered_globals_buffers[i].load(std::memory_order_acquire);
         if (slot == handle)
            return true;
         if (slot == 0)
            break;
      }
      // Not found, unless a rebuild moved it meanwhile
      std::atomic_thread_fence(std::memory_order_acquire);
      return game_device_data.mv_filtered_globals_rebuilds.load(std::memory_order_relaxed) != rebuilds;
   }

   // Motion vectors: the mapped pointer of a $Globals buffer a patched draw binds, copied at Unmap
   static void OnMapBufferRegion(reshade::api::device* device, reshade::api::resource resource, uint64_t offset, uint64_t size, reshade::api::map_access access, void** data)
   {
      DeviceData* const device_data = GetDeviceData(device);
      if (!device_data || !IsSRActive(*device_data))
         return;
      auto& game_device_data = GetGameDeviceData(*device_data);
#if DEVELOPMENT
      const Perf::HookTimer timer{&game_device_data.perf_window.hook_ns};
#endif
      if (access == reshade::api::map_access::write_discard && data && *data && MayBeGlobalsBuffer(game_device_data, resource.handle))
      {
         const std::lock_guard lock(game_device_data.mv_globals_mutex);
         if (const auto buffer = game_device_data.mv_globals_buffers.find(resource.handle); buffer != game_device_data.mv_globals_buffers.end())
         {
            buffer->second.mapped = *data;
         }
      }
      // A deferred context's first map of a dynamic buffer in a command list must discard, so the split discards these (see
      // "sr_no_overwrite_buffers")
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
            {
               game_device_data.sr_no_overwrite_buffers.clear();
            }
            game_device_data.sr_no_overwrite_buffers.emplace_back(buffer);
         }
      }
   }

   // Motion vectors: a destroyed $Globals buffer leaves the registry, as its address can come back as another (smaller) buffer
   static void OnDestroyResource(reshade::api::device* device, reshade::api::resource resource)
   {
      DeviceData* const device_data = GetDeviceData(device);
      if (!device_data)
         return;
      auto& game_device_data = GetGameDeviceData(*device_data);
      if (!MayBeGlobalsBuffer(game_device_data, resource.handle))
         return;
      const std::lock_guard lock(game_device_data.mv_globals_mutex);
      if (game_device_data.mv_globals_buffers.erase(resource.handle) == 0)
         return;
      for (uint32_t i = FilteredGlobalsBufferSlot(resource.handle);; i = (i + 1) & (Persona5StrikersGameDeviceData::filtered_globals_buffer_slots - 1))
      {
         const uint64_t slot = game_device_data.mv_filtered_globals_buffers[i].load(std::memory_order_relaxed);
         if (slot == resource.handle)
         {
            game_device_data.mv_filtered_globals_buffers[i].store(Persona5StrikersGameDeviceData::destroyed_globals_buffer_slot, std::memory_order_release);
            break;
         }
         if (slot == 0)
            break;
      }
   }

   // Motion vectors: snapshots a written $Globals buffer's mapped memory at Unmap
   static void OnUnmapBufferRegion(reshade::api::device* device, reshade::api::resource resource)
   {
      DeviceData* const device_data = GetDeviceData(device);
      if (!device_data || !IsSRActive(*device_data))
         return;
      auto& game_device_data = GetGameDeviceData(*device_data);
#if DEVELOPMENT
      const Perf::HookTimer timer{&game_device_data.perf_window.hook_ns};
#endif
      if (!MayBeGlobalsBuffer(game_device_data, resource.handle))
         return;
      const std::lock_guard lock(game_device_data.mv_globals_mutex);
      const auto buffer = game_device_data.mv_globals_buffers.find(resource.handle);
      if (buffer == game_device_data.mv_globals_buffers.end() || !buffer->second.mapped)
         return;
      const auto* const mapped = static_cast<const uint8_t*>(std::exchange(buffer->second.mapped, nullptr));
      buffer->second.copy = std::make_shared<const std::vector<uint8_t>>(mapped, mapped + buffer->second.size);
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
            patched = (vertex ? MotionVectorPatch::PatchVertexShader(code, desc->code_size, MotionVectorPatches::layout, &error) : MotionVectorPatch::PatchPixelShader(code, desc->code_size, MotionVectorPatches::layout, &error));
            com_ptr<ID3D11ShaderReflection> reflection;
            if (vertex && Shader::d3d_reflect && SUCCEEDED(Shader::d3d_reflect(code, desc->code_size, IID_PPV_ARGS(&reflection))))
            {
               // Missing names give dummy reflection objects whose GetDesc fails
               ID3D11ShaderReflectionConstantBuffer* const globals = reflection->GetConstantBufferByName("$Globals");
               D3D11_SHADER_VARIABLE_DESC variable;
               if (SUCCEEDED(globals->GetVariableByName("mW2P")->GetDesc(&variable)) && variable.Size == 64)
               {
                  layout.view_projection = variable.StartOffset;
               }
               if (SUCCEEDED(globals->GetVariableByName("mL2W")->GetDesc(&variable)) && variable.Size >= 48)
               {
                  layout.world = variable.StartOffset;
               }
               D3D11_SHADER_DESC shader_desc;
               if (SUCCEEDED(reflection->GetDesc(&shader_desc)))
               {
                  layout.resource_slots = 0;
                  for (UINT i = 0; i < shader_desc.BoundResources; i++)
                  {
                     D3D11_SHADER_INPUT_BIND_DESC bind;
                     if (FAILED(reflection->GetResourceBindingDesc(i, &bind)) || (bind.Type != D3D_SIT_TEXTURE && bind.Type != D3D_SIT_TBUFFER && bind.Type != D3D_SIT_STRUCTURED && bind.Type != D3D_SIT_BYTEADDRESS))
                        continue;
                     for (UINT slot = bind.BindPoint; slot < bind.BindPoint + bind.BindCount && slot < MotionVectorPatches::resource_slots; slot++)
                        layout.resource_slots |= 1u << slot;
                  }
               }
            }
         }
      }
      com_ptr<T> shader;
      if (!patched.empty())
      {
         const HRESULT hr = [&]
         {
            if constexpr (vertex)
               return native_device->CreateVertexShader(patched.data(), patched.size(), nullptr, &shader);
            else
               return native_device->CreatePixelShader(patched.data(), patched.size(), nullptr, &shader);
         }();
         if (FAILED(hr))
         {
            error = std::format("create 0x{:08X}", uint32_t(hr));
         }
      }
      // Failures in every build (bug reports), every patched shader only in development
      if (DEVELOPMENT || !shader)
      {
         reshade::log::message(shader ? reshade::log::level::info : reshade::log::level::warning, std::format("[P5S MV] {} 0x{:08X} {}", vertex ? "VS" : "PS", hash, shader ? "patched" : error).c_str());
      }
      const std::unique_lock lock(game_device_data.mv_mutex);
      if (vertex)
      {
         game_device_data.mv_globals_layouts.try_emplace(hash, layout);
      }
      return shaders->try_emplace(hash, shader).first->second;
   }

   // A scene frame starts at its first draw into the scene depth (depth prepass, else G-buffer): clears the motion vectors, moves the
   // object history to the previous frame, and sets the jitter
   static void StartMotionVectorFrame(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, DeviceData& device_data, const uint4& depth_size)
   {
      auto& game_device_data = GetGameDeviceData(device_data);
#if DEVELOPMENT
      g_jitter_draws_last_frame = g_jitter_draws.exchange(0);
      g_jitter_skipped_no_vb_last_frame = g_jitter_skipped_no_vb.exchange(0);
      g_jitter_skipped_unpatched_last_frame = g_jitter_skipped_unpatched.exchange(0);
      // "Performance Test": the scene's GPU time starts here, a timestamp recorded into the game's command list
      {
         const std::lock_guard lock(game_device_data.perf_mutex);
         game_device_data.perf_frame_queries = nullptr; // The last frame's if it never split
         auto& queries = game_device_data.perf_queries[game_device_data.perf_query_index];
         // Skipped while the ring's next set is still unread
         if (Perf::g_test != 0 && !queries.pending)
         {
            const D3D11_QUERY_DESC disjoint_desc = {.Query = D3D11_QUERY_TIMESTAMP_DISJOINT}, timestamp_desc = {.Query = D3D11_QUERY_TIMESTAMP};
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
      const FLOAT clear_value = (game_device_data.mv_fill_pending ? 65504.f : 0.f);
      const FLOAT clear[4] = {clear_value, clear_value, 0.f, 0.f};
      native_device_context->ClearRenderTargetView(game_device_data.mv_rtv.get(), clear);
      game_device_data.mv_frame_ended = false;
      game_device_data.mv_scene_context = native_device_context;
      game_device_data.sr_split_ready = true;
      game_device_data.scene_drawn = true;
      game_device_data.sr_upscaling_context = nullptr; // Until this frame's split decides
      {
         const std::unique_lock lock(game_device_data.composite_target_mutex);
         game_device_data.composite_target.reset();
      }
      // Last frame's camera is the previous one, unless scene-less frames (menus) came between
      game_device_data.mv_previous_view_projection = game_device_data.mv_view_projection;
      game_device_data.mv_previous_view_projection_valid = game_device_data.mv_view_projection_valid && game_device_data.mv_presents - game_device_data.mv_frame_present <= 1;
      game_device_data.mv_view_projection_valid = false;
      game_device_data.mv_frame_present = game_device_data.mv_presents;
      // Last frame's objects become the previous ones; keys drawn two frames ago keep their node and capacity for this frame
      std::swap(game_device_data.mv_objects, game_device_data.mv_previous_objects);
#if DEVELOPMENT
      g_mv_tiebreak_collisions_last_frame = PatchedDraws::CountTieBreakCollisions(game_device_data.mv_previous_objects, [](const auto& a, const auto& b)
         { return PatchedDraws::SameBytes(a.globals, b.globals); });
#endif
      std::erase_if(game_device_data.mv_objects, [](const auto& entry)
         {
            const auto& [key, objects] = entry;
            return objects.empty(); });
      for (auto& [key, objects] : game_device_data.mv_objects)
         objects.clear();
      if (!game_device_data.mv_previous_view_projection_valid)
      {
         game_device_data.mv_previous_objects.clear();
      }
      std::erase_if(game_device_data.mv_previous_resources, [&](const auto& entry)
         {
            const auto& [resource, previous] = entry;
            return previous.frame + 1 < game_device_data.mv_frame_present; });

      // Halton (2, 3) over the upscaler's phase count (from its last settings; more at lower render scales)
      const bool jitter = IsSRActive(device_data) && game_device_data.sr_drew;
      const SR::InstanceData* const sr_instance_data = (jitter ? device_data.GetSRInstanceData() : nullptr);
      const int phases = (sr_instance_data ? (std::max)(sr_implementations[device_data.sr_type]->GetJitterPhases(sr_instance_data), 1) : SR::GetDefaultJitterPhases());
      game_device_data.mv_jitter = (jitter ? std::array<float, 2>{SR::HaltonSequence(cb_luma_global_settings.FrameIndex % phases, 2), SR::HaltonSequence(cb_luma_global_settings.FrameIndex % phases, 3)} : std::array<float, 2>{});
      // Pixels to NDC (y up)
      const float ndc_jitter[4] = {game_device_data.mv_jitter[0] * 2.f / float(depth_size.x), game_device_data.mv_jitter[1] * -2.f / float(depth_size.y), 0.f, 0.f};
      if (!PatchedDraws::WriteDynamicConstants(native_device, native_device_context, std::addressof(game_device_data.mv_jitter_buffer), ndc_jitter, sizeof(ndc_jitter)))
      {
         // No stale jitter on the scene draws either (its slot then reads 0), nor for the upscaler
         game_device_data.mv_jitter = {};
         game_device_data.mv_jitter_buffer.reset();
      }
   }

   // A patched vertex shader's $Globals byte offsets, by original hash (none until patched)
   static Persona5StrikersGameDeviceData::GlobalsLayout GetGlobalsLayout(Persona5StrikersGameDeviceData* game_device_data, uint32_t hash)
   {
      const std::shared_lock lock(game_device_data->mv_mutex);
      const auto it = game_device_data->mv_globals_layouts.find(hash);
      return it != game_device_data->mv_globals_layouts.end() ? it->second : Persona5StrikersGameDeviceData::GlobalsLayout{};
   }

   // A $Globals buffer's CPU copy (null until its first Unmap); registers it for a copy at every Unmap
   static Persona5StrikersGameDeviceData::GlobalsCopy GetGlobalsCopy(Persona5StrikersGameDeviceData* game_device_data, ID3D11Buffer* buffer)
   {
      const std::lock_guard lock(game_device_data->mv_globals_mutex);
      const uint64_t handle = reinterpret_cast<uint64_t>(buffer);
      const auto [entry, inserted] = game_device_data->mv_globals_buffers.try_emplace(handle);
      if (inserted)
      {
         D3D11_BUFFER_DESC desc;
         buffer->GetDesc(&desc);
         entry->second.size = desc.ByteWidth;
         const uint32_t count = game_device_data->mv_filtered_globals_buffer_count.load(std::memory_order_relaxed);
         if (count <= Persona5StrikersGameDeviceData::max_filtered_globals_buffers)
         {
            uint32_t i = FilteredGlobalsBufferSlot(handle);
            uint64_t slot = 0;
            while ((slot = game_device_data->mv_filtered_globals_buffers[i].load(std::memory_order_relaxed)) != 0 && slot != Persona5StrikersGameDeviceData::destroyed_globals_buffer_slot)
               i = (i + 1) & (Persona5StrikersGameDeviceData::filtered_globals_buffer_slots - 1);
            if (slot == Persona5StrikersGameDeviceData::destroyed_globals_buffer_slot)
            {
               game_device_data->mv_filtered_globals_buffers[i].store(handle, std::memory_order_release);
            }
            else if (count < Persona5StrikersGameDeviceData::max_filtered_globals_buffers)
            {
               game_device_data->mv_filtered_globals_buffers[i].store(handle, std::memory_order_release);
               game_device_data->mv_filtered_globals_buffer_count.store(count + 1, std::memory_order_relaxed);
            }
            // Full: rebuilt without the markers, or past the limit every buffer goes through
            else if (game_device_data->mv_globals_buffers.size() <= Persona5StrikersGameDeviceData::max_filtered_globals_buffers)
            {
               auto& rebuilds = game_device_data->mv_filtered_globals_rebuilds;
               rebuilds.store(rebuilds.load(std::memory_order_relaxed) + 1, std::memory_order_relaxed);
               std::atomic_thread_fence(std::memory_order_release);
               for (auto& filtered : game_device_data->mv_filtered_globals_buffers)
                  filtered.store(0, std::memory_order_relaxed);
               for (const auto& [registered, globals] : game_device_data->mv_globals_buffers)
               {
                  uint32_t j = FilteredGlobalsBufferSlot(registered);
                  while (game_device_data->mv_filtered_globals_buffers[j].load(std::memory_order_relaxed) != 0)
                     j = (j + 1) & (Persona5StrikersGameDeviceData::filtered_globals_buffer_slots - 1);
                  game_device_data->mv_filtered_globals_buffers[j].store(registered, std::memory_order_relaxed);
               }
               game_device_data->mv_filtered_globals_buffer_count.store(uint32_t(game_device_data->mv_globals_buffers.size()), std::memory_order_relaxed);
               rebuilds.store(rebuilds.load(std::memory_order_relaxed) + 1, std::memory_order_release);
            }
            else
            {
               game_device_data->mv_filtered_globals_buffer_count.store(count + 1, std::memory_order_relaxed);
            }
         }
      }
      return entry->second.copy;
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
      {
         depth_texture->GetDesc(&depth_desc);
      }
      if ((depth_desc.BindFlags & D3D11_BIND_SHADER_RESOURCE) != 0)
      {
         if (GetViewResource(game_device_data.mv_scene_depth_srv.get()) != game_device_data.mv_scene_depth)
         {
            game_device_data.mv_scene_depth_srv.reset();
            // The depth formats' depth channel
            DXGI_FORMAT format = depth_desc.Format;
            switch (depth_desc.Format)
            {
            case DXGI_FORMAT_R32G8X24_TYPELESS:
               format = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
               break;
            case DXGI_FORMAT_R32_TYPELESS:
               format = DXGI_FORMAT_R32_FLOAT;
               break;
            case DXGI_FORMAT_R24G8_TYPELESS:
               format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
               break;
            default:
               break;
            }
            const D3D11_SHADER_RESOURCE_VIEW_DESC view_desc = {.Format = format, .ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D, .Texture2D = {.MostDetailedMip = 0, .MipLevels = 1}};
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
         // (centimetres) cancels out. Identity until both cameras are known, or if the current one can't be inverted.
         Math::Matrix44D reprojection;
         reprojection.SetIdentity();
         if (game_device_data.mv_view_projection_valid && game_device_data.mv_previous_view_projection_valid)
         {
            Math::Matrix44D current, previous;
            std::copy_n(game_device_data.mv_view_projection.data(), 16, current.GetData());
            std::copy_n(game_device_data.mv_previous_view_projection.data(), 16, previous.GetData());
            const Math::Matrix44D candidate = current.GetInverted() * previous;
            if (std::all_of(candidate.GetData(), candidate.GetData() + 16, [](double value)
                   { return std::isfinite(value); }))
            {
               reprojection = candidate;
            }
         }
         D3D11_TEXTURE2D_DESC mv_desc;
         game_device_data.mv_texture->GetDesc(&mv_desc);
         float constants[20] = {};
         for (int i = 0; i < 16; i++)
            constants[i] = float(reprojection.GetData()[i]);
         constants[16] = game_device_data.mv_jitter[0] * 2.f / float(mv_desc.Width);
         constants[17] = game_device_data.mv_jitter[1] * -2.f / float(mv_desc.Height);
         const std::shared_lock lock_shader_objects(s_mutex_shader_objects);
         const com_ptr<ID3D11ComputeShader> fill_shader = FindShader(device_data.native_compute_shaders, "P5S Motion Vector Fill CS"_h);
         // ponytail: a shader reload between the clear and here (DEV) leaves the 65504 marker for a frame
         if (depth_srv && fill_shader && PatchedDraws::WriteDynamicConstants(native_device, native_device_context, std::addressof(game_device_data.mv_fill_buffer), constants, sizeof(constants)))
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
      if (!depth || depth.get() != game_device_data.mv_scene_depth_id || !game_device_data.mv_rtv || (game_device_data.mv_frame_ended ? !depth_prepass : native_device_context != game_device_data.mv_scene_context))
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
      if (Perf::g_test == 2)
         return false;
#endif

      DrawWithVertexShader(native_device_context, vertex_shader.get(), MotionVectorPatches::jitter_slot, game_device_data.mv_jitter_buffer.get(), draw);
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
      if (!gbuffer && (game_device_data.mv_frame_ended || native_device_context != game_device_data.mv_scene_context || !depth || depth.get() != game_device_data.mv_scene_depth_id || !game_device_data.mv_rtv))
         return false;
      com_ptr<ID3D11BlendState> blend_state;
      FLOAT blend_factor[4];
      UINT sample_mask;
      native_device_context->OMGetBlendState(&blend_state, blend_factor, &sample_mask);
      D3D11_BLEND_DESC blend_desc = {};
      if (blend_state)
      {
         blend_state->GetDesc(&blend_desc);
      }
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
            desc.RenderTarget[MotionVectorPatches::target_slot] = {.BlendEnable = FALSE, .SrcBlend = D3D11_BLEND_ONE, .DestBlend = D3D11_BLEND_ZERO, .BlendOp = D3D11_BLEND_OP_ADD, .SrcBlendAlpha = D3D11_BLEND_ONE, .DestBlendAlpha = D3D11_BLEND_ZERO, .BlendOpAlpha = D3D11_BLEND_OP_ADD, .RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL};
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
            game_device_data.mv_scene_depth_id = depth.get();
            const std::unique_lock lock(game_device_data.mv_mutex);
            D3D11_TEXTURE2D_DESC desc = {};
            if (game_device_data.mv_texture)
            {
               game_device_data.mv_texture->GetDesc(&desc);
            }
            // R16G16_FLOAT: the patched draws' motion is analytic, and float16 keeps it within 0.1 % at less bandwidth
            const DXGI_FORMAT mv_format = (g_mv_half_float ? DXGI_FORMAT_R16G16_FLOAT : DXGI_FORMAT_R32G32_FLOAT);
            if (desc.Width != depth_size.x || desc.Height != depth_size.y || desc.Format != mv_format)
            {
               game_device_data.mv_texture.reset();
               game_device_data.mv_rtv.reset();
               game_device_data.mv_uav.reset();
               D3D11_FEATURE_DATA_FORMAT_SUPPORT2 support = {mv_format};
               const bool typed_uav_load = SUCCEEDED(native_device->CheckFeatureSupport(D3D11_FEATURE_FORMAT_SUPPORT2, &support, sizeof(support))) && (support.OutFormatSupport2 & D3D11_FORMAT_SUPPORT2_UAV_TYPED_LOAD) != 0;
               desc = {.Width = depth_size.x, .Height = depth_size.y, .MipLevels = 1, .ArraySize = 1, .Format = mv_format, .SampleDesc = {.Count = 1, .Quality = 0}, .Usage = D3D11_USAGE_DEFAULT, .BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | (typed_uav_load ? D3D11_BIND_UNORDERED_ACCESS : 0u)};
               if (FAILED(native_device->CreateTexture2D(&desc, nullptr, &game_device_data.mv_texture)) || FAILED(native_device->CreateRenderTargetView(game_device_data.mv_texture.get(), nullptr, &game_device_data.mv_rtv)))
               {
                  game_device_data.mv_texture.reset();
                  game_device_data.mv_rtv.reset();
                  return false;
               }
               if (typed_uav_load)
               {
                  native_device->CreateUnorderedAccessView(game_device_data.mv_texture.get(), nullptr, &game_device_data.mv_uav);
               }
               game_device_data.mv_frame_ended = true;
            }
         }
         if (game_device_data.mv_frame_ended)
         {
            StartMotionVectorFrame(native_device, native_device_context, device_data, depth_size);
         }
      }
#if DEVELOPMENT
      if (Perf::g_test == 2)
         return false;
#endif

      com_ptr<ID3D11PixelShader> original_pixel_shader;
      native_device_context->PSGetShader(&original_pixel_shader, nullptr, nullptr);
      com_ptr<ID3D11Buffer> globals;
      com_ptr<ID3D11Buffer> original_previous_globals;
      native_device_context->VSGetConstantBuffers(0, 1, &globals);
      native_device_context->VSGetConstantBuffers(MotionVectorPatches::previous_globals_slot, 1, &original_previous_globals);

      ID3D11RenderTargetView* targets[MotionVectorPatches::target_slot + 1] = {};
      std::copy_n(rtvs, MotionVectorPatches::target_slot, targets);
      targets[MotionVectorPatches::target_slot] = game_device_data.mv_rtv.get();
      native_device_context->OMSetRenderTargets(MotionVectorPatches::target_slot + 1, targets, dsv);
      // The previous frame's $Globals: the same object's from last frame, else this draw's with last frame's camera (no object motion).
      // Draws of a buffer without a CPU copy yet get the current data (zero motion).
      const Persona5StrikersGameDeviceData::GlobalsCopy globals_copy = GetGlobalsCopy(&game_device_data, globals.get());
      ID3D11Buffer* previous_globals = globals.get();
      if (globals_copy && layout.view_projection != UINT_MAX && layout.view_projection + 64 <= globals_copy->size())
      {
         const uint8_t* const view_projection = globals_copy->data() + layout.view_projection;
         // The frame's first draw has the camera
         if (!game_device_data.mv_view_projection_valid)
         {
            std::memcpy(game_device_data.mv_view_projection.data(), view_projection, sizeof(game_device_data.mv_view_projection));
            game_device_data.mv_view_projection_valid = true;
         }

         // Draw key: same mesh, same shaders. Objects sharing it (e.g. props) are told apart by their world matrix.
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
         PatchedDraws::ObjectTransform transform = {};
         if (layout.world != UINT_MAX && layout.world + sizeof(transform) <= globals_copy->size())
         {
            std::memcpy(transform.data(), globals_copy->data() + layout.world, sizeof(transform));
         }

         // ponytail: linear search among the key's candidates (a handful at most); a spatial lookup if big crowds share a mesh
         const Persona5StrikersGameDeviceData::MotionVectorObject* match = nullptr;
         if (const auto previous = game_device_data.mv_previous_objects.find(key); previous != game_device_data.mv_previous_objects.end())
         {
            match = PatchedDraws::FindNearest(previous->second, transform, [&](const auto& object)
               { return object.globals->size() == globals_copy->size(); });
         }
         // Kept as drawn for the next frame
         game_device_data.mv_objects[key].push_back({transform, globals_copy});
         const std::vector<uint8_t>* upload_data = (match ? match->globals.get() : globals_copy.get());
         // Not found: last frame's camera, if this is the frame's view projection (others are left as is)
         if (!match && game_device_data.mv_previous_view_projection_valid && std::memcmp(view_projection, game_device_data.mv_view_projection.data(), sizeof(game_device_data.mv_view_projection)) == 0)
         {
            game_device_data.mv_globals_scratch = *globals_copy;
            std::memcpy(game_device_data.mv_globals_scratch.data() + layout.view_projection, game_device_data.mv_previous_view_projection.data(), sizeof(game_device_data.mv_previous_view_projection));
            upload_data = &game_device_data.mv_globals_scratch;
         }

         com_ptr<ID3D11Buffer>& upload = game_device_data.mv_previous_globals_buffers[UINT(upload_data->size())];
         if (PatchedDraws::WriteDynamicConstants(native_device, native_device_context, std::addressof(upload), upload_data->data(), UINT(upload_data->size())))
         {
            previous_globals = upload.get();
         }
      }
      native_device_context->VSSetConstantBuffers(MotionVectorPatches::previous_globals_slot, 1, &previous_globals);
      // Second run resources, in the slots the shader reads: the previous frame's copies, else the current ones (immutable, or no
      // previous copy)
      com_ptr<ID3D11ShaderResourceView> srvs[MotionVectorPatches::resource_slots];
      if (layout.resource_slots != 0)
      {
         native_device_context->VSGetShaderResources(0, MotionVectorPatches::resource_slots, &srvs[0]);
      }
      ID3D11ShaderResourceView* previous_srvs[MotionVectorPatches::resource_slots] = {};
      for (UINT slot = 0; slot < MotionVectorPatches::resource_slots; slot++)
      {
         if ((layout.resource_slots & (1u << slot)) == 0)
            continue;
         previous_srvs[slot] = srvs[slot].get();
         const com_ptr<ID3D11Resource> resource = GetViewResource(srvs[slot].get());
         D3D11_RESOURCE_DIMENSION dimension = D3D11_RESOURCE_DIMENSION_UNKNOWN;
         if (resource)
         {
            resource->GetType(&dimension);
         }
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
                  buffer_desc = {.ByteWidth = buffer_desc.ByteWidth, .Usage = D3D11_USAGE_DEFAULT, .BindFlags = D3D11_BIND_SHADER_RESOURCE, .CPUAccessFlags = 0, .MiscFlags = buffer_desc.MiscFlags & (D3D11_RESOURCE_MISC_BUFFER_STRUCTURED | D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS), .StructureByteStride = buffer_desc.StructureByteStride};
                  if (SUCCEEDED(native_device->CreateBuffer(&buffer_desc, nullptr, &buffer)))
                  {
                     buffer->QueryInterface(&entry.copies[0]);
                  }
                  break;
               case D3D11_RESOURCE_DIMENSION_TEXTURE1D:
                  texture_1d_desc = {.Width = texture_1d_desc.Width, .MipLevels = texture_1d_desc.MipLevels, .ArraySize = texture_1d_desc.ArraySize, .Format = texture_1d_desc.Format, .Usage = D3D11_USAGE_DEFAULT, .BindFlags = D3D11_BIND_SHADER_RESOURCE};
                  if (SUCCEEDED(native_device->CreateTexture1D(&texture_1d_desc, nullptr, &texture_1d)))
                  {
                     texture_1d->QueryInterface(&entry.copies[0]);
                  }
                  break;
               case D3D11_RESOURCE_DIMENSION_TEXTURE2D:
                  texture_2d_desc = {.Width = texture_2d_desc.Width, .Height = texture_2d_desc.Height, .MipLevels = texture_2d_desc.MipLevels, .ArraySize = texture_2d_desc.ArraySize, .Format = texture_2d_desc.Format, .SampleDesc = texture_2d_desc.SampleDesc, .Usage = D3D11_USAGE_DEFAULT, .BindFlags = D3D11_BIND_SHADER_RESOURCE, .CPUAccessFlags = 0, .MiscFlags = texture_2d_desc.MiscFlags & D3D11_RESOURCE_MISC_TEXTURECUBE};
                  if (SUCCEEDED(native_device->CreateTexture2D(&texture_2d_desc, nullptr, &texture_2d)))
                  {
                     texture_2d->QueryInterface(&entry.copies[0]);
                  }
                  break;
               case D3D11_RESOURCE_DIMENSION_TEXTURE3D:
                  texture_3d_desc = {.Width = texture_3d_desc.Width, .Height = texture_3d_desc.Height, .Depth = texture_3d_desc.Depth, .MipLevels = texture_3d_desc.MipLevels, .Format = texture_3d_desc.Format, .Usage = D3D11_USAGE_DEFAULT, .BindFlags = D3D11_BIND_SHADER_RESOURCE};
                  if (SUCCEEDED(native_device->CreateTexture3D(&texture_3d_desc, nullptr, &texture_3d)))
                  {
                     texture_3d->QueryInterface(&entry.copies[0]);
                  }
                  break;
               }
            }
            if (entry.copies[0])
            {
               native_device_context->CopyResource(entry.copies[0].get(), resource.get());
            }
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
         {
            previous_srvs[slot] = entry.views[1].get();
         }
      }
      if (layout.resource_slots != 0)
      {
         native_device_context->VSSetShaderResources(MotionVectorPatches::previous_resources_slot, MotionVectorPatches::resource_slots, previous_srvs);
      }
      native_device_context->PSSetShader(pixel_shader.get(), nullptr, 0);
      if (motion_vector_blend_state)
      {
         native_device_context->OMSetBlendState(motion_vector_blend_state.get(), blend_factor, sample_mask);
      }
      DrawWithVertexShader(native_device_context, vertex_shader.get(), MotionVectorPatches::jitter_slot, game_device_data.mv_jitter_buffer.get(), draw);
      if (motion_vector_blend_state)
      {
         native_device_context->OMSetBlendState(blend_state.get(), blend_factor, sample_mask);
      }
      // Set directly, bypassing Core's state tracking: restore the game's
      native_device_context->PSSetShader(original_pixel_shader.get(), nullptr, 0);
      ID3D11Buffer* const restored_globals = original_previous_globals.get();
      native_device_context->VSSetConstantBuffers(MotionVectorPatches::previous_globals_slot, 1, &restored_globals);
      if (layout.resource_slots != 0)
      {
         ID3D11ShaderResourceView* const null_srvs[MotionVectorPatches::resource_slots] = {};
         native_device_context->VSSetShaderResources(MotionVectorPatches::previous_resources_slot, MotionVectorPatches::resource_slots, null_srvs);
      }
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
         const UINT8 write_mask = (i == 0 ? D3D11_COLOR_WRITE_ENABLE_ALL : D3D11_COLOR_WRITE_ENABLE_ALPHA);
         D3D11_BLEND_DESC blend_desc = {};
         blend_desc.RenderTarget[0] = {.BlendEnable = TRUE, .SrcBlend = D3D11_BLEND_ONE, .DestBlend = D3D11_BLEND_ONE, .BlendOp = D3D11_BLEND_OP_MIN, .SrcBlendAlpha = D3D11_BLEND_ONE, .DestBlendAlpha = D3D11_BLEND_ONE, .BlendOpAlpha = D3D11_BLEND_OP_MIN, .RenderTargetWriteMask = write_mask};
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
      // GameDeviceData lacks a virtual destructor; delete through the concrete type to release derived members. Cleared first: those
      // releases can fire "destroy_resource", whose hook reads it.
      delete static_cast<Persona5StrikersGameDeviceData*>(std::exchange(device_data.game, nullptr));
   }

   void OnInit(bool async) override
   {
#if DEVELOPMENT
      // For the MCP "luma_dev_values" tool
      Mcp::RegisterToggles({{"hide_ui", &g_hide_ui}, {"gtao_enable", &g_gtao_enable}, {"smaa_predication", &g_smaa_predication}, {"post_output_resolution", &g_post_output_resolution}, {"mv_globals_filter", &g_mv_globals_filter}, { "mv_half_float",
                               &g_mv_half_float }});
      Mcp::RegisterCounter("mv.tiebreak_collisions", &g_mv_tiebreak_collisions_last_frame);
      Mcp::RegisterCounters({{"jitter.draws", &g_jitter_draws_last_frame}, {"jitter.skipped_no_vb", &g_jitter_skipped_no_vb_last_frame}, { "jitter.skipped_unpatched",
                                &g_jitter_skipped_unpatched_last_frame }});
      Mcp::RegisterValues({{"gtao_final_value_power", &g_gtao_final_value_power, 0.3f, 4.5f}, {"gtao_radius_override", &g_gtao_radius_override, 0.f, 200.f}});
      Mcp::RegisterInts({{"render_scale", &g_render_scale, 5, 10}, {"gtao_debug_view", &g_gtao_debug_view, 0, 4}, {"smaa_debug_view", &g_smaa_debug_view, 0, 2},
         { "perf_test",
            &Perf::g_test,
            0,
            int(std::size(perf_test_modes)) - 1,
            [](DeviceData& device_data, double value)
            {
               // As the "Performance Test" combo
               if (value != 0.0 && !IsSRActive(device_data))
                  return std::string("Needs DLSS or FSR");
               Perf::g_test = int(value);
               return std::string();
            } }});
      Mcp::RegisterTextures({MCP_GAME_TEXTURE("smaa.input_linear", smaa_linear_texture),
         MCP_GAME_TEXTURE("smaa.input", smaa_gamma_srv),
         MCP_GAME_TEXTURE("smaa.pred_mask", smaa_predication_srv),
         MCP_GAME_TEXTURE("gtao.depth_mips", gtao_depth_mips_srv),
         MCP_GAME_TEXTURE("gtao.output", gtao_final_texture),
         MCP_GAME_TEXTURE("mv.velocity", mv_texture),
         MCP_GAME_TEXTURE("mv.depth", mv_scene_depth_srv),
         MCP_GAME_TEXTURE("sr.output", sr_upscaled_output)});
#endif
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
      native_shaders_definitions.emplace(CompileTimeStringHash("P5S UI Coverage Clamp PS"), ShaderDefinition{"Luma_P5S_UICoverageClamp", reshade::api::pipeline_subobject_type::pixel_shader});
      native_shaders_definitions.emplace(CompileTimeStringHash("P5S Layer Quad VS"), ShaderDefinition{"Luma_P5S_LayerQuad", reshade::api::pipeline_subobject_type::vertex_shader});
      native_shaders_definitions.emplace(CompileTimeStringHash("P5S Layer Sprite VS"), ShaderDefinition{"Luma_P5S_LayerSprite", reshade::api::pipeline_subobject_type::vertex_shader});
      native_shaders_definitions.emplace(CompileTimeStringHash("P5S Bloom Downsample PS"), ShaderDefinition{"Luma_P5S_Bloom", reshade::api::pipeline_subobject_type::pixel_shader, nullptr, "downsample_ps"});
      native_shaders_definitions.emplace(CompileTimeStringHash("P5S Bloom Upsample PS"), ShaderDefinition{"Luma_P5S_Bloom", reshade::api::pipeline_subobject_type::pixel_shader, nullptr, "upsample_ps"});
      // XeGTAO passes (Luma_P5S_XeGTAO.hlsl); the two denoisers differ only by XE_GTAO_FINAL_APPLY.
      native_shaders_definitions.emplace(CompileTimeStringHash("P5S XeGTAO Prefilter Depths CS"), ShaderDefinition{"Luma_P5S_XeGTAO", reshade::api::pipeline_subobject_type::compute_shader, nullptr, "prefilter_depths16x16_cs"});
      native_shaders_definitions.emplace(CompileTimeStringHash("P5S XeGTAO Main Pass CS"), ShaderDefinition{"Luma_P5S_XeGTAO", reshade::api::pipeline_subobject_type::compute_shader, nullptr, "main_pass_cs"});
      native_shaders_definitions.emplace(CompileTimeStringHash("P5S XeGTAO Denoise Pass 1 CS"), ShaderDefinition{"Luma_P5S_XeGTAO", reshade::api::pipeline_subobject_type::compute_shader, nullptr, "denoise_pass_cs", {{"XE_GTAO_FINAL_APPLY", "0"}}});
      native_shaders_definitions.emplace(CompileTimeStringHash("P5S XeGTAO Denoise Pass 2 CS"), ShaderDefinition{"Luma_P5S_XeGTAO", reshade::api::pipeline_subobject_type::compute_shader, nullptr, "denoise_pass_cs", {{"XE_GTAO_FINAL_APPLY", "1"}}});

      reshade::register_event<reshade::addon_event::map_buffer_region>(OnMapBufferRegion);
      reshade::register_event<reshade::addon_event::unmap_buffer_region>(OnUnmapBufferRegion);
      reshade::register_event<reshade::addon_event::destroy_resource>(OnDestroyResource);
      reshade::register_event<reshade::addon_event::execute_secondary_command_list>(OnExecuteSecondaryCommandList);
   }

   static void UnregisterEvents()
   {
      reshade::unregister_event<reshade::addon_event::map_buffer_region>(OnMapBufferRegion);
      reshade::unregister_event<reshade::addon_event::unmap_buffer_region>(OnUnmapBufferRegion);
      reshade::unregister_event<reshade::addon_event::destroy_resource>(OnDestroyResource);
      reshade::unregister_event<reshade::addon_event::execute_secondary_command_list>(OnExecuteSecondaryCommandList);
   }

   // Draws the composite, then SMAA on its canvas (swapchain, upscaled canvas, or without DLSS/FSR below render scale 1 its own target,
   // which the game stretches onto the swapchain) before the UI: copy, gamma encode, predication, SMAA into the gamma copy, finalize
   // (RCAS, decode, dither) into the canvas; without RCAS, SMAA writes the canvas. With "smaa" false (DLSS/FSR antialiased) only RCAS:
   // copy (not of the upscaled canvas), gamma encode, finalize. If anything is missing (shaders compiling, unexpected target) the
   // composite is left alone and the vanilla FXAA runs (not after DLSS/FSR).
   static DrawOrDispatchOverrideType DrawCompositeWithSMAAAndRCAS(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, CommandListData& cmd_list_data, DeviceData& device_data, bool* updated_cbuffers, const std::function<void()>& original_draw_dispatch_func, bool smaa)
   {
      auto& game_device_data = GetGameDeviceData(device_data);
      com_ptr<ID3D11RenderTargetView> canvas_rtv;
      com_ptr<ID3D11DepthStencilView> canvas_dsv;
      native_device_context->OMGetRenderTargets(1, &canvas_rtv, &canvas_dsv);
      const com_ptr<ID3D11Resource> canvas_resource = GetViewResource(canvas_rtv.get());
      com_ptr<ID3D11Texture2D> canvas_texture;
      if (!canvas_resource || FAILED(canvas_resource->QueryInterface(&canvas_texture)))
         return DrawOrDispatchOverrideType::None;
      D3D11_TEXTURE2D_DESC canvas_desc;
      canvas_texture->GetDesc(&canvas_desc);
      // Not the main menu's second, off-screen composite (RGBA8, see the format check)
      if (!IsGameBackBuffer(&device_data, canvas_resource.get()) && canvas_rtv != game_device_data.sr_upscaled_canvas_rtv && (IsSRActive(device_data) || canvas_desc.Width >= GetOutputSize(device_data).x))
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

      const std::unique_lock lock_smaa(game_device_data.smaa_mutex);
      game_device_data.smaa_copies_frame = cb_luma_global_settings.FrameIndex;
      if (smaa)
      {
         game_device_data.smaa_frame = cb_luma_global_settings.FrameIndex;
      }
      // RCAS alone on the upscaled canvas encodes straight from the canvas's view; SMAA needs the copy, as it samples it while writing the
      // canvas, and so does RCAS on the swapchain
      ID3D11ShaderResourceView* const canvas_srv = ((!smaa && canvas_rtv == game_device_data.sr_upscaled_canvas_rtv) ? game_device_data.sr_upscaled_canvas_srv.get() : nullptr);
      D3D11_TEXTURE2D_DESC scratch_desc = canvas_desc;
      scratch_desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
      scratch_desc.MipLevels = 1;
      scratch_desc.Usage = D3D11_USAGE_DEFAULT;
      scratch_desc.CPUAccessFlags = 0;
      scratch_desc.MiscFlags = 0;
      if (GetViewTextureSize(game_device_data.smaa_gamma_srv.get()) != uint2{canvas_desc.Width, canvas_desc.Height})
      {
         game_device_data.ReleaseSMAACopies();
         game_device_data.smaa_predication_srv.reset();
         game_device_data.smaa_predication_uav.reset();
         scratch_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET | D3D11_BIND_UNORDERED_ACCESS;
         com_ptr<ID3D11Texture2D> gamma_texture;
         if (FAILED(native_device->CreateTexture2D(&scratch_desc, nullptr, &gamma_texture)) || FAILED(native_device->CreateShaderResourceView(gamma_texture.get(), nullptr, &game_device_data.smaa_gamma_srv)) || FAILED(native_device->CreateRenderTargetView(gamma_texture.get(), nullptr, &game_device_data.smaa_gamma_rtv)) || FAILED(native_device->CreateUnorderedAccessView(gamma_texture.get(), nullptr, &game_device_data.smaa_gamma_uav)))
         {
            // Retried next frame (the size check above sees no texture)
            game_device_data.ReleaseSMAACopies();
            return DrawOrDispatchOverrideType::None;
         }
      }
      if (canvas_srv)
      {
         game_device_data.smaa_linear_texture.reset();
         game_device_data.smaa_linear_srv.reset();
      }
      else if (!game_device_data.smaa_linear_texture)
      {
         scratch_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
         if (FAILED(native_device->CreateTexture2D(&scratch_desc, nullptr, &game_device_data.smaa_linear_texture)) || FAILED(native_device->CreateShaderResourceView(game_device_data.smaa_linear_texture.get(), nullptr, &game_device_data.smaa_linear_srv)))
         {
            // Retried next frame
            game_device_data.smaa_linear_texture.reset();
            game_device_data.smaa_linear_srv.reset();
            return DrawOrDispatchOverrideType::None;
         }
      }
      // Created on SMAA's first use after a resize or an idle release; without it SMAA simply runs unpredicated.
      if (smaa && g_smaa_predication && !game_device_data.smaa_predication_uav)
      {
         D3D11_TEXTURE2D_DESC desc;
         game_device_data.smaa_linear_texture->GetDesc(&desc);
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
         {
            depth_srv.reset();
         }
      }

      // The replaced composite reads the Luma cbuffers, which Core binds only after this callback; they stay bound for SMAA and finalize.
      // The data carries the canvas size (CustomData1/2: the swapchain's, or the render resolution) and the predication scale
      // (CustomData3), never 0 here, which also defers the composite's dither to the chain's end so RCAS doesn't sharpen it.
      SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, reshade::api::shader_stage::vertex | reshade::api::shader_stage::pixel, LumaConstantBufferType::LumaSettings);
      SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, reshade::api::shader_stage::vertex | reshade::api::shader_stage::pixel, LumaConstantBufferType::LumaData, canvas_desc.Width, canvas_desc.Height, depth_srv ? 2.f : 1.f);
      *updated_cbuffers = true;
      original_draw_dispatch_func();
      if (canvas_srv)
      {
         // Still the composite's render target, which would null the view bound for reading
         native_device_context->OMSetRenderTargets(0, nullptr, nullptr);
      }
      else
      {
         native_device_context->CopyResource(game_device_data.smaa_linear_texture.get(), canvas_resource.get());
      }

      {
         DrawStateStack<DrawStateStackType::Compute> compute_state;
         compute_state.Cache(native_device_context, device_data.uav_max_count);
         ID3D11UnorderedAccessView* const gamma_uav = game_device_data.smaa_gamma_uav.get();
         ID3D11ShaderResourceView* const linear_srv = (canvas_srv ? canvas_srv : game_device_data.smaa_linear_srv.get());
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
      if (canvas_srv)
      {
         ID3D11RenderTargetView* const rtv = canvas_rtv.get();
         native_device_context->OMSetRenderTargets(1, &rtv, canvas_dsv.get());
      }

      // Without RCAS, SMAA writes (and dithers) the canvas directly, only sampling the linear copy; finalize would only decode
      const bool sharpen = cb_luma_global_settings.GameSettings.RCASSharpness > 0.f;
      if (smaa)
      {
         DrawSMAA(native_device, native_device_context, device_data, sharpen ? game_device_data.smaa_gamma_rtv.get() : canvas_rtv.get(), game_device_data.smaa_linear_srv.get(), game_device_data.smaa_gamma_srv.get(), depth_srv ? game_device_data.smaa_predication_srv.get() : nullptr);
      }

      // Development builds may also draw a debug view
      if (sharpen || DEVELOPMENT)
      {
         DrawStateStack<DrawStateStackType::FullGraphics> finalize_state;
         finalize_state.Cache(native_device_context, device_data.uav_max_count);
         if (sharpen)
         {
            DrawCustomPixelShader(native_device_context, device_data.default_depth_stencil_state.get(), device_data.default_blend_state.get(), nullptr, device_data.native_vertex_shaders.at("Copy VS"_h).get(), device_data.native_pixel_shaders.at("P5S SMAA Finalize PS"_h).get(), game_device_data.smaa_gamma_srv.get(), canvas_rtv.get(), canvas_desc.Width, canvas_desc.Height, false);
         }
#if DEVELOPMENT
         // Calibration aid: SMAA's edges (red = horizontal, green = vertical) or the predication edge-ness (red) replace the frame
         ID3D11ShaderResourceView* const edges_srv = device_data.managed_resources.shader_resource_views["smaa_edge_detection"_h].get();
         ID3D11ShaderResourceView* const debug_srv = (g_smaa_debug_view == 1 && smaa ? edges_srv : (g_smaa_debug_view == 2 && depth_srv ? game_device_data.smaa_predication_srv.get() : nullptr));
         if (debug_srv && HasShaders(device_data.native_pixel_shaders, "Copy PS"_h))
         {
            DrawCustomPixelShader(native_device_context, device_data.default_depth_stencil_state.get(), device_data.default_blend_state.get(), nullptr, device_data.native_vertex_shaders.at("Copy VS"_h).get(), device_data.native_pixel_shaders.at("Copy PS"_h).get(), debug_srv, canvas_rtv.get(), canvas_desc.Width, canvas_desc.Height, false);
         }
#endif
         finalize_state.Restore(native_device_context);
      }

      game_device_data.scene_antialiased = true;
      return DrawOrDispatchOverrideType::Replaced;
   }

   // XeGTAO in place of the SSAO calculate draw: prefilter, main pass and denoisers on its inputs (t0 half res depth, t1 full res
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
         // Retried next time (no size) if anything failed
         if (ok)
         {
            game_device_data.gtao_width = width;
            game_device_data.gtao_height = height;
         }
         else
         {
            game_device_data.ReleaseGTAOScratch();
         }
      }
      if (!game_device_data.gtao_final_uav)
         return false;

      // DLSS/FSR accumulate the lit scene the AO feeds: cycle the noise and denoise once (Intel's XeGTAO.h with TAA); without
      // them a moving pattern would boil, so it stays frozen and denoises twice
      const bool temporal = IsSRActive(device_data);
      const float knobs[8] = {g_gtao_final_value_power, float(normal_input_scale), g_gtao_radius_override, float(g_gtao_debug_view), 1.f / float(width), 1.f / float(height), temporal ? float(cb_luma_global_settings.FrameIndex % 64) : 0.f, 0.f};
      if (!PatchedDraws::WriteDynamicConstants(native_device, native_device_context, std::addressof(game_device_data.gtao_knobs_cb), knobs, sizeof(knobs)))
         return false;

      DrawStateStack<DrawStateStackType::Compute> compute_state;
      compute_state.Cache(native_device_context, device_data.uav_max_count);
      // $Globals may be a range of a bigger buffer; a zero count means a plain binding
      ID3D11Buffer* const cbs[] = {globals_cb.get(), game_device_data.gtao_knobs_cb.get()};
      if (globals_count != 0)
      {
         native_device_context1->CSSetConstantBuffers1(0, 1, &cbs[0], &globals_first, &globals_count);
      }
      else
      {
         native_device_context->CSSetConstantBuffers(0, 1, &cbs[0]);
      }
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
      if (!temporal)
      {
         pass("P5S XeGTAO Denoise Pass 1 CS"_h, 1, &working_uavs[1], {game_device_data.gtao_working_srvs[0].get(), nullptr}, (width + 15) / 16, (height + 7) / 8);
      }
      pass("P5S XeGTAO Denoise Pass 2 CS"_h, 1, &final_uav, {game_device_data.gtao_working_srvs[temporal ? 0 : 1].get(), nullptr}, (width + 15) / 16, (height + 7) / 8);
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
            {
               game_device_data.sr_pending_splits.clear();
            }
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
      {
         native_device_context->Begin(perf_queries->disjoint.get());
      }
#endif
      // Always, even if the upscaler went away meanwhile: it starts the game's frame
      native_device_context->ExecuteCommandList(split.partial.get(), FALSE);
#if DEVELOPMENT
      if (perf_queries)
      {
         native_device_context->End(perf_queries->sr_start.get());
      }
#endif

      D3D11_TEXTURE2D_DESC desc;
      split.source_color->GetDesc(&desc);
      bool drawn = false;
      // The instance too: the selection is taken at present, Core may have changed it since. NGX and FSR run on the immediate context
      // only (the game could execute the remainder into another command list).
      if (native_device_context->GetType() == D3D11_DEVICE_CONTEXT_IMMEDIATE && IsSRActive(*device_data) && split.motion_vectors && device_data->GetSRInstanceData())
      {
         // Written as a UAV. Upscaling: into the split's output, read by the post process; DLAA: copied back into the scene.
         D3D11_TEXTURE2D_DESC output_desc = {};
         bool output_recreated = split.output_color && split.output_color != game_device_data.sr_drawn_output;
         if (!split.output_color)
         {
            if (device_data->sr_output_color)
            {
               device_data->sr_output_color->GetDesc(&output_desc);
            }
            if (output_desc.Width != desc.Width || output_desc.Height != desc.Height)
            {
               device_data->sr_output_color.reset();
               output_desc = {.Width = desc.Width, .Height = desc.Height, .MipLevels = 1, .ArraySize = 1, .Format = DXGI_FORMAT_R16G16B16A16_FLOAT, .SampleDesc = {.Count = 1, .Quality = 0}, .Usage = D3D11_USAGE_DEFAULT, .BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS};
               native_device->CreateTexture2D(&output_desc, nullptr, &device_data->sr_output_color);
               output_recreated = true;
            }
         }
         ID3D11Texture2D* const output_color = (split.output_color ? split.output_color.get() : device_data->sr_output_color.get());
         if (output_color)
         {
            output_color->GetDesc(&output_desc);
         }

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
         draw_data.motion_vectors = split.motion_vectors.get();
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
            // DLAA writes back into the scene; upscaling leaves the output to the post process (see "RedirectPostDraw")
            if (!split.output_color)
            {
               native_device_context->CopySubresourceRegion(split.source_color.get(), 0, 0, 0, 0, output_color, 0, nullptr);
            }
            // DLSS draws nothing into a new output texture (the session's first, or one made after "None", which Core frees): the frame
            // shows the texture's stale memory until its feature is created again after a draw. Settings changed once here force that at
            // the next frame's "UpdateSettings".
            if (output_recreated && device_data->sr_type == SR::Type::DLSS)
            {
               SR::SettingsData throwaway_settings_data = settings_data;
               throwaway_settings_data.mvs_jittered = !throwaway_settings_data.mvs_jittered;
               sr_implementations[device_data->sr_type]->UpdateSettings(sr_instance_data, native_device_context.get(), throwaway_settings_data);
            }
            device_data->has_drawn_sr = true;
            game_device_data.sr_drawn_output = split.output_color;
            drawn = true;
         }
         else
         {
            // Back to SMAA until the upscaler is picked again
            device_data->sr_suppressed = true;
         }
      }
      // The post process reads the upscaled output: without the upscaler it gets the scene stretched, not the texture's old contents
      if (!drawn && split.output_color)
      {
         const D3D11_SHADER_RESOURCE_VIEW_DESC source_srv_desc = {.Format = DXGI_FORMAT_R16G16B16A16_FLOAT, .ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D, .Texture2D = {.MostDetailedMip = 0, .MipLevels = 1}};
         com_ptr<ID3D11ShaderResourceView> source_srv;
         com_ptr<ID3D11RenderTargetView> output_rtv;
         if (SUCCEEDED(native_device->CreateShaderResourceView(split.source_color.get(), &source_srv_desc, &source_srv)) && SUCCEEDED(native_device->CreateRenderTargetView(split.output_color.get(), nullptr, &output_rtv)))
         {
            D3D11_TEXTURE2D_DESC output_desc;
            split.output_color->GetDesc(&output_desc);
            DrawScaled(native_device_context.get(), *device_data, source_srv.get(), output_rtv.get(), output_desc.Width, output_desc.Height);
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
            {
               offset = variable.StartOffset;
            }
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
         const D3D11_BUFFER_DESC desc = {.ByteWidth = sizeof(uv_scale), .Usage = D3D11_USAGE_IMMUTABLE, .BindFlags = D3D11_BIND_CONSTANT_BUFFER};
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
      const uint2 output_size = GetOutputSize(device_data);
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
         if (!source || it == game_device_data.layer_frames.end() || !Containers::Contains(it->second.targets, source.get()))
            return false;
         std::copy_n(it->second.scale, 2, scale);
         game_device_data.layer_frames.erase(it);
         com_ptr<ID3D11RenderTargetView> rtv;
         native_device_context->OMGetRenderTargets(1, &rtv, nullptr);
         if (const com_ptr<ID3D11Resource> stretch_target = GetViewResource(rtv.get()))
         {
            game_device_data.layer_outputs[stretch_target.get()] = {scale[0], cb_luma_global_settings.FrameIndex};
         }
      }
      else
      {
         com_ptr<ID3D11RenderTargetView> rtv;
         com_ptr<ID3D11DepthStencilView> dsv;
         native_device_context->OMGetRenderTargets(1, &rtv, &dsv);
         target = GetViewResource(rtv.get());
         if (com_ptr<ID3D11Texture2D> texture; target && SUCCEEDED(target->QueryInterface(&texture)))
         {
            texture->GetDesc(&target_desc);
         }
         // An output sized off-screen target, with the viewport at its top left corner
         if (target_desc.Width != output_size.x || target_desc.Height != output_size.y || IsGameBackBuffer(&device_data, target.get()))
            return false;
         if (dsv)
         {
            uint4 depth_size;
            DXGI_FORMAT depth_format;
            GetResourceInfo(dsv.get(), depth_size, depth_format);
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
         {
            quad_vertex_shader.reset();
         }
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
      const UINT cluster_scale_offset = (stretch ? UINT_MAX : GetClusterScaleOffset(device_data, original_shader_hashes.pixel_shaders[0], cmd_list_data.pipeline_state_original_pixel_shader));
      if (cluster_scale_offset != UINT_MAX)
      {
         native_device_context->PSGetConstantBuffers(0, 1, &original_globals);
         const Persona5StrikersGameDeviceData::GlobalsCopy globals_copy = (original_globals ? GetGlobalsCopy(&game_device_data, original_globals.get()) : nullptr);
         if (globals_copy && cluster_scale_offset + sizeof(float) <= globals_copy->size())
         {
            std::vector<uint8_t> globals_data = *globals_copy;
            float cluster_scale;
            std::memcpy(&cluster_scale, globals_data.data() + cluster_scale_offset, sizeof(float));
            cluster_scale *= scale[0];
            std::memcpy(globals_data.data() + cluster_scale_offset, &cluster_scale, sizeof(float));
            const std::lock_guard lock(game_device_data.layer_mutex);
            com_ptr<ID3D11Buffer>& upload = game_device_data.layer_globals_buffers[UINT(globals_data.size())];
            if (PatchedDraws::WriteDynamicConstants(native_device, native_device_context, std::addressof(upload), globals_data.data(), UINT(globals_data.size())))
            {
               patched_globals = upload;
            }
         }
      }

      if (!stretch)
      {
         const std::lock_guard lock(game_device_data.layer_mutex);
         auto& frame = game_device_data.layer_frames[native_device_context];
         if (!Containers::Contains(frame.targets, target.get()))
         {
            frame.targets.push_back(target.get());
         }
         std::copy_n(scale, 2, frame.scale);
      }

      // Set directly (bypassing Core's state tracking), then restored
      D3D11_RECT scissor = {};
      UINT scissors = 1;
      native_device_context->RSGetScissorRects(&scissors, &scissor);
      if (!stretch)
      {
         SetFullTargetViewport(native_device_context, target_desc.Width, target_desc.Height, viewport.MinDepth, viewport.MaxDepth);
      }
      if (patched_globals)
      {
         ID3D11Buffer* const buffer = patched_globals.get();
         native_device_context->PSSetConstantBuffers(0, 1, &buffer);
      }
      DrawWithVertexShader(native_device_context, quad_vertex_shader.get(), layer_uv_scale_cb_slot, uv_scale_buffer.get(), draw);
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

   // The output resolution copy of a post process target made by halving the render resolution (DOF, bloom), sized as the game makes
   // it at the output resolution; created on first use if "create", null for anything else (exposure, lookup tables)
   static Persona5StrikersGameDeviceData::PostTarget* GetPostTarget(ID3D11Device* native_device, Persona5StrikersGameDeviceData* game_device_data, ID3D11Resource* resource, bool create)
   {
      if (const auto it = game_device_data->post_targets.find(resource); it != game_device_data->post_targets.end())
         return &it->second;
      com_ptr<ID3D11Texture2D> texture;
      if (!create || FAILED(resource->QueryInterface(&texture)))
         return nullptr;
      D3D11_TEXTURE2D_DESC desc;
      texture->GetDesc(&desc);
      const uint2 render = game_device_data->post_sizes[0];
      const uint2 output = game_device_data->post_sizes[1];
      const int halvings = int(std::lround(std::log2(double(render.x) / double((std::max)(desc.Width, 1u)))));
      if (halvings < 0 || desc.SampleDesc.Count != 1 || desc.ArraySize != 1 || std::abs(int(desc.Width) - int(render.x >> halvings)) > 1 || std::abs(int(desc.Height) - int(render.y >> halvings)) > 1)
         return nullptr;
      desc.Width = GetTextureMipSize(output.x, halvings);
      desc.Height = GetTextureMipSize(output.y, halvings);
      if (desc.MipLevels != 1)
      {
         desc.MipLevels = 0; // The full chain: the bloom adds iterations past the game's last mip
      }
      Persona5StrikersGameDeviceData::PostTarget target;
      target.original = resource;
      if (FAILED(native_device->CreateTexture2D(&desc, nullptr, &target.texture)))
         return nullptr;
      return &game_device_data->post_targets.emplace(resource, std::move(target)).first->second;
   }

   // A post process view on the output resolution copy of its texture (the upscaler's output for the scene), made once per game view;
   // null without one. Render targets get a copy made, shader resources only use an existing one (written by a redirected pass).
   template <typename T>
   static com_ptr<T> RedirectView(ID3D11Device* native_device, Persona5StrikersGameDeviceData* game_device_data, T* view)
   {
      constexpr bool rtv = std::is_same_v<T, ID3D11RenderTargetView>;
      if (const auto it = game_device_data->post_views.find(view); it != game_device_data->post_views.end())
         return com_ptr<T>(static_cast<T*>(it->second.replacement.get()));
      const com_ptr<ID3D11Resource> resource = GetViewResource(view);
      if (!resource)
         return nullptr;
      if (resource == game_device_data->post_scene)
      {
         if constexpr (rtv)
            return game_device_data->sr_upscaled_output_rtv;
         else
            return game_device_data->sr_upscaled_output_srv;
      }
      const auto* const target = GetPostTarget(native_device, game_device_data, resource.get(), rtv);
      if (!target)
         return nullptr;
      com_ptr<T> replacement;
      if constexpr (rtv)
      {
         D3D11_RENDER_TARGET_VIEW_DESC desc;
         view->GetDesc(&desc);
         native_device->CreateRenderTargetView(target->texture.get(), &desc, &replacement);
      }
      else
      {
         D3D11_SHADER_RESOURCE_VIEW_DESC desc;
         view->GetDesc(&desc);
         native_device->CreateShaderResourceView(target->texture.get(), &desc, &replacement);
      }
      if (replacement)
      {
         game_device_data->post_views.emplace(view, Persona5StrikersGameDeviceData::PostView{.original = com_ptr<ID3D11View>(view), .replacement = com_ptr<ID3D11View>(replacement.get())});
      }
      return replacement;
   }

   // Draws "source" stretched over "target" (width x height) with Core's scale shaders, restoring the graphics state; false without them
   static bool DrawScaled(ID3D11DeviceContext* native_device_context, DeviceData& device_data, ID3D11ShaderResourceView* source, ID3D11RenderTargetView* target, UINT width, UINT height)
   {
      com_ptr<ID3D11VertexShader> vertex_shader;
      com_ptr<ID3D11PixelShader> pixel_shader;
      {
         const std::shared_lock lock_shader_objects(s_mutex_shader_objects);
         vertex_shader = FindShader(device_data.native_vertex_shaders, "Scale VS"_h);
         pixel_shader = FindShader(device_data.native_pixel_shaders, "Scale PS"_h);
      }
      if (!vertex_shader || !pixel_shader)
         return false;
      DrawStateStack<DrawStateStackType::FullGraphics> state;
      state.Cache(native_device_context, device_data.uav_max_count);
      DrawCustomPixelShader(native_device_context, device_data.default_depth_stencil_state.get(), device_data.default_blend_state.get(), device_data.sampler_state_linear.get(), vertex_shader.get(), pixel_shader.get(), source, target, width, height);
      state.Restore(native_device_context);
      return true;
   }

   // Upscaling: a pass that isn't redirected but reads the render resolution scene (exposure histogram, the composite's draw into the
   // game's target; every post pass in the DEV A/B's previous way) gets the output scaled down into it first, again after redirected
   // passes wrote the output
   static void DownscalePostScene(ID3D11DeviceContext* native_device_context, DeviceData& device_data, reshade::api::shader_stage stages)
   {
      auto& game_device_data = GetGameDeviceData(device_data);
      if (game_device_data.post_scene_current || !game_device_data.post_scene_rtv)
         return;
      const bool compute = (stages & reshade::api::shader_stage::compute) == reshade::api::shader_stage::compute;
      com_ptr<ID3D11ShaderResourceView> srvs[post_srv_count];
      if (compute)
      {
         native_device_context->CSGetShaderResources(0, post_srv_count, &srvs[0]);
      }
      else
      {
         native_device_context->PSGetShaderResources(0, post_srv_count, &srvs[0]);
      }
      if (std::ranges::none_of(srvs, [&](const auto& srv)
             { return srv && GetViewResource(srv.get()) == game_device_data.post_scene; }))
         return;
      uint4 size;
      DXGI_FORMAT format;
      GetResourceInfo(game_device_data.post_scene.get(), size, format);
      game_device_data.post_scene_current = DrawScaled(native_device_context, device_data, game_device_data.sr_upscaled_output_srv.get(), game_device_data.post_scene_rtv.get(), size.x, size.y);
      // The runtime unbound the scene's views while it was a render target (the graphics ones are restored)
      if (compute)
      {
         native_device_context->CSSetShaderResources(0, post_srv_count, reinterpret_cast<ID3D11ShaderResourceView* const*>(&srvs[0]));
      }
   }

   // The frame's first bloom upsample (its t0 the game's deepest mip): Kino's iteration count is floor(logh), and its tent scale
   // 0.5 + frac(logh), logh = log2(mip 0 height) + radius - 8, so the output resolution chain has more iterations (one per doubling).
   // Recovers logh from the game's count and scale, shifted by log2 of the mip 0 height ratio, downsamples past the game's last mip and
   // upsamples back into it. The frame's upsamples take the output's tent scale ("bloom_sample_scale").
   static void ExtendBloom(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, CommandListData& cmd_list_data, DeviceData& device_data, ID3D11ShaderResourceView* game_source)
   {
      auto& game_device_data = GetGameDeviceData(device_data);
      game_device_data.bloom_sample_scale = 0.f; // Tried: the game's scale, unless it succeeds
      com_ptr<ID3D11Buffer> loop;
      native_device_context->PSGetConstantBuffers(bloom_loop_cb_slot, 1, &loop);
      // Empty on the buffer's first frame (see "OnUnmapBufferRegion")
      const Persona5StrikersGameDeviceData::GlobalsCopy constants = (loop ? GetGlobalsCopy(&game_device_data, loop.get()) : nullptr);
      const com_ptr<ID3D11Resource> resource = GetViewResource(game_source);
      auto* const target = (resource ? GetPostTarget(native_device, &game_device_data, resource.get(), false) : nullptr);
      if (!constants || constants->size() < 20 || !target)
         return;
      D3D11_SHADER_RESOURCE_VIEW_DESC source_desc;
      game_source->GetDesc(&source_desc);
      const UINT last_mip = source_desc.Texture2D.MostDetailedMip;
      float game_sample_scale;
      std::memcpy(&game_sample_scale, constants->data() + 16, sizeof(game_sample_scale));
      uint4 original_size;
      DXGI_FORMAT format;
      GetResourceInfo(resource.get(), original_size, format);
      D3D11_TEXTURE2D_DESC desc;
      target->texture->GetDesc(&desc);
      const double logh = double(last_mip + 1) + (game_sample_scale - 0.5) + std::log2(double(desc.Height) / double(original_size.y));
      const double whole = std::floor(logh);
      game_device_data.bloom_sample_scale = float(0.5 + (logh - whole));
      const UINT iterations = (std::min)(UINT(std::clamp(int(whole), 1, 16)), desc.MipLevels);
#if DEVELOPMENT
      static UINT logged_iterations = 0;
      if (std::exchange(logged_iterations, iterations) != iterations)
      {
         reshade::log::message(reshade::log::level::info, std::format("[P5S Bloom] {} iterations ({}x{} mip 0) -> {} ({}x{}), tent scale {} -> {}", last_mip + 1, original_size.x, original_size.y, iterations, desc.Width, desc.Height, game_sample_scale, game_device_data.bloom_sample_scale).c_str());
      }
#endif
      com_ptr<ID3D11VertexShader> vertex_shader;
      com_ptr<ID3D11PixelShader> downsample, upsample;
      {
         const std::shared_lock lock_shader_objects(s_mutex_shader_objects);
         vertex_shader = FindShader(device_data.native_vertex_shaders, "Scale VS"_h);
         downsample = FindShader(device_data.native_pixel_shaders, "P5S Bloom Downsample PS"_h);
         upsample = FindShader(device_data.native_pixel_shaders, "P5S Bloom Upsample PS"_h);
      }
      if (iterations <= last_mip + 1 || !vertex_shader || !downsample || !upsample)
         return;
      target->mip_srvs.resize(desc.MipLevels);
      target->mip_rtvs.resize(desc.MipLevels);
      for (UINT mip = last_mip; mip < iterations; mip++)
      {
         if (!target->mip_srvs[mip])
         {
            D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = source_desc;
            srv_desc.Texture2D = {mip, 1};
            native_device->CreateShaderResourceView(target->texture.get(), &srv_desc, &target->mip_srvs[mip]);
            D3D11_RENDER_TARGET_VIEW_DESC rtv_desc = {srv_desc.Format, D3D11_RTV_DIMENSION_TEXTURE2D};
            rtv_desc.Texture2D.MipSlice = mip;
            native_device->CreateRenderTargetView(target->texture.get(), &rtv_desc, &target->mip_rtvs[mip]);
         }
         if (!target->mip_srvs[mip] || !target->mip_rtvs[mip])
            return;
      }
      // Downsamples replace, upsamples add (the game's upsample blend); the game's upsample sampler stays bound
      DrawStateStack<DrawStateStackType::FullGraphics> state;
      state.Cache(native_device_context, device_data.uav_max_count);
      com_ptr<ID3D11BlendState> additive_blend;
      native_device_context->OMGetBlendState(&additive_blend, nullptr, nullptr);
      SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, reshade::api::shader_stage::pixel, LumaConstantBufferType::LumaData, 0, 0, game_device_data.bloom_sample_scale);
      for (UINT mip = last_mip; mip + 1 < iterations; mip++)
         DrawCustomPixelShader(native_device_context, device_data.default_depth_stencil_state.get(), device_data.default_blend_state.get(), nullptr, vertex_shader.get(), downsample.get(), target->mip_srvs[mip].get(), target->mip_rtvs[mip + 1].get(), GetTextureMipSize(desc.Width, mip + 1), GetTextureMipSize(desc.Height, mip + 1));
      for (UINT mip = iterations - 1; mip > last_mip; mip--)
         DrawCustomPixelShader(native_device_context, device_data.default_depth_stencil_state.get(), additive_blend.get(), nullptr, vertex_shader.get(), upsample.get(), target->mip_srvs[mip].get(), target->mip_rtvs[mip - 1].get(), GetTextureMipSize(desc.Width, mip - 1), GetTextureMipSize(desc.Height, mip - 1));
      state.Restore(native_device_context);
   }

   // Upscaling, from the split to the composite: draws a post process pass into the output resolution copies of its targets, reading
   // the copies of its textures (the P5R way; the Luma replacements take texels from texture sizes). False if nothing is redirected.
   static bool RedirectPostDraw(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, CommandListData& cmd_list_data, DeviceData& device_data, const ShaderHashesList<OneShaderPerPipeline>& original_shader_hashes, bool* updated_cbuffers, const std::function<void()>& draw)
   {
      auto& game_device_data = GetGameDeviceData(device_data);
      // All render targets or none (they must match in size)
      com_ptr<ID3D11RenderTargetView> rtvs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
      native_device_context->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, &rtvs[0], nullptr);
      bool redirected = false, writes_scene = false;
      for (auto& rtv : rtvs)
      {
         if (!rtv)
            continue;
         rtv = RedirectView(native_device, &game_device_data, rtv.get());
         if (!rtv)
            return false;
         redirected = true;
         writes_scene |= rtv.get() == game_device_data.sr_upscaled_output_rtv.get();
      }
      com_ptr<ID3D11ShaderResourceView> srvs[post_srv_count];
      native_device_context->PSGetShaderResources(0, post_srv_count, &srvs[0]);
      const com_ptr<ID3D11ShaderResourceView> game_source = srvs[0]; // The bloom upsample's source mip, before its redirection
      for (auto& srv : srvs)
      {
         if (com_ptr<ID3D11ShaderResourceView> redirected_srv = (srv ? RedirectView(native_device, &game_device_data, srv.get()) : nullptr))
         {
            srv = std::move(redirected_srv);
            redirected = true;
         }
      }
      if (!redirected)
         return false;
      game_device_data.post_scene_current &= !writes_scene;

      if (game_device_data.bloom_sample_scale < 0.f && original_shader_hashes.Contains(bloom_upsample_shader_hashes))
      {
         ExtendBloom(native_device, native_device_context, cmd_list_data, device_data, game_source.get());
      }
      DrawStateStack<DrawStateStackType::FullGraphics> state;
      state.Cache(native_device_context, device_data.uav_max_count);
      SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, reshade::api::shader_stage::pixel, LumaConstantBufferType::LumaSettings);
      SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, reshade::api::shader_stage::pixel, LumaConstantBufferType::LumaData, 0, 0, (std::max)(game_device_data.bloom_sample_scale, 0.f));
      *updated_cbuffers = true;
      if (const auto first_rtv = std::ranges::find_if(rtvs, [](const auto& rtv)
             { return bool(rtv); });
         first_rtv != std::end(rtvs))
      {
         // Post passes bind no depth, and a render resolution one couldn't go with output resolution targets
         native_device_context->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, reinterpret_cast<ID3D11RenderTargetView* const*>(&rtvs[0]), nullptr);
         uint4 size;
         DXGI_FORMAT format;
         GetResourceInfo(first_rtv->get(), size, format);
         D3D11_RENDER_TARGET_VIEW_DESC rtv_desc;
         (*first_rtv)->GetDesc(&rtv_desc);
         const UINT mip = GetRTVMipLevel(rtv_desc);
         SetViewportFullscreen(native_device_context, {GetTextureMipSize(size.x, mip), GetTextureMipSize(size.y, mip)});
      }
      native_device_context->PSSetShaderResources(0, post_srv_count, reinterpret_cast<ID3D11ShaderResourceView* const*>(&srvs[0]));
      draw();
      state.Restore(native_device_context);
      return true;
   }

   DrawOrDispatchOverrideType OnDrawOrDispatch(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, CommandListData& cmd_list_data, DeviceData& device_data, reshade::api::shader_stage stages, const ShaderHashesList<OneShaderPerPipeline>& original_shader_hashes, bool is_custom_pass, bool& updated_cbuffers, std::function<void()>* original_draw_dispatch_func) override
   {
      auto& game_device_data = GetGameDeviceData(device_data);
      const bool can_draw = original_draw_dispatch_func && *original_draw_dispatch_func;
      const bool sr_mesh_draw = can_draw && IsSRActive(device_data) && (stages & reshade::api::shader_stage::vertex) == reshade::api::shader_stage::vertex;

      // Before the UI's swapchain checks, which from now on see the letterboxed target instead (this draw and the bars aren't UI)
      if (original_shader_hashes.Contains(letterbox_hash, reshade::api::shader_stage::pixel))
      {
         com_ptr<ID3D11RenderTargetView> rtv;
         native_device_context->OMGetRenderTargets(1, &rtv, nullptr);
         com_ptr<ID3D11ShaderResourceView> srv;
         native_device_context->PSGetShaderResources(0, 1, &srv);
         const com_ptr<ID3D11Resource> source = GetViewResource(srv.get());
         if (const com_ptr<ID3D11Resource> target = GetViewResource(rtv.get()); source && target && IsSwapchainBackBuffer(&device_data, target.get()))
         {
            uint4 source_size;
            DXGI_FORMAT source_format;
            GetResourceInfo(source.get(), source_size, source_format);
            auto& letterbox = game_device_data.letterbox;
            const std::unique_lock lock(letterbox.mutex);
            letterbox.target = source;
            letterbox.size = {source_size.x, source_size.y};
            letterbox.drawn = true;
         }
      }

      if (original_shader_hashes.Contains(ssao_depth_downsample_hash, reshade::api::shader_stage::pixel))
      {
         com_ptr<ID3D11ShaderResourceView> depth_srv;
         native_device_context->PSGetShaderResources(0, 1, &depth_srv);
         D3D11_SHADER_RESOURCE_VIEW_DESC depth_srv_desc;
         if (depth_srv)
         {
            depth_srv->GetDesc(&depth_srv_desc);
         }
         if (depth_srv && depth_srv_desc.Format == DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS && depth_srv_desc.ViewDimension == D3D11_SRV_DIMENSION_TEXTURE2D)
         {
            const std::lock_guard lock(game_device_data.smaa_depth_mutex);
            game_device_data.smaa_depth_srv = depth_srv;
         }
      }

      if (sr_mesh_draw)
      {
#if DEVELOPMENT
         // Includes recording the draw itself (patched or not)
         const Perf::HookTimer timer{&game_device_data.perf_window.hook_ns};
#endif
         if (original_shader_hashes.Contains(post_process_start_shader_hashes))
         {
            if (native_device_context == game_device_data.mv_scene_context && !game_device_data.mv_frame_ended)
            {
               EndMotionVectorFrame(native_device, native_device_context, device_data);
            }
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
            const bool scene_draw = (game_device_data.mv_frame_ended ? !rtvs[0] : native_device_context == game_device_data.mv_scene_context);
            if (!gbuffer && dsv && scene_draw && GetViewResource(dsv.get()).get() == game_device_data.mv_scene_depth_id)
            {
               com_ptr<ID3D11DepthStencilState> depth_stencil_state;
               UINT stencil_ref;
               native_device_context->OMGetDepthStencilState(&depth_stencil_state, &stencil_ref);
               if (depth_stencil_state)
               {
                  depth_stencil_state->GetDesc(&depth_desc);
               }
               UINT stride, offset;
               native_device_context->IAGetVertexBuffers(0, 1, &vertex_buffer, &stride, &offset);
            }
            const bool depth_write = depth_desc.DepthEnable && depth_desc.DepthWriteMask == D3D11_DEPTH_WRITE_MASK_ALL;
            const bool forward = !gbuffer && rtvs[0] && !rtvs[1] && depth_write && vertex_buffer;
            if (gbuffer || forward)
            {
               ID3D11RenderTargetView* const bound_rtvs[8] = {rtvs[0].get(), rtvs[1].get(), rtvs[2].get(), rtvs[3].get(), rtvs[4].get(), rtvs[5].get(), rtvs[6].get(), rtvs[7].get()};
               if (DrawWithMotionVectors(native_device, native_device_context, cmd_list_data, device_data, original_shader_hashes, bound_rtvs, dsv.get(), gbuffer, *original_draw_dispatch_func))
               {
#if DEVELOPMENT
                  g_jitter_draws += !gbuffer;
#endif
                  return DrawOrDispatchOverrideType::Replaced;
               }
            }
            // Any other mesh depth tested against the scene: the depth prepass (no targets), or depth test only geometry
            const bool depth_prepass = !rtvs[0] && depth_write;
            if (!gbuffer && depth_desc.DepthEnable && vertex_buffer && DrawWithJitter(native_device, native_device_context, cmd_list_data, device_data, original_shader_hashes, dsv.get(), depth_prepass, *original_draw_dispatch_func))
            {
#if DEVELOPMENT
               g_jitter_draws++;
#endif
               return DrawOrDispatchOverrideType::Replaced;
            }
#if DEVELOPMENT
            // Only scene depth draws fill "depth_desc"
            if (!gbuffer && depth_desc.DepthEnable)
            {
               auto& skipped = (vertex_buffer ? g_jitter_skipped_unpatched : g_jitter_skipped_no_vb);
               skipped++;
               const uint64_t key = (uint64_t(original_shader_hashes.pixel_shaders.empty() ? 0 : original_shader_hashes.pixel_shaders[0]) << 1) | (vertex_buffer ? 1 : 0);
               const std::unique_lock lock(g_jitter_skipped_mutex);
               if (g_jitter_skipped_logged.insert(key).second)
               {
                  reshade::log::message(reshade::log::level::info, std::format("[P5S Jitter] not jittered: PS {:#010x} VS {:#010x} ({}, depth {})", original_shader_hashes.pixel_shaders.empty() ? 0u : original_shader_hashes.pixel_shaders[0], original_shader_hashes.vertex_shaders.empty() ? 0u : original_shader_hashes.vertex_shaders[0], vertex_buffer ? "unpatched" : "no vertex buffer", depth_write ? "written" : "tested").c_str());
               }
            }
#endif
         }
      }

      if (original_shader_hashes.Contains(layer_stretch_hash, reshade::api::shader_stage::pixel))
      {
         game_device_data.layer_drawn = true;
         game_device_data.layer_targets_created = true;
      }
      if (sr_mesh_draw && DrawLayerAtOutputResolution(native_device, native_device_context, cmd_list_data, device_data, original_shader_hashes, *original_draw_dispatch_func))
         return DrawOrDispatchOverrideType::Replaced;

      // The exposure histogram reads a layer stretch target's render resolution corner by pixel: give it the target downscaled to render
      // resolution
      if (original_shader_hashes.Contains(exposure_histogram_hash, reshade::api::shader_stage::compute) && can_draw)
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
               {
                  GetResourceInfo(game_device_data.layer_histogram_srv.get(), histogram_size, histogram_format);
               }
               if (histogram_size.x != width || histogram_size.y != height)
               {
                  game_device_data.layer_histogram_rtv.reset();
                  game_device_data.layer_histogram_srv.reset();
                  const D3D11_TEXTURE2D_DESC desc = {.Width = width, .Height = height, .MipLevels = 1, .ArraySize = 1, .Format = DXGI_FORMAT_R16G16B16A16_FLOAT, .SampleDesc = {.Count = 1, .Quality = 0}, .Usage = D3D11_USAGE_DEFAULT, .BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET};
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
         if (histogram_srv && DrawScaled(native_device_context, device_data, layer_srv.get(), histogram_rtv.get(), width, height))
         {
            ID3D11ShaderResourceView* srv = histogram_srv.get();
            native_device_context->CSSetShaderResources(0, 1, &srv);
            (*original_draw_dispatch_func)();
            srv = layer_srv.get();
            native_device_context->CSSetShaderResources(0, 1, &srv);
            return DrawOrDispatchOverrideType::Replaced;
         }
      }

      if (original_shader_hashes.Contains(ssao_hash, reshade::api::shader_stage::pixel))
      {
         if (g_gtao_enable)
            return RunXeGTAO(native_device, native_device_context, device_data) ? DrawOrDispatchOverrideType::Replaced : DrawOrDispatchOverrideType::None;
         const std::lock_guard lock(game_device_data.gtao_mutex);
         if (game_device_data.gtao_final_texture)
         {
            game_device_data.ReleaseGTAOScratch();
         }
      }

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
         {
            split.source_color->GetDesc(&scene_desc);
         }
         {
            const std::shared_lock lock(game_device_data.mv_mutex);
            split.motion_vectors = game_device_data.mv_texture;
         }
         uint4 mv_size = {};
         DXGI_FORMAT mv_format;
         if (split.motion_vectors)
         {
            GetResourceInfo(split.motion_vectors.get(), mv_size, mv_format);
         }
         split.depth = game_device_data.mv_frame_depth;
         split.jitter = game_device_data.mv_jitter;
         // "mW2P" is row major and multiplies row vectors: its column 1 (xyz) is the view's up axis times 1 / tan(fov / 2)
         if (game_device_data.mv_view_projection_valid)
         {
            const auto& view_projection = game_device_data.mv_view_projection;
            split.vertical_fov = 2.f * std::atan(1.f / std::sqrt(view_projection[1] * view_projection[1] + view_projection[5] * view_projection[5] + view_projection[9] * view_projection[9]));
         }
         // Upscaling when the game renders below output resolution (its render scale option): output and canvas at output resolution
         const uint2 output_size = GetOutputSize(device_data);
         if (scene_desc.Width < output_size.x || scene_desc.Height < output_size.y)
         {
            D3D11_TEXTURE2D_DESC output_desc = {};
            if (game_device_data.sr_upscaled_output)
            {
               game_device_data.sr_upscaled_output->GetDesc(&output_desc);
            }
            if (output_desc.Width != output_size.x || output_desc.Height != output_size.y)
            {
               game_device_data.sr_upscaled_output.reset();
               game_device_data.sr_upscaled_output_rtv.reset();
               game_device_data.sr_upscaled_output_srv.reset();
               game_device_data.sr_upscaled_canvas_rtv.reset();
               game_device_data.sr_upscaled_canvas_srv.reset();
               output_desc = {.Width = output_size.x, .Height = output_size.y, .MipLevels = 1, .ArraySize = 1, .Format = DXGI_FORMAT_R16G16B16A16_FLOAT, .SampleDesc = {.Count = 1, .Quality = 0}, .Usage = D3D11_USAGE_DEFAULT, .BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET | D3D11_BIND_UNORDERED_ACCESS};
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
            {
               split.output_color = game_device_data.sr_upscaled_output;
            }
         }
         // Back at the output resolution: the output, canvas and output sized post targets go. A pending split holds its own output, and
         // their other users (post, composite, stretch) record on this context, as the scene of every frame, so they already ran.
         else if (game_device_data.sr_upscaled_output)
         {
            game_device_data.post_targets.clear();
            game_device_data.post_views.clear();
            game_device_data.post_sizes = {};
            game_device_data.sr_upscaled_output.reset();
            game_device_data.sr_upscaled_output_rtv.reset();
            game_device_data.sr_upscaled_output_srv.reset();
            game_device_data.sr_upscaled_canvas_rtv.reset();
            game_device_data.sr_upscaled_canvas_srv.reset();
         }
         // The upgraded scene: motion vector sized, not multisampled
         if (split.depth && native_device_context->GetType() == D3D11_DEVICE_CONTEXT_DEFERRED && scene_desc.Width == mv_size.x && scene_desc.Height == mv_size.y && scene_desc.SampleDesc.Count == 1 && (scene_desc.Format == DXGI_FORMAT_R16G16B16A16_FLOAT || scene_desc.Format == DXGI_FORMAT_R16G16B16A16_TYPELESS) && SUCCEEDED(native_device_context->FinishCommandList(TRUE, &split.partial)))
         {
            const std::lock_guard lock(game_device_data.sr_mutex);
            // The game doesn't know its command list restarted: its next "D3D11_MAP_WRITE_NO_OVERWRITE" map fails without a discard first
            // (dialogue text went missing). Data appended before the split is lost.
            for (const auto& buffer : game_device_data.sr_no_overwrite_buffers)
            {
               D3D11_MAPPED_SUBRESOURCE mapped;
               if (SUCCEEDED(native_device_context->Map(buffer.get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
               {
                  native_device_context->Unmap(buffer.get(), 0);
               }
            }
            game_device_data.sr_upscaling_context = (split.output_color ? native_device_context : nullptr);
            // Upscaling: the post process up to the composite runs at the output resolution (see "RedirectPostDraw")
            game_device_data.post_redirect = split.output_color && g_post_output_resolution;
            game_device_data.post_scene_current = false;
            game_device_data.bloom_sample_scale = -1.f;
            if (split.output_color)
            {
               if (const std::array<uint2, 2> sizes = {uint2{scene_desc.Width, scene_desc.Height}, output_size}; game_device_data.post_sizes != sizes)
               {
                  game_device_data.post_targets.clear();
                  game_device_data.post_views.clear();
                  game_device_data.post_sizes = sizes;
               }
               if (game_device_data.post_scene.get() != split.source_color.get())
               {
                  game_device_data.post_scene = split.source_color.get();
                  game_device_data.post_scene_rtv.reset();
                  const D3D11_RENDER_TARGET_VIEW_DESC scene_rtv_desc = {.Format = DXGI_FORMAT_R16G16B16A16_FLOAT, .ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D};
                  native_device->CreateRenderTargetView(split.source_color.get(), &scene_rtv_desc, &game_device_data.post_scene_rtv);
               }
            }
            game_device_data.sr_heights.store({.render = scene_desc.Height, .output = (split.output_color ? output_size.y : scene_desc.Height)});
            device_data.render_resolution = {float(scene_desc.Width), float(scene_desc.Height)}; // Shown by Core
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
         // Not split: the scene runs without the upscaler (and without the jitter, see "sr_drew"). Each reason is logged once, as it's
         // the only trace of a setup (resolution, display mode) the upscaling doesn't expect.
         else
         {
            const bool fp16_scene = scene_desc.Format == DXGI_FORMAT_R16G16B16A16_FLOAT || scene_desc.Format == DXGI_FORMAT_R16G16B16A16_TYPELESS;
            const uint32_t reasons = (split.depth ? 0u : 1u) | ((native_device_context->GetType() == D3D11_DEVICE_CONTEXT_DEFERRED) ? 0u : 2u) | ((scene_desc.Width == mv_size.x && scene_desc.Height == mv_size.y) ? 0u : 4u) | ((scene_desc.SampleDesc.Count == 1) ? 0u : 8u) | (fp16_scene ? 0u : 16u);
            const uint32_t refusal = (reasons != 0 ? reasons : 32u); // 32: "FinishCommandList" failed
            if ((game_device_data.sr_split_refusals_logged.fetch_or(refusal) & refusal) != refusal)
            {
               reshade::log::message(reshade::log::level::warning, std::format("[P5S SR] a scene frame runs without DLSS/FSR (reasons {:#x}: 1 no depth, 2 immediate context, 4 size, 8 multisampled, 16 not upgraded to fp16, 32 command list): scene {}x{} format {} samples {}, motion vectors {}x{}, output {}x{}", refusal, scene_desc.Width, scene_desc.Height, int(scene_desc.Format), scene_desc.SampleDesc.Count, mv_size.x, mv_size.y, output_size.x, output_size.y).c_str());
            }
         }
      }

      // Upscaled: the post process up to the composite (which ends it) draws at the output resolution, other readers of the render
      // resolution scene get it downscaled (see "DownscalePostScene")
      if (native_device_context == game_device_data.sr_upscaling_context && can_draw)
      {
         game_device_data.post_redirect &= !original_shader_hashes.Contains(composite_hash, reshade::api::shader_stage::pixel);
         if (game_device_data.post_redirect && (stages & reshade::api::shader_stage::pixel) == reshade::api::shader_stage::pixel && RedirectPostDraw(native_device, native_device_context, cmd_list_data, device_data, original_shader_hashes, &updated_cbuffers, *original_draw_dispatch_func))
            return DrawOrDispatchOverrideType::Replaced;
         DownscalePostScene(native_device_context, device_data, stages);
#if DEVELOPMENT
         // The previous way (A/B): see "scene_post_writer_shader_hashes"
         if (!game_device_data.post_redirect && original_shader_hashes.Contains(scene_post_writer_shader_hashes))
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
            ID3D11RenderTargetView* const rtv = game_device_data.sr_upscaled_output_rtv.get();
            native_device_context->OMSetRenderTargets(1, &rtv, nullptr);
            SetFullTargetViewport(native_device_context, desc.Width, desc.Height);
            (*original_draw_dispatch_func)();
            state.Restore(native_device_context);
            return DrawOrDispatchOverrideType::Replaced;
         }
#endif
      }

      if (original_shader_hashes.Contains(composite_hash, reshade::api::shader_stage::pixel))
      {
         device_data.has_drawn_main_post_processing = true;
         // Here rather than at present, as the post process (the scratch's only user) records on a deferred context
         {
            const std::unique_lock lock_smaa(game_device_data.smaa_mutex);
            if (cb_luma_global_settings.FrameIndex - game_device_data.smaa_frame > smaa_idle_release_frames)
            {
               game_device_data.smaa_predication_srv.reset();
               game_device_data.smaa_predication_uav.reset();
               ReleaseSMAA(device_data);
            }
            if (cb_luma_global_settings.FrameIndex - game_device_data.smaa_copies_frame > smaa_idle_release_frames)
            {
               game_device_data.ReleaseSMAACopies();
            }
         }
         if (!can_draw)
            return DrawOrDispatchOverrideType::None;
         // Below render scale 1 the composite draws into its own target, which the game stretches onto the swapchain
         {
            com_ptr<ID3D11RenderTargetView> rtv;
            native_device_context->OMGetRenderTargets(1, &rtv, nullptr);
            const com_ptr<ID3D11Resource> target = GetViewResource(rtv.get());
            const bool own_target = target && !IsGameBackBuffer(&device_data, target.get());
            {
               const std::unique_lock lock(game_device_data.composite_target_mutex);
               game_device_data.composite_target.reset();
               if (own_target)
               {
                  game_device_data.composite_target = target;
               }
            }
            if (own_target)
            {
               uint4 target_size;
               DXGI_FORMAT target_format;
               GetResourceInfo(target.get(), target_size, target_format);
               if (target_size.x < GetOutputSize(device_data).x)
               {
                  game_device_data.render_resolution_composite = true;
               }

               // A 3D layer's composite, from its stretch target: see "layer_composed"
               com_ptr<ID3D11ShaderResourceView> scene_srv;
               native_device_context->PSGetShaderResources(0, 1, &scene_srv);
               float layer_scale = 0.f;
               if (const com_ptr<ID3D11Resource> scene = GetViewResource(scene_srv.get()))
               {
                  const std::lock_guard lock(game_device_data.layer_mutex);
                  layer_scale = GetRecentLayerScale(game_device_data.layer_outputs, scene.get());
                  if (layer_scale > 0.f)
                  {
                     game_device_data.layer_composed[target.get()] = {layer_scale, cb_luma_global_settings.FrameIndex};
                  }
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
                  SetFullTargetViewport(native_device_context, target_size.x, target_size.y, viewport.MinDepth, viewport.MaxDepth);
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
            // First into the game's own target, which the pause screen freezes as its background (else a stale frame); the cbuffers stay
            // bound for the canvas draw
            SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, reshade::api::shader_stage::pixel, LumaConstantBufferType::LumaSettings);
            SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, reshade::api::shader_stage::pixel, LumaConstantBufferType::LumaData);
            updated_cbuffers = true;
#if DEVELOPMENT
            Persona5StrikersGameDeviceData::PerfDrawQueries* perf_queries = nullptr;
            if (Perf::g_test != 0)
            {
               const std::lock_guard lock(game_device_data.perf_mutex);
               auto& queries = game_device_data.perf_composite_queries[game_device_data.perf_composite_query_index];
               if (!queries.pending)
               {
                  const D3D11_QUERY_DESC disjoint_desc = {.Query = D3D11_QUERY_TIMESTAMP_DISJOINT}, timestamp_desc = {.Query = D3D11_QUERY_TIMESTAMP};
                  if (!queries.disjoint)
                  {
                     native_device->CreateQuery(&disjoint_desc, &queries.disjoint);
                     native_device->CreateQuery(&timestamp_desc, &queries.start);
                     native_device->CreateQuery(&timestamp_desc, &queries.end);
                  }
                  if (queries.disjoint && queries.start && queries.end)
                  {
                     perf_queries = &queries;
                     queries.pending = true;
                     game_device_data.perf_composite_query_index = (game_device_data.perf_composite_query_index + 1) % game_device_data.perf_composite_queries.size();
                  }
               }
            }
            if (perf_queries)
            {
               native_device_context->Begin(perf_queries->disjoint.get());
               native_device_context->End(perf_queries->start.get());
            }
#endif
            (*original_draw_dispatch_func)();
#if DEVELOPMENT
            if (perf_queries)
            {
               native_device_context->End(perf_queries->end.get());
               native_device_context->End(perf_queries->disjoint.get());
            }
#endif
            DrawStateStack<DrawStateStackType::FullGraphics> state;
            state.Cache(native_device_context, device_data.uav_max_count);
            D3D11_TEXTURE2D_DESC desc;
            game_device_data.sr_upscaled_output->GetDesc(&desc); // The canvas' size
            ID3D11ShaderResourceView* const scene_srv = game_device_data.sr_upscaled_output_srv.get();
            ID3D11RenderTargetView* const canvas_rtv = game_device_data.sr_upscaled_canvas_rtv.get();
            native_device_context->PSSetShaderResources(0, 1, &scene_srv);
            native_device_context->OMSetRenderTargets(1, &canvas_rtv, nullptr);
            SetFullTargetViewport(native_device_context, desc.Width, desc.Height);
            if (cb_luma_global_settings.GameSettings.RCASSharpness <= 0.f || DrawCompositeWithSMAAAndRCAS(native_device, native_device_context, cmd_list_data, device_data, &updated_cbuffers, *original_draw_dispatch_func, false) == DrawOrDispatchOverrideType::None)
            {
               (*original_draw_dispatch_func)();
            }
            state.Restore(native_device_context);
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
      if (original_shader_hashes.Contains(copy_hash, reshade::api::shader_stage::pixel) && original_shader_hashes.Contains(quad_vertex_shader_hash, reshade::api::shader_stage::vertex) && can_draw)
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
         const com_ptr<ID3D11Buffer> uv_scale_buffer = (quad_vertex_shader ? GetLayerUVScaleBuffer(native_device, &game_device_data, scale) : nullptr);
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
            const D3D11_VIEWPORT whole_viewport = {.TopLeftX = viewport.TopLeftX / scale[0], .TopLeftY = viewport.TopLeftY / scale[1], .Width = viewport.Width / scale[0], .Height = viewport.Height / scale[1], .MinDepth = viewport.MinDepth, .MaxDepth = viewport.MaxDepth};
            const D3D11_RECT whole_scissor = {.left = 0, .top = 0, .right = LONG(target_size.x), .bottom = LONG(target_size.y)};
            native_device_context->RSSetViewports(1, &whole_viewport);
            native_device_context->RSSetScissorRects(1, &whole_scissor);
            DrawWithVertexShader(native_device_context, quad_vertex_shader.get(), layer_uv_scale_cb_slot, uv_scale_buffer.get(), *original_draw_dispatch_func);
            native_device_context->RSSetViewports(1, &viewport);
            native_device_context->RSSetScissorRects(scissors, &scissor);
            return DrawOrDispatchOverrideType::Replaced;
         }
      }

      // The game's stretch of the composite target onto the swapchain (render scales below 1) is scene, not UI. Upscaled, it copies the
      // canvas 1:1.
      if (original_shader_hashes.Contains(copy_hash, reshade::api::shader_stage::pixel) && can_draw)
      {
         com_ptr<ID3D11Resource> composite_target;
         {
            const std::shared_lock lock(game_device_data.composite_target_mutex);
            composite_target = game_device_data.composite_target;
         }
         com_ptr<ID3D11ShaderResourceView> game_srv;
         native_device_context->PSGetShaderResources(0, 1, &game_srv);
         if (const com_ptr<ID3D11Resource> source = GetViewResource(game_srv.get()); source && source == composite_target)
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

      // Everything drawn onto the swapchain after the composite is UI (HUD, menus, dialogue boxes, fades), except FXAA; so is all of it in
      // frames without one (pause screen, world transitions), whose blends need the same clamps. Hide UI keeps those frames.
      if ((stages & reshade::api::shader_stage::pixel) == reshade::api::shader_stage::pixel && !original_shader_hashes.Contains(fxaa_hash, reshade::api::shader_stage::pixel) && !original_shader_hashes.Contains(shader_hashes_apply_fxaa))
      {
         com_ptr<ID3D11RenderTargetView> rtv;
         native_device_context->OMGetRenderTargets(1, &rtv, nullptr);
         if (const com_ptr<ID3D11Resource> rtv_resource = GetViewResource(rtv.get()); rtv_resource && IsGameBackBuffer(&device_data, rtv_resource.get()))
         {
            if (g_hide_ui && device_data.has_drawn_main_post_processing)
               return DrawOrDispatchOverrideType::Skip;

            // The sprite putting a 3D layer's composite on the swapchain scales up its render resolution corner: draw all of it instead
            if (original_shader_hashes.Contains(ui_sprite_vertex_shader_hash, reshade::api::shader_stage::vertex) && can_draw)
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
               const com_ptr<ID3D11Buffer> uv_scale_buffer = (sprite_vertex_shader ? GetLayerUVScaleBuffer(native_device, &game_device_data, scale) : nullptr);
               if (uv_scale_buffer)
               {
                  DrawWithVertexShader(native_device_context, sprite_vertex_shader.get(), layer_uv_scale_cb_slot, uv_scale_buffer.get(), *original_draw_dispatch_func);
                  return DrawOrDispatchOverrideType::Replaced;
               }
            }

            // UI blends reading the swapchain saw it clamped to 0-1 by the vanilla UNORM target; in fp16, earlier additive UI (e.g. the
            // menu cursor's RGB cards, alpha 1 + 1 + 1) exceeds 1: the cursor's reverse subtracted text vanished and destination alpha
            // masks (HUD, main menu) read alphas up to 2. So the same draw (own geometry and stencil) first clamps with a MIN blend: all
            // channels before a color subtract, only where the sprite subtracts something (a HUD subtract's quad would clip the HDR scene
            // under it; this reads the UI pixel shaders' b0 "nStageNum", t0/s0 and TEXCOORD1 and assumes a source alpha factor), else only
            // the never displayed alpha (white). After it, subtracts are floored at 0 (dialogue bubbles reached -1.5) with a black pixel
            // shader and a MAX blend, and additive UI is clamped to the display's peak (vanilla clipped at 1) with a MIN blend.
            if (can_draw)
            {
               com_ptr<ID3D11BlendState> blend_state;
               FLOAT blend_factor[4];
               UINT sample_mask;
               native_device_context->OMGetBlendState(&blend_state, blend_factor, &sample_mask);
               D3D11_BLEND_DESC blend_desc = {};
               if (blend_state)
               {
                  blend_state->GetDesc(&blend_desc);
               }
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
                  com_ptr<ID3D11PixelShader> coverage_pixel_shader;
                  com_ptr<ID3D11PixelShader> black_pixel_shader;
                  com_ptr<ID3D11PixelShader> peak_pixel_shader;
                  {
                     const std::shared_lock lock_shader_objects(s_mutex_shader_objects);
                     white_pixel_shader = FindShader(device_data.native_pixel_shaders, "P5S Draw White PS"_h);
                     coverage_pixel_shader = FindShader(device_data.native_pixel_shaders, "P5S UI Coverage Clamp PS"_h);
                     black_pixel_shader = FindShader(device_data.native_pixel_shaders, "P5S Draw Black PS"_h);
                     peak_pixel_shader = FindShader(device_data.native_pixel_shaders, "P5S UI Peak Clamp PS"_h);
                  }
                  // Index 0 all channels, 1 alpha only
                  const int channels = (subtracts_colors ? 0 : 1);
                  if (white_pixel_shader && coverage_pixel_shader && black_pixel_shader && peak_pixel_shader && game_device_data.ui_min_blend_states[0] && game_device_data.ui_min_blend_states[1] && game_device_data.ui_max_blend_states[channels])
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
                     {
                        draw(subtracts_colors ? coverage_pixel_shader.get() : white_pixel_shader.get(), game_device_data.ui_min_blend_states[channels].get());
                     }
                     draw(pixel_shader.get(), blend_state.get());
                     if (subtracts_colors || subtracts_alpha)
                     {
                        draw(black_pixel_shader.get(), game_device_data.ui_max_blend_states[channels].get());
                     }
                     if (adds_colors)
                     {
                        SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, reshade::api::shader_stage::pixel, LumaConstantBufferType::LumaSettings);
                        updated_cbuffers = true;
                        draw(peak_pixel_shader.get(), game_device_data.ui_min_blend_states[0].get());
                     }
                     native_device_context->PSSetShader(pixel_shader.get(), nullptr, 0);
                     native_device_context->OMSetBlendState(blend_state.get(), blend_factor, sample_mask);
                     return DrawOrDispatchOverrideType::Replaced;
                  }
               }
            }
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

   // Keeps the game's render scale at the slider's value, live: after writing the setting, a WM_SIZE for the current size makes the game
   // rebuild its targets now (it does on any resize, e.g. alt-tab). Changing the option in the game's menu (the same setting) is undone,
   // also live. Without DLSS/FSR the game stretches its composite (SMAA runs before). With them the main menu ("menu") renders at 100%
   // (no DLSS/FSR there, so the background would be stretched), kept in the setting for the whole menu stay: P5StrikersFix rebuilds at
   // any value written back, so the options menu shows (and saves) 100% there. The same WM_SIZE also rebuilds on "targets_rebuild".
   static void UpdateRenderScale(Persona5StrikersGameDeviceData* game_device_data, bool menu)
   {
      if (!game_device_data->render_scale_searched)
      {
         game_device_data->render_scale_searched = true;
         game_device_data->render_scale_setting = FindRenderScaleSetting();
      }
      bool rebuild = std::exchange(game_device_data->targets_rebuild, false);
      if (int32_t* const setting = game_device_data->render_scale_setting)
      {
#if DEVELOPMENT
         const int32_t kept = (Perf::g_test != 0 ? perf_test_render_scales[Perf::g_test] : g_render_scale);
#else
         const int32_t kept = g_render_scale;
#endif
         const int32_t wanted = (menu ? 10 : kept);
         // The game rebuilds its targets at whatever the setting holds, also when its menu wrote it: the wanted value is forced back
         if (*setting != wanted)
         {
            *setting = wanted;
            rebuild = true;
         }
      }
      RECT client;
      if (rebuild && game_window && GetClientRect(game_window, &client))
      {
         PostMessageW(game_window, WM_SIZE, SIZE_RESTORED, MAKELPARAM(client.right - client.left, client.bottom - client.top));
      }
   }

   void OnPresent(ID3D11Device* native_device, DeviceData& device_data) override
   {
      // The upscaler's history restarts after any frame it didn't draw (menus, loading, just picked)
      auto& game_device_data = GetGameDeviceData(device_data);
      game_device_data.sr_drew = device_data.has_drawn_sr.load();
      game_device_data.sr_active = LatchSRFrame(device_data);
      game_device_data.mv_presents++;
      // A letterboxed target's aspect ratio (e.g. 2.4 at 3840x1600) isn't the swapchain's: the targets made after a rebuild are upgraded
      // at it too (the scene, for the upscaling, and the target itself, for HDR)
      {
         uint2 letterbox_size = {};
         {
            auto& letterbox = game_device_data.letterbox;
            const std::unique_lock lock(letterbox.mutex);
            if (!std::exchange(letterbox.drawn, false))
            {
               letterbox.target.reset();
            }
            if (letterbox.target)
            {
               letterbox_size = letterbox.size;
            }
         }
         if (letterbox_size.x != 0 && letterbox_size.y != 0)
         {
            const float aspect_ratio = float(letterbox_size.x) / float(letterbox_size.y);
            const std::unique_lock lock(device_data.resource_upgrades.mutex);
            if (!device_data.resource_upgrades.texture_format_upgrades_2d_custom_aspect_ratios.contains(aspect_ratio))
            {
               device_data.resource_upgrades.texture_format_upgrades_2d_custom_aspect_ratios = {aspect_ratio};
               game_device_data.targets_rebuild = true;
            }
         }
      }
      {
         const std::lock_guard lock(game_device_data.layer_mutex);
         const auto stale = [](const auto& entry)
         {
            const auto& [resource, output] = entry;
            return cb_luma_global_settings.FrameIndex - output.frame_index > 1;
         };
         std::erase_if(game_device_data.layer_outputs, stale);
         std::erase_if(game_device_data.layer_composed, stale);
      }
      {
         // The main menu after 30 presents (loading screens and fades between keep the game's scale), until the scene draws again. Scene
         // frames are only seen with DLSS/FSR (motion vectors start them), the only time 100% matters.
         const bool render_resolution_composite = game_device_data.render_resolution_composite.exchange(false);
         if (game_device_data.scene_drawn.exchange(false) || !IsSRActive(device_data))
         {
            game_device_data.menu_presents = 0;
         }
         else if (render_resolution_composite && game_device_data.menu_presents < 30)
         {
            game_device_data.menu_presents++;
         }
         if (!game_device_data.layer_targets_created)
         {
            game_device_data.menu_presents = 30;
         }
         const bool menu = game_device_data.menu_presents >= 30;
         UpdateRenderScale(&game_device_data, menu);
      }
      // A mip sharper under DLSS/FSR, which resolves the detail over its jittered frames (Core applies it to anisotropic samplers)
      if (!custom_texture_mip_lod_bias_offset)
      {
         const std::unique_lock lock(s_mutex_samplers);
         const auto heights = game_device_data.sr_heights.load();
         device_data.texture_mip_lod_bias_offset = (IsSRActive(device_data) ? SR::GetMipLODBias(heights.render, heights.output) : 0.f);
      }
#if DEVELOPMENT
      // "Performance Test": the finished timestamp sets, averaged into a log line every 120 frames (the first 60 after a mode change or
      // a pause skipped)
      if (Perf::g_test != 0)
      {
         com_ptr<ID3D11DeviceContext> immediate_context;
         native_device->GetImmediateContext(&immediate_context);
         const std::lock_guard lock(game_device_data.perf_mutex);
         auto& window = game_device_data.perf_window;
         const bool measuring = window.Present(std::to_string(Perf::g_test), 60);
         auto& stats = window.stats;
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
               window.disjoint++;
               continue;
            }
            stats.scene.Add(1000.0 * double(sr_start - scene_start) / double(disjoint.Frequency));
            stats.sr.Add(1000.0 * double(sr_end - sr_start) / double(disjoint.Frequency));
         }
         for (auto& queries : game_device_data.perf_composite_queries)
         {
            D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjoint;
            UINT64 start, end;
            if (!queries.pending || immediate_context->GetData(queries.disjoint.get(), &disjoint, sizeof(disjoint), D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK)
               continue;
            queries.pending = false;
            if (!measuring || disjoint.Disjoint || disjoint.Frequency == 0 || immediate_context->GetData(queries.start.get(), &start, sizeof(start), D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK || immediate_context->GetData(queries.end.get(), &end, sizeof(end), D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK)
               continue;
            stats.composite.Add(1000.0 * double(end - start) / double(disjoint.Frequency));
         }
         window.Finish(measuring, [&]
            { reshade::log::message(reshade::log::level::info, std::format("[P5S Perf] mode=\"{}\" sr={} render={}p output={}p gpu scene avg/max={:.3f}/{:.3f} ms gpu sr avg/max={:.3f}/{:.3f} ms gpu game composite avg/max={:.3f}/{:.3f} ms ({} samples) cpu hooks={:.3f} ms/frame globals filter={} samples={}/{} disjoint={}", perf_test_modes[Perf::g_test], device_data.sr_type == SR::Type::FSR ? "FSR" : "DLSS", game_device_data.sr_heights.load().render, game_device_data.sr_heights.load().output, stats.scene.Average(), stats.scene.max_ms, stats.sr.Average(), stats.sr.max_ms, stats.composite.Average(), stats.composite.max_ms, stats.composite.samples, window.HookMs(), game_device_data.mv_filtered_globals_buffer_count.load(), stats.scene.samples, window.frames, window.disjoint).c_str()); });
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
      // 0 was "the game's option" before it was always forced
      g_render_scale = (g_render_scale >= 5 && g_render_scale <= 10 ? g_render_scale : 10);
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
            *value = (enabled ? 1.f : 0.f);
            reshade::set_config_value(nullptr, NAME, key, *value);
            device_data.cb_luma_global_settings_dirty = true;
         }
         if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
         {
            ImGui::SetTooltip("%s", tooltip);
         }
         if (DrawResetButton(*value, default_value, key))
         {
            device_data.cb_luma_global_settings_dirty = true;
         }
         return *value > 0.5f;
      };
      // Persisted GameSettings slider, saved once the edit ends
      const auto settings_slider = [&](const char* label, const char* key, float* value, float default_value, float max_value, const char* tooltip)
      {
         if (ImGui::SliderFloat(label, value, 0.f, max_value))
         {
            device_data.cb_luma_global_settings_dirty = true;
         }
         if (ImGui::IsItemDeactivatedAfterEdit())
         {
            reshade::set_config_value(nullptr, NAME, key, *value);
         }
         if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
         {
            ImGui::SetTooltip("%s", tooltip);
         }
         if (DrawResetButton(*value, default_value, key))
         {
            device_data.cb_luma_global_settings_dirty = true;
         }
      };

      ImGui::SeparatorText("Anti-Aliasing");
      {
         // The game's render scale (its option steps by 10%), forced live (see "UpdateRenderScale")
         ImGui::BeginDisabled(!GetGameDeviceData(device_data).render_scale_setting);
         if (ImGui::SliderInt("Render Scale", &g_render_scale, 5, 10, "%d0%%", ImGuiSliderFlags_AlwaysClamp))
         {
            reshade::set_config_value(nullptr, NAME, "RenderScale", g_render_scale);
         }
         if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
         {
            ImGui::SetTooltip("The resolution the game renders at, upscaled by DLSS/FSR or stretched by the game.\nReplaces the game's own option.");
         }
         DrawResetButton(g_render_scale, 10, "RenderScale");
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
         {
            ImGui::SetTooltip("%s", smaa_tooltip);
         }
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
      {
         reshade::set_config_value(nullptr, NAME, "GTAOEnable", g_gtao_enable);
      }
      if (ImGui::IsItemHovered())
      {
         ImGui::SetTooltip("Replaces the game's SSAO with XeGTAO (cleaner, more accurate ambient occlusion; requires Ambient Occlusion enabled in the game's graphic settings).");
      }
      DrawResetButton(g_gtao_enable, true, "GTAOEnable");
#if DEVELOPMENT || TEST
      ImGui::BeginDisabled(!g_gtao_enable);
      ImGui::SliderFloat("GTAO Final Value Power", &g_gtao_final_value_power, 0.3f, 4.5f, "%.2f");
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
      {
         ImGui::SetTooltip("Primary darkness dial (higher = darker AO). Not saved.");
      }
      ImGui::SliderFloat("GTAO Radius Override", &g_gtao_radius_override, 0.f, 200.f, "%.1f");
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
      {
         ImGui::SetTooltip("0 = the game's SSAO radius (40 cm, capped at 108 half res pixels up close); > 0 overrides it, in centimetres, uncapped. Not saved.");
      }
#if DEVELOPMENT // the shader's debug blocks exist in DEVELOPMENT only
      ImGui::Combo("GTAO Debug View", &g_gtao_debug_view, "Off\0Depth gradient\0Normals\0AO x8\0Edges\0");
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
      {
         ImGui::SetTooltip("Draws diagnostics through the game's SSAO blurs into the G-buffer AO (darkens the ambient lighting only).\nDepth gradient flat or blocky = wrong input; Normals: camera-facing surfaces bright, black everywhere = NORMAL_Z_SIGN inverted;\nAO x8 = spot broad over-occlusion.");
      }
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
      {
         ImGui::SetTooltip("Disables the in-game UI.");
      }
   }

#if DEVELOPMENT
   void DrawImGuiDevSettings(DeviceData& device_data) override
   {
      ImGui::SeparatorText("SMAA");
      ImGui::Checkbox("SMAA Predication", &g_smaa_predication);
      if (ImGui::IsItemHovered())
      {
         ImGui::SetTooltip("Finds edges by geometry (plane deviation of the scene depth) as well as by color, so texture detail stays sharp while silhouettes are antialiased. Not saved.");
      }
      ImGui::Combo("SMAA Predication Debug View", &g_smaa_debug_view, "Off\0Edges\0Predication\0");
      if (ImGui::IsItemHovered())
      {
         ImGui::SetTooltip("Replaces the frame with SMAA's edges (red = horizontal, green = vertical) or the predication edge-ness (red).\nToggle SMAA Predication to compare: texture detail should lose edges, silhouettes keep them.");
      }

      ImGui::SeparatorText("Upscaling");
      ImGui::Checkbox("Post Process at Output Resolution", &g_post_output_resolution);
      if (ImGui::IsItemHovered())
      {
         ImGui::SetTooltip("Below a 100%% render scale, runs DOF and bloom after DLSS/FSR at the output resolution (bloom with the iterations the game adds there).\nOff: the previous way, at the render resolution on the upscaled frame scaled down. Not saved.");
      }

      ImGui::SeparatorText("Performance");
      ImGui::BeginDisabled(!IsSRActive(device_data));
      ImGui::Combo("Performance Test", &Perf::g_test, perf_test_modes, int(std::size(perf_test_modes)));
      ImGui::EndDisabled();
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
      {
         ImGui::SetTooltip("Sets the render scale for the mode and logs \"[P5S Perf]\" to ReShade.log every 120 frames: the GPU time of the scene (from its first depth draw to the split,\nthe motion vector draws included, and any GPU idle between the game's command lists) and of DLSS/FSR (timestamps, so the 60 fps cap doesn't matter), and the CPU time in the motion vector hooks.\nThe difference between DLAA and \"DLAA Without Motion Vector Draws\" is their cost (that mode's image is unjittered, with camera-only motion vectors).\nNeeds DLSS or FSR. Keep the camera still, set the GPU to maximum performance in the driver (a capped GPU downclocks). Not saved.");
      }
   }
#endif

   void PrintImGuiAbout() override
   {
      ImGui::PushTextWrapPos(0.f);
      ImGui::Text(
         "Luma for \"Persona 5 Strikers\" is developed by DristoforColumb and is open source and free.\n"
         "It adds HDR, DLSS or FSR 3 upscaling, and replaces the game's FXAA with SMAA and its SSAO with XeGTAO.\n"
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
      {
         ShellExecuteA(nullptr, "open", "https://ko-fi.com/dristoforcolumb", nullptr, nullptr, SW_SHOWNORMAL);
      }
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
      {
         ShellExecuteA(nullptr, "open", "https://github.com/Filoppi/Luma-Framework", nullptr, nullptr, SW_SHOWNORMAL);
      }

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
      // A letterboxed target's aspect ratio joins once seen (see "letterbox_hash" and "OnPresent"), none before
      texture_format_upgrades_2d_size_filters = (uint32_t)TextureFormatUpgrades2DSizeFilters::SwapchainAspectRatio | (uint32_t)TextureFormatUpgrades2DSizeFilters::CustomAspectRatio | (uint32_t)TextureFormatUpgrades2DSizeFilters::No1Px;
      texture_format_upgrades_2d_custom_aspect_ratios = {};
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
