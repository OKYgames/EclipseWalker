#pragma once
#include "d3dUtil.h"
#include "MeshGeometry.h"
#include "Vertices.h"

class MapSystem
{
public:
    MapSystem();
    ~MapSystem();

    void BuildFloor(MeshGeometry* geo,
        float scale, float rotX, float rotY, float rotZ,
        float posX, float posY, float posZ);

    void BuildWall(MeshGeometry* geo,
        float scale, float rotX, float rotY, float rotZ,
        float posX, float posY, float posZ);

    // 충돌 처리 함수들
    float GetFloorHeight(float x, float z, float currentY, float checkRange);
    bool CheckWall(float x, float z, float currentY, float dirX, float dirZ);
    bool CastRay(DirectX::FXMVECTOR origin, DirectX::FXMVECTOR dir, float maxDist, float& outDist);

private:
    struct Vertex
    {
        DirectX::XMFLOAT3 Pos;
    };

    // 1. 바닥 전용
    std::vector<Vertex> mFloorVertices;
    std::vector<uint32_t> mFloorIndices;

    // 2. 벽 전용 
    std::vector<Vertex> mWallVertices;
    std::vector<uint32_t> mWallIndices;
};