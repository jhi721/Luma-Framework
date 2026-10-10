#define GAME_SUNSET_OVERDRIVE 1

#define GEOMETRY_SHADER_SUPPORT 0
#define DISABLE_AUTO_DEBUGGER 1
// DLSS, FSR 3 and Luma TAA run at native resolution on the pre-tonemap scene, in place of the game's SMAA (see "DrawSR")
#define ENABLE_LUMA_TAA 1
// The velocity passes (with the motion vector target added), the bloom's split bright passes (on the pre-SR scene) and
// "PS_OcclCoverDropouts" (the reactive mask's copy point) draw through "original_draw_dispatch_func"
#define ENABLE_POST_DRAW_DISPATCH_CALLBACK 1
// Core's SMAA in place of the game's SMAA blend, 1x and T2x (see "DrawSMAAInPlaceOfBlend")
#define ENABLE_SMAA 1
#define ENABLE_RCAS 1
// SMAA's area texture without the U-shape smoothing (see Luma_SMAA_impl.hlsl)
#define SMAA_SMOOTH_U_SHAPES 0

#include "..\..\Core\core.hpp"
#include "..\..\Core\includes\patched_draws.h"

// Sunset Overdrive (PC, 2018) is an x64 D3D11 game on Insomniac's engine. Its engine shaders are embedded in "Sunset.exe" along with
// name tables (see "_tools/sunset_overdrive/shader_names.csv"), and the post passes below were confirmed in game.
// The 16 "PS_ToneMap*" permutations (bloom, 3D LUT, film grain and vignette, each on or off) apply the exposure, a per-channel 1D tone
// curve, the bloom, the sRGB encoding and the LUT (see "Includes/ToneMap.hlsl").
const ShaderHashesList shader_hashes_ToneMap = {.pixel_shaders = {
                                                   0x92D51420,
                                                   0x7880CCF1,
                                                   0x86EE37B6,
                                                   0xC291251A,
                                                   0xE4EAD75E,
                                                   0x6619803F,
                                                   0x2A6A872A,
                                                   0xEE7FB6CC,
                                                   0x58E6385C,
                                                   0x633A5894,
                                                   0xD6B7231C,
                                                   0xE3E7DD84,
                                                   0x980ACE18,
                                                   0x2F20FD47,
                                                   0x902D30F9,
                                                   0xB2BBC089,
                                                }};
// "PS_CopyBuffer" is a generic copy. One of its uses draws the post-AA scene onto the swapchain, scaled to the window, before the UI.
constexpr uint32_t shader_hash_CopyBuffer = 0xF7085ACD;
// The only two velocity writers, replaced to also write the motion vectors for SR and SMAA T2x (see their shaders). The object pass
// only runs on frames with motion blur on or with a jitter stamp in view+0x120C (see "JitterDetour").
constexpr uint32_t shader_hash_CameraVelocity = 0x524BC7C4;
constexpr uint32_t shader_hash_ObjectVelocity = 0x928DFB65;
// SR runs on the scene (R11G11B10F, t5) at the post chain's entry, once every scene draw is in. That's the motion blur's color
// downsample with motion blur on, else the exposure downsample "PS_Tex". That one is a generic copy that also runs mid-forward, so we
// recognize it by its target, the t5 of the exposure CS "CS_AverageLumAccum" on the previous frame. The tonemap (scene at t6) is the
// last resort.
constexpr uint32_t shader_hash_MotionBlurDownsampleColor = 0x665173C0;
constexpr uint32_t shader_hash_Tex = 0x6CDD786E;
// "PS_OcclCoverDropouts" draws into the scene once a frame, after the fog and before the transparents. It's the reactive mask's copy
// point because it also runs with the game's Bloom off, which drops the mid-forward "PS_Tex" copy.
constexpr uint32_t shader_hash_OcclCoverDropouts = 0x7B7BCCF2;
constexpr uint32_t shader_hash_AverageLumAccum = 0x0DEE3EB1;
// The bright passes that split the bloom into an opaque part, min(t5, t6), and an alpha part, t5 - min(t5, t6). t5 is the scene's
// exposure downsample and t6 the mid-forward copy (jittered, before SR).
constexpr uint32_t shader_hash_BrightPassOpaque = 0x8EACA7B7;
constexpr uint32_t shader_hash_BrightPassAlpha = 0x6EB0D51C;
constexpr uint32_t shader_hash_CopyVS = CompileTimeStringHash("Copy VS");
constexpr uint32_t shader_hash_ReactiveMask = CompileTimeStringHash("SO Reactive Mask CS");
constexpr uint32_t shader_hash_SRExposure = CompileTimeStringHash("SO SR Exposure CS");
// The Bink video shaders, replaced for AutoHDR (see "Includes/Video.hlsl"). Full-screen movies draw into the swapchain, other uses
// into UI textures.
const ShaderHashesList shader_hashes_Video = {.pixel_shaders = {0x80649033, 0x0FA626B3, 0x11DDCB3A}};
// The game's SMAA runs on the tonemap's output (see "Game SMAA 1x / T2x" in "_tools/sunset_overdrive/NOTES.md"): an edges CS, a
// weights CS dispatched indirectly over the edge pixels, then the blend PS. With SMAA 1x the blend reads a copy of the tonemap's output
// at t5 and writes the tonemap's target. With T2x it reads the tonemap's output at t5 and writes a temp.
constexpr uint32_t shader_hash_SmaaFindEdges = 0x8948AF03;
constexpr uint32_t shader_hash_SmaaComputeWeights = 0x0CC29EC5;
constexpr uint32_t shader_hash_SmaaApplyBlend = 0xD8DB5270;
constexpr uint32_t shader_hash_SmaaPredication = CompileTimeStringHash("SO SMAA Predication CS");
// The game's T2x temporal resolve into the tonemap's target. Without a history, "PS_Tex" copies the blend's temp there instead.
constexpr uint32_t shader_hash_SmaaResolveReproj = 0xFF745C34;
constexpr uint32_t shader_hash_SmaaT2xWeights = CompileTimeStringHash("SO SMAA T2x Blending Weight Calculation PS");
constexpr uint32_t shader_hash_SmaaT2xBlend = CompileTimeStringHash("SO SMAA T2x Neighborhood Blending PS");
constexpr uint32_t shader_hash_SmaaT2xResolve = CompileTimeStringHash("SO SMAA T2x Resolve PS");
// SMAA's scratch textures (Core's and the predication's) are released after this many presents without SMAA (~5 s at 120 fps), such
// as with "SMAA Enable" off, under SR or in menus
constexpr uint32_t SMAA_IDLE_RELEASE_FRAMES = 600;

static bool g_hide_ui = false; // Session only, so a restart always has a HUD
static bool g_fix_corrupted_save_warning = true;
static bool g_smaa_enable = true;
static bool g_smaa_t2x = false; // Our SMAA T2x (when "SMAA Enable" is on), whatever the game's option
static float g_rcas_sharpness = 0.f;
#if DEVELOPMENT
// Jitter sign (DLSS-Best-Practices JIT-6), found empirically
static bool g_sr_flip_jitter_x = false;
static bool g_sr_flip_jitter_y = false;
// The reactive mask for FSR and Luma TAA (see "Luma_SO_ReactiveMask.hlsl")
static bool g_sr_reactive_enable = true;
static float g_sr_reactive_scale = 1.f;
static float g_sr_reactive_threshold = 0.2f;
#else
constexpr bool g_sr_flip_jitter_x = false;
constexpr bool g_sr_flip_jitter_y = false;
constexpr bool g_sr_reactive_enable = true;
constexpr float g_sr_reactive_scale = 1.f;
constexpr float g_sr_reactive_threshold = 0.2f;
#endif

namespace
{
   // The "Sunset.exe" code and data that SR and our SMAA use, as RVAs of the Steam build (TimeDateStamp 0x5C096926, see
   // "_tools/sunset_overdrive/RE_cpu_dlss.md"). The code is encrypted on disk and unpacked at startup, so we check it against the live
   // bytes.
   struct GameAddresses
   {
      uint8_t* jitter = nullptr;                      // void(View*, float* jx, float* jy): the camera view's projection jitter, NDC
      uint8_t* frame_params = nullptr;                // void(packet*): render command 49, copies the frame packet for the render thread
      const int32_t* game_frame = nullptr;            // The frame counter the jitter stamps into view+0x120C
      const int32_t* render_frame = nullptr;          // The render thread's copy of it, for the frame it draws
      int32_t* render_aa_option = nullptr;            // The render thread's copy of the "Anti-Aliasing" option (from the frame packet), which the SMAA and motion blur passes read
      const int32_t* debug_render_mode = nullptr;     // The game's jitter needs it under 2
      const uint8_t* skip_smaa = nullptr;             // A settings flag that skips SMAA and the game's jitter
      const uint8_t* shared_render_buffers = nullptr; // The "RenderBuffers" of the game's camera views (view+0x1160)
      uint8_t* save_entry_is_file = nullptr;          // bool(entry*): the save scans' "the found file isn't a directory" (see "SaveEntryIsFileDetour")
   };
   // The "Anti-Aliasing" option's values (game global 0x42AED00)
   constexpr int32_t AA_OPTION_OFF = 0;
   constexpr int32_t AA_OPTION_SMAA = 1;
   constexpr int32_t AA_OPTION_SMAA_T2X = 2;

   GameAddresses game_addresses; // Set right before the hooks are installed (the detours read it)

   // The jitter the hook produced for a game frame, with the camera values FSR needs. The render thread looks up the frame it draws.
   struct JitterRecord
   {
      int32_t frame = -1;
      float x = 0.f; // Pixels, +x right
      float y = 0.f; // Pixels, +y down
      float near_plane = 0.f;
      float far_plane = 0.f;
      float tan_half_fov_y = 0.f;
   };
   // The game thread can run a frame or two ahead of the render thread
   struct JitterHistory
   {
      std::shared_mutex mutex;
      std::array<JitterRecord, 4> records;
   } jitter_history;

   // Set at present. "VANILLA_JITTER" keeps the game's own jitter (with "SMAA Enable" off). "NO_JITTER" gives no jitter and no stamp,
   // for our SMAA 1x: the game's own jitter would stamp the frame, which turns its T2x resolve on whatever the option.
   // "STAMP_ONLY_JITTER" stamps without an offset (SR before its first drawn frame) and "SMAA_T2X_JITTER" gives SMAA T2x's two phases.
   // A positive value is the Halton phase count.
   constexpr int VANILLA_JITTER = -1;
   constexpr int NO_JITTER = -3;
   constexpr int STAMP_ONLY_JITTER = 0;
   constexpr int SMAA_T2X_JITTER = -2;
   std::atomic<int> g_jitter_phases = VANILLA_JITTER;
   // The "Anti-Aliasing" option the game's SMAA (and the motion blur's T2x mask) see in place of the game's: off under SR, SMAA or T2x
   // with our SMAA, and the game's own ("RENDER_AA_GAME") otherwise. We write it into the render thread's copy after each frame packet
   // copy, so the game's option, its menu and the screens that force SMAA keep their own value.
   constexpr int32_t RENDER_AA_GAME = -1;
   std::atomic<int32_t> g_render_aa_option = RENDER_AA_GAME;

   using JitterFunc = void (*)(uint8_t* view, float* jx, float* jy);
   JitterFunc jitter_original = nullptr;
   using FrameParamsFunc = void (*)(int64_t packet);
   FrameParamsFunc frame_params_original = nullptr;
   using SaveEntryFunc = bool (*)(const uint8_t* entry);
   SaveEntryFunc save_entry_is_file_original = nullptr;
   bool hooks_attempted = false;

   // The save scans (0x1880F70 and 0x18815A3) load every file of the save folder as a slot, but the two settings files. Steam Cloud's
   // "steam_autocloud.vdf" then becomes a slot that fails to load, the "corrupted save" warning at every start. Only the ".save" files the
   // game writes count as slots here (_tools/sunset_overdrive/NOTES.md).
   bool SaveEntryIsFileDetour(const uint8_t* entry)
   {
      if (!save_entry_is_file_original(entry))
         return false;
      if (!g_fix_corrupted_save_warning)
         return true;
      const std::wstring_view name = reinterpret_cast<const wchar_t*>(entry + 0x34); // WIN32_FIND_DATAW::cFileName
      constexpr std::wstring_view save_extension = L".save";
      return name.size() >= save_extension.size() && _wcsicmp(name.data() + name.size() - save_extension.size(), save_extension.data()) == 0;
   }

   void FrameParamsDetour(int64_t packet)
   {
      frame_params_original(packet);
      if (const int32_t aa_option = g_render_aa_option.load(std::memory_order_relaxed); aa_option != RENDER_AA_GAME)
      {
         *game_addresses.render_aa_option = aa_option;
      }
   }

   // With SR or our SMAA T2x, the main scene view gets our jitter and the frame stamp in view+0x120C, which the object velocity pass needs
   // with motion blur off. Only game camera views call it (not shadow, reflection or UI views).
   void JitterDetour(uint8_t* view, float* jx, float* jy)
   {
      const int phases = g_jitter_phases.load(std::memory_order_relaxed);
      if (phases == NO_JITTER)
      {
         *jx = 0.f;
         *jy = 0.f;
         return;
      }
      // Our SMAA T2x keeps the game's jitter conditions (debug render mode, skip flag), but not its fade out in motion (view+0x1210)
      if (phases == VANILLA_JITTER || (phases == SMAA_T2X_JITTER && (*game_addresses.debug_render_mode >= 2 || *game_addresses.skip_smaa != 0)))
      {
         jitter_original(view, jx, jy);
         return;
      }
      // Only the main scene view gets our jitter: the one with the shared render buffers and a perspective projection (flag bit 1 =
      // orthographic). The others get none, since the game's own would follow the game's option, which Luma never writes.
      if (*reinterpret_cast<const uint8_t* const*>(view + 0x1160) != game_addresses.shared_render_buffers || (*reinterpret_cast<const uint32_t*>(view + 0x364) & 2) != 0)
      {
         *jx = 0.f;
         *jy = 0.f;
         return;
      }
      const int32_t frame = *game_addresses.game_frame;
      *reinterpret_cast<int32_t*>(view + 0x120C) = frame;
      // SMAA T2x moves the projection by (-0.25, -0.25) then (0.25, 0.25) pixels (y down), for "SMAA.hlsl"'s subsample indices 1 and 2.
      // That is its table read with y up, which measured better (docs/SMAA-Lab.md, as in Borderlands 2).
      std::array<float, 2> offset = {};
      if (phases > 0)
      {
         const unsigned int phase = unsigned(frame) % unsigned(phases);
         offset = {SR::HaltonSequence(phase, 2), SR::HaltonSequence(phase, 3)};
      }
      else if (phases == SMAA_T2X_JITTER)
      {
         const float smaa_t2x_offset = ((frame & 1) == 0 ? -0.25f : 0.25f);
         offset = {smaa_t2x_offset, smaa_t2x_offset};
      }
      const JitterRecord record = {
         .frame = frame,
         .x = offset[0],
         .y = offset[1],
         // The camera block still holds the previous frame's projection here: close enough for FSR's near, far and FOV
         .near_plane = *reinterpret_cast<const float*>(view + 0x330),
         .far_plane = *reinterpret_cast<const float*>(view + 0x334),
         .tan_half_fov_y = *reinterpret_cast<const float*>(view + 0x35C),
      };
      // Over the view rect (the render size), NDC y up
      *jx = 2.f * record.x / *reinterpret_cast<const float*>(view + 0x388);
      *jy = -2.f * record.y / *reinterpret_cast<const float*>(view + 0x38C);
      const std::unique_lock lock(jitter_history.mutex);
      jitter_history.records[unsigned(frame) % jitter_history.records.size()] = record;
   }

   // The jitter the hook produced for the frame the render thread draws, if it ran for it
   std::optional<JitterRecord> FindRenderFrameJitter()
   {
      const int32_t render_frame = *game_addresses.render_frame;
      const std::shared_lock lock(jitter_history.mutex);
      const JitterRecord& record = jitter_history.records[unsigned(render_frame) % jitter_history.records.size()];
      if (record.frame != render_frame)
         return std::nullopt;
      return record;
   }
} // namespace

// A texture with a view per bind flag
struct TextureViews
{
   com_ptr<ID3D11Texture2D> texture;
   com_ptr<ID3D11RenderTargetView> rtv;
   com_ptr<ID3D11ShaderResourceView> srv;
   com_ptr<ID3D11UnorderedAccessView> uav;
};

// Keeps "views" if they already have the size, format and bind flags, else recreates them, all or nothing (cleared on failure)
static bool EnsureTextureViews(ID3D11Device* native_device, uint2 size, DXGI_FORMAT format, UINT bind_flags, TextureViews* views)
{
   if (views->texture)
   {
      D3D11_TEXTURE2D_DESC existing_desc;
      views->texture->GetDesc(&existing_desc);
      if (existing_desc.Width == size.x && existing_desc.Height == size.y && existing_desc.Format == format && existing_desc.BindFlags == bind_flags)
         return true;
   }
   *views = {};
   const D3D11_TEXTURE2D_DESC desc = {.Width = size.x, .Height = size.y, .MipLevels = 1, .ArraySize = 1, .Format = format, .SampleDesc = {.Count = 1}, .Usage = D3D11_USAGE_DEFAULT, .BindFlags = bind_flags};
   TextureViews created;
   if (FAILED(native_device->CreateTexture2D(&desc, nullptr, &created.texture)) ||
       ((bind_flags & D3D11_BIND_RENDER_TARGET) != 0 && FAILED(native_device->CreateRenderTargetView(created.texture.get(), nullptr, &created.rtv))) ||
       ((bind_flags & D3D11_BIND_SHADER_RESOURCE) != 0 && FAILED(native_device->CreateShaderResourceView(created.texture.get(), nullptr, &created.srv))) ||
       ((bind_flags & D3D11_BIND_UNORDERED_ACCESS) != 0 && FAILED(native_device->CreateUnorderedAccessView(created.texture.get(), nullptr, &created.uav))))
      return false;
   *views = created;
   return true;
}

struct SunsetOverdriveGameDeviceData final : public GameDeviceData
{
   bool sr_active = false; // Latched at present for the next frame
   bool sr_tried = false;  // SR's "Draw" ran this frame (once, also while Luma TAA compiles and passes the color through)
   // R16G16_FLOAT motion vectors at the velocity target's size, drawn by the velocity passes
   TextureViews motion_vectors;
   bool motion_vectors_drawn = false;           // This frame
   com_ptr<ID3D11ShaderResourceView> depth_srv; // This frame's D32S8 scene depth (t0 of the camera velocity pass)
   com_ptr<ID3D11Resource> exposure_downsample; // t5 of "CS_AverageLumAccum" when it last ran
   bool sr_output_recreated = false;
   // The scene before SR at the exposure downsample's size, drawn as the game's "PS_Tex" (bilinear), for the bloom's opaque/alpha split
   TextureViews bloom_split_scene;
   uint32_t bloom_split_scene_frame = UINT32_MAX; // FrameIndex it was drawn on
   JitterRecord camera;                           // The last one found for a drawn frame (FSR fails without a FOV)
   // The reactive mask (FSR and Luma TAA): the scene before the transparent phase, copied this frame, and the mask at its size
   TextureViews reactive_opaque_scene;
   bool reactive_opaque_scene_copied = false;
   TextureViews reactive_mask;
   com_ptr<ID3D11ShaderResourceView> exposure_buffer_srv; // The tonemap's g_AdaptedLumBuffer (t7, [1] = exposure), the last time it ran
   TextureViews sr_exposure;                              // FSR's 1x1 exposure (see "Luma_SO_SRExposure.hlsl")

   // RCAS sharpens the tonemap's output into this, which the copy onto the swapchain then reads
   TextureViews rcas_output;

   // SMAA's predication edge-ness at the scene's size
   TextureViews smaa_predication;
   uint32_t smaa_last_frame = 0;                     // FrameIndex of SMAA's last run, for "SMAA_IDLE_RELEASE_FRAMES"
   uint32_t smaa_compute_skipped_frame = UINT32_MAX; // FrameIndex of the last frame the game's SMAA edges and weights were skipped

   // Whether our SMAA T2x is on, latched at present. "t2x_frames" holds SMAA's output of this frame and of the previous one,
   // alternating, as gamma 2.2 decoded RGB with the velocity length in alpha. The previous one is the history only if "t2x_frame", the
   // last frame the resolve ran, was the frame before.
   bool t2x_active = false;
   std::array<TextureViews, 2> t2x_frames;
   uint32_t t2x_frame = UINT32_MAX;
};

class SunsetOverdrive final : public Game
{
public:
   void OnInit(bool async) override
   {
      GetShaderDefineData(POST_PROCESS_SPACE_TYPE_HASH).SetDefaultValue('0');
      GetShaderDefineData(EARLY_DISPLAY_ENCODING_HASH).SetDefaultValue('0');
      GetShaderDefineData(VANILLA_ENCODING_TYPE_HASH).SetDefaultValue('0');
      GetShaderDefineData(GAMMA_CORRECTION_TYPE_HASH).SetDefaultValue('1');
      // The scene copy and videos onto the swapchain pre-scale by GamePaperWhite/UIPaperWhite, so the Scaleform UI drawn raw on top
      // gets its own paper white (Includes/Common.hlsl)
      GetShaderDefineData(UI_DRAW_TYPE_HASH).SetDefaultValue('2');
      // 0 stays selectable as the vanilla reference every HDR change gets compared against (Includes/ToneMap.hlsl)
      std::vector<ShaderDefineData> game_shader_defines_data = {
         {"TONEMAP_TYPE", '1', true, false, "0 - Vanilla SDR\n1 - Luma HDR (Vanilla+)", 1},
      };
      shader_defines_data.append_range(game_shader_defines_data);
      assert(shader_defines_data.size() < MAX_SHADER_DEFINES);

      // The embedded engine shaders bind b0-b3 (material shaders are unverified)
      luma_settings_cbuffer_index = 13;
      luma_data_cbuffer_index = 12;
      luma_ui_cbuffer_index = -1;

      // Manual paper white sliders, not the OS reference level. Core shows the UI one on UI_DRAW_TYPE >= 1.
      use_os_reference_white_level = false;

      default_luma_global_game_settings = {
         .Dithering = 1.f,
         .ColorGradingIntensity = 1.f,
         .BloomIntensity = 1.f,
         .LensFlareIntensity = 1.f,
         .VignetteIntensity = 1.f,
         .FilmGrainIntensity = 1.f,
         .Exposure = 1.f,
         .Contrast = 1.f,
         .Saturation = 1.f,
         .HighlightsDesaturation = 0.f,
         .VideoAutoHDREnable = 1.f,
         .VideoAutoHDRBoost = 0.5f,
      };
      cb_luma_global_settings.GameSettings = default_luma_global_game_settings;

      native_shaders_definitions.emplace(shader_hash_SmaaPredication, ShaderDefinition{"Luma_SO_SMAAPredication", reshade::api::pipeline_subobject_type::compute_shader});
      // SMAA T2x's own passes ("SMAA_T2X" in "Luma_SMAA_impl.hlsl"). The edge detection and the vertex shaders are 1x's.
      native_shaders_definitions.emplace(shader_hash_SmaaT2xWeights, ShaderDefinition{"Luma_SMAA_impl", reshade::api::pipeline_subobject_type::pixel_shader, nullptr, "smaa_blending_weight_calculation_ps", {{"SMAA_T2X", "1"}}});
      native_shaders_definitions.emplace(shader_hash_SmaaT2xBlend, ShaderDefinition{"Luma_SMAA_impl", reshade::api::pipeline_subobject_type::pixel_shader, nullptr, "smaa_neighborhood_blending_ps", {{"SMAA_T2X", "1"}}});
      native_shaders_definitions.emplace(shader_hash_SmaaT2xResolve, ShaderDefinition{"Luma_SMAA_impl", reshade::api::pipeline_subobject_type::pixel_shader, nullptr, "smaa_resolve_ps", {{"SMAA_T2X", "1"}}});
      native_shaders_definitions.emplace(shader_hash_ReactiveMask, ShaderDefinition{"Luma_SO_ReactiveMask", reshade::api::pipeline_subobject_type::compute_shader});
      native_shaders_definitions.emplace(shader_hash_SRExposure, ShaderDefinition{"Luma_SO_SRExposure", reshade::api::pipeline_subobject_type::compute_shader});

#if DEVELOPMENT
      // For the MCP "luma_dev_values" tool
      Mcp::RegisterToggles({{"hide_ui", &g_hide_ui}, {"smaa_enable", &g_smaa_enable}, {"smaa_t2x", &g_smaa_t2x}, {"sr_flip_jitter_x", &g_sr_flip_jitter_x}, {"sr_flip_jitter_y", &g_sr_flip_jitter_y}, { "sr_reactive_enable",
                               &g_sr_reactive_enable }});
      Mcp::RegisterValues({{"sr_reactive_scale", &g_sr_reactive_scale, 0.f, 4.f}, {"sr_reactive_threshold", &g_sr_reactive_threshold, 0.f, 1.f}});
      Mcp::RegisterTextures({MCP_GAME_TEXTURE("sr.motion_vectors", motion_vectors.texture), MCP_GAME_TEXTURE("sr.reactive_mask", reactive_mask.texture), MCP_GAME_TEXTURE("sr.reactive_opaque_scene", reactive_opaque_scene.texture), MCP_GAME_TEXTURE("sr.exposure", sr_exposure.texture)});
#endif
   }

   static SunsetOverdriveGameDeviceData& GetGameDeviceData(DeviceData& device_data)
   {
      return *static_cast<SunsetOverdriveGameDeviceData*>(device_data.game);
   }

   void OnCreateDevice(ID3D11Device* native_device, DeviceData& device_data) override
   {
      device_data.game = new SunsetOverdriveGameDeviceData;
      device_data.taa_detected = true; // No TAA to replace, but Core's upscaler status checks for it
   }

   void OnDestroyDeviceData(DeviceData& device_data) override
   {
      // GameDeviceData has no virtual destructor
      delete static_cast<SunsetOverdriveGameDeviceData*>(device_data.game);
      device_data.game = nullptr;
   }

   // Draws a velocity pass with the motion vectors added at "slot" (the shaders' extra output), then restores the game's targets
   static DrawOrDispatchOverrideType DrawVelocityWithMotionVectors(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, SunsetOverdriveGameDeviceData* game_device_data, std::function<void()>* original_draw_dispatch_func, UINT slot)
   {
      com_ptr<ID3D11RenderTargetView> rtvs[2];
      com_ptr<ID3D11DepthStencilView> dsv;
      native_device_context->OMGetRenderTargets(slot, &rtvs[0], &dsv);
      ID3D11RenderTargetView* const velocity_rtv = rtvs[slot - 1].get();
      if (!velocity_rtv || !original_draw_dispatch_func || !*original_draw_dispatch_func)
         return DrawOrDispatchOverrideType::None;
      // The camera pass runs first and starts the frame's motion vectors at the velocity target's size
      if (slot == 2)
      {
         const uint2 size = GetViewTextureSize(velocity_rtv);
         if (!EnsureTextureViews(native_device, size, DXGI_FORMAT_R16G16_FLOAT, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET, &game_device_data->motion_vectors))
            return DrawOrDispatchOverrideType::None;
         // Stencil rejected pixels (depth-stencil state 1 is unverified) stay still
         constexpr float zero[4] = {};
         native_device_context->ClearRenderTargetView(game_device_data->motion_vectors.rtv.get(), zero);
         game_device_data->motion_vectors_drawn = true;
      }
      else if (!game_device_data->motion_vectors_drawn)
      {
         return DrawOrDispatchOverrideType::None;
      }
      ID3D11RenderTargetView* draw_rtvs[] = {rtvs[0].get(), rtvs[1].get(), nullptr};
      draw_rtvs[slot] = game_device_data->motion_vectors.rtv.get();
      native_device_context->OMSetRenderTargets(slot + 1, draw_rtvs, dsv.get());
      (*original_draw_dispatch_func)();
      native_device_context->OMSetRenderTargets(slot, draw_rtvs, dsv.get());
      return DrawOrDispatchOverrideType::Replaced;
   }

   // FSR and Luma TAA read the reactive mask (DLSS's presets ignore it)
   static bool UsesReactiveMask(const DeviceData& device_data)
   {
      return g_sr_reactive_enable && (device_data.sr_type == SR::Type::FSR || device_data.sr_type == SR::Type::LumaTAA);
   }

   // The reactive mask from the scene's change since the copy before the transparent phase (see "Luma_SO_ReactiveMask.hlsl")
   static bool DrawReactiveMask(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, CommandListData& cmd_list_data, DeviceData& device_data, SunsetOverdriveGameDeviceData* game_device_data, ID3D11ShaderResourceView* scene_srv, uint2 size)
   {
      if (GetViewTextureSize(game_device_data->reactive_opaque_scene.srv.get()) != size)
         return false;
      if (!EnsureTextureViews(native_device, size, DXGI_FORMAT_R8_UNORM, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, &game_device_data->reactive_mask))
         return false;
      const std::shared_lock lock_shader_objects(s_mutex_shader_objects);
      auto* const reactive_mask_cs = FindShader(device_data.native_compute_shaders, shader_hash_ReactiveMask);
      if (!reactive_mask_cs)
         return false;
      DrawStateStack<DrawStateStackType::Compute> compute_state;
      compute_state.Cache(native_device_context, device_data.uav_max_count);
      SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, reshade::api::shader_stage::compute, LumaConstantBufferType::LumaData, 0, 0, g_sr_reactive_scale, g_sr_reactive_threshold);
      // UAV first: the scene can't be bound as one, but nothing else may alias it
      ID3D11UnorderedAccessView* const uav = game_device_data->reactive_mask.uav.get();
      native_device_context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);
      ID3D11ShaderResourceView* const srvs[3] = {game_device_data->reactive_opaque_scene.srv.get(), scene_srv, game_device_data->exposure_buffer_srv.get()};
      native_device_context->CSSetShaderResources(0, 3, srvs);
      native_device_context->CSSetShader(reactive_mask_cs, nullptr, 0);
      native_device_context->Dispatch((size.x + 7) / 8, (size.y + 7) / 8, 1);
      compute_state.Restore(native_device_context);
      return true;
   }

   // The reactive mask's "before": the scene after the fog and the occlusion dropouts, before the transparents and VFX (see
   // "shader_hash_OcclCoverDropouts")
   static void CopyReactiveOpaqueScene(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, ID3D11Resource* scene, SunsetOverdriveGameDeviceData* game_device_data)
   {
      com_ptr<ID3D11Texture2D> scene_texture;
      if (!game_device_data->motion_vectors_drawn || !scene || FAILED(scene->QueryInterface(&scene_texture)))
         return;
      D3D11_TEXTURE2D_DESC scene_desc;
      scene_texture->GetDesc(&scene_desc);
      const uint2 size = {scene_desc.Width, scene_desc.Height};
      if (scene_desc.Format != DXGI_FORMAT_R11G11B10_FLOAT || size != GetViewTextureSize(game_device_data->motion_vectors.rtv.get()))
         return;
      if (!EnsureTextureViews(native_device, size, DXGI_FORMAT_R11G11B10_FLOAT, D3D11_BIND_SHADER_RESOURCE, &game_device_data->reactive_opaque_scene))
         return;
      native_device_context->CopyResource(game_device_data->reactive_opaque_scene.texture.get(), scene);
      game_device_data->reactive_opaque_scene_copied = true;
   }

   // DLSS, FSR 3 or Luma TAA on the scene before the post chain reads it, written back in place. "split_bloom" means the bloom's bright
   // passes are still to come, which isn't the case at the tonemap.
   static void DrawSR(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, CommandListData& cmd_list_data, DeviceData& device_data, SunsetOverdriveGameDeviceData* game_device_data, ID3D11ShaderResourceView* scene_srv, bool split_bloom)
   {
      if (!scene_srv || !game_device_data->motion_vectors_drawn || !game_device_data->depth_srv)
         return;
      com_ptr<ID3D11Resource> depth;
      game_device_data->depth_srv->GetResource(&depth);
      com_ptr<ID3D11Resource> scene_resource;
      scene_srv->GetResource(&scene_resource);
      com_ptr<ID3D11Texture2D> scene;
      if (FAILED(scene_resource->QueryInterface(&scene)))
         return;
      D3D11_TEXTURE2D_DESC scene_desc;
      scene->GetDesc(&scene_desc);
      SR::InstanceData* const sr_instance_data = device_data.GetSRInstanceData();
      const uint2 scene_size = {scene_desc.Width, scene_desc.Height};
      const bool scene_matches_motion_vectors = scene_desc.Format == DXGI_FORMAT_R11G11B10_FLOAT && scene_size == GetViewTextureSize(game_device_data->motion_vectors.rtv.get());
      if (!sr_instance_data || !scene_matches_motion_vectors || (std::min)(scene_size.x, scene_size.y) < sr_instance_data->min_resolution)
         return;

      // The scene's format, so the output is copied back as is
      D3D11_TEXTURE2D_DESC output_desc = {};
      if (device_data.sr_output_color)
      {
         device_data.sr_output_color->GetDesc(&output_desc);
      }
      if (output_desc.Width != scene_desc.Width || output_desc.Height != scene_desc.Height)
      {
         device_data.sr_output_color = nullptr;
         output_desc = {.Width = scene_desc.Width, .Height = scene_desc.Height, .MipLevels = 1, .ArraySize = 1, .Format = scene_desc.Format, .SampleDesc = {.Count = 1}, .Usage = D3D11_USAGE_DEFAULT, .BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS};
         if (FAILED(native_device->CreateTexture2D(&output_desc, nullptr, &device_data.sr_output_color)))
         {
            device_data.sr_suppressed = true; // Back to the anti-aliasing without SR until SR is picked again
            return;
         }
         game_device_data->sr_output_recreated = true;
      }

      const SR::SettingsData settings_data = {
         .output_width = scene_desc.Width,
         .output_height = scene_desc.Height,
         .render_width = scene_desc.Width,
         .render_height = scene_desc.Height,
         .hdr = true, // Linear, scene referred, not pre-exposed
         .inverted_depth = true,
         // Render pixels, previous minus current, jitter removed (see the velocity shaders)
         .mvs_jittered = false,
         // DLSS's own (DLSS-Best-Practices EXP-4), and Luma TAA's. FSR's clips highlights, so it gets the tonemap's exposure below.
         .auto_exposure = device_data.sr_type != SR::Type::FSR,
         // "Default" is K for DLAA (L and M crush hues)
         .render_preset = (dlss_render_preset != 0 ? dlss_render_preset : 11u),
      };
      sr_implementations[device_data.sr_type]->UpdateSettings(sr_instance_data, native_device_context, settings_data);

      // The jitter of the frame the render thread draws; none (still a valid input) if the hook didn't run for it
      const std::optional<JitterRecord> render_frame_jitter = FindRenderFrameJitter();
      const JitterRecord jitter = render_frame_jitter.value_or(JitterRecord{});
      if (render_frame_jitter)
      {
         game_device_data->camera = *render_frame_jitter;
      }
      const JitterRecord& camera = game_device_data->camera;
      SR::SuperResolutionImpl::DrawData draw_data = {
         .reset = device_data.force_reset_sr || game_device_data->sr_output_recreated,
         .output_color = device_data.sr_output_color.get(),
         .source_color = scene.get(),
         .motion_vectors = game_device_data->motion_vectors.texture.get(),
         .depth_buffer = depth.get(),
         .render_width = scene_desc.Width,
         .render_height = scene_desc.Height,
         // As applied to the projection (pixels, +y down)
         .jitter_x = jitter.x * (g_sr_flip_jitter_x ? -1.f : 1.f),
         .jitter_y = jitter.y * (g_sr_flip_jitter_y ? -1.f : 1.f),
         .vert_fov = (camera.tan_half_fov_y > 0.f ? 2.f * std::atan(camera.tan_half_fov_y) : (60.f * float(M_PI) / 180.f)), // FSR fails without it
         .frame_index = cb_luma_global_settings.FrameIndex,
      };
      if (camera.near_plane > 0.f && camera.far_plane > camera.near_plane)
      {
         draw_data.near_plane = camera.near_plane;
         draw_data.far_plane = camera.far_plane;
      }
      if (game_device_data->reactive_opaque_scene_copied && UsesReactiveMask(device_data) && DrawReactiveMask(native_device, native_device_context, cmd_list_data, device_data, game_device_data, scene_srv, scene_size))
      {
         draw_data.bias_mask = game_device_data->reactive_mask.texture.get();
      }
      // FSR's exposure is the tonemap's own multiplier (FSR-Best-Practices FIN-4, see "Luma_SO_SRExposure.hlsl")
      const bool fsr_exposure = device_data.sr_type == SR::Type::FSR && game_device_data->exposure_buffer_srv;
      if (fsr_exposure && EnsureTextureViews(native_device, uint2{1, 1}, DXGI_FORMAT_R32_FLOAT, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, &game_device_data->sr_exposure))
      {
         const std::shared_lock lock_shader_objects(s_mutex_shader_objects);
         if (auto* const sr_exposure_cs = FindShader(device_data.native_compute_shaders, shader_hash_SRExposure))
         {
            DrawStateStack<DrawStateStackType::Compute> compute_state;
            compute_state.Cache(native_device_context, device_data.uav_max_count);
            SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, reshade::api::shader_stage::compute, LumaConstantBufferType::LumaSettings);
            ID3D11UnorderedAccessView* const uav = game_device_data->sr_exposure.uav.get();
            native_device_context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);
            ID3D11ShaderResourceView* const srv = game_device_data->exposure_buffer_srv.get();
            native_device_context->CSSetShaderResources(0, 1, &srv);
            native_device_context->CSSetShader(sr_exposure_cs, nullptr, 0);
            native_device_context->Dispatch(1, 1, 1);
            compute_state.Restore(native_device_context);
            draw_data.exposure = game_device_data->sr_exposure.texture.get();
         }
      }

      // NGX and FFX don't restore the state they touch
      DrawStateStack<DrawStateStackType::FullGraphics> graphics_state;
      DrawStateStack<DrawStateStackType::Compute> compute_state;
      graphics_state.Cache(native_device_context, device_data.uav_max_count);
      compute_state.Cache(native_device_context, device_data.uav_max_count);
      game_device_data->sr_tried = true;
      const bool drawn = sr_implementations[device_data.sr_type]->Draw(sr_instance_data, native_device_context, draw_data);
      // Luma TAA passes the color through while its shaders compile
      const bool upscaled = drawn && sr_implementations[device_data.sr_type]->IsReady(sr_instance_data);
      // The bloom's opaque/alpha split compares the scene with the jittered mid-forward copy. Fed SR's output, the anti-aliased edges
      // would leak into the alpha bloom and shimmer with the jitter, so those passes read the scene before SR, as in vanilla.
      if (com_ptr<ID3D11Texture2D> exposure_downsample; upscaled && split_bloom && game_device_data->exposure_downsample && SUCCEEDED(game_device_data->exposure_downsample->QueryInterface(&exposure_downsample)))
      {
         D3D11_TEXTURE2D_DESC exposure_downsample_desc;
         exposure_downsample->GetDesc(&exposure_downsample_desc);
         const uint2 size = {exposure_downsample_desc.Width, exposure_downsample_desc.Height};
         const std::shared_lock lock_shader_objects(s_mutex_shader_objects);
         auto* const scale_vs = FindShader(device_data.native_vertex_shaders, CompileTimeStringHash("Scale VS"));
         auto* const scale_ps = FindShader(device_data.native_pixel_shaders, CompileTimeStringHash("Scale PS"));
         if (scale_vs && scale_ps && EnsureTextureViews(native_device, size, DXGI_FORMAT_R11G11B10_FLOAT, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET, &game_device_data->bloom_split_scene))
         {
            native_device_context->OMSetRenderTargetsAndUnorderedAccessViews(0, nullptr, nullptr, 0, 0, nullptr, nullptr);
            DrawCustomPixelShader(native_device_context, device_data.default_depth_stencil_state.get(), device_data.default_blend_state.get(), device_data.sampler_state_linear.get(), scale_vs, scale_ps, scene_srv, game_device_data->bloom_split_scene.rtv.get(), size.x, size.y);
            game_device_data->bloom_split_scene_frame = cb_luma_global_settings.FrameIndex;
         }
      }
      compute_state.Restore(native_device_context);
      graphics_state.Restore(native_device_context);
      if (!drawn)
      {
         device_data.sr_suppressed = true;
         return;
      }
      if (!upscaled)
         return;
      native_device_context->CopyResource(scene.get(), device_data.sr_output_color.get());
      // DLSS draws nothing into a new output until its feature is created again after a draw (DLSS-Best-Practices OUT-4). A settings
      // change forces that at the next "UpdateSettings".
      if (std::exchange(game_device_data->sr_output_recreated, false) && device_data.sr_type == SR::Type::DLSS)
      {
         SR::SettingsData throwaway_settings_data = settings_data;
         throwaway_settings_data.mvs_jittered = !throwaway_settings_data.mvs_jittered;
         sr_implementations[device_data.sr_type]->UpdateSettings(sr_instance_data, native_device_context, throwaway_settings_data);
      }
      device_data.has_drawn_sr = true;
   }

   // Sharpens the tonemap's output (t6 of the copy onto the swapchain) with RCAS into a texture, which the copy then reads instead
   static void DrawRCASOnSwapchainCopy(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, DeviceData& device_data, SunsetOverdriveGameDeviceData* game_device_data)
   {
      com_ptr<ID3D11ShaderResourceView> source_srv;
      native_device_context->PSGetShaderResources(6, 1, &source_srv);
      if (!source_srv)
         return;
      D3D11_SHADER_RESOURCE_VIEW_DESC source_srv_desc;
      source_srv->GetDesc(&source_srv_desc);
      const uint2 size = GetViewTextureSize(source_srv.get());
      if (source_srv_desc.ViewDimension != D3D11_SRV_DIMENSION_TEXTURE2D || size.x == 0 || size.y == 0)
         return;

      // "DrawRCAS()" looks its shaders up with ".at()": a shader reload must not release them meanwhile
      const std::shared_lock lock_shader_objects(s_mutex_shader_objects);
      if (!PrepareRCAS(native_device, device_data))
         return;
      // The source's view format, so the copy reads the same encoding
      if (!EnsureTextureViews(native_device, size, source_srv_desc.Format, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET, &game_device_data->rcas_output))
         return;
      DrawRCAS(native_device_context, device_data, source_srv.get(), game_device_data->rcas_output.rtv.get(), g_rcas_sharpness);
      ID3D11ShaderResourceView* const rcas_srv = game_device_data->rcas_output.srv.get();
      native_device_context->PSSetShaderResources(6, 1, &rcas_srv);
   }

   // Core's SMAA in place of the game's blend draw: it reads the blend's color input (t5) and writes the blend's target. Returns false
   // when an input or a shader is missing.
   static bool DrawSMAAInPlaceOfBlend(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, CommandListData& cmd_list_data, DeviceData& device_data, SunsetOverdriveGameDeviceData* game_device_data)
   {
      com_ptr<ID3D11RenderTargetView> rtv;
      native_device_context->OMGetRenderTargets(1, &rtv, nullptr);
      com_ptr<ID3D11ShaderResourceView> color_srv;
      native_device_context->PSGetShaderResources(5, 1, &color_srv);
      if (!rtv || !color_srv)
         return false;
      const uint2 size = GetViewTextureSize(color_srv.get());
      if (size != GetViewTextureSize(rtv.get()) || size.x == 0 || size.y == 0)
         return false;

      // Held through SMAA so a shader reload cannot release them mid-use; "DrawSMAA" looks its shaders up with "at"
      const std::shared_lock lock_shader_objects(s_mutex_shader_objects);
      if (!HasSMAAShaders(device_data))
         return false;
      game_device_data->smaa_last_frame = cb_luma_global_settings.FrameIndex;

      // Predication from this frame's scene depth at the same size; without it plain ULTRA
      const bool predication = game_device_data->depth_srv && GetViewTextureSize(game_device_data->depth_srv.get()) == size && HasShaders(device_data.native_compute_shaders, shader_hash_SmaaPredication) &&
                               EnsureTextureViews(native_device, size, DXGI_FORMAT_R16_FLOAT, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, &game_device_data->smaa_predication);

      if (predication)
      {
         DrawStateStack<DrawStateStackType::Compute> compute_state;
         compute_state.Cache(native_device_context, device_data.uav_max_count);
         // Unbind the render targets: were the depth still bound as the DSV, D3D11 would silently null its SRV in the predication pass
         com_ptr<ID3D11RenderTargetView> rtvs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
         com_ptr<ID3D11DepthStencilView> dsv;
         native_device_context->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, &rtvs[0], &dsv);
         native_device_context->OMSetRenderTargets(0, nullptr, nullptr);

         ID3D11UnorderedAccessView* const predication_uav = game_device_data->smaa_predication.uav.get();
         ID3D11ShaderResourceView* const raw_depth_srv = game_device_data->depth_srv.get();
         native_device_context->CSSetUnorderedAccessViews(0, 1, &predication_uav, nullptr);
         native_device_context->CSSetShaderResources(0, 1, &raw_depth_srv);
         native_device_context->CSSetShader(device_data.native_compute_shaders.at(shader_hash_SmaaPredication).get(), nullptr, 0);
         native_device_context->Dispatch((size.x + 7) / 8, (size.y + 7) / 8, 1);

         native_device_context->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, reinterpret_cast<ID3D11RenderTargetView* const*>(&rtvs[0]), dsv.get());
         compute_state.Restore(native_device_context);
      }

      // SMAA T2x draws SMAA into this frame's linear target with the velocity, then the resolve with the previous frame into the blend's
      // target. It needs this frame's motion vectors at the same size. Without them this frame is SMAA 1x, which the game's own resolve
      // or copy then carries on.
      auto* const copy_vs = FindShader(device_data.native_vertex_shaders, shader_hash_CopyVS);
      auto* const t2x_weights_ps = FindShader(device_data.native_pixel_shaders, shader_hash_SmaaT2xWeights);
      auto* const t2x_blend_ps = FindShader(device_data.native_pixel_shaders, shader_hash_SmaaT2xBlend);
      auto* const t2x_resolve_ps = FindShader(device_data.native_pixel_shaders, shader_hash_SmaaT2xResolve);
      bool t2x = game_device_data->t2x_active && game_device_data->motion_vectors_drawn && GetViewTextureSize(game_device_data->motion_vectors.srv.get()) == size && copy_vs && t2x_weights_ps && t2x_blend_ps && t2x_resolve_ps;
      const uint32_t frame_index = cb_luma_global_settings.FrameIndex;
      const int t2x_current = int(frame_index & 1);
      bool t2x_history = game_device_data->t2x_frame + 1 == frame_index;
      auto& t2x_frames = game_device_data->t2x_frames;
      if (t2x && (GetViewTextureSize(t2x_frames[0].srv.get()) != size || GetViewTextureSize(t2x_frames[1].srv.get()) != size))
      {
         t2x_history = false;
         constexpr UINT bind_flags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
         t2x = EnsureTextureViews(native_device, size, DXGI_FORMAT_R16G16B16A16_FLOAT, bind_flags, &t2x_frames[0]) && EnsureTextureViews(native_device, size, DXGI_FORMAT_R16G16B16A16_FLOAT, bind_flags, &t2x_frames[1]);
      }
      // The area texture subsamples of the jitter phase the frame was drawn with (see "JitterDetour"), 0 if it didn't jitter
      float subsample_indices = 0.f;
      if (t2x)
      {
         if (const std::optional<JitterRecord> jitter = FindRenderFrameJitter(); jitter && jitter->x != 0.f)
         {
            subsample_indices = (jitter->x < 0.f ? 1.f : 2.f);
         }
      }

      // The SMAA shaders read the target size, the predication scale and T2x's subsample indices from the Luma data (the render
      // resolution isn't the swapchain's)
      SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, reshade::api::shader_stage::vertex | reshade::api::shader_stage::pixel, LumaConstantBufferType::LumaSettings);
      SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, reshade::api::shader_stage::vertex | reshade::api::shader_stage::pixel, LumaConstantBufferType::LumaData, size.x, size.y, (predication ? 2.f : 1.f), subsample_indices);
      // The neighborhood blend reads the gamma input too and filters it in linear light itself (see "SMAA_NEIGHBORHOOD_GAMMA_IN_LINEAR")
      const SMAAT2xPasses t2x_passes = {.blending_weight_calculation_ps = t2x_weights_ps, .neighborhood_blending_ps = t2x_blend_ps, .velocity = game_device_data->motion_vectors.srv.get()};
      DrawSMAA(native_device, native_device_context, device_data, (t2x ? t2x_frames[t2x_current].rtv.get() : rtv.get()), color_srv.get(), color_srv.get(), (predication ? game_device_data->smaa_predication.srv.get() : nullptr), nullptr, (t2x ? &t2x_passes : nullptr));
      if (t2x)
      {
         // The resolve: t0 this frame, t1 the previous one or this one again without a history, t2 the motion vectors, s0 linear and
         // s1 point
         DrawStateStack<DrawStateStackType::FullGraphics> resolve_state;
         resolve_state.Cache(native_device_context, device_data.uav_max_count);
         ID3D11ShaderResourceView* const resolve_srvs[2] = {t2x_frames[t2x_history ? (t2x_current ^ 1) : t2x_current].srv.get(), game_device_data->motion_vectors.srv.get()};
         native_device_context->PSSetShaderResources(1, 2, resolve_srvs);
         ID3D11SamplerState* const resolve_samplers[2] = {device_data.sampler_state_linear.get(), device_data.sampler_state_point.get()};
         native_device_context->PSSetSamplers(0, 2, resolve_samplers);
         DrawCustomPixelShader(native_device_context, device_data.default_depth_stencil_state.get(), device_data.default_blend_state.get(), nullptr, copy_vs, t2x_resolve_ps, t2x_frames[t2x_current].srv.get(), rtv.get(), size.x, size.y, false);
         resolve_state.Restore(native_device_context);
         game_device_data->t2x_frame = frame_index;
      }
      return true;
   }

   // Draws a plain copy of the bound t5 into the bound target, in place of the draw. We use it for the game's T2x resolve after ours,
   // where t5 is the blend's temp that ours wrote, and for the game's blend when its edges and weights were skipped but ours couldn't
   // draw. There t5 is a copy of the target with SMAA 1x, or with T2x the tonemap's output, which the game's resolve or copy carries on.
   static bool DrawColorInputCopy(ID3D11DeviceContext* native_device_context, const DeviceData& device_data)
   {
      com_ptr<ID3D11RenderTargetView> rtv;
      native_device_context->OMGetRenderTargets(1, &rtv, nullptr);
      com_ptr<ID3D11ShaderResourceView> srv;
      native_device_context->PSGetShaderResources(5, 1, &srv);
      const std::shared_lock lock_shader_objects(s_mutex_shader_objects);
      auto* const copy_vs = FindShader(device_data.native_vertex_shaders, shader_hash_CopyVS);
      auto* const copy_ps = FindShader(device_data.native_pixel_shaders, CompileTimeStringHash("Copy PS"));
      const uint2 size = GetViewTextureSize(rtv.get());
      if (!rtv || !srv || !copy_vs || !copy_ps || size != GetViewTextureSize(srv.get()))
         return false;
      DrawStateStack<DrawStateStackType::FullGraphics> state;
      state.Cache(native_device_context, device_data.uav_max_count);
      DrawCustomPixelShader(native_device_context, device_data.default_depth_stencil_state.get(), device_data.default_blend_state.get(), nullptr, copy_vs, copy_ps, srv.get(), rtv.get(), size.x, size.y);
      state.Restore(native_device_context);
      return true;
   }

   DrawOrDispatchOverrideType OnDrawOrDispatch(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, CommandListData& cmd_list_data, DeviceData& device_data, reshade::api::shader_stage stages, const ShaderHashesList<OneShaderPerPipeline>& original_shader_hashes, bool is_custom_pass, bool& updated_cbuffers, std::function<void()>* original_draw_dispatch_func) override
   {
      auto& game_device_data = GetGameDeviceData(device_data);
      if (original_shader_hashes.Contains(shader_hash_AverageLumAccum, reshade::api::shader_stage::compute))
      {
         game_device_data.exposure_downsample = nullptr;
         com_ptr<ID3D11ShaderResourceView> srv;
         native_device_context->CSGetShaderResources(5, 1, &srv);
         if (srv)
         {
            srv->GetResource(&game_device_data.exposure_downsample);
         }
         return DrawOrDispatchOverrideType::None;
      }
      // The game's SMAA edges and weights aren't needed while Core's SMAA replaces its blend (which turns into a copy should ours fail)
      if (original_shader_hashes.Contains(shader_hash_SmaaFindEdges, reshade::api::shader_stage::compute) || original_shader_hashes.Contains(shader_hash_SmaaComputeWeights, reshade::api::shader_stage::compute))
      {
         const std::shared_lock lock_shader_objects(s_mutex_shader_objects);
         if (!g_smaa_enable || !HasSMAAShaders(device_data))
            return DrawOrDispatchOverrideType::None;
         game_device_data.smaa_compute_skipped_frame = cb_luma_global_settings.FrameIndex;
         return DrawOrDispatchOverrideType::Skip;
      }
      if ((stages & reshade::api::shader_stage::pixel) != reshade::api::shader_stage::pixel)
         return DrawOrDispatchOverrideType::None;
      const auto is_pixel_shader = [&](uint32_t hash)
      { return original_shader_hashes.Contains(hash, reshade::api::shader_stage::pixel); };
      const auto render_target_resource = [&]()
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
      const auto targets_back_buffer = [&]()
      {
         const com_ptr<ID3D11Resource> resource = render_target_resource();
         const std::shared_lock lock(device_data.mutex);
         return resource && device_data.back_buffers.contains(reinterpret_cast<uint64_t>(resource.get()));
      };

      if (is_pixel_shader(shader_hash_CameraVelocity))
      {
         game_device_data.depth_srv = nullptr;
         native_device_context->PSGetShaderResources(0, 1, &game_device_data.depth_srv);
      }
      if (g_smaa_enable && is_pixel_shader(shader_hash_SmaaApplyBlend))
      {
         if (DrawSMAAInPlaceOfBlend(native_device, native_device_context, cmd_list_data, device_data, &game_device_data))
         {
            updated_cbuffers = true;
            return DrawOrDispatchOverrideType::Replaced;
         }
         if (game_device_data.smaa_compute_skipped_frame == cb_luma_global_settings.FrameIndex && DrawColorInputCopy(native_device_context, device_data))
            return DrawOrDispatchOverrideType::Replaced;
      }
      if (game_device_data.t2x_frame == cb_luma_global_settings.FrameIndex && is_pixel_shader(shader_hash_SmaaResolveReproj) && DrawColorInputCopy(native_device_context, device_data))
         return DrawOrDispatchOverrideType::Replaced;
      // The velocity passes add the motion vectors for SR and SMAA T2x
      const bool motion_vectors = (game_device_data.sr_active || game_device_data.t2x_active) && native_device_context->GetType() == D3D11_DEVICE_CONTEXT_IMMEDIATE;
      if (motion_vectors && is_pixel_shader(shader_hash_CameraVelocity))
         return DrawVelocityWithMotionVectors(native_device, native_device_context, &game_device_data, original_draw_dispatch_func, 2);
      if (motion_vectors && is_pixel_shader(shader_hash_ObjectVelocity))
         return DrawVelocityWithMotionVectors(native_device, native_device_context, &game_device_data, original_draw_dispatch_func, 1);
      const bool is_tonemap = shader_hashes_ToneMap.Contains(original_shader_hashes.pixel_shaders[0], reshade::api::shader_stage::pixel);
      if (is_tonemap)
      {
         game_device_data.exposure_buffer_srv = nullptr;
         native_device_context->PSGetShaderResources(7, 1, &game_device_data.exposure_buffer_srv);
      }
      if (game_device_data.sr_active && motion_vectors && !game_device_data.sr_tried && !device_data.sr_suppressed)
      {
         UINT scene_slot = 0;
         const bool needs_reactive_copy = !game_device_data.reactive_opaque_scene_copied && UsesReactiveMask(device_data);
         if (is_pixel_shader(shader_hash_MotionBlurDownsampleColor))
         {
            scene_slot = 5;
         }
         else if (is_pixel_shader(shader_hash_OcclCoverDropouts))
         {
            // The copy comes after its draw, from its target (the scene)
            if (needs_reactive_copy && original_draw_dispatch_func && *original_draw_dispatch_func)
            {
               (*original_draw_dispatch_func)();
               CopyReactiveOpaqueScene(native_device, native_device_context, render_target_resource().get(), &game_device_data);
               return DrawOrDispatchOverrideType::Replaced;
            }
         }
         else if (is_pixel_shader(shader_hash_Tex))
         {
            // A generic copy: the exposure downsample by its target, else the mid-forward copy (the reactive mask's fallback "before")
            if (game_device_data.exposure_downsample && render_target_resource() == game_device_data.exposure_downsample)
            {
               scene_slot = 5;
            }
            else if (needs_reactive_copy)
            {
               com_ptr<ID3D11ShaderResourceView> scene_srv;
               native_device_context->PSGetShaderResources(5, 1, &scene_srv);
               com_ptr<ID3D11Resource> scene;
               if (scene_srv)
               {
                  scene_srv->GetResource(&scene);
               }
               CopyReactiveOpaqueScene(native_device, native_device_context, scene.get(), &game_device_data);
            }
         }
         else if (is_tonemap)
         {
            scene_slot = 6;
         }
         if (scene_slot != 0)
         {
            com_ptr<ID3D11ShaderResourceView> scene_srv;
            native_device_context->PSGetShaderResources(scene_slot, 1, &scene_srv);
            DrawSR(native_device, native_device_context, cmd_list_data, device_data, &game_device_data, scene_srv.get(), scene_slot != 6);
         }
      }
      const bool is_bloom_split_pass = game_device_data.bloom_split_scene_frame == cb_luma_global_settings.FrameIndex && (is_pixel_shader(shader_hash_BrightPassOpaque) || is_pixel_shader(shader_hash_BrightPassAlpha));
      if (is_bloom_split_pass && original_draw_dispatch_func && *original_draw_dispatch_func)
      {
         com_ptr<ID3D11ShaderResourceView> scene_srv;
         native_device_context->PSGetShaderResources(5, 1, &scene_srv);
         if (GetViewTextureSize(scene_srv.get()) == GetViewTextureSize(game_device_data.bloom_split_scene.srv.get()))
         {
            ID3D11ShaderResourceView* const split_scene_srv = game_device_data.bloom_split_scene.srv.get();
            native_device_context->PSSetShaderResources(5, 1, &split_scene_srv);
            (*original_draw_dispatch_func)();
            ID3D11ShaderResourceView* const original_scene_srv = scene_srv.get();
            native_device_context->PSSetShaderResources(5, 1, &original_scene_srv);
            return DrawOrDispatchOverrideType::Replaced;
         }
      }
      if (is_tonemap)
      {
         device_data.has_drawn_main_post_processing = true;
         return DrawOrDispatchOverrideType::None;
      }
      // Videos: AutoHDR only onto the swapchain (LumaData.CustomData1), never skipped by Hide Gameplay UI
      if (shader_hashes_Video.Contains(original_shader_hashes.pixel_shaders[0], reshade::api::shader_stage::pixel))
      {
         SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, stages, LumaConstantBufferType::LumaSettings);
         SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, stages, LumaConstantBufferType::LumaData, (targets_back_buffer() ? 1 : 0));
         updated_cbuffers = true;
         return DrawOrDispatchOverrideType::None;
      }
      if (!device_data.has_drawn_main_post_processing)
         return DrawOrDispatchOverrideType::None;
      const bool is_copy = is_pixel_shader(shader_hash_CopyBuffer);
      if ((!is_copy && !g_hide_ui) || !targets_back_buffer())
         return DrawOrDispatchOverrideType::None;

      // The scene lands on the swapchain: sharpened after SR or our SMAA, and flagged so the copy dithers it once (LumaData.CustomData1)
      if (is_copy)
      {
         if ((device_data.has_drawn_sr || game_device_data.smaa_last_frame == cb_luma_global_settings.FrameIndex) && g_rcas_sharpness > 0.f)
         {
            DrawRCASOnSwapchainCopy(native_device, native_device_context, device_data, &game_device_data);
         }
         SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, stages, LumaConstantBufferType::LumaSettings);
         SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, stages, LumaConstantBufferType::LumaData, 1);
         updated_cbuffers = true;
         return DrawOrDispatchOverrideType::None;
      }

      // Everything drawn onto the swapchain after the scene copy is Scaleform UI (HUD, menus, prompts)
      return DrawOrDispatchOverrideType::Skip;
   }

   void OnPresent(ID3D11Device* native_device, DeviceData& device_data) override
   {
      auto& game_device_data = GetGameDeviceData(device_data);
      // Hooked once, at the first present, by which the game code is unpacked
      if (!std::exchange(hooks_attempted, true))
      {
         // All null for another build, or before the code is unpacked
         const auto find_game_addresses = []() -> GameAddresses
         {
            auto* const base = reinterpret_cast<uint8_t*>(GetModuleHandleW(nullptr));
            const auto* const nt_headers = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + reinterpret_cast<const IMAGE_DOS_HEADER*>(base)->e_lfanew);
            using System::ANY;
            // cmp [rip+?], 2; mov rax, [rcx+0x1160]
            constexpr std::array<System::BytePattern, 14> jitter_prologue = {{0x83, 0x3D, ANY, ANY, ANY, ANY, 0x02, 0x48, 0x8B, 0x81, 0x60, 0x11, 0x00, 0x00}};
            uint8_t* const jitter = base + 0x1886EC0;
            // sub rsp, 0x48; movaps xmm0, [rip+?]; mov edx, 5; mov rax, [rcx+8]
            constexpr std::array<System::BytePattern, 20> frame_params_prologue = {{0x48, 0x83, 0xEC, 0x48, 0x0F, 0x28, 0x05, ANY, ANY, ANY, ANY, 0xBA, 0x05, 0x00, 0x00, 0x00, 0x48, 0x8B, 0x41, 0x08}};
            uint8_t* const frame_params = base + 0x1895020;
            // mov eax, [rcx+8]; shr eax, 4; not eax; and eax, 1; ret (FILE_ATTRIBUTE_DIRECTORY clear)
            constexpr std::array<System::BytePattern, 12> save_entry_is_file_code = {{0x8B, 0x41, 0x08, 0xC1, 0xE8, 0x04, 0xF7, 0xD0, 0x83, 0xE0, 0x01, 0xC3}};
            uint8_t* const save_entry_is_file = base + 0x1E68240;
            const auto matches = [](const uint8_t* code, std::span<const System::BytePattern> prologue)
            { return !System::ScanMemoryForPattern(reinterpret_cast<const std::byte*>(code), prologue.size(), prologue, true).empty(); };
            if (nt_headers->FileHeader.TimeDateStamp != 0x5C096926 || !matches(jitter, jitter_prologue) || !matches(frame_params, frame_params_prologue) || !matches(save_entry_is_file, save_entry_is_file_code))
               return {};
            return {
               .jitter = jitter,
               .frame_params = frame_params,
               .game_frame = reinterpret_cast<const int32_t*>(base + 0x427CEB0),
               .render_frame = reinterpret_cast<const int32_t*>(base + 0x42AF3A4),
               .render_aa_option = reinterpret_cast<int32_t*>(base + 0x42AF340),
               .debug_render_mode = reinterpret_cast<const int32_t*>(base + 0x42AEB64),
               .skip_smaa = base + 0x42AEC60,
               .shared_render_buffers = base + 0x42AFFC0,
               .save_entry_is_file = save_entry_is_file,
            };
         };
         if (const GameAddresses addresses = find_game_addresses(); addresses.jitter && InitializeMinHook())
         {
            game_addresses = addresses;
            const auto install = [](uint8_t* target, void* detour, void** original)
            {
               if (MH_CreateHook(target, detour, original) != MH_OK || MH_EnableHook(target) != MH_OK)
               {
                  *original = nullptr;
               }
            };
            install(addresses.jitter, reinterpret_cast<void*>(&JitterDetour), reinterpret_cast<void**>(&jitter_original));
            install(addresses.frame_params, reinterpret_cast<void*>(&FrameParamsDetour), reinterpret_cast<void**>(&frame_params_original));
            install(addresses.save_entry_is_file, reinterpret_cast<void*>(&SaveEntryIsFileDetour), reinterpret_cast<void**>(&save_entry_is_file_original));
         }
         if (!jitter_original || !frame_params_original)
         {
            sr_game_tooltip = "Unsupported game executable version: Super Resolution can't engage.\n";
         }
      }

      // Any frame SR didn't draw (off, failed, a menu without a scene) restarts its history at the next one
      game_device_data.sr_active = LatchSRFrame(device_data) && jitter_original && frame_params_original;
      game_device_data.sr_tried = false;
      game_device_data.motion_vectors_drawn = false;
      game_device_data.depth_srv = nullptr;
      game_device_data.reactive_opaque_scene_copied = false;
      // SMAA's scratch textures are released once idle, Core's intermediates included
      if (game_device_data.smaa_last_frame != 0 && cb_luma_global_settings.FrameIndex - game_device_data.smaa_last_frame > SMAA_IDLE_RELEASE_FRAMES)
      {
         game_device_data.smaa_last_frame = 0;
         game_device_data.smaa_predication = {};
         ReleaseSMAAIntermediates(device_data);
      }
      // The jitter starts after a drawn frame (none while Luma TAA compiles, or while SR can't find its inputs)
      const SR::InstanceData* const sr_instance_data = (game_device_data.sr_active ? device_data.GetSRInstanceData() : nullptr);
      const bool sr_drew_last_frame = sr_instance_data && !device_data.force_reset_sr && sr_implementations[device_data.sr_type]->IsReady(sr_instance_data);
      // Our SMAA T2x in place of the game's: its own jitter (the game's fades out in motion and is half the reference offset)
      game_device_data.t2x_active = g_smaa_enable && g_smaa_t2x && !game_device_data.sr_active && jitter_original && frame_params_original;
      if (!game_device_data.t2x_active)
      {
         game_device_data.t2x_frames = {};
      }
      int32_t render_aa_option = RENDER_AA_GAME;
      int jitter_phases = VANILLA_JITTER;
      if (game_device_data.sr_active)
      {
         render_aa_option = AA_OPTION_OFF;
         jitter_phases = (sr_drew_last_frame ? (std::max)(sr_implementations[device_data.sr_type]->GetJitterPhases(sr_instance_data), 1) : STAMP_ONLY_JITTER);
      }
      else if (game_device_data.t2x_active)
      {
         render_aa_option = AA_OPTION_SMAA_T2X;
         jitter_phases = SMAA_T2X_JITTER;
      }
      else if (g_smaa_enable)
      {
         render_aa_option = AA_OPTION_SMAA;
         jitter_phases = NO_JITTER;
      }
      g_render_aa_option = render_aa_option;
      g_jitter_phases = jitter_phases;
      // SR resolves more texture detail: -1 mip bias at native resolution
      SetTextureMipLodBias(nullptr, device_data, (sr_drew_last_frame ? SR::SuperResolutionImpl::GetMipLODBias(sr_instance_data) : 0.f));
   }

   void CleanExtraSRResources(DeviceData& device_data) override
   {
      auto& game_device_data = GetGameDeviceData(device_data);
      game_device_data.motion_vectors = {};
      game_device_data.motion_vectors_drawn = false;
      game_device_data.exposure_downsample = nullptr;
      game_device_data.reactive_opaque_scene = {};
      game_device_data.reactive_mask = {};
      game_device_data.rcas_output = {};
      game_device_data.bloom_split_scene = {};
      game_device_data.sr_exposure = {};
   }

   void LoadConfigs() override
   {
      auto& settings = cb_luma_global_settings.GameSettings;
      reshade::get_config_value(nullptr, NAME, "Dithering", settings.Dithering);
      reshade::get_config_value(nullptr, NAME, "ColorGradingIntensity", settings.ColorGradingIntensity);
      reshade::get_config_value(nullptr, NAME, "BloomIntensity", settings.BloomIntensity);
      reshade::get_config_value(nullptr, NAME, "LensFlareIntensity", settings.LensFlareIntensity);
      reshade::get_config_value(nullptr, NAME, "VignetteIntensity", settings.VignetteIntensity);
      reshade::get_config_value(nullptr, NAME, "FilmGrainIntensity", settings.FilmGrainIntensity);
      reshade::get_config_value(nullptr, NAME, "Exposure", settings.Exposure);
      reshade::get_config_value(nullptr, NAME, "Contrast", settings.Contrast);
      reshade::get_config_value(nullptr, NAME, "Saturation", settings.Saturation);
      reshade::get_config_value(nullptr, NAME, "HighlightsDesaturation", settings.HighlightsDesaturation);
      reshade::get_config_value(nullptr, NAME, "VideoAutoHDREnable", settings.VideoAutoHDREnable);
      reshade::get_config_value(nullptr, NAME, "VideoAutoHDRBoost", settings.VideoAutoHDRBoost);
      reshade::get_config_value(nullptr, NAME, "RCASSharpness", g_rcas_sharpness);
      reshade::get_config_value(nullptr, NAME, "SMAAEnable", g_smaa_enable);
      reshade::get_config_value(nullptr, NAME, "SMAAT2x", g_smaa_t2x);
      reshade::get_config_value(nullptr, NAME, "FixCorruptedSaveWarning", g_fix_corrupted_save_warning);
   }

   void DrawImGuiSettings(DeviceData& device_data) override
   {
      auto& settings = cb_luma_global_settings.GameSettings;

      // A [0, max] slider over a setting, saved once the edit ends
      const auto slider = [](const char* label, const char* key, float* value, float default_value, float max_value, const char* tooltip)
      {
         ImGui::SliderFloat(label, value, 0.f, max_value);
         if (ImGui::IsItemDeactivatedAfterEdit())
         {
            reshade::set_config_value(nullptr, NAME, key, *value);
         }
         if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
         {
            ImGui::SetTooltip("%s", tooltip);
         }
         DrawResetButton(*value, default_value, key);
      };

      // A GameSettings value shown as a checkbox (0 or 1), saved on change; returns whether it's on
      const auto checkbox = [](const char* label, const char* key, float* value, float default_value, const char* tooltip)
      {
         bool enabled = *value > 0.5f;
         if (ImGui::Checkbox(label, &enabled))
         {
            *value = (enabled ? 1.f : 0.f);
            reshade::set_config_value(nullptr, NAME, key, *value);
         }
         if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
         {
            ImGui::SetTooltip("%s", tooltip);
         }
         DrawResetButton(*value, default_value, key);
         return *value > 0.5f;
      };
      // A bool setting, saved on change
      const auto bool_checkbox = [](const char* label, const char* key, bool* value, bool default_value, const char* tooltip)
      {
         if (ImGui::Checkbox(label, value))
         {
            reshade::set_config_value(nullptr, NAME, key, *value);
         }
         if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
         {
            ImGui::SetTooltip("%s", tooltip);
         }
         DrawResetButton(*value, default_value, key);
      };

      // Super Resolution itself is Core's combo, above
      ImGui::SeparatorText("Anti-Aliasing");
      // Super Resolution replaces SMAA only where it can engage (the game hooks are in)
      const bool sr_engaged = device_data.sr_type != SR::Type::None && !device_data.sr_suppressed && jitter_original && frame_params_original;
      ImGui::BeginDisabled(sr_engaged);
      bool_checkbox("SMAA Enable", "SMAAEnable", &g_smaa_enable, true, "Replaces the game's SMAA with Luma's (sharper, more complete edges; works with the game's Anti-Aliasing setting on or off; not used with DLSS/FSR or Luma TAA).");
      ImGui::BeginDisabled(!g_smaa_enable);
      bool_checkbox("SMAA T2x", "SMAAT2x", &g_smaa_t2x, false, "Smoother edges and less shimmering, by blending each frame with the previous one.\nCan look slightly softer in motion. Not used with DLSS/FSR or Luma TAA.");
      ImGui::EndDisabled();
      ImGui::EndDisabled();
      // RCAS runs after our SMAA or Super Resolution only
      ImGui::BeginDisabled(!g_smaa_enable && !sr_engaged);
      slider("RCAS Sharpness", "RCASSharpness", &g_rcas_sharpness, 0.f, 1.f, "Sharpening applied on top of anti-aliasing (0 = off).");
      ImGui::EndDisabled();

      // The 3D LUT, the bloom, its lens flare, the film grain and the vignette all live in the tonemap (only the permutations that have them)
      ImGui::SeparatorText("Grade");
      slider("Exposure", "Exposure", &settings.Exposure, default_luma_global_game_settings.Exposure, 2.f, "Overall image brightness (1 = vanilla).");
      slider("Contrast", "Contrast", &settings.Contrast, default_luma_global_game_settings.Contrast, 2.f, "Overall image contrast, HDR only (1 = vanilla).");
      slider("Saturation", "Saturation", &settings.Saturation, default_luma_global_game_settings.Saturation, 2.f, "Color saturation, HDR only (1 = vanilla).");
      slider("Highlights Desaturation", "HighlightsDesaturation", &settings.HighlightsDesaturation, default_luma_global_game_settings.HighlightsDesaturation, 1.f, "How far the brightest sources fade to neutral white, HDR only (0 = keep color at any brightness).");
      slider("Color Grading Intensity", "ColorGradingIntensity", &settings.ColorGradingIntensity, default_luma_global_game_settings.ColorGradingIntensity, 1.f, "Strength of the game's own color grading (1 = vanilla, 0 = neutral).");

      ImGui::SeparatorText("Bloom");
      slider("Bloom Intensity", "BloomIntensity", &settings.BloomIntensity, default_luma_global_game_settings.BloomIntensity, 2.f, "Bloom strength (1 = vanilla, 0 = none).");

      ImGui::SeparatorText("Effects");
      slider("Vignette Intensity", "VignetteIntensity", &settings.VignetteIntensity, default_luma_global_game_settings.VignetteIntensity, 1.f, "Scales the game's vignette darkening (1 = vanilla, 0 = none).");
      slider("Film Grain Intensity", "FilmGrainIntensity", &settings.FilmGrainIntensity, default_luma_global_game_settings.FilmGrainIntensity, 2.f, "Scales the game's film grain (1 = vanilla, 0 = off).");
      slider("Lens Flare Intensity", "LensFlareIntensity", &settings.LensFlareIntensity, default_luma_global_game_settings.LensFlareIntensity, 2.f, "Lens-flare / glare strength (1 = vanilla, 0 = off).");
      ImGui::BeginDisabled(!checkbox("Video AutoHDR", "VideoAutoHDREnable", &settings.VideoAutoHDREnable, default_luma_global_game_settings.VideoAutoHDREnable, "Adds HDR highlights to pre-rendered videos (HDR only)."));
      slider("Video HDR Boost", "VideoAutoHDRBoost", &settings.VideoAutoHDRBoost, default_luma_global_game_settings.VideoAutoHDRBoost, 1.f, "Video highlight strength (0 = off).");
      ImGui::EndDisabled();
      checkbox("Dithering", "Dithering", &settings.Dithering, default_luma_global_game_settings.Dithering, "Reduces gradient banding.");

      ImGui::SeparatorText("UI");
      ImGui::Checkbox("Hide Gameplay UI", &g_hide_ui);
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
      {
         ImGui::SetTooltip("Disables the in-game UI.");
      }

      ImGui::SeparatorText("Fixes");
      bool_checkbox("Fix Corrupted Save Warning", "FixCorruptedSaveWarning", &g_fix_corrupted_save_warning, true, "Stops the \"corrupted save\" warning at every start (caused by Steam Cloud's sync file in the save folder).");
   }

   void PrintImGuiAbout() override
   {
      ImGui::PushTextWrapPos(0.f);
      ImGui::Text(
         "Luma for \"Sunset Overdrive\" is developed by DristoforColumb and is open source and free.\n"
         "It adds HDR, DLAA or FSR 3 native anti-aliasing and improves SMAA anti-aliasing, plus 16x anisotropic filtering.\n"
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
                  "\nDICE (HDR tonemapper)"
                  "\nSMAA (Iryoku)"
                  "\nAMD FidelityFX (RCAS + FSR 3)"
                  "\nNVIDIA NGX (DLSS)");
   }
};

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
   if (ul_reason_for_call == DLL_PROCESS_ATTACH)
   {
      Globals::SetGlobals(PROJECT_NAME, "Sunset Overdrive Luma mod", "", 1);

      // The scene and bloom are already float (R11G11B10). The tonemap writes R10G10B10A2 at the game's render resolution,
      // which SMAA, the T2x history and PS_CopyBuffer carry to the RGBA8 swapchain (the window size, pillarboxed to 16:9).
      swapchain_format_upgrade_type = TextureFormatUpgradesType::AllowedEnabled;
      swapchain_upgrade_type = SwapchainUpgradeType::scRGB;
      texture_format_upgrades_type = TextureFormatUpgradesType::AllowedEnabled;
      texture_upgrade_formats = {
         reshade::api::format::r10g10b10a2_unorm,
         reshade::api::format::r10g10b10a2_typeless,
      };
      // The render resolution is an option independent of the window, so we also match its 16:9 aspect ratio, not only the swapchain size
      texture_format_upgrades_2d_size_filters = (uint32_t)TextureFormatUpgrades2DSizeFilters::SwapchainResolution | (uint32_t)TextureFormatUpgrades2DSizeFilters::CustomAspectRatio;

      // Exclusive fullscreen is blocked (Core default); turn the game's fullscreen requests into a borderless window instead
      force_borderless = true;

      // The SR mip bias (the engine's samplers have none and no option for it)
      enable_samplers_upgrade = true;

#if DEVELOPMENT
      // Offline map of the exe-embedded engine shaders, for traces and the DevKit
      for (const auto& [hash, name] : std::initializer_list<std::pair<uint32_t, const char*>>{
              {0x92D51420, "ToneMap"},
              {0x7880CCF1, "ToneMap (Bloom)"},
              {0x86EE37B6, "ToneMap (LUT)"},
              {0xC291251A, "ToneMap (Bloom LUT)"},
              {0xE4EAD75E, "ToneMap (Grain)"},
              {0x6619803F, "ToneMap (Bloom Grain)"},
              {0x2A6A872A, "ToneMap (LUT Grain)"},
              {0xEE7FB6CC, "ToneMap (Bloom LUT Grain)"},
              {0x58E6385C, "ToneMap (Vignette)"},
              {0x633A5894, "ToneMap (Bloom Vignette)"},
              {0xD6B7231C, "ToneMap (LUT Vignette)"},
              {0xE3E7DD84, "ToneMap (Bloom LUT Vignette)"},
              {0x980ACE18, "ToneMap (Grain Vignette)"},
              {0x2F20FD47, "ToneMap (Bloom Grain Vignette)"},
              {0x902D30F9, "ToneMap (LUT Grain Vignette)"},
              {0xB2BBC089, "ToneMap (Bloom LUT Grain Vignette)"},
              {0xF7085ACD, "Copy Buffer (post tonemap)"},
              {0x2EB348CF, "Bright Pass"},
              {0x8EACA7B7, "Bright Pass Opaque"},
              {0x6EB0D51C, "Bright Pass Alpha"},
              {0xE2230CBF, "Bloom Blur Down"},
              {0x52300544, "Bloom Blur Up"},
              {0x0DEE3EB1, "Average Lum Accumulate"},
              {0x33A2F37D, "Average Lum Finalize"},
              {0x8948AF03, "SMAA Find Edges"},
              {0x0CC29EC5, "SMAA Compute Weights"},
              {0xD8DB5270, "SMAA Neighborhood Blend"},
              {0xFF745C34, "SMAA T2x Resolve Reproject"},
              {0x524BC7C4, "Linear Depth + Camera Velocity"},
              {0x928DFB65, "Object Velocity"},
              {0xD665B34D, "Motion Blur Pack Velocity"},
              {0xFCE207A5, "Motion Blur Apply"},
              {0x665173C0, "Motion Blur Downsample Color"},
              {0x6CDD786E, "Tex (copy)"},
              {0xA5ADD0FE, "Apply Light Linked List (deferred lighting)"},
              {0x172FF9A3, "SSAO Depth Downsample"},
              {0x7770B550, "SSAO Generate"},
              {0xA1CCFEC8, "SSAO Generate Interleaved"},
              {0xF70ADA39, "SSAO Blur Combine"},
              {0x4750F817, "SSAO Upsample"},
              {0x6854DB47, "SSAO Upsample Combine"},
              {0xFEB8BC17, "DoF Downsample Far"},
              {0x575FE7DF, "DoF Downsample Near"},
              {0x1C679396, "DoF Downsample Neighborhood Near"},
              {0x2D574464, "DoF Gather Neighborhood Near"},
              {0xBA698E9E, "DoF Generate Far"},
              {0x81F5F129, "DoF Generate Near"},
              {0x80649033, "Video YCbCr"},
              {0x0FA626B3, "Video NV12"},
              {0x11DDCB3A, "Video YCbCrA"},
              {0x26F1A4C4, "Quantize Color Buffer"},
           })
      {
         forced_shader_names.emplace(hash, name);
      }
#endif

      game = new SunsetOverdrive();
   }

   CoreMain(hModule, ul_reason_for_call, lpReserved);

   return TRUE;
}
