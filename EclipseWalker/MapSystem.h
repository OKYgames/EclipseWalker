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

    bool CheckCollision(const DirectX::BoundingBox& box);
    float GetFloorHeight(float x, float z, float currentY, float checkRange);
    bool CheckWall(float x, float z, float currentY, float dirX, float dirZ);
    bool CastRay(DirectX::FXMVECTOR origin, DirectX::FXMVECTOR dir, float maxDist, float& outDist);

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
