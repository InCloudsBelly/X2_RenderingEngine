#include "SpotLight.h"
#include "Core/Logic/Object/GameObject.h"

RTTR_REGISTRATION
{
	rttr::registration::class_<SpotLight>("SpotLight");
}

void SpotLight::setLightInfo(LightInfo& info)
{
	info.type = 4;
	info.intensity = intensity;
	info.minRange = minRange;
	info.maxRange = maxRange;
	info.extraParamter = { innerAngle, outerAngle, 0, 0 };
	info.position = getGameObject()->transform.getModelMatrix() * glm::vec4(0, 0, 0, 1);
	info.direction = glm::normalize(glm::vec3(getGameObject()->transform.getModelMatrix() * glm::vec4(0, 0, -1, 0)));
	info.color = color;
}

void SpotLight::setBoundingBoxInfo(std::array<glm::vec4, 8>& boundingBoxVertexes)
{
	const float ANGLE_TO_RADIUS = 3.1415926535 / 180.0;
	float bottom = -maxRange;
	float top = outerAngle > 90 ? std::sin((outerAngle - 90) * ANGLE_TO_RADIUS) * maxRange : 0;
	float width = outerAngle > 90 ? maxRange : std::sin((outerAngle - 90) * ANGLE_TO_RADIUS) * maxRange;
	auto matrix = getGameObject()->transform.getModelMatrix();

	boundingBoxVertexes[0] = matrix * glm::vec4(width, width, top, 1);
	boundingBoxVertexes[1] = matrix * glm::vec4(width, -width, top, 1);
	boundingBoxVertexes[2] = matrix * glm::vec4(-width, width, top, 1);
	boundingBoxVertexes[3] = matrix * glm::vec4(-width, -width, top, 1);
	boundingBoxVertexes[4] = matrix * glm::vec4(width, width, bottom, 1);
	boundingBoxVertexes[5] = matrix * glm::vec4(width, -width, bottom, 1);
	boundingBoxVertexes[6] = matrix * glm::vec4(-width, width, bottom, 1);
	boundingBoxVertexes[7] = matrix * glm::vec4(-width, -width, bottom, 1);
}

SpotLight::SpotLight()
	: LightBase(LightBase::LightType::SPOT)
	, minRange(0.01f)
	, maxRange(10.0f)
	, innerAngle(30.0f)
	, outerAngle(40.0f)
{
}

SpotLight::~SpotLight()
{
}
