
#pragma once
#include "Core/Logic/Component/Base/CameraBase.h"


class PerspectiveCamera final : public CameraBase
{
private:
	void onSetSize(glm::vec2& parameter)override;
	void onSetClipPlanes(glm::vec4* clipPlanes)override;
	void onSetProjectionMatrix(glm::mat4& matrix)override;

	PerspectiveCamera(const PerspectiveCamera&) = delete;
	PerspectiveCamera& operator=(const PerspectiveCamera&) = delete;
	PerspectiveCamera(PerspectiveCamera&&) = delete;
	PerspectiveCamera& operator=(PerspectiveCamera&&) = delete;
public:
	float fovAngle;
	PerspectiveCamera(std::string rendererName, std::map<std::string, Image*> attachments);
	virtual ~PerspectiveCamera();

	RTTR_ENABLE(CameraBase)
};