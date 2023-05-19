#version 450
#extension GL_GOOGLE_include_directive: enable

#include "Camera.glsl"

layout(set = 0, binding = 0) uniform _CameraInfo
{
    CameraInfo info;
} cameraInfo;


layout(set = 2, binding = 0) uniform sampler2D albedoTexture;

layout(location = 0) in vec2 inTexCoords;
layout(location = 1) in vec3 inWorldPosition;
layout(location = 2) in vec3 inWorldNormal;
layout(location = 3) in vec3 inWorldTangent;
layout(location = 4) in vec3 inWorldBitangent;

layout(location = 0) out vec4 ColorAttachment;

void main() 
{
    vec3 wNormal = normalize(inWorldNormal);
    vec3 wView = CameraWObserveDirection(inWorldPosition, cameraInfo.info);

    vec3 albedo = texture(albedoTexture, inTexCoords).rgb;

    ColorAttachment = vec4(albedo, 1);
}
