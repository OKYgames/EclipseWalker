#include "Light.hlsl"

cbuffer cbPerObject : register(b0)
{
    float4x4 gWorld;       // 월드 행렬
    float4 gDiffuseAlbedo; // 재질 기본 색상
    float3 gFresnelR0;     // 프레넬 반사율
    float  gRoughness;     // 거칠기
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
    
    float4 gAmbientLight;        // 환경광
    Light gLights[MAX_LIGHTS];   // 조명 배열 (최대 16개)
};


Texture2D gDiffuseMap  : register(t0);
Texture2D gNormalMap   : register(t1);
Texture2D gEmissiveMap : register(t2);
Texture2D gMetallicMap : register(t3);

Texture2D gShadowMap   : register(t4);

SamplerState gsamShadow : register(s6);
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
    float4 PosH    : SV_POSITION; // 화면 좌표 (Homogeneous Clip Space)
    float3 PosW    : POSITION;    // 월드 좌표 (조명 계산용)
    float3 NormalW : NORMAL;      // 월드 법선 (조명 계산용)
    float3 TangentW : TANGENT;
    float2 TexC    : TEXCOORD;
};

// ---------------------------------------------------------------------------------------
// Vertex Shader
// ---------------------------------------------------------------------------------------
VertexOut VS(VertexIn vin)
{
    VertexOut vout;

    // 정점을 월드 공간으로 변환
    float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
    vout.PosW = posW.xyz;
    vout.PosH = mul(posW, gViewProj);

    // 법선(Normal)을 월드 공간으로 변환
    vout.NormalW = mul(vin.NormalL, (float3x3)gWorld);

    // 접선(Tangent)도 월드 공간으로 변환
    vout.TangentW = mul(vin.TangentU, (float3x3)gWorld);
   
    // UV 좌표 전달
    vout.TexC = vin.TexC;

    return vout;
}

// ---------------------------------------------------------------------------------------
// Pixel Shader
// ---------------------------------------------------------------------------------------

float CalcShadowFactor(float4 shadowPosH)
{
    // 1. 동차 좌표 나눗셈 (Perspective Divide)
    shadowPosH.xyz /= shadowPosH.w;

    // 2. 깊이값 (현재 픽셀의 깊이)
    float depth = shadowPosH.z;

    // 3. 그림자 맵 샘플링 
    float shadowMapDepth = gShadowMap.Sample(gsamShadow, shadowPosH.xy).r;

    float percent = 1.0f; 

    // 그림자 조건에 걸리면 0.0(어둠)으로 바꿉니다.
    if (depth - 0.001f > shadowMapDepth)
    {
        percent = 0.0f;
    }
    return percent;
}

float4 PS(VertexOut pin) : SV_Target
{
    // 텍스처 색상 추출
    float4 diffuseAlbedo = gDiffuseMap.Sample(gsamAnisotropicWrap, pin.TexC) * gDiffuseAlbedo;
    
    // 벡터 정규화 및 TBN 행렬 생성 
    pin.NormalW = normalize(pin.NormalW);
    pin.TangentW = normalize(pin.TangentW); 

    // [TBN 행렬 만들기]
    pin.TangentW = normalize(pin.TangentW - dot(pin.TangentW, pin.NormalW) * pin.NormalW);
    float3 bitangentW = cross(pin.NormalW, pin.TangentW);
    float3x3 TBN = float3x3(pin.TangentW, bitangentW, pin.NormalW);

    // [노멀 매핑 적용]
    float3 normalMapSample = gNormalMap.Sample(gsamAnisotropicWrap, pin.TexC).rgb;
    float3 bumpedNormalW = 2.0f * normalMapSample - 1.0f; 
    pin.NormalW = mul(bumpedNormalW, TBN); 
    
    // [금속 처리]
    float metallic = gMetallicMap.Sample(gsamAnisotropicWrap, pin.TexC).r;

    // 반사율(Fresnel) 결정
    float3 f0 = float3(0.04f, 0.04f, 0.04f); 
    float3 fresnelR0 = lerp(f0, diffuseAlbedo.rgb, metallic);

    // 그림자 계산 준비
    float4 shadowPosH = mul(float4(pin.PosW, 1.0f), gShadowTransform);
    float shadowFactor = CalcShadowFactor(shadowPosH);

    // 조명 계산 준비
    float3 toEyeW = normalize(gEyePosW - pin.PosW);
    float3 ambient = gAmbientLight.rgb * diffuseAlbedo.rgb;
    Material mat = { diffuseAlbedo, fresnelR0, gRoughness };
    
    float3 directLight = 0.0f;

    // 조명 계산
    for(int i = 0; i < 3; ++i)
    {
        directLight += ComputeDirectionalLight(gLights[i], mat, pin.NormalW, toEyeW) * shadowFactor;
    }

    for(int j = 3; j < MAX_LIGHTS; ++j)
    {
        directLight += ComputePointLight(gLights[j], mat, pin.PosW, pin.NormalW, toEyeW);
    }

    // 발광(Emissive) 및 최종 합산
    float3 emissiveColor = gEmissiveMap.Sample(gsamAnisotropicWrap, pin.TexC).rgb;

    float3 finalColor = ambient + directLight + emissiveColor;

    return float4(finalColor, diffuseAlbedo.a); 
    //return gNormalMap.Sample(gsamAnisotropicWrap, pin.TexC);
}