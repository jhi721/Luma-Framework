#pragma once

// An in-process D3D12 pass device for a native D3D9 (x86) add-on, separate from its D3D12 presenter (own thread and lifetime). The passes
// are a GPU round trip inside the D3D9 frame: D3D9 copies its input into a shared KMT texture and signals the D3D9 KMT fence "in"; this
// queue waits for it, runs the passes (ping-ponging between that texture and a second shared one) and signals "out"; D3D9 waits for "out"
// and copies the result back. Nothing here waits on the CPU. Recording, descriptors and PSOs come from D3D12Passes (d3d12_passes.h), the
// LumaSettings layout from cbuffers.h.
//
// The includer defines its game's CB structs (CB::LumaGameSettings and friends, e.g. Shaders/<Game>/Includes/GameCBuffers.hlsl) before
// including this header, as cbuffers.h needs them. The shader bytecode comes from the caller ("Shaders", only read during "Open()"): the
// pixel and compute shaders of Luma_RCAS_PS.hlsl, the game's display map, Luma_SMAA_impl.hlsl (1x and SMAA_T2X), DepthEdges, Luma_TAA_CS.hlsl
// (TAA_QUALITY 2) and XeGTAO (XE_GTAO_QUALITY 3, the final denoise writing R32F).
//
// Threading: one thread (the game's), which serializes every call.
// ponytail: a second D3D12 device next to the presenter's; share one if memory or queue contention shows up.

#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <memory>
#include <optional>
#include <span>
#include <tuple>
#include <vector>

#include "../includes/cbuffers.h"
#include "../includes/d3d12_passes.h"
#include "../texture_data/SMAA_AreaTex.h"
#include "../texture_data/SMAA_AreaTex_SharpU.h"
#include "../texture_data/SMAA_SearchTex.h"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

namespace D3D9Passes
{
   using Microsoft::WRL::ComPtr;

   // DXBC of every pass, read by "Open()" only
   struct Shaders
   {
      std::span<const uint8_t> compose_vs;     // the fullscreen triangle
      std::span<const uint8_t> display_map_ps; // b13 = LumaSettings
      std::span<const uint8_t> rcas_ps;        // b0 = width, height, sharpness, 0
      std::span<const uint8_t> smaa_edge_detection_vs, smaa_edge_detection_ps;
      std::span<const uint8_t> smaa_blending_weight_calculation_vs, smaa_blending_weight_calculation_ps;
      std::span<const uint8_t> smaa_neighborhood_blending_vs, smaa_neighborhood_blending_ps;
      // SMAA_T2X 1
      std::span<const uint8_t> smaa_t2x_blending_weight_calculation_ps, smaa_t2x_neighborhood_blending_ps, smaa_t2x_resolve_ps;
      std::span<const uint8_t> depth_edges_cs;
      std::span<const uint8_t> luma_taa_cs;
      int luma_taa_quality = 2; // TAA_QUALITY of "luma_taa_cs": its history is 2x2 the render size from High (TAA_HISTORY_2X)
      std::span<const uint8_t> gtao_prefilter_cs, gtao_main_cs, gtao_denoise_cs, gtao_denoise_final_cs;
      // Optional (empty = that GtaoParams option fails the run): GT-VBAO's main pass ("gtvbao_main_pass_cs"), its 1 slice variant
      // under the history (GTVBAO_SLICE_COUNT=1.0) and the AO history ("temporal_pass_cs")
      std::span<const uint8_t> gtvbao_main_cs, gtvbao_temporal_main_cs, gtao_temporal_cs;
   };

   // "Run()" passes over the canvas, in order (each skipped when off).
   struct Params
   {
      bool display_map = false;  // Shaders/Bayonetta/Includes/DisplayMap.hlsl
      uint32_t display_mode = 1; // LumaSettings.DisplayMode: 0 SDR, 1 HDR
      float peak_white_nits = 1000, game_paper_white_nits = 203, ui_paper_white_nits = 203;
      float contrast = 1, saturation = 1, highlights_desaturation = 0;
      bool smaa = false; // Bayonetta's Luma_SMAA_impl.hlsl, 1x
      // SMAA predication from the shared depth ("SetDepth()"): the depth prepass'
      // edges (Luma_Bayonetta_DepthEdges.hlsl). scene_origin = the scene copy's
      // viewport origin on the canvas, the depth being the scene rect's size.
      bool predication = false;
      float predication_tolerance = 0.02f; // Bayonetta's g_smaa_pred_tolerance
      float scene_origin[2] = {};
      // SMAA T2x (Bayonetta's DrawSMAAOnCanvas) on the shared motion vectors
      // ("SetMotion()", canvas sized): SMAA into frame `t2x_current` (0/1) with the
      // velocity, then the resolve with the other frame (t2x_history) or the same
      // one into the canvas, RGB only. t2x_phase = the scene's jitter phase, -1 =
      // not jittered (subsample indices 0).
      bool t2x = false, t2x_history = false;
      int t2x_current = 0, t2x_phase = -1;
      float rcas_sharpness = 0; // 0 = off
   };

   // Luma TAA at 1x, High quality (Shaders/Global/Luma_TAA_CS.hlsl, Core's
   // LumaTAA::TAA::Draw on this device) over the MV scene's shared textures.
   struct TaaParams
   {
      float jitter[2] = {}; // the projection's offset, y down pixels (SR "jitter")
      float near_plane = 0.1f, far_plane = 1000;
      bool reset = false;
   };

   // Bayonetta's XeGTAO (Shaders/Bayonetta/Luma_Bayonetta_XeGTAO.hlsl, the
   // dgVoodoo mod's RunXeGTAO) from the shared depth ("SetDepth()": the prepass'
   // "1 - z/w") into the shared R32F `output` of its size. `camera` = rows 0..27
   // of the shader's b4: the prepass VS's affine view (3 rows) at `affine_row`
   // and its view projection (4 rows) at `matrix_row`. The defaults run XeGTAO
   // alone, without the history.
   struct GtaoParams
   {
      float camera[28 * 4] = {};
      uint32_t affine_row = 0, matrix_row = 0;
      float final_value_power = 0.55f, depth_scale = 1, radius_override = 0;
      bool gtvbao = false; // GT-VBAO's main pass in place of XeGTAO's (its own final_value_power, the dgVoodoo mod's 0.35)
      // CB::GTAOKnobs: the shaders' DEVELOPMENT debug views (1 depth gradient, 2 normals, 3 AO x8, 4 edges), shown alone by the apply
      // PS's GTAO_DEBUG_VIEW variant; the noise layer (frame % 64 with the history, else 0: a moving pattern boils without one)
      float debug_view = 0, noise_index = 0;
      // The AO history ("temporal_pass_cs", Includes/XeGTAOTemporal.hlsl) between the main pass and the denoise, 0 = off. It reads
      // this frame's motion vectors from the shared motion texture ("SetMotion()", the AO's size): object motion where a patched
      // draw wrote one, MOTION_VECTOR_MARKER elsewhere (the shader gives those the camera motion). `reprojection` = current clip
      // space to the previous frame's (column vectors), its rows; `jitter_ndc` = this frame's projection jitter. `history_reset`
      // restarts it (a cut, or no run last frame); a run without it, or a switch between XeGTAO and GT-VBAO, restarts it too.
      float history_weight = 0;
      bool history_reset = false;
      float reprojection[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
      float jitter_ndc[2] = {};
   };

   // A fullscreen pass: root constants at "constants_register", one SRV at t0,
   // its pixel shader after the composition's fullscreen triangle VS.
   struct Pipeline
   {
      com_ptr<ID3D12RootSignature> root;
      com_ptr<ID3D12PipelineState> pso;
      UINT constants = 0;
   };

   // Bayonetta's SMAA ULTRA (Shaders/Bayonetta/Luma_SMAA_impl.hlsl through Core's
   // SMAA_Passes.hlsl, as Core's D3D12 DrawSMAA): b1 = CB::SmaaMetrics as root
   // constants, t0-t2, s0 linear / s1 point. The area texture without the U-shape
   // smoothing (Bayonetta's SMAA_SMOOTH_U_SHAPES 0).
   // ponytail: no stencil mask of the edges (the weights run on every pixel, with
   // the same result), as Core's D3D12 DrawSMAA.
   struct Smaa
   {
      com_ptr<ID3D12RootSignature> root;
      com_ptr<ID3D12PipelineState> edge_detection, blending_weight, neighborhood_blending;
      // Uploaded by the first run that draws SMAA; pixel shader resources since
      ComPtr<ID3D12Resource> area, search, area_upload, search_upload;
      // Canvas sized, pixel shader resources outside a run (RTV 2 and 3); the
      // predication signal (R16F, UAV) too
      ComPtr<ID3D12Resource> edges, weights, predication;
      // Predication: Luma_Bayonetta_DepthEdges.hlsl over the shared R32F prepass
      // copy (scene rect sized, COMMON outside a run)
      com_ptr<ID3D12RootSignature> depth_edges_root;
      com_ptr<ID3D12PipelineState> depth_edges;
      ComPtr<ID3D12Resource> depth;
      // T2x: its weights / blend (SMAA_T2X 1) and the resolve (RGB only); the two
      // canvas sized linear frames (velocity length in alpha; RTV 4 and 5, pixel
      // shader resources outside a run, made by the first T2x run); the shared
      // motion vectors (COMMON outside a run)
      com_ptr<ID3D12PipelineState> t2x_blending_weight, t2x_neighborhood_blending, t2x_resolve;
      ComPtr<ID3D12Resource> t2x_frames[2], motion;
   };

   // Luma TAA as Core's LumaTAA::TAA (D3D12): b0 = TaaConstants as root
   // constants, one table t0-t7 + u0-u3, s0 linear clamp. The shared inputs and
   // output are COMMON outside a run; the UAV `result` (copied into the shared
   // output, which D3D9 made without unordered access) stays UNORDERED_ACCESS,
   // the histories and locks NON_PIXEL_SHADER_RESOURCE.
   // ponytail: mirrors LumaTAA::TAA (Source/Core/taa/LumaTAA.hpp) on a device of its own; share its API-neutral part (constants, view
   // formats, history layout) once that file is split from its SR::SuperResolutionImpl.
   struct Taa
   {
      com_ptr<ID3D12RootSignature> root;
      com_ptr<ID3D12PipelineState> pipeline;
      ComPtr<ID3D12Resource> color, depth, motion, output, result;
      ComPtr<ID3D12Resource> history[2], locks[2];
      UINT history_scale = 2; // 2x2 history from High (LumaTAA::history_2x_quality), the render size below
      uint32_t history_index = 0;
      bool history_valid = false;
   };
   // Mirrors LumaTAAData (Luma_TAA_CS.hlsl) / LumaTAA::CBData
   struct TaaConstants
   {
      float render_resolution[2], inv_render_resolution[2];
      float motion_vector_scale[2], depth_near_far[2];
      uint32_t flags;
      float jitter[2]; // sample offset from the pixel centre: minus the SR jitter
      float padding;
      float output_resolution[2], inv_output_resolution[2];
   };
   static_assert(sizeof(TaaConstants) == 64);
   constexpr uint32_t TAA_FLAG_RESET = 1;
   constexpr UINT TAA_SRVS = 8, TAA_UAVS = 4;

   // XeGTAO: b4 = the camera rows (root CBV into `camera`, a slot per
   // recording), b9 = CB::GTAOKnobs and b10 = CB::GTAOTemporalConstants as root
   // constants, one SRV table (t0-t3) and one UAV table (u0-u4), s0 point clamp.
   // The depth mips, the working AO and the history are NON_PIXEL_SHADER_RESOURCE
   // outside a run, `result` (copied into the shared output) UNORDERED_ACCESS;
   // the inputs are the SMAA predication's shared depth and T2x's shared motion.
   struct Gtao
   {
      com_ptr<ID3D12RootSignature> root;
      com_ptr<ID3D12PipelineState> prefilter, main, denoise, denoise_final;
      com_ptr<ID3D12PipelineState> gtvbao_main, gtvbao_temporal_main, temporal; // null without their shaders
      ComPtr<ID3D12Resource> camera;                                            // upload, D3D12Passes::MAX_FRAMES slots
      void* camera_mapped = nullptr;
      uint32_t camera_slot = 0;
      ComPtr<ID3D12Resource> mips, working[2], result, output;
      // The AO history, R16G16_FLOAT (AO, view Z), ping-ponged, made by the first
      // run with it; `history_index` = the one written last
      ComPtr<ID3D12Resource> history[2];
      uint32_t history_index = 0;
      bool history_valid = false, history_gtvbao = false;
   };
   constexpr UINT GTAO_DEPTH_MIPS = 5, GTAO_CAMERA_SLOT_BYTES = 512, GTAO_SRVS = 4;

   struct PassDevice
   {
      ComPtr<ID3D12Device> device;
      ComPtr<ID3D12CommandQueue> queue;
      ComPtr<ID3D12Fence> fence_in, fence_out; // D3D9 KMT fences
      D3D12Passes::Context context;
      Pipeline display_map, rcas;
      Smaa smaa;
      Taa taa;
      Gtao gtao;
      ComPtr<ID3D12Resource> textures[2]; // shared KMT input, output
      // RTVs: the two textures, SMAA's edges and weights, T2x's frames. CPU
      // descriptors, read when recorded
      ComPtr<ID3D12DescriptorHeap> rtv_heap;
      UINT rtv_increment = 0;
   };
   inline PassDevice g_pass;
   inline char g_pass_error[160] = "";

   inline bool Fail(const char* what, HRESULT hr)
   {
      snprintf(g_pass_error, sizeof(g_pass_error), "%s 0x%08lX", what, hr);
      return false;
   }

   inline bool CreatePipeline(Pipeline* pipeline, std::span<const uint8_t> vs, std::span<const uint8_t> ps, UINT constants_register, UINT constants)
   {
      if (vs.empty() || ps.empty())
      {
         return Fail("missing compose_vs.cso or a pass shader", E_FAIL);
      }
      const D3D12_DESCRIPTOR_RANGE1 range = {
         .RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
         .NumDescriptors = 1,
         .Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE};
      const D3D12_ROOT_PARAMETER1 parameters[] = {
         {.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS,
            .Constants = {.ShaderRegister = constants_register, .Num32BitValues = constants},
            .ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL},
         {.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
            .DescriptorTable = {.NumDescriptorRanges = 1, .pDescriptorRanges = &range},
            .ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL},
      };
      pipeline->constants = constants;
      pipeline->root = D3D12Passes::CreateRootSignature(g_pass.device.Get(),
         {.Version = D3D_ROOT_SIGNATURE_VERSION_1_1,
            .Desc_1_1 = {.NumParameters = UINT(std::size(parameters)), .pParameters = parameters}});
      pipeline->pso = D3D12Passes::CreateFullscreenPipeline(g_pass.device.Get(), pipeline->root.get(), {vs.data(), vs.size()},
         {ps.data(), ps.size()}, DXGI_FORMAT_R16G16B16A16_FLOAT);
      return pipeline->pso ? true : Fail("pass root signature / pipeline", E_FAIL);
   }

   inline D3D12_CPU_DESCRIPTOR_HANDLE Rtv(UINT index)
   {
      return {g_pass.rtv_heap->GetCPUDescriptorHandleForHeapStart().ptr + index * g_pass.rtv_increment};
   }

   inline ComPtr<ID3D12Resource> CreateTexture(D3D12_HEAP_TYPE heap, UINT64 width, UINT height, DXGI_FORMAT format,
      D3D12_RESOURCE_STATES state, D3D12_RESOURCE_FLAGS flags)
   {
      const D3D12_HEAP_PROPERTIES properties = {.Type = heap};
      const D3D12_RESOURCE_DESC desc =
         heap == D3D12_HEAP_TYPE_UPLOAD
            ? D3D12_RESOURCE_DESC{.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
                 .Width = width,
                 .Height = 1,
                 .DepthOrArraySize = 1,
                 .MipLevels = 1,
                 .SampleDesc = {.Count = 1},
                 .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR}
            : D3D12_RESOURCE_DESC{.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
                 .Width = width,
                 .Height = height,
                 .DepthOrArraySize = 1,
                 .MipLevels = 1,
                 .Format = format,
                 .SampleDesc = {.Count = 1},
                 .Flags = flags};
      ComPtr<ID3D12Resource> resource;
      g_pass.device->CreateCommittedResource(&properties, D3D12_HEAP_FLAG_NONE, &desc, state, nullptr, IID_PPV_ARGS(&resource));
      return resource;
   }

   // A default heap texture and the upload buffer holding `bytes` in its
   // footprint, copied by the first SMAA run.
   inline bool CreateUploadedTexture(UINT width, UINT height, DXGI_FORMAT format, const unsigned char* bytes, UINT pitch,
      ComPtr<ID3D12Resource>* texture, ComPtr<ID3D12Resource>* upload)
   {
      *texture = CreateTexture(D3D12_HEAP_TYPE_DEFAULT, width, height, format, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_FLAG_NONE);
      if (!*texture)
      {
         return false;
      }
      const D3D12_RESOURCE_DESC desc = (*texture)->GetDesc();
      D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint;
      UINT64 size = 0;
      g_pass.device->GetCopyableFootprints(&desc, 0, 1, 0, &footprint, nullptr, nullptr, &size);
      *upload = CreateTexture(D3D12_HEAP_TYPE_UPLOAD, size, 1, DXGI_FORMAT_UNKNOWN, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_FLAG_NONE);
      void* mapped = nullptr;
      if (!*upload || FAILED((*upload)->Map(0, nullptr, &mapped)))
      {
         return false;
      }
      for (UINT row = 0; row < height; ++row)
      {
         memcpy(static_cast<char*>(mapped) + footprint.Offset + row * footprint.Footprint.RowPitch, bytes + row * pitch, pitch);
      }
      (*upload)->Unmap(0, nullptr);
      return true;
   }

   inline bool CreateSmaa(const Shaders& shaders)
   {
      Smaa& smaa = g_pass.smaa;
      const D3D12_DESCRIPTOR_RANGE1 range = {
         .RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
         .NumDescriptors = 3,
         .Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE | D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE};
      const D3D12_ROOT_PARAMETER1 parameters[] = {
         {.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS,
            .Constants = {.ShaderRegister = 1, .Num32BitValues = sizeof(CB::SmaaMetrics) / 4},
            .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL},
         {.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
            .DescriptorTable = {.NumDescriptorRanges = 1, .pDescriptorRanges = &range},
            .ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL},
      };
      D3D12_STATIC_SAMPLER_DESC samplers[2] = {};
      for (UINT i = 0; i < 2; ++i)
      {
         samplers[i] = {.Filter = i == 0 ? D3D12_FILTER_MIN_MAG_MIP_LINEAR : D3D12_FILTER_MIN_MAG_MIP_POINT,
            .AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
            .AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
            .AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
            .MaxLOD = D3D12_FLOAT32_MAX,
            .ShaderRegister = i,
            .ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL};
      }
      smaa.root = D3D12Passes::CreateRootSignature(g_pass.device.Get(),
         {.Version = D3D_ROOT_SIGNATURE_VERSION_1_1,
            .Desc_1_1 = {.NumParameters = UINT(std::size(parameters)),
               .pParameters = parameters,
               .NumStaticSamplers = UINT(std::size(samplers)),
               .pStaticSamplers = samplers}});
      const auto create = [&](std::span<const uint8_t> vs, std::span<const uint8_t> ps, DXGI_FORMAT format,
                             const D3D12_RENDER_TARGET_BLEND_DESC& blend = D3D12Passes::OPAQUE_BLEND)
      {
         return (vs.empty() || ps.empty() || !smaa.root)
                   ? com_ptr<ID3D12PipelineState>()
                   : D3D12Passes::CreateFullscreenPipeline(g_pass.device.Get(), smaa.root.get(), {vs.data(), vs.size()}, {ps.data(), ps.size()}, format, blend);
      };
      smaa.edge_detection = create(shaders.smaa_edge_detection_vs, shaders.smaa_edge_detection_ps, DXGI_FORMAT_R8G8_UNORM);
      smaa.blending_weight = create(shaders.smaa_blending_weight_calculation_vs, shaders.smaa_blending_weight_calculation_ps, DXGI_FORMAT_R8G8B8A8_UNORM);
      smaa.neighborhood_blending = create(shaders.smaa_neighborhood_blending_vs, shaders.smaa_neighborhood_blending_ps, DXGI_FORMAT_R16G16B16A16_FLOAT);
      smaa.t2x_blending_weight = create(shaders.smaa_blending_weight_calculation_vs, shaders.smaa_t2x_blending_weight_calculation_ps, DXGI_FORMAT_R8G8B8A8_UNORM);
      smaa.t2x_neighborhood_blending = create(shaders.smaa_neighborhood_blending_vs, shaders.smaa_t2x_neighborhood_blending_ps, DXGI_FORMAT_R16G16B16A16_FLOAT);
      // The resolve reads SV_Position only; the canvas alpha stays the scene's
      D3D12_RENDER_TARGET_BLEND_DESC rgb_only = D3D12Passes::OPAQUE_BLEND;
      rgb_only.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_RED | D3D12_COLOR_WRITE_ENABLE_GREEN | D3D12_COLOR_WRITE_ENABLE_BLUE;
      smaa.t2x_resolve = create(shaders.smaa_neighborhood_blending_vs, shaders.smaa_t2x_resolve_ps, DXGI_FORMAT_R16G16B16A16_FLOAT, rgb_only);
      // Predication: b0 = CB::DepthEdgesConstants, t0 depth, u0 the signal
      const D3D12_DESCRIPTOR_RANGE1 depth_edges_ranges[] = {
         {.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
            .NumDescriptors = 1,
            .Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE},
         {.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV,
            .NumDescriptors = 1,
            .Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE,
            .OffsetInDescriptorsFromTableStart = 1}};
      const D3D12_ROOT_PARAMETER1 depth_edges_parameters[] = {
         {.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS,
            .Constants = {.Num32BitValues = sizeof(CB::DepthEdgesConstants) / 4}},
         {.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
            .DescriptorTable = {.NumDescriptorRanges = 2, .pDescriptorRanges = depth_edges_ranges}},
      };
      smaa.depth_edges_root = D3D12Passes::CreateRootSignature(g_pass.device.Get(),
         {.Version = D3D_ROOT_SIGNATURE_VERSION_1_1,
            .Desc_1_1 = {.NumParameters = UINT(std::size(depth_edges_parameters)), .pParameters = depth_edges_parameters}});
      if (const std::span<const uint8_t> cs = shaders.depth_edges_cs; !cs.empty() && smaa.depth_edges_root)
      {
         smaa.depth_edges = D3D12Passes::CreateComputePipeline(g_pass.device.Get(), smaa.depth_edges_root.get(), {cs.data(), cs.size()});
      }
      std::vector<unsigned char> area(std::begin(areaTexBytes), std::end(areaTexBytes));
      for (const auto& [offset, value] : areaTexSharpUPatch)
      {
         area[offset] = value;
      }
      if (!smaa.edge_detection || !smaa.blending_weight || !smaa.neighborhood_blending || !smaa.depth_edges || !smaa.t2x_blending_weight ||
          !smaa.t2x_neighborhood_blending || !smaa.t2x_resolve ||
          !CreateUploadedTexture(AREATEX_WIDTH, AREATEX_HEIGHT, DXGI_FORMAT_R8G8_UNORM, area.data(), AREATEX_PITCH, &smaa.area, &smaa.area_upload) ||
          !CreateUploadedTexture(SEARCHTEX_WIDTH, SEARCHTEX_HEIGHT, DXGI_FORMAT_R8_UNORM, searchTexBytes, SEARCHTEX_PITCH, &smaa.search, &smaa.search_upload))
      {
         return Fail("SMAA shaders / pipelines / area or search texture", E_FAIL);
      }
      return true;
   }

   inline bool CreateTaa(std::span<const uint8_t> cs, int quality)
   {
      Taa& taa = g_pass.taa;
      taa.history_scale = (quality >= 2 ? 2 : 1);
      const D3D12_DESCRIPTOR_RANGE1 ranges[] = {
         {.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
            .NumDescriptors = TAA_SRVS,
            .Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE | D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE},
         {.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV,
            .NumDescriptors = TAA_UAVS,
            .Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE | D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE,
            .OffsetInDescriptorsFromTableStart = TAA_SRVS}};
      const D3D12_ROOT_PARAMETER1 parameters[] = {
         {.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS,
            .Constants = {.Num32BitValues = sizeof(TaaConstants) / 4}},
         {.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
            .DescriptorTable = {.NumDescriptorRanges = UINT(std::size(ranges)), .pDescriptorRanges = ranges}},
      };
      const D3D12_STATIC_SAMPLER_DESC sampler = {
         .Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR,
         .AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
         .AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
         .AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
         .MaxAnisotropy = 1,
         .ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER,
         .MaxLOD = D3D12_FLOAT32_MAX};
      taa.root = D3D12Passes::CreateRootSignature(g_pass.device.Get(),
         {.Version = D3D_ROOT_SIGNATURE_VERSION_1_1,
            .Desc_1_1 = {.NumParameters = UINT(std::size(parameters)),
               .pParameters = parameters,
               .NumStaticSamplers = 1,
               .pStaticSamplers = &sampler}});
      if (!cs.empty() && taa.root)
      {
         taa.pipeline = D3D12Passes::CreateComputePipeline(g_pass.device.Get(), taa.root.get(), {cs.data(), cs.size()});
      }
      return taa.pipeline ? true : Fail("Luma TAA pipeline", E_FAIL);
   }

   inline bool CreateGtao(const Shaders& shaders)
   {
      Gtao& gtao = g_pass.gtao;
      constexpr D3D12_DESCRIPTOR_RANGE_FLAGS volatile_range = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE | D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;
      const D3D12_DESCRIPTOR_RANGE1 srv_range = {
         .RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
         .NumDescriptors = GTAO_SRVS,
         .Flags = volatile_range};
      const D3D12_DESCRIPTOR_RANGE1 uav_range = {
         .RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV,
         .NumDescriptors = GTAO_DEPTH_MIPS,
         .Flags = volatile_range};
      const D3D12_ROOT_PARAMETER1 parameters[] = {
         {.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV,
            .Descriptor = {.ShaderRegister = 4, .Flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE}},
         {.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS,
            .Constants = {.ShaderRegister = 9, .Num32BitValues = sizeof(CB::GTAOKnobs) / 4}},
         {.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
            .DescriptorTable = {.NumDescriptorRanges = 1, .pDescriptorRanges = &srv_range}},
         {.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
            .DescriptorTable = {.NumDescriptorRanges = 1, .pDescriptorRanges = &uav_range}},
         {.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS,
            .Constants = {.ShaderRegister = 10, .Num32BitValues = sizeof(CB::GTAOTemporalConstants) / 4}},
      };
      const D3D12_STATIC_SAMPLER_DESC sampler = {
         .Filter = D3D12_FILTER_MIN_MAG_MIP_POINT,
         .AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
         .AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
         .AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
         .MaxAnisotropy = 1,
         .ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER,
         .MaxLOD = D3D12_FLOAT32_MAX};
      gtao.root = D3D12Passes::CreateRootSignature(g_pass.device.Get(),
         {.Version = D3D_ROOT_SIGNATURE_VERSION_1_1,
            .Desc_1_1 = {.NumParameters = UINT(std::size(parameters)),
               .pParameters = parameters,
               .NumStaticSamplers = 1,
               .pStaticSamplers = &sampler}});
      // The optional ones fail only when given
      struct Pipeline
      {
         std::span<const uint8_t> cs;
         com_ptr<ID3D12PipelineState>* pipeline = nullptr;
         bool required = false;
      };
      const Pipeline pipelines[] = {
         {.cs = shaders.gtao_prefilter_cs, .pipeline = std::addressof(gtao.prefilter), .required = true},
         {.cs = shaders.gtao_main_cs, .pipeline = std::addressof(gtao.main), .required = true},
         {.cs = shaders.gtao_denoise_cs, .pipeline = std::addressof(gtao.denoise), .required = true},
         {.cs = shaders.gtao_denoise_final_cs, .pipeline = std::addressof(gtao.denoise_final), .required = true},
         {.cs = shaders.gtvbao_main_cs, .pipeline = std::addressof(gtao.gtvbao_main)},
         {.cs = shaders.gtvbao_temporal_main_cs, .pipeline = std::addressof(gtao.gtvbao_temporal_main)},
         {.cs = shaders.gtao_temporal_cs, .pipeline = std::addressof(gtao.temporal)}};
      for (const auto& [cs, pipeline, required] : pipelines)
      {
         if (!cs.empty() && gtao.root)
         {
            *pipeline = D3D12Passes::CreateComputePipeline(g_pass.device.Get(), gtao.root.get(), {cs.data(), cs.size()});
         }
         if (!*pipeline && (required || !cs.empty()))
         {
            return Fail("XeGTAO pipelines", E_FAIL);
         }
      }
      gtao.camera = CreateTexture(D3D12_HEAP_TYPE_UPLOAD, D3D12Passes::MAX_FRAMES * GTAO_CAMERA_SLOT_BYTES, 1, DXGI_FORMAT_UNKNOWN,
         D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_FLAG_NONE);
      if (!gtao.camera || FAILED(gtao.camera->Map(0, nullptr, &gtao.camera_mapped)))
      {
         return Fail("XeGTAO camera buffer", E_OUTOFMEMORY);
      }
      return true;
   }

   // SMAA of texture `source` into the other one: the predication signal (when
   // asked and the shared depth is set), edge detection (gamma canvas; t1 the
   // signal, scale 2, or null, scale 1), blending weights, the neighborhood blend
   // (filtered in linear light). T2x: the phase's weights, the blend into this
   // frame's linear target with the velocity, then the resolve with the history
   // into the other texture. SRVs from *srv_index (T2x: 12, predication +2).
   inline void DrawSmaa(const D3D12Passes::Recording& recording, const Params& params, UINT* srv_index, int source)
   {
      Smaa& smaa = g_pass.smaa;
      ID3D12GraphicsCommandList* const list = recording.list;
      ID3D12Resource* const input = g_pass.textures[source].Get();
      ID3D12Resource* const output = g_pass.textures[1 - source].Get();
      const D3D12_RESOURCE_DESC desc = input->GetDesc();
      if (smaa.area_upload)
      {
         const D3D12_RESOURCE_DESC area_desc = smaa.area->GetDesc(), search_desc = smaa.search->GetDesc();
         for (const auto& [texture, upload, texture_desc] :
            {std::tuple{smaa.area.Get(), smaa.area_upload.Get(), area_desc},
               std::tuple{smaa.search.Get(), smaa.search_upload.Get(), search_desc}})
         {
            D3D12_TEXTURE_COPY_LOCATION destination = {
               .pResource = texture,
               .Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX};
            D3D12_TEXTURE_COPY_LOCATION origin = {
               .pResource = upload,
               .Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT};
            g_pass.device->GetCopyableFootprints(&texture_desc, 0, 1, 0, &origin.PlacedFootprint, nullptr, nullptr, nullptr);
            list->CopyTextureRegion(&destination, 0, 0, 0, &origin, nullptr);
            D3D12Passes::Transition(list, texture, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
         }
         D3D12Passes::Retire(&g_pass.context, smaa.area_upload.Get());
         D3D12Passes::Retire(&g_pass.context, smaa.search_upload.Get());
         smaa.area_upload.Reset();
         smaa.search_upload.Reset();
      }
      const bool predication = params.predication && smaa.depth;
      if (predication)
      {
         const CB::DepthEdgesConstants constants = {
            .tolerance = params.predication_tolerance,
            .scene_origin = {params.scene_origin[0], params.scene_origin[1]}};
         const D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {
            .Format = DXGI_FORMAT_R32_FLOAT,
            .ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
            .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
            .Texture2D = {.MipLevels = 1}};
         const D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc = {
            .Format = DXGI_FORMAT_R16_FLOAT,
            .ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D};
         g_pass.device->CreateShaderResourceView(smaa.depth.Get(), &srv_desc, {recording.srv_cpu.ptr + *srv_index * recording.srv_increment});
         g_pass.device->CreateUnorderedAccessView(smaa.predication.Get(), nullptr, &uav_desc,
            {recording.srv_cpu.ptr + (*srv_index + 1) * recording.srv_increment});
         D3D12Passes::Transition(list, smaa.depth.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
         D3D12Passes::Transition(list, smaa.predication.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
         list->SetComputeRootSignature(smaa.depth_edges_root.get());
         list->SetPipelineState(smaa.depth_edges.get());
         list->SetComputeRoot32BitConstants(0, sizeof(constants) / 4, &constants, 0);
         list->SetComputeRootDescriptorTable(1, {recording.srv_gpu.ptr + *srv_index * recording.srv_increment});
         *srv_index += 2;
         list->Dispatch(UINT(desc.Width + 7) / 8, (desc.Height + 7) / 8, 1);
         D3D12Passes::Transition(list, smaa.depth.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COMMON);
         D3D12Passes::Transition(list, smaa.predication.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
      }
      bool t2x = params.t2x && smaa.motion && smaa.motion->GetDesc().Width == desc.Width && smaa.motion->GetDesc().Height == desc.Height;
      if (t2x && !smaa.t2x_frames[0])
      {
         for (int i = 0; i < 2; ++i)
         {
            smaa.t2x_frames[i] = CreateTexture(D3D12_HEAP_TYPE_DEFAULT, desc.Width, desc.Height, DXGI_FORMAT_R16G16B16A16_FLOAT,
               D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
            if (smaa.t2x_frames[i])
            {
               g_pass.device->CreateRenderTargetView(smaa.t2x_frames[i].Get(), nullptr, Rtv(4 + i));
            }
         }
      }
      t2x = t2x && smaa.t2x_frames[0] && smaa.t2x_frames[1];
      // T2x's subsample indices for the jitter phase, 0 for 1x
      const float subsample_indices = (t2x && params.t2x_phase >= 0) ? float(params.t2x_phase + 1) : 0.0f;
      const CB::SmaaMetrics metrics = {
         .rt_metrics = {1.0f / desc.Width, 1.0f / desc.Height, float(desc.Width), float(desc.Height)},
         // Scale and mask fall back together (Bayonetta's DrawSMAAOnCanvas)
         .predication = {predication ? 2.0f : 1.0f, 0, 0, 0},
         .subsample_indices = {subsample_indices, subsample_indices, subsample_indices, 0}};
      list->SetGraphicsRootSignature(smaa.root.get());
      list->SetGraphicsRoot32BitConstants(0, sizeof(metrics) / 4, &metrics, 0);
      list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
      const D3D12_VIEWPORT viewport = {0, 0, float(desc.Width), float(desc.Height), 0, 1};
      const D3D12_RECT scissor = {0, 0, LONG(desc.Width), LONG(desc.Height)};
      list->RSSetViewports(1, &viewport);
      list->RSSetScissorRects(1, &scissor);
      // srvs: null = a null descriptor (reads 0)
      const auto draw = [&](ID3D12PipelineState* pipeline, ID3D12Resource* const(&srvs)[3], const DXGI_FORMAT(&formats)[3],
                           D3D12_CPU_DESCRIPTOR_HANDLE rtv)
      {
         for (UINT i = 0; i < 3; ++i)
         {
            const D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {
               .Format = formats[i],
               .ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
               .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
               .Texture2D = {.MipLevels = 1}};
            g_pass.device->CreateShaderResourceView(srvs[i], &srv_desc, {recording.srv_cpu.ptr + (*srv_index + i) * recording.srv_increment});
         }
         list->SetPipelineState(pipeline);
         list->SetGraphicsRootDescriptorTable(1, {recording.srv_gpu.ptr + *srv_index * recording.srv_increment});
         *srv_index += 3;
         list->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
         list->DrawInstanced(3, 1, 0, 0);
      };
      constexpr DXGI_FORMAT color = DXGI_FORMAT_R16G16B16A16_FLOAT;
      D3D12Passes::Transition(list, input, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
      // The edge detection discards the pixels without edges, which read as none
      D3D12Passes::Transition(list, smaa.edges.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
      static constexpr float clear[4] = {};
      list->ClearRenderTargetView(Rtv(2), clear, 0, nullptr);
      draw(smaa.edge_detection.get(), {input, predication ? smaa.predication.Get() : nullptr, nullptr},
         {color, DXGI_FORMAT_R16_FLOAT, DXGI_FORMAT_R16_FLOAT}, Rtv(2));
      D3D12Passes::Transition(list, smaa.edges.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
      D3D12Passes::Transition(list, smaa.weights.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
      draw(t2x ? smaa.t2x_blending_weight.get() : smaa.blending_weight.get(), {smaa.edges.Get(), smaa.area.Get(), smaa.search.Get()},
         {DXGI_FORMAT_R8G8_UNORM, DXGI_FORMAT_R8G8_UNORM, DXGI_FORMAT_R8_UNORM}, Rtv(3));
      D3D12Passes::Transition(list, smaa.weights.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
      if (!t2x)
      {
         D3D12Passes::Transition(list, output, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET);
         draw(smaa.neighborhood_blending.get(), {input, smaa.weights.Get(), nullptr},
            {color, DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_R16G16_FLOAT}, Rtv(1 - source));
         D3D12Passes::Transition(list, input, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COMMON);
         D3D12Passes::Transition(list, output, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COMMON);
         return;
      }
      // T2x: this frame's linear SMAA with the velocity length in alpha
      ID3D12Resource* const current = smaa.t2x_frames[params.t2x_current].Get();
      ID3D12Resource* const previous = smaa.t2x_frames[params.t2x_history ? 1 - params.t2x_current : params.t2x_current].Get();
      D3D12Passes::Transition(list, smaa.motion.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
      D3D12Passes::Transition(list, current, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
      draw(smaa.t2x_neighborhood_blending.get(), {input, smaa.weights.Get(), smaa.motion.Get()},
         {color, DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_R16G16_FLOAT}, Rtv(4 + params.t2x_current));
      D3D12Passes::Transition(list, current, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
      // The resolve writes RGB over a copy of the canvas (its alpha stays)
      D3D12Passes::Transition(list, input, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_SOURCE);
      D3D12Passes::Transition(list, output, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
      list->CopyResource(output, input);
      D3D12Passes::Transition(list, output, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_RENDER_TARGET);
      draw(smaa.t2x_resolve.get(), {current, previous, smaa.motion.Get()}, {color, color, DXGI_FORMAT_R16G16_FLOAT}, Rtv(1 - source));
      D3D12Passes::Transition(list, input, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_COMMON);
      D3D12Passes::Transition(list, output, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COMMON);
      D3D12Passes::Transition(list, smaa.motion.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COMMON);
   }

   // One fullscreen draw of `pipeline` from texture `source` into the other one.
   inline void Draw(const D3D12Passes::Recording& recording, UINT srv_index, const Pipeline& pipeline, const void* constants, int source)
   {
      ID3D12Resource* const input = g_pass.textures[source].Get();
      ID3D12Resource* const output = g_pass.textures[1 - source].Get();
      const D3D12_CPU_DESCRIPTOR_HANDLE srv = {recording.srv_cpu.ptr + srv_index * recording.srv_increment};
      const D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {
         .Format = DXGI_FORMAT_R16G16B16A16_FLOAT,
         .ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
         .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
         .Texture2D = {.MipLevels = 1}};
      g_pass.device->CreateShaderResourceView(input, &srv_desc, srv);
      const D3D12_CPU_DESCRIPTOR_HANDLE rtv = Rtv(1 - source);
      ID3D12GraphicsCommandList* const list = recording.list;
      D3D12Passes::Transition(list, input, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
      D3D12Passes::Transition(list, output, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET);
      const D3D12_RESOURCE_DESC desc = output->GetDesc();
      list->SetGraphicsRootSignature(pipeline.root.get());
      list->SetPipelineState(pipeline.pso.get());
      list->SetGraphicsRoot32BitConstants(0, pipeline.constants, constants, 0);
      list->SetGraphicsRootDescriptorTable(1, {recording.srv_gpu.ptr + srv_index * recording.srv_increment});
      list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
      const D3D12_VIEWPORT viewport = {0, 0, float(desc.Width), float(desc.Height), 0, 1};
      const D3D12_RECT scissor = {0, 0, LONG(desc.Width), LONG(desc.Height)};
      list->RSSetViewports(1, &viewport);
      list->RSSetScissorRects(1, &scissor);
      list->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
      list->DrawInstanced(3, 1, 0, 0);
      // Shared textures stay in COMMON outside a draw
      D3D12Passes::Transition(list, input, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COMMON);
      D3D12Passes::Transition(list, output, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COMMON);
   }

   // The recording between the D3D9 signal `value` on "in" and the queue's on
   // "out". False = nothing submitted.
   inline bool SubmitBetween(uint64_t value)
   {
      HRESULT hr = g_pass.queue->Wait(g_pass.fence_in.Get(), value);
      if (FAILED(hr))
      {
         return Fail("queue Wait(in)", hr);
      }
      if (!D3D12Passes::Submit(&g_pass.context, g_pass.queue.Get()))
      {
         return Fail("command list failed to close", E_FAIL);
      }
      if (FAILED(hr = g_pass.queue->Signal(g_pass.fence_out.Get(), value)))
      {
         return Fail("queue Signal(out)", hr);
      }
      return true;
   }

   inline const char* Error()
   {
      return g_pass_error;
   }

   inline void Close()
   {
      D3D12Passes::Destroy(&g_pass.context);
      g_pass = {};
   }

   // Device on the D3D9 adapter (outside ReShade, see the presenter), the D3D9
   // KMT fences and the pipelines: the display map (b13 = LumaSettings) and Core's
   // Luma_RCAS_PS (b0 = width, height, sharpness, 0).
   inline bool Open(LUID luid, HANDLE fence_in, HANDLE fence_out, const Shaders& shaders)
   {
      Close();
      ComPtr<IDXGIFactory6> factory;
      ComPtr<IDXGIAdapter1> adapter;
      ComPtr<ID3D12DeviceFactory> device_factory;
      HRESULT hr = CreateDXGIFactory2(0, IID_PPV_ARGS(&factory));
      if (SUCCEEDED(hr))
      {
         hr = factory->EnumAdapterByLuid(luid, IID_PPV_ARGS(&adapter));
      }
      if (SUCCEEDED(hr))
      {
         hr = D3D12GetInterface(CLSID_D3D12DeviceFactory, IID_PPV_ARGS(&device_factory));
      }
      if (SUCCEEDED(hr))
      {
         hr = device_factory->SetFlags(D3D12_DEVICE_FACTORY_FLAG_DISALLOW_STORING_NEW_DEVICE_AS_SINGLETON);
      }
      if (SUCCEEDED(hr))
      {
         hr = device_factory->CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&g_pass.device));
      }
      if (FAILED(hr))
      {
         return Fail("device", hr);
      }
      const D3D12_COMMAND_QUEUE_DESC queue_desc = {.Type = D3D12_COMMAND_LIST_TYPE_DIRECT};
      const D3D12_DESCRIPTOR_HEAP_DESC rtv_desc = {.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV, .NumDescriptors = 6};
      if (FAILED(hr = g_pass.device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&g_pass.queue))) ||
          FAILED(hr = g_pass.device->OpenSharedHandle(fence_in, IID_PPV_ARGS(&g_pass.fence_in))) ||
          FAILED(hr = g_pass.device->OpenSharedHandle(fence_out, IID_PPV_ARGS(&g_pass.fence_out))) ||
          FAILED(hr = g_pass.device->CreateDescriptorHeap(&rtv_desc, IID_PPV_ARGS(&g_pass.rtv_heap))))
      {
         return Fail("queue / fences / RTV heap", hr);
      }
      g_pass.rtv_increment = g_pass.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
      if (!D3D12Passes::Create(&g_pass.context, g_pass.device.Get()))
      {
         return Fail("D3D12Passes::Create", E_FAIL);
      }
      return CreatePipeline(&g_pass.display_map, shaders.compose_vs, shaders.display_map_ps, 13, sizeof(CB::LumaGlobalSettingsPadded) / 4) &&
             CreatePipeline(&g_pass.rcas, shaders.compose_vs, shaders.rcas_ps, 0, 4) && CreateSmaa(shaders) && CreateTaa(shaders.luma_taa_cs, shaders.luma_taa_quality) &&
             CreateGtao(shaders);
   }

   // The shared input (D3D9 copies into it) and output; both null = drop them.
   inline bool SetTextures(HANDLE input, HANDLE output)
   {
      for (ComPtr<ID3D12Resource>* texture :
         {std::addressof(g_pass.textures[0]), std::addressof(g_pass.textures[1]), std::addressof(g_pass.smaa.edges),
            std::addressof(g_pass.smaa.weights), std::addressof(g_pass.smaa.predication), std::addressof(g_pass.smaa.t2x_frames[0]),
            std::addressof(g_pass.smaa.t2x_frames[1])})
      {
         D3D12Passes::Retire(&g_pass.context, texture->Get());
         texture->Reset();
      }
      if (!input)
      {
         return true;
      }
      const HANDLE handles[2] = {input, output};
      for (int i = 0; i < 2; ++i)
      {
         if (const HRESULT hr = g_pass.device->OpenSharedHandle(handles[i], IID_PPV_ARGS(&g_pass.textures[i])); FAILED(hr))
         {
            return Fail("OpenSharedHandle(pass texture)", hr);
         }
         g_pass.device->CreateRenderTargetView(g_pass.textures[i].Get(), nullptr, Rtv(i));
      }
      const D3D12_RESOURCE_DESC desc = g_pass.textures[0]->GetDesc();
      g_pass.smaa.edges = CreateTexture(D3D12_HEAP_TYPE_DEFAULT, desc.Width, desc.Height, DXGI_FORMAT_R8G8_UNORM,
         D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
      g_pass.smaa.weights = CreateTexture(D3D12_HEAP_TYPE_DEFAULT, desc.Width, desc.Height, DXGI_FORMAT_R8G8B8A8_UNORM,
         D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
      g_pass.smaa.predication = CreateTexture(D3D12_HEAP_TYPE_DEFAULT, desc.Width, desc.Height, DXGI_FORMAT_R16_FLOAT,
         D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
      if (!g_pass.smaa.edges || !g_pass.smaa.weights || !g_pass.smaa.predication)
      {
         return Fail("SMAA edges / weights", E_OUTOFMEMORY);
      }
      g_pass.device->CreateRenderTargetView(g_pass.smaa.edges.Get(), nullptr, Rtv(2));
      g_pass.device->CreateRenderTargetView(g_pass.smaa.weights.Get(), nullptr, Rtv(3));
      return true;
   }

   // The shared R32F copy of the depth prepass (null = drop it).
   inline bool SetDepth(HANDLE depth)
   {
      D3D12Passes::Retire(&g_pass.context, g_pass.smaa.depth.Get());
      g_pass.smaa.depth.Reset();
      if (!depth)
      {
         return true;
      }
      const HRESULT hr = g_pass.device->OpenSharedHandle(depth, IID_PPV_ARGS(&g_pass.smaa.depth));
      return SUCCEEDED(hr) || Fail("OpenSharedHandle(depth)", hr);
   }

   // The shared G16R16F motion vectors (UV previous - current; null = drop them).
   inline bool SetMotion(HANDLE motion)
   {
      D3D12Passes::Retire(&g_pass.context, g_pass.smaa.motion.Get());
      g_pass.smaa.motion.Reset();
      if (!motion)
      {
         return true;
      }
      const HRESULT hr = g_pass.device->OpenSharedHandle(motion, IID_PPV_ARGS(&g_pass.smaa.motion));
      return SUCCEEDED(hr) || Fail("OpenSharedHandle(motion)", hr);
   }

   // Runs the passes ping-ponging between the two textures, between the D3D9
   // signal `value` on "in" and the queue's on "out". *result: 0 = the input
   // holds the final image, 1 = the output. False = nothing submitted (no pass on,
   // or a failure, see "Error()"): the caller must not wait for "out".
   inline bool Run(uint64_t value, const Params& params, int* result)
   {
      if (!params.display_map && !params.smaa && !(params.rcas_sharpness > 0))
      {
         return Fail("no pass on", S_FALSE);
      }
      if (!g_pass.textures[0])
      {
         return Fail("no pass textures", E_FAIL);
      }
      const std::optional<D3D12Passes::Recording> recording = D3D12Passes::Begin(&g_pass.context);
      if (!recording)
      {
         return Fail("no free frame", E_FAIL);
      }
      int source = 0;
      UINT srv_index = 0;
      if (params.display_map)
      {
         CB::LumaGlobalSettingsPadded settings = {};
         const D3D12_RESOURCE_DESC desc = g_pass.textures[0]->GetDesc();
         settings.SwapchainSize = {float(desc.Width), float(desc.Height)};
         settings.SwapchainInvSize = {1.0f / desc.Width, 1.0f / desc.Height};
         settings.RenderSize = settings.SwapchainSize;
         settings.RenderInvSize = settings.SwapchainInvSize;
         settings.DisplayMode = DisplayModeType(params.display_mode);
         settings.ScenePeakWhite = params.peak_white_nits;
         settings.ScenePaperWhite = params.game_paper_white_nits;
         settings.UIPaperWhite = params.ui_paper_white_nits;
         settings.GameSettings.Contrast = params.contrast;
         settings.GameSettings.Saturation = params.saturation;
         settings.GameSettings.HighlightsDesaturation = params.highlights_desaturation;
         Draw(*recording, srv_index++, g_pass.display_map, &settings, source);
         source = 1 - source;
      }
      if (params.smaa)
      {
         DrawSmaa(*recording, params, &srv_index, source);
         source = 1 - source;
      }
      if (params.rcas_sharpness > 0)
      {
         const D3D12_RESOURCE_DESC desc = g_pass.textures[0]->GetDesc();
         const float constants[4] = {float(desc.Width), float(desc.Height), params.rcas_sharpness, 0};
         Draw(*recording, srv_index++, g_pass.rcas, constants, source);
         source = 1 - source;
      }
      if (!SubmitBetween(value))
      {
         return false;
      }
      *result = source;
      return true;
   }

   // Shared colour (fp16), device depth (R32F, not inverted), motion (G16R16F, UV
   // previous - current) and output (fp16), one size; null colour = drop them.
   inline bool SetTaaTextures(HANDLE color, HANDLE depth, HANDLE motion, HANDLE output)
   {
      Taa& taa = g_pass.taa;
      for (ComPtr<ID3D12Resource>* texture :
         {std::addressof(taa.color), std::addressof(taa.depth), std::addressof(taa.motion), std::addressof(taa.output),
            std::addressof(taa.result), std::addressof(taa.history[0]), std::addressof(taa.history[1]), std::addressof(taa.locks[0]),
            std::addressof(taa.locks[1])})
      {
         D3D12Passes::Retire(&g_pass.context, texture->Get());
         texture->Reset();
      }
      taa.history_valid = false;
      if (!color)
      {
         return true;
      }
      const std::tuple<HANDLE, ComPtr<ID3D12Resource>*> shared[] = {
         {color, std::addressof(taa.color)},
         {depth, std::addressof(taa.depth)},
         {motion, std::addressof(taa.motion)},
         {output, std::addressof(taa.output)}};
      for (const auto& [handle, texture] : shared)
      {
         if (const HRESULT hr = g_pass.device->OpenSharedHandle(handle, IID_PPV_ARGS(&*texture)); FAILED(hr))
         {
            return Fail("OpenSharedHandle(TAA texture)", hr);
         }
      }
      const D3D12_RESOURCE_DESC desc = taa.color->GetDesc();
      taa.result = CreateTexture(D3D12_HEAP_TYPE_DEFAULT, desc.Width, desc.Height, DXGI_FORMAT_R16G16B16A16_FLOAT,
         D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
      for (int i = 0; i < 2; ++i)
      {
         taa.history[i] = CreateTexture(D3D12_HEAP_TYPE_DEFAULT, desc.Width * taa.history_scale, desc.Height * taa.history_scale, DXGI_FORMAT_R16G16B16A16_FLOAT,
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
         taa.locks[i] = CreateTexture(D3D12_HEAP_TYPE_DEFAULT, desc.Width, desc.Height, DXGI_FORMAT_R32_UINT,
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
         if (!taa.history[i] || !taa.locks[i])
         {
            return Fail("TAA history / locks", E_OUTOFMEMORY);
         }
      }
      return taa.result ? true : Fail("TAA result", E_OUTOFMEMORY);
   }

   // As "Run()": between the D3D9 signal `value` on "in" and the queue's on
   // "out"; the output holds the resolve. False = nothing submitted.
   inline bool RunTaa(uint64_t value, const TaaParams& params)
   {
      Taa& taa = g_pass.taa;
      if (!taa.color)
      {
         return Fail("no TAA textures", E_FAIL);
      }
      const std::optional<D3D12Passes::Recording> recording = D3D12Passes::Begin(&g_pass.context);
      if (!recording)
      {
         return Fail("no free frame", E_FAIL);
      }
      ID3D12GraphicsCommandList* const list = recording->list;
      const uint32_t write = taa.history_index;
      ID3D12Resource* const srvs[TAA_SRVS] = {
         taa.color.Get(), taa.depth.Get(),
         taa.motion.Get(), taa.history[write ^ 1].Get(),
         nullptr, taa.locks[write ^ 1].Get(),
         nullptr, nullptr};
      ID3D12Resource* const uavs[TAA_UAVS] = {taa.history[write].Get(), taa.result.Get(), nullptr, taa.locks[write].Get()};
      D3D12_CPU_DESCRIPTOR_HANDLE descriptor = recording->srv_cpu;
      for (ID3D12Resource* const resource : srvs)
      {
         const D3D12_SHADER_RESOURCE_VIEW_DESC view = {
            .Format = resource ? resource->GetDesc().Format : DXGI_FORMAT_R8_UNORM,
            .ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
            .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
            .Texture2D = {.MipLevels = 1}};
         g_pass.device->CreateShaderResourceView(resource, &view, descriptor);
         descriptor.ptr += recording->srv_increment;
      }
      for (ID3D12Resource* const resource : uavs)
      {
         const D3D12_UNORDERED_ACCESS_VIEW_DESC view = {
            .Format = resource ? resource->GetDesc().Format : DXGI_FORMAT_R8_UNORM,
            .ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D};
         g_pass.device->CreateUnorderedAccessView(resource, nullptr, &view, descriptor);
         descriptor.ptr += recording->srv_increment;
      }
      const D3D12_RESOURCE_DESC desc = taa.color->GetDesc();
      const float width = float(desc.Width), height = float(desc.Height);
      const TaaConstants constants = {
         .render_resolution = {width, height},
         .inv_render_resolution = {1 / width, 1 / height},
         .motion_vector_scale = {width, height}, // UV delta -> pixels
         .depth_near_far = {params.near_plane, params.far_plane},
         .flags = ((params.reset || !taa.history_valid) ? TAA_FLAG_RESET : 0u),
         .jitter = {-params.jitter[0], -params.jitter[1]},
         .output_resolution = {width, height},
         .inv_output_resolution = {1 / width, 1 / height}};
      for (ID3D12Resource* const input : {taa.color.Get(), taa.depth.Get(), taa.motion.Get()})
      {
         D3D12Passes::Transition(list, input, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
      }
      for (ID3D12Resource* const target : {taa.history[write].Get(), taa.locks[write].Get()})
      {
         D3D12Passes::Transition(list, target, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
      }
      list->SetComputeRootSignature(taa.root.get());
      list->SetPipelineState(taa.pipeline.get());
      list->SetComputeRoot32BitConstants(0, sizeof(constants) / 4, &constants, 0);
      list->SetComputeRootDescriptorTable(1, recording->srv_gpu);
      list->Dispatch((UINT(desc.Width) + 7) / 8, (desc.Height + 7) / 8, 1);
      for (ID3D12Resource* const target : {taa.history[write].Get(), taa.locks[write].Get()})
      {
         D3D12Passes::Transition(list, target, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
      }
      for (ID3D12Resource* const input : {taa.color.Get(), taa.depth.Get(), taa.motion.Get()})
      {
         D3D12Passes::Transition(list, input, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COMMON);
      }
      D3D12Passes::Transition(list, taa.result.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
      D3D12Passes::Transition(list, taa.output.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
      list->CopyResource(taa.output.Get(), taa.result.Get());
      D3D12Passes::Transition(list, taa.output.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COMMON);
      D3D12Passes::Transition(list, taa.result.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
      if (!SubmitBetween(value))
      {
         return false;
      }
      taa.history_index = write ^ 1;
      taa.history_valid = true;
      return true;
   }

   // Null = drop it.
   inline bool SetGtaoOutput(HANDLE output)
   {
      Gtao& gtao = g_pass.gtao;
      for (ComPtr<ID3D12Resource>* texture :
         {std::addressof(gtao.output), std::addressof(gtao.mips), std::addressof(gtao.working[0]), std::addressof(gtao.working[1]),
            std::addressof(gtao.result), std::addressof(gtao.history[0]), std::addressof(gtao.history[1])})
      {
         D3D12Passes::Retire(&g_pass.context, texture->Get());
         texture->Reset();
      }
      gtao.history_valid = false;
      if (!output)
      {
         return true;
      }
      if (const HRESULT hr = g_pass.device->OpenSharedHandle(output, IID_PPV_ARGS(&gtao.output)); FAILED(hr))
      {
         return Fail("OpenSharedHandle(XeGTAO output)", hr);
      }
      const D3D12_RESOURCE_DESC desc = gtao.output->GetDesc();
      const D3D12_HEAP_PROPERTIES heap = {.Type = D3D12_HEAP_TYPE_DEFAULT};
      const D3D12_RESOURCE_DESC mips_desc = {
         .Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
         .Width = desc.Width,
         .Height = desc.Height,
         .DepthOrArraySize = 1,
         .MipLevels = GTAO_DEPTH_MIPS,
         .Format = DXGI_FORMAT_R32_FLOAT,
         .SampleDesc = {.Count = 1},
         .Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS};
      g_pass.device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &mips_desc, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, nullptr,
         IID_PPV_ARGS(&gtao.mips));
      for (ComPtr<ID3D12Resource>& working : gtao.working)
      {
         working = CreateTexture(D3D12_HEAP_TYPE_DEFAULT, desc.Width, desc.Height, DXGI_FORMAT_R8G8_UNORM,
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
      }
      gtao.result = CreateTexture(D3D12_HEAP_TYPE_DEFAULT, desc.Width, desc.Height, DXGI_FORMAT_R32_FLOAT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
         D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
      return (gtao.mips && gtao.working[0] && gtao.working[1] && gtao.result) ? true : Fail("XeGTAO scratch", E_OUTOFMEMORY);
   }

   // The prefilter (5 depth mips), the main pass (XeGTAO's or GT-VBAO's), the
   // history when asked (working 0 into working 1 and the history), both denoise
   // passes from wherever the AO is (the last one into `result`), then the copy
   // into the shared output. Descriptors: SRVs 0 working 0, 1 the previous
   // history, 2 motion, 3 depth (the history's t0-t3), 4 mips, 5 working 1; UAVs
   // 6 working 0, 7 working 1, 8 the next history (the history's u0-u1), 9
   // result, 11-15 mips (a table spans 4 SRVs / 5 UAVs from its start, within
   // the frame's 16). As "Run()": false = nothing submitted.
   inline bool RunGtao(uint64_t value, const GtaoParams& params)
   {
      Gtao& gtao = g_pass.gtao;
      ID3D12Resource* const depth = g_pass.smaa.depth.Get();
      if (!gtao.output || !depth || depth->GetDesc().Width != gtao.output->GetDesc().Width ||
          depth->GetDesc().Height != gtao.output->GetDesc().Height)
      {
         return Fail("no XeGTAO depth / output of one size", E_FAIL);
      }
      const D3D12_RESOURCE_DESC desc = gtao.output->GetDesc();
      const UINT width = UINT(desc.Width), height = desc.Height;
      if (params.history_weight > 0 && !gtao.temporal)
      {
         return Fail("no XeGTAO history pipeline", E_FAIL);
      }
      // Without this frame's motion vectors of the AO's size the history skips
      // the run (and restarts at the next)
      ID3D12Resource* const motion = g_pass.smaa.motion.Get();
      const bool temporal = params.history_weight > 0 && motion && motion->GetDesc().Width == desc.Width &&
                            motion->GetDesc().Height == desc.Height;
      if (temporal && !gtao.history[0])
      {
         for (ComPtr<ID3D12Resource>& history : gtao.history)
         {
            history = CreateTexture(D3D12_HEAP_TYPE_DEFAULT, desc.Width, desc.Height, DXGI_FORMAT_R16G16_FLOAT,
               D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
         }
         gtao.history_valid = false;
         if (!gtao.history[0] || !gtao.history[1])
         {
            gtao.history[0].Reset();
            gtao.history[1].Reset();
            return Fail("XeGTAO history", E_OUTOFMEMORY);
         }
      }
      ID3D12PipelineState* main = gtao.main.get();
      if (params.gtvbao)
      {
         main = ((temporal && gtao.gtvbao_temporal_main) ? gtao.gtvbao_temporal_main.get() : gtao.gtvbao_main.get());
         if (!main)
         {
            return Fail("no GT-VBAO pipeline", E_FAIL);
         }
      }
      const std::optional<D3D12Passes::Recording> recording = D3D12Passes::Begin(&g_pass.context);
      if (!recording)
      {
         return Fail("no free frame", E_FAIL);
      }
      ID3D12GraphicsCommandList* const list = recording->list;
      ID3D12Device* const device = g_pass.device.Get();
      const auto cpu = [&](UINT index)
      {
         return D3D12_CPU_DESCRIPTOR_HANDLE{recording->srv_cpu.ptr + index * recording->srv_increment};
      };
      const auto gpu = [&](UINT index)
      {
         return D3D12_GPU_DESCRIPTOR_HANDLE{recording->srv_gpu.ptr + index * recording->srv_increment};
      };
      const auto srv = [&](UINT index, ID3D12Resource* resource, UINT mips)
      {
         const D3D12_SHADER_RESOURCE_VIEW_DESC view = {
            .Format = resource->GetDesc().Format,
            .ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
            .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
            .Texture2D = {.MipLevels = mips}};
         device->CreateShaderResourceView(resource, &view, cpu(index));
      };
      const auto uav = [&](UINT index, ID3D12Resource* resource, UINT mip)
      {
         const D3D12_UNORDERED_ACCESS_VIEW_DESC view = {
            .Format = resource->GetDesc().Format,
            .ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D,
            .Texture2D = {.MipSlice = mip}};
         device->CreateUnorderedAccessView(resource, nullptr, &view, cpu(index));
      };
      const uint32_t previous = gtao.history_index, next = previous ^ 1u;
      srv(0, gtao.working[0].Get(), 1);
      srv(3, depth, 1);
      srv(4, gtao.mips.Get(), GTAO_DEPTH_MIPS);
      srv(5, gtao.working[1].Get(), 1);
      uav(6, gtao.working[0].Get(), 0);
      uav(7, gtao.working[1].Get(), 0);
      uav(9, gtao.result.Get(), 0);
      for (UINT mip = 0; mip < GTAO_DEPTH_MIPS; ++mip)
      {
         uav(11 + mip, gtao.mips.Get(), mip);
      }
      if (temporal)
      {
         srv(1, gtao.history[previous].Get(), 1);
         srv(2, motion, 1);
         uav(8, gtao.history[next].Get(), 0);
      }

      // Begin never hands out a frame still on the GPU and at most MAX_FRAMES are
      // in flight, so the slot of MAX_FRAMES runs ago is free
      const UINT64 slot = UINT64(gtao.camera_slot++ % D3D12Passes::MAX_FRAMES) * GTAO_CAMERA_SLOT_BYTES;
      memcpy(static_cast<char*>(gtao.camera_mapped) + slot, params.camera, sizeof(params.camera));
      const CB::GTAOKnobs knobs = {
         .final_value_power = params.final_value_power,
         .depth_scale = params.depth_scale,
         .radius_override = params.radius_override,
         .debug_view = params.debug_view,
         .viewport_pixel_size = {1.0f / width, 1.0f / height},
         .noise_index = params.noise_index,
         .camera_affine_row = float(params.affine_row),
         .camera_matrix_row = float(params.matrix_row)};
      // Not across a switch between XeGTAO and GT-VBAO either (their AO differs)
      const bool history_valid = temporal && gtao.history_valid && !params.history_reset && gtao.history_gtvbao == params.gtvbao;
      CB::GTAOTemporalConstants temporal_constants = {};
      static_assert(sizeof(temporal_constants.reprojection) == sizeof(params.reprojection));
      memcpy(&temporal_constants.reprojection, params.reprojection, sizeof(params.reprojection));
      temporal_constants.jitter_ndc = {params.jitter_ndc[0], params.jitter_ndc[1]};
      temporal_constants.history_weight = (history_valid ? params.history_weight : 0.0f);
      list->SetComputeRootSignature(gtao.root.get());
      list->SetComputeRootConstantBufferView(0, gtao.camera->GetGPUVirtualAddress() + slot);
      list->SetComputeRoot32BitConstants(1, sizeof(knobs) / 4, &knobs, 0);
      list->SetComputeRoot32BitConstants(4, sizeof(temporal_constants) / 4, &temporal_constants, 0);
      D3D12Passes::Transition(list, depth, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
      // A pass from SRV `source` into UAV table `target`; `written` goes
      // UNORDERED_ACCESS around it (the result stays there)
      const auto pass = [&](ID3D12PipelineState* pipeline, UINT source, UINT target, ID3D12Resource* written, UINT groups_x, UINT groups_y)
      {
         if (written)
         {
            D3D12Passes::Transition(list, written, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
         }
         list->SetPipelineState(pipeline);
         list->SetComputeRootDescriptorTable(2, gpu(source));
         list->SetComputeRootDescriptorTable(3, gpu(target));
         list->Dispatch(groups_x, groups_y, 1);
         if (written)
         {
            D3D12Passes::Transition(list, written, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
         }
      };
      pass(gtao.prefilter.get(), 3, 11, gtao.mips.Get(), (width + 15) / 16, (height + 15) / 16);
      pass(main, 4, 6, gtao.working[0].Get(), (width + 7) / 8, (height + 7) / 8);
      // The denoise reads working 0, or working 1 after the history
      UINT denoise_source = 0;
      if (temporal)
      {
         ID3D12Resource* const history = gtao.history[next].Get();
         D3D12Passes::Transition(list, motion, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
         D3D12Passes::Transition(list, history, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
         pass(gtao.temporal.get(), 0, 7, gtao.working[1].Get(), (width + 7) / 8, (height + 7) / 8);
         D3D12Passes::Transition(list, history, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
         D3D12Passes::Transition(list, motion, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COMMON);
         denoise_source = 1;
      }
      const UINT working_srvs[2] = {0, 5}, working_uavs[2] = {6, 7};
      pass(gtao.denoise.get(), working_srvs[denoise_source], working_uavs[denoise_source ^ 1], gtao.working[denoise_source ^ 1].Get(),
         (width + 15) / 16, (height + 7) / 8);
      pass(gtao.denoise_final.get(), working_srvs[denoise_source ^ 1], 9, nullptr, (width + 15) / 16, (height + 7) / 8);
      D3D12Passes::Transition(list, depth, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COMMON);
      D3D12Passes::Transition(list, gtao.result.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
      D3D12Passes::Transition(list, gtao.output.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
      list->CopyResource(gtao.output.Get(), gtao.result.Get());
      D3D12Passes::Transition(list, gtao.output.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COMMON);
      D3D12Passes::Transition(list, gtao.result.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
      if (!SubmitBetween(value))
      {
         gtao.history_valid = false;
         return false;
      }
      gtao.history_valid = temporal;
      if (temporal)
      {
         gtao.history_index = next;
         gtao.history_gtvbao = params.gtvbao;
      }
      return true;
   }
} // namespace D3D9Passes
