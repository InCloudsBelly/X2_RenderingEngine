#pragma once

#include <string>

namespace X2 {

	namespace Tiering {

		namespace Renderer {

			enum class ShadowQualitySetting
			{
				None = 0, Low = 1, High = 2
			};

			enum class ShadowResolutionSetting
			{
				None = 0, Low = 1, Medium = 2, High = 3
			};

			enum class AmbientOcclusionTypeSetting
			{
				None = 0, HBAO = 1, GTAO = 2
			};

			struct RendererTieringSettings
			{
				float RendererScale = 1.0f;

				// Shadows
				bool EnableShadows = true;
				Renderer::ShadowQualitySetting ShadowQuality = Renderer::ShadowQualitySetting::High;
				Renderer::ShadowResolutionSetting ShadowResolution = Renderer::ShadowResolutionSetting::High;

				// Ambient Occlusion
				bool EnableAO = true;
				Renderer::AmbientOcclusionTypeSetting AOType = Renderer::AmbientOcclusionTypeSetting::HBAO;

				bool EnableBloom = true;

				// Screen-Space Reflections
				bool EnableSSR = false;
			};

			namespace Utils {

				inline const char* ShadowQualitySettingToString(ShadowQualitySetting shadowQualitySetting)
				{
					switch (shadowQualitySetting)
					{
					case ShadowQualitySetting::None: return "None";
					case ShadowQualitySetting::Low:  return "Low";
					case ShadowQualitySetting::High: return "High";
					}

					return nullptr;
				}

				inline ShadowQualitySetting ShadowQualitySettingFromString(std::string_view shadowQualitySetting)
				{
					if (shadowQualitySetting == "None") return ShadowQualitySetting::None;
					if (shadowQualitySetting == "Low")  return ShadowQualitySetting::Low;
					if (shadowQualitySetting == "High") return ShadowQualitySetting::High;

					return ShadowQualitySetting::None;
				}

				inline const char* ShadowResolutionSettingToString(ShadowResolutionSetting shadowResolutionSetting)
				{
					switch (shadowResolutionSetting)
					{
					case ShadowResolutionSetting::None:   return "None";
					case ShadowResolutionSetting::Low:    return "Low";
					case ShadowResolutionSetting::Medium: return "Medium";
					case ShadowResolutionSetting::High:   return "High";
					}

					return nullptr;
				}

				inline ShadowResolutionSetting ShadowResolutionSettingFromString(std::string_view shadowResolutionSetting)
				{
					if (shadowResolutionSetting == "None")   return ShadowResolutionSetting::None;
					if (shadowResolutionSetting == "Low")    return ShadowResolutionSetting::Low;
					if (shadowResolutionSetting == "Medium") return ShadowResolutionSetting::Medium;
					if (shadowResolutionSetting == "High")   return ShadowResolutionSetting::High;

					return ShadowResolutionSetting::None;
				}

			}
		}

		struct TieringSettings
		{
			Renderer::RendererTieringSettings RendererTS;
		};

	}

}
