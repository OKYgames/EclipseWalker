#include "EclipseWalkerGame.h"
#include "LoginScene.h"        
#include "Stage1Scene.h"
#include "Stage2Scene.h"
#include "DDSTextureLoader.h"
#include <windowsx.h>

EclipseWalkerGame::EclipseWalkerGame(HINSTANCE hInstance) : GameFramework(hInstance) {}
EclipseWalkerGame::~EclipseWalkerGame() {}

bool EclipseWalkerGame::Initialize()
{
    srand((unsigned int)time(NULL));
    m4xMsaaState = true;

    if (!GameFramework::Initialize()) return false;

    ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

    // DSV 占쏙옙 占쏙옙占쏙옙 
    {
        D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
        dsvHeapDesc.NumDescriptors = 2;
        dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        dsvHeapDesc.NodeMask = 0;
        ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&mDsvHeap)));
        CD3DX12_CPU_DESCRIPTOR_HANDLE mainDsvHandle(mDsvHeap->GetCPUDescriptorHandleForHeapStart());
        md3dDevice->CreateDepthStencilView(mDepthStencilBuffer.Get(), nullptr, mainDsvHandle);
    }

    // 占시쏙옙占쏙옙 占십깍옙화
    mResources = std::make_unique<ResourceManager>(md3dDevice.Get(), mCommandList.Get());
    mRenderer = std::make_unique<Renderer>(md3dDevice.Get());

    CD3DX12_CPU_DESCRIPTOR_HANDLE shadowHandle(mDsvHeap->GetCPUDescriptorHandleForHeapStart());
    UINT dsvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    shadowHandle.Offset(1, dsvDescriptorSize);
    mRenderer->Initialize(shadowHandle);

    // --- 占쌘억옙 占쏙옙占쏙옙 ---
    InitLights();
    BuildFrameResources();
    LoadCoreResources();  

    ThrowIfFailed(mCommandList->Close());
    ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    // GPU 占쏙옙占쏙옙화
    mCurrentFence++;
    mCommandQueue->Signal(mFence.Get(), mCurrentFence);
    if (mFence->GetCompletedValue() < mCurrentFence)
    {
        HANDLE eventHandle = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        mFence->SetEventOnCompletion(mCurrentFence, eventHandle);
        WaitForSingleObject(eventHandle, INFINITE);
        CloseHandle(eventHandle);
    }

    // 占쏙옙트占쏙옙호 확占쏙옙
    NetworkManager::Get()->ConnectAsync("127.0.0.1", 7777);

    // --- [占쏙옙 占쏙옙환] ---
    ChangeScene(std::make_unique<LoginScene>(this));
    BuildDescriptorHeaps();

    // 카占쌨띰옙 占쏙옙占쏙옙 占쏙옙占쏙옙
    mCamera.SetLens(0.25f * 3.14f, AspectRatio(), 1.0f, 10000.0f);

    return true;
}

void EclipseWalkerGame::ChangeScene(std::unique_ptr<Scene> newScene)
{
    mCurrentFence++;
    mCommandQueue->Signal(mFence.Get(), mCurrentFence);
    if (mFence->GetCompletedValue() < mCurrentFence)
    {
        HANDLE eventHandle = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        mFence->SetEventOnCompletion(mCurrentFence, eventHandle);
        WaitForSingleObject(eventHandle, INFINITE);
        CloseHandle(eventHandle);
    }

    // 1. 占쏙옙占쏙옙 占쏙옙 占쏙옙占쏙옙
    if (mCurrentScene) mCurrentScene->Exit();

    ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

    // 3. 占쏙옙 占쏙옙 占쏙옙占쏙옙
    mCurrentScene = std::move(newScene);
    mCurrentScene->Enter();

    // 4. 占싸듸옙 占쏙옙占?占쏙옙 占쏙옙占쏙옙占쏙옙占쏙옙 占쏙옙占시쇽옙 占쌥곤옙 GPU占쏙옙占쏙옙 占쏙옙占쏙옙
    ThrowIfFailed(mCommandList->Close());
    ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    mCurrentFence++;
    mCommandQueue->Signal(mFence.Get(), mCurrentFence);
    if (mFence->GetCompletedValue() < mCurrentFence)
    {
        HANDLE eventHandle = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        mFence->SetEventOnCompletion(mCurrentFence, eventHandle);
        WaitForSingleObject(eventHandle, INFINITE);
        CloseHandle(eventHandle);
    }
}

void EclipseWalkerGame::LoadCoreResources()
{
    mResources->LoadTexture("TitleTex", L"Textures/Title.dds");
    mResources->CreateMaterial("TitleMat", mResources->mMaterials.size(), "TitleTex", "", "", "",
        XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, 0.0f), 1.0f);
    mResources->GetMaterial("TitleMat")->NumFramesDirty = 3;

    std::array<Vertex, 4> quadVertices = {
        Vertex({ XMFLOAT3(-1.0f, -1.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT2(0.0f, 1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f) }),
        Vertex({ XMFLOAT3(-1.0f,  1.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT2(0.0f, 0.0f), XMFLOAT3(1.0f, 0.0f, 0.0f) }),
        Vertex({ XMFLOAT3(1.0f,  1.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT2(1.0f, 0.0f), XMFLOAT3(1.0f, 0.0f, 0.0f) }),
        Vertex({ XMFLOAT3(1.0f, -1.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT2(1.0f, 1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f) })
    };
    std::array<std::uint16_t, 6> quadIndices = { 0, 1, 2, 0, 2, 3 };

    const UINT quadVbSize = (UINT)quadVertices.size() * sizeof(Vertex);
    const UINT quadIbSize = (UINT)quadIndices.size() * sizeof(std::uint16_t);
    auto quadGeo = std::make_unique<MeshGeometry>();
    quadGeo->Name = "quadGeo";
    D3DCreateBlob(quadVbSize, &quadGeo->VertexBufferCPU); CopyMemory(quadGeo->VertexBufferCPU->GetBufferPointer(), quadVertices.data(), quadVbSize);
    D3DCreateBlob(quadIbSize, &quadGeo->IndexBufferCPU);  CopyMemory(quadGeo->IndexBufferCPU->GetBufferPointer(), quadIndices.data(), quadIbSize);
    quadGeo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(), mCommandList.Get(), quadVertices.data(), quadVbSize, quadGeo->VertexBufferUploader);
    quadGeo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(), mCommandList.Get(), quadIndices.data(), quadIbSize, quadGeo->IndexBufferUploader);
    quadGeo->VertexByteStride = sizeof(Vertex); quadGeo->VertexBufferByteSize = quadVbSize;
    quadGeo->IndexFormat = DXGI_FORMAT_R16_UINT; quadGeo->IndexBufferByteSize = quadIbSize;
    SubmeshGeometry quadSubmesh; quadSubmesh.IndexCount = (UINT)quadIndices.size(); quadSubmesh.StartIndexLocation = 0; quadSubmesh.BaseVertexLocation = 0;
    quadGeo->DrawArgs["quad"] = quadSubmesh;
    mResources->mGeometries[quadGeo->Name] = std::move(quadGeo);
}

void EclipseWalkerGame::LoadSharedGameResources()
{
    if (mIsSharedResourcesLoaded) return;
    OutputDebugStringA("\n[Shared] 占싸곤옙占쏙옙 占쏙옙占쏙옙 占쏙옙占쌀쏙옙 占싸듸옙 占쏙옙占쏙옙...\n");

    // 1. 占쏙옙占쏙옙 占쌔쏙옙처 占싸듸옙
    mResources->LoadTexture("Fire_1", L"Models/Stage1Map/Textures/Fire_1.dds");
    mResources->LoadTexture("Blue", L"Textures/Blue.dds");
    mResources->LoadTexture("white", L"Textures/white.dds");

    // Box Geometry
    std::array<Vertex, 8> boxVertices = {
        Vertex({ XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT2(0.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, 0.0f) }),
        Vertex({ XMFLOAT3(-1.0f, +1.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT2(0.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, 0.0f) }),
        Vertex({ XMFLOAT3(+1.0f, +1.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT2(0.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, 0.0f) }),
        Vertex({ XMFLOAT3(+1.0f, -1.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT2(0.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, 0.0f) }),
        Vertex({ XMFLOAT3(-1.0f, -1.0f, +1.0f), XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT2(0.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, 0.0f) }),
        Vertex({ XMFLOAT3(-1.0f, +1.0f, +1.0f), XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT2(0.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, 0.0f) }),
        Vertex({ XMFLOAT3(+1.0f, +1.0f, +1.0f), XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT2(0.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, 0.0f) }),
        Vertex({ XMFLOAT3(+1.0f, -1.0f, +1.0f), XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT2(0.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, 0.0f) })
    };
    std::array<std::uint16_t, 36> boxIndices = { 0,1,2,0,2,3, 4,6,5,4,7,6, 4,5,1,4,1,0, 3,2,6,3,6,7, 1,5,6,1,6,2, 4,0,3,4,3,7 };

    const UINT boxVbSize = (UINT)boxVertices.size() * sizeof(Vertex);
    const UINT boxIbSize = (UINT)boxIndices.size() * sizeof(std::uint16_t);
    auto boxGeo = std::make_unique<MeshGeometry>();
    boxGeo->Name = "boxGeo";
    D3DCreateBlob(boxVbSize, &boxGeo->VertexBufferCPU); CopyMemory(boxGeo->VertexBufferCPU->GetBufferPointer(), boxVertices.data(), boxVbSize);
    D3DCreateBlob(boxIbSize, &boxGeo->IndexBufferCPU);  CopyMemory(boxGeo->IndexBufferCPU->GetBufferPointer(), boxIndices.data(), boxIbSize);

    boxGeo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(), mCommandList.Get(), boxVertices.data(), boxVbSize, boxGeo->VertexBufferUploader);
    boxGeo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(), mCommandList.Get(), boxIndices.data(), boxIbSize, boxGeo->IndexBufferUploader);
    boxGeo->VertexByteStride = sizeof(Vertex); boxGeo->VertexBufferByteSize = boxVbSize;
    boxGeo->IndexFormat = DXGI_FORMAT_R16_UINT; boxGeo->IndexBufferByteSize = boxIbSize;

    SubmeshGeometry boxSubmesh;
    boxSubmesh.IndexCount = (UINT)boxIndices.size(); boxSubmesh.StartIndexLocation = 0; boxSubmesh.BaseVertexLocation = 0;
    boxGeo->DrawArgs["box"] = boxSubmesh;
    mResources->mGeometries[boxGeo->Name] = std::move(boxGeo);

    std::vector<Vertex> sphereVertices;
    std::vector<std::uint16_t> sphereIndices;

    float radius = 1.0f;
    UINT sliceCount = 30; // 占썸도 占쏙옙占쏙옙 占쏙옙
    UINT stackCount = 30; // 占쏙옙占쏙옙 占쏙옙占쏙옙 占쏙옙

    // 占쏙옙占쏙옙 占쏙옙占쏙옙 (North Pole)
    sphereVertices.push_back({ XMFLOAT3(0.0f, radius, 0.0f), XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT2(0.0f, 0.0f), XMFLOAT3(1.0f, 0.0f, 0.0f) });

    float phiStep = XM_PI / stackCount;
    float thetaStep = 2.0f * XM_PI / sliceCount;

    // 占쌩곤옙 占쏙옙占쏙옙占?占쏙옙占쏙옙
    for (UINT i = 1; i <= stackCount - 1; ++i) {
        float phi = i * phiStep;
        for (UINT j = 0; j <= sliceCount; ++j) {
            float theta = j * thetaStep;

            Vertex v;
            v.Pos.x = radius * sinf(phi) * cosf(theta);
            v.Pos.y = radius * cosf(phi);
            v.Pos.z = radius * sinf(phi) * sinf(theta);

            v.Normal = v.Pos;
            XMVECTOR n = XMVector3Normalize(XMLoadFloat3(&v.Normal));
            XMStoreFloat3(&v.Normal, n);

            v.TexC.x = (float)j / sliceCount;
            v.TexC.y = (float)i / stackCount;

            v.TangentU.x = -radius * sinf(phi) * sinf(theta);
            v.TangentU.y = 0.0f;
            v.TangentU.z = radius * sinf(phi) * cosf(theta);
            XMVECTOR t = XMVector3Normalize(XMLoadFloat3(&v.TangentU));
            XMStoreFloat3(&v.TangentU, t);

            sphereVertices.push_back(v);
        }
    }

    // 占싣뤄옙占쏙옙 占쏙옙占쏙옙 (South Pole)
    sphereVertices.push_back({ XMFLOAT3(0.0f, -radius, 0.0f), XMFLOAT3(0.0f, -1.0f, 0.0f), XMFLOAT2(0.0f, 1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f) });

    // 占싸듸옙占쏙옙 占쏙옙占쏙옙: 占쏙옙占쏙옙 캡
    for (UINT i = 1; i <= sliceCount; ++i) {
        sphereIndices.push_back(0);
        sphereIndices.push_back(i + 1);
        sphereIndices.push_back(i);
    }

    // 占싸듸옙占쏙옙 占쏙옙占쏙옙: 占쌩곤옙 占쏙옙占쏙옙占?
    UINT baseIndex = 1;
    UINT ringVertexCount = sliceCount + 1;
    for (UINT i = 0; i < stackCount - 2; ++i) {
        for (UINT j = 0; j < sliceCount; ++j) {
            sphereIndices.push_back(baseIndex + i * ringVertexCount + j);
            sphereIndices.push_back(baseIndex + i * ringVertexCount + j + 1);
            sphereIndices.push_back(baseIndex + (i + 1) * ringVertexCount + j);

            sphereIndices.push_back(baseIndex + (i + 1) * ringVertexCount + j);
            sphereIndices.push_back(baseIndex + i * ringVertexCount + j + 1);
            sphereIndices.push_back(baseIndex + (i + 1) * ringVertexCount + j + 1);
        }
    }

    // 占싸듸옙占쏙옙 占쏙옙占쏙옙: 占싣뤄옙占쏙옙 캡
    UINT southPoleIndex = (UINT)sphereVertices.size() - 1;
    baseIndex = southPoleIndex - ringVertexCount;
    for (UINT i = 0; i < sliceCount; ++i) {
        sphereIndices.push_back(southPoleIndex);
        sphereIndices.push_back(baseIndex + i);
        sphereIndices.push_back(baseIndex + i + 1);
    }

    // 占쏙옙체 占쏙옙占쏙옙占싶몌옙 GPU占쏙옙 占쏙옙占싸듸옙
    const UINT sphereVbSize = (UINT)sphereVertices.size() * sizeof(Vertex);
    const UINT sphereIbSize = (UINT)sphereIndices.size() * sizeof(std::uint16_t);
    auto sphereGeo = std::make_unique<MeshGeometry>();
    sphereGeo->Name = "sphereGeo";
    D3DCreateBlob(sphereVbSize, &sphereGeo->VertexBufferCPU); CopyMemory(sphereGeo->VertexBufferCPU->GetBufferPointer(), sphereVertices.data(), sphereVbSize);
    D3DCreateBlob(sphereIbSize, &sphereGeo->IndexBufferCPU);  CopyMemory(sphereGeo->IndexBufferCPU->GetBufferPointer(), sphereIndices.data(), sphereIbSize);

    sphereGeo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(), mCommandList.Get(), sphereVertices.data(), sphereVbSize, sphereGeo->VertexBufferUploader);
    sphereGeo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(), mCommandList.Get(), sphereIndices.data(), sphereIbSize, sphereGeo->IndexBufferUploader);
    sphereGeo->VertexByteStride = sizeof(Vertex); sphereGeo->VertexBufferByteSize = sphereVbSize;
    sphereGeo->IndexFormat = DXGI_FORMAT_R16_UINT; sphereGeo->IndexBufferByteSize = sphereIbSize;

    SubmeshGeometry sphereSubmesh;
    sphereSubmesh.IndexCount = (UINT)sphereIndices.size(); sphereSubmesh.StartIndexLocation = 0; sphereSubmesh.BaseVertexLocation = 0;
    sphereGeo->DrawArgs["sphere"] = sphereSubmesh;
    mResources->mGeometries[sphereGeo->Name] = std::move(sphereGeo);

    // 3. 占쏙옙占쏙옙 占쏙옙占쏙옙 占쏙옙占쏙옙 
    mResources->CreateMaterial("Fire_Mat", mResources->mMaterials.size(), "Fire_1", "", "", "", XMFLOAT4(1.0f, 0.3f, 0.1f, 0.8f), XMFLOAT3(0.1f, 0.1f, 0.1f), 0.1f);
    if (auto mat = mResources->GetMaterial("Fire_Mat")) { mat->IsTransparent = 1; mat->NumFramesDirty = 3; }

    mResources->CreateMaterial("PlayerBlue", mResources->mMaterials.size(), "Blue", "", "", "", XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f), XMFLOAT3(0.04f, 0.04f, 0.04f), 0.8f);
    if (auto mat = mResources->GetMaterial("PlayerBlue")) mat->NumFramesDirty = 3;

    mResources->CreateMaterial("MonsterRed", mResources->mMaterials.size(), "white", "", "", "", XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f), XMFLOAT3(0.04f, 0.04f, 0.04f), 0.8f);
    if (auto mat = mResources->GetMaterial("MonsterRed")) mat->NumFramesDirty = 3;

    mResources->CreateMaterial("DomainMat", mResources->mMaterials.size(), "white", "", "", "",
        XMFLOAT4(0.5f, 0.1f, 0.8f, 1.0f), XMFLOAT3(0.5f, 0.5f, 0.5f), 0.1f);
    if (auto domainMat = mResources->GetMaterial("DomainMat"))
    {
        domainMat->IsTransparent = 1; 
        domainMat->NumFramesDirty = 3;
    }

    // 4. 占쏙옙占쏙옙占쏙옙트 占쏙옙占쏙옙
    BuildPlayer();

    // 5. 占시뤄옙占싱억옙 占쏙옙占쏙옙 占십깍옙화
    if (!mPlayer) mPlayer = std::make_unique<Player>();
    mPlayer->Initialize(mPlayerObject, &mCamera);

	// 6. UI 占시쏙옙占쏙옙 占십깍옙화
    mUIManager = std::make_unique<UIManager>(this);
    mUIManager->BuildInGameUI();

    mIsSharedResourcesLoaded = true;
}

void EclipseWalkerGame::UnloadSharedGameResources()
{
    mIsSharedResourcesLoaded = false;
}

void EclipseWalkerGame::OnResize()
{
    GameFramework::OnResize();
    mCamera.SetLens(0.25f * 3.14f, AspectRatio(), 1.0f, 10000.0f);
}

void EclipseWalkerGame::Update(const GameTimer& gt)
{
    mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % 3;
    mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();

    if (mCurrFrameResource->Fence != 0 && mFence->GetCompletedValue() < mCurrFrameResource->Fence)
    {
        HANDLE eventHandle = CreateEventEx(nullptr, nullptr, false, EVENT_ALL_ACCESS);
        ThrowIfFailed(mFence->SetEventOnCompletion(mCurrFrameResource->Fence, eventHandle));
        WaitForSingleObject(eventHandle, INFINITE);
        CloseHandle(eventHandle);
    }

    OnKeyboardInput(gt);

    // [占쏙옙 占쏙옙占쏙옙占쏙옙트 호占쏙옙]
    if (mCurrentScene) mCurrentScene->Update(gt);

    auto stScene = dynamic_cast<Stage1Scene*>(mCurrentScene.get());
    if (mPlayer && stScene)
        mPlayer->Update(gt, stScene->GetActiveMapSystem());

    XMFLOAT3 camPos = mCamera.GetPosition3f();
    mCamera.UpdateViewMatrix();

    for (auto& obj : mGameObjects)
    {
        if (obj->mIsBillboard)
        {
            XMFLOAT3 firePos = obj->GetPosition();
            float dx = camPos.x - firePos.x;
            float dz = camPos.z - firePos.z;
            obj->SetRotation(0.0f, atan2(dx, dz), 0.0f);
        }
        obj->Update();
        obj->UpdateAnimation(gt.DeltaTime());
        if (obj->mLightIndex != -1)
        {
            float flickerSpeed = 3.0f;
            float baseFlicker = 0.8f + 0.2f * sinf(gt.TotalTime() * flickerSpeed);
            float noise = (float)(rand() % 100) / 2000.0f;
            float intensity = baseFlicker + noise;
            mGameLights[obj->mLightIndex].SetStrength({ 2.0f * intensity, 0.2f * intensity, 0.05f * intensity });
        }
    }

    if (!mAllRitems.empty()) {
        auto& lastItem = mAllRitems.back();
        if (lastItem->Geo && lastItem->Geo->Name == "boxGeo" && lastItem->Mat && lastItem->Mat->Name == "Mat_0") {
            DirectX::XMStoreFloat4x4(&lastItem->World, XMMatrixScaling(5000.0f, 5000.0f, 5000.0f));
            lastItem->NumFramesDirty = gNumFrameResources;
        }
    }

    UpdateMainPassCB(gt);
    UpdateShadowPassCB(gt);


    if (mUIManager && mPlayer) 
    {
        float curHp = mPlayer->GetHP();
        float maxHp = mPlayer->GetMaxHP();
        float curMp = mPlayer->GetMP();
        float maxMp = mPlayer->GetMaxMP();

        mUIManager->Update(curHp, maxHp, curMp, maxMp);
        mUIManager->UpdateEffect(gt.DeltaTime());
    }

    UpdateObjectCBs(gt);
    UpdateMaterialCBs(gt);
    UpdateUIPassCB(gt);

    NetworkManager::Get()->ProcessPackets();

    UpdateRemotePlayers();
}

static const float ClearColor[4] = { 0.690196097f, 0.768627465f, 0.870588243f, 1.0f };

void EclipseWalkerGame::Draw(const GameTimer& gt)
{
    auto cmdListAlloc = mCurrFrameResource->CmdListAlloc;
    ThrowIfFailed(cmdListAlloc->Reset());
    ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), nullptr));

    auto shadowMap = mRenderer->GetShadowMap();

    // [Pass 1] Shadow
    auto barrierShadowWrite = CD3DX12_RESOURCE_BARRIER::Transition(shadowMap->Resource(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE);
    mCommandList->ResourceBarrier(1, &barrierShadowWrite);

    D3D12_VIEWPORT shadowViewport = shadowMap->Viewport(); D3D12_RECT shadowScissorRect = shadowMap->ScissorRect();
    mCommandList->RSSetViewports(1, &shadowViewport); mCommandList->RSSetScissorRects(1, &shadowScissorRect);

    D3D12_CPU_DESCRIPTOR_HANDLE shadowDsv = shadowMap->Dsv();
    mCommandList->OMSetRenderTargets(0, nullptr, false, &shadowDsv);
    mCommandList->ClearDepthStencilView(shadowDsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    mRenderer->DrawScene(mCommandList.Get(), mGameObjects, mCurrFrameResource->PassCB->Resource(), mResources->GetSrvHeap(), mCurrFrameResource->ObjectCB->Resource(), mCurrFrameResource->MaterialCB->Resource(), mRenderer->GetShadowPSO(), 1);

    auto barrierShadowRead = CD3DX12_RESOURCE_BARRIER::Transition(shadowMap->Resource(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_GENERIC_READ);
    mCommandList->ResourceBarrier(1, &barrierShadowRead);

    // [Pass 2] Main
    mCommandList->RSSetViewports(1, &mScreenViewport); mCommandList->RSSetScissorRects(1, &mScissorRect);

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle, dsvHandle(DepthStencilView());
    if (m4xMsaaState) rtvHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(mRtvHeap->GetCPUDescriptorHandleForHeapStart()).Offset(2, mRtvDescriptorSize);
    else {
        auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
        mCommandList->ResourceBarrier(1, &barrier);
        rtvHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(CurrentBackBufferView());
    }

    mCommandList->ClearRenderTargetView(rtvHandle, ClearColor, 0, nullptr);
    mCommandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
    mCommandList->OMSetRenderTargets(1, &rtvHandle, true, &dsvHandle);

    // [占쏙옙 占쏙옙占쏙옙占쏙옙 호占쏙옙]
    if (mCurrentScene) mCurrentScene->Draw(gt);

    auto stScene = dynamic_cast<Stage1Scene*>(mCurrentScene.get());
    int skyIdx = mResources->GetTextureIndex("sky");

    if (skyIdx != -1)
    {
        mRenderer->DrawSkybox(mCommandList.Get(), mAllRitems, mResources->GetSrvHeap(), skyIdx, mCurrFrameResource->ObjectCB->Resource(), mCurrFrameResource->PassCB->Resource());
    }
    mRenderer->DrawScene(mCommandList.Get(), mGameObjects, mCurrFrameResource->PassCB->Resource(), mResources->GetSrvHeap(), mCurrFrameResource->ObjectCB->Resource(), mCurrFrameResource->MaterialCB->Resource(), mRenderer->GetPSO(), 0);
    mRenderer->DrawScene(mCommandList.Get(), mGameObjects, mCurrFrameResource->PassCB->Resource(), mResources->GetSrvHeap(), mCurrFrameResource->ObjectCB->Resource(), mCurrFrameResource->MaterialCB->Resource(), mRenderer->GetOutlinePSO(), 0);
    mRenderer->DrawScene(mCommandList.Get(), mGameObjects, mCurrFrameResource->PassCB->Resource(), mResources->GetSrvHeap(), mCurrFrameResource->ObjectCB->Resource(), mCurrFrameResource->MaterialCB->Resource(), mRenderer->GetTransparentPSO(), 0);

    bool isStage1 = dynamic_cast<Stage1Scene*>(mCurrentScene.get()) != nullptr;
    if (mUIManager && (isStage1))
    {
        mCommandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

        std::vector<GameObject*> normalUIObjs;
        std::vector<GameObject*> distObjs;

        bool isFlashActive = false;
        for (auto& obj : mUIManager->GetUIObjects()) {
            if (obj->Ritem->Mat->Name == "UI_FlashMat" && obj->Ritem->Mat->DiffuseAlbedo.w > 0.0f) {
                isFlashActive = true;
                break;
            }
        }

        for (auto& obj : mUIManager->GetUIObjects())
        {
            if (obj->Ritem->Mat->Name == "UI_FlashMat")
            {
                if (isFlashActive) distObjs.push_back(obj.get());
            }
            else if (obj->Ritem->Mat->Name == "UI_ScreenBgMat")
            {
                if (isFlashActive) normalUIObjs.push_back(obj.get());
            }
            else
            {
                if (!isFlashActive) {
                    normalUIObjs.push_back(obj.get());
                }
            }
        }   
        mRenderer->DrawScene(mCommandList.Get(), normalUIObjs, mCurrFrameResource->PassCB->Resource(), mResources->GetSrvHeap(), mCurrFrameResource->ObjectCB->Resource(), mCurrFrameResource->MaterialCB->Resource(), mRenderer->GetTransparentPSO(), 2);
        mRenderer->DrawScene(mCommandList.Get(), distObjs, mCurrFrameResource->PassCB->Resource(), mResources->GetSrvHeap(), mCurrFrameResource->ObjectCB->Resource(), mCurrFrameResource->MaterialCB->Resource(), mRenderer->GetDistortionPSO(), 2);
    }

    if (m4xMsaaState) {
        D3D12_RESOURCE_BARRIER barriers[2] = { CD3DX12_RESOURCE_BARRIER::Transition(mMSAART.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_RESOLVE_SOURCE), CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RESOLVE_DEST) };
        mCommandList->ResourceBarrier(2, barriers);
        mCommandList->ResolveSubresource(CurrentBackBuffer(), 0, mMSAART.Get(), 0, DXGI_FORMAT_R8G8B8A8_UNORM);
        D3D12_RESOURCE_BARRIER restoreBarriers[2] = { CD3DX12_RESOURCE_BARRIER::Transition(mMSAART.Get(), D3D12_RESOURCE_STATE_RESOLVE_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET), CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(), D3D12_RESOURCE_STATE_RESOLVE_DEST, D3D12_RESOURCE_STATE_PRESENT) };
        mCommandList->ResourceBarrier(2, restoreBarriers);
    }
    else {
        auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
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

void EclipseWalkerGame::BuildDescriptorHeaps()
{
    mResources->BuildDescriptorHeaps(md3dDevice.Get());
    ID3D12DescriptorHeap* srvHeap = mResources->GetSrvHeap();
    CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv(srvHeap->GetCPUDescriptorHandleForHeapStart()); hCpuSrv.Offset(1000, mCbvSrvUavDescriptorSize);
    CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv(srvHeap->GetGPUDescriptorHandleForHeapStart()); hGpuSrv.Offset(1000, mCbvSrvUavDescriptorSize);
    CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuDsv(mDsvHeap->GetCPUDescriptorHandleForHeapStart()); hCpuDsv.Offset(1, mDsvDescriptorSize);
    if (auto shadowMap = mRenderer->GetShadowMap()) shadowMap->BuildDescriptors(hCpuSrv, hGpuSrv, hCpuDsv);
    for (auto& e : mResources->mMaterials)
    {
        e.second->NumFramesDirty = gNumFrameResources; 
    }
}

void EclipseWalkerGame::BuildFrameResources()
{
    UINT maxObjCount = 2000;
    UINT maxMatCount = 500;
    UINT passCount = 3;
    for (int i = 0; i < gNumFrameResources; ++i)
        mFrameResources.push_back(std::make_unique<FrameResource>(md3dDevice.Get(), passCount, maxObjCount, maxMatCount));
}

void EclipseWalkerGame::CreateFire(float x, float y, float z, float scale)
{
    int assignedLightIndex = -1;
    if (mCurrentLightIndex < MaxLights) {
        assignedLightIndex = mCurrentLightIndex;
        mGameLights[assignedLightIndex].InitPoint({ x, y + 1.5f, z }, { 1.0f, 0.2f, 0.05f }, 10.0f);
        mCurrentLightIndex++;
    }
    int numParticles = 6;

    for (int i = 0; i < numParticles; ++i)
    {
        // 4장 중 하나만 랜덤으로 딱 고릅니다!
        int startFrame = rand() % 4;

        auto fire = std::make_unique<RenderItem>();

        // =========================================================
        // ★ [핵심 수정] C++에서 태어날 때부터 텍스처 조각 위치를 직접 잘라줍니다!
        // =========================================================
        float uOffset = (startFrame % 2) * 0.5f;
        float vOffset = (startFrame / 2) * 0.5f;

        // 크기는 절반(0.5)으로 줄이고, 계산한 위치(Offset)로 텍스처 UV를 이동시킵니다.
        DirectX::XMMATRIX texScale = DirectX::XMMatrixScaling(0.5f, 0.5f, 1.0f);
        DirectX::XMMATRIX texOffset = DirectX::XMMatrixTranslation(uOffset, vOffset, 0.0f);
        DirectX::XMStoreFloat4x4(&fire->TexTransform, DirectX::XMMatrixMultiply(texScale, texOffset));
        // =========================================================

        fire->Geo = mResources->mGeometries["quadGeo"].get();
        fire->Mat = mResources->GetMaterial("Fire_Mat");
        fire->ObjCBIndex = mAllRitems.size();
        fire->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

        if (fire->Geo && fire->Geo->DrawArgs.count("quad")) {
            fire->IndexCount = fire->Geo->DrawArgs["quad"].IndexCount;
            fire->StartIndexLocation = fire->Geo->DrawArgs["quad"].StartIndexLocation;
            fire->BaseVertexLocation = fire->Geo->DrawArgs["quad"].BaseVertexLocation;
        }

        auto obj = std::make_unique<GameObject>();
        obj->Ritem = fire.get();

        float randomOffsetX = ((static_cast<float>(rand()) / RAND_MAX) - 0.5f) * 0.15f;
        float randomOffsetZ = ((static_cast<float>(rand()) / RAND_MAX) - 0.5f) * 0.15f;

        obj->SetPosition(x + randomOffsetX, y, z + randomOffsetZ);

        obj->mIsAnimated = true;
        obj->mCurrFrame = startFrame;
        obj->mFrameDuration = 9999.0f; 
        obj->mNumCols = 2;
        obj->mNumRows = 2;
        obj->mIsBillboard = true;

        // 파티클 팽창 애니메이션 설정
        obj->mIsParticle = true;
        obj->mLifeTime = 0.6f;
        obj->mBaseScale = scale;
        obj->mBasePosY = y - 0.1f;

        obj->mAge = (static_cast<float>(rand()) / RAND_MAX) * 0.6f;

        if (i == 0) obj->mLightIndex = assignedLightIndex;
        else obj->mLightIndex = -1;

        obj->Update();
        mAllRitems.push_back(std::move(fire));
        mGameObjects.push_back(std::move(obj));
    }
}

void EclipseWalkerGame::BuildPlayer()
{
    auto playerRitem = std::make_unique<RenderItem>();
    playerRitem->World = MathHelper::Identity4x4(); playerRitem->TexTransform = MathHelper::Identity4x4();
    playerRitem->ObjCBIndex = mAllRitems.size(); playerRitem->Mat = mResources->GetMaterial("PlayerBlue");
    playerRitem->Geo = mResources->mGeometries["boxGeo"].get(); playerRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    auto& boxDrawArgs = playerRitem->Geo->DrawArgs["box"];
    playerRitem->IndexCount = boxDrawArgs.IndexCount; playerRitem->StartIndexLocation = boxDrawArgs.StartIndexLocation; playerRitem->BaseVertexLocation = boxDrawArgs.BaseVertexLocation;

    auto playerObj = std::make_unique<GameObject>();
    playerObj->SetScale(0.3f, 0.5f, 0.3f); playerObj->SetPosition(1.0f, 5.0f, 0.0f);
    playerObj->Ritem = playerRitem.get(); playerObj->Update();

    mPlayerObject = playerObj.get();
    mAllRitems.push_back(std::move(playerRitem)); mGameObjects.push_back(std::move(playerObj));
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
            DirectX::XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));
            XMMATRIX texTransform = XMLoadFloat4x4(&e->TexTransform);
            DirectX::XMStoreFloat4x4(&objConstants.TexTransform, XMMatrixTranspose(texTransform));
            objConstants.ColorMultiplier = e->ColorMultiplier;
            currObjectCB->CopyData(e->ObjCBIndex, objConstants);
            e->NumFramesDirty--;
        }
    }
}

void EclipseWalkerGame::UpdateMaterialCBs(const GameTimer& gt)
{
    auto currMaterialCB = mCurrFrameResource->MaterialCB.get();
    for (auto& e : mResources->mMaterials)
    {
        Material* mat = e.second.get();
        if (mat->NumFramesDirty > 0)
        {
            MaterialConstants matConstants;
            matConstants.DiffuseAlbedo = mat->DiffuseAlbedo; matConstants.FresnelR0 = mat->FresnelR0; matConstants.Roughness = mat->Roughness;
            matConstants.OutlineColor = mat->OutlineColor; matConstants.OutlineThickness = mat->OutlineThickness;
            matConstants.IsToon = mat->IsToon; matConstants.IsTransparent = mat->IsTransparent;
            matConstants.DiffuseMapIndex = mResources->GetTextureIndex(mat->DiffuseMapName);
            matConstants.NormalMapIndex = mResources->GetTextureIndex(mat->NormalMapName);
            matConstants.EmissiveMapIndex = mResources->GetTextureIndex(mat->EmissiveMapName);
            matConstants.MetallicMapIndex = mResources->GetTextureIndex(mat->MetallicMapName);
            currMaterialCB->CopyData(mat->MatCBIndex, matConstants);
            mat->NumFramesDirty--;
        }
    }
}

void EclipseWalkerGame::InitLights()
{
    mGameLights.resize(MaxLights);
    mGameLights[0].InitDirectional({ 0.3f, -1.0f, 0.3f }, { 0.8f, 0.8f, 0.8f });
    mCurrentLightIndex = 1;
}
float EclipseWalkerGame::AspectRatio() const { return static_cast<float>(mClientWidth) / mClientHeight; }

void EclipseWalkerGame::UpdateMainPassCB(const GameTimer& gt)
{
    XMMATRIX view = mCamera.GetView(); XMMATRIX proj = mCamera.GetProj(); XMMATRIX viewProj = XMMatrixMultiply(view, proj);
    XMMATRIX invView = MathHelper::Inverse(view); XMMATRIX invProj = MathHelper::Inverse(proj); XMMATRIX invViewProj = MathHelper::Inverse(viewProj);

    PassConstants mMainPassCB;
    DirectX::XMStoreFloat4x4(&mMainPassCB.View, XMMatrixTranspose(view)); DirectX::XMStoreFloat4x4(&mMainPassCB.InvView, XMMatrixTranspose(invView));
    DirectX::XMStoreFloat4x4(&mMainPassCB.Proj, XMMatrixTranspose(proj)); DirectX::XMStoreFloat4x4(&mMainPassCB.InvProj, XMMatrixTranspose(invProj));
    DirectX::XMStoreFloat4x4(&mMainPassCB.ViewProj, XMMatrixTranspose(viewProj)); DirectX::XMStoreFloat4x4(&mMainPassCB.InvViewProj, XMMatrixTranspose(invViewProj));

    Light sunLight = mGameLights[0].GetRawData(); XMVECTOR lightDir = XMLoadFloat3(&sunLight.Direction);

    XMVECTOR targetPos = mCamera.GetPosition();
    XMVECTOR lightPos = targetPos - (100.0f * lightDir);
    XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f); 
    XMMATRIX lightView = XMMatrixLookAtLH(lightPos, targetPos, up);
    XMMATRIX lightProj = XMMatrixOrthographicLH(100.0f, 100.0f, 1.0f, 1000.0f);

    XMMATRIX T(0.5f, 0.0f, 0.0f, 0.0f, 0.0f, -0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.5f, 0.5f, 0.0f, 1.0f);
    XMMATRIX S = lightView * lightProj * T;

    DirectX::XMStoreFloat4x4(&mMainPassCB.ShadowTransform, XMMatrixTranspose(S));
    mMainPassCB.EyePosW = mCamera.GetPosition3f(); mMainPassCB.RenderTargetSize = { (float)mClientWidth, (float)mClientHeight };
    mMainPassCB.InvRenderTargetSize = { 1.0f / mClientWidth, 1.0f / mClientHeight };
    mMainPassCB.NearZ = 1.0f; mMainPassCB.FarZ = 10000.0f; mMainPassCB.TotalTime = gt.TotalTime(); mMainPassCB.DeltaTime = gt.DeltaTime();
    mMainPassCB.AmbientLight = { 0.25f, 0.25f, 0.35f, 1.0f };
    for (int i = 0; i < MaxLights; ++i) { mGameLights[i].Update(gt.DeltaTime()); mMainPassCB.Lights[i] = mGameLights[i].GetRawData(); }

    if (mPlayer) {
        mMainPassCB.DomainCenter = mPlayer->GetPosition();
    }
    else {
        mMainPassCB.DomainCenter = { 0.0f, 0.0f, 0.0f };
    }

    auto stage1 = dynamic_cast<Stage1Scene*>(mCurrentScene.get());
    if (stage1) {
        mMainPassCB.DomainRadius = stage1->GetDomainRadius();
        mMainPassCB.IsDomainActive = stage1->GetIsDomainActive() ? 1 : 0;
    }
    else {
        mMainPassCB.DomainRadius = 0.0f;
        mMainPassCB.IsDomainActive = 0; 
    }

    mCurrFrameResource->PassCB->CopyData(0, mMainPassCB);
}

void EclipseWalkerGame::UpdateShadowPassCB(const GameTimer& gt)
{
    Light sunLight = mGameLights[0].GetRawData(); XMVECTOR lightDir = XMLoadFloat3(&sunLight.Direction);
    XMVECTOR targetPos = mCamera.GetPosition();
    XMVECTOR lightPos = targetPos - (100.0f * lightDir);
    XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    XMMATRIX lightView = XMMatrixLookAtLH(lightPos, targetPos, up);
    XMMATRIX lightProj = XMMatrixOrthographicLH(100.0f, 100.0f, 1.0f, 1000.0f);

    XMMATRIX viewProj = XMMatrixMultiply(lightView, lightProj);

    PassConstants shadowPassCB;
    DirectX::XMStoreFloat4x4(&shadowPassCB.View, XMMatrixTranspose(lightView)); DirectX::XMStoreFloat4x4(&shadowPassCB.Proj, XMMatrixTranspose(lightProj));
    DirectX::XMStoreFloat4x4(&shadowPassCB.ViewProj, XMMatrixTranspose(viewProj));
    shadowPassCB.EyePosW = { 0.0f, 0.0f, 0.0f }; shadowPassCB.RenderTargetSize = { 4096.0f, 4096.0f };
    shadowPassCB.InvRenderTargetSize = { 1.0f / 4096.0f, 1.0f / 4096.0f };

    shadowPassCB.NearZ = 1.0f; shadowPassCB.FarZ = 1000.0f;

    mCurrFrameResource->PassCB->CopyData(1, shadowPassCB);
}

void EclipseWalkerGame::UpdateUIPassCB(const GameTimer& gt)
{
    PassConstants uiPassCB;
    ZeroMemory(&uiPassCB, sizeof(PassConstants)); 

    uiPassCB.AmbientLight = { 1.0f, 1.0f, 1.0f, 1.0f };

    XMMATRIX view = XMMatrixIdentity();
    XMMATRIX proj = XMMatrixIdentity();
    XMMATRIX viewProj = XMMatrixMultiply(view, proj);

    DirectX::XMStoreFloat4x4(&uiPassCB.View, XMMatrixTranspose(view));
    DirectX::XMStoreFloat4x4(&uiPassCB.InvView, XMMatrixTranspose(view));
    DirectX::XMStoreFloat4x4(&uiPassCB.Proj, XMMatrixTranspose(proj));
    DirectX::XMStoreFloat4x4(&uiPassCB.InvProj, XMMatrixTranspose(proj));
    DirectX::XMStoreFloat4x4(&uiPassCB.ViewProj, XMMatrixTranspose(viewProj));
    DirectX::XMStoreFloat4x4(&uiPassCB.InvViewProj, XMMatrixTranspose(viewProj));

    uiPassCB.RenderTargetSize = { (float)mClientWidth, (float)mClientHeight };
    uiPassCB.InvRenderTargetSize = { 1.0f / mClientWidth, 1.0f / mClientHeight };
    uiPassCB.NearZ = 0.0f;
    uiPassCB.FarZ = 1.0f;
    uiPassCB.TotalTime = gt.TotalTime();
    uiPassCB.DeltaTime = gt.DeltaTime();

    mCurrFrameResource->PassCB->CopyData(2, uiPassCB);
}

LRESULT EclipseWalkerGame::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_LBUTTONDOWN: case WM_MBUTTONDOWN: case WM_RBUTTONDOWN: OnMouseDown(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)); return 0;
    case WM_LBUTTONUP: case WM_MBUTTONUP: case WM_RBUTTONUP: OnMouseUp(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)); return 0;
    case WM_MOUSEMOVE: OnMouseMove(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)); return 0;
    }
    return GameFramework::MsgProc(hwnd, msg, wParam, lParam);
}
void EclipseWalkerGame::OnKeyboardInput(const GameTimer& gt)
{
    if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) PostQuitMessage(0);

    static bool isFPressed = false;
    if (GetAsyncKeyState('F') & 0x8000)
    {
        if (!isFPressed)
        {
            bool isStage1 = dynamic_cast<Stage1Scene*>(mCurrentScene.get()) != nullptr;
            //bool isStage2 = dynamic_cast<Stage2Scene*>(mCurrentScene.get()) != nullptr;

            // 占쏙옙占쏙옙占쏙옙占쏙옙 1占싱놂옙 2占쏙옙 占쏙옙占쏙옙 占쏙옙占쏙옙 占쏙옙占쏙옙트 占쌩듸옙
            if (isStage1)
            {
                if (mUIManager)
                {
                    mUIManager->TriggerFlashEffect();
                }
            }
            isFPressed = true;
        }
    }
    else
    {
        isFPressed = false;
    }
}
void EclipseWalkerGame::OnMouseDown(WPARAM btnState, int x, int y) { mLastMousePos.x = x; mLastMousePos.y = y; SetCapture(mhMainWnd); SetFocus(mhMainWnd); }
void EclipseWalkerGame::OnMouseUp(WPARAM btnState, int x, int y) { ReleaseCapture(); }
void EclipseWalkerGame::OnMouseMove(WPARAM btnState, int x, int y) {
    if ((btnState & MK_RBUTTON) != 0) {
        float dx = XMConvertToRadians(0.25f * static_cast<float>(x - mLastMousePos.x)); float dy = XMConvertToRadians(0.25f * static_cast<float>(y - mLastMousePos.y));
        if (mPlayer) mPlayer->OnMouseMove(dx, dy);
    }
    mLastMousePos.x = x; mLastMousePos.y = y;
}

void EclipseWalkerGame::UpdateRemotePlayers()
{
    auto& remoteDataMap = NetworkManager::Get()->m_remotePlayers;

    for (auto& pair : remoteDataMap)
    {
        int playerId = pair.first;
        PKT_S_PLAYER_MOVE& data = pair.second;

        if (mRemotePlayerObjects.find(playerId) == mRemotePlayerObjects.end())
        {
            OutputDebugStringA("[Client] 새 원격 플레이어 등장! 3D 오브젝트 생성\n");

            auto ritem = std::make_unique<RenderItem>();
            ritem->TexTransform = MathHelper::Identity4x4();
            ritem->Mat = mResources->GetMaterial("PlayerBlue");
            ritem->Geo = mResources->mGeometries["boxGeo"].get();
            ritem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
            ritem->IndexCount = ritem->Geo->DrawArgs["box"].IndexCount;
            ritem->StartIndexLocation = ritem->Geo->DrawArgs["box"].StartIndexLocation;
            ritem->BaseVertexLocation = ritem->Geo->DrawArgs["box"].BaseVertexLocation;
            ritem->ObjCBIndex = (UINT)mAllRitems.size();
            ritem->NumFramesDirty = 3;

            auto newPlayerObj = std::make_unique<GameObject>();
            newPlayerObj->Ritem = ritem.get();
            newPlayerObj->SetPosition(data.x, data.y, data.z);
            newPlayerObj->SetScale(0.3f, 0.5f, 0.3f);
            newPlayerObj->Update(); // ← 추가: 생성 시 월드 행렬 초기화

            mRemotePlayerObjects[playerId] = newPlayerObj.get();
            mAllRitems.push_back(std::move(ritem));
            mGameObjects.push_back(std::move(newPlayerObj));

            BuildDescriptorHeaps();
        }
        else
        {
            GameObject* targetObj = mRemotePlayerObjects[playerId];
            targetObj->SetPosition(data.x, data.y, data.z);
            targetObj->SetRotation(0.0f, data.rotY, 0.0f);
            targetObj->Update(); // ← 핵심: 이게 없어서 화면에 안 보였던 것
        }
    }
}