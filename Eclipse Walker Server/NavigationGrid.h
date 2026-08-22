#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

class NavigationGrid
{
public:
    bool Load(const std::string& floorColliderPath, const std::string& wallColliderPath);
    bool IsReady() const { return m_ready; }

    bool HasDirectPath(float startX, float startZ, float targetX, float targetZ) const;

    bool FindPath(
        float startX,
        float startZ,
        float targetX,
        float targetZ,
        std::vector<std::pair<float, float>>& outWaypoints) const;

public:
    struct Vec3
    {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
    };

    struct Triangle
    {
        Vec3 a;
        Vec3 b;
        Vec3 c;
    };

    struct Cell
    {
        float floorY = 0.0f;
        bool walkable = false;
    };

private:
    bool LoadCollider(const std::string& relativeAssetPath, std::vector<Triangle>& outTriangles);
    void BuildGrid();
    bool GetFloorHeight(float x, float z, float& outHeight) const;
    bool CanTraverse(int fromIndex, int toIndex) const;
    bool SegmentHitsWall(const Vec3& start, const Vec3& end) const;
    bool SegmentIntersectsTriangle(const Vec3& start, const Vec3& end, const Triangle& triangle) const;
    int FindNearestWalkableCell(float x, float z) const;
    bool HasLineOfSight(int fromIndex, int toIndex) const;
    bool AreConnected(int fromIndex, int toIndex) const;
    int GetNeighborIndex(int cellIndex, int directionIndex) const;
    std::pair<float, float> GetCellCenter(int cellIndex) const;
    std::string FindAssetPath(const std::string& relativeAssetPath) const;

private:
    float m_cellSize = 0.65f;
    float m_minX = 0.0f;
    float m_minZ = 0.0f;
    int m_width = 0;
    int m_height = 0;
    bool m_ready = false;
    std::vector<Triangle> m_floorTriangles;
    std::vector<Triangle> m_wallTriangles;
    std::vector<Cell> m_cells;
    std::vector<std::uint8_t> m_connections;
};
