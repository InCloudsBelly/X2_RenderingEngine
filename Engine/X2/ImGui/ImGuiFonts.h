#pragma once

#include <imgui.h>

namespace X2::UI {

	struct FontConfiguration
	{
		std::string FontName;
		std::string FilePath;
		float Size = 16.0f;
		const ImWchar* GlyphRanges = nullptr;
		bool MergeWithLast = false;
	};

	class Fonts
	{
	public:
		static void Add(const FontConfiguration& config, bool isDefault = false);
		static void PushFont(const std::string& fontName);
		static void PopFont();
		static ImFont* Get(const std::string& fontName);
	};

}
