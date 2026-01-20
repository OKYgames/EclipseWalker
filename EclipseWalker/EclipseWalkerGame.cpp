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

    ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

    InitLights();
    LoadTextures();

    BuildRootSignature();
    BuildShadersAndInputLayout();
    BuildPSO();
    BuildShapeGeometry();
	BuildMaterials();
    BuildRenderItems();
    BuildFrameResources();

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

    mCamera.SetPosition(0.0f, 2.0f, -5.0f);
    mCamera.LookAt(
        XMFLOAT3(0.0f, 2.0f, -5.0f), 
        XMFLOAT3(0.0f, 0.0f, 0.0f),  
        XMFLOAT3(0.0f, 1.0f, 0.0f)   
    );
    mCamera.SetLens(0.25f * 3.14f, AspectRatio(), 1.0f, 1000.0f);

    return true;
}

void EclipseWalkerGame::OnResize()
{
    GameFramework::OnResize();
    mCamera.SetLens(0.25f * 3.14f, AspectRatio(), 1.0f, 1000.0f);
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


    if (GetAsyncKeyState('W') & 0x8000)
    {
        mTargetPos.x += forwardX * speed;
        mTargetPos.z += forwardZ * speed;
    }


    if (GetAsyncKeyState('S') & 0x8000)
    {
        mTargetPos.x -= forwardX * speed;
        mTargetPos.z -= forwardZ * speed;
    }


    if (GetAsyncKeyState('D') & 0x8000)
    {
        mTargetPos.x += rightX * speed;
        mTargetPos.z += rightZ * speed;
    }


    if (GetAsyncKeyState('A') & 0x8000)
    {
        mTargetPos.x -= rightX * speed;
        mTargetPos.z -= rightZ * speed;
    }

    if (GetAsyncKeyState(VK_LEFT) & 0x8000) mCameraTheta -= 2.0f * dt;
    if (GetAsyncKeyState(VK_RIGHT) & 0x8000) mCameraTheta += 2.0f * dt;
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

    // 2. 입력 처리 및 카메라 로직 수행
    OnKeyboardInput(gt);
    UpdateCamera();

    // 3. 플레이어(박스) 위치 업데이트
    if (mPlayerItem != nullptr)
    {
        mPlayerItem->NumFramesDirty = 3; 

        XMMATRIX scale = XMMatrixScaling(0.3f, 0.3f, 0.3f);
        XMMATRIX rot = XMMatrixRotationY(mCameraTheta + DirectX::XM_PI);
        XMMATRIX trans = XMMatrixTranslation(mTargetPos.x, mTargetPos.y, mTargetPos.z);

        XMMATRIX world = scale * rot * trans;
        XMStoreFloat4x4(&mPlayerItem->World, world);
    }

    // 4. 전역 상수 버퍼(카메라 행렬, 조명 등) 업데이트 
    UpdateMainPassCB(gt);

    // 5. 물체별 상수 버퍼 업데이트
    UpdateObjectCBs(gt);
}
void EclipseWalkerGame::Draw(const GameTimer& gt)
{
    // 1. 커맨드 할당자 리셋 
    auto cmdListAlloc = mCurrFrameResource->CmdListAlloc;
    ThrowIfFailed(cmdListAlloc->Reset());

    // 2. 커맨드 리스트 리셋
    ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPSO.Get()));

    // 3. 뷰포트 & 시저 사각형 설정
    mCommandList->RSSetViewports(1, &mScreenViewport);
    mCommandList->RSSetScissorRects(1, &mScissorRect);

    // 4. 리소스 배리어 (Present -> RenderTarget)
    CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        CurrentBackBuffer(),
        D3D12_RESOURCE_STATE_PRESENT,
        D3D12_RESOURCE_STATE_RENDER_TARGET);
    mCommandList->ResourceBarrier(1, &barrier);

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = CurrentBackBufferView();
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = DepthStencilView();

    // 5. 화면 지우기 (배경색 & 깊이 버퍼)
    mCommandList->ClearRenderTargetView(rtvHandle, Colors::LightSteelBlue, 0, nullptr);
    mCommandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

    // 6. 렌더 타겟 지정
    mCommandList->OMSetRenderTargets(1, &rtvHandle, true, &dsvHandle);

    // -----------------------------------------------------------------------------------------
    // 루트 서명 및 텍스처 힙 설정
    // -----------------------------------------------------------------------------------------
    mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

    // (1) 서술자 힙(SRV Heap) 설정
    ID3D12DescriptorHeap* descriptorHeaps[] = { mSrvDescriptorHeap.Get() };
    mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

    // (2) Pass 상수 버퍼 바인딩 (Slot 1)
    auto passCB = mCurrFrameResource->PassCB->Resource();
    mCommandList->SetGraphicsRootConstantBufferView(1, passCB->GetGPUVirtualAddress());

    // (3) 텍스처 테이블 바인딩 (Slot 2)
    mCommandList->SetGraphicsRootDescriptorTable(2, mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());

    // -----------------------------------------------------------------------------------------
    // 7. 물체 그리기 루프
    // -----------------------------------------------------------------------------------------
    UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
    auto objectCB = mCurrFrameResource->ObjectCB->Resource();

    for (size_t i = 0; i < mAllRitems.size(); ++i)
    {
        auto ri = mAllRitems[i].get();

        D3D12_VERTEX_BUFFER_VIEW vbv = ri->Geo->VertexBufferView();
        D3D12_INDEX_BUFFER_VIEW ibv = ri->Geo->IndexBufferView();

        mCommandList->IASetVertexBuffers(0, 1, &vbv);
        mCommandList->IASetIndexBuffer(&ibv);
        mCommandList->IASetPrimitiveTopology(ri->PrimitiveType);

        // (4) 물체별 상수 버퍼 바인딩 (Slot 0)
        D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress() + ri->ObjCBIndex * objCBByteSize;
        mCommandList->SetGraphicsRootConstantBufferView(0, objCBAddress);

        mCommandList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
    }

    // 8. 리소스 배리어 (RenderTarget -> Present)
    CD3DX12_RESOURCE_BARRIER barrier2 = CD3DX12_RESOURCE_BARRIER::Transition(
        CurrentBackBuffer(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PRESENT);
    mCommandList->ResourceBarrier(1, &barrier2);

    // 9. 명령 기록 종료
    ThrowIfFailed(mCommandList->Close());

    // 10. 커맨드 큐 실행
    ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    // 11. 화면 교체 (Present)
    ThrowIfFailed(mSwapChain->Present(0, 0));
    mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

    // 12. 펜스 값 갱신 (GPU 동기화)
    mCurrFrameResource->Fence = ++mCurrentFence;
    mCommandQueue->Signal(mFence.Get(), mCurrentFence);
}

void EclipseWalkerGame::BuildShapeGeometry()
{
    std::vector<VertexTypes::VertexPosNormalTex> vertices =
    {
        // [앞면]
         { XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT2(0.0f, 1.0f) },
         { XMFLOAT3(-1.0f, +1.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT2(0.0f, 0.0f) },
         { XMFLOAT3(+1.0f, +1.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT2(1.0f, 0.0f) },
         { XMFLOAT3(+1.0f, -1.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT2(1.0f, 1.0f) },

         // [뒷면]
         { XMFLOAT3(-1.0f, -1.0f, +1.0f), XMFLOAT3(0.0f, 0.0f, 1.0f), XMFLOAT2(1.0f, 1.0f) }, 
         { XMFLOAT3(-1.0f, +1.0f, +1.0f), XMFLOAT3(0.0f, 0.0f, 1.0f), XMFLOAT2(1.0f, 0.0f) }, 
         { XMFLOAT3(+1.0f, +1.0f, +1.0f), XMFLOAT3(0.0f, 0.0f, 1.0f), XMFLOAT2(0.0f, 0.0f) }, 
         { XMFLOAT3(+1.0f, -1.0f, +1.0f), XMFLOAT3(0.0f, 0.0f, 1.0f), XMFLOAT2(0.0f, 1.0f) }, 

         // [윗면]
         { XMFLOAT3(-1.0f, +1.0f, -1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT2(0.0f, 1.0f) },
         { XMFLOAT3(-1.0f, +1.0f, +1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT2(0.0f, 0.0f) },
         { XMFLOAT3(+1.0f, +1.0f, +1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT2(1.0f, 0.0f) },
         { XMFLOAT3(+1.0f, +1.0f, -1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT2(1.0f, 1.0f) },

         // [아랫면]
         { XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT3(0.0f, -1.0f, 0.0f), XMFLOAT2(1.0f, 1.0f) },
         { XMFLOAT3(+1.0f, -1.0f, -1.0f), XMFLOAT3(0.0f, -1.0f, 0.0f), XMFLOAT2(0.0f, 1.0f) },
         { XMFLOAT3(+1.0f, -1.0f, +1.0f), XMFLOAT3(0.0f, -1.0f, 0.0f), XMFLOAT2(0.0f, 0.0f) },
         { XMFLOAT3(-1.0f, -1.0f, +1.0f), XMFLOAT3(0.0f, -1.0f, 0.0f), XMFLOAT2(1.0f, 0.0f) },

         // [왼쪽면]
         { XMFLOAT3(-1.0f, -1.0f, +1.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f), XMFLOAT2(0.0f, 1.0f) },
         { XMFLOAT3(-1.0f, +1.0f, +1.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f), XMFLOAT2(0.0f, 0.0f) },
         { XMFLOAT3(-1.0f, +1.0f, -1.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f), XMFLOAT2(1.0f, 0.0f) },
         { XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f), XMFLOAT2(1.0f, 1.0f) },

         // [오른쪽면]
         { XMFLOAT3(+1.0f, -1.0f, -1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f), XMFLOAT2(0.0f, 1.0f) },
         { XMFLOAT3(+1.0f, +1.0f, -1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f), XMFLOAT2(0.0f, 0.0f) },
         { XMFLOAT3(+1.0f, +1.0f, +1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f), XMFLOAT2(1.0f, 0.0f) },
         { XMFLOAT3(+1.0f, -1.0f, +1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f), XMFLOAT2(1.0f, 1.0f) },

         // [바닥 (Grid)]
         { XMFLOAT3(-5.0f, -1.5f, -5.0f), XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT2(0.0f, 5.0f) },
         { XMFLOAT3(-5.0f, -1.5f, +5.0f), XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT2(0.0f, 0.0f) },
         { XMFLOAT3(+5.0f, -1.5f, +5.0f), XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT2(5.0f, 0.0f) },
         { XMFLOAT3(+5.0f, -1.5f, -5.0f), XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT2(5.0f, 5.0f) }
    };

    // 인덱스 정의
    std::vector<std::uint16_t> indices =
    {
        // 앞면
        0, 1, 2, 0, 2, 3,
        // 뒷면
        4, 5, 6, 4, 6, 7,
        // 윗면
        8, 9, 10, 8, 10, 11,
        // 아랫면
        12, 13, 14, 12, 14, 15,
        // 왼쪽면
        16, 17, 18, 16, 18, 19,
        // 오른쪽면
        20, 21, 22, 20, 22, 23,

        // 바닥
        24, 25, 26, 24, 26, 27
    };

    const UINT vbByteSize = (UINT)vertices.size() * sizeof(VertexTypes::VertexPosNormalTex);
    const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

    // ------------------------------------------------------------------
    // 2. MeshGeometry 생성 및 데이터 복사
    // ------------------------------------------------------------------
    auto geo = std::make_unique<MeshGeometry>();
    geo->Name = "shapeGeo"; 

    ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
    CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

    ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
    CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

    geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
        mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

    geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
        mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

    geo->VertexByteStride = sizeof(VertexTypes::VertexPosNormalTex);
    geo->VertexBufferByteSize = vbByteSize;
    geo->IndexFormat = DXGI_FORMAT_R16_UINT;
    geo->IndexBufferByteSize = ibByteSize;

    // ------------------------------------------------------------------
    // 3. 서브메쉬(Submesh) 설정 
    // ------------------------------------------------------------------
    // "전체 버퍼에서 어디부터 어디까지가 상자고, 어디가 바닥인가?"를 정의

    SubmeshGeometry boxSubmesh;
    boxSubmesh.IndexCount = 36;        
    boxSubmesh.StartIndexLocation = 0;   
    boxSubmesh.BaseVertexLocation = 0;

    SubmeshGeometry gridSubmesh;
    gridSubmesh.IndexCount = 6;          
    gridSubmesh.StartIndexLocation = 36; 
    gridSubmesh.BaseVertexLocation = 0;

    // 이름표 붙여서 저장
    geo->DrawArgs["box"] = boxSubmesh;
    geo->DrawArgs["grid"] = gridSubmesh;

    // ------------------------------------------------------------------
    // 4. 전역 맵(mGeometries)에 등록
    // ------------------------------------------------------------------
    mGeometries[geo->Name] = std::move(geo);
}

void EclipseWalkerGame::BuildMaterials()
{
    auto bricks = std::make_unique<Material>();
    bricks->Name = "bricks";
    bricks->MatCBIndex = 0;
    bricks->DiffuseAlbedo = XMFLOAT4(Colors::ForestGreen);
    bricks->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
    bricks->Roughness = 0.8f;

    auto ice = std::make_unique<Material>();
    ice->Name = "ice";
    ice->MatCBIndex = 1;
    ice->DiffuseAlbedo = XMFLOAT4(0.3f, 0.4f, 1.0f, 0.5f);
    ice->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
    ice->Roughness = 0.0f;

    mMaterials["bricks"] = std::move(bricks);
    mMaterials["ice"] = std::move(ice);
}

void EclipseWalkerGame::BuildRenderItems()
{
    // [상자] 아이템 만들기
    auto boxItem = std::make_unique<RenderItem>();

    XMStoreFloat4x4(&boxItem->World, XMMatrixIdentity());

    boxItem->ObjCBIndex = 0;

    boxItem->Geo = mGeometries["shapeGeo"].get();
    boxItem->Mat = mMaterials["ice"].get();

    boxItem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    boxItem->IndexCount = boxItem->Geo->DrawArgs["box"].IndexCount;
    boxItem->StartIndexLocation = boxItem->Geo->DrawArgs["box"].StartIndexLocation;
    boxItem->BaseVertexLocation = boxItem->Geo->DrawArgs["box"].BaseVertexLocation;

    mPlayerItem = boxItem.get();

    mAllRitems.push_back(std::move(boxItem));


    // 2. [바닥] 아이템 만들기
    auto gridItem = std::make_unique<RenderItem>();

    XMStoreFloat4x4(&gridItem->World, XMMatrixIdentity());

    gridItem->ObjCBIndex = 1;

    gridItem->Geo = mGeometries["shapeGeo"].get();
    gridItem->Mat = mMaterials["bricks"].get();

    gridItem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    gridItem->IndexCount = gridItem->Geo->DrawArgs["grid"].IndexCount;
    gridItem->StartIndexLocation = gridItem->Geo->DrawArgs["grid"].StartIndexLocation;
    gridItem->BaseVertexLocation = gridItem->Geo->DrawArgs["grid"].BaseVertexLocation;

    // 리스트에 추가
    mAllRitems.push_back(std::move(gridItem));

    mPlayerItem = mAllRitems[0].get();
}

void EclipseWalkerGame::BuildFrameResources()
{
    for (int i = 0; i < 3; ++i)
    {
        // (디바이스, 패스 개수 1개, 물체 개수 n개)
        mFrameResources.push_back(std::make_unique<FrameResource>(md3dDevice.Get(), 1, (UINT)mAllRitems.size()));
    }
}

float EclipseWalkerGame::AspectRatio() const
{
    return static_cast<float>(mClientWidth) / mClientHeight;
}


std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> EclipseWalkerGame::GetStaticSamplers()
{
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
        0.0f,                             // mipLODBias
        8);                               // maxAnisotropy

    const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(
        5, // shaderRegister
        D3D12_FILTER_ANISOTROPIC,
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        0.0f,
        8);

    return {
        pointWrap, pointClamp,
        linearWrap, linearClamp,
        anisotropicWrap, anisotropicClamp };
}

void EclipseWalkerGame::BuildRootSignature()
{
    CD3DX12_DESCRIPTOR_RANGE texTable;
    texTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

    CD3DX12_ROOT_PARAMETER slotRootParameter[3];

    // 물체용 상수 버퍼 (b0)
    slotRootParameter[0].InitAsConstantBufferView(0);
    //  패스(전역) 상수 버퍼 (b1)
    slotRootParameter[1].InitAsConstantBufferView(1);
    // 텍스처 테이블 (t0)
    slotRootParameter[2].InitAsDescriptorTable(1, &texTable, D3D12_SHADER_VISIBILITY_PIXEL);

    // 정적 샘플러(Sampler) 생성 - 텍스처를 부드럽게 읽는 도구
    auto staticSamplers = GetStaticSamplers(); 

    CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(3, slotRootParameter,
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

void EclipseWalkerGame::BuildShadersAndInputLayout()
{
    mvsByteCode = d3dUtil::CompileShader(L"color.hlsl", nullptr, "VS", "vs_5_0");
    mpsByteCode = d3dUtil::CompileShader(L"color.hlsl", nullptr, "PS", "ps_5_0");

    mInputLayout =
    {
       { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
       { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
       { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };
}

void EclipseWalkerGame::BuildPSO()
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;
    ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));

    psoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
    psoDesc.pRootSignature = mRootSignature.Get();
    psoDesc.VS =
    {
        reinterpret_cast<BYTE*>(mvsByteCode->GetBufferPointer()),
        mvsByteCode->GetBufferSize()
    };
    psoDesc.PS = 
    {
        reinterpret_cast<BYTE*>(mpsByteCode->GetBufferPointer()),
        mpsByteCode->GetBufferSize()
    };

    //psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    CD3DX12_RASTERIZER_DESC rasterizerDesc(D3D12_DEFAULT);
    rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
    psoDesc.RasterizerState = rasterizerDesc;

    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);

    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);

    // 기타 필수 설정
    psoDesc.SampleMask = UINT_MAX; 
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE; 
    psoDesc.NumRenderTargets = 1; 
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1; 
    psoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
    psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSO)));
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
    // (1) 현재 각도(mCameraTheta, Phi)를 이용해 카메라가 있을 곳 계산
    mCameraRadius = 6.0f; // 거리 고정

    float x = mCameraRadius * sinf(mCameraPhi) * cosf(mCameraTheta);
    float z = mCameraRadius * sinf(mCameraPhi) * sinf(mCameraTheta);
    float y = mCameraRadius * cosf(mCameraPhi);

    // (2) 캐릭터 위치(Target)를 가져옴
    XMVECTOR target = XMLoadFloat3(&mTargetPos);

    // (3) 카메라 위치 = 캐릭터 위치 + 각도에 따른 오프셋(x,y,z)
    // 숄더뷰 느낌을 위해 살짝 위(Up)로 올림
    XMVECTOR pos = XMVectorSet(x, y, z, 0.0f);
    XMVECTOR finalPos = target + pos; // 캐릭터 기준 상대 좌표

    // (4) 카메라 갱신 (LookAt: 카메라위치, 바라볼곳, 업벡터)
    mCamera.LookAt(finalPos, target, XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
    mCamera.UpdateViewMatrix();
}


void EclipseWalkerGame::LoadTextures()
{
    // 1. 텍스처 객체 생성
    auto boxTex = std::make_unique<Texture>();
    boxTex->Name = "boxTex";
    boxTex->Filename = L"Textures/box.dds"; // 경로 확인!

   
    std::unique_ptr<uint8_t[]> ddsData;
    std::vector<D3D12_SUBRESOURCE_DATA> subresources;

    
    ThrowIfFailed(DirectX::LoadDDSTextureFromFile(
        md3dDevice.Get(),
        boxTex->Filename.c_str(),
        boxTex->Resource.GetAddressOf(),
        ddsData,
        subresources));

    // 3. 업로드 힙 생성 (GPU로 보내기 위한 임시 버퍼)
    const UINT64 uploadBufferSize = GetRequiredIntermediateSize(boxTex->Resource.Get(), 0, static_cast<UINT>(subresources.size()));

    auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);

    ThrowIfFailed(md3dDevice->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(boxTex->UploadHeap.GetAddressOf())));

    // 4. 데이터 복사 명령 기록 (UpdateSubresources 헬퍼 함수)
    UpdateSubresources(mCommandList.Get(),
        boxTex->Resource.Get(),
        boxTex->UploadHeap.Get(),
        0, 0, static_cast<UINT>(subresources.size()),
        subresources.data());

    // 5. 리소스 상태 변경 (복사 대기 -> 픽셀 쉐이더 읽기 가능)
    CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        boxTex->Resource.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    mCommandList->ResourceBarrier(1, &barrier);

    // 6. 맵에 등록
    mTextures[boxTex->Name] = std::move(boxTex);

    // -----------------------------------------------------------------------
    // (이 아래 SRV 힙 생성 부분은 아까와 똑같습니다)
    // -----------------------------------------------------------------------
    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
    srvHeapDesc.NumDescriptors = 1;
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mSrvDescriptorHeap)));

    auto texture = mTextures["boxTex"]->Resource;

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = texture->GetDesc().Format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = texture->GetDesc().MipLevels;
    srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

    CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
    md3dDevice->CreateShaderResourceView(texture.Get(), &srvDesc, hDescriptor);
}


void EclipseWalkerGame::InitLights()
{
    mGameLights.resize(MaxLights); // 16개 생성 (생성자에서 모두 꺼짐 상태)

    // [0번 조명] 태양 (Directional Light) 설정
    // 방향: (0.57735f, -0.57735f, 0.57735f) -> 대각선 아래로 비춤
    // 색상: (0.8f, 0.8f, 0.8f) -> 밝은 백색광
    mGameLights[0].InitDirectional({ 0.57735f, -0.57735f, 0.57735f }, { 0.8f, 0.8f, 0.8f });

    // 나머지 1~15번은 GameLight 생성자에서 이미 꺼져있으므로 건드릴 필요 없음
}

void EclipseWalkerGame::UpdateMainPassCB(const GameTimer& gt)
{
    // 1. 행렬 계산 (기존 코드 유지)
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

    // -------------------------------------------------------------
    // [조명 업데이트]
    // -------------------------------------------------------------

    // 환경광 (은은한 그림자 색)
    mMainPassCB.AmbientLight = { 0.25f, 0.25f, 0.35f, 1.0f };

    // 모든 조명 업데이트 후 데이터 복사
    for (int i = 0; i < MaxLights; ++i)
    {
        mGameLights[i].Update(gt.DeltaTime());
        mMainPassCB.Lights[i] = mGameLights[i].GetRawData();
    }

    mCurrFrameResource->PassCB->CopyData(0, mMainPassCB);
}