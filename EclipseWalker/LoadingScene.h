#pragma once

#include "Scene.h"
#include <DescriptorHeap.h>
#include <GraphicsMemory.h>
#include <SpriteBatch.h>
#include <SpriteFont.h>

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

private:
    void InitializeUiResources();

private:
    std::unique_ptr<DirectX::GraphicsMemory> mGraphicsMemory;
    std::unique_ptr<DirectX::DescriptorHeap> mFontHeap;
    std::unique_ptr<DirectX::SpriteBatch> mSpriteBatch;
    std::unique_ptr<DirectX::SpriteFont> mFont;
    float mElapsedSeconds = 0.0f;
    bool mHasPresentedFrame = false;
};
