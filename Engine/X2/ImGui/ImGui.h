#pragma once

#include "UICore.h"

#include "X2/Asset/AssetMetadata.h"

#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif
#include "imgui_internal.h"

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <filesystem>
#include <map>

#include "X2/Scene/Entity.h"
#include "X2/Scene/Prefab.h"
#include "X2/Asset/AssetManager.h"

#include "X2/Editor/AssetEditorPanelInterface.h"

#include "X2/ImGui/Colours.h"
#include "X2/ImGui/ImGuiUtilities.h"
#include "X2/ImGui/ImGuiWidgets.h"
#include "X2/ImGui/ImGuiFonts.h"
#include "X2/Utilities/StringUtils.h"
//#include "X2/Script/ScriptCache.h"

#include "choc/text/choc_StringUtilities.h"

namespace X2::UI {

#define X2_MESSAGE_BOX_OK_BUTTON BIT(0)
#define X2_MESSAGE_BOX_CANCEL_BUTTON BIT(1)
#define X2_MESSAGE_BOX_USER_FUNC BIT(2)

	static char* s_MultilineBuffer = nullptr;
	static uint32_t s_MultilineBufferSize = 1024 * 1024; // 1KB

	struct MessageBoxData
	{
		std::string Title = "";
		std::string Body = "";
		uint32_t Flags = 0;
		uint32_t Width = 0;
		uint32_t Height = 0;
		std::function<void()> UserRenderFunction;

		bool ShouldOpen = true;
		bool IsOpen = false;
	};
	static std::unordered_map<std::string, MessageBoxData> s_MessageBoxes;

	static ImFont* FindFont(const std::string& fontName)
	{
		return Fonts::Get(fontName);
	}

	static void PushFontBold()
	{
		ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
	}

	static void PushFontLarge()
	{
		ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);
	}

	template<uint32_t flags = 0, typename... TArgs>
	static void ShowSimpleMessageBox(const char* title, const char* content, TArgs&&... contentArgs)
	{
		auto& messageBoxData = s_MessageBoxes[title];
		messageBoxData.Title = fmt::format("{0}##MessageBox{1}", title, s_MessageBoxes.size() + 1);
		if constexpr (sizeof...(contentArgs) > 0)
			messageBoxData.Body = fmt::format(content, std::forward<TArgs>(contentArgs)...);
		else
			messageBoxData.Body = content;
		messageBoxData.Flags = flags;
		messageBoxData.Width = 600;
		messageBoxData.Height = 0;
		messageBoxData.ShouldOpen = true;
	}

	static void ShowMessageBox(const char* title, const std::function<void()>& renderFunction, uint32_t width = 600, uint32_t height = 0)
	{
		auto& messageBoxData = s_MessageBoxes[title];
		messageBoxData.Title = fmt::format("{0}##MessageBox{1}", title, s_MessageBoxes.size() + 1);
		messageBoxData.UserRenderFunction = renderFunction;
		messageBoxData.Flags = X2_MESSAGE_BOX_USER_FUNC;
		messageBoxData.Width = width;
		messageBoxData.Height = height;
		messageBoxData.ShouldOpen = true;
	}

	static void RenderMessageBoxes()
	{
		for (auto& [key, messageBoxData] : s_MessageBoxes)
		{
			if (messageBoxData.ShouldOpen && !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId))
			{
				ImGui::OpenPopup(messageBoxData.Title.c_str());
				messageBoxData.ShouldOpen = false;
				messageBoxData.IsOpen = true;
			}

			if (!messageBoxData.IsOpen)
				continue;

			ImVec2 center = ImGui::GetMainViewport()->GetCenter();
			ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
			ImGui::SetNextWindowSize(ImVec2{ (float)messageBoxData.Width, (float)messageBoxData.Height });

			if (ImGui::BeginPopupModal(messageBoxData.Title.c_str(), &messageBoxData.IsOpen, ImGuiWindowFlags_AlwaysAutoResize))
			{
				if (messageBoxData.Flags & X2_MESSAGE_BOX_USER_FUNC)
				{
					X2_CORE_VERIFY(messageBoxData.UserRenderFunction, "No render function provided for message box!");
					messageBoxData.UserRenderFunction();
				}
				else
				{
					ImGui::TextWrapped(messageBoxData.Body.c_str());

					if (messageBoxData.Flags & X2_MESSAGE_BOX_OK_BUTTON)
					{
						if (ImGui::Button("Ok"))
							ImGui::CloseCurrentPopup();

						if (messageBoxData.Flags & X2_MESSAGE_BOX_CANCEL_BUTTON)
							ImGui::SameLine();
					}

					if (messageBoxData.Flags & X2_MESSAGE_BOX_CANCEL_BUTTON && ImGui::Button("Cancel"))
					{
						ImGui::CloseCurrentPopup();
					}
				}

				ImGui::EndPopup();
			}
		}
	}

	static bool PropertyGridHeader(const std::string& name, bool openByDefault = true)
	{
		ImGuiTreeNodeFlags treeNodeFlags = ImGuiTreeNodeFlags_Framed
			| ImGuiTreeNodeFlags_SpanAvailWidth
			| ImGuiTreeNodeFlags_AllowItemOverlap
			| ImGuiTreeNodeFlags_FramePadding;

		if (openByDefault)
			treeNodeFlags |= ImGuiTreeNodeFlags_DefaultOpen;

		bool open = false;
		const float framePaddingX = 6.0f;
		const float framePaddingY = 6.0f; // affects height of the header

		UI::ScopedStyle headerRounding(ImGuiStyleVar_FrameRounding, 0.0f);
		UI::ScopedStyle headerPaddingAndHeight(ImGuiStyleVar_FramePadding, ImVec2{ framePaddingX, framePaddingY });

		//UI::PushID();
		ImGui::PushID(name.c_str());
		open = ImGui::TreeNodeEx("##dummy_id", treeNodeFlags, Utils::ToUpper(name).c_str());
		//UI::PopID();
		ImGui::PopID();

		return open;
	}

	static bool TreeNodeWithIcon(const std::string& label, const Ref<VulkanTexture2D>& icon, const ImVec2& size, bool openByDefault = true)
	{
		ImGuiTreeNodeFlags treeNodeFlags = ImGuiTreeNodeFlags_Framed
			| ImGuiTreeNodeFlags_SpanAvailWidth
			| ImGuiTreeNodeFlags_AllowItemOverlap
			| ImGuiTreeNodeFlags_FramePadding;

		if (openByDefault)
			treeNodeFlags |= ImGuiTreeNodeFlags_DefaultOpen;

		bool open = false;
		const float framePaddingX = 6.0f;
		const float framePaddingY = 6.0f; // affects height of the header

		UI::ScopedStyle headerRounding(ImGuiStyleVar_FrameRounding, 0.0f);
		UI::ScopedStyle headerPaddingAndHeight(ImGuiStyleVar_FramePadding, ImVec2{ framePaddingX, framePaddingY });

		ImGui::PushID(label.c_str());
		ImVec2 contentRegionAvailable = ImGui::GetContentRegionAvail();
		open = ImGui::TreeNodeEx("##dummy_id", treeNodeFlags, "");

		float lineHeight = ImGui::GetItemRectMax().y - ImGui::GetItemRectMin().y;
		ImGui::SameLine();
		UI::ShiftCursorY(size.y / 2.0f - 1.0f);
		UI::Image(icon, size);
		ImGui::SameLine();
		UI::ShiftCursorY(-(size.y / 2.0f) + 1.0f);
		ImGui::TextUnformatted(Utils::ToUpper(label).c_str());

		ImGui::PopID();

		return open;
	}

	static void Separator()
	{
		ImGui::Separator();
	}

	static void BeginDisabled(bool disabled = true)
	{
		if (disabled)
			ImGui::BeginDisabled(true);
	}

	static bool IsItemDisabled()
	{
		return ImGui::GetItemFlags() & ImGuiItemFlags_Disabled;
	}

	static void EndDisabled()
	{
		// NOTE(Peter): Cheeky hack to prevent ImGui from asserting (required due to the nature of UI::BeginDisabled)
		if (GImGui->DisabledStackSize > 0)
			ImGui::EndDisabled();
	}

	static bool Property(const char* label, std::string& value)
	{
		bool modified = false;

		ShiftCursor(10.0f, 9.0f);
		ImGui::Text(label);
		ImGui::NextColumn();
		ShiftCursorY(4.0f);
		ImGui::PushItemWidth(-1);

		char buffer[256];
		strcpy_s(buffer, sizeof(buffer), value.c_str());

		if (ImGui::InputText(fmt::format("##{0}", label).c_str(), buffer, 256))
		{
			value = buffer;
			modified = true;
		}

		if (!IsItemDisabled())
			DrawItemActivityOutline(2.0f, true, Colours::Theme::accent);

		ImGui::PopItemWidth();
		ImGui::NextColumn();
		Draw::Underline();

		return modified;
	}

	static bool PropertyMultiline(const char* label, std::string& value)
	{
		bool modified = false;

		ImGui::Text(label);
		ImGui::NextColumn();
		ImGui::PushItemWidth(-1);

		if (!s_MultilineBuffer)
		{
			s_MultilineBuffer = hnew char[s_MultilineBufferSize];
			memset(s_MultilineBuffer, 0, s_MultilineBufferSize);
		}

		strcpy_s(s_MultilineBuffer, s_MultilineBufferSize, value.c_str());

		if (ImGui::InputTextMultiline(fmt::format("##{0}", label).c_str(), s_MultilineBuffer, 1024 * 1024))
		{
			value = s_MultilineBuffer;
			modified = true;
		}

		ImGui::PopItemWidth();
		ImGui::NextColumn();

		return modified;
	}

	static void Property(const char* label, const std::string& value)
	{
		ShiftCursor(10.0f, 9.0f);
		ImGui::Text(label);
		ImGui::NextColumn();
		ShiftCursorY(4.0f);
		ImGui::PushItemWidth(-1);
		BeginDisabled();
		ImGui::InputText(fmt::format("##{0}", label).c_str(), (char*)value.c_str(), value.size(), ImGuiInputTextFlags_ReadOnly);
		EndDisabled();

		if (!IsItemDisabled())
			DrawItemActivityOutline(2.0f, true, Colours::Theme::accent);

		ImGui::PopItemWidth();
		ImGui::NextColumn();
		Draw::Underline();
	}

	static void Property(const char* label, const char* value)
	{
		ShiftCursor(10.0f, 9.0f);
		ImGui::Text(label);
		ImGui::NextColumn();
		ShiftCursorY(4.0f);
		ImGui::PushItemWidth(-1);
		BeginDisabled();
		ImGui::InputText(fmt::format("##{0}", label).c_str(), (char*)value, 256, ImGuiInputTextFlags_ReadOnly);
		EndDisabled();

		if (!IsItemDisabled())
			DrawItemActivityOutline(2.0f, true, Colours::Theme::accent);

		ImGui::PopItemWidth();
		ImGui::NextColumn();
		Draw::Underline();
	}

	static bool Property(const char* label, char* value, size_t length)
	{
		ShiftCursor(10.0f, 9.0f);
		ImGui::Text(label);
		ImGui::NextColumn();
		ShiftCursorY(4.0f);
		ImGui::PushItemWidth(-1);

		bool modified = ImGui::InputText(fmt::format("##{0}", label).c_str(), value, length);

		if (!IsItemDisabled())
			DrawItemActivityOutline(2.0f, true, Colours::Theme::accent);

		ImGui::PopItemWidth();
		ImGui::NextColumn();
		Draw::Underline();

		return modified;
	}

	static bool Property(const char* label, bool& value, const char* helpText = "")
	{
		bool modified = false;

		ShiftCursor(10.0f, 9.0f);
		ImGui::Text(label);
		if (std::strlen(helpText) != 0)
		{
			ImGui::SameLine();
			HelpMarker(helpText);
		}
		ImGui::NextColumn();
		ShiftCursorY(4.0f);
		ImGui::PushItemWidth(-1);

		modified = ImGui::Checkbox(fmt::format("##{0}", label).c_str(), &value);

		if (!IsItemDisabled())
			DrawItemActivityOutline(2.0f, true, Colours::Theme::accent);

		ImGui::PopItemWidth();
		ImGui::NextColumn();
		Draw::Underline();

		return modified;
	}

	static bool Property(const char* label, int8_t& value, int8_t min = 0, int8_t max = 0)
	{
		ShiftCursor(10.0f, 9.0f);
		ImGui::Text(label);
		ImGui::NextColumn();
		ShiftCursorY(4.0f);
		ImGui::PushItemWidth(-1);

		bool modified = UI::DragInt8(fmt::format("##{0}", label).c_str(), &value, 1.0f, min, max);

		if (!IsItemDisabled())
			DrawItemActivityOutline(2.0f, true, Colours::Theme::accent);

		ImGui::PopItemWidth();
		ImGui::NextColumn();
		Draw::Underline();

		return modified;
	}

	static bool Property(const char* label, int16_t& value, int16_t min = 0, int16_t max = 0)
	{
		ShiftCursor(10.0f, 9.0f);
		ImGui::Text(label);
		ImGui::NextColumn();
		ShiftCursorY(4.0f);
		ImGui::PushItemWidth(-1);

		bool modified = UI::DragInt16(fmt::format("##{0}", label).c_str(), &value, 1.0f, min, max);

		if (!IsItemDisabled())
			DrawItemActivityOutline(2.0f, true, Colours::Theme::accent);

		ImGui::PopItemWidth();
		ImGui::NextColumn();
		Draw::Underline();

		return modified;
	}

	static bool Property(const char* label, int32_t& value, int32_t min = 0, int32_t max = 0)
	{
		ShiftCursor(10.0f, 9.0f);
		ImGui::Text(label);
		ImGui::NextColumn();
		ShiftCursorY(4.0f);
		ImGui::PushItemWidth(-1);

		bool modified = UI::DragInt32(fmt::format("##{0}", label).c_str(), &value, 1.0f, min, max);

		if (!IsItemDisabled())
			DrawItemActivityOutline(2.0f, true, Colours::Theme::accent);

		ImGui::PopItemWidth();
		ImGui::NextColumn();
		Draw::Underline();

		return modified;
	}

	static bool Property(const char* label, int64_t& value, int64_t min = 0, int64_t max = 0)
	{
		ShiftCursor(10.0f, 9.0f);
		ImGui::Text(label);
		ImGui::NextColumn();
		ShiftCursorY(4.0f);
		ImGui::PushItemWidth(-1);

		bool modified = UI::DragInt64(fmt::format("##{0}", label).c_str(), &value, 1.0f, min, max);

		if (!IsItemDisabled())
			DrawItemActivityOutline(2.0f, true, Colours::Theme::accent);

		ImGui::PopItemWidth();
		ImGui::NextColumn();
		Draw::Underline();

		return modified;
	}

	static bool Property(const char* label, uint8_t& value, uint8_t minValue = 0, uint8_t maxValue = 0)
	{
		ShiftCursor(10.0f, 9.0f);
		ImGui::Text(label);
		ImGui::NextColumn();
		ShiftCursorY(4.0f);
		ImGui::PushItemWidth(-1);

		bool modified = UI::DragUInt8(fmt::format("##{0}", label).c_str(), &value, 1.0f, minValue, maxValue);

		if (!IsItemDisabled())
			DrawItemActivityOutline(2.0f, true, Colours::Theme::accent);

		ImGui::PopItemWidth();
		ImGui::NextColumn();
		Draw::Underline();

		return modified;
	}

	static bool Property(const char* label, uint16_t& value, uint16_t minValue = 0, uint16_t maxValue = 0)
	{
		ShiftCursor(10.0f, 9.0f);
		ImGui::Text(label);
		ImGui::NextColumn();
		ShiftCursorY(4.0f);
		ImGui::PushItemWidth(-1);

		bool modified = UI::DragUInt16(fmt::format("##{0}", label).c_str(), &value, 1.0f, minValue, maxValue);

		if (!IsItemDisabled())
			DrawItemActivityOutline(2.0f, true, Colours::Theme::accent);

		ImGui::PopItemWidth();
		ImGui::NextColumn();
		Draw::Underline();

		return modified;
	}

	static bool Property(const char* label, uint32_t& value, uint32_t minValue = 0, uint32_t maxValue = 0)
	{
		ShiftCursor(10.0f, 9.0f);
		ImGui::Text(label);
		ImGui::NextColumn();
		ShiftCursorY(4.0f);
		ImGui::PushItemWidth(-1);

		bool modified = UI::DragUInt32(fmt::format("##{0}", label).c_str(), &value, 1.0f, minValue, maxValue);

		if (!IsItemDisabled())
			DrawItemActivityOutline(2.0f, true, Colours::Theme::accent);

		ImGui::PopItemWidth();
		ImGui::NextColumn();
		Draw::Underline();

		return modified;
	}

	static bool Property(const char* label, uint64_t& value, uint64_t minValue = 0, uint64_t maxValue = 0)
	{
		ShiftCursor(10.0f, 9.0f);
		ImGui::Text(label);
		ImGui::NextColumn();
		ShiftCursorY(4.0f);
		ImGui::PushItemWidth(-1);

		bool modified = UI::DragUInt64(fmt::format("##{0}", label).c_str(), &value, 1.0f, minValue, maxValue);

		if (!IsItemDisabled())
			DrawItemActivityOutline(2.0f, true, Colours::Theme::accent);

		ImGui::PopItemWidth();
		ImGui::NextColumn();
		Draw::Underline();

		return modified;
	}

	static bool PropertyRadio(const char* label, int& chosen, const std::map<int, const std::string_view>& options, const char* helpText = "", const std::map<int, const std::string_view>& optionHelpTexts = {})
	{
		bool modified = false;

		ShiftCursor(10.0f, 9.0f);
		ImGui::Text(label);
		if (std::strlen(helpText) != 0)
		{
			ImGui::SameLine();
			HelpMarker(helpText);
		}
		ImGui::NextColumn();
		ImGui::PushItemWidth(-1);

		for (auto [value, option] : options)
		{
			std::string radioLabel = fmt::format("{}##{}", option.data(), label);
			if (ImGui::RadioButton(radioLabel.c_str(), &chosen, value))
				modified = true;

			if (auto optionHelpText = optionHelpTexts.find(value); optionHelpText != optionHelpTexts.end())
			{
				if (std::strlen(optionHelpText->second.data()) != 0)
				{
					ImGui::SameLine();
					HelpMarker(optionHelpText->second.data());
				}
			}
		}

		ImGui::PopItemWidth();
		ImGui::NextColumn();
		Draw::Underline();

		return modified;
	}

	static bool Property(const char* label, float& value, float delta = 0.1f, float min = 0.0f, float max = 0.0f, const char* helpText = "")
	{
		ShiftCursor(10.0f, 9.0f);
		ImGui::Text(label);
		if (std::strlen(helpText) != 0)
		{
			ImGui::SameLine();
			HelpMarker(helpText);
		}
		ImGui::NextColumn();
		ShiftCursorY(4.0f);
		ImGui::PushItemWidth(-1);

		bool modified = UI::DragFloat(fmt::format("##{0}", label).c_str(), &value, delta, min, max);

		if (!IsItemDisabled())
			DrawItemActivityOutline(2.0f, true, Colours::Theme::accent);

		ImGui::PopItemWidth();
		ImGui::NextColumn();
		Draw::Underline();

		return modified;
	}

	static bool Property(const char* label, double& value, float delta = 0.1f, double min = 0.0, double max = 0.0)
	{
		ShiftCursor(10.0f, 9.0f);
		ImGui::Text(label);
		ImGui::NextColumn();
		ShiftCursorY(4.0f);
		ImGui::PushItemWidth(-1);

		bool modified = UI::DragDouble(fmt::format("##{0}", label).c_str(), &value, delta, min, max);

		if (!IsItemDisabled())
			DrawItemActivityOutline(2.0f, true, Colours::Theme::accent);

		ImGui::PopItemWidth();
		ImGui::NextColumn();
		Draw::Underline();

		return modified;
	}

	static bool Property(const char* label, glm::vec2& value, float delta = 0.1f, float min = 0.0f, float max = 0.0f)
	{
		ShiftCursor(10.0f, 9.0f);
		ImGui::Text(label);
		ImGui::NextColumn();
		ShiftCursorY(4.0f);
		ImGui::PushItemWidth(-1);

		bool modified = ImGui::DragFloat2(fmt::format("##{0}", label).c_str(), glm::value_ptr(value), delta, min, max);

		if (!IsItemDisabled())
			DrawItemActivityOutline(2.0f, true, Colours::Theme::accent);

		ImGui::PopItemWidth();
		ImGui::NextColumn();
		Draw::Underline();

		return modified;
	}

	static bool Property(const char* label, glm::vec3& value, float delta = 0.1f, float min = 0.0f, float max = 0.0f, const char* helpText = "")
	{
		ShiftCursor(10.0f, 9.0f);
		ImGui::Text(label);

		if (std::strlen(helpText) != 0)
		{
			ImGui::SameLine();
			HelpMarker(helpText);
		}

		ImGui::NextColumn();
		ShiftCursorY(4.0f);
		ImGui::PushItemWidth(-1);

		bool modified = ImGui::DragFloat3(fmt::format("##{0}", label).c_str(), glm::value_ptr(value), delta, min, max);

		if (!IsItemDisabled())
			DrawItemActivityOutline(2.0f, true, Colours::Theme::accent);

		ImGui::PopItemWidth();
		ImGui::NextColumn();
		Draw::Underline();

		return modified;
	}

	static bool Property(const char* label, glm::vec4& value, float delta = 0.1f, float min = 0.0f, float max = 0.0f)
	{
		ShiftCursor(10.0f, 9.0f);
		ImGui::Text(label);
		ImGui::NextColumn();
		ShiftCursorY(4.0f);
		ImGui::PushItemWidth(-1);

		bool modified = ImGui::DragFloat4(fmt::format("##{0}", label).c_str(), glm::value_ptr(value), delta, min, max);

		if (!IsItemDisabled())
			DrawItemActivityOutline(2.0f, true, Colours::Theme::accent);

		ImGui::PopItemWidth();
		ImGui::NextColumn();
		Draw::Underline();

		return modified;
	}

	static bool PropertySlider(const char* label, int& value, int min, int max)
	{
		ShiftCursor(10.0f, 9.0f);
		ImGui::Text(label);
		ImGui::NextColumn();
		ShiftCursorY(4.0f);
		ImGui::PushItemWidth(-1);

		bool modified = ImGui::SliderInt(GenerateID(), &value, min, max);

		if (!IsItemDisabled())
			DrawItemActivityOutline(2.0f, true, Colours::Theme::accent);

		ImGui::PopItemWidth();
		ImGui::NextColumn();
		Draw::Underline();

		return modified;
	}

	static bool PropertySlider(const char* label, float& value, float min, float max)
	{
		ShiftCursor(10.0f, 9.0f);
		ImGui::Text(label);
		ImGui::NextColumn();
		ShiftCursorY(4.0f);
		ImGui::PushItemWidth(-1);

		bool modified = ImGui::SliderFloat(GenerateID(), &value, min, max);

		if (!IsItemDisabled())
			DrawItemActivityOutline(2.0f, true, Colours::Theme::accent);

		ImGui::PopItemWidth();
		ImGui::NextColumn();
		Draw::Underline();

		return modified;
	}

	static bool PropertySlider(const char* label, glm::vec2& value, float min, float max)
	{
		ShiftCursor(10.0f, 9.0f);
		ImGui::Text(label);
		ImGui::NextColumn();
		ShiftCursorY(4.0f);
		ImGui::PushItemWidth(-1);

		bool modified = ImGui::SliderFloat2(GenerateID(), glm::value_ptr(value), min, max);

		if (!IsItemDisabled())
			DrawItemActivityOutline(2.0f, true, Colours::Theme::accent);

		ImGui::PopItemWidth();
		ImGui::NextColumn();
		Draw::Underline();

		return modified;
	}

	static bool PropertySlider(const char* label, glm::vec3& value, float min, float max)
	{
		ShiftCursor(10.0f, 9.0f);
		ImGui::Text(label);
		ImGui::NextColumn();
		ShiftCursorY(4.0f);
		ImGui::PushItemWidth(-1);

		bool modified = ImGui::SliderFloat3(GenerateID(), glm::value_ptr(value), min, max);

		if (!IsItemDisabled())
			DrawItemActivityOutline(2.0f, true, Colours::Theme::accent);

		ImGui::PopItemWidth();
		ImGui::NextColumn();
		Draw::Underline();

		return modified;
	}

	static bool PropertySlider(const char* label, glm::vec4& value, float min, float max)
	{
		ShiftCursor(10.0f, 9.0f);
		ImGui::Text(label);
		ImGui::NextColumn();
		ShiftCursorY(4.0f);
		ImGui::PushItemWidth(-1);

		bool modified = ImGui::SliderFloat4(GenerateID(), glm::value_ptr(value), min, max);

		if (!IsItemDisabled())
			DrawItemActivityOutline(2.0f, true, Colours::Theme::accent);

		ImGui::PopItemWidth();
		ImGui::NextColumn();
		Draw::Underline();

		return modified;
	}

	static bool PropertyInput(const char* label, int8_t& value, int8_t step = 1, int8_t stepFast = 1, ImGuiInputTextFlags flags = 0)
	{
		ShiftCursor(10.0f, 9.0f);
		ImGui::Text(label);
		ImGui::NextColumn();
		ShiftCursorY(4.0f);
		ImGui::PushItemWidth(-1);

		bool modified = UI::InputInt8(GenerateID(), &value, step, stepFast, flags);

		if (!IsItemDisabled())
			DrawItemActivityOutline(2.0f, true, Colours::Theme::accent);

		ImGui::PopItemWidth();
		ImGui::NextColumn();
		Draw::Underline();

		return modified;
	}

	static bool PropertyInput(const char* label, int16_t& value, int16_t step = 1, int16_t stepFast = 1, ImGuiInputTextFlags flags = 0)
	{
		ShiftCursor(10.0f, 9.0f);
		ImGui::Text(label);
		ImGui::NextColumn();
		ShiftCursorY(4.0f);
		ImGui::PushItemWidth(-1);

		bool modified = UI::InputInt16(GenerateID(), &value, step, stepFast, flags);

		if (!IsItemDisabled())
			DrawItemActivityOutline(2.0f, true, Colours::Theme::accent);

		ImGui::PopItemWidth();
		ImGui::NextColumn();
		Draw::Underline();

		return modified;
	}

	static bool PropertyInput(const char* label, int32_t& value, int32_t step = 1, int32_t stepFast = 1, ImGuiInputTextFlags flags = 0)
	{
		ShiftCursor(10.0f, 9.0f);
		ImGui::Text(label);
		ImGui::NextColumn();
		ShiftCursorY(4.0f);
		ImGui::PushItemWidth(-1);

		bool modified = UI::InputInt32(GenerateID(), &value, step, stepFast, flags);

		if (!IsItemDisabled())
			DrawItemActivityOutline(2.0f, true, Colours::Theme::accent);

		ImGui::PopItemWidth();
		ImGui::NextColumn();
		Draw::Underline();

		return modified;
	}

	static bool PropertyInput(const char* label, int64_t& value, int64_t step = 1, int64_t stepFast = 1, ImGuiInputTextFlags flags = 0)
	{
		ShiftCursor(10.0f, 9.0f);
		ImGui::Text(label);
		ImGui::NextColumn();
		ShiftCursorY(4.0f);
		ImGui::PushItemWidth(-1);

		bool modified = UI::InputInt64(GenerateID(), &value, step, stepFast, flags);

		if (!IsItemDisabled())
			DrawItemActivityOutline(2.0f, true, Colours::Theme::accent);

		ImGui::PopItemWidth();
		ImGui::NextColumn();
		Draw::Underline();

		return modified;
	}

	static bool PropertyInput(const char* label, uint8_t& value, uint8_t step = 1, uint8_t stepFast = 1, ImGuiInputTextFlags flags = 0)
	{
		ShiftCursor(10.0f, 9.0f);
		ImGui::Text(label);
		ImGui::NextColumn();
		ShiftCursorY(4.0f);
		ImGui::PushItemWidth(-1);

		bool modified = UI::InputUInt8(GenerateID(), &value, step, stepFast, flags);

		if (!IsItemDisabled())
			DrawItemActivityOutline(2.0f, true, Colours::Theme::accent);

		ImGui::PopItemWidth();
		ImGui::NextColumn();
		Draw::Underline();

		return modified;
	}

	static bool PropertyInput(const char* label, uint16_t& value, uint16_t step = 1, uint16_t stepFast = 1, ImGuiInputTextFlags flags = 0)
	{
		ShiftCursor(10.0f, 9.0f);
		ImGui::Text(label);
		ImGui::NextColumn();
		ShiftCursorY(4.0f);
		ImGui::PushItemWidth(-1);

		bool modified = UI::InputUInt16(GenerateID(), &value, step, stepFast, flags);

		if (!IsItemDisabled())
			DrawItemActivityOutline(2.0f, true, Colours::Theme::accent);

		ImGui::PopItemWidth();
		ImGui::NextColumn();
		Draw::Underline();

		return modified;
	}

	static bool PropertyInput(const char* label, uint32_t& value, uint32_t step = 1, uint32_t stepFast = 1, ImGuiInputTextFlags flags = 0)
	{
		ShiftCursor(10.0f, 9.0f);
		ImGui::Text(label);
		ImGui::NextColumn();
		ShiftCursorY(4.0f);
		ImGui::PushItemWidth(-1);

		bool modified = UI::InputUInt32(GenerateID(), &value, step, stepFast, flags);

		if (!IsItemDisabled())
			DrawItemActivityOutline(2.0f, true, Colours::Theme::accent);

		ImGui::PopItemWidth();
		ImGui::NextColumn();
		Draw::Underline();

		return modified;
	}

	static bool PropertyInput(const char* label, uint64_t& value, uint64_t step = 0, uint64_t stepFast = 0, ImGuiInputTextFlags flags = 0)
	{
		ShiftCursor(10.0f, 9.0f);
		ImGui::Text(label);
		ImGui::NextColumn();
		ShiftCursorY(4.0f);
		ImGui::PushItemWidth(-1);

		bool modified = UI::InputUInt64(GenerateID(), &value, step, stepFast, flags);

		if (!IsItemDisabled())
			DrawItemActivityOutline(2.0f, true, Colours::Theme::accent);

		ImGui::PopItemWidth();
		ImGui::NextColumn();
		Draw::Underline();

		return modified;
	}

	static bool PropertyColor(const char* label, glm::vec3& value)
	{
		ShiftCursor(10.0f, 9.0f);
		ImGui::Text(label);
		ImGui::NextColumn();
		ShiftCursorY(4.0f);
		ImGui::PushItemWidth(-1);

		bool modified = ImGui::ColorEdit3(GenerateID(), glm::value_ptr(value));

		if (!IsItemDisabled())
			DrawItemActivityOutline(2.0f, true, Colours::Theme::accent);

		ImGui::PopItemWidth();
		ImGui::NextColumn();
		Draw::Underline();

		return modified;
	}

	static bool PropertyColor(const char* label, glm::vec4& value)
	{
		ShiftCursor(10.0f, 9.0f);
		ImGui::Text(label);
		ImGui::NextColumn();
		ShiftCursorY(4.0f);
		ImGui::PushItemWidth(-1);

		bool modified = ImGui::ColorEdit4(GenerateID(), glm::value_ptr(value));

		if (!IsItemDisabled())
			DrawItemActivityOutline(2.0f, true, Colours::Theme::accent);

		ImGui::PopItemWidth();
		ImGui::NextColumn();
		Draw::Underline();

		return modified;
	}

	template<typename TEnum, typename TUnderlying = int32_t>
	static bool PropertyDropdown(const char* label, const char** options, int32_t optionCount, TEnum& selected)
	{
		TUnderlying selectedIndex = (TUnderlying)selected;

		const char* current = options[selectedIndex];
		ShiftCursor(10.0f, 9.0f);
		ImGui::Text(label);
		ImGui::NextColumn();
		ShiftCursorY(4.0f);
		ImGui::PushItemWidth(-1);

		bool modified = false;

		const std::string id = "##" + std::string(label);
		if (ImGui::BeginCombo(id.c_str(), current))
		{
			for (int i = 0; i < optionCount; i++)
			{
				const bool is_selected = (current == options[i]);
				if (ImGui::Selectable(options[i], is_selected))
				{
					current = options[i];
					selected = (TEnum)i;
					modified = true;
				}
				if (is_selected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}

		if (!IsItemDisabled())
			DrawItemActivityOutline(2.0f, true, Colours::Theme::accent);

		ImGui::PopItemWidth();
		ImGui::NextColumn();
		Draw::Underline();

		return modified;
	}

	static bool PropertyDropdown(const char* label, const char** options, int32_t optionCount, int32_t* selected)
	{
		const char* current = options[*selected];
		ShiftCursor(10.0f, 9.0f);
		ImGui::Text(label);
		ImGui::NextColumn();
		ShiftCursorY(4.0f);
		ImGui::PushItemWidth(-1);

		bool modified = false;

		const std::string id = "##" + std::string(label);
		if (ImGui::BeginCombo(id.c_str(), current))
		{
			for (int i = 0; i < optionCount; i++)
			{
				const bool is_selected = (current == options[i]);
				if (ImGui::Selectable(options[i], is_selected))
				{
					current = options[i];
					*selected = i;
					modified = true;
				}
				if (is_selected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}

		if (!IsItemDisabled())
			DrawItemActivityOutline(2.0f, true, Colours::Theme::accent);

		ImGui::PopItemWidth();
		ImGui::NextColumn();
		Draw::Underline();

		return modified;
	}

	static bool PropertyDropdownNoLabel(const char* strID, const char** options, int32_t optionCount, int32_t* selected)
	{
		const char* current = options[*selected];
		ShiftCursorY(4.0f);
		ImGui::PushItemWidth(-1);

		bool changed = false;

		const std::string id = "##" + std::string(strID);
		if (ImGui::BeginCombo(id.c_str(), current))
		{
			for (int i = 0; i < optionCount; i++)
			{
				const bool is_selected = (current == options[i]);
				if (ImGui::Selectable(options[i], is_selected))
				{
					current = options[i];
					*selected = i;
					changed = true;
				}
				if (is_selected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}

		if (!IsItemDisabled())
			DrawItemActivityOutline(2.0f, true, Colours::Theme::accent);

		ImGui::PopItemWidth();
		ImGui::NextColumn();
		Draw::Underline();

		return changed;
	}

	static bool PropertyDropdownNoLabel(const char* strID, const std::vector<std::string>& options, int32_t* selected, bool advanceColumn = true, bool fullWidth = true)
	{
		const char* current = options[*selected].c_str();
		ShiftCursorY(4.0f);

		if (fullWidth)
			ImGui::PushItemWidth(-1);

		bool changed = false;

		if (IsItemDisabled())
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);

		if ((GImGui->CurrentItemFlags & ImGuiItemFlags_MixedValue) != 0)
			current = "---";

		const std::string id = "##" + std::string(strID);
		if (ImGui::BeginCombo(id.c_str(), current))
		{
			for (int i = 0; i < options.size(); i++)
			{
				const bool is_selected = (current == options[i]);
				if (ImGui::Selectable(options[i].c_str(), is_selected))
				{
					current = options[i].c_str();
					*selected = i;
					changed = true;
				}
				if (is_selected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}

		if (IsItemDisabled())
			ImGui::PopStyleVar();

		if (!IsItemDisabled())
			DrawItemActivityOutline(2.0f, true, Colours::Theme::accent);

		if (fullWidth)
			ImGui::PopItemWidth();

		if (advanceColumn)
		{
			ImGui::NextColumn();
			Draw::Underline();
		}

		return changed;
	}

	static bool PropertyDropdown(const char* label, const std::vector<std::string>& options, int32_t optionCount, int32_t* selected)
	{
		const char* current = options[*selected].c_str();

		ShiftCursor(10.0f, 9.0f);
		ImGui::Text(label);
		ImGui::NextColumn();
		ShiftCursorY(4.0f);
		ImGui::PushItemWidth(-1);

		bool modified = false;

		const std::string id = "##" + std::string(label);
		if (ImGui::BeginCombo(id.c_str(), current))
		{
			for (int i = 0; i < optionCount; i++)
			{
				const bool is_selected = (current == options[i]);
				if (ImGui::Selectable(options[i].c_str(), is_selected))
				{
					current = options[i].c_str();
					*selected = i;
					modified = true;
				}
				if (is_selected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}

		if (!IsItemDisabled())
			DrawItemActivityOutline(2.0f, true, Colours::Theme::accent);

		ImGui::PopItemWidth();
		ImGui::NextColumn();
		Draw::Underline();

		return modified;
	}

	enum class PropertyAssetReferenceError
	{
		None = 0, InvalidMetadata
	};

	// TODO(Yan): move this somewhere better when restructuring API
	static AssetHandle s_PropertyAssetReferenceAssetHandle;

	struct PropertyAssetReferenceSettings
	{
		bool AdvanceToNextColumn = true;
		bool NoItemSpacing = false; // After label
		float WidthOffset = 0.0f;
		bool AllowMemoryOnlyAssets = false;
		ImVec4 ButtonLabelColor = ImGui::ColorConvertU32ToFloat4(Colours::Theme::text);
		ImVec4 ButtonLabelColorError = ImGui::ColorConvertU32ToFloat4(Colours::Theme::textError);
		bool ShowFullFilePath = false;
	};

	template<typename T>
	static bool PropertyAssetReference(const char* label, AssetHandle& outHandle, PropertyAssetReferenceError* outError = nullptr, const PropertyAssetReferenceSettings& settings = {})
	{
		bool modified = false;
		if (outError)
			*outError = PropertyAssetReferenceError::None;

		ShiftCursor(10.0f, 9.0f);
		ImGui::Text(label);
		ImGui::NextColumn();
		ShiftCursorY(4.0f);
		ImGui::PushItemWidth(-1);

		ImVec2 originalButtonTextAlign = ImGui::GetStyle().ButtonTextAlign;
		{
			ImGui::GetStyle().ButtonTextAlign = { 0.0f, 0.5f };
			float width = ImGui::GetContentRegionAvail().x - settings.WidthOffset;
			float itemHeight = 28.0f;

			std::string buttonText = "Null";
			bool valid = true;
			if (AssetManager::IsAssetHandleValid(outHandle))
			{
				auto object = AssetManager::GetAsset<T>(outHandle);
				valid = object && !object->IsFlagSet(AssetFlag::Invalid) && !object->IsFlagSet(AssetFlag::Missing);
				if (object && !object->IsFlagSet(AssetFlag::Missing))
				{
					if (settings.ShowFullFilePath)
						buttonText = Project::GetEditorAssetManager()->GetMetadata(outHandle).FilePath.string();
					else
						buttonText = Project::GetEditorAssetManager()->GetMetadata(outHandle).FilePath.stem().string();
				}
				else
				{
					buttonText = "Missing";
				}
			}

			if ((GImGui->CurrentItemFlags & ImGuiItemFlags_MixedValue) != 0)
				buttonText = "---";

			// PropertyAssetReference could be called multiple times in same "context"
			// and so we need a unique id for the asset search popup each time.
			// notes
			// - don't use GenerateID(), that's inviting id clashes, which would be super confusing.
			// - don't store return from GenerateLabelId in a const char* here. Because its pointing to an internal
			//   buffer which may get overwritten by the time you want to use it later on.
			std::string assetSearchPopupID = GenerateLabelID("ARSP");
			{
				UI::ScopedColour buttonLabelColor(ImGuiCol_Text, valid ? settings.ButtonLabelColor : settings.ButtonLabelColorError);
				ImGui::Button(GenerateLabelID(buttonText), { width, itemHeight });

				const bool isHovered = ImGui::IsItemHovered();

				if (isHovered)
				{
					if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
					{
						// NOTE(Peter): Ugly workaround since AssetEditorPanel includes ImGui.h (meaning ImGui.h can't include AssetEditorPanel).
						//				Will rework those includes at a later date...
						AssetEditorPanelInterface::OpenEditor(AssetManager::GetAsset<Asset>(outHandle));
					}
					else if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
					{
						ImGui::OpenPopup(assetSearchPopupID.c_str());
					}
				}
			}

			ImGui::GetStyle().ButtonTextAlign = originalButtonTextAlign;

			bool clear = false;
			if (Widgets::AssetSearchPopup(assetSearchPopupID.c_str(), T::GetStaticType(), outHandle, settings.AllowMemoryOnlyAssets, &clear))
			{
				if (clear)
					outHandle = 0;
				modified = true;
				s_PropertyAssetReferenceAssetHandle = outHandle;
			}
		}

		if (!IsItemDisabled())
		{
			if (ImGui::BeginDragDropTarget())
			{
				auto data = ImGui::AcceptDragDropPayload("asset_payload");

				if (data)
				{
					AssetHandle assetHandle = *(AssetHandle*)data->Data;
					s_PropertyAssetReferenceAssetHandle = assetHandle;
					Ref<Asset> asset = AssetManager::GetAsset<Asset>(assetHandle);
					if (asset && asset->GetAssetType() == T::GetStaticType())
					{
						outHandle = assetHandle;
						modified = true;
					}
				}
			}
		}

		if (!IsItemDisabled())
			DrawItemActivityOutline(2.0f, true, Colours::Theme::accent);

		ImGui::PopItemWidth();
		if (settings.AdvanceToNextColumn)
		{
			ImGui::NextColumn();
			Draw::Underline();
		}

		return modified;
	}

	template<AssetType... TAssetTypes>
	static bool PropertyMultiAssetReference(const char* label, AssetHandle& outHandle, PropertyAssetReferenceError* outError = nullptr, const PropertyAssetReferenceSettings& settings = PropertyAssetReferenceSettings())
	{
		bool modified = false;
		if (outError)
			*outError = PropertyAssetReferenceError::None;

		ShiftCursor(10.0f, 9.0f);
		ImGui::Text(label);
		ImGui::NextColumn();
		ShiftCursorY(4.0f);
		ImGui::PushItemWidth(-1);

		std::initializer_list<AssetType> assetTypes = { TAssetTypes... };

		ImVec2 originalButtonTextAlign = ImGui::GetStyle().ButtonTextAlign;
		{
			ImGui::GetStyle().ButtonTextAlign = { 0.0f, 0.5f };
			float width = ImGui::GetContentRegionAvail().x - settings.WidthOffset;
			float itemHeight = 28.0f;

			std::string buttonText = "Null";
			if (AssetManager::IsAssetHandleValid(outHandle))
			{
				auto object = AssetManager::GetAsset<Asset>(outHandle);
				if (object && !object->IsFlagSet(AssetFlag::Missing))
				{
					if (settings.ShowFullFilePath)
						buttonText = Project::GetEditorAssetManager()->GetMetadata(outHandle).FilePath.string();
					else
						buttonText = Project::GetEditorAssetManager()->GetMetadata(outHandle).FilePath.stem().string();
				}
				else
				{
					buttonText = "Missing";
				}
			}

			if ((GImGui->CurrentItemFlags & ImGuiItemFlags_MixedValue) != 0)
				buttonText = "---";

			// PropertyAssetReference could be called multiple times in same "context"
			// and so we need a unique id for the asset search popup each time.
			// notes
			// - don't use GenerateID(), that's inviting id clashes, which would be super confusing.
			// - don't store return from GenerateLabelId in a const char* here. Because its pointing to an internal
			//   buffer which may get overwritten by the time you want to use it later on.
			std::string assetSearchPopupID = GenerateLabelID("ARSP");

			ImGui::PushStyleColor(ImGuiCol_Text, settings.ButtonLabelColor);
			ImGui::Button(GenerateLabelID(buttonText), { width, itemHeight });

			const bool isHovered = ImGui::IsItemHovered();

			if (isHovered)
			{
				if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
				{
					// NOTE(Peter): Ugly workaround since AssetEditorPanel includes ImGui.h (meaning ImGui.h can't include AssetEditorPanel).
					//				Will rework those includes at a later date...
					AssetEditorPanelInterface::OpenEditor(AssetManager::GetAsset<Asset>(outHandle));
				}
				else if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
				{
					ImGui::OpenPopup(assetSearchPopupID.c_str());
				}
			}
			ImGui::PopStyleColor();
			ImGui::GetStyle().ButtonTextAlign = originalButtonTextAlign;

			bool clear = false;
			if (Widgets::AssetSearchPopup(assetSearchPopupID.c_str(), outHandle, &clear, "Search Assets", ImVec2{ 250.0f, 350.0f }, assetTypes))
			{
				if (clear)
					outHandle = 0;
				modified = true;
				s_PropertyAssetReferenceAssetHandle = outHandle;
			}
		}

		if (ImGui::BeginDragDropTarget())
		{
			auto data = ImGui::AcceptDragDropPayload("asset_payload");

			if (data)
			{
				AssetHandle assetHandle = *(AssetHandle*)data->Data;
				s_PropertyAssetReferenceAssetHandle = assetHandle;
				const auto& metadata = Project::GetEditorAssetManager()->GetMetadata(assetHandle);

				for (AssetType type : assetTypes)
				{
					if (metadata.Type == type)
					{
						outHandle = assetHandle;
						modified = true;
						break;
					}
				}
			}
		}

		if (!IsItemDisabled())
			DrawItemActivityOutline(2.0f, true, Colours::Theme::accent);

		ImGui::PopItemWidth();
		if (settings.AdvanceToNextColumn)
		{
			ImGui::NextColumn();
			Draw::Underline();
		}

		return modified;
	}


	template<typename TAssetType, typename TConversionType, typename Fn>
	static bool PropertyAssetReferenceWithConversion(const char* label, AssetHandle& outHandle, Fn&& conversionFunc, PropertyAssetReferenceError* outError = nullptr, const PropertyAssetReferenceSettings& settings = {})
	{
		bool succeeded = false;
		if (outError)
			*outError = PropertyAssetReferenceError::None;

		ShiftCursor(10.0f, 9.0f);
		ImGui::Text(label);
		ImGui::NextColumn();
		ShiftCursorY(4.0f);
		ImGui::PushItemWidth(-1);

		ImVec2 originalButtonTextAlign = ImGui::GetStyle().ButtonTextAlign;
		ImGui::GetStyle().ButtonTextAlign = { 0.0f, 0.5f };
		float width = ImGui::GetContentRegionAvail().x - settings.WidthOffset;
		UI::PushID();

		float itemHeight = 28.0f;

		std::string buttonText = "Null";
		bool valid = true;

		if (AssetManager::IsAssetHandleValid(outHandle))
		{
			auto object = AssetManager::GetAsset<TAssetType>(outHandle);
			valid = object && !object->IsFlagSet(AssetFlag::Invalid) && !object->IsFlagSet(AssetFlag::Missing);
			if (object && !object->IsFlagSet(AssetFlag::Missing))
			{
				buttonText = Project::GetEditorAssetManager()->GetMetadata(outHandle).FilePath.stem().string();
			}
			else
			{
				buttonText = "Missing";
			}
		}

		if ((GImGui->CurrentItemFlags & ImGuiItemFlags_MixedValue) != 0)
			buttonText = "---";

		// PropertyAssetReferenceWithConversion could be called multiple times in same "context"
		// and so we need a unique id for the asset search popup each time.
		// notes
		// - don't use GenerateID(), that's inviting id clashes, which would be super confusing.
		// - don't store return from GenerateLabelId in a const char* here. Because its pointing to an internal
		//   buffer which may get overwritten by the time you want to use it later on.
		std::string assetSearchPopupID = GenerateLabelID("ARWCSP");
		{
			UI::ScopedColour buttonLabelColor(ImGuiCol_Text, valid ? settings.ButtonLabelColor : settings.ButtonLabelColorError);
			ImGui::Button(GenerateLabelID(buttonText), { width, itemHeight });

			const bool isHovered = ImGui::IsItemHovered();

			if (isHovered)
			{
				if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
				{
					// NOTE(Peter): Ugly workaround since AssetEditorPanel includes ImGui.h (meaning ImGui.h can't include AssetEditorPanel).
					//				Will rework those includes at a later date...
					AssetEditorPanelInterface::OpenEditor(AssetManager::GetAsset<Asset>(outHandle));
				}
				else if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
				{
					ImGui::OpenPopup(assetSearchPopupID.c_str());
				}
			}
		}

		ImGui::GetStyle().ButtonTextAlign = originalButtonTextAlign;

		AssetHandle assetHandle = outHandle;

		bool clear = false;
		if (Widgets::AssetSearchPopup(assetSearchPopupID.c_str(), assetHandle, &clear, "Search Assets", ImVec2(250.0f, 350.0f), { TConversionType::GetStaticType(), TAssetType::GetStaticType() }))
		{
			if (clear)
			{
				outHandle = 0;
				assetHandle = 0;
				succeeded = true;
			}
		}
		UI::PopID();

		if (!IsItemDisabled())
		{
			if (ImGui::BeginDragDropTarget())
			{
				auto data = ImGui::AcceptDragDropPayload("asset_payload");

				if (data)
					assetHandle = *(AssetHandle*)data->Data;
			}

			if (assetHandle != outHandle && assetHandle != 0)
			{
				s_PropertyAssetReferenceAssetHandle = assetHandle;

				Ref<Asset> asset = AssetManager::GetAsset<Asset>(assetHandle);
				if (asset)
				{
					// No conversion necessary 
					if (asset->GetAssetType() == TAssetType::GetStaticType())
					{
						outHandle = assetHandle;
						succeeded = true;
					}
					// Convert
					else if (asset->GetAssetType() == TConversionType::GetStaticType())
					{
						conversionFunc(asset.As<TConversionType>());
						succeeded = false; // Must be handled by conversion function
					}
				}
				else
				{
					if (outError)
						*outError = PropertyAssetReferenceError::InvalidMetadata;
				}
			}
		}


		if (!IsItemDisabled())
			DrawItemActivityOutline(2.0f, true, Colours::Theme::accent);

		ImGui::PopItemWidth();
		ImGui::NextColumn();
		Draw::Underline();

		return succeeded;
	}

	static bool PropertyEntityReference(const char* label, UUID& entityID, Ref<Scene> currentScene)
	{
		bool receivedValidEntity = false;

		ShiftCursor(10.0f, 9.0f);
		ImGui::Text(label);
		ImGui::NextColumn();
		ShiftCursorY(4.0f);
		ImGui::PushItemWidth(-1);

		ImVec2 originalButtonTextAlign = ImGui::GetStyle().ButtonTextAlign;
		{
			ImGui::GetStyle().ButtonTextAlign = { 0.0f, 0.5f };
			float width = ImGui::GetContentRegionAvail().x;
			float itemHeight = 28.0f;

			std::string buttonText = "Null";

			Entity entity = currentScene->TryGetEntityWithUUID(entityID);
			if (entity)
				buttonText = entity.GetComponent<TagComponent>().Tag;

			if ((GImGui->CurrentItemFlags & ImGuiItemFlags_MixedValue) != 0)
				buttonText = "---";

			// PropertyEntityReference could be called multiple times in same "context"
			// and so we need a unique id for the asset search popup each time.
			// notes
			// - don't use GenerateID(), that's inviting id clashes, which would be super confusing.
			// - don't store return from GenerateLabelId in a const char* here. Because its pointing to an internal
			//   buffer which may get overwritten by the time you want to use it later on.
			std::string assetSearchPopupID = GenerateLabelID("ARSP");
			{
				UI::ScopedColour buttonLabelColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(Colours::Theme::text));
				ImGui::Button(GenerateLabelID(buttonText), { width, itemHeight });

				const bool isHovered = ImGui::IsItemHovered();
				if (isHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
					ImGui::OpenPopup(assetSearchPopupID.c_str());
			}

			ImGui::GetStyle().ButtonTextAlign = originalButtonTextAlign;

			bool clear = false;
			if (Widgets::EntitySearchPopup(assetSearchPopupID.c_str(), currentScene, entityID, &clear))
			{
				if (clear)
					entityID = 0;
				receivedValidEntity = true;
			}
		}

		if (!IsItemDisabled())
		{
			if (ImGui::BeginDragDropTarget())
			{
				auto data = ImGui::AcceptDragDropPayload("scene_entity_hierarchy");
				if (data)
				{
					entityID = *(UUID*)data->Data;
					receivedValidEntity = true;
				}

				ImGui::EndDragDropTarget();
			}
		}

		ImGui::PopItemWidth();
		ImGui::NextColumn();

		return receivedValidEntity;
	}

	/// <summary>
	/// Same as PropertyEntityReference, except you can pass in components that the entity is required to have.
	/// </summary>
	/// <typeparam name="...TComponents"></typeparam>
	/// <param name="label"></param>
	/// <param name="entity"></param>
	/// <param name="requiresAllComponents">If true, the entity <b>MUST</b> have all of the components. If false the entity must have only one</param>
	/// <returns></returns>
	template<typename... TComponents>
	static bool PropertyEntityReferenceWithComponents(const char* label, UUID& entityID, Ref<Scene> context, bool requiresAllComponents = true)
	{
		bool receivedValidEntity = false;

		ShiftCursor(10.0f, 9.0f);
		ImGui::Text(label);
		ImGui::NextColumn();
		ShiftCursorY(4.0f);
		ImGui::PushItemWidth(-1);

		ImVec2 originalButtonTextAlign = ImGui::GetStyle().ButtonTextAlign;
		{
			ImGui::GetStyle().ButtonTextAlign = { 0.0f, 0.5f };
			float width = ImGui::GetContentRegionAvail().x;
			float itemHeight = 28.0f;

			std::string buttonText = "Null";

			Entity entity = currentScene->TryGetEntityWithUUID(entityID);
			if (entity)
				buttonText = entity.GetComponent<TagComponent>().Tag;

			if ((GImGui->CurrentItemFlags & ImGuiItemFlags_MixedValue) != 0)
				buttonText = "---";

			ImGui::Button(GenerateLabelID(buttonText), { width, itemHeight });
		}
		ImGui::GetStyle().ButtonTextAlign = originalButtonTextAlign;

		if (ImGui::BeginDragDropTarget())
		{
			auto data = ImGui::AcceptDragDropPayload("scene_entity_hierarchy");
			if (data)
			{
				UUID tempID = *(UUID*)data->Data;
				Entity temp = currentScene->TryGetEntityWithUUID(tempID);

				if (requiresAllComponents)
				{
					if (temp.HasComponent<TComponents...>())
					{
						entityID = tempID;
						receivedValidEntity = true;
					}
				}
				else
				{
					if (temp.HasAny<TComponents...>())
					{
						entityID = tempID;
						receivedValidEntity = true;
					}
				}
			}

			ImGui::EndDragDropTarget();
		}

		ImGui::PopItemWidth();
		ImGui::NextColumn();

		return receivedValidEntity;
	}

	template<typename T, typename Fn>
	static bool PropertyAssetReferenceTarget(const char* label, const char* assetName, AssetHandle& outHandle, Fn&& targetFunc, const PropertyAssetReferenceSettings& settings = {})
	{
		bool modified = false;

		ShiftCursor(10.0f, 9.0f);
		ImGui::Text(label);
		ImGui::NextColumn();
		ShiftCursorY(4.0f);
		ImGui::PushItemWidth(-1);
		if (settings.NoItemSpacing)
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 0.0f, 0.0f });

		ImVec2 originalButtonTextAlign = ImGui::GetStyle().ButtonTextAlign;
		ImGui::GetStyle().ButtonTextAlign = { 0.0f, 0.5f };
		float width = ImGui::GetContentRegionAvail().x - settings.WidthOffset;
		UI::PushID();

		float itemHeight = 28.0f;

		std::string buttonText = "Null";
		bool valid = true;

		if (AssetManager::IsAssetHandleValid(outHandle))
		{
			auto source = AssetManager::GetAsset<T>(outHandle);
			valid = source && !source->IsFlagSet(AssetFlag::Invalid) && !source->IsFlagSet(AssetFlag::Missing);
			if (source && !source->IsFlagSet(AssetFlag::Missing))
			{
				if (assetName)
				{
					buttonText = assetName;
				}
				else
				{
					buttonText = Project::GetEditorAssetManager()->GetMetadata(outHandle).FilePath.stem().string();
					assetName = buttonText.c_str();
				}
			}
			else
			{
				buttonText = "Missing";
			}
		}

		if ((GImGui->CurrentItemFlags & ImGuiItemFlags_MixedValue) != 0)
			buttonText = "---";

		// PropertyAssetReferenceTarget could be called multiple times in same "context"
		// and so we need a unique id for the asset search popup each time.
		// notes
		// - don't use GenerateID(), that's inviting id clashes, which would be super confusing.
		// - don't store return from GenerateLabelId in a const char* here. Because its pointing to an internal
		//   buffer which may get overwritten by the time you want to use it later on.
		std::string assetSearchPopupID = GenerateLabelID("ARTSP");
		{
			UI::ScopedColour buttonLabelColor(ImGuiCol_Text, valid ? settings.ButtonLabelColor : settings.ButtonLabelColorError);
			ImGui::Button(GenerateLabelID(buttonText), { width, itemHeight });

			const bool isHovered = ImGui::IsItemHovered();

			if (isHovered)
			{
				if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
				{
					// NOTE(Peter): Ugly workaround since AssetEditorPanel includes ImGui.h (meaning ImGui.h can't include AssetEditorPanel).
					//				Will rework those includes at a later date...
					AssetEditorPanelInterface::OpenEditor(AssetManager::GetAsset<Asset>(outHandle));
				}
				else if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
				{
					ImGui::OpenPopup(assetSearchPopupID.c_str());
				}
			}
		}

		ImGui::GetStyle().ButtonTextAlign = originalButtonTextAlign;

		bool clear = false;
		if (Widgets::AssetSearchPopup(assetSearchPopupID.c_str(), T::GetStaticType(), outHandle, &clear))
		{
			if (clear)
				outHandle = 0;

			targetFunc(AssetManager::GetAsset<T>(outHandle));
			modified = true;
		}

		UI::PopID();

		if (!IsItemDisabled())
		{
			if (ImGui::BeginDragDropTarget())
			{
				auto data = ImGui::AcceptDragDropPayload("asset_payload");

				if (data)
				{
					AssetHandle assetHandle = *(AssetHandle*)data->Data;
					s_PropertyAssetReferenceAssetHandle = assetHandle;
					Ref<Asset> asset = AssetManager::GetAsset<Asset>(assetHandle);
					if (asset && asset->GetAssetType() == T::GetStaticType())
					{
						targetFunc(asset.As<T>());
						modified = true;
					}
				}
			}

			DrawItemActivityOutline(2.0f, true, Colours::Theme::accent);
		}

		ImGui::PopItemWidth();
		if (settings.AdvanceToNextColumn)
		{
			ImGui::NextColumn();
			Draw::Underline();
		}
		if (settings.NoItemSpacing)
			ImGui::PopStyleVar();
		return modified;
	}

	// To be used in conjunction with AssetSearchPopup. Call ImGui::OpenPopup for AssetSearchPopup if this is clicked.
	// Alternatively the click can be used to clear the reference.
	template<typename TAssetType>
	static bool AssetReferenceDropTargetButton(const char* label, Ref<TAssetType>& object, AssetType supportedType, bool& assetDropped, bool dropTargetEnabled = true)
	{
		bool clicked = false;

		UI::ScopedStyle(ImGuiStyleVar_FrameBorderSize, 0.0f);
		UI::ScopedColour bg(ImGuiCol_Button, Colours::Theme::propertyField);
		UI::ScopedColour bgH(ImGuiCol_ButtonHovered, Colours::Theme::propertyField);
		UI::ScopedColour bgA(ImGuiCol_ButtonActive, Colours::Theme::propertyField);

		UI::PushID();

		// For some reason ImGui handles button's width differently
		// So need to manually set it if the user has pushed item width
		auto* window = ImGui::GetCurrentWindow();
		const bool itemWidthChanged = !window->DC.ItemWidthStack.empty();
		const float buttonWidth = itemWidthChanged ? window->DC.ItemWidth : 0.0f;

		if (object)
		{
			if (!object->IsFlagSet(AssetFlag::Missing))
			{
				UI::ScopedColour text(ImGuiCol_Text, object->IsFlagSet(AssetFlag::Invalid) ? Colours::Theme::textError : Colours::Theme::text);
				auto assetFileName = Project::GetEditorAssetManager()->GetMetadata(object->Handle).FilePath.stem().string();

				if ((GImGui->CurrentItemFlags & ImGuiItemFlags_MixedValue) != 0)
					assetFileName = "---";

				if (ImGui::Button((char*)assetFileName.c_str(), { buttonWidth, 0.0f }))
					clicked = true;
			}
			else
			{
				if ((GImGui->CurrentItemFlags & ImGuiItemFlags_MixedValue) != 0)
				{
					UI::ScopedColour text(ImGuiCol_Text, ImVec4(0.9f, 0.4f, 0.3f, 1.0f));
					if (ImGui::Button("---", { buttonWidth, 0.0f }))
						clicked = true;
				}
				else
				{
					UI::ScopedColour text(ImGuiCol_Text, Colours::Theme::textError);
					if (ImGui::Button("Missing", { buttonWidth, 0.0f }))
						clicked = true;
				}
			}
		}
		else
		{
			if ((GImGui->CurrentItemFlags & ImGuiItemFlags_MixedValue) != 0)
			{
				UI::ScopedColour text(ImGuiCol_Text, Colours::Theme::muted);
				if (ImGui::Button("---", { buttonWidth, 0.0f }))
					clicked = true;
			}
			else
			{
				UI::ScopedColour text(ImGuiCol_Text, Colours::Theme::muted);
				if (ImGui::Button("Select Asset", { buttonWidth, 0.0f }))
					clicked = true;
			}
		}

		UI::PopID();

		if (dropTargetEnabled)
		{
			if (ImGui::BeginDragDropTarget())
			{
				auto data = ImGui::AcceptDragDropPayload("asset_payload");

				if (data)
				{
					AssetHandle assetHandle = *(AssetHandle*)data->Data;
					Ref<Asset> asset = AssetManager::GetAsset<Asset>(assetHandle);
					if (asset && asset->GetAssetType() == supportedType)
					{
						object = asset.As<TAssetType>();
						assetDropped = true;
					}
				}
			}
		}

		return clicked;
	}

	static bool DrawComboPreview(const char* preview, float width = 100.0f)
	{
		bool pressed = false;

		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
		ImGui::BeginHorizontal("horizontal_node_layout", ImVec2(width, 0.0f));
		ImGui::PushItemWidth(90.0f);
		ImGui::InputText("##selected_asset", (char*)preview, 512, ImGuiInputTextFlags_ReadOnly);
		pressed = ImGui::IsItemClicked();
		ImGui::PopItemWidth();

		ImGui::PushItemWidth(10.0f);
		pressed = ImGui::ArrowButton("combo_preview_button", ImGuiDir_Down) || pressed;
		ImGui::PopItemWidth();

		ImGui::EndHorizontal();
		ImGui::PopStyleVar();

		return pressed;
	}

	template<typename TAssetType>
	static bool PropertyAssetDropdown(const char* label, Ref<TAssetType>& object, const ImVec2& size, AssetHandle* selected)
	{
		bool modified = false;
		std::string preview;
		float itemHeight = size.y / 10.0f;

		if (AssetManager::IsAssetHandleValid(*selected))
			object = AssetManager::GetAsset<TAssetType>(*selected);

		if (object)
		{
			if (!object->IsFlagSet(AssetFlag::Missing))
				preview = Project::GetEditorAssetManager()->GetMetadata(object->Handle).FilePath.stem().string();
			else
				preview = "Missing";
		}
		else
		{
			preview = "Null";
		}

		auto& assets = AssetManager::GetLoadedAssets();
		AssetHandle current = *selected;

		ImGui::SetNextWindowSize(size);
		if (UI::BeginPopup(label, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize))
		{
			ImGui::SetKeyboardFocusHere(0);

			for (auto& [handle, asset] : assets)
			{
				if (asset->GetAssetType() != TAssetType::GetStaticType())
					continue;

				auto& metadata = Project::GetEditorAssetManager()->GetMetadata(handle);

				bool is_selected = (current == handle);
				if (ImGui::Selectable(metadata.FilePath.string().c_str(), is_selected))
				{
					current = handle;
					*selected = handle;
					modified = true;
				}
				if (is_selected)
					ImGui::SetItemDefaultFocus();
			}

			UI::EndPopup();
		}

		return modified;
	}

	static int s_CheckboxCount = 0;

	static void BeginCheckboxGroup(const char* label)
	{
		ImGui::Text(label);
		ImGui::NextColumn();
		ImGui::PushItemWidth(-1);
	}

	static bool PropertyCheckboxGroup(const char* label, bool& value)
	{
		bool modified = false;

		if (++s_CheckboxCount > 1)
			ImGui::SameLine();

		ImGui::Text(label);
		ImGui::SameLine();

		if (ImGui::Checkbox(GenerateID(), &value))
			modified = true;

		if (!IsItemDisabled())
			DrawItemActivityOutline(2.0f, true, Colours::Theme::accent);

		return modified;
	}

	static bool Button(const char* label, const ImVec2& size = ImVec2(0, 0))
	{
		bool result = ImGui::Button(label, size);
		ImGui::NextColumn();
		return result;
	}

	static void EndCheckboxGroup()
	{
		ImGui::PopItemWidth();
		ImGui::NextColumn();
		s_CheckboxCount = 0;
	}

	enum class FieldDisableCondition
	{
		Never, NotWritable, Always
	};

}
