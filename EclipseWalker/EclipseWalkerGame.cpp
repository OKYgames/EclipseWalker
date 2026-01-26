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
    // 1. 부모 클래스 초기화 (여기서 창 만들고, D3D 장치 만들고, 기본 힙(크기1)을 만듦)
    if (!GameFramework::Initialize())
        return false;

    // 명령 할당자 리셋
    ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

    // DSV 힙 확장 (1개 -> 2개) 및 재설정
    {
        D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
        dsvHeapDesc.NumDescriptors = 2; // [0: 화면용], [1: 그림자용]
        dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        dsvHeapDesc.NodeMask = 0;
        ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&mDsvHeap)));


        D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
        dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
        dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; 
        dsvDesc.Texture2D.MipSlice = 0;

        CD3DX12_CPU_DESCRIPTOR_HANDLE mainDsvHandle(mDsvHeap->GetCPUDescriptorHandleForHeapStart());
        md3dDevice->CreateDepthStencilView(mDepthStencilBuffer.Get(), &dsvDesc, mainDsvHandle);
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


    // 게임 데이터 구축
    BuildShapeGeometry();
    BuildMaterials();
    BuildRenderItems();
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
    UpdateCamera();

    // 플레이어 이동 (GameObject 사용)
    if (mPlayerObject != nullptr)
    {
        mPlayerObject->SetPosition(mTargetPos.x, mTargetPos.y, mTargetPos.z);
        mPlayerObject->SetRotation(0.0f, mCameraTheta + DirectX::XM_PI, 0.0f);
    }

    // 모든 게임 오브젝트 업데이트
    for (auto& e : mGameObjects)
    {
        e->Update();
    }

    // 3. 상수 버퍼(GPU 메모리) 갱신
    UpdateMainPassCB(gt);
    UpdateShadowPassCB(gt);
    UpdateObjectCBs(gt);
}

void EclipseWalkerGame::Draw(const GameTimer& gt)
{
    // 1. 명령 할당자 & 리스트 리셋
    auto cmdListAlloc = mCurrFrameResource->CmdListAlloc;
    ThrowIfFailed(cmdListAlloc->Reset());

    ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), nullptr));

    // 그림자 맵 가져오기
    auto shadowMap = mRenderer->GetShadowMap();

    // =======================================================================
    // [Pass 1] 그림자 맵 그리기 (Shadow Pass) - 조명 시점
    // =======================================================================
    auto barrierShadowWrite = CD3DX12_RESOURCE_BARRIER::Transition(
        shadowMap->Resource(),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        D3D12_RESOURCE_STATE_DEPTH_WRITE);

    mCommandList->ResourceBarrier(1, &barrierShadowWrite);

    D3D12_VIEWPORT shadowViewport = shadowMap->Viewport();
    D3D12_RECT shadowScissorRect = shadowMap->ScissorRect();

    mCommandList->RSSetViewports(1, &shadowViewport);
    mCommandList->RSSetScissorRects(1, &shadowScissorRect);

    D3D12_CPU_DESCRIPTOR_HANDLE shadowDsv = shadowMap->Dsv();
    mCommandList->OMSetRenderTargets(0, nullptr, false, &shadowDsv);

    mCommandList->ClearDepthStencilView(shadowDsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    mRenderer->DrawScene(
        mCommandList.Get(),
        mGameObjects,
        mCurrFrameResource->PassCB->Resource(),
        mSrvDescriptorHeap.Get(),
        mCurrFrameResource->ObjectCB->Resource(),
        mRenderer->GetShadowPSO(),
        1
    );

    auto barrierShadowRead = CD3DX12_RESOURCE_BARRIER::Transition(
        shadowMap->Resource(),
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        D3D12_RESOURCE_STATE_GENERIC_READ);

    mCommandList->ResourceBarrier(1, &barrierShadowRead);

    // =======================================================================
    // [Pass 2] 메인 화면 그리기 (Main Pass) - 플레이어 카메라 시점
    // =======================================================================

    mCommandList->RSSetViewports(1, &mScreenViewport);
    mCommandList->RSSetScissorRects(1, &mScissorRect);

    auto barrierRtv = CD3DX12_RESOURCE_BARRIER::Transition(
        CurrentBackBuffer(),
        D3D12_RESOURCE_STATE_PRESENT,
        D3D12_RESOURCE_STATE_RENDER_TARGET);

    mCommandList->ResourceBarrier(1, &barrierRtv);

    D3D12_CPU_DESCRIPTOR_HANDLE currentRtv = CurrentBackBufferView();
    D3D12_CPU_DESCRIPTOR_HANDLE currentDsv = DepthStencilView();

    mCommandList->ClearRenderTargetView(currentRtv, Colors::LightSteelBlue, 0, nullptr);
    mCommandList->ClearDepthStencilView(currentDsv, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

    mCommandList->OMSetRenderTargets(1, &currentRtv, true, &currentDsv);

    mRenderer->DrawScene(
        mCommandList.Get(),
        mGameObjects,
        mCurrFrameResource->PassCB->Resource(),
        mSrvDescriptorHeap.Get(),
        mCurrFrameResource->ObjectCB->Resource(),
        mRenderer->GetPSO(),
        0
    );

    // =======================================================================
    // 마무리 (Present)
    // =======================================================================
    auto barrierPresent = CD3DX12_RESOURCE_BARRIER::Transition(
        CurrentBackBuffer(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PRESENT);

    mCommandList->ResourceBarrier(1, &barrierPresent);

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
}

void EclipseWalkerGame::BuildMaterials()
{
    std::vector<std::string> texNames = ModelLoader::LoadTextureNames("Models/Map/Map.fbx");

    for (int i = 0; i < texNames.size(); ++i)
    {
        
        auto mat = std::make_unique<Material>();
        mat->Name = "Mat_" + std::to_string(i);
        mat->MatCBIndex = i;
        mat->DiffuseSrvHeapIndex = i;

        mat->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
        mat->FresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05f);
        mat->Roughness = 0.8f;
        mResources->CreateMaterial(mat->Name, mat->MatCBIndex, mat->DiffuseAlbedo, mat->FresnelR0, mat->Roughness);
        Material* storedMat = mResources->GetMaterial(mat->Name);
        if (storedMat != nullptr)
        {
            storedMat->DiffuseSrvHeapIndex = i; 
        }
    }
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
        if (mat == nullptr) mat = mResources->GetMaterial("stone");

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
}

void EclipseWalkerGame::LoadTextures()
{
    std::string modelPath = "Models/Map/Map.fbx";
    std::vector<std::string> texNames = ModelLoader::LoadTextureNames(modelPath);
    if (texNames.empty()) return;

    // 1. 힙 생성 (크기는 넉넉하게)
    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
    srvHeapDesc.NumDescriptors = 512;
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mSrvDescriptorHeap)));

    UINT descriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

    // -----------------------------------------------------------------------
    // 재질 1개당 4칸씩(Diffuse, Normal, Emiss, Metal) 연속으로 저장
    // -----------------------------------------------------------------------
    for (int i = 0; i < texNames.size(); ++i)
    {
        std::string originName = texNames[i];
        if (originName.empty()) { hDescriptor.Offset(4, descriptorSize); continue; }

        std::string baseName = originName.substr(0, originName.find_last_of('.'));

        // ===================================================
        // [Slot 0] Diffuse (색상) 
        // ===================================================
        std::wstring path = L"Models/Map/Textures/" + std::wstring(baseName.begin(), baseName.end()) + L".dds";

        // 1. 파일이 없으면 '_albedo'를 붙여서 다시 찾아봅니다.
        if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES)
        {
            std::string albedoName = baseName + "_albedo";
            path = L"Models/Map/Textures/" + std::wstring(albedoName.begin(), albedoName.end()) + L".dds";

            // 이름을 _albedo 붙은 걸로 업데이트 
            if (GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES)
                baseName = albedoName;
        }

        // 2. 로딩 시도 (있으면 로드, 없으면 CreateSRV가 검은색 처리)
        if (GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES)
            mResources->LoadTexture(baseName, path);

        auto tex = mResources->GetTexture(baseName);
        CreateSRV(tex, hDescriptor); // 헬퍼 함수 사용
        hDescriptor.Offset(1, descriptorSize);

        // ===================================================
        // [Slot 1] Normal (노말)
        // ===================================================
        std::string normalName = baseName;
        // _albedo가 붙어있다면 제거하고 _normal로 교체, 아니면 그냥 붙임
        if (baseName.find("_albedo") != std::string::npos)
            normalName.replace(baseName.find("_albedo"), 7, "_normal");
        else
            normalName += "_normal";

        path = L"Models/Map/Textures/" + std::wstring(normalName.begin(), normalName.end()) + L".dds";
        if (GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES) mResources->LoadTexture(normalName, path);

        tex = mResources->GetTexture(normalName);
        if (!tex) tex = mResources->GetTexture("Stones_normal"); // 기본값
        CreateSRV(tex, hDescriptor);
        hDescriptor.Offset(1, descriptorSize);

        // ===================================================
        // [Slot 2] Emissive (발광)
        // ===================================================
        std::string emissName = baseName;
        if (baseName.find("_albedo") != std::string::npos) emissName.replace(baseName.find("_albedo"), 7, "_emissive");
        else emissName += "_emissive";

        path = L"Models/Map/Textures/" + std::wstring(emissName.begin(), emissName.end()) + L".dds";
        if (GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES) mResources->LoadTexture(emissName, path);

        tex = mResources->GetTexture(emissName);
        CreateSRV(tex, hDescriptor);
        hDescriptor.Offset(1, descriptorSize);

        // ===================================================
        // [Slot 3] Metallic (금속)
        // ===================================================
        std::string metalName = baseName;
        if (baseName.find("_albedo") != std::string::npos) metalName.replace(baseName.find("_albedo"), 7, "_metallic");
        else metalName += "_metallic";

        path = L"Models/Map/Textures/" + std::wstring(metalName.begin(), metalName.end()) + L".dds";
        if (GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES) mResources->LoadTexture(metalName, path);

        tex = mResources->GetTexture(metalName);
        CreateSRV(tex, hDescriptor);
        hDescriptor.Offset(1, descriptorSize);
    }

    // 3. 그림자 맵 
    CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv(mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
    CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
    hCpuSrv.Offset(200, descriptorSize);
    hGpuSrv.Offset(200, descriptorSize);

    // DSV
    CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuDsv(mDsvHeap->GetCPUDescriptorHandleForHeapStart());
    UINT dsvSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    hCpuDsv.Offset(1, dsvSize);

    if (mRenderer->GetShadowMap())
        mRenderer->GetShadowMap()->BuildDescriptors(hCpuSrv, hGpuSrv, hCpuDsv);
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
    for (int i = 0; i < 3; ++i)
    {
        UINT objCount = (UINT)mAllRitems.size();
        if (objCount == 0) objCount = 1;
        mFrameResources.push_back(std::make_unique<FrameResource>(md3dDevice.Get(), 2, objCount));
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
    float dt = gt.DeltaTime();
    float speed = 5.0f * dt; 

    // 1. 카메라가 보고 있는 방향(Forward) 계산
    XMFLOAT3 camPos = mCamera.GetPosition3f();
    float dx = mTargetPos.x - camPos.x;
    float dz = mTargetPos.z - camPos.z;

    XMVECTOR forwardVec = XMVectorSet(dx, 0.0f, dz, 0.0f);

    // 방향 벡터 정규화 
    forwardVec = XMVector3Normalize(forwardVec);

    XMVECTOR upVec = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    XMVECTOR rightVec = XMVector3Cross(upVec, forwardVec); 

    XMFLOAT3 forward, right;
    XMStoreFloat3(&forward, forwardVec);
    XMStoreFloat3(&right, rightVec);

    // 3. 키 입력에 따른 이동
    if (GetAsyncKeyState('W') & 0x8000)
    {
        mTargetPos.x += forward.x * speed;
        mTargetPos.z += forward.z * speed;
    }
    if (GetAsyncKeyState('S') & 0x8000)
    {
        mTargetPos.x -= forward.x * speed;
        mTargetPos.z -= forward.z * speed;
    }
    if (GetAsyncKeyState('D') & 0x8000)
    {
        mTargetPos.x += right.x * speed;
        mTargetPos.z += right.z * speed;
    }
    if (GetAsyncKeyState('A') & 0x8000)
    {
        mTargetPos.x -= right.x * speed;
        mTargetPos.z -= right.z * speed;
    }

    // 카메라 회전 
    if (GetAsyncKeyState(VK_LEFT) & 0x8000) mCameraTheta -= 2.0f * dt;
    if (GetAsyncKeyState(VK_RIGHT) & 0x8000) mCameraTheta += 2.0f * dt;
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
    if ((btnState & MK_RBUTTON) != 0)
    {
        float dx = XMConvertToRadians(0.25f * static_cast<float>(x - mLastMousePos.x));
        float dy = XMConvertToRadians(0.25f * static_cast<float>(y - mLastMousePos.y));
        mCameraTheta += dx;
        mCameraPhi += dy;
        mCameraPhi = std::clamp(mCameraPhi, 0.5f, 2.4f);
    }
    mLastMousePos.x = x;
    mLastMousePos.y = y;
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

void EclipseWalkerGame::UpdateCamera()
{
    mCameraRadius = 6.0f;
    float x = mCameraRadius * sinf(mCameraPhi) * cosf(mCameraTheta);
    float z = mCameraRadius * sinf(mCameraPhi) * sinf(mCameraTheta);
    float y = mCameraRadius * cosf(mCameraPhi);
    XMVECTOR target = XMLoadFloat3(&mTargetPos);
    XMVECTOR pos = XMVectorSet(x, y, z, 0.0f);
    XMVECTOR finalPos = target + pos;
    mCamera.LookAt(finalPos, target, XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
    mCamera.UpdateViewMatrix();
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
    mMainPassCB.FarZ = 1000.0f;
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
    shadowPassCB.RenderTargetSize = { 2048.0f, 2048.0f };
    shadowPassCB.InvRenderTargetSize = { 1.0f / 2048.0f, 1.0f / 2048.0f };
    shadowPassCB.NearZ = 1.0f;
    shadowPassCB.FarZ = 100.0f;

    mCurrFrameResource->PassCB->CopyData(1, shadowPassCB);
}