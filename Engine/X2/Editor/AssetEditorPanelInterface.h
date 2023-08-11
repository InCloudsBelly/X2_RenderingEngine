#pragma once

#include "X2/Asset/Asset.h"

namespace X2 {

	// NOTE: Ugly workaround since AssetEditorPanel includes ImGui.h (meaning ImGui.h can't include AssetEditorPanel).
	//				Will rework those includes at a later date...
	class AssetEditorPanelInterface
	{
	public:

		static void OpenEditor(const Ref<Asset>& asset);
	};

}
