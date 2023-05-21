#include "AmbientLight.h"
#include "Core/Graphic/Instance/Image.h"

RTTR_REGISTRATION
{
	rttr::registration::class_<AmbientLight>("AmbientLight");
}

void AmbientLight::setLightInfo(LightInfo& info)
{
	info.type = 3;
	info.intensity = intensity;
	info.minRange = 0;
	info.maxRange = 0;
	info.extraParamter = glm::vec4(m_prefilteredCubeImage->getRawImageView().getMipmapLevelCount() - 1, 0, 0, 0);
	info.position = glm::vec3(0);
	info.direction = { 0, 0, 0 };
	info.color = color;
}

void AmbientLight::setBoundingBoxInfo(std::array<glm::vec4, 8>& boundingBoxVertexes)
{

}


AmbientLight::AmbientLight()
	: LightBase(LightType::AMBIENT)
	, m_irradianceCubeImage(nullptr)
	, m_prefilteredCubeImage(nullptr)
	, m_lutImage(nullptr)
{
}

AmbientLight::~AmbientLight()
{
}
