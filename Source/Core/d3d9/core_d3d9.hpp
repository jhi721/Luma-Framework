#pragma once

// Luma's native D3D9 module (x86): a ReShade add-on on the game's own D3D9 device, upgraded to D3D9Ex, kept separate from core.hpp (D3D11 /
// D3D12). The game's image goes into a shared fp16 canvas (its back buffer draws are redirected there) and is handed through KMT shared
// textures to an in-process D3D12 presenter (presenter.hpp), which composes and presents a flip swapchain on the game's window. A D3D12 pass
// device (passes.hpp) runs RCAS / SMAA (T2x, predication) / the display map / Luma TAA / XeGTAO mid frame, the SR bridge
// (super_resolution.hpp) DLSS / FSR / XeSS. Pixel shaders are replaced by the CRC32 of their SM3 bytecode, unreplaced ones get their output
// clamped (sm3_patch.h), render targets of the render size are upgraded to fp16 and the pixel samplers to anisotropic.
//
// Use: include the game's CB structs (Shaders/<Game>/Includes/GameCBuffers.hlsl, LUMA_GAME_CB_STRUCTS) before this header, as cbuffers.h
// needs them; subclass "D3D9Game" with the game's hooks and call "CoreMain()" from the add-on's DllMain. Shaders come from the shader root
// ("ShadersRoot()") compiled at runtime; DEVELOPMENT also reads the offline harness' .cso files and D9N_* environment variables
// (_tools/d9_native_spike, test_state.h).
//
// Threading: the game's thread (ReShade's events), the presenter's thread and the watchdogs; no locks, the D3D9 device is assumed to be
// used from one thread (no CSMT).

#ifdef _WIN32
#define ImTextureID ImU64
#endif

// std::min / std::max over the Windows.h macros (the game's translation unit includes this first)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <d3d9.h>
#include <d3dcompiler.h>
#include <dxgi1_6.h>

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <set>
#include <span>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <deps/imgui/imgui.h>
#include <include/reshade.hpp>

#include "../includes/globals.h"
#include "../includes/minhook.h"
#include "../sr_bridge/SRBridgeProtocol.h"
#include "kmt_sync.h"
#include "passes.hpp"
#include "presenter.hpp"
#include "presenter_protocol.h"
#include "sm3_patch.h"
#include "super_resolution.hpp"
#include "test_state.h"

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "shell32.lib") // System (unity_build.cpp) and the games' links ("ShellExecuteA")
#pragma comment(lib, "ole32.lib")   // System::OpenExplorerToFile

#ifndef RESHADE_EXTERNS
// These are needed by ReShade (as core.hpp)
extern "C" __declspec(dllexport) const char* NAME = &Globals::MOD_NAME[0];
extern "C" __declspec(dllexport) const char* DESCRIPTION = &Globals::DESCRIPTION[0];
extern "C" __declspec(dllexport) const char* WEBSITE = &Globals::WEBSITE[0];
#endif

#include "../includes/settings_widgets.h" // After "NAME"

using namespace reshade::api;
using reshade::addon_event;

// The game half of a native D3D9 mod: "CoreMain()" calls these hooks from its ReShade events, at the points where the game's own work
// belongs. They run on the game's thread, for the served D3D9 device only.
class D3D9Game
{
public:
   virtual ~D3D9Game() = default;

   // Shaders/<shaders_folder>/ holds the game's shaders: pixel shader replacements named <Name>_0x<CRC32>.ps_3_0.hlsl and its passes
   const char* shaders_folder = "";
   // Defines of every runtime compiled shader (e.g. VANILLA_ENCODING_TYPE, LUMA_SETTINGS_CB_INDEX)
   std::vector<std::string> shader_defines;
   // The game's pass device shaders in Shaders/<shaders_folder>/: the display map PS ("main"), SMAA (Luma_SMAA_impl.hlsl's entry points), the
   // depth edges CS ("main") and XeGTAO (its entry points, compiled with "gtao_defines")
   std::string display_map_file, smaa_file, depth_edges_file, gtao_file;
   std::vector<std::string> gtao_defines;

   // DllMain, after the event registration: the game's lists and settings
   virtual void OnInit()
   {
   }
   // create_device (the D3D9Ex upgrade), after Core's settings: the game's settings
   virtual void OnCreateDevice()
   {
   }
   // The render size (shared scene, render target upgrade filter) of a back buffer of "width" x "height"
   virtual void GetRenderSize(uint32_t width, uint32_t height, uint32_t* render_width, uint32_t* render_height)
   {
      *render_width = width;
      *render_height = height;
   }
   // Pipeline events (D3D9: one shader per pipeline)
   virtual void OnInitPipeline(device* device, uint32_t count, const pipeline_subobject* subobjects, pipeline pipeline)
   {
   }
   virtual void OnDestroyPipeline(uint64_t pipeline)
   {
   }
   virtual void OnBindPipeline(pipeline_stage stages, pipeline pipeline)
   {
   }
   virtual void OnBindRenderTargets(device* device, uint32_t count, const resource_view* rtvs, resource_view depth_stencil)
   {
   }
   // SetTexture of the pixel samplers "first" to "first + count - 1" (their textures are in "out.ps_textures")
   virtual void OnBindPixelTextures(uint32_t first, uint32_t count)
   {
   }
   // The game's SetVertexShaderConstantF / SetPixelShaderConstantF, after the call
   virtual void OnPushConstants(shader_stage stages, uint32_t layout_param, uint32_t first, uint32_t count, const void* values)
   {
   }
   // Every draw, before the game's: puts back what the game's hooks changed on the device, then sets its own state for this draw
   virtual void OnDrawBegin(device* device, uint32_t count, uint32_t first, int32_t vertex_offset)
   {
   }
   // A draw of a replaced pixel shader: the game's host values in the injected rows (4 * INJECT_REGISTER_COUNT floats, c<inject_base>..)
   virtual void OnInjectConstants(device* device, float* rows)
   {
   }
   // After the injection: true cancels the game's draw (the game ran its own work instead, or hides it)
   virtual bool OnDraw(command_list* cmd_list)
   {
      return false;
   }
   // A clear (depth or render target) is next: puts back what the game's hooks changed on the device
   virtual void OnClear(IDirect3DDevice9* native)
   {
   }
   virtual void OnCopyTextureRegion(device* device, resource source, resource destination, const subresource_box* source_box,
      const subresource_box* destination_box)
   {
   }
   virtual void OnDestroyResource(uint64_t resource)
   {
   }
   // ResetRender's passes on the canvas: the game's inputs in "params" (SMAA predication depth, T2x motion), then what ran
   virtual void OnRunPasses(device* device, const D3DSURFACE_DESC& canvas, D3D9Passes::Params* params)
   {
   }
   virtual void OnPassesRan(const D3D9Passes::Params& params)
   {
   }
   // The SR bridge may start ("g_sr_upscaler" chosen, the presenter's GPU sync up): the game shares its SR textures and starts "sr"
   virtual void OnStartUpscaler(device* device)
   {
   }
   // The upscaler ran in the last frame (the samplers take its mip bias)
   virtual bool UpscaledLastFrame() const
   {
      return false;
   }
   // Before a Reset, a device loss or the teardown: the game's DEFAULT pool resources go
   virtual void OnDestroyShared(device* device)
   {
   }
   virtual void OnDestroySwapchain(device* device, bool resize)
   {
   }
   // The present event, before the hand-off
   virtual void OnPresent(device* device)
   {
   }
   // ReShade's add-on overlay, after the display settings
   virtual void DrawImGuiSettings()
   {
   }
   virtual void PrintImGuiAbout()
   {
   }
};

inline D3D9Game* d3d9_game = nullptr; // Set by "CoreMain()"

// The global Luma settings (cbuffers.h): the display settings the overlay edits (copied into the passes' inputs by
// "ApplyDisplaySettings()"), the display mode of the last present and the game's own settings
inline CB::LumaGlobalSettingsPadded cb_luma_global_settings = {};

// DEVELOPMENT: environment variables that add CRCs to a list at every create_pipeline ("CRC,CRC", the offline harness); the game adds its own
inline std::vector<std::pair<const char*, std::set<uint32_t>*>> development_crc_lists;

static HMODULE g_module = nullptr;
static D9NState state = {};
struct Output
{
   resource rt0 = {0}; // the game's RT0 texture (0: none or a surface)
   // Hand-off: the shared scene RT (KMT, opened by the presenter) and the D3D9
   // EVENT query of the hand-off.
   resource shared_rt = {0};
   IDirect3DQuery9* query9 = nullptr;
   // Game-side tracking.
   resource back_buffer = {0};
   resource final_source = {0}; // last upgraded RT the game StretchRect-ed into
                                // the back buffer this frame
   resource draw_source = {0};  // texture at s0 of a copy draw into the back
                                // buffer (preferred hand-off source)
   // Back-buffer redirect: the game's draws into the 8-bit back buffer land in
   // this shared fp16 canvas (the hand-off texture).
   resource canvas = {0};
   IDirect3DSurface9* canvas_surface = nullptr;
   // The canvas right after ResetRender's passes or a movie draw: the floor the
   // presenter clamps the later UI to (shared, the presenter's 4th texture).
   resource snapshot = {0};
   IDirect3DSurface9* snapshot_surface = nullptr;
   bool snapshot_valid = false; // taken this frame (cleared by the frame line)
   uint32_t last_reset_render = UINT32_MAX;
   bool reset_render_pending = false; // ResetRender drew: passes + snapshot due
   bool video_pending = false;        // a movie drew: snapshot due
   bool msaa_resolved = false;        // an fp16 multisampled resolve this frame
   // The game's movie PS: a draw into the canvas with one takes the snapshot
   // (the UI clamp floor), and the game may keep it visible under its HideUI
   std::set<uint32_t> video_crcs;
   // Unreplaced PS whose output clamp is UNORM8 instead of BLEND_INPUT (the
   // game's velocity writers into targets the upgrade makes fp16; env
   // D9N_VELOCITY_PS adds CRCs)
   std::set<uint32_t> velocity_crcs;
   // Output clamp patches by original CRC (empty: refused)
   std::unordered_map<uint32_t, std::vector<uint32_t>> clamped_ps;
   // The hand-off draw's viewport: the scene rect on the canvas
   D3DVIEWPORT9 scene_rect = {};
   bool implicit_rt0_pending =
      false;                    // RT0 may still be the real back buffer without a bind event
                                // (after canvas creation)
   bool redirect_used = false;  // the redirect was active since the last present
   bool redirect_noted = false; // risk note logged
   bool handoff_canvas = false; // last hand-off came from the canvas (dump)
   std::set<uint64_t> upgraded;
   std::set<uint64_t> matched_pipelines;
   std::map<uint64_t, uint32_t> pipeline_crc; // pixel shader CRC32 per pipeline
   // CRC32 -> replacement bytecode: the scan of Shaders/<Game> and, in
   // DEVELOPMENT, the env lists (loaded first, so they win)
   std::map<uint32_t, std::vector<uint8_t>> replacements;
   bool shaders_scanned = false;
   // The game's copy PS of the scene into the back buffer (its s0 is the
   // hand-off source; env D9N_HANDOFF_PS adds CRCs)
   std::set<uint32_t> handoff_crcs;
   uint32_t pending_crc = 0;
   bool pending_replaced = false;
   uint32_t bound_ps_crc = 0;
   bool rt0_is_back_buffer = false;
   uint64_t ps_textures[16] =
      {}; // pixel sampler stage -> texture (D3D9 view handle = resource handle)
   uint32_t rt0_width = 0,
            rt0_height =
               0; // 0 = not bound since the last Reset: the back buffer's size
   bool matched_bound = false;
   bool upgrade_pending = false;
};
static Output out;
static uint32_t g_logical_width = 0, g_logical_height = 0;
static float g_decode_gamma = 0.0f;
static effect_runtime* g_runtime_d3d9 = nullptr;
// Proxy lifecycle: the D3D9 device/swapchain served (bound at the first
// present), failure and flip-chain history.
static device* g_device = nullptr;
static swapchain* g_swapchain = nullptr;
static bool g_bound = false, g_output_failed = false;
static bool g_flip_ever =
   false; // a flip present happened on the HWND: the D3D9 image can never
          // return (PROXY_RESEARCH 1.6)

static IDXGIFactory1* g_factory =
   nullptr; // display queries; replaced when IsCurrent() turns FALSE
static bool g_mode_unsupported[3] = {};
static RECT g_last_window_rect = {};
static bool g_d3d9_latency_done = false;

// Presenter: the in-process D3D12 output device (presenter.hpp) on its own
// thread (presenter_protocol.h lines over a pipe). `process` is that thread.
struct Helper
{
   HANDLE process = nullptr, stdin_write = nullptr, mapping = nullptr;
   volatile LONG hung =
      0; // watchdog gave up on the thread (it cannot be killed)
   PresenterStats* stats = nullptr;
   uint64_t frame = 0; // last frame line sent
   HANDLE kmt[4] = {}; // shared scene RT, canvas, overlay, snapshot (KMT)
   resource overlay = {
      0}; // shared copy of the D3D9 back buffer after ReShade drew its GUI
   bool textures_sent = false;
   bool pending = false; // hand-off done, the frame line waits for ReShade's
                         // overlay (reshade_present)
   bool pending_canvas = false;
   double pending_event_start = 0;
   uint32_t seen_swapchain_creates = 0;
   // GPU hand-off sync (presenter_protocol.h "fences"): D3D9 queue / context of
   // the served device and two fences on its kernel device.
   KmtSync sync;
   KmtFence fence_in, fence_out;
   bool gpu_sync = false, kmt_gave_up = false;
   HANDLE watchdog = nullptr;
   volatile LONG64 sent = 0; // last frame line sent (watchdog)
};
static Helper hp;

// SR bridge (env D9N_SR=dlss|fsr|xess, helper mode on GPU sync):
// Luma-Upscaler.exe next to the add-on runs the upscaler at 1x (DLAA / native
// AA) on the shared scene RT (SRBridgeProtocol.h); once ready its output
// replaces the scene in the presenter's "textures" line. ponytail: zero motion,
// flat depth, no jitter or render scale until the game side has them; a hung
// (alive, stalled) upscaler is not killed, only an exited one releases the D3D9
// GPU wait.
static int g_sr_upscaler = -1;
static D3D9SR::Bridge sr;

// The offline harness' mutations (MUT_*, each restores one old behaviour so its check must fail); never set outside DEVELOPMENT
inline bool Mutated(int32_t bit)
{
#if DEVELOPMENT
   return (state.mutate & bit) != 0;
#else
   return false;
#endif
}

inline IDirect3DDevice9* NativeOf(device* host)
{
   return reinterpret_cast<IDirect3DDevice9*>(host->get_native());
}

// An environment variable of the offline harness (D9N_*): read in DEVELOPMENT only
inline DWORD DevelopmentEnv(const char* name, char* text, DWORD size)
{
#if DEVELOPMENT
   return GetEnvironmentVariableA(name, text, size);
#else
   return 0;
#endif
}

inline DWORD DevelopmentEnv(const wchar_t* name, wchar_t* text, DWORD size)
{
#if DEVELOPMENT
   return GetEnvironmentVariableW(name, text, size);
#else
   return 0;
#endif
}

// DEVELOPMENT: the offline harness (_tools/d9_native_spike, whose run_tests.ps1 sets T_RESHADE for t9native.exe) loaded the add-on. Core
// keeps its test inputs (the injected rows (2, .5, .25, 0), paper white 0.5) and the game its built-in defaults instead of its settings.
inline bool OfflineHarness()
{
#if DEVELOPMENT
   return GetEnvironmentVariableA("T_RESHADE", nullptr, 0) > 0;
#else
   return false;
#endif
}

// A setting as text: "key" of the add-on's ini section, in DEVELOPMENT the environment variable D9N_<key> over it. False: neither is set
inline bool ReadSetting(const char* key, char* text, DWORD size)
{
   size_t ini_size = size;
   bool found = reshade::get_config_value(nullptr, NAME, key, text, &ini_size);
   char env[512] = "";
   const DWORD env_size = DevelopmentEnv(("D9N_" + std::string(key)).c_str(), env, sizeof(env));
   if (env_size > 0 && env_size < sizeof(env))
   {
      strncpy_s(text, size, env, _TRUNCATE);
      found = true;
   }
   return found;
}

// The overlay's display settings into the display map's and the present clamp's inputs, and the composition's scale: the canvas' 1 is
// the UI paper white (the display map pre-scales the scene to it), in scRGB units (1 = 80 nits)
inline void ApplyDisplaySettings()
{
   if (!OfflineHarness())
   {
      state.paper_white = cb_luma_global_settings.UIPaperWhite / srgb_white_level;
   }
   state.map_peak_nits = cb_luma_global_settings.ScenePeakWhite;
   state.map_paper_white_nits = cb_luma_global_settings.ScenePaperWhite;
   state.map_ui_paper_white_nits = cb_luma_global_settings.UIPaperWhite;
}

// Display settings (ini, core.hpp's names): peak white 1000, paper white 203 for both the scene and the UI by default
inline void LoadDisplaySettings()
{
   cb_luma_global_settings.ScenePeakWhite = default_peak_white;
   cb_luma_global_settings.ScenePaperWhite = default_paper_white;
   cb_luma_global_settings.UIPaperWhite = default_paper_white;
   reshade::get_config_value(nullptr, NAME, "ScenePeakWhite", cb_luma_global_settings.ScenePeakWhite);
   reshade::get_config_value(nullptr, NAME, "ScenePaperWhite", cb_luma_global_settings.ScenePaperWhite);
   reshade::get_config_value(nullptr, NAME, "UIPaperWhite", cb_luma_global_settings.UIPaperWhite);
   ApplyDisplaySettings();
}
// The back buffer redirect into the fp16 canvas is on and has not been given
// up.
static bool Redirecting()
{
   return state.bb_redirect && !g_output_failed;
}

template <typename T>
static ULONG Free(T** object)
{
   if (!*object)
   {
      return 0;
   }
   const ULONG refs = (*object)->Release();
   *object = nullptr;
   return refs;
}

static double NowMs()
{
   static const double ms_per_tick = []
   {
      LARGE_INTEGER frequency;
      QueryPerformanceFrequency(&frequency);
      return 1000.0 / static_cast<double>(frequency.QuadPart);
   }();
   LARGE_INTEGER now;
   QueryPerformanceCounter(&now);
   return static_cast<double>(now.QuadPart) * ms_per_tick;
}

static std::vector<char> ReadModuleFile(const wchar_t* name)
{
   wchar_t path[MAX_PATH] = L"";
   GetModuleFileNameW(g_module, path, MAX_PATH);
   wcscpy_s(wcsrchr(path, L'\\') + 1,
      MAX_PATH - (wcsrchr(path, L'\\') + 1 - path), name);
   std::ifstream file(path, std::ios::binary);
   return {std::istreambuf_iterator<char>(file), {}};
}

// ---- Observability: ReShade.log lines, seen-shader CSV, hand-off dumps ----

static void Log(const char* format, ...)
{
   char text[1400];
   va_list args;
   va_start(args, format);
   vsnprintf(text, sizeof(text), format, args);
   va_end(args);
   reshade::log::message(reshade::log::level::info, text);
}

static std::wstring ModuleFilePath(const wchar_t* name)
{
   wchar_t path[MAX_PATH] = L"";
   GetModuleFileNameW(g_module, path, MAX_PATH);
   wcscpy_s(wcsrchr(path, L'\\') + 1,
      MAX_PATH - (wcsrchr(path, L'\\') + 1 - path), name);
   return path;
}

// ---- Shaders: the shader root, runtime compilation, the replacement scan ----

// "Luma" next to the game's executable; DEVELOPMENT falls back on the solution's "Shaders" while that is missing or empty (as core.hpp)
inline std::filesystem::path ShadersRoot()
{
   wchar_t executable[MAX_PATH] = L"";
   GetModuleFileNameW(nullptr, executable, MAX_PATH);
   std::filesystem::path root = std::filesystem::path(executable).parent_path() / Globals::MOD_NAME;
#if DEVELOPMENT && defined(SOLUTION_DIR)
   std::error_code error;
   if (!std::filesystem::is_directory(root, error) || std::filesystem::is_empty(root, error))
   {
      root = std::filesystem::path(SOLUTION_DIR) / "Shaders";
   }
#endif
   return root;
}

// fxc's include rule: each "#include" relative to the file that has it. D3D_COMPILE_STANDARD_FILE_INCLUDE resolves nested ones from the
// root file's folder, which breaks "Includes/Common.hlsl" including "Math.hlsl" and the like.
// ponytail: the same as Shader::FxcD3DInclude (utils/shader_compiler.hpp), which needs core.hpp's prelude; share it once that header
// compiles on its own.
struct IncludeFromIncluder final : ID3DInclude
{
   std::filesystem::path root_folder;
   std::map<const void*, std::filesystem::path> folder_of_data;

   HRESULT __stdcall Open(D3D_INCLUDE_TYPE, LPCSTR file_name, LPCVOID parent_data, LPCVOID* data, UINT* bytes) override
   {
      const auto parent = folder_of_data.find(parent_data);
      const std::filesystem::path path = ((parent != folder_of_data.end() ? parent->second : root_folder) / file_name).lexically_normal();
      std::ifstream stream(path, std::ios::binary);
      if (!stream)
      {
         return E_FAIL;
      }
      const std::string text{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
      char* copy = new char[text.size() + 1];
      std::memcpy(copy, text.c_str(), text.size() + 1);
      folder_of_data[copy] = path.parent_path();
      *data = copy;
      *bytes = static_cast<UINT>(text.size());
      return S_OK;
   }

   HRESULT __stdcall Close(LPCVOID data) override
   {
      folder_of_data.erase(data);
      delete[] static_cast<const char*>(data);
      return S_OK;
   }
};

// fxc (D3DCompileFromFile, includes relative to the including file) of "file"'s "entry" with "defines" ("NAME" or "NAME=VALUE"); SM3
// targets as the offline harness builds them (/Gec /O3). Empty on failure (logged).
// ponytail: D3DCompiler_47.dll from the system, not Shader::CompileShaderFromFile: that compiles SM3 at D3DCOMPILE_OPTIMIZATION_LEVEL0,
// not the /O3 the ports were verified at (sm3_equiv.py), and needs core.hpp's prelude (Hash::MD5, InitShaderCompiler); switch with an
// optimization level parameter, for its bundled D3DCompiler_47_x32.dll on Wine.
inline std::vector<uint8_t> CompileShader(const std::filesystem::path& file, const char* entry, const char* target,
   const std::vector<std::string>& defines)
{
   std::vector<std::string> names, values;
   for (const std::string& define : defines)
   {
      const size_t equals = define.find('=');
      names.push_back(define.substr(0, equals));
      values.push_back(equals == std::string::npos ? "1" : define.substr(equals + 1));
   }
   std::vector<D3D_SHADER_MACRO> macros;
   for (size_t i = 0; i < names.size(); ++i)
   {
      macros.push_back({.Name = names[i].c_str(), .Definition = values[i].c_str()});
   }
   macros.push_back({});
   const UINT flags = (target[3] == '3' ? D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY | D3DCOMPILE_OPTIMIZATION_LEVEL3 : 0);
   ID3DBlob* code = nullptr;
   ID3DBlob* errors = nullptr;
   IncludeFromIncluder include; // Not an aggregate (virtual), so no designated initializer
   include.root_folder = file.parent_path();
   const HRESULT hr = D3DCompileFromFile(file.c_str(), macros.data(), &include, entry, target, flags, 0, &code, &errors);
   std::vector<uint8_t> bytecode;
   if (SUCCEEDED(hr))
   {
      const auto* bytes = static_cast<const uint8_t*>(code->GetBufferPointer());
      bytecode.assign(bytes, bytes + code->GetBufferSize());
   }
   else
   {
      Log("d9native: shader %ls (%s %s) failed to compile 0x%08X: %.1000s", file.c_str(), entry, target, static_cast<uint32_t>(hr),
         errors ? static_cast<const char*>(errors->GetBufferPointer()) : "");
   }
   Free(&code);
   Free(&errors);
   return bytecode;
}

// "file" (relative to the shader root) compiled with the game's defines and "defines". DEVELOPMENT: the offline harness' build of it,
// "cso" next to the add-on (build.ps1), when present.
inline std::vector<uint8_t> LoadShader(const wchar_t* cso, const std::string& file, const char* entry, const char* target,
   const std::vector<std::string>& defines = {})
{
#if DEVELOPMENT
   if (const std::vector<char> built = ReadModuleFile(cso); !built.empty())
   {
      return {built.begin(), built.end()};
   }
#endif
   std::vector<std::string> all_defines = d3d9_game->shader_defines;
   all_defines.insert(all_defines.end(), defines.begin(), defines.end());
#if DEVELOPMENT
   // cbuffers.h has the dev settings in LumaGlobalSettings in DEVELOPMENT (as core.hpp)
   all_defines.push_back("CPU_DEVELOPMENT=1");
#endif
   return CompileShader(ShadersRoot() / file, entry, target, all_defines);
}

// Every Shaders/<Game>/<Name>_0x<CRC32>.ps_3_0.hlsl ("main") into "out.replacements" under its CRC32 (of the game's bytecode), unless a
// DEVELOPMENT list already replaces it.
inline void ScanReplacementShaders()
{
   constexpr std::wstring_view suffix = L".ps_3_0.hlsl";
   const std::filesystem::path folder = ShadersRoot() / d3d9_game->shaders_folder;
   size_t found = 0, compiled = 0;
   std::error_code error;
   for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(folder, error))
   {
      const std::wstring name = entry.path().filename().wstring();
      const size_t crc_at = name.size() - suffix.size() - 8;
      if (!name.ends_with(suffix) || name.size() < suffix.size() + 11 || name.compare(crc_at - 2, 2, L"0x") != 0)
      {
         continue;
      }
      ++found;
      const uint32_t crc = wcstoul(name.substr(crc_at, 8).c_str(), nullptr, 16);
      if (out.replacements.contains(crc))
      {
         continue;
      }
      if (std::vector<uint8_t> code = CompileShader(entry.path(), "main", "ps_3_0", d3d9_game->shader_defines); !code.empty())
      {
         out.replacements[crc] = std::move(code);
         ++compiled;
      }
   }
   Log("d9native: %zu of %zu replacement pixel shaders compiled from %ls", compiled, found, folder.c_str());
}

struct Observe
{
   uint32_t frames = 0;
   std::map<uint32_t, uint32_t> replaced_creates, matched_draws;
   std::map<uint32_t, uint32_t> rt_sizes; // (width << 16 | height) -> count
   std::set<std::string> logged_failures;
   std::set<uint64_t> seen_shaders; // (pixel ? 1 : 0) << 32 | crc
   bool csv_loaded = false;
   size_t logged_handoff_count = 0;
   double last_log_ms = 0, last_dump_ms = 0;
   uint32_t dump_count = 0;
   double dump_every_s = 0;
   bool f10_down = false;
   uint32_t src_width = 0, src_height = 0;
   int src_format = 0;
} obs;
#if DEVELOPMENT

// Every distinct PS / VS CRC once into d9native_seen_shaders.csv (entries of
// earlier runs are kept and not repeated).
static void RecordSeenShader(bool pixel, uint32_t crc, const void* code,
   size_t size)
{
   const std::wstring path = ModuleFilePath(L"d9native_seen_shaders.csv");
   if (!obs.csv_loaded)
   {
      obs.csv_loaded = true;
      std::ifstream file(path);
      std::string line;
      while (std::getline(file, line))
      {
         const bool is_pixel = line.rfind("ps,", 0) == 0;
         if (is_pixel || line.rfind("vs,", 0) == 0)
         {
            obs.seen_shaders.insert((static_cast<uint64_t>(is_pixel) << 32) |
                                    strtoul(line.c_str() + 3, nullptr, 16));
         }
      }
   }
   if (!obs.seen_shaders.insert((static_cast<uint64_t>(pixel) << 32) | crc)
          .second)
   {
      return;
   }
   FILE* csv = nullptr;
   _wfopen_s(&csv, path.c_str(), L"a");
   if (!csv)
   {
      return;
   }
   fseek(csv, 0, SEEK_END);
   if (ftell(csv) == 0)
   {
      fputs("stage,crc32,bytes,version_token\n", csv);
   }
   fprintf(csv, "%s,%08X,%zu,%08X\n", pixel ? "ps" : "vs", crc, size,
      size >= 4 ? *static_cast<const uint32_t*>(code) : 0u);
   fclose(csv);
}

static void MaybeLogStats()
{
   const double now = NowMs();
   if (obs.last_log_ms == 0)
   {
      obs.last_log_ms = now;
      return;
   }
   if (now - obs.last_log_ms < 2000)
   {
      return;
   }
   obs.last_log_ms = now;
   char rts[256] = "";
   for (const auto& [size, count] : obs.rt_sizes)
   {
      sprintf_s(rts + strlen(rts), sizeof(rts) - strlen(rts), "%ux%u*%u ",
         size >> 16, size & 0xFFFF, count);
   }
   char repl[256] = "";
   for (const auto& [crc, creates] : obs.replaced_creates)
   {
      const auto draws = obs.matched_draws.find(crc);
      sprintf_s(repl + strlen(repl), sizeof(repl) - strlen(repl), "%08X:%u/%u ",
         crc, creates,
         draws == obs.matched_draws.end() ? 0u : draws->second);
   }
   Log("d9native: frames=%u d3d11_present ok=%u occluded=%u skipped=%u "
       "failed=%u (last 0x%08X) handoff draw=%u stretch=%u back_buffer=%u "
       "canvas=%u (redirected binds=%u copies=%u clears=%u) copies_from_bb=%u "
       "source=%ux%u fmt=%d | rt_upgraded=%u [%s] rt_skipped=%u | replaced "
       "creates/matched draws [%s] injected_draws=%u | d3d9_resets=%u "
       "d3d11_resizes=%u output=%ux%u render=%ux%u | d3d9 Present HRESULT: not "
       "observable from an add-on (present event fires before it)",
      obs.frames, state.present_ok, state.present_occluded,
      state.present_skipped, state.present_failed, state.last_present_hr,
      state.handoff_draw, state.handoff_stretch, state.handoff_back_buffer,
      state.handoff_canvas, state.redirected_binds, state.redirected_copies,
      state.redirected_clears, state.copies_from_bb, obs.src_width,
      obs.src_height, obs.src_format, state.rt_upgraded, rts, state.rt_skipped,
      repl, state.injected_draws, state.resets_seen, state.output_resizes,
      state.out_width, state.out_height, state.render_width,
      state.render_height);
}
#endif

static float HalfToFloat(uint16_t half)
{
   const uint32_t sign = (half >> 15) & 1;
   const uint32_t exponent = (half >> 10) & 0x1F;
   const uint32_t mantissa = half & 0x3FF;
   float value = 0;
   if (exponent == 0)
   {
      value = static_cast<float>(mantissa) * (1.0f / 16777216.0f);
   }
   else if (exponent == 31)
   {
      value = mantissa ? NAN : INFINITY;
   }
   else
   {
      value = (1.0f + static_cast<float>(mantissa) / 1024.0f) *
              powf(2.0f, static_cast<float>(static_cast<int>(exponent) - 15));
   }
   return sign ? -value : value;
}

static bool g_force_canvas_fail = false; // D9N_FORCE_FAIL=canvas (test)

// The output device must be on the D3D9 device's adapter (shared surfaces do
// not cross adapters: PROXY_RESEARCH 2.11).
static bool D3D9AdapterLuid(device* host, LUID* luid)
{
   auto* native = reinterpret_cast<IDirect3DDevice9*>(host->get_native());
   D3DDEVICE_CREATION_PARAMETERS creation = {};
   IDirect3D9* d3d = nullptr;
   IDirect3D9Ex* d3d_ex = nullptr;
   bool have_luid = false;
   if (SUCCEEDED(native->GetCreationParameters(&creation)) &&
       SUCCEEDED(native->GetDirect3D(&d3d)))
   {
      have_luid =
         SUCCEEDED(d3d->QueryInterface(IID_PPV_ARGS(&d3d_ex))) &&
         SUCCEEDED(d3d_ex->GetAdapterLUID(creation.AdapterOrdinal, luid));
   }
   Free(&d3d_ex);
   Free(&d3d);
   return have_luid;
}

// SDR white level of a monitor in nits (DISPLAYCONFIG_SDR_WHITE_LEVEL, 1000 =
// 80 nits). 80 when unavailable.
static float QuerySdrWhiteNits(HMONITOR monitor, int32_t* hr)
{
   *hr = E_FAIL;
   MONITORINFOEXW info = {};
   info.cbSize = sizeof(info);
   UINT32 path_count = 0, mode_count = 0;
   if (!GetMonitorInfoW(monitor, &info) ||
       GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &path_count,
          &mode_count) != ERROR_SUCCESS)
   {
      return 80.0f;
   }
   std::vector<DISPLAYCONFIG_PATH_INFO> paths(path_count);
   std::vector<DISPLAYCONFIG_MODE_INFO> modes(mode_count);
   if (QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &path_count, paths.data(),
          &mode_count, modes.data(), nullptr) != ERROR_SUCCESS)
   {
      return 80.0f;
   }
   for (UINT32 i = 0; i < path_count; ++i)
   {
      DISPLAYCONFIG_SOURCE_DEVICE_NAME source = {};
      source.header = {DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME, sizeof(source),
         paths[i].sourceInfo.adapterId, paths[i].sourceInfo.id};
      if (DisplayConfigGetDeviceInfo(&source.header) != ERROR_SUCCESS ||
          wcscmp(source.viewGdiDeviceName, info.szDevice) != 0)
      {
         continue;
      }
      DISPLAYCONFIG_SDR_WHITE_LEVEL white = {};
      white.header = {DISPLAYCONFIG_DEVICE_INFO_GET_SDR_WHITE_LEVEL,
         sizeof(white), paths[i].targetInfo.adapterId,
         paths[i].targetInfo.id};
      if (DisplayConfigGetDeviceInfo(&white.header) == ERROR_SUCCESS &&
          white.SDRWhiteLevel > 0)
      {
         *hr = S_OK;
         return static_cast<float>(white.SDRWhiteLevel) / 1000.0f * 80.0f;
      }
   }
   return 80.0f;
}

// HDR state of the output with the largest intersection with the window (not
// GetContainingOutput: stale once IsCurrent() is FALSE,
// https://learn.microsoft.com/en-us/windows/win32/direct3darticles/high-dynamic-range).
// Uses the current factory.
static void QueryDisplay(HWND hwnd)
{
   ++state.display_queries;
   RECT window = {};
   GetWindowRect(hwnd, &window);
   g_last_window_rect = window;
   IDXGIOutput* best = nullptr;
   LONGLONG best_area = -1;
   IDXGIAdapter1* adapter = nullptr;
   for (UINT a = 0;
      g_factory && SUCCEEDED(g_factory->EnumAdapters1(a, &adapter)); ++a)
   {
      IDXGIOutput* output = nullptr;
      for (UINT o = 0; SUCCEEDED(adapter->EnumOutputs(o, &output)); ++o)
      {
         DXGI_OUTPUT_DESC desc = {};
         output->GetDesc(&desc);
         const RECT& r = desc.DesktopCoordinates;
         const LONGLONG width =
                           std::max<LONG>(0, std::min(window.right, r.right) -
                                                std::max(window.left, r.left)),
                        height =
                           std::max<LONG>(0, std::min(window.bottom, r.bottom) -
                                                std::max(window.top, r.top));
         if (width * height > best_area)
         {
            best_area = width * height;
            Free(&best);
            best = output;
         }
         else
         {
            output->Release();
         }
      }
      Free(&adapter);
   }
   int actual_hdr = 0;
   if (IDXGIOutput6* output6 = nullptr;
      best && SUCCEEDED(best->QueryInterface(IID_PPV_ARGS(&output6))))
   {
      DXGI_OUTPUT_DESC1 desc1 = {};
      if (SUCCEEDED(output6->GetDesc1(&desc1)))
      {
         actual_hdr =
            desc1.ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020;
         state.sdr_white_nits =
            QuerySdrWhiteNits(desc1.Monitor, &state.sdr_white_hr);
      }
      output6->Release();
   }
   Free(&best);
   state.display_hdr_actual = actual_hdr;
   state.display_hdr =
      state.force_display_hdr >= 0 ? state.force_display_hdr : actual_hdr;
   Log("d9native: display: output HDR=%d (actual %d, force %d), SDR white level "
       "%.1f nits (query hr 0x%08X)",
      state.display_hdr, actual_hdr, state.force_display_hdr,
      state.sdr_white_nits, static_cast<uint32_t>(state.sdr_white_hr));
}

// Requested mode, minus modes the swapchain refused, minus HDR modes on an
// output without HDR (when the fallback is on).
static int EffectiveMode()
{
   int mode = state.output_mode_request;
   if (mode < 0 || mode > OUTPUT_SRGB || g_mode_unsupported[mode])
   {
      mode = OUTPUT_SRGB;
   }
   if (mode != OUTPUT_SRGB && state.hdr_fallback && !state.display_hdr)
   {
      mode = OUTPUT_SRGB;
   }
   return mode;
}

// Per present: IsCurrent() (the Win32 change notification, per the HDR page
// above); FALSE -> new factory, re-read the output and refresh the swapchain's
// colour space. The window moving also re-reads (IsCurrent does not cover moves
// between monitors).
static void PollDisplay(HWND hwnd)
{
   if (state.clear_mode_unsupported)
   {
      state.clear_mode_unsupported = 0;
      memset(g_mode_unsupported, 0, sizeof(g_mode_unsupported));
   }
   if (!g_factory)
   {
      CreateDXGIFactory1(IID_PPV_ARGS(&g_factory));
      QueryDisplay(hwnd);
      return;
   }
   if (Mutated(MUT_NO_ISCURRENT))
   {
      return;
   }
   RECT window = {};
   GetWindowRect(hwnd, &window);
   bool current = g_factory->IsCurrent();
   if (state.fault_is_current > 0)
   {
      --state.fault_is_current;
      current = false;
   }
   const bool moved = memcmp(&window, &g_last_window_rect, sizeof(RECT)) != 0;
   if (current && !moved)
   {
      return;
   }
   if (!current)
   {
      ++state.is_current_false;
      Free(&g_factory);
      CreateDXGIFactory1(IID_PPV_ARGS(&g_factory));
   }
   QueryDisplay(hwnd);
   if (!current)
   {
      ++state.color_refreshes; // forwarded in the frame line
   }
}

// Checks the D3D9 side of the shared hand-off texture against the sharing
// contract (the presenter reports its side: PresenterStats.shared_desc_ok).
static void VerifySharingContract()
{
   IDirect3DTexture9* texture9 = nullptr;
   if (out.shared_rt.handle &&
       SUCCEEDED(reinterpret_cast<IUnknown*>(out.shared_rt.handle)
             ->QueryInterface(IID_PPV_ARGS(&texture9))))
   {
      D3DSURFACE_DESC desc9 = {};
      texture9->GetLevelDesc(0, &desc9);
      state.share_texture_path = (desc9.Usage & D3DUSAGE_RENDERTARGET) &&
                                 desc9.Pool == D3DPOOL_DEFAULT &&
                                 desc9.Format == D3DFMT_A16B16G16R16F;
      state.share_single_mip = texture9->GetLevelCount() == 1 &&
                               desc9.MultiSampleType == D3DMULTISAMPLE_NONE;
      texture9->Release();
   }
   else
   {
      state.share_texture_path = state.share_single_mip = 0;
   }
   Log("d9native: sharing contract (D3D9 side): CreateTexture "
       "path/RT/DEFAULT/fp16=%d, single mip/no MSAA=%d",
      state.share_texture_path, state.share_single_mip);
}

// (3) fp16 DEFAULT-pool D3D9 RT created shared (KMT) through ReShade, opened by
// the presenter.
static bool CreateShared(device* device, UINT width, UINT height)
{
   resource_desc desc;
   desc.type = resource_type::texture_2d;
   desc.texture = {.width = width,
      .height = height,
      .depth_or_layers = 1,
      .levels = 1,
      .format = format::r16g16b16a16_float,
      .samples = 1};
   desc.heap = memory_heap::default_;
   // Both bind usages of the sharing contract (PROXY_RESEARCH 2.3): render
   // target and shader resource; texture_2d reaches CreateTexture in ReShade
   // (d3d9_impl_device.cpp create_resource), the only path documented for D3D11
   // OpenSharedResource.
   desc.usage = resource_usage::render_target | resource_usage::shader_resource |
                resource_usage::copy_dest;
   desc.flags = resource_flags::shared;
   HANDLE kmt_handle = nullptr;
   state.create_shared_ok = device->create_resource(
      desc, nullptr, resource_usage::general, &out.shared_rt, &kmt_handle);
   if (!state.create_shared_ok || !kmt_handle)
   {
      state.create_shared_ok = 0;
      out.shared_rt = {0};
      return false;
   }
   ++state.shared_created;
   reinterpret_cast<IDirect3DDevice9*>(device->get_native())
      ->CreateQuery(D3DQUERYTYPE_EVENT, &out.query9);
   hp.kmt[0] = kmt_handle; // opened by the presenter
   VerifySharingContract();
   return out.query9 != nullptr;
}

// Shared fp16 canvas of the back buffer's (logical) size. False = creation
// failed (the caller falls back).
static bool CreateCanvas(device* device, UINT width, UINT height)
{
   if (!Redirecting())
   {
      return true;
   }
   resource_desc desc;
   desc.type = resource_type::texture_2d;
   desc.texture = {.width = width,
      .height = height,
      .depth_or_layers = 1,
      .levels = 1,
      .format = format::r16g16b16a16_float,
      .samples = 1};
   desc.heap = memory_heap::default_;
   desc.usage = resource_usage::render_target | resource_usage::shader_resource |
                resource_usage::copy_dest;
   desc.flags = resource_flags::shared;
   HANDLE kmt_handle = nullptr;
   if (!device->create_resource(desc, nullptr, resource_usage::general,
          &out.canvas, &kmt_handle) ||
       !kmt_handle)
   {
      out.canvas = {0};
      Log("d9native: canvas creation FAILED (%ux%u)", width, height);
      return false;
   }
   hp.kmt[1] = kmt_handle; // opened by the presenter
   reinterpret_cast<IDirect3DTexture9*>(out.canvas.handle)
      ->GetSurfaceLevel(0, &out.canvas_surface);
   // The UI clamp floor (Present_0x880A17D3's t1), opened by the presenter
   if (device->create_resource(desc, nullptr, resource_usage::general,
          &out.snapshot, &hp.kmt[3]) &&
       hp.kmt[3])
   {
      reinterpret_cast<IDirect3DTexture9*>(out.snapshot.handle)
         ->GetSurfaceLevel(0, &out.snapshot_surface);
   }
   out.implicit_rt0_pending = true;
   Log("d9native: canvas %ux%u A16B16G16R16F shared%s, back buffer redirect on",
      width, height, g_force_canvas_fail ? " (forced failure)" : "");
   return !g_force_canvas_fail && out.canvas_surface;
}

// Its shared textures belong to the MV module (mv.textures).
static void StopUpscaler()
{
   D3D9SR::Stop(&sr);
   state.sr_ready = 0;
}

// (7) Everything DEFAULT-pool the add-on owns must go before the game's Reset.
// D3D12 passes (d9passes.cpp, second D3D12 device on this thread): a round
// trip inside the frame: StretchRect into the shared input, D3D9 KMT signal
// "in", the pass queue waits, runs the passes and signals "out", D3D9 waits for
// it (GPU side) and StretchRects the result back.

struct PassState
{
   KmtSync sync;
   KmtFence in, out;
   resource input = {0}, output = {0};
   IDirect3DSurface9 *input_surface = nullptr, *output_surface = nullptr;
   UINT width = 0, height = 0;
   // Shared R32F copy of the depth prepass (SMAA predication), its size
   resource depth = {0};
   IDirect3DSurface9* depth_surface = nullptr;
   UINT depth_width = 0, depth_height = 0;
   HANDLE motion = nullptr;    // mv.kmt[0] as the pass device opened it (T2x)
   HANDLE taa_color = nullptr; // mv.kmt[3] as Luma TAA opened the MV textures
   // XeGTAO's output (shared R32F, the depth's size), sampled by the apply PS
   resource ao = {0};
   UINT ao_width = 0, ao_height = 0;
   uint64_t value = 0;
   uint32_t last_present = UINT32_MAX; // one run per frame
   bool open = false, gave_up = false;
};
static PassState g_pass;

static void PassDestroyTextures(device* device)
{
   if (g_pass.open)
   {
      D3D9Passes::SetTextures(nullptr, nullptr);
      D3D9Passes::SetDepth(nullptr);
      D3D9Passes::SetMotion(nullptr);
      D3D9Passes::SetTaaTextures(nullptr, nullptr, nullptr, nullptr);
      D3D9Passes::SetGtaoOutput(nullptr);
   }
   g_pass.motion = g_pass.taa_color = nullptr;
   Free(&g_pass.input_surface);
   Free(&g_pass.output_surface);
   Free(&g_pass.depth_surface);
   g_pass.depth_width = g_pass.depth_height = 0;
   g_pass.ao_width = g_pass.ao_height = 0;
   for (resource* texture :
      {&g_pass.input, &g_pass.output, &g_pass.depth, &g_pass.ao})
   {
      if (texture->handle)
      {
         device->destroy_resource(*texture);
         *texture = {0};
      }
   }
   g_pass.width = g_pass.height = 0;
}

static void PassStop(device* device)
{
   PassDestroyTextures(device);
   D3D9Passes::Close();
   KmtDestroyFence(&g_pass.in);
   KmtDestroyFence(&g_pass.out);
   KmtRelease(&g_pass.sync);
   g_pass = {.gave_up = g_pass.gave_up};
   state.pass_open = 0;
}

// Pass device and fences once per device (one attempt).
// The compose shaders (Luma_D3D9_Compose.hlsl): the presenter's composition and the pass device's fullscreen triangle. Loaded on the game's
// thread (fxc is not thread safe), read by the presenter's Init.
inline std::vector<uint8_t> compose_vs, compose_ps;

inline void EnsureComposeShaders()
{
   if (compose_vs.empty())
   {
      compose_vs = LoadShader(L"compose_vs.cso", "Global/Luma_D3D9_Compose.hlsl", "VSMain", "vs_5_0");
   }
   if (compose_ps.empty())
   {
      compose_ps = LoadShader(L"compose_ps.cso", "Global/Luma_D3D9_Compose.hlsl", "PSMain", "ps_5_0");
   }
}

// The pass device on the D3D9 adapter with the KMT fences of "g_pass", its shaders loaded now (Shaders/Global and the game's passes)
inline bool OpenPassDevice(LUID luid)
{
   EnsureComposeShaders();
   const std::string game = std::string(d3d9_game->shaders_folder) + "/";
   const std::string smaa = game + d3d9_game->smaa_file, gtao = game + d3d9_game->gtao_file;
   const auto gtao_with = [](std::initializer_list<std::string> defines)
   {
      std::vector<std::string> all_defines = d3d9_game->gtao_defines;
      all_defines.insert(all_defines.end(), defines);
      return all_defines;
   };
   // Only read while "Open()" creates the pipelines
   const std::vector<uint8_t> display_map_ps = LoadShader(L"display_map_ps.cso", game + d3d9_game->display_map_file, "main", "ps_5_0"),
                              rcas_ps = LoadShader(L"rcas_ps.cso", "Global/Luma_RCAS_PS.hlsl", "sharpen_ps", "ps_5_0"),
                              smaa_edge_detection_vs = LoadShader(L"smaa_edge_detection_vs.cso", smaa, "smaa_edge_detection_vs", "vs_5_0"),
                              smaa_edge_detection_ps = LoadShader(L"smaa_edge_detection_ps.cso", smaa, "smaa_edge_detection_ps", "ps_5_0"),
                              smaa_blending_weight_calculation_vs = LoadShader(L"smaa_blending_weight_calculation_vs.cso", smaa,
                                 "smaa_blending_weight_calculation_vs", "vs_5_0"),
                              smaa_blending_weight_calculation_ps = LoadShader(L"smaa_blending_weight_calculation_ps.cso", smaa,
                                 "smaa_blending_weight_calculation_ps", "ps_5_0"),
                              smaa_neighborhood_blending_vs = LoadShader(L"smaa_neighborhood_blending_vs.cso", smaa,
                                 "smaa_neighborhood_blending_vs", "vs_5_0"),
                              smaa_neighborhood_blending_ps = LoadShader(L"smaa_neighborhood_blending_ps.cso", smaa,
                                 "smaa_neighborhood_blending_ps", "ps_5_0"),
                              smaa_t2x_blending_weight_calculation_ps = LoadShader(L"smaa_t2x_blending_weight_calculation_ps.cso", smaa,
                                 "smaa_blending_weight_calculation_ps", "ps_5_0", {"SMAA_T2X=1"}),
                              smaa_t2x_neighborhood_blending_ps = LoadShader(L"smaa_t2x_neighborhood_blending_ps.cso", smaa,
                                 "smaa_neighborhood_blending_ps", "ps_5_0", {"SMAA_T2X=1"}),
                              smaa_t2x_resolve_ps = LoadShader(L"smaa_t2x_resolve_ps.cso", smaa, "smaa_resolve_ps", "ps_5_0", {"SMAA_T2X=1"}),
                              depth_edges_cs = LoadShader(L"depth_edges_cs.cso", game + d3d9_game->depth_edges_file, "main", "cs_5_0"),
                              luma_taa_cs = LoadShader(L"luma_taa_cs.cso", "Global/Luma_TAA_CS.hlsl", "main", "cs_5_0", {"TAA_QUALITY=2"}),
                              gtao_prefilter_cs = LoadShader(L"gtao_prefilter_cs.cso", gtao, "prefilter_depths16x16_cs", "cs_5_0", gtao_with({})),
                              gtao_main_cs = LoadShader(L"gtao_main_cs.cso", gtao, "main_pass_cs", "cs_5_0", gtao_with({})),
                              gtao_denoise_cs = LoadShader(L"gtao_denoise_cs.cso", gtao, "denoise_pass_cs", "cs_5_0", gtao_with({"XE_GTAO_FINAL_APPLY=0"})),
                              gtao_denoise_final_cs = LoadShader(L"gtao_denoise_final_cs.cso", gtao, "denoise_pass_cs", "cs_5_0",
                                 gtao_with({"XE_GTAO_FINAL_APPLY=1", "XE_GTAO_FINAL_OUTPUT_TYPE=float"}));
   return D3D9Passes::Open(luid, g_pass.in.nt, g_pass.out.nt,
      {.compose_vs = compose_vs,
         .display_map_ps = display_map_ps,
         .rcas_ps = rcas_ps,
         .smaa_edge_detection_vs = smaa_edge_detection_vs,
         .smaa_edge_detection_ps = smaa_edge_detection_ps,
         .smaa_blending_weight_calculation_vs = smaa_blending_weight_calculation_vs,
         .smaa_blending_weight_calculation_ps = smaa_blending_weight_calculation_ps,
         .smaa_neighborhood_blending_vs = smaa_neighborhood_blending_vs,
         .smaa_neighborhood_blending_ps = smaa_neighborhood_blending_ps,
         .smaa_t2x_blending_weight_calculation_ps = smaa_t2x_blending_weight_calculation_ps,
         .smaa_t2x_neighborhood_blending_ps = smaa_t2x_neighborhood_blending_ps,
         .smaa_t2x_resolve_ps = smaa_t2x_resolve_ps,
         .depth_edges_cs = depth_edges_cs,
         .luma_taa_cs = luma_taa_cs,
         .gtao_prefilter_cs = gtao_prefilter_cs,
         .gtao_main_cs = gtao_main_cs,
         .gtao_denoise_cs = gtao_denoise_cs,
         .gtao_denoise_final_cs = gtao_denoise_final_cs});
}

static bool PassEnsureOpen(device* device)
{
   if (g_pass.gave_up)
   {
      return false;
   }
   if (!g_pass.open)
   {
      LUID luid = {};
      g_pass.open = D3D9AdapterLuid(device, &luid) &&
                    KmtSelect(&g_pass.sync, KmtGeneration(device)) &&
                    KmtCreateSharedFence(&g_pass.sync, &g_pass.in) &&
                    KmtCreateSharedFence(&g_pass.sync, &g_pass.out) &&
                    OpenPassDevice(luid);
      if (!g_pass.open)
      {
         Log("d9native: D3D12 passes not available (%s, kmt %s 0x%08X)",
            D3D9Passes::Error(), g_pass.sync.first_error_call,
            static_cast<uint32_t>(g_pass.sync.first_error));
         PassStop(device);
         g_pass.gave_up = true;
         return false;
      }
      state.pass_open = 1;
      Log("d9native: D3D12 pass device ready");
   }
   return true;
}

// The pass device and the shared fp16 textures of the source's size.
static bool PassEnsure(device* device, UINT width, UINT height)
{
   if (!PassEnsureOpen(device))
   {
      return false;
   }
   if (g_pass.width == width && g_pass.height == height)
   {
      return true;
   }
   PassDestroyTextures(device);
   resource_desc desc;
   desc.type = resource_type::texture_2d;
   desc.texture = {.width = width,
      .height = height,
      .depth_or_layers = 1,
      .levels = 1,
      .format = format::r16g16b16a16_float,
      .samples = 1};
   desc.heap = memory_heap::default_;
   desc.usage = resource_usage::render_target | resource_usage::shader_resource |
                resource_usage::copy_dest;
   desc.flags = resource_flags::shared;
   HANDLE kmt[2] = {};
   bool ok = true;
   resource* textures[2] = {&g_pass.input, &g_pass.output};
   IDirect3DSurface9** surfaces[2] = {&g_pass.input_surface,
      &g_pass.output_surface};
   for (int i = 0; i < 2 && ok; ++i)
   {
      ok = device->create_resource(desc, nullptr, resource_usage::general,
              textures[i], &kmt[i]) &&
           kmt[i] &&
           SUCCEEDED(reinterpret_cast<IDirect3DTexture9*>(textures[i]->handle)
                 ->GetSurfaceLevel(0, surfaces[i]));
   }
   if (!ok || !D3D9Passes::SetTextures(kmt[0], kmt[1]))
   {
      Log("d9native: D3D12 pass textures %ux%u failed (%s)", width, height,
         D3D9Passes::Error());
      PassDestroyTextures(device);
      return false;
   }
   g_pass.width = width;
   g_pass.height = height;
   return true;
}

// A shared R32F texture for the pass device (a render target: StretchRect and
// ColorFill reach it).
static bool CreateSharedR32f(device* device, UINT width, UINT height,
   resource* texture, HANDLE* kmt)
{
   resource_desc shared;
   shared.type = resource_type::texture_2d;
   shared.texture = {.width = width,
      .height = height,
      .depth_or_layers = 1,
      .levels = 1,
      .format = format::r32_float,
      .samples = 1};
   shared.heap = memory_heap::default_;
   shared.usage = resource_usage::render_target |
                  resource_usage::shader_resource | resource_usage::copy_dest;
   shared.flags = resource_flags::shared;
   return device->create_resource(shared, nullptr, resource_usage::general,
             texture, kmt) &&
          *kmt;
}

// The R32F depth prepass `texture` copied into the shared R32F texture the
// pass device reads (PassSetDepth), made at its size.
static bool ShareDepth(device* device, IDirect3DTexture9* texture,
   const D3DSURFACE_DESC& desc)
{
   if (g_pass.depth_width != desc.Width || g_pass.depth_height != desc.Height)
   {
      D3D9Passes::SetDepth(nullptr);
      Free(&g_pass.depth_surface);
      if (g_pass.depth.handle)
      {
         device->destroy_resource(g_pass.depth);
         g_pass.depth = {0};
      }
      HANDLE kmt = nullptr;
      if (!CreateSharedR32f(device, desc.Width, desc.Height, &g_pass.depth,
             &kmt) ||
          FAILED(reinterpret_cast<IDirect3DTexture9*>(g_pass.depth.handle)
                ->GetSurfaceLevel(0, &g_pass.depth_surface)) ||
          !D3D9Passes::SetDepth(kmt))
      {
         Log("d9native: shared depth %ux%u failed (%s)", desc.Width, desc.Height,
            D3D9Passes::Error());
         return false;
      }
      g_pass.depth_width = desc.Width;
      g_pass.depth_height = desc.Height;
   }
   IDirect3DSurface9* source = nullptr;
   const bool copied =
      SUCCEEDED(texture->GetSurfaceLevel(0, &source)) &&
      SUCCEEDED(NativeOf(device)->StretchRect(
         source, nullptr, g_pass.depth_surface, nullptr, D3DTEXF_NONE));
   Free(&source);
   return copied;
}

// ResetRender's passes on the canvas (display map, RCAS), then the snapshot the
// presenter clamps the later UI to (Present_0x880A17D3's t1). Run at the first
// event after the hand-off draw wrote the canvas (D3D9 has no post-draw event).
static void RunPasses(device* device)
{
   auto* native = reinterpret_cast<IDirect3DDevice9*>(device->get_native());
   IDirect3DSurface9* const canvas = out.canvas_surface;
   D3DSURFACE_DESC desc = {};
   D3D9Passes::Params params = {.display_map = state.display_map != 0,
      .display_mode = EffectiveMode() == OUTPUT_SRGB ? 0u : 1u,
      .peak_white_nits = state.map_peak_nits,
      .game_paper_white_nits = state.map_paper_white_nits,
      .ui_paper_white_nits = state.map_ui_paper_white_nits,
      .contrast = state.map_contrast,
      .saturation = state.map_saturation,
      .highlights_desaturation =
         state.map_highlights_desaturation,
      .smaa = state.smaa != 0,
      .rcas_sharpness = state.rcas_sharpness};
   if (!out.query9 || !canvas ||
       (!params.display_map && !params.smaa && !(params.rcas_sharpness > 0)) ||
       FAILED(canvas->GetDesc(&desc)) ||
       !PassEnsure(device, desc.Width, desc.Height))
   {
      return;
   }
   // The game's inputs (SMAA predication depth, T2x motion), before the flush
   // below so its copies are ordered before the D3D9 signal
   d3d9_game->OnRunPasses(device, desc, &params);
   params.scene_origin[0] = float(out.scene_rect.X);
   params.scene_origin[1] = float(out.scene_rect.Y);
   const uint64_t value = ++g_pass.value;
   const HRESULT copy_hr = native->StretchRect(
      canvas, nullptr, g_pass.input_surface, nullptr, D3DTEXF_NONE);
   out.query9->Issue(D3DISSUE_END);
   const HRESULT flush_hr = out.query9->GetData(nullptr, 0, D3DGETDATA_FLUSH);
   int result = 0;
   // ponytail: no watchdog: a removed pass device never signals "out" and the
   // D3D9 wait below hangs the GPU (TDR); add one if device removal shows up.
   if (FAILED(copy_hr) || FAILED(flush_hr) ||
       !KmtSignalObject(&g_pass.sync, g_pass.in.object, value) ||
       !D3D9Passes::Run(value, params, &result) ||
       !KmtWaitObject(&g_pass.sync, g_pass.out.object, value))
   {
      ++state.pass_failures;
      Log("d9native: ResetRender passes failed (copy 0x%08X flush 0x%08X, %s, "
          "kmt %s 0x%08X)",
         uint32_t(copy_hr), uint32_t(flush_hr), D3D9Passes::Error(),
         g_pass.sync.first_error_call,
         static_cast<uint32_t>(g_pass.sync.first_error));
      return;
   }
   if (!Mutated(MUT_NO_PASS_COPY_BACK))
   {
      native->StretchRect(result ? g_pass.output_surface : g_pass.input_surface,
         nullptr, canvas, nullptr, D3DTEXF_NONE);
   }
   ++state.pass_runs;
   state.predication_runs += params.predication ? 1 : 0;
   d3d9_game->OnPassesRan(params);
   state.t2x_runs += params.t2x ? 1 : 0;
   state.t2x_history_runs += params.t2x && params.t2x_history ? 1 : 0;
}

// The canvas as the UI clamp floor of this frame (ResetRender after its
// passes, or a movie draw).
static void TakeSnapshot(device* device)
{
   if (!out.snapshot_surface || Mutated(MUT_NO_SNAPSHOT))
   {
      return;
   }
   auto* native = reinterpret_cast<IDirect3DDevice9*>(device->get_native());
   if (SUCCEEDED(native->StretchRect(out.canvas_surface, nullptr,
          out.snapshot_surface, nullptr,
          D3DTEXF_NONE)))
   {
      out.snapshot_valid = true;
      ++state.snapshots;
   }
}

// At the first event after a draw that wrote the canvas: ResetRender's passes
// and snapshot, or a movie draw's snapshot.
static void FlushCanvasDraw(device* device)
{
   if (out.reset_render_pending)
   {
      out.reset_render_pending = false;
      RunPasses(device);
      ++state.reset_renders;
      TakeSnapshot(device);
   }
   else if (out.video_pending)
   {
      out.video_pending = false;
      TakeSnapshot(device);
   }
}

static void DestroyShared(device* device)
{
   PassDestroyTextures(device);
   state.d3d9_rc_left += static_cast<int32_t>(Free(&out.query9));
   if (out.shared_rt.handle)
   {
      auto* unknown = reinterpret_cast<IUnknown*>(out.shared_rt.handle);
      unknown->AddRef();
      device->destroy_resource(out.shared_rt);
      state.d3d9_rc_left += static_cast<int32_t>(
         unknown->Release()); // 0 = nobody else held the texture
      out.shared_rt = {0};
      ++state.shared_destroyed;
   }
   if (out.canvas.handle)
   {
      Free(&out.canvas_surface);
      device->destroy_resource(out.canvas);
      out.canvas = {0};
   }
   if (out.snapshot.handle)
   {
      Free(&out.snapshot_surface);
      device->destroy_resource(out.snapshot);
      out.snapshot = {0};
   }
   out.reset_render_pending = out.video_pending = false;
   if (hp.overlay.handle)
   {
      device->destroy_resource(hp.overlay);
      hp.overlay = {0};
   }
   std::fill(std::begin(hp.kmt), std::end(hp.kmt), nullptr);
   if (hp.textures_sent)
   {
      // The presenter drops its views.
      DWORD written = 0;
      WriteFile(hp.stdin_write, "textures 0 0 0 0\n", 17, &written, nullptr);
      hp.textures_sent = false;
   }
   StopUpscaler(); // restarts at the next present
   d3d9_game->OnDestroyShared(device);
   sr.gave_up = false;
   hp.pending = false;
   out.redirect_used = false;
   out.final_source = {0};
   out.draw_source = {0};
   std::fill(std::begin(out.ps_textures), std::end(out.ps_textures), 0);
   out.rt0_width = out.rt0_height = 0;
}

// Render size = D9N_RENDER_SIZE (DEVELOPMENT: "bb" = the game's back buffer
// size, "WxH") or the game's (GetRenderSize). Injection base = D9N_INJECT_BASE
// (DEVELOPMENT, default 208).
static void ComputeRenderSize()
{
   wchar_t text[32] = L"";
   DevelopmentEnv(L"D9N_RENDER_SIZE", text, 32);
   UINT width = state.width, height = state.height;
   if (!wcscmp(text, L"bb"))
   {
      // Game's back buffer size.
   }
   else if (UINT w = 0, h = 0; swscanf_s(text, L"%ux%u", &w, &h) == 2)
   {
      width = w;
      height = h;
   }
   else
   {
      d3d9_game->GetRenderSize(state.width, state.height, &width, &height);
   }
   state.render_width = width;
   state.render_height = height;
   Log("d9native: back buffer %ux%u (logical %ux%u) -> render size %ux%u "
       "(D9N_RENDER_SIZE=\"%ls\")",
      state.bb_width, state.bb_height, state.width, state.height, width, height,
      text);
   wchar_t base_text[16] = L"";
   DevelopmentEnv(L"D9N_INJECT_BASE", base_text, 16);
   state.inject_base = wcstoul(base_text, nullptr, 10);
   if (!state.inject_base)
   {
      state.inject_base = INJECT_BASE_REGISTER;
   }
   wchar_t redirect_text[8] = L"";
   state.bb_redirect =
      !DevelopmentEnv(L"D9N_BB_REDIRECT", redirect_text, 8) ||
      wcstol(redirect_text, nullptr, 10) != 0;
   Log("d9native: D9N_BB_REDIRECT -> %d", state.bb_redirect);
   wchar_t src_text[8] = L"";
   state.bb_src_redirect =
      !DevelopmentEnv(L"D9N_BB_SRC_REDIRECT", src_text, 8) ||
      wcstol(src_text, nullptr, 10) != 0;
   Log("d9native: D9N_BB_SRC_REDIRECT -> %d", state.bb_src_redirect);
   Log("d9native: injection base c%u (%u registers: c%u..c%u)",
      state.inject_base, INJECT_REGISTER_COUNT, state.inject_base,
      state.inject_base + INJECT_REGISTER_COUNT - 1);
}

// P10: the proxy serves one D3D9 swapchain, the one of the first present
// (RenoDX dp:1444-1448). Before that the swapchain of the latest init_swapchain
// is tracked; dummy devices and additional swapchains created later are
// ignored.
static bool OurSwapchain(swapchain* swapchain, bool is_init)
{
   if (Mutated(MUT_NO_GUARD))
   {
      return true;
   }
   const bool ours = g_bound
                        ? swapchain == g_swapchain
                        : (is_init || !g_swapchain || swapchain == g_swapchain);
   state.guard_ignored += !ours;
   return ours;
}

// Command-list events carry no swapchain: compare the device.
static bool OurDevice(device* device)
{
   return device->get_api() == device_api::d3d9 &&
          (Mutated(MUT_NO_GUARD) || !g_device || device == g_device);
}

// D3D9Ex frame latency 1 (default is 3; range 1-20):
// https://learn.microsoft.com/en-us/windows/win32/api/d3d9/nf-d3d9-idirect3ddevice9ex-setmaximumframelatency
static void SetD3D9Latency(device* device)
{
   if (!state.latency_enabled)
   {
      return;
   }
   if (IDirect3DDevice9Ex* device_ex = nullptr;
      SUCCEEDED(reinterpret_cast<IUnknown*>(device->get_native())
            ->QueryInterface(IID_PPV_ARGS(&device_ex))))
   {
      UINT latency = 0;
      state.d3d9_latency_hr = device_ex->SetMaximumFrameLatency(1);
      if (SUCCEEDED(device_ex->GetMaximumFrameLatency(&latency)))
      {
         state.d3d9_latency = static_cast<int32_t>(latency);
      }
      device_ex->Release();
   }
}

static void UpdateGeometry(swapchain* swapchain)
{
   device* device = swapchain->get_device();
   const resource back_buffer = swapchain->get_current_back_buffer();
   const resource_desc back_buffer_desc = device->get_resource_desc(back_buffer);
   state.bb_width = back_buffer_desc.texture.width;
   state.bb_height = back_buffer_desc.texture.height;
   state.width = g_logical_width ? g_logical_width : state.bb_width;
   state.height = g_logical_height ? g_logical_height : state.bb_height;
   out.back_buffer = back_buffer;
   ComputeRenderSize();
   state.msaa_back_buffer = back_buffer_desc.texture.samples > 1;
   if (state.msaa_back_buffer && state.bb_redirect)
   {
      // A non-MSAA fp16 canvas cannot replace an MSAA back buffer (render target
      // and depth-stencil must match).
      Log("d9native: back buffer is MSAA (%u samples): redirect off, the game's "
          "own back buffer is handed off",
         back_buffer_desc.texture.samples);
      state.bb_redirect = 0;
   }
   if (IDirect3DDevice9Ex* device_ex = nullptr;
      SUCCEEDED(reinterpret_cast<IUnknown*>(device->get_native())
            ->QueryInterface(IID_PPV_ARGS(&device_ex))))
   {
      state.is_ex = 1;
      device_ex->Release();
   }
}

// ---- Presenter helper (D9N_PRESENTER=helper) ----

static bool HelperGone()
{
   return hp.hung || WaitForSingleObject(hp.process, 0) == WAIT_OBJECT_0;
}

static bool SendHelper(const char* line)
{
   DWORD written = 0;
   const DWORD size = static_cast<DWORD>(strlen(line));
   return hp.stdin_write &&
          WriteFile(hp.stdin_write, line, size, &written, nullptr) &&
          written == size;
}

// EOF ends the helper; a helper that does not exit within 2 s is killed.
static void StopHelper()
{
   if (hp.stdin_write)
   {
      CloseHandle(hp.stdin_write);
   }
   if (hp.process)
   {
      // ponytail: a hung presenter thread is abandoned (holds its swapchain and
      // device), never terminated mid-call; game restart clears it.
      if (WaitForSingleObject(hp.process, 2000) != WAIT_OBJECT_0)
      {
         Log("d9native: presenter thread did not exit, abandoned");
      }
      CloseHandle(hp.process);
   }
   if (hp.stats)
   {
      UnmapViewOfFile(hp.stats);
   }
   if (hp.mapping)
   {
      CloseHandle(hp.mapping);
   }
   if (hp.watchdog)
   { // ends with the process, after releasing the D3D9 waits
      WaitForSingleObject(hp.watchdog, 3000);
      CloseHandle(hp.watchdog);
      hp.watchdog = nullptr;
   }
   KmtDestroyFence(&hp.fence_in);
   KmtDestroyFence(&hp.fence_out);
   KmtRelease(&hp.sync);
   hp.gpu_sync = false;
   hp.sent = 0;
   hp.hung = 0;
   hp.process = hp.stdin_write = hp.mapping = nullptr;
   hp.stats = nullptr;
   hp.frame = 0;
   hp.textures_sent = hp.pending = false;
   hp.seen_swapchain_creates = 0;
   state.output_ready = 0;
}

// Creation failure (P5). Before the flip chain ever presented, the D3D9 Present
// stays visible: drop the redirect (the game's draws go to its own back buffer
// again) and the presenter with its swapchain. After a flip present the D3D9
// image never returns on that HWND (PROXY_RESEARCH 1.6): the window keeps the
// last frame. ponytail: no error colour frame (the D3D11 path drew one); add
// one through the presenter if a game run shows a frozen frame confuses.
static void FailOutput(device* device, const char* reason)
{
   if (g_output_failed)
   {
      return;
   }
   g_output_failed = true;
   state.output_failed = 1;
   auto* native = reinterpret_cast<IDirect3DDevice9*>(device->get_native());
   IDirect3DSurface9 *render_target = nullptr, *back_buffer = nullptr;
   if (out.canvas_surface &&
       SUCCEEDED(native->GetRenderTarget(0, &render_target)) &&
       render_target == out.canvas_surface &&
       SUCCEEDED(
          native->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &back_buffer)))
   {
      native->SetRenderTarget(0, back_buffer);
   }
   Free(&render_target);
   Free(&back_buffer);
   DestroyShared(device);
   if (!g_flip_ever)
   {
      StopHelper();
      Log("d9native: ERROR %s: output disabled, the game keeps its own D3D9 "
          "Present",
         reason);
   }
   else
   {
      Log("d9native: ERROR %s after the flip chain presented: no return to the "
          "D3D9 Present is possible on this window, "
          "composing the error state",
         reason);
   }
}

inline FILE* presenter_log = nullptr; // d9present.log next to the add-on

// The presenter thread's body (presenter.hpp) on the compose shaders
inline DWORD WINAPI PresenterMain(void* pipe)
{
   if (!presenter_log)
   {
      _wfopen_s(&presenter_log, ModuleFilePath(L"d9present.log").c_str(), L"a");
   }
   return D3D9Presenter::PresenterThread(pipe, std::as_bytes(std::span(compose_vs)), std::as_bytes(std::span(compose_ps)), presenter_log);
}

static bool StartHelper(device* device, HWND hwnd)
{
   LUID luid = {};
   if (!D3D9AdapterLuid(device, &luid))
   {
      Log("d9native: presenter helper: no D3D9Ex adapter LUID");
      return false;
   }
   hp.mapping = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE,
      0, sizeof(PresenterStats), nullptr);
   hp.stats = hp.mapping ? static_cast<PresenterStats*>(MapViewOfFile(
                              hp.mapping, FILE_MAP_WRITE, 0, 0, 0))
                         : nullptr;
   HANDLE pipe_read = nullptr;
   // Empty when missing: the presenter fails in Init, after the start line, so
   // the error reaches the stats
   EnsureComposeShaders();
   if (hp.stats && CreatePipe(&pipe_read, &hp.stdin_write, nullptr, 0))
   {
      hp.process =
         CreateThread(nullptr, 0, PresenterMain, pipe_read, 0, nullptr);
      if (!hp.process)
      {
         CloseHandle(pipe_read);
      }
   }
   if (!hp.process)
   {
      StopHelper();
      return false;
   }
   char line[160];
   snprintf(
      line, sizeof(line), "start %d %llu %lu %ld %llu\n", PRESENTER_VERSION,
      static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(hwnd)),
      luid.LowPart, luid.HighPart,
      static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(hp.mapping)));
   SendHelper(line);
   ++state.helper_starts;
   state.helper_pid = GetThreadId(hp.process);
   hp.kmt_gave_up = false;
   Log("d9native: presenter thread started (tid %u, start #%u)",
      state.helper_pid, state.helper_starts);
   return true;
}

// Runs while the helper has the fences: when its process ends (normally or
// not) "out" goes to UINT64_MAX so no D3D9 GPU wait outlives it (the game
// thread may be blocked in D3D9 behind such a wait, so this cannot wait for
// the next present). A helper alive but without progress on "out" for 5 s is
// killed first.
static DWORD WINAPI HelperWatchdog(void*)
{
   uint64_t last_done = 0;
   double since = NowMs();
   while (WaitForSingleObject(hp.process, 500) == WAIT_TIMEOUT)
   {
      const uint64_t done = KmtCompleted(hp.fence_out);
      const uint64_t sent =
         static_cast<uint64_t>(InterlockedCompareExchange64(&hp.sent, 0, 0));
      if (done >= sent || done != last_done)
      {
         last_done = done;
         since = NowMs();
      }
      else if (NowMs() - since > 5000)
      {
         InterlockedExchange(&hp.hung, 1);
         break;
      }
   }
   KmtCpuSignal(&hp.sync, hp.fence_out, UINT64_MAX);
   return 0;
}

// GPU sync once the D3D9 queue is known (KmtSelect counts its submissions, so a
// few frames in). Any failure keeps the CPU-drained hand-off (E0).
// D9N_HELPER_SYNC=e0 keeps E0 (A/B).
static void SetupHelperSync(device* device)
{
   char text[8] = "";
   DevelopmentEnv("D9N_HELPER_SYNC", text, sizeof(text));
   hp.kmt_gave_up = true; // one attempt per helper
   if (!_stricmp(text, "e0"))
   {
      return;
   }
   const int generation = KmtGeneration(device);
   HANDLE remote_in = nullptr, remote_out = nullptr;
   const bool ok =
      generation && KmtSelect(&hp.sync, generation) &&
      KmtCreateSharedFence(&hp.sync, &hp.fence_in) &&
      KmtCreateSharedFence(&hp.sync, &hp.fence_out) &&
      DuplicateHandle(GetCurrentProcess(), hp.fence_in.nt, GetCurrentProcess(),
         &remote_in, 0, FALSE, DUPLICATE_SAME_ACCESS) &&
      DuplicateHandle(GetCurrentProcess(), hp.fence_out.nt, GetCurrentProcess(),
         &remote_out, 0, FALSE, DUPLICATE_SAME_ACCESS);
   if (!ok)
   {
      Log("d9native: presenter helper GPU sync not available (generation %d, "
          "%s 0x%08X), staying on CPU waits",
         generation, hp.sync.first_error_call,
         static_cast<uint32_t>(hp.sync.first_error));
      KmtDestroyFence(&hp.fence_in);
      KmtDestroyFence(&hp.fence_out);
      KmtRelease(&hp.sync);
      return;
   }
   char line[96];
   snprintf(
      line, sizeof(line), "fences %llu %llu\n",
      static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(remote_in)),
      static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(remote_out)));
   if (!SendHelper(line))
   {
      return; // the next frame finds the helper dead
   }
   hp.watchdog = CreateThread(nullptr, 0, HelperWatchdog, nullptr, 0, nullptr);
   hp.gpu_sync = hp.watchdog != nullptr;
   state.e3_hw_queue = hp.sync.hw_queue;
   Log("d9native: presenter helper GPU sync on D3D9 %s 0x%08X",
      hp.sync.hw_queue ? "hw queue" : "context",
      hp.sync.hw_queue ? hp.sync.queue : hp.sync.context);
}

// Shared copy of the back buffer: ReShade's GUI, drawn on the cleared D3D9 back
// buffer, composed by the helper.
static bool CreateOverlay(device* device)
{
   resource_desc desc;
   desc.type = resource_type::texture_2d;
   desc.texture = {.width = state.bb_width,
      .height = state.bb_height,
      .depth_or_layers = 1,
      .levels = 1,
      .format = format::b8g8r8a8_unorm,
      .samples = 1};
   desc.heap = memory_heap::default_;
   desc.usage = resource_usage::render_target | resource_usage::copy_dest;
   desc.flags = resource_flags::shared;
   return device->create_resource(desc, nullptr, resource_usage::general,
             &hp.overlay, &hp.kmt[2]) &&
          hp.kmt[2];
}

// Helper died or hung. Before any flip present the game keeps its own D3D9
// Present. After one the window keeps the dead helper's last frame: no process
// can create a flip swapchain on it any more (E_ACCESSDENIED, also the game's
// own D3D11) and a blt swapchain's presents stay hidden behind that frame
// (NOTES.md "Presenter helper"). A helper that exits normally (stdin EOF)
// releases its swapchain and can be replaced (StopHelper + EnsureHelper).
static void HelperDied(device* device, const char* how)
{
   DWORD code = STILL_ACTIVE;
   GetExitCodeThread(hp.process, &code);
   Log("d9native: presenter helper %s (exit code %lu, error \"%.128s\"), "
       "frame %llu",
      how, code, hp.stats ? hp.stats->error : "", hp.frame);
   StopHelper();
   ++state.helper_deaths;
   FailOutput(device, "presenter helper died");
}

static bool EnsureHelper(device* device, swapchain* swapchain)
{
   if (!out.shared_rt.handle &&
       !(CreateShared(device, state.render_width, state.render_height) &&
          CreateCanvas(device, state.width, state.height) &&
          CreateOverlay(device)))
   {
      StopHelper();
      FailOutput(device, "shared texture / canvas / overlay creation failed");
      return false;
   }
   if (!hp.process &&
       !StartHelper(device, static_cast<HWND>(swapchain->get_hwnd())))
   {
      HelperDied(device, "did not start");
      return false;
   }
   if (g_sr_upscaler >= 0 && hp.gpu_sync && !sr.process && !sr.gave_up)
   {
      d3d9_game->OnStartUpscaler(device);
   }
   if (sr.process && !sr.ready)
   {
      const uint64_t done = KmtCompleted(sr.out);
      if (done == UINT64_MAX)
      {
         Log("d9native: SR bridge exited before ready (see %ls)",
            SRBridgeProtocol::kLogName);
         StopUpscaler();
      }
      else if (done >= SRBridgeProtocol::kReady)
      {
         sr.ready = true;
         state.sr_ready = 1;
         Log("d9native: SR bridge ready");
      }
   }
   if (!hp.textures_sent)
   {
      char line[128];
      snprintf(
         line, sizeof(line), "textures %llu %llu %llu %llu\n",
         static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(hp.kmt[0])),
         static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(hp.kmt[1])),
         static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(hp.kmt[2])),
         static_cast<unsigned long long>(
            reinterpret_cast<uintptr_t>(hp.kmt[3])));
      hp.textures_sent = SendHelper(line);
   }
   return true;
}

// Lazily creates what the present needs, in an order that keeps failure before
// the first flip present harmless: device, shared texture, canvas, swapchain
// last. False = nothing to hand off this frame.
static bool EnsureOutput(device* device, swapchain* swapchain)
{
   return !g_output_failed && EnsureHelper(device, swapchain);
}

// P4 sampler upgrade: the DX11 mod's samplers_upgrade_mode 3 with
// force_upgrade_linear_samplers. ReShade's D3D9 SetSamplerState fires no event,
// so the native device's vtable entry is hooked (shared by every device of the
// class: only ours is changed). A pixel sampler whose MIN is ANISOTROPIC, or
// MIN and MAG LINEAR, with MIP LINEAR gets MIN ANISOTROPIC, MAXANISOTROPY 16
// and, where the game's bias is 0, the SR's (SR::GetMipLODBias at 1x: -1, 0
// for XeSS). The game's values are shadowed so a later change restores them.
// ponytail: state blocks (Apply) bypass the hook, and MAXMIPLEVEL is not
// lowered as Core does MinLOD; add both if the game uses them.
struct SamplerShadow
{
   DWORD min = D3DTEXF_POINT, mag = D3DTEXF_POINT, mip = D3DTEXF_NONE,
         anisotropy = 1, bias = 0; // D3D9 defaults, also after Reset
};
using SetSamplerStateFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*,
   DWORD,
   D3DSAMPLERSTATETYPE,
   DWORD);
static struct
{
   IDirect3DDevice9* native = nullptr;
   SetSamplerStateFn original = nullptr;
   std::array<SamplerShadow, 16> shadow;
   float bias = 0;
} samplers;

static void SamplerApply(IDirect3DDevice9* native, DWORD sampler)
{
   const SamplerShadow& game = samplers.shadow[sampler];
   const bool upgrade =
      !Mutated(MUT_NO_SAMPLER_UPGRADE) && game.mip == D3DTEXF_LINEAR &&
      (game.min == D3DTEXF_ANISOTROPIC ||
         (game.min == D3DTEXF_LINEAR && game.mag == D3DTEXF_LINEAR));
   float bias;
   memcpy(&bias, &game.bias, sizeof(bias));
   const float upgraded_bias = (bias == 0.0f ? samplers.bias : bias);
   DWORD bias_bits;
   memcpy(&bias_bits, &upgraded_bias, sizeof(bias_bits));
   samplers.original(native, sampler, D3DSAMP_MINFILTER,
      upgrade ? D3DTEXF_ANISOTROPIC : game.min);
   samplers.original(native, sampler, D3DSAMP_MAXANISOTROPY,
      upgrade ? 16 : game.anisotropy);
   samplers.original(native, sampler, D3DSAMP_MIPMAPLODBIAS,
      upgrade ? bias_bits : game.bias);
}

static HRESULT STDMETHODCALLTYPE HookSetSamplerState(IDirect3DDevice9* self,
   DWORD sampler,
   D3DSAMPLERSTATETYPE type,
   DWORD value)
{
   const HRESULT hr = samplers.original(self, sampler, type, value);
   if (self != samplers.native || !state.sampler_upgrade ||
       sampler >= samplers.shadow.size() || FAILED(hr))
   {
      return hr;
   }
   SamplerShadow& game = samplers.shadow[sampler];
   switch (type)
   {
   case D3DSAMP_MINFILTER:
      game.min = value;
      break;
   case D3DSAMP_MAGFILTER:
      game.mag = value;
      break;
   case D3DSAMP_MIPFILTER:
      game.mip = value;
      break;
   case D3DSAMP_MAXANISOTROPY:
      game.anisotropy = value;
      break;
   case D3DSAMP_MIPMAPLODBIAS:
      game.bias = value;
      break;
   default:
      return hr;
   }
   SamplerApply(self, sampler);
   return hr;
}

// At present: the bias for the next frame from whether the SR ran in this one
static void SamplerUpdateBias(IDirect3DDevice9* native)
{
   const float bias = (native == samplers.native &&
                         d3d9_game->UpscaledLastFrame() &&
                         g_sr_upscaler != SRBridgeProtocol::kXess)
                         ? -1.0f
                         : 0.0f;
   state.lod_bias = bias;
   if (bias == samplers.bias || !state.sampler_upgrade || !samplers.original)
   {
      samplers.bias = bias;
      return;
   }
   samplers.bias = bias;
   for (DWORD sampler = 0; sampler < samplers.shadow.size(); ++sampler)
   {
      SamplerApply(native, sampler);
   }
}

static void TextureSize(device* device, uint64_t texture, float* xy)
{
   if (!texture)
   {
      return;
   }
   const resource_desc desc = device->get_resource_desc(resource{texture});
   xy[0] = float(desc.texture.width);
   xy[1] = float(desc.texture.height);
}

static void OnInitSwapchain(swapchain* swapchain, bool resize)
{
   device* device = swapchain->get_device();
   if (device->get_api() != device_api::d3d9 || !OurSwapchain(swapchain, true))
   {
      return;
   }
   if (!g_bound || Mutated(MUT_NO_GUARD))
   {
      g_device = device;
      g_swapchain = swapchain;
   }
   UpdateGeometry(swapchain);
   g_d3d9_latency_done = false;
   if (!resize)
   {
      return;
   }
   ++state.resets_seen;
   samplers.shadow = {};
   // The shared objects now: no bind event fires for the back buffer the game
   // may bind before its next present, so the canvas must already exist.
   if (g_bound && hp.process && !g_output_failed &&
       !(CreateShared(device, state.render_width, state.render_height) &&
          CreateCanvas(device, state.width, state.height) &&
          CreateOverlay(device)))
   {
      FailOutput(device, "shared texture / canvas creation failed after Reset");
   }
}

static void OnDestroySwapchain(swapchain* swapchain, bool resize)
{
   device* device = swapchain->get_device();
   if (device->get_api() != device_api::d3d9 ||
       !OurSwapchain(swapchain, false))
   {
      return;
   }
   DestroyShared(device);
   d3d9_game->OnDestroySwapchain(device, resize);
   out.back_buffer = {0};
   if (!resize)
   {
      StopHelper();
      PassStop(device);
      g_pass.gave_up = false;
      Free(&g_factory);
      g_bound = g_output_failed = false;
      g_device = nullptr;
      g_swapchain = nullptr;
      state.output_failed = 0;
   }
}

static void OnInitDevice(device* device)
{
   if (device->get_api() != device_api::d3d9)
   {
      return;
   }
   KmtEndD3D9Device(device);
   samplers.native = reinterpret_cast<IDirect3DDevice9*>(device->get_native());
   samplers.shadow = {};
   samplers.bias = 0;
   if (!state.sampler_upgrade || samplers.original)
   {
      return;
   }
   void* target = (*reinterpret_cast<void***>(samplers.native))[69];
   if (!InitializeMinHook() ||
       MH_CreateHook(target, reinterpret_cast<void*>(&HookSetSamplerState),
          reinterpret_cast<void**>(&samplers.original)) != MH_OK ||
       MH_EnableHook(target) != MH_OK)
   {
      Log("d9native: SetSamplerState hook failed: no sampler upgrade");
      state.sampler_upgrade = 0;
   }
}

#if DEVELOPMENT
// (5) Replace one pixel shader by CRC32 of its SM3 bytecode (D9N_PS_CRC, hex,
// set by the test).
// Replacement list: env D9N_PS_CRC (legacy, replace_ps.ps_3_0.cso), env
// D9N_REPLACE ("CRC=file.cso;CRC=file.cso") and the file D9N_REPLACE_FILE
// (lines "CRC file.cso"); relative names are looked up next to the add-on.
// Hand-off draw shaders: built-in list plus env D9N_HANDOFF_PS ("CRC,CRC").
static void AddReplacement(const std::string& crc_text,
   const std::string& name)
{
   const uint32_t crc = strtoul(crc_text.c_str(), nullptr, 16);
   if (!crc || out.replacements.contains(crc))
   {
      return;
   }
   std::vector<char> bytes;
   if (name.find(':') != std::string::npos)
   {
      std::ifstream file(name, std::ios::binary);
      bytes.assign(std::istreambuf_iterator<char>(file), {});
   }
   else
   {
      bytes = ReadModuleFile(std::wstring(name.begin(), name.end()).c_str());
   }
   if (!bytes.empty())
   {
      Log("d9native: replacement %08X -> %s loaded (%zu bytes)", crc,
         name.c_str(), bytes.size());
      out.replacements[crc] = {bytes.begin(), bytes.end()};
   }
   else if (obs.logged_failures.insert(crc_text + "=" + name).second)
   {
      Log("d9native: replacement %08X -> %s FAILED to load (missing or empty "
          "file)",
         crc, name.c_str());
   }
}

static void LoadReplacementLists()
{
   char buffer[2048] = "";
   if (GetEnvironmentVariableA("D9N_PS_CRC", buffer, 16) && buffer[0])
   {
      AddReplacement(buffer, "replace_ps.ps_3_0.cso");
   }
   if (GetEnvironmentVariableA("D9N_REPLACE", buffer, sizeof(buffer)))
   {
      std::string list = buffer;
      for (size_t start = 0; start < list.size();)
      {
         const size_t end = std::min(list.find(';', start), list.size());
         const std::string entry = list.substr(start, end - start);
         if (const size_t equals = entry.find('='); equals != std::string::npos)
         {
            AddReplacement(entry.substr(0, equals), entry.substr(equals + 1));
         }
         start = end + 1;
      }
   }
   if (GetEnvironmentVariableA("D9N_REPLACE_FILE", buffer, MAX_PATH))
   {
      std::ifstream file(buffer);
      std::string crc_text, name;
      while (file >> crc_text >> name)
      {
         AddReplacement(crc_text, name);
      }
   }
   const auto add_crcs = [&](const char* name, std::set<uint32_t>* crcs)
   {
      if (!GetEnvironmentVariableA(name, buffer, sizeof(buffer)))
      {
         return;
      }
      const std::string list = buffer;
      for (size_t start = 0; start < list.size();)
      {
         const size_t end = std::min(list.find(',', start), list.size());
         crcs->insert(
            strtoul(list.substr(start, end - start).c_str(), nullptr, 16));
         start = end + 1;
      }
   };
   add_crcs("D9N_HANDOFF_PS", &out.handoff_crcs);
   add_crcs("D9N_VELOCITY_PS", &out.velocity_crcs);
   for (const auto& [name, crcs] : development_crc_lists)
   {
      add_crcs(name, crcs);
   }
   if (out.handoff_crcs.size() != obs.logged_handoff_count)
   {
      obs.logged_handoff_count = out.handoff_crcs.size();
      char crcs[256] = "";
      for (uint32_t crc : out.handoff_crcs)
      {
         sprintf_s(crcs + strlen(crcs), sizeof(crcs) - strlen(crcs), "%08X ", crc);
      }
      Log("d9native: hand-off draw PS CRCs: %s", crcs);
   }
}

#endif
// (1) D3D9Ex upgrade.
static bool OnCreateDevice(device_api api, uint32_t& api_version)
{
   if (api != device_api::d3d9)
   {
      return false;
   }
   api_version = 0x9100;
   // GPU hand-off sync: the D3D9 UMD's kernel queue is only seen while the
   // device is created, so the hooks go in now. On failure the device has no
   // generation: E0.
   if (!KmtBeginD3D9Device())
   {
      Log("d9native: KMT hooks on win32u failed, staying on E0");
   }
   // DEVELOPMENT env: D9N_LUMA="x,y,z,w,..." (settings rows c208.. in order, up to 4 * INJECT_REGISTER_COUNT floats; the game
   // rewrites them per present outside the harness)
   char text[512] = "";
   if (DevelopmentEnv("D9N_LUMA", text, sizeof(text)))
   {
      char* context = nullptr;
      int index = 0;
      for (char* token = strtok_s(text, ",", &context);
         token && index < int(4 * INJECT_REGISTER_COUNT);
         token = strtok_s(nullptr, ",", &context))
      {
         state.injected[index++] = static_cast<float>(atof(token));
      }
   }
   if (DevelopmentEnv("D9N_DUMP_EVERY_S", text, sizeof(text)))
   {
      obs.dump_every_s = atof(text);
   }
   // Game-run only: the game's scene texture is gamma encoded (R'G'B'); 0 =
   // treat as linear (tests).
   if (ReadSetting("DECODE_GAMMA", text, sizeof(text)))
   {
      g_decode_gamma = static_cast<float>(atof(text));
      Log("d9native: composition decodes the hand-off with gamma %g",
         g_decode_gamma);
   }
   if (ReadSetting("OUTPUT", text, sizeof(text)))
   {
      state.output_mode_request = !_stricmp(text, "hdr10")  ? OUTPUT_HDR10
                                  : !_stricmp(text, "srgb") ? OUTPUT_SRGB
                                                            : OUTPUT_SCRGB;
   }
   if (ReadSetting("HDR_FALLBACK", text, sizeof(text)))
   {
      state.hdr_fallback = atoi(text) != 0;
   }
   if (ReadSetting("LATENCY", text, sizeof(text)))
   {
      state.latency_enabled = atoi(text) != 0;
   }
   if (ReadSetting("SYNC_INTERVAL", text, sizeof(text)))
   {
      state.sync_interval = std::clamp(atoi(text), 0, 4);
   }
   if (ReadSetting("FORCE_IMMEDIATE", text, sizeof(text)))
   {
      state.force_immediate = atoi(text) != 0;
   }
   Log("d9native: output mode %d (D9N_OUTPUT), hdr_fallback=%d, latency=%d "
       "(D9N_LATENCY), output sync interval %d, force_immediate=%d",
      state.output_mode_request, state.hdr_fallback, state.latency_enabled,
      state.sync_interval, state.force_immediate);
   if (ReadSetting("RCAS", text, sizeof(text)))
   {
      state.rcas_sharpness = static_cast<float>(atof(text));
   }
   if (ReadSetting("SMAA", text, sizeof(text)))
   {
      state.smaa = atoi(text) != 0;
   }
   if (ReadSetting("SMAA_PREDICATION", text, sizeof(text)))
   {
      state.smaa_predication = atoi(text) != 0;
   }
   if (ReadSetting("SMAA_T2X", text, sizeof(text)))
   {
      state.smaa_t2x = atoi(text) != 0;
   }
   if (ReadSetting("LUMA_TAA", text, sizeof(text)))
   {
      state.luma_taa = atoi(text) != 0;
   }
   // ResetRender display map and the present clamp; DEVELOPMENT env D9N_PAPER_WHITE (scRGB scale of the composition) over the UI paper
   // white's
   LoadDisplaySettings();
   if (DevelopmentEnv("D9N_PAPER_WHITE", text, sizeof(text)))
   {
      state.paper_white = static_cast<float>(atof(text));
   }
   if (ReadSetting("DISPLAY_MAP", text, sizeof(text)))
   {
      state.display_map = atoi(text) != 0;
   }
   if (ReadSetting("PRESENT_CLAMP", text, sizeof(text)))
   {
      state.present_clamp = atoi(text) != 0;
   }
   if (ReadSetting("DITHER", text, sizeof(text)))
   {
      state.dither = atoi(text) != 0;
   }
   state.sampler_upgrade = state.output_clamp = 1;
   if (ReadSetting("OUTPUT_CLAMP", text, sizeof(text)))
   {
      state.output_clamp = atoi(text) != 0;
   }
   if (ReadSetting("SAMPLER_UPGRADE", text, sizeof(text)))
   {
      state.sampler_upgrade = atoi(text) != 0;
   }
   if (DevelopmentEnv("D9N_FORCE_FAIL", text, sizeof(text)))
   {
      g_force_canvas_fail = !strcmp(text, "canvas");
      Log("d9native: D9N_FORCE_FAIL=%s (test knob)", text);
   }
   d3d9_game->OnCreateDevice();
#if DEVELOPMENT
   LoadReplacementLists();
#endif
   if (!out.shaders_scanned)
   {
      out.shaders_scanned = true;
      ScanReplacementShaders();
   }
   Log("d9native: create_device D3D9 -> D3D9Ex. luma=(%g %g %g %g) "
       "paper_white=%g dump_every_s=%g (F10 = one-shot dump)",
      state.injected[0], state.injected[1], state.injected[2],
      state.injected[3], state.paper_white, obs.dump_every_s);
#if DEVELOPMENT
   for (const char* name :
      {"D9N_PS_CRC", "D9N_REPLACE", "D9N_REPLACE_FILE", "D9N_HANDOFF_PS",
         "D9N_RENDER_SIZE", "D9N_INJECT_BASE", "D9N_LUMA", "D9N_PAPER_WHITE",
         "D9N_DUMP_EVERY_S", "D9N_MIN_BB", "D9N_OUTPUT"})
   {
      char value[1024] = "";
      if (GetEnvironmentVariableA(name, value, sizeof(value)))
      {
         Log("d9native: env %s=%s", name, value);
      }
   }
#endif
   return true;
}

// Forced windowed (exclusive fullscreen is not supported with the output
// device).
// With D9N_MIN_BB=1 the D3D9 back buffer is shrunk to 1x1 (the game renders
// into its own RTs; the hand-off source is the upgraded RT), while
// state.width/height keep the size the game asked for.
static bool OnCreateSwapchain(device_api api, swapchain_desc& desc, void*)
{
   if (api != device_api::d3d9)
   {
      return false;
   }
   bool changed = false;
   // A game flip chain would be a second flip chain on the HWND (one per
   // window): force blt (PROXY_RESEARCH 1.12). The lockable back buffer flag
   // costs performance and is invalid with flip (D3DPRESENTFLAG docs).
   if (desc.present_mode == D3DSWAPEFFECT_FLIPEX)
   {
      desc.present_mode = D3DSWAPEFFECT_DISCARD;
      Log("d9native: game asked for D3DSWAPEFFECT_FLIPEX, rewritten to DISCARD");
      changed = true;
   }
   if (desc.present_flags & D3DPRESENTFLAG_LOCKABLE_BACKBUFFER)
   {
      desc.present_flags &=
         ~static_cast<uint32_t>(D3DPRESENTFLAG_LOCKABLE_BACKBUFFER);
      changed = true;
   }
   // One sync owner: the output chain paces (D9N_SYNC_INTERVAL); with
   // D9N_FORCE_IMMEDIATE=1 the game's D3D9 Present stops waiting for vblank.
   // Default keeps the game's interval (a game may use Present blocking as its
   // frame cap).
   if (state.force_immediate && !Mutated(MUT_NO_FORCE_IMMEDIATE) &&
       desc.sync_interval != 0)
   {
      desc.sync_interval = 0;
      ++state.game_interval_forced;
      changed = true;
   }
   // ReShade's GUI is copied out of the back buffer with its alpha (the overlay
   // coverage), which X8R8G8B8 does not keep.
   if (desc.back_buffer.texture.format == format::b8g8r8x8_unorm)
   {
      desc.back_buffer.texture.format = format::b8g8r8a8_unorm;
      changed = true;
   }
   if (desc.fullscreen_state)
   {
      desc.fullscreen_state = false;
      desc.fullscreen_refresh_rate = 0;
      state.windowed_forced = 1;
      changed = true;
   }
   if (DevelopmentEnv(L"D9N_MIN_BB", nullptr, 0) != 0)
   {
      g_logical_width = desc.back_buffer.texture.width;
      g_logical_height = desc.back_buffer.texture.height;
      desc.back_buffer.texture.width = desc.back_buffer.texture.height = 1;
      state.min_back_buffer = 1;
      changed = true;
   }
   return changed;
}

static bool OnCreatePipeline(device* device, pipeline_layout, uint32_t count,
   const pipeline_subobject* subobjects)
{
   if (device->get_api() != device_api::d3d9)
   {
      return false;
   }
#if DEVELOPMENT
   LoadReplacementLists();
#endif
   out.pending_crc = 0;
   out.pending_replaced = false;
   bool clamped = false;
   for (uint32_t i = 0; i < count; ++i)
   {
      if (subobjects[i].type == pipeline_subobject_type::vertex_shader)
      {
         const auto* vertex = static_cast<const shader_desc*>(subobjects[i].data);
#if DEVELOPMENT
         RecordSeenShader(false, Crc32(vertex->code, vertex->code_size),
            vertex->code, vertex->code_size);
#endif
         continue;
      }
      if (subobjects[i].type != pipeline_subobject_type::pixel_shader)
      {
         continue;
      }
      auto* shader = static_cast<shader_desc*>(subobjects[i].data);
      const uint32_t crc = Crc32(shader->code, shader->code_size);
#if DEVELOPMENT
      RecordSeenShader(true, crc, shader->code, shader->code_size);
#endif
      out.pending_crc = crc;
      if (state.seen_ps_count < 16)
      {
         state.seen_ps_crc[state.seen_ps_count++] = crc;
      }
      const auto replacement = out.replacements.find(crc);
      if (replacement != out.replacements.end())
      {
         shader->code = replacement->second.data();
         shader->code_size = replacement->second.size();
         out.pending_replaced = true;
         continue;
      }
      if (!state.output_clamp || Mutated(MUT_NO_OUTPUT_CLAMP))
      {
         continue;
      }
      auto [patch, inserted] = out.clamped_ps.try_emplace(crc);
      if (inserted)
      {
         const bool velocity = out.velocity_crcs.contains(crc);
         std::string error;
         if (Sm3MvPatch::PatchOutputClamp(
                static_cast<const uint32_t*>(shader->code),
                shader->code_size / 4,
                velocity ? Sm3MvPatch::OutputClamp::Unorm8
                         : Sm3MvPatch::OutputClamp::Blend,
                &patch->second, &error))
         {
            ++(velocity ? state.ps_clamped_velocity : state.ps_clamped_blend);
         }
         else
         {
            patch->second.clear();
            ++state.ps_clamp_refused;
            Log("d9native: PS %08X keeps its output unclamped: %s", crc,
               error.c_str());
         }
      }
      if (!patch->second.empty())
      {
         shader->code = patch->second.data();
         shader->code_size = patch->second.size() * sizeof(uint32_t);
         clamped = true;
      }
   }
   return out.pending_replaced || clamped;
}

static void OnInitPipeline(device* device, pipeline_layout, uint32_t count,
   const pipeline_subobject* subobjects,
   pipeline pipeline)
{
   if (device->get_api() != device_api::d3d9)
   {
      return;
   }
   out.pipeline_crc[pipeline.handle] = out.pending_crc;
   d3d9_game->OnInitPipeline(device, count, subobjects, pipeline);
   if (out.pending_replaced)
   {
      out.matched_pipelines.insert(pipeline.handle);
      ++state.ps_replaced;
      ++obs.replaced_creates[out.pending_crc];
   }
}

static void OnBindPipeline(command_list* cmd_list, pipeline_stage stages,
   pipeline pipeline)
{
   if (cmd_list->get_device()->get_api() != device_api::d3d9)
   {
      return;
   }
   d3d9_game->OnBindPipeline(stages, pipeline);
   if ((stages & pipeline_stage::pixel_shader) != 0)
   {
      out.matched_bound = out.matched_pipelines.contains(pipeline.handle);
      const auto crc = out.pipeline_crc.find(pipeline.handle);
      out.bound_ps_crc = crc == out.pipeline_crc.end() ? 0 : crc->second;
   }
}

// RT0 is the real back buffer without any bind event (device creation, Reset):
// rebind it to the canvas natively.
static bool RewriteImplicitRt0(device* device)
{
   out.implicit_rt0_pending = false;
   if (!Redirecting() || !out.canvas_surface || Mutated(MUT_NO_IMPLICIT_RT0))
   {
      return false;
   }
   auto* native = reinterpret_cast<IDirect3DDevice9*>(device->get_native());
   IDirect3DSurface9 *render_target = nullptr, *real_back_buffer = nullptr;
   const bool rewrite = SUCCEEDED(native->GetRenderTarget(0, &render_target)) &&
                        SUCCEEDED(native->GetBackBuffer(
                           0, 0, D3DBACKBUFFER_TYPE_MONO, &real_back_buffer)) &&
                        render_target == real_back_buffer;
   if (rewrite)
   {
      native->SetRenderTarget(0, out.canvas_surface);
      out.redirect_used = true;
      ++state.implicit_rt0_rewrites;
   }
   Free(&render_target);
   Free(&real_back_buffer);
   return rewrite;
}

static void OnBindRenderTargets(command_list* cmd_list, uint32_t count,
   const resource_view* rtvs,
   resource_view depth_stencil)
{
   device* device = cmd_list->get_device();
   if (!OurDevice(device))
   {
      return;
   }
   d3d9_game->OnBindRenderTargets(device, count, rtvs, depth_stencil);
   out.implicit_rt0_pending = false; // the game binds explicitly
   const resource rt0 = count > 0 && rtvs[0].handle
                           ? device->get_resource_from_view(rtvs[0])
                           : resource{0};
   out.rt0_is_back_buffer = rt0.handle && rt0.handle == out.back_buffer.handle;
   out.rt0 = rt0;
   if (rt0.handle)
   {
      const resource_desc desc = device->get_resource_desc(rt0);
      out.rt0_width = desc.texture.width;
      out.rt0_height = desc.texture.height;
   }
   state.rt0_back_buffer_binds += out.rt0_is_back_buffer;
   // The game re-binds the canvas it got from GetRenderTarget (save/restore
   // idiom): already redirected.
   if (Redirecting() && out.canvas_surface && !Mutated(MUT_NO_STICKY) &&
       count > 0 &&
       (rtvs[0].handle == reinterpret_cast<uintptr_t>(out.canvas_surface) ||
          (rt0.handle && rt0.handle == out.canvas.handle)))
   {
      out.redirect_used = true;
      out.rt0_is_back_buffer = true;
      return;
   }
   // Back-buffer redirect: rebind RT0 to the fp16 canvas natively (no ReShade
   // wrapper, so no event recursion; D3D9 resets the viewport on SetRenderTarget
   // exactly as it just did for the back buffer).
   if (out.rt0_is_back_buffer && Redirecting() && out.canvas_surface)
   {
      reinterpret_cast<IDirect3DDevice9*>(device->get_native())
         ->SetRenderTarget(0, out.canvas_surface);
      out.redirect_used = true;
      ++state.redirected_binds;
      if (!out.redirect_noted)
      {
         out.redirect_noted = true;
         Log("d9native: back buffer redirected to the canvas. RISK: "
             "GetRenderTarget(0)/GetBackBuffer called by the game now see the "
             "canvas / "
             "the real back buffer respectively and cannot be intercepted from an "
             "add-on");
      }
   }
}

// D3D9 SetTexture: pixel stage, layout param 1, binding = sampler stage; the
// view handle is the texture pointer.
static void OnPushDescriptors(command_list* cmd_list, shader_stage stages,
   pipeline_layout, uint32_t layout_param,
   const descriptor_table_update& update)
{
   if (cmd_list->get_device()->get_api() != device_api::d3d9 ||
       stages != shader_stage::pixel || layout_param != 1 ||
       update.type != descriptor_type::sampler_with_resource_view)
   {
      return;
   }
   const auto* descriptors =
      static_cast<const sampler_with_resource_view*>(update.descriptors);
   d3d9_game->OnBindPixelTextures(update.binding, update.count);
   for (uint32_t i = 0;
      i < update.count && update.binding + i < std::size(out.ps_textures);
      ++i)
   {
      out.ps_textures[update.binding + i] = descriptors[i].view.handle;
   }
}

// The game's SetVertexShaderConstantF / SetPixelShaderConstantF (fired after
// the call; the add-on's own push_constants go to the device directly and fire
// nothing).
static void OnPushConstants(command_list* cmd_list, shader_stage stages,
   pipeline_layout, uint32_t layout_param,
   uint32_t first, uint32_t count,
   const void* values)
{
   if (cmd_list->get_device()->get_api() == device_api::d3d9)
   {
      d3d9_game->OnPushConstants(stages, layout_param, first, count, values);
   }
}

// Injection: pixel stage, layout param 5 (PS float constants), registers
// c<inject_base>.. (first/count are in floats), at the draws of a replaced PS.
static bool DrawCommon(command_list* cmd_list, uint32_t vertex_count,
   uint32_t first_vertex, int32_t vertex_offset)
{
   device* device = cmd_list->get_device();
   if (!OurDevice(device))
   {
      return false;
   }
   FlushCanvasDraw(device);
   d3d9_game->OnDrawBegin(device, vertex_count, first_vertex, vertex_offset);
   if (out.implicit_rt0_pending)
   {
      RewriteImplicitRt0(device);
   }
   if (out.matched_bound)
   {
      float rows[4 * INJECT_REGISTER_COUNT];
      memcpy(rows, state.injected, sizeof(rows));
      d3d9_game->OnInjectConstants(device, rows);
      cmd_list->push_constants(shader_stage::pixel, pipeline_layout{0}, 5,
         state.inject_base * 4, INJECT_REGISTER_COUNT * 4,
         rows);
      ++state.injected_draws;
      ++obs.matched_draws[out.bound_ps_crc];
   }
   if (d3d9_game->OnDraw(cmd_list))
   {
      return true;
   }
   // Copy draw into the back buffer (the game's ResetRender): its s0 texture is
   // the scene.
   if (out.rt0_is_back_buffer && out.ps_textures[0] &&
       out.handoff_crcs.contains(out.bound_ps_crc))
   {
      out.draw_source = {out.ps_textures[0]};
      NativeOf(device)->GetViewport(&out.scene_rect);
      // ResetRender is the frame's first copy into the canvas
      if (Redirecting() && out.last_reset_render != state.presents)
      {
         out.last_reset_render = state.presents;
         out.reset_render_pending = true;
      }
   }
   else if (out.rt0_is_back_buffer && Redirecting() &&
            out.video_crcs.contains(out.bound_ps_crc))
   {
      out.video_pending = true;
   }
   return false;
}

static bool OnDraw(command_list* cmd_list, uint32_t vertex_count, uint32_t,
   uint32_t first_vertex, uint32_t)
{
   return DrawCommon(cmd_list, vertex_count, first_vertex, 0);
}

static bool OnDrawIndexed(command_list* cmd_list, uint32_t index_count,
   uint32_t, uint32_t first_index, int32_t vertex_offset,
   uint32_t)
{
   return DrawCommon(cmd_list, index_count, first_index, vertex_offset);
}

static void OnDestroyPipeline(device* device, pipeline pipeline)
{
   if (device->get_api() == device_api::d3d9)
   {
      d3d9_game->OnDestroyPipeline(pipeline.handle);
   }
}

// A clear with our RT1 still bound would wipe the motion target too.
static bool OnClearDepthStencil(command_list* cmd_list, resource_view,
   const float*, const uint8_t*, uint32_t,
   const rect*)
{
   if (OurDevice(cmd_list->get_device()))
   {
      d3d9_game->OnClear(NativeOf(cmd_list->get_device()));
   }
   return false;
}

// (6) Upgrade A8R8G8B8 render targets of the render size, /2 and /4 to
// A16B16G16R16F (never R32F, depth or fixed-size small RTs).
static bool OnCreateResource(device* device, resource_desc& desc,
   subresource_data*, resource_usage)
{
   if (device->get_api() != device_api::d3d9 ||
       desc.type != resource_type::texture_2d ||
       (desc.usage & resource_usage::render_target) == 0 ||
       desc.texture.format != format::b8g8r8a8_unorm)
   {
      return false;
   }
   bool size_ok = false;
   for (uint32_t divisor : {1u, 2u, 4u})
   {
      size_ok |= desc.texture.width == state.render_width / divisor &&
                 desc.texture.height == state.render_height / divisor;
   }
   if (!size_ok)
   {
      ++state.rt_skipped;
      return false;
   }
   desc.texture.format = format::r16g16b16a16_float;
   out.upgrade_pending = true;
   ++state.rt_upgraded;
   ++obs.rt_sizes[(desc.texture.width << 16) | desc.texture.height];
   return true;
}

static void OnInitResource(device* device, const resource_desc&,
   const subresource_data*, resource_usage,
   resource resource)
{
   if (device->get_api() == device_api::d3d9 && out.upgrade_pending)
   {
      out.upgraded.insert(resource.handle);
      out.upgrade_pending = false;
   }
}

static void OnDestroyResource(device* device, resource resource)
{
   if (device->get_api() == device_api::d3d9)
   {
      d3d9_game->OnDestroyResource(resource.handle);
      out.upgraded.erase(resource.handle);
      if (out.final_source.handle == resource.handle)
      {
         out.final_source = {0};
      }
      if (out.draw_source.handle == resource.handle)
      {
         out.draw_source = {0};
      }
      for (uint64_t& texture : out.ps_textures)
      {
         if (texture == resource.handle)
         {
            texture = 0;
         }
      }
   }
}

// The 8-bit back buffer would clamp HDR: remember which upgraded RT the game
// copies into it and hand that one off.
// The game copies regions OUT of the back buffer into its own textures (menu
// boxes etc.): with the redirect the real back buffer is empty, so those copies
// must read the canvas. Logs the format conversion once.
static void NoteCopyFromBackBuffer(device* device, resource destination)
{
   static bool noted = false;
   ++state.copies_from_bb;
   if (noted)
   {
      return;
   }
   noted = true;
   const resource_desc dest_desc = device->get_resource_desc(destination);
   const D3DFORMAT dest_format =
      dest_desc.texture.format == format::b8g8r8a8_unorm   ? D3DFMT_A8R8G8B8
      : dest_desc.texture.format == format::b8g8r8x8_unorm ? D3DFMT_X8R8G8B8
                                                           : D3DFMT_UNKNOWN;
   IDirect3D9* d3d = nullptr;
   HRESULT conversion = E_NOTIMPL;
   if (dest_format != D3DFMT_UNKNOWN &&
       SUCCEEDED(reinterpret_cast<IDirect3DDevice9*>(device->get_native())
             ->GetDirect3D(&d3d)))
   {
      conversion = d3d->CheckDeviceFormatConversion(
         D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, D3DFMT_A16B16G16R16F, dest_format);
      d3d->Release();
   }
   Log("d9native: first copy out of the back buffer redirected to the canvas "
       "(A16B16G16R16F -> reshade format %d / D3DFMT %d, "
       "CheckDeviceFormatConversion 0x%08X; the StretchRect result itself is "
       "not observable from the event)",
      static_cast<int>(dest_desc.texture.format), static_cast<int>(dest_format),
      static_cast<uint32_t>(conversion));
}

static bool OnCopyTextureRegion(command_list* cmd_list, resource source,
   uint32_t source_subresource,
   const subresource_box* source_box,
   resource destination,
   uint32_t destination_subresource,
   const subresource_box* destination_box,
   filter_mode filter)
{
   device* device = cmd_list->get_device();
   if (!OurDevice(device))
   {
      return false;
   }
   FlushCanvasDraw(device);
   d3d9_game->OnCopyTextureRegion(device, source, destination, source_box,
      destination_box);
   const bool from_back_buffer = source.handle == out.back_buffer.handle,
              to_back_buffer = destination.handle == out.back_buffer.handle;
   if (!from_back_buffer && !to_back_buffer)
   {
      return false;
   }
   if (Redirecting() && out.canvas.handle)
   {
      const bool redirect_source = from_back_buffer && state.bb_src_redirect;
      if (redirect_source || to_back_buffer)
      {
         if (redirect_source)
         {
            NoteCopyFromBackBuffer(device, destination);
         }
         cmd_list->copy_texture_region(
            redirect_source ? out.canvas : source, source_subresource, source_box,
            to_back_buffer ? out.canvas : destination, destination_subresource,
            destination_box, filter);
         if (to_back_buffer)
         {
            out.redirect_used = true;
            ++state.redirected_copies;
         }
         return true;
      }
   }
   if (to_back_buffer && out.upgraded.contains(source.handle))
   {
      out.final_source = source;
   }
   return false;
}

static bool OnCopyResource(command_list* cmd_list, resource source,
   resource destination)
{
   device* device = cmd_list->get_device();
   if (!OurDevice(device) || !Redirecting() || !out.canvas.handle)
   {
      return false;
   }
   FlushCanvasDraw(device);
   const bool from_back_buffer = source.handle == out.back_buffer.handle &&
                                 state.bb_src_redirect,
              to_back_buffer = destination.handle == out.back_buffer.handle;
   if (!from_back_buffer && !to_back_buffer)
   {
      return false;
   }
   if (from_back_buffer)
   {
      NoteCopyFromBackBuffer(device, destination);
   }
   cmd_list->copy_resource(from_back_buffer ? out.canvas : source,
      to_back_buffer ? out.canvas : destination);
   if (to_back_buffer)
   {
      out.redirect_used = true;
      ++state.redirected_copies;
   }
   return true;
}

// ColorFill of the back buffer -> the canvas (Clear of a bound RT already hits
// the canvas).
static bool OnClearRenderTarget(command_list* cmd_list, resource_view rtv,
   const float color[4], uint32_t rect_count,
   const rect* rects)
{
   device* device = cmd_list->get_device();
   if (OurDevice(device))
   {
      d3d9_game->OnClear(NativeOf(device));
      FlushCanvasDraw(device);
   }
   if (!OurDevice(device) || !Redirecting() || !out.canvas_surface ||
       !rtv.handle ||
       device->get_resource_from_view(rtv).handle != out.back_buffer.handle)
   {
      return false;
   }
   if (out.implicit_rt0_pending)
   {
      RewriteImplicitRt0(device);
   }
   cmd_list->clear_render_target_view(
      resource_view{reinterpret_cast<uintptr_t>(out.canvas_surface)}, color,
      rect_count, rects);
   out.redirect_used = true;
   ++state.redirected_clears;
   return true;
}

// StretchRect from a multisampled surface into the back buffer (a resolve).
// With the redirect the back buffer is empty: resolve into the real back buffer
// as the game asked, then copy that region into the canvas (an fp16 source
// would resolve straight into the canvas, but the add-on only upgrades
// textures, and D3D9 MSAA surfaces are never textures).
static bool OnResolveTextureRegion(
   command_list* cmd_list, resource source, uint32_t source_subresource,
   const subresource_box* source_box, resource destination,
   uint32_t destination_subresource, uint32_t destination_x,
   uint32_t destination_y, uint32_t destination_z, format resolve_format)
{
   device* device = cmd_list->get_device();
   // ReShade raises this event only for a multisampled source: the game's MSAA
   // resolves its fp16 scene
   if (OurDevice(device) && device->get_resource_desc(source).texture.format ==
                               format::r16g16b16a16_float)
   {
      out.msaa_resolved = true;
   }
   if (!OurDevice(device) || !Redirecting() || !out.canvas.handle ||
       Mutated(MUT_NO_RESOLVE) || destination.handle != out.back_buffer.handle)
   {
      return false;
   }
   FlushCanvasDraw(device);
   const resource_desc source_desc = device->get_resource_desc(source);
   const uint32_t width = source_box ? source_box->right - source_box->left
                                     : source_desc.texture.width,
                  height = source_box ? source_box->bottom - source_box->top
                                      : source_desc.texture.height;
   cmd_list->resolve_texture_region(source, source_subresource, source_box,
      destination, destination_subresource,
      destination_x, destination_y, destination_z,
      resolve_format);
   const subresource_box region = {static_cast<uint32_t>(destination_x),
      static_cast<uint32_t>(destination_y),
      0,
      static_cast<uint32_t>(destination_x + width),
      static_cast<uint32_t>(destination_y + height),
      1};
   cmd_list->copy_texture_region(destination, 0, &region, out.canvas, 0, &region,
      filter_mode::min_mag_mip_point);
   out.redirect_used = true;
   ++state.redirected_copies;
   ++state.resolves_redirected;
   return true;
}

#if DEVELOPMENT
// Read the hand-off texture (canvas or shared scene RT, fp16) back through a
// SYSTEMMEM surface and write it raw (RGBA16F, tight rows) + .json + a stats
// line. Stalls the game thread: debugging only (F10 / D9N_DUMP_EVERY_S).
static void DumpHandoff(IDirect3DDevice9* native, const char* reason)
{
   const resource source = out.handoff_canvas ? out.canvas : out.shared_rt;
   IDirect3DSurface9 *surface = nullptr, *readback = nullptr;
   D3DSURFACE_DESC desc = {};
   D3DLOCKED_RECT mapped = {};
   const bool ok =
      source.handle &&
      SUCCEEDED(reinterpret_cast<IDirect3DTexture9*>(source.handle)
            ->GetSurfaceLevel(0, &surface)) &&
      SUCCEEDED(surface->GetDesc(&desc)) &&
      SUCCEEDED(native->CreateOffscreenPlainSurface(
         desc.Width, desc.Height, desc.Format, D3DPOOL_SYSTEMMEM, &readback,
         nullptr)) &&
      SUCCEEDED(native->GetRenderTargetData(surface, readback)) &&
      SUCCEEDED(readback->LockRect(&mapped, nullptr, D3DLOCK_READONLY));
   Free(&surface);
   if (!ok)
   {
      Free(&readback);
      Log("d9native: dump (%s) failed", reason);
      return;
   }
   const uint32_t number = ++obs.dump_count;
   wchar_t name[64];
   swprintf_s(name, L"d9native_dump_%u.bin", number);
   FILE* bin = nullptr;
   _wfopen_s(&bin, ModuleFilePath(name).c_str(), L"wb");
   double sum[4] = {}, min_value[4] = {1e30, 1e30, 1e30, 1e30},
          max_value[4] = {-1e30, -1e30, -1e30, -1e30};
   uint64_t above_one = 0, non_finite = 0;
   for (UINT y = 0; y < desc.Height; ++y)
   {
      const auto* row = reinterpret_cast<const uint16_t*>(
         static_cast<const uint8_t*>(mapped.pBits) +
         static_cast<size_t>(y) * mapped.Pitch);
      if (bin)
      {
         fwrite(row, 8, desc.Width, bin);
      }
      for (UINT x = 0; x < desc.Width; ++x)
      {
         for (int c = 0; c < 4; ++c)
         {
            const float value = HalfToFloat(row[x * 4 + c]);
            if (!std::isfinite(value))
            {
               ++non_finite;
               continue;
            }
            sum[c] += value;
            min_value[c] = std::min<double>(min_value[c], value);
            max_value[c] = std::max<double>(max_value[c], value);
            above_one += c < 3 && value > 1.0f;
         }
      }
   }
   readback->UnlockRect();
   readback->Release();
   if (bin)
   {
      fclose(bin);
   }
   swprintf_s(name, L"d9native_dump_%u.json", number);
   FILE* json = nullptr;
   _wfopen_s(&json, ModuleFilePath(name).c_str(), L"w");
   if (json)
   {
      fprintf(json,
         "{\"width\":%u,\"height\":%u,\"format\":\"R16G16B16A16_FLOAT\","
         "\"bytes_per_pixel\":8,\"frame\":%u,\"reason\":\"%s\"}\n",
         desc.Width, desc.Height, obs.frames, reason);
      fclose(json);
   }
   const double pixels = static_cast<double>(desc.Width) * desc.Height;
   Log("d9native: dump #%u (%s) %ux%u frame %u: min/max/mean R %.4f/%.4f/%.4f G "
       "%.4f/%.4f/%.4f B %.4f/%.4f/%.4f A %.4f/%.4f/%.4f "
       "rgb_values>1.0=%llu nan_or_inf=%llu",
      number, reason, desc.Width, desc.Height, obs.frames, min_value[0],
      max_value[0], sum[0] / pixels, min_value[1], max_value[1],
      sum[1] / pixels, min_value[2], max_value[2], sum[2] / pixels,
      min_value[3], max_value[3], sum[3] / pixels, above_one, non_finite);
}

#endif
// Helper frame: copies ReShade's overlay out of the back buffer, drains the
// D3D9 queue (E0), sends the frame line and waits until the helper presented
// and released the textures (the game writes the canvas / shared RT again
// right after). ponytail: SwitchToThread spin, an event in the stats mapping
// if the spinning core matters.
static void SendHelperFrame(device* device)
{
   hp.pending = false;
   // Before any write: a hung thread stops reading and a full pipe would block
   // the game thread.
   if (HelperGone())
   {
      HelperDied(device, hp.hung ? "hung (watchdog)" : "exited");
      return;
   }
   auto* native = reinterpret_cast<IDirect3DDevice9*>(device->get_native());
   const bool overlay = g_runtime_d3d9 && hp.overlay.handle;
   if (overlay)
   {
      IDirect3DSurface9 *back_buffer = nullptr, *target = nullptr;
      if (SUCCEEDED(native->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO,
             &back_buffer)) &&
          SUCCEEDED(reinterpret_cast<IDirect3DTexture9*>(hp.overlay.handle)
                ->GetSurfaceLevel(0, &target)))
      {
         native->StretchRect(back_buffer, nullptr, target, nullptr, D3DTEXF_NONE);
      }
      Free(&target);
      Free(&back_buffer);
   }
   state.helper_overlay = overlay;
   const uint64_t n = hp.frame + 1;
   // GPU sync: one FLUSH submits the D3D9 work, then "in" n behind it. A failed
   // signal drops to E0 for good: drain, then the CPU signals "in" n so the
   // helper's GPU wait for this frame still passes.
   bool gpu = hp.gpu_sync;
   const double drain_start = NowMs();
   out.query9->Issue(D3DISSUE_END);
   HRESULT query_hr = out.query9->GetData(nullptr, 0, D3DGETDATA_FLUSH);
   if (gpu && SUCCEEDED(query_hr))
   {
      const bool forced_failure =
         state.fault_kmt_signal > 0 && state.fault_kmt_signal--; // test
      if (forced_failure || !KmtSignalObject(&hp.sync, hp.fence_in.object, n))
      {
         Log("d9native: presenter GPU sync: signal failed at %s (0x%08X), back to "
             "CPU waits",
            forced_failure ? "forced (test)" : hp.sync.first_error_call,
            static_cast<uint32_t>(hp.sync.first_error));
         hp.gpu_sync = gpu = false;
         state.e3_gave_up = 1;
      }
      else
      {
         ++state.e3_signals;
      }
   }
   while (!gpu && query_hr == S_FALSE)
   {
      YieldProcessor();
      query_hr = out.query9->GetData(nullptr, 0, D3DGETDATA_FLUSH);
   }
   if (!gpu && hp.fence_in.object)
   {
      KmtCpuSignal(&hp.sync, hp.fence_in, n);
   }
   const double drain_ms = NowMs() - drain_start;
   if (state.fault_query_hung > 0)
   {
      --state.fault_query_hung;
      query_hr = D3DERR_DEVICEHUNG;
   }
   if (FAILED(query_hr))
   {
      // D3DERR_DEVICEHUNG/REMOVED: the device's texture memory is lost (D3D9Ex:
      // ResetEx; driver stop: new IDirect3D9Ex), so the shared textures are gone
      // too. Free them; EnsureOutput recreates them at the next present.
      Log("d9native: D3D9 hand-off query failed 0x%08X, frame skipped",
         static_cast<uint32_t>(query_hr));
      if ((query_hr == D3DERR_DEVICEHUNG || query_hr == D3DERR_DEVICEREMOVED) &&
          !Mutated(MUT_NO_SHARED_LOSS))
      {
         ++state.shared_lost;
         DestroyShared(device);
      }
      return;
   }
   hp.frame = n;
   const int mode = EffectiveMode();
   if (n > 1 && mode != state.output_mode)
   {
      ++state.reconfigures; // the presenter recreates its swapchain in `mode`
   }
   // Test faults go to the presenter, which consumes them.
   for (auto [from, to] :
      {std::pair{&state.fault_present_invalid,
          &hp.stats->fault_present_invalid},
         {&state.fault_resize, &hp.stats->fault_resize},
         {&state.fault_cs_unsupported, &hp.stats->fault_cs_unsupported}})
   {
      InterlockedExchangeAdd(to, static_cast<long>(*from));
      *from = 0;
   }
   hp.stats->no_recovery = Mutated(MUT_NO_RECOVERY);
   const bool latency_wait = state.latency_enabled &&
                             state.latency_wait_enabled &&
                             !Mutated(MUT_NO_LATENCY_WAIT);
   const bool readback = state.probe_count > 0 || state.helper_block;
   const int32_t* xy = state.probe_xy;
   char line[320];
   snprintf(line, sizeof(line),
      "frame %llu %d %d %g %g %d %d %d %d %u %u %d %d %d %d %d %d %d %d "
      "%d %d %d %d %u\n",
      n, hp.pending_canvas ? 1 : 0, mode, state.paper_white,
      g_decode_gamma, state.sync_interval, overlay ? 1 : 0,
      (Mutated(MUT_NO_ENCODE) ? 1 : 0) | (Mutated(MUT_NO_BT2020) ? 2 : 0) |
         (state.present_clamp && hp.pending_canvas ? 8 : 0) |
         (out.snapshot_valid ? 16 : 0) |
         (state.dither && !Mutated(MUT_NO_DITHER) ? 32 : 0),
      state.follow_window ? 1 : 0, state.width, state.height,
      latency_wait ? 1 : 0, state.helper_block ? 1 : 0,
      std::clamp<int>(state.probe_count, 0, 4), xy[0], xy[1], xy[2], xy[3],
      xy[4], xy[5], xy[6], xy[7], gpu ? 1 : 0, state.color_refreshes);
   out.snapshot_valid = false;
   const double wait_start = NowMs();
   if (!SendHelper(line))
   {
      HelperDied(device, "closed its stdin");
      return;
   }
   InterlockedExchange64(&hp.sent, static_cast<LONG64>(n));
   // The D3D9 queue writes the textures again only after the helper's reads
   // ("out" n). A failed wait packet: CPU wait for "out" n this time, E0 after.
   state.e3_waits += gpu;
   if (gpu && !KmtWaitObject(&hp.sync, hp.fence_out.object, n))
   {
      --state.e3_waits;
      Log("d9native: presenter helper GPU sync: wait failed at %s (0x%08X), "
          "back to CPU waits",
         hp.sync.first_error_call, static_cast<uint32_t>(hp.sync.first_error));
      hp.gpu_sync = false;
      state.e3_gave_up = 1;
      while (KmtCompleted(hp.fence_out) < n)
      {
         if (HelperGone())
         {
            HelperDied(device, "exited");
            return;
         }
         SwitchToThread();
      }
   }
   // E0 frames and readback frames: the helper writes frames = n after its GPU
   // finished (and the stats of this frame).
   auto* done = reinterpret_cast<volatile LONG64*>(&hp.stats->frames);
   while ((!gpu || readback) &&
          static_cast<uint64_t>(InterlockedCompareExchange64(done, 0, 0)) < n)
   {
      if (HelperGone())
      {
         HelperDied(device, "exited");
         return;
      }
      if (NowMs() - wait_start > 5000)
      {
         HelperDied(device, "hung (5 s without the frame)");
         return;
      }
      SwitchToThread();
   }
   const double wait_ms = NowMs() - wait_start;
   const PresenterStats& stats = *hp.stats;
   state.output_ready = stats.ready;
   state.flip_hr = stats.swapchain_hr;
   state.color_space_hr = stats.color_space_hr;
   state.last_present_hr = stats.last_present_hr;
   state.resize_hr = stats.resize_hr;
   state.out_width = stats.width;
   state.out_height = stats.height;
   state.output_mode = stats.mode;
   state.out_format = stats.format;
   state.out_color_space = stats.color_space;
   state.present_ok = stats.present_ok;
   state.present_failed = stats.present_failed;
   state.swapchain_creates = stats.swapchain_creates;
   state.swapchain_releases = stats.swapchain_releases;
   state.recoveries_invalid = stats.recoveries;
   state.out_flags = stats.swapchain_flags;
   state.latency_hr = stats.latency_hr;
   state.vp_x = stats.vp[0];
   state.vp_y = stats.vp[1];
   state.vp_w = stats.vp[2];
   state.vp_h = stats.vp[3];
   state.share_bind_ok = stats.shared_desc_ok;
   state.output_resizes = stats.resizes;
   state.overlay_block_sum = stats.overlay_block_sum;
   state.overlay_block_frames = stats.overlay_block_frames;
   state.latency_flag = stats.latency_flag;
   state.latency_waits = stats.latency_waits;
   state.latency_timeouts = stats.latency_timeouts;
   memcpy(state.probe_rgba, stats.probe_rgba, sizeof(state.probe_rgba));
   ++state.helper_frames;
   g_flip_ever |= stats.present_ok > 0;
   // A colour space the new swapchain refused excludes its mode (as
   // CreateSwapchain11); the next frame asks for the fallback.
   if (stats.swapchain_creates != hp.seen_swapchain_creates)
   {
      hp.seen_swapchain_creates = stats.swapchain_creates;
      if (FAILED(stats.color_space_hr) && stats.mode != OUTPUT_SRGB)
      {
         g_mode_unsupported[stats.mode] = true;
         ++state.color_space_fallbacks;
      }
   }
   if (state.perf_count < PERF_CAPACITY)
   {
      state.drain_ms[state.perf_count] = static_cast<float>(drain_ms);
      state.handoff_ms[state.perf_count] = static_cast<float>(drain_ms);
      state.wait11_ms[state.perf_count] = static_cast<float>(wait_ms);
      state.latency_ms[state.perf_count] = 0;
      state.event_ms[state.perf_count] =
         static_cast<float>(NowMs() - hp.pending_event_start);
      state.gpu_ms[state.perf_count] = 0;
      ++state.perf_count;
   }
#if DEVELOPMENT
   MaybeLogStats();
#endif
}

// Present event: hand-off copy into the shared scene RT, then the back
// buffer is cleared for ReShade's GUI (premultiplied over transparent black:
// ImGui blends alpha ONE, INV_SRC_ALPHA, so alpha = coverage). The frame line
// goes out in reshade_present, after the GUI, or now without a D3D9 runtime.
static void HelperPresent(command_queue* queue, swapchain* swapchain,
   double event_start)
{
   device* device = queue->get_device();
   if (hp.pending)
   {
      SendHelperFrame(device); // last frame's reshade_present did not come
   }
   if (state.helper_restart)
   { // test: normal exit + new helper
      state.helper_restart = 0;
      StopHelper();
   }
   if (!EnsureOutput(device, swapchain))
   {
      return;
   }
   if (IsIconic(static_cast<HWND>(swapchain->get_hwnd())))
   {
      ++state.present_skipped;
      return;
   }
   ++state.presents;
   cb_luma_global_settings.DisplayMode =
      DisplayModeType(EffectiveMode() == OUTPUT_SRGB ? 0 : 1);
   if (!hp.gpu_sync && !hp.kmt_gave_up && state.presents >= 3)
   {
      SetupHelperSync(device);
   }
   state.e3_active = hp.gpu_sync;
   const bool use_canvas = out.redirect_used && out.canvas.handle;
   if (Mutated(MUT_NO_STICKY))
   {
      out.redirect_used = false;
   }
   if (!use_canvas)
   {
      RewriteImplicitRt0(device);
   }
   const resource source = out.draw_source.handle ? out.draw_source
                           : out.final_source.handle
                              ? out.final_source
                              : swapchain->get_current_back_buffer();
   if (use_canvas)
   {
      ++state.handoff_canvas;
   }
   else
   {
      ++(out.draw_source.handle    ? state.handoff_draw
         : out.final_source.handle ? state.handoff_stretch
                                   : state.handoff_back_buffer);
      queue->get_immediate_command_list()->copy_texture_region(
         source, 0, nullptr, out.shared_rt, 0, nullptr,
         filter_mode::min_mag_mip_point);
   }
   out.final_source = {0};
   out.draw_source = {0};
   out.handoff_canvas = use_canvas;
   ++state.handoffs;
#if DEVELOPMENT
   // Hand-off texture dump: F10 (edge) or every D9N_DUMP_EVERY_S seconds.
   {
      const bool f10 = (GetAsyncKeyState(VK_F10) & 0x8000) != 0;
      const bool hotkey = f10 && !obs.f10_down;
      obs.f10_down = f10;
      const double now = NowMs();
      if (obs.last_dump_ms == 0)
      {
         obs.last_dump_ms = now;
      }
      auto* native = reinterpret_cast<IDirect3DDevice9*>(device->get_native());
      if (hotkey)
      {
         DumpHandoff(native, "F10");
      }
      else if (obs.dump_every_s > 0 &&
               now - obs.last_dump_ms >= obs.dump_every_s * 1000)
      {
         obs.last_dump_ms = now;
         DumpHandoff(native, "timer");
      }
   }
#endif
   if (g_runtime_d3d9 && hp.overlay.handle)
   {
      auto* native = reinterpret_cast<IDirect3DDevice9*>(device->get_native());
      IDirect3DSurface9* back_buffer = nullptr;
      if (SUCCEEDED(native->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO,
             &back_buffer)))
      {
         native->ColorFill(back_buffer, nullptr, 0);
      }
      Free(&back_buffer);
   }
   hp.pending = true;
   hp.pending_canvas = use_canvas;
   hp.pending_event_start = event_start;
   if (!g_runtime_d3d9)
   {
      SendHelperFrame(device);
   }
}

// (4) Hand-off to the presenter. ReShade has no hook to skip the D3D9 Present
// that follows (see NOTES.md).
static void OnPresent(command_queue* queue, swapchain* swapchain, const rect*,
   const rect*, uint32_t, const rect*)
{
   device* device = queue->get_device();
   if (device->get_api() != device_api::d3d9)
   {
      return;
   }
   if (g_bound && swapchain != g_swapchain && !Mutated(MUT_NO_GUARD))
   {
      ++state.guard_ignored;
      return;
   }
   ++obs.frames;
   const double event_start = NowMs();
   if (g_bound)
   {
      FlushCanvasDraw(device);
   }
   d3d9_game->OnPresent(device);
   SamplerUpdateBias(NativeOf(device));
   const bool msaa = out.msaa_resolved && !Mutated(MUT_NO_MSAA_DETECT);
   if (msaa && !state.msaa_active)
   {
      Log("d9native: the game's MSAA is on: DLAA, FSR, XeSS, Luma TAA and SMAA "
          "T2x stay off until Anti-Aliasing is off in the game's options");
   }
   state.msaa_active = msaa;
   out.msaa_resolved = false;
   if (!g_bound)
   {
      // First present: this device and swapchain are the proxied ones
      // (dp:1444-1448).
      g_bound = true;
      g_device = device;
      g_swapchain = swapchain;
      UpdateGeometry(swapchain);
      Log("d9native: bound to the first presenting D3D9 swapchain %p (device %p)",
         swapchain, device);
   }
   if (!g_d3d9_latency_done)
   {
      g_d3d9_latency_done = true;
      SetD3D9Latency(device);
   }
   // The D3D9 runtime's back buffer is never shown: its effects would be wasted
   // GPU work.
   if (state.overlay_fix && g_runtime_d3d9)
   {
      g_runtime_d3d9->set_effects_state(false);
   }
   if (g_runtime_d3d9)
   {
      state.d3d9_effects_enabled = g_runtime_d3d9->get_effects_state();
   }
   if (g_output_failed)
   {
      state.present_event_end_ms = NowMs();
      return;
   }
   PollDisplay(static_cast<HWND>(swapchain->get_hwnd()));
   HelperPresent(queue, swapchain, event_start);
   state.present_event_end_ms = NowMs();
}

// ---- ReShade overlay / effects (PROXY_PARITY P1) ----
// One effect runtime: the D3D9 one. ReShade does not wrap the presenter's
// D3D12 device (device factory) and skips its swapchain; the D3D9 GUI reaches
// the screen through the shared overlay copy.
constexpr UINT OVERLAY_BLOCK = 256;

static void OnInitEffectRuntime(effect_runtime* runtime)
{
   const device_api api = runtime->get_device()->get_api();
   Log("d9native: init_effect_runtime api=0x%x runtime=%p hwnd=%p",
      static_cast<unsigned>(api), runtime, runtime->get_hwnd());
   if (api == device_api::d3d9)
   {
      ++state.runtimes_d3d9;
      if (!g_bound || runtime->get_device() == g_device)
      {
         g_runtime_d3d9 = runtime;
      }
   }
   else if (api == device_api::d3d11)
   {
      ++state.runtimes_d3d11;
   }
}

static void OnDestroyEffectRuntime(effect_runtime* runtime)
{
   if (runtime == g_runtime_d3d9)
   {
      g_runtime_d3d9 = nullptr;
   }
}

// Returning true vetoes the change (ReShade then keeps the overlay as it was).
static bool OnOpenOverlay(effect_runtime* runtime, bool open,
   input_source source)
{
   const device_api api = runtime->get_device()->get_api();
   const bool veto = false; // the D3D9 runtime's GUI is the overlay
   Log("d9native: reshade_open_overlay api=0x%x open=%d source=%d -> %s",
      static_cast<unsigned>(api), open, static_cast<int>(source),
      veto ? "vetoed" : "accepted");
   if (api == device_api::d3d9)
   {
      ++state.overlay_events_d3d9;
      state.overlay_vetoed_d3d9 += veto;
      if (!veto)
      {
         state.overlay_open_d3d9 = open;
      }
   }
   else if (api == device_api::d3d11)
   {
      ++state.overlay_events_d3d11;
      state.overlay_open_d3d11 = open;
   }
   return veto;
}

// After ReShade drew its GUI into the D3D9 back buffer: the frame line goes to
// the presenter with the overlay copy.
static void OnReshadePresent(effect_runtime* runtime)
{
   if (runtime == g_runtime_d3d9 && hp.pending)
   {
      SendHelperFrame(runtime->get_device());
   }
}

#if DEVELOPMENT
extern "C" __declspec(dllexport) D9NState* D9N_Get()
{
   return &state;
}
#endif

// ReShade's add-on overlay: the display settings, then the game's
inline void DrawOverlay(effect_runtime*)
{
   static constexpr const char* output_names[] = {"scRGB (HDR)", "HDR10", "SDR (sRGB)"};
   static constexpr const char* output_keys[] = {"scrgb", "hdr10", "srgb"};
   int output = std::clamp<int>(state.output_mode_request, OUTPUT_SCRGB, OUTPUT_SRGB);
   if (ImGui::Combo("Output", &output, output_names, int(std::size(output_names))))
   {
      state.output_mode_request = output;
      reshade::set_config_value(nullptr, NAME, "OUTPUT", output_keys[output]);
   }
   if (ImGui::IsItemHovered())
   {
      ImGui::SetTooltip("The output swapchain's format and colour space. The HDR ones fall back to SDR on a display without HDR.");
   }
   bool display_changed = DrawSettingSlider("Peak White", "ScenePeakWhite", &cb_luma_global_settings.ScenePeakWhite, default_peak_white,
      srgb_white_level, 10000.f, "The display's peak brightness in nits.", "%.0f");
   display_changed |= DrawSettingSlider("Scene Paper White", "ScenePaperWhite", &cb_luma_global_settings.ScenePaperWhite, default_paper_white,
      srgb_white_level, 500.f, "The scene's brightness of white in nits.", "%.0f");
   display_changed |= DrawSettingSlider("UI Paper White", "UIPaperWhite", &cb_luma_global_settings.UIPaperWhite, default_paper_white,
      srgb_white_level, 500.f, "The UI's brightness of white in nits.", "%.0f");
   if (display_changed)
   {
      ApplyDisplaySettings();
   }
   d3d9_game->DrawImGuiSettings();
   if (ImGui::CollapsingHeader("About"))
   {
      d3d9_game->PrintImGuiAbout();
   }
}

// The add-on's DllMain for a native D3D9 game: registers the ReShade events and the overlay, reads Core's settings, then runs
// "game->OnInit()". "game" must outlive the add-on.
inline BOOL CoreMain(HMODULE module, DWORD reason, D3D9Game* game)
{
   switch (reason)
   {
   case DLL_PROCESS_ATTACH:
   {
      g_module = module;
      d3d9_game = game;
      if (!reshade::register_addon(module))
      {
         return FALSE;
      }
      reshade::register_event<addon_event::create_device>(OnCreateDevice);
      reshade::register_event<addon_event::create_swapchain>(OnCreateSwapchain);
      reshade::register_event<addon_event::init_swapchain>(OnInitSwapchain);
      reshade::register_event<addon_event::destroy_swapchain>(OnDestroySwapchain);
      reshade::register_event<addon_event::create_pipeline>(OnCreatePipeline);
      reshade::register_event<addon_event::init_pipeline>(OnInitPipeline);
      reshade::register_event<addon_event::bind_pipeline>(OnBindPipeline);
      reshade::register_event<addon_event::bind_render_targets_and_depth_stencil>(OnBindRenderTargets);
      reshade::register_event<addon_event::push_descriptors>(OnPushDescriptors);
      reshade::register_event<addon_event::push_constants>(OnPushConstants);
      reshade::register_event<addon_event::clear_render_target_view>(OnClearRenderTarget);
      reshade::register_event<addon_event::clear_depth_stencil_view>(OnClearDepthStencil);
      reshade::register_event<addon_event::destroy_pipeline>(OnDestroyPipeline);
      reshade::register_event<addon_event::copy_resource>(OnCopyResource);
      reshade::register_event<addon_event::draw>(OnDraw);
      reshade::register_event<addon_event::draw_indexed>(OnDrawIndexed);
      reshade::register_event<addon_event::create_resource>(OnCreateResource);
      reshade::register_event<addon_event::init_resource>(OnInitResource);
      reshade::register_event<addon_event::destroy_resource>(OnDestroyResource);
      reshade::register_event<addon_event::copy_texture_region>(OnCopyTextureRegion);
      reshade::register_event<addon_event::resolve_texture_region>(OnResolveTextureRegion);
      reshade::register_event<addon_event::init_device>(OnInitDevice);
      reshade::register_event<addon_event::present>(OnPresent);
      reshade::register_event<addon_event::init_effect_runtime>(OnInitEffectRuntime);
      reshade::register_event<addon_event::destroy_effect_runtime>(OnDestroyEffectRuntime);
      reshade::register_event<addon_event::reshade_open_overlay>(OnOpenOverlay);
      reshade::register_event<addon_event::reshade_present>(OnReshadePresent);
      reshade::register_overlay(nullptr, DrawOverlay);
      state.hdr_fallback = state.latency_enabled = state.latency_wait_enabled = 1;
      state.force_display_hdr = -1;
      state.sdr_white_nits = 80.0f;
      state.presenter_helper = 1;
      // The display map's contrast / saturation (the game's settings replace them in "OnInit()")
      state.map_contrast = state.map_saturation = 1;
      if (OfflineHarness())
      {
         state.injected[0] = 2.0f;
         state.injected[1] = 0.5f;
         state.injected[2] = 0.25f;
         state.paper_white = 0.5f;
      }
      char text[8] = "";
      if (ReadSetting("SR", text, sizeof(text)))
      {
         g_sr_upscaler = !_stricmp(text, "dlss")   ? SRBridgeProtocol::kDlss
                         : !_stricmp(text, "fsr")  ? SRBridgeProtocol::kFsr
                         : !_stricmp(text, "xess") ? SRBridgeProtocol::kXess
                                                   : -1;
      }
      state.overlay_fix = !ReadSetting("OVERLAY_FIX", text, sizeof(text)) || text[0] != '0';
      game->OnInit();
      break;
   }
   case DLL_PROCESS_DETACH:
      reshade::unregister_overlay(nullptr, DrawOverlay);
      reshade::unregister_addon(module);
      break;
   }
   return TRUE;
}
