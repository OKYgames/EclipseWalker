#pragma once

#include <Windows.h>
#include <vector>
#include <string>
#include <iostream>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem> 
#include <limits>
#include <DirectXMath.h>
#include "Vertices.h" 

// Assimp 헤더
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

using namespace DirectX;

struct Subset
{
    UINT Id;
    UINT VertexStart;    // 전체 정점 버퍼에서 이 덩어리의 시작점
    UINT IndexStart;     // 전체 인덱스 버퍼에서 이 덩어리의 시작점
    UINT IndexCount;     // 이 덩어리가 사용하는 인덱스 개수
    UINT MaterialIndex;  // 이 덩어리가 사용하는 재질(텍스처) 번호
    std::string Name;    // 메쉬 이름 
};

struct MapMeshData
{
    std::vector<Vertex> Vertices;
    std::vector<std::uint32_t> Indices;
    std::vector<Subset> Subsets;
};

struct NamedMeshBounds
{
    std::string Name;
    DirectX::XMFLOAT3 Center;
    DirectX::XMFLOAT3 Extents;
};

struct ImportedMaterialInfo
{
    std::string DiffuseTextureName;
    DirectX::XMFLOAT3 FresnelR0 = { 0.05f, 0.05f, 0.05f };
    float Roughness = 0.8f;
    float MetallicFactor = 0.0f;
    bool HasFresnelR0 = false;
    bool HasRoughness = false;
    bool HasMetallicFactor = false;
};

class ModelLoader
{
public:
    // 1. FBX 파일을 읽어서 정점/인덱스/서브셋 데이터 추출
    static bool Load(const std::string& filename, MapMeshData& outData)
    {
        Assimp::Importer importer;

        // 옵션 설명:
        // - Triangulate: 사각형 면을 삼각형으로 쪼갬
        // - FlipUVs: 텍스처 좌표계 뒤집기 
        // - GenSmoothNormals: 법선 벡터 생성
        // - PreTransformVertices: 복잡한 노드 구조를 무시하고 좌표를 다 합쳐버림 (맵 로딩에 유리)
        // - ConvertToLeftHanded: DirectX 왼손 좌표계로 변환
        const aiScene* scene = importer.ReadFile(filename,
            aiProcess_Triangulate |
            aiProcess_FlipUVs |
            aiProcess_GenSmoothNormals |
            aiProcess_PreTransformVertices |
            aiProcess_ConvertToLeftHanded |
            aiProcess_CalcTangentSpace);

        if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
        {
            OutputDebugStringA("ERROR::ASSIMP::LOAD_FAILED\n");
            return false;
        }

        ProcessNode(scene->mRootNode, scene, outData);
        return true;
    }

    static std::vector<std::string> LoadTextureNames(const std::string& filename)
    {
        Assimp::Importer importer;
        const aiScene* scene = importer.ReadFile(filename, 0);

        std::vector<std::string> textureNames;
        if (!scene) return textureNames;

        // 재질 개수만큼 리사이즈
        textureNames.resize(scene->mNumMaterials);

        for (UINT i = 0; i < scene->mNumMaterials; ++i)
        {
            aiMaterial* mat = scene->mMaterials[i];
            aiString path;

            if (mat->GetTexture(aiTextureType_DIFFUSE, 0, &path) == AI_SUCCESS)
            {
                textureNames[i] = GetFileNameFromPath(path.C_Str());
            }
            else
            {
                textureNames[i] = ""; 
            }
        }
        return textureNames;
    }

    static std::vector<ImportedMaterialInfo> LoadMaterialInfos(const std::string& filename)
    {
        Assimp::Importer importer;
        const aiScene* scene = importer.ReadFile(filename, 0);

        std::vector<ImportedMaterialInfo> materialInfos;
        if (!scene)
        {
            return materialInfos;
        }

        materialInfos.resize(scene->mNumMaterials);

        for (UINT i = 0; i < scene->mNumMaterials; ++i)
        {
            aiMaterial* mat = scene->mMaterials[i];
            ImportedMaterialInfo& info = materialInfos[i];

            aiString diffusePath;
            if (mat->GetTexture(aiTextureType_DIFFUSE, 0, &diffusePath) == AI_SUCCESS)
            {
                info.DiffuseTextureName = GetFileNameFromPath(diffusePath.C_Str());
            }

            aiColor3D specularColor(0.0f, 0.0f, 0.0f);
            if (mat->Get(AI_MATKEY_COLOR_SPECULAR, specularColor) == AI_SUCCESS)
            {
                info.FresnelR0 =
                {
                    ClampFloat(specularColor.r, 0.0f, 1.0f),
                    ClampFloat(specularColor.g, 0.0f, 1.0f),
                    ClampFloat(specularColor.b, 0.0f, 1.0f)
                };
                info.HasFresnelR0 = true;
            }

            float roughness = 0.0f;
            if (mat->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness) == AI_SUCCESS)
            {
                info.Roughness = ClampFloat(roughness, 0.05f, 1.0f);
                info.HasRoughness = true;
            }

            float metallic = 0.0f;
            if (mat->Get(AI_MATKEY_METALLIC_FACTOR, metallic) == AI_SUCCESS)
            {
                info.MetallicFactor = ClampFloat(metallic, 0.0f, 1.0f);
                info.HasMetallicFactor = true;
            }

            float shininess = 0.0f;
            if (!info.HasRoughness && mat->Get(AI_MATKEY_SHININESS, shininess) == AI_SUCCESS && shininess > 0.0f)
            {
                info.Roughness = ClampFloat(std::sqrt(2.0f / (shininess + 2.0f)), 0.05f, 1.0f);
                info.HasRoughness = true;
            }
        }

        return materialInfos;
    }

    static std::vector<NamedMeshBounds> LoadNamedMeshBounds(const std::string& filename, const std::string& nameFilter)
    {
        Assimp::Importer importer;
        const aiScene* scene = importer.ReadFile(
            filename,
            aiProcess_Triangulate |
            aiProcess_ConvertToLeftHanded);

        std::vector<NamedMeshBounds> meshBounds;
        if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
        {
            return meshBounds;
        }

        const aiMatrix4x4 identityTransform;
        CollectNamedMeshBounds(scene->mRootNode, scene, identityTransform, nameFilter, meshBounds);
        return meshBounds;
    }

private:
    static float ClampFloat(float value, float minValue, float maxValue)
    {
        return (std::max)(minValue, (std::min)(value, maxValue));
    }

    // 경로에서 파일명만 남기는 헬퍼 함수
    // 예: "C:\Users\Kim\Desktop\Textures\Wall.png" -> "Wall.png"
    static std::string GetFileNameFromPath(const std::string& fullPath)
    {
        std::filesystem::path path(fullPath);
        return path.filename().string();
    }

    static void ProcessNode(aiNode* node, const aiScene* scene, MapMeshData& outData)
    {
        // 현재 노드의 모든 메쉬 처리
        for (unsigned int i = 0; i < node->mNumMeshes; i++)
        {
            aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
            ProcessMesh(mesh, scene, node->mName.C_Str(), outData);
        }

        // 자식 노드 재귀 호출
        for (unsigned int i = 0; i < node->mNumChildren; i++)
        {
            ProcessNode(node->mChildren[i], scene, outData);
        }
    }

    static bool IsGenericSubsetName(const std::string& name)
    {
        if (name.empty())
        {
            return true;
        }

        std::string lower = name;
        std::transform(
            lower.begin(),
            lower.end(),
            lower.begin(),
            [](unsigned char c)
            {
                return static_cast<char>(std::tolower(c));
            });

        return lower == "scene";
    }

    static bool ContainsInsensitive(const std::string& text, const std::string& needle)
    {
        if (needle.empty())
        {
            return true;
        }

        return std::search(
            text.begin(),
            text.end(),
            needle.begin(),
            needle.end(),
            [](unsigned char lhs, unsigned char rhs)
            {
                return std::tolower(lhs) == std::tolower(rhs);
            }) != text.end();
    }

    static std::string ResolveMeshName(aiMesh* mesh, const std::string& nodeName)
    {
        const std::string meshName = mesh->mName.C_Str();
        return IsGenericSubsetName(meshName) && !nodeName.empty()
            ? nodeName
            : meshName;
    }

    static bool TryComputeTransformedMeshBounds(
        aiMesh* mesh,
        const aiMatrix4x4& worldTransform,
        DirectX::XMFLOAT3& outCenter,
        DirectX::XMFLOAT3& outExtents)
    {
        if (mesh == nullptr || mesh->mNumVertices == 0)
        {
            return false;
        }

        const float maxFloat = (std::numeric_limits<float>::max)();
        DirectX::XMFLOAT3 minPos{ maxFloat, maxFloat, maxFloat };
        DirectX::XMFLOAT3 maxPos{ -maxFloat, -maxFloat, -maxFloat };

        for (unsigned int i = 0; i < mesh->mNumVertices; ++i)
        {
            const aiVector3D transformed = worldTransform * mesh->mVertices[i];
            minPos.x = (std::min)(minPos.x, transformed.x);
            minPos.y = (std::min)(minPos.y, transformed.y);
            minPos.z = (std::min)(minPos.z, transformed.z);
            maxPos.x = (std::max)(maxPos.x, transformed.x);
            maxPos.y = (std::max)(maxPos.y, transformed.y);
            maxPos.z = (std::max)(maxPos.z, transformed.z);
        }

        outCenter =
        {
            (minPos.x + maxPos.x) * 0.5f,
            (minPos.y + maxPos.y) * 0.5f,
            (minPos.z + maxPos.z) * 0.5f
        };
        outExtents =
        {
            (maxPos.x - minPos.x) * 0.5f,
            (maxPos.y - minPos.y) * 0.5f,
            (maxPos.z - minPos.z) * 0.5f
        };
        return true;
    }

    static void CollectNamedMeshBounds(
        aiNode* node,
        const aiScene* scene,
        const aiMatrix4x4& parentTransform,
        const std::string& nameFilter,
        std::vector<NamedMeshBounds>& outBounds)
    {
        if (node == nullptr)
        {
            return;
        }

        const aiMatrix4x4 worldTransform = parentTransform * node->mTransformation;
        const std::string nodeName = node->mName.C_Str();

        for (unsigned int i = 0; i < node->mNumMeshes; ++i)
        {
            aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
            const std::string resolvedName = ResolveMeshName(mesh, nodeName);
            if (!ContainsInsensitive(resolvedName, nameFilter))
            {
                continue;
            }

            DirectX::XMFLOAT3 center{};
            DirectX::XMFLOAT3 extents{};
            if (!TryComputeTransformedMeshBounds(mesh, worldTransform, center, extents))
            {
                continue;
            }

            outBounds.push_back({ resolvedName, center, extents });
        }

        for (unsigned int i = 0; i < node->mNumChildren; ++i)
        {
            CollectNamedMeshBounds(node->mChildren[i], scene, worldTransform, nameFilter, outBounds);
        }
    }

    static void ProcessMesh(aiMesh* mesh, const aiScene* scene, const std::string& nodeName, MapMeshData& outData)
    {
        Subset subset;
        subset.Id = (UINT)outData.Subsets.size();
        subset.VertexStart = (UINT)outData.Vertices.size(); // 현재 쌓인 정점 개수가 시작점
        subset.IndexStart = (UINT)outData.Indices.size();   // 현재 쌓인 인덱스 개수가 시작점
        subset.IndexCount = mesh->mNumFaces * 3;
        subset.MaterialIndex = mesh->mMaterialIndex;        // Assimp가 알려준 재질 번호
        subset.Name = ResolveMeshName(mesh, nodeName);

        // 1. 정점 추출
        for (unsigned int i = 0; i < mesh->mNumVertices; i++)
        {
            Vertex v;
            v.Pos.x = mesh->mVertices[i].x;
            v.Pos.y = mesh->mVertices[i].y;
            v.Pos.z = mesh->mVertices[i].z;

            if (mesh->HasNormals())
            {
                v.Normal.x = mesh->mNormals[i].x;
                v.Normal.y = mesh->mNormals[i].y;
                v.Normal.z = mesh->mNormals[i].z;
            }

            if (mesh->mTextureCoords[0])
            {
                v.TexC.x = mesh->mTextureCoords[0][i].x;
                v.TexC.y = mesh->mTextureCoords[0][i].y;
            }
            else
            {
                v.TexC = { 0.0f, 0.0f };
            }

            if (mesh->HasTangentsAndBitangents())
            {
                v.TangentU.x = mesh->mTangents[i].x;
                v.TangentU.y = mesh->mTangents[i].y;
                v.TangentU.z = mesh->mTangents[i].z;
            }
            else
            {
                // 없으면 X축을 기본값으로 설정 
                v.TangentU = { 1.0f, 0.0f, 0.0f };
            }

            outData.Vertices.push_back(v);
        }

        // 2. 인덱스 추출
        for (unsigned int i = 0; i < mesh->mNumFaces; i++)
        {
            aiFace face = mesh->mFaces[i];
            for (unsigned int j = 0; j < face.mNumIndices; j++)
            {
                // 전역 버퍼에 합치므로, 현재 서브셋의 VertexStart만큼 더해줘야 함
                outData.Indices.push_back(subset.VertexStart + face.mIndices[j]);
            }
        }

        outData.Subsets.push_back(subset);
    }
};
