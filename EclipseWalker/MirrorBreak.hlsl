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
    int gPadding;
};

Texture2D gTextureMaps[1000] : register(t0);
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
    const float2 i = floor(p);
    const float2 f = frac(p);
    const float2 u = f * f * (3.0f - 2.0f * f);

    const float a = Hash(i + float2(0.0f, 0.0f));
    const float b = Hash(i + float2(1.0f, 0.0f));
    const float c = Hash(i + float2(0.0f, 1.0f));
    const float d = Hash(i + float2(1.0f, 1.0f));

    return lerp(lerp(a, b, u.x), lerp(c, d, u.x), u.y);
}

float Fbm(float2 p)
{
    float sum = 0.0f;
    float amp = 0.5f;
    for (int i = 0; i < 4; ++i)
    {
        sum += Noise(p) * amp;
        p = p * 2.03f + float2(11.3f, 7.1f);
        amp *= 0.5f;
    }
    return sum;
}

VertexOut VS(VertexIn vin)
{
    VertexOut vout = (VertexOut)0.0f;
    float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
    vout.PosH = mul(posW, gViewProj);
    vout.TexC = mul(float4(vin.TexC, 0.0f, 1.0f), gTexTransform).xy;
    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
    if (gDiffuseMapIndex < 0)
    {
        return float4(0.0f, 0.0f, 0.0f, 1.0f);
    }

    const float progress = saturate(gDomainRadius);
    const bool isActive = (gIsDomainActive != 0) && (progress > 0.0001f);

    float2 uv = saturate(pin.TexC);
    float4 baseColor = gTextureMaps[gDiffuseMapIndex].Sample(gsamAnisotropicWrap, uv) * gDiffuseAlbedo;
    if (!isActive)
    {
        return baseColor;
    }

    const float2 centeredUv = uv * 2.0f - 1.0f;
    const float radius = length(centeredUv);
    const float2 radialDir = radius > 0.0001f ? centeredUv / radius : float2(0.0f, 0.0f);
    const float radialWeight = smoothstep(0.08f, 1.0f, radius);

    const float2 seamOrigin = float2(0.48f, 0.68f);
    const float2 seamTangent = normalize(float2(1.0f, -0.52f));
    const float2 seamNormal = float2(-seamTangent.y, seamTangent.x);
    const float seamCoord = dot(uv - seamOrigin, seamTangent);

    const float seamNoiseA = Fbm(float2(seamCoord * 7.0f - gTotalTime * 0.20f, seamCoord * 1.7f + 11.0f));
    const float seamNoiseB = Fbm(float2(seamCoord * 13.0f + 23.0f, seamCoord * 2.1f + gTotalTime * 0.14f));
    const float seamWobble = (seamNoiseA * 2.0f - 1.0f) * (0.006f + progress * 0.006f);
    const float seamWobble2 = (seamNoiseB * 2.0f - 1.0f) * (0.002f + progress * 0.003f);
    const float signedDist = dot(uv - seamOrigin, seamNormal) + seamWobble + seamWobble2;

    const bool isLowerShard = signedDist >= 0.0f;
    const float sideSign = isLowerShard ? 1.0f : -1.0f;
    const float seamEdge = 1.0f - smoothstep(0.010f + progress * 0.010f, 0.080f + progress * 0.030f, abs(signedDist));
    const float seamCore = 1.0f - smoothstep(0.0016f, 0.0052f + progress * 0.0025f, abs(signedDist));
    const float seamInfluence = smoothstep(0.28f, 0.0f, abs(signedDist));

    const float flowA = Fbm(uv * (4.8f + progress * 3.0f) + float2(gTotalTime * 0.10f, -gTotalTime * 0.06f));
    const float seamDrift = (flowA * 2.0f - 1.0f) * (0.004f + progress * 0.006f);
    const float sideSplitScale = sideSign >= 0.0f ? 1.45f : 1.0f;
    const float splitAmount = progress * (0.022f + seamInfluence * 0.050f) * sideSplitScale;
    const float tangentAmount = (progress * (0.006f + seamInfluence * 0.016f) + seamDrift) * (sideSign >= 0.0f ? 1.20f : 1.0f);

    const float2 leftUv = clamp(
        uv + seamNormal * splitAmount + seamTangent * tangentAmount + radialDir * radialWeight * progress * 0.003f,
        float2(0.001f, 0.001f),
        float2(0.999f, 0.999f));
    const float2 rightUv = clamp(
        uv - seamNormal * splitAmount - seamTangent * tangentAmount - radialDir * radialWeight * progress * 0.003f,
        float2(0.001f, 0.001f),
        float2(0.999f, 0.999f));

    const float2 ghostUv = isLowerShard ? leftUv : rightUv;
    const float upperShiftX = progress * (0.018f + seamInfluence * 0.022f);
    const float2 upperPrimaryUv = clamp(
        uv + float2(upperShiftX, 0.0f),
        float2(0.001f, 0.001f),
        float2(0.999f, 0.999f));
    const float2 primaryUv = isLowerShard ? ghostUv : upperPrimaryUv;
    float3 finalColor = gTextureMaps[gDiffuseMapIndex].Sample(gsamAnisotropicWrap, primaryUv).rgb;

    if (isLowerShard)
    {
        const float2 ghostTrailUv = clamp(
            ghostUv + seamNormal * 0.008f + seamTangent * 0.006f,
            float2(0.001f, 0.001f),
            float2(0.999f, 0.999f));
        const float3 ghostTrailColor = gTextureMaps[gDiffuseMapIndex].Sample(gsamAnisotropicWrap, ghostTrailUv).rgb;
        const float ghostWeight = seamInfluence * (0.18f + progress * 0.28f);
        finalColor = lerp(finalColor, ghostTrailColor * float3(0.78f, 0.90f, 1.06f), ghostWeight);
    }

    const float chromaStrength = progress * (0.002f + seamInfluence * 0.010f);
    const float3 sceneR = gTextureMaps[gDiffuseMapIndex].Sample(
        gsamAnisotropicWrap,
        clamp(primaryUv + seamNormal * chromaStrength, 0.001f, 0.999f)).rgb;
    const float3 sceneB = gTextureMaps[gDiffuseMapIndex].Sample(
        gsamAnisotropicWrap,
        clamp(primaryUv - seamNormal * chromaStrength, 0.001f, 0.999f)).rgb;
    finalColor = float3(sceneR.r, finalColor.g, sceneB.b);

    const float luminance = dot(finalColor, float3(0.299f, 0.587f, 0.114f));
    const float desaturateAmount = isLowerShard ? (0.20f + progress * 0.18f) : (0.06f + progress * 0.05f);
    finalColor = lerp(finalColor, luminance.xxx, desaturateAmount);
    finalColor *= isLowerShard
        ? lerp(float3(0.92f, 1.0f, 1.0f), float3(0.62f, 0.92f, 1.12f), 0.42f + progress * 0.28f)
        : lerp(float3(1.0f, 1.0f, 1.0f), float3(0.90f, 0.97f, 1.03f), 0.10f + progress * 0.08f);

    const float electricNoise = Fbm(float2(seamCoord * 20.0f + gTotalTime * 1.2f, signedDist * 180.0f - gTotalTime * 2.0f));
    const float electric = smoothstep(0.68f, 0.84f, electricNoise) * seamEdge * (0.55f + progress * 0.65f);
    const float seamGlow = seamEdge * (0.35f + 0.65f * sin(gTotalTime * 12.0f + seamCoord * 45.0f) * 0.5f + 0.5f);

    finalColor = lerp(finalColor, float3(0.02f, 0.03f, 0.05f), seamCore * 0.95f);
    finalColor += float3(0.18f, 0.42f, 0.72f) * seamGlow * 0.30f * progress;
    finalColor += float3(0.22f, 0.62f, 1.0f) * electric;
    finalColor *= 1.0f - radialWeight * progress * 0.10f;

    if (gFogPad.y > 0.5f)
    {
        float2 texel = gInvRenderTargetSize;
        float3 bloom = 0.0f;
        bloom += EwExtractBloom(gTextureMaps[gDiffuseMapIndex].Sample(gsamAnisotropicWrap, primaryUv).rgb) * 0.24f;
        bloom += EwExtractBloom(gTextureMaps[gDiffuseMapIndex].Sample(gsamAnisotropicWrap, saturate(primaryUv + texel * float2(3.0f, 0.0f))).rgb) * 0.10f;
        bloom += EwExtractBloom(gTextureMaps[gDiffuseMapIndex].Sample(gsamAnisotropicWrap, saturate(primaryUv + texel * float2(-3.0f, 0.0f))).rgb) * 0.10f;
        bloom += EwExtractBloom(gTextureMaps[gDiffuseMapIndex].Sample(gsamAnisotropicWrap, saturate(primaryUv + texel * float2(0.0f, 3.0f))).rgb) * 0.10f;
        bloom += EwExtractBloom(gTextureMaps[gDiffuseMapIndex].Sample(gsamAnisotropicWrap, saturate(primaryUv + texel * float2(0.0f, -3.0f))).rgb) * 0.10f;

        finalColor = EwApplyFilmGrade(finalColor + bloom * 0.12f, uv, 0.85f);
    }
    return float4(saturate(finalColor), 1.0f);
}
