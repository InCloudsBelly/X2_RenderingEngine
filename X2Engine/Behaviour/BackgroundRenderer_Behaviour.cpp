#include "BackgroundRenderer_Behaviour.h"
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
	rttr::registration::class_<BackgroundRenderer_Behaviour>("BackgroundRenderer_Behaviour");
}


void BackgroundRenderer_Behaviour::onAwake()
{
}

void BackgroundRenderer_Behaviour::onStart()
{
	mesh = Instance::getAssetManager()->load<Mesh>(std::string(MODEL_DIR) + "default/BackgroundMesh.ply");
	shader = Instance::getAssetManager()->load<Shader>(std::string(SHADER_DIR) + "Background_Shader.shader");


	auto background = Instance::g_backgroundImage;

	sampler = new ImageSampler(
		VkFilter::VK_FILTER_LINEAR,
		VkSamplerMipmapMode::VK_SAMPLER_MIPMAP_MODE_LINEAR,
		VkSamplerAddressMode::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		0.0f,
		VkBorderColor::VK_BORDER_COLOR_INT_OPAQUE_BLACK
	);

	auto material = new Material(shader);
	//material->setSampledImageCube("backgroundTexture", background, sampler);

	auto renderer = getGameObject()->getComponent<Renderer>();
	renderer->addMaterial(material);
	renderer->mesh = mesh;
}

void BackgroundRenderer_Behaviour::onUpdate()
{

}

void BackgroundRenderer_Behaviour::onDestroy()
{
	delete sampler;
	// shader & mesh are deleted when delete Renderer.

}

BackgroundRenderer_Behaviour::BackgroundRenderer_Behaviour()
{
}

BackgroundRenderer_Behaviour::~BackgroundRenderer_Behaviour()
{
}
