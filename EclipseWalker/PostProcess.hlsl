#include "Light.hlsl"
#include "PostProcessCommon.hlsl"

cbuffer cbPerObject : register(b0)
{
    float4x4 gWorld;
    float4x4 gTexTransform;
    float4 gColorMultiplier;
};

cbuffer cbPass : register(b1)
{
    float4x4 gView;
    float4x4 gInvView;
    float4x4 gProj;
    float4x4 gInvProj;
    float4x4 gViewProj;
    float4x4 gInvViewProj;
    float4x4 gShadowTransform;
    float3 gEyePosW;
    float cbPerObjectPad1;
    float2 gRenderTargetSize;
    float2 gInvRenderTargetSize;
    float gNearZ;
    float gFarZ;
    float gTotalTime;
    float gDeltaTime;
    float4 gAmbientLight;
    Light gLights[MAX_LIGHTS];
    float3 gDomainCenter;
    float gDomainRadius;
    int gIsDomainActive;
    float3 gDomainPad;
    float4 gFogColor;
    float gFogStart;
    float gFogRange;
    float2 gFogPad;
    float4 gSkyTint;
    float gHeightFogTop;
    float gHeightFogRange;
    float gHeightFogStrength;
    float gHeightFogPad;
    float4 gSkyEclipseDirection;
    float4 gSkyEclipseParams;
};

cbuffer cbMaterial : register(b2)
{
    float4 gDiffuseAlbedo;
    float3 gFresnelR0;
    float gRoughness;
    float4 gOutlineColor;
    float gOutlineThickness;
    int gIsToon;
    int gIsTransparent;
    int gDiffuseMapIndex;
    int gNormalMapIndex;
    int gEmissiveMapIndex;
    int gMetallicMapIndex;
    int gPadding;
};

Texture2D gTextureMaps[1000] : register(t0);
Texture2DMS<float> gSceneDepthMS : register(t1001);
SamplerState gsamAnisotropicWrap : register(s4);

static const int SSAO_SAMPLE_COUNT = 8;

struct VertexIn
{
    float3 PosL : POSITION;
    float3 NormalL : NORMAL;
    float2 TexC : TEXCOORD;
    float3 TangentU : TANGENT;
};

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float2 TexC : TEXCOORD;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout = (VertexOut)0.0f;
    vout.PosH = float4(vin.PosL.xy, 0.0f, 1.0f);
    vout.TexC = mul(float4(vin.TexC, 0.0f, 1.0f), gTexTransform).xy;
    return vout;
}

float3 SampleBloom(float2 uv)
{
    float2 texel = gInvRenderTargetSize;
    float3 bloom = 0.0f;

    bloom += EwExtractBloom(gTextureMaps[gDiffuseMapIndex].Sample(gsamAnisotropicWrap, uv).rgb) * 0.28f;
    bloom += EwExtractBloom(gTextureMaps[gDiffuseMapIndex].Sample(gsamAnisotropicWrap, uv + texel * float2(2.0f, 0.0f)).rgb) * 0.12f;
    bloom += EwExtractBloom(gTextureMaps[gDiffuseMapIndex].Sample(gsamAnisotropicWrap, uv + texel * float2(-2.0f, 0.0f)).rgb) * 0.12f;
    bloom += EwExtractBloom(gTextureMaps[gDiffuseMapIndex].Sample(gsamAnisotropicWrap, uv + texel * float2(0.0f, 2.0f)).rgb) * 0.12f;
    bloom += EwExtractBloom(gTextureMaps[gDiffuseMapIndex].Sample(gsamAnisotropicWrap, uv + texel * float2(0.0f, -2.0f)).rgb) * 0.12f;
    bloom += EwExtractBloom(gTextureMaps[gDiffuseMapIndex].Sample(gsamAnisotropicWrap, uv + texel * float2(4.0f, 4.0f)).rgb) * 0.06f;
    bloom += EwExtractBloom(gTextureMaps[gDiffuseMapIndex].Sample(gsamAnisotropicWrap, uv + texel * float2(-4.0f, 4.0f)).rgb) * 0.06f;
    bloom += EwExtractBloom(gTextureMaps[gDiffuseMapIndex].Sample(gsamAnisotropicWrap, uv + texel * float2(4.0f, -4.0f)).rgb) * 0.06f;
    bloom += EwExtractBloom(gTextureMaps[gDiffuseMapIndex].Sample(gsamAnisotropicWrap, uv + texel * float2(-4.0f, -4.0f)).rgb) * 0.06f;

    return bloom;
}

float SampleSceneDepth(int2 pixel)
{
    int2 maxPixel = int2(gRenderTargetSize) - int2(1, 1);
    pixel = clamp(pixel, int2(0, 0), maxPixel);
    return min(gSceneDepthMS.Load(pixel, 0), 0.9995f);
}

float3 ReconstructWorldPosition(float2 uv, float depth)
{
    float2 ndc = float2(uv.x * 2.0f - 1.0f, (1.0f - uv.y) * 2.0f - 1.0f);
    float4 clipPos = float4(ndc, depth, 1.0f);
    float4 worldPos = mul(clipPos, gInvViewProj);
    return worldPos.xyz / max(worldPos.w, 0.0001f);
}

float ComputeSSAO(float2 uv, int2 pixel)
{
    float centerDepth = SampleSceneDepth(pixel);
    float ao = 1.0f;

    if (centerDepth < 0.9990f)
    {
        float3 centerWorld = ReconstructWorldPosition(uv, centerDepth);
        float centerViewDistance = length(centerWorld - gEyePosW);
        float radiusPixels = lerp(11.0f, 5.0f, saturate(centerViewDistance / 85.0f));
        float worldRadius = lerp(1.4f, 5.5f, saturate(centerViewDistance / 120.0f));

        static const float2 sampleOffsets[SSAO_SAMPLE_COUNT] =
        {
            float2(1.0f, 0.0f),
            float2(-1.0f, 0.0f),
            float2(0.0f, 1.0f),
            float2(0.0f, -1.0f),
            float2(0.707f, 0.707f),
            float2(-0.707f, 0.707f),
            float2(0.707f, -0.707f),
            float2(-0.707f, -0.707f)
        };

        float occlusion = 0.0f;
        [unroll]
        for (int i = 0; i < SSAO_SAMPLE_COUNT; ++i)
        {
            float ringScale = (i < 4) ? 0.62f : 1.0f;
            int2 samplePixel = pixel + int2(sampleOffsets[i] * radiusPixels * ringScale);
            float sampleDepth = SampleSceneDepth(samplePixel);
            if (sampleDepth >= 0.9990f)
            {
                continue;
            }

            float2 sampleUv = (float2(samplePixel) + 0.5f) * gInvRenderTargetSize;
            float3 sampleWorld = ReconstructWorldPosition(sampleUv, sampleDepth);
            float sampleViewDistance = length(sampleWorld - gEyePosW);
            float distanceToSample = length(sampleWorld - centerWorld);
            float rangeWeight = saturate(1.0f - distanceToSample / max(worldRadius, 0.001f));
            float closerAmount = centerViewDistance - sampleViewDistance;
            float depthWeight = smoothstep(0.025f, 0.85f, closerAmount);

            occlusion += depthWeight * rangeWeight;
        }

        float aoStrength = (gSkyEclipseDirection.w > 0.5f) ? 0.36f : 0.62f;
        float minAo = (gSkyEclipseDirection.w > 0.5f) ? 0.72f : 0.52f;
        ao = max(1.0f - saturate(occlusion / SSAO_SAMPLE_COUNT) * aoStrength, minAo);
    }

    return saturate(ao);
}

float4 PS(VertexOut pin) : SV_Target
{
    if (gDiffuseMapIndex < 0)
    {
        return float4(0.0f, 0.0f, 0.0f, 1.0f);
    }

    float2 uv = saturate(pin.TexC);
    float3 sceneColor = gTextureMaps[gDiffuseMapIndex].Sample(gsamAnisotropicWrap, uv).rgb;
    float ssao = ComputeSSAO(uv, int2(pin.PosH.xy));
    sceneColor *= ssao;

    float3 bloom = SampleBloom(uv) * 0.18f;
    float3 finalColor = sceneColor + bloom;

    if (gSkyEclipseDirection.w > 0.5f)
    {
        float eclipse = saturate(gSkyEclipseParams.x);
        finalColor *= lerp(float3(1.0f, 1.0f, 1.0f), float3(0.88f, 0.91f, 1.02f), eclipse * 0.25f);
    }

    float gradeStrength = (gSkyEclipseDirection.w > 0.5f) ? 0.62f : 1.0f;
    finalColor = EwApplyFilmGrade(finalColor, uv, gradeStrength);
    return float4(saturate(finalColor), 1.0f);
}
