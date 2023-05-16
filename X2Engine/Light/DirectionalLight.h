#pragma once
#include <glm/glm.hpp>
#include "Core/Logic/Component/Base/LightBase.h"

class DirectionalLight final : public LightBase
{
private:
	void setLightInfo(LightInfo& info)override;
	void setBoundingBoxInfo(std::array<glm::vec4, 8>& boundingBoxVertexes)override;
public:
	DirectionalLight();
	~DirectionalLight();
	DirectionalLight(const DirectionalLight&) = delete;
	DirectionalLight& operator=(const DirectionalLight&) = delete;
	DirectionalLight(DirectionalLight&&) = delete;
	DirectionalLight& operator=(DirectionalLight&&) = delete;

	RTTR_ENABLE(LightBase)
};