#pragma once
#include "d3dUtil.h"
#include "MeshGeometry.h"
#include "Material.h"

const int gNumFrameResources = 3;

// 렌더링할 물체 하나를 정의하는 구조체
struct RenderItem
{
    RenderItem() = default;

    DirectX::XMFLOAT4X4 World = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();
    DirectX::XMFLOAT4 ColorMultiplier = { 1.0f, 1.0f, 1.0f, 1.0f };
    int NumFramesDirty = gNumFrameResources;
    UINT ObjCBIndex = -1;
    UINT SkinnedCBIndex = -1;

    MeshGeometry* Geo = nullptr;
    Material* Mat = nullptr;
    D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    UINT IndexCount = 0;
    UINT StartIndexLocation = 0;
    int BaseVertexLocation = 0;

    bool Visible = true;
    bool CastShadow = true;
    bool IsSkinned = false;
    bool IsSkybox = false;
};
