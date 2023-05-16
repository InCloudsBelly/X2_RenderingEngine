// Code from: https://github.com/SaschaWillems/Vulkan-glTF-PBR/blob/06b2dbf65793a0f912216cd63ab542fc44fde844/data/shaders/filtercube.vert

#version 450
#extension GL_GOOGLE_include_directive: enable

#include "BackgroundRendering.glsl"

layout (location = 0) in vec3 vertexPosition;
layout(location = 0) out vec3 worldPosition;

layout(push_constant) uniform PushConsts {
	layout (offset = 0) mat4 mvp;
  	layout (offset = 64) float roughness;
	layout (offset = 68) int numSamples;
} pushConsts;


void main() 
{
	worldPosition = vertexPosition;
	gl_Position = pushConsts.mvp * vec4(vertexPosition.xyz, 1.0);
	gl_Position.y = - gl_Position.y;
}
