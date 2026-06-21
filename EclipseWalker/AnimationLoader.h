#pragma once

#include "AnimationStructures.h"
#include <DirectXMath.h>
#include <assimp/Importer.hpp>
#include <assimp/config.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <map>
#include <string>
#include <vector>

struct SkinnedMeshSubset
{
    unsigned int VertexStart = 0;
    unsigned int IndexStart = 0;
    unsigned int IndexCount = 0;
    unsigned int MaterialIndex = 0;
    std::string Name;
};

class AnimationLoader
{
public:
    AnimationLoader();
    ~AnimationLoader();

    bool Load(
        const std::string& filePath,
        const std::string& alias = "",
        bool loadAnimations = true,
        bool allowAnimationOnly = false);

    const std::vector<SkinnedVertex>& GetVertices() const { return m_Vertices; }
    const std::vector<unsigned int>& GetIndices() const { return m_Indices; }
    const std::vector<BoneInfo>& GetBoneInfo() const { return m_BoneInfo; }
    const std::map<std::string, unsigned int>& GetBoneMapping() const { return m_BoneMapping; }
    const std::vector<AnimationClip>& GetAnimations() const { return m_Animations; }
    const std::vector<SkinnedMeshSubset>& GetSubsets() const { return m_Subsets; }
    const NodeData& GetRootNode() const { return m_RootNode; }

public:
    std::vector<SkinnedVertex> m_Vertices;
    std::vector<unsigned int> m_Indices;
    std::vector<BoneInfo> m_BoneInfo;
    std::map<std::string, unsigned int> m_BoneMapping;
    unsigned int m_NumBones = 0;
    std::vector<AnimationClip> m_Animations;
    std::vector<SkinnedMeshSubset> m_Subsets;
    NodeData m_RootNode;

private:
    DirectX::XMFLOAT4X4 ConvertAssimpMatrix(const aiMatrix4x4& matrix) const;
    void Clear();
    void ProcessNode(aiNode* node, const aiScene* scene);
    void ProcessMesh(aiMesh* mesh, const aiScene* scene);
    void LoadBones(aiMesh* mesh, std::vector<SkinnedVertex>& vertices);
    void NormalizeBoneWeights(SkinnedVertex& vertex) const;
    void ReadHierarchyData(NodeData& dest, const aiNode* src);
    void ProcessAnimations(const aiScene* scene, const std::string& alias);
};
