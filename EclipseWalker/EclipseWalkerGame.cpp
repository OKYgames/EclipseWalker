#include "EclipseWalkerGame.h"
#include "Stage1Scene.h"         // [추가] 이제 분리된 헤더를 참조합니다.
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

    // DSV 힙 설정 
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

    // 시스템 초기화
    mResources = std::make_unique<ResourceManager>(md3dDevice.Get(), mCommandList.Get());
    mRenderer = std::make_unique<Renderer>(md3dDevice.Get());

    CD3DX12_CPU_DESCRIPTOR_HANDLE shadowHandle(mDsvHeap->GetCPUDescriptorHandleForHeapStart());
    UINT dsvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    shadowHandle.Offset(1, dsvDescriptorSize);
    mRenderer->Initialize(shadowHandle);

    // --- [1단계] 코어 세팅 ---
    InitLights();
    BuildFrameResources();
    LoadCoreResources();

    // --- [씬 전환 (Stage 1 진입)] ---
    ChangeScene(std::make_unique<Stage1Scene>(this));

    // --- 힙 빌드는 씬 로딩 후 마지막에 1번만 ---
    BuildDescriptorHeaps();

    // 초기화 명령 실행 및 동기화
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

    // 카메라 설정
    mCamera.SetPosition(0.0f, 2.0f, -5.0f);
    mCamera.SetLens(0.25f * 3.14f, AspectRatio(), 1.0f, 1000.0f);
    mCamera.Pitch(XMConvertToRadians(15.0f));

    return true;
}

void EclipseWalkerGame::ChangeScene(std::unique_ptr<Scene> newScene)
{
    if (mCurrentScene) mCurrentScene->Exit();
    mCurrentScene = std::move(newScene);
    mCurrentScene->Enter();
}

void EclipseWalkerGame::LoadCoreResources()
{
    // (폰트, 마우스 커서 등 항상 메모리에 상주할 것들을 추후 이곳에 로드)
    OutputDebugStringA("\n[Core] 시스템 폰트 및 UI 로드 완료.\n");
}

void EclipseWalkerGame::LoadSharedGameResources()
{
    if (mIsSharedResourcesLoaded) return;
    OutputDebugStringA("\n[Shared] 인게임 공용 리소스 (플레이어, 불꽃) 로딩 시작...\n");

    // 1. 공용 텍스처 로드
    mResources->LoadTexture("Fire_1", L"Models/Map/Textures/Fire_1.dds");
    mResources->LoadTexture("Blue", L"Textures/Blue.dds");

    // 2. 공용 지오메트리 로드 (Quad, Box)
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
    D3DCreateBlob(quadVbSize, &quadGeo->VertexBufferCPU);
    CopyMemory(quadGeo->VertexBufferCPU->GetBufferPointer(), quadVertices.data(), quadVbSize);
    D3DCreateBlob(quadIbSize, &quadGeo->IndexBufferCPU);
    CopyMemory(quadGeo->IndexBufferCPU->GetBufferPointer(), quadIndices.data(), quadIbSize);

    quadGeo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(), mCommandList.Get(), quadVertices.data(), quadVbSize, quadGeo->VertexBufferUploader);
    quadGeo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(), mCommandList.Get(), quadIndices.data(), quadIbSize, quadGeo->IndexBufferUploader);
    quadGeo->VertexByteStride = sizeof(Vertex);
    quadGeo->VertexBufferByteSize = quadVbSize;
    quadGeo->IndexFormat = DXGI_FORMAT_R16_UINT;
    quadGeo->IndexBufferByteSize = quadIbSize;

    SubmeshGeometry quadSubmesh;
    quadSubmesh.IndexCount = (UINT)quadIndices.size(); quadSubmesh.StartIndexLocation = 0; quadSubmesh.BaseVertexLocation = 0;
    quadGeo->DrawArgs["quad"] = quadSubmesh;
    mResources->mGeometries[quadGeo->Name] = std::move(quadGeo);

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

    // 3. 공용 재질 생성 (Fire, Player)
    mResources->CreateMaterial("Fire_Mat", mResources->mMaterials.size(), "Fire_1", "", "", "", XMFLOAT4(1.0f, 0.3f, 0.1f, 0.8f), XMFLOAT3(0.1f, 0.1f, 0.1f), 0.1f);
    if (auto mat = mResources->GetMaterial("Fire_Mat")) { mat->IsTransparent = 1; mat->NumFramesDirty = 3; }

    mResources->CreateMaterial("PlayerBlue", mResources->mMaterials.size(), "Blue", "", "", "", XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f), XMFLOAT3(0.04f, 0.04f, 0.04f), 0.8f);
    if (auto mat = mResources->GetMaterial("PlayerBlue")) mat->NumFramesDirty = 3;

    // 4. 오브젝트 조립
    CreateFire(-0.1f, 0.8f, 1.1f, 0.3f);
    CreateFire(4.1f, 0.8f, 1.1f, 0.3f);
    BuildPlayer();

    // 5. 플레이어 로직 초기화
    if (!mPlayer) mPlayer = std::make_unique<Player>();
    mPlayer->Initialize(mPlayerObject, &mCamera);

    mIsSharedResourcesLoaded = true;
}

void EclipseWalkerGame::UnloadSharedGameResources()
{
    mIsSharedResourcesLoaded = false;
}

void EclipseWalkerGame::OnResize()
{
    GameFramework::OnResize();
    mCamera.SetLens(0.25f * 3.14f, AspectRatio(), 1.0f, 1000.0f);
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

    // [씬 업데이트 호출]
    if (mCurrentScene) mCurrentScene->Update(gt);

    auto stScene = dynamic_cast<Stage1Scene*>(mCurrentScene.get());
    if (mPlayer && stScene) mPlayer->Update(gt, stScene->mMapSystem.get());

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
            mGameLights[obj->mLightIndex].SetStrength({ 1.0f * intensity, 0.2f * intensity, 0.05f * intensity });
        }
    }

    if (!mAllRitems.empty()) {
        auto& skyRitem = mAllRitems.back();
        DirectX::XMStoreFloat4x4(&skyRitem->World, XMMatrixScaling(5000.0f, 5000.0f, 5000.0f));
        skyRitem->NumFramesDirty = gNumFrameResources;
    }

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

    // [씬 렌더링 호출]
    if (mCurrentScene) mCurrentScene->Draw(gt);

    auto stScene = dynamic_cast<Stage1Scene*>(mCurrentScene.get());
    int skyIdx = mResources->GetTextureIndex("sky");

    mRenderer->DrawSkybox(mCommandList.Get(), mAllRitems, mResources->GetSrvHeap(), skyIdx, mCurrFrameResource->ObjectCB->Resource(), mCurrFrameResource->PassCB->Resource());
    mRenderer->DrawScene(mCommandList.Get(), mGameObjects, mCurrFrameResource->PassCB->Resource(), mResources->GetSrvHeap(), mCurrFrameResource->ObjectCB->Resource(), mCurrFrameResource->MaterialCB->Resource(), mRenderer->GetPSO(), 0);
    mRenderer->DrawScene(mCommandList.Get(), mGameObjects, mCurrFrameResource->PassCB->Resource(), mResources->GetSrvHeap(), mCurrFrameResource->ObjectCB->Resource(), mCurrFrameResource->MaterialCB->Resource(), mRenderer->GetOutlinePSO(), 0);
    mRenderer->DrawScene(mCommandList.Get(), mGameObjects, mCurrFrameResource->PassCB->Resource(), mResources->GetSrvHeap(), mCurrFrameResource->ObjectCB->Resource(), mCurrFrameResource->MaterialCB->Resource(), mRenderer->GetTransparentPSO(), 0);

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
}

void EclipseWalkerGame::BuildFrameResources()
{
    UINT maxObjCount = 2000;
    UINT maxMatCount = 500;
    UINT passCount = 2;
    for (int i = 0; i < gNumFrameResources; ++i)
        mFrameResources.push_back(std::make_unique<FrameResource>(md3dDevice.Get(), passCount, maxObjCount, maxMatCount));
}

void EclipseWalkerGame::CreateFire(float x, float y, float z, float scale)
{
    int startFrame = rand() % 4;
    float randomSpeed = 0.05f + (static_cast<float>(rand()) / RAND_MAX) * 0.05f;
    auto fire = std::make_unique<RenderItem>();
    DirectX::XMStoreFloat4x4(&fire->TexTransform, XMMatrixScaling(0.5f, 0.5f, 1.0f));
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
    obj->Ritem = fire.get(); obj->SetPosition(x, y, z); obj->SetScale(scale, scale, scale);
    obj->mIsAnimated = true; obj->mCurrFrame = startFrame; obj->mFrameDuration = randomSpeed;
    obj->mNumCols = 2; obj->mNumRows = 2; obj->mIsBillboard = true;

    if (mCurrentLightIndex < MaxLights) {
        mGameLights[mCurrentLightIndex].InitPoint({ 1.0f, 0.2f, 0.05f }, { x, y , z }, 10.0f);
        obj->mLightIndex = mCurrentLightIndex; mCurrentLightIndex++;
    }
    obj->Update();
    mAllRitems.push_back(std::move(fire)); mGameObjects.push_back(std::move(obj));
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
    playerObj->SetScale(0.3f, 0.5f, 0.3f); playerObj->SetPosition(1.0f, 10.0f, 0.0f);
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

void EclipseWalkerGame::InitLights() { mGameLights.resize(MaxLights); mGameLights[0].InitDirectional({ 0.57735f, -0.57735f, 0.57735f }, { 0.8f, 0.8f, 0.8f }); }
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
    XMVECTOR targetPos = mCamera.GetPosition(); XMVECTOR lightPos = targetPos - (100.0f * lightDir); XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    XMMATRIX lightView = XMMatrixLookAtLH(lightPos, targetPos, up); XMMATRIX lightProj = XMMatrixOrthographicLH(100.0f, 100.0f, 1.0f, 1000.0f);
    XMMATRIX T(0.5f, 0.0f, 0.0f, 0.0f, 0.0f, -0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.5f, 0.5f, 0.0f, 1.0f);
    XMMATRIX S = lightView * lightProj * T;

    DirectX::XMStoreFloat4x4(&mMainPassCB.ShadowTransform, XMMatrixTranspose(S));
    mMainPassCB.EyePosW = mCamera.GetPosition3f(); mMainPassCB.RenderTargetSize = { (float)mClientWidth, (float)mClientHeight };
    mMainPassCB.InvRenderTargetSize = { 1.0f / mClientWidth, 1.0f / mClientHeight };
    mMainPassCB.NearZ = 1.0f; mMainPassCB.FarZ = 10000.0f; mMainPassCB.TotalTime = gt.TotalTime(); mMainPassCB.DeltaTime = gt.DeltaTime();
    mMainPassCB.AmbientLight = { 0.25f, 0.25f, 0.35f, 1.0f };
    for (int i = 0; i < MaxLights; ++i) { mGameLights[i].Update(gt.DeltaTime()); mMainPassCB.Lights[i] = mGameLights[i].GetRawData(); }
    mCurrFrameResource->PassCB->CopyData(0, mMainPassCB);
}

void EclipseWalkerGame::UpdateShadowPassCB(const GameTimer& gt)
{
    Light sunLight = mGameLights[0].GetRawData(); XMVECTOR lightDir = XMLoadFloat3(&sunLight.Direction);
    XMVECTOR targetPos = mCamera.GetPosition(); XMVECTOR lightPos = targetPos - (100.0f * lightDir); XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    XMMATRIX lightView = XMMatrixLookAtLH(lightPos, targetPos, up); XMMATRIX lightProj = XMMatrixOrthographicLH(100.0f, 100.0f, 1.0f, 1000.0f);
    XMMATRIX viewProj = XMMatrixMultiply(lightView, lightProj);

    PassConstants shadowPassCB;
    DirectX::XMStoreFloat4x4(&shadowPassCB.View, XMMatrixTranspose(lightView)); DirectX::XMStoreFloat4x4(&shadowPassCB.Proj, XMMatrixTranspose(lightProj));
    DirectX::XMStoreFloat4x4(&shadowPassCB.ViewProj, XMMatrixTranspose(viewProj));
    shadowPassCB.EyePosW = { 0.0f, 0.0f, 0.0f }; shadowPassCB.RenderTargetSize = { 4096.0f, 4096.0f };
    shadowPassCB.InvRenderTargetSize = { 1.0f / 4096.0f, 1.0f / 4096.0f }; shadowPassCB.NearZ = 1.0f; shadowPassCB.FarZ = 100.0f;
    mCurrFrameResource->PassCB->CopyData(1, shadowPassCB);
}

LRESULT EclipseWalkerGame::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_LBUTTONDOWN: case WM_MBUTTONDOWN: case WM_RBUTTONDOWN: OnMouseDown(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)); return 0;
    case WM_LBUTTONUP: case WM_MBUTTONUP: case WM_RBUTTONUP: OnMouseUp(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)); return 0;
    case WM_MOUSEMOVE: OnMouseMove(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)); return 0;
    }
    return GameFramework::MsgProc(hwnd, msg, wParam, lParam);
}
void EclipseWalkerGame::OnKeyboardInput(const GameTimer& gt) { if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) PostQuitMessage(0); }
void EclipseWalkerGame::OnMouseDown(WPARAM btnState, int x, int y) { mLastMousePos.x = x; mLastMousePos.y = y; SetCapture(mhMainWnd); SetFocus(mhMainWnd); }
void EclipseWalkerGame::OnMouseUp(WPARAM btnState, int x, int y) { ReleaseCapture(); }
void EclipseWalkerGame::OnMouseMove(WPARAM btnState, int x, int y) {
    if ((btnState & MK_RBUTTON) != 0) {
        float dx = XMConvertToRadians(0.25f * static_cast<float>(x - mLastMousePos.x)); float dy = XMConvertToRadians(0.25f * static_cast<float>(y - mLastMousePos.y));
        if (mPlayer) mPlayer->OnMouseMove(dx, dy);
    }
    mLastMousePos.x = x; mLastMousePos.y = y;
}