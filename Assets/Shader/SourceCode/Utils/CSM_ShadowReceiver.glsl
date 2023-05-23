#ifndef _CSM_SHADOW_RECEIVER_GLSL_
#define _CSM_SHADOW_RECEIVER_GLSL_

#define CASCADE_COUNT 4

#ifdef CSM_SHADOW_RECEIVER_DESCRIPTOR_START_INDEX
#define CSM_SHADOW_RECEIVER_DESCRIPTOR_COUNT 2

layout(set = CSM_SHADOW_RECEIVER_DESCRIPTOR_START_INDEX + 0, binding = 0) uniform CsmShadowReceiverInfo
{
    float thresholdVZ[CASCADE_COUNT * 2];
    vec3 wLightDirection;
    vec3 vLightDirection;
    vec2 bias[CASCADE_COUNT];
    mat4 matrixVC2PL[CASCADE_COUNT];
    vec2 texelSize[CASCADE_COUNT];
    int sampleHalfWidth;
}csmShadowReceiverInfo;

layout(set = CSM_SHADOW_RECEIVER_DESCRIPTOR_START_INDEX + 1, binding = 0) uniform sampler2DArray shadowTextureArray;

#define SampleShadowTexture(cascadIndex, aPosition) (texture(shadowTextureArray, vec3(aPosition, cascadIndex)).r)

float GetShadowIntensity(in vec3 vPosition, in vec3 wNormal)
{
    int cascadIndex = -1;
    for(int i = 0; i < CASCADE_COUNT * 2; i++)
    {
        if(vPosition.z > csmShadowReceiverInfo.thresholdVZ[i])
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
        int index = (cascadIndex + 1) / 2;

        float bias = 0;
        float cosLightWithNormal = dot(wNormal, -csmShadowReceiverInfo.wLightDirection);
        if(cosLightWithNormal <= 0)
        {
            bias = csmShadowReceiverInfo.bias[index].x;
        }
        else
        {
            bias = max(csmShadowReceiverInfo.bias[index].y * (1.0 - cosLightWithNormal), csmShadowReceiverInfo.bias[index].x);
        }
   
        vec4 lpPosition = csmShadowReceiverInfo.matrixVC2PL[index] * vec4(vPosition, 1);
        vec3 lnPosition = lpPosition.xyz / lpPosition.w;
        lnDepth0 = lnPosition.z;
        vec2 laPosition = (clamp(lnPosition.xy, -1, 1) + vec2(1, 1)) / vec2(2, 2);

        int inShadowCount = 0;
        for(int i = -csmShadowReceiverInfo.sampleHalfWidth; i <= csmShadowReceiverInfo.sampleHalfWidth; i++)
        {
            for(int j = -csmShadowReceiverInfo.sampleHalfWidth; j <= csmShadowReceiverInfo.sampleHalfWidth; j++)
            {
                vec2 offsetAPosition = clamp(laPosition + vec2(i, j) * csmShadowReceiverInfo.texelSize[index], 0, 1);
                inShadowCount += ((lnDepth0 - bias) > SampleShadowTexture(index, offsetAPosition) ? 1 : 0);
            }
        }
        shadowIntensity0 = float(inShadowCount) / float((csmShadowReceiverInfo.sampleHalfWidth * 2 + 1) * (csmShadowReceiverInfo.sampleHalfWidth * 2 + 1));
        
        //shadowIntensity0 = lnDepth0;
        //shadowIntensity0 = SampleShadowTexture(index, laPosition);

        // shadowIntensity0 = (lnDepth0 - bias) > SampleShadowTexture(index, laPosition) ? 1 : 0;
    }

    if(cascadIndex % 2 == 0)
    {
        return shadowIntensity0;
    }
    else
    {
        int index = cascadIndex / 2;

        float bias = 0;
        float cosLightWithNormal = dot(wNormal, -csmShadowReceiverInfo.wLightDirection);
        if(cosLightWithNormal <= 0)
        {
            bias = csmShadowReceiverInfo.bias[index].x;
        }
        else
        {
            bias = max(csmShadowReceiverInfo.bias[index].y * (1.0 - cosLightWithNormal), csmShadowReceiverInfo.bias[index].x);
        }

        vec4 lpPosition = csmShadowReceiverInfo.matrixVC2PL[index] * vec4(vPosition, 1);
        vec3 lnPosition = lpPosition.xyz / lpPosition.w;
        lnDepth1 = lpPosition.z;
        vec2 laPosition = (clamp(lnPosition.xy, -1, 1) + vec2(1, 1)) / vec2(2, 2);

        int inShadowCount = 0;
        for(int i = -csmShadowReceiverInfo.sampleHalfWidth; i <= csmShadowReceiverInfo.sampleHalfWidth; i++)
        {
            for(int j = -csmShadowReceiverInfo.sampleHalfWidth; j <= csmShadowReceiverInfo.sampleHalfWidth; j++)
            {
                vec2 offsetAPosition = clamp(laPosition + vec2(i, j) * csmShadowReceiverInfo.texelSize[index], 0, 1);
                inShadowCount += ((lnDepth1 - bias) > SampleShadowTexture(index, offsetAPosition) ? 1 : 0);
            }
        }
        shadowIntensity1 = float(inShadowCount) / float((csmShadowReceiverInfo.sampleHalfWidth * 2 + 1) * (csmShadowReceiverInfo.sampleHalfWidth * 2 + 1));
        //shadowIntensity1 = lnDepth1;
        //shadowIntensity1 = SampleShadowTexture(index, laPosition);

        // shadowIntensity1 = (lnDepth1 - bias) > SampleShadowTexture(index, laPosition) ? 1 : 0;

        float len = csmShadowReceiverInfo.thresholdVZ[cascadIndex] - csmShadowReceiverInfo.thresholdVZ[cascadIndex + 1];
        float pre = csmShadowReceiverInfo.thresholdVZ[cascadIndex] - vPosition.z;
        float next = vPosition.z - csmShadowReceiverInfo.thresholdVZ[cascadIndex + 1];

        return shadowIntensity0 * pre / len + shadowIntensity1 * next / len;
    }

    // return shadowIntensity0;
}

float GetShadowIntensityWithVNormal(in vec3 vPosition, in vec3 vNormal)
{
    int cascadIndex = -1;
    for(int i = 0; i < CASCADE_COUNT * 2; i++)
    {
        if(vPosition.z > csmShadowReceiverInfo.thresholdVZ[i])
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
        int index = (cascadIndex + 1) / 2;

        float bias = 0;
        float cosLightWithNormal = dot(vNormal, -csmShadowReceiverInfo.vLightDirection);
        if(cosLightWithNormal <= 0)
        {
            bias = csmShadowReceiverInfo.bias[index].x;
        }
        else
        {
            bias = max(csmShadowReceiverInfo.bias[index].y * (1.0 - cosLightWithNormal), csmShadowReceiverInfo.bias[index].x);
        }
   
        vec4 lpPosition = csmShadowReceiverInfo.matrixVC2PL[index] * vec4(vPosition, 1);
        vec3 lnPosition = lpPosition.xyz / lpPosition.w;
        lnDepth0 = lnPosition.z;
        vec2 laPosition = (clamp(lnPosition.xy, -1, 1) + vec2(1, -1)) / vec2(2, -2);

        int inShadowCount = 0;
        for(int i = -csmShadowReceiverInfo.sampleHalfWidth; i <= csmShadowReceiverInfo.sampleHalfWidth; i++)
        {
            for(int j = -csmShadowReceiverInfo.sampleHalfWidth; j <= csmShadowReceiverInfo.sampleHalfWidth; j++)
            {
                vec2 offsetAPosition = clamp(laPosition + vec2(i, j) * csmShadowReceiverInfo.texelSize[index], 0, 1);
                inShadowCount += ((lnDepth0 - bias) > SampleShadowTexture(index, offsetAPosition) ? 1 : 0);
            }
        }
        shadowIntensity0 = float(inShadowCount) / float((csmShadowReceiverInfo.sampleHalfWidth * 2 + 1) * (csmShadowReceiverInfo.sampleHalfWidth * 2 + 1));
        
        // shadowIntensity0 = (lnDepth0 - bias) > SampleShadowTexture(index, laPosition) ? 1 : 0;
    }

    if(cascadIndex % 2 == 0)
    {
        return shadowIntensity0;
    }
    else
    {
        int index = cascadIndex / 2;

        float bias = 0;
        float cosLightWithNormal = dot(vNormal, -csmShadowReceiverInfo.vLightDirection);
        if(cosLightWithNormal <= 0)
        {
            bias = csmShadowReceiverInfo.bias[index].x;
        }
        else
        {
            bias = max(csmShadowReceiverInfo.bias[index].y * (1.0 - cosLightWithNormal), csmShadowReceiverInfo.bias[index].x);
        }

        vec4 lpPosition = csmShadowReceiverInfo.matrixVC2PL[index] * vec4(vPosition, 1);
        vec3 lnPosition = lpPosition.xyz / lpPosition.w;
        lnDepth1 = lpPosition.z;
        vec2 laPosition = (clamp(lnPosition.xy, -1, 1) + vec2(1, -1)) / vec2(2, -2);

        int inShadowCount = 0;
        for(int i = -csmShadowReceiverInfo.sampleHalfWidth; i <= csmShadowReceiverInfo.sampleHalfWidth; i++)
        {
            for(int j = -csmShadowReceiverInfo.sampleHalfWidth; j <= csmShadowReceiverInfo.sampleHalfWidth; j++)
            {
                vec2 offsetAPosition = clamp(laPosition + vec2(i, j) * csmShadowReceiverInfo.texelSize[index], 0, 1);
                inShadowCount += ((lnDepth1 - bias) > SampleShadowTexture(index, offsetAPosition) ? 1 : 0);
            }
        }
        shadowIntensity1 = float(inShadowCount) / float((csmShadowReceiverInfo.sampleHalfWidth * 2 + 1) * (csmShadowReceiverInfo.sampleHalfWidth * 2 + 1));
        
        // shadowIntensity1 = (lnDepth1 - bias) > SampleShadowTexture(index, laPosition) ? 1 : 0;

        float len = csmShadowReceiverInfo.thresholdVZ[cascadIndex] - csmShadowReceiverInfo.thresholdVZ[cascadIndex + 1];
        float pre = csmShadowReceiverInfo.thresholdVZ[cascadIndex] - vPosition.z;
        float next = vPosition.z - csmShadowReceiverInfo.thresholdVZ[cascadIndex + 1];

        return shadowIntensity0 * pre / len + shadowIntensity1 * next / len;
    }

    // return shadowIntensity0;
}

#endif ///#ifdef CSM_SHADOW_RECEIVER_DESCRIPTOR_START_INDEX

#endif ///#ifndef _CSM_SHADOW_RECEIVER_GLSL_