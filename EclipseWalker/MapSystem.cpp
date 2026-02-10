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

void MapSystem::Build(MeshGeometry* geo, const std::string& drawArgName)
{
    // 1. 해당 이름의 서브메쉬 정보 찾기
    if (geo->DrawArgs.find(drawArgName) == geo->DrawArgs.end())
    {
        OutputDebugStringA("MapSystem Error: DrawArgName not found!\n");
        return;
    }

    // (참고) submesh 변수는 아래에서 검증용으로 쓸 수 있지만, 
    // 전체 맵을 로딩할 때는 굳이 안 써도 됩니다.
    auto& submesh = geo->DrawArgs[drawArgName];

    // 2. 원본 데이터 포인터 가져오기 (System Memory 복사본)
    Vertex* vRaw = (Vertex*)geo->VertexBufferCPU->GetBufferPointer();
    std::uint16_t* iRaw = (std::uint16_t*)geo->IndexBufferCPU->GetBufferPointer();

    // =========================================================
    // 3. 전체 데이터 복사 
    // =========================================================

    // 전체 정점/인덱스 개수 계산
    size_t totalVCount = geo->VertexBufferByteSize / sizeof(Vertex);
    size_t totalICount = geo->IndexBufferByteSize / sizeof(std::uint16_t);

    // 벡터 크기 재할당
    mMapVertices.resize(totalVCount);
    mMapIndices.resize(totalICount);

    // 메모리 통째로 복사 (가장 빠르고 정확함)
    memcpy(mMapVertices.data(), vRaw, geo->VertexBufferByteSize);
    memcpy(mMapIndices.data(), iRaw, geo->IndexBufferByteSize);
}

bool MapSystem::CheckCollision(const BoundingBox& playerBox)
{
    // 최적화: 플레이어가 맵 전체 범위 밖이면 검사 안 함 (생략 가능)
    // if (!playerBox.Intersects(mMapBounds)) return false;

    UINT triCount = (UINT)mMapIndices.size() / 3;

    // 모든 삼각형 순회 (Brute Force)
    // 맵이 아주 크면(삼각형 1만개 이상) 여기서 렉이 걸릴 수 있습니다.
    // 그때는 Octree나 Grid 분할을 도입해야 합니다.
    for (UINT i = 0; i < triCount; ++i)
    {
        UINT i0 = mMapIndices[i * 3 + 0];
        UINT i1 = mMapIndices[i * 3 + 1];
        UINT i2 = mMapIndices[i * 3 + 2];

        XMVECTOR v0 = XMLoadFloat3(&mMapVertices[i0].Pos);
        XMVECTOR v1 = XMLoadFloat3(&mMapVertices[i1].Pos);
        XMVECTOR v2 = XMLoadFloat3(&mMapVertices[i2].Pos);

        // DirectXCollision 함수 사용
        if (playerBox.Intersects(v0, v1, v2))
        {
            return true; 
        }
    }

    return false;
}

float MapSystem::GetFloorHeight(float x, float z)
{
    // 하늘(y=1000)에서 땅으로 레이를 쏨
    XMVECTOR rayOrigin = XMVectorSet(x, 1000.0f, z, 1.0f);
    XMVECTOR rayDir = XMVectorSet(0.0f, -1.0f, 0.0f, 0.0f); // 아래쪽

    float minDist = FLT_MAX;
    bool hit = false;

    UINT triCount = (UINT)mMapIndices.size() / 3;

    for (UINT i = 0; i < triCount; ++i)
    {
        UINT i0 = mMapIndices[i * 3 + 0];
        UINT i1 = mMapIndices[i * 3 + 1];
        UINT i2 = mMapIndices[i * 3 + 2];

        XMVECTOR v0 = XMLoadFloat3(&mMapVertices[i0].Pos);
        XMVECTOR v1 = XMLoadFloat3(&mMapVertices[i1].Pos);
        XMVECTOR v2 = XMLoadFloat3(&mMapVertices[i2].Pos);

        float dist;
        // 삼각형과 레이의 충돌 검사
        if (TriangleTests::Intersects(rayOrigin, rayDir, v0, v1, v2, dist))
        {
            if (dist < minDist)
            {
                minDist = dist;
                hit = true;
            }
        }
    }

    if (hit)
    {
        // 1000(시작높이) - 거리 = 바닥 높이
        return 1000.0f - minDist;
    }

    return -9999.0f; // 바닥 없음 (낙사 구간)
}