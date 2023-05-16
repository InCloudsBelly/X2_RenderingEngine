#pragma once
#include <glm/glm.hpp>
#include "Core/Logic/Component/Base/LightBase.h"

class SpotLight final : public LightBase
{
private:
	void setLightInfo(LightInfo& info)override;
	void setBoundingBoxInfo(std::array<glm::vec4, 8>& boundingBoxVertexes)override;
public:
	float minRange;
	float maxRange;
	float innerAngle;
	float outerAngle;
	SpotLight();
	~SpotLight();
	SpotLight(const SpotLight&) = delete;
	SpotLight& operator=(const SpotLight&) = delete;
	SpotLight(SpotLight&&) = delete;
	SpotLight& operator=(SpotLight&&) = delete;

	RTTR_ENABLE(LightBase)
};