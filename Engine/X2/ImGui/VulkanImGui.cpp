#include "Precompiled.h"
#include "UICore.h"

#include "X2/vULKAN/VulkanRenderer.h"

#include "X2/Vulkan/VulkanTexture.h"
#include "X2/Vulkan/VulkanImage.h"

#include "imgui/examples/imgui_impl_vulkan_with_textures.h"

namespace ImGui {
	extern bool ImageButtonEx(ImGuiID id, ImTextureID texture_id, const ImVec2& size, const ImVec2& uv0, const ImVec2& uv1, const ImVec2& padding, const ImVec4& bg_col, const ImVec4& tint_col);
}

namespace X2::UI {

	ImTextureID GetTextureID(Ref<VulkanTexture2D> texture)
	{

		Ref<VulkanTexture2D> vulkanTexture = texture;
		const VkDescriptorImageInfo& imageInfo = vulkanTexture->GetVulkanDescriptorInfo();
		if (!imageInfo.imageView)
			return nullptr;

		return ImGui_ImplVulkan_AddTexture(imageInfo.sampler, imageInfo.imageView, imageInfo.imageLayout);

		return (ImTextureID)0;
	}

	void Image(const Ref<VulkanImage2D>& image, const ImVec2& size, const ImVec2& uv0, const ImVec2& uv1, const ImVec4& tint_col, const ImVec4& border_col)
	{
		Ref<VulkanImage2D> vulkanImage = image;
		const auto& imageInfo = vulkanImage->GetImageInfo();
		if (!imageInfo.ImageView)
			return;
		const auto textureID = ImGui_ImplVulkan_AddTexture(imageInfo.Sampler, imageInfo.ImageView, vulkanImage->GetDescriptorInfo().imageLayout);
		ImGui::Image(textureID, size, uv0, uv1, tint_col, border_col);
	}

	void Image(const Ref<VulkanImage2D>& image, uint32_t imageLayer, const ImVec2& size, const ImVec2& uv0, const ImVec2& uv1, const ImVec4& tint_col, const ImVec4& border_col)
	{
			Ref<VulkanImage2D> vulkanImage = image;
			auto imageInfo = vulkanImage->GetImageInfo();
			imageInfo.ImageView = vulkanImage->GetLayerImageView(imageLayer);
			if (!imageInfo.ImageView)
				return;
			const auto textureID = ImGui_ImplVulkan_AddTexture(imageInfo.Sampler, imageInfo.ImageView, vulkanImage->GetDescriptorInfo().imageLayout);
			ImGui::Image(textureID, size, uv0, uv1, tint_col, border_col);
	}

	void ImageMip(const Ref<VulkanImage2D>& image, uint32_t mip, const ImVec2& size, const ImVec2& uv0, const ImVec2& uv1, const ImVec4& tint_col, const ImVec4& border_col)
	{
		Ref<VulkanImage2D> vulkanImage = image;
		auto imageInfo = vulkanImage->GetImageInfo();
		imageInfo.ImageView = vulkanImage->GetMipImageView(mip);
		if (!imageInfo.ImageView)
			return;

		const auto textureID = ImGui_ImplVulkan_AddTexture(imageInfo.Sampler, imageInfo.ImageView, vulkanImage->GetDescriptorInfo().imageLayout);
		ImGui::Image(textureID, size, uv0, uv1, tint_col, border_col);
	}

	void Image(const Ref<VulkanTexture2D>& texture, const ImVec2& size, const ImVec2& uv0, const ImVec2& uv1, const ImVec4& tint_col, const ImVec4& border_col)
	{
		Ref<VulkanTexture2D> vulkanTexture = texture;
		const VkDescriptorImageInfo& imageInfo = vulkanTexture->GetVulkanDescriptorInfo();
		if (!imageInfo.imageView)
			return;
		const auto textureID = ImGui_ImplVulkan_AddTexture(imageInfo.sampler, imageInfo.imageView, imageInfo.imageLayout);
		ImGui::Image(textureID, size, uv0, uv1, tint_col, border_col);
	}

	bool ImageButton(const Ref<VulkanImage2D>& image, const ImVec2& size, const ImVec2& uv0, const ImVec2& uv1, int frame_padding, const ImVec4& bg_col, const ImVec4& tint_col)
	{
		return ImageButton(nullptr, image, size, uv0, uv1, frame_padding, bg_col, tint_col);
	}

	bool ImageButton(const char* stringID, const Ref<VulkanImage2D>& image, const ImVec2& size, const ImVec2& uv0, const ImVec2& uv1, int frame_padding, const ImVec4& bg_col, const ImVec4& tint_col)
	{
		Ref<VulkanImage2D> vulkanImage = image;
		const auto& imageInfo = vulkanImage->GetImageInfo();
		if (!imageInfo.ImageView)
			return false;
		const auto textureID = ImGui_ImplVulkan_AddTexture(imageInfo.Sampler, imageInfo.ImageView, vulkanImage->GetDescriptorInfo().imageLayout);
		ImGuiID id = (ImGuiID)((((uint64_t)imageInfo.ImageView) >> 32) ^ (uint32_t)(uint64_t)imageInfo.ImageView);
		if (stringID)
		{
			const ImGuiID strID = ImGui::GetID(stringID);
			id = id ^ strID;
		}
		return ImGui::ImageButtonEx(id, textureID, size, uv0, uv1, ImVec2{ (float)frame_padding, (float)frame_padding }, bg_col, tint_col);
	}

	bool ImageButton(const Ref<VulkanTexture2D>& texture, const ImVec2& size, const ImVec2& uv0, const ImVec2& uv1, const int frame_padding, const ImVec4& bg_col, const ImVec4& tint_col)
	{
		return ImageButton(nullptr, texture, size, uv0, uv1, frame_padding, bg_col, tint_col);
	}

	bool ImageButton(const char* stringID, const Ref<VulkanTexture2D>& texture, const ImVec2& size, const ImVec2& uv0, const ImVec2& uv1, int frame_padding, const ImVec4& bg_col, const ImVec4& tint_col)
	{
		X2_CORE_VERIFY(texture);
		if (!texture)
			return false;


		Ref<VulkanTexture2D> vulkanTexture = texture;
			
		// This is technically okay, could mean that GPU just hasn't created the texture yet
		X2_CORE_VERIFY(vulkanTexture->GetImage()); 
		if (!vulkanTexture->GetImage())
			return false;

		const VkDescriptorImageInfo& imageInfo = vulkanTexture->GetVulkanDescriptorInfo();
		const auto textureID = ImGui_ImplVulkan_AddTexture(imageInfo.sampler, imageInfo.imageView, imageInfo.imageLayout);
		ImGuiID id = (ImGuiID)((((uint64_t)imageInfo.imageView) >> 32) ^ (uint32_t)(uint64_t)imageInfo.imageView);
		if (stringID)
		{
			const ImGuiID strID = ImGui::GetID(stringID);
			id = id ^ strID;
		}
		return ImGui::ImageButtonEx(id, textureID, size, uv0, uv1, ImVec2{ (float)frame_padding, (float)frame_padding }, bg_col, tint_col);
		
	}

}
