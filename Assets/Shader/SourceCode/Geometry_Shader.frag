#version 450
#extension GL_GOOGLE_include_directive: enable

#include "Camera.glsl"
#include "Object.glsl"

layout(set = 0, binding = 0) uniform _CameraInfo
{
    CameraInfo info;
} cameraInfo;

layout(set = 2, binding = 0) uniform sampler2D normalTexture;

layout(location = 0) in vec2 inTexCoords;
layout(location = 1) in vec3 inWorldPosition;
layout(location = 2) in vec3 inWorldNormal;

layout(location = 0) out float Depth;
layout(location = 1) out vec3 vPosition;
layout(location = 2) out vec3 vNormal;

vec3 calculateNormal();

void main() 
{
    Depth = gl_FragCoord.z;
    vPosition = (cameraInfo.info.view * vec4(inWorldPosition,1.0)).rgb;
    vNormal = calculateNormal();
}


vec3 calculateNormal()
{
    vec3 tangentNormal = texture(normalTexture,inTexCoords).xyz ;

	vec3 q1 = dFdx(inWorldPosition);
	vec3 q2 = dFdy(inWorldPosition);
	vec2 st1 = dFdx(inTexCoords);
	vec2 st2 = dFdy(inTexCoords);

   vec3 N = normalize(inWorldNormal);
	vec3 T = normalize(q1 * st2.t - q2 * st1.t);
	vec3 B = -normalize(cross(N, T));
	mat3 TBN = mat3(T, B, N);

    return normalize(TBN * tangentNormal);
}