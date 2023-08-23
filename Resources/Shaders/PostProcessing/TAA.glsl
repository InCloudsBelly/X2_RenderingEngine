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
layout(set = 0, binding = 2) uniform sampler2D u_velocity;
layout(set = 0, binding = 3) uniform sampler2D u_depth;
layout(set = 0, binding = 4) uniform sampler2D u_colorHistory;

bool clipped = false;

vec3 RGB2YCoCgR(vec3 rgbColor)
{
    float Y = dot(rgbColor, vec3(1, 2, 1));
    float Co = dot(rgbColor, vec3(2, 0, -2));
    float Cg = dot(rgbColor, vec3(-1, 2, -1));

    vec3 YCoCg = vec3(Y, Co, Cg);
    return YCoCg;

    // vec3 YCoCgRColor;

    // YCoCgRColor.y = rgbColor.r - rgbColor.b;
    // float temp = rgbColor.b + YCoCgRColor.y / 2;
    // YCoCgRColor.z = rgbColor.g - temp;
    // YCoCgRColor.x = temp + YCoCgRColor.z / 2;

    // return YCoCgRColor;
}

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

    // vec3 rgbColor;

    // float temp = YCoCgRColor.x - YCoCgRColor.z / 2;
    // rgbColor.g = YCoCgRColor.z + temp;
    // rgbColor.b = temp - YCoCgRColor.y / 2;
    // rgbColor.r = rgbColor.b + YCoCgRColor.y;

    // return rgbColor;
}

float Luminance(vec3 color)
{
    return 0.25 * color.r + 0.5 * color.g + 0.25 * color.b;
}

vec3 ToneMap(vec3 color)
{
    return color / (1 + Luminance(color));
}

vec3 UnToneMap(vec3 color)
{
    return color / (1 - Luminance(color));
}

vec2 getClosestOffset(vec2 _coord)
{
    vec2 deltaRes = u_ScreenData.InvFullResolution;
    float closestDepth = 1.0f;
    vec2 closestUV = _coord;

    for(int i=-1;i<=1;++i)
    {
        for(int j=-1;j<=1;++j)
        {
            vec2 newUV = _coord + deltaRes * vec2(i, j);

            float depth = texture(u_depth, newUV).r;

            if(depth < closestDepth)
            {
                closestDepth = depth;
                closestUV = newUV;
            }
        }
    }

    return closestUV;
}

vec3 clipAABB(vec3 nowColor, vec3 preColor)
{
    vec3 aabbMin = nowColor, aabbMax = nowColor;
    vec2 deltaRes = u_ScreenData.InvFullResolution;
    vec3 m1 = vec3(0), m2 = vec3(0);

    for(int i=-1;i<=1;++i)
    {
        for(int j=-1;j<=1;++j)
        {
            vec2 newUV = texcoord + deltaRes * vec2(i, j);
            vec3 C = texture(u_color, newUV).rgb;
            m1 += C;
            m2 += C * C;
        }
    }

    // Variance clip
    const int N = 9;
    const float VarianceClipGamma = 1.0f;
    vec3 mu = m1 / N;
    vec3 sigma = sqrt(abs(m2 / N - mu * mu));
    aabbMin = mu - VarianceClipGamma * sigma;
    aabbMax = mu + VarianceClipGamma * sigma;

    // clip to center
    vec3 p_clip = 0.5 * (aabbMax + aabbMin);
    vec3 e_clip = 0.5 * (aabbMax - aabbMin);

    vec3 v_clip = preColor - p_clip;
    vec3 v_unit = v_clip.xyz / e_clip;
    vec3 a_unit = abs(v_unit);
    float ma_unit = max(a_unit.x, max(a_unit.y, a_unit.z));

    if (ma_unit > 1.0)
	{
		clipped = true;
        return p_clip + v_clip / ma_unit;
	}
	else
        return preColor;
}




void main()
{
	vec4 colorCurrent = texture(u_color, texcoord -  u_TAA.jitter );
	float depthCurrent = texture(u_depth, texcoord - u_TAA.jitter ).r;

#ifdef STATIC_REPROJECTION
	//Convert current screenspace to world space
	float z = depthCurrent * 2.0 - 1.0;
	vec4 CVVPosCURRENT = vec4((texcoord) * 2.f - 1.f, z, 1.f);
	vec4 worldSpacePosition = u_Camera.InverseViewProjectionMatrix  * CVVPosCURRENT;
	worldSpacePosition /= worldSpacePosition.w;

	//Convert this into previous UV coords.
	vec4 CVVPosHISTORY = u_TAA.ViewProjectionMatrixHistory * worldSpacePosition;
	vec2 uvHISTORY = 0.5 * (CVVPosHISTORY.xy / CVVPosHISTORY.w) + 0.5;
	//Initialise the velocity to account for the jitter
	vec2 vel = uvHISTORY - texcoord;
#endif  

    vec2 vel = texture(u_velocity, getClosestOffset(texcoord - u_TAA.jitter)).rg;
    vec2 offsetUV = texcoord + vel;
    if(offsetUV.x >0 && offsetUV.x < 1 && offsetUV.y > 0 && offsetUV.y < 1)
    {
        vec4 colorHistory = texture(u_colorHistory, offsetUV);
	
        vec3 colorCurrentCoCgR = colorCurrent.rgb;
        vec3 colorHistoryCoCgR = colorHistory.rgb;

        vec3 colorHistoryClipped = clipAABB(colorCurrentCoCgR, colorHistoryCoCgR);

        if(colorHistory.a < 0.01){outColor.a = float(clipped);} //If there's nothing, store the clipped flag (could still be nothing)
        else {outColor.a = mix(colorHistory.a, float(clipped), u_TAA.feedback);}


        float clipBlendFactor = outColor.a;
        
        //Lerp based on recent clipping events
        vec3 colorHistoryClippedBlended = mix(colorHistory.rgb, colorHistoryClipped, clamp(clipBlendFactor, 0.f, 1.f));

        outColor.rgb = mix(colorHistoryClipped , colorCurrent.rgb, u_TAA.feedback);
    }
    else
    {
        outColor.rgb = colorCurrent.rgb;
    }

    // outColor.rgb = colorCurrent.rgb;
}
