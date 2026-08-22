#include "RoomSelectScene.h"
#include "EclipseWalkerGame.h"
#include "GameObject.h"
#include "MainMenuScene.h"
#include <RenderTargetState.h>
#include <ResourceUploadBatch.h>
#include <Windows.h>
#include <algorithm>

using namespace DirectX;

namespace
{
    constexpr float kUiBaseWidth = 1280.0f;
    constexpr float kUiBaseHeight = 720.0f;
    constexpr float kListLeft = 120.0f;
    constexpr float kListTop = 185.0f;
    constexpr float kListRight = 900.0f;
    constexpr float kRowHeight = 48.0f;
    constexpr int kVisibleRoomCount = 8;
    constexpr float kCreateLeft = 940.0f;
    constexpr float kCreateTop = 555.0f;
    constexpr float kCreateRight = 1165.0f;
    constexpr float kCreateBottom = 595.0f;
    constexpr float kRefreshLeft = 120.0f;
    constexpr float kRefreshTop = 610.0f;
    constexpr float kRefreshRight = 270.0f;
    constexpr float kRefreshBottom = 650.0f;
    constexpr float kTitleLabelX = 940.0f;
    constexpr float kTitleLabelY = 455.0f;
    constexpr float kTitleInputX = 940.0f;
    constexpr float kTitleInputY = 492.0f;

    std::wstring Utf8ToWideRoomSelect(const std::string& text)
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

    XMFLOAT2 GetUiScale(const D3D12_VIEWPORT& viewport)
    {
        return
        {
            viewport.Width / kUiBaseWidth,
            viewport.Height / kUiBaseHeight
        };
    }

    XMFLOAT2 ScaleUiPoint(const D3D12_VIEWPORT& viewport, float x, float y)
    {
        const XMFLOAT2 uiScale = GetUiScale(viewport);
        return { x * uiScale.x, y * uiScale.y };
    }

    float GetUiTextScale(const D3D12_VIEWPORT& viewport)
    {
        const XMFLOAT2 uiScale = GetUiScale(viewport);
        return std::clamp((std::min)(uiScale.x, uiScale.y), 0.85f, 1.65f);
    }

    bool IsInside(float x, float y, float left, float top, float right, float bottom)
    {
        return x >= left && x <= right && y >= top && y <= bottom;
    }

    std::string WideCharToUtf8(wchar_t ch)
    {
        wchar_t source[2] = { ch, L'\0' };
        char buffer[8] = {};
        const int bytes = WideCharToMultiByte(
            CP_UTF8,
            0,
            source,
            1,
            buffer,
            static_cast<int>(sizeof(buffer)),
            nullptr,
            nullptr);
        if (bytes <= 0)
        {
            return "";
        }

        return std::string(buffer, static_cast<size_t>(bytes));
    }

    void PopLastUtf8Character(std::string& text)
    {
        if (text.empty())
        {
            return;
        }

        size_t erasePos = text.size() - 1;
        while (erasePos > 0 && (static_cast<unsigned char>(text[erasePos]) & 0xC0) == 0x80)
        {
            --erasePos;
        }
        text.erase(erasePos);
    }
}

void RoomSelectScene::Enter()
{
    mGame->FlushCommandQueue();

    auto* res = mGame->GetResources();
    auto& ritems = mGame->GetRitems();
    auto& gameObjects = mGame->GetGameObjects();
    ritems.clear();
    gameObjects.clear();

    auto camera = mGame->GetCamera();
    camera->SetPosition(0.0f, 0.0f, -10.0f);
    camera->LookAt(
        XMFLOAT3(0.0f, 0.0f, -10.0f),
        XMFLOAT3(0.0f, 0.0f, 0.0f),
        XMFLOAT3(0.0f, 1.0f, 0.0f));
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
    RefreshRoomList();
}

void RoomSelectScene::Exit()
{
    mGame->FlushCommandQueue();
    mGame->UnloadSharedGameResources();
    mGame->GetRitems().clear();
    mGame->GetGameObjects().clear();
}

void RoomSelectScene::InitializeUiResources()
{
    auto* device = mGame->GetDevice();
    auto* cmdQueue = mGame->GetCommandQueue();

    if (!mGraphicsMemory)
    {
        mGraphicsMemory = std::make_unique<GraphicsMemory>(device);
    }

    if (!mFontHeap)
    {
        mFontHeap = std::make_unique<DescriptorHeap>(
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

void RoomSelectScene::RefreshRoomList()
{
    if (NetworkManager::Get()->IsConnected() && NetworkManager::Get()->m_myPlayerId > 0)
    {
        NetworkManager::Get()->SendRoomListRequest();
        mStatusText = "Refreshing rooms...";
        return;
    }

    mStatusText = "Logging in...";
}

void RoomSelectScene::TryJoinSelectedRoom()
{
    if (NetworkManager::Get()->m_myPlayerId <= 0)
    {
        mStatusText = "Logging in...";
        return;
    }

    mRooms = NetworkManager::Get()->GetRoomListSnapshot();
    if (mRooms.empty())
    {
        mStatusText = "No room selected";
        return;
    }

    mSelectedRoomIndex = std::clamp(mSelectedRoomIndex, 0, static_cast<int>(mRooms.size()) - 1);
    const RoomListItem& room = mRooms[static_cast<size_t>(mSelectedRoomIndex)];
    if (room.inGame || room.playerCount >= room.maxPlayers)
    {
        mStatusText = "Room is not joinable";
        return;
    }

    NetworkManager::Get()->SendJoinRoom(room.roomId);
    mStatusText = "Joining room...";
}

void RoomSelectScene::TryCreateRoom()
{
    if (NetworkManager::Get()->m_myPlayerId <= 0)
    {
        mStatusText = "Logging in...";
        return;
    }

    std::string title = mRoomTitle;
    if (title.empty())
    {
        title = "New Room";
    }

    NetworkManager::Get()->SendCreateRoom(title);
    mStatusText = "Creating room...";
}

void RoomSelectScene::HandleMouseClick(float baseX, float baseY)
{
    if (IsInside(baseX, baseY, kRefreshLeft, kRefreshTop, kRefreshRight, kRefreshBottom))
    {
        RefreshRoomList();
        return;
    }

    if (IsInside(baseX, baseY, kCreateLeft, kCreateTop, kCreateRight, kCreateBottom))
    {
        TryCreateRoom();
        return;
    }

    if (IsInside(baseX, baseY, kListLeft, kListTop, kListRight, kListTop + kRowHeight * kVisibleRoomCount))
    {
        const int row = static_cast<int>((baseY - kListTop) / kRowHeight);
        if (row >= 0 && row < static_cast<int>(mRooms.size()))
        {
            mSelectedRoomIndex = row;
            TryJoinSelectedRoom();
        }
    }
}

void RoomSelectScene::Update(const GameTimer& gt)
{
    mRooms = NetworkManager::Get()->GetRoomListSnapshot();
    if (!mRooms.empty())
    {
        mSelectedRoomIndex = std::clamp(mSelectedRoomIndex, 0, static_cast<int>(mRooms.size()) - 1);
    }
    else
    {
        mSelectedRoomIndex = 0;
    }

    const int createResult = NetworkManager::Get()->ConsumeCreateRoomResult();
    if (createResult < 0)
    {
        mStatusText = "Create room failed";
    }

    const int joinResult = NetworkManager::Get()->ConsumeJoinRoomResult();
    if (joinResult > 0)
    {
        mGame->ChangeScene(std::make_unique<MainMenuScene>(mGame));
        return;
    }
    if (joinResult < 0)
    {
        mStatusText = "Join room failed";
        RefreshRoomList();
    }

    mRefreshTimer += gt.DeltaTime();
    if (mRefreshTimer >= 2.0f)
    {
        mRefreshTimer = 0.0f;
        RefreshRoomList();
    }

    const bool hasFocus = GetForegroundWindow() == mGame->GetMainWindowHandle();
    if (hasFocus)
    {
        if (GetAsyncKeyState(VK_UP) & 0x8000)
        {
            if (!mUpPressed && !mRooms.empty())
            {
                mSelectedRoomIndex = (std::max)(0, mSelectedRoomIndex - 1);
                mUpPressed = true;
            }
        }
        else
        {
            mUpPressed = false;
        }

        if (GetAsyncKeyState(VK_DOWN) & 0x8000)
        {
            if (!mDownPressed && !mRooms.empty())
            {
                mSelectedRoomIndex = (std::min)(static_cast<int>(mRooms.size()) - 1, mSelectedRoomIndex + 1);
                mDownPressed = true;
            }
        }
        else
        {
            mDownPressed = false;
        }

        if (GetAsyncKeyState(VK_RETURN) & 0x8000)
        {
            if (!mEnterPressed)
            {
                TryJoinSelectedRoom();
                mEnterPressed = true;
            }
        }
        else
        {
            mEnterPressed = false;
        }

        if (GetAsyncKeyState(VK_F5) & 0x8000)
        {
            if (!mRefreshPressed)
            {
                RefreshRoomList();
                mRefreshPressed = true;
            }
        }
        else
        {
            mRefreshPressed = false;
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
    }

    if (mGraphicsMemory)
    {
        mGraphicsMemory->Commit(mGame->GetCommandQueue());
    }
}

void RoomSelectScene::Draw(const GameTimer& gt)
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

    mFont->DrawString(mSpriteBatch.get(), L"ROOM SELECT", scalePoint(120.0f, 95.0f), Colors::White, 0.0f, XMFLOAT2(0.0f, 0.0f), 1.15f * textScale);
    mFont->DrawString(mSpriteBatch.get(), L"ROOMS", scalePoint(kListLeft, 160.0f), Colors::LightYellow, 0.0f, XMFLOAT2(0.0f, 0.0f), 0.72f * textScale);

    mFont->DrawString(mSpriteBatch.get(), L"Room Title", scalePoint(kTitleLabelX, kTitleLabelY), Colors::LightGray, 0.0f, XMFLOAT2(0.0f, 0.0f), 0.68f * textScale);
    const std::wstring titleText = mRoomTitle.empty()
        ? L"[ Room name ]"
        : L"[ " + Utf8ToWideRoomSelect(mRoomTitle) + L" ]";
    mFont->DrawString(mSpriteBatch.get(), titleText.c_str(), scalePoint(kTitleInputX, kTitleInputY), Colors::White, 0.0f, XMFLOAT2(0.0f, 0.0f), 0.72f * textScale);

    for (int i = 0; i < kVisibleRoomCount; ++i)
    {
        const float rowY = kListTop + kRowHeight * i;
        if (i >= static_cast<int>(mRooms.size()))
        {
            mFont->DrawString(mSpriteBatch.get(), L"[ Empty ]", scalePoint(kListLeft, rowY), Colors::DarkGray, 0.0f, XMFLOAT2(0.0f, 0.0f), 0.68f * textScale);
            continue;
        }

        const RoomListItem& room = mRooms[static_cast<size_t>(i)];
        std::wstring rowText = (i == mSelectedRoomIndex) ? L"> " : L"  ";
        rowText += Utf8ToWideRoomSelect(room.title);
        if (rowText.size() < 24)
        {
            rowText.append(24 - rowText.size(), L' ');
        }
        rowText += L"  " + std::to_wstring(room.playerCount) + L" / " + std::to_wstring(room.maxPlayers);
        if (room.inGame)
        {
            rowText += L"  IN GAME";
        }

        const XMVECTORF32 color = room.inGame ? Colors::Gray : (i == mSelectedRoomIndex ? Colors::Cyan : Colors::White);
        mFont->DrawString(mSpriteBatch.get(), rowText.c_str(), scalePoint(kListLeft, rowY), color, 0.0f, XMFLOAT2(0.0f, 0.0f), 0.72f * textScale);
    }

    mFont->DrawString(mSpriteBatch.get(), L"[ REFRESH ]", scalePoint(kRefreshLeft, kRefreshTop), Colors::LightSkyBlue, 0.0f, XMFLOAT2(0.0f, 0.0f), 0.78f * textScale);
    mFont->DrawString(mSpriteBatch.get(), L"[ CREATE ROOM ]", scalePoint(kCreateLeft, kCreateTop), Colors::LimeGreen, 0.0f, XMFLOAT2(0.0f, 0.0f), 0.78f * textScale);

    if (!mStatusText.empty())
    {
        mFont->DrawString(mSpriteBatch.get(), Utf8ToWideRoomSelect(mStatusText).c_str(), scalePoint(120.0f, 650.0f), Colors::LightGray, 0.0f, XMFLOAT2(0.0f, 0.0f), 0.62f * textScale);
    }

    mSpriteBatch->End();
}

void RoomSelectScene::OnCharInput(WPARAM wParam)
{
    if (wParam == VK_BACK)
    {
        if (!mRoomTitle.empty())
        {
            PopLastUtf8Character(mRoomTitle);
        }
        return;
    }

    if (wParam == VK_TAB || wParam == VK_RETURN)
    {
        return;
    }

    if (wParam >= 32)
    {
        const std::string utf8Char = WideCharToUtf8(static_cast<wchar_t>(wParam));
        if (!utf8Char.empty() && mRoomTitle.size() + utf8Char.size() < MAX_ROOM_TITLE)
        {
            mRoomTitle += utf8Char;
        }
    }
}
