#include "MapSystem.h"


using namespace DirectX;

MapSystem::MapSystem()
{
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

bool MapSystem::CheckWall(float x, float z, float currentY, float dirX, float dirZ)
{
    if (mMapIndices.empty() || mMapVertices.empty()) return false;

    // 1. 방향 벡터 정규화
    XMVECTOR dirVec = XMVectorSet(dirX, 0.0f, dirZ, 0.0f);
    dirVec = XMVector3Normalize(dirVec);

    // 2. 레이저 시작점: 발바닥이 아닌 "가슴 높이(1.0m)"
    // (낮은 턱이나 계단에 걸리지 않도록 함)
    XMVECTOR rayOrigin = XMVectorSet(x, currentY + 1.0f, z, 1.0f);

    // 3. 감지 거리: 몸 두께(0.5m) + 여유분 -> 약 0.8m 설정
    float checkDist = 0.8f;

    // 최적화: 내 주변 3m만 검사
    float searchRadius = 3.0f;
    UINT triCount = (UINT)mMapIndices.size() / 3;

    for (UINT i = 0; i < triCount; ++i)
    {
        UINT i0 = mMapIndices[i * 3 + 0];
        UINT i1 = mMapIndices[i * 3 + 1];
        UINT i2 = mMapIndices[i * 3 + 2];

        if (i0 >= mMapVertices.size() || i1 >= mMapVertices.size() || i2 >= mMapVertices.size())
            continue;

        const auto& p0 = mMapVertices[i0].Pos;

        // 거리 최적화
        if (abs(p0.x - x) > searchRadius || abs(p0.z - z) > searchRadius) continue;

        // 높이 최적화 (내 키 범위 밖의 벽은 무시)
        if (p0.y > currentY + 3.0f || p0.y < currentY - 1.0f) continue;

        const auto& p1 = mMapVertices[i1].Pos;
        const auto& p2 = mMapVertices[i2].Pos;

        XMVECTOR v0 = XMLoadFloat3(&p0);
        XMVECTOR v1 = XMLoadFloat3(&p1);
        XMVECTOR v2 = XMLoadFloat3(&p2);

        // 4. 레이저 충돌 검사
        float dist = 0.0f;
        if (DirectX::TriangleTests::Intersects(rayOrigin, dirVec, v0, v1, v2, dist))
        {
            // 너무 가까우면 벽으로 판정
            if (dist < checkDist)
            {
                return true; // 벽 있음!
            }
        }
    }

    return false; // 벽 없음
}

float Area2D(float x1, float z1, float x2, float z2, float x3, float z3)
{
    return abs((x1 * (z2 - z3) + x2 * (z3 - z1) + x3 * (z1 - z2)) / 2.0f);
}

float MapSystem::GetFloorHeight(float x, float z, float currentY, float checkRange)
{
    if (mMapIndices.empty() || mMapVertices.empty()) return -9999.0f;

    float bestFloorY = -9999.0f;
    bool found = false;

    // [설정] 최적화 범위 및 경사 제한
    float searchRadius = 5.0f;
    float slopeLimit = 0.5f; // 0.0(수직) ~ 1.0(평지), 0.5 이하면 벽으로 간주

    UINT triCount = (UINT)mMapIndices.size() / 3;

    for (UINT i = 0; i < triCount; ++i)
    {
        UINT i0 = mMapIndices[i * 3 + 0];
        UINT i1 = mMapIndices[i * 3 + 1];
        UINT i2 = mMapIndices[i * 3 + 2];

        // 인덱스 범위 초과 방지
        if (i0 >= mMapVertices.size() || i1 >= mMapVertices.size() || i2 >= mMapVertices.size())
            continue;

        const auto& p0 = mMapVertices[i0].Pos;

        // 1. [거리 최적화] 내 주변 5m만 검사
        if (abs(p0.x - x) > searchRadius || abs(p0.z - z) > searchRadius) continue;

        // 2. [높이 최적화]
        // (1) 머리 위 무시
        if (p0.y > currentY + checkRange) continue;

        // (2) 바닥 뚫림 방지
        if (p0.y < currentY - 50.0f) continue;

        const auto& p1 = mMapVertices[i1].Pos;
        const auto& p2 = mMapVertices[i2].Pos;

        // 삼각형의 다른 점들도 머리보다 높으면 무시 
        if (p1.y > currentY + checkRange || p2.y > currentY + checkRange) continue;


        // 3. [법선 벡터 체크] 벽 & 천장 무시
        XMVECTOR v0 = XMLoadFloat3(&p0);
        XMVECTOR v1 = XMLoadFloat3(&p1);
        XMVECTOR v2 = XMLoadFloat3(&p2);

        XMVECTOR edge1 = v1 - v0;
        XMVECTOR edge2 = v2 - v0;
        XMVECTOR normalVec = XMVector3Cross(edge1, edge2);
        normalVec = XMVector3Normalize(normalVec);

        float normalY = XMVectorGetY(normalVec);

        // (1) 천장 무시: 법선 Y가 음수면 아래를 보고 있는 면임
        if (normalY < -0.1f) continue;

        // (2) 벽 무시: 법선 Y가 너무 작으면(0에 가까우면) 가파른 벽임
        if (abs(normalY) < slopeLimit) continue;


        // 4. [무게중심 좌표법] (x, z)가 삼각형 안에 있는지 정밀 검사
        float areaABC = Area2D(p0.x, p0.z, p1.x, p1.z, p2.x, p2.z);

        // 면적이 너무 작으면(선이나 점) 계산 불가
        if (areaABC < 0.001f) continue;

        float areaPBC = Area2D(x, z, p1.x, p1.z, p2.x, p2.z);
        float areaPCA = Area2D(p0.x, p0.z, x, z, p2.x, p2.z);
        float areaPAB = Area2D(p0.x, p0.z, p1.x, p1.z, x, z);

        // 오차 범위(0.01) 내에서 면적 합이 일치하면 내부에 있는 것
        if (abs(areaABC - (areaPBC + areaPCA + areaPAB)) < 0.01f)
        {
            // 높이(Y) 보간 (Interpolation)
            float u = areaPBC / areaABC;
            float v = areaPCA / areaABC;
            float w = areaPAB / areaABC;

            float height = u * p0.y + v * p1.y + w * p2.y;

            // 여러 층이 겹쳐있을 경우, 내 발 밑에 있는 가장 높은 바닥을 선택
            if (height > bestFloorY)
            {
                bestFloorY = height;
                found = true;
            }
        }
    }

    if (found)
    {
        // 바닥에 파묻히지 않게 아주 살짝(0.1f) 띄워서 리턴
        return bestFloorY + 0.1f;
    }

    // 바닥을 못 찾음 (허공/낭떠러지)
    return -9999.0f;
}
