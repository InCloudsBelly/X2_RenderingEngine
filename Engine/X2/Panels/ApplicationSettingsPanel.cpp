#include "Precompiled.h"
#include "ApplicationSettingsPanel.h"

#include "X2/Editor/ApplicationSettings.h"
#include "X2/Editor/EditorResources.h"

#include "X2/ImGui/ImGui.h"

namespace X2 {

	ApplicationSettingsPanel::ApplicationSettingsPanel()
	{
		m_Pages.push_back({ "X2", [this]() { DrawX2Page(); } });
		//m_Pages.push_back({ "Scripting", [this]() { DrawScriptingPage(); } });
		m_Pages.push_back({ "Renderer", [this]() { DrawRendererPage(); } });
	}

	ApplicationSettingsPanel::~ApplicationSettingsPanel()
	{
		m_Pages.clear();
	}

	void ApplicationSettingsPanel::OnImGuiRender(bool& isOpen)
	{
		if (!isOpen)
			return;

		if (ImGui::Begin("Application Settings", &isOpen))
		{
			UI::ScopedStyle spacing(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 8.0f));
			UI::ScopedStyle padding(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 4.0f));

			ImGuiTableFlags tableFlags = ImGuiTableFlags_Resizable
				| ImGuiTableFlags_SizingFixedFit
				| ImGuiTableFlags_BordersInnerV;

			UI::PushID();
			if (ImGui::BeginTable("", 2, tableFlags, ImVec2(ImGui::GetContentRegionAvail().x,
				ImGui::GetContentRegionAvail().y - 8.0f)))
			{
				ImGui::TableSetupColumn("Page List", 0, 300.0f);
				ImGui::TableSetupColumn("Page Contents", ImGuiTableColumnFlags_WidthStretch);

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				DrawPageList();

				ImGui::TableSetColumnIndex(1);
				if (m_CurrentPage < m_Pages.size())
				{
					ImGui::BeginChild("##page_contents", ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetWindowHeight()));
					m_Pages[m_CurrentPage].RenderFunction();
					ImGui::EndChild();
				}
				else
				{
					X2_CORE_ERROR_TAG("ApplicationSettingsPanel", "Invalid page selected!");
				}

				ImGui::EndTable();
			}

			UI::PopID();
		}

		ImGui::End();
	}

	void ApplicationSettingsPanel::DrawPageList()
	{
		if (!ImGui::BeginChild("##page_list"))
			return;

		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
		ImGui::PushStyleColor(ImGuiCol_Header, IM_COL32_DISABLE);
		ImGui::PushStyleColor(ImGuiCol_HeaderActive, IM_COL32_DISABLE);
		ImGui::PushStyleColor(ImGuiCol_HeaderHovered, IM_COL32_DISABLE);

		for (uint32_t i = 0; i < m_Pages.size(); i++)
		{
			UI::ShiftCursorX(ImGuiStyleVar_WindowPadding);
			const auto& page = m_Pages[i];

			const char* label = UI::GenerateLabelID(page.Name);
			bool selected = i == m_CurrentPage;

			// ImGui item height hack
			auto* window = ImGui::GetCurrentWindow();
			window->DC.CurrLineSize.y = 20.0f;
			window->DC.CurrLineTextBaseOffset = 3.0f;
			//---------------------------------------------

			const ImRect itemRect = { window->WorkRect.Min.x, window->DC.CursorPos.y,
									  window->WorkRect.Max.x, window->DC.CursorPos.y + window->DC.CurrLineSize.y };

			const bool isItemClicked = [&itemRect, &label]
			{
				if (ImGui::ItemHoverable(itemRect, ImGui::GetID(label)))
				{
					return ImGui::IsMouseDown(ImGuiMouseButton_Left) || ImGui::IsMouseReleased(ImGuiMouseButton_Left);
				}
				return false;
			}();

			const bool isWindowFocused = ImGui::IsWindowFocused();


			auto fillWithColour = [&](const ImColor& colour)
			{
				const ImU32 bgColour = ImGui::ColorConvertFloat4ToU32(colour);
				ImGui::GetWindowDrawList()->AddRectFilled(itemRect.Min, itemRect.Max, bgColour);
			};

			ImGuiTreeNodeFlags flags = (selected ? ImGuiTreeNodeFlags_Selected : 0) | ImGuiTreeNodeFlags_SpanFullWidth;

			// Fill background
			//----------------
			if (selected || isItemClicked)
			{
				if (isWindowFocused)
					fillWithColour(Colours::Theme::selection);
				else
				{
					const ImColor col = UI::ColourWithMultipliedValue(Colours::Theme::selection, 0.8f);
					fillWithColour(UI::ColourWithMultipliedSaturation(col, 0.7f));
				}

				ImGui::PushStyleColor(ImGuiCol_Text, Colours::Theme::backgroundDark);
			}
			else
			{
				ImGui::PushStyleColor(ImGuiCol_HeaderHovered, Colours::Theme::groupHeader);
			}

			if (ImGui::Selectable(label, &selected))
				m_CurrentPage = i;

			ImGui::PopStyleColor();

			UI::ShiftCursorY(3.0f);
		}

		ImGui::PopStyleColor(3);
		ImGui::PopStyleVar();

		// Draw side shadow
		ImRect windowRect = UI::RectExpanded(ImGui::GetCurrentWindow()->Rect(), 0.0f, 10.0f);
		ImGui::PushClipRect(windowRect.Min, windowRect.Max, false);
		UI::DrawShadowInner(EditorResources::ShadowTexture, 20.0f, windowRect, 1.0f, windowRect.GetHeight() / 4.0f, false, true, false, false);
		ImGui::PopClipRect();

		ImGui::EndChild();
	}

	void ApplicationSettingsPanel::DrawRendererPage()
	{
		auto& editorSettings = ApplicationSettings::Get();
		bool saveSettings = false;

		UI::BeginPropertyGrid();
		{
			ImGui::Text("Nothing here for now...");
		}
		UI::EndPropertyGrid();

		if (saveSettings)
			ApplicationSettingsSerializer::SaveSettings();
	}


	void ApplicationSettingsPanel::DrawX2Page()
	{
		auto& editorSettings = ApplicationSettings::Get();
		bool saveSettings = false;

	/*	UI::BeginPropertyGrid();
		{
			saveSettings |= UI::Property("Advanced Mode", editorSettings.AdvancedMode);
			UI::SetTooltip("Shows hidden options in which can be used to fix things.");
		}
		UI::EndPropertyGrid();*/

		if (saveSettings)
			ApplicationSettingsSerializer::SaveSettings();
	}

}
