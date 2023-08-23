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



layout (location = 0) in vec2 texcoord;

layout(location = 0) out vec4 outColor;

// PBR texture inputs
layout(set = 0, binding = 1) uniform sampler2D u_color;



vec3 YCoCgR2RGB(vec3 YCoCgRColor)
{
    float Y = YCoCgRColor.x * 0.25;
    float Co = YCoCgRColor.y * 0.25;
    float Cg = YCoCgRColor.z * 0.25;

    float R = Y + Co - Cg;
    float G = Y + Cg;
    float B = Y - Co - Cg;

    vec3 RGB = vec3(R, G, B);
    return RGB;
}

float Luminance(vec3 color)
{
    return 0.25 * color.r + 0.5 * color.g + 0.25 * color.b;
}

vec3 UnToneMap(vec3 color)
{
    return color / (1 - Luminance(color));
}


void main()
{
	vec4 rawColor = texture(u_color, texcoord);
    outColor.rgb = UnToneMap(YCoCgR2RGB(rawColor.rgb));
    outColor.a = rawColor.a;
}
