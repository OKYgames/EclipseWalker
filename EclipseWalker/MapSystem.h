#pragma once
#include "d3dUtil.h"
#include "MeshGeometry.h"
#include "Vertices.h"

class MapSystem
{
public:
    MapSystem();
    ~MapSystem();

    bool LoadFloorCollider(const std::string& filename, float scale = 1.0f, float rotX = 0.0f, float rotY = 0.0f, float rotZ = 0.0f, float posX = 0.0f, float posY = 0.0f, float posZ = 0.0f);
    bool LoadWallCollider(const std::string& filename, float scale = 1.0f, float rotX = 0.0f, float rotY = 0.0f, float rotZ = 0.0f, float posX = 0.0f, float posY = 0.0f, float posZ = 0.0f);

    // 충돌 처리 함수들
    float GetFloorHeight(float x, float z, float currentY, float checkRange);
    bool CheckWall(float x, float z, float currentY, float dirX, float dirZ);
    bool CastRay(DirectX::FXMVECTOR origin, DirectX::FXMVECTOR dir, float maxDist, float& outDist);

private:
    // 1. 바닥 전용
    std::vector<Vertex> mFloorVertices;
    std::vector<uint32_t> mFloorIndices;

    // 2. 벽 전용 
    std::vector<Vertex> mWallVertices;
    std::vector<uint32_t> mWallIndices;
};