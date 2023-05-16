#include "Renderer.h"
#include <iostream>
#include "Core/Graphic/Instance/Buffer.h"
#include "Core/Logic/Object/GameObject.h"
#include "Core/Graphic/Rendering/Material.h"
#include "Core/Graphic/Rendering/Shader.h"
#include "Asset/Mesh.h"

#include "Core/Graphic/CoreObject/GraphicInstance.h"
#include "Core/IO/Manager/AssetManager.h"



RTTR_REGISTRATION
{
	rttr::registration::class_<Renderer>("Renderer");
}

void Renderer::refreshObjectInfo()
{
	ObjectInfo data = { m_gameObject->transform.getModelMatrix(), glm::transpose(glm::inverse(m_gameObject->transform.getModelMatrix())) };
	m_objectInfoBuffer->WriteData(&data, sizeof(ObjectInfo));
}

Buffer* Renderer::getObjectInfoBuffer()
{
	return m_objectInfoBuffer;
}

void Renderer::addMaterial(Material* material)
{
	m_materials[material->getShader()->getSettings()->renderPass] = material;
}

Material* Renderer::getMaterial(std::string pass)
{
	auto iterator = m_materials.find(pass);
	return iterator == std::end(m_materials) ? nullptr : iterator->second;
}

const std::map<std::string, Material*>* Renderer::getMaterials()
{
	return &m_materials;
}

Renderer::Renderer()
	: Component(ComponentType::RENDERER)
	, mesh(nullptr)
	, enableFrustumCulling(true)
	, m_objectInfo()
	, m_materials()
	, m_objectInfoBuffer(new Buffer(sizeof(ObjectInfo), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,VMA_MEMORY_USAGE_CPU_ONLY))
{
}

Renderer::~Renderer()
{
}

void Renderer::onAwake()
{
}

void Renderer::onStart()
{
}

void Renderer::onUpdate()
{
}

void Renderer::onDestroy()
{
	if (mesh != nullptr)
		Instance::getAssetManager()->unload(mesh);

	delete m_objectInfoBuffer;

	for (auto mtr : m_materials)
		delete mtr.second;
}
