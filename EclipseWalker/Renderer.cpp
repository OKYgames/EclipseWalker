#include "Renderer.h"

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
    ID3D12Resource* matCB,
    ID3D12PipelineState* pso,
    UINT passIndex)
{
    if (pso != nullptr) cmdList->SetPipelineState(pso);
    else cmdList->SetPipelineState(mPSO.Get());

    cmdList->SetGraphicsRootSignature(mRootSignature.Get());

    if (srvHeap)
    {
        ID3D12DescriptorHeap* heaps[] = { srvHeap };
        cmdList->SetDescriptorHeaps(1, heaps);

        CD3DX12_GPU_DESCRIPTOR_HANDLE shadowHandle(srvHeap->GetGPUDescriptorHandleForHeapStart());
        shadowHandle.Offset(200, md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
        cmdList->SetGraphicsRootDescriptorTable(3, shadowHandle);
    }

    if (passCB)
    {
        UINT passCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(PassConstants));
        D3D12_GPU_VIRTUAL_ADDRESS passCBAddress = passCB->GetGPUVirtualAddress() + (passIndex * passCBByteSize);
        cmdList->SetGraphicsRootConstantBufferView(1, passCBAddress);
    }

    UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
    UINT matCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(MaterialConstants));

    for (const auto& obj : gameObjects)
    {
        if (obj->Ritem == nullptr) continue;
        auto ri = obj->Ritem;
        
        if (pso == mTransparentPSO.Get())
        {
            // 재질이 없거나, 투명 체크가 안 되어 있으면 -> 건너뜀 
            if (ri->Mat == nullptr || ri->Mat->IsTransparent == 0) continue;
        }
        // 외곽선(Outline) 패스인 경우
        else if (pso == mOutlinePSO.Get())
        {
            // 툰이 아니거나(0) 혹은 투명한 물체(1)라면 -> 외곽선 그리지 않음
            if (ri->Mat == nullptr || ri->Mat->IsToon == 0 || ri->Mat->IsTransparent == 1) continue;
        }
        // 기본 불투명(Opaque) 패스인 경우 
        else
        {
            if (pso == mShadowPSO.Get())
            {
                if (ri->Mat != nullptr && ri->Mat->IsTransparent == 1) continue;
            }
            // 일반 불투명 패스라면?
            else
            {
                // 투명한 건 여기서 그리면 안 됨 (나중에 투명 패스에서 그릴 거니까)
                if (ri->Mat != nullptr && ri->Mat->IsTransparent == 1) continue;
            }
        }

        D3D12_VERTEX_BUFFER_VIEW vbv = ri->Geo->VertexBufferView();
        D3D12_INDEX_BUFFER_VIEW ibv = ri->Geo->IndexBufferView();

        cmdList->IASetVertexBuffers(0, 1, &vbv);
        cmdList->IASetIndexBuffer(&ibv);
        cmdList->IASetPrimitiveTopology(ri->PrimitiveType);

        if (srvHeap)
        {
            CD3DX12_GPU_DESCRIPTOR_HANDLE tex(srvHeap->GetGPUDescriptorHandleForHeapStart());
            int offset = ri->Mat->DiffuseSrvHeapIndex * 4;
            tex.Offset(offset, md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
            cmdList->SetGraphicsRootDescriptorTable(2, tex);
        }

        D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress() + ri->ObjCBIndex * objCBByteSize;
        cmdList->SetGraphicsRootConstantBufferView(0, objCBAddress);

        if (matCB != nullptr && ri->Mat != nullptr)
        {
            D3D12_GPU_VIRTUAL_ADDRESS matCBAddress = matCB->GetGPUVirtualAddress() + ri->Mat->MatCBIndex * matCBByteSize;
            cmdList->SetGraphicsRootConstantBufferView(4, matCBAddress);
        }

        cmdList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
    }
}

void Renderer::BuildRootSignature()
{
    // 테이블 1: 재질용 텍스처 (Diffuse, Normal, Emiss, Metal) -> t0 ~ t3
    CD3DX12_DESCRIPTOR_RANGE texTable0;
    texTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 4, 0); 

    // 테이블 2: 그림자 맵 (Shadow) -> t4
    CD3DX12_DESCRIPTOR_RANGE texTable1;
    texTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 4); 

    // 파라미터를 4개
    CD3DX12_ROOT_PARAMETER slotRootParameter[5];

    // 0: ObjectCB (b0)
    slotRootParameter[0].InitAsConstantBufferView(0);

    // 1: PassCB (b1)
    slotRootParameter[1].InitAsConstantBufferView(1);

    // 2: 재질 텍스처 테이블 (t0 ~ t39) - 움직이는 핸들
    slotRootParameter[2].InitAsDescriptorTable(1, &texTable0, D3D12_SHADER_VISIBILITY_PIXEL);

    // 3: 그림자 맵 테이블 (t40) - 고정된 핸들
    slotRootParameter[3].InitAsDescriptorTable(1, &texTable1, D3D12_SHADER_VISIBILITY_PIXEL);

    slotRootParameter[4].InitAsConstantBufferView(2);

    auto staticSamplers = GetStaticSamplers();

    CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(5, slotRootParameter,
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
    mShaders["standardVS"] = d3dUtil::CompileShader(L"color.hlsl", nullptr, "VS", "vs_5_0");
    mShaders["opaquePS"] = d3dUtil::CompileShader(L"color.hlsl", nullptr, "PS", "ps_5_0");
    mShaders["shadowVS"] = d3dUtil::CompileShader(L"Shadow.hlsl", nullptr, "VS", "vs_5_0");
    mShaders["shadowOpaquePS"] = d3dUtil::CompileShader(L"Shadow.hlsl", nullptr, "PS", "ps_5_0");
    mShaders["outlineVS"] = d3dUtil::CompileShader(L"color.hlsl", nullptr, "VS_Outline", "vs_5_1");
    mShaders["outlinePS"] = d3dUtil::CompileShader(L"color.hlsl", nullptr, "PS_Outline", "ps_5_1");

    // 2. 입력 레이아웃 설정
    mInputLayout =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TANGENT",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
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
    smapPsoDesc.RasterizerState.DepthBias = 100000;
    smapPsoDesc.RasterizerState.DepthBiasClamp = 0.0f;
    smapPsoDesc.RasterizerState.SlopeScaledDepthBias = 1.0f;

    // PSO 생성
    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&smapPsoDesc, IID_PPV_ARGS(&mShadowPSO)));

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

    // 3. 컬링 모드 반전! 
    outlinePsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_FRONT;

    // 4. PSO 생성
    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&outlinePsoDesc, IID_PPV_ARGS(&mOutlinePSO)));

    // =======================================================
    // 투명(Transparent)용 PSO 생성 (Fire, Decals 등)
    // =======================================================
    // 1. 기본 설정 복사 
    D3D12_GRAPHICS_PIPELINE_STATE_DESC transPsoDesc = psoDesc;

    // 2. 블렌드 상태 설정 
    // --> 색상을 덮어쓰지 않고, 알파값에 따라 섞어주는 설정
    D3D12_RENDER_TARGET_BLEND_DESC transparencyBlendDesc;
    transparencyBlendDesc.BlendEnable = true; 
    transparencyBlendDesc.LogicOpEnable = false;
    transparencyBlendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;       // 소스(텍스처)의 알파값 사용
    transparencyBlendDesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;  
    transparencyBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;           // 두 색을 더함
    transparencyBlendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
    transparencyBlendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;
    transparencyBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    transparencyBlendDesc.LogicOp = D3D12_LOGIC_OP_NOOP;
    transparencyBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    transPsoDesc.BlendState.RenderTarget[0] = transparencyBlendDesc;

    // 3. 깊이 쓰기 마스크 (선택사항)
    // 불꽃(Fire) 같은 이펙트는 보통 깊이 버퍼에 기록하지 않아야 
    // 뒤에 있는 물체가 가려지는 현상을 막을 수 있습니다.
    // (지금은 일반적인 반투명 물체도 고려해서 DepthWrite는 켜두셔도 됩니다. 
    //  만약 불꽃 주변 사각형 테두리가 보인다면 아래 줄 주석을 해제하세요.)
    //transPsoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;

    // 4. PSO 생성 
    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&transPsoDesc, IID_PPV_ARGS(&mTransparentPSO)));
}
