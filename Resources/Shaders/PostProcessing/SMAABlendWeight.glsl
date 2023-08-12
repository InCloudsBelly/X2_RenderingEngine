#version 430 core
#pragma stage:vert

#include <Buffers.glslh>

#define SMAA_RT_METRICS vec4(u_ScreenData.InvFullResolution, u_ScreenData.FullResolution)

#define SMAA_GLSL_4 1

#define SMAA_INCLUDE_PS 0
#define SMAA_INCLUDE_VS 1

#ifdef VULKAN_FLIP
#define SMAA_FLIP_Y 0
#endif

#include <smaa_lib.glsl>


layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec2 a_TexCoord;


layout(location = 0) out vec2 texcoord;
layout(location = 1) out vec2 pixcoord;
layout(location = 2) out vec4 offsets[3];


void main()
{
    texcoord = a_TexCoord;
    gl_Position = vec4(a_Position.xy, 0.0, 1.0);
	
	SMAABlendingWeightCalculationVS(texcoord, pixcoord, offsets);
}



#version 450 core
#pragma stage:frag
#include <Buffers.glslh>

#define SMAA_RT_METRICS vec4(u_ScreenData.InvFullResolution, u_ScreenData.FullResolution)
#define SMAA_GLSL_4 1

#define SMAA_INCLUDE_PS 1
#define SMAA_INCLUDE_VS 0

#ifdef VULKAN_FLIP
#define SMAA_FLIP_Y 0
#endif


#ifdef SMAA_PRESET_CUSTOM

#define SMAA_THRESHOLD                 u_SMAA.smaaParameters.threshold
#define SMAA_DEPTH_THRESHOLD           u_SMAA.smaaParameters.depthThreshold
#define SMAA_MAX_SEARCH_STEPS          u_SMAA.smaaParameters.maxSearchSteps
#define SMAA_MAX_SEARCH_STEPS_DIAG     u_SMAA.smaaParameters.maxSearchStepsDiag
#define SMAA_CORNER_ROUNDING           u_SMAA.smaaParameters.cornerRounding

#define SMAA_PREDICATION_THRESHOLD  u_SMAA.predicationThreshold
#define SMAA_PREDICATION_SCALE      u_SMAA.predicationScale
#define SMAA_PREDICATION_STRENGTH   u_SMAA.predicationStrength

#endif  // SMAA_PRESET_CUSTOM

#define SMAA_PRESET_ULTRA


#include <smaa_lib.glsl>

layout(set = 0, binding = 1) uniform SMAATexture2D(edgesTex);
layout(set = 0, binding = 2) uniform SMAATexture2D(areaTex);
layout(set = 0, binding = 3) uniform SMAATexture2D(searchTex);


layout(location = 0) out vec4 outColor;

layout (location = 0) in vec2 texcoord;
layout (location = 1) in vec2 pixcoord;
layout (location = 2) in vec4 offsets[3];

void main()
{
	outColor = SMAABlendingWeightCalculationPS(texcoord, pixcoord, offsets, edgesTex, areaTex, searchTex, u_SMAA.subsampleIndices);
}
