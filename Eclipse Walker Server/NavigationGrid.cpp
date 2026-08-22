#include "NavigationGrid.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <queue>
#include <windows.h>

namespace
{
    constexpr float kStage1MapScale = 0.014f;
    constexpr float kMaxStepHeight = 1.15f;
    constexpr float kAgentRayHeight = 0.55f;
    constexpr float kAgentRadius = 0.18f;
    constexpr int kNearestCellSearchRadius = 10;
    constexpr float kEpsilon = 0.0001f;

    constexpr std::array<std::pair<int, int>, 8> kDirections =
    {
        std::pair<int, int>{ 1, 0 },
        std::pair<int, int>{ 1, 1 },
        std::pair<int, int>{ 0, 1 },
        std::pair<int, int>{ -1, 1 },
        std::pair<int, int>{ -1, 0 },
        std::pair<int, int>{ -1, -1 },
        std::pair<int, int>{ 0, -1 },
        std::pair<int, int>{ 1, -1 }
    };

    NavigationGrid::Vec3 Add(const NavigationGrid::Vec3& a, const NavigationGrid::Vec3& b)
    {
        return { a.x + b.x, a.y + b.y, a.z + b.z };
    }

    NavigationGrid::Vec3 Subtract(const NavigationGrid::Vec3& a, const NavigationGrid::Vec3& b)
    {
        return { a.x - b.x, a.y - b.y, a.z - b.z };
    }

    float Dot(const NavigationGrid::Vec3& a, const NavigationGrid::Vec3& b)
    {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }

    NavigationGrid::Vec3 Cross(const NavigationGrid::Vec3& a, const NavigationGrid::Vec3& b)
    {
        return {
            a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x
        };
    }

    float Distance2D(float ax, float az, float bx, float bz)
    {
        const float dx = bx - ax;
        const float dz = bz - az;
        return std::sqrt(dx * dx + dz * dz);
    }
}

bool NavigationGrid::Load(const std::string& floorColliderPath, const std::string& wallColliderPath)
{
    m_ready = false;
    m_floorTriangles.clear();
    m_wallTriangles.clear();
    m_cells.clear();
    m_connections.clear();

    if (!LoadCollider(floorColliderPath, m_floorTriangles) ||
        !LoadCollider(wallColliderPath, m_wallTriangles))
    {
        std::cout << "[Navigation] Collider load failed. Monsters will not path through geometry." << std::endl;
        return false;
    }

    BuildGrid();
    m_ready = !m_cells.empty();

    std::cout << "[Navigation] Grid " << m_width << "x" << m_height
        << " built from " << m_floorTriangles.size() << " floor and "
        << m_wallTriangles.size() << " wall triangles." << std::endl;
    return m_ready;
}

bool NavigationGrid::FindPath(
    float startX,
    float startZ,
    float targetX,
    float targetZ,
    std::vector<std::pair<float, float>>& outWaypoints) const
{
    outWaypoints.clear();
    if (!m_ready)
    {
        return false;
    }

    const int startIndex = FindNearestWalkableCell(startX, startZ);
    const int targetIndex = FindNearestWalkableCell(targetX, targetZ);
    if (startIndex < 0 || targetIndex < 0)
    {
        return false;
    }

    if (startIndex == targetIndex || HasLineOfSight(startIndex, targetIndex))
    {
        outWaypoints.push_back({ targetX, targetZ });
        return true;
    }

    const int cellCount = static_cast<int>(m_cells.size());
    const float infinity = std::numeric_limits<float>::infinity();
    std::vector<float> gScore(cellCount, infinity);
    std::vector<int> cameFrom(cellCount, -1);
    std::vector<bool> closed(cellCount, false);
    using QueueEntry = std::pair<float, int>;
    std::priority_queue<QueueEntry, std::vector<QueueEntry>, std::greater<QueueEntry>> open;

    gScore[startIndex] = 0.0f;
    open.push({ 0.0f, startIndex });

    while (!open.empty())
    {
        const int current = open.top().second;
        open.pop();
        if (closed[current])
        {
            continue;
        }
        closed[current] = true;

        if (current == targetIndex)
        {
            break;
        }

        for (int directionIndex = 0; directionIndex < static_cast<int>(kDirections.size()); ++directionIndex)
        {
            if ((m_connections[current] & (1u << directionIndex)) == 0)
            {
                continue;
            }

            const int neighbor = GetNeighborIndex(current, directionIndex);
            if (neighbor < 0 || closed[neighbor])
            {
                continue;
            }

            const auto currentCenter = GetCellCenter(current);
            const auto neighborCenter = GetCellCenter(neighbor);
            const float stepCost = Distance2D(
                currentCenter.first,
                currentCenter.second,
                neighborCenter.first,
                neighborCenter.second);
            const float tentativeScore = gScore[current] + stepCost;
            if (tentativeScore >= gScore[neighbor])
            {
                continue;
            }

            cameFrom[neighbor] = current;
            gScore[neighbor] = tentativeScore;
            const auto targetCenter = GetCellCenter(targetIndex);
            const float heuristic = Distance2D(
                neighborCenter.first,
                neighborCenter.second,
                targetCenter.first,
                targetCenter.second);
            open.push({ tentativeScore + heuristic, neighbor });
        }
    }

    if (cameFrom[targetIndex] < 0)
    {
        return false;
    }

    std::vector<int> rawPath;
    for (int current = targetIndex; current >= 0; current = cameFrom[current])
    {
        rawPath.push_back(current);
        if (current == startIndex)
        {
            break;
        }
    }
    if (rawPath.empty() || rawPath.back() != startIndex)
    {
        return false;
    }
    std::reverse(rawPath.begin(), rawPath.end());

    size_t anchor = 0;
    while (anchor + 1 < rawPath.size())
    {
        size_t furthestVisible = anchor + 1;
        for (size_t candidate = anchor + 2; candidate < rawPath.size(); ++candidate)
        {
            if (HasLineOfSight(rawPath[anchor], rawPath[candidate]))
            {
                furthestVisible = candidate;
            }
        }

        if (furthestVisible + 1 == rawPath.size())
        {
            outWaypoints.push_back({ targetX, targetZ });
        }
        else
        {
            outWaypoints.push_back(GetCellCenter(rawPath[furthestVisible]));
        }
        anchor = furthestVisible;
    }

    return !outWaypoints.empty();
}

bool NavigationGrid::HasDirectPath(float startX, float startZ, float targetX, float targetZ) const
{
    if (!m_ready)
    {
        return false;
    }

    const int startIndex = FindNearestWalkableCell(startX, startZ);
    const int targetIndex = FindNearestWalkableCell(targetX, targetZ);
    return startIndex >= 0 && targetIndex >= 0 &&
        (startIndex == targetIndex || HasLineOfSight(startIndex, targetIndex));
}

bool NavigationGrid::LoadCollider(const std::string& relativeAssetPath, std::vector<Triangle>& outTriangles)
{
    const std::string assetPath = FindAssetPath(relativeAssetPath);
    if (assetPath.empty())
    {
        std::cout << "[Navigation] Collider asset missing: " << relativeAssetPath << std::endl;
        return false;
    }

    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(
        assetPath,
        aiProcess_Triangulate |
        aiProcess_PreTransformVertices |
        aiProcess_ConvertToLeftHanded);
    if (scene == nullptr || scene->mRootNode == nullptr)
    {
        std::cout << "[Navigation] Failed to import " << assetPath << ": " << importer.GetErrorString() << std::endl;
        return false;
    }

    for (unsigned int meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex)
    {
        const aiMesh* mesh = scene->mMeshes[meshIndex];
        if (mesh == nullptr || mesh->mVertices == nullptr)
        {
            continue;
        }

        for (unsigned int faceIndex = 0; faceIndex < mesh->mNumFaces; ++faceIndex)
        {
            const aiFace& face = mesh->mFaces[faceIndex];
            if (face.mNumIndices != 3)
            {
                continue;
            }

            const auto MakePoint = [&](unsigned int index)
            {
                const aiVector3D& vertex = mesh->mVertices[index];
                return Vec3{
                    vertex.x * kStage1MapScale,
                    vertex.y * kStage1MapScale,
                    vertex.z * kStage1MapScale
                };
            };

            outTriangles.push_back({
                MakePoint(face.mIndices[0]),
                MakePoint(face.mIndices[1]),
                MakePoint(face.mIndices[2])
            });
        }
    }

    return !outTriangles.empty();
}

void NavigationGrid::BuildGrid()
{
    float minX = std::numeric_limits<float>::infinity();
    float maxX = -std::numeric_limits<float>::infinity();
    float minZ = std::numeric_limits<float>::infinity();
    float maxZ = -std::numeric_limits<float>::infinity();

    for (const Triangle& triangle : m_floorTriangles)
    {
        for (const Vec3& point : { triangle.a, triangle.b, triangle.c })
        {
            minX = (std::min)(minX, point.x);
            maxX = (std::max)(maxX, point.x);
            minZ = (std::min)(minZ, point.z);
            maxZ = (std::max)(maxZ, point.z);
        }
    }

    if (!std::isfinite(minX) || !std::isfinite(minZ))
    {
        return;
    }

    m_minX = minX - m_cellSize;
    m_minZ = minZ - m_cellSize;
    m_width = static_cast<int>(std::ceil((maxX - m_minX) / m_cellSize)) + 1;
    m_height = static_cast<int>(std::ceil((maxZ - m_minZ) / m_cellSize)) + 1;
    if (m_width <= 0 || m_height <= 0)
    {
        return;
    }

    m_cells.assign(static_cast<size_t>(m_width * m_height), {});
    m_connections.assign(m_cells.size(), 0);

    for (int z = 0; z < m_height; ++z)
    {
        for (int x = 0; x < m_width; ++x)
        {
            const int index = z * m_width + x;
            const auto center = GetCellCenter(index);
            float floorY = 0.0f;
            if (GetFloorHeight(center.first, center.second, floorY))
            {
                m_cells[index].walkable = true;
                m_cells[index].floorY = floorY;
            }
        }
    }

    for (int cellIndex = 0; cellIndex < static_cast<int>(m_cells.size()); ++cellIndex)
    {
        if (!m_cells[cellIndex].walkable)
        {
            continue;
        }

        for (int directionIndex = 0; directionIndex < static_cast<int>(kDirections.size()); ++directionIndex)
        {
            const int neighbor = GetNeighborIndex(cellIndex, directionIndex);
            if (CanTraverse(cellIndex, neighbor))
            {
                m_connections[cellIndex] |= static_cast<std::uint8_t>(1u << directionIndex);
            }
        }
    }
}

bool NavigationGrid::GetFloorHeight(float x, float z, float& outHeight) const
{
    bool found = false;
    float bestHeight = -std::numeric_limits<float>::infinity();

    for (const Triangle& triangle : m_floorTriangles)
    {
        const float denominator =
            (triangle.b.z - triangle.c.z) * (triangle.a.x - triangle.c.x) +
            (triangle.c.x - triangle.b.x) * (triangle.a.z - triangle.c.z);
        if (std::fabs(denominator) <= kEpsilon)
        {
            continue;
        }

        const float u =
            ((triangle.b.z - triangle.c.z) * (x - triangle.c.x) +
                (triangle.c.x - triangle.b.x) * (z - triangle.c.z)) /
            denominator;
        const float v =
            ((triangle.c.z - triangle.a.z) * (x - triangle.c.x) +
                (triangle.a.x - triangle.c.x) * (z - triangle.c.z)) /
            denominator;
        const float w = 1.0f - u - v;
        if (u < -kEpsilon || v < -kEpsilon || w < -kEpsilon)
        {
            continue;
        }

        const float height = u * triangle.a.y + v * triangle.b.y + w * triangle.c.y;
        if (!found || height > bestHeight)
        {
            bestHeight = height;
            found = true;
        }
    }

    if (found)
    {
        outHeight = bestHeight;
    }
    return found;
}

bool NavigationGrid::CanTraverse(int fromIndex, int toIndex) const
{
    if (fromIndex < 0 || toIndex < 0 ||
        fromIndex >= static_cast<int>(m_cells.size()) || toIndex >= static_cast<int>(m_cells.size()) ||
        !m_cells[fromIndex].walkable || !m_cells[toIndex].walkable)
    {
        return false;
    }

    const int fromX = fromIndex % m_width;
    const int fromZ = fromIndex / m_width;
    const int toX = toIndex % m_width;
    const int toZ = toIndex / m_width;
    const int deltaX = toX - fromX;
    const int deltaZ = toZ - fromZ;
    if (std::abs(deltaX) > 1 || std::abs(deltaZ) > 1 || (deltaX == 0 && deltaZ == 0))
    {
        return false;
    }

    if (std::fabs(m_cells[toIndex].floorY - m_cells[fromIndex].floorY) > kMaxStepHeight)
    {
        return false;
    }

    if (deltaX != 0 && deltaZ != 0)
    {
        const int horizontal = fromZ * m_width + toX;
        const int vertical = toZ * m_width + fromX;
        if (!CanTraverse(fromIndex, horizontal) || !CanTraverse(fromIndex, vertical))
        {
            return false;
        }
    }

    const auto fromCenter = GetCellCenter(fromIndex);
    const auto toCenter = GetCellCenter(toIndex);
    const float dx = toCenter.first - fromCenter.first;
    const float dz = toCenter.second - fromCenter.second;
    const float length = std::sqrt(dx * dx + dz * dz);
    if (length <= kEpsilon)
    {
        return false;
    }

    const float sideX = -dz / length * kAgentRadius;
    const float sideZ = dx / length * kAgentRadius;
    const Vec3 start = { fromCenter.first, m_cells[fromIndex].floorY + kAgentRayHeight, fromCenter.second };
    const Vec3 end = { toCenter.first, m_cells[toIndex].floorY + kAgentRayHeight, toCenter.second };

    return !SegmentHitsWall(start, end) &&
        !SegmentHitsWall(Add(start, { sideX, 0.0f, sideZ }), Add(end, { sideX, 0.0f, sideZ })) &&
        !SegmentHitsWall(Add(start, { -sideX, 0.0f, -sideZ }), Add(end, { -sideX, 0.0f, -sideZ }));
}

bool NavigationGrid::SegmentHitsWall(const Vec3& start, const Vec3& end) const
{
    for (const Triangle& triangle : m_wallTriangles)
    {
        if (SegmentIntersectsTriangle(start, end, triangle))
        {
            return true;
        }
    }
    return false;
}

bool NavigationGrid::SegmentIntersectsTriangle(const Vec3& start, const Vec3& end, const Triangle& triangle) const
{
    const Vec3 direction = Subtract(end, start);
    const Vec3 edge1 = Subtract(triangle.b, triangle.a);
    const Vec3 edge2 = Subtract(triangle.c, triangle.a);
    const Vec3 pVector = Cross(direction, edge2);
    const float determinant = Dot(edge1, pVector);
    if (std::fabs(determinant) <= kEpsilon)
    {
        return false;
    }

    const float inverseDeterminant = 1.0f / determinant;
    const Vec3 tVector = Subtract(start, triangle.a);
    const float u = Dot(tVector, pVector) * inverseDeterminant;
    if (u < 0.0f || u > 1.0f)
    {
        return false;
    }

    const Vec3 qVector = Cross(tVector, edge1);
    const float v = Dot(direction, qVector) * inverseDeterminant;
    if (v < 0.0f || u + v > 1.0f)
    {
        return false;
    }

    const float t = Dot(edge2, qVector) * inverseDeterminant;
    return t >= 0.0f && t <= 1.0f;
}

int NavigationGrid::FindNearestWalkableCell(float x, float z) const
{
    if (!m_ready || m_width <= 0 || m_height <= 0)
    {
        return -1;
    }

    const int originX = static_cast<int>(std::floor((x - m_minX) / m_cellSize));
    const int originZ = static_cast<int>(std::floor((z - m_minZ) / m_cellSize));
    int nearest = -1;
    float nearestDistance = std::numeric_limits<float>::infinity();

    for (int radius = 0; radius <= kNearestCellSearchRadius; ++radius)
    {
        for (int offsetZ = -radius; offsetZ <= radius; ++offsetZ)
        {
            for (int offsetX = -radius; offsetX <= radius; ++offsetX)
            {
                if (radius > 0 && std::abs(offsetX) != radius && std::abs(offsetZ) != radius)
                {
                    continue;
                }

                const int cellX = originX + offsetX;
                const int cellZ = originZ + offsetZ;
                if (cellX < 0 || cellZ < 0 || cellX >= m_width || cellZ >= m_height)
                {
                    continue;
                }

                const int index = cellZ * m_width + cellX;
                if (!m_cells[index].walkable)
                {
                    continue;
                }

                const auto center = GetCellCenter(index);
                const float distance = Distance2D(x, z, center.first, center.second);
                if (distance < nearestDistance)
                {
                    nearestDistance = distance;
                    nearest = index;
                }
            }
        }

        if (nearest >= 0)
        {
            return nearest;
        }
    }

    return -1;
}

bool NavigationGrid::HasLineOfSight(int fromIndex, int toIndex) const
{
    if (fromIndex < 0 || toIndex < 0)
    {
        return false;
    }

    int x = fromIndex % m_width;
    int z = fromIndex / m_width;
    const int targetX = toIndex % m_width;
    const int targetZ = toIndex / m_width;
    const int deltaX = std::abs(targetX - x);
    const int deltaZ = std::abs(targetZ - z);
    const int stepX = x < targetX ? 1 : -1;
    const int stepZ = z < targetZ ? 1 : -1;
    int error = deltaX - deltaZ;
    int current = fromIndex;

    while (current != toIndex)
    {
        const int error2 = error * 2;
        int nextX = x;
        int nextZ = z;
        if (error2 > -deltaZ)
        {
            error -= deltaZ;
            nextX += stepX;
        }
        if (error2 < deltaX)
        {
            error += deltaX;
            nextZ += stepZ;
        }

        if (nextX < 0 || nextZ < 0 || nextX >= m_width || nextZ >= m_height)
        {
            return false;
        }

        const int next = nextZ * m_width + nextX;
        if (!AreConnected(current, next))
        {
            return false;
        }

        x = nextX;
        z = nextZ;
        current = next;
    }

    return true;
}

bool NavigationGrid::AreConnected(int fromIndex, int toIndex) const
{
    if (fromIndex < 0 || toIndex < 0 || fromIndex >= static_cast<int>(m_connections.size()))
    {
        return false;
    }

    for (int directionIndex = 0; directionIndex < static_cast<int>(kDirections.size()); ++directionIndex)
    {
        if ((m_connections[fromIndex] & (1u << directionIndex)) != 0 &&
            GetNeighborIndex(fromIndex, directionIndex) == toIndex)
        {
            return true;
        }
    }
    return false;
}

int NavigationGrid::GetNeighborIndex(int cellIndex, int directionIndex) const
{
    if (cellIndex < 0 || directionIndex < 0 || directionIndex >= static_cast<int>(kDirections.size()))
    {
        return -1;
    }

    const int x = cellIndex % m_width + kDirections[directionIndex].first;
    const int z = cellIndex / m_width + kDirections[directionIndex].second;
    if (x < 0 || z < 0 || x >= m_width || z >= m_height)
    {
        return -1;
    }
    return z * m_width + x;
}

std::pair<float, float> NavigationGrid::GetCellCenter(int cellIndex) const
{
    const int x = cellIndex % m_width;
    const int z = cellIndex / m_width;
    return {
        m_minX + (static_cast<float>(x) + 0.5f) * m_cellSize,
        m_minZ + (static_cast<float>(z) + 0.5f) * m_cellSize
    };
}

std::string NavigationGrid::FindAssetPath(const std::string& relativeAssetPath) const
{
    auto FileExists = [](const std::string& path)
    {
        return GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES;
    };

    std::string parentPath;
    for (int level = 0; level < 6; ++level)
    {
        // Assimp cannot consistently open a narrow absolute path containing Korean characters.
        // Keep the input relative and ASCII-only, as the client-side loader does.
        const std::string candidate = parentPath + "EclipseWalker\\Models\\" + relativeAssetPath;
        if (FileExists(candidate))
        {
            return candidate;
        }

        const std::string directCandidate = parentPath + "Models\\" + relativeAssetPath;
        if (FileExists(directCandidate))
        {
            return directCandidate;
        }

        parentPath += "..\\";
    }
    return {};
}
