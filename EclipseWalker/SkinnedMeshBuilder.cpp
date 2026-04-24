#include "SkinnedMeshBuilder.h"
#include <algorithm>
#include <cfloat>

std::unique_ptr<MeshGeometry> SkinnedMeshBuilder::BuildMeshGeometry(
    ID3D12Device* device,
    ID3D12GraphicsCommandList* cmdList,
    const std::string& geoName,
    const AnimationLoader& loader,
    const std::string& submeshName)
{
    return BuildMeshGeometry(device, cmdList, geoName, loader.GetVertices(), loader.GetIndices(), submeshName);
}

std::unique_ptr<MeshGeometry> SkinnedMeshBuilder::BuildMeshGeometry(
    ID3D12Device* device,
    ID3D12GraphicsCommandList* cmdList,
    const std::string& geoName,
    const std::vector<SkinnedVertex>& vertices,
    const std::vector<unsigned int>& indices,
    const std::string& submeshName)
{
    if (device == nullptr || cmdList == nullptr || vertices.empty() || indices.empty())
    {
        return nullptr;
    }

    auto geometry = std::make_unique<MeshGeometry>();
    geometry->Name = geoName;

    const UINT vbByteSize = static_cast<UINT>(vertices.size() * sizeof(SkinnedVertex));
    const UINT ibByteSize = static_cast<UINT>(indices.size() * sizeof(std::uint32_t));

    ThrowIfFailed(D3DCreateBlob(vbByteSize, &geometry->VertexBufferCPU));
    CopyMemory(geometry->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

    ThrowIfFailed(D3DCreateBlob(ibByteSize, &geometry->IndexBufferCPU));
    CopyMemory(geometry->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

    geometry->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(device, cmdList, vertices.data(), vbByteSize, geometry->VertexBufferUploader);
    geometry->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(device, cmdList, indices.data(), ibByteSize, geometry->IndexBufferUploader);
    geometry->VertexByteStride = sizeof(SkinnedVertex);
    geometry->VertexBufferByteSize = vbByteSize;
    geometry->IndexFormat = DXGI_FORMAT_R32_UINT;
    geometry->IndexBufferByteSize = ibByteSize;

    DirectX::XMFLOAT3 minPoint(FLT_MAX, FLT_MAX, FLT_MAX);
    DirectX::XMFLOAT3 maxPoint(-FLT_MAX, -FLT_MAX, -FLT_MAX);
    for (const auto& vertex : vertices)
    {
        minPoint.x = (std::min)(minPoint.x, vertex.Position[0]);
        minPoint.y = (std::min)(minPoint.y, vertex.Position[1]);
        minPoint.z = (std::min)(minPoint.z, vertex.Position[2]);
        maxPoint.x = (std::max)(maxPoint.x, vertex.Position[0]);
        maxPoint.y = (std::max)(maxPoint.y, vertex.Position[1]);
        maxPoint.z = (std::max)(maxPoint.z, vertex.Position[2]);
    }

    SubmeshGeometry submesh;
    submesh.IndexCount = static_cast<UINT>(indices.size());
    submesh.StartIndexLocation = 0;
    submesh.BaseVertexLocation = 0;
    submesh.Bounds.Center = DirectX::XMFLOAT3(
        0.5f * (minPoint.x + maxPoint.x),
        0.5f * (minPoint.y + maxPoint.y),
        0.5f * (minPoint.z + maxPoint.z));
    submesh.Bounds.Extents = DirectX::XMFLOAT3(
        0.5f * (maxPoint.x - minPoint.x),
        0.5f * (maxPoint.y - minPoint.y),
        0.5f * (maxPoint.z - minPoint.z));

    geometry->DrawArgs[submeshName] = submesh;

    return geometry;
}
