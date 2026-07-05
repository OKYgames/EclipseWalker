#include "MainMenuScene.h"
#include "EclipseWalkerGame.h"
#include "CharSelectScene.h"
#include "DebugConfig.h"
#include "GameObject.h"
#include "RoomSelectScene.h"
#include <ResourceUploadBatch.h>
#include <RenderTargetState.h>
#include <Windows.h>
#include <algorithm>

using namespace DirectX;

namespace
{
    constexpr float kUiBaseWidth = 1280.0f;
    constexpr float kUiBaseHeight = 720.0f;
    constexpr float kBackLeft = 1030.0f;
    constexpr float kBackTop = 650.0f;
    constexpr float kBackRight = 1165.0f;
    constexpr float kBackBottom = 690.0f;

    std::wstring Utf8ToWideLobby(const std::string& text)
    {
        if (text.empty())
        {
            return L"";
        }

        const int sizeNeeded = MultiByteToWideChar(
            CP_UTF8,
            0,
            text.c_str(),
            static_cast<int>(text.size()),
            nullptr,
            0);
        if (sizeNeeded <= 0)
        {
            return std::wstring(text.begin(), text.end());
        }

        std::wstring result(static_cast<size_t>(sizeNeeded), L'\0');
        MultiByteToWideChar(
            CP_UTF8,
            0,
            text.c_str(),
            static_cast<int>(text.size()),
            result.data(),
            sizeNeeded);
        return result;
    }

    DirectX::XMFLOAT2 GetUiScale(const D3D12_VIEWPORT& viewport)
    {
        const float width = (std::max)(1.0f, viewport.Width);
        const float height = (std::max)(1.0f, viewport.Height);
        return { width / kUiBaseWidth, height / kUiBaseHeight };
    }

    DirectX::XMFLOAT2 ScaleUiPoint(const D3D12_VIEWPORT& viewport, float x, float y)
    {
        const DirectX::XMFLOAT2 uiScale = GetUiScale(viewport);
        return { x * uiScale.x, y * uiScale.y };
    }

    float GetUiTextScale(const D3D12_VIEWPORT& viewport, float minScale = 0.85f, float maxScale = 1.65f)
    {
        const DirectX::XMFLOAT2 uiScale = GetUiScale(viewport);
        return std::clamp((std::min)(uiScale.x, uiScale.y), minScale, maxScale);
    }

    bool IsInside(float x, float y, float left, float top, float right, float bottom)
    {
        return x >= left && x <= right && y >= top && y <= bottom;
    }
}

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
    mBackKeyPressed = false;
    mMousePressed = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    mLeavingRoom = false;
    RefreshLobbyState();
}

void MainMenuScene::Exit()
{
    mGame->FlushCommandQueue();
    mGame->UnloadSharedGameResources();
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
    int connectedCount = 0;

    for (const auto& player : mLobbyState.players)
    {
        if (!player.connected)
        {
            continue;
        }

        ++connectedCount;
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

    mLobbyState.canStart = IsLocalPlayerHost()
        && hasLocalPlayer
        && localReady
        && connectedCount == MAX_LOBBY_PLAYERS
        && allConnectedReady;
}

void MainMenuScene::RefreshLobbyState()
{
    if (DebugConfig::kEnableBackendConnection)
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
    }

    mLobbyState = {};
    if (DebugConfig::kEnableBackendConnection)
    {
        mLocalReady = false;
        return;
    }

    mLobbyState.selfPlayerId = 1;
    mLobbyState.hostPlayerId = 1;
    mLobbyState.playerCount = 1;
    if (DebugConfig::kAllowSoloLobbyStart)
    {
        mLocalReady = true;
    }
    mLobbyState.players[0].playerId = 1;
    mLobbyState.players[0].connected = true;
    mLobbyState.players[0].ready = mLocalReady;
    mLobbyState.players[0].isHost = true;
    RecalculateCanStart();
    if (DebugConfig::kAllowSoloLobbyStart)
    {
        mLobbyState.canStart = true;
    }
}

void MainMenuScene::RequestLeaveRoom()
{
    if (mLeavingRoom)
    {
        return;
    }

    if (DebugConfig::kEnableBackendConnection && NetworkManager::Get()->IsConnected())
    {
        mLeavingRoom = true;
        NetworkManager::Get()->SendLeaveRoom();
        return;
    }

    mGame->ChangeScene(std::make_unique<RoomSelectScene>(mGame));
}

void MainMenuScene::HandleMouseClick(float baseX, float baseY)
{
    if (IsInside(baseX, baseY, kBackLeft, kBackTop, kBackRight, kBackBottom))
    {
        RequestLeaveRoom();
    }
}

void MainMenuScene::Update(const GameTimer& gt)
{
    UNREFERENCED_PARAMETER(gt);
    RefreshLobbyState();

    const int leaveResult = NetworkManager::Get()->ConsumeLeaveRoomResult();
    if (leaveResult > 0)
    {
        mGame->ChangeScene(std::make_unique<RoomSelectScene>(mGame));
        return;
    }
    if (leaveResult < 0)
    {
        mLeavingRoom = false;
    }

    if (DebugConfig::kEnableBackendConnection && NetworkManager::Get()->ConsumeGameStartSignal())
    {
        gLastSceneChangeTime = GetTickCount64();
        mGame->RequestSceneChange(std::make_unique<CharSelectScene>(mGame), L"LOADING CHARACTER SELECT");
        return;
    }

    const bool hasFocus = GetForegroundWindow() == mGame->GetMainWindowHandle();
    if (!hasFocus)
    {
        mReadyKeyPressed = false;
        mStartKeyPressed = false;
        mBackKeyPressed = false;
        mMousePressed = false;
        if (mGraphicsMemory)
        {
            mGraphicsMemory->Commit(mGame->GetCommandQueue());
        }
        return;
    }

    if (GetAsyncKeyState(VK_ESCAPE) & 0x8000)
    {
        if (!mBackKeyPressed)
        {
            RequestLeaveRoom();
            mBackKeyPressed = true;
        }
    }
    else
    {
        mBackKeyPressed = false;
    }

    const bool leftMouseDown = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    if (leftMouseDown && !mMousePressed)
    {
        POINT cursor = {};
        if (GetCursorPos(&cursor) && ScreenToClient(mGame->GetMainWindowHandle(), &cursor))
        {
            const auto viewport = mGame->GetScreenViewport();
            const float baseX = cursor.x * kUiBaseWidth / (std::max)(1.0f, viewport.Width);
            const float baseY = cursor.y * kUiBaseHeight / (std::max)(1.0f, viewport.Height);
            HandleMouseClick(baseX, baseY);
        }
    }
    mMousePressed = leftMouseDown;

    if (mLeavingRoom)
    {
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
            if (DebugConfig::kEnableBackendConnection)
            {
                NetworkManager::Get()->SendPlayerReady(mLocalReady);
            }
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
                if (DebugConfig::kEnableBackendConnection)
                {
                    NetworkManager::Get()->SendGameStart();
                }
                else
                {
                    mGame->RequestSceneChange(std::make_unique<CharSelectScene>(mGame), L"LOADING CHARACTER SELECT");
                    return;
                }
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

    const auto viewport = mGame->GetScreenViewport();
    mSpriteBatch->SetViewport(viewport);
    mSpriteBatch->Begin(cmdList);

    const float textScale = GetUiTextScale(viewport);
    const auto scalePoint = [&](float x, float y)
    {
        return ScaleUiPoint(viewport, x, y);
    };

    const XMFLOAT2 titlePos = scalePoint(120.0f, 120.0f);
    mFont->DrawString(mSpriteBatch.get(), L"LOBBY", titlePos, Colors::White, 0.0f, XMFLOAT2(0.0f, 0.0f), 1.2f * textScale);

    std::wstring roomTitle = Utf8ToWideLobby(mLobbyState.roomTitle);
    if (roomTitle.empty())
    {
        roomTitle = L"Room";
    }
    mFont->DrawString(mSpriteBatch.get(), roomTitle.c_str(), scalePoint(120.0f, 165.0f), Colors::LightYellow, 0.0f, XMFLOAT2(0.0f, 0.0f), 0.72f * textScale);

    const std::wstring countText = L"Connected : " + std::to_wstring(mLobbyState.playerCount) + L" / " + std::to_wstring(MAX_LOBBY_PLAYERS);
    mFont->DrawString(mSpriteBatch.get(), countText.c_str(), scalePoint(120.0f, 205.0f), Colors::LightGray, 0.0f, XMFLOAT2(0.0f, 0.0f), 0.85f * textScale);

    float slotY = scalePoint(120.0f, 270.0f).y;
    const float slotLineX = scalePoint(120.0f, 0.0f).x;
    const float slotStateX = scalePoint(520.0f, 0.0f).x;
    const float slotStepY = viewport.Height * (72.0f / kUiBaseHeight);
    for (int i = 0; i < MAX_LOBBY_PLAYERS; ++i)
    {
        const auto& player = mLobbyState.players[i];
        if (player.connected)
        {
            std::wstring line = Utf8ToWideLobby(player.displayName);
            if (line.empty())
            {
                line = L"Player " + std::to_wstring(i + 1);
            }
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

            mFont->DrawString(mSpriteBatch.get(), line.c_str(), XMFLOAT2(slotLineX, slotY), Colors::White, 0.0f, XMFLOAT2(0.0f, 0.0f), 0.78f * textScale);
            mFont->DrawString(mSpriteBatch.get(), stateText, XMFLOAT2(slotStateX, slotY), stateColor, 0.0f, XMFLOAT2(0.0f, 0.0f), 0.78f * textScale);
        }
        else
        {
            mFont->DrawString(mSpriteBatch.get(), L"[ Empty Slot ]", XMFLOAT2(slotLineX, slotY), Colors::DarkGray, 0.0f, XMFLOAT2(0.0f, 0.0f), 0.78f * textScale);
        }

        slotY += slotStepY;
    }

    mFont->DrawString(mSpriteBatch.get(), L"R : Toggle Ready", scalePoint(120.0f, 520.0f), Colors::LightYellow, 0.0f, XMFLOAT2(0.0f, 0.0f), 0.72f * textScale);

    if (IsLocalPlayerHost())
    {
        const XMVECTORF32 buttonColor = mLobbyState.canStart ? Colors::Cyan : Colors::Gray;
        mFont->DrawString(mSpriteBatch.get(), L"[ GAME START ]", scalePoint(120.0f, 590.0f), buttonColor, 0.0f, XMFLOAT2(0.0f, 0.0f), 0.95f * textScale);

        const wchar_t* helpText = mLobbyState.canStart
            ? L"Press Enter to start"
            : L"Waiting for 3 players and every ready.";
        const XMVECTORF32 helpColor = mLobbyState.canStart ? Colors::White : Colors::LightGray;
        mFont->DrawString(mSpriteBatch.get(), helpText, scalePoint(120.0f, 635.0f), helpColor, 0.0f, XMFLOAT2(0.0f, 0.0f), 0.68f * textScale);
    }
    else
    {
        mFont->DrawString(mSpriteBatch.get(), L"Waiting for host to start", scalePoint(120.0f, 590.0f), Colors::LightGray, 0.0f, XMFLOAT2(0.0f, 0.0f), 0.72f * textScale);
    }

    if (mLobbyState.playerCount == 0)
    {
        mFont->DrawString(mSpriteBatch.get(), L"Waiting for lobby sync...", scalePoint(120.0f, 680.0f), Colors::Gray, 0.0f, XMFLOAT2(0.0f, 0.0f), 0.66f * textScale);
    }

    const wchar_t* backText = mLeavingRoom ? L"LEAVING..." : L"[ BACK ]";
    const XMVECTORF32 backColor = mLeavingRoom ? Colors::Gray : Colors::LightSkyBlue;
    mFont->DrawString(mSpriteBatch.get(), backText, scalePoint(kBackLeft, kBackTop), backColor, 0.0f, XMFLOAT2(0.0f, 0.0f), 0.82f * textScale);

    mSpriteBatch->End();
}
