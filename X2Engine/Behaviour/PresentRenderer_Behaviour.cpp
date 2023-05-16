#include "PresentRenderer_Behaviour.h"
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
	rttr::registration::class_<PresentRenderer_Behaviour>("PresentRenderer_Behaviour");
}


void PresentRenderer_Behaviour::onAwake()
{
}

void PresentRenderer_Behaviour::onStart()
{
	mesh = Instance::getAssetManager()->load<Mesh>(std::string(MODEL_DIR) + "default/BackgroundMesh.ply");
	shader = Instance::getAssetManager()->load<Shader>(std::string(SHADER_DIR) + "PresentShader.shader");


	auto material = new Material(shader);

	auto renderer = getGameObject()->getComponent<Renderer>();
	renderer->addMaterial(material);
	renderer->mesh = mesh;
}

void PresentRenderer_Behaviour::onUpdate()
{

}

void PresentRenderer_Behaviour::onDestroy()
{
	// shader & mesh are deleted when delete Renderer.
}

PresentRenderer_Behaviour::PresentRenderer_Behaviour()
{
}

PresentRenderer_Behaviour::~PresentRenderer_Behaviour()
{
}
