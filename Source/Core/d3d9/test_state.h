#pragma once

// Runtime state and test ABI of the native D3D9 module (core_d3d9.hpp): a DEVELOPMENT build exports "D9N_Get()" and the offline
// harness (_tools/d9_native_spike test.cpp, same process) reads and writes this POD directly. Layout is an ABI with the harness:
// append fields, never reorder or reinterpret them, and rebuild both sides.
#include <cstddef>
#include <cstdint>

// Pixel-shader float constants c208..c214 hold the injected Luma block
// (bayonetta/Luma_Block_sm3.hlsli); the game uses up to c202
// (bayonetta/NOTES.md). injected[] holds the settings rows; at each replaced
// draw the add-on overwrites the bound-state fields (c210.xy s0 size, c211.zw
// s0 texels per RT0 pixel, c212.xy kernel target size, c213.xy s14 size) before
// pushing all rows.
constexpr uint32_t INJECT_BASE_REGISTER = 208; // default; env D9N_INJECT_BASE
constexpr uint32_t INJECT_REGISTER_COUNT = 7;
constexpr uint32_t GAME_TARGET_SIZE_REGISTER = 194; // Bayonetta "g_TargetSize"
constexpr uint32_t SHADOW_MAP_SAMPLER =
   14; // ShadowReceive's R32F map (g_Sampler1)
constexpr uint32_t PERF_CAPACITY = 1024;
constexpr uint32_t MAX_PROBES = 4;
constexpr uint32_t MV_MAX_PROBES = 8; // motion / depth probes (mv_probe_*)
constexpr int32_t MUT_KEEP_SWAPCHAIN = 1, MUT_NO_GUARD = 2, MUT_NO_STICKY = 4,
                  MUT_NO_IMPLICIT_RT0 = 8, MUT_NO_RECOVERY = 16,
                  MUT_RESIZE_UNCHECKED = 32, MUT_NO_RESOLVE = 64,
                  MUT_NO_ENCODE = 128, MUT_NO_BT2020 = 256,
                  MUT_NO_ISCURRENT = 512, MUT_NO_RECONFIGURE = 1024,
                  MUT_RESIZE_DROP_LATENCY = 2048, MUT_NO_SHARED_LOSS = 4096,
                  MUT_NO_LATENCY_WAIT = 8192, MUT_NO_FORCE_IMMEDIATE = 16384,
                  MUT_NO_HOST_CONSTS = 32768, MUT_NO_KERNEL_RESTORE = 65536,
                  MUT_NO_PREV_BONES = 131072, MUT_NO_COPY_BACK = 262144,
                  MUT_NO_PASS_COPY_BACK = 524288, MUT_NO_SNAPSHOT = 1048576,
                  MUT_NO_T2X_HISTORY = 2097152, MUT_NO_TAA_HISTORY = 4194304,
                  MUT_NO_GTAO_PASS = 8388608, MUT_NO_DITHER = 16777216,
                  MUT_NO_SAMPLER_UPGRADE = 33554432, MUT_NO_HIDE_UI = 67108864,
                  MUT_NO_MSAA_DETECT = 134217728,
                  MUT_NO_OUTPUT_CLAMP = 268435456,
                  MUT_NO_LUMA_BLOOM = 536870912;
// Output modes (PROXY_PARITY P4): swapchain format + DXGI colour space of the
// D3D11 flip chain.
constexpr int32_t OUTPUT_SCRGB = 0, OUTPUT_HDR10 = 1, OUTPUT_SRGB = 2;

struct D9NState
{
   // Output device bring-up.
   int32_t is_ex;            // native D3D9 device answers IDirect3DDevice9Ex (create_device
                             // upgrade worked)
   int32_t windowed_forced;  // create_swapchain forced windowed at least once
   int32_t output_ready;     // D3D11 device + flip swapchain exist
   int32_t bootstrap_hr;     // throwaway non-flip swapchain on the game HWND
   int32_t flip_hr;          // FLIP_DISCARD R16G16B16A16_FLOAT swapchain on the game HWND
   int32_t color_space_hr;   // SetColorSpace1(scRGB)
   int32_t create_shared_ok; // D3D9 shared (KMT) fp16 RT created
   int32_t open_shared_hr;   // D3D11 OpenSharedResource
   int32_t last_present_hr;  // last D3D11 Present
   uint32_t width, height;   // current D3D9 back buffer = output size
   // Event counters.
   uint32_t presents, handoffs, resets_seen;
   uint32_t ps_replaced;    // pixel shaders swapped at create_pipeline
   uint32_t injected_draws; // draws that got push_constants
   uint32_t rt_upgraded, rt_skipped;
   uint32_t shared_created, shared_destroyed;
   uint32_t d3d11_device_rc; // refs on the output device after the last
                             // init_swapchain (stability check)
   // Leak accounting: sum of refcounts that were NOT zero after the add-on
   // released its own objects.
   int32_t d3d9_rc_left, d3d11_rc_left;
   // Teardown refcounts (informational: ReShade proxies hold their own
   // references).
   int32_t final_rc_device, final_rc_context, final_rc_swapchain;
   // Test-controlled input.
   float injected[4 * INJECT_REGISTER_COUNT];
   float paper_white;
   int32_t probe_count;
   int32_t probe_xy[2 * MAX_PROBES];
   // Output: D3D11 back buffer pixels read before Present.
   float probe_rgba[MAX_PROBES][4];
   // Per-present samples (test resets perf_count to 0 to start a run).
   uint32_t perf_count;
   float drain_ms[PERF_CAPACITY]; // D3D9: wait for the frame to finish on the
                                  // GPU before the copy
   float
      handoff_ms[PERF_CAPACITY];   // D3D9 StretchRect + EVENT query + flush wait
   float wait11_ms[PERF_CAPACITY]; // wait for the previous frame's D3D11 work
   float gpu_ms[PERF_CAPACITY];    // D3D11 composition GPU time (timestamp queries)
   // Debug: CRC32 of every pixel shader seen at create_pipeline.
   uint32_t seen_ps_count;
   uint32_t seen_ps_crc[16];
   // Output sizing / occlusion (test-controlled follow_window: ResizeBuffers to
   // the window client size at present).
   int32_t follow_window;
   int32_t output_disabled;        // test: skip the D3D11 hand-off/present (D3D9 alone)
   uint32_t out_width, out_height; // D3D11 flip swapchain size
   uint32_t output_resizes;
   int32_t resize_hr; // last ResizeBuffers
   // D3D11 Present results: S_OK / DXGI_STATUS_OCCLUDED / FAILED, and frames
   // skipped while a DXGI_PRESENT_TEST still says occluded.
   uint32_t present_ok, present_occluded, present_failed, present_skipped;
   int32_t first_failed_present_hr;
   int32_t last_test_interval1_hr; // same with sync interval 1
   int32_t last_test_present_hr;   // DXGI_PRESENT_TEST at the top of each present
   int32_t removed_after_resize, removed_before_present;
   int32_t
      device_removed_hr; // D3D11 GetDeviceRemovedReason after the last present
   // Minimal D3D9 back buffer variant (env D9N_MIN_BB=1): the add-on shrinks the
   // game's back buffer in create_swapchain; width/height stay the game's size.
   int32_t min_back_buffer;
   uint32_t bb_width, bb_height; // actual D3D9 back buffer
   // Bayonetta adaptation: render size (shared RT / RT upgrade filter),
   // injection base, composition viewport inside the output, hand-off source
   // counters.
   uint32_t render_width, render_height;
   uint32_t inject_base;
   uint32_t vp_x, vp_y, vp_w, vp_h;
   uint32_t handoff_draw, handoff_stretch, handoff_back_buffer;
   uint32_t rt0_back_buffer_binds;
   int32_t bb_redirect;     // render-target redirect of the back buffer into an fp16
                            // canvas
   int32_t bb_src_redirect; // copies out of the back buffer read the canvas
                            // (D9N_BB_SRC_REDIRECT, default 1)
   uint32_t copies_from_bb;
   uint32_t handoff_canvas, redirected_binds, redirected_copies,
      redirected_clears;
   // ReShade overlay on the proxy (PROXY_PARITY P1): runtimes seen per API,
   // reshade_open_overlay events per API (accepted / vetoed), open state of each
   // runtime, and a checksum of the D3D11 back buffer's top-left block taken at
   // reshade_present (after the overlay is drawn, before Present).
   int32_t overlay_fix; // 1 = veto the D3D9 runtime's overlay, disable its
                        // effects (env D9N_OVERLAY_FIX=0 turns it off)
   uint32_t runtimes_d3d9, runtimes_d3d11;
   uint32_t overlay_events_d3d9, overlay_events_d3d11, overlay_vetoed_d3d9;
   int32_t overlay_open_d3d9, overlay_open_d3d11;
   int32_t d3d9_effects_enabled; // last get_effects_state of the D3D9 runtime
   double overlay_block_sum;     // sum of RGB over the block read from the D3D11
                                 // back buffer
   uint32_t overlay_block_frames;
   // Proxy lifecycle (PROXY_PARITY P2/P3/P5/P10, d9native.cpp). mutate =
   // test-controlled bit mask that restores one old behaviour each (MUT_*), so a
   // check must fail without its fix.
   int32_t mutate;
   int32_t output_failed;    // redirect/output disabled after a creation failure
                             // (game keeps its own Present)
   int32_t same_adapter;     // D3D11 device created on the D3D9 adapter LUID
   int32_t d3d11_wrapped;    // ReShade wrapped the D3D11 device the add-on created
   int32_t msaa_back_buffer; // game back buffer is multisampled: redirect not
                             // possible
   uint32_t swapchain_creates, swapchain_releases, guard_ignored;
   uint32_t tests_issued; // DXGI_PRESENT_TEST calls
   uint32_t recoveries_test, recoveries_invalid;
   uint32_t implicit_rt0_rewrites, resolves_redirected;
   int32_t fault_present_invalid, fault_test_fail,
      fault_resize;             // test: remaining forced failures
   double present_event_end_ms; // QPC ms at the end of the add-on's present
                                // event (the game's D3D9 Present follows)
   // Output modes / display state (P4, items 3, 5). output_mode_request is
   // test/env controlled and may change at runtime.
   int32_t output_mode_request; // OUTPUT_* (env D9N_OUTPUT)
   int32_t output_mode;         // OUTPUT_* of the live swapchain
   int32_t out_format;          // DXGI_FORMAT of the live swapchain
   int32_t out_color_space;     // DXGI_COLOR_SPACE_TYPE set on it
   uint32_t reconfigures;       // mode changes that tore the swapchain down
   int32_t hdr_fallback;        // 1 = scRGB/HDR10 become sRGB when the output has no
                                // HDR (env D9N_HDR_FALLBACK=0 off)
   int32_t display_hdr;         // output with the largest window intersection reports
                                // G2084 P2020 (after force_display_hdr)
   int32_t display_hdr_actual;  // as reported by IDXGIOutput6::GetDesc1
   int32_t force_display_hdr;   // test: -1 = real, 0/1 = override
   float sdr_white_nits;        // DISPLAYCONFIG_SDR_WHITE_LEVEL of that output (80 when
                                // unavailable)
   int32_t sdr_white_hr;        // 0 = queried
   uint32_t display_queries, is_current_false, color_refreshes;
   int32_t fault_is_current;     // test: remaining forced IsCurrent == FALSE
   int32_t fault_cs_unsupported; // test: next CheckColorSpaceSupport reports
                                 // unsupported
   uint32_t color_space_fallbacks;
   // probe_rgba holds: fp16 values (scrgb), code / 1023 (hdr10), code / 255
   // (srgb). Latency (item 1) and sync (item 2).
   int32_t latency_enabled;      // env D9N_LATENCY (default 1), read when the
                                 // swapchain is created
   int32_t latency_wait_enabled; // 0 = flag/handle exist but no wait (A/B of the
                                 // added time)
   int32_t latency_flag;         // live swapchain has FRAME_LATENCY_WAITABLE_OBJECT
   int32_t latency_hr;           // SetMaximumFrameLatency on the DXGI swapchain
   int32_t d3d9_latency_hr,
      d3d9_latency; // D3D9Ex SetMaximumFrameLatency(1) result and read-back
   uint32_t latency_waits, latency_timeouts;
   uint32_t out_flags;      // DXGI_SWAP_CHAIN_FLAG of the live swapchain
   int32_t force_immediate; // env D9N_FORCE_IMMEDIATE: game's D3D9 presentation
                            // interval -> immediate (default keep)
   uint32_t game_interval_forced;
   int32_t sync_interval;           // D3D11 present sync interval (env D9N_SYNC_INTERVAL,
                                    // default 0)
   float latency_ms[PERF_CAPACITY]; // timed wait after the D3D11 Present
   float event_ms[PERF_CAPACITY];   // whole present event
   // Sharing contract (item 6).
   int32_t
      share_texture_path;    // shared RT is an IDirect3DTexture9 (CreateTexture
                             // path), render target usage, DEFAULT pool
   int32_t share_bind_ok;    // D3D11 side has SHADER_RESOURCE and RENDER_TARGET
                             // bind flags
   int32_t share_single_mip; // one mip, no MSAA on both sides
   int32_t fault_query_hung; // test: forced D3DERR_DEVICEHUNG from the hand-off
                             // query
   uint32_t shared_lost;
   int32_t clear_mode_unsupported; // test: forget modes the swapchain refused
                                   // (set to 1, consumed at the next present)
   // Host constants (bayonetta/NOTES.md 5.2, parity slice 5). kernel_720 = Fix
   // High Resolution Effects: kernel target size = RT0 size * 720 / RT0 height
   // (env D9N_KERNEL_720, default 1), else RT0 size. Kernel pixel shaders
   // (filtershadow_1..3, filtershaderssao; env D9N_KERNEL_PS adds CRCs) get c194
   // = kernel size pushed at their draw; the game's own c194 (game_c194, tracked
   // from its SetPixelShaderConstantF) is pushed back at the next other draw.
   int32_t kernel_720;
   float game_c194[4];
   uint32_t kernel_pushes, kernel_restores, host_rows_draws;
   // Present hand-off sync (kmt_sync.h, NOTES.md "E3 in the add-on").
   // sync_request: 0 = E0 CPU spin waits (default), 1 = E3 GPU fence through
   // D3DKMT (env D9N_SYNC=e3). e3_active: the fence path is set up and in use;
   // e3_gave_up: a setup or packet call failed and the add-on fell back to E0
   // for this device (e3_error = the failing NTSTATUS/HRESULT).
   // fault_kmt_signal: test, forced signal failures.
   int32_t sync_request, e3_active, e3_gave_up, e3_error, e3_hw_queue;
   uint32_t e3_signals, e3_waits, e3_setups;
   int32_t fault_kmt_signal;
   // Presenter helper (env D9N_PRESENTER=helper, presenter_protocol.h):
   // d9present64.exe presents instead of the D3D11 output device; output_*,
   // out_*, present_*, probe_rgba, overlay_block_* and latency_* then mirror its
   // PresenterStats.
   int32_t presenter_helper;
   uint32_t helper_starts, helper_deaths, helper_frames, helper_pid;
   int32_t helper_overlay; // last frame carried ReShade's D3D9 overlay
   int32_t helper_restart; // test: stop the helper normally at the next present
                           // (a new one starts)
   int32_t helper_block;   // ask for the overlay block sum (forces a helper GPU
                           // wait + readback per frame)
   // SR bridge (env D9N_SR, helper mode): Luma-Upscaler.exe ready (its output is
   // the presenter's scene), starts, frames upscaled.
   int32_t sr_ready;
   uint32_t sr_starts, sr_frames;
   // Motion vectors and jitter of the native path (env D9N_MV=1, implied by
   // D9N_SR; d9native.cpp "Motion vectors"). The SM3 patcher (sm3_mv_patch.h)
   // creates a patched copy of every VS / PS at init_pipeline; draws into the
   // scene (opened by the R32F prepass) bind them.
   int32_t mv_enabled;
   uint32_t
      mv_current_bones;  // unmatched skinned draws (previous bones = current)
   int32_t mv_vtf_ok;    // A32B32G32R32F vertex texture fetch supported (else MV
                         // is disabled)
   int32_t mv_indep_mrt; // motion target is G16R16F (else A16B16G16R16F: the
                         // MRT bit depths must match)
   uint32_t mv_patched_vs, mv_patched_ps, mv_refused_vs, mv_refused_ps,
      mv_shaders_live;
   uint32_t mv_scene_frames; // scenes opened
   uint32_t mv_scene_ends;   // scenes ended by a post pass (fill run)
   uint32_t mv_draws_prepass, mv_draws_patched, mv_draws_jitter_only,
      mv_draws_unpatched, mv_draws_blended;
   uint32_t mv_matches, mv_fallback_camera, mv_fallback_reprojection,
      mv_fallback_zero;
   float jitter_x, jitter_y; // this scene's jitter, pixels, +y down
   int32_t force_jitter;     // test / env D9N_MV_JITTER=1: jitter without an SR
   uint32_t sr_mid_frames;   // SR frames run at scene end
   // The last SR frame line: jitter (pixels), reset, FSR camera
   float sr_frame_jitter[2];
   int32_t sr_frame_reset;
   float sr_frame_camera[3]; // vertical fov, near, far
   uint32_t mv_copy_backs;   // RGB copy-backs (scene + the game's scene copy)
   // Probes of the shared motion (G16R16F UV delta previous - current, y down)
   // and depth (R32F) textures after the fill, read at scene end. The test sets
   // the count and pixels.
   int32_t mv_probe_count;
   int32_t mv_probe_xy[2 * MV_MAX_PROBES];
   float mv_probe_motion[MV_MAX_PROBES][2];
   float mv_probe_depth[MV_MAX_PROBES];
   // Raw (pre fill) motion target at the same pixels: 65504 = nothing wrote it.
   float mv_probe_raw[MV_MAX_PROBES][2];
   // P2 D3D12 passes (d9passes.cpp): RCAS round trip at the hand-off draw.
   float rcas_sharpness; // 0 = off; test / env D9N_RCAS
   int32_t pass_open;    // pass device + fences + pipeline ready
   uint32_t pass_runs, pass_failures;
   // ResetRender (canvas mode): display map pass, snapshot, present clamp
   // min(canvas, max(snapshot, 1)) in the presenter (env D9N_DISPLAY_MAP,
   // D9N_PRESENT_CLAMP; Shaders/Bayonetta/Includes/DisplayMap.hlsl settings).
   int32_t display_map, present_clamp;
   // Output dither in the composition (D9N_DITHER; Bayonetta's "Dithering",
   // Present_0x880A17D3): one 8 bit sRGB step in SDR, one 10 bit PQ step in HDR
   int32_t dither;
   float map_peak_nits, map_paper_white_nits, map_ui_paper_white_nits;
   float map_contrast, map_saturation, map_highlights_desaturation;
   uint32_t reset_renders, snapshots;
   int32_t smaa; // P3.2 SMAA 1x at ResetRender (D9N_SMAA)
   // SMAA predication from the SSAO pass' depth (D9N_SMAA_PREDICATION); runs
   // that had the depth
   int32_t smaa_predication;
   uint32_t predication_runs;
   // SMAA T2x (D9N_SMAA_T2X, needs the motion vectors at the canvas size and no
   // SR): runs, runs that had last frame as the history, this scene's phase (-1
   // none: not jittered)
   int32_t smaa_t2x;
   uint32_t t2x_runs, t2x_history_runs;
   int32_t t2x_phase;
   // P3.3 Luma TAA at 1x on the pass device as the SR (D9N_LUMA_TAA, instead
   // of the SR bridge): resolves, of them with the history reset
   int32_t luma_taa;
   uint32_t taa_runs, taa_resets;
   // P3.4 XeGTAO on the pass device in place of the game's SSAO draw
   // (D9N_GTAO, turns the motion vector path on for the prepass camera): runs,
   // SSAO draws left to the game (no camera this frame, no pass device)
   int32_t gtao;
   uint32_t gtao_runs, gtao_fallbacks;
   // P4 SetSamplerState hook (D9N_SAMPLER_UPGRADE, default on): the DX11 mod's
   // samplers_upgrade_mode 3, AF16 on trilinear / anisotropic pixel samplers and
   // the SR mip bias where the game's bias is 0. lod_bias: the bias applied now
   int32_t sampler_upgrade;
   float lod_bias;
   // HideUI (D9N_HIDE_UI, Bayonetta's "Hide UI"): the canvas draws after this
   // frame's ResetRender cancelled, movies kept
   int32_t hide_ui;
   uint32_t hidden_ui_draws;
   // The game's MSAA: an fp16 multisampled resolve in the last frame (the DX11
   // mod's msaa_active). While on: no motion vectors, so no jitter, SR, Luma TAA
   // or T2x
   int32_t msaa_active;
   // The DX11 mod's ClampOutput0 on the game's pixel shaders (D9N_OUTPUT_CLAMP,
   // default on; the upgraded fp16 targets don't clamp): BLEND_INPUT (rgb >= 0,
   // alpha 0..1) on all unreplaced, UNORM8 on the motion blur velocity (env
   // D9N_VELOCITY_PS adds CRCs). Per CRC: patched, refused
   int32_t output_clamp;
   uint32_t ps_clamped_blend, ps_clamped_velocity, ps_clamp_refused;
   // Luma Bloom (D9N_LUMA_BLOOM, default on): the bloom chain at bloom_scale
   // times the game's target. Chain draws redirected, copies mirrored,
   // composites given the Luma texture
   int32_t luma_bloom;
   uint32_t bloom_scale, bloom_draws, bloom_mirrors, bloom_composites;
};

inline uint32_t Crc32(const void* data, size_t size)
{
   uint32_t crc = 0xFFFFFFFF;
   for (size_t i = 0; i < size; ++i)
   {
      crc ^= static_cast<const uint8_t*>(data)[i];
      for (int bit = 0; bit < 8; ++bit)
      {
         crc = (crc >> 1) ^ (0xEDB88320 & (0u - (crc & 1)));
      }
   }
   return ~crc;
}
