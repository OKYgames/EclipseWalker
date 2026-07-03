#include "Light.hlsl" 

// Shadow pass constants
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
};

cbuffer cbMaterial : register(b2)
{
    float4 gDiffuseAlbedo; float3 gFresnelR0; float gRoughness;
    float4 gOutlineColor; float gOutlineThickness;
    int gIsToon; int gIsTransparent;
    int gDiffuseMapIndex; int gNormalMapIndex; int gEmissiveMapIndex; int gMetallicMapIndex;
    float gMetallicFactor;
};

Texture2D gTextureMaps[1000] : register(t0);
SamplerState gsamAnisotropicWrap : register(s4);

// Input / output structures
struct VertexIn
{
    float3 PosL : POSITION;
    float2 TexC : TEXCOORD;
};

struct VertexOut
{
    float4 PosH : SV_POSITION; // Clip-space position
    float2 TexC : TEXCOORD;
};

// Vertex Shader
VertexOut VS(VertexIn vin)
{
    VertexOut vout;

    // Local -> world
    float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);

    // World -> clip
    vout.PosH = mul(posW, gViewProj);
    
    // UV transform
    float4 texC = mul(float4(vin.TexC, 0.0f, 1.0f), gTexTransform);
    vout.TexC = mul(float4(vin.TexC, 0.0f, 1.0f), gTexTransform).xy;

    return vout;
}

// Pixel Shader
void PS(VertexOut pin)
{
    float4 textureSample = gTextureMaps[gDiffuseMapIndex].Sample(gsamAnisotropicWrap, pin.TexC);
    float4 diffuse = textureSample * gDiffuseAlbedo;
    if (gIsTransparent == 3)
    {
        float colorCoverage = max(textureSample.r, max(textureSample.g, textureSample.b));
        float coverage = textureSample.a;
        float magentaKey = smoothstep(0.28f, 0.62f, min(textureSample.r, textureSample.b));
        magentaKey *= 1.0f - smoothstep(0.12f, 0.44f, textureSample.g);
        magentaKey *= 1.0f - smoothstep(0.18f, 0.55f, abs(textureSample.r - textureSample.b));

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
    }
    else
    {
        clip(diffuse.a - 0.1f);
    }
}
