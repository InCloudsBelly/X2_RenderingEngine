#include "SimpleForwardRenderer_Behaviour.h"
#include "Core/Graphic/CoreObject/GraphicInstance.h"
#include "Core/IO/Manager/AssetManager.h"
#include "Core/Logic/Object/GameObject.h"
#include "Core/Logic/Component/Base/Renderer.h"
#include "Core/Graphic/Rendering/Material.h"

#include "Asset/Mesh.h"
#include "Core/Graphic/Rendering/Shader.h"
#include "Core/Graphic/Instance/Image.h"

RTTR_REGISTRATION
{
	rttr::registration::class_<SimpleForwardRenderer_Behaviour>("SimpleForwardRenderer_Behaviour");
}


void SimpleForwardRenderer_Behaviour::onAwake()
{
}

void SimpleForwardRenderer_Behaviour::onStart()
{
	mesh = Instance::getAssetManager()->load<Mesh>(std::string(MODEL_DIR) + "default/Box.ply");
	albedoTexture = Instance::getAssetManager()->load<Image>(std::string(MODEL_DIR) + "defaultTexture/DefaultTexture.png");
	shader = Instance::getAssetManager()->load<Shader>(std::string(SHADER_DIR) + "SimpleMesh.shader");
	
	sampler = new ImageSampler(
		VkFilter::VK_FILTER_LINEAR,
		VkSamplerMipmapMode::VK_SAMPLER_MIPMAP_MODE_NEAREST,
		VkSamplerAddressMode::VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
		0.0f,
		VkBorderColor::VK_BORDER_COLOR_INT_OPAQUE_BLACK
	);

	auto material = new Material(shader);

	auto renderer = getGameObject()->getComponent<Renderer>();
	material->setSampledImage2D("albedoTexture", albedoTexture, sampler);

	renderer->addMaterial(material);
	renderer->mesh = mesh;
}

void SimpleForwardRenderer_Behaviour::onUpdate()
{

}

void SimpleForwardRenderer_Behaviour::onDestroy()
{
	// shader & mesh are deleted when delete Renderer.

	delete sampler;
	Instance::getAssetManager()->unload(albedoTexture);
}

SimpleForwardRenderer_Behaviour::SimpleForwardRenderer_Behaviour()
{
}

SimpleForwardRenderer_Behaviour::~SimpleForwardRenderer_Behaviour()
{
}
