// Pre-depth shader

#version 450 core
#pragma stage : vert


// Vertex buffer
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec3 a_Tangent;
layout(location = 3) in vec3 a_Binormal;
layout(location = 4) in vec2 a_TexCoord;

// Transform buffer
layout(location = 5) in vec4 a_MRow0;
layout(location = 6) in vec4 a_MRow1;
layout(location = 7) in vec4 a_MRow2;

// Bone influences
layout(location = 8) in ivec4 a_BoneIndices;
layout(location = 9) in vec4 a_BoneWeights;

layout(std140, binding = 0) uniform Camera
{
	mat4 u_ViewProjectionMatrix;
	mat4 u_InverseViewProjectionMatrix;
	mat4 u_ProjectionMatrix;
	mat4 u_InverseProjectionMatrix;
	mat4 u_ViewMatrix;
	mat4 u_InverseViewMatrix;
	float NearClip;
	float FarClip;
	float FOV; // Radians
	float Padding0;
};

const int MAX_BONES = 100;
const int MAX_ANIMATED_MESHES = 1024;

layout (std140, set = 1, binding = 0) readonly buffer BoneTransforms
{
	mat4 BoneTransforms[MAX_BONES * MAX_ANIMATED_MESHES];
} r_BoneTransforms;

layout(push_constant) uniform BoneTransformIndex
{
	uint Base;
} u_BoneTransformIndex;

// layout(location = 0) out float LinearDepth;

// Make sure both shaders compute the exact same answer(PBR shader). 
// We need to have the same exact calculations to produce the gl_Position value (eg. matrix multiplications).
precise invariant gl_Position;

void main()
{
	mat4 transform = mat4(
		vec4(a_MRow0.x, a_MRow1.x, a_MRow2.x, 0.0),
		vec4(a_MRow0.y, a_MRow1.y, a_MRow2.y, 0.0),
		vec4(a_MRow0.z, a_MRow1.z, a_MRow2.z, 0.0),
		vec4(a_MRow0.w, a_MRow1.w, a_MRow2.w, 1.0)
	);

	mat4 boneTransform = r_BoneTransforms.BoneTransforms[(u_BoneTransformIndex.Base + gl_InstanceIndex) * MAX_BONES + a_BoneIndices[0]] * a_BoneWeights[0];
	boneTransform     += r_BoneTransforms.BoneTransforms[(u_BoneTransformIndex.Base + gl_InstanceIndex) * MAX_BONES + a_BoneIndices[1]] * a_BoneWeights[1];
	boneTransform     += r_BoneTransforms.BoneTransforms[(u_BoneTransformIndex.Base + gl_InstanceIndex) * MAX_BONES + a_BoneIndices[2]] * a_BoneWeights[2];
	boneTransform     += r_BoneTransforms.BoneTransforms[(u_BoneTransformIndex.Base + gl_InstanceIndex) * MAX_BONES + a_BoneIndices[3]] * a_BoneWeights[3];

	vec4 worldPosition = transform * boneTransform * vec4(a_Position, 1.0);

	//vec4 viewPosition = u_ViewMatrix * worldPosition;
	// Linear depth is not flipped.
	//LinearDepth = -viewPosition.z;

	// Near and far are flipped for better precision.
	// Only change along with the PBR shader.
	gl_Position = u_ViewProjectionMatrix * worldPosition;
}

#version 450 core
#pragma stage : frag

//layout(location = 0) out vec4 o_LinearDepth;

//layout(location = 0) in float LinearDepth;

void main()
{
	// TODO: Check for alpha in texture
	//o_LinearDepth = vec4(LinearDepth, 0.0, 0.0, 1.0);
}
