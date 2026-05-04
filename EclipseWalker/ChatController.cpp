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

void ChatController::Draw(bool showDoorPrompt)
{
    if (!mFont || !mSpriteBatch || !mFontHeap)
    {
        return;
    }

    auto* cmdList = mGame->GetCommandList();
    ID3D12DescriptorHeap* heaps[] = { mFontHeap->Heap() };
    cmdList->SetDescriptorHeaps(1, heaps);

    mSpriteBatch->SetViewport(mGame->GetScreenViewport());
    mSpriteBatch->Begin(cmdList);

    const float startX = 28.0f;
    float startY = 510.0f;
    constexpr float chatTextScale = 0.72f;

    for (const auto& line : mChatLines)
    {
        const DirectX::XMFLOAT2 linePos(startX, startY);
        mFont->DrawString(mSpriteBatch.get(), line.c_str(), DirectX::XMFLOAT2(linePos.x + 1.0f, linePos.y + 1.0f), DirectX::XMVECTORF32{ 0.f, 0.f, 0.f, 0.65f }, 0.0f, DirectX::XMFLOAT2(0.0f, 0.0f), chatTextScale);
        mFont->DrawString(mSpriteBatch.get(), line.c_str(), linePos, DirectX::Colors::White, 0.0f, DirectX::XMFLOAT2(0.0f, 0.0f), chatTextScale);
        startY += 24.0f;
    }

    const std::wstring promptText = mChatInput + mComposingText;
    const std::wstring prompt = mIsChatting ? (L"> " + promptText + L"_") : L"Enter : Chat";
    const DirectX::XMFLOAT2 promptPos(28.0f, 654.0f);
    const DirectX::XMVECTORF32 promptColor = mIsChatting ? DirectX::Colors::Yellow : DirectX::Colors::LightGray;

    mFont->DrawString(mSpriteBatch.get(), prompt.c_str(), DirectX::XMFLOAT2(promptPos.x + 1.0f, promptPos.y + 1.0f), DirectX::XMVECTORF32{ 0.f, 0.f, 0.f, 0.65f }, 0.0f, DirectX::XMFLOAT2(0.0f, 0.0f), chatTextScale);
    mFont->DrawString(mSpriteBatch.get(), prompt.c_str(), promptPos, promptColor, 0.0f, DirectX::XMFLOAT2(0.0f, 0.0f), chatTextScale);

    if (showDoorPrompt && !mIsChatting)
    {
        const wchar_t* doorPrompt = L"[ F ] 문 열기 / 닫기";
        constexpr float doorPromptScale = 0.82f;
        const auto viewport = mGame->GetScreenViewport();
        const DirectX::XMVECTOR textSize = mFont->MeasureString(doorPrompt);
        const DirectX::XMFLOAT2 origin(
            DirectX::XMVectorGetX(textSize) * 0.5f,
            DirectX::XMVectorGetY(textSize) * 0.5f);
        const DirectX::XMFLOAT2 promptCenter(
            viewport.Width * 0.5f,
            viewport.Height * 0.72f);

        mFont->DrawString(
            mSpriteBatch.get(),
            doorPrompt,
            DirectX::XMFLOAT2(promptCenter.x + 2.0f, promptCenter.y + 2.0f),
            DirectX::XMVECTORF32{ 0.0f, 0.0f, 0.0f, 0.75f },
            0.0f,
            origin,
            doorPromptScale);
        mFont->DrawString(
            mSpriteBatch.get(),
            doorPrompt,
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
        const std::wstring line = L"[" + std::to_wstring(message.playerId) + L"] " + Utf8ToWide(message.text);
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
        PushChatLine(L"[나] " + mChatInput);
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
