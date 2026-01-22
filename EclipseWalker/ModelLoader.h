#pragma once

#include <vector>
#include <string>
#include <iostream>
#include <filesystem> 
#include <DirectXMath.h>
#include "Vertices.h" 

// Assimp 헤더
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

using namespace DirectX;

// 덩어리(Subset) 정보
struct Subset
{
    UINT Id;
    UINT VertexStart;    // 전체 정점 버퍼에서 이 덩어리의 시작점
    UINT IndexStart;     // 전체 인덱스 버퍼에서 이 덩어리의 시작점
    UINT IndexCount;     // 이 덩어리가 사용하는 인덱스 개수
    UINT MaterialIndex;  // 이 덩어리가 사용하는 재질(텍스처) 번호
    std::string Name;    // 메쉬 이름 (디버깅용)
};

// 맵 데이터 전체를 담는 통
struct MapMeshData
{
    std::vector<Vertex> Vertices;
    std::vector<std::uint32_t> Indices;
    std::vector<Subset> Subsets;
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
        // - FlipUVs: 텍스처 좌표계 뒤집기 (DirectX용)
        // - GenSmoothNormals: 법선 벡터 생성
        // - PreTransformVertices: 복잡한 노드 구조를 무시하고 좌표를 다 합쳐버림 (맵 로딩에 유리)
        // - ConvertToLeftHanded: DirectX 왼손 좌표계로 변환
        const aiScene* scene = importer.ReadFile(filename,
            aiProcess_Triangulate |
            aiProcess_FlipUVs |
            aiProcess_GenSmoothNormals |
            aiProcess_PreTransformVertices |
            aiProcess_ConvertToLeftHanded);

        if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
        {
            OutputDebugStringA("ERROR::ASSIMP::LOAD_FAILED\n");
            return false;
        }

        ProcessNode(scene->mRootNode, scene, outData);
        return true;
    }

    // 2. FBX 안에 있는 텍스처 파일 이름들만 쏙쏙 뽑아오기
    static std::vector<std::string> LoadTextureNames(const std::string& filename)
    {
        Assimp::Importer importer;
        const aiScene* scene = importer.ReadFile(filename, 0); // 옵션 없이 정보만 읽음

        std::vector<std::string> textureNames;
        if (!scene) return textureNames;

        // 재질 개수만큼 리사이즈
        textureNames.resize(scene->mNumMaterials);

        for (UINT i = 0; i < scene->mNumMaterials; ++i)
        {
            aiMaterial* mat = scene->mMaterials[i];
            aiString path;

            // Diffuse(색상) 텍스처가 있는지 확인
            if (mat->GetTexture(aiTextureType_DIFFUSE, 0, &path) == AI_SUCCESS)
            {
                // ★ 경로 세탁 (전체 경로에서 파일명만 추출)
                textureNames[i] = GetFileNameFromPath(path.C_Str());
            }
            else
            {
                textureNames[i] = ""; // 텍스처 없는 재질
            }
        }
        return textureNames;
    }

private:
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
            ProcessMesh(mesh, scene, outData);
        }

        // 자식 노드 재귀 호출
        for (unsigned int i = 0; i < node->mNumChildren; i++)
        {
            ProcessNode(node->mChildren[i], scene, outData);
        }
    }

    static void ProcessMesh(aiMesh* mesh, const aiScene* scene, MapMeshData& outData)
    {
        Subset subset;
        subset.Id = (UINT)outData.Subsets.size();
        subset.VertexStart = (UINT)outData.Vertices.size(); // 현재 쌓인 정점 개수가 시작점
        subset.IndexStart = (UINT)outData.Indices.size();   // 현재 쌓인 인덱스 개수가 시작점
        subset.IndexCount = mesh->mNumFaces * 3;
        subset.MaterialIndex = mesh->mMaterialIndex;        // ★ Assimp가 알려준 재질 번호
        subset.Name = mesh->mName.C_Str();

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