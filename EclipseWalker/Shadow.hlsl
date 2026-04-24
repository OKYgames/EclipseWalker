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
    int gPadding; 
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
    float4 diffuse = gTextureMaps[gDiffuseMapIndex].Sample(gsamAnisotropicWrap, pin.TexC) * gDiffuseAlbedo;
    clip(diffuse.a - 0.1f);
}
