#version 450
#extension GL_GOOGLE_include_directive: enable

layout(location = 0) in vec3 vertexPosition;

void main() 
{
    gl_Position = vec4(vertexPosition.xy, 0, 1.0);
}
