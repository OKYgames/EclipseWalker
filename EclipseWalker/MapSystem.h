#pragma once
#include "d3dUtil.h"
#include "MeshGeometry.h"
#include "Vertices.h"

class MapSystem
{
public:
    MapSystem();
    ~MapSystem();

    // 맵 지오메트리 데이터를 받아와서 저장하는 함수
    // geo: 맵의 메쉬 정보, drawArgName: 서브메쉬 이름(예: "map", "box" 등)
    void Build(MeshGeometry* geo, const std::string& drawArgName);

    // 1. 박스(플레이어)가 맵(벽)과 부딪혔는지 검사
    bool CheckCollision(const DirectX::BoundingBox& box);

    // 2. 특정 위치(x, z)의 땅 높이(y)를 구함 (레이캐스팅)
    float GetFloorHeight(float x, float z);

private:
    // 충돌 검사를 위해 CPU 쪽에 복사해둔 맵 데이터
    std::vector<Vertex> mMapVertices;
    std::vector<std::uint16_t> mMapIndices;

    // (최적화용) 맵이 너무 크면 Octree 같은 공간 분할 자료구조가 필요하지만,
    // 지금은 단순 리스트로 충분합니다.
    DirectX::BoundingBox mMapBounds;
};
