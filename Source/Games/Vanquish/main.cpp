#define GAME_VANQUISH 1

#define GEOMETRY_SHADER_SUPPORT 0
#define DISABLE_AUTO_DEBUGGER 1
// The velocity passes get their 8-bit clamp back and every other pass its blend input clamp (see "PatchShaderBytecodeSync")
#define LUMA_PATCH_BYTECODE_SYNC 1
// Outside DEVELOPMENT only this define makes original_draw_dispatch_func non-null (the gather pass is drawn, then snapshotted)
#define ENABLE_POST_DRAW_DISPATCH_CALLBACK 1
// The motion vector draw key reads "last_draw_dispatch_data", which Publishing only fills with this
#define ENABLE_DRAW_DISPATCH_DATA_CACHE 1
// SMAA in the game's AA slot after the gather pass (MGR:R's)
#define ENABLE_SMAA 1
// SMAA's area texture without the U-shape smoothing (see Luma_SMAA_impl.hlsl)
#define SMAA_SMOOTH_U_SHAPES 0
// RCAS sharpening after SMAA, opt-in (MGR:R's)
#define ENABLE_RCAS 1
// Luma TAA among the upscalers (in process; DLSS, FSR and XeSS run in the SR bridge's x64 helper, "UseLumaSRBridge")
#define ENABLE_LUMA_TAA 1

#include "..\..\Core\core.hpp"
#include "MotionVectorPatches.h"
#include "FrameRateFixes.h"
#include "..\..\Core\includes\patched_draws.h"

// Vanquish (PC, Steam, exe link time 2017-07-31) is x86 D3D9 and runs through dgVoodoo (D3D9 -> D3D11), so every hash below is a
// dgVoodoo 2.87.6 translation (ps_5_0) from the offline map of data000.cpk's shader.dat/shader2.dat and of the exe's embedded shaders.
// dgVoodoo translates each pixel shader once per alpha test function in use, so the lists hold all three. They also hold the
// dgVoodoo 2.81.3 twins (ps_4_0, the Linux package's build), paired offline by alpha test and opcodes.
// The frame, as seen in game: the scene in 8-bit targets, FilterShaderGather (grade + bloom) into a scene sized target, the game's
// FXAA or edge AA (the exe's own shaders, or a dgVoodoo copy with the game's AA off) into dgVoodoo's D3D9 back buffer (the canvas),
// the UI on it, then dgVoodoo's present. The replacement of FilterShaderGather does the HDR display map.
const ShaderHashesList shader_hashes_Gather = {.pixel_shaders = {0xBC9B20C2, 0xE004F806, 0x021D5E4D, 0x48FFD7B4, 0xEB2A1BAE, 0xB3C88B24}};
// What writes the canvas after the gather pass: the game's FXAA (exe_007) or edge AA 2x / 4x / 8x (exe_004 / 005 / 006, the game's
// "MSAA" option), a plain copy in their place once SMAA ran on their input, else dgVoodoo's copy with the game's AA off
const ShaderHashesList shader_hashes_GameAA = {.pixel_shaders = {0x15857C80, 0xB39B8ED0, 0xEB476A79, 0x32A44A9C, 0x3341D6E4, 0x491F4CBE, 0xC674EE68, 0x4434D070, 0xAE5DF7AE, 0x85CECC90, 0x75FCCD11, 0xDF16B483, 0x7C74E55F, 0x315CE602, 0x5F4256CE, 0xB2602160, 0x49BC8BE9, 0xAC49314B, 0x9440733F, 0x3B8132DA, 0x2A9954D3, 0xC8AAB307, 0x11B65982, 0xB3D9035C}};
// dgVoodoo 2.81.3 has no twin: with the game's AA off its gather pass draws straight into the canvas (seen in game)
const ShaderHashesList shader_hashes_AAOffCopy = {.pixel_shaders = {0xE9A53CDA}};
// The depth prepass (zmapscr / zmapscrnki, seen in game): linear view depth into a scene sized R32F target that SSAO, DoF and the soft
// particles read. It is also SMAA's predication source.
const ShaderHashesList shader_hashes_DepthPrepass = {.pixel_shaders = {0x5F3E9D55, 0xBA502672, 0x6C46DB88, 0x64549F7E}};
// dgVoodoo's present blit of the canvas into the swapchain (MGR:R's and Bayonetta's hash), drawn once the swapchain is upgraded
const ShaderHashesList shader_hashes_DgVoodooPresent = {.pixel_shaders = {0x880A17D3}};
// The movies: Sofdec decodes on the CPU, dgVoodoo unpacks the uploaded frame (its R32 UINT texture) into an R8G8B8A8 target, then the
// game's "filtershadercopytex" draws that into the final target. The copy also runs in the bloom chain and after the gather pass, so a
// movie is the copy that reads the unpack's output.
const ShaderHashesList shader_hashes_VideoUnpack = {.pixel_shaders = {0x471059BE}};
const ShaderHashesList shader_hashes_Copy = {.pixel_shaders = {0x91A6C437, 0x7584B641}};
// The bloom chain, all into the fixed size 320x176 bloom target (seen in game at 4K), each draw followed by a dgVoodoo copy of the
// written region into a texture of its size that the next pass samples: filter05 (the scene into level 0, then each level into the
// next: 160x80, 80x48, 48x32, 32x16), filter02 / filter01 (each level's V / H blur), then the copy shader drawing the levels into
// level 0 (the first opaque, the 80x48, 48x32 and 32x16 ones SRC_ALPHA / ONE), which the gather pass reads as t1.
const ShaderHashesList shader_hashes_BloomSource = {.pixel_shaders = {0x8B6A8D61, 0x9C312037, 0xAD095D6B, 0x29FE50C2, 0x2F68CC26, 0x2A79AB5C}};
const ShaderHashesList shader_hashes_BloomBlur = {.pixel_shaders = {0x3B0C76B0, 0xCC2E3748, 0x8D9CA7F2, 0x55E02134, 0xCCB5D7CF, 0x383F18C2, 0x918D9D54, 0x36CCFFDC, 0x3901D0F8, 0xE4C43DF5, 0x92DAACE5, 0xC51AC86F}};
// Every list above "OnDrawOrDispatch" tests: a draw in none of them (most of the scene) skips their lookups
const ShaderHashesList shader_hashes_Listed = []
{
   ShaderHashesList<> listed;
   for (const ShaderHashesList<>* list : {&shader_hashes_Gather, &shader_hashes_GameAA, &shader_hashes_AAOffCopy, &shader_hashes_DepthPrepass, &shader_hashes_DgVoodooPresent, &shader_hashes_VideoUnpack, &shader_hashes_Copy, &shader_hashes_BloomSource, &shader_hashes_BloomBlur})
   {
      listed.pixel_shaders.insert(list->pixel_shaders.begin(), list->pixel_shaders.end());
   }
   return listed;
}();
// The motion blur's velocity: (ndc - previous ndc) * scale + 0.5 in o0.xy, which the 8-bit target clamped and quantized
// (modelshadervelocitymappl, mapscr, nomovevelocitymap, directionvelocitymap), its dilation (blurpass2) and downsampling
// (modelshaderdownsampling, only seen on the velocity). Clamped as it stored it now that the scene sized and shaped targets are fp16.
const std::unordered_set<uint32_t> velocity_pixel_shaders = {
   0x38A99388, 0x25928798, 0x9A548B81, 0x004FC83F, 0x6A31B7CC, 0xD7A426D5, 0x33EC7BFF, 0x92C62DC9, 0xFBEBED66, 0x168CA31F, 0x84DD0B27, 0xB7198BCC,
   0x853E8BF8, 0x423D2F0D, 0x97F3C1F4, 0x5EE654CA, 0x5EAF888B, 0xD00F7593, 0xF4393B3B, 0x250BB857, 0x3166B668, 0x77858A85, 0x73C8073C, 0x80E0122D,
   0x4FF9C639, 0xB3D64BF4, 0xD6B4AD11, 0xFEF7E743, 0xAD422EAD, 0xD19830D7, 0xBECA1D05, 0xD95F9759, 0x178EC9F2, 0xF696E494, 0x42338E8B, 0xD8EE9774};
// FSR's and Luma TAA's masks, written by the scene's alpha blended draws past dgVoodoo's targets (their pixel shaders patched with
// "PatchPixelShaderReactive", the slot max blended in every blend state: "PatchedDraws::OnCreateBlendState")
constexpr UINT REACTIVE_SLOT = MotionVectorPatches::reactive_slot;
using SceneMotionVectors::PixelShaderPatch;

static bool g_smaa_enable = true;
static bool g_smaa_t2x = false;         // SMAA T2x: a two phase jitter and the previous frame, through the upscalers' motion vectors
static float g_rcas_sharpness = 0.f;    // RCAS on the antialiased scene, 0 = off
static bool g_luma_bloom_enable = true; // The game's bloom chain drawn at a higher resolution (see "DrawBloomPass")
static bool g_hide_ui = false;          // Session-only, never persisted: a stuck "on" would look like a broken HUD
static bool g_fixes_installed = false;  // See "FrameRateFixes::Install"
#if DEVELOPMENT
static int g_bloom_scale_override = 0;      // Luma Bloom's scale, 0 = from the resolution
static bool g_smaa_predication = true;      // Off: plain SMAA ULTRA (the fallback without the prepass, e.g. in the menus)
static float g_smaa_pred_tolerance = 0.02f; // A fraction of view depth (MGR:R's value)
static bool g_smaa_pred_debug = false;      // The predication mask instead of the scene
static bool g_mv_debug_view = false;        // Core's debug draw of the motion vector target
static bool g_mv_enable = true;             // Off: the patched draws keep no object motion (camera motion only), for A/B
static bool g_sr_reactive_enable = true;
// The additive draws' reactivity, scaled and capped at 0.9; the alpha blended draws' threshold (Bayonetta's values)
static float g_sr_reactive_scale = 2.f;
static float g_sr_reactive_threshold = 0.5f;
static bool g_sr_reactive_debug_view = false;
#else
constexpr int g_bloom_scale_override = 0;
constexpr bool g_smaa_predication = true;
constexpr float g_smaa_pred_tolerance = 0.02f;
constexpr bool g_mv_enable = true;
constexpr bool g_sr_reactive_enable = true;
constexpr float g_sr_reactive_scale = 2.f;
constexpr float g_sr_reactive_threshold = 0.5f;
#endif

struct VanquishGameDeviceData final : public GameDeviceData
{
   // The gather pass' output, before the AA and the UI: the present blit clamps the UI to it
   ComPtr<ID3D11Texture2D> scene_texture;
   ComPtr<ID3D11ShaderResourceView> scene_srv;
   bool has_scene_snapshot = false; // This frame's, until the present blit reads it
   bool has_drawn_gather = false;   // This frame's: the scene's draws come before it, the UI and movies after
   // This frame's gather pass target and canvas (the game's AA output, or dgVoodoo's copy), which the UI draws on
   uint64_t gather_target = 0;
   uint64_t canvas_target = 0;
   // The last video unpack target: the copy reading it draws a movie (Copy_0x91A6C437's AutoHDR). Kept across frames: a movie unpacks
   // only on its own frame rate (every 2-4 game frames seen) while the copy draws every frame. Our reference keeps it alive after the
   // movie, unbound, until the next one replaces it.
   ComPtr<ID3D11Resource> video_frame;
   // The game's 320x176 bloom target (the chain's first filter05 draws into it from the scene), identity only
   uint64_t bloom_target = 0;
   // Luma Bloom ("DrawBloomPass", Bayonetta's): the target's chain at "bloom_scale" times its size. Each pass draws into the target's top
   // left (the level's size) and dgVoodoo then copies that region into a texture of the region's size, which the next passes sample:
   // "bloom_shadows" are those, by the game's texture, at the same scale ("MirrorBloomCopy"). The gather pass reads "bloom_srv".
   struct BloomShadow
   {
      uint64_t game_texture = 0;
      ComPtr<ID3D11Texture2D> texture;
      ComPtr<ID3D11ShaderResourceView> srv;
   };
   uint32_t bloom_scale = 0;
   ComPtr<ID3D11Texture2D> bloom_texture;
   ComPtr<ID3D11RenderTargetView> bloom_rtv;
   ComPtr<ID3D11ShaderResourceView> bloom_srv;
   std::vector<BloomShadow> bloom_shadows;
   // This frame's depth prepass target (the game's), and SMAA's predication: its copy and the mask from it
   ComPtr<ID3D11Texture2D> depth_prepass;
   ComPtr<ID3D11Texture2D> smaa_depth;
   ComPtr<ID3D11ShaderResourceView> smaa_depth_srv;
   ComPtr<ID3D11Texture2D> smaa_predication;
   ComPtr<ID3D11UnorderedAccessView> smaa_predication_uav;
   ComPtr<ID3D11ShaderResourceView> smaa_predication_srv;
   com_ptr<ID3D11Buffer> smaa_predication_buffer;
   com_ptr<ID3D11Buffer> smaa_metrics_buffer;
   bool has_drawn_smaa = false; // This frame's: the game's AA then only copies
   // SMAA T2x (see "DrawSMAAOnScene"), set at present for the whole frame like "sr_active". The scene jitters ("jitter", see
   // "OpenScene") only after a frame the resolve ran. The two scene sized frames (linear colour, velocity length in alpha) alternate;
   // the other one is the history only if "t2x_frame", the last frame the resolve ran, was the frame before.
   bool t2x_active = false;
   bool mv_filled = false; // The fill wrote this frame's motion vectors for T2x (reset at present)
   com_ptr<ID3D11Texture2D> t2x_frames[2];
   com_ptr<ID3D11RenderTargetView> t2x_frame_rtvs[2];
   com_ptr<ID3D11ShaderResourceView> t2x_frame_srvs[2];
   uint32_t t2x_frame = 0;
   // T2x's resolve writes RGB only: the gather target's alpha stays. Made at its first use.
   com_ptr<ID3D11BlendState> rgb_blend_state;
   // max(target, 0) with a black draw, made at its first use (see "subtracts")
   com_ptr<ID3D11BlendState> max_blend_state;

   // DLAA, FSR Native AA, XeSS or Luma TAA (Core's SR bridge: the game is 32-bit) on the motion vectors of Core's "SceneMotionVectors"
   // (vc4 layout in "MotionVectorPatches"), before the gather pass
   // An upscaler is picked and hasn't failed, latched at present for the whole frame
   bool sr_active = false;
   // The motion vector targets, camera, objects and vc4 copies; "mv.active": the motion vector draws this frame (set at present)
   SceneMotionVectors::State mv;
   // The scene's depth (the D24 the depth prepass opens the scene with) and its view for the fill (dgVoodoo's R24G8 textures get a shader
   // resource bind, see "OnCreateResource"); the last frame's tells the jittered draws before this frame's scene opens
   com_ptr<ID3D11Resource> scene_depth;
   com_ptr<ID3D11ShaderResourceView> scene_depth_srv;
   // The last depth view found to be of "scene_depth": the scene's draws match it without a "GetResource" (referenced, so its address
   // can't come back for a view of another resource)
   com_ptr<ID3D11DepthStencilView> scene_dsv;
   // The size of the last gather pass' scene: the depth prepass' target of this size opens the scene (the shadow maps draw R32F targets
   // too)
   uint2 scene_size_hint = {};
   // The last two gather passes' scenes (two if the game alternates them), which are the translucents' target
   // ("MotionVectorGame::second_scene_color")
   com_ptr<ID3D11Resource> scene_color_hints[2];
   ID3D11DepthStencilView* open_refused_dsv = nullptr; // The last depth view "OpenScene" refused (reset at present)
   // FSR's vertical FOV and planes, from the frame camera's projection (60 degrees until a camera is read)
   float sr_vert_fov = 1.0471976f;
   float sr_near_plane = SR::SuperResolutionImpl::DrawData{}.near_plane;
   float sr_far_plane = SR::SuperResolutionImpl::DrawData{}.far_plane;
   com_ptr<ID3D11ShaderResourceView> sr_output_srv;
   // The projection jitter (pixels, +y down) of this frame, zero unless the upscaler drew the last one (frames it skips would reach the
   // screen jittered), applied as a viewport offset (see "JitterSceneViewport")
   std::array<float, 2> jitter = {};
   bool viewport_jittered = false;
   D3D11_VIEWPORT game_viewport = {};
   D3D11_VIEWPORT jittered_viewport = {};
   // "IsSceneDepthTested"'s last depth stencil state, whether it tests depth and whether it writes it (forgotten when the scene
   // opens: a released state's address comes back for another)
   ID3D11DepthStencilState* depth_test_state = nullptr;
   bool depth_test = false;
   bool depth_write = false;

   // FSR's and Luma TAA's masks ("mv.reactive_target", "mv.reactive", "mv.transparency", "mv.reactive_previous"), made here: this
   // frame's mask target was cleared when the scene opened, the alpha blended draws after it write it
   bool reactive_ready = false;
#if DEVELOPMENT
   // This frame's and the last complete frame's "DrawWithReactiveMask" draws
   uint32_t reactive_draws = 0;
   uint32_t last_reactive_draws = 0;
#endif

   void ReleaseSmaaPredication()
   {
      smaa_predication_srv.reset();
      smaa_predication_uav.reset();
      smaa_predication.reset();
      smaa_depth_srv.reset();
      smaa_depth.reset();
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

   // The other T2x frame holds the previous frame's resolve: the jitter (OpenScene) and the resolve (DrawSMAAOnScene) agree on it
   bool HasT2xHistory(uint32_t frame_index) const
   {
      return t2x_frames[0] && t2x_frame + 1 == frame_index;
   }
};

class Vanquish final : public Game
{
   static VanquishGameDeviceData& GetGameDeviceData(DeviceData& device_data)
   {
      return *static_cast<VanquishGameDeviceData*>(device_data.game);
   }

   // Copies the bound render target 0 into "scene_texture", recreated on a size or format change
   static void SnapshotScene(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, VanquishGameDeviceData* game_device_data)
   {
      ComPtr<ID3D11RenderTargetView> rtv;
      native_device_context->OMGetRenderTargets(1, rtv.put(), nullptr);
      if (!rtv)
         return;
      ComPtr<ID3D11Resource> resource;
      rtv->GetResource(resource.put());
      ComPtr<ID3D11Texture2D> target;
      if (FAILED(resource->QueryInterface(target.put())))
         return;
      D3D11_TEXTURE2D_DESC target_desc;
      target->GetDesc(&target_desc);
      // A typeless target needs the render target's view format
      D3D11_RENDER_TARGET_VIEW_DESC rtv_desc;
      rtv->GetDesc(&rtv_desc);
      D3D11_TEXTURE2D_DESC scene_desc = {};
      if (game_device_data->scene_texture)
      {
         game_device_data->scene_texture->GetDesc(&scene_desc);
      }
      if (scene_desc.Width != target_desc.Width || scene_desc.Height != target_desc.Height || scene_desc.Format != rtv_desc.Format)
      {
         game_device_data->scene_srv.reset();
         game_device_data->scene_texture.reset();
         const D3D11_TEXTURE2D_DESC desc = {.Width = target_desc.Width, .Height = target_desc.Height, .MipLevels = 1, .ArraySize = 1, .Format = rtv_desc.Format, .SampleDesc = {.Count = 1}, .Usage = D3D11_USAGE_DEFAULT, .BindFlags = D3D11_BIND_SHADER_RESOURCE};
         if (FAILED(native_device->CreateTexture2D(&desc, nullptr, game_device_data->scene_texture.put())) || FAILED(native_device->CreateShaderResourceView(game_device_data->scene_texture.get(), nullptr, game_device_data->scene_srv.put())))
         {
            game_device_data->scene_srv.reset();
            game_device_data->scene_texture.reset();
            return;
         }
      }
      native_device_context->CopySubresourceRegion(game_device_data->scene_texture.get(), 0, 0, 0, 0, target.get(), 0, nullptr);
      game_device_data->has_scene_snapshot = true;
   }

   // SMAA ULTRA on the gather pass' snapshot, written back into its target before the game's AA slot and the UI, predicated by the depth
   // prepass' edges ("Luma_Vanquish_DepthEdges.hlsl"). MGR:R's "DrawSMAAOnScene".
   static void DrawSMAAOnScene(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, DeviceData& device_data, ID3D11RenderTargetView* target_rtv)
   {
      auto& gd = GetGameDeviceData(device_data);
      if (!HasSMAAShaders(device_data))
         return;
      const uint2 size = GetViewTextureSize(gd.scene_srv.get());

      // Scale and mask fall back together: 2 with a null mask raises the threshold frame wide
      auto* const predication_shader = FindShader(device_data.native_compute_shaders, CompileTimeStringHash("Vanquish Depth Edges CS"));
      D3D11_TEXTURE2D_DESC depth_desc = {};
      if (gd.depth_prepass)
      {
         gd.depth_prepass->GetDesc(&depth_desc);
      }
      bool predication = g_smaa_predication && predication_shader && depth_desc.Format == DXGI_FORMAT_R32_FLOAT && depth_desc.SampleDesc.Count == 1 && depth_desc.Width == size.x && depth_desc.Height == size.y;
      if (predication && GetViewTextureSize(gd.smaa_predication_srv.get()) != size)
      {
         gd.ReleaseSmaaPredication();
         const CD3D11_TEXTURE2D_DESC smaa_depth_desc(DXGI_FORMAT_R32_FLOAT, size.x, size.y, 1, 1, D3D11_BIND_SHADER_RESOURCE);
         const CD3D11_TEXTURE2D_DESC predication_desc(DXGI_FORMAT_R16_FLOAT, size.x, size.y, 1, 1, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS);
         if (FAILED(native_device->CreateTexture2D(&smaa_depth_desc, nullptr, gd.smaa_depth.put())) || FAILED(native_device->CreateShaderResourceView(gd.smaa_depth.get(), nullptr, gd.smaa_depth_srv.put())) ||
             FAILED(native_device->CreateTexture2D(&predication_desc, nullptr, gd.smaa_predication.put())) || FAILED(native_device->CreateUnorderedAccessView(gd.smaa_predication.get(), nullptr, gd.smaa_predication_uav.put())) ||
             FAILED(native_device->CreateShaderResourceView(gd.smaa_predication.get(), nullptr, gd.smaa_predication_srv.put())))
         {
            gd.ReleaseSmaaPredication();
         }
      }
      const float tolerance[4] = {g_smaa_pred_tolerance, 0.f, 0.f, 0.f};
      predication = predication && gd.smaa_predication_srv && PatchedDraws::WriteDynamicConstants(native_device, native_device_context, std::addressof(gd.smaa_predication_buffer), tolerance, sizeof(tolerance));
      // SMAA T2x: SMAA into this frame's linear target with the velocity, then the resolve with the previous frame into the gather
      // target (Bayonetta's). It needs this frame's motion vectors from the fill, at the target's size. Without them this frame is 1x
      // and the next one doesn't jitter.
      auto* const copy_vs = FindShader(device_data.native_vertex_shaders, CompileTimeStringHash("Copy VS"));
      bool t2x = gd.t2x_active && gd.mv_filled && gd.mv.srv && copy_vs && GetViewTextureSize(gd.mv.srv.get()) == size;
      ID3D11PixelShader* t2x_weight_ps = nullptr;
      ID3D11PixelShader* t2x_blend_ps = nullptr;
      ID3D11PixelShader* t2x_resolve_ps = nullptr;
      if (t2x)
      {
         t2x_weight_ps = FindShader(device_data.native_pixel_shaders, CompileTimeStringHash("Vanquish SMAA T2x Blending Weight Calculation PS"));
         t2x_blend_ps = FindShader(device_data.native_pixel_shaders, CompileTimeStringHash("Vanquish SMAA T2x Neighborhood Blending PS"));
         t2x_resolve_ps = FindShader(device_data.native_pixel_shaders, CompileTimeStringHash("Vanquish SMAA T2x Resolve PS"));
         t2x = t2x_weight_ps && t2x_blend_ps && t2x_resolve_ps;
      }
      const uint32_t frame_index = cb_luma_global_settings.FrameIndex;
      const int t2x_current = int(frame_index & 1);
      bool t2x_history = gd.HasT2xHistory(frame_index);
      if (t2x && GetViewTextureSize(gd.t2x_frame_srvs[0].get()) != size)
      {
         t2x_history = false;
         gd.ReleaseT2xFrames();
         for (int i = 0; i < 2; i++)
         {
            const CD3D11_TEXTURE2D_DESC desc(DXGI_FORMAT_R16G16B16A16_FLOAT, size.x, size.y, 1, 1, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET);
            if (FAILED(native_device->CreateTexture2D(&desc, nullptr, &gd.t2x_frames[i])) || FAILED(native_device->CreateRenderTargetView(gd.t2x_frames[i].get(), nullptr, &gd.t2x_frame_rtvs[i])) ||
                FAILED(native_device->CreateShaderResourceView(gd.t2x_frames[i].get(), nullptr, &gd.t2x_frame_srvs[i])))
            {
               gd.ReleaseT2xFrames();
               break;
            }
         }
         t2x = gd.t2x_frames[0].get() != nullptr;
      }
      if (t2x && !gd.rgb_blend_state)
      {
         D3D11_BLEND_DESC blend_desc = CD3D11_BLEND_DESC(D3D11_DEFAULT);
         blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_RED | D3D11_COLOR_WRITE_ENABLE_GREEN | D3D11_COLOR_WRITE_ENABLE_BLUE;
         native_device->CreateBlendState(&blend_desc, &gd.rgb_blend_state);
      }
      // Without it the resolve would write the gather target's alpha: 1x
      t2x = t2x && gd.rgb_blend_state;
      // b1 ("Luma_SMAA_impl.hlsl"'s "SmaaMetricsCB"): T2x's subsample indices for the jitter phase (see "OpenScene", the phase is the
      // frame's parity), 0 for 1x or a scene that didn't jitter (only T2x jitters with it active)
      const float subsample_indices = ((t2x && gd.jitter[0] != 0.f) ? float(t2x_current + 1) : 0.f);
      const CB::SmaaMetrics metrics = {
         .rt_metrics = {1.f / float(size.x), 1.f / float(size.y), float(size.x), float(size.y)},
         .predication = {(predication ? 2.f : 1.f), 0.f, 0.f, 0.f},
         .subsample_indices = {subsample_indices, subsample_indices, subsample_indices, 0.f},
      };
      if (!PatchedDraws::WriteDynamicConstants(native_device, native_device_context, std::addressof(gd.smaa_metrics_buffer), &metrics, sizeof(metrics)))
         return;

      if (predication)
      {
         // A copy: the prepass target may still be bound as a shader resource
         native_device_context->CopySubresourceRegion(gd.smaa_depth.get(), 0, 0, 0, 0, gd.depth_prepass.get(), 0, nullptr);
         ID3D11ShaderResourceView* const srv = gd.smaa_depth_srv.get();
         ID3D11UnorderedAccessView* const uav = gd.smaa_predication_uav.get();
         ID3D11Buffer* const buffer = gd.smaa_predication_buffer.get();
         DrawStateStack<DrawStateStackType::Compute> compute_state;
         compute_state.Cache(native_device_context, device_data.uav_max_count);
         native_device_context->CSSetShaderResources(0, 1, &srv);
         native_device_context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);
         native_device_context->CSSetConstantBuffers(0, 1, &buffer);
         native_device_context->CSSetShader(predication_shader, nullptr, 0);
         native_device_context->Dispatch((size.x + 7) / 8, (size.y + 7) / 8, 1);
         compute_state.Restore(native_device_context);
      }
      gd.has_drawn_smaa = true;
#if DEVELOPMENT
      // Calibration aid: predication's effect is the absence of smearing, which the eye misjudges; judge the mask instead (red)
      if (predication && g_smaa_pred_debug)
      {
         auto* const copy_ps = FindShader(device_data.native_pixel_shaders, CompileTimeStringHash("Copy PS"));
         if (copy_vs && copy_ps)
         {
            DrawStateStack<DrawStateStackType::FullGraphics> draw_state_stack;
            draw_state_stack.Cache(native_device_context, device_data.uav_max_count);
            DrawCustomPixelShader(native_device_context, device_data.default_depth_stencil_state.get(), device_data.default_blend_state.get(), nullptr, copy_vs, copy_ps, gd.smaa_predication_srv.get(), target_rtv, size.x, size.y, false);
            draw_state_stack.Restore(native_device_context);
            return;
         }
      }
#endif
      // Writing the gather target is safe: the chain samples the snapshot, never the target itself
      if (!t2x)
      {
         DrawSMAA(native_device, native_device_context, device_data, target_rtv, gd.scene_srv.get(), gd.scene_srv.get(), predication ? gd.smaa_predication_srv.get() : nullptr, gd.smaa_metrics_buffer.get());
         return;
      }
      // The game's state back after the resolve (dgVoodoo doesn't rebind what it thinks is bound)
      DrawStateStack<DrawStateStackType::FullGraphics> graphics_state;
      graphics_state.Cache(native_device_context, device_data.uav_max_count);
      // The metrics at VS+PS b1, bound here rather than through "DrawSMAA()" (which restores the slot): the resolve reads them too
      ID3D11Buffer* const metrics_buffer = gd.smaa_metrics_buffer.get();
      native_device_context->VSSetConstantBuffers(1, 1, &metrics_buffer);
      native_device_context->PSSetConstantBuffers(1, 1, &metrics_buffer);
      const SMAAT2xPasses t2x_passes = {.blending_weight_calculation_ps = t2x_weight_ps, .neighborhood_blending_ps = t2x_blend_ps, .velocity = gd.mv.srv.get()};
      DrawSMAA(native_device, native_device_context, device_data, gd.t2x_frame_rtvs[t2x_current].get(), gd.scene_srv.get(), gd.scene_srv.get(), predication ? gd.smaa_predication_srv.get() : nullptr, nullptr, &t2x_passes);
      // The resolve: t0 this frame, t1 the previous one or this one again without a history, t2 the motion vectors, s0 linear and s1 point
      native_device_context->OMSetRenderTargets(0, nullptr, nullptr);
      ID3D11ShaderResourceView* const resolve_srvs[2] = {gd.t2x_frame_srvs[t2x_history ? (t2x_current ^ 1) : t2x_current].get(), gd.mv.srv.get()};
      native_device_context->PSSetShaderResources(1, 2, resolve_srvs);
      ID3D11SamplerState* const resolve_samplers[2] = {device_data.sampler_state_linear.get(), device_data.sampler_state_point.get()};
      native_device_context->PSSetSamplers(0, 2, resolve_samplers);
      DrawCustomPixelShader(native_device_context, device_data.default_depth_stencil_state.get(), gd.rgb_blend_state.get(), nullptr, copy_vs, t2x_resolve_ps, gd.t2x_frame_srvs[t2x_current].get(), target_rtv, size.x, size.y, false);
      graphics_state.Restore(native_device_context);
      gd.t2x_frame = frame_index;
   }

   // Whether the bound draw tests depth against the scene's depth: the scene's draws (the depth prepass, the G-buffer, the lights, the
   // velocity pass, the translucents), not full screen passes or shadow maps
   static bool IsSceneDepthTested(ID3D11DeviceContext* native_device_context, VanquishGameDeviceData* gd, const com_ptr<ID3D11DepthStencilView>& dsv)
   {
      if (!dsv)
         return false;
      if (dsv != gd->scene_dsv)
      {
         com_ptr<ID3D11Resource> depth;
         dsv->GetResource(&depth);
         if (depth != gd->scene_depth)
            return false;
         gd->scene_dsv = dsv;
      }
      ID3D11DepthStencilState* const state = gd->mv.bound_states.DepthStencilState(native_device_context);
      if (state != gd->depth_test_state)
      {
         D3D11_DEPTH_STENCIL_DESC desc = CD3D11_DEPTH_STENCIL_DESC(D3D11_DEFAULT);
         if (state)
         {
            state->GetDesc(&desc);
         }
         gd->depth_test = desc.DepthEnable;
         gd->depth_write = desc.DepthEnable && desc.DepthWriteMask != D3D11_DEPTH_WRITE_MASK_ZERO;
         gd->depth_test_state = state;
      }
      return gd->depth_test;
   }

   // Every scene draw ("IsSceneDepthTested", "jitter") takes the jitter, as a subpixel offset of its viewport: the same NDC shift as a
   // projection jitter, with the game's shaders as they are. The others keep the game's viewport. The patched vertex shaders add the
   // jitter buffer (zero here, see "OpenScene") to the same positions, and their motion vectors never contain a jitter, so the two
   // don't stack and the motion vectors are unjittered.
   static void JitterSceneViewport(ID3D11DeviceContext* native_device_context, VanquishGameDeviceData* gd, bool jitter)
   {
      UINT count = 1;
      D3D11_VIEWPORT viewport;
      native_device_context->RSGetViewports(&count, &viewport);
      if (count == 0)
         return;
      // The game sets its viewports itself: ours is still bound only if it hasn't since
      const bool jittered = gd->viewport_jittered && std::memcmp(&viewport, &gd->jittered_viewport, sizeof(viewport)) == 0;
      if (jitter == jittered)
         return;
      if (jitter)
      {
         gd->game_viewport = viewport;
         viewport.TopLeftX += gd->jitter[0];
         viewport.TopLeftY += gd->jitter[1];
         gd->jittered_viewport = viewport;
      }
      else
      {
         viewport = gd->game_viewport;
      }
      gd->viewport_jittered = jitter;
      native_device_context->RSSetViewports(1, &viewport);
   }

   // Opens the scene at the frame's first depth prepass draw into the scene's depth: a single sample D24 of the last gather pass' scene
   // size, whose target is the prepass' (the shadow maps and the other offscreen passes aren't). Takes the scene depth and its view for
   // the fill, rolls the last frame's camera and objects over and makes the masks' targets. Once per present (the later passes never
   // reopen it). The motion vector draws' jitter is zero: the scene jitters through its viewports ("JitterSceneViewport") and the
   // patched vertex shaders add the jitter buffer to the same positions, so a jitter there would shift them twice.
   static void OpenScene(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, DeviceData& device_data, bool reactive, ID3D11DepthStencilView* dsv)
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
      if (depth_desc.SampleDesc.Count != 1 || depth_desc.Format != DXGI_FORMAT_R24G8_TYPELESS || depth_size != gd.scene_size_hint)
      {
         gd.open_refused_dsv = dsv;
         return;
      }
      if (depth != gd.scene_depth)
      {
         gd.scene_depth = depth;
         gd.scene_dsv.reset();
         gd.scene_depth_srv.reset();
         const D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = CD3D11_SHADER_RESOURCE_VIEW_DESC(D3D11_SRV_DIMENSION_TEXTURE2D, DXGI_FORMAT_R24_UNORM_X8_TYPELESS);
         native_device->CreateShaderResourceView(depth.get(), &srv_desc, &gd.scene_depth_srv);
      }
      gd.depth_test_state = nullptr;
      if (!SceneMotionVectors::BeginFrame(&gd.mv, native_device, native_device_context, dsv, depth_size, {}))
         return;
      // SMAA T2x: the sample offsets (0.25, 0.25) then (-0.25, -0.25) in y down pixels, for "SMAA.hlsl"'s T2x subsample indices 1 and 2
      // (Bayonetta's phases, in the upscalers' jitter convention, which the viewport applies). That is its table read with y up, which
      // measured better.
      if (gd.t2x_active && gd.HasT2xHistory(cb_luma_global_settings.FrameIndex))
      {
         const float shift = ((cb_luma_global_settings.FrameIndex & 1) == 0 ? -0.25f : 0.25f);
         gd.jitter = {shift, shift};
      }
      // The masks at the scene's size; what the alpha blended draws write starts from zero every frame
      if (reactive && GetViewTextureSize(gd.mv.reactive_target_srv.get()) != depth_size)
      {
         gd.mv.ReleaseReactiveMasks();
         const CD3D11_TEXTURE2D_DESC target_desc(DXGI_FORMAT_R8G8_UNORM, depth_size.x, depth_size.y, 1, 1, D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE);
         const CD3D11_TEXTURE2D_DESC mask_desc(DXGI_FORMAT_R8_UNORM, depth_size.x, depth_size.y, 1, 1, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS);
         const CD3D11_TEXTURE2D_DESC previous_desc(DXGI_FORMAT_R32_FLOAT, depth_size.x, depth_size.y, 1, 1, D3D11_BIND_UNORDERED_ACCESS);
         if (FAILED(native_device->CreateTexture2D(&target_desc, nullptr, &gd.mv.reactive_target)) || FAILED(native_device->CreateRenderTargetView(gd.mv.reactive_target.get(), nullptr, &gd.mv.reactive_target_rtv)) ||
             FAILED(native_device->CreateShaderResourceView(gd.mv.reactive_target.get(), nullptr, &gd.mv.reactive_target_srv)) || FAILED(SRBridge::CreateSharableTexture(native_device, mask_desc, &gd.mv.reactive)) ||
             FAILED(native_device->CreateUnorderedAccessView(gd.mv.reactive.get(), nullptr, &gd.mv.reactive_uav)) || FAILED(SRBridge::CreateSharableTexture(native_device, mask_desc, &gd.mv.transparency)) ||
             FAILED(native_device->CreateUnorderedAccessView(gd.mv.transparency.get(), nullptr, &gd.mv.transparency_uav)) || FAILED(native_device->CreateTexture2D(&previous_desc, nullptr, &gd.mv.reactive_previous)) ||
             FAILED(native_device->CreateUnorderedAccessView(gd.mv.reactive_previous.get(), nullptr, &gd.mv.reactive_previous_uav)))
         {
            gd.mv.ReleaseReactiveMasks();
         }
         else
         {
            constexpr float zero[4] = {};
            native_device_context->ClearUnorderedAccessViewFloat(gd.mv.reactive_previous_uav.get(), zero);
         }
      }
      gd.reactive_ready = reactive && gd.mv.reactive_previous_uav;
      if (gd.reactive_ready)
      {
         constexpr FLOAT zero[4] = {};
         native_device_context->ClearRenderTargetView(gd.mv.reactive_target_rtv.get(), zero);
      }
   }

   // The upscalers that read the masks: FSR and Luma TAA (DLSS's current presets ignore them, DLSS-Best-Practices TRN-2)
   static bool IsReactiveMaskUsed(const DeviceData& device_data)
   {
      return g_sr_reactive_enable && (device_data.sr_type == SR::Type::FSR || device_data.sr_type == SR::Type::LumaTAA);
   }

   // An alpha blended scene draw after the opaque scene (the translucents: smoke, glass, sparks, glows), which has no motion vectors of
   // its own, also writes its mask: its pixel shader patched, the mask target added at "REACTIVE_SLOT", both put back after. False if it
   // isn't one or can't be patched: the caller lets the game draw it.
   static bool DrawWithReactiveMask(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, DeviceData& device_data, uint64_t pixel_shader_hash, reshade::api::pipeline pipeline, const std::function<void()>& draw, const com_ptr<ID3D11RenderTargetView>* rtvs, ID3D11DepthStencilView* dsv)
   {
      auto& gd = GetGameDeviceData(device_data);
      SceneMotionVectors::ClassifyBoundBlend(native_device_context, &gd.mv);
      // A blended draw that writes depth is solid geometry (the masks are for content without motion vectors): masking it only makes
      // its edges flicker. The caller tested the scene's depth.
      if (gd.mv.reactive_blend == SceneMotionVectors::ReactiveBlend::NONE || gd.depth_write)
         return false;
      // Into the scene: a full size target (not a smaller particle buffer); the mask target is the scene's size
      if (!rtvs[0] || GetViewTextureSize(rtvs[0].get()) != gd.mv.scene_size)
         return false;
      const PixelShaderPatch patch = (gd.mv.reactive_blend == SceneMotionVectors::ReactiveBlend::ADDITIVE ? PixelShaderPatch::MASK_ADDITIVE : PixelShaderPatch::MASK_ALPHA);
      ID3D11PixelShader* const mask_shader = SceneMotionVectors::GetPatchedShader<MotionVectorGame, ID3D11PixelShader>(&gd.mv, native_device, device_data, uint32_t(pixel_shader_hash), pipeline, patch).shader.get();
      if (!mask_shader)
         return false;

      com_ptr<ID3D11PixelShader> game_shader;
      native_device_context->PSGetShader(&game_shader, nullptr, nullptr);
      ID3D11RenderTargetView* targets[REACTIVE_SLOT + 1] = {};
      for (UINT slot = 0; slot < REACTIVE_SLOT; slot++)
      {
         targets[slot] = rtvs[slot].get();
      }
      targets[REACTIVE_SLOT] = gd.mv.reactive_target_rtv.get();
      native_device_context->OMSetRenderTargets(REACTIVE_SLOT + 1, targets, dsv);
      native_device_context->PSSetShader(mask_shader, nullptr, 0);
      draw();
      native_device_context->PSSetShader(game_shader.get(), nullptr, 0);
      native_device_context->OMSetRenderTargets(REACTIVE_SLOT, targets, dsv);
#if DEVELOPMENT
      gd.reactive_draws++;
#endif
      return true;
   }

   // The fill ("Luma_Vanquish_MotionVectorFill.hlsl"): the camera motion of the texels no motion vector draw wrote, the upscalers' depth and
   // (with "write_reactive") the masks. Again at the scene's end after the alpha blended draws' early one (it only fills what's left).
   // Unbinds the output targets; the caller restores the game's state.
   static bool FillMotionVectors(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, DeviceData& device_data, bool write_reactive)
   {
      auto& gd = GetGameDeviceData(device_data);
      auto* const fill_shader = FindShader(device_data.native_compute_shaders, CompileTimeStringHash("Vanquish Motion Vector Fill CS"));
      // The targets exist once a draw was accepted ("DrawWithMotionVectors"), the camera once a draw of a camera kind was
      if (!fill_shader || !gd.mv.uav || !gd.mv.device_depth_uav || !gd.mv.camera || !gd.scene_depth_srv)
         return false;
#if ENABLE_LUMA_TAA
      // Luma TAA without its 2x history flickers on lasers at the High scale: half of it balances flicker and smear
      const bool luma_taa_without_2x_history = device_data.sr_type == SR::Type::LumaTAA && LumaTAA::quality.load() < LumaTAA::history_2x_quality;
      const float reactive_scale = (luma_taa_without_2x_history ? 0.5f : 1.f) * g_sr_reactive_scale;
#else
      const float reactive_scale = g_sr_reactive_scale;
#endif
      const uint2 size = gd.mv.scene_size;
      // The frame camera's reprojection and the viewport jitter (in the depth, not in the motion vectors)
      CB::MotionVectorFillConstants fill_constants = {
         .jitter_ndc = {gd.jitter[0] * 2.f / float(size.x), gd.jitter[1] * -2.f / float(size.y)},
         .reactive_scale = reactive_scale,
         .reactive_threshold = g_sr_reactive_threshold,
         .reactive_enabled = (write_reactive ? 1.f : 0.f),
      };
      for (int i = 0; i < 16; i++)
      {
         fill_constants.reprojection.GetData()[i] = float(gd.mv.reprojection.GetData()[i]);
      }
      if (!PatchedDraws::WriteDynamicConstants(native_device, native_device_context, std::addressof(gd.mv.fill_buffer), &fill_constants, sizeof(fill_constants)))
         return false;
      // The depth and the motion vectors may be bound as targets
      native_device_context->OMSetRenderTargets(0, nullptr, nullptr);
      ID3D11Buffer* const buffer = gd.mv.fill_buffer.get();
      ID3D11ShaderResourceView* const srvs[2] = {gd.scene_depth_srv.get(), (write_reactive ? gd.mv.reactive_target_srv.get() : nullptr)};
      ID3D11UnorderedAccessView* const uavs[5] = {gd.mv.uav.get(), gd.mv.device_depth_uav.get(), (write_reactive ? gd.mv.reactive_uav.get() : nullptr), (write_reactive ? gd.mv.transparency_uav.get() : nullptr),
         (write_reactive ? gd.mv.reactive_previous_uav.get() : nullptr)};
      native_device_context->CSSetConstantBuffers(0, 1, &buffer);
      native_device_context->CSSetShaderResources(0, UINT(std::size(srvs)), srvs);
      native_device_context->CSSetUnorderedAccessViews(0, UINT(std::size(uavs)), uavs, nullptr);
      native_device_context->CSSetShader(fill_shader, nullptr, 0);
      native_device_context->Dispatch((size.x + 7) / 8, (size.y + 7) / 8, 1);
      ID3D11ShaderResourceView* const null_srvs[std::size(srvs)] = {};
      ID3D11UnorderedAccessView* const null_uavs[std::size(uavs)] = {};
      native_device_context->CSSetShaderResources(0, UINT(std::size(null_srvs)), null_srvs);
      native_device_context->CSSetUnorderedAccessViews(0, UINT(std::size(null_uavs)), null_uavs, nullptr);
      return true;
   }

   // The fill, then DLAA, FSR Native AA, XeSS or Luma TAA on the jittered scene the gather pass reads ("scene"). The output's view (the
   // gather pass reads it in the scene's place), null if it didn't draw (missing input, or the upscaler failed).
   static ID3D11ShaderResourceView* DrawUpscaler(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, DeviceData& device_data, ID3D11ShaderResourceView* scene)
   {
      auto& gd = GetGameDeviceData(device_data);
      if (!scene || !gd.mv.texture || !std::exchange(gd.mv.fill_pending, false))
         return nullptr;
      com_ptr<ID3D11Resource> scene_resource;
      scene->GetResource(&scene_resource);
      com_ptr<ID3D11Texture2D> scene_texture;
      if (FAILED(scene_resource->QueryInterface(&scene_texture)))
         return nullptr;
      D3D11_TEXTURE2D_DESC scene_desc;
      scene_texture->GetDesc(&scene_desc);
      D3D11_TEXTURE2D_DESC mv_desc;
      gd.mv.texture->GetDesc(&mv_desc);
      if (scene_desc.Width != mv_desc.Width || scene_desc.Height != mv_desc.Height || scene_desc.SampleDesc.Count != 1)
         return nullptr;

      D3D11_TEXTURE2D_DESC output_desc = {};
      if (device_data.sr_output_color)
      {
         device_data.sr_output_color->GetDesc(&output_desc);
      }
      bool output_recreated = false;
      if (output_desc.Width != scene_desc.Width || output_desc.Height != scene_desc.Height)
      {
         device_data.sr_output_color.reset();
         gd.sr_output_srv.reset();
         output_desc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R16G16B16A16_FLOAT, scene_desc.Width, scene_desc.Height, 1, 1, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS);
         if (SUCCEEDED(SRBridge::CreateSharableTexture(native_device, output_desc, &device_data.sr_output_color)))
         {
            native_device->CreateShaderResourceView(device_data.sr_output_color.get(), nullptr, &gd.sr_output_srv);
         }
         output_recreated = true;
      }
      if (!device_data.sr_output_color || !gd.sr_output_srv)
      {
         // Back to SMAA until the upscaler is picked again
         device_data.sr_suppressed = true;
         return nullptr;
      }
      // The masks from what this frame's alpha blended draws wrote
      const bool reactive_mask = gd.reactive_ready && IsReactiveMaskUsed(device_data);
      DrawStateStack<DrawStateStackType::FullGraphics> graphics_state;
      DrawStateStack<DrawStateStackType::Compute> compute_state;
      graphics_state.Cache(native_device_context, device_data.uav_max_count);
      compute_state.Cache(native_device_context, device_data.uav_max_count);
      if (!FillMotionVectors(native_device, native_device_context, device_data, reactive_mask))
      {
         compute_state.Restore(native_device_context);
         graphics_state.Restore(native_device_context);
         return nullptr;
      }

      bool drawn = false;
      {
         const SRDrawLock sr = LockSRForDraw(device_data);
         if (sr.implementation)
         {
            const SR::SettingsData settings_data = {
               .output_width = scene_desc.Width,
               .output_height = scene_desc.Height,
               .render_width = scene_desc.Width,
               .render_height = scene_desc.Height,
               // The scene before the gather pass: the fp16 mirror of the game's A8R8G8B8 target, gamma encoded (1 = SDR white, the values
               // above it kept by the upgrade)
               .hdr = true,
               // The motion vectors are UV deltas, previous minus current
               .mvs_x_scale = float(scene_desc.Width),
               .mvs_y_scale = float(scene_desc.Height),
               .auto_exposure = sr.type != SR::Type::FSR,
               // DLAA on HDR input takes preset J or K, never L or M (DLSS-Best-Practices PRE-4): "Default" picks
               // K (11), so NVIDIA can't change it over the air; an explicit choice is kept
               .render_preset = (dlss_render_preset != 0 ? dlss_render_preset : 11u),
            };
            sr.implementation->UpdateSettings(sr.instance_data, native_device_context, settings_data);
            // FSR requires a FOV (it errors on 0): a camera without a usable projection keeps the last one. The projection alone (the
            // view is affine): column vectors, one row per clip component.
            const SR::ViewProjectionCamera camera = SR::GetViewProjectionCamera(reinterpret_cast<const float*>(gd.mv.camera->data() + MotionVectorPatches::frame_camera.projection_row * 16), /* row_vectors */ false);
            if (camera.vert_fov > 0.0)
            {
               gd.sr_vert_fov = float(camera.vert_fov);
            }
            if (camera.near_plane > 0.0)
            {
               gd.sr_near_plane = float(camera.near_plane);
               gd.sr_far_plane = float(camera.far_plane);
            }
            const SR::SuperResolutionImpl::DrawData draw_data = {
               .reset = device_data.force_reset_sr || output_recreated,
               .output_color = device_data.sr_output_color.get(),
               .source_color = scene_texture.get(),
               .motion_vectors = gd.mv.texture.get(),
               .depth_buffer = gd.mv.device_depth.get(),
               .bias_mask = (reactive_mask ? gd.mv.reactive.get() : nullptr),
               .transparency_alpha = (reactive_mask ? gd.mv.transparency.get() : nullptr),
               // As applied (pixels, +y down)
               .jitter_x = gd.jitter[0],
               .jitter_y = gd.jitter[1],
               .vert_fov = gd.sr_vert_fov,
               .near_plane = gd.sr_near_plane,
               .far_plane = gd.sr_far_plane,
            };
            drawn = sr.implementation->Draw(sr.instance_data, native_device_context, draw_data);
            // Back to SMAA until the upscaler is picked again
            device_data.sr_suppressed = !drawn;
            // Not while the bridge's helper starts (the color copied as it is): SMAA stays on and the next frame resets
            device_data.has_drawn_sr = drawn && sr.implementation->IsReady(sr.instance_data);
         }
      }
      compute_state.Restore(native_device_context);
      graphics_state.Restore(native_device_context);
      return (drawn ? gd.sr_output_srv.get() : nullptr);
   }

   // The fill reads the scene's depth, which dgVoodoo creates typeless (R24G8) without a shader resource bind. The bind flag only: the
   // format stays as created.
   static bool OnCreateResource(reshade::api::device* device, reshade::api::resource_desc& desc, reshade::api::subresource_data* initial_data, reshade::api::resource_usage initial_state)
   {
      if (desc.type != reshade::api::resource_type::texture_2d || desc.texture.samples != 1 || desc.texture.format != reshade::api::format::r24_g8_typeless || (desc.usage & reshade::api::resource_usage::depth_stencil) == 0 ||
          (desc.usage & reshade::api::resource_usage::shader_resource) != 0)
         return false;
      desc.usage |= reshade::api::resource_usage::shader_resource;
      return true;
   }

   // The game's motion vector state for Core's buffer and bind events, null before the device data exists
   static SceneMotionVectors::State* GetSceneMotionVectors(reshade::api::device* device)
   {
      DeviceData* const device_data = device->get_private_data<DeviceData>();
      return ((device_data && device_data->game) ? &GetGameDeviceData(*device_data).mv : nullptr);
   }

   // What "SceneMotionVectors" asks of the game
   struct MotionVectorGame
   {
      static constexpr const char* log_tag = "Vanquish";
      static constexpr MotionVectorPatch::Layout layout = MotionVectorPatches::layout;
      static constexpr auto cameras = MotionVectorPatches::cameras;
      static constexpr auto tie_break_rows = MotionVectorPatches::tie_break_rows;
      static constexpr auto frame_camera = MotionVectorPatches::frame_camera;
      static constexpr const char* patch_names[size_t(PixelShaderPatch::COUNT)] = {"MV PS", "Mask", "Mask (additive)", "MV PS (blended)", "MV PS (blended, mask)", "", "", ""};
      static constexpr bool second_scene_color = true;
      static constexpr PixelShaderPatch blended_mask_patch = PixelShaderPatch::GAME_0;

      // The opaque meshes' motion vectors are carried by the depth prepass' R32F target (not the upscaler's shader resource, the gather
      // pass' input is). The alpha blended draws go to the second target, the gather pass' scene (its fp16 mirror), which the caller
      // picks (see "scene_color_hints"). The G-buffer's draws are refused (several targets). Both are single sample.
      static bool AcceptSceneColor(const D3D11_RENDER_TARGET_VIEW_DESC& rtv_desc, const D3D11_TEXTURE2D_DESC& color_desc)
      {
         // The scene's fp16 mirror (bound in place of the game's R8G8B8A8 target) and the depth prepass' R32F
         return color_desc.SampleDesc.Count == 1 && (rtv_desc.Format == DXGI_FORMAT_R32_FLOAT || rtv_desc.Format == DXGI_FORMAT_R16G16B16A16_FLOAT || rtv_desc.Format == DXGI_FORMAT_R8G8B8A8_UNORM || rtv_desc.Format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
      }

      static bool MaskUnmatchedBlended(DeviceData& device_data)
      {
         return GetGameDeviceData(device_data).reactive_ready && IsReactiveMaskUsed(device_data);
      }

      // The alpha blended draws blend their motion vectors over the opaque ones: the texels none wrote get their camera motion first
      static void FillMarkersBeforeBlend(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, DeviceData& device_data)
      {
         FillMotionVectors(native_device, native_device_context, device_data, /* write_reactive */ false);
      }

      static bool IsExcludedPixelShader(uint32_t pixel_shader_hash)
      {
         return false;
      }

      // The masks are Vanquish's own ("DrawWithReactiveMask"), written by the translucents that come later and refused here
      static bool ReactiveMaskUsed(const DeviceData& device_data)
      {
         return false;
      }

      static bool CanFill(DeviceData& device_data)
      {
         return GetGameDeviceData(device_data).scene_depth_srv && HasShaders(device_data.native_compute_shaders, CompileTimeStringHash("Vanquish Motion Vector Fill CS"));
      }

      // A game pixel shader's "patch", from the bytecode its shader object was created with; empty with "error" if it can't be patched. Any
      // thread ("AddShaderWarmupVariants" too).
      static std::vector<uint8_t> PatchPixelShader(SceneMotionVectors::PixelShaderPatch patch, const uint8_t* code, size_t code_size, std::string* error)
      {
         switch (patch)
         {
         case PixelShaderPatch::MOTION_VECTORS:
            return MotionVectorPatch::PatchPixelShader(code, code_size, MotionVectorPatches::layout, error, /* targets_only */ true);
         case PixelShaderPatch::MASK_ALPHA:
         case PixelShaderPatch::MASK_ADDITIVE:
            return MotionVectorPatch::PatchPixelShaderReactive(code, code_size, MotionVectorPatches::layout, REACTIVE_SLOT,
               (patch == PixelShaderPatch::MASK_ADDITIVE ? MotionVectorPatch::ReactiveMode::ADDITIVE : MotionVectorPatch::ReactiveMode::ALPHA), error);
         case PixelShaderPatch::BLENDED:
            return MotionVectorPatch::PatchPixelShaderReactive(code, code_size, MotionVectorPatches::layout, UINT32_MAX, MotionVectorPatch::ReactiveMode::ALPHA_MOTION_VECTORS, error);
         case blended_mask_patch:
            return MotionVectorPatch::PatchPixelShaderReactive(code, code_size, MotionVectorPatches::layout, REACTIVE_SLOT, MotionVectorPatch::ReactiveMode::ALPHA_MOTION_VECTORS, error);
         }
         return {};
      }
   };

   // dgVoodoo's copy of the bloom target's region a chain pass just wrote into the texture the next passes sample, repeated from
   // "bloom_texture" into that texture's "bloom_shadows" entry (made at the same scale on its first copy)
   static void MirrorBloomCopy(reshade::api::command_list* cmd_list, reshade::api::resource source, reshade::api::resource dest, const reshade::api::subresource_box* source_box, const reshade::api::subresource_box* dest_box)
   {
      DeviceData* const device_data = cmd_list->get_device()->get_private_data<DeviceData>();
      if (!g_luma_bloom_enable || !device_data || !device_data->game)
         return;
      auto& gd = GetGameDeviceData(*device_data);
      if (source.handle != gd.bloom_target || !gd.bloom_texture)
         return;
      auto* const dest_texture = reinterpret_cast<ID3D11Texture2D*>(dest.handle);
      D3D11_TEXTURE2D_DESC dest_desc = {};
      dest_texture->GetDesc(&dest_desc);
      const uint32_t scale = gd.bloom_scale;
      auto shadow = std::ranges::find(gd.bloom_shadows, dest.handle, &VanquishGameDeviceData::BloomShadow::game_texture);
      if (shadow == gd.bloom_shadows.end())
      {
         D3D11_TEXTURE2D_DESC desc = {};
         gd.bloom_texture->GetDesc(&desc);
         desc.Width = dest_desc.Width * scale;
         desc.Height = dest_desc.Height * scale;
         desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
         VanquishGameDeviceData::BloomShadow created{.game_texture = dest.handle};
         ComPtr<ID3D11Device> native_device;
         gd.bloom_texture->GetDevice(native_device.put());
         if (FAILED(native_device->CreateTexture2D(&desc, nullptr, created.texture.put())) || FAILED(native_device->CreateShaderResourceView(created.texture.get(), nullptr, created.srv.put())))
            return;
         shadow = gd.bloom_shadows.insert(gd.bloom_shadows.end(), std::move(created));
      }
      const D3D11_BOX box = (source_box ? D3D11_BOX{.left = source_box->left * scale, .top = source_box->top * scale, .front = 0, .right = source_box->right * scale, .bottom = source_box->bottom * scale, .back = 1}
                                        : D3D11_BOX{.left = 0, .top = 0, .front = 0, .right = dest_desc.Width * scale, .bottom = dest_desc.Height * scale, .back = 1});
      auto* const native_device_context = reinterpret_cast<ID3D11DeviceContext*>(cmd_list->get_native());
      const UINT dest_x = (dest_box ? (dest_box->left * scale) : 0);
      const UINT dest_y = (dest_box ? (dest_box->top * scale) : 0);
      native_device_context->CopySubresourceRegion(shadow->texture.get(), 0, dest_x, dest_y, 0, gd.bloom_texture.get(), 0, &box);
   }

   static bool OnCopyResource(reshade::api::command_list* cmd_list, reshade::api::resource source, reshade::api::resource dest)
   {
      MirrorBloomCopy(cmd_list, source, dest, nullptr, nullptr);
      return false;
   }

   static bool OnCopyTextureRegion(reshade::api::command_list* cmd_list, reshade::api::resource source, uint32_t source_subresource, const reshade::api::subresource_box* source_box, reshade::api::resource dest, uint32_t dest_subresource, const reshade::api::subresource_box* dest_box, reshade::api::filter_mode filter)
   {
      MirrorBloomCopy(cmd_list, source, dest, source_box, dest_box);
      return false;
   }

   // A bloom chain draw (filter05, the blurs, a level copy). Records the bloom target at the chain's first draw (the scene into it);
   // with Luma Bloom, draws into "bloom_texture" instead: the bloom target's size times the scene's height / 720 (level 0 a quarter
   // of the screen, as at 1280x720), the viewport scaled alike, sampling the scene or a "bloom_shadows" texture, the game's shaders and
   // constants unchanged (their taps are UV fractions: the same footprint, sampled finer). False, for the game's draw, on anything else.
   static bool DrawBloomPass(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, CommandListData& cmd_list_data, DeviceData& device_data, reshade::api::shader_stage stages, bool is_bloom_source, const std::function<void()>& draw, bool* updated_cbuffers)
   {
      auto& gd = GetGameDeviceData(device_data);
      // The copies only matter into a known bloom target
      if (!is_bloom_source && (!g_luma_bloom_enable || gd.bloom_target == 0))
         return false;
      ComPtr<ID3D11RenderTargetView> rtv;
      native_device_context->OMGetRenderTargets(1, rtv.put(), nullptr);
      if (!rtv)
         return false;
      ComPtr<ID3D11Resource> target;
      rtv->GetResource(target.put());
      if (!is_bloom_source && uint64_t(target.get()) != gd.bloom_target)
         return false;
      ComPtr<ID3D11ShaderResourceView> source_srv;
      native_device_context->PSGetShaderResources(0, 1, source_srv.put());
      if (!source_srv)
         return false;
      const uint2 target_size = GetViewTextureSize(rtv.get());
      const uint2 source_size = GetViewTextureSize(source_srv.get());
      const bool from_scene = is_bloom_source && source_size.y > target_size.y;
      if (from_scene)
      {
         gd.bloom_target = uint64_t(target.get());
      }
      if (!g_luma_bloom_enable || uint64_t(target.get()) != gd.bloom_target)
         return false;

      if (from_scene)
      {
         const uint32_t scale = ((g_bloom_scale_override > 0) ? uint32_t(g_bloom_scale_override) : (std::max)(1u, uint32_t(std::lround(source_size.y / 720.0))));
         if (scale != gd.bloom_scale || !gd.bloom_rtv || GetViewTextureSize(gd.bloom_rtv.get()) != uint2{target_size.x * scale, target_size.y * scale})
         {
            gd.bloom_rtv.reset();
            gd.bloom_srv.reset();
            gd.bloom_texture.reset();
            gd.bloom_shadows.clear();
            // 16 bit UNORM: the 8 bit target's [0, 1] range (the chain relies on its clamp) without its banding at the finer size
            const CD3D11_TEXTURE2D_DESC desc(DXGI_FORMAT_R16G16B16A16_UNORM, target_size.x * scale, target_size.y * scale, 1, 1, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET);
            if (FAILED(native_device->CreateTexture2D(&desc, nullptr, gd.bloom_texture.put())) || FAILED(native_device->CreateRenderTargetView(gd.bloom_texture.get(), nullptr, gd.bloom_rtv.put())) ||
                FAILED(native_device->CreateShaderResourceView(gd.bloom_texture.get(), nullptr, gd.bloom_srv.put())))
            {
               gd.bloom_rtv.reset();
            }
            gd.bloom_scale = scale;
         }
      }
      if (!gd.bloom_rtv)
         return false;
      ID3D11ShaderResourceView* source = source_srv.get();
      if (!from_scene)
      {
         ComPtr<ID3D11Resource> game_source;
         source_srv->GetResource(game_source.put());
         const auto shadow = std::ranges::find(gd.bloom_shadows, uint64_t(game_source.get()), &VanquishGameDeviceData::BloomShadow::game_texture);
         // Not copied yet (Luma Bloom just turned on): the game's pass, this frame
         if (shadow == gd.bloom_shadows.end())
            return false;
         source = shadow->srv.get();
      }

      DrawStateStack<DrawStateStackType::FullGraphics> graphics_state;
      graphics_state.Cache(native_device_context, device_data.uav_max_count);
      D3D11_VIEWPORT viewports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
      UINT viewport_count = std::size(viewports);
      native_device_context->RSGetViewports(&viewport_count, viewports);
      // The origin (dgVoodoo's 0.5) scales too: its quads carry D3D9's half pixel offset in clip space, which scales with the viewport.
      for (UINT i = 0; i < viewport_count; i++)
      {
         viewports[i].TopLeftX *= gd.bloom_scale;
         viewports[i].TopLeftY *= gd.bloom_scale;
         viewports[i].Width *= gd.bloom_scale;
         viewports[i].Height *= gd.bloom_scale;
      }
      native_device_context->RSSetViewports(viewport_count, viewports);
      D3D11_RECT scissors[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
      UINT scissor_count = std::size(scissors);
      native_device_context->RSGetScissorRects(&scissor_count, scissors);
      for (UINT i = 0; i < scissor_count; i++)
      {
         scissors[i].left *= gd.bloom_scale;
         scissors[i].top *= gd.bloom_scale;
         scissors[i].right *= gd.bloom_scale;
         scissors[i].bottom *= gd.bloom_scale;
      }
      native_device_context->RSSetScissorRects(scissor_count, scissors);
      ID3D11RenderTargetView* const bloom_rtv = gd.bloom_rtv.get();
      native_device_context->OMSetRenderTargets(1, &bloom_rtv, nullptr);
      native_device_context->PSSetShaderResources(0, 1, &source);
      // filter05 (BloomSource_0x8B6A8D61) averages the whole footprint of each texel; the blurs (BlurV_0xCC2E3748, BlurH_0x55E02134)
      // widen each tap to a game texel at the scale (CustomData3)
      SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, stages, LumaConstantBufferType::LumaSettings);
      SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, stages, LumaConstantBufferType::LumaData, is_bloom_source ? 1 : 0, 0, float(gd.bloom_scale));
      *updated_cbuffers = true;
      draw();
      graphics_state.Restore(native_device_context);
      // The game's quads start about a tenth of a game texel before its first texel's centre, enough for it but short of the first Luma
      // row and column, a third of a game texel further out (no origin covers both ends: the quads are a little short of the target).
      // Those repeat their neighbours, else they stay black and the blurs' clamped taps darken the top and left edge.
      const uint2 size = {target_size.x * gd.bloom_scale, target_size.y * gd.bloom_scale};
      const D3D11_BOX row = {.left = 0, .top = 1, .front = 0, .right = size.x, .bottom = 2, .back = 1};
      native_device_context->CopySubresourceRegion(gd.bloom_texture.get(), 0, 0, 0, 0, gd.bloom_texture.get(), 0, &row);
      const D3D11_BOX column = {.left = 1, .top = 0, .front = 0, .right = 2, .bottom = size.y, .back = 1};
      native_device_context->CopySubresourceRegion(gd.bloom_texture.get(), 0, 0, 0, 0, gd.bloom_texture.get(), 0, &column);
      return true;
   }

public:
   // Core's motion vector events (the motion vector target in every blend state, the tracked bindings, the vc4 copies; the mask target
   // is in every blend state too, see "REACTIVE_SLOT"), the scene depth's extra bind and Luma Bloom's copies
   static void SetEvents(bool enable)
   {
      SceneMotionVectors::SetEvents<GetSceneMotionVectors>(enable);
      if (enable)
      {
         reshade::register_event<reshade::addon_event::create_resource>(OnCreateResource);
         reshade::register_event<reshade::addon_event::copy_resource>(OnCopyResource);
         reshade::register_event<reshade::addon_event::copy_texture_region>(OnCopyTextureRegion);
      }
      else
      {
         reshade::unregister_event<reshade::addon_event::create_resource>(OnCreateResource);
         reshade::unregister_event<reshade::addon_event::copy_resource>(OnCopyResource);
         reshade::unregister_event<reshade::addon_event::copy_texture_region>(OnCopyTextureRegion);
      }
   }

   void OnInit(bool async) override
   {
      // The canvas stays gamma encoded (1 = paper white, gamma 2.2 in and out as the other dgVoodoo mods). The gather replacement does
      // the HDR display map and pre-scales the scene by the UI paper white, which the composition applies to the canvas.
      std::vector<ShaderDefineData> game_shader_defines_data = {
         {"TONEMAP_TYPE", '1', true, false, "0 - SDR: Vanilla (clamped reference)\n1 - HDR: extended grade + C-infinity display map", 1},
      };
      shader_defines_data.append_range(game_shader_defines_data);
      assert(shader_defines_data.size() < MAX_SHADER_DEFINES);
      GetShaderDefineData(POST_PROCESS_SPACE_TYPE_HASH).SetDefaultValue('0');
      GetShaderDefineData(EARLY_DISPLAY_ENCODING_HASH).SetDefaultValue('0');
      GetShaderDefineData(VANILLA_ENCODING_TYPE_HASH).SetDefaultValue('1');
      GetShaderDefineData(GAMMA_CORRECTION_TYPE_HASH).SetDefaultValue('1');
      GetShaderDefineData(UI_DRAW_TYPE_HASH).SetDefaultValue('2');

      // dgVoodoo's translated shaders use b3 (its state) and b4 (the D3D9 constants)
      luma_settings_cbuffer_index = 13;
      luma_data_cbuffer_index = 12;
      luma_ui_cbuffer_index = -1;

      // SMAA: Core registers its 6 passes ("Luma_SMAA_impl.hlsl"); the predication mask from the depth prepass
      native_shaders_definitions.emplace(CompileTimeStringHash("Vanquish Depth Edges CS"), ShaderDefinition("Luma_Vanquish_DepthEdges", reshade::api::pipeline_subobject_type::compute_shader));
      // SMAA T2x's own passes ("SMAA_T2X" in Luma_SMAA_impl.hlsl). The edge detection and the vertex shaders are 1x's.
      native_shaders_definitions.emplace(CompileTimeStringHash("Vanquish SMAA T2x Blending Weight Calculation PS"),
         ShaderDefinition{"Luma_SMAA_impl", reshade::api::pipeline_subobject_type::pixel_shader, nullptr, "smaa_blending_weight_calculation_ps", {{"SMAA_T2X", "1"}}});
      native_shaders_definitions.emplace(CompileTimeStringHash("Vanquish SMAA T2x Neighborhood Blending PS"),
         ShaderDefinition{"Luma_SMAA_impl", reshade::api::pipeline_subobject_type::pixel_shader, nullptr, "smaa_neighborhood_blending_ps", {{"SMAA_T2X", "1"}}});
      native_shaders_definitions.emplace(CompileTimeStringHash("Vanquish SMAA T2x Resolve PS"),
         ShaderDefinition{"Luma_SMAA_impl", reshade::api::pipeline_subobject_type::pixel_shader, nullptr, "smaa_resolve_ps", {{"SMAA_T2X", "1"}}});
      native_shaders_definitions.emplace(CompileTimeStringHash("Vanquish Motion Vector Fill CS"), ShaderDefinition("Luma_Vanquish_MotionVectorFill", reshade::api::pipeline_subobject_type::compute_shader));
      // Max blended after a subtractive UI draw (see "subtracts")
      native_shaders_definitions.emplace(CompileTimeStringHash("Vanquish Draw Black PS"), ShaderDefinition{"Luma_DrawColor_PS", reshade::api::pipeline_subobject_type::pixel_shader, nullptr, nullptr, {{"COLOR", "float4(0.0, 0.0, 0.0, 0.0)"}}});

      sr_game_tooltip = "Needs Anti-Aliasing off in the game's options. DLSS, FSR and XeSS require Luma-Upscaler.exe next to the game's exe (Luma TAA doesn't).\n";

#if DEVELOPMENT
      Mcp::RegisterToggles({
         {"hide_ui", &g_hide_ui},
         {"smaa_enable", &g_smaa_enable},
         {"smaa_t2x", &g_smaa_t2x},
         {"luma_bloom_enable", &g_luma_bloom_enable},
         {"smaa_predication", &g_smaa_predication},
         {"smaa_pred_debug", &g_smaa_pred_debug},
         {"mv_debug_view", &g_mv_debug_view},
         {"mv_enable", &g_mv_enable},
         {"sr_reactive_enable", &g_sr_reactive_enable},
         {"sr_reactive_debug_view", &g_sr_reactive_debug_view},
         {"fix_frame_rate_bugs", &FrameRateFixes::enabled},
      });
      Mcp::RegisterValues({{"smaa_pred_tolerance", &g_smaa_pred_tolerance, 0.002f, 0.2f}, {"rcas_sharpness", &g_rcas_sharpness, 0.f, 1.f}, {"sr_reactive_scale", &g_sr_reactive_scale, 0.f, 4.f}, {"sr_reactive_threshold", &g_sr_reactive_threshold, 0.f, 1.f}});
      Mcp::RegisterTextures({MCP_GAME_TEXTURE("mv.velocity", mv.texture), MCP_GAME_TEXTURE("mv.depth", mv.device_depth), MCP_GAME_TEXTURE("sr.reactive", mv.reactive), MCP_GAME_TEXTURE("sr.transparency", mv.transparency), MCP_GAME_TEXTURE("sr.draws_mask", mv.reactive_target), MCP_GAME_TEXTURE("bloom.target", bloom_texture)});
      Mcp::RegisterInts({{"bloom_scale_override", &g_bloom_scale_override, 0, 4}});
#endif

      default_luma_global_game_settings.Dithering = 1.f;
      // The gather pass' grading, neutral
      default_luma_global_game_settings.Exposure = 1.f;
      default_luma_global_game_settings.Contrast = 1.f;
      default_luma_global_game_settings.Saturation = 1.f;
      default_luma_global_game_settings.HighlightsDesaturation = 0.f;
      default_luma_global_game_settings.ColorGradingIntensity = 1.f;
      default_luma_global_game_settings.BloomIntensity = 1.f;
      default_luma_global_game_settings.VideoAutoHDREnable = 1.f;
      default_luma_global_game_settings.VideoAutoHDRBoost = 0.5f;
      cb_luma_global_settings.GameSettings = default_luma_global_game_settings;
      SetEvents(true);
      g_fixes_installed = FrameRateFixes::Install();
   }

   void OnCreateDevice(ID3D11Device* native_device, DeviceData& device_data) override
   {
      device_data.game = new VanquishGameDeviceData;
#if DEVELOPMENT
      Mcp::RegisterCounters({{ "sr.mask_draws",
         &GetGameDeviceData(device_data).last_reactive_draws }},
         &device_data);
      // The last complete frame's motion vector counts ("mv.*")
      SceneMotionVectors::RegisterDevValues(GetGameDeviceData(device_data).mv, &device_data);
#endif
   }

   std::unique_ptr<std::byte[]> PatchShaderBytecodeSync(const std::byte* code, size_t& size, reshade::api::pipeline_subobject_type type, uint64_t shader_hash, const std::byte* shader_object, size_t shader_object_size) override
   {
      if (type != reshade::api::pipeline_subobject_type::pixel_shader)
         return nullptr;
      if (velocity_pixel_shaders.contains(uint32_t(shader_hash)))
         return DXBC::ClampOutput0(code, &size, DXBC::Output0Clamp::UNORM8);
      // Every other float target 0 (dgVoodoo's own R32_UINT outputs aside), MGR:R's: the game's targets are A8R8G8B8 (upgraded to fp16,
      // which doesn't clamp blend inputs), R32F or G32R32F. Negative colour and alpha above 1 would make the SRC_ALPHA/INV_SRC_ALPHA
      // blends' destination weight negative; colour above 1 is the scene's HDR and stays.
      std::vector<DXBC::Chunk> chunks;
      std::vector<DXBC::SignatureElement> outputs;
      DXBC::Chunk* output_signature = nullptr;
      if (DXBC::ReadChunks(reinterpret_cast<const uint8_t*>(shader_object), shader_object_size, &chunks))
      {
         output_signature = DXBC::FindChunk(&chunks, DXBC::FourCC("OSGN"));
      }
      if (!output_signature || !DXBC::ReadSignature(output_signature->data, &outputs))
         return nullptr;
      const auto target = std::ranges::find_if(outputs, [](const DXBC::SignatureElement& element)
         { return MotionVectorPatch::IsTarget(element) && element.reg == 0; });
      if (target == outputs.end() || target->component_type != D3D_REGISTER_COMPONENT_FLOAT32)
         return nullptr;
      return DXBC::ClampOutput0(code, &size, DXBC::Output0Clamp::BLEND_INPUT, target->mask);
   }

   DrawOrDispatchOverrideType OnDrawOrDispatch(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, CommandListData& cmd_list_data, DeviceData& device_data, reshade::api::shader_stage stages, const ShaderHashesList<OneShaderPerPipeline>& original_shader_hashes, bool is_custom_pass, bool& updated_cbuffers, std::function<void()>* original_draw_dispatch_func) override
   {
      auto& game_device_data = GetGameDeviceData(device_data);
      const bool is_immediate = native_device_context->GetType() == D3D11_DEVICE_CONTEXT_IMMEDIATE;
      const bool can_draw = is_immediate && original_draw_dispatch_func && *original_draw_dispatch_func;
      const auto bound_render_target = [&]
      {
         ComPtr<ID3D11RenderTargetView> rtv;
         native_device_context->OMGetRenderTargets(1, rtv.put(), nullptr);
         ComPtr<ID3D11Resource> resource;
         if (rtv)
         {
            rtv->GetResource(resource.put());
         }
         return resource;
      };
      // Every draw of the frame goes through here: one lookup for a draw in no list, each list tested once
      const bool is_listed = original_shader_hashes.Contains(shader_hashes_Listed);
      const auto is_in = [&](const ShaderHashesList<>& list)
      {
         return is_listed && original_shader_hashes.Contains(list);
      };
      const bool is_depth_prepass = is_in(shader_hashes_DepthPrepass);
      // The first prepass draw of the frame names its target (not returned: the scene opens at it below)
      if (g_smaa_enable && is_immediate && !game_device_data.depth_prepass && is_depth_prepass)
      {
         if (const auto resource = bound_render_target())
         {
            resource->QueryInterface(game_device_data.depth_prepass.put());
         }
      }
      // DLAA/FSR/XeSS/Luma TAA: the scene opens at the depth prepass' first draw and its mesh draws take motion vectors (see
      // "DrawWithMotionVectors"), else the viewport jitter and the alpha blended ones their masks; the fill and the upscaler run at the
      // gather pass
      if (game_device_data.mv.active && is_immediate && !game_device_data.has_drawn_gather && (game_device_data.mv.scene_open || (!game_device_data.mv.scene_done && is_depth_prepass)))
      {
         com_ptr<ID3D11DepthStencilView> dsv;
         native_device_context->OMGetRenderTargets(0, nullptr, &dsv);
         if (!game_device_data.mv.scene_open && dsv && dsv.get() != game_device_data.open_refused_dsv)
         {
            OpenScene(native_device, native_device_context, device_data, IsReactiveMaskUsed(device_data), dsv.get());
         }
         if (game_device_data.mv.scene_open)
         {
            const bool scene_draw = IsSceneDepthTested(native_device_context, &game_device_data, dsv);
            if (game_device_data.jitter != std::array<float, 2>{} || game_device_data.viewport_jittered)
            {
               JitterSceneViewport(native_device_context, &game_device_data, scene_draw);
            }
            const bool has_vertex_shader = (stages & reshade::api::shader_stage::vertex) == reshade::api::shader_stage::vertex;
            if (scene_draw && can_draw && !is_custom_pass && has_vertex_shader && original_shader_hashes.pixel_shaders[0] != SHADER_HASH_NONE)
            {
               com_ptr<ID3D11RenderTargetView> rtvs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
               native_device_context->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, &rtvs[0], nullptr);
               game_device_data.mv.object_motion = g_mv_enable;
               // The draws that write depth (the depth prepass' meshes; the G-buffer's draws have several targets) and the alpha blended
               // ones (hair, smoke: their own motion blended by their alpha), into the prepass' target or the gather pass' scene (another
               // 8-bit target of the scene's depth would take the second target's place)
               bool offered = game_device_data.depth_write;
               if (!offered)
               {
                  SceneMotionVectors::ClassifyBoundBlend(native_device_context, &game_device_data.mv);
                  offered = game_device_data.mv.alpha_blended;
               }
               if (offered && rtvs[0] && game_device_data.mv.scene_color && rtvs[0] != game_device_data.mv.scene_rtv && rtvs[0] != game_device_data.mv.second_scene_rtv)
               {
                  com_ptr<ID3D11Resource> color;
                  rtvs[0]->GetResource(&color);
                  offered = color == game_device_data.mv.scene_color || color == game_device_data.scene_color_hints[0] || color == game_device_data.scene_color_hints[1];
               }
               const bool motion_vectors = offered && SceneMotionVectors::DrawWithMotionVectors<MotionVectorGame>(&game_device_data.mv, native_device, native_device_context, cmd_list_data, device_data, original_shader_hashes, *original_draw_dispatch_func, rtvs, dsv.get());
               // An alpha blending state blends the motion vector target by the alpha written there: a draw that doesn't write it
               // must not have it bound
               if (!motion_vectors && rtvs[MotionVectorPatches::target_slot] && rtvs[MotionVectorPatches::target_slot] == game_device_data.mv.rtv)
               {
                  SceneMotionVectors::ClassifyBoundBlend(native_device_context, &game_device_data.mv);
                  if (game_device_data.mv.alpha_blended)
                  {
                     rtvs[MotionVectorPatches::target_slot].reset();
                     ID3D11RenderTargetView* targets[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
                     for (UINT slot = 0; slot < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; slot++)
                     {
                        targets[slot] = rtvs[slot].get();
                     }
                     native_device_context->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, targets, dsv.get());
                  }
               }
#if DEVELOPMENT
               Mcp::Annotate(cmd_list_data, motion_vectors ? "mv" : "unpatched");
               if (const SceneMotionVectors::MotionVectorReject reject = std::exchange(game_device_data.mv.draw_reject, SceneMotionVectors::REJECT_COUNT); reject != SceneMotionVectors::REJECT_COUNT)
               {
                  Mcp::Annotate(cmd_list_data, "mv_reject", int(reject));
               }
#endif
               if (motion_vectors)
                  return DrawOrDispatchOverrideType::Replaced;
               if (game_device_data.reactive_ready && DrawWithReactiveMask(native_device, native_device_context, device_data, original_shader_hashes.pixel_shaders[0], cmd_list_data.pipeline_state_original_pixel_shader, *original_draw_dispatch_func, rtvs, dsv.get()))
                  return DrawOrDispatchOverrideType::Replaced;
            }
         }
      }
      // Every draw without a patched shader: the game's own, if the last patched draw's are still bound
      if (is_immediate)
      {
         PatchedDraws::RestoreGameShader(native_device_context, &game_device_data.mv.bound_vertex_shader);
         PatchedDraws::RestoreGameShader(native_device_context, &game_device_data.mv.bound_pixel_shader);
      }
      // Luma Bloom's chain and its level copies (the copy shader also draws them into the bloom target); false: the game's draw
      const bool is_bloom_source = is_in(shader_hashes_BloomSource);
      const bool is_copy = is_in(shader_hashes_Copy);
      if (can_draw && (is_bloom_source || is_copy || is_in(shader_hashes_BloomBlur)) &&
          DrawBloomPass(native_device, native_device_context, cmd_list_data, device_data, stages, is_bloom_source, *original_draw_dispatch_func, &updated_cbuffers))
         return DrawOrDispatchOverrideType::Replaced;
      if (is_in(shader_hashes_Gather))
      {
         device_data.has_drawn_main_post_processing = true;
         game_device_data.has_drawn_gather = true;
         if (!can_draw)
            return DrawOrDispatchOverrideType::None;
         game_device_data.gather_target = uint64_t(bound_render_target().get());
         // Luma Bloom's texture in the bloom target's place (t1), read with a B-spline (LumaData.CustomData2 1)
         com_ptr<ID3D11ShaderResourceView> game_bloom;
         native_device_context->PSGetShaderResources(1, 1, &game_bloom);
         ID3D11ShaderResourceView* luma_bloom = nullptr;
         if (g_luma_bloom_enable && game_device_data.bloom_srv && game_bloom)
         {
            com_ptr<ID3D11Resource> game_bloom_texture;
            game_bloom->GetResource(&game_bloom_texture);
            if (uint64_t(game_bloom_texture.get()) == game_device_data.bloom_target)
            {
               luma_bloom = game_device_data.bloom_srv.get();
            }
         }
         SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, stages, LumaConstantBufferType::LumaSettings);
         SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, stages, LumaConstantBufferType::LumaData, 0, luma_bloom ? 1 : 0);
         updated_cbuffers = true;
         // The upscaler's output in the scene's place (t0)
         com_ptr<ID3D11ShaderResourceView> scene;
         native_device_context->PSGetShaderResources(0, 1, &scene);
         game_device_data.scene_size_hint = GetViewTextureSize(scene.get());
         com_ptr<ID3D11Resource> scene_color;
         if (scene)
         {
            scene->GetResource(&scene_color);
         }
         if (scene_color && scene_color != game_device_data.scene_color_hints[0])
         {
            game_device_data.scene_color_hints[1] = std::move(game_device_data.scene_color_hints[0]);
            game_device_data.scene_color_hints[0] = std::move(scene_color);
         }
         ID3D11ShaderResourceView* sr_output = nullptr;
         // The scene ends here: the fill, then the upscaler, before this pass reads the scene
         if (std::exchange(game_device_data.mv.scene_open, false))
         {
            game_device_data.mv.scene_done = true;
#if DEVELOPMENT
            game_device_data.mv.stats.ended_by = uint32_t(original_shader_hashes.pixel_shaders[0]);
#endif
            if (game_device_data.sr_active)
            {
               const std::shared_lock lock_shader_objects(s_mutex_shader_objects);
               sr_output = DrawUpscaler(native_device, native_device_context, device_data, scene.get());
#if DEVELOPMENT
               game_device_data.mv.stats.sr_draws += (sr_output != nullptr);
#endif
            }
            // SMAA T2x's velocity, read after this pass ("DrawSMAAOnScene")
            else if (game_device_data.t2x_active && std::exchange(game_device_data.mv.fill_pending, false))
            {
               const std::shared_lock lock_shader_objects(s_mutex_shader_objects);
               DrawStateStack<DrawStateStackType::FullGraphics> graphics_state;
               DrawStateStack<DrawStateStackType::Compute> compute_state;
               graphics_state.Cache(native_device_context, device_data.uav_max_count);
               compute_state.Cache(native_device_context, device_data.uav_max_count);
               game_device_data.mv_filled = FillMotionVectors(native_device, native_device_context, device_data, /* write_reactive */ false);
               compute_state.Restore(native_device_context);
               graphics_state.Restore(native_device_context);
            }
         }
         if (sr_output)
         {
            native_device_context->PSSetShaderResources(0, 1, &sr_output);
         }
         if (luma_bloom)
         {
            native_device_context->PSSetShaderResources(1, 1, &luma_bloom);
         }
         (*original_draw_dispatch_func)();
         if (sr_output)
         {
            ID3D11ShaderResourceView* const game_scene = scene.get();
            native_device_context->PSSetShaderResources(0, 1, &game_scene);
         }
         if (luma_bloom)
         {
            ID3D11ShaderResourceView* const game_bloom_srv = game_bloom.get();
            native_device_context->PSSetShaderResources(1, 1, &game_bloom_srv);
         }
         SnapshotScene(native_device, native_device_context, &game_device_data);
         if (g_smaa_enable && !device_data.has_drawn_sr && game_device_data.has_scene_snapshot)
         {
            ComPtr<ID3D11RenderTargetView> target_rtv;
            native_device_context->OMGetRenderTargets(1, target_rtv.put(), nullptr);
            DrawSMAAOnScene(native_device, native_device_context, device_data, target_rtv.get());
            // Again, so the UI clamp's floor is the antialiased scene (MGR:R's order)
            SnapshotScene(native_device, native_device_context, &game_device_data);
         }
         // RCAS after SMAA or the upscaler, the same way
         if (g_rcas_sharpness > 0.f && game_device_data.has_scene_snapshot)
         {
            const std::shared_lock lock_shader_objects(s_mutex_shader_objects);
            if (PrepareRCAS(native_device, device_data))
            {
               ComPtr<ID3D11RenderTargetView> target_rtv;
               native_device_context->OMGetRenderTargets(1, target_rtv.put(), nullptr);
               DrawRCAS(native_device_context, device_data, game_device_data.scene_srv.get(), target_rtv.get(), g_rcas_sharpness);
               SnapshotScene(native_device, native_device_context, &game_device_data);
            }
         }
         return DrawOrDispatchOverrideType::Replaced;
      }
      const bool is_game_aa = is_in(shader_hashes_GameAA);
      const bool is_canvas_writer = game_device_data.has_drawn_gather && (is_game_aa || is_in(shader_hashes_AAOffCopy));
      if (can_draw && is_canvas_writer)
      {
         game_device_data.canvas_target = uint64_t(bound_render_target().get());
      }
      // Hide UI: cancel the game's draws on the gather target or the canvas after the gather pass (HUD, menus, subtitles), not what writes
      // the canvas, the movies (see the copy below) or the present blit. An original pixel shader hash is what separates a game draw.
      const auto is_hidden_ui_draw = [&]
      {
         if (!g_hide_ui || !can_draw || !game_device_data.has_drawn_gather || original_shader_hashes.pixel_shaders[0] == SHADER_HASH_NONE ||
             is_canvas_writer || is_in(shader_hashes_DgVoodooPresent))
            return false;
         const uint64_t target = uint64_t(bound_render_target().get());
         return target != 0 && (target == game_device_data.gather_target || target == game_device_data.canvas_target);
      };
      if (!is_copy && is_hidden_ui_draw())
         return DrawOrDispatchOverrideType::Replaced;
      // SMAA or the upscaler ran on the gather target: the game's FXAA or edge AA (t0 that target, a new target out) becomes a copy
      if (can_draw && (game_device_data.has_drawn_smaa || device_data.has_drawn_sr) && is_game_aa)
      {
         auto* const copy_vs = FindShader(device_data.native_vertex_shaders, CompileTimeStringHash("Copy VS"));
         auto* const copy_ps = FindShader(device_data.native_pixel_shaders, CompileTimeStringHash("Copy PS"));
         ComPtr<ID3D11ShaderResourceView> source;
         native_device_context->PSGetShaderResources(0, 1, source.put());
         ComPtr<ID3D11RenderTargetView> target_rtv;
         native_device_context->OMGetRenderTargets(1, target_rtv.put(), nullptr);
         if (copy_vs && copy_ps && source && target_rtv)
         {
            const uint2 size = GetViewTextureSize(source.get());
            DrawStateStack<DrawStateStackType::FullGraphics> draw_state_stack;
            draw_state_stack.Cache(native_device_context, device_data.uav_max_count);
            DrawCustomPixelShader(native_device_context, device_data.default_depth_stencil_state.get(), device_data.default_blend_state.get(), nullptr, copy_vs, copy_ps, source.get(), target_rtv.get(), size.x, size.y, false);
            draw_state_stack.Restore(native_device_context);
            return DrawOrDispatchOverrideType::Replaced;
         }
      }
      if (is_in(shader_hashes_VideoUnpack))
      {
         game_device_data.video_frame = bound_render_target();
         return DrawOrDispatchOverrideType::None;
      }
      // Every copy gets the flag: the luma data cbuffer stays bound between draws
      if (can_draw && is_copy)
      {
         bool is_video = false;
         if (game_device_data.video_frame)
         {
            ComPtr<ID3D11ShaderResourceView> srv;
            native_device_context->PSGetShaderResources(0, 1, srv.put());
            ComPtr<ID3D11Resource> source;
            if (srv)
            {
               srv->GetResource(source.put());
            }
            is_video = (source == game_device_data.video_frame);
         }
         if (!is_video && is_hidden_ui_draw())
            return DrawOrDispatchOverrideType::Replaced;
         SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, stages, LumaConstantBufferType::LumaSettings);
         SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, stages, LumaConstantBufferType::LumaData, is_video ? 1 : 0);
         updated_cbuffers = true;
         (*original_draw_dispatch_func)();
         return DrawOrDispatchOverrideType::Replaced;
      }
      // The present blit: clamp the UI to the scene before it, once. It always gets the flag: the luma data cbuffer stays bound between
      // draws.
      if (can_draw && is_in(shader_hashes_DgVoodooPresent))
      {
         const bool clamp_ui = game_device_data.has_scene_snapshot;
         game_device_data.has_scene_snapshot = false;
         SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, stages, LumaConstantBufferType::LumaSettings);
         SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, stages, LumaConstantBufferType::LumaData, clamp_ui ? 1 : 0);
         updated_cbuffers = true;
         ID3D11ShaderResourceView* const scene_srv = (clamp_ui ? game_device_data.scene_srv.get() : nullptr);
         native_device_context->PSSetShaderResources(1, 1, &scene_srv);
         (*original_draw_dispatch_func)();
         ID3D11ShaderResourceView* const null_srv = nullptr;
         native_device_context->PSSetShaderResources(1, 1, &null_srv);
         return DrawOrDispatchOverrideType::Replaced;
      }
      // The 8-bit UNORM targets floored a subtractive blend at 0, their fp16 mirrors don't: the pause menu's tint (uibaseshader 0CD3C0A6,
      // "SRC_ALPHA REV_SUBTRACT ONE") left negative red on the canvas. The game's draw, then max(target, 0) (P5S' way). Not only after
      // the gather pass: the pause menu's frames have none (a copy of the last scene).
      if (can_draw && original_shader_hashes.pixel_shaders[0] != SHADER_HASH_NONE)
      {
         com_ptr<ID3D11BlendState> blend_state;
         native_device_context->OMGetBlendState(&blend_state, nullptr, nullptr);
         D3D11_BLEND_DESC blend_desc = {};
         if (blend_state)
         {
            blend_state->GetDesc(&blend_desc);
         }
         const D3D11_RENDER_TARGET_BLEND_DESC& rt_blend = blend_desc.RenderTarget[0];
         const auto subtracts = [](D3D11_BLEND_OP op)
         { return op == D3D11_BLEND_OP_SUBTRACT || op == D3D11_BLEND_OP_REV_SUBTRACT; };
         if (rt_blend.BlendEnable && (subtracts(rt_blend.BlendOp) || subtracts(rt_blend.BlendOpAlpha)))
         {
            if (!game_device_data.max_blend_state)
            {
               D3D11_BLEND_DESC max_blend_desc = CD3D11_BLEND_DESC(D3D11_DEFAULT);
               max_blend_desc.RenderTarget[0] = {.BlendEnable = TRUE, .SrcBlend = D3D11_BLEND_ONE, .DestBlend = D3D11_BLEND_ONE, .BlendOp = D3D11_BLEND_OP_MAX, .SrcBlendAlpha = D3D11_BLEND_ONE, .DestBlendAlpha = D3D11_BLEND_ONE, .BlendOpAlpha = D3D11_BLEND_OP_MAX, .RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL};
               native_device->CreateBlendState(&max_blend_desc, &game_device_data.max_blend_state);
            }
            auto* const copy_vs = FindShader(device_data.native_vertex_shaders, CompileTimeStringHash("Copy VS"));
            auto* const black_ps = FindShader(device_data.native_pixel_shaders, CompileTimeStringHash("Vanquish Draw Black PS"));
            ComPtr<ID3D11RenderTargetView> target_rtv;
            native_device_context->OMGetRenderTargets(1, target_rtv.put(), nullptr);
            if (game_device_data.max_blend_state && copy_vs && black_ps && target_rtv)
            {
               (*original_draw_dispatch_func)();
               const uint2 size = GetViewTextureSize(target_rtv.get());
               DrawStateStack<DrawStateStackType::FullGraphics> draw_state_stack;
               draw_state_stack.Cache(native_device_context, device_data.uav_max_count);
               DrawCustomPixelShader(native_device_context, device_data.default_depth_stencil_state.get(), game_device_data.max_blend_state.get(), nullptr, copy_vs, black_ps, nullptr, target_rtv.get(), size.x, size.y);
               draw_state_stack.Restore(native_device_context);
               return DrawOrDispatchOverrideType::Replaced;
            }
         }
      }
      return DrawOrDispatchOverrideType::None;
   }

   void OnPresent(ID3D11Device* native_device, DeviceData& device_data) override
   {
      auto& game_device_data = GetGameDeviceData(device_data);
#if DEVELOPMENT
      // Ends Core's debug draw of a target about to be released or no longer shown
      const auto stop_debug_view = [&device_data](const auto* texture)
      {
         if (device_data.debug_draw_texture && device_data.debug_draw_texture.get() == texture)
         {
            device_data.debug_draw_texture = nullptr;
         }
      };
#endif
      game_device_data.has_scene_snapshot = false;
      game_device_data.has_drawn_gather = false;
      game_device_data.gather_target = 0;
      game_device_data.canvas_target = 0;
      game_device_data.depth_prepass.reset();
      game_device_data.has_drawn_smaa = false;

      // The upscaler: the history restarts after any frame it didn't draw (menus, loading, a scene that never opened); the selection and
      // the motion vector state are fixed here for the next frame. The scene jitters only after a frame it drew.
      const bool jitter_allowed = device_data.has_drawn_sr;
      game_device_data.sr_active = LatchSRFrame(device_data);
      // SMAA T2x runs on the upscalers' motion vectors and gives way to an upscaler. Turned off, its frames go, and the motion vectors
      // go as when no upscaler is picked.
      game_device_data.mv_filled = false;
      const bool t2x_was_active = game_device_data.t2x_active;
      game_device_data.t2x_active = g_smaa_enable && g_smaa_t2x && !game_device_data.sr_active;
      if (t2x_was_active && !game_device_data.t2x_active)
      {
         game_device_data.ReleaseT2xFrames();
         device_data.release_sr_resources = true;
      }
      const bool mv_active = game_device_data.sr_active || game_device_data.t2x_active;
      if (!game_device_data.mv.active.exchange(mv_active) && mv_active)
      {
         game_device_data.mv.constants.Restart();
      }
      // A scene no gather pass ended ends here: its jitter must not reach the next frame's draws before its first mesh
      game_device_data.mv.scene_open = false;
      game_device_data.mv.scene_done = false;
      game_device_data.mv.frame_ended = true;
      game_device_data.mv.fill_pending = false;
      game_device_data.open_refused_dsv = nullptr;
      game_device_data.viewport_jittered = false;
      game_device_data.reactive_ready = false;
#if DEVELOPMENT
      game_device_data.last_reactive_draws = std::exchange(game_device_data.reactive_draws, 0);
#endif
      game_device_data.jitter = {};
      if (game_device_data.sr_active && jitter_allowed)
      {
         const SRDrawLock sr = LockSRForDraw(device_data);
         if (sr.implementation && sr.implementation->IsReady(sr.instance_data))
         {
            game_device_data.jitter = GetSRJitter(device_data, sr.instance_data);
         }
      }
      // -1 at native resolution while the upscaler draws, 0 with XeSS (Core biases the game's samplers at bias 0, see "DllMain")
      const float mip_lod_bias = (game_device_data.jitter != std::array<float, 2>{} ? SR::GetMipLODBias(device_data.sr_type, device_data.output_resolution.y, device_data.output_resolution.y) : 0.f);
      SetTextureMipLodBias(nullptr, device_data, mip_lod_bias);
      // None picked: Core stopped the SR bridge's helper, our inputs and output go too (recreated when an upscaler is picked again)
      if (device_data.sr_type == SR::Type::None && device_data.release_sr_resources.exchange(false))
      {
         game_device_data.sr_output_srv.reset();
#if DEVELOPMENT
         stop_debug_view(game_device_data.mv.texture.get());
#endif
         const std::unique_lock lock(game_device_data.mv.mutex);
         game_device_data.mv.ReleaseMotionVectorTargets();
         game_device_data.scene_depth_srv.reset();
         game_device_data.scene_dsv.reset();
         game_device_data.scene_depth.reset();
         // The scene, its depth and the vc4 copies the object tables and the cameras hold (taken again at the next scene), so a resize
         // after None doesn't keep the old ones alive
         game_device_data.mv.depth.reset();
         game_device_data.mv.scene_color.reset();
         game_device_data.mv.scene_rtv.reset();
         game_device_data.mv.scene_size = {};
         game_device_data.mv.objects.Reset();
         game_device_data.mv.camera.reset();
         game_device_data.mv.previous_camera.reset();
         game_device_data.mv.fill_buffer.reset();
         game_device_data.mv.jitter_buffer.reset();
         game_device_data.mv.previous_constants = {}; // Its 4 MiB dynamic ring is address space too
         game_device_data.mv.constants.ReleasePool();
      }
      game_device_data.mv.constants.TrimPool(game_device_data.mv.active);
#if DEVELOPMENT
      game_device_data.mv.stats.constants_pool = uint32_t(game_device_data.mv.constants.pool.size());
      game_device_data.mv.last_stats = std::exchange(game_device_data.mv.stats, {});
#endif
      // DLSS, XeSS or none picked: no reader of the masks (recreated when FSR or Luma TAA is picked again)
      if (!IsReactiveMaskUsed(device_data) && game_device_data.mv.reactive_target)
      {
#if DEVELOPMENT
         stop_debug_view(game_device_data.mv.reactive.get());
#endif
         game_device_data.mv.ReleaseReactiveMasks();
      }
#if DEVELOPMENT
      // "MV Debug View": Core's debug draw of the target, absolute values in pixels. No lock: the motion vector draw creates the target
      // on the immediate context, which presents too
      if (g_mv_debug_view && game_device_data.mv.texture)
      {
         D3D11_TEXTURE2D_DESC desc;
         game_device_data.mv.texture->GetDesc(&desc);
         debug_draw_auto_clear_texture = false;
         debug_draw_options |= (uint32_t)DebugDrawTextureOptionsMask::Abs | (uint32_t)DebugDrawTextureOptionsMask::UVToPixelSpace;
         device_data.debug_draw_texture = game_device_data.mv.texture.get();
         device_data.debug_draw_texture_format = desc.Format;
         device_data.debug_draw_texture_size = {desc.Width, desc.Height, 1, 1};
      }
      else if (g_sr_reactive_debug_view && game_device_data.mv.reactive)
      {
         D3D11_TEXTURE2D_DESC desc;
         game_device_data.mv.reactive->GetDesc(&desc);
         debug_draw_auto_clear_texture = false;
         debug_draw_options |= (uint32_t)DebugDrawTextureOptionsMask::RedOnly;
         debug_draw_options &= ~((uint32_t)DebugDrawTextureOptionsMask::Abs | (uint32_t)DebugDrawTextureOptionsMask::UVToPixelSpace);
         device_data.debug_draw_texture = game_device_data.mv.reactive.get();
         device_data.debug_draw_texture_format = desc.Format;
         device_data.debug_draw_texture_size = {desc.Width, desc.Height, 1, 1};
      }
      else
      {
         stop_debug_view(game_device_data.mv.texture.get());
         stop_debug_view(game_device_data.mv.reactive.get());
      }
#endif
   }

   void LoadConfigs() override
   {
      reshade::get_config_value(nullptr, NAME, "SMAAEnable", g_smaa_enable);
      reshade::get_config_value(nullptr, NAME, "SMAAT2x", g_smaa_t2x);
      reshade::get_config_value(nullptr, NAME, "RCASSharpness", g_rcas_sharpness);
      reshade::get_config_value(nullptr, NAME, "LumaBloomEnable", g_luma_bloom_enable);
      reshade::get_config_value(nullptr, NAME, "FixFrameRateBugs", FrameRateFixes::enabled);
#if DEVELOPMENT // Development settings only (see "DrawImGuiSettings"): a saved "off" must not outlive its control
      reshade::get_config_value(nullptr, NAME, "SMAAPredication", g_smaa_predication);
      reshade::get_config_value(nullptr, NAME, "SMAAPredicationTolerance", g_smaa_pred_tolerance);
#endif
      reshade::get_config_value(nullptr, NAME, "Dithering", cb_luma_global_settings.GameSettings.Dithering);
      reshade::get_config_value(nullptr, NAME, "Exposure", cb_luma_global_settings.GameSettings.Exposure);
      reshade::get_config_value(nullptr, NAME, "Contrast", cb_luma_global_settings.GameSettings.Contrast);
      reshade::get_config_value(nullptr, NAME, "Saturation", cb_luma_global_settings.GameSettings.Saturation);
      reshade::get_config_value(nullptr, NAME, "HighlightsDesaturation", cb_luma_global_settings.GameSettings.HighlightsDesaturation);
      reshade::get_config_value(nullptr, NAME, "ColorGradingIntensity", cb_luma_global_settings.GameSettings.ColorGradingIntensity);
      reshade::get_config_value(nullptr, NAME, "BloomIntensity", cb_luma_global_settings.GameSettings.BloomIntensity);
      reshade::get_config_value(nullptr, NAME, "VideoAutoHDREnable", cb_luma_global_settings.GameSettings.VideoAutoHDREnable);
      reshade::get_config_value(nullptr, NAME, "VideoAutoHDRBoost", cb_luma_global_settings.GameSettings.VideoAutoHDRBoost);
   }

   void DrawImGuiSettings(DeviceData& device_data) override
   {
      auto& gs = cb_luma_global_settings.GameSettings;
      ImGui::SeparatorText("Anti-Aliasing");
      // The upscaler (Super Resolution, in the Settings tab) replaces SMAA: greyed, its value kept
      const bool sr_active = GetGameDeviceData(device_data).sr_active;
      ImGui::BeginDisabled(sr_active);
      DrawSettingCheckbox("SMAA Enable", "SMAAEnable", &g_smaa_enable, true, "SMAA anti-aliasing, in place of the game's FXAA or edge AA (Anti-Aliasing in the game's options) when those are on (not used with DLSS/FSR/XeSS or Luma TAA).", sr_active);
      ImGui::EndDisabled();
      ImGui::BeginDisabled(!g_smaa_enable || sr_active);
      DrawSettingCheckbox("SMAA T2x", "SMAAT2x", &g_smaa_t2x, false, "Smoother edges and less shimmering, by blending each frame with the previous one.\nCan look slightly softer in motion. Not used with DLSS/FSR/XeSS or Luma TAA.", sr_active);
      ImGui::EndDisabled();
#if DEVELOPMENT
      // Predication only relaxes the edge threshold back to ULTRA's on geometry, never below: off is strictly worse (MGR:R's finding)
      if (g_smaa_enable)
      {
         DrawSettingCheckbox("SMAA Predication", "SMAAPredication", &g_smaa_predication, true, "Edges from the depth prepass' geometry (gameplay only: the menus have no prepass).");
         ImGui::BeginDisabled(!g_smaa_predication);
         DrawSettingSlider("SMAA Predication Tolerance", "SMAAPredicationTolerance", &g_smaa_pred_tolerance, 0.02f, 0.002f, 0.2f, "Deviation from the local depth plane that counts as an edge, as a fraction of view depth. Lower = more edges.", "%.3f", ImGuiSliderFlags_Logarithmic);
         ImGui::EndDisabled();
         ImGui::Checkbox("SMAA Predication Debug View", &g_smaa_pred_debug);
         if (ImGui::IsItemHovered())
         {
            ImGui::SetTooltip("Show the predication mask (red) instead of the frame.\nWant: black on flat surfaces, red across silhouettes.");
         }
      }
#endif
      DrawSettingSlider("RCAS Sharpness", "RCASSharpness", &g_rcas_sharpness, 0.f, 0.f, 1.f, "Sharpening applied on top of anti-aliasing (0 = off).");

      // Read in Gather_0xBC9B20C2.ps_5_0.hlsl
      ImGui::SeparatorText("Grade");
      DrawSettingSlider("Exposure", "Exposure", &gs.Exposure, default_luma_global_game_settings.Exposure, 0.f, 2.f, "Overall image brightness (1 = vanilla).");
      DrawSettingSlider("Contrast", "Contrast", &gs.Contrast, default_luma_global_game_settings.Contrast, 0.f, 2.f, "Overall image contrast, HDR only (1 = vanilla).");
      DrawSettingSlider("Saturation", "Saturation", &gs.Saturation, default_luma_global_game_settings.Saturation, 0.f, 2.f, "Color saturation, HDR only (1 = vanilla).");
      DrawSettingSlider("Highlights Desaturation", "HighlightsDesaturation", &gs.HighlightsDesaturation, default_luma_global_game_settings.HighlightsDesaturation, 0.f, 1.f, "How far the brightest sources fade to neutral white, HDR only (0 = keep color at any brightness).");
      DrawSettingSlider("Color Grading Intensity", "ColorGradingIntensity", &gs.ColorGradingIntensity, default_luma_global_game_settings.ColorGradingIntensity, 0.f, 1.f, "Strength of the game's own color grading (1 = vanilla, 0 = neutral).");

      ImGui::SeparatorText("Bloom");
      DrawSettingCheckbox("Luma Bloom Enable", "LumaBloomEnable", &g_luma_bloom_enable, true, "Replaces the game's bloom with a higher resolution one (the same look, without its blocky, flickering glow).");
      DrawSettingSlider("Bloom Intensity", "BloomIntensity", &gs.BloomIntensity, default_luma_global_game_settings.BloomIntensity, 0.f, 2.f, "Bloom strength (1 = vanilla, 0 = none).");

      ImGui::SeparatorText("Effects");
      // Read in Copy_0x91A6C437.ps_5_0.hlsl, in HDR only
      DrawSettingCheckbox("Video AutoHDR", "VideoAutoHDREnable", &gs.VideoAutoHDREnable, default_luma_global_game_settings.VideoAutoHDREnable, "Adds HDR highlights to pre-rendered videos (HDR only).");

      ImGui::BeginDisabled(gs.VideoAutoHDREnable <= 0.5f);
      DrawSettingSlider("Video HDR Boost", "VideoAutoHDRBoost", &gs.VideoAutoHDRBoost, default_luma_global_game_settings.VideoAutoHDRBoost, 0.f, 1.f, "Video highlight strength (0 = off).");
      ImGui::EndDisabled();
      // Read in Present_0x880A17D3.ps_4_0.hlsl
      DrawSettingCheckbox("Dithering", "Dithering", &gs.Dithering, default_luma_global_game_settings.Dithering, "Reduces gradient banding.");

      ImGui::SeparatorText("UI");
      ImGui::Checkbox("Hide Gameplay UI", &g_hide_ui);
      if (ImGui::IsItemHovered())
      {
         ImGui::SetTooltip("Disables the in-game UI.");
      }

      ImGui::SeparatorText("Fixes");
      ImGui::BeginDisabled(!g_fixes_installed);
      DrawSettingCheckbox("Fix Frame Rate Bugs", "FixFrameRateBugs", &FrameRateFixes::enabled, true,
         "Gameplay runs as it does at 30 fps at any frame rate: homing, aiming, beam damage, stuns and timers.",
         !g_fixes_installed);
      ImGui::EndDisabled();
   }

#if DEVELOPMENT
   void DrawImGuiDevSettings(DeviceData& device_data) override
   {
      ImGui::Checkbox("MV Object Motion", &g_mv_enable);
      if (ImGui::IsItemHovered())
      {
         ImGui::SetTooltip("Off: the motion vector draws keep their own constants with last frame's camera (camera motion only), for A/B.");
      }
      ImGui::Checkbox("MV Debug View", &g_mv_debug_view);
      if (ImGui::IsItemHovered())
      {
         ImGui::SetTooltip("Shows the motion vectors' absolute value in pixels instead of the frame.");
      }
      ImGui::Checkbox("Reactive Mask", &g_sr_reactive_enable);
      if (ImGui::IsItemHovered())
      {
         ImGui::SetTooltip("FSR's and Luma TAA's reactive and transparency masks from the alpha blended draws.");
      }
      ImGui::Checkbox("Reactive Mask Debug View", &g_sr_reactive_debug_view);
      if (ImGui::IsItemHovered())
      {
         ImGui::SetTooltip("Shows the reactive mask (red) instead of the frame.");
      }
   }
#endif

   void PrintImGuiAbout() override
   {
      ImGui::PushTextWrapPos(0.f);
      ImGui::Text(
         "Luma for \"Vanquish\" is developed by DristoforColumb and is open source and free.\n"
         "It adds HDR, DLAA, FSR 3 or XeSS native anti-aliasing or Luma TAA, SMAA and RCAS sharpening.\n"
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
                  "\nMacLeod-Boynton hue emulation (RenoDX)"
                  "\nSMAA (Iryoku)"
                  "\nAMD FidelityFX (RCAS + FSR 3)"
                  "\nNVIDIA NGX (DLSS)"
                  "\nIntel XeSS"
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
      Globals::SetGlobals(cleared_project_name, "Vanquish Luma mod", "", mod_version);

      swapchain_format_upgrade_type = TextureFormatUpgradesType::AllowedEnabled;
      swapchain_upgrade_type = SwapchainUpgradeType::scRGB;

      // Exclusive fullscreen -> borderless, also when leaving it, as the other dgVoodoo mods
      prevent_fullscreen_state = true;
      force_borderless = true;

      // dgVoodoo forwards D3D9 SetGammaRamp to the OS device ramp, which distorts HDR output
      allow_disabling_gamma_ramp = true;

      // The upscalers' mip bias, Bayonetta's (the same engine): mode 3 biases only samplers at bias 0 (the game sets its own on some),
      // force_upgrade_linear_samplers the trilinear ones too
      enable_samplers_upgrade = true; // boot-time only (cannot be changed after device creation)
      samplers_upgrade_mode = 3;
      force_upgrade_linear_samplers = true;

      // Every colour target is A8R8G8B8 (FUN_00b968f0 and its siblings, offline). The scene and post targets become fp16 mirrors at creation
      // (the swapchain sized and shaped ones: the 320x176 bloom chain, the 8x8 exposure meter, the 256x1 grade table and the R32F/G32R32F
      // depth and shadow targets stay), as MGR:R's: the scene's lights carry values above 1 into the gather pass. Indirect (changing
      // dgVoodoo's own creation formats breaks its bookkeeping). The gather target and the canvas also upgrade by their writers: dgVoodoo
      // creates its D3D9 back buffer before it sizes the swapchain, so the size filters miss it at creation.
      texture_format_upgrades_type = TextureFormatUpgradesType::AllowedEnabled;
      enable_indirect_texture_format_upgrades = true;
      enable_chain_indirect_texture_format_upgrades = ChainTextureFormatUpgradesType::DirectDependencies;
      texture_upgrade_formats = {
         reshade::api::format::r8g8b8a8_typeless,
         reshade::api::format::r8g8b8a8_unorm,
      };
      for (const auto& list : {shader_hashes_Gather, shader_hashes_GameAA, shader_hashes_AAOffCopy})
      {
         for (const uint32_t hash : list.pixel_shaders)
         {
            auto_texture_format_upgrade_shader_hashes[hash] = {.rtv_slots = {0}};
         }
      }
      // "No1Px" as in the other dgVoodoo mods: the wrapper binds 1x1 placeholders in unused sampler slots, which pass the aspect filter
      texture_format_upgrades_2d_size_filters = (uint32_t)TextureFormatUpgrades2DSizeFilters::SwapchainResolution | (uint32_t)TextureFormatUpgrades2DSizeFilters::SwapchainAspectRatio | (uint32_t)TextureFormatUpgrades2DSizeFilters::No1Px;

#if DEVELOPMENT
      // Offline map of the post shaders (dgVoodoo 2.87.6), for traces and the DevKit. The in-game alpha test
      // variant comes first where known.
      for (const auto& [hash, name] : std::initializer_list<std::pair<const char*, const char*>>{
              {"BC9B20C2", "Gather (grade + bloom)"},
              {"021D5E4D", "Gather (grade + bloom)"},
              {"E004F806", "Gather (grade + bloom)"},
              {"91A6C437", "Copy (filtershadercopytex)"},
              {"EEA9CD70", "Movie"},
              {"B39B8ED0", "FXAA (exe)"},
              {"15857C80", "FXAA (exe)"},
              {"EB476A79", "FXAA (exe)"},
              {"32A44A9C", "Edge AA 2x (exe)"},
              {"AE5DF7AE", "Edge AA 4x (exe)"},
              {"75FCCD11", "Edge AA 8x (exe)"},
              {"E9A53CDA", "AA Off Copy (dgVoodoo)"},
              {"256DD653", "SSAO (exe)"},
              {"55E02134", "Bloom Blur (filter01)"},
              {"CC2E3748", "Bloom Blur (filter02)"},
              {"3B0C76B0", "Bloom Blur (filter02)"},
              {"EA483785", "Exposure Average (filter03)"},
              {"1D145663", "Exposure Average (filter03)"},
              {"8B6A8D61", "Bloom Source (filter05)"},
              {"AB528F6D", "Bloom Blur H (filter06)"},
              {"22B8B61A", "Bloom Blur V (filter07)"},
              {"109D919C", "DoF"},
              {"C7B1698D", "Deferred Composition"},
              {"940A71F7", "Deferred Directional Light"},
              {"B3FD87F5", "Motion Blur (camerablur)"},
              {"9020F444", "Motion Blur (modelshaderblur)"},
              {"33EC7BFF", "Camera Velocity (nomovevelocitymap)"},
              {"F4D20C7C", "Depth Pack (zconversion)"},
              {"0CD3C0A6", "UI (uibaseshader)"},
              {"23B645CF", "UI Overlay (uioverlayshader)"},
              {"16EBBF67", "UI Background (uifilter00shader)"},
           })
      {
         forced_shader_names.emplace(std::stoul(hash, nullptr, 16), name);
      }
#endif

      game = new Vanquish();
   }

   else if (ul_reason_for_call == DLL_PROCESS_DETACH)
   {
      Vanquish::SetEvents(false);
      FrameRateFixes::Uninstall();
   }

   CoreMain(hModule, ul_reason_for_call, lpReserved);

   return TRUE;
}
