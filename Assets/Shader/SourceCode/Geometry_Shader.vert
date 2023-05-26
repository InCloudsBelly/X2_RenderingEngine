#version 450
#extension GL_GOOGLE_include_directive: enable

#include "Camera.glsl"
#include "Object.glsl"

layout(set = 0, binding = 0) uniform _CameraInfo
{
    CameraInfo info;
} cameraInfo;

layout(set = 1, binding = 0) uniform MeshObjectInfo
{
    ObjectInfo info;
} meshObjectInfo;

layout(location = 0) in vec3 vertexPosition;
layout(location = 1) in vec2 vertexTexCoords;
layout(location = 2) in vec3 vertexNormal;

layout(location = 0) out vec2 outTexCoords;
layout(location = 1) out vec3 outWorldPosition;
layout(location = 2) out vec3 outWorldNormal;

void main() 
{
    gl_Position = PositionO2P(vertexPosition, meshObjectInfo.info, cameraInfo.info);
    vec3 worldNormal = DirectionO2W(vertexNormal, meshObjectInfo.info);
    vec3 worldPosition = PositionO2W(vertexPosition, meshObjectInfo.info);
    
    outTexCoords = vertexTexCoords;
    outWorldPosition = worldPosition;
    outWorldNormal = worldNormal;
}
