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


Texture2D gDiffuseMap : register(t0);
SamplerState gsamLinear : register(s2); // Linear Sampler


struct VertexIn
{
    float3 PosL    : POSITION;
    float3 NormalL : NORMAL;
    float2 TexC    : TEXCOORD;
};

struct VertexOut
{
    float4 PosH    : SV_POSITION; // 화면 좌표 (Homogeneous Clip Space)
    float3 PosW    : POSITION;    // 월드 좌표 (조명 계산용)
    float3 NormalW : NORMAL;      // 월드 법선 (조명 계산용)
    float2 TexC    : TEXCOORD;
};

// ---------------------------------------------------------------------------------------
// Vertex Shader
// ---------------------------------------------------------------------------------------
VertexOut VS(VertexIn vin)
{
    VertexOut vout;

    // 1. 정점을 월드 공간으로 변환
    float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
    vout.PosW = posW.xyz;

    // 2. 법선(Normal)을 월드 공간으로 변환
    // (스케일링이 비균등할 경우 역전치 행렬을 써야 하지만, 균등 스케일이라 가정하고 gWorld 사용)
    vout.NormalW = mul(vin.NormalL, (float3x3)gWorld);

    // 3. 화면 공간으로 변환 (Projetion)
    vout.PosH = mul(posW, gViewProj);
    
    // 4. UV 좌표 전달
    vout.TexC = vin.TexC;

    return vout;
}

// ---------------------------------------------------------------------------------------
// Pixel Shader
// ---------------------------------------------------------------------------------------
float4 PS(VertexOut pin) : SV_Target
{
    // 1. 텍스처 색상 추출
    float4 diffuseAlbedo = gDiffuseMap.Sample(gsamLinear, pin.TexC) * gDiffuseAlbedo;
    
    // 알파 클리핑: 투명도가 낮으면 픽셀 버림
    // if((diffuseAlbedo.a - 0.1f) < 0.0f) discard;

    // 2. 벡터 정규화 (보간 과정에서 길이가 변할 수 있음)
    pin.NormalW = normalize(pin.NormalW);
    
    // 카메라를 향하는 벡터
    float3 toEyeW = normalize(gEyePosW - pin.PosW);

    // 3. 환경광(Ambient) 계산
    float3 ambient = gAmbientLight.rgb * diffuseAlbedo.rgb;

    // 4. 직접광(Direct Light) 합산
    Material mat = { diffuseAlbedo, gFresnelR0, gRoughness };
    float3 directLight = 0.0f;

    // 0번~2번 조명은 Directional Light (태양 등)라고 가정
   for(int i = 0; i < 3; ++i)
    {
        directLight += ComputeDirectionalLight(gLights[i], mat, pin.NormalW, toEyeW);
    }

    // 3번~ 나머지 조명들
    for(int j = 3; j < MAX_LIGHTS; ++j)
    {
        directLight += ComputePointLight(gLights[j], mat, pin.PosW, pin.NormalW, toEyeW);
    }

    // 5. 최종 색상 결정
    float3 finalColor = ambient + directLight;

    return float4(finalColor, diffuseAlbedo.a);
}