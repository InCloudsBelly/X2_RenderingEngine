#version 450
#extension GL_GOOGLE_include_directive: enable

#include "Utils/Object.glsl"

#define MAX_GAUSSION_KERNAL_RADIUS 4

const float GAUSSION_WEIGHT[MAX_GAUSSION_KERNAL_RADIUS] = {0.324, 0.232, 0.0855, 0.0205};

layout(set = 0, binding = 0) uniform sampler2D shadowTexture;
layout(set = 1, binding = 0) uniform CascadeEvsmBlurInfo
{
    vec2 texelSize;
    vec2 sampleOffset;
}cascadeEvsmBlurInfo;

layout(location = 0) out vec4 ShadowTexture;

void main() 
{
    vec2 aPosition = gl_FragCoord.xy * cascadeEvsmBlurInfo.texelSize;

    vec4 shadow = vec4(0);
    float weightSum = 0;

    shadow += texture(shadowTexture, aPosition).rgba * GAUSSION_WEIGHT[0];
    weightSum += GAUSSION_WEIGHT[0];

    vec2 texcoordSampleOffset = cascadeEvsmBlurInfo.texelSize * cascadeEvsmBlurInfo.sampleOffset;
    for(int i = 1; i < MAX_GAUSSION_KERNAL_RADIUS; i++)
    {
        vec2 offsetAPosition = aPosition + texcoordSampleOffset * i;
        if(0 < offsetAPosition.x && offsetAPosition.x < 1 && 0 < offsetAPosition.y && offsetAPosition.y < 1)
        {
            shadow += texture(shadowTexture, offsetAPosition).rgba * GAUSSION_WEIGHT[i];
            weightSum += GAUSSION_WEIGHT[i];
        }

        offsetAPosition = aPosition - texcoordSampleOffset * i;
        if(0 < offsetAPosition.x && offsetAPosition.x < 1 && 0 < offsetAPosition.y && offsetAPosition.y < 1)
        {
            shadow += texture(shadowTexture, offsetAPosition).rgba * GAUSSION_WEIGHT[i];
            weightSum += GAUSSION_WEIGHT[i];
        }
    }

    ShadowTexture = shadow / weightSum;
}
