#version 450
#extension GL_GOOGLE_include_directive: enable

#include "Camera.glsl"
#include "Object.glsl"

#define PI (acos(-1.0))
#define DOUBLE_PI (2 * acos(-1.0))

layout(set = 0, binding = 0) uniform _CameraInfo
{
    CameraInfo info;
} cameraInfo;
layout(set = 1, binding = 0) uniform sampler2D noiseTexture;
layout(set = 2, binding = 0) uniform sampler2D depthTexture;
layout(set = 3, binding = 0) uniform sampler2D normalTexture;
layout(set = 4, binding = 0) uniform sampler2D positionTexture;
layout(set = 5, binding = 0) uniform HbaoInfo
{
    vec2 attachmentSize;
    vec2 attachmentTexelSize;
    float sampleRadius;
    float sampleBiasAngle;
    int stepCount;
    int directionCount;
    int noiseTextureWidth;
} hbaoInfo;

layout(location = 0) out float OcclusionTexture;

void main()
{
    vec2 aPosition = gl_FragCoord.xy * hbaoInfo.attachmentTexelSize;

    vec3 vPosition = texture(positionTexture, aPosition).rgb;
    vec3 vNormal =  DirectionTransition(texture(normalTexture, aPosition).rgb, mat3(cameraInfo.info.view));
    
    float noiseRadian = texture(noiseTexture, mod(gl_FragCoord.xy, hbaoInfo.noiseTextureWidth) / hbaoInfo.noiseTextureWidth).r * DOUBLE_PI;

    float occlusion = 0;
    for(int i = 0; i < hbaoInfo.directionCount; i++)
    {
    
        float directionRadian = 1.0 * i / hbaoInfo.directionCount * DOUBLE_PI + noiseRadian;
        vec3 direction = vec3(cos(directionRadian), sin(directionRadian), 0);
        vec3 vStep = direction * hbaoInfo.sampleRadius / hbaoInfo.stepCount;
        float sinTangent = -dot(direction, vNormal);
        float minSinHorizon = clamp(sin(hbaoInfo.sampleBiasAngle * PI / 180), 0, 1);

        float previousAo = 0;
        float wao = 0;
        for(int j = 0; j < hbaoInfo.stepCount; j++)
        {
            vec3 sampleVPosition = vPosition + vStep * (j + 0.5);
            vec4 samplePPosition = cameraInfo.info.projection * vec4(sampleVPosition, 1);
            if(samplePPosition.w == 0) continue;
            vec3 sampleNdcPosition = samplePPosition.xyz / samplePPosition.w;

            vec2 samplePointTexCoords = clamp(PositionN2A(sampleNdcPosition.xy), 0, 1);
            if(samplePointTexCoords.x != 0 && samplePointTexCoords.x != 1 && samplePointTexCoords.y != 0 && samplePointTexCoords.y != 1)
            {
                float samplePointNdcDepth = sampleNdcPosition.z;
                float visiableSamplePointNdcDepth = texture(depthTexture, samplePointTexCoords).r;
                vec3 visiableSamplePointVPosition = vec3(sampleVPosition.xy, DepthN2V(visiableSamplePointNdcDepth, cameraInfo.info));
                
                vec3 vector = visiableSamplePointVPosition - vPosition;
                vector.z = clamp(vector.z, -sqrt(1 - dot(vector.xy, vector.xy)) * hbaoInfo.sampleRadius, sqrt(1 - dot(vector.xy, vector.xy)) * hbaoInfo.sampleRadius);

                float sinHorizon = vector.z / length(vector);
                float ao = clamp(sinHorizon - sinTangent - minSinHorizon, 0, 1);
                if(ao >= previousAo)
                {
                    wao += (1 - dot(vector, vector) / pow(hbaoInfo.sampleRadius, 2)) * (ao - previousAo);
                    previousAo = ao;
                }
            }
        }
        occlusion += wao;
    }

    //OcclusionTexture = vec4(vec3(occlusion / hbaoInfo.directionCount),1.0f);
    OcclusionTexture = occlusion / hbaoInfo.directionCount;

}
