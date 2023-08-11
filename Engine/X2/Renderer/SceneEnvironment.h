#pragma once

#include "X2/Vulkan/VulkanTexture.h"

namespace X2 {

	class Environment : public Asset
	{
	public:
		Ref<VulkanTextureCube> RawEnvMap;
		Ref<VulkanTextureCube> RadianceMap;
		Ref<VulkanTextureCube> IrradianceMap;

		Environment() = default;
		Environment(const Ref<VulkanTextureCube>& rawEnvMap, const Ref<VulkanTextureCube>& radianceMap, const Ref<VulkanTextureCube>& irradianceMap)
			: RawEnvMap(rawEnvMap),RadianceMap(radianceMap), IrradianceMap(irradianceMap) {}

		static AssetType GetStaticType() { return AssetType::EnvMap; }
		virtual AssetType GetAssetType() const override { return GetStaticType(); }

	};
}
