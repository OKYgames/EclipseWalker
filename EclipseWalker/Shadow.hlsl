#include "Light.hlsl" 

// -----------------------------------------------------------------------
// 상수 버퍼 
// -----------------------------------------------------------------------
cbuffer cbPerObject : register(b0)
{
    float4x4 gWorld;       
    float4x4 gTexTransform;
    float4 gDiffuseAlbedo; 
    float3 gFresnelR0;
    float  gRoughness;
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

Texture2D gDiffuseMap[10] : register(t0);
SamplerState gsamLinear   : register(s2); // 또는 s4 (Anisotropic)

// -----------------------------------------------------------------------
// 입출력 구조체
// -----------------------------------------------------------------------
struct VertexIn
{
    float3 PosL : POSITION;
    float2 TexC : TEXCOORD;
};

struct VertexOut
{
    float4 PosH : SV_POSITION; // 화면(조명 시점) 좌표
    float2 TexC : TEXCOORD;
};

// -----------------------------------------------------------------------
// Vertex Shader (정점 쉐이더)
// -----------------------------------------------------------------------
VertexOut VS(VertexIn vin)
{
    VertexOut vout;

    // 1. 로컬 -> 월드 변환
    float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);

    // 2. 월드 -> 조명 클립 공간 변환 (gViewProj가 조명 기준임)
    vout.PosH = mul(posW, gViewProj);
    
    // 3. 텍스처 좌표 전달 (구멍 뚫기용)
    float4 texC = mul(float4(vin.TexC, 0.0f, 1.0f), gTexTransform);
    vout.TexC = mul(float4(vin.TexC, 0.0f, 1.0f), gTexTransform).xy;

    return vout;
}

// -----------------------------------------------------------------------
// Pixel Shader (픽셀 쉐이더)
// -----------------------------------------------------------------------
void PS(VertexOut pin)
{
    // 1. 텍스처 색상 읽기
    float4 diffuse = gDiffuseMap[0].Sample(gsamLinear, pin.TexC) * gDiffuseAlbedo;

    // 2. 알파 클리핑
    clip(diffuse.a - 0.1f);
}