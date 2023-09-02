#version 430
#pragma stage : comp
#include <Buffers.glslh>

#define LOCAL_SIZE_X 64
#define LOCAL_SIZE_Y 2
#define LOCAL_SIZE_Z 1

#define VOLUME_DEPTH 128


#define M_PI 3.14159265359
#define EPSILON 0.0001f

layout(set = 0, binding = 6) uniform sampler3D i_VoxelGrid;
// OutPut 
layout(binding = 7, rgba16f) uniform writeonly image3D o_VoxelGrid;


float slice_distance(int z)
{
    float n = u_RayMarching.bias_near_far_pow.y;
    float f = u_RayMarching.bias_near_far_pow.z;

    return n * pow(f / n, (float(z) + 0.5f) / float(VOLUME_DEPTH));
}

// ------------------------------------------------------------------

float slice_thickness(int z)
{
    return slice_distance(z + 1) - slice_distance(z);
}

vec4 ScatterStep(int z, vec3 accumulatedLight, float accumulatedTransmittance, vec3 sliceLight, float sliceDensity)
{
	float thickness = slice_thickness(z);
	float  sliceTransmittance = exp(-sliceDensity * thickness * 0.01f);

	// Seb Hillaire's improved transmission by calculating an integral over slice depth instead of
	// constant per slice value. Light still constant per slice, but that's acceptable. See slide 28 of
	// Physically-based & Unified Volumetric Rendering in Frostbite
	// http://www.frostbite.com/2015/08/physically-based-unified-volumetric-rendering-in-frostbite/
	vec3 sliceLightIntegral = sliceLight * (1.0 - sliceTransmittance) / sliceDensity;

	accumulatedLight += sliceLightIntegral * accumulatedTransmittance;
	accumulatedTransmittance *= sliceTransmittance;
	
	return vec4(accumulatedLight, accumulatedTransmittance);
}



layout(local_size_x = LOCAL_SIZE_X, local_size_y = LOCAL_SIZE_Y, local_size_z = LOCAL_SIZE_Z) in;

void main()
{
	vec4 accum = vec4(0, 0, 0, 1);
	ivec3 pos = ivec3(ivec2(gl_WorkGroupID.xy) * ivec2(LOCAL_SIZE_X, LOCAL_SIZE_Y) + ivec2(gl_LocalInvocationID.xy), 0);

	uint steps = VOLUME_DEPTH;

	for(int z = 0; z < steps; z++)
	{
		pos.z = z;
		vec4 slice = texelFetch(i_VoxelGrid, pos, 0);

		accum = ScatterStep(pos.z, accum.rgb, accum.a, slice.rgb, slice.a);

		imageStore(o_VoxelGrid, pos, accum);
	}
}


