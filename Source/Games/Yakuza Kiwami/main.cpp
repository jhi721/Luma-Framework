#define GAME_YAKUZA_KIWAMI 1

#define DISABLE_AUTO_DEBUGGER 1

// The engine's hardware probe creates a D3D11 device before the D3D12 ones: skip it
#define CHECK_GRAPHICS_API_COMPATIBILITY 1

// SMAA replaces the game's FXAA (see "OnDrawOrDispatch()"), RCAS sharpens its output
#define ENABLE_SMAA 1
#define ENABLE_RCAS 1
// SMAA's area texture without the U-shape smoothing (see Luma_SMAA_impl.hlsl)
#define SMAA_SMOOTH_U_SHAPES 0
// Luma Bloom replaces the glow pyramid (see "DrawLumaBloom()")
#define ENABLE_BLOOM 1

// The patched prepass draws of the characters are re-issued natively (see "DrawObjectMotionVectors()"): Core keeps their arguments, and the vertex buffers they read
#define ENABLE_DRAW_DISPATCH_DATA_CACHE 1

#include "..\..\Core\core.hpp"

#include "MotionVectorShaders.h"

// RGG "Old Engine" (the Kiwami 1 / Yakuza 3-5 family), D3D12 only (DXIL SM 6.0). See NOTES.md.
// The materials tonemap inline (Hable, then a gamma 2.0 encoding) into an 8-bit scene: their replacements continue the curve
// unbounded in HDR, every scene-sized target is upgraded to fp16 (placed textures get fp16 clones), and the "color correct"
// replacements (Shaders/Yakuza Kiwami/Includes/ColorCorrect.hlsl) map the scene to the display before FXAA, the HUD and render_fb.

namespace
{
   // ps_down_sample_2x4 (the glow source) and ps_down_sample_4x4 (the auto exposure meter source) read the fp16 scene into a target
   // that stays 8-bit: their replacements read it as vanilla stored it (see "YKR_SampleVanillaScene"), on the draws flagged here
   const ShaderHashesList shader_hashes_vanilla_scene_sources = {.pixel_shaders = {0xEDB2FBDE, 0x53B63FF4}};

   // SMAA in place of FXAA. D3D12 release builds don't know the resources behind the SRVs, so the inputs are taken where they are
   // written: the scene depth is the depth target of the depth + normals prepass (ps_normal, ps_normal_pt, ps_normal_tree, _pt, and the
   // characters' ps_normal_mask_n, _pt, _wk: the main menu's scene has only those), the canvas FXAA reads is the target of
   // ps_resolve_alphaluma (the ccr output with luma in alpha, for FXAA).
   const ShaderHashesList shader_hashes_depth_prepass = {.pixel_shaders = {0x3042199D, 0x4FA302B9, 0x4F16A152, 0xD9458156, 0x8146B106, 0xB805419B, 0x898DF083}};
   const ShaderHashesList shader_hashes_fxaa_input = {.pixel_shaders = {0xBF71EF76}};
   // fx_ssao_test, fx_ssao: their t0 is the scene depth, complete there and in a state pixel shaders read (the game clears it before
   // the post chain), so the predication draws from it at that point
   const ShaderHashesList shader_hashes_ssao = {.pixel_shaders = {0x5F373FFF, 0x83F4F60B}};
   // The low-res depth draw (t0 = the full D32S8, SV_Depth out): DLSS' depth copy is taken there. It comes after the hair prepass draws (which write
   // depth after the SSAO draw) and before the game clears the depth again, so DLSS' depth has the hair
   const ShaderHashesList shader_hashes_depth_copy = {.pixel_shaders = {0x053562E5}};
   // fx_fxaa311 q-low, q-medium, q-high, q-ultra. The "linear" permutations read a linear canvas: they stay
   const ShaderHashesList shader_hashes_fxaa = {.pixel_shaders = {0x47272CCB, 0xF1B4271C, 0xE4123563, 0x03933167}};

   // XeGTAO in place of the game's SSAO (FUN_140864d90, see "DrawXeGTAO()"): it runs at fx_ssao_test, which is skipped with fx_ssao_temporal_filter
   // and the 8 ps_ssao_blur draws; ps_ssao_upsampling writes the AO into the alpha of the screen shadow mask (the materials' t13.w, color write
   // mask A), XeGTAO's goes there in its place
   const ShaderHashesList shader_hashes_ssao_test = {.pixel_shaders = {0x5F373FFF}};
   const ShaderHashesList shader_hashes_ssao_filters = {.pixel_shaders = {0xC806318D, 0x2AA0CC2B}};
   const ShaderHashesList shader_hashes_ssao_upsampling = {.pixel_shaders = {0x18B4D161}};
   constexpr uint16_t gtao_depth_mip_count = 5; // XE_GTAO_DEPTH_MIP_LEVELS in Includes/XeGTAO.hlsl
   constexpr D3D12_RENDER_TARGET_BLEND_DESC alpha_only_blend = {.SrcBlend = D3D12_BLEND_ONE, .DestBlend = D3D12_BLEND_ZERO, .BlendOp = D3D12_BLEND_OP_ADD, .SrcBlendAlpha = D3D12_BLEND_ONE, .DestBlendAlpha = D3D12_BLEND_ZERO, .BlendOpAlpha = D3D12_BLEND_OP_ADD, .LogicOp = D3D12_LOGIC_OP_NOOP, .RenderTargetWriteMask = UINT8(D3D12_COLOR_WRITE_ENABLE_ALPHA)};
   // The glow (FUN_1402e7c70, see "DrawLumaBloom()"): glow_pass0 writes its level 0, the 8 glow_pass1 draws (the chromatic aberration variant
   // 0x8B729110 isn't replaced) blur it down to 4 levels, which glow_pass2 sums (glow_pass2_5 0x928D48D6 isn't replaced either)
   const ShaderHashesList shader_hashes_glow_pass0 = {.pixel_shaders = {0x88B980FC}};
   const ShaderHashesList shader_hashes_glow_pass1 = {.pixel_shaders = {0x4050FD15}};
   const ShaderHashesList shader_hashes_glow_pass1_ca = {.pixel_shaders = {0x8B729110}};
   const ShaderHashesList shader_hashes_glow_pass2 = {.pixel_shaders = {0x1EE5A3E2}};
   // The glow composite (FUN_1402dde60 -> FUN_1402e8840, after DoF): the glow target (glow_pass2's, RT 0x45) added onto the scene by ps_flickerfree_fast,
   // a plain copy the game also draws opaque elsewhere: only this draw blends ONE/ONE. The exposure meter (copy 0x7561D004, ps_down_sample_4x4,
   // with_glare 0x68A0C136) and the water read the glow target before it, so "Bloom Intensity" is applied here, not in the target
   const ShaderHashesList shader_hashes_glow_composite = {.pixel_shaders = {0xBA1E66BE}};
   constexpr D3D12_RENDER_TARGET_BLEND_DESC additive_blend = {.BlendEnable = TRUE, .SrcBlend = D3D12_BLEND_ONE, .DestBlend = D3D12_BLEND_ONE, .BlendOp = D3D12_BLEND_OP_ADD, .SrcBlendAlpha = D3D12_BLEND_ONE, .DestBlendAlpha = D3D12_BLEND_ONE, .BlendOpAlpha = D3D12_BLEND_OP_ADD, .LogicOp = D3D12_LOGIC_OP_NOOP, .RenderTargetWriteMask = UINT8(D3D12_COLOR_WRITE_ENABLE_ALL)};
   constexpr uint32_t bloom_mip_count = 4; // 512x256 down to 64x32, the vanilla levels
   // c_ps_user (cb9, PS b9) in the engine's constant mirror: "FUN_1400f29c0(row, data, rows)" writes it with descriptor 0x35102002190000 | row << 4,
   // whose bits 44+ are its base in the data area (see "GetConstantMirror()")
   constexpr uintptr_t cb9_base_offset = 0x3510;

   // The color correct (ccr) permutations: every ps_ccr_* (but ps_texture_a255) and the 3 fx_ccr_*_mask. Their t0 is the fp16 scene, rt0 of
   // the draw right before them on their list (the glow composite, or a late particle): DLAA runs on it there, before the grade.
   const ShaderHashesList shader_hashes_color_correct = {.pixel_shaders = {0x6D1C9FA9, 0x58F2C4F0, 0x9DAF7DB7, 0x9ECD97D6, 0x61966971, 0xB3FB5120, 0x17B5EBE2, 0x06317C91, 0xB65DCB82, 0x610F5616, 0x5ED223C8, 0x1CA987C6, 0xFC89801D, 0xEE029834, 0x40982C7F,
                                                            0xCB74D2A0, 0x4E638B19, 0x6A3DAC9E, 0xB82F56C5, 0x56F12D55, 0x5D77D604, 0xA78E78A7, 0xF0C9699F, 0xD7FA738E, 0x19119791, 0x6BCBF281, 0x20579838, 0xC29C4AB0, 0xCD437C1A, 0x205D75F8,
                                                            0x064069A9, 0xEEDDE2B4, 0xC198424E, 0x5EF619B4, 0xB6C95B59, 0x89305791, 0xBF672B8C, 0x5FAFE955, 0xB2A5A291, 0x58953ECA, 0x996843BB, 0x6AB907C7, 0xB0EFB0C5, 0x58514F0F}};

   bool g_smaa_enable = true;
   // Predication only relaxes the edge threshold back to ULTRA's on geometry, never below: off is strictly worse (MoH 2010, ME1)
   bool g_smaa_predication = true;
   // SMAA T2x without DLSS: the camera jitters by a quarter pixel in two phases and Core's SMAA blends with the previous frame along the motion vectors
   bool g_smaa_t2x = false;
   float g_rcas_sharpness = 0.f;    // RCAS on SMAA's output (after T2x's resolve), 0 = off
   bool g_hide_ui = false;          // Session only, so a restart always has a HUD
   bool g_luma_bloom_enable = true; // Not a setting: the DEV/MCP A/B against the game's glow pyramid
   float g_bloom_intensity = 1.f;
   bool g_gtao_enable = true;
   bool g_gtvbao_enable = false;         // GT-VBAO's main pass in place of XeGTAO's, on the same inputs, denoise and apply
   float g_gtao_final_value_power = 1.f; // DEV/TEST calibration knobs, not persisted
   float g_gtao_radius_override = 0.f;   // > 0 overrides the shader's EFFECT_RADIUS (view units, meters)
#if DEVELOPMENT
   int g_gtao_debug_view = 0; // 0 off, 1 depth gradient, 2 normals, 3 AO x8, 4 edges
   int g_gtao_temporal = 0;   // 0 = under DLAA or SMAA T2x (see "DrawXeGTAO()"), 1 = off, 2 = on
#else
   constexpr int g_gtao_temporal = 0;
#endif
   // The draws of the HUD phases that "Hide Gameplay UI" keeps besides the screen effects (see "screen_effect_nodes"): phase 0x19's copy of the FXAA
   // output into the output size target ("FUN_140486d50": sys_2d_no_color + ps_texture, the bytecode of ps_flickerfree_fast; ps_texture also draws
   // UI and screen noises in 0x1a), ps_render_fb and the videos (ps_sofdec, _alpha, _vp9)
   constexpr uint32_t depth_2d_phase = 0x19;
   constexpr uint32_t depth_2d_copy_vertex_shader = 0x3E35F20E;
   constexpr uint32_t depth_2d_copy_pixel_shader = 0xBA1E66BE;
   const ShaderHashesList shader_hashes_hud_phase_kept = {.pixel_shaders = {0xA8F2453C, 0x42769B75, 0x5B3CAE09, 0x549F26AA}};
   // The screen effects' shaders that add their own color over the canvas (see NOTES.md "The screen effects, resolved offline"): sys_diffuse
   // (the fade; also Rain2 before the ccr and the debug primitives), ps_diffuse_dither_a_4x4 (Gradation over, add and subtract; also a
   // world-space draw) and sys_modulatealpha (2D over, random noise add; also the HUD). Not the multiply Gradation, nor the noises that copy the canvas
   const ShaderHashesList shader_hashes_screen_effect_color = {.pixel_shaders = {0x85067FE9, 0x4CE9E58F, 0x8C7C87AF}};

   // The engine's projection setter "FUN_140babb00(context, const float4x4* projection)" (see NOTES.md "Exe RE"): it copies the matrix (row
   // vectors, clip w = -view z) into the context's cb12 mirror, for every pass, on several threads. The main camera's passes pass one of
   // the global camera slots, the others (shadows, cubemaps, icons, UI) their own matrices. Steam build only (exe TimeDateStamp below).
   constexpr DWORD game_time_date_stamp = 0x696F498D;
   constexpr uintptr_t set_projection_rva = 0xBABB00;
   // vmovups xmm1, [rdx]; mov rcx, [rcx+0x12C98]; vmovups [rcx+0x3A0], xmm1; vmovups xmm5, [rdx+0x10] (unique)
   constexpr uint8_t set_projection_prologue[] = {0xC5, 0xF8, 0x10, 0x0A, 0x48, 0x8B, 0x89, 0x98, 0x2C, 0x01, 0x00, 0xC5, 0xF8, 0x11, 0x89, 0xA0, 0x03, 0x00, 0x00, 0xC5, 0xF8, 0x10, 0x6A, 0x10};
   // MinHook can't hook it: its disassembler stops at the VEX encoded instructions around the "mov rcx". So the 7 byte "mov rcx" at +4
   // becomes "jmp rel32" (+ 2 nops) to a relay near the exe that jumps to the detour, written with the 16 aligned bytes around it in one
   // atomic operation (a thread runs either the old or the new code). The original runs from a stub: the first 11 bytes (no RIP relative
   // operand), then a jump to +11.
   constexpr uintptr_t set_projection_patch_offset = 4;
   constexpr size_t set_projection_stub_bytes = 11;
   constexpr uint8_t jmp_absolute[] = {0xFF, 0x25, 0x00, 0x00, 0x00, 0x00}; // jmp [rip+0], then the address
   // The 4 global camera slots' views, then their projections (a matrix each). Slot 1 is the planar reflection's, which gets no jitter.
   constexpr uintptr_t camera_views_rva = 0x15CBAF0;
   constexpr uintptr_t camera_projections_rva = 0x15CBBF0;
   constexpr uintptr_t camera_matrix_size = 16 * sizeof(float);

   using SetProjection = void (*)(void* context, const float* projection);
   SetProjection set_projection_original = nullptr; // Null without the hook (unknown build): no DLAA
   uintptr_t game_module = 0;
   bool projection_hook_attempted = false; // At the first present

   // The replay thread's constant mirror (the draw world matrix, see "OnDrawOrDispatch()"), from "FUN_1400ee460" and the pre-draw commit "FUN_140ba5b70": the thread finds
   // its D3D12 context through the exe's TLS, "ctx = *(*(game_module + render_globals_rva) + thread_contexts_offset + thread_number * 8)", the
   // engine's fallback context "*(... + fallback_context_offset)" if that is null. The mirror "*(ctx + context_mirror_offset)" holds the
   // cbuffers the commit flushed right before the draw, cb10 (id 0x0a: bones "c_matTBL", the rigid "c_matW"; 3 float4 rows per bone) at
   // "mirror + mirror_data_offset + cb10_base_offset"
   constexpr uintptr_t render_globals_rva = 0x1D39AA0; // A pointer to the render globals
   constexpr uintptr_t thread_contexts_offset = 0x38A228;
   constexpr uintptr_t fallback_context_offset = 0xC0;
   constexpr uintptr_t context_mirror_offset = 0x12C98;
   constexpr uintptr_t mirror_data_offset = 0x420;
   constexpr uintptr_t cb10_base_offset = 0x250;
   // cb12 (id 0x0c, VS|PS|GS|DS: the transposed projection, then the view, ...; 496 bytes, bound as 512) at the same base: descriptor base
   // 0x60 -> "mirror + 0x480" (_tools/ykr/mv/ykr_re_bones.md, section 1)
   constexpr uintptr_t cb12_base_offset = 0x60;
   constexpr uint64_t cb12_size = 496;
   constexpr uintptr_t tls_thread_number_offset = 0x80; // In the exe's TLS block, 0 until the engine numbers the thread
   constexpr uintptr_t tls_initialized_offset = 0xA8;   // The MSVC on-demand TLS init flag
   constexpr uint32_t tls_index_unknown = UINT32_MAX;
   uint32_t exe_tls_index = tls_index_unknown; // "_tls_index" of the exe, set with "game_module"
   // The phase of the frame the engine records on this thread: "FUN_140488770" writes the action's id there before it binds the action's camera
   // through the projection setter, the shadow map job "FUN_140160b60" too, and nothing else writes it. The actions run in id order: 0x17 the post
   // effects with the color correct (where DLAA draws) last, 0x18 FXAA, 0x19 the world-space HUD, 0x1a the 2D (_tools/ykr/sys/sys_shaders.md)
   constexpr uintptr_t tls_frame_phase_offset = 0x3C;
   constexpr uint32_t first_post_anti_aliasing_phase = 0x18;
   constexpr uint32_t last_post_anti_aliasing_phase = 0x1A;

   // The exe's TLS block of this thread, null for an unknown build or before the engine's on-demand TLS init
   const uint8_t* GetExeTlsBlock()
   {
      if (exe_tls_index == tls_index_unknown)
      {
         return nullptr;
      }
      const auto* const tls_blocks = reinterpret_cast<const uint8_t* const*>(__readgsqword(0x58)); // "ThreadLocalStoragePointer"
      const uint8_t* const tls_block = (tls_blocks ? tls_blocks[exe_tls_index] : nullptr);
      return ((tls_block && tls_block[tls_initialized_offset] != 0) ? tls_block : nullptr);
   }

   // A pass recorded on a worker list may be recorded after the list that reads it, the GPU still runs it first (it's submitted first): last
   // frame's stamp stands for this frame's too
   bool IsThisOrLastFrame(uint64_t stamp, uint64_t present_count)
   {
      return stamp != UINT64_MAX && (stamp == present_count || stamp + 1 == present_count);
   }

   // See "tls_frame_phase_offset", UINT32_MAX without the TLS block
   uint32_t GetFramePhase()
   {
      const uint8_t* const tls_block = GetExeTlsBlock();
      return (tls_block ? *reinterpret_cast<const uint32_t*>(tls_block + tls_frame_phase_offset) : UINT32_MAX);
   }

   // The vertex shaders of the depth prepass draws with a world matrix in cb10 (bone 0, or a rigid part's c_matW: "FUN_1400f1180" uploads both
   // as id 0x0a, _tools/ykr/mv/ykr_re_bones.md): the characters' (ps_normal_mask_n*), the skinned ps_normal, the hair, then the rigid world's
   // (ps_normal, ps_normal_pt) and the trees' (ps_normal_tree, wind on top). Last, vs_normal_skin_pt and vs_depth_skin, never seen in game (the
   // engine picks the prepass shaders by material); vs_depth_skin is also the highlight mask's VS, whose pass has no pair in "MotionVectorShaders.h"
   const ShaderHashesList shader_hashes_object_prepass = {.vertex_shaders = {0x026FFF36, 0x6AE49DE2, 0xEEFE68EE, 0xDBB9339C, 0x6A6B42A3, 0x277CAA28, 0x0ED40764, 0x74E0172A, 0xC3E471F7, 0xD5A379DD}};
   // The rigid world's: matched draw by draw on their world matrix (see "rigid_draws")
   const ShaderHashesList shader_hashes_rigid_prepass = {.vertex_shaders = {0x277CAA28, 0x0ED40764}};
   constexpr size_t world_matrix_floats = 12; // 3x4, row-major, translations in floats 3, 7, 11
   constexpr uint64_t world_matrix_size = world_matrix_floats * sizeof(float);

   // Whether the camera jitters and has its motion vectors made (DLAA or SMAA T2x) and the jitter of the main camera's projections (0 in a frame
   // after one they didn't draw), latched at present for the next frame, and the main camera as its passes set it
   struct CameraState
   {
      std::shared_mutex mutex;
      bool motion_active = false;
      std::array<float, 2> jitter_pixels = {}; // As applied: +y down
      std::array<float, 2> jitter_ndc = {};
      // The last unjittered view and projection of slot 0, which DLAA reprojects with, and the jitter they got
      struct Capture
      {
         std::array<float, 16> view = {};
         std::array<float, 16> projection = {};
         std::array<float, 2> jitter_pixels = {};
         std::array<float, 2> jitter_ndc = {};
      };
      std::optional<Capture> capture;
   };
   CameraState camera_state;

   // The scene (rt0) of this thread's last draw and its state since, from the game's barriers (see "OnBarrier()"): what a ccr draw reads
   struct SceneWriter
   {
      uint64_t resource = 0;
      reshade::api::resource_view_desc desc = {};
      reshade::api::resource_usage state = reshade::api::resource_usage::render_target;
      uint64_t present_count = UINT64_MAX; // Of that draw: one from an older frame may be of a texture the game destroyed since
   };
   thread_local SceneWriter scene_writer;

   // glow_pass1_ca drew this glow chain's levels (one chain records on one thread, from glow_pass0): its glow_pass2 stays the game's
   thread_local bool glow_aberration_drawn = false;

   // DLAA writes the scene's color back, its alpha stays (some ccr permutations read it)
   constexpr D3D12_RENDER_TARGET_BLEND_DESC rgb_only_blend = {.SrcBlend = D3D12_BLEND_ONE, .DestBlend = D3D12_BLEND_ZERO, .BlendOp = D3D12_BLEND_OP_ADD, .SrcBlendAlpha = D3D12_BLEND_ONE, .DestBlendAlpha = D3D12_BLEND_ZERO, .BlendOpAlpha = D3D12_BLEND_OP_ADD, .LogicOp = D3D12_LOGIC_OP_NOOP, .RenderTargetWriteMask = UINT8(D3D12_COLOR_WRITE_ENABLE_RED | D3D12_COLOR_WRITE_ENABLE_GREEN | D3D12_COLOR_WRITE_ENABLE_BLUE)};

   PUBLISHING_CONSTEXPR bool g_dlaa_mip_bias = true;
#if DEVELOPMENT
   bool g_mv_debug_view = false;
   constexpr const char* mv_debug_view_texture = "dlaa.motion_vectors"; // Registered with the dev MCP
#endif

   // Object motion vectors ("Object motion vectors" in NOTES.md): the prepass draws of the characters, the hair and the trees run the offline patched
   // shaders of "MotionVectorShaders.h", which keep every vertex' position in a cache, take last frame's from the other one (design B), and write the
   // motion between them to a second render target ("DrawObjectMotionVectors()"); the rigid world's moved parts get last frame's world matrix instead
   // (design A). DLAA's motion vector pass uses the target where it was written.
   PUBLISHING_CONSTEXPR bool g_object_motion_vectors = true;
   // The anchors of two draw runs (bone 0's world position) at most this far apart (world units) are the same object in two frames
   PUBLISHING_CONSTEXPR float g_object_mv_match_distance = 0.5f;
   // A previous position further than this (world units) from where it would be if the vertex were static is a wrong match: camera motion instead
   PUBLISHING_CONSTEXPR float g_object_mv_max_distance = 1.f;
   // The rigid world's draws are matched one by one on their world matrix ("rigid_draws"): an unchanged one is static (the camera motion is
   // exact for it), else the nearest of last frame's within this distance (world units) is the same part moved
   PUBLISHING_CONSTEXPR bool g_object_mv_rigid = true;
   PUBLISHING_CONSTEXPR float g_object_mv_rigid_match_distance = 1.f;

   // The largest float16, what the object motion vector target is cleared to (MOTION_VECTOR_MARKER, see "Luma_YKR_DLAA.hlsl")
   constexpr float motion_vector_marker = 65504.f;
   // A position cache holds this many vertices of the largest stride. A slot's base is in vertices of its pair's stride (the shader multiplies), so
   // slots are laid out in bytes: 12 per vertex for the characters and the hair, 16 for the trees (their wind also moves w). The first 16 bytes are
   // the trash slot that vertices outside their object's slot read and write
   constexpr uint32_t object_mv_cache_vertices = 1u << 21;
   constexpr uint32_t object_mv_max_vertex_stride = 4 * sizeof(float);
   constexpr uint64_t object_mv_cache_bytes = uint64_t(object_mv_cache_vertices) * object_mv_max_vertex_stride;
   constexpr uint32_t object_mv_max_slot_vertices = 65536; // 16 bit indices
   // The threads that record patched draws, each with its own native descriptors (see "GetObjectMotionThreadSlot()")
   constexpr uint32_t object_mv_max_threads = 64;
   // The patched vertex shaders' b9: last frame's cb12 (31 rows), then the jitter and gate row, the c_location delta row, and last frame's c_matW
   // (3 rows) of the rigid world's draw (only its shaders read it)
   constexpr size_t object_mv_camera_floats = 36 * 4;
   constexpr size_t object_mv_previous_world = 33 * 4;
   constexpr size_t cb12_floats = cb12_size / sizeof(float);
   static_assert(cb12_floats == 31 * 4);
   // cb12 holds the transposed projection: P._31 and P._32 (the jitter the projection hook moved) are the z of its rows 0 and 1
   constexpr size_t cb12_projection_31 = 2;
   constexpr size_t cb12_projection_32 = 6;
   // Its transposed view (rows 4..7) and view projection (rows 8..11, P x V transposed: the trees' VS reads it)
   constexpr size_t cb12_view = 4 * 4;
   constexpr size_t cb12_view_projection = 8 * 4;
   constexpr size_t cb12_location = 30 * 4;

   enum ObjectMotionCount : size_t
   {
      MV_PATCHED,
      MV_MATCHED,
      MV_NEW,
      MV_OVERFLOW,
      MV_TIEBREAK_COLLISIONS,
      MV_RIGID_STATIC,
      MV_RIGID_MOVING,
      MV_RIGID_NEW,
      MV_REFUSED_BUNDLE,
      MV_REFUSED_RENDER_PASS,
      MV_REFUSED_NOT_INDEXED,
      MV_REFUSED_INSTANCED,
      MV_REFUSED_BASE_VERTEX,
      MV_REFUSED_RT_COUNT,
      MV_REFUSED_OTHER_DSV,
      MV_REFUSED_NO_TEXTURE,
      MV_REFUSED_NO_GAME_PARAMS,
      MV_REFUSED_NO_VARIANT,
      MV_REFUSED_NO_THREAD_SLOT,
      MV_REFUSED_NO_MIRROR,
      MV_REFUSED_NO_VERTEX_BUFFER,
      MV_REFUSED_BAD_VIEW,
      MV_REFUSED_NO_BUFFERS,
      MV_COUNT,
   };
#if DEVELOPMENT
   constexpr std::array<const char*, MV_COUNT> object_motion_count_names = {"mv.patched", "mv.matched", "mv.new", "mv.overflow", "mv.tiebreak_collisions", "mv.rigid_static", "mv.rigid_moving", "mv.rigid_new", "mv.refused.bundle", "mv.refused.render_pass", "mv.refused.not_indexed", "mv.refused.instanced", "mv.refused.base_vertex", "mv.refused.rt_count", "mv.refused.other_dsv", "mv.refused.no_texture", "mv.refused.no_game_params", "mv.refused.no_variant", "mv.refused.no_thread_slot", "mv.refused.no_mirror", "mv.refused.no_vertex_buffer", "mv.refused.bad_view", "mv.refused.no_buffers"};
   // Being counted, and the last frame's
   std::array<uint32_t, MV_COUNT> object_motion_counts = {};
   std::array<uint32_t, MV_COUNT> object_motion_counts_frame = {};
#endif

   void CountObjectMotion([[maybe_unused]] ObjectMotionCount count)
   {
#if DEVELOPMENT
      std::atomic_ref(object_motion_counts[count]).fetch_add(1, std::memory_order_relaxed);
#endif
   }

   // The vertex buffer a draw reads its first stream from
   struct ObjectMotionVertexBuffer
   {
      uint64_t buffer = 0;
      uint64_t offset = 0;
      bool operator==(const ObjectMotionVertexBuffer&) const = default;
   };

   // A rigid part of the world: its asset's vertex buffer and its first index (a part's draw always starts there)
   struct RigidPartKey
   {
      ObjectMotionVertexBuffer vertex_buffer;
      uint32_t first_index = 0;
      bool operator==(const RigidPartKey&) const = default;
   };
   struct RigidPartKeyHash
   {
      size_t operator()(const RigidPartKey& key) const
      {
         return size_t(key.vertex_buffer.buffer ^ (key.vertex_buffer.offset * 0x9E3779B97F4A7C15ull) ^ (uint64_t(key.first_index) * 0xC2B2AE3D27D4EB4Full));
      }
   };
   enum class RigidMotion
   {
      STATIC,
      MOVING,
      NEW,
   };

   // A character or hair mesh instance of a frame: the parts of one asset drawn in a row
   struct ObjectMotionObject
   {
      ObjectMotionVertexBuffer vertex_buffer;
      uint32_t first_index = 0; // Of its first part
      std::array<float, 3> anchor = {};
      uint32_t write_base = 0;   // Its slot in the cache this frame wrote
      uint32_t vertex_count = 0; // 0: no slot (trash)
      bool claimed = false;
      std::vector<uint32_t> parts; // The first indices of the parts it drew, rising: only their vertices are in its slot
   };

   // cb12 as the engine's mirror held it for the main camera at the first patched draw of a frame, and the jitter that frame's projections got
   struct ObjectMotionCamera
   {
      std::array<float, cb12_floats> cb12 = {};
      std::array<float, 2> jitter_ndc = {};
   };

   // The run of parts of one object this thread is recording on this command list in this frame: its draws' root arguments
   struct ObjectMotionRun
   {
      const void* cmd_list = nullptr;
      uint64_t stamp = UINT64_MAX;
      ObjectMotionVertexBuffer vertex_buffer;
      uint32_t last_first_index = 0;
      std::array<uint32_t, 4> draw_constants = {}; // b8: read base, write base, slot vertex count, has previous
      D3D12_GPU_VIRTUAL_ADDRESS write_address = 0; // u0: the cache of this frame's parity, t0: the other
      D3D12_GPU_VIRTUAL_ADDRESS read_address = 0;
      std::array<float, object_mv_camera_floats> camera_constants = {};
      size_t object_index = 0;              // Its object in the frame's "objects"
      std::vector<uint32_t> previous_parts; // The parts its last frame's match drew (empty without one)
   };
   thread_local ObjectMotionRun object_motion_run;

   // The thread slots of the native descriptor heaps, taken at a thread's first patched draw and given back when it ends
   struct
   {
      std::shared_mutex mutex;
      std::vector<uint32_t> free_slots;
      uint32_t next_slot = 0;
   } object_motion_thread_slots;

   struct ObjectMotionThreadSlot
   {
      uint32_t index = UINT32_MAX;

      ~ObjectMotionThreadSlot()
      {
         if (index != UINT32_MAX)
         {
            const std::unique_lock lock(object_motion_thread_slots.mutex);
            object_motion_thread_slots.free_slots.push_back(index);
         }
      }
   };
   thread_local ObjectMotionThreadSlot object_motion_thread_slot;

   // The slot of this thread, UINT32_MAX if all "object_mv_max_threads" are taken
   uint32_t GetObjectMotionThreadSlot()
   {
      if (object_motion_thread_slot.index == UINT32_MAX)
      {
         const std::unique_lock lock(object_motion_thread_slots.mutex);
         if (!object_motion_thread_slots.free_slots.empty())
         {
            object_motion_thread_slot.index = object_motion_thread_slots.free_slots.back();
            object_motion_thread_slots.free_slots.pop_back();
         }
         else if (object_motion_thread_slots.next_slot < object_mv_max_threads)
         {
            object_motion_thread_slot.index = object_motion_thread_slots.next_slot++;
         }
      }
      return object_motion_thread_slot.index;
   }

   // The pipeline variants of the patched pairs (see "FindOrCreateD3D12GameVariant()"), one per pair, in the order of "MotionVectorShaders::PAIRS". Only
   // pipelines that write depth with one render target are patched: the test-only redraw of the same shaders must not be
   const D3D12GameVariant& GetObjectMotionVariant(size_t pair_index)
   {
      constexpr size_t pair_count = std::size(MotionVectorShaders::PAIRS);
      static const std::array<std::array<D3D12StageReplacement, 2>, pair_count> stages = []
      {
         std::array<std::array<D3D12StageReplacement, 2>, pair_count> result = {};
         for (size_t i = 0; i < pair_count; i++)
         {
            const MotionVectorShaders::Pair& pair = MotionVectorShaders::PAIRS[i];
            result[i] = {{{.type = reshade::api::pipeline_subobject_type::vertex_shader, .stream_type = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_VS, .shader_hash = pair.vertex_shader_hash, .original = nullptr, .code = pair.vertex_shader_code},
               {.type = reshade::api::pipeline_subobject_type::pixel_shader, .stream_type = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PS, .shader_hash = pair.pixel_shader_hash, .original = nullptr, .code = pair.pixel_shader_code}}};
         }
         return result;
      }();
      static const std::array<D3D12GameVariant, pair_count> variants = []
      {
         std::array<D3D12GameVariant, pair_count> result = {};
         for (size_t i = 0; i < pair_count; i++)
         {
            result[i] = {.id = uint32_t(i + 1),
               .replacements = stages[i],
               .appended_render_target = {.format = DXGI_FORMAT_R16G16_FLOAT, .blend = D3D12Passes::OPAQUE_BLEND},
               .accept = [](const Shader::CachedPipeline& cached_pipeline, D3D12PsoCapture::Stream& capture)
               { return cached_pipeline.render_target_count == 1 && D3D12PsoCapture::WritesDepth(capture); }};
         }
         return result;
      }();
      return variants[pair_index];
   }

   // The texture views of a patched draw, from what Core tracked at bind time. False for a kind other than a 2D texture or an array of them, or an untyped format
   bool ToRenderTargetViewDesc(const reshade::api::resource_view_desc& desc, D3D12_RENDER_TARGET_VIEW_DESC* render_target_view_desc)
   {
      if (reshade::api::format_is_typeless(desc.format) || desc.format == reshade::api::format::unknown)
      {
         return false;
      }
      *render_target_view_desc = {.Format = DXGI_FORMAT(desc.format)};
      switch (desc.type)
      {
      case reshade::api::resource_view_type::texture_2d:
         render_target_view_desc->ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
         render_target_view_desc->Texture2D = {.MipSlice = desc.texture.first_level};
         return true;
      case reshade::api::resource_view_type::texture_2d_array:
         render_target_view_desc->ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
         render_target_view_desc->Texture2DArray = {.MipSlice = desc.texture.first_level, .FirstArraySlice = desc.texture.first_layer, .ArraySize = desc.texture.layers};
         return true;
      default:
         return false;
      }
   }

   bool ToDepthStencilViewDesc(const reshade::api::resource_view_desc& desc, D3D12_DEPTH_STENCIL_VIEW_DESC* depth_stencil_view_desc)
   {
      if (reshade::api::format_is_typeless(desc.format) || desc.format == reshade::api::format::unknown)
      {
         return false;
      }
      // The flags (read only depth) aren't tracked: the patched draws write depth
      *depth_stencil_view_desc = {.Format = DXGI_FORMAT(desc.format)};
      switch (desc.type)
      {
      case reshade::api::resource_view_type::texture_2d:
         depth_stencil_view_desc->ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
         depth_stencil_view_desc->Texture2D = {.MipSlice = desc.texture.first_level};
         return true;
      case reshade::api::resource_view_type::texture_2d_array:
         depth_stencil_view_desc->ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
         depth_stencil_view_desc->Texture2DArray = {.MipSlice = desc.texture.first_level, .FirstArraySlice = desc.texture.first_layer, .ArraySize = desc.texture.layers};
         return true;
      default:
         return false;
      }
   }

   void SetProjectionDetour(void* context, const float* projection)
   {
      const uintptr_t offset = reinterpret_cast<uintptr_t>(projection) - (game_module + camera_projections_rva);
      const uint32_t slot = (offset % camera_matrix_size == 0 && offset / camera_matrix_size < 4) ? uint32_t(offset / camera_matrix_size) : 4;
      // A draw that sets its own copy of the main camera's projection (the main menu's characters' forward pass) depth tests against the
      // prepass, which had the jitter: it needs it too, or whole triangles fail the test. ponytail: a copy read while the engine writes slot 0
      // can tear, that call then goes unjittered
      const bool main_camera_copy = (slot == 4 && std::memcmp(projection, reinterpret_cast<const void*>(game_module + camera_projections_rva), camera_matrix_size) == 0);
      if (slot == 1 || (slot == 4 && !main_camera_copy))
      {
         set_projection_original(context, projection);
         return;
      }
      // The phases after the anti-aliasing draw on the anti-aliased image: their world-space draws (the HUD markers, whatever their shaders)
      // get the main camera unjittered. Only this setter call adds the jitter, so a draw setting its own projection never had it
      if (const uint32_t phase = GetFramePhase(); phase >= first_post_anti_aliasing_phase && phase <= last_post_anti_aliasing_phase)
      {
         set_projection_original(context, projection);
         return;
      }
      std::array<float, 16> jittered;
      std::copy_n(projection, 16, jittered.data());
      bool motion_active = false;
      {
         const std::unique_lock lock(camera_state.mutex);
         motion_active = camera_state.motion_active;
         if (motion_active && slot == 0)
         {
            CameraState::Capture& capture = camera_state.capture.emplace(CameraState::Capture{.projection = jittered, .jitter_pixels = camera_state.jitter_pixels, .jitter_ndc = camera_state.jitter_ndc});
            std::copy_n(reinterpret_cast<const float*>(game_module + camera_views_rva), 16, capture.view.data());
         }
         // NDC x = clip x / -view z, so _31 and _32 (view z's) move it by minus themselves
         jittered[8] -= camera_state.jitter_ndc[0];
         jittered[9] -= camera_state.jitter_ndc[1];
      }
      set_projection_original(context, motion_active ? jittered.data() : projection);
   }

   // Writes the 16 "bytes" over the 16 aligned bytes of code at "code" in one atomic operation, if they still are the 16 "expected"
   bool WriteCodeAtomically(uint8_t* code, const uint8_t* expected, const uint8_t* bytes)
   {
      DWORD old_protection = 0;
      if (!VirtualProtect(code, 16, PAGE_EXECUTE_READWRITE, &old_protection))
      {
         return false;
      }
      alignas(16) long long comparand[2];
      alignas(16) long long exchange[2];
      std::memcpy(comparand, expected, 16);
      std::memcpy(exchange, bytes, 16);
      const bool written = _InterlockedCompareExchange128(reinterpret_cast<volatile long long*>(code), exchange[1], exchange[0], comparand) != 0;
      VirtualProtect(code, 16, old_protection, &old_protection);
      FlushInstructionCache(GetCurrentProcess(), code, 16);
      return written;
   }

   // The phase 0x1a screen effects' nodes ("FUN_1402de200" registers them). Their vtables have two slots, the destructor then the draw, which records
   // the effect's draws right there on this thread (the arguments are unused). The HUD draws with the same shaders, so only this tells them apart
   struct ScreenEffectNode
   {
      uintptr_t vtable_rva;
      uintptr_t draw_rva;
   };
   constexpr std::array<ScreenEffectNode, 6> screen_effect_nodes = {{
      {.vtable_rva = 0xDEF9A8, .draw_rva = 0x2DE100}, // CDrawEffectScreenGradation
      {.vtable_rva = 0xDEF9B8, .draw_rva = 0x2DDE40}, // CDrawEffectScreenBlockNoise
      {.vtable_rva = 0xDEF9C8, .draw_rva = 0x2DDE20}, // CDrawEffectScreenAnalogNoise
      {.vtable_rva = 0xDEF9D8, .draw_rva = 0x2DE1E0}, // CDrawEffectScreenRandomNoise
      {.vtable_rva = 0xDEF9E8, .draw_rva = 0x2DE060}, // CDrawEffectScreenFade (sys_2d + sys_diffuse)
      {.vtable_rva = 0xDEF9F8, .draw_rva = 0x2DDDC0}, // CDrawEffectScreen2D
   }};
   using ScreenEffectDraw = void (*)(void* node, void* a, void* b, void* c);
   std::array<ScreenEffectDraw, screen_effect_nodes.size()> screen_effect_draw_originals = {};
   thread_local uint32_t screen_effect_depth = 0; // Inside a screen effect node's draw on this thread

   template <size_t I>
   void ScreenEffectDrawDetour(void* node, void* a, void* b, void* c)
   {
      screen_effect_depth++;
      screen_effect_draw_originals[I](node, a, b, c);
      screen_effect_depth--;
   }

   void InstallProjectionHook()
   {
      auto* const module = reinterpret_cast<uint8_t*>(GetModuleHandleW(nullptr));
      const auto* const nt_headers = reinterpret_cast<const IMAGE_NT_HEADERS64*>(module + reinterpret_cast<const IMAGE_DOS_HEADER*>(module)->e_lfanew);
      uint8_t* const set_projection = module + set_projection_rva;
      if (nt_headers->FileHeader.TimeDateStamp != game_time_date_stamp || std::memcmp(set_projection, set_projection_prologue, sizeof(set_projection_prologue)) != 0)
      {
         reshade::log::message(reshade::log::level::warning, "[DLAA] Unknown game build (its projection code isn't where expected), so DLAA is unavailable");
         return;
      }
      game_module = uintptr_t(module);
      // The TLS directory's index slot is a VA of the loaded image, the loader numbered it already
      if (const DWORD tls_directory_rva = nt_headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_TLS].VirtualAddress; tls_directory_rva != 0)
      {
         const auto* const tls_directory = reinterpret_cast<const IMAGE_TLS_DIRECTORY64*>(module + tls_directory_rva);
         exe_tls_index = *reinterpret_cast<const DWORD*>(tls_directory->AddressOfIndex);
      }

      // The screen effect nodes' draws, swapped in their vtables (one aligned pointer write each, safe while another thread calls them)
      constexpr std::array<ScreenEffectDraw, screen_effect_nodes.size()> screen_effect_detours = {&ScreenEffectDrawDetour<0>, &ScreenEffectDrawDetour<1>, &ScreenEffectDrawDetour<2>, &ScreenEffectDrawDetour<3>, &ScreenEffectDrawDetour<4>, &ScreenEffectDrawDetour<5>};
      bool screen_effects_hooked = std::ranges::all_of(screen_effect_nodes, [module](const ScreenEffectNode& node)
         { return *reinterpret_cast<const uintptr_t*>(module + node.vtable_rva + sizeof(void*)) == uintptr_t(module + node.draw_rva); });
      for (size_t i = 0; screen_effects_hooked && i < screen_effect_nodes.size(); i++)
      {
         auto* const slot = reinterpret_cast<void**>(module + screen_effect_nodes[i].vtable_rva + sizeof(void*));
         DWORD old_protection = 0;
         screen_effects_hooked = VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &old_protection) != FALSE;
         if (screen_effects_hooked)
         {
            screen_effect_draw_originals[i] = reinterpret_cast<ScreenEffectDraw>(*slot);
            InterlockedExchangePointer(slot, reinterpret_cast<void*>(screen_effect_detours[i]));
            VirtualProtect(slot, sizeof(void*), old_protection, &old_protection);
         }
      }
      if (!screen_effects_hooked)
      {
         reshade::log::message(reshade::log::level::warning, "[ScreenEffect] The screen effects' vtables aren't as expected: Hide Gameplay UI hides them too and they follow the UI brightness");
      }

      // The relay (at 0) and the stub (at 16), within a rel32 jump of the exe
      uint8_t* const code = static_cast<uint8_t*>(System::VirtualAllocNear(set_projection, 64, PAGE_READWRITE));
      if (!code)
      {
         reshade::log::message(reshade::log::level::warning, "[DLAA] No memory near the game's code for the projection hook, so DLAA is unavailable");
         return;
      }
      const void* const detour = reinterpret_cast<const void*>(&SetProjectionDetour);
      const uint8_t* const resume = set_projection + set_projection_stub_bytes;
      std::memcpy(code, jmp_absolute, sizeof(jmp_absolute));
      std::memcpy(code + sizeof(jmp_absolute), &detour, sizeof(detour));
      uint8_t* const stub = code + 16;
      std::memcpy(stub, set_projection_prologue, set_projection_stub_bytes);
      std::memcpy(stub + set_projection_stub_bytes, jmp_absolute, sizeof(jmp_absolute));
      std::memcpy(stub + set_projection_stub_bytes + sizeof(jmp_absolute), &resume, sizeof(resume));
      DWORD old_protection = 0;
      VirtualProtect(code, 64, PAGE_EXECUTE_READ, &old_protection);
      FlushInstructionCache(GetCurrentProcess(), code, 64);
      // Before the hook runs
      set_projection_original = reinterpret_cast<SetProjection>(stub);

      // jmp rel32 to the relay, then nops to the end of the "mov rcx" (never run: no thread can be inside it)
      uint8_t patched[16];
      std::copy_n(set_projection_prologue, 16, patched);
      const int32_t relay_offset = int32_t(intptr_t(code) - intptr_t(set_projection + set_projection_patch_offset + 5));
      patched[set_projection_patch_offset] = 0xE9;
      std::memcpy(patched + set_projection_patch_offset + 1, &relay_offset, sizeof(relay_offset));
      patched[set_projection_patch_offset + 5] = 0x90;
      patched[set_projection_patch_offset + 6] = 0x90;
      if (!WriteCodeAtomically(set_projection, set_projection_prologue, patched))
      {
         set_projection_original = nullptr;
         reshade::log::message(reshade::log::level::warning, "[DLAA] Couldn't write the projection hook, so DLAA is unavailable");
      }
   }

   // The constant mirror of the draw being recorded on this thread (see "render_globals_rva"). The engine flushes it right before
   // "DrawIndexedInstanced()" on the thread that replays the draw, so from ReShade's draw event it holds this draw's constants. Null without
   // that: unknown build, a thread the engine hasn't numbered (its allocator is never called from here), or no context. Plain reads: no
   // lock, no allocation.
   const uint8_t* GetConstantMirror()
   {
      const uint8_t* const tls_block = GetExeTlsBlock();
      if (!tls_block)
      {
         return nullptr;
      }
      const uint32_t thread_number = *reinterpret_cast<const uint32_t*>(tls_block + tls_thread_number_offset);
      const uint8_t* const render_globals = *reinterpret_cast<const uint8_t* const*>(game_module + render_globals_rva);
      if (thread_number == 0 || !render_globals)
      {
         return nullptr;
      }
      const uint8_t* context = *reinterpret_cast<const uint8_t* const*>(render_globals + thread_contexts_offset + thread_number * sizeof(void*));
      if (!context)
      {
         context = *reinterpret_cast<const uint8_t* const*>(render_globals + fallback_context_offset);
      }
      return (context ? *reinterpret_cast<const uint8_t* const*>(context + context_mirror_offset) : nullptr);
   }

   void OnBarrier(reshade::api::command_list* cmd_list, uint32_t count, const reshade::api::resource* resources, const reshade::api::resource_usage* old_states, const reshade::api::resource_usage* new_states)
   {
      if (scene_writer.resource == 0)
      {
         return;
      }
      for (uint32_t i = 0; i < count; i++)
      {
         if (resources[i].handle == scene_writer.resource)
         {
            scene_writer.state = new_states[i];
         }
      }
   }
} // namespace

struct YakuzaKiwamiDeviceData final : public GameDeviceData
{
   // The SMAA predication pass ("Luma_YKR_SMAAPredication") and its R8 target, at the depth's size (made again with it). Its mutex also
   // guards the target, which is in a shader resource state between draws
   DeviceData::GamePassD3D12 predication_pass;
   reshade::api::resource predication = {};
   uint64_t predication_present = UINT64_MAX; // The present count of its last draw: it counts in that frame and the next (see "IsThisOrLastFrame()")
   // Recorded on other threads than the post chain (see "shader_hashes_depth_prepass")
   std::atomic<uint64_t> scene_depth = 0;
   std::atomic<reshade::api::format> scene_depth_format = reshade::api::format::unknown;
   std::atomic<uint64_t> fxaa_input = 0;
   std::atomic<reshade::api::format> fxaa_input_format = reshade::api::format::unknown;
   // The view normals the depth prepass draws write (their rt0), XeGTAO's normals
   std::atomic<uint64_t> scene_normals = 0;
   std::atomic<reshade::api::format> scene_normals_format = reshade::api::format::unknown;

   // XeGTAO (see "DrawXeGTAO()"), at the depth's size: its inputs, copies of the depth (R32) and normals (RGBA8) the copy pass draws (it also draws the
   // apply), the view depth mips, the AO + edges ping-pong and the final AO the compute pass writes. "gtao_pass.mutex" guards them; they rest as shader
   // resources
   DeviceData::GamePassD3D12 gtao_copy_pass;
   DeviceData::GamePassD3D12 gtao_pass;
   reshade::api::resource gtao_depth = {};
   reshade::api::resource gtao_normals = {};
   reshade::api::resource gtao_depth_mips = {};
   std::array<reshade::api::resource, 2> gtao_working = {};
   reshade::api::resource gtao_final = {};
   std::atomic<uint64_t> gtao_present = UINT64_MAX; // The present count of XeGTAO's last draw (its textures go when it's idle)
   // The command lists whose fx_ssao_test XeGTAO replaced: the rest of the chain on each (filters skipped, upsampling drawing XeGTAO's) follows,
   // and the upsampling takes it out. Not the present count, which the game's worker threads can see change within one frame's recording: a chain
   // that ran the game's filters and upsampling after a replaced test would read its stale 1080p target.
   std::shared_mutex gtao_command_lists_mutex;
   std::vector<reshade::api::command_list*> gtao_command_lists;

   // Luma Bloom (see "DrawLumaBloom()"): glow_pass0's level 0 (its rt0), the sigma of glow_pass1's kernel and glow_pass2's target (the glow composite's
   // source, recorded on another thread than the composite), recorded at their draws; the folded level 0 (RGBA16F, "bloom_fold_pass.mutex" guards
   // it, at rest as a shader resource)
   std::atomic<uint64_t> glow_level0 = 0;
   std::atomic<reshade::api::format> glow_level0_format = reshade::api::format::unknown;
   std::atomic<float> glow_sigma = 1.7f;
   std::atomic<uint64_t> glow_target = 0;
   std::atomic<reshade::api::format> glow_target_format = reshade::api::format::unknown;
   DeviceData::GamePassD3D12 bloom_fold_pass;
   DeviceData::GamePassD3D12 bloom_composite_pass;
   DeviceData::GamePassD3D12 bloom_apply_pass;
   reshade::api::resource bloom_input = {};
   std::atomic<uint64_t> bloom_drawn_present = UINT64_MAX; // glow_pass1 is skipped the frame after it too

   // DLAA (see "DrawDLAA()"). Each pass' mutex also guards its targets, kept in "state" between draws: the copy pass the depth copy (R32,
   // a shader resource; it also stands in for FXAA), the motion vector pass the motion vectors (RG16F, a shader resource), the color pass
   // the decoded scene (RGBA16F, a shader resource) and DLSS' output (RGBA16F, unordered access)
   std::atomic<bool> dlaa_active = false; // Latched at present
   std::atomic<bool> t2x_active = false;  // SMAA T2x, latched at present
   DeviceData::GamePassD3D12 copy_pass;
   DeviceData::GamePassD3D12 motion_vector_pass;
   DeviceData::GamePassD3D12 color_pass;
   reshade::api::resource depth_copy = {};
   uint64_t depth_copy_present = UINT64_MAX; // The present count of its last copy: it counts in that frame and the next (see "DrawDLAA()")
   reshade::api::resource motion_vectors = {};
   reshade::api::resource decoded_scene = {};
   reshade::api::resource dlaa_output = {};
   bool dlaa_output_recreated = false;    // Until a DLSS draw into it succeeds
   std::atomic<uint32_t> depth_width = 0; // For the jitter in NDC
   std::atomic<uint32_t> depth_height = 0;
   std::atomic<uint64_t> dlaa_claimed_present = UINT64_MAX; // DLAA draws at the first ccr draw of a frame
   std::atomic<uint64_t> dlaa_drawn_present = UINT64_MAX;   // FXAA and SMAA step aside in that frame
   // The present count of the last motion vector draw: it counts in that frame and the next, as "depth_copy_present" does (see "DrawDLAA()")
   std::atomic<uint64_t> motion_vectors_present = UINT64_MAX;

   // SMAA T2x (see "OnDrawOrDispatch()"): the neighborhood blends of this frame and the last (RGBA16F, linear, the velocity length in alpha), which
   // alternate by the present count, then the resolve's output when RCAS sharpens it; "t2x_pass.mutex" guards them, they rest as shader resources
   DeviceData::GamePassD3D12 t2x_pass;
   std::array<reshade::api::resource, 3> t2x_frames = {};
   std::atomic<uint64_t> t2x_claimed_present = UINT64_MAX; // T2x draws at the first FXAA draw of a frame
   std::atomic<uint64_t> t2x_drawn_present = UINT64_MAX;   // The jitter follows it, and its history is valid the frame after
   // The main camera at the last DLAA draw, column vectors (only "DrawDLAA()" touches it, once a frame)
   Math::Matrix44D previous_view_projection;
   uint64_t previous_view_projection_present = UINT64_MAX;

   // The target of the object motion vectors (RG16F), cleared to the marker at the end of each DLAA draw for the next frame, resting as a render target.
   // "motion_vector_pass.mutex" guards the texture; the patched draws read its mirrors without a lock. "object_motion_vectors_ready_present" is the frame
   // whose patched draws may write it (the one after its clear): also the frame DLAA's motion vector pass reads it in
   reshade::api::resource object_motion_vectors = {};
   std::atomic<uint64_t> object_motion_vectors_handle = 0;
   std::atomic<uint64_t> object_motion_vectors_size = 0; // Width << 32 | height
   std::atomic<uint64_t> object_motion_vectors_ready_present = UINT64_MAX;

   // The matcher of the patched draws' objects between frames, the position caches and the native descriptors they bind (see "DrawObjectMotionVectors()")
   struct ObjectMotionState
   {
      std::shared_mutex mutex;         // Guards everything below, but the descriptor heaps once made
      uint64_t stamp = UINT64_MAX;     // The present count of "objects"
      bool has_previous_frame = false; // "previous_objects" and "camera" are those of the frame before
      std::vector<ObjectMotionObject> previous_objects;
      std::vector<ObjectMotionObject> objects;
      uint64_t next_byte = object_mv_max_vertex_stride; // Where the next slot may start in this frame's cache, past the trash slot
      ObjectMotionCamera camera;                        // Of "stamp"
      std::array<float, object_mv_camera_floats> camera_constants = {};
      // Two raw position caches (default heap, created in the common state and never transitioned: D3D12 promotes a buffer to unordered access or a
      // non pixel shader resource at its first use in an ExecuteCommandLists() call, and decays it again at the end), one written and one read by
      // each frame, swapped by the parity of the present count
      std::array<reshade::api::resource, 2> caches = {};
      std::array<D3D12_GPU_VIRTUAL_ADDRESS, 2> cache_addresses = {};
      bool caches_failed = false;
      // Non shader visible heaps of native views, written for each patched draw because the game's CPU descriptors are its own: two render target
      // views and a depth stencil view per thread slot. Read without the lock once made (never replaced)
      com_ptr<ID3D12DescriptorHeap> render_target_heap;
      com_ptr<ID3D12DescriptorHeap> depth_stencil_heap;
      D3D12_CPU_DESCRIPTOR_HANDLE render_target_heap_start = {};
      D3D12_CPU_DESCRIPTOR_HANDLE depth_stencil_heap_start = {};
      UINT render_target_increment = 0;
      UINT depth_stencil_increment = 0;
      bool heaps_failed = false;

      // The first patched draw of frame "new_stamp": flips the objects and the camera, builds b9. The history is kept only if the frame before was
      // "new_stamp" - 1 and the caches are the ones it wrote. With "mutex" held. False if there are no caches or heaps
      bool BeginFrame(reshade::api::device* device, uint64_t new_stamp)
      {
         bool fresh_caches = false;
         if (caches[0].handle == 0 && !caches_failed)
         {
            fresh_caches = true;
            const reshade::api::resource_desc cache_desc(object_mv_cache_bytes, reshade::api::memory_heap::gpu_only, reshade::api::resource_usage::shader_resource | reshade::api::resource_usage::unordered_access);
            // "general" is the common state
            for (size_t i = 0; i < caches.size() && !caches_failed; i++)
            {
               caches_failed = !device->create_resource(cache_desc, nullptr, reshade::api::resource_usage::general, &caches[i]);
               if (!caches_failed)
               {
                  cache_addresses[i] = reinterpret_cast<ID3D12Resource*>(caches[i].handle)->GetGPUVirtualAddress();
                  reinterpret_cast<ID3D12Resource*>(caches[i].handle)->SetName(i == 0 ? L"Luma YKR Position Cache 0" : L"Luma YKR Position Cache 1");
               }
            }
            if (caches_failed)
            {
               // What was made is destroyed with the device
               caches = {};
               reshade::log::message(reshade::log::level::error, "[Object MV] ReShade refused the position caches, so the object motion vectors are off");
            }
         }
         if (caches_failed)
         {
            return false;
         }
         if (!render_target_heap && !heaps_failed)
         {
            auto* const native_device = reinterpret_cast<ID3D12Device*>(device->get_native());
            const D3D12_DESCRIPTOR_HEAP_DESC render_target_heap_desc = {.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV, .NumDescriptors = 2 * object_mv_max_threads};
            const D3D12_DESCRIPTOR_HEAP_DESC depth_stencil_heap_desc = {.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV, .NumDescriptors = object_mv_max_threads};
            heaps_failed = FAILED(native_device->CreateDescriptorHeap(&render_target_heap_desc, IID_PPV_ARGS(&render_target_heap))) || FAILED(native_device->CreateDescriptorHeap(&depth_stencil_heap_desc, IID_PPV_ARGS(&depth_stencil_heap)));
            if (heaps_failed)
            {
               render_target_heap = nullptr;
               depth_stencil_heap = nullptr;
               reshade::log::message(reshade::log::level::error, "[Object MV] The native descriptor heaps couldn't be made, so the object motion vectors are off");
            }
            else
            {
               render_target_heap_start = render_target_heap->GetCPUDescriptorHandleForHeapStart();
               depth_stencil_heap_start = depth_stencil_heap->GetCPUDescriptorHandleForHeapStart();
               render_target_increment = native_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
               depth_stencil_increment = native_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
            }
         }
         if (heaps_failed)
         {
            return false;
         }

         ObjectMotionCamera current_camera;
         const uint8_t* const mirror = GetConstantMirror();
         if (!mirror)
         {
            return false;
         }
         std::memcpy(current_camera.cb12.data(), mirror + mirror_data_offset + cb12_base_offset, cb12_size);
         {
            const std::shared_lock lock(camera_state.mutex);
            current_camera.jitter_ndc = (camera_state.motion_active ? camera_state.jitter_ndc : std::array<float, 2>{});
         }

         has_previous_frame = !fresh_caches && stamp != UINT64_MAX && stamp + 1 == new_stamp;
         stamp = new_stamp;
         if (has_previous_frame)
         {
            previous_objects.swap(objects);
         }
         else
         {
            previous_objects.clear();
         }
         objects.clear();
         next_byte = object_mv_max_vertex_stride;

         // Last frame's camera, as its draws saw it before the projection hook: with its jitter taken out. Without a last frame, this one (no motion)
         const ObjectMotionCamera& previous_camera = (has_previous_frame ? camera : current_camera);
         std::copy_n(previous_camera.cb12.data(), cb12_floats, camera_constants.data());
         camera_constants[cb12_projection_31] += previous_camera.jitter_ndc[0];
         camera_constants[cb12_projection_32] += previous_camera.jitter_ndc[1];
         // VP's rows 0 and 1 got the jitter through P's _31/_32 times V's row 2
         for (size_t i = 0; i < 4; i++)
         {
            camera_constants[cb12_view_projection + i] += previous_camera.jitter_ndc[0] * previous_camera.cb12[cb12_view + 2 * 4 + i];
            camera_constants[cb12_view_projection + 4 + i] += previous_camera.jitter_ndc[1] * previous_camera.cb12[cb12_view + 2 * 4 + i];
         }
         // Rows 31 and 32: this frame's jitter and the gate, the move of c_location since last frame
         camera_constants[cb12_floats + 0] = current_camera.jitter_ndc[0];
         camera_constants[cb12_floats + 1] = current_camera.jitter_ndc[1];
         camera_constants[cb12_floats + 2] = g_object_mv_max_distance * g_object_mv_max_distance;
         camera_constants[cb12_floats + 3] = 0.f;
         for (size_t i = 0; i < 3; i++)
         {
            camera_constants[cb12_floats + 4 + i] = current_camera.cb12[cb12_location + i] - previous_camera.cb12[cb12_location + i];
         }
         camera_constants[cb12_floats + 7] = 0.f;
         camera = current_camera;
         return true;
      }

      // The first part of an object this thread records on this list in this frame: finds its slot in the caches (of "vertex_stride" bytes per
      // vertex, its shader's) and the one it had last frame. With "run" set up for its draws. False if the frame couldn't begin
      bool StartRun(ObjectMotionRun* run, reshade::api::device* device, uint64_t new_stamp, const void* cmd_list, const ObjectMotionVertexBuffer& vertex_buffer, uint32_t first_index, const std::array<float, 3>& anchor, uint32_t vertex_count, uint32_t vertex_stride)
      {
         const std::unique_lock lock(mutex);
         if (stamp != new_stamp && !BeginFrame(device, new_stamp))
         {
            return false;
         }

         // The nearest object of last frame with the same asset and first part, if it is near enough
         ObjectMotionObject* match = nullptr;
         const float match_distance_squared = g_object_mv_match_distance * g_object_mv_match_distance;
         float nearest_distance_squared = match_distance_squared;
         uint32_t candidates_in_range = 0; // More than one: the nearest wins a tie that could swap identical objects (DLSS best practices MV-9)
         for (ObjectMotionObject& candidate : previous_objects)
         {
            if (candidate.claimed || candidate.first_index != first_index || candidate.vertex_buffer != vertex_buffer)
            {
               continue;
            }
            const float distance_squared = (candidate.anchor[0] - anchor[0]) * (candidate.anchor[0] - anchor[0]) + (candidate.anchor[1] - anchor[1]) * (candidate.anchor[1] - anchor[1]) + (candidate.anchor[2] - anchor[2]) * (candidate.anchor[2] - anchor[2]);
            if (distance_squared <= match_distance_squared)
            {
               candidates_in_range++;
            }
            if (distance_squared <= nearest_distance_squared)
            {
               nearest_distance_squared = distance_squared;
               match = &candidate;
            }
         }
         if (candidates_in_range > 1)
         {
            CountObjectMotion(MV_TIEBREAK_COLLISIONS);
         }

         // Out of slots: all trash, so the object keeps the camera motion
         const uint64_t slot_base = (next_byte + vertex_stride - 1) / vertex_stride;
         const bool has_slot = vertex_count != 0 && (slot_base + vertex_count) * vertex_stride <= object_mv_cache_bytes;
         ObjectMotionObject object = {.vertex_buffer = vertex_buffer, .first_index = first_index, .anchor = anchor, .write_base = (has_slot ? uint32_t(slot_base) : 0), .vertex_count = (has_slot ? vertex_count : 0)};
         if (has_slot)
         {
            next_byte = (slot_base + vertex_count) * vertex_stride;
         }
         else
         {
            CountObjectMotion(MV_OVERFLOW);
         }
         // A mesh of another size is not the same object: its vertices are not the ones last frame's slot holds
         const bool has_previous = match && has_slot && match->vertex_count == vertex_count;
         CountObjectMotion(match ? MV_MATCHED : MV_NEW);
         run->cmd_list = cmd_list;
         run->stamp = new_stamp;
         run->vertex_buffer = vertex_buffer;
         run->last_first_index = first_index;
         run->draw_constants = {(has_previous ? match->write_base : 0), object.write_base, object.vertex_count, (has_previous ? 1u : 0u)};
         run->camera_constants = camera_constants;
         run->write_address = cache_addresses[new_stamp & 1];
         run->read_address = cache_addresses[(new_stamp & 1) ^ 1];
         if (match)
         {
            match->claimed = true;
         }
         run->previous_parts.clear();
         if (has_previous)
         {
            run->previous_parts.swap(match->parts);
         }
         run->object_index = objects.size();
         object.parts = {first_index};
         objects.push_back(std::move(object));
         return true;
      }

      // A further part of the object "run" draws, recorded for next frame's draws of it
      void AddPart(const ObjectMotionRun& run, uint32_t first_index)
      {
         const std::unique_lock lock(mutex);
         if (stamp == run.stamp && run.object_index < objects.size())
         {
            objects[run.object_index].parts.push_back(first_index);
         }
      }

      // A moved part of the rigid world in frame "new_stamp": b9 with its last frame's matrix, and the caches, which its shader doesn't read but its
      // root descriptors must point at. False if the frame couldn't begin
      bool StartRigidDraw(ObjectMotionRun* run, reshade::api::device* device, uint64_t new_stamp, const std::array<float, world_matrix_floats>& previous_world)
      {
         const std::unique_lock lock(mutex);
         if (stamp != new_stamp && !BeginFrame(device, new_stamp))
         {
            return false;
         }
         run->draw_constants = {};
         run->camera_constants = camera_constants;
         std::copy_n(previous_world.data(), world_matrix_floats, run->camera_constants.data() + object_mv_previous_world);
         run->write_address = cache_addresses[new_stamp & 1];
         run->read_address = cache_addresses[(new_stamp & 1) ^ 1];
         return true;
      }
   } object_motion;

   // The rigid world's draws of this frame and the last one by part, with their world matrices (see "Match()")
   struct
   {
      struct Draw
      {
         std::array<float, world_matrix_floats> world = {};
         bool claimed = false;
      };
      std::shared_mutex mutex;     // Guards everything below
      uint64_t stamp = UINT64_MAX; // The present count of "current"
      std::unordered_map<RigidPartKey, std::vector<Draw>, RigidPartKeyHash> previous;
      std::unordered_map<RigidPartKey, std::vector<Draw>, RigidPartKeyHash> current;

      // A draw of part "key" with "world" in frame "present_count", against last frame's draws of the part: one with the same matrix (static), else the
      // nearest within "g_object_mv_rigid_match_distance" (moved: its matrix goes to "*previous_world"), else none (new). An unchanged draw is looked for
      // first, so a moving copy of an asset doesn't take a static copy's draw
      RigidMotion Match(uint64_t present_count, const RigidPartKey& key, const std::array<float, world_matrix_floats>& world, std::array<float, world_matrix_floats>* previous_world)
      {
         const std::unique_lock lock(mutex);
         if (stamp != present_count)
         {
            // The history is only the frame right before
            if (stamp != UINT64_MAX && stamp + 1 == present_count)
            {
               previous.swap(current);
            }
            else
            {
               previous.clear();
            }
            current.clear();
            stamp = present_count;
         }

         RigidMotion motion = RigidMotion::NEW;
         Draw* matched = nullptr;
         if (const auto it = previous.find(key); it != previous.end())
         {
            float nearest_distance_squared = g_object_mv_rigid_match_distance * g_object_mv_rigid_match_distance;
            for (Draw& candidate : it->second)
            {
               if (candidate.claimed)
               {
                  continue;
               }
               if (candidate.world == world)
               {
                  motion = RigidMotion::STATIC;
                  matched = &candidate;
                  break;
               }
               const float dx = candidate.world[3] - world[3];
               const float dy = candidate.world[7] - world[7];
               const float dz = candidate.world[11] - world[11];
               const float distance_squared = dx * dx + dy * dy + dz * dz;
               if (distance_squared <= nearest_distance_squared)
               {
                  nearest_distance_squared = distance_squared;
                  motion = RigidMotion::MOVING;
                  matched = &candidate;
               }
            }
         }
         if (matched)
         {
            matched->claimed = true;
            if (motion == RigidMotion::MOVING)
            {
               *previous_world = matched->world;
            }
         }
         current[key].push_back({.world = world});
         return motion;
      }
   } rigid_draws;

   // "*target" (a texture of "pass", in "state" between draws) at "width" x "height", made again (the old one retired) at another size. With
   // "pass->mutex" held. "*created": whether it was made now. "levels": its mips
   static bool EnsurePassTarget(DeviceData::GamePassD3D12* pass, reshade::api::resource* target, reshade::api::device* device, uint64_t present_count, uint32_t width, uint32_t height, reshade::api::format format, reshade::api::resource_usage usage, reshade::api::resource_usage state, const wchar_t* name, bool* created = nullptr, uint16_t levels = 1)
   {
      if (target->handle != 0)
      {
         const reshade::api::resource_desc desc = device->get_resource_desc(*target);
         if (desc.texture.width == width && desc.texture.height == height)
         {
            return true;
         }
         RetireD3D12GamePassResource(pass, present_count, target);
      }
      const reshade::api::resource_desc desc(width, height, 1, levels, format, 1, reshade::api::memory_heap::gpu_only, usage);
      // Destroyed with the device otherwise
      const bool made = device->create_resource(desc, nullptr, state, target);
      if (made)
      {
         reinterpret_cast<ID3D12Resource*>(target->handle)->SetName(name);
      }
      if (created)
      {
         *created = made;
      }
      return made;
   }

   // The predication target at "width" x "height". With "predication_pass.mutex" held
   bool EnsurePredicationTarget(reshade::api::device* device, uint64_t present_count, uint32_t width, uint32_t height)
   {
      return EnsurePassTarget(&predication_pass, &predication, device, present_count, width, height, reshade::api::format::r8_unorm, reshade::api::resource_usage::render_target | reshade::api::resource_usage::shader_resource, reshade::api::resource_usage::shader_resource, L"Luma YKR SMAA Predication");
   }
};

class YakuzaKiwami final : public Game
{
public:
   void OnInit(bool async) override
   {
      std::vector<ShaderDefineData> game_shader_defines_data = {
         {"TONEMAP_TYPE", '1', true, false, "0 - Vanilla SDR\n1 - Luma HDR (Vanilla+)", 1},
      };
      shader_defines_data.append_range(game_shader_defines_data);
      assert(shader_defines_data.size() < MAX_SHADER_DEFINES);

      // Gamma-space post processing (the vanilla chain and the HUD are 2.2 gamma, UNORM, no sRGB views), 1.0 = paper white.
      GetShaderDefineData(POST_PROCESS_SPACE_TYPE_HASH).SetDefaultValue('0');
      GetShaderDefineData(EARLY_DISPLAY_ENCODING_HASH).SetDefaultValue('0');
      GetShaderDefineData(VANILLA_ENCODING_TYPE_HASH).SetDefaultValue('1');
      GetShaderDefineData(GAMMA_CORRECTION_TYPE_HASH).SetDefaultValue('1');
      // The HUD is drawn straight onto the post-FXAA scene: the grade pre-scales the scene by game/UI paper white.
      GetShaderDefineData(UI_DRAW_TYPE_HASH).SetDefaultValue('2');

      // The game binds b1-b3, b5-b10, b12 and b13. Luma's cbuffers live in their own register space on D3D12 (SM 6), so these
      // registers can't collide with the game's; b11 and b13 keep them out of the way of SM 5.0 tools too.
      luma_settings_cbuffer_index = 13;
      luma_data_cbuffer_index = 11;

      // Root parameters of the object motion vectors (design B, "Object motion vectors" in NOTES.md), appended after Luma's CBVs to every
      // root signature: the per-frame camera CBV, the per-draw constants, the current position cache (raw UAV) and the previous one (raw SRV).
      // "DrawObjectMotionVectors()" binds them on its draws. Core reads this once at device init, which comes after OnInit.
      d3d12_game_root_parameters = {
         {.type = D3D12_ROOT_PARAMETER_TYPE_CBV, .shader_register = 9},
         {.type = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS, .shader_register = 8, .constant_count = 4},
         {.type = D3D12_ROOT_PARAMETER_TYPE_UAV, .shader_register = 0},
         {.type = D3D12_ROOT_PARAMETER_TYPE_SRV, .shader_register = 0},
      };

      default_luma_global_game_settings.VideoAutoHDREnable = cb_luma_global_settings.GameSettings.VideoAutoHDREnable = 1.f;
      default_luma_global_game_settings.VideoAutoHDRBoost = cb_luma_global_settings.GameSettings.VideoAutoHDRBoost = 0.5f; // peak ~165 nits
      default_luma_global_game_settings.Dithering = cb_luma_global_settings.GameSettings.Dithering = 1.f;
      default_luma_global_game_settings.Exposure = cb_luma_global_settings.GameSettings.Exposure = 1.f;
      default_luma_global_game_settings.Contrast = cb_luma_global_settings.GameSettings.Contrast = 1.f;
      default_luma_global_game_settings.Saturation = cb_luma_global_settings.GameSettings.Saturation = 1.f;
      default_luma_global_game_settings.HighlightDechroma = cb_luma_global_settings.GameSettings.HighlightDechroma = 0.f;

      native_shaders_definitions.emplace(CompileTimeStringHash("YKR SMAA Predication PS"), ShaderDefinition{.file_name = "Luma_YKR_SMAAPredication", .type = reshade::api::pipeline_subobject_type::pixel_shader});
      native_shaders_definitions.emplace(CompileTimeStringHash("YKR SMAA T2x Blending Weight Calculation PS"), ShaderDefinition{.file_name = "Luma_SMAA_impl", .type = reshade::api::pipeline_subobject_type::pixel_shader, .function_name = "smaa_blending_weight_calculation_ps", .defines_data = {{"SMAA_T2X", "1"}}});
      native_shaders_definitions.emplace(CompileTimeStringHash("YKR SMAA T2x Neighborhood Blending PS"), ShaderDefinition{.file_name = "Luma_SMAA_impl", .type = reshade::api::pipeline_subobject_type::pixel_shader, .function_name = "smaa_neighborhood_blending_ps", .defines_data = {{"SMAA_T2X", "1"}}});
      native_shaders_definitions.emplace(CompileTimeStringHash("YKR SMAA T2x Resolve PS"), ShaderDefinition{.file_name = "Luma_YKR_SMAAT2xResolve", .type = reshade::api::pipeline_subobject_type::pixel_shader});
      native_shaders_definitions.emplace(CompileTimeStringHash("YKR DLAA Copy PS"), ShaderDefinition{.file_name = "Luma_YKR_DLAA", .type = reshade::api::pipeline_subobject_type::pixel_shader, .function_name = "copy_ps"});
      native_shaders_definitions.emplace(CompileTimeStringHash("YKR DLAA Motion Vectors PS"), ShaderDefinition{.file_name = "Luma_YKR_DLAA", .type = reshade::api::pipeline_subobject_type::pixel_shader, .function_name = "motion_vectors_ps"});
      native_shaders_definitions.emplace(CompileTimeStringHash("YKR DLAA Decode PS"), ShaderDefinition{.file_name = "Luma_YKR_DLAA", .type = reshade::api::pipeline_subobject_type::pixel_shader, .function_name = "decode_ps"});
      native_shaders_definitions.emplace(CompileTimeStringHash("YKR DLAA Encode PS"), ShaderDefinition{.file_name = "Luma_YKR_DLAA", .type = reshade::api::pipeline_subobject_type::pixel_shader, .function_name = "encode_ps"});
      native_shaders_definitions.emplace(CompileTimeStringHash("YKR XeGTAO Prefilter Depths CS"), ShaderDefinition{.file_name = "Luma_YKR_XeGTAO", .type = reshade::api::pipeline_subobject_type::compute_shader, .function_name = "prefilter_depths16x16_cs"});
      native_shaders_definitions.emplace(CompileTimeStringHash("YKR XeGTAO Main Pass CS"), ShaderDefinition{.file_name = "Luma_YKR_XeGTAO", .type = reshade::api::pipeline_subobject_type::compute_shader, .function_name = "main_pass_cs"});
      native_shaders_definitions.emplace(CompileTimeStringHash("YKR GT-VBAO Main Pass CS"), ShaderDefinition{.file_name = "Luma_YKR_XeGTAO", .type = reshade::api::pipeline_subobject_type::compute_shader, .function_name = "gtvbao_main_pass_cs"});
      native_shaders_definitions.emplace(CompileTimeStringHash("YKR GT-VBAO Temporal Main Pass CS"), ShaderDefinition{.file_name = "Luma_YKR_XeGTAO", .type = reshade::api::pipeline_subobject_type::compute_shader, .function_name = "gtvbao_main_pass_cs", .defines_data = {{"GTVBAO_SLICE_COUNT", "1.0"}}});
      native_shaders_definitions.emplace(CompileTimeStringHash("YKR XeGTAO Denoise Pass 1 CS"), ShaderDefinition{.file_name = "Luma_YKR_XeGTAO", .type = reshade::api::pipeline_subobject_type::compute_shader, .function_name = "denoise_pass_cs", .defines_data = {{"XE_GTAO_FINAL_APPLY", "0"}}});
      native_shaders_definitions.emplace(CompileTimeStringHash("YKR XeGTAO Denoise Pass 2 CS"), ShaderDefinition{.file_name = "Luma_YKR_XeGTAO", .type = reshade::api::pipeline_subobject_type::compute_shader, .function_name = "denoise_pass_cs", .defines_data = {{"XE_GTAO_FINAL_APPLY", "1"}}});
      native_shaders_definitions.emplace(CompileTimeStringHash("YKR XeGTAO Apply PS"), ShaderDefinition{.file_name = "Luma_YKR_XeGTAO", .type = reshade::api::pipeline_subobject_type::pixel_shader, .function_name = "apply_ps"});
      native_shaders_definitions.emplace(CompileTimeStringHash("YKR Bloom Fold PS"), ShaderDefinition{.file_name = "Luma_YKR_Bloom", .type = reshade::api::pipeline_subobject_type::pixel_shader, .function_name = "fold_ps"});
      native_shaders_definitions.emplace(CompileTimeStringHash("YKR Bloom Composite PS"), ShaderDefinition{.file_name = "Luma_YKR_Bloom", .type = reshade::api::pipeline_subobject_type::pixel_shader, .function_name = "composite_ps"});
      native_shaders_definitions.emplace(CompileTimeStringHash("YKR Bloom Apply PS"), ShaderDefinition{.file_name = "Luma_YKR_Bloom", .type = reshade::api::pipeline_subobject_type::pixel_shader, .function_name = "apply_ps"});

      sr_game_tooltip = "DLAA only (no upscaling). Characters, hair, trees and moved world objects get their own motion; particles and other effects move with the camera's and can leave trails.\n";

      reshade::register_event<reshade::addon_event::barrier>(OnBarrier);
#if DEVELOPMENT
      Mcp::RegisterToggles({{"hide_ui", &g_hide_ui}, {"smaa_enable", &g_smaa_enable}, {"smaa_predication", &g_smaa_predication}, {"smaa_t2x", &g_smaa_t2x}, {"dlaa_mip_bias", &g_dlaa_mip_bias}, {"object_motion_vectors", &g_object_motion_vectors}, {"mv_debug_view", &g_mv_debug_view},
         {"object_mv.rigid", &g_object_mv_rigid}, {"luma_bloom_enable", &g_luma_bloom_enable}, {"gtao_enable", &g_gtao_enable}, { "gtvbao_enable",
            &g_gtvbao_enable }});
      Mcp::RegisterValues({{"object_mv.match_distance", &g_object_mv_match_distance, 0.f, 10.f}, {"object_mv.max_distance", &g_object_mv_max_distance, 0.f, 100.f}, {"object_mv.rigid_match_distance", &g_object_mv_rigid_match_distance, 0.f, 10.f},
         {"bloom_intensity", &g_bloom_intensity, 0.f, 2.f}, {"rcas_sharpness", &g_rcas_sharpness, 0.f, 1.f}, {"gtao_final_value_power", &g_gtao_final_value_power, 0.3f, 4.5f}, {"gtao_radius_override", &g_gtao_radius_override, 0.f, 5.f}});
      Mcp::RegisterInts({{"gtao_debug_view", &g_gtao_debug_view, 0, 4}, {"gtao_temporal", &g_gtao_temporal, 0, 2}});
      for (size_t i = 0; i < MV_COUNT; i++)
      {
         Mcp::RegisterCounter(object_motion_count_names[i], &object_motion_counts_frame[i]);
      }
      Mcp::RegisterTextures({
         {"smaa.predication", [](DeviceData& device_data) -> Mcp::DevTexture
            {
               auto& game_device_data = static_cast<YakuzaKiwamiDeviceData&>(*device_data.game);
               const std::shared_lock lock(game_device_data.predication_pass.mutex);
               return {reinterpret_cast<ID3D12Resource*>(game_device_data.predication.handle), D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE};
            }},
         {"dlaa.depth", [](DeviceData& device_data) -> Mcp::DevTexture
            {
               auto& game_device_data = static_cast<YakuzaKiwamiDeviceData&>(*device_data.game);
               const std::shared_lock lock(game_device_data.copy_pass.mutex);
               return {reinterpret_cast<ID3D12Resource*>(game_device_data.depth_copy.handle), D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE};
            }},
         {mv_debug_view_texture, [](DeviceData& device_data) -> Mcp::DevTexture
            {
               auto& game_device_data = static_cast<YakuzaKiwamiDeviceData&>(*device_data.game);
               const std::shared_lock lock(game_device_data.motion_vector_pass.mutex);
               return {reinterpret_cast<ID3D12Resource*>(game_device_data.motion_vectors.handle), D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE};
            }},
         // Read at present, after DLAA cleared it to the marker for the next frame: the object motion vectors of the frame are the second input of its "DLAA Motion Vectors" trace entry
         {"dlaa.object_motion_vectors", [](DeviceData& device_data) -> Mcp::DevTexture
            {
               auto& game_device_data = static_cast<YakuzaKiwamiDeviceData&>(*device_data.game);
               const std::shared_lock lock(game_device_data.motion_vector_pass.mutex);
               return {reinterpret_cast<ID3D12Resource*>(game_device_data.object_motion_vectors.handle), D3D12_RESOURCE_STATE_RENDER_TARGET};
            }},
         {"dlaa.decoded", [](DeviceData& device_data) -> Mcp::DevTexture
            {
               auto& game_device_data = static_cast<YakuzaKiwamiDeviceData&>(*device_data.game);
               const std::shared_lock lock(game_device_data.color_pass.mutex);
               return {reinterpret_cast<ID3D12Resource*>(game_device_data.decoded_scene.handle), D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE};
            }},
         {"dlaa.output", [](DeviceData& device_data) -> Mcp::DevTexture
            {
               auto& game_device_data = static_cast<YakuzaKiwamiDeviceData&>(*device_data.game);
               const std::shared_lock lock(game_device_data.color_pass.mutex);
               return {reinterpret_cast<ID3D12Resource*>(game_device_data.dlaa_output.handle), D3D12_RESOURCE_STATE_UNORDERED_ACCESS};
            }},
      });
#endif
   }

   void OnCreateDevice(NativeDevice* native_device, DeviceData& device_data) override
   {
      device_data.game = new YakuzaKiwamiDeviceData;
   }

   DrawOrDispatchOverrideType OnDrawOrDispatch(NativeDevice* native_device, NativeCommandList* native_device_context, CommandListData& cmd_list_data, DeviceData& device_data, reshade::api::shader_stage stages, const ShaderHashesList<OneShaderPerPipeline>& original_shader_hashes, bool is_custom_pass, bool& updated_cbuffers, std::function<void()>* original_draw_dispatch_func) override
   {
      auto& game_device_data = static_cast<YakuzaKiwamiDeviceData&>(*device_data.game);
      const D3D12State::CommandListState& state = cmd_list_data.d3d12_state;
      // DLAA or SMAA T2x: the motion vectors are made
      const bool motion_active = game_device_data.dlaa_active.load(std::memory_order_relaxed) || game_device_data.t2x_active.load(std::memory_order_relaxed);

      // "Hide Gameplay UI": the game's draws of the phases after FXAA are skipped, 0x19 (the world-space HUD, cutscene and menu backgrounds) and
      // 0x1a (the HUD, menus, messages), but the screen effects, the copy, the output and the videos
      if (g_hide_ui && screen_effect_depth == 0 && !original_shader_hashes.Contains(shader_hashes_hud_phase_kept))
      {
         const uint32_t hud_phase = GetFramePhase();
         const bool is_depth_2d_copy = hud_phase == depth_2d_phase && original_shader_hashes.Contains(depth_2d_copy_vertex_shader, reshade::api::shader_stage::vertex) && original_shader_hashes.Contains(depth_2d_copy_pixel_shader, reshade::api::shader_stage::pixel);
         if (hud_phase > first_post_anti_aliasing_phase && hud_phase <= last_post_anti_aliasing_phase && !is_depth_2d_copy)
         {
            return DrawOrDispatchOverrideType::Replaced;
         }
      }
      const bool is_color_correct = original_shader_hashes.Contains(shader_hashes_color_correct);
      if (is_color_correct)
      {
         // The last pass of the scene's post (DLSS best practices BK-1: Core's upscaler indicator)
         device_data.has_drawn_main_post_processing = true;
      }
      if (motion_active && is_color_correct)
      {
         DrawDLAA(cmd_list_data, device_data, &game_device_data);
      }
      // What the next draw on this list may read as the scene
      if (motion_active && !state.render_target_views.empty() && !state.render_target_descs.empty())
      {
         scene_writer = {.resource = state.render_target_views[0].resource, .desc = state.render_target_descs[0], .state = reshade::api::resource_usage::render_target, .present_count = device_data.present_count.load(std::memory_order_relaxed)};
      }

      // The anchor (the world matrix' translation: bone 0, or a rigid part's c_matW, from the constant mirror's cb10 of the draw being recorded on this
      // thread) tells the instances of an asset apart for the object motion vectors; it reads the mirror in release builds too
      std::array<float, world_matrix_floats> world = {};
      std::array<float, 3> anchor = {};
      bool anchor_read = false;
      const bool is_object_prepass = original_shader_hashes.Contains(shader_hashes_object_prepass);
      if (is_object_prepass)
      {
         if (const uint8_t* const mirror = GetConstantMirror())
         {
            std::memcpy(world.data(), mirror + mirror_data_offset + cb10_base_offset, world_matrix_size);
            anchor_read = true;
         }
         anchor = {world[3], world[7], world[11]};
      }

      // The offline patched pair of the draw, by both of its shaders: ps_depth's bytecode is also ps_zero's and the confetti shadow's PS, and
      // vs_depth_skin's the highlight mask's VS, so neither tells a prepass draw alone
      const MotionVectorShaders::Pair* motion_vector_pair = nullptr;
      if (is_object_prepass)
      {
         const auto pair = std::ranges::find_if(MotionVectorShaders::PAIRS, [&original_shader_hashes](const MotionVectorShaders::Pair& candidate)
            { return candidate.vertex_shader_hash == original_shader_hashes.vertex_shaders[0] && candidate.pixel_shader_hash == original_shader_hashes.pixel_shaders[0]; });
         motion_vector_pair = ((pair != std::end(MotionVectorShaders::PAIRS)) ? &*pair : nullptr);
      }
      const bool is_normals_prepass = original_shader_hashes.Contains(shader_hashes_depth_prepass);
      if (is_normals_prepass || motion_vector_pair)
      {
         game_device_data.scene_depth_format.store(state.depth_stencil_view.format, std::memory_order_relaxed);
         game_device_data.scene_depth.store(state.depth_stencil_view.resource, std::memory_order_relaxed);
         // Only the ps_normal family writes the view normals (fx_ssao_test's t1): the hair depth pair (ps_depth_pt, also a motion vector pair) draws
         // into another target of the same size and format, on another thread, so taking its RT0 fed XeGTAO a color buffer as normals in some frames
         if (is_normals_prepass && !state.render_target_views.empty())
         {
            game_device_data.scene_normals_format.store(state.render_target_views[0].format, std::memory_order_relaxed);
            game_device_data.scene_normals.store(state.render_target_views[0].resource, std::memory_order_relaxed);
         }
      }
      // The rigid world's parts against last frame's: a static or new one keeps the camera motion (exact for it), a moved one draws its own
      std::array<float, world_matrix_floats> previous_world = {};
      bool rigid_moved = false;
      if (motion_active && g_object_motion_vectors && g_object_mv_rigid && anchor_read && original_shader_hashes.Contains(shader_hashes_rigid_prepass) && !state.vertex_buffers.empty() && state.depth_stencil_view.resource == game_device_data.scene_depth.load(std::memory_order_relaxed))
      {
         const RigidPartKey key = {.vertex_buffer = {.buffer = state.vertex_buffers[0].buffer, .offset = state.vertex_buffers[0].offset}, .first_index = last_draw_dispatch_data.first_index};
         switch (game_device_data.rigid_draws.Match(device_data.present_count.load(std::memory_order_relaxed), key, world, &previous_world))
         {
         case RigidMotion::STATIC:
            CountObjectMotion(MV_RIGID_STATIC);
            break;
         case RigidMotion::MOVING:
            CountObjectMotion(MV_RIGID_MOVING);
            rigid_moved = true;
            break;
         case RigidMotion::NEW:
            CountObjectMotion(MV_RIGID_NEW);
            break;
         }
      }
      // Before the depth prepass return: the skinned ps_normal pair is in that list
      if (motion_active && g_object_motion_vectors && DrawObjectMotionVectors(native_device, native_device_context, cmd_list_data, device_data, &game_device_data, motion_vector_pair, anchor_read, anchor, (rigid_moved ? &previous_world : nullptr)))
      {
         return DrawOrDispatchOverrideType::Replaced;
      }
      if (is_normals_prepass || motion_vector_pair)
      {
         return DrawOrDispatchOverrideType::None;
      }

      // Barriers are invalid in a native render pass (the Luma passes refuse to draw there too)
      // Not under DLAA, where SMAA doesn't run (a frame DLAA can't draw gets SMAA without the predication)
      const bool predicate = g_smaa_enable && g_smaa_predication && !game_device_data.dlaa_active.load(std::memory_order_relaxed);
      const bool is_predication_draw = predicate && original_shader_hashes.Contains(shader_hashes_ssao);
      const bool is_depth_copy_draw = motion_active && original_shader_hashes.Contains(shader_hashes_depth_copy);
      const bool is_ssao_test = original_shader_hashes.Contains(shader_hashes_ssao_test);
      // Every test starts a chain of its own: one XeGTAO doesn't replace (off, refused) must not inherit an older replaced test's skips
      if (is_ssao_test)
      {
         const std::unique_lock lock(game_device_data.gtao_command_lists_mutex);
         std::erase(game_device_data.gtao_command_lists, cmd_list_data.cmd_list);
      }
      const bool is_gtao_draw = g_gtao_enable && is_ssao_test;
      if (!state.in_render_pass && (is_predication_draw || is_depth_copy_draw || is_gtao_draw))
      {
         const reshade::api::resource depth = {game_device_data.scene_depth.load(std::memory_order_relaxed)};
         if (depth.handle == 0)
         {
            return DrawOrDispatchOverrideType::None;
         }
         reshade::api::device* const device = cmd_list_data.cmd_list->get_device();
         const reshade::api::resource_desc depth_desc = device->get_resource_desc(depth);
         const uint32_t width = depth_desc.texture.width;
         const uint32_t height = depth_desc.texture.height;
         game_device_data.depth_width.store(width, std::memory_order_relaxed);
         game_device_data.depth_height.store(height, std::memory_order_relaxed);
         const uint64_t present_count = device_data.present_count.load(std::memory_order_relaxed);
         const reshade::api::format depth_format = reshade::api::format_to_default_typed(game_device_data.scene_depth_format.load(std::memory_order_relaxed));
         if (is_predication_draw)
         {
            std::unique_lock lock(game_device_data.predication_pass.mutex);
            if (game_device_data.EnsurePredicationTarget(device, present_count, width, height))
            {
               const reshade::api::resource target = game_device_data.predication;
               // "DrawFullscreenPassD3D12()" takes it. A target retired meanwhile lives on until the lists recorded with it ran.
               lock.unlock();
               const reshade::api::resource_view_desc target_desc(reshade::api::resource_view_type::texture_2d, reshade::api::format::r8_unorm, 0, 1, 0, 1);
               // TW2's validated tolerance (2% of view depth), standard Z
               const float params[] = {0.02f, 0.f};
               cmd_list_data.cmd_list->barrier(target, reshade::api::resource_usage::shader_resource, reshade::api::resource_usage::render_target);
               const bool drawn = DrawFullscreenPassD3D12(cmd_list_data, device_data, &game_device_data.predication_pass, CompileTimeStringHash("YKR SMAA Predication PS"), "SMAA Predication", "Luma SMAA Predication", depth, depth_format, target, target_desc, params);
               cmd_list_data.cmd_list->barrier(target, reshade::api::resource_usage::render_target, reshade::api::resource_usage::shader_resource);
               if (drawn)
               {
                  lock.lock();
                  game_device_data.predication_present = present_count;
               }
            }
         }
         if (is_depth_copy_draw)
         {
            std::unique_lock lock(game_device_data.copy_pass.mutex);
            if (game_device_data.EnsurePassTarget(&game_device_data.copy_pass, &game_device_data.depth_copy, device, present_count, width, height, reshade::api::format::r32_float, reshade::api::resource_usage::render_target | reshade::api::resource_usage::shader_resource, reshade::api::resource_usage::shader_resource, L"Luma YKR DLAA Depth"))
            {
               const reshade::api::resource target = game_device_data.depth_copy;
               lock.unlock();
               const reshade::api::resource_view_desc target_desc(reshade::api::resource_view_type::texture_2d, reshade::api::format::r32_float, 0, 1, 0, 1);
               cmd_list_data.cmd_list->barrier(target, reshade::api::resource_usage::shader_resource, reshade::api::resource_usage::render_target);
               const bool drawn = DrawFullscreenPassD3D12(cmd_list_data, device_data, &game_device_data.copy_pass, CompileTimeStringHash("YKR DLAA Copy PS"), "DLAA Depth Copy", "Luma DLAA Depth Copy", depth, depth_format, target, target_desc, {});
               cmd_list_data.cmd_list->barrier(target, reshade::api::resource_usage::render_target, reshade::api::resource_usage::shader_resource);
               if (drawn)
               {
                  lock.lock();
                  game_device_data.depth_copy_present = present_count;
               }
            }
         }
         if (is_gtao_draw)
         {
            if (DrawXeGTAO(cmd_list_data, device_data, &game_device_data, depth, depth_format, width, height))
            {
               {
                  const std::unique_lock lock(game_device_data.gtao_command_lists_mutex);
                  game_device_data.gtao_command_lists.push_back(cmd_list_data.cmd_list);
               }
               game_device_data.gtao_present.store(present_count, std::memory_order_relaxed);
               return DrawOrDispatchOverrideType::Replaced;
            }
         }
         return DrawOrDispatchOverrideType::None;
      }

      // The rest of the game's SSAO, on a list where XeGTAO replaced its test
      const bool is_ssao_filter = original_shader_hashes.Contains(shader_hashes_ssao_filters);
      const bool is_ssao_upsampling = original_shader_hashes.Contains(shader_hashes_ssao_upsampling);
      bool gtao_list = false;
      if (is_ssao_filter || is_ssao_upsampling)
      {
         const std::unique_lock lock(game_device_data.gtao_command_lists_mutex);
         gtao_list = std::ranges::find(game_device_data.gtao_command_lists, cmd_list_data.cmd_list) != game_device_data.gtao_command_lists.end();
         if (gtao_list && is_ssao_upsampling)
         {
            std::erase(game_device_data.gtao_command_lists, cmd_list_data.cmd_list);
         }
      }
      if (gtao_list)
      {
         if (is_ssao_filter)
         {
            return DrawOrDispatchOverrideType::Replaced;
         }
         if (!state.render_target_views.empty() && !state.render_target_descs.empty())
         {
            reshade::api::resource ao = {};
            {
               const std::shared_lock lock(game_device_data.gtao_pass.mutex);
               ao = game_device_data.gtao_final;
            }
            reshade::api::device* const device = cmd_list_data.cmd_list->get_device();
            const reshade::api::resource target = {state.render_target_views[0].resource};
            const reshade::api::resource_desc target_desc = device->get_resource_desc(target);
            // "gtao_final" is the depth's size (a retired one lives on until the lists recorded with it ran)
            const reshade::api::resource_desc ao_desc = (ao.handle != 0 ? device->get_resource_desc(ao) : reshade::api::resource_desc{});
            if (ao.handle != 0 && ao_desc.texture.width == target_desc.texture.width && ao_desc.texture.height == target_desc.texture.height &&
                DrawFullscreenPassD3D12(cmd_list_data, device_data, &game_device_data.gtao_copy_pass, CompileTimeStringHash("YKR XeGTAO Apply PS"), "XeGTAO Apply", "Luma XeGTAO Apply", ao, reshade::api::format::r8_unorm, target, state.render_target_descs[0], {}, alpha_only_blend))
            {
               return DrawOrDispatchOverrideType::Replaced;
            }
            // ponytail: the game's upsampling then reads its blur target, which the skipped blurs left as an older frame's
            return DrawOrDispatchOverrideType::None;
         }
      }

      const bool is_glow_pass0 = original_shader_hashes.Contains(shader_hashes_glow_pass0);
      if (is_glow_pass0)
      {
         glow_aberration_drawn = false;
      }
      if (g_luma_bloom_enable && is_glow_pass0 && !state.render_target_views.empty())
      {
         game_device_data.glow_level0_format.store(state.render_target_views[0].format, std::memory_order_relaxed);
         game_device_data.glow_level0.store(state.render_target_views[0].resource, std::memory_order_relaxed);
         return DrawOrDispatchOverrideType::None;
      }
      if (g_luma_bloom_enable && original_shader_hashes.Contains(shader_hashes_glow_pass1))
      {
         // Its kernel (FUN_1402e7c70, one for all 8 draws, of the stage's sigma): c_ps_user[1..3].x weigh the tap pairs at +-0.5, 1.5 and 2.5 source
         // texels (vs_glow_pass1), all on texel centers as every draw halves one axis. The same variance is DrawBloom's sigma
         if (const uint8_t* const mirror = GetConstantMirror())
         {
            const auto* const c_ps_user = reinterpret_cast<const float*>(mirror + mirror_data_offset + cb9_base_offset);
            const float w0 = c_ps_user[4];
            const float w1 = c_ps_user[8];
            const float w2 = c_ps_user[12];
            const float weight_sum = w0 + w1 + w2;
            if (weight_sum > 0.f)
            {
               game_device_data.glow_sigma.store(std::sqrt((0.25f * w0 + 2.25f * w1 + 6.25f * w2) / weight_sum), std::memory_order_relaxed);
            }
         }
         // The levels are Luma Bloom's while it draws (from the frame before too, so a frame it starts in keeps the game's)
         // ponytail: a frame it then can't draw (glow_pass2_5, a shader reload) shows the levels of the last frame the game drew them
         const uint64_t present_count = device_data.present_count.load(std::memory_order_relaxed);
         const uint64_t bloom_drawn_present = game_device_data.bloom_drawn_present.load(std::memory_order_relaxed);
         if (IsThisOrLastFrame(bloom_drawn_present, present_count))
         {
            return DrawOrDispatchOverrideType::Replaced;
         }
         return DrawOrDispatchOverrideType::None;
      }
      if (original_shader_hashes.Contains(shader_hashes_glow_pass1_ca))
      {
         glow_aberration_drawn = true;
         return DrawOrDispatchOverrideType::None;
      }
      const bool is_glow_pass2 = original_shader_hashes.Contains(shader_hashes_glow_pass2);
      if (is_glow_pass2 && !state.render_target_views.empty())
      {
         game_device_data.glow_target_format.store(state.render_target_views[0].format, std::memory_order_relaxed);
         game_device_data.glow_target.store(state.render_target_views[0].resource, std::memory_order_relaxed);
      }
      if (g_luma_bloom_enable && is_glow_pass2 && !glow_aberration_drawn && DrawLumaBloom(cmd_list_data, device_data, &game_device_data))
      {
         game_device_data.bloom_drawn_present.store(device_data.present_count.load(std::memory_order_relaxed), std::memory_order_relaxed);
         return DrawOrDispatchOverrideType::Replaced;
      }
      if (g_luma_bloom_enable && g_bloom_intensity != 1.f && original_shader_hashes.Contains(shader_hashes_glow_composite) && !state.render_target_views.empty() && !state.render_target_descs.empty())
      {
         const D3D12_BLEND_DESC* const blend = ((cmd_list_data.bound_cached_pipeline && cmd_list_data.bound_cached_pipeline->capture) ? cmd_list_data.bound_cached_pipeline->capture->Find<D3D12_BLEND_DESC>(D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_BLEND) : nullptr);
         const reshade::api::resource glow = {game_device_data.glow_target.load(std::memory_order_relaxed)};
         if (blend && blend->RenderTarget[0].BlendEnable && blend->RenderTarget[0].SrcBlend == D3D12_BLEND_ONE && blend->RenderTarget[0].DestBlend == D3D12_BLEND_ONE && glow.handle != 0)
         {
            if (g_bloom_intensity <= 0.f)
            {
               return DrawOrDispatchOverrideType::Replaced;
            }
            // "GlowConstants" after the target's size: the weight scale (unused here), the intensity
            const float constants[] = {1.f, g_bloom_intensity};
            if (DrawFullscreenPassD3D12(cmd_list_data, device_data, &game_device_data.bloom_apply_pass, CompileTimeStringHash("YKR Bloom Apply PS"), "Bloom Apply", "Luma Bloom Apply", glow, game_device_data.glow_target_format.load(std::memory_order_relaxed), {state.render_target_views[0].resource}, state.render_target_descs[0], constants, additive_blend))
            {
               return DrawOrDispatchOverrideType::Replaced;
            }
         }
      }

      if (original_shader_hashes.Contains(shader_hashes_fxaa_input) && !state.render_target_views.empty())
      {
         game_device_data.fxaa_input_format.store(state.render_target_views[0].format, std::memory_order_relaxed);
         game_device_data.fxaa_input.store(state.render_target_views[0].resource, std::memory_order_relaxed);
         return DrawOrDispatchOverrideType::None;
      }

      if (!state.in_render_pass && original_shader_hashes.Contains(shader_hashes_fxaa))
      {
         const reshade::api::resource color = {game_device_data.fxaa_input.load(std::memory_order_relaxed)};
         if (color.handle == 0 || state.render_target_views.empty() || state.render_target_descs.empty() || state.render_target_views[0].resource == 0 || state.render_target_views[0].resource == color.handle)
         {
            return DrawOrDispatchOverrideType::None;
         }
         const reshade::api::resource target = {state.render_target_views[0].resource};
         const reshade::api::format color_format = game_device_data.fxaa_input_format.load(std::memory_order_relaxed);
         const uint64_t present_count = device_data.present_count.load(std::memory_order_relaxed);
         // DLAA antialiased the scene: the canvas goes through as it is, sharpened as SMAA's output is (the same gamma canvas, its alpha unused after FXAA)
         if (game_device_data.dlaa_drawn_present.load(std::memory_order_relaxed) == present_count)
         {
            bool drawn = false;
            if (g_rcas_sharpness > 0.f)
            {
               drawn = DrawRCAS(cmd_list_data, device_data, color, color_format, target, state.render_target_descs[0], g_rcas_sharpness);
            }
            if (!drawn)
            {
               drawn = DrawFullscreenPassD3D12(cmd_list_data, device_data, &game_device_data.copy_pass, CompileTimeStringHash("YKR DLAA Copy PS"), "FXAA Bypass", "Luma FXAA Bypass", color, color_format, target, state.render_target_descs[0], {});
            }
            return drawn ? DrawOrDispatchOverrideType::Replaced : DrawOrDispatchOverrideType::None;
         }
         if (!g_smaa_enable)
         {
            return DrawOrDispatchOverrideType::None;
         }
         reshade::api::device* const device = cmd_list_data.cmd_list->get_device();
         const reshade::api::resource_desc target_desc = device->get_resource_desc(target);
         reshade::api::resource predication = {};
         bool predicated = false;
         {
            const std::unique_lock lock(game_device_data.predication_pass.mutex);
            // The edge detection reads its t1 unpredicated too (at strength 0): without the SSAO draw, a target of the canvas' size is made for it
            if (game_device_data.predication.handle != 0 || game_device_data.EnsurePredicationTarget(device, present_count, target_desc.texture.width, target_desc.texture.height))
            {
               predication = game_device_data.predication;
               const reshade::api::resource_desc predication_desc = device->get_resource_desc(predication);
               predicated = g_smaa_predication && IsThisOrLastFrame(game_device_data.predication_present, present_count) && predication_desc.texture.width == target_desc.texture.width && predication_desc.texture.height == target_desc.texture.height;
            }
         }
         if (predication.handle == 0)
         {
            return DrawOrDispatchOverrideType::None;
         }
         // "Luma_SMAA_impl.hlsl": the predication's scale and strength, plain ULTRA without it
         const float smaa_constants[4] = {(predicated ? 2.f : 1.f), (predicated ? 0.5f : 0.f), 0.f, 0.f};
         // SMAA T2x: once a frame, from the motion vectors DLAA's pass made for it. Without any piece the 1x SMAA below runs
         if (game_device_data.t2x_active.load(std::memory_order_relaxed) && game_device_data.t2x_claimed_present.exchange(present_count, std::memory_order_relaxed) != present_count)
         {
            reshade::api::resource motion_vectors = {};
            {
               const std::shared_lock lock(game_device_data.motion_vector_pass.mutex);
               // Recorded at the ccr draw, maybe on another worker (see "IsThisOrLastFrame()")
               if (IsThisOrLastFrame(game_device_data.motion_vectors_present.load(std::memory_order_relaxed), present_count))
               {
                  motion_vectors = game_device_data.motion_vectors;
               }
            }
            if (motion_vectors.handle != 0)
            {
               const reshade::api::resource_desc motion_vectors_desc = device->get_resource_desc(motion_vectors);
               float applied_jitter_x = 0.f;
               {
                  const std::shared_lock lock(camera_state.mutex);
                  applied_jitter_x = (camera_state.capture ? camera_state.capture->jitter_pixels[0] : 0.f);
               }
               // A sample offset of (+0.25, +0.25) takes the area texture's subsample 1, (-0.25, -0.25) the 2 (0: no jitter)
               float subsample_index = 0.f;
               if (applied_jitter_x != 0.f)
               {
                  subsample_index = ((applied_jitter_x > 0.f) ? 1.f : 2.f);
               }
               const uint32_t current_frame = uint32_t(present_count & 1);
               std::array<reshade::api::resource, 3> frames = {};
               bool history_valid = false;
               if (motion_vectors_desc.texture.width == target_desc.texture.width && motion_vectors_desc.texture.height == target_desc.texture.height)
               {
                  const std::unique_lock lock(game_device_data.t2x_pass.mutex);
                  bool created_frames[2] = {};
                  if (game_device_data.EnsurePassTarget(&game_device_data.t2x_pass, &game_device_data.t2x_frames[0], device, present_count, target_desc.texture.width, target_desc.texture.height, reshade::api::format::r16g16b16a16_float, reshade::api::resource_usage::render_target | reshade::api::resource_usage::shader_resource, reshade::api::resource_usage::shader_resource, L"Luma YKR SMAA T2x Frame 0", &created_frames[0]) &&
                      game_device_data.EnsurePassTarget(&game_device_data.t2x_pass, &game_device_data.t2x_frames[1], device, present_count, target_desc.texture.width, target_desc.texture.height, reshade::api::format::r16g16b16a16_float, reshade::api::resource_usage::render_target | reshade::api::resource_usage::shader_resource, reshade::api::resource_usage::shader_resource, L"Luma YKR SMAA T2x Frame 1", &created_frames[1]))
                  {
                     // "DrawFullscreenPassD3D12()" takes the pass' mutex. A target retired meanwhile lives on until the lists recorded with it ran.
                     history_valid = !created_frames[0] && !created_frames[1] && game_device_data.t2x_drawn_present.load(std::memory_order_relaxed) + 1 == present_count;
                     // RCAS can't read the target it writes: the resolve goes to a third frame first
                     if (g_rcas_sharpness > 0.f)
                     {
                        game_device_data.EnsurePassTarget(&game_device_data.t2x_pass, &game_device_data.t2x_frames[2], device, present_count, target_desc.texture.width, target_desc.texture.height, reshade::api::format::r16g16b16a16_float, reshade::api::resource_usage::render_target | reshade::api::resource_usage::shader_resource, reshade::api::resource_usage::shader_resource, L"Luma YKR SMAA T2x Resolved");
                     }
                     frames = game_device_data.t2x_frames;
                  }
               }
               if (frames[current_frame].handle != 0)
               {
                  const reshade::api::resource blend_target = frames[current_frame];
                  // The first frame (or after a gap) has no history: the resolve reads this frame's blend twice
                  const reshade::api::resource history = (history_valid ? frames[current_frame ^ 1] : blend_target);
                  const reshade::api::resource_view_desc blend_desc(reshade::api::resource_view_type::texture_2d, reshade::api::format::r16g16b16a16_float, 0, 1, 0, 1);
                  const float t2x_constants[4] = {smaa_constants[0], smaa_constants[1], subsample_index, 0.f};
                  const SMAAT2xD3D12 t2x = {.blending_weight_calculation_ps = CompileTimeStringHash("YKR SMAA T2x Blending Weight Calculation PS"), .neighborhood_blending_ps = CompileTimeStringHash("YKR SMAA T2x Neighborhood Blending PS"), .velocity = motion_vectors, .velocity_format = reshade::api::format::r16g16_float};
                  cmd_list_data.cmd_list->barrier(blend_target, reshade::api::resource_usage::shader_resource, reshade::api::resource_usage::render_target);
                  const bool blended = DrawSMAA(cmd_list_data, device_data, color, color_format, predication, reshade::api::format::r8_unorm, blend_target, blend_desc, t2x_constants, 0.f, &t2x);
                  cmd_list_data.cmd_list->barrier(blend_target, reshade::api::resource_usage::render_target, reshade::api::resource_usage::shader_resource);
                  // The canvas' alpha stays
                  const D3D12FullscreenSource resolve_sources[] = {{.resource = blend_target, .format = reshade::api::format::r16g16b16a16_float}, {.resource = history, .format = reshade::api::format::r16g16b16a16_float}, {.resource = motion_vectors, .format = reshade::api::format::r16g16_float}, {.resource = color, .format = color_format}};
                  const reshade::api::resource resolved = frames[2];
                  bool resolved_drawn = false;
                  if (blended && g_rcas_sharpness > 0.f && resolved.handle != 0)
                  {
                     cmd_list_data.cmd_list->barrier(resolved, reshade::api::resource_usage::shader_resource, reshade::api::resource_usage::render_target);
                     const bool resolved_into_frame = DrawFullscreenPassD3D12(cmd_list_data, device_data, &game_device_data.t2x_pass, CompileTimeStringHash("YKR SMAA T2x Resolve PS"), "SMAA T2x Resolve", "Luma SMAA T2x Resolve", resolve_sources, resolved, blend_desc, {});
                     cmd_list_data.cmd_list->barrier(resolved, reshade::api::resource_usage::render_target, reshade::api::resource_usage::shader_resource);
                     resolved_drawn = resolved_into_frame && DrawRCAS(cmd_list_data, device_data, resolved, reshade::api::format::r16g16b16a16_float, target, state.render_target_descs[0], g_rcas_sharpness);
                  }
                  // Without RCAS (or if it couldn't draw) the resolve writes the target itself
                  if (blended && !resolved_drawn)
                  {
                     resolved_drawn = DrawFullscreenPassD3D12(cmd_list_data, device_data, &game_device_data.t2x_pass, CompileTimeStringHash("YKR SMAA T2x Resolve PS"), "SMAA T2x Resolve", "Luma SMAA T2x Resolve", resolve_sources, target, state.render_target_descs[0], {});
                  }
                  if (resolved_drawn)
                  {
                     game_device_data.t2x_drawn_present.store(present_count, std::memory_order_relaxed);
                     return DrawOrDispatchOverrideType::Replaced;
                  }
               }
            }
         }
         if (DrawSMAA(cmd_list_data, device_data, color, color_format, predication, reshade::api::format::r8_unorm, target, state.render_target_descs[0], smaa_constants, g_rcas_sharpness))
         {
            return DrawOrDispatchOverrideType::Replaced;
         }
         return DrawOrDispatchOverrideType::None;
      }

      if (is_custom_pass && original_shader_hashes.Contains(shader_hashes_vanilla_scene_sources))
      {
         const std::vector<D3D12State::ViewInfo>& render_targets = cmd_list_data.d3d12_state.render_target_views;
         const reshade::api::format target_format = (render_targets.empty() ? reshade::api::format::unknown : render_targets[0].format);
         const bool target_stays_8_bit = target_format == reshade::api::format::b8g8r8a8_unorm || target_format == reshade::api::format::r8g8b8a8_unorm;
         SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, stages, LumaConstantBufferType::LumaData, target_stays_8_bit ? 1 : 0);
         updated_cbuffers = true;
      }
      // The screen effects draw over the canvas the ccr pre-scaled to the UI paper white: flagged, their color goes back to the scene's paper white
      if (is_custom_pass && original_shader_hashes.Contains(shader_hashes_screen_effect_color))
      {
         const bool scene_brightness = screen_effect_depth > 0 && GetFramePhase() == last_post_anti_aliasing_phase;
         SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, stages, LumaConstantBufferType::LumaData, scene_brightness ? 1 : 0);
         updated_cbuffers = true;
      }
      return DrawOrDispatchOverrideType::None;
   }

   // The prepass draw of a character or hair part, re-issued with the offline patched shaders of its pair (see "MotionVectorShaders.h"): a pipeline
   // variant with a second render target, the object motion vectors, and the roots the patch reads (b9 last frame's camera, b8 the draw's slot in the
   // position caches, u0 the cache it writes, t0 the cache it reads). The pipeline and render targets are set again after, as the game left them (the
   // roots are Luma's own, which the game's shaders don't read). True if the
   // draw was recorded here (the game's must be skipped). Recorded natively, on the game's list, with the views of this thread's slot of the native
   // descriptor heaps: D3D12 copies descriptors at "OMSetRenderTargets()", so they serve the bind and the restore.
   static bool DrawObjectMotionVectors(NativeDevice* native_device, NativeCommandList* native_list, CommandListData& cmd_list_data, DeviceData& device_data, YakuzaKiwamiDeviceData* game_device_data, const MotionVectorShaders::Pair* pair, bool anchor_read, const std::array<float, 3>& anchor, const std::array<float, world_matrix_floats>* rigid_previous_world)
   {
      if (!pair)
      {
         return false;
      }
      // Of the rigid world, only the parts that moved since last frame (see "rigid_draws")
      const bool rigid = pair->kind == MotionVectorShaders::Kind::RIGID_MATRIX;
      if (rigid && !rigid_previous_world)
      {
         return false;
      }

      const D3D12State::CommandListState& state = cmd_list_data.d3d12_state;
      const DrawDispatchData& draw = last_draw_dispatch_data;
      const uint64_t present_count = device_data.present_count.load(std::memory_order_relaxed);
      reshade::api::device* const device = cmd_list_data.cmd_list->get_device();
      const auto refuse = [](ObjectMotionCount reason)
      {
         CountObjectMotion(reason);
         return false;
      };
      if (!anchor_read)
      {
         return refuse(MV_REFUSED_NO_MIRROR);
      }
      if (cmd_list_data.is_bundle)
      {
         return refuse(MV_REFUSED_BUNDLE);
      }
      if (state.in_render_pass)
      {
         return refuse(MV_REFUSED_RENDER_PASS);
      }
      if (!draw.indexed || draw.indirect)
      {
         return refuse(MV_REFUSED_NOT_INDEXED);
      }
      if (draw.instance_count != 1)
      {
         return refuse(MV_REFUSED_INSTANCED);
      }
      if (state.render_target_views.size() != 1 || state.render_target_descs.size() != 1 || state.render_target_views[0].resource == 0)
      {
         return refuse(MV_REFUSED_RT_COUNT);
      }
      if (state.depth_stencil_view.resource == 0 || state.depth_stencil_view.resource != game_device_data->scene_depth.load(std::memory_order_relaxed))
      {
         return refuse(MV_REFUSED_OTHER_DSV);
      }

      // The target, cleared by the last frame's DLAA draw, at the size of this one. This frame's DLAA draw may already have been recorded (another worker
      // records its list first) and set the next frame: the GPU still runs this list before it, so what it writes reaches this frame's DLAA
      const uint64_t object_motion_handle = game_device_data->object_motion_vectors_handle.load(std::memory_order_relaxed);
      const uint64_t object_motion_ready_present = game_device_data->object_motion_vectors_ready_present.load(std::memory_order_relaxed);
      const reshade::api::resource_desc render_target_desc = device->get_resource_desc({state.render_target_views[0].resource});
      if (object_motion_handle == 0 || (object_motion_ready_present != present_count && object_motion_ready_present != present_count + 1) || game_device_data->object_motion_vectors_size.load(std::memory_order_relaxed) != ((uint64_t(render_target_desc.texture.width) << 32) | render_target_desc.texture.height))
      {
         return refuse(MV_REFUSED_NO_TEXTURE);
      }

      // The first of the game's root parameters of the draw's root signature, set again every draw (a Luma pass on the list resets them)
      thread_local uint64_t cached_layout = 0;
      thread_local int32_t cached_game_index = -1;
      if (cached_layout != state.graphics.layout.handle)
      {
         cached_layout = state.graphics.layout.handle;
         cached_game_index = D3D12RootSignatures::Find(cached_layout).game_index;
      }
      if (cached_game_index < 0)
      {
         return refuse(MV_REFUSED_NO_GAME_PARAMS);
      }
      if (state.vertex_buffers.empty() || state.vertex_buffers[0].buffer == 0)
      {
         return refuse(MV_REFUSED_NO_VERTEX_BUFFER);
      }

      D3D12_RENDER_TARGET_VIEW_DESC render_target_view_desc;
      D3D12_DEPTH_STENCIL_VIEW_DESC depth_stencil_view_desc;
      if (!ToRenderTargetViewDesc(state.render_target_descs[0], &render_target_view_desc) || !ToDepthStencilViewDesc(state.depth_stencil_desc, &depth_stencil_view_desc))
      {
         return refuse(MV_REFUSED_BAD_VIEW);
      }
      const uint32_t thread_slot = GetObjectMotionThreadSlot();
      if (thread_slot == UINT32_MAX)
      {
         return refuse(MV_REFUSED_NO_THREAD_SLOT);
      }
      // One variant per pair and render target format; the draw keeps the game's pipeline where the variant is refused (the test-only redraw)
      std::array<DXGI_FORMAT, 8> variant_formats = {};
      variant_formats[0] = DXGI_FORMAT(state.render_target_views[0].format);
      const uint64_t variant_pipeline = (cmd_list_data.bound_cached_pipeline ? FindOrCreateD3D12GameVariant(*cmd_list_data.bound_cached_pipeline, GetObjectMotionVariant(pair - std::begin(MotionVectorShaders::PAIRS)), variant_formats) : 0);
      if (variant_pipeline == 0)
      {
         return refuse(MV_REFUSED_NO_VARIANT);
      }

      // The cache is indexed by SV_VertexID, the raw index (D3D11.3 functional spec 8.16, checked on D3D12): parts of one vertex buffer with
      // different base vertices would share slot entries. Every captured character draw has base 0; any other keeps the camera motion
      if (!rigid && draw.vertex_offset != 0)
      {
         return refuse(MV_REFUSED_BASE_VERTEX);
      }

      // A rigid part draws alone; the parts of a skinned object come one after the other on this list, their first indices rising
      ObjectMotionRun rigid_run;
      ObjectMotionRun& run = (rigid ? rigid_run : object_motion_run);
      const ObjectMotionVertexBuffer vertex_buffer = {.buffer = state.vertex_buffers[0].buffer, .offset = state.vertex_buffers[0].offset};
      // The vertices the object's streams hold, at most (the buffer is the asset's: they are the first vertices of the asset). Only the slots its input
      // layout reads per vertex count: the engine rebinds slot 0 alone, so slot 1 can still hold the 144 byte instance stream of an earlier
      // instanced draw (FUN_140b9b2c0), which made some characters' punch-through parts (hair) a 1 vertex slot
      const auto streams_vertex_count = [&]()
      {
         uint32_t vertex_slots = UINT32_MAX; // Without the captured layout every bound slot counts
         if (cmd_list_data.bound_cached_pipeline && cmd_list_data.bound_cached_pipeline->capture)
         {
            vertex_slots = 0;
            for (const D3D12_INPUT_ELEMENT_DESC& element : cmd_list_data.bound_cached_pipeline->capture->input_elements)
            {
               if (element.InputSlotClass == D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA)
               {
                  vertex_slots |= 1u << element.InputSlot;
               }
            }
         }
         uint32_t vertex_count = object_mv_max_slot_vertices;
         for (size_t slot = 0; slot < state.vertex_buffers.size(); slot++)
         {
            const D3D12State::BufferBinding& binding = state.vertex_buffers[slot];
            if (binding.buffer != 0 && binding.stride != 0 && (vertex_slots & (1u << slot)) != 0)
            {
               const uint64_t buffer_size = reinterpret_cast<ID3D12Resource*>(binding.buffer)->GetDesc().Width;
               vertex_count = (std::min)(vertex_count, buffer_size > binding.offset ? uint32_t((std::min)((buffer_size - binding.offset) / binding.stride, uint64_t(object_mv_max_slot_vertices))) : 0u);
            }
         }
         return vertex_count;
      };
      // Camera constants of the frame, in the memory of the list's upload pages (freed once its executions are over). Taken before the run records
      // the part as drawn: a part refused after that would leave its cache entries last frame's while the next frame reads them as this one's
      const D3D12UploadRing::Allocation camera_upload = D3D12UploadRing::Allocate(cmd_list_data.upload_ring.get(), &cmd_list_data.upload_pages, sizeof(run.camera_constants), present_count);
      if (!camera_upload.cpu)
      {
         return refuse(MV_REFUSED_NO_BUFFERS);
      }
      if (rigid)
      {
         if (!game_device_data->object_motion.StartRigidDraw(&run, device, present_count, *rigid_previous_world))
         {
            return refuse(MV_REFUSED_NO_BUFFERS);
         }
      }
      else if (run.cmd_list == cmd_list_data.cmd_list && run.stamp == present_count && run.vertex_buffer == vertex_buffer && draw.first_index > run.last_first_index)
      {
         run.last_first_index = draw.first_index;
         game_device_data->object_motion.AddPart(run, draw.first_index);
      }
      else
      {
         // The tree shaders keep w too (their wind moves it)
         const uint32_t vertex_stride = ((pair->kind == MotionVectorShaders::Kind::TREE_CACHE) ? 4 : 3) * sizeof(float);
         if (!game_device_data->object_motion.StartRun(&run, device, present_count, cmd_list_data.cmd_list, vertex_buffer, draw.first_index, anchor, streams_vertex_count(), vertex_stride))
         {
            return refuse(MV_REFUSED_NO_BUFFERS);
         }
      }
      std::memcpy(camera_upload.cpu, run.camera_constants.data(), sizeof(run.camera_constants));

      const YakuzaKiwamiDeviceData::ObjectMotionState& object_motion = game_device_data->object_motion;
      const D3D12_CPU_DESCRIPTOR_HANDLE render_target_handles[2] = {{object_motion.render_target_heap_start.ptr + size_t(thread_slot) * 2 * object_motion.render_target_increment}, {object_motion.render_target_heap_start.ptr + (size_t(thread_slot) * 2 + 1) * object_motion.render_target_increment}};
      const D3D12_CPU_DESCRIPTOR_HANDLE depth_stencil_handle = {object_motion.depth_stencil_heap_start.ptr + size_t(thread_slot) * object_motion.depth_stencil_increment};
      constexpr D3D12_RENDER_TARGET_VIEW_DESC object_motion_view_desc = {.Format = DXGI_FORMAT_R16G16_FLOAT, .ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D};
      native_device->CreateRenderTargetView(reinterpret_cast<ID3D12Resource*>(state.render_target_views[0].resource), &render_target_view_desc, render_target_handles[0]);
      native_device->CreateRenderTargetView(reinterpret_cast<ID3D12Resource*>(object_motion_handle), &object_motion_view_desc, render_target_handles[1]);
      native_device->CreateDepthStencilView(reinterpret_cast<ID3D12Resource*>(state.depth_stencil_view.resource), &depth_stencil_view_desc, depth_stencil_handle);

      const UINT game_index = UINT(cached_game_index);
      native_list->OMSetRenderTargets(2, render_target_handles, FALSE, &depth_stencil_handle);
      native_list->SetPipelineState(reinterpret_cast<ID3D12PipelineState*>(variant_pipeline));
      native_list->SetGraphicsRootConstantBufferView(game_index, camera_upload.gpu);
      // A part the object didn't draw last frame (culled, another LOD) has no positions of last frame in the slot, only older ones, maybe
      // another object's: it keeps the camera motion
      std::array<uint32_t, 4> draw_constants = run.draw_constants;
      draw_constants[3] = ((draw_constants[3] != 0 && std::binary_search(run.previous_parts.begin(), run.previous_parts.end(), draw.first_index)) ? 1u : 0u);
      native_list->SetGraphicsRoot32BitConstants(game_index + 1, UINT(draw_constants.size()), draw_constants.data(), 0);
      native_list->SetGraphicsRootUnorderedAccessView(game_index + 2, run.write_address);
      native_list->SetGraphicsRootShaderResourceView(game_index + 3, run.read_address);
      native_list->DrawIndexedInstanced(draw.index_count, 1, draw.first_index, draw.vertex_offset, draw.first_instance);
      native_list->SetPipelineState(reinterpret_cast<ID3D12PipelineState*>(cmd_list_data.bound_current_pipeline));
      native_list->OMSetRenderTargets(1, render_target_handles, FALSE, &depth_stencil_handle);
      CountObjectMotion(MV_PATCHED);
      return true;
   }

   // DLAA of the scene a ccr draw reads (see "shader_hashes_color_correct"), recorded before that draw, at most once a frame: camera motion
   // vectors from the depth copy, the scene decoded into DLSS' input, DLSS, then its output encoded back into the scene in place. Without a
   // piece (no depth copy this frame, no camera, another size) the scene stays as it is and FXAA or SMAA run; a failed DLSS turns it off
   // until it's picked again.
   static void DrawDLAA(CommandListData& cmd_list_data, DeviceData& device_data, YakuzaKiwamiDeviceData* game_device_data)
   {
      const D3D12State::CommandListState& state = cmd_list_data.d3d12_state;
      const uint64_t present_count = device_data.present_count.load(std::memory_order_relaxed);
      SR::InstanceData* const sr_instance_data = LockSRForDraw(device_data).instance_data;
      const SceneWriter scene = scene_writer;
      if (state.in_render_pass || scene.resource == 0 || scene.present_count != present_count || state.render_target_views.empty() || state.render_target_views[0].resource == scene.resource || game_device_data->dlaa_claimed_present.exchange(present_count, std::memory_order_relaxed) == present_count)
      {
         return;
      }
      reshade::api::command_list* const cmd_list = cmd_list_data.cmd_list;
      reshade::api::device* const device = cmd_list->get_device();
      const reshade::api::resource scene_resource = {scene.resource};
      const reshade::api::resource_desc scene_desc = device->get_resource_desc(scene_resource);
      const uint32_t width = scene_desc.texture.width;
      const uint32_t height = scene_desc.texture.height;

      reshade::api::resource depth = {};
      {
         const std::shared_lock lock(game_device_data->copy_pass.mutex);
         // Recorded at the low-res depth draw, often on another worker (see "IsThisOrLastFrame()"). A copy point that stops drawing turns DLAA off a frame later
         if (IsThisOrLastFrame(game_device_data->depth_copy_present, present_count))
         {
            depth = game_device_data->depth_copy;
         }
      }
      if (depth.handle == 0)
      {
         return;
      }
      // A retired copy lives on until the lists recorded with it ran
      const reshade::api::resource_desc depth_desc = device->get_resource_desc(depth);
      if (depth_desc.texture.width != width || depth_desc.texture.height != height)
      {
         return;
      }

      const std::optional<CameraState::Capture> capture = []
      {
         const std::shared_lock lock(camera_state.mutex);
         return camera_state.capture;
      }();
      if (!capture)
      {
         return;
      }
      const auto& [view, projection, jitter_pixels, jitter_ndc] = *capture;

      constexpr reshade::api::resource_usage render_target_and_shader_resource = reshade::api::resource_usage::render_target | reshade::api::resource_usage::shader_resource;
      reshade::api::resource motion_vectors = {};
      reshade::api::resource object_motion_vectors = {};
      {
         const std::unique_lock lock(game_device_data->motion_vector_pass.mutex);
         if (game_device_data->EnsurePassTarget(&game_device_data->motion_vector_pass, &game_device_data->motion_vectors, device, present_count, width, height, reshade::api::format::r16g16_float, render_target_and_shader_resource, reshade::api::resource_usage::shader_resource, L"Luma YKR DLAA Motion Vectors"))
         {
            motion_vectors = game_device_data->motion_vectors;
         }
         bool object_motion_created = false;
         if (g_object_motion_vectors && game_device_data->EnsurePassTarget(&game_device_data->motion_vector_pass, &game_device_data->object_motion_vectors, device, present_count, width, height, reshade::api::format::r16g16_float, render_target_and_shader_resource, reshade::api::resource_usage::render_target, L"Luma YKR DLAA Object Motion Vectors", &object_motion_created))
         {
            object_motion_vectors = game_device_data->object_motion_vectors;
            game_device_data->object_motion_vectors_handle.store(object_motion_vectors.handle, std::memory_order_relaxed);
            game_device_data->object_motion_vectors_size.store((uint64_t(width) << 32) | height, std::memory_order_relaxed);
            if (object_motion_created)
            {
               // Its content is undefined until the clear below
               game_device_data->object_motion_vectors_ready_present.store(UINT64_MAX, std::memory_order_relaxed);
            }
         }
      }
      // Only what the patched draws wrote since the clear of the frame before is the object motion
      const bool use_object_motion = object_motion_vectors.handle != 0 && game_device_data->object_motion_vectors_ready_present.load(std::memory_order_relaxed) == present_count;
      const bool dlaa_active = game_device_data->dlaa_active.load(std::memory_order_relaxed);
      if (motion_vectors.handle == 0)
      {
         if (dlaa_active)
         {
            device_data.sr_suppressed = true;
            reshade::log::message(reshade::log::level::error, "[DLAA] ReShade refused its textures, so it's off until it's picked again");
         }
         return;
      }

      // Current clip space to the previous frame's, column vectors (the game's matrices multiply row vectors), in double
      Math::Matrix44D current_view;
      Math::Matrix44D current_projection;
      std::copy_n(view.data(), 16, current_view.GetData());
      std::copy_n(projection.data(), 16, current_projection.GetData());
      Math::Matrix44D current = current_view * current_projection;
      current.Transpose();
      const bool has_previous = game_device_data->previous_view_projection_present != UINT64_MAX && game_device_data->previous_view_projection_present + 1 == present_count;
      const Math::Matrix44D previous = (has_previous ? game_device_data->previous_view_projection : current);
      game_device_data->previous_view_projection = current;
      game_device_data->previous_view_projection_present = present_count;
      const Math::Matrix44D reprojection = previous * current.GetInverted();
      // After the target's size: whether the object motion vectors count (and a padding to the matrix' 16 byte row), the reprojection, the jitter (see "Luma_YKR_DLAA.hlsl")
      std::array<float, 20> motion_constants = {};
      motion_constants[0] = (use_object_motion ? 1.f : 0.f);
      std::transform(reprojection.GetData(), reprojection.GetData() + 16, motion_constants.begin() + 2, [](double value)
         { return float(value); });
      motion_constants[18] = jitter_ndc[0];
      motion_constants[19] = jitter_ndc[1];
      const reshade::api::resource_view_desc motion_vectors_desc(reshade::api::resource_view_type::texture_2d, reshade::api::format::r16g16_float, 0, 1, 0, 1);
      cmd_list->barrier(motion_vectors, reshade::api::resource_usage::shader_resource, reshade::api::resource_usage::render_target);
      if (use_object_motion)
      {
         cmd_list->barrier(object_motion_vectors, reshade::api::resource_usage::render_target, reshade::api::resource_usage::shader_resource);
      }
      // The pass keeps two sources: the depth stands in for the object motion vectors where they don't count
      const D3D12FullscreenSource motion_sources[] = {{.resource = depth, .format = reshade::api::format::r32_float}, {.resource = (use_object_motion ? object_motion_vectors : depth), .format = (use_object_motion ? reshade::api::format::r16g16_float : reshade::api::format::r32_float)}};
      const bool motion_vectors_drawn = DrawFullscreenPassD3D12(cmd_list_data, device_data, &game_device_data->motion_vector_pass, CompileTimeStringHash("YKR DLAA Motion Vectors PS"), "DLAA Motion Vectors", "Luma DLAA Motion Vectors", motion_sources, motion_vectors, motion_vectors_desc, motion_constants);
      cmd_list->barrier(motion_vectors, reshade::api::resource_usage::render_target, reshade::api::resource_usage::shader_resource);
      if (motion_vectors_drawn)
      {
         game_device_data->motion_vectors_present.store(present_count, std::memory_order_relaxed);
      }
      if (use_object_motion)
      {
         cmd_list->barrier(object_motion_vectors, reshade::api::resource_usage::shader_resource, reshade::api::resource_usage::render_target);
      }
      // Cleared whether it was read or not, for the next frame's patched draws
      if (object_motion_vectors.handle != 0)
      {
         reshade::api::resource_view object_motion_view = {};
         if (device->create_resource_view(object_motion_vectors, reshade::api::resource_usage::render_target, reshade::api::resource_view_desc(reshade::api::resource_view_type::texture_2d, reshade::api::format::r16g16_float, 0, 1, 0, 1), &object_motion_view))
         {
            const float marker_color[4] = {motion_vector_marker, motion_vector_marker, 0.f, 0.f};
            cmd_list->clear_render_target_view(object_motion_view, marker_color);
            device->destroy_resource_view(object_motion_view);
            game_device_data->object_motion_vectors_ready_present.store(present_count + 1, std::memory_order_relaxed);
         }
      }
      // SMAA T2x needs the motion vectors only
      if (!dlaa_active || !sr_instance_data || !motion_vectors_drawn)
      {
         return;
      }
      reshade::api::resource decoded_scene = {};
      reshade::api::resource output = {};
      bool output_recreated = false;
      {
         const std::unique_lock lock(game_device_data->color_pass.mutex);
         bool output_created = false;
         if (game_device_data->EnsurePassTarget(&game_device_data->color_pass, &game_device_data->decoded_scene, device, present_count, width, height, reshade::api::format::r16g16b16a16_float, render_target_and_shader_resource, reshade::api::resource_usage::shader_resource, L"Luma YKR DLAA Decoded Scene") &&
             game_device_data->EnsurePassTarget(&game_device_data->color_pass, &game_device_data->dlaa_output, device, present_count, width, height, reshade::api::format::r16g16b16a16_float, reshade::api::resource_usage::unordered_access | reshade::api::resource_usage::shader_resource, reshade::api::resource_usage::unordered_access, L"Luma YKR DLAA Output", &output_created))
         {
            decoded_scene = game_device_data->decoded_scene;
            output = game_device_data->dlaa_output;
            game_device_data->dlaa_output_recreated = game_device_data->dlaa_output_recreated || output_created;
            output_recreated = game_device_data->dlaa_output_recreated;
         }
      }
      if (output.handle == 0)
      {
         device_data.sr_suppressed = true;
         reshade::log::message(reshade::log::level::error, "[DLAA] ReShade refused its textures, so it's off until it's picked again");
         return;
      }

      // The scene is in the state the game's barriers left it in since its last draw, a pixel shader resource normally (ccr reads it)
      const bool scene_readable = (scene.state & reshade::api::resource_usage::shader_resource_pixel) == reshade::api::resource_usage::shader_resource_pixel;
      const reshade::api::resource_usage scene_read_state = (scene_readable ? scene.state : reshade::api::resource_usage::shader_resource_pixel);
      if (!scene_readable)
      {
         cmd_list->barrier(scene_resource, scene.state, scene_read_state);
      }
      const reshade::api::resource_view_desc color_desc(reshade::api::resource_view_type::texture_2d, reshade::api::format::r16g16b16a16_float, 0, 1, 0, 1);
      cmd_list->barrier(decoded_scene, reshade::api::resource_usage::shader_resource, reshade::api::resource_usage::render_target);
      const bool decoded = DrawFullscreenPassD3D12(cmd_list_data, device_data, &game_device_data->color_pass, CompileTimeStringHash("YKR DLAA Decode PS"), "DLAA Decode", "Luma DLAA Decode", scene_resource, scene.desc.format, decoded_scene, color_desc, {});
      cmd_list->barrier(decoded_scene, reshade::api::resource_usage::render_target, reshade::api::resource_usage::shader_resource);

      bool sr_drawn = false;
      bool sr_switched = false;
      if (decoded)
      {
         // The inputs stay in a shader resource state (pixel and non pixel), which DLSS and FSR read; the output is in unordered access
         auto* const native_list = reinterpret_cast<ID3D12GraphicsCommandList*>(cmd_list->get_native());
         // UV deltas, previous minus current. Exposure 1: the scene is already exposed and tonemapped
         const SR::SettingsData settings_data = {.output_width = width, .output_height = height, .render_width = width, .render_height = height, .hdr = true, .inverted_depth = false, .mvs_jittered = false, .mvs_x_scale = float(width), .mvs_y_scale = float(height), .auto_exposure = false, .render_preset = dlss_render_preset};
         // FSR's camera (DLSS ignores it), from the engine's row vector projection
         const SR::ViewProjectionCamera camera = SR::GetViewProjectionCamera(projection.data(), true);
         const SR::SuperResolutionImpl::DrawData draw_data = {
            .reset = device_data.force_reset_sr,
            .output_recreated = output_recreated,
            .output_color = reinterpret_cast<ID3D12Resource*>(output.handle),
            .source_color = reinterpret_cast<ID3D12Resource*>(decoded_scene.handle),
            .motion_vectors = reinterpret_cast<ID3D12Resource*>(motion_vectors.handle),
            .depth_buffer = reinterpret_cast<ID3D12Resource*>(depth.handle),
            .render_width = width,
            .render_height = height,
            .jitter_x = jitter_pixels[0],
            .jitter_y = jitter_pixels[1],
            .vert_fov = float(camera.vert_fov),
            .near_plane = float(camera.near_plane),
            .far_plane = float(camera.far_plane),
         };
         {
            const SRDrawLock sr = LockSRForDraw(device_data);
            // A present switched the SR since "sr_instance_data" was read: nothing is drawn this frame
            sr_switched = sr.instance_data != sr_instance_data;
            sr_drawn = !sr_switched && sr.implementation->UpdateSettings(sr.instance_data, native_list, settings_data) && sr.implementation->Draw(sr.instance_data, native_list, draw_data);
         }
         // DLSS and FSR bind their own descriptor heaps, root signatures and pipelines
         D3D12State::RestoreCompute(cmd_list, state, {});
         RestoreGameGraphicsD3D12(cmd_list_data);
         TraceLumaPass(&cmd_list_data, device_data, "Luma DLAA", {{.resource = decoded_scene.handle, .format = reshade::api::format::r16g16b16a16_float}, {.resource = motion_vectors.handle, .format = reshade::api::format::r16g16_float}, {.resource = depth.handle, .format = reshade::api::format::r32_float}}, {{.resource = output.handle, .format = reshade::api::format::r16g16b16a16_float}}, true);
         if (sr_drawn)
         {
            const std::unique_lock lock(game_device_data->color_pass.mutex);
            game_device_data->dlaa_output_recreated = false;
         }
         else if (!sr_switched)
         {
            device_data.sr_suppressed = true;
            reshade::log::message(reshade::log::level::error, std::format("[DLAA] {} failed, so it's off until it's picked again", SR::GetTypeName(device_data.sr_type)).c_str());
         }
      }

      bool encoded = false;
      if (sr_drawn)
      {
         cmd_list->barrier(output, reshade::api::resource_usage::unordered_access, reshade::api::resource_usage::shader_resource);
         cmd_list->barrier(scene_resource, scene_read_state, reshade::api::resource_usage::render_target);
         encoded = DrawFullscreenPassD3D12(cmd_list_data, device_data, &game_device_data->color_pass, CompileTimeStringHash("YKR DLAA Encode PS"), "DLAA Encode", "Luma DLAA Encode", output, reshade::api::format::r16g16b16a16_float, scene_resource, scene.desc, {}, rgb_only_blend);
         cmd_list->barrier(scene_resource, reshade::api::resource_usage::render_target, scene_read_state);
         cmd_list->barrier(output, reshade::api::resource_usage::shader_resource, reshade::api::resource_usage::unordered_access);
      }
      if (!scene_readable)
      {
         cmd_list->barrier(scene_resource, scene_read_state, scene.state);
      }
      if (encoded)
      {
         device_data.has_drawn_sr = true;
         game_device_data->dlaa_drawn_present.store(present_count, std::memory_order_relaxed);
      }
   }

   // XeGTAO (Luma_YKR_XeGTAO.hlsl) at fx_ssao_test, whose t0 is the scene depth ("depth", "width" x "height") and t1 the prepass normals, both in a
   // state pixel shaders read: copies of both, then prefilter, main pass and two denoisers into "gtao_final", recorded on the game's list. The
   // camera is the main one's (the exe's slot 0, unjittered there). False, with the game's SSAO to run, if anything is missing.
   static bool DrawXeGTAO(CommandListData& cmd_list_data, DeviceData& device_data, YakuzaKiwamiDeviceData* game_device_data, reshade::api::resource depth, reshade::api::format depth_format, uint32_t width, uint32_t height)
   {
      reshade::api::command_list* const cmd_list = cmd_list_data.cmd_list;
      reshade::api::device* const device = cmd_list->get_device();
      const reshade::api::resource normals = {game_device_data->scene_normals.load(std::memory_order_relaxed)};
      if (game_module == 0 || normals.handle == 0)
      {
         return false;
      }
      const reshade::api::resource_desc normals_desc = device->get_resource_desc(normals);
      if (normals_desc.texture.width != width || normals_desc.texture.height != height)
      {
         return false;
      }
      // Row vectors, right-handed, standard Z (see NOTES.md "Exe RE")
      const auto* const projection = reinterpret_cast<const float*>(game_module + camera_projections_rva);
      const float p11 = projection[0];
      const float p22 = projection[5];
      // The game's radius, as the engine wrote it into its constant mirror for this draw (FUN_140864d90, see Luma_YKR_XeGTAO.hlsl)
      const uint8_t* const mirror = GetConstantMirror();
      if (p11 == 0.f || p22 == 0.f || !mirror)
      {
         return false;
      }
      const auto* const c_ps_user = reinterpret_cast<const float*>(mirror + mirror_data_offset + cb9_base_offset);
      const std::array<float, 4> vanilla_radius = {c_ps_user[8 * 4 + 2], c_ps_user[8 * 4 + 3], c_ps_user[10 * 4 + 2], c_ps_user[10 * 4 + 3]};
#if DEVELOPMENT
      static std::atomic_flag vanilla_radius_logged;
      if (!vanilla_radius_logged.test_and_set())
      {
         reshade::log::message(reshade::log::level::info, std::format("[XeGTAO] The game's SSAO radius: max({}, {} * view z), max({}, {} * view z)", vanilla_radius[0], vanilla_radius[1], vanilla_radius[2], vanilla_radius[3]).c_str());
      }
      const float debug_view = float(g_gtao_debug_view);
#else
      const float debug_view = 0.f;
#endif
      // Under a temporal anti-aliasing (DLAA, SMAA T2x) the noise moves every frame for it to average, with one denoise pass (Intel's XeGTAO.h with
      // TAA), and GT-VBAO takes one slice; without one a moving pattern would boil, so it stays frozen and denoises twice
      const bool temporal = (g_gtao_temporal ? g_gtao_temporal == 2 : (game_device_data->dlaa_active.load(std::memory_order_relaxed) || game_device_data->t2x_active.load(std::memory_order_relaxed)));
      const float noise_index = (temporal ? float(device_data.present_count.load(std::memory_order_relaxed) % 64) : 0.f);
      // The power matching the game's SSAO darkening at YKR_GTAO_RADIUS_SCALE (see NOTES.md "Calibration"): XeGTAO 1.3 (an office, one scene);
      // GT-VBAO 1, its strength is its thickness (Luma_YKR_XeGTAO.hlsl). "g_gtao_final_value_power" scales it
      const float final_value_power = g_gtao_final_value_power * (g_gtvbao_enable ? 1.f : 1.3f);
      // "LumaGTAO" in Luma_YKR_XeGTAO.hlsl
      const float constants[] = {1.f / float(width), 1.f / float(height), projection[10], projection[14], 2.f / p11, -2.f / p22, (projection[8] - 1.f) / p11, (1.f + projection[9]) / p22, final_value_power, g_gtao_radius_override, debug_view, noise_index, vanilla_radius[0], vanilla_radius[1], vanilla_radius[2], vanilla_radius[3]};

      auto& pass = game_device_data->gtao_pass;
      const std::unique_lock lock(pass.mutex);
      const reshade::api::sampler_desc point_sampler = {.filter = reshade::api::filter_mode::min_mag_mip_point};
      reshade::api::descriptor_range_with_flags sampler_range;
      sampler_range.count = 1;
      sampler_range.visibility = reshade::api::shader_stage::compute;
      sampler_range.type = reshade::api::descriptor_type::sampler;
      sampler_range.static_samplers = &point_sampler;
      // t0, t1, then u0-u4 (the prefilter writes the 5 depth mips, the others u0)
      const reshade::api::descriptor_range view_ranges[] = {{.count = 2, .visibility = reshade::api::shader_stage::compute, .type = reshade::api::descriptor_type::texture_shader_resource_view}, {.binding = 2, .count = gtao_depth_mip_count, .visibility = reshade::api::shader_stage::compute, .type = reshade::api::descriptor_type::texture_unordered_access_view}};
      const reshade::api::pipeline_layout_param layout_params[] = {reshade::api::constant_range{.count = uint32_t(std::size(constants)), .visibility = reshade::api::shader_stage::compute}, {uint32_t(std::size(view_ranges)), view_ranges}, {1, &sampler_range}};
      constexpr uint32_t dispatch_count = 4;
      const reshade::api::descriptor_table* const tables = PrepareD3D12GamePass(&pass, device_data, device, layout_params, 1, dispatch_count, "XeGTAO");
      if (!tables)
      {
         return false;
      }
      auto* const native_device = reinterpret_cast<ID3D12Device*>(device->get_native());
      const std::array<uint32_t, dispatch_count> shader_hashes = {CompileTimeStringHash("YKR XeGTAO Prefilter Depths CS"), (temporal && g_gtvbao_enable) ? CompileTimeStringHash("YKR GT-VBAO Temporal Main Pass CS") : (g_gtvbao_enable ? CompileTimeStringHash("YKR GT-VBAO Main Pass CS") : CompileTimeStringHash("YKR XeGTAO Main Pass CS")), CompileTimeStringHash("YKR XeGTAO Denoise Pass 1 CS"), CompileTimeStringHash("YKR XeGTAO Denoise Pass 2 CS")};
      std::array<ID3D12PipelineState*, dispatch_count> pipelines = {};
      for (size_t i = 0; i < dispatch_count; i++)
      {
         pipelines[i] = GetD3D12GamePassPipeline(&pass, native_device, 0, shader_hashes[i], DXGI_FORMAT_UNKNOWN, "XeGTAO");
         if (!pipelines[i])
         {
            return false;
         }
      }

      const uint64_t present_count = device_data.present_count.load(std::memory_order_relaxed);
      constexpr reshade::api::resource_usage render_target_and_shader_resource = reshade::api::resource_usage::render_target | reshade::api::resource_usage::shader_resource;
      constexpr reshade::api::resource_usage unordered_access_and_shader_resource = reshade::api::resource_usage::unordered_access | reshade::api::resource_usage::shader_resource;
      constexpr reshade::api::resource_usage at_rest = reshade::api::resource_usage::shader_resource;
      if (!game_device_data->EnsurePassTarget(&pass, &game_device_data->gtao_depth, device, present_count, width, height, reshade::api::format::r32_float, render_target_and_shader_resource, at_rest, L"Luma YKR XeGTAO Depth") ||
          !game_device_data->EnsurePassTarget(&pass, &game_device_data->gtao_normals, device, present_count, width, height, reshade::api::format::r8g8b8a8_unorm, render_target_and_shader_resource, at_rest, L"Luma YKR XeGTAO Normals") ||
          !game_device_data->EnsurePassTarget(&pass, &game_device_data->gtao_depth_mips, device, present_count, width, height, reshade::api::format::r32_float, unordered_access_and_shader_resource, at_rest, L"Luma YKR XeGTAO Depth Mips", nullptr, gtao_depth_mip_count) ||
          !game_device_data->EnsurePassTarget(&pass, &game_device_data->gtao_working[0], device, present_count, width, height, reshade::api::format::r8g8_unorm, unordered_access_and_shader_resource, at_rest, L"Luma YKR XeGTAO Working 0") ||
          !game_device_data->EnsurePassTarget(&pass, &game_device_data->gtao_working[1], device, present_count, width, height, reshade::api::format::r8g8_unorm, unordered_access_and_shader_resource, at_rest, L"Luma YKR XeGTAO Working 1") ||
          !game_device_data->EnsurePassTarget(&pass, &game_device_data->gtao_final, device, present_count, width, height, reshade::api::format::r8_unorm, unordered_access_and_shader_resource, at_rest, L"Luma YKR XeGTAO Final"))
      {
         static std::atomic_flag logged;
         if (!logged.test_and_set())
         {
            reshade::log::message(reshade::log::level::error, std::format("[XeGTAO] ReShade refused its {}x{} textures, so the game's SSAO runs", width, height).c_str());
         }
         return false;
      }

      // The inputs, copied by pixel shaders (the game's depth and normals stay in their state)
      const auto copy = [&](reshade::api::resource source, reshade::api::format source_format, reshade::api::resource target, reshade::api::format target_format, const char* name)
      {
         cmd_list->barrier(target, at_rest, reshade::api::resource_usage::render_target);
         const bool copied = DrawFullscreenPassD3D12(cmd_list_data, device_data, &game_device_data->gtao_copy_pass, CompileTimeStringHash("YKR DLAA Copy PS"), name, name, source, source_format, target, reshade::api::resource_view_desc(reshade::api::resource_view_type::texture_2d, target_format, 0, 1, 0, 1), {});
         cmd_list->barrier(target, reshade::api::resource_usage::render_target, at_rest);
         return copied;
      };
      if (!copy(depth, depth_format, game_device_data->gtao_depth, reshade::api::format::r32_float, "Luma XeGTAO Depth Copy") ||
          !copy(normals, game_device_data->scene_normals_format.load(std::memory_order_relaxed), game_device_data->gtao_normals, reshade::api::format::r8g8b8a8_unorm, "Luma XeGTAO Normals Copy"))
      {
         return false;
      }

      // The views of every dispatch, written into its table (which keeps copies of them)
      std::vector<reshade::api::resource_view> views;
      const auto make_view = [&](reshade::api::resource resource, reshade::api::resource_usage usage, reshade::api::format format, uint32_t first_level, uint32_t levels)
      {
         reshade::api::resource_view view = {};
         device->create_resource_view(resource, usage, reshade::api::resource_view_desc(reshade::api::resource_view_type::texture_2d, format, first_level, levels, 0, 1), &view);
         views.push_back(view);
         return view;
      };
      std::array<reshade::api::resource_view, gtao_depth_mip_count> mip_uavs;
      for (uint32_t mip = 0; mip < gtao_depth_mip_count; mip++)
      {
         mip_uavs[mip] = make_view(game_device_data->gtao_depth_mips, reshade::api::resource_usage::unordered_access, reshade::api::format::r32_float, mip, 1);
      }
      const reshade::api::resource_view depth_srv = make_view(game_device_data->gtao_depth, reshade::api::resource_usage::shader_resource, reshade::api::format::r32_float, 0, 1);
      const reshade::api::resource_view normals_srv = make_view(game_device_data->gtao_normals, reshade::api::resource_usage::shader_resource, reshade::api::format::r8g8b8a8_unorm, 0, 1);
      const reshade::api::resource_view depth_mips_srv = make_view(game_device_data->gtao_depth_mips, reshade::api::resource_usage::shader_resource, reshade::api::format::r32_float, 0, gtao_depth_mip_count);
      const std::array<reshade::api::resource_view, 2> working_srvs = {make_view(game_device_data->gtao_working[0], reshade::api::resource_usage::shader_resource, reshade::api::format::r8g8_unorm, 0, 1), make_view(game_device_data->gtao_working[1], reshade::api::resource_usage::shader_resource, reshade::api::format::r8g8_unorm, 0, 1)};
      const std::array<reshade::api::resource_view, 2> working_uavs = {make_view(game_device_data->gtao_working[0], reshade::api::resource_usage::unordered_access, reshade::api::format::r8g8_unorm, 0, 1), make_view(game_device_data->gtao_working[1], reshade::api::resource_usage::unordered_access, reshade::api::format::r8g8_unorm, 0, 1)};
      const reshade::api::resource_view final_uav = make_view(game_device_data->gtao_final, reshade::api::resource_usage::unordered_access, reshade::api::format::r8_unorm, 0, 1);
      const bool views_made = std::ranges::none_of(views, [](reshade::api::resource_view view)
         { return view.handle == 0; });
      if (views_made)
      {
         // Every slot of a table holds a valid view of a resource in a state it allows; the unused ones repeat the dispatch's output (UAVs) and source (SRVs)
         const std::array<std::array<reshade::api::resource_view, 2>, dispatch_count> srvs = {{{depth_srv, depth_srv}, {depth_mips_srv, normals_srv}, {working_srvs[0], working_srvs[0]}, {working_srvs[temporal ? 0 : 1], working_srvs[temporal ? 0 : 1]}}};
         const std::array<reshade::api::resource_view, dispatch_count> outputs = {mip_uavs[0], working_uavs[0], working_uavs[1], final_uav}; // The prefilter's are all the mips
         for (uint32_t i = 0; i < dispatch_count; i++)
         {
            std::array<reshade::api::resource_view, gtao_depth_mip_count> uavs = mip_uavs;
            if (i != 0)
            {
               uavs.fill(outputs[i]);
            }
            const reshade::api::descriptor_table_update updates[] = {{.table = tables[i], .count = 2, .type = reshade::api::descriptor_type::texture_shader_resource_view, .descriptors = srvs[i].data()}, {.table = tables[i], .binding = 2, .count = gtao_depth_mip_count, .type = reshade::api::descriptor_type::texture_unordered_access_view, .descriptors = uavs.data()}};
            device->update_descriptor_tables(uint32_t(std::size(updates)), updates);
         }
      }
      for (const reshade::api::resource_view view : views)
      {
         if (view.handle != 0)
         {
            device->destroy_resource_view(view);
         }
      }
      if (!views_made)
      {
         return false;
      }

      // Each output goes to unordered access around its dispatch, so the next one reads it complete
      const std::array<reshade::api::resource, dispatch_count> outputs = {game_device_data->gtao_depth_mips, game_device_data->gtao_working[0], game_device_data->gtao_working[1], game_device_data->gtao_final};
      // Prefilter: 16x16 pixels a group; main: 8x8; denoise: 2 pixels a thread, 16x8
      const std::array<std::array<uint32_t, 2>, dispatch_count> groups = {{{(width + 15) / 16, (height + 15) / 16}, {(width + 7) / 8, (height + 7) / 8}, {(width + 15) / 16, (height + 7) / 8}, {(width + 15) / 16, (height + 7) / 8}}};
      for (uint32_t i = 0; i < dispatch_count; i++)
      {
         // The first denoiser is skipped when temporal: the final one reads the main pass' output
         if (temporal && i == 2)
            continue;
         cmd_list->barrier(outputs[i], at_rest, reshade::api::resource_usage::unordered_access);
         cmd_list->bind_pipeline(reshade::api::pipeline_stage::all_compute, reshade::api::pipeline{reinterpret_cast<uint64_t>(pipelines[i])});
         cmd_list->push_constants(reshade::api::shader_stage::compute, pass.layout, 0, 0, uint32_t(std::size(constants)), constants);
         cmd_list->bind_descriptor_tables(reshade::api::shader_stage::compute, pass.layout, 1, 1, &tables[i]);
         cmd_list->dispatch(groups[i][0], groups[i][1], 1);
         cmd_list->barrier(outputs[i], reshade::api::resource_usage::unordered_access, at_rest);
      }
      TraceLumaPass(&cmd_list_data, device_data, "Luma XeGTAO", {{.resource = game_device_data->gtao_depth.handle, .format = reshade::api::format::r32_float}, {.resource = game_device_data->gtao_normals.handle, .format = reshade::api::format::r8g8b8a8_unorm}}, {{.resource = game_device_data->gtao_final.handle, .format = reshade::api::format::r8_unorm}}, true);
      D3D12State::RestoreCompute(cmd_list, cmd_list_data.d3d12_state, {});
      RestoreGameGraphicsD3D12(cmd_list_data);
      return true;
   }

   // Luma Bloom (Luma_YKR_Bloom.hlsl) at glow_pass2, in its place: glow_pass0's level 0 with pass2's weights folded in, Core's DrawBloom of it at
   // glow_pass1's sigma over the 4 vanilla levels, and the composite into pass2's target. The weights are the stage's c_ps_user[0], which the engine
   // wrote into its constant mirror for this draw. False, with the game's draw to run, if anything is missing.
   static bool DrawLumaBloom(CommandListData& cmd_list_data, DeviceData& device_data, YakuzaKiwamiDeviceData* game_device_data)
   {
      const D3D12State::CommandListState& state = cmd_list_data.d3d12_state;
      const reshade::api::resource level0 = {game_device_data->glow_level0.load(std::memory_order_relaxed)};
      const uint8_t* const mirror = GetConstantMirror();
      if (state.in_render_pass || !mirror || level0.handle == 0 || state.render_target_views.empty() || state.render_target_descs.empty() || state.render_target_views[0].resource == level0.handle)
      {
         return false;
      }
      reshade::api::command_list* const cmd_list = cmd_list_data.cmd_list;
      reshade::api::device* const device = cmd_list->get_device();
      const reshade::api::resource_desc level0_desc = device->get_resource_desc(level0);
      const auto* const weights = reinterpret_cast<const float*>(mirror + mirror_data_offset + cb9_base_offset);
      const float weight_scale = (std::max)({weights[0], weights[1], weights[2], weights[3], 1e-4f});
      // "GlowConstants" after the target's size
      const float constants[] = {weight_scale, g_bloom_intensity, weights[0], weights[1], weights[2], weights[3]};

      reshade::api::resource input = {};
      {
         const std::unique_lock lock(game_device_data->bloom_fold_pass.mutex);
         if (game_device_data->EnsurePassTarget(&game_device_data->bloom_fold_pass, &game_device_data->bloom_input, device, device_data.present_count.load(std::memory_order_relaxed), level0_desc.texture.width, level0_desc.texture.height, reshade::api::format::r16g16b16a16_float, reshade::api::resource_usage::render_target | reshade::api::resource_usage::shader_resource, reshade::api::resource_usage::shader_resource, L"Luma YKR Bloom Input"))
         {
            input = game_device_data->bloom_input;
         }
      }
      if (input.handle == 0)
      {
         return false;
      }
      // Level 0 is a pixel shader resource at this draw: pass2 reads it
      cmd_list->barrier(input, reshade::api::resource_usage::shader_resource, reshade::api::resource_usage::render_target);
      const bool folded = DrawFullscreenPassD3D12(cmd_list_data, device_data, &game_device_data->bloom_fold_pass, CompileTimeStringHash("YKR Bloom Fold PS"), "Bloom Fold", "Luma Bloom Fold", level0, game_device_data->glow_level0_format.load(std::memory_order_relaxed), input, reshade::api::resource_view_desc(reshade::api::resource_view_type::texture_2d, reshade::api::format::r16g16b16a16_float, 0, 1, 0, 1), constants);
      cmd_list->barrier(input, reshade::api::resource_usage::render_target, reshade::api::resource_usage::shader_resource);
      if (!folded)
      {
         return false;
      }
      std::array<float, bloom_mip_count> sigmas;
      sigmas.fill(game_device_data->glow_sigma.load(std::memory_order_relaxed));
      reshade::api::resource bloom = {};
      if (!DrawBloom(cmd_list_data, device_data, input, reshade::api::format::r16g16b16a16_float, bloom_mip_count, sigmas.data(), &bloom))
      {
         return false;
      }
      const D3D12FullscreenSource sources[] = {{.resource = input, .format = reshade::api::format::r16g16b16a16_float}, {.resource = bloom, .format = reshade::api::format::r16g16b16a16_float, .levels = bloom_mip_count}};
      return DrawFullscreenPassD3D12(cmd_list_data, device_data, &game_device_data->bloom_composite_pass, CompileTimeStringHash("YKR Bloom Composite PS"), "Bloom Composite", "Luma Bloom Composite", sources, {state.render_target_views[0].resource}, state.render_target_descs[0], constants);
   }

   void OnPresent(NativeDevice* native_device, DeviceData& device_data) override
   {
      auto& game_device_data = static_cast<YakuzaKiwamiDeviceData&>(*device_data.game);
      if (!std::exchange(projection_hook_attempted, true))
      {
         InstallProjectionHook();
      }

      // DLAA and its jitter for the next frame: DLSS' history restarts after a frame it didn't draw (menus, loading, just picked)
      const bool dlaa_active = LatchSRFrame(device_data) && set_projection_original;
      game_device_data.dlaa_active.store(dlaa_active, std::memory_order_relaxed);
      // SMAA T2x needs the motion vectors and the jitter too, which DLAA's pass and the projection hook give it
      const bool t2x_active = g_smaa_enable && g_smaa_t2x && !dlaa_active && set_projection_original;
      const bool motion_active = dlaa_active || t2x_active;
      game_device_data.t2x_active.store(t2x_active, std::memory_order_relaxed);
      const uint32_t width = game_device_data.depth_width.load(std::memory_order_relaxed);
      const uint32_t height = game_device_data.depth_height.load(std::memory_order_relaxed);
      // The jitter follows DLAA's draws: in a frame it can't draw (no depth copy, a menu without a scene), FXAA would show it
      const uint64_t present_count = device_data.present_count.load(std::memory_order_relaxed);
      const bool dlaa_drawn = game_device_data.dlaa_drawn_present.load(std::memory_order_relaxed) == present_count;
      const bool t2x_drawn = game_device_data.t2x_drawn_present.load(std::memory_order_relaxed) == present_count;

      // The textures of a mode that stopped drawing go after "D3D12_GAME_PASS_IDLE_PRESENTS" (DLAA, SMAA, T2x picked in turn), and what the
      // passes retired once no list can read it: their draws only destroy it while they keep drawing
      const auto idle = [present_count](uint64_t last_present)
      {
         return last_present != UINT64_MAX && present_count - last_present > D3D12_GAME_PASS_IDLE_PRESENTS;
      };
      if (!motion_active && idle(game_device_data.motion_vectors_present.load(std::memory_order_relaxed)))
      {
         CleanExtraSRResources(device_data);
         game_device_data.motion_vectors_present.store(UINT64_MAX, std::memory_order_relaxed);
      }
      if (!t2x_active && idle(game_device_data.t2x_drawn_present.load(std::memory_order_relaxed)))
      {
         const std::unique_lock lock(game_device_data.t2x_pass.mutex);
         for (reshade::api::resource& frame : game_device_data.t2x_frames)
         {
            RetireD3D12GamePassResource(&game_device_data.t2x_pass, present_count, &frame);
         }
         game_device_data.t2x_drawn_present.store(UINT64_MAX, std::memory_order_relaxed);
      }
      {
         const std::unique_lock lock(game_device_data.predication_pass.mutex);
         if (idle(game_device_data.predication_present))
         {
            RetireD3D12GamePassResource(&game_device_data.predication_pass, present_count, &game_device_data.predication);
            game_device_data.predication_present = UINT64_MAX;
         }
      }
      if (idle(game_device_data.gtao_present.load(std::memory_order_relaxed)))
      {
         const std::unique_lock lock(game_device_data.gtao_pass.mutex);
         for (reshade::api::resource* const texture : {&game_device_data.gtao_depth, &game_device_data.gtao_normals, &game_device_data.gtao_depth_mips, &game_device_data.gtao_working[0], &game_device_data.gtao_working[1], &game_device_data.gtao_final})
         {
            RetireD3D12GamePassResource(&game_device_data.gtao_pass, present_count, texture);
         }
         game_device_data.gtao_present.store(UINT64_MAX, std::memory_order_relaxed);
      }
      if (idle(game_device_data.bloom_drawn_present.load(std::memory_order_relaxed)))
      {
         const std::unique_lock lock(game_device_data.bloom_fold_pass.mutex);
         RetireD3D12GamePassResource(&game_device_data.bloom_fold_pass, present_count, &game_device_data.bloom_input);
         game_device_data.bloom_drawn_present.store(UINT64_MAX, std::memory_order_relaxed);
      }
      for (DeviceData::GamePassD3D12* const pass : {&game_device_data.predication_pass, &game_device_data.copy_pass, &game_device_data.motion_vector_pass, &game_device_data.color_pass, &game_device_data.t2x_pass, &game_device_data.gtao_pass, &game_device_data.bloom_fold_pass})
      {
         const std::unique_lock lock(pass->mutex);
         DestroyRetiredD3D12GamePassResources(pass, device_data.reshade_device, present_count);
      }
      {
         const std::unique_lock lock(camera_state.mutex);
         camera_state.motion_active = motion_active;
         const bool jittered = (dlaa_active && dlaa_drawn) || (t2x_active && t2x_drawn);
         camera_state.jitter_pixels = {};
         if (jittered && width != 0 && height != 0)
         {
            if (t2x_active)
            {
               // Two phases, a quarter pixel diagonally
               const float t2x_jitter = ((present_count & 1) ? 0.25f : -0.25f);
               camera_state.jitter_pixels = {t2x_jitter, t2x_jitter};
            }
            else
            {
               camera_state.jitter_pixels = GetSRJitter(device_data, device_data.GetSRInstanceData());
            }
         }
         camera_state.jitter_ndc = {camera_state.jitter_pixels[0] * 2.f / float((std::max)(width, 1u)), camera_state.jitter_pixels[1] * -2.f / float((std::max)(height, 1u))};
         if (!motion_active)
         {
            camera_state.capture.reset();
         }
      }
      // The upscaler's bias at native resolution (-1 for DLSS and FSR, 0 for XeSS), with DLAA only (the game has no TAA: a negative bias would shimmer)
      const SR::InstanceData* const sr_instance_data = device_data.GetSRInstanceData();
      SetTextureMipLodBias(nullptr, device_data, ((dlaa_active && g_dlaa_mip_bias && sr_instance_data) ? sr_implementations[device_data.sr_type]->GetMipLODBias(sr_instance_data) : 0.f));

#if DEVELOPMENT
      for (size_t i = 0; i < MV_COUNT; i++)
      {
         object_motion_counts_frame[i] = std::atomic_ref(object_motion_counts[i]).exchange(0, std::memory_order_relaxed);
      }
      // "MV Debug View": Core's debug draw of the merged motion vectors (UV deltas), absolute, in pixels
      if (g_mv_debug_view)
      {
         debug_draw_options |= uint32_t(DebugDrawTextureOptionsMask::Abs) | uint32_t(DebugDrawTextureOptionsMask::UVToPixelSpace);
         debug_draw_registered_texture = mv_debug_view_texture;
      }
      else if (debug_draw_registered_texture == mv_debug_view_texture)
      {
         debug_draw_registered_texture.clear();
      }
#endif
   }

   // DLAA was turned off: its textures go once no list can read them anymore
   void CleanExtraSRResources(DeviceData& device_data) override
   {
      auto& game_device_data = static_cast<YakuzaKiwamiDeviceData&>(*device_data.game);
      const uint64_t present_count = device_data.present_count.load(std::memory_order_relaxed);
      {
         const std::unique_lock lock(game_device_data.copy_pass.mutex);
         RetireD3D12GamePassResource(&game_device_data.copy_pass, present_count, &game_device_data.depth_copy);
      }
      {
         const std::unique_lock lock(game_device_data.motion_vector_pass.mutex);
         RetireD3D12GamePassResource(&game_device_data.motion_vector_pass, present_count, &game_device_data.motion_vectors);
         RetireD3D12GamePassResource(&game_device_data.motion_vector_pass, present_count, &game_device_data.object_motion_vectors);
         game_device_data.object_motion_vectors_handle.store(0, std::memory_order_relaxed);
         game_device_data.object_motion_vectors_ready_present.store(UINT64_MAX, std::memory_order_relaxed);
      }
      // The position caches go with their history; the descriptor heaps are small and stay
      std::array<reshade::api::resource, 2> caches = {};
      {
         const std::unique_lock lock(game_device_data.object_motion.mutex);
         caches = std::exchange(game_device_data.object_motion.caches, {});
         game_device_data.object_motion.stamp = UINT64_MAX;
         game_device_data.object_motion.caches_failed = false;
      }
      {
         const std::unique_lock lock(game_device_data.motion_vector_pass.mutex);
         for (reshade::api::resource& cache : caches)
         {
            RetireD3D12GamePassResource(&game_device_data.motion_vector_pass, present_count, &cache);
         }
      }
      {
         const std::unique_lock lock(game_device_data.color_pass.mutex);
         RetireD3D12GamePassResource(&game_device_data.color_pass, present_count, &game_device_data.decoded_scene);
         RetireD3D12GamePassResource(&game_device_data.color_pass, present_count, &game_device_data.dlaa_output);
         game_device_data.dlaa_output_recreated = false;
      }
   }

   void LoadConfigs() override
   {
      auto& gs = cb_luma_global_settings.GameSettings;
      reshade::get_config_value(nullptr, NAME, "VideoAutoHDREnable", gs.VideoAutoHDREnable);
      reshade::get_config_value(nullptr, NAME, "VideoAutoHDRBoost", gs.VideoAutoHDRBoost);
      reshade::get_config_value(nullptr, NAME, "Dithering", gs.Dithering);
      reshade::get_config_value(nullptr, NAME, "Exposure", gs.Exposure);
      reshade::get_config_value(nullptr, NAME, "Contrast", gs.Contrast);
      reshade::get_config_value(nullptr, NAME, "Saturation", gs.Saturation);
      reshade::get_config_value(nullptr, NAME, "HighlightDechroma", gs.HighlightDechroma);
      reshade::get_config_value(nullptr, NAME, "SMAAEnable", g_smaa_enable);
#if DEVELOPMENT
      // Its control is DEVELOPMENT only: a saved "off" must not outlive it
      reshade::get_config_value(nullptr, NAME, "SMAAPredication", g_smaa_predication);
#endif
      reshade::get_config_value(nullptr, NAME, "SMAAT2x", g_smaa_t2x);
      reshade::get_config_value(nullptr, NAME, "RCASSharpness", g_rcas_sharpness);
      reshade::get_config_value(nullptr, NAME, "BloomIntensity", g_bloom_intensity);
      reshade::get_config_value(nullptr, NAME, "GTAOEnable", g_gtao_enable);
      reshade::get_config_value(nullptr, NAME, "GTVBAOEnable", g_gtvbao_enable);
   }

   void DrawImGuiSettings(DeviceData& device_data) override
   {
      auto& gs = cb_luma_global_settings.GameSettings;

      ImGui::SeparatorText("Anti-Aliasing");

      // DLAA takes FXAA's place itself
      const bool sr_active = device_data.sr_type != SR::Type::None;
      DrawSettingCheckbox("SMAA Enable", "SMAAEnable", &g_smaa_enable, true, "Replaces the game's FXAA with SMAA (requires Anti-Aliasing enabled in the game's graphics settings; not used with DLSS, FSR or XeSS).", sr_active);
      DrawSettingCheckbox("SMAA T2x", "SMAAT2x", &g_smaa_t2x, false, "Smoother edges and less shimmering, by blending each frame with the previous one.\nCan look slightly softer in motion. Not used with DLSS, FSR or XeSS.", sr_active || !g_smaa_enable);
      // RCAS sharpens SMAA's, T2x's and the upscaler's output, never the game's FXAA
      ImGui::BeginDisabled(!sr_active && !g_smaa_enable);
      DrawSettingSlider("RCAS Sharpness", "RCASSharpness", &g_rcas_sharpness, 0.f, 0.f, 1.f, "Sharpening applied on top of SMAA, SMAA T2x, DLSS, FSR or XeSS (0 = off; requires Anti-Aliasing enabled in the game's graphics settings).");
      ImGui::EndDisabled();
#if DEVELOPMENT
      if (g_smaa_enable)
      {
         DrawSettingCheckbox("SMAA Predication", "SMAAPredication", &g_smaa_predication, true, "Finds edges by geometry (scene depth) instead of by brightness alone (needs SSAO on in the game's graphics settings).");
      }
#endif

      // The grade sliders act on the HDR tonemap only.
      if (cb_luma_global_settings.DisplayMode != DisplayModeType::SDR)
      {
         ImGui::SeparatorText("Grade");

         DrawSettingSlider("Exposure", "Exposure", &gs.Exposure, default_luma_global_game_settings.Exposure, 0.f, 2.f, "Overall image brightness (1 = vanilla).");

         DrawSettingSlider("Contrast", "Contrast", &gs.Contrast, default_luma_global_game_settings.Contrast, 0.f, 2.f, "Overall image contrast, HDR only (1 = vanilla).");

         DrawSettingSlider("Saturation", "Saturation", &gs.Saturation, default_luma_global_game_settings.Saturation, 0.f, 2.f, "Color saturation, HDR only (1 = vanilla).");

         DrawSettingSlider("Highlights Desaturation", "HighlightDechroma", &gs.HighlightDechroma, default_luma_global_game_settings.HighlightDechroma, 0.f, 1.f, "How far the brightest sources fade to neutral white, HDR only (0 = keep color at any brightness).");
      }

      ImGui::SeparatorText("Bloom");

      // Off the canon "wider, softer HDR bloom": its width follows the game's glow and it stays within its 8-bit target
      DrawSettingSlider("Bloom Intensity", "BloomIntensity", &g_bloom_intensity, 1.f, 0.f, 2.f, "Bloom strength (1 = vanilla, 0 = none).");

      ImGui::SeparatorText("Ambient Occlusion");

      DrawSettingCheckbox("XeGTAO Enable", "GTAOEnable", &g_gtao_enable, true, "Replaces the game's SSAO with XeGTAO (cleaner, more accurate ambient occlusion; requires SSAO enabled in the game's graphics settings).");
      DrawSettingCheckbox("GT-VBAO Enable", "GTVBAOEnable", &g_gtvbao_enable, false, "Replaces XeGTAO's occlusion with GT-VBAO (less dark halo around thin objects). Needs XeGTAO Enable.", !g_gtao_enable, false);
#if DEVELOPMENT || TEST
      ImGui::BeginDisabled(!g_gtao_enable);
      ImGui::SliderFloat("GTAO Final Value Power", &g_gtao_final_value_power, 0.3f, 4.5f, "%.2f");
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
      {
         ImGui::SetTooltip("Primary darkness dial (higher = darker AO).");
      }
      ImGui::SliderFloat("GTAO Radius Override", &g_gtao_radius_override, 0.f, 5.f, "%.3f");
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
      {
         ImGui::SetTooltip("0 = the game's own SSAO radius; > 0 overrides it, in meters.");
      }
#if DEVELOPMENT // the shader's debug blocks exist in DEVELOPMENT only
      ImGui::Combo("GTAO Debug View", &g_gtao_debug_view, "Off\0Depth gradient\0Normals\0AO x8\0Edges\0");
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
      {
         ImGui::SetTooltip("Draws diagnostics into the AO the materials multiply their ambient light by. Depth gradient flat or blocky = wrong input;\nNormals: camera-facing surfaces bright; AO x8 = spot broad over-occlusion.");
      }
      ImGui::Combo("GTAO Temporal Noise", &g_gtao_temporal, "Auto (DLAA/SMAA T2x)\0Off (frozen, 2 denoise passes)\0On (frame % 64, 1 denoise pass)\0");
#endif
      ImGui::EndDisabled();
#endif

      ImGui::SeparatorText("Effects");

      DrawSettingCheckbox("Video AutoHDR", "VideoAutoHDREnable", &gs.VideoAutoHDREnable, default_luma_global_game_settings.VideoAutoHDREnable, "Adds HDR highlights to pre-rendered videos (HDR only).");

      ImGui::BeginDisabled(gs.VideoAutoHDREnable < 0.5f);
      DrawSettingSlider("Video HDR Boost", "VideoAutoHDRBoost", &gs.VideoAutoHDRBoost, default_luma_global_game_settings.VideoAutoHDRBoost, 0.f, 1.f, "Video highlight strength (0 = off).");
      ImGui::EndDisabled();

      DrawSettingCheckbox("Dithering", "Dithering", &gs.Dithering, default_luma_global_game_settings.Dithering, "Reduces gradient banding.");

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
      ImGui::SeparatorText("Motion vector research");
      ImGui::Checkbox("MV Object Motion", &g_object_motion_vectors);
      if (ImGui::IsItemHovered())
      {
         ImGui::SetTooltip("The characters' and hair's prepass draws write their own motion (patched shaders); off: camera motion only.\nThe debug view lights up only what moves with a static camera. Not saved.");
      }
      ImGui::Checkbox("MV Rigid Objects", &g_object_mv_rigid);
      if (ImGui::IsItemHovered())
      {
         ImGui::SetTooltip("Matches the rigid world's draws with last frame's on their world matrix: an unchanged one is static (camera motion), the\nothers moved. Not saved.");
      }
      ImGui::Checkbox("MV Debug View", &g_mv_debug_view);
      if (ImGui::IsItemHovered())
      {
         ImGui::SetTooltip("Shows DLAA's motion vectors (absolute, in pixels) through Core's debug draw.");
      }
      ImGui::Text("Last frame: %u patched draws (%u matched, %u new, %u overflowed)", object_motion_counts_frame[MV_PATCHED], object_motion_counts_frame[MV_MATCHED], object_motion_counts_frame[MV_NEW], object_motion_counts_frame[MV_OVERFLOW]);
      ImGui::Text("Rigid draws: %u static, %u moving, %u new", object_motion_counts_frame[MV_RIGID_STATIC], object_motion_counts_frame[MV_RIGID_MOVING], object_motion_counts_frame[MV_RIGID_NEW]);
      std::string refused = "Refused:";
      for (size_t i = MV_REFUSED_BUNDLE; i < MV_COUNT; i++)
      {
         if (object_motion_counts_frame[i] != 0)
         {
            refused += std::format(" {} {}", object_motion_counts_frame[i], object_motion_count_names[i] + std::strlen("mv.refused."));
         }
      }
      ImGui::TextUnformatted(refused.c_str());
   }
#endif

   void PrintImGuiAbout() override
   {
      ImGui::PushTextWrapPos(0.f);
      ImGui::Text(
         "Luma for \"Yakuza Kiwami\" is developed by DristoforColumb and is open source and free.\n"
         "It adds HDR and DLAA, FSR 3/4 or XeSS native anti-aliasing and replaces the game's FXAA with SMAA (with T2x), its bloom with a smoother one and its SSAO with XeGTAO, plus 16x anisotropic filtering.\n"
         "Enable Anti-Aliasing and Ambient Occlusion in the game's graphics settings for SMAA and XeGTAO to apply.\n"
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
                  "\nMacLeod-Boynton hue emulation (RenoDX)"
                  "\nSMAA (Iryoku)"
                  "\nXeGTAO (Intel)"
                  "\nGT-VBAO (Mirko Salm)"
                  "\nAMD FidelityFX (RCAS + FSR 3/4)"
                  "\nNVIDIA NGX (DLSS)"
                  "\nIntel XeSS");
   }
};

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
   if (ul_reason_for_call == DLL_PROCESS_ATTACH)
   {
      Globals::SetGlobals(PROJECT_NAME, "Yakuza Kiwami Luma mod", "", 1);

      swapchain_format_upgrade_type = TextureFormatUpgradesType::AllowedEnabled;
      swapchain_upgrade_type = SwapchainUpgradeType::scRGB; // B8G8R8A8 back buffer -> R16G16B16A16 FLOAT
      texture_format_upgrades_type = TextureFormatUpgradesType::AllowedEnabled;
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

         reshade::api::format::r10g10b10a2_unorm,
         reshade::api::format::r10g10b10a2_typeless,

         reshade::api::format::r11g11b10_float,
      };
      texture_format_upgrades_2d_size_filters = 0 | (uint32_t)TextureFormatUpgrades2DSizeFilters::SwapchainResolution | (uint32_t)TextureFormatUpgrades2DSizeFilters::SwapchainAspectRatio | (uint32_t)TextureFormatUpgrades2DSizeFilters::No1Px;

      // DLAA's mip bias (see "OnPresent()"), on the anisotropic samplers, which also get 16x
      enable_samplers_upgrade = true;

      game = new YakuzaKiwami();
   }
   else if (ul_reason_for_call == DLL_PROCESS_DETACH)
   {
      if (set_projection_original)
      {
         // ponytail: the relay and the stub stay (a thread may still be in them), 64 bytes
         uint8_t* const set_projection = reinterpret_cast<uint8_t*>(game_module + set_projection_rva);
         uint8_t patched[16];
         std::copy_n(set_projection, 16, patched);
         WriteCodeAtomically(set_projection, patched, set_projection_prologue);
      }
   }

   CoreMain(hModule, ul_reason_for_call, lpReserved);

   return TRUE;
}
