#pragma once

namespace X2 {

	struct ApplicationSettings
	{
		//---------- Content Browser ------------
		bool ContentBrowserShowAssetTypes = true;
		int ContentBrowserThumbnailSize = 128;

		//---------- X2 ------------
		bool AdvancedMode = false;

		static ApplicationSettings& Get();
	};

	class ApplicationSettingsSerializer
	{
	public:
		static void Init();

		static void LoadSettings();
		static void SaveSettings();
	};

}
