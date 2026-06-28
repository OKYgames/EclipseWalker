#pragma once

#include "AnimationLoader.h"
#include "MeshGeometry.h"
#include "d3dUtil.h"
#include <memory>
#include <string>

class SkinnedMeshBuilder
{
public:
    static std::unique_ptr<MeshGeometry> BuildMeshGeometry(
        ID3D12Device* device,
        ID3D12GraphicsCommandList* cmdList,
        const std::string& geoName,
        const AnimationLoader& loader,
        const std::string& submeshName = "skinnedMesh");

    static std::unique_ptr<MeshGeometry> BuildMeshGeometry(
        ID3D12Device* device,
        ID3D12GraphicsCommandList* cmdList,
        const std::string& geoName,
        const std::vector<SkinnedVertex>& vertices,
        const std::vector<unsigned int>& indices,
        const std::string& submeshName = "skinnedMesh");
};
