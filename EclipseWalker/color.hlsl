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
    float4x4 gShadowTransform;
    float3 gEyePosW;
    float cbPerObjectPad1;
    float2 gRenderTargetSize;
    float2 gInvRenderTargetSize;
    float gNearZ;
    float gFarZ;
    float gTotalTime;
    float gDeltaTime;
    
    float4 gAmbientLight;        // ȯ�汤
    Light gLights[MAX_LIGHTS];   // ���� �迭 (�ִ� 16��)

    float3 gDomainCenter;   // ������ �߽� (�÷��̾� ��ġ)
    float  gDomainRadius;   // ���� ����Ʈ�� ��â ������
    int    gIsDomainActive; // ���� Ȱ��ȭ ����
    float3 gDomainPad;      // 16����Ʈ ���� �е�
    float4 gFogColor;
    float gFogStart;
    float gFogRange;
    float2 gFogPad;
    float4 gSkyTint;
};

cbuffer cbMaterial : register(b2)
{
    float4 gDiffuseAlbedo;
    float3 gFresnelR0;
    float  gRoughness;
    float4 gOutlineColor;
    float  gOutlineThickness;
    int    gIsToon;
    int    gIsTransparent;
    
    int    gDiffuseMapIndex;
    int    gNormalMapIndex;
    int    gEmissiveMapIndex;
    int    gMetallicMapIndex;
    int    gPadding; 
};

Texture2D gTextureMaps[1000] : register(t0);
Texture2D gShadowMap         : register(t1000);

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
    float4 PosH    : SV_POSITION; // ȭ�� ��ǥ (Homogeneous Clip Space)
    float3 PosW    : POSITION;    // ���� ��ǥ (���� ����)
    float3 NormalW : NORMAL;      // ���� ���� (���� ����)
    float3 TangentW : TANGENT;
    float2 TexC    : TEXCOORD;
    float ViewDepth : TEXCOORD1;
};

// ---------------------------------------------------------------------------------------
// Vertex Shader
// ---------------------------------------------------------------------------------------
VertexOut VS(VertexIn vin)
{
    VertexOut vout;

    // ������ ���� �������� ��ȯ
    float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
    vout.PosW = posW.xyz;
    vout.PosH = mul(posW, gViewProj);
    vout.ViewDepth = mul(posW, gView).z;

    // ����(Normal)�� ���� �������� ��ȯ
    vout.NormalW = mul(vin.NormalL, (float3x3)gWorld);

    // ����(Tangent)�� ���� �������� ��ȯ
    vout.TangentW = mul(vin.TangentU, (float3x3)gWorld);
   
    // UV ��ǥ ����
    float4 texC = mul(float4(vin.TexC, 0.0f, 1.0f), gTexTransform);
    vout.TexC = mul(float4(vin.TexC, 0.0f, 1.0f), gTexTransform).xy;

    return vout;
}

// �ܰ����� ���ؽ� ���̴� 
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

// �ܰ����� �ȼ� ���̴�
float4 PS_Outline(VertexOut pin) : SV_Target
{
   return gOutlineColor;
}

// ---------------------------------------------------------------------------------------
// Pixel Shader
// ---------------------------------------------------------------------------------------

float CalcShadowFactor(float4 shadowPosH)
{
    // 1. ���� ��ǥ ����ȭ
    shadowPosH.xyz /= shadowPosH.w;

    // 2. ���� �� (���� ����)
    float depth = shadowPosH.z;

    // 3. �ؽ�ó ũ�� �������� (dx = �ؼ� �ϳ��� ũ��)
    uint width, height, numMips;
    gShadowMap.GetDimensions(0, width, height, numMips);
    float dx = 1.0f / (float)width;

    // 4. �ֺ� 9�� �ȼ�(3x3)�� �˻��ؼ� ��� ���� (PCF)
    float percentLit = 0.0f;
    const float2 offsets[9] = {
        float2(-dx,  -dx), float2(0.0f,  -dx), float2(dx,  -dx),
        float2(-dx, 0.0f), float2(0.0f, 0.0f), float2(dx, 0.0f),
        float2(-dx,  dx), float2(0.0f,  dx), float2(dx,  dx)
    };

    [unroll] 
    for(int i = 0; i < 9; ++i)
    {
        // SampleCmpLevelZero�� �ϵ����� �� + ���� ����
        percentLit += gShadowMap.SampleCmpLevelZero(gsamShadow,
            shadowPosH.xy + offsets[i], depth).r;
    }

    // 9�� ������ ��հ� ���� 
    return percentLit / 9.0f;
}

float4 PS(VertexOut pin) : SV_Target
{
    // 1. Diffuse Map (���޹��� gDiffuseMapIndex ���)
    float4 texDiffuse = gTextureMaps[gDiffuseMapIndex].Sample(gsamAnisotropicWrap, pin.TexC) * gDiffuseAlbedo;
    texDiffuse *= gColorMultiplier;
    if (gIsTransparent == 1)
    {
        return texDiffuse; 
    }

    // ���� ����ȭ �� TBN ��� ���� 
    pin.NormalW = normalize(pin.NormalW);
    pin.TangentW = normalize(pin.TangentW); 

    pin.TangentW = normalize(pin.TangentW - dot(pin.TangentW, pin.NormalW) * pin.NormalW);
    float3 bitangentW = cross(pin.NormalW, pin.TangentW);
    float3x3 TBN = float3x3(pin.TangentW, bitangentW, pin.NormalW);

    // 2. Normal Map
    float3 normalMapSample = gTextureMaps[gNormalMapIndex].Sample(gsamAnisotropicWrap, pin.TexC).rgb;
    
    // �븻�� ������ ��ȯ (0~1 -> -1~1)
    float3 bumpedNormalW = 2.0f * normalMapSample - 1.0f; 
    pin.NormalW = mul(bumpedNormalW, TBN); 
    
    // 3. Metallic Map (gMetallicMapIndex ���)
    float metallic = gTextureMaps[gMetallicMapIndex].Sample(gsamAnisotropicWrap, pin.TexC).r;

    // �ݻ���(Fresnel) ����
    float3 f0 = float3(0.04f, 0.04f, 0.04f); 
    float3 fresnelR0 = lerp(f0, texDiffuse.rgb, metallic);

    // �׸��� ���
    float4 shadowPosH = mul(float4(pin.PosW, 1.0f), gShadowTransform);
    float shadowFactor = CalcShadowFactor(shadowPosH);

    // ���� ��� �غ�
    float3 toEyeW = normalize(gEyePosW - pin.PosW);
    float3 ambient = gAmbientLight.rgb * texDiffuse.rgb;
    
    // Material ����ü ����
    Material mat = { texDiffuse, gFresnelR0, gRoughness, gIsToon };
    
    float3 directLight = 0.0f;

    // ���� ��� ����
    for(int i = 0; i < 1; ++i)
    {
        directLight += ComputeDirectionalLight(gLights[i], mat, pin.NormalW, toEyeW) * shadowFactor;
    }

    for(int j = 1; j < MAX_LIGHTS; ++j)
    {
        directLight += ComputePointLight(gLights[j], mat, pin.PosW, pin.NormalW, toEyeW);
    }

    // 4. Emissive Map (gEmissiveMapIndex ���)
    float3 emissiveColor = gTextureMaps[gEmissiveMapIndex].Sample(gsamAnisotropicWrap, pin.TexC).rgb;

    float3 finalColor = ambient + directLight + emissiveColor;
    float fogDepth = abs(pin.ViewDepth);
    float fogAmount = saturate((fogDepth - gFogStart) / max(gFogRange, 0.001f));
    finalColor = lerp(finalColor, gFogColor.rgb, fogAmount);

    return float4(finalColor, texDiffuse.a);
}
