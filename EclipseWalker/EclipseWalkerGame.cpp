#include "EclipseWalkerGame.h"
#include "DDSTextureLoader.h"
#include <windowsx.h>

EclipseWalkerGame::EclipseWalkerGame(HINSTANCE hInstance)
    : GameFramework(hInstance)
{
}

EclipseWalkerGame::~EclipseWalkerGame()
{
}

bool EclipseWalkerGame::Initialize()
{
    srand((unsigned int)time(NULL));
    m4xMsaaState = true;

    // 1. 부모 클래스 초기화 
    if (!GameFramework::Initialize())
        return false;

    // 명령 할당자 리셋
    ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

    // DSV 힙 설정 (화면용 + 그림자용)
    {
        D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
        dsvHeapDesc.NumDescriptors = 2; // [0: 화면용], [1: 그림자용]
        dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        dsvHeapDesc.NodeMask = 0;
        ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&mDsvHeap)));

        CD3DX12_CPU_DESCRIPTOR_HANDLE mainDsvHandle(mDsvHeap->GetCPUDescriptorHandleForHeapStart());
        md3dDevice->CreateDepthStencilView(mDepthStencilBuffer.Get(), nullptr, mainDsvHandle);
    }

    // 2. 시스템 초기화 (매니저 & 렌더러 생성)
    mResources = std::make_unique<ResourceManager>(md3dDevice.Get(), mCommandList.Get());
    mRenderer = std::make_unique<Renderer>(md3dDevice.Get());

   
    CD3DX12_CPU_DESCRIPTOR_HANDLE shadowHandle(mDsvHeap->GetCPUDescriptorHandleForHeapStart());
    UINT dsvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    shadowHandle.Offset(1, dsvDescriptorSize); 
    mRenderer->Initialize(shadowHandle); 

    // 3. 리소스 로드 및 설정
    InitLights();
    LoadTextures();
    BuildDescriptorHeaps();
    BuildShapeGeometry();
    

    mMapSystem = std::make_unique<MapSystem>();

    float mapOffsetX = 0.0f;
    float mapOffsetY = 0.0f;
    float mapOffsetZ = 0.0f;

    if (mResources->mGeometries.find("mapGeo") != mResources->mGeometries.end())
    {
        mMapSystem->Build(
            mResources->mGeometries["mapGeo"].get(),

            0.01f,              // Scale
            0.0f, 0.0f, 0.0f,   // Rotation
            0.0f, 0.0f, 0.0f    // Position 
        );
    }
    
    BuildMaterials();
    BuildRenderItems();

    mPlayer = std::make_unique<Player>();
    mPlayer->Initialize(mPlayerObject, &mCamera);

    BuildFrameResources();

    // 4. 초기화 명령 실행
    ThrowIfFailed(mCommandList->Close());
    ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    // GPU 동기화
    mCurrentFence++;
    mCommandQueue->Signal(mFence.Get(), mCurrentFence);
    if (mFence->GetCompletedValue() < mCurrentFence)
    {
        HANDLE eventHandle = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        mFence->SetEventOnCompletion(mCurrentFence, eventHandle);
        WaitForSingleObject(eventHandle, INFINITE);
        CloseHandle(eventHandle);
    }

    // 5. 카메라 초기 위치 설정
    mCamera.SetPosition(0.0f, 2.0f, -5.0f);
    mCamera.SetLens(0.25f * 3.14f, AspectRatio(), 1.0f, 1000.0f);
    mCamera.Pitch(XMConvertToRadians(15.0f));

    return true;
}

void EclipseWalkerGame::OnResize()
{
    GameFramework::OnResize();
    mCamera.SetLens(0.25f * 3.14f, AspectRatio(), 1.0f, 1000.0f);
}

void EclipseWalkerGame::Update(const GameTimer& gt)
{
    // 1. 프레임 리소스 순환
    mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % 3;
    mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();

    if (mCurrFrameResource->Fence != 0 && mFence->GetCompletedValue() < mCurrFrameResource->Fence)
    {
        HANDLE eventHandle = CreateEventEx(nullptr, nullptr, false, EVENT_ALL_ACCESS);
        ThrowIfFailed(mFence->SetEventOnCompletion(mCurrFrameResource->Fence, eventHandle));
        WaitForSingleObject(eventHandle, INFINITE);
        CloseHandle(eventHandle);
    }

    // 2. 입력 및 로직 처리
    OnKeyboardInput(gt);

    if (mPlayer)
    {
        mPlayer->Update(gt, mMapSystem.get());
    }
   
    XMFLOAT3 camPos = mCamera.GetPosition3f();
    mCamera.UpdateViewMatrix();
    XMFLOAT3 pos = mPlayerObject->GetPosition();
    char buf[256];
    //sprintf_s(buf, "Player Pos: (%.2f, %.2f, %.2f)\n", pos.x, pos.y, pos.z);
    //OutputDebugStringA(buf);

    // 모든 게임 오브젝트 업데이트
    for (auto& obj : mGameObjects)
    {
        if (obj->mIsBillboard)
        {
            // 불 위치
            XMFLOAT3 firePos = obj->GetPosition();

            // 카메라와의 거리 벡터
            float dx = camPos.x - firePos.x;
            float dz = camPos.z - firePos.z;
            float angleY = atan2(dx, dz);

            // 회전 적용 
            obj->SetRotation(0.0f, angleY, 0.0f);
        }

        obj->Update();
        obj->UpdateAnimation(gt.DeltaTime());
        if (obj->mLightIndex != -1) 
        {
            float flickerSpeed = 3.0f;
            float baseFlicker = 0.8f + 0.2f * sinf(gt.TotalTime() * flickerSpeed);
            float noise = (float)(rand() % 100) / 2000.0f;
            float intensity = baseFlicker + noise;
            mGameLights[obj->mLightIndex].SetStrength({ 1.0f * intensity, 0.2f * intensity, 0.05f * intensity });
        }
    }


    auto& skyRitem = mAllRitems.back();
    XMMATRIX skyWorld = XMMatrixScaling(5000.0f, 5000.0f, 5000.0f);
    XMStoreFloat4x4(&skyRitem->World, skyWorld);
    skyRitem->NumFramesDirty = gNumFrameResources;

    // 3. 상수 버퍼(GPU 메모리) 갱신
    UpdateMainPassCB(gt);
    UpdateShadowPassCB(gt);
    UpdateObjectCBs(gt);
    UpdateMaterialCBs(gt);
}

static const float ClearColor[4] = { 0.690196097f, 0.768627465f, 0.870588243f, 1.0f };

void EclipseWalkerGame::Draw(const GameTimer& gt)
{
    auto cmdListAlloc = mCurrFrameResource->CmdListAlloc;
    ThrowIfFailed(cmdListAlloc->Reset());
    ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), nullptr));

    auto shadowMap = mRenderer->GetShadowMap();

    // [Pass 1] Shadow Pass
    auto barrierShadowWrite = CD3DX12_RESOURCE_BARRIER::Transition(
        shadowMap->Resource(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE);
    mCommandList->ResourceBarrier(1, &barrierShadowWrite);

    D3D12_VIEWPORT shadowViewport = shadowMap->Viewport();
    D3D12_RECT shadowScissorRect = shadowMap->ScissorRect();
    mCommandList->RSSetViewports(1, &shadowViewport);
    mCommandList->RSSetScissorRects(1, &shadowScissorRect);

    D3D12_CPU_DESCRIPTOR_HANDLE shadowDsv = shadowMap->Dsv();
    mCommandList->OMSetRenderTargets(0, nullptr, false, &shadowDsv);
    mCommandList->ClearDepthStencilView(shadowDsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    mRenderer->DrawScene(
        mCommandList.Get(), mGameObjects, mCurrFrameResource->PassCB->Resource(),
        mSrvDescriptorHeap.Get(), mCurrFrameResource->ObjectCB->Resource(),
        mCurrFrameResource->MaterialCB->Resource(),
        mRenderer->GetShadowPSO(), 1);

    auto barrierShadowRead = CD3DX12_RESOURCE_BARRIER::Transition(
        shadowMap->Resource(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_GENERIC_READ);
    mCommandList->ResourceBarrier(1, &barrierShadowRead);

    // [Pass 2] Main Pass
    mCommandList->RSSetViewports(1, &mScreenViewport);
    mCommandList->RSSetScissorRects(1, &mScissorRect);

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle;
    CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(DepthStencilView());

    if (m4xMsaaState)
    {
        rtvHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(mRtvHeap->GetCPUDescriptorHandleForHeapStart());
        rtvHandle.Offset(2, mRtvDescriptorSize);
    }
    else
    {
        auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            CurrentBackBuffer(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
        mCommandList->ResourceBarrier(1, &barrier);
        rtvHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(CurrentBackBufferView());
    }

    mCommandList->ClearRenderTargetView(rtvHandle, ClearColor, 0, nullptr);
    mCommandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

    mCommandList->OMSetRenderTargets(1, &rtvHandle, true, &dsvHandle);

    // 1. 스카이박스
    mRenderer->DrawSkybox(
        mCommandList.Get(),
        mAllRitems,
        mSrvDescriptorHeap.Get(),
        mSkyTexHeapIndex,
        mCurrFrameResource->ObjectCB->Resource(),
        mCurrFrameResource->PassCB->Resource()
    );

    // 2. 일반 물체
    mRenderer->DrawScene(
        mCommandList.Get(), mGameObjects, mCurrFrameResource->PassCB->Resource(),
        mSrvDescriptorHeap.Get(), mCurrFrameResource->ObjectCB->Resource(),
        mCurrFrameResource->MaterialCB->Resource(),
        mRenderer->GetPSO(),
        0);

    // 3. 외곽선
    mRenderer->DrawScene(
        mCommandList.Get(), mGameObjects, mCurrFrameResource->PassCB->Resource(),
        mSrvDescriptorHeap.Get(), mCurrFrameResource->ObjectCB->Resource(),
        mCurrFrameResource->MaterialCB->Resource(),
        mRenderer->GetOutlinePSO(),
        0);

    // 4. 투명 물체
    mRenderer->DrawScene(
        mCommandList.Get(),
        mGameObjects,
        mCurrFrameResource->PassCB->Resource(),
        mSrvDescriptorHeap.Get(),
        mCurrFrameResource->ObjectCB->Resource(),
        mCurrFrameResource->MaterialCB->Resource(),
        mRenderer->GetTransparentPSO(),
        0);

    if (m4xMsaaState)
    {
        D3D12_RESOURCE_BARRIER barriers[2] = {
            CD3DX12_RESOURCE_BARRIER::Transition(mMSAART.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_RESOLVE_SOURCE),
            CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RESOLVE_DEST)
        };
        mCommandList->ResourceBarrier(2, barriers);

        mCommandList->ResolveSubresource(
            CurrentBackBuffer(), 0, mMSAART.Get(), 0, DXGI_FORMAT_R8G8B8A8_UNORM);

        D3D12_RESOURCE_BARRIER restoreBarriers[2] = {
            CD3DX12_RESOURCE_BARRIER::Transition(mMSAART.Get(), D3D12_RESOURCE_STATE_RESOLVE_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET),
            CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(), D3D12_RESOURCE_STATE_RESOLVE_DEST, D3D12_RESOURCE_STATE_PRESENT)
        };
        mCommandList->ResourceBarrier(2, restoreBarriers);
    }
    else
    {
        auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            CurrentBackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
        mCommandList->ResourceBarrier(1, &barrier);
    }

    ThrowIfFailed(mCommandList->Close());
    ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    ThrowIfFailed(mSwapChain->Present(0, 0));
    mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

    mCurrFrameResource->Fence = ++mCurrentFence;
    mCommandQueue->Signal(mFence.Get(), mCurrentFence);
}

void EclipseWalkerGame::BuildShapeGeometry()
{
    // Assimp 로드
    MapMeshData mapData;
    string path = "Models/Map/Map.fbx";
    if (!ModelLoader::Load(path, mapData))
    {
        MessageBox(0, L"Map Load Failed!", 0, 0);
        return;
    }
    mMapSubsets = mapData.Subsets;



    // 버퍼 생성 로직
    const UINT vbByteSize = (UINT)mapData.Vertices.size() * sizeof(Vertex);
    const UINT ibByteSize = (UINT)mapData.Indices.size() * sizeof(std::uint32_t);

    auto geo = std::make_unique<MeshGeometry>();
    geo->Name = "mapGeo";

    ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
    CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), mapData.Vertices.data(), vbByteSize);

    ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
    CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), mapData.Indices.data(), ibByteSize);

    geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
        mCommandList.Get(), mapData.Vertices.data(), vbByteSize, geo->VertexBufferUploader);

    geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
        mCommandList.Get(), mapData.Indices.data(), ibByteSize, geo->IndexBufferUploader);

    geo->VertexByteStride = sizeof(Vertex);
    geo->VertexBufferByteSize = vbByteSize;
    geo->IndexFormat = DXGI_FORMAT_R32_UINT;
    geo->IndexBufferByteSize = ibByteSize;

    for (const auto& subset : mMapSubsets)
    {
        SubmeshGeometry submesh;
        submesh.IndexCount = subset.IndexCount;
        submesh.StartIndexLocation = subset.IndexStart;
        submesh.BaseVertexLocation = 0;
        geo->DrawArgs["subset_" + std::to_string(subset.Id)] = submesh;
    }

    mResources->mGeometries[geo->Name] = std::move(geo);

    // ========================================================
    // 사각형(Quad) 메쉬 생성
    // ========================================================

    // 1. 정점 4개 정의 (왼쪽아래, 왼쪽위, 오른쪽위, 오른쪽아래)
    std::array<Vertex, 4> quadVertices =
    {
        //  Position(위치)              Normal(법선)              TexC(UV)          Tangent(접선)
        Vertex({ XMFLOAT3(-1.0f, -1.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT2(0.0f, 1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f) }),
        Vertex({ XMFLOAT3(-1.0f,  1.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT2(0.0f, 0.0f), XMFLOAT3(1.0f, 0.0f, 0.0f) }),
        Vertex({ XMFLOAT3(1.0f,  1.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT2(1.0f, 0.0f), XMFLOAT3(1.0f, 0.0f, 0.0f) }),
        Vertex({ XMFLOAT3(1.0f, -1.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT2(1.0f, 1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f) })
    };

    std::array<std::uint16_t, 6> quadIndices =
    {
        0, 1, 2, 
        0, 2, 3  
    };

    const UINT quadVbByteSize = (UINT)quadVertices.size() * sizeof(Vertex);
    const UINT quadIbByteSize = (UINT)quadIndices.size() * sizeof(std::uint16_t);

    auto quadGeo = std::make_unique<MeshGeometry>();
    quadGeo->Name = "quadGeo"; 

    ThrowIfFailed(D3DCreateBlob(quadVbByteSize, &quadGeo->VertexBufferCPU));
    CopyMemory(quadGeo->VertexBufferCPU->GetBufferPointer(), quadVertices.data(), quadVbByteSize);

    ThrowIfFailed(D3DCreateBlob(quadIbByteSize, &quadGeo->IndexBufferCPU));
    CopyMemory(quadGeo->IndexBufferCPU->GetBufferPointer(), quadIndices.data(), quadIbByteSize);

    quadGeo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
        mCommandList.Get(), quadVertices.data(), quadVbByteSize, quadGeo->VertexBufferUploader);

    quadGeo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
        mCommandList.Get(), quadIndices.data(), quadIbByteSize, quadGeo->IndexBufferUploader);

    quadGeo->VertexByteStride = sizeof(Vertex);
    quadGeo->VertexBufferByteSize = quadVbByteSize;
    quadGeo->IndexFormat = DXGI_FORMAT_R16_UINT; 
    quadGeo->IndexBufferByteSize = quadIbByteSize;

    SubmeshGeometry quadSubmesh;
    quadSubmesh.IndexCount = (UINT)quadIndices.size();
    quadSubmesh.StartIndexLocation = 0;
    quadSubmesh.BaseVertexLocation = 0;

    quadGeo->DrawArgs["quad"] = quadSubmesh;

    mResources->mGeometries[quadGeo->Name] = std::move(quadGeo);

    // ========================================================
    // 스카이박스용 박스(Cube) 메쉬 생성
    // ========================================================

    // 1. 정점 8개 (육면체 꼭짓점)
    std::array<Vertex, 8> boxVertices =
    {
        Vertex({ XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT2(0.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, 0.0f) }),
        Vertex({ XMFLOAT3(-1.0f, +1.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT2(0.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, 0.0f) }),
        Vertex({ XMFLOAT3(+1.0f, +1.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT2(0.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, 0.0f) }),
        Vertex({ XMFLOAT3(+1.0f, -1.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT2(0.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, 0.0f) }),
        Vertex({ XMFLOAT3(-1.0f, -1.0f, +1.0f), XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT2(0.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, 0.0f) }),
        Vertex({ XMFLOAT3(-1.0f, +1.0f, +1.0f), XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT2(0.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, 0.0f) }),
        Vertex({ XMFLOAT3(+1.0f, +1.0f, +1.0f), XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT2(0.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, 0.0f) }),
        Vertex({ XMFLOAT3(+1.0f, -1.0f, +1.0f), XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT2(0.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, 0.0f) })
    };

    // 2. 인덱스 (삼각형 12개 = 36개 인덱스)
    std::array<std::uint16_t, 36> boxIndices =
    {
        // 앞면
        0, 1, 2, 0, 2, 3,
        // 뒷면
        4, 6, 5, 4, 7, 6,
        // 왼쪽
        4, 5, 1, 4, 1, 0,
        // 오른쪽
        3, 2, 6, 3, 6, 7,
        // 윗면
        1, 5, 6, 1, 6, 2,
        // 아랫면
        4, 0, 3, 4, 3, 7
    };

    const UINT boxVbByteSize = (UINT)boxVertices.size() * sizeof(Vertex);
    const UINT boxIbByteSize = (UINT)boxIndices.size() * sizeof(std::uint16_t);

    auto boxGeo = std::make_unique<MeshGeometry>();
    boxGeo->Name = "boxGeo";

    ThrowIfFailed(D3DCreateBlob(boxVbByteSize, &boxGeo->VertexBufferCPU));
    CopyMemory(boxGeo->VertexBufferCPU->GetBufferPointer(), boxVertices.data(), boxVbByteSize);

    ThrowIfFailed(D3DCreateBlob(boxIbByteSize, &boxGeo->IndexBufferCPU));
    CopyMemory(boxGeo->IndexBufferCPU->GetBufferPointer(), boxIndices.data(), boxIbByteSize);

    boxGeo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
        mCommandList.Get(), boxVertices.data(), boxVbByteSize, boxGeo->VertexBufferUploader);

    boxGeo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
        mCommandList.Get(), boxIndices.data(), boxIbByteSize, boxGeo->IndexBufferUploader);

    boxGeo->VertexByteStride = sizeof(Vertex);
    boxGeo->VertexBufferByteSize = boxVbByteSize;
    boxGeo->IndexFormat = DXGI_FORMAT_R16_UINT;
    boxGeo->IndexBufferByteSize = boxIbByteSize;

    SubmeshGeometry boxSubmesh;
    boxSubmesh.IndexCount = (UINT)boxIndices.size();
    boxSubmesh.StartIndexLocation = 0;
    boxSubmesh.BaseVertexLocation = 0;

    boxGeo->DrawArgs["box"] = boxSubmesh;

    mResources->mGeometries[boxGeo->Name] = std::move(boxGeo);
}

void EclipseWalkerGame::BuildMaterials()
{
    std::vector<std::string> texNames = ModelLoader::LoadTextureNames("Models/Map/Map.fbx");
    int mapMatCount = texNames.size(); 

    // -------------------------------------------------------
    // 맵 재질들 자동 생성 (Stones, Wood 등)
    // -------------------------------------------------------
    for (int i = 0; i < mapMatCount; ++i)
    {
        auto mat = std::make_unique<Material>();
        mat->Name = "Mat_" + std::to_string(i);
        mat->MatCBIndex = i;
        mat->DiffuseSrvHeapIndex = i * 4;

        mat->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
        mat->FresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05f);
        mat->Roughness = 0.8f;
        mResources->CreateMaterial(mat->Name, mat->MatCBIndex, mat->DiffuseAlbedo, mat->FresnelR0, mat->Roughness);
        Material* storedMat = mResources->GetMaterial(mat->Name);
        if (storedMat != nullptr)
        {
            storedMat->DiffuseSrvHeapIndex = i * 4;
            storedMat->IsToon = 0;
            storedMat->IsTransparent = 0; 
            storedMat->NumFramesDirty = 3;
        }
    }

    // -------------------------------------------------------
    // 'Fire' 재질 수동 생성
    // -------------------------------------------------------
    auto fireMat = std::make_unique<Material>();
    fireMat->Name = "Fire_Mat";

    fireMat->MatCBIndex = mResources->mMaterials.size();

    fireMat->DiffuseSrvHeapIndex = mapMatCount * 4;

    fireMat->DiffuseAlbedo = XMFLOAT4(1.0f, 0.3f, 0.1f, 0.8f);
    fireMat->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
    fireMat->Roughness = 0.1f;

    fireMat->IsTransparent = 1; 
    fireMat->IsToon = 0;        
    fireMat->NumFramesDirty = 3;
    mResources->mMaterials["Fire_Mat"] = std::move(fireMat);
}

void EclipseWalkerGame::BuildRenderItems()
{
    // 맵 렌더 아이템 생성
    for (const auto& subset : mMapSubsets)
    {
        auto ritem = std::make_unique<RenderItem>();
        ritem->World = MathHelper::Identity4x4();
        ritem->TexTransform = MathHelper::Identity4x4();
        ritem->Geo = mResources->mGeometries["mapGeo"].get();

        string subsetName = "subset_" + std::to_string(subset.Id);
        ritem->IndexCount = ritem->Geo->DrawArgs[subsetName].IndexCount;
        ritem->BaseVertexLocation = ritem->Geo->DrawArgs[subsetName].BaseVertexLocation;
        ritem->StartIndexLocation = ritem->Geo->DrawArgs[subsetName].StartIndexLocation;

        string matName = "Mat_" + std::to_string(subset.MaterialIndex);
        Material* mat = mResources->GetMaterial(matName);

        ritem->Mat = mat;
        ritem->ObjCBIndex = mAllRitems.size();

        // GameObject 생성
        auto mapObj = std::make_unique<GameObject>();
        mapObj->SetScale(0.01f, 0.01f, 0.01f);
        mapObj->SetPosition(0.0f, 0.0f, 0.0f);
        mapObj->Ritem = ritem.get();
        mapObj->Update();

        mAllRitems.push_back(std::move(ritem));
        mGameObjects.push_back(std::move(mapObj));
    }
    CreateFire(-0.1f, 0.8f, 1.1f, 0.3f);
    CreateFire(4.1f, 0.8f, 1.1f, 0.3f);

    BuildPlayer();


    auto skyRitem = std::make_unique<RenderItem>();

    XMStoreFloat4x4(&skyRitem->World, XMMatrixScaling(5000.0f, 5000.0f, 5000.0f));

    skyRitem->TexTransform = MathHelper::Identity4x4();
    skyRitem->ObjCBIndex = mAllRitems.size();
    skyRitem->Mat = mResources->GetMaterial("Mat_0");

    skyRitem->Geo = mResources->mGeometries["boxGeo"].get();
    skyRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    auto& drawArgs = skyRitem->Geo->DrawArgs["box"];

    skyRitem->IndexCount = drawArgs.IndexCount;
    skyRitem->StartIndexLocation = drawArgs.StartIndexLocation;
    skyRitem->BaseVertexLocation = drawArgs.BaseVertexLocation;
    mAllRitems.push_back(std::move(skyRitem));
}

void EclipseWalkerGame::LoadTextures()
{
    OutputDebugStringA("\n================== [텍스처 파일 로딩 시작] ==================\n");

    // 1. 모델에서 텍스처 이름 목록 가져오기
    std::string modelPath = "Models/Map/Map.fbx";
    std::vector<std::string> texNames = ModelLoader::LoadTextureNames(modelPath);

    // 2. 맵 텍스처 로딩 루프
    for (const auto& originName : texNames)
    {
        if (originName.empty()) continue;

        std::string baseName = originName.substr(0, originName.find_last_of('.'));

        // 헬퍼 함수: 파일 있으면 로딩, 없으면 스킵
        auto LoadMapTex = [&](std::string suffix) {
            std::string name = baseName + suffix;
            std::wstring path = L"Models/Map/Textures/" + std::wstring(name.begin(), name.end()) + L".dds";

            if (GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES)
            {
                mResources->LoadTexture(name, path); 
            }
            };

        // 각 타입별 텍스처 로딩
        LoadMapTex("");           // Diffuse
        LoadMapTex("_normal");    // Normal
        LoadMapTex("_emissive");  // Emissive
        LoadMapTex("_metallic");  // Metallic
    }

    // 3. 수동 텍스처 로딩 (Fire, Skybox)
    mResources->LoadTexture("Fire", L"Models/Map/Textures/Fire_1.dds");
    mResources->LoadTexture("skyTex", L"Textures/sky.dds"); 
    mResources->LoadTexture("blueTex", L"Textures/Blue.dds");

    OutputDebugStringA("\n================== [텍스처 파일 로딩 종료] ==================\n");
}

void EclipseWalkerGame::BuildDescriptorHeaps()
{
    OutputDebugStringA("\n================== [서술자 힙(SRV Heap) 생성 시작] ==================\n");

    // -------------------------------------------------------
    // 1. 힙(Heap) 생성
    // -------------------------------------------------------
    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
    srvHeapDesc.NumDescriptors = 2048;
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mSrvDescriptorHeap)));

    CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
    UINT descriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    // [카운터] 현재 몇 번째 칸인지 세는 변수
    int currentHeapIndex = 0;


    // -------------------------------------------------------
    // 2. 맵(Map) 텍스처 등록 (한 텍스처당 4칸씩)
    // -------------------------------------------------------
    std::string modelPath = "Models/Map/Map.fbx";
    std::vector<std::string> texNames = ModelLoader::LoadTextureNames(modelPath);

    for (int i = 0; i < texNames.size(); ++i)
    {
        std::string originName = texNames[i];

        if (originName.empty())
        {
            hDescriptor.Offset(4, descriptorSize);
            currentHeapIndex += 4;
            continue;
        }

        std::string baseName = originName.substr(0, originName.find_last_of('.'));

        // 람다 함수: 텍스처 등록 및 로그 출력
        auto CreateView = [&](std::string suffix, std::string logName, std::string fallbackName = "")
            {
                std::string targetName = baseName + suffix;
                auto tex = mResources->GetTexture(targetName);

                if (!tex && !fallbackName.empty())
                {
                    tex = mResources->GetTexture(fallbackName);
                }

                if (tex)
                {
                    CreateSRV(tex, hDescriptor);
                }

                // 로그 출력 (디버깅용)
                std::string log = "Map[" + std::to_string(i) + "] " + logName + 
                                   " -> Index: " + std::to_string(currentHeapIndex) + "\n";
                 OutputDebugStringA(log.c_str());

                hDescriptor.Offset(1, descriptorSize);
                currentHeapIndex++; 
            };

        CreateView("", "Diffuse");
        CreateView("_normal", "Normal", "Stones_normal");
        CreateView("_emissive", "Emissive");
        CreateView("_metallic", "Metallic");
    }


    // -------------------------------------------------------
    // 3. Fire 텍스처 등록 (1칸)
    // -------------------------------------------------------
    auto fireTex = mResources->GetTexture("Fire");
    if (fireTex)
    {
        CreateSRV(fireTex, hDescriptor);
        std::string log = ">> [Fire] 불 텍스처 -> Index: " + std::to_string(currentHeapIndex) + "\n";
        OutputDebugStringA(log.c_str());
    }

    hDescriptor.Offset(1, descriptorSize); 
    currentHeapIndex++;                    


    // -------------------------------------------------------
    // 4. 스카이박스 (Skybox) 등록 (1칸)
    // -------------------------------------------------------
    auto skyTex = mResources->GetTexture("skyTex");
    if (skyTex)
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = skyTex->Resource->GetDesc().Format;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
        srvDesc.TextureCube.MostDetailedMip = 0;
        srvDesc.TextureCube.MipLevels = skyTex->Resource->GetDesc().MipLevels;
        srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
        md3dDevice->CreateShaderResourceView(skyTex->Resource.Get(), &srvDesc, hDescriptor);

        mSkyTexHeapIndex = currentHeapIndex; 

        std::string log = ">> [Skybox] 하늘 -> Index: " + std::to_string(currentHeapIndex) + "\n";
        OutputDebugStringA(log.c_str());
    }
    else
    {
        OutputDebugStringA("[Heap] 경고: Skybox 텍스처 없음\n");
    }

    hDescriptor.Offset(1, descriptorSize);
    currentHeapIndex++;                    


    // -------------------------------------------------------
    // 5. 플레이어 (Player) 등록 
    // -------------------------------------------------------
    auto playerTex = mResources->GetTexture("blueTex"); 
    if (playerTex)
    {
        CreateSRV(playerTex, hDescriptor);
        std::string log = ">> [Player] 플레이어 -> Index: " + std::to_string(currentHeapIndex) + "\n";
        OutputDebugStringA(log.c_str());
    }
    else
    {
        // 텍스처 없으면 빈 뷰 생성 (에러 방지)
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        md3dDevice->CreateShaderResourceView(nullptr, &srvDesc, hDescriptor);
        OutputDebugStringA(">> [Heap] 경고: PlayerTex를 찾을 수 없음\n");
    }

    hDescriptor.Offset(1, descriptorSize);
    currentHeapIndex++;


    // -------------------------------------------------------
    // 6. 그림자 맵 (Shadow Map)
    // -------------------------------------------------------
    int shadowMapIndex = 1000;
    
    // CPU 핸들 이동
    CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv(mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
    hCpuSrv.Offset(shadowMapIndex, descriptorSize);

    // GPU 핸들 이동
    CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
    hGpuSrv.Offset(shadowMapIndex, descriptorSize);

    // DSV 핸들 준비 (이건 그대로)
    CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuDsv(mDsvHeap->GetCPUDescriptorHandleForHeapStart());
    UINT dsvSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    hCpuDsv.Offset(1, dsvSize);

    if (mRenderer->GetShadowMap())
    {
        mRenderer->GetShadowMap()->BuildDescriptors(hCpuSrv, hGpuSrv, hCpuDsv);

        // 로그 출력 수정
        std::string log = "[Heap] ShadowMap 등록 완료 (Index: " + std::to_string(shadowMapIndex) + ")\n";
        OutputDebugStringA(log.c_str());
    }

    OutputDebugStringA("================== [서술자 힙 생성 종료] ==================\n");
}

void EclipseWalkerGame::CreateSRV(Texture* tex, D3D12_CPU_DESCRIPTOR_HANDLE hDescriptor)
{
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

    if (tex && tex->Resource)
    {
        srvDesc.Format = tex->Resource->GetDesc().Format;
        srvDesc.Texture2D.MipLevels = tex->Resource->GetDesc().MipLevels;
        md3dDevice->CreateShaderResourceView(tex->Resource.Get(), &srvDesc, hDescriptor);
    }
    else
    {
        // 텍스처가 없으면 기본값(검은색/투명)으로 뷰 생성
        srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        srvDesc.Texture2D.MipLevels = 1;
        md3dDevice->CreateShaderResourceView(nullptr, &srvDesc, hDescriptor);
    }
}

void EclipseWalkerGame::BuildFrameResources()
{
    UINT objCount = (UINT)mAllRitems.size();

    UINT matCount = (UINT)mResources->mMaterials.size();
    UINT passCount = 2;

    for (int i = 0; i < gNumFrameResources; ++i)
    {
        mFrameResources.push_back(std::make_unique<FrameResource>(md3dDevice.Get(),
            passCount, objCount, matCount));
    }
}

float EclipseWalkerGame::AspectRatio() const
{
    return static_cast<float>(mClientWidth) / mClientHeight;
}

LRESULT EclipseWalkerGame::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_LBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_RBUTTONDOWN:
        OnMouseDown(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;
    case WM_LBUTTONUP:
    case WM_MBUTTONUP:
    case WM_RBUTTONUP:
        OnMouseUp(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;
    case WM_MOUSEMOVE:
        OnMouseMove(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;
    }
    return GameFramework::MsgProc(hwnd, msg, wParam, lParam);
}

void EclipseWalkerGame::OnKeyboardInput(const GameTimer& gt)
{
    if (GetAsyncKeyState(VK_ESCAPE) & 0x8000)
        PostQuitMessage(0);
}

void EclipseWalkerGame::OnMouseDown(WPARAM btnState, int x, int y)
{
    mLastMousePos.x = x;
    mLastMousePos.y = y;
    SetCapture(mhMainWnd);
    SetFocus(mhMainWnd);
}

void EclipseWalkerGame::OnMouseUp(WPARAM btnState, int x, int y)
{
    ReleaseCapture();
}

void EclipseWalkerGame::OnMouseMove(WPARAM btnState, int x, int y)
{
    // 마우스 우클릭 상태일 때만 화면 회전 
    if ((btnState & MK_RBUTTON) != 0)
    {
        // 마우스 이동량에 따라 회전 각도 계산 (감도 0.25)
        float dx = XMConvertToRadians(0.25f * static_cast<float>(x - mLastMousePos.x));
        float dy = XMConvertToRadians(0.25f * static_cast<float>(y - mLastMousePos.y));

        // 카메라 회전 (Pitch: 위아래, RotateY: 좌우)
        mCamera.Pitch(dy);
        mCamera.RotateY(dx);
    }

    mLastMousePos.x = x;
    mLastMousePos.y = y;
}

void EclipseWalkerGame::InitLights()
{
    mGameLights.resize(MaxLights);
    mGameLights[0].InitDirectional({ 0.57735f, -0.57735f, 0.57735f }, { 0.8f, 0.8f, 0.8f });
}

void EclipseWalkerGame::UpdateMainPassCB(const GameTimer& gt)
{
    XMMATRIX view = mCamera.GetView();
    XMMATRIX proj = mCamera.GetProj();
    XMMATRIX viewProj = XMMatrixMultiply(view, proj);
    XMMATRIX invView = MathHelper::Inverse(view);
    XMMATRIX invProj = MathHelper::Inverse(proj);
    XMMATRIX invViewProj = MathHelper::Inverse(viewProj);

    PassConstants mMainPassCB;
    XMStoreFloat4x4(&mMainPassCB.View, XMMatrixTranspose(view));
    XMStoreFloat4x4(&mMainPassCB.InvView, XMMatrixTranspose(invView));
    XMStoreFloat4x4(&mMainPassCB.Proj, XMMatrixTranspose(proj));
    XMStoreFloat4x4(&mMainPassCB.InvProj, XMMatrixTranspose(invProj));
    XMStoreFloat4x4(&mMainPassCB.ViewProj, XMMatrixTranspose(viewProj));
    XMStoreFloat4x4(&mMainPassCB.InvViewProj, XMMatrixTranspose(invViewProj));

    Light sunLight = mGameLights[0].GetRawData();
    XMVECTOR lightDir = XMLoadFloat3(&sunLight.Direction);
    XMVECTOR targetPos = mCamera.GetPosition(); 
    XMVECTOR lightPos = targetPos - (100.0f * lightDir);
    XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

    XMMATRIX lightView = XMMatrixLookAtLH(lightPos, targetPos, up);

    // 2. 조명의 Projection 행렬 만들기
    XMMATRIX lightProj = XMMatrixOrthographicLH(100.0f, 100.0f, 1.0f, 1000.0f);

    // 3. NDC 좌표([-1,1])를 텍스처 좌표([0,1])로 바꾸는 행렬 T
    // x: [-1, 1] -> [0, 1]  => 0.5x + 0.5
    // y: [-1, 1] -> [1, 0]  => -0.5y + 0.5 (텍스처는 Y가 아래로 증가하므로 뒤집음)
    XMMATRIX T(
        0.5f, 0.0f, 0.0f, 0.0f,
        0.0f, -0.5f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.5f, 0.5f, 0.0f, 1.0f);

    // 4. 최종 행렬 결합: World -> Light View -> Light Proj -> Texture UV
    XMMATRIX S = lightView * lightProj * T;

    // 5. 구조체에 저장 
    XMStoreFloat4x4(&mMainPassCB.ShadowTransform, XMMatrixTranspose(S));

    mMainPassCB.EyePosW = mCamera.GetPosition3f();
    mMainPassCB.RenderTargetSize = { (float)mClientWidth, (float)mClientHeight };
    mMainPassCB.InvRenderTargetSize = { 1.0f / mClientWidth, 1.0f / mClientHeight };
    mMainPassCB.NearZ = 1.0f;
    mMainPassCB.FarZ = 10000.0f;
    mMainPassCB.TotalTime = gt.TotalTime();
    mMainPassCB.DeltaTime = gt.DeltaTime();
    mMainPassCB.AmbientLight = { 0.25f, 0.25f, 0.35f, 1.0f };
    for (int i = 0; i < MaxLights; ++i)
    {
        mGameLights[i].Update(gt.DeltaTime());
        mMainPassCB.Lights[i] = mGameLights[i].GetRawData();
    }
    mCurrFrameResource->PassCB->CopyData(0, mMainPassCB);
}

void EclipseWalkerGame::UpdateShadowPassCB(const GameTimer& gt)
{
    // 1. 빛의 시점 (View Matrix)
    Light sunLight = mGameLights[0].GetRawData();
    XMVECTOR lightDir = XMLoadFloat3(&sunLight.Direction);
    XMVECTOR targetPos = mCamera.GetPosition();
    XMVECTOR lightPos = targetPos - (100.0f * lightDir);
    XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

    XMMATRIX lightView = XMMatrixLookAtLH(lightPos, targetPos, up);

    // 2. 빛의 범위 (Projection Matrix)
    XMMATRIX lightProj = XMMatrixOrthographicLH(100.0f, 100.0f, 1.0f, 1000.0f);

    // 3. PassCB 구조체 채우기
    XMMATRIX viewProj = XMMatrixMultiply(lightView, lightProj);

    PassConstants shadowPassCB;
    XMStoreFloat4x4(&shadowPassCB.View, XMMatrixTranspose(lightView));
    XMStoreFloat4x4(&shadowPassCB.Proj, XMMatrixTranspose(lightProj));
    XMStoreFloat4x4(&shadowPassCB.ViewProj, XMMatrixTranspose(viewProj));


    shadowPassCB.EyePosW = { 0.0f, 0.0f, 0.0f };
    shadowPassCB.RenderTargetSize = { 4096.0f, 4096.0f };
    shadowPassCB.InvRenderTargetSize = { 1.0f / 4096.0f, 1.0f / 4096.0f };
    shadowPassCB.NearZ = 1.0f;
    shadowPassCB.FarZ = 100.0f;

    mCurrFrameResource->PassCB->CopyData(1, shadowPassCB);
}

void EclipseWalkerGame::UpdateObjectCBs(const GameTimer& gt)
{
    auto currObjectCB = mCurrFrameResource->ObjectCB.get();
    for (auto& e : mAllRitems)
    {
        if (e->NumFramesDirty > 0)
        {
            XMMATRIX world = XMLoadFloat4x4(&e->World);
            ObjectConstants objConstants;
            XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));
            XMMATRIX texTransform = XMLoadFloat4x4(&e->TexTransform);
            XMStoreFloat4x4(&objConstants.TexTransform, XMMatrixTranspose(texTransform));
            if (e->Mat != nullptr)
            {
                objConstants.DiffuseAlbedo = e->Mat->DiffuseAlbedo;
                objConstants.FresnelR0 = e->Mat->FresnelR0;
                objConstants.Roughness = e->Mat->Roughness;
            }
            currObjectCB->CopyData(e->ObjCBIndex, objConstants);
            e->NumFramesDirty--;
        }
    }
}

void EclipseWalkerGame::UpdateMaterialCBs(const GameTimer& gt)
{
    auto currMaterialCB = mCurrFrameResource->MaterialCB.get();
    auto& materials = mResources->mMaterials;

    for (auto& e : materials)
    {
        Material* mat = e.second.get();

        if (mat->NumFramesDirty > 0)
        {
            MaterialConstants matConstants;
            matConstants.DiffuseAlbedo = mat->DiffuseAlbedo;
            matConstants.FresnelR0 = mat->FresnelR0;
            matConstants.Roughness = mat->Roughness;
            matConstants.IsToon = mat->IsToon;
            matConstants.OutlineThickness = mat->OutlineThickness;
            matConstants.IsTransparent = mat->IsTransparent;
            matConstants.DiffuseMapIndex = mat->DiffuseSrvHeapIndex;
            matConstants.OutlineColor = mat->OutlineColor;
            currMaterialCB->CopyData(mat->MatCBIndex, matConstants);

            mat->NumFramesDirty--;
        }
    }


}

void EclipseWalkerGame::CreateFire(float x, float y, float z, float scale)
{
    int startFrame = rand() % 4;
    float randomSpeed = 0.05f + (static_cast<float>(rand()) / RAND_MAX) * 0.05f;
    auto fire = std::make_unique<RenderItem>();

    XMStoreFloat4x4(&fire->TexTransform, XMMatrixScaling(0.5f, 0.5f, 1.0f));

    fire->Geo = mResources->mGeometries["quadGeo"].get();
    fire->Mat = mResources->GetMaterial("Fire_Mat");
    fire->ObjCBIndex = mAllRitems.size();

    fire->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    if (fire->Geo && fire->Geo->DrawArgs.count("quad"))
    {
        fire->IndexCount = fire->Geo->DrawArgs["quad"].IndexCount;
        fire->StartIndexLocation = fire->Geo->DrawArgs["quad"].StartIndexLocation;
        fire->BaseVertexLocation = fire->Geo->DrawArgs["quad"].BaseVertexLocation;
    }

    auto obj = std::make_unique<GameObject>();
    obj->Ritem = fire.get();

    obj->SetPosition(x, y, z);
    obj->SetScale(scale, scale, scale); 

    obj->mIsAnimated = true;
    obj->mCurrFrame = startFrame;
    obj->mFrameDuration = randomSpeed;
    obj->mNumCols = 2;
    obj->mNumRows = 2;
    obj->mIsBillboard = true;

    if (mCurrentLightIndex < MaxLights)
    {
        // 1. 빛의 색상
        XMFLOAT3 lightColor = { 1.0f, 0.2f, 0.05f };

        // 2. 빛의 위치
        XMFLOAT3 lightPos = { x, y , z };

        // 3. 도달 거리
        float lightRange = 10.0f;

        // 조명 데이터 설정
        mGameLights[mCurrentLightIndex].InitPoint(lightColor, lightPos, lightRange);
        obj->mLightIndex = mCurrentLightIndex;
        mCurrentLightIndex++;
    }

    obj->Update();
    mAllRitems.push_back(std::move(fire));
    mGameObjects.push_back(std::move(obj));
}

void EclipseWalkerGame::BuildPlayer()
{
    // =========================================================
    // 1. 플레이어 전용 재질
    // =========================================================
    auto playerMat = std::make_unique<Material>();
    playerMat->Name = "PlayerBlue";
    playerMat->MatCBIndex = mResources->mMaterials.size();

    std::vector<std::string> texNames = ModelLoader::LoadTextureNames("Models/Map/Map.fbx");

    int playerTexIndex = 18;

    playerMat->DiffuseSrvHeapIndex = playerTexIndex;

    // [색상] 
    playerMat->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);

    // [설정]
    playerMat->Roughness = 0.4f;
    playerMat->IsToon = 0;
	playerMat->IsTransparent = 0;
    playerMat->FresnelR0 = XMFLOAT3(0.5f, 0.5f, 0.5f);

    mResources->mMaterials["PlayerBlue"] = std::move(playerMat);


    // =========================================================
    // 2. 렌더 아이템 생성
    // =========================================================
    auto playerRitem = std::make_unique<RenderItem>();
    playerRitem->World = MathHelper::Identity4x4();
    playerRitem->TexTransform = MathHelper::Identity4x4();
    playerRitem->ObjCBIndex = mAllRitems.size();

    // 재질 연결
    playerRitem->Mat = mResources->GetMaterial("PlayerBlue");

    // 박스 지오메트리 연결
    playerRitem->Geo = mResources->mGeometries["boxGeo"].get();
    playerRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    auto& boxDrawArgs = playerRitem->Geo->DrawArgs["box"];
    playerRitem->IndexCount = boxDrawArgs.IndexCount;
    playerRitem->StartIndexLocation = boxDrawArgs.StartIndexLocation;
    playerRitem->BaseVertexLocation = boxDrawArgs.BaseVertexLocation;


    // =========================================================
    // 3. 게임 오브젝트 생성
    // =========================================================
    auto playerObj = std::make_unique<GameObject>();

    playerObj->SetScale(0.3f, 0.5f, 0.3f);
    playerObj->SetPosition(1.0f, 10.0f, 0.0f);
    playerObj->Ritem = playerRitem.get();

    playerObj->Update();

    mPlayerObject = playerObj.get();

    mAllRitems.push_back(std::move(playerRitem));
    mGameObjects.push_back(std::move(playerObj));
}