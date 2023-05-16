#pragma once
#include <glm/glm.hpp>
#include "Core/Logic/Component/Base/LightBase.h"

class Image;

class AmbientLight final : public LightBase
{
private:
	void setLightInfo(LightInfo& info)override;
	void setBoundingBoxInfo(std::array<glm::vec4, 8>& boundingBoxVertexes)override;
public:
	Image* m_irradianceCubeImage;
	Image* m_prefilteredCubeImage;
	Image* m_lutImage;

	AmbientLight();
	~AmbientLight();
	AmbientLight(const AmbientLight&) = delete;
	AmbientLight& operator=(const AmbientLight&) = delete;
	AmbientLight(AmbientLight&&) = delete;
	AmbientLight& operator=(AmbientLight&&) = delete;

	RTTR_ENABLE(LightBase)
};