#define GAME_ASSASSINS_CREED_VALHALLA 1

#define DISABLE_AUTO_DEBUGGER 1

// The game loses and takes focus as it does without the mod
#define DISABLE_FOCUS_LOSS_SUPPRESSION 1

// The engine creates a D3D11 device and D3D9 objects next to its D3D12 device
#define CHECK_GRAPHICS_API_COMPATIBILITY 1

// TAA's jitter is only in a CBV of a descriptor table, and the exe stays unhooked
#define ENABLE_D3D12_DESCRIPTOR_TRACKING 1

#define ENABLE_RCAS 1

#define ENABLE_LUMA_TAA 1

#include "..\..\Core\core.hpp"

// Scimitar (Anvil), D3D12 only (DXBC SM 5.1, no reflection).
// Upgrades and replaces nothing. DLAA takes the place of the game's tiled TAA, at the render size of the game's own settings.

namespace
{
   // Every TAA pixel shader of temporalaa.hlsl, tiled or fullscreen. Only the tiled, direct, no upsample ones drew so far.
   // ponytail: the 392 CS variants aren't listed, they never drew: what picks them isn't known. If they do the game's TAA stays
   const ShaderHashesList shader_hashes_taa = {.pixel_shaders = {
                                                  0x00753740, 0x0190F90B, 0x06682C9D, 0x072F93C7, 0x095334CC, 0x0A969B7A, 0x0B2E9FBA, 0x0EA6A1CC, 0x0EEBB4D1, 0x0FDB7890, 0x11026D71, 0x13DB996D,
                                                  0x153A510B, 0x189B0873, 0x1B73934D, 0x1B946DE6, 0x1BEC1FCA, 0x1C31268D, 0x1D7289F5, 0x1E1E922E, 0x202003FB, 0x210666A8, 0x22EB1A3E, 0x2350B915,
                                                  0x243148F9, 0x2645FEC1, 0x27427425, 0x2A92F9DB, 0x2B125C81, 0x2DD7D524, 0x2FD75037, 0x3066DCC3, 0x32675728, 0x32FB5FC8, 0x33479110, 0x334CB872,
                                                  0x3357BA90, 0x3515B80E, 0x372C1ABE, 0x39FAC49C, 0x3B431681, 0x3C6A90C1, 0x3E8D184D, 0x3EA81C03, 0x42E0C0F5, 0x4607BAEA, 0x48A7E3F4, 0x4B3B6275,
                                                  0x4B99FE90, 0x4F65D654, 0x5844050C, 0x5A12392B, 0x5E32B2E6, 0x5E612648, 0x5EF0A2CF, 0x6160EFCF, 0x68D0E21E, 0x68D69136, 0x695F34EB, 0x6B3A4EC6,
                                                  0x6C56B2FB, 0x71C1542E, 0x72028C03, 0x7627F472, 0x7821D8BE, 0x7977189D, 0x7AD5F140, 0x7B6FF6F5, 0x7D4051A3, 0x803C6C74, 0x82391B0D, 0x83601518,
                                                  0x8511A7E2, 0x85DAF57B, 0x85F2A7E9, 0x88569468, 0x88D70924, 0x8933A9D6, 0x8F810811, 0x9048C19E, 0x908D9A4D, 0x91252B5D, 0x9264EF45, 0x949A0AA9,
                                                  0x94B88A41, 0x94C73223, 0x9585A8CE, 0x96164D20, 0x9E6971BD, 0x9E77407A, 0xA197DFD1, 0xA2340A22, 0xA23EA2CE, 0xA3B800BC, 0xA72D654C, 0xA9801B70,
                                                  0xAA864C86, 0xB0653082, 0xB06EA218, 0xB0FD6958, 0xB1DE17E4, 0xB457FA25, 0xB49420CC, 0xB6468E29, 0xB76000CB, 0xB8334A3C, 0xBAC3F17E, 0xBB773731,
                                                  0xBB78F8FD, 0xBC47D494, 0xBC729158, 0xBF63D277, 0xC018FE02, 0xC021EC95, 0xC09AD856, 0xC229CA32, 0xC383E8A1, 0xD047E0F8, 0xD144ACF6, 0xD5A7FB54,
                                                  0xD5C46116, 0xD722B0A1, 0xD8D4C38C, 0xDAD93682, 0xDC856C2B, 0xE0B13456, 0xE2AE915E, 0xE3FE7044, 0xE59B80F0, 0xE7FAFB94, 0xEBD83079, 0xF1833F50,
                                                  0xF1A36170, 0xF50080D2, 0xF530F54D, 0xF88B61F8, 0xFA998580, 0xFDCD0C03, 0xFE5AC77F, 0xFF973329}};
   // DitherBlendTiles, DitherBlendTilesCameraFade: right before TAA, the 2x2 blends TAA's dither tiles read instead of the raw dither of its t0
   const ShaderHashesList shader_hashes_dither_blend = {.pixel_shaders = {0x3F91ECEF, 0x83F6536B}};

   // What the game runs after TAA with its FSR 1: "x^4" (upscaleutils), EASU, RCAS. With its resolution scale alone none of them runs, the composite stretches TAA's output
   const ShaderHashesList shader_hashes_game_upscale = {.compute_shaders = {0x3464C344, 0x5BF6A0C0, 0x76E046D7, 0x80B48883, 0x8A62E5E7, 0x8EEB6AD8, 0xA09C5B53, 0xA4B11DF4, 0xC74DEBF0}};
   // The game's HDR composites of the scene and the UI into the swapchain, the main readers of the scene at the end of both paths: HDRSCRGB, and HDR / HDRGameDVR (HDR10 or a Game DVR
   // recording), the same mix left in PQ. Its other readers (the calibration and ForcedHDR composites, photo mode, CAS, portraits) get "encoded" scaled down into TAA's target
   constexpr uint32_t composite_hdr10 = 0x11856D93;
   const ShaderHashesList shader_hashes_composite_hdr = {.pixel_shaders = {0xEF757BEB, composite_hdr10}};

   float g_rcas_sharpness = 0.f; // RCAS on the upscaler's output, 0 = off

#if DEVELOPMENT
   // A/B: the upscalers on the PQ decoded scene with their HDR mode, as opposed to the PQ signal the game's TAA reads
   bool g_linear_upscaler_input = true;
#else
   constexpr bool g_linear_upscaler_input = true;
#endif

   constexpr reshade::api::format dlaa_color_format = reshade::api::format::r16g16b16a16_float;
   constexpr reshade::api::resource_usage render_target_and_shader_resource = reshade::api::resource_usage::render_target | reshade::api::resource_usage::shader_resource;
} // namespace

struct AssassinsCreedValhallaDeviceData final : public GameDeviceData
{
   // The pipelines drawn on the game's root signature (see "GetGameRootPipeline()"), which it doesn't own. Its mutex guards the textures too
   DeviceData::GamePassD3D12 game_root_pass;
   DeviceData::GamePassD3D12 color_pass;
   DeviceData::GamePassD3D12 composite_ui_pass; // On the composite's root signature. Its mutex guards "composite_ui"
   DeviceData::GamePassD3D12 composite_pass;

   // In a shader resource state between draws, the output in unordered access
   reshade::api::resource color = {};
   reshade::api::resource motion_vectors = {};
   reshade::api::resource depth = {};
   reshade::api::resource reactive = {}; // FSR's
   reshade::api::resource output = {};
   reshade::api::resource sharpened = {};    // RCAS on "output"
   reshade::api::resource encoded = {};      // Upscaling: "output" in the game's encoding, the scene of "composite_ps"
   reshade::api::resource composite_ui = {}; // "composite_ui_ps"
   bool output_recreated = false;            // Until a DLSS draw into it succeeds

   std::atomic<bool> dlaa_active = false; // Latched at present
   // Of the frame being recorded, reset at its dither blend: a present can fall between that and TAA, the game records ahead of it
   std::atomic<bool> dlaa_claimed = false;
   std::atomic<bool> dlaa_drawn = false;
   // The upscaler wrote the output size, until a frame's TAA runs: stays across frames, as the list with the composite and the game's upscaling can be recorded before TAA's
   std::atomic<bool> upscaled = false;
   uint32_t presents_without_upscaler = 0; // Of the present thread
   // The target of this frame's dither blend, S everywhere since "scene_copy_ps" (Luma_ACV_DLAA.hlsl)
   std::atomic<bool> scene_ready = false;
   std::atomic<uint64_t> scene_resource = 0;
   std::atomic<reshade::api::format> scene_format = reshade::api::format::unknown;
};

#if DEVELOPMENT
// A texture of ours for "Mcp::RegisterTextures()", in the state it's left in between draws
template <reshade::api::resource AssassinsCreedValhallaDeviceData::* member, D3D12_RESOURCE_STATES resource_state>
Mcp::DevTexture GetDevTexture(DeviceData& device_data)
{
   if (!device_data.game)
   {
      return {};
   }
   auto& game_device_data = static_cast<AssassinsCreedValhallaDeviceData&>(*device_data.game);
   return {reinterpret_cast<ID3D12Resource*>((game_device_data.*member).handle), resource_state};
}
#endif

// The exe's code, in memory only: "write" runs with its pages writable
template <typename Write>
bool WriteExeCode(void* at, size_t size, Write&& write)
{
   DWORD protection = 0;
   if (!VirtualProtect(at, size, PAGE_EXECUTE_READWRITE, &protection))
   {
      return false;
   }
   write();
   VirtualProtect(at, size, protection, &protection);
   FlushInstructionCache(GetCurrentProcess(), at, size);
   return true;
}

// The game's Halton jitter: the getter switches over a mode copied from the renderer every frame, 2 = 8 phases, 3 = 16, 4 = 32. The upscalers want more below native
// (DLSS-Best-Practices UPS-3), and the mode doesn't stay written, so mode 2's entry of the switch's jump table is pointed at the case wanted. In memory only, on this build of the exe:
// another one fails the checks and stays at 8, as does an entry something else changed. On the present thread
namespace JitterPhases
{
   constexpr uintptr_t switch_rva = 0xAA1FB5; // movsxd rcx, [rax+14h]; cmp ecx, 0Bh; ja
   constexpr uint8_t switch_code[] = {0x48, 0x63, 0x48, 0x14, 0x83, 0xF9, 0x0B, 0x0F, 0x87, 0xA8, 0x00, 0x00, 0x00};
   constexpr uintptr_t jump_table_rva = 0xAA208C;
   constexpr uint32_t phase_cases[] = {0xAA1FEB, 0xAA1FF7, 0xAA2003}; // Entries 2, 3 and 4

   uint32_t* entry = nullptr;
   uint32_t written = 0;
   bool checked = false;

   void Apply(uint32_t wanted_phases)
   {
      if (!checked)
      {
         checked = true;
         auto* const exe = reinterpret_cast<uint8_t*>(GetModuleHandleW(nullptr));
         auto* const table = reinterpret_cast<uint32_t*>(exe + jump_table_rva);
         if (std::memcmp(exe + switch_rva, switch_code, sizeof(switch_code)) == 0 && std::memcmp(table + 2, phase_cases, sizeof(phase_cases)) == 0)
         {
            entry = table + 2;
            written = phase_cases[0];
         }
         else
         {
            reshade::log::message(reshade::log::level::warning, "[Jitter] Unknown build of the exe, the jitter stays at 8 phases");
         }
      }
      if (!entry)
      {
         return;
      }
      if (*entry != written)
      {
         reshade::log::message(reshade::log::level::warning, "[Jitter] Something else changed the jump table, it's left alone");
         entry = nullptr;
         return;
      }
      const uint32_t wanted = phase_cases[wanted_phases > 16 ? 2 : (wanted_phases > 8 ? 1 : 0)];
      if (wanted == written)
      {
         return;
      }
      if (!WriteExeCode(entry, sizeof(*entry), [&]()
             { InterlockedExchange(reinterpret_cast<volatile LONG*>(entry), LONG(wanted)); }))
      {
         reshade::log::message(reshade::log::level::warning, "[Jitter] The jump table can't be written, the jitter stays as it is");
         entry = nullptr;
         return;
      }
      written = wanted;
   }
} // namespace JitterPhases

// The game's Anti-Aliasing Low and Medium also lower the render area (0.7, 0.85) for its TAA to upsample: in TAA's place an upscaler only gets fewer pixels.
// "UpdateRatio" (0x1408BB950) copies the TAA options' area ([rdi+38h]) into the view's ([rbx+40h]) when [rdi+0A7h] is set. While an upscaler draws, the
// branch of that check goes to a stub in the int3 padding after the function, which writes 1 instead. The branch is 2 aligned bytes written in one store,
// so a thread running the function sees either version. The game's FSR area is chosen after it and still applies. In memory only, on this build of the exe
namespace FullRenderArea
{
   // cmp byte ptr [rdi+0A7h], 0; je +6; mov eax, [rdi+38h]; mov [rbx+40h], eax
   constexpr uintptr_t check_rva = 0x8BB9CF;
   constexpr uint8_t check_code[] = {0x80, 0xBF, 0xA7, 0x00, 0x00, 0x00, 0x00, 0x74, 0x06, 0x8B, 0x47, 0x38, 0x89, 0x43, 0x40};
   constexpr uintptr_t branch_rva = check_rva + 7;
   constexpr uintptr_t stub_rva = 0x8BBA52; // After "pop rdi; ret", 14 int3
   // je 0x1408BB9DE (unset: the area stays); mov dword ptr [rbx+40h], 1.0; jmp 0x1408BB9DE
   constexpr uint8_t stub_code[] = {0x74, 0x8A, 0xC7, 0x43, 0x40, 0x00, 0x00, 0x80, 0x3F, 0xEB, 0x81};
   constexpr uint16_t original_branch = 0x0674; // je +6
   constexpr uint16_t stub_branch = 0x7AEB;     // jmp to the stub

   uint16_t* branch = nullptr;
   bool checked = false;

   void Apply(bool full_area)
   {
      if (!checked)
      {
         checked = true;
         auto* const exe = reinterpret_cast<uint8_t*>(GetModuleHandleW(nullptr));
         uint8_t* const stub = exe + stub_rva;
         const bool padding = stub[-1] == 0xC3 && std::all_of(stub, stub + sizeof(stub_code), [](uint8_t byte)
                                                     { return byte == 0xCC; });
         if (std::memcmp(exe + check_rva, check_code, sizeof(check_code)) != 0 || !padding)
         {
            reshade::log::message(reshade::log::level::warning, "[Render Area] Unknown build of the exe, Anti-Aliasing Low and Medium keep lowering the render size");
            return;
         }
         if (!WriteExeCode(stub, sizeof(stub_code), [&]()
                { std::memcpy(stub, stub_code, sizeof(stub_code)); }))
         {
            reshade::log::message(reshade::log::level::warning, "[Render Area] The exe can't be written, Anti-Aliasing Low and Medium keep lowering the render size");
            return;
         }
         branch = reinterpret_cast<uint16_t*>(exe + branch_rva);
      }
      if (!branch)
      {
         return;
      }
      const uint16_t wanted = full_area ? stub_branch : original_branch;
      if (*branch == wanted)
      {
         return;
      }
      if (*branch != (full_area ? original_branch : stub_branch))
      {
         reshade::log::message(reshade::log::level::warning, "[Render Area] Something else changed the exe there, it's left alone");
         branch = nullptr;
         return;
      }
      if (!WriteExeCode(branch, sizeof(*branch), [&]()
             { InterlockedExchange16(reinterpret_cast<volatile SHORT*>(branch), SHORT(wanted)); }))
      {
         reshade::log::message(reshade::log::level::warning, "[Render Area] The exe can't be written, Anti-Aliasing Low and Medium keep lowering the render size");
         branch = nullptr;
      }
   }
} // namespace FullRenderArea

class AssassinsCreedValhalla final : public Game
{
   // A pipeline of the native "pixel_shader_hash" into "format" on the root signature the game has bound. With "pass->mutex" held
   static ID3D12PipelineState* GetGameRootPipeline(DeviceData::GamePassD3D12* pass, DeviceData& device_data, reshade::api::device* device, const D3D12State::CommandListState& state, uint32_t pixel_shader_hash, DXGI_FORMAT format, const char* name)
   {
      const uint64_t present_count = device_data.present_count.load(std::memory_order_relaxed);
      const uint32_t natives_generation = device_data.native_shaders_generation.load(std::memory_order_acquire);
      if (pass->layout != state.graphics.layout || pass->pipelines_generation != natives_generation)
      {
         for (auto& [key, pipeline] : pass->pipelines)
         {
            pass->retired.emplace_back(present_count, std::move(pipeline));
         }
         pass->pipelines.clear();
         pass->layout = state.graphics.layout;
         pass->pipelines_generation = natives_generation;
      }
      DestroyRetiredD3D12GamePassResources(pass, device, present_count);
      return GetD3D12GamePassPipeline(pass, reinterpret_cast<ID3D12Device*>(device->get_native()), CompileTimeStringHash("Copy VS"), pixel_shader_hash, format, name);
   }

   // A fullscreen draw of "pixel_shader_hash" into "target" that keeps the game's root signature and arguments, so the shader reads what the game's draw would.
   // "target" must be a render target. The game's graphics state comes back after it
   static bool DrawGameRootPass(CommandListData& cmd_list_data, DeviceData& device_data, DeviceData::GamePassD3D12* pass, uint32_t pixel_shader_hash, reshade::api::resource target, reshade::api::format format, const char* name)
   {
      reshade::api::command_list* const cmd_list = cmd_list_data.cmd_list;
      reshade::api::device* const device = cmd_list->get_device();
      ID3D12PipelineState* const pipeline = GetGameRootPipeline(pass, device_data, device, cmd_list_data.d3d12_state, pixel_shader_hash, DXGI_FORMAT(format), name);
      reshade::api::resource_view target_view = {};
      if (!pipeline || !device->create_resource_view(target, reshade::api::resource_usage::render_target, reshade::api::resource_view_desc(reshade::api::resource_view_type::texture_2d, format, 0, 1, 0, 1), &target_view))
      {
         return false;
      }
      const reshade::api::resource_desc desc = device->get_resource_desc(target);
      cmd_list->bind_pipeline(reshade::api::pipeline_stage::all_graphics, reshade::api::pipeline{reinterpret_cast<uint64_t>(pipeline)});
      cmd_list->bind_render_targets_and_depth_stencil(1, &target_view);
      device->destroy_resource_view(target_view);
      const reshade::api::viewport viewport = {0.f, 0.f, float(desc.texture.width), float(desc.texture.height), 0.f, 1.f};
      const reshade::api::rect scissor = {0, 0, int32_t(desc.texture.width), int32_t(desc.texture.height)};
      cmd_list->bind_viewports(0, 1, &viewport);
      cmd_list->bind_scissor_rects(0, 1, &scissor);
      const reshade::api::dynamic_state topology_state = reshade::api::dynamic_state::primitive_topology;
      const uint32_t topology = uint32_t(reshade::api::primitive_topology::triangle_strip);
      cmd_list->bind_pipeline_states(1, &topology_state, &topology);
      cmd_list->draw(4, 1, 0, 0);
      TraceLumaPass(&cmd_list_data, device_data, name, {}, {{.resource = target.handle, .format = format}});
      RestoreGameGraphicsD3D12(cmd_list_data);
      return true;
   }

   struct GameTableSlot
   {
      reshade::api::descriptor_table table = {};
      uint32_t binding = 0;
   };

   // Where a descriptor table bound to "root" holds a register of the game's root signature
   static std::optional<GameTableSlot> FindGameTableSlot(DeviceData& device_data, const D3D12State::RootState& root, reshade::api::descriptor_type type, uint32_t register_space, uint32_t register_index)
   {
      const std::shared_lock lock(device_data.pipeline_layouts.mutex);
      const auto parameters = device_data.pipeline_layouts.parameters_by_layout.find(root.layout.handle);
      if (parameters == device_data.pipeline_layouts.parameters_by_layout.end())
      {
         return std::nullopt;
      }
      for (size_t index = 0; index < parameters->second.size() && index < root.bindings.size(); index++)
      {
         for (const reshade::api::descriptor_range& range : parameters->second[index].ranges)
         {
            const uint32_t register_offset = register_index - range.dx_register_index;
            if (range.type == type && range.dx_register_space == register_space && register_index >= range.dx_register_index && register_offset < range.count && root.bindings[index].table.handle != 0)
            {
               return GameTableSlot{.table = root.bindings[index].table, .binding = range.binding + register_offset};
            }
         }
      }
      return std::nullopt;
   }

   // Reads the b0 CBV of "register_space" the game's draw has bound, in an upload buffer the game wrote before recording the draw
   static bool ReadGameConstants(DeviceData& device_data, const D3D12State::CommandListState& state, uint32_t register_space, size_t offset, void* out, size_t size)
   {
      const std::optional<GameTableSlot> slot = FindGameTableSlot(device_data, state.graphics, reshade::api::descriptor_type::constant_buffer, register_space, 0);
      const D3D12State::Descriptor* const descriptor = slot ? D3D12State::FindDescriptor(device_data.descriptor_heaps, device_data.reshade_device, slot->table, slot->binding) : nullptr;
      if (!descriptor || descriptor->type != reshade::api::descriptor_type::constant_buffer)
      {
         return false;
      }
      const size_t begin = size_t(descriptor->offset) + offset;
      auto* const buffer = reinterpret_cast<ID3D12Resource*>(descriptor->handle);
      const D3D12_RANGE read_range = {begin, begin + size};
      const D3D12_RANGE nothing_written = {};
      void* data = nullptr;
      if (!buffer || FAILED(buffer->Map(0, &read_range, &data)))
      {
         return false;
      }
      std::memcpy(out, static_cast<const uint8_t*>(data) + begin, size);
      buffer->Unmap(0, &nothing_written);
      return true;
   }

   // DLAA in place of the game's TAA, recorded at its first tile, into TAA's RT0. RT1, the R8 mask and TAA's histories are read by TAA's draws alone.
   // False, with the game's TAA to run, if a piece is missing
   static bool DrawDLAA(CommandListData& cmd_list_data, DeviceData& device_data, AssassinsCreedValhallaDeviceData* game_device_data)
   {
      const D3D12State::CommandListState& state = cmd_list_data.d3d12_state;
      const uint64_t present_count = device_data.present_count.load(std::memory_order_relaxed);
      SR::InstanceData* const sr_instance_data = LockSRForDraw(device_data).instance_data;
      if (!sr_instance_data || state.in_render_pass || state.render_target_views.empty() || state.render_target_descs.empty() || !game_device_data->scene_ready.load(std::memory_order_relaxed))
      {
         return false;
      }
      // TAA's c3.xy jitter in render pixels and c6.y (-near), the view's c27.y projection up axis
      constexpr size_t constant_size = 16;
      std::array<float, 2> jitter;
      float negated_near_plane = 0.f;
      float projection_up_axis = 0.f;
      if (!ReadGameConstants(device_data, state, 2, 3 * constant_size, jitter.data(), sizeof(jitter)) ||
          !ReadGameConstants(device_data, state, 2, 6 * constant_size + sizeof(float), &negated_near_plane, sizeof(negated_near_plane)) ||
          !ReadGameConstants(device_data, state, 1, 27 * constant_size + sizeof(float), &projection_up_axis, sizeof(projection_up_axis)))
      {
         device_data.sr_suppressed = true;
         reshade::log::message(reshade::log::level::error, "[DLAA] The game's constants aren't readable (no tracked CBV, or not in an upload buffer), so there's no jitter and it's off until it's picked again");
         return false;
      }

      reshade::api::command_list* const cmd_list = cmd_list_data.cmd_list;
      reshade::api::device* const device = cmd_list->get_device();
      const reshade::api::resource target = {state.render_target_views[0].resource};
      const reshade::api::resource_desc target_desc = device->get_resource_desc(target);
      const reshade::api::resource scene = {game_device_data->scene_resource.load(std::memory_order_relaxed)};
      const reshade::api::resource_desc scene_desc = device->get_resource_desc(scene);
      // The render size: the game's Anti-Aliasing below High has TAA upsample by itself, into a bigger target
      const uint32_t width = scene_desc.texture.width;
      const uint32_t height = scene_desc.texture.height;
      // The game's HDR is what makes the target fp16
      const bool hdr = state.render_target_views[0].format == dlaa_color_format;
      const uint32_t output_width = uint32_t(device_data.output_resolution.x);
      const uint32_t output_height = uint32_t(device_data.output_resolution.y);
      // ponytail: HDR only, where the RCAS target is fp16 and the encoding known. In SDR the upscaler stays at the render size and the game's EASU follows it
      const bool upscaling = hdr && width < output_width;
      // Encoded into TAA's target otherwise, which needs the upscaler's output size
      if (!upscaling && (target_desc.texture.width != width || target_desc.texture.height != height))
      {
         return false;
      }
      // Not on top of the game's RCAS, which only runs behind its EASU (FSR-Best-Practices FIN-10)
      const bool sharpen = g_rcas_sharpness > 0.f && (upscaling || width >= output_width);

      // DLSS and XeSS take no mask from here
      const bool fsr_masks_used = device_data.sr_type == SR::Type::FSR || device_data.sr_type == SR::Type::LumaTAA;

      auto& inputs_pass = game_device_data->game_root_pass;
      bool output_recreated = false;
      {
         const std::unique_lock lock(inputs_pass.mutex);
         constexpr auto at_rest = reshade::api::resource_usage::shader_resource;
         bool output_created = false;
         const bool targets_ready = EnsureD3D12GamePassTarget(&inputs_pass, &game_device_data->color, device, present_count, width, height, dlaa_color_format, render_target_and_shader_resource, at_rest, L"Luma ACV DLAA Color") &&
                                    EnsureD3D12GamePassTarget(&inputs_pass, &game_device_data->motion_vectors, device, present_count, width, height, reshade::api::format::r16g16_float, render_target_and_shader_resource, at_rest, L"Luma ACV DLAA Motion Vectors") &&
                                    EnsureD3D12GamePassTarget(&inputs_pass, &game_device_data->depth, device, present_count, width, height, reshade::api::format::r32_float, render_target_and_shader_resource, at_rest, L"Luma ACV DLAA Depth") &&
                                    EnsureD3D12GamePassTarget(&inputs_pass, &game_device_data->reactive, device, present_count, width, height, reshade::api::format::r8_unorm, render_target_and_shader_resource, at_rest, L"Luma ACV DLAA Reactive") &&
                                    EnsureD3D12GamePassTarget(&inputs_pass, &game_device_data->output, device, present_count, (upscaling ? output_width : width), (upscaling ? output_height : height), dlaa_color_format, reshade::api::resource_usage::unordered_access | reshade::api::resource_usage::shader_resource, reshade::api::resource_usage::unordered_access, L"Luma ACV DLAA Output", &output_created) &&
                                    (!sharpen || EnsureD3D12GamePassTarget(&inputs_pass, &game_device_data->sharpened, device, present_count, (upscaling ? output_width : width), (upscaling ? output_height : height), dlaa_color_format, render_target_and_shader_resource, at_rest, L"Luma ACV DLAA Sharpened")) &&
                                    (!upscaling || EnsureD3D12GamePassTarget(&inputs_pass, &game_device_data->encoded, device, present_count, output_width, output_height, dlaa_color_format, render_target_and_shader_resource, at_rest, L"Luma ACV DLAA Encoded"));
         if (!targets_ready)
         {
            device_data.sr_suppressed = true;
            reshade::log::message(reshade::log::level::error, "[DLAA] ReShade refused its textures, so it's off until it's picked again");
            return false;
         }
         game_device_data->output_recreated = game_device_data->output_recreated || output_created;
         output_recreated = game_device_data->output_recreated;

         const auto draw_input = [&](reshade::api::resource input, reshade::api::format format, uint32_t pixel_shader_hash, const char* name)
         {
            cmd_list->barrier(input, reshade::api::resource_usage::shader_resource, reshade::api::resource_usage::render_target);
            const bool drawn = DrawGameRootPass(cmd_list_data, device_data, &inputs_pass, pixel_shader_hash, input, format, name);
            cmd_list->barrier(input, reshade::api::resource_usage::render_target, reshade::api::resource_usage::shader_resource);
            return drawn;
         };
         if (!draw_input(game_device_data->motion_vectors, reshade::api::format::r16g16_float, CompileTimeStringHash("ACV DLAA Motion Vectors PS"), "Luma DLAA Motion Vectors") ||
             !draw_input(game_device_data->depth, reshade::api::format::r32_float, CompileTimeStringHash("ACV DLAA Depth PS"), "Luma DLAA Depth") ||
             (fsr_masks_used && !draw_input(game_device_data->reactive, reshade::api::format::r8_unorm, CompileTimeStringHash("ACV DLAA Reactive PS"), "Luma DLAA Reactive")))
         {
            return false;
         }
      }

      // The scene is PQ: decoded, with an exposure, as DLSS' HDR mode requires (DLSS-Best-Practices EXP-7)
      const bool linear_input = hdr && g_linear_upscaler_input;
      const float color_params[] = {linear_input ? 1.f : 0.f, 0.f};
      const reshade::api::resource_view_desc color_desc(reshade::api::resource_view_type::texture_2d, dlaa_color_format, 0, 1, 0, 1);
      cmd_list->barrier(game_device_data->color, reshade::api::resource_usage::shader_resource, reshade::api::resource_usage::render_target);
      const bool decoded = DrawFullscreenPassD3D12(cmd_list_data, device_data, &game_device_data->color_pass, CompileTimeStringHash("ACV DLAA Decode PS"), "DLAA Decode", "Luma DLAA Decode", scene, game_device_data->scene_format.load(std::memory_order_relaxed), game_device_data->color, color_desc, color_params);
      cmd_list->barrier(game_device_data->color, reshade::api::resource_usage::render_target, reshade::api::resource_usage::shader_resource);
      if (!decoded)
      {
         return false;
      }

      auto* const native_list = reinterpret_cast<ID3D12GraphicsCommandList*>(cmd_list->get_native());
      // UV deltas, previous minus current. The game has no exposure to give at this point (DLSS-Best-Practices EXP-4, FSR-Best-Practices FIN-3)
      const SR::SettingsData settings_data = {.output_width = (upscaling ? output_width : width), .output_height = (upscaling ? output_height : height), .render_width = width, .render_height = height, .hdr = linear_input, .inverted_depth = true, .mvs_jittered = false, .mvs_x_scale = float(width), .mvs_y_scale = float(height), .auto_exposure = (linear_input && device_data.sr_type != SR::Type::FSR), .render_preset = dlss_render_preset};
      const SR::SuperResolutionImpl::DrawData draw_data = {
         .reset = device_data.force_reset_sr,
         .output_recreated = output_recreated,
         .output_color = reinterpret_cast<ID3D12Resource*>(game_device_data->output.handle),
         .source_color = reinterpret_cast<ID3D12Resource*>(game_device_data->color.handle),
         .motion_vectors = reinterpret_cast<ID3D12Resource*>(game_device_data->motion_vectors.handle),
         .depth_buffer = reinterpret_cast<ID3D12Resource*>(game_device_data->depth.handle),
         .bias_mask = (fsr_masks_used ? reinterpret_cast<ID3D12Resource*>(game_device_data->reactive.handle) : nullptr),
         .render_width = width,
         .render_height = height,
         // As is (DLSS-Best-Practices JIT-6): the input moves by +jitter on both axes, the crawl on a frozen frame is the lowest of the four signs
         .jitter_x = jitter[0],
         .jitter_y = jitter[1],
         // FSR's camera (FSR-Best-Practices FCAM-1..3). The projection is reverse Z infinite: a stand in far
         .vert_fov = 2.f * std::atan(1.f / std::abs(projection_up_axis)),
         .near_plane = -negated_near_plane,
         .far_plane = 100000.f,
      };
      bool sr_drawn = false;
      bool sr_switched = false;
      {
         const SRDrawLock sr = LockSRForDraw(device_data);
         // A present switched the SR since "sr_instance_data" was read: nothing is drawn this frame
         sr_switched = sr.instance_data != sr_instance_data;
         sr_drawn = !sr_switched && sr.implementation->UpdateSettings(sr.instance_data, native_list, settings_data) && sr.implementation->Draw(sr.instance_data, native_list, draw_data);
      }
      // The upscalers bind their own descriptor heaps, root signatures and pipelines
      D3D12State::RestoreCompute(cmd_list, state, {});
      RestoreGameGraphicsD3D12(cmd_list_data);
      TraceLumaPass(&cmd_list_data, device_data, "Luma DLAA", {{.resource = game_device_data->color.handle, .format = dlaa_color_format}, {.resource = game_device_data->motion_vectors.handle, .format = reshade::api::format::r16g16_float}, {.resource = game_device_data->depth.handle, .format = reshade::api::format::r32_float}}, {{.resource = game_device_data->output.handle, .format = dlaa_color_format}}, true);
      if (!sr_drawn)
      {
         if (!sr_switched)
         {
            device_data.sr_suppressed = true;
            reshade::log::message(reshade::log::level::error, std::format("[DLAA] {} failed, so it's off until it's picked again", SR::GetTypeName(device_data.sr_type)).c_str());
         }
         return false;
      }
      {
         const std::unique_lock lock(inputs_pass.mutex);
         game_device_data->output_recreated = false;
      }

      cmd_list->barrier(game_device_data->output, reshade::api::resource_usage::unordered_access, reshade::api::resource_usage::shader_resource);
      if (sharpen)
      {
         cmd_list->barrier(game_device_data->sharpened, reshade::api::resource_usage::shader_resource, reshade::api::resource_usage::render_target);
         const bool sharpened = DrawRCAS(cmd_list_data, device_data, game_device_data->output, dlaa_color_format, game_device_data->sharpened, color_desc, g_rcas_sharpness);
         cmd_list->barrier(game_device_data->sharpened, reshade::api::resource_usage::render_target, reshade::api::resource_usage::shader_resource);
         if (!sharpened)
         {
            cmd_list->barrier(game_device_data->output, reshade::api::resource_usage::shader_resource, reshade::api::resource_usage::unordered_access);
            return false;
         }
      }
      if (upscaling)
      {
         cmd_list->barrier(game_device_data->encoded, reshade::api::resource_usage::shader_resource, reshade::api::resource_usage::render_target);
      }
      const bool encoded = DrawFullscreenPassD3D12(cmd_list_data, device_data, &game_device_data->color_pass, CompileTimeStringHash("ACV DLAA Encode PS"), "DLAA Encode", "Luma DLAA Encode", (sharpen ? game_device_data->sharpened : game_device_data->output), dlaa_color_format, (upscaling ? game_device_data->encoded : target), (upscaling ? color_desc : state.render_target_descs[0]), color_params);
      if (upscaling)
      {
         cmd_list->barrier(game_device_data->encoded, reshade::api::resource_usage::render_target, reshade::api::resource_usage::shader_resource);
      }
      // TAA's target at its own size too, for what reads it after TAA besides the replaced composites (DLSS-Best-Practices POST-6): they'd read a stale scene otherwise
      if (upscaling && encoded && !DrawFullscreenPassD3D12(cmd_list_data, device_data, &game_device_data->color_pass, CompileTimeStringHash("ACV DLAA Resample PS"), "DLAA Resample", "Luma DLAA Resample", game_device_data->encoded, dlaa_color_format, target, state.render_target_descs[0], color_params))
      {
         static std::atomic_flag logged;
         if (!logged.test_and_set())
         {
            reshade::log::message(reshade::log::level::warning, "[DLAA] TAA's target couldn't be filled: what reads it after TAA, besides the composite, sees a stale scene");
         }
      }
      cmd_list->barrier(game_device_data->output, reshade::api::resource_usage::shader_resource, reshade::api::resource_usage::unordered_access);
      device_data.has_drawn_sr = device_data.has_drawn_sr || encoded;
      if (encoded)
      {
         game_device_data->upscaled.store(upscaling, std::memory_order_relaxed);
      }
      return encoded;
   }

   // The game's composite with the upscaler's output for a scene: its t1,space4 is the render size, and its descriptor can't be replaced, as the game can write it after the draw is recorded.
   // The UI goes through the game's own bindings first, then a Luma pass mixes the two
   // "pq_output": HDR / HDRGameDVR, which leaves the mix in PQ; HDRSCRGB converts it to scRGB
   static bool DrawUpscaledComposite(CommandListData& cmd_list_data, DeviceData& device_data, AssassinsCreedValhallaDeviceData* game_device_data, bool pq_output)
   {
      const D3D12State::CommandListState& state = cmd_list_data.d3d12_state;
      float scene_limit = 0.f; // c0.w
      if (state.in_render_pass || state.render_target_views.empty() || state.render_target_descs.empty() || !ReadGameConstants(device_data, state, 4, 3 * sizeof(float), &scene_limit, sizeof(scene_limit)))
      {
         return false;
      }
      reshade::api::command_list* const cmd_list = cmd_list_data.cmd_list;
      reshade::api::device* const device = cmd_list->get_device();
      const reshade::api::resource target = {state.render_target_views[0].resource};
      const reshade::api::resource_desc target_desc = device->get_resource_desc(target);
      {
         auto& pass = game_device_data->composite_ui_pass;
         const std::unique_lock lock(pass.mutex);
         constexpr auto at_rest = reshade::api::resource_usage::shader_resource;
         if (!EnsureD3D12GamePassTarget(&pass, &game_device_data->composite_ui, device, device_data.present_count.load(std::memory_order_relaxed), target_desc.texture.width, target_desc.texture.height, dlaa_color_format, render_target_and_shader_resource, at_rest, L"Luma ACV DLAA Composite UI"))
         {
            return false;
         }
         cmd_list->barrier(game_device_data->composite_ui, at_rest, reshade::api::resource_usage::render_target);
         const bool ui_drawn = DrawGameRootPass(cmd_list_data, device_data, &pass, CompileTimeStringHash("ACV DLAA Composite UI PS"), game_device_data->composite_ui, dlaa_color_format, "Luma DLAA Composite UI");
         cmd_list->barrier(game_device_data->composite_ui, reshade::api::resource_usage::render_target, at_rest);
         if (!ui_drawn)
         {
            return false;
         }
      }
      const D3D12FullscreenSource sources[] = {{.resource = game_device_data->encoded, .format = dlaa_color_format}, {.resource = game_device_data->composite_ui, .format = dlaa_color_format}};
      const float params[] = {pq_output ? 1.f : 0.f, scene_limit};
      return DrawFullscreenPassD3D12(cmd_list_data, device_data, &game_device_data->composite_pass, CompileTimeStringHash("ACV DLAA Composite PS"), "DLAA Composite", "Luma DLAA Composite", sources, target, state.render_target_descs[0], params);
   }

public:
   void OnInit(bool async) override
   {
      // No replacement reads them, so the game's root signatures stay as they came (see "D3D12RootSignatures")
      luma_settings_cbuffer_index = -1;
      luma_data_cbuffer_index = -1;

      constexpr auto pixel_shader = reshade::api::pipeline_subobject_type::pixel_shader;
      native_shaders_definitions.emplace(CompileTimeStringHash("ACV DLAA Scene Copy PS"), ShaderDefinition{.file_name = "Luma_ACV_DLAA", .type = pixel_shader, .function_name = "scene_copy_ps"});
      native_shaders_definitions.emplace(CompileTimeStringHash("ACV DLAA Motion Vectors PS"), ShaderDefinition{.file_name = "Luma_ACV_DLAA", .type = pixel_shader, .function_name = "motion_vectors_ps"});
      native_shaders_definitions.emplace(CompileTimeStringHash("ACV DLAA Depth PS"), ShaderDefinition{.file_name = "Luma_ACV_DLAA", .type = pixel_shader, .function_name = "depth_ps"});
      native_shaders_definitions.emplace(CompileTimeStringHash("ACV DLAA Reactive PS"), ShaderDefinition{.file_name = "Luma_ACV_DLAA", .type = pixel_shader, .function_name = "reactive_ps"});
      native_shaders_definitions.emplace(CompileTimeStringHash("ACV DLAA Decode PS"), ShaderDefinition{.file_name = "Luma_ACV_DLAA", .type = pixel_shader, .function_name = "decode_ps"});
#if DEVELOPMENT
      Mcp::RegisterToggles({{ "linear_upscaler_input",
         &g_linear_upscaler_input }});
      // Between draws in shader resource states, "output" in unordered access
      constexpr D3D12_RESOURCE_STATES shader_resource = D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;
      Mcp::RegisterTextures({{"acv.color", &GetDevTexture<&AssassinsCreedValhallaDeviceData::color, shader_resource>},
         {"acv.output", &GetDevTexture<&AssassinsCreedValhallaDeviceData::output, D3D12_RESOURCE_STATE_UNORDERED_ACCESS>},
         { "acv.encoded",
            &GetDevTexture<&AssassinsCreedValhallaDeviceData::encoded, shader_resource> }});
#endif
      native_shaders_definitions.emplace(CompileTimeStringHash("ACV DLAA Encode PS"), ShaderDefinition{.file_name = "Luma_ACV_DLAA", .type = pixel_shader, .function_name = "encode_ps"});
      native_shaders_definitions.emplace(CompileTimeStringHash("ACV DLAA Resample PS"), ShaderDefinition{.file_name = "Luma_ACV_DLAA", .type = pixel_shader, .function_name = "resample_ps"});
      native_shaders_definitions.emplace(CompileTimeStringHash("ACV DLAA Composite UI PS"), ShaderDefinition{.file_name = "Luma_ACV_DLAA", .type = pixel_shader, .function_name = "composite_ui_ps"});
      native_shaders_definitions.emplace(CompileTimeStringHash("ACV DLAA Composite PS"), ShaderDefinition{.file_name = "Luma_ACV_DLAA", .type = pixel_shader, .function_name = "composite_ps"});
   }

   void OnCreateDevice(NativeDevice* native_device, DeviceData& device_data) override
   {
      device_data.game = new AssassinsCreedValhallaDeviceData;
   }

   DrawOrDispatchOverrideType OnDrawOrDispatch(NativeDevice* native_device, NativeCommandList* native_device_context, CommandListData& cmd_list_data, DeviceData& device_data, reshade::api::shader_stage stages, const ShaderHashesList<OneShaderPerPipeline>& original_shader_hashes, bool is_custom_pass, bool& updated_cbuffers, std::function<void()>* original_draw_dispatch_func) override
   {
      auto& game_device_data = static_cast<AssassinsCreedValhallaDeviceData&>(*device_data.game);
      if (original_shader_hashes.Contains(shader_hashes_dither_blend) && game_device_data.dlaa_active.load(std::memory_order_relaxed))
      {
         game_device_data.dlaa_claimed.store(false, std::memory_order_relaxed);
         game_device_data.dlaa_drawn.store(false, std::memory_order_relaxed);
         game_device_data.scene_ready.store(false, std::memory_order_relaxed);
         const D3D12State::CommandListState& state = cmd_list_data.d3d12_state;
         if (state.render_target_views.empty())
         {
            return DrawOrDispatchOverrideType::None;
         }
         const D3D12State::ViewInfo& target = state.render_target_views[0];
         auto& pass = game_device_data.game_root_pass;
         const std::unique_lock lock(pass.mutex);
         if (DrawGameRootPass(cmd_list_data, device_data, &pass, CompileTimeStringHash("ACV DLAA Scene Copy PS"), {target.resource}, target.format, "Luma DLAA Scene Copy"))
         {
            game_device_data.scene_resource.store(target.resource, std::memory_order_relaxed);
            game_device_data.scene_format.store(target.format, std::memory_order_relaxed);
            game_device_data.scene_ready.store(true, std::memory_order_relaxed);
         }
         return DrawOrDispatchOverrideType::None;
      }
      if (original_shader_hashes.Contains(shader_hashes_composite_hdr))
      {
         const bool upscaled = game_device_data.upscaled.load(std::memory_order_relaxed);
         const bool composited = upscaled && DrawUpscaledComposite(cmd_list_data, device_data, &game_device_data, original_shader_hashes.pixel_shaders[0] == composite_hdr10);
         if (upscaled && !composited)
         {
            // The game's composite shows the scaled down copy in TAA's target this frame
            device_data.sr_suppressed = true;
            reshade::log::message(reshade::log::level::error, "[DLAA] The composite couldn't be drawn, so it's off until it's picked again");
         }
         return (composited ? DrawOrDispatchOverrideType::Skip : DrawOrDispatchOverrideType::None);
      }
      if (game_device_data.upscaled.load(std::memory_order_relaxed) && original_shader_hashes.Contains(shader_hashes_game_upscale))
      {
         return DrawOrDispatchOverrideType::Skip;
      }
      if (!original_shader_hashes.Contains(shader_hashes_taa))
      {
         return DrawOrDispatchOverrideType::None;
      }
      // The last pass of the scene's post (DLSS best practices BK-1: Core's upscaler indicator)
      device_data.has_drawn_main_post_processing = true;
      if (!game_device_data.dlaa_active.load(std::memory_order_relaxed))
      {
         game_device_data.upscaled.store(false, std::memory_order_relaxed);
         return DrawOrDispatchOverrideType::None;
      }
      if (!game_device_data.dlaa_claimed.exchange(true, std::memory_order_relaxed) && DrawDLAA(cmd_list_data, device_data, &game_device_data))
      {
         game_device_data.dlaa_drawn.store(true, std::memory_order_relaxed);
      }
      if (game_device_data.dlaa_drawn.load(std::memory_order_relaxed))
      {
         return DrawOrDispatchOverrideType::Skip;
      }
      game_device_data.upscaled.store(false, std::memory_order_relaxed);
      return DrawOrDispatchOverrideType::None;
   }

   void OnPresent(NativeDevice* native_device, DeviceData& device_data) override
   {
      auto& game_device_data = static_cast<AssassinsCreedValhallaDeviceData&>(*device_data.game);
      // The upscaler's history restarts after a frame it didn't draw (menus, loading, just picked)
      // Video and menus have no TAA to turn "upscaled" off, and the composite must go back to their scene. Not at once, as a list can land past a present
      constexpr uint32_t presents_to_leave_upscaled = 3;
      game_device_data.presents_without_upscaler = (device_data.has_drawn_sr ? 0 : game_device_data.presents_without_upscaler + 1);
      if (game_device_data.presents_without_upscaler >= presents_to_leave_upscaled)
      {
         game_device_data.upscaled.store(false, std::memory_order_relaxed);
      }
      const bool dlaa_active = LatchSRFrame(device_data);
      game_device_data.dlaa_active.store(dlaa_active, std::memory_order_relaxed);
      // The game adds no bias of its own
      const SR::InstanceData* const sr_instance_data = device_data.GetSRInstanceData();
      SetTextureMipLodBias(nullptr, device_data, ((dlaa_active && sr_instance_data) ? sr_implementations[device_data.sr_type]->GetMipLODBias(sr_instance_data) : 0.f));
      JitterPhases::Apply((dlaa_active && sr_instance_data) ? uint32_t(sr_implementations[device_data.sr_type]->GetJitterPhases(sr_instance_data)) : 8);
      FullRenderArea::Apply(dlaa_active && sr_instance_data);
   }

   void LoadConfigs() override
   {
      reshade::get_config_value(nullptr, NAME, "RCASSharpness", g_rcas_sharpness);
   }

   void DrawImGuiSettings(DeviceData& device_data) override
   {
      DrawSettingSlider("RCAS Sharpness", "RCASSharpness", &g_rcas_sharpness, 0.f, 0.f, 1.f, "Sharpening applied on top of DLSS, FSR or XeSS (0 = off).");
   }

   void PrintImGuiAbout() override
   {
      ImGui::Text("Assassin's Creed Valhalla Luma mod - about and credits section", "");
   }
};

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
   if (ul_reason_for_call == DLL_PROCESS_ATTACH)
   {
      Globals::SetGlobals(PROJECT_NAME, "Assassin's Creed Valhalla Luma mod", "", 1);

      // The game's own HDR writes scRGB: Core's composition would scale it by the paper white again
      force_disable_display_composition = true;

      // The upscalers' mip bias (see "OnPresent()")
      enable_samplers_upgrade = true;

      game = new AssassinsCreedValhalla();
   }

   CoreMain(hModule, ul_reason_for_call, lpReserved);

   return TRUE;
}
