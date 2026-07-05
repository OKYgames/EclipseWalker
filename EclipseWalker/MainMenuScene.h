#pragma once
#include "Scene.h"
#include <SpriteBatch.h>
#include <SpriteFont.h>
#include <GraphicsMemory.h>
#include <DescriptorHeap.h>

class MainMenuScene : public Scene
{
public:
    MainMenuScene(EclipseWalkerGame* game) : Scene(game) {}

    virtual void Enter() override;
    virtual void Exit() override;
    virtual void Update(const GameTimer& gt) override;
    virtual void Draw(const GameTimer& gt) override;

private:
    void InitializeUiResources();
    bool IsLocalPlayerHost() const;
    bool UpdateLocalReadyFromSnapshot();
    void RefreshLobbyState();
    void RecalculateCanStart();
    void RequestLeaveRoom();
    void HandleMouseClick(float baseX, float baseY);

private:
    LobbyStateSnapshot mLobbyState;
    bool mReadyKeyPressed = false;
    bool mStartKeyPressed = false;
    bool mBackKeyPressed = false;
    bool mMousePressed = false;
    bool mLeavingRoom = false;
    bool mLocalReady = false;
    std::unique_ptr<DirectX::GraphicsMemory> mGraphicsMemory;
    std::unique_ptr<DirectX::DescriptorHeap> mFontHeap;
    std::unique_ptr<DirectX::SpriteBatch> mSpriteBatch;
    std::unique_ptr<DirectX::SpriteFont> mFont;
};
