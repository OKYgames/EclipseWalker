#pragma once
#include "d3dUtil.h"
#include "GameObject.h"
#include "FrameResource.h"


class Renderer
{
public:
    Renderer(ID3D12Device* device);
    ~Renderer();

    void Initialize();

    void DrawScene(
        ID3D12GraphicsCommandList* cmdList,
        const std::vector<std::unique_ptr<GameObject>>& gameObjects,
        ID3D12Resource* passCB,
        ID3D12DescriptorHeap* srvHeap,
        ID3D12Resource* objectCB  
    );

private:
    void BuildRootSignature();
    void BuildShadersAndInputLayout();
    void BuildPSO();

private:
    ID3D12Device* md3dDevice = nullptr;

    // DX12 ·»´õ¸µ ÇÙ½É 
    Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> mPSO;

    // ½¦ÀÌ´õ¿Í ÀÔ·Â ·¹ÀÌ¾Æ¿ô
    std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3DBlob>> mShaders;
    std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;
};
