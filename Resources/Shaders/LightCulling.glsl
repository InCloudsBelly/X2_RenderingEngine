// ---------------------------------------
// -- Hazel Engine light culling shader --
// ---------------------------------------
// 
// References:
// - SIGGRAPH 2011 - Rendering in Battlefield 3
// - Implementation mostly adapted from https://github.com/bcrusco/Forward-Plus-Renderer
//
#version 450 core
#pragma stage : comp
#include <Buffers.glslh>
#include <intersect_lib.glslh>


layout(set = 0, binding = 15) uniform sampler2D u_DepthMap;

#define TILE_SIZE 16
#define MAX_LIGHT_COUNT 1024


vec3 ComputeViewspacePosition(const vec2 screenPos, const float viewspaceDepth)
{
    vec3 ret;
    ret.xy = fma(u_Camera.NDCToViewMul, screenPos.xy,  u_Camera.NDCToViewAdd) * abs(viewspaceDepth);
    ret.z = viewspaceDepth;
    return ret;
}


bool TestFrustumSides(vec3 c, float r, vec3 plane0, vec3 plane1, vec3 plane2, vec3 plane3)
{
	bool intersectingOrInside0 = dot(c, plane0) < r;
	bool intersectingOrInside1 = dot(c, plane1) < r;
	bool intersectingOrInside2 = dot(c, plane2) < r;
	bool intersectingOrInside3 = dot(c, plane3) < r;

	return (intersectingOrInside0 && intersectingOrInside1 &&
		intersectingOrInside2 && intersectingOrInside3);
}


float ScreenSpaceToViewSpaceDepth(const float screenDepth)
{
	return -u_Camera.ProjectionMatrix[3][2] / (screenDepth+ u_Camera.ProjectionMatrix[2][2] );
}

// Shared values between all the threads in the group
shared uint minDepthInt;
shared uint maxDepthInt;
shared uint visiblePointLightCount;
shared uint visibleSpotLightCount;
shared vec4 frustumPlanes[6];

shared IsectFrustum tileFrustum;

// Shared local storage for visible indices, will be written out to the global buffer at the end
shared int visiblePointLightIndices[MAX_LIGHT_COUNT];
shared int visibleSpotLightIndices[MAX_LIGHT_COUNT];

layout(local_size_x = TILE_SIZE, local_size_y = TILE_SIZE, local_size_z = 1) in;
void main()
{
    ivec2 location = ivec2(gl_GlobalInvocationID.xy);
    ivec2 itemID = ivec2(gl_LocalInvocationID.xy);
    ivec2 tileID = ivec2(gl_WorkGroupID.xy);
    ivec2 tileNumber = ivec2(gl_NumWorkGroups.xy);
    uint index = tileID.y * tileNumber.x + tileID.x;

    // Initialize shared global values for depth and light count
    if (gl_LocalInvocationIndex == 0)
    {
		minDepthInt = 0xFFFFFFFF;
		maxDepthInt = 0;
		visiblePointLightCount = 0;
		visibleSpotLightCount = 0;
    }

    barrier();

    // Step 1: Calculate the minimum and maximum depth values (from the depth buffer) for this group's tile
    vec2 tc = vec2(location) / u_ScreenData.FullResolution;
    float linearDepth = ScreenSpaceToViewSpaceDepth(textureLod(u_DepthMap, tc, 0).r);

    // Convert depth(negative) to uint so we can do atomic min and max comparisons between the threads
    uint depthInt = floatBitsToUint( -linearDepth);
    atomicMin(minDepthInt, depthInt);
    atomicMax(maxDepthInt, depthInt);

    barrier();

	//imageStore(o_Debug, location, vec4( uintBitsToFloat(maxDepthInt),uintBitsToFloat(minDepthInt), 0, 0 ));


    // Step 2: One thread should calculate the frustum planes to be used for this tile
    if (gl_LocalInvocationIndex == 0)
    {
		// Convert the min and max across the entire tile back to float
		float minDepth = uintBitsToFloat(minDepthInt);
		float maxDepth = uintBitsToFloat(maxDepthInt);

		// Steps based on tile sale
		vec2 negativeStep = (2.0 * vec2(tileID)) / vec2(tileNumber);
		vec2 positiveStep = (2.0 * vec2(tileID + ivec2(1, 1))) / vec2(tileNumber);


		vec3 cornersV[8];

		vec2 tileBias = vec2(tileID) / vec2(tileNumber);
		vec2 deltaBias = vec2(1.0) / vec2(tileNumber);  //inverse sampler Dir


		cornersV[0] = ComputeViewspacePosition(tileBias + vec2(0, 0), 						-minDepth);
		cornersV[1] = ComputeViewspacePosition(tileBias + vec2(0,deltaBias.y), 				-minDepth);
		cornersV[2] = ComputeViewspacePosition(tileBias + vec2(0, deltaBias.y), 			-maxDepth);
		cornersV[3] = ComputeViewspacePosition(tileBias + vec2(0, 0), 						-maxDepth);
		cornersV[4] = ComputeViewspacePosition(tileBias + vec2(deltaBias.x, 0), 			-minDepth);
		cornersV[5] = ComputeViewspacePosition(tileBias + vec2(deltaBias.x, deltaBias.y),  	-minDepth);
		cornersV[6] = ComputeViewspacePosition(tileBias + vec2(deltaBias.x, deltaBias.y), 	-maxDepth);
		cornersV[7] = ComputeViewspacePosition(tileBias + vec2(deltaBias.x, 0), 			-maxDepth);

		tileFrustum = isect_data_setup(shape_frustum(cornersV));
		
		
		frustumPlanes[4] = vec4(0.0, 0.0, -1.0, -minDepth); // Near
		frustumPlanes[5] = vec4(0.0, 0.0, 1.0, maxDepth ); // Far

		// Transform the first four planes
		for (uint i = 0; i < 4; i++)
		{
		    //frustumPlanes[i] *= u_Camera.InverseProjectionMatrix;
		    frustumPlanes[i] /= length(frustumPlanes[i].xyz);
		}


    }

	

    barrier();
	
    // Step 3: Cull lights.
    // Parallelize the threads against the lights now.
    // Can handle 256 simultaniously. Anymore lights than that and additional passes are performed
    const uint threadCount = TILE_SIZE * TILE_SIZE;
    uint passCount = (u_PointLights.LightCount + threadCount - 1) / threadCount;
    for (uint i = 0; i < passCount; i++)
    {
		// Get the lightIndex to test for this thread / pass. If the index is >= light count, then this thread can stop testing lights
		uint lightIndex = i * threadCount + gl_LocalInvocationIndex;
		if (lightIndex >= u_PointLights.LightCount)
		    break;
		

		vec4 position = vec4((u_Camera.ViewMatrix * vec4(u_PointLights.Lights[lightIndex].Position, 1.0)).xyz, 1.0f);
		float radius = u_PointLights.Lights[lightIndex].Radius;
		radius *= (1.0f);

		Sphere sV;
		sV.center = (u_Camera.ViewMatrix * vec4(u_PointLights.Lights[lightIndex].Position, 1.0)).xyz; 
		sV.radius = radius;

		if( intersect(tileFrustum , sV))
		{
			uint offset = atomicAdd(visiblePointLightCount, 1);
		    visiblePointLightIndices[offset] = int(lightIndex);
		}
    }

	passCount = (u_SpotLights.LightCount + threadCount - 1) / threadCount;
	for (uint i = 0; i < passCount; i++)
	{
		// Get the lightIndex to test for this thread / pass. If the index is >= light count, then this thread can stop testing lights
		uint lightIndex = i * threadCount + gl_LocalInvocationIndex;
		if (lightIndex >= u_SpotLights.LightCount)
			break;

		SpotLight light = u_SpotLights.Lights[lightIndex];


		Pyramid pyramid;
		pyramid.corners[0] = (u_Camera.ViewMatrix * vec4(light.Position,1.0f)).xyz;
		pyramid.corners[1] = (u_Camera.ViewMatrix * light.CornerPositions[0]).xyz;
		pyramid.corners[2] = (u_Camera.ViewMatrix * light.CornerPositions[1]).xyz;
		pyramid.corners[3] = (u_Camera.ViewMatrix * light.CornerPositions[2]).xyz;
		pyramid.corners[4] = (u_Camera.ViewMatrix * light.CornerPositions[3]).xyz;
		

		if(intersect( tileFrustum , pyramid))
		{
			uint offset = atomicAdd(visibleSpotLightCount, 1);
			visibleSpotLightIndices[offset] = int(lightIndex);
		}

		IsectPyramid i_pyramid = isect_data_setup(pyramid);
	}

    barrier();

	// imageStore(o_Debug, location, vec4(vec3(float(visiblePointLightCount) / 8), 1.0f));

    // One thread should fill the global light buffer
    if (gl_LocalInvocationIndex == 0)
    {
		const uint offset = index * MAX_LIGHT_COUNT; // Determine position in global buffer
		for (uint i = 0; i < visiblePointLightCount; i++) 
		{
			s_VisiblePointLightIndicesBuffer.Indices[offset + i] = visiblePointLightIndices[i];
		}

		for (uint i = 0; i < visibleSpotLightCount; i++) {
			s_VisibleSpotLightIndicesBuffer.Indices[offset + i] = visibleSpotLightIndices[i];
		}

		if (visiblePointLightCount != MAX_LIGHT_COUNT)
		{
		    // Unless we have totally filled the entire array, mark it's end with -1
		    // Final shader step will use this to determine where to stop (without having to pass the light count)
			s_VisiblePointLightIndicesBuffer.Indices[offset + visiblePointLightCount] = -1;
		}

		if (visibleSpotLightCount != MAX_LIGHT_COUNT)
		{
			// Unless we have totally filled the entire array, mark it's end with -1
			// Final shader step will use this to determine where to stop (without having to pass the light count)
			s_VisibleSpotLightIndicesBuffer.Indices[offset + visibleSpotLightCount] = -1;
		}
    }
}