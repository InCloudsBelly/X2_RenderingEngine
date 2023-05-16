#include "Core/Logic/Component/Base/LightBase.h"

RTTR_REGISTRATION
{
	rttr::registration::class_<LightBase>("LightBase");
}

const LightBase::LightInfo* LightBase::getLightInfo()
{
	return &m_lightInfo;
}

const std::array<glm::vec4, 8>* LightBase::getBoundingBox()
{
	return &m_boundingBoxVertexes;
}

void LightBase::onUpdate()
{
	setLightInfo(m_lightInfo);
	setBoundingBoxInfo(m_boundingBoxVertexes);
}

LightBase::LightBase(LightType lightType)
	: Component(Component::ComponentType::LIGHT)
	, lightType(lightType)
	, color({ 1, 1, 1, 1 })
	, intensity(1.0f)
	, m_lightInfo()
	, m_boundingBoxVertexes()
{
}

LightBase::~LightBase()
{
}
