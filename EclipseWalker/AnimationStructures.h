#pragma once

#include <DirectXMath.h>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

struct SkinnedVertex
{
    float Position[3] = {};
    float Normal[3] = {};
    float TexCoords[2] = {};
    float Tangent[3] = {};
    float Weights[4] = {};
    std::uint32_t BoneIndices[4] = {};
};

struct NodeData
{
    std::string Name;
    DirectX::XMFLOAT4X4 LocalTransform = {};
    std::vector<NodeData> Children;
};

struct BoneInfo
{
    int id = -1;
    DirectX::XMFLOAT4X4 OffsetMatrix = {};
};

struct KeyPosition
{
    float Position[3] = {};
    double TimeStamp = 0.0;
};

struct KeyRotation
{
    float Orientation[4] = {};
    double TimeStamp = 0.0;
};

struct KeyScale
{
    float Scale[3] = {};
    double TimeStamp = 0.0;
};

struct BoneAnimation
{
    std::string BoneName;
    std::vector<KeyPosition> Positions;
    std::vector<KeyRotation> Rotations;
    std::vector<KeyScale> Scales;
};

struct AnimationClip
{
    std::string Name;
    float Duration = 0.0f;
    float TicksPerSecond = 0.0f;
    bool LockRootMotionXZ = false;
    NodeData RootNode;
    std::vector<BoneAnimation> BoneAnimations;
};
