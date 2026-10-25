#define GAME_YAKUZA_KIWAMI 1

#define DISABLE_AUTO_DEBUGGER 1

// The engine's hardware probe creates a D3D11 device before the D3D12 ones: skip it
#define CHECK_GRAPHICS_API_COMPATIBILITY 1

// SMAA replaces the game's FXAA (see "OnDrawOrDispatch()"), RCAS sharpens its output
#define ENABLE_SMAA 1
#define ENABLE_RCAS 1
// SMAA's area texture without the U-shape smoothing (see Luma_SMAA_impl.hlsl)
#define SMAA_SMOOTH_U_SHAPES 0

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
   float g_rcas_sharpness = 0.f; // RCAS on SMAA's output (after T2x's resolve), 0 = off
   bool g_hide_ui = false;       // Session only, so a restart always has a HUD
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
   constexpr uint32_t color_correct_phase = 0x17;
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
   // The engine's root signature (FUN_140ba26f0): the VS' CBV b0-b13 + SRV t0-t23 table is root parameter 2, so b10 is its descriptor 10
   constexpr uint32_t vs_descriptor_table_root_parameter = 2;
   constexpr uint32_t bone_cbv_binding = 10;
   constexpr size_t world_matrix_floats = 12; // 3x4, row-major, translations in floats 3, 7, 11
   constexpr uint64_t world_matrix_size = world_matrix_floats * sizeof(float);
   constexpr uint32_t cb12_binding = 12;

   // Whether the camera jitters and has its motion vectors made (DLAA or SMAA T2x) and the jitter of the main camera's projections (0 in a frame
   // after one they didn't draw), latched at present for the next frame, and the main camera as its passes set it
   struct CameraState
   {
      std::shared_mutex mutex;
      bool motion_active = false;
      std::array<float, 2> jitter_pixels = {}; // As applied: +y down
      std::array<float, 2> jitter_ndc = {};
      // The last unjittered view and projection of the slot DLAA reprojects with ("dlaa_camera_slot"), and the jitter they got
      std::array<float, 16> view = {};
      std::array<float, 16> projection = {};
      std::array<float, 2> applied_jitter_pixels = {};
      std::array<float, 2> applied_jitter_ndc = {};
      bool captured = false;
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

   // DLAA writes the scene's color back, its alpha stays (some ccr permutations read it)
   constexpr D3D12_RENDER_TARGET_BLEND_DESC rgb_only_blend = {.SrcBlend = D3D12_BLEND_ONE, .DestBlend = D3D12_BLEND_ZERO, .BlendOp = D3D12_BLEND_OP_ADD, .SrcBlendAlpha = D3D12_BLEND_ONE, .DestBlendAlpha = D3D12_BLEND_ZERO, .BlendOpAlpha = D3D12_BLEND_OP_ADD, .LogicOp = D3D12_LOGIC_OP_NOOP, .RenderTargetWriteMask = UINT8(D3D12_COLOR_WRITE_ENABLE_RED | D3D12_COLOR_WRITE_ENABLE_GREEN | D3D12_COLOR_WRITE_ENABLE_BLUE)};

   PUBLISHING_CONSTEXPR int g_dlaa_camera_slot = 0;
   // How far DLAA goes, to isolate a pass: -1 nothing (the jitter hook and the mip bias only), 0 the depth copy and the motion vectors, 1 the
   // decoded scene too, 2 DLSS too (its output not written back), 3 all
   PUBLISHING_CONSTEXPR int g_dlaa_stages = 3;
   PUBLISHING_CONSTEXPR bool g_dlaa_mip_bias = true;
   PUBLISHING_CONSTEXPR bool g_mv_force_jitter = false;   // The projection jitter without DLAA (FXAA/SMAA show it)
   PUBLISHING_CONSTEXPR bool g_mv_disable_jitter = false; // No projection jitter under DLAA (DLSS gets zero jitter): A/B of jitter-dependent artifacts
   PUBLISHING_CONSTEXPR bool g_smaa_t2x_flip = false;     // Swaps T2x's subsample indices (the sign of the jitter against the area texture)
   PUBLISHING_CONSTEXPR int g_dlaa_jitter_flip = 0;       // Bit 0: x, bit 1: y, of the jitter given to DLSS
   PUBLISHING_CONSTEXPR int g_dlaa_motion_flip = 0;       // Bit 0: x, bit 1: y, of the motion vectors
#if DEVELOPMENT
   bool g_mv_debug_view = false;
   // The per draw checks of the object motion vectors' assumptions (the anchor and cb12 against the bound CBVs, the views, the list stamps):
   // they read the shadowed descriptors and the constant ring on every prepass draw
   bool g_mv_live_checks = false;
   // One shot: logs the next "object_mv_runs_to_log" patched skinned draws and their runs (matches, slots), then disarms
   bool g_mv_log_runs = false;
   constexpr uint32_t object_mv_runs_to_log = 400;
   std::atomic<uint32_t> object_mv_runs_logged = 0;
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
#if DEVELOPMENT
   // The projection setter's calls by the slot passed (4: any other matrix): being counted, and the last frame's
   std::array<uint32_t, 5> projection_sets = {};
   std::array<uint32_t, 5> projection_sets_frame = {};
   uint32_t dlaa_scene_state = 0; // The scene's "reshade::api::resource_usage" at the last DLAA draw
   // The draw world matrix reads (the anchor, see "OnDrawOrDispatch()") by outcome, being counted, and the last frame's: read, unavailable, compared with the
   // bound CBV's first 3x4 ("checked"), and different from it. "anchor_mismatch_total" counts them all along
   enum AnchorCount : size_t
   {
      ANCHOR_READS,
      ANCHOR_UNAVAILABLE,
      ANCHOR_CHECKED,
      ANCHOR_MISMATCH,
      ANCHOR_COUNT,
   };
   std::array<uint32_t, ANCHOR_COUNT> anchor_counts = {};
   std::array<uint32_t, ANCHOR_COUNT> anchor_counts_frame = {};
   uint32_t anchor_mismatch_total = 0;
   std::atomic_flag anchor_mismatch_logged;

   // The assumptions the object motion vectors (at the draws of "shader_hashes_object_prepass") and the unjittered phases after the
   // anti-aliasing rest on (see "OnDrawOrDispatch()"), being counted, and the last frame's
   enum LiveCheck : size_t
   {
      CB12_CHECKED,           // The mirror's cb12 compared with the bound VS b12
      CB12_UNAVAILABLE,       // ... but the mirror or the bound CBV couldn't be read
      CB12_MISMATCH,          // ... and they differ
      VIEWS_CHECKED,          // The bound render target 0 and depth stencil handles read again now (one per view)
      VIEWS_CHANGED,          // ... and they describe something else than at bind time
      PREPASS_DRAWS,          // Draws of the patched pairs' vertex shaders
      PREPASS_INSTANCED,      // ... with an instance count other than 1
      PREPASS_RT_COUNT_NOT_1, // ... with other than one render target
      PREPASS_OTHER_DSV,      // ... with a depth stencil that isn't the scene depth
      EXEC_LISTS,             // Executed command lists that had such a draw (or the DLAA ccr draw)
      EXEC_STAMP_MISMATCH,    // ... executed in another frame than they were recorded in
      EXEC_STAMP_MIXED,       // Lists recorded across a present
      PROJECTION_POST_AA,     // Projection setter calls left unjittered, in a phase after the anti-aliasing (see "SetProjectionDetour()")
      POST_AA_DRAWS,          // Draws in those phases with a constant mirror
      POST_AA_JITTERED,       // ... whose cb12 projection still has a jitter (_31 or _32 not 0: the projections are symmetric)
      PROJECTION_MAIN_COPY,   // Setter calls with a copy of the main camera's projection, jittered as slot 0
      CCR_OTHER_PHASE,        // Color correct draws outside the post effects phase
      SCREEN_EFFECT_DRAWS,    // Draws inside a screen effect node's draw (see "screen_effect_nodes")
      VP_RECOMPOSE_CHECKED,   // Frames whose cb12 VP was compared with its P x V (see "ObjectMotionState::BeginFrame()")
      VP_RECOMPOSE_MISMATCH,  // ... and they differ
      LIVE_CHECK_COUNT,
   };
   std::array<uint32_t, LIVE_CHECK_COUNT> live_checks = {};
   std::array<uint32_t, LIVE_CHECK_COUNT> live_checks_frame = {};
   std::atomic_flag cb12_mismatch_logged;
   std::atomic_flag views_changed_logged;
   std::atomic_flag stamp_mismatch_logged;
   std::atomic_flag post_aa_jittered_logged;
   std::atomic_flag vp_recompose_mismatch_logged;

   void CountLiveCheck(LiveCheck check)
   {
      std::atomic_ref(live_checks[check]).fetch_add(1, std::memory_order_relaxed);
   }

   // The largest |_31| and |_32| the projection setter was given for the jittered slots since the last present, as float bits (non negative
   // floats order like their bits), and the last frame's. Both 0 if the projections are symmetric
   std::array<uint32_t, 2> projection_max_abs_bits = {};
   std::array<float, 2> projection_max_abs_frame = {};

   // Every command list with a patched pair's draw or the DLAA ccr draw, by the present count it was recorded in, until it runs or is reset
   struct
   {
      std::shared_mutex mutex;
      std::unordered_map<const void*, uint64_t> stamps;
   } list_stamps;

   void RecordListStamp(const CommandListData& cmd_list_data, const DeviceData& device_data)
   {
      constexpr size_t max_lists = 4096; // Bundles never reach the execute event: they aren't recorded
      if (cmd_list_data.is_bundle)
      {
         return;
      }
      const uint64_t present_count = device_data.present_count.load(std::memory_order_relaxed);
      const std::unique_lock lock(list_stamps.mutex);
      if (const auto it = list_stamps.stamps.find(cmd_list_data.cmd_list); it != list_stamps.stamps.end())
      {
         if (it->second != present_count)
         {
            CountLiveCheck(EXEC_STAMP_MIXED);
            it->second = present_count;
         }
      }
      else if (list_stamps.stamps.size() < max_lists)
      {
         list_stamps.stamps.emplace(cmd_list_data.cmd_list, present_count);
      }
   }

   // ReShade fires it before the native "ExecuteCommandLists()", on the thread that executes
   void OnExecuteStampedCommandList(reshade::api::command_queue* queue, reshade::api::command_list* cmd_list)
   {
      uint64_t recorded_present_count = 0;
      {
         const std::unique_lock lock(list_stamps.mutex);
         const auto it = list_stamps.stamps.find(cmd_list);
         if (it == list_stamps.stamps.end())
         {
            return;
         }
         recorded_present_count = it->second;
         list_stamps.stamps.erase(it);
      }
      const uint64_t present_count = cmd_list->get_device()->get_private_data<DeviceData>()->present_count.load(std::memory_order_relaxed);
      CountLiveCheck(EXEC_LISTS);
      if (recorded_present_count != present_count)
      {
         CountLiveCheck(EXEC_STAMP_MISMATCH);
         if (!stamp_mismatch_logged.test_and_set())
         {
            reshade::log::message(reshade::log::level::warning, std::format("[Exec] First list executed in another frame than it was recorded in: recorded at present {}, executed at present {}", recorded_present_count, present_count).c_str());
         }
      }
   }

   // A list reset or destroyed before it ran never executes its recorded draws
   void OnForgetCommandList(reshade::api::command_list* cmd_list)
   {
      const std::unique_lock lock(list_stamps.mutex);
      list_stamps.stamps.erase(cmd_list);
   }
#endif

   void SetProjectionDetour(void* context, const float* projection)
   {
      const uintptr_t offset = reinterpret_cast<uintptr_t>(projection) - (game_module + camera_projections_rva);
      const uint32_t slot = (offset % camera_matrix_size == 0 && offset / camera_matrix_size < 4) ? uint32_t(offset / camera_matrix_size) : 4;
#if DEVELOPMENT
      std::atomic_ref(projection_sets[slot]).fetch_add(1, std::memory_order_relaxed);
#endif
      // A draw that sets its own copy of the main camera's projection (the main menu's characters' forward pass) depth tests against the
      // prepass, which had the jitter: it needs it too, or whole triangles fail the test. ponytail: a copy read while the engine writes slot 0
      // can tear, that call then goes unjittered
      const bool main_camera_copy = (slot == 4 && std::memcmp(projection, reinterpret_cast<const void*>(game_module + camera_projections_rva), camera_matrix_size) == 0);
#if DEVELOPMENT
      if (main_camera_copy)
      {
         CountLiveCheck(PROJECTION_MAIN_COPY);
      }
#endif
      if (slot == 1 || (slot == 4 && !main_camera_copy))
      {
         set_projection_original(context, projection);
         return;
      }
      // The phases after the anti-aliasing draw on the anti-aliased image: their world-space draws (the HUD markers, whatever their shaders)
      // get the main camera unjittered. Only this setter call adds the jitter, so a draw setting its own projection never had it
      if (const uint32_t phase = GetFramePhase(); phase >= first_post_anti_aliasing_phase && phase <= last_post_anti_aliasing_phase)
      {
#if DEVELOPMENT
         CountLiveCheck(PROJECTION_POST_AA);
#endif
         set_projection_original(context, projection);
         return;
      }
#if DEVELOPMENT
      // Before the jitter: is the engine's projection symmetric (_31 = _32 = 0), as unjittering from the matrix itself assumes
      for (size_t i = 0; i < projection_max_abs_bits.size(); i++)
      {
         const uint32_t bits = std::bit_cast<uint32_t>(std::fabs(projection[8 + i]));
         std::atomic_ref max_bits(projection_max_abs_bits[i]);
         uint32_t seen = max_bits.load(std::memory_order_relaxed);
         while (bits > seen && !max_bits.compare_exchange_weak(seen, bits, std::memory_order_relaxed))
         {
         }
      }
#endif
      std::array<float, 16> jittered;
      std::copy_n(projection, 16, jittered.data());
      bool motion_active = false;
      {
         const std::unique_lock lock(camera_state.mutex);
         motion_active = camera_state.motion_active;
         if (motion_active && slot == uint32_t(g_dlaa_camera_slot))
         {
            std::copy_n(reinterpret_cast<const float*>(game_module + camera_views_rva + slot * camera_matrix_size), 16, camera_state.view.data());
            camera_state.projection = jittered;
            camera_state.applied_jitter_pixels = camera_state.jitter_pixels;
            camera_state.applied_jitter_ndc = camera_state.jitter_ndc;
            camera_state.captured = true;
         }
         // NDC x = clip x / -view z, so _31 and _32 (view z's) move it by minus themselves
         jittered[8] -= camera_state.jitter_ndc[0];
         jittered[9] -= camera_state.jitter_ndc[1];
      }
      set_projection_original(context, (motion_active || g_mv_force_jitter) ? jittered.data() : projection);
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

#if DEVELOPMENT
   // DRED (auto breadcrumbs and page faults), enabled before the game's device is made (ReShade's "create_device" comes before it): when the
   // GPU hangs, "LogDeviceRemoval()" logs what the unfinished command lists were doing and the faulting address. Only with "DRED=1" in the
   // mod's ReShade.ini section: the breadcrumbs cost 2-5% of the GPU (Microsoft's "Use DRED")
   bool EnableDred(reshade::api::device_api api, uint32_t& api_version)
   {
      bool dred = false;
      reshade::get_config_value(nullptr, NAME, "DRED", dred);
      if (!dred)
      {
         return false;
      }
      const HMODULE d3d12 = GetModuleHandleW(L"d3d12.dll");
      const auto get_debug_interface = (d3d12 ? reinterpret_cast<PFN_D3D12_GET_DEBUG_INTERFACE>(GetProcAddress(d3d12, "D3D12GetDebugInterface")) : nullptr);
      com_ptr<ID3D12DeviceRemovedExtendedDataSettings> settings;
      if (api == reshade::api::device_api::d3d12 && get_debug_interface && SUCCEEDED(get_debug_interface(IID_PPV_ARGS(&settings))))
      {
         settings->SetAutoBreadcrumbsEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
         settings->SetPageFaultEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
         reshade::log::message(reshade::log::level::info, "[DLAA] DRED enabled");
      }
      return false;
   }

   std::atomic_flag device_removal_logged;
   void LogDeviceRemoval(ID3D12Device* device)
   {
      if (SUCCEEDED(device->GetDeviceRemovedReason()) || device_removal_logged.test_and_set())
      {
         return;
      }
      const auto narrow = [](const wchar_t* text)
      {
         std::string result;
         for (; text && *text; text++)
         {
            result += (*text < 128 ? char(*text) : '?');
         }
         return result;
      };
      const auto log = [](const std::string& line)
      {
         reshade::log::message(reshade::log::level::error, ("[DRED] " + line).c_str());
      };
      log(std::format("Device removed: 0x{:08X}", uint32_t(device->GetDeviceRemovedReason())));
      com_ptr<ID3D12DeviceRemovedExtendedData1> dred;
      if (FAILED(device->QueryInterface(IID_PPV_ARGS(&dred))))
      {
         log("No DRED interface");
         return;
      }
      D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT1 breadcrumbs = {};
      if (SUCCEEDED(dred->GetAutoBreadcrumbsOutput1(&breadcrumbs)))
      {
         for (const D3D12_AUTO_BREADCRUMB_NODE1* node = breadcrumbs.pHeadAutoBreadcrumbNode; node; node = node->pNext)
         {
            const UINT done = (node->pLastBreadcrumbValue ? *node->pLastBreadcrumbValue : 0);
            if (done >= node->BreadcrumbCount)
            {
               continue;
            }
            // D3D12_AUTO_BREADCRUMB_OP values, the first one not done in brackets
            std::string ops;
            for (UINT i = (done > 12 ? done - 12 : 0); i < (std::min)(node->BreadcrumbCount, done + 6); i++)
            {
               ops += (i == done ? std::format("[{}] ", int(node->pCommandHistory[i])) : std::format("{} ", int(node->pCommandHistory[i])));
            }
            log(std::format("List \"{}\" {} on queue \"{}\" {}: {} of {} done, ops {}", narrow(node->pCommandListDebugNameW), static_cast<const void*>(node->pCommandList), narrow(node->pCommandQueueDebugNameW), static_cast<const void*>(node->pCommandQueue), done, node->BreadcrumbCount, ops));
            for (UINT i = 0; i < node->BreadcrumbContextsCount; i++)
            {
               log(std::format("   context at {}: {}", node->pBreadcrumbContexts[i].BreadcrumbIndex, narrow(node->pBreadcrumbContexts[i].pContextString)));
            }
         }
      }
      D3D12_DRED_PAGE_FAULT_OUTPUT1 page_fault = {};
      if (SUCCEEDED(dred->GetPageFaultAllocationOutput1(&page_fault)))
      {
         log(std::format("Page fault VA 0x{:X}", page_fault.PageFaultVA));
         for (const D3D12_DRED_ALLOCATION_NODE1* node = page_fault.pHeadExistingAllocationNode; node; node = node->pNext)
         {
            log(std::format("   existing allocation \"{}\" type {}", narrow(node->ObjectNameW), int(node->AllocationType)));
         }
         for (const D3D12_DRED_ALLOCATION_NODE1* node = page_fault.pHeadRecentFreedAllocationNode; node; node = node->pNext)
         {
            log(std::format("   recently freed \"{}\" type {}", narrow(node->ObjectNameW), int(node->AllocationType)));
         }
      }
   }
#endif

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
   uint64_t predication_present = UINT64_MAX; // The present count of its last draw: it only counts in that frame
   // Recorded on other threads than the post chain (see "shader_hashes_depth_prepass")
   std::atomic<uint64_t> scene_depth = 0;
   std::atomic<reshade::api::format> scene_depth_format = reshade::api::format::unknown;
   std::atomic<uint64_t> fxaa_input = 0;
   std::atomic<reshade::api::format> fxaa_input_format = reshade::api::format::unknown;

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
#if DEVELOPMENT
         // The layout the above assumes: the mirror's VP is its P x V (both transposed)
         if (g_mv_live_checks)
         {
            CountLiveCheck(VP_RECOMPOSE_CHECKED);
            for (size_t row = 0; row < 4; row++)
            {
               for (size_t column = 0; column < 4; column++)
               {
                  double recomposed = 0.0;
                  for (size_t k = 0; k < 4; k++)
                  {
                     recomposed += double(current_camera.cb12[row * 4 + k]) * double(current_camera.cb12[cb12_view + k * 4 + column]);
                  }
                  const double stored = current_camera.cb12[cb12_view_projection + row * 4 + column];
                  if (std::abs(recomposed - stored) > 1e-4 * (1.0 + std::abs(stored)))
                  {
                     CountLiveCheck(VP_RECOMPOSE_MISMATCH);
                     if (!vp_recompose_mismatch_logged.test_and_set())
                     {
                        reshade::log::message(reshade::log::level::warning, std::format("[CB12] VP isn't P x V at row {} column {}: {} recomposed, {} stored", row, column, recomposed, stored).c_str());
                     }
                     row = 4;
                     break;
                  }
               }
            }
         }
#endif
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
#if DEVELOPMENT
         if (g_mv_log_runs)
         {
            reshade::log::message(reshade::log::level::info, std::format("[ObjectMV] Run: stamp {} list {} vb {:#x}+{} first index {} vertices {} stride {} anchor ({:.3f} {:.3f} {:.3f}) match {} (distance {:.3f}, vertices {}) previous {} read {} write {} (previous objects {}, objects {})", new_stamp, cmd_list, vertex_buffer.buffer, vertex_buffer.offset, first_index, vertex_count, vertex_stride, anchor[0], anchor[1], anchor[2], match != nullptr, (match ? std::sqrt(nearest_distance_squared) : -1.f), (match ? match->vertex_count : 0u), has_previous, run->draw_constants[0], run->draw_constants[1], previous_objects.size(), objects.size()).c_str());
         }
#endif
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

#if DEVELOPMENT
   // The engine's constant ring "<pxd:cb_pool>" (one persistently mapped upload buffer every cbuffer CBV points into), mapped on the first
   // CBV found in it. Its mutex guards the fields, placed first
   struct
   {
      std::shared_mutex mutex;
      uint64_t resource = 0;
      const uint8_t* mapped = nullptr;
      uint64_t size = 0;
   } constant_ring;

   // The first "byte_count" bytes of the draw's bound VS CBV "binding" (the descriptors Core shadows -> the ring's CPU pointer): null if any
   // of that isn't known. Mapping an upload buffer again returns the pointer it was mapped at already.
   const uint8_t* ReadBoundConstants(const D3D12State::CommandListState& state, const DeviceData& device_data, uint32_t binding, uint64_t byte_count)
   {
      if (state.graphics.bindings.size() <= vs_descriptor_table_root_parameter)
      {
         return nullptr;
      }
      const reshade::api::descriptor_table table = state.graphics.bindings[vs_descriptor_table_root_parameter].table;
      if (table.handle == 0)
      {
         return nullptr;
      }
      const D3D12State::Descriptor* const descriptor = D3D12State::FindDescriptor(device_data.descriptor_heaps, device_data.reshade_device, table, binding);
      if (!descriptor || descriptor->type != reshade::api::descriptor_type::constant_buffer)
      {
         return nullptr;
      }
      const uint64_t buffer = descriptor->handle;
      const uint64_t offset = descriptor->offset;
      if (buffer == 0)
      {
         return nullptr;
      }
      const uint8_t* mapped = nullptr;
      {
         const std::shared_lock lock(constant_ring.mutex);
         if (constant_ring.resource == buffer && offset + byte_count <= constant_ring.size)
         {
            mapped = constant_ring.mapped;
         }
      }
      if (!mapped)
      {
         auto* const resource = reinterpret_cast<ID3D12Resource*>(buffer);
         D3D12_HEAP_PROPERTIES heap = {};
         resource->GetHeapProperties(&heap, nullptr);
         const D3D12_RESOURCE_DESC desc = resource->GetDesc();
         void* map = nullptr;
         // No read range: the CPU reads it
         if (heap.Type != D3D12_HEAP_TYPE_UPLOAD || desc.Dimension != D3D12_RESOURCE_DIMENSION_BUFFER || offset + byte_count > desc.Width || FAILED(resource->Map(0, nullptr, &map)))
         {
            return nullptr;
         }
         const std::unique_lock lock(constant_ring.mutex);
         constant_ring.resource = buffer;
         constant_ring.mapped = static_cast<const uint8_t*>(map);
         constant_ring.size = desc.Width;
         mapped = constant_ring.mapped;
      }
      return mapped + offset;
   }
#endif

   // "*target" (a texture of "pass", in "state" between draws) at "width" x "height", made again (the old one retired) at another size. With
   // "pass->mutex" held. "*created": whether it was made now
   static bool EnsurePassTarget(DeviceData::GamePassD3D12* pass, reshade::api::resource* target, reshade::api::device* device, uint64_t present_count, uint32_t width, uint32_t height, reshade::api::format format, reshade::api::resource_usage usage, reshade::api::resource_usage state, const wchar_t* name, bool* created = nullptr)
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
      const reshade::api::resource_desc desc(width, height, 1, 1, format, 1, reshade::api::memory_heap::gpu_only, usage);
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

      native_shaders_definitions.emplace(CompileTimeStringHash("YKR SMAA Predication PS"), ShaderDefinition{"Luma_YKR_SMAAPredication", reshade::api::pipeline_subobject_type::pixel_shader});
      native_shaders_definitions.emplace(CompileTimeStringHash("YKR SMAA T2x Blending Weight Calculation PS"), ShaderDefinition{.file_name = "Luma_SMAA_impl", .type = reshade::api::pipeline_subobject_type::pixel_shader, .function_name = "smaa_blending_weight_calculation_ps", .defines_data = {{"SMAA_T2X", "1"}}});
      native_shaders_definitions.emplace(CompileTimeStringHash("YKR SMAA T2x Neighborhood Blending PS"), ShaderDefinition{.file_name = "Luma_SMAA_impl", .type = reshade::api::pipeline_subobject_type::pixel_shader, .function_name = "smaa_neighborhood_blending_ps", .defines_data = {{"SMAA_T2X", "1"}}});
      native_shaders_definitions.emplace(CompileTimeStringHash("YKR SMAA T2x Resolve PS"), ShaderDefinition{"Luma_YKR_SMAAT2xResolve", reshade::api::pipeline_subobject_type::pixel_shader});
      native_shaders_definitions.emplace(CompileTimeStringHash("YKR DLAA Copy PS"), ShaderDefinition{"Luma_YKR_DLAA", reshade::api::pipeline_subobject_type::pixel_shader, nullptr, "copy_ps"});
      native_shaders_definitions.emplace(CompileTimeStringHash("YKR DLAA Motion Vectors PS"), ShaderDefinition{"Luma_YKR_DLAA", reshade::api::pipeline_subobject_type::pixel_shader, nullptr, "motion_vectors_ps"});
      native_shaders_definitions.emplace(CompileTimeStringHash("YKR DLAA Decode PS"), ShaderDefinition{"Luma_YKR_DLAA", reshade::api::pipeline_subobject_type::pixel_shader, nullptr, "decode_ps"});
      native_shaders_definitions.emplace(CompileTimeStringHash("YKR DLAA Encode PS"), ShaderDefinition{"Luma_YKR_DLAA", reshade::api::pipeline_subobject_type::pixel_shader, nullptr, "encode_ps"});

      sr_game_tooltip = "DLAA only (no upscaling). Characters, hair, trees and moved world objects get their own motion; particles and other effects move with the camera's and can leave trails.\n";

      reshade::register_event<reshade::addon_event::barrier>(OnBarrier);
#if DEVELOPMENT
      reshade::register_event<reshade::addon_event::create_device>(EnableDred);
      // Core has no game hook for the executions: the stamps of "list_stamps" are checked here (unregistered in "DllMain()")
      reshade::register_event<reshade::addon_event::execute_command_list>(OnExecuteStampedCommandList);
      reshade::register_event<reshade::addon_event::reset_command_list>(OnForgetCommandList);
      reshade::register_event<reshade::addon_event::destroy_command_list>(OnForgetCommandList);
#endif

#if DEVELOPMENT
      Mcp::RegisterToggles({{"hide_ui", &g_hide_ui}, {"smaa_enable", &g_smaa_enable}, {"smaa_predication", &g_smaa_predication}, {"smaa_t2x", &g_smaa_t2x}, {"smaa_t2x_flip", &g_smaa_t2x_flip}, {"dlaa_mip_bias", &g_dlaa_mip_bias}, {"object_motion_vectors", &g_object_motion_vectors},
         {"mv_force_jitter", &g_mv_force_jitter}, {"mv_disable_jitter", &g_mv_disable_jitter}, {"mv_debug_view", &g_mv_debug_view}, {"mv_log_runs", &g_mv_log_runs}, {"mv_live_checks", &g_mv_live_checks}, {"object_mv.rigid", &g_object_mv_rigid}});
      Mcp::RegisterValues({{"object_mv.match_distance", &g_object_mv_match_distance, 0.f, 10.f}, {"object_mv.max_distance", &g_object_mv_max_distance, 0.f, 100.f}, {"object_mv.rigid_match_distance", &g_object_mv_rigid_match_distance, 0.f, 10.f}});
      for (size_t i = 0; i < MV_COUNT; i++)
      {
         Mcp::RegisterCounter(object_motion_count_names[i], &object_motion_counts_frame[i]);
      }
      Mcp::RegisterInts({{"dlaa_camera_slot", &g_dlaa_camera_slot, 0, 3}, {"dlaa_jitter_flip", &g_dlaa_jitter_flip, 0, 3}, {"dlaa_motion_flip", &g_dlaa_motion_flip, 0, 3}, {"dlaa_stages", &g_dlaa_stages, -1, 3}});
      Mcp::RegisterCounters({
         {"projection_sets.slot0", &projection_sets_frame[0]},
         {"projection_sets.slot1", &projection_sets_frame[1]},
         {"projection_sets.slot2", &projection_sets_frame[2]},
         {"projection_sets.slot3", &projection_sets_frame[3]},
         {"projection_sets.other", &projection_sets_frame[4]},
         {"dlaa.scene_state", &dlaa_scene_state},
         {"anchor.reads", &anchor_counts_frame[ANCHOR_READS]},
         {"anchor.unavailable", &anchor_counts_frame[ANCHOR_UNAVAILABLE]},
         {"anchor.checked", &anchor_counts_frame[ANCHOR_CHECKED]},
         {"anchor.mismatch", &anchor_counts_frame[ANCHOR_MISMATCH]},
         {"anchor.mismatch_total", &anchor_mismatch_total},
         {"cb12.checked", &live_checks_frame[CB12_CHECKED]},
         {"cb12.unavailable", &live_checks_frame[CB12_UNAVAILABLE]},
         {"cb12.mismatch", &live_checks_frame[CB12_MISMATCH]},
         {"views.checked", &live_checks_frame[VIEWS_CHECKED]},
         {"views.changed", &live_checks_frame[VIEWS_CHANGED]},
         {"prepass.draws", &live_checks_frame[PREPASS_DRAWS]},
         {"prepass.instanced", &live_checks_frame[PREPASS_INSTANCED]},
         {"prepass.rt_count_not_1", &live_checks_frame[PREPASS_RT_COUNT_NOT_1]},
         {"prepass.other_dsv", &live_checks_frame[PREPASS_OTHER_DSV]},
         {"exec.lists", &live_checks_frame[EXEC_LISTS]},
         {"exec.stamp_mismatch", &live_checks_frame[EXEC_STAMP_MISMATCH]},
         {"exec.stamp_mixed", &live_checks_frame[EXEC_STAMP_MIXED]},
         {"projection_sets.post_aa", &live_checks_frame[PROJECTION_POST_AA]},
         {"projection_sets.main_copy", &live_checks_frame[PROJECTION_MAIN_COPY]},
         {"post_aa.draws", &live_checks_frame[POST_AA_DRAWS]},
         {"post_aa.jittered", &live_checks_frame[POST_AA_JITTERED]},
         {"phase.ccr_other_phase", &live_checks_frame[CCR_OTHER_PHASE]},
         {"screen_effect.draws", &live_checks_frame[SCREEN_EFFECT_DRAWS]},
         {"vp.recompose_checked", &live_checks_frame[VP_RECOMPOSE_CHECKED]},
         {"vp.recompose_mismatch", &live_checks_frame[VP_RECOMPOSE_MISMATCH]},
      });
      // Written by the latch at present only: the last frame's maxima
      Mcp::RegisterValues({{"projection.max_abs_31", &projection_max_abs_frame[0]}, {"projection.max_abs_32",
                                                                                       &projection_max_abs_frame[1]}});
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

#if DEVELOPMENT
      // What "SetProjectionDetour()" leaves unjittered must be all the camera the draws after the anti-aliasing see, and DLAA must run in
      // the phase before them
      const uint32_t phase = GetFramePhase();
      if (phase >= first_post_anti_aliasing_phase && phase <= last_post_anti_aliasing_phase)
      {
         if (const uint8_t* const mirror = GetConstantMirror())
         {
            const auto* const cb12 = reinterpret_cast<const float*>(mirror + mirror_data_offset + cb12_base_offset);
            CountLiveCheck(POST_AA_DRAWS);
            if (cb12[cb12_projection_31] != 0.f || cb12[cb12_projection_32] != 0.f)
            {
               CountLiveCheck(POST_AA_JITTERED);
               if (!post_aa_jittered_logged.test_and_set())
               {
                  reshade::log::message(reshade::log::level::warning, std::format("[PostAA] First draw after the anti-aliasing with a jittered camera: VS {:#010X} PS {:#010X}, phase {:#x}, _31 {}, _32 {}", original_shader_hashes.vertex_shaders[0], original_shader_hashes.pixel_shaders[0], phase, cb12[cb12_projection_31], cb12[cb12_projection_32]).c_str());
               }
            }
         }
      }
      if (original_shader_hashes.Contains(shader_hashes_color_correct) && phase != color_correct_phase)
      {
         CountLiveCheck(CCR_OTHER_PHASE);
      }
      // Which shaders the screen effects draw with (only the fade's are known offline), for their scene brightness
      if (screen_effect_depth > 0)
      {
         CountLiveCheck(SCREEN_EFFECT_DRAWS);
         static std::atomic<uint32_t> screen_effect_draws_logged = 0;
         if (screen_effect_draws_logged.fetch_add(1, std::memory_order_relaxed) < 32)
         {
            reshade::log::message(reshade::log::level::info, std::format("[ScreenEffect] Draw: VS {:#010X} PS {:#010X}, phase {:#x}", original_shader_hashes.vertex_shaders[0], original_shader_hashes.pixel_shaders[0], phase).c_str());
         }
      }
#endif
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
      if (motion_active && g_dlaa_stages >= 0 && is_color_correct)
      {
         DrawDLAA(cmd_list_data, device_data, &game_device_data);
#if DEVELOPMENT
         if (g_mv_live_checks)
         {
            RecordListStamp(cmd_list_data, device_data);
         }
#endif
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
#if DEVELOPMENT
         if (g_mv_live_checks)
         {
            // Against the first 3x4 of the CBV the draw reads, which the CPU copy of the engine's mirror must have left exactly
            if (!anchor_read)
            {
               std::atomic_ref(anchor_counts[ANCHOR_UNAVAILABLE]).fetch_add(1, std::memory_order_relaxed);
            }
            else
            {
               std::atomic_ref(anchor_counts[ANCHOR_READS]).fetch_add(1, std::memory_order_relaxed);
               if (const auto* const bound_world = reinterpret_cast<const float*>(game_device_data.ReadBoundConstants(state, device_data, bone_cbv_binding, world_matrix_size)))
               {
                  std::atomic_ref(anchor_counts[ANCHOR_CHECKED]).fetch_add(1, std::memory_order_relaxed);
                  if (std::memcmp(world.data(), bound_world, world_matrix_size) != 0)
                  {
                     std::atomic_ref(anchor_counts[ANCHOR_MISMATCH]).fetch_add(1, std::memory_order_relaxed);
                     std::atomic_ref(anchor_mismatch_total).fetch_add(1, std::memory_order_relaxed);
                     if (!anchor_mismatch_logged.test_and_set())
                     {
                        reshade::log::message(reshade::log::level::warning, std::format("[Anchor] First mismatch, VS 0x{:08X}: mirror translation ({}, {}, {}), bound CBV ({}, {}, {})", original_shader_hashes.vertex_shaders[0], anchor[0], anchor[1], anchor[2], bound_world[3], bound_world[7], bound_world[11]).c_str());
                     }
                  }
               }
            }

            // The assumptions of the object motion vectors (see "LiveCheck")
            CountLiveCheck(PREPASS_DRAWS);
            // Valid in this build only if Core assigns it for D3D12 draws before this callback
            if (last_draw_dispatch_data.instance_count != 1)
            {
               CountLiveCheck(PREPASS_INSTANCED);
            }
            if (state.render_target_views.size() != 1)
            {
               CountLiveCheck(PREPASS_RT_COUNT_NOT_1);
            }
            // The scene depth is stored by the depth prepass draws below: the first draws after it was made again see the old one
            if (state.depth_stencil_view.resource != game_device_data.scene_depth.load(std::memory_order_relaxed))
            {
               CountLiveCheck(PREPASS_OTHER_DSV);
            }

            // The mirror's cb12 against the bound VS b12
            const uint8_t* const mirror = GetConstantMirror();
            const auto* const bound_cb12 = reinterpret_cast<const uint32_t*>(game_device_data.ReadBoundConstants(state, device_data, cb12_binding, cb12_size));
            if (!mirror || !bound_cb12)
            {
               CountLiveCheck(CB12_UNAVAILABLE);
            }
            else
            {
               CountLiveCheck(CB12_CHECKED);
               const auto* const mirror_cb12 = reinterpret_cast<const uint32_t*>(mirror + mirror_data_offset + cb12_base_offset);
               const auto [mirror_word, bound_word] = std::mismatch(mirror_cb12, mirror_cb12 + cb12_size / sizeof(uint32_t), bound_cb12);
               if (mirror_word != mirror_cb12 + cb12_size / sizeof(uint32_t))
               {
                  CountLiveCheck(CB12_MISMATCH);
                  if (!cb12_mismatch_logged.test_and_set())
                  {
                     reshade::log::message(reshade::log::level::warning, std::format("[CB12] First mismatch, VS 0x{:08X}: float {} is {} in the mirror, {} in the bound CBV", original_shader_hashes.vertex_shaders[0], mirror_word - mirror_cb12, std::bit_cast<float>(*mirror_word), std::bit_cast<float>(*bound_word)).c_str());
                  }
               }
            }

            // The CPU descriptors the game bound still describe the views Core tracked at bind time (D3D12 reads them when they are bound, and
            // games rewrite them right after), so the draw can bind them again
            const auto check_view = [&](const char* name, reshade::api::resource_view handle, const D3D12State::ViewInfo& tracked)
            {
               if (handle.handle == 0)
               {
                  return;
               }
               CountLiveCheck(VIEWS_CHECKED);
               const D3D12State::ViewInfo current = D3D12State::GetViewInfo(device_data.reshade_device, handle);
               if (current.resource != tracked.resource || current.format != tracked.format || current.first_level != tracked.first_level || current.first_layer != tracked.first_layer || current.layer_count != tracked.layer_count)
               {
                  CountLiveCheck(VIEWS_CHANGED);
                  if (!views_changed_logged.test_and_set())
                  {
                     reshade::log::message(reshade::log::level::warning, std::format("[Views] First change, VS 0x{:08X}, {}: tracked resource 0x{:X} format {}, now resource 0x{:X} format {}", original_shader_hashes.vertex_shaders[0], name, tracked.resource, uint32_t(tracked.format), current.resource, uint32_t(current.format)).c_str());
                  }
               }
            };
            if (!state.render_targets.empty() && state.render_target_views.size() == state.render_targets.size())
            {
               check_view("RT0", state.render_targets[0], state.render_target_views[0]);
            }
            check_view("DSV", state.depth_stencil, state.depth_stencil_view);

            // Recorded in a frame, executed in the same
            RecordListStamp(cmd_list_data, device_data);
         }
#endif
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
      const bool is_depth_prepass = original_shader_hashes.Contains(shader_hashes_depth_prepass) || motion_vector_pair;
      if (is_depth_prepass)
      {
         game_device_data.scene_depth_format.store(state.depth_stencil_view.format, std::memory_order_relaxed);
         game_device_data.scene_depth.store(state.depth_stencil_view.resource, std::memory_order_relaxed);
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
      if (is_depth_prepass)
      {
         return DrawOrDispatchOverrideType::None;
      }

      // Barriers are invalid in a native render pass (the Luma passes refuse to draw there too)
      // Not under DLAA, where SMAA doesn't run (a frame DLAA can't draw gets SMAA without the predication)
      const bool predicate = g_smaa_enable && g_smaa_predication && !game_device_data.dlaa_active.load(std::memory_order_relaxed);
      const bool is_predication_draw = predicate && original_shader_hashes.Contains(shader_hashes_ssao);
      const bool is_depth_copy_draw = motion_active && g_dlaa_stages >= 0 && original_shader_hashes.Contains(shader_hashes_depth_copy);
      if (!state.in_render_pass && (is_predication_draw || is_depth_copy_draw))
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
         return DrawOrDispatchOverrideType::None;
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
         // DLAA antialiased the scene: the canvas goes through as it is
         if (game_device_data.dlaa_drawn_present.load(std::memory_order_relaxed) == present_count)
         {
            const bool copied = DrawFullscreenPassD3D12(cmd_list_data, device_data, &game_device_data.copy_pass, CompileTimeStringHash("YKR DLAA Copy PS"), "FXAA Bypass", "Luma FXAA Bypass", color, color_format, target, state.render_target_descs[0], {});
            return copied ? DrawOrDispatchOverrideType::Replaced : DrawOrDispatchOverrideType::None;
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
               predicated = g_smaa_predication && game_device_data.predication_present == present_count && predication_desc.texture.width == target_desc.texture.width && predication_desc.texture.height == target_desc.texture.height;
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
               // Recorded at the ccr draw, maybe on a worker whose list is recorded after this one: the GPU still runs it first (see "DrawDLAA()")
               const uint64_t motion_vectors_present = game_device_data.motion_vectors_present.load(std::memory_order_relaxed);
               if (motion_vectors_present == present_count || motion_vectors_present + 1 == present_count)
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
                  applied_jitter_x = camera_state.applied_jitter_pixels[0];
               }
               // A sample offset of (+0.25, +0.25) takes the area texture's subsample 1, (-0.25, -0.25) the 2 (0: no jitter)
               float subsample_index = 0.f;
               if (applied_jitter_x != 0.f)
               {
                  subsample_index = (((applied_jitter_x > 0.f) != g_smaa_t2x_flip) ? 1.f : 2.f);
               }
               const uint32_t current_frame = uint32_t(present_count & 1);
               std::array<reshade::api::resource, 3> frames = {};
               bool history_valid = false;
               if (motion_vectors_desc.texture.width == target_desc.texture.width && motion_vectors_desc.texture.height == target_desc.texture.height)
               {
                  const std::unique_lock lock(game_device_data.t2x_pass.mutex);
                  bool created = false;
                  if (game_device_data.EnsurePassTarget(&game_device_data.t2x_pass, &game_device_data.t2x_frames[0], device, present_count, target_desc.texture.width, target_desc.texture.height, reshade::api::format::r16g16b16a16_float, reshade::api::resource_usage::render_target | reshade::api::resource_usage::shader_resource, reshade::api::resource_usage::shader_resource, L"Luma YKR SMAA T2x Frame 0", &created) &&
                      game_device_data.EnsurePassTarget(&game_device_data.t2x_pass, &game_device_data.t2x_frames[1], device, present_count, target_desc.texture.width, target_desc.texture.height, reshade::api::format::r16g16b16a16_float, reshade::api::resource_usage::render_target | reshade::api::resource_usage::shader_resource, reshade::api::resource_usage::shader_resource, L"Luma YKR SMAA T2x Frame 1", &created))
                  {
                     // "DrawFullscreenPassD3D12()" takes the pass' mutex. A target retired meanwhile lives on until the lists recorded with it ran.
                     history_valid = !created && game_device_data.t2x_drawn_present.load(std::memory_order_relaxed) + 1 == present_count;
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
      [[maybe_unused]] bool continued_run = false;
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
         continued_run = true;
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
#if DEVELOPMENT
      if (g_mv_log_runs && !rigid)
      {
         if (object_mv_runs_logged.fetch_add(1, std::memory_order_relaxed) < object_mv_runs_to_log)
         {
            std::string streams;
            for (const D3D12State::BufferBinding& binding : state.vertex_buffers)
            {
               streams += std::format(" {:#x}+{}/{}", binding.buffer, binding.offset, binding.stride);
            }
            reshade::log::message(reshade::log::level::info, std::format("[ObjectMV] Draw: VS {:#010X} list {} thread {} present {} first index {} index count {} vertex offset {}{} | run: read {} write {} vertices {} previous {} | streams' vertices {}:{}", pair->vertex_shader_hash, static_cast<const void*>(cmd_list_data.cmd_list), GetCurrentThreadId(), present_count, draw.first_index, draw.index_count, draw.vertex_offset, (continued_run ? " (continues)" : ""), run.draw_constants[0], run.draw_constants[1], run.draw_constants[2], run.draw_constants[3], streams_vertex_count(), streams).c_str());
         }
         else
         {
            g_mv_log_runs = false;
            object_mv_runs_logged.store(0, std::memory_order_relaxed);
         }
      }
#endif

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
      SR::InstanceData* const sr_instance_data = device_data.GetSRInstanceData();
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
         // The copy is recorded at the low-res depth draw, often on another worker that records its list after this one: the GPU still runs it
         // first (it's submitted first), so last frame's stamp stands for this frame's copy too. A copy point that stops drawing turns DLAA off a frame later
         if (game_device_data->depth_copy_present == present_count || game_device_data->depth_copy_present + 1 == present_count)
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

      std::array<float, 16> view;
      std::array<float, 16> projection;
      std::array<float, 2> jitter_pixels;
      std::array<float, 2> jitter_ndc;
      {
         const std::shared_lock lock(camera_state.mutex);
         if (!camera_state.captured)
         {
            return;
         }
         view = camera_state.view;
         projection = camera_state.projection;
         jitter_pixels = camera_state.applied_jitter_pixels;
         jitter_ndc = camera_state.applied_jitter_ndc;
      }

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
      if (!dlaa_active || !sr_instance_data || !motion_vectors_drawn || g_dlaa_stages < 1)
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
#if DEVELOPMENT
      dlaa_scene_state = uint32_t(scene.state);
#endif

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

      bool dlss_drawn = false;
      if (decoded && g_dlaa_stages >= 2)
      {
         const float jitter_sign_x = ((g_dlaa_jitter_flip & 1) ? -1.f : 1.f);
         const float jitter_sign_y = ((g_dlaa_jitter_flip & 2) ? -1.f : 1.f);
         const float motion_sign_x = ((g_dlaa_motion_flip & 1) ? -1.f : 1.f);
         const float motion_sign_y = ((g_dlaa_motion_flip & 2) ? -1.f : 1.f);
         // The inputs stay in a shader resource state (pixel and non pixel), which NGX reads; the output is in unordered access
         auto* const native_list = reinterpret_cast<ID3D12GraphicsCommandList*>(cmd_list->get_native());
         // UV deltas, previous minus current. Exposure 1: the scene is already exposed and tonemapped
         const SR::SettingsData settings_data = {.output_width = width, .output_height = height, .render_width = width, .render_height = height, .hdr = true, .inverted_depth = false, .mvs_jittered = false, .mvs_x_scale = float(width) * motion_sign_x, .mvs_y_scale = float(height) * motion_sign_y, .auto_exposure = false, .render_preset = dlss_render_preset};
         const SR::SuperResolutionImpl::DrawData draw_data = {
            .reset = device_data.force_reset_sr,
            .output_recreated = output_recreated,
            .output_color = reinterpret_cast<ID3D12Resource*>(output.handle),
            .source_color = reinterpret_cast<ID3D12Resource*>(decoded_scene.handle),
            .motion_vectors = reinterpret_cast<ID3D12Resource*>(motion_vectors.handle),
            .depth_buffer = reinterpret_cast<ID3D12Resource*>(depth.handle),
            .render_width = width,
            .render_height = height,
            .jitter_x = jitter_pixels[0] * jitter_sign_x,
            .jitter_y = jitter_pixels[1] * jitter_sign_y,
         };
         dlss_drawn = sr_implementations[device_data.sr_type]->UpdateSettings(sr_instance_data, native_list, settings_data) && sr_implementations[device_data.sr_type]->Draw(sr_instance_data, native_list, draw_data);
         // NGX binds its own descriptor heaps, root signature and pipeline
         D3D12State::RestoreCompute(cmd_list, state, {});
         RestoreGameGraphicsD3D12(cmd_list_data);
         TraceLumaPass(&cmd_list_data, device_data, "Luma DLAA", {{.resource = decoded_scene.handle, .format = reshade::api::format::r16g16b16a16_float}, {.resource = motion_vectors.handle, .format = reshade::api::format::r16g16_float}, {.resource = depth.handle, .format = reshade::api::format::r32_float}}, {{.resource = output.handle, .format = reshade::api::format::r16g16b16a16_float}}, true);
         if (dlss_drawn)
         {
            const std::unique_lock lock(game_device_data->color_pass.mutex);
            game_device_data->dlaa_output_recreated = false;
         }
         else
         {
            device_data.sr_suppressed = true;
            reshade::log::message(reshade::log::level::error, "[DLAA] DLSS failed, so it's off until it's picked again");
         }
      }

      bool encoded = false;
      if (dlss_drawn && g_dlaa_stages >= 3)
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
      for (DeviceData::GamePassD3D12* const pass : {&game_device_data.predication_pass, &game_device_data.copy_pass, &game_device_data.motion_vector_pass, &game_device_data.color_pass, &game_device_data.t2x_pass})
      {
         const std::unique_lock lock(pass->mutex);
         DestroyRetiredD3D12GamePassResources(pass, device_data.reshade_device, present_count);
      }
      {
         const std::unique_lock lock(camera_state.mutex);
         camera_state.motion_active = motion_active;
         const bool jittered = ((dlaa_active && dlaa_drawn) || (t2x_active && t2x_drawn) || g_mv_force_jitter) && !g_mv_disable_jitter;
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
         camera_state.captured = camera_state.captured && motion_active;
      }
      // -1 at native resolution, with DLAA only (the game has no TAA: a negative bias would shimmer)
      SetTextureMipLodBias(nullptr, device_data, ((dlaa_active && g_dlaa_mip_bias) ? SR::GetMipLODBias(1u, 1u) : 0.f));

#if DEVELOPMENT
      LogDeviceRemoval(native_device);
      const auto latch = [](auto* counting, auto* last_frame)
      {
         for (size_t i = 0; i < counting->size(); i++)
         {
            (*last_frame)[i] = std::bit_cast<std::remove_reference_t<decltype((*last_frame)[i])>>(std::atomic_ref((*counting)[i]).exchange(0, std::memory_order_relaxed));
         }
      };
      latch(&projection_sets, &projection_sets_frame);
      latch(&anchor_counts, &anchor_counts_frame);
      latch(&object_motion_counts, &object_motion_counts_frame);
      latch(&live_checks, &live_checks_frame);
      latch(&projection_max_abs_bits, &projection_max_abs_frame);
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
   }

   void DrawImGuiSettings(DeviceData& device_data) override
   {
      auto& gs = cb_luma_global_settings.GameSettings;

      ImGui::SeparatorText("Anti-Aliasing");

      // DLAA takes FXAA's place itself
      const bool sr_active = device_data.sr_type != SR::Type::None;
      DrawSettingCheckbox("SMAA Enable", "SMAAEnable", &g_smaa_enable, true, "Replaces the game's FXAA with SMAA (requires Anti-Aliasing enabled in the game's graphics settings; not used with DLSS).", sr_active);
      DrawSettingCheckbox("SMAA T2x", "SMAAT2x", &g_smaa_t2x, false, "Smoother edges and less shimmering, by blending each frame with the previous one.\nCan look slightly softer in motion. Not used with DLSS.", sr_active || !g_smaa_enable);
      ImGui::BeginDisabled(sr_active || !g_smaa_enable);
      DrawSettingSlider("RCAS Sharpness", "RCASSharpness", &g_rcas_sharpness, 0.f, 0.f, 1.f, "Sharpening applied on top of SMAA (0 = off).");
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
      ImGui::Checkbox("MV Force Jitter", &g_mv_force_jitter);
      if (ImGui::IsItemHovered())
      {
         ImGui::SetTooltip("Jitters the scene (Halton 2/3, 8 phases) without DLAA. The image shakes by a subpixel; nothing may flicker or lose\npixels (a draw that sets its own projection would). Not saved.");
      }
      ImGui::Checkbox("MV Disable Jitter", &g_mv_disable_jitter);
      if (ImGui::IsItemHovered())
      {
         ImGui::SetTooltip("No projection jitter under DLAA (DLSS gets zero jitter): isolates artifacts that come from the jitter. Not saved.");
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
      ImGui::Text("Projection setter calls: %u slot 0, %u slot 1, %u slot 2, %u slot 3, %u other (%u main camera copies), %u left unjittered after the anti-aliasing", projection_sets_frame[0], projection_sets_frame[1], projection_sets_frame[2], projection_sets_frame[3], projection_sets_frame[4], live_checks_frame[PROJECTION_MAIN_COPY], live_checks_frame[PROJECTION_POST_AA]);
   }
#endif

   void PrintImGuiAbout() override
   {
      ImGui::PushTextWrapPos(0.f);
      ImGui::Text(
         "Luma for \"Yakuza Kiwami\" is developed by DristoforColumb and is open source and free.\n"
         "It adds HDR and DLAA native anti-aliasing and replaces the game's FXAA with SMAA (with T2x), plus 16x anisotropic filtering.\n"
         "Enable Anti-Aliasing in the game's graphics settings for SMAA to apply.\n"
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
                  "\nAMD FidelityFX (RCAS)"
                  "\nNVIDIA NGX (DLSS)");
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
#if DEVELOPMENT
      reshade::unregister_event<reshade::addon_event::execute_command_list>(OnExecuteStampedCommandList);
      reshade::unregister_event<reshade::addon_event::reset_command_list>(OnForgetCommandList);
      reshade::unregister_event<reshade::addon_event::destroy_command_list>(OnForgetCommandList);
#endif
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
