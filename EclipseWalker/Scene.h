#pragma once
#include "NetworkManager.h"
#include "GameTimer.h"
#include <string>

class EclipseWalkerGame;

class Scene
{
protected:
    EclipseWalkerGame* mGame; 

public:
    Scene(EclipseWalkerGame* game) : mGame(game) {}
    virtual ~Scene() {}

    virtual void Enter() = 0;   // 씬이 시작될 때 (텍스처 로드)
    virtual void Exit() = 0;    // 씬이 끝날 때 (메모리 해제)
    virtual void Update(const GameTimer& gt) = 0;
    virtual void Draw(const GameTimer& gt) = 0;
    virtual void OnRemotePlayerAttack(const PKT_S_PLAYER_ATTACK& attack) { (void)attack; }
    virtual void OnCharInput(WPARAM charCode) {}
    virtual void OnTextInput(const std::wstring& text)
    {
        for (wchar_t ch : text)
        {
            OnCharInput(static_cast<WPARAM>(ch));
        }
    }
    virtual void OnCompositionInput(const std::wstring& text, bool isFinal)
    {
        if (isFinal)
        {
            OnTextInput(text);
        }
    }
    virtual void DrawOverlay() {}
};

inline bool gIsChatInputActive = false;
inline bool gIsLanternUiInputActive = false;
inline ULONGLONG gLastSceneChangeTime = 0;
