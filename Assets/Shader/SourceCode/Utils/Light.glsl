#ifndef _LIGHT_GLSL_
#define _LIGHT_GLSL_

#define INVALID_LIGHT 0
#define DIRECTIONL_LIGHT 1
#define POINT_LIGHT 2
#define SKYBOX_LIGHT 3
#define SPOT_LIGHT 4

#define FORWARD_LIGHTING 0
#define TILE_BASED_FORWARD_LIGHTING 1

#define PI 3.1415926535
#define ANGLE_TO_RADIANS (3.1415926535 / 180.0)
#define RADIANS_TO_ANGLE (180.0 / 3.1415926535)

struct LightInfo
{
    int type;
    float intensity;
    float minRange;
    float maxRange;
    vec4 extraParameter;
    vec3 position;
    vec3 direction;
    vec4 color;
};

struct LightBoundingBoxInfo
{
    vec3[8] vertexes;
};

vec3 DiffuseDirectionalLighting(in LightInfo lightInfo, in vec3 worldNormal)
{
    vec4 color = lightInfo.intensity * lightInfo.color * max(0, dot(normalize(worldNormal), -normalize(lightInfo.direction)));
    return color.xyz;
}

vec3 SpecularDirectionalLighting(in LightInfo lightInfo, in vec3 worldView, in vec3 worldNormal, in float gloss)
{
    vec3 worldReflect = normalize(reflect(normalize(lightInfo.direction), normalize(worldNormal)));
    vec3 inverseWorldView = normalize(-worldView);
    vec4 color = lightInfo.intensity * lightInfo.color * pow(max(0, dot(worldReflect, inverseWorldView)), gloss);
    return color.xyz;
}

#define PBR_LIGHTING(outRadiance, lightRadiance, wLight, wView, wNormal, albedo, roughness, metallic) \
{ \
    vec3 wH = normalize(-wView - wLight); \
 \
    float coshv = max(dot(wH, -wView), 0); \
    float coshn = max(dot(wNormal, wH), 0); \
    float cosnv = max(dot(wNormal, -wView), 0); \
    float cosnl = max(dot(wNormal, -wLight), 0); \
 \
    vec3 fresnel; \
    { \
        vec3 F0 = mix(vec3(0.04), albedo, metallic); \
        fresnel = F0 + (1.0 - F0) * pow(1.0 - coshv, 5.0); \
    } \
 \
    float distribution; \
    { \
        float a2 = roughness * roughness; \
        float cosTheta2 = coshn * coshn; \
\
        float d = (cosTheta2 * (a2 - 1) + 1); \
\
        distribution = a2 / (PI * d * d); \
    } \
 \
    float inGeomery; \
    { \
        float r = (roughness + 1.0); \
        float k = (r * r) / 8.0; \
\
        inGeomery = cosnl / (cosnl * (1 - k) + k); \
    } \
 \
    float outGeomery; \
    { \
        float r = (roughness + 1.0); \
        float k = (r * r) / 8.0; \
\
        outGeomery = cosnv / (cosnv * (1 - k) + k); \
    } \
 \
    float geometry = inGeomery * outGeomery; \
 \
    vec3 kd = (vec3(1) - fresnel) * (1 - metallic); \
 \
    vec3 diffuse = kd * albedo / PI; \
    vec3 specular = distribution * fresnel * geometry / ( 4 * cosnv * cosnl + 0.0001); \
 \
    outRadiance = (diffuse + specular) * lightRadiance * cosnl; \
} 

#define PBR_DIRECTIONAL_LIGHTING(outRadiance, lightInfo, wView, wNormal, albedo, roughness, metallic) \
{ \
    vec3 wLight = lightInfo.direction; \
    vec3 lightRadiance = lightInfo.intensity * lightInfo.color.rgb; \
 \
    PBR_LIGHTING(outRadiance, lightRadiance, wLight, wView, wNormal, albedo, roughness, metallic);\
}

#define PBR_POINT_LIGHTING(outRadiance, lightInfo, wPosition, wView, wNormal, albedo, roughness, metallic) \
{ \
    float d = distance(lightInfo.position, wPosition); \
    float k1 = 1.0 / max(lightInfo.minRange, d); \
    float disAttenuation = k1 * k1; \
    float win = pow(max(1 - pow(d / lightInfo.maxRange, 4), 0), 2); \
 \
    vec3 wLight = normalize(wPosition - lightInfo.position); \
    vec3 lightRadiance = lightInfo.intensity * disAttenuation * win * lightInfo.color.rgb; \
 \
    PBR_LIGHTING(outRadiance, lightRadiance, wLight, wView, wNormal, albedo, roughness, metallic);\
}

#define PBR_SPOT_LIGHTING(outRadiance, lightInfo, wPosition, wView, wNormal, albedo, roughness, metallic) \
{ \
    vec3 wLight = normalize(wPosition - lightInfo.position); \
\
    float d = distance(lightInfo.position, wPosition); \
    float k1 = 1.0 / max(lightInfo.minRange, d); \
    float disAttenuation = k1 * k1; \
 \
    float win = pow(max(1 - pow(d / lightInfo.maxRange, 4), 0), 2); \
 \
    float innerCos = cos(ANGLE_TO_RADIANS * lightInfo.extraParameter.x); \
    float outerCos = cos(ANGLE_TO_RADIANS * lightInfo.extraParameter.y); \
    float dirCos = dot(lightInfo.direction, wLight); \
    float t = clamp((dirCos - outerCos) / (innerCos - outerCos), 0.0, 1.0); \
    float dirAttenuation = t * t; \
 \
    vec3 lightRadiance = lightInfo.intensity * dirAttenuation * disAttenuation * win * lightInfo.color.rgb; \
 \
    PBR_LIGHTING(outRadiance, lightRadiance, wLight, wView, wNormal, albedo, roughness, metallic);\
}

vec3 PbrLighting(in LightInfo lightInfo, in vec3 wPosition, in vec3 wView, in vec3 wNormal, in vec3 albedo, float roughness, float metallic)
{
    vec3 outRadiance;
    switch(lightInfo.type)
    {
        case INVALID_LIGHT:
        {
            outRadiance = vec3(0, 0, 0);
            break;
        }
        case DIRECTIONL_LIGHT:
        {
            PBR_DIRECTIONAL_LIGHTING(outRadiance, lightInfo, wView, wNormal, albedo, roughness, metallic);
            break;
        }
        case POINT_LIGHT:
        {
            PBR_POINT_LIGHTING(outRadiance, lightInfo, wPosition, wView, wNormal, albedo, roughness, metallic);
            break;
        }
        case SPOT_LIGHT:
        {
            PBR_SPOT_LIGHTING(outRadiance, lightInfo, wPosition, wView, wNormal, albedo, roughness, metallic);
            break;
        }

    }
    return outRadiance;
}

#define PBR_IBL_LIGHTING(outRadiance, lightInfo, wPosition, wView, wNormal, albedo, roughness, metallic, irradianceImage, prefilteredImage, lutImage) \
{ \
    vec3 wiLight = reflect(wView, wNormal); \
    float cosnv = max(dot(wNormal, -wView), 0); \
 \
    vec3 normalSampleDirection = vec3(wNormal.xy, -wNormal.z); \
    vec3 iLightSampleDirection = vec3(wiLight.xy, -wiLight.z); \
 \
    float maxPrefilteredImageRoughnessLevelIndex = lightInfo.extraParameter.x; \
 \
    vec3 fresnel; \
    { \
        vec3 F0 = mix(vec3(0.04), albedo, metallic); \
        fresnel = F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(1.0 - cosnv, 5.0); \
    } \
 \
    vec3 irradiance = texture(irradianceImage, normalSampleDirection).rgb; \
    vec3 diffuse = (vec3(1.0) - fresnel) * (1 - metallic) * albedo * irradiance; \
 \
    vec3 prefiltered = textureLod(prefilteredImage, iLightSampleDirection,  roughness * maxPrefilteredImageRoughnessLevelIndex).rgb; \
    vec2 parameter = vec2(0, 0); \
    { \
        uvec4 packed4 = texture(lutImage, vec2(cosnv, roughness)).rgba; \
        uint packed = (packed4.r << 24) | (packed4.g << 16) | (packed4.b << 8) | (packed4.a); \
        parameter = unpackHalf2x16(packed); \
    } \
    vec3 specular = prefiltered * (fresnel * parameter.x + parameter.y); \
 \
    outRadiance = (diffuse + specular) * lightInfo.color.rgb * lightInfo.intensity; \
}

vec3 DiffusePointLighting(in LightInfo lightInfo, in vec3 worldNormal, in vec3 worldPosition)
{
    vec3 lightingDirection = normalize(worldPosition - lightInfo.position);
    float d = distance(lightInfo.position, worldPosition);
    float k1 = 1.0 / max(lightInfo.minRange, d);
    float disAttenuation = k1 * k1;

    float win = pow(max(1 - pow(d / lightInfo.maxRange, 4), 0), 2);
    
    vec4 color = lightInfo.intensity * disAttenuation * win * lightInfo.color * max(0, dot(worldNormal, -lightingDirection));
    return color.xyz;
}

vec3 SpecularPointLighting(in LightInfo lightInfo, in vec3 worldView, in vec3 worldPosition, in vec3 worldNormal, in float gloss)
{
    vec3 worldReflect = normalize(reflect(normalize(worldPosition - lightInfo.position), worldNormal));
    vec3 inverseWorldView = normalize(-worldView);

    vec3 lightingDirection = normalize(worldPosition - lightInfo.position);
    float d = distance(lightInfo.position, worldPosition);
    float k1 = 1.0 / max(lightInfo.minRange, d);
    float disAttenuation = k1 * k1;

    float win = pow(max(1 - pow(d / lightInfo.maxRange, 4), 0), 2);
    
    vec4 color = lightInfo.intensity * disAttenuation * win * lightInfo.color * pow(max(0, dot(worldReflect, inverseWorldView)), gloss);
    return color.xyz;
}

vec3 DiffuseSpotLighting(in LightInfo lightInfo, in vec3 worldNormal, in vec3 worldPosition)
{
    vec3 lightingDirection = normalize(worldPosition - lightInfo.position);
    float d = distance(lightInfo.position, worldPosition);
    float k1 = 1.0 / max(lightInfo.minRange, d);
    float disAttenuation = k1 * k1;

    float win = pow(max(1 - pow(d / lightInfo.maxRange, 4), 0), 2);

    float innerCos = cos(ANGLE_TO_RADIANS * lightInfo.extraParameter.x);
    float outerCos = cos(ANGLE_TO_RADIANS * lightInfo.extraParameter.y);
    float dirCos = dot(normalize(lightInfo.direction), lightingDirection);
    float t = clamp((dirCos - outerCos) / (innerCos - outerCos), 0.0, 1.0);
    float dirAttenuation = t * t;

    vec4 color = lightInfo.intensity * dirAttenuation * disAttenuation * win * lightInfo.color * max(0, dot(worldNormal, -lightingDirection));
    return color.xyz;
}

vec3 SpecularSpotLighting(in LightInfo lightInfo, in vec3 worldView, in vec3 worldPosition, in vec3 worldNormal, in float gloss)
{
    vec3 worldReflect = normalize(reflect(normalize(worldPosition - lightInfo.position), worldNormal));
    vec3 inverseWorldView = normalize(-worldView);

    vec3 lightingDirection = normalize(worldPosition - lightInfo.position);
    float d = distance(lightInfo.position, worldPosition);
    float k1 = 1.0 / max(lightInfo.minRange, d);
    float disAttenuation = k1 * k1;

    float win = pow(max(1 - pow(d / lightInfo.maxRange, 4), 0), 2);

    float innerCos = cos(ANGLE_TO_RADIANS * lightInfo.extraParameter.x);
    float outerCos = cos(ANGLE_TO_RADIANS * lightInfo.extraParameter.y);
    float dirCos = dot(normalize(lightInfo.direction), lightingDirection);
    float t = clamp((dirCos - outerCos) / (innerCos - outerCos), 0.0, 1.0);
    float dirAttenuation = t * t;

    vec4 color = lightInfo.intensity * dirAttenuation * disAttenuation * win * lightInfo.color * pow(max(0, dot(worldReflect, inverseWorldView)), gloss);
    return color.xyz;
}

vec3 DiffuseLighting(in LightInfo lightInfo, in vec3 worldNormal, in vec3 worldPosition)
{
    switch(lightInfo.type)
    {
        case INVALID_LIGHT:
        {
            return vec3(0, 0, 0);
        }
        case DIRECTIONL_LIGHT:
        {
            return DiffuseDirectionalLighting(lightInfo, worldNormal);
        }
        case POINT_LIGHT:
        {
            return DiffusePointLighting(lightInfo, worldNormal, worldPosition);
        }
        case SPOT_LIGHT:
        {
            return DiffuseSpotLighting(lightInfo, worldNormal, worldPosition);
        }

    }
    return vec3(0, 0, 0);
}

vec3 SpecularLighting(in LightInfo lightInfo, in vec3 worldView, in vec3 worldPosition, in vec3 worldNormal, in float gloss)
{
    switch(lightInfo.type)
    {
        case INVALID_LIGHT:
        {
            return vec3(0, 0, 0);
        }
        case DIRECTIONL_LIGHT:
        {
            return SpecularDirectionalLighting(lightInfo, worldView, worldNormal, gloss);
        }
        case POINT_LIGHT:
        {
            return SpecularPointLighting(lightInfo, worldView, worldPosition, worldNormal, gloss);
        }
        case SPOT_LIGHT:
        {
            return SpecularSpotLighting(lightInfo, worldView, worldPosition, worldNormal, gloss);
        }
    }
    return vec3(0, 0, 0);
}

#endif