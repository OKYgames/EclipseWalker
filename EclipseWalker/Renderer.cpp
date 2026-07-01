#include "Renderer.h"

namespace
{
    RenderItem* FindSkyboxItem(const std::vector<std::unique_ptr<RenderItem>>& allRitems)
    {
        for (const auto& item : allRitems)
        {
            if (item && item->IsSkybox)
            {
                return item.get();
            }
        }

        return nullptr;
    }
}

std::array<const CD3DX12_STATIC_SAMPLER_DESC, 7> GetStaticSamplers()
{
    // 일반적인 샘플러 필터들 정의 (Point, Linear, Anisotropic 등)
    const CD3DX12_STATIC_SAMPLER_DESC pointWrap(
        0, // shaderRegister
        D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

    const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
        1, // shaderRegister
        D3D12_FILTER_MIN_MAG_MIP_POINT,
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

    const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
        2, // shaderRegister
        D3D12_FILTER_MIN_MAG_MIP_LINEAR,
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        D3D12_TEXTURE_ADDRESS_MODE_WRAP);

    const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
        3, // shaderRegister
        D3D12_FILTER_MIN_MAG_MIP_LINEAR,
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

    const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
        4, // shaderRegister
        D3D12_FILTER_ANISOTROPIC,
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        0.0f,                            
        8);                              

    const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(
        5, // shaderRegister
        D3D12_FILTER_ANISOTROPIC,
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        0.0f,
        8);

    const CD3DX12_STATIC_SAMPLER_DESC shadowSampler(
        6, 
        D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT, 
        D3D12_TEXTURE_ADDRESS_MODE_BORDER,
        D3D12_TEXTURE_ADDRESS_MODE_BORDER,
        D3D12_TEXTURE_ADDRESS_MODE_BORDER,
        0.0f,
        16,
        D3D12_COMPARISON_FUNC_LESS_EQUAL,
        D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE);

    return {
        pointWrap, pointClamp,
        linearWrap, linearClamp,
        anisotropicWrap, anisotropicClamp,
        shadowSampler};
}

Renderer::Renderer(ID3D12Device* device)
    : md3dDevice(device)
{
}

Renderer::~Renderer()
{
}

void Renderer::Initialize(CD3DX12_CPU_DESCRIPTOR_HANDLE shadowDsvHandle)
{
    mShadowDsvHandle = shadowDsvHandle;

    // 2. 그림자 맵 객체 생성
    mShadowMap = std::make_unique<ShadowMap>(md3dDevice, 4096, 4096);

    // 3. 쉐이더랑 파이프라인(PSO) 만들기
    BuildRootSignature();
    BuildShadersAndInputLayout();
    BuildPSO();
}

void Renderer::DrawScene(ID3D12GraphicsCommandList* cmdList,
    const std::vector<std::unique_ptr<GameObject>>& gameObjects,
    ID3D12Resource* passCB,
    ID3D12DescriptorHeap* srvHeap,
    ID3D12Resource* objectCB,
    ID3D12Resource* skinnedCB,
    ID3D12Resource* matCB,
    ID3D12PipelineState* pso,
    UINT passIndex)
{
    cmdList->SetGraphicsRootSignature(mRootSignature.Get());

    // =========================================================
    //  텍스처 힙 설정 
    // =========================================================
    if (srvHeap)
    {
        ID3D12DescriptorHeap* heaps[] = { srvHeap };
        cmdList->SetDescriptorHeaps(1, heaps);
        CD3DX12_GPU_DESCRIPTOR_HANDLE shadowHandle(srvHeap->GetGPUDescriptorHandleForHeapStart());
        shadowHandle.Offset(1000, md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
        cmdList->SetGraphicsRootDescriptorTable(3, shadowHandle);

        CD3DX12_GPU_DESCRIPTOR_HANDLE sceneDepthHandle(srvHeap->GetGPUDescriptorHandleForHeapStart());
        sceneDepthHandle.Offset(1001, md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
        cmdList->SetGraphicsRootDescriptorTable(6, sceneDepthHandle);

        CD3DX12_GPU_DESCRIPTOR_HANDLE texBaseHandle(srvHeap->GetGPUDescriptorHandleForHeapStart());
        cmdList->SetGraphicsRootDescriptorTable(2, texBaseHandle);
    }

    if (passCB)
    {
        UINT passCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(PassConstants));
        D3D12_GPU_VIRTUAL_ADDRESS passCBAddress = passCB->GetGPUVirtualAddress() + (passIndex * passCBByteSize);
        cmdList->SetGraphicsRootConstantBufferView(1, passCBAddress);
    }

    UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
    UINT skinnedCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(SkinnedConstants));
    UINT matCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(MaterialConstants));
    ID3D12PipelineState* currentPSO = nullptr;

    // =========================================================
    // 물체 그리기 루프
    // =========================================================
    for (const auto& obj : gameObjects)
    {
        if (obj->Ritem == nullptr) continue;
        auto ri = obj->Ritem;

        if (ri->Visible == false) continue;
        if (pso == mShadowPSO.Get() && ri->CastShadow == false) continue;
        if (pso == mUIPSO.Get() || pso == mMirrorBreakPSO.Get())
        {
            if (ri->Mat == nullptr) continue;
        }
        else if (pso == mTransparentPSO.Get() || pso == mDistortionPSO.Get() || pso == mFogVolumePSO.Get())
        {
            if (ri->Mat == nullptr) continue;
            if (pso == mTransparentPSO.Get() && ri->Mat->IsTransparent != 1) continue;
            if (pso == mFogVolumePSO.Get() && ri->Mat->IsTransparent != 2) continue;
            if (pso == mDistortionPSO.Get() && ri->Mat->IsTransparent == 2) continue;
        }
        else if (pso == mOutlinePSO.Get())
        {
            if (ri->Mat == nullptr || ri->Mat->OutlineThickness <= 0.0001f || ri->Mat->IsTransparent != 0) continue;
        }
        else
        {
            if (ri->Mat != nullptr && ri->Mat->IsTransparent != 0 && ri->Mat->IsTransparent != 3) continue;
        }

        ID3D12PipelineState* resolvedPSO = ResolvePipelineState(pso, ri);
        if (resolvedPSO != currentPSO)
        {
            cmdList->SetPipelineState(resolvedPSO);
            currentPSO = resolvedPSO;
        }

        D3D12_VERTEX_BUFFER_VIEW vbv = ri->Geo->VertexBufferView();
        D3D12_INDEX_BUFFER_VIEW ibv = ri->Geo->IndexBufferView();

        cmdList->IASetVertexBuffers(0, 1, &vbv);
        cmdList->IASetIndexBuffer(&ibv);
        cmdList->IASetPrimitiveTopology(ri->PrimitiveType);

        // 오브젝트 상수 버퍼
        D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress() + ri->ObjCBIndex * objCBByteSize;
        cmdList->SetGraphicsRootConstantBufferView(0, objCBAddress);

        if (ri->IsSkinned && skinnedCB != nullptr)
        {
            D3D12_GPU_VIRTUAL_ADDRESS skinnedCBAddress = skinnedCB->GetGPUVirtualAddress() + ri->SkinnedCBIndex * skinnedCBByteSize;
            cmdList->SetGraphicsRootConstantBufferView(5, skinnedCBAddress);
        }

        // 재질 상수 버퍼
        if (matCB != nullptr)
        {
            if (ri->Mat == nullptr || ri->Mat->MatCBIndex < 0) continue;
            D3D12_GPU_VIRTUAL_ADDRESS matCBAddress = matCB->GetGPUVirtualAddress() +
                (UINT64)ri->Mat->MatCBIndex * matCBByteSize;
            cmdList->SetGraphicsRootConstantBufferView(4, matCBAddress);
        }

        cmdList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
    }
}

void Renderer::DrawScene(ID3D12GraphicsCommandList* cmdList,
    const std::vector<GameObject*>& gameObjects,
    ID3D12Resource* passCB,
    ID3D12DescriptorHeap* srvHeap,
    ID3D12Resource* objectCB,
    ID3D12Resource* skinnedCB,
    ID3D12Resource* matCB,
    ID3D12PipelineState* pso,
    UINT passIndex)
{
    cmdList->SetGraphicsRootSignature(mRootSignature.Get());

    if (srvHeap)
    {
        ID3D12DescriptorHeap* heaps[] = { srvHeap };
        cmdList->SetDescriptorHeaps(1, heaps);
        CD3DX12_GPU_DESCRIPTOR_HANDLE shadowHandle(srvHeap->GetGPUDescriptorHandleForHeapStart());
        shadowHandle.Offset(1000, md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
        cmdList->SetGraphicsRootDescriptorTable(3, shadowHandle);

        CD3DX12_GPU_DESCRIPTOR_HANDLE sceneDepthHandle(srvHeap->GetGPUDescriptorHandleForHeapStart());
        sceneDepthHandle.Offset(1001, md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
        cmdList->SetGraphicsRootDescriptorTable(6, sceneDepthHandle);

        CD3DX12_GPU_DESCRIPTOR_HANDLE texBaseHandle(srvHeap->GetGPUDescriptorHandleForHeapStart());
        cmdList->SetGraphicsRootDescriptorTable(2, texBaseHandle);
    }

    if (passCB)
    {
        UINT passCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(PassConstants));
        D3D12_GPU_VIRTUAL_ADDRESS passCBAddress = passCB->GetGPUVirtualAddress() + (passIndex * passCBByteSize);
        cmdList->SetGraphicsRootConstantBufferView(1, passCBAddress);
    }

    UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
    UINT skinnedCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(SkinnedConstants));
    UINT matCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(MaterialConstants));
    ID3D12PipelineState* currentPSO = nullptr;

    for (const auto& obj : gameObjects)
    {
        if (obj->Ritem == nullptr) continue;
        auto ri = obj->Ritem;

        if (ri->Visible == false) continue;
        if (pso == mShadowPSO.Get() && ri->CastShadow == false) continue;

        if (pso == mUIPSO.Get() || pso == mMirrorBreakPSO.Get())
        {
            if (ri->Mat == nullptr) continue;
        }
        else if (pso == mTransparentPSO.Get() || pso == mDistortionPSO.Get() || pso == mFogVolumePSO.Get()) {
            if (ri->Mat == nullptr) continue;
            if (pso == mTransparentPSO.Get() && ri->Mat->IsTransparent != 1) continue;
            if (pso == mFogVolumePSO.Get() && ri->Mat->IsTransparent != 2) continue;
            if (pso == mDistortionPSO.Get() && ri->Mat->IsTransparent == 2) continue;
        }
        else if (pso == mOutlinePSO.Get()) {
            if (ri->Mat == nullptr || ri->Mat->OutlineThickness <= 0.0001f || ri->Mat->IsTransparent != 0) continue;
        }
        else {
            if (ri->Mat != nullptr && ri->Mat->IsTransparent != 0 && ri->Mat->IsTransparent != 3) continue;
        }

        ID3D12PipelineState* resolvedPSO = ResolvePipelineState(pso, ri);
        if (resolvedPSO != currentPSO)
        {
            cmdList->SetPipelineState(resolvedPSO);
            currentPSO = resolvedPSO;
        }

        D3D12_VERTEX_BUFFER_VIEW vbv = ri->Geo->VertexBufferView();
        D3D12_INDEX_BUFFER_VIEW ibv = ri->Geo->IndexBufferView();

        cmdList->IASetVertexBuffers(0, 1, &vbv);
        cmdList->IASetIndexBuffer(&ibv);
        cmdList->IASetPrimitiveTopology(ri->PrimitiveType);

        D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress() + ri->ObjCBIndex * objCBByteSize;
        cmdList->SetGraphicsRootConstantBufferView(0, objCBAddress);

        if (ri->IsSkinned && skinnedCB != nullptr)
        {
            D3D12_GPU_VIRTUAL_ADDRESS skinnedCBAddress = skinnedCB->GetGPUVirtualAddress() + ri->SkinnedCBIndex * skinnedCBByteSize;
            cmdList->SetGraphicsRootConstantBufferView(5, skinnedCBAddress);
        }

        if (matCB != nullptr)
        {
            if (ri->Mat == nullptr || ri->Mat->MatCBIndex < 0) continue;
            D3D12_GPU_VIRTUAL_ADDRESS matCBAddress = matCB->GetGPUVirtualAddress() +
                (UINT64)ri->Mat->MatCBIndex * matCBByteSize;
            cmdList->SetGraphicsRootConstantBufferView(4, matCBAddress);
        }

        cmdList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
    }
}

ID3D12PipelineState* Renderer::ResolvePipelineState(ID3D12PipelineState* requestedPSO, const RenderItem* renderItem) const
{
    ID3D12PipelineState* basePSO = (requestedPSO != nullptr) ? requestedPSO : mPSO.Get();
    if (renderItem == nullptr || !renderItem->IsSkinned)
    {
        return basePSO;
    }

    if (basePSO == mPSO.Get())
    {
        return mSkinnedPSO.Get();
    }

    if (basePSO == mShadowPSO.Get())
    {
        return mSkinnedShadowPSO.Get();
    }

    if (basePSO == mOutlinePSO.Get())
    {
        return mSkinnedOutlinePSO.Get();
    }

    return basePSO;
}

void Renderer::DrawSkybox(
    ID3D12GraphicsCommandList* cmdList,
    const std::vector<std::unique_ptr<RenderItem>>& allRitems,
    ID3D12DescriptorHeap* srvHeap,
    int skyTexHeapIndex,
    int skyEclipseTexHeapIndex,
    ID3D12Resource* objectCB,
    ID3D12Resource* passCB) 
{
    cmdList->SetGraphicsRootSignature(mRootSignature.Get());

    // 1. 스카이박스 PSO 적용
    if (mSkyPSO != nullptr)
    {
        cmdList->SetPipelineState(mSkyPSO.Get());
    }

    // 2. 스카이박스 아이템 가져오기
    RenderItem* skyRitem = FindSkyboxItem(allRitems);
    if (skyRitem == nullptr || skyRitem->Geo == nullptr)
    {
        return;
    }

    // 3. 텍스처 힙 설정
    if (srvHeap)
    {
        ID3D12DescriptorHeap* heaps[] = { srvHeap };
        cmdList->SetDescriptorHeaps(1, heaps);

        CD3DX12_GPU_DESCRIPTOR_HANDLE skyTexHandle(srvHeap->GetGPUDescriptorHandleForHeapStart());
        UINT descriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

        // 스카이박스 텍스처 위치로 이동 후 연결 (t0)
        skyTexHandle.Offset(skyTexHeapIndex, descriptorSize);
        cmdList->SetGraphicsRootDescriptorTable(2, skyTexHandle);

        if (skyEclipseTexHeapIndex >= 0)
        {
            CD3DX12_GPU_DESCRIPTOR_HANDLE skyEclipseTexHandle(srvHeap->GetGPUDescriptorHandleForHeapStart());
            skyEclipseTexHandle.Offset(skyEclipseTexHeapIndex, descriptorSize);
            cmdList->SetGraphicsRootDescriptorTable(7, skyEclipseTexHandle);
        }

        CD3DX12_GPU_DESCRIPTOR_HANDLE shadowHandle(srvHeap->GetGPUDescriptorHandleForHeapStart());
        shadowHandle.Offset(1000, descriptorSize);
        cmdList->SetGraphicsRootDescriptorTable(3, shadowHandle);

        CD3DX12_GPU_DESCRIPTOR_HANDLE sceneDepthHandle(srvHeap->GetGPUDescriptorHandleForHeapStart());
        sceneDepthHandle.Offset(1001, descriptorSize);
        cmdList->SetGraphicsRootDescriptorTable(6, sceneDepthHandle);
    }

    // 4. 상수 버퍼 연결
    UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
    D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress() + skyRitem->ObjCBIndex * objCBByteSize;
    cmdList->SetGraphicsRootConstantBufferView(0, objCBAddress);


    if (passCB)
    {
        cmdList->SetGraphicsRootConstantBufferView(1, passCB->GetGPUVirtualAddress());
    }

    // 5. 지오메트리 설정 및 그리기
    D3D12_VERTEX_BUFFER_VIEW vbv = skyRitem->Geo->VertexBufferView();
    D3D12_INDEX_BUFFER_VIEW ibv = skyRitem->Geo->IndexBufferView();

    cmdList->IASetVertexBuffers(0, 1, &vbv);
    cmdList->IASetIndexBuffer(&ibv);
    cmdList->IASetPrimitiveTopology(skyRitem->PrimitiveType);

    cmdList->DrawIndexedInstanced(skyRitem->IndexCount, 1, skyRitem->StartIndexLocation, skyRitem->BaseVertexLocation, 0);
}

void Renderer::BuildRootSignature()
{
    // 테이블 1: 재질용 텍스처 (Diffuse, Normal, Emiss, Metal) -> t0 ~ t3
    CD3DX12_DESCRIPTOR_RANGE texTable0;
    texTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1000, 0, 0);

    // 테이블 2: 그림자 맵 (Shadow) -> t4
    CD3DX12_DESCRIPTOR_RANGE texTable1;
    texTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1000);

    CD3DX12_DESCRIPTOR_RANGE texTable2;
    texTable2.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1001);

    CD3DX12_DESCRIPTOR_RANGE texTable3;
    texTable3.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1002);

    // 파라미터를 4개
    CD3DX12_ROOT_PARAMETER slotRootParameter[8];

    // 0: ObjectCB (b0)
    slotRootParameter[0].InitAsConstantBufferView(0);

    // 1: PassCB (b1)
    slotRootParameter[1].InitAsConstantBufferView(1);

    // 2: 재질 텍스처 테이블 (t0 ~ t39) - 움직이는 핸들
    slotRootParameter[2].InitAsDescriptorTable(1, &texTable0, D3D12_SHADER_VISIBILITY_PIXEL);

    // 3: 그림자 맵 테이블 (t40) - 고정된 핸들
    slotRootParameter[3].InitAsDescriptorTable(1, &texTable1, D3D12_SHADER_VISIBILITY_PIXEL);

    slotRootParameter[4].InitAsConstantBufferView(2);
    slotRootParameter[5].InitAsConstantBufferView(3);
    slotRootParameter[6].InitAsDescriptorTable(1, &texTable2, D3D12_SHADER_VISIBILITY_PIXEL);
    slotRootParameter[7].InitAsDescriptorTable(1, &texTable3, D3D12_SHADER_VISIBILITY_PIXEL);

    auto staticSamplers = GetStaticSamplers();

    CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(8, slotRootParameter,
        (UINT)staticSamplers.size(), staticSamplers.data(),
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
    ComPtr<ID3DBlob> serializedRootSig = nullptr;
    ComPtr<ID3DBlob> errorBlob = nullptr;
    HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
        serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

    if (errorBlob != nullptr)
    {
        ::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
    }
    ThrowIfFailed(hr);

    ThrowIfFailed(md3dDevice->CreateRootSignature(
        0,
        serializedRootSig->GetBufferPointer(),
        serializedRootSig->GetBufferSize(),
        IID_PPV_ARGS(mRootSignature.GetAddressOf())));
}

void Renderer::BuildShadersAndInputLayout()
{
    // 1. 쉐이더 컴파일 및 저장
    mShaders["standardVS"] = d3dUtil::CompileShader(L"color.hlsl", nullptr, "VS", "vs_5_1");
    mShaders["opaquePS"] = d3dUtil::CompileShader(L"color.hlsl", nullptr, "PS", "ps_5_1");
    mShaders["shadowVS"] = d3dUtil::CompileShader(L"Shadow.hlsl", nullptr, "VS", "vs_5_1");
    mShaders["shadowOpaquePS"] = d3dUtil::CompileShader(L"Shadow.hlsl", nullptr, "PS", "ps_5_1");
    mShaders["outlineVS"] = d3dUtil::CompileShader(L"color.hlsl", nullptr, "VS_Outline", "vs_5_1");
    mShaders["outlinePS"] = d3dUtil::CompileShader(L"color.hlsl", nullptr, "PS_Outline", "ps_5_1");
    mShaders["skinnedVS"] = d3dUtil::CompileShader(L"Skinned.hlsl", nullptr, "VS_Opaque", "vs_5_1");
    mShaders["skinnedOutlineVS"] = d3dUtil::CompileShader(L"Skinned.hlsl", nullptr, "VS_Outline", "vs_5_1");
    mShaders["skyVS"] = d3dUtil::CompileShader(L"Sky.hlsl", nullptr, "VS", "vs_5_1");
    mShaders["skyPS"] = d3dUtil::CompileShader(L"Sky.hlsl", nullptr, "PS", "ps_5_1");
    mShaders["distortionVS"] = d3dUtil::CompileShader(L"Distortion.hlsl", nullptr, "VS", "vs_5_1");
    mShaders["distortionPS"] = d3dUtil::CompileShader(L"Distortion.hlsl", nullptr, "PS", "ps_5_1");
    mShaders["mirrorBreakVS"] = d3dUtil::CompileShader(L"MirrorBreak.hlsl", nullptr, "VS", "vs_5_1");
    mShaders["mirrorBreakPS"] = d3dUtil::CompileShader(L"MirrorBreak.hlsl", nullptr, "PS", "ps_5_1");
    mShaders["volumetricFogVS"] = d3dUtil::CompileShader(L"VolumetricFog.hlsl", nullptr, "VS", "vs_5_1");
    mShaders["volumetricFogPS"] = d3dUtil::CompileShader(L"VolumetricFog.hlsl", nullptr, "PS", "ps_5_1");
    mShaders["skinnedShadowVS"] = d3dUtil::CompileShader(L"Skinned.hlsl", nullptr, "VS_Shadow", "vs_5_1");
    // 2. 입력 레이아웃 설정
    mInputLayout =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TANGENT",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };

    mSkinnedInputLayout =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TANGENT",  0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "WEIGHTS",  0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 44, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "BONEINDICES", 0, DXGI_FORMAT_R32G32B32A32_UINT, 0, 60, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };
}

void Renderer::BuildPSO()
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;
    ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));

    psoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
    psoDesc.pRootSignature = mRootSignature.Get();
    psoDesc.VS =
    {
        reinterpret_cast<BYTE*>(mShaders["standardVS"]->GetBufferPointer()),
        mShaders["standardVS"]->GetBufferSize()
    };
    psoDesc.PS =
    {
        reinterpret_cast<BYTE*>(mShaders["opaquePS"]->GetBufferPointer()),
        mShaders["opaquePS"]->GetBufferSize()
    };

    D3D12_RASTERIZER_DESC rasterizerDesc = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
    psoDesc.RasterizerState = rasterizerDesc;

    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.BlendState.AlphaToCoverageEnable = TRUE;
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;

    psoDesc.SampleDesc.Count = 4;
    psoDesc.SampleDesc.Quality = 0;

    psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSO)));

    D3D12_GRAPHICS_PIPELINE_STATE_DESC skinnedPsoDesc = psoDesc;
    skinnedPsoDesc.InputLayout = { mSkinnedInputLayout.data(), (UINT)mSkinnedInputLayout.size() };
    skinnedPsoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    skinnedPsoDesc.VS =
    {
        reinterpret_cast<BYTE*>(mShaders["skinnedVS"]->GetBufferPointer()),
        mShaders["skinnedVS"]->GetBufferSize()
    };
    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&skinnedPsoDesc, IID_PPV_ARGS(&mSkinnedPSO)));

    // -----------------------------------------------------------------------
    // 그림자 맵용 PSO 생성 (Shadow Map Pass)
    // -----------------------------------------------------------------------
    D3D12_GRAPHICS_PIPELINE_STATE_DESC smapPsoDesc = psoDesc;

    // 1. 쉐이더 교체
    smapPsoDesc.VS =
    {
        reinterpret_cast<BYTE*>(mShaders["shadowVS"]->GetBufferPointer()),
        mShaders["shadowVS"]->GetBufferSize()
    };
    smapPsoDesc.PS =
    {
        reinterpret_cast<BYTE*>(mShaders["shadowOpaquePS"]->GetBufferPointer()),
        mShaders["shadowOpaquePS"]->GetBufferSize()
    };

    // 2.렌더 타겟(색상) 끄기
    smapPsoDesc.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;
    smapPsoDesc.NumRenderTargets = 0;

    // 3. 깊이 스텐실 설정 
    smapPsoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    smapPsoDesc.SampleDesc.Count = 1;
    smapPsoDesc.SampleDesc.Quality = 0;

    // 4. 라스터라이저 수정
    smapPsoDesc.RasterizerState.DepthBias = 5000;
    smapPsoDesc.RasterizerState.DepthBiasClamp = 0.0f;
    smapPsoDesc.RasterizerState.SlopeScaledDepthBias = 1.0f;

    // PSO 생성
    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&smapPsoDesc, IID_PPV_ARGS(&mShadowPSO)));

    D3D12_GRAPHICS_PIPELINE_STATE_DESC skinnedShadowPsoDesc = smapPsoDesc;
    skinnedShadowPsoDesc.InputLayout = { mSkinnedInputLayout.data(), (UINT)mSkinnedInputLayout.size() };
    skinnedShadowPsoDesc.VS =
    {
        reinterpret_cast<BYTE*>(mShaders["skinnedShadowVS"]->GetBufferPointer()),
        mShaders["skinnedShadowVS"]->GetBufferSize()
    };
    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&skinnedShadowPsoDesc, IID_PPV_ARGS(&mSkinnedShadowPSO)));


    // =======================================================
    // 외곽선(Outline)용 PSO 생성
    // =======================================================
    // 1. 기본 설정 복사
    D3D12_GRAPHICS_PIPELINE_STATE_DESC outlinePsoDesc = psoDesc;

    // 2. 쉐이더 교체
    outlinePsoDesc.VS =
    {
        reinterpret_cast<BYTE*>(mShaders["outlineVS"]->GetBufferPointer()),
        mShaders["outlineVS"]->GetBufferSize()
    };
    outlinePsoDesc.PS =
    {
        reinterpret_cast<BYTE*>(mShaders["outlinePS"]->GetBufferPointer()),
        mShaders["outlinePS"]->GetBufferSize()
    };

    // 3. 컬링 모드 반전
    outlinePsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_FRONT;

    // 4. PSO 생성
    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&outlinePsoDesc, IID_PPV_ARGS(&mOutlinePSO)));

    D3D12_GRAPHICS_PIPELINE_STATE_DESC skinnedOutlinePsoDesc = outlinePsoDesc;
    skinnedOutlinePsoDesc.InputLayout = { mSkinnedInputLayout.data(), (UINT)mSkinnedInputLayout.size() };
    skinnedOutlinePsoDesc.VS =
    {
        reinterpret_cast<BYTE*>(mShaders["skinnedOutlineVS"]->GetBufferPointer()),
        mShaders["skinnedOutlineVS"]->GetBufferSize()
    };
    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&skinnedOutlinePsoDesc, IID_PPV_ARGS(&mSkinnedOutlinePSO)));

    // =======================================================
    // 투명(Transparent)용 PSO 생성 (Fire, Decals 등)
    // =======================================================
    // 1. 기본 설정 복사 
    D3D12_GRAPHICS_PIPELINE_STATE_DESC transPsoDesc = psoDesc;

    // 2. 블렌드 상태 설정 
    D3D12_RENDER_TARGET_BLEND_DESC transparencyBlendDesc;
    transparencyBlendDesc.BlendEnable = true;
    transparencyBlendDesc.LogicOpEnable = false;
    transparencyBlendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;       
    transparencyBlendDesc.DestBlend = D3D12_BLEND_ONE;
    transparencyBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
    transparencyBlendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
    transparencyBlendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;
    transparencyBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    transparencyBlendDesc.LogicOp = D3D12_LOGIC_OP_NOOP;
    transparencyBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    transPsoDesc.BlendState.RenderTarget[0] = transparencyBlendDesc;

    // 4. PSO 생성 
    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&transPsoDesc, IID_PPV_ARGS(&mTransparentPSO)));
    transPsoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;

    // =======================================================
    // 안개 볼륨(Fog Volume)용 PSO 생성
    // =======================================================
    D3D12_GRAPHICS_PIPELINE_STATE_DESC fogPsoDesc = psoDesc;
    D3D12_RENDER_TARGET_BLEND_DESC fogBlendDesc;
    fogBlendDesc.BlendEnable = true;
    fogBlendDesc.LogicOpEnable = false;
    fogBlendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
    fogBlendDesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    fogBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
    fogBlendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
    fogBlendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;
    fogBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    fogBlendDesc.LogicOp = D3D12_LOGIC_OP_NOOP;
    fogBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    fogPsoDesc.BlendState.RenderTarget[0] = fogBlendDesc;
    fogPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    fogPsoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&fogPsoDesc, IID_PPV_ARGS(&mFogVolumePSO)));

    // =======================================================
    // UI 전용 PSO 생성
    // =======================================================
    D3D12_GRAPHICS_PIPELINE_STATE_DESC uiPsoDesc = psoDesc;
    D3D12_RENDER_TARGET_BLEND_DESC uiBlendDesc = {};
    uiBlendDesc.BlendEnable = true;
    uiBlendDesc.LogicOpEnable = false;
    uiBlendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
    uiBlendDesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    uiBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
    uiBlendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
    uiBlendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;
    uiBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    uiBlendDesc.LogicOp = D3D12_LOGIC_OP_NOOP;
    uiBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    uiPsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    uiPsoDesc.BlendState.AlphaToCoverageEnable = FALSE;
    uiPsoDesc.BlendState.RenderTarget[0] = uiBlendDesc;
    uiPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    uiPsoDesc.DepthStencilState.DepthEnable = FALSE;
    uiPsoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&uiPsoDesc, IID_PPV_ARGS(&mUIPSO)));

    D3D12_GRAPHICS_PIPELINE_STATE_DESC mirrorBreakPsoDesc = uiPsoDesc;
    mirrorBreakPsoDesc.VS =
    {
        reinterpret_cast<BYTE*>(mShaders["mirrorBreakVS"]->GetBufferPointer()),
        mShaders["mirrorBreakVS"]->GetBufferSize()
    };
    mirrorBreakPsoDesc.PS =
    {
        reinterpret_cast<BYTE*>(mShaders["mirrorBreakPS"]->GetBufferPointer()),
        mShaders["mirrorBreakPS"]->GetBufferSize()
    };
    mirrorBreakPsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    mirrorBreakPsoDesc.BlendState.AlphaToCoverageEnable = FALSE;
    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&mirrorBreakPsoDesc, IID_PPV_ARGS(&mMirrorBreakPSO)));

    D3D12_GRAPHICS_PIPELINE_STATE_DESC volumetricFogPsoDesc = mirrorBreakPsoDesc;
    volumetricFogPsoDesc.VS =
    {
        reinterpret_cast<BYTE*>(mShaders["volumetricFogVS"]->GetBufferPointer()),
        mShaders["volumetricFogVS"]->GetBufferSize()
    };
    volumetricFogPsoDesc.PS =
    {
        reinterpret_cast<BYTE*>(mShaders["volumetricFogPS"]->GetBufferPointer()),
        mShaders["volumetricFogPS"]->GetBufferSize()
    };
    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&volumetricFogPsoDesc, IID_PPV_ARGS(&mVolumetricFogPSO)));

    // =======================================================
    // 차원 전환(Distortion)용 PSO 생성
    // =======================================================
    D3D12_GRAPHICS_PIPELINE_STATE_DESC distPsoDesc = psoDesc;
    D3D12_RENDER_TARGET_BLEND_DESC distBlendDesc;
    distBlendDesc.BlendEnable = true;
    distBlendDesc.LogicOpEnable = false;
    distBlendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
    distBlendDesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA; 
    distBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
    distBlendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
    distBlendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;
    distBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    distBlendDesc.LogicOp = D3D12_LOGIC_OP_NOOP;
    distBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    distPsoDesc.BlendState.RenderTarget[0] = distBlendDesc;

    // 3. 컬링 끄기 (구체 안쪽에서도 보이도록)
    distPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

    // 4. 깊이 버퍼 쓰기 끄기 (다른 투명 물체와 겹칠 때 깨짐 방지)
    distPsoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    distPsoDesc.VS = { reinterpret_cast<BYTE*>(mShaders["distortionVS"]->GetBufferPointer()), mShaders["distortionVS"]->GetBufferSize() };
    distPsoDesc.PS = { reinterpret_cast<BYTE*>(mShaders["distortionPS"]->GetBufferPointer()), mShaders["distortionPS"]->GetBufferSize() };

    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&distPsoDesc, IID_PPV_ARGS(&mDistortionPSO)));
    distPsoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    // =======================================================
    // 스카이박스(Skybox)용 PSO 생성
    // =======================================================
    D3D12_GRAPHICS_PIPELINE_STATE_DESC skyPsoDesc = psoDesc;

    // 2. 쉐이더 교체 
    skyPsoDesc.VS =
    {
        reinterpret_cast<BYTE*>(mShaders["skyVS"]->GetBufferPointer()),
        mShaders["skyVS"]->GetBufferSize()
    };
    skyPsoDesc.PS =
    {
        reinterpret_cast<BYTE*>(mShaders["skyPS"]->GetBufferPointer()),
        mShaders["skyPS"]->GetBufferSize()
    };

    // 3. 컬링 끄기 
    skyPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    skyPsoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    // 4. 깊이 비교 함수 변경 
    skyPsoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

    // 5. PSO 생성
    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&skyPsoDesc, IID_PPV_ARGS(&mSkyPSO)));

    D3D12_GRAPHICS_PIPELINE_STATE_DESC wirePsoDesc = psoDesc;

    // 2. 레스터라이저 상태 변경: SOLID(채우기) -> WIREFRAME(선 그리기)
    wirePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
    // 3. 컬링 끄기 
    wirePsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    wirePsoDesc.DepthStencilState.DepthEnable = FALSE;

    // 4. PSO 생성
    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&wirePsoDesc, IID_PPV_ARGS(&mWireframePSO)));

}
