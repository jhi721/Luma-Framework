#pragma once

// The native D3D9 module's backend of the Luma MCP bridge (Scripts/luma_mcp.py), DEVELOPMENT only. Included by core_d3d9.hpp after
// "ReloadShaders()", it reads and drives the module's state; the pipe transport is includes/mcp_pipe.h. Jobs run on the game's thread
// at the present event ("Drain()", before the hand-off), as the module has no locks. The per draw recording ("mcp_capture",
// "McpAfterDraw()") is in core_d3d9.hpp, ahead of the draw hooks. The tools are the "tools" table, at the end.

#include "../includes/mcp_pipe.h"

namespace McpD3D9
{
   // A knob ("toggle", "float", "int"), a "counter" or a "texture" (read with view=registered:<name>)
   struct DevValue
   {
      std::string name;
      const char* kind = "counter";
      std::function<double()> get;
      std::function<void(double)> set; // Empty: read only
      double min = -DBL_MAX, max = DBL_MAX;
      const char* note = nullptr;
      std::function<IDirect3DResource9*()> texture;
   };
   inline std::vector<DevValue> dev_values;
   // Settings ("ReadSetting()" keys) "get_settings" lists: Core's, then the game's
   inline std::vector<const char*> setting_keys = {"DECODE_GAMMA", "DISPLAY_MAP", "DITHER", "FORCE_IMMEDIATE", "HDR_FALLBACK", "LATENCY",
      "OUTPUT", "OUTPUT_CLAMP", "OVERLAY_FIX", "PRESENT_CLAMP", "RCAS", "SAMPLER_UPGRADE", "SMAA", "SMAA_PREDICATION", "SMAA_T2X",
      "SYNC_INTERVAL", "SRUserType", "DLSSRenderPreset", "LumaTAAQuality", "GammaCorrectionType", "ReflexMode", "HDR10PlusGaming",
      "UseOSReferenceWhiteLevel", "ScenePeakWhite", "ScenePaperWhite", "UIPaperWhite"};
   // A view=rtv / srv read waits this many presents for its draw
   constexpr uint32_t READ_MAX_FRAMES = 60;

   // "kind" toggle, float or int over a live value: set here, it isn't saved, and the settings UI may write it again
   template <typename T>
   inline void RegisterValue(std::string name, const char* kind, T* value, double min, double max, const char* note = nullptr)
   {
      dev_values.push_back({.name = std::move(name),
         .kind = kind,
         .get = [value]
         { return double(*value); },
         .set = [value](double parsed)
         { *value = static_cast<T>(parsed); },
         .min = min,
         .max = max,
         .note = note});
   }

   // Read only: a counter, or a value that can't change live ("note" says why)
   template <typename T>
   inline void RegisterCounter(std::string name, const T* value, const char* note = nullptr)
   {
      dev_values.push_back({.name = std::move(name), .get = [value]
                                                     { return double(*value); },
         .note = note});
   }

   inline void RegisterTexture(std::string name, std::function<IDirect3DResource9*()> texture)
   {
      dev_values.push_back({.name = std::move(name), .kind = "texture", .texture = std::move(texture)});
   }

   inline DevValue* FindDevValue(std::string_view name)
   {
      const auto value = std::ranges::find(dev_values, name, &DevValue::name);
      return value != dev_values.end() ? &*value : nullptr;
   }

   inline std::string HandleText(uint64_t handle)
   {
      return std::format("0x{:X}", handle);
   }

   // Core's knobs, counters and textures, then the game's ("D3D9Game::RegisterDevValues()")
   inline void RegisterDevValues()
   {
      for (const auto& [name, value] : {std::pair{"smaa", &state.smaa}, {"smaa_t2x", &state.smaa_t2x}, {"smaa_predication", &state.smaa_predication},
              {"display_map", &state.display_map}, {"present_clamp", &state.present_clamp}, {"dither", &state.dither}, {"luma_taa", &state.luma_taa},
              {"gtao", &state.gtao}, {"luma_bloom", &state.luma_bloom}, {"kernel_720", &state.kernel_720}, {"hide_ui", &state.hide_ui},
              {"sampler_upgrade", &state.sampler_upgrade}, {"overlay_fix", &state.overlay_fix}})
      {
         RegisterValue(name, "toggle", value, 0, 1);
      }
      RegisterValue("output_clamp", "toggle", &state.output_clamp, 0, 1, "Create time: only pixel shaders the game creates after the change follow it");
      RegisterValue("rcas_sharpness", "float", &state.rcas_sharpness, 0, 1);
      RegisterValue("paper_white", "float", &state.paper_white, 0, 100, "The canvas' 1 in scRGB units (1 = 80 nits)");
      for (const auto& [name, value] : {std::pair{"map_peak_nits", &state.map_peak_nits}, {"map_paper_white_nits", &state.map_paper_white_nits},
              {"map_ui_paper_white_nits", &state.map_ui_paper_white_nits}})
      {
         RegisterValue(name, "float", value, 1, 10000);
      }
      for (const auto& [name, value] : {std::pair{"map_contrast", &state.map_contrast}, {"map_saturation", &state.map_saturation},
              {"map_highlights_desaturation", &state.map_highlights_desaturation}})
      {
         RegisterValue(name, "float", value, 0, 2);
      }
      for (uint32_t i = 0; i < 4; ++i)
      {
         RegisterValue(std::format("injected_c{}_{}", INJECT_BASE_REGISTER, "xyzw"[i]), "float", &state.injected[i], -FLT_MAX, FLT_MAX,
            "The Luma block's first row; the game may rewrite it every present");
      }
      for (const auto& [name, value] : {std::pair{"presents", &state.presents}, {"handoffs", &state.handoffs}, {"ps_replaced", &state.ps_replaced},
              {"injected_draws", &state.injected_draws}, {"rt_upgraded", &state.rt_upgraded}, {"rt_skipped", &state.rt_skipped},
              {"pass_runs", &state.pass_runs}, {"pass_failures", &state.pass_failures}, {"reset_renders", &state.reset_renders},
              {"snapshots", &state.snapshots}, {"predication_runs", &state.predication_runs}, {"t2x_runs", &state.t2x_runs},
              {"taa_runs", &state.taa_runs}, {"taa_resets", &state.taa_resets}, {"gtao_runs", &state.gtao_runs},
              {"gtao_fallbacks", &state.gtao_fallbacks}, {"hidden_ui_draws", &state.hidden_ui_draws}, {"sr_frames", &state.sr_frames},
              {"sr_mid_frames", &state.sr_mid_frames}, {"kernel_pushes", &state.kernel_pushes}, {"kernel_restores", &state.kernel_restores},
              {"host_rows_draws", &state.host_rows_draws}, {"ps_clamped_blend", &state.ps_clamped_blend},
              {"ps_clamped_velocity", &state.ps_clamped_velocity}, {"ps_clamp_refused", &state.ps_clamp_refused},
              {"bloom_draws", &state.bloom_draws}, {"bloom_mirrors", &state.bloom_mirrors}, {"bloom_composites", &state.bloom_composites},
              {"mv_scene_frames", &state.mv_scene_frames}, {"mv_draws_prepass", &state.mv_draws_prepass},
              {"mv_draws_patched", &state.mv_draws_patched}, {"mv_draws_jitter_only", &state.mv_draws_jitter_only},
              {"mv_draws_unpatched", &state.mv_draws_unpatched}, {"mv_draws_blended", &state.mv_draws_blended},
              {"mv_matches", &state.mv_matches}, {"mv_fallback_camera", &state.mv_fallback_camera},
              {"mv_fallback_reprojection", &state.mv_fallback_reprojection}, {"mv_fallback_zero", &state.mv_fallback_zero},
              {"mv_refused_vs", &state.mv_refused_vs}, {"mv_refused_ps", &state.mv_refused_ps}, {"helper_starts", &state.helper_starts},
              {"helper_deaths", &state.helper_deaths}, {"redirected_binds", &state.redirected_binds},
              {"redirected_copies", &state.redirected_copies}})
      {
         RegisterCounter(name, value);
      }
      for (const auto& [name, value] : {std::pair{"pass_open", &state.pass_open}, {"msaa_active", &state.msaa_active}, {"sr_ready", &state.sr_ready}})
      {
         RegisterCounter(name, value);
      }
      for (const auto& [name, value] : {std::pair{"canvas", &out.canvas}, {"shared_rt", &out.shared_rt}, {"snapshot", &out.snapshot},
              {"overlay", &hp.overlay}, {"pass_input", &g_pass.input}, {"pass_output", &g_pass.output}, {"pass_depth", &g_pass.depth},
              {"ao", &g_pass.ao}})
      {
         RegisterTexture(name, [value]
            { return reinterpret_cast<IDirect3DResource9*>(value->handle); });
      }
      d3d9_game->RegisterDevValues();
   }

   // The bridge's file (luma_mcp.py "decode()" reads these fields) and the readback's .bin, written by the pipe thread
   inline void WriteReadback(McpPipe::Job* job, D3D9Readback* readback, std::string_view stem, std::string_view view, std::string_view note)
   {
      const D3D9FormatInfo format = FindD3D9Format(readback->format);
      const std::filesystem::path path = McpPipe::ReadbackPath(*job, stem, state.presents);
      auto& json = job->result;
      json.Path("path", path).Field("width", readback->width).Field("height", readback->height).Field("depth", 1).Field("slices", 1);
      json.Field("rows", readback->height).Field("row_pitch", readback->row_pitch);
      json.Field("format", format.name).Field("format_id", format.dxgi_format).Field("view_format", format.name).Field("view_format_id", format.dxgi_format);
      json.Field("d3d_format", uint32_t(readback->format)).Field("view", view).Field("note", note);
      if (format.note)
      {
         json.Field("format_note", format.note);
      }
      else if (format.dxgi_format == 0)
      {
         json.Field("format_note", "No DXGI equivalent known for this D3D9 format: rows are the locked pitch, the bridge can't decode them");
      }
      job->files.emplace_back(path, std::move(readback->bytes));
   }

   inline bool RunStatus(McpPipe::Job* job)
   {
      auto& json = job->result;
      json.Field("game", McpPipe::game_name).Field("version", Globals::VERSION).Field("pid", GetCurrentProcessId()).Field("bits", sizeof(void*) * 8);
      json.Path("exe_path", McpPipe::exe_path).Field("backend", "d3d9_native");
      json.Field("frame_index", state.presents).Field("presenting", true);
      json.Key("output_resolution").BeginArray().Value(state.width).Value(state.height).EndArray();
      json.Key("render_resolution").BeginArray().Value(state.render_width).Value(state.render_height).EndArray();
      json.Key("back_buffer").BeginArray().Value(state.bb_width).Value(state.bb_height).EndArray();
      json.Field("output_mode", state.output_mode).Field("output_mode_request", state.output_mode_request).Field("display_hdr", state.display_hdr != 0);
      json.Field("redirecting", Redirecting()).Field("output_failed", g_output_failed).Field("bypassed", luma_bypassed);
      json.Field("replaced_shaders", out.replacements.size()).Field("reloaded_shaders", out.reloaded_ps.size());
      json.Field("has_compilation_errors", !reload_shaders_errors.empty());
      json.Field("pass_open", g_pass.open).Field("pass_gave_up", g_pass.gave_up);
      json.Key("presenter").BeginObject().Field("running", hp.process != nullptr).Field("starts", state.helper_starts).Field("deaths", state.helper_deaths);
      json.Field("frames", state.helper_frames).Field("gpu_sync", hp.gpu_sync).EndObject();
      json.Key("sr").BeginObject().Field("upscaler", g_sr_upscaler).Field("ready", state.sr_ready != 0).Field("frames", state.sr_frames);
      json.Field("luma_taa", state.luma_taa != 0).EndObject();
      json.Field("mv_enabled", state.mv_enabled != 0).Field("msaa_active", state.msaa_active != 0);
      MEMORYSTATUSEX memory_status = {sizeof(MEMORYSTATUSEX)};
      if (GlobalMemoryStatusEx(&memory_status))
      {
         // Process address space, what x86 games run out of
         json.Field("virtual_used_mb", (memory_status.ullTotalVirtual - memory_status.ullAvailVirtual) >> 20);
         json.Field("virtual_total_mb", memory_status.ullTotalVirtual >> 20);
      }
      return true;
   }

   inline bool RunDevValues(McpPipe::Job* job)
   {
      const std::string* filter = job->Arg("filter");
      auto& json = job->result;
      json.Key("values").BeginArray();
      for (const DevValue& value : dev_values)
      {
         if (filter && value.name.find(*filter) == std::string::npos)
         {
            continue;
         }
         json.BeginObject().Field("name", value.name).Field("kind", value.kind);
         if (value.texture)
         {
            const McpTraceTexture info = McpTextureInfo(reinterpret_cast<uint64_t>(value.texture()));
            json.Field("allocated", info.handle != 0);
            if (info.handle)
            {
               json.Field("format", FindD3D9Format(info.format).name).Field("d3d_format", uint32_t(info.format));
               json.Key("size").BeginArray().Value(info.width).Value(info.height).EndArray();
            }
         }
         else if (value.kind == std::string_view("toggle"))
         {
            json.Field("value", value.get() != 0);
         }
         else if (value.kind == std::string_view("float"))
         {
            json.Field("value", value.get());
            if (value.min != -FLT_MAX || value.max != FLT_MAX)
            {
               json.Field("min", value.min).Field("max", value.max);
            }
         }
         else
         {
            json.Field("value", int64_t(value.get()));
            if (value.set)
            {
               json.Field("min", value.min).Field("max", value.max);
            }
         }
         if (value.note)
         {
            json.Field("note", value.note);
         }
         json.EndObject();
      }
      json.EndArray();
      return true;
   }

   inline bool RunSetDevValue(McpPipe::Job* job)
   {
      const std::string* name = job->Arg("name");
      const std::string* text = job->Arg("value");
      if (!name || !text)
      {
         job->error = "Pass \"name\" and \"value\"";
         return true;
      }
      DevValue* value = FindDevValue(*name);
      if (!value)
      {
         job->error = "Unknown dev value " + *name + " (see luma_dev_values)";
         return true;
      }
      if (!value->set)
      {
         job->error = "Counters and textures are read only";
         return true;
      }
      // Strict, a typo must not silently become 0 or false
      double parsed = 0.0;
      char* end = nullptr;
      if (value->kind == std::string_view("toggle"))
      {
         if (*text != "1" && *text != "true" && *text != "0" && *text != "false")
         {
            job->error = *name + " takes 1, 0, true or false";
            return true;
         }
         parsed = ((*text == "1" || *text == "true") ? 1.0 : 0.0);
      }
      else if (value->kind == std::string_view("float"))
      {
         parsed = std::strtof(text->c_str(), &end);
      }
      else
      {
         parsed = double(std::strtoll(text->c_str(), &end, 0));
      }
      if (end && *end != '\0')
      {
         job->error = "\"" + *text + "\" is not a number";
         return true;
      }
      if (parsed < value->min || parsed > value->max)
      {
         job->error = std::format("{} takes {} to {}", *name, value->min, value->max);
         return true;
      }
      value->set(parsed);
      job->result.Field("name", *name).Field("value", parsed);
      return true;
   }

   inline bool RunGetSettings(McpPipe::Job* job)
   {
      auto& json = job->result;
      const auto& settings = cb_luma_global_settings;
      json.Field("display_mode", uint32_t(settings.DisplayMode)).Field("output_mode_request", state.output_mode_request);
      json.Field("scene_peak_white", settings.ScenePeakWhite).Field("scene_paper_white", settings.ScenePaperWhite).Field("ui_paper_white", settings.UIPaperWhite);
      json.Field("ini_section", NAME).Key("settings").BeginObject();
      for (const char* key : setting_keys)
      {
         char text[512] = "";
         if (ReadSetting(key, text, sizeof(text)))
         {
            json.Field(key, std::string_view(text));
         }
         else
         {
            json.Key(key).Null();
         }
      }
      json.EndObject();
      // The per game struct has no reflection, its layout is in the game's GameCBuffers.hlsl
      const float* game_settings = reinterpret_cast<const float*>(&settings.GameSettings);
      json.Key("game_settings").BeginArray();
      for (size_t i = 0; i < sizeof(settings.GameSettings) / sizeof(float); i++)
      {
         json.BeginObject().Field("index", i).Field("float", game_settings[i]).EndObject();
      }
      json.EndArray().Key("shader_defines").BeginArray();
      for (const std::string& define : d3d9_game->shader_defines)
      {
         json.Value(define);
      }
      json.EndArray();
      return true;
   }

   // Every pixel shader CRC the game created, replaced or matched (D3D9 replaces pixel shaders only; ReShade fires no destroy for the
   // game's, so "pipelines" counts every one created)
   inline bool RunShaderList(McpPipe::Job* job)
   {
      const std::string* stage = job->Arg("stage");
      const bool replaced_only = job->BoolArg("replaced_only", false);
      job->BoolArg("include_unloaded", false); // Every CRC is listed: nothing is unloaded
      const size_t offset = size_t((std::max)(job->IntArg("offset", 0), int64_t(0)));
      const size_t limit = size_t(job->IntArg("limit", 200, 1, 100000));
      std::map<uint32_t, uint32_t> pipelines;
      for (const auto& [pipeline, crc] : out.pipeline_crc)
      {
         ++pipelines[crc];
      }
      std::set<uint32_t> crcs;
      for (const auto& [crc, code] : out.replacements)
      {
         crcs.insert(crc);
      }
      for (const auto& [crc, count] : pipelines)
      {
         crcs.insert(crc);
      }
      for (const auto& [crc, error] : out.replacement_errors)
      {
         crcs.insert(crc);
      }
      crcs.erase(0); // Pipelines without a pixel shader
      auto& json = job->result;
      size_t matched = 0;
      json.Key("shaders").BeginArray();
      for (const uint32_t crc : crcs)
      {
         const bool replaced = out.replacements.contains(crc);
         const bool clamp_tried = out.clamped_ps.contains(crc);
         const bool failed = out.replacement_errors.contains(crc);
         if ((stage && _stricmp(stage->c_str(), "PS") != 0) || (replaced_only && !replaced && !failed && !clamp_tried))
         {
            continue;
         }
         const size_t index = matched++;
         if (index < offset || index >= offset + limit)
         {
            continue;
         }
         const char* kind = (replaced                         ? "file"
                             : !clamp_tried                   ? "none"
                             : out.clamped_ps.at(crc).empty() ? "clamp_refused"
                                                              : "patch_clamp");
         const auto count_of = [crc](const std::map<uint32_t, uint32_t>& counts)
         { return counts.contains(crc) ? counts.at(crc) : 0u; };
         json.BeginObject().Hash("hash", crc).Field("stage", "PS").Field("replaced", kind).Field("pipelines", count_of(pipelines));
         json.Field("replaced_creates", count_of(obs.replaced_creates)).Field("matched_draws", count_of(obs.matched_draws));
         json.Field("reloaded", out.reloaded_ps.contains(crc)).Field("velocity", out.velocity_crcs.contains(crc));
         if (replacement_files.contains(crc))
         {
            // A failed file keeps its last good replacement (if any) until a restart
            json.Path("file", replacement_files.at(crc)).Field("compiled", !failed);
         }
         if (failed)
         {
            json.Field("error", out.replacement_errors.at(crc));
         }
         if (replaced)
         {
            json.Field("bytes", out.replacements[crc].size());
         }
         json.EndObject();
      }
      json.EndArray().Field("live", pipelines.size()).Field("loaded", out.replacements.size());
      json.Field("matched", matched).Field("offset", offset).Field("returned", (matched > offset ? (std::min)(matched - offset, limit) : 0));
      return true;
   }

   inline bool RunReloadShaders(McpPipe::Job* job)
   {
      if (job->BoolArg("unload", false))
      {
         job->error = "The D3D9 module can't unload its replacements (the game's pipelines were created with them)";
         return true;
      }
      reload_shaders_errors = ReloadShaders(g_device);
      job->result.Field("compilation_errors", reload_shaders_errors);
      job->result.Field("note", "The pass device reopens and the presenter restarts over the next frames; their compile errors go to the log");
      return true;
   }

   inline bool RunWaitFrames(McpPipe::Job* job)
   {
      if (++job->frames < uint32_t(job->IntArg("frames", 1, 1, 600)))
      {
         return false;
      }
      job->result.Field("frames", job->frames).Field("frame_index", state.presents);
      return true;
   }

   // Phase 0 arms the recording at this present, every later present ends a recorded frame
   inline bool RunTraceCapture(McpPipe::Job* job)
   {
      const std::optional<uint32_t> trigger = job->HashArg("trigger");
      if (job->IntArg("frames", 1) > 1)
      {
         job->error = "frames (draw counts over several captures) isn't implemented on D3D9: capture single frames";
         return true;
      }
      if (job->phase == 0)
      {
         job->phase = 1;
         mcp_capture.entries.clear();
         mcp_capture.tracing = true;
         return false;
      }
      McpTraceEvent("present", "present", out.canvas.handle);
      ++job->frames;
      std::vector<size_t> trigger_indices;
      if (trigger)
      {
         for (size_t i = 0; i < mcp_capture.entries.size() && trigger_indices.size() < 32; ++i)
         {
            if (mcp_capture.entries[i].type == std::string_view("shader") && mcp_capture.entries[i].hash == *trigger)
            {
               trigger_indices.push_back(i);
            }
         }
         // Keep capturing until the pass shows up
         if (trigger_indices.empty() && job->frames < uint32_t(job->IntArg("max_frames", 120, 1, 3600)))
         {
            mcp_capture.entries.clear();
            return false;
         }
      }
      mcp_capture.tracing = false;
      auto& json = job->result;
      json.Field("count", mcp_capture.entries.size()).Field("frames_captured", job->frames);
      if (trigger)
      {
         json.Field("trigger_found", !trigger_indices.empty()).Key("trigger_indices").BeginArray();
         for (const size_t index : trigger_indices)
         {
            json.Value(index);
         }
         json.EndArray();
      }
      return true;
   }

   inline bool RunTraceList(McpPipe::Job* job)
   {
      if (mcp_capture.tracing)
      {
         job->error = "A capture is in progress";
         return true;
      }
      const std::string* type = job->Arg("type");
      const std::optional<uint32_t> hash = job->HashArg("hash");
      const std::string* stage = job->Arg("stage");
      const std::string* resource = job->Arg("resource");
      const uint64_t resource_handle = (resource ? std::strtoull(resource->c_str(), nullptr, 16) : 0);
      const bool replaced_only = job->BoolArg("replaced_only", false);
      // Neither exists here: no VS entries, bindings or buffer writes are recorded
      job->BoolArg("include_vs", false);
      job->BoolArg("include_bindings", false);
      job->BoolArg("include_buffer_writes", false);
      const size_t offset = size_t((std::max)(job->IntArg("offset", 0), int64_t(0)));
      const size_t limit = size_t(job->IntArg("limit", 300, 1, 5000));
      auto& json = job->result;
      json.Field("total", mcp_capture.entries.size()).Key("entries").BeginArray();
      size_t matched = 0;
      for (size_t i = 0; i < mcp_capture.entries.size(); ++i)
      {
         const McpTraceEntry& entry = mcp_capture.entries[i];
         const bool shader = entry.type == std::string_view("shader");
         const bool uses_resource = entry.rt0.handle == resource_handle || entry.source == resource_handle || entry.destination == resource_handle ||
                                    std::ranges::find(entry.ps_textures, resource_handle, &McpTraceTexture::handle) != entry.ps_textures.end();
         if ((type && *type != entry.type) || (hash && (!shader || entry.hash != *hash)) || (stage && (!shader || _stricmp(stage->c_str(), "PS") != 0)) ||
             (resource && !uses_resource) || (replaced_only && !entry.replaced))
         {
            continue;
         }
         if (matched >= offset && matched < offset + limit)
         {
            json.BeginObject().Field("index", i).Field("type", entry.type);
            if (shader)
            {
               json.Field("stage", "PS").Hash("hash", entry.hash).Field("instance", entry.instance);
               json.Field("replaced", (entry.reloaded ? "reloaded" : (entry.replaced ? "file" : "none")));
               json.Field("rt0", HandleText(entry.rt0.handle));
            }
            else
            {
               json.Field("name", entry.name);
            }
            if (!entry.note.empty())
            {
               json.Field("note", entry.note);
            }
            json.EndObject();
         }
         ++matched;
      }
      json.EndArray();
      json.Field("matched", matched).Field("offset", offset).Field("returned", (matched > offset ? (std::min)(matched - offset, limit) : 0));
      return true;
   }

   inline bool RunTraceGet(McpPipe::Job* job)
   {
      const size_t index = size_t(job->IntArg("index", -1));
      if (mcp_capture.tracing || index >= mcp_capture.entries.size())
      {
         job->error = std::format("Trace index {} is out of range (captured {})", index, mcp_capture.entries.size());
         return true;
      }
      const McpTraceEntry& entry = mcp_capture.entries[index];
      const auto write_texture = [](McpPipe::JsonWriter* json, const McpTraceTexture& texture)
      {
         json->Field("resource", HandleText(texture.handle));
         if (texture.handle)
         {
            json->Field("width", texture.width).Field("height", texture.height).Field("format", FindD3D9Format(texture.format).name);
            json->Field("d3d_format", uint32_t(texture.format));
         }
      };
      auto& json = job->result;
      json.Field("index", index).Field("type", entry.type).Field("name", entry.name);
      if (entry.type != std::string_view("shader"))
      {
         json.Field("source", HandleText(entry.source)).Field("destination", HandleText(entry.destination));
         return true;
      }
      json.Field("stage", "PS").Hash("hash", entry.hash).Field("instance", entry.instance).Field("replaced", entry.replaced).Field("reloaded", entry.reloaded);
      json.Field("vertex_count", entry.vertex_count).Field("skipped_by_luma", entry.cancelled).Field("note", entry.note);
      json.Key("rt0").BeginObject();
      write_texture(&json, entry.rt0);
      json.Field("back_buffer", entry.rt0_is_back_buffer).EndObject();
      json.Key("ps_textures").BeginArray();
      for (size_t slot = 0; slot < entry.ps_textures.size(); ++slot)
      {
         json.BeginObject().Field("slot", slot);
         write_texture(&json, entry.ps_textures[slot]);
         json.EndObject();
      }
      json.EndArray();
      const D3DVIEWPORT9& viewport = entry.viewport;
      json.Key("viewport").BeginObject().Field("x", viewport.X).Field("y", viewport.Y).Field("width", viewport.Width).Field("height", viewport.Height);
      json.Field("min_z", viewport.MinZ).Field("max_z", viewport.MaxZ).EndObject();
      json.Field("state_note", "RT0 is the target after Luma's redirects (canvas, Luma Bloom), the textures are the game's bindings");
      return true;
   }

   inline bool RunReadResource(McpPipe::Job* job)
   {
      IDirect3DDevice9* native = NativeOf(g_device);
      const std::string* view_arg = job->Arg("view");
      const std::string view = (view_arg ? *view_arg : "rtv");
      if (job->phase == 0)
      {
         if (view == "swapchain" || view.starts_with("registered:"))
         {
            IDirect3DResource9* resource = nullptr;
            IDirect3DSurface9* back_buffer = nullptr;
            std::string_view stem = view;
            std::string_view note = "Read at present: the texture's last content, usually this frame's";
            if (view == "swapchain")
            {
               resource = reinterpret_cast<IDirect3DResource9*>(out.canvas.handle);
               note = "The fp16 canvas (the game's back buffer draws, redirected) at present: before the hand-off, the presenter's composition and its UI clamp";
               if (!resource && SUCCEEDED(native->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &back_buffer)))
               {
                  resource = back_buffer;
                  note = "The game's D3D9 back buffer at present (no canvas: the redirect is off)";
               }
            }
            else
            {
               stem = std::string_view(view).substr(11);
               const DevValue* value = FindDevValue(stem);
               if (!value || !value->texture)
               {
                  job->error = std::format("Unknown registered texture {} (see luma_dev_values, kind texture)", stem);
                  return true;
               }
               resource = value->texture();
               if (!resource)
               {
                  job->error = std::format("{} is not allocated (its feature is off, or hasn't run yet)", stem);
                  return true;
               }
            }
            D3D9Readback readback;
            if (ReadD3D9Resource(native, resource, UINT(job->IntArg("mip", 0, 0, 15)), &readback, &job->error))
            {
               WriteReadback(job, &readback, stem, view, note);
            }
            Free(&back_buffer);
            return true;
         }
         if (view != "rtv" && view != "srv")
         {
            job->error = "\"view\" must be one of rtv, srv, swapchain, registered:<name> (D3D9: no uav, depth, stencil or ui)";
            return true;
         }
         if (const std::string* index_text = job->Arg("index"))
         {
            const size_t index = size_t(std::strtoull(index_text->c_str(), nullptr, 0));
            if (mcp_capture.tracing || index >= mcp_capture.entries.size() || mcp_capture.entries[index].type != std::string_view("shader"))
            {
               job->error = std::format("Trace index {} is not a draw of the last capture ({} entries), run luma_trace_capture first", index,
                  mcp_capture.entries.size());
               return true;
            }
            mcp_capture.read.hash = mcp_capture.entries[index].hash;
            mcp_capture.read.instance = mcp_capture.entries[index].instance;
         }
         else if (const std::optional<uint32_t> hash = job->HashArg("hash"))
         {
            mcp_capture.read.hash = *hash;
            mcp_capture.read.instance = uint32_t(job->IntArg("instance", 0, 0, INT32_MAX));
         }
         else
         {
            job->error = "Pass \"index\" (luma_trace_list) or \"hash\" (+ \"instance\"), or view=swapchain / registered:<name>";
            return true;
         }
         mcp_capture.read.srv = view == "srv";
         mcp_capture.read.slot = uint32_t(job->IntArg("slot", 0, 0, (mcp_capture.read.srv ? int64_t(std::size(out.ps_textures)) - 1 : 3)));
         mcp_capture.read.mip = uint32_t(job->IntArg("mip", 0, 0, 15));
         mcp_capture.read.armed = true;
         mcp_capture.read.done = false;
         job->phase = 1;
         return false;
      }
      if (!mcp_capture.read.done)
      {
         if (++job->frames < READ_MAX_FRAMES)
         {
            return false;
         }
         mcp_capture.read.armed = false;
         job->error = std::format("The draw (hash 0x{:08X} instance {}) didn't come in {} frames", mcp_capture.read.hash, mcp_capture.read.instance,
            READ_MAX_FRAMES);
         return true;
      }
      mcp_capture.read.done = false;
      if (!mcp_capture.read.error.empty())
      {
         job->error = mcp_capture.read.error;
         return true;
      }
      job->result.Hash("hash", mcp_capture.read.hash).Field("instance", mcp_capture.read.instance).Field("slot", mcp_capture.read.slot);
      const std::string stem = std::format("{:08X}_{}_{}{}", mcp_capture.read.hash, mcp_capture.read.instance, view, mcp_capture.read.slot);
      WriteReadback(job, &mcp_capture.read.readback, stem, view,
         mcp_capture.read.srv ? "The draw's pixel shader texture, read right after the draw" : "The draw's render target (after Luma's redirects), read right after the draw");
      return true;
   }

   // A tool's run: true when its job is done, false to run it again at the next present
   struct Tool
   {
      std::string_view name;
      bool (*run)(McpPipe::Job*) = nullptr;
   };
   inline constexpr Tool tools[] = {
      {.name = "status", .run = RunStatus},
      {.name = "dev_values", .run = RunDevValues},
      {.name = "set_dev_value", .run = RunSetDevValue},
      {.name = "get_settings", .run = RunGetSettings},
      {.name = "shader_list", .run = RunShaderList},
      {.name = "reload_shaders", .run = RunReloadShaders},
      {.name = "wait_frames", .run = RunWaitFrames},
      {.name = "trace_capture", .run = RunTraceCapture},
      {.name = "trace_list", .run = RunTraceList},
      {.name = "trace_get", .run = RunTraceGet},
      {.name = "read_resource", .run = RunReadResource},
   };

   inline bool RunJob(McpPipe::Job* job)
   {
      if (const auto tool = std::ranges::find(tools, job->tool, &Tool::name); tool != std::end(tools))
      {
         return tool->run(job);
      }
      job->error = "Unknown tool " + job->tool + " (the D3D9 backend has";
      for (const Tool& tool : tools)
      {
         job->error += " ";
         job->error += tool.name;
      }
      job->error += ")";
      return true;
   }

   // The present event, on the game's thread, before the hand-off: the queued jobs, then the next frame's draw counts
   inline void Drain(device* device)
   {
      if (mcp_capture.read.resource) // The armed draw was the frame's last
      {
         McpReadPending(NativeOf(device));
      }
      McpPipe::Drain(state.presents, RunJob);
      if (McpActive())
      {
         mcp_capture.instances.clear();
      }
   }
} // namespace McpD3D9
