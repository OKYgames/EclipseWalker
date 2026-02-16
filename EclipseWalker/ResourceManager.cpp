#include "ResourceManager.h"

ResourceManager::ResourceManager(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList)
    : md3dDevice(device), mCommandList(cmdList)
{
}

ResourceManager::~ResourceManager()
{
}

void ResourceManager::LoadTexture(std::string name, std::wstring filename)
{
    if (mTextures.find(name) != mTextures.end()) return;

    auto tex = std::make_unique<Texture>();
    tex->Name = name;
    tex->Filename = filename;

    std::unique_ptr<uint8_t[]> ddsData;
    std::vector<D3D12_SUBRESOURCE_DATA> subresources;

    ThrowIfFailed(DirectX::LoadDDSTextureFromFile(md3dDevice, filename.c_str(),
        tex->Resource.GetAddressOf(), ddsData, subresources));

    const UINT64 uploadBufferSize = GetRequiredIntermediateSize(tex->Resource.Get(), 0, static_cast<UINT>(subresources.size()));

    // 업로드 힙 생성
    auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);

    ThrowIfFailed(md3dDevice->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(tex->UploadHeap.GetAddressOf())));

    UpdateSubresources(mCommandList, tex->Resource.Get(), tex->UploadHeap.Get(),
        0, 0, static_cast<UINT>(subresources.size()), subresources.data());

    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(tex->Resource.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    mCommandList->ResourceBarrier(1, &barrier);

    mTextures[name] = std::move(tex);
}

Texture* ResourceManager::GetTexture(std::string name)
{
    if (mTextures.find(name) != mTextures.end())
        return mTextures[name].get();
    return nullptr;
}

int ResourceManager::GetTextureIndex(std::string name)
{
    int index = 0;
    for (const auto& tex : mTextures)
    {
        if (tex.first == name)
        {
            return index; 
        }
        index++;
    }
    return -1; 
}

void ResourceManager::CreateMaterial(std::string name, int matCBIndex, XMFLOAT4 diffuseAlbedo, XMFLOAT3 fresnelR0, float roughness)
{
    auto mat = std::make_unique<Material>();
    mat->Name = name;
    mat->MatCBIndex = matCBIndex;
    mat->DiffuseAlbedo = diffuseAlbedo;
    mat->FresnelR0 = fresnelR0;
    mat->Roughness = roughness;

    mMaterials[name] = std::move(mat);
}

Material* ResourceManager::GetMaterial(std::string name)
{
    if (mMaterials.find(name) != mMaterials.end())
        return mMaterials[name].get();
    return nullptr;
}