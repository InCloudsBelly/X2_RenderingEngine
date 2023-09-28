﻿/*// -- PBR shader --
// -----------------------------
// Note: this shader is still very much in progress. There are likely many bugs and future additions that will go in.
//       Currently heavily updated. 
//
// References upon which this is based:
// - Unreal Engine 4 PBR notes (https://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf)
// - Frostbite's SIGGRAPH 2014 paper (https://seblagarde.wordpress.com/2015/07/14/siggraph-2014-moving-frostbite-to-physically-based-rendering/)
*/
#version 450 core
#pragma stage:vert

#include <Buffers.glslh>
#include <Lighting.glslh>
#include <ShadowMapping.glslh>

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec3 a_Tangent;
layout(location = 3) in vec3 a_Binormal;
layout(location = 4) in vec2 a_TexCoord;

// Transform buffer
layout(location = 5) in vec4 a_MRow0;
layout(location = 6) in vec4 a_MRow1;
layout(location = 7) in vec4 a_MRow2;

layout(location = 8) 	in vec4 a_MRowPrev0;
layout(location = 9) 	in vec4 a_MRowPrev1;
layout(location = 10) 	in vec4 a_MRowPrev2;


struct VertexOutput
{
	vec3 WorldPosition;
	vec3 Normal;
	vec2 TexCoord;
	mat3 WorldNormals;
	mat3 WorldTransform;
	vec3 Binormal;

	mat3 CameraView;

	vec3 ShadowMapCoords[4];
	vec3 ViewPosition;

	vec4 ndcPosCur;
	vec4 ndcPosPrev;
};

layout(location = 0) out VertexOutput Output;


// Make sure both shaders compute the exact same answer(PreDepth). 
// We need to have the same exact calculations to produce the gl_Position value (eg. matrix multiplications).
invariant gl_Position;

void main()
{
	mat4 transform = mat4(
		vec4(a_MRow0.x, a_MRow1.x, a_MRow2.x, 0.0),
		vec4(a_MRow0.y, a_MRow1.y, a_MRow2.y, 0.0),
		vec4(a_MRow0.z, a_MRow1.z, a_MRow2.z, 0.0),
		vec4(a_MRow0.w, a_MRow1.w, a_MRow2.w, 1.0)
	);
	vec4 worldPosition = transform * vec4(a_Position, 1.0);

	Output.WorldPosition = worldPosition.xyz;
	Output.Normal = mat3(transform) * a_Normal;
	Output.TexCoord = vec2(a_TexCoord.x, 1.0 - a_TexCoord.y);
	Output.WorldNormals = mat3(transform) * mat3(a_Tangent, a_Binormal, a_Normal);
	Output.WorldTransform = mat3(transform);
	Output.Binormal = a_Binormal;

	Output.CameraView = mat3(u_Camera.ViewMatrix);

	vec4 shadowCoords[4];
	shadowCoords[0] = u_DirShadow.DirLightMatrices[0] * vec4(Output.WorldPosition.xyz, 1.0);
	shadowCoords[1] = u_DirShadow.DirLightMatrices[1] * vec4(Output.WorldPosition.xyz, 1.0);
	shadowCoords[2] = u_DirShadow.DirLightMatrices[2] * vec4(Output.WorldPosition.xyz, 1.0);
	shadowCoords[3] = u_DirShadow.DirLightMatrices[3] * vec4(Output.WorldPosition.xyz, 1.0);
	Output.ShadowMapCoords[0] = vec3(shadowCoords[0].xyz / shadowCoords[0].w);
	Output.ShadowMapCoords[1] = vec3(shadowCoords[1].xyz / shadowCoords[1].w);
	Output.ShadowMapCoords[2] = vec3(shadowCoords[2].xyz / shadowCoords[2].w);
	Output.ShadowMapCoords[3] = vec3(shadowCoords[3].xyz / shadowCoords[3].w);

	Output.ViewPosition = vec3(u_Camera.ViewMatrix * vec4(Output.WorldPosition, 1.0));

	gl_Position = u_TAA.JitterdViewProjection* worldPosition;

	mat4 transformPrev = mat4(
		vec4(a_MRowPrev0.x, a_MRowPrev1.x, a_MRowPrev2.x, 0.0),
		vec4(a_MRowPrev0.y, a_MRowPrev1.y, a_MRowPrev2.y, 0.0),
		vec4(a_MRowPrev0.z, a_MRowPrev1.z, a_MRowPrev2.z, 0.0),
		vec4(a_MRowPrev0.w, a_MRowPrev1.w, a_MRowPrev2.w, 1.0)
	);

	vec4 posProjHistory = (u_TAA.ViewProjectionMatrixHistory * transformPrev * vec4(a_Position, 1.0));
	vec4 posProjCur = (u_Camera.ViewProjectionMatrix * transform * vec4(a_Position,1.0));
	
	Output.ndcPosCur = posProjCur;
	Output.ndcPosPrev = posProjHistory;
}



#version 450 core 

#pragma stage : frag 

#include <Buffers.glslh>
#include <PBR.glslh>
#include <Lighting.glslh>
#include <ShadowMapping.glslh>

 

// Constant normal incidence Fresnel factor for all dielectrics.
const vec3 Fdielectric = vec3(0.04);

struct VertexOutput
{
	vec3 WorldPosition;
	vec3 Normal;
	vec2 TexCoord;
	mat3 WorldNormals;
	mat3 WorldTransform;
	vec3 Binormal;

	mat3 CameraView;

	vec3 ShadowMapCoords[4];
	vec3 ViewPosition;

	vec4 ndcPosCur;
	vec4 ndcPosPrev;
};
 
layout(location = 0) in VertexOutput Input;

layout(location = 0) out vec4 color;
layout(location = 1) out vec4 o_ViewNormalsLuminance;
layout(location = 2) out vec4 o_MetalnessRoughness;

layout(location = 3) out vec2 o_Velocity;



// PBR texture inputs
layout(set = 0, binding = 5) uniform sampler2D u_AlbedoTexture;
layout(set = 0, binding = 6) uniform sampler2D u_NormalTexture;
layout(set = 0, binding = 7) uniform sampler2D u_MetallicRoughnessTexture;
layout(set = 0, binding = 8) uniform sampler2D u_EmissionTexture;


// Environment maps
layout(set = 1, binding = 9) uniform samplerCube u_EnvRadianceTex;
layout(set = 1, binding = 10) uniform samplerCube u_EnvIrradianceTex;

// BRDF LUT
layout(set = 1, binding = 11) uniform sampler2D u_BRDFLUTTexture;

// Shadow maps
layout(set = 1, binding = 12) uniform sampler2DArray u_ShadowMapTexture;
layout(set = 1, binding = 21) uniform sampler2DArray u_SpotShadowTexture;

layout(push_constant) uniform Material
{
	vec3 AlbedoColor;
	float Metalness;
	float Roughness;
	float Emission;

	float EnvMapRotation;
	
	bool UseNormalMap;
} u_MaterialUniforms;


vec3 IBL(vec3 F0, vec3 Lr)
{
	vec3 irradiance = texture(u_EnvIrradianceTex, m_Params.Normal).rgb;
	vec3 F = FresnelSchlickRoughness(F0, m_Params.NdotV, m_Params.Roughness);
	vec3 kd = (1.0 - F) * (1.0 - m_Params.Metalness);
	vec3 diffuseIBL = m_Params.Albedo * irradiance;

	int envRadianceTexLevels = textureQueryLevels(u_EnvRadianceTex);
	float NoV = clamp(m_Params.NdotV, 0.0, 1.0);
	vec3 R = 2.0 * dot(m_Params.View, m_Params.Normal) * m_Params.Normal - m_Params.View;
	vec3 specularIrradiance = textureLod(u_EnvRadianceTex, RotateVectorAboutY(u_MaterialUniforms.EnvMapRotation, Lr), (m_Params.Roughness) * envRadianceTexLevels).rgb;
	//specularIrradiance = vec3(Convert_sRGB_FromLinear(specularIrradiance.r), Convert_sRGB_FromLinear(specularIrradiance.g), Convert_sRGB_FromLinear(specularIrradiance.b));

	// Sample BRDF Lut, 1.0 - roughness for y-coord because texture was generated (in Sparky) for gloss model
	vec2 specularBRDF = texture(u_BRDFLUTTexture, vec2(m_Params.NdotV, 1.0 - m_Params.Roughness)).rg;
	vec3 specularIBL = specularIrradiance * (F0 * specularBRDF.x + specularBRDF.y);

	return kd * diffuseIBL + specularIBL;
}


/////////////////////////////////////////////

vec3 GetGradient(float value)
{
	vec3 zero = vec3(0.0, 0.0, 0.0);
	vec3 white = vec3(0.0, 0.1, 0.9);
	vec3 red = vec3(0.2, 0.9, 0.4);
	vec3 blue = vec3(0.8, 0.8, 0.3);
	vec3 green = vec3(0.9, 0.2, 0.3);

	float step0 = 0.0f;
	float step1 = 2.0f;
	float step2 = 4.0f;
	float step3 = 8.0f;
	float step4 = 16.0f;

	vec3 color = mix(zero, white, smoothstep(step0, step1, value));
	color = mix(color, white, smoothstep(step1, step2, value));
	color = mix(color, red, smoothstep(step1, step2, value));
	color = mix(color, blue, smoothstep(step2, step3, value));
	color = mix(color, green, smoothstep(step3, step4, value));

	return color;
}

void main()
{
	// Standard PBR inputs
	vec4 albedoTexColor = texture(u_AlbedoTexture, Input.TexCoord);
	m_Params.Albedo = albedoTexColor.rgb * u_MaterialUniforms.AlbedoColor;
	float alpha = albedoTexColor.a;
	m_Params.Metalness = texture(u_MetallicRoughnessTexture, Input.TexCoord).b * u_MaterialUniforms.Metalness;
	m_Params.Roughness = texture(u_MetallicRoughnessTexture, Input.TexCoord).g * u_MaterialUniforms.Roughness;
	vec3 m_Emission = texture(u_EmissionTexture, Input.TexCoord).rgb * u_MaterialUniforms.Emission;
	o_MetalnessRoughness = vec4(m_Params.Metalness, m_Params.Roughness, 0.f, 1.f);
	m_Params.Roughness = max(m_Params.Roughness, 0.05); // Minimum roughness of 0.05 to keep specular highlight


	// Normals (either from vertex or map)
	m_Params.Normal = normalize(Input.Normal);
	if (u_MaterialUniforms.UseNormalMap)
	{
		m_Params.Normal = normalize(texture(u_NormalTexture, Input.TexCoord).rgb * 2.0f - 1.0f);
		m_Params.Normal = normalize(Input.WorldNormals * m_Params.Normal);
	}
	// View normals
	o_ViewNormalsLuminance.xyz = Input.CameraView * normalize(Input.Normal);

	m_Params.View = normalize(u_Scene.CameraPosition - Input.WorldPosition);
	m_Params.NdotV = max(dot(m_Params.Normal, m_Params.View), 0.0);

	// Specular reflection vector
	vec3 Lr = 2.0 * m_Params.NdotV * m_Params.Normal - m_Params.View;

	// Fresnel reflectance, metals use albedo
	vec3 F0 = mix(Fdielectric, m_Params.Albedo, m_Params.Metalness);

	uint cascadeIndex = 0;

	const uint SHADOW_MAP_CASCADE_COUNT = 4;
	for (uint i = 0; i < SHADOW_MAP_CASCADE_COUNT - 1; i++)
	{
		if (Input.ViewPosition.z < u_RendererData.CascadeSplits[i])
			cascadeIndex = i + 1;
	}

	float shadowDistance = u_RendererData.MaxShadowDistance;//u_CascadeSplits[3];
	float transitionDistance = u_RendererData.ShadowFade;
	float distance = length(Input.ViewPosition);
	ShadowFade = distance - (shadowDistance - transitionDistance);
	ShadowFade /= transitionDistance;
	ShadowFade = clamp(1.0 - ShadowFade, 0.0, 1.0);

	float shadowScale;

	bool fadeCascades = u_RendererData.CascadeFading;
	if (fadeCascades)
	{
		float cascadeTransitionFade = u_RendererData.CascadeTransitionFade;

		float c0 = smoothstep(u_RendererData.CascadeSplits[0] + cascadeTransitionFade * 0.5f, u_RendererData.CascadeSplits[0] - cascadeTransitionFade * 0.5f, Input.ViewPosition.z);
		float c1 = smoothstep(u_RendererData.CascadeSplits[1] + cascadeTransitionFade * 0.5f, u_RendererData.CascadeSplits[1] - cascadeTransitionFade * 0.5f, Input.ViewPosition.z);
		float c2 = smoothstep(u_RendererData.CascadeSplits[2] + cascadeTransitionFade * 0.5f, u_RendererData.CascadeSplits[2] - cascadeTransitionFade * 0.5f, Input.ViewPosition.z);
		if (c0 > 0.0 && c0 < 1.0)
		{
			// Sample 0 & 1
			vec3 shadowMapCoords = GetShadowMapCoords(Input.ShadowMapCoords, 0);
			float shadowAmount0 = u_RendererData.SoftShadows ? PCSS_DirectionalLight(u_ShadowMapTexture, 0, shadowMapCoords, u_RendererData.LightSize) : HardShadows_DirectionalLight(u_ShadowMapTexture, 0, shadowMapCoords);
			shadowMapCoords = GetShadowMapCoords(Input.ShadowMapCoords, 1);
			float shadowAmount1 = u_RendererData.SoftShadows ? PCSS_DirectionalLight(u_ShadowMapTexture, 1, shadowMapCoords, u_RendererData.LightSize) : HardShadows_DirectionalLight(u_ShadowMapTexture, 1, shadowMapCoords);

			shadowScale = mix(shadowAmount0, shadowAmount1, c0);
		}
		else if (c1 > 0.0 && c1 < 1.0)
		{
			// Sample 1 & 2
			vec3 shadowMapCoords = GetShadowMapCoords(Input.ShadowMapCoords, 1);
			float shadowAmount1 = u_RendererData.SoftShadows ? PCSS_DirectionalLight(u_ShadowMapTexture, 1, shadowMapCoords, u_RendererData.LightSize) : HardShadows_DirectionalLight(u_ShadowMapTexture, 1, shadowMapCoords);
			shadowMapCoords = GetShadowMapCoords(Input.ShadowMapCoords, 2);
			float shadowAmount2 = u_RendererData.SoftShadows ? PCSS_DirectionalLight(u_ShadowMapTexture, 2, shadowMapCoords, u_RendererData.LightSize) : HardShadows_DirectionalLight(u_ShadowMapTexture, 2, shadowMapCoords);

			shadowScale = mix(shadowAmount1, shadowAmount2, c1);
		}
		else if (c2 > 0.0 && c2 < 1.0)
		{
			// Sample 2 & 3
			vec3 shadowMapCoords = GetShadowMapCoords(Input.ShadowMapCoords, 2);
			float shadowAmount2 = u_RendererData.SoftShadows ? PCSS_DirectionalLight(u_ShadowMapTexture, 2, shadowMapCoords, u_RendererData.LightSize) : HardShadows_DirectionalLight(u_ShadowMapTexture, 2, shadowMapCoords);
			shadowMapCoords = GetShadowMapCoords(Input.ShadowMapCoords, 3);
			float shadowAmount3 = u_RendererData.SoftShadows ? PCSS_DirectionalLight(u_ShadowMapTexture, 3, shadowMapCoords, u_RendererData.LightSize) : HardShadows_DirectionalLight(u_ShadowMapTexture, 3, shadowMapCoords);

			shadowScale = mix(shadowAmount2, shadowAmount3, c2);
		}
		else
		{
			vec3 shadowMapCoords = GetShadowMapCoords(Input.ShadowMapCoords, cascadeIndex);
			shadowScale = u_RendererData.SoftShadows ? PCSS_DirectionalLight(u_ShadowMapTexture, cascadeIndex, shadowMapCoords, u_RendererData.LightSize) : HardShadows_DirectionalLight(u_ShadowMapTexture, cascadeIndex, shadowMapCoords);
		}
	}
	else
	{
		vec3 shadowMapCoords = GetShadowMapCoords(Input.ShadowMapCoords, cascadeIndex);
		shadowScale = u_RendererData.SoftShadows ? PCSS_DirectionalLight(u_ShadowMapTexture, cascadeIndex, shadowMapCoords, u_RendererData.LightSize) : HardShadows_DirectionalLight(u_ShadowMapTexture, cascadeIndex, shadowMapCoords);
	}

	shadowScale = 1.0 - clamp(u_Scene.DirectionalLights.ShadowAmount - shadowScale, 0.0f, 1.0f);

	// Direct lighting
	vec3 lightContribution = CalculateDirLights(F0) * shadowScale;
	lightContribution += CalculatePointLights(F0, Input.WorldPosition);

	//SpotLights
	for (int i = 0; i < u_PointLights.LightCount; i++)
	{
		int lightIndex = GetSpotLightBufferIndex(i);
		if (lightIndex == -1)
			break;

		lightContribution += CalculateSpotLightByIndex(F0, Input.WorldPosition, lightIndex) * SpotShadowCalculationByIndex(u_SpotShadowTexture, Input.WorldPosition,lightIndex);
	}
	// lightContribution += CalculateSpotLightsWithShadow(F0, Input.WorldPosition) ;//* SpotShadowCalculation(u_SpotShadowTexture, Input.WorldPosition);
	lightContribution += m_Emission;

	// Indirect lighting
	vec3 iblContribution = IBL(F0, Lr) * u_Scene.EnvironmentMapIntensity;

	// Final color
	color = vec4(iblContribution + lightContribution, 1.0);

	// TODO: Temporary bug fix.
	if (u_Scene.DirectionalLights.Multiplier <= 0.0f)
		shadowScale = 0.0f;

	// Shadow mask with respect to bright surfaces.
	o_ViewNormalsLuminance.a = clamp(shadowScale + dot(color.rgb, vec3(0.2125f, 0.7154f, 0.0721f)), 0.0f, 1.0f);
	 
	if (u_RendererData.ShowLightComplexity)
	{
		int pointLightCount = GetPointLightCount();
		int spotLightCount = GetSpotLightCount();

		float value = float(pointLightCount + spotLightCount);
		color.rgb = (color.rgb * 0.2) + GetGradient(value);
	}
	// TODO(Karim): Have a separate render pass for translucent and transparent objects.
	// Because we use the pre-depth image for depth test.
	// color.a = alpha; 
	
	// (shading-only)
	// color.rgb = vec3(1.0) * shadowScale + 0.2f;

	if (u_RendererData.ShowCascades)
	{
		switch (cascadeIndex)
		{
		case 0:
			color.rgb *= vec3(1.0f, 0.25f, 0.25f);
			break;
		case 1:
			color.rgb *= vec3(0.25f, 1.0f, 0.25f);
			break;
		case 2:
			color.rgb *= vec3(0.25f, 0.25f, 1.0f);
			break;
		case 3:
			color.rgb *= vec3(1.0f, 1.0f, 0.25f);
			break;
		}
	}
	vec4 ndcPre = Input.ndcPosPrev;
	ndcPre = ndcPre/ndcPre.w;

	vec4 ndcCur = Input.ndcPosCur;
	ndcCur = ndcCur/ndcCur.w;

	vec2 screenPosCur = (ndcCur.xy * vec2(0.5f, 0.5f) ) +vec2(0.5);
	vec2 screenPosPrev = (ndcPre.xy* vec2(0.5f, 0.5f)) + vec2(0.5);

	o_Velocity = screenPosPrev - screenPosCur;
}


