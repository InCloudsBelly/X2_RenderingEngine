#version 450
#extension GL_GOOGLE_include_directive: enable

layout(set = 0, binding = 0) uniform sampler2D colorTexture;
layout(set = 1, binding = 0) uniform AttachmentSizeInfo
{
    vec2 size;
}attachmentSizeInfo;

layout(location = 0) out vec4 SwapchainAttachment;

void main() 
{
    vec2 aPosition = gl_FragCoord.xy / attachmentSizeInfo.size;
    SwapchainAttachment = vec4(texture(colorTexture, aPosition).rgb, 1);
}
