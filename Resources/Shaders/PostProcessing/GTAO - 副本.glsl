#version 430
#pragma stage : comp
#include <Buffers.glslh>
#include <GTAO.slh>

layout(binding = 1) uniform sampler2D			u_ViewNormal;
layout(binding = 2)	uniform usampler2D			u_HilbertLut;		// hilbert lookup table
layout(binding = 3)	uniform sampler2D			u_HiZDepth;   

layout(binding = 4, r8ui)   restrict writeonly uniform uimage2D   o_AOwBentNormals; // output AO term (includes bent normals if enabled - packed as R11G11B10 scaled by AO)
layout(binding = 5, r8)     restrict uniform writeonly image2D o_Edges;
layout(binding = 6, rgba32f) restrict writeonly uniform image2D o_Debug;

#define GTAO_DEPTH_MIP_LEVELS                    5                   // this one is hard-coded to 5 for now
#define GTAO_NUMTHREADS_X                        16                   // these can be changed
#define GTAO_NUMTHREADS_Y                        16                   // these can be changed

layout(push_constant)  uniform PushConstants{
    
    vec2 NDCToViewMul_x_PixelSize;
    float EffectRadius; // world (viewspace) maximum size of the shadow
    float EffectFalloffRange;

    float RadiusMultiplier;
    float FinalValuePower;
    float DenoiseBlurBeta;
    bool HalfRes;

    float SampleDistributionPower;
    float ThinOccluderCompensation;
    float DepthMIPSamplingOffset;
    int NoiseIndex; // frameIndex % 64 if using TAA or 0 otherwise

    vec2 X2BUVFactor;
    float ShadowTolerance;
    float Padding;

} u_GTAOConsts;

#define PI               	(3.1415926535897932384626433832795)
#define PI_HALF             (1.5707963267948966192313216916398)

uint  GTAO_FLOAT4_to_R8G8B8A8_UNORM(vec4 unpackedInput) {
    return uint(clamp(unpackedInput.x, 0.0, 1.0) * 255.0 + 0.5) |
        (uint(clamp(unpackedInput.y, 0.0, 1.0) * 255.0 + 0.5) << 8) |
        (uint(clamp(unpackedInput.z, 0.0, 1.0) * 255.0 + 0.5) << 16) |
        (uint(clamp(unpackedInput.w, 0.0, 1.0) * 255.0 + 0.5) << 24);
}

vec3 GTAO_ComputeViewspacePosition(const vec2 screenPos, const float viewspaceDepth)
{
    vec3 ret;
    ret.xy = fma(u_Camera.NDCToViewMul, screenPos.xy, u_Camera.NDCToViewAdd) * abs(viewspaceDepth);
    ret.z = viewspaceDepth;
    return ret;
}

vec4 GTAO_CalculateEdges(const float centerZ, const float leftZ, const float rightZ, const float topZ, const float bottomZ)
{
    vec4 edgesLRTB = vec4(leftZ, rightZ, topZ, bottomZ) - vec4(centerZ);

    float slopeLR = (edgesLRTB.y - edgesLRTB.x) * 0.5;
    float slopeTB = (edgesLRTB.w - edgesLRTB.z) * 0.5;
    vec4 edgesLRTBSlopeAdjusted = edgesLRTB + vec4(slopeLR, -slopeLR, slopeTB, -slopeTB);
    edgesLRTB = max(-abs(edgesLRTB), -abs(edgesLRTBSlopeAdjusted));
    return vec4(clamp((1.25 - edgesLRTB / (centerZ * 0.011)), 0.0, 1.0));
}


// packing/unpacking for edges; 2 bits per edge mean 4 gradient values (0, 0.33, 0.66, 1) for smoother transitions!
float GTAO_PackEdges(vec4 edgesLRTB)
{
    // integer version:
    // edgesLRTB = saturate(edgesLRTB) * 2.9.xxxx + 0.5.xxxx;
    // return (((uint)edgesLRTB.x) << 6) + (((uint)edgesLRTB.y) << 4) + (((uint)edgesLRTB.z) << 2) + (((uint)edgesLRTB.w));

    // GLSL version:
    edgesLRTB = round(clamp(edgesLRTB, 0.0, 1.0) * 2.9);
    return dot(edgesLRTB, vec4(64.0 / 255.0, 16.0 / 255.0, 4.0 / 255.0, 1.0 / 255.0));
}

// http://h14s.p5r.org/2012/09/0x5f3759df.html, [Drobot2014a] Low Level Optimizations for GCN, https://blog.selfshadow.com/publications/s2016-shading-course/activision/s2016_pbs_activision_occlusion.pdf slide 63
float GTAO_FastSqrt(float x)
{
    return (intBitsToFloat(0x1fbd1df5 + (floatBitsToInt(x) >> 1)));
}

// input [-1, 1] and output [0, PI], from https://seblagarde.wordpress.com/2014/12/01/inverse-trigonometric-functions-gpu-optimization-for-amd-gcn-architecture/
float GTAO_FastACos(float inX)
{
    float x = abs(inX);
    float res = -0.156583 * x + PI_HALF;
    //res *= GTAO_FastSqrt(1.0 - x);
    res *= sqrt(1.0 - x);

    return (inX >= 0) ? res : PI - res;
}


uint  GTAO_EncodeVisibilityBentNormal(float visibility, vec3 bentNormal)
{
    return GTAO_FLOAT4_to_R8G8B8A8_UNORM(vec4(bentNormal * 0.5 + 0.5, visibility));
}

void GTAO_OutputWorkingTerm(ivec2 pixCoord, float visibility, vec3 bentNormal)
{
    visibility = clamp(visibility / GTAO_OCCLUSION_TERM_SCALE, 0.0, 1.0);
//#if defined(__X2_GTAO_COMPUTE_BENT_NORMALS)
//    imageStore(o_AOwBentNormals, pixCoord, uvec4(GTAO_EncodeVisibilityBentNormal(visibility, bentNormal), 0, 0, 0));
//#else
    imageStore(o_AOwBentNormals, pixCoord, uvec4(uint(visibility * 255.0 + 0.5), 0, 0, 0));
//#endif
}


// "Efficiently building a matrix to rotate one vector to another"
// http://cs.brown.edu/research/pubs/pdfs/1999/Moller-1999-EBA.pdf / https://dl.acm.org/doi/10.1080/10867651.1999.10487509
// (using https://github.com/assimp/assimp/blob/master/include/assimp/matrix3x3.inl#L275 as a code reference as it seems to be best)
mat3 GTAO_RotFromToMatrix(vec3 from, vec3 to)
{
    const float e = dot(from, to);
    const float f = abs(e); //(e < 0)? -e:e;

    // WARNING: This has not been tested/worked through, especially not for 16bit floats; seems to work in our special use case (from is always {0, 0, -1}) but wouldn't use it in general
    if (f > float(1.0 - 0.0003))
        return mat3(1, 0, 0, 0, 1, 0, 0, 0, 1);

    const vec3 v = cross(from, to);
    /* ... use this hand optimized version (9 mults less) */
    const float h = (1.0) / (1.0 + e);      /* optimization by Gottfried Chen */
    const float hvx = h * v.x;
    const float hvz = h * v.z;
    const float hvxy = hvx * v.y;
    const float hvxz = hvx * v.z;
    const float hvyz = hvz * v.y;

    mat3 mtx;
    mtx[0][0] = e + hvx * v.x;
    mtx[0][1] = hvxy - v.z;
    mtx[0][2] = hvxz + v.y;

    mtx[1][0] = hvxy + v.z;
    mtx[1][1] = e + h * v.y * v.y;
    mtx[1][2] = hvyz - v.x;

    mtx[2][0] = hvxz - v.y;
    mtx[2][1] = hvyz + v.x;
    mtx[2][2] = e + hvz * v.z;

    return mtx;
}

float LinearizeDepth(const float screenDepth)
{
    float depthLinearizeMul = u_Camera.DepthUnpackConsts.x;
    float depthLinearizeAdd = u_Camera.DepthUnpackConsts.y;
    // Optimised version of "-cameraClipNear / (cameraClipFar - projDepth * (cameraClipFar - cameraClipNear)) * cameraClipFar"
    return depthLinearizeMul / (depthLinearizeAdd - screenDepth);
}

//float LinearizeDepth(const float screenDepth)
//{
//    return u_Camera.nearPlane * u_Camera.farPlane / (screenDepth * (u_Camera.farPlane - u_Camera.nearPlane) - u_Camera.farPlane);
//}

vec4 LinearizeDepth(const vec4 screenDepths)
{
    return vec4(LinearizeDepth(screenDepths.x), LinearizeDepth(screenDepths.y), LinearizeDepth(screenDepths.z), LinearizeDepth(screenDepths.w));
}

void GTAO_MainPass(const ivec2 outputPixCoord, const ivec2 inputPixCoords, float sliceCount, float stepsPerSlice, const vec2 localNoise)
{
    vec4 viewspaceNormalLuminance = texelFetch(u_ViewNormal, inputPixCoords, 0);
    vec3 viewspaceNormal = viewspaceNormalLuminance.xyz;
    //viewspaceNormal.yz = -viewspaceNormalLuminance.yz;


    vec2 normalizedScreenPos = (inputPixCoords + 0.5.xx) * u_ScreenData.InvFullResolution;

    vec4 deviceZs = textureGather(u_HiZDepth, vec2(inputPixCoords * u_ScreenData.InvFullResolution * u_GTAOConsts.X2BUVFactor));
    vec4 valuesUL = LinearizeDepth(deviceZs);
    vec4 valuesBR = LinearizeDepth(textureGatherOffset(u_HiZDepth, vec2(inputPixCoords * u_ScreenData.InvFullResolution * u_GTAOConsts.X2BUVFactor), ivec2(1, 1)));

    // viewspace Z at the center
    float viewspaceZ = valuesUL.y; //u_HiZDepth.SampleLevel( u_samplerPointClamp, normalizedScreenPos, 0 ).x; 

    // viewspace Zs left top right bottom
    const float pixLZ = valuesUL.x;
    const float pixTZ = valuesUL.z;
    const float pixRZ = valuesBR.z;
    const float pixBZ = valuesBR.x;



    vec4 edgesLRTB = GTAO_CalculateEdges(viewspaceZ, pixLZ, pixRZ, pixTZ, pixBZ);

    imageStore(o_Edges, outputPixCoord, vec4(GTAO_PackEdges(edgesLRTB), 0.0, 0.0 ,0.0));

    // Move center pixel slightly towards camera to avoid imprecision artifacts due to depth buffer imprecision; offset depends on depth texture format used

    viewspaceZ *= 0.99999;     // this is good for FP32 depth buffer

    const vec3 pixCenterPos = GTAO_ComputeViewspacePosition(normalizedScreenPos, viewspaceZ);
    const vec3 viewVec = normalize(-pixCenterPos);

    


    // prevents normals that are facing away from the view vector - GTAO struggles with extreme cases, but in Vanilla it seems rare so it's disabled by default
    // viewspaceNormal = normalize( viewspaceNormal + max( 0, -dot( viewspaceNormal, viewVec ) ) * viewVec );


    const float effectRadius = u_GTAOConsts.EffectRadius * u_GTAOConsts.RadiusMultiplier;
    const float sampleDistributionPower = u_GTAOConsts.SampleDistributionPower;
    const float thinOccluderCompensation = u_GTAOConsts.ThinOccluderCompensation;
    const float falloffRange = u_GTAOConsts.EffectFalloffRange * effectRadius;

    const float falloffFrom = effectRadius * (1.0 - u_GTAOConsts.EffectFalloffRange);

    // fadeout precompute optimisation
    const float falloffMul = -1.0 / (falloffRange);
    const float falloffAdd = falloffFrom / (falloffRange)+1.0;

    float visibility = 0;

//#if __X2_GTAO_COMPUTE_BENT_NORMALS
//    vec3 bentNormal = vec3(0); 
//#else
    vec3 bentNormal = viewspaceNormal;
//#endif

    // see "Algorithm 1" in https://www.activision.com/cdn/research/Practical_Real_Time_Strategies_for_Accurate_Indirect_Occlusion_NEW%20VERSION_COLOR.pdf
    {
        const float noiseSlice = localNoise.x;
        const float noiseSample = localNoise.y;

        // quality settings / tweaks / hacks
        const float pixelTooCloseThreshold = 1.3;      // if the offset is under approx pixel size (pixelTooCloseThreshold), push it out to the minimum distance

        // approx viewspace pixel size at inputPixCoords; approximation of NDCToViewspace( normalizedScreenPos.xy + u_ScreenData.InvFullResolution.xy, pixCenterPos.z ).xy - pixCenterPos.xy;
        const vec2 pixelDirRBViewspaceSizeAtCenterZ = -viewspaceZ.xx * u_GTAOConsts.NDCToViewMul_x_PixelSize; // - becaues viewspaceZ is negative!

        float screenspaceRadius = effectRadius / pixelDirRBViewspaceSizeAtCenterZ.x;

       

        // fade out for small screen radii 
        visibility += clamp(((10 + screenspaceRadius) / 100),0.0,1.0) * 0.5;

#if 1   // sensible early-out for even more performance; disabled because not yet tested
        //float normals = viewspaceNormal;

        //imageStore(o_Debug, outputPixCoord, vec4(vec3(screenspaceRadius), 1.0f));

        if ( screenspaceRadius < pixelTooCloseThreshold)
        {
            GTAO_OutputWorkingTerm(outputPixCoord, 1, viewspaceNormal);
            return;
        }
#endif

        // this is the min distance to start sampling from to avoid sampling from the center pixel (no useful data obtained from sampling center pixel)
        //const float minS = pixelTooCloseThreshold / screenspaceRadius;

        #pragma unroll 6
        for (float slice = 0; slice < sliceCount; slice++)
        {
            float sliceK = (slice + noiseSlice) / sliceCount;

            // lines 5, 6 from the paper
            float phi = 0.166* PI;
            float cosPhi = cos(phi);
            float sinPhi = sin(phi);
            vec2 omega = vec2(cosPhi, sinPhi);       //vec2 on omega causes issues with big radii

            // convert to screen units (pixels) for later use
            omega *= screenspaceRadius;

            // line 8 from the paper
            const vec3 directionVec = vec3(cosPhi, sinPhi, 0);

            // line 9 from the paper
            const vec3 orthoDirectionVec = directionVec - (dot(directionVec, viewVec) * viewVec);

            // line 10 from the paper
            //axisVec is orthogonal to directionVec and viewVec, used to define projectedNormal
            const vec3 axisVec = normalize(cross(directionVec, viewVec));

            // alternative line 9 from the paper
            // vec3 orthoDirectionVec = cross( viewVec, axisVec );

            // line 11 from the paper
            vec3 projectedNormalVec = viewspaceNormal - axisVec * dot(viewspaceNormal, axisVec);

            // line 13 from the paper
            float signNorm = sign(dot(orthoDirectionVec, projectedNormalVec));

            // line 14 from the paper
            float projectedNormalVecLength = length(projectedNormalVec);
            float cosNorm = clamp((dot(projectedNormalVec, viewVec) / projectedNormalVecLength), 0.0, 1.0);

            // line 15 from the paper
            float n = signNorm * GTAO_FastACos(cosNorm);


            // this is a lower weight target; not using -1 as in the original paper because it is under horizon, so a 'weight' has different meaning based on the normal
            const float lowHorizonCos0 = cos(n + PI_HALF);
            const float lowHorizonCos1 = cos(n - PI_HALF);

            // lines 17, 18 from the paper, manually unrolled the 'side' loop
            float horizonCos0 = lowHorizonCos0; //-1;
            float horizonCos1 = lowHorizonCos1; //-1;

            #pragma unroll 5
            for (float step = 0; step < stepsPerSlice; step++)
            {
                // R1 sequence (http://extremelearning.com.au/unreasonable-effectiveness-of-quasirandom-sequences/)
                const float stepBaseNoise = float(slice + step * stepsPerSlice) * 0.6180339887498948482; // <- this should unroll
                float stepNoise = fract(noiseSample + stepBaseNoise) * 0.5 + 0.5;

                // approx line 20 from the paper, with added noise
                float s = stepNoise * (step ) / (stepsPerSlice); // + (vec2)1e-6f);

                // additional distribution modifier
                s = pow(s, sampleDistributionPower);

                // avoid sampling center pixel
                //s += minS;

                // approx lines 21-22 from the paper, unrolled
                vec2 sampleOffset = s * omega;

                float sampleOffsetLength = length(sampleOffset);

                // note: when sampling, using point_point_point or point_point_linear sampler works, but linear_linear_linear will cause unwanted interpolation between neighbouring depth values on the same MIP level!
                const float mipLevel = clamp(log2(sampleOffsetLength) - u_GTAOConsts.DepthMIPSamplingOffset, 0, GTAO_DEPTH_MIP_LEVELS);

                // Snap to pixel center (more correct direction math, avoids artifacts due to sampling pos not matching depth texel center - messes up slope - but adds other 
                // artifacts due to them being pushed off the slice). Also use full precision for high res cases.
                sampleOffset = sampleOffset * u_ScreenData.InvFullResolution;

                vec2 sampleScreenPos0 = normalizedScreenPos + sampleOffset;
                float  SZ0 = LinearizeDepth(textureLod(u_HiZDepth, sampleScreenPos0 * u_GTAOConsts.X2BUVFactor, mipLevel).x);
                vec3 samplePos0 = GTAO_ComputeViewspacePosition(sampleScreenPos0, SZ0);

                vec2 sampleScreenPos1 = normalizedScreenPos - sampleOffset;
                float  SZ1 = LinearizeDepth(textureLod(u_HiZDepth, sampleScreenPos1 * u_GTAOConsts.X2BUVFactor, mipLevel).x);
                vec3 samplePos1 = GTAO_ComputeViewspacePosition(sampleScreenPos1, SZ1);

                vec3 sampleDelta0 = (samplePos0 - pixCenterPos); // using float for sampleDelta causes precision issues
                vec3 sampleDelta1 = (samplePos1 - pixCenterPos); // using float for sampleDelta causes precision issues
                float sampleDist0 = length(sampleDelta0);
                float sampleDist1 = length(sampleDelta1);

                // approx lines 23, 24 from the paper, unrolled
                vec3 sampleHorizonVec0 = (sampleDelta0 / sampleDist0);
                vec3 sampleHorizonVec1 = (sampleDelta1 / sampleDist1);

                // this is our own thickness heuristic that relies on sooner discarding samples behind the center
                float falloffBase0 = length(vec3(sampleDelta0.x, sampleDelta0.y, sampleDelta0.z * (1 + thinOccluderCompensation)));
                float falloffBase1 = length(vec3(sampleDelta1.x, sampleDelta1.y, sampleDelta1.z * (1 + thinOccluderCompensation)));
                float weight0 = clamp((falloffBase0 * falloffMul + falloffAdd), 0.0, 1.0);
                float weight1 = clamp((falloffBase1 * falloffMul + falloffAdd), 0.0, 1.0);

                // sample horizon cos
               // float rad0 = PI_HALF + asin(-sampleHorizonVec0.z / length(sampleHorizonVec0));
                //float rad1 = asin(-sampleHorizonVec1.z / length(sampleHorizonVec1)) - PI_HALF;
                
                //float shc0 = cos(rad0);
                //float shc1 = cos(rad1);

                float shc0 = dot(sampleHorizonVec0, viewVec);
                float shc1 = dot(sampleHorizonVec1, viewVec);

                // discard unwanted samples
                shc0 = mix(horizonCos0, shc0, weight0); // this would be more correct but too expensive: cos(lerp( acos(lowHorizonCos0), acos(shc0), weight0 ));
                shc1 = mix(horizonCos1, shc1, weight1); // this would be more correct but too expensive: cos(lerp( acos(lowHorizonCos1), acos(shc1), weight1 ));

                // thickness heuristic - see "4.3 Implementation details, Height-field assumption considerations"

                horizonCos0 = max(horizonCos0, shc0);
                horizonCos1 = max(horizonCos1, shc1);
            }


            // I can't figure out the slight overdarkening on high slopes, so I'm adding this fudge - in the training set, 0.05 is close (PSNR 21.34) to disabled (PSNR 21.45)
            projectedNormalVecLength = mix(projectedNormalVecLength, 1, 0.05);

            // line ~27, unrolled
            float h0 = n + clamp( -GTAO_FastACos(horizonCos1) - n , -PI_HALF, PI_HALF);
            float h1 = n + clamp( GTAO_FastACos(horizonCos0) -n , -PI_HALF, PI_HALF);

            imageStore(o_Debug, outputPixCoord, vec4(h1, n, horizonCos0, horizonCos1));


            float iarc0 = (cosNorm + 2.0 * h0 * sin(n) - cos(2 * h0 - n)) / 4.0;
            float iarc1 = (cosNorm + 2.0 * h1 * sin(n) - cos(2 * h1 - n)) / 4.0;
            float localVisibility = projectedNormalVecLength * (iarc0 + iarc1);
            visibility += localVisibility;

//#if __X2_GTAO_COMPUTE_BENT_NORMALS
//            // see "Algorithm 2 Extension that computes bent normals b."
//            float t0 = (6 * sin(h0 - n) - sin(3 * h0 - n) + 6 * sin(h1 - n) - sin(3 * h1 - n) + 16 * sin(n) - 3 * (sin(h0 + n) + sin(h1 + n))) / 12;
//            float t1 = (-cos(3 * h0 - n) - cos(3 * h1 - n) + 8 * cos(n) - 3 * (cos(h0 + n) + cos(h1 + n))) / 12;
//            vec3 localBentNormal = vec3(directionVec.x * t0, directionVec.y * t0, - t1 );
//            localBentNormal = (vec3)mul(GTAO_RotFromToMatrix(vec3(0, 0, -1), viewVec), localBentNormal) * projectedNormalVecLength;
//            bentNormal += localBentNormal;
//#endif
        }
        visibility /= sliceCount;

        visibility = pow(visibility, u_GTAOConsts.FinalValuePower * mix(1.0f, u_GTAOConsts.ShadowTolerance, viewspaceNormalLuminance.a));
        visibility = max(0.03, visibility); // disallow total occlusion (which wouldn't make any sense anyhow since pixel is visible but also helps with packing bent normals)

//#if __X2_GTAO_COMPUTE_BENT_NORMALS
//        bentNormal = normalize(bentNormal);
//#endif
    }

    GTAO_OutputWorkingTerm(outputPixCoord, visibility, bentNormal);
}


vec2 SpatioTemporalNoise(uvec2 pixCoord, uint temporalIndex)    // without TAA, temporalIndex is always 0
{
    vec2 noise;
    // Hilbert curve driving R2 (see https://www.shadertoy.com/view/3tB3z3)
    uint index = texture(u_HilbertLut, pixCoord % 64).x;
    index += 288 * (temporalIndex % 64); // why 288? tried out a few and that's the best so far (with XE_HILBERT_LEVEL 6U) - but there's probably better :)
    index += 0; // why 288? tried out a few and that's the best so far (with XE_HILBERT_LEVEL 6U) - but there's probably better :)
    // R2 sequence - see http://extremelearning.com.au/unreasonable-effectiveness-of-quasirandom-sequences/
    return vec2(fract(0.5 + index * vec2(0.75487766624669276005, 0.5698402909980532659114)));
}


layout(local_size_x = GTAO_NUMTHREADS_X, local_size_y = GTAO_NUMTHREADS_Y, local_size_z = 1) in;
void main()
{
    const ivec2 outputPixCoords = ivec2(gl_WorkGroupID.xy) * ivec2(GTAO_NUMTHREADS_X, GTAO_NUMTHREADS_Y) + ivec2(gl_LocalInvocationID.xy);
    const ivec2 inputPixCoords = outputPixCoords * (1 + int(u_GTAOConsts.HalfRes));
    GTAO_MainPass(outputPixCoords, inputPixCoords, 6, 5, SpatioTemporalNoise(inputPixCoords, u_GTAOConsts.NoiseIndex));
}


