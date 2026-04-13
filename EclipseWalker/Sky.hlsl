#include "Light.hlsl"

TextureCube gCubeMap : register(t0);
SamplerState gsamAnisotropic : register(s4);

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
};


cbuffer cbPerObject : register(b0)
{
    float4x4 gWorld;
};

struct VertexIn
{
    float3 PosL : POSITION;
    float3 NormalL : NORMAL;
    float2 TexC : TEXCOORD;
};

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float3 PosL : POSITION;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout;
    vout.PosL = vin.PosL;

    float3 fixedScalePos = vin.PosL * 5000.0f; 
    float4 posW = float4(fixedScalePos, 1.0f);

    vout.PosH = mul(posW, gViewProj);

    
    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{

    float4 texColor = gCubeMap.Sample(gsamAnisotropic, pin.PosL);
    return texColor * gSkyTint;
}
