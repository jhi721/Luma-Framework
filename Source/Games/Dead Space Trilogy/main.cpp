#define GAME_DEAD_SPACE_TRILOGY 1

#define GEOMETRY_SHADER_SUPPORT 0
#define DISABLE_AUTO_DEBUGGER 1
// The forward output clamps and the uber's LUT tail patch the bytecode ("PatchShaderBytecodeSync"); Core's "SceneMotionVectors" reads
// the shaders from the patch context
#define LUMA_PATCH_BYTECODE_SYNC 1
// Makes "original_draw_dispatch_func" non-null: the scene's draws run again with the motion vector shaders, the screen space UI draws
// again into the UI layer ("ui_layer")
#define ENABLE_POST_DRAW_DISPATCH_CALLBACK 1
// The motion vector draw key reads the draw's arguments ("last_draw_dispatch_data")
#define ENABLE_DRAW_DISPATCH_DATA_CACHE 1
// A "Super Resolution" choice next to the SR bridge's DLSS, FSR and XeSS, drawn in process on any GPU
#define ENABLE_LUMA_TAA 1
// Optional sharpening of the upscaler's or SMAA's output (DLSS-Best-Practices SHP-2)
#define ENABLE_RCAS 1
// SMAA ULTRA (1x or T2x) in the game's own AA slot or at the scene's end (see "RunSMAA"); Core registers its 6 passes from
// Luma_SMAA_impl.hlsl
#define ENABLE_SMAA 1
// SMAA's area texture without the U-shape smoothing (see Luma_SMAA_impl.hlsl)
#define SMAA_SMOOTH_U_SHAPES 0

#include "..\..\Core\core.hpp"
#include "DS1\MotionVectorPatches.h"
#include "DS1\WriterPixelShaders.h"
#include "DS1\ForwardPixelShaders.h"
#include "DS1\UberPixelShaders.h"
#include "DS1\PostPixelShaders.h"
#include "DS1\UiPixelShaders.h"
#include "DS2\MotionVectorPatches.h"
#include "DS2\WriterPixelShaders.h"
#include "DS2\ForwardPixelShaders.h"
#include "DS2\UberPixelShaders.h"
#include "DS2\PostPixelShaders.h"
#include "DS3\MotionVectorPatches.h"
#include "DS3\WriterPixelShaders.h"
#include "DS3\ForwardPixelShaders.h"
#include "UberLutPatch.h"
#include "DS3\UberPixelShaders.h"
#include "..\..\Core\includes\patched_draws.h"

#include <d3d9.h> // Dead Space 1's and 2's device (see "GammaRamp")

// Dead Space 1, 2 and 3 in one addon, the game picked by its exe in "DllMain". All three are x86 Visceral engine on D3D9, run through
// dgVoodoo (D3D9 -> D3D11), so every hash is a dgVoodoo translation. "DeadSpaceGame" holds what the three share (DLAA / FSR / XeSS /
// Luma TAA from Core's motion vector patch, SMAA 1x/T2x, RCAS, the settings); each game's subclass holds its passes. Each game's hash
// lists and motion vector rows live in its folder (DS1/, DS2/, DS3/), inside a namespace of the same name.

#if DEVELOPMENT
static bool g_mv_enable = false;       // Motion vectors without an upscaler
static bool g_mv_debug_view = false;   // Core's debug draw of the motion vector target
static bool g_mv_force_jitter = false; // The projection jitter without an upscaler
static bool g_sr_reactive_enable = true;
static float g_sr_reactive_scale = 1.f;      // The alpha blended draws' reactivity, scaled (AMD's default 1)
static float g_sr_reactive_threshold = 0.5f; // Under it 0, over it 0.9 (Mass Effect 2007's and Mirror's Edge's); 0: the scaled reactivity, capped at 0.9
static bool g_sr_reactive_debug_view = false;
// Predication from the scene depth, and its plane-deviation tolerance ("Luma_DS_DepthEdges.hlsl")
static bool g_smaa_predication = true;
static float g_smaa_pred_tolerance = 0.02f;
#else
constexpr bool g_mv_enable = false;
constexpr bool g_mv_force_jitter = false;
constexpr bool g_sr_reactive_enable = true;
constexpr float g_sr_reactive_scale = 1.f;
constexpr float g_sr_reactive_threshold = 0.5f;
constexpr bool g_smaa_predication = true;
constexpr float g_smaa_pred_tolerance = 0.02f;
#endif
static bool g_smaa_enable = true;
static bool g_smaa_t2x = false;      // SMAA T2x: a two phase jitter and the previous frame, through the upscalers' motion vectors
static float g_rcas_sharpness = 0.f; // RCAS on the upscaler's or SMAA's output, 0 = off
// SMAA's and T2x's resources and the reactive masks go after this many frames without their pass (MEM-11)
constexpr uint32_t IDLE_RELEASE_FRAMES = 600;

using SceneMotionVectors::PixelShaderPatch;

// A row vector view projection that projects (w depends on the position), not an orthographic transform (2D passes)
template <typename T>
static bool IsProjecting(const T* m)
{
   return std::abs(m[3]) + std::abs(m[7]) + std::abs(m[11]) >= T(0.5);
}

// The Steam exes (no ASLR, one build each) have "code" at "address": each game's fixed addresses are read only on it
static bool MatchesExeCode(uintptr_t address, std::span<const uint8_t> code)
{
   const auto* const image = reinterpret_cast<const IMAGE_DOS_HEADER*>(GetModuleHandleW(nullptr));
   if (reinterpret_cast<uintptr_t>(image) != 0x400000)
      return false;
   const auto* const nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(reinterpret_cast<const uint8_t*>(image) + image->e_lfanew);
   return nt->OptionalHeader.SizeOfImage > address + code.size() - 0x400000 && std::memcmp(reinterpret_cast<const void*>(address), code.data(), code.size()) == 0;
}

// The v2.87 hash of a dgVoodoo 2.81.3 (the Linux zip, ps_4_0) translation of a pass keyed by one hash (the same D3D9 blob and alpha
// test), from the game's "pixel_shader_hashes_from_v281"; any other hash as it is
static uint32_t ToV287Hash(uint32_t hash, std::span<const std::pair<uint32_t, uint32_t>> v281_to_v287)
{
   for (const auto& [v281_hash, v287_hash] : v281_to_v287)
   {
      if (v281_hash == hash)
         return v287_hash;
   }
   return hash;
}

// The game's brightness slider is a D3D9 SetGammaRamp, which dgVoodoo applies in its present blit (or forwards to the OS ramp),
// distorting HDR output. The hook keeps the ramp off dgVoodoo and hands its exponent to the display map ("GammaRampExponent").
// Dead Space 1: settings.txt "Window.Gamma" (default 0.5) sets pow(x, 0.94 - 0.4 * gamma) (FUN_007c1140; 0.74 at the default,
// brightening). Dead Space 2: the same setting (0.5 = identity) sets pow(x, 1.5 - gamma) (FUN_013cb3e0).
namespace GammaRamp
{
   constexpr size_t SET_GAMMA_RAMP_SLOT = 21; // IDirect3DDevice9 vtable
   using SetGammaRampFunc = void(STDMETHODCALLTYPE*)(IDirect3DDevice9*, UINT, DWORD, const D3DGAMMARAMP*);
   static SetGammaRampFunc original = nullptr;
   static void** patched_slot = nullptr;
   static std::atomic<float> exponent = 1.f;

   // The default slider's exponent, set by the game before "Install". Windowed vanilla shows no ramp, so the default is normalized to no
   // change (as DeadSpaceFixes' FixFullscreenBrightness, which our vtable hook bypasses, and Dead Space 2's "1.5 - gamma"): the same
   // look with or without it.
   static float default_slider_exponent = 1.f;

   // From the ramp's middle entry (16 bit): Dead Space 1's slider range is 0.54-0.94 (0.73-1.27 normalized), clamped wider against odd
   // ramps
   static float ExponentOf(uint32_t middle_entry)
   {
      const float ramp_value = (std::max)(middle_entry / 65535.f, 1e-4f);
      return std::clamp(std::log(ramp_value) / std::log(128.f / 255.f) / default_slider_exponent, 0.25f, 4.f);
   }

   static void STDMETHODCALLTYPE HookSetGammaRamp(IDirect3DDevice9* device, UINT swap_chain, DWORD flags, const D3DGAMMARAMP* ramp)
   {
      if (ramp)
      {
         exponent = ExponentOf(ramp->red[128]);
      }
   }

   static void Install(IDirect3DDevice9* device)
   {
      void** const vtable = *reinterpret_cast<void***>(device);
      if (vtable[SET_GAMMA_RAMP_SLOT] == reinterpret_cast<void*>(&HookSetGammaRamp))
         return;
      // The ramp the game set before the hook; dgVoodoo's GetGammaRamp returns it divided by 257
      D3DGAMMARAMP ramp = {};
      device->GetGammaRamp(0, &ramp);
      exponent = ExponentOf(ramp.red[128] * 257u);
      original = reinterpret_cast<SetGammaRampFunc>(vtable[SET_GAMMA_RAMP_SLOT]);
      // That ramp already reached dgVoodoo: back to identity
      D3DGAMMARAMP identity;
      for (WORD i = 0; i < 256; i++)
      {
         identity.red[i] = identity.green[i] = identity.blue[i] = WORD(i * 257);
      }
      original(device, 0, D3DSGR_NO_CALIBRATION, &identity);
      const void* const hook = reinterpret_cast<const void*>(&HookSetGammaRamp);
      System::PatchMemory(&vtable[SET_GAMMA_RAMP_SLOT], &hook, sizeof(hook), System::PatchMemoryType::Data);
      patched_slot = &vtable[SET_GAMMA_RAMP_SLOT];
   }

   static void Uninstall()
   {
      if (!patched_slot)
         return;
      System::PatchMemory(patched_slot, &original, sizeof(original), System::PatchMemoryType::Data);
      patched_slot = nullptr;
   }
} // namespace GammaRamp

// A replacement registered by name ("Name_0x########"): "Name" binds "hash" and, when dgVoodoo 2.81.3 translates it differently (ps_4_0),
// "Name_v281" binds "v281_hash" (one name keys one hash set, and the twin is its own source)
struct ShaderRedirect
{
   const char* name;
   uint32_t hash;
   uint32_t v281_hash = 0;
};

// Dead Space 2's and 3's bloom.fx passes: the same D3D9 shaders, so the same dgVoodoo hashes (DS_Bloom*)
constexpr ShaderRedirect bloom_redirects[] = {
   {"DS_BloomBright", 0x5977AF81, 0x81312A1E},
   {"DS_BloomBrightModulate", 0x24D9ED06, 0xD37A09A2},
   {"DS_BloomLowHigh", 0xB8962C50, 0x22CBCFD0},
   {"DS_BloomLowHighModulate", 0xA9A54ADF, 0x02841931},
};

// Every field any of the three games uses: Core creates it in "OnCreateDevice", and "MCP_GAME_TEXTURE" reads it unqualified
struct DeadSpaceGameDeviceData final : public GameDeviceData
{
   // The uber post drew this frame: its LUT tail ("WrapUberLut") already carries the vanilla clip's hue turn, so the display map then
   // takes no hue from its reference (LumaData.CustomData2, Includes/DS_DisplayMap.hlsl). Dead Space 1/2: it also picks where the display
   // map runs (see "scene_over"). Immediate context only.
   bool uber_drawn = false;
   // Dead Space 1/2: the HDR display map ran this frame, before the UI ("DrawDisplayMap", or Dead Space 2's AA slot). The present blit
   // then doesn't map the frame again (Includes/DS_Present.hlsl). Immediate context only.
   bool display_mapped = false;
   // Dead Space 1: a material apply draw ran this frame (the 3D scene): the screen space prims.fx (HUD 0xA5B366DB, 0x0CCDEEF3) also draw
   // inside its forward pass, so from then on only a post pass ends the scene ("scene_over"). Immediate context only.
   bool scene_applied = false;
   // Dead Space 1/2: the last output sized post pass' target (the frame: uber, depth of field, the AA pass), and the copy of it
   // "DrawDisplayMap" reads
   com_ptr<ID3D11Resource> post_target;
   com_ptr<ID3D11Texture2D> display_map_source;
   com_ptr<ID3D11ShaderResourceView> display_map_source_srv;
   // Dead Space 1/2: the frame right after the display map (and a video drawn after it): the present blit bounds what drew later by it,
   // as the vanilla UNORM frame clamped those blends at 1 (Includes/DS_Present.hlsl). "ui_bound_pending" is the frame of a video drawn after we
   // return, copied at the next immediate draw. Immediate context only.
   com_ptr<ID3D11Texture2D> ui_bound;
   com_ptr<ID3D11ShaderResourceView> ui_bound_srv;
   com_ptr<ID3D11Resource> ui_bound_pending;
   DXGI_FORMAT ui_bound_pending_format = DXGI_FORMAT_UNKNOWN;
   bool ui_bound_valid = false;
   // Dead Space 3: the final display pass's source (t0), latched at its draw: the frame the screen space UI draws on, the next frame
   com_ptr<ID3D11Resource> final_source;
   // Dead Space 3: the post chain started this frame ("POST_DOWNSAMPLE_PS" read the canvas): what then draws on the canvas is UI
   bool post_started = false;
   // HDR: the UI drawn on the canvas after the post chain (Dead Space 3: APT subtitles and prompts, the tutorial holograms, depth tested
   // or not; the menus' panels draw before it, in the scene) draws here instead, premultiplied over transparent black, so the final
   // display pass maps the scene alone and puts the UI over it at the UI paper white (Luma's UI_DRAW_TYPE 2 pre-scale). Its alpha is
   // the coverage (see "ui_layer_blend"). Immediate context only. Not Core's UI separation (UI_DRAW_TYPE 3): the game composes in its
   // own final display pass, so this keeps UI_DRAW_TYPE 2 and rewrites the draws' alpha blend instead.
   com_ptr<ID3D11Texture2D> ui_layer;
   com_ptr<ID3D11RenderTargetView> ui_layer_rtv;
   com_ptr<ID3D11ShaderResourceView> ui_layer_srv;
   bool ui_layer_drawn = false;

   // ---- DLAA / FSR / XeSS / Luma TAA with motion vectors and jitter from patched shaders (one per-draw buffer: dgVoodoo's vc4) ----
   // DLSS, FSR and XeSS run in the x64 helper of Core's SR bridge (the games are 32-bit, see "SRBridge.h"), Luma TAA in process.
   // An upscaler is picked and hasn't failed, latched at present for the whole frame
   bool sr_active = false;
   // The upscaler's output goes back into the scene without its alpha, drawn from this view of it
   com_ptr<ID3D11BlendState> sr_rgb_blend_state;
   com_ptr<ID3D11ShaderResourceView> sr_output_srv;
   // The upscalers' input: the scene decoded to linear ("Luma_DS_SRGamma.hlsl"), RCAS' output afterwards; and the scene's view for it
   com_ptr<ID3D11Texture2D> sr_input;
   com_ptr<ID3D11ShaderResourceView> sr_input_srv;
   com_ptr<ID3D11RenderTargetView> sr_input_rtv;
   com_ptr<ID3D11Resource> sr_scene;
   com_ptr<ID3D11ShaderResourceView> sr_scene_srv;

   // Motion vectors and jitter this frame (an upscaler or the DEV toggle, set at present) and their state machine (Core's)
   SceneMotionVectors::State mv;
   // The upscaler drew the last frame: only then the scene jitters (frames it skips would reach the screen jittered)
   bool mv_jitter_allowed = false;
   float sr_vert_fov = 1.0471976f; // FSR's vertical FOV (radians): the last camera's, 60 degrees until one is seen
   // The upscalers' near and far: the last camera's with a usable projection, Core's defaults until one is seen
   float sr_near_plane = SR::SuperResolutionImpl::DrawData{}.near_plane;
   float sr_far_plane = SR::SuperResolutionImpl::DrawData{}.far_plane;
   // The scene depth (D24S8, dgVoodoo's INTZ texture) and the fill's view of it, kept while the scene depth is the same resource
   com_ptr<ID3D11Resource> scene_depth;
   com_ptr<ID3D11ShaderResourceView> scene_depth_srv;
   // This frame's scene color: the material apply's target (see "apply_pixel_shaders"), null until its first draw. Only draws into it
   // take motion vectors; the G-buffer's and the lights' are only jittered.
   com_ptr<ID3D11Resource> scene_target;
   // The depth view "OpenScene" refused (not output sized), reset at present
   ID3D11DepthStencilView* open_refused_dsv = nullptr;
   // The scene depth was larger than the output this frame (an SSAA mod, see "ssaa_mods"), latched at present into "ssaa_active": T2x
   // can't run
   bool ssaa_drawn = false;
   bool ssaa_active = false;
   // SMAA predication's linearization of the scene depth: 1 / the projection's A and its near, from the last frame camera (0: none
   // seen, plain ULTRA)
   float pred_depth_scale = 0.f;
   float pred_near_plane = 0.f;

   // ---- Luma SMAA (see "RunSMAA") ----
   // "game_aa_drawn": the game's AA pass drew this frame (read at present). "smaa_slot_a": this frame's SMAA runs in its slot, else at
   // the scene's end. "aa_drawn": the upscaler or Luma's SMAA antialiased this frame (the game's AA pass is then a copy).
   bool game_aa_drawn = false;
   bool smaa_slot_a = false;
   bool aa_drawn = false;
#if DEVELOPMENT
   // The MCP counters ("aa.*"): this frame's, and the last complete frame's
   struct AAStats
   {
      uint32_t smaa_slot_a = 0;
      uint32_t smaa_slot_b = 0;
      uint32_t t2x_resolves = 0;
      uint32_t game_aa_copies = 0;
      uint32_t game_aa = 0;
      uint32_t ssaa = 0;
      uint32_t smaa_slot_a_chosen = 0;
      uint32_t t2x_active = 0;
   };
   AAStats aa_stats;
   AAStats last_aa_stats;
#endif
   // SMAA metrics CB (b1), predication edge-ness and its CB, written every SMAA frame
   com_ptr<ID3D11Buffer> cb_smaa_metrics;
   com_ptr<ID3D11Buffer> cb_pred;
   com_ptr<ID3D11Texture2D> tex_pred;
   com_ptr<ID3D11UnorderedAccessView> uav_pred;
   com_ptr<ID3D11ShaderResourceView> srv_pred;
   // SMAA's output (fp16, encoded as its input), and RCAS' input when it runs after SMAA
   com_ptr<ID3D11Texture2D> tex_smaa_out;
   com_ptr<ID3D11RenderTargetView> tex_smaa_out_rtv;
   com_ptr<ID3D11ShaderResourceView> tex_smaa_out_srv;
   com_ptr<ID3D11Texture2D> tex_rcas_in;
   com_ptr<ID3D11RenderTargetView> tex_rcas_in_rtv;
   com_ptr<ID3D11ShaderResourceView> tex_rcas_in_srv;
   // The scene's copy SMAA reads at the scene's end (it writes the scene)
   com_ptr<ID3D11Texture2D> tex_snapshot;
   com_ptr<ID3D11ShaderResourceView> srv_snapshot;
   // A Texture2D view of the game's frame (dgVoodoo's views are one-slice arrays), kept while the frame is the same resource
   com_ptr<ID3D11Resource> frame_resource;
   com_ptr<ID3D11ShaderResourceView> frame_srv;
   uint32_t smaa_frame = 0; // The last frame SMAA ran, for "IDLE_RELEASE_FRAMES"

   // SMAA T2x, set at present for the whole frame like "sr_active". The scene jitters only after a frame the resolve ran. The phase is
   // chosen when the scene opens, -1 if it didn't jitter.
   bool t2x_active = false;
   int t2x_phase = -1;
   bool mv_filled = false; // The fill wrote this frame's motion vectors (reset at present)
   // SMAA's output of this frame and of the previous one, alternating: linear RGB with the velocity length in alpha. The previous one
   // is the history only if "t2x_frame", the last frame the resolve ran, was the frame before.
   com_ptr<ID3D11Texture2D> t2x_frames[2];
   com_ptr<ID3D11RenderTargetView> t2x_frame_rtvs[2];
   com_ptr<ID3D11ShaderResourceView> t2x_frame_srvs[2];
   uint32_t t2x_frame = 0;

   bool HasT2xHistory(uint32_t frame_index) const
   {
      return t2x_frames[0] && t2x_frame + 1 == frame_index;
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

   // The upscaler's input and our view of Core's output ("sr_output_color"), recreated at the next upscale
   void ReleaseSRInputs()
   {
      sr_output_srv.reset();
      sr_input.reset();
      sr_input_srv.reset();
      sr_input_rtv.reset();
   }

   // SMAA's own resources, apart from Core's ("ReleaseSMAAIntermediates"); recreated at their next use
   void ReleaseSMAAScratch()
   {
      tex_pred.reset();
      uav_pred.reset();
      srv_pred.reset();
      tex_smaa_out.reset();
      tex_smaa_out_rtv.reset();
      tex_smaa_out_srv.reset();
      tex_rcas_in.reset();
      tex_rcas_in_rtv.reset();
      tex_rcas_in_srv.reset();
      tex_snapshot.reset();
      srv_snapshot.reset();
      frame_resource.reset();
      frame_srv.reset();
   }
};

static DeadSpaceGameDeviceData& GetGameDeviceData(DeviceData& device_data)
{
   return *static_cast<DeadSpaceGameDeviceData*>(device_data.game);
}

static const DeadSpaceGameDeviceData& GetGameDeviceData(const DeviceData& device_data)
{
   return *static_cast<const DeadSpaceGameDeviceData*>(device_data.game);
}

// What the three games share. A subclass passes its "MotionVectorGame" traits (Core's "SceneMotionVectors" config, plus
// "EngineViewProjection()", "infinite_depth" and "ssaa_mods") to "OpenScene".
class DeadSpaceGame : public Game
{
   // The game's motion vector state for Core's buffer and bind events, null before the device data exists
   static SceneMotionVectors::State* GetSceneMotionVectors(reshade::api::device* device)
   {
      DeviceData* const device_data = device->get_private_data<DeviceData>();
      return ((device_data && device_data->game) ? &GetGameDeviceData(*device_data).mv : nullptr);
   }

protected:
   // The settings' per game text
   struct Tooltips
   {
      const char* smaa = nullptr; // Which of the game's AA passes Luma's SMAA takes the place of
      const char* t2x = nullptr;
      const char* hide_ui = nullptr; // What "Hide Gameplay UI" hides
   };

   const Tooltips tooltips;

   explicit DeadSpaceGame(const Tooltips& tooltips) : tooltips(tooltips)
   {
   }

   // The picked game's first writes of each mirrored target (its "ConfigureLoad")
   static void AutoUpgradeWriters(std::initializer_list<std::span<const uint32_t>> writers)
   {
      for (const std::span<const uint32_t> hashes : writers)
      {
         for (const uint32_t hash : hashes)
         {
            auto_texture_format_upgrade_shader_hashes[hash] = {.rtv_slots = {0}};
         }
      }
   }

   // Only the picked game's replacements compile and bind (its "ConfigureLoad")
   static void RedirectShaders(std::span<const ShaderRedirect> redirects)
   {
      for (const ShaderRedirect& shader : redirects)
      {
         redirected_shader_hashes[shader.name] = {Shader::Hash_NumToStr(shader.hash)};
         if (shader.v281_hash != 0)
         {
            redirected_shader_hashes[std::string(shader.name) + "_v281"] = {Shader::Hash_NumToStr(shader.v281_hash)};
         }
      }
   }

   // The frame camera's view projection in vc4 (row vectors, the engine's, see "OpenScene"): every game's frame camera sits at
   // "object_row_offset + 8" ("MotionVectorGameBase" static_asserts it)
   static constexpr size_t VIEW_PROJECTION_OFFSET = (MotionVectorPatch::DgVoodoo::object_row_offset + 8) * 16;

   // The upscalers that read the reactive mask the alpha blended draws write: FSR and Luma TAA (DLSS's current presets ignore it,
   // DLSS-Best-Practices TRN-2)
   static bool ReactiveMaskUsed(const DeviceData& device_data)
   {
      return g_sr_reactive_enable && GetGameDeviceData(device_data).sr_active && (device_data.sr_type == SR::Type::FSR || device_data.sr_type == SR::Type::LumaTAA);
   }

   // What "SceneMotionVectors" (and "OpenScene") asks of every game. "Self" (CRTP) adds its rows from its MotionVectorPatches.h ("layout",
   // "cameras", "tie_break_rows", "frame_camera", "previous_vertices_slot": a namespace can't be a template argument), "log_tag",
   // "infinite_depth", "ssaa_mods" (and "ssaa_log") and the exe probe ("exe_code_address", "exe_code", "view_projection_address").
   template <typename Self>
   struct MotionVectorGameBase
   {
      static constexpr const char* patch_names[size_t(PixelShaderPatch::COUNT)] = {"MV PS", "MV PS (mask)", "MV PS (mask)", "MV PS (blended)", "MV PS (blended, mask)", "", "", ""};
      // The particles are rewritten on the CPU every frame, so they never match their previous selves: their motion is unknown, they
      // write the reactive mask (FSR-Best-Practices FMSK-7, MGRR's and Vanquish's). The characters are skinned on the CPU too: their
      // vertices are rewritten every frame, their motion comes from the previous ones ("previous_vertices_slot").
      static constexpr PixelShaderPatch blended_mask_patch = PixelShaderPatch::GAME_0;

      // The frame camera's view projection ("Self::view_projection_address"), read only on the exe build that has "Self::exe_code" at
      // "Self::exe_code_address" (see "MatchesExeCode"); null on another build
      static const float* EngineViewProjection()
      {
         static const float* const engine_view_projection = []() -> const float*
         {
            if (!MatchesExeCode(Self::exe_code_address, Self::exe_code))
            {
               reshade::log::message(reshade::log::level::warning,
                  std::format("[{}] Unknown exe build (not the Steam one): no camera, DLAA, FSR, XeSS and Luma TAA stay off", Self::log_tag).c_str());
               return nullptr;
            }
            return reinterpret_cast<const float*>(Self::view_projection_address);
         }();
         return engine_view_projection;
      }

      // The scene: the fp16 mirror of the game's RGBA8 target, or the target itself where it isn't upgraded; single sample, with a
      // shader resource view (the upscaler reads it). The caller only lets the material apply's target through.
      static bool AcceptSceneColor(const D3D11_RENDER_TARGET_VIEW_DESC& rtv_desc, const D3D11_TEXTURE2D_DESC& color_desc)
      {
         const bool format = rtv_desc.Format == DXGI_FORMAT_R16G16B16A16_FLOAT || rtv_desc.Format == DXGI_FORMAT_R8G8B8A8_UNORM || rtv_desc.Format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
         return format && color_desc.SampleDesc.Count == 1 && (color_desc.BindFlags & D3D11_BIND_SHADER_RESOURCE) != 0;
      }

      static bool IsExcludedPixelShader(uint32_t pixel_shader_hash)
      {
         return false;
      }

      // The main view's camera projects. Its determinant is negative: the engines' projections mirror x (_11 < 0). Row vectors: w is
      // each row's 4th value.
      static bool AcceptFrameCamera(const std::vector<uint8_t>& constants)
      {
         static_assert(Self::frame_camera.row * 16 == VIEW_PROJECTION_OFFSET);
         const Math::Matrix44D view_projection = SceneMotionVectors::ReadMatrix(constants, VIEW_PROJECTION_OFFSET);
         return IsProjecting(view_projection.GetData());
      }

      static bool ReactiveMaskUsed(const DeviceData& device_data)
      {
         return DeadSpaceGame::ReactiveMaskUsed(device_data);
      }

      static bool MaskUnmatchedBlended(DeviceData& device_data)
      {
         return ReactiveMaskUsed(device_data);
      }

      static bool CanFill(DeviceData& device_data)
      {
         return GetGameDeviceData(device_data).scene_depth_srv && HasShaders(device_data.native_compute_shaders, CompileTimeStringHash("DS Motion Vector Fill CS"));
      }

      // The marker texels' camera motion from the opaque scene's depth (the end of the scene fills the rest and the masks)
      static void FillMarkersBeforeBlend(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, DeviceData& device_data)
      {
         FillMotionVectors(native_device, native_device_context, device_data, /* write_reactive */ false, Self::infinite_depth);
      }

      // A game pixel shader's "patch", from the bytecode its shader object was created with; empty with "error" if it can't be patched
      static std::vector<uint8_t> PatchPixelShader(PixelShaderPatch patch, const uint8_t* code, size_t code_size, std::string* error)
      {
         switch (patch)
         {
         case PixelShaderPatch::MOTION_VECTORS:
            return MotionVectorPatch::PatchPixelShader(code, code_size, Self::layout, error, /* targets_only */ true);
         case PixelShaderPatch::MASK_ALPHA:
         case PixelShaderPatch::MASK_ADDITIVE:
            return MotionVectorPatch::PatchPixelShaderReactive(code, code_size, Self::layout, SceneMotionVectors::reactive_slot,
               (patch == PixelShaderPatch::MASK_ADDITIVE ? MotionVectorPatch::ReactiveMode::ADDITIVE : MotionVectorPatch::ReactiveMode::ALPHA), error);
         case PixelShaderPatch::BLENDED:
            return MotionVectorPatch::PatchPixelShaderReactive(code, code_size, Self::layout, UINT32_MAX, MotionVectorPatch::ReactiveMode::ALPHA_MOTION_VECTORS, error);
         case blended_mask_patch:
            return MotionVectorPatch::PatchPixelShaderReactive(code, code_size, Self::layout, SceneMotionVectors::reactive_slot, MotionVectorPatch::ReactiveMode::ALPHA_MOTION_VECTORS, error);
         }
         return {};
      }
   };

   // The forward passes' "BLEND_INPUT" output clamp, else the uber's LUT tail ("WrapUberLut"); null for any other pixel shader. Only the
   // uber's LUT permutations get the latter: its structural match scans the whole program, too slow for every shader dgVoodoo translates
   // mid-game. "Color Grading Intensity" and the display mode in Luma's settings, bound on the uber's draws (see "OnDrawOrDispatch").
   static std::unique_ptr<std::byte[]> PatchBlendInputOrUber(const std::byte* code, size_t* size, uint32_t shader_hash, std::span<const uint32_t> blend_input_pixel_shaders,
      std::span<const uint32_t> uber_pixel_shaders)
   {
      if (std::ranges::binary_search(blend_input_pixel_shaders, shader_hash))
         return DXBC::ClampOutput0(code, size, DXBC::Output0Clamp::BLEND_INPUT);
      if (!std::ranges::binary_search(uber_pixel_shaders, shader_hash))
         return nullptr;
      return WrapUberLut(code, size, luma_settings_cbuffer_index, uint32_t(offsetof(CB::LumaGlobalSettings, GameSettings) + offsetof(CB::LumaGameSettings, ColorGradingIntensity)),
         uint32_t(offsetof(CB::LumaGlobalSettings, DisplayMode)));
   }

   // The scene depth (the G-buffer's), from the scene's open or, without motion vectors, the material apply. The fill and SMAA's
   // predication read it through their own view (the game samples dgVoodoo's INTZ copies of it), kept while it is the same resource;
   // the predication linearizes it with the frame camera's projection ("view_projection", the engine's, null if unknown).
   static void TrackSceneDepth(ID3D11Device* native_device, DeadSpaceGameDeviceData* gd, ID3D11Resource* depth, const float* view_projection)
   {
      if (depth != gd->scene_depth.get())
      {
         gd->scene_depth = depth;
         gd->scene_depth_srv.reset();
         const D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = CD3D11_SHADER_RESOURCE_VIEW_DESC(D3D11_SRV_DIMENSION_TEXTURE2D, DXGI_FORMAT_R24_UNORM_X8_TYPELESS);
         native_device->CreateShaderResourceView(depth, &srv_desc, &gd->scene_depth_srv);
      }
      // An orthographic transform left current would give a meaningless A
      if (!view_projection || !IsProjecting(view_projection))
         return;
      const SR::ViewProjectionCamera camera = SR::GetViewProjectionCamera(view_projection, /* row_vectors */ true);
      if (camera.depth_a > 0.0 && camera.near_plane > 0.0)
      {
         gd->pred_depth_scale = float(1.0 / camera.depth_a);
         gd->pred_near_plane = float(camera.near_plane);
      }
   }

   // A Texture2D view of the game's bound "game_srv" (dgVoodoo's are one-slice Texture2DArray views, which Luma's shaders don't
   // declare), in its format
   static ID3D11ShaderResourceView* GetFrameView(ID3D11Device* native_device, DeadSpaceGameDeviceData* gd, ID3D11ShaderResourceView* game_srv)
   {
      com_ptr<ID3D11Resource> resource;
      game_srv->GetResource(&resource);
      if (resource != gd->frame_resource)
      {
         gd->frame_resource = resource;
         gd->frame_srv.reset();
         D3D11_SHADER_RESOURCE_VIEW_DESC game_desc = {};
         game_srv->GetDesc(&game_desc);
         const D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = CD3D11_SHADER_RESOURCE_VIEW_DESC(D3D11_SRV_DIMENSION_TEXTURE2D, game_desc.Format, 0, 1);
         native_device->CreateShaderResourceView(resource.get(), &srv_desc, &gd->frame_srv);
      }
      return gd->frame_srv.get();
   }

   // SMAA ULTRA, 1x or T2x, then RCAS, of "source_srv" into "tex_smaa_out" (predication CS -> DrawSMAA -> T2x resolve -> RCAS).
   // "source_srv" isn't written: sRGB encoded if "srgb_source" (Dead Space 2's MarkerPatch frame copy), else gamma 2.2 (the game's AA
   // pass' input, the scene); the output is encoded the same way. The caller caches and restores the state. False if it didn't draw.
   static bool RunSMAA(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, DeviceData& device_data, ID3D11ShaderResourceView* source_srv, bool srgb_source)
   {
      auto& gd = GetGameDeviceData(device_data);
      const uint2 size = GetViewTextureSize(source_srv);
      if (size.x == 0 || size.y == 0)
         return false;

      // "DrawSMAA" looks its passes up with ".at()": a shader reload must not release them mid-chain (Sunset Overdrive)
      const std::shared_lock lock_shader_objects(s_mutex_shader_objects);
      auto* const copy_vs = FindShader(device_data.native_vertex_shaders, CompileTimeStringHash("Copy VS"));
      // All or nothing: skip SMAA this frame while any pass is missing (async load, live reload)
      if (!copy_vs || !HasSMAAShaders(device_data))
         return false;
      const uint32_t frame_index = cb_luma_global_settings.FrameIndex;
      gd.smaa_frame = frame_index;

      const auto create_target = [&](com_ptr<ID3D11Texture2D>* texture, com_ptr<ID3D11RenderTargetView>* rtv, com_ptr<ID3D11ShaderResourceView>* srv)
      {
         if (*srv && GetViewTextureSize(srv->get()) == size)
            return true;
         texture->reset();
         rtv->reset();
         srv->reset();
         const D3D11_TEXTURE2D_DESC desc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R16G16B16A16_FLOAT, size.x, size.y, 1, 1, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET);
         if (FAILED(native_device->CreateTexture2D(&desc, nullptr, &(*texture))) || FAILED(native_device->CreateRenderTargetView(texture->get(), nullptr, &(*rtv))) ||
             FAILED(native_device->CreateShaderResourceView(texture->get(), nullptr, &(*srv))))
         {
            texture->reset();
            rtv->reset();
            srv->reset();
            return false;
         }
         return true;
      };
      if (!create_target(std::addressof(gd.tex_smaa_out), std::addressof(gd.tex_smaa_out_rtv), std::addressof(gd.tex_smaa_out_srv)))
         return false;
      const bool sharpen = g_rcas_sharpness > 0.f && PrepareRCAS(native_device, device_data) && create_target(std::addressof(gd.tex_rcas_in), std::addressof(gd.tex_rcas_in_rtv), std::addressof(gd.tex_rcas_in_srv));

      // The depth and the motion vectors may be bound as targets (an SRV of a bound target reads as null)
      native_device_context->OMSetRenderTargets(0, nullptr, nullptr);

      // Predication from the scene depth, which maps 1:1 only at the same size. Plain ULTRA when any input is missing (never scale 2.0
      // with a null texture)
      auto* const pred_cs = FindShader(device_data.native_compute_shaders, CompileTimeStringHash("DS Depth Edges CS"));
      bool predication_ready = g_smaa_predication && pred_cs && gd.scene_depth_srv && gd.pred_depth_scale > 0.f && GetViewTextureSize(gd.scene_depth_srv.get()) == size;
      if (predication_ready)
      {
         const CB::DepthEdgesConstants constants = {.tolerance = g_smaa_pred_tolerance, .depth_scale = gd.pred_depth_scale, .near_plane = gd.pred_near_plane};
         predication_ready = PatchedDraws::WriteDynamicConstants(native_device, native_device_context, std::addressof(gd.cb_pred), &constants, sizeof(constants));
         if (!gd.srv_pred || GetViewTextureSize(gd.srv_pred.get()) != size)
         {
            gd.tex_pred.reset();
            gd.uav_pred.reset();
            gd.srv_pred.reset();
            const D3D11_TEXTURE2D_DESC desc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R16_FLOAT, size.x, size.y, 1, 1, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS);
            if (SUCCEEDED(native_device->CreateTexture2D(&desc, nullptr, &gd.tex_pred)))
            {
               native_device->CreateUnorderedAccessView(gd.tex_pred.get(), nullptr, &gd.uav_pred);
               native_device->CreateShaderResourceView(gd.tex_pred.get(), nullptr, &gd.srv_pred);
            }
         }
         predication_ready = predication_ready && gd.uav_pred && gd.srv_pred;
      }
      // Unbinding the mask's UAV before "DrawSMAA" is what makes it readable: an SRV of a resource still bound as a UAV reads as null
      if (predication_ready)
      {
         ID3D11ShaderResourceView* const cs_srv = gd.scene_depth_srv.get();
         ID3D11UnorderedAccessView* const cs_uav = gd.uav_pred.get();
         ID3D11Buffer* const cs_cb = gd.cb_pred.get();
         native_device_context->CSSetUnorderedAccessViews(0, 1, &cs_uav, nullptr);
         native_device_context->CSSetShaderResources(0, 1, &cs_srv);
         native_device_context->CSSetConstantBuffers(0, 1, &cs_cb);
         native_device_context->CSSetShader(pred_cs, nullptr, 0);
         native_device_context->Dispatch((size.x + 7) / 8, (size.y + 7) / 8, 1);
         ID3D11UnorderedAccessView* const null_uav = nullptr;
         ID3D11ShaderResourceView* const null_srv = nullptr;
         native_device_context->CSSetUnorderedAccessViews(0, 1, &null_uav, nullptr);
         native_device_context->CSSetShaderResources(0, 1, &null_srv);
      }

      // SMAA T2x: SMAA into this frame's linear target with the velocity, then the resolve with the previous frame into the output. It
      // needs this frame's motion vectors from the fill, at the frame's size. Without them this frame is 1x and the next one doesn't
      // jitter.
      auto* const t2x_weight_ps = FindShader(device_data.native_pixel_shaders, CompileTimeStringHash("DS SMAA T2x Blending Weight Calculation PS"));
      auto* const t2x_blend_ps = FindShader(device_data.native_pixel_shaders, CompileTimeStringHash("DS SMAA T2x Neighborhood Blending PS"));
      auto* const t2x_resolve_ps = FindShader(device_data.native_pixel_shaders, CompileTimeStringHash("DS SMAA T2x Resolve PS"));
      bool t2x = gd.t2x_active && gd.mv_filled && gd.mv.srv && t2x_weight_ps && t2x_blend_ps && t2x_resolve_ps && GetViewTextureSize(gd.mv.srv.get()) == size;
      const int t2x_current = int(frame_index & 1);
      bool t2x_history = gd.HasT2xHistory(frame_index);
      if (t2x && GetViewTextureSize(gd.t2x_frame_srvs[0].get()) != size)
      {
         t2x_history = false;
         for (int i = 0; i < 2 && t2x; i++)
         {
            t2x = create_target(std::addressof(gd.t2x_frames[i]), std::addressof(gd.t2x_frame_rtvs[i]), std::addressof(gd.t2x_frame_srvs[i]));
         }
         if (!t2x)
         {
            gd.ReleaseT2xFrames();
         }
      }

      // b1: the metrics, the predication threshold scale, the source's encoding (1: sRGB, 0: gamma 2.2) and T2x's subsample indices for
      // the jitter phase (see "OpenScene"), 0 for 1x
      const float subsample_indices = ((t2x && gd.t2x_phase >= 0) ? float(gd.t2x_phase + 1) : 0.f);
      const CB::SmaaMetrics metrics = {
         .rt_metrics = {1.f / float(size.x), 1.f / float(size.y), float(size.x), float(size.y)},
         .predication = {(predication_ready ? 2.f : 1.f), (srgb_source ? 1.f : 0.f), 0.f, 0.f},
         .subsample_indices = {subsample_indices, subsample_indices, subsample_indices, 0.f},
      };
      if (!PatchedDraws::WriteDynamicConstants(native_device, native_device_context, std::addressof(gd.cb_smaa_metrics), &metrics, sizeof(metrics)))
         return false;
      // Metrics at VS+PS b1, bound here rather than through "DrawSMAA()" (which restores the slot): the T2x resolve reads them too
      ID3D11Buffer* const metrics_cb = gd.cb_smaa_metrics.get();
      native_device_context->VSSetConstantBuffers(1, 1, &metrics_cb);
      native_device_context->PSSetConstantBuffers(1, 1, &metrics_cb);

      ID3D11RenderTargetView* const output_rtv = (sharpen ? gd.tex_rcas_in_rtv.get() : gd.tex_smaa_out_rtv.get());
      const SMAAT2xPasses t2x_passes = {.blending_weight_calculation_ps = t2x_weight_ps, .neighborhood_blending_ps = t2x_blend_ps, .velocity = gd.mv.srv.get()};
      DrawSMAA(native_device, native_device_context, device_data, (t2x ? gd.t2x_frame_rtvs[t2x_current].get() : output_rtv), source_srv /* neighborhood blend (decoded) */,
         source_srv /* edge detection (encoded) */, (predication_ready ? gd.srv_pred.get() : nullptr), nullptr /* metrics, bound above */, (t2x ? &t2x_passes : nullptr));

      if (t2x)
      {
         // The resolve: t0 this frame, t1 the previous one or this one again without a history, t2 the motion vectors, s0 linear and
         // s1 point
         native_device_context->OMSetRenderTargets(0, nullptr, nullptr);
         ID3D11ShaderResourceView* const resolve_srvs[2] = {gd.t2x_frame_srvs[t2x_history ? (t2x_current ^ 1) : t2x_current].get(), gd.mv.srv.get()};
         native_device_context->PSSetShaderResources(1, 2, resolve_srvs);
         ID3D11SamplerState* const resolve_samplers[2] = {device_data.sampler_state_linear.get(), device_data.sampler_state_point.get()};
         native_device_context->PSSetSamplers(0, 2, resolve_samplers);
         DrawCustomPixelShader(native_device_context, device_data.default_depth_stencil_state.get(), device_data.default_blend_state.get(), nullptr, copy_vs, t2x_resolve_ps,
            gd.t2x_frame_srvs[t2x_current].get(), output_rtv, size.x, size.y, false);
         ID3D11ShaderResourceView* const null_srvs[3] = {};
         native_device_context->PSSetShaderResources(0, 3, null_srvs);
         gd.t2x_frame = frame_index;
#if DEVELOPMENT
         gd.aa_stats.t2x_resolves++;
#endif
      }

      if (sharpen)
      {
         DrawRCAS(native_device_context, device_data, gd.tex_rcas_in_srv.get(), gd.tex_smaa_out_rtv.get(), g_rcas_sharpness);
      }
      return true;
   }

   // Opens the scene at the frame's first mesh draw into output sized depth (the G-buffer's; shadows and the render to texture screens
   // have their own): takes the scene depth, rolls the last frame's camera and objects over and picks the jitter the whole scene draws
   // with. Once per present. "G": the game's "MotionVectorGame" traits; with "ssaa_mods", larger depth (an SSAA mod's) marks
   // "ssaa_drawn" (and logs "ssaa_log" once).
   template <typename G>
   static void OpenScene(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, const CommandListData& cmd_list_data, DeviceData& device_data, uint32_t vertex_shader_hash, ID3D11DepthStencilView* dsv)
   {
      auto& gd = GetGameDeviceData(device_data);
      com_ptr<ID3D11Resource> depth;
      dsv->GetResource(&depth);
      D3D11_TEXTURE2D_DESC depth_desc = {};
      if (com_ptr<ID3D11Texture2D> depth_texture; SUCCEEDED(depth->QueryInterface(&depth_texture)))
      {
         depth_texture->GetDesc(&depth_desc);
      }
      const uint2 depth_size = {depth_desc.Width, depth_desc.Height};
      if (depth_desc.SampleDesc.Count != 1 || depth_desc.Format != DXGI_FORMAT_R24G8_TYPELESS || depth_size.x != uint32_t(device_data.output_resolution.x) || depth_size.y != uint32_t(device_data.output_resolution.y))
      {
         if constexpr (G::ssaa_mods)
         {
            static bool ssaa_logged = false;
            if (depth_desc.Format == DXGI_FORMAT_R24G8_TYPELESS && depth_size.x > uint32_t(device_data.output_resolution.x))
            {
               gd.ssaa_drawn = true;
               if (!std::exchange(ssaa_logged, true))
               {
                  reshade::log::message(reshade::log::level::warning, G::ssaa_log);
               }
            }
         }
         gd.open_refused_dsv = dsv;
         return;
      }
      UINT viewport_count = 1;
      D3D11_VIEWPORT viewport = {};
      native_device_context->RSGetViewports(&viewport_count, &viewport);
      if (viewport_count == 0 || viewport.Width != float(depth_size.x) || viewport.Height != float(depth_size.y) || !SceneMotionVectors::GetPatchedVertexShader<G>(&gd.mv, native_device, cmd_list_data, device_data, vertex_shader_hash).shader)
         return;
      TrackSceneDepth(native_device, &gd, depth.get(), G::EngineViewProjection());
      // Halton (2, 3) over the upscaler's phase count; pixels to NDC (y up). None until the upscaler is ready (the bridge's helper
      // starting shows the scene as it is).
      const SR::InstanceData* sr_instance_data = (gd.sr_active ? device_data.GetSRInstanceData() : nullptr);
      if (sr_instance_data && !sr_implementations[device_data.sr_type]->IsReady(sr_instance_data))
      {
         sr_instance_data = nullptr;
      }
      std::array<float, 2> jitter = (((sr_instance_data && gd.mv_jitter_allowed) || g_mv_force_jitter) ? GetSRJitter(device_data, sr_instance_data) : std::array<float, 2>{});
      // SMAA T2x: the sample offsets (0.25, 0.25) then (-0.25, -0.25) in y down pixels, for "SMAA.hlsl"'s T2x subsample indices 1 and
      // 2 (Mirror's Edge's). Only after a frame the resolve ran.
      gd.t2x_phase = ((gd.t2x_active && gd.HasT2xHistory(cb_luma_global_settings.FrameIndex)) ? int(cb_luma_global_settings.FrameIndex & 1) : -1);
      if (gd.t2x_phase >= 0)
      {
         const float shift = (gd.t2x_phase == 0 ? -0.25f : 0.25f);
         jitter = {shift, shift};
      }
      if (!SceneMotionVectors::BeginFrame(&gd.mv, native_device, native_device_context, dsv, depth_size, jitter))
      {
         gd.t2x_phase = -1;
         return;
      }
      if (const float* const engine_view_projection = G::EngineViewProjection())
      {
         auto camera = std::make_shared<std::vector<uint8_t>>(VIEW_PROJECTION_OFFSET + SceneMotionVectors::CAMERA_SIZE);
         std::memcpy(camera->data() + VIEW_PROJECTION_OFFSET, engine_view_projection, SceneMotionVectors::CAMERA_SIZE);
         SceneMotionVectors::SetFrameCamera<G>(&gd.mv, std::move(camera));
      }
   }

   // SMAA's predication without a scene opened (no motion vectors, e.g. in the game's AA slot, or an SSAA mod's): the scene depth at the
   // material apply's first draw ("scene_target" then marks it done for the frame). "apply_draw": an immediate game draw of a material
   // apply shader.
   template <typename G>
   static void TrackSceneDepthWithoutScene(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, DeviceData& device_data, bool apply_draw)
   {
      auto& gd = GetGameDeviceData(device_data);
      if (!apply_draw || !g_smaa_enable || gd.sr_active || gd.mv.scene_open || gd.scene_target)
         return;
      com_ptr<ID3D11RenderTargetView> rtv;
      com_ptr<ID3D11DepthStencilView> dsv;
      native_device_context->OMGetRenderTargets(1, &rtv, &dsv);
      if (!rtv || !dsv)
         return;
      rtv->GetResource(&gd.scene_target);
      if constexpr (G::ssaa_mods)
      {
         if (GetViewTextureSize(dsv.get()).x > uint32_t(device_data.output_resolution.x))
         {
            gd.ssaa_drawn = true;
         }
      }
      com_ptr<ID3D11Resource> depth;
      dsv->GetResource(&depth);
      TrackSceneDepth(native_device, &gd, depth.get(), G::EngineViewProjection());
   }

   // DLAA/FSR/XeSS/Luma TAA: the scene's draws with motion vectors or jitter (see "DrawWithMotionVectors"), and the fill and upscaler at
   // "scene_end", the game's first pass after the scene (no scene opens on it). "apply": a material apply shader, whose target is the
   // scene. Then every draw without a patched shader gets the game's own back, if the last patched draw's are still bound. True if the
   // draw was drawn again (patched).
   template <typename G>
   static bool DrawSceneMotionVectors(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, CommandListData& cmd_list_data, DeviceData& device_data,
      reshade::api::shader_stage stages, const ShaderHashesList<OneShaderPerPipeline>& original_shader_hashes, std::function<void()>* original_draw_dispatch_func, bool is_immediate,
      bool game_draw, bool scene_end, bool apply, uint32_t pixel_shader_hash)
   {
      if (!is_immediate)
         return false;
      auto& gd = GetGameDeviceData(device_data);
      if (gd.mv.active)
      {
         // With a vertex shader, which the patched shaders can redo
         const bool game_mesh_draw = game_draw && (stages & reshade::api::shader_stage::vertex) == reshade::api::shader_stage::vertex;
         if (scene_end)
         {
            if (gd.mv.scene_open)
            {
#if DEVELOPMENT
               gd.mv.stats.ended_by = pixel_shader_hash;
#endif
               EndScene(native_device, native_device_context, device_data, G::infinite_depth);
            }
         }
         // Until the scene opens only the depth view matters (the shadow maps, the environment cube and the render to texture screens
         // come first)
         else if (!gd.mv.scene_done && !gd.mv.scene_open && game_mesh_draw)
         {
            com_ptr<ID3D11DepthStencilView> scene_dsv;
            native_device_context->OMGetRenderTargets(0, nullptr, &scene_dsv);
            if (scene_dsv && scene_dsv.get() != gd.open_refused_dsv)
            {
               OpenScene<G>(native_device, native_device_context, cmd_list_data, device_data, original_shader_hashes.vertex_shaders[0], scene_dsv.get());
            }
         }
         if (gd.mv.scene_open && game_mesh_draw)
         {
            com_ptr<ID3D11RenderTargetView> rtvs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
            com_ptr<ID3D11DepthStencilView> dsv;
            native_device_context->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, &rtvs[0], &dsv);
            com_ptr<ID3D11Resource> target;
            if (rtvs[0])
            {
               rtvs[0]->GetResource(&target);
            }
            if (!gd.scene_target && target && apply)
            {
               gd.scene_target = target;
            }
            const bool motion_vectors = target && target == gd.scene_target &&
                                        SceneMotionVectors::DrawWithMotionVectors<G>(&gd.mv, native_device, native_device_context, cmd_list_data, device_data, original_shader_hashes, *original_draw_dispatch_func, rtvs, dsv.get());
            // An alpha blending state blends the motion vector target by the alpha written there: a draw that doesn't write it (refused
            // above) must not have it bound
            if (!motion_vectors && rtvs[SceneMotionVectors::target_slot] && rtvs[SceneMotionVectors::target_slot] == gd.mv.rtv)
            {
               SceneMotionVectors::ClassifyBoundBlend(native_device_context, &gd.mv);
               if (gd.mv.alpha_blended)
               {
                  rtvs[SceneMotionVectors::target_slot].reset();
                  ID3D11RenderTargetView* targets[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
                  for (UINT slot = 0; slot < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; slot++)
                  {
                     targets[slot] = rtvs[slot].get();
                  }
                  native_device_context->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, targets, dsv.get());
               }
            }
            // Only the meshes placed by a camera jitter: the screen passes that test the scene depth (Dead Space 1/2's
            // vPosScaleTranslationVS) keep their pixels
            bool jitter = false;
            if (!motion_vectors && SceneMotionVectors::GetPatchedVertexShader<G>(&gd.mv, native_device, cmd_list_data, device_data, original_shader_hashes.vertex_shaders[0]).camera_offset != 0)
            {
               jitter = SceneMotionVectors::DrawWithJitter<G>(&gd.mv, native_device, native_device_context, cmd_list_data, device_data, original_shader_hashes, *original_draw_dispatch_func, rtvs, dsv.get());
            }
#if DEVELOPMENT
            Mcp::Annotate(cmd_list_data, motion_vectors ? "mv" : (jitter ? "jitter" : "unpatched"));
            if (const SceneMotionVectors::MotionVectorReject reject = std::exchange(gd.mv.draw_reject, SceneMotionVectors::REJECT_COUNT); reject != SceneMotionVectors::REJECT_COUNT)
            {
               Mcp::Annotate(cmd_list_data, "mv_reject", int(reject));
            }
#endif
            if (motion_vectors || jitter)
               return true;
         }
      }
      PatchedDraws::RestoreGameShader(native_device_context, &gd.mv.bound_vertex_shader);
      PatchedDraws::RestoreGameShader(native_device_context, &gd.mv.bound_pixel_shader);
      return false;
   }

   // DLAA, FSR, XeSS or Luma TAA on the jittered scene, its depth (from the fill) and the motion vectors; the result goes back into the
   // scene's color, its alpha kept. False if it didn't draw (missing input, or the upscaler failed). "reactive_mask": the fill wrote
   // this scene's masks.
   static bool DrawUpscaler(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, DeviceData& device_data, bool reactive_mask)
   {
      auto& gd = GetGameDeviceData(device_data);
      auto* const copy_vs = FindShader(device_data.native_vertex_shaders, CompileTimeStringHash("Copy VS"));
      auto* const decode_ps = FindShader(device_data.native_pixel_shaders, CompileTimeStringHash("DS SR Decode PS"));
      auto* const encode_ps = FindShader(device_data.native_pixel_shaders, CompileTimeStringHash("DS SR Encode PS"));
      if (!gd.mv.texture || !gd.mv.camera || !gd.mv.device_depth || !gd.mv.scene_color || !gd.mv.scene_rtv || !gd.sr_rgb_blend_state || !copy_vs || !decode_ps || !encode_ps)
         return false;
      com_ptr<ID3D11Texture2D> scene;
      if (FAILED(gd.mv.scene_color->QueryInterface(&scene)))
         return false;
      D3D11_TEXTURE2D_DESC scene_desc = {};
      scene->GetDesc(&scene_desc);
      SR::InstanceData* const sr_instance_data = device_data.GetSRInstanceData();
      if (!sr_instance_data)
         return false;

      // The scene's view for the decode, in its render target view's format (the target may be typeless)
      if (gd.sr_scene != gd.mv.scene_color)
      {
         gd.sr_scene = gd.mv.scene_color;
         gd.sr_scene_srv.reset();
         D3D11_RENDER_TARGET_VIEW_DESC rtv_desc = {};
         gd.mv.scene_rtv->GetDesc(&rtv_desc);
         const D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = CD3D11_SHADER_RESOURCE_VIEW_DESC(D3D11_SRV_DIMENSION_TEXTURE2D, rtv_desc.Format, 0, 1);
         native_device->CreateShaderResourceView(gd.sr_scene.get(), &srv_desc, &gd.sr_scene_srv);
      }
      D3D11_TEXTURE2D_DESC output_desc = {};
      if (device_data.sr_output_color)
      {
         device_data.sr_output_color->GetDesc(&output_desc);
      }
      if (output_desc.Width != scene_desc.Width || output_desc.Height != scene_desc.Height || !gd.sr_input)
      {
         device_data.sr_output_color.reset();
         gd.ReleaseSRInputs();
         output_desc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R16G16B16A16_FLOAT, scene_desc.Width, scene_desc.Height, 1, 1, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS);
         if (SUCCEEDED(SRBridge::CreateSharableTexture(native_device, output_desc, &device_data.sr_output_color)))
         {
            native_device->CreateShaderResourceView(device_data.sr_output_color.get(), nullptr, &gd.sr_output_srv);
         }
         const D3D11_TEXTURE2D_DESC input_desc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R16G16B16A16_FLOAT, scene_desc.Width, scene_desc.Height, 1, 1, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET);
         if (SUCCEEDED(SRBridge::CreateSharableTexture(native_device, input_desc, &gd.sr_input)))
         {
            native_device->CreateShaderResourceView(gd.sr_input.get(), nullptr, &gd.sr_input_srv);
            native_device->CreateRenderTargetView(gd.sr_input.get(), nullptr, &gd.sr_input_rtv);
         }
      }
      if (!device_data.sr_output_color || !gd.sr_output_srv || !gd.sr_input_srv || !gd.sr_input_rtv || !gd.sr_scene_srv)
      {
         device_data.sr_suppressed = true;
         return false;
      }

      const SR::ViewProjectionCamera camera = SR::GetViewProjectionCamera(reinterpret_cast<const float*>(gd.mv.camera->data() + VIEW_PROJECTION_OFFSET), /* row_vectors */ true);
      const SR::SettingsData settings_data = {
         .output_width = scene_desc.Width,
         .output_height = scene_desc.Height,
         .render_width = scene_desc.Width,
         .render_height = scene_desc.Height,
         // The fp16 mirror of the game's RGBA8 scene (1 = SDR white, the values above it kept by the upgrade), decoded to linear
         .hdr = true,
         // The motion vectors are UV deltas, previous minus current
         .mvs_x_scale = float(scene_desc.Width),
         .mvs_y_scale = float(scene_desc.Height),
         .auto_exposure = device_data.sr_type != SR::Type::FSR,
         // DLAA on HDR input takes preset J or K, never L or M (DLSS-Best-Practices PRE-4): "Default" picks K (11); an explicit choice
         // is kept
         .render_preset = (dlss_render_preset != 0 ? dlss_render_preset : 11u),
      };
      sr_implementations[device_data.sr_type]->UpdateSettings(sr_instance_data, native_device_context, settings_data);

      // FSR requires a FOV (it errors on 0): a camera without an up axis or a usable projection keeps the last ones
      if (camera.vert_fov > 0.0)
      {
         gd.sr_vert_fov = float(camera.vert_fov);
      }
      if (camera.near_plane > 0.0)
      {
         gd.sr_near_plane = float(camera.near_plane);
         gd.sr_far_plane = float(camera.far_plane);
      }
      DrawCustomPixelShader(native_device_context, device_data.default_depth_stencil_state.get(), device_data.default_blend_state.get(), nullptr, copy_vs, decode_ps,
         gd.sr_scene_srv.get(), gd.sr_input_rtv.get(), scene_desc.Width, scene_desc.Height, true);
      const SR::SuperResolutionImpl::DrawData draw_data = {
         .reset = device_data.force_reset_sr,
         .output_color = device_data.sr_output_color.get(),
         .source_color = gd.sr_input.get(),
         .motion_vectors = gd.mv.texture.get(),
         .depth_buffer = gd.mv.device_depth.get(),
         .bias_mask = (reactive_mask ? gd.mv.reactive.get() : nullptr),
         .transparency_alpha = (reactive_mask ? gd.mv.transparency.get() : nullptr),
         // As applied (pixels, +y down)
         .jitter_x = gd.mv.jitter[0],
         .jitter_y = gd.mv.jitter[1],
         .vert_fov = gd.sr_vert_fov,
         .near_plane = gd.sr_near_plane,
         .far_plane = gd.sr_far_plane,
      };
      if (!sr_implementations[device_data.sr_type]->Draw(sr_instance_data, native_device_context, draw_data))
      {
         device_data.sr_suppressed = true;
         return false;
      }
      // RCAS on the linear output into the input (the upscaler is done reading it), then encoded back into the scene. The RGB write mask
      // keeps the scene's alpha (unknown use, not opaque).
      ID3D11ShaderResourceView* encode_source = gd.sr_output_srv.get();
      if (g_rcas_sharpness > 0.f && PrepareRCAS(native_device, device_data))
      {
         DrawRCAS(native_device_context, device_data, gd.sr_output_srv.get(), gd.sr_input_rtv.get(), g_rcas_sharpness);
         encode_source = gd.sr_input_srv.get();
      }
      DrawCustomPixelShader(native_device_context, device_data.default_depth_stencil_state.get(), gd.sr_rgb_blend_state.get(), nullptr, copy_vs, encode_ps,
         encode_source, gd.mv.scene_rtv.get(), scene_desc.Width, scene_desc.Height, true);
      // Not while the bridge's helper starts (the color copied as it is): the next frame resets
      device_data.has_drawn_sr = sr_implementations[device_data.sr_type]->IsReady(sr_instance_data);
      return true;
   }

   // The depth and camera motion fill (see "Luma_DS_MotionVectorFill.hlsl"), with the reactive masks if "write_reactive". The caller
   // caches and restores the state. False if it didn't run. "infinite_depth": the game's "MotionVectorGame::infinite_depth".
   static bool FillMotionVectors(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, DeviceData& device_data, bool write_reactive, bool infinite_depth)
   {
      auto& gd = GetGameDeviceData(device_data);
      auto* const fill_shader = FindShader(device_data.native_compute_shaders, CompileTimeStringHash("DS Motion Vector Fill CS"));
      if (!fill_shader || !gd.mv.uav || !gd.mv.device_depth_uav || !gd.mv.camera || !gd.scene_depth_srv)
         return false;
      // With "infinite_depth", the upscalers' depth as an infinite projection with the same near (FSR-Best-Practices FCAM-2): device z /
      // the projection's A (Dead Space 2's and 3's depth range puts infinity at A ~0.99; "DrawUpscaler" passes the infinite far for A < 1)
      const double depth_a = SR::GetViewProjectionCamera(reinterpret_cast<const float*>(gd.mv.camera->data() + VIEW_PROJECTION_OFFSET), /* row_vectors */ true).depth_a;
      CB::MotionVectorFillConstants constants = {
         .jitter_ndc = {gd.mv.jitter_ndc[0], gd.mv.jitter_ndc[1]},
         .reactive_scale = g_sr_reactive_scale,
         .reactive_threshold = g_sr_reactive_threshold,
         .reactive_enabled = (write_reactive ? 1.f : 0.f),
         .depth_scale = ((infinite_depth && depth_a > 0.0) ? float(1.0 / depth_a) : 1.f),
      };
      for (int i = 0; i < 16; i++)
      {
         constants.reprojection.GetData()[i] = float(gd.mv.reprojection.GetData()[i]);
      }
      if (!PatchedDraws::WriteDynamicConstants(native_device, native_device_context, std::addressof(gd.mv.fill_buffer), &constants, sizeof(constants)))
         return false;
      // The depth and the motion vectors may be bound as targets
      native_device_context->OMSetRenderTargets(0, nullptr, nullptr);
      ID3D11Buffer* const buffer = gd.mv.fill_buffer.get();
      ID3D11ShaderResourceView* const srvs[2] = {gd.scene_depth_srv.get(), (write_reactive ? gd.mv.reactive_target_srv.get() : nullptr)};
      ID3D11UnorderedAccessView* const uavs[4] = {gd.mv.uav.get(), gd.mv.device_depth_uav.get(), (write_reactive ? gd.mv.reactive_uav.get() : nullptr), (write_reactive ? gd.mv.transparency_uav.get() : nullptr)};
      native_device_context->CSSetConstantBuffers(0, 1, &buffer);
      native_device_context->CSSetShaderResources(0, UINT(std::size(srvs)), srvs);
      native_device_context->CSSetUnorderedAccessViews(0, UINT(std::size(uavs)), uavs, nullptr);
      native_device_context->CSSetShader(fill_shader, nullptr, 0);
      const uint2 size = gd.mv.scene_size;
      native_device_context->Dispatch((size.x + 7) / 8, (size.y + 7) / 8, 1);
      ID3D11UnorderedAccessView* const null_uavs[std::size(uavs)] = {};
      ID3D11ShaderResourceView* const null_srvs[std::size(srvs)] = {};
      native_device_context->CSSetUnorderedAccessViews(0, UINT(std::size(null_uavs)), null_uavs, nullptr);
      native_device_context->CSSetShaderResources(0, UINT(std::size(null_srvs)), null_srvs);
      gd.mv_filled = true;
      if (write_reactive)
      {
         gd.mv.sr_reactive_frame = cb_luma_global_settings.FrameIndex;
      }
      return true;
   }

   // Ends the scene at the first pass after it: the fill (the last camera motion, the depth, the reactive masks), then the upscaler,
   // both before that pass reads the scene
   static void EndScene(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, DeviceData& device_data, bool infinite_depth)
   {
      auto& gd = GetGameDeviceData(device_data);
      gd.mv.scene_open = false;
      gd.mv.scene_done = true;
      DrawStateStack<DrawStateStackType::FullGraphics> graphics_state;
      DrawStateStack<DrawStateStackType::Compute> compute_state;
      graphics_state.Cache(native_device_context, device_data.uav_max_count);
      compute_state.Cache(native_device_context, device_data.uav_max_count);
      // The reactive and transparency & composition masks, written by the fill from what the alpha blended draws wrote
      const bool reactive = ReactiveMaskUsed(device_data) && gd.mv.reactive_target_srv;
      const uint2 size = gd.mv.scene_size;
      if (reactive && GetViewTextureSize(gd.mv.reactive_target_srv.get()) == size)
      {
         D3D11_TEXTURE2D_DESC desc = {};
         if (gd.mv.reactive)
         {
            gd.mv.reactive->GetDesc(&desc);
         }
         if (desc.Width != size.x || desc.Height != size.y)
         {
            const std::unique_lock lock(gd.mv.mutex);
            gd.mv.reactive.reset();
            gd.mv.reactive_uav.reset();
            gd.mv.transparency.reset();
            gd.mv.transparency_uav.reset();
            desc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R8_UNORM, size.x, size.y, 1, 1, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS);
            if (SUCCEEDED(SRBridge::CreateSharableTexture(native_device, desc, &gd.mv.reactive)))
            {
               native_device->CreateUnorderedAccessView(gd.mv.reactive.get(), nullptr, &gd.mv.reactive_uav);
            }
            if (SUCCEEDED(SRBridge::CreateSharableTexture(native_device, desc, &gd.mv.transparency)))
            {
               native_device->CreateUnorderedAccessView(gd.mv.transparency.get(), nullptr, &gd.mv.transparency_uav);
            }
         }
      }
      const bool write_reactive = reactive && gd.mv.reactive_uav && gd.mv.transparency_uav;
      const bool filled = std::exchange(gd.mv.fill_pending, false) && FillMotionVectors(native_device, native_device_context, device_data, write_reactive, infinite_depth);
      // The upscaler's depth comes from the fill
      if (gd.sr_active && filled)
      {
         [[maybe_unused]] const bool drawn = DrawUpscaler(native_device, native_device_context, device_data, write_reactive);
#if DEVELOPMENT
         gd.mv.stats.sr_draws += drawn;
#endif
         gd.aa_drawn = device_data.has_drawn_sr;
      }
      // Luma's SMAA where the game's AA doesn't draw: on a copy of the scene (gamma 2.2 encoded, the values above 1 kept by the fp16
      // mirror), back into it without its alpha
      else if (g_smaa_enable && !gd.sr_active && !gd.smaa_slot_a && gd.mv.scene_color && gd.mv.scene_rtv && gd.sr_rgb_blend_state)
      {
         com_ptr<ID3D11Texture2D> scene;
         D3D11_TEXTURE2D_DESC scene_desc = {};
         D3D11_RENDER_TARGET_VIEW_DESC rtv_desc = {};
         if (SUCCEEDED(gd.mv.scene_color->QueryInterface(&scene)))
         {
            scene->GetDesc(&scene_desc);
            gd.mv.scene_rtv->GetDesc(&rtv_desc);
         }
         auto* const copy_vs = FindShader(device_data.native_vertex_shaders, CompileTimeStringHash("Copy VS"));
         auto* const copy_ps = FindShader(device_data.native_pixel_shaders, CompileTimeStringHash("Copy PS"));
         if (scene && copy_vs && copy_ps && EnsureFrameCopy(native_device, scene_desc, rtv_desc.Format, std::addressof(gd.tex_snapshot), std::addressof(gd.srv_snapshot)))
         {
            native_device_context->CopyResource(gd.tex_snapshot.get(), scene.get());
            if (RunSMAA(native_device, native_device_context, device_data, gd.srv_snapshot.get(), /* srgb_source */ false))
            {
               DrawCustomPixelShader(native_device_context, device_data.default_depth_stencil_state.get(), gd.sr_rgb_blend_state.get(), nullptr, copy_vs, copy_ps,
                  gd.tex_smaa_out_srv.get(), gd.mv.scene_rtv.get(), scene_desc.Width, scene_desc.Height, true);
               gd.aa_drawn = true;
#if DEVELOPMENT
               gd.aa_stats.smaa_slot_b++;
#endif
            }
         }
      }
      compute_state.Restore(native_device_context);
      graphics_state.Restore(native_device_context);
   }

   // Keeps "texture" a copy target matching "frame_desc", with "srv" its view in "view_format" (the render target view's: the frame
   // may be typeless); recreated on any change. False if either can't be created. Pass the members with std::addressof: com_ptr's
   // operator& is the COM out parameter.
   static bool EnsureFrameCopy(ID3D11Device* native_device, const D3D11_TEXTURE2D_DESC& frame_desc, DXGI_FORMAT view_format, com_ptr<ID3D11Texture2D>* texture,
      com_ptr<ID3D11ShaderResourceView>* srv)
   {
      D3D11_TEXTURE2D_DESC copy_desc = {};
      D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
      if (*texture && *srv)
      {
         (*texture)->GetDesc(&copy_desc);
         (*srv)->GetDesc(&srv_desc);
      }
      if (copy_desc.Width == frame_desc.Width && copy_desc.Height == frame_desc.Height && copy_desc.Format == frame_desc.Format && srv_desc.Format == view_format)
         return true;
      srv->reset();
      texture->reset();
      copy_desc = CD3D11_TEXTURE2D_DESC(frame_desc.Format, frame_desc.Width, frame_desc.Height, 1, 1, D3D11_BIND_SHADER_RESOURCE);
      srv_desc = CD3D11_SHADER_RESOURCE_VIEW_DESC(D3D11_SRV_DIMENSION_TEXTURE2D, view_format, 0, 1);
      if (SUCCEEDED(native_device->CreateTexture2D(&copy_desc, nullptr, &(*texture))) && SUCCEEDED(native_device->CreateShaderResourceView(texture->get(), &srv_desc, &(*srv))))
         return true;
      texture->reset();
      return false;
   }

public:
   // Core's motion vector events: the motion vector target in every blend state, the tracked bindings and the vc4 copies
   static void SetEvents(bool enable)
   {
      SceneMotionVectors::SetEvents<GetSceneMotionVectors>(enable);
   }

   // The subclass registers its own shaders and "sr_game_tooltip" first
   void OnInit(bool async) override
   {
      // The scene stays gamma encoded (1 = paper white) up to the game's display map (see the subclass), pre-scaled by Game/UI paper
      // white (UI_DRAW_TYPE 2); the composition encodes scRGB.
      GetShaderDefineData(POST_PROCESS_SPACE_TYPE_HASH).SetDefaultValue('0');
      GetShaderDefineData(EARLY_DISPLAY_ENCODING_HASH).SetDefaultValue('0');
      GetShaderDefineData(VANILLA_ENCODING_TYPE_HASH).SetDefaultValue('1'); // Gamma 2.2 in and out, as the other D3D9 games under the same wrapper
      GetShaderDefineData(GAMMA_CORRECTION_TYPE_HASH).SetDefaultValue('1');
      GetShaderDefineData(GAMUT_MAPPING_TYPE_HASH).SetDefaultValue('1');
      GetShaderDefineData(UI_DRAW_TYPE_HASH).SetDefaultValue('2');

      // Scene and UI paper white sliders (the UI one needs UI_DRAW_TYPE >= 1 and no OS reference white)
      use_os_reference_white_level = false;

      default_luma_global_game_settings.Exposure = 1.f;
      default_luma_global_game_settings.Contrast = 1.f;
      default_luma_global_game_settings.Saturation = 1.f;
      default_luma_global_game_settings.HighlightsDesaturation = 0.f;
      default_luma_global_game_settings.ColorGradingIntensity = 1.f; // The vanilla LUT grade is kept as is
      default_luma_global_game_settings.Dithering = 1.f;
      // The videos bypass the scene: without AutoHDR they sit flat at paper white. BL2's and Mirror's Edge's pair.
      default_luma_global_game_settings.VideoAutoHDREnable = 1.f;
      default_luma_global_game_settings.VideoAutoHDRBoost = 0.5f;
      // Dead Space 1/2's gamma ramp ("GammaRamp"); neutral and unread in Dead Space 3, which has none
      default_luma_global_game_settings.GammaRampExponent = 1.f;
      cb_luma_global_settings.GameSettings = default_luma_global_game_settings;
      // Occupancy unmeasured past the traced menus and gameplay: Dead Space 3's dgVoodoo binds b0, b3 and b4 so far, Dead Space 2's
      // 2.87.x translations b0-b4 only, 2.81.3's b0-b5
      luma_settings_cbuffer_index = 13;
      luma_data_cbuffer_index = 12;
      luma_ui_cbuffer_index = -1;

      // DLSS/FSR/XeSS/Luma TAA: their depth and the camera motion from the scene depth, the CPU copies of vc4 (dgVoodoo maps it or
      // updates it), and the motion vector target written by every blend state
      native_shaders_definitions.emplace(CompileTimeStringHash("DS Motion Vector Fill CS"),
         ShaderDefinition{.file_name = "Luma_DS_MotionVectorFill", .type = reshade::api::pipeline_subobject_type::compute_shader});
      native_shaders_definitions.emplace(CompileTimeStringHash("DS SR Decode PS"),
         ShaderDefinition{.file_name = "Luma_DS_SRGamma", .type = reshade::api::pipeline_subobject_type::pixel_shader, .function_name = "decode_ps"});
      native_shaders_definitions.emplace(CompileTimeStringHash("DS SR Encode PS"),
         ShaderDefinition{.file_name = "Luma_DS_SRGamma", .type = reshade::api::pipeline_subobject_type::pixel_shader, .function_name = "encode_ps"});
      // Core registers the 6 SMAA passes and RCAS; these are the games' own. SMAA predication: the scene depth -> R16F plane-deviation
      // edge-ness
      native_shaders_definitions.emplace(CompileTimeStringHash("DS Depth Edges CS"),
         ShaderDefinition{.file_name = "Luma_DS_DepthEdges", .type = reshade::api::pipeline_subobject_type::compute_shader});
      // SMAA T2x's own passes ("SMAA_T2X" in "Luma_SMAA_impl.hlsl"). The edge detection and the vertex shaders are 1x's.
      native_shaders_definitions.emplace(CompileTimeStringHash("DS SMAA T2x Blending Weight Calculation PS"),
         ShaderDefinition{.file_name = "Luma_SMAA_impl", .type = reshade::api::pipeline_subobject_type::pixel_shader, .function_name = "smaa_blending_weight_calculation_ps", .defines_data = {{"SMAA_T2X", "1"}}});
      native_shaders_definitions.emplace(CompileTimeStringHash("DS SMAA T2x Neighborhood Blending PS"),
         ShaderDefinition{.file_name = "Luma_SMAA_impl", .type = reshade::api::pipeline_subobject_type::pixel_shader, .function_name = "smaa_neighborhood_blending_ps", .defines_data = {{"SMAA_T2X", "1"}}});
      native_shaders_definitions.emplace(CompileTimeStringHash("DS SMAA T2x Resolve PS"),
         ShaderDefinition{.file_name = "Luma_SMAA_impl", .type = reshade::api::pipeline_subobject_type::pixel_shader, .function_name = "smaa_resolve_ps", .defines_data = {{"SMAA_T2X", "1"}}});
      SetEvents(true);

#if DEVELOPMENT
      // For the MCP "luma_dev_values" tool
      Mcp::RegisterToggles({{"smaa_enable", &g_smaa_enable}, {"smaa_t2x", &g_smaa_t2x}, {"smaa_predication", &g_smaa_predication}, {"mv_enable", &g_mv_enable}, {"mv_debug_view", &g_mv_debug_view},
         {"mv_force_jitter", &g_mv_force_jitter}, {"sr_reactive_enable", &g_sr_reactive_enable}, { "sr_reactive_debug_view",
            &g_sr_reactive_debug_view }});
      Mcp::RegisterValues({{"rcas_sharpness", &g_rcas_sharpness, 0.f, 1.f}, {"smaa_pred_tolerance", &g_smaa_pred_tolerance, 0.002f, 0.2f}, {"sr_reactive_scale", &g_sr_reactive_scale, 0.f, 4.f},
         {"sr_reactive_threshold", &g_sr_reactive_threshold, 0.f, 1.f}});
      Mcp::RegisterTextures({MCP_GAME_TEXTURE("smaa.output", tex_smaa_out), MCP_GAME_TEXTURE("smaa.pred_mask", tex_pred), MCP_GAME_TEXTURE("mv.velocity", mv.texture),
         MCP_GAME_TEXTURE("mv.depth", mv.device_depth), MCP_GAME_TEXTURE("sr.reactive", mv.reactive), MCP_GAME_TEXTURE("sr.transparency", mv.transparency),
         MCP_GAME_TEXTURE("sr.draws_mask", mv.reactive_target), MCP_GAME_TEXTURE("sr.input", sr_input), MCP_GAME_TEXTURE("sr.output", sr_output_srv),
         MCP_GAME_TEXTURE("smaa.input", frame_resource), MCP_GAME_TEXTURE("smaa.snapshot", tex_snapshot), MCP_GAME_TEXTURE("smaa.rcas_input", tex_rcas_in),
         MCP_GAME_TEXTURE("smaa.t2x_frame0", t2x_frames[0]), MCP_GAME_TEXTURE("smaa.t2x_frame1", t2x_frames[1]), MCP_GAME_TEXTURE("scene.depth", scene_depth)});
#endif
   }

   void OnCreateDevice(ID3D11Device* native_device, DeviceData& device_data) override
   {
      device_data.game = new DeadSpaceGameDeviceData;
      auto& gd = GetGameDeviceData(device_data);
      // The upscaler's and the scene's end SMAA's output go back into the scene without its alpha (unknown use, not opaque)
      D3D11_BLEND_DESC blend_desc = CD3D11_BLEND_DESC(D3D11_DEFAULT);
      blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_RED | D3D11_COLOR_WRITE_ENABLE_GREEN | D3D11_COLOR_WRITE_ENABLE_BLUE;
      native_device->CreateBlendState(&blend_desc, &gd.sr_rgb_blend_state);
#if DEVELOPMENT
      // For the MCP "luma_dev_values" tool: the last complete frame's counts
      SceneMotionVectors::RegisterDevValues(gd.mv, &device_data);
      const auto& stats = gd.last_aa_stats;
      Mcp::RegisterCounters({{"aa.smaa_slot_a", &stats.smaa_slot_a}, {"aa.smaa_slot_b", &stats.smaa_slot_b}, {"aa.t2x_resolves", &stats.t2x_resolves},
                               {"aa.game_aa_copies", &stats.game_aa_copies}, {"aa.game_aa", &stats.game_aa}, {"aa.smaa_slot_a_chosen", &stats.smaa_slot_a_chosen},
                               { "aa.t2x_active",
                                  &stats.t2x_active }},
         &device_data);
#endif
   }

   void OnDestroyDeviceData(DeviceData& device_data) override
   {
#if DEVELOPMENT
      Mcp::Unregister(&device_data);
#endif
      Game::OnDestroyDeviceData(device_data);
   }

   // Every game's per frame state resets here, also the fields another game alone uses (unused, they stay at their defaults)
   void OnPresent(ID3D11Device* native_device, DeviceData& device_data) override
   {
      auto& gd = GetGameDeviceData(device_data);
      gd.open_refused_dsv = nullptr;
      gd.scene_target.reset();

      // DLSS/FSR/XeSS/Luma TAA: the history restarts after any frame it didn't draw (menus, loading, just picked); the selection and the
      // motion vector state are fixed here for the next frame (see "sr_active")
      gd.mv_jitter_allowed = device_data.has_drawn_sr;
      gd.sr_active = LatchSRFrame(device_data);
      // Luma's SMAA gives way to an upscaler. It takes the game's AA slot if that drew last frame, else the scene's end (with the motion
      // vector path's scene tracking). T2x runs on the upscalers' motion vectors, not with an SSAA mod ("OpenScene" refuses its scene);
      // turned off, its frames go, and the motion vectors go as when no upscaler is picked.
      const bool game_aa = std::exchange(gd.game_aa_drawn, false);
      gd.ssaa_active = std::exchange(gd.ssaa_drawn, false);
      // Without its shaders (still compiling, failed) Luma's SMAA can't take the game's slot, which then still antialiases
      const bool smaa_available = g_smaa_enable && HasSMAAShaders(device_data);
      gd.smaa_slot_a = smaa_available && !gd.sr_active && game_aa;
#if DEVELOPMENT
      gd.aa_stats.game_aa = game_aa;
      gd.aa_stats.ssaa = gd.ssaa_active;
      gd.aa_stats.smaa_slot_a_chosen = gd.smaa_slot_a;
      gd.aa_stats.t2x_active = gd.t2x_active;
      gd.last_aa_stats = std::exchange(gd.aa_stats, {});
#endif
      gd.aa_drawn = false;
      gd.display_mapped = false;
      gd.uber_drawn = false;
      gd.post_target.reset();
      gd.ui_bound_pending.reset();
      gd.ui_bound_valid = false;
      gd.scene_applied = false;
      gd.post_started = false;
      gd.ui_layer_drawn = false;
      gd.t2x_phase = -1;
      gd.mv_filled = false;
      const bool t2x_was_active = gd.t2x_active;
      gd.t2x_active = g_smaa_enable && g_smaa_t2x && !gd.sr_active && !gd.ssaa_active;
      if (t2x_was_active && !gd.t2x_active)
      {
         gd.ReleaseT2xFrames();
         device_data.release_sr_resources = true;
      }
      if (gd.t2x_frames[0] && cb_luma_global_settings.FrameIndex - gd.t2x_frame > IDLE_RELEASE_FRAMES)
      {
         gd.ReleaseT2xFrames();
      }
      // SMAA's resources go once it stopped running (MEM-11)
      if (gd.tex_smaa_out && cb_luma_global_settings.FrameIndex - gd.smaa_frame > IDLE_RELEASE_FRAMES)
      {
         gd.ReleaseSMAAScratch();
         ReleaseSMAAIntermediates(device_data);
      }
      // ponytail: the scene's end SMAA 1x draws the motion vectors it doesn't use; split the scene tracking from the motion vector draws
      // if its cost shows
      const bool smaa_scene_end = smaa_available && !gd.sr_active && !game_aa;
      // The uploads the buffer hooks missed while motion vectors were off make every earlier copy stale
      const bool mv_active = gd.sr_active || gd.t2x_active || g_mv_enable || smaa_scene_end;
      if (!gd.mv.active.exchange(mv_active) && mv_active)
      {
         gd.mv.constants.Restart();
      }
      // None picked: Core stopped the SR bridge's helper ("ReleaseResources"), our upscaler inputs and output go too. Recreated when an
      // upscaler is picked again (the helper takes seconds to start).
      if (device_data.sr_type == SR::Type::None && device_data.release_sr_resources.exchange(false))
      {
         gd.ReleaseSRInputs();
         gd.sr_scene.reset();
         gd.sr_scene_srv.reset();
         if (!g_mv_enable && !gd.t2x_active && !smaa_scene_end)
         {
            const std::unique_lock lock(gd.mv.mutex);
            gd.mv.ReleaseMotionVectorTargets();
            // The game's scene and depth (taken again at the next scene), so a resize after None doesn't keep the old ones alive; and
            // the vc4 copies the object tables and the cameras hold
            gd.mv.depth.reset();
            gd.mv.scene_color.reset();
            gd.mv.scene_rtv.reset();
            gd.scene_depth.reset();
            gd.scene_depth_srv.reset();
            gd.mv.objects.Reset();
            gd.mv.camera.reset();
            gd.mv.previous_camera.reset();
            gd.mv.fill_buffer.reset();
            gd.mv.jitter_buffer.reset();
            gd.mv.previous_constants = {}; // Its 4 MiB dynamic ring is address space too
            gd.mv.constants.ReleasePool();
         }
      }
      gd.mv.constants.TrimPool(gd.mv.active);
#if DEVELOPMENT
      gd.mv.stats.constants_pool = uint32_t(gd.mv.constants.pool.size());
#endif
      // The reactive masks go once the fill stopped writing them (DLSS, the masks off)
      if (gd.mv.reactive_target && cb_luma_global_settings.FrameIndex - gd.mv.sr_reactive_frame > IDLE_RELEASE_FRAMES)
      {
         const std::unique_lock lock(gd.mv.mutex);
         gd.mv.ReleaseReactiveMasks();
      }
      // A scene nothing ended ends here: its jitter must not reach the next frame's draws before its first mesh
      gd.mv.scene_open = false;
      gd.mv.scene_done = false;
      gd.mv.frame_ended = true;
      gd.mv.fill_pending = false;
      // The upscalers' mip bias, once the upscaler draws (Core biases the upgraded samplers)
      SetTextureMipLodBias(nullptr, device_data, ((gd.sr_active && gd.mv_jitter_allowed) ? SR::GetMipLODBias(device_data.sr_type, device_data.output_resolution.y, device_data.output_resolution.y) : 0.f));
#if DEVELOPMENT
      gd.mv.last_stats = std::exchange(gd.mv.stats, {});
      // "MV Debug View": Core's debug draw of the target, absolute values in pixels
      {
         const std::shared_lock lock(gd.mv.mutex);
         if (g_mv_debug_view && gd.mv.texture)
         {
            D3D11_TEXTURE2D_DESC desc = {};
            gd.mv.texture->GetDesc(&desc);
            debug_draw_auto_clear_texture = false;
            debug_draw_options |= (uint32_t)DebugDrawTextureOptionsMask::Abs | (uint32_t)DebugDrawTextureOptionsMask::UVToPixelSpace;
            device_data.debug_draw_texture = gd.mv.texture.get();
            device_data.debug_draw_texture_format = desc.Format;
            device_data.debug_draw_texture_size = {desc.Width, desc.Height, 1, 1};
         }
         else if (g_sr_reactive_debug_view && gd.mv.reactive)
         {
            debug_draw_auto_clear_texture = false;
            debug_draw_options |= (uint32_t)DebugDrawTextureOptionsMask::RedOnly;
            debug_draw_options &= ~((uint32_t)DebugDrawTextureOptionsMask::Abs | (uint32_t)DebugDrawTextureOptionsMask::UVToPixelSpace);
            device_data.debug_draw_texture = gd.mv.reactive.get();
            device_data.debug_draw_texture_format = DXGI_FORMAT_R8_UNORM;
            device_data.debug_draw_texture_size = {uint32_t(device_data.output_resolution.x), uint32_t(device_data.output_resolution.y), 1, 1};
         }
         else if (device_data.debug_draw_texture && (device_data.debug_draw_texture.get() == gd.mv.texture.get() || device_data.debug_draw_texture.get() == gd.mv.reactive.get()))
         {
            device_data.debug_draw_texture = nullptr;
         }
      }
#endif
   }

   void LoadConfigs() override
   {
      auto& gs = cb_luma_global_settings.GameSettings;
      reshade::get_config_value(nullptr, NAME, "Exposure", gs.Exposure);
      reshade::get_config_value(nullptr, NAME, "Contrast", gs.Contrast);
      reshade::get_config_value(nullptr, NAME, "Saturation", gs.Saturation);
      reshade::get_config_value(nullptr, NAME, "HighlightsDesaturation", gs.HighlightsDesaturation);
      reshade::get_config_value(nullptr, NAME, "ColorGradingIntensity", gs.ColorGradingIntensity);
      reshade::get_config_value(nullptr, NAME, "Dithering", gs.Dithering);
      reshade::get_config_value(nullptr, NAME, "VideoAutoHDREnable", gs.VideoAutoHDREnable);
      reshade::get_config_value(nullptr, NAME, "VideoAutoHDRBoost", gs.VideoAutoHDRBoost);
      reshade::get_config_value(nullptr, NAME, "SMAAEnable", g_smaa_enable);
      reshade::get_config_value(nullptr, NAME, "SMAAT2x", g_smaa_t2x);
      reshade::get_config_value(nullptr, NAME, "RCASSharpness", g_rcas_sharpness);
      reshade::get_config_value(nullptr, NAME, "HideUI", hide_ui);
#if DEVELOPMENT
      reshade::get_config_value(nullptr, NAME, "SMAAPredication", g_smaa_predication);
      reshade::get_config_value(nullptr, NAME, "SMAAPredicationTolerance", g_smaa_pred_tolerance);
#endif
   }

   void DrawImGuiSettings(DeviceData& device_data) override
   {
      auto& gs = cb_luma_global_settings.GameSettings;
      const auto& gs_def = default_luma_global_game_settings;
      ImGui::SeparatorText("Anti-Aliasing");
      const auto& gd = GetGameDeviceData(device_data);
      // The upscaler (Super Resolution, in the Settings tab) replaces SMAA
      DrawSettingCheckbox("SMAA Enable", "SMAAEnable", &g_smaa_enable, true, tooltips.smaa, gd.sr_active);
      // T2x needs the motion vectors, which an SSAA mod's scene doesn't get
      const bool t2x_unavailable = gd.sr_active || gd.ssaa_active;
      ImGui::BeginDisabled(!g_smaa_enable || t2x_unavailable);
      DrawSettingCheckbox("SMAA T2x", "SMAAT2x", &g_smaa_t2x, false, tooltips.t2x, t2x_unavailable);
      ImGui::EndDisabled();
      ImGui::BeginDisabled(!g_smaa_enable && !gd.sr_active);
      DrawSettingSlider("RCAS Sharpness", "RCASSharpness", &g_rcas_sharpness, 0.f, 0.f, 1.f, "Sharpening applied on top of anti-aliasing (0 = off).");
      ImGui::EndDisabled();
#if DEVELOPMENT
      DrawSettingCheckbox("SMAA Predication", "SMAAPredication", &g_smaa_predication, true, nullptr);
      DrawSettingSlider("SMAA Predication Tolerance", "SMAAPredicationTolerance", &g_smaa_pred_tolerance, 0.02f, 0.002f, 0.2f, nullptr, "%.3f", ImGuiSliderFlags_Logarithmic);
#endif

      ImGui::SeparatorText("Grade");
      DrawSettingSlider("Exposure", "Exposure", &gs.Exposure, gs_def.Exposure, 0.f, 2.f, "Overall image brightness (1 = vanilla).");
      DrawSettingSlider("Contrast", "Contrast", &gs.Contrast, gs_def.Contrast, 0.f, 2.f, "Overall image contrast, HDR only (1 = vanilla).");
      DrawSettingSlider("Saturation", "Saturation", &gs.Saturation, gs_def.Saturation, 0.f, 2.f, "Color saturation, HDR only (1 = vanilla).");
      DrawSettingSlider("Highlights Desaturation", "HighlightsDesaturation", &gs.HighlightsDesaturation, gs_def.HighlightsDesaturation, 0.f, 1.f,
         "How far the brightest sources fade to neutral white, HDR only (0 = keep color at any brightness).");
      DrawSettingSlider("Color Grading Intensity", "ColorGradingIntensity", &gs.ColorGradingIntensity, gs_def.ColorGradingIntensity, 0.f, 1.f,
         "Strength of the game's own color grading (1 = vanilla, 0 = neutral).");

      ImGui::SeparatorText("Effects");
      DrawSettingCheckbox("Video AutoHDR", "VideoAutoHDREnable", &gs.VideoAutoHDREnable, gs_def.VideoAutoHDREnable, "Adds HDR highlights to pre-rendered videos (HDR only).");
      if (gs.VideoAutoHDREnable > 0.5f)
      {
         DrawSettingSlider("Video HDR Boost", "VideoAutoHDRBoost", &gs.VideoAutoHDRBoost, gs_def.VideoAutoHDRBoost, 0.f, 1.f, "Video highlight strength (0 = off).");
      }
      DrawSettingCheckbox("Dithering", "Dithering", &gs.Dithering, gs_def.Dithering, "Reduces gradient banding.");

      ImGui::SeparatorText("UI");
      DrawSettingCheckbox("Hide Gameplay UI", "HideUI", &hide_ui, false, tooltips.hide_ui);

#if DEVELOPMENT
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
   }

   // The build date and the credits; Dead Space 1/2 put their introduction and links before them ("LumaDisplayMapGame")
   void PrintImGuiAbout() override
   {
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
                  "\nAMD FidelityFX (RCAS + FSR 3)"
                  "\nNVIDIA NGX (DLSS)"
                  "\nIntel XeSS"
                  "\ndgVoodoo2 by Dege (DirectX 9 -> 11 wrapper, required)");
   }
};

// Dead Space 1 and 2: no game display pass, so the HDR display map runs where the scene ends, before what draws over it (each game's
// "scene_over" rule, "DrawDisplayMapOverScene"), else in dgVoodoo's present blit (Includes/DS_Present.hlsl), which also bounds what
// drew after the map ("ui_bound"). The brightness slider is a D3D9 gamma ramp ("GammaRamp").
class LumaDisplayMapGame : public DeadSpaceGame
{
   const char* const about_name; // The game's name in "PrintImGuiAbout"

protected:
   LumaDisplayMapGame(const Tooltips& tooltips, const char* about_name) : DeadSpaceGame(tooltips), about_name(about_name)
   {
   }

   // The game's IDirect3DDevice9* (for "GammaRamp"): null on another exe build or before the device exists
   virtual IDirect3DDevice9* GetGameDevice() const = 0;

   // Copies the frame into "ui_bound", whose view is in "view_format" (the render target view's: the frame may be typeless)
   static void CaptureUiBound(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, DeadSpaceGameDeviceData* gd, ID3D11Resource* frame_resource, DXGI_FORMAT view_format)
   {
      com_ptr<ID3D11Texture2D> frame;
      frame_resource->QueryInterface(&frame);
      if (!frame)
         return;
      D3D11_TEXTURE2D_DESC frame_desc = {};
      frame->GetDesc(&frame_desc);
      gd->ui_bound_valid = EnsureFrameCopy(native_device, frame_desc, view_format, std::addressof(gd->ui_bound), std::addressof(gd->ui_bound_srv));
      if (gd->ui_bound_valid)
      {
         native_device_context->CopyResource(gd->ui_bound.get(), frame.get());
      }
   }

   // The HDR display map in place on the frame ("target"), from a copy of it; see "Luma_DS_DisplayMap.hlsl" (registered as "DS Display
   // Map PS")
   static void DrawDisplayMap(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, CommandListData& cmd_list_data, DeviceData& device_data, ID3D11RenderTargetView* target)
   {
      auto& gd = GetGameDeviceData(device_data);
      auto* const copy_vs = FindShader(device_data.native_vertex_shaders, CompileTimeStringHash("Copy VS"));
      auto* const display_map_ps = FindShader(device_data.native_pixel_shaders, CompileTimeStringHash("DS Display Map PS"));
      if (!copy_vs || !display_map_ps)
         return;
      com_ptr<ID3D11Resource> frame_resource;
      target->GetResource(&frame_resource);
      com_ptr<ID3D11Texture2D> frame;
      frame_resource->QueryInterface(&frame);
      if (!frame)
         return;
      D3D11_TEXTURE2D_DESC frame_desc = {};
      frame->GetDesc(&frame_desc);
      D3D11_RENDER_TARGET_VIEW_DESC rtv_desc = {};
      target->GetDesc(&rtv_desc);
      if (!EnsureFrameCopy(native_device, frame_desc, rtv_desc.Format, std::addressof(gd.display_map_source), std::addressof(gd.display_map_source_srv)))
         return;
      DrawStateStack<DrawStateStackType::FullGraphics> graphics_state;
      graphics_state.Cache(native_device_context, device_data.uav_max_count);
      native_device_context->CopyResource(gd.display_map_source.get(), frame.get());
      SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, reshade::api::shader_stage::pixel, LumaConstantBufferType::LumaSettings);
      SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, reshade::api::shader_stage::pixel, LumaConstantBufferType::LumaData, 0, gd.uber_drawn);
      DrawCustomPixelShader(native_device_context, device_data.default_depth_stencil_state.get(), device_data.default_blend_state.get(), nullptr, copy_vs, display_map_ps,
         gd.display_map_source_srv.get(), target, frame_desc.Width, frame_desc.Height);
      graphics_state.Restore(native_device_context);
      gd.display_mapped = true;
      CaptureUiBound(native_device, native_device_context, &gd, frame.get(), rtv_desc.Format);
   }

   // The previous draw (a display map or a video in its target, "ui_bound_pending") has executed by now
   static void CapturePendingUiBound(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, DeadSpaceGameDeviceData* gd)
   {
      if (!gd->ui_bound_pending)
         return;
      CaptureUiBound(native_device, native_device_context, gd, gd->ui_bound_pending.get(), gd->ui_bound_pending_format);
      gd->ui_bound_pending.reset();
   }

   // The bound target becomes "ui_bound" at the next immediate draw
   static void MarkUiBound(ID3D11DeviceContext* native_device_context, DeadSpaceGameDeviceData* gd)
   {
      com_ptr<ID3D11RenderTargetView> rtv;
      native_device_context->OMGetRenderTargets(1, &rtv, nullptr);
      if (!rtv)
         return;
      D3D11_RENDER_TARGET_VIEW_DESC rtv_desc = {};
      rtv->GetDesc(&rtv_desc);
      gd->ui_bound_pending.reset();
      rtv->GetResource(&gd->ui_bound_pending);
      gd->ui_bound_pending_format = rtv_desc.Format;
   }

   // A post pass' output sized target is the frame ("post_target"); its half and quarter resolution passes aren't
   static void TrackPostTarget(ID3D11DeviceContext* native_device_context, DeviceData& device_data)
   {
      com_ptr<ID3D11RenderTargetView> rtv;
      native_device_context->OMGetRenderTargets(1, &rtv, nullptr);
      if (rtv && GetViewTextureSize(rtv.get()).x >= uint32_t(device_data.output_resolution.x))
      {
         auto& gd = GetGameDeviceData(device_data);
         gd.post_target.reset();
         rtv->GetResource(&gd.post_target);
      }
   }

   // A video drawn without depth test draws over the scene (a full screen video or video log); the in-world screens are depth tested
   static bool IsVideoOverScene(ID3D11DeviceContext* native_device_context)
   {
      // A null state is D3D's default, depth tested
      com_ptr<ID3D11DepthStencilState> depth_state;
      native_device_context->OMGetDepthStencilState(&depth_state, nullptr);
      D3D11_DEPTH_STENCIL_DESC depth_desc = {};
      if (depth_state)
      {
         depth_state->GetDesc(&depth_desc);
      }
      return depth_state && !depth_desc.DepthEnable;
   }

   // What draws over the scene starts with "scene_over_draw" (an immediate game draw the game's "scene_over" rule picked), and the display
   // map runs first, so it keeps the UI paper white and the present blit doesn't map it with the scene. Only on the frame: the last post
   // pass' target, or without the uber post any output sized target (the UI may draw on another one).
   static void DrawDisplayMapOverScene(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, CommandListData& cmd_list_data, DeviceData& device_data, bool scene_over_draw)
   {
      auto& gd = GetGameDeviceData(device_data);
      if (!scene_over_draw || gd.display_mapped)
         return;
      com_ptr<ID3D11RenderTargetView> rtv;
      native_device_context->OMGetRenderTargets(1, &rtv, nullptr);
      com_ptr<ID3D11Resource> target;
      if (rtv)
      {
         rtv->GetResource(&target);
      }
      bool frame = target && target == gd.post_target;
      if (target && !frame && (!gd.post_target || !gd.uber_drawn))
      {
         const auto size = GetViewTextureSize(rtv.get());
         frame = size.x == uint32_t(device_data.output_resolution.x) && size.y == uint32_t(device_data.output_resolution.y);
      }
      if (frame)
      {
         DrawDisplayMap(native_device, native_device_context, cmd_list_data, device_data, rtv.get());
      }
   }

   // The VP6 video (Includes/DS_VideoVP6.hlsl). LumaData.CustomData3: drawn over the mapped frame (UI side, pre-scaled for the UI paper
   // white); else an in-world screen mapped with the scene later. "updated_cbuffers" skips Core's own bind of the settings. A video over
   // the mapped frame raises the bound: its AutoHDR goes above 1 by design.
   static void BindVideoCBuffers(ID3D11DeviceContext* native_device_context, CommandListData& cmd_list_data, DeviceData& device_data, reshade::api::shader_stage stages, bool* updated_cbuffers)
   {
      auto& gd = GetGameDeviceData(device_data);
      SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, stages, LumaConstantBufferType::LumaSettings);
      SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, stages, LumaConstantBufferType::LumaData, 0, 0, gd.display_mapped ? 1.f : 0.f);
      *updated_cbuffers = true;
      if (gd.display_mapped)
      {
         MarkUiBound(native_device_context, &gd);
      }
   }

   // Luma's SMAA in the game's AA slot (a replaced pass, so a custom pass): from the game's copy of the frame (t0, returned in
   // "frame_srv") into "tex_smaa_out", if "allowed". "srgb_source": see "RunSMAA". False if it didn't draw.
   static bool RunSMAAInGameSlot(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, DeviceData& device_data, bool allowed, bool srgb_source,
      ID3D11ShaderResourceView** frame_srv)
   {
      auto& gd = GetGameDeviceData(device_data);
      native_device_context->PSGetShaderResources(0, 1, frame_srv);
      ID3D11ShaderResourceView* const source_srv = ((allowed && *frame_srv) ? GetFrameView(native_device, &gd, *frame_srv) : nullptr);
      if (!source_srv)
         return false;
      DrawStateStack<DrawStateStackType::FullGraphics> graphics_state;
      DrawStateStack<DrawStateStackType::Compute> compute_state;
      graphics_state.Cache(native_device_context, device_data.uav_max_count);
      compute_state.Cache(native_device_context, device_data.uav_max_count);
      const bool smaa_drawn = RunSMAA(native_device, native_device_context, device_data, source_srv, srgb_source);
      compute_state.Restore(native_device_context);
      graphics_state.Restore(native_device_context);
      return smaa_drawn;
   }

   // The game's AA pass replacement drawn again from SMAA's output ("RunSMAAInGameSlot") at t0, then the game's frame back
   static void DrawFromSMAAOutput(ID3D11DeviceContext* native_device_context, DeadSpaceGameDeviceData* gd, ID3D11ShaderResourceView* frame_srv, std::function<void()>* original_draw_dispatch_func)
   {
      gd->aa_drawn = true;
#if DEVELOPMENT
      gd->aa_stats.smaa_slot_a++;
#endif
      ID3D11ShaderResourceView* const smaa_srv = gd->tex_smaa_out_srv.get();
      native_device_context->PSSetShaderResources(0, 1, &smaa_srv);
      (*original_draw_dispatch_func)();
      native_device_context->PSSetShaderResources(0, 1, &frame_srv);
   }

   // The present blit drawn again with "ui_bound" at t1 (LumaData.CustomData4), then the game's t1 back
   static void DrawPresentWithUiBound(ID3D11DeviceContext* native_device_context, DeadSpaceGameDeviceData* gd, std::function<void()>* original_draw_dispatch_func)
   {
      com_ptr<ID3D11ShaderResourceView> previous_t1;
      native_device_context->PSGetShaderResources(1, 1, &previous_t1);
      ID3D11ShaderResourceView* const ui_bound_srv = gd->ui_bound_srv.get();
      native_device_context->PSSetShaderResources(1, 1, &ui_bound_srv);
      (*original_draw_dispatch_func)();
      ID3D11ShaderResourceView* const previous_srv = previous_t1.get();
      native_device_context->PSSetShaderResources(1, 1, &previous_srv);
   }

public:
   // The subclass sets "sr_game_tooltip" (and "GammaRamp::default_slider_exponent") first
   void OnInit(bool async) override
   {
      native_shaders_definitions.emplace(CompileTimeStringHash("DS Display Map PS"),
         ShaderDefinition{.file_name = "Luma_DS_DisplayMap", .type = reshade::api::pipeline_subobject_type::pixel_shader});
      DeadSpaceGame::OnInit(async);
   }

   void OnCreateDevice(ID3D11Device* native_device, DeviceData& device_data) override
   {
      DeadSpaceGame::OnCreateDevice(native_device, device_data);
#if DEVELOPMENT
      Mcp::RegisterCounters({{ "aa.ssaa",
         &GetGameDeviceData(device_data).last_aa_stats.ssaa }},
         &device_data);
#endif
   }

   void OnDestroyDeviceData(DeviceData& device_data) override
   {
      GammaRamp::Uninstall();
      DeadSpaceGame::OnDestroyDeviceData(device_data);
   }

   void OnPresent(ID3D11Device* native_device, DeviceData& device_data) override
   {
      // Installed once the game's device exists, again after a new one
      if (IDirect3DDevice9* const d3d9_device = GetGameDevice())
      {
         GammaRamp::Install(d3d9_device);
      }
      if (const float exponent = GammaRamp::exponent; cb_luma_global_settings.GameSettings.GammaRampExponent != exponent)
      {
         cb_luma_global_settings.GameSettings.GammaRampExponent = exponent;
         device_data.cb_luma_global_settings_dirty = true;
      }
      DeadSpaceGame::OnPresent(native_device, device_data);
   }

   void PrintImGuiAbout() override
   {
      ImGui::PushTextWrapPos(0.f);
      ImGui::Text(
         "Luma for \"%s\" is developed by DristoforColumb and is open source and free.\n"
         "It adds HDR, DLAA, FSR 3 or XeSS native anti-aliasing or Luma TAA, and SMAA (with T2x), plus 16x anisotropic filtering.\n"
         "It runs through dgVoodoo2 (DirectX 9 -> 11).\n"
         "Do NOT run another HDR mod (e.g. RenoDX) alongside it.\n"
         "Thanks to the Luma team and contributors.\n"
         "If you enjoy it, consider donating.",
         about_name);
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
      DeadSpaceGame::PrintImGuiAbout();
   }
};

// Dead Space 3 (Steam): every hash is a dgVoodoo 2.87.6 ps_5_0 translation (mapped offline from the game's D3D9 shaders); the
// arrays also hold the 2.81.3 (ps_4_0) twins, the passes keyed by one hash map them through "pixel_shader_hashes_from_v281".
// HDR ("WrapUberLut" and the fp16 mirrors, as Dead Space 1/2): the light buffer is fp16 but the lit scene canvas and the post chain are RGBA8 with no tonemap. They get
// fp16 mirrors, the uber post's LUT tail keeps what's above 1 ("WrapUberLut"), and the final display pass (the game's own last draw
// into the swapchain, nothing draws after it) maps to the display (DS3_FinalDisplay).
// DLAA / FSR / XeSS / Luma TAA and SMAA: "DeadSpaceGame"'s path (motion vectors from Core's DXBC patch, the frame camera from the engine's
// globals, the upscaler at the scene's end), with the game's own SMAA or FXAA pass as SMAA's slot.
namespace DS3
{
   constexpr uint32_t FINAL_DISPLAY_PS = 0x9AF6BE17;
   // The half resolution copy of the scene canvas (input_texture, blob 197f1fa9c176): the post chain's first pass (bloom, depth of field)
   constexpr uint32_t POST_DOWNSAMPLE_PS = 0x0FC0AA7A;
   constexpr uint32_t SCENE_COPY_PS = 0x6B413C5D; // Point copy, before FXAA 1 and SMAA, also of the render to texture targets
   // The game's AA (system.txt "QualityOptions.AAMode", live): 1 "FXAA2 (Console)" is folded into the uber (0x2B156875), 2 FXAA 1, 3
   // FXAA 3.11 Quality, 4 SMAA 1x (the edges, weights and neighborhood blending). Each reads the graded frame at t0, after the uber.
   constexpr uint32_t FXAA1_PS = 0x9A8B2113;
   constexpr uint32_t FXAA3_PS = 0xDA5FD1AB;
   constexpr uint32_t SMAA_EDGES_PS = 0xC40BF6B4;
   constexpr uint32_t SMAA_WEIGHTS_PS = 0x6EB8F52A;
   constexpr uint32_t SMAA_NEIGHBORHOOD_PS = 0xB7F107B1; // t0 the frame, t1 the blend weights
   // The Y'CbCr video passes (DS3_VideoYCbCr, DS3_VideoYUVVolume, DS3_VideoPacked: their hashes are registered in "DllMain", the same
   // D3D9 video shaders exist in Dead Space 1/2)
   constexpr uint32_t VIDEO_YCBCR_PS = 0x94EDB896;
   constexpr uint32_t VIDEO_YUV_VOLUME_PS = 0xA4AEB579;
   constexpr uint32_t VIDEO_PACKED_PS = 0x8AE909CA;
   // dgVoodoo 2.81.3 (the Linux zip, ps_4_0) translations of the passes keyed by one hash above, read as their 2.87 hash (the same D3D9
   // blob and alpha test, "hashgen\ds3_ps_hashes.tsv"). The named ones are also replaced or patched by their own hash.
   constexpr uint32_t FINAL_DISPLAY_V281_PS = 0xC60B589D;
   constexpr uint32_t POST_DOWNSAMPLE_V281_PS = 0xF8E50FA4;
   constexpr uint32_t VIDEO_YCBCR_V281_PS = 0xC747B9DD;
   constexpr uint32_t VIDEO_YUV_VOLUME_V281_PS = 0x61AED491;
   constexpr uint32_t VIDEO_PACKED_V281_PS = 0xBA227CA7;
   constexpr std::pair<uint32_t, uint32_t> pixel_shader_hashes_from_v281[] = {
      {FINAL_DISPLAY_V281_PS, FINAL_DISPLAY_PS},
      {POST_DOWNSAMPLE_V281_PS, POST_DOWNSAMPLE_PS},
      {0xBEA51796, SCENE_COPY_PS},
      {0x6B98A2EB, FXAA1_PS},
      {0xF662B924, FXAA3_PS},
      {0x2C79D297, SMAA_EDGES_PS},
      {0x867B6A4F, SMAA_WEIGHTS_PS},
      {0x22BE87D4, SMAA_NEIGHBORHOOD_PS},
      {VIDEO_YCBCR_V281_PS, VIDEO_YCBCR_PS},
      {VIDEO_YUV_VOLUME_V281_PS, VIDEO_YUV_VOLUME_PS},
      {VIDEO_PACKED_V281_PS, VIDEO_PACKED_PS},
   };

   class DeadSpace3 final : public DeadSpaceGame
   {
      // What "SceneMotionVectors" (and "OpenScene") asks of the game
      struct MotionVectorGame : MotionVectorGameBase<MotionVectorGame>
      {
         static constexpr const char* log_tag = "Dead Space 3";
         static constexpr MotionVectorPatch::Layout layout = MotionVectorPatches::layout;
         static constexpr auto cameras = MotionVectorPatches::cameras;
         static constexpr auto tie_break_rows = MotionVectorPatches::tie_break_rows;
         static constexpr auto frame_camera = MotionVectorPatches::frame_camera;
         static constexpr uint32_t previous_vertices_slot = MotionVectorPatches::previous_vertices_slot;
         // The engine's custom depth range puts infinity at A ~0.99: the fill rescales the depth by 1 / A (see "FillMotionVectors")
         static constexpr bool infinite_depth = true;
         static constexpr bool ssaa_mods = false;

         // The frame camera: the engine's view projection (0x132c160 in the matrix stack's product block, row vectors in the layout the
         // vertex shaders read, translation in the 4th row). Every camera apply (shadow views,
         // render to texture) rewrites it, so it's read at the G-buffer's first draw. Found through the products' setter FUN_00b77df0
         // (0xb77e0a: push 0x132c160; mov eax, 4; mov ecx, 0x132c220 (the view)); null on another build. The Steam exe (TimeDateStamp
         // 0x511e9327) has no ASLR, and its .text is decrypted by the EA wrapper before the game creates its device. LH [0, ~0.99] depth
         // (A < 1: an infinite far), x mirrored (_11 < 0) as Dead Space 1/2's.
         static constexpr uintptr_t exe_code_address = 0xb77e0a;
         static constexpr uint8_t exe_code[] = {0x68, 0x60, 0xC1, 0x32, 0x01, 0xB8, 0x04, 0x00, 0x00, 0x00, 0xB9, 0x20, 0xC2, 0x32, 0x01};
         static constexpr uintptr_t view_projection_address = 0x132c160;
      };

      // The game's AA pass (FXAA 1, FXAA 3.11 or SMAA's neighborhood blending) once Luma antialiased the frame: Luma's SMAA of its input
      // (t0, the graded frame) in its slot, or its input as it is after the upscaler, copied into its target in place of the pass. False
      // (the game's pass draws) when nothing replaces it.
      static bool DrawGameAAReplacement(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, DeviceData& device_data)
      {
         auto& gd = GetGameDeviceData(device_data);
         if (!gd.aa_drawn && !gd.smaa_slot_a)
            return false;
         com_ptr<ID3D11ShaderResourceView> frame_srv;
         native_device_context->PSGetShaderResources(0, 1, &frame_srv);
         com_ptr<ID3D11RenderTargetView> rtv;
         native_device_context->OMGetRenderTargets(1, &rtv, nullptr);
         auto* const copy_vs = FindShader(device_data.native_vertex_shaders, CompileTimeStringHash("Copy VS"));
         auto* const copy_ps = FindShader(device_data.native_pixel_shaders, CompileTimeStringHash("Copy PS"));
         if (!frame_srv || !rtv || !copy_vs || !copy_ps)
            return false;
         ID3D11ShaderResourceView* const source_srv = GetFrameView(native_device, &gd, frame_srv.get());
         if (!source_srv)
            return false;
         DrawStateStack<DrawStateStackType::FullGraphics> graphics_state;
         DrawStateStack<DrawStateStackType::Compute> compute_state;
         graphics_state.Cache(native_device_context, device_data.uav_max_count);
         compute_state.Cache(native_device_context, device_data.uav_max_count);
         ID3D11ShaderResourceView* copy_source = source_srv;
         if (!gd.aa_drawn && RunSMAA(native_device, native_device_context, device_data, source_srv, /* srgb_source */ false))
         {
            copy_source = gd.tex_smaa_out_srv.get();
            gd.aa_drawn = true;
#if DEVELOPMENT
            gd.aa_stats.smaa_slot_a++;
#endif
         }
#if DEVELOPMENT
         else
         {
            gd.aa_stats.game_aa_copies++;
         }
#endif
         const uint2 size = GetViewTextureSize(rtv.get());
         DrawCustomPixelShader(native_device_context, device_data.default_depth_stencil_state.get(), device_data.default_blend_state.get(), nullptr, copy_vs, copy_ps, copy_source,
            rtv.get(), size.x, size.y, true);
         compute_state.Restore(native_device_context);
         graphics_state.Restore(native_device_context);
         return true;
      }

   public:
      DeadSpace3() : DeadSpaceGame({
                        .smaa = "SMAA anti-aliasing (sharper, more complete edges), in place of the game's FXAA or SMAA when it runs (not its \"FXAA2 (Console)\").\nNot used with DLSS/FSR/XeSS or Luma TAA.",
                        .t2x = "Smoother edges and less shimmering, by blending each frame with the previous one.\nCan look slightly softer in motion. Not used with DLSS/FSR/XeSS or Luma TAA.",
                        .hide_ui = "Disables the on-screen UI (subtitles, prompts, tutorials). The in-world HUD stays.",
                     })
      {
      }

      // What "DllMain" sets for this exe before Core starts: the mirrored writers, the replacements it binds, the shader define and
      // the development names
      static void ConfigureLoad()
      {
         AutoUpgradeWriters({apply_pixel_shaders});
         // The video replacements: the same D3D9 video shaders (so the same dgVoodoo hashes) exist in Dead Space 1/2, routed differently
         constexpr ShaderRedirect redirects[] = {
            {"DS3_FinalDisplay", FINAL_DISPLAY_PS, FINAL_DISPLAY_V281_PS},
            {"DS3_VideoYCbCr", VIDEO_YCBCR_PS, VIDEO_YCBCR_V281_PS},
            {"DS3_VideoYUVVolume", VIDEO_YUV_VOLUME_PS, VIDEO_YUV_VOLUME_V281_PS},
            {"DS3_VideoPacked", VIDEO_PACKED_PS, VIDEO_PACKED_V281_PS},
         };
         RedirectShaders(redirects);
         RedirectShaders(bloom_redirects);
         sub_game_shader_define = "GAME_DEAD_SPACE_3";
         sub_game_shaders_appendix = "DS3";

#if DEVELOPMENT
         forced_shader_names.emplace(Shader::Hash_StrToNum("8C2B0166"), "Uber Post (LUT)");
         forced_shader_names.emplace(Shader::Hash_StrToNum("E7A0D8E6"), "Uber Post (bloom, LUT)");
         forced_shader_names.emplace(Shader::Hash_StrToNum("F8F7525A"), "Uber Post (AA, bloom, distort, saturate, luma, vignette, LUT)");
         forced_shader_names.emplace(Shader::Hash_StrToNum("2B156875"), "Uber Post (bloom, LUT, FXAA2 Console)");
         forced_shader_names.emplace(FXAA1_PS, "FXAA 1");
         forced_shader_names.emplace(FXAA3_PS, "FXAA 3.11 Quality");
         forced_shader_names.emplace(SMAA_EDGES_PS, "SMAA Edge Detection");
         forced_shader_names.emplace(SMAA_WEIGHTS_PS, "SMAA Blending Weights");
         forced_shader_names.emplace(SMAA_NEIGHBORHOOD_PS, "SMAA Neighborhood Blending");
         forced_shader_names.emplace(Shader::Hash_StrToNum("B30009A3"), "LUT Bake (1 LUT per blue slice)");
         forced_shader_names.emplace(Shader::Hash_StrToNum("75A2E161"), "Bloom Composite (sRGB scene)");
         forced_shader_names.emplace(Shader::Hash_StrToNum("88A0FF60"), "Bloom");
         forced_shader_names.emplace(SCENE_COPY_PS, "Scene Copy");
         forced_shader_names.emplace(POST_DOWNSAMPLE_PS, "Post Downsample (half resolution canvas)");
         forced_shader_names.emplace(VIDEO_YCBCR_PS, "Video (Y/Cb/Cr, BT.601 limited)");
         forced_shader_names.emplace(VIDEO_YUV_VOLUME_PS, "Video (Y/U/V volume, BT.601 limited)");
         forced_shader_names.emplace(VIDEO_PACKED_PS, "Video (packed YUV, BT.601 limited)");
         forced_shader_names.emplace(FINAL_DISPLAY_PS, "Final Display (brightness, saturation, S-curve)");
         forced_shader_names.emplace(Shader::Hash_StrToNum("7BFAD23F"), "Mask Lerp + Vignette (ToneMapTexture)");
         forced_shader_names.emplace(Shader::Hash_StrToNum("A349BAFC"), "SSAO Apply");
         forced_shader_names.emplace(Shader::Hash_StrToNum("12443A75"), "SSAO");
         forced_shader_names.emplace(Shader::Hash_StrToNum("A14986C6"), "SSR");
         forced_shader_names.emplace(Shader::Hash_StrToNum("658E27F4"), "Camera Motion Blur");
         forced_shader_names.emplace(Shader::Hash_StrToNum("80EC18BA"), "Deferred EdgeAA");
#endif
      }

      std::unique_ptr<std::byte[]> PatchShaderBytecodeSync(const std::byte* code, size_t& size, reshade::api::pipeline_subobject_type type, uint64_t shader_hash, const std::byte* shader_object, size_t shader_object_size) override
      {
         if (type != reshade::api::pipeline_subobject_type::pixel_shader)
            return nullptr;
         // The post downsample's UNORM target clipped the canvas at 1 for the bloom bright pass (3FC5D8F0, a steep power curve): on the fp16
         // mirror a canvas at 2 came out of it at ~120 and bloomed into blotches
         if (std::ranges::binary_search(saturate_pixel_shaders, uint32_t(shader_hash)) || shader_hash == POST_DOWNSAMPLE_PS || shader_hash == POST_DOWNSAMPLE_V281_PS)
            return DXBC::ClampOutput0(code, &size, DXBC::Output0Clamp::SATURATE);
         return PatchBlendInputOrUber(code, &size, uint32_t(shader_hash), blend_input_pixel_shaders, uber_pixel_shaders);
      }

      // The final display pass (the game's own last draw into the swapchain) is the display map
      void OnInit(bool async) override
      {
         sr_game_tooltip = "DLSS, FSR and XeSS require Luma-Upscaler.exe next to the game's exe (Luma TAA doesn't).\nThe game's \"FXAA2 (Console)\" AA still runs on top of them: pick another AA mode in the game.\n";
         DeadSpaceGame::OnInit(async);
#if DEVELOPMENT
         Mcp::RegisterTextures({MCP_GAME_TEXTURE("ui.layer", ui_layer)});
#endif
      }

      DrawOrDispatchOverrideType OnDrawOrDispatch(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, CommandListData& cmd_list_data, DeviceData& device_data, reshade::api::shader_stage stages, const ShaderHashesList<OneShaderPerPipeline>& original_shader_hashes, bool is_custom_pass, bool& updated_cbuffers, std::function<void()>* original_draw_dispatch_func) override
      {
         const bool is_immediate = native_device_context->GetType() == D3D11_DEVICE_CONTEXT_IMMEDIATE;
         auto& gd = GetGameDeviceData(device_data);
         const uint32_t pixel_shader_hash = ToV287Hash(uint32_t(original_shader_hashes.pixel_shaders[0]), pixel_shader_hashes_from_v281);
         const bool can_redraw = original_draw_dispatch_func && *original_draw_dispatch_func;
         // A game draw that can be drawn again with patched shaders (not Luma's, nor one whose shader Luma replaced: "is_custom_pass")
         const bool game_draw = !is_custom_pass && can_redraw;
         const bool uber = std::ranges::binary_search(uber_pixel_shaders, pixel_shader_hash);
         const bool video = pixel_shader_hash == VIDEO_YCBCR_PS || pixel_shader_hash == VIDEO_YUV_VOLUME_PS || pixel_shader_hash == VIDEO_PACKED_PS;
         const bool game_aa = pixel_shader_hash == FXAA1_PS || pixel_shader_hash == FXAA3_PS || pixel_shader_hash == SMAA_NEIGHBORHOOD_PS;
         // The bound t0's resource (the pass' source)
         const auto source_resource = [&]()
         {
            com_ptr<ID3D11ShaderResourceView> source_srv;
            native_device_context->PSGetShaderResources(0, 1, &source_srv);
            com_ptr<ID3D11Resource> source;
            if (source_srv)
            {
               source_srv->GetResource(&source);
            }
            return source;
         };

         if (uber)
         {
            // The patched LUT tail reads "Color Grading Intensity" there ("WrapUberLut")
            SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, reshade::api::shader_stage::pixel, LumaConstantBufferType::LumaSettings);
            if (is_immediate)
            {
               gd.uber_drawn = true;
               device_data.has_drawn_main_post_processing = true;
            }
         }
         else if (pixel_shader_hash == FINAL_DISPLAY_PS)
         {
            if (!is_immediate)
               return DrawOrDispatchOverrideType::None;
            gd.final_source = source_resource();
            // LumaData.CustomData2: the uber's hue turn is already in the frame; CustomData3: the UI layer (t1) drew this frame.
            // "updated_cbuffers" skips Core's own bind of the settings.
            SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, stages, LumaConstantBufferType::LumaSettings);
            SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, stages, LumaConstantBufferType::LumaData, 0, gd.uber_drawn, gd.ui_layer_drawn);
            updated_cbuffers = true;
            if (gd.ui_layer_drawn && can_redraw)
            {
               ID3D11ShaderResourceView* const ui_layer_srv = gd.ui_layer_srv.get();
               native_device_context->PSSetShaderResources(1, 1, &ui_layer_srv);
               (*original_draw_dispatch_func)();
               ID3D11ShaderResourceView* const no_srv = nullptr;
               native_device_context->PSSetShaderResources(1, 1, &no_srv);
               return DrawOrDispatchOverrideType::Replaced;
            }
         }
         else if (pixel_shader_hash == POST_DOWNSAMPLE_PS)
         {
            // It also copies render to texture targets: only the canvas starts the post chain
            if (is_immediate && gd.final_source && source_resource() == gd.final_source)
            {
               gd.post_started = true;
               device_data.has_drawn_main_post_processing = true;
            }
         }

         // The UI: drawn on the canvas (the frame the final display pass reads) after the post chain started, and the videos there
         com_ptr<ID3D11RenderTargetView> ui_rtv;
         com_ptr<ID3D11DepthStencilView> ui_dsv;
         bool ui_draw = false;
         if (is_immediate && gd.post_started && !is_custom_pass &&
             (video || std::ranges::binary_search(saturate_pixel_shaders, pixel_shader_hash) || std::ranges::binary_search(blend_input_pixel_shaders, pixel_shader_hash)))
         {
            native_device_context->OMGetRenderTargets(1, &ui_rtv, &ui_dsv);
            com_ptr<ID3D11Resource> target;
            if (ui_rtv)
            {
               ui_rtv->GetResource(&target);
            }
            ui_draw = target && target == gd.final_source;
         }
         // "Hide Gameplay UI": that UI (subtitles, prompts, the tutorial holograms), not the videos
         if (hide_ui && ui_draw && !video)
            return DrawOrDispatchOverrideType::Skip;
         const bool ui_layer_draw = ui_draw && cb_luma_global_settings.DisplayMode == DisplayModeType::HDR && can_redraw;
         if (is_immediate && video)
         {
            // LumaData.CustomData3: drawn into the UI layer (UI side, pre-scaled for the UI paper white); else an in-world screen mapped with
            // the scene. "updated_cbuffers" skips Core's own bind of the settings.
            SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, stages, LumaConstantBufferType::LumaSettings);
            SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, stages, LumaConstantBufferType::LumaData, 0, 0, ui_layer_draw ? 1.f : 0.f);
            updated_cbuffers = true;
         }
         if (ui_layer_draw)
         {
            com_ptr<ID3D11Resource> target;
            ui_rtv->GetResource(&target);
            D3D11_TEXTURE2D_DESC target_desc = {};
            static_cast<ID3D11Texture2D*>(target.get())->GetDesc(&target_desc);
            D3D11_TEXTURE2D_DESC ui_desc = {};
            if (gd.ui_layer)
            {
               gd.ui_layer->GetDesc(&ui_desc);
            }
            if (!gd.ui_layer || ui_desc.Width != target_desc.Width || ui_desc.Height != target_desc.Height)
            {
               gd.ui_layer_srv.reset();
               gd.ui_layer_rtv.reset();
               gd.ui_layer.reset();
               const D3D11_TEXTURE2D_DESC desc = {
                  .Width = target_desc.Width,
                  .Height = target_desc.Height,
                  .MipLevels = 1,
                  .ArraySize = 1,
                  .Format = DXGI_FORMAT_R16G16B16A16_FLOAT,
                  .SampleDesc = {.Count = 1},
                  .Usage = D3D11_USAGE_DEFAULT,
                  .BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE,
               };
               if (FAILED(native_device->CreateTexture2D(&desc, nullptr, &gd.ui_layer)) || FAILED(native_device->CreateRenderTargetView(gd.ui_layer.get(), nullptr, &gd.ui_layer_rtv)) ||
                   FAILED(native_device->CreateShaderResourceView(gd.ui_layer.get(), nullptr, &gd.ui_layer_srv)))
               {
                  gd.ui_layer.reset();
                  return DrawOrDispatchOverrideType::None;
               }
            }
            ID3D11RenderTargetView* const ui_layer_rtv = gd.ui_layer_rtv.get();
            if (!gd.ui_layer_drawn)
            {
               const FLOAT transparent[4] = {0.f, 0.f, 0.f, 0.f};
               native_device_context->ClearRenderTargetView(ui_layer_rtv, transparent);
               gd.ui_layer_drawn = true;
            }

            com_ptr<ID3D11BlendState> blend_state;
            FLOAT blend_factor[4] = {};
            UINT sample_mask = 0;
            native_device_context->OMGetBlendState(&blend_state, blend_factor, &sample_mask);
            D3D11_BLEND_DESC blend_desc = CD3D11_BLEND_DESC(D3D11_DEFAULT);
            if (blend_state)
            {
               blend_state->GetDesc(&blend_desc);
            }
            // "ui_layer_blend": the alpha is the coverage, how much of the scene the draw's colour blend kept: over ("..., INV_SRC_ALPHA")
            // it scales by 1 - a, additive ("..., ONE") it keeps, an opaque draw (a full screen video, whose Y/Cb/Cr pass writes alpha 0)
            // replaces it (the blend factor's alpha 1). Other blends keep the game's alpha blend.
            FLOAT ui_blend_factor[4] = {blend_factor[0], blend_factor[1], blend_factor[2], blend_factor[3]};
            auto& rt_blend = blend_desc.RenderTarget[0];
            if (rt_blend.BlendEnable && rt_blend.BlendOp == D3D11_BLEND_OP_ADD && rt_blend.DestBlend == D3D11_BLEND_INV_SRC_ALPHA)
            {
               rt_blend.SrcBlendAlpha = D3D11_BLEND_ONE;
               rt_blend.DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
               rt_blend.BlendOpAlpha = D3D11_BLEND_OP_ADD;
            }
            else if (rt_blend.BlendEnable && rt_blend.BlendOp == D3D11_BLEND_OP_ADD && rt_blend.DestBlend == D3D11_BLEND_ONE)
            {
               rt_blend.SrcBlendAlpha = D3D11_BLEND_ZERO;
               rt_blend.DestBlendAlpha = D3D11_BLEND_ONE;
               rt_blend.BlendOpAlpha = D3D11_BLEND_OP_ADD;
            }
            else if (!rt_blend.BlendEnable)
            {
               rt_blend.BlendEnable = TRUE;
               rt_blend.SrcBlend = D3D11_BLEND_ONE;
               rt_blend.DestBlend = D3D11_BLEND_ZERO;
               rt_blend.BlendOp = D3D11_BLEND_OP_ADD;
               rt_blend.SrcBlendAlpha = D3D11_BLEND_BLEND_FACTOR;
               rt_blend.DestBlendAlpha = D3D11_BLEND_ZERO;
               rt_blend.BlendOpAlpha = D3D11_BLEND_OP_ADD;
               ui_blend_factor[3] = 1.f;
            }
            com_ptr<ID3D11BlendState> ui_blend_state; // D3D returns the same object for the same desc
            native_device->CreateBlendState(&blend_desc, &ui_blend_state);

            native_device_context->OMSetRenderTargets(1, &ui_layer_rtv, ui_dsv.get());
            native_device_context->OMSetBlendState(ui_blend_state.get(), ui_blend_factor, sample_mask);
            (*original_draw_dispatch_func)();
            ID3D11RenderTargetView* const game_rtv = ui_rtv.get();
            native_device_context->OMSetRenderTargets(1, &game_rtv, ui_dsv.get());
            native_device_context->OMSetBlendState(blend_state.get(), blend_factor, sample_mask);
            return DrawOrDispatchOverrideType::Replaced;
         }

         const bool apply = std::ranges::binary_search(apply_pixel_shaders, pixel_shader_hash);
         TrackSceneDepthWithoutScene<MotionVectorGame>(native_device, native_device_context, device_data, is_immediate && game_draw && apply);
         // The first pass after the scene: the post chain's downsample or the scene copy reading the canvas (both also copy the render to
         // texture screens), else the uber, the game's AA or the final display (live: the downsample in gameplay and the menu)
         bool scene_end = false;
         if (gd.mv.active && is_immediate && !gd.mv.scene_done && gd.mv.scene_open)
         {
            if (pixel_shader_hash == POST_DOWNSAMPLE_PS || pixel_shader_hash == SCENE_COPY_PS)
            {
               scene_end = gd.scene_target && source_resource() == gd.scene_target;
            }
            else
            {
               scene_end = uber || game_aa || pixel_shader_hash == SMAA_EDGES_PS || pixel_shader_hash == FINAL_DISPLAY_PS;
            }
         }
         if (DrawSceneMotionVectors<MotionVectorGame>(native_device, native_device_context, cmd_list_data, device_data, stages, original_shader_hashes, original_draw_dispatch_func, is_immediate, game_draw,
                scene_end, apply, pixel_shader_hash))
            return DrawOrDispatchOverrideType::Replaced;

         // The game's AA (FXAA 1, FXAA 3.11, SMAA): Luma's SMAA in its slot, or a copy once the upscaler antialiased the frame. SMAA's edges
         // and weights then have no reader.
         if (is_immediate && (pixel_shader_hash == SMAA_EDGES_PS || pixel_shader_hash == SMAA_WEIGHTS_PS) && (gd.aa_drawn || gd.smaa_slot_a))
            return DrawOrDispatchOverrideType::Skip;
         if (is_immediate && game_aa)
         {
            gd.game_aa_drawn = true;
            if (DrawGameAAReplacement(native_device, native_device_context, device_data))
               return DrawOrDispatchOverrideType::Replaced;
         }
         return DrawOrDispatchOverrideType::None;
      }
   };
} // namespace DS3

// Dead Space (2008, Steam) is x86 EA Redwood Shores engine: every hash is a dgVoodoo 2.87.6 ps_5_0 translation (mapped offline from the
// game's D3D9 shaders). The Linux zip's 2.81.3 emits ps_4_0: its twins are keyed too (the arrays
// hold both, see "pixel_shader_hashes_from_v281").
// HDR ("LumaDisplayMapGame", as Dead Space 2): the game has no tonemap and every target is RGBA8. They get fp16 mirrors, the uber post's LUT tail keeps
// what's above 1 ("WrapUberLut"), and the display map runs where the scene ends, before the UI.
namespace DS1
{
   constexpr uint32_t VIDEO_PS = 0xD8BAFBCF; // VP6 videos (DS1_Video)
   // dgVoodoo's present blit (DS1_Present), and its variant with the colour adjustment and gamma ramp (DS1_PresentRamp) it takes while
   // the game's ramp isn't identity (before "GammaRamp::Install"). The same bytecode in 2.87.6 and 2.81.3; their hashes are registered in
   // "DllMain".
   constexpr uint32_t PRESENT_BLIT_PS = 0x880A17D3;
   constexpr uint32_t PRESENT_RAMP_BLIT_PS = 0x97C61073;
   constexpr uint32_t SCENE_COPY_PS = 0x6B413C5D; // The uber post's input
   // DeadSpaceFixes 1.8.0's replacements (UseFXAA: its FXAA in the standalone EdgeAA pass, one per EdgeAA table; SSAAScale above 1: the
   // downsample into the back buffer, one per tap count). Luma replaces them (DS1_FXAA*, DS1_SSAADownsample*) and puts its SMAA in the
   // FXAA slot.
   constexpr uint32_t DSFIXES_FXAA_FAST_PS = 0x86AF7742;     // In place of EdgeAAFast (DS1_FXAAFast)
   constexpr uint32_t DSFIXES_FXAA_DEFERRED_PS = 0x24FE53E9; // In place of DeferredEdgeAA (DS1_FXAADeferred)
   constexpr uint32_t DSFIXES_SSAA_DOWNSAMPLE_2_PS = 0xB10E5C9B;
   constexpr uint32_t DSFIXES_SSAA_DOWNSAMPLE_3_PS = 0x06CF49DC;
   constexpr uint32_t DSFIXES_SSAA_DOWNSAMPLE_4_PS = 0x5406A8BC;
   constexpr uint32_t UBER_EDGEAA_BLOOM_LUT_PS = 0xCC629DA4; // DS1_Uber
   // dgVoodoo 2.81.3 (the Linux zip, ps_4_0) translations of the passes keyed by one hash above, read as their 2.87 hash (the same D3D9
   // blob and alpha test). The arrays hold both builds' hashes; the present blit is the same bytecode in both. The named ones are also
   // replaced by their own hash.
   constexpr uint32_t VIDEO_V281_PS = 0xE6893646;
   constexpr uint32_t DSFIXES_FXAA_FAST_V281_PS = 0x7CA17B52;
   constexpr uint32_t DSFIXES_FXAA_DEFERRED_V281_PS = 0xCBDD5D29;
   constexpr uint32_t DSFIXES_SSAA_DOWNSAMPLE_2_V281_PS = 0x7A89E764;
   constexpr uint32_t DSFIXES_SSAA_DOWNSAMPLE_3_V281_PS = 0xAD2A8F49;
   constexpr uint32_t DSFIXES_SSAA_DOWNSAMPLE_4_V281_PS = 0x679CFF4E;
   constexpr std::pair<uint32_t, uint32_t> pixel_shader_hashes_from_v281[] = {
      {VIDEO_V281_PS, VIDEO_PS},
      {0xBEA51796, SCENE_COPY_PS},
      {DSFIXES_FXAA_FAST_V281_PS, DSFIXES_FXAA_FAST_PS},
      {DSFIXES_FXAA_DEFERRED_V281_PS, DSFIXES_FXAA_DEFERRED_PS},
      {DSFIXES_SSAA_DOWNSAMPLE_2_V281_PS, DSFIXES_SSAA_DOWNSAMPLE_2_PS},
      {DSFIXES_SSAA_DOWNSAMPLE_3_V281_PS, DSFIXES_SSAA_DOWNSAMPLE_3_PS},
      {DSFIXES_SSAA_DOWNSAMPLE_4_V281_PS, DSFIXES_SSAA_DOWNSAMPLE_4_PS},
   };

   class DeadSpace1 final : public LumaDisplayMapGame
   {
      // What "SceneMotionVectors" (and "OpenScene") asks of the game
      struct MotionVectorGame : MotionVectorGameBase<MotionVectorGame>
      {
         static constexpr const char* log_tag = "Dead Space";
         static constexpr MotionVectorPatch::Layout layout = MotionVectorPatches::layout;
         static constexpr auto cameras = MotionVectorPatches::cameras;
         static constexpr auto tie_break_rows = MotionVectorPatches::tie_break_rows;
         static constexpr auto frame_camera = MotionVectorPatches::frame_camera;
         static constexpr uint32_t previous_vertices_slot = MotionVectorPatches::previous_vertices_slot;
         // The upscalers' depth is the device depth as it is (FSR-Best-Practices FCAM-2): the projection is the standard finite one
         // (A = f / (f - n)), and "DrawUpscaler" passes that near and far
         static constexpr bool infinite_depth = false;
         // DeadSpaceFixes' SSAA (SSAAScale above 1) renders the scene larger than the output
         static constexpr bool ssaa_mods = true;
         static constexpr const char* ssaa_log = "[Dead Space] DeadSpaceFixes' SSAA is on (SSAAScale above 1): DLAA, FSR, XeSS, Luma TAA and SMAA T2x stay off";

         // The frame camera: the engine's view projection global (0xf35720 = view 0xf35620 x proj 0xf35660), the current transform state
         // the renderer uploads from. Every camera apply (shadow views, 2D) rewrites it, so it's read at the G-buffer's first draw, after
         // the shadow maps (the opaque draws read only their own ModelViewProj). Found through the transform upload (0x7cd214: mov edx,
         // 0xf35720; mov ecx, 0xf35620; mov esi, 0x0f35660; then the matrix multiply into edx); null on another build. Row vectors, LH
         // [0, 1] depth, A = f / (f - n), x mirrored (_11 < 0) as Dead Space 2's and 3's.
         static constexpr uintptr_t exe_code_address = 0x7cd214;
         static constexpr uint8_t exe_code[] = {0xBA, 0x20, 0x57, 0xF3, 0x00, 0xB9, 0x20, 0x56, 0xF3, 0x00, 0xBE, 0x60, 0x56, 0xF3, 0x00};
         static constexpr uintptr_t view_projection_address = 0xf35720;
      };

      // The game's IDirect3DDevice9* (0xf445b0), read where the gamma setter loads it (0x7c122b: mov eax, [0xf445b0]; mov ecx, [eax];
      // mov edx, [ecx + 0x54] (SetGammaRamp); push 0xef7b90 (the ramp))
      IDirect3DDevice9* GetGameDevice() const override
      {
         static const bool known_build = []()
         {
            constexpr uint8_t gamma_code[] = {0xA1, 0xB0, 0x45, 0xF4, 0x00, 0x8B, 0x08, 0x8B, 0x51, 0x54, 0x68, 0x90, 0x7B, 0xEF, 0x00};
            const bool known = MatchesExeCode(0x7c122b, gamma_code);
            if (!known)
            {
               reshade::log::message(reshade::log::level::warning, "[Dead Space] Unknown exe build (not the Steam one): the brightness slider isn't read");
            }
            return known;
         }();
         return (known_build ? *reinterpret_cast<IDirect3DDevice9* const*>(0xf445b0) : nullptr);
      }

   public:
      DeadSpace1() : LumaDisplayMapGame({
                                           .smaa = "SMAA anti-aliasing (sharper, more complete edges), in place of DeadSpaceFixes' FXAA when it runs.\nNot used with DLSS/FSR/XeSS or Luma TAA.",
                                           .t2x = "Smoother edges and less shimmering, by blending each frame with the previous one.\nCan look slightly softer in motion. Not used with DLSS/FSR/XeSS, Luma TAA or DeadSpaceFixes' SSAA.",
                                           .hide_ui = "Disables the in-game UI.",
                                        },
                        "Dead Space")
      {
      }

      // What "DllMain" sets for this exe before Core starts: the mirrored writers, the replacements it binds, the shader define and
      // the development names
      static void ConfigureLoad()
      {
         // The brightness slider is a D3D9 SetGammaRamp, which dgVoodoo forwards to the OS device ramp and distorts HDR output
         allow_disabling_gamma_ramp = true;
         // The light buffer and the canvas are mirrored at their first light or material apply draw
         AutoUpgradeWriters({light_pixel_shaders, apply_pixel_shaders});
         // dgVoodoo's present blits: the same bytecode in every dgVoodoo game, and in 2.87.6 and 2.81.3
         constexpr ShaderRedirect redirects[] = {
            {"DS1_FXAAFast", DSFIXES_FXAA_FAST_PS, DSFIXES_FXAA_FAST_V281_PS},
            {"DS1_FXAADeferred", DSFIXES_FXAA_DEFERRED_PS, DSFIXES_FXAA_DEFERRED_V281_PS},
            {"DS1_SSAADownsample2", DSFIXES_SSAA_DOWNSAMPLE_2_PS, DSFIXES_SSAA_DOWNSAMPLE_2_V281_PS},
            {"DS1_SSAADownsample3", DSFIXES_SSAA_DOWNSAMPLE_3_PS, DSFIXES_SSAA_DOWNSAMPLE_3_V281_PS},
            {"DS1_SSAADownsample4", DSFIXES_SSAA_DOWNSAMPLE_4_PS, DSFIXES_SSAA_DOWNSAMPLE_4_V281_PS},
            {"DS1_Uber", UBER_EDGEAA_BLOOM_LUT_PS},
            {"DS1_Video", VIDEO_PS, VIDEO_V281_PS},
            {"DS1_Present", PRESENT_BLIT_PS},
            {"DS1_PresentRamp", PRESENT_RAMP_BLIT_PS},
         };
         RedirectShaders(redirects);
         sub_game_shader_define = "GAME_DEAD_SPACE_1";
         sub_game_shaders_appendix = "DS1";

#if DEVELOPMENT
         forced_shader_names.emplace(UBER_EDGEAA_BLOOM_LUT_PS, "Uber Post (EdgeAA, bloom, 1 LUT)");
         forced_shader_names.emplace(Shader::Hash_StrToNum("77AD3317"), "Uber Post (bloom, 1 LUT)");
         forced_shader_names.emplace(SCENE_COPY_PS, "Scene Copy (uber input)");
         forced_shader_names.emplace(VIDEO_PS, "Video (VP6, BT.601 limited)");
         forced_shader_names.emplace(Shader::Hash_StrToNum("A5B366DB"), "HUD");
         // DeadSpaceFixes 1.8.0 (xinput1_3.dll): FXAA in the standalone EdgeAA pass, SSAA's downsample into the back buffer
         forced_shader_names.emplace(DSFIXES_FXAA_FAST_PS, "DeadSpaceFixes FXAA (EdgeAAFast)");
         forced_shader_names.emplace(DSFIXES_FXAA_DEFERRED_PS, "DeadSpaceFixes FXAA (DeferredEdgeAA)");
         forced_shader_names.emplace(DSFIXES_SSAA_DOWNSAMPLE_2_PS, "DeadSpaceFixes SSAA Downsample (2x)");
         forced_shader_names.emplace(DSFIXES_SSAA_DOWNSAMPLE_3_PS, "DeadSpaceFixes SSAA Downsample (3x)");
         forced_shader_names.emplace(DSFIXES_SSAA_DOWNSAMPLE_4_PS, "DeadSpaceFixes SSAA Downsample (4x)");
         // Scene end for the upscalers in gameplay (the first post pass reading the canvas), and the game's actor velocity pass
         forced_shader_names.emplace(Shader::Hash_StrToNum("962908C3"), "Bloom Downsample");
#endif
      }

      std::unique_ptr<std::byte[]> PatchShaderBytecodeSync(const std::byte* code, size_t& size, reshade::api::pipeline_subobject_type type, uint64_t shader_hash, const std::byte* shader_object, size_t shader_object_size) override
      {
         if (type != reshade::api::pipeline_subobject_type::pixel_shader)
            return nullptr;
         if (std::ranges::binary_search(saturate_pixel_shaders, uint32_t(shader_hash)))
            return DXBC::ClampOutput0(code, &size, DXBC::Output0Clamp::SATURATE);
         return PatchBlendInputOrUber(code, &size, uint32_t(shader_hash), blend_input_pixel_shaders, uber_pixel_shaders);
      }

      // The vanilla gamma ramp (the brightness slider) reaches the display map as "GammaRampExponent" (see "GammaRamp")
      void OnInit(bool async) override
      {
         GammaRamp::default_slider_exponent = 0.74f;
         sr_game_tooltip = "DLSS, FSR and XeSS require Luma-Upscaler.exe next to the game's exe (Luma TAA doesn't).\nNone of them runs with DeadSpaceFixes' SSAA (SSAAScale above 1).\n";
         LumaDisplayMapGame::OnInit(async);
      }

      DrawOrDispatchOverrideType OnDrawOrDispatch(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, CommandListData& cmd_list_data, DeviceData& device_data, reshade::api::shader_stage stages, const ShaderHashesList<OneShaderPerPipeline>& original_shader_hashes, bool is_custom_pass, bool& updated_cbuffers, std::function<void()>* original_draw_dispatch_func) override
      {
         const bool is_immediate = native_device_context->GetType() == D3D11_DEVICE_CONTEXT_IMMEDIATE;
         auto& gd = GetGameDeviceData(device_data);
         const uint32_t pixel_shader_hash = ToV287Hash(uint32_t(original_shader_hashes.pixel_shaders[0]), pixel_shader_hashes_from_v281);
         if (is_immediate)
         {
            CapturePendingUiBound(native_device, native_device_context, &gd);
         }
         const bool can_redraw = original_draw_dispatch_func && *original_draw_dispatch_func;
         // A game draw that can be drawn again with patched shaders (not Luma's, nor one whose shader Luma replaced: "is_custom_pass")
         const bool game_draw = !is_custom_pass && can_redraw;
         const bool uber = std::ranges::binary_search(uber_pixel_shaders, pixel_shader_hash);
         const bool apply = std::ranges::binary_search(apply_pixel_shaders, pixel_shader_hash);
         if (uber)
         {
            // The patched LUT tail reads "Color Grading Intensity" there ("WrapUberLut")
            SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, reshade::api::shader_stage::pixel, LumaConstantBufferType::LumaSettings);
            if (is_immediate)
            {
               gd.uber_drawn = true;
            }
         }
         // Areas without the uber's LUT end with its no-LUT permutations, depth of field or motion blur; their half and quarter
         // resolution passes aren't the frame. DeadSpaceFixes' FXAA (the EdgeAA pass) and SSAA downsample (into the back buffer) are post
         // passes too. The first of them (live: the bloom downsample 0x962908C3, the first pass reading the canvas; else the uber, depth of
         // field, the EdgeAA pass or DeadSpaceFixes' FXAA) also ends the scene for the upscalers ("DrawSceneMotionVectors"): the fill and
         // the upscaler run before it. The scene copy 0x6B413C5D doesn't: it's a draw that also runs mid-forward and for the menus' render
         // to texture.
         const bool post = uber || std::ranges::binary_search(post_pixel_shaders, pixel_shader_hash);
         if (is_immediate && post)
         {
            // The scene rendered this frame
            device_data.has_drawn_main_post_processing = true;
            TrackPostTarget(native_device_context, device_data);
         }
         // "scene_over": after an output sized post pass (the uber, or the pause menu's no-LUT lutcolor.fx), the first game draw that isn't
         // one (the HUD, the pause menu's holographic panels, videos). Without one: the first 2D UI draw while no 3D scene drew (some UI
         // shaders also draw in its forward pass), or a video drawn without depth test. Frames with neither (the main menu) are mapped by
         // the present blit.
         if (is_immediate && apply)
         {
            gd.scene_applied = true;
         }
         bool scene_over = std::ranges::binary_search(ui_pixel_shaders, pixel_shader_hash) && !gd.scene_applied;
         if (gd.post_target)
         {
            // Not the scene copy between the uber and DeadSpaceFixes' FXAA: the map draws before it, and the copy overwrites its target
            scene_over = !post && pixel_shader_hash != SCENE_COPY_PS;
         }
         else if (is_immediate && pixel_shader_hash == VIDEO_PS)
         {
            scene_over = IsVideoOverScene(native_device_context);
         }
         DrawDisplayMapOverScene(native_device, native_device_context, cmd_list_data, device_data, is_immediate && scene_over && !is_custom_pass);
         // "Hide Gameplay UI": every draw over the mapped frame (HUD, the pause menu's holographic panels (particles.fx), text and cursor),
         // not only the UI shader list: the panels share their shaders with the scene's particles. Videos stay.
         if (hide_ui && is_immediate && gd.display_mapped && !is_custom_pass && pixel_shader_hash != VIDEO_PS && pixel_shader_hash != PRESENT_BLIT_PS && pixel_shader_hash != PRESENT_RAMP_BLIT_PS)
            return DrawOrDispatchOverrideType::Skip;
         if (is_immediate && pixel_shader_hash == VIDEO_PS)
         {
            BindVideoCBuffers(native_device_context, cmd_list_data, device_data, stages, &updated_cbuffers);
         }

         TrackSceneDepthWithoutScene<MotionVectorGame>(native_device, native_device_context, device_data, is_immediate && game_draw && apply);
         if (DrawSceneMotionVectors<MotionVectorGame>(native_device, native_device_context, cmd_list_data, device_data, stages, original_shader_hashes, original_draw_dispatch_func, is_immediate, game_draw,
                !gd.mv.scene_done && post, apply, pixel_shader_hash))
            return DrawOrDispatchOverrideType::Replaced;
         // DeadSpaceFixes' FXAA (replaced, DS1_FXAA*): Luma's SMAA in its slot, from the game's copy of the graded frame (t0) into our
         // output, which the replacement then copies instead. The display map runs later, after the last post pass ("scene_over").
         if ((pixel_shader_hash == DSFIXES_FXAA_FAST_PS || pixel_shader_hash == DSFIXES_FXAA_DEFERRED_PS) && is_immediate)
         {
            gd.game_aa_drawn = true;
            com_ptr<ID3D11ShaderResourceView> frame_srv;
            // Not "game_draw": this pass is replaced, so it counts as a custom pass
            const bool smaa_drawn = RunSMAAInGameSlot(native_device, native_device_context, device_data, gd.smaa_slot_a && !gd.aa_drawn && can_redraw, /* srgb_source */ false, &frame_srv);
            // LumaData.CustomData1: the frame is already antialiased (an upscaler, Luma's SMAA at the scene's end or here), the replacement
            // only copies it. "updated_cbuffers" skips Core's own bind of the settings.
            SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, stages, LumaConstantBufferType::LumaSettings);
            SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, stages, LumaConstantBufferType::LumaData, gd.aa_drawn || smaa_drawn);
            updated_cbuffers = true;
#if DEVELOPMENT
            gd.aa_stats.game_aa_copies += gd.aa_drawn;
#endif
            if (!smaa_drawn)
               return DrawOrDispatchOverrideType::None;
            DrawFromSMAAOutput(native_device_context, &gd, frame_srv.get(), original_draw_dispatch_func);
            return DrawOrDispatchOverrideType::Replaced;
         }
         if (pixel_shader_hash == PRESENT_BLIT_PS || pixel_shader_hash == PRESENT_RAMP_BLIT_PS)
         {
            // The present blit bounds what drew after the display map by "ui_bound" (t1, LumaData.CustomData4), drawn here to restore t1.
            // "updated_cbuffers" skips Core's own bind of the settings, which the display map reads.
            const bool present_ui_bound = is_immediate && gd.ui_bound_valid && can_redraw;
            SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, stages, LumaConstantBufferType::LumaSettings);
            SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, stages, LumaConstantBufferType::LumaData, gd.display_mapped, gd.uber_drawn, 1.f, present_ui_bound ? 1.f : 0.f);
            updated_cbuffers = true;
            gd.display_mapped = false;
            if (present_ui_bound)
            {
               DrawPresentWithUiBound(native_device_context, &gd, original_draw_dispatch_func);
               return DrawOrDispatchOverrideType::Replaced;
            }
         }
         return DrawOrDispatchOverrideType::None;
      }
   };
} // namespace DS1

// Dead Space 2 (Steam) is x86 Visceral engine: every hash is a dgVoodoo 2.87.6 ps_5_0 translation of the game's or MarkerPatch's D3D9
// shaders (mapped offline); the arrays also hold the 2.81.3 (ps_4_0) twins, the passes keyed by one hash map them
// through "pixel_shader_hashes_from_v281".
// HDR: the game has no tonemap and every target is RGBA8. They get fp16 mirrors, the uber post's LUT tail keeps what's above 1
// ("WrapUberLut"), and the display map runs where the scene ends, before the UI. MarkerPatch's SMAA or FXAA pass is SMAA's slot.
namespace DS2
{
   // The frame passes Luma replaces (or routes) by hash, see "OnDrawOrDispatch"
   constexpr uint32_t VIDEO_PS = 0xD49CB77E;             // VP6 video logs and videos (DS2_VideoVP6, its hashes registered in "DllMain")
   constexpr uint32_t MARKERPATCH_FXAA_PS = 0x0A91885E;  // MarkerPatch's FXAA (DS2_FXAA)
   constexpr uint32_t SMAA_NEIGHBORHOOD_PS = 0xBE49E10A; // MarkerPatch's SMAA neighborhood blending (DS2_SMAANeighborhood)
   constexpr uint32_t PRESENT_BLIT_PS = 0x880A17D3;      // dgVoodoo's present blit (DS2_Present, as DS1_Present)
   constexpr uint32_t SSAA_DOWNSAMPLE_PS = 0x113CFB69;   // MarkerPatch's SSAA downsample (DS2_SSAADownsample)
   constexpr uint32_t MARKERPATCH_SMAA_EDGES_PS = 0x8736599C;
   constexpr uint32_t MARKERPATCH_SMAA_WEIGHTS_PS = 0x764C97BB;
   constexpr uint32_t SCENE_COPY_PS = 0x934360AD; // The uber post's input
   constexpr uint32_t CAMERA_MOTION_BLUR_DOWNSAMPLE_PS = 0xDE99AE79;
   constexpr uint32_t UBER_LUT_ONLY_PS = 0x583C9562;
   constexpr uint32_t UBER_DOF_ONLY_PS = 0xC76B708E;
   // The passes after the scene (live traces), the first of which ends it (the fill and the upscaler run before it): the scene
   // copy for the uber post, the camera motion blur's downsample, the uber post permutations seen live, MarkerPatch's SMAA and FXAA and
   // the video. Without any of them the scene ends at the game's copy of it ("OverrideCopyResource").
   constexpr uint32_t scene_end_pixel_shaders[] = {
      SCENE_COPY_PS, CAMERA_MOTION_BLUR_DOWNSAMPLE_PS, UBER_LUT_ONLY_PS, UBER_DOF_ONLY_PS, MARKERPATCH_SMAA_EDGES_PS, MARKERPATCH_FXAA_PS, VIDEO_PS};
   // MarkerPatch's SMAA edges and weights: skipped once an upscaler antialiased the frame or Luma's SMAA takes its slot (its neighborhood
   // blending still draws, for the display map, see "display_mapped")
   constexpr uint32_t markerpatch_smaa_pixel_shaders[] = {MARKERPATCH_SMAA_EDGES_PS, MARKERPATCH_SMAA_WEIGHTS_PS};
   // dgVoodoo 2.81.3 (the Linux zip, ps_4_0) translations of the passes keyed by one hash above, read as their 2.87 hash (the same D3D9
   // blob and alpha test). The arrays hold both builds' hashes; the present blit is the same bytecode in both. The named ones are also
   // replaced by their own hash.
   constexpr uint32_t VIDEO_V281_PS = 0x60E6C751;
   constexpr uint32_t MARKERPATCH_FXAA_V281_PS = 0xE492B3F0;
   constexpr uint32_t SMAA_NEIGHBORHOOD_V281_PS = 0x93099EB5;
   constexpr uint32_t SSAA_DOWNSAMPLE_V281_PS = 0xE05704FB;
   constexpr std::pair<uint32_t, uint32_t> pixel_shader_hashes_from_v281[] = {
      {VIDEO_V281_PS, VIDEO_PS},
      {MARKERPATCH_FXAA_V281_PS, MARKERPATCH_FXAA_PS},
      {SMAA_NEIGHBORHOOD_V281_PS, SMAA_NEIGHBORHOOD_PS},
      {SSAA_DOWNSAMPLE_V281_PS, SSAA_DOWNSAMPLE_PS},
      {0x89485C8E, MARKERPATCH_SMAA_EDGES_PS},
      {0xB3BD0218, MARKERPATCH_SMAA_WEIGHTS_PS},
      {0x137ED10E, SCENE_COPY_PS},
      {0x79098250, CAMERA_MOTION_BLUR_DOWNSAMPLE_PS},
      {0x1E9D8652, UBER_LUT_ONLY_PS},
      {0x5667D230, UBER_DOF_ONLY_PS},
   };

   class DeadSpace2 final : public LumaDisplayMapGame
   {
      // What "SceneMotionVectors" (and "OpenScene") asks of the game
      struct MotionVectorGame : MotionVectorGameBase<MotionVectorGame>
      {
         static constexpr const char* log_tag = "Dead Space 2";
         static constexpr MotionVectorPatch::Layout layout = MotionVectorPatches::layout;
         static constexpr auto cameras = MotionVectorPatches::cameras;
         static constexpr auto tie_break_rows = MotionVectorPatches::tie_break_rows;
         static constexpr auto frame_camera = MotionVectorPatches::frame_camera;
         static constexpr uint32_t previous_vertices_slot = MotionVectorPatches::previous_vertices_slot;
         // The engine's custom depth range puts infinity at A ~0.99: the fill rescales the depth by 1 / A (see "FillMotionVectors")
         static constexpr bool infinite_depth = true;
         // MarkerPatch's SSAA (ImprovedAntiAliasingMode 3 or 4) renders the scene larger than the output
         static constexpr bool ssaa_mods = true;
         static constexpr const char* ssaa_log = "[Dead Space 2] MarkerPatch's SSAA is on (ImprovedAntiAliasingMode 3 or 4): DLAA, FSR, XeSS, Luma TAA and SMAA T2x stay off";

         // The frame camera: gtGlobals' view projection (TS_GlobalVariables::viewProj), the immediate transform state the renderer
         // uploads from, current on the D3D thread at the G-buffer's first draw (the opaque draws read only their own ModelViewProj).
         // Found through its getter (013cf4b0: mov eax, 0x0211d340 + 0x1c0; ret; no ASLR, the DRM zeroes the TimeDateStamp); null on
         // another build.
         static constexpr uintptr_t exe_code_address = 0x13cf4b0;
         static constexpr uint8_t exe_code[] = {0xB8, 0x00, 0xD5, 0x11, 0x02, 0xC3};
         static constexpr uintptr_t view_projection_address = 0x211d500;
      };

      // The game's IDirect3DDevice9* (0x211cdcc), on the Steam build only (as the camera)
      IDirect3DDevice9* GetGameDevice() const override
      {
         return (MotionVectorGame::EngineViewProjection() ? *reinterpret_cast<IDirect3DDevice9* const*>(0x211cdcc) : nullptr);
      }

   public:
      DeadSpace2() : LumaDisplayMapGame({
                                           .smaa = "Replaces the game's anti-aliasing with SMAA (sharper, more complete edges; not used with DLSS/FSR/XeSS or Luma TAA).",
                                           .t2x = "Smoother edges and less shimmering, by blending each frame with the previous one.\nCan look slightly softer in motion. Not used with DLSS/FSR/XeSS, Luma TAA or MarkerPatch's SSAA.",
                                           .hide_ui = "Disables the in-game UI.",
                                        },
                        "Dead Space 2")
      {
      }

      // What "DllMain" sets for this exe before Core starts: the mirrored writers, the replacements it binds, the shader define and
      // the development names
      static void ConfigureLoad()
      {
         // The brightness slider is a D3D9 SetGammaRamp, which dgVoodoo forwards to the OS device ramp and distorts HDR output
         allow_disabling_gamma_ramp = true;
         // The light buffer and the scene are mirrored at their first light or material apply draw. Not the G-buffer, which shares their
         // RGBA8 desc: a mirror lets the gloss go past 1 (0x3A1AA0D5 scales it by a material constant, the light pass raises it to ~a^3).
         AutoUpgradeWriters({light_pixel_shaders, apply_pixel_shaders});
         // dgVoodoo's present blit (the same bytecode in every dgVoodoo game, as DS1_Present, and in 2.87.6 and 2.81.3) and the VP6 video
         // (the same dgVoodoo hash is in Dead Space 3's forward lists)
         constexpr ShaderRedirect redirects[] = {
            {"DS2_FXAA", MARKERPATCH_FXAA_PS, MARKERPATCH_FXAA_V281_PS},
            {"DS2_SMAANeighborhood", SMAA_NEIGHBORHOOD_PS, SMAA_NEIGHBORHOOD_V281_PS},
            {"DS2_SSAADownsample", SSAA_DOWNSAMPLE_PS, SSAA_DOWNSAMPLE_V281_PS},
            {"DS2_Present", PRESENT_BLIT_PS},
            {"DS2_VideoVP6", VIDEO_PS, VIDEO_V281_PS},
         };
         RedirectShaders(redirects);
         RedirectShaders(bloom_redirects);
         sub_game_shader_define = "GAME_DEAD_SPACE_2";
         sub_game_shaders_appendix = "DS2";

#if DEVELOPMENT
         // The gameplay uber post, LUT only, every frame
         forced_shader_names.emplace(UBER_LUT_ONLY_PS, "Uber Post (LUT only)");
         // The uber permutation and its post copy run only where the area's post-effect volume asks for an effect (live traces)
         forced_shader_names.emplace(UBER_DOF_ONLY_PS, "Uber Post (DoF only)");
         forced_shader_names.emplace(SCENE_COPY_PS, "Scene Copy (uber input)");
         forced_shader_names.emplace(CAMERA_MOTION_BLUR_DOWNSAMPLE_PS, "Camera Motion Blur Downsample");
         forced_shader_names.emplace(Shader::Hash_StrToNum("87246FA1"), "Camera Motion Blur");
         forced_shader_names.emplace(VIDEO_PS, "Video (VP6, BT.601 limited)");
         // Textured UI and overlays (RIG hologram, menus, MarkerPatch's achievements window), drawn after SMAA
         forced_shader_names.emplace(Shader::Hash_StrToNum("2A2C3104"), "UI");
         forced_shader_names.emplace(Shader::Hash_StrToNum("CBFBF98D"), "Deferred Sun Light (last light buffer pass)");
         forced_shader_names.emplace(Shader::Hash_StrToNum("5234894F"), "Deferred Material Apply");
         forced_shader_names.emplace(Shader::Hash_StrToNum("AC550A39"), "Deferred Material Apply");
         // MarkerPatch 3.3.1 (dinput8.dll, github.com/Wemino/MarkerPatch) draws its own passes and swaps some game shaders. Hashes mapped
         // offline from its embedded bytecode, the SMAA ones confirmed live. dgVoodoo specializes vertex shaders on the input layout, so
         // the VS hashes below can differ in game.
         // ImprovedAntiAliasingMode 2 (default) and 4: SMAA in place of the game's EdgeAA pass
         forced_shader_names.emplace(MARKERPATCH_SMAA_EDGES_PS, "MarkerPatch SMAA Edge Detection");
         forced_shader_names.emplace(MARKERPATCH_SMAA_WEIGHTS_PS, "MarkerPatch SMAA Blending Weight");
         forced_shader_names.emplace(SMAA_NEIGHBORHOOD_PS, "MarkerPatch SMAA Neighborhood Blending");
         // ImprovedAntiAliasingMode 1: FXAA in place of the EdgeAA pixel shaders (the game's own vertex shader)
         forced_shader_names.emplace(MARKERPATCH_FXAA_PS, "MarkerPatch FXAA");
         // ImprovedAntiAliasingMode 3 and 4: SSAA, the frame rendered at SSAAScale x the back buffer, then averaged into it
         forced_shader_names.emplace(SSAA_DOWNSAMPLE_PS, "MarkerPatch SSAA Downsample");
         // FixVertexNormals: replacements of game vertex shaders, so these draws lose their vanilla hashes
         forced_shader_names.emplace(Shader::Hash_StrToNum("7B1A4448"), "MarkerPatch Glass Reflection VS");
         forced_shader_names.emplace(Shader::Hash_StrToNum("202A858A"), "MarkerPatch Flashlight Overlay VS");
         forced_shader_names.emplace(Shader::Hash_StrToNum("90F04172"), "MarkerPatch Mesh Particle VS");
         forced_shader_names.emplace(Shader::Hash_StrToNum("368FBCDE"), "MarkerPatch Screen Mesh VS");
         forced_shader_names.emplace(Shader::Hash_StrToNum("90883B6F"), "MarkerPatch Motion Blur Velocity VS");
         // Live translations, matched by code to the offline ones
         forced_shader_names.emplace(Shader::Hash_StrToNum("76BC76F8"), "MarkerPatch Motion Blur Velocity VS");
         forced_shader_names.emplace(Shader::Hash_StrToNum("912E2945"), "MarkerPatch SMAA Edge Detection VS");
         forced_shader_names.emplace(Shader::Hash_StrToNum("92F966E1"), "MarkerPatch SMAA Blending Weight VS");
         forced_shader_names.emplace(Shader::Hash_StrToNum("4A9962FA"), "MarkerPatch SMAA Neighborhood Blending VS");
#endif
      }

      std::unique_ptr<std::byte[]> PatchShaderBytecodeSync(const std::byte* code, size_t& size, reshade::api::pipeline_subobject_type type, uint64_t shader_hash, const std::byte* shader_object, size_t shader_object_size) override
      {
         if (type != reshade::api::pipeline_subobject_type::pixel_shader)
            return nullptr;
         if (std::ranges::binary_search(apt_pixel_shaders, uint32_t(shader_hash)) || std::ranges::binary_search(hologram_pixel_shaders, uint32_t(shader_hash)))
            return DXBC::ClampOutput0(code, &size, DXBC::Output0Clamp::SATURATE);
         return PatchBlendInputOrUber(code, &size, uint32_t(shader_hash), blend_input_pixel_shaders, uber_pixel_shaders);
      }

      void OnInit(bool async) override
      {
         sr_game_tooltip = "DLSS, FSR and XeSS require Luma-Upscaler.exe next to the game's exe (Luma TAA doesn't).\nNone of them runs with MarkerPatch's SSAA (ImprovedAntiAliasingMode 3 or 4).\n";
         // "Hide Gameplay UI" (Core's "hide_ui" skips these draws; the present blit then maps the frame). APT only: holograms and video
         // logs stay
         shader_hashes_UI.pixel_shaders.insert(apt_pixel_shaders.begin(), apt_pixel_shaders.end());
         LumaDisplayMapGame::OnInit(async);
      }

      // The game's copy of the scene (MarkerPatch's SMAA input, the copy to the UI canvas) ends a scene no post pass ended
      bool OverrideCopyResource(ID3D11Device* native_device, DeviceData& device_data, uint64_t& dst_resource, uint64_t& src_resource) override
      {
         auto& gd = GetGameDeviceData(device_data);
         if (gd.mv.active && gd.mv.scene_open && gd.mv.scene_color && src_resource == reinterpret_cast<uintptr_t>(gd.mv.scene_color.get()))
         {
#if DEVELOPMENT
            gd.mv.stats.ended_by = 0;
#endif
            com_ptr<ID3D11DeviceContext> native_device_context;
            native_device->GetImmediateContext(&native_device_context);
            EndScene(native_device, native_device_context.get(), device_data, MotionVectorGame::infinite_depth);
         }
         return false;
      }

      DrawOrDispatchOverrideType OnDrawOrDispatch(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, CommandListData& cmd_list_data, DeviceData& device_data, reshade::api::shader_stage stages, const ShaderHashesList<OneShaderPerPipeline>& original_shader_hashes, bool is_custom_pass, bool& updated_cbuffers, std::function<void()>* original_draw_dispatch_func) override
      {
         const bool is_immediate = native_device_context->GetType() == D3D11_DEVICE_CONTEXT_IMMEDIATE;
         auto& gd = GetGameDeviceData(device_data);
         const uint32_t pixel_shader_hash = ToV287Hash(uint32_t(original_shader_hashes.pixel_shaders[0]), pixel_shader_hashes_from_v281);
         if (is_immediate)
         {
            CapturePendingUiBound(native_device, native_device_context, &gd);
         }
         // A game draw that can be drawn again with patched shaders (not Luma's, nor one whose shader Luma replaced: "is_custom_pass")
         const bool can_redraw = original_draw_dispatch_func && *original_draw_dispatch_func;
         const bool game_draw = !is_custom_pass && can_redraw;
         // The scene rendered this frame (DLSS-Best-Practices BK-1): Core's SR icon then warns when the upscaler didn't draw (SSAA, another
         // exe build). Not on the video.
         const bool scene_end = std::ranges::contains(scene_end_pixel_shaders, pixel_shader_hash);
         if (is_immediate && pixel_shader_hash != VIDEO_PS && scene_end)
         {
            device_data.has_drawn_main_post_processing = true;
         }
         const bool uber = std::ranges::binary_search(uber_pixel_shaders, pixel_shader_hash);
         if (uber)
         {
            // The patched LUT tail reads "Color Grading Intensity" there ("WrapUberLut")
            SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, reshade::api::shader_stage::pixel, LumaConstantBufferType::LumaSettings);
            if (is_immediate)
            {
               gd.uber_drawn = true;
            }
         }
         // Areas without the uber post end with depth of field or motion blur; their half and quarter resolution passes aren't the frame.
         // MarkerPatch's SSAA downsample (a replaced pass) writes the output sized frame the UI then draws on.
         const bool post = uber || pixel_shader_hash == SSAA_DOWNSAMPLE_PS || std::ranges::binary_search(post_pixel_shaders, pixel_shader_hash);
         if (is_immediate && post)
         {
            TrackPostTarget(native_device_context, device_data);
         }
         // "scene_over" (unless MarkerPatch's antialiasing slot already mapped the frame): with the uber post, the first game draw on the
         // frame after the last post pass (menus, RIG and pickup holograms, video logs; not MarkerPatch's SMAA edges and weights, the
         // replaced passes are custom passes), or the first APT draw on a screen sized target when no post pass drew (the pause menu stops
         // them).
         // Without it the engine still draws scene content after the post passes (additive particles, holograms, the in-world video
         // monitor), so only the 2D UI starts it: the first APT draw, or a video drawn without depth test (full screen video or video log;
         // the in-world monitor is depth tested). Frames with neither are mapped by the present blit.
         bool scene_over = std::ranges::binary_search(apt_pixel_shaders, pixel_shader_hash);
         if (gd.uber_drawn && gd.post_target)
         {
            scene_over = !post && !std::ranges::contains(markerpatch_smaa_pixel_shaders, pixel_shader_hash);
         }
         else if (!gd.uber_drawn && is_immediate && pixel_shader_hash == VIDEO_PS)
         {
            scene_over = IsVideoOverScene(native_device_context);
         }
         DrawDisplayMapOverScene(native_device, native_device_context, cmd_list_data, device_data, is_immediate && scene_over && !is_custom_pass);
         if (is_immediate && pixel_shader_hash == VIDEO_PS)
         {
            // Else (not over the mapped frame) the in-world monitor of a frame without the uber post
            BindVideoCBuffers(native_device_context, cmd_list_data, device_data, stages, &updated_cbuffers);
         }

         const bool apply = std::ranges::binary_search(apply_pixel_shaders, pixel_shader_hash);
         TrackSceneDepthWithoutScene<MotionVectorGame>(native_device, native_device_context, device_data, is_immediate && game_draw && apply);
         if (DrawSceneMotionVectors<MotionVectorGame>(native_device, native_device_context, cmd_list_data, device_data, stages, original_shader_hashes, original_draw_dispatch_func, is_immediate, game_draw,
                !gd.mv.scene_done && scene_end, apply, pixel_shader_hash))
            return DrawOrDispatchOverrideType::Replaced;
         // The HDR display map, where the scene ends: in MarkerPatch's SMAA neighborhood or FXAA replacement in uber frames (their
         // antialiasing only runs when neither an upscaler nor Luma's SMAA did; without the uber post scene content still draws after them,
         // see "scene_over"); else "DrawDisplayMap" before the UI, and the present blit for frames nothing mapped
         if (pixel_shader_hash == MARKERPATCH_FXAA_PS && is_immediate)
         {
            // Both: "updated_cbuffers" skips Core's own bind of the settings, which the display map reads. LumaData.CustomData3: the display
            // map runs elsewhere
            const bool map_here = gd.uber_drawn && !gd.display_mapped;
            SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, stages, LumaConstantBufferType::LumaSettings);
            SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, stages, LumaConstantBufferType::LumaData, gd.aa_drawn, gd.uber_drawn, map_here ? 0.f : 1.f);
            updated_cbuffers = true;
            if (map_here)
            {
               MarkUiBound(native_device_context, &gd);
               gd.display_mapped = true;
            }
#if DEVELOPMENT
            gd.aa_stats.game_aa_copies += gd.aa_drawn;
#endif
            return DrawOrDispatchOverrideType::None;
         }
         if (pixel_shader_hash == SMAA_NEIGHBORHOOD_PS || pixel_shader_hash == PRESENT_BLIT_PS)
         {
            const bool neighborhood = pixel_shader_hash == SMAA_NEIGHBORHOOD_PS;
            // Luma's SMAA in MarkerPatch's slot: from its copy of the frame (t0) into our output, which the replacement then reads instead
            com_ptr<ID3D11ShaderResourceView> frame_srv;
            bool smaa_drawn = false;
            if (neighborhood && is_immediate)
            {
               gd.game_aa_drawn = true;
               // Not "game_draw": this pass is replaced, so it counts as a custom pass
               smaa_drawn = RunSMAAInGameSlot(native_device, native_device_context, device_data, gd.smaa_slot_a && can_redraw, /* srgb_source */ true, &frame_srv);
            }
            // The neighborhood replacement only display maps once MarkerPatch's blending weights were skipped (see below), whether or not our
            // SMAA drew, and only in uber frames (LumaData.CustomData3: the display map runs elsewhere; the present blit doesn't read it)
            const bool map_here = neighborhood && gd.uber_drawn && !gd.display_mapped;
            // The present blit bounds what drew after the display map by "ui_bound" (t1, LumaData.CustomData4), drawn here to restore t1
            const bool present_ui_bound = !neighborhood && is_immediate && gd.ui_bound_valid && can_redraw;
            SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, stages, LumaConstantBufferType::LumaSettings);
            SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, stages, LumaConstantBufferType::LumaData, neighborhood ? (device_data.has_drawn_sr || gd.smaa_slot_a) : gd.display_mapped, gd.uber_drawn,
               map_here ? 0.f : 1.f, present_ui_bound ? 1.f : 0.f);
            updated_cbuffers = true;
            if (map_here && is_immediate)
            {
               MarkUiBound(native_device_context, &gd);
            }
            gd.display_mapped = neighborhood && (gd.display_mapped || gd.uber_drawn);
            if (present_ui_bound)
            {
               DrawPresentWithUiBound(native_device_context, &gd, original_draw_dispatch_func);
               return DrawOrDispatchOverrideType::Replaced;
            }
            if (!smaa_drawn)
               return DrawOrDispatchOverrideType::None;
            DrawFromSMAAOutput(native_device_context, &gd, frame_srv.get(), original_draw_dispatch_func);
            return DrawOrDispatchOverrideType::Replaced;
         }
         // TODO: the vanilla EdgeAA blur (no MarkerPatch AA, game option on) draws only where a level effect requests it (MarkerPatch forces
         // the request every frame, "OnScreenEdgeAARender"); none seen live yet, so it isn't skipped after Luma's AA
         // The upscaler antialiased this frame or Luma's SMAA takes MarkerPatch's slot: MarkerPatch's SMAA edges and weights would only
         // soften it (they write nothing else the frame reads)
         if ((gd.smaa_slot_a || device_data.has_drawn_sr) && game_draw && std::ranges::contains(markerpatch_smaa_pixel_shaders, pixel_shader_hash))
            return DrawOrDispatchOverrideType::Replaced;
         return DrawOrDispatchOverrideType::None;
      }
   };
} // namespace DS2

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
   if (ul_reason_for_call == DLL_PROCESS_ATTACH)
   {
      const char* project_name = PROJECT_NAME;
      const char* cleared_project_name = ((project_name[0] == '_') ? (project_name + 1) : project_name);

      uint32_t mod_version = 1;
      // For every exe: the game name also names the shader folder ("Dead Space Trilogy")
      Globals::SetGlobals(cleared_project_name, "Dead Space Trilogy Luma mod", "", mod_version);

      std::string executable_name = System::GetProcessExecutableName();
      std::transform(executable_name.begin(), executable_name.end(), executable_name.begin(), [](unsigned char c)
         { return char(std::tolower(c)); });
      // The picked game's 2.81.3 twins of the 2.87.6 hashes it keys by one hash (named after them below)
      [[maybe_unused]] std::span<const std::pair<uint32_t, uint32_t>> v281_to_v287;
      if (executable_name == "deadspace3.exe")
      {
         v281_to_v287 = DS3::pixel_shader_hashes_from_v281;
         DS3::DeadSpace3::ConfigureLoad();
         game = new DS3::DeadSpace3();
      }
      else if (executable_name == "dead space.exe")
      {
         v281_to_v287 = DS1::pixel_shader_hashes_from_v281;
         DS1::DeadSpace1::ConfigureLoad();
         game = new DS1::DeadSpace1();
      }
      else if (executable_name == "deadspace2.exe")
      {
         v281_to_v287 = DS2::pixel_shader_hashes_from_v281;
         DS2::DeadSpace2::ConfigureLoad();
         game = new DS2::DeadSpace2();
      }
      else
      {
         reshade::log::message(reshade::log::level::warning, "[Dead Space Trilogy] Unknown exe (not \"Dead Space.exe\", deadspace2.exe or deadspace3.exe): the mod stays inactive");
      }
#if DEVELOPMENT
      for (const auto& [v281_hash, v287_hash] : v281_to_v287)
      {
         if (const auto name = forced_shader_names.find(v287_hash); name != forced_shader_names.end())
         {
            forced_shader_names.emplace(v281_hash, name->second);
         }
      }
#endif

      // The shared upgrades, only for a supported game (an unknown exe keeps the default game, with nothing upgraded)
      if (game != &default_game)
      {
         // As in the other dgVoodoo mods: "force_borderless" also covers leaving fullscreen, so the window can't come back with a title bar
         prevent_fullscreen_state = true;
         force_borderless = true;

         // HDR: no tonemap and every target RGBA8, so what the light buffer and the scene keep above 1 is the signal
         swapchain_format_upgrade_type = TextureFormatUpgradesType::AllowedEnabled;
         swapchain_upgrade_type = SwapchainUpgradeType::scRGB;

         // Indirect, by writer (each game lists its writers above): dgVoodoo's own creation formats must stay, and the G-buffer and the
         // canvas share one RGBA8 desc. The canvas is mirrored at its first material apply draw; the chain then carries the post passes
         // that read it.
         texture_format_upgrades_type = TextureFormatUpgradesType::AllowedEnabled;
         enable_indirect_texture_format_upgrades = true;
         texture_format_upgrades_on_creation = false;
         enable_chain_indirect_texture_format_upgrades = ChainTextureFormatUpgradesType::DirectAndIndirectDependencies;
         texture_upgrade_formats = {
            reshade::api::format::r8g8b8a8_typeless, // dgVoodoo's D3D9 A8R8G8B8 surfaces
         };
         // "No1Px": dgVoodoo fills unused SRV slots with 1x1 placeholders, which pass the aspect filter
         texture_format_upgrades_2d_size_filters = (uint32_t)TextureFormatUpgrades2DSizeFilters::SwapchainResolution | (uint32_t)TextureFormatUpgrades2DSizeFilters::SwapchainAspectRatio | (uint32_t)TextureFormatUpgrades2DSizeFilters::No1Px;

         // The upscalers' mip bias (see "OnPresent") reaches only upgraded samplers. AF16x, as in the other dgVoodoo mods;
         // "force_upgrade_linear_samplers" also gives the trilinear ones the bias (Core rewrites only anisotropic samplers otherwise, TW2).
         enable_samplers_upgrade = true; // boot-time only (cannot be changed after device creation)
         samplers_upgrade_mode = 4;
         force_upgrade_linear_samplers = true;
      }
   }
   else if (ul_reason_for_call == DLL_PROCESS_DETACH && game != &default_game)
   {
      DeadSpaceGame::SetEvents(false);
   }

   CoreMain(hModule, ul_reason_for_call, lpReserved);

   return TRUE;
}
