#include "Light.hlsl"

#define MAX_SKINNED_BONES 96

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
    float4 gAmbientLight;
    Light gLights[MAX_LIGHTS];

    float3 gDomainCenter;
    float gDomainRadius;
    int gIsDomainActive;
    float3 gDomainPad;
    float4 gFogColor;
    float gFogStart;
    float gFogRange;
    float2 gFogPad;
    float4 gSkyTint;
};

cbuffer cbSkinned : register(b3)
{
    float4x4 gBoneTransforms[MAX_SKINNED_BONES];
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

struct VertexInSkinned
{
    float3 PosL : POSITION;
    float3 NormalL : NORMAL;
    float2 TexC : TEXCOORD;
    float3 TangentU : TANGENT;
    float4 BoneWeights : WEIGHTS;
    uint4 BoneIndices : BONEINDICES;
};

struct SkinnedLocalData
{
    float4 PosL;
    float3 NormalL;
    float3 TangentL;
};

struct OpaqueVertexOut
{
    float4 PosH : SV_POSITION;
    float3 PosW : POSITION;
    float3 NormalW : NORMAL;
    float3 TangentW : TANGENT;
    float2 TexC : TEXCOORD;
    float ViewDepth : TEXCOORD1;
};

struct ShadowVertexOut
{
    float4 PosH : SV_POSITION;
    float2 TexC : TEXCOORD;
};

SkinnedLocalData SkinVertex(VertexInSkinned vin)
{
    SkinnedLocalData localData = (SkinnedLocalData)0.0f;

    [unroll]
    for (int i = 0; i < 4; ++i)
    {
        float weight = vin.BoneWeights[i];
        if (weight <= 0.0f)
        {
            continue;
        }

        uint boneIndex = vin.BoneIndices[i];
        localData.PosL += weight * mul(float4(vin.PosL, 1.0f), gBoneTransforms[boneIndex]);
        localData.NormalL += weight * mul(float4(vin.NormalL, 0.0f), gBoneTransforms[boneIndex]).xyz;
        localData.TangentL += weight * mul(float4(vin.TangentU, 0.0f), gBoneTransforms[boneIndex]).xyz;
    }

    if (dot(localData.PosL, localData.PosL) < 0.0001f)
    {
        localData.PosL = float4(vin.PosL, 1.0f);
        localData.NormalL = vin.NormalL;
        localData.TangentL = vin.TangentU;
    }

    return localData;
}

OpaqueVertexOut VS_Opaque(VertexInSkinned vin)
{
    OpaqueVertexOut vout;
    SkinnedLocalData localData = SkinVertex(vin);

    float4 posW = mul(localData.PosL, gWorld);
    vout.PosW = posW.xyz;
    vout.PosH = mul(posW, gViewProj);
    vout.ViewDepth = mul(posW, gView).z;
    vout.NormalW = mul(localData.NormalL, (float3x3)gWorld);
    vout.TangentW = mul(localData.TangentL, (float3x3)gWorld);
    vout.TexC = mul(float4(vin.TexC, 0.0f, 1.0f), gTexTransform).xy;

    return vout;
}

ShadowVertexOut VS_Shadow(VertexInSkinned vin)
{
    ShadowVertexOut vout;
    SkinnedLocalData localData = SkinVertex(vin);

    float4 posW = mul(localData.PosL, gWorld);
    vout.PosH = mul(posW, gViewProj);
    vout.TexC = mul(float4(vin.TexC, 0.0f, 1.0f), gTexTransform).xy;

    return vout;
}

OpaqueVertexOut VS_Outline(VertexInSkinned vin)
{
    OpaqueVertexOut vout = (OpaqueVertexOut)0.0f;
    SkinnedLocalData localData = SkinVertex(vin);

    float normalLengthSq = dot(localData.NormalL, localData.NormalL);
    float3 outlineNormal = (normalLengthSq > 0.0001f)
        ? normalize(localData.NormalL)
        : float3(0.0f, 1.0f, 0.0f);
    float4 outlinePosL = localData.PosL + float4(outlineNormal * gOutlineThickness, 0.0f);
    float4 posW = mul(outlinePosL, gWorld);
    vout.PosH = mul(posW, gViewProj);
    vout.TexC = vin.TexC;

    return vout;
}
