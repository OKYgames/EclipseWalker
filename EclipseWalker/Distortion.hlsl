#ifndef MAX_LIGHTS
    #define MAX_LIGHTS 16 
#endif

#include "Light.hlsl"

// b0: 오브젝트 상수 버퍼 (위치, 스케일 등)
cbuffer cbPerObject : register(b0) { float4x4 gWorld; float4x4 gTexTransform; };

// b1: 패스 상수 버퍼 (플레이어 위치, 영역 반경 등)
cbuffer cbPass : register(b1) 
{ 
    float4x4 gView; float4x4 gInvView; float4x4 gProj; float4x4 gInvProj; 
    float4x4 gViewProj; float4x4 gInvViewProj; 
    float3 gEyePosW; float cbPerObjectPad1; 
    float2 gRenderTargetSize; float2 gInvRenderTargetSize; 
    float gNearZ; float gFarZ; float gTotalTime; float gDeltaTime; 
    float4 gAmbientLight; Light gLights[MAX_LIGHTS]; 
    
    // 플레이어가 넘겨주는 영역 데이터
    float3 gDomainCenter; 
    float  gDomainRadius;
    int    gIsDomainActive;
    float3 gDomainPad;
};

// 재질 데이터 (진행도 제어용)
cbuffer cbMaterial : register(b2) { float4 gDiffuseAlbedo; float3 gFresnelR0; float gRoughness; float4x4 gMatTransform; };

struct VertexIn { float3 PosL : POSITION; float3 NormalL : NORMAL; float2 TexC : TEXCOORD; };
struct VertexOut { float4 PosH : SV_POSITION; float3 PosW : POSITION; float2 TexC : TEXCOORD; };

// 1. 필수 노이즈 함수들
float hash(float2 p) {
    return frac(sin(dot(p, float2(12.9898, 78.233))) * 43758.5453);
}

float noise(float2 p) {
    float2 i = floor(p);
    float2 f = frac(p);
    float2 u = f * f * (3.0 - 2.0 * f);
    return lerp(lerp(hash(i + float2(0.0, 0.0)), hash(i + float2(1.0, 0.0)), u.x),
                lerp(hash(i + float2(0.0, 1.0)), hash(i + float2(1.0, 1.0)), u.x), u.y);
}

// 2. fbm 함수 (PS에서 호출하기 전에 정의)
float fbm(float2 p) {
    float v = 0.0;
    float a = 0.5;
    float2 shift = float2(100.0, 100.0);
    for (int i = 0; i < 4; ++i) {
        v += a * noise(p);
        p = p * 2.0 + shift;
        a *= 0.5;
    }
    return v;
}

VertexOut VS(VertexIn vin)
{
    VertexOut vout = (VertexOut)0.0f;
    
    // 3D 공간의 월드 좌표 계산
    float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
    vout.PosW = posW.xyz;
    vout.PosH = mul(posW, gViewProj); // 투영 변환
    vout.TexC = vin.TexC;
    
    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
    if (gIsDomainActive == 0) discard; // 영역이 꺼져있으면 그리지 않음

    // 픽셀에서 영역 중심까지의 실제 3D 거리 계산
    float3 distVec = pin.PosW - gDomainCenter;
    float currentDist = length(distVec);
    
    // 셰이더 내부 소용돌이 애니메이션을 위한 UV 계산
    float2 sphereUV = pin.TexC;
    float t = gTotalTime * 1.5f;

    // 영역의 경계면(표면) 근처만 시각화
    // currentDist가 gDomainRadius 근처일 때만 출력 (두께 0.5f 정도의 막)
    float edgeMask = smoothstep(gDomainRadius + 0.5f, gDomainRadius, currentDist) 
                   - smoothstep(gDomainRadius, gDomainRadius - 0.5f, currentDist);
    
    if (edgeMask <= 0.0f) discard; // 구체의 표면이 아니면 픽셀을 버림

    // 경계면 소용돌이 이펙트 (FBM 노이즈 활용)
    float2 warpUV = sphereUV * 5.0f;
    warpUV.x += t * 0.2f;
    warpUV.y += sin(t * 0.5f) * 0.1f;
    
    float noiseVal = fbm(warpUV);
    
    // 보라색과 금색의 혼합 (이면 세계 느낌)
    float3 color1 = float3(1.0f, 0.7f, 0.2f); // 경계선 발광 (금색)
    float3 color2 = float3(0.4f, 0.05f, 0.6f); // 이면 기운 (보라색)
    float3 finalEffectColor = lerp(color2, color1, noiseVal);

    // 구체 표면의 투명도 조절
    float alpha = edgeMask * (0.3f + noiseVal * 0.7f);

    return float4(finalEffectColor * 2.0f, alpha); // 밝게 발광 출력
}