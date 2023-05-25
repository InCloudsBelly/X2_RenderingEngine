#version 450
#extension GL_GOOGLE_include_directive: enable

#include "Object.glsl"

layout(set = 0, binding = 0) uniform MeshObjectInfo
{
    ObjectInfo info;
} meshObjectInfo;

layout(set = 1, binding = 0) uniform LightCameraInfo
{
    mat4 view;
    mat4 projection;
	mat4 viewProjection; 
} lightCameraInfo;

layout(location = 0) in vec3 vertexPosition;

void main() 
{
    gl_Position = lightCameraInfo.viewProjection * meshObjectInfo.info.model * vec4(vertexPosition, 1);
}
