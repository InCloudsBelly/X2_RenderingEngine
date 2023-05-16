#pragma once
#include "Core/Logic/Component/Component.h"
#include <glm/glm.hpp>
#include <array>
#include <map>
#include <string>

class Mesh;

class Material;

class Buffer;

class Renderer : public Component
{
public:
	struct ObjectInfo
	{
		alignas(16) glm::mat4 model;
		alignas(16) glm::mat4 itModel;
	};
	bool enableFrustumCulling;
	Mesh* mesh;
	void refreshObjectInfo();
	Buffer* getObjectInfoBuffer();

	void addMaterial(Material* material);
	Material* getMaterial(std::string pass);
	const std::map<std::string, Material*>* getMaterials();

	Renderer();
	virtual ~Renderer();
	Renderer(const Renderer&) = delete;
	Renderer& operator=(const Renderer&) = delete;
	Renderer(Renderer&&) = delete;
	Renderer& operator=(Renderer&&) = delete;

	void onAwake()override;
	void onStart()override;
	void onUpdate()override;
	void onDestroy()override;
private:
	ObjectInfo m_objectInfo;
	Buffer* m_objectInfoBuffer;
	std::map<std::string, Material*> m_materials;
	RTTR_ENABLE(Component)

};