#version 450
#extension GL_GOOGLE_include_directive: enable

layout(set = 0, binding = 0) uniform sampler2D depthTexture;
layout(set = 1, binding = 0) uniform CascadeEvsmBlitInfo
{
    vec2 texelSize;
    float c1;
    float c2;
}cascadeEvsmBlitInfo;

layout(location = 0) out vec4 ColorAttachment;

void main() 
{
    vec2 aPosition = gl_FragCoord.xy * cascadeEvsmBlitInfo.texelSize;
    float depth = texture(depthTexture, aPosition).r;
    float e1 = exp(cascadeEvsmBlitInfo.c1 * depth);
    float e2 = -exp(-cascadeEvsmBlitInfo.c2 * depth);

    ColorAttachment = vec4(e1, e1 * e1, e2, e2 * e2);
}
