#version 450
#extension GL_GOOGLE_include_directive: enable

#include "BackgroundRendering.glsl"

layout(set = START_SET_INDEX + 0, binding = 0) uniform samplerCube backgroundTexture;

layout(location = 0) out vec4 ColorAttachment;

void main() 
{
    vec2 iPosition = gl_FragCoord.xy * attachmentSizeInfo.texelSize;
    vec3 ndcPosition = IMAGE_TO_NDC_SPACE(iPosition, 1);
    vec3 wPosition = PositionN2W(ndcPosition, cameraInfo.info);
    vec3 observeDirection = VIEW_DIRECTION(cameraInfo.info, wPosition);

    ColorAttachment = vec4(texture(backgroundTexture, observeDirection).rgb, 1);
}
