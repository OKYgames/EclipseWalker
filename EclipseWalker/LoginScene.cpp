#include "LoginScene.h"
#include "EclipseWalkerGame.h"
#include "MainMenuScene.h" 
#include "RoomSelectScene.h"
#include "NetworkManager.h"
#include "DebugConfig.h"
#include "GameObject.h"
#include <ResourceUploadBatch.h>
#include <RenderTargetState.h>
#include <Windows.h>

namespace
{
    constexpr float kInputLeft = 500.0f;
    constexpr float kInputRight = 835.0f;
    constexpr float kServerInputRight = 980.0f;
    constexpr float kIdTextY = 390.0f;
    constexpr float kPwTextY = 460.0f;
    constexpr float kServerTextY = 530.0f;
    constexpr float kServerInputTextScale = 0.78f;
    constexpr float kLoginButtonLeft = 510.0f;
    constexpr float kLoginButtonTop = 600.0f;
    constexpr float kLoginButtonRight = 620.0f;
    constexpr float kLoginButtonBottom = 630.0f;
    constexpr float kRegisterButtonLeft = 650.0f;
    constexpr float kRegisterButtonTop = 600.0f;
    constexpr float kRegisterButtonRight = 805.0f;
    constexpr float kRegisterButtonBottom = 630.0f;
    constexpr float kAuthButtonTextScale = 0.78f;

    bool IsPointInside(float x, float y, float left, float top, float right, float bottom)
    {
        return x >= left && x <= right && y >= top && y <= bottom;
    }

    bool IsServerIpChar(char c)
    {
        return (c >= '0' && c <= '9') || c == '.';
    }
}

void LoginScene::Enter()
{
    mGame->FlushCommandQueue();
    OutputDebugStringA("[Login Scene] 진입: 타이틀 화면 로드 중...\n");

    auto res = mGame->GetResources();
    auto& ritems = mGame->GetRitems();

    auto& gameObjects = mGame->GetGameObjects();

    ritems.clear();
    gameObjects.clear();

    auto camera = mGame->GetCamera(); 
    camera->SetPosition(0.0f, 0.0f, -10.0f);
    camera->LookAt(
        DirectX::XMFLOAT3(0.0f, 0.0f, -10.0f), 
        DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f),  
        DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f)   
    );
    camera->UpdateViewMatrix();

    // =========================================================
    // 2. 타이틀 배치 
    // =========================================================
    auto titleRitem = std::make_unique<RenderItem>();

    titleRitem->TexTransform = MathHelper::Identity4x4();
    titleRitem->ObjCBIndex = 0;

    titleRitem->NumFramesDirty = 3;

    titleRitem->Mat = res->GetMaterial("TitleMat");
    titleRitem->Geo = res->mGeometries["quadGeo"].get();
    titleRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    titleRitem->IndexCount = titleRitem->Geo->DrawArgs["quad"].IndexCount;
    titleRitem->StartIndexLocation = titleRitem->Geo->DrawArgs["quad"].StartIndexLocation;
    titleRitem->BaseVertexLocation = titleRitem->Geo->DrawArgs["quad"].BaseVertexLocation;

    auto titleObj = std::make_unique<GameObject>();
    titleObj->Ritem = titleRitem.get();

    titleObj->SetScale(8.0f, 4.5f, 1.0f);
    titleObj->SetPosition(0.0f, 0.0f, 0.0f);

    ritems.push_back(std::move(titleRitem));
    gameObjects.push_back(std::move(titleObj));

    // =========================================================
    // 2. ID / PW 입력창 배경 박스 배치 (UI 대용)
    // =========================================================
    // [ID 입력창 배경]
    res->LoadTexture("white", L"Textures/white.dds");
    res->CreateMaterial(
        "UI_BgMat",                 // 재질 이름
        static_cast<int>(res->mMaterials.size()), // MatCBIndex
        "white",                    // Diffuse 텍스처 
        "None",                     // Normal
        "white",                     // Emissive
        "None",                     // Metallic
        DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 0.8f),
        DirectX::XMFLOAT3(0.04f, 0.04f, 0.04f),
        0.8f                                       // Roughness
    );

    auto idBgRitem = std::make_unique<RenderItem>();
    idBgRitem->TexTransform = MathHelper::Identity4x4();
    idBgRitem->ObjCBIndex = static_cast<UINT>(ritems.size());
    idBgRitem->NumFramesDirty = 3;
    idBgRitem->Mat = res->GetMaterial("UI_BgMat");
    idBgRitem->Geo = res->mGeometries["quadGeo"].get();
    idBgRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    idBgRitem->IndexCount = idBgRitem->Geo->DrawArgs["quad"].IndexCount;
    idBgRitem->StartIndexLocation = idBgRitem->Geo->DrawArgs["quad"].StartIndexLocation;
    idBgRitem->BaseVertexLocation = idBgRitem->Geo->DrawArgs["quad"].BaseVertexLocation;

    auto idBgObj = std::make_unique<GameObject>();
    idBgObj->Ritem = idBgRitem.get();
    idBgObj->SetScale(1.5f, 0.2f, 1.0f);
    idBgObj->SetPosition(0.0f, -0.55f, -0.001f);

    ritems.push_back(std::move(idBgRitem));
    gameObjects.push_back(std::move(idBgObj));

    // [PW 입력창 배경]
    auto pwBgRitem = std::make_unique<RenderItem>();
    pwBgRitem->TexTransform = MathHelper::Identity4x4();
    pwBgRitem->ObjCBIndex = static_cast<UINT>(ritems.size());
    pwBgRitem->NumFramesDirty = 3;
    pwBgRitem->Mat = res->GetMaterial("UI_BgMat"); // ID창과 동일한 재질 사용
    pwBgRitem->Geo = res->mGeometries["quadGeo"].get();
    pwBgRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    pwBgRitem->IndexCount = pwBgRitem->Geo->DrawArgs["quad"].IndexCount;
    pwBgRitem->StartIndexLocation = pwBgRitem->Geo->DrawArgs["quad"].StartIndexLocation;
    pwBgRitem->BaseVertexLocation = pwBgRitem->Geo->DrawArgs["quad"].BaseVertexLocation;

    auto pwBgObj = std::make_unique<GameObject>();
    pwBgObj->Ritem = pwBgRitem.get();
    pwBgObj->SetScale(1.5f, 0.2f, 1.0f);
    pwBgObj->SetPosition(0.0f, -1.35f, -0.001f);

    ritems.push_back(std::move(pwBgRitem));
    gameObjects.push_back(std::move(pwBgObj));

    auto ipBgRitem = std::make_unique<RenderItem>();
    ipBgRitem->TexTransform = MathHelper::Identity4x4();
    ipBgRitem->ObjCBIndex = static_cast<UINT>(ritems.size());
    ipBgRitem->NumFramesDirty = 3;
    ipBgRitem->Mat = res->GetMaterial("UI_BgMat");
    ipBgRitem->Geo = res->mGeometries["quadGeo"].get();
    ipBgRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    ipBgRitem->IndexCount = ipBgRitem->Geo->DrawArgs["quad"].IndexCount;
    ipBgRitem->StartIndexLocation = ipBgRitem->Geo->DrawArgs["quad"].StartIndexLocation;
    ipBgRitem->BaseVertexLocation = ipBgRitem->Geo->DrawArgs["quad"].BaseVertexLocation;

    auto ipBgObj = std::make_unique<GameObject>();
    ipBgObj->Ritem = ipBgRitem.get();
    ipBgObj->SetScale(2.2f, 0.2f, 1.0f);
    ipBgObj->SetPosition(0.0f, -2.15f, -0.001f);

    ritems.push_back(std::move(ipBgRitem));
    gameObjects.push_back(std::move(ipBgObj));
    mGame->BuildDescriptorHeaps();

    // =========================================================
    //폰트 로딩 시스템 초기화
    auto device = mGame->GetDevice();
    auto cmdQueue = mGame->GetCommandQueue();

    // 1. 메모리 및 힙 중복 생성 방지
    if (!mGraphicsMemory) {
        mGraphicsMemory = std::make_unique<DirectX::GraphicsMemory>(device);
    }
    if (!mFontHeap) {
        mFontHeap = std::make_unique<DirectX::DescriptorHeap>(
            device,
            D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
            D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
            1);
    }

    DirectX::ResourceUploadBatch resourceUpload(device);
    resourceUpload.Begin();

    try
    {
        mFont = std::make_unique<DirectX::SpriteFont>(
            device, resourceUpload,
            L"Textures/myfile.spritefont",
            mFontHeap->GetCpuHandle(0),
            mFontHeap->GetGpuHandle(0)
        );
    }
    catch (std::exception& e)
    {
        OutputDebugStringA("\n=================================\n");
        OutputDebugStringA("[!!! 폰트 로딩 에러 원인 !!!]: ");
        OutputDebugStringA(e.what());
        OutputDebugStringA("\n=================================\n");
        throw; // 에러 메시지를 찍고 다시 튕기게 합니다.
    }

    // 3. 포맷 설정
    DirectX::RenderTargetState rtState(
        DXGI_FORMAT_R8G8B8A8_UNORM,
        DXGI_FORMAT_D24_UNORM_S8_UINT
    );

    DirectX::SpriteBatchPipelineStateDescription pd(rtState);
    mSpriteBatch = std::make_unique<DirectX::SpriteBatch>(device, resourceUpload, pd);
    try
    {
        auto uploadResourcesFinished = resourceUpload.End(cmdQueue);
        uploadResourcesFinished.wait();
    }
    catch (std::exception& e)
    {
        OutputDebugStringA("\n=================================\n");
        OutputDebugStringA("[!!! GPU 업로드 에러 원인 !!!]: ");
        OutputDebugStringA(e.what());
        OutputDebugStringA("\n=================================\n");
        throw;
    }
}

void LoginScene::Exit() 
{
    mGame->FlushCommandQueue();
    mGame->UnloadSharedGameResources();
    auto& ritems = mGame->GetRitems();
    auto& gameObjects = mGame->GetGameObjects(); 

    ritems.clear();
    gameObjects.clear();
}

void LoginScene::TryLogin()
{
    if (mLoginRequested || mRegisterRequested)
    {
        return;
    }

    if (GetTickCount64() - gLastSceneChangeTime <= 500)
    {
        return;
    }

    gLastSceneChangeTime = GetTickCount64();
    if (!DebugConfig::kEnableDbLogin)
    {
        if (DebugConfig::kEnableBackendConnection)
        {
            mGame->ChangeScene(std::make_unique<RoomSelectScene>(mGame));
        }
        else
        {
            mGame->ChangeScene(std::make_unique<MainMenuScene>(mGame));
        }
        return;
    }

    if (mInputID.empty() || mInputPW.empty())
    {
        mStatusText = "Enter ID and PW";
        return;
    }

    BeginAuthRequest(1);
}

void LoginScene::TryRegister()
{
    if (mLoginRequested || mRegisterRequested || !DebugConfig::kEnableDbLogin)
    {
        return;
    }

    if (mInputID.empty() || mInputPW.empty())
    {
        mStatusText = "Enter ID and PW";
        return;
    }

    BeginAuthRequest(2);
}

void LoginScene::BeginAuthRequest(int authMode)
{
    if (!DebugConfig::kEnableBackendConnection)
    {
        mStatusText = "Backend connection disabled";
        return;
    }

    if (mInputServerIp.empty())
    {
        mStatusText = "Enter server IP";
        return;
    }

    auto* network = NetworkManager::Get();
    if (authMode == 1)
    {
        mLoginRequested = true;
    }
    else
    {
        mRegisterRequested = true;
    }

    if (network->IsConnected())
    {
        if (authMode == 1)
        {
            network->SendLogin(mInputID, mInputPW);
            mStatusText = "Logging in...";
        }
        else
        {
            network->SendRegister(mInputID, mInputPW);
            mStatusText = "Registering...";
        }
        return;
    }

    mPendingAuthMode = authMode;
    if (!network->IsConnecting())
    {
        network->ConnectAsync(mInputServerIp, DebugConfig::kServerPort);
    }
    mStatusText = "Connecting to server...";
}

void LoginScene::HandleMouseClick(float x, float y)
{
    if (IsPointInside(x, y, kInputLeft, kIdTextY - 10.0f, kInputRight, kIdTextY + 32.0f))
    {
        mCurrentFocus = 0;
        return;
    }

    if (IsPointInside(x, y, kInputLeft, kPwTextY - 10.0f, kInputRight, kPwTextY + 32.0f))
    {
        mCurrentFocus = 1;
        return;
    }

    if (IsPointInside(x, y, kInputLeft, kServerTextY - 10.0f, kServerInputRight, kServerTextY + 32.0f))
    {
        mCurrentFocus = 2;
        return;
    }

    if (IsPointInside(x, y, kLoginButtonLeft, kLoginButtonTop, kLoginButtonRight, kLoginButtonBottom))
    {
        TryLogin();
        return;
    }

    if (IsPointInside(x, y, kRegisterButtonLeft, kRegisterButtonTop, kRegisterButtonRight, kRegisterButtonBottom))
    {
        TryRegister();
    }
}

void LoginScene::Update(const GameTimer& gt)
{
    auto* network = NetworkManager::Get();
    if (mPendingAuthMode != 0)
    {
        if (network->ConsumeConnectFailed())
        {
            mLoginRequested = false;
            mRegisterRequested = false;
            mPendingAuthMode = 0;
            mStatusText = "Server connect failed";
        }
        else if (network->IsConnected())
        {
            if (mPendingAuthMode == 1)
            {
                network->SendLogin(mInputID, mInputPW);
                mStatusText = "Logging in...";
            }
            else
            {
                network->SendRegister(mInputID, mInputPW);
                mStatusText = "Registering...";
            }
            mPendingAuthMode = 0;
        }
    }

    const int loginResult = DebugConfig::kEnableDbLogin
        ? NetworkManager::Get()->ConsumeLoginResult()
        : 0;
    if (loginResult > 0)
    {
        gLastSceneChangeTime = GetTickCount64();
        mGame->ChangeScene(std::make_unique<RoomSelectScene>(mGame));
        return;
    }
    if (loginResult < 0)
    {
        mLoginRequested = false;
        mPendingAuthMode = 0;
        mStatusText = "Login failed";
    }

    const int registerResult = DebugConfig::kEnableDbLogin
        ? NetworkManager::Get()->ConsumeRegisterResult()
        : 0;
    if (registerResult > 0)
    {
        mRegisterRequested = false;
        mStatusText = "Register success. Click LOGIN";
    }
    if (registerResult < 0)
    {
        mRegisterRequested = false;
        mPendingAuthMode = 0;
        mStatusText = "Register failed";
    }

    const bool hasFocus = (mGame != nullptr && GetForegroundWindow() == mGame->GetMainWindowHandle());
    if (hasFocus && (GetAsyncKeyState(VK_RETURN) & 0x8000))
    {
        TryLogin();
    }

    const bool leftMouseDown = hasFocus && (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    if (leftMouseDown && !mMousePressed)
    {
        POINT cursor = {};
        if (GetCursorPos(&cursor) && ScreenToClient(mGame->GetMainWindowHandle(), &cursor))
        {
            HandleMouseClick(static_cast<float>(cursor.x), static_cast<float>(cursor.y));
        }
    }
    mMousePressed = leftMouseDown;

    if (mGraphicsMemory)
    {
        mGraphicsMemory->Commit(mGame->GetCommandQueue());
    }
}

void LoginScene::Draw(const GameTimer& gt)
{
    try
    {
        auto cmdList = mGame->GetCommandList();

        // 1. 폰트용 힙(Heap) 세팅
        ID3D12DescriptorHeap* heaps[] = { mFontHeap->Heap() };
        cmdList->SetDescriptorHeaps(1, heaps);
        mSpriteBatch->SetViewport(mGame->GetScreenViewport());
        // 2. 글자 그리기 시작
        mSpriteBatch->Begin(cmdList);

        // 커서 깜빡임 효과
        static float cursorBlinkTime = 0.0f;
        cursorBlinkTime += gt.DeltaTime();
        bool showCursor = (fmod(cursorBlinkTime, 1.0f) < 0.5f);

        std::string idText = "ID : " + mInputID + (mCurrentFocus == 0 && showCursor ? "_" : "");
        std::string hiddenPW(mInputPW.length(), '*');
        std::string pwText = "PW : " + hiddenPW + (mCurrentFocus == 1 && showCursor ? "_" : "");
        std::string serverText = "IP : " + mInputServerIp + (mCurrentFocus == 2 && showCursor ? "_" : "");

        // 화면에 글자 찍기(위치 X, Y와 색상 지정)
        mFont->DrawString(mSpriteBatch.get(), idText.c_str(), DirectX::XMFLOAT2(510.0f, kIdTextY), DirectX::Colors::Black);
        mFont->DrawString(mSpriteBatch.get(), pwText.c_str(), DirectX::XMFLOAT2(510.0f, kPwTextY), DirectX::Colors::Black);
        mFont->DrawString(
            mSpriteBatch.get(),
            serverText.c_str(),
            DirectX::XMFLOAT2(510.0f, kServerTextY),
            DirectX::Colors::Black,
            0.0f,
            DirectX::XMFLOAT2(0.0f, 0.0f),
            kServerInputTextScale);
        mFont->DrawString(
            mSpriteBatch.get(),
            "[ LOGIN ]",
            DirectX::XMFLOAT2(kLoginButtonLeft, kLoginButtonTop),
            DirectX::Colors::LimeGreen,
            0.0f,
            DirectX::XMFLOAT2(0.0f, 0.0f),
            kAuthButtonTextScale);
        mFont->DrawString(
            mSpriteBatch.get(),
            "[ REGISTER ]",
            DirectX::XMFLOAT2(kRegisterButtonLeft, kRegisterButtonTop),
            DirectX::Colors::DodgerBlue,
            0.0f,
            DirectX::XMFLOAT2(0.0f, 0.0f),
            kAuthButtonTextScale);

        // ★ 여기서 터지면 아래 catch 블록이 잡아서 로그를 띄워줍니다!
        if (!mStatusText.empty())
        {
            mFont->DrawString(mSpriteBatch.get(), mStatusText.c_str(), DirectX::XMFLOAT2(510.0f, 650.0f), DirectX::Colors::DarkRed);
        }

        mSpriteBatch->End();
    }
    catch (std::exception& e)
    {
        OutputDebugStringA("\n=================================\n");
        OutputDebugStringA("[!!! Draw End 에러 원인 !!!]: ");
        OutputDebugStringA(e.what());
        OutputDebugStringA("\n=================================\n");
        throw;
    }
}

void LoginScene::OnCharInput(WPARAM wParam)
{
    // 백스페이스(지우기) 처리
    if (wParam == VK_BACK)
    {
        if (mCurrentFocus == 0 && !mInputID.empty()) mInputID.pop_back();
        else if (mCurrentFocus == 1 && !mInputPW.empty()) mInputPW.pop_back();
        else if (mCurrentFocus == 2 && !mInputServerIp.empty()) mInputServerIp.pop_back();
        mLoginRequested = false;
        mRegisterRequested = false;
        mPendingAuthMode = 0;
        mStatusText.clear();
    }
    // 탭(Tab) 키: ID 창과 PW 창 전환
    else if (wParam == VK_TAB)
    {
        mCurrentFocus = (mCurrentFocus + 1) % 3;
    }
    // 일반 글자 입력 (알파벳, 숫자 등)
    else if (wParam >= 32 && wParam <= 126)
    {
        char typedChar = static_cast<char>(wParam);

        if (mCurrentFocus == 0 && mInputID.length() < 15) mInputID += typedChar;
        else if (mCurrentFocus == 1 && mInputPW.length() < 15) mInputPW += typedChar;
        else if (mCurrentFocus == 2 && mInputServerIp.length() < 15 && IsServerIpChar(typedChar)) mInputServerIp += typedChar;
        mLoginRequested = false;
        mRegisterRequested = false;
        mPendingAuthMode = 0;
        mStatusText.clear();
    }

    std::string debugMsg = "[Login Input Test] ID: " + mInputID + " / PW: " + mInputPW + "\n";
    OutputDebugStringA(debugMsg.c_str());
}
