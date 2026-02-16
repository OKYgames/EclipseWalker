#pragma once
#include "d3dUtil.h"
#include "MeshGeometry.h"
#include "Vertices.h"


class MapSystem
{
public:
    MapSystem();
    ~MapSystem();

    void Build(MeshGeometry* geo,
        float scale, float rotX, float rotY, float rotZ,
        float posX, float posY, float posZ);

    // 1. 박스(플레이어)가 맵(벽)과 부딪혔는지 검사
    bool CheckCollision(const DirectX::BoundingBox& box);

    // 2. 특정 위치(x, z)의 땅 높이(y)를 구함 
    float GetFloorHeight(float x, float z, float currentY, float checkRange);

    bool CheckWall(float x, float z, float currentY, float dirX, float dirZ);

private:
    // 충돌 검사를 위해 CPU 쪽에 복사해둔 맵 데이터
    std::vector<Vertex> mMapVertices;
    std::vector<uint32_t> mMapIndices;
    struct Vertex
    {
        DirectX::XMFLOAT3 Pos;
    };
    DirectX::BoundingBox mMapBounds;
};
