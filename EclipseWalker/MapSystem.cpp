#include "MapSystem.h"
#include "ModelLoader.h"

using namespace DirectX;

MapSystem::MapSystem()
{
}

MapSystem::~MapSystem()
{
}

bool MapSystem::LoadFloorCollider(const std::string& filename, float scale, float rotX, float rotY, float rotZ, float posX, float posY, float posZ)
{
    MapMeshData data;
    if (!ModelLoader::Load(filename, data))
        return false;

    char debugMsg[256];
    sprintf_s(debugMsg, "\n[FBX 로드 완료] %s\n - 정점(Vertex) 개수: %zu 개\n\n", filename.c_str(), data.Vertices.size());
    OutputDebugStringA(debugMsg);

    // FBX를 원하는 크기와 위치로 변환하기 위한 행렬
    XMMATRIX worldTransform = XMMatrixScaling(scale, scale, scale) * XMMatrixRotationRollPitchYaw(XMConvertToRadians(rotX), XMConvertToRadians(rotY), XMConvertToRadians(rotZ)) * XMMatrixTranslation(posX, posY, posZ);

    size_t currentVertexOffset = mFloorVertices.size();

    // 1. 정점(Vertex) 로드 및 변환
    for (size_t i = 0; i < data.Vertices.size(); ++i)
    {
        XMVECTOR P = XMLoadFloat3(&data.Vertices[i].Pos);
        P = XMVector3TransformCoord(P, worldTransform); // 스케일, 회전, 위치 적용

        Vertex v;
        XMStoreFloat3(&v.Pos, P);
        mFloorVertices.push_back(v);
    }

    // 2. 인덱스(Index) 로드
    for (size_t i = 0; i < data.Indices.size(); ++i)
    {
        mFloorIndices.push_back(data.Indices[i] + (UINT)currentVertexOffset);
    }

    return true;
}

bool MapSystem::LoadWallCollider(const std::string& filename, float scale, float rotX, float rotY, float rotZ, float posX, float posY, float posZ)
{
    MapMeshData data;
    if (!ModelLoader::Load(filename, data))
        return false;

    XMMATRIX worldTransform = XMMatrixScaling(scale, scale, scale) * XMMatrixRotationRollPitchYaw(XMConvertToRadians(rotX), XMConvertToRadians(rotY), XMConvertToRadians(rotZ)) * XMMatrixTranslation(posX, posY, posZ);

    size_t currentVertexOffset = mWallVertices.size();

    for (size_t i = 0; i < data.Vertices.size(); ++i)
    {
        XMVECTOR P = XMVector3TransformCoord(XMLoadFloat3(&data.Vertices[i].Pos), worldTransform);
        Vertex v; XMStoreFloat3(&v.Pos, P);
        mWallVertices.push_back(v);
    }

    for (size_t i = 0; i < data.Indices.size(); ++i)
    {
        mWallIndices.push_back(data.Indices[i] + (UINT)currentVertexOffset);
    }

    return true;
}

// =========================================================
// 3. 바닥 높이 찾기 
// =========================================================
float MapSystem::GetFloorHeight(float x, float z, float currentY, float checkRange)
{
    if (mFloorIndices.empty() || mFloorVertices.empty()) return -9999.0f;

    float bestFloorY = -9999.0f;
    bool found = false;

    XMVECTOR rayOrigin = XMVectorSet(x, currentY, z, 1.0f);
    XMVECTOR rayDir = XMVectorSet(0.0f, -1.0f, 0.0f, 0.0f);

    UINT triCount = (UINT)mFloorIndices.size() / 3;

    for (UINT i = 0; i < triCount; ++i)
    {
        UINT i0 = mFloorIndices[i * 3 + 0];
        UINT i1 = mFloorIndices[i * 3 + 1];
        UINT i2 = mFloorIndices[i * 3 + 2];

        XMVECTOR v0 = XMLoadFloat3(&mFloorVertices[i0].Pos);
        XMVECTOR v1 = XMLoadFloat3(&mFloorVertices[i1].Pos);
        XMVECTOR v2 = XMLoadFloat3(&mFloorVertices[i2].Pos);

        float dist = 0.0f;
        if (DirectX::TriangleTests::Intersects(rayOrigin, rayDir, v0, v1, v2, dist))
        {
            float hitY = currentY - dist; 
            if (hitY > bestFloorY)
            {
                bestFloorY = hitY;
                found = true;
            }
        }
    }

    if (found) return bestFloorY; 
    return -9999.0f;
}

// =========================================================
// 4. 벽 충돌 검사 
// =========================================================
bool MapSystem::CheckWall(float x, float z, float currentY, float dirX, float dirZ)
{
    if (mWallIndices.empty() || mWallVertices.empty()) return false;

    XMVECTOR dirVec = XMVector3Normalize(XMVectorSet(dirX, 0.0f, dirZ, 0.0f));

    XMVECTOR rayOrigin = XMVectorSet(x, currentY + 0.55f, z, 1.0f);
    float checkDist = 0.8f; 

    UINT triCount = (UINT)mWallIndices.size() / 3;

    for (UINT i = 0; i < triCount; ++i)
    {
        UINT i0 = mWallIndices[i * 3 + 0];
        UINT i1 = mWallIndices[i * 3 + 1];
        UINT i2 = mWallIndices[i * 3 + 2];

        XMVECTOR v0 = XMLoadFloat3(&mWallVertices[i0].Pos);
        XMVECTOR v1 = XMLoadFloat3(&mWallVertices[i1].Pos);
        XMVECTOR v2 = XMLoadFloat3(&mWallVertices[i2].Pos);

        float dist = 0.0f;
        if (DirectX::TriangleTests::Intersects(rayOrigin, dirVec, v0, v1, v2, dist))
        {
            if (dist < checkDist) return true;
        }
    }

    return false; 
}

// =========================================================
// 5. 레이캐스트
// =========================================================
bool MapSystem::CastRay(FXMVECTOR origin, FXMVECTOR dir, float maxDist, float& outDist)
{
    float closestDist = maxDist;
    bool hitFound = false;

    // 1. 벽 큐브 먼저 찔러보기
    if (!mWallIndices.empty() && !mWallVertices.empty())
    {
        UINT triCount = (UINT)mWallIndices.size() / 3;
        for (UINT i = 0; i < triCount; ++i)
        {
            XMVECTOR v0 = XMLoadFloat3(&mWallVertices[mWallIndices[i * 3 + 0]].Pos);
            XMVECTOR v1 = XMLoadFloat3(&mWallVertices[mWallIndices[i * 3 + 1]].Pos);
            XMVECTOR v2 = XMLoadFloat3(&mWallVertices[mWallIndices[i * 3 + 2]].Pos);

            float dist = 0.0f;
            if (DirectX::TriangleTests::Intersects(origin, dir, v0, v1, v2, dist))
            {
                if (dist < closestDist) { closestDist = dist; hitFound = true; }
            }
        }
    }

    // 2. 바닥 큐브도 찔러보기
    if (!mFloorIndices.empty() && !mFloorVertices.empty())
    {
        UINT triCount = (UINT)mFloorIndices.size() / 3;
        for (UINT i = 0; i < triCount; ++i)
        {
            XMVECTOR v0 = XMLoadFloat3(&mFloorVertices[mFloorIndices[i * 3 + 0]].Pos);
            XMVECTOR v1 = XMLoadFloat3(&mFloorVertices[mFloorIndices[i * 3 + 1]].Pos);
            XMVECTOR v2 = XMLoadFloat3(&mFloorVertices[mFloorIndices[i * 3 + 2]].Pos);

            float dist = 0.0f;
            if (DirectX::TriangleTests::Intersects(origin, dir, v0, v1, v2, dist))
            {
                if (dist < closestDist) { closestDist = dist; hitFound = true; }
            }
        }
    }

    if (hitFound)
    {
        outDist = closestDist;
        return true;
    }
    return false;
}