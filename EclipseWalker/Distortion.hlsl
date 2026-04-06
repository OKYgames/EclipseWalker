#ifndef MAX_LIGHTS
    #define MAX_LIGHTS 16 
#endif

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

    float3 gDomainCenter;   // 영역의 중심 (플레이어 위치)
    float  gDomainRadius;   // 현재 이펙트의 팽창 반지름
    int    gIsDomainActive; // 영역 활성화 여부
    float3 gDomainPad;      // 16바이트 정렬 패딩
};

cbuffer cbMaterial : register(b2)
{
    float4 gDiffuseAlbedo; 
    float3 gFresnelR0;
    float  gRoughness;
    float4x4 gMatTransform;
};

Texture2D gTextureMaps[1000] : register(t0); 
SamplerState gsamAnisotropicWrap : register(s4); 

struct VertexIn { 
    float3 PosL : POSITION; 
    float3 NormalL : NORMAL; 
    float2 TexC : TEXCOORD; 
};

struct VertexOut { 
    float4 PosH : SV_POSITION; 
    float3 PosW : POSITION; // [추가] 3D 거리 계산을 위한 월드 좌표
    float2 TexC : TEXCOORD; 
};

// -------------------------------------------------------------------------
// Vertex Shader
// -------------------------------------------------------------------------
VertexOut VS(VertexIn vin)
{
    VertexOut vout = (VertexOut)0.0f;
   
    // 월드 좌표 계산 (구체 형태를 깎아내기 위해 필수)
    float4 posW = mul(float4(vin.PosL, 1.0f), gWorld); 
    vout.PosW = posW.xyz;
    vout.PosH = mul(posW, gViewProj); 
    vout.TexC = mul(float4(vin.TexC, 0.0f, 1.0f), gTexTransform).xy;
    
    return vout;
}

// -------------------------------------------------------------------------
// 노이즈 함수 (기존 유지)
// -------------------------------------------------------------------------
float hash(float2 p) {
    return frac(sin(dot(p, float2(12.9898f, 78.233f))) * 43758.5453f);
}

float noise(float2 p) {
    float2 i = floor(p);
    float2 f = frac(p);
    float2 u = f * f * (3.0f - 2.0f * f);

    float a = hash(i + float2(0.0f, 0.0f));
    float b = hash(i + float2(1.0f, 0.0f));
    float c = hash(i + float2(0.0f, 1.0f));
    float d = hash(i + float2(1.0f, 1.0f));

    return lerp(lerp(a, b, u.x), lerp(c, d, u.x), u.y);
}

float fbm(float2 p) {
    float f = 0.0f;
    float amp = 0.5f;
    for (int i = 0; i < 4; i++) {
        f += amp * noise(p);
        p *= 2.0f;
        amp *= 0.5f;
    }
    return f;
}

// -------------------------------------------------------------------------
// Pixel Shader - 3D 구체막 이펙트 연출
// -------------------------------------------------------------------------
float4 PS(VertexOut pin) : SV_Target
{
    // 영역 전개가 꺼져있으면 아예 그리지 않음
    if (gIsDomainActive == 0) discard;

    // 1. 플레이어(중심)에서 현재 픽셀까지의 3D 월드 거리 계산
    float3 distVec = pin.PosW - gDomainCenter;
    float currentDist = length(distVec);

    // 2. 박스 메쉬의 모서리를 잘라내어 완벽한 "구(Sphere)" 형태로 만들기
    if (currentDist > gDomainRadius) discard;

    // 3. 구체의 표면(껍질) 두께 계산
    float edge = smoothstep(gDomainRadius * 0.9f, gDomainRadius, currentDist);

    // 안쪽 공간은 맵이 잘 보이도록 투명하게 파냅니다.
    if (edge <= 0.01f) discard; 

    // 4. 노이즈(에너지 흐름) 생성
    float t = gTotalTime * 1.5f;
    float n = fbm(pin.TexC * 4.0f - float2(t * 0.2f, t * 0.5f));

    float3 baseColor = gDiffuseAlbedo.rgb; 
  
    float3 glowColor = baseColor * 2.5f; 
    float3 finalColor = lerp(baseColor, glowColor, n);

    // 가장자리로 갈수록 + 노이즈가 강할수록 불투명해짐
    float alpha = edge * (0.1f + n * 0.7f);

    return float4(finalColor, alpha);
}