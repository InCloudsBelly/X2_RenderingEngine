#ifndef _BACKGROUND_RENDERING_GLSL_
#define _BACKGROUND_RENDERING_GLSL_

#include "Camera.glsl"

#define START_SET_INDEX 2

layout(set = 0, binding = 0) uniform _CameraInfo
{
    CameraInfo info;
} cameraInfo;

layout(set = 1, binding = 0) uniform AttachmentSizeInfo
{
    vec2 size;
    vec2 texelSize;
} attachmentSizeInfo;

#endif ///#ifndef _BACKGROUND_RENDERING_GLSL_