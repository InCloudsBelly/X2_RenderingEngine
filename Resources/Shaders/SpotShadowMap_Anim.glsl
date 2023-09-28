// Spot Shadow Map shader

#version 450 core
#pragma stage : vert

#include <Buffers.glslh>

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec3 a_Tangent;
layout(location = 3) in vec3 a_Binormal;
layout(location = 4) in vec2 a_TexCoord;

// Transform buffer
layout(location = 5) in vec4 a_MRow0;
layout(location = 6) in vec4 a_MRow1;
layout(location = 7) in vec4 a_MRow2;

layout(location = 8) 	in vec4 a_MRowPrev0;
layout(location = 9) 	in vec4 a_MRowPrev1;
layout(location = 10) 	in vec4 a_MRowPrev2;

// Bone influences
layout(location = 11) in ivec4 a_BoneIndices;
layout(location = 12) in vec4 a_BoneWeights;

const int MAX_BONES = 100;
const int MAX_ANIMATED_MESHES = 1024;

layout (std140, set = 1, binding = 0) readonly buffer BoneTransforms
{
	mat4 BoneTransforms[MAX_BONES * MAX_ANIMATED_MESHES];
} r_BoneTransforms;

layout(push_constant) uniform PushConstants
{
	uint LightIndex;
	uint BoneTransformBaseIndex;
} u_Constants;

void main()
{
	mat4 transform = mat4(
		vec4(a_MRow0.x, a_MRow1.x, a_MRow2.x, 0.0),
		vec4(a_MRow0.y, a_MRow1.y, a_MRow2.y, 0.0),
		vec4(a_MRow0.z, a_MRow1.z, a_MRow2.z, 0.0),
		vec4(a_MRow0.w, a_MRow1.w, a_MRow2.w, 1.0)
	);

	mat4 boneTransform = r_BoneTransforms.BoneTransforms[(u_Constants.BoneTransformBaseIndex + gl_InstanceIndex) * MAX_BONES + a_BoneIndices[0]] * a_BoneWeights[0];
	boneTransform     += r_BoneTransforms.BoneTransforms[(u_Constants.BoneTransformBaseIndex + gl_InstanceIndex) * MAX_BONES + a_BoneIndices[1]] * a_BoneWeights[1];
	boneTransform     += r_BoneTransforms.BoneTransforms[(u_Constants.BoneTransformBaseIndex + gl_InstanceIndex) * MAX_BONES + a_BoneIndices[2]] * a_BoneWeights[2];
	boneTransform     += r_BoneTransforms.BoneTransforms[(u_Constants.BoneTransformBaseIndex + gl_InstanceIndex) * MAX_BONES + a_BoneIndices[3]] * a_BoneWeights[3];

	gl_Position =  u_SpotLights.Mats[u_Constants.LightIndex] * transform * boneTransform * vec4(a_Position, 1.0);
}

#version 450 core
#pragma stage : frag

void main()
{
	// TODO: Check for alpha in texture
}
