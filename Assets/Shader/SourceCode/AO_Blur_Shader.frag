#version 450
#extension GL_GOOGLE_include_directive: enable

#include "Utils/Object.glsl"

#define MAX_GAUSSION_KERNAL_RADIUS 4

const float GAUSSION_WEIGHT[MAX_GAUSSION_KERNAL_RADIUS] = {0.324, 0.232, 0.0855, 0.0205};

layout(set = 0, binding = 0) uniform BlurInfo
{
    vec2 size;
    vec2 texelSize;
    vec2 sampleOffset;
} blurInfo;
layout(set = 1, binding = 0) uniform sampler2D normalTexture;
layout(set = 2, binding = 0) uniform sampler2D occlusionTexture;

layout(location = 0) out float OcclusionTexture;

void main()
{
    vec2 aPosition = gl_FragCoord.xy * blurInfo.texelSize;
    vec3 vNormal = ParseFromColor(texture(normalTexture, aPosition).rgb);

    float occlusion = 0;
    float weightSum = 0;

    occlusion += texture(occlusionTexture, aPosition).r * GAUSSION_WEIGHT[0];
    weightSum += GAUSSION_WEIGHT[0];

    vec2 texcoordSampleOffset = blurInfo.texelSize * blurInfo.sampleOffset;
    for(int i = 1; i < MAX_GAUSSION_KERNAL_RADIUS; i++)
    {
        vec2 offsetAPosition = aPosition + texcoordSampleOffset * i;
        if(0 < offsetAPosition.x && offsetAPosition.x < 1 && 0 < offsetAPosition.y && offsetAPosition.y < 1)
        {
            vec3 offsetVNormal = ParseFromColor(texture(normalTexture, offsetAPosition).rgb);
            float weight = GAUSSION_WEIGHT[i] * smoothstep(0.1, 0.8, dot(offsetVNormal, vNormal));

            occlusion += texture(occlusionTexture, offsetAPosition).r * weight;
            weightSum += weight;
        }

        offsetAPosition = aPosition - texcoordSampleOffset * i;
        if(0 < offsetAPosition.x && offsetAPosition.x < 1 && 0 < offsetAPosition.y && offsetAPosition.y < 1)
        {
            vec3 offsetVNormal = ParseFromColor(texture(normalTexture, offsetAPosition).rgb);
            float weight = GAUSSION_WEIGHT[i] * smoothstep(0.1, 0.8, dot(offsetVNormal, vNormal));

            occlusion += texture(occlusionTexture, offsetAPosition).r * weight;
            weightSum += weight;
        }
    }

    OcclusionTexture = occlusion / weightSum;
}
