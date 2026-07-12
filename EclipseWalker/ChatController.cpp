#include "ChatController.h"

#include "EclipseWalkerGame.h"
#include "NetworkManager.h"
#include "Scene.h"
#include <ResourceUploadBatch.h>
#include <RenderTargetState.h>
#include <Windows.h>
#include <algorithm>

namespace
{
    constexpr float kUiBaseWidth = 1280.0f;
    constexpr float kUiBaseHeight = 720.0f;

    std::string WideToUtf8(const std::wstring& text)
    {
        if (text.empty())
        {
            return {};
        }

        int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
        std::string result(sizeNeeded, '\0');
        WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), result.data(), sizeNeeded, nullptr, nullptr);
        return result;
    }

    std::wstring Utf8ToWide(const std::string& text)
    {
        if (text.empty())
        {
            return {};
        }

        int sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0);
        std::wstring result(sizeNeeded, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), result.data(), sizeNeeded);
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
}

ChatController::ChatController(EclipseWalkerGame* game)
    : mGame(game)
{
}

void ChatController::Initialize()
{
    auto* device = mGame->GetDevice();
    auto* cmdQueue = mGame->GetCommandQueue();

    if (!mGraphicsMemory)
    {
        mGraphicsMemory = std::make_unique<DirectX::GraphicsMemory>(device);
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
        DirectX::ResourceUploadBatch resourceUpload(device);
        resourceUpload.Begin();

        if (!mFont)
        {
            mFont = std::make_unique<DirectX::SpriteFont>(
                device,
                resourceUpload,
                L"Textures/chat_korean.spritefont",
                mFontHeap->GetCpuHandle(0),
                mFontHeap->GetGpuHandle(0));
        }

        if (!mSpriteBatch)
        {
            DirectX::RenderTargetState rtState(DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_D24_UNORM_S8_UINT);
            DirectX::SpriteBatchPipelineStateDescription pd(rtState);
            mSpriteBatch = std::make_unique<DirectX::SpriteBatch>(device, resourceUpload, pd);
        }

        auto uploadResourcesFinished = resourceUpload.End(cmdQueue);
        uploadResourcesFinished.wait();
    }

    Reset();
}

void ChatController::Reset()
{
    mIsChatting = false;
    mEscKeyPressed = false;
    mEnterKeyPressed = false;
    mChatInput.clear();
    mComposingText.clear();
    mLastCommittedComposition.clear();
    mChatLines.clear();
    mChatKeyPressed.fill(false);
    gIsChatInputActive = false;
}

void ChatController::Update(const GameTimer& gt)
{
    UNREFERENCED_PARAMETER(gt);

    PollChatMessages();

    if (mGraphicsMemory)
    {
        mGraphicsMemory->Commit(mGame->GetCommandQueue());
    }

    const bool hasFocus = (mGame != nullptr && GetForegroundWindow() == mGame->GetMainWindowHandle());
    if (!hasFocus)
    {
        mEscKeyPressed = false;
        mEnterKeyPressed = false;
        mChatKeyPressed.fill(false);
        return;
    }

    if (mIsChatting)
    {
        PollChatKeyboardInput();
    }

    if (GetAsyncKeyState(VK_ESCAPE) & 0x8000)
    {
        if (mIsChatting && !mEscKeyPressed)
        {
            EndChatInput(false);
            mEscKeyPressed = true;
        }
    }
    else
    {
        mEscKeyPressed = false;
    }

    if (GetAsyncKeyState(VK_RETURN) & 0x8000)
    {
        if (!mEnterKeyPressed)
        {
            if (!mIsChatting)
            {
                BeginChatInput();
            }
            else
            {
                EndChatInput(true);
            }
            mEnterKeyPressed = true;
        }
    }
    else
    {
        mEnterKeyPressed = false;
    }
}

void ChatController::UpdateMessagesOnly()
{
    PollChatMessages();

    if (mGraphicsMemory)
    {
        mGraphicsMemory->Commit(mGame->GetCommandQueue());
    }

    mEscKeyPressed = false;
    mEnterKeyPressed = false;
    mChatKeyPressed.fill(false);
}

void ChatController::Draw(bool showDoorPrompt, bool showSkullPrompt)
{
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

    const DirectX::XMFLOAT2 startPos = ScaleUiPoint(viewport, 12.0f, 580.0f);
    const DirectX::XMFLOAT2 promptPos = ScaleUiPoint(viewport, 12.0f, 678.0f);
    const DirectX::XMFLOAT2 shadowOffset = ScaleUiPoint(viewport, 1.0f, 1.0f);
    const float chatTextScale = 0.48f * GetUiTextScale(viewport);
    const float lineStep = 17.0f * (viewport.Height / kUiBaseHeight);
    const float startX = startPos.x;
    float startY = startPos.y;

    for (const auto& line : mChatLines)
    {
        const DirectX::XMFLOAT2 linePos(startX, startY);
        mFont->DrawString(mSpriteBatch.get(), line.c_str(), DirectX::XMFLOAT2(linePos.x + shadowOffset.x, linePos.y + shadowOffset.y), DirectX::XMVECTORF32{ 0.f, 0.f, 0.f, 0.65f }, 0.0f, DirectX::XMFLOAT2(0.0f, 0.0f), chatTextScale);
        mFont->DrawString(mSpriteBatch.get(), line.c_str(), linePos, DirectX::Colors::White, 0.0f, DirectX::XMFLOAT2(0.0f, 0.0f), chatTextScale);
        startY += lineStep;
    }

    const std::wstring promptText = mChatInput + mComposingText;
    const std::wstring prompt = mIsChatting ? (L"> " + promptText + L"_") : L"Enter : Chat";
    const DirectX::XMVECTORF32 promptColor = mIsChatting ? DirectX::Colors::Yellow : DirectX::Colors::LightGray;

    mFont->DrawString(mSpriteBatch.get(), prompt.c_str(), DirectX::XMFLOAT2(promptPos.x + shadowOffset.x, promptPos.y + shadowOffset.y), DirectX::XMVECTORF32{ 0.f, 0.f, 0.f, 0.65f }, 0.0f, DirectX::XMFLOAT2(0.0f, 0.0f), chatTextScale);
    mFont->DrawString(mSpriteBatch.get(), prompt.c_str(), promptPos, promptColor, 0.0f, DirectX::XMFLOAT2(0.0f, 0.0f), chatTextScale);

    if ((showDoorPrompt || showSkullPrompt) && !mIsChatting)
    {
        const wchar_t* interactionPrompt = showSkullPrompt ? L"[ F ] 해골 조사하기" : L"[ F ] 문 열기 / 닫기";
        const float doorPromptScale = 0.82f * GetUiTextScale(viewport);
        const DirectX::XMVECTOR textSize = mFont->MeasureString(interactionPrompt);
        const DirectX::XMFLOAT2 origin(
            DirectX::XMVectorGetX(textSize) * 0.5f,
            DirectX::XMVectorGetY(textSize) * 0.5f);
        const DirectX::XMFLOAT2 promptCenter(
            viewport.Width * 0.5f,
            viewport.Height * 0.72f);

        mFont->DrawString(
            mSpriteBatch.get(),
            interactionPrompt,
            DirectX::XMFLOAT2(promptCenter.x + shadowOffset.x * 2.0f, promptCenter.y + shadowOffset.y * 2.0f),
            DirectX::XMVECTORF32{ 0.0f, 0.0f, 0.0f, 0.75f },
            0.0f,
            origin,
            doorPromptScale);
        mFont->DrawString(
            mSpriteBatch.get(),
            interactionPrompt,
            promptCenter,
            DirectX::Colors::LightYellow,
            0.0f,
            origin,
            doorPromptScale);
    }

    mSpriteBatch->End();
}

void ChatController::Draw(bool showDoorPrompt, bool showSkullPrompt, const wchar_t* customInteractionPrompt)
{
    if (customInteractionPrompt == nullptr)
    {
        Draw(showDoorPrompt, showSkullPrompt);
        return;
    }

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

    const DirectX::XMFLOAT2 startPos = ScaleUiPoint(viewport, 12.0f, 580.0f);
    const DirectX::XMFLOAT2 promptPos = ScaleUiPoint(viewport, 12.0f, 678.0f);
    const DirectX::XMFLOAT2 shadowOffset = ScaleUiPoint(viewport, 1.0f, 1.0f);
    const float chatTextScale = 0.48f * GetUiTextScale(viewport);
    const float lineStep = 17.0f * (viewport.Height / kUiBaseHeight);
    const float startX = startPos.x;
    float startY = startPos.y;

    for (const auto& line : mChatLines)
    {
        const DirectX::XMFLOAT2 linePos(startX, startY);
        mFont->DrawString(mSpriteBatch.get(), line.c_str(), DirectX::XMFLOAT2(linePos.x + shadowOffset.x, linePos.y + shadowOffset.y), DirectX::XMVECTORF32{ 0.f, 0.f, 0.f, 0.65f }, 0.0f, DirectX::XMFLOAT2(0.0f, 0.0f), chatTextScale);
        mFont->DrawString(mSpriteBatch.get(), line.c_str(), linePos, DirectX::Colors::White, 0.0f, DirectX::XMFLOAT2(0.0f, 0.0f), chatTextScale);
        startY += lineStep;
    }

    const std::wstring promptText = mChatInput + mComposingText;
    const std::wstring prompt = mIsChatting ? (L"> " + promptText + L"_") : L"Enter : Chat";
    const DirectX::XMVECTORF32 promptColor = mIsChatting ? DirectX::Colors::Yellow : DirectX::Colors::LightGray;

    mFont->DrawString(mSpriteBatch.get(), prompt.c_str(), DirectX::XMFLOAT2(promptPos.x + shadowOffset.x, promptPos.y + shadowOffset.y), DirectX::XMVECTORF32{ 0.f, 0.f, 0.f, 0.65f }, 0.0f, DirectX::XMFLOAT2(0.0f, 0.0f), chatTextScale);
    mFont->DrawString(mSpriteBatch.get(), prompt.c_str(), promptPos, promptColor, 0.0f, DirectX::XMFLOAT2(0.0f, 0.0f), chatTextScale);

    if ((showDoorPrompt || showSkullPrompt) && !mIsChatting)
    {
        const float doorPromptScale = 0.82f * GetUiTextScale(viewport);
        const DirectX::XMVECTOR textSize = mFont->MeasureString(customInteractionPrompt);
        const DirectX::XMFLOAT2 origin(
            DirectX::XMVectorGetX(textSize) * 0.5f,
            DirectX::XMVectorGetY(textSize) * 0.5f);
        const DirectX::XMFLOAT2 promptCenter(
            viewport.Width * 0.5f,
            viewport.Height * 0.72f);

        mFont->DrawString(
            mSpriteBatch.get(),
            customInteractionPrompt,
            DirectX::XMFLOAT2(promptCenter.x + shadowOffset.x * 2.0f, promptCenter.y + shadowOffset.y * 2.0f),
            DirectX::XMVECTORF32{ 0.0f, 0.0f, 0.0f, 0.75f },
            0.0f,
            origin,
            doorPromptScale);
        mFont->DrawString(
            mSpriteBatch.get(),
            customInteractionPrompt,
            promptCenter,
            DirectX::Colors::LightYellow,
            0.0f,
            origin,
            doorPromptScale);
    }

    mSpriteBatch->End();
}

void ChatController::OnCharInput(WPARAM charCode)
{
    if (charCode == VK_RETURN)
    {
        if (!mEnterKeyPressed)
        {
            if (!mIsChatting)
            {
                BeginChatInput();
            }
            else
            {
                EndChatInput(true);
            }
            mEnterKeyPressed = true;
        }
        return;
    }

    if (!mIsChatting)
    {
        return;
    }

    if (charCode == VK_BACK)
    {
        if (!mComposingText.empty())
        {
            mComposingText.pop_back();
        }
        else if (!mChatInput.empty())
        {
            mChatInput.pop_back();
        }
        return;
    }

    if (charCode == VK_SPACE)
    {
        CommitComposingText();
        OnTextInput(L" ");
    }
}

void ChatController::OnTextInput(const std::wstring& text)
{
    if (!mIsChatting || text.empty())
    {
        return;
    }

    const size_t maxLength = 48;
    const size_t currentLength = std::min(mChatInput.size(), maxLength);
    const size_t appendCount = std::min(text.size(), maxLength - currentLength);
    if (appendCount == 0)
    {
        return;
    }

    mChatInput.append(text.substr(0, appendCount));
}

void ChatController::OnCompositionInput(const std::wstring& text, bool isFinal)
{
    if (!mIsChatting)
    {
        return;
    }

    if (isFinal)
    {
        if (!mLastCommittedComposition.empty() && text == mLastCommittedComposition)
        {
            mLastCommittedComposition.clear();
            mComposingText.clear();
            return;
        }

        mComposingText.clear();
        OnTextInput(text);
        return;
    }

    mComposingText = text;
}

void ChatController::PollChatMessages()
{
    auto messages = NetworkManager::Get()->PopChatMessages();
    for (const auto& message : messages)
    {
        std::wstring senderName = Utf8ToWide(message.senderName);
        if (senderName.empty())
        {
            senderName = L"Player " + std::to_wstring(message.playerId);
        }

        const std::wstring line = L"[" + senderName + L"] " + Utf8ToWide(message.text);
        PushChatLine(line);
    }
}

void ChatController::PollChatKeyboardInput()
{
    auto handleKey = [&](int virtualKey, const std::wstring& text)
    {
        const bool isDown = (GetAsyncKeyState(virtualKey) & 0x8000) != 0;
        if (isDown && !mChatKeyPressed[virtualKey])
        {
            if (virtualKey == VK_SPACE)
            {
                CommitComposingText();
            }
            OnTextInput(text);
        }
        mChatKeyPressed[virtualKey] = isDown;
    };

    handleKey(VK_SPACE, L" ");
}

void ChatController::PushChatLine(const std::wstring& line)
{
    mChatLines.push_back(line);
    while (mChatLines.size() > 5)
    {
        mChatLines.pop_front();
    }
}

void ChatController::BeginChatInput()
{
    mIsChatting = true;
    mChatInput.clear();
    mComposingText.clear();
    mLastCommittedComposition.clear();
    gIsChatInputActive = true;
}

void ChatController::EndChatInput(bool sendMessage)
{
    CommitComposingText();

    if (sendMessage && !mChatInput.empty())
    {
        NetworkManager::Get()->SendChat(WideToUtf8(mChatInput));
        PushChatLine(L"[" + Utf8ToWide(NetworkManager::Get()->GetMyDisplayName()) + L"] " + mChatInput);
    }

    mIsChatting = false;
    mChatInput.clear();
    mComposingText.clear();
    mLastCommittedComposition.clear();
    gIsChatInputActive = false;
}

void ChatController::CommitComposingText()
{
    if (mComposingText.empty())
    {
        return;
    }

    mLastCommittedComposition = mComposingText;
    OnTextInput(mComposingText);
    mComposingText.clear();
}
