#pragma once
#include "Scene.h"
#include "NetworkManager.h"
#include <DescriptorHeap.h>
#include <GraphicsMemory.h>
#include <SpriteBatch.h>
#include <SpriteFont.h>
#include <memory>
#include <string>
#include <vector>

class RoomSelectScene : public Scene
{
public:
    RoomSelectScene(EclipseWalkerGame* game) : Scene(game) {}

    virtual void Enter() override;
    virtual void Exit() override;
    virtual void Update(const GameTimer& gt) override;
    virtual void Draw(const GameTimer& gt) override;
    virtual void OnCharInput(WPARAM wParam) override;

private:
    void InitializeUiResources();
    void RefreshRoomList();
    void TryJoinSelectedRoom();
    void TryCreateRoom();
    void HandleMouseClick(float baseX, float baseY);

private:
    std::vector<RoomListItem> mRooms;
    std::string mRoomTitle;
    std::string mStatusText;
    int mSelectedRoomIndex = 0;
    float mRefreshTimer = 0.0f;
    bool mMousePressed = false;
    bool mUpPressed = false;
    bool mDownPressed = false;
    bool mEnterPressed = false;
    bool mRefreshPressed = false;
    std::unique_ptr<DirectX::GraphicsMemory> mGraphicsMemory;
    std::unique_ptr<DirectX::DescriptorHeap> mFontHeap;
    std::unique_ptr<DirectX::SpriteBatch> mSpriteBatch;
    std::unique_ptr<DirectX::SpriteFont> mFont;
};
