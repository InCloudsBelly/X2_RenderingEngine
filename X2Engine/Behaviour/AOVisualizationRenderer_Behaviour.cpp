#include "AOVisualizationRenderer_Behaviour.h"
#include "Core/Graphic/CoreObject/GraphicInstance.h"
#include "Core/IO/Manager/AssetManager.h"
#include "Core/Graphic/Manager/LightManager.h"
#include "Core/Logic/Object/GameObject.h"
#include "Core/Logic/Component/Base/Renderer.h"
#include "Core/Graphic/Rendering/Material.h"

#include "Asset/Mesh.h"
#include "Asset/Model.h"
#include "Core/Graphic/Rendering/Shader.h"
#include "Core/Graphic/Instance/Image.h"

RTTR_REGISTRATION
{
	rttr::registration::class_<AOVisualizationRenderer_Behaviour>("AOVisualizationRenderer_Behaviour");
}


AOVisualizationRenderer_Behaviour::AOVisualizationRenderer_Behaviour(std::string modelPath)
{
	m_model = Instance::getAssetManager()->load<Model>(modelPath);
}

void AOVisualizationRenderer_Behaviour::onAwake()
{
}

void AOVisualizationRenderer_Behaviour::onStart()
{
	static int meshId = 0;
	for (auto pair : m_model->m_meshTextureMap)
	{
		GameObject* meshGo = new GameObject("MeshRenderer_" + std::to_string(meshId));
		getGameObject()->addChild(meshGo);
		meshGo->addComponent(new Renderer());
		auto renderer = meshGo->getComponent<Renderer>();

		renderer->mesh = pair.first;

		{
			shader = Instance::getAssetManager()->load<Shader>(std::string(SHADER_DIR) + "Geometry_Shader.shader");
			auto material = new Material(shader);
			material->setSampledImage2D("normalTexture", pair.second->normal, pair.second->normal->getSampler());
			renderer->addMaterial(material);
		}


		//{
		//	auto csm_casterShader = Instance::getAssetManager()->load<Shader>(std::string(SHADER_DIR) + "CSM_ShadowCaster_Shader.shader");
		//	auto csm_casterMaterial = new Material(csm_casterShader);
		//	renderer->addMaterial(csm_casterMaterial);
		//}


		meshId++;
	}
}

void AOVisualizationRenderer_Behaviour::onUpdate()
{

}

void AOVisualizationRenderer_Behaviour::onDestroy()
{
	// shader & mesh are deleted when delete Renderer.

	Instance::getAssetManager()->unload(m_model);
}

AOVisualizationRenderer_Behaviour::AOVisualizationRenderer_Behaviour()
{
}

AOVisualizationRenderer_Behaviour::~AOVisualizationRenderer_Behaviour()
{
}
