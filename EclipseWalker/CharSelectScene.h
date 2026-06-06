#pragma once
#include "Scene.h"
#include "Player.h"
#include <SpriteBatch.h>
#include <SpriteFont.h>
#include <GraphicsMemory.h>
#include <DescriptorHeap.h>
#include <array>
#include <memory>
#include <vector>

struct RenderItem;
class GameObject;

class CharSelectScene : public Scene
{
public:
    CharSelectScene(EclipseWalkerGame* game) : Scene(game) {}

    virtual void Enter() override;
    virtual void Exit() override;
    virtual void Update(const GameTimer& gt) override;
    virtual void Draw(const GameTimer& gt) override;

    struct UiRect
    {
        float left = 0.0f;
        float top = 0.0f;
        float right = 0.0f;
        float bottom = 0.0f;
    };

private:
    bool mLeftKeyPressed = false;
    bool mRightKeyPressed = false;
    bool mEnterKeyPressed = false;
    bool mMousePressed = false;
    float mLastViewportWidth = 0.0f;
    float mLastViewportHeight = 0.0f;
    std::array<UiRect, 3> mClassCardRects = {};
    UiRect mConfirmButtonRect = {};
    std::array<GameObject*, 3> mClassCardObjects = {};
    std::array<GameObject*, 3> mClassPreviewObjects = {};
    std::array<std::vector<GameObject*>, 3> mClassPreviewOverlayObjects = {};
    GameObject* mSelectionHighlightObj = nullptr;
    RenderItem* mSkillIcon1Ritem = nullptr;
    RenderItem* mSkillIcon2Ritem = nullptr;
    std::unique_ptr<DirectX::GraphicsMemory> mGraphicsMemory;
    std::unique_ptr<DirectX::DescriptorHeap> mFontHeap;
    std::unique_ptr<DirectX::SpriteBatch> mSpriteBatch;
    std::unique_ptr<DirectX::SpriteFont> mFont;

    void CycleSelection(int direction);
    void InitializeUiResources();
    void BuildStaticUi();
    void RebuildStaticUiForCurrentViewport();
    void BuildClassPreviewModels(
        const std::array<DirectX::XMFLOAT3, 3>& spawnPositions,
        const std::array<float, 3>& targetHeights);
    void UpdateSelectionVisuals();
    void SelectClass(PlayerClass playerClass);
    bool HandleMouseInput();
    bool ConfirmSelection();
};
