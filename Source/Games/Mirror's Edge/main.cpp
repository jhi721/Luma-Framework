#define GAME_MIRRORS_EDGE 1

#define GEOMETRY_SHADER_SUPPORT 0
#define DISABLE_AUTO_DEBUGGER 1
// The UI and LDR overlay passes get their UNORM clamp back in place (see "PatchShaderBytecodeSync")
#define LUMA_PATCH_BYTECODE_SYNC 1
// SMAA ULTRA (+ RCAS) right after TdToneMapping (see "RunPostTonemapSMAA"); Core registers its 6 passes from Luma_SMAA_impl.hlsl
#define ENABLE_SMAA 1
#define ENABLE_RCAS 1
// SMAA's area texture without the U-shape smoothing (see Luma_SMAA_impl.hlsl)
#define SMAA_SMOOTH_U_SHAPES 0
// Makes "original_draw_dispatch_func" non-null: the tonemap draw runs first, then SMAA on its output; the scene's draws run with the
// motion vector shaders
#define ENABLE_POST_DRAW_DISPATCH_CALLBACK 1
// The motion vector draw key reads the draw's arguments ("last_draw_dispatch_data")
#define ENABLE_DRAW_DISPATCH_DATA_CACHE 1
// A third "Super Resolution" choice next to the SR bridge's DLSS and FSR 3, drawn in process on any GPU
#define ENABLE_LUMA_TAA 1

#include "..\..\Core\core.hpp"
#include "..\..\External\WDK\includes\d3d11TokenizedProgramFormat.hpp"
#include "MotionVectorPatches.h"
#include "..\..\Core\includes\patched_draws.h"

// Mirror's Edge (2008, Steam) is x86 UE3 on D3D9: it runs through dgVoodoo (D3D9 -> D3D11), so every hash below is a dgVoodoo
// 2.87.5 (ps_5_0) and 2.81.3 (ps_4_0, the Linux build) translation, mapped offline from the cooked RefShaderCache (NOTES.md).
// 2.87.5 hashes match the game; 2.81.3 ones reproduce BL2's Bink (0x7EBF990A).
// TdToneMapping is the only grade of the post chain (HDR -> LDR); GammaCorrection replaces it when the "TdTonemapping" system setting is off.
const ShaderHashesList shader_hashes_ToneMapping = {.pixel_shaders = {0x1A760388, 0x602DA054}};
// Passes drawn onto the post-tonemap canvas, which used to be UNORM: it clamped their output (dgVoodoo drops the SM3 _sat). Pairs are
// 2.87.5, 2.81.3 (Source/Games/Mirror's Edge/NOTES.md, UI table). Each has a single o0.xyzw output and a single final ret.
// UI overlays: saturate, as the UNORM store did.
// Simple Element, Simple Element Gamma (HUD, text), Simple Element Masked Gamma, Td UI Compositing, Td UI Blur Effect, Td UI Blur
// Gather (and Max Samples), Td UI Approximate Alpha, menu panels, pause background blur
const std::unordered_set<uint32_t> ui_saturated_pixel_shaders = {0x088F76F5, 0x889B0C84, 0xD81C8836, 0x3DFF6336, 0xC39EFBF2, 0x5325C38B, 0xA5AC2B70, 0xFB0A84E8, 0x92F6FB51, 0x49DB6B1C, 0xE93BE915, 0xA288BD8B, 0xC58A89A8, 0xC086B17E, 0xF8554FA4, 0x643DB945, 0xACB81A27, 0x6B9A89FF, 0x713B6AF9, 0xBFF59A82};
// Gameplay effects over the scene they read: floored at 0 only, an upper clamp would clip the HDR scene under them.
// Sniper scope, hurt filter, speed lines
const std::unordered_set<uint32_t> scene_floored_pixel_shaders = {0x946977C0, 0xC982CDCC, 0x70AD2D05, 0x15B360EC, 0x26F03F7F, 0x63D0CA51};
const ShaderHashesList shader_hashes_GammaCorrection = {.pixel_shaders = {0x0160196C, 0x87D136D3}};
// The last scene pass: it writes the canvas the HUD then draws onto
const ShaderHashesList shader_hashes_TdMotionBlur = {.pixel_shaders = {0x4177BC6E, 0xA99AD9DD}};
// DLAA / FSR / Luma TAA: the scene's first post passes (FX_PostProcess chain order, NOTES.md), which end a motion
// vector frame (the upscaler runs right before the first one). Pairs are 2.87.5, 2.81.3: TdDirectionalHaze, DOF And Bloom Gather (and
// Max Samples), DOF And Bloom Blend, engine Motion Blur (and Dynamic Velocities Only; off in the shipped chain), Uber Post Process
// Blend (cooked, unused), then TdToneMapping and GammaCorrection, which end a chain without the others.
const ShaderHashesList shader_hashes_ScenePost = {.pixel_shaders = {0x19BCE666, 0x9C77EAD7, 0x2BE8B514, 0x587465D1, 0xB977A1C4, 0x69A1F64A, 0x780BCE69, 0x8B012337, 0x8F95BB29, 0x37EF3D5C, 0xFF77A7B0, 0xE9224963, 0xFC10C0BD, 0x39B803FA, 0x1A760388, 0x602DA054, 0x0160196C, 0x87D136D3}};

// User settings, persisted in the [Luma] config section (LoadConfigs)
static bool g_smaa_enable = true;
static bool g_smaa_t2x = false; // SMAA T2x: a two phase jitter and the previous frame, through the upscalers' motion vectors
static float g_rcas_sharpness = 0.f;
// DEV tuning: predication from the scene alpha's linear depth, and its plane-deviation tolerance (Luma_ME_DepthExtract.hlsl)
static bool g_smaa_predication = true;
static float g_smaa_pred_tolerance = 0.02f;
// HDR-weighted MSAA resolve (Luma_ME_MSAAResolve) in place of the game's, with the game's Anti-Aliasing on
static bool g_msaa_resolve_enable = true;
#if DEVELOPMENT
static bool g_msaa_resolve_average_depth = false; // The game's averaged depth in the resolved alpha, for halo A/B
static bool g_msaa_coverage_enable = true;        // The masked materials' alpha to coverage (with the resolve, "g_msaa_resolve_enable")
#else
constexpr bool g_msaa_resolve_average_depth = false;
constexpr bool g_msaa_coverage_enable = true;
#endif
// Session only, never persisted: a stuck "on" would look like a broken HUD (Mass Effect 2007)
static bool g_hide_ui = false;
// SMAA's, T2x's and the reactive masks' resources go after this many frames without their pass (MEM-11)
constexpr uint32_t IDLE_RELEASE_FRAMES = 600;

#if DEVELOPMENT
static bool g_mv_enable = false;       // Motion vectors without an upscaler
static bool g_mv_debug_view = false;   // Core's debug draw of the motion vector target
static bool g_mv_force_jitter = false; // The projection jitter without an upscaler
static bool g_sr_reactive_enable = true;
static float g_sr_reactive_scale = 1.f;      // The alpha blended draws' reactivity, scaled (AMD's default 1)
static float g_sr_reactive_threshold = 0.5f; // Under it 0, over it 0.9 (AMD's 0.2; Mass Effect 2007's and Borderlands 2's 0.5); 0: the scaled reactivity, capped at 0.9
static bool g_sr_reactive_debug_view = false;
#else
constexpr bool g_mv_enable = false;
constexpr bool g_mv_force_jitter = false;
constexpr bool g_sr_reactive_enable = true;
constexpr float g_sr_reactive_scale = 1.f;
constexpr float g_sr_reactive_threshold = 0.5f;
#endif

// "DrawWithMotionVectors" refusals, counted per frame in DEVELOPMENT ("mv.rejected.*")
enum MotionVectorReject
{
   REJECT_EXTRA_TARGET,
   REJECT_NO_SCENE,
   REJECT_DEPTH_TEST,
   REJECT_BLEND,
   REJECT_OTHER_DEPTH_COLOR,
   REJECT_FORMAT,
   REJECT_SIZE,
   REJECT_CREATE,
   REJECT_SHADERS,
   REJECT_COUNT
};
#if DEVELOPMENT
constexpr const char* MOTION_VECTOR_REJECT_NAMES[REJECT_COUNT] = {"extra_target", "no_scene", "depth_test", "blend", "other_depth_color", "format", "size", "create", "shaders"};
#endif

// A pixel shader's patch: the motion vector target, the mask target of an alpha blended (1, as "mv_reactive_blend") or additive (2)
// draw, or an alpha blended draw's own motion vectors; or the MSAA alpha to coverage
enum class PixelShaderPatch : uint8_t
{
   MOTION_VECTORS,
   MASK_ALPHA,
   MASK_ADDITIVE,
   BLENDED,
   COVERAGE,
   COUNT
};
constexpr const char* PIXEL_SHADER_PATCH_NAMES[size_t(PixelShaderPatch::COUNT)] = {"MV PS", "MV PS (mask)", "MV PS (mask)", "MV PS (blended)", "Coverage PS"};

struct MirrorsEdgeGameDeviceData final : public GameDeviceData
{
   // SMAA metrics CB (b1) = (1/w, 1/h, w, h) + (predication threshold scale, 0, 0, 0) + T2x's subsample indices: scale 2.0 with
   // predication, else 1.0. Written every frame.
   com_ptr<ID3D11Buffer> cb_smaa_metrics;

   // SMAA and RCAS input: a snapshot of the tonemap's output (fp16, gamma encoded). Edge detection reads it as stored, the
   // neighborhood blend filters it in linear light (Luma_SMAA_impl.hlsl)
   ComPtr<ID3D11Texture2D> tex_input_encoded;
   ComPtr<ID3D11ShaderResourceView> srv_input_encoded;
   // The frames SMAA, the snapshot's users (SMAA or RCAS) and SMAA's output temp last ran, for "IDLE_RELEASE_FRAMES"
   uint32_t smaa_frame = 0;
   uint32_t snapshot_frame = 0;
   uint32_t smaa_out_frame = 0;

   // SMAA output temp, only with RCAS (its input). Without RCAS SMAA renders into the tonemap's target directly
   ComPtr<ID3D11Texture2D> tex_smaa_out;
   ComPtr<ID3D11RenderTargetView> tex_smaa_out_rtv;
   ComPtr<ID3D11ShaderResourceView> tex_smaa_out_srv;

   // Predication: the tonemap's scene SRV (linear depth in .a), captured at its draw, and the R16F edge-ness built from it
   ComPtr<ID3D11ShaderResourceView> srv_scene_depth;
   // TdToneMapping's exposure (t3, 1x1, exposure / 64), kept across frames: the MSAA resolves run before this frame's is built
   ComPtr<ID3D11ShaderResourceView> srv_exposure;
   com_ptr<ID3D11Buffer> cb_pred;
   ComPtr<ID3D11Texture2D> tex_pred;
   ComPtr<ID3D11UnorderedAccessView> uav_pred;
   ComPtr<ID3D11ShaderResourceView> srv_pred;

   // The canvas TdMotionBlur wrote this frame (the upgrade's mirror when upgraded: the bound resource either way), for Hide UI
   com_ptr<ID3D11Resource> canvas_res;

   // SMAA T2x (see "RunPostTonemapSMAA"), set at present for the whole frame like "sr_active". The scene jitters only after a frame
   // the resolve ran. The phase is chosen when the scene opens, -1 if it didn't jitter.
   bool t2x_active = false;
   int t2x_phase = -1;
   // SMAA's output of this frame and of the previous one, alternating: linear RGB with the velocity length in alpha. The previous one
   // is the history only if "t2x_frame", the last frame the resolve ran, was the frame before.
   ComPtr<ID3D11Texture2D> t2x_frames[2];
   ComPtr<ID3D11RenderTargetView> t2x_frame_rtvs[2];
   ComPtr<ID3D11ShaderResourceView> t2x_frame_srvs[2];
   uint32_t t2x_frame = 0;

   // ---- DLAA / FSR 3 Native AA / Luma TAA with motion vectors and jitter from patched shaders (Mass Effect 2007's path, one per-draw
   // buffer: dgVoodoo's vc4) ----
   // DLSS and FSR run in the x64 helper of Core's SR bridge (the game is 32-bit, see "SRBridge.h"), Luma TAA in process.
   // An upscaler is picked, hasn't failed (it then gives way to SMAA until picked again) and the game's MSAA is off, latched at present
   // for the whole frame: a selection made after the motion vector state was set would otherwise run the upscaler on mixed state
   bool sr_active = false;
   // The upscaler's output goes back into the scene (and its copy, see "mv_scene_copy") without its alpha (the linear depth the post
   // passes read), drawn from this view of it
   com_ptr<ID3D11BlendState> sr_rgb_blend_state;
   com_ptr<ID3D11ShaderResourceView> sr_output_srv;
   // None was picked ("CleanExtraSRResources", from the overlay) or T2x turned off: the upscaler's resources go at the next present
   // while none is picked
   std::atomic<bool> release_sr_resources = false;

   std::atomic<bool> mv_active = false; // Motion vectors and jitter this frame: an upscaler, SMAA T2x or the DEV toggle (set at present)
   std::shared_mutex mv_mutex;
   // A game shader's patched version (null if refused), patched on first use, by its hash; a vertex shader's with the bytes of vc4 it
   // reads ("DXBC::ConstantBufferBytes": the previous frame's copy uploads only those) and its LocalToWorld translation row
   template <typename T>
   struct PatchedShader
   {
      com_ptr<T> shader;
      UINT read_size = 0;
      UINT translation_offset = 0;
   };
   std::unordered_map<uint32_t, PatchedShader<ID3D11VertexShader>> mv_vertex_shaders;
   // By "PixelShaderPatch" (the coverage one null for every shader without a masked material's clip, see "PatchPixelShaderCoverage")
   std::unordered_map<uint32_t, PatchedShader<ID3D11PixelShader>> patched_pixel_shaders[size_t(PixelShaderPatch::COUNT)];
   // The motion vector target (sized like the scene; the alpha blending states blend it like their color, the others write it
   // unblended, see "OnCreateBlendState")
   com_ptr<ID3D11Texture2D> mv_texture;
   com_ptr<ID3D11RenderTargetView> mv_rtv;
   com_ptr<ID3D11UnorderedAccessView> mv_uav; // Null without typed UAV loads of its format (then no upscaler)
   com_ptr<ID3D11ShaderResourceView> mv_srv;  // SMAA T2x's view of it
   bool mv_filled = false;                    // The fill wrote this frame's motion vectors (reset at present)
   // The upscaler's depth, built from the scene's alpha by the fill (the game's depth has no shader resource view)
   com_ptr<ID3D11Texture2D> mv_device_depth;
   com_ptr<ID3D11UnorderedAccessView> mv_device_depth_uav;
   // The reactive and transparency & composition masks (FSR, Luma TAA), written by the fill from "mv_reactive_target"
   com_ptr<ID3D11Texture2D> mv_reactive;
   com_ptr<ID3D11UnorderedAccessView> mv_reactive_uav;
   com_ptr<ID3D11Texture2D> mv_transparency;
   com_ptr<ID3D11UnorderedAccessView> mv_transparency_uav;
   // What the alpha blended draws wrote (x reactive, y transparency & composition; max blended, see "OnCreateBlendState"), read by the
   // fill. Created when a mask user first runs, gone "IDLE_RELEASE_FRAMES" after the fill last wrote them.
   com_ptr<ID3D11Texture2D> mv_reactive_target;
   com_ptr<ID3D11RenderTargetView> mv_reactive_target_rtv;
   com_ptr<ID3D11ShaderResourceView> mv_reactive_target_srv;
   uint32_t sr_reactive_frame = 0;
   // A frame opens at its first mesh draw into output sized depth (the jitter is chosen there), starts at its first motion vector
   // draw (the target is cleared) and ends at the first post pass ("shader_hashes_ScenePost"), once per present.
   bool mv_scene_open = false;
   bool mv_scene_done = false;
   bool mv_frame_ended = true;
   bool mv_fill_pending = false;
   // The upscaler drew the last frame: only then the scene jitters (frames it skips would reach the screen jittered)
   bool mv_jitter_allowed = false;
   float sr_vert_fov = 1.0471976f; // FSR's vertical FOV (radians): the last camera's, 60 degrees until one is seen
   // The upscalers' near and far (game units): the last camera's with a usable projection, Core's defaults until one is seen
   float sr_near_plane = SR::SuperResolutionImpl::DrawData{}.near_plane;
   float sr_far_plane = SR::SuperResolutionImpl::DrawData{}.far_plane;
   com_ptr<ID3D11Resource> mv_depth; // The scene depth (the depth view's resource)
   // The fp16 scene the motion vector draws write, and the game's view of it (the upscaler's copy back)
   com_ptr<ID3D11Resource> mv_scene_color;
   com_ptr<ID3D11RenderTargetView> mv_scene_rtv;
   com_ptr<ID3D11ShaderResourceView> mv_scene_srv; // The fill's view of it, kept while the scene is the same resource
   // The copy of the scene the first post pass reads (UE3 resolves the scene surface into a texture), null if it reads none. The
   // upscaler's output goes into both ("ME Copy Back PS"): a post pass that draws onto the scene and resolves it again would
   // otherwise resolve the jittered scene (Borderlands 2's light shafts).
   com_ptr<ID3D11Resource> mv_scene_copy;
   com_ptr<ID3D11RenderTargetView> mv_scene_copy_rtv;
   com_ptr<ID3D11Buffer> mv_fill_buffer;
   // The projection jitter (pixels, +y down), chosen when the scene opens; its NDC offset is at VS "MotionVectorPatches::jitter_slot"
   // of every mesh draw depth tested against the scene
   std::array<float, 2> mv_jitter = {};
   std::array<float, 2> mv_jitter_ndc = {}; // The same offset in NDC (y up), as the jitter buffer holds it
   com_ptr<ID3D11Buffer> mv_jitter_buffer;
   // Per-draw lookups kept for the next draw (reset when the scene opens): the jitter path's last depth view and whether it's the scene
   // depth, its last depth stencil state's depth test, the motion vector path's last accepted targets and the last blend state's
   // opacity, the last vertex and pixel shader's patched versions (owned by the maps above, never erased)
   ID3D11DepthStencilView* jitter_dsv = nullptr;
   bool jitter_dsv_scene = false;
   ID3D11DepthStencilState* jitter_depth_stencil_state = nullptr;
   bool jitter_depth_test = true;
   ID3D11DepthStencilView* mv_accepted_dsv = nullptr;
   ID3D11BlendState* mv_blend_state = nullptr; // See "ClassifyBoundBlend"
   bool mv_blend_opaque = true;
   bool mv_alpha_blended = false;
   uint8_t mv_reactive_blend = 0;
   uint32_t mv_last_vertex_shader_hash = 0;
   PatchedShader<ID3D11VertexShader> mv_last_vertex_shader;
   uint32_t mv_last_pixel_shader_hash = 0;
   ID3D11PixelShader* mv_last_pixel_shader = nullptr;
   PatchedDraws::BoundShader<ID3D11VertexShader> mv_bound_vertex_shader;
   PatchedDraws::BoundShader<ID3D11PixelShader> mv_bound_pixel_shader;
   // The scene's targets refused by "DrawWithMotionVectors" (other depth or color, format, size) and the reason, the depth view
   // "OpenScene" refused (not output sized) and whether the jitter buffer is bound; reset when the scene opens (the refused depth view
   // at present). The prepass's R8G8B8A8 target and the shadow maps are refused once per frame, not per draw.
   ID3D11RenderTargetView* mv_refused_rtv = nullptr;
   ID3D11DepthStencilView* mv_refused_dsv = nullptr;
   MotionVectorReject mv_refused_reason = REJECT_COUNT;
   ID3D11DepthStencilView* open_refused_dsv = nullptr;
   bool mv_jitter_bound = false;
   // The marker texels got their camera motion before the frame's first alpha blended motion vector draw (see "DrawWithMotionVectors")
   bool mv_markers_filled = false;
   // MSAA is on in the game (a multisampled scene resolved): the upscalers and SMAA T2x can't run (shown in the settings).
   // "msaa_resolved" this frame, "msaa_active" the last one (the coverage draws come before this frame's resolves).
   bool msaa_resolved = false;
   bool msaa_active = false;
   // The immediate context's blend and depth stencil states, vc4 (VS "MotionVectorPatches::object_slot"), vertex buffer 0 and index
   // buffer as the game last bound them ("OnBindPipeline", "OnPushConstantBuffers", "OnBindVertexBuffers", "OnBindIndexBuffer"), so
   // the draw hooks don't query them (an AddRef and a Release each, several per draw; Borderlands 2's). Not referenced: an object lives
   // while bound. Luma's own passes bind theirs natively and put the game's back. False after a bind this can't read (another add-on's
   // combined pipeline): queried then.
   ID3D11BlendState* bound_blend_state = nullptr;
   ID3D11DepthStencilState* bound_depth_stencil_state = nullptr;
   ID3D11Buffer* bound_object_buffer = nullptr;
   ID3D11Buffer* bound_vertex_buffer = nullptr;
   UINT bound_vertex_offset = 0;
   ID3D11Buffer* bound_index_buffer = nullptr;
   UINT bound_index_offset = 0;
   bool bound_states_tracked = true;

   // Pointer and "HashCombine" keys: one multiply spreads them over the buckets (std::hash runs FNV-1a on every byte)
   struct KeyHash
   {
      size_t operator()(uint64_t key) const noexcept
      {
         return size_t((key * 0x9E3779B97F4A7C15ull) >> 32);
      }
   };

   // CPU copies of the vc4 buffers the motion vector draws bind, by buffer (an entry registers it, empty until its first upload), from
   // a Map(WRITE_DISCARD) at its Unmap or an UpdateSubresource: a draw's constants are its buffer's latest upload, taken as is. Unlocked:
   // the buffer hooks, the draws and "OnPresent" all run on the immediate context's thread (dgVoodoo has no deferred contexts).
   using ConstantsCopy = std::shared_ptr<const std::vector<uint8_t>>;
   struct RegisteredConstants
   {
      // The last upload, a pooled copy (dgVoodoo maps a vc4 buffer for most draws, motion vector draws or not)
      ConstantsCopy copy;
      void* mapped = nullptr; // Mapped now, until its Unmap
      UINT size = 0;          // The buffer's
   };
   std::unordered_map<uint64_t, RegisteredConstants, KeyHash> mv_constants_copies;
   // The first registered buffers and their entries (a node's address stays while the map grows), so a buffer hook finds them without
   // a lookup. With more registered, every buffer takes the map lookup.
   static constexpr uint32_t MAX_FILTERED_BUFFERS = 8;
   std::array<std::pair<uint64_t, RegisteredConstants*>, MAX_FILTERED_BUFFERS> mv_filtered_buffers = {};
   uint32_t mv_filtered_buffer_count = 0;
   bool mv_filter_overflow = false;
   // Every pooled vc4 copy, and those nobody held anymore at the last present (taken by the next copies)
   std::vector<std::shared_ptr<std::vector<uint8_t>>> mv_constants_pool;
   std::vector<uint32_t> mv_constants_pool_free;
   size_t mv_constants_made = 0; // Copies asked for since the last present
   // Previous frame constants of the motion vector draws (see "PatchedDraws::PreviousConstants")
   PatchedDraws::PreviousConstants mv_previous_constants;
   // Motion vector draws by draw key (shaders, buffers, arguments), with LocalToWorld and vc4. A draw takes the previous frame's vc4 of
   // its key's nearest draw (same object, a frame earlier), its camera included.
   struct MotionVectorObject
   {
      PatchedDraws::ObjectTransform transform;
      ConstantsCopy constants;
   };
   std::unordered_map<uint64_t, std::vector<MotionVectorObject>, KeyHash> mv_objects;
   std::unordered_map<uint64_t, std::vector<MotionVectorObject>, KeyHash> mv_previous_objects;
   // The frame's camera (vc4 of its first motion vector draw) and the previous frame's
   ConstantsCopy mv_camera;
   ConstantsCopy mv_previous_camera;
   uint32_t mv_frame_index = 0;              // The Luma frame index of the last motion vector frame
   std::vector<uint8_t> mv_camera_only_copy; // An unmatched draw's constants with last frame's camera (reused)

#if DEVELOPMENT
   // Per frame counts for the MCP (the last complete frame's)
   struct MotionVectorStats
   {
      uint32_t motion_vector_draws = 0, jitter_draws = 0, reactive_draws = 0, matched = 0, camera_only = 0, other_camera = 0, uncopied = 0, maps = 0, updates = 0, other_maps = 0, sr_draws = 0;
      uint32_t tiebreak_collisions = 0; // Objects sharing a key and a transform with other constants ("PatchedDraws::CountTieBreakCollisions")
      uint32_t ended_by = 0;            // The ending pass's PS hash
      uint32_t msaa_resolves = 0;       // "OnResolveTextureRegion"'s replaced resolves
      uint32_t rejected[REJECT_COUNT] = {};
   };
   MotionVectorStats mv_stats, mv_last_stats;
   MotionVectorReject mv_draw_reject = REJECT_COUNT; // The current draw's refusal ("REJECT_COUNT" for none), for the MCP trace note
#endif

   void ReleaseT2xFrames()
   {
      for (int i = 0; i < 2; i++)
      {
         t2x_frames[i].reset();
         t2x_frame_rtvs[i].reset();
         t2x_frame_srvs[i].reset();
      }
   }

   // The motion vector target and the device depth the fill writes, recreated at their next use
   void ReleaseMotionVectorTargets()
   {
      mv_texture.reset();
      mv_rtv.reset();
      mv_uav.reset();
      mv_srv.reset();
      mv_device_depth.reset();
      mv_device_depth_uav.reset();
      ReleaseReactiveMasks();
   }

   // The reactive masks and what the draws wrote for them, recreated at their next use
   void ReleaseReactiveMasks()
   {
      mv_reactive.reset();
      mv_reactive_uav.reset();
      mv_transparency.reset();
      mv_transparency_uav.reset();
      mv_reactive_target.reset();
      mv_reactive_target_rtv.reset();
      mv_reactive_target_srv.reset();
   }

   // SMAA's own resources (predication, output temp), apart from Core's ("ReleaseSMAAIntermediates"); recreated at their next use
   void ReleaseSMAAScratch()
   {
      tex_pred.reset();
      uav_pred.reset();
      srv_pred.reset();
      ReleaseSMAAOutput();
   }

   void ReleaseSMAAOutput()
   {
      tex_smaa_out.reset();
      tex_smaa_out_rtv.reset();
      tex_smaa_out_srv.reset();
   }

   void ReleaseSnapshotScratch()
   {
      tex_input_encoded.reset();
      srv_input_encoded.reset();
   }
};

static MirrorsEdgeGameDeviceData& GetGameDeviceData(DeviceData& device_data)
{
   return *static_cast<MirrorsEdgeGameDeviceData*>(device_data.game);
}

static const MirrorsEdgeGameDeviceData& GetGameDeviceData(const DeviceData& device_data)
{
   return *static_cast<const MirrorsEdgeGameDeviceData*>(device_data.game);
}

class MirrorsEdge final : public Game
{
   // Temps shaped like the tonemap's target take its format, since CopyResource requires identical formats
   static bool CreateDefaultTex(ID3D11Device* device, uint32_t w, uint32_t h, UINT bind_flags, ComPtr<ID3D11Texture2D>* out, DXGI_FORMAT format)
   {
      out->reset();
      const CD3D11_TEXTURE2D_DESC td(format, w, h, 1, 1, bind_flags);
      return SUCCEEDED(device->CreateTexture2D(&td, nullptr, out->put()));
   }

   // SMAA on the tonemap's output (gamma encoded, before TdMotionBlur and the HUD, so the UI is never antialiased), Borderlands 2's
   // chain: snapshot -> predication extract CS -> DrawSMAA -> optional RCAS, the last pass writing the tonemap's target. Without
   // "smaa" (an upscaler antialiased the frame) only RCAS runs.
   void RunPostTonemapSMAA(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, DeviceData& device_data, MirrorsEdgeGameDeviceData* gd, ID3D11RenderTargetView* ldr_rtv, bool smaa)
   {
      ComPtr<ID3D11Resource> ldr_res;
      ldr_rtv->GetResource(ldr_res.put());
      uint4 ldr_size{};
      DXGI_FORMAT ldr_format = DXGI_FORMAT_UNKNOWN;
      GetResourceInfo(ldr_res.get(), ldr_size, ldr_format);
      const uint32_t w = ldr_size.x, h = ldr_size.y;
      if (w == 0 || h == 0 || ldr_format == DXGI_FORMAT_UNKNOWN)
         return;

      // DrawSMAA looks its passes up with ".at()": a shader reload must not release them mid-chain (Sunset Overdrive)
      const std::shared_lock lock_shader_objects(s_mutex_shader_objects);

      auto* copy_vs = FindShader(device_data.native_vertex_shaders, CompileTimeStringHash("Copy VS"));
      bool do_sharpen = g_rcas_sharpness > 0.f && PrepareRCAS(native_device, device_data);

      const uint32_t frame_index = cb_luma_global_settings.FrameIndex;
      const auto snapshot = [&]
      {
         gd->snapshot_frame = frame_index;
         if (!gd->srv_input_encoded || GetViewTextureSize(gd->srv_input_encoded.get()) != uint2{w, h})
         {
            gd->ReleaseSnapshotScratch();
            if (CreateDefaultTex(native_device, w, h, D3D11_BIND_SHADER_RESOURCE, std::addressof(gd->tex_input_encoded), ldr_format))
            {
               native_device->CreateShaderResourceView(gd->tex_input_encoded.get(), nullptr, gd->srv_input_encoded.put());
            }
         }
         if (!gd->srv_input_encoded)
            return false;
         native_device_context->CopyResource(gd->tex_input_encoded.get(), ldr_res.get());
         return true;
      };

      if (!smaa)
      {
         if (!do_sharpen)
            return;
         if (snapshot())
         {
            DrawRCAS(native_device_context, device_data, gd->srv_input_encoded.get(), ldr_rtv, g_rcas_sharpness);
         }
         return;
      }

      // All or nothing: skip SMAA this frame while any pass is missing (async load, live reload)
      if (!HasSMAAShaders(device_data))
         return;
      gd->smaa_frame = frame_index;

      // Predication from the scene the tonemap read, which maps 1:1 only at the same size. Plain ULTRA when any input is missing
      // (never scale 2.0 with a null texture)
      auto* pred_cs = FindShader(device_data.native_compute_shaders, CompileTimeStringHash("ME Depth Extract CS"));
      bool pred_ok = g_smaa_predication && gd->srv_scene_depth && pred_cs != nullptr && GetViewTextureSize(gd->srv_scene_depth.get()) == uint2{w, h};
      if (pred_ok)
      {
         const float p[4] = {g_smaa_pred_tolerance, 0.f, 0.f, 0.f};
         PatchedDraws::WriteDynamicConstants(native_device, native_device_context, std::addressof(gd->cb_pred), p, sizeof(p));
         if (!gd->srv_pred || GetViewTextureSize(gd->srv_pred.get()) != uint2{w, h})
         {
            gd->uav_pred.reset();
            gd->srv_pred.reset();
            if (CreateDefaultTex(native_device, w, h, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, std::addressof(gd->tex_pred), DXGI_FORMAT_R16_FLOAT))
            {
               native_device->CreateUnorderedAccessView(gd->tex_pred.get(), nullptr, gd->uav_pred.put());
               native_device->CreateShaderResourceView(gd->tex_pred.get(), nullptr, gd->srv_pred.put());
            }
         }
         pred_ok = gd->cb_pred && gd->uav_pred && gd->srv_pred;
      }

      if (do_sharpen)
      {
         gd->smaa_out_frame = frame_index;
         if (!gd->tex_smaa_out_srv || GetViewTextureSize(gd->tex_smaa_out_srv.get()) != uint2{w, h})
         {
            gd->ReleaseSMAAOutput();
            if (CreateDefaultTex(native_device, w, h, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET, std::addressof(gd->tex_smaa_out), ldr_format))
            {
               native_device->CreateRenderTargetView(gd->tex_smaa_out.get(), nullptr, gd->tex_smaa_out_rtv.put());
               native_device->CreateShaderResourceView(gd->tex_smaa_out.get(), nullptr, gd->tex_smaa_out_srv.put());
            }
         }
         do_sharpen = gd->tex_smaa_out_rtv && gd->tex_smaa_out_srv;
      }

      if (!snapshot())
         return;

      // Unbinding the mask's UAV before DrawSMAA is what makes it readable: an SRV of a resource still bound as a UAV reads as null
      if (pred_ok)
      {
         ID3D11ShaderResourceView* cs_srv = gd->srv_scene_depth.get();
         ID3D11UnorderedAccessView* cs_uav = gd->uav_pred.get();
         ID3D11Buffer* cs_cb = gd->cb_pred.get();
         native_device_context->CSSetUnorderedAccessViews(0, 1, &cs_uav, nullptr);
         native_device_context->CSSetShaderResources(0, 1, &cs_srv);
         native_device_context->CSSetConstantBuffers(0, 1, &cs_cb);
         native_device_context->CSSetShader(pred_cs, nullptr, 0);
         native_device_context->Dispatch((w + 7) / 8, (h + 7) / 8, 1);
         ID3D11UnorderedAccessView* const null_uav = nullptr;
         native_device_context->CSSetUnorderedAccessViews(0, 1, &null_uav, nullptr);
      }

      // SMAA T2x: SMAA into this frame's linear target with the velocity, then the resolve with the previous frame into the chain's
      // output (Borderlands 2's). It needs this frame's motion vectors from the fill, at the target's size. Without them this frame is
      // 1x and the next one doesn't jitter.
      auto* const t2x_weight_ps = FindShader(device_data.native_pixel_shaders, CompileTimeStringHash("ME SMAA T2x Blending Weight Calculation PS"));
      auto* const t2x_blend_ps = FindShader(device_data.native_pixel_shaders, CompileTimeStringHash("ME SMAA T2x Neighborhood Blending PS"));
      auto* const t2x_resolve_ps = FindShader(device_data.native_pixel_shaders, CompileTimeStringHash("ME SMAA T2x Resolve PS"));
      bool t2x = gd->t2x_active && gd->mv_filled && gd->mv_srv && t2x_weight_ps && t2x_blend_ps && t2x_resolve_ps && copy_vs && GetViewTextureSize(gd->mv_srv.get()) == uint2{w, h};
      const int t2x_current = int(frame_index & 1);
      bool t2x_history = gd->t2x_frames[0] && gd->t2x_frame + 1 == frame_index;
      if (t2x && GetViewTextureSize(gd->t2x_frame_srvs[0].get()) != uint2{w, h})
      {
         t2x_history = false;
         gd->ReleaseT2xFrames();
         for (int i = 0; i < 2; i++)
         {
            if (!CreateDefaultTex(native_device, w, h, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET, std::addressof(gd->t2x_frames[i]), DXGI_FORMAT_R16G16B16A16_FLOAT) ||
                FAILED(native_device->CreateRenderTargetView(gd->t2x_frames[i].get(), nullptr, gd->t2x_frame_rtvs[i].put())) ||
                FAILED(native_device->CreateShaderResourceView(gd->t2x_frames[i].get(), nullptr, gd->t2x_frame_srvs[i].put())))
            {
               gd->ReleaseT2xFrames();
               break;
            }
         }
         t2x = gd->t2x_frames[0].get() != nullptr;
      }

      // b1: the metrics, the predication threshold scale and T2x's subsample indices for the jitter phase (see "OpenScene"), 0 for 1x
      const float subsample_indices = ((t2x && gd->t2x_phase >= 0) ? float(gd->t2x_phase + 1) : 0.f);
      // "Luma_SMAA_impl.hlsl"'s "SmaaMetricsCB"
      struct SmaaMetricsConstants
      {
         float rt_metrics[4];
         float predication[4];
         float subsample_indices[4];
      };
      static_assert(sizeof(SmaaMetricsConstants) == 48);
      const SmaaMetricsConstants metrics = {
         .rt_metrics = {1.f / (float)w, 1.f / (float)h, (float)w, (float)h},
         .predication = {(pred_ok ? 2.0f : 1.0f), 0.f, 0.f, 0.f},
         .subsample_indices = {subsample_indices, subsample_indices, subsample_indices, 0.f},
      };
      if (!PatchedDraws::WriteDynamicConstants(native_device, native_device_context, std::addressof(gd->cb_smaa_metrics), &metrics, sizeof(metrics)))
         return;

      // Metrics at VS+PS b1, bound here rather than through "DrawSMAA()" (which restores the slot): the T2x resolve reads them too
      ID3D11Buffer* metrics_cb = gd->cb_smaa_metrics.get();
      native_device_context->VSSetConstantBuffers(1, 1, &metrics_cb);
      native_device_context->PSSetConstantBuffers(1, 1, &metrics_cb);

      // Rendering into the tonemap's target is safe: SMAA and RCAS sample the snapshot, never the target itself
      ID3D11RenderTargetView* const output_rtv = (do_sharpen ? gd->tex_smaa_out_rtv.get() : ldr_rtv);
      const SMAAT2xPasses t2x_passes = {.blending_weight_calculation_ps = t2x_weight_ps, .neighborhood_blending_ps = t2x_blend_ps, .velocity = gd->mv_srv.get()};
      DrawSMAA(native_device, native_device_context, device_data,
         t2x ? gd->t2x_frame_rtvs[t2x_current].get() : output_rtv,
         gd->srv_input_encoded.get() /*neighborhood blend (filtered in linear light)*/,
         gd->srv_input_encoded.get() /*edge detection (gamma)*/,
         pred_ok ? gd->srv_pred.get() : nullptr,
         nullptr /*metrics, bound above*/,
         t2x ? &t2x_passes : nullptr);

      if (t2x)
      {
         // The resolve: t0 this frame, t1 the previous one or this one again without a history, t2 the motion vectors, s0 linear and
         // s1 point
         // The patched draws may leave the motion vector target bound, and its view would then read as null
         native_device_context->OMSetRenderTargets(0, nullptr, nullptr);
         ID3D11ShaderResourceView* const resolve_srvs[2] = {gd->t2x_frame_srvs[t2x_history ? (t2x_current ^ 1) : t2x_current].get(), gd->mv_srv.get()};
         native_device_context->PSSetShaderResources(1, 2, resolve_srvs);
         ID3D11SamplerState* const resolve_samplers[2] = {device_data.sampler_state_linear.get(), device_data.sampler_state_point.get()};
         native_device_context->PSSetSamplers(0, 2, resolve_samplers);
         DrawCustomPixelShader(native_device_context, device_data.default_depth_stencil_state.get(), device_data.default_blend_state.get(), nullptr,
            copy_vs, t2x_resolve_ps, gd->t2x_frame_srvs[t2x_current].get(), output_rtv, w, h, false);
         gd->t2x_frame = frame_index;
      }

      if (do_sharpen)
      {
         DrawRCAS(native_device_context, device_data, gd->tex_smaa_out_srv.get(), ldr_rtv, g_rcas_sharpness);
      }
   }

   // ---- DLAA / FSR 3 Native AA / Luma TAA (vc4 layout in "MotionVectorPatches") ----
   // c0-c3, row vectors
   static constexpr size_t VIEW_PROJECTION_OFFSET = MotionVectorPatches::view_projection_row * 16;
   // ... and c4, the camera position
   static constexpr size_t CAMERA_SIZE = 5 * 16;
   // c8: LocalToWorld's translation row
   static constexpr size_t TRANSLATION_OFFSET = (MotionVectorPatches::view_projection_row + 8) * 16;
   // Skinned vertex shaders (bones from c5) hold LocalToWorld at c230-c233
   static constexpr size_t SKINNED_TRANSLATION_OFFSET = (MotionVectorPatches::view_projection_row + 233) * 16;
   // A camera ("mv_camera") comes from constants holding the translation row, so it always holds the whole camera
   static_assert(TRANSLATION_OFFSET + 16 >= VIEW_PROJECTION_OFFSET + CAMERA_SIZE);

   // The upscalers that read the reactive mask the alpha blended draws write: FSR and Luma TAA (DLSS's current presets ignore it,
   // DLSS-Best-Practices TRN-2)
   static bool IsReactiveMaskUsed(const DeviceData& device_data)
   {
      return g_sr_reactive_enable && GetGameDeviceData(device_data).sr_active && (device_data.sr_type == SR::Type::FSR || device_data.sr_type == SR::Type::LumaTAA);
   }

   // The registered vc4 buffer's entry in "mv_constants_copies", null if the buffer isn't one
   static MirrorsEdgeGameDeviceData::RegisteredConstants* FindRegisteredBuffer(MirrorsEdgeGameDeviceData* gd, uint64_t handle)
   {
      if (!gd->mv_filter_overflow)
      {
         for (uint32_t i = 0; i < gd->mv_filtered_buffer_count; i++)
         {
            if (const auto& [filtered_handle, entry] = gd->mv_filtered_buffers[i]; filtered_handle == handle)
               return entry;
         }
         return nullptr;
      }
      const auto registered = gd->mv_constants_copies.find(handle);
      return (registered != gd->mv_constants_copies.end() ? &registered->second : nullptr);
   }

   // Registers a vc4 buffer for the copies (see "mv_constants_copies"), in the filter while it has room
   static MirrorsEdgeGameDeviceData::RegisteredConstants* RegisterBuffer(MirrorsEdgeGameDeviceData* gd, ID3D11Buffer* buffer)
   {
      MirrorsEdgeGameDeviceData::RegisteredConstants& entry = gd->mv_constants_copies[reinterpret_cast<uint64_t>(buffer)];
      D3D11_BUFFER_DESC desc;
      buffer->GetDesc(&desc);
      entry.size = desc.ByteWidth;
      if (gd->mv_filtered_buffer_count < MirrorsEdgeGameDeviceData::MAX_FILTERED_BUFFERS)
      {
         gd->mv_filtered_buffers[gd->mv_filtered_buffer_count++] = {reinterpret_cast<uint64_t>(buffer), &entry};
      }
      else
      {
         gd->mv_filter_overflow = true;
      }
      return &entry;
   }

   // A vc4 copy of "size" bytes from "bytes" (null: zeroed): one nobody held anymore at the last present, else a new one (a copy per
   // upload)
   static std::shared_ptr<std::vector<uint8_t>> NewConstantsCopy(MirrorsEdgeGameDeviceData* gd, const uint8_t* bytes, size_t size)
   {
      gd->mv_constants_made++;
      std::shared_ptr<std::vector<uint8_t>> copy;
      if (gd->mv_constants_pool_free.empty())
      {
         copy = std::make_shared<std::vector<uint8_t>>();
         gd->mv_constants_pool.push_back(copy);
      }
      else
      {
         copy = gd->mv_constants_pool[gd->mv_constants_pool_free.back()];
         gd->mv_constants_pool_free.pop_back();
      }
      if (bytes)
      {
         copy->assign(bytes, bytes + size);
      }
      else
      {
         copy->assign(size, 0);
      }
      return copy;
   }

   // Motion vectors: a registered vc4 buffer mapped for a whole rewrite, remembered until its Unmap
   static void OnMapBufferRegion(reshade::api::device* device, reshade::api::resource resource, uint64_t offset, uint64_t size, reshade::api::map_access access, void** data)
   {
      DeviceData* const device_data = device->get_private_data<DeviceData>();
      if (!device_data || !device_data->game || !GetGameDeviceData(*device_data).mv_active || !data || !*data)
         return;
      auto& gd = GetGameDeviceData(*device_data);
      MirrorsEdgeGameDeviceData::RegisteredConstants* const registered = FindRegisteredBuffer(&gd, resource.handle);
      if (!registered)
         return;
      if (access == reshade::api::map_access::write_discard && offset == 0)
      {
         registered->mapped = *data;
      }
#if DEVELOPMENT
      else
      {
         gd.mv_stats.other_maps++; // A partial or appending write: the copies would miss it
      }
#endif
   }

   // Motion vectors: the CPU copy of a vc4 buffer, before its Unmap (the game has written it). Reads the mapped memory back.
   static void OnUnmapBufferRegion(reshade::api::device* device, reshade::api::resource resource)
   {
      DeviceData* const device_data = device->get_private_data<DeviceData>();
      if (!device_data || !device_data->game || !GetGameDeviceData(*device_data).mv_active)
         return;
      auto& gd = GetGameDeviceData(*device_data);
      MirrorsEdgeGameDeviceData::RegisteredConstants* const registered = FindRegisteredBuffer(&gd, resource.handle);
      if (!registered || !registered->mapped)
         return;
      registered->copy = NewConstantsCopy(&gd, static_cast<const uint8_t*>(std::exchange(registered->mapped, nullptr)), registered->size);
#if DEVELOPMENT
      gd.mv_stats.maps++;
#endif
   }

   // Motion vectors: the CPU copy of a vc4 buffer from an UpdateSubresource (before it runs); a partial update is merged into the last
   // copy. Needs the unsigned "full add-on support" ReShade (the signed build doesn't raise the event).
   static bool OnUpdateBufferRegion(reshade::api::device* device, const void* data, reshade::api::resource resource, uint64_t offset, uint64_t size)
   {
      DeviceData* const device_data = device->get_private_data<DeviceData>();
      if (!device_data || !device_data->game || !data || !GetGameDeviceData(*device_data).mv_active)
         return false;
      auto& gd = GetGameDeviceData(*device_data);
      MirrorsEdgeGameDeviceData::RegisteredConstants* const registered = FindRegisteredBuffer(&gd, resource.handle);
      if (!registered || offset >= registered->size)
         return false;
      const size_t updated_size = size_t((std::min)(size, uint64_t(registered->size) - offset));
      // Merged into a copy of the last upload, else into zeros
      auto updated = NewConstantsCopy(&gd, ((registered->copy && registered->copy->size() == registered->size) ? registered->copy->data() : nullptr), registered->size);
      std::memcpy(updated->data() + offset, data, updated_size);
      registered->copy = std::move(updated);
#if DEVELOPMENT
      gd.mv_stats.updates++;
#endif
      return false;
   }

   // "bound_blend_state" and "bound_depth_stencil_state", from the game's binds on the immediate context (ReShade raises one per state,
   // and a reset binds none to every stage)
   static void OnBindPipeline(reshade::api::command_list* cmd_list, reshade::api::pipeline_stage stages, reshade::api::pipeline pipeline)
   {
      using reshade::api::pipeline_stage;
      if ((stages & (pipeline_stage::output_merger | pipeline_stage::depth_stencil)) == 0)
         return;
      DeviceData* const device_data = cmd_list->get_device()->get_private_data<DeviceData>();
      const CommandListData* const cmd_list_data = cmd_list->get_private_data<CommandListData>();
      if (!device_data || !device_data->game || !cmd_list_data || !cmd_list_data->is_primary)
         return;
      auto& gd = GetGameDeviceData(*device_data);
      if (stages == pipeline_stage::output_merger)
      {
         gd.bound_blend_state = reinterpret_cast<ID3D11BlendState*>(pipeline.handle);
      }
      else if (stages == pipeline_stage::depth_stencil)
      {
         gd.bound_depth_stencil_state = reinterpret_cast<ID3D11DepthStencilState*>(pipeline.handle);
      }
      else if (pipeline.handle == 0)
      {
         gd.bound_blend_state = nullptr;
         gd.bound_depth_stencil_state = nullptr;
      }
      else
      {
         gd.bound_states_tracked = false;
      }
   }

   // vc4's tracked binding (see "bound_object_buffer"); a game bind over the jitter slot (none seen) has it bound again
   static void OnPushConstantBuffers(reshade::api::command_list* cmd_list, reshade::api::shader_stage stages, reshade::api::pipeline_layout layout, uint32_t layout_param, const reshade::api::descriptor_table_update& update)
   {
      constexpr uint32_t slot = MotionVectorPatches::object_slot;
      const auto covers = [&](uint32_t binding)
      { return binding >= update.binding && binding < update.binding + update.count; };
      if ((stages & reshade::api::shader_stage::vertex) == 0 || update.type != reshade::api::descriptor_type::constant_buffer || (!covers(slot) && !covers(MotionVectorPatches::jitter_slot)))
         return;
      DeviceData* const device_data = cmd_list->get_device()->get_private_data<DeviceData>();
      const CommandListData* const cmd_list_data = cmd_list->get_private_data<CommandListData>();
      if (!device_data || !device_data->game || !cmd_list_data || !cmd_list_data->is_primary)
         return;
      auto& gd = GetGameDeviceData(*device_data);
      if (covers(MotionVectorPatches::jitter_slot))
      {
         gd.mv_jitter_bound = false;
      }
      if (covers(slot))
      {
         const auto* const ranges = static_cast<const reshade::api::buffer_range*>(update.descriptors);
         gd.bound_object_buffer = reinterpret_cast<ID3D11Buffer*>(ranges[slot - update.binding].buffer.handle);
      }
   }

   // Vertex buffer 0's tracked binding (see "bound_vertex_buffer")
   static void OnBindVertexBuffers(reshade::api::command_list* cmd_list, uint32_t first, uint32_t count, const reshade::api::resource* buffers, const uint64_t* offsets, const uint32_t* strides)
   {
      if (first != 0 || count == 0)
         return;
      DeviceData* const device_data = cmd_list->get_device()->get_private_data<DeviceData>();
      const CommandListData* const cmd_list_data = cmd_list->get_private_data<CommandListData>();
      if (!device_data || !device_data->game || !cmd_list_data || !cmd_list_data->is_primary)
         return;
      auto& gd = GetGameDeviceData(*device_data);
      gd.bound_vertex_buffer = reinterpret_cast<ID3D11Buffer*>(buffers[0].handle);
      gd.bound_vertex_offset = UINT(offsets[0]);
   }

   // The index buffer's tracked binding (see "bound_index_buffer")
   static void OnBindIndexBuffer(reshade::api::command_list* cmd_list, reshade::api::resource buffer, uint64_t offset, uint32_t index_size)
   {
      DeviceData* const device_data = cmd_list->get_device()->get_private_data<DeviceData>();
      const CommandListData* const cmd_list_data = cmd_list->get_private_data<CommandListData>();
      if (!device_data || !device_data->game || !cmd_list_data || !cmd_list_data->is_primary)
         return;
      auto& gd = GetGameDeviceData(*device_data);
      gd.bound_index_buffer = reinterpret_cast<ID3D11Buffer*>(buffer.handle);
      gd.bound_index_offset = UINT(offset);
   }

   // A bound object of the immediate context (not referenced: it lives while bound): "tracked" (see "bound_blend_state"), else what
   // "query" writes to its argument (an object pointer to take over)
   template <typename T, typename Query>
   static T* BoundState(const MirrorsEdgeGameDeviceData* gd, T* tracked, const Query& query)
   {
      if (gd->bound_states_tracked)
         return tracked;
      com_ptr<T> queried;
      query(&queried);
      return queried.get();
   }

   // Alpha to coverage for the masked materials, whose clip(OpacityMask - OpacityMaskClipValue) dgVoodoo translates as
   // "add rD, rS.c, l(-k, -k, -k, -k)" -> lt -> discard_nz (k 0.3333 or 0.25, baked per material; base and light passes alike). Their
   // scene alpha is the linear depth, so the coverage goes out through oMask rather than A2C. Right after the add: the sharpened
   // alpha c = saturate(d / fwidth(d) + 0.5) (Golus; the plain alpha would thin cut-outs out) and oMask = the first
   // n = floor(c * samples + 0.5) samples. The clip's value is then replaced by n - 0.5, so only an empty mask discards and coverage
   // ramps 0..1 across the vanilla edge (thin wires keep their weight). When something else reads that value the discard is left as
   // it was ("inward only": c >= 0.5 where a pixel is kept, edges soften inward). The same mask in a material's base and light passes
   // keeps the light off the samples the base pass left out. Empty, with "error", for any other shader (the light radius clips add to
   // a negated source, dissolve clips to l(-1)).
   static std::vector<uint8_t> PatchPixelShaderCoverage(const uint8_t* code, size_t size, std::string* error)
   {
      std::vector<DXBC::Chunk> chunks;
      std::vector<DXBC::SignatureElement> outputs;
      std::vector<uint32_t> tokens;
      std::vector<DXBC::Instruction> instructions;
      size_t first_body = 0;
      DXBC::Chunk* program = nullptr;
      DXBC::Chunk* output_signature = nullptr;
      if (DXBC::ReadChunks(code, size, &chunks))
      {
         program = DXBC::FindChunk(&chunks, DXBC::FourCC("SHEX"), DXBC::FourCC("SHDR"));
         output_signature = DXBC::FindChunk(&chunks, DXBC::FourCC("OSGN"));
      }
      if (!program || !output_signature || !DXBC::ReadSignature(output_signature->data, &outputs) || !DXBC::ReadProgram(*program, &tokens, &instructions, &first_body))
      {
         *error = "unreadable";
         return {};
      }
      // sampleinfo and oMask are SM 4.1 (dgVoodoo 2.81.3's ps_4_0 has neither)
      if (DECODE_D3D10_SB_TOKENIZED_PROGRAM_MAJOR_VERSION(tokens[0]) * 10 + DECODE_D3D10_SB_TOKENIZED_PROGRAM_MINOR_VERSION(tokens[0]) < 41)
      {
         *error = "no clip (below SM 4.1)";
         return {};
      }

      // Flow control nesting: 1 opens a block, -1 closes one
      const auto nesting = [](auto opcode)
      {
         switch (opcode)
         {
         case D3D10_SB_OPCODE_IF:
         case D3D10_SB_OPCODE_LOOP:
         case D3D10_SB_OPCODE_SWITCH:
            return 1;
         case D3D10_SB_OPCODE_ENDIF:
         case D3D10_SB_OPCODE_ENDLOOP:
         case D3D10_SB_OPCODE_ENDSWITCH:
            return -1;
         default:
            return 0;
         }
      };
      // The clip's add, outside flow control (the derivatives need all the quad's lanes): a temp destination, a temp source without a
      // modifier and the threshold as a replicated immediate
      size_t clip = SIZE_MAX;
      uint32_t clip_register = 0;
      uint32_t clip_component = 0;
      uint32_t clip_mask = 0; // Operand token form (x = D3D10_SB_OPERAND_4_COMPONENT_MASK_X)
      int depth = 0;
      for (size_t i = first_body; i < instructions.size() && clip == SIZE_MAX; i++)
      {
         const DXBC::Instruction& instruction = instructions[i];
         if (const int step = nesting(instruction.opcode); step != 0)
         {
            depth += step;
            continue;
         }
         const uint32_t* const t = &tokens[instruction.begin];
         if (depth != 0 || instruction.opcode != D3D10_SB_OPCODE_ADD || instruction.length != 10 || DECODE_IS_D3D10_SB_OPCODE_EXTENDED(t[0]))
            continue;
         // dgVoodoo's immediates carry an xyzw swizzle (0x00004E46), fxc's don't (0x00004002): the type and width only
         if (DECODE_D3D10_SB_OPERAND_TYPE(t[1]) != D3D10_SB_OPERAND_TYPE_TEMP || DECODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(t[1]) != D3D10_SB_OPERAND_4_COMPONENT_MASK_MODE || DECODE_D3D10_SB_OPERAND_TYPE(t[3]) != D3D10_SB_OPERAND_TYPE_TEMP || DECODE_IS_D3D10_SB_OPERAND_EXTENDED(t[3]) ||
             DECODE_D3D10_SB_OPERAND_TYPE(t[5]) != D3D10_SB_OPERAND_TYPE_IMMEDIATE32 || DECODE_D3D10_SB_OPERAND_NUM_COMPONENTS(t[5]) != D3D10_SB_OPERAND_4_COMPONENT)
            continue;
         const float threshold = -std::bit_cast<float>(t[6]);
         if (!(threshold > 0.f && threshold < 1.f) || t[7] != t[6] || t[8] != t[6] || t[9] != t[6])
            continue;
         clip = i;
         clip_register = t[2];
         clip_mask = DECODE_D3D10_SB_OPERAND_4_COMPONENT_MASK(t[1]);
         clip_component = std::countr_zero(clip_mask >> 4);
      }
      if (clip == SIZE_MAX)
      {
         *error = "no clip";
         return {};
      }
      // The add must reach a discard's condition (data flow through temp components): other subtractions of a constant are no clip.
      // "symmetric": nothing else reads what the clip computed (no output written from it before the discard, no read of it after),
      // so the patch can replace it with the coverage's own test (see below).
      bool symmetric = true;
      {
         // Components (bits xyzw) of each temp holding the clip's value or values computed from it
         std::unordered_map<uint32_t, uint8_t> tainted = {{clip_register, uint8_t(clip_mask >> 4)}};
         bool reaches = false;
         depth = 0;
         for (size_t i = clip + 1; i < instructions.size() && symmetric; i++)
         {
            const DXBC::Instruction& instruction = instructions[i];
            depth += nesting(instruction.opcode);
            // The first operand is the destination, except for these; two destinations (sincos, udiv...) aren't followed
            bool has_destination = true;
            bool two_destinations = false;
            switch (instruction.opcode)
            {
            case D3D10_SB_OPCODE_IF:
            case D3D10_SB_OPCODE_BREAKC:
            case D3D10_SB_OPCODE_CONTINUEC:
            case D3D10_SB_OPCODE_RETC:
            case D3D10_SB_OPCODE_CALLC:
            case D3D10_SB_OPCODE_DISCARD:
            case D3D10_SB_OPCODE_SWITCH:
            case D3D10_SB_OPCODE_CASE:
               has_destination = false;
               break;
            case D3D10_SB_OPCODE_SINCOS:
            case D3D10_SB_OPCODE_UDIV:
            case D3D10_SB_OPCODE_UMUL:
            case D3D10_SB_OPCODE_IMUL:
            case D3D11_SB_OPCODE_SWAPC:
            case D3D11_SB_OPCODE_UADDC:
            case D3D11_SB_OPCODE_USUBB:
               two_destinations = true;
               break;
            default:
               break;
            }
            bool reads = false;
            bool first_operand = true;
            bool temp_destination = false;
            uint32_t written_register = 0;
            uint8_t written_mask = 0;
            const bool walked = DXBC::WalkOperands(tokens, instruction, [&](size_t token_position, size_t index_position)
               {
                  const uint32_t token = tokens[token_position];
                  const D3D10_SB_OPERAND_TYPE type = DECODE_D3D10_SB_OPERAND_TYPE(token);
                  const auto mode = DECODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(token);
                  if (std::exchange(first_operand, false) && has_destination)
                  {
                     // Any destination's mask (outputs too, for the swizzle positions below); a 1 component one (oMask, oDepth) is x
                     const bool masked = DECODE_D3D10_SB_OPERAND_NUM_COMPONENTS(token) == D3D10_SB_OPERAND_4_COMPONENT && mode == D3D10_SB_OPERAND_4_COMPONENT_MASK_MODE;
                     written_mask = (masked ? uint8_t(DECODE_D3D10_SB_OPERAND_4_COMPONENT_MASK(token) >> 4) : uint8_t(0x1));
                     temp_destination = type == D3D10_SB_OPERAND_TYPE_TEMP && index_position != DXBC::no_index && masked;
                     if (temp_destination)
                     {
                        written_register = tokens[index_position];
                     }
                     return true;
                  }
                  if (type != D3D10_SB_OPERAND_TYPE_TEMP || index_position == DXBC::no_index)
                     return true;
                  const auto found = tainted.find(tokens[index_position]);
                  if (found == tainted.end())
                     return true;
                  uint8_t components = 0;
                  if (mode == D3D10_SB_OPERAND_4_COMPONENT_SWIZZLE_MODE)
                  {
                     // The swizzle positions the instruction uses: the dot products' first 2 to 4, a component wise operation's
                     // written ones, all of them otherwise (texture operations, no destination)
                     uint8_t positions = 0xF;
                     switch (instruction.opcode)
                     {
                     case D3D10_SB_OPCODE_DP2:
                        positions = 0x3;
                        break;
                     case D3D10_SB_OPCODE_DP3:
                        positions = 0x7;
                        break;
                     case D3D10_SB_OPCODE_DP4:
                     case D3D10_SB_OPCODE_SAMPLE:
                     case D3D10_SB_OPCODE_SAMPLE_L:
                     case D3D10_SB_OPCODE_SAMPLE_D:
                     case D3D10_SB_OPCODE_SAMPLE_B:
                     case D3D10_SB_OPCODE_SAMPLE_C:
                     case D3D10_SB_OPCODE_SAMPLE_C_LZ:
                     case D3D10_SB_OPCODE_LD:
                     case D3D10_SB_OPCODE_LD_MS:
                     case D3D10_SB_OPCODE_RESINFO:
                     case D3D10_1_SB_OPCODE_LOD:
                     case D3D10_1_SB_OPCODE_GATHER4:
                     case D3D10_1_SB_OPCODE_SAMPLE_POS:
                     case D3D10_1_SB_OPCODE_SAMPLE_INFO:
                     case D3D11_SB_OPCODE_GATHER4_C:
                     case D3D11_SB_OPCODE_GATHER4_PO:
                     case D3D11_SB_OPCODE_GATHER4_PO_C:
                     case D3D11_SB_OPCODE_LD_UAV_TYPED:
                     case D3D11_SB_OPCODE_LD_RAW:
                     case D3D11_SB_OPCODE_LD_STRUCTURED:
                     case D3D11_SB_OPCODE_BUFINFO:
                        break;
                     default:
                        if (has_destination && !two_destinations)
                        {
                           positions = written_mask;
                        }
                        break;
                     }
                     for (uint32_t component = 0; component < 4; component++)
                     {
                        if (positions & (1u << component))
                        {
                           components |= uint8_t(1u << DECODE_D3D10_SB_OPERAND_4_COMPONENT_SWIZZLE_SOURCE(token, component));
                        }
                     }
                  }
                  else if (mode == D3D10_SB_OPERAND_4_COMPONENT_SELECT_1_MODE)
                  {
                     components = uint8_t(1u << DECODE_D3D10_SB_OPERAND_4_COMPONENT_SELECT_1(token));
                  }
                  else
                  {
                     components = 0xF;
                  }
                  reads |= (found->second & components) != 0;
                  return true; });
            if (!walked)
            {
               symmetric = false;
               break;
            }
            if (reaches)
            {
               // Read after the discard: the patch must keep the value
               if (reads)
               {
                  symmetric = false;
                  break;
               }
            }
            else
            {
               if (instruction.opcode == D3D10_SB_OPCODE_DISCARD && reads)
               {
                  reaches = true;
                  continue;
               }
               // Up to the discard the clip's value may flow through temps, not out of the shader
               if (reads)
               {
                  if (two_destinations || !temp_destination)
                  {
                     symmetric = false;
                     break;
                  }
                  tainted[written_register] |= written_mask;
                  continue;
               }
            }
            // Overwritten (only where it surely runs)
            if (temp_destination && !two_destinations && depth == 0)
            {
               if (const auto found = tainted.find(written_register); found != tainted.end())
               {
                  found->second &= uint8_t(~written_mask);
               }
            }
         }
         // When the analysis stopped early, a discard after the clip still has to be there for the inward only coverage
         if (!reaches && (symmetric || std::ranges::none_of(instructions.begin() + clip + 1, instructions.end(), [](const DXBC::Instruction& instruction)
                                          { return instruction.opcode == D3D10_SB_OPCODE_DISCARD; })))
         {
            *error = "no clip (no discard on it)";
            return {};
         }
      }
      if (std::ranges::any_of(outputs, [](const DXBC::SignatureElement& element)
             { return element.name == "SV_Coverage"; }))
      {
         *error = "writes oMask";
         return {};
      }
      const size_t last_output = DXBC::FindLastDeclaration(instructions, first_body, {D3D10_SB_OPCODE_DCL_OUTPUT, D3D10_SB_OPCODE_DCL_OUTPUT_SGV, D3D10_SB_OPCODE_DCL_OUTPUT_SIV});
      if (last_output == first_body)
      {
         *error = "no outputs";
         return {};
      }

      // dcl_output oMask, as fxc encodes it, after the other outputs
      constexpr uint32_t omask = ENCODE_D3D10_SB_OPERAND_TYPE(D3D10_SB_OPERAND_TYPE_OUTPUT_COVERAGE_MASK);
      uint32_t temp = 0;
      std::vector<uint32_t> patched = DXBC::CopyDeclarations(tokens, instructions, first_body, 1, &temp, [&](size_t i)
         { return i == last_output ? std::vector<uint32_t>{ENCODE_D3D10_SB_OPCODE_TYPE(D3D10_SB_OPCODE_DCL_OUTPUT) | ENCODE_D3D10_SB_TOKENIZED_INSTRUCTION_LENGTH(2), omask} : std::vector<uint32_t>{}; });
      constexpr uint32_t immediate_1 = ENCODE_D3D10_SB_OPERAND_NUM_COMPONENTS(D3D10_SB_OPERAND_1_COMPONENT) | ENCODE_D3D10_SB_OPERAND_TYPE(D3D10_SB_OPERAND_TYPE_IMMEDIATE32);
      constexpr uint32_t x_dst = DXBC::Destination(D3D10_SB_OPERAND_TYPE_TEMP, D3D10_SB_OPERAND_4_COMPONENT_MASK_X);
      constexpr uint32_t y_dst = DXBC::Destination(D3D10_SB_OPERAND_TYPE_TEMP, D3D10_SB_OPERAND_4_COMPONENT_MASK_Y);
      const uint32_t t_x = DXBC::Source(D3D10_SB_OPERAND_TYPE_TEMP, 0, 0, 0, 0);
      const uint32_t t_y = DXBC::Source(D3D10_SB_OPERAND_TYPE_TEMP, 1, 1, 1, 1);
      const uint32_t d = DXBC::Source(D3D10_SB_OPERAND_TYPE_TEMP, clip_component, clip_component, clip_component, clip_component);
      constexpr uint32_t abs_modifier = ENCODE_D3D10_SB_EXTENDED_OPERAND_TYPE(D3D10_SB_EXTENDED_OPERAND_MODIFIER) | ENCODE_D3D10_SB_EXTENDED_OPERAND_MODIFIER(D3D10_SB_OPERAND_MODIFIER_ABS);
      // sampleinfo's float form: the render target's sample count
      constexpr uint32_t rasterizer_x = ENCODE_D3D10_SB_OPERAND_NUM_COMPONENTS(D3D10_SB_OPERAND_4_COMPONENT) | ENCODE_D3D10_SB_OPERAND_TYPE(D3D10_SB_OPERAND_TYPE_RASTERIZER) | ENCODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(D3D10_SB_OPERAND_4_COMPONENT_SELECT_1_MODE);
      static_assert(omask == 0x0000F000 && rasterizer_x == 0x0000E00A); // fxc's "dcl_output oMask" and "sampleinfo r1.y, rasterizer.x"
      const auto opcode = [](D3D10_SB_OPCODE_TYPE type, uint32_t length, bool saturate = false)
      { return ENCODE_D3D10_SB_OPCODE_TYPE(type) | ENCODE_D3D10_SB_TOKENIZED_INSTRUCTION_LENGTH(length) | ENCODE_D3D10_SB_INSTRUCTION_SATURATE(saturate); };
      std::vector<uint32_t> coverage = {
         // deriv_rtx t.x, d | deriv_rty t.y, d | add t.x, |t.x|, |t.y| | max t.x, t.x, l(0.0001)
         opcode(D3D10_SB_OPCODE_DERIV_RTX, 5), x_dst, temp, d, clip_register,
         opcode(D3D10_SB_OPCODE_DERIV_RTY, 5), y_dst, temp, d, clip_register,
         opcode(D3D10_SB_OPCODE_ADD, 9), x_dst, temp, t_x | ENCODE_D3D10_SB_OPERAND_EXTENDED(1), abs_modifier, temp, t_y | ENCODE_D3D10_SB_OPERAND_EXTENDED(1), abs_modifier, temp,
         opcode(D3D10_SB_OPCODE_MAX, 7), x_dst, temp, t_x, temp, immediate_1, std::bit_cast<uint32_t>(0.0001f),
         // div t.x, d, t.x | add_sat t.x, t.x, l(0.5): the sharpened alpha
         opcode(D3D10_SB_OPCODE_DIV, 7), x_dst, temp, d, clip_register, t_x, temp,
         opcode(D3D10_SB_OPCODE_ADD, 7, true), x_dst, temp, t_x, temp, immediate_1, std::bit_cast<uint32_t>(0.5f),
         // sampleinfo t.y, rasterizer.x | mad t.x, t.x, t.y, l(0.5) | ftou t.x, t.x: the covered sample count, rounded
         opcode(D3D10_1_SB_OPCODE_SAMPLE_INFO, 4), y_dst, temp, rasterizer_x,
         opcode(D3D10_SB_OPCODE_MAD, 9), x_dst, temp, t_x, temp, t_y, temp, immediate_1, std::bit_cast<uint32_t>(0.5f),
         opcode(D3D10_SB_OPCODE_FTOU, 5), x_dst, temp, t_x, temp};
      if (symmetric)
      {
         // utof t.y, t.x | add rD.mask, t.y, l(-0.5): the clip now discards only an empty mask, so coverage ramps 0..1 across the
         // vanilla edge (the alpha test's line at half coverage) instead of 0.5..1 inside it
         coverage.insert(coverage.end(), {opcode(D3D10_SB_OPCODE_UTOF, 5), y_dst, temp, t_x, temp,
                                            opcode(D3D10_SB_OPCODE_ADD, 7), DXBC::Destination(D3D10_SB_OPERAND_TYPE_TEMP, clip_mask), clip_register, t_y, temp, immediate_1, std::bit_cast<uint32_t>(-0.5f)});
      }
      // ishl t.x, l(1), t.x | iadd oMask, t.x, l(-1)
      coverage.insert(coverage.end(), {opcode(D3D10_SB_OPCODE_ISHL, 7), x_dst, temp, immediate_1, 1, t_x, temp,
                                         opcode(D3D10_SB_OPCODE_IADD, 6), omask | ENCODE_D3D10_SB_OPERAND_NUM_COMPONENTS(D3D10_SB_OPERAND_1_COMPONENT), t_x, temp, immediate_1, 0xFFFFFFFF});
      for (size_t i = first_body; i < instructions.size(); i++)
      {
         patched.insert(patched.end(), tokens.begin() + instructions[i].begin, tokens.begin() + instructions[i].begin + instructions[i].length);
         if (i == clip)
         {
            patched.insert(patched.end(), coverage.begin(), coverage.end());
         }
      }
      DXBC::WriteProgram(&patched, program);
      // fxc's element for it: uint, no register, x written
      outputs.push_back({.name = "SV_Coverage", .semantic_index = 0, .system_value = 0, .component_type = 1, .reg = 0xFFFFFFFF, .mask = 0x1, .rw_mask = 0xE});
      output_signature->data = DXBC::WriteSignature(outputs);
      *error = (symmetric ? "" : "inward only (the clip's value is read elsewhere)");
      return DXBC::WriteChunks(chunks);
   }

   // The bound shader's patched version ("patch" for pixel shaders, see above), patched from Core's bytecode copy on first use (null if
   // it can't be, e.g. a vertex shader that doesn't place vertices with the view projection). The maps' nodes are never erased.
   template <typename T>
   static const MirrorsEdgeGameDeviceData::PatchedShader<T>& GetPatchedShader(ID3D11Device* native_device, DeviceData& device_data, uint32_t hash, reshade::api::pipeline pipeline,
      PixelShaderPatch patch = PixelShaderPatch::MOTION_VECTORS)
   {
      constexpr bool vertex = std::is_same_v<T, ID3D11VertexShader>;
      auto& gd = GetGameDeviceData(device_data);
      auto* const shaders = [&]
      {
         if constexpr (vertex)
            return &gd.mv_vertex_shaders;
         else
            return &gd.patched_pixel_shaders[size_t(patch)];
      }();
      {
         const std::shared_lock lock(gd.mv_mutex);
         if (const auto it = shaders->find(hash); it != shaders->end())
            return it->second;
      }
      std::vector<uint8_t> patched;
      std::string error; // Written by a refusal only
      UINT read_size = 0;
      UINT translation_offset = TRANSLATION_OFFSET;
      {
         const std::shared_lock lock(s_mutex_generic);
         if (const auto it = device_data.pipeline_cache_by_pipeline_handle.find(pipeline.handle); it != device_data.pipeline_cache_by_pipeline_handle.end() && it->second->subobjects_cache)
         {
            const auto* desc = static_cast<const reshade::api::shader_desc*>(it->second->subobjects_cache[0].data);
            const auto* code = static_cast<const uint8_t*>(desc->code);
            if constexpr (vertex)
            {
               patched = MotionVectorPatch::PatchVertexShader(code, desc->code_size, MotionVectorPatches::layout, &error);
               read_size = DXBC::ConstantBufferBytes(code, desc->code_size, MotionVectorPatches::object_slot);
               if (DXBC::ReadsConstantRow(code, desc->code_size, MotionVectorPatches::object_slot, SKINNED_TRANSLATION_OFFSET / 16))
               {
                  translation_offset = SKINNED_TRANSLATION_OFFSET;
               }
            }
            else
            {
               switch (patch)
               {
               case PixelShaderPatch::MOTION_VECTORS:
                  patched = MotionVectorPatch::PatchPixelShader(code, desc->code_size, MotionVectorPatches::layout, &error, /* targets_only */ true);
                  break;
               case PixelShaderPatch::MASK_ALPHA:
               case PixelShaderPatch::MASK_ADDITIVE:
                  patched = MotionVectorPatch::PatchPixelShaderReactive(code, desc->code_size, MotionVectorPatches::layout, MotionVectorPatches::reactive_slot,
                     (patch == PixelShaderPatch::MASK_ADDITIVE ? MotionVectorPatch::ReactiveMode::ADDITIVE : MotionVectorPatch::ReactiveMode::ALPHA), &error);
                  break;
               case PixelShaderPatch::BLENDED:
                  patched = MotionVectorPatch::PatchPixelShaderReactive(code, desc->code_size, MotionVectorPatches::layout, UINT32_MAX, MotionVectorPatch::ReactiveMode::ALPHA_MOTION_VECTORS, &error);
                  break;
               case PixelShaderPatch::COVERAGE:
                  patched = PatchPixelShaderCoverage(code, desc->code_size, &error);
                  break;
               }
            }
         }
         else
         {
            error = "no bytecode";
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
      // Failures in every build (bug reports), except the expected refusals (screen space; every shader without a clip offered to the
      // coverage patch, logged in development only when patched); every patched shader only in development
      const bool expected = error.starts_with("screen space") || error.starts_with("no clip");
      if ((DEVELOPMENT && (shader || patch != PixelShaderPatch::COVERAGE)) || (!shader && !expected))
      {
         // A patched shader's note (the coverage's "inward only") follows "patched"
         std::string status = error;
         if (shader)
         {
            status = (error.empty() ? std::string("patched") : "patched, " + error);
         }
         reshade::log::message(((shader || expected) ? reshade::log::level::info : reshade::log::level::warning),
            std::format("[Mirror's Edge] {} 0x{:08X} {}", (vertex ? "MV VS" : PIXEL_SHADER_PATCH_NAMES[size_t(patch)]), hash, status).c_str());
      }
      const std::unique_lock lock(gd.mv_mutex);
      const auto [entry, inserted] = shaders->try_emplace(hash, MirrorsEdgeGameDeviceData::PatchedShader<T>{.shader = shader, .read_size = read_size, .translation_offset = translation_offset});
      return entry->second;
   }

   // The bound vertex shader's patched version (null shader if refused), looked up again only when the game's shader changes
   static const MirrorsEdgeGameDeviceData::PatchedShader<ID3D11VertexShader>& GetPatchedVertexShader(ID3D11Device* native_device, const CommandListData& cmd_list_data, DeviceData& device_data, uint32_t hash)
   {
      auto& gd = GetGameDeviceData(device_data);
      if (hash != gd.mv_last_vertex_shader_hash)
      {
         gd.mv_last_vertex_shader = GetPatchedShader<ID3D11VertexShader>(native_device, device_data, hash, cmd_list_data.pipeline_state_original_vertex_shader);
         gd.mv_last_vertex_shader_hash = hash;
      }
      return gd.mv_last_vertex_shader;
   }

   // Classifies the bound blend state (cached in "mv_blend_state"): "mv_blend_opaque" for the motion vectors (additive lights, decals
   // and translucents keep the motion vectors of what's behind them, and so do colorless draws: the occlusion query bounding boxes), and
   // "mv_reactive_blend": 0 not alpha blended (opaque, lights ONE/ONE, shadows DEST_COLOR), 1 alpha blended (SRC_ALPHA / INV_SRC_ALPHA:
   // smoke, glass, water; reactive and transparency & composition), 2 additive (SRC_ALPHA / ONE: sparks, glows; reactive)
   static void ClassifyBoundBlend(ID3D11DeviceContext* native_device_context, MirrorsEdgeGameDeviceData* gd)
   {
      ID3D11BlendState* const blend_state = BoundState(gd, gd->bound_blend_state, [&](ID3D11BlendState** state)
         { native_device_context->OMGetBlendState(state, nullptr, nullptr); });
      if (blend_state == gd->mv_blend_state)
         return;
      D3D11_BLEND_DESC blend_desc = CD3D11_BLEND_DESC(D3D11_DEFAULT);
      if (blend_state)
      {
         blend_state->GetDesc(&blend_desc);
      }
      const D3D11_RENDER_TARGET_BLEND_DESC& rt0 = blend_desc.RenderTarget[0];
      gd->mv_blend_opaque = rt0.RenderTargetWriteMask != 0 && (!rt0.BlendEnable || (rt0.SrcBlend == D3D11_BLEND_ONE && rt0.DestBlend == D3D11_BLEND_ZERO && rt0.BlendOp == D3D11_BLEND_OP_ADD));
      gd->mv_reactive_blend = ((!rt0.BlendEnable || rt0.SrcBlend != D3D11_BLEND_SRC_ALPHA) ? 0 : (rt0.DestBlend == D3D11_BLEND_ONE ? 2 : 1));
      // Exactly the states whose motion vector target "PatchedDraws::OnCreateBlendState" blends ("blend_alpha_blended_target")
      gd->mv_alpha_blended = rt0.BlendEnable && rt0.SrcBlend == D3D11_BLEND_SRC_ALPHA && rt0.DestBlend == D3D11_BLEND_INV_SRC_ALPHA && rt0.BlendOp == D3D11_BLEND_OP_ADD;
      gd->mv_blend_state = blend_state;
   }

   // The bound depth stencil state tests depth (meshes do; full screen passes and composites don't), cached by state
   static bool IsDepthTested(ID3D11DeviceContext* native_device_context, MirrorsEdgeGameDeviceData* gd)
   {
      ID3D11DepthStencilState* const depth_stencil_state = BoundState(gd, gd->bound_depth_stencil_state, [&](ID3D11DepthStencilState** state)
         { native_device_context->OMGetDepthStencilState(state, nullptr); });
      if (depth_stencil_state != gd->jitter_depth_stencil_state)
      {
         D3D11_DEPTH_STENCIL_DESC depth_desc = CD3D11_DEPTH_STENCIL_DESC(D3D11_DEFAULT);
         if (depth_stencil_state)
         {
            depth_stencil_state->GetDesc(&depth_desc);
         }
         gd->jitter_depth_test = depth_desc.DepthEnable;
         gd->jitter_depth_stencil_state = depth_stencil_state;
      }
      return gd->jitter_depth_test;
   }

   // Opens the scene at the frame's first mesh draw into output sized depth (the menu's 1280x720 scene never opens it): takes the scene
   // depth and picks the jitter the whole scene draws with. Once per present (the HUD and later passes never reopen it).
   static void OpenScene(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, const CommandListData& cmd_list_data, DeviceData& device_data, uint32_t vertex_shader_hash, ID3D11DepthStencilView* dsv)
   {
      auto& gd = GetGameDeviceData(device_data);
      uint4 depth_size;
      DXGI_FORMAT unused_format;
      GetResourceInfo(dsv, depth_size, unused_format);
      if (depth_size.x != device_data.output_resolution.x || depth_size.y != device_data.output_resolution.y)
      {
         gd.open_refused_dsv = dsv;
         return;
      }
      // The menu draws its scene twice: first into a 1280x720 viewport of the same output sized targets (then GammaCorrection), then
      // at full size. Only the full size one is the scene (NOTES.md).
      UINT viewport_count = 1;
      D3D11_VIEWPORT viewport = {};
      native_device_context->RSGetViewports(&viewport_count, &viewport);
      if (viewport_count == 0 || viewport.Width != device_data.output_resolution.x || viewport.Height != device_data.output_resolution.y || !GetPatchedVertexShader(native_device, cmd_list_data, device_data, vertex_shader_hash).shader)
         return;
      gd.mv_scene_open = true;
      gd.mv_depth.reset();
      dsv->GetResource(&gd.mv_depth);
      gd.mv_scene_color.reset();
      gd.mv_scene_rtv.reset();
      gd.jitter_dsv = nullptr;
      gd.jitter_depth_stencil_state = nullptr;
      gd.jitter_depth_test = true;
      gd.mv_accepted_dsv = nullptr;
      gd.mv_refused_rtv = nullptr;
      gd.mv_refused_dsv = nullptr;
      gd.mv_jitter_bound = false;
      gd.mv_blend_state = nullptr;
      gd.mv_blend_opaque = true;
      gd.mv_alpha_blended = false;
      gd.mv_reactive_blend = 0;
      // Halton (2, 3) over the upscaler's phase count; pixels to NDC (y up). None until the upscaler is ready (the bridge's helper
      // starting shows the scene as it is, antialiased with SMAA).
      const SR::InstanceData* sr_instance_data = (gd.sr_active ? device_data.GetSRInstanceData() : nullptr);
      if (sr_instance_data && !sr_implementations[device_data.sr_type]->IsReady(sr_instance_data))
      {
         sr_instance_data = nullptr;
      }
      const unsigned int phase = cb_luma_global_settings.FrameIndex % (sr_instance_data ? (std::max)(sr_implementations[device_data.sr_type]->GetJitterPhases(sr_instance_data), 1) : SR::GetDefaultJitterPhases());
      gd.mv_jitter = (((sr_instance_data && gd.mv_jitter_allowed) || g_mv_force_jitter) ? std::array<float, 2>{SR::HaltonSequence(phase, 2), SR::HaltonSequence(phase, 3)} : std::array<float, 2>{});
      // SMAA T2x: the sample offsets (0.25, 0.25) then (-0.25, -0.25) in y down pixels, for "SMAA.hlsl"'s T2x subsample indices 1 and
      // 2. That is its table read with y up, which measured better (docs/SMAA-Lab.md). The projection moves the other way.
      gd.t2x_phase = ((gd.t2x_active && gd.t2x_frames[0] && gd.t2x_frame + 1 == cb_luma_global_settings.FrameIndex) ? int(cb_luma_global_settings.FrameIndex & 1) : -1);
      if (gd.t2x_phase >= 0)
      {
         const float shift = (gd.t2x_phase == 0 ? -0.25f : 0.25f);
         gd.mv_jitter = {shift, shift};
      }
      gd.mv_jitter_ndc = {gd.mv_jitter[0] * 2.f / device_data.output_resolution.x, gd.mv_jitter[1] * -2.f / device_data.output_resolution.y};
      const float ndc_jitter[4] = {gd.mv_jitter_ndc[0], gd.mv_jitter_ndc[1], 0.f, 0.f};
      if (!PatchedDraws::WriteDynamicConstants(native_device, native_device_context, std::addressof(gd.mv_jitter_buffer), ndc_jitter, sizeof(ndc_jitter)))
      {
         // No stale jitter on the scene draws either: no motion vectors this frame
         gd.mv_jitter = {};
         gd.mv_jitter_ndc = {};
         gd.mv_jitter_buffer.reset();
         gd.t2x_phase = -1;
      }
   }

#if DEVELOPMENT
#define MV_REJECT(reason) \
   ([&](auto& gd) { gd.mv_stats.rejected[reason]++; gd.mv_draw_reject = reason; return false; }(GetGameDeviceData(device_data)))
#else
#define MV_REJECT(reason) false
#endif

   // The jitter buffer, bound once per scene (no translated shader reads b9, Luma draws nothing during the scene; a game bind over it
   // resets "mv_jitter_bound", see "OnPushConstantBuffers")
   static void BindJitter(ID3D11DeviceContext* native_device_context, MirrorsEdgeGameDeviceData* gd)
   {
      if (std::exchange(gd->mv_jitter_bound, true))
         return;
      ID3D11Buffer* const jitter = gd->mv_jitter_buffer.get();
      native_device_context->VSSetConstantBuffers(MotionVectorPatches::jitter_slot, 1, &jitter);
   }

   // Draws an opaque draw into the fp16 scene (the scene target alone, output sized, with the scene depth) with the patched shaders,
   // adding the motion vector target ("target_slot", past the game's) and the previous frame's vc4 ("previous_slots"). False if it
   // can't (the draw then goes to "DrawWithJitter").
   static bool DrawWithMotionVectors(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, const CommandListData& cmd_list_data, DeviceData& device_data,
      const ShaderHashesList<OneShaderPerPipeline>& original_shader_hashes, const std::function<void()>& draw, const com_ptr<ID3D11RenderTargetView> (&rtvs)[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT], ID3D11DepthStencilView* dsv)
   {
      auto& gd = GetGameDeviceData(device_data);
      // The scene target alone, plus the motion vector and mask targets the last patched draws left bound
      for (UINT slot = 1; slot < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; slot++)
      {
         if (rtvs[slot] && (slot != MotionVectorPatches::target_slot || rtvs[slot] != gd.mv_rtv) && (slot != MotionVectorPatches::reactive_slot || rtvs[slot] != gd.mv_reactive_target_rtv))
            return MV_REJECT(REJECT_EXTRA_TARGET);
      }
      if (!rtvs[0] || !dsv || !gd.mv_scene_open)
         return MV_REJECT(REJECT_NO_SCENE);
      if (!IsDepthTested(native_device_context, &gd))
         return MV_REJECT(REJECT_DEPTH_TEST);
      ClassifyBoundBlend(native_device_context, &gd);
      // Alpha blended draws (SRC_ALPHA / INV_SRC_ALPHA) take their own motion vectors too, blended by their alpha: the chain link
      // fences are such a draw, their wires opaque (see "PatchPixelShaderReactive"'s "motion_vectors"). Additive ones and the other
      // blends keep what's behind them.
      const bool blended = gd.mv_alpha_blended;
      if (!gd.mv_blend_opaque && !blended)
         return MV_REJECT(REJECT_BLEND);
      // Known targets: checked, and the motion vector target built for them (the depth prepass's R8G8B8A8 dummy target is refused here)
      if (rtvs[0] != gd.mv_scene_rtv || dsv != gd.mv_accepted_dsv)
      {
         if (rtvs[0].get() == gd.mv_refused_rtv && dsv == gd.mv_refused_dsv)
            return MV_REJECT(gd.mv_refused_reason);
         const auto refuse = [&](MotionVectorReject reason)
         {
            gd.mv_refused_rtv = rtvs[0].get();
            gd.mv_refused_dsv = dsv;
            gd.mv_refused_reason = reason;
            return MV_REJECT(reason);
         };
         com_ptr<ID3D11Resource> depth;
         dsv->GetResource(&depth);
         com_ptr<ID3D11Resource> color;
         rtvs[0]->GetResource(&color);
         if (depth != gd.mv_depth || !color || (gd.mv_scene_color && color != gd.mv_scene_color))
            return refuse(REJECT_OTHER_DEPTH_COLOR);
         D3D11_RENDER_TARGET_VIEW_DESC rtv_desc;
         rtvs[0]->GetDesc(&rtv_desc);
         com_ptr<ID3D11Texture2D> color_texture;
         D3D11_TEXTURE2D_DESC color_desc = {};
         if (SUCCEEDED(color->QueryInterface(&color_texture)))
         {
            color_texture->GetDesc(&color_desc);
         }
         // The fill reads the scene: it needs a shader resource view. Filtered by the resource, not the view (dgVoodoo binds single-slice
         // array views). A multisampled scene is refused here only on MSAA's first frame ("msaa_active" then stops this path).
         if (rtv_desc.Format != DXGI_FORMAT_R16G16B16A16_FLOAT || color_desc.ArraySize != 1 || color_desc.SampleDesc.Count != 1 || (color_desc.BindFlags & D3D11_BIND_SHADER_RESOURCE) == 0)
            return refuse(REJECT_FORMAT);
         const uint2 size = {color_desc.Width, color_desc.Height};
         if (size.x != device_data.output_resolution.x || size.y != device_data.output_resolution.y)
            return refuse(REJECT_SIZE);
         const std::unique_lock lock(gd.mv_mutex);
         D3D11_TEXTURE2D_DESC desc = {};
         if (gd.mv_texture)
         {
            gd.mv_texture->GetDesc(&desc);
         }
         // R16G16_FLOAT: FSR keeps 16 bits internally; the error is under 0.1% of the motion (BL GOTY)
         constexpr DXGI_FORMAT format = DXGI_FORMAT_R16G16_FLOAT;
         if (desc.Width != size.x || desc.Height != size.y)
         {
            gd.ReleaseMotionVectorTargets();
            // The fill reads the target back through its UAV
            D3D11_FEATURE_DATA_FORMAT_SUPPORT2 support = {format};
            const bool typed_uav_load = SUCCEEDED(native_device->CheckFeatureSupport(D3D11_FEATURE_FORMAT_SUPPORT2, &support, sizeof(support))) && (support.OutFormatSupport2 & D3D11_FORMAT_SUPPORT2_UAV_TYPED_LOAD) != 0;
            desc = CD3D11_TEXTURE2D_DESC(format, size.x, size.y, 1, 1, D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | (typed_uav_load ? D3D11_BIND_UNORDERED_ACCESS : 0u));
            if (FAILED(SRBridge::CreateSharableTexture(native_device, desc, &gd.mv_texture)) || FAILED(native_device->CreateRenderTargetView(gd.mv_texture.get(), nullptr, &gd.mv_rtv)))
            {
               gd.mv_texture.reset();
               gd.mv_rtv.reset();
               return MV_REJECT(REJECT_CREATE);
            }
            if (typed_uav_load)
            {
               native_device->CreateUnorderedAccessView(gd.mv_texture.get(), nullptr, &gd.mv_uav);
            }
            native_device->CreateShaderResourceView(gd.mv_texture.get(), nullptr, &gd.mv_srv);
            const CD3D11_TEXTURE2D_DESC depth_desc(DXGI_FORMAT_R32_FLOAT, size.x, size.y, 1, 1, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS);
            if (SUCCEEDED(SRBridge::CreateSharableTexture(native_device, depth_desc, &gd.mv_device_depth)))
            {
               native_device->CreateUnorderedAccessView(gd.mv_device_depth.get(), nullptr, &gd.mv_device_depth_uav);
            }
            gd.mv_frame_ended = true;
         }
         gd.mv_scene_color = color;
         gd.mv_scene_rtv = rtvs[0];
         gd.mv_accepted_dsv = dsv;
      }

      ID3D11VertexShader* const vertex_shader = GetPatchedVertexShader(native_device, cmd_list_data, device_data, original_shader_hashes.vertex_shaders[0]).shader.get();
      // The alpha blended draws write no masks: they write their own motion vectors, and FSR's reactive and transparency & composition
      // masks are for content without them (AMD's FSR manual)
      ID3D11PixelShader* pixel_shader = nullptr;
      if (blended)
      {
         pixel_shader = GetPatchedShader<ID3D11PixelShader>(native_device, device_data, original_shader_hashes.pixel_shaders[0], cmd_list_data.pipeline_state_original_pixel_shader, PixelShaderPatch::BLENDED).shader.get();
      }
      else
      {
         if (const uint32_t pixel_shader_hash = original_shader_hashes.pixel_shaders[0]; pixel_shader_hash != gd.mv_last_pixel_shader_hash)
         {
            gd.mv_last_pixel_shader = GetPatchedShader<ID3D11PixelShader>(native_device, device_data, pixel_shader_hash, cmd_list_data.pipeline_state_original_pixel_shader).shader.get();
            gd.mv_last_pixel_shader_hash = pixel_shader_hash;
         }
         pixel_shader = gd.mv_last_pixel_shader;
      }
      if (!vertex_shader || !pixel_shader || !gd.mv_jitter_buffer)
         return MV_REJECT(REJECT_SHADERS);
      if (std::exchange(gd.mv_frame_ended, false))
      {
         gd.mv_markers_filled = false;
         gd.mv_fill_pending = gd.mv_uav && gd.mv_device_depth_uav && HasShaders(device_data.native_compute_shaders, CompileTimeStringHash("ME Motion Vector Fill CS"));
         // The fill's marker: the largest float16 (a larger clear value is stored as it in R16G16_FLOAT)
         const FLOAT clear_value = (gd.mv_fill_pending ? 65504.f : 0.f);
         const FLOAT clear[4] = {clear_value, clear_value, 0.f, 0.f};
         native_device_context->ClearRenderTargetView(gd.mv_rtv.get(), clear);
         // The reactive masks: what the alpha blended draws write, from zero
         if (IsReactiveMaskUsed(device_data) && !gd.mv_reactive_target)
         {
            D3D11_TEXTURE2D_DESC mv_desc;
            gd.mv_texture->GetDesc(&mv_desc);
            const CD3D11_TEXTURE2D_DESC reactive_desc(DXGI_FORMAT_R8G8_UNORM, mv_desc.Width, mv_desc.Height, 1, 1, D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE);
            const std::unique_lock lock(gd.mv_mutex);
            if (SUCCEEDED(native_device->CreateTexture2D(&reactive_desc, nullptr, &gd.mv_reactive_target)) && SUCCEEDED(native_device->CreateRenderTargetView(gd.mv_reactive_target.get(), nullptr, &gd.mv_reactive_target_rtv)))
            {
               native_device->CreateShaderResourceView(gd.mv_reactive_target.get(), nullptr, &gd.mv_reactive_target_srv);
            }
            gd.sr_reactive_frame = cb_luma_global_settings.FrameIndex;
         }
         if (gd.mv_reactive_target_rtv)
         {
            const FLOAT zero[4] = {};
            native_device_context->ClearRenderTargetView(gd.mv_reactive_target_rtv.get(), zero);
         }
         // Last frame's camera and objects are the previous ones, unless frames without a scene (menus, videos) came between
         const bool previous_valid = gd.mv_camera && cb_luma_global_settings.FrameIndex - gd.mv_frame_index <= 1;
         gd.mv_previous_camera = (previous_valid ? gd.mv_camera : nullptr);
         gd.mv_camera = nullptr;
         gd.mv_frame_index = cb_luma_global_settings.FrameIndex;
         // Swapped, not rebuilt: the lists keep their nodes and capacity (an empty list matches nothing); keys drawn in neither of the
         // last two frames go
         gd.mv_previous_objects.swap(gd.mv_objects);
#if DEVELOPMENT
         gd.mv_stats.tiebreak_collisions = PatchedDraws::CountTieBreakCollisions(gd.mv_previous_objects, [](const auto& a, const auto& b)
            { return PatchedDraws::SameBytes(a.constants, b.constants); });
#endif
         std::erase_if(gd.mv_objects, [](const auto& entry)
            {
               const auto& [key, objects] = entry;
               return objects.empty(); });
         for (auto& [key, objects] : gd.mv_objects)
         {
            objects.clear();
         }
         if (!previous_valid)
         {
            gd.mv_previous_objects.clear();
         }
      }

      // The game's vc4 (object, camera and bones in one). The slots added past it stay bound after the draw: no translated shader reads
      // a constant buffer past b4.
      ID3D11Buffer* const current = BoundState(&gd, gd.bound_object_buffer, [&](ID3D11Buffer** buffer)
         { native_device_context->VSGetConstantBuffers(MotionVectorPatches::object_slot, 1, buffer); });
      const UINT read_size = gd.mv_last_vertex_shader.read_size;
      // The buffer's CPU copy (null until its first upload); the lookup registers it for a copy at every upload
      MirrorsEdgeGameDeviceData::ConstantsCopy constants;
      if (current)
      {
         MirrorsEdgeGameDeviceData::RegisteredConstants* entry = FindRegisteredBuffer(&gd, reinterpret_cast<uint64_t>(current));
         if (!entry)
         {
            entry = RegisterBuffer(&gd, current);
         }
         constants = entry->copy;
      }
      // The previous frame's vc4: the same object's from last frame, else this draw's with last frame's camera (no object motion).
      // None (no CPU copy yet, another camera): the current one (zero motion).
      const std::vector<uint8_t>* upload = nullptr;
      if (constants && constants->size() >= TRANSLATION_OFFSET + 16)
      {
         // The frame's camera: its first motion vector draw's
         if (!gd.mv_camera)
         {
            gd.mv_camera = constants;
         }

         // Draw key: same mesh, same shaders, no instance count
         // The offsets go with the buffers: queried with them, else tracked with them
         UINT vertex_offset = gd.bound_vertex_offset;
         ID3D11Buffer* const vertex_buffer = BoundState(&gd, gd.bound_vertex_buffer, [&](ID3D11Buffer** buffer)
            {
               UINT stride;
               native_device_context->IAGetVertexBuffers(0, 1, buffer, &stride, &vertex_offset); });
         UINT index_offset = gd.bound_index_offset;
         ID3D11Buffer* const index_buffer = BoundState(&gd, gd.bound_index_buffer, [&](ID3D11Buffer** buffer)
            {
               DXGI_FORMAT format;
               native_device_context->IAGetIndexBuffer(buffer, &format, &index_offset); });
         const DrawDispatchData& draw_data = last_draw_dispatch_data;
         uint64_t key = 0;
         for (const uint64_t value : {uint64_t(original_shader_hashes.vertex_shaders[0]), uint64_t(original_shader_hashes.pixel_shaders[0]), reinterpret_cast<uint64_t>(vertex_buffer), uint64_t(vertex_offset),
                 reinterpret_cast<uint64_t>(index_buffer), uint64_t(index_offset), uint64_t(draw_data.index_count), uint64_t(draw_data.first_index), uint64_t(uint32_t(draw_data.vertex_offset)),
                 uint64_t(draw_data.vertex_count), uint64_t(draw_data.first_vertex)})
         {
            HashCombine(key, value);
         }
         // LocalToWorld (c5-c8) separates objects that share a key (props), as a tie-break only (see "PatchedDraws::ObjectTransform").
         // Skinned meshes' c8 is a bone row, the same for copies of a model (Mass Effect 2007's holstered weapons): theirs is c230-c233.
         const size_t translation_offset = (constants->size() >= gd.mv_last_vertex_shader.translation_offset + 16 ? gd.mv_last_vertex_shader.translation_offset : TRANSLATION_OFFSET);
         const PatchedDraws::ObjectTransform transform = PatchedDraws::ReadRowVectorTransform(constants->data() + translation_offset - 3 * 16);

         // ponytail: linear search among the key's candidates (a handful at most); a spatial lookup if big crowds share a mesh
         const MirrorsEdgeGameDeviceData::MotionVectorObject* match = nullptr;
         if (const auto previous = gd.mv_previous_objects.find(key); previous != gd.mv_previous_objects.end())
         {
            match = PatchedDraws::FindNearest(previous->second, transform, [&](const auto& candidate)
               { return candidate.constants->size() == constants->size(); });
         }
         if (match)
         {
            // Last frame's list outlives the draw ("mv_previous_objects" only changes at the next frame start)
            upload = &*match->constants;
#if DEVELOPMENT
            gd.mv_stats.matched++;
#endif
         }
         else if (gd.mv_previous_camera && std::memcmp(constants->data() + VIEW_PROJECTION_OFFSET, gd.mv_camera->data() + VIEW_PROJECTION_OFFSET, CAMERA_SIZE) == 0)
         {
            // Not found, drawn with the frame's camera: its own constants with last frame's camera (camera motion only), only what the
            // shader reads (at least the camera)
            const size_t copy_size = (read_size != 0 ? std::clamp<size_t>(read_size, VIEW_PROJECTION_OFFSET + CAMERA_SIZE, constants->size()) : constants->size());
            gd.mv_camera_only_copy.assign(constants->begin(), constants->begin() + copy_size);
            std::memcpy(gd.mv_camera_only_copy.data() + VIEW_PROJECTION_OFFSET, gd.mv_previous_camera->data() + VIEW_PROJECTION_OFFSET, CAMERA_SIZE);
            upload = &gd.mv_camera_only_copy;
#if DEVELOPMENT
            gd.mv_stats.camera_only++;
#endif
         }
#if DEVELOPMENT
         else
         {
            gd.mv_stats.other_camera++;
         }
#endif
         // Kept as drawn for the next frame
         gd.mv_objects[key].push_back({.transform = transform, .constants = std::move(constants)});
      }
#if DEVELOPMENT
      else
      {
         gd.mv_stats.uncopied++;
      }
#endif
      // An alpha blended draw blends its motion vector with what's under it: texels no opaque motion vector draw wrote still hold the
      // fill's marker (65504), which the blend would turn into a huge motion. The fill gives them their camera motion first, from the
      // opaque scene's depth (the end of the scene fills the rest and the masks).
      if (blended && gd.mv_fill_pending && !std::exchange(gd.mv_markers_filled, true))
      {
         DrawStateStack<DrawStateStackType::FullGraphics> graphics_state;
         DrawStateStack<DrawStateStackType::Compute> compute_state;
         graphics_state.Cache(native_device_context, device_data.uav_max_count);
         compute_state.Cache(native_device_context, device_data.uav_max_count);
         FillMotionVectors(native_device, native_device_context, device_data, /* write_reactive */ false);
         compute_state.Restore(native_device_context);
         graphics_state.Restore(native_device_context);
      }
      ID3D11Buffer* const previous_current[] = {current};
      gd.mv_previous_constants.Bind(native_device, native_device_context, MotionVectorPatches::previous_slots, {&upload, 1}, previous_current, "Mirror's Edge", {&read_size, 1});
      BindJitter(native_device_context, &gd);
      // Left bound after the draw (set directly, bypassing Core's state tracking): the game's next draws either bind their own targets
      // and shaders, or are motion vector draws too. The game's pixel shaders write o0 only, so the motion vector target keeps its
      // contents; a mask target left by an earlier draw goes (its max blend would take an undefined value).
      if (rtvs[MotionVectorPatches::target_slot] != gd.mv_rtv || rtvs[MotionVectorPatches::reactive_slot] != nullptr)
      {
         ID3D11RenderTargetView* targets[MotionVectorPatches::target_slot + 1] = {rtvs[0].get()};
         targets[MotionVectorPatches::target_slot] = gd.mv_rtv.get();
         native_device_context->OMSetRenderTargets(MotionVectorPatches::target_slot + 1, targets, dsv);
      }
      PatchedDraws::BindPatchedShader(native_device_context, vertex_shader, &gd.mv_bound_vertex_shader);
      PatchedDraws::BindPatchedShader(native_device_context, pixel_shader, &gd.mv_bound_pixel_shader);

      draw();
#if DEVELOPMENT
      gd.mv_stats.motion_vector_draws++;
#endif
      return true;
   }

   // Jitter for the scene's mesh draws without motion vectors (patched vertex shader, game pixel shader): the depth prepass, lights,
   // shadow projections, decals, translucents. Every draw depth tested against the scene takes the same jitter, or jittered and
   // unjittered depths of the same surface fail each other's test. False if it can't (the draw runs untouched).
   static bool DrawWithJitter(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, const CommandListData& cmd_list_data, DeviceData& device_data,
      const ShaderHashesList<OneShaderPerPipeline>& original_shader_hashes, const std::function<void()>& draw, const com_ptr<ID3D11RenderTargetView> (&rtvs)[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT], ID3D11DepthStencilView* dsv)
   {
      auto& gd = GetGameDeviceData(device_data);
      if (!gd.mv_scene_open || gd.mv_jitter == std::array<float, 2>{} || !gd.mv_jitter_buffer)
         return false;
      // Meshes only (full screen passes have no vertex buffer or no depth test), into the scene depth (not shadows)
      if (!dsv)
         return false;
      if (dsv != gd.jitter_dsv)
      {
         com_ptr<ID3D11Resource> depth;
         dsv->GetResource(&depth);
         gd.jitter_dsv = dsv;
         gd.jitter_dsv_scene = depth == gd.mv_depth;
      }
      if (!gd.jitter_dsv_scene || !IsDepthTested(native_device_context, &gd))
         return false;
      const ID3D11Buffer* const vertex_buffer = BoundState(&gd, gd.bound_vertex_buffer, [&](ID3D11Buffer** buffer)
         {
            UINT stride, offset;
            native_device_context->IAGetVertexBuffers(0, 1, buffer, &stride, &offset); });
      if (!vertex_buffer)
         return false;
      ID3D11VertexShader* const vertex_shader = GetPatchedVertexShader(native_device, cmd_list_data, device_data, original_shader_hashes.vertex_shaders[0]).shader.get();
      if (!vertex_shader)
         return false;

      // An alpha blended draw into the scene writes its mask, reactive or transparency & composition (its pixel shader patched, the mask
      // target added past the motion vector one)
      ID3D11PixelShader* reactive_shader = nullptr;
      if (IsReactiveMaskUsed(device_data) && gd.mv_reactive_target_rtv && rtvs[0] && rtvs[0] == gd.mv_scene_rtv)
      {
         ClassifyBoundBlend(native_device_context, &gd);
         if (const uint8_t blend = gd.mv_reactive_blend; blend != 0)
         {
            reactive_shader = GetPatchedShader<ID3D11PixelShader>(native_device, device_data, original_shader_hashes.pixel_shaders[0], cmd_list_data.pipeline_state_original_pixel_shader, (blend == 2 ? PixelShaderPatch::MASK_ADDITIVE : PixelShaderPatch::MASK_ALPHA)).shader.get();
         }
      }

      // The patched vertex shader and the jitter stay bound after the draw (see "DrawWithMotionVectors"), with the game's pixel shader
      // (a motion vector draw's is put back) or its mask version, and the mask target
      PatchedDraws::BindPatchedShader(native_device_context, vertex_shader, &gd.mv_bound_vertex_shader);
      BindJitter(native_device_context, &gd);
      // A mask target left by an earlier draw goes when this one doesn't write it
      if (ID3D11RenderTargetView* const mask_rtv = (reactive_shader ? gd.mv_reactive_target_rtv.get() : nullptr); rtvs[MotionVectorPatches::reactive_slot].get() != mask_rtv)
      {
         ID3D11RenderTargetView* targets[MotionVectorPatches::reactive_slot + 1] = {};
         for (uint32_t slot = 0; slot < MotionVectorPatches::reactive_slot; slot++)
         {
            targets[slot] = rtvs[slot].get();
         }
         targets[MotionVectorPatches::reactive_slot] = mask_rtv;
         native_device_context->OMSetRenderTargets(MotionVectorPatches::reactive_slot + 1, targets, dsv);
      }
      if (reactive_shader)
      {
         PatchedDraws::BindPatchedShader(native_device_context, reactive_shader, &gd.mv_bound_pixel_shader);
#if DEVELOPMENT
         gd.mv_stats.reactive_draws++;
#endif
      }
      else
      {
         PatchedDraws::RestoreGameShader(native_device_context, &gd.mv_bound_pixel_shader);
      }
      draw();
#if DEVELOPMENT
      gd.mv_stats.jitter_draws++;
#endif
      return true;
   }

   // DLAA, FSR 3 Native AA or Luma TAA on the jittered scene, its depth (from its alpha, see the fill) and the motion vectors; the result
   // goes back into the scene's color (and its copy, see "mv_scene_copy"), its alpha (the post passes' linear depth) kept. False if it
   // didn't draw (missing input, or the upscaler failed). "reactive_mask": the fill wrote this scene's masks.
   static bool DrawUpscaler(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, DeviceData& device_data, bool reactive_mask)
   {
      auto& gd = GetGameDeviceData(device_data);
      auto* const copy_vs = FindShader(device_data.native_vertex_shaders, CompileTimeStringHash("Copy VS"));
      auto* const copy_ps = FindShader(device_data.native_pixel_shaders, CompileTimeStringHash("Copy PS"));
      if (!gd.mv_texture || !gd.mv_camera || !gd.mv_device_depth || !gd.mv_scene_color || !gd.mv_scene_rtv || !copy_vs || !copy_ps)
         return false;
      com_ptr<ID3D11Texture2D> scene;
      if (FAILED(gd.mv_scene_color->QueryInterface(&scene)))
         return false;
      D3D11_TEXTURE2D_DESC scene_desc;
      scene->GetDesc(&scene_desc);
      SR::InstanceData* const sr_instance_data = device_data.GetSRInstanceData();
      if (!sr_instance_data)
         return false;

      D3D11_TEXTURE2D_DESC output_desc = {};
      if (device_data.sr_output_color)
      {
         device_data.sr_output_color->GetDesc(&output_desc);
      }
      if (output_desc.Width != scene_desc.Width || output_desc.Height != scene_desc.Height)
      {
         device_data.sr_output_color.reset();
         gd.sr_output_srv.reset();
         output_desc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R16G16B16A16_FLOAT, scene_desc.Width, scene_desc.Height, 1, 1, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS);
         if (SUCCEEDED(SRBridge::CreateSharableTexture(native_device, output_desc, &device_data.sr_output_color)))
         {
            native_device->CreateShaderResourceView(device_data.sr_output_color.get(), nullptr, &gd.sr_output_srv);
         }
      }
      if (!device_data.sr_output_color || !gd.sr_output_srv)
      {
         // Back to SMAA until the upscaler is picked again
         device_data.sr_suppressed = true;
         return false;
      }
      if (!gd.sr_rgb_blend_state)
      {
         D3D11_BLEND_DESC blend_desc = CD3D11_BLEND_DESC(D3D11_DEFAULT);
         blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_RED | D3D11_COLOR_WRITE_ENABLE_GREEN | D3D11_COLOR_WRITE_ENABLE_BLUE;
         native_device->CreateBlendState(&blend_desc, &gd.sr_rgb_blend_state);
         if (!gd.sr_rgb_blend_state)
            return false;
      }

      // FSR needs the camera (vc4's view projection multiplies row vectors)
      const SR::ViewProjectionCamera camera = SR::GetViewProjectionCamera(reinterpret_cast<const float*>(gd.mv_camera->data() + VIEW_PROJECTION_OFFSET), /* row_vectors */ true);

      const SR::SettingsData settings_data = {
         .output_width = scene_desc.Width,
         .output_height = scene_desc.Height,
         .render_width = scene_desc.Width,
         .render_height = scene_desc.Height,
         .hdr = true,
         // The motion vectors are UV deltas, previous minus current
         .mvs_x_scale = float(scene_desc.Width),
         .mvs_y_scale = float(scene_desc.Height),
         // The scene is not exposed yet: TdToneMapping multiplies it by its eye adaptation after the upscaler. DLSS's auto exposure
         // covers that; FSR's clips highlights (FSR-Best-Practices FIN-3), so it runs at exposure 1 (Borderlands 2's).
         .auto_exposure = device_data.sr_type != SR::Type::FSR,
         // DLAA on scene-referred HDR input takes preset J or K, never L or M (DLSS-Best-Practices PRE-4): "Default" picks K (11), so
         // NVIDIA can't change it over the air; an explicit choice is kept
         .render_preset = (dlss_render_preset != 0 ? dlss_render_preset : 11u),
      };
      sr_implementations[device_data.sr_type]->UpdateSettings(sr_instance_data, native_device_context, settings_data);

      // FSR requires a FOV (it errors on 0), and both take near and far: a camera without an up axis or a usable projection keeps the
      // last ones. A finite far (depth 1) even when the projection has none (FSR's context is FFX_FSR3_ENABLE_DEPTH_INFINITE only with
      // inverted depth).
      if (camera.vert_fov > 0.0)
      {
         gd.sr_vert_fov = float(camera.vert_fov);
      }
      if (camera.near_plane > 0.0)
      {
         gd.sr_near_plane = float(camera.near_plane);
         gd.sr_far_plane = float(camera.far_plane);
      }
      // The reactive masks (FSR and Luma TAA; DLSS ignores them)
      const SR::SuperResolutionImpl::DrawData draw_data = {
         .reset = device_data.force_reset_sr,
         .output_color = device_data.sr_output_color.get(),
         .source_color = scene.get(),
         .motion_vectors = gd.mv_texture.get(),
         .depth_buffer = gd.mv_device_depth.get(),
         .bias_mask = (reactive_mask ? gd.mv_reactive.get() : nullptr),
         .transparency_alpha = (reactive_mask ? gd.mv_transparency.get() : nullptr),
         // As applied (pixels, +y down)
         .jitter_x = gd.mv_jitter[0],
         .jitter_y = gd.mv_jitter[1],
         .vert_fov = gd.sr_vert_fov,
         .near_plane = gd.sr_near_plane,
         .far_plane = gd.sr_far_plane,
      };
      if (!sr_implementations[device_data.sr_type]->Draw(sr_instance_data, native_device_context, draw_data))
      {
         // Back to SMAA until the upscaler is picked again
         device_data.sr_suppressed = true;
         return false;
      }
      // The RGB write mask keeps the scene's linear depth; the copy is the game's resolve of the same scene, written in the same pass
      // (with a view like the scene's: dgVoodoo's are single slice arrays), else copied whole after it
      ID3D11RenderTargetView* copy_rtv = nullptr;
      auto* const copy_back_ps = FindShader(device_data.native_pixel_shaders, CompileTimeStringHash("ME Copy Back PS"));
      if (gd.mv_scene_copy && copy_back_ps)
      {
         com_ptr<ID3D11Resource> copy_rtv_resource;
         if (gd.mv_scene_copy_rtv)
         {
            gd.mv_scene_copy_rtv->GetResource(&copy_rtv_resource);
         }
         if (copy_rtv_resource != gd.mv_scene_copy)
         {
            gd.mv_scene_copy_rtv.reset();
            com_ptr<ID3D11Texture2D> copy;
            if (SUCCEEDED(gd.mv_scene_copy->QueryInterface(&copy)))
            {
               D3D11_TEXTURE2D_DESC copy_desc;
               copy->GetDesc(&copy_desc);
               D3D11_RENDER_TARGET_VIEW_DESC rtv_desc;
               gd.mv_scene_rtv->GetDesc(&rtv_desc);
               if (copy_desc.BindFlags & D3D11_BIND_RENDER_TARGET)
               {
                  native_device->CreateRenderTargetView(copy.get(), &rtv_desc, &gd.mv_scene_copy_rtv);
               }
            }
         }
         copy_rtv = gd.mv_scene_copy_rtv.get();
      }
      DrawCustomPixelShader(native_device_context, device_data.default_depth_stencil_state.get(), gd.sr_rgb_blend_state.get(), nullptr, copy_vs, copy_rtv ? copy_back_ps : copy_ps,
         gd.sr_output_srv.get(), gd.mv_scene_rtv.get(), scene_desc.Width, scene_desc.Height, true, copy_rtv);
      if (gd.mv_scene_copy && !copy_rtv)
      {
         native_device_context->CopyResource(gd.mv_scene_copy.get(), scene.get());
      }
      // Not while the bridge's helper starts (the color copied as it is): SMAA stays on and the next frame resets
      device_data.has_drawn_sr = sr_implementations[device_data.sr_type]->IsReady(sr_instance_data);
      return true;
   }

   // The fill's constants ("Luma_ME_MotionVectorFill.hlsl" b0)
   struct MotionVectorFillConstants
   {
      float reprojection[16]; // Current clip space to the previous frame's, row major
      float jitter_ndc[2];
      float depth_from_view[2];
      float reactive_scale;
      float reactive_threshold;
      float reactive_enabled;
      float padding;
   };
   static_assert(sizeof(MotionVectorFillConstants) == 96);

   // The depth and camera motion fill (see "Luma_ME_MotionVectorFill.hlsl"), with the reactive masks if "write_reactive". The caller
   // caches and restores the state. False if it didn't run.
   static bool FillMotionVectors(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, DeviceData& device_data, bool write_reactive)
   {
      auto& gd = GetGameDeviceData(device_data);
      auto* const fill_shader = FindShader(device_data.native_compute_shaders, CompileTimeStringHash("ME Motion Vector Fill CS"));
      if (gd.mv_scene_srv)
      {
         com_ptr<ID3D11Resource> srv_resource;
         gd.mv_scene_srv->GetResource(&srv_resource);
         if (srv_resource != gd.mv_scene_color)
         {
            gd.mv_scene_srv.reset();
         }
      }
      if (!fill_shader || !gd.mv_camera || !gd.mv_scene_color ||
          (!gd.mv_scene_srv && FAILED(native_device->CreateShaderResourceView(gd.mv_scene_color.get(), nullptr, &gd.mv_scene_srv))))
         return false;
      // Current clip space to the previous frame's: previous * inverse(current) for column vectors (vc4 holds the row vector matrix,
      // transposed here), in double (absolute world translation)
      const float* const view_projection = reinterpret_cast<const float*>(gd.mv_camera->data() + VIEW_PROJECTION_OFFSET);
      Math::Matrix44D current, previous;
      current.SetIdentity();
      previous.SetIdentity();
      if (gd.mv_previous_camera)
      {
         std::copy_n(view_projection, 16, current.GetData());
         std::copy_n(reinterpret_cast<const float*>(gd.mv_previous_camera->data() + VIEW_PROJECTION_OFFSET), 16, previous.GetData());
         current.Transpose();
         previous.Transpose();
         current.Invert();
      }
      const Math::Matrix44D reprojection = previous * current;
      const SR::ViewProjectionCamera camera = SR::GetViewProjectionCamera(view_projection, /* row_vectors */ true);
      MotionVectorFillConstants constants = {
         .jitter_ndc = {gd.mv_jitter_ndc[0], gd.mv_jitter_ndc[1]},
         .depth_from_view = {float(camera.depth_a), float(camera.depth_b)},
         .reactive_scale = g_sr_reactive_scale,
         .reactive_threshold = g_sr_reactive_threshold,
         .reactive_enabled = (write_reactive ? 1.f : 0.f),
      };
      for (int i = 0; i < 16; i++)
      {
         constants.reprojection[i] = float(reprojection.GetData()[i]);
      }
      if (!PatchedDraws::WriteDynamicConstants(native_device, native_device_context, std::addressof(gd.mv_fill_buffer), &constants, sizeof(constants)))
         return false;
      // The scene and the motion vectors may be bound as render targets
      native_device_context->OMSetRenderTargets(0, nullptr, nullptr);
      ID3D11Buffer* const buffer = gd.mv_fill_buffer.get();
      ID3D11ShaderResourceView* const srvs[2] = {gd.mv_scene_srv.get(), write_reactive ? gd.mv_reactive_target_srv.get() : nullptr};
      ID3D11UnorderedAccessView* const uavs[4] = {gd.mv_uav.get(), gd.mv_device_depth_uav.get(), write_reactive ? gd.mv_reactive_uav.get() : nullptr, write_reactive ? gd.mv_transparency_uav.get() : nullptr};
      native_device_context->CSSetConstantBuffers(0, 1, &buffer);
      native_device_context->CSSetShaderResources(0, UINT(std::size(srvs)), srvs);
      native_device_context->CSSetUnorderedAccessViews(0, UINT(std::size(uavs)), uavs, nullptr);
      native_device_context->CSSetShader(fill_shader, nullptr, 0);
      native_device_context->Dispatch((uint32_t(device_data.output_resolution.x) + 7) / 8, (uint32_t(device_data.output_resolution.y) + 7) / 8, 1);
      ID3D11UnorderedAccessView* const null_uavs[std::size(uavs)] = {};
      ID3D11ShaderResourceView* const null_srvs[std::size(srvs)] = {};
      native_device_context->CSSetUnorderedAccessViews(0, UINT(std::size(null_uavs)), null_uavs, nullptr);
      native_device_context->CSSetShaderResources(0, UINT(std::size(null_srvs)), null_srvs);
      gd.mv_filled = true;
      if (write_reactive)
      {
         gd.sr_reactive_frame = cb_luma_global_settings.FrameIndex;
      }
      return true;
   }

   // Ends the scene at its first post pass: the fill (the last camera motion, the depth, the reactive masks), then the upscaler, both
   // before that pass reads the scene
   static void EndScene(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, DeviceData& device_data)
   {
      auto& gd = GetGameDeviceData(device_data);
      gd.mv_scene_open = false;
      gd.mv_scene_done = true;
      DrawStateStack<DrawStateStackType::FullGraphics> graphics_state;
      DrawStateStack<DrawStateStackType::Compute> compute_state;
      graphics_state.Cache(native_device_context, device_data.uav_max_count);
      compute_state.Cache(native_device_context, device_data.uav_max_count);
      // The reactive and transparency & composition masks, written by the fill from what the alpha blended draws wrote
      const bool reactive = IsReactiveMaskUsed(device_data) && gd.mv_reactive_target_srv;
      if (reactive)
      {
         D3D11_TEXTURE2D_DESC desc = {};
         if (gd.mv_reactive)
         {
            gd.mv_reactive->GetDesc(&desc);
         }
         if (desc.Width != uint32_t(device_data.output_resolution.x) || desc.Height != uint32_t(device_data.output_resolution.y))
         {
            const std::unique_lock lock(gd.mv_mutex);
            gd.mv_reactive.reset();
            gd.mv_reactive_uav.reset();
            gd.mv_transparency.reset();
            gd.mv_transparency_uav.reset();
            desc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R8_UNORM, uint32_t(device_data.output_resolution.x), uint32_t(device_data.output_resolution.y), 1, 1, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS);
            if (SUCCEEDED(SRBridge::CreateSharableTexture(native_device, desc, &gd.mv_reactive)))
            {
               native_device->CreateUnorderedAccessView(gd.mv_reactive.get(), nullptr, &gd.mv_reactive_uav);
            }
            if (SUCCEEDED(SRBridge::CreateSharableTexture(native_device, desc, &gd.mv_transparency)))
            {
               native_device->CreateUnorderedAccessView(gd.mv_transparency.get(), nullptr, &gd.mv_transparency_uav);
            }
         }
      }
      const bool write_reactive = reactive && gd.mv_reactive_uav && gd.mv_transparency_uav;
      const bool filled = std::exchange(gd.mv_fill_pending, false) && FillMotionVectors(native_device, native_device_context, device_data, write_reactive);
      // The upscaler's depth comes from the fill
      if (gd.sr_active && filled)
      {
         [[maybe_unused]] const bool drawn = DrawUpscaler(native_device, native_device_context, device_data, write_reactive);
#if DEVELOPMENT
         gd.mv_stats.sr_draws += drawn;
#endif
      }
      compute_state.Restore(native_device_context);
      graphics_state.Restore(native_device_context);
   }

   // The weighted resolve reads the samples of the fp16 MSAA scene, which the game (dgVoodoo) only ever resolves and creates without
   // a shader resource bind. The bind flag only: dgVoodoo's formats stay as created.
   static bool OnCreateResource(reshade::api::device* device, reshade::api::resource_desc& desc, reshade::api::subresource_data* initial_data, reshade::api::resource_usage initial_state)
   {
      if (desc.type != reshade::api::resource_type::texture_2d || desc.texture.samples <= 1 || (desc.usage & reshade::api::resource_usage::render_target) == 0)
         return false;
      if (desc.texture.format != reshade::api::format::r16g16b16a16_float && desc.texture.format != reshade::api::format::r16g16b16a16_typeless)
         return false;
      if ((desc.usage & reshade::api::resource_usage::shader_resource) != 0)
         return false;
      // The scene's are output sized; any size before the swapchain is known (an unbindable one keeps the hardware resolve)
      if (const DeviceData* device_data = device->get_private_data<DeviceData>();
         device_data && device_data->output_resolution.x > 0 && (desc.texture.width != uint32_t(device_data->output_resolution.x) || desc.texture.height != uint32_t(device_data->output_resolution.y)))
         return false;
      desc.usage |= reshade::api::resource_usage::shader_resource;
      return true;
   }

   // The game's ResolveSubresource of the fp16 MSAA scene (three a frame, all into the texture the refraction and post passes read):
   // Luma_ME_MSAAResolve instead. Anything else, or a pair we can't bind, keeps the hardware resolve.
   static bool OnResolveTextureRegion(reshade::api::command_list* cmd_list, reshade::api::resource source, uint32_t source_subresource, const reshade::api::subresource_box* source_box, reshade::api::resource dest, uint32_t dest_subresource, uint32_t dest_x, uint32_t dest_y, uint32_t dest_z, reshade::api::format format)
   {
      if (format != reshade::api::format::r16g16b16a16_float || source_subresource != 0 || dest_subresource != 0 || source_box != nullptr || dest_x != 0 || dest_y != 0 || dest_z != 0)
         return false;

      com_ptr<ID3D11Texture2D> source_texture;
      com_ptr<ID3D11Texture2D> dest_texture;
      if (FAILED(reinterpret_cast<ID3D11Resource*>(source.handle)->QueryInterface(&source_texture)) || FAILED(reinterpret_cast<ID3D11Resource*>(dest.handle)->QueryInterface(&dest_texture)))
         return false;
      D3D11_TEXTURE2D_DESC source_desc;
      source_texture->GetDesc(&source_desc);
      if (source_desc.SampleDesc.Count <= 1)
         return false;
      DeviceData& device_data = *cmd_list->get_device()->get_private_data<DeviceData>();
      auto& gd = GetGameDeviceData(device_data);
      // MSAA is on in the game, whatever the AA picked or the toggle (the settings, and the coverage draws' gate)
      gd.msaa_resolved = true;
      if (!g_msaa_resolve_enable)
         return false;
      D3D11_TEXTURE2D_DESC dest_desc;
      dest_texture->GetDesc(&dest_desc);
      if (source_desc.ArraySize != 1 || dest_desc.ArraySize != 1 || (source_desc.BindFlags & D3D11_BIND_SHADER_RESOURCE) == 0 || (dest_desc.BindFlags & D3D11_BIND_RENDER_TARGET) == 0)
         return false;

      // Held through the draw so a shader reload can't release them mid-use
      const std::shared_lock lock_shader_objects(s_mutex_shader_objects);
      auto* const copy_vs = FindShader(device_data.native_vertex_shaders, CompileTimeStringHash("Copy VS"));
      auto* const resolve_ps = FindShader(device_data.native_pixel_shaders, g_msaa_resolve_average_depth ? CompileTimeStringHash("ME MSAA Resolve Average Depth PS") : CompileTimeStringHash("ME MSAA Resolve PS"));
      if (!copy_vs || !resolve_ps)
         return false;

      ID3D11Device* native_device = (ID3D11Device*)(cmd_list->get_device()->get_native());
      ID3D11DeviceContext* native_device_context = (ID3D11DeviceContext*)(cmd_list->get_native());

      // ponytail: views are created per resolve (three a frame); cache them per resource if it ever shows in a profile
      const D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {.Format = DXGI_FORMAT_R16G16B16A16_FLOAT, .ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DMS};
      const D3D11_RENDER_TARGET_VIEW_DESC rtv_desc = {.Format = DXGI_FORMAT_R16G16B16A16_FLOAT, .ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D};
      com_ptr<ID3D11ShaderResourceView> source_srv;
      com_ptr<ID3D11RenderTargetView> dest_rtv;
      if (FAILED(native_device->CreateShaderResourceView(source_texture.get(), &srv_desc, &source_srv)) || FAILED(native_device->CreateRenderTargetView(dest_texture.get(), &rtv_desc, &dest_rtv)))
         return false;

      DrawStateStack<DrawStateStackType::FullGraphics> draw_state_stack;
      draw_state_stack.Cache(native_device_context, device_data.uav_max_count);
      // The game resolves with the MSAA scene still bound for output, and D3D11 refuses it as a shader resource then
      native_device_context->OMSetRenderTargetsAndUnorderedAccessViews(0, nullptr, nullptr, 0, 0, nullptr, nullptr);
      ID3D11ShaderResourceView* const exposure_srv = gd.srv_exposure.get();
      native_device_context->PSSetShaderResources(1, 1, &exposure_srv);
      DrawCustomPixelShader(native_device_context, device_data.default_depth_stencil_state.get(), device_data.default_blend_state.get(), nullptr, copy_vs, resolve_ps, source_srv.get(), dest_rtv.get(), dest_desc.Width, dest_desc.Height);
      draw_state_stack.Restore(native_device_context);
#if DEVELOPMENT
      gd.mv_stats.msaa_resolves++;
#endif
      return true;
   }

public:
   // The motion vector events: the vc4 copies, the motion vector target in every blend state and the tracked bindings; the MSAA resolve
   static void SetEvents(bool enable)
   {
      const auto set = [enable]<reshade::addon_event event>(typename reshade::addon_event_traits<event>::decl callback)
      {
         if (enable)
         {
            reshade::register_event<event>(callback);
         }
         else
         {
            reshade::unregister_event<event>(callback);
         }
      };
      set.template operator()<reshade::addon_event::map_buffer_region>(OnMapBufferRegion);
      set.template operator()<reshade::addon_event::unmap_buffer_region>(OnUnmapBufferRegion);
      set.template operator()<reshade::addon_event::update_buffer_region>(OnUpdateBufferRegion);
      set.template operator()<reshade::addon_event::create_pipeline>(PatchedDraws::OnCreateBlendState<MotionVectorPatches::target_slot, MotionVectorPatches::reactive_slot, false, true>);
      set.template operator()<reshade::addon_event::bind_pipeline>(OnBindPipeline);
      set.template operator()<reshade::addon_event::push_descriptors>(OnPushConstantBuffers);
      set.template operator()<reshade::addon_event::bind_vertex_buffers>(OnBindVertexBuffers);
      set.template operator()<reshade::addon_event::bind_index_buffer>(OnBindIndexBuffer);
      set.template operator()<reshade::addon_event::create_resource>(OnCreateResource);
      set.template operator()<reshade::addon_event::resolve_texture_region>(OnResolveTextureRegion);
   }

   std::unique_ptr<std::byte[]> PatchShaderBytecodeSync(const std::byte* code, size_t& size, reshade::api::pipeline_subobject_type type, uint64_t shader_hash, const std::byte* shader_object, size_t shader_object_size) override
   {
      constexpr uint32_t ret_token = ENCODE_D3D10_SB_OPCODE_TYPE(D3D10_SB_OPCODE_RET) | ENCODE_D3D10_SB_TOKENIZED_INSTRUCTION_LENGTH(1);
      if (type != reshade::api::pipeline_subobject_type::pixel_shader || size % sizeof(uint32_t) != 0 || size < sizeof(uint32_t))
         return nullptr;
      const bool saturate = ui_saturated_pixel_shaders.contains(uint32_t(shader_hash));
      if (!saturate && !scene_floored_pixel_shaders.contains(uint32_t(shader_hash)))
         return nullptr;
      if (reinterpret_cast<const uint32_t*>(code)[size / sizeof(uint32_t) - 1] != ret_token)
         return nullptr;
      // As fxc encodes them (Yakuza Remastered Collection's mov_sat; the max checked on an fxc ps_4_0 compile)
      constexpr uint32_t dst_o0 = DXBC::Destination(D3D10_SB_OPERAND_TYPE_OUTPUT, D3D10_SB_OPERAND_4_COMPONENT_MASK_ALL);
      constexpr uint32_t src_o0 = DXBC::Source(D3D10_SB_OPERAND_TYPE_OUTPUT, 0, 1, 2, 3);
      constexpr uint32_t immediate_4 = ENCODE_D3D10_SB_OPERAND_NUM_COMPONENTS(D3D10_SB_OPERAND_4_COMPONENT) | ENCODE_D3D10_SB_OPERAND_TYPE(D3D10_SB_OPERAND_TYPE_IMMEDIATE32);
      // mov_sat o0.xyzw, o0.xyzw
      constexpr uint32_t saturate_patch[] = {ENCODE_D3D10_SB_OPCODE_TYPE(D3D10_SB_OPCODE_MOV) | ENCODE_D3D10_SB_TOKENIZED_INSTRUCTION_LENGTH(5) | ENCODE_D3D10_SB_INSTRUCTION_SATURATE(true), dst_o0, 0, src_o0, 0};
      // max o0.xyzw, o0.xyzw, l(0, 0, 0, 0)
      constexpr uint32_t floor_patch[] = {ENCODE_D3D10_SB_OPCODE_TYPE(D3D10_SB_OPCODE_MAX) | ENCODE_D3D10_SB_TOKENIZED_INSTRUCTION_LENGTH(10), dst_o0, 0, src_o0, 0, immediate_4, 0, 0, 0, 0};
      static_assert(saturate_patch[0] == 0x05002036 && dst_o0 == 0x001020F2 && src_o0 == 0x00102E46 && floor_patch[0] == 0x0A000034 && immediate_4 == 0x00004002);
      const std::span<const uint32_t> patch = (saturate ? std::span<const uint32_t>(saturate_patch) : std::span<const uint32_t>(floor_patch));
      const size_t ret_offset = size - sizeof(uint32_t);
      auto new_code = std::make_unique<std::byte[]>(size + patch.size_bytes());
      std::memcpy(new_code.get(), code, ret_offset);
      std::memcpy(new_code.get() + ret_offset, patch.data(), patch.size_bytes());
      std::memcpy(new_code.get() + ret_offset + patch.size_bytes(), code + ret_offset, sizeof(uint32_t));
      size += patch.size_bytes();
      return new_code;
   }

   void OnInit(bool async) override
   {
      std::vector<ShaderDefineData> game_shader_defines_data = {
         {"TONEMAP_TYPE", '1', true, false, "0 - SDR: Vanilla (clamped reference)\n1 - HDR: extended native grade + MacLeod-Boynton hue + DICE display map", 1},
      };
      shader_defines_data.append_range(game_shader_defines_data);
      assert(shader_defines_data.size() < MAX_SHADER_DEFINES);

      // Core registers the 6 SMAA passes and RCAS (ENABLE_RCAS); these are this game's own.
      // SMAA predication: the scene alpha's linear depth -> R16F plane-deviation edge-ness
      native_shaders_definitions.emplace(CompileTimeStringHash("ME Depth Extract CS"),
         ShaderDefinition("Luma_ME_DepthExtract", reshade::api::pipeline_subobject_type::compute_shader));
      // SMAA T2x's own passes ("SMAA_T2X" in Luma_SMAA_impl.hlsl). The edge detection and the vertex shaders are 1x's.
      native_shaders_definitions.emplace(CompileTimeStringHash("ME SMAA T2x Blending Weight Calculation PS"),
         ShaderDefinition{"Luma_SMAA_impl", reshade::api::pipeline_subobject_type::pixel_shader, nullptr, "smaa_blending_weight_calculation_ps", {{"SMAA_T2X", "1"}}});
      native_shaders_definitions.emplace(CompileTimeStringHash("ME SMAA T2x Neighborhood Blending PS"),
         ShaderDefinition{"Luma_SMAA_impl", reshade::api::pipeline_subobject_type::pixel_shader, nullptr, "smaa_neighborhood_blending_ps", {{"SMAA_T2X", "1"}}});
      native_shaders_definitions.emplace(CompileTimeStringHash("ME SMAA T2x Resolve PS"),
         ShaderDefinition{"Luma_SMAA_impl", reshade::api::pipeline_subobject_type::pixel_shader, nullptr, "smaa_resolve_ps", {{"SMAA_T2X", "1"}}});

      // DLSS/FSR/Luma TAA: their depth and the camera motion from the scene's alpha, the CPU copies of vc4 (dgVoodoo maps it or updates
      // it), and the motion vector target written by every blend state
      sr_game_tooltip = "DLSS and FSR 3 require Luma-Upscaler.exe next to the game's exe (Luma TAA doesn't).\n";
      native_shaders_definitions.emplace(CompileTimeStringHash("ME Motion Vector Fill CS"),
         ShaderDefinition("Luma_ME_MotionVectorFill", reshade::api::pipeline_subobject_type::compute_shader));
      native_shaders_definitions.emplace(CompileTimeStringHash("ME Copy Back PS"),
         ShaderDefinition("Luma_ME_CopyBack", reshade::api::pipeline_subobject_type::pixel_shader));
      native_shaders_definitions.emplace(CompileTimeStringHash("ME MSAA Resolve PS"),
         ShaderDefinition("Luma_ME_MSAAResolve", reshade::api::pipeline_subobject_type::pixel_shader));
#if DEVELOPMENT
      native_shaders_definitions.emplace(CompileTimeStringHash("ME MSAA Resolve Average Depth PS"),
         ShaderDefinition{"Luma_ME_MSAAResolve", reshade::api::pipeline_subobject_type::pixel_shader, nullptr, "main", { {"RESOLVE_AVERAGE_DEPTH", "1"} }});
#endif
      SetEvents(true);

#if DEVELOPMENT
      // For the MCP "luma_dev_values" tool
      Mcp::RegisterToggles({{"smaa_enable", &g_smaa_enable}, {"smaa_t2x", &g_smaa_t2x}, {"smaa_predication", &g_smaa_predication}, {"hide_ui", &g_hide_ui}, {"mv_enable", &g_mv_enable},
         {"mv_debug_view", &g_mv_debug_view}, {"mv_force_jitter", &g_mv_force_jitter}, {"sr_reactive_enable", &g_sr_reactive_enable}, {"sr_reactive_debug_view", &g_sr_reactive_debug_view}, {"msaa_resolve_enable", &g_msaa_resolve_enable}, {"msaa_resolve_average_depth", &g_msaa_resolve_average_depth}, { "msaa_coverage_enable",
            &g_msaa_coverage_enable }});
      Mcp::RegisterValues({{"rcas_sharpness", &g_rcas_sharpness, 0.f, 1.f}, {"smaa_pred_tolerance", &g_smaa_pred_tolerance, 0.002f, 0.2f}, {"sr_reactive_scale", &g_sr_reactive_scale, 0.f, 4.f},
         {"sr_reactive_threshold", &g_sr_reactive_threshold, 0.f, 1.f}});
      Mcp::RegisterTextures({MCP_GAME_TEXTURE("smaa.input", tex_input_encoded), MCP_GAME_TEXTURE("smaa.pred_mask", tex_pred), MCP_GAME_TEXTURE("smaa.output", tex_smaa_out),
         MCP_GAME_TEXTURE("mv.velocity", mv_texture), MCP_GAME_TEXTURE("mv.depth", mv_device_depth), MCP_GAME_TEXTURE("sr.reactive", mv_reactive),
         MCP_GAME_TEXTURE("sr.transparency", mv_transparency), MCP_GAME_TEXTURE("sr.draws_mask", mv_reactive_target)});
#endif

      // The canvas stays in gamma space: the HUD blends onto it right after TdMotionBlur. The tonemap pre-scales by
      // Game/UI paper white (UI_DRAW_TYPE 2), composition encodes scRGB.
      GetShaderDefineData(POST_PROCESS_SPACE_TYPE_HASH).SetDefaultValue('0');
      GetShaderDefineData(EARLY_DISPLAY_ENCODING_HASH).SetDefaultValue('0');
      GetShaderDefineData(VANILLA_ENCODING_TYPE_HASH).SetDefaultValue('1');
      GetShaderDefineData(GAMMA_CORRECTION_TYPE_HASH).SetDefaultValue('1');
      GetShaderDefineData(GAMUT_MAPPING_TYPE_HASH).SetDefaultValue('1');
      GetShaderDefineData(UI_DRAW_TYPE_HASH).SetDefaultValue('2');

      // dgVoodoo's translated shaders use b3 (its state) and b4 (the D3D9 constants)
      luma_settings_cbuffer_index = 13;
      luma_data_cbuffer_index = 12;
      luma_ui_cbuffer_index = -1;

      // Scene and UI paper white sliders (the UI one needs UI_DRAW_TYPE >= 1 and no OS reference white)
      use_os_reference_white_level = false;

      default_luma_global_game_settings.Exposure = 1.f;
      default_luma_global_game_settings.Contrast = 1.f;
      default_luma_global_game_settings.Saturation = 1.f;
      default_luma_global_game_settings.HighlightsDesaturation = 0.f;
      default_luma_global_game_settings.ColorGradingIntensity = 1.f;
      default_luma_global_game_settings.Dithering = 1.f;
      // Bink bypasses the scene passes: without AutoHDR the movies sit flat at paper white. BL2's calibrated pair.
      default_luma_global_game_settings.VideoAutoHDREnable = 1.f;
      default_luma_global_game_settings.VideoAutoHDRBoost = 0.5f;
      cb_luma_global_settings.GameSettings = default_luma_global_game_settings;
   }

   void OnCreateDevice(ID3D11Device* native_device, DeviceData& device_data) override
   {
      device_data.game = new MirrorsEdgeGameDeviceData;
      device_data.taa_detected = true; // No TAA to replace, but Core's upscaler status checks for it
#if DEVELOPMENT
      // For the MCP "luma_dev_values" tool: the last complete frame's counts
      const auto& stats = GetGameDeviceData(device_data).mv_last_stats;
      Mcp::RegisterCounters({{"mv.draws", &stats.motion_vector_draws}, {"mv.jitter_draws", &stats.jitter_draws}, {"mv.reactive_draws", &stats.reactive_draws}, {"mv.matched", &stats.matched},
                               {"mv.camera_only", &stats.camera_only}, {"mv.other_camera", &stats.other_camera}, {"mv.uncopied", &stats.uncopied}, {"mv.maps", &stats.maps}, {"mv.updates", &stats.updates},
                               {"mv.other_maps", &stats.other_maps}, {"mv.sr_draws", &stats.sr_draws}, {"mv.tiebreak_collisions", &stats.tiebreak_collisions}, {"mv.ended_by_hash", &stats.ended_by},
                               { "msaa_resolves",
                                  &stats.msaa_resolves }},
         &device_data);
      for (size_t i = 0; i < std::size(MOTION_VECTOR_REJECT_NAMES); i++)
      {
         Mcp::RegisterCounter(std::string("mv.rejected.") + MOTION_VECTOR_REJECT_NAMES[i], &stats.rejected[i], &device_data);
      }
#endif
   }

   void OnDestroyDeviceData(DeviceData& device_data) override
   {
#if DEVELOPMENT
      Mcp::Unregister(&device_data);
#endif
      // GameDeviceData lacks a virtual destructor; delete through the concrete type to release derived members
      delete static_cast<MirrorsEdgeGameDeviceData*>(device_data.game);
      device_data.game = nullptr;
   }

   void OnPresent(ID3D11Device* native_device, DeviceData& device_data) override
   {
      auto& gd = GetGameDeviceData(device_data);
      // SMAA's resources go once it stopped running, the snapshot too once RCAS stopped as well (MEM-11); between frames
      if (cb_luma_global_settings.FrameIndex - gd.smaa_frame > IDLE_RELEASE_FRAMES)
      {
         gd.ReleaseSMAAScratch();
         ReleaseSMAAIntermediates(device_data);
      }
      if (cb_luma_global_settings.FrameIndex - gd.snapshot_frame > IDLE_RELEASE_FRAMES)
      {
         gd.ReleaseSnapshotScratch();
      }
      if (gd.tex_smaa_out && cb_luma_global_settings.FrameIndex - gd.smaa_out_frame > IDLE_RELEASE_FRAMES)
      {
         gd.ReleaseSMAAOutput();
      }
      // Captured again at the next tonemap and TdMotionBlur; holding them would keep the game's targets alive across a resize
      gd.srv_scene_depth.reset();
      const bool msaa_was_active = gd.msaa_active;
      gd.msaa_active = std::exchange(gd.msaa_resolved, false);
      if (gd.msaa_active && !msaa_was_active)
      {
         reshade::log::message(reshade::log::level::warning, "[Mirror's Edge] The game's MSAA is on: DLAA, FSR, Luma TAA and SMAA T2x stay off until Anti-Aliasing is off in the game's display settings");
      }
      gd.canvas_res.reset();
      gd.open_refused_dsv = nullptr;

      // DLSS/FSR/Luma TAA: the history restarts after any frame it didn't draw (menus, loading, just picked); the selection and the
      // motion vector state are fixed here for the next frame (see "sr_active")
      gd.mv_jitter_allowed = device_data.has_drawn_sr;
      // Not on a multisampled scene (the motion vector draws refuse it): with MSAA the motion vector path stays off
      gd.sr_active = LatchSRFrame(device_data) && !gd.msaa_active;
      // SMAA T2x runs on the upscalers' motion vectors and gives way to an upscaler. Turned off, its frames go, and the motion vectors
      // go as when no upscaler is picked.
      gd.t2x_phase = -1;
      gd.mv_filled = false;
      const bool t2x_was_active = gd.t2x_active;
      gd.t2x_active = g_smaa_enable && g_smaa_t2x && !gd.sr_active && !gd.msaa_active;
      if (t2x_was_active && !gd.t2x_active)
      {
         gd.ReleaseT2xFrames();
         gd.release_sr_resources = true;
      }
      if (gd.t2x_frames[0] && cb_luma_global_settings.FrameIndex - gd.t2x_frame > IDLE_RELEASE_FRAMES)
      {
         gd.ReleaseT2xFrames();
      }
      gd.mv_active = (gd.sr_active || gd.t2x_active || g_mv_enable) && !gd.msaa_active;
      // None picked: Core stopped the SR bridge's helper ("ReleaseResources"), our upscaler inputs and output go too. Recreated when an
      // upscaler is picked again (the helper takes seconds to start).
      if (device_data.sr_type == SR::Type::None && gd.release_sr_resources.exchange(false))
      {
         gd.sr_output_srv.reset();
         if (!g_mv_enable && !gd.t2x_active)
         {
            const std::unique_lock lock(gd.mv_mutex);
            gd.ReleaseMotionVectorTargets();
            // The game's scene, depth and scene copy (taken again at the next scene), so a resize after None doesn't keep the old ones
            // alive; and the vc4 copies the object tables and the cameras hold
            gd.mv_depth.reset();
            gd.mv_scene_color.reset();
            gd.mv_scene_rtv.reset();
            gd.mv_scene_srv.reset();
            gd.mv_scene_copy.reset();
            gd.mv_scene_copy_rtv.reset();
            gd.mv_objects.clear();
            gd.mv_previous_objects.clear();
            gd.mv_camera.reset();
            gd.mv_previous_camera.reset();
            gd.mv_fill_buffer.reset();
            gd.mv_jitter_buffer.reset();
            gd.mv_previous_constants = {}; // Its 4 MiB dynamic ring is address space too
            gd.mv_constants_pool.clear();
            gd.mv_constants_pool_free.clear();
         }
      }
      {
         // The pooled vc4 copies only the pool holds (superseded, no object or camera keeps them) are free for the next ones, as many as
         // the last frame asked for: frames without a scene (loading, videos, menus) would keep their peak otherwise. Without motion
         // vectors, none.
         size_t kept_free = 0;
         if (gd.mv_active)
         {
            kept_free = std::exchange(gd.mv_constants_made, 0);
         }
         std::erase_if(gd.mv_constants_pool, [&](const auto& copy)
            {
               if (copy.use_count() != 1)
                  return false;
               if (kept_free == 0)
                  return true;
               kept_free--;
               return false; });
         gd.mv_constants_pool_free.clear();
         for (uint32_t i = 0; i < uint32_t(gd.mv_constants_pool.size()); i++)
         {
            if (gd.mv_constants_pool[i].use_count() == 1)
            {
               gd.mv_constants_pool_free.push_back(i);
            }
         }
      }
      // The reactive masks go once the fill stopped writing them (DLSS, the masks off, SMAA), like SMAA's resources above
      if (gd.mv_reactive_target && cb_luma_global_settings.FrameIndex - gd.sr_reactive_frame > IDLE_RELEASE_FRAMES)
      {
         const std::unique_lock lock(gd.mv_mutex);
         gd.ReleaseReactiveMasks();
      }
      // A scene no post pass ended ends here: its jitter must not reach the next frame's draws before its first mesh
      gd.mv_scene_open = false;
      gd.mv_scene_done = false;
      gd.mv_frame_ended = true;
      gd.mv_fill_pending = false;
      // -1 at native resolution once the upscaler draws (Core biases the game's anisotropic samplers, the trilinear ones upgraded to
      // AF16x); none while the bridge's helper starts (SMAA on the unjittered scene)
      SetTextureMipLodBias(nullptr, device_data, ((gd.sr_active && gd.mv_jitter_allowed) ? SR::GetMipLODBias(device_data.output_resolution.y, device_data.output_resolution.y) : 0.f));
#if DEVELOPMENT
      gd.mv_last_stats = std::exchange(gd.mv_stats, {});
      // "MV Debug View": Core's debug draw of the target, absolute values in pixels
      {
         const std::shared_lock lock(gd.mv_mutex);
         if (g_mv_debug_view && gd.mv_texture)
         {
            D3D11_TEXTURE2D_DESC desc;
            gd.mv_texture->GetDesc(&desc);
            debug_draw_auto_clear_texture = false;
            debug_draw_options |= (uint32_t)DebugDrawTextureOptionsMask::Abs | (uint32_t)DebugDrawTextureOptionsMask::UVToPixelSpace;
            device_data.debug_draw_texture = gd.mv_texture.get();
            device_data.debug_draw_texture_format = desc.Format;
            device_data.debug_draw_texture_size = {desc.Width, desc.Height, 1, 1};
         }
         else if (g_sr_reactive_debug_view && gd.mv_reactive)
         {
            debug_draw_auto_clear_texture = false;
            debug_draw_options |= (uint32_t)DebugDrawTextureOptionsMask::RedOnly;
            debug_draw_options &= ~((uint32_t)DebugDrawTextureOptionsMask::Abs | (uint32_t)DebugDrawTextureOptionsMask::UVToPixelSpace);
            device_data.debug_draw_texture = gd.mv_reactive.get();
            device_data.debug_draw_texture_format = DXGI_FORMAT_R8_UNORM;
            device_data.debug_draw_texture_size = {uint32_t(device_data.output_resolution.x), uint32_t(device_data.output_resolution.y), 1, 1};
         }
         else if (device_data.debug_draw_texture && (device_data.debug_draw_texture.get() == gd.mv_texture.get() || device_data.debug_draw_texture.get() == gd.mv_reactive.get()))
         {
            device_data.debug_draw_texture = nullptr;
         }
      }
#endif
   }

   void CleanExtraSRResources(DeviceData& device_data) override
   {
      GetGameDeviceData(device_data).release_sr_resources = true;
   }

   void LoadConfigs() override
   {
      reshade::get_config_value(nullptr, NAME, "SMAAEnable", g_smaa_enable);
      reshade::get_config_value(nullptr, NAME, "SMAAT2x", g_smaa_t2x);
      reshade::get_config_value(nullptr, NAME, "RCASSharpness", g_rcas_sharpness);
      reshade::get_config_value(nullptr, NAME, "SMAAPredication", g_smaa_predication);
      reshade::get_config_value(nullptr, NAME, "SMAAPredicationTolerance", g_smaa_pred_tolerance);
      reshade::get_config_value(nullptr, NAME, "MSAAResolveEnable", g_msaa_resolve_enable);
      auto& gs = cb_luma_global_settings.GameSettings;
      reshade::get_config_value(nullptr, NAME, "Exposure", gs.Exposure);
      reshade::get_config_value(nullptr, NAME, "Contrast", gs.Contrast);
      reshade::get_config_value(nullptr, NAME, "Saturation", gs.Saturation);
      reshade::get_config_value(nullptr, NAME, "HighlightsDesaturation", gs.HighlightsDesaturation);
      reshade::get_config_value(nullptr, NAME, "ColorGradingIntensity", gs.ColorGradingIntensity);
      reshade::get_config_value(nullptr, NAME, "Dithering", gs.Dithering);
      reshade::get_config_value(nullptr, NAME, "VideoAutoHDREnable", gs.VideoAutoHDREnable);
      reshade::get_config_value(nullptr, NAME, "VideoAutoHDRBoost", gs.VideoAutoHDRBoost);
   }

   void DrawImGuiSettings(DeviceData& device_data) override
   {
      ImGui::SeparatorText("Anti-Aliasing");
      const auto& gd = GetGameDeviceData(device_data);
      if (gd.msaa_active)
      {
         ImGui::TextColored(ImVec4(1.f, 0.8f, 0.f, 1.f), "MSAA is on: DLAA, FSR, Luma TAA and SMAA T2x need Anti-Aliasing off in the game's display settings.");
      }
      // A disabled checkbox shows "off" through this, the saved choice kept
      bool off = false;
      // Only does something with the game's MSAA
      ImGui::BeginDisabled(!gd.msaa_active);
      if (ImGui::Checkbox("Luma MSAA Enable", gd.msaa_active ? &g_msaa_resolve_enable : &off))
      {
         reshade::set_config_value(nullptr, NAME, "MSAAResolveEnable", g_msaa_resolve_enable);
      }
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
      {
         ImGui::SetTooltip("Replaces the game's MSAA with an HDR-aware one (smoother edges on bright lights, sky, foliage and fences; requires Anti-Aliasing enabled in the game's display settings; not used with DLSS/FSR or Luma TAA).");
      }
      ImGui::EndDisabled();
      // The upscaler (Super Resolution, in the Settings tab) replaces SMAA. Not with MSAA ("sr_active"): SMAA runs then
      ImGui::BeginDisabled(gd.sr_active);
      if (ImGui::Checkbox("SMAA Enable", gd.sr_active ? &off : &g_smaa_enable))
      {
         reshade::set_config_value(nullptr, NAME, "SMAAEnable", g_smaa_enable);
      }
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
      {
         ImGui::SetTooltip("Adds SMAA anti-aliasing on top of the game's own (most noticeable with Anti-Aliasing off in the game's display settings; not used with DLSS/FSR or Luma TAA).");
      }
      ImGui::EndDisabled();
      // T2x needs the motion vectors, which a multisampled scene doesn't get
      const bool t2x_unavailable = gd.sr_active || gd.msaa_active;
      ImGui::BeginDisabled(!g_smaa_enable || t2x_unavailable);
      if (ImGui::Checkbox("SMAA T2x", t2x_unavailable ? &off : &g_smaa_t2x))
      {
         reshade::set_config_value(nullptr, NAME, "SMAAT2x", g_smaa_t2x);
      }
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
      {
         ImGui::SetTooltip("Smoother edges and less shimmering, by blending each frame with the previous one.\nCan look slightly softer in motion. Not used with DLSS/FSR, Luma TAA or the game's MSAA (Anti-Aliasing in the game's display settings).");
      }
      ImGui::EndDisabled();
      ImGui::BeginDisabled(!g_smaa_enable && !gd.sr_active);
      if (ImGui::SliderFloat("RCAS Sharpness", &g_rcas_sharpness, 0.f, 1.f))
      {
         reshade::set_config_value(nullptr, NAME, "RCASSharpness", g_rcas_sharpness);
      }
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
      {
         ImGui::SetTooltip("Sharpening applied on top of SMAA, DLSS/FSR or Luma TAA (0 = off).");
      }
      DrawResetButton(g_rcas_sharpness, 0.f, "RCASSharpness");
      ImGui::EndDisabled();
#if DEVELOPMENT
      if (ImGui::Checkbox("SMAA Predication", &g_smaa_predication))
      {
         reshade::set_config_value(nullptr, NAME, "SMAAPredication", g_smaa_predication);
      }
      if (ImGui::SliderFloat("SMAA Predication Tolerance", &g_smaa_pred_tolerance, 0.002f, 0.2f, "%.3f", ImGuiSliderFlags_Logarithmic))
      {
         reshade::set_config_value(nullptr, NAME, "SMAAPredicationTolerance", g_smaa_pred_tolerance);
      }
      DrawResetButton(g_smaa_pred_tolerance, 0.02f, "SMAAPredicationTolerance");

      ImGui::SeparatorText("Motion Vectors (DLSS/FSR/Luma TAA)");
      ImGui::Checkbox("MV Enable", &g_mv_enable);
      if (ImGui::IsItemHovered())
      {
         ImGui::SetTooltip("Draws the scene with the motion vector shaders without an upscaler. The image must not change; the debug view is\nblack with a static camera and lights up only what moves. Not saved.");
      }
      ImGui::Checkbox("MV Debug View", &g_mv_debug_view);
      ImGui::Checkbox("MV Force Jitter", &g_mv_force_jitter);
      ImGui::Checkbox("Reactive Mask", &g_sr_reactive_enable);
      ImGui::Checkbox("Reactive Mask Debug View", &g_sr_reactive_debug_view);
#endif

      auto& gs = cb_luma_global_settings.GameSettings;
      const auto& gs_def = default_luma_global_game_settings;
      ImGui::SeparatorText("Grade");

      const auto slider = [&](const char* label, float* value, float default_value, const char* key, float max_value, const char* tooltip)
      {
         if (ImGui::SliderFloat(label, value, 0.f, max_value))
         {
            reshade::set_config_value(nullptr, NAME, key, *value);
            device_data.cb_luma_global_settings_dirty = true;
         }
         if (ImGui::IsItemHovered())
         {
            ImGui::SetTooltip("%s", tooltip);
         }
         if (DrawResetButton(*value, default_value, key))
         {
            device_data.cb_luma_global_settings_dirty = true;
         }
      };

      slider("Exposure", &gs.Exposure, gs_def.Exposure, "Exposure", 2.f, "Overall image brightness (1 = vanilla).");
      slider("Contrast", &gs.Contrast, gs_def.Contrast, "Contrast", 2.f, "Overall image contrast, HDR only (1 = vanilla).");
      slider("Saturation", &gs.Saturation, gs_def.Saturation, "Saturation", 2.f, "Color saturation, HDR only (1 = vanilla).");
      slider("Highlights Desaturation", &gs.HighlightsDesaturation, gs_def.HighlightsDesaturation, "HighlightsDesaturation", 1.f,
         "How far the brightest sources fade to neutral white, HDR only (0 = keep color at any brightness).");
      slider("Color Grading Intensity", &gs.ColorGradingIntensity, gs_def.ColorGradingIntensity, "ColorGradingIntensity", 1.f,
         "Strength of the game's own color grading (1 = vanilla, 0 = neutral).");

      // Float backed 0/1 toggles (cbuffer fields)
      const auto toggle = [&](const char* label, float* value, float default_value, const char* key, const char* tooltip)
      {
         bool enabled = *value > 0.5f;
         if (ImGui::Checkbox(label, &enabled))
         {
            *value = (enabled ? 1.f : 0.f);
            reshade::set_config_value(nullptr, NAME, key, *value);
            device_data.cb_luma_global_settings_dirty = true;
         }
         if (ImGui::IsItemHovered())
         {
            ImGui::SetTooltip("%s", tooltip);
         }
         if (DrawResetButton(*value, default_value, key))
         {
            device_data.cb_luma_global_settings_dirty = true;
         }
      };

      ImGui::SeparatorText("Effects");
      toggle("Video AutoHDR", &gs.VideoAutoHDREnable, gs_def.VideoAutoHDREnable, "VideoAutoHDREnable", "Adds HDR highlights to pre-rendered videos (HDR only).");
      if (gs.VideoAutoHDREnable > 0.5f)
      {
         slider("Video HDR Boost", &gs.VideoAutoHDRBoost, gs_def.VideoAutoHDRBoost, "VideoAutoHDRBoost", 1.f, "Video highlight strength (0 = off).");
      }
      toggle("Dithering", &gs.Dithering, gs_def.Dithering, "Dithering", "Reduces gradient banding.");

      ImGui::SeparatorText("UI");
      ImGui::Checkbox("Hide Gameplay UI", &g_hide_ui);
      if (ImGui::IsItemHovered())
      {
         ImGui::SetTooltip("Disables the in-game UI.");
      }
   }

   DrawOrDispatchOverrideType OnDrawOrDispatch(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, CommandListData& cmd_list_data, DeviceData& device_data, reshade::api::shader_stage stages, const ShaderHashesList<OneShaderPerPipeline>& original_shader_hashes, bool is_custom_pass, bool& updated_cbuffers, std::function<void()>* original_draw_dispatch_func) override
   {
      const bool is_immediate = native_device_context->GetType() == D3D11_DEVICE_CONTEXT_IMMEDIATE;
      auto& gd = GetGameDeviceData(device_data);
      // Render target 0's resource (null without one)
      const auto bound_render_target_resource = [&]
      {
         com_ptr<ID3D11RenderTargetView> rtv;
         native_device_context->OMGetRenderTargets(1, &rtv, nullptr);
         com_ptr<ID3D11Resource> resource;
         if (rtv)
         {
            rtv->GetResource(&resource);
         }
         return resource;
      };
      if (is_immediate && original_shader_hashes.Contains(shader_hashes_TdMotionBlur))
      {
         gd.canvas_res = bound_render_target_resource();
      }
      // Hide UI: cancel this frame's later draws into the canvas (the HUD, menus and overlays; "canvas_res" only lives from TdMotionBlur
      // to the present). The UI shaders are patched, so "is_custom_pass" is true for them: an original pixel shader hash is what
      // separates a game draw from Luma's own passes
      else if (g_hide_ui && is_immediate && gd.canvas_res && (!is_custom_pass || original_shader_hashes.pixel_shaders[0] != UINT64_MAX))
      {
         // Any view of the canvas
         if (bound_render_target_resource() == gd.canvas_res)
            return DrawOrDispatchOverrideType::Replaced;
      }

      // MSAA (4x and up, as Saints Row: The Third's: 2x gives a coarser edge than the alpha test): the masked materials' draws into
      // the multisampled scene with their alpha to coverage version (see "PatchPixelShaderCoverage"), the game's shader put back after
      // Ahead of the motion vector path, which refuses multisampled scenes anyway
      if (g_msaa_resolve_enable && g_msaa_coverage_enable && gd.msaa_active && is_immediate && !is_custom_pass && original_draw_dispatch_func && *original_draw_dispatch_func && original_shader_hashes.pixel_shaders[0] != UINT64_MAX)
      {
         ID3D11PixelShader* const coverage_shader = GetPatchedShader<ID3D11PixelShader>(native_device, device_data, uint32_t(original_shader_hashes.pixel_shaders[0]), cmd_list_data.pipeline_state_original_pixel_shader, PixelShaderPatch::COVERAGE).shader.get();
         if (coverage_shader)
         {
            com_ptr<ID3D11Texture2D> rt_texture;
            D3D11_TEXTURE2D_DESC rt_desc = {};
            if (const com_ptr<ID3D11Resource> rt = bound_render_target_resource(); rt && SUCCEEDED(rt->QueryInterface(&rt_texture)))
            {
               rt_texture->GetDesc(&rt_desc);
            }
            if (rt_desc.SampleDesc.Count >= 4)
            {
               com_ptr<ID3D11PixelShader> game_shader;
               native_device_context->PSGetShader(&game_shader, nullptr, nullptr);
               native_device_context->PSSetShader(coverage_shader, nullptr, 0);
               (*original_draw_dispatch_func)();
               native_device_context->PSSetShader(game_shader.get(), nullptr, 0);
               return DrawOrDispatchOverrideType::Replaced;
            }
         }
      }

      // DLAA/FSR/Luma TAA and SMAA T2x: the scene's draws with motion vectors or jitter (see "DrawWithMotionVectors"), and the fill and
      // upscaler at its first post pass
      if (gd.mv_active && is_immediate)
      {
         // A game draw (not Luma's) with a vertex shader, which the patched shaders can redo
         const bool game_mesh_draw = !is_custom_pass && original_draw_dispatch_func && *original_draw_dispatch_func && (stages & reshade::api::shader_stage::vertex) == reshade::api::shader_stage::vertex;
         if (!gd.mv_scene_done && original_shader_hashes.Contains(shader_hashes_ScenePost))
         {
            if (gd.mv_scene_open)
            {
#if DEVELOPMENT
               gd.mv_stats.ended_by = uint32_t(original_shader_hashes.pixel_shaders[0]);
#endif
               // A copy of the scene (same description, CopyResource compatible) among the pass's textures
               gd.mv_scene_copy.reset();
               if (gd.mv_scene_color)
               {
                  com_ptr<ID3D11ShaderResourceView> srvs[8];
                  native_device_context->PSGetShaderResources(0, 8, &srvs[0]);
                  for (const auto& srv : srvs)
                  {
                     com_ptr<ID3D11Resource> resource;
                     if (srv)
                     {
                        srv->GetResource(&resource);
                     }
                     if (resource && resource != gd.mv_scene_color && AreResourcesEqual(resource.get(), gd.mv_scene_color.get()))
                     {
                        gd.mv_scene_copy = resource;
                        break;
                     }
                  }
               }
               EndScene(native_device, native_device_context, device_data);
            }
         }
         // Until the scene opens only the depth view matters (the shadow maps and the menu's 1280x720 pass come first)
         else if (!gd.mv_scene_done && !gd.mv_scene_open && game_mesh_draw)
         {
            com_ptr<ID3D11DepthStencilView> scene_dsv;
            native_device_context->OMGetRenderTargets(0, nullptr, &scene_dsv);
            if (scene_dsv && scene_dsv.get() != gd.open_refused_dsv)
            {
               OpenScene(native_device, native_device_context, cmd_list_data, device_data, original_shader_hashes.vertex_shaders[0], scene_dsv.get());
            }
         }
         if (gd.mv_scene_open && game_mesh_draw)
         {
            com_ptr<ID3D11RenderTargetView> rtvs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
            com_ptr<ID3D11DepthStencilView> dsv;
            native_device_context->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, &rtvs[0], &dsv);
            const bool motion_vectors = DrawWithMotionVectors(native_device, native_device_context, cmd_list_data, device_data, original_shader_hashes, *original_draw_dispatch_func, rtvs, dsv.get());
            // An alpha blending state blends the motion vector target by the alpha written there: a draw that doesn't write it (refused
            // above) must not have it bound
            if (!motion_vectors && rtvs[MotionVectorPatches::target_slot] && rtvs[MotionVectorPatches::target_slot] == gd.mv_rtv)
            {
               ClassifyBoundBlend(native_device_context, &gd);
               if (gd.mv_alpha_blended)
               {
                  rtvs[MotionVectorPatches::target_slot].reset();
                  ID3D11RenderTargetView* targets[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
                  for (UINT slot = 0; slot < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; slot++)
                  {
                     targets[slot] = rtvs[slot].get();
                  }
                  native_device_context->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, targets, dsv.get());
               }
            }
            const bool jitter = !motion_vectors && DrawWithJitter(native_device, native_device_context, cmd_list_data, device_data, original_shader_hashes, *original_draw_dispatch_func, rtvs, dsv.get());
#if DEVELOPMENT
            Mcp::Annotate(cmd_list_data, motion_vectors ? "mv" : (jitter ? "jitter" : "unpatched"));
            if (const MotionVectorReject reject = std::exchange(gd.mv_draw_reject, REJECT_COUNT); reject != REJECT_COUNT)
            {
               Mcp::Annotate(cmd_list_data, "mv_reject", int(reject));
            }
#endif
            if (motion_vectors || jitter)
               return DrawOrDispatchOverrideType::Replaced;
         }
      }
      // Every draw without a patched shader: the game's own, if the last patched draw's are still bound
      if (is_immediate)
      {
         PatchedDraws::RestoreGameShader(native_device_context, &gd.mv_bound_vertex_shader);
         PatchedDraws::RestoreGameShader(native_device_context, &gd.mv_bound_pixel_shader);
      }

#if DEVELOPMENT
      // Census of the passes drawn onto the canvas after TdToneMapping (UI and LDR overlays): each pixel shader logged once, to find the
      // ones the clamp lists above miss. Keyed on the tonemap's frame, not "has_drawn_main_post_processing": the menu's 1280x720 scene
      // ends in GammaCorrection before the main scene is drawn.
      static std::atomic<uint32_t> tonemap_frame = UINT32_MAX;
#endif
      if (const bool tonemap = original_shader_hashes.Contains(shader_hashes_ToneMapping); tonemap || original_shader_hashes.Contains(shader_hashes_GammaCorrection))
      {
         device_data.has_drawn_main_post_processing = true;
#if DEVELOPMENT
         if (tonemap)
         {
            tonemap_frame = cb_luma_global_settings.FrameIndex;
         }
#endif
         if (tonemap && is_immediate)
         {
            gd.srv_exposure.reset();
            native_device_context->PSGetShaderResources(3, 1, gd.srv_exposure.put());
         }
         // TdToneMapping: run it, then SMAA on its target. The bindings are read here, before any "is_custom_pass" gate: the
         // replaced tonemap is a custom pass
         const bool antialias = (device_data.has_drawn_sr ? g_rcas_sharpness > 0.f : g_smaa_enable);
         if (antialias && tonemap && is_immediate && original_draw_dispatch_func != nullptr)
         {
            ComPtr<ID3D11RenderTargetView> rtv;
            native_device_context->OMGetRenderTargets(1, rtv.put(), nullptr);
            // The scene (linear depth in .a) for the predication; the tonemap only reads it
            gd.srv_scene_depth.reset();
            native_device_context->PSGetShaderResources(0, 1, gd.srv_scene_depth.put());
            // Returning Replaced skips Core's per-pass SetLumaConstantBuffers: upload them here or the draw reads last frame's values
            SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, reshade::api::shader_stage::pixel, LumaConstantBufferType::LumaSettings);
            updated_cbuffers = true;
            (*original_draw_dispatch_func)();
            if (rtv)
            {
               // One state save and restore around the whole chain (its passes bind their own, see "RunPostTonemapSMAA")
               DrawStateStack<DrawStateStackType::FullGraphics> graphics_state;
               DrawStateStack<DrawStateStackType::Compute> compute_state;
               graphics_state.Cache(native_device_context, device_data.uav_max_count);
               compute_state.Cache(native_device_context, device_data.uav_max_count);
               RunPostTonemapSMAA(native_device, native_device_context, device_data, &gd, rtv.get(), !device_data.has_drawn_sr);
               compute_state.Restore(native_device_context);
               graphics_state.Restore(native_device_context);
            }
            return DrawOrDispatchOverrideType::Replaced;
         }
      }
#if DEVELOPMENT
      else if (tonemap_frame == cb_luma_global_settings.FrameIndex && (stages & reshade::api::shader_stage::pixel) != 0)
      {
         static std::shared_mutex census_mutex;
         static std::unordered_set<uint32_t> census;
         const uint32_t hash = original_shader_hashes.pixel_shaders[0];
         const std::unique_lock lock(census_mutex);
         if (const auto [it, inserted] = census.insert(hash); inserted)
         {
            reshade::log::message(reshade::log::level::info,
               std::format("[Mirror's Edge] Post-tonemap PS 0x{:08X} ({}), frame {}", hash, (ui_saturated_pixel_shaders.contains(hash) ? "saturated" : (scene_floored_pixel_shaders.contains(hash) ? "floored" : "UNCLAMPED")), cb_luma_global_settings.FrameIndex).c_str());
         }
      }
#endif
      return DrawOrDispatchOverrideType::None;
   }

   void PrintImGuiAbout() override
   {
      ImGui::PushTextWrapPos(0.f);
      ImGui::Text(
         "Luma for \"Mirror's Edge\" is developed by DristoforColumb and is open source and free.\n"
         "It adds HDR, DLAA, FSR 3 native anti-aliasing or Luma TAA, SMAA (with T2x) and an HDR-aware MSAA with alpha to coverage, plus 16x anisotropic filtering.\n"
         "It runs through dgVoodoo2 (DirectX 9 -> 11).\n"
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
                  "\nMacLeod-Boynton hue emulation (RenoDX)"
                  "\nSMAA (Iryoku)"
                  "\nAMD FidelityFX (RCAS + FSR 3)"
                  "\nNVIDIA NGX (DLSS)"
                  "\ndgVoodoo2 by Dege (DirectX 9 -> 11 wrapper, required)");
   }
};

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
   if (ul_reason_for_call == DLL_PROCESS_ATTACH)
   {
      const char* project_name = PROJECT_NAME;
      const char* cleared_project_name = ((project_name[0] == '_') ? (project_name + 1) : project_name);

      uint32_t mod_version = 1;
      Globals::SetGlobals(cleared_project_name, "Mirror's Edge Luma mod", "", mod_version);

      swapchain_format_upgrade_type = TextureFormatUpgradesType::AllowedEnabled;
      swapchain_upgrade_type = SwapchainUpgradeType::scRGB;

      // "force_borderless" also covers leaving fullscreen, so the window can't come back with a title bar (TW2)
      prevent_fullscreen_state = true;
      force_borderless = true;

      // The scene is already fp16 (alpha = linear depth); TdToneMapping writes fp16 too, then TdMotionBlur and the HUD
      // draw into the r8g8b8a8_typeless canvas that is copied to the swapchain: that canvas is the only HDR clip.
      // Indirect (mirror substituted at bind): changing a dgVoodoo resource's creation format breaks its bookkeeping
      // (black screen, MOHA/BL2). Upgrading the A16B16G16R16 UNORM exposure and bloom buffers is optional (HDR bloom);
      // the passes reading them then need the UNORM [0, 1] clamp back where vanilla relies on it.
      texture_format_upgrades_type = TextureFormatUpgradesType::AllowedEnabled;
      enable_indirect_texture_format_upgrades = true;
      enable_chain_indirect_texture_format_upgrades = ChainTextureFormatUpgradesType::DirectDependencies;
      texture_upgrade_formats = {
         reshade::api::format::r8g8b8a8_typeless,
      };
      // "No1Px" as in the other dgVoodoo mods: the wrapper binds 1x1 placeholders in unused sampler slots, which pass the aspect filter
      texture_format_upgrades_2d_size_filters = (uint32_t)TextureFormatUpgrades2DSizeFilters::SwapchainResolution | (uint32_t)TextureFormatUpgrades2DSizeFilters::SwapchainAspectRatio | (uint32_t)TextureFormatUpgrades2DSizeFilters::No1Px;

      // The upscalers' mip bias (see "OnPresent") reaches only upgraded samplers. AF16x, as in the other UE3 dgVoodoo mods;
      // force_upgrade_linear_samplers also gives the trilinear ones the bias (Core rewrites only anisotropic samplers otherwise, TW2).
      enable_samplers_upgrade = true; // boot-time only (cannot be changed after device creation)
      samplers_upgrade_mode = 4;
      force_upgrade_linear_samplers = true;

#if DEVELOPMENT
      // Offline map of the global post/UI/video pixel shaders (cooked RefShaderCache type names), for traces and the DevKit.
      // Each name twice: 2.87.5, then 2.81.3
      for (const auto& [hash, name] : std::initializer_list<std::pair<const char*, const char*>>{
              {"19BCE666", "Td Directional Haze"},
              {"9C77EAD7", "Td Directional Haze"},
              {"2BE8B514", "DOF And Bloom Gather"},
              {"587465D1", "DOF And Bloom Gather"},
              {"B977A1C4", "DOF And Bloom Gather (Max Samples)"},
              {"69A1F64A", "DOF And Bloom Gather (Max Samples)"},
              {"780BCE69", "DOF And Bloom Blend"},
              {"8B012337", "DOF And Bloom Blend"},
              {"25752EDB", "Td Tone Map Exposure"},
              {"9F2AF349", "Td Tone Map Exposure"},
              {"1A760388", "Td Tone Mapping"},
              {"602DA054", "Td Tone Mapping"},
              {"0160196C", "Gamma Correction"},
              {"87D136D3", "Gamma Correction"},
              {"FC10C0BD", "Uber Post Process Blend"},
              {"39B803FA", "Uber Post Process Blend"},
              {"8F95BB29", "Motion Blur"},
              {"37EF3D5C", "Motion Blur"},
              {"FF77A7B0", "Motion Blur (Dynamic Velocities Only)"},
              {"E9224963", "Motion Blur (Dynamic Velocities Only)"},
              {"4177BC6E", "Td Motion Blur"},
              {"A99AD9DD", "Td Motion Blur"},
              {"C1095A48", "Td Contrast Setting"},
              {"8FC13045", "Td Contrast Setting"},
              {"50D42156", "Td Interpolate"},
              {"A9884E2F", "Td Interpolate"},
              {"BA7480CD", "Td Clear"},
              {"75EB6E89", "Td Clear"},
              {"F66C72BC", "Distortion Apply Screen"},
              {"B414DB90", "Distortion Apply Screen"},
              {"A5AC2B70", "Td UI Compositing"},
              {"FB0A84E8", "Td UI Compositing"},
              {"92F6FB51", "Td UI Blur Effect"},
              {"49DB6B1C", "Td UI Blur Effect"},
              {"E93BE915", "Td UI Blur Gather"},
              {"A288BD8B", "Td UI Blur Gather"},
              {"C58A89A8", "Td UI Blur Gather (Max Samples)"},
              {"C086B17E", "Td UI Blur Gather (Max Samples)"},
              {"F8554FA4", "Td UI Approximate Alpha"},
              {"643DB945", "Td UI Approximate Alpha"},
              {"088F76F5", "Simple Element"},
              {"889B0C84", "Simple Element"},
              {"D81C8836", "Simple Element Gamma"},
              {"3DFF6336", "Simple Element Gamma"},
              {"C39EFBF2", "Simple Element Masked Gamma"},
              {"5325C38B", "Simple Element Masked Gamma"},
              {"E41621CF", "Bink YCrCb To RGB"},
              {"7EBF990A", "Bink YCrCb To RGB"},
              {"B19ED21F", "Bink YCrCb To RGB (Alpha)"},
              {"C430F4E0", "Bink YCrCb To RGB (Alpha)"},
              {"39D9728E", "Ambient Occlusion (High Quality)"},
              {"AB839F6F", "Ambient Occlusion (High Quality)"},
              {"63885863", "Ambient Occlusion (Low Quality)"},
              {"C8D0904C", "Ambient Occlusion (Low Quality)"},
              {"F53BEC3C", "AO Apply"},
              {"2BD8E1EC", "AO Apply"},
              {"24BBD0BD", "AO History Update"},
              {"B8DAA094", "AO History Update"},
              {"9A32CD46", "Downsample Depth"},
              {"3A3184FB", "Downsample Depth"},
           })
      {
         forced_shader_names.emplace(Shader::Hash_StrToNum(hash), name);
      }
#endif

      game = new MirrorsEdge();
   }
   else if (ul_reason_for_call == DLL_PROCESS_DETACH)
   {
      MirrorsEdge::SetEvents(false);
   }

   CoreMain(hModule, ul_reason_for_call, lpReserved);

   return TRUE;
}
