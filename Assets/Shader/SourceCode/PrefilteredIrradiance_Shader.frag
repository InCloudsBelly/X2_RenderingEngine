// Code from: https://github.com/SaschaWillems/Vulkan-glTF-PBR/blob/06b2dbf65793a0f912216cd63ab542fc44fde844/data/shaders/prefilterenvmap.frag

#version 450
#extension GL_GOOGLE_include_directive: enable

layout (location = 0) in vec3 worldPosition;
layout (location = 0) out vec4 ColorAttachment;

layout(push_constant) uniform PushConsts {
	layout (offset = 0) mat4 mvp;
    layout (offset = 64) float deltaPhi;
	layout (offset = 68) float deltaTheta;
} pushConsts;


layout(set = 0, binding = 0) uniform samplerCube environmentImage;

#define PI 3.1415926535897932384626433832795

void main()
{
	vec3 N = normalize(worldPosition);
	N.y = -N.y;
	vec3 up = vec3(0.0, 1.0, 0.0);
	vec3 right = normalize(cross(up, N));
	up = cross(N, right);

	const float TWO_PI = PI * 2.0;
	const float HALF_PI = PI * 0.5;

	vec3 color = vec3(0.0);
	uint sampleCount = 0u;
	for (float phi = 0.0; phi < TWO_PI; phi += pushConsts.deltaPhi) {
		for (float theta = 0.0; theta < HALF_PI; theta += pushConsts.deltaTheta) {
			vec3 tempVec = cos(phi) * right + sin(phi) * up;
			vec3 sampleVector = cos(theta) * N + sin(theta) * tempVec;
			color += texture(environmentImage, sampleVector).rgb * cos(theta) * sin(theta);
			sampleCount++;
		}
	}
	ColorAttachment = vec4(PI * color / float(sampleCount), 1.0);
	
}
