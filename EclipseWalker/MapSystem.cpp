#include "MapSystem.h"


using namespace DirectX;

MapSystem::MapSystem()
{
    // 초기 바운딩 박스 무한대 설정
    mMapBounds.Center = { 0.0f, 0.0f, 0.0f };
    mMapBounds.Extents = { 1000.0f, 1000.0f, 1000.0f };
}

MapSystem::~MapSystem()
{
}

void MapSystem::Build(MeshGeometry* geo,
    float scale, float rotX, float rotY, float rotZ,
    float posX, float posY, float posZ)
{
    // 0. 변환 행렬
    XMMATRIX S = XMMatrixScaling(scale, scale, scale);
    XMMATRIX R = XMMatrixRotationRollPitchYaw(XMConvertToRadians(rotX), XMConvertToRadians(rotY), XMConvertToRadians(rotZ));
    XMMATRIX T = XMMatrixTranslation(posX, posY, posZ);
    XMMATRIX worldTransform = S * R * T;

    // 1. 버퍼 포인터 가져오기
    uint8_t* pVBuffer = (uint8_t*)geo->VertexBufferCPU->GetBufferPointer();
    uint8_t* pIBuffer = (uint8_t*)geo->IndexBufferCPU->GetBufferPointer();

    UINT vStride = geo->VertexByteStride;
    bool is32BitIndex = (geo->IndexFormat == DXGI_FORMAT_R32_UINT);

    mMapVertices.clear();
    mMapIndices.clear();

    // 2. 전체 버텍스 변환 (여기는 그대로)
    size_t totalVCount = geo->VertexBufferByteSize / vStride;
    mMapVertices.resize(totalVCount);

    XMVECTOR vMin = XMVectorReplicate(+FLT_MAX);
    XMVECTOR vMax = XMVectorReplicate(-FLT_MAX);

    for (size_t i = 0; i < totalVCount; ++i)
    {
        XMFLOAT3* pPos = (XMFLOAT3*)(pVBuffer + i * vStride);
        XMVECTOR P = XMLoadFloat3(pPos);
        P = XMVector3TransformCoord(P, worldTransform);
        XMStoreFloat3(&mMapVertices[i].Pos, P);

        vMin = XMVectorMin(vMin, P);
        vMax = XMVectorMax(vMax, P);
    }

    for (auto& pair : geo->DrawArgs)
    {
        auto& submesh = pair.second;

        UINT startIndex = submesh.StartIndexLocation;
        UINT indexCount = submesh.IndexCount;
        int baseVertexLoc = submesh.BaseVertexLocation;

        for (UINT i = 0; i < indexCount; ++i)
        {
            UINT originalIndex = 0;

            if (is32BitIndex)
            {
                originalIndex = ((std::uint32_t*)pIBuffer)[startIndex + i];
            }
            else
            {
                originalIndex = ((std::uint16_t*)pIBuffer)[startIndex + i];
            }

            // 진짜 인덱스 = 파일상 인덱스 + 오프셋
            UINT realIndex = (UINT)(originalIndex + baseVertexLoc);

            if (realIndex < totalVCount)
            {
                mMapIndices.push_back(realIndex);
            }
        }
    }

    XMVECTOR center = (vMin + vMax) * 0.5f;
    XMVECTOR extents = (vMax - vMin) * 0.5f;
    XMStoreFloat3(&mMapBounds.Center, center);
    XMStoreFloat3(&mMapBounds.Extents, extents);

    char buf[512];
    sprintf_s(buf, ">> [MapSystem] 스마트 로드 완료! (총 정점: %lld, 총 인덱스: %lld)\n",
        mMapVertices.size(), mMapIndices.size());
    OutputDebugStringA(buf);
}

bool MapSystem::CheckCollision(const BoundingBox& playerBox)
{
    return false;
}

float Area2D(float x1, float z1, float x2, float z2, float x3, float z3)
{
    return abs((x1 * (z2 - z3) + x2 * (z3 - z1) + x3 * (z1 - z2)) / 2.0f);
}

float MapSystem::GetFloorHeight(float x, float z, float currentY)
{
    if (mMapIndices.empty() || mMapVertices.empty()) return -9999.0f;

    // 1. 레이(Ray) 설정
    // 시작점: 플레이어의 머리 위 (키가 1.7m라고 가정하고, 넉넉하게 2.0m 위에서 쏨)
    // 방향: 정확히 아래쪽 (0, -1, 0)
    XMVECTOR rayOrigin = XMVectorSet(x, currentY + 2.0f, z, 1.0f);
    XMVECTOR rayDir = XMVectorSet(0.0f, -1.0f, 0.0f, 0.0f);

    float closestDist = FLT_MAX; // 가장 가까운 충돌 거리
    bool hitFound = false;

    // 최적화: 내 주변 5m 안의 삼각형만 검사
    float searchRadius = 5.0f;

    UINT triCount = (UINT)mMapIndices.size() / 3;

    for (UINT i = 0; i < triCount; ++i)
    {
        // 인덱스 가져오기
        UINT i0 = mMapIndices[i * 3 + 0];
        UINT i1 = mMapIndices[i * 3 + 1];
        UINT i2 = mMapIndices[i * 3 + 2];

        // 안전 장치
        if (i0 >= mMapVertices.size() || i1 >= mMapVertices.size() || i2 >= mMapVertices.size())
            continue;

        const auto& p0 = mMapVertices[i0].Pos;

        // 1. 거리 최적화 (너무 먼 삼각형은 계산 안 함)
        if (abs(p0.x - x) > searchRadius || abs(p0.z - z) > searchRadius)
            continue;

        const auto& p1 = mMapVertices[i1].Pos;
        const auto& p2 = mMapVertices[i2].Pos;

        XMVECTOR v0 = XMLoadFloat3(&p0);
        XMVECTOR v1 = XMLoadFloat3(&p1);
        XMVECTOR v2 = XMLoadFloat3(&p2);

        // 2. [핵심] 레이저가 삼각형을 뚫었는가? (DirectX 기능 사용)
        float dist = 0.0f;
        if (DirectX::TriangleTests::Intersects(rayOrigin, rayDir, v0, v1, v2, dist))
        {
            // 뚫었다면, 이 거리가 기존에 찾은 것보다 가까운가? (가장 위의 바닥을 찾음)
            if (dist < closestDist)
            {
                closestDist = dist;
                hitFound = true;
            }
        }
    }

    // 3. 결과 반환
    if (hitFound)
    {
        // 바닥 높이 = 레이저 시작 높이 - 닿은 거리
        float floorY = (currentY + 2.0f) - closestDist;

        // 살짝(0.1f) 띄워줘서 파묻힘 방지
        return floorY + 0.1f;
    }

    // 바닥이 없으면 허공
    return -9999.0f;
}