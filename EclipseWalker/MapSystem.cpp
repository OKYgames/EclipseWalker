#include "MapSystem.h"


using namespace DirectX;

MapSystem::MapSystem()
{
    // 초기 바운딩 박스 무한대 설정 (안전을 위해)
    mMapBounds.Center = { 0.0f, 0.0f, 0.0f };
    mMapBounds.Extents = { 1000.0f, 1000.0f, 1000.0f };
}

MapSystem::~MapSystem()
{
}

void MapSystem::Build(MeshGeometry* geo, const std::string& drawArgName, float scale)
{
    if (geo->DrawArgs.find(drawArgName) == geo->DrawArgs.end())
        return;

    Vertex* vRaw = (Vertex*)geo->VertexBufferCPU->GetBufferPointer();
    std::uint16_t* iRaw = (std::uint16_t*)geo->IndexBufferCPU->GetBufferPointer();

    size_t totalVCount = geo->VertexBufferByteSize / sizeof(Vertex);
    size_t totalICount = geo->IndexBufferByteSize / sizeof(std::uint16_t);

    mMapVertices.resize(totalVCount);
    mMapIndices.resize(totalICount);

    for (size_t i = 0; i < totalVCount; ++i)
    {
        mMapVertices[i] = vRaw[i];

        // 위치(Position)에 스케일 곱하기
        mMapVertices[i].Pos.x *= scale;
        mMapVertices[i].Pos.y *= scale;
        mMapVertices[i].Pos.z *= scale;
    }

    // 인덱스는 그대로 복사
    memcpy(mMapIndices.data(), iRaw, geo->IndexBufferByteSize);

    mMapBounds.Extents = { 1000.0f * scale, 1000.0f * scale, 1000.0f * scale };
}

bool MapSystem::CheckCollision(const BoundingBox& playerBox)
{
    // 최적화: 플레이어가 맵 전체 범위 밖이면 검사 안 함
    if (!playerBox.Intersects(mMapBounds)) return false;

    UINT triCount = (UINT)mMapIndices.size() / 3;

    for (UINT i = 0; i < triCount; ++i)
    {
        UINT i0 = mMapIndices[i * 3 + 0];
        UINT i1 = mMapIndices[i * 3 + 1];
        UINT i2 = mMapIndices[i * 3 + 2];

        XMVECTOR v0 = XMLoadFloat3(&mMapVertices[i0].Pos);
        XMVECTOR v1 = XMLoadFloat3(&mMapVertices[i1].Pos);
        XMVECTOR v2 = XMLoadFloat3(&mMapVertices[i2].Pos);

        // [★핵심 수정] "바닥"은 충돌 벽으로 치지 않는다!
        // 1. 법선 벡터(Normal) 계산
        XMVECTOR edge1 = v1 - v0;
        XMVECTOR edge2 = v2 - v0;
        XMVECTOR normal = XMVector3Normalize(XMVector3Cross(edge1, edge2));

        // 2. 기울기 확인 (Y값이 크면 바닥이나 천장임)
        XMFLOAT3 normalF;
        XMStoreFloat3(&normalF, normal);

        // [중요] 기울기가 45도보다 완만하면(0.7f) "밟을 수 있는 땅"으로 간주하고 무시합니다.
        // 이렇게 해야 이동할 때 바닥에 걸리지 않습니다.
        if (abs(normalF.y) > 0.5f) continue;

        // 3. 진짜 "벽"인 경우에만 충돌 검사
        if (playerBox.Intersects(v0, v1, v2))
        {
            return true; // 벽에 막힘!
        }
    }

    return false;
}

float MapSystem::GetFloorHeight(float x, float z, float currentY)
{
    // 레이저 발사 (머리 위에서 아래로)
    XMVECTOR rayOrigin = XMVectorSet(x, currentY + 2.0f, z, 1.0f);
    XMVECTOR rayDir = XMVectorSet(0.0f, -1.0f, 0.0f, 0.0f);

    float minDist = FLT_MAX;
    bool hit = false;

    UINT triCount = (UINT)mMapIndices.size() / 3;

    for (UINT i = 0; i < triCount; ++i)
    {
        // 1. 삼각형 정점 가져오기
        UINT i0 = mMapIndices[i * 3 + 0];
        UINT i1 = mMapIndices[i * 3 + 1];
        UINT i2 = mMapIndices[i * 3 + 2];

        XMVECTOR v0 = XMLoadFloat3(&mMapVertices[i0].Pos);
        XMVECTOR v1 = XMLoadFloat3(&mMapVertices[i1].Pos);
        XMVECTOR v2 = XMLoadFloat3(&mMapVertices[i2].Pos);

        // 2. [★핵심] 법선 벡터(Normal) 계산
        // 벽인지 바닥인지 구분하기 위함
        XMVECTOR edge1 = v1 - v0;
        XMVECTOR edge2 = v2 - v0;
        XMVECTOR normal = XMVector3Normalize(XMVector3Cross(edge1, edge2));

        // 3. 기울기 확인 (Y값이 작으면 벽입니다)
        XMFLOAT3 normalF;
        XMStoreFloat3(&normalF, normal);

        // 0.5f(45도) 미만이면 벽으로 간주하고 무시 (절대 밟지 않음)
        if (abs(normalF.y) < 0.5f) continue;

        float dist;
        if (TriangleTests::Intersects(rayOrigin, rayDir, v0, v1, v2, dist))
        {
            // 거리 양수 체크 (가끔 뒤쪽이 맞을 수도 있음)
            if (dist > 0.0f && dist < minDist)
            {
                minDist = dist;
                hit = true;
            }
        }
    }

    if (hit)
    {
        // 머리 위 높이(currentY + 2.0f) - 거리
        return (currentY + 2.0f) - minDist;
    }

    return -9999.0f; // 바닥 없음
}