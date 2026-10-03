// The Witcher Enhanced Edition — Luma mod (Aurora engine, 32-bit, DX9 -> D3D11 via dgVoodoo2).
//
// Hashes are the dgVoodoo-TRANSLATED ones (2.87.3), precomputed offline for every .bfx blob. No motion vectors or jitter of its own: DLAA/FSR's are
// built by patched shaders (see MotionVectorPatches.h), as in Mass Effect 2007 under the same wrapper.

// The MessageBox is invisible under a borderless/fullscreen game and blocks the loader -> ReShade error 1114.
#define DISABLE_AUTO_DEBUGGER 1

#define GAME_THE_WITCHER 1

#define ENABLE_NGX 0 // NGX is x64-only and the game is 32-bit: DLSS and FSR run in the SR bridge's x64 helper
#define ENABLE_FIDELITY_SK 0
#define GEOMETRY_SHADER_SUPPORT 0
// Outside DEVELOPMENT only this define makes original_draw_dispatch_func non-null (the motion vector draws redo the game's)
#define ENABLE_POST_DRAW_DISPATCH_CALLBACK 1
// The motion vector draw key reads the draw's arguments ("last_draw_dispatch_data")
#define ENABLE_DRAW_DISPATCH_DATA_CACHE 1
// The game ships no shader AA (MSAA only), so SMAA adds rather than replaces
#define ENABLE_SMAA 1
// Every game pixel shader gets the output clamp of its vanilla UNORM targets (see "OutputClamp.h")
#define LUMA_PATCH_BYTECODE_SYNC 1

#include "..\..\Core\core.hpp"
#include "..\..\External\WDK\includes\d3d11TokenizedProgramFormat.hpp"
#include "MotionVectorPatches.h"
#include "OutputClamp.h"
#include "VertexShaderRegisters.h"
#include "..\..\Core\includes\patched_draws.h"

// The scene's first full screen post passes (Lua "fullscreenfx.luc" effects; no material draws with them): the upscaler runs right
// before the first one. Hashes shared with materials (pass_fx 0x0BCB25C1, colorfill/zdepth 0xA1D2337D) and the mid-scene ones
// (shadow_blur_fx, shadows_fx) aren't here. dgVoodoo's present blit ends the frames without any (the UI is then in the scene), and
// a depth clear of the scene depth (before the UI, see "OnClearDepthStencilView") those without post passes.
static constexpr uint32_t kScenePostHashes[] = {
   0x61CDDC46, // brightpass_fx (ColorGlow)
   0x54F4B86D, // glow_fx
   0xD1FDE9E9, // whitebalance_fx
   0xCA9E344E, // depthoffield_fx
   0x4849C51A, // gauss_blur_fx: the depth of field blurs read the scene before depthoffield_fx (ColorGlow runs it after brightpass)
   0x50FD3FE1, // critical_fx
   0x6E9147B5, // critwounds_fx
   0xC01126F7, // combine_fx
   0x5FF3FE91, // colorsat_fx
   0x5F248BF2, // sharpen_fx
   0xFD2B943E, // desaturate_fx
   0x91918250, // bright_comb_fx
   0x6A9F9020, // brigness_comb_fx
   0xE1CDC3DD, // brightness_fx
   0x631B0782, // contrast_fx
   0x1A254A59, // darkvisionpt_fx
   0x3FC6FC68, // drunk_fx
   0x4A7EB438, // nightvision_fx
   0xAC41E55F, // nvisionsimp_fx
   0x98CC7D53, // radial_blur_fx
   0xE9F54A6E, // toxic_fx
   0x06D874F0, // underwater_fx
   0xD0F29A4D, // grayscale_fx
   0x6053C355, // inverse_fx
   0x8E401E4C, // blind_fx
   0x8B80BE48, // glowclrfiltr_fx
   0x43CE8048, // wobble_fx
   0x880A17D3, // dgVoodoo's present blit
};

#if DEVELOPMENT
static bool g_mv_enable = false;       // Motion vectors without an upscaler
static bool g_mv_debug_view = false;   // Core's debug draw of the motion vector target
static bool g_mv_force_jitter = false; // The projection jitter without an upscaler
#else
static constexpr bool g_mv_enable = false;
static constexpr bool g_mv_force_jitter = false;
#endif
static bool g_smaa_enable = true;
#if DEVELOPMENT
static bool g_smaa_predication = true;      // On geometry, from the scene depth's edges
static float g_smaa_pred_tolerance = 0.02f; // A fraction of view depth (ME1's)
static bool g_smaa_pred_debug = false;      // The predication mask instead of the antialiased scene
#else
static constexpr bool g_smaa_predication = true;
static constexpr float g_smaa_pred_tolerance = 0.02f;
#endif
// SMAA's resources go after this many presents without it (the upscaler antialiasing, SMAA off): ~5 s, so menus and loading
// screens between scenes don't recreate them each time
static constexpr uint32_t smaa_idle_release_frames = 300;

struct TheWitcherGameDeviceData final : public GameDeviceData
{
   // ---- DLAA / FSR Native AA (Mass Effect 2007's motion vector path, one per-draw buffer: dgVoodoo's vc4) ----
   // DLSS and FSR run in the x64 helper of Core's SR bridge (the game is 32-bit, see "SRBridge.h").
   // "IsSRActive", taken at present: Core's "Super Resolution" selection changes after it, mid frame for the draws
   bool sr_active = false;
   // The upscaler's output goes back into the scene without its alpha (additive blends leave values above 1 there, which the
   // post passes read), drawn from this view of it
   com_ptr<ID3D11BlendState> sr_rgb_blend_state;
   com_ptr<ID3D11ShaderResourceView> sr_output_srv;
   // None was picked ("CleanExtraSRResources", from the overlay): the upscaler's resources go at the next present
   std::atomic<bool> release_sr_resources = false;

   std::atomic<bool> mv_active = false; // Motion vectors and jitter this frame: an upscaler is active, or the DEV toggle (set at present)
   bool scene_active = false;           // The scene's start and end are tracked this frame: motion vectors, or SMAA (set at present)
   std::shared_mutex mv_mutex;
   // A game shader's patched version (null if refused) by its hash; a vertex shader's with the bytes of vc4 it reads
   // ("DXBC::ConstantBufferBytes": the previous frame's copy uploads only those) and its matrix registers (null if unknown)
   template <typename T>
   struct PatchedShader
   {
      com_ptr<T> shader;
      UINT read_size = 0;
      const VertexShaderRegisters::Entry* registers = nullptr;
   };
   std::unordered_map<uint32_t, PatchedShader<ID3D11VertexShader>> mv_vertex_shaders;
   std::unordered_map<uint32_t, PatchedShader<ID3D11PixelShader>> mv_pixel_shaders;
   // The motion vector target (sized like the scene; every blend state writes it unblended, see "OnCreateBlendState")
   com_ptr<ID3D11Texture2D> mv_texture;
   com_ptr<ID3D11RenderTargetView> mv_rtv;
   com_ptr<ID3D11UnorderedAccessView> mv_uav; // Null without typed UAV loads of its format (then no upscaler)
   // The upscaler's depth, a sharable copy of the scene depth written by the fill
   com_ptr<ID3D11Texture2D> mv_device_depth;
   com_ptr<ID3D11UnorderedAccessView> mv_device_depth_uav;
   // The scene depth (the depth view's resource), and the fill's view of it: of the resource itself if it's shader readable, else of
   // a copy of it taken when the scene ends
   com_ptr<ID3D11Resource> mv_depth;
   com_ptr<ID3D11Texture2D> mv_depth_copy;
   com_ptr<ID3D11ShaderResourceView> mv_depth_srv;
   com_ptr<ID3D11Resource> mv_depth_srv_source; // The scene depth "mv_depth_srv" was made for
   // A frame opens at its first mesh draw into output sized depth (the jitter is chosen there), starts at its first motion vector
   // draw (the target is cleared) and ends at its first post pass ("kScenePostHashes"), once per present.
   bool mv_scene_open = false;
   bool mv_scene_done = false;
   bool mv_frame_ended = true;
   float sr_vert_fov = 1.0471976f; // FSR's vertical FOV (radians): the last camera's, 60 degrees until one is seen
   // The fp16 scene the motion vector draws write (the canvas mirror), and the game's view of it (the upscaler's copy back)
   com_ptr<ID3D11Resource> mv_scene_color;
   com_ptr<ID3D11RenderTargetView> mv_scene_rtv;
   // The canvas copies the first post pass reads (dgVoodoo's StretchRect blits into the effect's textures, before it; depthoffield
   // reads two, measured 2026-09-30): the upscaler's (or SMAA's) output goes there too
   std::vector<com_ptr<ID3D11Resource>> mv_scene_copies;
   com_ptr<ID3D11Buffer> mv_fill_buffer;
   // The projection jitter (pixels, +y down), chosen when the scene opens; its NDC offset is at VS "MotionVectorPatches::jitter_slot"
   // of every mesh draw depth tested against the scene
   std::array<float, 2> mv_jitter = {};
   std::array<float, 2> mv_jitter_ndc = {}; // The same offset in NDC (y up), as the jitter buffer holds it
   com_ptr<ID3D11Buffer> mv_jitter_buffer;
   // Per-draw lookups kept for the next draw (reset when the scene opens): the jitter path's last depth view and whether it's the
   // scene depth, the last depth stencil state's depth test and write, the motion vector path's last accepted targets and the last
   // blend state's opacity, the last vertex and pixel shader's patched versions (owned by the maps above, never erased)
   ID3D11DepthStencilView* jitter_dsv = nullptr;
   bool jitter_dsv_scene = false;
   ID3D11DepthStencilState* depth_stencil_state = nullptr;
   bool depth_test = true;
   bool depth_write = true;
   ID3D11DepthStencilView* mv_accepted_dsv = nullptr;
   ID3D11BlendState* mv_blend_state = nullptr;
   bool mv_blend_opaque = true;
   bool mv_blend_alpha = false; // SRC_ALPHA / INV_SRC_ALPHA, see "DrawWithMotionVectors"
   uint32_t mv_last_vertex_shader_hash = 0;
   PatchedShader<ID3D11VertexShader> mv_last_vertex_shader;
   uint32_t mv_last_pixel_shader_hash = 0;
   ID3D11PixelShader* mv_last_pixel_shader = nullptr;
   PatchedDraws::BoundShader<ID3D11VertexShader> mv_bound_vertex_shader;
   PatchedDraws::BoundShader<ID3D11PixelShader> mv_bound_pixel_shader;

   // CPU copies of the vc4 buffers the motion vector draws bind, by buffer (an entry registers it, null until its first upload), from
   // a Map(WRITE_DISCARD) at its Unmap or an UpdateSubresource: a draw's constants are its buffer's latest copy
   using ConstantsCopy = std::shared_ptr<const std::vector<uint8_t>>;
   std::mutex mv_constants_mutex;
   std::unordered_map<uint64_t, ConstantsCopy> mv_constants_copies;
   std::unordered_map<uint64_t, void*> mv_mapped_constants; // Registered buffers mapped now, until their Unmap
   // Previous frame constants of the motion vector draws (see "PatchedDraws::PreviousConstants")
   PatchedDraws::PreviousConstants mv_previous_constants;
   // Motion vector draws by draw key (shaders, buffers, arguments), with a tie-break translation and vc4. A draw takes the previous
   // frame's vc4 of its key's nearest draw (same object, a frame earlier), its camera included.
   struct MotionVectorObject
   {
      PatchedDraws::ObjectTransform transform; // The world matrix's translation only (the rest zero)
      ConstantsCopy constants;
   };
   std::unordered_map<uint64_t, std::vector<MotionVectorObject>> mv_objects;
   std::unordered_map<uint64_t, std::vector<MotionVectorObject>> mv_previous_objects;
   // The frame's camera view projection (column vectors, from its first depth writing motion vector draw that has one, see
   // "GetViewProjection") and the previous frame's
   std::optional<Math::Matrix44D> mv_camera;
   std::optional<Math::Matrix44D> mv_previous_camera;
   uint32_t mv_frame_index = 0;              // The Luma frame index of the last motion vector frame
   std::vector<uint8_t> mv_camera_only_copy; // An unmatched draw's constants with last frame's camera (reused)

   // ---- SMAA (ME1's, at the scene end, see "DrawSMAAOnScene") ----
   // The scene size every resource below (and Core's DrawSMAA intermediates) was made for: a change releases them all
   uint32_t smaa_w = 0, smaa_h = 0;
   uint32_t smaa_idle_frames = 0; // Presents since SMAA last ran
   // Metrics (b1: 1/w, 1/h, w, h, predication scale) and the predication tolerance (b0 of the depth edges CS)
   com_ptr<ID3D11Buffer> smaa_metrics_buffer;
   com_ptr<ID3D11Buffer> smaa_predication_buffer;
   // A snapshot of the scene: the chain writes the scene, so it samples this copy
   com_ptr<ID3D11Texture2D> smaa_input;
   com_ptr<ID3D11ShaderResourceView> smaa_input_srv;
   // The predication mask (R16F edge-ness from the scene depth)
   com_ptr<ID3D11Texture2D> smaa_predication;
   com_ptr<ID3D11UnorderedAccessView> smaa_predication_uav;
   com_ptr<ID3D11ShaderResourceView> smaa_predication_srv;

   void ReleaseSMAAScratch()
   {
      smaa_input_srv.reset();
      smaa_input.reset();
      smaa_predication_srv.reset();
      smaa_predication_uav.reset();
      smaa_predication.reset();
   }

#if DEVELOPMENT
   // Per frame counts for the DEV panel (the last complete frame's shown)
   struct MotionVectorStats
   {
      uint32_t motion_vector_draws = 0, jitter_draws = 0, matched = 0, camera_only = 0, other_camera = 0, uncopied = 0, unknown_registers = 0, maps = 0, updates = 0, other_maps = 0, sr_draws = 0;
      uint32_t ended_by = 0; // The ending pass's PS hash (1: a depth clear)
      bool smaa = false, smaa_predication = false;
      float near_plane = 0.f, far_plane = 0.f; // The upscaler's, from the camera's projection (0: none found)
      bool camera = false, depth_copied = false;
      uint32_t scene_copies = 0;
      uint32_t rejected[8] = {};                                             // "DrawWithMotionVectors" refusals by reason ("MV_REJECT")
      uint32_t rejected_format = 0, rejected_width = 0, rejected_height = 0; // The last target refused by format or size
   };
   MotionVectorStats mv_stats, mv_last_stats;
   int mv_draw_reject = -1; // The current draw's "MV_REJECT" reason (-1 for none), for the MCP trace note
#endif
};

class TheWitcherGame final : public Game
{
   static TheWitcherGameDeviceData& GetGameDeviceData(DeviceData& device_data)
   {
      return *static_cast<TheWitcherGameDeviceData*>(device_data.game);
   }

   // An upscaler is picked and hasn't failed. Fixed for the whole frame (see "OnPresent"): a selection made after the motion vector
   // state was set would otherwise run the upscaler on mixed state.
   static bool IsSRActive(DeviceData& device_data)
   {
      return GetGameDeviceData(device_data).sr_active;
   }

   static const VertexShaderRegisters::Entry* FindVertexShaderRegisters(uint32_t hash)
   {
      const auto* const entry = std::ranges::lower_bound(VertexShaderRegisters::entries, hash, {}, &VertexShaderRegisters::Entry::hash);
      return entry != std::end(VertexShaderRegisters::entries) && entry->hash == hash ? entry : nullptr;
   }

   static constexpr uint8_t kNoRegister = 255;

   // A matrix at D3D9 register "reg" of vc4 (column vector rows; a 3 row world gets (0, 0, 0, 1)). False if the constants end first.
   static bool ReadMatrix(const std::vector<uint8_t>& constants, uint8_t reg, uint32_t rows, Math::Matrix44D* matrix)
   {
      const size_t offset = (size_t(reg) + MotionVectorPatches::object_row_offset) * 16;
      if (reg == kNoRegister || constants.size() < offset + rows * 16)
         return false;
      matrix->SetIdentity();
      const float* const values = reinterpret_cast<const float*>(constants.data() + offset);
      std::copy_n(values, rows * 4, matrix->GetData());
      return true;
   }

   // The draw's camera view projection (column vectors): world view projection * inverse(world), or projection * view (the
   // _default___ family multiplies them in the shader). False if the shader has neither pair.
   static bool GetViewProjection(const VertexShaderRegisters::Entry& registers, const std::vector<uint8_t>& constants, Math::Matrix44D* view_projection)
   {
      Math::Matrix44D first, second;
      if (ReadMatrix(constants, registers.world_view_projection, 4, &first) && ReadMatrix(constants, registers.world, 3, &second))
      {
         second.Invert();
         *view_projection = first * second;
         return true;
      }
      if (ReadMatrix(constants, registers.projection, 4, &first) && ReadMatrix(constants, registers.view, 4, &second))
      {
         *view_projection = first * second;
         return true;
      }
      return false;
   }

   // Writes a matrix (column vector rows) at D3D9 register "reg" of vc4
   static void WriteMatrix(const Math::Matrix44D& matrix, uint8_t reg, std::vector<uint8_t>* constants)
   {
      float* const values = reinterpret_cast<float*>(constants->data() + (size_t(reg) + MotionVectorPatches::object_row_offset) * 16);
      for (int i = 0; i < 16; i++)
         values[i] = float(matrix.GetData()[i]);
   }

   // Motion vectors: a registered vc4 buffer mapped for a whole rewrite, remembered until its Unmap
   static void OnMapBufferRegion(reshade::api::device* device, reshade::api::resource resource, uint64_t offset, uint64_t size, reshade::api::map_access access, void** data)
   {
      DeviceData* const device_data = device->get_private_data<DeviceData>();
      if (!device_data || !device_data->game || !GetGameDeviceData(*device_data).mv_active || !data || !*data)
         return;
      auto& gd = GetGameDeviceData(*device_data);
      // ponytail: a lock and a lookup on every Map of any buffer while motion vectors run; ME1's lock free filter of the registered
      // buffers if it shows in the frame time
      const std::lock_guard lock(gd.mv_constants_mutex);
      if (!gd.mv_constants_copies.contains(resource.handle))
         return;
      if (access == reshade::api::map_access::write_discard && offset == 0)
         gd.mv_mapped_constants[resource.handle] = *data;
#if DEVELOPMENT
      else
         gd.mv_stats.other_maps++; // A partial or appending write: the copies would miss it
#endif
   }

   // Motion vectors: the CPU copy of a vc4 buffer, before its Unmap (the game has written it). Reads the mapped memory back.
   static void OnUnmapBufferRegion(reshade::api::device* device, reshade::api::resource resource)
   {
      DeviceData* const device_data = device->get_private_data<DeviceData>();
      if (!device_data || !device_data->game || !GetGameDeviceData(*device_data).mv_active)
         return;
      auto& gd = GetGameDeviceData(*device_data);
      const std::lock_guard lock(gd.mv_constants_mutex);
      const auto mapped = gd.mv_mapped_constants.find(resource.handle);
      if (mapped == gd.mv_mapped_constants.end())
         return;
      D3D11_BUFFER_DESC desc;
      reinterpret_cast<ID3D11Buffer*>(resource.handle)->GetDesc(&desc);
      const auto* const bytes = static_cast<const uint8_t*>(mapped->second);
      gd.mv_constants_copies[resource.handle] = std::make_shared<std::vector<uint8_t>>(bytes, bytes + desc.ByteWidth);
      gd.mv_mapped_constants.erase(mapped);
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
      const std::lock_guard lock(gd.mv_constants_mutex);
      const auto copy = gd.mv_constants_copies.find(resource.handle);
      if (copy == gd.mv_constants_copies.end())
         return false;
      D3D11_BUFFER_DESC desc;
      reinterpret_cast<ID3D11Buffer*>(resource.handle)->GetDesc(&desc);
      if (offset >= desc.ByteWidth)
         return false;
      const size_t updated_size = size_t((std::min)(size, uint64_t(desc.ByteWidth) - offset));
      auto updated = copy->second && copy->second->size() == desc.ByteWidth ? std::make_shared<std::vector<uint8_t>>(*copy->second) : std::make_shared<std::vector<uint8_t>>(desc.ByteWidth);
      std::memcpy(updated->data() + offset, data, updated_size);
      copy->second = std::move(updated);
#if DEVELOPMENT
      gd.mv_stats.updates++;
#endif
      return false;
   }

   // The bound shader's motion vector version, patched from Core's bytecode copy on first use (null if it can't be, e.g. a vertex
   // shader without vc4)
   template <typename T>
   static TheWitcherGameDeviceData::PatchedShader<T> GetMotionVectorShader(ID3D11Device* native_device, DeviceData& device_data,
      std::unordered_map<uint32_t, TheWitcherGameDeviceData::PatchedShader<T>>* shaders, uint32_t hash, reshade::api::pipeline pipeline)
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
            }
            else
            {
               // With the output clamp (again, if Core's copy already has it: a clamp is idempotent)
               const std::vector<uint8_t> clamped = OutputClamp::ClampPixelShader(code, desc->code_size);
               patched = clamped.empty() ? MotionVectorPatch::PatchPixelShader(code, desc->code_size, MotionVectorPatches::layout, &error) : MotionVectorPatch::PatchPixelShader(clamped.data(), clamped.size(), MotionVectorPatches::layout, &error);
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
      const VertexShaderRegisters::Entry* registers = vertex ? FindVertexShaderRegisters(hash) : nullptr;
      // Failures in every build (bug reports), every patched shader only in development. A vertex shader without known matrix
      // registers still draws motion vectors (matched by key), without a camera or a tie-break of its own.
      if (DEVELOPMENT || !shader || (vertex && !registers))
         reshade::log::message(shader ? reshade::log::level::info : reshade::log::level::warning,
            std::format("[Witcher MV] {} 0x{:08X} {}{}", vertex ? "VS" : "PS", hash, shader ? "patched" : error, vertex && !registers ? " (matrix registers unknown)" : "").c_str());
      const std::unique_lock lock(gd.mv_mutex);
      return shaders->try_emplace(hash, TheWitcherGameDeviceData::PatchedShader<T>{shader, read_size, registers}).first->second;
   }

   // The bound vertex shader's patched version (null shader if refused), looked up again only when the game's changes
   static const TheWitcherGameDeviceData::PatchedShader<ID3D11VertexShader>& GetPatchedVertexShader(ID3D11Device* native_device, CommandListData& cmd_list_data, DeviceData& device_data, uint32_t hash)
   {
      auto& gd = GetGameDeviceData(device_data);
      if (hash != gd.mv_last_vertex_shader_hash)
      {
         gd.mv_last_vertex_shader = GetMotionVectorShader(native_device, device_data, &gd.mv_vertex_shaders, hash, cmd_list_data.pipeline_state_original_vertex_shader);
         gd.mv_last_vertex_shader_hash = hash;
      }
      return gd.mv_last_vertex_shader;
   }

   // The bound depth stencil state's depth test and write (cached in "depth_stencil_state")
   static void ClassifyBoundDepthState(ID3D11DeviceContext* native_device_context, TheWitcherGameDeviceData* gd)
   {
      com_ptr<ID3D11DepthStencilState> depth_stencil_state;
      native_device_context->OMGetDepthStencilState(&depth_stencil_state, nullptr);
      if (depth_stencil_state.get() == gd->depth_stencil_state)
         return;
      D3D11_DEPTH_STENCIL_DESC depth_desc = CD3D11_DEPTH_STENCIL_DESC(D3D11_DEFAULT);
      if (depth_stencil_state)
         depth_stencil_state->GetDesc(&depth_desc);
      gd->depth_test = depth_desc.DepthEnable;
      gd->depth_write = depth_desc.DepthEnable && depth_desc.DepthWriteMask == D3D11_DEPTH_WRITE_MASK_ALL;
      gd->depth_stencil_state = depth_stencil_state.get();
   }

   // Opens the scene at the frame's first mesh draw into output sized depth (the half resolution water reflection draws before it):
   // takes the scene depth and picks the jitter the whole scene draws with. Once per present (the HUD and later passes never reopen it).
   static void OpenScene(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, CommandListData& cmd_list_data, DeviceData& device_data, uint32_t vertex_shader_hash, ID3D11DepthStencilView* dsv)
   {
      auto& gd = GetGameDeviceData(device_data);
      uint4 depth_size;
      DXGI_FORMAT unused_format;
      GetResourceInfo(dsv, depth_size, unused_format);
      if (depth_size.x != device_data.output_resolution.x || depth_size.y != device_data.output_resolution.y || (gd.mv_active && !GetPatchedVertexShader(native_device, cmd_list_data, device_data, vertex_shader_hash).shader))
         return;
      gd.mv_scene_open = true;
      gd.mv_depth.reset();
      dsv->GetResource(&gd.mv_depth);
      gd.mv_scene_color.reset();
      gd.mv_scene_rtv.reset();
      gd.jitter_dsv = nullptr;
      gd.depth_stencil_state = nullptr;
      gd.depth_test = true;
      gd.depth_write = true;
      gd.mv_accepted_dsv = nullptr;
      gd.mv_blend_state = nullptr;
      gd.mv_blend_opaque = true;
      gd.mv_blend_alpha = false;
      // Halton (2, 3) over the upscaler's phase count; pixels to NDC (y up). None until the upscaler is ready (the bridge's helper
      // starting shows the scene as it is).
      const SR::InstanceData* sr_instance_data = IsSRActive(device_data) ? device_data.GetSRInstanceData() : nullptr;
      if (sr_instance_data && !sr_implementations[device_data.sr_type]->IsReady(sr_instance_data))
         sr_instance_data = nullptr;
      const unsigned int phase = cb_luma_global_settings.FrameIndex % (sr_instance_data ? (std::max)(sr_implementations[device_data.sr_type]->GetJitterPhases(sr_instance_data), 1) : SR::GetDefaultJitterPhases());
      gd.mv_jitter = (sr_instance_data || g_mv_force_jitter) ? std::array<float, 2>{SR::HaltonSequence(phase, 2), SR::HaltonSequence(phase, 3)} : std::array<float, 2>{};
      gd.mv_jitter_ndc = {gd.mv_jitter[0] * 2.f / device_data.output_resolution.x, gd.mv_jitter[1] * -2.f / device_data.output_resolution.y};
      const float ndc_jitter[4] = {gd.mv_jitter_ndc[0], gd.mv_jitter_ndc[1], 0.f, 0.f};
      if (!PatchedDraws::WriteDynamicConstants(native_device, native_device_context, std::addressof(gd.mv_jitter_buffer), ndc_jitter, sizeof(ndc_jitter)))
      {
         // No stale jitter on the scene draws either: no motion vectors this frame
         gd.mv_jitter = {};
         gd.mv_jitter_ndc = {};
         gd.mv_jitter_buffer.reset();
      }
   }

#if DEVELOPMENT
#define MV_REJECT(reason) \
   ([&](auto& gd) { gd.mv_stats.rejected[reason]++; gd.mv_draw_reject = int(reason); return false; }(GetGameDeviceData(device_data)))
#else
#define MV_REJECT(reason) false
#endif

   // Draws an opaque draw into the fp16 scene (the scene target alone, output sized, with the scene depth) with the patched shaders,
   // adding the motion vector target ("target_slot", past the game's) and the previous frame's vc4 ("previous_slots"). False if it
   // can't (the draw then goes to "DrawWithJitter").
   static bool DrawWithMotionVectors(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, CommandListData& cmd_list_data, DeviceData& device_data,
      const ShaderHashesList<OneShaderPerPipeline>& original_shader_hashes, const std::function<void()>& draw, const com_ptr<ID3D11RenderTargetView> (&rtvs)[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT], ID3D11DepthStencilView* dsv)
   {
      auto& gd = GetGameDeviceData(device_data);
      // The game's targets (the scene; with depth of field on, the "RenderWithDepth" techniques add the CoC at o1, measured 2026-09-30:
      // every material of a DOF scene), plus the motion vector target the last motion vector draw left bound
      for (UINT slot = MotionVectorPatches::target_slot; slot < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; slot++)
      {
         if (rtvs[slot] && (slot != MotionVectorPatches::target_slot || rtvs[slot] != gd.mv_rtv))
            return MV_REJECT(0);
      }
      if (!rtvs[0] || !dsv || !gd.mv_scene_open)
         return MV_REJECT(1);
      // Additive lights, decals and translucents keep the motion vectors of what's behind them, and so do colorless draws (stencil
      // shadow volumes). Aurora draws its opaque materials alpha blended (SRC_ALPHA / INV_SRC_ALPHA, alpha 1) with depth writes, the
      // translucents without: alpha blended draws that write depth count as opaque (measured 2026-09-30: 455 of 456 scene draws).
      com_ptr<ID3D11BlendState> blend_state;
      native_device_context->OMGetBlendState(&blend_state, nullptr, nullptr);
      if (blend_state.get() != gd.mv_blend_state)
      {
         D3D11_BLEND_DESC blend_desc = CD3D11_BLEND_DESC(D3D11_DEFAULT);
         if (blend_state)
            blend_state->GetDesc(&blend_desc);
         const D3D11_RENDER_TARGET_BLEND_DESC& rt0 = blend_desc.RenderTarget[0];
         gd.mv_blend_opaque = rt0.RenderTargetWriteMask != 0 && (!rt0.BlendEnable || (rt0.SrcBlend == D3D11_BLEND_ONE && rt0.DestBlend == D3D11_BLEND_ZERO && rt0.BlendOp == D3D11_BLEND_OP_ADD));
         gd.mv_blend_alpha = rt0.RenderTargetWriteMask != 0 && rt0.BlendEnable && rt0.SrcBlend == D3D11_BLEND_SRC_ALPHA && rt0.DestBlend == D3D11_BLEND_INV_SRC_ALPHA && rt0.BlendOp == D3D11_BLEND_OP_ADD;
         gd.mv_blend_state = blend_state.get();
      }
      ClassifyBoundDepthState(native_device_context, &gd);
      if (!gd.mv_blend_opaque && !(gd.mv_blend_alpha && gd.depth_write))
         return MV_REJECT(6);
      // Known targets: checked, and the motion vector target built for them
      if (rtvs[0] != gd.mv_scene_rtv || dsv != gd.mv_accepted_dsv)
      {
         com_ptr<ID3D11Resource> depth;
         dsv->GetResource(&depth);
         com_ptr<ID3D11Resource> color;
         rtvs[0]->GetResource(&color);
         if (depth != gd.mv_depth || !color || (gd.mv_scene_color && color != gd.mv_scene_color))
            return MV_REJECT(2);
         D3D11_RENDER_TARGET_VIEW_DESC rtv_desc;
         rtvs[0]->GetDesc(&rtv_desc);
         com_ptr<ID3D11Texture2D> color_texture;
         D3D11_TEXTURE2D_DESC color_desc = {};
         if (SUCCEEDED(color->QueryInterface(&color_texture)))
            color_texture->GetDesc(&color_desc);
#if DEVELOPMENT
         gd.mv_stats.rejected_format = rtv_desc.Format;
         gd.mv_stats.rejected_width = color_desc.Width;
         gd.mv_stats.rejected_height = color_desc.Height;
#endif
         // The canvas' fp16 mirror (the texture upgrades on). Filtered by the resource, not the view (dgVoodoo binds single-slice
         // array views).
         if (rtv_desc.Format != DXGI_FORMAT_R16G16B16A16_FLOAT || color_desc.ArraySize != 1 || color_desc.SampleDesc.Count != 1)
            return MV_REJECT(3);
         const uint2 size = {color_desc.Width, color_desc.Height};
         if (size.x != device_data.output_resolution.x || size.y != device_data.output_resolution.y)
            return MV_REJECT(4);
         const std::unique_lock lock(gd.mv_mutex);
         D3D11_TEXTURE2D_DESC desc = {};
         if (gd.mv_texture)
            gd.mv_texture->GetDesc(&desc);
         // R16G16_FLOAT: FSR keeps 16 bits internally; the error is under 0.1% of the motion (BL GOTY)
         constexpr DXGI_FORMAT format = DXGI_FORMAT_R16G16_FLOAT;
         if (desc.Width != size.x || desc.Height != size.y)
         {
            gd.mv_texture.reset();
            gd.mv_rtv.reset();
            gd.mv_uav.reset();
            gd.mv_device_depth.reset();
            gd.mv_device_depth_uav.reset();
            // The fill reads the target back through its UAV
            D3D11_FEATURE_DATA_FORMAT_SUPPORT2 support = {format};
            const bool typed_uav_load = SUCCEEDED(native_device->CheckFeatureSupport(D3D11_FEATURE_FORMAT_SUPPORT2, &support, sizeof(support))) && (support.OutFormatSupport2 & D3D11_FORMAT_SUPPORT2_UAV_TYPED_LOAD) != 0;
            desc = CD3D11_TEXTURE2D_DESC(format, size.x, size.y, 1, 1, D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | (typed_uav_load ? D3D11_BIND_UNORDERED_ACCESS : 0u));
            if (FAILED(SRBridge::CreateSharableTexture(native_device, desc, &gd.mv_texture)) || FAILED(native_device->CreateRenderTargetView(gd.mv_texture.get(), nullptr, &gd.mv_rtv)))
            {
               gd.mv_texture.reset();
               gd.mv_rtv.reset();
               return MV_REJECT(5);
            }
            if (typed_uav_load)
               native_device->CreateUnorderedAccessView(gd.mv_texture.get(), nullptr, &gd.mv_uav);
            const CD3D11_TEXTURE2D_DESC depth_desc(DXGI_FORMAT_R32_FLOAT, size.x, size.y, 1, 1, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS);
            if (SUCCEEDED(SRBridge::CreateSharableTexture(native_device, depth_desc, &gd.mv_device_depth)))
               native_device->CreateUnorderedAccessView(gd.mv_device_depth.get(), nullptr, &gd.mv_device_depth_uav);
            gd.mv_frame_ended = true;
         }
         gd.mv_scene_color = color;
         gd.mv_scene_rtv = rtvs[0];
         gd.mv_accepted_dsv = dsv;
      }

      const auto& vertex_shader = GetPatchedVertexShader(native_device, cmd_list_data, device_data, original_shader_hashes.vertex_shaders[0]);
      if (const uint32_t pixel_shader_hash = original_shader_hashes.pixel_shaders[0]; pixel_shader_hash != gd.mv_last_pixel_shader_hash)
      {
         gd.mv_last_pixel_shader = GetMotionVectorShader(native_device, device_data, &gd.mv_pixel_shaders, pixel_shader_hash, cmd_list_data.pipeline_state_original_pixel_shader).shader.get();
         gd.mv_last_pixel_shader_hash = pixel_shader_hash;
      }
      ID3D11PixelShader* const pixel_shader = gd.mv_last_pixel_shader;
      if (!vertex_shader.shader || !pixel_shader || !gd.mv_jitter_buffer)
         return MV_REJECT(7);
      if (std::exchange(gd.mv_frame_ended, false))
      {
         // The fill's marker: the largest float16 (a larger clear value is stored as it in R16G16_FLOAT)
         const FLOAT clear[4] = {65504.f, 65504.f, 0.f, 0.f};
         native_device_context->ClearRenderTargetView(gd.mv_rtv.get(), clear);
         // Last frame's camera and objects are the previous ones, unless frames without a scene (menus, videos) came between
         const bool previous_valid = gd.mv_camera && cb_luma_global_settings.FrameIndex - gd.mv_frame_index <= 1;
         gd.mv_previous_camera = previous_valid ? gd.mv_camera : std::nullopt;
         gd.mv_camera.reset();
         gd.mv_frame_index = cb_luma_global_settings.FrameIndex;
         // Swapped, not rebuilt: the lists keep their nodes and capacity (an empty list matches nothing); keys drawn in neither of the
         // last two frames go
         gd.mv_previous_objects.swap(gd.mv_objects);
         std::erase_if(gd.mv_objects, [](const auto& entry)
            { return entry.second.empty(); });
         for (auto& entry : gd.mv_objects)
            entry.second.clear();
         if (!previous_valid)
            gd.mv_previous_objects.clear();
      }

      // The game's vc4 (object, camera and bones in one). The slots added past it stay bound after the draw: no translated shader
      // reads a constant buffer past b4.
      com_ptr<ID3D11Buffer> current;
      native_device_context->VSGetConstantBuffers(MotionVectorPatches::object_slot, 1, &current);
      TheWitcherGameDeviceData::ConstantsCopy constants;
      {
         const std::lock_guard lock(gd.mv_constants_mutex);
         // The buffer's CPU copy (null until its first upload); the lookup registers it for a copy at every upload
         if (current)
            constants = gd.mv_constants_copies.try_emplace(reinterpret_cast<uint64_t>(current.get())).first->second;
      }
      // The previous frame's vc4: the same object's from last frame, else this draw's with last frame's camera (no object motion).
      // None (no CPU copy yet, another camera, unknown registers): the current one (zero motion).
      const std::vector<uint8_t>* upload = nullptr;
      if (constants)
      {
         const VertexShaderRegisters::Entry* const registers = vertex_shader.registers;
#if DEVELOPMENT
         gd.mv_stats.unknown_registers += registers == nullptr;
#endif
         // This draw's camera, the frame's if it's the first depth writing one (the sky draws without depth writes, maybe with its own
         // projection)
         Math::Matrix44D view_projection;
         const bool has_camera = registers && GetViewProjection(*registers, *constants, &view_projection);
         if (has_camera && !gd.mv_camera && gd.depth_write)
            gd.mv_camera = view_projection;

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
         for (const uint64_t value : {uint64_t(original_shader_hashes.vertex_shaders[0]), uint64_t(original_shader_hashes.pixel_shaders[0]), reinterpret_cast<uint64_t>(vertex_buffer.get()), uint64_t(vertex_offset),
                 reinterpret_cast<uint64_t>(index_buffer.get()), uint64_t(index_offset), uint64_t(draw_data.index_count), uint64_t(draw_data.first_index), uint64_t(uint32_t(draw_data.vertex_offset)),
                 uint64_t(draw_data.vertex_count), uint64_t(draw_data.first_vertex)})
            HashCombine(key, value);
         // The world matrix's translation (its rows' w) separates objects that share a key, as a tie-break only; else the world view
         // projection's (the camera's motion is small next to the objects' spacing)
         PatchedDraws::ObjectTransform transform = {};
         if (registers)
         {
            const uint8_t tie_break_register = registers->world != kNoRegister ? registers->world : registers->world_view_projection;
            const size_t offset = (size_t(tie_break_register) + MotionVectorPatches::object_row_offset) * 16;
            if (tie_break_register != kNoRegister && constants->size() >= offset + 3 * 16)
            {
               for (int row = 0; row < 3; row++)
                  std::memcpy(&transform[row], constants->data() + offset + row * 16 + 12, sizeof(float));
            }
         }

         // ponytail: linear search among the key's candidates (a handful at most); a spatial lookup if big crowds share a mesh
         const TheWitcherGameDeviceData::MotionVectorObject* match = nullptr;
         if (const auto previous = gd.mv_previous_objects.find(key); previous != gd.mv_previous_objects.end())
         {
            match = PatchedDraws::FindNearest(previous->second, transform, [&](const auto& candidate)
               { return candidate.constants->size() == constants->size(); });
         }
         if (match)
         {
            // Last frame's list outlives the draw ("mv_previous_objects" only changes at the next frame start)
            upload = &*match->constants;
#if DEVELOPMENT
            gd.mv_stats.matched++;
#endif
         }
         else if (has_camera && gd.mv_camera && gd.mv_previous_camera && [&]
                  {
                        // Drawn with the frame's camera (a relative tolerance: each draw's is derived from its own matrices)
                        double largest = 0.0, difference = 0.0;
                        for (int i = 0; i < 16; i++)
                        {
                           largest = (std::max)(largest, std::abs(gd.mv_camera->GetData()[i]));
                           difference = (std::max)(difference, std::abs(gd.mv_camera->GetData()[i] - view_projection.GetData()[i]));
                        }
                        return difference <= largest * 1e-4; }())
         {
            // Not found, drawn with the frame's camera: its own constants with last frame's camera (camera motion only), in the world
            // view projection (previous view projection * world), or in the view (inverse(projection) * previous view projection)
            gd.mv_camera_only_copy = *constants;
            Math::Matrix44D matrix;
            if (ReadMatrix(*constants, registers->world, 3, &matrix) && registers->world_view_projection != kNoRegister)
            {
               WriteMatrix(*gd.mv_previous_camera * matrix, registers->world_view_projection, &gd.mv_camera_only_copy);
            }
            else if (ReadMatrix(*constants, registers->projection, 4, &matrix))
            {
               matrix.Invert();
               WriteMatrix(matrix * *gd.mv_previous_camera, registers->view, &gd.mv_camera_only_copy);
            }
            upload = &gd.mv_camera_only_copy;
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
         gd.mv_objects[key].push_back({transform, constants});
      }
#if DEVELOPMENT
      else
      {
         gd.mv_stats.uncopied++;
      }
#endif
      ID3D11Buffer* const previous_current[] = {current.get()};
      gd.mv_previous_constants.Bind(native_device, native_device_context, MotionVectorPatches::previous_slots, {&upload, 1}, previous_current, "Witcher", {&vertex_shader.read_size, 1});
      ID3D11Buffer* const jitter = gd.mv_jitter_buffer.get();
      native_device_context->VSSetConstantBuffers(MotionVectorPatches::jitter_slot, 1, &jitter);
      // Left bound after the draw (set directly, bypassing Core's state tracking): the game's next draws either bind their own targets
      // and shaders, or are motion vector draws too
      if (rtvs[MotionVectorPatches::target_slot] != gd.mv_rtv)
      {
         ID3D11RenderTargetView* targets[MotionVectorPatches::target_slot + 1] = {};
         for (UINT slot = 0; slot < MotionVectorPatches::target_slot; slot++)
            targets[slot] = rtvs[slot].get();
         targets[MotionVectorPatches::target_slot] = gd.mv_rtv.get();
         native_device_context->OMSetRenderTargets(MotionVectorPatches::target_slot + 1, targets, dsv);
      }
      PatchedDraws::BindPatchedShader(native_device_context, vertex_shader.shader.get(), &gd.mv_bound_vertex_shader);
      PatchedDraws::BindPatchedShader(native_device_context, pixel_shader, &gd.mv_bound_pixel_shader);

      draw();
#if DEVELOPMENT
      gd.mv_stats.motion_vector_draws++;
#endif
      return true;
   }

   // Jitter for the scene's mesh draws without motion vectors (patched vertex shader, game pixel shader): stencil shadow volumes, the
   // lights' additive second wave, decals, translucents. Every draw depth tested against the scene takes the same jitter, or jittered
   // and unjittered depths of the same surface fail each other's test. False if it can't (the draw runs untouched).
   static bool DrawWithJitter(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, CommandListData& cmd_list_data, DeviceData& device_data,
      const ShaderHashesList<OneShaderPerPipeline>& original_shader_hashes, const std::function<void()>& draw, ID3D11DepthStencilView* dsv)
   {
      auto& gd = GetGameDeviceData(device_data);
      if (!gd.mv_scene_open || gd.mv_jitter == std::array<float, 2>{} || !gd.mv_jitter_buffer || !dsv)
         return false;
      // Meshes only (full screen passes have no vertex buffer or no depth test), into the scene depth (not the reflection's)
      if (dsv != gd.jitter_dsv)
      {
         com_ptr<ID3D11Resource> depth;
         dsv->GetResource(&depth);
         gd.jitter_dsv = dsv;
         gd.jitter_dsv_scene = depth == gd.mv_depth;
      }
      if (!gd.jitter_dsv_scene)
         return false;
      ClassifyBoundDepthState(native_device_context, &gd);
      if (!gd.depth_test)
         return false;
      com_ptr<ID3D11Buffer> vertex_buffer;
      UINT vertex_stride, vertex_offset;
      native_device_context->IAGetVertexBuffers(0, 1, &vertex_buffer, &vertex_stride, &vertex_offset);
      if (!vertex_buffer)
         return false;
      ID3D11VertexShader* const vertex_shader = GetPatchedVertexShader(native_device, cmd_list_data, device_data, original_shader_hashes.vertex_shaders[0]).shader.get();
      if (!vertex_shader)
         return false;
      // The patched vertex shader and the jitter stay bound after the draw (see "DrawWithMotionVectors"), with the game's pixel shader
      // (a motion vector draw's is put back)
      PatchedDraws::BindPatchedShader(native_device_context, vertex_shader, &gd.mv_bound_vertex_shader);
      ID3D11Buffer* const jitter = gd.mv_jitter_buffer.get();
      native_device_context->VSSetConstantBuffers(MotionVectorPatches::jitter_slot, 1, &jitter);
      PatchedDraws::RestoreGameShader(native_device_context, &gd.mv_bound_pixel_shader);
      draw();
#if DEVELOPMENT
      gd.mv_stats.jitter_draws++;
#endif
      return true;
   }

   // DLAA or FSR 3 Native AA on the jittered scene, its depth (the fill's copy) and the motion vectors; the result goes back into the
   // scene's color (and the copy the first post pass reads), its alpha kept. False if it didn't draw (missing input, or the upscaler
   // failed).
   static bool DrawUpscaler(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, DeviceData& device_data)
   {
      auto& gd = GetGameDeviceData(device_data);
      auto* const copy_vs = FindShader(device_data.native_vertex_shaders, CompileTimeStringHash("Copy VS"));
      auto* const copy_ps = FindShader(device_data.native_pixel_shaders, CompileTimeStringHash("Copy PS"));
      if (!gd.mv_texture || !gd.mv_device_depth || !gd.mv_scene_color || !gd.mv_scene_rtv || !copy_vs || !copy_ps)
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
         device_data.sr_output_color->GetDesc(&output_desc);
      if (output_desc.Width != scene_desc.Width || output_desc.Height != scene_desc.Height)
      {
         device_data.sr_output_color.reset();
         gd.sr_output_srv.reset();
         output_desc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R16G16B16A16_FLOAT, scene_desc.Width, scene_desc.Height, 1, 1, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS);
         if (SUCCEEDED(SRBridge::CreateSharableTexture(native_device, output_desc, &device_data.sr_output_color)))
            native_device->CreateShaderResourceView(device_data.sr_output_color.get(), nullptr, &gd.sr_output_srv);
      }
      if (!device_data.sr_output_color || !gd.sr_output_srv)
      {
         device_data.sr_suppressed = true; // Until the upscaler is picked again
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

      SR::SettingsData settings_data;
      settings_data.output_width = scene_desc.Width;
      settings_data.output_height = scene_desc.Height;
      settings_data.render_width = scene_desc.Width;
      settings_data.render_height = scene_desc.Height;
      settings_data.hdr = true;
      // The motion vectors are UV deltas, previous minus current
      settings_data.mvs_x_scale = float(scene_desc.Width);
      settings_data.mvs_y_scale = float(scene_desc.Height);
      settings_data.auto_exposure = false; // FSR's clips highlights (FSR-Best-Practices FIN-3); the scene is already exposed
      settings_data.render_preset = dlss_render_preset;
      sr_implementations[device_data.sr_type]->UpdateSettings(sr_instance_data, native_device_context, settings_data);

      SR::SuperResolutionImpl::DrawData draw_data;
      draw_data.source_color = scene.get();
      draw_data.output_color = device_data.sr_output_color.get();
      draw_data.motion_vectors = gd.mv_texture.get();
      draw_data.depth_buffer = gd.mv_device_depth.get();
      // As applied (pixels, +y down)
      draw_data.jitter_x = gd.mv_jitter[0];
      draw_data.jitter_y = gd.mv_jitter[1];
      draw_data.reset = device_data.force_reset_sr;
      // FSR needs the camera (column vectors). It errors on a 0 FOV: a frame without a camera keeps the last one.
      if (gd.mv_camera)
      {
         const SR::ViewProjectionCamera camera = SR::GetViewProjectionCamera(gd.mv_camera->GetData(), /* row_vectors */ false);
         if (camera.vert_fov > 0.0)
            gd.sr_vert_fov = float(camera.vert_fov);
         if (camera.near_plane > 0.0)
         {
            draw_data.near_plane = float(camera.near_plane);
            draw_data.far_plane = float(camera.far_plane);
         }
      }
      draw_data.vert_fov = gd.sr_vert_fov;
#if DEVELOPMENT
      gd.mv_stats.near_plane = draw_data.near_plane;
      gd.mv_stats.far_plane = draw_data.far_plane;
#endif
      if (!sr_implementations[device_data.sr_type]->Draw(sr_instance_data, native_device_context, draw_data))
      {
         device_data.sr_suppressed = true; // Until the upscaler is picked again
         return false;
      }
      DrawCustomPixelShader(native_device_context, device_data.default_depth_stencil_state.get(), gd.sr_rgb_blend_state.get(), nullptr, copy_vs, copy_ps, gd.sr_output_srv.get(), gd.mv_scene_rtv.get(), scene_desc.Width, scene_desc.Height);
      for (const auto& copy : gd.mv_scene_copies)
         native_device_context->CopyResource(copy.get(), scene.get());
      // Not while the bridge's helper starts (the color copied as it is): the next frame resets
      device_data.has_drawn_sr = sr_implementations[device_data.sr_type]->IsReady(sr_instance_data);
      return true;
   }

   // SMAA ULTRA on the scene (the post passes then see it antialiased, the UI draws after), predicated by the scene depth's edges
   // ("Luma_Witcher_DepthEdges.hlsl"). ME1's "RunPostFinalGradeSMAA" without RCAS. False if it didn't draw.
   static bool DrawSMAAOnScene(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, DeviceData& device_data)
   {
      auto& gd = GetGameDeviceData(device_data);
      uint4 size = {};
      DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
      GetResourceInfo(gd.mv_scene_color.get(), size, format);
      if (size.x == 0 || size.y == 0 || format == DXGI_FORMAT_UNKNOWN)
         return false;
      if (gd.smaa_w != size.x || gd.smaa_h != size.y)
      {
         gd.ReleaseSMAAScratch();
         gd.smaa_w = size.x;
         gd.smaa_h = size.y;
      }
      gd.smaa_idle_frames = 0;
      // Shader readiness (async loader, DEV live reload): skip SMAA this frame if one is missing
      for (const uint32_t name : {CompileTimeStringHash("SMAA Edge Detection PS"), CompileTimeStringHash("SMAA Blending Weight Calculation PS"), CompileTimeStringHash("SMAA Neighborhood Blending PS")})
      {
         if (!FindShader(device_data.native_pixel_shaders, name))
            return false;
      }
      for (const uint32_t name : {CompileTimeStringHash("SMAA Edge Detection VS"), CompileTimeStringHash("SMAA Blending Weight Calculation VS"), CompileTimeStringHash("SMAA Neighborhood Blending VS")})
      {
         if (!FindShader(device_data.native_vertex_shaders, name))
            return false;
      }

      if (!gd.smaa_input_srv)
      {
         gd.smaa_input.reset();
         const CD3D11_TEXTURE2D_DESC desc(format, size.x, size.y, 1, 1, D3D11_BIND_SHADER_RESOURCE);
         if (SUCCEEDED(native_device->CreateTexture2D(&desc, nullptr, &gd.smaa_input)))
            native_device->CreateShaderResourceView(gd.smaa_input.get(), nullptr, &gd.smaa_input_srv);
      }
      if (!gd.smaa_input_srv)
         return false;
      native_device_context->CopyResource(gd.smaa_input.get(), gd.mv_scene_color.get());

      // Scale and mask fall back together: 2 with a null mask raises the threshold frame wide. The CS maps texels 1:1.
      auto* const predication_shader = FindShader(device_data.native_compute_shaders, CompileTimeStringHash("Witcher Depth Edges CS"));
      bool predication = g_smaa_predication && predication_shader && gd.mv_depth_srv && GetViewTextureSize(gd.mv_depth_srv.get()) == uint2{size.x, size.y};
      if (predication && !gd.smaa_predication_srv)
      {
         const CD3D11_TEXTURE2D_DESC desc(DXGI_FORMAT_R16_FLOAT, size.x, size.y, 1, 1, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS);
         if (SUCCEEDED(native_device->CreateTexture2D(&desc, nullptr, &gd.smaa_predication)) && SUCCEEDED(native_device->CreateUnorderedAccessView(gd.smaa_predication.get(), nullptr, &gd.smaa_predication_uav)))
            native_device->CreateShaderResourceView(gd.smaa_predication.get(), nullptr, &gd.smaa_predication_srv);
      }
      const float tolerance[4] = {g_smaa_pred_tolerance, 0.f, 0.f, 0.f};
      predication = predication && gd.smaa_predication_srv && PatchedDraws::WriteDynamicConstants(native_device, native_device_context, std::addressof(gd.smaa_predication_buffer), tolerance, sizeof(tolerance));
      const float metrics[8] = {1.f / float(size.x), 1.f / float(size.y), float(size.x), float(size.y), predication ? 2.f : 1.f, 0.f, 0.f, 0.f};
      if (!PatchedDraws::WriteDynamicConstants(native_device, native_device_context, std::addressof(gd.smaa_metrics_buffer), metrics, sizeof(metrics)))
         return false;
#if DEVELOPMENT
      gd.mv_stats.smaa = true;
      gd.mv_stats.smaa_predication = predication;
#endif

      if (predication)
      {
         ID3D11ShaderResourceView* const srv = gd.mv_depth_srv.get();
         ID3D11UnorderedAccessView* const uav = gd.smaa_predication_uav.get();
         ID3D11Buffer* const buffer = gd.smaa_predication_buffer.get();
         native_device_context->CSSetShaderResources(0, 1, &srv);
         native_device_context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);
         native_device_context->CSSetConstantBuffers(0, 1, &buffer);
         native_device_context->CSSetShader(predication_shader, nullptr, 0);
         native_device_context->Dispatch((size.x + 7) / 8, (size.y + 7) / 8, 1);
         ID3D11ShaderResourceView* const null_srv = nullptr;
         ID3D11UnorderedAccessView* const null_uav = nullptr;
         native_device_context->CSSetShaderResources(0, 1, &null_srv);
         native_device_context->CSSetUnorderedAccessViews(0, 1, &null_uav, nullptr);
      }
#if DEVELOPMENT
      // Calibration aid: predication's effect is the absence of smearing, which the eye misjudges; judge the mask instead (red)
      if (predication && g_smaa_pred_debug)
      {
         auto* const copy_vs = FindShader(device_data.native_vertex_shaders, CompileTimeStringHash("Copy VS"));
         auto* const copy_ps = FindShader(device_data.native_pixel_shaders, CompileTimeStringHash("Copy PS"));
         if (copy_vs && copy_ps)
         {
            DrawCustomPixelShader(native_device_context, device_data.default_depth_stencil_state.get(), device_data.default_blend_state.get(), nullptr, copy_vs, copy_ps, gd.smaa_predication_srv.get(), gd.mv_scene_rtv.get(), size.x, size.y, false);
            return true;
         }
      }
#endif

      // Metrics at VS and PS b1 (DrawSMAA restores the shaders, views and targets, not the constant buffers)
      com_ptr<ID3D11Buffer> vs_buffer, ps_buffer;
      native_device_context->VSGetConstantBuffers(1, 1, &vs_buffer);
      native_device_context->PSGetConstantBuffers(1, 1, &ps_buffer);
      ID3D11Buffer* const metrics_buffer = gd.smaa_metrics_buffer.get();
      native_device_context->VSSetConstantBuffers(1, 1, &metrics_buffer);
      native_device_context->PSSetConstantBuffers(1, 1, &metrics_buffer);
      // Writing the scene is safe: the chain samples the snapshot, never the scene itself
      DrawSMAA(native_device, native_device_context, device_data, gd.mv_scene_rtv.get(), gd.smaa_input_srv.get(), gd.smaa_input_srv.get(), predication ? gd.smaa_predication_srv.get() : nullptr);
      ID3D11Buffer* const vs_restore = vs_buffer.get();
      ID3D11Buffer* const ps_restore = ps_buffer.get();
      native_device_context->VSSetConstantBuffers(1, 1, &vs_restore);
      native_device_context->PSSetConstantBuffers(1, 1, &ps_restore);
      return true;
   }

   // Ends the scene at its first post pass (or the depth clear before the UI), before that pass reads the scene: the depth copy and
   // camera motion fill (see "Luma_Witcher_MotionVectorFill.hlsl"), then the upscaler; SMAA if the upscaler didn't draw
   static void EndScene(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, DeviceData& device_data)
   {
      auto& gd = GetGameDeviceData(device_data);
      gd.mv_scene_open = false;
      gd.mv_scene_done = true;
      const bool motion_vectors = gd.mv_active && !gd.mv_frame_ended; // Else nothing to fill or upscale
      if ((!motion_vectors && !g_smaa_enable) || !gd.mv_scene_rtv)
         return;
      DrawStateStack<DrawStateStackType::FullGraphics> graphics_state;
      DrawStateStack<DrawStateStackType::Compute> compute_state;
      graphics_state.Cache(native_device_context, device_data.uav_max_count);
      compute_state.Cache(native_device_context, device_data.uav_max_count);
      // The scene depth may be bound (as the motion vectors are): unbound before it's read
      native_device_context->OMSetRenderTargets(0, nullptr, nullptr);

      // The fill's view of the scene depth: the depth itself if shader readable, else a copy of it
      com_ptr<ID3D11Texture2D> depth_texture;
      D3D11_TEXTURE2D_DESC depth_desc = {};
      if (gd.mv_depth && SUCCEEDED(gd.mv_depth->QueryInterface(&depth_texture)))
         depth_texture->GetDesc(&depth_desc);
      const bool readable = (depth_desc.BindFlags & D3D11_BIND_SHADER_RESOURCE) != 0;
      if (gd.mv_depth_srv_source != gd.mv_depth)
      {
         gd.mv_depth_srv.reset();
         gd.mv_depth_copy.reset();
         gd.mv_depth_srv_source = gd.mv_depth;
         if (depth_texture && depth_desc.SampleDesc.Count == 1 && (depth_desc.Format == DXGI_FORMAT_R24G8_TYPELESS || depth_desc.Format == DXGI_FORMAT_R32G8X24_TYPELESS || depth_desc.Format == DXGI_FORMAT_R32_TYPELESS))
         {
            if (!readable)
            {
               D3D11_TEXTURE2D_DESC copy_desc = depth_desc;
               copy_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
               copy_desc.MiscFlags = 0;
               copy_desc.CPUAccessFlags = 0;
               copy_desc.Usage = D3D11_USAGE_DEFAULT;
               native_device->CreateTexture2D(&copy_desc, nullptr, &gd.mv_depth_copy);
            }
            const DXGI_FORMAT srv_format = depth_desc.Format == DXGI_FORMAT_R24G8_TYPELESS ? DXGI_FORMAT_R24_UNORM_X8_TYPELESS : (depth_desc.Format == DXGI_FORMAT_R32G8X24_TYPELESS ? DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS : DXGI_FORMAT_R32_FLOAT);
            const CD3D11_SHADER_RESOURCE_VIEW_DESC srv_desc(D3D11_SRV_DIMENSION_TEXTURE2D, srv_format, 0, 1);
            ID3D11Resource* const srv_resource = readable ? static_cast<ID3D11Resource*>(depth_texture.get()) : gd.mv_depth_copy.get();
            if (srv_resource)
               native_device->CreateShaderResourceView(srv_resource, &srv_desc, &gd.mv_depth_srv);
         }
      }
      if (gd.mv_depth_copy && gd.mv_depth_srv)
         native_device_context->CopyResource(gd.mv_depth_copy.get(), gd.mv_depth.get());
#if DEVELOPMENT
      gd.mv_stats.depth_copied = gd.mv_depth_copy != nullptr;
      gd.mv_stats.camera = gd.mv_camera.has_value();
      gd.mv_stats.scene_copies = uint32_t(gd.mv_scene_copies.size());
#endif

      auto* const fill_shader = FindShader(device_data.native_compute_shaders, CompileTimeStringHash("Witcher Motion Vector Fill CS"));
      bool filled = false;
      if (motion_vectors && fill_shader && gd.mv_depth_srv && gd.mv_uav && gd.mv_device_depth_uav)
      {
         // Current clip space to the previous frame's: previous * inverse(current) (column vectors), in double (absolute world
         // translation). No camera: zero camera motion.
         Math::Matrix44D reprojection;
         reprojection.SetIdentity();
         if (gd.mv_camera && gd.mv_previous_camera)
         {
            Math::Matrix44D current = *gd.mv_camera;
            current.Invert();
            reprojection = *gd.mv_previous_camera * current;
         }
         float constants[20] = {}; // A multiple of 16 bytes
         for (int i = 0; i < 16; i++)
            constants[i] = float(reprojection.GetData()[i]);
         constants[16] = gd.mv_jitter_ndc[0];
         constants[17] = gd.mv_jitter_ndc[1];
         if (PatchedDraws::WriteDynamicConstants(native_device, native_device_context, std::addressof(gd.mv_fill_buffer), constants, sizeof(constants)))
         {
            ID3D11Buffer* const buffer = gd.mv_fill_buffer.get();
            ID3D11ShaderResourceView* const srv = gd.mv_depth_srv.get();
            ID3D11UnorderedAccessView* const uavs[2] = {gd.mv_uav.get(), gd.mv_device_depth_uav.get()};
            native_device_context->CSSetConstantBuffers(0, 1, &buffer);
            native_device_context->CSSetShaderResources(0, 1, &srv);
            native_device_context->CSSetUnorderedAccessViews(0, UINT(std::size(uavs)), uavs, nullptr);
            native_device_context->CSSetShader(fill_shader, nullptr, 0);
            native_device_context->Dispatch((uint32_t(device_data.output_resolution.x) + 7) / 8, (uint32_t(device_data.output_resolution.y) + 7) / 8, 1);
            ID3D11UnorderedAccessView* const null_uavs[std::size(uavs)] = {};
            ID3D11ShaderResourceView* const null_srv = nullptr;
            native_device_context->CSSetUnorderedAccessViews(0, UINT(std::size(null_uavs)), null_uavs, nullptr);
            native_device_context->CSSetShaderResources(0, 1, &null_srv);
            filled = true;
         }
      }
      // The upscaler's depth comes from the fill
      if (IsSRActive(device_data) && filled)
      {
         [[maybe_unused]] const bool drawn = DrawUpscaler(native_device, native_device_context, device_data);
#if DEVELOPMENT
         gd.mv_stats.sr_draws += drawn;
#endif
      }
      // Also while the bridge's helper starts (the upscaler then passes the color through)
      if (g_smaa_enable && !device_data.has_drawn_sr && DrawSMAAOnScene(native_device, native_device_context, device_data))
      {
         for (const auto& copy : gd.mv_scene_copies)
            native_device_context->CopyResource(copy.get(), gd.mv_scene_color.get());
      }
      compute_state.Restore(native_device_context);
      graphics_state.Restore(native_device_context);
   }

   // Frames without post passes: the scene ends at the depth clear before the UI (the 3D HUD medallion draws with the scene depth).
   // Stencil only clears (the shadow volumes' per light) don't end it.
   static bool OnClearDepthStencilView(reshade::api::command_list* cmd_list, reshade::api::resource_view dsv, const float* depth, const uint8_t* stencil, uint32_t rect_count, const reshade::api::rect* rects)
   {
      if (!depth)
         return false;
      reshade::api::device* const device = cmd_list->get_device();
      DeviceData* const device_data = device->get_private_data<DeviceData>();
      if (!device_data || !device_data->game)
         return false;
      auto& gd = GetGameDeviceData(*device_data);
      auto* const native_device_context = reinterpret_cast<ID3D11DeviceContext*>(cmd_list->get_native());
      if (!gd.scene_active || !gd.mv_scene_open || native_device_context->GetType() != D3D11_DEVICE_CONTEXT_IMMEDIATE)
         return false;
      com_ptr<ID3D11Resource> cleared;
      reinterpret_cast<ID3D11DepthStencilView*>(dsv.handle)->GetResource(&cleared);
      if (cleared != gd.mv_depth)
         return false;
#if DEVELOPMENT
      gd.mv_stats.ended_by = 1;
#endif
      gd.mv_scene_copies.clear();
      EndScene(reinterpret_cast<ID3D11Device*>(device->get_native()), native_device_context, *device_data);
      return false;
   }

public:
   // "OutputClamp::PatchOutputClamp" on every game pixel shader (Core then rebuilds the container around the returned program body)
   std::unique_ptr<std::byte[]> PatchShaderBytecodeSync(const std::byte* code, size_t& size, reshade::api::pipeline_subobject_type type, uint64_t shader_hash, const std::byte* shader_object, size_t shader_object_size) override
   {
      if (type != reshade::api::pipeline_subobject_type::pixel_shader || !shader_object)
         return nullptr;
      std::vector<DXBC::Chunk> chunks;
      if (!DXBC::ReadChunks(reinterpret_cast<const uint8_t*>(shader_object), shader_object_size, &chunks))
         return nullptr;
      DXBC::Chunk* const program = DXBC::FindChunk(&chunks, DXBC::FourCC("SHEX"), DXBC::FourCC("SHDR"));
      if (!program || !OutputClamp::PatchOutputClamp(program))
         return nullptr;
      // The program body, after its version and length tokens
      size = program->data.size() - 8;
      auto new_code = std::make_unique<std::byte[]>(size);
      std::memcpy(new_code.get(), program->data.data() + 8, size);
      return new_code;
   }

   void OnInit(bool async) override
   {
#if DEVELOPMENT
      // For the MCP "luma_dev_values" tool
      Mcp::RegisterToggles({{"mv_enable", &g_mv_enable}, {"mv_debug_view", &g_mv_debug_view}, {"mv_force_jitter", &g_mv_force_jitter}, {"smaa_enable", &g_smaa_enable}, {"smaa_predication", &g_smaa_predication}, { "smaa_pred_debug",
                               &g_smaa_pred_debug }});
      Mcp::RegisterValues({{"smaa_pred_tolerance", &g_smaa_pred_tolerance, 0.002f, 0.2f}});
      Mcp::RegisterTextures({MCP_GAME_TEXTURE("mv.velocity", mv_texture), MCP_GAME_TEXTURE("mv.depth", mv_device_depth)});
#endif

      // The canvas stays gamma encoded (1 = paper white) up to the present blit replacement, which does the HDR display map.
      GetShaderDefineData(POST_PROCESS_SPACE_TYPE_HASH).SetDefaultValue('0');
      GetShaderDefineData(EARLY_DISPLAY_ENCODING_HASH).SetDefaultValue('0');
      GetShaderDefineData(VANILLA_ENCODING_TYPE_HASH).SetDefaultValue('1'); // Gamma 2.2 in and out, as ME1/TW2 under the same wrapper
      GetShaderDefineData(GAMMA_CORRECTION_TYPE_HASH).SetDefaultValue('1');
      GetShaderDefineData(GAMUT_MAPPING_TYPE_HASH).SetDefaultValue('1');
      // The UI draws straight into the scene canvas and no pass marks where the scene ends, so the UI follows the game paper white.
      GetShaderDefineData(UI_DRAW_TYPE_HASH).SetDefaultValue('0');

      // The translated shaders bind b0-b4 only (every dgVoodoo 2.87.3 translation of the game's .bfx blobs,
      // offline census); b12/b13 are free for Luma.
      luma_settings_cbuffer_index = 13;
      luma_data_cbuffer_index = 12;
      luma_ui_cbuffer_index = -1;

      default_luma_global_game_settings.Dithering = 1.f;
      // Light AutoHDR on the Bink pass (Video_0x72C37F0F): movies bypass the scene. ME1's pair: at 0.5 the peak is 165 nits.
      default_luma_global_game_settings.VideoAutoHDREnable = 1.f;
      default_luma_global_game_settings.VideoAutoHDRBoost = 0.5f;
      cb_luma_global_settings.GameSettings = default_luma_global_game_settings;

      sr_game_tooltip = "Requires Luma-Upscaler.exe next to the game's exe.\n";

      // DLSS/FSR: its depth and the camera motion from the scene depth, the CPU copies of vc4 (dgVoodoo maps it or updates it), the
      // motion vector target written by every blend state, and the scene's end before the UI
      native_shaders_definitions.emplace(CompileTimeStringHash("Witcher Motion Vector Fill CS"), ShaderDefinition("Luma_Witcher_MotionVectorFill", reshade::api::pipeline_subobject_type::compute_shader));
      // SMAA: Core registers its 6 passes ("Luma_SMAA_impl.hlsl"); the predication mask from the scene depth
      native_shaders_definitions.emplace(CompileTimeStringHash("Witcher Depth Edges CS"), ShaderDefinition("Luma_Witcher_DepthEdges", reshade::api::pipeline_subobject_type::compute_shader));
      reshade::register_event<reshade::addon_event::map_buffer_region>(OnMapBufferRegion);
      reshade::register_event<reshade::addon_event::unmap_buffer_region>(OnUnmapBufferRegion);
      reshade::register_event<reshade::addon_event::update_buffer_region>(OnUpdateBufferRegion);
      reshade::register_event<reshade::addon_event::create_pipeline>(PatchedDraws::OnCreateBlendState<MotionVectorPatches::target_slot>);
      reshade::register_event<reshade::addon_event::clear_depth_stencil_view>(OnClearDepthStencilView);
   }

   static void UnregisterEvents()
   {
      reshade::unregister_event<reshade::addon_event::map_buffer_region>(OnMapBufferRegion);
      reshade::unregister_event<reshade::addon_event::unmap_buffer_region>(OnUnmapBufferRegion);
      reshade::unregister_event<reshade::addon_event::update_buffer_region>(OnUpdateBufferRegion);
      reshade::unregister_event<reshade::addon_event::create_pipeline>(PatchedDraws::OnCreateBlendState<MotionVectorPatches::target_slot>);
      reshade::unregister_event<reshade::addon_event::clear_depth_stencil_view>(OnClearDepthStencilView);
   }

   void OnCreateDevice(ID3D11Device* native_device, DeviceData& device_data) override
   {
      device_data.game = new TheWitcherGameDeviceData;
      device_data.taa_detected = true; // No TAA to replace, but Core's upscaler status checks for it
#if DEVELOPMENT
      // For the MCP "luma_dev_values" tool: the last complete frame's counts
      const auto& stats = GetGameDeviceData(device_data).mv_last_stats;
      Mcp::RegisterCounters({{"mv.draws", &stats.motion_vector_draws}, {"mv.jitter_draws", &stats.jitter_draws}, {"mv.matched", &stats.matched}, {"mv.camera_only", &stats.camera_only},
                               {"mv.other_camera", &stats.other_camera}, {"mv.uncopied", &stats.uncopied}, {"mv.unknown_registers", &stats.unknown_registers}, {"mv.maps", &stats.maps},
                               {"mv.updates", &stats.updates}, {"mv.other_maps", &stats.other_maps}, {"mv.sr_draws", &stats.sr_draws}, { "mv.ended_by_hash",
                                  &stats.ended_by }},
         &device_data);
      constexpr const char* reject_names[] = {"extra_target", "no_scene", "other_depth_color", "format", "size", "create", "blend", "shaders"};
      for (size_t i = 0; i < std::size(reject_names); i++)
         Mcp::RegisterCounter(std::string("mv.rejected.") + reject_names[i], &stats.rejected[i], &device_data);
#endif
   }

   // Core's default deletes through GameDeviceData*, which has no virtual destructor: delete the concrete type.
   void OnDestroyDeviceData(DeviceData& device_data) override
   {
#if DEVELOPMENT
      Mcp::Unregister(&device_data);
#endif
      delete static_cast<TheWitcherGameDeviceData*>(device_data.game);
      device_data.game = nullptr;
   }

   DrawOrDispatchOverrideType OnDrawOrDispatch(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, CommandListData& cmd_list_data, DeviceData& device_data, reshade::api::shader_stage stages, const ShaderHashesList<OneShaderPerPipeline>& original_shader_hashes, bool is_custom_pass, bool& updated_cbuffers, std::function<void()>* original_draw_dispatch_func) override
   {
      auto& gd = GetGameDeviceData(device_data);
      const bool is_immediate = native_device_context->GetType() == D3D11_DEVICE_CONTEXT_IMMEDIATE;

      // DLSS/FSR: the scene's draws with motion vectors or jitter (see "DrawWithMotionVectors"), and the upscaler at its first post pass
      if (gd.scene_active && is_immediate && !gd.mv_scene_done)
      {
         if (std::ranges::any_of(kScenePostHashes, [&](uint32_t hash)
                { return original_shader_hashes.Contains(hash, reshade::api::shader_stage::pixel); }))
         {
            if (gd.mv_scene_open)
            {
#if DEVELOPMENT
               gd.mv_stats.ended_by = uint32_t(original_shader_hashes.pixel_shaders[0]);
#endif
               // The canvas copies among the pass's textures (dgVoodoo's StretchRect blits copied the scene into the effect's own
               // textures): same size and format as the scene
               gd.mv_scene_copies.clear();
               com_ptr<ID3D11Texture2D> scene;
               D3D11_TEXTURE2D_DESC scene_desc = {};
               if (gd.mv_scene_color && SUCCEEDED(gd.mv_scene_color->QueryInterface(&scene)))
                  scene->GetDesc(&scene_desc);
               com_ptr<ID3D11ShaderResourceView> srvs[8];
               native_device_context->PSGetShaderResources(0, UINT(std::size(srvs)), &srvs[0]);
               for (const auto& srv : srvs)
               {
                  com_ptr<ID3D11Resource> resource;
                  com_ptr<ID3D11Texture2D> texture;
                  if (!scene || !srv)
                     continue;
                  srv->GetResource(&resource);
                  D3D11_TEXTURE2D_DESC desc;
                  if (resource == gd.mv_scene_color || FAILED(resource->QueryInterface(&texture)))
                     continue;
                  texture->GetDesc(&desc);
                  if (desc.Width == scene_desc.Width && desc.Height == scene_desc.Height && desc.Format == scene_desc.Format && desc.ArraySize == 1 && desc.SampleDesc.Count == 1)
                  {
                     gd.mv_scene_copies.push_back(resource);
                  }
               }
               EndScene(native_device, native_device_context, device_data);
            }
         }
         else if (!is_custom_pass && original_draw_dispatch_func && *original_draw_dispatch_func && (stages & reshade::api::shader_stage::vertex) == reshade::api::shader_stage::vertex)
         {
            com_ptr<ID3D11RenderTargetView> rtvs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
            com_ptr<ID3D11DepthStencilView> dsv;
            native_device_context->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, &rtvs[0], &dsv);
            if (!gd.mv_scene_open && dsv)
               OpenScene(native_device, native_device_context, cmd_list_data, device_data, original_shader_hashes.vertex_shaders[0], dsv.get());
            if (gd.mv_active)
            {
               const std::function<void()>& draw = *original_draw_dispatch_func;
               const bool motion_vectors = DrawWithMotionVectors(native_device, native_device_context, cmd_list_data, device_data, original_shader_hashes, draw, rtvs, dsv.get());
               const bool jitter = !motion_vectors && DrawWithJitter(native_device, native_device_context, cmd_list_data, device_data, original_shader_hashes, draw, dsv.get());
#if DEVELOPMENT
               Mcp::Annotate(cmd_list_data, motion_vectors ? "mv" : (jitter ? "jitter" : "unpatched"));
               if (const int reject = std::exchange(gd.mv_draw_reject, -1); reject >= 0)
                  Mcp::Annotate(cmd_list_data, "mv_reject", reject);
#endif
               if (motion_vectors || jitter)
                  return DrawOrDispatchOverrideType::Replaced;
            }
            else if (gd.mv_scene_open && !gd.mv_scene_rtv && rtvs[0] && dsv)
            {
               // SMAA alone: the scene target is the one drawn with the scene depth (the motion vector draws check theirs)
               com_ptr<ID3D11Resource> depth, color;
               dsv->GetResource(&depth);
               rtvs[0]->GetResource(&color);
               if (depth == gd.mv_depth && GetViewTextureSize(rtvs[0].get()) == uint2{uint32_t(device_data.output_resolution.x), uint32_t(device_data.output_resolution.y)})
               {
                  gd.mv_scene_color = color;
                  gd.mv_scene_rtv = rtvs[0];
               }
            }
         }
      }
      // Every draw without a patched shader: the game's own, if the last patched draw's are still bound
      if (is_immediate)
      {
         PatchedDraws::RestoreGameShader(native_device_context, &gd.mv_bound_vertex_shader);
         PatchedDraws::RestoreGameShader(native_device_context, &gd.mv_bound_pixel_shader);
      }
      return DrawOrDispatchOverrideType::None;
   }

   // Core's "force_borderless" acts only when the game asks for exclusive fullscreen; the game's windowed mode (registry
   // "FullScreen" 0) keeps its title bar, so the borders go at every swapchain too
   void OnInitSwapchain(reshade::api::swapchain* swapchain) override
   {
      if (game_window)
         CenterWindowAndRemoveBorders();
   }

   void CleanExtraSRResources(DeviceData& device_data) override
   {
      GetGameDeviceData(device_data).release_sr_resources = true;
   }

   void OnPresent(ID3D11Device* native_device, DeviceData& device_data) override
   {
      auto& gd = GetGameDeviceData(device_data);
      // DLSS/FSR: the history restarts after any frame it didn't draw (menus, loading, just picked); the selection and the motion
      // vector state are fixed here for the next frame (see "IsSRActive")
      gd.sr_active = LatchSRFrame(device_data);
      gd.mv_active = IsSRActive(device_data) || g_mv_enable;
      gd.scene_active = gd.mv_active || g_smaa_enable;
      if (gd.smaa_idle_frames++ == smaa_idle_release_frames)
      {
         gd.ReleaseSMAAScratch();
         ReleaseSMAA(device_data);
      }
      // None picked: Core stopped the SR bridge's helper ("ReleaseResources"), our upscaler inputs and output go too. Recreated when
      // an upscaler is picked again (the helper takes seconds to start).
      if (device_data.sr_type == SR::Type::None && gd.release_sr_resources.exchange(false))
      {
         gd.sr_output_srv.reset();
         if (!g_mv_enable)
         {
            const std::unique_lock lock(gd.mv_mutex);
            gd.mv_texture.reset();
            gd.mv_rtv.reset();
            gd.mv_uav.reset();
            gd.mv_device_depth.reset();
            gd.mv_device_depth_uav.reset();
            // The game's scene, depth and scene copy (taken again at the next scene), so a resize after None doesn't keep the old
            // ones alive; and the vc4 copies the object tables hold
            gd.mv_depth.reset();
            gd.mv_depth_copy.reset();
            gd.mv_depth_srv.reset();
            gd.mv_depth_srv_source.reset();
            gd.mv_scene_color.reset();
            gd.mv_scene_rtv.reset();
            gd.mv_scene_copies.clear();
            gd.mv_objects.clear();
            gd.mv_previous_objects.clear();
            gd.mv_camera.reset();
            gd.mv_previous_camera.reset();
         }
      }
      gd.mv_scene_open = false;
      gd.mv_scene_done = false;
      gd.mv_frame_ended = true;
      if (!custom_texture_mip_lod_bias_offset)
      {
         const std::unique_lock lock(s_mutex_samplers);
         // -1 at native resolution (Core biases the anisotropic samplers)
         device_data.texture_mip_lod_bias_offset = IsSRActive(device_data) ? SR::GetMipLODBias(device_data.output_resolution.y, device_data.output_resolution.y) : 0.f;
      }
#if DEVELOPMENT
      gd.mv_last_stats = std::exchange(gd.mv_stats, {});
      // The DEV panel's counts in ReShade.log every 300 frames while motion vectors run
      if (const auto& stats = gd.mv_last_stats; gd.mv_active && cb_luma_global_settings.FrameIndex % 300 == 0)
         reshade::log::message(reshade::log::level::info, std::format("[Witcher MV] frame {}: {} mv ({} matched, {} camera only, {} other camera, {} uncopied, {} unknown registers), {} jitter, {} maps, {} updates, {} other maps, sr {} ({}), camera {}, near {:.3f} far {:.0f}, ended by 0x{:08X} ({} copies), depth copied {}, refused {}/{}/{}/{}/{}/{}/{}/{} (last format {} {}x{})",
                                                             cb_luma_global_settings.FrameIndex, stats.motion_vector_draws, stats.matched, stats.camera_only, stats.other_camera, stats.uncopied, stats.unknown_registers, stats.jitter_draws, stats.maps, stats.updates, stats.other_maps, stats.sr_draws, int(device_data.sr_type),
                                                             stats.camera, stats.near_plane, stats.far_plane, stats.ended_by, stats.scene_copies, stats.depth_copied, stats.rejected[0], stats.rejected[1], stats.rejected[2], stats.rejected[3], stats.rejected[4], stats.rejected[5], stats.rejected[6], stats.rejected[7],
                                                             stats.rejected_format, stats.rejected_width, stats.rejected_height)
                                                             .c_str());
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
   }

   void LoadConfigs() override
   {
      reshade::get_config_value(nullptr, NAME, "SMAAEnable", g_smaa_enable);
      reshade::get_config_value(nullptr, NAME, "Dithering", cb_luma_global_settings.GameSettings.Dithering);
      reshade::get_config_value(nullptr, NAME, "VideoAutoHDREnable", cb_luma_global_settings.GameSettings.VideoAutoHDREnable);
      reshade::get_config_value(nullptr, NAME, "VideoAutoHDRBoost", cb_luma_global_settings.GameSettings.VideoAutoHDRBoost);
   }

   void DrawImGuiSettings(DeviceData& device_data) override
   {
      ImGui::SeparatorText("Anti-Aliasing");
      // The upscaler (Super Resolution, in the Settings tab) replaces SMAA: shown off, the saved choice is kept
      const bool sr_active = IsSRActive(device_data);
      ImGui::BeginDisabled(sr_active);
      bool smaa_shown = g_smaa_enable && !sr_active;
      if (ImGui::Checkbox("SMAA Enable", sr_active ? &smaa_shown : &g_smaa_enable))
         reshade::set_config_value(nullptr, NAME, "SMAAEnable", g_smaa_enable);
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
         ImGui::SetTooltip("Adds SMAA anti-aliasing (the game has none of its own; not used with DLSS/FSR).");
      ImGui::EndDisabled();
#if DEVELOPMENT
      if (g_smaa_enable && !sr_active)
      {
         // Not a preference: on geometry it relaxes the threshold back to base ULTRA, never below. A bisect switch.
         ImGui::Checkbox("SMAA Predication", &g_smaa_predication);
         if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Finds edges by geometry (scene depth) instead of by brightness alone.\nKeeps textures sharp while still antialiasing real silhouettes.");
         ImGui::SliderFloat("SMAA Predication Tolerance", &g_smaa_pred_tolerance, 0.002f, 0.2f, "%.3f", ImGuiSliderFlags_Logarithmic);
         if (ImGui::IsItemHovered())
            ImGui::SetTooltip("How far a surface may deviate from its local plane before it counts as an edge,\nas a fraction of view depth. Lower = more edges.");
         ImGui::Checkbox("SMAA Predication Debug View", &g_smaa_pred_debug);
         if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Show the predication mask (red) instead of the scene.\nWant: black on flat surfaces, red across silhouettes.");
      }
#endif

      auto& gs = cb_luma_global_settings.GameSettings;
      ImGui::SeparatorText("Effects");
      // Read in Video_0x72C37F0F.ps_5_0.hlsl. Inert in SDR: peak == paper white makes PumboAutoHDR an identity.
      bool video_auto_hdr = gs.VideoAutoHDREnable > 0.5f;
      if (ImGui::Checkbox("Video AutoHDR", &video_auto_hdr))
      {
         gs.VideoAutoHDREnable = video_auto_hdr ? 1.f : 0.f;
         reshade::set_config_value(nullptr, NAME, "VideoAutoHDREnable", gs.VideoAutoHDREnable);
         device_data.cb_luma_global_settings_dirty = true;
      }
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Adds HDR highlights to pre-rendered videos (HDR only).");

      ImGui::BeginDisabled(!video_auto_hdr);
      if (ImGui::SliderFloat("Video HDR Boost", &gs.VideoAutoHDRBoost, 0.f, 1.f))
      {
         reshade::set_config_value(nullptr, NAME, "VideoAutoHDRBoost", gs.VideoAutoHDRBoost);
         device_data.cb_luma_global_settings_dirty = true;
      }
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
         ImGui::SetTooltip("Video highlight strength (0 = off).");
      if (DrawResetButton(gs.VideoAutoHDRBoost, default_luma_global_game_settings.VideoAutoHDRBoost, "VideoAutoHDRBoost"))
         device_data.cb_luma_global_settings_dirty = true;
      ImGui::EndDisabled();

      bool dithering = gs.Dithering > 0.5f;
      if (ImGui::Checkbox("Dithering", &dithering))
      {
         gs.Dithering = dithering ? 1.f : 0.f;
         reshade::set_config_value(nullptr, NAME, "Dithering", gs.Dithering);
         device_data.cb_luma_global_settings_dirty = true;
      }
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Reduces gradient banding.");
   }

#if DEVELOPMENT
   void DrawImGuiDevSettings(DeviceData& device_data) override
   {
      ImGui::SeparatorText("Motion Vectors");
      ImGui::Checkbox("MV Enable (without an upscaler)", &g_mv_enable);
      ImGui::Checkbox("MV Force Jitter", &g_mv_force_jitter);
      ImGui::Checkbox("MV Debug View", &g_mv_debug_view);
      const auto& stats = GetGameDeviceData(device_data).mv_last_stats;
      ImGui::Text("MV draws %u (matched %u, camera only %u, other camera %u, uncopied %u, unknown registers %u), jitter draws %u", stats.motion_vector_draws, stats.matched, stats.camera_only, stats.other_camera, stats.uncopied, stats.unknown_registers, stats.jitter_draws);
      ImGui::Text("Ended by 0x%08X, camera %d, scene copies %u, depth copied %d, upscaler draws %u, SMAA %d (predication %d)", stats.ended_by, int(stats.camera), stats.scene_copies, int(stats.depth_copied), stats.sr_draws, int(stats.smaa), int(stats.smaa_predication));
   }
#endif

   void PrintImGuiAbout() override
   {
      ImGui::PushTextWrapPos(0.f);
      ImGui::Text(
         "Luma for \"The Witcher Enhanced Edition\" is developed by DristoforColumb and is open source and free.\n"
         "It runs through dgVoodoo2 (DirectX 9 -> 11).\n"
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
                  "\nAMD FidelityFX (FSR 3)"
                  "\nNVIDIA NGX (DLSS)"
                  "\ndgVoodoo2 by Dege (DirectX 9 -> 11 wrapper, required)");
   }
};

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
   if (ul_reason_for_call == DLL_PROCESS_ATTACH)
   {
      Globals::SetGlobals(PROJECT_NAME, "The Witcher Luma mod");

      // HDR research: the scheme is Mass Effect 2007's and TW2's under the same wrapper (DEV panel buttons turn each off again).
      swapchain_format_upgrade_type = TextureFormatUpgradesType::AllowedEnabled;
      swapchain_upgrade_type = SwapchainUpgradeType::scRGB;

      // Exclusive fullscreen -> borderless, also when leaving it (alt-tab cannot restore a title bar), as ME1/TW2
      prevent_fullscreen_state = true;
      force_borderless = true;

      // dgVoodoo forwards D3D9 SetGammaRamp (the game's gamma slider) to the OS device ramp, which distorts HDR output.
      allow_disabling_gamma_ramp = true;

      // Indirect (creation-time mirrors, substituted at bind): changing dgVoodoo's own creation formats breaks its bookkeeping (TW2).
      // The motion vector draws need the canvas mirror (fp16).
      texture_format_upgrades_type = TextureFormatUpgradesType::AllowedEnabled;
      enable_indirect_texture_format_upgrades = true;
      enable_chain_indirect_texture_format_upgrades = ChainTextureFormatUpgradesType::DirectDependencies;
      texture_upgrade_formats = {
         reshade::api::format::r8g8b8a8_typeless, // dgVoodoo's D3D9 backbuffer surface (the canvas) and the post effects' 4K copies
      };
      // "No1Px": dgVoodoo fills unused SRV slots with 1x1 placeholders, which pass the aspect filter.
      texture_format_upgrades_2d_size_filters = (uint32_t)TextureFormatUpgrades2DSizeFilters::SwapchainResolution | (uint32_t)TextureFormatUpgrades2DSizeFilters::SwapchainAspectRatio | (uint32_t)TextureFormatUpgrades2DSizeFilters::No1Px;

      game = new TheWitcherGame();
   }
   else if (ul_reason_for_call == DLL_PROCESS_DETACH)
   {
      TheWitcherGame::UnregisterEvents();
   }

   CoreMain(hModule, ul_reason_for_call, lpReserved);

   return TRUE;
}
