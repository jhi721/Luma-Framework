// Borderlands 2 + The Pre-Sequel — Luma HDR, SMAA and DLAA/FSR mod (Unreal Engine 3, native DX9 -> D3D11 via dgVoodoo2).
//
// dgVoodoo2 translates SM3.0 to ps_5_0 (ps_4_0 under 2.81.3), so CSO hashes differ from the native DX9 ones. Launch
// the game exe DIRECTLY: XNA Launcher.exe also loads d3d9 and would capture ReShade instead of the game.
// One shared addon serves both games, discriminated by the tonemap hash:
// - TONEMAP PS 0xD00AA2A7 (BL2) / 0xFCFE623E (TPS): scene fp16 + bloom + vignette + LUT + DOF -> 8-bit LDR.
//   Replaced to recover HDR; the HUD draws after it (onto the LDR on BL2, onto a post-FXAA buffer on TPS).
// - FXAA PS 0x0D3001F6 (only with in-game AA on) -> cancelled while SMAA is on (see the FXAA override): dropped on
//   BL2, where it runs pre-tonemap into a buffer nothing samples, and reduced to a plain copy on TPS, where it runs
//   post-tonemap into the buffer the HUD then draws onto. SMAA itself is injected post-tonemap, so it neither
//   depends on the game's AA setting nor perturbs the DoF.
// Post buffers stay gamma-space SDR. Only ONE Luma .addon per game folder, and no other HDR mod (e.g. RenoDX)
// alongside it.
// No motion vectors or jitter of its own: DLAA/FSR's are built by patched shaders (see MotionVectorPatches.h), as in Mass
// Effect 2007 under the same wrapper, and run in the SR bridge's x64 helper (NGX is x64-only, the game 32-bit).

// Don't pop the DEVELOPMENT auto-debugger MessageBox on DLL attach: under a borderless/fullscreen game it's
// invisible and blocks the loader (ReShade times out the addon load -> error 1114).
#define DISABLE_AUTO_DEBUGGER 1

#define GEOMETRY_SHADER_SUPPORT 0
#define ENABLE_SMAA 1     // SMAA ULTRA (+RCAS) injected post-tonemap; core auto-registers the 6 "SMAA ..." passes from Luma_SMAA_impl
#define ENABLE_BLOOM 1    // core auto-registers the Bloom VS/Prefilter/Downsample/Upsample passes -> Luma_Bloom_impl
#define ENABLE_LUMA_TAA 1 // A third "Super Resolution" choice next to the bridge's DLSS and FSR 3, drawn in process on any GPU
// SMAA runs POST-tonemap through the post-draw callback (see RunPostTonemapSMAA); needs original_draw_dispatch_func.
#define ENABLE_POST_DRAW_DISPATCH_CALLBACK 1
// The motion vector draw key reads the draw's arguments ("last_draw_dispatch_data")
#define ENABLE_DRAW_DISPATCH_DATA_CACHE 1
// SMAA's area texture without the U-shape smoothing (see Luma_SMAA_impl.hlsl)
#define SMAA_SMOOTH_U_SHAPES 0

#include "..\..\Core\core.hpp"
#include "..\..\External\WDK\includes\d3d11TokenizedProgramFormat.hpp"
#include "MotionVectorPatches.h"
#include "..\..\Tools\D3D9 VA Fix\vertex_constants.h"
#include "..\..\Core\includes\patched_draws.h"
#if DEVELOPMENT
#include "..\..\Core\includes\perf_test.h"
#endif
#include <share.h>
#include <shellapi.h> // ShellExecuteA for About links (system("start ...") hangs the render thread in exclusive fullscreen)

// FXAA resolve PS (only present when AA is enabled in the game's video settings). Cancelled while SMAA is on (see
// the draw override); also used by Hide UI to keep this opaque pass out of the HUD filter.
static constexpr uint32_t kFXAAResolveHash = 0x0D3001F6;
static constexpr uint32_t kTonemapHash = 0xD00AA2A7;    // BL2: writes the LDR buffer the HUD then draws onto
static constexpr uint32_t kTonemapHashTPS = 0xFCFE623E; // The Pre-Sequel: same engine, different tonemap CSO

// dgVoodoo 2.81.3 emits ps_4_0 where 2.87.3 emits ps_5_0, so the SAME shaders hash differently; the Is* helpers
// below match both. FXAA/video/icon are byte-shared between BL2 and TPS, so one 2.81.3 hash each covers both.
static constexpr uint32_t kFXAAResolveHash_v281 = 0xDF7DB98D; // BL2/TPS FXAA under dgVoodoo 2.81.3
static constexpr uint32_t kTonemapHash_v281 = 0xF14F8664;     // BL2 tonemap under dgVoodoo 2.81.3
static constexpr uint32_t kTonemapHashTPS_v281 = 0x2079F1E8;  // The Pre-Sequel tonemap under dgVoodoo 2.81.3

// Native bloom bright pass: 4 taps of the full-res scene into the half-res bloom buffer, weighting each by
// saturate((max3(colour * BloomScale) - BloomThreshold) * 0.5). It carries the per-area authored pair the Luma
// bloom needs - BloomScale in cb4[16].x, BloomThreshold in cb4[17].y. Byte-shared between BL2 and TPS.
static constexpr uint32_t kBloomBrightPassHash = 0x997ACB8E;      // dgVoodoo 2.87.3 (ps_5_0)
static constexpr uint32_t kBloomBrightPassHash_v281 = 0x5605F6C2; // dgVoodoo 2.81.3 (ps_4_0)

// SRV slots on the tonemap. No compile-time link to the shader register macros in Luma_BL2TPS_Tonemap.hlsl (BL2 default) and
// Tonemap_0xFCFE623E.ps_5_0.hlsl (TPS), so keep them in sync:
//   Luma bloom (injected) -> TM_T_LUMABLOOM : BL2 t5 / TPS t8 (TPS t5 is the native DOF)
//   native bloom          -> TM_T_BLOOM     : BL2 t1 / TPS t2 (TPS t1 is the light shafts)
static constexpr uint32_t kLumaBloomSlotBL2 = 5;
static constexpr uint32_t kLumaBloomSlotTPS = 8;
static constexpr uint32_t kNativeBloomSlotBL2 = 1;
static constexpr uint32_t kNativeBloomSlotTPS = 2;

// Scaleform item-card price shaders: mask-fill + digit-glyph PS hashes (a pair per dgVoodoo build).
static constexpr uint32_t kScaleformMaskFillHash_v281 = 0x9F8EA541;
static constexpr uint32_t kScaleformDigitGlyphHash_v281 = 0x63898919;
static constexpr uint32_t kScaleformMaskFillHash = 0x616BEBBD;
static constexpr uint32_t kScaleformDigitGlyphHash = 0x79CDF7BA;

// DLAA / FSR Native AA: the scene's first post passes after its translucency, which end a motion vector frame (the upscaler runs
// right before the first one; measured at BL2 4K: light shafts, FXAA, the depth of field's low resolution copy, bloom, tonemap).
// Not the depth of field's CoC (0x003AD65E): it runs before the translucent draws, which then draw into the scene again.
// The 2.81.3 twins are hashed offline from GlobalShaderCache-PC-D3D-SM3.bin (TDownsampleLightShaftsPixelShader<LS_Point> and its
// directional/spot light types, TFilterPixelShader, TDOFGatherPixelShader0; the bright pass's known twin reproduced as the control).
// TPS's cache shares these byte for byte except its tonemap and light shaft downsamples, and only its directional downsample still
// hashes apart after dgVoodoo.
static constexpr uint32_t kScenePostHashes[] = {
   0xEB2678CD, // light shafts downsample
   0x13098E07, // light shafts downsample, dgVoodoo 2.81.3
   0xFE261570, // light shafts downsample, directional light
   0xBFD04077, // light shafts downsample, directional light, dgVoodoo 2.81.3
   0xA33D1DF9, // light shafts downsample, directional light, TPS
   0x944209BA, // light shafts downsample, directional light, TPS, dgVoodoo 2.81.3
   0xC3B44D30, // light shafts downsample, spot light (BL2 and TPS)
   0x93DD55D8, // light shafts downsample, spot light (BL2 and TPS), dgVoodoo 2.81.3
   0x070EAE70, // depth of field low resolution scene
   0x6DF81571, // depth of field low resolution scene, dgVoodoo 2.81.3
   0xC710CF7C, // bloom downsample
   0x77A6E651, // bloom downsample, dgVoodoo 2.81.3
   kBloomBrightPassHash,
   kBloomBrightPassHash_v281,
   kFXAAResolveHash,
   kFXAAResolveHash_v281,
   kTonemapHash,
   kTonemapHash_v281,
   kTonemapHashTPS,
   kTonemapHashTPS_v281,
};

#if DEVELOPMENT
static bool g_mv_enable = false;       // Motion vectors without an upscaler
static bool g_mv_debug_view = false;   // Core's debug draw of the motion vector target
static bool g_mv_force_jitter = false; // The projection jitter without an upscaler
static bool g_mv_match_objects = true; // Off: every motion vector draw takes the camera only path (object matching A/B)
// One frame capture (Luma MCP "luma_trace_list") at the first upscaled frame with a near plane under 1 (the loading screen's spinning
// weapon; the game camera's is 10); re-armed through MCP
static bool g_mv_trace_loading = true;
// A/B of the CPU savings (see "FindRegisteredBuffer", "NewConstantsCopy", "FixImpossiblePerRTBlend")
static bool g_mv_buffer_filter = true;
static bool g_mv_constants_pool = true;
static bool g_blend_memo = true;
static bool g_bound_state_tracking = true; // See "BoundState"
static bool g_vc4_mirror = true;           // vc4 from the D3D9 proxy's constant mirror when it runs (see "vertex_constants")
static bool g_vc4_mirror_check = false;    // The mirror against the copies of vc4's uploads ("vc4 mirror mismatches" in the "[BL2 MV]" log)
static bool g_bound_state_check = false;   // The tracked states against the bound ones ("bound state mismatches" in the "[BL2 MV]" log)
// FSR's reactive and transparency & composition masks from the scene's alpha blended draws (Mass Effect 2007's, see
// "MotionVectorPatch::PatchPixelShaderReactive")
static bool g_sr_reactive_enable = true;
static float g_sr_reactive_scale = 1.f;      // The alpha blended draws' reactivity, scaled (AMD's default 1)
static float g_sr_reactive_threshold = 0.5f; // Under it 0, over it 0.9 (AMD's 0.2; Mass Effect 2007's 0.5: lower shakes static glows); 0: the scaled reactivity, capped at 0.9
static bool g_sr_reactive_debug_view = false;
static bool g_sr_tc_from_mask = false;       // The reactive mask as FSR's transparency & composition mask too, instead of the draws' own (OptiScaler does it)
static bool g_sr_reactive_pass = true;       // Off: the mask still runs, FSR doesn't get it (isolation test)
static bool g_sr_reactive_skip_fill = false; // The draws still write their mask, the fill doesn't pass it on (isolation test)
static bool g_sr_reactive_zero_test = false; // The mask cleared to 0 but still passed (isolates FSR's reaction to having one)
// "Performance Test" (see "OnPresent"): GPU timestamps and hook CPU time to ReShade.log ("[BL2 Perf]"). A mode ("Perf::g_test")
// sets the anti-aliasing, or turns one of the CPU savings above off, while it runs (the user's values come back on leaving it, never
// saved)
struct PerfTestMode
{
   const char* name;
   bool set_aa = false; // Else the current anti-aliasing ("sr_type", "smaa" and "luma_taa_quality" unused)
   SR::Type sr_type = SR::Type::None;
   bool smaa = false;
   int motion_vector_draws = 2; // 2 patched (motion vectors and jitter), 1 jitter only, 0 untouched (unjittered)
   int luma_taa_quality = -1;   // With "sr_type" Luma TAA: its "TAA_QUALITY" (else the user's)
   // One CPU saving off, the others as the user set them
   bool vc4_filter_off = false;
   bool vc4_pool_off = false;
   bool blend_memo_off = false;
};
constexpr PerfTestMode perf_test_modes[] = {
   {.name = "Off"},
   {.name = "Current Settings"},
   {.name = "DLSS", .set_aa = true, .sr_type = SR::Type::DLSS},
   {.name = "DLSS Jitter Only", .set_aa = true, .sr_type = SR::Type::DLSS, .motion_vector_draws = 1},
   {.name = "DLSS Without Motion Vector Draws", .set_aa = true, .sr_type = SR::Type::DLSS, .motion_vector_draws = 0},
   {.name = "FSR 3", .set_aa = true, .sr_type = SR::Type::FSR},
   {.name = "Luma TAA High", .set_aa = true, .sr_type = SR::Type::LumaTAA, .luma_taa_quality = 2},
   {.name = "Luma TAA Ultra", .set_aa = true, .sr_type = SR::Type::LumaTAA, .luma_taa_quality = 3},
   {.name = "SMAA", .set_aa = true, .smaa = true},
   {.name = "No AA", .set_aa = true},
   {.name = "Without vc4 Filter", .vc4_filter_off = true},
   {.name = "Without vc4 Pool", .vc4_pool_off = true},
   {.name = "Without Blend Memo", .blend_memo_off = true},
};
// "Sweep": these modes in turn, a log window each, over several rounds (interleaved, so the scene's drift averages out), then a
// median per mode against the last one. "CPU Sweep": the CPU savings each off in turn under the current anti-aliasing (an upscaler:
// without motion vectors the hooks return early), against "Current Settings".
constexpr int perf_sweep_modes[] = {2, 3, 4, 5, 6, 7, 8, 9};
constexpr int perf_cpu_sweep_modes[] = {10, 11, 12, 1};
static_assert(std::string_view(perf_test_modes[perf_sweep_modes[std::size(perf_sweep_modes) - 1]].name) == "No AA");
static_assert(std::string_view(perf_test_modes[perf_cpu_sweep_modes[std::size(perf_cpu_sweep_modes) - 1]].name) == "Current Settings");
constexpr Perf::SweepDef perf_sweeps[] = {{"Sweep", perf_sweep_modes}, {"CPU Sweep", perf_cpu_sweep_modes}};
// Per mode, each window's GPU frame, scene, end, hook and CPU frame times, and the end's parts. A log window (120 frames) per mode and
// round.
enum PerfColumn : size_t
{
   PERF_COLUMN_FRAME,
   PERF_COLUMN_SCENE,
   PERF_COLUMN_TAIL,
   PERF_COLUMN_HOOKS,
   PERF_COLUMN_CPU_FRAME,
   PERF_COLUMN_FILL,
   PERF_COLUMN_UPSCALER,
   PERF_COLUMN_COPY_BACK,
   PERF_COLUMN_SCENE_COPY,
   PERF_COLUMN_HELPER_EVALUATE, // The SR bridge helper's upscaler on its own GPU queue, within "PERF_COLUMN_UPSCALER"
   PERF_COLUMN_COUNT
};
static Perf::Sweep<PERF_COLUMN_COUNT> g_perf_sweep = {.defs = perf_sweeps, .rounds = 3, .windows = 1};
constexpr int perf_settle_frames = 30; // Skipped after a settings change (history reset, targets rebuilt) and the upscaler being ready
// Skipped after leaving the upscaler: the SR bridge's helper exits on its own time, and its GPU context slowed SMAA's frame by ~2 ms
// while it did (measured in a sweep after FSR 3)
constexpr int perf_helper_exit_settle_frames = 600;
// The scene's tail, from its end: after the fill, the upscaler and its copy back, and the tail's end
enum PerfStamp : size_t
{
   PERF_SCENE_START = Perf::FIRST_GAME_STAMP,
   PERF_SCENE_END,
   PERF_FILL_END,
   PERF_UPSCALER_END,
   PERF_COPY_END,
   PERF_SCENE_TAIL_END,
   PERF_STAMP_COUNT
};
static int GetPerfMotionVectorDraws()
{
   return perf_test_modes[Perf::g_test].motion_vector_draws;
}
#else
static constexpr bool g_mv_enable = false;
static constexpr bool g_mv_force_jitter = false;
static constexpr bool g_mv_match_objects = true;
static constexpr bool g_mv_buffer_filter = true;
static constexpr bool g_mv_constants_pool = true;
static constexpr bool g_blend_memo = true;
static constexpr bool g_bound_state_tracking = true;
static constexpr bool g_vc4_mirror = true;
static constexpr bool g_vc4_mirror_check = false;
static constexpr bool g_bound_state_check = false;
static constexpr bool g_sr_reactive_enable = true;
static constexpr float g_sr_reactive_scale = 1.f;
static constexpr float g_sr_reactive_threshold = 0.5f;
static constexpr bool g_sr_tc_from_mask = false;
static constexpr bool g_sr_reactive_pass = true;
static constexpr bool g_sr_reactive_skip_fill = false;
static constexpr int GetPerfMotionVectorDraws()
{
   return 2;
}
#endif

// User settings (persisted via ReShade config under the shared NAME section; loaded in LoadConfigs).
static bool g_smaa_enable = true;
static bool g_smaa_t2x = false;             // SMAA T2x: a two phase jitter and the previous frame, through the upscalers' motion vectors
static float g_rcas_sharpness = 0.f;        // RCAS sharpen on SMAA output (0 = off)
static bool g_hide_ui = false;              // hide the game's HUD (for clean screenshots)
static bool g_smaa_predication = true;      // SMAA predication on geometry (depth from scene-color .a)
static float g_smaa_pred_tolerance = 0.02f; // plane deviation counted as a full edge, as a fraction of view depth (MoH Airborne's calibrated value)
#if DEVELOPMENT
// Calibration aid for g_smaa_pred_tolerance, the only free parameter in the predication path: what predication
// does is an ABSENCE of smearing, which the eye reads badly and worse in motion, so judge the mask, not the frame.
static bool g_smaa_pred_debug = false;   // show the predication mask instead of the antialiased frame
static bool g_smaa_pred_measure = false; // one-shot: read the mask back and log its distribution (UI button)
#endif
static bool g_luma_bloom_enable = true;                                        // replace the game's clamped bloom with Luma HDR pyramidal bloom (live toggle)
static float g_bloom_intensity = 1.f;                                          // user-facing bloom strength (1 = vanilla); the uploaded value is derived from it
static bool g_video_auto_hdr_enable = true;                                    // light AutoHDR on Bink videos, HDR only (live toggle)
static constexpr int kBloomMipCount = 6;                                       // bloom pyramid mip count
static constexpr float kBloomSigmas[6] = {1.5f, 2.0f, 2.0f, 2.0f, 1.0f, 1.0f}; // per-mip Gaussian sigma (tapered, wider middle for a soft natural halo)
// Ratio of the BT.709-weighted means of the game's own bloom buffer and this pyramid, read back live on the same frames:
// native sits at 0.158 of ours. Folded into the effective BloomIntensity so 1 means vanilla strength. A property of
// THIS pyramid (octaves, Karis prefilter, threshold placement): retuning them invalidates it; the DEVELOPMENT A/B
// re-checks it every run.
static constexpr float kBloomPyramidToNativeEnergy = 0.158f;

// SMAA's resources go after this many presents without it (the upscaler antialiasing, or SMAA off): ~5 s, so menus and loading
// screens between upscaled frames, which run SMAA, don't recreate them each time
static constexpr uint32_t smaa_idle_release_frames = 600;

// Why "DrawWithMotionVectors" refused a draw: the DEV counters, the MCP trace note, and the names they are logged and registered
// under
enum MotionVectorReject : int
{
   REJECT_EXTRA_TARGET,
   REJECT_NO_SCENE,
   REJECT_OTHER_DEPTH_COLOR,
   REJECT_FORMAT,
   REJECT_SIZE,
   REJECT_CREATE,
   REJECT_BLEND,
   REJECT_SHADERS,
   REJECT_DEPTH_TEST,
   REJECT_COUNT
};
static constexpr const char* kMotionVectorRejectNames[REJECT_COUNT] = {"extra_target", "no_scene", "other_depth_color", "format", "size", "create", "blend", "shaders", "depth_test"};

// The buffer hooks (map/unmap/update_buffer_region: copies of vc4's uploads) are registered, only while motion vectors need them
// and the proxy's vertex constant mirror can't replace them (see "OnPresent")
static bool g_buffer_hooks_registered = false;

struct Borderlands2GameDeviceData final : public GameDeviceData
{
   // Repaired blend states, keyed by the ORIGINAL desc. Keying by desc rather than by the
   // source state's pointer means a released state can't leave a stale key that a later allocation reuses.
   struct BlendDescCompare
   {
      bool operator()(const D3D11_BLEND_DESC& a, const D3D11_BLEND_DESC& b) const
      {
         return memcmp(&a, &b, sizeof(D3D11_BLEND_DESC)) < 0;
      }
   };
   std::map<D3D11_BLEND_DESC, ComPtr<ID3D11BlendState>, BlendDescCompare> fixed_blend_states;

   // One staging copy per captured pass, read a frame late (see CaptureConstantRows). One buffer each, not a
   // ring: the reader may skip a frame, so a DO_NOT_WAIT map of LAST frame's copy is enough and never blocks.
   struct ConstantCapture
   {
      ComPtr<ID3D11Buffer> staging;
      UINT bytes = 0;
      bool copy_pending = false;
   };

   // Live bright-pass readback: the artist-authored bloom threshold this area is using. Negative until the first
   // successful map, which the C++ fallback covers.
   ConstantCapture bloom_cb;
   float bloom_threshold_live = -1.f;
   bool bloom_scale_warned = false;     // one-shot: bright-pass BloomScale is not the 4 the knee assumes
   bool bloom_threshold_warned = false; // one-shot: the bright pass never reported, so the knee is a fallback

   // Paces the shipping bright-pass readback, and the DEVELOPMENT A/B below.
   uint32_t frame_counter = 0;

#if DEVELOPMENT
   ConstantCapture grade_cb;
   bool grade_k_warned = false; // one-shot: a non-zero ImageAdjustments K, which the HDR continuation does not model

   // A/B energy measurement between the native bloom buffer and the Luma pyramid (see LogBloomEnergy): one staging
   // texture each, read a frame late on a slow cadence.
   struct TextureCapture
   {
      ComPtr<ID3D11Texture2D> staging;
      UINT width = 0;
      UINT height = 0;
      DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
      bool copy_pending = false;
   };
   TextureCapture bloom_ab_native;
   TextureCapture bloom_ab_luma;
   TextureCapture pred_measure;                   // one-shot readback of the predication mask, driven by the UI button
   float bloom_tint_live[3] = {-1.f, -1.f, -1.f}; // tonemap cb4[16].rgb, the per-area authored composite tint
   std::unordered_set<uint64_t> grade_cb_logged;  // quantized constant sets already reported
   std::unordered_set<uint64_t> bloom_cb_logged;
#endif

   // SMAA metrics CB (b1) = (1/w,1/h,w,h) + (predication scale,0,0,0) + T2x's subsample indices; scale 2.0 when predication on, else
   // 1.0. Written every frame.
   com_ptr<ID3D11Buffer> cb_smaa_metrics;

   // SMAA and RCAS input: a snapshot of the LDR as the tonemap wrote it (gamma 2.2). SMAA's edge detection reads it as stored, its
   // neighborhood blend filters it in linear light (Luma_SMAA_impl.hlsl).
   ComPtr<ID3D11Texture2D> tex_input_encoded;
   ComPtr<ID3D11ShaderResourceView> srv_input_encoded;
   // The frames SMAA, the snapshot's users (SMAA or RCAS) and SMAA's output temp (RCAS after SMAA) last ran, for
   // "smaa_idle_release_frames"
   uint32_t smaa_frame = 0;
   uint32_t snapshot_frame = 0;
   uint32_t smaa_out_frame = 0;

   // SMAA output temp (SRV+RTV), allocated only when RCAS is on: it is the RCAS input. Without RCAS the SMAA
   // chain renders into the LDR RTV directly.
   ComPtr<ID3D11Texture2D> tex_smaa_out;
   ComPtr<ID3D11RenderTargetView> tex_smaa_out_rtv;
   ComPtr<ID3D11ShaderResourceView> tex_smaa_out_srv;
   uint32_t smaa_out_w = 0, smaa_out_h = 0;

   // SMAA T2x (see "RunPostTonemapSMAA"), set at present for the whole frame like "sr_active". The scene jitters only after a frame
   // the resolve ran, as with "mv_jitter_allowed". The phase is chosen when the scene opens, -1 if it didn't jitter.
   bool t2x_active = false;
   int t2x_phase = -1;
   // SMAA's output of this frame and of the previous one, alternating: linear RGB with the velocity length in alpha. The previous one
   // is the history only if "t2x_frame", the last frame the resolve ran, was the frame before.
   ComPtr<ID3D11Texture2D> t2x_frames[2];
   ComPtr<ID3D11RenderTargetView> t2x_frame_rtvs[2];
   ComPtr<ID3D11ShaderResourceView> t2x_frame_srvs[2];
   uint32_t t2x_frame = 0;

   // RCAS sharpen CB (b0) = (w,h,sharpness,0). RCAS writes the LDR RTV, so it needs no output temp.
   ComPtr<ID3D11Buffer> cb_sharpen;
   uint32_t sharpen_w = 0, sharpen_h = 0;
   float sharpen_amount = -1.f;

   // Resource the tonemap renders to; on BL2 the HUD draws onto it afterwards. Used by Hide UI and the FXAA override.
   uint64_t ldr_buffer_handle = 0;
   // Set when the tonemap runs, cleared every Present: scopes Hide UI's alpha-blend skip to the post-tonemap
   // span of THIS frame (so next frame's pre-tonemap transparents aren't dropped). See the Hide UI block in OnDrawOrDispatch.
   bool tonemap_fired_this_frame = false;

   // SMAA depth predication. Scene-color SRV (depth packed in .a) captured at the tonemap, which the FXAA override
   // also keys on, + the single-channel (R16F) plane-deviation edge-ness the BL2TPS Depth Extract CS builds from it.
   ComPtr<ID3D11ShaderResourceView> srv_scene_depth;
   ComPtr<ID3D11Texture2D> tex_pred;
   ComPtr<ID3D11UnorderedAccessView> uav_pred;
   ComPtr<ID3D11ShaderResourceView> srv_pred;
   uint32_t pred_w = 0, pred_h = 0;
   ComPtr<ID3D11Buffer> cb_pred;
   float pred_tolerance = -1.f;

   // Luma HDR pyramidal bloom output (linear fp16), generated at the tonemap from the scene SRV, bound to PS t5 (BL2) / t8 (TPS).
   ComPtr<ID3D11ShaderResourceView> srv_luma_bloom;

   // Scaleform price-digit stencil repair: armed ("dsv_scaleform_mask_active" set) between a mask-submit and the glyph strips; the
   // mask is duplicated into a private scratch D24S8 (cached per RT size) that the strips then test EQUAL/ref=1 against.
   ComPtr<ID3D11DepthStencilState> dss_scaleform_mask_write;
   ComPtr<ID3D11DepthStencilState> dss_scaleform_mask_test;
   ComPtr<ID3D11DepthStencilView> dsv_scaleform_mask_active;
   struct ScaleformMaskDS
   {
      uint32_t width = 0, height = 0;
      ComPtr<ID3D11Texture2D> tex;
      ComPtr<ID3D11DepthStencilView> dsv;
   };
   ScaleformMaskDS scaleform_mask_ds_cache[4];
   uint32_t scaleform_mask_ds_next = 0;

   // ---- DLAA / FSR Native AA (Mass Effect 2007's motion vector path, one per-draw buffer: dgVoodoo's vc4) ----
   // DLSS and FSR run in the x64 helper of Core's SR bridge (the game is 32-bit, see "SRBridge.h").
   // "IsSRActive", taken at present: Core's "Super Resolution" selection changes after it, mid frame for the draws
   bool sr_active = false;
   // The upscaler's output goes back into the scene (and its copy, see "mv_scene_copy") without its alpha (the encoded view depth the
   // post passes read), drawn from this view of it
   com_ptr<ID3D11BlendState> sr_rgb_blend_state;
   com_ptr<ID3D11ShaderResourceView> sr_output_srv;
   // None was picked ("CleanExtraSRResources", from the overlay): the upscaler's resources go at the next present
   std::atomic<bool> release_sr_resources = false;

   std::atomic<bool> mv_active = false; // Motion vectors and jitter this frame: an upscaler is active, or the DEV toggle (set at present)
   std::shared_mutex mv_mutex;
   // A game shader's patched version (null if refused), patched on first use, by its hash; a vertex shader's with the bytes of vc4 it
   // reads ("DXBC::ConstantBufferBytes": the previous frame's copy uploads only those), its LocalToWorld translation row, and whether
   // its vertices come already translated by this frame's PreViewTranslation (see "DrawWithMotionVectors")
   template <typename T>
   struct PatchedShader
   {
      com_ptr<T> shader;
      UINT read_size = 0;
      UINT translation_offset = 0;
      bool camera_relative_vertices = false;
   };
   std::unordered_map<uint32_t, PatchedShader<ID3D11VertexShader>> mv_vertex_shaders;
   std::unordered_map<uint32_t, PatchedShader<ID3D11PixelShader>> mv_pixel_shaders;
   // The alpha blended draws' pixel shaders with the mask target, by blend (see "ClassifyBoundBlend", index - 1)
   std::unordered_map<uint32_t, PatchedShader<ID3D11PixelShader>> mv_reactive_pixel_shaders[2];
   // The motion vector target (sized like the scene; every blend state writes it unblended, see "OnCreateBlendState")
   com_ptr<ID3D11Texture2D> mv_texture;
   com_ptr<ID3D11RenderTargetView> mv_rtv;
   com_ptr<ID3D11UnorderedAccessView> mv_uav; // Null without typed UAV loads of its format (then no upscaler)
   com_ptr<ID3D11ShaderResourceView> mv_srv;  // SMAA T2x's view of it
   bool mv_filled = false;                    // The fill wrote this frame's motion vectors (reset at present)
   // The upscaler's depth, built from the scene's alpha by the fill (the game's depth has no shader resource view)
   com_ptr<ID3D11Texture2D> mv_device_depth;
   com_ptr<ID3D11UnorderedAccessView> mv_device_depth_uav;
   // FSR's reactive and transparency & composition masks (written by the fill from "mv_reactive_target")
   com_ptr<ID3D11Texture2D> mv_reactive;
   com_ptr<ID3D11UnorderedAccessView> mv_reactive_uav;
   com_ptr<ID3D11Texture2D> mv_transparency;
   com_ptr<ID3D11UnorderedAccessView> mv_transparency_uav;
   // What the alpha blended draws wrote (x reactive, y transparency & composition; max blended, see "OnCreateBlendState"), read by the
   // fill. Created when FSR first runs with the masks, gone "smaa_idle_release_frames" after the fill last wrote them (address space).
   com_ptr<ID3D11Texture2D> mv_reactive_target;
   com_ptr<ID3D11RenderTargetView> mv_reactive_target_rtv;
   com_ptr<ID3D11ShaderResourceView> mv_reactive_target_srv;
   uint32_t sr_reactive_frame = 0;
   // A frame opens at its first mesh draw into output sized depth (the jitter is chosen there), starts at its first motion vector
   // draw (the target is cleared) and ends at the first post pass ("kScenePostHashes"), once per present.
   bool mv_scene_open = false;
   bool mv_scene_done = false;
   bool mv_frame_ended = true;
   bool mv_fill_pending = false;
   // The upscaler drew the last frame: only then the scene jitters. Frames it skips would reach the screen jittered: scenes no listed
   // pass ends, and the loading screen's (its spinning weapon draws with jitter, but without a motion vector draw the fill and the
   // upscaler don't run, measured).
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
   // upscaler's output goes into both: post passes draw onto the scene and resolve it again (light shafts, measured), so
   // ME1's copy only write would be overwritten with the jittered scene.
   com_ptr<ID3D11Resource> mv_scene_copy;
   com_ptr<ID3D11RenderTargetView> mv_scene_copy_rtv; // The upscaler's output goes into both in one pass ("BL2TPS Copy Back PS")
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
   // The immediate context's blend and depth stencil states, vc4 (VS "MotionVectorPatches::object_slot"), vertex buffer 0 and index
   // buffer as the game last bound them ("OnBindPipeline", "OnPushConstantBuffers", "OnBindVertexBuffers", "OnBindIndexBuffer"), so
   // the draw hooks don't query them (an AddRef and a Release each, several per draw). Not referenced: an object lives while bound.
   // Luma's own passes bind theirs natively and put the game's back. False after a bind this can't read (another add-on's combined
   // pipeline): queried then.
   ID3D11BlendState* bound_blend_state = nullptr;
   ID3D11DepthStencilState* bound_depth_stencil_state = nullptr;
   ID3D11Buffer* bound_object_buffer = nullptr;
   ID3D11Buffer* bound_vertex_buffer = nullptr;
   UINT bound_vertex_offset = 0;
   ID3D11Buffer* bound_index_buffer = nullptr;
   UINT bound_index_offset = 0;
   bool bound_states_tracked = true;
   uint8_t mv_reactive_blend = 0;
   uint32_t mv_last_vertex_shader_hash = 0;
   PatchedShader<ID3D11VertexShader> mv_last_vertex_shader;
   uint32_t mv_last_pixel_shader_hash = 0;
   ID3D11PixelShader* mv_last_pixel_shader = nullptr;
   PatchedDraws::BoundShader<ID3D11VertexShader> mv_bound_vertex_shader;
   PatchedDraws::BoundShader<ID3D11PixelShader> mv_bound_pixel_shader;

   // CPU copies of the vc4 buffers the motion vector draws bind, by buffer (an entry registers it, empty until its first upload), from
   // a Map(WRITE_DISCARD) at its Unmap or an UpdateSubresource: a draw's constants are its buffer's latest upload. Unlocked: the buffer
   // hooks, the draws and "OnPresent" all run on the immediate context's thread ("mv_constants_thread"; dgVoodoo has no deferred
   // contexts). A lock here was ~9000 SRW acquires per frame on the render thread.
   using ConstantsCopy = std::shared_ptr<const std::vector<uint8_t>>;
   struct RegisteredConstants
   {
      // The last upload, rewritten in place: dgVoodoo maps a vc4 buffer for every draw, about 3 of 4 of them no motion vector draw
      std::vector<uint8_t> latest;
      ConstantsCopy copy;     // "latest" as the motion vector draws since that upload took it (made by the first)
      void* mapped = nullptr; // Mapped now, until its Unmap
   };
   std::unordered_map<uint64_t, RegisteredConstants> mv_constants_copies;
#if DEVELOPMENT
   DWORD mv_constants_thread = 0; // The thread of the last "OnPresent", checked by the buffer hooks
#endif
   // "g_mv_buffer_filter": the first registered buffers, their sizes and entries (a node's address stays while the map grows), so a
   // buffer hook finds them without a lookup. With more registered, every buffer takes the map lookup as without the filter.
   static constexpr uint32_t kMaxFilteredBuffers = 8;
   std::array<uint64_t, kMaxFilteredBuffers> mv_filtered_buffers = {};
   std::array<UINT, kMaxFilteredBuffers> mv_filtered_buffer_sizes = {};
   std::array<RegisteredConstants*, kMaxFilteredBuffers> mv_filtered_entries = {};
   uint32_t mv_filtered_buffer_count = 0;
   bool mv_filter_overflow = false;
   // The D3D9 VA Fix proxy's vertex constant mirror while its CSMT layer runs (null otherwise, checked at every present): dgVoodoo's
   // vc4 as uploaded for the draw being translated, on this same thread, so the buffer hooks aren't needed. Its snapshot for the
   // motion vector draws since it last changed.
   const VertexConstantMirror* vertex_constants = nullptr;
   ConstantsCopy vertex_constants_copy;
   uint32_t vertex_constants_copy_generation = 0;
   // "g_mv_constants_pool": every pooled copy, and those nobody held anymore at the last present (taken by the next copies)
   std::vector<std::shared_ptr<std::vector<uint8_t>>> mv_constants_pool;
   std::vector<uint32_t> mv_constants_pool_free;
   size_t mv_constants_made = 0; // Copies asked for since the last present
   // Previous frame constants of the motion vector draws (see "PatchedDraws::PreviousConstants")
   PatchedDraws::PreviousConstants mv_previous_constants;
   // Motion vector draws by draw key (shaders, buffers, arguments), with a world LocalToWorld and vc4. A draw takes the previous
   // frame's vc4 of its key's nearest draw (same object, a frame earlier), its camera included.
   struct MotionVectorObject
   {
      PatchedDraws::ObjectTransform transform; // LocalToWorld, its translation in world space
      ConstantsCopy constants;
#if DEVELOPMENT
      uint32_t vertex_shader = 0, pixel_shader = 0; // For the tie-break collision log
#endif
   };
   // The keys are "HashCombine" outputs already: one multiply spreads them over the buckets (std::hash runs FNV-1a on every byte)
   struct DrawKeyHash
   {
      size_t operator()(uint64_t key) const noexcept
      {
         return size_t((key * 0x9E3779B97F4A7C15ull) >> 32);
      }
   };
   std::unordered_map<uint64_t, std::vector<MotionVectorObject>, DrawKeyHash> mv_objects;
   std::unordered_map<uint64_t, std::vector<MotionVectorObject>, DrawKeyHash> mv_previous_objects;
   // The frame's camera (vc4 of its first motion vector draw) and the previous frame's
   ConstantsCopy mv_camera;
   ConstantsCopy mv_previous_camera;
   uint32_t mv_frame_index = 0;              // The Luma frame index of the last motion vector frame
   std::vector<uint8_t> mv_camera_only_copy; // An unmatched draw's constants with last frame's camera (reused)

#if DEVELOPMENT
   // Per frame counts for the DEV panel (the last complete frame's shown)
   struct MotionVectorStats
   {
      uint32_t motion_vector_draws = 0, jitter_draws = 0, matched = 0, camera_only = 0, other_camera = 0, uncopied = 0, maps = 0, updates = 0, other_maps = 0, sr_draws = 0;
      uint32_t tiebreak_collisions = 0;                                                              // Objects sharing a key and a transform with other constants ("PatchedDraws::CountTieBreakCollisions")
      uint32_t vc4_mirror_mismatches = 0;                                                            // "g_vc4_mirror_check": motion vector draws whose mirror rows weren't the copied upload
      uint32_t vc4_mirror_used_mismatches = 0;                                                       // Of them, the ones in the rows read here (camera, LocalToWorld)
      uint32_t vc4_mirror_mismatch_row = 0, vc4_mirror_mismatch_vs = 0;                              // The first row and the shader of the last one
      uint32_t vc4_mirror_unknown = 0;                                                               // Motion vector draws with rows the mirror doesn't know (after a Reset or a state block's Apply)
      uint32_t bound_state_mismatches = 0;                                                           // "g_bound_state_check": tracked states that weren't the bound ones
      uint32_t reactive_draws = 0;                                                                   // Alpha blended draws that wrote FSR's masks
      uint32_t ended_by = 0;                                                                         // The ending pass's PS hash
      int ended_by_scene_slot = -1;                                                                  // The PS slot it reads the scene's copy from, -1 if none
      float near_plane = 0.f, far_plane = 0.f;                                                       // The upscaler's, from the camera's projection (0: none found)
      uint32_t rejected[REJECT_COUNT] = {};                                                          // "DrawWithMotionVectors" refusals by reason ("MotionVectorReject")
      uint32_t rejected_format = 0, rejected_dimension = 0, rejected_width = 0, rejected_height = 0; // The last target refused by format or size
   };
   MotionVectorStats mv_stats, mv_last_stats;
   int mv_draw_reject = -1; // The current draw's "MotionVectorReject" reason (-1 for none), for the MCP trace note
   // Scene end audit (each pixel shader logged once): passes that read the scene or its copy into another target while the scene is
   // open (a post pass "kScenePostHashes" lacks: the scene ended late), and draws into the scene and its depth after its end (it
   // ended early: they draw unjittered)
   std::unordered_set<uint32_t> mv_reads_before_end;
   std::unordered_set<uint32_t> mv_draws_after_end;
   // "Performance Test": GPU timestamps per frame (present to present, the scene from its opening to its first post pass, the end
   // of the scene: the fill, the upscaler and copies, see "PerfStamp"); and the CPU time in the scene hooks
   struct PerfStats
   {
      Perf::Stat frame, scene, end;
      Perf::Stat end_parts[4]; // The end of the scene: fill, upscaler, copy back, scene copy (the rest)
   };
   Perf::TimestampRing<PERF_STAMP_COUNT> perf_timestamps;
   Perf::Window<PerfStats> perf_window;
   bool perf_sr_active = false; // The last measured frame's upscaler
   // The SR bridge helper's "LUMA_UPSCALER_PROFILE" CSV while a test runs, and how far it was read
   std::wstring perf_helper_profile_path;
   int64_t perf_helper_profile_read = 0;
   // The user's anti-aliasing and CPU savings while a mode that sets its own runs
   SR::Type perf_user_sr_type = SR::Type::None;
   int perf_user_taa_quality = 2;
   bool perf_user_smaa = false;
   bool perf_user_vc4_filter = true;
   bool perf_user_vc4_pool = true;
   bool perf_user_blend_memo = true;
#endif

   // SMAA's own resources (predication, output temp), apart from Core's ("ReleaseSMAA"); recreated at their next use
   void ReleaseSMAAScratch()
   {
      tex_pred.reset();
      uav_pred.reset();
      srv_pred.reset();
      pred_w = pred_h = 0;
      ReleaseSMAAOutput();
   }

   void ReleaseSMAAOutput()
   {
      tex_smaa_out.reset();
      tex_smaa_out_rtv.reset();
      tex_smaa_out_srv.reset();
      smaa_out_w = smaa_out_h = 0;
   }

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

   // FSR's masks and what the draws wrote for them, recreated at their next use
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

   // What SMAA and RCAS share: the snapshot
   void ReleaseSnapshotScratch()
   {
      tex_input_encoded.reset();
      srv_input_encoded.reset();
   }
};

class Borderlands2 final : public Game
{
   static Borderlands2GameDeviceData& GetGameDeviceData(DeviceData& device_data)
   {
      return *static_cast<Borderlands2GameDeviceData*>(device_data.game);
   }

   // Pass identity by shader hash, folding every supported dgVoodoo version (2.87.3 ps_5_0 + 2.81.3 ps_4_0).
   static bool ContainsPixelShader(const ShaderHashesList<OneShaderPerPipeline>& hashes, uint32_t hash, uint32_t hash_v281)
   {
      return hashes.Contains(hash, reshade::api::shader_stage::pixel) || hashes.Contains(hash_v281, reshade::api::shader_stage::pixel);
   }
   static bool IsBL2Tonemap(const ShaderHashesList<OneShaderPerPipeline>& hashes)
   {
      return ContainsPixelShader(hashes, kTonemapHash, kTonemapHash_v281);
   }
   static bool IsTPSTonemap(const ShaderHashesList<OneShaderPerPipeline>& hashes)
   {
      return ContainsPixelShader(hashes, kTonemapHashTPS, kTonemapHashTPS_v281);
   }
   static bool IsAnyTonemap(const ShaderHashesList<OneShaderPerPipeline>& hashes)
   {
      return IsBL2Tonemap(hashes) || IsTPSTonemap(hashes);
   }
   static bool IsFXAA(const ShaderHashesList<OneShaderPerPipeline>& hashes)
   {
      return ContainsPixelShader(hashes, kFXAAResolveHash, kFXAAResolveHash_v281);
   }

   static bool CreateImmutableCB(ID3D11Device* device, const void* data, UINT size, ComPtr<ID3D11Buffer>* out)
   {
      out->reset();
      const CD3D11_BUFFER_DESC bd(size, D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_IMMUTABLE);
      const D3D11_SUBRESOURCE_DATA initial_data = {.pSysMem = data};
      return SUCCEEDED(device->CreateBuffer(&bd, &initial_data, out->put()));
   }

   // Default-pool 2D texture. LDR-shaped temps take the live LDR format, since CopyResource requires identical formats.
   static bool CreateDefaultTex(ID3D11Device* device, uint32_t w, uint32_t h, UINT bind_flags, ComPtr<ID3D11Texture2D>* out, DXGI_FORMAT format)
   {
      out->reset();
      const CD3D11_TEXTURE2D_DESC td(format, w, h, 1, 1, bind_flags);
      return SUCCEEDED(device->CreateTexture2D(&td, nullptr, out->put()));
   }

#if ENABLE_SMAA
   // Post-tonemap SMAA on the gamma LDR: AFTER the tonemap, so it cannot perturb the DoF composited inside it.
   // Snapshot LDR -> predication extract CS -> DrawSMAA -> optional RCAS; the last pass writes the LDR RTV.
   // Without "smaa" (the upscaler antialiased the frame, ME1's shape) only RCAS runs: snapshot -> RCAS -> LDR.
   void RunPostTonemapSMAA(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, DeviceData& device_data, Borderlands2GameDeviceData* gd, ID3D11RenderTargetView* ldr_rtv, bool smaa)
   {
      ComPtr<ID3D11Resource> ldr_res;
      ldr_rtv->GetResource(ldr_res.put());
      uint4 ldr_size{};
      DXGI_FORMAT ldr_format = DXGI_FORMAT_UNKNOWN;
      GetResourceInfo(ldr_res.get(), ldr_size, ldr_format);
      uint32_t w = ldr_size.x, h = ldr_size.y;
      if (w == 0 || h == 0 || (uint32_t)ldr_format == (uint32_t)DXGI_FORMAT_UNKNOWN)
      {
         return;
      }

      // RCAS decides the chain's shape: SMAA renders into the LDR RTV directly or into the intermediate RCAS reads.
      auto* copy_vs = FindShader(device_data.native_vertex_shaders, CompileTimeStringHash("Copy VS"));
      auto* sharpen_ps = FindShader(device_data.native_pixel_shaders, CompileTimeStringHash("BL2TPS Sharpen PS"));
      bool do_sharpen = g_rcas_sharpness > 0.f && copy_vs != nullptr && sharpen_ps != nullptr;
      if (do_sharpen && (!gd->cb_sharpen || gd->sharpen_w != w || gd->sharpen_h != h || gd->sharpen_amount != g_rcas_sharpness))
      {
         const float sp[4] = {(float)w, (float)h, g_rcas_sharpness, 0.f};
         if (CreateImmutableCB(native_device, sp, sizeof(sp), std::addressof(gd->cb_sharpen)))
         {
            gd->sharpen_w = w;
            gd->sharpen_h = h;
            gd->sharpen_amount = g_rcas_sharpness;
         }
      }
      do_sharpen = do_sharpen && gd->cb_sharpen;
      const auto sharpen = [&](ID3D11ShaderResourceView* source)
      {
         DrawStateStack<DrawStateStackType::FullGraphics> sharpen_state;
         sharpen_state.Cache(native_device_context, device_data.uav_max_count);

         ID3D11Buffer* scb = gd->cb_sharpen.get();
         native_device_context->PSSetConstantBuffers(0, 1, &scb);
         DrawCustomPixelShader(native_device_context, device_data.default_depth_stencil_state.get(), device_data.default_blend_state.get(), nullptr,
            copy_vs, sharpen_ps, source, ldr_rtv, w, h, false);

         sharpen_state.Restore(native_device_context);
      };

      // Copies the LDR into the snapshot both passes read (LDR format, so CopyResource matches): the LDR is also the chain's output
      const auto snapshot = [&]
      {
         uint4 snapshot_size{};
         DXGI_FORMAT snapshot_format = DXGI_FORMAT_UNKNOWN;
         if (gd->tex_input_encoded)
         {
            GetResourceInfo(gd->tex_input_encoded.get(), snapshot_size, snapshot_format);
         }
         if (!gd->srv_input_encoded || snapshot_size.x != w || snapshot_size.y != h || snapshot_format != ldr_format)
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
         gd->snapshot_frame = cb_luma_global_settings.FrameIndex;
         if (snapshot())
         {
            sharpen(gd->srv_input_encoded.get());
         }
         return;
      }

      // Shader-readiness gate (async loader / dev live-reload): skip SMAA this frame if any pass is missing, the chain is
      // all-or-nothing rather than partially configured.
      const bool smaa_ready = HasSMAAShaders(device_data);
      if (!smaa_ready)
      {
         return;
      }

      gd->smaa_frame = cb_luma_global_settings.FrameIndex;
      gd->snapshot_frame = cb_luma_global_settings.FrameIndex;

      // SMAA depth predication: plane-deviation edge-ness from the captured scene-color SRV (.a). Plain ULTRA fallback when
      // any input is missing (never scale 2.0 with a null texture).
      auto* pred_cs = FindShader(device_data.native_compute_shaders, CompileTimeStringHash("BL2TPS Depth Extract CS"));
      bool pred_ok = g_smaa_predication && gd->srv_scene_depth && pred_cs != nullptr;
      if (pred_ok)
      {
         // The extract CS maps texels 1:1, so a scene buffer of a different size would read a sub-rect and
         // misalign the predication signal against the LDR grid. Fall back to plain ULTRA instead.
         uint4 dinfo{};
         DXGI_FORMAT dfmt = DXGI_FORMAT_UNKNOWN;
         GetResourceInfo(gd->srv_scene_depth.get(), dinfo, dfmt);
         pred_ok = dinfo.x == w && dinfo.y == h;
      }
      if (pred_ok)
      {
         if (!gd->cb_pred || gd->pred_tolerance != g_smaa_pred_tolerance)
         {
            const float p[4] = {g_smaa_pred_tolerance, 0.f, 0.f, 0.f};
            if (CreateImmutableCB(native_device, p, sizeof(p), std::addressof(gd->cb_pred)))
            {
               gd->pred_tolerance = g_smaa_pred_tolerance;
            }
         }
         if (!gd->tex_pred || gd->pred_w != w || gd->pred_h != h)
         {
            gd->uav_pred.reset();
            gd->srv_pred.reset();
            gd->tex_pred.reset();
            if (CreateDefaultTex(native_device, w, h, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, std::addressof(gd->tex_pred), DXGI_FORMAT_R16_FLOAT))
            {
               native_device->CreateUnorderedAccessView(gd->tex_pred.get(), nullptr, gd->uav_pred.put());
               native_device->CreateShaderResourceView(gd->tex_pred.get(), nullptr, gd->srv_pred.put());
               gd->pred_w = w;
               gd->pred_h = h;
            }
         }
         pred_ok = gd->cb_pred && gd->uav_pred && gd->srv_pred;
      }

      if (do_sharpen)
      {
         gd->smaa_out_frame = cb_luma_global_settings.FrameIndex;
         if (!gd->tex_smaa_out || gd->smaa_out_w != w || gd->smaa_out_h != h)
         {
            gd->ReleaseSMAAOutput();
            if (CreateDefaultTex(native_device, w, h, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET, std::addressof(gd->tex_smaa_out), ldr_format))
            {
               native_device->CreateRenderTargetView(gd->tex_smaa_out.get(), nullptr, gd->tex_smaa_out_rtv.put());
               native_device->CreateShaderResourceView(gd->tex_smaa_out.get(), nullptr, gd->tex_smaa_out_srv.put());
               gd->smaa_out_w = w;
               gd->smaa_out_h = h;
            }
         }
         if (!gd->tex_smaa_out_rtv || !gd->tex_smaa_out_srv)
         {
            do_sharpen = false;
         }
      }

      if (!snapshot())
         return;

      // Predication extract: scene-color .a -> plane-deviation edge-ness in R16F (gd->tex_pred); see Luma_BL2TPS_DepthExtract.hlsl
      // for why this is an edge test rather than a depth rescale. Restoring the state stack before DrawSMAA is what makes the result
      // readable: an SRV of a resource still bound as a UAV reads as null.
      if (pred_ok)
      {
         DrawStateStack<DrawStateStackType::Compute> cs_state;
         cs_state.Cache(native_device_context, device_data.uav_max_count);
         ID3D11ShaderResourceView* ps_srv = gd->srv_scene_depth.get();
         ID3D11UnorderedAccessView* ps_uav = gd->uav_pred.get();
         ID3D11Buffer* ps_cb = gd->cb_pred.get();
         native_device_context->CSSetUnorderedAccessViews(0, 1, &ps_uav, nullptr);
         native_device_context->CSSetShaderResources(0, 1, &ps_srv);
         native_device_context->CSSetConstantBuffers(0, 1, &ps_cb);
         native_device_context->CSSetShader(pred_cs, nullptr, 0);
         native_device_context->Dispatch((w + 7) / 8, (h + 7) / 8, 1);
         cs_state.Restore(native_device_context);
      }

#if DEVELOPMENT
      // Both calibration aids read the mask the extract CS just wrote. The numeric one runs first so it still
      // reports while the debug view is on (that path returns early).
      if (pred_ok)
      {
         LogPredicationStats(native_device, native_device_context, gd);
      }

      if (pred_ok && g_smaa_pred_debug)
      {
         auto* copy_ps = FindShader(device_data.native_pixel_shaders, CompileTimeStringHash("Copy PS"));
         if (copy_vs != nullptr && copy_ps != nullptr)
         {
            // The mask is single-channel, so the core copy lands it in RED - unmistakably a debug view. It
            // REPLACES the antialiased frame rather than blending over it, hence the early return (which also
            // skips the metrics CB swap below, so there is nothing left to restore).
            DrawStateStack<DrawStateStackType::FullGraphics> debug_state;
            debug_state.Cache(native_device_context, device_data.uav_max_count);
            DrawCustomPixelShader(native_device_context, device_data.default_depth_stencil_state.get(), device_data.default_blend_state.get(), nullptr,
               copy_vs, copy_ps, gd->srv_pred.get(), ldr_rtv, w, h, false);
            debug_state.Restore(native_device_context);
            return;
         }
      }
#endif

      // SMAA T2x: SMAA into this frame's linear target with the velocity, then the resolve with the previous frame into the chain's
      // output. It needs this frame's motion vectors from the fill, at the LDR's size. Without them this frame is 1x and the next one
      // doesn't jitter.
      auto* const t2x_weight_ps = FindShader(device_data.native_pixel_shaders, CompileTimeStringHash("BL2TPS SMAA T2x Blending Weight Calculation PS"));
      auto* const t2x_blend_ps = FindShader(device_data.native_pixel_shaders, CompileTimeStringHash("BL2TPS SMAA T2x Neighborhood Blending PS"));
      auto* const t2x_resolve_ps = FindShader(device_data.native_pixel_shaders, CompileTimeStringHash("BL2TPS SMAA T2x Resolve PS"));
      bool t2x = gd->t2x_active && gd->mv_filled && gd->mv_srv && t2x_weight_ps && t2x_blend_ps && t2x_resolve_ps && copy_vs && GetViewTextureSize(gd->mv_srv.get()) == uint2{w, h};
      const uint32_t frame_index = cb_luma_global_settings.FrameIndex;
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
      const float metrics[12] = {1.f / (float)w, 1.f / (float)h, (float)w, (float)h, (pred_ok ? 2.0f : 1.0f), 0.f, 0.f, 0.f, subsample_indices, subsample_indices, subsample_indices, 0.f};
      if (!PatchedDraws::WriteDynamicConstants(native_device, native_device_context, std::addressof(gd->cb_smaa_metrics), metrics, sizeof(metrics)))
         return;

      // SMAA (3 passes). Metrics CB at VS+PS b1 (DrawSMAA restores VS/PS/SRVs/RTs, not cbuffers).
      ComPtr<ID3D11Buffer> vs_cb1_orig, ps_cb1_orig;
      native_device_context->VSGetConstantBuffers(1, 1, vs_cb1_orig.put());
      native_device_context->PSGetConstantBuffers(1, 1, ps_cb1_orig.put());
      ID3D11Buffer* metrics_cb = gd->cb_smaa_metrics.get();
      native_device_context->VSSetConstantBuffers(1, 1, &metrics_cb);
      native_device_context->PSSetConstantBuffers(1, 1, &metrics_cb);

      // The last pass of the chain renders straight into the LDR RTV, which is safe because SMAA and RCAS sample
      // the snapshot copies, never the LDR itself.
      ID3D11RenderTargetView* const output_rtv = (do_sharpen ? gd->tex_smaa_out_rtv.get() : ldr_rtv);
      const SMAAT2xPasses t2x_passes = {.blending_weight_calculation_ps = t2x_weight_ps, .neighborhood_blending_ps = t2x_blend_ps, .velocity = gd->mv_srv.get()};
      DrawSMAA(native_device, native_device_context, device_data,
         t2x ? gd->t2x_frame_rtvs[t2x_current].get() : output_rtv,
         gd->srv_input_encoded.get() /*neighborhood blend (filtered in linear light)*/,
         gd->srv_input_encoded.get() /*edge detection (gamma 2.2)*/,
         pred_ok ? gd->srv_pred.get() : nullptr /*predication signal*/,
         t2x ? &t2x_passes : nullptr);

      if (t2x)
      {
         // The resolve: t0 this frame, t1 the previous one or this one again without a history, t2 the motion vectors, s0 linear and
         // s1 point
         DrawStateStack<DrawStateStackType::FullGraphics> resolve_state;
         resolve_state.Cache(native_device_context, device_data.uav_max_count);
         // The patched draws may leave the motion vector target bound, and its view would then read as null
         native_device_context->OMSetRenderTargets(0, nullptr, nullptr);
         ID3D11ShaderResourceView* const resolve_srvs[2] = {gd->t2x_frame_srvs[t2x_history ? (t2x_current ^ 1) : t2x_current].get(), gd->mv_srv.get()};
         native_device_context->PSSetShaderResources(1, 2, resolve_srvs);
         ID3D11SamplerState* const resolve_samplers[2] = {device_data.sampler_state_linear.get(), device_data.sampler_state_point.get()};
         native_device_context->PSSetSamplers(0, 2, resolve_samplers);
         DrawCustomPixelShader(native_device_context, device_data.default_depth_stencil_state.get(), device_data.default_blend_state.get(), nullptr,
            copy_vs, t2x_resolve_ps, gd->t2x_frame_srvs[t2x_current].get(), output_rtv, w, h, false);
         resolve_state.Restore(native_device_context);
         gd->t2x_frame = frame_index;
      }

      if (do_sharpen)
      {
         sharpen(gd->tex_smaa_out_srv.get());
      }

      ID3D11Buffer* vs_cb1 = vs_cb1_orig.get();
      ID3D11Buffer* ps_cb1 = ps_cb1_orig.get();
      native_device_context->VSSetConstantBuffers(1, 1, &vs_cb1);
      native_device_context->PSSetConstantBuffers(1, 1, &ps_cb1);
   }
#endif // ENABLE_SMAA

   // ---- DLAA / FSR Native AA with motion vectors and jitter from patched shaders (ME1's path; vc4 layout in "MotionVectorPatches") ----
   // c0-c3, row vectors, translated by PreViewTranslation (UE3 draws in world space minus the camera position)
   static constexpr size_t kViewProjectionOffset = MotionVectorPatches::view_projection_row * 16;
   // ... c4 the camera position and c5 PreViewTranslation: the whole camera
   static constexpr size_t kCameraSize = 6 * 16;
   static constexpr size_t kPreViewTranslationOffset = (MotionVectorPatches::object_row_offset + 5) * 16;
   // c9: LocalToWorld's translation row, which includes PreViewTranslation
   static constexpr size_t kTranslationOffset = (MotionVectorPatches::object_row_offset + 9) * 16;
   // Skinned vertex shaders (bones from c6) hold LocalToWorld at c231-c234
   static constexpr size_t kSkinnedTranslationOffset = (MotionVectorPatches::object_row_offset + 234) * 16;
   // A camera ("mv_camera") comes from constants holding the translation row, so it always holds the whole camera
   static_assert(kTranslationOffset + 16 >= kViewProjectionOffset + kCameraSize);

   // An upscaler is picked and hasn't failed (it then gives way to SMAA until picked again). Fixed for the whole frame (see
   // "OnPresent"): a selection made after the motion vector state was set would otherwise run the upscaler on mixed state.
   static bool IsSRActive(DeviceData& device_data)
   {
      return GetGameDeviceData(device_data).sr_active;
   }

   // The upscalers that read the reactive mask the alpha blended draws write: FSR and Luma TAA (DLSS's current presets ignore it,
   // DLSS-Best-Practices TRN-2)
   static bool IsReactiveMaskUsed(DeviceData& device_data)
   {
      return g_sr_reactive_enable && IsSRActive(device_data) && (device_data.sr_type == SR::Type::FSR || device_data.sr_type == SR::Type::LumaTAA);
   }

   static std::array<float, 3> GetPreViewTranslation(const std::vector<uint8_t>& constants)
   {
      std::array<float, 3> translation;
      std::memcpy(translation.data(), constants.data() + kPreViewTranslationOffset, sizeof(translation));
      return translation;
   }

   // Last frame's view projection (row vectors) for this frame's PreViewTranslation: previous clip = (world + previous PVT) * previous
   // VP = (translated + previous PVT - PVT) * previous VP, so the translation row gains the delta times the first three rows
   static std::array<double, 16> GetPreviousViewProjection(const std::vector<uint8_t>& previous_camera, const std::vector<uint8_t>& camera)
   {
      const float* const previous = reinterpret_cast<const float*>(previous_camera.data() + kViewProjectionOffset);
      const std::array<float, 3> previous_translation = GetPreViewTranslation(previous_camera), translation = GetPreViewTranslation(camera);
      std::array<double, 16> view_projection;
      std::copy_n(previous, 16, view_projection.data());
      for (int row = 0; row < 3; row++)
      {
         const double delta = double(previous_translation[row]) - translation[row];
         for (int column = 0; column < 4; column++)
         {
            view_projection[12 + column] += delta * previous[row * 4 + column];
         }
      }
      return view_projection;
   }

   // The registered vc4 buffer's entry in "mv_constants_copies", null if the buffer isn't one; "size" its size if known (else 0)
   static Borderlands2GameDeviceData::RegisteredConstants* FindRegisteredBuffer(Borderlands2GameDeviceData* gd, uint64_t handle, UINT* size)
   {
      *size = 0;
      if (g_mv_buffer_filter && !gd->mv_filter_overflow)
      {
         for (uint32_t i = 0; i < gd->mv_filtered_buffer_count; i++)
         {
            if (gd->mv_filtered_buffers[i] == handle)
            {
               *size = gd->mv_filtered_buffer_sizes[i];
               return gd->mv_filtered_entries[i];
            }
         }
         return nullptr;
      }
      const auto registered = gd->mv_constants_copies.find(handle);
      return (registered != gd->mv_constants_copies.end() ? &registered->second : nullptr);
   }

   // A buffer registered in "mv_constants_copies" joins the filter ("g_mv_buffer_filter")
   static void AddFilteredBuffer(Borderlands2GameDeviceData* gd, ID3D11Buffer* buffer, Borderlands2GameDeviceData::RegisteredConstants* entry)
   {
      const uint32_t count = gd->mv_filtered_buffer_count;
      if (count >= Borderlands2GameDeviceData::kMaxFilteredBuffers)
      {
         gd->mv_filter_overflow = true;
         return;
      }
      D3D11_BUFFER_DESC desc;
      buffer->GetDesc(&desc);
#if DEVELOPMENT
      reshade::log::message(reshade::log::level::info, std::format("[BL2 MV] vc4 buffer {} registered: {} bytes, usage {}", count, desc.ByteWidth, int(desc.Usage)).c_str());
#endif
      gd->mv_filtered_buffer_sizes[count] = desc.ByteWidth;
      gd->mv_filtered_buffers[count] = reinterpret_cast<uint64_t>(buffer);
      gd->mv_filtered_entries[count] = entry;
      gd->mv_filtered_buffer_count = count + 1;
   }

   static UINT GetBufferSize(uint64_t handle, UINT known_size)
   {
      if (known_size != 0)
         return known_size;
      D3D11_BUFFER_DESC desc;
      reinterpret_cast<ID3D11Buffer*>(handle)->GetDesc(&desc);
      return desc.ByteWidth;
   }

   // A vc4 copy of "size" bytes from "bytes" (null: zeroed): one nobody held anymore at the last present ("g_mv_constants_pool"),
   // else a new one (a copy per motion vector draw after an upload)
   static std::shared_ptr<std::vector<uint8_t>> NewConstantsCopy(Borderlands2GameDeviceData& gd, const uint8_t* bytes, size_t size)
   {
      gd.mv_constants_made++;
      if (g_mv_constants_pool && !gd.mv_constants_pool_free.empty())
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
      if (g_mv_constants_pool)
      {
         gd.mv_constants_pool.push_back(copy);
      }
      return copy;
   }

#if DEVELOPMENT
   // The upscaler latches at the next present ("sr_active"): the settle covers the frame in between
   static void ApplyPerfTestMode(DeviceData& device_data, int mode_index)
   {
      auto& gd = GetGameDeviceData(device_data);
      const PerfTestMode& mode = perf_test_modes[mode_index];
      const PerfTestMode& previous_mode = perf_test_modes[Perf::g_test];
      if (!previous_mode.set_aa && mode.set_aa)
      {
         gd.perf_user_sr_type = device_data.sr_type;
         gd.perf_user_taa_quality = LumaTAA::quality.load();
         gd.perf_user_smaa = g_smaa_enable;
      }
      if (mode.set_aa || previous_mode.set_aa)
      {
         SetSRType(device_data, (mode.set_aa ? mode.sr_type : gd.perf_user_sr_type));
         LumaTAA::quality = ((mode.set_aa && mode.luma_taa_quality >= 0) ? mode.luma_taa_quality : gd.perf_user_taa_quality);
         device_data.sr_suppressed = false;
         g_smaa_enable = (mode.set_aa ? mode.smaa : gd.perf_user_smaa);
      }
      const auto sets_cpu = [](const PerfTestMode& test_mode)
      { return test_mode.vc4_filter_off || test_mode.vc4_pool_off || test_mode.blend_memo_off; };
      if (!sets_cpu(previous_mode) && sets_cpu(mode))
      {
         gd.perf_user_vc4_filter = g_mv_buffer_filter;
         gd.perf_user_vc4_pool = g_mv_constants_pool;
         gd.perf_user_blend_memo = g_blend_memo;
      }
      if (sets_cpu(mode) || sets_cpu(previous_mode))
      {
         g_mv_buffer_filter = gd.perf_user_vc4_filter && !mode.vc4_filter_off;
         g_mv_constants_pool = gd.perf_user_vc4_pool && !mode.vc4_pool_off;
         g_blend_memo = gd.perf_user_blend_memo && !mode.blend_memo_off;
      }
      // A helper started from now on writes its profile (one already running doesn't: its rows come once it restarts, at the next
      // upscaler change)
      if (Perf::g_test == 0 && mode_index != 0)
      {
         wchar_t temp[MAX_PATH];
         GetTempPathW(MAX_PATH, temp);
         gd.perf_helper_profile_path = std::format(L"{}Luma-Upscaler-profile-{}.csv", temp, GetCurrentProcessId());
         gd.perf_helper_profile_read = 0;
         SetEnvironmentVariableW(L"LUMA_UPSCALER_PROFILE", gd.perf_helper_profile_path.c_str());
      }
      else if (Perf::g_test != 0 && mode_index == 0)
      {
         SetEnvironmentVariableW(L"LUMA_UPSCALER_PROFILE", nullptr);
         DeleteFileW(gd.perf_helper_profile_path.c_str()); // Fails while a helper still writes it; the next test rewrites it
      }
      Perf::g_test = mode_index;
   }

   // The helper's evaluation in ms (GPU timestamps after its wait for the game's "in" and after the upscaler), the median of the
   // profile rows written since the last call; NaN without rows
   static double ReadHelperEvaluateMs(Borderlands2GameDeviceData& gd)
   {
      FILE* csv = _wfsopen(gd.perf_helper_profile_path.c_str(), L"rb", _SH_DENYNO);
      if (!csv)
         return std::numeric_limits<double>::quiet_NaN();
      _fseeki64(csv, 0, SEEK_END);
      const int64_t size = _ftelli64(csv);
      if (size < gd.perf_helper_profile_read)
      {
         gd.perf_helper_profile_read = 0; // A new helper rewrote it
      }
      std::string text(size_t(size - gd.perf_helper_profile_read), '\0');
      _fseeki64(csv, gd.perf_helper_profile_read, SEEK_SET);
      text.resize(fread(text.data(), 1, text.size(), csv));
      fclose(csv);
      std::vector<double> values;
      size_t start = 0;
      // Whole lines only (the last one may be half written); the header doesn't parse
      for (size_t end; (end = text.find('\n', start)) != std::string::npos; start = end + 1)
      {
         unsigned long long n, queued, waited, evaluated, frequency;
         long long cpu[4];
         int disjoint;
         if (sscanf_s(text.c_str() + start, "%llu,%lld,%lld,%lld,%lld,%llu,%llu,%llu,%llu,%d", &n, &cpu[0], &cpu[1], &cpu[2], &cpu[3], &queued, &waited, &evaluated, &frequency,
                &disjoint) == 10 &&
             !disjoint && frequency != 0 && evaluated >= waited)
         {
            values.push_back(double(evaluated - waited) * 1000.0 / double(frequency));
         }
      }
      gd.perf_helper_profile_read += int64_t(start);
      return values.empty() ? std::numeric_limits<double>::quiet_NaN() : Perf::Median(std::move(values));
   }

#endif

   // Motion vectors: a registered vc4 buffer mapped for a whole rewrite, remembered until its Unmap
   static void OnMapBufferRegion(reshade::api::device* device, reshade::api::resource resource, uint64_t offset, uint64_t size, reshade::api::map_access access, void** data)
   {
      DeviceData* const device_data = device->get_private_data<DeviceData>();
      if (!device_data || !device_data->game || !GetGameDeviceData(*device_data).mv_active || !data || !*data)
         return;
      auto& gd = GetGameDeviceData(*device_data);
#if DEVELOPMENT
      const Perf::HookTimer perf_timer{&gd.perf_window.hook_ns};
#endif
      UINT buffer_size;
      Borderlands2GameDeviceData::RegisteredConstants* const registered = FindRegisteredBuffer(&gd, resource.handle, &buffer_size);
      if (!registered)
         return;
#if DEVELOPMENT
      ASSERT_ONCE(gd.mv_constants_thread == 0 || gd.mv_constants_thread == GetCurrentThreadId());
#endif
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
#if DEVELOPMENT
      const Perf::HookTimer perf_timer{&gd.perf_window.hook_ns};
#endif
      UINT buffer_size;
      Borderlands2GameDeviceData::RegisteredConstants* const registered = FindRegisteredBuffer(&gd, resource.handle, &buffer_size);
      if (!registered || !registered->mapped)
         return;
      const uint8_t* const mapped = static_cast<const uint8_t*>(std::exchange(registered->mapped, nullptr));
      registered->latest.assign(mapped, mapped + GetBufferSize(resource.handle, buffer_size));
      registered->copy.reset();
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
#if DEVELOPMENT
      const Perf::HookTimer perf_timer{&gd.perf_window.hook_ns};
#endif
      UINT known_size;
      Borderlands2GameDeviceData::RegisteredConstants* const registered = FindRegisteredBuffer(&gd, resource.handle, &known_size);
      if (!registered)
         return false;
#if DEVELOPMENT
      ASSERT_ONCE(gd.mv_constants_thread == 0 || gd.mv_constants_thread == GetCurrentThreadId());
#endif
      const UINT buffer_size = GetBufferSize(resource.handle, known_size);
      if (offset >= buffer_size)
         return false;
      const size_t updated_size = size_t((std::min)(size, uint64_t(buffer_size) - offset));
      // Merged into the last upload of the same size, else into zeros
      std::vector<uint8_t>& latest = registered->latest;
      if (latest.size() != buffer_size)
      {
         latest.assign(buffer_size, 0);
      }
      std::memcpy(latest.data() + offset, data, updated_size);
      registered->copy.reset();
#if DEVELOPMENT
      gd.mv_stats.updates++;
#endif
      return false;
   }

   // The bound shader's motion vector version (or, "reactive" > 0, a pixel shader's mask one, see "ClassifyBoundBlend"), patched from
   // Core's bytecode copy on first use (null if it can't be, e.g. a vertex shader that doesn't place vertices with the view projection)
   template <typename T>
   static Borderlands2GameDeviceData::PatchedShader<T> GetMotionVectorShader(ID3D11Device* native_device, DeviceData& device_data,
      std::unordered_map<uint32_t, Borderlands2GameDeviceData::PatchedShader<T>>* shaders, uint32_t hash, reshade::api::pipeline pipeline, uint8_t reactive = 0)
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
      UINT translation_offset = kTranslationOffset;
      bool camera_relative_vertices = false;
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
               if (DXBC::ReadsConstantRow(code, desc->code_size, MotionVectorPatches::object_slot, kSkinnedTranslationOffset / 16))
               {
                  translation_offset = kSkinnedTranslationOffset;
               }
               // Camera relative: the position depends on vc4's view projection (c0-c3) alone, no translation from it (PreViewTranslation
               // c5, LocalToWorld c6-c9 or skinned c231-c234, terrain's c10-c13, a sprite's center...). The rows read don't tell: instanced
               // foliage reads c6 as its light map scale and bias, with its camera relative instance matrices in v4-v7 (0x726C5819, the dry
               // bushes' color pass), while other instanced meshes add c6 to their instance translation. 13 of the 874 dumped vertex
               // shaders: instanced foliage (0x35035418, 0x36AAA831, 0x4562B3A2, 0x55238014, 0x69DB8300, 0x726C5819, 0xAF966A18,
               // 0xBADE03D9, 0xCBBB7761, 0xD59C4FB4, 0xFD587A0F) and position only ones (0x483A369B, 0x5FC5315D).
               camera_relative_vertices = DXBC::PositionDependsOnlyOnConstantRows(code, desc->code_size, MotionVectorPatches::object_slot, kViewProjectionOffset / 16, kViewProjectionOffset / 16 + 3);
            }
            else if (reactive != 0)
            {
               patched = MotionVectorPatch::PatchPixelShaderReactive(code, desc->code_size, MotionVectorPatches::layout, MotionVectorPatches::reactive_slot, reactive == 2, &error);
            }
            else
            {
               patched = MotionVectorPatch::PatchPixelShader(code, desc->code_size, MotionVectorPatches::layout, &error);
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
         {
            hr = native_device->CreatePixelShader(patched.data(), patched.size(), nullptr, &shader);
         }
         if (FAILED(hr))
         {
            error = std::format("create 0x{:08X}", uint32_t(hr));
         }
      }
      // Failures in every build (bug reports), except the expected screen space refusals; every patched shader only in development
      const bool screen_space = error.starts_with("screen space");
      if (DEVELOPMENT || (!shader && !screen_space))
      {
         reshade::log::message((shader || screen_space) ? reshade::log::level::info : reshade::log::level::warning,
            std::format("[BL2 MV] {} 0x{:08X} {}", vertex ? "VS" : (reactive != 0 ? "PS (mask)" : "PS"), hash, shader ? "patched" : error).c_str());
      }
      const std::unique_lock lock(gd.mv_mutex);
      return shaders->try_emplace(hash, Borderlands2GameDeviceData::PatchedShader<T>{shader, read_size, translation_offset, camera_relative_vertices}).first->second;
   }

   // The bound vertex shader's patched version (null shader if refused), looked up again only when the game's shader changes
   static const Borderlands2GameDeviceData::PatchedShader<ID3D11VertexShader>& GetPatchedVertexShader(ID3D11Device* native_device, CommandListData& cmd_list_data, DeviceData& device_data, uint32_t hash)
   {
      auto& gd = GetGameDeviceData(device_data);
      if (hash != gd.mv_last_vertex_shader_hash)
      {
         gd.mv_last_vertex_shader = GetMotionVectorShader(native_device, device_data, &gd.mv_vertex_shaders, hash, cmd_list_data.pipeline_state_original_vertex_shader);
         gd.mv_last_vertex_shader_hash = hash;
      }
      return gd.mv_last_vertex_shader;
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
#if DEVELOPMENT
      if (auto* const perf_queries = gd.perf_timestamps.frame)
      {
         perf_queries->Mark(native_device_context, PERF_SCENE_START);
      }
#endif
      gd.mv_depth.reset();
      dsv->GetResource(&gd.mv_depth);
      gd.mv_scene_color.reset();
      gd.mv_scene_rtv.reset();
      gd.jitter_dsv = nullptr;
      gd.jitter_depth_stencil_state = nullptr;
      gd.jitter_depth_test = true;
      gd.mv_accepted_dsv = nullptr;
      gd.mv_blend_state = nullptr;
      gd.mv_blend_opaque = true;
      gd.mv_reactive_blend = 0;
      // Halton (2, 3) over the upscaler's phase count; pixels to NDC (y up). None until the upscaler is ready (the bridge's helper
      // starting shows the scene as it is, antialiased with SMAA).
      const SR::InstanceData* sr_instance_data = (IsSRActive(device_data) ? device_data.GetSRInstanceData() : nullptr);
      if (sr_instance_data && !sr_implementations[device_data.sr_type]->IsReady(sr_instance_data))
      {
         sr_instance_data = nullptr;
      }
      const unsigned int phase = cb_luma_global_settings.FrameIndex % (sr_instance_data ? (std::max)(sr_implementations[device_data.sr_type]->GetJitterPhases(sr_instance_data), 1) : SR::GetDefaultJitterPhases());
      gd.mv_jitter = (((sr_instance_data && gd.mv_jitter_allowed) || g_mv_force_jitter) && GetPerfMotionVectorDraws() != 0) ? std::array<float, 2>{SR::HaltonSequence(phase, 2), SR::HaltonSequence(phase, 3)} : std::array<float, 2>{};
      // SMAA T2x: the sample offsets (0.25, 0.25) then (-0.25, -0.25) in y down pixels, for "SMAA.hlsl"'s T2x subsample indices 1 and
      // 2. That is its table read with y up, which measured better (docs/SMAA-Lab.md). The projection moves the other way.
      gd.t2x_phase = ((gd.t2x_active && gd.t2x_frames[0] && gd.t2x_frame + 1 == cb_luma_global_settings.FrameIndex && GetPerfMotionVectorDraws() != 0) ? int(cb_luma_global_settings.FrameIndex & 1) : -1);
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

   // vc4's tracked binding (see "bound_object_buffer")
   static void OnPushConstantBuffers(reshade::api::command_list* cmd_list, reshade::api::shader_stage stages, reshade::api::pipeline_layout layout, uint32_t layout_param, const reshade::api::descriptor_table_update& update)
   {
      constexpr uint32_t slot = MotionVectorPatches::object_slot;
      if ((stages & reshade::api::shader_stage::vertex) == 0 || update.type != reshade::api::descriptor_type::constant_buffer || slot < update.binding || slot >= update.binding + update.count)
         return;
      DeviceData* const device_data = cmd_list->get_device()->get_private_data<DeviceData>();
      const CommandListData* const cmd_list_data = cmd_list->get_private_data<CommandListData>();
      if (!device_data || !device_data->game || !cmd_list_data || !cmd_list_data->is_primary)
         return;
      const auto* const ranges = static_cast<const reshade::api::buffer_range*>(update.descriptors);
      GetGameDeviceData(*device_data).bound_object_buffer = reinterpret_cast<ID3D11Buffer*>(ranges[slot - update.binding].buffer.handle);
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
   // "query" writes to its argument (an object pointer to take over). "g_bound_state_check" counts the times they differ.
   template <typename T, typename Query>
   static T* BoundState(Borderlands2GameDeviceData* gd, T* tracked, const Query& query)
   {
      if (g_bound_state_tracking && gd->bound_states_tracked && !g_bound_state_check)
         return tracked;
      com_ptr<T> queried;
      query(&queried);
#if DEVELOPMENT
      if (g_bound_state_check && gd->bound_states_tracked && queried.get() != tracked)
      {
         gd->mv_stats.bound_state_mismatches++;
      }
#endif
      return queried.get();
   }

   // Classifies the bound blend state (cached in "mv_blend_state"): "mv_blend_opaque" for the motion vectors (additive lights, decals
   // and translucents keep the motion vectors of what's behind them, and so do colorless draws: the occlusion query bounding boxes),
   // and "mv_reactive_blend" for FSR's masks: 0 not alpha blended (opaque, additive ONE/ONE lights, modulated shadows), 1 alpha
   // blended (SRC_ALPHA / INV_SRC_ALPHA: smoke, glass, water; reactive and transparency & composition), 2 additive (SRC_ALPHA / ONE:
   // sparks, glows; reactive)
   static void ClassifyBoundBlend(ID3D11DeviceContext* native_device_context, Borderlands2GameDeviceData* gd)
   {
      ID3D11BlendState* const blend_state = BoundState(gd, gd->bound_blend_state, [&](ID3D11BlendState** state)
         { native_device_context->OMGetBlendState(state, nullptr, nullptr); });
      if (blend_state == gd->mv_blend_state)
      {
         return;
      }
      D3D11_BLEND_DESC blend_desc = CD3D11_BLEND_DESC(D3D11_DEFAULT);
      if (blend_state)
      {
         blend_state->GetDesc(&blend_desc);
      }
      const D3D11_RENDER_TARGET_BLEND_DESC& rt0 = blend_desc.RenderTarget[0];
      gd->mv_blend_opaque = rt0.RenderTargetWriteMask != 0 && (!rt0.BlendEnable || (rt0.SrcBlend == D3D11_BLEND_ONE && rt0.DestBlend == D3D11_BLEND_ZERO && rt0.BlendOp == D3D11_BLEND_OP_ADD));
      gd->mv_reactive_blend = (!rt0.BlendEnable || rt0.SrcBlend != D3D11_BLEND_SRC_ALPHA) ? 0 : (rt0.DestBlend == D3D11_BLEND_ONE ? 2 : 1);
      gd->mv_blend_state = blend_state;
   }

   // The bound depth stencil state tests depth (meshes do; full screen passes and composites don't), cached by state
   static bool IsDepthTested(ID3D11DeviceContext* native_device_context, Borderlands2GameDeviceData* gd)
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

#if DEVELOPMENT
#define MV_REJECT(reason) \
   ([&](auto& gd) { gd.mv_stats.rejected[reason]++; gd.mv_draw_reject = int(reason); return false; }(GetGameDeviceData(device_data)))
#else
#define MV_REJECT(reason) false
#endif

#if DEVELOPMENT
   // "mv.tiebreak_collisions" explained: the objects of one draw key with the same transform but other constants (their previous
   // frame match is arbitrary), and the vc4 rows that differ (c<N>: the D3D9 constant, see "MotionVectorPatches")
   static void LogTieBreakCollisions(const decltype(Borderlands2GameDeviceData::mv_objects)& objects_by_key)
   {
      uint32_t logged = 0;
      for (const auto& [key, objects] : objects_by_key)
      {
         for (size_t i = 0; i < objects.size() && logged < 8; i++)
         {
            for (size_t j = i + 1; j < objects.size(); j++)
            {
               const auto& a = objects[i];
               const auto& b = objects[j];
               if (a.transform != b.transform || PatchedDraws::SameBytes(a.constants, b.constants) || !a.constants || !b.constants)
               {
                  continue;
               }
               std::string rows;
               const size_t row_count = (std::min)(a.constants->size(), b.constants->size()) / 16;
               uint32_t differing = 0;
               for (size_t row = 0; row < row_count; row++)
               {
                  if (std::memcmp(a.constants->data() + row * 16, b.constants->data() + row * 16, 16) == 0)
                  {
                     continue;
                  }
                  if (differing++ < 12)
                  {
                     rows += std::format(" c{}", int(row) - int(MotionVectorPatches::object_row_offset));
                  }
               }
               reshade::log::message(reshade::log::level::info,
                  std::format("[BL2 MV] tie-break collision: key 0x{:016X} VS 0x{:08X} PS 0x{:08X}, {} objects, translation ({:.1f}, {:.1f}, {:.1f}), sizes {}/{}, {} rows differ:{}", key, a.vertex_shader, a.pixel_shader,
                     objects.size(), a.transform[9], a.transform[10], a.transform[11], a.constants->size(), b.constants->size(), differing, rows)
                     .c_str());
               logged++;
               break;
            }
         }
      }
   }
#endif

   // Draws an opaque draw into the fp16 scene (the scene target alone, output sized, with the scene depth) with the patched shaders,
   // adding the motion vector target ("target_slot", past the game's) and the previous frame's vc4 ("previous_slots"). False if it
   // can't (the draw then goes to "DrawWithJitter").
   static bool DrawWithMotionVectors(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, CommandListData& cmd_list_data, DeviceData& device_data,
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
      // Depth tested meshes only: the loading screen's stencil masked composites (PS 0xD81C32BD, 0xC121788F) draw into another fp16
      // target before its weapon and would take the scene's place
      if (!IsDepthTested(native_device_context, &gd))
         return MV_REJECT(REJECT_DEPTH_TEST);
      ClassifyBoundBlend(native_device_context, &gd);
      if (!gd.mv_blend_opaque)
         return MV_REJECT(REJECT_BLEND);
      // Known targets: checked, and the motion vector target built for them
      if (rtvs[0] != gd.mv_scene_rtv || dsv != gd.mv_accepted_dsv)
      {
         com_ptr<ID3D11Resource> depth;
         dsv->GetResource(&depth);
         com_ptr<ID3D11Resource> color;
         rtvs[0]->GetResource(&color);
         if (depth != gd.mv_depth || !color || (gd.mv_scene_color && color != gd.mv_scene_color))
            return MV_REJECT(REJECT_OTHER_DEPTH_COLOR);
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
         gd.mv_stats.rejected_dimension = rtv_desc.ViewDimension;
         gd.mv_stats.rejected_width = color_desc.Width;
         gd.mv_stats.rejected_height = color_desc.Height;
#endif
         // The fill reads the scene: it needs a shader resource view. Filtered by the resource, not the view (dgVoodoo binds single-slice
         // array views).
         if (rtv_desc.Format != DXGI_FORMAT_R16G16B16A16_FLOAT || color_desc.ArraySize != 1 || color_desc.SampleDesc.Count != 1 || (color_desc.BindFlags & D3D11_BIND_SHADER_RESOURCE) == 0)
            return MV_REJECT(REJECT_FORMAT);
         const uint2 size = {color_desc.Width, color_desc.Height};
         if (size.x != device_data.output_resolution.x || size.y != device_data.output_resolution.y)
            return MV_REJECT(REJECT_SIZE);
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
      if (const uint32_t pixel_shader_hash = original_shader_hashes.pixel_shaders[0]; pixel_shader_hash != gd.mv_last_pixel_shader_hash)
      {
         gd.mv_last_pixel_shader = GetMotionVectorShader(native_device, device_data, &gd.mv_pixel_shaders, pixel_shader_hash, cmd_list_data.pipeline_state_original_pixel_shader).shader.get();
         gd.mv_last_pixel_shader_hash = pixel_shader_hash;
      }
      ID3D11PixelShader* const pixel_shader = gd.mv_last_pixel_shader;
      if (!vertex_shader || !pixel_shader || !gd.mv_jitter_buffer)
         return MV_REJECT(REJECT_SHADERS);
      if (std::exchange(gd.mv_frame_ended, false))
      {
         gd.mv_fill_pending = gd.mv_uav && gd.mv_device_depth_uav && FindShader(device_data.native_compute_shaders, CompileTimeStringHash("BL2TPS Motion Vector Fill CS")) != nullptr;
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
         gd.mv_previous_camera = previous_valid ? gd.mv_camera : nullptr;
         gd.mv_camera = nullptr;
         gd.mv_frame_index = cb_luma_global_settings.FrameIndex;
         // Swapped, not rebuilt: the lists keep their nodes and capacity (an empty list matches nothing); keys drawn in neither of the
         // last two frames go
         gd.mv_previous_objects.swap(gd.mv_objects);
#if DEVELOPMENT
         gd.mv_stats.tiebreak_collisions = PatchedDraws::CountTieBreakCollisions(gd.mv_previous_objects, [](const auto& a, const auto& b)
            { return PatchedDraws::SameBytes(a.constants, b.constants); });
         if (gd.mv_stats.tiebreak_collisions != 0 && Perf::g_test == 0 && cb_luma_global_settings.FrameIndex % 300 == 0)
         {
            LogTieBreakCollisions(gd.mv_previous_objects);
         }
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

      // The game's vc4 (object, camera and bones in one). The slots added past it stay bound after the draw: no translated shader reads
      // a constant buffer past b4.
      ID3D11Buffer* const current = BoundState(&gd, gd.bound_object_buffer, [&](ID3D11Buffer** buffer)
         { native_device_context->VSGetConstantBuffers(MotionVectorPatches::object_slot, 1, buffer); });
      // The buffer's CPU copy (null until its first upload); the lookup registers it for a copy at every upload. From the mirror instead
      // when it runs: the rows read here must be known, the camera (c0-c5) and the LocalToWorld rows (zero motion otherwise, as without
      // a copy). Not every row the shader may read: a skinned mesh sets only its own bones of c6-c230, the rest of vc4 holds older
      // draws' values its vertices don't index (the mirror has zeros there).
      const UINT read_size = gd.mv_last_vertex_shader.read_size;
      Borderlands2GameDeviceData::ConstantsCopy constants;
      const VertexConstantMirror* const mirror = (g_vc4_mirror ? gd.vertex_constants : nullptr);
      if (mirror)
      {
         const uint32_t transform_row = gd.mv_last_vertex_shader.translation_offset / 16 - 3;
         const auto known = [&](uint32_t first_row, uint32_t row_count)
         { return first_row + row_count <= VertexConstantMirror::ROWS && !std::memchr(mirror->known + first_row, 0, row_count); };
         if (known(kViewProjectionOffset / 16, kCameraSize / 16) && known(transform_row, 4))
         {
            if (!gd.vertex_constants_copy || gd.vertex_constants_copy_generation != mirror->generation)
            {
               gd.vertex_constants_copy = NewConstantsCopy(gd, reinterpret_cast<const uint8_t*>(mirror->rows), sizeof(mirror->rows));
               gd.vertex_constants_copy_generation = mirror->generation;
            }
            constants = gd.vertex_constants_copy;
         }
#if DEVELOPMENT
         else
         {
            gd.mv_stats.vc4_mirror_unknown++;
         }
#endif
      }
      if (current && (!mirror || g_vc4_mirror_check))
      {
         const auto [copy, registered] = gd.mv_constants_copies.try_emplace(reinterpret_cast<uint64_t>(current));
         Borderlands2GameDeviceData::RegisteredConstants& entry = copy->second;
         if (registered)
         {
            AddFilteredBuffer(&gd, current, &entry);
         }
#if DEVELOPMENT
         // The bytes the shader may read, of the rows the mirror knows (the rest it holds as zero). A row it doesn't read differs
         // legitimately: dgVoodoo uploads only what it reads, so vc4 keeps an older draw's value there while the mirror has the game's
         // own (the skinned VS 0x400DB863 doesn't read c5, ~45 draws a frame in BL2: they count as mismatches here, harmless)
         if (mirror && constants && entry.latest.size() == sizeof(mirror->rows))
         {
            const uint32_t read_rows = (read_size != 0 ? (std::min)((read_size + 15) / 16, VertexConstantMirror::ROWS) : VertexConstantMirror::ROWS);
            const uint32_t transform_row = gd.mv_last_vertex_shader.translation_offset / 16 - 3;
            bool mismatch = false, used_mismatch = false;
            for (uint32_t row = 0; row < read_rows; row++)
            {
               if (!mirror->known[row] || std::memcmp(mirror->rows[row], entry.latest.data() + row * 16, 16) == 0)
                  continue;
               if (!mismatch)
               {
                  gd.mv_stats.vc4_mirror_mismatch_row = row;
                  gd.mv_stats.vc4_mirror_mismatch_vs = uint32_t(original_shader_hashes.vertex_shaders[0]);
               }
               mismatch = true;
               // The rows read here: the camera and the LocalToWorld rows
               used_mismatch |= (row >= kViewProjectionOffset / 16 && row < (kViewProjectionOffset + kCameraSize) / 16) || (row >= transform_row && row < transform_row + 4);
            }
            gd.mv_stats.vc4_mirror_mismatches += mismatch;
            gd.mv_stats.vc4_mirror_used_mismatches += used_mismatch;
         }
#endif
         if (!mirror)
         {
            if (!entry.copy && !entry.latest.empty())
            {
               entry.copy = NewConstantsCopy(gd, entry.latest.data(), entry.latest.size());
            }
            constants = entry.copy;
         }
      }
      // The previous frame's vc4: the same object's from last frame, else this draw's with last frame's camera (no object motion).
      // None (no CPU copy yet, another camera): the current one (zero motion).
      const std::vector<uint8_t>* upload = nullptr;
      if (constants && constants->size() >= kTranslationOffset + 16)
      {
         // The frame's camera: its first motion vector draw's
         if (!gd.mv_camera)
         {
            gd.mv_camera = constants;
         }
         const bool frame_camera = std::memcmp(constants->data() + kViewProjectionOffset, gd.mv_camera->data() + kViewProjectionOffset, kCameraSize) == 0;

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
#if DEVELOPMENT
         if (g_bound_state_check && gd.bound_states_tracked && (vertex_offset != gd.bound_vertex_offset || index_offset != gd.bound_index_offset))
         {
            gd.mv_stats.bound_state_mismatches++;
         }
#endif
         const DrawDispatchData& draw_data = last_draw_dispatch_data;
         uint64_t key = 0;
         for (const uint64_t value : {uint64_t(original_shader_hashes.vertex_shaders[0]), uint64_t(original_shader_hashes.pixel_shaders[0]), reinterpret_cast<uint64_t>(vertex_buffer), uint64_t(vertex_offset),
                 reinterpret_cast<uint64_t>(index_buffer), uint64_t(index_offset), uint64_t(draw_data.index_count), uint64_t(draw_data.first_index), uint64_t(uint32_t(draw_data.vertex_offset)),
                 uint64_t(draw_data.vertex_count), uint64_t(draw_data.first_vertex)})
         {
            HashCombine(key, value);
         }
         // LocalToWorld (its translation in world space: it includes the draw's PreViewTranslation) separates objects that share a key
         // (props), as a tie-break only. The axes too: modular pieces share a pivot and differ only by rotation (BL2's arches and
         // discs, measured). Skinned meshes' c9 is a bone row, the same for copies of a model (ME1's holstered weapons).
         const size_t translation_offset = (constants->size() >= gd.mv_last_vertex_shader.translation_offset + 16 ? gd.mv_last_vertex_shader.translation_offset : kTranslationOffset);
         PatchedDraws::ObjectTransform transform = PatchedDraws::ReadRowVectorTransform(constants->data() + translation_offset - 3 * 16);
         const std::array<float, 3> view_translation = GetPreViewTranslation(*constants);
         for (int i = 0; i < 3; i++)
         {
            transform[9 + i] -= view_translation[i];
         }

         // ponytail: linear search among the key's candidates (a handful at most); a spatial lookup if big crowds share a mesh
         const Borderlands2GameDeviceData::MotionVectorObject* match = nullptr;
         if (const auto previous = gd.mv_previous_objects.find(key); g_mv_match_objects && previous != gd.mv_previous_objects.end())
         {
            match = PatchedDraws::FindNearest(previous->second, transform, [&](const auto& candidate)
               { return candidate.constants->size() == constants->size(); });
         }
         // "source" (only what the shader reads, at least the camera) with the view projection of "previous_camera" for this frame's
         // PreViewTranslation
         const auto upload_moved_camera = [&](const std::vector<uint8_t>& source, const std::vector<uint8_t>& previous_camera)
         {
            const size_t copy_size = (read_size != 0 ? std::clamp<size_t>(read_size, kViewProjectionOffset + kCameraSize, source.size()) : source.size());
            gd.mv_camera_only_copy.assign(source.begin(), source.begin() + copy_size);
            const std::array<double, 16> previous_view_projection = GetPreviousViewProjection(previous_camera, *constants);
            float* const view_projection = reinterpret_cast<float*>(gd.mv_camera_only_copy.data() + kViewProjectionOffset);
            for (int i = 0; i < 16; i++)
            {
               view_projection[i] = float(previous_view_projection[i]);
            }
            upload = &gd.mv_camera_only_copy;
         };
         if (match)
         {
            // Last frame's list outlives the draw ("mv_previous_objects" only changes at the next frame start). Camera relative vertices
            // are already translated by this frame's PreViewTranslation: last frame's constants, with its view projection moved to it.
            if (gd.mv_last_vertex_shader.camera_relative_vertices)
            {
               upload_moved_camera(*match->constants, *match->constants);
            }
            else
            {
               upload = &*match->constants;
            }
#if DEVELOPMENT
            gd.mv_stats.matched++;
#endif
         }
         else if (frame_camera && gd.mv_previous_camera)
         {
            // Not found, drawn with the frame's camera: its own constants with last frame's view projection (camera motion only)
            upload_moved_camera(*constants, *gd.mv_previous_camera);
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
         auto& object = gd.mv_objects[key].emplace_back(transform, std::move(constants));
#if DEVELOPMENT
         object.vertex_shader = uint32_t(original_shader_hashes.vertex_shaders[0]);
         object.pixel_shader = uint32_t(original_shader_hashes.pixel_shaders[0]);
#endif
      }
#if DEVELOPMENT
      else
      {
         gd.mv_stats.uncopied++;
      }
#endif
      // "Performance Test" modes without motion vector draws: the draw goes to "DrawWithJitter" (jittered or, without jitter, untouched)
      if (GetPerfMotionVectorDraws() < 2)
         return false;
      ID3D11Buffer* const previous_current[] = {current};
      gd.mv_previous_constants.Bind(native_device, native_device_context, MotionVectorPatches::previous_slots, {&upload, 1}, previous_current, "BL2", {&read_size, 1});
      ID3D11Buffer* const jitter = gd.mv_jitter_buffer.get();
      native_device_context->VSSetConstantBuffers(MotionVectorPatches::jitter_slot, 1, &jitter);
      // Left bound after the draw (set directly, bypassing Core's state tracking): the game's next draws either bind their own targets
      // and shaders, or are motion vector draws too. The game's pixel shaders write o0 only, so the target keeps its contents.
      if (rtvs[MotionVectorPatches::target_slot] != gd.mv_rtv)
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

   // Jitter for the scene's mesh draws without motion vectors (patched vertex shader, game pixel shader): depth passes, lights, decals,
   // translucents. Every draw depth tested against the scene takes the same jitter, or jittered and unjittered depths of the same
   // surface fail each other's test. False if it can't (the draw runs untouched).
   static bool DrawWithJitter(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, CommandListData& cmd_list_data, DeviceData& device_data,
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
            reactive_shader = GetMotionVectorShader(native_device, device_data, &gd.mv_reactive_pixel_shaders[blend - 1], original_shader_hashes.pixel_shaders[0], cmd_list_data.pipeline_state_original_pixel_shader, blend).shader.get();
         }
      }

      // The patched vertex shader and the jitter stay bound after the draw (see "DrawWithMotionVectors"), with the game's pixel shader
      // (a motion vector draw's is put back) or its mask version, and the mask target
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

   // DLAA or FSR 3 Native AA on the jittered scene, its depth (from its alpha, see the fill) and the motion vectors; the result goes back
   // into the scene's color (and its copy, see "mv_scene_copy"), its alpha (the post passes' encoded depth) kept. False if it didn't draw
   // (missing input, or the upscaler failed).
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
      const SR::ViewProjectionCamera camera = SR::GetViewProjectionCamera(reinterpret_cast<const float*>(gd.mv_camera->data() + kViewProjectionOffset), /* row_vectors */ true);

      const SR::SettingsData settings_data = {
         .output_width = scene_desc.Width,
         .output_height = scene_desc.Height,
         .render_width = scene_desc.Width,
         .render_height = scene_desc.Height,
         .hdr = true,
         // The motion vectors are UV deltas, previous minus current
         .mvs_x_scale = float(scene_desc.Width),
         .mvs_y_scale = float(scene_desc.Height),
         // The scene is not exposed yet: the tonemap multiplies it by its exposure (ImageAdjustments2.w, 1.0 to 1.8) after the
         // upscaler. DLSS's auto exposure covers that; FSR's clips highlights (FSR-Best-Practices FIN-3), so it runs at exposure 1.
         .auto_exposure = device_data.sr_type != SR::Type::FSR,
         // DLAA on scene-referred HDR input takes preset J or K, never L or M (DLSS-Best-Practices PRE-4): "Default" picks K (11), so
         // NVIDIA can't change it over the air; an explicit choice is kept
         .render_preset = (dlss_render_preset != 0 ? dlss_render_preset : 11u),
      };
      sr_implementations[device_data.sr_type]->UpdateSettings(sr_instance_data, native_device_context, settings_data);

      // FSR requires a FOV (it errors on 0), and both take near and far: a camera without an up axis or a usable projection keeps the
      // last ones
      if (camera.vert_fov > 0.0)
      {
         gd.sr_vert_fov = float(camera.vert_fov);
      }
      // A finite far (depth 1) even when the projection has none (FSR's context is FFX_FSR3_ENABLE_DEPTH_INFINITE only with inverted
      // depth)
      if (camera.near_plane > 0.0)
      {
         gd.sr_near_plane = float(camera.near_plane);
         gd.sr_far_plane = float(camera.far_plane);
      }
      // The reactive masks (FSR and Luma TAA; DLSS ignores them)
      ID3D11Resource* const bias_mask = ((reactive_mask && g_sr_reactive_pass) ? gd.mv_reactive.get() : nullptr);
      const SR::SuperResolutionImpl::DrawData draw_data = {
         .reset = device_data.force_reset_sr,
         .output_color = device_data.sr_output_color.get(),
         .source_color = scene.get(),
         .motion_vectors = gd.mv_texture.get(),
         .depth_buffer = gd.mv_device_depth.get(),
         .bias_mask = bias_mask,
         .transparency_alpha = (g_sr_tc_from_mask ? bias_mask : ((reactive_mask && g_sr_reactive_pass) ? gd.mv_transparency.get() : nullptr)),
         // As applied (pixels, +y down)
         .jitter_x = gd.mv_jitter[0],
         .jitter_y = gd.mv_jitter[1],
         .vert_fov = gd.sr_vert_fov,
         .near_plane = gd.sr_near_plane,
         .far_plane = gd.sr_far_plane,
      };
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
#if DEVELOPMENT
      auto* const perf_queries = gd.perf_timestamps.frame;
      const bool perf_end_parts = perf_queries && perf_queries->Marked(PERF_SCENE_END) && !perf_queries->Marked(PERF_SCENE_TAIL_END);
      if (perf_end_parts)
      {
         perf_queries->Mark(native_device_context, PERF_UPSCALER_END);
      }
#endif
      // The RGB write mask keeps the scene's encoded depth; the copy is the game's resolve of the same scene, written in the same pass
      // (with a view like the scene's: dgVoodoo's are single slice arrays), else copied whole after it
      ID3D11RenderTargetView* copy_rtv = nullptr;
      auto* const copy_back_ps = FindShader(device_data.native_pixel_shaders, CompileTimeStringHash("BL2TPS Copy Back PS"));
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
#if DEVELOPMENT
      if (perf_end_parts)
      {
         perf_queries->Mark(native_device_context, PERF_COPY_END);
      }
#endif
      if (gd.mv_scene_copy && !copy_rtv)
      {
         native_device_context->CopyResource(gd.mv_scene_copy.get(), scene.get());
      }
      // Not while the bridge's helper starts (the color copied as it is): SMAA stays on and the next frame resets
      device_data.has_drawn_sr = sr_implementations[device_data.sr_type]->IsReady(sr_instance_data);
      return true;
   }

   // Ends the scene at its first post pass: the depth and camera motion fill (see "Luma_BL2TPS_MotionVectorFill.hlsl"), then the
   // upscaler, both before that pass reads the scene
   static void EndScene(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, DeviceData& device_data)
   {
      auto& gd = GetGameDeviceData(device_data);
#if DEVELOPMENT
      auto* const perf_queries = gd.perf_timestamps.frame;
      if (perf_queries && perf_queries->Marked(PERF_SCENE_START))
      {
         perf_queries->Mark(native_device_context, PERF_SCENE_END);
      }
#endif
      gd.mv_scene_open = false;
      gd.mv_scene_done = true;
      DrawStateStack<DrawStateStackType::FullGraphics> graphics_state;
      DrawStateStack<DrawStateStackType::Compute> compute_state;
      graphics_state.Cache(native_device_context, device_data.uav_max_count);
      compute_state.Cache(native_device_context, device_data.uav_max_count);
      auto* const fill_shader = FindShader(device_data.native_compute_shaders, CompileTimeStringHash("BL2TPS Motion Vector Fill CS"));
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
      // The reactive and transparency & composition masks, written by the fill from what the alpha blended draws wrote
      const bool reactive = IsReactiveMaskUsed(device_data) && gd.mv_reactive_target_srv && !g_sr_reactive_skip_fill;
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
      if (std::exchange(gd.mv_fill_pending, false) && fill_shader && gd.mv_camera && gd.mv_scene_color &&
          (gd.mv_scene_srv || SUCCEEDED(native_device->CreateShaderResourceView(gd.mv_scene_color.get(), nullptr, &gd.mv_scene_srv))))
      {
         // Current clip space to the previous frame's: previous * inverse(current) for column vectors (vc4 holds the row vector matrix,
         // transposed here), the previous one moved to this frame's PreViewTranslation, in double
         const float* const view_projection = reinterpret_cast<const float*>(gd.mv_camera->data() + kViewProjectionOffset);
         Math::Matrix44D current, previous;
         current.SetIdentity();
         previous.SetIdentity();
         if (gd.mv_previous_camera)
         {
            std::copy_n(view_projection, 16, current.GetData());
            const std::array<double, 16> previous_view_projection = GetPreviousViewProjection(*gd.mv_previous_camera, *gd.mv_camera);
            std::copy_n(previous_view_projection.data(), 16, previous.GetData());
            current.Transpose();
            previous.Transpose();
            current.Invert();
         }
         const Math::Matrix44D reprojection = previous * current;
         const SR::ViewProjectionCamera camera = SR::GetViewProjectionCamera(view_projection, /* row_vectors */ true);
         float constants[24] = {}; // A multiple of 16 bytes
         for (int i = 0; i < 16; i++)
         {
            constants[i] = float(reprojection.GetData()[i]);
         }
         constants[16] = gd.mv_jitter_ndc[0];
         constants[17] = gd.mv_jitter_ndc[1];
         constants[18] = float(camera.depth_a);
         constants[19] = float(camera.depth_b);
         constants[20] = g_sr_reactive_scale;
         constants[21] = g_sr_reactive_threshold;
         constants[22] = (write_reactive ? 1.f : 0.f);
         if (PatchedDraws::WriteDynamicConstants(native_device, native_device_context, std::addressof(gd.mv_fill_buffer), constants, sizeof(constants)))
         {
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
            filled = true;
            gd.mv_filled = true;
            if (write_reactive)
            {
               gd.sr_reactive_frame = cb_luma_global_settings.FrameIndex;
            }
#if DEVELOPMENT
            if (write_reactive && g_sr_reactive_zero_test)
            {
               const FLOAT zero[4] = {};
               native_device_context->ClearUnorderedAccessViewFloat(gd.mv_reactive_uav.get(), zero);
               native_device_context->ClearUnorderedAccessViewFloat(gd.mv_transparency_uav.get(), zero);
            }
#endif
#if DEVELOPMENT
            if (perf_queries && perf_queries->Marked(PERF_SCENE_END) && !perf_queries->Marked(PERF_SCENE_TAIL_END))
            {
               perf_queries->Mark(native_device_context, PERF_FILL_END);
            }
#endif
         }
      }
      // The upscaler's depth comes from the fill
      if (IsSRActive(device_data) && filled)
      {
         [[maybe_unused]] const bool drawn = DrawUpscaler(native_device, native_device_context, device_data, write_reactive);
#if DEVELOPMENT
         gd.mv_stats.sr_draws += drawn;
#endif
      }
      compute_state.Restore(native_device_context);
      graphics_state.Restore(native_device_context);
#if DEVELOPMENT
      if (perf_queries && perf_queries->Marked(PERF_SCENE_END))
      {
         perf_queries->Mark(native_device_context, PERF_SCENE_TAIL_END);
      }
#endif
   }

public:
   static void UnregisterEvents()
   {
      reshade::unregister_event<reshade::addon_event::map_buffer_region>(OnMapBufferRegion);
      reshade::unregister_event<reshade::addon_event::unmap_buffer_region>(OnUnmapBufferRegion);
      reshade::unregister_event<reshade::addon_event::update_buffer_region>(OnUpdateBufferRegion);
      reshade::unregister_event<reshade::addon_event::create_pipeline>(PatchedDraws::OnCreateBlendState<MotionVectorPatches::target_slot, MotionVectorPatches::reactive_slot>);
      reshade::unregister_event<reshade::addon_event::bind_pipeline>(OnBindPipeline);
      reshade::unregister_event<reshade::addon_event::push_descriptors>(OnPushConstantBuffers);
      reshade::unregister_event<reshade::addon_event::bind_vertex_buffers>(OnBindVertexBuffers);
      reshade::unregister_event<reshade::addon_event::bind_index_buffer>(OnBindIndexBuffer);
   }

   void OnInit(bool async) override
   {
#if DEVELOPMENT
      // For the MCP "luma_dev_values" tool
      Mcp::RegisterToggles({{"smaa_enable", &g_smaa_enable}, {"smaa_t2x", &g_smaa_t2x}, {"smaa_predication", &g_smaa_predication}, {"smaa_pred_debug", &g_smaa_pred_debug}, {"smaa_pred_measure", &g_smaa_pred_measure},
         { "hide_ui",
            &g_hide_ui }});
      Mcp::RegisterToggles({{"mv_enable", &g_mv_enable}, {"mv_debug_view", &g_mv_debug_view}, {"mv_force_jitter", &g_mv_force_jitter}, {"mv_match_objects", &g_mv_match_objects}, {"mv_buffer_filter", &g_mv_buffer_filter}, {"mv_constants_pool", &g_mv_constants_pool}, {"blend_memo", &g_blend_memo}, {"bound_state_tracking", &g_bound_state_tracking}, {"bound_state_check", &g_bound_state_check}, {"vc4_mirror", &g_vc4_mirror}, {"vc4_mirror_check", &g_vc4_mirror_check}, {"mv_trace_loading", &g_mv_trace_loading}, { "perf_hook_timers",
                               &Perf::g_hook_timers }});
      // As the "Performance Test" combo: a mode cancels a running sweep; "perf_sweep" 1 starts the GPU "Sweep", 2 the "CPU Sweep", 0 stops it
      Mcp::RegisterInts({{"perf_test", &Perf::g_test, 0, int(std::size(perf_test_modes)) - 1, [](DeviceData& device_data, double value)
                            {
                               g_perf_sweep.Stop();
                               ApplyPerfTestMode(device_data, int(value));
                               return std::string();
                            }},
         { "perf_sweep",
            &g_perf_sweep.running,
            0,
            int(std::size(perf_sweeps)),
            [](DeviceData& device_data, double value)
            {
               if (value == 0.0)
               {
                  g_perf_sweep.Stop();
                  ApplyPerfTestMode(device_data, 0);
               }
               else
               {
                  ApplyPerfTestMode(device_data, g_perf_sweep.Start(int(value) - 1));
               }
               return std::string();
            } }});
      Mcp::RegisterToggles({{"sr_reactive_enable", &g_sr_reactive_enable}, {"sr_reactive_debug_view", &g_sr_reactive_debug_view}, {"sr_tc_from_mask", &g_sr_tc_from_mask}, {"sr_reactive_pass", &g_sr_reactive_pass},
         {"sr_reactive_skip_fill", &g_sr_reactive_skip_fill}, { "sr_reactive_zero_test",
            &g_sr_reactive_zero_test }});
      Mcp::RegisterValues({{"sr_reactive_scale", &g_sr_reactive_scale, 0.f, 4.f}, {"sr_reactive_threshold", &g_sr_reactive_threshold, 0.f, 1.f}});
      Mcp::RegisterTextures({MCP_GAME_TEXTURE("mv.velocity", mv_texture), MCP_GAME_TEXTURE("mv.depth", mv_device_depth), MCP_GAME_TEXTURE("sr.reactive", mv_reactive),
         MCP_GAME_TEXTURE("sr.transparency", mv_transparency), MCP_GAME_TEXTURE("sr.draws_mask", mv_reactive_target)});
      Mcp::RegisterMirroredToggle("luma_bloom_enable", &g_luma_bloom_enable, &cb_luma_global_settings.GameSettings.LumaBloomEnable);
      Mcp::RegisterMirroredToggle("video_auto_hdr_enable", &g_video_auto_hdr_enable, &cb_luma_global_settings.GameSettings.VideoAutoHDREnable);
      Mcp::RegisterValues({{"rcas_sharpness", &g_rcas_sharpness, 0.f, 1.f}, {"smaa_pred_tolerance", &g_smaa_pred_tolerance, 0.002f, 0.2f}, {"bloom_intensity", &g_bloom_intensity, 0.f, 2.f}});
      Mcp::RegisterTextures({MCP_GAME_TEXTURE("smaa.input", tex_input_encoded),
         MCP_GAME_TEXTURE("smaa.pred_mask", tex_pred),
         MCP_GAME_TEXTURE("smaa.output", tex_smaa_out)});
#endif
      // Game-specific toggle consumed by the tonemap replacement (Luma_BL2TPS_Tonemap.hlsl).
      std::vector<ShaderDefineData> game_shader_defines_data = {
         {"TONEMAP_TYPE", '1', true, false, "0 - SDR: Vanilla grade (reference)\n1 - HDR: native grade + reconstructed luminance + DICE display map", 1},
      };
      shader_defines_data.append_range(game_shader_defines_data);
      assert(shader_defines_data.size() < MAX_SHADER_DEFINES);

      // UE3 is all SDR (UNORM) gamma space: post buffers stay GAMMA so the gamma-SDR HUD blends like vanilla.
      // The tonemap pre-scales by GamePaperWhite/UIPaperWhite (UI_DRAW_TYPE 2) so the HUD lands at its own level.
      GetShaderDefineData(POST_PROCESS_SPACE_TYPE_HASH).SetDefaultValue('0');
      GetShaderDefineData(EARLY_DISPLAY_ENCODING_HASH).SetDefaultValue('0');
      GetShaderDefineData(VANILLA_ENCODING_TYPE_HASH).SetDefaultValue('1'); // Gamma 2.2 in and out
      GetShaderDefineData(GAMMA_CORRECTION_TYPE_HASH).SetDefaultValue('1');
      GetShaderDefineData(GAMUT_MAPPING_TYPE_HASH).SetDefaultValue('1'); // gamut-map wild colors in composition
      GetShaderDefineData(UI_DRAW_TYPE_HASH).SetDefaultValue('2');       // HUD gets its own UIPaperWhite + gamma blend

      // Manual Scene + UI Paper White sliders instead of the OS HDR reference level. Core gates the separate
      // "UI Paper White" slider on UI_DRAW_TYPE >= 1 && !use_os_reference_white_level. UI default 203 nits (BT.2408).
      use_os_reference_white_level = false;

      // Core auto-registers the 6 SMAA passes; these are this game's own. RCAS sharpen PS (drawn via core "Copy VS" +
      // DrawCustomPixelShader after SMAA).
      native_shaders_definitions.emplace(CompileTimeStringHash("BL2TPS Sharpen PS"),
         ShaderDefinition{"Luma_RCAS_PS", reshade::api::pipeline_subobject_type::pixel_shader, nullptr, "sharpen_ps"});
      // SMAA T2x's own passes ("SMAA_T2X" in Luma_SMAA_impl.hlsl). The edge detection and the vertex shaders are 1x's.
      native_shaders_definitions.emplace(CompileTimeStringHash("BL2TPS SMAA T2x Blending Weight Calculation PS"),
         ShaderDefinition{"Luma_SMAA_impl", reshade::api::pipeline_subobject_type::pixel_shader, nullptr, "smaa_blending_weight_calculation_ps", {{"SMAA_T2X", "1"}}});
      native_shaders_definitions.emplace(CompileTimeStringHash("BL2TPS SMAA T2x Neighborhood Blending PS"),
         ShaderDefinition{"Luma_SMAA_impl", reshade::api::pipeline_subobject_type::pixel_shader, nullptr, "smaa_neighborhood_blending_ps", {{"SMAA_T2X", "1"}}});
      native_shaders_definitions.emplace(CompileTimeStringHash("BL2TPS SMAA T2x Resolve PS"),
         ShaderDefinition{"Luma_SMAA_impl", reshade::api::pipeline_subobject_type::pixel_shader, nullptr, "smaa_resolve_ps", {{"SMAA_T2X", "1"}}});
      // Depth-extract CS for SMAA predication: scene-color .a (Gearbox EncodeFloatW view depth) -> R16F plane-deviation edge-ness.
      native_shaders_definitions.emplace(CompileTimeStringHash("BL2TPS Depth Extract CS"),
         ShaderDefinition("Luma_BL2TPS_DepthExtract", reshade::api::pipeline_subobject_type::compute_shader));

      // DLSS/FSR: their depth and the camera motion from the scene's alpha, the CPU copies of vc4 (dgVoodoo maps it or updates it),
      // and the motion vector target written by every blend state
      sr_game_tooltip = "DLSS and FSR 3 require Luma-Upscaler.exe next to the game's exe (Luma TAA doesn't).\n";
      native_shaders_definitions.emplace(CompileTimeStringHash("BL2TPS Motion Vector Fill CS"),
         ShaderDefinition("Luma_BL2TPS_MotionVectorFill", reshade::api::pipeline_subobject_type::compute_shader));
      native_shaders_definitions.emplace(CompileTimeStringHash("BL2TPS Copy Back PS"),
         ShaderDefinition("Luma_BL2TPS_CopyBack", reshade::api::pipeline_subobject_type::pixel_shader));
      reshade::register_event<reshade::addon_event::create_pipeline>(PatchedDraws::OnCreateBlendState<MotionVectorPatches::target_slot, MotionVectorPatches::reactive_slot>);
      reshade::register_event<reshade::addon_event::bind_pipeline>(OnBindPipeline);
      reshade::register_event<reshade::addon_event::push_descriptors>(OnPushConstantBuffers);
      reshade::register_event<reshade::addon_event::bind_vertex_buffers>(OnBindVertexBuffers);
      reshade::register_event<reshade::addon_event::bind_index_buffer>(OnBindIndexBuffer);

      // The game's post passes use cb0..cb5 and no translated shader reads one past b4, so b9/b10 (the motion vector draws, see
      // "MotionVectorPatches"), b11 (core DrawBloom's own constants) and b12/b13 are free for Luma.
      luma_settings_cbuffer_index = 13;
      luma_data_cbuffer_index = 12;

      // User settings mirrored into LumaSettings.GameSettings; GameCBuffers.hlsl says which act in SDR too.
      default_luma_global_game_settings.Exposure = 1.f;           // scene multiplier (1x)
      default_luma_global_game_settings.Saturation = 1.f;         // BT.709-luminance lerp (Color.hlsl Saturation)
      default_luma_global_game_settings.HighlightDechroma = 0.f;  // off; only mandatory DICE/gamut desat applies
      default_luma_global_game_settings.BloomIntensity = 1.f;     // seed only; OnDrawOrDispatch owns the effective value
      default_luma_global_game_settings.Contrast = 1.f;           // multiplicative around 18% mid-gray, pre-DICE
      default_luma_global_game_settings.VignetteIntensity = 1.f;  // game vignette darkening scale
      default_luma_global_game_settings.LumaBloomEnable = 1.f;    // 1 = Luma HDR pyramidal bloom, 0 = vanilla game bloom
      default_luma_global_game_settings.Dithering = 1.f;          // animated triangular dither at output (HDR and SDR), anti-banding on
      default_luma_global_game_settings.VideoAutoHDREnable = 1.f; // light AutoHDR on Bink videos (HDR only)
      default_luma_global_game_settings.VideoAutoHDRBoost = 0.5f; // highlight-expansion strength (peak ~165 nits at 0.5)
      default_luma_global_game_settings.BloomThreshold = 1.f;     // replaced once the native bright pass is read back
      cb_luma_global_settings.GameSettings = default_luma_global_game_settings;
   }

   void OnCreateDevice(ID3D11Device* native_device, DeviceData& device_data) override
   {
      device_data.game = new Borderlands2GameDeviceData;
      device_data.taa_detected = true; // No TAA to replace, but Core's upscaler status checks for it
#if DEVELOPMENT
      // For the MCP "luma_dev_values" tool: the last complete frame's counts
      const auto& stats = GetGameDeviceData(device_data).mv_last_stats;
      Mcp::RegisterCounters({{"mv.draws", &stats.motion_vector_draws}, {"mv.jitter_draws", &stats.jitter_draws}, {"mv.matched", &stats.matched}, {"mv.camera_only", &stats.camera_only},
                               {"mv.other_camera", &stats.other_camera}, {"mv.uncopied", &stats.uncopied}, {"mv.maps", &stats.maps}, {"mv.updates", &stats.updates}, {"mv.other_maps", &stats.other_maps},
                               {"mv.sr_draws", &stats.sr_draws}, {"mv.tiebreak_collisions", &stats.tiebreak_collisions}, {"mv.reactive_draws", &stats.reactive_draws}, { "mv.ended_by_hash",
                                  &stats.ended_by }},
         &device_data);
      for (size_t i = 0; i < std::size(kMotionVectorRejectNames); i++)
      {
         Mcp::RegisterCounter(std::string("mv.rejected.") + kMotionVectorRejectNames[i], &stats.rejected[i], &device_data);
      }
#endif
   }

   void OnDestroyDeviceData(DeviceData& device_data) override
   {
#if DEVELOPMENT
      Mcp::Unregister(&device_data);
#endif
      // GameDeviceData lacks a virtual destructor; delete through the concrete type to release derived members.
      delete static_cast<Borderlands2GameDeviceData*>(device_data.game);
      device_data.game = nullptr;
   }

   // dgVoodoo sometimes leaves blending ENABLED on a secondary RT while RT0 has it off; D3D9 has one global blend
   // state, so the game never asked for it and that target is corrupted (diagnosed in TW2, water PS 0xDA16C815). No
   // symptom known here: preventive, and the DEVELOPMENT log reports whether it occurs. Repair = copy RT0's blend
   // fields onto the offenders, write masks kept. Not gated on is_immediate: blend state records fine into a deferred
   // list. Returns true iff it ran the original draw itself.
   bool FixImpossiblePerRTBlend(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, bool is_immediate, Borderlands2GameDeviceData* gd, reshade::api::shader_stage stages, const ShaderHashesList<OneShaderPerPipeline>& original_shader_hashes, bool is_custom_pass, std::function<void()>* original_draw_dispatch_func)
   {
      // Our own injected passes set their blend state deliberately. Re-issuing the draw is the only way to
      // apply a different state, so without that callback there is nothing to do.
      if (is_custom_pass || (stages & reshade::api::shader_stage::pixel) == 0 || original_draw_dispatch_func == nullptr)
         return false;

      // Tracked on the immediate context (see "bound_blend_state")
      com_ptr<ID3D11BlendState> queried_blend_state;
      ID3D11BlendState* blend_state = nullptr;
      if (is_immediate)
      {
         blend_state = BoundState(gd, gd->bound_blend_state, [&](ID3D11BlendState** state)
            { native_device_context->OMGetBlendState(state, nullptr, nullptr); });
      }
      else
      {
         native_device_context->OMGetBlendState(&queried_blend_state, nullptr, nullptr);
         blend_state = queried_blend_state.get();
      }
      if (!blend_state)
         return false; // no state object = default (blending off everywhere)

      // Only BOUND targets count: the wrapper leaves stale BlendEnable in unused descriptor slots, which alone
      // matches nearly every draw and would take over passes other hooks own; Luma's own slots (from
      // "MotionVectorPatches::target_slot") differ by design (see "OnCreateBlendState"). Free descriptor scan first.
      constexpr UINT game_targets = MotionVectorPatches::target_slot;
      // "g_blend_memo": the checks on the state alone, remembered for it (a GetDesc and a loop on every draw of the frame: every
      // blending state has independent blend, see "OnCreateBlendState"). Per thread (this runs on every context) and per frame (a
      // released state's pointer can come back as another's).
      struct BlendMemo
      {
         ID3D11BlendState* state = nullptr;
         uint32_t frame = UINT32_MAX;
         bool candidate = false;
         D3D11_BLEND_DESC desc;
      };
      thread_local BlendMemo memo;
      const bool memoized = g_blend_memo && memo.state == blend_state && memo.frame == cb_luma_global_settings.FrameIndex;
      if (memoized && !memo.candidate)
         return false;
      D3D11_BLEND_DESC queried;
      if (!memoized)
      {
         blend_state->GetDesc(&queried);
         // One state for all targets is already D3D9-shaped
         bool candidate = queried.IndependentBlendEnable;
#if !DEVELOPMENT
         // Only the "RT0 off, RTn on" shape is ever repaired, so outside DEVELOPMENT (where the inverse shape is logged) a blending
         // RT0 has nothing to do here: skip the descriptor scan and the render-target query both.
         candidate &= !queried.RenderTarget[0].BlendEnable;
#endif
         bool disagreement = false;
         for (UINT i = 1; i < game_targets && !disagreement; i++)
         {
            disagreement = queried.RenderTarget[i].BlendEnable != queried.RenderTarget[0].BlendEnable;
         }
         candidate &= disagreement;
         if (g_blend_memo)
         {
            memo.state = blend_state;
            memo.frame = cb_luma_global_settings.FrameIndex;
            memo.candidate = candidate;
            if (candidate)
            {
               memo.desc = queried;
            }
         }
         if (!candidate)
            return false;
      }
      const D3D11_BLEND_DESC& bd = (memoized ? memo.desc : queried);
      const bool rt0_blending = bd.RenderTarget[0].BlendEnable != FALSE;

      ID3D11RenderTargetView* rtvs[game_targets] = {};
      native_device_context->OMGetRenderTargets(game_targets, rtvs, nullptr);
      bool bound[game_targets] = {};
      for (UINT i = 0; i < game_targets; i++)
      {
         bound[i] = rtvs[i] != nullptr;
         if (rtvs[i])
         {
            rtvs[i]->Release(); // OMGetRenderTargets hands back references; only the bound/not-bound answer is kept
         }
      }

      bool needs_fix = false;
      [[maybe_unused]] bool inverse_shape = false;
      for (UINT i = 1; i < game_targets; i++)
      {
         if (!bound[i] || bd.RenderTarget[i].BlendEnable == bd.RenderTarget[0].BlendEnable)
            continue;
         if (bd.RenderTarget[0].BlendEnable == FALSE)
         {
            needs_fix = true;
         }
         else
         {
            inverse_shape = true;
         }
      }

#if DEVELOPMENT
      // One line per distinct shader, so a play session reports every pass that carries this.
      if (needs_fix || inverse_shape)
      {
         // Locked: this function deliberately runs on every context (see its header), so two can reach the set.
         static std::mutex logged_shaders_mutex;
         static std::unordered_set<uint64_t> logged_shaders;
         const uint64_t pixel_shader_hash = original_shader_hashes.pixel_shaders[0];
         const std::scoped_lock logged_shaders_lock(logged_shaders_mutex);
         if (logged_shaders.emplace(pixel_shader_hash).second)
         {
            reshade::log::message(reshade::log::level::warning,
               std::format("[BL-BlendFix] impossible per-RT blend state (dgVoodoo artefact) on pixel shader 0x{:X} - {}", pixel_shader_hash, needs_fix ? "repaired" : "inverse shape, left alone").c_str());
         }
      }
#endif

      if (!needs_fix)
         return false;

      ComPtr<ID3D11BlendState> fixed_state;
      if (const auto it = gd->fixed_blend_states.find(bd); it != gd->fixed_blend_states.end())
      {
         fixed_state = it->second;
      }
      else
      {
         D3D11_BLEND_DESC fixed_desc = bd;
         for (UINT i = 1; i < game_targets; i++)
         {
            if (fixed_desc.RenderTarget[i].BlendEnable == bd.RenderTarget[0].BlendEnable)
               continue;
            const UINT8 write_mask = fixed_desc.RenderTarget[i].RenderTargetWriteMask; // legal per-RT in D3D9, keep it
            fixed_desc.RenderTarget[i] = bd.RenderTarget[0];
            fixed_desc.RenderTarget[i].RenderTargetWriteMask = write_mask;
         }
         if (FAILED(native_device->CreateBlendState(&fixed_desc, fixed_state.put())) || !fixed_state)
            return false; // leave the draw untouched rather than run it half-applied
         gd->fixed_blend_states[bd] = fixed_state;
      }

      FLOAT blend_factor[4];
      UINT sample_mask = 0;
      com_ptr<ID3D11BlendState> bound_blend_state;
      native_device_context->OMGetBlendState(&bound_blend_state, blend_factor, &sample_mask);
      native_device_context->OMSetBlendState(fixed_state.get(), blend_factor, sample_mask);
      (*original_draw_dispatch_func)();
      native_device_context->OMSetBlendState(blend_state, blend_factor, sample_mask); // hand the game back its own state
      return true;
   }

   // Re-applies the stencil test dgVoodoo drops on the item card's rolling price digits. Returns true iff it ran
   // the original draw itself (caller returns Replaced).
   bool RepairScaleformStencilMask(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, Borderlands2GameDeviceData* gd, const ShaderHashesList<OneShaderPerPipeline>& original_shader_hashes, bool is_custom_pass, bool is_immediate, std::function<void()>* original_draw_dispatch_func)
   {
      if (!is_immediate || is_custom_pass)
         return false;
      const bool is_mask_shader = ContainsPixelShader(original_shader_hashes, kScaleformMaskFillHash, kScaleformMaskFillHash_v281);
      if (!gd->dsv_scaleform_mask_active && !is_mask_shader)
         return false;

      // A mask submit is the mask PS drawn with color writes off + a stencil-writing state.
      const auto is_mask_submit = [&]
      {
         ComPtr<ID3D11BlendState> blend_state;
         FLOAT blend_factor[4];
         UINT sample_mask = 0;
         native_device_context->OMGetBlendState(blend_state.put(), blend_factor, &sample_mask);
         if (!blend_state)
            return false;
         D3D11_BLEND_DESC bd;
         blend_state->GetDesc(&bd);
         if (bd.RenderTarget[0].RenderTargetWriteMask != 0)
            return false;
         ComPtr<ID3D11DepthStencilState> ds_state;
         UINT stencil_ref = 0;
         native_device_context->OMGetDepthStencilState(ds_state.put(), &stencil_ref);
         if (!ds_state)
            return false;
         D3D11_DEPTH_STENCIL_DESC dsd;
         ds_state->GetDesc(&dsd);
         return dsd.StencilEnable != FALSE;
      };

      if (is_mask_shader && is_mask_submit())
      {
         // Duplicate the mask into the scratch stencil (REPLACE/1); arm only when the duplicate actually lands.
         // The real mask draw still proceeds through the normal path either way.
         gd->dsv_scaleform_mask_active.reset();
         ComPtr<ID3D11RenderTargetView> rtv;
         ComPtr<ID3D11DepthStencilView> prev_dsv;
         native_device_context->OMGetRenderTargets(1, rtv.put(), prev_dsv.put());
         if (!rtv || original_draw_dispatch_func == nullptr)
            return false;
         ComPtr<ID3D11Resource> rt_res;
         rtv->GetResource(rt_res.put());
         ComPtr<ID3D11Texture2D> rt_tex;
         if (!rt_res || FAILED(rt_res->QueryInterface(IID_PPV_ARGS(rt_tex.put()))))
            return false;
         D3D11_TEXTURE2D_DESC rt_desc;
         rt_tex->GetDesc(&rt_desc);

         // The scratch of this size, else the first free slot, else the next one round robin
         Borderlands2GameDeviceData::ScaleformMaskDS* scratch = nullptr;
         Borderlands2GameDeviceData::ScaleformMaskDS* free_slot = nullptr;
         for (auto& slot : gd->scaleform_mask_ds_cache)
         {
            if (slot.dsv && slot.width == rt_desc.Width && slot.height == rt_desc.Height)
            {
               scratch = &slot;
               break;
            }
            if (!slot.dsv && !free_slot)
            {
               free_slot = &slot;
            }
         }
         if (!scratch)
         {
            scratch = (free_slot ? free_slot : &gd->scaleform_mask_ds_cache[gd->scaleform_mask_ds_next++ % std::size(gd->scaleform_mask_ds_cache)]);
            scratch->dsv.reset();
            scratch->tex.reset();
            const CD3D11_TEXTURE2D_DESC ds_desc(DXGI_FORMAT_D24_UNORM_S8_UINT, rt_desc.Width, rt_desc.Height, 1, 1, D3D11_BIND_DEPTH_STENCIL, D3D11_USAGE_DEFAULT, 0, rt_desc.SampleDesc.Count, rt_desc.SampleDesc.Quality);
            if (SUCCEEDED(native_device->CreateTexture2D(&ds_desc, nullptr, scratch->tex.put())))
            {
               native_device->CreateDepthStencilView(scratch->tex.get(), nullptr, scratch->dsv.put());
            }
            scratch->width = rt_desc.Width;
            scratch->height = rt_desc.Height;
         }
         if (!gd->dss_scaleform_mask_write)
         {
            // Write 1 everywhere the mask draws, depth off
            CD3D11_DEPTH_STENCIL_DESC write_desc(D3D11_DEFAULT);
            write_desc.DepthEnable = FALSE;
            write_desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
            write_desc.DepthFunc = D3D11_COMPARISON_ALWAYS;
            write_desc.StencilEnable = TRUE;
            write_desc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_REPLACE;
            write_desc.BackFace = write_desc.FrontFace;
            native_device->CreateDepthStencilState(&write_desc, gd->dss_scaleform_mask_write.put());
         }
         if (!scratch->dsv || !gd->dss_scaleform_mask_write)
            return false;

         ComPtr<ID3D11DepthStencilState> prev_ds_state;
         UINT prev_stencil_ref = 0;
         native_device_context->OMGetDepthStencilState(prev_ds_state.put(), &prev_stencil_ref);
         native_device_context->ClearDepthStencilView(scratch->dsv.get(), D3D11_CLEAR_STENCIL, 1.f, 0);
         ID3D11RenderTargetView* rtv_raw = rtv.get();
         native_device_context->OMSetRenderTargets(1, &rtv_raw, scratch->dsv.get());
         native_device_context->OMSetDepthStencilState(gd->dss_scaleform_mask_write.get(), 1u);
         (*original_draw_dispatch_func)();
         native_device_context->OMSetDepthStencilState(prev_ds_state.get(), prev_stencil_ref);
         native_device_context->OMSetRenderTargets(1, &rtv_raw, prev_dsv.get());
         gd->dsv_scaleform_mask_active = scratch->dsv;
         return false;
      }

      if (gd->dsv_scaleform_mask_active && ContainsPixelShader(original_shader_hashes, kScaleformDigitGlyphHash, kScaleformDigitGlyphHash_v281))
      {
         // Only intervene on glyph draws with stencil ENABLED but the test neutered (ALWAYS func / zero read mask /
         // no stencil plane) = the broken masked content; StencilEnable FALSE = genuinely unmasked glyphs (leave),
         // an effective test = a correctly translated path (leave).
         ComPtr<ID3D11DepthStencilState> prev_ds_state;
         UINT prev_stencil_ref = 0;
         native_device_context->OMGetDepthStencilState(prev_ds_state.put(), &prev_stencil_ref);
         ComPtr<ID3D11RenderTargetView> rtv;
         ComPtr<ID3D11DepthStencilView> prev_dsv;
         native_device_context->OMGetRenderTargets(1, rtv.put(), prev_dsv.put());
         D3D11_DEPTH_STENCIL_DESC dsd = {};
         if (prev_ds_state)
         {
            prev_ds_state->GetDesc(&dsd);
         }
         const bool stencil_enabled = prev_ds_state && dsd.StencilEnable != FALSE;
         bool effective_stencil_test = false;
         if (stencil_enabled && prev_dsv)
         {
            D3D11_DEPTH_STENCIL_VIEW_DESC dsv_desc;
            prev_dsv->GetDesc(&dsv_desc);
            const bool has_stencil_plane = dsv_desc.Format == DXGI_FORMAT_D24_UNORM_S8_UINT || dsv_desc.Format == DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
            const bool any_func_tests = dsd.FrontFace.StencilFunc != D3D11_COMPARISON_ALWAYS || dsd.BackFace.StencilFunc != D3D11_COMPARISON_ALWAYS;
            effective_stencil_test = dsd.StencilReadMask != 0 && has_stencil_plane && any_func_tests;
         }
         if (!stencil_enabled || effective_stencil_test || original_draw_dispatch_func == nullptr || !rtv)
            return false;
         if (!gd->dss_scaleform_mask_test)
         {
            // Test-only: pass exactly where the duplicated mask wrote 1, never write, depth off.
            CD3D11_DEPTH_STENCIL_DESC test_desc(D3D11_DEFAULT);
            test_desc.DepthEnable = FALSE;
            test_desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
            test_desc.DepthFunc = D3D11_COMPARISON_ALWAYS;
            test_desc.StencilEnable = TRUE;
            test_desc.StencilWriteMask = 0;
            test_desc.FrontFace.StencilFunc = D3D11_COMPARISON_EQUAL;
            test_desc.BackFace = test_desc.FrontFace;
            native_device->CreateDepthStencilState(&test_desc, gd->dss_scaleform_mask_test.put());
         }
         if (!gd->dss_scaleform_mask_test)
            return false;
         ID3D11RenderTargetView* rtv_raw = rtv.get();
         native_device_context->OMSetRenderTargets(1, &rtv_raw, gd->dsv_scaleform_mask_active.get());
         native_device_context->OMSetDepthStencilState(gd->dss_scaleform_mask_test.get(), 1u);
         (*original_draw_dispatch_func)();
         native_device_context->OMSetDepthStencilState(prev_ds_state.get(), prev_stencil_ref);
         native_device_context->OMSetRenderTargets(1, &rtv_raw, prev_dsv.get());
         return true;
      }

      // Any other draw ends the mask span.
      gd->dsv_scaleform_mask_active.reset();
      return false;
   }

   // The Steam launcher's ReShade instance (it creates its own D3D11 device through dgVoodoo) owns ReShade.log for
   // the whole session and the game's lines are dropped, so mirror every line into our own file beside the exe.
   static void LogLine(const std::string& line)
   {
      reshade::log::message(reshade::log::level::info, line.c_str());
      std::ofstream file("Luma-BL2.log", std::ios::app);
      if (file)
      {
         file << line << std::endl;
      }
   }

   // Copy the pass's PS constant buffer and hand back `row_count` rows of it, one frame late (the path proven on this
   // wrapper in MoH Airborne). False = nothing to read yet, normal on the first frames.
   static bool CaptureConstantRows(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context,
      Borderlands2GameDeviceData::ConstantCapture* capture, uint32_t slot, uint32_t first_row, uint32_t row_count, float* out)
   {
      ComPtr<ID3D11Buffer> cb;
      native_device_context->PSGetConstantBuffers(slot, 1, cb.put());
      if (!cb)
         return false;
      D3D11_BUFFER_DESC bd = {};
      cb->GetDesc(&bd);
      if (bd.ByteWidth < (first_row + row_count) * 16)
         return false;

      if (capture->bytes != bd.ByteWidth)
      {
         D3D11_BUFFER_DESC staging_desc = {};
         staging_desc.ByteWidth = bd.ByteWidth;
         staging_desc.Usage = D3D11_USAGE_STAGING;
         staging_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
         capture->staging.reset();
         capture->copy_pending = false;
         if (FAILED(native_device->CreateBuffer(&staging_desc, nullptr, capture->staging.put())) || !capture->staging)
         {
            capture->bytes = 0;
            return false;
         }
         capture->bytes = bd.ByteWidth;
      }

      bool have_rows = false;
      if (capture->copy_pending)
      {
         D3D11_MAPPED_SUBRESOURCE mapped = {};
         if (SUCCEEDED(native_device_context->Map(capture->staging.get(), 0, D3D11_MAP_READ, D3D11_MAP_FLAG_DO_NOT_WAIT, &mapped)) && mapped.pData != nullptr)
         {
            std::memcpy(out, (const uint8_t*)mapped.pData + (size_t)first_row * 16, (size_t)row_count * 16);
            native_device_context->Unmap(capture->staging.get(), 0);
            capture->copy_pending = false;
            have_rows = true;
         }
      }
      if (!capture->copy_pending)
      {
         native_device_context->CopyResource(capture->staging.get(), cb.get());
         capture->copy_pending = true;
      }
      return have_rows;
   }

   // The artist-authored bloom pair off the game's bright pass ("kBloomBrightPassHash"), which per tap computes
   //     colour = tex.rgb * cb4[16].x;  w = saturate((max3(colour) - cb4[17].y) * 0.5);  out += colour * w
   // i.e. BloomScale in cb4[16].x, BloomThreshold in cb4[17].y. The scale measured a constant 4 everywhere, so only
   // the threshold is tracked; each distinct set is reported once.
   static void CaptureBloomConstants(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, Borderlands2GameDeviceData* gd)
   {
      // Paced: the value is authored per PostProcessVolume and moves on a human timescale; each capture copies cb4.
      if ((gd->frame_counter & 7u) != 0u)
         return;

      float rows[2 * 4] = {};
      if (!CaptureConstantRows(native_device, native_device_context, &gd->bloom_cb, 4, 16, 2, rows))
         return;
      const float scale = rows[0];              // cb4[16].x
      const float threshold = rows[5];          // cb4[17].y
      if (threshold > 0.f && threshold < 100.f) // reject implausible readback rather than let it reach the frame
      {
         gd->bloom_threshold_live = threshold;
      }

      // The prefilter's knee drops the x4 because the bright pass reads the scene pre-divided by 4 and BloomScale
      // cancels it; an area authoring another scale would silently shift the knee by that factor, so say it once.
      if (!gd->bloom_scale_warned && fabsf(scale - 4.f) > 1e-3f)
      {
         gd->bloom_scale_warned = true;
         LogLine(std::format("[BL-Bloom] WARNING: bright-pass BloomScale is {:.5f}, not the 4.0 the Luma prefilter's knee assumes - the bloom threshold is off by that factor", scale));
      }

#if DEVELOPMENT
      const float keyed[2] = {scale, threshold};
      const float quanta[2] = {1000.f, 1000.f};
      if (gd->bloom_cb_logged.size() < 64 && gd->bloom_cb_logged.emplace(QuantizedKey(keyed, quanta, 2)).second)
      {
         LogLine(std::format("[BL-Bloom] bright pass: BloomScale={:.5f} BloomThreshold={:.5f}", scale, threshold));
      }
#endif
   }

#if DEVELOPMENT
   static uint64_t QuantizedKey(const float* values, const float* quanta, uint32_t count)
   {
      uint64_t key = 0;
      for (uint32_t i = 0; i < count; i++)
      {
         HashCombine(key, uint64_t(uint32_t(lroundf(values[i] * quanta[i]))));
      }
      return key;
   }

   // One-shot readback of the predication mask, so g_smaa_pred_tolerance is calibrated from numbers rather than
   // from screenshots. A mean would be useless here: on a well-tuned frame the mask is 0 nearly everywhere and any
   // average drowns in that, so this reports COVERAGE at the level SMAA actually compares against (0.5) plus the
   // shape either side of it. Every texel is read, not a stride - a 1px silhouette is precisely the signal a
   // subsample would step over. Copies on the frame the button is pressed and maps on a later one (non-blocking),
   // like the bloom A/B below, so the press never stalls the render thread.
   static void LogPredicationStats(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, Borderlands2GameDeviceData* gd)
   {
      auto& capture = gd->pred_measure;
      if (!gd->tex_pred || (!g_smaa_pred_measure && !capture.copy_pending))
         return;

      D3D11_TEXTURE2D_DESC td = {};
      gd->tex_pred->GetDesc(&td);
      if (capture.width != td.Width || capture.height != td.Height || capture.format != td.Format)
      {
         D3D11_TEXTURE2D_DESC staging_desc = td;
         staging_desc.Usage = D3D11_USAGE_STAGING;
         staging_desc.BindFlags = 0;
         staging_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
         staging_desc.MiscFlags = 0;
         capture.staging.reset();
         capture.copy_pending = false;
         if (FAILED(native_device->CreateTexture2D(&staging_desc, nullptr, capture.staging.put())) || !capture.staging)
         {
            capture.width = 0;
            return;
         }
         capture.width = td.Width;
         capture.height = td.Height;
         capture.format = td.Format;
      }

      if (!capture.copy_pending)
      {
         g_smaa_pred_measure = false;
         native_device_context->CopyResource(capture.staging.get(), gd->tex_pred.get());
         capture.copy_pending = true;
         return;
      }

      D3D11_MAPPED_SUBRESOURCE mapped = {};
      if (FAILED(native_device_context->Map(capture.staging.get(), 0, D3D11_MAP_READ, D3D11_MAP_FLAG_DO_NOT_WAIT, &mapped)) || mapped.pData == nullptr)
         return; // still in flight, retry next frame

      constexpr uint32_t kBins = 256;
      uint64_t histogram[kBins] = {};
      const uint64_t total = (uint64_t)capture.width * capture.height;
      uint64_t non_finite = 0;
      for (UINT y = 0; y < capture.height; y++)
      {
         const uint16_t* row = (const uint16_t*)((const uint8_t*)mapped.pData + (size_t)y * mapped.RowPitch);
         for (UINT x = 0; x < capture.width; x++)
         {
            const float v = DirectX::PackedVector::XMConvertHalfToFloat(row[x]);
            // The range test, not a clamp: a NaN fails BOTH ordered comparisons, so the clamped form would fall
            // through to (uint32_t)NaN - 0x80000000 on x86 - and index far outside this stack array. The extract
            // CS saturates, so a NaN here means it met one upstream (ME2 measured inf - inf in the scene alpha on
            // the same shader family); count them rather than bin them, a silent 0 would read as a flat surface.
            if (v >= 0.f && v <= 1.f)
            {
               histogram[(uint32_t)(v * (float)(kBins - 1))]++;
            }
            else
            {
               non_finite++;
            }
         }
      }
      native_device_context->Unmap(capture.staging.get(), 0);
      capture.copy_pending = false;

      auto fraction_above = [&](float level)
      {
         uint64_t hits = 0;
         for (uint32_t b = (uint32_t)(level * (float)(kBins - 1)) + 1u; b < kBins; b++)
         {
            hits += histogram[b];
         }
         return 100.0 * (double)hits / (double)total;
      };
      auto percentile = [&](double p)
      {
         const uint64_t target = (uint64_t)(p * (double)total);
         uint64_t running = 0;
         for (uint32_t b = 0; b < kBins; b++)
         {
            running += histogram[b];
            if (running >= target)
               return (float)b / (float)(kBins - 1);
         }
         return 1.f;
      };

      char line[512];
      snprintf(line, sizeof(line),
         "[BL2-Pred] tol=%.4f | FIRES(>0.5)=%.3f%% | >0.1=%.3f%% >0.25=%.3f%% >0.75=%.3f%% >0.9=%.3f%% | flat(bin0)=%.2f%% | p50=%.3f p90=%.3f p99=%.3f p999=%.3f | nonfinite=%llu | %ux%u",
         g_smaa_pred_tolerance, fraction_above(0.5f), fraction_above(0.1f), fraction_above(0.25f), fraction_above(0.75f), fraction_above(0.9f),
         100.0 * (double)histogram[0] / (double)total, percentile(0.5), percentile(0.9), percentile(0.99), percentile(0.999),
         (unsigned long long)non_finite, capture.width, capture.height);
      LogLine(line);
   }

   static bool SampleTextureStats(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context,
      ID3D11ShaderResourceView* srv, Borderlands2GameDeviceData::TextureCapture* capture, float* out_mean, float* out_max, DXGI_FORMAT* out_format)
   {
      if (srv == nullptr)
         return false;
      ComPtr<ID3D11Resource> res;
      srv->GetResource(res.put());
      ComPtr<ID3D11Texture2D> tex;
      if (!res || FAILED(res->QueryInterface(IID_PPV_ARGS(tex.put()))))
         return false;
      D3D11_TEXTURE2D_DESC td = {};
      tex->GetDesc(&td);
      D3D11_SHADER_RESOURCE_VIEW_DESC vd = {};
      srv->GetDesc(&vd);
      *out_format = vd.Format; // the VIEW format is what the shader reads, and what decides UNORM vs FLOAT

      if (capture->width != td.Width || capture->height != td.Height || capture->format != td.Format)
      {
         D3D11_TEXTURE2D_DESC staging_desc = td;
         staging_desc.MipLevels = 1;
         staging_desc.ArraySize = 1;
         staging_desc.Usage = D3D11_USAGE_STAGING;
         staging_desc.BindFlags = 0;
         staging_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
         staging_desc.MiscFlags = 0;
         capture->staging.reset();
         capture->copy_pending = false;
         if (FAILED(native_device->CreateTexture2D(&staging_desc, nullptr, capture->staging.put())) || !capture->staging)
         {
            capture->width = 0;
            return false;
         }
         capture->width = td.Width;
         capture->height = td.Height;
         capture->format = td.Format;
      }

      bool have_stats = false;
      if (capture->copy_pending)
      {
         D3D11_MAPPED_SUBRESOURCE mapped = {};
         if (SUCCEEDED(native_device_context->Map(capture->staging.get(), 0, D3D11_MAP_READ, D3D11_MAP_FLAG_DO_NOT_WAIT, &mapped)) && mapped.pData != nullptr)
         {
            const bool is_unorm = vd.Format == DXGI_FORMAT_R16G16B16A16_UNORM;
            double sum = 0.0;
            double peak = 0.0;
            uint32_t count = 0;
            for (UINT y = 0; y < capture->height; y += 4)
            {
               const uint16_t* row = (const uint16_t*)((const uint8_t*)mapped.pData + (size_t)y * mapped.RowPitch);
               for (UINT x = 0; x < capture->width; x += 4)
               {
                  float rgb[3];
                  for (uint32_t c = 0; c < 3; c++)
                  {
                     const uint16_t raw = row[(size_t)x * 4 + c];
                     rgb[c] = (is_unorm ? (raw / 65535.f) : DirectX::PackedVector::XMConvertHalfToFloat(raw));
                  }
                  const float weighted_rgb = 0.2126f * rgb[0] + 0.7152f * rgb[1] + 0.0722f * rgb[2]; // BT.709 weights, as the bloom composite reads it
                  sum += weighted_rgb;
                  peak = ((weighted_rgb > peak) ? weighted_rgb : peak);
                  count++;
               }
            }
            native_device_context->Unmap(capture->staging.get(), 0);
            capture->copy_pending = false;
            if (count > 0)
            {
               *out_mean = (float)(sum / count);
               *out_max = (float)peak;
               have_stats = true;
            }
         }
      }
      if (!capture->copy_pending)
      {
         native_device_context->CopySubresourceRegion(capture->staging.get(), 0, 0, 0, 0, tex.get(), 0, nullptr);
         capture->copy_pending = true;
      }
      return have_stats;
   }

   // Vanilla adds native_buffer * BloomTint * 4 * gate (gate = 1 below a (0.3, 0.59, 0.11) weighted scene RGB of 1.107); Luma adds pyramid *
   // BloomTint * 4 * BloomIntensity (pre-scaled by kBloomPyramidToNativeEnergy). The ratio of the two means is the
   // factor that constant is off by.
   static void LogBloomEnergy(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, Borderlands2GameDeviceData* gd, bool is_tps, float effective_threshold)
   {
      if ((gd->frame_counter % 120) != 0)
         return;
      ComPtr<ID3D11ShaderResourceView> native_srv;
      native_device_context->PSGetShaderResources(is_tps ? kNativeBloomSlotTPS : kNativeBloomSlotBL2, 1, native_srv.put());

      float native_mean = 0.f, native_max = 0.f, luma_mean = 0.f, luma_max = 0.f;
      DXGI_FORMAT native_fmt = DXGI_FORMAT_UNKNOWN, luma_fmt = DXGI_FORMAT_UNKNOWN; // luma_fmt: unused, SampleTextureStats reports it
      const bool have_native = SampleTextureStats(native_device, native_device_context, native_srv.get(), &gd->bloom_ab_native, &native_mean, &native_max, &native_fmt);
      const bool have_luma = SampleTextureStats(native_device, native_device_context, gd->srv_luma_bloom.get(), &gd->bloom_ab_luma, &luma_mean, &luma_max, &luma_fmt);
      if (!have_native || !have_luma || luma_mean <= 1e-9f)
         return;

      const float tint = (gd->bloom_tint_live[0] >= 0.f)
                            ? (gd->bloom_tint_live[0] + gd->bloom_tint_live[1] + gd->bloom_tint_live[2]) / 3.f
                            : 1.f;
      // Mirror of the two composites, so the ratio reads 1.0 when Bloom Intensity 1 really is vanilla strength.
      const float vanilla_add = native_mean * tint * 4.f;
      const float luma_add = luma_mean * tint * 4.f * cb_luma_global_settings.GameSettings.BloomIntensity;
      // Bloom Intensity at 0 makes the ratio meaningless, but the measured means still are: report those.
      const bool ratio_valid = luma_add > 1e-9f;
      const float ratio = (ratio_valid ? (vanilla_add / luma_add) : 0.f);
      LogLine(std::format("[BL-BloomAB] {} | thr_eff={:.4f} | native mean={:.6f} max={:.6f} fmt={} | luma mean={:.6f} max={:.6f} | tint={:.4f} | vanilla adds {:.6f}, we add {:.6f} -> x{:.4f}",
         is_tps ? "TPS" : "BL2", effective_threshold, native_mean, native_max, (uint32_t)native_fmt, luma_mean, luma_max, tint, vanilla_add, luma_add, ratio));

      // Fires on any scene, either game, where the shipped constant misses vanilla strength. Loose bound because the
      // ratio wanders with content (0.75-1.75 across BL2 areas, 0.59-1.27 across TPS), so only a real miss trips it.
      if (ratio_valid && native_mean > 1e-5f && (ratio < 0.5f || ratio > 2.0f))
      {
         LogLine(std::format("[BL-BloomAB] WARNING: bloom energy off by x{:.2f} on {} - kBloomPyramidToNativeEnergy ({:.4f}) may need re-measuring for this game",
            ratio, is_tps ? "TPS" : "BL2", kBloomPyramidToNativeEnergy));
      }
   }

   // The tonemap's own constants off cb4: rows 15..22 are DX9 c7..c14 (ImageAdjustments1..3, HalfResMaskRect,
   // DOFKernelSize, vignette). Raw values only; the curve itself lives in the shader, which continues it for HDR
   // (BL2TPS_TryBuildWorkingLuminance). cb4[16].rgb is the per-area bloom tint the bloom A/B mirrors.
   static void CaptureGradeConstants(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, Borderlands2GameDeviceData* gd)
   {
      constexpr uint32_t kFirstRow = 15;
      constexpr uint32_t kRows = 8;

      float rows[kRows * 4] = {};
      if (!CaptureConstantRows(native_device, native_device_context, &gd->grade_cb, 4, kFirstRow, kRows, rows))
         return;

      for (uint32_t i = 0; i < 3; i++)
      {
         gd->bloom_tint_live[i] = rows[1 * 4 + i]; // cb4[16].rgb
      }

      const float* ia2 = rows + 2 * 4; // cb4[17] = (A, Y, Z, W)
      const float* ia3 = rows + 3 * 4; // cb4[18].x = K

      // The HDR working continuation models the K = 0 curve only (BL2TPS_TryBuildWorkingLuminance in
      // Luma_BL2TPS_Tonemap.hlsl); on anything else it declines and the native graded colour is presented as-is.
      // K has never been observed non-zero, so say so loudly, once, if it ever is. Ahead of the de-dup below on
      // purpose: A and Z follow W, so exposure drift alone keeps adding sets until its 64-set cap returns early.
      if (ia3[0] != 0.f && !gd->grade_k_warned)
      {
         gd->grade_k_warned = true;
         LogLine(std::format("[BL-HDR] WARNING: non-zero ImageAdjustments K={:.5f}; the HDR working continuation does not "
                             "model this curve, so HDR falls back to the native graded range for it.",
            ia3[0]));
      }

      // W is the live exposure gain and moves every frame while the eye adapts, so it is quantized coarsely and
      // the shape parameters keep 1e-3: one line per distinct curve, not per frame.
      const float keyed[5] = {ia2[0], ia2[1], ia2[2], ia2[3], ia3[0]};
      const float quanta[5] = {1000.f, 1000.f, 1000.f, 20.f, 1000.f};
      if (gd->grade_cb_logged.size() >= 64 || !gd->grade_cb_logged.emplace(QuantizedKey(keyed, quanta, 5)).second)
         return;

      LogLine(std::format("[BL-Grade] A={:.5f} Y={:.5f} Z={:.5f} W={:.5f} K={:.5f} | vignette cb4[21]=({:.5f}, {:.5f}, {:.5f}, {:.5f}) cb4[22]=({:.5f}, {:.5f}, {:.5f}, {:.5f})",
         ia2[0], ia2[1], ia2[2], ia2[3], ia3[0],
         rows[6 * 4 + 0], rows[6 * 4 + 1], rows[6 * 4 + 2], rows[6 * 4 + 3],
         rows[7 * 4 + 0], rows[7 * 4 + 1], rows[7 * 4 + 2], rows[7 * 4 + 3]));
   }

#endif // DEVELOPMENT

   DrawOrDispatchOverrideType OnDrawOrDispatch(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, CommandListData& cmd_list_data, DeviceData& device_data, reshade::api::shader_stage stages, const ShaderHashesList<OneShaderPerPipeline>& original_shader_hashes, bool is_custom_pass, bool& updated_cbuffers, std::function<void()>* original_draw_dispatch_func) override
   {
      auto& gd = GetGameDeviceData(device_data);
      const bool is_immediate = native_device_context->GetType() == D3D11_DEVICE_CONTEXT_IMMEDIATE;

      // DLSS/FSR: the scene's draws with motion vectors or jitter (see "DrawWithMotionVectors"), and the upscaler at its first post pass
      if (gd.mv_active && is_immediate)
      {
#if DEVELOPMENT
         const Perf::HookTimer perf_timer{&gd.perf_window.hook_ns}; // The draws' own submission included
#endif
         if (!gd.mv_scene_done && std::ranges::any_of(kScenePostHashes, [&](uint32_t hash)
                                     { return original_shader_hashes.Contains(hash, reshade::api::shader_stage::pixel); }))
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
                  for (int slot = 0; slot < 8 && !gd.mv_scene_copy; slot++)
                  {
                     com_ptr<ID3D11Resource> resource;
                     if (srvs[slot])
                     {
                        srvs[slot]->GetResource(&resource);
                     }
                     if (!resource || resource == gd.mv_scene_color || !AreResourcesEqual(resource.get(), gd.mv_scene_color.get()))
                        continue;
#if DEVELOPMENT
                     gd.mv_stats.ended_by_scene_slot = slot;
#endif
                     gd.mv_scene_copy = resource;
                  }
               }
               EndScene(native_device, native_device_context, device_data);
            }
         }
         else if (!gd.mv_scene_done && !is_custom_pass && original_draw_dispatch_func && *original_draw_dispatch_func && (stages & reshade::api::shader_stage::vertex) == reshade::api::shader_stage::vertex)
         {
            com_ptr<ID3D11RenderTargetView> rtvs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
            com_ptr<ID3D11DepthStencilView> dsv;
            native_device_context->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, &rtvs[0], &dsv);
            if (!gd.mv_scene_open && dsv)
            {
               OpenScene(native_device, native_device_context, cmd_list_data, device_data, original_shader_hashes.vertex_shaders[0], dsv.get());
            }
            // The draw with dgVoodoo's per-target blend repaired, as every other draw (see "FixImpossiblePerRTBlend")
            const std::function<void()> draw = [&]
            {
               if (!FixImpossiblePerRTBlend(native_device, native_device_context, true, &gd, stages, original_shader_hashes, is_custom_pass, original_draw_dispatch_func))
               {
                  (*original_draw_dispatch_func)();
               }
            };
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
#if DEVELOPMENT
            // A pass reading the scene (or a copy of it) into another target before the end. Off while a "Performance Test" runs
            // (CPU on every draw that Publishing doesn't pay), as the audit below and the stats log.
            if (Perf::g_test == 0 && gd.mv_scene_open && gd.mv_scene_color && !gd.mv_reads_before_end.contains(uint32_t(original_shader_hashes.pixel_shaders[0])))
            {
               com_ptr<ID3D11Resource> target;
               if (rtvs[0])
               {
                  rtvs[0]->GetResource(&target);
               }
               com_ptr<ID3D11ShaderResourceView> srvs[8];
               native_device_context->PSGetShaderResources(0, 8, &srvs[0]);
               for (const auto& srv : srvs)
               {
                  com_ptr<ID3D11Resource> resource;
                  if (srv)
                  {
                     srv->GetResource(&resource);
                  }
                  if (!resource || target == gd.mv_scene_color || (resource != gd.mv_scene_color && !AreResourcesEqual(resource.get(), gd.mv_scene_color.get())))
                     continue;
                  uint4 size;
                  DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
                  if (target)
                  {
                     GetResourceInfo(target.get(), size, format);
                  }
                  gd.mv_reads_before_end.insert(uint32_t(original_shader_hashes.pixel_shaders[0]));
                  reshade::log::message(reshade::log::level::info, std::format("[BL2 MV] frame {}: PS 0x{:08X} reads the scene before its end, into {} {}x{} (depth {})", cb_luma_global_settings.FrameIndex,
                                                                      uint32_t(original_shader_hashes.pixel_shaders[0]), target ? GetFormatNameSafe(format) : "none", size.x, size.y, dsv ? "bound" : "none")
                                                                      .c_str());
                  break;
               }
            }
#endif
         }
#if DEVELOPMENT
         // A draw into the scene and its depth after the end
         else if (Perf::g_test == 0 && gd.mv_scene_done && gd.mv_scene_color && gd.mv_depth && !is_custom_pass && !gd.mv_draws_after_end.contains(uint32_t(original_shader_hashes.pixel_shaders[0])))
         {
            com_ptr<ID3D11RenderTargetView> rtv;
            com_ptr<ID3D11DepthStencilView> dsv;
            native_device_context->OMGetRenderTargets(1, &rtv, &dsv);
            com_ptr<ID3D11Resource> color, depth;
            if (rtv)
            {
               rtv->GetResource(&color);
            }
            if (dsv)
            {
               dsv->GetResource(&depth);
            }
            if (color == gd.mv_scene_color && depth == gd.mv_depth)
            {
               gd.mv_draws_after_end.insert(uint32_t(original_shader_hashes.pixel_shaders[0]));
               reshade::log::message(reshade::log::level::warning, std::format("[BL2 MV] frame {}: PS 0x{:08X} (VS 0x{:08X}) draws into the scene after its end (ended by 0x{:08X})", cb_luma_global_settings.FrameIndex,
                                                                      uint32_t(original_shader_hashes.pixel_shaders[0]), uint32_t(original_shader_hashes.vertex_shaders[0]), gd.mv_stats.ended_by)
                                                                      .c_str());
            }
         }
#endif
      }
      // Every draw without a patched shader: the game's own, if the last patched draw's are still bound
      if (is_immediate)
      {
         PatchedDraws::RestoreGameShader(native_device_context, &gd.mv_bound_vertex_shader);
         PatchedDraws::RestoreGameShader(native_device_context, &gd.mv_bound_pixel_shader);
      }

      if (is_immediate && !is_custom_pass && (stages & reshade::api::shader_stage::pixel) != 0)
      {
         // Track this area's authored bloom threshold off the native bright pass; the Luma prefilter needs it.
         if (ContainsPixelShader(original_shader_hashes, kBloomBrightPassHash, kBloomBrightPassHash_v281))
         {
            CaptureBloomConstants(native_device, native_device_context, &gd);
         }
      }

      // Scaleform item-card price-digit stencil repair: returns Replaced when it re-runs the draw itself.
      if (RepairScaleformStencilMask(native_device, native_device_context, &gd, original_shader_hashes, is_custom_pass, is_immediate, original_draw_dispatch_func))
         return DrawOrDispatchOverrideType::Replaced;

      // The tonemap draw: track its LDR target (Hide UI, FXAA override), capture the scene SRV, build the Luma bloom, then SMAA.
      if (is_immediate && IsAnyTonemap(original_shader_hashes))
      {
         // TPS's tonemap binds its textures at other slots (see "kLumaBloomSlotTPS")
         const bool is_tps = IsTPSTonemap(original_shader_hashes);
#if DEVELOPMENT
         // Read the grade constants off this very draw: they are the input the vanilla curve runs on, so they
         // must be sampled here rather than at present, when another volume's values may already be bound.
         // Not during a "Performance Test": its copy and map would land in the measured frames.
         if (Perf::g_test == 0)
         {
            CaptureGradeConstants(native_device, native_device_context, &gd);
         }
#endif
         gd.tonemap_fired_this_frame = true; // Hide UI scopes its post-tonemap alpha-blend skip to this span
         // The scene's post processing ran this frame (Core's upscaler status icon, DLSS-Best-Practices BK-1)
         device_data.has_drawn_main_post_processing = true;
         ComPtr<ID3D11RenderTargetView> rtv;
         native_device_context->OMGetRenderTargets(1, rtv.put(), nullptr);
         if (rtv)
         {
            ComPtr<ID3D11Resource> res;
            rtv->GetResource(res.put());
            if (res)
            {
               gd.ldr_buffer_handle = (uint64_t)res.get();
            }
         }
         // Capture the scene-color SRV (PS t0) for SMAA predication (its .a holds the encoded view depth) and for the
         // FXAA override, which recognizes BL2's resolve by it. The tonemap reads (doesn't overwrite) this buffer, so .a
         // is still valid when SMAA runs later this frame.
         ComPtr<ID3D11ShaderResourceView> scene_srv;
         native_device_context->PSGetShaderResources(0, 1, scene_srv.put());
         if (scene_srv)
         {
            gd.srv_scene_depth = scene_srv;
         }

#if ENABLE_BLOOM
         // Pyramidal bloom from the fp16 scene (tonemap t0), bound at the Luma bloom slot; the tonemap then ignores the native
         // bloom, so no doubling. The graphics state stack restores the tonemap's RT/PS/SRVs afterwards.
         if (g_luma_bloom_enable && scene_srv)
         {
            auto& gs = cb_luma_global_settings.GameSettings;

            // THE SOLE WRITER of the effective BloomIntensity - the UI only touches the raw slider. Only the Luma
            // composite reads it (the game's own bloom is never scaled), so it is written only where the pyramid runs.
            const float effective_intensity = g_bloom_intensity * kBloomPyramidToNativeEnergy;
            if (fabsf(gs.BloomIntensity - effective_intensity) > 1e-6f)
            {
               gs.BloomIntensity = effective_intensity;
               device_data.cb_luma_global_settings_dirty = true;
            }

            // Follow the area's authored knee: the prefilter reads GameSettings.BloomThreshold, kept live off the
            // native bright pass. 1.0 until the first readback, mid-range for this game (measured 0.50 to 1.32).
            // Never-captured means the bright-pass hash did not match (new dgVoodoo build, or the pass is not
            // byte-shared after all). The fallback looks plausible, so say it once.
            if (gd.bloom_threshold_live < 0.f && !gd.bloom_threshold_warned && gd.frame_counter > 600)
            {
               gd.bloom_threshold_warned = true;
               LogLine("[BL-Bloom] WARNING: the native bright pass never reported a threshold - the Luma bloom is running on the fallback, so its knee is not the game's");
            }
            const float bloom_threshold = (gd.bloom_threshold_live >= 0.f ? gd.bloom_threshold_live : default_luma_global_game_settings.BloomThreshold);
            if (fabsf(gs.BloomThreshold - bloom_threshold) > 1e-4f)
            {
               gs.BloomThreshold = bloom_threshold;
               device_data.cb_luma_global_settings_dirty = true;
            }

            DrawStateStack<DrawStateStackType::FullGraphics> bloom_state;
            bloom_state.Cache(native_device_context, device_data.uav_max_count);

            // DrawBloom binds only b11, so the prefilter would not see LumaSettings without this (donor: MELE).
            if (luma_settings_cbuffer_index < D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT)
            {
               ID3D11Buffer* settings_cb = device_data.luma_global_settings.get();
               native_device_context->PSSetConstantBuffers(luma_settings_cbuffer_index, 1, &settings_cb);
            }

            ComPtr<ID3D11ShaderResourceView> srv_karis;
            DrawKarisAverage(native_device, native_device_context, device_data, scene_srv.get(), srv_karis.put());
            if (srv_karis)
            {
               DrawBloom(native_device, native_device_context, device_data, srv_karis.get(), kBloomMipCount, kBloomSigmas, gd.srv_luma_bloom.put());
            }

            bloom_state.Restore(native_device_context);

            if (gd.srv_luma_bloom)
            {
               ID3D11ShaderResourceView* const luma_bloom_srv = gd.srv_luma_bloom.get();
               native_device_context->PSSetShaderResources(is_tps ? kLumaBloomSlotTPS : kLumaBloomSlotBL2, 1, &luma_bloom_srv);
            }
#if DEVELOPMENT
            // Every 120th frame, which is the "Performance Test" log window: off while one runs
            if (Perf::g_test == 0)
            {
               LogBloomEnergy(native_device, native_device_context, &gd, is_tps, gs.BloomThreshold);
            }
#endif
         }
#endif

#if ENABLE_SMAA
         // Run the original tonemap draw ourselves, then SMAA on its LDR output; a normal draw without the callback. On a frame the
         // upscaler already antialiased only RCAS runs.
         const bool antialias = (device_data.has_drawn_sr ? g_rcas_sharpness > 0.f : g_smaa_enable);
         if (antialias && original_draw_dispatch_func != nullptr)
         {
            // Returning Replaced short-circuits core's per-pass SetLumaConstantBuffers, so upload the grade CB
            // here or the draw samples last-present's values (one frame of lag on the sliders).
            SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, reshade::api::shader_stage::pixel, LumaConstantBufferType::LumaSettings);
            updated_cbuffers = true;
            (*original_draw_dispatch_func)();
            if (rtv)
            {
               RunPostTonemapSMAA(native_device, native_device_context, device_data, &gd, rtv.get(), !device_data.has_drawn_sr);
            }
            return DrawOrDispatchOverrideType::Replaced; // we ran the original draw ourselves
         }
#endif
      }

#if ENABLE_SMAA
      // Cancel the native FXAA resolve while SMAA or the upscaler is on, keyed on the resolve's own SOURCE, so nothing here has to know
      // which game it is in. Reading the scene the tonemap samples at t0 is BL2's shape (it draws before the
      // tonemap and nothing samples its output), so the draw is dropped. Reading the tonemap's output
      // is TPS's: it routes the antialiased frame to the buffer the HUD draws onto, so it is copied. Anything else (no
      // tonemap captured yet, an unhooked uber permutation, a recreated target, a size or format mismatch) keeps the
      // game's own draw: a second AA pass is harmless, a buffer left unwritten under the HUD is not.
      if ((g_smaa_enable || IsSRActive(device_data)) && is_immediate && !is_custom_pass && IsFXAA(original_shader_hashes))
      {
         ComPtr<ID3D11ShaderResourceView> fxaa_srv;
         native_device_context->PSGetShaderResources(0, 1, fxaa_srv.put());
         ComPtr<ID3D11Resource> src_res;
         if (fxaa_srv)
         {
            fxaa_srv->GetResource(src_res.put());
         }
         if (src_res)
         {
            ComPtr<ID3D11Resource> scene_res;
            if (gd.srv_scene_depth)
            {
               gd.srv_scene_depth->GetResource(scene_res.put());
            }
            if (src_res.get() == scene_res.get())
               return DrawOrDispatchOverrideType::Replaced; // BL2: upstream of the tonemap, its output feeds nothing

            if ((uint64_t)src_res.get() == gd.ldr_buffer_handle)
            {
               ComPtr<ID3D11RenderTargetView> fxaa_rtv;
               native_device_context->OMGetRenderTargets(1, fxaa_rtv.put(), nullptr);
               ComPtr<ID3D11Resource> dst_res;
               if (fxaa_rtv)
               {
                  fxaa_rtv->GetResource(dst_res.put());
               }
               // CopyResource silently no-ops on a mismatch, which would leave a stale buffer under the HUD.
               if (dst_res && dst_res.get() != src_res.get() && AreResourcesEqual(dst_res.get(), src_res.get()))
               {
                  native_device_context->CopyResource(dst_res.get(), src_res.get());
                  return DrawOrDispatchOverrideType::Replaced;
               }
            }
         }
      }
#endif

      // After the tonemap the Scaleform HUD is the only alpha-blended geometry. Keying on blend state rather than the
      // RT is buffer-independent: BL2 draws the HUD on the tonemap's LDR, TPS on a separate post-FXAA buffer.
      if (g_hide_ui && is_immediate && !is_custom_pass && gd.tonemap_fired_this_frame && !IsAnyTonemap(original_shader_hashes) && !IsFXAA(original_shader_hashes))
      {
         ComPtr<ID3D11BlendState> blend_state;
         FLOAT blend_factor[4];
         UINT sample_mask = 0;
         native_device_context->OMGetBlendState(blend_state.put(), blend_factor, &sample_mask);
         bool alpha_blended = false;
         if (blend_state)
         {
            D3D11_BLEND_DESC bd;
            blend_state->GetDesc(&bd);
            alpha_blended = bd.RenderTarget[0].BlendEnable != FALSE;
         }

         bool on_ldr_buffer = false;
         if (gd.ldr_buffer_handle)
         {
            ComPtr<ID3D11RenderTargetView> rtv;
            native_device_context->OMGetRenderTargets(1, rtv.put(), nullptr);
            if (rtv)
            {
               ComPtr<ID3D11Resource> res;
               rtv->GetResource(res.put());
               on_ldr_buffer = (res && (uint64_t)res.get() == gd.ldr_buffer_handle);
            }
         }

         if (alpha_blended || on_ldr_buffer)
            return DrawOrDispatchOverrideType::Skip;
      }

      // LAST on purpose: this one re-issues the draw itself, so it must yield to every hook above, or it runs
      // vanilla a pass another hook meant to take over.
      if (FixImpossiblePerRTBlend(native_device, native_device_context, is_immediate, &gd, stages, original_shader_hashes, is_custom_pass, original_draw_dispatch_func))
         return DrawOrDispatchOverrideType::Replaced;

      return DrawOrDispatchOverrideType::None;
   }

   void CleanExtraSRResources(DeviceData& device_data) override
   {
      GetGameDeviceData(device_data).release_sr_resources = true;
   }

   void OnPresent(ID3D11Device* native_device, DeviceData& device_data) override
   {
      auto& gd = GetGameDeviceData(device_data);
      gd.tonemap_fired_this_frame = false; // new frame: re-arm Hide UI's post-tonemap alpha-blend scope
      gd.frame_counter++;
      // Never carry a Scaleform mask span across frames.
      gd.dsv_scaleform_mask_active.reset();

      // DLSS/FSR: the history restarts after any frame it didn't draw (menus, loading, just picked); the selection and the motion
      // vector state are fixed here for the next frame (see "IsSRActive")
      gd.mv_jitter_allowed = device_data.has_drawn_sr;
      gd.sr_active = LatchSRFrame(device_data);
      // SMAA T2x runs on the upscalers' motion vectors and gives way to an upscaler. Turned off, its frames go, and the motion vectors
      // go as when no upscaler is picked.
      gd.t2x_phase = -1;
      gd.mv_filled = false;
      const bool t2x_was_active = gd.t2x_active;
      gd.t2x_active = g_smaa_enable && g_smaa_t2x && !gd.sr_active;
      if (t2x_was_active && !gd.t2x_active)
      {
         gd.ReleaseT2xFrames();
         gd.release_sr_resources = true;
      }
      gd.mv_active = IsSRActive(device_data) || g_mv_enable || gd.t2x_active;
      // The proxy's vertex constant mirror, while its CSMT layer runs; the buffer hooks only where it can't replace them. Registered on
      // this thread, the only one their events come from (ReShade changes its callback lists without a lock).
      {
         static const auto get_vertex_constants = reinterpret_cast<GetVertexConstantMirrorFn>(GetProcAddress(GetModuleHandleW(L"d3d9.dll"), kGetVertexConstantMirrorExport));
         const VertexConstantMirror* const mirror = (get_vertex_constants ? get_vertex_constants() : nullptr);
         gd.vertex_constants = ((mirror && mirror->version == VertexConstantMirror::VERSION && mirror->active) ? mirror : nullptr);
         if (!gd.vertex_constants)
         {
            gd.vertex_constants_copy.reset();
         }
         const bool buffer_hooks = gd.mv_active && (!gd.vertex_constants || !g_vc4_mirror || g_vc4_mirror_check);
         if (buffer_hooks != g_buffer_hooks_registered)
         {
            g_buffer_hooks_registered = buffer_hooks;
            if (buffer_hooks)
            {
               reshade::register_event<reshade::addon_event::map_buffer_region>(OnMapBufferRegion);
               reshade::register_event<reshade::addon_event::unmap_buffer_region>(OnUnmapBufferRegion);
               reshade::register_event<reshade::addon_event::update_buffer_region>(OnUpdateBufferRegion);
            }
            else
            {
               reshade::unregister_event<reshade::addon_event::map_buffer_region>(OnMapBufferRegion);
               reshade::unregister_event<reshade::addon_event::unmap_buffer_region>(OnUnmapBufferRegion);
               reshade::unregister_event<reshade::addon_event::update_buffer_region>(OnUpdateBufferRegion);
            }
         }
      }
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
         // The pooled vc4 copies only the pool holds (superseded, no object or camera keeps them) are free for the next ones, as many
         // as the last frame asked for: frames without a scene (loading, videos, menus) still copy every Unmap, and would keep their
         // peak otherwise. Without motion vectors, none.
#if DEVELOPMENT
         gd.mv_constants_thread = GetCurrentThreadId();
#endif
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
      // FSR's masks go once the fill stopped writing them (DLSS, the masks off, SMAA), like SMAA's resources below
      if (gd.mv_reactive_target && cb_luma_global_settings.FrameIndex - gd.sr_reactive_frame > smaa_idle_release_frames)
      {
         const std::unique_lock lock(gd.mv_mutex);
         gd.ReleaseReactiveMasks();
      }
#if ENABLE_SMAA
      // SMAA's resources go once it stopped running (the upscaler antialiases, or SMAA is off), the snapshot too once RCAS stopped as
      // well; on the render thread, between frames
      if (cb_luma_global_settings.FrameIndex - gd.smaa_frame > smaa_idle_release_frames)
      {
         gd.ReleaseSMAAScratch();
         ReleaseSMAA(device_data);
      }
      if (cb_luma_global_settings.FrameIndex - gd.snapshot_frame > smaa_idle_release_frames)
      {
         gd.ReleaseSnapshotScratch();
      }
      // SMAA writes the LDR itself once RCAS is off again: its temp goes too
      if (gd.tex_smaa_out && cb_luma_global_settings.FrameIndex - gd.smaa_out_frame > smaa_idle_release_frames)
      {
         gd.ReleaseSMAAOutput();
      }
      if (gd.t2x_frames[0] && cb_luma_global_settings.FrameIndex - gd.t2x_frame > smaa_idle_release_frames)
      {
         gd.ReleaseT2xFrames();
      }
#endif
#if ENABLE_BLOOM
      // Luma bloom off (from the overlay or MCP): its chains go, and the mip 0 view kept for the tonemap with them
      if (!g_luma_bloom_enable && gd.srv_luma_bloom)
      {
         gd.srv_luma_bloom.reset();
         ReleaseKarisAverage(device_data);
         ReleaseBloom();
      }
#endif
#if DEVELOPMENT
      // "Performance Test": closes this frame's timestamp set, reads back the finished ones (a log line every 120 frames, the first 30
      // after a settings change or a pause skipped), opens the next frame's
      com_ptr<ID3D11DeviceContext> perf_context;
      native_device->GetImmediateContext(&perf_context);
      gd.perf_timestamps.Close(perf_context.get());
      if (Perf::g_test != 0)
      {
         auto& window = gd.perf_window;
         const char* const aa = (IsSRActive(device_data) ? SR::GetTypeName(device_data.sr_type) : (g_mv_enable ? "MV only" : (g_smaa_enable ? "SMAA" : "none")));
         const std::string settings = std::format("mode=\"{}\" hook_timers={} aa={} vc4_filter={} vc4_pool={} blend_memo={} output={}x{}", perf_test_modes[Perf::g_test].name, Perf::g_hook_timers, aa,
            g_mv_buffer_filter, g_mv_constants_pool, g_blend_memo, uint32_t(device_data.output_resolution.x), uint32_t(device_data.output_resolution.y));
         // Also until the upscaler draws (the SR bridge's helper takes seconds to start, passing the color through meanwhile), and
         // longer after leaving it
         const bool sr_exit = std::exchange(gd.perf_sr_active, IsSRActive(device_data)) && !IsSRActive(device_data);
         const bool sr_starting = IsSRActive(device_data) && !sr_implementations[device_data.sr_type]->IsReady(device_data.GetSRInstanceData());
         const bool measuring = window.Present(settings, sr_exit ? perf_helper_exit_settle_frames : perf_settle_frames, sr_exit || sr_starting);
         auto& stats = window.stats;
         window.disjoint += gd.perf_timestamps.Collect(perf_context.get(), measuring, [&](const auto& frame)
            {
               stats.frame.Add(frame.Ms(Perf::FRAME_START, Perf::FRAME_END));
               if (!frame.Has(PERF_SCENE_START) || !frame.Has(PERF_SCENE_END))
                  return;
               stats.scene.Add(frame.Ms(PERF_SCENE_START, PERF_SCENE_END));
               if (!frame.Has(PERF_SCENE_TAIL_END))
                  return;
               stats.end.Add(frame.Ms(PERF_SCENE_END, PERF_SCENE_TAIL_END));
               if (frame.Has(PERF_FILL_END) && frame.Has(PERF_UPSCALER_END) && frame.Has(PERF_COPY_END))
               {
                  const size_t bounds[5] = {PERF_SCENE_END, PERF_FILL_END, PERF_UPSCALER_END, PERF_COPY_END, PERF_SCENE_TAIL_END};
                  for (int part = 0; part < 4; part++)
                  {
                     stats.end_parts[part].Add(frame.Ms(bounds[part], bounds[part + 1]));
                  }
               } });
         window.Finish(measuring, [&]
            {
            const double fill = stats.end_parts[0].Average(), upscaler = stats.end_parts[1].Average(), copy_back = stats.end_parts[2].Average(), scene_copy = stats.end_parts[3].Average();
            const double hooks = window.HookMs();
            // Read every window (a helper still exiting after the upscaler was left keeps writing), kept only where the upscaler ran
            const double helper_rows = ReadHelperEvaluateMs(gd);
            const double helper_evaluate = (stats.end_parts[1].samples != 0 ? helper_rows : std::numeric_limits<double>::quiet_NaN());
            reshade::log::message(reshade::log::level::info, std::format("[BL2 Perf] {} gpu frame avg/max={:.3f}/{:.3f} ms scene avg/max={:.3f}/{:.3f} ms ({}) end avg/max={:.3f}/{:.3f} ms ({}) = fill {:.3f} + upscaler {:.3f} (helper evaluate {:.3f}) + copy back {:.3f} + scene copy {:.3f} ms ({}) cpu frame avg={:.3f} ms cpu hooks={:.3f} ms/frame samples={}/{} disjoint={}",
                                                                settings, stats.frame.Average(), stats.frame.max_ms, stats.scene.Average(), stats.scene.max_ms, stats.scene.samples, stats.end.Average(), stats.end.max_ms, stats.end.samples, fill, upscaler, helper_evaluate, copy_back, scene_copy, stats.end_parts[0].samples, window.CpuFrameMs(), hooks, stats.frame.samples, window.frames, window.disjoint)
                                                                .c_str());
            // The row in "PerfColumn" order
            g_perf_sweep.OnWindow(Perf::g_test, {stats.frame.Average(), stats.scene.Average(), stats.end.Average(), hooks, window.CpuFrameMs(), fill, upscaler, copy_back, scene_copy, helper_evaluate}, [&](int mode_index)
               { ApplyPerfTestMode(device_data, mode_index); }, [&](int mode, int baseline_mode, double baseline)
               {
                     const double cpu_frame = g_perf_sweep.Median(mode, PERF_COLUMN_CPU_FRAME);
                     reshade::log::message(reshade::log::level::info, std::format("[BL2 Perf] sweep mode=\"{}\" hook_timers={} windows={} cpu frame median={:.3f} ms ({:.1f} fps) gpu frame median={:.3f} ms ({:+.3f} vs \"{}\") scene median={:.3f} ms end median={:.3f} ms (fill {:.3f} upscaler {:.3f} helper evaluate {:.3f} copy back {:.3f} scene copy {:.3f}) cpu hooks median={:.3f} ms/frame", perf_test_modes[mode].name, Perf::g_hook_timers, g_perf_sweep.results[mode].size(), cpu_frame, cpu_frame > 0.0 ? 1000.0 / cpu_frame : 0.0, g_perf_sweep.Median(mode, PERF_COLUMN_FRAME), g_perf_sweep.Median(mode, PERF_COLUMN_FRAME) - baseline, perf_test_modes[baseline_mode].name, g_perf_sweep.Median(mode, PERF_COLUMN_SCENE), g_perf_sweep.Median(mode, PERF_COLUMN_TAIL), g_perf_sweep.Median(mode, PERF_COLUMN_FILL), g_perf_sweep.Median(mode, PERF_COLUMN_UPSCALER), g_perf_sweep.Median(mode, PERF_COLUMN_HELPER_EVALUATE), g_perf_sweep.Median(mode, PERF_COLUMN_COPY_BACK), g_perf_sweep.Median(mode, PERF_COLUMN_SCENE_COPY), g_perf_sweep.Median(mode, PERF_COLUMN_HOOKS)).c_str()); }); });
         gd.perf_timestamps.Open(native_device, perf_context.get());
      }
#endif
      // A scene no post pass ended ends here: its jitter must not reach the next frame's draws before its first mesh
      gd.mv_scene_open = false;
      gd.mv_scene_done = false;
      gd.mv_frame_ended = true;
      gd.mv_fill_pending = false;
      if (!custom_texture_mip_lod_bias_offset)
      {
         const std::unique_lock lock(s_mutex_samplers);
         // -1 at native resolution (Core biases the game's anisotropic samplers, all upgraded to AF16x)
         device_data.texture_mip_lod_bias_offset = IsSRActive(device_data) ? SR::GetMipLODBias(device_data.output_resolution.y, device_data.output_resolution.y) : 0.f;
      }
#if DEVELOPMENT
      gd.mv_last_stats = std::exchange(gd.mv_stats, {});
      if (g_mv_trace_loading && gd.mv_last_stats.near_plane > 0.f && gd.mv_last_stats.near_plane < 1.f)
      {
         g_mv_trace_loading = false;
         trace_scheduled = true;
         reshade::log::message(reshade::log::level::info, std::format("[BL2 MV] frame {}: near plane {}, capture scheduled", cb_luma_global_settings.FrameIndex, gd.mv_last_stats.near_plane).c_str());
      }
      // The DEV panel's counts in ReShade.log every 300 frames while motion vectors run
      if (const auto& stats = gd.mv_last_stats; gd.mv_active && Perf::g_test == 0 && cb_luma_global_settings.FrameIndex % 300 == 0)
      {
         reshade::log::message(reshade::log::level::info, std::format("[BL2 MV] frame {}: {} mv ({} matched, {} camera only, {} other camera, {} uncopied), {} jitter, {} maps, {} updates, {} other maps, sr {} ({}), near {:.3f} far {:.0f}, ended by 0x{:08X} (scene slot {}, copy {}), refused {}/{}/{}/{}/{}/{}/{}/{}/{}, bound state mismatches {}, vc4 mirror {} (mismatches {}, in used rows {}, last at row {} of VS 0x{:08X}, unknown {})", cb_luma_global_settings.FrameIndex, stats.motion_vector_draws, stats.matched, stats.camera_only, stats.other_camera, stats.uncopied, stats.jitter_draws, stats.maps, stats.updates, stats.other_maps, stats.sr_draws, int(device_data.sr_type), stats.near_plane, stats.far_plane, stats.ended_by, stats.ended_by_scene_slot, gd.mv_scene_copy != nullptr, stats.rejected[0], stats.rejected[1], stats.rejected[2], stats.rejected[3], stats.rejected[4], stats.rejected[5], stats.rejected[6], stats.rejected[7], stats.rejected[8], stats.bound_state_mismatches, gd.vertex_constants ? "on" : "off", stats.vc4_mirror_mismatches, stats.vc4_mirror_used_mismatches, stats.vc4_mirror_mismatch_row, stats.vc4_mirror_mismatch_vs, stats.vc4_mirror_unknown).c_str());
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

   void LoadConfigs() override
   {
      reshade::get_config_value(nullptr, NAME, "SMAAEnable", g_smaa_enable);
      reshade::get_config_value(nullptr, NAME, "SMAAT2x", g_smaa_t2x);
      reshade::get_config_value(nullptr, NAME, "RCASSharpness", g_rcas_sharpness);
      // Predication has no shipping UI (see DrawImGuiSettings), but stays overridable from the ini.
      reshade::get_config_value(nullptr, NAME, "SMAAPredication", g_smaa_predication);
      reshade::get_config_value(nullptr, NAME, "SMAAPredicationTolerance", g_smaa_pred_tolerance);
      reshade::get_config_value(nullptr, NAME, "HideUI", g_hide_ui);

      // Grade sliders (cb_luma_global_settings_dirty is already true at init -> uploaded on first frame).
      auto& gs = cb_luma_global_settings.GameSettings;
      reshade::get_config_value(nullptr, NAME, "Exposure", gs.Exposure);
      reshade::get_config_value(nullptr, NAME, "Saturation", gs.Saturation);
      reshade::get_config_value(nullptr, NAME, "HighlightDechroma", gs.HighlightDechroma);
      reshade::get_config_value(nullptr, NAME, "BloomIntensity", g_bloom_intensity);
      reshade::get_config_value(nullptr, NAME, "Contrast", gs.Contrast);
      reshade::get_config_value(nullptr, NAME, "VignetteIntensity", gs.VignetteIntensity);

      reshade::get_config_value(nullptr, NAME, "LumaBloomEnable", g_luma_bloom_enable);
      gs.LumaBloomEnable = g_luma_bloom_enable ? 1.f : 0.f; // mirror to the shader composite switch

      reshade::get_config_value(nullptr, NAME, "Dithering", gs.Dithering);

      reshade::get_config_value(nullptr, NAME, "VideoAutoHDREnable", g_video_auto_hdr_enable);
      gs.VideoAutoHDREnable = g_video_auto_hdr_enable ? 1.f : 0.f; // mirror to the shader runtime gate
      reshade::get_config_value(nullptr, NAME, "VideoAutoHDRBoost", gs.VideoAutoHDRBoost);
   }

   void DrawImGuiSettings(DeviceData& device_data) override
   {
      ImGui::SeparatorText("Anti-Aliasing");
      const bool sr_active = IsSRActive(device_data);
      // The upscaler (Super Resolution, in the Settings tab) replaces SMAA: shown off, the saved choice is kept
      ImGui::BeginDisabled(sr_active);
      bool smaa_shown = g_smaa_enable && !sr_active;
      if (ImGui::Checkbox("SMAA Enable", sr_active ? &smaa_shown : &g_smaa_enable))
      {
         reshade::set_config_value(nullptr, NAME, "SMAAEnable", g_smaa_enable);
      }
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
      {
         ImGui::SetTooltip("Replaces the game's FXAA with SMAA (works with the game's Anti-aliasing setting on or off; not used with DLSS/FSR or Luma TAA).");
      }
      ImGui::EndDisabled();
      ImGui::BeginDisabled(!g_smaa_enable || sr_active);
      bool t2x_shown = g_smaa_t2x && !sr_active;
      if (ImGui::Checkbox("SMAA T2x", sr_active ? &t2x_shown : &g_smaa_t2x))
      {
         reshade::set_config_value(nullptr, NAME, "SMAAT2x", g_smaa_t2x);
      }
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
      {
         ImGui::SetTooltip("Smoother edges and less shimmering, by blending each frame with the previous one.\nCan look slightly softer in motion. Not used with DLSS/FSR or Luma TAA.");
      }
      ImGui::EndDisabled();
      ImGui::BeginDisabled(!g_smaa_enable && !sr_active);
      ImGui::SliderFloat("RCAS Sharpness", &g_rcas_sharpness, 0.f, 1.f);
      if (ImGui::IsItemDeactivatedAfterEdit())
      {
         reshade::set_config_value(nullptr, NAME, "RCASSharpness", g_rcas_sharpness);
      }
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
      {
         ImGui::SetTooltip("Sharpening applied on top of SMAA, DLSS/FSR or Luma TAA (0 = off).");
      }
      if (DrawResetButton<float, false>(g_rcas_sharpness, 0.f, "RCASSharpness"))
      {
         reshade::set_config_value(nullptr, NAME, "RCASSharpness", g_rcas_sharpness);
      }
      ImGui::EndDisabled();
      ImGui::BeginDisabled(!g_smaa_enable || sr_active);
#if DEVELOPMENT
      // Predication is not a preference: it only relaxes the edge threshold back to base ULTRA on geometry and
      // never below, so off is strictly worse. Kept as a bisect switch for devs, shipped on and out of sight.
      if (ImGui::Checkbox("SMAA Predication", &g_smaa_predication))
      {
         reshade::set_config_value(nullptr, NAME, "SMAAPredication", g_smaa_predication);
      }
      if (ImGui::IsItemHovered())
      {
         ImGui::SetTooltip("Finds edges by geometry (scene depth) instead of by brightness alone.\nKeeps cel-shade texture noise from being antialiased while still catching real silhouettes.");
      }
      if (ImGui::SliderFloat("SMAA Predication Tolerance", &g_smaa_pred_tolerance, 0.002f, 0.2f, "%.3f", ImGuiSliderFlags_Logarithmic))
      {
         reshade::set_config_value(nullptr, NAME, "SMAAPredicationTolerance", g_smaa_pred_tolerance);
      }
      if (ImGui::IsItemHovered())
      {
         ImGui::SetTooltip("How far a surface may deviate from its local plane before it counts as an edge,\nas a fraction of view depth. Lower = more edges. This is the calibration lever,\nnot the SMAA threshold. Logarithmic: the parameter is relative.");
      }
      ImGui::Checkbox("SMAA Predication Debug View", &g_smaa_pred_debug);
      if (ImGui::IsItemHovered())
      {
         ImGui::SetTooltip("Show the predication mask (red) instead of the frame.\nWant: black on flat surfaces, red across silhouettes.\nAll red = tolerance too low (predication is doing nothing).\nAll black = too high (silhouettes never regain sensitivity).");
      }
      if (ImGui::Button("Measure Predication"))
      {
         g_smaa_pred_measure = true;
      }
      if (ImGui::IsItemHovered())
      {
         ImGui::SetTooltip("Read the mask back and log its distribution to Luma-BL2.log (next to the exe).\nStand still, set a tolerance, press; repeat per value and compare the lines.\nFIRES(>0.5) is the share of the frame that regains base sensitivity.");
      }
#endif
      ImGui::EndDisabled();

#if DEVELOPMENT
      ImGui::SeparatorText("Motion Vectors (DLSS/FSR)");
      ImGui::Checkbox("MV Enable", &g_mv_enable);
      if (ImGui::IsItemHovered())
      {
         ImGui::SetTooltip("Draws the scene with the motion vector shaders without an upscaler. The image must not change; the debug view is\nblack with a static camera and lights up only what moves. ReShade.log: patched/refused shaders. Not saved.");
      }
      ImGui::Checkbox("MV Debug View", &g_mv_debug_view);
      ImGui::Checkbox("FSR Reactive Mask", &g_sr_reactive_enable);
      if (ImGui::IsItemHovered())
      {
         ImGui::SetTooltip("Marks the pixels alpha blended draws drew, so FSR trusts their history less: all of them as reactive (over the\nthreshold), the non-additive ones (smoke, glass, water) also as transparency & composition. Not saved.");
      }
      ImGui::SliderFloat("FSR Reactive Scale", &g_sr_reactive_scale, 0.f, 4.f);
      ImGui::SliderFloat("FSR Reactive Threshold", &g_sr_reactive_threshold, 0.f, 1.f);
      if (ImGui::IsItemHovered())
      {
         ImGui::SetTooltip("Scaled reactivity under it is 0, over it 0.9 (AMD's binary mask; AMD 0.2, default 0.5: lower makes static glows shake). 0: the scaled reactivity itself.");
      }
      ImGui::Checkbox("FSR Reactive Debug View", &g_sr_reactive_debug_view);
      ImGui::Checkbox("FSR T&C From Mask", &g_sr_tc_from_mask);
      if (ImGui::IsItemHovered())
      {
         ImGui::SetTooltip("Passes the reactive mask as FSR's transparency & composition mask too, instead of the alpha blended draws' own. Not saved.");
      }
      ImGui::Checkbox("FSR Reactive Zero Test", &g_sr_reactive_zero_test);
      ImGui::Checkbox("FSR Reactive Pass", &g_sr_reactive_pass);
      ImGui::Checkbox("FSR Reactive Skip Fill", &g_sr_reactive_skip_fill);
      ImGui::Checkbox("MV Force Jitter", &g_mv_force_jitter);
      if (ImGui::IsItemHovered())
      {
         ImGui::SetTooltip("Jitters the scene without an upscaler, with MV Enable. The image shakes by a subpixel; nothing may flicker or lose\npixels, and the debug view stays black with a static camera. Not saved.");
      }
      const auto& stats = GetGameDeviceData(device_data).mv_last_stats;
      ImGui::Text("%u mv (%u matched, %u camera only, %u other camera, %u uncopied), %u jitter, sr %u, ended by 0x%08X", stats.motion_vector_draws, stats.matched, stats.camera_only, stats.other_camera,
         stats.uncopied, stats.jitter_draws, stats.sr_draws, stats.ended_by);
      Perf::DrawCombo(perf_test_modes, &g_perf_sweep, [&](int mode_index)
         { ApplyPerfTestMode(device_data, mode_index); });
      if (ImGui::IsItemHovered())
      {
         ImGui::SetTooltip("Logs GPU and CPU times every 120 frames ([BL2 Perf] in ReShade.log): the frame, the scene, the end of the scene\n(fill, the upscaler, copies) and the scene hooks' CPU time. The first 30 frames after a settings change are skipped.\nKeep the camera still. \"Sweep\" runs the anti-aliasing modes, 3 rounds, then logs medians against No AA; \"CPU Sweep\"\nthe CPU savings each off in turn under the current DLSS/FSR, against Current Settings. Not saved.");
      }
      ImGui::Checkbox("Hook Timers", &Perf::g_hook_timers);
      if (ImGui::IsItemHovered())
      {
         ImGui::SetTooltip("Times the motion vector draw and buffer hooks for \"cpu hooks\" (two clock reads each, thousands a frame).\nRun a Sweep with it off to see their own cost in the frame times. The test also turns off the per draw diagnostics.");
      }
#endif

      // Grade sliders, read in Luma_BL2TPS_Tonemap.hlsl; every default is a vanilla no-op, and GameCBuffers.hlsl says
      // which of them act in SDR as well.
      ImGui::SeparatorText("Grade");
      auto& gs = cb_luma_global_settings.GameSettings;
      auto& default_game_settings = default_luma_global_game_settings;

      // A [0, max] float setting: slider, saved when the edit ends, reset button (saved too)
      const auto slider = [&](const char* label, float* value, float default_value, const char* key, float max, const char* tooltip)
      {
         if (ImGui::SliderFloat(label, value, 0.f, max))
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
         if (DrawResetButton<float, false>(*value, default_value, key))
         {
            device_data.cb_luma_global_settings_dirty = true;
            reshade::set_config_value(nullptr, NAME, key, *value);
         }
      };

      slider("Exposure", &gs.Exposure, default_game_settings.Exposure, "Exposure", 2.f, "Overall image brightness (1 = vanilla).");
      slider("Contrast", &gs.Contrast, default_game_settings.Contrast, "Contrast", 2.f, "Overall image contrast, HDR only (1 = vanilla).");
      slider("Saturation", &gs.Saturation, default_game_settings.Saturation, "Saturation", 2.f, "Color saturation, HDR only (1 = vanilla).");
      slider("Highlights Desaturation", &gs.HighlightDechroma, default_game_settings.HighlightDechroma, "HighlightDechroma", 1.f, "How far the brightest sources fade to neutral white, HDR only (0 = keep color at any brightness).");

      // A switch the shaders read as a float: checkbox, mirrored, saved
      const auto toggle = [&](const char* label, bool* value, float* shader_value, const char* key, const char* tooltip)
      {
         if (ImGui::Checkbox(label, value))
         {
            *shader_value = *value ? 1.f : 0.f;
            device_data.cb_luma_global_settings_dirty = true;
            reshade::set_config_value(nullptr, NAME, key, *value);
         }
         if (ImGui::IsItemHovered())
         {
            ImGui::SetTooltip("%s", tooltip);
         }
      };

      ImGui::SeparatorText("Bloom");
      toggle("Luma Bloom Enable", &g_luma_bloom_enable, &gs.LumaBloomEnable, "LumaBloomEnable", "Replaces the game's bloom with a wider, softer HDR bloom.");

      // Scales the injected Luma bloom only. With the pyramid off the game's own bloom runs at the strength its
      // artists authored, so there is nothing here to turn - hence disabled rather than silently inert.
      ImGui::BeginDisabled(!g_luma_bloom_enable);
      slider("Bloom Intensity", &g_bloom_intensity, 1.f, "BloomIntensity", 2.f, "Bloom strength (1 = vanilla, 0 = none).");
      ImGui::EndDisabled();

      ImGui::SeparatorText("Effects");
      slider("Vignette Intensity", &gs.VignetteIntensity, default_game_settings.VignetteIntensity, "VignetteIntensity", 1.f, "Scales the game's vignette darkening (1 = vanilla, 0 = none).");

      toggle("Video AutoHDR", &g_video_auto_hdr_enable, &gs.VideoAutoHDREnable, "VideoAutoHDREnable", "Adds HDR highlights to pre-rendered videos (HDR only).");

      ImGui::BeginDisabled(!g_video_auto_hdr_enable);
      slider("Video HDR Boost", &gs.VideoAutoHDRBoost, default_game_settings.VideoAutoHDRBoost, "VideoAutoHDRBoost", 1.f, "Video highlight strength (0 = off).");
      ImGui::EndDisabled();

      // Luma_BL2TPS_Tonemap.hlsl dithers in HDR and SDR alike, so this checkbox has no display-mode gate.
      bool dithering = gs.Dithering > 0.5f;
      if (ImGui::Checkbox("Dithering", &dithering))
      {
         gs.Dithering = dithering ? 1.f : 0.f;
         device_data.cb_luma_global_settings_dirty = true;
         reshade::set_config_value(nullptr, NAME, "Dithering", gs.Dithering);
      }
      if (ImGui::IsItemHovered())
      {
         ImGui::SetTooltip("Reduces gradient banding.");
      }

      ImGui::SeparatorText("UI");
      if (ImGui::Checkbox("Hide Gameplay UI", &g_hide_ui))
      {
         reshade::set_config_value(nullptr, NAME, "HideUI", g_hide_ui);
      }
      if (ImGui::IsItemHovered())
      {
         ImGui::SetTooltip("Disables the in-game UI.");
      }
   }

   void PrintImGuiAbout() override
   {
      ImGui::PushTextWrapPos(0.f);
      ImGui::Text(
         "Luma for \"Borderlands 2 & The Pre-Sequel\" is developed by DristoforColumb and is open source and free.\n"
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
                  "\nAMD FidelityFX (RCAS + FSR 3)"
                  "\nNVIDIA NGX (DLSS)"
                  "\ndgVoodoo2 by Dege (DirectX 9 -> 11 wrapper, required)");
   }
};

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
   if (ul_reason_for_call == DLL_PROCESS_ATTACH)
   {
      Globals::SetGlobals(PROJECT_NAME, "Borderlands 2 & The Pre-Sequel Luma mod");
      Globals::VERSION = 1;

      swapchain_format_upgrade_type = TextureFormatUpgradesType::AllowedEnabled;
      swapchain_upgrade_type = SwapchainUpgradeType::scRGB;
      texture_format_upgrades_type = TextureFormatUpgradesType::AllowedEnabled;
      enable_indirect_texture_format_upgrades = true;
      enable_chain_indirect_texture_format_upgrades = ChainTextureFormatUpgradesType::DirectDependencies;
      texture_upgrade_formats = {
         reshade::api::format::r8g8b8a8_unorm,
         reshade::api::format::r8g8b8a8_unorm_srgb,
         reshade::api::format::r8g8b8a8_typeless,
         reshade::api::format::r8g8b8x8_unorm,
         reshade::api::format::r8g8b8x8_unorm_srgb,
         reshade::api::format::b8g8r8a8_unorm,
         reshade::api::format::b8g8r8a8_unorm_srgb,
         reshade::api::format::b8g8r8a8_typeless,
         reshade::api::format::b8g8r8x8_unorm,
         reshade::api::format::b8g8r8x8_unorm_srgb,
         reshade::api::format::b8g8r8x8_typeless,

         reshade::api::format::r16g16b16a16_unorm,

         reshade::api::format::r10g10b10a2_unorm,
         reshade::api::format::r10g10b10a2_typeless,

         reshade::api::format::r11g11b10_float,
      };
      // The LDR backbuffer the tonemap writes (8-bit) + the bloom buffers are swapchain-res/aspect.
      texture_format_upgrades_2d_size_filters = 0 | (uint32_t)TextureFormatUpgrades2DSizeFilters::SwapchainResolution | (uint32_t)TextureFormatUpgrades2DSizeFilters::SwapchainAspectRatio | (uint32_t)TextureFormatUpgrades2DSizeFilters::No1Px;
      // The game runs within 16:9 unless aspect ratio is unlocked; force-upgrade that aspect too.
      int screen_width = GetSystemMetrics(SM_CXSCREEN);
      int screen_height = GetSystemMetrics(SM_CYSCREEN);
      texture_format_upgrades_2d_size_filters |= (uint32_t)TextureFormatUpgrades2DSizeFilters::CustomAspectRatio;
      texture_format_upgrades_2d_custom_aspect_ratios = {float(screen_width) / float(screen_height), 16.f / 9.f};

      // AF16x: mode 4 upgrades the game's AF samplers to MaxAnisotropy=16 (clarity on oblique surfaces). The LOD bias offset is
      // negative only under DLSS/FSR (see "OnPresent"): without a temporal resolve it would shimmer.
      enable_samplers_upgrade = true; // boot-time only (can't change after device creation)
      samplers_upgrade_mode = 4;

      game = new Borderlands2();
   }
   else if (ul_reason_for_call == DLL_PROCESS_DETACH)
   {
      Borderlands2::UnregisterEvents();
   }

   CoreMain(hModule, ul_reason_for_call, lpReserved);

   return TRUE;
}
