#include "OrthographicCamera.h"

RTTR_REGISTRATION
{
	using namespace rttr;
	registration::class_<OrthographicCamera>("OrthographicCamera");
}

void OrthographicCamera::onSetSize(glm::vec2& parameter)
{
	float halfWidth = aspectRatio * size;
	float halfHeight = size;
	parameter = glm::vec2(halfWidth, halfHeight);
}

void OrthographicCamera::onSetClipPlanes(glm::vec4* clipPlanes)
{
	clipPlanes[0] = glm::vec4(-1, 0, 0, aspectRatio * size);
	clipPlanes[1] = glm::vec4(1, 0, 0, aspectRatio * size);
	clipPlanes[2] = glm::vec4(0, -1, 0, size);
	clipPlanes[3] = glm::vec4(0, 1, 0, size);
	clipPlanes[4] = glm::vec4(0, 0, -1, -nearFlat);
	clipPlanes[5] = glm::vec4(0, 0, 1, farFlat);
}

void OrthographicCamera::onSetProjectionMatrix(glm::mat4& matrix)
{
	float halfWidth = aspectRatio * size;
	float halfHeight = size;
	float flatDistence = farFlat - nearFlat;

	matrix = glm::mat4(
		1.0 / halfWidth, 0, 0, 0,
		0, 1.0 / halfHeight, 0, 0,
		0, 0, -1.0 / flatDistence, 0,
		0, 0, -nearFlat / flatDistence, 1
	);
}

OrthographicCamera::OrthographicCamera(std::string rendererName, std::map<std::string, Image*> attachments)
	: CameraBase(CameraType::ORTHOGRAPHIC, rendererName, attachments)
	, size(2.25f)
{
}

OrthographicCamera::~OrthographicCamera()
{
}
