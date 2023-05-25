#ifndef _CASCADE_EVSM_SHADOW_RECEIVER_GLSL_
#define _CASCADE_EVSM_SHADOW_RECEIVER_GLSL_

#ifdef CASCADE_EVSM_SHADOW_RECEIVER_DESCRIPTOR_START_INDEX
#define CASCADE_EVSM_SHADOW_RECEIVER_DESCRIPTOR_COUNT 5

#define CASCADE_COUNT 4

layout(set = CASCADE_EVSM_SHADOW_RECEIVER_DESCRIPTOR_START_INDEX + 0, binding = 0) uniform CascadeEvsmShadowReceiverInfo
{
    float thresholdVZ[CASCADE_COUNT * 2];
    vec3 wLightDirection;
    vec3 vLightDirection;
    mat4 matrixVC2PL[CASCADE_COUNT];
    vec2 texelSize[CASCADE_COUNT];
    float c1;
    float c2;
    float threshold;
}cascadeEvsmShadowReceiverInfo;

layout(set = CASCADE_EVSM_SHADOW_RECEIVER_DESCRIPTOR_START_INDEX + 1, binding = 0) uniform sampler2D shadowTexture_0;
layout(set = CASCADE_EVSM_SHADOW_RECEIVER_DESCRIPTOR_START_INDEX + 2, binding = 0) uniform sampler2D shadowTexture_1;
layout(set = CASCADE_EVSM_SHADOW_RECEIVER_DESCRIPTOR_START_INDEX + 3, binding = 0) uniform sampler2D shadowTexture_2;
layout(set = CASCADE_EVSM_SHADOW_RECEIVER_DESCRIPTOR_START_INDEX + 4, binding = 0) uniform sampler2D shadowTexture_3;

vec4 SampleShadowTexture(int cascadIndex, vec2 aPosition)
{
    vec4 param;
    switch(cascadIndex)
    {
        case 0:
        {
            param = texture(shadowTexture_0, aPosition).rgba;
            break;
        }
        case 1:
        {
            param = texture(shadowTexture_1, aPosition).rgba;
            break;
        }
        case 2:
        {
            param = texture(shadowTexture_2, aPosition).rgba;
            break;
        }
        case 3:
        {
            param = texture(shadowTexture_3, aPosition).rgba;
            break;
        }
    }
    return param;
}


float GetShadowIntensity(in vec3 vPosition)
{
    int cascadIndex = -1;
    for(int i = 0; i < CASCADE_COUNT * 2; i++)
    {
        if(vPosition.z > cascadeEvsmShadowReceiverInfo.thresholdVZ[i])
        {
            cascadIndex = i - 1;
            break;
        }
    }

    if(cascadIndex == -1) return 1;
    float shadowIntensity0 = 0;
    float lnDepth0 = 0;
    float shadowIntensity1 = 0;
    float lnDepth1 = 0;
    
    {
        vec4 lpPosition = cascadeEvsmShadowReceiverInfo.matrixVC2PL[(cascadIndex + 1) / 2] * vec4(vPosition, 1);
        vec3 lnPosition = lpPosition.xyz / lpPosition.w;
        lnDepth0 = lnPosition.z;
        vec2 laPosition = (clamp(lnPosition.xy, -1, 1) + vec2(1, 1)) / vec2(2, 2);

        vec4 param = SampleShadowTexture((cascadIndex + 1) / 2, laPosition);
        {
            vec2 mu = param.xz;
            vec2 sigma2 = param.yw - mu * mu;
            vec2 expDepth = vec2(exp(lnDepth0 * cascadeEvsmShadowReceiverInfo.c1), -exp(-lnDepth0 * cascadeEvsmShadowReceiverInfo.c2));
            vec2 lightIntensity = clamp(sigma2 / (sigma2 + (expDepth - mu) * (expDepth - mu)), 0, 1);
            if(expDepth.x <= mu.x || expDepth.y <= mu.y)
            {
                shadowIntensity0 = 0;
            }
            else
            {
                shadowIntensity0 = clamp((lightIntensity.x < lightIntensity.y ? lightIntensity.x : lightIntensity.y) - cascadeEvsmShadowReceiverInfo.threshold, 0, 1);
                shadowIntensity0 = 1 - shadowIntensity0 / (1 - cascadeEvsmShadowReceiverInfo.threshold);
            }
        }
    }

    if(cascadIndex % 2 == 0)
    {
        return shadowIntensity0;
    }
    else
    {
        vec4 lpPosition = cascadeEvsmShadowReceiverInfo.matrixVC2PL[cascadIndex / 2] * vec4(vPosition, 1);
        vec3 lnPosition = lpPosition.xyz / lpPosition.w;
        lnDepth1 = lnPosition.z;
        vec2 laPosition = (clamp(lnPosition.xy, -1, 1) + vec2(1, 1)) / vec2(2, 2);

        vec4 param = SampleShadowTexture(cascadIndex / 2, laPosition);
        {
            vec2 mu = param.xz;
            vec2 sigma2 = param.yw - mu * mu;
            vec2 expDepth = vec2(exp(lnDepth1 * cascadeEvsmShadowReceiverInfo.c1), -exp(-lnDepth1 * cascadeEvsmShadowReceiverInfo.c2));
            vec2 lightIntensity = clamp(sigma2 / (sigma2 + (expDepth - mu) * (expDepth - mu)), cascadeEvsmShadowReceiverInfo.threshold, 1);
            if(expDepth.x <= mu.x || expDepth.y <= mu.y)
            {
                shadowIntensity1 = 0;
            }
            else
            {
                shadowIntensity1 = clamp((lightIntensity.x < lightIntensity.y ? lightIntensity.x : lightIntensity.y) - cascadeEvsmShadowReceiverInfo.threshold, 0, 1);
                shadowIntensity1 = 1 - shadowIntensity1 / (1 - cascadeEvsmShadowReceiverInfo.threshold);
            }
        }

        float len = cascadeEvsmShadowReceiverInfo.thresholdVZ[cascadIndex] - cascadeEvsmShadowReceiverInfo.thresholdVZ[cascadIndex + 1];
        float pre = cascadeEvsmShadowReceiverInfo.thresholdVZ[cascadIndex] - vPosition.z;
        float next = vPosition.z - cascadeEvsmShadowReceiverInfo.thresholdVZ[cascadIndex + 1];

        return shadowIntensity0 * pre / len + shadowIntensity1 * next / len;
    }

    // return shadowIntensity0;
}

#endif ///#ifdef CASCADE_EVSM_SHADOW_RECEIVER_DESCRIPTOR_START_INDEX

#endif ///#ifndef _CASCADE_EVSM_SHADOW_RECEIVER_GLSL_