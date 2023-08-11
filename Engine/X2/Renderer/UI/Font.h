#pragma once

#include "X2/Vulkan/VulkanTexture.h"
#include "X2/Scene/Components.h"

#include <filesystem>

namespace X2 {

	struct MSDFData;

	class Font : public Asset
	{
	public:
		Font(const std::filesystem::path& filepath);
		Font(const std::string& name, Buffer buffer);
		virtual ~Font();

		Ref<VulkanTexture2D> GetFontAtlas() const { return m_TextureAtlas; }
		const MSDFData* GetMSDFData() const { return m_MSDFData; }

		static void Init();
		static void Shutdown();
		static Ref<Font> GetDefaultFont();
		static Ref<Font> GetFontAssetForTextComponent(const TextComponent& textComponent);

		static AssetType GetStaticType() { return AssetType::Font; }
		virtual AssetType GetAssetType() const override { return GetStaticType(); }

		const std::string& GetName() const { return m_Name; }
	private:
		void CreateAtlas(Buffer buffer);
	private:
		std::string m_Name;
		Ref<VulkanTexture2D> m_TextureAtlas;
		MSDFData* m_MSDFData = nullptr;
	private:
		static Ref<Font> s_DefaultFont;
	};


}
