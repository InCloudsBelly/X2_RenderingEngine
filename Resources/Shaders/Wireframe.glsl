// Outline Shader

#version 450 core
#pragma stage : vert
#include <Buffers.glslh>

layout(location = 0) in vec3 a_Position;

//////////////////////////////////////////
// UNUSED
//////////////////////////////////////////
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec3 a_Tangent;
layout(location = 3) in vec3 a_Binormal;
layout(location = 4) in vec2 a_TexCoord;
//////////////////////////////////////////


// Transform buffer
layout(location = 5) in vec4 a_MRow0;
layout(location = 6) in vec4 a_MRow1;
layout(location = 7) in vec4 a_MRow2;

layout(location = 8) 	in vec4 a_MRowPrev0;
layout(location = 9) 	in vec4 a_MRowPrev1;
layout(location = 10) 	in vec4 a_MRowPrev2;


void main()
{
	mat4 transform = mat4(
		vec4(a_MRow0.x, a_MRow1.x, a_MRow2.x, 0.0),
		vec4(a_MRow0.y, a_MRow1.y, a_MRow2.y, 0.0),
		vec4(a_MRow0.z, a_MRow1.z, a_MRow2.z, 0.0),
		vec4(a_MRow0.w, a_MRow1.w, a_MRow2.w, 1.0)
	);

	gl_Position = u_Camera.ViewProjectionMatrix * transform * vec4(a_Position, 1.0);
}

#version 450 core
#pragma stage : frag

layout(location = 0) out vec4 color;

layout (push_constant) uniform Material
{
	layout (offset = 64) vec4 Color;
} u_MaterialUniforms;

void main()
{
	color = u_MaterialUniforms.Color;
}
