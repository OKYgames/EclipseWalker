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

// 텍스처 배열
Texture2D gTextureMaps[1000] : register(t0);
SamplerState gsamAnisotropicWrap : register(s4);

struct VertexIn
{
    float3 PosL : POSITION;
    float3 NormalL : NORMAL;
    float2 TexC : TEXCOORD;
};

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float3 PosW : POSITION; // 3D 거리 계산을 위한 월드 좌표
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
// 노이즈 함수들 (기존 유지)
// -------------------------------------------------------------------------
float hash(float2 p)
{
    return frac(sin(dot(p, float2(12.9898f, 78.233f))) * 43758.5453f);
}

float noise(float2 p)
{
    float2 i = floor(p);
    float2 f = frac(p);
    float2 u = f * f * (3.0f - 2.0f * f);

    float a = hash(i + float2(0.0f, 0.0f));
    float b = hash(i + float2(1.0f, 0.0f));
    float c = hash(i + float2(0.0f, 1.0f));
    float d = hash(i + float2(1.0f, 1.0f));

    return lerp(lerp(a, b, u.x), lerp(c, d, u.x), u.y);
}

float fbm(float2 p)
{
    float f = 0.0f;
    float amp = 0.5f;
    for (int i = 0; i < 4; i++)
    {
        f += amp * noise(p);
        p *= 2.0f;
        amp *= 0.5f;
    }
    return f;
}

// -------------------------------------------------------------------------
// Pixel Shader
// -------------------------------------------------------------------------
float4 PS(VertexOut pin) : SV_Target
{
    if (gDomainRadius > 50.0f)
    {
        return float4(1.0f, 1.0f, 1.0f, 1.0f); // 100% 하얀색(섬광탄 효과)
    }

    float3 centerPos = mul(float4(0.0f, 0.0f, 0.0f, 1.0f), gWorld).xyz;
    float3 normalW = normalize(pin.PosW - centerPos);
    float3 viewDir = normalize(gEyePosW - pin.PosW);
    float fresnelBase = saturate(1.0f - dot(normalW, viewDir));
    float fresnel = pow(fresnelBase, 2.5f);

    float4 texColor = gTextureMaps[4].Sample(gsamAnisotropicWrap, pin.TexC);
    float magicCircleMask = smoothstep(0.1f, 0.5f, texColor.a);

    float3 baseSphereColor = float3(0.0f, 0.15f, 0.6f);
    float3 rimLightColor = float3(0.4f, 0.7f, 1.0f);
    float3 magicLineColor = float3(0.5f, 0.8f, 1.0f);

    float3 sphereFinal = baseSphereColor + (rimLightColor * fresnel * 2.0f);
    float3 finalColor = lerp(sphereFinal, magicLineColor, magicCircleMask);

    if (magicCircleMask > 0.1f)
    {
        finalColor += magicLineColor * 0.5f;
    }

    float glassAlpha = 0.4f + (fresnel * 0.5f);
    float finalAlpha = lerp(glassAlpha, 1.0f, magicCircleMask);

    return float4(finalColor, saturate(finalAlpha));
}
