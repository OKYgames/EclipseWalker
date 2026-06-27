#pragma once

#include "Scene.h"
#include <vector>
#include <DescriptorHeap.h>
#include <GraphicsMemory.h>
#include <SpriteBatch.h>
#include <SpriteFont.h>

class GameObject;
struct RenderItem;

class LoadingScene : public Scene
{
public:
    explicit LoadingScene(EclipseWalkerGame* game)
        : Scene(game)
    {
    }

    virtual void Enter() override;
    virtual void Exit() override;
    virtual void Update(const GameTimer& gt) override;
    virtual void Draw(const GameTimer& gt) override;
    void EnforceHiddenGameplayRenderItems();

private:
    struct HiddenRenderItemState
    {
        RenderItem* Item = nullptr;
        bool WasVisible = false;
    };

    void InitializeUiResources();
    void TrackOwned(GameObject* object, RenderItem* renderItem);

private:
    std::unique_ptr<DirectX::GraphicsMemory> mGraphicsMemory;
    std::unique_ptr<DirectX::DescriptorHeap> mFontHeap;
    std::unique_ptr<DirectX::SpriteBatch> mSpriteBatch;
    std::unique_ptr<DirectX::SpriteFont> mFont;
    std::vector<GameObject*> mOwnedObjects;
    std::vector<RenderItem*> mOwnedRenderItems;
    std::vector<HiddenRenderItemState> mHiddenRenderItems;
    float mElapsedSeconds = 0.0f;
    bool mHasPresentedFrame = false;
};
