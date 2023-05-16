#include "PointLight.h"
#include "Core/Logic/Object/GameObject.h"


RTTR_REGISTRATION
{
	rttr::registration::class_<PointLight>("PointLight");
}

void PointLight::setLightInfo(LightInfo& info)
{
	info.type = 2;
	info.intensity = intensity;
	info.minRange = minRange;
	info.maxRange = maxRange;
	info.extraParamter = { 0, 0, 0, 0 };
	info.position = getGameObject()->transform.getModelMatrix() * glm::vec4(0, 0, 0, 1);
	info.direction = { 0, 0, 0 };
	info.color = color;
}

void PointLight::setBoundingBoxInfo(std::array<glm::vec4, 8>& boundingBoxVertexes)
{
	auto matrix = getGameObject()->transform.getModelMatrix();
	boundingBoxVertexes[0] = matrix * glm::vec4(maxRange, maxRange, maxRange, 1);
	boundingBoxVertexes[1] = matrix * glm::vec4(maxRange, maxRange, -maxRange, 1);
	boundingBoxVertexes[2] = matrix * glm::vec4(maxRange, -maxRange, maxRange, 1);
	boundingBoxVertexes[3] = matrix * glm::vec4(maxRange, -maxRange, -maxRange, 1);
	boundingBoxVertexes[4] = matrix * glm::vec4(-maxRange, maxRange, maxRange, 1);
	boundingBoxVertexes[5] = matrix * glm::vec4(-maxRange, maxRange, -maxRange, 1);
	boundingBoxVertexes[6] = matrix * glm::vec4(-maxRange, -maxRange, maxRange, 1);
	boundingBoxVertexes[7] = matrix * glm::vec4(-maxRange, -maxRange, -maxRange, 1);
}

PointLight::PointLight()
	: LightBase(LightBase::LightType::POINT)
	, minRange(1.0f)
	, maxRange(10.0f)
{
}

PointLight::~PointLight()
{
}
