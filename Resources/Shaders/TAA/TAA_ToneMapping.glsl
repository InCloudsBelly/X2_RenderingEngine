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

layout(set = 0, binding = 1) uniform sampler2D u_color;


bool clipped = false;

vec3 RGB2YCoCgR(vec3 rgbColor)
{
    float Y = dot(rgbColor, vec3(1, 2, 1));
    float Co = dot(rgbColor, vec3(2, 0, -2));
    float Cg = dot(rgbColor, vec3(-1, 2, -1));

    vec3 YCoCg = vec3(Y, Co, Cg);
    return YCoCg;
}

float Luminance(vec3 color)
{
    return 0.25 * color.r + 0.5 * color.g + 0.25 * color.b;
}

vec3 ToneMap(vec3 color)
{
    return color / (1 + Luminance(color));
}


void main()
{
    vec4 rawColor = texture(u_color, texcoord);
    outColor.rgb = RGB2YCoCgR(ToneMap(rawColor.rgb));
    outColor.a = rawColor.a;
}
