#pragma once

#include "X2/ImGui/ImGuiLayer.h"
#include "VulkanRenderCommandBuffer.h"

namespace X2 {

	class VulkanImGuiLayer : public ImGuiLayer
	{
	public:
		VulkanImGuiLayer();
		VulkanImGuiLayer(const std::string& name);
		virtual ~VulkanImGuiLayer();

		virtual void Begin() override;
		virtual void End() override;

		virtual void OnAttach() override;
		virtual void OnDetach() override;
		virtual void OnImGuiRender() override;
	private:
		Ref<VulkanRenderCommandBuffer> m_RenderCommandBuffer;
		float m_Time = 0.0f;
	};

}
