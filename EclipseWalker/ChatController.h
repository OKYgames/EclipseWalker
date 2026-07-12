#pragma once

#include "GameTimer.h"
#include <array>
#include <deque>
#include <memory>
#include <string>
#include <SpriteBatch.h>
#include <SpriteFont.h>
#include <GraphicsMemory.h>
#include <DescriptorHeap.h>

class EclipseWalkerGame;

class ChatController
{
public:
    explicit ChatController(EclipseWalkerGame* game);

    void Initialize();
    void Reset();
    void Update(const GameTimer& gt);
    void UpdateMessagesOnly();
    void Draw(bool showDoorPrompt = false, bool showSkullPrompt = false);
    void Draw(bool showDoorPrompt, bool showSkullPrompt, const wchar_t* customInteractionPrompt);

    void OnCharInput(WPARAM charCode);
    void OnTextInput(const std::wstring& text);
    void OnCompositionInput(const std::wstring& text, bool isFinal);

    bool IsChatting() const { return mIsChatting; }
    bool HasMessages() const { return !mChatLines.empty(); }

private:
    void PollChatMessages();
    void PollChatKeyboardInput();
    void PushChatLine(const std::wstring& line);
    void BeginChatInput();
    void EndChatInput(bool sendMessage);
    void CommitComposingText();

private:
    EclipseWalkerGame* mGame = nullptr;

    bool mIsChatting = false;
    bool mEscKeyPressed = false;
    bool mEnterKeyPressed = false;

    std::wstring mChatInput;
    std::wstring mComposingText;
    std::wstring mLastCommittedComposition;
    std::array<bool, 256> mChatKeyPressed{};
    std::deque<std::wstring> mChatLines;

    std::unique_ptr<DirectX::GraphicsMemory> mGraphicsMemory;
    std::unique_ptr<DirectX::DescriptorHeap> mFontHeap;
    std::unique_ptr<DirectX::SpriteBatch> mSpriteBatch;
    std::unique_ptr<DirectX::SpriteFont> mFont;
};
