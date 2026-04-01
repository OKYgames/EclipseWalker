#ifndef MAX_LIGHTS
    #define MAX_LIGHTS 16 
#endif

#include "Light.hlsl"

cbuffer cbPerObject : register(b0) { float4x4 gWorld; float4x4 gTexTransform; };
cbuffer cbPass : register(b1) { float4x4 gView; float4x4 gInvView; float4x4 gProj; float4x4 gInvProj; float4x4 gViewProj; float4x4 gInvViewProj; float3 gEyePosW; float cbPerObjectPad1; float2 gRenderTargetSize; float2 gInvRenderTargetSize; float gNearZ; float gFarZ; float gTotalTime; float gDeltaTime; float4 gAmbientLight; Light gLights[MAX_LIGHTS]; };
cbuffer cbMaterial : register(b2) { float4 gDiffuseAlbedo; float3 gFresnelR0; float gRoughness; float4x4 gMatTransform; };

Texture2D gTextureMaps[1000] : register(t0); 
SamplerState gsamAnisotropicWrap : register(s4); 

// 필수 노이즈 함수
float hash(float2 p) { return frac(sin(dot(p, float2(12.9898f, 78.233f))) * 43758.5453f); }
float noise(float2 p) { float2 i = floor(p); float2 f = frac(p); float2 u = f * f * (3.0f - 2.0f * f);
    return lerp(lerp(hash(i + float2(0.0f, 0.0f)), hash(i + float2(1.0f, 0.0f)), u.x), 
                lerp(hash(i + float2(0.0f, 1.0f)), hash(i + float2(1.0f, 1.0f)), u.x), u.y); }
float fbm(float2 p) { float f = 0.0f; float amp = 0.5f; 
    for (int i = 0; i < 4; i++) { f += amp * noise(p); p *= 2.0f; amp *= 0.5f; } return f; }


struct VertexIn { float3 PosL : POSITION; float3 NormalL : NORMAL; float2 TexC : TEXCOORD; };
struct VertexOut { float4 PosH : SV_POSITION; float2 TexC : TEXCOORD; float LocalZ : TEXCOORD1; };

VertexOut VS(VertexIn vin)
{
    VertexOut vout = (VertexOut)0.0f;
    float3 posL = vin.PosL;
    
    // 화면 중앙(0,0)으로부터의 거리
    float dist = length(posL.xy);

    // saturate(1.0f - dist)는 중앙에서 1, 외곽에서 0
    float suction = saturate(1.0f - dist);
    
    // 버텍스의 Z(깊이)를 물리적으로 조작! 중앙을 깔때기 모양으로 푹 팝니다.
    // 0.8f는 파이는 깊이 조절
    posL.z += suction * 0.8f; 

    vout.PosH = mul(float4(posL, 1.0f), gWorld);
    vout.TexC = mul(float4(vin.TexC, 0.0f, 1.0f), gTexTransform).xy;
    
    // 조작된 최종 Z축 위치를 넘김
    vout.LocalZ = posL.z; 
    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
    float progress = gDiffuseAlbedo.a; // C++에서 넘어오는 진행도 (1.0 -> 0.0)

    float2 uv = pin.TexC;
    float2 center = float2(0.5f, 0.5f);
    float2 dir = uv - center;
    float dist = length(dir);
    float t = gTotalTime * 2.0f;

    float theta = -t * 2.0f + (1.0f / (dist + 0.1f)) * 0.2f; 
    float cosT = cos(theta);
    float sinT = sin(theta);
    float2 rotatedDir = float2(dir.x * cosT - dir.y * sinT, dir.x * sinT + dir.y * cosT);
    
    float2 warpUV = rotatedDir * 12.0f; 
    warpUV -= float2(t, t); 
    
    float n1 = fbm(warpUV);
    float n2 = fbm(warpUV + float2(1.2f, 3.4f));

    float physicalShadow = 1.0f - saturate(pin.LocalZ * 1.5f);
    float haze = saturate(n1 * 2.5f) * physicalShadow;
    
    float3 color1 = float3(1.0f, 0.5f, 0.0f); // 랜턴 금빛
    float3 color2 = float3(0.3f, 0.0f, 0.4f); // 이면세계 보라빛
    float3 nebulaColor = lerp(color1, color2, n2);

    float coreGlow = smoothstep(0.15f, 0.0f, dist) * 2.5f;
    float3 coreColor = float3(1.0f, 0.8f, 0.4f) * coreGlow; 

    // 순수 이펙트 색상
    float3 effectColor = nebulaColor * (0.2f + haze * 3.0f) + coreColor;

    float finalAlpha = 1.0f;
    float3 finalColor = float3(0, 0, 0);
    float3 edgeGlow = float3(0, 0, 0);

    if (progress > 0.5f) 
    {
        finalAlpha = 1.0f; // 화면 전체를 완전히 가림
        
        // 1.0 -> 0.5 수치를 0.0 -> 1.0 비율로 변환
        float fillPhase = (1.0f - progress) * 2.0f; 
        float radius = fillPhase * 1.2f; // 안에서 밖으로 커지는 반경
        
        float effectMask = smoothstep(radius + 0.05f, radius - 0.05f, dist);
        
        float edge = smoothstep(radius - 0.05f, radius, dist) 
                   - smoothstep(radius, radius + 0.05f, dist);
        edgeGlow = color1 * 15.0f * edge;

        float3 pitchBlack = float3(0.01f, 0.0f, 0.02f); // 칠흑 같은 암전
        
        // 암전 위로 이펙트가 덮어씌워짐
        finalColor = lerp(pitchBlack, effectColor, effectMask) + edgeGlow;
    }
    else 
    {
        // 0.5 -> 0.0 수치를 1.0 -> 0.0 비율로 변환
        float dissolvePhase = progress * 2.0f; 
        
        // 반경이 1.2(가득 참)에서 0.0(완전 수축)으로 줄어듦
        float currentRadius = dissolvePhase * 1.2f - 0.1f; 
        
        // 노이즈를 섞어서 원형이 아닌 거칠게 타들어가는 단면 생성
        float dissolveMask = dist + (1.0f - haze) * 0.2f; 

        finalAlpha = smoothstep(currentRadius + 0.1f, currentRadius - 0.1f, dissolveMask);
        
        // 타들어가는 불꽃 테두리
        float edge = smoothstep(currentRadius - 0.05f, currentRadius, dissolveMask) 
                   - smoothstep(currentRadius, currentRadius + 0.1f, dissolveMask);
        edgeGlow = color1 * 15.0f * edge;

        finalColor = effectColor + edgeGlow;
    }

    // 시네마틱 비네팅
    float vignette = smoothstep(1.0f, 0.2f, dist);
    finalColor *= vignette;

    return float4(finalColor, finalAlpha);
}