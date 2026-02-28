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
    if (name.empty() || name == "None")
        return -1;
    if (mTextureHeapIndices.count(name))
        return mTextureHeapIndices[name];
    return -1;
}

void ResourceManager::BuildDescriptorHeaps(ID3D12Device* device)
{
    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
    srvHeapDesc.NumDescriptors = 1024; // 씬마다 달라질 수 있으므로 넉넉히
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    ThrowIfFailed(device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mSrvDescriptorHeap)));

    CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
    UINT descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    mTextureHeapIndices.clear();
    int currentIdx = 0;

    // 로드된 순서대로 힙에 등록하고 위치 저장
    for (auto& pair : mTextures)
    {
        Texture* tex = pair.second.get();

        // 텍스처의 원본 리소스 정보 가져오기
        D3D12_RESOURCE_DESC desc = tex->Resource->GetDesc();

        // SRV 기본 설정
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = desc.Format;

        if (tex->Name == "sky" || desc.DepthOrArraySize == 6)
        {
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE; // 큐브 맵으로 설정
            srvDesc.TextureCube.MostDetailedMip = 0;
            srvDesc.TextureCube.MipLevels = desc.MipLevels;
            srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
        }
        else
        {
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D; // 일반 2D 텍스처로 설정
            srvDesc.Texture2D.MostDetailedMip = 0;
            srvDesc.Texture2D.MipLevels = desc.MipLevels;
            srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
        }

        // 뷰(SRV) 생성
        device->CreateShaderResourceView(tex->Resource.Get(), &srvDesc, hDescriptor);

        mTextureHeapIndices[tex->Name] = currentIdx; // 이름-인덱스 매핑

        hDescriptor.Offset(1, descriptorSize);
        currentIdx++;
    }
}

void ResourceManager::CreateMaterial(
    std::string name,
    int matCBIndex,
    std::string diffuseTex,  
    std::string normalTex,
    std::string emissiveTex,
    std::string metallicTex,
    XMFLOAT4 diffuseAlbedo,
    XMFLOAT3 fresnelR0,
    float roughness)
{
    // 이미 존재하는 재질이면 리턴
    if (mMaterials.find(name) != mMaterials.end()) return;

    auto mat = std::make_unique<Material>();
    mat->Name = name;
    mat->MatCBIndex = matCBIndex;

    // 인덱스 숫자가 아닌 텍스처 파일 이름을 저장함
    mat->DiffuseMapName = diffuseTex;
    mat->NormalMapName = normalTex;
    mat->EmissiveMapName = emissiveTex;
    mat->MetallicMapName = metallicTex;

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