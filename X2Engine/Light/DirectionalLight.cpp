#include "DirectionalLight.h"
#include "Core/Logic/Object/GameObject.h"

RTTR_REGISTRATION
{
	rttr::registration::class_<DirectionalLight>("DirectionalLight");
}

void DirectionalLight::setLightInfo(LightInfo& info)
{
	info.type = 1;
	info.intensity = intensity;
	info.minRange = 0;
	info.maxRange = 0;
	info.extraParamter = { 0, 0, 0, 0 };
	info.position = glm::vec3(getGameObject()->transform.getModelMatrix() * glm::vec4(0, 0, 0, 1));
	info.direction = glm::normalize(glm::vec3(getGameObject()->transform.getModelMatrix() * glm::vec4(0, 0, -1, 0)));
	info.color = color;
}

void DirectionalLight::setBoundingBoxInfo(std::array<glm::vec4, 8>& boundingBoxVertexes)
{
}

DirectionalLight::DirectionalLight()
	: LightBase(LightBase::LightType::DIRECTIONAL)
{
}

DirectionalLight::~DirectionalLight()
{
}
