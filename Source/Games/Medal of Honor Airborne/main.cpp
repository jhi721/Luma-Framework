// Medal of Honor: Airborne - Luma HDR mod (Unreal Engine 3 2007, 32-bit, DX9 -> D3D11 via dgVoodoo2).
// Hashes are dgVoodoo-TRANSLATED and change per wrapper build: 2.87.3 and 2.81.3 are keyed (2.81.3 emits ps_4_0),
// any other build needs a re-dump.
// The post chain is fp16 with scene DEPTH in the alpha. No motion vectors or jitter of its own: DLAA/FSR's are built by
// patched shaders (see MotionVectorPatches.h), as in Mass Effect 2007 under the same wrapper.
// One of two final colour passes ends the frame (with DoF on: DoF composite + bloom + grade + output gamma) into an
// 8-bit canvas the HUD blends onto; its two saturate()s clip 19.4% of a bright frame, which the extended HDR grade
// keeps.

// No DEVELOPMENT auto-debugger MessageBox on DLL attach: invisible under borderless/fullscreen and it blocks the
// loader (ReShade times out the addon load -> error 1114). Same failure as BL2/TW2 under dgVoodoo.
#define DISABLE_AUTO_DEBUGGER 1

#define GAME_MEDAL_OF_HONOR_AIRBORNE 1

#define ENABLE_NGX 0 // NGX is x64-only and the game is 32-bit: DLSS and FSR run in the SR bridge's x64 helper
#define ENABLE_FIDELITY_SK 0
#define GEOMETRY_SHADER_SUPPORT 0
// The game ships no AA at all (no option, no post AA pass in the dump, sampleCount 1 everywhere), so SMAA adds
// rather than replaces. Core auto-registers the 6 "SMAA ..." passes from Luma_SMAA_impl.hlsl.
#define ENABLE_SMAA 1
// SMAA's area texture without the U-shape smoothing (see Luma_SMAA_impl.hlsl)
#define SMAA_SMOOTH_U_SHAPES 0
// Luma's multi-scale HDR bloom pyramid REPLACES the game's own quarter-res bright-pass glow (which the replaced
// gather pass then stops writing). Core auto-registers the 4 "Bloom ..." passes from Luma_Bloom_impl.hlsl.
#define ENABLE_BLOOM 1
// SMAA runs POST-final-grade, before the HUD draws on the canvas, via the post-draw callback. Outside DEVELOPMENT
// this define is what makes "original_draw_dispatch_func" non-null; without it the callback silently never fires.
// The motion vector draws redo the game's through it too.
#define ENABLE_POST_DRAW_DISPATCH_CALLBACK 1
// The motion vector draw key reads the draw's arguments ("last_draw_dispatch_data")
#define ENABLE_DRAW_DISPATCH_DATA_CACHE 1

#include "..\..\Core\core.hpp"
#include "MotionVectorPatches.h"
#include "..\..\Core\includes\patched_draws.h"
#include <shellapi.h> // ShellExecuteA for About links (system() hangs the render thread in exclusive fullscreen)

// The two shaders that can end the frame, both replaced; "bAllowDepthOfField" in MOHASettings.ini picks which one.
// Separate register maps, hence two replacements - missing one puts gamma-encoded SDR into the scRGB swapchain.
static constexpr uint32_t kUberPostHash = 0xB9548800;        // UberPost_0xB9548800.ps_5_0.hlsl
static constexpr uint32_t kGammaCorrectionHash = 0x52B868E0; // GammaCorrection_0x52B868E0.ps_5_0.hlsl

// UE3 DOFAndBloomGather, REPLACED: it writes the game's bloom and its DoF blur SUMMED into one quarter-res target,
// so the vanilla glow can only be switched off inside this shader — anything downstream takes DoF with it.
static constexpr uint32_t kDofBloomGatherHash = 0x33BD72CF;

// The same four passes under dgVoodoo 2.81.3, which emits ps_4_0 and therefore different hashes. Dump-verified as
// signature-identical (same interpolators, t/s registers, cb4 rows), so replacements and slot captures are shared.
static constexpr uint32_t kUberPostHash_v281 = 0x32F77C2B;
static constexpr uint32_t kGammaCorrectionHash_v281 = 0x1B319406;
static constexpr uint32_t kDofBloomGatherHash_v281 = 0x4146E600;

// UE3's camera motion blur (5 taps along the depth reprojection), drawn into the scene before the first person weapon's depth clear,
// so before the upscaler's scene end: held back and replayed after the upscaler (see "ReplayMotionBlur"). 2.87.3 ps_5_0 / 2.81.3
// ps_4_0.
static constexpr uint32_t kMotionBlurHash = 0x57D9358C;
static constexpr uint32_t kMotionBlurHash_v281 = 0x0F1082B0;

// Luma bloom pyramid mip 0, read by the grade replacements at register(t6): clear of the two slots those
// shaders actually declare (t0 scene, t1 blur).
static constexpr uint32_t kLumaBloomSlot = 6;
// One sigma per mip, count taken FROM the array so the two cannot drift (MELE). BL2's set with the LAST octave
// halved: unhalved it spans ~16.6 px at 4K vs the engine's ~13.5 px widest kernel, and reads as haze.
static float g_bloom_sigmas[] = {1.5f, 2.f, 2.f, 2.f, 1.f, 0.5f};
static constexpr int kBloomNMips = (int)std::size(g_bloom_sigmas);

// SMAA's resources go after this many presents without it (the upscaler antialiasing, SMAA off): ~10 s at 60 fps, so loading
// screens and menus between scenes don't recreate them each time (ME1's)
static constexpr uint32_t smaa_idle_release_frames = 600;

// User settings, persisted in the [Luma] config section (LoadConfigs) unless noted otherwise.
static bool g_hide_ui = false; // hide the game's HUD (for clean screenshots); session-only, never persisted
#if ENABLE_SMAA
static bool g_smaa_enable = true;
static bool g_smaa_predication = true;      // predicate SMAA on geometry, using the depth in the scene buffer's alpha
static float g_smaa_pred_tolerance = 0.02f; // plane deviation counted as a full edge, as a fraction of view depth
// RCAS sharpen on the SMAA output, opt-in at 0 (BL2/TW2 precedent): how much sharpening is wanted is a
// preference, not a target. At 0 the pass does not run and its full-resolution intermediate is never allocated.
static float g_rcas_sharpness = 0.f;
#if DEVELOPMENT
// Predication's effect is the ABSENCE of smearing, which the eye reads badly and worse in motion: judge the mask itself
static bool g_smaa_pred_debug = false; // show the predication mask instead of the antialiased frame
#endif
#endif

// Luma HDR bloom. Mirrored into GameSettings.LumaBloomEnable, which both the grade (composite) and the replaced
// gather (stop writing the vanilla glow) read — so this single switch really swaps one bloom for the other.
static bool g_luma_bloom_enable = true;
// RAW slider, driving the Luma pyramid only (the vanilla glow shares a buffer with the DoF blur, see the blur
// term in Luma_MOHA_Tonemap.hlsl). GameSettings.BloomIntensity is DERIVED from it in OnPresent, its sole writer.
static float g_bloom_intensity = 1.f;

#if DEVELOPMENT
static bool g_mv_enable = false;       // Motion vectors without an upscaler
static bool g_mv_debug_view = false;   // Core's debug draw of the motion vector target
static bool g_mv_force_jitter = false; // The projection jitter without an upscaler
#else
static constexpr bool g_mv_enable = false;
static constexpr bool g_mv_force_jitter = false;
#endif
// FSR's reactive mask: the alpha blended draws' reactivity, scaled (AMD's default 1); under the threshold 0, over it 0.9 (ME1's,
// tuned there in game: lower shakes static glows)
static constexpr float g_sr_reactive_scale = 1.f;
static constexpr float g_sr_reactive_threshold = 0.5f;
#if DEVELOPMENT
static bool g_sr_reactive_enable = true;
static bool g_sr_reactive_debug_view = false;
#else
static constexpr bool g_sr_reactive_enable = true;
#endif
// Why "DrawWithMotionVectors" refused a draw (it then only gets the jitter); DEV counters "mv.rejected.<name>", in this order
enum class MotionVectorReject : uint8_t
{
   EXTRA_TARGET,
   NO_SCENE,
   OTHER_DEPTH_COLOR,
   FORMAT,
   SIZE,
   CREATE,
   BLEND,
   SHADERS,
   COUNT
};
#if DEVELOPMENT
static constexpr const char* motion_vector_reject_names[] = {"extra_target", "no_scene", "other_depth_color", "format", "size", "create", "blend", "shaders"};
static_assert(std::size(motion_vector_reject_names) == size_t(MotionVectorReject::COUNT));
#endif

#if DEVELOPMENT
static bool g_dof_history_enable = true; // Under DLSS/FSR, the gather's DoF amount from its history (see "DrawDOFHistory")
#else
static constexpr bool g_dof_history_enable = true;
#endif

struct MedalOfHonorAirborneGameDeviceData final : public GameDeviceData
{
   // Wrapper-build telemetry: an unkeyed dgVoodoo build fails SILENTLY (format-keyed upgrades still fire: fp16
   // canvas, no replacements). Latched across frames, reported once after warmup (OnPresent).
   bool ever_matched_final_pass = false;
   uint32_t frames_presented = 0; // stops counting at the warmup frame, which is when the check runs
#if DEVELOPMENT
   // Format-upgrade diagnostics, one-shot per DEVICE rather than per process: dgVoodoo recreates the device on
   // resolution and display-mode changes, which is exactly when the fp16 mirror is worth re-reading.
   bool diag_logged_gather = false;
   bool diag_logged_rt = false;
#endif
   // The canvas the final color pass wrote into this frame, captured from its bound RTV. Consumed by Hide UI
   // (see OnDrawOrDispatch) and by the SMAA hook. Released every Present.
   ComPtr<ID3D11Resource> canvas_res;

   // Non-owning view onto core's DrawBloom mip 0 (AddRef'd by DrawBloom; the pyramid itself is core-managed and
   // released with the swapchain). Rebuilt every frame the feature is on.
   ComPtr<ID3D11ShaderResourceView> srv_luma_bloom;

   // The scene buffer captured from t0 of the hooked final pass. Its ALPHA carries linear depth, which is what
   // SMAA predication reads. Released every Present.
   ComPtr<ID3D11ShaderResourceView> srv_scene;

#if ENABLE_SMAA
   // ---- SMAA (TW2/BL2 shape, see RunPostFinalGradeSMAA) ----
   // Canvas size every resource below (and core's DrawSMAA intermediates) was created for. A change releases them all
   // at once (RunPostFinalGradeSMAA); each is then recreated on first use, so no resource tracks a size of its own.
   uint32_t smaa_w = 0, smaa_h = 0;
   uint32_t smaa_idle_frames = 0; // Presents since SMAA last ran
   // Metrics CB (b1) = (1/w, 1/h, w, h) + (predication scale, 0, 0, 0).
   ComPtr<ID3D11Buffer> cb_smaa_metrics;
   float smaa_metrics_pred_scale = -1.f;
   // SRV-readable snapshot of the canvas; the chain writes the canvas, so it must sample this copy instead.
   ComPtr<ID3D11Texture2D> tex_input;
   ComPtr<ID3D11ShaderResourceView> srv_input;
   // Its linear-light decode, for the neighborhood blend.
   ComPtr<ID3D11Texture2D> tex_input_linear;
   ComPtr<ID3D11UnorderedAccessView> uav_input_linear;
   ComPtr<ID3D11ShaderResourceView> srv_input_linear;

   // SMAA predication: srv_scene's alpha turned into an edge-ness mask by the depth-extract CS.
   ComPtr<ID3D11Buffer> cb_pred;
   float pred_tolerance = -1.f;
   ComPtr<ID3D11Texture2D> tex_pred;
   ComPtr<ID3D11UnorderedAccessView> uav_pred;
   ComPtr<ID3D11ShaderResourceView> srv_pred;

   void ReleasePredicationScratch()
   {
      uav_pred.reset();
      srv_pred.reset();
      tex_pred.reset();
   }

   // RCAS. The intermediate exists ONLY while sharpening is on: with the slider at 0 the SMAA chain renders
   // straight into the canvas, which saves both this full-resolution copy and a write-back.
   ComPtr<ID3D11Buffer> cb_sharpen;
   float sharpen_amount = -1.f;
   ComPtr<ID3D11Texture2D> tex_smaa_out;
   ComPtr<ID3D11RenderTargetView> tex_smaa_out_rtv;
   ComPtr<ID3D11ShaderResourceView> tex_smaa_out_srv;

   void ReleaseSharpenScratch()
   {
      tex_smaa_out_rtv.reset();
      tex_smaa_out_srv.reset();
      tex_smaa_out.reset();
   }

   // Turning the feature off must give the address space back: MOHA.exe is not large-address-aware (2 GB) and these
   // two copies alone are ~132 MB at 4K, with core's own SMAA intermediates on top.
   void ReleaseSMAAScratch()
   {
      srv_input.reset();
      tex_input.reset();
      uav_input_linear.reset();
      srv_input_linear.reset();
      tex_input_linear.reset();
      ReleasePredicationScratch();
      ReleaseSharpenScratch();
   }
#endif

   // The engine's own per-area bloom scale (TrackBloomScale), off the gather pass, read back deferred: copy cb4 at the
   // draw into a ring of staging buffers, map the copy made two frames earlier with a non-blocking Map, because the
   // synchronous form would stall the GPU every frame. Negative until the first successful readback, which selects the
   // neutral fallback.
   static constexpr uint32_t kBloomCBSlots = 3;
   ComPtr<ID3D11Buffer> bloom_cb_staging[kBloomCBSlots];
   uint32_t bloom_cb_bytes = 0;
   uint32_t bloom_cb_writes = 0;
   float bloom_scale_live = -1.f;
   bool bloom_scale_captured_this_frame = false; // one ring advance per frame, re-armed at Present

   // ---- DLAA / FSR Native AA (Mass Effect 2007's motion vector path, one per-draw buffer: dgVoodoo's vc4) ----
   // DLSS and FSR run in the x64 helper of Core's SR bridge (the game is 32-bit, see "SRBridge.h").
   // "IsSRActive", taken at present: Core's "Super Resolution" selection changes after it, mid frame for the draws
   bool sr_active = false;
   // The upscaler's output goes back into the scene without its alpha (the post passes' linear depth), drawn from this view of it
   com_ptr<ID3D11BlendState> sr_rgb_blend_state;
   com_ptr<ID3D11ShaderResourceView> sr_output_srv;
   // None was picked ("CleanExtraSRResources", from the overlay): the upscaler's resources go at the next present
   std::atomic<bool> release_sr_resources = false;

   std::atomic<bool> mv_active = false; // Motion vectors and jitter this frame: an upscaler is active, or the DEV toggle (set at present)
   std::shared_mutex mv_mutex;
   // A game shader's patched version (null if refused) by its hash; a vertex shader's with the bytes of vc4 it reads
   // ("DXBC::ConstantBufferBytes": the previous frame's copy uploads only those) and its matrices ("MatrixRegisters")
   template <typename T>
   struct PatchedShader
   {
      com_ptr<T> shader;
      UINT read_size = 0;
      std::vector<uint32_t> matrix_registers;
   };
   std::unordered_map<uint32_t, PatchedShader<ID3D11VertexShader>> mv_vertex_shaders;
   std::unordered_map<uint32_t, PatchedShader<ID3D11PixelShader>> mv_pixel_shaders;
   // The alpha blended draws' pixel shaders with the mask target, by blend (see "ClassifyBoundBlend", index - 1)
   std::unordered_map<uint32_t, PatchedShader<ID3D11PixelShader>> mv_reactive_pixel_shaders[2];
   // The motion vector target (sized like the scene; every blend state writes it unblended, see "OnCreateBlendState")
   com_ptr<ID3D11Texture2D> mv_texture;
   com_ptr<ID3D11RenderTargetView> mv_rtv;
   com_ptr<ID3D11UnorderedAccessView> mv_uav; // Null without typed UAV loads of its format (then no upscaler)
   com_ptr<ID3D11ShaderResourceView> mv_srv;  // The DoF history's reprojection (see "DrawDOFHistory")
   // The upscaler's depth, built from the scene's alpha by the fill (the game's depth has no shader resource view)
   com_ptr<ID3D11Texture2D> mv_device_depth;
   com_ptr<ID3D11UnorderedAccessView> mv_device_depth_uav;
   // FSR's reactive and transparency & composition masks (written by the fill from "mv_reactive_target", see
   // "Luma_MOHA_MotionVectorFill.hlsl")
   com_ptr<ID3D11Texture2D> mv_reactive;
   com_ptr<ID3D11UnorderedAccessView> mv_reactive_uav;
   com_ptr<ID3D11Texture2D> mv_transparency;
   com_ptr<ID3D11UnorderedAccessView> mv_transparency_uav;
   // The masks the alpha blended draws write (R8G8: x reactive from all, y transparency & composition from the non-additive ones;
   // max blended, see "OnCreateBlendState"), read by the fill
   com_ptr<ID3D11Texture2D> mv_reactive_target;
   com_ptr<ID3D11RenderTargetView> mv_reactive_target_rtv;
   com_ptr<ID3D11ShaderResourceView> mv_reactive_target_srv;

   // The targets sized like the scene (made again at a resize, see "DrawWithMotionVectors"; FSR's masks at the scene's end)
   void ReleaseMotionVectorTargets()
   {
      mv_texture.reset();
      mv_rtv.reset();
      mv_uav.reset();
      mv_srv.reset();
      mv_device_depth.reset();
      mv_device_depth_uav.reset();
      mv_reactive_target.reset();
      mv_reactive_target_rtv.reset();
      mv_reactive_target_srv.reset();
      mv_reactive.reset();
      mv_reactive_uav.reset();
      mv_transparency.reset();
      mv_transparency_uav.reset();
   }
   // A frame opens at its first mesh draw into output sized depth (the jitter is chosen there), starts at its first motion vector
   // draw (the target is cleared) and ends at the first post pass (the gather, or a final pass), once per present.
   bool mv_scene_open = false;
   bool mv_scene_done = false;
   bool mv_frame_ended = true;
   bool mv_fill_pending = false;
   bool mv_fsr_masks = false;        // FSR's reactive masks this frame (set when the scene opens, so a toggle mid frame doesn't split them)
   float sr_vert_fov = 1.0471976f;   // FSR's vertical FOV (radians): the last camera's, 60 degrees until one is seen
   com_ptr<ID3D11Resource> mv_depth; // The scene depth (the depth view's resource)
   // The fp16 scene the motion vector draws write, and the game's view of it (the upscaler's copy back)
   com_ptr<ID3D11Resource> mv_scene_color;
   com_ptr<ID3D11RenderTargetView> mv_scene_rtv;
   com_ptr<ID3D11ShaderResourceView> mv_scene_srv; // The fill's view of it, kept while the scene is the same resource
   // The copy of the scene the first post pass reads (UE3 resolves the scene surface into a texture), null if it reads none. The
   // upscaler's output goes into the copy only, unless the pass also reads the scene or the copy is not render target bindable.
   com_ptr<ID3D11Resource> mv_scene_copy;
   com_ptr<ID3D11RenderTargetView> mv_scene_copy_rtv;
   bool mv_scene_read_by_end = false; // The first post pass reads the scene itself
   com_ptr<ID3D11Buffer> mv_fill_buffer;
   // The projection jitter (pixels, +y down), chosen when the scene opens; its NDC offset is at VS "MotionVectorPatches::jitter_slot"
   // of every mesh draw depth tested against the scene
   std::array<float, 2> mv_jitter = {};
   std::array<float, 2> mv_jitter_ndc = {}; // The same offset in NDC (y up), as the jitter buffer holds it
   com_ptr<ID3D11Buffer> mv_jitter_buffer;
   // Per-draw lookups kept for the next draw (reset when the scene opens): the jitter path's last depth view and whether it's the
   // scene depth, its last depth stencil state's depth test, the motion vector path's last accepted targets and the last blend
   // state's opacity, the last vertex and pixel shader's patched versions (owned by the maps above, never erased)
   ID3D11DepthStencilView* jitter_dsv = nullptr;
   bool jitter_dsv_scene = false;
   ID3D11DepthStencilState* jitter_depth_stencil_state = nullptr;
   bool jitter_depth_test = true;
   ID3D11BlendState* mv_blend_state = nullptr;
   bool mv_blend_opaque = true;
   uint8_t mv_reactive_blend = 0;
   uint32_t mv_last_vertex_shader_hash = 0;
   const PatchedShader<ID3D11VertexShader>* mv_last_vertex_shader = nullptr;
   uint32_t mv_last_pixel_shader_hash = 0;
   ID3D11PixelShader* mv_last_pixel_shader = nullptr;
   PatchedDraws::BoundShader<ID3D11VertexShader> mv_bound_vertex_shader;
   PatchedDraws::BoundShader<ID3D11PixelShader> mv_bound_pixel_shader;

   // CPU copies of the vc4 buffers the motion vector draws bind, by buffer (an entry registers it, null until its first upload), from
   // a Map(WRITE_DISCARD) at its Unmap or an UpdateSubresource: a draw's constants are its buffer's latest copy
   using ConstantsCopy = std::shared_ptr<const std::vector<uint8_t>>;
   std::mutex mv_constants_mutex;
   std::unordered_map<uint64_t, ConstantsCopy> mv_constants_copies;
   std::unordered_map<uint64_t, void*> mv_mapped_constants; // Registered buffers mapped now, until their Unmap
   // The first registered buffers and their sizes, read by the buffer hooks without the lock (written under it, on the immediate
   // context's thread as the hooks): dgVoodoo maps its streaming buffers too. With more registered, every buffer takes the lock.
   static constexpr uint32_t kMaxFilteredBuffers = 8;
   std::array<std::atomic<uint64_t>, kMaxFilteredBuffers> mv_filtered_buffers = {};
   std::array<UINT, kMaxFilteredBuffers> mv_filtered_buffer_sizes = {};
   std::atomic<uint32_t> mv_filtered_buffer_count = 0;
   std::atomic<bool> mv_filter_overflow = false;
   // Every pooled vc4 copy, and those nobody held anymore at the last present (taken by the next copies): no allocation per upload.
   // Under "mv_constants_mutex".
   std::vector<std::shared_ptr<std::vector<uint8_t>>> mv_constants_pool;
   std::vector<uint32_t> mv_constants_pool_free;
   size_t mv_constants_made = 0; // Copies asked for since the last present
   // Previous frame constants of the motion vector draws (see "PatchedDraws::PreviousConstants")
   PatchedDraws::PreviousConstants mv_previous_constants;
   // Motion vector draws by draw key (shaders, buffers, arguments), with a tie-break transform and vc4. A draw takes the previous
   // frame's vc4 of its key's nearest draw (same object, a frame earlier), its camera included.
   struct MotionVectorObject
   {
      PatchedDraws::ObjectTransform transform; // LocalToWorld's (see "FindMatrices"); zero if unknown
      ConstantsCopy constants;
   };
   std::unordered_map<uint64_t, std::vector<MotionVectorObject>> mv_objects;
   std::unordered_map<uint64_t, std::vector<MotionVectorObject>> mv_previous_objects;
   // The frame's camera view projection (row vectors, from its first motion vector draw that has one, see "FindMatrices") and
   // the previous frame's
   using Matrix = std::array<float, 16>;
   std::optional<Matrix> mv_camera;
   std::optional<Matrix> mv_previous_camera;
   uint32_t mv_frame_index = 0;                  // The Luma frame index of the last motion vector frame
   std::vector<uint8_t> mv_camera_only_copy;     // An unmatched draw's constants with last frame's camera (reused)
   com_ptr<ID3D11DepthStencilView> mv_scene_dsv; // The motion vector draws' accepted depth view (also the motion blur replay's)

   // The game's motion blur draw, held back while the upscaler runs (it would get a blurred scene) and replayed after it (see
   // "ReplayMotionBlur"): its state, and copies of its constant, vertex and index buffers (the game rewrites them before the replay)
   struct DeferredMotionBlur
   {
      bool pending = false;
      bool depth_cleared = false; // The scene depth was cleared after it (the first person weapon's pass): the depth test masks the weapon
      com_ptr<ID3D11VertexShader> vertex_shader;
      com_ptr<ID3D11PixelShader> pixel_shader;
      com_ptr<ID3D11InputLayout> input_layout;
      D3D11_PRIMITIVE_TOPOLOGY topology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
      com_ptr<ID3D11Buffer> vertex_buffer;
      UINT vertex_stride = 0, vertex_offset = 0;
      com_ptr<ID3D11Buffer> index_buffer;
      DXGI_FORMAT index_format = DXGI_FORMAT_UNKNOWN;
      UINT index_offset = 0;
      static constexpr UINT constant_slots = 6; // dgVoodoo binds b0-b5
      com_ptr<ID3D11Buffer> vertex_constants[constant_slots];
      com_ptr<ID3D11Buffer> pixel_constants[constant_slots];
      com_ptr<ID3D11SamplerState> sampler;
      com_ptr<ID3D11BlendState> blend_state;
      FLOAT blend_factor[4] = {};
      UINT sample_mask = UINT_MAX;
      com_ptr<ID3D11RasterizerState> rasterizer_state;
      D3D11_VIEWPORT viewport = {};
      D3D11_SHADER_RESOURCE_VIEW_DESC source_view_desc = {};
      DrawDispatchData draw;
      // The replay's input: the upscaled scene, copied (the replay writes it)
      com_ptr<ID3D11Texture2D> source;
      com_ptr<ID3D11ShaderResourceView> source_srv;
      com_ptr<ID3D11DepthStencilState> depth_stencil_state;
      // The weapon's mask for the blur's taps (t1, read by "MotionBlur_0x57D9358C.ps_5_0.hlsl"): a copy of the scene depth after
      // the weapon's depth clear, below 1 on the weapon only
      com_ptr<ID3D11Texture2D> weapon_depth;
      com_ptr<ID3D11ShaderResourceView> weapon_depth_srv;
   };
   DeferredMotionBlur mv_motion_blur;

   // The gather's DoF amount per scene pixel accumulated over frames (see "DrawDOFHistory"): last frame's is read, the other written
   com_ptr<ID3D11ShaderResourceView> dof_history_srvs[2]; // The views hold the textures
   com_ptr<ID3D11UnorderedAccessView> dof_history_uavs[2];
   uint2 dof_history_size = {}; // Zero while released
   uint32_t dof_history_index = 0;
   bool dof_history_drawn = false; // This frame
   bool dof_history_valid = false; // Drawn last frame (taken at present)
   com_ptr<ID3D11Buffer> dof_history_buffer;
   void ReleaseDOFHistory()
   {
      for (size_t i = 0; i < std::size(dof_history_srvs); i++)
      {
         dof_history_srvs[i].reset();
         dof_history_uavs[i].reset();
      }
      dof_history_size = {};
   }

#if DEVELOPMENT
   // Per frame counts for the DEV panel (the last complete frame's shown)
   struct MotionVectorStats
   {
      uint32_t motion_vector_draws = 0, jitter_draws = 0, matched = 0, camera_only = 0, other_camera = 0, no_camera = 0, uncopied = 0, maps = 0, updates = 0, other_maps = 0, sr_draws = 0;
      uint32_t reactive_draws = 0;                                           // Jitter draws that wrote FSR's masks
      uint32_t tiebreak_collisions = 0;                                      // Objects of one key with another one's transform but other constants (see "CountTieBreakCollisions")
      uint32_t motion_blur_replays = 0, motion_blur_masked = 0;              // "ReplayMotionBlur"s, and those with the weapon masked out
      uint32_t ended_by = 0;                                                 // The ending pass's PS hash
      float near_plane = 0.f, far_plane = 0.f;                               // The upscaler's, from the camera's projection (0: none found)
      uint32_t rejected[size_t(MotionVectorReject::COUNT)] = {};             // "DrawWithMotionVectors" refusals by reason ("MV_REJECT")
      uint32_t rejected_format = 0, rejected_width = 0, rejected_height = 0; // The last target refused by format or size
   };
   MotionVectorStats mv_stats, mv_last_stats;
   int mv_draw_reject = -1; // The current draw's "MV_REJECT" reason (-1 for none), for the MCP trace note
#endif
};

class MedalOfHonorAirborne final : public Game
{
   // Pass identity by shader hash, folding both supported dgVoodoo builds (2.87.3 ps_5_0 + 2.81.3 ps_4_0).
   // Same shape as the two sibling ports on this wrapper (TW2 ContainsPixelShader/IsTonemap, BL2 IsBL2Tonemap).
   static bool ContainsPixelShader(const ShaderHashesList<OneShaderPerPipeline>& shader_hashes, uint32_t hash, uint32_t hash_v281)
   {
      return shader_hashes.Contains(hash, reshade::api::shader_stage::pixel) || shader_hashes.Contains(hash_v281, reshade::api::shader_stage::pixel);
   }

   static bool IsDofBloomGather(const ShaderHashesList<OneShaderPerPipeline>& hashes)
   {
      return ContainsPixelShader(hashes, kDofBloomGatherHash, kDofBloomGatherHash_v281);
   }

   // Exactly one of the two runs per frame, decided by the game's DoF setting (see the hash declarations).
   static bool IsFinalColorPass(const ShaderHashesList<OneShaderPerPipeline>& hashes)
   {
      return ContainsPixelShader(hashes, kUberPostHash, kUberPostHash_v281) || ContainsPixelShader(hashes, kGammaCorrectionHash, kGammaCorrectionHash_v281);
   }

   static MedalOfHonorAirborneGameDeviceData& GetGameDeviceData(DeviceData& device_data)
   {
      return *static_cast<MedalOfHonorAirborneGameDeviceData*>(device_data.game);
   }

   static bool CreateImmutableCB(ID3D11Device* device, const void* data, UINT size, ComPtr<ID3D11Buffer>& out)
   {
      out.reset();
      D3D11_BUFFER_DESC bd = {};
      bd.ByteWidth = size;
      bd.Usage = D3D11_USAGE_IMMUTABLE;
      bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
      D3D11_SUBRESOURCE_DATA sd = {};
      sd.pSysMem = data;
      return SUCCEEDED(device->CreateBuffer(&bd, &sd, out.put()));
   }

   // The SMAA snapshot must pass the canvas' live format: CopyResource requires source and destination formats
   // to match.
   static bool CreateDefaultTex(ID3D11Device* device, uint32_t w, uint32_t h, UINT bind_flags, ComPtr<ID3D11Texture2D>& out, DXGI_FORMAT format)
   {
      out.reset();
      D3D11_TEXTURE2D_DESC td = {};
      td.Width = w;
      td.Height = h;
      td.MipLevels = 1;
      td.ArraySize = 1;
      td.Format = format;
      td.SampleDesc.Count = 1;
      td.Usage = D3D11_USAGE_DEFAULT;
      td.BindFlags = bind_flags;
      return SUCCEEDED(device->CreateTexture2D(&td, nullptr, out.put()));
   }

   // Follow the artist's per-area bloom: UE3 authors Bloom_Scale per PostProcessVolume and the gather gets it in
   // cb4[10].x, keeping area-to-area variation. Donor: MELE. Threshold is a literal 1.0 in the shader. Leaves
   // bloom_scale_live alone while there is nothing to read yet: fresh ring, failed allocation, or a slot still in
   // flight. Two frames of latency is nothing against an area's bloom scale.
   static void TrackBloomScale(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, MedalOfHonorAirborneGameDeviceData* gd)
   {
      constexpr uint32_t kBloomScaleRow = 10;
      ComPtr<ID3D11Buffer> cb;
      native_device_context->PSGetConstantBuffers(4, 1, cb.put());
      if (!cb)
         return;
      D3D11_BUFFER_DESC bd = {};
      cb->GetDesc(&bd);
      if (bd.ByteWidth < (kBloomScaleRow + 1) * 16)
         return;

      // Copying the whole buffer (a few KB) rather than a byte range: a full-resource copy is the path already
      // proven on this game's dgVoodoo-created constant buffers, and the size is irrelevant at this rate.
      if (gd->bloom_cb_bytes != bd.ByteWidth)
      {
         D3D11_BUFFER_DESC sd = {};
         sd.ByteWidth = bd.ByteWidth;
         sd.Usage = D3D11_USAGE_STAGING;
         sd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
         for (auto& s : gd->bloom_cb_staging)
         {
            s.reset();
            if (FAILED(native_device->CreateBuffer(&sd, nullptr, s.put())))
            {
               for (auto& r : gd->bloom_cb_staging) // drop a partial allocation rather than run on half a ring
                  r.reset();
               gd->bloom_cb_bytes = 0;
               return;
            }
         }
         gd->bloom_cb_bytes = bd.ByteWidth;
         gd->bloom_cb_writes = 0;
      }

      constexpr uint32_t kSlots = MedalOfHonorAirborneGameDeviceData::kBloomCBSlots;
      native_device_context->CopyResource(gd->bloom_cb_staging[gd->bloom_cb_writes % kSlots].get(), cb.get());
      gd->bloom_cb_writes++;
      if (gd->bloom_cb_writes < kSlots)
         return; // nothing old enough to read yet

      // The slot about to be overwritten next is the oldest one, i.e. the copy issued kSlots frames ago.
      ID3D11Buffer* oldest = gd->bloom_cb_staging[gd->bloom_cb_writes % kSlots].get();
      D3D11_MAPPED_SUBRESOURCE mapped = {};
      if (FAILED(native_device_context->Map(oldest, 0, D3D11_MAP_READ, D3D11_MAP_FLAG_DO_NOT_WAIT, &mapped)) || mapped.pData == nullptr)
         return; // still in flight; try again next frame rather than blocking
      float bloom_scale;
      std::memcpy(&bloom_scale, (const uint8_t*)mapped.pData + kBloomScaleRow * 16, sizeof(bloom_scale));
      native_device_context->Unmap(oldest, 0);
      // Reject implausible readback data rather than let it reach the frame.
      if (bloom_scale >= 0.f && bloom_scale < 100.f)
      {
         gd->bloom_scale_live = bloom_scale;
      }
   }

   // RTV 0 as bound right now, and the resource behind it (both null when nothing is bound). Identifies the canvas at
   // the final color pass, and tests whether a later draw targets that same canvas (Hide UI, so not DEVELOPMENT-only).
   static void GetBoundRenderTarget(ID3D11DeviceContext* native_device_context, ComPtr<ID3D11RenderTargetView>* rtv, ComPtr<ID3D11Resource>* res)
   {
      rtv->reset();
      res->reset();
      native_device_context->OMGetRenderTargets(1, rtv->put(), nullptr);
      if (*rtv)
      {
         (*rtv)->GetResource(res->put());
      }
   }

#if DEVELOPMENT
   // The only honest read of an indirect format upgrade: the devkit sees the original handle and reports
   // r8g8b8a8_typeless / isResourceUpgraded:false either way (BL2). Core rebinds substituted RTVs first.
   static void DumpBoundRenderTarget(ID3D11DeviceContext* native_device_context, const char* label)
   {
      char msg[256];
      ComPtr<ID3D11RenderTargetView> rtv;
      native_device_context->OMGetRenderTargets(1, rtv.put(), nullptr);
      if (!rtv)
      {
         std::snprintf(msg, sizeof(msg), "[Luma] MOHA DIAG: %s RTV0 NOT BOUND", label);
         reshade::log::message(reshade::log::level::info, msg);
         return;
      }
      uint4 size;
      DXGI_FORMAT format;
      GetResourceInfo(rtv.get(), size, format);
      std::snprintf(msg, sizeof(msg), "[Luma] MOHA DIAG: %s RTV0 %ux%u res_fmt %s -> upgrade %s",
         label, size.x, size.y, GetFormatNameSafe(format),
         format == DXGI_FORMAT_R16G16B16A16_FLOAT ? "OK (fp16)" : "MISSING (not fp16)");
      reshade::log::message(reshade::log::level::info, msg);
   }
#endif // DEVELOPMENT

   // ---- DLAA / FSR Native AA with motion vectors and jitter from patched shaders (Mass Effect 2007's path) ----

   // An upscaler is picked and hasn't failed (it then gives way to SMAA until picked again). Fixed for the whole frame (see
   // "OnPresent"): a selection made after the motion vector state was set would otherwise run the upscaler on mixed state.
   static bool IsSRActive(DeviceData& device_data)
   {
      return GetGameDeviceData(device_data).sr_active;
   }

   // The vc4 byte offsets of the draw's view projection and LocalToWorld (SIZE_MAX if none), among its vertex shader's matrices
   // ("MatrixRegisters"): the first that projects (its w column not (0, 0, 0, 1)) and the first affine one. A GPU skin shader adds
   // its LocalToWorld's translation instead of multiplying it, so it's no matrix there: every cooked one places it in the 4 rows
   // before the view projection (c225, c229; NOTES.md "DLAA groundwork").
   struct DrawMatrices
   {
      size_t view_projection = SIZE_MAX;
      size_t world = SIZE_MAX;
   };
   static DrawMatrices FindMatrices(const std::vector<uint32_t>& matrix_registers, const std::vector<uint8_t>& constants)
   {
      DrawMatrices matrices;
      for (const uint32_t reg : matrix_registers)
      {
         const size_t offset = (size_t(reg) + MotionVectorPatches::object_row_offset) * 16;
         if (constants.size() < offset + 4 * 16)
            break;
         float w[4];
         for (size_t row = 0; row < 4; row++)
         {
            std::memcpy(&w[row], constants.data() + offset + row * 16 + 3 * sizeof(float), sizeof(float));
         }
         const bool projects = w[0] != 0.f || w[1] != 0.f || w[2] != 0.f || w[3] != 1.f;
         size_t& found = (projects ? matrices.view_projection : matrices.world);
         if (found == SIZE_MAX)
         {
            found = offset;
         }
      }
      if (matrices.world == SIZE_MAX && matrices.view_projection != SIZE_MAX && matrices.view_projection >= (MotionVectorPatches::object_row_offset + 4) * 16)
      {
         matrices.world = matrices.view_projection - 4 * 16;
      }
      return matrices;
   }

   // False if the buffer surely isn't a registered vc4 one; "size" its size if known (else 0). Lock free.
   static bool MayBeRegisteredBuffer(const MedalOfHonorAirborneGameDeviceData& gd, uint64_t handle, UINT* size)
   {
      *size = 0;
      if (gd.mv_filter_overflow.load(std::memory_order_relaxed))
         return true;
      const uint32_t count = gd.mv_filtered_buffer_count.load(std::memory_order_acquire);
      for (uint32_t i = 0; i < count; i++)
      {
         if (gd.mv_filtered_buffers[i].load(std::memory_order_relaxed) == handle)
         {
            *size = gd.mv_filtered_buffer_sizes[i];
            return true;
         }
      }
      return false;
   }

   // A buffer registered in "mv_constants_copies" joins the filter. Under "mv_constants_mutex".
   static void AddFilteredBuffer(MedalOfHonorAirborneGameDeviceData& gd, ID3D11Buffer* buffer)
   {
      const uint32_t count = gd.mv_filtered_buffer_count.load(std::memory_order_relaxed);
      if (count >= MedalOfHonorAirborneGameDeviceData::kMaxFilteredBuffers)
      {
         gd.mv_filter_overflow = true;
         return;
      }
      D3D11_BUFFER_DESC desc;
      buffer->GetDesc(&desc);
      gd.mv_filtered_buffer_sizes[count] = desc.ByteWidth;
      gd.mv_filtered_buffers[count].store(reinterpret_cast<uint64_t>(buffer), std::memory_order_relaxed);
      gd.mv_filtered_buffer_count.store(count + 1, std::memory_order_release);
   }

   static UINT GetBufferSize(uint64_t handle, UINT known_size)
   {
      if (known_size != 0)
         return known_size;
      D3D11_BUFFER_DESC desc;
      reinterpret_cast<ID3D11Buffer*>(handle)->GetDesc(&desc);
      return desc.ByteWidth;
   }

   // A vc4 copy of "size" bytes from "bytes" (null: zeroed): one nobody held anymore at the last present, else a new one. Under
   // "mv_constants_mutex".
   static std::shared_ptr<std::vector<uint8_t>> NewConstantsCopy(MedalOfHonorAirborneGameDeviceData& gd, const uint8_t* bytes, size_t size)
   {
      gd.mv_constants_made++;
      if (!gd.mv_constants_pool_free.empty())
      {
         auto copy = gd.mv_constants_pool[gd.mv_constants_pool_free.back()];
         gd.mv_constants_pool_free.pop_back();
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
      auto copy = (bytes ? std::make_shared<std::vector<uint8_t>>(bytes, bytes + size) : std::make_shared<std::vector<uint8_t>>(size));
      gd.mv_constants_pool.push_back(copy);
      return copy;
   }

   // Motion vectors: a registered vc4 buffer mapped for a whole rewrite, remembered until its Unmap
   static void OnMapBufferRegion(reshade::api::device* device, reshade::api::resource resource, uint64_t offset, uint64_t size, reshade::api::map_access access, void** data)
   {
      DeviceData* const device_data = device->get_private_data<DeviceData>();
      if (!device_data || !device_data->game || !GetGameDeviceData(*device_data).mv_active || !data || !*data)
         return;
      auto& gd = GetGameDeviceData(*device_data);
      UINT buffer_size;
      if (!MayBeRegisteredBuffer(gd, resource.handle, &buffer_size))
         return;
      const std::lock_guard lock(gd.mv_constants_mutex);
      if (!gd.mv_constants_copies.contains(resource.handle))
         return;
      if (access != reshade::api::map_access::write_discard || offset != 0)
      {
#if DEVELOPMENT
         gd.mv_stats.other_maps++; // A partial or appending write: the copies would miss it
#endif
         return;
      }
      gd.mv_mapped_constants[resource.handle] = *data;
   }

   // Motion vectors: the CPU copy of a vc4 buffer, before its Unmap (the game has written it). Reads the mapped memory back.
   static void OnUnmapBufferRegion(reshade::api::device* device, reshade::api::resource resource)
   {
      DeviceData* const device_data = device->get_private_data<DeviceData>();
      if (!device_data || !device_data->game || !GetGameDeviceData(*device_data).mv_active)
         return;
      auto& gd = GetGameDeviceData(*device_data);
      UINT buffer_size;
      if (!MayBeRegisteredBuffer(gd, resource.handle, &buffer_size))
         return;
      const std::lock_guard lock(gd.mv_constants_mutex);
      const auto mapped = gd.mv_mapped_constants.find(resource.handle);
      if (mapped == gd.mv_mapped_constants.end())
         return;
      gd.mv_constants_copies[resource.handle] = NewConstantsCopy(gd, static_cast<const uint8_t*>(mapped->second), GetBufferSize(resource.handle, buffer_size));
      gd.mv_mapped_constants.erase(mapped);
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
      UINT known_size;
      if (!MayBeRegisteredBuffer(gd, resource.handle, &known_size))
         return false;
      const std::lock_guard lock(gd.mv_constants_mutex);
      const auto copy = gd.mv_constants_copies.find(resource.handle);
      if (copy == gd.mv_constants_copies.end())
         return false;
      const UINT buffer_size = GetBufferSize(resource.handle, known_size);
      if (offset >= buffer_size)
         return false;
      const size_t updated_size = size_t((std::min)(size, uint64_t(buffer_size) - offset));
      const auto* const bytes = static_cast<const uint8_t*>(data);
      if (updated_size == buffer_size)
      {
         copy->second = NewConstantsCopy(gd, bytes, updated_size);
      }
      else
      {
         const bool merge = copy->second && copy->second->size() == buffer_size;
         auto updated = NewConstantsCopy(gd, merge ? copy->second->data() : nullptr, buffer_size);
         std::memcpy(updated->data() + offset, bytes, updated_size);
         copy->second = std::move(updated);
      }
#if DEVELOPMENT
      gd.mv_stats.updates++;
#endif
      return false;
   }

   // The bound shader's motion vector version, patched from Core's bytecode copy on first use (null if it can't be, e.g. a vertex
   // shader without vc4)
   template <typename T>
   static const MedalOfHonorAirborneGameDeviceData::PatchedShader<T>& GetMotionVectorShader(ID3D11Device* native_device, DeviceData& device_data,
      std::unordered_map<uint32_t, MedalOfHonorAirborneGameDeviceData::PatchedShader<T>>* shaders, uint32_t hash, reshade::api::pipeline pipeline, uint8_t reactive = 0)
   {
      constexpr bool vertex = std::is_same_v<T, ID3D11VertexShader>;
      auto& gd = GetGameDeviceData(device_data);
      {
         const std::shared_lock lock(gd.mv_mutex);
         if (const auto it = shaders->find(hash); it != shaders->end())
            return it->second;
      }
      std::vector<uint8_t> patched;
      std::string error = "no bytecode";
      UINT read_size = 0;
      std::vector<uint32_t> matrix_registers;
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
               matrix_registers = MotionVectorPatches::MatrixRegisters(code, desc->code_size);
            }
            else if (reactive != 0)
            {
               patched = MotionVectorPatch::PatchPixelShaderReactive(code, desc->code_size, MotionVectorPatches::layout, MotionVectorPatches::reactive_slot, reactive == 2, &error);
            }
            else
            {
               // Every dumped scene pixel shader writes o0 only; others (depth writers) are refused
               patched = MotionVectorPatch::PatchPixelShader(code, desc->code_size, MotionVectorPatches::layout, &error, /* targets_only */ true);
            }
         }
      }
      com_ptr<T> shader;
      if (!patched.empty())
      {
         HRESULT hr = E_FAIL;
         if constexpr (vertex)
         {
            hr = native_device->CreateVertexShader(patched.data(), patched.size(), nullptr, &shader);
         }
         else
         {
            hr = native_device->CreatePixelShader(patched.data(), patched.size(), nullptr, &shader);
         }
         if (FAILED(hr))
         {
            error = std::format("create 0x{:08X}", uint32_t(hr));
         }
      }
      // Failures in every build (bug reports), every patched shader only in development
      if (DEVELOPMENT || !shader)
      {
         const char* kind = "VS";
         if constexpr (!vertex)
         {
            switch (reactive)
            {
            case 0:
               kind = "PS";
               break;
            case 2:
               kind = "PS reactive additive";
               break;
            default:
               kind = "PS reactive alpha";
               break;
            }
         }
         reshade::log::message((shader ? reshade::log::level::info : reshade::log::level::warning),
            std::format("[MOHA MV] {} 0x{:08X} {} ({} matrices)", kind, hash, (shader ? "patched" : error), matrix_registers.size()).c_str());
      }
      const std::unique_lock lock(gd.mv_mutex);
      return shaders->try_emplace(hash, MedalOfHonorAirborneGameDeviceData::PatchedShader<T>{.shader = shader, .read_size = read_size, .matrix_registers = std::move(matrix_registers)}).first->second;
   }

   // The bound vertex shader's patched version (null shader if refused), looked up again only when the game's changes
   static const MedalOfHonorAirborneGameDeviceData::PatchedShader<ID3D11VertexShader>& GetPatchedVertexShader(ID3D11Device* native_device, CommandListData& cmd_list_data, DeviceData& device_data, uint32_t hash)
   {
      auto& gd = GetGameDeviceData(device_data);
      if (hash != gd.mv_last_vertex_shader_hash || !gd.mv_last_vertex_shader)
      {
         gd.mv_last_vertex_shader = &GetMotionVectorShader(native_device, device_data, &gd.mv_vertex_shaders, hash, cmd_list_data.pipeline_state_original_vertex_shader);
         gd.mv_last_vertex_shader_hash = hash;
      }
      return *gd.mv_last_vertex_shader;
   }

   // Classifies the bound blend state (cached in "mv_blend_state"): "mv_blend_opaque" for the motion vectors (additive lights, decals
   // and translucents keep the motion vectors of what's behind them, and so do colorless draws: the occlusion query boxes), and
   // "mv_reactive_blend": 0 not alpha blended (opaque, lights ONE/ONE), 1 alpha blended (SRC_ALPHA / INV_SRC_ALPHA: smoke, glass;
   // reactive and transparency & composition), 2 additive (SRC_ALPHA / ONE: sparks, muzzle flashes; reactive)
   static void ClassifyBoundBlend(ID3D11DeviceContext* native_device_context, MedalOfHonorAirborneGameDeviceData* gd)
   {
      com_ptr<ID3D11BlendState> blend_state;
      native_device_context->OMGetBlendState(&blend_state, nullptr, nullptr);
      if (blend_state.get() == gd->mv_blend_state)
         return;
      D3D11_BLEND_DESC blend_desc = CD3D11_BLEND_DESC(D3D11_DEFAULT);
      if (blend_state)
      {
         blend_state->GetDesc(&blend_desc);
      }
      const D3D11_RENDER_TARGET_BLEND_DESC& rt0 = blend_desc.RenderTarget[0];
      gd->mv_blend_opaque = rt0.RenderTargetWriteMask != 0 && (!rt0.BlendEnable || (rt0.SrcBlend == D3D11_BLEND_ONE && rt0.DestBlend == D3D11_BLEND_ZERO && rt0.BlendOp == D3D11_BLEND_OP_ADD));
      gd->mv_reactive_blend = ((!rt0.BlendEnable || rt0.SrcBlend != D3D11_BLEND_SRC_ALPHA) ? 0 : (rt0.DestBlend == D3D11_BLEND_ONE ? 2 : 1));
      gd->mv_blend_state = blend_state.get();
   }

   // Opens the scene at the frame's first mesh draw into output sized depth: takes the scene depth and picks the jitter the whole
   // scene draws with. Once per present (the HUD and later passes never reopen it).
   static void OpenScene(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, CommandListData& cmd_list_data, DeviceData& device_data, uint32_t vertex_shader_hash, ID3D11DepthStencilView* dsv)
   {
      auto& gd = GetGameDeviceData(device_data);
      uint4 depth_size;
      DXGI_FORMAT unused_format;
      GetResourceInfo(dsv, depth_size, unused_format);
      if (depth_size.x != device_data.output_resolution.x || depth_size.y != device_data.output_resolution.y || !GetPatchedVertexShader(native_device, cmd_list_data, device_data, vertex_shader_hash).shader)
         return;
      gd.mv_scene_open = true;
      gd.mv_depth.reset();
      dsv->GetResource(&gd.mv_depth);
      gd.mv_scene_color.reset();
      gd.mv_scene_rtv.reset();
      gd.jitter_dsv = nullptr;
      gd.jitter_depth_stencil_state = nullptr;
      gd.jitter_depth_test = true;
      gd.mv_scene_dsv.reset();
      gd.mv_blend_state = nullptr;
      gd.mv_blend_opaque = true;
      gd.mv_reactive_blend = 0;
      gd.mv_fsr_masks = g_sr_reactive_enable && IsSRActive(device_data) && device_data.sr_type == SR::Type::FSR;
      // Halton (2, 3) over the upscaler's phase count; pixels to NDC (y up). None until the upscaler is ready (the bridge's helper
      // starting shows the scene as it is, antialiased with SMAA).
      const SR::InstanceData* sr_instance_data = (IsSRActive(device_data) ? device_data.GetSRInstanceData() : nullptr);
      if (sr_instance_data && !sr_implementations[device_data.sr_type]->IsReady(sr_instance_data))
      {
         sr_instance_data = nullptr;
      }
      const unsigned int phase = cb_luma_global_settings.FrameIndex % (sr_instance_data ? (std::max)(sr_implementations[device_data.sr_type]->GetJitterPhases(sr_instance_data), 1) : SR::GetDefaultJitterPhases());
      gd.mv_jitter = ((sr_instance_data || g_mv_force_jitter) ? std::array<float, 2>{SR::HaltonSequence(phase, 2), SR::HaltonSequence(phase, 3)} : std::array<float, 2>{});
      gd.mv_jitter_ndc = {gd.mv_jitter[0] * 2.f / device_data.output_resolution.x, gd.mv_jitter[1] * -2.f / device_data.output_resolution.y};
      const float ndc_jitter[4] = {gd.mv_jitter_ndc[0], gd.mv_jitter_ndc[1], 0.f, 0.f};
      if (!PatchedDraws::WriteDynamicConstants(native_device, native_device_context, std::addressof(gd.mv_jitter_buffer), ndc_jitter, sizeof(ndc_jitter)))
      {
         // No stale jitter on the scene draws either: no motion vectors this frame
         gd.mv_jitter = {};
         gd.mv_jitter_ndc = {};
         gd.mv_jitter_buffer.reset();
      }
   }

#if DEVELOPMENT
#define MV_REJECT(reason) \
   ([&](auto& gd) { gd.mv_stats.rejected[size_t(reason)]++; gd.mv_draw_reject = int(reason); return false; }(GetGameDeviceData(device_data)))
#else
#define MV_REJECT(reason) false
#endif

   // Draws an opaque draw into the fp16 scene (the scene target alone, output sized, with the scene depth) with the patched shaders,
   // adding the motion vector target ("target_slot", past the game's) and the previous frame's vc4 ("previous_slots"). False if it
   // can't (the draw then goes to "DrawWithJitter").
   static bool DrawWithMotionVectors(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, CommandListData& cmd_list_data, DeviceData& device_data,
      const ShaderHashesList<OneShaderPerPipeline>& original_shader_hashes, const std::function<void()>& draw, const com_ptr<ID3D11RenderTargetView> (&rtvs)[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT], ID3D11DepthStencilView* dsv)
   {
      auto& gd = GetGameDeviceData(device_data);
      // The scene target alone, plus the motion vector target the last motion vector draw left bound
      for (UINT slot = 1; slot < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; slot++)
      {
         if (rtvs[slot] && (slot != MotionVectorPatches::target_slot || rtvs[slot] != gd.mv_rtv) &&
             (slot != MotionVectorPatches::reactive_slot || rtvs[slot] != gd.mv_reactive_target_rtv))
            return MV_REJECT(MotionVectorReject::EXTRA_TARGET);
      }
      if (!rtvs[0] || !dsv || !gd.mv_scene_open)
         return MV_REJECT(MotionVectorReject::NO_SCENE);
      ClassifyBoundBlend(native_device_context, &gd);
      if (!gd.mv_blend_opaque)
         return MV_REJECT(MotionVectorReject::BLEND);
      // Targets other than the last accepted ones: checked, and the motion vector target sized for them
      if (rtvs[0] != gd.mv_scene_rtv || dsv != gd.mv_scene_dsv.get())
      {
         com_ptr<ID3D11Resource> depth;
         dsv->GetResource(&depth);
         com_ptr<ID3D11Resource> color;
         rtvs[0]->GetResource(&color);
         if (depth != gd.mv_depth || !color || (gd.mv_scene_color && color != gd.mv_scene_color))
            return MV_REJECT(MotionVectorReject::OTHER_DEPTH_COLOR);
         D3D11_RENDER_TARGET_VIEW_DESC rtv_desc;
         rtvs[0]->GetDesc(&rtv_desc);
         com_ptr<ID3D11Texture2D> color_texture;
         D3D11_TEXTURE2D_DESC color_desc = {};
         if (SUCCEEDED(color->QueryInterface(&color_texture)))
         {
            color_texture->GetDesc(&color_desc);
         }
#if DEVELOPMENT
         gd.mv_stats.rejected_format = rtv_desc.Format;
         gd.mv_stats.rejected_width = color_desc.Width;
         gd.mv_stats.rejected_height = color_desc.Height;
#endif
         // The fill reads the scene: it needs a shader resource view. Filtered by the resource, not the view (dgVoodoo binds
         // single-slice array views).
         if (rtv_desc.Format != DXGI_FORMAT_R16G16B16A16_FLOAT || color_desc.ArraySize != 1 || color_desc.SampleDesc.Count != 1 || (color_desc.BindFlags & D3D11_BIND_SHADER_RESOURCE) == 0)
            return MV_REJECT(MotionVectorReject::FORMAT);
         const uint2 size = {color_desc.Width, color_desc.Height};
         if (size.x != device_data.output_resolution.x || size.y != device_data.output_resolution.y)
            return MV_REJECT(MotionVectorReject::SIZE);
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
               return MV_REJECT(MotionVectorReject::CREATE);
            }
            native_device->CreateShaderResourceView(gd.mv_texture.get(), nullptr, &gd.mv_srv);
            if (typed_uav_load)
            {
               native_device->CreateUnorderedAccessView(gd.mv_texture.get(), nullptr, &gd.mv_uav);
            }
            const CD3D11_TEXTURE2D_DESC depth_desc(DXGI_FORMAT_R32_FLOAT, size.x, size.y, 1, 1, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS);
            if (SUCCEEDED(SRBridge::CreateSharableTexture(native_device, depth_desc, &gd.mv_device_depth)))
            {
               native_device->CreateUnorderedAccessView(gd.mv_device_depth.get(), nullptr, &gd.mv_device_depth_uav);
            }
            const CD3D11_TEXTURE2D_DESC reactive_desc(DXGI_FORMAT_R8G8_UNORM, size.x, size.y, 1, 1, D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE);
            if (SUCCEEDED(native_device->CreateTexture2D(&reactive_desc, nullptr, &gd.mv_reactive_target)) &&
                SUCCEEDED(native_device->CreateRenderTargetView(gd.mv_reactive_target.get(), nullptr, &gd.mv_reactive_target_rtv)))
            {
               native_device->CreateShaderResourceView(gd.mv_reactive_target.get(), nullptr, &gd.mv_reactive_target_srv);
            }
            gd.mv_frame_ended = true;
         }
         gd.mv_scene_color = color;
         gd.mv_scene_rtv = rtvs[0];
         gd.mv_scene_dsv.reset(dsv);
      }

      const auto& vertex_shader = GetPatchedVertexShader(native_device, cmd_list_data, device_data, original_shader_hashes.vertex_shaders[0]);
      if (const uint32_t pixel_shader_hash = original_shader_hashes.pixel_shaders[0]; pixel_shader_hash != gd.mv_last_pixel_shader_hash)
      {
         gd.mv_last_pixel_shader = GetMotionVectorShader(native_device, device_data, &gd.mv_pixel_shaders, pixel_shader_hash, cmd_list_data.pipeline_state_original_pixel_shader).shader.get();
         gd.mv_last_pixel_shader_hash = pixel_shader_hash;
      }
      ID3D11PixelShader* const pixel_shader = gd.mv_last_pixel_shader;
      if (!vertex_shader.shader || !pixel_shader || !gd.mv_jitter_buffer)
         return MV_REJECT(MotionVectorReject::SHADERS);
      if (std::exchange(gd.mv_frame_ended, false))
      {
         gd.mv_fill_pending = gd.mv_uav && gd.mv_device_depth_uav && FindShader(device_data.native_compute_shaders, CompileTimeStringHash("MOHA Motion Vector Fill CS")) != nullptr;
         // The fill's marker: the largest float16 (a larger clear value is stored as it in R16G16_FLOAT)
         const FLOAT clear_value = (gd.mv_fill_pending ? 65504.f : 0.f);
         const FLOAT clear[4] = {clear_value, clear_value, 0.f, 0.f};
         native_device_context->ClearRenderTargetView(gd.mv_rtv.get(), clear);
         if (gd.mv_reactive_target_rtv)
         {
            const FLOAT zero[4] = {};
            native_device_context->ClearRenderTargetView(gd.mv_reactive_target_rtv.get(), zero);
         }
         // Last frame's camera and objects are the previous ones, unless frames without a scene (menus, videos) came between
         const bool previous_valid = gd.mv_camera && cb_luma_global_settings.FrameIndex - gd.mv_frame_index <= 1;
         gd.mv_previous_camera = (previous_valid ? gd.mv_camera : std::nullopt);
         gd.mv_camera.reset();
         gd.mv_frame_index = cb_luma_global_settings.FrameIndex;
         // Swapped, not rebuilt: the lists keep their nodes and capacity (an empty list matches nothing); keys drawn in neither of the
         // last two frames go
         gd.mv_previous_objects.swap(gd.mv_objects);
#if DEVELOPMENT
         gd.mv_stats.tiebreak_collisions = PatchedDraws::CountTieBreakCollisions(gd.mv_previous_objects, [](const auto& a, const auto& b)
            { return PatchedDraws::SameBytes(a.constants, b.constants); });
#endif
         std::erase_if(gd.mv_objects, [](const auto& entry)
            { return entry.second.empty(); });
         for (auto& entry : gd.mv_objects)
         {
            entry.second.clear();
         }
         if (!previous_valid)
         {
            gd.mv_previous_objects.clear();
         }
      }

      // The game's vc4 (object, camera and bones in one). The slots added past it stay bound after the draw: no translated shader
      // reads a constant buffer past b4.
      com_ptr<ID3D11Buffer> current;
      native_device_context->VSGetConstantBuffers(MotionVectorPatches::object_slot, 1, &current);
      MedalOfHonorAirborneGameDeviceData::ConstantsCopy constants;
      {
         const std::lock_guard lock(gd.mv_constants_mutex);
         // The buffer's CPU copy (null until its first upload); the lookup registers it for a copy at every upload
         if (current)
         {
            const auto [copy, registered] = gd.mv_constants_copies.try_emplace(reinterpret_cast<uint64_t>(current.get()));
            constants = copy->second;
            if (registered)
            {
               AddFilteredBuffer(gd, current.get());
            }
         }
      }
      // The previous frame's vc4: the same object's from last frame, else this draw's with last frame's camera (no object motion).
      // None (no CPU copy yet, another camera, no view projection found): the current one (zero motion).
      const std::vector<uint8_t>* upload = nullptr;
      if (constants)
      {
         // This draw's camera, the frame's if it's the first one found
         const DrawMatrices matrices = FindMatrices(vertex_shader.matrix_registers, *constants);
         MedalOfHonorAirborneGameDeviceData::Matrix view_projection = {};
         if (matrices.view_projection != SIZE_MAX)
         {
            std::memcpy(view_projection.data(), constants->data() + matrices.view_projection, sizeof(view_projection));
            if (!gd.mv_camera)
            {
               gd.mv_camera = view_projection;
            }
         }

         // Draw key: same mesh, same shaders, no instance count
         com_ptr<ID3D11Buffer> vertex_buffer;
         UINT vertex_stride = 0, vertex_offset = 0;
         native_device_context->IAGetVertexBuffers(0, 1, &vertex_buffer, &vertex_stride, &vertex_offset);
         com_ptr<ID3D11Buffer> index_buffer;
         DXGI_FORMAT index_format;
         UINT index_offset = 0;
         native_device_context->IAGetIndexBuffer(&index_buffer, &index_format, &index_offset);
         const DrawDispatchData& draw_data = last_draw_dispatch_data;
         uint64_t key = 0;
         for (const uint64_t value : {uint64_t(original_shader_hashes.vertex_shaders[0]), uint64_t(original_shader_hashes.pixel_shaders[0]), reinterpret_cast<uint64_t>(vertex_buffer.get()), uint64_t(vertex_offset),
                 reinterpret_cast<uint64_t>(index_buffer.get()), uint64_t(index_offset), uint64_t(draw_data.index_count), uint64_t(draw_data.first_index), uint64_t(uint32_t(draw_data.vertex_offset)),
                 uint64_t(draw_data.vertex_count), uint64_t(draw_data.first_vertex)})
         {
            HashCombine(key, value);
         }

         // LocalToWorld separates objects that share a key (props, soldiers of one model), as a tie-break only (see
         // "PatchedDraws::ObjectTransform")
         PatchedDraws::ObjectTransform transform = {};
         if (matrices.world != SIZE_MAX)
         {
            transform = PatchedDraws::ReadRowVectorTransform(constants->data() + matrices.world);
         }

         // A linear search among the key's candidates: a handful at most here (a spatial lookup would pay off only for big crowds
         // sharing one mesh)
         const MedalOfHonorAirborneGameDeviceData::MotionVectorObject* match = nullptr;
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
         else if (matrices.view_projection != SIZE_MAX && gd.mv_previous_camera && view_projection == *gd.mv_camera)
         {
            // Not found, drawn with the frame's camera: its own constants with last frame's camera (camera motion only)
            gd.mv_camera_only_copy.assign(constants->begin(), constants->end());
            std::memcpy(gd.mv_camera_only_copy.data() + matrices.view_projection, gd.mv_previous_camera->data(), sizeof(MedalOfHonorAirborneGameDeviceData::Matrix));
            upload = &gd.mv_camera_only_copy;
#if DEVELOPMENT
            gd.mv_stats.camera_only++;
#endif
         }
#if DEVELOPMENT
         else if (matrices.view_projection == SIZE_MAX)
         {
            gd.mv_stats.no_camera++;
         }
         else
         {
            gd.mv_stats.other_camera++;
         }
#endif
         // Kept as drawn for the next frame
         gd.mv_objects[key].push_back({.transform = transform, .constants = constants});
      }
#if DEVELOPMENT
      else
      {
         gd.mv_stats.uncopied++;
      }
#endif
      ID3D11Buffer* const previous_current[] = {current.get()};
      gd.mv_previous_constants.Bind(native_device, native_device_context, MotionVectorPatches::previous_slots, {&upload, 1}, previous_current, "MOHA", {&vertex_shader.read_size, 1});
      ID3D11Buffer* const jitter = gd.mv_jitter_buffer.get();
      native_device_context->VSSetConstantBuffers(MotionVectorPatches::jitter_slot, 1, &jitter);
      // Left bound after the draw (set directly, bypassing Core's state tracking): the game's next draws either bind their own targets
      // and shaders, or are motion vector draws too. No dumped scene pixel shader writes past o0, so the target keeps its contents.
      if (rtvs[MotionVectorPatches::target_slot] != gd.mv_rtv)
      {
         ID3D11RenderTargetView* targets[MotionVectorPatches::target_slot + 1] = {rtvs[0].get()};
         targets[MotionVectorPatches::target_slot] = gd.mv_rtv.get();
         native_device_context->OMSetRenderTargets(MotionVectorPatches::target_slot + 1, targets, dsv);
      }
      PatchedDraws::BindPatchedShader(native_device_context, vertex_shader.shader.get(), &gd.mv_bound_vertex_shader);
      PatchedDraws::BindPatchedShader(native_device_context, pixel_shader, &gd.mv_bound_pixel_shader);

      draw();
#if DEVELOPMENT
      gd.mv_stats.motion_vector_draws++;
#endif
      return true;
   }

   // Jitter for the scene's mesh draws without motion vectors (patched vertex shader, game pixel shader): depth passes, lights,
   // decals, translucents. Every draw depth tested against the scene takes the same jitter, or jittered and unjittered depths of
   // the same surface fail each other's test. False if it can't (the draw runs untouched).
   static bool DrawWithJitter(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, CommandListData& cmd_list_data, DeviceData& device_data,
      const ShaderHashesList<OneShaderPerPipeline>& original_shader_hashes, const std::function<void()>& draw, const com_ptr<ID3D11RenderTargetView> (&rtvs)[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT], ID3D11DepthStencilView* dsv)
   {
      auto& gd = GetGameDeviceData(device_data);
      if (!gd.mv_scene_open || gd.mv_jitter == std::array<float, 2>{} || !gd.mv_jitter_buffer || !dsv)
         return false;
      // Meshes only (full screen passes have no vertex buffer or no depth test), into the scene depth (not shadows)
      if (dsv != gd.jitter_dsv)
      {
         com_ptr<ID3D11Resource> depth;
         dsv->GetResource(&depth);
         gd.jitter_dsv = dsv;
         gd.jitter_dsv_scene = depth == gd.mv_depth;
      }
      if (!gd.jitter_dsv_scene)
         return false;
      com_ptr<ID3D11DepthStencilState> depth_stencil_state;
      native_device_context->OMGetDepthStencilState(&depth_stencil_state, nullptr);
      if (depth_stencil_state.get() != gd.jitter_depth_stencil_state)
      {
         D3D11_DEPTH_STENCIL_DESC depth_desc = CD3D11_DEPTH_STENCIL_DESC(D3D11_DEFAULT);
         if (depth_stencil_state)
         {
            depth_stencil_state->GetDesc(&depth_desc);
         }
         gd.jitter_depth_test = depth_desc.DepthEnable;
         gd.jitter_depth_stencil_state = depth_stencil_state.get();
      }
      if (!gd.jitter_depth_test)
         return false;
      com_ptr<ID3D11Buffer> vertex_buffer;
      UINT vertex_stride, vertex_offset;
      native_device_context->IAGetVertexBuffers(0, 1, &vertex_buffer, &vertex_stride, &vertex_offset);
      if (!vertex_buffer)
         return false;
      ID3D11VertexShader* const vertex_shader = GetPatchedVertexShader(native_device, cmd_list_data, device_data, original_shader_hashes.vertex_shaders[0]).shader.get();
      if (!vertex_shader)
         return false;
      // An alpha blended draw into the scene writes its mask, reactive or transparency & composition (its pixel shader patched, the
      // mask target added past the motion vector one). FSR only: DLSS's current presets ignore them (DLSS-Best-Practices TRN-2).
      ID3D11PixelShader* reactive_shader = nullptr;
      if (gd.mv_fsr_masks && gd.mv_reactive_target_rtv && rtvs[0] && rtvs[0] == gd.mv_scene_rtv)
      {
         ClassifyBoundBlend(native_device_context, &gd);
         if (const uint8_t blend = gd.mv_reactive_blend; blend != 0)
         {
            reactive_shader = GetMotionVectorShader(native_device, device_data, &gd.mv_reactive_pixel_shaders[blend - 1], original_shader_hashes.pixel_shaders[0], cmd_list_data.pipeline_state_original_pixel_shader, blend).shader.get();
         }
      }

      // The patched vertex shader and the jitter stay bound after the draw (see "DrawWithMotionVectors"), with the game's pixel shader
      // (a motion vector draw's is put back) or its reactive version, and the mask target
      PatchedDraws::BindPatchedShader(native_device_context, vertex_shader, &gd.mv_bound_vertex_shader);
      ID3D11Buffer* const jitter = gd.mv_jitter_buffer.get();
      native_device_context->VSSetConstantBuffers(MotionVectorPatches::jitter_slot, 1, &jitter);
      if (reactive_shader)
      {
         if (rtvs[MotionVectorPatches::reactive_slot] != gd.mv_reactive_target_rtv)
         {
            ID3D11RenderTargetView* targets[MotionVectorPatches::reactive_slot + 1] = {};
            for (uint32_t slot = 0; slot < MotionVectorPatches::reactive_slot; slot++)
            {
               targets[slot] = rtvs[slot].get();
            }
            targets[MotionVectorPatches::reactive_slot] = gd.mv_reactive_target_rtv.get();
            native_device_context->OMSetRenderTargets(MotionVectorPatches::reactive_slot + 1, targets, dsv);
         }
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

   // "*texture" made again from "desc" when it's missing or its size or format differ (empty; the caller makes its views again): true
   // if it was (null if that failed). "sharable": for the SR bridge's helper.
   static bool RecreateTexture(ID3D11Device* native_device, const D3D11_TEXTURE2D_DESC& desc, bool sharable, com_ptr<ID3D11Texture2D>* texture)
   {
      if (*texture)
      {
         D3D11_TEXTURE2D_DESC current = {};
         (*texture)->GetDesc(&current);
         if (current.Width == desc.Width && current.Height == desc.Height && current.Format == desc.Format)
            return false;
      }
      texture->reset();
      if (sharable)
      {
         SRBridge::CreateSharableTexture(native_device, desc, &*texture);
      }
      else
      {
         native_device->CreateTexture2D(&desc, nullptr, &*texture);
      }
      return true;
   }

   // Whether the scene's end writes into the copy of the scene the first post pass reads ("mv_scene_copy") rather than the scene: the
   // pass reads the copy only, and it's render target bindable (its view made here, "mv_scene_copy_rtv")
   static bool IsSceneCopyTarget(ID3D11Device* native_device, MedalOfHonorAirborneGameDeviceData* gd)
   {
      if (!gd->mv_scene_copy || gd->mv_scene_read_by_end || !gd->mv_scene_rtv)
         return false;
      com_ptr<ID3D11Resource> copy_rtv_resource;
      if (gd->mv_scene_copy_rtv)
      {
         gd->mv_scene_copy_rtv->GetResource(&copy_rtv_resource);
      }
      if (copy_rtv_resource != gd->mv_scene_copy)
      {
         gd->mv_scene_copy_rtv.reset();
         com_ptr<ID3D11Texture2D> copy;
         if (SUCCEEDED(gd->mv_scene_copy->QueryInterface(&copy)))
         {
            D3D11_TEXTURE2D_DESC copy_desc = {};
            copy->GetDesc(&copy_desc);
            D3D11_RENDER_TARGET_VIEW_DESC rtv_desc = {};
            gd->mv_scene_rtv->GetDesc(&rtv_desc);
            if (copy_desc.BindFlags & D3D11_BIND_RENDER_TARGET)
            {
               native_device->CreateRenderTargetView(copy.get(), &rtv_desc, &gd->mv_scene_copy_rtv);
            }
         }
      }
      return gd->mv_scene_copy_rtv != nullptr;
   }

   // "copy" = a GPU copy of "buffer" (default usage, same size and bindings, made again when they change); null without a buffer
   static void CopyToDefaultBuffer(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, ID3D11Buffer* buffer, com_ptr<ID3D11Buffer>* copy)
   {
      if (!buffer)
      {
         copy->reset();
         return;
      }
      D3D11_BUFFER_DESC desc;
      buffer->GetDesc(&desc);
      D3D11_BUFFER_DESC copy_desc = {};
      if (*copy)
      {
         (*copy)->GetDesc(&copy_desc);
      }
      if (copy_desc.ByteWidth != desc.ByteWidth || copy_desc.BindFlags != desc.BindFlags)
      {
         copy->reset();
         desc.Usage = D3D11_USAGE_DEFAULT;
         desc.CPUAccessFlags = 0;
         desc.MiscFlags = 0;
         desc.StructureByteStride = 0;
         if (FAILED(native_device->CreateBuffer(&desc, nullptr, &*copy)))
            return;
      }
      native_device_context->CopyResource(copy->get(), buffer);
   }

   // Holds the game's motion blur draw back while the upscaler runs (see "DeferredMotionBlur"): its bound state and copies of its
   // buffers. False if it can't (it then draws now).
   static bool DeferMotionBlur(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, MedalOfHonorAirborneGameDeviceData& gd)
   {
      auto& mb = gd.mv_motion_blur;
      com_ptr<ID3D11ShaderResourceView> source;
      native_device_context->PSGetShaderResources(0, 1, &source);
      if (!source || !gd.mv_scene_dsv)
         return false;
      source->GetDesc(&mb.source_view_desc);
      // The game's own shaders, not the last motion vector draw's left bound
      PatchedDraws::RestoreGameShader(native_device_context, &gd.mv_bound_vertex_shader);
      PatchedDraws::RestoreGameShader(native_device_context, &gd.mv_bound_pixel_shader);
      mb.vertex_shader.reset();
      native_device_context->VSGetShader(&mb.vertex_shader, nullptr, nullptr);
      mb.pixel_shader.reset();
      native_device_context->PSGetShader(&mb.pixel_shader, nullptr, nullptr);
      mb.input_layout.reset();
      native_device_context->IAGetInputLayout(&mb.input_layout);
      native_device_context->IAGetPrimitiveTopology(&mb.topology);
      com_ptr<ID3D11Buffer> vertex_buffer, index_buffer;
      native_device_context->IAGetVertexBuffers(0, 1, &vertex_buffer, &mb.vertex_stride, &mb.vertex_offset);
      native_device_context->IAGetIndexBuffer(&index_buffer, &mb.index_format, &mb.index_offset);
      CopyToDefaultBuffer(native_device, native_device_context, vertex_buffer.get(), std::addressof(mb.vertex_buffer));
      CopyToDefaultBuffer(native_device, native_device_context, index_buffer.get(), std::addressof(mb.index_buffer));
      com_ptr<ID3D11Buffer> vertex_constants[mb.constant_slots], pixel_constants[mb.constant_slots];
      native_device_context->VSGetConstantBuffers(0, mb.constant_slots, &vertex_constants[0]);
      native_device_context->PSGetConstantBuffers(0, mb.constant_slots, &pixel_constants[0]);
      for (UINT slot = 0; slot < mb.constant_slots; slot++)
      {
         CopyToDefaultBuffer(native_device, native_device_context, vertex_constants[slot].get(), std::addressof(mb.vertex_constants[slot]));
         CopyToDefaultBuffer(native_device, native_device_context, pixel_constants[slot].get(), std::addressof(mb.pixel_constants[slot]));
      }
      mb.sampler.reset();
      native_device_context->PSGetSamplers(0, 1, &mb.sampler);
      mb.blend_state.reset();
      native_device_context->OMGetBlendState(&mb.blend_state, mb.blend_factor, &mb.sample_mask);
      mb.rasterizer_state.reset();
      native_device_context->RSGetState(&mb.rasterizer_state);
      UINT viewports = 1;
      native_device_context->RSGetViewports(&viewports, &mb.viewport);
      mb.draw = last_draw_dispatch_data;
      if (!mb.vertex_shader || !mb.pixel_shader || !mb.vertex_buffer || (mb.draw.indexed && !mb.index_buffer) || mb.draw.indirect)
         return false;
      mb.pending = true;
      mb.depth_cleared = false;
      return true;
   }

   // The held back motion blur, drawn on the upscaled scene into what the first post pass reads. After the first person weapon's
   // depth clear, the depth test against the scene depth (all at 1 but the weapon's) keeps it to the world's pixels, as it was drawn
   // before the weapon. Inside "EndScene"'s state cache (the vertex and index buffers it doesn't cache are put back here).
   static bool ReplayMotionBlur(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, MedalOfHonorAirborneGameDeviceData& gd)
   {
      auto& mb = gd.mv_motion_blur;
      if (!std::exchange(mb.pending, false) || !gd.mv_scene_color || !gd.mv_scene_rtv)
         return false;
      const bool into_copy = IsSceneCopyTarget(native_device, &gd);
      ID3D11Resource* const target = (into_copy ? gd.mv_scene_copy.get() : gd.mv_scene_color.get());
      ID3D11RenderTargetView* const target_rtv = (into_copy ? gd.mv_scene_copy_rtv.get() : gd.mv_scene_rtv.get());
      com_ptr<ID3D11Texture2D> target_texture;
      if (FAILED(target->QueryInterface(&target_texture)))
         return false;
      D3D11_TEXTURE2D_DESC source_desc = {};
      target_texture->GetDesc(&source_desc);
      source_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
      source_desc.MiscFlags = 0;
      source_desc.CPUAccessFlags = 0;
      source_desc.Usage = D3D11_USAGE_DEFAULT;
      if (RecreateTexture(native_device, source_desc, /* sharable */ false, std::addressof(mb.source)))
      {
         mb.source_srv.reset();
         if (mb.source)
         {
            native_device->CreateShaderResourceView(mb.source.get(), &mb.source_view_desc, &mb.source_srv);
         }
      }
      if (!mb.depth_stencil_state)
      {
         D3D11_DEPTH_STENCIL_DESC depth_desc = CD3D11_DEPTH_STENCIL_DESC(D3D11_DEFAULT);
         depth_desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
         depth_desc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
         native_device->CreateDepthStencilState(&depth_desc, &mb.depth_stencil_state);
      }
      if (!mb.source_srv || !mb.depth_stencil_state)
         return false;
      native_device_context->CopyResource(mb.source.get(), target);
      // The weapon's mask: the depth copied, of its resource's typeless format (D24S8 or D32, with or without stencil)
      ID3D11ShaderResourceView* weapon_mask = nullptr;
      if (mb.depth_cleared)
      {
         com_ptr<ID3D11Texture2D> depth_texture;
         D3D11_TEXTURE2D_DESC depth_desc = {};
         if (gd.mv_depth && SUCCEEDED(gd.mv_depth->QueryInterface(&depth_texture)))
         {
            depth_texture->GetDesc(&depth_desc);
         }
         DXGI_FORMAT view_format = DXGI_FORMAT_UNKNOWN;
         switch (depth_desc.Format)
         {
         case DXGI_FORMAT_R24G8_TYPELESS:
            view_format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
            break;
         case DXGI_FORMAT_R32G8X24_TYPELESS:
            view_format = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
            break;
         case DXGI_FORMAT_R32_TYPELESS:
            view_format = DXGI_FORMAT_R32_FLOAT;
            break;
         default:
            break;
         }
         if (depth_texture && depth_desc.SampleDesc.Count == 1 && view_format != DXGI_FORMAT_UNKNOWN)
         {
            D3D11_TEXTURE2D_DESC copy_desc = depth_desc;
            copy_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            copy_desc.MiscFlags = 0;
            copy_desc.CPUAccessFlags = 0;
            copy_desc.Usage = D3D11_USAGE_DEFAULT;
            if (RecreateTexture(native_device, copy_desc, /* sharable */ false, std::addressof(mb.weapon_depth)))
            {
               mb.weapon_depth_srv.reset();
               const CD3D11_SHADER_RESOURCE_VIEW_DESC view_desc(D3D11_SRV_DIMENSION_TEXTURE2D, view_format, 0, 1);
               if (mb.weapon_depth)
               {
                  native_device->CreateShaderResourceView(mb.weapon_depth.get(), &view_desc, &mb.weapon_depth_srv);
               }
            }
         }
         else
         {
            mb.weapon_depth.reset();
            mb.weapon_depth_srv.reset();
         }
         if (mb.weapon_depth_srv)
         {
            native_device_context->CopyResource(mb.weapon_depth.get(), gd.mv_depth.get());
            weapon_mask = mb.weapon_depth_srv.get();
         }
      }

      com_ptr<ID3D11Buffer> vertex_buffer, index_buffer;
      UINT vertex_stride = 0, vertex_offset = 0, index_offset = 0;
      DXGI_FORMAT index_format = DXGI_FORMAT_UNKNOWN;
      native_device_context->IAGetVertexBuffers(0, 1, &vertex_buffer, &vertex_stride, &vertex_offset);
      native_device_context->IAGetIndexBuffer(&index_buffer, &index_format, &index_offset);

      // Every pixel at depth 1: the test passes where the scene depth is still the clear's
      D3D11_VIEWPORT viewport = mb.viewport;
      viewport.MinDepth = 1.f;
      viewport.MaxDepth = 1.f;
      native_device_context->OMSetRenderTargets(1, &target_rtv, mb.depth_cleared ? gd.mv_scene_dsv.get() : nullptr);
      native_device_context->OMSetDepthStencilState(mb.depth_stencil_state.get(), 0);
      native_device_context->OMSetBlendState(mb.blend_state.get(), mb.blend_factor, mb.sample_mask);
      native_device_context->RSSetState(mb.rasterizer_state.get());
      native_device_context->RSSetViewports(1, &viewport);
      native_device_context->IASetInputLayout(mb.input_layout.get());
      native_device_context->IASetPrimitiveTopology(mb.topology);
      ID3D11Buffer* const replay_vertex_buffer = mb.vertex_buffer.get();
      native_device_context->IASetVertexBuffers(0, 1, &replay_vertex_buffer, &mb.vertex_stride, &mb.vertex_offset);
      native_device_context->IASetIndexBuffer(mb.index_buffer.get(), mb.index_format, mb.index_offset);
      ID3D11Buffer* vertex_constants[mb.constant_slots];
      ID3D11Buffer* pixel_constants[mb.constant_slots];
      for (UINT slot = 0; slot < mb.constant_slots; slot++)
      {
         vertex_constants[slot] = mb.vertex_constants[slot].get();
         pixel_constants[slot] = mb.pixel_constants[slot].get();
      }
      native_device_context->VSSetConstantBuffers(0, mb.constant_slots, vertex_constants);
      native_device_context->PSSetConstantBuffers(0, mb.constant_slots, pixel_constants);
      ID3D11ShaderResourceView* const srvs[2] = {mb.source_srv.get(), weapon_mask};
      native_device_context->PSSetShaderResources(0, UINT(std::size(srvs)), srvs);
      ID3D11SamplerState* const sampler = mb.sampler.get();
      native_device_context->PSSetSamplers(0, 1, &sampler);
      native_device_context->VSSetShader(mb.vertex_shader.get(), nullptr, 0);
      native_device_context->PSSetShader(mb.pixel_shader.get(), nullptr, 0);
      if (mb.draw.indexed)
      {
         native_device_context->DrawIndexed(mb.draw.index_count, mb.draw.first_index, mb.draw.vertex_offset);
      }
      else
      {
         native_device_context->Draw(mb.draw.vertex_count, mb.draw.first_vertex);
      }

      ID3D11Buffer* const restored_vertex_buffer = vertex_buffer.get();
      native_device_context->IASetVertexBuffers(0, 1, &restored_vertex_buffer, &vertex_stride, &vertex_offset);
      native_device_context->IASetIndexBuffer(index_buffer.get(), index_format, index_offset);
#if DEVELOPMENT
      gd.mv_stats.motion_blur_replays++;
      gd.mv_stats.motion_blur_masked += mb.depth_cleared;
#endif
      return true;
   }

   // The first person weapon's depth clear after the held back motion blur (see "ReplayMotionBlur")
   static bool OnClearDepthStencilView(reshade::api::command_list* cmd_list, reshade::api::resource_view dsv, const float* depth, const uint8_t* stencil, uint32_t rect_count, const reshade::api::rect* rects)
   {
      DeviceData* const device_data = cmd_list->get_device()->get_private_data<DeviceData>();
      if (!depth || !device_data || !device_data->game)
         return false;
      auto& gd = GetGameDeviceData(*device_data);
      if (!gd.mv_motion_blur.pending || *depth != 1.f || rect_count != 0)
         return false;
      com_ptr<ID3D11Resource> cleared;
      reinterpret_cast<ID3D11DepthStencilView*>(dsv.handle)->GetResource(&cleared);
      if (cleared == gd.mv_depth)
      {
         gd.mv_motion_blur.depth_cleared = true;
      }
      return false;
   }

   // DLAA or FSR 3 Native AA on the jittered scene, its depth (from its alpha, see the fill) and the motion vectors; the result goes
   // back into the scene's color (or its copy, see "mv_scene_copy"), its alpha (the post passes' linear depth) kept. False if it
   // didn't draw (missing input, or the upscaler failed).
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

      const CD3D11_TEXTURE2D_DESC output_desc(DXGI_FORMAT_R16G16B16A16_FLOAT, scene_desc.Width, scene_desc.Height, 1, 1, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS);
      if (RecreateTexture(native_device, output_desc, /* sharable */ true, std::addressof(device_data.sr_output_color)))
      {
         gd.sr_output_srv.reset();
         if (device_data.sr_output_color)
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
      const SR::ViewProjectionCamera camera = SR::GetViewProjectionCamera(gd.mv_camera->data(), /* row_vectors */ true);

      const SR::SettingsData settings_data = {
         .output_width = scene_desc.Width,
         .output_height = scene_desc.Height,
         .render_width = scene_desc.Width,
         .render_height = scene_desc.Height,
         .hdr = true,
         // The motion vectors are UV deltas, previous minus current
         .mvs_x_scale = float(scene_desc.Width),
         .mvs_y_scale = float(scene_desc.Height),
         // DLSS's own (DLSS-Best-Practices EXP-4 canon; presets L and M ignore the texture anyway); FSR's clips highlights
         // (FSR-Best-Practices FIN-3), and the scene is already exposed
         .auto_exposure = (device_data.sr_type != SR::Type::FSR),
         .render_preset = dlss_render_preset,
      };
      sr_implementations[device_data.sr_type]->UpdateSettings(sr_instance_data, native_device_context, settings_data);

      // FSR requires a FOV (it errors on 0): a camera without an up axis keeps the last one
      if (camera.vert_fov > 0.0)
      {
         gd.sr_vert_fov = float(camera.vert_fov);
      }
      SR::SuperResolutionImpl::DrawData draw_data = {
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
      };
      if (camera.near_plane > 0.0)
      {
         draw_data.near_plane = float(camera.near_plane);
         draw_data.far_plane = float(camera.far_plane);
      }
#if DEVELOPMENT
      gd.mv_stats.near_plane = draw_data.near_plane;
      gd.mv_stats.far_plane = draw_data.far_plane;
#endif
      if (!sr_implementations[device_data.sr_type]->Draw(sr_instance_data, native_device_context, draw_data))
      {
         // Back to SMAA until the upscaler is picked again
         device_data.sr_suppressed = true;
         return false;
      }
      // The copy's alpha is the game's resolve of the same scene (the RGB write mask keeps its linear depth)
      DrawCustomPixelShader(native_device_context, device_data.default_depth_stencil_state.get(), gd.sr_rgb_blend_state.get(), nullptr, copy_vs, copy_ps,
         gd.sr_output_srv.get(), (IsSceneCopyTarget(native_device, &gd) ? gd.mv_scene_copy_rtv.get() : gd.mv_scene_rtv.get()), scene_desc.Width, scene_desc.Height);
      // Not while the bridge's helper starts (the color copied as it is): SMAA stays on and the next frame resets
      device_data.has_drawn_sr = sr_implementations[device_data.sr_type]->IsReady(sr_instance_data);
      return true;
   }

   // DLSS/FSR, at the gather's draw: its DoF amount for every scene pixel, blended into last frame's (see "Luma_MOHA_DOFHistory.hlsl"),
   // from what it reads (t0, b3/b4). The new history's view, null if it didn't draw (the gather then keeps its own amount).
   static ID3D11ShaderResourceView* DrawDOFHistory(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, DeviceData& device_data)
   {
      auto& gd = GetGameDeviceData(device_data);
      auto* const history_shader = FindShader(device_data.native_compute_shaders, CompileTimeStringHash("MOHA DOF History CS"));
      if (!g_dof_history_enable || !device_data.has_drawn_sr || !history_shader || !gd.mv_srv)
         return nullptr;
      const uint2 size = {uint32_t(device_data.output_resolution.x), uint32_t(device_data.output_resolution.y)};
      const bool resized = size.x != gd.dof_history_size.x || size.y != gd.dof_history_size.y;
      if (resized)
      {
         gd.ReleaseDOFHistory();
         gd.dof_history_size = size;
         const CD3D11_TEXTURE2D_DESC history_desc(DXGI_FORMAT_R16_FLOAT, size.x, size.y, 1, 1, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS);
         for (size_t i = 0; i < std::size(gd.dof_history_srvs); i++)
         {
            com_ptr<ID3D11Texture2D> texture;
            if (SUCCEEDED(native_device->CreateTexture2D(&history_desc, nullptr, &texture)))
            {
               native_device->CreateShaderResourceView(texture.get(), nullptr, &gd.dof_history_srvs[i]);
               native_device->CreateUnorderedAccessView(texture.get(), nullptr, &gd.dof_history_uavs[i]);
            }
         }
      }
      const uint32_t previous = gd.dof_history_index, next = gd.dof_history_index ^ 1;
      // Weight of this frame: 0.1 (about ten frames, BL GOTY's), 1 when the history restarts with the upscaler's
      const CB::DOFHistoryConstants constants = {.history_weight = ((!resized && gd.dof_history_valid && !device_data.force_reset_sr) ? 0.1f : 1.f)};
      com_ptr<ID3D11ShaderResourceView> scene_srv;
      native_device_context->PSGetShaderResources(0, 1, &scene_srv);
      if (!scene_srv || !gd.dof_history_srvs[previous] || !gd.dof_history_uavs[next] ||
          !PatchedDraws::WriteDynamicConstants(native_device, native_device_context, std::addressof(gd.dof_history_buffer), &constants, sizeof(constants)))
         return nullptr;

      DrawStateStack<DrawStateStackType::Compute> compute_state;
      compute_state.Cache(native_device_context, device_data.uav_max_count);
      // dgVoodoo's b3 (the texture masks) and b4 (the gather's DoF rows)
      com_ptr<ID3D11Buffer> game_constants[2];
      native_device_context->PSGetConstantBuffers(3, UINT(std::size(game_constants)), &game_constants[0]);
      ID3D11Buffer* const buffers[5] = {gd.dof_history_buffer.get(), nullptr, nullptr, game_constants[0].get(), game_constants[1].get()};
      ID3D11ShaderResourceView* const srvs[3] = {scene_srv.get(), gd.mv_srv.get(), gd.dof_history_srvs[previous].get()};
      ID3D11UnorderedAccessView* const uav = gd.dof_history_uavs[next].get();
      ID3D11SamplerState* const linear_sampler = device_data.sampler_state_linear.get();
      native_device_context->CSSetConstantBuffers(0, UINT(std::size(buffers)), buffers);
      native_device_context->CSSetShaderResources(0, UINT(std::size(srvs)), srvs);
      native_device_context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);
      native_device_context->CSSetSamplers(0, 1, &linear_sampler);
      native_device_context->CSSetShader(history_shader, nullptr, 0);
      native_device_context->Dispatch((size.x + 7) / 8, (size.y + 7) / 8, 1);
      compute_state.Restore(native_device_context);
      gd.dof_history_index = next;
      gd.dof_history_drawn = true;
      return gd.dof_history_srvs[next].get();
   }

   // Ends the scene at its first post pass: the depth and camera motion fill (see "Luma_MOHA_MotionVectorFill.hlsl"), then the
   // upscaler, both before that pass reads the scene
   static void EndScene(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, DeviceData& device_data)
   {
      auto& gd = GetGameDeviceData(device_data);
      gd.mv_scene_open = false;
      gd.mv_scene_done = true;
      DrawStateStack<DrawStateStackType::FullGraphics> graphics_state;
      DrawStateStack<DrawStateStackType::Compute> compute_state;
      graphics_state.Cache(native_device_context, device_data.uav_max_count);
      compute_state.Cache(native_device_context, device_data.uav_max_count);
      auto* const fill_shader = FindShader(device_data.native_compute_shaders, CompileTimeStringHash("MOHA Motion Vector Fill CS"));
      bool filled = false;
      if (gd.mv_scene_srv)
      {
         com_ptr<ID3D11Resource> srv_resource;
         gd.mv_scene_srv->GetResource(&srv_resource);
         if (srv_resource != gd.mv_scene_color)
         {
            gd.mv_scene_srv.reset();
         }
      }
      // The reactive and transparency & composition masks, written by the fill from what the alpha blended draws wrote (FSR only)
      const bool reactive = gd.mv_fsr_masks && gd.mv_reactive_target_srv;
      if (reactive)
      {
         const CD3D11_TEXTURE2D_DESC desc(DXGI_FORMAT_R8_UNORM, uint32_t(device_data.output_resolution.x), uint32_t(device_data.output_resolution.y), 1, 1, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS);
         const auto recreate_mask = [&](com_ptr<ID3D11Texture2D>* mask, com_ptr<ID3D11UnorderedAccessView>* mask_uav)
         {
            if (RecreateTexture(native_device, desc, /* sharable */ true, mask))
            {
               mask_uav->reset();
               if (*mask)
               {
                  native_device->CreateUnorderedAccessView(mask->get(), nullptr, &*mask_uav);
               }
            }
         };
         recreate_mask(std::addressof(gd.mv_reactive), std::addressof(gd.mv_reactive_uav));
         recreate_mask(std::addressof(gd.mv_transparency), std::addressof(gd.mv_transparency_uav));
      }
      const bool write_reactive = reactive && gd.mv_reactive_uav && gd.mv_transparency_uav;
      if (std::exchange(gd.mv_fill_pending, false) && fill_shader && gd.mv_camera && gd.mv_scene_color &&
          (gd.mv_scene_srv || SUCCEEDED(native_device->CreateShaderResourceView(gd.mv_scene_color.get(), nullptr, &gd.mv_scene_srv))))
      {
         // Current clip space to the previous frame's: previous * inverse(current) for column vectors (vc4 holds the row vector
         // matrix, transposed here), in double (absolute world translation)
         Math::Matrix44D current, previous;
         current.SetIdentity();
         previous.SetIdentity();
         if (gd.mv_previous_camera)
         {
            std::copy_n(gd.mv_camera->data(), 16, current.GetData());
            std::copy_n(gd.mv_previous_camera->data(), 16, previous.GetData());
            current.Transpose();
            previous.Transpose();
            current.Invert();
         }
         const Math::Matrix44D reprojection = previous * current;
         const SR::ViewProjectionCamera camera = SR::GetViewProjectionCamera(gd.mv_camera->data(), /* row_vectors */ true);
         CB::MotionVectorFillConstants constants = {
            .jitter_ndc = {gd.mv_jitter_ndc[0], gd.mv_jitter_ndc[1]},
            .depth_from_view = {float(camera.depth_a), float(camera.depth_b)},
            .reactive_scale = g_sr_reactive_scale,
            .reactive_threshold = g_sr_reactive_threshold,
            .reactive_enabled = (write_reactive ? 1.f : 0.f),
         };
         for (int i = 0; i < 16; i++)
         {
            constants.reprojection.GetData()[i] = float(reprojection.GetData()[i]);
         }
         if (PatchedDraws::WriteDynamicConstants(native_device, native_device_context, std::addressof(gd.mv_fill_buffer), &constants, sizeof(constants)))
         {
            // The scene and the motion vectors may be bound as render targets
            native_device_context->OMSetRenderTargets(0, nullptr, nullptr);
            ID3D11Buffer* const buffer = gd.mv_fill_buffer.get();
            ID3D11ShaderResourceView* const srvs[2] = {gd.mv_scene_srv.get(), (write_reactive ? gd.mv_reactive_target_srv.get() : nullptr)};
            ID3D11UnorderedAccessView* const uavs[4] = {gd.mv_uav.get(), gd.mv_device_depth_uav.get(), (write_reactive ? gd.mv_reactive_uav.get() : nullptr), (write_reactive ? gd.mv_transparency_uav.get() : nullptr)};
            native_device_context->CSSetConstantBuffers(0, 1, &buffer);
            native_device_context->CSSetShaderResources(0, UINT(std::size(srvs)), srvs);
            native_device_context->CSSetUnorderedAccessViews(0, UINT(std::size(uavs)), uavs, nullptr);
            native_device_context->CSSetShader(fill_shader, nullptr, 0);
            native_device_context->Dispatch((uint32_t(device_data.output_resolution.x) + 7) / 8, (uint32_t(device_data.output_resolution.y) + 7) / 8, 1);
            ID3D11UnorderedAccessView* const null_uavs[std::size(uavs)] = {};
            ID3D11ShaderResourceView* const null_srvs[std::size(srvs)] = {};
            native_device_context->CSSetUnorderedAccessViews(0, UINT(std::size(null_uavs)), null_uavs, nullptr);
            native_device_context->CSSetShaderResources(0, UINT(std::size(null_srvs)), null_srvs);
            filled = true;
         }
      }
      // The upscaler's depth comes from the fill
      const bool upscaled = IsSRActive(device_data) && filled && DrawUpscaler(native_device, native_device_context, device_data, write_reactive);
#if DEVELOPMENT
      gd.mv_stats.sr_draws += upscaled;
#endif
      const bool blurred = ReplayMotionBlur(native_device, native_device_context, gd);
      // Both drew into the copy the first post pass reads, or into the scene, which the copy then takes again
      if ((upscaled || blurred) && gd.mv_scene_copy && !IsSceneCopyTarget(native_device, &gd))
      {
         native_device_context->CopyResource(gd.mv_scene_copy.get(), gd.mv_scene_color.get());
      }
      compute_state.Restore(native_device_context);
      graphics_state.Restore(native_device_context);
   }

public:
   static void UnregisterEvents()
   {
      reshade::unregister_event<reshade::addon_event::map_buffer_region>(OnMapBufferRegion);
      reshade::unregister_event<reshade::addon_event::unmap_buffer_region>(OnUnmapBufferRegion);
      reshade::unregister_event<reshade::addon_event::update_buffer_region>(OnUpdateBufferRegion);
      reshade::unregister_event<reshade::addon_event::create_pipeline>(PatchedDraws::OnCreateBlendState<MotionVectorPatches::target_slot, MotionVectorPatches::reactive_slot, true>);
      reshade::unregister_event<reshade::addon_event::clear_depth_stencil_view>(OnClearDepthStencilView);
   }

   void OnInit(bool async) override
   {
#if DEVELOPMENT
      // For the MCP "luma_dev_values" tool
      Mcp::RegisterToggles({{"hide_ui", &g_hide_ui}, {"mv_enable", &g_mv_enable}, {"mv_debug_view", &g_mv_debug_view}, {"mv_force_jitter", &g_mv_force_jitter}, {"sr_reactive_enable", &g_sr_reactive_enable}, {"sr_reactive_debug_view", &g_sr_reactive_debug_view}, { "dof_history_enable",
                               &g_dof_history_enable }});
      Mcp::RegisterTextures({MCP_GAME_TEXTURE("mv.velocity", mv_texture), MCP_GAME_TEXTURE("mv.depth", mv_device_depth), MCP_GAME_TEXTURE("sr.reactive", mv_reactive), MCP_GAME_TEXTURE("sr.transparency", mv_transparency)});
      Mcp::RegisterMirroredToggle("luma_bloom_enable", &g_luma_bloom_enable, &cb_luma_global_settings.GameSettings.LumaBloomEnable);
      Mcp::RegisterValues({{"bloom_intensity", &g_bloom_intensity, 0.f, 2.f}});
#if ENABLE_SMAA
      Mcp::RegisterToggles({{"smaa_enable", &g_smaa_enable}, {"smaa_predication", &g_smaa_predication}, { "smaa_pred_debug",
                               &g_smaa_pred_debug }});
      Mcp::RegisterValues({{"smaa_pred_tolerance", &g_smaa_pred_tolerance, 0.002f, 0.2f}, {"rcas_sharpness", &g_rcas_sharpness, 0.f, 1.f}});
      Mcp::RegisterTextures({MCP_GAME_TEXTURE("smaa.input", tex_input),
         MCP_GAME_TEXTURE("smaa.input_linear", tex_input_linear),
         MCP_GAME_TEXTURE("smaa.pred_mask", tex_pred),
         MCP_GAME_TEXTURE("smaa.output", tex_smaa_out)});
#endif
#endif
      // Game-specific toggles consumed by both replaced final passes (Luma_MOHA_Tonemap.hlsl).
      std::vector<ShaderDefineData> game_shader_defines_data = {
         {"TONEMAP_TYPE", '1', true, false, "0 - SDR: Vanilla (clamped reference)\n1 - HDR: extended native grade + MacLeod-Boynton hue + DICE display map", 1},
      };
      shader_defines_data.append_range(game_shader_defines_data);
      assert(shader_defines_data.size() < MAX_SHADER_DEFINES);

#if ENABLE_SMAA
      // The 6 SMAA passes are auto-registered by core from Luma_SMAA_impl. Ours: the linear decode its blend reads,
      // and the predication CS that turns the scene buffer's alpha (linear depth) into R16F edge-ness in [0,1].
      native_shaders_definitions.emplace(CompileTimeStringHash("MOHA SMAA Linearize CS"),
         ShaderDefinition("Luma_MOHA_SMAALinearize", reshade::api::pipeline_subobject_type::compute_shader));
      native_shaders_definitions.emplace(CompileTimeStringHash("MOHA Depth Extract CS"),
         ShaderDefinition("Luma_MOHA_DepthExtract", reshade::api::pipeline_subobject_type::compute_shader));
      // RCAS sharpen PS, drawn via core "Copy VS" + DrawCustomPixelShader after SMAA.
      native_shaders_definitions.emplace(CompileTimeStringHash("MOHA Sharpen PS"),
         ShaderDefinition{"Luma_RCAS_PS", reshade::api::pipeline_subobject_type::pixel_shader, nullptr, "sharpen_ps"});
#endif

      sr_game_tooltip = "Requires Luma-Upscaler.exe next to the game's exe.\n";

      // DLSS/FSR: its depth and the camera motion from the scene's alpha, the CPU copies of vc4 (dgVoodoo maps it or updates it), and the
      // motion vector target written by every blend state
      native_shaders_definitions.emplace(CompileTimeStringHash("MOHA Motion Vector Fill CS"),
         ShaderDefinition("Luma_MOHA_MotionVectorFill", reshade::api::pipeline_subobject_type::compute_shader));
      // DLSS/FSR: the gather's DoF amount accumulated per pixel (the jittered depth flips it at silhouettes)
      native_shaders_definitions.emplace(CompileTimeStringHash("MOHA DOF History CS"),
         ShaderDefinition("Luma_MOHA_DOFHistory", reshade::api::pipeline_subobject_type::compute_shader));
      reshade::register_event<reshade::addon_event::map_buffer_region>(OnMapBufferRegion);
      reshade::register_event<reshade::addon_event::unmap_buffer_region>(OnUnmapBufferRegion);
      reshade::register_event<reshade::addon_event::update_buffer_region>(OnUpdateBufferRegion);
      // dgVoodoo's per-target blend repaired too (TW2's water, see "PatchedDraws::OnCreateBlendState"), preventive here
      reshade::register_event<reshade::addon_event::create_pipeline>(PatchedDraws::OnCreateBlendState<MotionVectorPatches::target_slot, MotionVectorPatches::reactive_slot, true>);
      reshade::register_event<reshade::addon_event::clear_depth_stencil_view>(OnClearDepthStencilView);

      // Buffers stay in GAMMA space: the gamma-SDR HUD blends onto this canvas and a linear buffer washes it out.
      // Replaced passes pre-scale by Game/UIPaperWhite (UI_DRAW_TYPE 2); core composition encodes scRGB.
      GetShaderDefineData(POST_PROCESS_SPACE_TYPE_HASH).SetDefaultValue('0');
      GetShaderDefineData(EARLY_DISPLAY_ENCODING_HASH).SetDefaultValue('0');
      GetShaderDefineData(VANILLA_ENCODING_TYPE_HASH).SetDefaultValue('1'); // game shipped gamma-2.2 SDR
      GetShaderDefineData(GAMMA_CORRECTION_TYPE_HASH).SetDefaultValue('1');
      GetShaderDefineData(GAMUT_MAPPING_TYPE_HASH).SetDefaultValue('1'); // gamut-map wild colors in composition
      GetShaderDefineData(UI_DRAW_TYPE_HASH).SetDefaultValue('2');       // HUD gets its own UIPaperWhite + gamma blend

      // dgVoodoo binds b0-b5 only (measured on every captured draw), so b9/b10 (the motion vector draws, see
      // "MotionVectorPatches"), b11 (core DrawBloom's own constants) and b12/b13 are free for Luma.
      // luma_data is used by the Display Composition; luma_ui stays off (UI drawn by the game).
      luma_settings_cbuffer_index = 13;
      luma_data_cbuffer_index = 12;
      luma_ui_cbuffer_index = -1;

      // Manual Scene + UI Paper White sliders instead of the OS HDR reference level. Core gates the separate
      // "UI Paper White" slider on UI_DRAW_TYPE >= 1 && !use_os_reference_white_level.
      use_os_reference_white_level = false;

      // User grade controls (read in Luma_MOHA_Tonemap.hlsl via LumaSettings.GameSettings). All vanilla by default.
      default_luma_global_game_settings.Exposure = 1.f; // multiplier (1x)
      default_luma_global_game_settings.Saturation = 1.f;
      default_luma_global_game_settings.HighlightsDesaturation = 0.f; // off by default; only the mandatory DICE/gamut desaturation applies
      default_luma_global_game_settings.BloomIntensity = 1.f;
      default_luma_global_game_settings.Contrast = 1.f;
      default_luma_global_game_settings.Dithering = 1.f; // subtle anti-banding on by default
      default_luma_global_game_settings.LumaBloomEnable = ENABLE_BLOOM ? 1.f : 0.f;
      // 1.0 is exactly where the game's own bright-pass sits, and the scene peaks at ~3.9 — only real sources bloom.
      default_luma_global_game_settings.BloomThreshold = 1.f;
      // Light AutoHDR on the Bink movie pass (Video_0x1AAC12AD): movies bypass the scene passes entirely, so
      // without it they sit flat at paper white. The pair is BL2's calibrated one (peak ~165 nits at 0.5 against an
      // 80-nit white, i.e. ~419 nits at a 203-nit paper white).
      default_luma_global_game_settings.VideoAutoHDREnable = 1.f;
      default_luma_global_game_settings.VideoAutoHDRBoost = 0.5f;
      cb_luma_global_settings.GameSettings = default_luma_global_game_settings;
   }

   void OnCreateDevice(ID3D11Device* native_device, DeviceData& device_data) override
   {
      device_data.game = new MedalOfHonorAirborneGameDeviceData;
      device_data.taa_detected = true; // No TAA to replace, but Core's upscaler status checks for it
#if DEVELOPMENT
      // For the MCP "luma_dev_values" tool: the last complete frame's counts
      const auto& stats = GetGameDeviceData(device_data).mv_last_stats;
      Mcp::RegisterCounters({{"mv.draws", &stats.motion_vector_draws}, {"mv.jitter_draws", &stats.jitter_draws}, {"mv.matched", &stats.matched}, {"mv.camera_only", &stats.camera_only},
                               {"mv.other_camera", &stats.other_camera}, {"mv.no_camera", &stats.no_camera}, {"mv.uncopied", &stats.uncopied}, {"mv.maps", &stats.maps}, {"mv.updates", &stats.updates},
                               {"mv.other_maps", &stats.other_maps}, {"mv.sr_draws", &stats.sr_draws}, {"mv.tiebreak_collisions", &stats.tiebreak_collisions}, {"mv.reactive_draws", &stats.reactive_draws}, {"mv.motion_blur_replays", &stats.motion_blur_replays}, {"mv.motion_blur_masked", &stats.motion_blur_masked}, { "mv.ended_by_hash",
                                  &stats.ended_by }},
         &device_data);
      for (size_t i = 0; i < std::size(motion_vector_reject_names); i++)
      {
         Mcp::RegisterCounter(std::string("mv.rejected.") + motion_vector_reject_names[i], &stats.rejected[i], &device_data);
      }
#endif
   }

   // Core calls this at device destruction but never frees "device_data.game", so the allocation is ours (TW2/BL2).
   // GameDeviceData has no virtual destructor: delete through the concrete type or members leak.
   void OnDestroyDeviceData(DeviceData& device_data) override
   {
#if DEVELOPMENT
      Mcp::Unregister(&device_data);
#endif
      delete static_cast<MedalOfHonorAirborneGameDeviceData*>(device_data.game);
      device_data.game = nullptr;
   }

#if ENABLE_SMAA
   // SMAA on the graded gamma canvas from the post-draw callback, so it lands after the grade and before the HUD.
   // TW2/BL2 chain: snapshot -> SRV -> DrawSMAA, last pass into the canvas RTV. No-op if incomplete. Without "smaa" (the
   // upscaler antialiased the frame, ME1's shape) only RCAS runs: snapshot -> RCAS -> canvas.
   void RunPostFinalGradeSMAA(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, DeviceData& device_data, MedalOfHonorAirborneGameDeviceData& gd, ID3D11Resource* canvas_res, ID3D11RenderTargetView* canvas_rtv, bool smaa)
   {
      uint4 cinfo{};
      DXGI_FORMAT cfmt = DXGI_FORMAT_UNKNOWN;
      GetResourceInfo(canvas_res, cinfo, cfmt);
      const uint32_t w = cinfo.x, h = cinfo.y;
      if (w == 0 || h == 0 || cfmt == DXGI_FORMAT_UNKNOWN)
         return;

      // Resolution change: drop every size-bound resource of ours, so each is recreated at the new size (below; Core's DrawSMAA
      // checks its own). The predication CB does not depend on the size and stays.
      if (gd.smaa_w != w || gd.smaa_h != h)
      {
         gd.ReleaseSMAAScratch();
         gd.cb_smaa_metrics.reset();
         gd.cb_sharpen.reset();
         gd.smaa_w = w;
         gd.smaa_h = h;
      }

      // RCAS decides the chain's SHAPE, so resolve it before allocating anything: with sharpening off the last SMAA pass writes the
      // canvas directly, which removes both a full-frame write-back and the intermediate. Core's fullscreen "Copy VS", shared by RCAS
      // and by the predication debug view.
      auto* copy_vs = FindShader(device_data.native_vertex_shaders, CompileTimeStringHash("Copy VS"));
      auto* sharpen_ps = FindShader(device_data.native_pixel_shaders, CompileTimeStringHash("MOHA Sharpen PS"));
      bool do_sharpen = g_rcas_sharpness > 0.f && copy_vs != nullptr && sharpen_ps != nullptr;
      if (do_sharpen && (!gd.cb_sharpen || gd.sharpen_amount != g_rcas_sharpness))
      {
         const float sp[4] = {(float)w, (float)h, g_rcas_sharpness, 0.f};
         if (CreateImmutableCB(native_device, sp, sizeof(sp), gd.cb_sharpen))
         {
            gd.sharpen_amount = g_rcas_sharpness;
         }
      }
      do_sharpen = do_sharpen && gd.cb_sharpen;
      const auto sharpen = [&](ID3D11ShaderResourceView* source)
      {
         DrawStateStack<DrawStateStackType::FullGraphics> sharpen_state;
         sharpen_state.Cache(native_device_context, device_data.uav_max_count);
         ID3D11Buffer* scb = gd.cb_sharpen.get();
         native_device_context->PSSetConstantBuffers(0, 1, &scb);
         DrawCustomPixelShader(native_device_context, device_data.default_depth_stencil_state.get(), device_data.default_blend_state.get(), nullptr,
            copy_vs, sharpen_ps, source, canvas_rtv, w, h, false);
         sharpen_state.Restore(native_device_context);
      };

      if (!smaa)
      {
         if (!do_sharpen)
            return;
         if (!gd.srv_input)
         {
            gd.tex_input.reset();
            if (CreateDefaultTex(native_device, w, h, D3D11_BIND_SHADER_RESOURCE, gd.tex_input, cfmt))
            {
               native_device->CreateShaderResourceView(gd.tex_input.get(), nullptr, gd.srv_input.put());
            }
         }
         if (!gd.srv_input)
            return;
         native_device_context->CopyResource(gd.tex_input.get(), canvas_res);
         sharpen(gd.srv_input.get());
         return;
      }

      gd.smaa_idle_frames = 0;

      // Shader-readiness gate (async loader / dev live-reload): skip SMAA this frame if anything is missing.
      auto* linearize_cs = FindShader(device_data.native_compute_shaders, CompileTimeStringHash("MOHA SMAA Linearize CS"));
      const bool smaa_ready = linearize_cs != nullptr &&
                              HasSMAAShaders(device_data);
      if (!smaa_ready)
         return;

      // Edge-ness from the scene alpha (linear depth). Scale and mask fall back together: 2.0 with a null mask would
      // raise the threshold frame-wide. The CS maps texels 1:1, hence the size check.
      auto* pred_cs = FindShader(device_data.native_compute_shaders, CompileTimeStringHash("MOHA Depth Extract CS"));
      bool pred_ok = g_smaa_predication && gd.srv_scene.get() != nullptr && pred_cs != nullptr;
      if (pred_ok)
      {
         uint4 sinfo{};
         DXGI_FORMAT sfmt = DXGI_FORMAT_UNKNOWN;
         GetResourceInfo(gd.srv_scene.get(), sinfo, sfmt);
         pred_ok = sinfo.x == w && sinfo.y == h;
      }
      if (pred_ok)
      {
         if (!gd.cb_pred || gd.pred_tolerance != g_smaa_pred_tolerance)
         {
            const float p[4] = {g_smaa_pred_tolerance, 0.f, 0.f, 0.f};
            if (CreateImmutableCB(native_device, p, sizeof(p), gd.cb_pred))
               gd.pred_tolerance = g_smaa_pred_tolerance;
         }
         if (!gd.uav_pred || !gd.srv_pred)
         {
            gd.ReleasePredicationScratch();
            if (CreateDefaultTex(native_device, w, h, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, gd.tex_pred, DXGI_FORMAT_R16_FLOAT))
            {
               native_device->CreateUnorderedAccessView(gd.tex_pred.get(), nullptr, gd.uav_pred.put());
               native_device->CreateShaderResourceView(gd.tex_pred.get(), nullptr, gd.srv_pred.put());
            }
         }
         pred_ok = gd.cb_pred && gd.uav_pred && gd.srv_pred;
      }

      const float pred_scale = pred_ok ? 2.f : 1.f;
      if (!gd.cb_smaa_metrics || gd.smaa_metrics_pred_scale != pred_scale)
      {
         const float metrics[8] = {1.f / (float)w, 1.f / (float)h, (float)w, (float)h, pred_scale, 0.f, 0.f, 0.f};
         if (CreateImmutableCB(native_device, metrics, sizeof(metrics), gd.cb_smaa_metrics))
            gd.smaa_metrics_pred_scale = pred_scale;
      }
      if (!gd.cb_smaa_metrics)
         return;

      if (do_sharpen)
      {
         if (!gd.tex_smaa_out_rtv || !gd.tex_smaa_out_srv)
         {
            gd.ReleaseSharpenScratch();
            if (CreateDefaultTex(native_device, w, h, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET, gd.tex_smaa_out, cfmt))
            {
               native_device->CreateRenderTargetView(gd.tex_smaa_out.get(), nullptr, gd.tex_smaa_out_rtv.put());
               native_device->CreateShaderResourceView(gd.tex_smaa_out.get(), nullptr, gd.tex_smaa_out_srv.put());
            }
         }
         if (!gd.tex_smaa_out_rtv || !gd.tex_smaa_out_srv)
            do_sharpen = false; // allocation failed: fall back to the un-sharpened chain rather than dropping SMAA
      }

      if (!gd.srv_input || !gd.uav_input_linear || !gd.srv_input_linear)
      {
         gd.srv_input.reset();
         gd.tex_input.reset();
         gd.uav_input_linear.reset();
         gd.srv_input_linear.reset();
         if (CreateDefaultTex(native_device, w, h, D3D11_BIND_SHADER_RESOURCE, gd.tex_input, cfmt) &&
             CreateDefaultTex(native_device, w, h, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, gd.tex_input_linear, DXGI_FORMAT_R16G16B16A16_FLOAT))
         {
            native_device->CreateShaderResourceView(gd.tex_input.get(), nullptr, gd.srv_input.put());
            native_device->CreateUnorderedAccessView(gd.tex_input_linear.get(), nullptr, gd.uav_input_linear.put());
            native_device->CreateShaderResourceView(gd.tex_input_linear.get(), nullptr, gd.srv_input_linear.put());
         }
      }
      if (!gd.srv_input || !gd.uav_input_linear || !gd.srv_input_linear)
         return;

      native_device_context->CopyResource(gd.tex_input.get(), canvas_res);

      {
         DrawStateStack<DrawStateStackType::Compute> compute_state;
         compute_state.Cache(native_device_context, device_data.uav_max_count);

         // Linear-light decode of the snapshot for the neighborhood blend (Luma_MOHA_SMAALinearize.hlsl).
         ID3D11ShaderResourceView* lin_srv = gd.srv_input.get();
         ID3D11UnorderedAccessView* lin_uav = gd.uav_input_linear.get();
         native_device_context->CSSetUnorderedAccessViews(0, 1, &lin_uav, nullptr);
         native_device_context->CSSetShaderResources(0, 1, &lin_srv);
         native_device_context->CSSetShader(linearize_cs, nullptr, 0);
         native_device_context->Dispatch((w + 7) / 8, (h + 7) / 8, 1);

         // Scene alpha (linear depth) -> plane-deviation edge-ness in R16F; see Luma_MOHA_DepthExtract.hlsl for why
         // this is an edge test rather than a depth rescale.
         if (pred_ok)
         {
            ID3D11ShaderResourceView* pred_srv = gd.srv_scene.get();
            ID3D11UnorderedAccessView* pred_uav = gd.uav_pred.get();
            ID3D11Buffer* pred_cb = gd.cb_pred.get();
            native_device_context->CSSetUnorderedAccessViews(0, 1, &pred_uav, nullptr);
            native_device_context->CSSetShaderResources(0, 1, &pred_srv);
            native_device_context->CSSetConstantBuffers(0, 1, &pred_cb);
            native_device_context->CSSetShader(pred_cs, nullptr, 0);
            native_device_context->Dispatch((w + 7) / 8, (h + 7) / 8, 1);
         }

         compute_state.Restore(native_device_context);
      }

#if DEVELOPMENT
      // Calibration aid, driven from the Anti-Aliasing section. Reads the mask that was just written.
      if (pred_ok && g_smaa_pred_debug)
      {
         auto* copy_ps = FindShader(device_data.native_pixel_shaders, CompileTimeStringHash("Copy PS"));
         if (copy_vs != nullptr && copy_ps != nullptr)
         {
            // The mask is single-channel, so the core copy lands it in RED — unmistakably a debug view. Replaces
            // the antialiased frame rather than blending over it, hence the early return.
            DrawStateStack<DrawStateStackType::FullGraphics> debug_state;
            debug_state.Cache(native_device_context, device_data.uav_max_count);
            DrawCustomPixelShader(native_device_context, device_data.default_depth_stencil_state.get(), device_data.default_blend_state.get(), nullptr,
               copy_vs, copy_ps, gd.srv_pred.get(), canvas_rtv, w, h, false);
            debug_state.Restore(native_device_context);
            return;
         }
      }
#endif

      // Metrics CB at VS+PS b1. DrawSMAA restores VS/PS/SRVs/RTs but not cbuffers, and a full state stack every SMAA
      // frame would be wasted work for two slots, so only these two are saved.
      ComPtr<ID3D11Buffer> vs_cb1_orig, ps_cb1_orig;
      native_device_context->VSGetConstantBuffers(1, 1, vs_cb1_orig.put());
      native_device_context->PSGetConstantBuffers(1, 1, ps_cb1_orig.put());
      ID3D11Buffer* mcb = gd.cb_smaa_metrics.get();
      native_device_context->VSSetConstantBuffers(1, 1, &mcb);
      native_device_context->PSSetConstantBuffers(1, 1, &mcb);

      // Reading the canvas as the target is safe: the chain samples the snapshot, never the canvas itself.
      DrawSMAA(native_device, native_device_context, device_data, do_sharpen ? gd.tex_smaa_out_rtv.get() : canvas_rtv, gd.srv_input_linear.get(), gd.srv_input.get(), pred_ok ? gd.srv_pred.get() : nullptr /*predication signal*/);

      // RCAS on the SMAA output, written into the canvas.
      if (do_sharpen)
      {
         sharpen(gd.tex_smaa_out_srv.get());
      }

      ID3D11Buffer* vcb = vs_cb1_orig.get();
      ID3D11Buffer* pcb = ps_cb1_orig.get();
      native_device_context->VSSetConstantBuffers(1, 1, &vcb);
      native_device_context->PSSetConstantBuffers(1, 1, &pcb);
   }
#endif // ENABLE_SMAA

   DrawOrDispatchOverrideType OnDrawOrDispatch(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, CommandListData& cmd_list_data, DeviceData& device_data, reshade::api::shader_stage stages, const ShaderHashesList<OneShaderPerPipeline>& original_shader_hashes, bool is_custom_pass, bool& updated_cbuffers, std::function<void()>* original_draw_dispatch_func) override
   {
      auto& gd = GetGameDeviceData(device_data);

      const bool is_immediate = native_device_context->GetType() == D3D11_DEVICE_CONTEXT_IMMEDIATE;

      // Hide HUD: cancel draws that run AFTER the final color pass AND target the same canvas. The render-target test
      // is load-bearing - "everything after the tonemap" also swallows the present blit.
      if (g_hide_ui && is_immediate && !is_custom_pass && device_data.has_drawn_main_post_processing && gd.canvas_res)
      {
         ComPtr<ID3D11RenderTargetView> rtv;
         ComPtr<ID3D11Resource> rt;
         GetBoundRenderTarget(native_device_context, std::addressof(rtv), std::addressof(rt)); // com_ptr's operator& is not the address
         if (rt.get() == gd.canvas_res.get())
            return DrawOrDispatchOverrideType::Replaced;
      }

      // DLSS/FSR: the scene's draws with motion vectors or jitter (see "DrawWithMotionVectors"), and the upscaler at its first post pass
      if (gd.mv_active && is_immediate && !gd.mv_scene_done)
      {
         // The scene's first post pass (the gather, or a final pass in gather-less chains): the upscaler runs right before it
         if (IsDofBloomGather(original_shader_hashes) || IsFinalColorPass(original_shader_hashes))
         {
            if (gd.mv_scene_open)
            {
#if DEVELOPMENT
               gd.mv_stats.ended_by = uint32_t(original_shader_hashes.pixel_shaders[0]);
#endif
               // The scene or a copy of it (same description, CopyResource compatible) among the pass's textures
               gd.mv_scene_copy.reset();
               gd.mv_scene_read_by_end = false;
               if (gd.mv_scene_color)
               {
                  com_ptr<ID3D11ShaderResourceView> srvs[8];
                  native_device_context->PSGetShaderResources(0, UINT(std::size(srvs)), &srvs[0]);
                  for (const auto& srv : srvs)
                  {
                     com_ptr<ID3D11Resource> resource;
                     if (srv)
                     {
                        srv->GetResource(&resource);
                     }
                     if (!resource || (resource != gd.mv_scene_color && !AreResourcesEqual(resource.get(), gd.mv_scene_color.get())))
                        continue;
                     if (resource == gd.mv_scene_color)
                     {
                        gd.mv_scene_read_by_end = true;
                     }
                     else if (!gd.mv_scene_copy)
                     {
                        gd.mv_scene_copy = resource;
                     }
                  }
               }
               EndScene(native_device, native_device_context, device_data);
            }
         }
         // The motion blur after the upscaler, not before it (it would get a blurred scene): held back, replayed at the scene's end
         else if (gd.mv_scene_open && IsSRActive(device_data) && !gd.mv_motion_blur.pending && ContainsPixelShader(original_shader_hashes, kMotionBlurHash, kMotionBlurHash_v281) &&
                  DeferMotionBlur(native_device, native_device_context, gd))
         {
            return DrawOrDispatchOverrideType::Replaced;
         }
         else if (!is_custom_pass && original_draw_dispatch_func && *original_draw_dispatch_func && (stages & reshade::api::shader_stage::vertex) == reshade::api::shader_stage::vertex)
         {
            com_ptr<ID3D11RenderTargetView> rtvs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
            com_ptr<ID3D11DepthStencilView> dsv;
            native_device_context->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, &rtvs[0], &dsv);
            if (!gd.mv_scene_open && dsv)
            {
               OpenScene(native_device, native_device_context, cmd_list_data, device_data, original_shader_hashes.vertex_shaders[0], dsv.get());
            }
            const std::function<void()>& draw = *original_draw_dispatch_func;
            const bool motion_vectors = DrawWithMotionVectors(native_device, native_device_context, cmd_list_data, device_data, original_shader_hashes, draw, rtvs, dsv.get());
            const bool jitter = !motion_vectors && DrawWithJitter(native_device, native_device_context, cmd_list_data, device_data, original_shader_hashes, draw, rtvs, dsv.get());
#if DEVELOPMENT
            Mcp::Annotate(cmd_list_data, motion_vectors ? "mv" : (jitter ? "jitter" : "unpatched"));
            if (const int reject = std::exchange(gd.mv_draw_reject, -1); reject >= 0)
            {
               Mcp::Annotate(cmd_list_data, "mv_reject", reject);
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
      // Producer-side gate for the bloom buffer upgrade: the devkit cannot see an indirect upgrade at all, so the
      // render target bound at this draw is the only place the fp16 mirror is observable.
      if (is_immediate && !gd.diag_logged_gather && IsDofBloomGather(original_shader_hashes))
      {
         gd.diag_logged_gather = true;
         DumpBoundRenderTarget(native_device_context, "bloom buffer");
      }
#endif

#if ENABLE_BLOOM
      // The gather is where the engine hands over its per-area Bloom_Scale, so it is where we take it. Once per
      // frame: a second capture would advance the ring twice and read a slot that is still in flight.
      if (g_luma_bloom_enable && is_immediate && !gd.bloom_scale_captured_this_frame && IsDofBloomGather(original_shader_hashes))
      {
         gd.bloom_scale_captured_this_frame = true;
         TrackBloomScale(native_device, native_device_context, &gd);
      }
#endif

      // Wrapper-build telemetry, deliberately AHEAD of the gate below: an unkeyed dgVoodoo build has to be
      // reported whichever context recorded the pass (the warning itself fires in OnPresent).
      // The hash lookup runs only while one of its two consumers still needs it: the telemetry until the first match,
      // the final-pass gate until the pass has run this frame.
      const bool final_pass_pending = is_immediate && !device_data.has_drawn_main_post_processing;
      const bool is_final_color_pass = (final_pass_pending || !gd.ever_matched_final_pass) && IsFinalColorPass(original_shader_hashes);
      if (is_final_color_pass)
         gd.ever_matched_final_pass = true;

      // The final color pass ends main post processing. Read the hash list before any early-out: is_custom_pass is
      // true for hash-replaced passes too. Gated on is_immediate (BL GOTY does the same).
      if (final_pass_pending && is_final_color_pass)
      {
         device_data.has_drawn_main_post_processing = true;

         // Push LumaSettings HERE, at the seam, not inside a feature block: every consumer below reads b13 (the
         // bloom prefilter's BloomThreshold, and the grade itself), and the SMAA path runs the original draw and
         // returns Replaced, which makes core skip its own upload for this draw entirely (core.hpp: the override
         // check returns before SetLumaConstantBuffers). With this inside the bloom block, turning Luma bloom off
         // left the grade reading the previous upload. Above the state stack too: Restore() rolls PS constant
         // buffers back wholesale. "updated_cbuffers" is left alone so core still uploads on non-Replaced frames.
         SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, reshade::api::shader_stage::pixel, LumaConstantBufferType::LumaSettings);

         // Remember what this pass draws into: Hide UI needs the resource identity, SMAA needs the view. Captured
         // every frame because an indirect upgrade or a resolution change can swap the mirror.
         ComPtr<ID3D11RenderTargetView> canvas_rtv;
         GetBoundRenderTarget(native_device_context, std::addressof(canvas_rtv), std::addressof(gd.canvas_res));

         // The scene is bound at t0 right here, and its alpha is the depth SMAA predication reads — so unlike the
         // TW2 donor there is no separate capture pass to keep in sync with this one.
         gd.srv_scene.reset();
         native_device_context->PSGetShaderResources(0, 1, gd.srv_scene.put());

#if DEVELOPMENT
         // Primary gate for the canvas upgrade: with indirect upgrades the devkit only ever sees the original
         // r8g8b8a8 resource, so the bound render target here is the only honest read of the fp16 mirror.
         if (!gd.diag_logged_rt)
         {
            gd.diag_logged_rt = true;
            DumpBoundRenderTarget(native_device_context, "canvas");
         }
#endif

#if ENABLE_BLOOM
         // Luma bloom pyramid off the fp16 LINEAR scene at t0, pre-glow by construction (the halo is added later, in
         // the grade). Karis average first: no TAA, so fireflies die spatially.
         gd.srv_luma_bloom.reset(); // DrawBloom AddRef's its mip 0 into this
         if (g_luma_bloom_enable && gd.srv_scene)
         {
            DrawStateStack<DrawStateStackType::FullGraphics> bloom_state;
            bloom_state.Cache(native_device_context, device_data.uav_max_count);

            ComPtr<ID3D11ShaderResourceView> srv_karis;
            DrawKarisAverage(native_device, native_device_context, device_data, gd.srv_scene.get(), srv_karis.put());
            if (srv_karis)
               DrawBloom(native_device, native_device_context, device_data, srv_karis.get(), kBloomNMips, g_bloom_sigmas, gd.srv_luma_bloom.put());

            bloom_state.Restore(native_device_context);
         }
         {
            // Bound every frame, null included: the composite is gated on LumaBloomEnable, not on the slot, and
            // dgVoodoo's placeholder would be sampled as garbage. The composite ADDS this.
            ID3D11ShaderResourceView* bloom_srv = gd.srv_luma_bloom.get();
            native_device_context->PSSetShaderResources(kLumaBloomSlot, 1, &bloom_srv);
         }
#endif

#if ENABLE_SMAA
         // Run the grade ourselves, then SMAA on its output, so the antialiasing lands before the HUD. Falls back to
         // a plain draw when the callback is unavailable (one frame without AA) rather than skipping the grade. On a
         // frame the upscaler already antialiased only RCAS runs.
         const bool antialias = (device_data.has_drawn_sr ? (g_rcas_sharpness > 0.f) : g_smaa_enable);
         if (antialias && original_draw_dispatch_func != nullptr && canvas_rtv && gd.canvas_res)
         {
            (*original_draw_dispatch_func)();
            RunPostFinalGradeSMAA(native_device, native_device_context, device_data, gd, gd.canvas_res.get(), canvas_rtv.get(), !device_data.has_drawn_sr);
            return DrawOrDispatchOverrideType::Replaced; // we ran the original draw ourselves
         }
#endif
      }

      // DLSS/FSR: the gather takes its DoF amount from the per pixel history at t1 (see "DrawDOFHistory"), drawn here so the slot goes
      // back to dgVoodoo's binding after it (its state cache would skip rebinding it)
      if (is_immediate && original_draw_dispatch_func && *original_draw_dispatch_func && IsDofBloomGather(original_shader_hashes))
      {
         if (ID3D11ShaderResourceView* const history = DrawDOFHistory(native_device, native_device_context, device_data))
         {
            com_ptr<ID3D11ShaderResourceView> game_srv;
            native_device_context->PSGetShaderResources(1, 1, &game_srv);
            native_device_context->PSSetShaderResources(1, 1, &history);
            // Core uploads none for a draw we replaced, and the gather reads LumaSettings (Luma bloom)
            SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, reshade::api::shader_stage::pixel, LumaConstantBufferType::LumaSettings);
            (*original_draw_dispatch_func)();
            ID3D11ShaderResourceView* const restored = game_srv.get();
            native_device_context->PSSetShaderResources(1, 1, &restored);
            return DrawOrDispatchOverrideType::Replaced;
         }
      }

      return DrawOrDispatchOverrideType::None; // never cancel the original draw (the replacement is by hash)
   }

   void CleanExtraSRResources(DeviceData& device_data) override
   {
      GetGameDeviceData(device_data).release_sr_resources = true;
   }

   void OnPresent(ID3D11Device* native_device, DeviceData& device_data) override
   {
      auto& gd = GetGameDeviceData(device_data);

      // DLSS/FSR: the history restarts after any frame it didn't draw (menus, loading, just picked); the selection and the motion
      // vector state are fixed here for the next frame (see "IsSRActive")
      gd.sr_active = LatchSRFrame(device_data);
      gd.mv_active = IsSRActive(device_data) || g_mv_enable;
      // None picked: Core stopped the SR bridge's helper ("ReleaseResources"), our upscaler inputs and output go too. Recreated when an
      // upscaler is picked again (the helper takes seconds to start).
      if (device_data.sr_type == SR::Type::None && gd.release_sr_resources.exchange(false))
      {
         gd.sr_output_srv.reset();
         gd.ReleaseDOFHistory();
         if (!g_mv_enable)
         {
            const std::unique_lock lock(gd.mv_mutex);
            gd.ReleaseMotionVectorTargets();
            // The game's scene, depth and scene copy (taken again at the next scene), so a resize after None doesn't keep the old
            // ones alive; and the vc4 copies the object tables hold
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
            const std::lock_guard constants_lock(gd.mv_constants_mutex);
            gd.mv_constants_pool.clear();
            gd.mv_constants_pool_free.clear();
         }
      }
      // A scene no post pass ended ends here: its jitter must not reach the next frame's draws before its first mesh
      gd.mv_scene_open = false;
      gd.mv_scene_done = false;
      gd.mv_frame_ended = true;
      gd.mv_fill_pending = false;
      gd.mv_fsr_masks = false;
      gd.mv_motion_blur.pending = false;
      // The DoF history continues only from a frame that drew it (menus, loading and frames without the gather restart it)
      gd.dof_history_valid = std::exchange(gd.dof_history_drawn, false);
      {
         // The pooled vc4 copies only the pool holds (superseded, no object keeps them) are free for the next ones, as many as the last
         // frame asked for: frames without a scene (loading, videos, menus) still copy every Unmap, and would keep their peak otherwise.
         // Without motion vectors, none.
         const std::lock_guard lock(gd.mv_constants_mutex);
         size_t kept_free = (gd.mv_active ? std::exchange(gd.mv_constants_made, 0) : 0);
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
      if (!custom_texture_mip_lod_bias_offset)
      {
         const std::unique_lock lock(s_mutex_samplers);
         // -1 at native resolution (Core biases the anisotropic samplers, all of the game's with the AF16x upgrade)
         device_data.texture_mip_lod_bias_offset = (IsSRActive(device_data) ? SR::GetMipLODBias(device_data.output_resolution.y, device_data.output_resolution.y) : 0.f);
      }
#if DEVELOPMENT
      gd.mv_last_stats = std::exchange(gd.mv_stats, {});
      // The DEV panel's counts in ReShade.log every 300 frames while motion vectors run
      if (const auto& stats = gd.mv_last_stats; gd.mv_active && cb_luma_global_settings.FrameIndex % 300 == 0)
         reshade::log::message(reshade::log::level::info, std::format("[MOHA MV] frame {}: {} mv ({} matched, {} camera only, {} other camera, {} no camera, {} uncopied), {} jitter, {} maps, {} updates, {} other maps, sr {} ({}), motion blur replays {} (masked {}), near {:.3f} far {:.0f}, ended by 0x{:08X} (copy {}, scene read {}), refused {}/{}/{}/{}/{}/{}/{}/{} (last format {} {}x{})",
                                                             cb_luma_global_settings.FrameIndex, stats.motion_vector_draws, stats.matched, stats.camera_only, stats.other_camera, stats.no_camera, stats.uncopied, stats.jitter_draws, stats.maps, stats.updates, stats.other_maps, stats.sr_draws, int(device_data.sr_type), stats.motion_blur_replays, stats.motion_blur_masked,
                                                             stats.near_plane, stats.far_plane, stats.ended_by, gd.mv_scene_copy != nullptr, gd.mv_scene_read_by_end, stats.rejected[0], stats.rejected[1], stats.rejected[2], stats.rejected[3], stats.rejected[4], stats.rejected[5], stats.rejected[6], stats.rejected[7],
                                                             stats.rejected_format, stats.rejected_width, stats.rejected_height)
                                                             .c_str());
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

      // --- Per-frame capture state, re-armed for the next frame ---
      gd.canvas_res.reset(); // do not hold a reference across frames: it would outlive a resize or a mirror swap
      gd.srv_scene.reset();  // recaptured at the final pass every frame; never hold it across one
#if ENABLE_BLOOM
      gd.bloom_scale_captured_this_frame = false; // re-arm the once-per-frame ring advance

      // --- Derived settings ---
      // THE SOLE WRITER of the effective BloomIntensity. The UI only ever touches the raw slider and the enable
      // flag; if it wrote this field too, the two would fight each other while the slider is dragged.
      {
         auto& gs = cb_luma_global_settings.GameSettings;
         float effective = g_bloom_intensity;
         // Only while the Luma pyramid is the active bloom: with it off the vanilla glow already carries the engine's
         // area scale from inside the gather, so riding the scale on top would count it twice.
         if (g_luma_bloom_enable && gd.bloom_scale_live >= 0.f)
            effective = g_bloom_intensity * std::clamp(gd.bloom_scale_live, 0.f, 4.f);
         if (std::abs(gs.BloomIntensity - effective) > 1e-4f)
         {
            gs.BloomIntensity = effective;
            device_data.cb_luma_global_settings_dirty = true;
         }
      }
#endif

      // --- Feature-off releases: give the address space back here rather than in the ImGui handler, so it happens on
      // the render thread, never while a frame is mid-flight ---
#if ENABLE_BLOOM
      // Core's DrawKarisAverage output (~66 MB at 4K) and DrawBloom's mip chains (~66 MB). Unconditional while off: resetting
      // empty entries is a few lookups.
      if (!g_luma_bloom_enable)
      {
         ReleaseKarisAverage(device_data);
         ReleaseBloom();
      }
#endif
#if ENABLE_SMAA
      // Also once SMAA stopped running (the upscaler antialiases every frame it draws); RCAS on the upscaler keeps the snapshot
      if (!g_smaa_enable || ++gd.smaa_idle_frames > smaa_idle_release_frames)
      {
         // Unconditional while idle, as the bloom's above
         if (IsSRActive(device_data) && g_rcas_sharpness > 0.f)
         {
            gd.uav_input_linear.reset();
            gd.srv_input_linear.reset();
            gd.tex_input_linear.reset();
            gd.ReleasePredicationScratch();
            gd.ReleaseSharpenScratch();
         }
         else if (gd.tex_input)
         {
            gd.ReleaseSMAAScratch();
            gd.smaa_w = gd.smaa_h = 0; // core recreates lazily; keep the latch from claiming anything is current
         }
         ReleaseSMAA(device_data);
      }
      else
      {
         if (!g_smaa_predication && gd.tex_pred)
            gd.ReleasePredicationScratch();
         if (g_rcas_sharpness <= 0.f && gd.tex_smaa_out)
            gd.ReleaseSharpenScratch();
      }
#endif

      // --- One-shot wrapper-build telemetry (BL GOTY precedent) ---
      // The frame budget is warmup only: the menu runs the same pass, so a few frames are enough.
      constexpr uint32_t kBuildCheckFrame = 120;
      if (gd.frames_presented < kBuildCheckFrame && ++gd.frames_presented == kBuildCheckFrame && !gd.ever_matched_final_pass)
         reshade::log::message(reshade::log::level::warning,
            "[Luma] MOHA: no keyed final color pass seen after warmup -- the dgVoodoo build is probably neither 2.87.3/2.87.5 nor 2.81.3, so every shader replacement is inactive (re-dump the shaders for it).");
   }

   void LoadConfigs() override
   {
      // Grade sliders (cb_luma_global_settings_dirty is already true at init -> uploaded on first frame).
      reshade::get_config_value(nullptr, NAME, "Exposure", cb_luma_global_settings.GameSettings.Exposure);
      reshade::get_config_value(nullptr, NAME, "Saturation", cb_luma_global_settings.GameSettings.Saturation);
      reshade::get_config_value(nullptr, NAME, "HighlightsDesaturation", cb_luma_global_settings.GameSettings.HighlightsDesaturation);
#if ENABLE_BLOOM
      // The RAW slider; OnPresent derives GameSettings.BloomIntensity from it (see the sole-writer block there).
      // Inside the guard because it drives the Luma pyramid alone: with no pyramid there is nothing for it to scale.
      reshade::get_config_value(nullptr, NAME, "BloomIntensity", g_bloom_intensity);
      cb_luma_global_settings.GameSettings.BloomIntensity = g_bloom_intensity; // until the first Present
      reshade::get_config_value(nullptr, NAME, "LumaBloomEnable", g_luma_bloom_enable);
      cb_luma_global_settings.GameSettings.LumaBloomEnable = g_luma_bloom_enable ? 1.f : 0.f; // mirror to both shaders
#if DEVELOPMENT
      // Loaded only where its UI exists: a value a DEV session left in the ini must not silently steer Publishing.
      reshade::get_config_value(nullptr, NAME, "BloomThreshold", cb_luma_global_settings.GameSettings.BloomThreshold);
#endif
#endif
      reshade::get_config_value(nullptr, NAME, "Contrast", cb_luma_global_settings.GameSettings.Contrast);
      reshade::get_config_value(nullptr, NAME, "Dithering", cb_luma_global_settings.GameSettings.Dithering);
      reshade::get_config_value(nullptr, NAME, "VideoAutoHDREnable", cb_luma_global_settings.GameSettings.VideoAutoHDREnable);
      reshade::get_config_value(nullptr, NAME, "VideoAutoHDRBoost", cb_luma_global_settings.GameSettings.VideoAutoHDRBoost);
#if ENABLE_SMAA
      reshade::get_config_value(nullptr, NAME, "SMAAEnable", g_smaa_enable);
#if DEVELOPMENT
      // Loaded only where their UI exists: a value a DEV session left in the ini must not silently steer Publishing.
      reshade::get_config_value(nullptr, NAME, "SMAAPredication", g_smaa_predication);
      reshade::get_config_value(nullptr, NAME, "SMAAPredicationTolerance", g_smaa_pred_tolerance);
#endif
      reshade::get_config_value(nullptr, NAME, "RCASSharpness", g_rcas_sharpness);
#endif
   }

   void DrawImGuiSettings(DeviceData& device_data) override
   {
#if ENABLE_SMAA
      ImGui::SeparatorText("Anti-Aliasing");
      // The upscaler (Super Resolution, in the Settings tab) replaces SMAA: shown off, the saved choice is kept
      const bool sr_active = IsSRActive(device_data);
      ImGui::BeginDisabled(sr_active);
      bool smaa_shown = g_smaa_enable && !sr_active;
      if (ImGui::Checkbox("SMAA Enable", sr_active ? &smaa_shown : &g_smaa_enable))
      {
         reshade::set_config_value(nullptr, NAME, "SMAAEnable", g_smaa_enable);
      }
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
      {
         ImGui::SetTooltip("Adds SMAA anti-aliasing (the game has none of its own; not used with DLAA/FSR).");
      }
      ImGui::EndDisabled();
      if (g_smaa_enable || sr_active)
      {
#if DEVELOPMENT
         // Predication is not a preference: it only relaxes the edge threshold back to base ULTRA on geometry and
         // never below, so off is strictly worse. Kept as a bisect switch for devs, shipped on and out of sight.
         ImGui::BeginDisabled(sr_active);
         if (ImGui::Checkbox("SMAA Predication", &g_smaa_predication))
            reshade::set_config_value(nullptr, NAME, "SMAAPredication", g_smaa_predication);
         if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Finds edges by geometry (scene depth) instead of by brightness alone.\nKeeps textures sharp while still antialiasing real silhouettes.");
         if (ImGui::SliderFloat("SMAA Predication Tolerance", &g_smaa_pred_tolerance, 0.002f, 0.2f, "%.3f", ImGuiSliderFlags_Logarithmic))
            reshade::set_config_value(nullptr, NAME, "SMAAPredicationTolerance", g_smaa_pred_tolerance);
         if (ImGui::IsItemHovered())
            ImGui::SetTooltip("How far a surface may deviate from its local plane before it counts as an edge,\nas a fraction of view depth. Lower = more edges. This is the calibration lever,\nnot the SMAA threshold. Logarithmic: the parameter is relative.");
         ImGui::Checkbox("SMAA Predication Debug View", &g_smaa_pred_debug);
         if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Show the predication mask (red) instead of the frame.\nWant: black on flat surfaces, red across silhouettes.\nAll red = tolerance too low (predication is doing nothing).\nAll black = too high (silhouettes never regain sensitivity).");
         ImGui::EndDisabled();
#endif

         if (ImGui::SliderFloat("RCAS Sharpness", &g_rcas_sharpness, 0.f, 1.f))
            reshade::set_config_value(nullptr, NAME, "RCASSharpness", g_rcas_sharpness);
         if (ImGui::IsItemHovered())
         {
            ImGui::SetTooltip("Sharpening applied on top of SMAA, DLAA or FSR (0 = off).");
         }
         DrawResetButton(g_rcas_sharpness, 0.f, "RCASSharpness"); // writes the config itself (Serialize defaults true)
      }
#endif

      // --- Grade (read in Luma_MOHA_Tonemap.hlsl via LumaSettings.GameSettings). HDR tonemap path only except
      // Exposure, which is applied scene-referred on the vanilla SDR path as well. ---
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
            ImGui::SetTooltip("%s", tooltip);
         if (DrawResetButton(*value, default_value, key))
            device_data.cb_luma_global_settings_dirty = true;
      };

      slider("Exposure", &gs.Exposure, gs_def.Exposure, "Exposure", 2.f, "Overall image brightness (1 = vanilla).");
      slider("Contrast", &gs.Contrast, gs_def.Contrast, "Contrast", 2.f, "Overall image contrast, HDR only (1 = vanilla).");
      slider("Saturation", &gs.Saturation, gs_def.Saturation, "Saturation", 2.f, "Color saturation, HDR only (1 = vanilla).");
      slider("Highlights Desaturation", &gs.HighlightsDesaturation, gs_def.HighlightsDesaturation, "HighlightsDesaturation", 1.f,
         "How far the brightest sources fade to neutral white, HDR only (0 = keep color at any brightness).");

#if ENABLE_BLOOM
      ImGui::SeparatorText("Bloom");
      if (ImGui::Checkbox("Luma Bloom Enable", &g_luma_bloom_enable))
      {
         reshade::set_config_value(nullptr, NAME, "LumaBloomEnable", g_luma_bloom_enable);
         gs.LumaBloomEnable = g_luma_bloom_enable ? 1.f : 0.f;
         device_data.cb_luma_global_settings_dirty = true;
      }
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Replaces the game's bloom with a wider, softer HDR bloom.");

      // Everything below drives the Luma pyramid and nothing else, so it all greys out with it: no slider here
      // can reach the game's own glow.
      ImGui::BeginDisabled(!g_luma_bloom_enable);

      // Raw slider only — the effective value is derived in OnPresent, which is its sole writer.
      if (ImGui::SliderFloat("Bloom Intensity", &g_bloom_intensity, 0.f, 2.f))
         reshade::set_config_value(nullptr, NAME, "BloomIntensity", g_bloom_intensity);
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
         ImGui::SetTooltip("Bloom strength (1 = vanilla, 0 = none).");
      DrawResetButton(g_bloom_intensity, default_luma_global_game_settings.BloomIntensity, "BloomIntensity"); // writes the config itself (Serialize defaults true)

#if DEVELOPMENT
      if (ImGui::SliderFloat("Bloom Threshold", &gs.BloomThreshold, 0.f, 4.f, "%.2f"))
      {
         reshade::set_config_value(nullptr, NAME, "BloomThreshold", gs.BloomThreshold);
         device_data.cb_luma_global_settings_dirty = true;
      }
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Linear scene brightness where bloom starts. 1.0 is where the game's own bright-pass sits,\nand the scene peaks around 3.9, so only real light sources glow.\nNear 0 the whole scene glows — that is the failure mode, not a setting.");
      if (DrawResetButton(gs.BloomThreshold, default_luma_global_game_settings.BloomThreshold, "BloomThreshold"))
         device_data.cb_luma_global_settings_dirty = true;

      {
         // Says at a glance whether the capture ever succeeded, and whether the engine actually authors this per
         // area: a scale_live that never moves between locations means the mechanism is inert by construction.
         const auto* gd_ui = static_cast<const MedalOfHonorAirborneGameDeviceData*>(device_data.game);
         const float live = gd_ui != nullptr ? gd_ui->bloom_scale_live : -1.f;
         ImGui::Text("  scale_live %.4f (-1 = not captured) | effective %.4f", live, gs.BloomIntensity);
      }
#endif // DEVELOPMENT
      ImGui::EndDisabled();
#endif // ENABLE_BLOOM

      ImGui::SeparatorText("Effects");
      // Read in Video_0x1AAC12AD.ps_5_0.hlsl. Inert in SDR by construction (peak == paper white there makes
      // PumboAutoHDR an identity), so no display-mode gate is needed on either side.
      bool video_auto_hdr = gs.VideoAutoHDREnable > 0.5f;
      if (ImGui::Checkbox("Video AutoHDR", &video_auto_hdr))
      {
         gs.VideoAutoHDREnable = video_auto_hdr ? 1.f : 0.f;
         reshade::set_config_value(nullptr, NAME, "VideoAutoHDREnable", gs.VideoAutoHDREnable);
         device_data.cb_luma_global_settings_dirty = true;
      }
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Adds HDR highlights to pre-rendered videos (HDR only).");

      ImGui::BeginDisabled(!video_auto_hdr);
      if (ImGui::SliderFloat("Video HDR Boost", &gs.VideoAutoHDRBoost, 0.f, 1.f))
      {
         reshade::set_config_value(nullptr, NAME, "VideoAutoHDRBoost", gs.VideoAutoHDRBoost);
         device_data.cb_luma_global_settings_dirty = true;
      }
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
         ImGui::SetTooltip("Video highlight strength (0 = off).");
      if (DrawResetButton(gs.VideoAutoHDRBoost, default_luma_global_game_settings.VideoAutoHDRBoost, "VideoAutoHDRBoost"))
         device_data.cb_luma_global_settings_dirty = true;
      ImGui::EndDisabled();

      bool dithering = gs.Dithering > 0.5f;
      if (ImGui::Checkbox("Dithering", &dithering))
      {
         gs.Dithering = dithering ? 1.f : 0.f;
         reshade::set_config_value(nullptr, NAME, "Dithering", gs.Dithering);
         device_data.cb_luma_global_settings_dirty = true;
      }
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Reduces gradient banding.");

      ImGui::SeparatorText("UI");
      ImGui::Checkbox("Hide Gameplay UI", &g_hide_ui); // Session-only: a stuck "on" would look like a broken HUD.
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Disables the in-game UI.");
   }

#if DEVELOPMENT
   void DrawImGuiDevSettings(DeviceData& device_data) override
   {
      ImGui::SeparatorText("Motion Vectors");
      ImGui::Checkbox("MV Enable (without an upscaler)", &g_mv_enable);
      ImGui::Checkbox("MV Force Jitter", &g_mv_force_jitter);
      ImGui::Checkbox("MV Debug View", &g_mv_debug_view);
      ImGui::Checkbox("FSR Reactive Mask", &g_sr_reactive_enable);
      ImGui::Checkbox("FSR Reactive Debug View", &g_sr_reactive_debug_view);
      ImGui::Checkbox("DoF History (DLSS/FSR)", &g_dof_history_enable);
      const auto& stats = GetGameDeviceData(device_data).mv_last_stats;
      ImGui::Text("MV draws %u (matched %u, camera only %u, other camera %u, no camera %u, uncopied %u), jitter draws %u", stats.motion_vector_draws, stats.matched, stats.camera_only, stats.other_camera, stats.no_camera, stats.uncopied, stats.jitter_draws);
      ImGui::Text("Ended by 0x%08X, upscaler draws %u, near %.3f far %.0f, motion blur replays %u (masked %u)", stats.ended_by, stats.sr_draws, stats.near_plane, stats.far_plane, stats.motion_blur_replays, stats.motion_blur_masked);
   }
#endif

   void PrintImGuiAbout() override
   {
      ImGui::PushTextWrapPos(0.f);
      ImGui::Text(
         "Luma for \"Medal of Honor: Airborne\" is developed by DristoforColumb and is open source and free.\n"
         "It adds HDR, DLAA or FSR 3 native anti-aliasing, HDR bloom and SMAA anti-aliasing, plus 16x anisotropic filtering.\n"
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
      const char* cleared_project_name = (project_name[0] == '_') ? (project_name + 1) : project_name;

      uint32_t mod_version = 1;
      Globals::SetGlobals(cleared_project_name, "Medal of Honor Airborne Luma HDR mod", "", mod_version);
      // Finished: Publishing drops the "proceed at your own risk" warning WorkInProgress raises.
      Globals::DEVELOPMENT_STATE = Globals::ModDevelopmentState::Finished;

      // scRGB fp16 swapchain (the game's backbuffer is 8-bit).
      swapchain_format_upgrade_type = TextureFormatUpgradesType::AllowedEnabled;
      swapchain_upgrade_type = SwapchainUpgradeType::scRGB;

      // Exclusive fullscreen -> borderless. "force_borderless" is what matters on top of the core default: it
      // covers LEAVING fullscreen too, so the window cannot come back with a title bar after alt-tab (TW2).
      prevent_fullscreen_state = true;
      force_borderless = true;

      // Two families clip the HDR signal, both upgraded INDIRECTLY (a mirror substituted at bind): changing a
      // dgVoodoo resource's creation format breaks the translator's bookkeeping - black screen even in menus (MEA
      // class). The canvas is r8g8b8a8_typeless; the DoF/bloom chain is r16g16b16a16_typeless the game VIEWS as
      // unorm, so writes clamped at 1.0. Keyed by FORMAT, not by shader hash: blits (0xE64861E9) interleave with the
      // blurs and the last writer before the grade is a blit. The size filters are load-bearing too: 2 GB here, and
      // TW2 hit bad_alloc at 4K on a broad list.
      texture_format_upgrades_type = TextureFormatUpgradesType::AllowedEnabled;
      enable_indirect_texture_format_upgrades = true; // creation-time mirrors, substituted at bind (BL2/TW2 scheme)
      enable_chain_indirect_texture_format_upgrades = ChainTextureFormatUpgradesType::DirectDependencies;
      texture_upgrade_formats = {
         reshade::api::format::r8g8b8a8_typeless,     // dgVoodoo's D3D9 backbuffer surface (the LDR canvas)
         reshade::api::format::r16g16b16a16_typeless, // DoF/bloom gather, blur and blit targets, viewed as unorm
      };
      // "No1Px" is mandatory under dgVoodoo (BL2): the wrapper binds 1x1 placeholders in every unused sampler slot
      // and a 1x1 trivially passes the aspect filter, so core would mirror those too (and assert in DEVELOPMENT).
      texture_format_upgrades_2d_size_filters = (uint32_t)TextureFormatUpgrades2DSizeFilters::SwapchainResolution | (uint32_t)TextureFormatUpgrades2DSizeFilters::SwapchainAspectRatio | (uint32_t)TextureFormatUpgrades2DSizeFilters::No1Px;

      // AF16x, an addition rather than an upgrade: the 2007 video menu has no anisotropic option at all. The
      // load-bearing line is "force_upgrade_linear_samplers" - core otherwise rewrites only ANISOTROPIC samplers and
      // this wrapper binds none (TW2 census), which made mode 4 alone a no-op there. Mip LOD bias 0 without an
      // upscaler (it would buy shimmer), see OnPresent.
      enable_samplers_upgrade = true; // boot-time only (cannot be changed after device creation)
      samplers_upgrade_mode = 4;
      force_upgrade_linear_samplers = true;

      game = new MedalOfHonorAirborne();
   }
   else if (ul_reason_for_call == DLL_PROCESS_DETACH)
   {
      MedalOfHonorAirborne::UnregisterEvents();
   }

   CoreMain(hModule, ul_reason_for_call, lpReserved);

   return TRUE;
}
