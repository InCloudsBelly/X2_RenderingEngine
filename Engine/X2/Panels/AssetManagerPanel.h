#pragma once

#include "X2/Editor/EditorPanel.h"

namespace X2 {

	class AssetManagerPanel : public EditorPanel
	{
	public:
		AssetManagerPanel() = default;
		virtual ~AssetManagerPanel() = default;

		virtual void OnImGuiRender(bool& isOpen) override;
	};

}
