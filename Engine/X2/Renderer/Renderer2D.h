#pragma once

#include <glm/glm.hpp>

#include "X2/Math/AABB.h"

#include "X2/Renderer/Mesh.h"

#include "X2/Vulkan/VulkanRenderPass.h"
#include "X2/Vulkan/VulkanTexture.h"
#include "X2/Vulkan/VulkanRenderCommandBuffer.h"
#include "X2/Vulkan/VulkanUniformBufferSet.h"

#include "X2/Renderer/UI/Font.h"

namespace X2 {

	struct Renderer2DSpecification
	{
		bool SwapChainTarget = false;
	};

	class Renderer2D : public RefCounted
	{
	public:
		Renderer2D(const Renderer2DSpecification& specification = Renderer2DSpecification());
		virtual ~Renderer2D();

		void Init();
		void Shutdown();

		void BeginScene(const glm::mat4& viewProj, const glm::mat4& view, bool depthTest = true);
		void EndScene();

		Ref<VulkanRenderPass> GetTargetRenderPass();
		void SetTargetRenderPass(Ref<VulkanRenderPass> renderPass);

		void OnRecreateSwapchain();

		// Primitives
		void DrawQuad(const glm::mat4& transform, const glm::vec4& color);
		void DrawQuad(const glm::mat4& transform, const Ref<VulkanTexture2D>& texture, float tilingFactor = 1.0f, const glm::vec4& tintColor = glm::vec4(1.0f), glm::vec2 uv0 = glm::vec2(0.0f), glm::vec2 uv1 = glm::vec2(1.0f));

		void DrawQuad(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color);
		void DrawQuad(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color);
		void DrawQuad(const glm::vec2& position, const glm::vec2& size, const Ref<VulkanTexture2D>& texture, float tilingFactor = 1.0f, const glm::vec4& tintColor = glm::vec4(1.0f), glm::vec2 uv0 = glm::vec2(0.0f), glm::vec2 uv1 = glm::vec2(1.0f));
		void DrawQuad(const glm::vec3& position, const glm::vec2& size, const Ref<VulkanTexture2D>& texture, float tilingFactor = 1.0f, const glm::vec4& tintColor = glm::vec4(1.0f), glm::vec2 uv0 = glm::vec2(0.0f), glm::vec2 uv1 = glm::vec2(1.0f));

		void DrawQuadBillboard(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color);
		void DrawQuadBillboard(const glm::vec3& position, const glm::vec2& size, const Ref<VulkanTexture2D>& texture, float tilingFactor = 1.0f, const glm::vec4& tintColor = glm::vec4(1.0f));

		void DrawRotatedQuad(const glm::vec2& position, const glm::vec2& size, float rotation, const glm::vec4& color);
		void DrawRotatedQuad(const glm::vec3& position, const glm::vec2& size, float rotation, const glm::vec4& color);
		void DrawRotatedQuad(const glm::vec2& position, const glm::vec2& size, float rotation, const Ref<VulkanTexture2D>& texture, float tilingFactor = 1.0f, const glm::vec4& tintColor = glm::vec4(1.0f));
		void DrawRotatedQuad(const glm::vec3& position, const glm::vec2& size, float rotation, const Ref<VulkanTexture2D>& texture, float tilingFactor = 1.0f, const glm::vec4& tintColor = glm::vec4(1.0f));

		void DrawRotatedRect(const glm::vec2& position, const glm::vec2& size, float rotation, const glm::vec4& color);
		void DrawRotatedRect(const glm::vec3& position, const glm::vec2& size, float rotation, const glm::vec4& color);

		// Thickness is between 0 and 1
		void DrawCircle(const glm::vec3& p0, const glm::vec3& rotation, float radius, const glm::vec4& color);
		void DrawCircle(const glm::mat4& transform, const glm::vec4& color);
		void FillCircle(const glm::vec2& p0, float radius, const glm::vec4& color, float thickness = 0.05f);
		void FillCircle(const glm::vec3& p0, float radius, const glm::vec4& color, float thickness = 0.05f);

		void DrawLine(const glm::vec3& p0, const glm::vec3& p1, const glm::vec4& color = glm::vec4(1.0f));

		void DrawAABB(const AABB& aabb, const glm::mat4& transform, const glm::vec4& color = glm::vec4(1.0f));
		void DrawAABB(Ref<Mesh> mesh, const glm::mat4& transform, const glm::vec4& color = glm::vec4(1.0f));

		void DrawString(const std::string& string, const glm::vec3& position, float maxWidth, const glm::vec4& color = glm::vec4(1.0f));
		void DrawString(const std::string& string, const Ref<Font>& font, const glm::vec3& position, float maxWidth, const glm::vec4& color = glm::vec4(1.0f));
		void DrawString(const std::string& string, const Ref<Font>& font, const glm::mat4& transform, float maxWidth, const glm::vec4& color = glm::vec4(1.0f), float lineHeightOffset = 0.0f, float kerningOffset = 0.0f);

		float GetLineWidth();
		void SetLineWidth(float lineWidth);

		// Stats
		struct Statistics
		{
			uint32_t DrawCalls = 0;
			uint32_t QuadCount = 0;
			uint32_t LineCount = 0;

			uint32_t GetTotalVertexCount() { return QuadCount * 4 + LineCount * 2; }
			uint32_t GetTotalIndexCount() { return QuadCount * 6 + LineCount * 2; }
		};
		void ResetStats();
		Statistics GetStats();
	private:
		void Flush();

		void FlushAndReset();
		void FlushAndResetLines();
	private:
		struct QuadVertex
		{
			glm::vec3 Position;
			glm::vec4 Color;
			glm::vec2 TexCoord;
			float TexIndex;
			float TilingFactor;
		};

		struct TextVertex
		{
			glm::vec3 Position;
			glm::vec4 Color;
			glm::vec2 TexCoord;
			float TexIndex;
		};

		struct LineVertex
		{
			glm::vec3 Position;
			glm::vec4 Color;
		};

		struct CircleVertex
		{
			glm::vec3 WorldPosition;
			float Thickness;
			glm::vec2 LocalPosition;
			glm::vec4 Color;
		};

		static const uint32_t MaxQuads = 10000;
		static const uint32_t MaxVertices = MaxQuads * 4;
		static const uint32_t MaxIndices = MaxQuads * 6;
		static const uint32_t MaxTextureSlots = 32; // TODO: RenderCaps

		static const uint32_t MaxLines = 2000;
		static const uint32_t MaxLineVertices = MaxLines * 2;
		static const uint32_t MaxLineIndices = MaxLines * 6;

		Renderer2DSpecification m_Specification;
		Ref<VulkanRenderCommandBuffer> m_RenderCommandBuffer;

		Ref<VulkanTexture2D> m_WhiteTexture;

		Ref<VulkanPipeline> m_QuadPipeline;
		std::vector<Ref<VulkanVertexBuffer>> m_QuadVertexBuffer;
		Ref<VulkanIndexBuffer> m_QuadIndexBuffer;
		Ref<VulkanMaterial> m_QuadMaterial;

		uint32_t m_QuadIndexCount = 0;
		std::vector<QuadVertex*> m_QuadVertexBufferBase;
		QuadVertex* m_QuadVertexBufferPtr;

		Ref<VulkanPipeline> m_CirclePipeline;
		Ref<VulkanMaterial> m_CircleMaterial;
		std::vector<Ref<VulkanVertexBuffer>> m_CircleVertexBuffer;
		uint32_t m_CircleIndexCount = 0;
		std::vector<CircleVertex*> m_CircleVertexBufferBase;
		CircleVertex* m_CircleVertexBufferPtr;

		std::array<Ref<VulkanTexture2D>, MaxTextureSlots> m_TextureSlots;
		uint32_t m_TextureSlotIndex = 1; // 0 = white texture

		glm::vec4 m_QuadVertexPositions[4];

		// Lines
		Ref<VulkanPipeline> m_LinePipeline;
		Ref<VulkanPipeline> m_LineOnTopPipeline;
		std::vector<Ref<VulkanVertexBuffer>> m_LineVertexBuffer;
		Ref<VulkanIndexBuffer> m_LineIndexBuffer;
		Ref<VulkanMaterial> m_LineMaterial;

		uint32_t m_LineIndexCount = 0;
		std::vector<LineVertex*> m_LineVertexBufferBase;
		LineVertex* m_LineVertexBufferPtr;

		// Text
		Ref<VulkanPipeline> m_TextPipeline;
		std::vector<Ref<VulkanVertexBuffer>> m_TextVertexBuffer;
		Ref<VulkanIndexBuffer> m_TextIndexBuffer;
		Ref<VulkanMaterial> m_TextMaterial;
		std::array<Ref<VulkanTexture2D>, MaxTextureSlots> m_FontTextureSlots;
		uint32_t m_FontTextureSlotIndex = 0;

		uint32_t m_TextIndexCount = 0;
		std::vector<TextVertex*> m_TextVertexBufferBase;
		TextVertex* m_TextVertexBufferPtr;

		glm::mat4 m_CameraViewProj;
		glm::mat4 m_CameraView;
		bool m_DepthTest = true;

		float m_LineWidth = 1.0f;


		Statistics m_Stats;

		Ref<VulkanUniformBufferSet> m_UniformBufferSet;

		struct UBCamera
		{
			glm::mat4 ViewProjection;
		};
	};

}
