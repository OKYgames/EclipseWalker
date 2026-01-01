#include "EclipseWalkerGame.h"
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

    BuildConstantBuffer();
    BuildRootSignature();
    BuildShadersAndInputLayout();
    BuildPSO();

    // 상자
    BuildBoxGeometry();

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

void EclipseWalkerGame::Update(const GameTimer& gt)
{
    OnKeyboardInput(gt);
    UpdateCamera();
    UpdateObjectCBs(gt);
}

void EclipseWalkerGame::Draw(const GameTimer& gt)
{
    // 1. 명령 할당자 리셋
    ThrowIfFailed(mDirectCmdListAlloc->Reset());

    // 2. 명령 리스트 리셋
    ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), mPSO.Get()));

    // 3. 리소스 배리어 
    CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        mSwapChainBuffer[mCurrBackBuffer].Get(),
        D3D12_RESOURCE_STATE_PRESENT,
        D3D12_RESOURCE_STATE_RENDER_TARGET);
    mCommandList->ResourceBarrier(1, &barrier);

    // 4. 뷰포트/시저 설정
    mCommandList->RSSetViewports(1, &mScreenViewport);
    mCommandList->RSSetScissorRects(1, &mScissorRect);

    // 5. 렌더 타겟 핸들(RTV) 가져오기
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(
        mRtvHeap->GetCPUDescriptorHandleForHeapStart(),
        mCurrBackBuffer,
        mRtvDescriptorSize);

    // 깊이 스텐실 핸들(DSV) 가져오기
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = mDsvHeap->GetCPUDescriptorHandleForHeapStart();

    // 6. 화면 지우기 (파란색)
    float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
    mCommandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

    // 깊이 버퍼 지우기
    mCommandList->ClearDepthStencilView(
        dsvHandle,
        D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

    // 7. 렌더 타겟 지정
    mCommandList->OMSetRenderTargets(1, &rtvHandle, true, &dsvHandle);

    mCommandList->SetGraphicsRootSignature(mRootSignature.Get());
    mCommandList->SetGraphicsRootConstantBufferView(0, mObjectCB->GetGPUVirtualAddress());

    auto vbv = mBoxGeo->VertexBufferView();
    auto ibv = mBoxGeo->IndexBufferView();
    mCommandList->IASetVertexBuffers(0, 1, &vbv);
    mCommandList->IASetIndexBuffer(&ibv);

    mCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    mCommandList->DrawIndexedInstanced(
        mBoxGeo->IndexBufferByteSize / sizeof(uint16_t), 
        1, 0, 0, 0);

    barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        mSwapChainBuffer[mCurrBackBuffer].Get(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PRESENT);
    mCommandList->ResourceBarrier(1, &barrier);

    ThrowIfFailed(mCommandList->Close());

    ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    ThrowIfFailed(mSwapChain->Present(0, 0));
    mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

    mCurrentFence++;
    ThrowIfFailed(mCommandQueue->Signal(mFence.Get(), mCurrentFence));

    if (mFence->GetCompletedValue() < mCurrentFence)
    {
        HANDLE eventHandle = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (eventHandle == nullptr)
        {
            ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
        }

        ThrowIfFailed(mFence->SetEventOnCompletion(mCurrentFence, eventHandle));
        WaitForSingleObject(eventHandle, INFINITE);
        CloseHandle(eventHandle);
    }
}

void EclipseWalkerGame::BuildBoxGeometry()
{
    std::array<VertexTypes::VertexPosColor, 8> vertices =
    { {
            { XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT4(Colors::White) },
            { XMFLOAT3(-1.0f, +1.0f, -1.0f), XMFLOAT4(Colors::Black) },
            { XMFLOAT3(+1.0f, +1.0f, -1.0f), XMFLOAT4(Colors::Red) },
            { XMFLOAT3(+1.0f, -1.0f, -1.0f), XMFLOAT4(Colors::Green) },

            { XMFLOAT3(-1.0f, -1.0f, +1.0f), XMFLOAT4(Colors::Blue) },
            { XMFLOAT3(-1.0f, +1.0f, +1.0f), XMFLOAT4(Colors::Yellow) },
            { XMFLOAT3(+1.0f, +1.0f, +1.0f), XMFLOAT4(Colors::Cyan) },
            { XMFLOAT3(+1.0f, -1.0f, +1.0f), XMFLOAT4(Colors::Magenta) }
        } };

    std::array<std::uint16_t, 36> indices =
    {
        // 앞면
        0, 1, 2,
        0, 2, 3,

        // 뒷면
        4, 6, 5,
        4, 7, 6,

        // 왼쪽면
        4, 5, 1,
        4, 1, 0,

        // 오른쪽면
        3, 2, 6,
        3, 6, 7,

        // 윗면
        1, 5, 6,
        1, 6, 2,

        // 아랫면
        4, 0, 3,
        4, 3, 7
    };

    const UINT vbByteSize = (UINT)vertices.size() * sizeof(VertexTypes::VertexPosColor);
    const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

    mBoxGeo = std::make_unique<MeshGeometry>();
    mBoxGeo->Name = "boxGeo";

    ThrowIfFailed(D3DCreateBlob(vbByteSize, &mBoxGeo->VertexBufferCPU));
    CopyMemory(mBoxGeo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

    ThrowIfFailed(D3DCreateBlob(ibByteSize, &mBoxGeo->IndexBufferCPU));
    CopyMemory(mBoxGeo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

    mBoxGeo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
        mCommandList.Get(), vertices.data(), vbByteSize, mBoxGeo->VertexBufferUploader);

    mBoxGeo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
        mCommandList.Get(), indices.data(), ibByteSize, mBoxGeo->IndexBufferUploader);

    mBoxGeo->VertexByteStride = sizeof(VertexTypes::VertexPosColor);
    mBoxGeo->VertexBufferByteSize = vbByteSize;
    mBoxGeo->IndexFormat = DXGI_FORMAT_R16_UINT;
    mBoxGeo->IndexBufferByteSize = ibByteSize;
}

float EclipseWalkerGame::AspectRatio() const
{
    return static_cast<float>(mClientWidth) / mClientHeight;
}

void EclipseWalkerGame::BuildRootSignature()
{
    CD3DX12_ROOT_PARAMETER slotRootParameter[1];

    slotRootParameter[0].InitAsConstantBufferView(0);

    CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(1, slotRootParameter, 0, nullptr,
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
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
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

    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);

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

void EclipseWalkerGame::BuildConstantBuffer()
{
    UINT elementByteSize = (sizeof(ObjectConstants) + 255) & ~255;

    auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(elementByteSize);

    ThrowIfFailed(md3dDevice->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&mObjectCB)));

    ThrowIfFailed(mObjectCB->Map(0, nullptr, reinterpret_cast<void**>(&mMappedData)));
}


void EclipseWalkerGame::OnMouseDown(WPARAM btnState, int x, int y)
{
  
    mLastMousePos.x = x;
    mLastMousePos.y = y;
    SetCapture(mhMainWnd); 
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
    static float t = 0.0f;
    t += gt.DeltaTime();

    XMMATRIX scale = XMMatrixScaling(0.5f, 0.5f, 0.5f);

    XMMATRIX rot = XMMatrixRotationY(mCameraTheta + 3.141592f);

    XMMATRIX trans = XMMatrixTranslation(mTargetPos.x, mTargetPos.y, mTargetPos.z);

    XMMATRIX world = scale * rot * trans;

    XMStoreFloat4x4(&mWorld, world);
    XMMATRIX viewProj = mCamera.GetViewProj();
    XMMATRIX worldViewProj = world * viewProj;

    ObjectConstants objConstants;
    XMStoreFloat4x4(&objConstants.WorldViewProj, XMMatrixTranspose(worldViewProj));

    memcpy(mMappedData, &objConstants, sizeof(ObjectConstants));
}

void EclipseWalkerGame::UpdateCamera()
{
    // 1. 카메라 방향 벡터 다시 계산 
    float x = sinf(mCameraPhi) * cosf(mCameraTheta);
    float z = sinf(mCameraPhi) * sinf(mCameraTheta);
    float y = cosf(mCameraPhi);

    XMVECTOR lookVec = XMVectorSet(x, y, z, 0.0f);
    XMVECTOR upVec = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    XMVECTOR rightVec = XMVector3Normalize(XMVector3Cross(upVec, lookVec));

    // Y축이 제거된 평면 앞방향 
    XMVECTOR flatForward = XMVector3Normalize(XMVectorSet(x, 0.0f, z, 0.0f));

    // 2. 숄더뷰 위치 계산
    XMVECTOR targetPos = XMLoadFloat3(&mTargetPos);

    float shoulderOffset = 2.0f;
    float upOffset = 2.5f;
    float backDist = 6.0f;

    XMVECTOR cameraPos = targetPos
        - (flatForward * backDist)      // 뒤로
        + (rightVec * shoulderOffset)   // 오른쪽으로
        + (upVec * upOffset);           // 위로

    // 3. 카메라가 쳐다볼 곳 (캐릭터보다 살짝 오른쪽 위)
    XMVECTOR focusPoint = targetPos
        + (rightVec * shoulderOffset)
        + (upVec * 1.5f);

    mCamera.LookAt(cameraPos, focusPoint, upVec);
    mCamera.UpdateViewMatrix();
}

void EclipseWalkerGame::OnKeyboardInput(const GameTimer& gt)
{
    float dt = gt.DeltaTime();
    float speed = 10.0f;

    float x = sinf(mCameraPhi) * cosf(mCameraTheta);
    float z = sinf(mCameraPhi) * sinf(mCameraTheta);
    float y = cosf(mCameraPhi);

    XMVECTOR lookVec = XMVectorSet(x, y, z, 0.0f);
    XMVECTOR flatForward = XMVector3Normalize(XMVectorSet(x, 0.0f, z, 0.0f)); 
    XMVECTOR rightVec = XMVector3Normalize(XMVector3Cross(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), lookVec));

    if (GetAsyncKeyState('W') & 0x8000)
    {
        mTargetPos.x += XMVectorGetX(flatForward) * speed * dt;
        mTargetPos.z += XMVectorGetZ(flatForward) * speed * dt;
    }
    if (GetAsyncKeyState('S') & 0x8000)
    {
        mTargetPos.x -= XMVectorGetX(flatForward) * speed * dt;
        mTargetPos.z -= XMVectorGetZ(flatForward) * speed * dt;
    }
    if (GetAsyncKeyState('D') & 0x8000)
    {
        mTargetPos.x += XMVectorGetX(rightVec) * speed * dt;
        mTargetPos.z += XMVectorGetZ(rightVec) * speed * dt;
    }
    if (GetAsyncKeyState('A') & 0x8000)
    {
        mTargetPos.x -= XMVectorGetX(rightVec) * speed * dt;
        mTargetPos.z -= XMVectorGetZ(rightVec) * speed * dt;
    }
}