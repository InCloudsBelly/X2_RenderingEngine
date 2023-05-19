#include "PerspectiveCamera.h"

#include <glm/gtc/matrix_transform.hpp>

RTTR_REGISTRATION
{
    using namespace rttr;
    registration::class_<PerspectiveCamera>("PerspectiveCamera");
}

void PerspectiveCamera::onSetSize(glm::vec2& parameter)
{
    const double pi = std::acos(-1.0);
    double halfFov = fovAngle * pi / 360.0;
    float halfHeight = std::tanf(halfFov) * nearFlat;
    float halfWidth = halfHeight * aspectRatio;
    parameter = glm::vec2(halfWidth, halfHeight);
}

void PerspectiveCamera::onSetClipPlanes(glm::vec4* clipPlanes)
{
    const double pi = std::acos(-1.0);
    double halfFov = fovAngle * pi / 360.0;
    float tanHalfFov = std::tan(halfFov);
    clipPlanes[0] = glm::vec4(0, -nearFlat, -nearFlat * tanHalfFov, 0);
    clipPlanes[1] = glm::vec4(0, nearFlat, -nearFlat * tanHalfFov, 0);
    clipPlanes[2] = glm::vec4(-nearFlat, 0, -aspectRatio * nearFlat * tanHalfFov, 0);
    clipPlanes[3] = glm::vec4(nearFlat, 0, -aspectRatio * nearFlat * tanHalfFov, 0);
    clipPlanes[4] = glm::vec4(0, 0, -1, -nearFlat);
    clipPlanes[5] = glm::vec4(0, 0, 1, farFlat);
}

void PerspectiveCamera::onSetProjectionMatrix(glm::mat4& matrix)
{
    const double pi = std::acos(-1.0);
    double halfFov = fovAngle * pi / 360.0;
    double cot = 1.0 / std::tan(halfFov);
    float flatDistence = farFlat - nearFlat;

    matrix = glm::mat4(
        cot / aspectRatio, 0, 0, 0,
        0, cot, 0, 0,
        0, 0, -farFlat / flatDistence, -1,
        0, 0, -nearFlat * farFlat / flatDistence, 0
    );


    // left-handed coordinate system to right-handed
    matrix[1][1] = -matrix[1][1];
}

PerspectiveCamera::PerspectiveCamera(std::string rendererName, std::map<std::string, Image*> attachments)
    : CameraBase(CameraType::PERSPECTIVE, rendererName, attachments)
    , fovAngle(60)
{
    nearFlat = 0.5f;
}

PerspectiveCamera::~PerspectiveCamera()
{
}
