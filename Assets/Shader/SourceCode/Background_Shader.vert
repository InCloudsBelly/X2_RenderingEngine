#version 450
#extension GL_GOOGLE_include_directive: enable

#include "BackgroundRendering.glsl"

layout(location = 0) in vec3 vertexPosition;

void main() 
{
    gl_Position = vec4(vertexPosition.xy, 1.0, 1.0);
}
