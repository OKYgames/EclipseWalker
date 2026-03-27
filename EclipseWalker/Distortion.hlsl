#ifndef MAX_LIGHTS
    #define MAX_LIGHTS 16 
#endif

#include "Light.hlsl"

cbuffer cbPerObject : register(b0)
{
    float4x4 gWorld;
    float4x4 gTexTransform;
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

struct VertexIn { float3 PosL : POSITION; float3 NormalL : NORMAL; float2 TexC : TEXCOORD; };
struct VertexOut { float4 PosH : SV_POSITION; float2 TexC : TEXCOORD; };

VertexOut VS(VertexIn vin)
{
    VertexOut vout = (VertexOut)0.0f;
   
    vout.PosH = mul(float4(vin.PosL, 1.0f), gWorld); 
    vout.TexC = mul(float4(vin.TexC, 0.0f, 1.0f), gTexTransform).xy;
    return vout;
}


float hash(float2 p) {
    // 임의의 소수점을 곱해서 예측 불가능한 노이즈 값을 만듭니다.
    return frac(sin(dot(p, float2(12.9898f, 78.233f))) * 43758.5453f);
}


float noise(float2 p) {
    float2 i = floor(p);
    float2 f = frac(p);
    // 곡선을 부드럽게 깎아주는 공식 (Hermite Curve)
    float2 u = f * f * (3.0f - 2.0f * f);

    // 네 모서리의 난수값을 구해서 부드럽게 섞습니다.
    float a = hash(i + float2(0.0f, 0.0f));
    float b = hash(i + float2(1.0f, 0.0f));
    float c = hash(i + float2(0.0f, 1.0f));
    float d = hash(i + float2(1.0f, 1.0f));

    return lerp(lerp(a, b, u.x), lerp(c, d, u.x), u.y);
}

float fbm(float2 p) {
    float f = 0.0f;
    float amp = 0.5f;
    // 4겹의 노이즈를 크기를 줄여가며 쌓아 올립니다.
    for (int i = 0; i < 4; i++) {
        f += amp * noise(p);
        p *= 2.0f;
        amp *= 0.5f;
    }
    return f;
}


float4 PS(VertexOut pin) : SV_Target
{
    float2 uv = pin.TexC;
    float2 center = float2(0.5f, 0.5f);
    float2 dir = uv - center;
    float dist = length(dir);
    
    float t = gTotalTime * 1.5f;

    // 1. 블랙홀 소용돌이 왜곡
    float angle = atan2(dir.y, dir.x);
    angle += t - (dist * 6.0f); 
    float2 swirlUV = float2(cos(angle), sin(angle)) * dist;
    swirlUV -= normalize(dir) * (t * 0.2f);

    // 2. fBM 노이즈 생성
    float n = fbm(swirlUV * 10.0f + float2(t * 0.5f, t * 0.5f));
    float haze = saturate(n * 2.5f);
    float vignette = smoothstep(0.8f, 0.0f, dist);
    haze *= vignette;

    // ========================================================
    // 디졸브(Dissolve) 연출 - 불타는 경계선
    // ========================================================
    float progress = gDiffuseAlbedo.a; // C++에서 넘겨준 진행도 (1.0 -> 0.0)
    float threshold = 1.0f - progress; // 침식 임계값 (0.0 -> 1.0)
    
    float finalAlpha = 1.0f;
    float3 edgeGlow = float3(0.0f, 0.0f, 0.0f);

    // 진행도가 0.99보다 작을 때(즉, 1초 대기시간이 끝나고 사라지기 시작할 때) 디졸브
    if (progress < 0.99f) 
    {
        // 1. 노이즈(haze) 결을 따라 구멍 뚫기
        finalAlpha = smoothstep(threshold - 0.05f, threshold + 0.05f, haze);

        // 2. 구멍이 뚫리는 경계선 부분만 추출
        float edgeWidth = 0.15f; 
        float edge = smoothstep(threshold, threshold + edgeWidth, haze) 
                   - smoothstep(threshold + edgeWidth, threshold + edgeWidth * 2.0f, haze);
        
        edgeGlow = gDiffuseAlbedo.rgb * 5.0f * edge;
    }

    float3 baseColor = gDiffuseAlbedo.rgb;
    float3 finalColor = baseColor * (0.5f + haze * 2.0f);
    
    finalColor += edgeGlow;

    return float4(finalColor, finalAlpha);
}