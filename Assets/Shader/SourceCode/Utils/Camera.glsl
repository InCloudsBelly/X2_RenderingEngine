#ifndef _CAMERA_GLSL_
#define _CAMERA_GLSL_

#define ORTHOGRAPHIC_CAMERA 1
#define PERSPECTIVE_CAMERA 2

struct CameraInfo
{
    int type;
    float nearFlat;
    float farFlat;
    float aspectRatio;
    vec3 position;
    vec2 halfSize;
    vec4 parameter;
    vec3 forward;
    vec3 right;
    vec4 clipPlanes[6];
    mat4 projection;
    mat4 view;
};

///NDC Z -> View space Z
float OrthographicCameraDepthN2V(in float ndcDepth, in CameraInfo cameraInfo)
{
    return ndcDepth * (cameraInfo.nearFlat - cameraInfo.farFlat) - cameraInfo.nearFlat;
}

float PerspectiveCameraDepthN2V(float ndcDepth, in CameraInfo cameraInfo)
{
    return cameraInfo.nearFlat * cameraInfo.farFlat / (ndcDepth * (cameraInfo.farFlat - cameraInfo.nearFlat) - cameraInfo.farFlat);
}

float DepthN2V(in float ndcDepth, in CameraInfo cameraInfo)
{
    switch(cameraInfo.type)
    {
        case ORTHOGRAPHIC_CAMERA:
        {
            return OrthographicCameraDepthN2V(ndcDepth, cameraInfo);
        }
        case PERSPECTIVE_CAMERA:
        {
            return PerspectiveCameraDepthN2V(ndcDepth, cameraInfo);
        }
    }
    return 0;
}

///NDC Z -> NDC linear Z
float OrthographicCameraDepthN2LN(in float ndcDepth)
{
    return ndcDepth;
}

float PerspectiveCameraDepthN2LN(in float ndcDepth, in CameraInfo cameraInfo)
{
    return cameraInfo.nearFlat * ndcDepth / (ndcDepth * (cameraInfo.nearFlat - cameraInfo.farFlat) + cameraInfo.farFlat);
}

float DepthN2LN(in float ndcDepth, in CameraInfo cameraInfo)
{
    switch(cameraInfo.type)
    {
        case ORTHOGRAPHIC_CAMERA:
        {
            return OrthographicCameraDepthN2LN(ndcDepth);
        }
        case PERSPECTIVE_CAMERA:
        {
            return PerspectiveCameraDepthN2LN(ndcDepth, cameraInfo);
        }
    }
    return 0;
}

///NDC Position -> View space Position
vec3 OrthographicCameraPositionN2V(in vec3 ndcPosition, in CameraInfo cameraInfo)
{
    vec3 nearFlatPosition = vec3(ndcPosition.xy * cameraInfo.halfSize, -cameraInfo.nearFlat);
    vec3 farFlatPosition = vec3(ndcPosition.xy * cameraInfo.halfSize, -cameraInfo.farFlat);
    float linearDepth = ndcPosition.z;
    return nearFlatPosition * (1 - linearDepth) + farFlatPosition * linearDepth;
}

vec3 PerspectiveCameraPositionN2V(in vec3 ndcPosition, in CameraInfo cameraInfo)
{
    vec3 nearFlatPosition = vec3(ndcPosition.xy * cameraInfo.halfSize, -cameraInfo.nearFlat);
    vec3 farFlatPosition = vec3(ndcPosition.xy * cameraInfo.halfSize * cameraInfo.farFlat / cameraInfo.nearFlat, -cameraInfo.farFlat);
    float linearDepth = cameraInfo.nearFlat * ndcPosition.z / (ndcPosition.z * (cameraInfo.nearFlat - cameraInfo.farFlat) + cameraInfo.farFlat);
    return nearFlatPosition * (1 - linearDepth) + farFlatPosition * linearDepth;
}
vec3 PositionN2V(in vec3 ndcPosition, in CameraInfo cameraInfo)
{
    switch(cameraInfo.type)
    {
        case ORTHOGRAPHIC_CAMERA:
        {
            return OrthographicCameraPositionN2V(ndcPosition, cameraInfo);
        }
        case PERSPECTIVE_CAMERA:
        {
            return PerspectiveCameraPositionN2V(ndcPosition, cameraInfo);
        }
    }
    return vec3(0, 0, 0);
}

vec2 PositionS2N(in vec2 imagePosition)
{
    float x = clamp((2 * imagePosition.x  - 1), -1, 1);
    float y = clamp((2 * imagePosition.y  - 1), -1, 1);
    return vec2(x, y);
}

vec2 PositionN2S(in vec2 ndcPosition)
{
    float x = (clamp(ndcPosition.x, -1, 1) + 1) / 2;
    float y = (clamp(ndcPosition.y, -1, 1) + 1) / 2;
    return vec2(x, y);
}

vec2 PositionA2N(in vec2 attachmentPosition)
{
    float x = clamp((2 * attachmentPosition.x  - 1), -1, 1);
    float y = clamp((2 * attachmentPosition.y  - 1), -1, 1);
    return vec2(x, y);
}

vec2 PositionN2A(in vec2 ndcPosition)
{
    float x = (clamp(ndcPosition.x, -1, 1) + 1) / 2;
    float y = (clamp(ndcPosition.y, -1, 1) + 1) / 2;
    return vec2(x, y);
}

#define IMAGE_TO_NDC_SPACE(imagePosition, depth) (vec3(2 * (imagePosition).x - 1, 1 - 2 * (imagePosition).y, (depth)))

vec3 PositionN2W(in vec3 ndcPosition, in CameraInfo cameraInfo)
{
    vec3 vPosition = PositionN2V(ndcPosition, cameraInfo);
    vec3 wPosition = (inverse(cameraInfo.view) * vec4(vPosition, 1)).xyz;
    return wPosition;
}

vec3 PositionS2NFW(in vec2 screenPosition, in CameraInfo cameraInfo)
{
    vec2 ndcPos = PositionS2N(screenPosition);
    float x = cameraInfo.halfSize.x * ndcPos.x;
    float y = cameraInfo.halfSize.y * ndcPos.y;
    vec3 up = cross(cameraInfo.right, cameraInfo.forward);
    return cameraInfo.position + normalize(cameraInfo.forward) * cameraInfo.nearFlat + normalize(cameraInfo.right) * x + normalize(up) * y;
}


vec3 OrthographicCameraWViewDirection(in CameraInfo cameraInfo)
{
    return normalize(cameraInfo.forward);
}

vec3 PerspectiveCameraWViewDirection(in vec3 worldPosition, in CameraInfo cameraInfo)
{
    return normalize(worldPosition - cameraInfo.position);
}

vec3 CameraWViewDirection(in vec3 worldPosition, in CameraInfo cameraInfo)
{
    switch(cameraInfo.type)
    {
        case ORTHOGRAPHIC_CAMERA:
        {
            return OrthographicCameraWViewDirection(cameraInfo);
        }
        case PERSPECTIVE_CAMERA:
        {
            return PerspectiveCameraWViewDirection(worldPosition, cameraInfo);
        }
    }
    return vec3(0, 0, -1);
}

vec3 CameraWObserveDirection(in vec3 worldPosition, in CameraInfo cameraInfo)
{
    return normalize(worldPosition - cameraInfo.position);
}
#define VIEW_DIRECTION(cameraInfo, worldPosition) (normalize((worldPosition) - (cameraInfo).position))

#endif ///#ifndef _CAMERA_GLSL_