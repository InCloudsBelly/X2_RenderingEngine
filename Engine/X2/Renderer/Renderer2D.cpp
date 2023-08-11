#include "Precompiled.h"
#include "Renderer2D.h"

#include "X2/Renderer/Renderer.h"

#include "X2/Vulkan/VulkanPipeline.h"
#include "X2/Vulkan/VulkanShader.h"
#include "X2/Vulkan/VulkanRenderCommandBuffer.h"
#include "X2/Vulkan/VulkanRenderCommandBuffer.h"


#include <glm/gtc/matrix_transform.hpp>

#include "X2/Renderer/UI/MSDFData.h"

#include <codecvt>

namespace X2 {

	Renderer2D::Renderer2D(const Renderer2DSpecification& specification)
		: m_Specification(specification)
	{
		Init();
	}

	Renderer2D::~Renderer2D()
	{
		Shutdown();
	}

	void Renderer2D::Init()
	{
		if (m_Specification.SwapChainTarget)
			m_RenderCommandBuffer = Ref<VulkanRenderCommandBuffer>::Create("Renderer2D", true);
		else
			m_RenderCommandBuffer = Ref<VulkanRenderCommandBuffer>::Create(0, "Renderer2D");

		uint32_t framesInFlight = Renderer::GetConfig().FramesInFlight;

		FramebufferSpecification framebufferSpec;
		framebufferSpec.Attachments = { ImageFormat::RGBA32F, ImageFormat::Depth };
		framebufferSpec.Samples = 1;
		framebufferSpec.ClearColorOnLoad = false;
		framebufferSpec.ClearColor = { 0.1f, 0.5f, 0.5f, 1.0f };
		framebufferSpec.DebugName = "Renderer2D Framebuffer";

		Ref<VulkanFramebuffer> framebuffer = Ref<VulkanFramebuffer>::Create(framebufferSpec);

		RenderPassSpecification renderPassSpec;
		renderPassSpec.TargetFramebuffer = framebuffer;
		renderPassSpec.DebugName = "Renderer2D";
		Ref<VulkanRenderPass> renderPass = Ref<VulkanRenderPass>::Create(renderPassSpec);

		{
			PipelineSpecification pipelineSpecification;
			pipelineSpecification.DebugName = "Renderer2D-Quad";
			pipelineSpecification.Shader = Renderer::GetShaderLibrary()->Get("Renderer2D");
			pipelineSpecification.RenderPass = renderPass;
			pipelineSpecification.BackfaceCulling = false;
			pipelineSpecification.Layout = {
				{ ShaderDataType::Float3, "a_Position" },
				{ ShaderDataType::Float4, "a_Color" },
				{ ShaderDataType::Float2, "a_TexCoord" },
				{ ShaderDataType::Float, "a_TexIndex" },
				{ ShaderDataType::Float, "a_TilingFactor" }
			};
			m_QuadPipeline = Ref<VulkanPipeline>::Create(pipelineSpecification);

			m_QuadVertexBuffer.resize(framesInFlight);
			m_QuadVertexBufferBase.resize(framesInFlight);
			for (uint32_t i = 0; i < framesInFlight; i++)
			{
				m_QuadVertexBuffer[i] = Ref<VulkanVertexBuffer>::Create(MaxVertices * sizeof(QuadVertex));
				m_QuadVertexBufferBase[i] = hnew QuadVertex[MaxVertices];
			}

			uint32_t* quadIndices = hnew uint32_t[MaxIndices];

			uint32_t offset = 0;
			for (uint32_t i = 0; i < MaxIndices; i += 6)
			{
				quadIndices[i + 0] = offset + 0;
				quadIndices[i + 1] = offset + 1;
				quadIndices[i + 2] = offset + 2;

				quadIndices[i + 3] = offset + 2;
				quadIndices[i + 4] = offset + 3;
				quadIndices[i + 5] = offset + 0;

				offset += 4;
			}

			m_QuadIndexBuffer = Ref<VulkanIndexBuffer>::Create(quadIndices, MaxIndices);
			hdelete[] quadIndices;
		}

		m_WhiteTexture = Renderer::GetWhiteTexture();

		// Set all texture slots to 0
		m_TextureSlots[0] = m_WhiteTexture;

		m_QuadVertexPositions[0] = { -0.5f, -0.5f, 0.0f, 1.0f };
		m_QuadVertexPositions[1] = { -0.5f,  0.5f, 0.0f, 1.0f };
		m_QuadVertexPositions[2] = { 0.5f,  0.5f, 0.0f, 1.0f };
		m_QuadVertexPositions[3] = { 0.5f, -0.5f, 0.0f, 1.0f };

		// Lines
		{
			PipelineSpecification pipelineSpecification;
			pipelineSpecification.DebugName = "Renderer2D-Line";
			pipelineSpecification.Shader = Renderer::GetShaderLibrary()->Get("Renderer2D_Line");
			pipelineSpecification.RenderPass = renderPass;
			pipelineSpecification.Topology = PrimitiveTopology::Lines;
			pipelineSpecification.LineWidth = 2.0f;
			pipelineSpecification.Layout = {
				{ ShaderDataType::Float3, "a_Position" },
				{ ShaderDataType::Float4, "a_Color" }
			};
			m_LinePipeline = Ref<VulkanPipeline>::Create(pipelineSpecification);
			pipelineSpecification.DepthTest = false;
			m_LineOnTopPipeline = Ref<VulkanPipeline>::Create(pipelineSpecification);

			m_LineVertexBuffer.resize(framesInFlight);
			m_LineVertexBufferBase.resize(framesInFlight);
			for (uint32_t i = 0; i < framesInFlight; i++)
			{
				m_LineVertexBuffer[i] = Ref<VulkanVertexBuffer>::Create(MaxLineVertices * sizeof(LineVertex));
				m_LineVertexBufferBase[i] = hnew LineVertex[MaxLineVertices];
			}

			uint32_t* lineIndices = hnew uint32_t[MaxLineIndices];
			for (uint32_t i = 0; i < MaxLineIndices; i++)
				lineIndices[i] = i;

			m_LineIndexBuffer = Ref<VulkanIndexBuffer>::Create(lineIndices, MaxLineIndices);
			hdelete[] lineIndices;
		}

		// Text
		{
			PipelineSpecification pipelineSpecification;
			pipelineSpecification.DebugName = "Renderer2D-Text";
			pipelineSpecification.Shader = Renderer::GetShaderLibrary()->Get("Renderer2D_Text");
			pipelineSpecification.RenderPass = renderPass;
			pipelineSpecification.BackfaceCulling = false;
			pipelineSpecification.Layout = {
				{ ShaderDataType::Float3, "a_Position" },
				{ ShaderDataType::Float4, "a_Color" },
				{ ShaderDataType::Float2, "a_TexCoord" },
				{ ShaderDataType::Float, "a_TexIndex" }
			};

			m_TextPipeline = Ref<VulkanPipeline>::Create(pipelineSpecification);
			m_TextMaterial = Ref<VulkanMaterial>::Create(pipelineSpecification.Shader);

			m_TextVertexBuffer.resize(framesInFlight);
			m_TextVertexBufferBase.resize(framesInFlight);
			for (uint32_t i = 0; i < framesInFlight; i++)
			{
				m_TextVertexBuffer[i] = Ref<VulkanVertexBuffer>::Create(MaxVertices * sizeof(TextVertex));
				m_TextVertexBufferBase[i] = hnew TextVertex[MaxVertices];
			}

			uint32_t* textQuadIndices = hnew uint32_t[MaxIndices];

			uint32_t offset = 0;
			for (uint32_t i = 0; i < MaxIndices; i += 6)
			{
				textQuadIndices[i + 0] = offset + 0;
				textQuadIndices[i + 1] = offset + 1;
				textQuadIndices[i + 2] = offset + 2;

				textQuadIndices[i + 3] = offset + 2;
				textQuadIndices[i + 4] = offset + 3;
				textQuadIndices[i + 5] = offset + 0;

				offset += 4;
			}

			m_TextIndexBuffer = Ref<VulkanIndexBuffer>::Create(textQuadIndices, MaxIndices);
			hdelete[] textQuadIndices;
		}

		// Circles
		{
			PipelineSpecification pipelineSpecification;
			pipelineSpecification.DebugName = "Renderer2D-Circle";
			pipelineSpecification.Shader = Renderer::GetShaderLibrary()->Get("Renderer2D_Circle");
			pipelineSpecification.BackfaceCulling = false;
			pipelineSpecification.RenderPass = renderPass;
			pipelineSpecification.Layout = {
				{ ShaderDataType::Float3, "a_WorldPosition" },
				{ ShaderDataType::Float,  "a_Thickness" },
				{ ShaderDataType::Float2, "a_LocalPosition" },
				{ ShaderDataType::Float4, "a_Color" }
			};
			m_CirclePipeline = Ref<VulkanPipeline>::Create(pipelineSpecification);
			m_CircleMaterial = Ref<VulkanMaterial>::Create(pipelineSpecification.Shader);

			m_CircleVertexBuffer.resize(framesInFlight);
			m_CircleVertexBufferBase.resize(framesInFlight);
			for (uint32_t i = 0; i < framesInFlight; i++)
			{
				m_CircleVertexBuffer[i] = Ref<VulkanVertexBuffer>::Create(MaxVertices * sizeof(QuadVertex));
				m_CircleVertexBufferBase[i] = hnew CircleVertex[MaxVertices];
			}
		}

		m_UniformBufferSet = Ref<VulkanUniformBufferSet>::Create(framesInFlight);
		m_UniformBufferSet->Create(sizeof(UBCamera), 0);

		m_QuadMaterial = Ref<VulkanMaterial>::Create(m_QuadPipeline->GetSpecification().Shader, "QuadMaterial");
		m_LineMaterial = Ref<VulkanMaterial>::Create(m_LinePipeline->GetSpecification().Shader, "LineMaterial");

	}

	void Renderer2D::Shutdown()
	{
		for (auto buffer : m_QuadVertexBufferBase)
			hdelete[] buffer;

		for (auto buffer : m_TextVertexBufferBase)
			hdelete[] buffer;

		for (auto buffer : m_LineVertexBufferBase)
			hdelete[] buffer;

		for (auto buffer : m_CircleVertexBufferBase)
			hdelete[] buffer;
	}

	void Renderer2D::BeginScene(const glm::mat4& viewProj, const glm::mat4& view, bool depthTest)
	{
		uint32_t frameIndex = Renderer::GetCurrentFrameIndex();

		const bool updatedAnyShaders = Renderer::UpdateDirtyShaders();
		if (updatedAnyShaders)
		{
			// Update materials that aren't set on use.
		}
		m_CameraViewProj = viewProj;
		m_CameraView = view;
		m_DepthTest = depthTest;

		Renderer::Submit([uniformBufferSet = m_UniformBufferSet, viewProj]() mutable
			{
				uint32_t bufferIndex = Renderer::RT_GetCurrentFrameIndex();
				uniformBufferSet->Get(0, 0, bufferIndex)->RT_SetData(&viewProj, sizeof(UBCamera));
			});

		X2_CORE_TRACE_TAG("Renderer", "Renderer2D::BeginScene frame {}", frameIndex);

		m_QuadIndexCount = 0;
		m_QuadVertexBufferPtr = m_QuadVertexBufferBase[frameIndex];

		m_TextIndexCount = 0;
		m_TextVertexBufferPtr = m_TextVertexBufferBase[frameIndex];

		m_LineIndexCount = 0;
		m_LineVertexBufferPtr = m_LineVertexBufferBase[frameIndex];

		m_CircleIndexCount = 0;
		m_CircleVertexBufferPtr = m_CircleVertexBufferBase[frameIndex];

		m_TextureSlotIndex = 1;
		m_FontTextureSlotIndex = 0;

		for (uint32_t i = 1; i < m_TextureSlots.size(); i++)
			m_TextureSlots[i] = nullptr;

		for (uint32_t i = 0; i < m_FontTextureSlots.size(); i++)
			m_FontTextureSlots[i] = nullptr;
	}

	void Renderer2D::EndScene()
	{
		uint32_t frameIndex = Renderer::GetCurrentFrameIndex();

		m_RenderCommandBuffer->Begin();
		Renderer::BeginRenderPass(m_RenderCommandBuffer, m_QuadPipeline->GetSpecification().RenderPass);

		X2_CORE_TRACE_TAG("Renderer", "Renderer2D::EndScene frame {}", frameIndex);
		X2_CORE_ASSERT(m_QuadVertexBufferPtr >= m_QuadVertexBufferBase[frameIndex]);
		uint32_t dataSize = (uint32_t)((uint8_t*)m_QuadVertexBufferPtr - (uint8_t*)m_QuadVertexBufferBase[frameIndex]);
		X2_CORE_TRACE_TAG("Renderer", "Data size: {}", dataSize);
		if (dataSize)
		{
			m_QuadVertexBuffer[frameIndex]->SetData(m_QuadVertexBufferBase[frameIndex], dataSize);

			for (uint32_t i = 0; i < m_TextureSlots.size(); i++)
			{
				if (m_TextureSlots[i])
					m_QuadMaterial->Set("u_Textures", m_TextureSlots[i], i);
				else
					m_QuadMaterial->Set("u_Textures", m_WhiteTexture, i);
			}

			Renderer::RenderGeometry(m_RenderCommandBuffer, m_QuadPipeline, m_UniformBufferSet, nullptr, m_QuadMaterial, m_QuadVertexBuffer[frameIndex], m_QuadIndexBuffer, glm::mat4(1.0f), m_QuadIndexCount);

			m_Stats.DrawCalls++;
		}

		// Render text
		dataSize = (uint32_t)((uint8_t*)m_TextVertexBufferPtr - (uint8_t*)m_TextVertexBufferBase[frameIndex]);
		if (dataSize)
		{
			m_TextVertexBuffer[frameIndex]->SetData(m_TextVertexBufferBase[frameIndex], dataSize);

			for (uint32_t i = 0; i < m_FontTextureSlots.size(); i++)
			{
				if (m_FontTextureSlots[i])
					m_TextMaterial->Set("u_FontAtlases", m_FontTextureSlots[i], i);
				else
					m_TextMaterial->Set("u_FontAtlases", m_WhiteTexture, i);
			}

			Renderer::RenderGeometry(m_RenderCommandBuffer, m_TextPipeline, m_UniformBufferSet, nullptr, m_TextMaterial, m_TextVertexBuffer[frameIndex], m_TextIndexBuffer, glm::mat4(1.0f), m_TextIndexCount);

			m_Stats.DrawCalls++;
		}

		// Lines
		dataSize = (uint32_t)((uint8_t*)m_LineVertexBufferPtr - (uint8_t*)m_LineVertexBufferBase[frameIndex]);
		if (dataSize)
		{
			m_LineVertexBuffer[frameIndex]->SetData(m_LineVertexBufferBase[frameIndex], dataSize);

			Renderer::Submit([lineWidth = m_LineWidth, renderCommandBuffer = m_RenderCommandBuffer]()
				{
					uint32_t frameIndex = Renderer::RT_GetCurrentFrameIndex();
					VkCommandBuffer commandBuffer = renderCommandBuffer.As<VulkanRenderCommandBuffer>()->GetCommandBuffer(frameIndex);
					vkCmdSetLineWidth(commandBuffer, lineWidth);
				});
			Renderer::RenderGeometry(m_RenderCommandBuffer, m_LinePipeline, m_UniformBufferSet, nullptr, m_LineMaterial, m_LineVertexBuffer[frameIndex], m_LineIndexBuffer, glm::mat4(1.0f), m_LineIndexCount);

			m_Stats.DrawCalls++;
		}

		// Circles
		dataSize = (uint32_t)((uint8_t*)m_CircleVertexBufferPtr - (uint8_t*)m_CircleVertexBufferBase[frameIndex]);
		if (dataSize)
		{
			m_CircleVertexBuffer[frameIndex]->SetData(m_CircleVertexBufferBase[frameIndex], dataSize);
			Renderer::RenderGeometry(m_RenderCommandBuffer, m_CirclePipeline, m_UniformBufferSet, nullptr, m_CircleMaterial, m_CircleVertexBuffer[frameIndex], m_QuadIndexBuffer, glm::mat4(1.0f), m_CircleIndexCount);

			m_Stats.DrawCalls++;
		}

		Renderer::EndRenderPass(m_RenderCommandBuffer);
		m_RenderCommandBuffer->End();
		m_RenderCommandBuffer->Submit();

#if 0
		dataSize = (uint8_t*)m_CircleVertexBufferPtr - (uint8_t*)m_CircleVertexBufferBase;
		if (dataSize)
		{
			m_CircleVertexBuffer->SetData(m_CircleVertexBufferBase, dataSize);

			m_CircleShader->Bind();
			m_CircleShader->SetMat4("u_ViewProjection", m_CameraViewProj);

			m_CircleVertexBuffer->Bind();
			m_CirclePipeline->Bind();
			m_QuadIndexBuffer->Bind();
			Renderer::DrawIndexed(m_CircleIndexCount, PrimitiveType::Triangles, false, false);
			m_Stats.DrawCalls++;
		}
#endif
#if OLD
		Flush();
#endif
	}

	void Renderer2D::Flush()
	{
#if OLD
		// Bind textures
		for (uint32_t i = 0; i < m_TextureSlotIndex; i++)
			m_TextureSlots[i]->Bind(i);

		m_QuadVertexArray->Bind();
		Renderer::DrawIndexed(m_QuadIndexCount, false);
		m_Stats.DrawCalls++;
#endif
	}

	Ref<VulkanRenderPass> Renderer2D::GetTargetRenderPass()
	{
		return m_QuadPipeline->GetSpecification().RenderPass;
	}

	void Renderer2D::SetTargetRenderPass(Ref<VulkanRenderPass> renderPass)
	{
		if (renderPass != m_QuadPipeline->GetSpecification().RenderPass)
		{
			{
				PipelineSpecification pipelineSpecification = m_QuadPipeline->GetSpecification();
				pipelineSpecification.RenderPass = renderPass;
				m_QuadPipeline = Ref<VulkanPipeline>::Create(pipelineSpecification);
			}

			{
				PipelineSpecification pipelineSpecification = m_LinePipeline->GetSpecification();
				pipelineSpecification.RenderPass = renderPass;
				m_LinePipeline = Ref<VulkanPipeline>::Create(pipelineSpecification);
			}

			{
				PipelineSpecification pipelineSpecification = m_TextPipeline->GetSpecification();
				pipelineSpecification.RenderPass = renderPass;
				m_TextPipeline = Ref<VulkanPipeline>::Create(pipelineSpecification);
			}
		}
	}

	void Renderer2D::OnRecreateSwapchain()
	{
		if (m_Specification.SwapChainTarget)
			m_RenderCommandBuffer = Ref<VulkanRenderCommandBuffer>::Create("Renderer2D", true);
	}

	void Renderer2D::FlushAndReset()
	{
		uint32_t frameIndex = Renderer::GetCurrentFrameIndex();
		//EndScene();

		m_QuadIndexCount = 0;
		m_QuadVertexBufferPtr = m_QuadVertexBufferBase[frameIndex];

		m_TextureSlotIndex = 1;
	}

	void Renderer2D::FlushAndResetLines()
	{

	}

	void Renderer2D::DrawQuad(const glm::mat4& transform, const glm::vec4& color)
	{
		constexpr size_t quadVertexCount = 4;
		const float textureIndex = 0.0f; // White Texture
		constexpr glm::vec2 textureCoords[] = { { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f } };
		const float tilingFactor = 1.0f;

		if (m_QuadIndexCount >= MaxIndices)
			FlushAndReset();

		for (size_t i = 0; i < quadVertexCount; i++)
		{
			m_QuadVertexBufferPtr->Position = transform * m_QuadVertexPositions[i];
			m_QuadVertexBufferPtr->Color = color;
			m_QuadVertexBufferPtr->TexCoord = textureCoords[i];
			m_QuadVertexBufferPtr->TexIndex = textureIndex;
			m_QuadVertexBufferPtr->TilingFactor = tilingFactor;
			m_QuadVertexBufferPtr++;
		}

		m_QuadIndexCount += 6;

		m_Stats.QuadCount++;
	}

	void Renderer2D::DrawQuad(const glm::mat4& transform, const Ref<VulkanTexture2D>& texture, float tilingFactor, const glm::vec4& tintColor, glm::vec2 uv0, glm::vec2 uv1)
	{
		constexpr size_t quadVertexCount = 4;
		glm::vec2 textureCoords[] = { uv0, { uv1.x, uv0.y }, uv1, { uv0.x, uv1.y } };

		if (m_QuadIndexCount >= MaxIndices)
			FlushAndReset();

		float textureIndex = 0.0f;
		for (uint32_t i = 1; i < m_TextureSlotIndex; i++)
		{
			if (m_TextureSlots[i]->GetHash() == texture->GetHash())
			{
				textureIndex = (float)i;
				break;
			}
		}

		if (textureIndex == 0.0f)
		{
			if (m_TextureSlotIndex >= MaxTextureSlots)
				FlushAndReset();

			textureIndex = (float)m_TextureSlotIndex;
			m_TextureSlots[m_TextureSlotIndex] = texture;
			m_TextureSlotIndex++;
		}

		for (size_t i = 0; i < quadVertexCount; i++)
		{
			m_QuadVertexBufferPtr->Position = transform * m_QuadVertexPositions[i];
			m_QuadVertexBufferPtr->Color = tintColor;
			m_QuadVertexBufferPtr->TexCoord = textureCoords[i];
			m_QuadVertexBufferPtr->TexIndex = textureIndex;
			m_QuadVertexBufferPtr->TilingFactor = tilingFactor;
			m_QuadVertexBufferPtr++;
		}

		m_QuadIndexCount += 6;

		m_Stats.QuadCount++;
	}

	void Renderer2D::DrawQuad(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color)
	{
		DrawQuad({ position.x, position.y, 0.0f }, size, color);
	}

	void Renderer2D::DrawQuad(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color)
	{
		if (m_QuadIndexCount >= MaxIndices)
			FlushAndReset();

		const float textureIndex = 0.0f; // White Texture
		const float tilingFactor = 1.0f;

		glm::mat4 transform = glm::translate(glm::mat4(1.0f), position)
			* glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });

		m_QuadVertexBufferPtr->Position = transform * m_QuadVertexPositions[0];
		m_QuadVertexBufferPtr->Color = color;
		m_QuadVertexBufferPtr->TexCoord = { 0.0f, 0.0f };
		m_QuadVertexBufferPtr->TexIndex = textureIndex;
		m_QuadVertexBufferPtr->TilingFactor = tilingFactor;
		m_QuadVertexBufferPtr++;

		m_QuadVertexBufferPtr->Position = transform * m_QuadVertexPositions[1];
		m_QuadVertexBufferPtr->Color = color;
		m_QuadVertexBufferPtr->TexCoord = { 1.0f, 0.0f };
		m_QuadVertexBufferPtr->TexIndex = textureIndex;
		m_QuadVertexBufferPtr->TilingFactor = tilingFactor;
		m_QuadVertexBufferPtr++;

		m_QuadVertexBufferPtr->Position = transform * m_QuadVertexPositions[2];
		m_QuadVertexBufferPtr->Color = color;
		m_QuadVertexBufferPtr->TexCoord = { 1.0f, 1.0f };
		m_QuadVertexBufferPtr->TexIndex = textureIndex;
		m_QuadVertexBufferPtr->TilingFactor = tilingFactor;
		m_QuadVertexBufferPtr++;

		m_QuadVertexBufferPtr->Position = transform * m_QuadVertexPositions[3];
		m_QuadVertexBufferPtr->Color = color;
		m_QuadVertexBufferPtr->TexCoord = { 0.0f, 1.0f };
		m_QuadVertexBufferPtr->TexIndex = textureIndex;
		m_QuadVertexBufferPtr->TilingFactor = tilingFactor;
		m_QuadVertexBufferPtr++;

		m_QuadIndexCount += 6;

		m_Stats.QuadCount++;
	}

	void Renderer2D::DrawQuad(const glm::vec2& position, const glm::vec2& size, const Ref<VulkanTexture2D>& texture, float tilingFactor, const glm::vec4& tintColor, glm::vec2 uv0, glm::vec2 uv1)
	{
		DrawQuad({ position.x, position.y, 0.0f }, size, texture, tilingFactor, tintColor, uv0, uv1);
	}

	void Renderer2D::DrawQuad(const glm::vec3& position, const glm::vec2& size, const Ref<VulkanTexture2D>& texture, float tilingFactor, const glm::vec4& tintColor, glm::vec2 uv0, glm::vec2 uv1)
	{
		if (m_QuadIndexCount >= MaxIndices)
			FlushAndReset();

		float textureIndex = 0.0f;
		for (uint32_t i = 1; i < m_TextureSlotIndex; i++)
		{
			if (*m_TextureSlots[i].Raw() == *texture.Raw())
			{
				textureIndex = (float)i;
				break;
			}
		}

		if (textureIndex == 0.0f)
		{
			textureIndex = (float)m_TextureSlotIndex;
			m_TextureSlots[m_TextureSlotIndex] = texture;
			m_TextureSlotIndex++;
		}

		glm::vec2 textureCoords[] = { uv0, { uv1.x, uv0.y }, uv1, { uv0.x, uv1.y } };

		glm::mat4 transform = glm::translate(glm::mat4(1.0f), position)
			* glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });

		m_QuadVertexBufferPtr->Position = transform * m_QuadVertexPositions[0];
		m_QuadVertexBufferPtr->Color = tintColor;
		m_QuadVertexBufferPtr->TexCoord = textureCoords[0];
		m_QuadVertexBufferPtr->TexIndex = textureIndex;
		m_QuadVertexBufferPtr->TilingFactor = tilingFactor;
		m_QuadVertexBufferPtr++;

		m_QuadVertexBufferPtr->Position = transform * m_QuadVertexPositions[1];
		m_QuadVertexBufferPtr->Color = tintColor;
		m_QuadVertexBufferPtr->TexCoord = textureCoords[1];
		m_QuadVertexBufferPtr->TexIndex = textureIndex;
		m_QuadVertexBufferPtr->TilingFactor = tilingFactor;
		m_QuadVertexBufferPtr++;

		m_QuadVertexBufferPtr->Position = transform * m_QuadVertexPositions[2];
		m_QuadVertexBufferPtr->Color = tintColor;
		m_QuadVertexBufferPtr->TexCoord = textureCoords[2];
		m_QuadVertexBufferPtr->TexIndex = textureIndex;
		m_QuadVertexBufferPtr->TilingFactor = tilingFactor;
		m_QuadVertexBufferPtr++;

		m_QuadVertexBufferPtr->Position = transform * m_QuadVertexPositions[3];
		m_QuadVertexBufferPtr->Color = tintColor;
		m_QuadVertexBufferPtr->TexCoord = textureCoords[3];
		m_QuadVertexBufferPtr->TexIndex = textureIndex;
		m_QuadVertexBufferPtr->TilingFactor = tilingFactor;
		m_QuadVertexBufferPtr++;

		m_QuadIndexCount += 6;

		m_Stats.QuadCount++;
	}

	void Renderer2D::DrawQuadBillboard(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color)
	{
		if (m_QuadIndexCount >= MaxIndices)
			FlushAndReset();

		const float textureIndex = 0.0f; // White Texture
		const float tilingFactor = 1.0f;

		glm::vec3 camRightWS = { m_CameraView[0][0], m_CameraView[1][0], m_CameraView[2][0] };
		glm::vec3 camUpWS = { m_CameraView[0][1], m_CameraView[1][1], m_CameraView[2][1] };

		m_QuadVertexBufferPtr->Position = position + camRightWS * (m_QuadVertexPositions[0].x) * size.x + camUpWS * m_QuadVertexPositions[0].y * size.y;
		m_QuadVertexBufferPtr->Color = color;
		m_QuadVertexBufferPtr->TexCoord = { 0.0f, 0.0f };
		m_QuadVertexBufferPtr->TexIndex = textureIndex;
		m_QuadVertexBufferPtr->TilingFactor = tilingFactor;
		m_QuadVertexBufferPtr++;

		m_QuadVertexBufferPtr->Position = position + camRightWS * m_QuadVertexPositions[1].x * size.x + camUpWS * m_QuadVertexPositions[1].y * size.y;
		m_QuadVertexBufferPtr->Color = color;
		m_QuadVertexBufferPtr->TexCoord = { 1.0f, 0.0f };
		m_QuadVertexBufferPtr->TexIndex = textureIndex;
		m_QuadVertexBufferPtr->TilingFactor = tilingFactor;
		m_QuadVertexBufferPtr++;

		m_QuadVertexBufferPtr->Position = position + camRightWS * m_QuadVertexPositions[2].x * size.x + camUpWS * m_QuadVertexPositions[2].y * size.y;
		m_QuadVertexBufferPtr->Color = color;
		m_QuadVertexBufferPtr->TexCoord = { 1.0f, 1.0f };
		m_QuadVertexBufferPtr->TexIndex = textureIndex;
		m_QuadVertexBufferPtr->TilingFactor = tilingFactor;
		m_QuadVertexBufferPtr++;

		m_QuadVertexBufferPtr->Position = position + camRightWS * m_QuadVertexPositions[3].x * size.x + camUpWS * m_QuadVertexPositions[3].y * size.y;
		m_QuadVertexBufferPtr->Color = color;
		m_QuadVertexBufferPtr->TexCoord = { 0.0f, 1.0f };
		m_QuadVertexBufferPtr->TexIndex = textureIndex;
		m_QuadVertexBufferPtr->TilingFactor = tilingFactor;
		m_QuadVertexBufferPtr++;

		m_QuadIndexCount += 6;

		m_Stats.QuadCount++;
	}

	void Renderer2D::DrawQuadBillboard(const glm::vec3& position, const glm::vec2& size, const Ref<VulkanTexture2D>& texture, float tilingFactor, const glm::vec4& tintColor)
	{
		if (m_QuadIndexCount >= MaxIndices)
			FlushAndReset();

		float textureIndex = 0.0f;
		for (uint32_t i = 1; i < m_TextureSlotIndex; i++)
		{
			if (m_TextureSlots[i]->GetHash() == texture->GetHash())
			{
				textureIndex = (float)i;
				break;
			}
		}

		if (textureIndex == 0.0f)
		{
			textureIndex = (float)m_TextureSlotIndex;
			m_TextureSlots[m_TextureSlotIndex] = texture;
			m_TextureSlotIndex++;
		}

		glm::vec3 camRightWS = { m_CameraView[0][0], m_CameraView[1][0], m_CameraView[2][0] };
		glm::vec3 camUpWS = { m_CameraView[0][1], m_CameraView[1][1], m_CameraView[2][1] };

		m_QuadVertexBufferPtr->Position = position + camRightWS * (m_QuadVertexPositions[0].x) * size.x + camUpWS * m_QuadVertexPositions[0].y * size.y;
		m_QuadVertexBufferPtr->Color = tintColor;
		m_QuadVertexBufferPtr->TexCoord = { 0.0f, 1.0f };
		m_QuadVertexBufferPtr->TexIndex = textureIndex;
		m_QuadVertexBufferPtr->TilingFactor = tilingFactor;
		m_QuadVertexBufferPtr++;

		m_QuadVertexBufferPtr->Position = position + camRightWS * m_QuadVertexPositions[1].x * size.x + camUpWS * m_QuadVertexPositions[1].y * size.y;
		m_QuadVertexBufferPtr->Color = tintColor;
		m_QuadVertexBufferPtr->TexCoord = { 0.0f, 0.0f };
		m_QuadVertexBufferPtr->TexIndex = textureIndex;
		m_QuadVertexBufferPtr->TilingFactor = tilingFactor;
		m_QuadVertexBufferPtr++;

		m_QuadVertexBufferPtr->Position = position + camRightWS * m_QuadVertexPositions[2].x * size.x + camUpWS * m_QuadVertexPositions[2].y * size.y;
		m_QuadVertexBufferPtr->Color = tintColor;
		m_QuadVertexBufferPtr->TexCoord = { 1.0f, 0.0f };
		m_QuadVertexBufferPtr->TexIndex = textureIndex;
		m_QuadVertexBufferPtr->TilingFactor = tilingFactor;
		m_QuadVertexBufferPtr++;

		m_QuadVertexBufferPtr->Position = position + camRightWS * m_QuadVertexPositions[3].x * size.x + camUpWS * m_QuadVertexPositions[3].y * size.y;
		m_QuadVertexBufferPtr->Color = tintColor;
		m_QuadVertexBufferPtr->TexCoord = { 1.0f, 1.0f };
		m_QuadVertexBufferPtr->TexIndex = textureIndex;
		m_QuadVertexBufferPtr->TilingFactor = tilingFactor;
		m_QuadVertexBufferPtr++;

		m_QuadIndexCount += 6;

		m_Stats.QuadCount++;
	}

	void Renderer2D::DrawRotatedQuad(const glm::vec2& position, const glm::vec2& size, float rotation, const glm::vec4& color)
	{
		DrawRotatedQuad({ position.x, position.y, 0.0f }, size, rotation, color);
	}

	void Renderer2D::DrawRotatedQuad(const glm::vec3& position, const glm::vec2& size, float rotation, const glm::vec4& color)
	{
		if (m_QuadIndexCount >= MaxIndices)
			FlushAndReset();

		const float textureIndex = 0.0f; // White Texture
		const float tilingFactor = 1.0f;

		glm::mat4 transform = glm::translate(glm::mat4(1.0f), position)
			* glm::rotate(glm::mat4(1.0f), rotation, { 0.0f, 0.0f, 1.0f })
			* glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });

		m_QuadVertexBufferPtr->Position = transform * m_QuadVertexPositions[0];
		m_QuadVertexBufferPtr->Color = color;
		m_QuadVertexBufferPtr->TexCoord = { 0.0f, 0.0f };
		m_QuadVertexBufferPtr->TexIndex = textureIndex;
		m_QuadVertexBufferPtr->TilingFactor = tilingFactor;
		m_QuadVertexBufferPtr++;

		m_QuadVertexBufferPtr->Position = transform * m_QuadVertexPositions[1];
		m_QuadVertexBufferPtr->Color = color;
		m_QuadVertexBufferPtr->TexCoord = { 1.0f, 0.0f };
		m_QuadVertexBufferPtr->TexIndex = textureIndex;
		m_QuadVertexBufferPtr->TilingFactor = tilingFactor;
		m_QuadVertexBufferPtr++;

		m_QuadVertexBufferPtr->Position = transform * m_QuadVertexPositions[2];
		m_QuadVertexBufferPtr->Color = color;
		m_QuadVertexBufferPtr->TexCoord = { 1.0f, 1.0f };
		m_QuadVertexBufferPtr->TexIndex = textureIndex;
		m_QuadVertexBufferPtr->TilingFactor = tilingFactor;
		m_QuadVertexBufferPtr++;

		m_QuadVertexBufferPtr->Position = transform * m_QuadVertexPositions[3];
		m_QuadVertexBufferPtr->Color = color;
		m_QuadVertexBufferPtr->TexCoord = { 0.0f, 1.0f };
		m_QuadVertexBufferPtr->TexIndex = textureIndex;
		m_QuadVertexBufferPtr->TilingFactor = tilingFactor;
		m_QuadVertexBufferPtr++;

		m_QuadIndexCount += 6;

		m_Stats.QuadCount++;
	}

	void Renderer2D::DrawRotatedQuad(const glm::vec2& position, const glm::vec2& size, float rotation, const Ref<VulkanTexture2D>& texture, float tilingFactor, const glm::vec4& tintColor)
	{
		DrawRotatedQuad({ position.x, position.y, 0.0f }, size, rotation, texture, tilingFactor, tintColor);
	}

	void Renderer2D::DrawRotatedQuad(const glm::vec3& position, const glm::vec2& size, float rotation, const Ref<VulkanTexture2D>& texture, float tilingFactor, const glm::vec4& tintColor)
	{
		if (m_QuadIndexCount >= MaxIndices)
			FlushAndReset();

		float textureIndex = 0.0f;
		for (uint32_t i = 1; i < m_TextureSlotIndex; i++)
		{
			if (m_TextureSlots[i]->GetHash() == texture->GetHash())
			{
				textureIndex = (float)i;
				break;
			}
		}

		if (textureIndex == 0.0f)
		{
			textureIndex = (float)m_TextureSlotIndex;
			m_TextureSlots[m_TextureSlotIndex] = texture;
			m_TextureSlotIndex++;
		}

		glm::mat4 transform = glm::translate(glm::mat4(1.0f), position)
			* glm::rotate(glm::mat4(1.0f), rotation, { 0.0f, 0.0f, 1.0f })
			* glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });

		m_QuadVertexBufferPtr->Position = transform * m_QuadVertexPositions[0];
		m_QuadVertexBufferPtr->Color = tintColor;
		m_QuadVertexBufferPtr->TexCoord = { 0.0f, 0.0f };
		m_QuadVertexBufferPtr->TexIndex = textureIndex;
		m_QuadVertexBufferPtr->TilingFactor = tilingFactor;
		m_QuadVertexBufferPtr++;

		m_QuadVertexBufferPtr->Position = transform * m_QuadVertexPositions[1];
		m_QuadVertexBufferPtr->Color = tintColor;
		m_QuadVertexBufferPtr->TexCoord = { 1.0f, 0.0f };
		m_QuadVertexBufferPtr->TexIndex = textureIndex;
		m_QuadVertexBufferPtr->TilingFactor = tilingFactor;
		m_QuadVertexBufferPtr++;

		m_QuadVertexBufferPtr->Position = transform * m_QuadVertexPositions[2];
		m_QuadVertexBufferPtr->Color = tintColor;
		m_QuadVertexBufferPtr->TexCoord = { 1.0f, 1.0f };
		m_QuadVertexBufferPtr->TexIndex = textureIndex;
		m_QuadVertexBufferPtr->TilingFactor = tilingFactor;
		m_QuadVertexBufferPtr++;

		m_QuadVertexBufferPtr->Position = transform * m_QuadVertexPositions[3];
		m_QuadVertexBufferPtr->Color = tintColor;
		m_QuadVertexBufferPtr->TexCoord = { 0.0f, 1.0f };
		m_QuadVertexBufferPtr->TexIndex = textureIndex;
		m_QuadVertexBufferPtr->TilingFactor = tilingFactor;
		m_QuadVertexBufferPtr++;

		m_QuadIndexCount += 6;

		m_Stats.QuadCount++;
	}

	void Renderer2D::DrawRotatedRect(const glm::vec2& position, const glm::vec2& size, float rotation, const glm::vec4& color)
	{
		DrawRotatedRect({ position.x, position.y, 0.0f }, size, rotation, color);
	}

	void Renderer2D::DrawRotatedRect(const glm::vec3& position, const glm::vec2& size, float rotation, const glm::vec4& color)
	{
		if (m_LineIndexCount >= MaxLineIndices)
			FlushAndResetLines();

		glm::mat4 transform = glm::translate(glm::mat4(1.0f), position)
			* glm::rotate(glm::mat4(1.0f), rotation, { 0.0f, 0.0f, 1.0f })
			* glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });

		glm::vec3 positions[4] =
		{
			transform * m_QuadVertexPositions[0],
			transform * m_QuadVertexPositions[1],
			transform * m_QuadVertexPositions[2],
			transform * m_QuadVertexPositions[3]
		};

		for (int i = 0; i < 4; i++)
		{
			auto& v0 = positions[i];
			auto& v1 = positions[(i + 1) % 4];

			m_LineVertexBufferPtr->Position = v0;
			m_LineVertexBufferPtr->Color = color;
			m_LineVertexBufferPtr++;

			m_LineVertexBufferPtr->Position = v1;
			m_LineVertexBufferPtr->Color = color;
			m_LineVertexBufferPtr++;

			m_LineIndexCount += 2;
			m_Stats.LineCount++;
		}
	}

	void Renderer2D::FillCircle(const glm::vec2& position, float radius, const glm::vec4& color, float thickness)
	{
		FillCircle({ position.x, position.y, 0.0f }, radius, color, thickness);
	}

	void Renderer2D::FillCircle(const glm::vec3& position, float radius, const glm::vec4& color, float thickness)
	{
		if (m_CircleIndexCount >= MaxIndices)
			FlushAndReset();

		glm::mat4 transform = glm::translate(glm::mat4(1.0f), position)
			* glm::scale(glm::mat4(1.0f), { radius * 2.0f, radius * 2.0f, 1.0f });

		for (int i = 0; i < 4; i++)
		{
			m_CircleVertexBufferPtr->WorldPosition = transform * m_QuadVertexPositions[i];
			m_CircleVertexBufferPtr->Thickness = thickness;
			m_CircleVertexBufferPtr->LocalPosition = m_QuadVertexPositions[i] * 2.0f;
			m_CircleVertexBufferPtr->Color = color;
			m_CircleVertexBufferPtr++;

			m_CircleIndexCount += 6;
			m_Stats.QuadCount++;
		}

	}

	void Renderer2D::DrawLine(const glm::vec3& p0, const glm::vec3& p1, const glm::vec4& color)
	{
		if (m_LineIndexCount >= MaxLineIndices)
			FlushAndResetLines();

		m_LineVertexBufferPtr->Position = p0;
		m_LineVertexBufferPtr->Color = color;
		m_LineVertexBufferPtr++;

		m_LineVertexBufferPtr->Position = p1;
		m_LineVertexBufferPtr->Color = color;
		m_LineVertexBufferPtr++;

		m_LineIndexCount += 2;

		m_Stats.LineCount++;
	}

	void Renderer2D::DrawCircle(const glm::vec3& position, const glm::vec3& rotation, float radius, const glm::vec4& color)
	{
		const glm::mat4 transform = glm::translate(glm::mat4(1.0f), position)
			* glm::rotate(glm::mat4(1.0f), rotation.x, { 1.0f, 0.0f, 0.0f })
			* glm::rotate(glm::mat4(1.0f), rotation.y, { 0.0f, 1.0f, 0.0f })
			* glm::rotate(glm::mat4(1.0f), rotation.z, { 0.0f, 0.0f, 1.0f })
			* glm::scale(glm::mat4(1.0f), glm::vec3(radius));

		DrawCircle(transform, color);
	}

	void Renderer2D::DrawCircle(const glm::mat4& transform, const glm::vec4& color)
	{
		int segments = 32;
		for (int i = 0; i < segments; i++)
		{
			float angle = 2.0f * glm::pi<float>() * (float)i / segments;
			glm::vec4 startPosition = { glm::cos(angle), glm::sin(angle), 0.0f, 1.0f };
			angle = 2.0f * glm::pi<float>() * (float)((i + 1) % segments) / segments;
			glm::vec4 endPosition = { glm::cos(angle), glm::sin(angle), 0.0f, 1.0f };

			glm::vec3 p0 = transform * startPosition;
			glm::vec3 p1 = transform * endPosition;
			DrawLine(p0, p1, color);
		}
	}

	void Renderer2D::DrawAABB(Ref<Mesh> mesh, const glm::mat4& transform, const glm::vec4& color)
	{
		const auto& meshAssetSubmeshes = mesh->GetMeshSource()->GetSubmeshes();
		auto& submeshes = mesh->GetSubmeshes();
		for (uint32_t submeshIndex : submeshes)
		{
			const Submesh& submesh = meshAssetSubmeshes[submeshIndex];
			auto& aabb = submesh.BoundingBox;
			auto aabbTransform = transform * submesh.Transform;
			DrawAABB(aabb, aabbTransform);
		}
	}

	void Renderer2D::DrawAABB(const AABB& aabb, const glm::mat4& transform, const glm::vec4& color /*= glm::vec4(1.0f)*/)
	{
		glm::vec4 min = { aabb.Min.x, aabb.Min.y, aabb.Min.z, 1.0f };
		glm::vec4 max = { aabb.Max.x, aabb.Max.y, aabb.Max.z, 1.0f };

		glm::vec4 corners[8] =
		{
			transform * glm::vec4 { aabb.Min.x, aabb.Min.y, aabb.Max.z, 1.0f },
			transform * glm::vec4 { aabb.Min.x, aabb.Max.y, aabb.Max.z, 1.0f },
			transform * glm::vec4 { aabb.Max.x, aabb.Max.y, aabb.Max.z, 1.0f },
			transform * glm::vec4 { aabb.Max.x, aabb.Min.y, aabb.Max.z, 1.0f },

			transform * glm::vec4 { aabb.Min.x, aabb.Min.y, aabb.Min.z, 1.0f },
			transform * glm::vec4 { aabb.Min.x, aabb.Max.y, aabb.Min.z, 1.0f },
			transform * glm::vec4 { aabb.Max.x, aabb.Max.y, aabb.Min.z, 1.0f },
			transform * glm::vec4 { aabb.Max.x, aabb.Min.y, aabb.Min.z, 1.0f }
		};

		for (uint32_t i = 0; i < 4; i++)
			DrawLine(corners[i], corners[(i + 1) % 4], color);

		for (uint32_t i = 0; i < 4; i++)
			DrawLine(corners[i + 4], corners[((i + 1) % 4) + 4], color);

		for (uint32_t i = 0; i < 4; i++)
			DrawLine(corners[i], corners[i + 4], color);
	}

	static bool NextLine(int index, const std::vector<int>& lines)
	{
		for (int line : lines)
		{
			if (line == index)
				return true;
		}
		return false;
	}

	void Renderer2D::DrawString(const std::string& string, const glm::vec3& position, float maxWidth, const glm::vec4& color)
	{
		// Use default font
		DrawString(string, Font::GetDefaultFont(), position, maxWidth, color);
	}

	void Renderer2D::DrawString(const std::string& string, const Ref<Font>& font, const glm::vec3& position, float maxWidth, const glm::vec4& color)
	{
		DrawString(string, font, glm::translate(glm::mat4(1.0f), position), maxWidth, color);
	}

	// warning C4996: 'std::codecvt_utf8<char32_t,1114111,(std::codecvt_mode)0>': warning STL4017: std::wbuffer_convert, std::wstring_convert, and the <codecvt> header
	// (containing std::codecvt_mode, std::codecvt_utf8, std::codecvt_utf16, and std::codecvt_utf8_utf16) are deprecated in C++17. (The std::codecvt class template is NOT deprecated.)
	// The C++ Standard doesn't provide equivalent non-deprecated functionality; consider using MultiByteToWideChar() and WideCharToMultiByte() from <Windows.h> instead.
	// You can define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING or _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS to acknowledge that you have received this warning.
#pragma warning(disable : 4996)

	// From https://stackoverflow.com/questions/31302506/stdu32string-conversion-to-from-stdstring-and-stdu16string
	static std::u32string To_UTF32(const std::string& s)
	{
		std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> conv;
		return conv.from_bytes(s);
	}

#pragma warning(default : 4996)

	void Renderer2D::DrawString(const std::string& string, const Ref<Font>& font, const glm::mat4& transform, float maxWidth, const glm::vec4& color, float lineHeightOffset, float kerningOffset)
	{
		if (string.empty())
			return;

		float textureIndex = -1.0f;

		// TODO(Yan): this isn't really ideal, but we need to iterate through UTF-8 code points
		std::u32string utf32string = To_UTF32(string);

		Ref<VulkanTexture2D> fontAtlas = font->GetFontAtlas();
		X2_CORE_ASSERT(fontAtlas);

		for (uint32_t i = 0; i < m_FontTextureSlotIndex; i++)
		{
			if (*m_FontTextureSlots[i].Raw() == *fontAtlas.Raw())
			{
				textureIndex = (float)i;
				break;
			}
		}

		if (textureIndex == -1.0f)
		{
			textureIndex = (float)m_FontTextureSlotIndex;
			m_FontTextureSlots[m_FontTextureSlotIndex] = fontAtlas;
			m_FontTextureSlotIndex++;
		}

		auto& fontGeometry = font->GetMSDFData()->FontGeometry;
		const auto& metrics = fontGeometry.getMetrics();

		// TODO(Yan): these font metrics really should be cleaned up/refactored...
		//            (this is a first pass WIP)
		std::vector<int> nextLines;
		{
			double x = 0.0;
			double fsScale = 1 / (metrics.ascenderY - metrics.descenderY);
			double y = -fsScale * metrics.ascenderY;
			int lastSpace = -1;
			for (int i = 0; i < utf32string.size(); i++)
			{
				char32_t character = utf32string[i];
				if (character == '\n')
				{
					x = 0;
					y -= fsScale * metrics.lineHeight + lineHeightOffset;
					continue;
				}

				auto glyph = fontGeometry.getGlyph(character);
				if (!glyph)
					glyph = fontGeometry.getGlyph('?');
				if (!glyph)
					continue;

				if (character != ' ')
				{
					// Calc geo
					double pl, pb, pr, pt;
					glyph->getQuadPlaneBounds(pl, pb, pr, pt);
					glm::vec2 quadMin((float)pl, (float)pb);
					glm::vec2 quadMax((float)pr, (float)pt);

					quadMin *= fsScale;
					quadMax *= fsScale;
					quadMin += glm::vec2(x, y);
					quadMax += glm::vec2(x, y);

					if (quadMax.x > maxWidth && lastSpace != -1)
					{
						i = lastSpace;
						nextLines.emplace_back(lastSpace);
						lastSpace = -1;
						x = 0;
						y -= fsScale * metrics.lineHeight + lineHeightOffset;
					}
				}
				else
				{
					lastSpace = i;
				}

				double advance = glyph->getAdvance();
				fontGeometry.getAdvance(advance, character, utf32string[i + 1]);
				x += fsScale * advance + kerningOffset;
			}
		}

		{
			double x = 0.0;
			double fsScale = 1 / (metrics.ascenderY - metrics.descenderY);
			double y = 0.0;// -fsScale * metrics.ascenderY;
			for (int i = 0; i < utf32string.size(); i++)
			{
				char32_t character = utf32string[i];
				if (character == '\n' || NextLine(i, nextLines))
				{
					x = 0;
					y -= fsScale * metrics.lineHeight + lineHeightOffset;
					continue;
				}

				auto glyph = fontGeometry.getGlyph(character);
				if (!glyph)
					glyph = fontGeometry.getGlyph('?');
				if (!glyph)
					continue;

				double l, b, r, t;
				glyph->getQuadAtlasBounds(l, b, r, t);

				double pl, pb, pr, pt;
				glyph->getQuadPlaneBounds(pl, pb, pr, pt);

				pl *= fsScale, pb *= fsScale, pr *= fsScale, pt *= fsScale;
				pl += x, pb += y, pr += x, pt += y;

				double texelWidth = 1. / fontAtlas->GetWidth();
				double texelHeight = 1. / fontAtlas->GetHeight();
				l *= texelWidth, b *= texelHeight, r *= texelWidth, t *= texelHeight;

				// ImGui::Begin("Font");
				// ImGui::Text("Size: %d, %d", m_ExampleFontSheet->GetWidth(), m_ExampleFontSheet->GetHeight());
				// UI::Image(m_ExampleFontSheet, ImVec2(m_ExampleFontSheet->GetWidth(), m_ExampleFontSheet->GetHeight()), ImVec2(0, 1), ImVec2(1, 0));
				// ImGui::End();

				m_TextVertexBufferPtr->Position = transform * glm::vec4(pl, pb, 0.0f, 1.0f);
				m_TextVertexBufferPtr->Color = color;
				m_TextVertexBufferPtr->TexCoord = { l, b };
				m_TextVertexBufferPtr->TexIndex = textureIndex;
				m_TextVertexBufferPtr++;

				m_TextVertexBufferPtr->Position = transform * glm::vec4(pl, pt, 0.0f, 1.0f);
				m_TextVertexBufferPtr->Color = color;
				m_TextVertexBufferPtr->TexCoord = { l, t };
				m_TextVertexBufferPtr->TexIndex = textureIndex;
				m_TextVertexBufferPtr++;

				m_TextVertexBufferPtr->Position = transform * glm::vec4(pr, pt, 0.0f, 1.0f);
				m_TextVertexBufferPtr->Color = color;
				m_TextVertexBufferPtr->TexCoord = { r, t };
				m_TextVertexBufferPtr->TexIndex = textureIndex;
				m_TextVertexBufferPtr++;

				m_TextVertexBufferPtr->Position = transform * glm::vec4(pr, pb, 0.0f, 1.0f);
				m_TextVertexBufferPtr->Color = color;
				m_TextVertexBufferPtr->TexCoord = { r, b };
				m_TextVertexBufferPtr->TexIndex = textureIndex;
				m_TextVertexBufferPtr++;

				m_TextIndexCount += 6;

				double advance = glyph->getAdvance();
				fontGeometry.getAdvance(advance, character, utf32string[i + 1]);
				x += fsScale * advance + kerningOffset;

				m_Stats.QuadCount++;
			}
		}

	}

	float Renderer2D::GetLineWidth()
	{
		return m_LineWidth;
	}

	void Renderer2D::SetLineWidth(float lineWidth)
	{
		m_LineWidth = lineWidth;
	}

	void Renderer2D::ResetStats()
	{
		memset(&m_Stats, 0, sizeof(Statistics));
	}

	Renderer2D::Statistics Renderer2D::GetStats()
	{
		return m_Stats;
	}

}
