#version 450 core
#pragma stage:vert

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec2 a_TexCoord;


layout(location = 0) out vec2 texcoord;


void main()
{
    texcoord = a_TexCoord;
    gl_Position = vec4(a_Position.xy, 0.0, 1.0);

}



#version 450 core
#pragma stage:frag
#include <Buffers.glslh>
#include <Common.glslh>



layout (location = 0) in vec2 texcoord;

layout(location = 0) out vec4 outColor;

// PBR texture inputs
layout(set = 0, binding = 1) uniform sampler2D u_color;
layout(set = 0, binding = 2) uniform sampler2D u_depth;
layout(set = 0, binding = 3) uniform sampler3D u_FrovelGrid;



vec3 add_inscattered_light(vec3 color, vec3 uv)
{
    // vec4  scattered_light = u_Tricubic ? textureTricubic(s_VoxelGrid, uv) : textureLod(s_VoxelGrid, uv, 0.0f);
	vec4  scattered_light = textureTricubic(u_FrovelGrid, uv) ;
    float transmittance   = scattered_light.a;

    return color * transmittance + scattered_light.rgb;
}

//negtive
float LinearizeDepth(const float screenDepth)
{
	return -u_Camera.ProjectionMatrix[3][2] / (screenDepth+ u_Camera.ProjectionMatrix[2][2] );
}

void main()
{
	vec4 rawColor = texture(u_color, texcoord);
    float screenDepth = texture(u_depth,texcoord).r;

    vec3 uv;
    uv.xy = texcoord.xy;

    float n = u_FroxelFog.bias_near_far_pow.y;
	float f = u_FroxelFog.bias_near_far_pow.z;

	vec2 params = vec2(1.0 / log2(f/n), -(log2(n))/log2(f/n));

    float linearDepth = -LinearizeDepth(screenDepth);
    uv.z = max(log2(linearDepth)* params.x + params.y, 0.0f);



    outColor.rgb = add_inscattered_light(rawColor.xyz, uv);
    outColor.a = rawColor.a;
}
