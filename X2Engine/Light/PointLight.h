#pragma once
#include <glm/glm.hpp>
#include "Core/Logic/Component/Base/LightBase.h"

class PointLight final : public LightBase
{
private:
	void setLightInfo(LightInfo& info)override;
	void setBoundingBoxInfo(std::array<glm::vec4, 8>& boundingBoxVertexes)override;
public:
	float minRange;
	float maxRange;
	PointLight();
	~PointLight();
	PointLight(const PointLight&) = delete;
	PointLight& operator=(const PointLight&) = delete;
	PointLight(PointLight&&) = delete;
	PointLight& operator=(PointLight&&) = delete;

	RTTR_ENABLE(LightBase)
};