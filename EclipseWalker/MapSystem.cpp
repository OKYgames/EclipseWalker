#include "MapSystem.h"

using namespace DirectX;

MapSystem::MapSystem()
{
}

MapSystem::~MapSystem()
{
}

// =========================================================
// 1. 바닥(Floor) 큐브 데이터 빌드
// =========================================================
void MapSystem::BuildFloor(MeshGeometry* geo, float scale, float rotX, float rotY, float rotZ, float posX, float posY, float posZ)
{
    XMMATRIX worldTransform = XMMatrixScaling(scale, scale, scale) * XMMatrixRotationRollPitchYaw(XMConvertToRadians(rotX), XMConvertToRadians(rotY), XMConvertToRadians(rotZ)) * XMMatrixTranslation(posX, posY, posZ);

    uint8_t* pVBuffer = (uint8_t*)geo->VertexBufferCPU->GetBufferPointer();
    uint8_t* pIBuffer = (uint8_t*)geo->IndexBufferCPU->GetBufferPointer();
    UINT vStride = geo->VertexByteStride;
    bool is32BitIndex = (geo->IndexFormat == DXGI_FORMAT_R32_UINT);

    size_t totalVCount = geo->VertexBufferByteSize / vStride;
    size_t currentVertexOffset = mFloorVertices.size(); 

    // 정점 변환 및 저장
    for (size_t i = 0; i < totalVCount; ++i)
    {
        XMFLOAT3* pPos = (XMFLOAT3*)(pVBuffer + i * vStride);
        XMVECTOR P = XMLoadFloat3(pPos);
        P = XMVector3TransformCoord(P, worldTransform);

        Vertex v;
        XMStoreFloat3(&v.Pos, P);
        mFloorVertices.push_back(v);
    }

    // 인덱스 저장
    for (auto& pair : geo->DrawArgs)
    {
        auto& submesh = pair.second;
        for (UINT i = 0; i < submesh.IndexCount; ++i)
        {
            UINT originalIndex = is32BitIndex ? ((std::uint32_t*)pIBuffer)[submesh.StartIndexLocation + i]
                : ((std::uint16_t*)pIBuffer)[submesh.StartIndexLocation + i];
            mFloorIndices.push_back((UINT)(originalIndex + submesh.BaseVertexLocation + currentVertexOffset));
        }
    }
}

// =========================================================
// 2. 벽(Wall) 큐브 데이터 빌드
// =========================================================
void MapSystem::BuildWall(MeshGeometry* geo, float scale, float rotX, float rotY, float rotZ, float posX, float posY, float posZ)
{
    XMMATRIX worldTransform = XMMatrixScaling(scale, scale, scale) * XMMatrixRotationRollPitchYaw(XMConvertToRadians(rotX), XMConvertToRadians(rotY), XMConvertToRadians(rotZ)) * XMMatrixTranslation(posX, posY, posZ);

    uint8_t* pVBuffer = (uint8_t*)geo->VertexBufferCPU->GetBufferPointer();
    uint8_t* pIBuffer = (uint8_t*)geo->IndexBufferCPU->GetBufferPointer();
    UINT vStride = geo->VertexByteStride;
    bool is32BitIndex = (geo->IndexFormat == DXGI_FORMAT_R32_UINT);

    size_t totalVCount = geo->VertexBufferByteSize / vStride;
    size_t currentVertexOffset = mWallVertices.size();

    for (size_t i = 0; i < totalVCount; ++i)
    {
        XMFLOAT3* pPos = (XMFLOAT3*)(pVBuffer + i * vStride);
        XMVECTOR P = XMVector3TransformCoord(XMLoadFloat3(pPos), worldTransform);
        Vertex v; XMStoreFloat3(&v.Pos, P);
        mWallVertices.push_back(v);
    }

    for (auto& pair : geo->DrawArgs)
    {
        auto& submesh = pair.second;
        for (UINT i = 0; i < submesh.IndexCount; ++i)
        {
            UINT originalIndex = is32BitIndex ? ((std::uint32_t*)pIBuffer)[submesh.StartIndexLocation + i]
                : ((std::uint16_t*)pIBuffer)[submesh.StartIndexLocation + i];
            mWallIndices.push_back((UINT)(originalIndex + submesh.BaseVertexLocation + currentVertexOffset));
        }
    }
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