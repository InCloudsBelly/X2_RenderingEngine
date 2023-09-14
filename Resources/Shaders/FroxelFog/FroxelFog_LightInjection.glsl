#version 430
#pragma stage : comp
#include <Buffers.glslh>
#include <ShadowMapping_withBias.glslh>

#define VOXEL_GRID_SIZE_X 160 
#define VOXEL_GRID_SIZE_Y 90
#define VOXEL_GRID_SIZE_Z 128

#define LOCAL_SIZE_X 16
#define LOCAL_SIZE_Y 2
#define LOCAL_SIZE_Z 16

#define BLUE_NOISE_TEXTURE_SIZE 256


#define M_PI 3.14159265359
#define EPSILON 0.0001f

layout(set = 0, binding = 5) uniform sampler2DArray u_ShadowMapTexture;
layout(set = 0, binding = 6) uniform sampler2D u_BlueNoise;


layout(set = 0, binding = 7) uniform sampler3D u_History;
// OutPut 
layout(binding = 8, rgba16f) uniform writeonly image3D o_VoxelGrid;


vec3 id_to_uv_with_jitter(ivec3 id, float n, float f, float jitter)
{
    // Exponential View-Z
    float view_z = n * pow(f / n, (float(id.z) + 0.5f + jitter) / float(VOXEL_GRID_SIZE_Z));

    return vec3((float(id.x) + 0.5f) / float(VOXEL_GRID_SIZE_X),
                (float(id.y) + 0.5f) / float(VOXEL_GRID_SIZE_Y),
                view_z - n);
}


// Henyey-Greenstein
float phase_function(vec3 Wo, vec3 Wi, float g)
{
    float cos_theta = dot(Wo, Wi);
    float denom     = 1.0f + g * g - 2.0f * g * cos_theta;
    return (1.0f / (4.0f * M_PI)) * (1.0f - g * g) / max(pow(denom, 1.5f), EPSILON);
}

vec3 FrustumRay(vec2 uv, vec4 frustumRays[4])
{
	vec3 ray0 = mix(frustumRays[1].xyz, frustumRays[0].xyz, uv.x);
	vec3 ray1 = mix(frustumRays[3].xyz, frustumRays[2].xyz, uv.x);
	return mix(ray1, ray0, uv.y);
}


float cal_density(vec3 worldPos)
{
	float density = u_FroxelFog.fogParams.x;  // constant density

	density += max(exp(u_FroxelFog.fogParams.y*(-worldPos.y + u_FroxelFog.fogParams.z)) * u_FroxelFog.fogParams.w, 0.0);  //height fog density

	for(int i = 0; i <u_FogVoulumes.FogVolumeCount; i++)
	{
		vec4 posLocal = u_FogVoulumes.FogVolumes[i].worldToLocal * vec4(worldPos, 1.0f);
		if(all(bvec3(abs(posLocal.x)< 0.5,abs(posLocal.y)< 0.5,abs(posLocal.z)< 0.5)))
			density += u_FogVoulumes.FogVolumes[i].density;
	}
	return density;
}


layout(local_size_x = LOCAL_SIZE_X, local_size_y = LOCAL_SIZE_Y, local_size_z = LOCAL_SIZE_Z) in;
void main()
{
    ivec3 outputCoord = ivec3(gl_WorkGroupID.xyz) * ivec3(LOCAL_SIZE_X, LOCAL_SIZE_Y, LOCAL_SIZE_Z) + ivec3(gl_LocalInvocationID.xyz);

    ivec2 noise_coord = (outputCoord.xy + ivec2(0, 1) * outputCoord.z * BLUE_NOISE_TEXTURE_SIZE) % BLUE_NOISE_TEXTURE_SIZE;
    
	float jitter  = 0.0f;
	if(u_FroxelFog.enableJitter)
	{
		jitter = texelFetch(u_BlueNoise, noise_coord, 0).r;
    	jitter = (jitter - 0.5f) * 0.999f;
	}

	vec2 uv = vec2((float(outputCoord.x) + 0.5f)/ float(VOXEL_GRID_SIZE_X),  (float(outputCoord.y) + 0.5f)/ float(VOXEL_GRID_SIZE_Y)) + vec2(jitter, -jitter) * u_ScreenData.InvFullResolution;

	float f = u_FroxelFog.bias_near_far_pow.z;
	float n = u_FroxelFog.bias_near_far_pow.y;

	float viewZ = n * pow(f/n , (float(outputCoord.z) + 0.5f + jitter) / float(VOXEL_GRID_SIZE_Z));
	
	float k = viewZ/(f-n);
	vec3 rayDir = FrustumRay(uv, u_FroxelFog.frustumRays) ;
	vec3 worldPos = rayDir * k + u_Scene.CameraPosition;

    // Get the view direction from the current voxel.
    vec3 Wo = normalize(u_Scene.CameraPosition - worldPos.xyz);

    // Density and coefficient estimation.
    float density = u_FroxelFog.aniso_density_multipler_absorption.y;

    vec3 lighting = vec3(0.0);// u_Scene.DirectionalLights.Radiance * u_Scene.DirectionalLights.Multiplier * 0.1;


    // shadow 
    vec4 shadowCoords[4];
	shadowCoords[0] = u_DirShadow.DirLightMatrices[0] * vec4(worldPos.xyz, 1.0);
	shadowCoords[1] = u_DirShadow.DirLightMatrices[1] * vec4(worldPos.xyz, 1.0);
	shadowCoords[2] = u_DirShadow.DirLightMatrices[2] * vec4(worldPos.xyz, 1.0);
	shadowCoords[3] = u_DirShadow.DirLightMatrices[3] * vec4(worldPos.xyz, 1.0);

    vec3 shadowCoords_xyz[4];
    
    shadowCoords_xyz[0] = shadowCoords[0].xyz / shadowCoords[0].w;
    shadowCoords_xyz[1] = shadowCoords[1].xyz / shadowCoords[1].w;
    shadowCoords_xyz[2] = shadowCoords[2].xyz / shadowCoords[2].w;
    shadowCoords_xyz[3] = shadowCoords[3].xyz / shadowCoords[3].w;


    uint cascadeIndex = 0;
	const uint SHADOW_MAP_CASCADE_COUNT = 4;
	for (uint i = 0; i < SHADOW_MAP_CASCADE_COUNT - 1; i++)
	{
		if ( viewZ < u_RendererData.CascadeSplits[i])
			cascadeIndex = i + 1;
	}


    float shadowScale;
    bool fadeCascades = u_RendererData.CascadeFading;
	if (fadeCascades)
	{
		float cascadeTransitionFade = u_RendererData.CascadeTransitionFade;

		float c0 = smoothstep(u_RendererData.CascadeSplits[0] + cascadeTransitionFade * 0.5f, u_RendererData.CascadeSplits[0] - cascadeTransitionFade * 0.5f,  viewZ);
		float c1 = smoothstep(u_RendererData.CascadeSplits[1] + cascadeTransitionFade * 0.5f, u_RendererData.CascadeSplits[1] - cascadeTransitionFade * 0.5f,  viewZ);
		float c2 = smoothstep(u_RendererData.CascadeSplits[2] + cascadeTransitionFade * 0.5f, u_RendererData.CascadeSplits[2] - cascadeTransitionFade * 0.5f,  viewZ);
		if (c0 > 0.0 && c0 < 1.0)
		{
			// Sample 0 & 1
			vec3 shadowMapCoords = GetShadowMapCoords(shadowCoords_xyz, 0);
			float shadowAmount0 = HardShadows_DirectionalLight_withBias(u_ShadowMapTexture, 0, shadowMapCoords, 0.0);
			shadowMapCoords = GetShadowMapCoords(shadowCoords_xyz, 1);
			float shadowAmount1 = HardShadows_DirectionalLight_withBias(u_ShadowMapTexture, 1, shadowMapCoords, 0.0);

			shadowScale = mix(shadowAmount0, shadowAmount1, c0);
		}
		else if (c1 > 0.0 && c1 < 1.0)
		{
			// Sample 1 & 2
			vec3 shadowMapCoords = GetShadowMapCoords(shadowCoords_xyz, 1);
			float shadowAmount1 =  HardShadows_DirectionalLight_withBias(u_ShadowMapTexture, 1, shadowMapCoords, 0.0);
			shadowMapCoords = GetShadowMapCoords(shadowCoords_xyz, 2);
			float shadowAmount2 = HardShadows_DirectionalLight_withBias(u_ShadowMapTexture, 2, shadowMapCoords, 0.0);

			shadowScale = mix(shadowAmount1, shadowAmount2, c1);
		}
		else if (c2 > 0.0 && c2 < 1.0)
		{
			// Sample 2 & 3
			vec3 shadowMapCoords = GetShadowMapCoords(shadowCoords_xyz, 2);
			float shadowAmount2 = HardShadows_DirectionalLight_withBias(u_ShadowMapTexture, 2, shadowMapCoords, 0.0);
			shadowMapCoords = GetShadowMapCoords(shadowCoords_xyz, 3);
			float shadowAmount3 = HardShadows_DirectionalLight_withBias(u_ShadowMapTexture, 3, shadowMapCoords, 0.0);

			shadowScale = mix(shadowAmount2, shadowAmount3, c2);
		}
		else
		{
			vec3 shadowMapCoords = GetShadowMapCoords(shadowCoords_xyz, cascadeIndex);
			shadowScale = HardShadows_DirectionalLight_withBias(u_ShadowMapTexture, cascadeIndex, shadowMapCoords, 0.0);
		}
	}
	else
	{
		vec3 shadowMapCoords = GetShadowMapCoords(shadowCoords_xyz, cascadeIndex);
		shadowScale = HardShadows_DirectionalLight_withBias(u_ShadowMapTexture, cascadeIndex, shadowMapCoords, 0.0);
	}

	shadowScale = 1.0 - clamp(u_Scene.DirectionalLights.ShadowAmount - shadowScale, 0.0f, 1.0f);
    
    if (shadowScale > EPSILON)
    	lighting += shadowScale * u_Scene.DirectionalLights.Radiance * u_Scene.DirectionalLights.Multiplier * phase_function(Wo,  -normalize(u_Scene.DirectionalLights.Direction), u_FroxelFog.aniso_density_multipler_absorption.x);

	
	float den = cal_density(worldPos);


    vec4 color_and_density = vec4(lighting * den *  u_FroxelFog.aniso_density_multipler_absorption.z, den);


	if(u_FroxelFog.enableTemperalAccumulating)
	{
		float viewZ_without_Jitter = n * pow(f/n , (float(outputCoord.z) + 0.5f) / float(VOXEL_GRID_SIZE_Z));
		vec3 worldPos_without_Jitter = rayDir * (viewZ_without_Jitter/(f-n)) + u_Scene.CameraPosition;

		vec4 pNdc_History = u_TAA.ViewProjectionMatrixHistory * vec4(worldPos_without_Jitter, 1.0f);
		pNdc_History.xyz /= pNdc_History.w;

		vec3 uv_History;
		uv_History.xy = pNdc_History.xy * 0.5f + 0.5f ;

		//LinearizeDepth
		float linearDepth_History = u_Camera.ProjectionMatrix[3][2] / (pNdc_History.z + u_Camera.ProjectionMatrix[2][2] );

		vec2 params = vec2(1.0 / log2(f/n), -(log2(n))/log2(f/n));
		uv_History.z = max(log2(linearDepth_History)* params.x + params.y, 0.0f);

		if (all(greaterThan(uv_History, vec3(0.0f))) && all(lessThan(uv_History, vec3(1.0f))))
		{
			vec4  history  = textureLod(u_History, uv_History, 0.0f) ;
			color_and_density = mix(history, color_and_density, 0.05f);
		}
	}
     // Write out lighting.
    imageStore(o_VoxelGrid, outputCoord, vec4(color_and_density));

}

