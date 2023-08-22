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
layout(location = 1) out vec4 offset[3];


void main()
{
    texcoord = a_TexCoord;
    gl_Position = vec4(a_Position.xy, 0.0, 1.0);

	SMAAEdgeDetectionVS(texcoord, offset);
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

#ifndef __X2_EDGEMETHOD
#define __X2_EDGEMETHOD 1
#endif

#ifndef __X2_SMAAQUALITY
#define __X2_SMAAQUALITY 0
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


#if __X2_SMAAQUALITY == 0
	#define SMAA_PRESET_ULTRA
#elif __X2_SMAAQUALITY == 1
	#define SMAA_PRESET_HIGH
#elif __X2_SMAAQUALITY == 2
	#define SMAA_PRESET_MEDIUM
#elif __X2_SMAAQUALITY == 3
	#define SMAA_PRESET_LOW
#else
	#error Bad SMAAQUALITY
#endif


#include <smaa_lib.glsl>


#if __X2_EDGEMETHOD == 2

layout( binding = 1) uniform SMAATexture2D(depthTex);

#else  // __X2_EDGEMETHOD

layout( binding = 1) uniform sampler2D colorTex;

#endif  // __X2_EDGEMETHOD

#if SMAA_PREDICATION

layout( binding = 2) uniform SMAATexture2D(predicationTex);

#endif  // SMAA_PREDICATION



layout(location = 0) out vec4 outEdge;

layout (location = 0) in vec2 texcoord;
layout (location = 1) in vec4 offsets[3];


void main()
{
	#if __X2_EDGEMETHOD == 0

		#if SMAA_PREDICATION
			outEdge = vec4(SMAAColorEdgeDetectionPS(texcoord, offsets, colorTex, predicationTex), 0.0, 0.0);
		#else  // SMAA_PREDICATION
			outEdge = vec4(SMAAColorEdgeDetectionPS(texcoord, offsets, colorTex), 0.0f, 1.0f);
			// outColor = vec4(1.0f,1.0f,1.0f,1.0f);
		#endif  // SMAA_PREDICATION
	#elif __X2_EDGEMETHOD == 1
		#if SMAA_PREDICATION
			outEdge = vec4(SMAALumaEdgeDetectionPS(texcoord, offsets, colorTex, predicationTex), 0.0, 0.0);
		#else  // SMAA_PREDICATION
			outEdge = vec4(SMAALumaEdgeDetectionPS(texcoord, offsets, colorTex), 0.0, 1.0f);
		#endif  // SMAA_PREDICATION

	#elif __X2_EDGEMETHOD == 2
		outEdge = vec4(SMAADepthEdgeDetectionPS(texcoord, offsets, depthTex), 0.0, 0.0);
	#else

	#error Bad EDGEMETHOD
	#endif

	
}
