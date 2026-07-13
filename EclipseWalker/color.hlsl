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
    
    float4 gAmbientLight;        // Ambient light
    Light gLights[MAX_LIGHTS];   // Scene lights

    float3 gDomainCenter;   // Domain center
    float  gDomainRadius;   // Domain shell radius
    int    gIsDomainActive; // Domain active flag
    float3 gDomainPad;      // Padding
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
    float  gRoughness;
    float4 gOutlineColor;
    float  gOutlineThickness;
    int    gIsToon;
    int    gIsTransparent;
    
    int    gDiffuseMapIndex;
    int    gNormalMapIndex;
    int    gEmissiveMapIndex;
    int    gMetallicMapIndex;
    float  gMetallicFactor;
};

Texture2D gTextureMaps[1000] : register(t0);
Texture2D gShadowMap         : register(t1000);

SamplerComparisonState gsamShadow : register(s6);
SamplerState gsamAnisotropicWrap : register(s4);

struct VertexIn
{
    float3 PosL    : POSITION;
    float3 NormalL : NORMAL;
    float2 TexC    : TEXCOORD;
    float3 TangentU : TANGENT;
};

struct VertexOut
{
    float4 PosH    : SV_POSITION; // Clip-space position
    float3 PosW    : POSITION;    // World position
    float3 NormalW : NORMAL;      // World normal
    float3 TangentW : TANGENT;
    float2 TexC    : TEXCOORD;
    float ViewDepth : TEXCOORD1;
};

// ---------------------------------------------------------------------------------------
// Vertex Shader
// ---------------------------------------------------------------------------------------
VertexOut VS(VertexIn vin)
{
    VertexOut vout;

    // Local -> world
    float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
    vout.PosW = posW.xyz;
    vout.PosH = mul(posW, gViewProj);
    vout.ViewDepth = mul(posW, gView).z;

    // Transform normal
    vout.NormalW = mul(vin.NormalL, (float3x3)gWorld);

    // Transform tangent
    vout.TangentW = mul(vin.TangentU, (float3x3)gWorld);
   
    // Transform UV
    float4 texC = mul(float4(vin.TexC, 0.0f, 1.0f), gTexTransform);
    vout.TexC = mul(float4(vin.TexC, 0.0f, 1.0f), gTexTransform).xy;

    return vout;
}

// Outline vertex shader
VertexOut VS_Outline(VertexIn vin)
{
    VertexOut vout = (VertexOut)0.0f;
    float outlineWidth = gOutlineThickness; 
    
    float3 pos = vin.PosL + (vin.NormalL * outlineWidth);

    float4 posW = mul(float4(pos, 1.0f), gWorld);
    vout.PosH = mul(posW, gViewProj);
    vout.TexC = vin.TexC;

    return vout;
}

// Outline pixel shader
float4 PS_Outline(VertexOut pin) : SV_Target
{
   return gOutlineColor;
}

// ---------------------------------------------------------------------------------------
// Pixel Shader
// ---------------------------------------------------------------------------------------

float CalcShadowFactor(float4 shadowPosH)
{
    // Perspective divide
    shadowPosH.xyz /= shadowPosH.w;

    // Shadow depth
    float depth = shadowPosH.z;

    // Texel size
    uint width, height, numMips;
    gShadowMap.GetDimensions(0, width, height, numMips);
    float dx = 1.0f / (float)width;

    // 3x3 PCF
    float percentLit = 0.0f;
    const float2 offsets[9] = {
        float2(-dx,  -dx), float2(0.0f,  -dx), float2(dx,  -dx),
        float2(-dx, 0.0f), float2(0.0f, 0.0f), float2(dx, 0.0f),
        float2(-dx,  dx), float2(0.0f,  dx), float2(dx,  dx)
    };

    [unroll] 
    for(int i = 0; i < 9; ++i)
    {
        // Hardware depth comparison
        percentLit += gShadowMap.SampleCmpLevelZero(gsamShadow,
            shadowPosH.xy + offsets[i], depth).r;
    }

    // Average result
    return percentLit / 9.0f;
}

float SoftenDirectionalShadow(float shadowFactor)
{
    // Keep the cast shadow readable, but prevent it from crushing dark stage areas.
    const float minDirectionalShadowLight = 0.42f;
    return lerp(minDirectionalShadowLight, 1.0f, saturate(shadowFactor));
}

float4 PS(VertexOut pin) : SV_Target
{
    float4 texDiffuse = gDiffuseAlbedo;
    float4 textureSample = float4(1.0f, 1.0f, 1.0f, 1.0f);
    if (gDiffuseMapIndex >= 0)
    {
        textureSample = gTextureMaps[gDiffuseMapIndex].Sample(gsamAnisotropicWrap, pin.TexC);
        texDiffuse *= textureSample;
    }

    texDiffuse *= gColorMultiplier;
    if (gIsTransparent == 3)
    {
        float colorCoverage = max(textureSample.r, max(textureSample.g, textureSample.b));
        float coverage = textureSample.a;
        float magentaKey = smoothstep(0.28f, 0.62f, min(textureSample.r, textureSample.b));
        magentaKey *= 1.0f - smoothstep(0.12f, 0.44f, textureSample.g);
        magentaKey *= 1.0f - smoothstep(0.18f, 0.55f, abs(textureSample.r - textureSample.b));

        // Some decal textures ship with a solid alpha channel and a black RGB background.
        // In that case treat brightness as coverage so the black plate gets clipped away.
        if (coverage >= 0.999f)
        {
            coverage = colorCoverage;
        }
        else
        {
            coverage *= colorCoverage > 0.0f ? 1.0f : 0.0f;
        }

        coverage = saturate(coverage - magentaKey * 0.95f);
        clip(coverage - 0.06f);

        float neutral = saturate(colorCoverage * 1.12f);
        float3 neutralTint = float3(neutral, neutral, neutral);
        texDiffuse.rgb = lerp(texDiffuse.rgb, neutralTint, magentaKey);
        texDiffuse.a = 1.0f;
    }
    else if (gIsTransparent != 0)
    {
        if (gDiffuseMapIndex >= 0 && gDiffuseAlbedo.g > 0.9f && gDiffuseAlbedo.r < 0.6f)
        {
            float colorKeyAlpha = smoothstep(0.025f, 0.16f, max(texDiffuse.r, max(texDiffuse.g, texDiffuse.b)));
            texDiffuse.a *= colorKeyAlpha;
        }
        return texDiffuse; 
    }

    // Normalize basis vectors
    pin.NormalW = normalize(pin.NormalW);
    pin.TangentW = normalize(pin.TangentW); 

    pin.TangentW = normalize(pin.TangentW - dot(pin.TangentW, pin.NormalW) * pin.NormalW);
    float3 bitangentW = cross(pin.NormalW, pin.TangentW);
    float3x3 TBN = float3x3(pin.TangentW, bitangentW, pin.NormalW);

    if (gNormalMapIndex >= 0)
    {
        float3 normalMapSample = gTextureMaps[gNormalMapIndex].Sample(gsamAnisotropicWrap, pin.TexC).rgb;
        float3 bumpedNormalW = 2.0f * normalMapSample - 1.0f;
        pin.NormalW = mul(bumpedNormalW, TBN);
    }
    
    float metallic = saturate(gMetallicFactor);
    if (gMetallicMapIndex >= 0)
    {
        metallic *= gTextureMaps[gMetallicMapIndex].Sample(gsamAnisotropicWrap, pin.TexC).r;
    }

    // Fresnel base reflectance
    float3 dielectricF0 = max(gFresnelR0, float3(0.02f, 0.02f, 0.02f));
    float3 fresnelR0 = lerp(dielectricF0, saturate(texDiffuse.rgb), metallic);

    // Shadow factor
    float4 shadowPosH = mul(float4(pin.PosW, 1.0f), gShadowTransform);
    float shadowFactor = SoftenDirectionalShadow(CalcShadowFactor(shadowPosH));

    // View and ambient
    float3 toEyeW = normalize(gEyePosW - pin.PosW);
    float3 ambient = gAmbientLight.rgb * texDiffuse.rgb;
    
    // Material setup
    Material mat = { texDiffuse, fresnelR0, gRoughness, metallic, gIsToon };
    
    float3 directLight = 0.0f;

    // Directional lights
    for(int i = 0; i < 1; ++i)
    {
        directLight += ComputeDirectionalLight(gLights[i], mat, pin.NormalW, toEyeW) * shadowFactor;
    }

    for(int j = 1; j < MAX_LIGHTS; ++j)
    {
        if (dot(gLights[j].Direction, gLights[j].Direction) > 0.0001f)
        {
            directLight += ComputeSpotLight(gLights[j], mat, pin.PosW, pin.NormalW, toEyeW);
        }
        else
        {
            directLight += ComputePointLight(gLights[j], mat, pin.PosW, pin.NormalW, toEyeW);
        }
    }

    float3 emissiveColor = 0.0f;
    if (gEmissiveMapIndex >= 0)
    {
        float emissiveStrength = (gIsTransparent == 1) ? 1.8f : 1.0f;
        emissiveColor = gTextureMaps[gEmissiveMapIndex].Sample(gsamAnisotropicWrap, pin.TexC).rgb * gDiffuseAlbedo.rgb * emissiveStrength;
    }

    float3 finalColor = ambient + directLight + emissiveColor;
    if (gFogPad.x < 0.5f)
    {
        float fogDepth = abs(pin.ViewDepth);
        float fogAmount = saturate((fogDepth - gFogStart) / max(gFogRange, 0.001f));
        float heightFogAmount = saturate((gHeightFogTop - pin.PosW.y) / max(gHeightFogRange, 0.001f));
        fogAmount = saturate(max(fogAmount, heightFogAmount * gHeightFogStrength));
        finalColor = lerp(finalColor, gFogColor.rgb, fogAmount);
    }

    return float4(finalColor, texDiffuse.a);
}
