#pragma once

#include "X2/Core/Layer.h"

namespace X2 {

	class ImGuiLayer : public Layer
	{
	public:
		virtual void Begin() = 0;
		virtual void End() = 0;

		void SetDarkThemeColors();
		void SetDarkThemeV2Colors();

		void AllowInputEvents(bool allowEvents);

		static ImGuiLayer* Create();
	};



}
