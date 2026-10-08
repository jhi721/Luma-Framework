#pragma once

#include <algorithm>
#include <cstring>
#include <span>
#include <string>

#include <deps/imgui/imgui.h>

// Two-column settings rows: the label in a fixed left column, the control in the right one, ending where the reset button slot of
// "DrawResetButton" (or its placeholder) begins, so every row lines up. The wrappers take the same arguments as their ImGui
// counterparts, so "IsItemHovered()" and "DrawResetButton()" after them still refer to the control.
namespace SettingsUI
{
   // Sizes the next item to end where the reset button slot begins
   inline void SetControlWidth()
   {
      const ImGuiStyle& style = ImGui::GetStyle();
      ImGui::SetNextItemWidth(-(ImGui::CalcTextSize(reinterpret_cast<const char*>(u8"\uf0e2") /*ICON_FK_UNDO*/).x + (style.FramePadding.x * 2.f) + style.ItemSpacing.x));
   }

   // Draws the label (without its "##" suffix) and places and sizes the next item. Returns the hidden label to give that item,
   // unique as it carries the visible one.
   inline std::string DrawLabel(const char* label)
   {
      const ImGuiStyle& style = ImGui::GetStyle();
      const char* const label_end = strstr(label, "##");
      const float label_start_x = ImGui::GetCursorPosX();
      ImGui::AlignTextToFramePadding();
      ImGui::TextUnformatted(label, label_end);
      // Absolute, so rows indented by tree nodes still line up, while a label wider than the column only pushes its own control
      constexpr float label_column_font_sizes = 15.f;
      const float label_column_x = ImGui::GetCursorStartPos().x + (ImGui::GetFontSize() * label_column_font_sizes);
      ImGui::SameLine((std::max)(label_column_x, label_start_x + ImGui::CalcTextSize(label, label_end).x + style.ItemSpacing.x));
      SetControlWidth();
      return std::string("##") + label;
   }

   inline bool SliderFloat(const char* label, float* v, float v_min, float v_max, const char* format = "%.3f", ImGuiSliderFlags flags = 0)
   {
      return ImGui::SliderFloat(DrawLabel(label).c_str(), v, v_min, v_max, format, flags);
   }

   inline bool SliderInt(const char* label, int* v, int v_min, int v_max, const char* format = "%d", ImGuiSliderFlags flags = 0)
   {
      return ImGui::SliderInt(DrawLabel(label).c_str(), v, v_min, v_max, format, flags);
   }

   inline bool Checkbox(const char* label, bool* v)
   {
      return ImGui::Checkbox(DrawLabel(label).c_str(), v);
   }

   inline bool Combo(const char* label, int* current_item, const char* const items[], int items_count, int popup_max_height_in_items = -1)
   {
      return ImGui::Combo(DrawLabel(label).c_str(), current_item, items, items_count, popup_max_height_in_items);
   }

   inline bool Combo(const char* label, int* current_item, const char* items_separated_by_zeros, int popup_max_height_in_items = -1)
   {
      return ImGui::Combo(DrawLabel(label).c_str(), current_item, items_separated_by_zeros, popup_max_height_in_items);
   }

   inline bool BeginCombo(const char* label, const char* preview_value, ImGuiComboFlags flags = 0)
   {
      return ImGui::BeginCombo(DrawLabel(label).c_str(), preview_value, flags);
   }

   // One button per option over the control width, the selected one tinted with the check mark color (Luma's accent in its
   // overlay), also when hovered. Returns the clicked option, or -1.
   inline int SegmentedButtons(const char* label, std::span<const char* const> names, int selected_index)
   {
      constexpr float spacing = 1.f;
      ImGui::PushID(DrawLabel(label).c_str());
      const float button_width = (ImGui::CalcItemWidth() - (spacing * float(names.size() - 1))) / float(names.size());
      const ImVec4 frame_color = ImGui::GetStyleColorVec4(ImGuiCol_FrameBg);
      const ImVec4 accent_color = ImGui::GetStyleColorVec4(ImGuiCol_CheckMark);
      // Opaque blends of the accent over the frame color
      const auto Tint = [&](float accent_amount)
      {
         return ImVec4(frame_color.x + ((accent_color.x - frame_color.x) * accent_amount), frame_color.y + ((accent_color.y - frame_color.y) * accent_amount), frame_color.z + ((accent_color.z - frame_color.z) * accent_amount), 1.f);
      };
      const ImVec4 selected_color = Tint(0.45f);
      const ImVec4 selected_hovered_color = Tint(0.6f);
      int clicked_index = -1;
      ImGui::BeginGroup();
      ImGui::PushStyleVarX(ImGuiStyleVar_ItemSpacing, spacing);
      for (int i = 0; i < int(names.size()); i++)
      {
         if (i > 0)
         {
            ImGui::SameLine();
         }
         const bool selected = (i == selected_index);
         if (selected)
         {
            ImGui::PushStyleColor(ImGuiCol_Button, selected_color);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, selected_hovered_color);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, selected_hovered_color);
         }
         if (ImGui::Button(names[i], ImVec2(button_width, 0.f)) && !selected)
         {
            clicked_index = i;
         }
         if (selected)
         {
            ImGui::PopStyleColor(3);
         }
      }
      ImGui::PopStyleVar();
      ImGui::EndGroup();
      ImGui::PopID();
      return clicked_index;
   }
} // namespace SettingsUI
