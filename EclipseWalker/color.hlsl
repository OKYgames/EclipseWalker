#include "Light.hlsl"

cbuffer cbPerObject : register(b0)
{
    float4x4 gWorld;

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

cbuffer cbMaterial : register(b2)
{
    float4 gDiffuseAlbedo;
    float3 gFresnelR0;
    float  gRoughness;
    int    gIsToon;        
    float  gOutlineThickness;
    float2 gPad;            
    float4 gOutlineColor;
};

Texture2D gDiffuseMap  : register(t0);
Texture2D gNormalMap   : register(t1);
Texture2D gEmissiveMap : register(t2);
Texture2D gMetallicMap : register(t3);

Texture2D gShadowMap   : register(t4);

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

// 외곽선용 버텍스 쉐이더 
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

// 외곽선용 픽셀 쉐이더
float4 PS_Outline(VertexOut pin) : SV_Target
{
   return gOutlineColor;
}

// ---------------------------------------------------------------------------------------
// Pixel Shader
// ---------------------------------------------------------------------------------------

float CalcShadowFactor(float4 shadowPosH)
{
    // 1. 투영 좌표 정규화
    shadowPosH.xyz /= shadowPosH.w;

    // 2. 깊이 값 (조명 기준)
    float depth = shadowPosH.z;

    // 3. 텍스처 크기 가져오기 (dx = 텍셀 하나의 크기)
    uint width, height, numMips;
    gShadowMap.GetDimensions(0, width, height, numMips);
    float dx = 1.0f / (float)width;

    // 4. 주변 9개 픽셀(3x3)을 검사해서 평균 내기 (PCF)
    float percentLit = 0.0f;
    const float2 offsets[9] = {
        float2(-dx,  -dx), float2(0.0f,  -dx), float2(dx,  -dx),
        float2(-dx, 0.0f), float2(0.0f, 0.0f), float2(dx, 0.0f),
        float2(-dx,  dx), float2(0.0f,  dx), float2(dx,  dx)
    };

    [unroll] 
    for(int i = 0; i < 9; ++i)
    {
        // SampleCmpLevelZero는 하드웨어에서 비교 + 선형 보간
        percentLit += gShadowMap.SampleCmpLevelZero(gsamShadow,
            shadowPosH.xy + offsets[i], depth).r;
    }

    // 9로 나누어 평균값 리턴 
    return percentLit / 9.0f;
}

float4 PS(VertexOut pin) : SV_Target
{
    // 텍스처 색상 추출
    float4 texDiffuse = gDiffuseMap.Sample(gsamAnisotropicWrap, pin.TexC) * gDiffuseAlbedo;
    clip(texDiffuse.a - 0.1f);
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
    float3 fresnelR0 = lerp(f0, texDiffuse.rgb, metallic);

    // 그림자 계산 준비
    float4 shadowPosH = mul(float4(pin.PosW, 1.0f), gShadowTransform);
    float shadowFactor = CalcShadowFactor(shadowPosH);

    // 조명 계산 준비
    float3 toEyeW = normalize(gEyePosW - pin.PosW);
    float3 ambient = gAmbientLight.rgb * texDiffuse.rgb;
    Material mat = { texDiffuse, gFresnelR0, gRoughness, gIsToon };
    
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

    return float4(finalColor, texDiffuse.a);
    //return float4(normalMapSample, 1.0f);
}