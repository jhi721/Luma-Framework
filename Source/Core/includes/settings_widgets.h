#pragma once

// Persisted ImGui settings widgets, shared by core.hpp and Core modules that don't include it.
// Requires "NAME" (the game's ReShade config section), defined by every game project before its Core includes.

#include <cmath>
#include <cstdint>
#include <type_traits>

#include <deps/imgui/imgui.h>
#include <include/reshade.hpp>

#ifndef ICON_FK_UNDO
#define ICON_FK_UNDO reinterpret_cast<const char*>(u8"\uf0e2")
#endif

// TODO: apply this everywhere!
template <typename T, bool Serialize = true>
bool DrawResetButton(
   T& value,               // The current value to modify
   const T& default_value, // The default value to compare against
   const char* name,       // Unique ID string for ImGui, and ReShade serialization
   reshade::api::effect_runtime* runtime = nullptr)
{
   bool edited = false;
   ImGui::SameLine();
   if (value != default_value)
   {
      int id = static_cast<int>(reinterpret_cast<uintptr_t>(name)); // Hacky, but it will do, almost certainly
      ImGui::PushID(id);
      if (ImGui::SmallButton(ICON_FK_UNDO))
      {
         value = default_value;
         edited = true;
         if constexpr (Serialize)
         {
            if (name)
            {
               reshade::set_config_value(runtime, NAME, name, value);
            }
         }
      }
      ImGui::PopID();
   }
   else // Draw a disabled placeholder so layout stays consistent
   {
      const auto& style = ImGui::GetStyle();
      ImVec2 size = ImGui::CalcTextSize(ICON_FK_UNDO);
      size.x += style.FramePadding.x;
      size.y += style.FramePadding.y;
      ImGui::InvisibleButton("", ImVec2(size.x, size.y));
   }
   return edited;
}

#ifndef LUMA_SETTINGS_SECTION
#define LUMA_SETTINGS_SECTION NAME
#endif

// Persisted user setting widgets: the control, its ini save, a tooltip (also shown while disabled) and a reset button.
// They save to LUMA_SETTINGS_SECTION, which must match the section the game's "LoadConfigs" reads. Each returns whether the value changed this frame.
inline bool DrawSettingSlider(const char* label, const char* key, float* value, float default_value, float min_value, float max_value, const char* tooltip, const char* format = "%.3f", ImGuiSliderFlags flags = 0)
{
   bool changed = ImGui::SliderFloat(label, value, min_value, max_value, format, flags);
   // Saved once the edit ends, not on every drag step
   if (ImGui::IsItemDeactivatedAfterEdit())
   {
      reshade::set_config_value(nullptr, LUMA_SETTINGS_SECTION, key, *value);
   }
   if (tooltip && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
   {
      ImGui::SetTooltip("%s", tooltip);
   }
   if (DrawResetButton<float, false>(*value, default_value, key))
   {
      reshade::set_config_value(nullptr, LUMA_SETTINGS_SECTION, key, *value);
      changed = true;
   }
   return changed;
}

// A bool, or a 0/1 float (usually a cbuffer field), shown as a checkbox. "forced": shown as "forced_value" and disabled (e.g. while
// another feature replaces it or always applies it), keeping the saved choice
template <typename T>
bool DrawSettingCheckbox(const char* label, const char* key, T* value, std::type_identity_t<T> default_value, const char* tooltip, bool forced = false, bool forced_value = false)
{
   static_assert(std::is_same_v<T, bool> || std::is_same_v<T, float>);
   bool shown = forced ? forced_value : (float(*value) > 0.5f);
   ImGui::BeginDisabled(forced);
   bool changed = ImGui::Checkbox(label, &shown);
   ImGui::EndDisabled();
   if (changed)
   {
      *value = T(shown);
   }
   if (tooltip && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
   {
      ImGui::SetTooltip("%s", tooltip);
   }
   if (forced)
   {
      DrawResetButton<bool, false>(shown, shown, key); // Only its spacer, the saved choice can't be reset from here
   }
   else
   {
      changed |= DrawResetButton<T, false>(*value, default_value, key);
   }
   if (changed)
   {
      reshade::set_config_value(nullptr, LUMA_SETTINGS_SECTION, key, *value);
   }
   return changed;
}

// The "Render Scale (%)" slider of a Luma-driven render scale, saved as a 0-1 float ("RenderScale"). Applied on release: every
// render size recreates the DLSS/FSR feature, a hitch per 1% step while dragging. Draw it once per mod (it holds the dragged value).
inline void DrawRenderScaleSlider(float* render_scale, float min_render_scale, const char* tooltip)
{
   static int held_render_scale = 0; // The slider's value while it's held, else 0
   int render_scale_percent = (held_render_scale != 0 ? held_render_scale : int(std::round(*render_scale * 100.f)));
   ImGui::SliderInt("Render Scale (%)", &render_scale_percent, int(min_render_scale * 100.f), 100, "%d%%", ImGuiSliderFlags_AlwaysClamp);
   held_render_scale = (ImGui::IsItemActive() ? render_scale_percent : 0);
   if (ImGui::IsItemDeactivatedAfterEdit())
   {
      *render_scale = float(render_scale_percent) / 100.f;
      reshade::set_config_value(nullptr, LUMA_SETTINGS_SECTION, "RenderScale", *render_scale);
   }
   if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
   {
      ImGui::SetTooltip("%s", tooltip);
   }
   if (DrawResetButton<float, false>(*render_scale, 1.f, "RenderScale"))
   {
      reshade::set_config_value(nullptr, LUMA_SETTINGS_SECTION, "RenderScale", *render_scale);
   }
}
