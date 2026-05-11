#pragma once
#include "Scene.h"
#include <SpriteBatch.h>
#include <SpriteFont.h>
#include <GraphicsMemory.h>
#include <DescriptorHeap.h>
#include <string>

class LoginScene : public Scene
{
public:
    LoginScene(EclipseWalkerGame* game) : Scene(game) {}

    virtual void Enter() override;
    virtual void Exit() override;
    virtual void Update(const GameTimer& gt) override;
    virtual void Draw(const GameTimer& gt) override;
    virtual void OnCharInput(WPARAM wParam) override;
private:
    std::string mInputID = "";
    std::string mInputPW = "";
    std::string mStatusText = "";
    int mCurrentFocus = 0; // 0: ID, 1: PW
    bool mLoginRequested = false;
    std::unique_ptr<DirectX::GraphicsMemory> mGraphicsMemory;
    std::unique_ptr<DirectX::DescriptorHeap> mFontHeap;
    std::unique_ptr<DirectX::SpriteBatch> mSpriteBatch;
    std::unique_ptr<DirectX::SpriteFont> mFont;
};
