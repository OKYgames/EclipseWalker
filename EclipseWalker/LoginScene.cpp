#include "LoginScene.h"
#include "EclipseWalkerGame.h"
#include "MainMenuScene.h" 
#include "NetworkManager.h"
#include "GameObject.h"
#include <ResourceUploadBatch.h>
#include <RenderTargetState.h>
#include <Windows.h>

namespace
{
    constexpr bool kEnableDbLogin = false;
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
    idBgObj->SetPosition(0.0f, -1.0f, -0.001f);

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
    pwBgObj->SetPosition(0.0f, -1.8f, -0.001f);

    ritems.push_back(std::move(pwBgRitem));
    gameObjects.push_back(std::move(pwBgObj));
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
    auto& ritems = mGame->GetRitems();
    auto& gameObjects = mGame->GetGameObjects(); 

    ritems.clear();
    gameObjects.clear();
}

void LoginScene::Update(const GameTimer& gt)
{
    const int loginResult = NetworkManager::Get()->ConsumeLoginResult();
    if (loginResult > 0)
    {
        gLastSceneChangeTime = GetTickCount64();
        mGame->ChangeScene(std::make_unique<MainMenuScene>(mGame));
        return;
    }
    if (loginResult < 0)
    {
        mLoginRequested = false;
        mStatusText = "Login failed";
    }

    const bool hasFocus = (mGame != nullptr && GetForegroundWindow() == mGame->GetMainWindowHandle());
    if (hasFocus && (GetAsyncKeyState(VK_RETURN) & 0x8000))
    {
        if (!mLoginRequested && GetTickCount64() - gLastSceneChangeTime > 500)
        {
            gLastSceneChangeTime = GetTickCount64();
            if (mInputID.empty() || mInputPW.empty())
            {
                mStatusText = kEnableDbLogin ? "Enter ID and PW" : "Waiting for debug login...";
            }
            else
            {
                if (kEnableDbLogin)
                {
                    NetworkManager::Get()->SendLogin(mInputID, mInputPW);
                    mLoginRequested = true;
                    mStatusText = "Logging in...";
                }
                else
                {
                    mStatusText = "DB login disabled";
                }
            }
        }
    }
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

        // 화면에 글자 찍기(위치 X, Y와 색상 지정)
        mFont->DrawString(mSpriteBatch.get(), idText.c_str(), DirectX::XMFLOAT2(510.0f, 430.0f), DirectX::Colors::Black);
        mFont->DrawString(mSpriteBatch.get(), pwText.c_str(), DirectX::XMFLOAT2(510.0f, 500.0f), DirectX::Colors::Black);

        // ★ 여기서 터지면 아래 catch 블록이 잡아서 로그를 띄워줍니다!
        if (!mStatusText.empty())
        {
            mFont->DrawString(mSpriteBatch.get(), mStatusText.c_str(), DirectX::XMFLOAT2(510.0f, 560.0f), DirectX::Colors::DarkRed);
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
        mLoginRequested = false;
        mStatusText.clear();
    }
    // 탭(Tab) 키: ID 창과 PW 창 전환
    else if (wParam == VK_TAB)
    {
        mCurrentFocus = (mCurrentFocus == 0) ? 1 : 0;
    }
    // 일반 글자 입력 (알파벳, 숫자 등)
    else if (wParam >= 32 && wParam <= 126)
    {
        char typedChar = static_cast<char>(wParam);

        if (mCurrentFocus == 0 && mInputID.length() < 15) mInputID += typedChar;
        else if (mCurrentFocus == 1 && mInputPW.length() < 15) mInputPW += typedChar;
        mLoginRequested = false;
        mStatusText.clear();
    }

    std::string debugMsg = "[Login Input Test] ID: " + mInputID + " / PW: " + mInputPW + "\n";
    OutputDebugStringA(debugMsg.c_str());
}
