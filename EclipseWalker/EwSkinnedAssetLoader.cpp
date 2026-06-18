#include "EwSkinnedAssetLoader.h"

#include "AnimationLoader.h"

#include <Windows.h>
#include <cstdint>
#include <fstream>
#include <limits>
#include <sstream>
#include <type_traits>

namespace
{
    constexpr std::uint32_t kMagic = 0x4B535745; // EWSK
    constexpr std::uint32_t kVersion = 1;
    constexpr std::uint32_t kMaxVertices = 10'000'000;
    constexpr std::uint32_t kMaxIndices = 30'000'000;
    constexpr std::uint32_t kMaxBones = 256;
    constexpr std::uint32_t kMaxSubsets = 100'000;
    constexpr std::uint32_t kMaxClips = 1'000;
    constexpr std::uint32_t kMaxChannels = 10'000;
    constexpr std::uint32_t kMaxKeys = 1'000'000;
    constexpr std::uint32_t kMaxStringBytes = 1'048'576;
    constexpr int kMaxHierarchyDepth = 512;

    class BinaryReader
    {
    public:
        explicit BinaryReader(const std::string& filePath)
            : m_Stream(filePath, std::ios::binary)
        {
        }

        bool IsOpen() const { return m_Stream.is_open(); }
        bool IsValid() const { return m_Valid && static_cast<bool>(m_Stream); }

        template <typename T>
        bool Read(T& value)
        {
            static_assert(std::is_trivially_copyable_v<T>);
            return ReadBytes(&value, sizeof(T));
        }

        bool ReadBytes(void* destination, std::size_t byteCount)
        {
            if (!m_Valid || byteCount > static_cast<std::size_t>((std::numeric_limits<std::streamsize>::max)()))
            {
                m_Valid = false;
                return false;
            }

            m_Stream.read(static_cast<char*>(destination), static_cast<std::streamsize>(byteCount));
            if (!m_Stream)
            {
                m_Valid = false;
                return false;
            }
            return true;
        }

        bool ReadString(std::string& value)
        {
            std::uint32_t length = 0;
            if (!Read(length) || length > kMaxStringBytes)
            {
                m_Valid = false;
                return false;
            }

            value.resize(length);
            return length == 0 || ReadBytes(value.data(), length);
        }

    private:
        std::ifstream m_Stream;
        bool m_Valid = true;
    };

    bool ReadMatrix(BinaryReader& reader, DirectX::XMFLOAT4X4& matrix)
    {
        return reader.ReadBytes(&matrix, sizeof(matrix));
    }

    bool ReadHierarchy(BinaryReader& reader, NodeData& node, int depth)
    {
        if (depth > kMaxHierarchyDepth ||
            !reader.ReadString(node.Name) ||
            !ReadMatrix(reader, node.LocalTransform))
        {
            return false;
        }

        std::uint32_t childCount = 0;
        if (!reader.Read(childCount) || childCount > kMaxChannels)
        {
            return false;
        }

        node.Children.resize(childCount);
        for (NodeData& child : node.Children)
        {
            if (!ReadHierarchy(reader, child, depth + 1))
            {
                return false;
            }
        }
        return true;
    }

    bool ReadAnimationClip(BinaryReader& reader, const NodeData& rootNode, AnimationClip& clip)
    {
        std::uint8_t lockRootMotion = 0;
        std::uint32_t channelCount = 0;
        if (!reader.ReadString(clip.Name) ||
            !reader.Read(clip.Duration) ||
            !reader.Read(clip.TicksPerSecond) ||
            !reader.Read(lockRootMotion) ||
            !reader.Read(channelCount) ||
            channelCount > kMaxChannels)
        {
            return false;
        }

        clip.LockRootMotionXZ = lockRootMotion != 0;
        clip.RootNode = rootNode;
        clip.BoneAnimations.resize(channelCount);

        for (BoneAnimation& channel : clip.BoneAnimations)
        {
            if (!reader.ReadString(channel.BoneName))
            {
                return false;
            }

            std::uint32_t positionCount = 0;
            if (!reader.Read(positionCount) || positionCount > kMaxKeys)
            {
                return false;
            }
            channel.Positions.resize(positionCount);
            for (KeyPosition& key : channel.Positions)
            {
                if (!reader.Read(key.Position[0]) || !reader.Read(key.Position[1]) ||
                    !reader.Read(key.Position[2]) || !reader.Read(key.TimeStamp))
                {
                    return false;
                }
            }

            std::uint32_t rotationCount = 0;
            if (!reader.Read(rotationCount) || rotationCount > kMaxKeys)
            {
                return false;
            }
            channel.Rotations.resize(rotationCount);
            for (KeyRotation& key : channel.Rotations)
            {
                if (!reader.Read(key.Orientation[0]) || !reader.Read(key.Orientation[1]) ||
                    !reader.Read(key.Orientation[2]) || !reader.Read(key.Orientation[3]) ||
                    !reader.Read(key.TimeStamp))
                {
                    return false;
                }
            }

            std::uint32_t scaleCount = 0;
            if (!reader.Read(scaleCount) || scaleCount > kMaxKeys)
            {
                return false;
            }
            channel.Scales.resize(scaleCount);
            for (KeyScale& key : channel.Scales)
            {
                if (!reader.Read(key.Scale[0]) || !reader.Read(key.Scale[1]) ||
                    !reader.Read(key.Scale[2]) || !reader.Read(key.TimeStamp))
                {
                    return false;
                }
            }
        }

        return true;
    }

    void ResetLoader(AnimationLoader& loader)
    {
        loader.m_Vertices.clear();
        loader.m_Indices.clear();
        loader.m_BoneInfo.clear();
        loader.m_BoneMapping.clear();
        loader.m_NumBones = 0;
        loader.m_Animations.clear();
        loader.m_Subsets.clear();
        loader.m_RootNode = {};
    }
}

bool EwSkinnedAssetLoader::Load(const std::string& filePath, AnimationLoader& destination)
{
    BinaryReader reader(filePath);
    if (!reader.IsOpen())
    {
        OutputDebugStringA(("[EWSK] Failed to open: " + filePath + "\n").c_str());
        return false;
    }

    std::uint32_t magic = 0, version = 0, vertexCount = 0, indexCount = 0;
    std::uint32_t boneCount = 0, subsetCount = 0, clipCount = 0;
    if (!reader.Read(magic) || !reader.Read(version) ||
        !reader.Read(vertexCount) || !reader.Read(indexCount) ||
        !reader.Read(boneCount) || !reader.Read(subsetCount) || !reader.Read(clipCount) ||
        magic != kMagic || version != kVersion ||
        vertexCount > kMaxVertices || indexCount > kMaxIndices ||
        boneCount == 0 || boneCount > kMaxBones ||
        subsetCount > kMaxSubsets || clipCount > kMaxClips)
    {
        OutputDebugStringA(("[EWSK] Invalid header: " + filePath + "\n").c_str());
        return false;
    }

    AnimationLoader loaded;
    ResetLoader(loaded);
    loaded.m_Vertices.resize(vertexCount);
    for (SkinnedVertex& vertex : loaded.m_Vertices)
    {
        if (!reader.ReadBytes(vertex.Position, sizeof(vertex.Position)) ||
            !reader.ReadBytes(vertex.Normal, sizeof(vertex.Normal)) ||
            !reader.ReadBytes(vertex.TexCoords, sizeof(vertex.TexCoords)) ||
            !reader.ReadBytes(vertex.Tangent, sizeof(vertex.Tangent)) ||
            !reader.ReadBytes(vertex.Weights, sizeof(vertex.Weights)) ||
            !reader.ReadBytes(vertex.BoneIndices, sizeof(vertex.BoneIndices)))
        {
            return false;
        }
    }

    loaded.m_Indices.resize(indexCount);
    if (indexCount > 0 &&
        !reader.ReadBytes(loaded.m_Indices.data(), sizeof(std::uint32_t) * indexCount))
    {
        return false;
    }

    loaded.m_BoneInfo.resize(boneCount);
    for (std::uint32_t index = 0; index < boneCount; ++index)
    {
        std::int32_t id = -1;
        BoneInfo& bone = loaded.m_BoneInfo[index];
        if (!reader.Read(id) || !reader.ReadString(bone.Name) ||
            !ReadMatrix(reader, bone.OffsetMatrix) || id < 0 ||
            static_cast<std::uint32_t>(id) >= boneCount)
        {
            return false;
        }
        bone.id = id;
        loaded.m_BoneMapping.try_emplace(bone.Name, static_cast<unsigned int>(index));
    }
    loaded.m_NumBones = boneCount;

    loaded.m_Subsets.resize(subsetCount);
    for (SkinnedMeshSubset& subset : loaded.m_Subsets)
    {
        if (!reader.Read(subset.VertexStart) || !reader.Read(subset.IndexStart) ||
            !reader.Read(subset.IndexCount) || !reader.Read(subset.MaterialIndex) ||
            !reader.ReadString(subset.Name))
        {
            return false;
        }
    }

    if (!ReadHierarchy(reader, loaded.m_RootNode, 0))
    {
        return false;
    }

    loaded.m_Animations.resize(clipCount);
    for (AnimationClip& clip : loaded.m_Animations)
    {
        if (!ReadAnimationClip(reader, loaded.m_RootNode, clip))
        {
            return false;
        }
    }

    if (!reader.IsValid())
    {
        return false;
    }

    destination = std::move(loaded);
    std::ostringstream log;
    log << "[EWSK] Loaded " << filePath
        << " vertices=" << destination.m_Vertices.size()
        << " indices=" << destination.m_Indices.size()
        << " bones=" << destination.m_BoneInfo.size()
        << " clips=" << destination.m_Animations.size() << "\n";
    OutputDebugStringA(log.str().c_str());
    return true;
}
