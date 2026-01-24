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
    if (!GameFramework::Initialize())
        return false;

    // 명령 할당자 리셋
    ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

    // -----------------------------------------------------------
    // 1. 시스템 초기화 (매니저 & 렌더러 생성)
    // -----------------------------------------------------------
    mResources = std::make_unique<ResourceManager>(md3dDevice.Get(), mCommandList.Get());
    mRenderer = std::make_unique<Renderer>(md3dDevice.Get());

    // -----------------------------------------------------------
    // 2. 리소스 로드 및 설정
    // -----------------------------------------------------------
    InitLights();
    LoadTextures(); 

    // 렌더러 초기화 
    mRenderer->Initialize();

    // 게임 데이터 구축
    BuildShapeGeometry();
    BuildMaterials();
    BuildRenderItems();
    BuildFrameResources();

    // -----------------------------------------------------------
    // 3. 초기화 명령 실행
    // -----------------------------------------------------------
    ThrowIfFailed(mCommandList->Close());
    ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    // GPU 동기화 (초기화 끝날 때까지 대기)
    mCurrentFence++;
    mCommandQueue->Signal(mFence.Get(), mCurrentFence);
    if (mFence->GetCompletedValue() < mCurrentFence)
    {
        HANDLE eventHandle = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        mFence->SetEventOnCompletion(mCurrentFence, eventHandle);
        WaitForSingleObject(eventHandle, INFINITE);
        CloseHandle(eventHandle);
    }

    // 4. 카메라 초기 위치 설정
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
    UpdateObjectCBs(gt);
}

void EclipseWalkerGame::Draw(const GameTimer& gt)
{
    auto cmdListAlloc = mCurrFrameResource->CmdListAlloc;
    ThrowIfFailed(cmdListAlloc->Reset());

    // PSO는 Renderer가 관리하므로 여기선 nullptr로 리셋
    ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), nullptr));

    // 1. 뷰포트 & 화면 지우기 (Clear)
    mCommandList->RSSetViewports(1, &mScreenViewport);
    mCommandList->RSSetScissorRects(1, &mScissorRect);

    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    mCommandList->ResourceBarrier(1, &barrier);

    // ★★★ [수정 포인트] 리턴값을 변수(l-value)에 먼저 저장해야 주소(&)를 가져올 수 있습니다!
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
        mCurrFrameResource->ObjectCB->Resource()
    );

    // 2. 화면 전송 준비 (Present)
    auto barrier2 = CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    mCommandList->ResourceBarrier(1, &barrier2);

    ThrowIfFailed(mCommandList->Close());

    // 3. 실행 및 교체
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
    // 1. 텍스처 이름 목록 추출
    std::string modelPath = "Models/Map/Map.fbx";
    std::vector<std::string> texNames = ModelLoader::LoadTextureNames(modelPath);
    if (texNames.empty()) return;

    // 2. 매니저에게 로딩 명령
    for (const auto& originName : texNames)
    {
        if (originName.empty()) continue;
        std::string nameNoExt = originName.substr(0, originName.find_last_of('.'));
        std::wstring ddsFilename = L"Models/Map/Textures/" + std::wstring(nameNoExt.begin(), nameNoExt.end()) + L".dds";
        mResources->LoadTexture(nameNoExt, ddsFilename);
    }

    // 3. 힙 생성
    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
    srvHeapDesc.NumDescriptors = (UINT)texNames.size();
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mSrvDescriptorHeap)));

    CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
    UINT descriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    // 4. 뷰 생성
    for (const auto& originName : texNames)
    {
        if (originName.empty()) {
            hDescriptor.Offset(1, descriptorSize);
            continue;
        }
        std::string nameNoExt = originName.substr(0, originName.find_last_of('.'));
        Texture* tex = mResources->GetTexture(nameNoExt);

        if (tex != nullptr) {
            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvDesc.Format = tex->Resource->GetDesc().Format;
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Texture2D.MostDetailedMip = 0;
            srvDesc.Texture2D.MipLevels = tex->Resource->GetDesc().MipLevels;
            srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
            md3dDevice->CreateShaderResourceView(tex->Resource.Get(), &srvDesc, hDescriptor);
        }
        hDescriptor.Offset(1, descriptorSize);
    }
}

// ------------------------------------------------------
// 나머지 입력 처리, 카메라, 상수버퍼 업데이트 등은 기존과 동일
// ------------------------------------------------------

void EclipseWalkerGame::BuildFrameResources()
{
    for (int i = 0; i < 3; ++i)
    {
        UINT objCount = (UINT)mAllRitems.size();
        if (objCount == 0) objCount = 1;
        mFrameResources.push_back(std::make_unique<FrameResource>(md3dDevice.Get(), 1, objCount));
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
    float forwardX = -sinf(mCameraTheta);
    float forwardZ = -cosf(mCameraTheta);
    float rightX = -cosf(mCameraTheta);
    float rightZ = sinf(mCameraTheta);

    if (GetAsyncKeyState('W') & 0x8000) { mTargetPos.x += forwardX * speed; mTargetPos.z += forwardZ * speed; }
    if (GetAsyncKeyState('S') & 0x8000) { mTargetPos.x -= forwardX * speed; mTargetPos.z -= forwardZ * speed; }
    if (GetAsyncKeyState('D') & 0x8000) { mTargetPos.x += rightX * speed; mTargetPos.z += rightZ * speed; }
    if (GetAsyncKeyState('A') & 0x8000) { mTargetPos.x -= rightX * speed; mTargetPos.z -= rightZ * speed; }
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