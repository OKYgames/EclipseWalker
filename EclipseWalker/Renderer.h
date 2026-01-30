#pragma once
#include "d3dUtil.h"
#include "ShadowMap.h"
#include "GameObject.h"
#include "FrameResource.h"


class Renderer
{
public:
    Renderer(ID3D12Device* device);
    ~Renderer();

    void Initialize(CD3DX12_CPU_DESCRIPTOR_HANDLE shadowDsvHandle);

    void DrawScene(
        ID3D12GraphicsCommandList* cmdList,
        const std::vector<std::unique_ptr<GameObject>>& gameObjects,
        ID3D12Resource* passCB,
        ID3D12DescriptorHeap* srvHeap,
        ID3D12Resource* objectCB,
        ID3D12PipelineState* pso, 
        UINT passIndex           
    );

    ShadowMap* GetShadowMap() { return mShadowMap.get(); }
    ID3D12PipelineState* GetPSO() { return mPSO.Get(); }
    ID3D12PipelineState* GetShadowPSO() { return mShadowPSO.Get(); }
    ID3D12PipelineState* GetOutlinePSO() const { return mOutlinePSO.Get(); }

private:
    void BuildRootSignature();
    void BuildShadersAndInputLayout();
    void BuildPSO();

private:
    ID3D12Device* md3dDevice = nullptr;

    // DX12 렌더링 핵심 
    Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature;

    // 파이프라인 상태 객체
    Microsoft::WRL::ComPtr<ID3D12PipelineState> mPSO;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> mShadowPSO;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> mOutlinePSO;

    // 쉐이더와 입력 레이아웃
    std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3DBlob>> mShaders;
    std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;

    // 그림자 맵 관리자
    std::unique_ptr<ShadowMap> mShadowMap;

    // 그림자 맵이 사용할 힙의 주소(핸들) 보관용
    CD3DX12_CPU_DESCRIPTOR_HANDLE mShadowDsvHandle; // 쓰기용 (DSV)
    CD3DX12_GPU_DESCRIPTOR_HANDLE mShadowSrvHandle; // 읽기용 (SRV)
};
