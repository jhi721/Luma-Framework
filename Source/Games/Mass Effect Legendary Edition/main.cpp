// Mass Effect Legendary Edition trilogy Luma mod (Unreal Engine 3, native DX11, x64). One addon, three games:
// stage-1 hashes, bloom slots and the GTAO radius are per game, the FXAA/HBAO+/DoF/bloom/present chains are shared.
//
// Stage 1 replaces the SDR-clamping uber-post tonemap with an HDR reconstruction and DICE rolloff; stage 2
// (0x0765601C) decodes the gamma intermediate and applies Game Paper White. SMAA replaces compute FXAA, fp16
// pyramidal bloom replaces the UNORM bloom, XeGTAO writes the native AO target; native fp16 DoF is unchanged.
//
// Sub-native borderless is best-effort: the game allocates desktop-sized targets but renders a top-left
// sub-rectangle through cb2 DynamicScale, so injected in-place passes process the full allocation.
//
// DLSS / FSR 3 (DLAA): the engine's velocity target is unusable (RGBA8, one velocity per object, the player only), so motion
// vectors and jitter come from patched shaders (MotionVectorPatches.h, Borderlands GOTY Enhanced's method): every scene draw's
// vertex shader runs a second time on its previous frame constants. The upscaler runs at the scene's first post pass; SMAA
// then steps aside (RCAS stays).

#define DISABLE_AUTO_DEBUGGER 1 // The DEVELOPMENT attach prompt is hidden by fullscreen and blocks the loader.

#define ENABLE_SMAA 1  // replaces the game's compute FXAA
#define ENABLE_BLOOM 1 // fp16 pyramidal bloom replaces the game's clamped bloom
// The motion vector and jitter draws wrap the game's own draws, so they need "original_draw_dispatch_func"
#define ENABLE_POST_DRAW_DISPATCH_CALLBACK 1
// The motion vector draw key reads the draw's arguments ("last_draw_dispatch_data")
#define ENABLE_DRAW_DISPATCH_DATA_CACHE 1

#include "..\..\Core\core.hpp"
#include "..\..\External\WDK\includes\d3d11TokenizedProgramFormat.hpp"
#include "MotionVectorPatches.h"
#include "..\..\Core\includes\patched_draws.h"
#include <shellapi.h> // ShellExecuteA for About links (system() hangs the render thread in exclusive fullscreen)
#include <vector>
#include <chrono>
#include <unordered_set>
#include <cmath>
#include <array>
#include <memory>
#include <optional>
#include <span>

// Selects the per-game profile (tonemap slot table, GTAO radius); replacement stays CSO-hash keyed.
enum class MEGame
{
   ME1LE,
   ME2LE,
   ME3LE,
};
static MEGame g_me_game = MEGame::ME1LE;

static MEGame DetectMEGame()
{
   std::string exe = System::GetProcessExecutableName();
   for (auto& c : exe)
      c = (char)tolower((unsigned char)c);
   if (exe.find("masseffect3") != std::string::npos)
      return MEGame::ME3LE;
   if (exe.find("masseffect2") != std::string::npos)
      return MEGame::ME2LE;
   return MEGame::ME1LE; // Also an unknown executable
}

static const char* GameName(MEGame game)
{
   return game == MEGame::ME3LE ? "ME3LE" : (game == MEGame::ME2LE ? "ME2LE" : "ME1LE");
}

// SMAA replaces the shared MiniEngine FXAA resolve on the fp16 gamma post buffer. The prepass and indirect-
// argument dispatches touch only work queues and remain native.
static constexpr uint32_t kFXAAResolveHHash = 0xB53BB634; // Horizontal resolve: replaced with SMAA.
static constexpr uint32_t kFXAAResolveVHash = 0xF43DBFFD; // Vertical in-place refine: skipped after SMAA.

// Stage-1 tonemap permutations (MB = motion blur). Slots are stored per permutation because MB binds depth at t0
// and pushes everything up one, and ME3LE additionally binds velocity at t2. They mirror the scene and bloom
// registers of the matching HLSL body with no compile-time cross-check, so a re-captured permutation must move both
// sides. Unlisted permutations stay vanilla; this table drives bloom, SMAA depth capture and the DEVELOPMENT readout.
struct TonemapPermDesc
{
   uint32_t hash;
   uint8_t scene_slot; // 1 on motion-blur permutations, where t0 is depth instead of scene color.
   uint8_t bloom_slot;
};
static constexpr TonemapPermDesc kTonemapPermsME1LE[] = {
   {0x151FE4CA, 1, 5}, // MB + grain, LUT grade
   {0x69F03340, 1, 5}, // MB, LUT grade
   {0x109F3B6E, 0, 4}, // grain, LUT grade
   {0x8C8E8CA2, 0, 4}, // LUT grade
   {0xAAE8755A, 0, 4}, // analytic grade (no LUT)
};
static constexpr TonemapPermDesc kTonemapPermsME2LE[] = {
   {0x2754F750, 1, 5}, // MB, LUT grade
   {0x1536C5B5, 1, 5}, // MB + grain, LUT grade
   {0x940979D8, 1, 5}, // MB, filmic + LUT grade
   {0x75BFAFBC, 1, 5}, // MB + grain, filmic + LUT grade
   {0xCC76075F, 0, 4}, // analytic grade (no LUT)
   {0xD077D06B, 0, 4}, // LUT grade
   {0x8E0C0DBB, 0, 4}, // grain, LUT grade
   {0x222186F8, 0, 4}, // filmic + LUT grade
   {0xEC890842, 0, 4}, // grain, filmic + LUT grade
};
static constexpr TonemapPermDesc kTonemapPermsME3LE[] = {
   {0x36B90B12, 1, 6}, // MB, filmic + LUT grade
   {0x49BD5A95, 1, 6}, // MB + grain, filmic + LUT grade
   {0x00944C2E, 0, 4}, // filmic + LUT grade
   {0x5AA0BD09, 0, 4}, // grain, filmic + LUT grade
   {0x225A8330, 0, 4}, // analytic grade (no LUT)
};
// Selected once in DllMain.
static std::span<const TonemapPermDesc> g_tonemap_perms = kTonemapPermsME1LE;

// Everything that differs between the three games. ME2LE/ME3LE share the native HBAO+ radius of 48 uu against ME1LE's
// 30 uu; GTAO visibility power is 1 everywhere, so it stays at its global default instead of living here.
struct MEGameProfile
{
   std::span<const TonemapPermDesc> tonemap_perms;
   float gtao_radius_override; // Native radius (uu) / DepthScale; 0 keeps the shader's own radius.
};
static constexpr MEGameProfile ProfileFor(MEGame game)
{
   switch (game)
   {
   case MEGame::ME2LE:
      return {kTonemapPermsME2LE, 0.96f};
   case MEGame::ME3LE:
      return {kTonemapPermsME3LE, 0.96f};
   default:
      return {kTonemapPermsME1LE, 0.f};
   }
}
// Quarter-resolution bloom bright-pass; cb0.xy = (BloomScale, Threshold).
static constexpr uint32_t kBloomBrightPassHash = 0xF8942FF1;

// User-facing AA settings persisted through ReShade.
static bool g_smaa_enable = true;
static float g_rcas_sharpness = 0.f; // RCAS on the SMAA or DLSS / FSR output (0 = off)

// SMAA predication, uploaded as SmaaPredication.xyz. Not exposed: tuned to this engine's non-reverse hyperbolic
// R24 depth, whose small far-silhouette deltas need 0.001 rather than SMAA's generic 0.01.
static constexpr float kPredScale = 2.0f;       // [1,5] off-edge threshold multiplier.
static constexpr float kPredThreshold = 0.001f; // Depth delta that identifies an edge.
static constexpr float kPredStrength = 0.4f;    // [0,1] edge influence on the color threshold.

// fp16 pyramidal bloom built from the stage-1 linear scene and rebound at the native bloom slot, keeping the
// game's tint and screen blend.
static bool g_bloom_enable = true;
// One sigma per mip, so the two counts cannot drift apart.
static constexpr std::array<float, 6> kBloomSigmas = {1.5f, 2.f, 2.f, 2.f, 2.f, 1.f};
// Multiplies the artist-authored per-scene cb0.x, read two frames late through a no-stall ring.
static float g_bloom_intensity = 1.0f;

// Bink targets the intermediate gamma buffer or the swapchain directly; GameSettings.VideoOnSwapchain reports
// which, so the UI/Game ratio and Game Paper White are applied exactly once on either path.
static constexpr uint32_t kVideoBinkHash = 0x7B5C59DF;
// Shared stage 2 decodes the gamma scene/HUD composite into linear scRGB and applies the UI/Game ratio and Game
// Paper White. Native SDR omits it.
static constexpr uint32_t kOutputStage2Hash = 0x0765601C;

// XeGTAO replaces the half-resolution GFSDK HBAO+ chain, writing the game's R8_UNORM AO target at the blur
// dispatch; the native apply blit still composites. Noise and denoise pass count follow "IsGTAOTemporal".
static constexpr uint32_t kAODeinterleaveHash = 0x497830D8; // Depth deinterleave: skipped after capture.
static constexpr uint32_t kAOHorizonHash = 0x80212FD6;      // Horizon march: skipped after normal capture.
static constexpr uint32_t kAOBlurHash = 0x06D92B08;         // Blur: replaced with XeGTAO.

static bool g_gtao_enable = true;
static bool g_video_auto_hdr_enable = true; // Expand Bink highlights in HDR; off preserves vanilla SDR video.
static bool g_hide_ui = false;              // Session-only HUD suppression for clean captures.
// Runtime XeGTAO parameters in b11. HBAO+ PowExponent does not transfer numerically (different visibility
// curve), so FinalValuePower is calibrated against the native AO histogram.
static float g_gtao_final_value_power = 1.0f;
// Converts UE3 view-Z units (approximately 2 cm per uu) to the scale expected by XeGTAO.
static float g_gtao_depth_scale = 50.f;
// A positive value overrides EFFECT_RADIUS; native radius is sqrt(-1 / NegInvR2) / DepthScale.
static float g_gtao_radius_override = 0.f;
#if DEVELOPMENT || TEST
static int g_gtao_debug_view = 0; // 0=off, 1=depth, 2=normals, 3=AO x8, 4=edges.
#endif
#if DEVELOPMENT
static int g_gtao_temporal = 0; // 0 = with DLSS/FSR (see "IsGTAOTemporal"), 1 = off, 2 = on
#else
static constexpr int g_gtao_temporal = 0;
#endif

// Native DoF is retained: its fp16 near/far chain has no SDR clamp, and stage 1 composites those buffers.

// DLSS / FSR: the post passes that end a motion vector frame (the upscaler runs right before the first one): the bloom bright
// pass and the DoF downsample read the scene, else stage 1 does (see "g_tonemap_perms").
static constexpr uint32_t kDOFDownsampleHash = 0x94FA3B53;
// b1 VSOffsetConstants: ViewProjectionMatrix (c0-c3), CameraPosition (c4), PreViewTranslation (c5), the same in every vertex
// shader. UE3 draws in world space translated by PreViewTranslation (minus the camera position): LocalToWorld and
// ViewProjectionMatrix both include it.
static constexpr size_t kPreViewTranslationOffset = 5 * 16;

// Motion vectors for DLSS / FSR: the opaque draws into the fp16 scene draw with patched shaders that also write an extra target,
// the vertex shader's second run reading the draw's previous frame b0 / b1 / b3.
#if DEVELOPMENT
static bool g_mv_enable = false;
static bool g_mv_debug_view = false;
static bool g_mv_force_jitter = false;   // The projection jitter without an upscaler
static bool g_mv_disable_jitter = false; // No projection jitter under the upscaler (A/B of jitter-dependent artifacts)
static bool g_mv_skip_fill = false;      // No camera fill (the debug view then shows the patched draws alone)
static bool g_mv_dump = false;           // One shot: the motion vectors, depth and cameras to "Luma_MV_Dump" (see "DumpMotionVectors")
#else
static constexpr bool g_mv_enable = false;
static constexpr bool g_mv_force_jitter = false;
static constexpr bool g_mv_disable_jitter = false;
#endif

#if DEVELOPMENT
// "Performance Test" (see "OnPresent"): the mode, and the anti-aliasing it sets while it runs (the user's is restored on "Off" or
// "Current Settings", and never saved)
static int g_perf_test = 0;
static bool g_perf_hook_timers = true; // The hooks' CPU time (two clock reads per hooked draw, themselves a cost to measure)
struct PerfTestMode
{
   const char* name;
   bool set_aa = false; // Else the current settings (the fields below too)
   SR::Type sr_type = SR::Type::None;
   unsigned int dlss_preset = 0; // NVSDK_NGX_DLSS_Hint_Render_Preset (5 = E, 11 = K, ...)
   bool smaa = false;
   int motion_vector_draws = 2; // 2 patched (motion vectors and jitter), 1 jitter only, 0 untouched
};
static constexpr PerfTestMode perf_test_modes[] = {
   {"Off"},
   {"Current Settings"},
   {"DLSS K", true, SR::Type::DLSS, 11},
   {"DLSS K Jitter Only", true, SR::Type::DLSS, 11, false, 1},
   {"DLSS K Without Motion Vector Draws", true, SR::Type::DLSS, 11, false, 0},
   {"DLSS L", true, SR::Type::DLSS, 12},
   {"DLSS M", true, SR::Type::DLSS, 13},
   {"DLSS E (CNN)", true, SR::Type::DLSS, 5},
   {"FSR 3", true, SR::Type::FSR},
   {"SMAA", true, SR::Type::None, 0, true},
   {"SMAA Off", true, SR::Type::None, 0, false}, // The game's FXAA, if it's on in the game's settings
};
// "Sweep": these modes in turn, a few log windows each, over several rounds (interleaved, so the scene's drift averages out), then a
// median per mode
static bool g_perf_sweep = false;
static constexpr int perf_sweep_modes[] = {2, 3, 4, 10};
static_assert(std::string_view(perf_test_modes[perf_sweep_modes[0]].name) == "DLSS K" && std::string_view(perf_test_modes[perf_sweep_modes[std::size(perf_sweep_modes) - 1]].name) == "SMAA Off");
static_assert(std::size(perf_test_modes) <= 32); // 5 bits in "perf_settings"
static constexpr int perf_sweep_rounds = 3;
static constexpr int perf_sweep_windows = 2; // Per mode and round
#endif

// fp16 scratch target and the views it was created with; EnsureRGBA16FTarget rebuilds it on resolution change.
struct RGBA16FTarget
{
   ComPtr<ID3D11Texture2D> tex;
   ComPtr<ID3D11RenderTargetView> rtv;
   ComPtr<ID3D11ShaderResourceView> srv;
   ComPtr<ID3D11UnorderedAccessView> uav;
   uint32_t w = 0, h = 0;
};

// Per-device resources and per-frame state. SMAA detects edges on a gamma snapshot and blends a linear decode of it.
struct MassEffectGameDeviceData final : public GameDeviceData
{
   // Handles already processed by SMAA this frame; later in-place FXAA resolves must be skipped.
   std::unordered_set<uint64_t> smaa_applied_handles;

   // R24 scene depth captured from motion-blur tonemap permutations for SMAA predication.
   ComPtr<ID3D11ShaderResourceView> srv_depth;

   // SMAA b1: target metrics followed by predication scale, threshold, and strength.
   com_ptr<ID3D11Buffer> cb_smaa_metrics;

   // Size DrawSMAA built its core-managed intermediates at (rebuild on resolution change).
   uint32_t smaa_core_w = 0, smaa_core_h = 0;

   // SRV-readable snapshot of the in-place gamma post buffer.
   RGBA16FTarget smaa_input;
   // Its linear-light decode, for the neighborhood blend.
   RGBA16FTarget smaa_input_linear;

   // fp16 SMAA output, copied back directly or through RCAS.
   RGBA16FTarget smaa_out;

   // RCAS b0 = (width, height, sharpness, 0).
   com_ptr<ID3D11Buffer> cb_sharpen;
   RGBA16FTarget rcas_out;

   // XeGTAO inputs at half-res AO size: R24 depth from deinterleave t0, packed R8G8 view normals from horizon t0.
   ComPtr<ID3D11ShaderResourceView> srv_gtao_depth;
   ComPtr<ID3D11ShaderResourceView> srv_gtao_normals;
   // Set only after a complete takeover at deinterleave; otherwise the native chain remains intact.
   bool gtao_active_this_frame = false;

   // Five-level R32F view-space-depth pyramid.
   ComPtr<ID3D11Texture2D> tex_gtao_depth_mips;
   ComPtr<ID3D11UnorderedAccessView> gtao_depth_mip_uavs[5];
   ComPtr<ID3D11ShaderResourceView> srv_gtao_depth_mips;
   // R8G8_UNORM AO/edge outputs of the main pass and first denoiser; the second denoiser writes the game's u0.
   ComPtr<ID3D11Texture2D> tex_gtao_working[2];
   ComPtr<ID3D11UnorderedAccessView> uav_gtao_working[2];
   ComPtr<ID3D11ShaderResourceView> srv_gtao_working[2];
   uint32_t gtao_w = 0, gtao_h = 0;

   // b11 = (FinalValuePower, DepthScale, RadiusOverride, DebugView, NoiseIndex), written every frame.
   com_ptr<ID3D11Buffer> cb_gtao;

   // Bright-pass cb0 staging ring for no-stall per-scene BloomScale and threshold capture.
   ComPtr<ID3D11Buffer> bloom_scale_ring[3];
   int bloom_scale_ring_wr = 0;
   int bloom_scale_ring_filled = 0;              // Do not map until every slot contains real data.
   bool bloom_scale_captured_this_frame = false; // Once-per-frame capture gate.
   bool scene_post_done_this_frame = false;      // Arms HUD suppression after the FXAA resolve.
   bool stage2_seen_this_frame = false;          // False means final composition must decode the gamma swapchain.
   float bloom_scale_live = -1.f;                // Negative until the first successful readback.
   float bloom_threshold_live = -1.f;            // Negative selects the 1.2 fallback.
#if DEVELOPMENT
   int bloom_bright_pass_hits = 0; // Bright-pass captures this frame.
   // Which stage-1 permutation the game last drew, and how many stage-1 draws the frame contained. A count of two
   // can mean a mid-frame permutation switch, and so two HDR families in one frame.
   const TonemapPermDesc* stage1_perm = nullptr;
   int stage1_draws = 0;
#endif

   // "IsSRActive", taken at present: Core's "Super Resolution" selection changes after it, mid frame for the draws
   bool sr_active = false;
   float sr_vert_fov = 1.047f; // The last valid vertical field of view (radians) for FSR, 60 degrees until one is seen
   // "DrawUpscaler" made a new upscaler output texture (see there)
   bool sr_output_recreated = false;
   // The upscaler's output, read in place of its input (the scene) by the post passes up to stage 1 (see "RebindUpscaledScene")
   com_ptr<ID3D11ShaderResourceView> sr_output_srv;
   ID3D11Resource* sr_input = nullptr; // The scene's last copy, else the scene
   bool sr_rebind_done = false;        // Stage 1, the scene's last reader, has drawn

   // Motion vectors: shaders patched on first use, by original hash (null on failure), and the target (sized like the scene; every
   // blend state writes it unblended, see "OnCreateBlendState")
   std::atomic<bool> mv_active = false; // Motion vectors and jitter this frame: an upscaler is active, or the DEV toggle (set at present)
   std::shared_mutex mv_mutex;
   std::unordered_map<uint32_t, com_ptr<ID3D11VertexShader>> mv_vertex_shaders;
   std::unordered_map<uint32_t, com_ptr<ID3D11PixelShader>> mv_pixel_shaders;
   // The patched vertex shaders that read b3 (skinned), and the byte offset of each one's LocalToWorld translation row in b0 (none
   // for world space geometry, matched by draw key alone), by original hash
   std::unordered_set<uint32_t> mv_bone_vertex_shaders;
   std::unordered_map<uint32_t, uint32_t> mv_translation_offsets;
   com_ptr<ID3D11Texture2D> mv_texture;
   com_ptr<ID3D11RenderTargetView> mv_rtv;
   com_ptr<ID3D11UnorderedAccessView> mv_uav; // Null without typed UAV loads of its format (then no fill)
   // A frame opens at its first mesh draw into output sized depth (the depth prepass: the jitter is chosen there), starts at its
   // first motion vector draw (the target is cleared) and ends at the first post pass, once per present.
   bool mv_scene_open = false;
   bool mv_scene_done = false;
   bool mv_frame_ended = true;
   // The frame's scene depth (the depth view's resource), its last copy, and a view of the one the camera fill and the upscaler
   // read (the depth itself if it can be read, else the copy)
   com_ptr<ID3D11Resource> mv_depth;
   uint64_t mv_depth_copy = 0;
   com_ptr<ID3D11ShaderResourceView> mv_depth_srv;
   bool mv_fill_pending = false;
   com_ptr<ID3D11Buffer> mv_fill_buffer;
   // The fp16 scene the motion vector draws write, and its last copy (UE3's resolve into "SceneColorTexture": the post passes read it)
   com_ptr<ID3D11Resource> mv_scene_color;
   uint64_t mv_scene_color_copy = 0;
   // The projection jitter (pixels, +y down), chosen when the scene opens; its NDC offset is at VS "MotionVectorPatches::jitter_slot"
   // of every mesh draw depth tested against the scene
   std::array<float, 2> mv_jitter = {};
   std::array<float, 2> mv_jitter_ndc = {}; // The same offset in NDC (y up), as the jitter buffer holds it
   com_ptr<ID3D11Buffer> mv_jitter_buffer;
   // Per-draw lookups kept for the next draw (reset when the scene opens, views and states can be recreated between scenes): the
   // jitter path's last depth view and whether it's the scene depth, its last depth stencil state's depth test, the motion vector
   // path's last accepted and last refused targets (with the refusal's "MV_REJECT" reason) and the last blend state's opacity (null =
   // the default state, opaque), the last vertex and pixel shader's patched versions (owned by "mv_vertex_shaders" /
   // "mv_pixel_shaders", never erased) and the vertex shader's facts.
   ID3D11DepthStencilView* jitter_dsv = nullptr;
   bool jitter_dsv_scene = false;
   ID3D11DepthStencilState* jitter_depth_stencil_state = nullptr;
   bool jitter_depth_test = true;
   ID3D11RenderTargetView* mv_accepted_rtv = nullptr;
   ID3D11DepthStencilView* mv_accepted_dsv = nullptr;
   ID3D11RenderTargetView* mv_refused_rtv = nullptr;
   ID3D11DepthStencilView* mv_refused_dsv = nullptr;
   int mv_refused_reason = 0;
   ID3D11BlendState* mv_blend_state = nullptr;
   bool mv_blend_opaque = true;
   uint32_t mv_last_vertex_shader_hash = 0;
   ID3D11VertexShader* mv_last_vertex_shader = nullptr;
   bool mv_last_vertex_shader_skinned = false;
   uint32_t mv_last_vertex_shader_translation = UINT_MAX;
   uint32_t mv_last_pixel_shader_hash = 0;
   ID3D11PixelShader* mv_last_pixel_shader = nullptr;
   PatchedDraws::BoundShader<ID3D11VertexShader> mv_bound_vertex_shader;
   PatchedDraws::BoundShader<ID3D11PixelShader> mv_bound_pixel_shader;

   // CPU copies of the b0 / b1 / b3 buffers the motion vector draws bind, by buffer (an entry registers it, null until its first
   // update): the engine uploads every one with UpdateSubresource (whole buffer, only when a constant changed), so a draw's constants
   // are its buffer's latest copy
   using ConstantsCopy = std::shared_ptr<const std::vector<uint8_t>>;
   std::mutex mv_constants_mutex;
   std::unordered_map<uint64_t, ConstantsCopy> mv_constants_copies;
   // Previous frame constants of the motion vector draws (see "PatchedDraws::PreviousConstants")
   PatchedDraws::PreviousConstants mv_previous_constants;
   // Motion vector draws by draw key (shaders, buffers, arguments), with the LocalToWorld translation (in world space) and b0 / b1 /
   // b3. A draw takes the previous frame's constants of its key's nearest draw (same object, a frame earlier), its camera included.
   struct MotionVectorObject
   {
      std::array<float, 3> translation;
      ConstantsCopy object; // b0
      ConstantsCopy camera; // b1
      ConstantsCopy bones;  // b3, skinned draws only
   };
   std::unordered_map<uint64_t, std::vector<MotionVectorObject>> mv_objects;
   std::unordered_map<uint64_t, std::vector<MotionVectorObject>> mv_previous_objects;
   // The frame's world camera (b1 of its first motion vector draw) and the previous frame's
   ConstantsCopy mv_camera;
   ConstantsCopy mv_previous_camera;
   // The last draw's b1 copy and whether it's a mirrored view (see "IsMirroredView"); the b1 of the view that opened the scene, the last
   // depth view checked for output size, and whether the scene reopened for a later view since the target was last cleared (see
   // "IsNewView")
   ConstantsCopy view_camera;
   bool view_mirrored = false;
   ConstantsCopy scene_view_camera;
   ID3D11DepthStencilView* view_dsv = nullptr;
   bool view_dsv_output = false;
   bool mv_view_restarted = false;
   uint32_t mv_frame_index = 0; // The Luma frame index of the last motion vector frame

#if DEVELOPMENT
   // Per frame counts for the DEV panel (the last complete frame's shown)
   struct MotionVectorStats
   {
      uint32_t motion_vector_draws = 0, jitter_draws = 0, matched = 0, camera_only = 0, other_camera = 0, uncopied = 0, updates = 0, sr_draws = 0;
      uint32_t matched_same_camera = 0; // Matched draws whose previous b1 is byte equal to the current one
      uint32_t mirrored = 0;            // Draws of mirrored views (planar reflections), left untouched
      uint32_t view_restarts = 0;       // Scene reopened for a later view (see "IsNewView")
      uint32_t ended_by = 0;
      uint32_t rejected[8] = {};                                                                     // "DrawWithMotionVectors" refusals by reason ("MV_REJECT")
      uint32_t rejected_format = 0, rejected_dimension = 0, rejected_width = 0, rejected_height = 0; // The last target refused by format or size
   };
   MotionVectorStats mv_stats, mv_last_stats;
   // The views that opened the scene this frame (and the last frame's): near plane, determinant and PreViewTranslation z of their b1,
   // viewport, scene draws and motion vector draws
   struct ViewInfo
   {
      float near_plane, determinant, view_translation_z, viewport_width, viewport_height;
      uint32_t draws = 0, motion_vector_draws = 0;
   };
   std::vector<ViewInfo> views, last_views;
   // Scene probe (see "OnDrawOrDispatch"): the lines already logged
   std::unordered_set<std::string> probe_logged;
   // "MV Dump": staging copies taken at the scene's end, written at present
   com_ptr<ID3D11Texture2D> dump_textures[2]; // Motion vectors, depth
   std::vector<uint8_t> dump_cameras[2];      // b1, current and previous
   std::array<float, 2> dump_jitter = {};

   // "Performance Test": GPU timestamps per frame (present to present, the scene from the depth prepass to its first post pass, the
   // upscaler), all on the immediate context, in a ring read back a few frames later without waiting; and the CPU time in the
   // motion vector hooks. Inside the scene: the camera fill (from its start to the scene's end).
   struct PerfQueries
   {
      com_ptr<ID3D11Query> disjoint, frame_start, scene_start, fill_start, scene_end, sr_end, frame_end;
      bool scene_started = false; // scene_start issued
      bool fill = false;          // fill_start issued
      bool scene = false;         // ... and scene_end
      bool sr = false;            // sr_end issued
      bool pending = false;
   };
   struct PerfStats
   {
      double frame_ms = 0.0, frame_max_ms = 0.0, scene_ms = 0.0, scene_max_ms = 0.0, sr_ms = 0.0, sr_max_ms = 0.0, cpu_frame_ms = 0.0;
      double fill_ms = 0.0, unused_max_ms = 0.0;
      uint32_t samples = 0, scene_samples = 0, sr_samples = 0, fill_samples = 0, disjoint = 0, frames = 0;
   };
   std::array<PerfQueries, 8> perf_queries;
   size_t perf_query_index = 0;
   PerfQueries* perf_frame_queries = nullptr; // This frame's, from present to present
   PerfStats perf_stats;                      // This log window's
   int perf_settle_frames = 0;                // Frames skipped after a change (targets rebuilt, history reset)
   uint32_t perf_settings = 0;                // The measured settings, to restart the settle on a change
   std::chrono::steady_clock::time_point perf_last_present;
   std::atomic<int64_t> perf_hook_ns = 0; // This log window's
   // The user's anti-aliasing, while a mode that sets its own runs
   SR::Type perf_user_sr_type = SR::Type::None;
   unsigned int perf_user_dlss_preset = 0;
   bool perf_user_smaa = false;
   bool perf_user_mv_enable = false;
   // "Sweep": the step over all rounds, the log windows done in it, and per mode each window's frame, scene, SR, hook and fill times
   int perf_sweep_step = 0;
   int perf_sweep_windows_done = 0;
   std::vector<std::array<double, 5>> perf_sweep_results[std::size(perf_test_modes)];
#endif
};

class MassEffectLE final : public Game
{
   static MassEffectGameDeviceData& GetGameDeviceData(DeviceData& device_data)
   {
      return *static_cast<MassEffectGameDeviceData*>(device_data.game);
   }

#if DEVELOPMENT
   // "Performance Test": adds the scope's CPU time to the motion vector hooks' total, while the test runs
   struct PerfHookTimer
   {
      std::atomic<int64_t>& total_ns;
      const bool enabled = g_perf_test != 0 && g_perf_hook_timers;
      const std::chrono::steady_clock::time_point start = enabled ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
      ~PerfHookTimer()
      {
         if (enabled)
            total_ns += std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count();
      }
   };

   // "Performance Test": switches to a mode, setting its anti-aliasing as Core's "Super Resolution" and "DLSS Preset" selection do
   // (without saving), keeping the user's while any mode that sets its own runs and restoring it after
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
         game_device_data.perf_user_mv_enable = g_mv_enable;
      }
      if (mode.set_aa || previous_mode.set_aa)
      {
         device_data.sr_type = mode.set_aa ? mode.sr_type : game_device_data.perf_user_sr_type;
         device_data.sr_suppressed = false;
         dlss_render_preset = mode.set_aa && mode.sr_type == SR::Type::DLSS ? mode.dlss_preset : game_device_data.perf_user_dlss_preset;
         g_smaa_enable = mode.set_aa ? mode.smaa : game_device_data.perf_user_smaa;
         // "MV Enable" would draw motion vectors in the modes without an upscaler (the SMAA baselines)
         g_mv_enable = mode.set_aa ? false : game_device_data.perf_user_mv_enable;
      }
      g_perf_test = mode_index;
   }
#endif

   // Registered shader names, hashed once so registration, readiness probes and dispatch cannot drift apart.
   static constexpr uint32_t kNameBloomVS = CompileTimeStringHash("Bloom VS");
   static constexpr uint32_t kNameBloomPrefilterPS = CompileTimeStringHash("Bloom Prefilter PS");
   static constexpr uint32_t kNameBloomDownsamplePS = CompileTimeStringHash("Bloom Downsample PS");
   static constexpr uint32_t kNameBloomUpsamplePS = CompileTimeStringHash("Bloom Upsample PS");
   static constexpr uint32_t kNameGTAOPrefilterCS = CompileTimeStringHash("MELE XeGTAO Prefilter Depths CS");
   static constexpr uint32_t kNameGTAOMainPassCS = CompileTimeStringHash("MELE XeGTAO Main Pass CS");
   static constexpr uint32_t kNameGTAODenoise1CS = CompileTimeStringHash("MELE XeGTAO Denoise Pass 1 CS");
   static constexpr uint32_t kNameGTAODenoise2CS = CompileTimeStringHash("MELE XeGTAO Denoise Pass 2 CS");
   static constexpr uint32_t kNameSMAAEdgeVS = CompileTimeStringHash("SMAA Edge Detection VS");
   static constexpr uint32_t kNameSMAAEdgePS = CompileTimeStringHash("SMAA Edge Detection PS");
   static constexpr uint32_t kNameSMAAWeightVS = CompileTimeStringHash("SMAA Blending Weight Calculation VS");
   static constexpr uint32_t kNameSMAAWeightPS = CompileTimeStringHash("SMAA Blending Weight Calculation PS");
   static constexpr uint32_t kNameSMAABlendVS = CompileTimeStringHash("SMAA Neighborhood Blending VS");
   static constexpr uint32_t kNameSMAABlendPS = CompileTimeStringHash("SMAA Neighborhood Blending PS");
   static constexpr uint32_t kNameCopyVS = CompileTimeStringHash("Copy VS");
   static constexpr uint32_t kNameSharpenPS = CompileTimeStringHash("MELE Sharpen PS");
   static constexpr uint32_t kNameSMAALinearizeCS = CompileTimeStringHash("MELE SMAA Linearize CS");
   static constexpr uint32_t kNameMVFillCS = CompileTimeStringHash("MELE Motion Vector Fill CS");

   // operator[] default-inserts on a miss, which would mutate a map the render thread otherwise only reads.
   template <typename ShaderMap>
   static auto FindShader(const ShaderMap& shaders, uint32_t name)
   {
      const auto it = shaders.find(name);
      return it != shaders.end() ? it->second.get() : nullptr;
   }
   template <typename ShaderMap>
   static bool AllShadersReady(const ShaderMap& shaders, std::initializer_list<uint32_t> names)
   {
      for (const uint32_t name : names)
      {
         if (FindShader(shaders, name) == nullptr)
            return false;
      }
      return true;
   }

   static bool CreateDefaultRGBA16FTex(ID3D11Device* device, uint32_t w, uint32_t h, UINT bind_flags, ComPtr<ID3D11Texture2D>& out)
   {
      out.reset();
      D3D11_TEXTURE2D_DESC td = {};
      td.Width = w;
      td.Height = h;
      td.MipLevels = 1;
      td.ArraySize = 1;
      td.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
      td.SampleDesc.Count = 1;
      td.Usage = D3D11_USAGE_DEFAULT;
      td.BindFlags = bind_flags;
      return SUCCEEDED(device->CreateTexture2D(&td, nullptr, out.put()));
   }

   // (Re)create an fp16 scratch target on resolution change, with one view per requested bind flag. Returns false if
   // the texture or any requested view is missing.
   static bool EnsureRGBA16FTarget(ID3D11Device* device, uint32_t w, uint32_t h, UINT bind_flags, RGBA16FTarget* target)
   {
      if (!target->tex || target->w != w || target->h != h)
      {
         *target = {};
         if (CreateDefaultRGBA16FTex(device, w, h, bind_flags, target->tex))
         {
            if (bind_flags & D3D11_BIND_RENDER_TARGET)
               device->CreateRenderTargetView(target->tex.get(), nullptr, target->rtv.put());
            if (bind_flags & D3D11_BIND_SHADER_RESOURCE)
               device->CreateShaderResourceView(target->tex.get(), nullptr, target->srv.put());
            if (bind_flags & D3D11_BIND_UNORDERED_ACCESS)
               device->CreateUnorderedAccessView(target->tex.get(), nullptr, target->uav.put());
            target->w = w;
            target->h = h;
         }
      }
      return target->tex && (!(bind_flags & D3D11_BIND_RENDER_TARGET) || target->rtv) && (!(bind_flags & D3D11_BIND_SHADER_RESOURCE) || target->srv) && (!(bind_flags & D3D11_BIND_UNORDERED_ACCESS) || target->uav);
   }

   static void ReleaseGTAOScratch(MassEffectGameDeviceData& gd)
   {
      gd.tex_gtao_depth_mips.reset();
      for (auto& uav : gd.gtao_depth_mip_uavs)
         uav.reset();
      gd.srv_gtao_depth_mips.reset();
      for (int i = 0; i < 2; i++)
      {
         gd.tex_gtao_working[i].reset();
         gd.uav_gtao_working[i].reset();
         gd.srv_gtao_working[i].reset();
      }
      gd.gtao_w = gd.gtao_h = 0;
   }

   // Motion vectors: the copies of the scene and of its depth while it's open (the last ones before the first post pass are what
   // post reads). UE3 copies the scene into "SceneColorTexture" before distortion, translucency and post.
   bool OverrideCopyResource(ID3D11Device* native_device, DeviceData& device_data, uint64_t& dst_resource, uint64_t& src_resource) override
   {
      auto& game_device_data = GetGameDeviceData(device_data);
      if (game_device_data.mv_scene_open)
      {
         if (src_resource == uint64_t(game_device_data.mv_scene_color.get()))
            game_device_data.mv_scene_color_copy = dst_resource;
         else if (src_resource == uint64_t(game_device_data.mv_depth.get()))
            game_device_data.mv_depth_copy = dst_resource;
      }
#if DEVELOPMENT
      else if (game_device_data.mv_scene_done && (src_resource == uint64_t(game_device_data.mv_scene_color.get()) || src_resource == game_device_data.mv_scene_color_copy))
      {
         const std::string line = std::format("[MELE Probe] copy of the scene after its end -> 0x{:X}", dst_resource);
         if (game_device_data.probe_logged.insert(line).second)
            reshade::log::message(reshade::log::level::info, line.c_str());
      }
#endif
      return false;
   }
   bool OverrideCopyTextureRegion(ID3D11Device* native_device, DeviceData& device_data, uint64_t& dst_resource, uint32_t dst_subresource, const D3D11_BOX* dst_box, uint64_t& src_resource, uint32_t src_subresource, const D3D11_BOX* src_box) override
   {
      // Only a whole texture copy is one of the scene or its depth (the upscaler reads it, and the post passes read it in full)
      if (dst_subresource != 0 || src_subresource != 0 || (dst_box && (dst_box->left != 0 || dst_box->top != 0 || dst_box->front != 0)))
         return false;
      auto& game_device_data = GetGameDeviceData(device_data);
      if (src_box && (src_resource == uint64_t(game_device_data.mv_scene_color.get()) || src_resource == uint64_t(game_device_data.mv_depth.get())))
      {
         uint4 size;
         DXGI_FORMAT unused_format;
         GetResourceInfo(reinterpret_cast<ID3D11Resource*>(src_resource), size, unused_format);
         if (src_box->left != 0 || src_box->top != 0 || src_box->right < size.x || src_box->bottom < size.y)
            return false;
      }
      return OverrideCopyResource(native_device, device_data, dst_resource, src_resource);
   }

   // The stage-1 permutation a draw uses, or null
   static const TonemapPermDesc* FindTonemapPerm(const ShaderHashesList<OneShaderPerPipeline>& original_shader_hashes)
   {
      for (const TonemapPermDesc& candidate : g_tonemap_perms)
      {
         if (original_shader_hashes.Contains(candidate.hash, reshade::api::shader_stage::pixel))
            return &candidate;
      }
      return nullptr;
   }

   // An upscaler is picked and hasn't failed (it then gives way to SMAA until picked again). Fixed for the whole frame (see
   // "OnPresent"): a selection made after the motion vector state was set would otherwise run the upscaler on mixed state.
   static bool IsSRActive(DeviceData& device_data)
   {
      return GetGameDeviceData(device_data).sr_active;
   }

   // XeGTAO noise per frame and one denoise pass (Intel's XeGTAO.h with TAA) while DLSS / FSR accumulate the lit scene the AO multiplies
   // into; without them a moving pattern would boil, so it stays frozen and denoises twice
   static bool IsGTAOTemporal(DeviceData& device_data)
   {
      return g_gtao_temporal ? g_gtao_temporal == 2 : IsSRActive(device_data);
   }

   // A b1 copy's PreViewTranslation (see "kPreViewTranslationOffset")
   static std::array<float, 3> GetPreViewTranslation(const std::vector<uint8_t>& camera)
   {
      std::array<float, 3> translation = {};
      if (camera.size() >= kPreViewTranslationOffset + sizeof(translation))
         std::memcpy(translation.data(), camera.data() + kPreViewTranslationOffset, sizeof(translation));
      return translation;
   }

   // A b0 / b1 / b3 buffer's CPU copy (null until its first update); registers it for a copy at every update. Under "mv_constants_mutex".
   static MassEffectGameDeviceData::ConstantsCopy GetConstantsCopy(MassEffectGameDeviceData* game_device_data, ID3D11Buffer* buffer)
   {
      return buffer ? game_device_data->mv_constants_copies[reinterpret_cast<uint64_t>(buffer)] : nullptr;
   }

   // Motion vectors: the CPU copy of a registered b0 / b1 / b3 buffer, from the game's UpdateSubresource (before it runs). UE3 uploads
   // whole buffers (no box: "size" is UINT64_MAX); a partial update is merged into the last copy. Needs the ReShade build with full
   // add-on support (the signed one raises no event for UpdateSubresource).
   // ponytail: one allocation per update (a few thousand a frame); pool the copies if the hook shows in a profile
   static bool OnUpdateBufferRegion(reshade::api::device* device, const void* data, reshade::api::resource resource, uint64_t offset, uint64_t size)
   {
      DeviceData* const device_data = device->get_private_data<DeviceData>();
      if (!device_data || !device_data->game || !data)
         return false;
      auto& game_device_data = GetGameDeviceData(*device_data);
      if (!game_device_data.mv_active)
         return false;
#if DEVELOPMENT
      const PerfHookTimer timer{game_device_data.perf_hook_ns};
#endif
      const std::lock_guard lock(game_device_data.mv_constants_mutex);
      const auto copy = game_device_data.mv_constants_copies.find(resource.handle);
      if (copy == game_device_data.mv_constants_copies.end())
         return false;
      D3D11_BUFFER_DESC desc;
      reinterpret_cast<ID3D11Buffer*>(resource.handle)->GetDesc(&desc);
      if (offset >= desc.ByteWidth)
         return false;
      const size_t updated_size = size_t((std::min)(size, uint64_t(desc.ByteWidth) - offset));
      const auto* const bytes = static_cast<const uint8_t*>(data);
      if (updated_size == desc.ByteWidth)
      {
         copy->second = std::make_shared<std::vector<uint8_t>>(bytes, bytes + updated_size);
      }
      else
      {
         auto updated = copy->second && copy->second->size() == desc.ByteWidth ? std::make_shared<std::vector<uint8_t>>(*copy->second) : std::make_shared<std::vector<uint8_t>>(desc.ByteWidth);
         std::memcpy(updated->data() + offset, bytes, updated_size);
         copy->second = std::move(updated);
      }
#if DEVELOPMENT
      game_device_data.mv_stats.updates++;
#endif
      return false;
   }

   // The bound shader's motion vector version, patched from Core's bytecode copy on first use (null if it can't be). Vertex shaders
   // that don't place vertices with b1's ViewProjectionMatrix are refused: jittered, they would shift against their own UVs.
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
            if constexpr (vertex)
               patched = MotionVectorPatch::PatchVertexShader(code, desc->code_size, MotionVectorPatches::layout, &error);
            else
               patched = MotionVectorPatch::PatchPixelShader(code, desc->code_size, MotionVectorPatches::layout, &error);
            com_ptr<ID3D11ShaderReflection> reflection;
            if (vertex && !patched.empty() && Shader::d3d_reflect && SUCCEEDED(Shader::d3d_reflect(code, desc->code_size, IID_PPV_ARGS(&reflection))))
            {
               D3D11_SHADER_VARIABLE_DESC variable_desc;
               if (FAILED(reflection->GetConstantBufferByName("VSOffsetConstants")->GetVariableByName("ViewProjectionMatrix")->GetDesc(&variable_desc)) || (variable_desc.uFlags & D3D_SVF_USED) == 0)
               {
                  patched.clear();
                  error = "screen space (ViewProjectionMatrix unused)";
                  screen_space = true;
               }
               else
               {
                  D3D11_SHADER_INPUT_BIND_DESC bind_desc;
                  const bool skinned = SUCCEEDED(reflection->GetResourceBindingDescByName("VSBoneConstants", &bind_desc)) && bind_desc.BindPoint == MotionVectorPatches::previous_slots[2].first;
                  // The object's translation, LocalToWorld's 4th row (row vectors)
                  const bool translated = SUCCEEDED(reflection->GetConstantBufferByName("$Globals")->GetVariableByName("LocalToWorld")->GetDesc(&variable_desc)) && (variable_desc.uFlags & D3D_SVF_USED) != 0;
                  const std::unique_lock lock(game_device_data.mv_mutex);
                  if (skinned)
                     game_device_data.mv_bone_vertex_shaders.insert(hash);
                  if (translated)
                     game_device_data.mv_translation_offsets[hash] = variable_desc.StartOffset + 3 * 16;
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
         reshade::log::message((shader || screen_space) ? reshade::log::level::info : reshade::log::level::warning, std::format("[MELE MV] {} 0x{:08X} {}", vertex ? "VS" : "PS", hash, shader ? "patched" : error).c_str());
      const std::unique_lock lock(game_device_data.mv_mutex);
      return shaders->try_emplace(hash, shader).first->second;
   }

   // The bound vertex shader's patched version (null if refused), looked up again only when the game's changes
   static ID3D11VertexShader* GetPatchedVertexShader(ID3D11Device* native_device, const CommandListData& cmd_list_data, DeviceData& device_data, uint32_t hash)
   {
      auto& game_device_data = GetGameDeviceData(device_data);
      if (hash != game_device_data.mv_last_vertex_shader_hash)
      {
         game_device_data.mv_last_vertex_shader = GetMotionVectorShader(native_device, device_data, &game_device_data.mv_vertex_shaders, hash, cmd_list_data.pipeline_state_original_vertex_shader).get();
         const std::shared_lock lock(game_device_data.mv_mutex);
         game_device_data.mv_last_vertex_shader_skinned = game_device_data.mv_bone_vertex_shaders.contains(hash);
         const auto translation = game_device_data.mv_translation_offsets.find(hash);
         game_device_data.mv_last_vertex_shader_translation = translation != game_device_data.mv_translation_offsets.end() ? translation->second : UINT_MAX;
         game_device_data.mv_last_vertex_shader_hash = hash;
      }
      return game_device_data.mv_last_vertex_shader;
   }

   // A b1 copy's ViewProjectionMatrix (row vectors) as clip z = A * clip w + B, w the view depth: A from the z and w columns, B at the
   // camera. A <= 1 is an infinite far plane.
   static std::array<double, 2> ViewDepthTerms(const std::vector<uint8_t>& camera)
   {
      if (camera.size() < 16 * sizeof(float))
         return {};
      const float* const vp = reinterpret_cast<const float*>(camera.data());
      const double w_axis = double(vp[3]) * vp[3] + double(vp[7]) * vp[7] + double(vp[11]) * vp[11];
      const double a = w_axis > 0.0 ? (double(vp[2]) * vp[3] + double(vp[6]) * vp[7] + double(vp[10]) * vp[11]) / w_axis : 0.0;
      return {a, double(vp[14]) - a * vp[15]};
   }

   // Near plane of a b1 copy's ViewProjectionMatrix: -B / A (see "ViewDepthTerms")
   static double ViewNearPlane(const std::vector<uint8_t>& camera)
   {
      const auto [a, b] = ViewDepthTerms(camera);
      return a != 0.0 ? -b / a : 0.0;
   }

   // Determinant of a b1 copy's ViewProjectionMatrix rotation and scale (its upper 3x3)
   static double ViewDeterminant(const std::vector<uint8_t>& camera)
   {
      if (camera.size() < 16 * sizeof(float))
         return 0.0;
      const float* const m = reinterpret_cast<const float*>(camera.data());
      return double(m[0]) * (double(m[5]) * m[10] - double(m[6]) * m[9]) - double(m[1]) * (double(m[4]) * m[10] - double(m[6]) * m[8]) + double(m[2]) * (double(m[4]) * m[9] - double(m[5]) * m[8]);
   }

   // Planar reflections (water) render a mirrored view (negative determinant; also an oblique near plane) into the scene's own color
   // and depth before the main view: they are no part of the scene (no motion vectors, no jitter, never open it). Same meshes as the
   // main view, so their draws would also be matched with the reflection's camera.
   static bool IsMirroredView(ID3D11DeviceContext* native_device_context, MassEffectGameDeviceData* game_device_data)
   {
      com_ptr<ID3D11Buffer> camera_buffer;
      native_device_context->VSGetConstantBuffers(MotionVectorPatches::object_slot, 1, &camera_buffer);
      MassEffectGameDeviceData::ConstantsCopy camera;
      {
         const std::lock_guard lock(game_device_data->mv_constants_mutex);
         camera = GetConstantsCopy(game_device_data, camera_buffer.get());
      }
      if (camera != game_device_data->view_camera)
      {
         game_device_data->view_mirrored = camera && ViewDeterminant(*camera) < 0.0;
         game_device_data->view_camera = std::move(camera);
      }
      return game_device_data->view_mirrored;
   }

   // Whether a depth view is output sized (the scene's, not a shadow's or a capture's own), cached for the last view checked (reset at
   // present: views can be recreated)
   static bool IsOutputSizedDepth(DeviceData& device_data, MassEffectGameDeviceData* game_device_data, ID3D11DepthStencilView* dsv)
   {
      if (dsv != game_device_data->view_dsv)
      {
         uint4 size;
         DXGI_FORMAT unused_format;
         GetResourceInfo(dsv, size, unused_format);
         game_device_data->view_dsv = dsv;
         game_device_data->view_dsv_output = size.x == device_data.output_resolution.x && size.y == device_data.output_resolution.y;
      }
      return game_device_data->view_dsv_output;
   }

   // A later view into output sized depth than the one that opened the scene: a scene capture (not mirrored: another camera, a far near
   // plane) draws into the scene's targets before the main view, every other frame. The main view is the last one before post, so the
   // scene reopens for it. Needs "IsMirroredView" first (it reads the draw's b1).
   static bool IsNewView(DeviceData& device_data, MassEffectGameDeviceData* game_device_data, ID3D11DepthStencilView* dsv)
   {
      if (!game_device_data->view_camera || game_device_data->view_camera == game_device_data->scene_view_camera || !IsOutputSizedDepth(device_data, game_device_data, dsv))
         return false;
      // The same view (re-uploaded after shadow views, or its ViewProjectionMatrix slightly changed between passes): the same
      // PreViewTranslation (the camera position), near plane and determinant (field of view, mirroring)
      const auto& scene_camera = game_device_data->scene_view_camera;
      const auto& camera = game_device_data->view_camera;
      if (!scene_camera)
         return false;
      const std::array<float, 3> scene_translation = GetPreViewTranslation(*scene_camera), translation = GetPreViewTranslation(*camera);
      const double scene_near = ViewNearPlane(*scene_camera), near_plane = ViewNearPlane(*camera);
      const double scene_determinant = ViewDeterminant(*scene_camera), determinant = ViewDeterminant(*camera);
      const bool same_view = std::abs(scene_translation[0] - translation[0]) <= 1.f && std::abs(scene_translation[1] - translation[1]) <= 1.f && std::abs(scene_translation[2] - translation[2]) <= 1.f &&
                             std::abs(scene_near - near_plane) <= 0.01 * std::abs(scene_near) && std::abs(scene_determinant - determinant) <= 0.01 * std::abs(scene_determinant);
      if (same_view)
         game_device_data->scene_view_camera = camera;
      return !same_view;
   }

   // Opens the scene at the frame's first mesh draw into output sized depth (the depth prepass): takes the scene depth and picks the
   // jitter the whole scene draws with. Once per present (the HUD and later passes never reopen it).
   static void OpenScene(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, const CommandListData& cmd_list_data, DeviceData& device_data, uint32_t vertex_shader_hash, ID3D11DepthStencilView* dsv)
   {
      auto& game_device_data = GetGameDeviceData(device_data);
      if (!IsOutputSizedDepth(device_data, &game_device_data, dsv) || !GetPatchedVertexShader(native_device, cmd_list_data, device_data, vertex_shader_hash))
         return;
      game_device_data.mv_scene_open = true;
#if DEVELOPMENT
      if (auto* const perf_queries = game_device_data.perf_frame_queries; perf_queries && !std::exchange(perf_queries->scene_started, true))
         native_device_context->End(perf_queries->scene_start.get());
#endif
      game_device_data.scene_view_camera = game_device_data.view_camera;
      game_device_data.mv_depth.reset();
      dsv->GetResource(&game_device_data.mv_depth);
      game_device_data.mv_depth_copy = 0;
      game_device_data.mv_scene_color.reset();
      game_device_data.mv_scene_color_copy = 0;
      game_device_data.jitter_dsv = nullptr;
      game_device_data.jitter_depth_stencil_state = nullptr;
      game_device_data.jitter_depth_test = true;
      game_device_data.mv_accepted_rtv = nullptr;
      game_device_data.mv_accepted_dsv = nullptr;
      game_device_data.mv_refused_rtv = nullptr;
      game_device_data.mv_refused_dsv = nullptr;
      game_device_data.mv_blend_state = nullptr;
      game_device_data.mv_blend_opaque = true;
      // Halton (2, 3) over the upscaler's phase count; pixels to NDC (y up)
      const SR::InstanceData* const sr_instance_data = IsSRActive(device_data) ? device_data.GetSRInstanceData() : nullptr;
      const unsigned int phase = cb_luma_global_settings.FrameIndex % (sr_instance_data ? (std::max)(sr_implementations[device_data.sr_type]->GetJitterPhases(sr_instance_data), 1) : SR::GetDefaultJitterPhases());
      bool jitter = (sr_instance_data || g_mv_force_jitter) && !g_mv_disable_jitter;
#if DEVELOPMENT
      jitter &= perf_test_modes[g_perf_test].motion_vector_draws >= 1; // "Performance Test" without jitter draws
#endif
      game_device_data.mv_jitter = jitter ? std::array<float, 2>{SR::HaltonSequence(phase, 2), SR::HaltonSequence(phase, 3)} : std::array<float, 2>{};
      game_device_data.mv_jitter_ndc = {game_device_data.mv_jitter[0] * 2.f / device_data.output_resolution.x, game_device_data.mv_jitter[1] * -2.f / device_data.output_resolution.y};
      const float ndc_jitter[4] = {game_device_data.mv_jitter_ndc[0], game_device_data.mv_jitter_ndc[1], 0.f, 0.f};
      if (!PatchedDraws::WriteDynamicConstants(native_device, native_device_context, std::addressof(game_device_data.mv_jitter_buffer), ndc_jitter, sizeof(ndc_jitter)))
      {
         // No stale jitter on the scene draws either: no motion vectors this frame
         game_device_data.mv_jitter = {};
         game_device_data.mv_jitter_ndc = {};
         game_device_data.mv_jitter_buffer.reset();
      }
   }

#if DEVELOPMENT
#define MV_REJECT(reason) (GetGameDeviceData(device_data).mv_stats.rejected[reason]++, false)
#else
#define MV_REJECT(reason) false
#endif

   // Draws an opaque draw into the fp16 scene (the base pass: the scene target and its other targets below "target_slot", output
   // sized, with the scene depth) with the patched shaders, adding the motion vector target ("target_slot") and the previous frame's
   // b0 / b1 / b3 ("previous_slots"). False if it can't (the draw then goes to "DrawWithJitter").
   static bool DrawWithMotionVectors(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, const CommandListData& cmd_list_data, DeviceData& device_data, const ShaderHashesList<OneShaderPerPipeline>& original_shader_hashes, const std::function<void()>& draw, const com_ptr<ID3D11RenderTargetView> (&rtvs)[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT], ID3D11DepthStencilView* dsv)
   {
      auto& game_device_data = GetGameDeviceData(device_data);
      // Nothing at or past the motion vector slot, but the motion vector target the last motion vector draw left bound
      for (UINT slot = MotionVectorPatches::target_slot; slot < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; slot++)
      {
         if (rtvs[slot] && (slot != MotionVectorPatches::target_slot || rtvs[slot] != game_device_data.mv_rtv))
            return MV_REJECT(0);
      }
      if (!rtvs[0] || !dsv || !game_device_data.mv_scene_open)
         return MV_REJECT(1);
      // Known targets: checked, and the motion vector target built for them
      if (rtvs[0].get() != game_device_data.mv_accepted_rtv || dsv != game_device_data.mv_accepted_dsv)
      {
         // Refused targets stay refused until the scene reopens (the scene depth and color only change there)
         if (rtvs[0].get() == game_device_data.mv_refused_rtv && dsv == game_device_data.mv_refused_dsv)
            return MV_REJECT(game_device_data.mv_refused_reason);
         const auto refuse = [&](int reason)
         {
            game_device_data.mv_refused_rtv = rtvs[0].get();
            game_device_data.mv_refused_dsv = dsv;
            game_device_data.mv_refused_reason = reason;
            return MV_REJECT(reason);
         };
         com_ptr<ID3D11Resource> depth;
         dsv->GetResource(&depth);
         com_ptr<ID3D11Resource> color;
         rtvs[0]->GetResource(&color);
         if (depth != game_device_data.mv_depth || !color || (game_device_data.mv_scene_color && color != game_device_data.mv_scene_color))
            return refuse(2);
         // UE3 may view its single sample targets as multisampled (TEXTURE2DMS)
         D3D11_RENDER_TARGET_VIEW_DESC rtv_desc;
         rtvs[0]->GetDesc(&rtv_desc);
         com_ptr<ID3D11Texture2D> color_texture;
         D3D11_TEXTURE2D_DESC color_desc = {};
         if (SUCCEEDED(color->QueryInterface(&color_texture)))
            color_texture->GetDesc(&color_desc);
#if DEVELOPMENT
         game_device_data.mv_stats.rejected_format = rtv_desc.Format;
         game_device_data.mv_stats.rejected_dimension = rtv_desc.ViewDimension;
         game_device_data.mv_stats.rejected_width = color_desc.Width;
         game_device_data.mv_stats.rejected_height = color_desc.Height;
#endif
         if (rtv_desc.Format != DXGI_FORMAT_R16G16B16A16_FLOAT || (rtv_desc.ViewDimension != D3D11_RTV_DIMENSION_TEXTURE2D && rtv_desc.ViewDimension != D3D11_RTV_DIMENSION_TEXTURE2DMS) || color_desc.SampleDesc.Count != 1)
            return refuse(3);
         const uint2 size = {color_desc.Width, color_desc.Height};
         if (size.x != device_data.output_resolution.x || size.y != device_data.output_resolution.y)
            return refuse(4);
         game_device_data.mv_scene_color = color;
         const std::unique_lock lock(game_device_data.mv_mutex);
         D3D11_TEXTURE2D_DESC desc = {};
         if (game_device_data.mv_texture)
            game_device_data.mv_texture->GetDesc(&desc);
         // R16G16_FLOAT: DLSS takes it, FSR keeps 16 bits internally; the error is under 0.1% of the motion (Borderlands GOTY Enhanced)
         constexpr DXGI_FORMAT format = DXGI_FORMAT_R16G16_FLOAT;
         if (desc.Width != size.x || desc.Height != size.y)
         {
            game_device_data.mv_texture.reset();
            game_device_data.mv_rtv.reset();
            game_device_data.mv_uav.reset();
            // The fill reads the target back through its UAV
            D3D11_FEATURE_DATA_FORMAT_SUPPORT2 support = {format};
            const bool typed_uav_load = SUCCEEDED(native_device->CheckFeatureSupport(D3D11_FEATURE_FORMAT_SUPPORT2, &support, sizeof(support))) && (support.OutFormatSupport2 & D3D11_FORMAT_SUPPORT2_UAV_TYPED_LOAD) != 0;
            desc = {size.x, size.y, 1, 1, format, {1, 0}, D3D11_USAGE_DEFAULT, D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | (typed_uav_load ? D3D11_BIND_UNORDERED_ACCESS : 0u)};
            if (FAILED(native_device->CreateTexture2D(&desc, nullptr, &game_device_data.mv_texture)) || FAILED(native_device->CreateRenderTargetView(game_device_data.mv_texture.get(), nullptr, &game_device_data.mv_rtv)))
            {
               game_device_data.mv_texture.reset();
               game_device_data.mv_rtv.reset();
               return MV_REJECT(5);
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
         // Additive lights, decals and translucents keep the motion vectors of what's behind them, and so do draws that write no color
         // (occlusion query bounding boxes: every blend state writes the motion vector target, see "OnCreateBlendState")
         const D3D11_RENDER_TARGET_BLEND_DESC& rt0_blend = blend_desc.RenderTarget[0];
         game_device_data.mv_blend_opaque = rt0_blend.RenderTargetWriteMask != 0 && (!rt0_blend.BlendEnable || (rt0_blend.SrcBlend == D3D11_BLEND_ONE && rt0_blend.DestBlend == D3D11_BLEND_ZERO && rt0_blend.BlendOp == D3D11_BLEND_OP_ADD));
         game_device_data.mv_blend_state = blend_state.get();
      }
      if (!game_device_data.mv_blend_opaque)
         return MV_REJECT(6);

      ID3D11VertexShader* const vertex_shader = GetPatchedVertexShader(native_device, cmd_list_data, device_data, original_shader_hashes.vertex_shaders[0]);
      if (const uint32_t pixel_shader_hash = original_shader_hashes.pixel_shaders[0]; pixel_shader_hash != game_device_data.mv_last_pixel_shader_hash)
      {
         game_device_data.mv_last_pixel_shader = GetMotionVectorShader(native_device, device_data, &game_device_data.mv_pixel_shaders, pixel_shader_hash, cmd_list_data.pipeline_state_original_pixel_shader).get();
         game_device_data.mv_last_pixel_shader_hash = pixel_shader_hash;
      }
      ID3D11PixelShader* const pixel_shader = game_device_data.mv_last_pixel_shader;
      if (!vertex_shader || !pixel_shader || !game_device_data.mv_jitter_buffer)
         return MV_REJECT(7);
      const bool frame_start = std::exchange(game_device_data.mv_frame_ended, false);
      if (frame_start || std::exchange(game_device_data.mv_view_restarted, false))
      {
         // The camera fill's and the upscaler's depth: the scene depth if it can be read, else its copy (taken after the depth prepass)
         game_device_data.mv_depth_srv.reset();
         com_ptr<ID3D11Resource> depth = game_device_data.mv_depth;
         com_ptr<ID3D11Texture2D> depth_texture;
         D3D11_TEXTURE2D_DESC depth_desc = {};
         if (depth && SUCCEEDED(depth->QueryInterface(&depth_texture)))
            depth_texture->GetDesc(&depth_desc);
         if ((depth_desc.BindFlags & D3D11_BIND_SHADER_RESOURCE) == 0 && game_device_data.mv_depth_copy)
         {
            depth_texture.reset();
            depth_desc = {};
            if (SUCCEEDED(reinterpret_cast<ID3D11Resource*>(game_device_data.mv_depth_copy)->QueryInterface(&depth_texture)))
               depth_texture->GetDesc(&depth_desc);
         }
         if ((depth_desc.BindFlags & D3D11_BIND_SHADER_RESOURCE) != 0 && (depth_desc.Format == DXGI_FORMAT_R24G8_TYPELESS || depth_desc.Format == DXGI_FORMAT_R32G8X24_TYPELESS || depth_desc.Format == DXGI_FORMAT_R32_TYPELESS) && depth_desc.SampleDesc.Count == 1)
         {
            const DXGI_FORMAT srv_format = depth_desc.Format == DXGI_FORMAT_R24G8_TYPELESS ? DXGI_FORMAT_R24_UNORM_X8_TYPELESS : (depth_desc.Format == DXGI_FORMAT_R32G8X24_TYPELESS ? DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS : DXGI_FORMAT_R32_FLOAT);
            const CD3D11_SHADER_RESOURCE_VIEW_DESC srv_desc(D3D11_SRV_DIMENSION_TEXTURE2D, srv_format, 0, 1);
            native_device->CreateShaderResourceView(depth_texture.get(), &srv_desc, &game_device_data.mv_depth_srv);
         }
         game_device_data.mv_fill_pending = game_device_data.mv_depth_srv && game_device_data.mv_uav && FindShader(device_data.native_compute_shaders, kNameMVFillCS) != nullptr;
         // The fill's marker: the largest float16 (a larger clear value is stored as it in R16G16_FLOAT)
         const FLOAT clear_value = game_device_data.mv_fill_pending ? 65504.f : 0.f;
         const FLOAT clear[4] = {clear_value, clear_value, 0.f, 0.f};
         native_device_context->ClearRenderTargetView(game_device_data.mv_rtv.get(), clear);
      }
      if (frame_start)
      {
         // Last frame's camera and objects are the previous ones, unless frames without a scene (menus, videos) came between
         const bool previous_valid = game_device_data.mv_camera && cb_luma_global_settings.FrameIndex - game_device_data.mv_frame_index <= 1;
         game_device_data.mv_previous_camera = previous_valid ? game_device_data.mv_camera : nullptr;
         game_device_data.mv_camera = nullptr;
         game_device_data.mv_frame_index = cb_luma_global_settings.FrameIndex;
         // Swapped, not rebuilt: the lists keep their nodes and capacity (an empty list matches nothing); keys drawn in neither of the last
         // two frames go
         game_device_data.mv_previous_objects.swap(game_device_data.mv_objects);
         std::erase_if(game_device_data.mv_objects, [](const auto& entry)
            { return entry.second.empty(); });
         for (auto& entry : game_device_data.mv_objects)
            entry.second.clear();
         if (!previous_valid)
            game_device_data.mv_previous_objects.clear();
      }

      // The game's b0 / b1 / b3. The slots added past them stay bound after the draw: no game vertex shader reads a constant buffer
      // at slot 4 or above.
      com_ptr<ID3D11Buffer> game_cbs[4];
      native_device_context->VSGetConstantBuffers(0, UINT(std::size(game_cbs)), &game_cbs[0]);
      ID3D11Buffer* const current[std::size(MotionVectorPatches::previous_slots)] = {game_cbs[MotionVectorPatches::previous_slots[0].first].get(), game_cbs[MotionVectorPatches::previous_slots[1].first].get(), game_cbs[MotionVectorPatches::previous_slots[2].first].get()};
      const bool skinned = game_device_data.mv_last_vertex_shader_skinned;
      // b1's copy was taken for this draw by "IsMirroredView" (every draw with a depth target)
      MassEffectGameDeviceData::ConstantsCopy object, camera = game_device_data.view_camera, bones;
      {
         const std::lock_guard lock(game_device_data.mv_constants_mutex);
         object = GetConstantsCopy(&game_device_data, current[0]);
         if (skinned)
            bones = GetConstantsCopy(&game_device_data, current[2]);
      }
      const auto copy_size = [](const MassEffectGameDeviceData::ConstantsCopy& copy)
      { return copy ? copy->size() : size_t(0); };
#if DEVELOPMENT
      // "Performance Test" without motion vector draws: the frame (camera, target clear, camera fill, upscaler) still happens, the draws
      // run jittered only, or untouched
      if (perf_test_modes[g_perf_test].motion_vector_draws < 2)
      {
         if (object && camera && !game_device_data.mv_camera)
            game_device_data.mv_camera = camera;
         return false;
      }
#endif
      // The previous frame's b0 / b1 / b3: the same object's from last frame, else this draw's with last frame's world camera (no object
      // motion). None (no CPU copy yet, or an unmatched draw with another camera): the current ones (zero motion).
      const std::vector<uint8_t>* uploads[std::size(MotionVectorPatches::previous_slots)] = {};
      std::vector<uint8_t> object_previous_view; // Outlives "mv_previous_constants.Bind"
      if (object && camera && (!skinned || bones))
      {
         // The world camera: the frame's first motion vector draw's
         if (!game_device_data.mv_camera)
            game_device_data.mv_camera = camera;

         // Draw key: same mesh, same shaders. Objects sharing it (props) are told apart by translation. No instance count.
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
         // The translation in world space: LocalToWorld includes the camera's PreViewTranslation
         const std::array<float, 3> view_translation = GetPreViewTranslation(*camera);
         const uint32_t translation_offset = game_device_data.mv_last_vertex_shader_translation;
         const bool translated = translation_offset != UINT_MAX && translation_offset + sizeof(float) * 3 <= object->size();
         std::array<float, 3> translation = {};
         if (translated)
         {
            std::memcpy(translation.data(), object->data() + translation_offset, sizeof(translation));
            for (size_t i = 0; i < translation.size(); i++)
               translation[i] -= view_translation[i];
         }

         // ponytail: linear search among the key's candidates (a handful at most); a spatial lookup if big crowds share a mesh
         const MassEffectGameDeviceData::MotionVectorObject* match = nullptr;
         if (const auto previous = game_device_data.mv_previous_objects.find(key); previous != game_device_data.mv_previous_objects.end())
         {
            float nearest = FLT_MAX;
            for (const auto& candidate : previous->second)
            {
               const float dx = candidate.translation[0] - translation[0], dy = candidate.translation[1] - translation[1], dz = candidate.translation[2] - translation[2];
               const float distance = dx * dx + dy * dy + dz * dz;
               if (copy_size(candidate.object) == object->size() && copy_size(candidate.camera) == camera->size() && copy_size(candidate.bones) == copy_size(bones) && distance < nearest)
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
            uploads[1] = match->camera.get();
            if (skinned)
               uploads[2] = match->bones.get();
#if DEVELOPMENT
            game_device_data.mv_stats.matched++;
            game_device_data.mv_stats.matched_same_camera += match->camera->size() == camera->size() && std::memcmp(match->camera->data(), camera->data(), camera->size()) == 0;
#endif
         }
         else if (game_device_data.mv_previous_camera && copy_size(game_device_data.mv_previous_camera) == camera->size() && (camera == game_device_data.mv_camera || std::memcmp(camera->data(), game_device_data.mv_camera->data(), camera->size()) == 0))
         {
            // Not found, drawn with the world camera: its own constants with last frame's world camera (camera motion only), its
            // LocalToWorld moved to last frame's PreViewTranslation
            uploads[1] = game_device_data.mv_previous_camera.get();
            if (translated)
            {
               const std::array<float, 3> previous_view_translation = GetPreViewTranslation(*game_device_data.mv_previous_camera);
               object_previous_view = *object;
               float row[3];
               std::memcpy(row, object_previous_view.data() + translation_offset, sizeof(row));
               for (size_t i = 0; i < std::size(row); i++)
                  row[i] += previous_view_translation[i] - view_translation[i];
               std::memcpy(object_previous_view.data() + translation_offset, row, sizeof(row));
               uploads[0] = &object_previous_view;
            }
#if DEVELOPMENT
            game_device_data.mv_stats.camera_only++;
#endif
         }
#if DEVELOPMENT
         else
         {
            game_device_data.mv_stats.other_camera++;
         }
#endif
         // Kept as drawn for the next frame
         game_device_data.mv_objects[key].push_back({translation, object, camera, bones});
      }
#if DEVELOPMENT
      else
      {
         game_device_data.mv_stats.uncopied++;
      }
#endif
      game_device_data.mv_previous_constants.Bind(native_device, native_device_context, MotionVectorPatches::previous_slots, uploads, current, "MELE");
      ID3D11Buffer* const jitter = game_device_data.mv_jitter_buffer.get();
      native_device_context->VSSetConstantBuffers(MotionVectorPatches::jitter_slot, 1, &jitter);
      // Left bound after the draw (set directly, bypassing Core's state tracking): the game's next draws either bind their own
      // targets and shaders, or are motion vector draws too. The draws in between write no "o4" (no game pixel shader declares a
      // fifth target), so the motion vector target keeps its contents.
      if (rtvs[MotionVectorPatches::target_slot] != game_device_data.mv_rtv)
      {
         ID3D11RenderTargetView* targets[MotionVectorPatches::target_slot + 1] = {};
         for (UINT slot = 0; slot < MotionVectorPatches::target_slot; slot++)
            targets[slot] = rtvs[slot].get();
         targets[MotionVectorPatches::target_slot] = game_device_data.mv_rtv.get();
         native_device_context->OMSetRenderTargets(MotionVectorPatches::target_slot + 1, targets, dsv);
      }
      PatchedDraws::BindPatchedShader(native_device_context, vertex_shader, &game_device_data.mv_bound_vertex_shader);
      PatchedDraws::BindPatchedShader(native_device_context, pixel_shader, &game_device_data.mv_bound_pixel_shader);

      draw();
#if DEVELOPMENT
      game_device_data.mv_stats.motion_vector_draws++;
      if (!game_device_data.views.empty())
         game_device_data.views.back().motion_vector_draws++;
#endif
      return true;
   }

   // Jitter for the scene's mesh draws without motion vectors (patched vertex shader, game pixel shader): the depth prepass, lights,
   // decals, fog volumes, distortion, the velocity pass and translucents. Every draw depth tested against the scene takes the same
   // jitter, or jittered and unjittered depths of the same surface fail each other's test. False if it can't (the draw runs untouched).
   static bool DrawWithJitter(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, const CommandListData& cmd_list_data, DeviceData& device_data, const ShaderHashesList<OneShaderPerPipeline>& original_shader_hashes, const std::function<void()>& draw, ID3D11DepthStencilView* dsv)
   {
      auto& game_device_data = GetGameDeviceData(device_data);
      if (!game_device_data.mv_scene_open || game_device_data.mv_jitter == std::array<float, 2>{} || !game_device_data.mv_jitter_buffer)
         return false;
#if DEVELOPMENT
      // "Performance Test" without motion vector draws: the whole frame unjittered, so its depth tests stay consistent
      if (perf_test_modes[g_perf_test].motion_vector_draws < 1)
         return false;
#endif
      // Meshes only (full screen passes have no vertex buffer or no depth test), into the scene depth (not shadows)
      if (!dsv)
         return false;
      if (dsv != game_device_data.jitter_dsv)
      {
         com_ptr<ID3D11Resource> depth;
         dsv->GetResource(&depth);
         game_device_data.jitter_dsv = dsv;
         game_device_data.jitter_dsv_scene = depth == game_device_data.mv_depth;
      }
      if (!game_device_data.jitter_dsv_scene)
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
      // (a motion vector draw's is put back)
      PatchedDraws::BindPatchedShader(native_device_context, vertex_shader, &game_device_data.mv_bound_vertex_shader);
      ID3D11Buffer* const jitter = game_device_data.mv_jitter_buffer.get();
      native_device_context->VSSetConstantBuffers(MotionVectorPatches::jitter_slot, 1, &jitter);
      PatchedDraws::RestoreGameShader(native_device_context, &game_device_data.mv_bound_pixel_shader);
      draw();
#if DEVELOPMENT
      game_device_data.mv_stats.jitter_draws++;
#endif
      return true;
   }

   // DLSS / FSR on the jittered scene (its last copy, which the post passes read, else the scene itself), the scene depth and the motion
   // vectors, at native resolution; the post passes read its output in place of both (see "RebindUpscaledScene"). False if it didn't
   // draw (missing input, or the upscaler failed).
   static bool DrawUpscaler(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, DeviceData& device_data)
   {
      auto& game_device_data = GetGameDeviceData(device_data);
      if (!game_device_data.mv_texture || !game_device_data.mv_camera || game_device_data.mv_camera->size() < 16 * sizeof(float) || !game_device_data.mv_depth_srv || !game_device_data.mv_scene_color)
         return false;
      ID3D11Resource* const scene_resource = game_device_data.mv_scene_color_copy ? reinterpret_cast<ID3D11Resource*>(game_device_data.mv_scene_color_copy) : game_device_data.mv_scene_color.get();
      com_ptr<ID3D11Texture2D> scene;
      if (FAILED(scene_resource->QueryInterface(&scene)))
         return false;
      D3D11_TEXTURE2D_DESC scene_desc, mv_desc;
      scene->GetDesc(&scene_desc);
      game_device_data.mv_texture->GetDesc(&mv_desc);
      if ((scene_desc.Format != DXGI_FORMAT_R16G16B16A16_FLOAT && scene_desc.Format != DXGI_FORMAT_R16G16B16A16_TYPELESS) || scene_desc.SampleDesc.Count != 1 || (scene_desc.BindFlags & D3D11_BIND_SHADER_RESOURCE) == 0 || scene_desc.Width != mv_desc.Width || scene_desc.Height != mv_desc.Height)
         return false;
      // None when "Super Resolution" changed after present (see "IsSRActive"): no output texture is made for it
      SR::InstanceData* const sr_instance_data = device_data.GetSRInstanceData();
      if (!sr_instance_data)
         return false;
      com_ptr<ID3D11Resource> depth;
      game_device_data.mv_depth_srv->GetResource(&depth);

      D3D11_TEXTURE2D_DESC output_desc = {};
      if (device_data.sr_output_color)
         device_data.sr_output_color->GetDesc(&output_desc);
      if (output_desc.Width != scene_desc.Width || output_desc.Height != scene_desc.Height)
      {
         device_data.sr_output_color.reset();
         output_desc = {scene_desc.Width, scene_desc.Height, 1, 1, DXGI_FORMAT_R16G16B16A16_FLOAT, {1, 0}, D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS};
         game_device_data.sr_output_srv.reset();
         if (SUCCEEDED(native_device->CreateTexture2D(&output_desc, nullptr, &device_data.sr_output_color)) && FAILED(native_device->CreateShaderResourceView(device_data.sr_output_color.get(), nullptr, &game_device_data.sr_output_srv)))
            device_data.sr_output_color.reset();
         game_device_data.sr_output_recreated = true;
      }
      if (!device_data.sr_output_color)
      {
         // Back to SMAA until the upscaler is picked again
         device_data.sr_suppressed = true;
         return false;
      }

      // FSR needs the camera (DLSS ignores it). b1's ViewProjectionMatrix multiplies row vectors: column 1 is the up axis /
      // tan(fov / 2); near and far from "ViewDepthTerms".
      const float* const vp = reinterpret_cast<const float*>(game_device_data.mv_camera->data());
      const double up_length = std::sqrt(double(vp[1]) * vp[1] + double(vp[5]) * vp[5] + double(vp[9]) * vp[9]);
      // The last valid field of view is kept (FSR asserts on 0)
      if (const double vert_fov = up_length > 0.0 ? 2.0 * std::atan(1.0 / up_length) : 0.0; vert_fov > 0.0 && vert_fov <= 3.14159265358979)
         game_device_data.sr_vert_fov = float(vert_fov);
      const auto [a, b] = ViewDepthTerms(*game_device_data.mv_camera);
      const double near_plane = ViewNearPlane(*game_device_data.mv_camera);

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
      settings_data.auto_exposure = device_data.sr_type != SR::Type::FSR; // FSR's clips highlights (Luma canon)
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
      draw_data.vert_fov = game_device_data.sr_vert_fov;
      if (near_plane > 0.0)
      {
         // ponytail: FSR gets a large finite far for an infinite one (Core sets FFX_FSR3_ENABLE_DEPTH_INFINITE only with inverted depth)
         draw_data.near_plane = float(near_plane);
         draw_data.far_plane = float(a > 1.0 ? b / (1.0 - a) : near_plane * 1e6);
      }
      if (!sr_implementations[device_data.sr_type]->Draw(sr_instance_data, native_device_context, draw_data))
      {
         // Back to SMAA until the upscaler is picked again
         device_data.sr_suppressed = true;
         return false;
      }
      game_device_data.sr_input = scene_resource;
      // DLSS draws nothing into a new output texture (the session's first, or one made after "None", which Core frees): the frame shows
      // the texture's stale memory until its feature is created again after a draw. Settings changed once here force that at the next
      // frame's "UpdateSettings".
      if (std::exchange(game_device_data.sr_output_recreated, false) && device_data.sr_type == SR::Type::DLSS)
      {
         SR::SettingsData throwaway_settings_data = settings_data;
         throwaway_settings_data.mvs_jittered = !throwaway_settings_data.mvs_jittered;
         sr_implementations[device_data.sr_type]->UpdateSettings(sr_instance_data, native_device_context, throwaway_settings_data);
      }
      device_data.has_drawn_sr = true;
      device_data.has_drawn_main_post_processing = true; // Core's upscaler status icon
      return true;
   }

   // Ends the scene at its first post pass (immediate context): the camera motion fill for the pixels no motion vector draw wrote, then
   // DLSS / FSR ("DrawUpscaler"), both before that pass reads the scene
   static void EndScene(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, DeviceData& device_data)
   {
      auto& game_device_data = GetGameDeviceData(device_data);
      game_device_data.mv_scene_open = false;
      game_device_data.mv_scene_done = true;
      game_device_data.mv_frame_ended = true;
      DrawStateStack<DrawStateStackType::FullGraphics> graphics_state;
      DrawStateStack<DrawStateStackType::Compute> compute_state;
      graphics_state.Cache(native_device_context, device_data.uav_max_count);
      compute_state.Cache(native_device_context, device_data.uav_max_count);
      auto* const fill_shader = FindShader(device_data.native_compute_shaders, kNameMVFillCS);
#if DEVELOPMENT
      if (g_mv_skip_fill)
         game_device_data.mv_fill_pending = false;
      if (auto* const perf_queries = game_device_data.perf_frame_queries; perf_queries && perf_queries->scene_started && !std::exchange(perf_queries->fill, true))
         native_device_context->End(perf_queries->fill_start.get());
#endif
      if (std::exchange(game_device_data.mv_fill_pending, false) && fill_shader)
      {
         // Current clip space to the previous frame's, for column vectors (b1 holds row vector matrices, transposed here), in double:
         // previous * translation(previous PreViewTranslation - current) * inverse(current)
         Math::Matrix44D current, previous, view_translation;
         current.SetIdentity();
         previous.SetIdentity();
         view_translation.SetIdentity();
         if (game_device_data.mv_camera && game_device_data.mv_previous_camera && game_device_data.mv_camera->size() >= 16 * sizeof(float) && game_device_data.mv_previous_camera->size() >= 16 * sizeof(float))
         {
            std::copy_n(reinterpret_cast<const float*>(game_device_data.mv_camera->data()), 16, current.GetData());
            std::copy_n(reinterpret_cast<const float*>(game_device_data.mv_previous_camera->data()), 16, previous.GetData());
            current.Transpose();
            previous.Transpose();
            current.Invert();
            const std::array<float, 3> current_translation = GetPreViewTranslation(*game_device_data.mv_camera), previous_translation = GetPreViewTranslation(*game_device_data.mv_previous_camera);
            view_translation.m03 = double(previous_translation[0]) - current_translation[0];
            view_translation.m13 = double(previous_translation[1]) - current_translation[1];
            view_translation.m23 = double(previous_translation[2]) - current_translation[2];
         }
         const Math::Matrix44D reprojection = previous * view_translation * current;
         float constants[20] = {}; // A multiple of 16 bytes
         for (int i = 0; i < 16; i++)
            constants[i] = float(reprojection.GetData()[i]);
         constants[16] = game_device_data.mv_jitter_ndc[0];
         constants[17] = game_device_data.mv_jitter_ndc[1];
         if (PatchedDraws::WriteDynamicConstants(native_device, native_device_context, std::addressof(game_device_data.mv_fill_buffer), constants, sizeof(constants)))
         {
            // The depth may be bound as the depth target, and the motion vectors as a render target
            native_device_context->OMSetRenderTargets(0, nullptr, nullptr);
            ID3D11Buffer* const buffer = game_device_data.mv_fill_buffer.get();
            ID3D11ShaderResourceView* const srv = game_device_data.mv_depth_srv.get();
            ID3D11UnorderedAccessView* const uav = game_device_data.mv_uav.get();
            native_device_context->CSSetConstantBuffers(0, 1, &buffer);
            native_device_context->CSSetShaderResources(0, 1, &srv);
            native_device_context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);
            native_device_context->CSSetShader(fill_shader, nullptr, 0);
            native_device_context->Dispatch((uint32_t(device_data.output_resolution.x) + 7) / 8, (uint32_t(device_data.output_resolution.y) + 7) / 8, 1);
            ID3D11UnorderedAccessView* const null_uav = nullptr;
            native_device_context->CSSetUnorderedAccessViews(0, 1, &null_uav, nullptr);
         }
      }
#if DEVELOPMENT
      auto* const perf_queries = game_device_data.perf_frame_queries;
      if (perf_queries && perf_queries->scene_started && !perf_queries->scene)
      {
         native_device_context->End(perf_queries->scene_end.get());
         perf_queries->scene = true;
      }
      // "MV Dump": staging copies of the motion vectors (after the fill) and the depth they were made from
      if (std::exchange(g_mv_dump, false) && game_device_data.mv_texture && game_device_data.mv_depth_srv && game_device_data.mv_camera)
      {
         com_ptr<ID3D11Resource> depth;
         game_device_data.mv_depth_srv->GetResource(&depth);
         ID3D11Resource* const sources[2] = {game_device_data.mv_texture.get(), depth.get()};
         for (size_t i = 0; i < std::size(sources); i++)
         {
            com_ptr<ID3D11Texture2D> texture;
            D3D11_TEXTURE2D_DESC desc = {};
            if (FAILED(sources[i]->QueryInterface(&texture)))
               continue;
            texture->GetDesc(&desc);
            desc.Usage = D3D11_USAGE_STAGING;
            desc.BindFlags = 0;
            desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            desc.MiscFlags = 0;
            game_device_data.dump_textures[i].reset();
            if (SUCCEEDED(native_device->CreateTexture2D(&desc, nullptr, &game_device_data.dump_textures[i])))
               native_device_context->CopyResource(game_device_data.dump_textures[i].get(), texture.get());
         }
         game_device_data.dump_cameras[0] = *game_device_data.mv_camera;
         game_device_data.dump_cameras[1] = game_device_data.mv_previous_camera ? *game_device_data.mv_previous_camera : *game_device_data.mv_camera;
         game_device_data.dump_jitter = game_device_data.mv_jitter;
      }
#endif
      if (IsSRActive(device_data))
      {
         // The scene may still be bound as a render target
         native_device_context->OMSetRenderTargets(0, nullptr, nullptr);
         const bool drawn = DrawUpscaler(native_device, native_device_context, device_data);
#if DEVELOPMENT
         game_device_data.mv_stats.sr_draws += drawn;
         if (perf_queries && perf_queries->scene && !perf_queries->sr && drawn)
         {
            native_device_context->End(perf_queries->sr_end.get());
            perf_queries->sr = true;
         }
#else
         (void)drawn;
#endif
      }
      compute_state.Restore(native_device_context);
      graphics_state.Restore(native_device_context);
   }

   // The post passes from the scene's end up to stage 1 read the upscaled scene where they bind the scene or its copy (any shader
   // resource slot)
   static void RebindUpscaledScene(ID3D11DeviceContext* native_device_context, const MassEffectGameDeviceData& gd, bool compute)
   {
      com_ptr<ID3D11ShaderResourceView> srvs[16]; // t0-t15
      if (compute)
         native_device_context->CSGetShaderResources(0, UINT(std::size(srvs)), &srvs[0]);
      else
         native_device_context->PSGetShaderResources(0, UINT(std::size(srvs)), &srvs[0]);
      ID3D11ShaderResourceView* const upscaled = gd.sr_output_srv.get();
      for (UINT slot = 0; slot < std::size(srvs); slot++)
      {
         if (!srvs[slot])
            continue;
         com_ptr<ID3D11Resource> resource;
         srvs[slot]->GetResource(&resource);
         // The scene itself too ("mv_scene_color" is fixed once the scene has ended)
         if (resource.get() != gd.sr_input && resource.get() != gd.mv_scene_color.get())
            continue;
         if (compute)
            native_device_context->CSSetShaderResources(slot, 1, &upscaled);
         else
            native_device_context->PSSetShaderResources(slot, 1, &upscaled);
      }
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

public:
   void OnInit(bool async) override
   {
      // The game follows Windows HDR. Native HDR runs stage 2, so the frame reaches the swapchain linear and this
      // game owns Game Paper White; native SDR has no stage 2, so it stays gamma and Core's composition owns the
      // decode and the scale. Decided here because the shader compiler starts right after OnInit, with a null
      // window because no swapchain exists yet (Display falls back to the primary monitor).
      bool hdr_supported = false, hdr_enabled = false;
      Display::IsHDRSupportedAndEnabled(0, hdr_supported, hdr_enabled);
      const char native_hdr = hdr_enabled ? '1' : '0';

      auto& post_process_space = GetShaderDefineData(POST_PROCESS_SPACE_TYPE_HASH);
      post_process_space.SetDefaultValue(native_hdr);
      post_process_space.SetValueFixed(true);
      auto& early_display_encoding = GetShaderDefineData(EARLY_DISPLAY_ENCODING_HASH); // Inert unless the above is 1.
      early_display_encoding.SetDefaultValue(native_hdr);
      early_display_encoding.SetValueFixed(true);
      // The stage-1/stage-2 chain already carries gamma 2.2, so 1 would apply the sRGB mismatch a second time and
      // crush shadows. On the native-SDR topology Core still corrects sRGB against 2.2 on its own, through the
      // display-mode term of its composition pass.
      GetShaderDefineData(GAMMA_CORRECTION_TYPE_HASH).SetDefaultValue('0');
      // Exposes UI Paper White without renormalizing the already combined scene/HUD buffer; type 2 would
      // double-apply the transport ratio.
      GetShaderDefineData(UI_DRAW_TYPE_HASH).SetDefaultValue('1');
      use_os_reference_white_level = false; // Explicit Scene and UI Paper White controls.

      // Native post passes use b0-b3; inject Luma cbuffers at the high slots.
      luma_settings_cbuffer_index = 13;
      luma_data_cbuffer_index = 12;

      // Core registers SMAA through ENABLE_SMAA; its neighborhood blend reads a linear decode of the gamma post buffer.
      // RCAS runs afterwards through Copy VS and DrawCustomPixelShader.
      native_shaders_definitions.emplace(kNameSMAALinearizeCS,
         ShaderDefinition("Luma_MELE_SMAALinearize", reshade::api::pipeline_subobject_type::compute_shader));
      native_shaders_definitions.emplace(kNameSharpenPS,
         ShaderDefinition{"Luma_MELE_Sharpen", reshade::api::pipeline_subobject_type::pixel_shader, nullptr, "sharpen_ps"});

      // Four XeGTAO compute entries share one source; XE_GTAO_FINAL_APPLY selects the game's R8_UNORM target.
      native_shaders_definitions.emplace(kNameGTAOPrefilterCS,
         ShaderDefinition{"Luma_MELE_XeGTAO", reshade::api::pipeline_subobject_type::compute_shader, nullptr, "prefilter_depths16x16_cs"});
      native_shaders_definitions.emplace(kNameGTAOMainPassCS,
         ShaderDefinition{"Luma_MELE_XeGTAO", reshade::api::pipeline_subobject_type::compute_shader, nullptr, "main_pass_cs"});
      native_shaders_definitions.emplace(kNameGTAODenoise1CS,
         ShaderDefinition{"Luma_MELE_XeGTAO", reshade::api::pipeline_subobject_type::compute_shader, nullptr, "denoise_pass_cs", {{"XE_GTAO_FINAL_APPLY", "0"}}});
      native_shaders_definitions.emplace(kNameGTAODenoise2CS,
         ShaderDefinition{"Luma_MELE_XeGTAO", reshade::api::pipeline_subobject_type::compute_shader, nullptr, "denoise_pass_cs", {{"XE_GTAO_FINAL_APPLY", "1"}}});

      // DLSS / FSR: camera motion for the motion vector pixels no patched draw wrote (sky, unpatched draws), the CPU copies of the
      // per-draw constants, and the motion vector target written by every blend state
      native_shaders_definitions.emplace(kNameMVFillCS,
         ShaderDefinition("Luma_MELE_MotionVectorFill", reshade::api::pipeline_subobject_type::compute_shader));
      reshade::register_event<reshade::addon_event::update_buffer_region>(OnUpdateBufferRegion);
      reshade::register_event<reshade::addon_event::create_pipeline>(OnCreateBlendState);

      // Very High slice count for spatial stability (no TAA without DLSS/FSR).
      std::vector<ShaderDefineData> game_shader_defines_data = {
         {"XE_GTAO_QUALITY", '3', true, false, "XeGTAO quality (slice count)\n0 - Low\n1 - Medium\n2 - High\n3 - Very High\n4 - Ultra", 4},
      };
      shader_defines_data.append_range(game_shader_defines_data);

      // Grade controls default to a vanilla no-op. Contrast, Saturation and HighlightDechroma are HDR only.
      default_luma_global_game_settings.Exposure = 1.f;
      default_luma_global_game_settings.Saturation = 1.f;
      default_luma_global_game_settings.HighlightDechroma = 0.f;
      default_luma_global_game_settings.Contrast = 1.f;
      default_luma_global_game_settings.VignetteIntensity = 1.f;
      default_luma_global_game_settings.FilmGrainIntensity = 1.f;
      default_luma_global_game_settings.BloomIntensity = g_bloom_intensity; // Scaled by the live per-scene cb0.x.
      default_luma_global_game_settings.BloomThreshold = 1.2f;              // ME1LE's value, the fallback in every game until live capture succeeds.
      default_luma_global_game_settings.Dithering = 1.f;                    // Output anti-banding.
      default_luma_global_game_settings.VideoAutoHDREnable = 1.f;           // Off preserves vanilla SDR video.
      default_luma_global_game_settings.VideoAutoHDRBoost = 0.5f;           // 0=1x, 0.5=2.0625x, 1=3.125x UI white.
      default_luma_global_game_settings.VideoOnSwapchain = 0.f;             // Set per Bink draw.
      cb_luma_global_settings.GameSettings = default_luma_global_game_settings;
   }

   void OnCreateDevice(ID3D11Device* native_device, DeviceData& device_data) override
   {
      device_data.game = new MassEffectGameDeviceData;
      device_data.taa_detected = true; // No TAA to replace, but Core's upscaler UI checks for it
   }

   void OnDestroyDeviceData(DeviceData& device_data) override
   {
      // GameDeviceData lacks a virtual destructor; delete through the concrete type to release derived members.
      delete static_cast<MassEffectGameDeviceData*>(device_data.game);
      device_data.game = nullptr;
   }

   // Capture bright-pass cb0 into a staging ring and map the oldest entry with DO_NOT_WAIT, so the readback
   // never stalls. OnPresent turns the live artist-authored BloomScale into the effective intensity.
   void CaptureBloomScale(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, MassEffectGameDeviceData& gd, const ShaderHashesList<OneShaderPerPipeline>& original_shader_hashes)
   {
      if (g_bloom_enable && original_shader_hashes.Contains(kBloomBrightPassHash, reshade::api::shader_stage::pixel) && !gd.bloom_scale_captured_this_frame)
      {
         gd.bloom_scale_captured_this_frame = true; // Do not advance the ring twice in one frame.
#if DEVELOPMENT
         gd.bloom_bright_pass_hits++;
#endif
         ComPtr<ID3D11Buffer> cb0;
         native_device_context->PSGetConstantBuffers(0, 1, cb0.put());
         if (cb0)
         {
            if (!gd.bloom_scale_ring[2]) // Gate on the last slot so partial allocation retries next frame.
            {
               D3D11_BUFFER_DESC bd{};
               cb0->GetDesc(&bd);
               bd.Usage = D3D11_USAGE_STAGING;
               bd.BindFlags = 0;
               bd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
               bd.MiscFlags = 0;
               for (auto& b : gd.bloom_scale_ring)
                  b = nullptr; // com_ptr::put() requires null and this discards prior partial allocation.
               for (auto& b : gd.bloom_scale_ring)
               {
                  if (FAILED(native_device->CreateBuffer(&bd, nullptr, b.put())))
                  {
                     for (auto& r : gd.bloom_scale_ring)
                        r = nullptr;
                     break;
                  }
               }
            }
            if (gd.bloom_scale_ring[2])
            {
               native_device_context->CopyResource(gd.bloom_scale_ring[gd.bloom_scale_ring_wr].get(), cb0.get());
               if (gd.bloom_scale_ring_filled < 3)
                  gd.bloom_scale_ring_filled++;
               if (gd.bloom_scale_ring_filled >= 3)
               {
                  const int oldest = (gd.bloom_scale_ring_wr + 1) % 3; // Written two frames ago and expected idle.
                  D3D11_MAPPED_SUBRESOURCE ms{};
                  if (SUCCEEDED(native_device_context->Map(gd.bloom_scale_ring[oldest].get(), 0, D3D11_MAP_READ, D3D11_MAP_FLAG_DO_NOT_WAIT, &ms)))
                  {
                     const float* f = reinterpret_cast<const float*>(ms.pData);
                     const float bscale = f[0];     // BloomScaleAndThreshold.x.
                     const float bthreshold = f[1]; // BloomScaleAndThreshold.y, per-scene artist dial.
                     native_device_context->Unmap(gd.bloom_scale_ring[oldest].get(), 0);
                     if (bscale >= 0.f && bscale < 100.f) // Reject implausible readback data.
                     {
                        gd.bloom_scale_live = bscale;
                     }
                     if (bthreshold > 0.f && bthreshold < 100.f)
                     {
                        gd.bloom_threshold_live = bthreshold;
                     }
                  }
               }
               gd.bloom_scale_ring_wr = (gd.bloom_scale_ring_wr + 1) % 3; // Map failure skips only this update.
            }
         }
      }
   }

   // Core uploads a dirty settings cbuffer before the replaced pass runs, so the flag applies to the same draw.
   void TagVideoTarget(ID3D11DeviceContext* native_device_context, DeviceData& device_data, const ShaderHashesList<OneShaderPerPipeline>& original_shader_hashes)
   {
      if (original_shader_hashes.Contains(kVideoBinkHash, reshade::api::shader_stage::pixel))
      {
         ComPtr<ID3D11RenderTargetView> rtv;
         native_device_context->OMGetRenderTargets(1, rtv.put(), nullptr);
         ComPtr<ID3D11Resource> rt_res;
         if (rtv)
            rtv->GetResource(rt_res.put());
         if (rt_res)
         {
            const uint64_t rt_handle = reinterpret_cast<uint64_t>(rt_res.get());
            bool on_swapchain;
            {
               // Swapchain recreation mutates back_buffers under unique_lock; draw hooks must take a shared lock.
               std::shared_lock lock(device_data.mutex);
               on_swapchain = device_data.back_buffers.contains(rt_handle);
            }
            auto& gs = cb_luma_global_settings.GameSettings;
            const float flag = on_swapchain ? 1.f : 0.f;
            if (gs.VideoOnSwapchain != flag)
            {
               gs.VideoOnSwapchain = flag;
               device_data.cb_luma_global_settings_dirty = true;
            }
         }
      }
   }

   // Stage 1 supplies motion-blur depth for SMAA and the scene/bloom slots for fp16 bloom injection.
   void InjectBloomAndDepth(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, DeviceData& device_data, MassEffectGameDeviceData& gd, const TonemapPermDesc* perm)
   {
      if (perm == nullptr)
         return;

#if DEVELOPMENT
      gd.stage1_perm = perm; // Recorded before the SMAA and bloom gates, so the readout reflects every stage-1 draw.
      ++gd.stage1_draws;
#endif

      if (g_smaa_enable)
      {
         if (perm->scene_slot != 0)
         {
            // Only motion-blur permutations bind depth at t0; non-MB t0 is scene color.
            ComPtr<ID3D11ShaderResourceView> srv_d;
            native_device_context->PSGetShaderResources(0, 1, srv_d.put());
            if (srv_d)
               gd.srv_depth = srv_d;
         }
         else
         {
            gd.srv_depth.reset(); // Disable predication instead of reusing stale depth.
         }
      }

      // Build fp16 bloom from the tonemap's linear scene and rebind its native bloom slot.
      const bool bloom_ready =
         AllShadersReady(device_data.native_vertex_shaders, {kNameBloomVS}) &&
         AllShadersReady(device_data.native_pixel_shaders, {kNameBloomPrefilterPS, kNameBloomDownsamplePS, kNameBloomUpsamplePS});
      if (g_bloom_enable && bloom_ready)
      {
         ComPtr<ID3D11ShaderResourceView> srv_scene;
         native_device_context->PSGetShaderResources(perm->scene_slot, 1, srv_scene.put());
         if (srv_scene)
         {
            // Bloom prefilter reads GameSettings.BloomThreshold from b13; DrawBloom binds only b11.
            if (luma_settings_cbuffer_index < D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT)
            {
               ID3D11Buffer* settings_cb = device_data.luma_global_settings.get();
               native_device_context->PSSetConstantBuffers(luma_settings_cbuffer_index, 1, &settings_cb);
            }
            ComPtr<ID3D11ShaderResourceView> srv_bloom;
            DrawBloom(native_device, native_device_context, device_data, srv_scene.get(), (int)kBloomSigmas.size(), kBloomSigmas.data(), srv_bloom.put());
            if (srv_bloom)
            {
               ID3D11ShaderResourceView* p = srv_bloom.get();
               native_device_context->PSSetShaderResources(perm->bloom_slot, 1, &p);
            }
         }
      }
   }

   // Take over HBAO+ only when every XeGTAO shader and resource is ready at the first dispatch, otherwise the
   // whole native deinterleave -> horizon -> blur -> apply chain stays active. A returned value is terminal for
   // the callback; nullopt means XeGTAO is off or no AO hash matched, and the caller continues.
   std::optional<DrawOrDispatchOverrideType> RunXeGTAO(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, DeviceData& device_data, MassEffectGameDeviceData& gd, const ShaderHashesList<OneShaderPerPipeline>& original_shader_hashes)
   {
      if (g_gtao_enable)
      {
         // Deinterleave: capture half-resolution R24 depth, prepare all scratch resources, then skip native work.
         if (original_shader_hashes.Contains(kAODeinterleaveHash, reshade::api::shader_stage::compute))
         {
            const bool gtao_shaders_ready =
               AllShadersReady(device_data.native_compute_shaders, {kNameGTAOPrefilterCS, kNameGTAOMainPassCS, kNameGTAODenoise1CS, kNameGTAODenoise2CS});
            if (!gtao_shaders_ready)
               return DrawOrDispatchOverrideType::None;

            ComPtr<ID3D11ShaderResourceView> srv_d;
            native_device_context->CSGetShaderResources(0, 1, srv_d.put());
            if (!srv_d)
               return DrawOrDispatchOverrideType::None;
            uint4 dinfo{};
            DXGI_FORMAT dfmt = DXGI_FORMAT_UNKNOWN;
            GetResourceInfo(srv_d.get(), dinfo, dfmt);
            if (dinfo.x == 0 || dinfo.y == 0)
               return DrawOrDispatchOverrideType::None;
            // Native cb0 stays content-dimensioned, so pixel/UV mapping holds even when the allocation is larger.
            uint32_t w = dinfo.x, h = dinfo.y;

            if (gd.gtao_w != w || gd.gtao_h != h || !gd.tex_gtao_depth_mips || !gd.tex_gtao_working[1])
            {
               ReleaseGTAOScratch(gd);

               D3D11_TEXTURE2D_DESC td = {};
               td.Width = w;
               td.Height = h;
               td.MipLevels = 5; // XE_GTAO_DEPTH_MIP_LEVELS.
               td.ArraySize = 1;
               td.Format = DXGI_FORMAT_R32_FLOAT;
               td.SampleDesc.Count = 1;
               td.Usage = D3D11_USAGE_DEFAULT;
               td.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
               bool ok = SUCCEEDED(native_device->CreateTexture2D(&td, nullptr, gd.tex_gtao_depth_mips.put()));
               D3D11_UNORDERED_ACCESS_VIEW_DESC ud = {};
               ud.Format = td.Format;
               ud.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
               for (int i = 0; ok && i < 5; i++)
               {
                  ud.Texture2D.MipSlice = i;
                  ok = SUCCEEDED(native_device->CreateUnorderedAccessView(gd.tex_gtao_depth_mips.get(), &ud, gd.gtao_depth_mip_uavs[i].put()));
               }
               ok = ok && SUCCEEDED(native_device->CreateShaderResourceView(gd.tex_gtao_depth_mips.get(), nullptr, gd.srv_gtao_depth_mips.put()));

               td.MipLevels = 1;
               td.Format = DXGI_FORMAT_R8G8_UNORM;
               for (int i = 0; ok && i < 2; i++)
               {
                  ok = ok && SUCCEEDED(native_device->CreateTexture2D(&td, nullptr, gd.tex_gtao_working[i].put()));
                  ok = ok && SUCCEEDED(native_device->CreateUnorderedAccessView(gd.tex_gtao_working[i].get(), nullptr, gd.uav_gtao_working[i].put()));
                  ok = ok && SUCCEEDED(native_device->CreateShaderResourceView(gd.tex_gtao_working[i].get(), nullptr, gd.srv_gtao_working[i].put()));
               }
               if (!ok)
               {
                  ReleaseGTAOScratch(gd);                  // All or nothing; retry clean next frame.
                  return DrawOrDispatchOverrideType::None; // Leaves the native chain active.
               }
               gd.gtao_w = w;
               gd.gtao_h = h;
            }

#if DEVELOPMENT || TEST
            const float dbg = (float)g_gtao_debug_view;
#else
            const float dbg = 0.f;
#endif
            // The last is the noise index (see "IsGTAOTemporal")
            const float knobs[8] = {g_gtao_final_value_power, g_gtao_depth_scale, g_gtao_radius_override, dbg, IsGTAOTemporal(device_data) ? float(cb_luma_global_settings.FrameIndex % 64) : 0.f};
            if (!PatchedDraws::WriteDynamicConstants(native_device, native_device_context, std::addressof(gd.cb_gtao), knobs, sizeof(knobs)))
               return DrawOrDispatchOverrideType::None;

            gd.srv_gtao_depth = srv_d;
            gd.gtao_active_this_frame = true;
            return DrawOrDispatchOverrideType::Replaced;
         }

         // Horizon march: capture packed view normals and skip native work only after a successful takeover.
         if (original_shader_hashes.Contains(kAOHorizonHash, reshade::api::shader_stage::compute))
         {
            if (!gd.gtao_active_this_frame)
               return DrawOrDispatchOverrideType::None;
            ComPtr<ID3D11ShaderResourceView> srv_n;
            native_device_context->CSGetShaderResources(0, 1, srv_n.put());
            if (srv_n)
               gd.srv_gtao_normals = srv_n;

            return DrawOrDispatchOverrideType::Replaced;
         }

         // Blur: run all XeGTAO passes and write the game's final R8_UNORM u0; native apply performs composition.
         if (original_shader_hashes.Contains(kAOBlurHash, reshade::api::shader_stage::compute))
         {
            if (!gd.gtao_active_this_frame)
               return DrawOrDispatchOverrideType::None;

            ComPtr<ID3D11UnorderedAccessView> uav_final;
            native_device_context->CSGetUnorderedAccessViews(0, 1, uav_final.put());
            if (!uav_final)
               return DrawOrDispatchOverrideType::Replaced; // Earlier native stages were already skipped.
            if (!gd.srv_gtao_depth || !gd.srv_gtao_normals)
            {
               // Missing fixed-order inputs: write neutral AO instead of applying stale data.
               const FLOAT ones[4] = {1.f, 1.f, 1.f, 1.f};
               native_device_context->ClearUnorderedAccessViewFloat(uav_final.get(), ones);
               return DrawOrDispatchOverrideType::Replaced;
            }

            // Dispatch dimensions follow the captured depth allocation, not the DynamicScale content size; native
            // apply ignores the region outside the content.
            const uint32_t w = gd.gtao_w, h = gd.gtao_h;
            DrawStateStack<DrawStateStackType::Compute> st;
            st.Cache(native_device_context, device_data.uav_max_count);

            ID3D11Buffer* kcb = gd.cb_gtao.get();
            native_device_context->CSSetConstantBuffers(11, 1, &kcb);
            ID3D11SamplerState* smp = device_data.sampler_state_point.get();
            native_device_context->CSSetSamplers(0, 1, &smp);
            // XeGTAO deliberately inherits native b0 ($Globals/ProjInfo) and b2 (MinZ_MaxZRatioCS): the immediate
            // context runs a fixed contiguous AO chain with no ClearState, so the horizon-pass bindings persist.

            // Each pass binds its destination UAVs before its source SRVs (the previous pass's SRVs cleared first): D3D11 otherwise keeps
            // the previous UAV hazard and silently nulls the conflicting SRV
            const auto dispatch = [&](uint32_t shader_name, std::initializer_list<ID3D11ShaderResourceView*> srvs, std::initializer_list<ID3D11UnorderedAccessView*> uavs, UINT groups_x, UINT groups_y)
            {
               static constexpr std::array<ID3D11ShaderResourceView*, 2> srv_nulls = {};
               static constexpr std::array<ID3D11UnorderedAccessView*, 5> uav_nulls = {};
               native_device_context->CSSetShaderResources(0, UINT(srv_nulls.size()), srv_nulls.data());
               native_device_context->CSSetUnorderedAccessViews(0, UINT(uavs.size()), uavs.begin(), nullptr);
               native_device_context->CSSetShaderResources(0, UINT(srvs.size()), srvs.begin());
               native_device_context->CSSetShader(FindShader(device_data.native_compute_shaders, shader_name), nullptr, 0);
               native_device_context->Dispatch(groups_x, groups_y, 1);
               native_device_context->CSSetUnorderedAccessViews(0, UINT(uavs.size()), uav_nulls.data(), nullptr);
            };
            // Prefilter game depth into the R32F mip pyramid; each thread covers 2x2 pixels.
            dispatch(kNameGTAOPrefilterCS, {gd.srv_gtao_depth.get()}, {gd.gtao_depth_mip_uavs[0].get(), gd.gtao_depth_mip_uavs[1].get(), gd.gtao_depth_mip_uavs[2].get(), gd.gtao_depth_mip_uavs[3].get(), gd.gtao_depth_mip_uavs[4].get()}, (w + 15) / 16, (h + 15) / 16);
            // Main pass writes AO and edges to working0.
            dispatch(kNameGTAOMainPassCS, {gd.srv_gtao_depth_mips.get(), gd.srv_gtao_normals.get()}, {gd.uav_gtao_working[0].get()}, (w + 7) / 8, (h + 7) / 8);
            // First denoiser writes working1, two horizontal pixels per thread. Skipped when temporal: the final one reads working0.
            const bool single_denoise = IsGTAOTemporal(device_data);
            if (!single_denoise)
               dispatch(kNameGTAODenoise1CS, {gd.srv_gtao_working[0].get()}, {gd.uav_gtao_working[1].get()}, (w + 15) / 16, (h + 7) / 8);
            // Final denoiser writes the game's R8_UNORM AO target.
            dispatch(kNameGTAODenoise2CS, {gd.srv_gtao_working[single_denoise ? 0 : 1].get()}, {uav_final.get()}, (w + 15) / 16, (h + 7) / 8);

            st.Restore(native_device_context);
            return DrawOrDispatchOverrideType::Replaced;
         }
      }

      return {};
   }

   // Replaces the in-place FXAA resolve (color at u0) with SMAA, or with RCAS alone after DLSS / FSR. Disabled SMAA leaves FXAA
   // native only without an upscaler.
   DrawOrDispatchOverrideType RunSMAAResolve(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, DeviceData& device_data, MassEffectGameDeviceData& gd, const ShaderHashesList<OneShaderPerPipeline>& original_shader_hashes, bool is_custom_pass)
   {
      const bool is_resolve_h = original_shader_hashes.Contains(kFXAAResolveHHash, reshade::api::shader_stage::compute);
      const bool is_resolve_v = original_shader_hashes.Contains(kFXAAResolveVHash, reshade::api::shader_stage::compute);
      // The resolve is the final scene-post step and arms HUD suppression for subsequent draws, with or without SMAA.
      if (is_resolve_h || is_resolve_v)
         gd.scene_post_done_this_frame = true;
      // After DLSS / FSR (which already antialiased the scene, FXAA would blur it) only RCAS runs, or nothing
      const bool smaa = !device_data.has_drawn_sr;
      if (!g_smaa_enable && smaa)
         return DrawOrDispatchOverrideType::None;
      if (!is_custom_pass && (is_resolve_h || is_resolve_v))
      {
         ComPtr<ID3D11UnorderedAccessView> uav_color;
         native_device_context->CSGetUnorderedAccessViews(0, 1, uav_color.put());
         if (!uav_color)
            return DrawOrDispatchOverrideType::None;
         ComPtr<ID3D11Resource> color_res;
         uav_color->GetResource(color_res.put());
         if (!color_res)
            return DrawOrDispatchOverrideType::None;
         const uint64_t color_handle = reinterpret_cast<uint64_t>(color_res.get());

         // Skip later resolves for an already processed resource; rerunning in-place FXAA would corrupt SMAA.
         if (gd.smaa_applied_handles.count(color_handle) != 0)
            return DrawOrDispatchOverrideType::Replaced;
         // Only horizontal resolve triggers SMAA; an unexpected vertical-only resolve remains native (skipped after DLSS / FSR).
         if (!is_resolve_h)
            return smaa ? DrawOrDispatchOverrideType::None : DrawOrDispatchOverrideType::Replaced;
         if (!smaa && g_rcas_sharpness <= 0.f)
         {
            gd.smaa_applied_handles.insert(color_handle);
            return DrawOrDispatchOverrideType::Replaced;
         }

         uint4 cinfo{};
         DXGI_FORMAT cfmt = DXGI_FORMAT_UNKNOWN;
         GetResourceInfo(color_res.get(), cinfo, cfmt);
         const uint32_t w = cinfo.x, h = cinfo.y;
         if (w == 0 || h == 0)
            return DrawOrDispatchOverrideType::None;
         // SMAA temporaries are RGBA16F; CopyResource silently fails across formats, so fall back to FXAA on mismatch.
         if (cfmt != DXGI_FORMAT_R16G16B16A16_FLOAT)
            return DrawOrDispatchOverrideType::None;

         // Predication requires current, same-sized depth; otherwise a null texture and scale 1 select plain ULTRA.
         bool depth_ok = false;
         if (smaa && gd.srv_depth)
         {
            uint4 dinfo{};
            DXGI_FORMAT dfmt = DXGI_FORMAT_UNKNOWN;
            GetResourceInfo(gd.srv_depth.get(), dinfo, dfmt);
            depth_ok = (dinfo.x == w && dinfo.y == h);
         }
         // Threshold and strength are inert without the depth-edge term.
         const float pred_scale = depth_ok ? kPredScale : 1.f;

         // Async loading and live reload may temporarily require native FXAA fallback.
         auto* linearize_cs = FindShader(device_data.native_compute_shaders, kNameSMAALinearizeCS);
         const bool smaa_ready = linearize_cs != nullptr &&
                                 AllShadersReady(device_data.native_pixel_shaders, {kNameSMAAEdgePS, kNameSMAAWeightPS, kNameSMAABlendPS}) &&
                                 AllShadersReady(device_data.native_vertex_shaders, {kNameSMAAEdgeVS, kNameSMAAWeightVS, kNameSMAABlendVS});
         if (smaa && !smaa_ready)
            return DrawOrDispatchOverrideType::None;

         // DrawSMAA rebuilds managed views only on swapchain init; drop them explicitly on resolution changes.
         if (smaa && (gd.smaa_core_w != w || gd.smaa_core_h != h))
         {
            auto& mr = device_data.managed_resources;
            mr.depth_stencil_views[CompileTimeStringHash("smaa_dsv")].reset();
            mr.render_target_views[CompileTimeStringHash("smaa_edge_detection")].reset();
            mr.render_target_views[CompileTimeStringHash("smaa_blending_weight_calculation")].reset();
            gd.smaa_core_w = w;
            gd.smaa_core_h = h;
         }

         const float metrics[8] = {1.f / (float)w, 1.f / (float)h, (float)w, (float)h, pred_scale, kPredThreshold, kPredStrength, 0.f};
         if (smaa && !PatchedDraws::WriteDynamicConstants(native_device, native_device_context, std::addressof(gd.cb_smaa_metrics), metrics, sizeof(metrics)))
            return DrawOrDispatchOverrideType::None;

         // The fp16 SMAA output is both a render target and an SRV for the optional sharpen pass (after DLSS / FSR: RCAS's input copy).
         if (!EnsureRGBA16FTarget(native_device, w, h, D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE, &gd.smaa_out))
            return smaa ? DrawOrDispatchOverrideType::None : DrawOrDispatchOverrideType::Replaced;

         // The gamma snapshot is only ever read; its linear-light decode feeds the neighborhood blend.
         if (smaa && (!EnsureRGBA16FTarget(native_device, w, h, D3D11_BIND_SHADER_RESOURCE, &gd.smaa_input) ||
                        !EnsureRGBA16FTarget(native_device, w, h, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, &gd.smaa_input_linear)))
            return DrawOrDispatchOverrideType::None;

         native_device_context->CopyResource(smaa ? gd.smaa_input.tex.get() : gd.smaa_out.tex.get(), color_res.get());
         if (smaa)
         {
            DrawStateStack<DrawStateStackType::Compute> linearize_state;
            linearize_state.Cache(native_device_context, device_data.uav_max_count);
            ID3D11ShaderResourceView* lin_srv = gd.smaa_input.srv.get();
            ID3D11UnorderedAccessView* lin_uav = gd.smaa_input_linear.uav.get();
            native_device_context->CSSetUnorderedAccessViews(0, 1, &lin_uav, nullptr);
            native_device_context->CSSetShaderResources(0, 1, &lin_srv);
            native_device_context->CSSetShader(linearize_cs, nullptr, 0);
            native_device_context->Dispatch((w + 7) / 8, (h + 7) / 8, 1);
            linearize_state.Restore(native_device_context);
         }

         // DrawSMAA restores shaders, resources, and targets, but not cbuffer slots; save VS/PS b1 explicitly.
         ComPtr<ID3D11Buffer> vs_cb1_orig, ps_cb1_orig;
         native_device_context->VSGetConstantBuffers(1, 1, vs_cb1_orig.put());
         native_device_context->PSGetConstantBuffers(1, 1, ps_cb1_orig.put());
         if (smaa)
         {
            ID3D11Buffer* mcb = gd.cb_smaa_metrics.get();
            native_device_context->VSSetConstantBuffers(1, 1, &mcb);
            native_device_context->PSSetConstantBuffers(1, 1, &mcb);

            DrawSMAA(native_device, native_device_context, device_data,
               gd.smaa_out.rtv.get(), gd.smaa_input_linear.srv.get() /*blend color (linear)*/, gd.smaa_input.srv.get() /*edge color (gamma)*/,
               depth_ok ? gd.srv_depth.get() : nullptr /*predication*/);
         }

         // Apply optional RCAS, otherwise copy SMAA directly so the cancelled resolve always produces output.
         auto* const sharpen_vs = FindShader(device_data.native_vertex_shaders, kNameCopyVS);
         auto* const sharpen_ps = FindShader(device_data.native_pixel_shaders, kNameSharpenPS);
         const float sp[4] = {(float)w, (float)h, g_rcas_sharpness, 0.f};
         // The RCAS output is written by the sharpen pass and then copied out, so it needs no SRV.
         const bool do_sharpen = g_rcas_sharpness > 0.f && sharpen_vs && sharpen_ps && PatchedDraws::WriteDynamicConstants(native_device, native_device_context, std::addressof(gd.cb_sharpen), sp, sizeof(sp)) &&
                                 EnsureRGBA16FTarget(native_device, w, h, D3D11_BIND_RENDER_TARGET, &gd.rcas_out);

         if (do_sharpen)
         {
            // DrawCustomPixelShader does not restore state; FullGraphics also prevents RCAS b0 leaking into HUD draws.
            DrawStateStack<DrawStateStackType::FullGraphics> sharpen_state;
            sharpen_state.Cache(native_device_context, device_data.uav_max_count);

            ID3D11Buffer* scb = gd.cb_sharpen.get();
            native_device_context->PSSetConstantBuffers(0, 1, &scb);
            DrawCustomPixelShader(native_device_context, device_data.default_depth_stencil_state.get(), device_data.default_blend_state.get(), nullptr,
               sharpen_vs, sharpen_ps, gd.smaa_out.srv.get(), gd.rcas_out.rtv.get(), w, h, false);

            sharpen_state.Restore(native_device_context);

            native_device_context->CopyResource(color_res.get(), gd.rcas_out.tex.get());
         }
         else
         {
            native_device_context->CopyResource(color_res.get(), gd.smaa_out.tex.get());
         }

         // Restore native VS/PS b1; the linearize dispatch already restored its compute state.
         ID3D11Buffer* vcb = vs_cb1_orig.get();
         ID3D11Buffer* pcb = ps_cb1_orig.get();
         native_device_context->VSSetConstantBuffers(1, 1, &vcb);
         native_device_context->PSSetConstantBuffers(1, 1, &pcb);

         gd.smaa_applied_handles.insert(color_handle);
         device_data.has_drawn_main_post_processing = true;
         return DrawOrDispatchOverrideType::Replaced;
      }

      return DrawOrDispatchOverrideType::None;
   }

   // Runs the DLSS / FSR motion vector and jitter draws, captures inputs for SMAA, bloom, XeGTAO and Bink, replaces the FXAA and
   // HBAO+ dispatches, and applies Hide UI; shader replacement itself stays hash-driven by Core.
   DrawOrDispatchOverrideType OnDrawOrDispatch(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, CommandListData& cmd_list_data, DeviceData& device_data, reshade::api::shader_stage stages, const ShaderHashesList<OneShaderPerPipeline>& original_shader_hashes, bool is_custom_pass, bool& updated_cbuffers, std::function<void()>* original_draw_dispatch_func) override
   {
      auto& gd = GetGameDeviceData(device_data);

      const bool is_immediate = cmd_list_data.is_primary; // Cached by Core; avoids a per-draw virtual query.
      // Do not reject custom passes globally: the hash-replaced stage-1 tonemap still supplies SMAA depth and the
      // bloom slot. Individual native-only branches apply !is_custom_pass where required.
      if (!is_immediate)
         return DrawOrDispatchOverrideType::None;

      const bool compute = (stages & reshade::api::shader_stage::compute) != 0;
      const TonemapPermDesc* const stage1_perm = compute ? nullptr : FindTonemapPerm(original_shader_hashes);
      const bool stage1 = stage1_perm != nullptr;
#if DEVELOPMENT
      // Scene probe: the passes that read the scene after its first motion vector draw, before its end ("open") and after it
      // ("done"), logged once per shader, phase and slot (the evidence for the post passes that end the scene, and that the upscaled
      // scene reaches every reader). Stale bindings show up too.
      if (gd.mv_active && gd.mv_scene_color && (gd.mv_scene_open || gd.mv_scene_done))
      {
         const uint32_t hash = compute ? uint32_t(original_shader_hashes.compute_shaders[0]) : uint32_t(original_shader_hashes.pixel_shaders[0]);
         com_ptr<ID3D11ShaderResourceView> srvs[16];
         if (compute)
            native_device_context->CSGetShaderResources(0, UINT(std::size(srvs)), &srvs[0]);
         else
            native_device_context->PSGetShaderResources(0, UINT(std::size(srvs)), &srvs[0]);
         for (UINT slot = 0; slot < std::size(srvs); slot++)
         {
            com_ptr<ID3D11Resource> resource;
            if (srvs[slot])
               srvs[slot]->GetResource(&resource);
            const bool scene = resource && (resource == gd.mv_scene_color || uint64_t(resource.get()) == gd.mv_scene_color_copy);
            const bool upscaled = resource && device_data.sr_output_color && resource.get() == static_cast<ID3D11Resource*>(device_data.sr_output_color.get());
            if (!scene && !upscaled)
               continue;
            std::string line = std::format("[MELE Probe] {} {} 0x{:08X} reads the {} at t{}", gd.mv_scene_open ? "open" : "done", compute ? "CS" : "PS", hash, scene ? "scene" : "upscaled scene", slot);
            if (gd.probe_logged.insert(line).second)
               reshade::log::message(reshade::log::level::info, (line + std::format(" (frame {})", cb_luma_global_settings.FrameIndex)).c_str());
         }
      }
#endif

      // DLSS / FSR: the scene's draws with motion vectors or jitter (see "DrawWithMotionVectors"), and the upscaler at its first post pass
      if (gd.mv_active)
      {
         if (stage1 || original_shader_hashes.Contains(kBloomBrightPassHash, reshade::api::shader_stage::pixel) || original_shader_hashes.Contains(kDOFDownsampleHash, reshade::api::shader_stage::pixel))
         {
            if (gd.mv_scene_open)
            {
#if DEVELOPMENT
               gd.mv_stats.ended_by = uint32_t(original_shader_hashes.pixel_shaders[0]);
#endif
               EndScene(native_device, native_device_context, device_data);
            }
         }
         else if (!gd.mv_scene_done && !is_custom_pass && original_draw_dispatch_func && *original_draw_dispatch_func && (stages & reshade::api::shader_stage::vertex) == reshade::api::shader_stage::vertex)
         {
#if DEVELOPMENT
            const PerfHookTimer timer{gd.perf_hook_ns}; // The draw's own submission included
#endif
            com_ptr<ID3D11RenderTargetView> rtvs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
            com_ptr<ID3D11DepthStencilView> dsv;
            native_device_context->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, &rtvs[0], &dsv);
            const bool mirrored = dsv && IsMirroredView(native_device_context, &gd);
#if DEVELOPMENT
            gd.mv_stats.mirrored += mirrored;
#endif
            const bool scene_view = dsv && !mirrored;
            if (gd.mv_scene_open && scene_view && IsNewView(device_data, &gd, dsv.get()))
            {
               // The earlier view's motion vectors (cleared again at the next motion vector draw), camera and objects go. Before the
               // frame's first motion vector draw they are still last frame's (see "DrawWithMotionVectors"): kept.
               gd.mv_scene_open = false;
               gd.mv_view_restarted = true;
               if (!gd.mv_frame_ended)
               {
                  gd.mv_camera = nullptr;
                  for (auto& entry : gd.mv_objects)
                     entry.second.clear();
               }
#if DEVELOPMENT
               gd.mv_stats.view_restarts++;
#endif
            }
            if (!gd.mv_scene_open && scene_view)
            {
               OpenScene(native_device, native_device_context, cmd_list_data, device_data, original_shader_hashes.vertex_shaders[0], dsv.get());
#if DEVELOPMENT
               if (gd.mv_scene_open && gd.view_camera && gd.view_camera->size() >= kPreViewTranslationOffset + 12)
               {
                  D3D11_VIEWPORT viewport = {};
                  UINT viewport_count = 1;
                  native_device_context->RSGetViewports(&viewport_count, &viewport);
                  gd.views.push_back({float(ViewNearPlane(*gd.view_camera)), float(ViewDeterminant(*gd.view_camera)), GetPreViewTranslation(*gd.view_camera)[2], viewport.Width, viewport.Height});
               }
#endif
            }
#if DEVELOPMENT
            if (gd.mv_scene_open && !mirrored && !gd.views.empty())
               gd.views.back().draws++;
#endif
            if (!mirrored && (DrawWithMotionVectors(native_device, native_device_context, cmd_list_data, device_data, original_shader_hashes, *original_draw_dispatch_func, rtvs, dsv.get()) ||
                                DrawWithJitter(native_device, native_device_context, cmd_list_data, device_data, original_shader_hashes, *original_draw_dispatch_func, dsv.get())))
               return DrawOrDispatchOverrideType::Replaced;
         }
      }
      // DLSS / FSR: the post passes up to stage 1 (the scene's last reader) read the upscaled scene; before the bloom capture below,
      // which takes stage 1's scene
      if (device_data.has_drawn_sr && gd.sr_input && !gd.sr_rebind_done)
      {
         RebindUpscaledScene(native_device_context, gd, compute);
         gd.sr_rebind_done = stage1;
      }
      // Every draw without a patched shader: the game's own, if the last patched draw's are still bound
      PatchedDraws::RestoreGameShader(native_device_context, &gd.mv_bound_vertex_shader);
      PatchedDraws::RestoreGameShader(native_device_context, &gd.mv_bound_pixel_shader);

      // Core uploads LumaData after this callback for replaced shaders, so stage 2 and any later direct Bink draw
      // receive the updated encoding state in the same frame.
      if (original_shader_hashes.Contains(kOutputStage2Hash, reshade::api::shader_stage::pixel))
         gd.stage2_seen_this_frame = true;

      // HUD draws occur after the FXAA resolve. Suppress only plain game draws in that window so stage 1, stage 2,
      // movies without a scene resolve, and pre-scene menus remain intact.
      if (g_hide_ui && !is_custom_pass && gd.scene_post_done_this_frame)
         return DrawOrDispatchOverrideType::Replaced;

      CaptureBloomScale(native_device, native_device_context, gd, original_shader_hashes);

      TagVideoTarget(native_device_context, device_data, original_shader_hashes);

      InjectBloomAndDepth(native_device, native_device_context, device_data, gd, stage1_perm);

      if (const auto gtao_result = RunXeGTAO(native_device, native_device_context, device_data, gd, original_shader_hashes))
         return *gtao_result;

      return RunSMAAResolve(native_device, native_device_context, device_data, gd, original_shader_hashes, is_custom_pass);
   }

   void UpdateLumaInstanceDataCB(CB::LumaInstanceDataPadded& data, CommandListData&, DeviceData& device_data) override
   {
      // Read by the Bink replacement to decide whether a direct swapchain draw may emit linear scRGB.
      const auto& gd = GetGameDeviceData(device_data);
      data.GameData.SwapchainGammaEncoded = gd.stage2_seen_this_frame ? 0.f : 1.f;
   }

   void OnPresent(ID3D11Device* native_device, DeviceData& device_data) override
   {
      auto& gd = GetGameDeviceData(device_data);

      // DLSS / FSR: the upscaler's history restarts after any frame it didn't draw (menus, loading, just picked); the selection and the
      // motion vector state are fixed here for the next frame (see "IsSRActive")
      device_data.force_reset_sr = !device_data.has_drawn_sr;
      device_data.has_drawn_sr = false;
      gd.sr_active = device_data.sr_type != SR::Type::None && !device_data.sr_suppressed;
      gd.mv_active = IsSRActive(device_data) || g_mv_enable;
      gd.sr_input = nullptr;
      gd.sr_rebind_done = false;
      // A scene no post pass ended ends here: its jitter must not reach the next frame's draws before the prepass
      gd.mv_scene_open = false;
      gd.mv_scene_done = false;
      gd.mv_frame_ended = true;
      gd.mv_view_restarted = false;
      gd.mv_fill_pending = false;
      gd.view_dsv = nullptr;
      if (!custom_texture_mip_lod_bias_offset)
      {
         const std::unique_lock lock(s_mutex_samplers);
         // -1 at native resolution (Core biases the anisotropic samplers, all of the game's with the AF16x upgrade)
         device_data.texture_mip_lod_bias_offset = IsSRActive(device_data) ? SR::GetMipLODBias(device_data.output_resolution.y, device_data.output_resolution.y) : 0.f;
      }
#if DEVELOPMENT
      // "Performance Test": closes this frame's timestamp set, reads back the finished ones (a log line every 120 frames, the first 60
      // after a change of mode or AA settings skipped), opens the next frame's
      com_ptr<ID3D11DeviceContext> native_device_context;
      native_device->GetImmediateContext(&native_device_context);
      if (auto* const queries = std::exchange(gd.perf_frame_queries, nullptr))
      {
         native_device_context->End(queries->frame_end.get());
         native_device_context->End(queries->disjoint.get());
         queries->pending = true;
      }
      if (g_perf_test != 0)
      {
         const auto now = std::chrono::steady_clock::now();
         const uint32_t settings = uint32_t(g_perf_test) | (uint32_t(int(device_data.sr_type) + 1) << 5) | (dlss_render_preset << 8) | (uint32_t(g_smaa_enable) << 16) | (uint32_t(g_gtao_enable) << 17) | (uint32_t(g_perf_hook_timers) << 18) | (uint32_t(g_bloom_enable) << 19) | (uint32_t(g_mv_enable) << 20) | (uint32_t(g_rcas_sharpness > 0.f) << 21) | (uint32_t(g_gtao_temporal) << 22);
         // Also after a pause (the game stops presenting while unfocused): the upscaler history and the clocks restart
         if (std::exchange(gd.perf_settings, settings) != settings || now - gd.perf_last_present > std::chrono::milliseconds(250))
            gd.perf_settle_frames = 60;
         const bool measuring = gd.perf_settle_frames <= 0;
         auto& stats = gd.perf_stats;
         for (auto& queries : gd.perf_queries)
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
            UINT64 frame_start, frame_end, scene_start, fill_start, scene_end, sr_end;
            if (!read(queries.frame_start, &frame_start) || !read(queries.frame_end, &frame_end))
               continue;
            add(frame_start, frame_end, &stats.frame_ms, &stats.frame_max_ms, &stats.samples);
            if (queries.scene && read(queries.scene_start, &scene_start) && read(queries.scene_end, &scene_end))
            {
               add(scene_start, scene_end, &stats.scene_ms, &stats.scene_max_ms, &stats.scene_samples);
               if (queries.fill && read(queries.fill_start, &fill_start))
                  add(fill_start, scene_end, &stats.fill_ms, &stats.unused_max_ms, &stats.fill_samples);
               if (queries.sr && read(queries.sr_end, &sr_end))
                  add(scene_end, sr_end, &stats.sr_ms, &stats.sr_max_ms, &stats.sr_samples);
            }
         }
         if (!measuring)
         {
            gd.perf_settle_frames--;
            stats = {};
            gd.perf_hook_ns = 0;
         }
         else
         {
            stats.cpu_frame_ms += std::chrono::duration<double, std::milli>(now - gd.perf_last_present).count();
            if (++stats.frames >= 120)
            {
               const auto average = [](double total, uint32_t samples)
               { return samples != 0 ? total / samples : 0.0; };
               std::string aa = g_smaa_enable ? "SMAA" : "None";
               if (IsSRActive(device_data))
                  aa = device_data.sr_type == SR::Type::FSR ? "FSR" : (dlss_render_preset != 0 ? std::format("DLSS_{}", char('A' + dlss_render_preset - 1)) : "DLSS_Default");
               const std::array<double, 5> window = {average(stats.frame_ms, stats.samples), average(stats.scene_ms, stats.scene_samples), average(stats.sr_ms, stats.sr_samples), double(gd.perf_hook_ns.exchange(0)) / 1e6 / stats.frames, average(stats.fill_ms, stats.fill_samples)};
               const char* const game_name = GameName(g_me_game);
               reshade::log::message(reshade::log::level::info, std::format("[MELE Perf] game={} mode=\"{}\" aa={} hook_timers={} mv_enable={} gtao={} gtao_temporal={} bloom={} rcas={:.2f} output={}x{} gpu frame avg/max={:.3f}/{:.3f} ms scene avg/max={:.3f}/{:.3f} ms ({}) sr avg/max={:.3f}/{:.3f} ms ({}) cpu frame avg={:.3f} ms cpu hooks={:.3f} ms/frame fill={:.3f} ms ({}) samples={}/{} disjoint={}", game_name, perf_test_modes[g_perf_test].name, aa, g_perf_hook_timers, g_mv_enable, g_gtao_enable, g_gtao_temporal, g_bloom_enable, g_rcas_sharpness, uint32_t(device_data.output_resolution.x), uint32_t(device_data.output_resolution.y), window[0], stats.frame_max_ms, window[1], stats.scene_max_ms, stats.scene_samples, window[2], stats.sr_max_ms, stats.sr_samples, stats.cpu_frame_ms / stats.frames, window[3], window[4], stats.fill_samples, stats.samples, stats.frames, stats.disjoint).c_str());
               stats = {};

               if (g_perf_sweep)
               {
                  gd.perf_sweep_results[g_perf_test].push_back(window);
                  if (++gd.perf_sweep_windows_done >= perf_sweep_windows)
                  {
                     gd.perf_sweep_windows_done = 0;
                     const int step = ++gd.perf_sweep_step;
                     if (step < perf_sweep_rounds * int(std::size(perf_sweep_modes)))
                     {
                        ApplyPerfTestMode(device_data, perf_sweep_modes[step % std::size(perf_sweep_modes)]);
                     }
                     else
                     {
                        // Per mode: the median window (and the frame's range), and the frame against the last mode's (SMAA Off)
                        const auto median = [&](int mode, size_t column)
                        {
                           std::vector<double> values;
                           for (const auto& result : gd.perf_sweep_results[mode])
                              values.push_back(result[column]);
                           std::nth_element(values.begin(), values.begin() + values.size() / 2, values.end());
                           return values.empty() ? 0.0 : values[values.size() / 2];
                        };
                        const double baseline = median(perf_sweep_modes[std::size(perf_sweep_modes) - 1], 0);
                        for (const int mode : perf_sweep_modes)
                        {
                           const auto& results = gd.perf_sweep_results[mode];
                           const auto [min_frame, max_frame] = std::minmax_element(results.begin(), results.end(), [](const auto& a, const auto& b)
                              { return a[0] < b[0]; });
                           reshade::log::message(reshade::log::level::info, std::format("[MELE Perf] sweep game={} mode=\"{}\" hook_timers={} windows={} gpu frame median={:.3f} ms (min/max {:.3f}/{:.3f}, {:+.3f} vs \"{}\") scene median={:.3f} ms sr median={:.3f} ms cpu hooks median={:.3f} ms/frame fill={:.3f} ms (medians)", game_name, perf_test_modes[mode].name, g_perf_hook_timers, results.size(), median(mode, 0), results.empty() ? 0.0 : (*min_frame)[0], results.empty() ? 0.0 : (*max_frame)[0], median(mode, 0) - baseline, perf_test_modes[perf_sweep_modes[std::size(perf_sweep_modes) - 1]].name, median(mode, 1), median(mode, 2), median(mode, 3), median(mode, 4)).c_str());
                        }
                        g_perf_sweep = false;
                        ApplyPerfTestMode(device_data, 0);
                     }
                  }
               }
            }
         }
         gd.perf_last_present = now;

         auto& queries = gd.perf_queries[gd.perf_query_index];
         if (!queries.pending)
         {
            if (!queries.disjoint)
            {
               const D3D11_QUERY_DESC disjoint_desc = {D3D11_QUERY_TIMESTAMP_DISJOINT}, timestamp_desc = {D3D11_QUERY_TIMESTAMP};
               native_device->CreateQuery(&disjoint_desc, &queries.disjoint);
               for (auto* const query : {&queries.frame_start, &queries.scene_start, &queries.fill_start, &queries.scene_end, &queries.sr_end, &queries.frame_end})
                  native_device->CreateQuery(&timestamp_desc, &*query);
            }
            if (queries.disjoint && queries.frame_start && queries.scene_start && queries.fill_start && queries.scene_end && queries.sr_end && queries.frame_end)
            {
               native_device_context->Begin(queries.disjoint.get());
               native_device_context->End(queries.frame_start.get());
               queries.scene_started = queries.fill = queries.scene = queries.sr = false;
               gd.perf_frame_queries = &queries;
               gd.perf_query_index = (gd.perf_query_index + 1) % gd.perf_queries.size();
            }
         }
      }
      gd.mv_last_stats = std::exchange(gd.mv_stats, {});
      gd.last_views.swap(gd.views);
      gd.views.clear();
      if (gd.mv_active && cb_luma_global_settings.FrameIndex % 300 == 0)
      {
         std::string line = std::format("[MELE MV] frame {} views:", cb_luma_global_settings.FrameIndex);
         for (const auto& view : gd.last_views)
            line += std::format(" [near {:.2f} det {:.4f} pvt.z {:.1f} viewport {:.0f}x{:.0f} draws {} mv {}]", view.near_plane, view.determinant, view.view_translation_z, view.viewport_width, view.viewport_height, view.draws, view.motion_vector_draws);
         reshade::log::message(reshade::log::level::info, line.c_str());
      }
      // The DEV panel's counts in ReShade.log every 300 frames while motion vectors run
      if (const auto& stats = gd.mv_last_stats; gd.mv_active && cb_luma_global_settings.FrameIndex % 300 == 0)
         reshade::log::message(reshade::log::level::info, std::format("[MELE MV] frame {}: {} mv ({} matched, {} camera only, {} other camera, {} uncopied), {} jitter, {} mirrored, {} view restarts, {} updates, sr {} ({}), ended by 0x{:08X}, refused {}/{}/{}/{}/{}/{}/{}/{}", cb_luma_global_settings.FrameIndex, stats.motion_vector_draws, stats.matched, stats.camera_only, stats.other_camera, stats.uncopied, stats.jitter_draws, stats.mirrored, stats.view_restarts, stats.updates, stats.sr_draws, int(device_data.sr_type), stats.ended_by, stats.rejected[0], stats.rejected[1], stats.rejected[2], stats.rejected[3], stats.rejected[4], stats.rejected[5], stats.rejected[6], stats.rejected[7]).c_str());
      // "MV Dump": raw rows of the staging copies (motion vectors R16G16_FLOAT, depth R24G8) and the two b1 copies, beside the executable
      if (gd.dump_textures[0] && gd.dump_textures[1])
      {
         com_ptr<ID3D11DeviceContext> context;
         native_device->GetImmediateContext(&context);
         std::filesystem::create_directories("Luma_MV_Dump");
         std::ofstream info("Luma_MV_Dump/info.txt");
         for (size_t i = 0; i < std::size(gd.dump_textures); i++)
         {
            D3D11_TEXTURE2D_DESC desc;
            gd.dump_textures[i]->GetDesc(&desc);
            D3D11_MAPPED_SUBRESOURCE mapped;
            if (FAILED(context->Map(gd.dump_textures[i].get(), 0, D3D11_MAP_READ, 0, &mapped)))
               continue;
            const UINT row_size = desc.Width * 4; // Both formats are 4 bytes per texel
            std::ofstream file(i == 0 ? "Luma_MV_Dump/mv.bin" : "Luma_MV_Dump/depth.bin", std::ios::binary);
            for (UINT y = 0; y < desc.Height; y++)
               file.write(static_cast<const char*>(mapped.pData) + size_t(y) * mapped.RowPitch, row_size);
            context->Unmap(gd.dump_textures[i].get(), 0);
            info << (i == 0 ? "mv " : "depth ") << desc.Width << " " << desc.Height << " " << int(desc.Format) << "\n";
         }
         for (size_t i = 0; i < std::size(gd.dump_cameras); i++)
         {
            info << (i == 0 ? "camera" : "previous_camera");
            const float* const values = reinterpret_cast<const float*>(gd.dump_cameras[i].data());
            for (size_t j = 0; j < gd.dump_cameras[i].size() / sizeof(float); j++)
               info << " " << std::format("{:.9g}", values[j]);
            info << "\n";
         }
         info << "jitter " << gd.dump_jitter[0] << " " << gd.dump_jitter[1] << "\n";
         gd.dump_textures[0].reset();
         gd.dump_textures[1].reset();
         reshade::log::message(reshade::log::level::info, "[MELE MV] dump written to Luma_MV_Dump");
      }
      // The world camera against last frame's: the PreViewTranslations and the largest ViewProjectionMatrix change
      if (gd.mv_active && cb_luma_global_settings.FrameIndex % 300 == 0 && gd.mv_camera && gd.mv_previous_camera && gd.mv_camera->size() == gd.mv_previous_camera->size() && gd.mv_camera->size() >= kPreViewTranslationOffset + 12)
      {
         const float* const current = reinterpret_cast<const float*>(gd.mv_camera->data());
         const float* const previous = reinterpret_cast<const float*>(gd.mv_previous_camera->data());
         float change = 0.f;
         for (int i = 0; i < 16; i++)
            change = (std::max)(change, std::abs(current[i] - previous[i]));
         const auto current_translation = GetPreViewTranslation(*gd.mv_camera), previous_translation = GetPreViewTranslation(*gd.mv_previous_camera);
         reshade::log::message(reshade::log::level::info, std::format("[MELE MV] camera: PVT ({:.3f}, {:.3f}, {:.3f}) previous ({:.3f}, {:.3f}, {:.3f}), max VP change {:.6f}, VP row 3 ({:.4f}, {:.4f}, {:.4f}, {:.4f}), b1 {} bytes, determinant {:.4f}, {} matched with an identical camera", current_translation[0], current_translation[1], current_translation[2], previous_translation[0], previous_translation[1], previous_translation[2], change, current[12], current[13], current[14], current[15], gd.mv_camera->size(), ViewDeterminant(*gd.mv_camera), gd.mv_last_stats.matched_same_camera).c_str());
      }
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
         else if (device_data.debug_draw_texture && device_data.debug_draw_texture.get() == gd.mv_texture.get())
         {
            device_data.debug_draw_texture = nullptr;
         }
      }
#endif

      gd.stage2_seen_this_frame = false;
      // Clear per-frame SMAA state so menus and transitions cannot reuse stale predication depth.
      gd.smaa_applied_handles.clear();
      gd.srv_depth.reset();
      // Drop XeGTAO inputs and ownership so failed or absent AO chains cannot reuse stale resources.
      gd.srv_gtao_depth.reset();
      gd.srv_gtao_normals.reset();
      gd.gtao_active_this_frame = false;
      gd.bloom_scale_captured_this_frame = false; // Re-arm BloomScale capture.
      gd.scene_post_done_this_frame = false;      // Re-armed by the FXAA resolve.
#if DEVELOPMENT
      gd.bloom_bright_pass_hits = 0;
      gd.stage1_draws = 0; // stage1_perm deliberately survives: a paused or menu frame keeps the last answer.
#endif

      // This is the sole writer of effective BloomIntensity. UI code edits only the raw slider and enable state,
      // preventing raw/derived values from oscillating while dragging. Native bloom keeps multiplier 1.
      {
         auto& gs = cb_luma_global_settings.GameSettings;
         float eff = 1.f;
         if (g_bloom_enable)
         {
            const float scale = gd.bloom_scale_live >= 0.f ? std::clamp(gd.bloom_scale_live, 0.f, 4.f) : 1.f;
            eff = g_bloom_intensity * scale;
         }
         if (fabsf(gs.BloomIntensity - eff) > 1e-4f)
         {
            gs.BloomIntensity = eff;
            device_data.cb_luma_global_settings_dirty = true;
         }

         // Follow native per-scene cb0.y; the default until the first readback.
         const float thr = gd.bloom_threshold_live >= 0.f ? gd.bloom_threshold_live : default_luma_global_game_settings.BloomThreshold;
         if (fabsf(gs.BloomThreshold - thr) > 1e-4f)
         {
            gs.BloomThreshold = thr;
            device_data.cb_luma_global_settings_dirty = true;
         }
      }
   }

   void LoadConfigs() override
   {
      reshade::get_config_value(nullptr, PROJECT_NAME, "SMAAEnable", g_smaa_enable);
      reshade::get_config_value(nullptr, PROJECT_NAME, "RCASSharpness", g_rcas_sharpness);
      auto& gs = cb_luma_global_settings.GameSettings;
      reshade::get_config_value(nullptr, PROJECT_NAME, "Exposure", gs.Exposure);
      reshade::get_config_value(nullptr, PROJECT_NAME, "Saturation", gs.Saturation);
      reshade::get_config_value(nullptr, PROJECT_NAME, "HighlightDechroma", gs.HighlightDechroma);
      reshade::get_config_value(nullptr, PROJECT_NAME, "Contrast", gs.Contrast);
      reshade::get_config_value(nullptr, PROJECT_NAME, "VignetteIntensity", gs.VignetteIntensity);
      reshade::get_config_value(nullptr, PROJECT_NAME, "FilmGrainIntensity", gs.FilmGrainIntensity);
      reshade::get_config_value(nullptr, PROJECT_NAME, "BloomEnable", g_bloom_enable);
      reshade::get_config_value(nullptr, PROJECT_NAME, "BloomIntensity", g_bloom_intensity);
      reshade::get_config_value(nullptr, PROJECT_NAME, "Dithering", gs.Dithering);
      reshade::get_config_value(nullptr, PROJECT_NAME, "VideoAutoHDREnable", g_video_auto_hdr_enable);
      gs.VideoAutoHDREnable = g_video_auto_hdr_enable ? 1.f : 0.f;
      reshade::get_config_value(nullptr, PROJECT_NAME, "VideoAutoHDRBoost", gs.VideoAutoHDRBoost);
      reshade::get_config_value(nullptr, PROJECT_NAME, "GTAOEnable", g_gtao_enable);
      reshade::get_config_value(nullptr, PROJECT_NAME, "GTAOFinalValuePower", g_gtao_final_value_power);
      reshade::get_config_value(nullptr, PROJECT_NAME, "GTAODepthScale", g_gtao_depth_scale);
      reshade::get_config_value(nullptr, PROJECT_NAME, "GTAORadiusOverride", g_gtao_radius_override);
   }

   void DrawImGuiSettings(DeviceData& device_data) override
   {
      ImGui::SeparatorText("Anti-Aliasing");
      const bool sr_active = IsSRActive(device_data);
      // The upscaler (Super Resolution, in the Settings tab) replaces SMAA: shown off, the saved choice is kept
      ImGui::BeginDisabled(sr_active);
      bool smaa_shown = g_smaa_enable && !sr_active;
      if (ImGui::Checkbox("SMAA Enable", sr_active ? &smaa_shown : &g_smaa_enable))
         reshade::set_config_value(nullptr, PROJECT_NAME, "SMAAEnable", g_smaa_enable);
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
         ImGui::SetTooltip("Replaces the game's FXAA with SMAA (requires AA enabled in the game's video settings; not used with DLSS/FSR).");
      ImGui::EndDisabled();
      ImGui::BeginDisabled(!g_smaa_enable && !sr_active);
      ImGui::SliderFloat("RCAS Sharpness", &g_rcas_sharpness, 0.f, 1.f);
      if (ImGui::IsItemDeactivatedAfterEdit())
         reshade::set_config_value(nullptr, PROJECT_NAME, "RCASSharpness", g_rcas_sharpness);
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
         ImGui::SetTooltip("Sharpening applied on top of SMAA or DLSS/FSR (0 = off; requires AA enabled in the game's video settings).");
      ImGui::EndDisabled();

      ImGui::SeparatorText("Grade");
      auto& gs = cb_luma_global_settings.GameSettings;
      auto& gd_def = default_luma_global_game_settings;
      // A [0, max] float setting: slider, saved on edit, reset button (saved too: LoadConfigs reads PROJECT_NAME, not [Luma])
      const auto slider = [&](const char* label, float* value, float default_value, const char* key, float max, const char* tooltip)
      {
         if (ImGui::SliderFloat(label, value, 0.f, max))
            device_data.cb_luma_global_settings_dirty = true;
         if (ImGui::IsItemDeactivatedAfterEdit())
            reshade::set_config_value(nullptr, PROJECT_NAME, key, *value);
         if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip("%s", tooltip);
         if (DrawResetButton<float, false>(*value, default_value, key))
         {
            device_data.cb_luma_global_settings_dirty = true;
            reshade::set_config_value(nullptr, PROJECT_NAME, key, *value);
         }
      };

      slider("Exposure", &gs.Exposure, gd_def.Exposure, "Exposure", 2.f, "Overall image brightness (1 = vanilla).");

      if (cb_luma_global_settings.DisplayMode == DisplayModeType::HDR)
      {
         slider("Contrast", &gs.Contrast, gd_def.Contrast, "Contrast", 2.f, "Overall image contrast, HDR only (1 = vanilla).");

         slider("Saturation", &gs.Saturation, gd_def.Saturation, "Saturation", 2.f, "Color saturation, HDR only (1 = vanilla).");

         slider("Highlights Desaturation", &gs.HighlightDechroma, gd_def.HighlightDechroma, "HighlightDechroma", 1.f, "How far the brightest sources fade to neutral white, HDR only (0 = keep color at any brightness).");
      }

      ImGui::SeparatorText("Bloom");
      if (ImGui::Checkbox("Luma Bloom Enable", &g_bloom_enable))
      {
         reshade::set_config_value(nullptr, PROJECT_NAME, "BloomEnable", g_bloom_enable);
         device_data.cb_luma_global_settings_dirty = true;
      }
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Replaces the game's bloom with a wider, softer HDR bloom.");
      ImGui::BeginDisabled(!g_bloom_enable);
      slider("Bloom Intensity", &g_bloom_intensity, gd_def.BloomIntensity, "BloomIntensity", 2.f, "Bloom strength (1 = vanilla, 0 = none).");
      ImGui::EndDisabled();

      ImGui::SeparatorText("Ambient Occlusion");
      if (ImGui::Checkbox("XeGTAO Enable", &g_gtao_enable))
         reshade::set_config_value(nullptr, PROJECT_NAME, "GTAOEnable", g_gtao_enable);
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Replaces the game's HBAO+ with XeGTAO (cleaner, more accurate ambient occlusion).");
#if DEVELOPMENT || TEST
      ImGui::BeginDisabled(!g_gtao_enable);
      ImGui::SliderFloat("GTAO Final Value Power", &g_gtao_final_value_power, 0.3f, 4.5f, "%.2f");
      if (ImGui::IsItemDeactivatedAfterEdit())
         reshade::set_config_value(nullptr, PROJECT_NAME, "GTAOFinalValuePower", g_gtao_final_value_power);
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
         ImGui::SetTooltip("Primary darkness dial (higher = darker AO). Calibrate to match vanilla's overall darkening.");
      ImGui::SliderFloat("GTAO Depth Scale", &g_gtao_depth_scale, 10.f, 200.f, "%.0f", ImGuiSliderFlags_Logarithmic);
      if (ImGui::IsItemDeactivatedAfterEdit())
         reshade::set_config_value(nullptr, PROJECT_NAME, "GTAODepthScale", g_gtao_depth_scale);
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
         ImGui::SetTooltip("viewZ divisor (UE3 units -> ~meters). FIRST dial if AO shows broad depth-correlated over-occlusion.");
      ImGui::SliderFloat("GTAO Radius Override", &g_gtao_radius_override, 0.f, 3.f, "%.3f");
      if (ImGui::IsItemDeactivatedAfterEdit())
         reshade::set_config_value(nullptr, PROJECT_NAME, "GTAORadiusOverride", g_gtao_radius_override);
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
         ImGui::SetTooltip("0 = shader EFFECT_RADIUS define; > 0 overrides it to match the vanilla HBAO+ radius (uu / DepthScale).");
      ImGui::Combo("GTAO Debug View", &g_gtao_debug_view, "Off\0Depth gradient\0Normals\0AO x8\0Edges\0");
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
         ImGui::SetTooltip("Draws diagnostics through the game's AO apply (multiplied into the scene). Depth gradient dead/flat or normals blocky = wrong input; AO x8 = spot broad over-occlusion.");
#if DEVELOPMENT
      ImGui::Combo("GTAO Temporal Noise", &g_gtao_temporal, "Auto (DLSS/FSR)\0Off (frozen, 2 denoise passes)\0On (frame % 64, 1 denoise pass)\0");
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
         ImGui::SetTooltip("A/B of the XeGTAO noise: per frame with one denoise pass (DLSS/FSR accumulate it), or frozen with two. Not saved.");
#endif
      ImGui::EndDisabled();
#endif

      ImGui::SeparatorText("Effects");
      slider("Vignette Intensity", &gs.VignetteIntensity, gd_def.VignetteIntensity, "VignetteIntensity", 1.f, "Scales the game's vignette darkening (1 = vanilla, 0 = none).");

      slider("Film Grain Intensity", &gs.FilmGrainIntensity, gd_def.FilmGrainIntensity, "FilmGrainIntensity", 1.f, "Scales the game's film grain (1 = vanilla, 0 = off).");

      if (cb_luma_global_settings.DisplayMode == DisplayModeType::HDR)
      {
         if (ImGui::Checkbox("Video AutoHDR", &g_video_auto_hdr_enable))
         {
            reshade::set_config_value(nullptr, PROJECT_NAME, "VideoAutoHDREnable", g_video_auto_hdr_enable);
            gs.VideoAutoHDREnable = g_video_auto_hdr_enable ? 1.f : 0.f;
            device_data.cb_luma_global_settings_dirty = true;
         }
         if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Adds HDR highlights to pre-rendered videos (HDR only).");
         ImGui::BeginDisabled(!g_video_auto_hdr_enable);
         slider("Video HDR Boost", &gs.VideoAutoHDRBoost, gd_def.VideoAutoHDRBoost, "VideoAutoHDRBoost", 1.f, "Video highlight strength (0 = off).");
         ImGui::EndDisabled();
      }

      bool dithering = gs.Dithering > 0.5f;
      if (ImGui::Checkbox("Dithering", &dithering))
      {
         gs.Dithering = dithering ? 1.f : 0.f;
         device_data.cb_luma_global_settings_dirty = true;
         reshade::set_config_value(nullptr, PROJECT_NAME, "Dithering", gs.Dithering);
      }
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Reduces gradient banding.");

      ImGui::SeparatorText("UI");
      ImGui::Checkbox("Hide Gameplay UI", &g_hide_ui); // Session-only to avoid a confusing HUD-less restart.
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Disables the in-game UI.");

#if DEVELOPMENT
      {
         auto& gd = GetGameDeviceData(device_data);
         const char* gname = GameName(g_me_game);
         const float eff_thr = gd.bloom_threshold_live >= 0.f ? gd.bloom_threshold_live : default_luma_global_game_settings.BloomThreshold;
         ImGui::SeparatorText("Stage 1 DEV readout");
         if (gd.stage1_perm == nullptr)
         {
            ImGui::Text("no stage-1 tonemap draw seen yet");
         }
         else
         {
            ImGui::Text("perm 0x%08X  (draws this frame: %d)", gd.stage1_perm->hash, gd.stage1_draws);
            if (ImGui::IsItemHovered())
               ImGui::SetTooltip("The stage-1 permutation the game drew last; its description and HDR family are in the matching Tonemap_0x<hash> shader. A frame reporting two draws switched permutation mid-frame and may be using two families.");
         }

         ImGui::SeparatorText("Bloom DEV readout");
         ImGui::Text("game=%s  bright-pass hits/frame=%d  (0 = capture hook never fired)", gname, gd.bloom_bright_pass_hits);
         ImGui::Text("threshold_live=%.4f  scale_live=%.4f  (-1 = not captured yet)", gd.bloom_threshold_live, gd.bloom_scale_live);
         ImGui::Text("eff threshold=%.4f  eff intensity=%.4f", eff_thr, cb_luma_global_settings.GameSettings.BloomIntensity);
      }
#endif
   }

#if DEVELOPMENT
   void DrawImGuiDevSettings(DeviceData& device_data) override
   {
      const auto& stats = GetGameDeviceData(device_data).mv_last_stats;
      ImGui::SeparatorText("Motion vector research");
      ImGui::BeginDisabled(perf_test_modes[g_perf_test].set_aa); // The "Performance Test" mode owns it
      ImGui::Checkbox("MV Enable", &g_mv_enable);
      ImGui::EndDisabled();
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
         ImGui::SetTooltip("Draws the scene with the motion vector shaders without DLSS/FSR: camera and object motion (each draw finds its own\nprevious frame b0/b1/b3). The image must not change; the debug view is black with a static camera and lights up only\nwhat moves. ReShade.log: patched/refused shaders. Not saved; greyed while a Performance Test mode sets the anti-aliasing.");
      ImGui::Checkbox("MV Force Jitter", &g_mv_force_jitter);
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Jitters the scene (Halton 2/3, 8 phases) without an upscaler, with MV Enable. The image shakes by a subpixel; nothing\nmay flicker or lose pixels, and the debug view stays black with a static camera. Not saved.");
      ImGui::Checkbox("MV Disable Jitter", &g_mv_disable_jitter);
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("No projection jitter under DLSS/FSR (the upscaler gets zero jitter): isolates artifacts that come from the jitter. Not saved.");
      ImGui::Checkbox("MV Skip Fill", &g_mv_skip_fill);
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("No camera motion fill: pixels no patched draw wrote stay at zero, so the debug view shows the object draws alone. Not saved.");
      if (ImGui::Button("MV Dump"))
         g_mv_dump = true;
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Writes this frame's motion vectors, depth and cameras to \"Luma_MV_Dump\" beside the executable (offline analysis).");
      ImGui::Checkbox("MV Debug View", &g_mv_debug_view);
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Shows the motion vector target (absolute, in pixels) through Core's debug draw.");
      ImGui::Text("Last frame: %u motion vector draws (%u matched, %u camera only, %u other camera, %u uncopied), %u jitter draws, %u mirrored, %u view restarts", stats.motion_vector_draws, stats.matched, stats.camera_only, stats.other_camera, stats.uncopied, stats.jitter_draws, stats.mirrored, stats.view_restarts);
      ImGui::Text("Constant updates copied: %u, upscaler draws: %u, scene ended by 0x%08X, matched with an identical camera: %u", stats.updates, stats.sr_draws, stats.ended_by, stats.matched_same_camera);
      ImGui::Text("Refused: %u extra target, %u no scene, %u other depth/color, %u format, %u size, %u create, %u blend, %u shaders", stats.rejected[0], stats.rejected[1], stats.rejected[2], stats.rejected[3], stats.rejected[4], stats.rejected[5], stats.rejected[6], stats.rejected[7]);
      ImGui::Text("Last checked target: format %u, dimension %u, %ux%u (output %.0fx%.0f)", stats.rejected_format, stats.rejected_dimension, stats.rejected_width, stats.rejected_height, double(device_data.output_resolution.x), double(device_data.output_resolution.y));

      ImGui::SeparatorText("Performance");
      auto& game_device_data = GetGameDeviceData(device_data);
      const std::string sweep_label = std::format("Sweep ({}/{})", game_device_data.perf_sweep_step + 1, perf_sweep_rounds * std::size(perf_sweep_modes));
      if (ImGui::BeginCombo("Performance Test", g_perf_sweep ? sweep_label.c_str() : perf_test_modes[g_perf_test].name))
      {
         for (int i = 0; i < int(std::size(perf_test_modes)); i++)
         {
            const PerfTestMode& mode = perf_test_modes[i];
            ImGui::BeginDisabled(mode.set_aa && mode.sr_type != SR::Type::None && !device_data.sr_implementations_instances.contains(mode.sr_type));
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
         ImGui::SetTooltip("GPU timestamps (frame, scene, upscaler, camera fill) and the motion vector hooks' CPU time, a \"[MELE Perf]\" ReShade.log\nline every 120 frames after 60 settle frames. Sweep: DLSS K, jitter only, no motion vector draws and SMAA Off in turn,\n3 rounds, then the medians. Hold the camera still. Not saved.");
      ImGui::Checkbox("Hook Timers", &g_perf_hook_timers);
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Times the motion vector hooks for \"cpu hooks\" (two clock reads per hooked draw and constant upload).\nRun a Sweep with it off to see their own cost in the frame times.");
   }
#endif

   void PrintImGuiAbout() override
   {
      ImGui::PushTextWrapPos(0.f);
      ImGui::Text(
         "Luma for \"Mass Effect Legendary Edition\" is developed by DristoforColumb and is open source and free.\n"
         "It adds DLAA or FSR 3 native anti-aliasing, and replaces the game's FXAA with SMAA, its bloom with a wider HDR bloom, and its HBAO+ with XeGTAO, plus 16x anisotropic filtering.\n"
         "With the game's HDR enabled it also replaces the native HDR tonemap with a higher quality one.\n"
         "Enable Anti-Aliasing and Ambient Occlusion in the game's video settings for SMAA and XeGTAO to apply.\n"
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
      g_me_game = DetectMEGame();
      // Per-game defaults; LoadConfigs may override the persisted ones afterwards.
      const MEGameProfile profile = ProfileFor(g_me_game);
      g_tonemap_perms = profile.tonemap_perms;
      g_gtao_radius_override = profile.gtao_radius_override;

      Globals::SetGlobals(PROJECT_NAME, "Mass Effect Legendary Edition Luma mod", "", 3);

      // With in-game HDR on, the game already supplies the fp16 scRGB swapchain and RGBA16F stage-1/2 transport.
      // A general texture upgrade would also catch R8G8B8A8 velocity and UI, breaking their contracts.
      swapchain_format_upgrade_type = TextureFormatUpgradesType::AllowedEnabled; // Enables scRGB and linear composition.
      swapchain_upgrade_type = SwapchainUpgradeType::scRGB;
      texture_format_upgrades_type = TextureFormatUpgradesType::None; // HDR buffers are already fp16.

      // Mode 4 sets MaxAnisotropy=16. LOD bias 0 without DLSS/FSR, set per frame with them (see "OnPresent").
      enable_samplers_upgrade = true; // Boot-time only.
      samplers_upgrade_mode = 4;

      game = new MassEffectLE();
   }

   CoreMain(hModule, ul_reason_for_call, lpReserved);

   return TRUE;
}
