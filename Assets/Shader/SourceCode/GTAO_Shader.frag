#version 450
#extension GL_GOOGLE_include_directive: enable

#include "Camera.glsl"
#include "Object.glsl"

#define PI (acos(-1.0))
#define DOUBLE_PI (2 * acos(-1.0))
#define HALF_PI (0.5 * acos(-1.0))

layout(set = 0, binding = 0) uniform _CameraInfo
{
    CameraInfo info;
} cameraInfo;
layout(set = 1, binding = 0) uniform sampler2D noiseTexture;
layout(set = 2, binding = 0) uniform sampler2D depthTexture;
layout(set = 3, binding = 0) uniform sampler2D normalTexture;
layout(set = 4, binding = 0) uniform sampler2D positionTexture;
layout(set = 5, binding = 0) uniform GtaoInfo
{
    vec2 attachmentSize;
    vec2 attachmentTexelSize;
    float sampleRadius;
    float sampleBiasAngle;
    int stepCount;
    int sliceCount;
    int noiseTextureWidth;
} gtaoInfo;

layout(location = 0) out float OcclusionTexture;

void main()
{
    vec2 aPosition = gl_FragCoord.xy * gtaoInfo.attachmentTexelSize;

    vec3 vPosition = texture(positionTexture, aPosition).rgb;
    vec3 vNormal =  DirectionTransition(texture(normalTexture, aPosition).rgb, mat3(cameraInfo.info.view));
    vec3 vDir = normalize(0 - vPosition);

    float noiseRadian = texture(noiseTexture, mod(gl_FragCoord.xy, gtaoInfo.noiseTextureWidth) / gtaoInfo.noiseTextureWidth).r * DOUBLE_PI;

    float occlusion = 0;
    for(int i = 0; i < gtaoInfo.sliceCount; i++)
    {
        float directionRadian = (i + noiseRadian)/ gtaoInfo.sliceCount * PI ;
        vec3 direction = vec3(cos(directionRadian), sin(directionRadian), 0);
        vec3 vStep = direction * gtaoInfo.sampleRadius / gtaoInfo.stepCount;

        vec3 sliceTangentDirection = normalize(cross(direction, vDir));
        vec3 slicePlainNormal = vNormal - dot(vNormal, sliceTangentDirection) * sliceTangentDirection;
        vec3 slicePlainNormalDirection = normalize(slicePlainNormal );

        float theta = acos(dot(slicePlainNormalDirection, vDir));
        theta = dot(slicePlainNormalDirection, direction) < 0 ? theta : -theta;
        float rightThreshold = theta - HALF_PI + gtaoInfo.sampleBiasAngle * PI / 180;
        float leftThreshold = theta + HALF_PI - gtaoInfo.sampleBiasAngle * PI / 180;

        float maxRightHorizonRadian = theta - HALF_PI;
        ///Right
        for(int j = 0; j < gtaoInfo.stepCount; j++)
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
                if (abs(vector.z) >  gtaoInfo.sampleRadius) continue;

                float horizonRadian = asin(vector.z / (length(vector))) - HALF_PI;
                maxRightHorizonRadian = maxRightHorizonRadian < horizonRadian ? horizonRadian : maxRightHorizonRadian;
            }
        }
        // float rh = clamp(maxRightHorizonRadian, theta - HALF_PI + gtaoInfo.sampleBiasAngle * PI / 180, 0);
    

        float rao = maxRightHorizonRadian < rightThreshold ? 2 : -cos(2 * maxRightHorizonRadian - theta) + cos(theta) + 2 * maxRightHorizonRadian * sin(theta);
        // float rao = -cos(2 * maxRightHorizonRadian - theta) + cos(theta) + 2 * maxRightHorizonRadian * sin(theta);
        // float rh = clamp(maxRightHorizonRadian, - HALF_PI, 0);
        // float rao = 1 - cos(rh);

        float minLeftHorizonRadian = theta + HALF_PI;
        ///Left
        for(int j = 0; j < gtaoInfo.stepCount; j++)
        {
            vec3 sampleVPosition = vPosition - vStep * (j + 0.5);
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
                if (abs(vector.z) >  gtaoInfo.sampleRadius) continue;

                float horizonRadian = HALF_PI - asin(vector.z / length(vector));
                minLeftHorizonRadian = minLeftHorizonRadian > horizonRadian ? horizonRadian : minLeftHorizonRadian;
            }
           
        }
        // float lh = clamp(minLeftHorizonRadian, 0, theta + HALF_PI - gtaoInfo.sampleBiasAngle * PI / 180);
        // float lh = minLeftHorizonRadian;
        float lao = minLeftHorizonRadian > leftThreshold ? 2 : -cos(2 * minLeftHorizonRadian - theta) + cos(theta) + 2 * minLeftHorizonRadian * sin(theta);
        // float lao = -cos(2 * minLeftHorizonRadian - theta) + cos(theta) + 2 * minLeftHorizonRadian * sin(theta);
        // float lh = clamp(minLeftHorizonRadian, 0, HALF_PI);
        // float lao = 1 - cos(lh);

        //rao = 2;
        //lao = 2;
        float ao = 1 - 0.25 * (rao + lao);

        occlusion += ao;
    }

    OcclusionTexture = clamp(occlusion / gtaoInfo.sliceCount, 0, 1);
}
