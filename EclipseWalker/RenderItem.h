#pragma once

#include <DirectXMath.h>
#include "MeshGeometry.h" 

// GPU로 보낼 데이터 구조체
struct ObjectConstants
{
    DirectX::XMFLOAT4X4 WorldViewProj = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };
};

// 렌더링할 물체 하나를 정의하는 구조체
struct RenderItem
{
    RenderItem() = default;

    // 월드 행렬 (위치, 회전, 크기)
    DirectX::XMFLOAT4X4 World = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };

    // 더티 플래그 (값이 바뀌어서 GPU 업데이트가 필요한가?)
    int NumFramesDirty = 3;

    // 상수 버퍼 인덱스 (0: 상자, 1: 바닥 ...)
    UINT ObjCBIndex = -1;

    // 기하 구조 (Mesh)
    MeshGeometry* Geo = nullptr;

    // 그리기 설정 
    D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    UINT IndexCount = 0;
    UINT StartIndexLocation = 0;
    int BaseVertexLocation = 0;
};