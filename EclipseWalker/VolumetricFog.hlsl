#include "Light.hlsl"

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
    int gPadding;
};

Texture2D gTextureMaps[1000] : register(t0);
Texture2DMS<float> gSceneDepthMS : register(t1001);
SamplerState gsamAnisotropicWrap : register(s4);

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

float3 AccumulatePointLightScatter(float3 samplePos)
{
    float3 result = 0.0f;

    [unroll]
    for (int i = 1; i < MAX_LIGHTS; ++i)
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

    const int sampleCount = 12;
    float stepLength = rayLength / sampleCount;
    float baseDensity = 0.62f / max(gFogRange, 1.0f);
    float3 fogColor = 0.0f;
    float transmittance = 1.0f;

    [unroll]
    for (int i = 0; i < sampleCount; ++i)
    {
        float t = (i + 0.5f) / sampleCount;
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
    return float4(finalColor, 1.0f);
}
