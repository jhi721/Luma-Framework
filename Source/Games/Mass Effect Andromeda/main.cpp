// Mass Effect: Andromeda — Luma anti-aliasing mod (Frostbite 3, D3D11).
// Injects DLAA or FSR 3 native AA (in-game AA = TAA) and replaces FXAA with SMAA (in-game AA = FXAA).

#define GAME_MASS_EFFECT_ANDROMEDA 1

// Frostbite input quirks: any message box (e.g. dev asserts) permanently breaks the game's
// input, and focus-loss handling interferes too.
#define DISABLE_AUTO_DEBUGGER 1
#define DISABLE_FOCUS_LOSS_SUPPRESSION 1
#define AVOID_INPUT_LOSS 1

#define GEOMETRY_SHADER_SUPPORT 0
#define ENABLE_SMAA 1 // replaces the game's FXAA pass (FXAA AA mode) with SMAA
// The dialogue/cutscene DOF-variant resolve is run-native-then-override via original_draw_dispatch_func,
// which the core only populates with this enabled — otherwise it is null outside DEVELOPMENT builds and
// every DOF resolve silently bails to native TAA in Test/Publishing.
#define ENABLE_POST_DRAW_DISPATCH_CALLBACK 1

#include "..\..\Core\core.hpp"
#include <d3d11_1.h>  // ID3D11DeviceContext1 (bound-range CB queries)
#include <shellapi.h> // ShellExecuteA for the About-tab link buttons (system("start") hangs in exclusive fullscreen)

// TAA color-resolve CS — DLAA/FSR injection point. The game ships this resolve as a permutation matrix: 4
// logic variants × 2 GPU tile-sizes (32x16 = warp-32/NVIDIA, 8x8 = wave-64/AMD+Intel), and picks the tile by
// GPU arch at runtime — so different vendors dispatch DIFFERENT hashes of the SAME pass, instruction-for-
// instruction identical per pair (e.g. 0x70E49B83 (8x8) == 0xD7E13B2A (32x16)). Hooking only one perm
// silently no-ops SR on every other vendor, so we match the whole set. (Broader feature variants with extra
// t5/t6 SRVs + u4/u5 UAVs are NOT hooked until confirmed color-resolve, not a temporal SSR/AO pass.)
static const ShaderHashesList shader_hashes_taa_resolve = {
   .compute_shaders = {
      0xD7E13B2A,
      0x70E49B83, // variant A  (32x16 / 8x8)
      0xFE348A4C,
      0x174F06D1, // variant B
      0x789DECF6,
      0x3D06A19E, // variant C
      0x960B6C89,
      0x1986BDD0, // variant D
      // E family = the same resolve quality ladder minus the t2 mask input / u0 mask output (96% identical
      // body, tile twins instruction-identical). Dispatched for the MAIN MENU / loading background scene —
      // hooked so DLAA covers the menu too. I/O contract is a strict subset of A-D (t0/t1/t3/t4 in, u2/u3 out).
      0x1C0D65CC,
      0x3DD7FD62, // variant E-A (32x16 / 8x8)
      0x40918D68,
      0xCEF1D745, // variant E-B
      0xCFB58943,
      0xA1AA9997, // variant E-C
      0x2F3D250F,
      0x633550DF, // variant E-D
   },
};
// DOF variant of the resolve (dialogue / cutscene): additionally temporally filters the DOF CoC
// (t5/t6 r16f current+history in -> u4/u5 filtered+history out; u4 is consumed by the DOF-setup CS right
// after the resolve). We must NOT cancel this dispatch — u4/u5 would go stale and break the bokeh.
// Instead: run the native dispatch first (it writes u0/u4/u5 with the game's own math — zero quality
// loss), then run SR and overwrite only its u2/u3 color output.
static const ShaderHashesList shader_hashes_taa_resolve_dof = {
   .compute_shaders = {
      0x42871661,
      0xA280FBF8, // variant DOF-A (32x16 / 8x8)
      0x6514D8F7,
      0x34C459FC, // variant DOF-B
      0x631EF4A0,
      0xEBA90095, // variant DOF-C
      0x65882783,
      0x3BEAC6F3, // variant DOF-D
   },
};

static constexpr uint32_t kFXAAHash = 0x5B81D1F2;    // FXAA PS — replaced with SMAA
static constexpr uint32_t kGbufferVS_A = 0xC089424D; // gbuffer VS that binds the main camera CB at VS slot 2
static constexpr uint32_t kGbufferVS_B = 0xFF93953D; // (reliable jitter capture point)
// PS that turns the D24 reverse-Z depth into the game's linear view depth (r32_float, metres; near / device Z), the SMAA
// predication source. Drawn twice per frame, in gameplay and the main menu alike; the second write is the final depth.
static constexpr uint32_t kLinearDepthHash = 0xDE1C9EB9;
// Tonemap PS (LUT 33^3, also writes an R8 mask at RT1): its RT0 is display-encoded and bounded, the FXAA pass' input, and
// only UI and the final encode follow it. RCAS runs on it under DLSS/FSR.
static constexpr uint32_t kTonemapHash = 0xB6A91712;
// DLSS far_plane stand-in: MEA's projection is reverse-Z INFINITE-far (no finite far). DLSS is insensitive to the
// exact large value (used only for depth linearization).
static constexpr float kCamFar = 100000.f;
// Presents after which SMAA's and RCAS's resources go once they stopped running
static constexpr uint32_t smaa_idle_release_frames = 600;

// --- User-facing settings (persisted via ReShade config; loaded in LoadConfigs, saved on UI change). Kept as
// file-scope globals so LoadConfigs (pre-device) can populate them. ---
static constexpr bool kDefaultSmaaEnable = true;
static constexpr float kDefaultRcasSharpness = 0.f;       // RCAS on SMAA or DLSS/FSR, off by default
static constexpr bool kDefaultSmaaPredication = true;     // predicate SMAA on geometry, using the game's linear view depth
static constexpr float kDefaultSmaaPredTolerance = 0.02f; // plane deviation counted as a full edge, as a fraction of view depth
static constexpr bool kDefaultDisableTaaSharpening = false;
static constexpr bool kDefaultImproveTaaJitter = true;
static bool g_smaa_enable = kDefaultSmaaEnable;
static float g_rcas_sharpness = kDefaultRcasSharpness;
static bool g_smaa_predication = kDefaultSmaaPredication;
static float g_smaa_pred_tolerance = kDefaultSmaaPredTolerance;
static bool g_disable_taa_sharpening = kDefaultDisableTaaSharpening;
static bool g_improve_taa_jitter = kDefaultImproveTaaJitter;
// DEV knob: Halton (2, 3) jitter while SR runs (else the game's vanilla correlated multi-jittered sequence)
static bool g_halton_jitter = true;
// DEV knob: the Halton phase count "Fix Native TAA Jitter" gives the game's own TAA. 16 measured best on its resolve (static
// background, vs the vanilla 389 point table): frame-to-frame flicker -70%, sharpness +9%; 8 gave -57%/+6%, 32 -40%/+3%.
static int g_halton_jitter_native_phases = 16;

// Frostbite's "WorldRender" settings container (reflected class WorldRenderSettings, 0x4E0 bytes). Field offsets from its
// TypeInfo field table. The TAA setup reads these every frame and rebuilds its jitter sequence when they change (the engine
// writes fields of this container at runtime itself), so writing them is the engine's own path, no code patch.
static constexpr size_t kWrsJitterCount = 0x2E4;          // uint32: sequence length (vanilla 389)
static constexpr size_t kWrsPostSharpeningAmount = 0x2E8; // float: the TAA resolve's sharpen, its cb0[19].w (vanilla 0.5)
static constexpr size_t kWrsJitterUseCmj = 0x478;         // bool: the jitter table below, else the engine's own Halton (2, 3) from index 0
struct WorldRenderSettingsValues
{
   uint8_t jitter_use_cmj = 0;
   uint32_t jitter_count = 0;
   float post_sharpening_amount = 0.f;
};
static uint8_t* g_world_render_settings = nullptr;                     // Render thread only (OnPresent)
static WorldRenderSettingsValues g_world_render_settings_vanilla = {}; // Valid while "g_world_render_settings" is set
static bool g_world_render_settings_rejected = false;                  // The container failed the layout check: not this build

// --- Live dev knobs (DEV overlay and MCP, not saved). Default signs are the stable trail-free set:
// MV flip X+Y, jitter flip Y; jitter flip X shakes. ---
static bool g_mv_flip_x = true;      // -> MV X scale = -0.5*W
static bool g_mv_flip_y = true;      // -> MV Y scale = +0.5*H
static float g_mv_scale_mult = 1.f;  // 0.25..4
static bool g_jitter_flip_x = false; // jitter X = -clipX*0.5*W (flipping X shakes — keep off)
static bool g_jitter_flip_y = true;  // jitter Y = +clipY*0.5*H (removes shimmer)
static bool g_mv_jittered = false;

#if DEVELOPMENT
// What the TAA and FXAA hooks did in a frame, for the MCP "luma_dev_values" tool ("last" = the last complete frame).
// MEA_COUNT also notes the outcome on the draw or dispatch in a running "luma_trace_capture".
struct FrameCounters
{
   uint32_t taa_resolves = 0;          // hooked TAA resolve dispatches
   uint32_t taa_resolves_deferred = 0; // on a deferred context: SR can't run there, native TAA
   uint32_t dof_resolves = 0;          // DOF variant: native resolve run first, then SR
   uint32_t camera_gbuffer = 0;        // camera captured at the gbuffer VS
   uint32_t camera_scene_vs = 0;       // camera captured at another scene VS (main menu), keyed to the scene size
   uint32_t camera_probe = 0;          // camera found by the fallback probe at the resolve
   uint32_t camera_misses = 0;         // no camera at the resolve: native TAA
   uint32_t handoff_cs = 0;            // SR output copied through the game's UAVs (copy CS)
   uint32_t handoff_incompatible = 0;  // no hand-off into u2 (copy CS not ready, or u2 of another size): native TAA
   uint32_t u2_r11g11b10 = 0;          // u2 is r11g11b10_float (the game's alternate "Buffer Format")
   uint32_t sr_draws = 0;              // SR drew and replaced the resolve
   uint32_t sr_failures = 0;           // output creation or Draw failed: SR suppressed until re-picked
   uint32_t fxaa_draws = 0;            // the game's FXAA pass
   uint32_t smaa_draws = 0;            // SMAA replaced it
   uint32_t rcas_draws = 0;            // RCAS ran (on SMAA's output or the SR tonemap's)
};
static FrameCounters g_counters_this_frame;
static FrameCounters g_counters_last_frame;
#define MEA_COUNT(counter)                    \
   do                                         \
   {                                          \
      ++g_counters_this_frame.counter;        \
      Mcp::Annotate(cmd_list_data, #counter); \
   } while (false)
#else
#define MEA_COUNT(counter) \
   do                      \
   {                       \
   } while (false)
#endif

struct MassEffectAndromedaGameDeviceData final : public GameDeviceData
{
   // --- Camera state captured once per frame from the per-view camera CB (CPU map pointer + bound offset) ---
   bool cam_valid_this_frame = false;
   // The CPU map pointer of every large WRITE_NO_OVERWRITE DYNAMIC CB this frame (the camera ring among them), deduped by
   // handle: the camera is read straight from pointer + bound offset, no GPU readback. The ring is mapped on both the
   // immediate and Frostbite worker threads, hence the mutex. Reset each present.
   struct MapRec
   {
      uint64_t handle = 0;
      const void* data = nullptr; // nullptr = banned (seen DISCARD-mapped)
      uint64_t size = 0;
   };
   struct MapCache
   {
      std::shared_mutex mutex;
      static constexpr int kMaxMaps = 16;
      MapRec recs[kMaxMaps] = {};
      int count = 0;
#if DEVELOPMENT || TEST
      bool logged_full = false; // one-shot kMaxMaps cap warning
#endif

      // Caller holds "mutex"
      int Find(uint64_t handle) const
      {
         for (int i = 0; i < count; ++i)
         {
            if (recs[i].handle == handle)
               return i;
         }
         return -1;
      }
   };
   MapCache map_cache;
   // The immediate context's ID3D11DeviceContext1 (bound-range CB queries), queried once. Non-owning: the device owns its
   // immediate context for its whole lifetime.
   ID3D11DeviceContext1* immediate_context1 = nullptr;
   // Where the camera CB was last found by the resolve's probe, to avoid probing all 28 combos every frame
   struct CameraSlot
   {
      bool compute = false;
      UINT slot = 0;
   };
   std::optional<CameraSlot> cam_probe_slot;
   float cam_jitter_clip_x = 0.f; // raw [6].z
   float cam_jitter_clip_y = 0.f; // raw [7].z
   float cam_near = 0.06f;        // [8].w
   float cam_proj_m11 = 0.f;      // [7].y (vertical FOV for FSR)
   // The last hooked resolve's scene size: keys the camera capture at draws other than the gbuffer pass (0 = none yet)
   uint2 scene_size = {};

#if ENABLE_SR
   // --- SR (the output texture is Core's device_data.sr_output_color; DLSS/FSR write there, we copy into u2/u3) ---
   ComPtr<ID3D11ShaderResourceView> sr_output_srv; // t0 of the format-converting hand-off CS
   // A new output texture: DLSS needs its feature recreated after the first draw into it (see the Draw block)
   bool sr_output_recreated = false;
   // "LatchSRFrame" at present: an upscaler is picked and hasn't failed. Fixed for the whole frame.
   bool sr_active = false;
   // "CleanExtraSRResources" ran (None picked): ours go at the next present
   std::atomic<bool> release_sr_resources = false;
#endif

#if ENABLE_SMAA
   // --- SMAA (replaces the game's FXAA pass in FXAA AA mode) ---
   ComPtr<ID3D11Buffer> cb_smaa_metrics; // float4(1/W,1/H,W,H) at output res + float4(predication scale,0,0,0), at VS+PS b1
   uint2 smaa_metrics_size = {};
   float smaa_metrics_pred_scale = -1.f;
   // Predication: the game's linear view depth (the RT of "kLinearDepthHash", keyed by its resource) turned into an
   // edge-ness mask by the depth-extract CS. A capture from an earlier frame (the pass didn't draw) is not used.
   ComPtr<ID3D11Resource> scene_depth;
   ComPtr<ID3D11ShaderResourceView> srv_scene_depth;
   uint32_t scene_depth_frame = UINT32_MAX;
   ComPtr<ID3D11Buffer> cb_pred;
   float pred_tolerance = -1.f;
   ComPtr<ID3D11Texture2D> tex_pred;
   ComPtr<ID3D11UnorderedAccessView> uav_pred;
   ComPtr<ID3D11ShaderResourceView> srv_pred;
   uint2 pred_size = {};

   void ReleasePredication()
   {
      scene_depth.reset();
      srv_scene_depth.reset();
      cb_pred.reset();
      uav_pred.reset();
      srv_pred.reset();
      tex_pred.reset();
   }
   // RCAS's input: SMAA's output, or the tonemap's under DLSS/FSR (both display-encoded)
   ComPtr<ID3D11Texture2D> tex_rcas_input;
   ComPtr<ID3D11RenderTargetView> tex_rcas_input_rtv;
   ComPtr<ID3D11ShaderResourceView> tex_rcas_input_srv;
   ComPtr<ID3D11Buffer> cb_sharpen; // (W, H, sharpness, 0)
   uint2 rcas_input_size = {};
   float sharpen_amount = -1.f; // cache key for cb_sharpen

   void ReleaseRCAS()
   {
      tex_rcas_input_rtv.reset();
      tex_rcas_input_srv.reset();
      tex_rcas_input.reset();
      cb_sharpen.reset();
   }
   // The frames SMAA and RCAS last ran, for "smaa_idle_release_frames"
   uint32_t smaa_frame = 0;
   uint32_t sharpen_frame = 0;
#endif
};

class MassEffectAndromeda final : public Game
{
   static MassEffectAndromedaGameDeviceData& GetGameDeviceData(DeviceData& device_data)
   {
      return *static_cast<MassEffectAndromedaGameDeviceData*>(device_data.game);
   }

#if ENABLE_SR
   static bool IsSRActive(DeviceData& device_data)
   {
      return GetGameDeviceData(device_data).sr_active;
   }
#endif

   // A texture with a SRV, plus a RTV and/or a UAV when asked. False on any failure (the caller resets what was made).
   static bool CreateTexture(ID3D11Device* native_device, DXGI_FORMAT format, uint2 size, UINT bind_flags, ID3D11Texture2D** texture, ID3D11ShaderResourceView** srv,
      ID3D11RenderTargetView** rtv = nullptr, ID3D11UnorderedAccessView** uav = nullptr)
   {
      const CD3D11_TEXTURE2D_DESC desc(format, size.x, size.y, 1, 1, bind_flags);
      if (FAILED(native_device->CreateTexture2D(&desc, nullptr, texture)) || FAILED(native_device->CreateShaderResourceView(*texture, nullptr, srv)))
         return false;
      if (rtv && FAILED(native_device->CreateRenderTargetView(*texture, nullptr, rtv)))
         return false;
      return !uav || SUCCEEDED(native_device->CreateUnorderedAccessView(*texture, nullptr, uav));
   }

   static ID3D11DeviceContext1* GetImmediateContext1(ID3D11DeviceContext* immediate_context, MassEffectAndromedaGameDeviceData* gd)
   {
      if (!gd->immediate_context1)
      {
         ComPtr<ID3D11DeviceContext1> context1;
         if (SUCCEEDED(immediate_context->QueryInterface(context1.put())))
         {
            gd->immediate_context1 = context1.get();
         }
      }
      return gd->immediate_context1;
   }

#if ENABLE_SMAA
   // RCAS on a display-encoded pass (SMAA, or the tonemap under DLSS/FSR): the pass draws into "tex_rcas_input" and RCAS
   // writes the pass' own target. False when a piece is missing (shaders still compiling, a failed creation): the pass then
   // draws straight to its target, or the image would be lost.
   static bool PrepareRCAS(ID3D11Device* native_device, DeviceData& device_data, MassEffectAndromedaGameDeviceData* gd, uint2 size)
   {
      if (g_rcas_sharpness <= 0.f || FindShader(device_data.native_vertex_shaders, CompileTimeStringHash("Copy VS")) == nullptr ||
          FindShader(device_data.native_pixel_shaders, CompileTimeStringHash("MEA Sharpen PS")) == nullptr)
         return false;
      if (!gd->tex_rcas_input || gd->rcas_input_size != size)
      {
         gd->ReleaseRCAS(); // the CB holds the size too
         if (CreateTexture(native_device, DXGI_FORMAT_R16G16B16A16_FLOAT, size, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET, gd->tex_rcas_input.put(),
                gd->tex_rcas_input_srv.put(), gd->tex_rcas_input_rtv.put()))
         {
            gd->rcas_input_size = size;
         }
      }
      if (!gd->cb_sharpen || gd->sharpen_amount != g_rcas_sharpness)
      {
         const float sp[4] = {(float)size.x, (float)size.y, g_rcas_sharpness, 0.f};
         if (CreateImmutableCB(native_device, sp, sizeof(sp), std::addressof(gd->cb_sharpen)))
         {
            gd->sharpen_amount = g_rcas_sharpness;
         }
      }
      return gd->tex_rcas_input_rtv && gd->tex_rcas_input_srv && gd->cb_sharpen;
   }

   static void DrawRCAS(ID3D11DeviceContext* native_device_context, DeviceData& device_data, MassEffectAndromedaGameDeviceData* gd, ID3D11RenderTargetView* target, uint2 size)
   {
      gd->sharpen_frame = cb_luma_global_settings.FrameIndex;
      // DrawCustomPixelShader does NOT restore state
      DrawStateStack<DrawStateStackType::FullGraphics> sharpen_state;
      sharpen_state.Cache(native_device_context, device_data.uav_max_count);
      ID3D11Buffer* scb = gd->cb_sharpen.get();
      native_device_context->PSSetConstantBuffers(0, 1, &scb);
      DrawCustomPixelShader(native_device_context, device_data.default_depth_stencil_state.get(), device_data.default_blend_state.get(), nullptr,
         FindShader(device_data.native_vertex_shaders, CompileTimeStringHash("Copy VS")), FindShader(device_data.native_pixel_shaders, CompileTimeStringHash("MEA Sharpen PS")),
         gd->tex_rcas_input_srv.get(), target, size.x, size.y, false);
      sharpen_state.Restore(native_device_context);
   }
#endif

   static bool CreateImmutableCB(ID3D11Device* device, const void* data, UINT size, ComPtr<ID3D11Buffer>* out)
   {
      out->reset();
      const CD3D11_BUFFER_DESC bd(size, D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_IMMUTABLE);
      const D3D11_SUBRESOURCE_DATA initial_data = {.pSysMem = data};
      return SUCCEEDED(device->CreateBuffer(&bd, &initial_data, out->put()));
   }

   // Read the per-view camera CB bound at (cs,slot) from its cached CPU map ptr; if valid, cache jitter/proj/near
   // into gd. Validation: signature [5]==(0,0,0,1) and [9].z==-1; plus, with "expected_size", [1].xy must match the
   // render res (rejects other views). Without it, the signature alone accepts (gbuffer VS = reliably the main
   // view). Caller MUST hold gd->map_cache.mutex (shared is enough).
   static bool TryStoreCamera(ID3D11DeviceContext1* ctx1, MassEffectAndromedaGameDeviceData* gd, bool cs, UINT slot, const uint2* expected_size)
   {
      ID3D11Buffer* cb = nullptr;
      UINT fc = 0, nc = 0;
      if (cs)
      {
         ctx1->CSGetConstantBuffers1(slot, 1, &cb, &fc, &nc);
      }
      else
      {
         ctx1->VSGetConstantBuffers1(slot, 1, &cb, &fc, &nc);
      }
      if (!cb)
         return false;
      const uint64_t h = reinterpret_cast<uint64_t>(cb);
      const uint32_t off = fc * 16u;
      cb->Release(); // only the handle is needed, as the map cache key
      for (int i = 0; i < gd->map_cache.count; ++i)
      {
         const auto& rec = gd->map_cache.recs[i];
         if (rec.handle != h || rec.data == nullptr || (uint64_t)off + 160u > rec.size)
            continue; // need [off .. off+10 float4]; nullptr = banned (seen DISCARD-mapped → pointer unsafe)
         const float* r = reinterpret_cast<const float*>(reinterpret_cast<const uint8_t*>(rec.data) + off);
         if (expected_size && (fabsf(r[1 * 4 + 0] - (float)expected_size->x) > 1.f || fabsf(r[1 * 4 + 1] - (float)expected_size->y) > 1.f))
            return false; // [1].xy == res
         if (fabsf(r[5 * 4 + 0]) > 1e-3f || fabsf(r[5 * 4 + 1]) > 1e-3f || fabsf(r[5 * 4 + 2]) > 1e-3f || fabsf(r[5 * 4 + 3] - 1.f) > 1e-3f)
            return false; // [5]==(0,0,0,1)
         if (fabsf(r[9 * 4 + 2] + 1.f) > 1e-2f)
            return false; // [9].z == -1 (reverse-Z m32)
         gd->cam_proj_m11 = r[7 * 4 + 1];
         gd->cam_jitter_clip_x = r[6 * 4 + 2];
         gd->cam_jitter_clip_y = r[7 * 4 + 2];
         gd->cam_near = r[8 * 4 + 3];
         gd->cam_valid_this_frame = true;
         return true;
      }
      return false;
   }

   // Cache the CPU map ptr of every large (≥64KB) WRITE_NO_OVERWRITE DYNAMIC CB (camera ring among them), deduped
   // by handle. NO_OVERWRITE only (allocation stays committed → post-Unmap read is safe). A DISCARD map of a
   // tracked handle BANS it (data=nullptr, kept in the array): DISCARD hands the driver a new allocation and can
   // free the old one, so the cached pointer is a use-after-free for the camera probe — and a buffer the game
   // EVER discard-maps is not the always-NO_OVERWRITE camera ring, so re-caching it later would just re-arm the
   // hazard (the wide slot probe crashed on exactly this in menus, where UI CBs are DISCARD-cycled every frame).
   // OnDestroyResource removes the record entirely, which also un-bans a handle the runtime later recycles.
   // Fires on Frostbite worker threads → the map cache's mutex.
   static void OnMapBufferRegion(reshade::api::device* device, reshade::api::resource resource, uint64_t offset, uint64_t size, reshade::api::map_access access, void** data)
   {
      DeviceData& device_data = *device->get_private_data<DeviceData>();
      if (!device_data.game)
         return;
      auto& gd = GetGameDeviceData(device_data);
      const bool discard = access == reshade::api::map_access::write_discard;
      if (!discard && (access != reshade::api::map_access::write_only || data == nullptr || *data == nullptr))
         return; // NO_OVERWRITE (cached) or DISCARD (bans) only
      ID3D11Buffer* buffer = reinterpret_cast<ID3D11Buffer*>(resource.handle);
      D3D11_BUFFER_DESC bd = {};
      buffer->GetDesc(&bd);
      if ((bd.BindFlags & D3D11_BIND_CONSTANT_BUFFER) == 0 || bd.Usage != D3D11_USAGE_DYNAMIC)
         return;
      if (bd.ByteWidth < 65536)
         return; // camera ring buffer is ~1MB; skip small per-draw CBs
      const std::unique_lock lock(gd.map_cache.mutex);
      if (const int i = gd.map_cache.Find(resource.handle); i >= 0) // dedupe by handle — keep latest pointer
      {
         if (discard)
         {
            gd.map_cache.recs[i].data = nullptr; // ban — see header comment
         }
         else if (gd.map_cache.recs[i].data != nullptr) // banned handles stay banned until destroyed
         {
            gd.map_cache.recs[i].data = *data;
            gd.map_cache.recs[i].size = bd.ByteWidth;
         }
         return;
      }
      if (discard)
         return;
      if (gd.map_cache.count < MassEffectAndromedaGameDeviceData::MapCache::kMaxMaps)
      {
         gd.map_cache.recs[gd.map_cache.count++] = {.handle = resource.handle, .data = *data, .size = bd.ByteWidth};
      }
#if DEVELOPMENT || TEST
      else if (!gd.map_cache.logged_full) // cap hit → the camera handle may be dropped this frame (SR falls back to native TAA)
      {
         gd.map_cache.logged_full = true;
         reshade::log::message(reshade::log::level::warning, "MEA: map cache cap (kMaxMaps) hit — raise it if camera capture starts missing.");
      }
#endif
   }

   // Drop the record of a destroyed buffer: its cached map pointer is dead, and the runtime may recycle the
   // handle for a brand-new buffer (which must not inherit a stale pointer or a DISCARD ban).
   static void OnDestroyResource(reshade::api::device* device, reshade::api::resource resource)
   {
      DeviceData* device_data = device->get_private_data<DeviceData>();
      if (device_data == nullptr || !device_data->game)
         return;
      auto& gd = GetGameDeviceData(*device_data);
      const std::unique_lock lock(gd.map_cache.mutex);
      if (const int i = gd.map_cache.Find(resource.handle); i >= 0)
      {
         gd.map_cache.recs[i] = gd.map_cache.recs[--gd.map_cache.count];
      }
   }

   // The engine's "WorldRender" settings container, null until the engine made it, or on an unknown build. Found through the
   // function that creates it: sub rsp, 0x48; mov rcx, [rip+settings_manager]; lea rdx, [rip+"WorldRender"]; call get_container;
   // test rax, rax; jnz; lea r8d, [rax+0x10]; lea rcx, [rip+?]; mov edx, 0x4E0 (the class size). Its get_container call is reused.
   static uint8_t* FindWorldRenderSettings()
   {
      using GetContainer = uint8_t* (*)(void* manager, const char* name);
      struct Locator
      {
         void* const* manager = nullptr;
         const char* name = nullptr;
         GetContainer get_container = nullptr;
      };
      static const Locator locator = []
      {
         // clang-format off
         constexpr std::array<System::BytePattern, 40> pattern = {{
            0x48, 0x83, 0xEC, 0x48, 0x48, 0x8B, 0x0D, System::ANY, System::ANY, System::ANY, System::ANY, 0x48, 0x8D, 0x15, System::ANY, System::ANY,
            System::ANY, System::ANY, 0xE8, System::ANY, System::ANY, System::ANY, System::ANY, 0x48, 0x85, 0xC0, 0x75, 0x6E, 0x44, 0x8D, 0x40, 0x10,
            0x48, 0x8D, 0x0D, System::ANY, System::ANY, System::ANY, System::ANY, 0xBA
         }};
         // clang-format on
         const std::vector<std::byte*> matches = System::ScanModuleForPattern(pattern);
         if (matches.size() != 1)
            return Locator{};
         const uint8_t* function = reinterpret_cast<const uint8_t*>(matches[0]);
         if (std::memcmp(function + 0x28, "\xE0\x04\x00\x00", 4) != 0)
            return Locator{};
         const auto rip_target = [&](size_t operand, size_t next)
         { return function + next + *reinterpret_cast<const int32_t*>(function + operand); };
         const char* name = reinterpret_cast<const char*>(rip_target(0x0E, 0x12));
         if (std::strcmp(name, "WorldRender") != 0)
            return Locator{};
         return Locator{
            .manager = reinterpret_cast<void* const*>(rip_target(0x07, 0x0B)),
            .name = name,
            .get_container = reinterpret_cast<GetContainer>(const_cast<uint8_t*>(rip_target(0x13, 0x17))),
         };
      }();
      if (!locator.get_container || *locator.manager == nullptr)
         return nullptr;
      return locator.get_container(*locator.manager, locator.name);
   }

   // The engine's jitter table, a vector of float2 pixel offsets in [-0.5, 0.5] that it fills with a correlated multi-jittered
   // sequence of "TemporalAAJitterCount" points, rebuilt only when the count changes. Found through its lookup, which every
   // frame returns table[frame % size]: mov r9, [rip+begin]; mov r8, [rip+end]; mov eax, edx; xor edx, edx; sub r8, r9;
   // sar r8, 3; div r8d; mov rax, [r9+rdx*8]; mov [rcx], rax; mov rax, rcx; ret
   struct JitterTable
   {
      float* const* begin = nullptr;
      float* const* end = nullptr;
   };
   static const JitterTable& FindJitterTable()
   {
      static const JitterTable table = []
      {
         // clang-format off
         constexpr std::array<System::BytePattern, 39> pattern = {{
            0x4C, 0x8B, 0x0D, System::ANY, System::ANY, System::ANY, System::ANY, 0x4C, 0x8B, 0x05, System::ANY, System::ANY, System::ANY, System::ANY,
            0x8B, 0xC2, 0x33, 0xD2, 0x4D, 0x2B, 0xC1, 0x49, 0xC1, 0xF8, 0x03, 0x41, 0xF7, 0xF0, 0x49, 0x8B, 0x04, 0xD1, 0x48, 0x89, 0x01, 0x48,
            0x8B, 0xC1, 0xC3
         }};
         // clang-format on
         const std::vector<std::byte*> matches = System::ScanModuleForPattern(pattern);
         if (matches.size() != 1)
            return JitterTable{};
         const uint8_t* function = reinterpret_cast<const uint8_t*>(matches[0]);
         const auto rip_target = [&](size_t operand, size_t next)
         { return reinterpret_cast<float* const*>(function + next + *reinterpret_cast<const int32_t*>(function + operand)); };
         return JitterTable{.begin = rip_target(0x03, 0x07), .end = rip_target(0x0A, 0x0E)};
      }();
      return table;
   }

   static void WriteWorldRenderSettings(const WorldRenderSettingsValues& values)
   {
      uint8_t* settings = g_world_render_settings;
      settings[kWrsJitterUseCmj] = values.jitter_use_cmj;
      std::memcpy(settings + kWrsJitterCount, &values.jitter_count, sizeof(values.jitter_count));
      std::memcpy(settings + kWrsPostSharpeningAmount, &values.post_sharpening_amount, sizeof(values.post_sharpening_amount));
   }

   // The game's TAA settings for this frame, from the user's choices: while SR runs, a jitter table of the SR's phase count filled
   // with Luma's Halton (2, 3) (DLSS/FSR want one; the vanilla 389 point table isn't, and the engine's own Halton starts at index
   // 0, a corner sample with a -0.11 pixel mean in y over 8 phases), and no resolve sharpen when turned off
   static void UpdateWorldRenderSettings(DeviceData& device_data)
   {
      if (!g_world_render_settings)
      {
         if (g_world_render_settings_rejected)
            return;
         g_world_render_settings = FindWorldRenderSettings();
         if (!g_world_render_settings)
            return;
         WorldRenderSettingsValues vanilla = {.jitter_use_cmj = g_world_render_settings[kWrsJitterUseCmj]};
         std::memcpy(&vanilla.jitter_count, g_world_render_settings + kWrsJitterCount, sizeof(vanilla.jitter_count));
         std::memcpy(&vanilla.post_sharpening_amount, g_world_render_settings + kWrsPostSharpeningAmount, sizeof(vanilla.post_sharpening_amount));
         // A layout check: a bool, a sane sequence length and sharpen amount, else this isn't the build the offsets come from
         if (vanilla.jitter_use_cmj > 1 || vanilla.jitter_count == 0 || vanilla.jitter_count > 4096 || !(vanilla.post_sharpening_amount >= 0.f && vanilla.post_sharpening_amount <= 16.f))
         {
            g_world_render_settings = nullptr;
            g_world_render_settings_rejected = true;
            return;
         }
         g_world_render_settings_vanilla = vanilla;
      }
      WorldRenderSettingsValues values = g_world_render_settings_vanilla;
#if ENABLE_SR
      const bool sr_active = IsSRActive(device_data);
      if (sr_active ? g_halton_jitter : g_improve_taa_jitter)
      {
         const SR::InstanceData* sr_instance_data = device_data.GetSRInstanceData();
         values.jitter_use_cmj = 1;
         values.jitter_count = uint32_t((sr_active && sr_instance_data) ? (std::max)(sr_implementations[device_data.sr_type]->GetJitterPhases(sr_instance_data), 1) : g_halton_jitter_native_phases);
         // The engine rebuilds the table at the new count during the next frame: the points go in from the present after it
         const JitterTable& table = FindJitterTable();
         if (table.begin && *table.begin && size_t(*table.end - *table.begin) == size_t(values.jitter_count) * 2)
         {
            for (uint32_t i = 0; i < values.jitter_count; ++i)
            {
               (*table.begin)[i * 2] = SR::HaltonSequence(i, 2);
               (*table.begin)[i * 2 + 1] = SR::HaltonSequence(i, 3);
            }
         }
      }
#endif
      if (g_disable_taa_sharpening)
      {
         values.post_sharpening_amount = 0.f;
      }
      WriteWorldRenderSettings(values);
   }

   // Camera capture (jitter/near/FOV) at a scene VS draw, once per frame (retries on the next draw if this one's camera map
   // hasn't landed). Immediate only keeps "cam_valid_this_frame" single-threaded. Here, not at the compute TAA dispatch: by
   // then VS slot 2 holds another view's camera. The gameplay gbuffer VS identifies the main view by hash. Scenes without it
   // (the main menu's forward-shaded planet) bind the same per-view CB at VS b2: any draw whose b2 passes the layout
   // signature with [1].xy = the last resolve's scene size is the main view (shadow and half-res views have their own sizes).
   static void CaptureCamera(ID3D11DeviceContext* native_device_context, [[maybe_unused]] CommandListData& cmd_list_data, MassEffectAndromedaGameDeviceData* gd,
      const ShaderHashesList<OneShaderPerPipeline>& original_shader_hashes)
   {
      const bool gbuffer = original_shader_hashes.Contains(kGbufferVS_A, reshade::api::shader_stage::vertex) ||
                           original_shader_hashes.Contains(kGbufferVS_B, reshade::api::shader_stage::vertex);
      if (!gbuffer && gd->scene_size.x == 0)
         return;
      ID3D11DeviceContext1* context1 = GetImmediateContext1(native_device_context, gd);
      if (!context1)
         return;
      const std::shared_lock lock(gd->map_cache.mutex);
      // Signature-only at the gbuffer VS (the hash identifies the main view), else keyed to the scene size
      if (!TryStoreCamera(context1, gd, false, 2, (gbuffer ? nullptr : &gd->scene_size)))
         return;
      if (gbuffer)
      {
         MEA_COUNT(camera_gbuffer);
      }
      else
      {
         MEA_COUNT(camera_scene_vs);
      }
   }

#if ENABLE_SMAA
   // SMAA predication source, captured at its writer (its readers vary with the settings). Immediate only, as the FXAA pass
   // that reads it: a capture recorded on a worker thread would race it.
   static void CaptureLinearDepth(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, MassEffectAndromedaGameDeviceData* gd)
   {
      ComPtr<ID3D11RenderTargetView> depth_rtv;
      native_device_context->OMGetRenderTargets(1, depth_rtv.put(), nullptr);
      if (!depth_rtv)
         return;
      D3D11_RENDER_TARGET_VIEW_DESC depth_rtv_desc = {};
      depth_rtv->GetDesc(&depth_rtv_desc);
      if (depth_rtv_desc.Format != DXGI_FORMAT_R32_FLOAT || depth_rtv_desc.ViewDimension != D3D11_RTV_DIMENSION_TEXTURE2D)
         return;
      ComPtr<ID3D11Resource> depth_resource;
      depth_rtv->GetResource(depth_resource.put());
      if (depth_resource.get() != gd->scene_depth.get())
      {
         gd->srv_scene_depth.reset();
         gd->scene_depth = depth_resource;
         const CD3D11_SHADER_RESOURCE_VIEW_DESC srv_desc(D3D11_SRV_DIMENSION_TEXTURE2D, DXGI_FORMAT_R32_FLOAT, depth_rtv_desc.Texture2D.MipSlice, 1);
         native_device->CreateShaderResourceView(depth_resource.get(), &srv_desc, gd->srv_scene_depth.put());
      }
      gd->scene_depth_frame = cb_luma_global_settings.FrameIndex;
   }

   // The FXAA pass (in-game FXAA AA mode) replaced with SMAA (with predication, then RCAS when on). Mutually exclusive with the
   // TAA path: FXAA mode has no TAA dispatch, and vice versa.
   static DrawOrDispatchOverrideType ReplaceFXAAWithSMAA(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, [[maybe_unused]] CommandListData& cmd_list_data,
      DeviceData& device_data, MassEffectAndromedaGameDeviceData* gd)
   {
      ComPtr<ID3D11ShaderResourceView> srv_color;
      native_device_context->PSGetShaderResources(0, 1, srv_color.put()); // t0 = display-encoded color
      ComPtr<ID3D11RenderTargetView> rtv;
      native_device_context->OMGetRenderTargets(1, rtv.put(), nullptr); // output (separate resource)
      // The real RTV size (not the swapchain's): it follows the in-game Resolution Scale
      const uint2 size = GetViewTextureSize(rtv.get());
      if (!srv_color || size.x == 0 || size.y == 0)
         return DrawOrDispatchOverrideType::None;
      gd->smaa_frame = cb_luma_global_settings.FrameIndex;

      // Predication: the scale and the mask fall back together (2.0 without a mask raises the threshold frame-wide).
      // The CS maps texels 1:1, so the depth must be this frame's and of the FXAA target's size.
      auto* pred_cs = FindShader(device_data.native_compute_shaders, CompileTimeStringHash("MEA Depth Extract CS"));
      bool pred_ok = g_smaa_predication && pred_cs != nullptr && gd->srv_scene_depth && gd->scene_depth_frame == cb_luma_global_settings.FrameIndex &&
                     GetViewTextureSize(gd->srv_scene_depth.get()) == size;
      if (pred_ok)
      {
         if (!gd->cb_pred || gd->pred_tolerance != g_smaa_pred_tolerance)
         {
            const float pred_params[4] = {g_smaa_pred_tolerance, 0.f, 0.f, 0.f};
            if (CreateImmutableCB(native_device, pred_params, sizeof(pred_params), std::addressof(gd->cb_pred)))
            {
               gd->pred_tolerance = g_smaa_pred_tolerance;
            }
         }
         if (!gd->tex_pred || gd->pred_size != size)
         {
            gd->uav_pred.reset();
            gd->srv_pred.reset();
            gd->tex_pred.reset();
            if (CreateTexture(native_device, DXGI_FORMAT_R16_FLOAT, size, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, gd->tex_pred.put(), gd->srv_pred.put(), nullptr,
                   gd->uav_pred.put()))
            {
               gd->pred_size = size;
            }
         }
         pred_ok = gd->cb_pred && gd->uav_pred && gd->srv_pred;
      }

      const float pred_scale = (pred_ok ? 2.f : 1.f);
      if (!gd->cb_smaa_metrics || gd->smaa_metrics_size != size || gd->smaa_metrics_pred_scale != pred_scale)
      {
         const float metrics[8] = {1.f / (float)size.x, 1.f / (float)size.y, (float)size.x, (float)size.y, pred_scale, 0.f, 0.f, 0.f};
         if (CreateImmutableCB(native_device, metrics, sizeof(metrics), std::addressof(gd->cb_smaa_metrics)))
         {
            gd->smaa_metrics_size = size;
            gd->smaa_metrics_pred_scale = pred_scale;
         }
      }
      if (!gd->cb_smaa_metrics)
         return DrawOrDispatchOverrideType::None;

      // Bind the metrics CB at VS+PS b1 (DrawSMAA restores VS/PS/SRVs/RTs but NOT cbuffer slots).
      ComPtr<ID3D11Buffer> vs_cb1_orig, ps_cb1_orig;
      native_device_context->VSGetConstantBuffers(1, 1, vs_cb1_orig.put());
      native_device_context->PSGetConstantBuffers(1, 1, ps_cb1_orig.put());
      ID3D11Buffer* mcb = gd->cb_smaa_metrics.get();
      native_device_context->VSSetConstantBuffers(1, 1, &mcb);
      native_device_context->PSSetConstantBuffers(1, 1, &mcb);

      // With sharpening on, SMAA renders into "tex_rcas_input" and RCAS writes the final RTV
      const bool do_sharpen = PrepareRCAS(native_device, device_data, gd, size);

      // Linear view depth -> plane-deviation edge-ness (R16F); see Luma_MEA_DepthExtract.hlsl
      if (pred_ok)
      {
         DrawStateStack<DrawStateStackType::Compute> pred_state;
         pred_state.Cache(native_device_context, device_data.uav_max_count);
         ID3D11ShaderResourceView* pred_srv = gd->srv_scene_depth.get();
         ID3D11UnorderedAccessView* pred_uav = gd->uav_pred.get();
         ID3D11Buffer* pred_cb = gd->cb_pred.get();
         native_device_context->CSSetShaderResources(0, 1, &pred_srv);
         native_device_context->CSSetUnorderedAccessViews(0, 1, &pred_uav, nullptr);
         native_device_context->CSSetConstantBuffers(0, 1, &pred_cb);
         native_device_context->CSSetShader(pred_cs, nullptr, 0);
         native_device_context->Dispatch((size.x + 7) / 8, (size.y + 7) / 8, 1);
         pred_state.Restore(native_device_context);
      }

      // MEA's FXAA input is already display-encoded → use it as both color and gamma
      ID3D11RenderTargetView* smaa_target = (do_sharpen ? gd->tex_rcas_input_rtv.get() : rtv.get());
      DrawSMAA(native_device, native_device_context, device_data, smaa_target, srv_color.get(), srv_color.get(), (pred_ok ? gd->srv_pred.get() : nullptr));
      MEA_COUNT(smaa_draws);

      if (do_sharpen)
      {
         MEA_COUNT(rcas_draws);
         DrawRCAS(native_device_context, device_data, gd, rtv.get(), size);
      }

      ID3D11Buffer* vcb = vs_cb1_orig.get();
      ID3D11Buffer* pcb = ps_cb1_orig.get();
      native_device_context->VSSetConstantBuffers(1, 1, &vcb);
      native_device_context->PSSetConstantBuffers(1, 1, &pcb);

      device_data.has_drawn_main_post_processing = true;
      return DrawOrDispatchOverrideType::Replaced; // cancel native FXAA
   }
#endif // ENABLE_SMAA

#if ENABLE_SMAA && ENABLE_SR
   // RCAS on top of DLSS/FSR, on the tonemap's output (see "kTonemapHash"): the tonemap draws into RCAS's input (RT1, its mask,
   // stays the game's), then RCAS writes the tonemap's own target
   static DrawOrDispatchOverrideType DrawTonemapWithRCAS(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, [[maybe_unused]] CommandListData& cmd_list_data,
      DeviceData& device_data, MassEffectAndromedaGameDeviceData* gd, const std::function<void()>& original_draw)
   {
      com_ptr<ID3D11RenderTargetView> rtvs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
      com_ptr<ID3D11DepthStencilView> dsv;
      native_device_context->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, &rtvs[0], &dsv);
      const uint2 size = (rtvs[0] ? GetViewTextureSize(rtvs[0].get()) : uint2{});
      if (size.x == 0 || size.y == 0 || !PrepareRCAS(native_device, device_data, gd, size))
         return DrawOrDispatchOverrideType::None;
      ID3D11RenderTargetView* tonemap_rtvs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = {};
      for (UINT i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
      {
         tonemap_rtvs[i] = rtvs[i].get();
      }
      tonemap_rtvs[0] = gd->tex_rcas_input_rtv.get();
      native_device_context->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, tonemap_rtvs, dsv.get());
      original_draw();
      native_device_context->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, &rtvs[0], dsv.get());
      MEA_COUNT(rcas_draws);
      DrawRCAS(native_device_context, device_data, gd, rtvs[0].get(), size);
      return DrawOrDispatchOverrideType::Replaced;
   }
#endif

#if ENABLE_SR
   // The hooked TAA resolve dispatch replaced with DLSS/FSR (or, for the DOF variant, run first and then overwritten by them)
   static DrawOrDispatchOverrideType ReplaceTAAResolve(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, [[maybe_unused]] CommandListData& cmd_list_data,
      DeviceData& device_data, MassEffectAndromedaGameDeviceData* gd, bool dof_variant, std::function<void()>* original_draw_dispatch_func)
   {
      SR::InstanceData* const sr_instance_data = (IsSRActive(device_data) ? device_data.GetSRInstanceData() : nullptr);
      if (!sr_instance_data)
         return DrawOrDispatchOverrideType::None;

      // Capture the TAA's CS bindings FIRST — we need the real input texture size (and the camera CB keyed to it)
      // before configuring SR or probing the camera.
      ComPtr<ID3D11ShaderResourceView> srv_depth, srv_mvs, srv_color;
      native_device_context->CSGetShaderResources(0, 1, srv_depth.put()); // t0
      native_device_context->CSGetShaderResources(1, 1, srv_mvs.put());   // t1
      native_device_context->CSGetShaderResources(3, 1, srv_color.put()); // t3
      ComPtr<ID3D11UnorderedAccessView> uav_resolved, uav_history;
      native_device_context->CSGetUnorderedAccessViews(2, 1, uav_resolved.put()); // u2
      native_device_context->CSGetUnorderedAccessViews(3, 1, uav_history.put());  // u3
      if (!srv_depth || !srv_mvs || !srv_color || !uav_resolved)
         return DrawOrDispatchOverrideType::None;

      ComPtr<ID3D11Resource> res_depth, res_mvs, res_color, res_u2;
      srv_depth->GetResource(res_depth.put());
      srv_mvs->GetResource(res_mvs.put());
      srv_color->GetResource(res_color.put());
      uav_resolved->GetResource(res_u2.put());

      // The REAL render res comes from the t3 scene-color texture, not core's swapchain-sized render_resolution —
      // under Resolution Scale they diverge and a wrong size breaks AA (camera-CB res-key mismatch + corner-only u2
      // copy). DLAA keeps render==output.
      const uint2 size = GetViewTextureSize(srv_color.get());
      if (size.x == 0 || size.y == 0)
         return DrawOrDispatchOverrideType::None;
      const uint32_t w = size.x, h = size.y;
      const float rw = (float)w, rh = (float)h;
      gd->scene_size = size;

      // Camera was normally captured at the gbuffer VS this frame. Only fall back to the bound-CB probe if that missed
      // (e.g. a frame with no main gbuffer pass).
      if (ID3D11DeviceContext1* context1 = (gd->cam_valid_this_frame ? nullptr : GetImmediateContext1(native_device_context, gd)))
      {
         // Guarded: it reads the map cache, which Frostbite worker threads mutate via OnMapBufferRegion.
         const std::shared_lock lock(gd->map_cache.mutex);
         if (!gd->cam_probe_slot || !TryStoreCamera(context1, gd, gd->cam_probe_slot->compute, gd->cam_probe_slot->slot, &size))
         {
            // Slots 2/1/3 first (the observed camera slots), then the whole CB range — frames without a main
            // gbuffer pass (menus) bind the per-view CB at less common slots, if at all. TryStoreCamera's
            // validation (layout signature + render-res match) rejects non-camera CBs, so the wide scan is safe.
            static constexpr UINT kProbeSlots[] = {2, 1, 3, 0, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13};
            for (UINT slot : kProbeSlots)
            {
               if (TryStoreCamera(context1, gd, false, slot, &size))
               {
                  gd->cam_probe_slot = MassEffectAndromedaGameDeviceData::CameraSlot{.compute = false, .slot = slot};
                  break;
               }
               if (TryStoreCamera(context1, gd, true, slot, &size))
               {
                  gd->cam_probe_slot = MassEffectAndromedaGameDeviceData::CameraSlot{.compute = true, .slot = slot};
                  break;
               }
            }
         }
         if (gd->cam_valid_this_frame)
         {
            MEA_COUNT(camera_probe);
         }
      }
      // No camera this frame -> no jitter info. Running SR with jitter=0 while the scene IS jittered leaves the jitter
      // uncompensated -> visible per-frame camera shake (menus / dialogue frames without a main gbuffer pass). Bail to
      // the native resolve — it de-jitters itself. Must stay BEFORE the DOF-variant manual dispatch below, so None
      // still means "the native dispatch runs exactly once, by the fall-through".
      if (!gd->cam_valid_this_frame)
      {
         MEA_COUNT(camera_misses);
         return DrawOrDispatchOverrideType::None;
      }

      // The hand-off into u2/u3 is the copy CS (see Luma_MEA_CopyColor.hlsl): the game's "Buffer Format" setting swaps them
      // between rgba16f and r11g11b10_float, where a plain copy silently no-ops, and u3 needs the native history encoding.
      // Without it (shaders still compiling, or u2 of another size) the native resolve runs. Checked every dispatch: the
      // transient pool can recycle a pointer for a texture of another format, so a pointer key would go stale.
      auto* copy_cs = FindShader(device_data.native_compute_shaders, CompileTimeStringHash("MEA SR Output Copy CS"));
      auto* history_cs = FindShader(device_data.native_compute_shaders, CompileTimeStringHash("MEA SR History Copy CS"));
      D3D11_TEXTURE2D_DESC u2_desc = {};
      if (ComPtr<ID3D11Texture2D> u2_tex; SUCCEEDED(res_u2->QueryInterface(u2_tex.put())))
      {
         u2_tex->GetDesc(&u2_desc);
      }
      if (u2_desc.Format == DXGI_FORMAT_R11G11B10_FLOAT)
      {
         MEA_COUNT(u2_r11g11b10);
      }
      if (copy_cs == nullptr || history_cs == nullptr || u2_desc.Width != w || u2_desc.Height != h)
      {
         MEA_COUNT(handoff_incompatible);
         return DrawOrDispatchOverrideType::None;
      }

      // DOF variant: the native dispatch is manually re-issued here (its u4/u5 CoC outputs feed the DOF chain); from
      // then on every exit must return Replaced, never None — the fall-through dispatch would run the resolve twice.
      if (dof_variant)
      {
         if (original_draw_dispatch_func == nullptr)
            return DrawOrDispatchOverrideType::None; // can't re-issue manually — let the native dispatch run itself
         // Run the game's own resolve first: it writes u0 (mask) + u4/u5 (temporally filtered DOF CoC) with native
         // math — zero quality loss. Its u2/u3 color output is overwritten by our SR copy below; the SR inputs
         // (t0/t1/t3) are not mutated by it and stay bound across the dispatch.
         (*original_draw_dispatch_func)();
         MEA_COUNT(dof_resolves);
      }
      // Back to the native TAA until the upscaler is picked again
      const auto suppress_sr = [&]
      {
         MEA_COUNT(sr_failures);
         device_data.sr_suppressed = true;
         return (dof_variant ? DrawOrDispatchOverrideType::Replaced : DrawOrDispatchOverrideType::None);
      };

      // (Re)create the output texture at the scene size
      D3D11_TEXTURE2D_DESC output_desc = {};
      if (device_data.sr_output_color)
      {
         device_data.sr_output_color->GetDesc(&output_desc);
      }
      const bool output_resized = output_desc.Width != w || output_desc.Height != h;
      if (output_resized || !gd->sr_output_srv)
      {
         gd->sr_output_srv.reset();
         device_data.sr_output_color.reset();
         CreateTexture(native_device, DXGI_FORMAT_R16G16B16A16_FLOAT, size, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, &device_data.sr_output_color,
            gd->sr_output_srv.put());
         gd->sr_output_recreated = true;
      }
      if (!gd->sr_output_srv)
         return suppress_sr();

      const SR::SettingsData settings_data = {
         .output_width = w,
         .output_height = h,
         .render_width = w,
         .render_height = h,
         .hdr = true,            // MEA scene color = linear HDR
         .inverted_depth = true, // reverse-Z
         .mvs_jittered = g_mv_jittered,
         .mvs_x_scale = (g_mv_flip_x ? -1.f : 1.f) * g_mv_scale_mult * 0.5f * rw,
         .mvs_y_scale = (g_mv_flip_y ? 1.f : -1.f) * g_mv_scale_mult * 0.5f * rh,
         .auto_exposure = (device_data.sr_type != SR::Type::FSR),
         .render_preset = dlss_render_preset,
      };
      sr_implementations[device_data.sr_type]->UpdateSettings(sr_instance_data, native_device_context, settings_data);

      // No bias mask: MEA has no usable reactivity source. Jitter is this frame's: the capture-miss case bailed above.
      const SR::SuperResolutionImpl::DrawData draw_data = {
         // Smooth FOV changes are left to the upscaler's own history rejection: resetting every ramp frame starves it.
         .reset = device_data.force_reset_sr || output_resized,
         .output_color = device_data.sr_output_color.get(),
         .source_color = res_color.get(),
         .motion_vectors = res_mvs.get(),
         .depth_buffer = res_depth.get(),
         .render_width = w,
         .render_height = h,
         .jitter_x = (g_jitter_flip_x ? 1.f : -1.f) * gd->cam_jitter_clip_x * 0.5f * rw,
         .jitter_y = (g_jitter_flip_y ? 1.f : -1.f) * gd->cam_jitter_clip_y * 0.5f * rh,
         // FSR consumes vert FOV (DLSS ignores) and HARD-ASSERTS on ≤0 → fall back to ~60° when m11==0
         // (first SR frames, pre-capture; that frame is a reset anyway).
         .vert_fov = (gd->cam_proj_m11 > 0.f ? (2.f * atanf(1.f / gd->cam_proj_m11)) : 1.047f),
         .near_plane = gd->cam_near,
         .far_plane = kCamFar,
         .frame_index = cb_luma_global_settings.FrameIndex,
      };

      // NGX/FFX don't restore the state they touch
      DrawStateStack<DrawStateStackType::FullGraphics> graphics_state;
      DrawStateStack<DrawStateStackType::Compute> compute_state;
      graphics_state.Cache(native_device_context, device_data.uav_max_count);
      compute_state.Cache(native_device_context, device_data.uav_max_count);

      const bool drawn = sr_implementations[device_data.sr_type]->Draw(sr_instance_data, native_device_context, draw_data);
      if (drawn)
      {
         MEA_COUNT(handoff_cs);
         native_device_context->CSSetShader(copy_cs, nullptr, 0);
         // UAV first, then SRV (an SRV whose resource is still UAV-bound gets silently NULLed). The FULL range: besides the
         // game's own u2/u3 bindings, the SR backend may have left sr_output_color on any UAV slot.
         ID3D11UnorderedAccessView* uavs[D3D11_1_UAV_SLOT_COUNT] = {uav_resolved.get()};
         native_device_context->CSSetUnorderedAccessViews(0, device_data.uav_max_count, uavs, nullptr);
         ID3D11ShaderResourceView* srv = gd->sr_output_srv.get();
         native_device_context->CSSetShaderResources(0, 1, &srv);
         native_device_context->Dispatch((w + 7) / 8, (h + 7) / 8, 1);
         if (uav_history)
         {
            native_device_context->CSSetShader(history_cs, nullptr, 0); // the native history encoding, see the shader
            ID3D11UnorderedAccessView* uav_h = uav_history.get();
            native_device_context->CSSetUnorderedAccessViews(0, 1, &uav_h, nullptr);
            native_device_context->Dispatch((w + 7) / 8, (h + 7) / 8, 1);
         }
      }
      compute_state.Restore(native_device_context);
      graphics_state.Restore(native_device_context);

      if (!drawn)
         return suppress_sr();
      // DLSS draws nothing into a new output texture (the session's first, or one made after "None", which Core frees): the frame
      // shows the texture's stale memory until its feature is created again after a draw. Settings changed once here force that
      // at the next frame's "UpdateSettings".
      if (std::exchange(gd->sr_output_recreated, false) && device_data.sr_type == SR::Type::DLSS)
      {
         SR::SettingsData throwaway_settings_data = settings_data;
         throwaway_settings_data.mvs_jittered = !throwaway_settings_data.mvs_jittered;
         sr_implementations[device_data.sr_type]->UpdateSettings(sr_instance_data, native_device_context, throwaway_settings_data);
      }
      MEA_COUNT(sr_draws);
      device_data.has_drawn_sr = true;
      device_data.has_drawn_main_post_processing = true;
      return DrawOrDispatchOverrideType::Replaced; // cancel native TAA
   }
#endif // ENABLE_SR

public:
   // "restore_engine_settings": the process outlives the addon (an unload), not at process exit, where the engine's memory
   // may already be freed
   static void UnregisterEvents(bool restore_engine_settings)
   {
      reshade::unregister_event<reshade::addon_event::map_buffer_region>(OnMapBufferRegion);
      reshade::unregister_event<reshade::addon_event::destroy_resource>(OnDestroyResource);
      if (restore_engine_settings && g_world_render_settings)
      {
         WriteWorldRenderSettings(g_world_render_settings_vanilla);
      }
   }

   void OnInit(bool async) override
   {
#if DEVELOPMENT
      // For the MCP "luma_dev_values" tool (the counters are the last complete frame's)
      Mcp::RegisterToggles({
         {"smaa_enable", &g_smaa_enable},
         {"smaa_predication", &g_smaa_predication},
         {"mv_flip_x", &g_mv_flip_x},
         {"mv_flip_y", &g_mv_flip_y},
         {"mv_jittered", &g_mv_jittered},
         {"jitter_flip_x", &g_jitter_flip_x},
         {"jitter_flip_y", &g_jitter_flip_y},
         {"disable_taa_sharpening", &g_disable_taa_sharpening},
         {"halton_jitter", &g_halton_jitter},
         {"improve_taa_jitter", &g_improve_taa_jitter},
      });
      Mcp::RegisterInts({{"halton_jitter_native_phases", &g_halton_jitter_native_phases, 1, 64}});
      Mcp::RegisterValues({{"rcas_sharpness", &g_rcas_sharpness, 0.f, 1.f}, {"smaa_pred_tolerance", &g_smaa_pred_tolerance, 0.002f, 0.2f}, {"mv_scale_mult", &g_mv_scale_mult, 0.25f, 4.f}});
      const FrameCounters& c = g_counters_last_frame;
      Mcp::RegisterCounters({
         {"taa.resolves", &c.taa_resolves},
         {"taa.resolves_deferred", &c.taa_resolves_deferred},
         {"taa.dof_resolves", &c.dof_resolves},
         {"camera.gbuffer", &c.camera_gbuffer},
         {"camera.scene_vs", &c.camera_scene_vs},
         {"camera.probe", &c.camera_probe},
         {"camera.misses", &c.camera_misses},
         {"handoff.cs", &c.handoff_cs},
         {"handoff.incompatible", &c.handoff_incompatible},
         {"handoff.u2_r11g11b10", &c.u2_r11g11b10},
         {"sr.draws", &c.sr_draws},
         {"sr.failures", &c.sr_failures},
         {"fxaa.draws", &c.fxaa_draws},
         {"smaa.draws", &c.smaa_draws},
         {"rcas.draws", &c.rcas_draws},
      });
#if ENABLE_SR
      Mcp::RegisterTextures({MCP_GAME_TEXTURE("sr.output", sr_output_srv)});
#endif
      Mcp::RegisterTextures({MCP_GAME_TEXTURE("rcas.input", tex_rcas_input_srv), MCP_GAME_TEXTURE("smaa.predication", srv_pred)});
#endif
      // Luma's own shaders read none of its CBs → disable all three (-1 = unused).
      luma_settings_cbuffer_index = -1;
      luma_data_cbuffer_index = -1;
      luma_ui_cbuffer_index = -1;
#if ENABLE_SMAA
      // RCAS sharpening PS for the SMAA output (reuses core's "Copy VS" fullscreen vertex shader).
      native_shaders_definitions.emplace(CompileTimeStringHash("MEA Sharpen PS"),
         ShaderDefinition{"Luma_RCAS_PS", reshade::api::pipeline_subobject_type::pixel_shader, nullptr, "sharpen_ps"});
      native_shaders_definitions.emplace(CompileTimeStringHash("MEA Depth Extract CS"),
         ShaderDefinition{"Luma_MEA_DepthExtract", reshade::api::pipeline_subobject_type::compute_shader});
#endif
#if ENABLE_SR
      // The SR output hand-off into the resolve's u2/u3 (see Luma_MEA_CopyColor.hlsl)
      native_shaders_definitions.emplace(CompileTimeStringHash("MEA SR Output Copy CS"),
         ShaderDefinition{"Luma_MEA_CopyColor", reshade::api::pipeline_subobject_type::compute_shader, nullptr, "copy_color_cs"});
      native_shaders_definitions.emplace(CompileTimeStringHash("MEA SR History Copy CS"),
         ShaderDefinition{"Luma_MEA_CopyColor", reshade::api::pipeline_subobject_type::compute_shader, nullptr, "copy_color_history_cs"});
#endif
      // Cache the camera ring-buffer's CPU map pointer (read at the gbuffer draw / TAA dispatch by bound offset) —
      // replaces a per-frame GPU readback stall. Must run in all configs.
      reshade::register_event<reshade::addon_event::map_buffer_region>(OnMapBufferRegion);
      reshade::register_event<reshade::addon_event::destroy_resource>(OnDestroyResource);
   }

   void OnCreateDevice(ID3D11Device* native_device, DeviceData& device_data) override
   {
      device_data.game = new MassEffectAndromedaGameDeviceData;
   }

   void OnDestroyDeviceData(DeviceData& device_data) override
   {
      // GameDeviceData has no virtual destructor: delete through the derived type so its members are released.
      delete static_cast<MassEffectAndromedaGameDeviceData*>(device_data.game);
      device_data.game = nullptr;
   }

   DrawOrDispatchOverrideType OnDrawOrDispatch(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, CommandListData& cmd_list_data, DeviceData& device_data, reshade::api::shader_stage stages, const ShaderHashesList<OneShaderPerPipeline>& original_shader_hashes, bool is_custom_pass, bool& updated_cbuffers, std::function<void()>* original_draw_dispatch_func) override
   {
      auto& gd = GetGameDeviceData(device_data);

      // OnDrawOrDispatch also fires on Frostbite deferred contexts (worker threads); GetType() is invariant for this
      // call, so evaluate it once. All immediacy gates below use this.
      const bool is_immediate = native_device_context->GetType() == D3D11_DEVICE_CONTEXT_IMMEDIATE;

      if (is_immediate && !gd.cam_valid_this_frame && (stages & reshade::api::shader_stage::vertex) != 0)
      {
         CaptureCamera(native_device_context, cmd_list_data, &gd, original_shader_hashes);
      }

#if DEVELOPMENT
      if (original_shader_hashes.Contains(kFXAAHash, reshade::api::shader_stage::pixel))
      {
         MEA_COUNT(fxaa_draws);
      }
#endif
#if ENABLE_SMAA
      // Only while SMAA runs: it ran last frame (the first FXAA frame goes without predication)
      if (g_smaa_enable && g_smaa_predication && is_immediate && cb_luma_global_settings.FrameIndex - gd.smaa_frame <= 1 &&
          original_shader_hashes.Contains(kLinearDepthHash, reshade::api::shader_stage::pixel))
      {
         CaptureLinearDepth(native_device, native_device_context, &gd);
      }
      if (g_smaa_enable && is_immediate && original_shader_hashes.Contains(kFXAAHash, reshade::api::shader_stage::pixel) && HasSMAAShaders(device_data))
      {
         return ReplaceFXAAWithSMAA(native_device, native_device_context, cmd_list_data, device_data, &gd);
      }
#endif

#if ENABLE_SMAA && ENABLE_SR
      // "has_drawn_sr" is this frame's (reset at present)
      if (g_rcas_sharpness > 0.f && device_data.has_drawn_sr && is_immediate && original_draw_dispatch_func && *original_draw_dispatch_func &&
          original_shader_hashes.Contains(kTonemapHash, reshade::api::shader_stage::pixel))
      {
         return DrawTonemapWithRCAS(native_device, native_device_context, cmd_list_data, device_data, &gd, *original_draw_dispatch_func);
      }
#endif

      const bool dof_variant = original_shader_hashes.Contains(shader_hashes_taa_resolve_dof);
      if (!dof_variant && !original_shader_hashes.Contains(shader_hashes_taa_resolve))
         return DrawOrDispatchOverrideType::None;

      device_data.taa_detected = true; // game's TAA pass present (feeds core's SR-engaged ✓ indicator); latched, never reset
      MEA_COUNT(taa_resolves);
      if (!is_immediate)
      {
         MEA_COUNT(taa_resolves_deferred);
         return DrawOrDispatchOverrideType::None; // SR can't run on a deferred context: native TAA
      }
#if ENABLE_SR
      return ReplaceTAAResolve(native_device, native_device_context, cmd_list_data, device_data, &gd, dof_variant, original_draw_dispatch_func);
#else
      return DrawOrDispatchOverrideType::None;
#endif
   }

#if ENABLE_SR
   void CleanExtraSRResources(DeviceData& device_data) override
   {
      GetGameDeviceData(device_data).release_sr_resources = true;
   }
#endif

   void OnPresent(ID3D11Device* native_device, DeviceData& device_data) override
   {
      auto& gd = GetGameDeviceData(device_data);

#if ENABLE_SR
      // The upscaler's history restarts after any frame it didn't draw (menus, loading, FXAA mode, just picked)
      gd.sr_active = LatchSRFrame(device_data);
#endif
      UpdateWorldRenderSettings(device_data);
#if ENABLE_SR
      // None picked: Core freed its upscaler resources and output, ours go too (recreated when an upscaler is picked again)
      if (device_data.sr_type == SR::Type::None && gd.release_sr_resources.exchange(false))
      {
         gd.sr_output_srv.reset();
      }
#endif

#if ENABLE_SMAA
      // SMAA's resources go once it stopped running (TAA mode, or it's off), RCAS's once it stopped as well
      if (cb_luma_global_settings.FrameIndex - gd.smaa_frame > smaa_idle_release_frames)
      {
         gd.cb_smaa_metrics.reset();
         gd.ReleasePredication();
         ReleaseSMAA(device_data);
      }
      else if (!g_smaa_predication && gd.tex_pred)
      {
         gd.ReleasePredication();
      }
      if (gd.tex_rcas_input && cb_luma_global_settings.FrameIndex - gd.sharpen_frame > smaa_idle_release_frames)
      {
         gd.ReleaseRCAS();
      }
#endif

      // -1 at native resolution (Core biases the game's anisotropic samplers, all upgraded to AF16x); vanilla without SR
#if ENABLE_SR
      const float mip_lod_bias = (IsSRActive(device_data) ? SR::GetMipLODBias(device_data.output_resolution.y, device_data.output_resolution.y) : 0.f);
#else
      constexpr float mip_lod_bias = 0.f;
#endif
      if (enable_samplers_upgrade && !custom_texture_mip_lod_bias_offset && device_data.texture_mip_lod_bias_offset != mip_lod_bias)
      {
         // Core reads the offset as a sampler map key under a shared lock on Frostbite worker threads
         const std::unique_lock lock(s_mutex_samplers);
         device_data.texture_mip_lod_bias_offset = mip_lod_bias;
      }

      gd.cam_valid_this_frame = false;
#if DEVELOPMENT
      g_counters_last_frame = std::exchange(g_counters_this_frame, {});
#endif
      {
         const std::unique_lock lock(gd.map_cache.mutex);
         gd.map_cache.count = 0; // re-capture ring ptr next frame
      }
   }

   // Persist the user settings across launches (ReShade config section NAME="Luma"). Called on boot.
   void LoadConfigs() override
   {
      reshade::get_config_value(nullptr, NAME, "SMAAEnable", g_smaa_enable);
      // "SMAASharpness" is the key before the rename: still read so existing values carry over
      reshade::get_config_value(nullptr, NAME, "SMAASharpness", g_rcas_sharpness);
      reshade::get_config_value(nullptr, NAME, "RCASSharpness", g_rcas_sharpness);
      reshade::get_config_value(nullptr, NAME, "SMAAPredication", g_smaa_predication);
      reshade::get_config_value(nullptr, NAME, "SMAAPredicationTolerance", g_smaa_pred_tolerance);
      reshade::get_config_value(nullptr, NAME, "DisableTAASharpening", g_disable_taa_sharpening);
      reshade::get_config_value(nullptr, NAME, "ImproveTAAJitter", g_improve_taa_jitter);
   }

   void DrawImGuiSettings(DeviceData& device_data) override
   {
#if ENABLE_SR
      const bool sr_selected = device_data.sr_type != SR::Type::None && !device_data.sr_suppressed;
#else
      constexpr bool sr_selected = false;
#endif
      ImGui::SeparatorText("Anti-Aliasing");
      if (ImGui::Checkbox("SMAA Enable", &g_smaa_enable))
      {
         reshade::set_config_value(nullptr, NAME, "SMAAEnable", g_smaa_enable);
      }
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
      {
         ImGui::SetTooltip("Replaces the game's FXAA with SMAA (only active when in-game Anti-Aliasing is set to FXAA).");
      }
      DrawResetButton(g_smaa_enable, kDefaultSmaaEnable, "SMAAEnable");
      ImGui::BeginDisabled(!g_smaa_enable && !sr_selected);
      ImGui::SliderFloat("RCAS Sharpness", &g_rcas_sharpness, 0.f, 1.f);
      if (ImGui::IsItemDeactivatedAfterEdit())
      {
         reshade::set_config_value(nullptr, NAME, "RCASSharpness", g_rcas_sharpness);
      }
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
      {
         ImGui::SetTooltip("Sharpening applied on top of SMAA or DLSS/FSR (0 = off).");
      }
      DrawResetButton(g_rcas_sharpness, kDefaultRcasSharpness, "RCASSharpness");
      ImGui::EndDisabled();
#if DEVELOPMENT || TEST
      ImGui::BeginDisabled(!g_smaa_enable);
      if (ImGui::Checkbox("SMAA Predication", &g_smaa_predication))
      {
         reshade::set_config_value(nullptr, NAME, "SMAAPredication", g_smaa_predication);
      }
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
      {
         ImGui::SetTooltip("Finds edges by geometry (scene depth) as well as by brightness.\nKeeps textures sharp while still antialiasing real silhouettes.");
      }
      DrawResetButton(g_smaa_predication, kDefaultSmaaPredication, "SMAAPredication");
      if (ImGui::SliderFloat("SMAA Predication Tolerance", &g_smaa_pred_tolerance, 0.002f, 0.2f, "%.3f", ImGuiSliderFlags_Logarithmic))
      {
         reshade::set_config_value(nullptr, NAME, "SMAAPredicationTolerance", g_smaa_pred_tolerance);
      }
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
      {
         ImGui::SetTooltip("How far a surface may deviate from its local plane before it counts as an edge,\nas a fraction of view depth. Lower = more edges.");
      }
      DrawResetButton(g_smaa_pred_tolerance, kDefaultSmaaPredTolerance, "SMAAPredicationTolerance");
      ImGui::EndDisabled();
#endif

      ImGui::SeparatorText("Fixes");
      // SR replaces the TAA resolve (no built-in sharpening) and uses its own Halton jitter: both fixes are shown as always on
      ImGui::BeginDisabled(sr_selected);
      bool disable_taa_sharpening = g_disable_taa_sharpening || sr_selected;
      if (ImGui::Checkbox("Disable Native TAA Sharpening", &disable_taa_sharpening))
      {
         reshade::set_config_value(nullptr, NAME, "DisableTAASharpening", disable_taa_sharpening);
      }
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
      {
         ImGui::SetTooltip("The game's TAA sharpens its output in tiles, which boosts leftover noise and leaves a faint grid. This turns it off, for a cleaner image.\nDLSS / FSR never use it.");
      }
      DrawResetButton(disable_taa_sharpening, kDefaultDisableTaaSharpening, "DisableTAASharpening");
      bool improve_taa_jitter = g_improve_taa_jitter || sr_selected;
      if (ImGui::Checkbox("Fix Native TAA Jitter", &improve_taa_jitter))
      {
         reshade::set_config_value(nullptr, NAME, "ImproveTAAJitter", improve_taa_jitter);
      }
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
      {
         ImGui::SetTooltip("The game's TAA samples each pixel in a clumped pattern, so still images flicker. This spreads the samples evenly, for a steadier image.\nDLSS / FSR always use their own fix.");
      }
      DrawResetButton(improve_taa_jitter, kDefaultImproveTaaJitter, "ImproveTAAJitter"); // Hidden while forced on
      if (!sr_selected)
      {
         g_disable_taa_sharpening = disable_taa_sharpening;
         g_improve_taa_jitter = improve_taa_jitter;
      }
      ImGui::EndDisabled();
   }

#if DEVELOPMENT
   void DrawImGuiDevSettings(DeviceData& device_data) override
   {
      ImGui::SeparatorText("Motion vectors");
      ImGui::Checkbox("MV flip X", &g_mv_flip_x);
      ImGui::Checkbox("MV flip Y", &g_mv_flip_y);
      ImGui::SliderFloat("MV scale mult", &g_mv_scale_mult, 0.25f, 4.0f);
      ImGui::Checkbox("MVs already jittered", &g_mv_jittered);

      ImGui::SeparatorText("Jitter");
      ImGui::Checkbox("Jitter flip X", &g_jitter_flip_x);
      ImGui::Checkbox("Jitter flip Y", &g_jitter_flip_y);
      ImGui::Checkbox("Halton jitter with SR", &g_halton_jitter);
      ImGui::SliderInt("Halton phases with the game's TAA", &g_halton_jitter_native_phases, 1, 64);
   }
#endif // DEVELOPMENT

   void PrintImGuiAbout() override
   {
      ImGui::PushTextWrapPos(0.f);
      ImGui::Text(
         "Luma for \"Mass Effect: Andromeda\" is developed by DristoforColumb and is open source and free.\n"
         "It adds DLAA or FSR 3 native anti-aliasing, and replaces the game's FXAA with SMAA, plus 16x anisotropic filtering.\n"
         "Set Anti-Aliasing to TAA in the game's video settings for DLAA and FSR 3 to apply, or to FXAA for SMAA.\n"
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
                  "\nSMAA (Iryoku)"
                  "\nAMD FidelityFX (RCAS + FSR 3)"
                  "\nNVIDIA NGX (DLSS)");
   }
};

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
   if (ul_reason_for_call == DLL_PROCESS_ATTACH)
   {
      Globals::SetGlobals(PROJECT_NAME, "Mass Effect: Andromeda Luma mod", "", 2);
      Globals::DEVELOPMENT_STATE = Globals::ModDevelopmentState::Finished;

      // AA-only: leave HDR / swapchain / texture-format handling alone.
      swapchain_format_upgrade_type = TextureFormatUpgradesType::None;
      swapchain_upgrade_type = SwapchainUpgradeType::None;
      texture_format_upgrades_type = TextureFormatUpgradesType::None;

      // AA-only must NOT touch the display: core's per-present display-composition encode pass otherwise darkens
      // the backbuffer even with no upgrades.
      force_disable_display_composition = true;

      // Sharper textures: AF16x + negative mip LOD bias (mode 4 = AF16x + additive bias). The bias value is set
      // per-frame by the active AA mode in OnPresent.
      enable_samplers_upgrade = true; // boot-time only
      samplers_upgrade_mode = 4;

      game = new MassEffectAndromeda();
   }
   else if (ul_reason_for_call == DLL_PROCESS_DETACH)
   {
      // We registered these in OnInit; unregister so a reload doesn't double-register / dangle. A null "lpReserved" is an unload.
      MassEffectAndromeda::UnregisterEvents(lpReserved == nullptr);
   }

   CoreMain(hModule, ul_reason_for_call, lpReserved);

   return TRUE;
}
