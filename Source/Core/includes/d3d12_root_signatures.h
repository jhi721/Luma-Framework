#pragma once

#if DX12 && ENABLE_MINHOOK

// Luma's own root CBVs (LumaSettings and LumaData) in every root signature the game creates, so the shaders Luma replaces can read them.
// ReShade's way to edit a root signature (an add-on changing the layout in "create_pipeline_layout") makes it rebuild the root signature
// from its own model of it, which loses what the model lacks (root signature 1.2's static samplers, the root descriptors' flags, the node
// mask) and fails the game's create if the rebuild fails. So the blob is rewritten before ReShade sees it: a detour on the
// "CreateRootSignature" of ReShade's device proxy (one function for every proxy) appends the CBVs with the D3D12 runtime's own
// (de)serializer, at the blob's own version, and ReShade creates the result as it would the game's. The root signatures that can't take
// the CBVs (see "Skip") go through without them. The static samplers are upgraded in the same rewrite, CBVs or not.
// Include it where "utils/shader_compiler.hpp" can be included: that header relies on "Globals", "Hash" and <fstream> coming before it.

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <format>
#include <mutex>
#include <shared_mutex>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <Windows.h>
#include <d3d12.h>
#include <source/com_ptr.hpp>

#include "d3d12_resource_creation.h" // The hook of a function of ReShade's device proxy
#include "../utils/shader_compiler.hpp"

namespace D3D12RootSignatures
{
   // The register space of Luma's CBVs, "space51" in HLSL
   constexpr uint32_t LUMA_CB_REGISTER_SPACE = 51;

   // "ID3D12Device::CreateRootSignature" follows IUnknown (3 methods), ID3D12Object (4) and the 9 methods of ID3D12Device before it
   constexpr size_t CREATE_ROOT_SIGNATURE_SLOT = 16;

   // The DWORDs a root descriptor takes of the 64 ("D3D12_MAX_ROOT_COST") a root signature holds (a table takes 1, root constants 1 each)
   constexpr uint32_t ROOT_DESCRIPTOR_COST = 2;
   // Our CBVs must reach the replaced stages, which these flags keep a root signature's parameters from
   constexpr D3D12_ROOT_SIGNATURE_FLAGS DENY_SHADER_ROOT_ACCESS =
      D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS | D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
      D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS | D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
      D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS | D3D12_ROOT_SIGNATURE_FLAG_DENY_AMPLIFICATION_SHADER_ROOT_ACCESS |
      D3D12_ROOT_SIGNATURE_FLAG_DENY_MESH_SHADER_ROOT_ACCESS;

   // Why a root signature went through as it came
   enum class Skip : uint8_t
   {
      None,            // It was rewritten
      ShaderContainer, // A shader with an embedded root signature: the shader can't change, and its pipelines would take its own root signature
      NotDeserializable,
      LocalRootSignature, // For DXR shader records
      RegistersInUse,     // A parameter already uses our registers in our space
      OverBudget,         // It would hold more than "D3D12_MAX_ROOT_COST" DWORDs with the CBVs
      NotSerializable,
   };
   constexpr std::array<const char*, 7> SKIP_NAMES = {
      "rewritten",
      "it is a shader with an embedded root signature",
      "it can't be deserialized",
      "it is a local root signature",
      "a parameter already uses Luma's registers",
      "it would go over the 64 DWORDs a root signature holds",
      "it can't be serialized with the CBVs",
   };

   // The root parameters of the CBVs appended to a root signature (the game's own keep their indices), -1 for a CBV it has none of, and why
   // a root signature created through the hook has none ("Skip::None" for one the hook never saw)
   struct LumaParams
   {
      int32_t settings_index = -1;
      int32_t data_index = -1;
      Skip skip = Skip::None;
   };

   using CreateRootSignatureFunction = HRESULT(STDMETHODCALLTYPE*)(ID3D12Device*, UINT, const void*, SIZE_T, REFIID, void**);
   // Writes a line to the log. The header doesn't call ReShade's, as offline tests have no ReShade runtime
   using LogFunction = void (*)(const char* message);

   struct Hook : D3D12ProxyHook::HookedFunction<CreateRootSignatureFunction>
   {
      std::shared_mutex mutex; // For "Install()" and "Remove()". The detour reads the rest without it, as it only runs between them

      // Set before the hook goes in
      uint32_t settings_register = 0;
      int32_t data_register = -1;
      LogFunction log = nullptr;
      // The runtime's exports, which Luma doesn't link
      PFN_D3D12_CREATE_VERSIONED_ROOT_SIGNATURE_DESERIALIZER create_deserializer = nullptr;
      PFN_D3D12_SERIALIZE_VERSIONED_ROOT_SIGNATURE serialize = nullptr;
      // Set by the user of the header, optional: changes a static sampler (of any version: the older ones come as a 1.2 one with no flags) and says whether it did.
      // ReShade doesn't pass root signature 1.2 static samplers to its "create_sampler" event, and the older versions' make it rebuild the root signature from its own
      // model when an add-on changes one, so the samplers are upgraded here, on the blob ReShade creates the root signature from. Called under the detour, any thread
      bool (*upgrade_static_sampler)(D3D12_STATIC_SAMPLER_DESC1* desc) = nullptr;
   };
   // Shared by the devices: ReShade's proxies all lead to one function, hooked once per process
   inline Hook hook;

   // Appends our CBVs to a serialized root signature and upgrades its static samplers ("Hook::upgrade_static_sampler"). The result is about the CBVs: why they couldn't go in,
   // "Skip::None" if they did. "rewritten" is written when the blob changed (the CBVs, the samplers or both), "params" when the CBVs went in, "parameter_count" is the
   // root signature's own (0 if it couldn't be deserialized). Needs "Install()" to have found the runtime's exports.
   // "static_samplers", if given, gets the static samplers of a root signature 1.2 (ReShade skips them) as the game made them, or as "Hook::upgrade_static_sampler" changed them in the
   // rewritten one: it is written whatever the result, nothing for the older versions.
   inline Skip Rewrite(std::span<const std::byte> blob, uint32_t settings_register, int32_t data_register, std::vector<std::byte>* rewritten, LumaParams* params, uint32_t* parameter_count, std::vector<D3D12_STATIC_SAMPLER_DESC1>* static_samplers = nullptr)
   {
      *parameter_count = 0;
      // A plain serialized root signature is a container too, with only its "RTS0" part
      if (Shader::FindProgramChunk(blob.data(), blob.size()))
      {
         return Skip::ShaderContainer;
      }
      com_ptr<ID3D12VersionedRootSignatureDeserializer> deserializer;
      if (FAILED(hook.create_deserializer(blob.data(), blob.size(), IID_PPV_ARGS(&deserializer))))
      {
         return Skip::NotDeserializable;
      }
      // At the version the blob has: converting it would lose what that version holds (1.2's static sampler flags, 1.1's root descriptor flags)
      const D3D12_VERSIONED_ROOT_SIGNATURE_DESC* const original = deserializer->GetUnconvertedRootSignatureDesc();
      if (!original)
      {
         return Skip::NotDeserializable;
      }
      D3D12_VERSIONED_ROOT_SIGNATURE_DESC extended = *original; // Its pointers lead to the deserializer's memory

      const std::array<uint32_t, 2> registers = {settings_register, static_cast<uint32_t>(data_register)};
      const std::span<const uint32_t> luma_registers = std::span(registers).first(data_register >= 0 ? 2 : 1);
      // Whether "count" registers from "first" in a space (UINT_MAX: unbounded) hold one of ours
      const auto uses_luma_register = [&luma_registers](uint32_t space, uint32_t first, uint32_t count)
      {
         const auto in_range = [first, count](uint32_t luma_register)
         {
            return luma_register >= first && (count == UINT_MAX || luma_register - first < count);
         };
         return space == LUMA_CB_REGISTER_SPACE && std::ranges::any_of(luma_registers, in_range);
      };

      // The three versions have their own types ("desc" is a "D3D12_ROOT_SIGNATURE_DESC", "...DESC1" or "...DESC2", with "D3D12_ROOT_PARAMETER"s
      // or "...PARAMETER1"s) but the same member names, so these lambdas take them all. "append" changes "desc", the member of "extended" it is.
      const auto check_cbvs = [&](const auto& desc) -> Skip
      {
         using Parameter = std::remove_cvref_t<decltype(*desc.pParameters)>;
         if ((desc.Flags & D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE) != 0)
         {
            return Skip::LocalRootSignature;
         }

         bool registers_in_use = false;
         uint64_t cost = 0;
         for (const Parameter& parameter : std::span(desc.pParameters, desc.NumParameters))
         {
            switch (parameter.ParameterType)
            {
            case D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE:
            {
               const auto ranges = std::span(parameter.DescriptorTable.pDescriptorRanges, parameter.DescriptorTable.NumDescriptorRanges);
               registers_in_use = registers_in_use || std::ranges::any_of(ranges, [&](const auto& range)
                                                         { return range.RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_CBV && uses_luma_register(range.RegisterSpace, range.BaseShaderRegister, range.NumDescriptors); });
               cost += 1;
               break;
            }
            case D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS:
            {
               registers_in_use = registers_in_use || uses_luma_register(parameter.Constants.RegisterSpace, parameter.Constants.ShaderRegister, 1);
               cost += parameter.Constants.Num32BitValues;
               break;
            }
            case D3D12_ROOT_PARAMETER_TYPE_CBV:
            {
               registers_in_use = registers_in_use || uses_luma_register(parameter.Descriptor.RegisterSpace, parameter.Descriptor.ShaderRegister, 1);
               cost += ROOT_DESCRIPTOR_COST;
               break;
            }
            case D3D12_ROOT_PARAMETER_TYPE_SRV:
            case D3D12_ROOT_PARAMETER_TYPE_UAV:
            {
               cost += ROOT_DESCRIPTOR_COST;
               break;
            }
            }
         }
         if (registers_in_use)
         {
            return Skip::RegistersInUse;
         }
         if (cost + ROOT_DESCRIPTOR_COST * luma_registers.size() > D3D12_MAX_ROOT_COST)
         {
            return Skip::OverBudget;
         }
         return Skip::None;
      };

      const auto append = [&](auto& desc) -> Skip
      {
         using Parameter = std::remove_cvref_t<decltype(*desc.pParameters)>;
         using StaticSampler = std::remove_cvref_t<decltype(*desc.pStaticSamplers)>;
         constexpr bool has_static_samplers_1 = std::is_same_v<StaticSampler, D3D12_STATIC_SAMPLER_DESC1>;
         *parameter_count = desc.NumParameters;
         if constexpr (has_static_samplers_1)
         {
            if (static_samplers)
            {
               static_samplers->assign(desc.pStaticSamplers, desc.pStaticSamplers + desc.NumStaticSamplers);
            }
         }

         const Skip skip = check_cbvs(desc);
         const uint32_t original_parameter_count = desc.NumParameters;
         std::vector<Parameter> parameters;
         if (skip == Skip::None)
         {
            parameters.assign(desc.pParameters, desc.pParameters + desc.NumParameters);
            for (const uint32_t luma_register : luma_registers)
            {
               Parameter& cbv = parameters.emplace_back();
               cbv.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
               cbv.Descriptor.ShaderRegister = luma_register;
               cbv.Descriptor.RegisterSpace = LUMA_CB_REGISTER_SPACE;
               cbv.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
               if constexpr (requires { cbv.Descriptor.Flags; })
               {
                  cbv.Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE; // Root signature 1.1 and later
               }
            }
            desc.NumParameters = static_cast<uint32_t>(parameters.size());
            desc.pParameters = parameters.data();
            desc.Flags &= ~DENY_SHADER_ROOT_ACCESS; // The static samplers and everything else stay, but for the ones the hook upgrades
         }

         // Upgraded whether or not the CBVs went in. The older versions' samplers lack 1.2's "Flags", their last member, and go through the callback as a 1.2 one.
         static_assert(offsetof(D3D12_STATIC_SAMPLER_DESC1, Flags) == sizeof(D3D12_STATIC_SAMPLER_DESC));
         std::vector<StaticSampler> upgraded_samplers;
         bool samplers_upgraded = false;
         if (hook.upgrade_static_sampler && desc.NumStaticSamplers != 0)
         {
            upgraded_samplers.assign(desc.pStaticSamplers, desc.pStaticSamplers + desc.NumStaticSamplers);
            for (StaticSampler& sampler : upgraded_samplers)
            {
               if constexpr (has_static_samplers_1)
               {
                  samplers_upgraded |= hook.upgrade_static_sampler(&sampler);
               }
               else
               {
                  D3D12_STATIC_SAMPLER_DESC1 widened = {};
                  std::memcpy(&widened, &sampler, sizeof(sampler));
                  if (hook.upgrade_static_sampler(&widened))
                  {
                     std::memcpy(&sampler, &widened, sizeof(sampler));
                     samplers_upgraded = true;
                  }
               }
            }
            if (samplers_upgraded)
            {
               desc.pStaticSamplers = upgraded_samplers.data();
            }
         }
         if (skip != Skip::None && !samplers_upgraded)
         {
            return skip;
         }

         com_ptr<ID3DBlob> serialized;
         if (FAILED(hook.serialize(&extended, &serialized, nullptr)))
         {
            return (skip == Skip::None ? Skip::NotSerializable : skip);
         }
         if constexpr (has_static_samplers_1)
         {
            if (static_samplers && samplers_upgraded)
            {
               *static_samplers = upgraded_samplers;
            }
         }
         const auto* const bytes = static_cast<const std::byte*>(serialized->GetBufferPointer());
         rewritten->assign(bytes, bytes + serialized->GetBufferSize());
         if (skip == Skip::None)
         {
            *params = {.settings_index = static_cast<int32_t>(original_parameter_count), .data_index = (data_register >= 0 ? static_cast<int32_t>(original_parameter_count) + 1 : -1)};
         }
         return skip;
      };

      switch (extended.Version)
      {
      case D3D_ROOT_SIGNATURE_VERSION_1_0:
         return append(extended.Desc_1_0);
      case D3D_ROOT_SIGNATURE_VERSION_1_1:
         return append(extended.Desc_1_1);
      case D3D_ROOT_SIGNATURE_VERSION_1_2:
         return append(extended.Desc_1_2);
      default: // A version newer than the ones we know the layout of
         return Skip::NotDeserializable;
      }
   }

   // The root signatures created through the hook, with or without CBVs, by their "ID3D12RootSignature" (a "reshade::api::pipeline_layout"
   // handle)
   struct Injected
   {
      std::shared_mutex mutex;
      std::unordered_map<uint64_t, LumaParams> by_root_signature;
      // The "RTS0" parts of their originals, as the game made them
      std::unordered_set<std::string> original_parts;
   };
   inline Injected injected;

   // The "RTS0" part of a serialized root signature or of a shader with one (the root signature itself, without the container around it),
   // empty if there is none
   inline std::string_view RootSignaturePart(const void* container, size_t size)
   {
      const Shader::DXBCChunk* const part = Shader::FindChunk(container, size, "RTS0");
      return (part ? std::string_view(reinterpret_cast<const char*>(part->chunk_data), part->chunk_size) : std::string_view());
   }

   // The logs are for the first few of each, as a game can create thousands of root signatures
   constexpr uint32_t MAX_LOGS = 3;
   inline std::array<std::atomic<uint32_t>, SKIP_NAMES.size()> skip_logs;
   inline std::atomic<uint32_t> create_failure_logs;
   // Set while this thread creates a root signature through the hook: ReShade's events run inside the create, and a root signature an
   // add-on creates from one of them is not the game's
   inline thread_local bool rewriting = false;
   // Set while this thread's ReShade proxy creates a rewritten blob: its static samplers went through "Hook::upgrade_static_sampler" already, so the "create_sampler" event
   // ReShade fires for the older versions' must leave them (a second upgrade would add the bias again, and a change makes ReShade rebuild the root signature)
   inline thread_local bool static_samplers_upgraded = false;
#if DEVELOPMENT
   // The root signature 1.2 static samplers of the root signature this thread creates through the hook, for the "init_pipeline_layout" event that runs inside the create (ReShade's
   // pipeline layout lacks them): the frame capture lists them (see "D3D12State::AddStaticSamplers()")
   inline thread_local std::vector<D3D12_STATIC_SAMPLER_DESC1> pending_static_samplers;
#endif

   inline HRESULT STDMETHODCALLTYPE Detour(ID3D12Device* device, UINT node_mask, const void* blob, SIZE_T size, REFIID riid, void** root_signature)
   {
      if (rewriting || !root_signature || riid != __uuidof(ID3D12RootSignature))
      {
         return hook.original(device, node_mask, blob, size, riid, root_signature);
      }
      const bool was_rewriting = std::exchange(rewriting, true);

      std::vector<std::byte> rewritten;
      LumaParams params;
      uint32_t parameter_count = 0;
      std::vector<D3D12_STATIC_SAMPLER_DESC1> static_samplers;
      const Skip skip = Rewrite({static_cast<const std::byte*>(blob), size}, hook.settings_register, hook.data_register, &rewritten, &params, &parameter_count, &static_samplers);
      const bool injecting = !rewritten.empty(); // The CBVs, the upgraded static samplers or both
#if DEVELOPMENT
      pending_static_samplers = std::move(static_samplers);
#endif
      if (skip != Skip::None && skip_logs[static_cast<size_t>(skip)].fetch_add(1) < MAX_LOGS)
      {
         hook.log(std::format("A root signature{} is left without Luma's constant buffers: {}", (parameter_count != 0 ? std::format(" (root parameters: {})", parameter_count) : std::string()), SKIP_NAMES[static_cast<size_t>(skip)]).c_str());
      }
      const bool were_static_samplers_upgraded = std::exchange(static_samplers_upgraded, injecting);
      HRESULT result = hook.original(device, node_mask, (injecting ? rewritten.data() : blob), (injecting ? rewritten.size() : size), riid, root_signature);
      static_samplers_upgraded = were_static_samplers_upgraded;
      if (SUCCEEDED(result))
      {
         const std::unique_lock lock(injected.mutex);
         injected.by_root_signature[reinterpret_cast<uint64_t>(*root_signature)] = (skip == Skip::None ? params : LumaParams{.skip = skip});
         if (injecting)
         {
            injected.original_parts.emplace(RootSignaturePart(blob, size));
         }
      }
      else if (injecting)
      {
         // The game gets its root signature as it made it
         const HRESULT rewritten_result = result;
         result = hook.original(device, node_mask, blob, size, riid, root_signature);
         if (create_failure_logs.fetch_add(1) < MAX_LOGS)
         {
            hook.log(std::format("Creating a root signature (root parameters: {}) as Luma rewrote it failed with {:#010x}, retried as the game made it, which returned {:#010x}", parameter_count, static_cast<uint32_t>(rewritten_result), static_cast<uint32_t>(result)).c_str());
         }
      }
      rewriting = was_rewriting;
#if DEVELOPMENT
      pending_static_samplers.clear();
#endif
      return result;
   }

   // True if the game's root signatures get the CBVs: the function is hooked, by now or for an earlier device (the registers and log of the
   // first install stay). False if there is no ReShade device proxy, the runtime lacks the (de)serializer or MinHook failed. A later
   // device's proxy can lead elsewhere when another module patched the proxies' vtable after the first install, to a function that calls
   // ReShade's: the hook on that covers it.
   // "settings_register" and "data_register" (-1: no LumaData) are the CBVs' registers in "LUMA_CB_REGISTER_SPACE".
   inline bool Install(ID3D12Device* native_device, uint32_t settings_register, int32_t data_register, LogFunction log)
   {
      void** const proxy_vtable = D3D12ProxyHook::FindProxyVtable(native_device);
      if (!proxy_vtable)
      {
         return false;
      }
      const std::unique_lock lock(hook.mutex);
      if (hook.target)
      {
         return true;
      }
      const HMODULE d3d12 = GetModuleHandleW(L"d3d12.dll");
      if (!d3d12)
      {
         return false;
      }
      hook.create_deserializer = reinterpret_cast<PFN_D3D12_CREATE_VERSIONED_ROOT_SIGNATURE_DESERIALIZER>(GetProcAddress(d3d12, "D3D12CreateVersionedRootSignatureDeserializer"));
      hook.serialize = reinterpret_cast<PFN_D3D12_SERIALIZE_VERSIONED_ROOT_SIGNATURE>(GetProcAddress(d3d12, "D3D12SerializeVersionedRootSignature"));
      hook.settings_register = settings_register;
      hook.data_register = data_register;
      hook.log = log;
      return hook.create_deserializer && hook.serialize && D3D12ProxyHook::Attach(&hook, proxy_vtable[CREATE_ROOT_SIGNATURE_SLOT], &Detour);
   }

   // See "D3D12ProxyHook::Detach()"
   inline bool Remove()
   {
      const std::unique_lock lock(hook.mutex);
      return D3D12ProxyHook::Detach(&hook);
   }

   // The CBVs of a root signature ("ID3D12RootSignature" pointer), all -1 if the hook didn't add any (with the reason, if it saw it)
   inline LumaParams Find(uint64_t root_signature)
   {
      const std::shared_lock lock(injected.mutex);
      const auto found = injected.by_root_signature.find(root_signature);
      return (found != injected.by_root_signature.end() ? found->second : LumaParams());
   }

   // For a destroyed root signature. A deduplicated one (the runtime returns the same object for equal blobs) is destroyed more than once.
   inline void Forget(uint64_t root_signature)
   {
      const std::unique_lock lock(injected.mutex);
      injected.by_root_signature.erase(root_signature);
   }

   // Whether the root signature in a container (a serialized one, or a shader with one embedded) is the original of one the hook added
   // CBVs to: a pipeline using the embedded one would mismatch the rewritten one the game binds
   inline bool IsInjectedOriginal(const void* container, size_t size)
   {
      const std::string_view part = RootSignaturePart(container, size);
      if (part.empty())
      {
         return false;
      }
      const std::shared_lock lock(injected.mutex);
      return injected.original_parts.contains(std::string(part));
   }
} // namespace D3D12RootSignatures

#endif // DX12 && ENABLE_MINHOOK
