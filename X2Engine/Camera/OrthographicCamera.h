#pragma once
#include "Core/Logic/Component/Base/CameraBase.h"


class OrthographicCamera final : public CameraBase
{
private:
	void onSetSize(glm::vec2& parameter)override;
	void onSetClipPlanes(glm::vec4* clipPlanes)override;
	void onSetProjectionMatrix(glm::mat4& matrix)override;

	OrthographicCamera(const OrthographicCamera&) = delete;
	OrthographicCamera& operator=(const OrthographicCamera&) = delete;
	OrthographicCamera(OrthographicCamera&&) = delete;
	OrthographicCamera& operator=(OrthographicCamera&&) = delete;
public:
	float size;
	OrthographicCamera(std::string rendererName, std::map<std::string, Image*> attachments);
	virtual ~OrthographicCamera();

	RTTR_ENABLE(CameraBase)
};