#version 450 core
#pragma stage : vert

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec2 a_TexCoord;

struct OutputBlock
{
	vec2 TexCoord;
};

layout (location = 0) out OutputBlock Output;

void main()
{
	vec4 position = vec4(a_Position.xy, 0.0, 1.0);
	Output.TexCoord = a_TexCoord;
	gl_Position = position;
}

#version 450 core
#pragma stage : frag

#include <Buffers.glslh>

layout(location = 0) out vec4 o_Color;

struct OutputBlock
{
	vec2 TexCoord;
};

layout (location = 0) in OutputBlock Input;

layout (binding = 5) uniform sampler2D u_Texture;
layout (binding = 6) uniform sampler2D u_BloomTexture;
layout (binding = 7) uniform sampler2D u_BloomDirtTexture;
layout (binding = 8) uniform sampler2D u_DepthTexture;
layout (binding = 9) uniform sampler2D u_TransparentDepthTexture;

layout(push_constant) uniform Uniforms
{
	float Exposure;
	float BloomIntensity;
	float BloomDirtIntensity;
	float Opacity;
	float Time;
} u_Uniforms;

vec3 UpsampleTent9(sampler2D tex, float lod, vec2 uv, vec2 texelSize, float radius)
{
	vec4 offset = texelSize.xyxy * vec4(1.0f, 1.0f, -1.0f, 0.0f) * radius;

	// Center
	vec3 result = textureLod(tex, uv, lod).rgb * 4.0f;

	result += textureLod(tex, uv - offset.xy, lod).rgb;
	result += textureLod(tex, uv - offset.wy, lod).rgb * 2.0;
	result += textureLod(tex, uv - offset.zy, lod).rgb;

	result += textureLod(tex, uv + offset.zw, lod).rgb * 2.0;
	result += textureLod(tex, uv + offset.xw, lod).rgb * 2.0;

	result += textureLod(tex, uv + offset.zy, lod).rgb;
	result += textureLod(tex, uv + offset.wy, lod).rgb * 2.0;
	result += textureLod(tex, uv + offset.xy, lod).rgb;

	return result * (1.0f / 16.0f);
}

// Based on http://www.oscars.org/science-technology/sci-tech-projects/aces
vec3 ACESTonemap(vec3 color)
{
	mat3 m1 = mat3(
		0.59719, 0.07600, 0.02840,
		0.35458, 0.90834, 0.13383,
		0.04823, 0.01566, 0.83777
	);
	mat3 m2 = mat3(
		1.60475, -0.10208, -0.00327,
		-0.53108, 1.10813, -0.07276,
		-0.07367, -0.00605, 1.07602
	);
	vec3 v = m1 * color;
	vec3 a = v * (v + 0.0245786) - 0.000090537;
	vec3 b = v * (0.983729 * v + 0.4329510) + 0.238081;
	return clamp(m2 * (a / b), 0.0, 1.0);
}

vec3 GammaCorrect(vec3 color, float gamma)
{
	return pow(color, vec3(1.0f / gamma));
}

void main()
{
	const float gamma     = 2.2;
	const float pureWhite = 1.0;
	float sampleScale = 0.5;

	vec3 color = texture(u_Texture, Input.TexCoord).rgb;

	ivec2 texSize = textureSize(u_BloomTexture, 0);
	vec2 fTexSize = vec2(float(texSize.x), float(texSize.y));
	vec3 bloom = UpsampleTent9(u_BloomTexture, 0, Input.TexCoord, 1.0f / fTexSize, sampleScale) * u_Uniforms.BloomIntensity;
	vec3 bloomDirt = texture(u_BloomDirtTexture, Input.TexCoord).rgb * u_Uniforms.BloomDirtIntensity;

	float d = 0.0;

	color += bloom;
	color += bloom * bloomDirt;
	color *= u_Uniforms.Exposure;

	// Grain
	float strength = 5.0;
    
    float x = (Input.TexCoord.x + 1.0 ) * (Input.TexCoord.y + 1.0 ) * u_Uniforms.Time;
	float grain = mod((mod(x, 13.0) + 1.0) * (mod(x, 123.0) + 1.0), 0.01) - 0.006;
    
    //color += grain * strength;
	color = ACESTonemap(color);

	color = GammaCorrect(color.rgb, gamma);

	color *= u_Uniforms.Opacity;

	o_Color = vec4(color, 1.0);
}
