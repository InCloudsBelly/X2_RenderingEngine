#include "SimpleForwardRenderer_Behaviour.h"
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
	static int meshId = 0;
	for (auto pair : m_model->m_meshTextureMap)
	{
		GameObject* meshGo = new GameObject("MeshRenderer_" + std::to_string(meshId));
		getGameObject()->addChild(meshGo);
		meshGo->addComponent(new Renderer());
		auto renderer = meshGo->getComponent<Renderer>();

		shader = Instance::getAssetManager()->load<Shader>(std::string(SHADER_DIR) + "ForwardPBR_Shader.shader");
		auto material = new Material(shader);

		material->setSampledImage2D("albedoTexture", pair.second->albedo, pair.second->albedo->getSampler());
		material->setSampledImage2D("metallicTexture", pair.second->metallic, pair.second->metallic->getSampler());
		material->setSampledImage2D("roughnessTexture", pair.second->roughness, pair.second->roughness->getSampler());
		material->setSampledImage2D("emissiveTexture", pair.second->emissive, pair.second->emissive->getSampler());
		material->setSampledImage2D("ambientOcclusionTexture", pair.second->ao, pair.second->ao->getSampler());
		material->setSampledImage2D("normalTexture", pair.second->normal, pair.second->normal->getSampler());

		renderer->addMaterial(material);
		renderer->mesh = pair.first;

		{
			auto csm_casterShader = Instance::getAssetManager()->load<Shader>(std::string(SHADER_DIR) + "CSM_ShadowCaster_Shader.shader");
			auto csm_casterMaterial = new Material(csm_casterShader);
			renderer->addMaterial(csm_casterMaterial);
		}

		{
			auto cascaded_evsm_casterShader = Instance::getAssetManager()->load<Shader>(std::string(SHADER_DIR) + "CascadeEVSM_ShadowCaster_Shader.shader");
			auto cascaded_evsm_casterMaterial = new Material(cascaded_evsm_casterShader);
			renderer->addMaterial(cascaded_evsm_casterMaterial);
		}

		meshId++;
	}
}

void SimpleForwardRenderer_Behaviour::onUpdate()
{

}

void SimpleForwardRenderer_Behaviour::onDestroy()
{
	// shader & mesh are deleted when delete Renderer.

	Instance::getAssetManager()->unload(m_model);
}

SimpleForwardRenderer_Behaviour::SimpleForwardRenderer_Behaviour()
{
}

SimpleForwardRenderer_Behaviour::~SimpleForwardRenderer_Behaviour()
{
}
