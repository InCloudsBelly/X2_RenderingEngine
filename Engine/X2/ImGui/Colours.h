#pragma once
#include <imgui.h>

namespace Colours
{
	static inline float Convert_sRGB_FromLinear(float theLinearValue);
	static inline float Convert_sRGB_ToLinear(float thesRGBValue);
	ImVec4 ConvertFromSRGB(ImVec4 colour);
	ImVec4 ConvertToSRGB(ImVec4 colour);

	// To experiment with editor theme live you can change these constexpr into static
	// members of a static "Theme" class and add a quick ImGui window to adjust the colour values
	namespace Theme
	{
		constexpr auto accent               = IM_COL32(236, 158, 36, 255);
		constexpr auto highlight            = IM_COL32(39, 185, 242, 255);
		constexpr auto niceBlue             = IM_COL32(83, 232, 254, 255);
		constexpr auto compliment           = IM_COL32(78, 151, 166, 255);
		constexpr auto background           = IM_COL32(36, 36, 36, 255);
		constexpr auto backgroundDark       = IM_COL32(26, 26, 26, 255);
		constexpr auto titlebar             = IM_COL32(21, 21, 21, 255);
		constexpr auto propertyField        = IM_COL32(15, 15, 15, 255);
		constexpr auto text                 = IM_COL32(192, 192, 192, 255);
		constexpr auto textBrighter         = IM_COL32(210, 210, 210, 255);
		constexpr auto textDarker           = IM_COL32(128, 128, 128, 255);
		constexpr auto textError            = IM_COL32(230, 51, 51, 255);
		constexpr auto muted                = IM_COL32(77, 77, 77, 255);
		constexpr auto groupHeader          = IM_COL32(47, 47, 47, 255);
		constexpr auto selection            = IM_COL32(237, 192, 119, 255);
		constexpr auto selectionMuted       = IM_COL32(237, 201, 142, 23);
		constexpr auto backgroundPopup      = IM_COL32(50, 50, 50, 255);
	}
}
