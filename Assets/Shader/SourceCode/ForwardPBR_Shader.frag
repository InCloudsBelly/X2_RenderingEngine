#version 450
#extension GL_GOOGLE_include_directive: enable


#include "Camera.glsl"
#include "Object.glsl"
#include "Light.glsl"

#define SHADOW_CEVSM


#define MAX_ORTHER_LIGHT_COUNT 4

#ifdef SHADOW_CSM
   #define CSM_SHADOW_RECEIVER_DESCRIPTOR_START_INDEX 12
   #include "CSM_ShadowReceiver.glsl"
#endif

#ifdef SHADOW_CEVSM
   #define CASCADE_EVSM_SHADOW_RECEIVER_DESCRIPTOR_START_INDEX 12
   #include "CascadeEVSM_ShadowReceiver.glsl"
#endif

layout(set = 0, binding = 0) uniform _CameraInfo
{
    CameraInfo info;
} cameraInfo;

layout(set = 1, binding = 0) uniform MeshObjectInfo
{
    ObjectInfo info;
} meshObjectInfo;

layout(set = 2, binding = 0) uniform LightInfos
{
    LightInfo ambientLightInfo;
    LightInfo mainLightInfo;
    int importantLightCount;
    LightInfo[MAX_ORTHER_LIGHT_COUNT] importantLightInfos;
    int unimportantLightCount;
    LightInfo[MAX_ORTHER_LIGHT_COUNT] unimportantLightInfos;
} lightInfos;

layout(set = 3, binding = 0) uniform sampler2D albedoTexture;
layout(set = 4, binding = 0) uniform sampler2D metallicTexture;
layout(set = 5, binding = 0) uniform sampler2D roughnessTexture;
layout(set = 6, binding = 0) uniform sampler2D emissiveTexture;
layout(set = 7, binding = 0) uniform sampler2D ambientOcclusionTexture;
layout(set = 8, binding = 0) uniform sampler2D normalTexture;

// IBL Samplers
layout(set = 9, binding = 0) uniform samplerCube irradianceCubeImage;
layout(set = 10, binding = 0) uniform sampler2D lutImage;
layout(set = 11, binding = 0) uniform samplerCube prefilteredCubeImage;

layout(location = 0) in vec2 inTexCoords;
layout(location = 1) in vec3 inWorldPosition;
layout(location = 2) in vec3 inWorldNormal;
layout(location = 3) in vec3 inWorldTangent;
layout(location = 4) in vec3 inWorldBitangent;

layout(location = 0) out vec4 ColorAttachment;

struct Material
{
   vec3 albedo;
   float metallicFactor;
   float roughnessFactor;
   vec3 emissiveColor;
   float AO;
};

struct PBRinfo
{
   // cos angle between normal and light direction.
	float NdotL;          
   // cos angle between normal and view direction.
	float NdotV;          
   // cos angle between normal and half vector.
	float NdotH;          
   // cos angle between view direction and half vector.
	float VdotH;
   // Roughness value, as authored by the model creator.
    float perceptualRoughness;
   // Roughness mapped to a more linear value.
    float alphaRoughness;
   // color contribution from diffuse lighting.
	vec3 diffuseColor;    
   // color contribution from specular lighting.
	vec3 specularColor;

    // full reflectance color(normal incidence angle)
    vec3 reflectance0;
   // reflectance color at grazing angle
    vec3 reflectance90;
};

struct IBLinfo
{
   vec3 diffuseLight;
   vec3 specularLight;
   vec2 brdf;
};


//////////////////////////////////////PBR//////////////////////////////////////

float distributionGGX(float nDotH, float rough);
float geometricOcclusion(PBRinfo pbrInfo);
vec3 fresnelSchlick(PBRinfo pbrInfo);

///////////////////////////////////////////////////////////////////////////////

vec3 calculateNormal();
vec3 calculateDirLight(int i,vec3 normal,vec3 view,Material material,PBRinfo pbrInfo);
vec3 calculatePointLight(int i,vec3 normal,vec3 view,Material material,PBRinfo pbrInfo);
vec3 calculateSpotLight(int i,vec3 normal,vec3 view,Material material,PBRinfo pbrInfo);

vec3 calculateDirLight(int i, Material material, PBRinfo pbrInfo);
void calculatePointLight();

float filterPCF(vec3 shadowCoords);
float calculateShadow(vec3 shadowCoords, vec2 off);

vec3 getIBLcontribution(PBRinfo pbrInfo, IBLinfo iblInfo, Material material);

vec3 getSHIrradianceContribution(vec3 dir);

float ambient = 0.5;

void main()
{
    vec3 normal = calculateNormal();
    vec3 view =  CameraWObserveDirection(inWorldPosition, cameraInfo.info);
    vec3 reflection = normalize(reflect( view, normal));
    reflection.y = reflection.y;

    Material material;
    {
        material.albedo = texture(albedoTexture, inTexCoords).rgb;
        
        material.metallicFactor = texture(metallicTexture, inTexCoords).b;
        material.roughnessFactor = texture(roughnessTexture, inTexCoords).g;
        
        material.AO = texture(ambientOcclusionTexture, inTexCoords).r;
        material.AO = (material.AO < 0.01) ? 1.0 : material.AO;
        material.emissiveColor = texture(emissiveTexture, inTexCoords).rgb;
    }

    PBRinfo pbrInfo;
    {
        float F0 = 0.04;

        pbrInfo.NdotV = clamp(dot(normal, -view), 0.001, 1.0);
        
        pbrInfo.diffuseColor = material.albedo.rgb * (vec3(1.0) - vec3(F0));
        pbrInfo.diffuseColor *= 1.0 - material.metallicFactor;
      
        pbrInfo.specularColor = mix(vec3(F0),material.albedo,material.metallicFactor);

        pbrInfo.perceptualRoughness = clamp(material.roughnessFactor, 0.04, 1.0);
        //alpha = r*r
        pbrInfo.alphaRoughness = (pbrInfo.perceptualRoughness * pbrInfo.perceptualRoughness);

        // Reflectance
        float reflectance = max(max(pbrInfo.specularColor.r, pbrInfo.specularColor.g),pbrInfo.specularColor.b);
        // - For typical incident reflectance range (between 4% to 100%) set the
        // grazing reflectance to 100% for typical fresnel effect.
	    // - For very low reflectance range on highly diffuse objects (below 4%),
        // incrementally reduce grazing reflecance to 0%.
        pbrInfo.reflectance0 = pbrInfo.specularColor.rgb;
        pbrInfo.reflectance90 = vec3(clamp(reflectance * 25.0, 0.0, 1.0));
    }

    IBLinfo iblInfo;
    {
        // HDR textures are already linear
        iblInfo.diffuseLight = texture(irradianceCubeImage, vec3(normal.x, normal.y, normal.z)).rgb;

        float mipCount = float(textureQueryLevels(prefilteredCubeImage));
        float lod = pbrInfo.perceptualRoughness * mipCount;

        iblInfo.brdf = texture(lutImage, vec2(max(pbrInfo.NdotV, 0.0),  pbrInfo.perceptualRoughness)).rg;
        iblInfo.specularLight = textureLod(prefilteredCubeImage,reflection.xyz,lod).rgb;
   }

    vec3 color = getIBLcontribution(pbrInfo, iblInfo, material);

   float shadowIntensity = 0;
#ifdef SHADOW_CSM
   shadowIntensity = GetShadowIntensity((cameraInfo.info.view * vec4(inWorldPosition, 1)).xyz, normal);
#endif

#ifdef SHADOW_CEVSM
   shadowIntensity = GetShadowIntensity((cameraInfo.info.view * vec4(inWorldPosition, 1)).xyz);
#endif

   color += (1 - shadowIntensity) * PbrLighting(lightInfos.mainLightInfo, inWorldPosition, view, normal, pbrInfo.diffuseColor, pbrInfo.perceptualRoughness, material.metallicFactor);


    // AO
    color = material.AO * color;

    // Emissive
    color = material.emissiveColor + color;

    color = pow(color,vec3(1.0 / 2.2));

    ColorAttachment = ambient * vec4(color, 1.0);

   //ColorAttachment = vec4(PbrLighting(lightInfos.mainLightInfo, inWorldPosition, view, normal, pbrInfo.diffuseColor, pbrInfo.perceptualRoughness, material.metallicFactor), 1.0f);

//    ColorAttachment =  vec4(iblInfo.specularLight * (pbrInfo.specularColor* iblInfo.brdf.x + iblInfo.brdf.y) , 1.0);
    

//    vec3 shadowCoords = inShadowCoords.xyz / inShadowCoords.w;
//    shadowCoords.xy = shadowCoords.xy * 0.5 + 0.5;
//    outColor = vec4( shadowCoords.z - texture(shadowMapSampler, shadowCoords.xy).r); 

}

vec3 getIBLcontribution(PBRinfo pbrInfo, IBLinfo iblInfo, Material material)
{

   vec3 diffuse = iblInfo.diffuseLight * pbrInfo.diffuseColor;
   vec3 specular = (iblInfo.specularLight *(pbrInfo.specularColor * iblInfo.brdf.x + iblInfo.brdf.y));

   return diffuse + specular;
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

///////////////////////////////PBR - Helper functions//////////////////////////

/*
 * Trowbridge-Reitz GGX approximation.
 */
float distributionGGX(float nDotH, float rough)
{
   float a = rough * rough;
   float a2 = a * a;

   float denominator = nDotH * nDotH * (a2 - 1.0) + 1.0;
   denominator = 1 / (PI * denominator * denominator);

   return a2 * denominator;
}

float geometricOcclusion(PBRinfo pbrInfo)
{
   float alphaRoughness2 = pbrInfo.alphaRoughness * pbrInfo.alphaRoughness;
   float NdotL2 = pbrInfo.NdotL * pbrInfo.NdotL;
   float NdotV2 = pbrInfo.NdotV * pbrInfo.NdotV;

   float attenuationL = (
         2.0 * pbrInfo.NdotL /
         (
            pbrInfo.NdotL +
            sqrt(alphaRoughness2 + (1.0 - alphaRoughness2) * (NdotL2))
         )
   );

   float attenuationV = (
         2.0 * pbrInfo.NdotV /
         (
            pbrInfo.NdotV +
            sqrt(alphaRoughness2 + (1.0 - alphaRoughness2) * (NdotV2))
         )
   );

   return attenuationL * attenuationV;
}

/*
 * Fresnel Schlick approximation(for specular reflection).
 */
vec3 fresnelSchlick(PBRinfo pbrInfo)
{
    return (pbrInfo.reflectance0 + (pbrInfo.reflectance90 - pbrInfo.reflectance0) *pow(clamp(1.0 - pbrInfo.VdotH, 0.0, 1.0), 5.0));
}