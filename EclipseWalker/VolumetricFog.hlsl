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
    float gMetallicFactor;
};

Texture2D gTextureMaps[1000] : register(t0);
Texture2DMS<float> gSceneDepthMS : register(t1001);
SamplerState gsamAnisotropicWrap : register(s4);

static const int FOG_SAMPLE_COUNT = 8;
static const int FOG_POINT_LIGHT_END = 6; // Stage1 torches use light slots 1-5.
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

float Hash(float2 p)
{
    return frac(sin(dot(p, float2(127.1f, 311.7f))) * 43758.5453123f);
}

float Noise(float2 p)
{
    float2 i = floor(p);
    float2 f = frac(p);
    float2 u = f * f * (3.0f - 2.0f * f);

    float a = Hash(i);
    float b = Hash(i + float2(1.0f, 0.0f));
    float c = Hash(i + float2(0.0f, 1.0f));
    float d = Hash(i + float2(1.0f, 1.0f));
    return lerp(lerp(a, b, u.x), lerp(c, d, u.x), u.y);
}

float Fbm(float2 p)
{
    float value = 0.0f;
    float amp = 0.5f;
    [unroll]
    for (int i = 0; i < 4; ++i)
    {
        value += Noise(p) * amp;
        p = p * 2.03f + float2(9.7f, 4.3f);
        amp *= 0.5f;
    }
    return value;
}

VertexOut VS(VertexIn vin)
{
    VertexOut vout = (VertexOut)0.0f;
    vout.PosH = float4(vin.PosL.xy, 0.0f, 1.0f);
    vout.TexC = vin.TexC;
    return vout;
}

float3 ReconstructWorldPosition(float2 uv, float depth)
{
    float2 ndc = float2(uv.x * 2.0f - 1.0f, (1.0f - uv.y) * 2.0f - 1.0f);
    float4 clipPos = float4(ndc, depth, 1.0f);
    float4 worldPos = mul(clipPos, gInvViewProj);
    return worldPos.xyz / max(worldPos.w, 0.0001f);
}

float SampleSceneDepth(int2 pixel)
{
    int2 maxPixel = int2(gRenderTargetSize) - int2(1, 1);
    pixel = clamp(pixel, int2(0, 0), maxPixel);
    return min(gSceneDepthMS.Load(pixel, 0), 0.9995f);
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

        ao = 1.0f - saturate(occlusion / SSAO_SAMPLE_COUNT) * 0.68f;
    }

    return saturate(ao);
}

float3 AccumulatePointLightScatter(float3 samplePos)
{
    float3 result = 0.0f;

    [loop]
    for (int i = 1; i < FOG_POINT_LIGHT_END; ++i)
    {
        float strengthLength = length(gLights[i].Strength);
        if (strengthLength <= 0.0001f)
        {
            continue;
        }

        float3 toLight = gLights[i].Position - samplePos;
        float distanceToLight = length(toLight);
        if (distanceToLight >= gLights[i].FalloffEnd)
        {
            continue;
        }

        float range = max(gLights[i].FalloffEnd - gLights[i].FalloffStart, 0.001f);
        float attenuation = saturate((gLights[i].FalloffEnd - distanceToLight) / range);
        attenuation *= attenuation;
        result += gLights[i].Strength * attenuation * 0.08f;
    }

    return result;
}

float4 PS(VertexOut pin) : SV_Target
{
    if (gDiffuseMapIndex < 0)
    {
        return float4(0.0f, 0.0f, 0.0f, 1.0f);
    }

    float2 uv = saturate(pin.TexC);
    float3 sceneColor = gTextureMaps[gDiffuseMapIndex].Sample(gsamAnisotropicWrap, uv).rgb;

    int2 pixel = int2(pin.PosH.xy);
    int2 maxPixel = int2(gRenderTargetSize) - int2(1, 1);
    pixel = clamp(pixel, int2(0, 0), maxPixel);

    float depth = min(gSceneDepthMS.Load(pixel, 0), 0.9995f);

    float3 worldEnd = ReconstructWorldPosition(uv, depth);
    float3 ray = worldEnd - gEyePosW;
    float rayLength = length(ray);
    if (rayLength <= 0.001f)
    {
        return float4(sceneColor, 1.0f);
    }

    float3 rayDir = ray / rayLength;
    float maxMarchLength = max(gFogStart + gFogRange * 2.2f, 22.0f);
    rayLength = min(rayLength, maxMarchLength);
    ray = rayDir * rayLength;

    float3 lightDir = normalize(-gLights[0].Direction);
    float phase = pow(saturate(dot(rayDir, lightDir)) * 0.5f + 0.5f, 3.0f);
    float3 scatterColor = gFogColor.rgb * (0.55f + phase * 1.15f) + gLights[0].Strength * phase * 0.12f;

    float stepLength = rayLength / FOG_SAMPLE_COUNT;
    float baseDensity = 0.62f / max(gFogRange, 1.0f);
    float3 fogColor = 0.0f;
    float transmittance = 1.0f;

    [loop]
    for (int i = 0; i < FOG_SAMPLE_COUNT; ++i)
    {
        float t = (i + 0.5f) / FOG_SAMPLE_COUNT;
        float3 samplePos = gEyePosW + ray * t;
        float travel = rayLength * t;

        float distanceMask = smoothstep(gFogStart, gFogStart + gFogRange, travel);
        float heightMask = gHeightFogStrength > 0.0001f
            ? saturate((gHeightFogTop - samplePos.y) / max(gHeightFogRange, 0.001f)) * gHeightFogStrength
            : 0.0f;

        float noise = Fbm(samplePos.xz * 0.085f + float2(gTotalTime * 0.035f, -gTotalTime * 0.022f));
        float density = baseDensity * max(distanceMask, heightMask) * lerp(0.55f, 1.45f, noise);
        float alpha = 1.0f - exp(-density * stepLength);
        float3 localScatter = scatterColor + AccumulatePointLightScatter(samplePos);

        fogColor += transmittance * alpha * localScatter;
        transmittance *= exp(-density * stepLength);
    }

    float3 finalColor = sceneColor * transmittance + fogColor;
    if (gFogPad.y > 0.5f)
    {
        finalColor *= ComputeSSAO(uv, pixel);

        float2 bloomTexel = gInvRenderTargetSize;
        float3 bloom = 0.0f;
        bloom += EwExtractBloom(sceneColor) * 0.22f;
        bloom += EwExtractBloom(gTextureMaps[gDiffuseMapIndex].Sample(gsamAnisotropicWrap, saturate(uv + bloomTexel * float2(3.0f, 0.0f))).rgb) * 0.08f;
        bloom += EwExtractBloom(gTextureMaps[gDiffuseMapIndex].Sample(gsamAnisotropicWrap, saturate(uv + bloomTexel * float2(-3.0f, 0.0f))).rgb) * 0.08f;
        bloom += EwExtractBloom(gTextureMaps[gDiffuseMapIndex].Sample(gsamAnisotropicWrap, saturate(uv + bloomTexel * float2(0.0f, 3.0f))).rgb) * 0.08f;
        bloom += EwExtractBloom(gTextureMaps[gDiffuseMapIndex].Sample(gsamAnisotropicWrap, saturate(uv + bloomTexel * float2(0.0f, -3.0f))).rgb) * 0.08f;

        finalColor = EwApplyFilmGrade(finalColor + bloom * 0.12f, uv, 0.9f);
    }
    return float4(finalColor, 1.0f);
}
