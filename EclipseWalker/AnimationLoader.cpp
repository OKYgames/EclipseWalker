#include "AnimationLoader.h"

#include <Windows.h>
#include <algorithm>
#include <sstream>

AnimationLoader::AnimationLoader()
{
}

AnimationLoader::~AnimationLoader()
{
}

DirectX::XMFLOAT4X4 AnimationLoader::ConvertAssimpMatrix(const aiMatrix4x4& matrix) const
{
    DirectX::XMFLOAT4X4 out;
    out._11 = matrix.a1; out._12 = matrix.b1; out._13 = matrix.c1; out._14 = matrix.d1;
    out._21 = matrix.a2; out._22 = matrix.b2; out._23 = matrix.c2; out._24 = matrix.d2;
    out._31 = matrix.a3; out._32 = matrix.b3; out._33 = matrix.c3; out._34 = matrix.d3;
    out._41 = matrix.a4; out._42 = matrix.b4; out._43 = matrix.c4; out._44 = matrix.d4;
    return out;
}

void AnimationLoader::Clear()
{
    m_Vertices.clear();
    m_Indices.clear();
    m_BoneInfo.clear();
    m_BoneMapping.clear();
    m_Animations.clear();
    m_Subsets.clear();
    m_RootNode = {};
    m_NumBones = 0;
}

bool AnimationLoader::Load(const std::string& filePath, const std::string& alias, bool loadAnimations)
{
    Assimp::Importer importer;
    // ============================
    // 인생의 전환점 개쩌는 코드임 건들 ㄴㄴ
    importer.SetPropertyBool(AI_CONFIG_IMPORT_FBX_PRESERVE_PIVOTS, false);  // 인생의 전환점 개쩌는 코드임 건들 ㄴㄴ
    // FBX importer가 pivot/pre-rotation을 별도 helper node
    // (e.g. _$AssimpFbx$_Translation, _$AssimpFbx$_PreRotation)로 보존하면
    // skeletal hierarchy가 꼬여 애니메이션이 틀어질 수 있다.
    // false로 두면 가능한 경우 pivot/offset을 내부적으로 평가해
    // 더 단순한 hierarchy로 가져온다.
    // ============================

    const aiScene* scene = importer.ReadFile(filePath,
        aiProcess_Triangulate |
        aiProcess_ConvertToLeftHanded |
        aiProcess_LimitBoneWeights |
        aiProcess_JoinIdenticalVertices |
        aiProcess_FlipUVs |
        aiProcess_GenSmoothNormals |
        aiProcess_CalcTangentSpace);

    if (!scene || !scene->mRootNode)
    {
        std::ostringstream oss;
        oss << "ERROR::ANIMATION_LOADER::LOAD_FAILED: " << importer.GetErrorString() << "\n";
        OutputDebugStringA(oss.str().c_str());
        return false;
    }

    if (!scene->HasMeshes() && !scene->HasAnimations())
    {
        std::ostringstream oss;
        oss << "ERROR::ANIMATION_LOADER::NO_MESH_OR_ANIMATION_DATA: " << filePath << "\n";
        OutputDebugStringA(oss.str().c_str());
        return false;
    }

    Clear();
    ReadHierarchyData(m_RootNode, scene->mRootNode);
    ProcessNode(scene->mRootNode, scene);

    if (loadAnimations && scene->HasAnimations())
    {
        ProcessAnimations(scene, alias);
    }

    std::ostringstream oss;
    oss << "[AnimationLoader] Loaded " << filePath
        << " vertices=" << m_Vertices.size()
        << " indices=" << m_Indices.size()
        << " bones=" << m_NumBones
        << " clips=" << m_Animations.size()
        << "\n";
    OutputDebugStringA(oss.str().c_str());

    return true;
}

void AnimationLoader::ProcessNode(aiNode* node, const aiScene* scene)
{
    for (unsigned int i = 0; i < node->mNumMeshes; ++i)
    {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        ProcessMesh(mesh, scene);
    }

    for (unsigned int i = 0; i < node->mNumChildren; ++i)
    {
        ProcessNode(node->mChildren[i], scene);
    }
}

void AnimationLoader::ProcessMesh(aiMesh* mesh, const aiScene* scene)
{
    UNREFERENCED_PARAMETER(scene);

    const size_t baseVertex = m_Vertices.size();
    SkinnedMeshSubset subset;
    subset.VertexStart = static_cast<unsigned int>(baseVertex);
    subset.IndexStart = static_cast<unsigned int>(m_Indices.size());
    subset.IndexCount = mesh->mNumFaces * 3;
    subset.MaterialIndex = mesh->mMaterialIndex;
    subset.Name = mesh->mName.C_Str();

    for (unsigned int i = 0; i < mesh->mNumVertices; ++i)
    {
        SkinnedVertex vertex;

        vertex.Position[0] = mesh->mVertices[i].x;
        vertex.Position[1] = mesh->mVertices[i].y;
        vertex.Position[2] = mesh->mVertices[i].z;

        if (mesh->HasNormals())
        {
            vertex.Normal[0] = mesh->mNormals[i].x;
            vertex.Normal[1] = mesh->mNormals[i].y;
            vertex.Normal[2] = mesh->mNormals[i].z;
        }

        if (mesh->mTextureCoords[0])
        {
            vertex.TexCoords[0] = mesh->mTextureCoords[0][i].x;
            vertex.TexCoords[1] = mesh->mTextureCoords[0][i].y;
        }

        if (mesh->HasTangentsAndBitangents())
        {
            vertex.Tangent[0] = mesh->mTangents[i].x;
            vertex.Tangent[1] = mesh->mTangents[i].y;
            vertex.Tangent[2] = mesh->mTangents[i].z;
        }
        else
        {
            vertex.Tangent[0] = 1.0f;
        }

        m_Vertices.push_back(vertex);
    }

    for (unsigned int i = 0; i < mesh->mNumFaces; ++i)
    {
        const aiFace& face = mesh->mFaces[i];
        for (unsigned int j = 0; j < face.mNumIndices; ++j)
        {
            m_Indices.push_back(face.mIndices[j] + static_cast<unsigned int>(baseVertex));
        }
    }

    if (mesh->HasBones())
    {
        LoadBones(mesh, m_Vertices);
    }

    for (size_t i = baseVertex; i < m_Vertices.size(); ++i)
    {
        NormalizeBoneWeights(m_Vertices[i]);
    }

    m_Subsets.push_back(std::move(subset));
}

void AnimationLoader::LoadBones(aiMesh* mesh, std::vector<SkinnedVertex>& vertices)
{
    const size_t baseVertex = m_Vertices.size() - mesh->mNumVertices;

    for (unsigned int i = 0; i < mesh->mNumBones; ++i)
    {
        unsigned int boneIndex = 0;
        const std::string boneName(mesh->mBones[i]->mName.data);

        auto mappingIt = m_BoneMapping.find(boneName);
        if (mappingIt == m_BoneMapping.end())
        {
            boneIndex = m_NumBones++;

            BoneInfo boneInfo;
            boneInfo.id = static_cast<int>(boneIndex);
            boneInfo.OffsetMatrix = ConvertAssimpMatrix(mesh->mBones[i]->mOffsetMatrix);

            m_BoneInfo.push_back(boneInfo);
            m_BoneMapping[boneName] = boneIndex;
        }
        else
        {
            boneIndex = mappingIt->second;
        }

        aiBone* bone = mesh->mBones[i];
        for (unsigned int j = 0; j < bone->mNumWeights; ++j)
        {
            const unsigned int vertexID = bone->mWeights[j].mVertexId;
            const float weight = bone->mWeights[j].mWeight;
            const size_t globalVertexID = baseVertex + vertexID;

            if (globalVertexID >= vertices.size())
            {
                continue;
            }

            for (int slot = 0; slot < 4; ++slot)
            {
                if (vertices[globalVertexID].Weights[slot] == 0.0f)
                {
                    vertices[globalVertexID].Weights[slot] = weight;
                    vertices[globalVertexID].BoneIndices[slot] = boneIndex;
                    break;
                }
            }
        }
    }
}

void AnimationLoader::NormalizeBoneWeights(SkinnedVertex& vertex) const
{
    float totalWeight = 0.0f;
    for (float weight : vertex.Weights)
    {
        totalWeight += weight;
    }

    if (totalWeight <= 0.0001f)
    {
        vertex.Weights[0] = 1.0f;
        vertex.BoneIndices[0] = 0;
        return;
    }

    for (float& weight : vertex.Weights)
    {
        weight /= totalWeight;
    }
}

void AnimationLoader::ReadHierarchyData(NodeData& dest, const aiNode* src)
{
    dest.Name = src->mName.C_Str();
    dest.LocalTransform = ConvertAssimpMatrix(src->mTransformation);

    dest.Children.resize(src->mNumChildren);
    for (unsigned int i = 0; i < src->mNumChildren; ++i)
    {
        ReadHierarchyData(dest.Children[i], src->mChildren[i]);
    }
}

void AnimationLoader::ProcessAnimations(const aiScene* scene, const std::string& alias)
{
    for (unsigned int i = 0; i < scene->mNumAnimations; ++i)
    {
        aiAnimation* srcAnim = scene->mAnimations[i];
        AnimationClip destAnim;

        destAnim.Name = alias.empty() ? srcAnim->mName.C_Str() : alias;
        destAnim.Duration = static_cast<float>(srcAnim->mDuration);
        destAnim.TicksPerSecond = static_cast<float>(srcAnim->mTicksPerSecond);

        if (destAnim.TicksPerSecond <= 0.0f)
        {
            destAnim.TicksPerSecond = 24.0f;
        }

        destAnim.RootNode = m_RootNode;

        for (unsigned int j = 0; j < srcAnim->mNumChannels; ++j)
        {
            aiNodeAnim* channel = srcAnim->mChannels[j];
            BoneAnimation boneAnim;
            boneAnim.BoneName = channel->mNodeName.C_Str();

            for (unsigned int k = 0; k < channel->mNumPositionKeys; ++k)
            {
                KeyPosition data;
                data.TimeStamp = channel->mPositionKeys[k].mTime;
                data.Position[0] = channel->mPositionKeys[k].mValue.x;
                data.Position[1] = channel->mPositionKeys[k].mValue.y;
                data.Position[2] = channel->mPositionKeys[k].mValue.z;
                boneAnim.Positions.push_back(data);
            }

            for (unsigned int k = 0; k < channel->mNumRotationKeys; ++k)
            {
                KeyRotation data;
                data.TimeStamp = channel->mRotationKeys[k].mTime;
                data.Orientation[0] = channel->mRotationKeys[k].mValue.x;
                data.Orientation[1] = channel->mRotationKeys[k].mValue.y;
                data.Orientation[2] = channel->mRotationKeys[k].mValue.z;
                data.Orientation[3] = channel->mRotationKeys[k].mValue.w;
                boneAnim.Rotations.push_back(data);
            }

            for (unsigned int k = 0; k < channel->mNumScalingKeys; ++k)
            {
                KeyScale data;
                data.TimeStamp = channel->mScalingKeys[k].mTime;
                data.Scale[0] = channel->mScalingKeys[k].mValue.x;
                data.Scale[1] = channel->mScalingKeys[k].mValue.y;
                data.Scale[2] = channel->mScalingKeys[k].mValue.z;
                boneAnim.Scales.push_back(data);
            }

            destAnim.BoneAnimations.push_back(boneAnim);
        }

        m_Animations.push_back(destAnim);
    }
}
