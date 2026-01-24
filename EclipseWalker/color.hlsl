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


Texture2D gDiffuseMap[10] : register(t0);
Texture2D gNormalMap[10]  : register(t10);
//SamplerState gsamLinear : register(s2); // Linear Sampler
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
float4 PS(VertexOut pin) : SV_Target
{
    // 1. 텍스처 색상 추출
    float4 diffuseAlbedo = gDiffuseMap[0].Sample(gsamAnisotropicWrap, pin.TexC) * gDiffuseAlbedo;
    
    // 2. 벡터 정규화 및 TBN 행렬 생성 
    pin.NormalW = normalize(pin.NormalW);
    pin.TangentW = normalize(pin.TangentW); 

    // [TBN 행렬 만들기]
    // 1) 그람-슈미트(Gram-Schmidt) 과정: 접선과 법선이 정확히 90도가 되도록 교정
    pin.TangentW = normalize(pin.TangentW - dot(pin.TangentW, pin.NormalW) * pin.NormalW);
    
    // 2) 종선(Bitangent) 계산: 법선(N)과 접선(T)을 외적하면 나옴
    float3 bitangentW = cross(pin.NormalW, pin.TangentW);

    // 3) 접선 공간 행렬(TBN) 완성
    float3x3 TBN = float3x3(pin.TangentW, bitangentW, pin.NormalW);

    // -----------------------------------------------------------------------
    // [노멀 매핑 적용 구간] 
    // -----------------------------------------------------------------------  
    float3 normalMapSample = gNormalMap[0].Sample(gsamAnisotropicWrap, pin.TexC).rgb;
    float3 bumpedNormalW = 2.0f * normalMapSample - 1.0f; // [0,1] -> [-1,1]
    pin.NormalW = mul(bumpedNormalW, TBN); // 법선 교체 (TBN 공간 -> 월드 공간)
    
    // 3. 조명 계산 준비
    float3 toEyeW = normalize(gEyePosW - pin.PosW);
    float3 ambient = gAmbientLight.rgb * diffuseAlbedo.rgb;

    Material mat = { diffuseAlbedo, gFresnelR0, gRoughness };
    float3 directLight = 0.0f;

    // 4. 조명 계산 (바뀐 pin.NormalW를 사용하게 됨)
    for(int i = 0; i < 3; ++i)
    {
        directLight += ComputeDirectionalLight(gLights[i], mat, pin.NormalW, toEyeW);
    }

    for(int j = 3; j < MAX_LIGHTS; ++j)
    {
        directLight += ComputePointLight(gLights[j], mat, pin.PosW, pin.NormalW, toEyeW);
    }

    float3 finalColor = ambient + directLight;

    return float4(finalColor, diffuseAlbedo.a);
}