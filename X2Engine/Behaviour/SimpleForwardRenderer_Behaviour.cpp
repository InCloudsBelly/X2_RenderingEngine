#include "SimpleForwardRenderer_Behaviour.h"
#include "Core/Graphic/CoreObject/GraphicInstance.h"
#include "Core/IO/Manager/AssetManager.h"
#include "Core/Logic/Object/GameObject.h"
#include "Core/Logic/Component/Base/Renderer.h"
#include "Core/Graphic/Rendering/Material.h"

#include "Asset/Mesh.h"
#include "Asset/Model.h"
#include "Core/Graphic/Rendering/Shader.h"
#include "Core/Graphic/Instance/Image.h"

RTTR_REGISTRATION
{
	rttr::registration::class_<SimpleForwardRenderer_Behaviour>("SimpleForwardRenderer_Behaviour");
}


SimpleForwardRenderer_Behaviour::SimpleForwardRenderer_Behaviour(std::string modelPath)
{
	m_model = Instance::getAssetManager()->load<Model>(modelPath);
}

void SimpleForwardRenderer_Behaviour::onAwake()
{
}

void SimpleForwardRenderer_Behaviour::onStart()
{
	shader = Instance::getAssetManager()->load<Shader>(std::string(SHADER_DIR) + "SimpleMesh.shader");

	sampler = new ImageSampler(
		VkFilter::VK_FILTER_LINEAR,
		VkSamplerMipmapMode::VK_SAMPLER_MIPMAP_MODE_NEAREST,
		VkSamplerAddressMode::VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
		0.0f,
		VkBorderColor::VK_BORDER_COLOR_INT_OPAQUE_BLACK
	);

	static int meshId = 0;
	for (auto pair : m_model->m_meshTextureMap)
	{
		GameObject* meshGo = new GameObject("MeshRenderer_" + std::to_string(meshId));
		getGameObject()->addChild(meshGo);
		meshGo->addComponent(new Renderer());
		auto renderer = meshGo->getComponent<Renderer>();

		auto material = new Material(shader);

		material->setSampledImage2D("albedoTexture", pair.second->albedo, sampler);
		//material->setSampledImage2D("metallicRoughnessTexture", pair.second->metallicRoughness, sampler);
		//material->setSampledImage2D("emissiveTexture", pair.second->emissive, sampler);
		//material->setSampledImage2D("ambientOcclusionTexture", pair.second->ao, sampler);
		//material->setSampledImage2D("normalTexture", pair.second->normal, sampler);

		renderer->addMaterial(material);
		renderer->mesh = pair.first;
	}
}

void SimpleForwardRenderer_Behaviour::onUpdate()
{

}

void SimpleForwardRenderer_Behaviour::onDestroy()
{
	// shader & mesh are deleted when delete Renderer.

	delete sampler;

	Instance::getAssetManager()->unload(m_model);
}

SimpleForwardRenderer_Behaviour::SimpleForwardRenderer_Behaviour()
{
}

SimpleForwardRenderer_Behaviour::~SimpleForwardRenderer_Behaviour()
{
}
