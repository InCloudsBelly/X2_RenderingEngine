#pragma once

#include "X2/Core/Ref.h"

#include <imgui/imgui.h>

namespace X2 {
	class VulkanTexture2D;
}

namespace ImGui
{
    bool TreeNodeWithIcon(X2::Ref<X2::VulkanTexture2D> icon, ImGuiID id, ImGuiTreeNodeFlags flags, const char* label, const char* label_end, ImColor iconTint = IM_COL32_WHITE);
    bool TreeNodeWithIcon(X2::Ref<X2::VulkanTexture2D> icon, const void* ptr_id, ImGuiTreeNodeFlags flags, ImColor iconTint, const char* fmt, ...);
    bool TreeNodeWithIcon(X2::Ref<X2::VulkanTexture2D> icon, const char* label, ImGuiTreeNodeFlags flags, ImColor iconTint = IM_COL32_WHITE);

} // namespace UI
