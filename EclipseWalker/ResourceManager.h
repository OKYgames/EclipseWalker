#pragma once
#include "d3dUtil.h"
#include "Texture.h"
#include "Material.h"
#include "MeshGeometry.h"
#include <map>
#include <string>
#include <memory>

class ResourceManager
{
public:
    ResourceManager(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList);
    ~ResourceManager();

    // 1. 텍스처 로드 함수 
    void LoadTexture(std::string name, std::wstring filename);
    Texture* GetTexture(std::string name);
    void BuildDescriptorHeaps(ID3D12Device* device);
    int GetTextureIndex(std::string name);

    ID3D12DescriptorHeap* GetSrvHeap() { return mSrvDescriptorHeap.Get(); }

    // 2. 재질 생성 함수
    void CreateMaterial(
        std::string name,
        int matCBIndex,
        std::string diffuseTex, 
        std::string normalTex,
        std::string emissiveTex,
        std::string metallicTex,
        XMFLOAT4 diffuseAlbedo,
        XMFLOAT3 fresnelR0,
        float roughness);

    Material* GetMaterial(std::string name);

    // 3. 모델(Mesh) 저장소
    std::map<std::string, std::unique_ptr<MeshGeometry>> mGeometries;
    std::map<std::string, std::unique_ptr<Texture>> mTextures;
    std::map<std::string, std::unique_ptr<Material>> mMaterials;
private:
    ID3D12Device* md3dDevice = nullptr;
    ID3D12GraphicsCommandList* mCommandList = nullptr;

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mSrvDescriptorHeap = nullptr;
    std::unordered_map<std::string, int> mTextureHeapIndices;

    const std::map<std::string, std::unique_ptr<Material>>& GetMaterials() const
    {
        return mMaterials;
    }
  
};
