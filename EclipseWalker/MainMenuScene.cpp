#include "MainMenuScene.h"
#include "EclipseWalkerGame.h"
#include "CharSelectScene.h"
#include "GameObject.h"
#include <ResourceUploadBatch.h>
#include <RenderTargetState.h>
#include <Windows.h>

using namespace DirectX;

void MainMenuScene::Enter()
{
    mGame->FlushCommandQueue();
    OutputDebugStringA("\n[Main Menu Scene] 진입: 로비 대기 화면\n");

    auto* res = mGame->GetResources();
    auto& ritems = mGame->GetRitems();
    auto& gameObjects = mGame->GetGameObjects();
    ritems.clear();
    gameObjects.clear();

    auto camera = mGame->GetCamera();
    camera->SetPosition(0.0f, 0.0f, -10.0f);
    camera->LookAt(
        DirectX::XMFLOAT3(0.0f, 0.0f, -10.0f),
        DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f),
        DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f));
    camera->UpdateViewMatrix();

    auto backgroundRitem = std::make_unique<RenderItem>();
    backgroundRitem->TexTransform = MathHelper::Identity4x4();
    backgroundRitem->ObjCBIndex = 0;
    backgroundRitem->NumFramesDirty = 3;
    backgroundRitem->Mat = res->GetMaterial("MainMenuMat");
    backgroundRitem->Geo = res->mGeometries["quadGeo"].get();
    backgroundRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    backgroundRitem->IndexCount = backgroundRitem->Geo->DrawArgs["quad"].IndexCount;
    backgroundRitem->StartIndexLocation = backgroundRitem->Geo->DrawArgs["quad"].StartIndexLocation;
    backgroundRitem->BaseVertexLocation = backgroundRitem->Geo->DrawArgs["quad"].BaseVertexLocation;

    auto backgroundObj = std::make_unique<GameObject>();
    backgroundObj->Ritem = backgroundRitem.get();
    backgroundObj->SetScale(8.0f, 4.5f, 1.0f);
    backgroundObj->SetPosition(0.0f, 0.0f, 0.0f);

    ritems.push_back(std::move(backgroundRitem));
    gameObjects.push_back(std::move(backgroundObj));

    InitializeUiResources();
    mReadyKeyPressed = false;
    mStartKeyPressed = false;
    RefreshLobbyState();
}

void MainMenuScene::Exit()
{
    mGame->FlushCommandQueue();
    mGame->GetRitems().clear();
    mGame->GetGameObjects().clear();
}

void MainMenuScene::InitializeUiResources()
{
    auto* device = mGame->GetDevice();
    auto* cmdQueue = mGame->GetCommandQueue();

    if (!mGraphicsMemory)
    {
        mGraphicsMemory = std::make_unique<GraphicsMemory>(device);
    }

    if (!mFontHeap)
    {
        mFontHeap = std::make_unique<DirectX::DescriptorHeap>(
            device,
            D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
            D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
            1);
    }

    if (!mFont || !mSpriteBatch)
    {
        ResourceUploadBatch resourceUpload(device);
        resourceUpload.Begin();

        if (!mFont)
        {
            mFont = std::make_unique<SpriteFont>(
                device,
                resourceUpload,
                L"Textures/chat_korean.spritefont",
                mFontHeap->GetCpuHandle(0),
                mFontHeap->GetGpuHandle(0));
        }

        if (!mSpriteBatch)
        {
            RenderTargetState rtState(DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_D24_UNORM_S8_UINT);
            SpriteBatchPipelineStateDescription pd(rtState);
            mSpriteBatch = std::make_unique<SpriteBatch>(device, resourceUpload, pd);
        }

        auto uploadResourcesFinished = resourceUpload.End(cmdQueue);
        uploadResourcesFinished.wait();
    }
}

bool MainMenuScene::IsLocalPlayerHost() const
{
    return mLobbyState.selfPlayerId != -1 && mLobbyState.selfPlayerId == mLobbyState.hostPlayerId;
}

bool MainMenuScene::UpdateLocalReadyFromSnapshot()
{
    for (const auto& player : mLobbyState.players)
    {
        if (player.connected && player.playerId == mLobbyState.selfPlayerId)
        {
            return true;
        }
    }

    return false;
}

void MainMenuScene::RecalculateCanStart()
{
    bool hasLocalPlayer = false;
    bool localReady = false;
    bool allConnectedReady = true;

    for (const auto& player : mLobbyState.players)
    {
        if (!player.connected)
        {
            continue;
        }

        if (player.playerId == mLobbyState.selfPlayerId)
        {
            hasLocalPlayer = true;
            localReady = player.ready;
        }
        if (!player.ready)
        {
            allConnectedReady = false;
        }
    }

    mLobbyState.canStart = hasLocalPlayer && localReady && allConnectedReady;
}

void MainMenuScene::RefreshLobbyState()
{
    LobbyStateSnapshot networkState = NetworkManager::Get()->GetLobbyState();
    if (networkState.playerCount > 0)
    {
        mLobbyState = networkState;
        mLocalReady = false;
        for (auto& player : mLobbyState.players)
        {
            if (player.connected && player.playerId == mLobbyState.selfPlayerId)
            {
                mLocalReady = player.ready;
                break;
            }
        }
        RecalculateCanStart();
        return;
    }

    mLobbyState = {};
    mLobbyState.selfPlayerId = 1;
    mLobbyState.hostPlayerId = 1;
    mLobbyState.playerCount = 1;
    mLobbyState.players[0].playerId = 1;
    mLobbyState.players[0].connected = true;
    mLobbyState.players[0].ready = mLocalReady;
    mLobbyState.players[0].isHost = true;
    RecalculateCanStart();
}

void MainMenuScene::Update(const GameTimer& gt)
{
    UNREFERENCED_PARAMETER(gt);
    RefreshLobbyState();

    if (NetworkManager::Get()->ConsumeGameStartSignal())
    {
        gLastSceneChangeTime = GetTickCount64();
        mGame->ChangeScene(std::make_unique<CharSelectScene>(mGame));
        return;
    }

    const bool hasFocus = GetForegroundWindow() == mGame->GetMainWindowHandle();
    if (!hasFocus)
    {
        mReadyKeyPressed = false;
        mStartKeyPressed = false;
        if (mGraphicsMemory)
        {
            mGraphicsMemory->Commit(mGame->GetCommandQueue());
        }
        return;
    }

    if (GetAsyncKeyState('R') & 0x8000)
    {
        if (!mReadyKeyPressed)
        {
            mLocalReady = !mLocalReady;
            for (auto& player : mLobbyState.players)
            {
                if (player.connected && player.playerId == mLobbyState.selfPlayerId)
                {
                    player.ready = mLocalReady;
                    break;
                }
            }
            NetworkManager::Get()->SendPlayerReady(mLocalReady);
            RecalculateCanStart();
            mReadyKeyPressed = true;
        }
    }
    else
    {
        mReadyKeyPressed = false;
    }

    if (GetAsyncKeyState(VK_RETURN) & 0x8000)
    {
        if (!mStartKeyPressed && IsLocalPlayerHost() && mLobbyState.canStart)
        {
            if (GetTickCount64() - gLastSceneChangeTime > 300)
            {
                gLastSceneChangeTime = GetTickCount64();
                NetworkManager::Get()->SendGameStart();
            }
            mStartKeyPressed = true;
        }
    }
    else
    {
        mStartKeyPressed = false;
    }

    if (mGraphicsMemory)
    {
        mGraphicsMemory->Commit(mGame->GetCommandQueue());
    }
}

void MainMenuScene::Draw(const GameTimer& gt)
{
    UNREFERENCED_PARAMETER(gt);

    if (!mFont || !mSpriteBatch || !mFontHeap)
    {
        return;
    }

    auto* cmdList = mGame->GetCommandList();
    ID3D12DescriptorHeap* heaps[] = { mFontHeap->Heap() };
    cmdList->SetDescriptorHeaps(1, heaps);

    mSpriteBatch->SetViewport(mGame->GetScreenViewport());
    mSpriteBatch->Begin(cmdList);

    const XMFLOAT2 titlePos(120.0f, 120.0f);
    mFont->DrawString(mSpriteBatch.get(), L"LOBBY", titlePos, Colors::White, 0.0f, XMFLOAT2(0.0f, 0.0f), 1.2f);

    const std::wstring countText = L"Connected : " + std::to_wstring(mLobbyState.playerCount) + L" / " + std::to_wstring(MAX_LOBBY_PLAYERS);
    mFont->DrawString(mSpriteBatch.get(), countText.c_str(), XMFLOAT2(120.0f, 180.0f), Colors::LightGray, 0.0f, XMFLOAT2(0.0f, 0.0f), 0.85f);

    float slotY = 250.0f;
    for (int i = 0; i < MAX_LOBBY_PLAYERS; ++i)
    {
        const auto& player = mLobbyState.players[i];
        if (player.connected)
        {
            std::wstring line = L"Player " + std::to_wstring(i + 1);
            if (player.isHost)
            {
                line += L"  [HOST]";
            }
            if (player.playerId == mLobbyState.selfPlayerId)
            {
                line += L"  [YOU]";
            }

            const wchar_t* stateText = player.ready ? L"READY" : L"WAIT";
            XMVECTORF32 stateColor = player.ready ? Colors::LimeGreen : Colors::Orange;

            mFont->DrawString(mSpriteBatch.get(), line.c_str(), XMFLOAT2(120.0f, slotY), Colors::White, 0.0f, XMFLOAT2(0.0f, 0.0f), 0.78f);
            mFont->DrawString(mSpriteBatch.get(), stateText, XMFLOAT2(520.0f, slotY), stateColor, 0.0f, XMFLOAT2(0.0f, 0.0f), 0.78f);
        }
        else
        {
            mFont->DrawString(mSpriteBatch.get(), L"[ Empty Slot ]", XMFLOAT2(120.0f, slotY), Colors::DarkGray, 0.0f, XMFLOAT2(0.0f, 0.0f), 0.78f);
        }

        slotY += 72.0f;
    }

    mFont->DrawString(mSpriteBatch.get(), L"R : Toggle Ready", XMFLOAT2(120.0f, 520.0f), Colors::LightYellow, 0.0f, XMFLOAT2(0.0f, 0.0f), 0.72f);

    if (IsLocalPlayerHost())
    {
        const XMVECTORF32 buttonColor = mLobbyState.canStart ? Colors::Cyan : Colors::Gray;
        mFont->DrawString(mSpriteBatch.get(), L"[ GAME START ]", XMFLOAT2(120.0f, 590.0f), buttonColor, 0.0f, XMFLOAT2(0.0f, 0.0f), 0.95f);

        const wchar_t* helpText = mLobbyState.canStart
            ? L"Press Enter to start"
            : L"Waiting for every player to ready.";
        const XMVECTORF32 helpColor = mLobbyState.canStart ? Colors::White : Colors::LightGray;
        mFont->DrawString(mSpriteBatch.get(), helpText, XMFLOAT2(120.0f, 635.0f), helpColor, 0.0f, XMFLOAT2(0.0f, 0.0f), 0.68f);
    }
    else
    {
        mFont->DrawString(mSpriteBatch.get(), L"Waiting for host to start", XMFLOAT2(120.0f, 590.0f), Colors::LightGray, 0.0f, XMFLOAT2(0.0f, 0.0f), 0.72f);
    }

    if (mLobbyState.playerCount == 0)
    {
        mFont->DrawString(mSpriteBatch.get(), L"Waiting for lobby sync...", XMFLOAT2(120.0f, 680.0f), Colors::Gray, 0.0f, XMFLOAT2(0.0f, 0.0f), 0.66f);
    }

    mSpriteBatch->End();
}
