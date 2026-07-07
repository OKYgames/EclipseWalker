#pragma once

#include "ChatController.h"
#include "MapSystem.h"
#include "RedPortalEffect.h"
#include "Scene.h"
#include <memory>
#include <vector>

class GameObject;
struct RenderItem;

class VillageScene : public Scene
{
public:
    VillageScene(EclipseWalkerGame* game);

    virtual void Enter() override;
    virtual void Exit() override;
    virtual void Update(const GameTimer& gt) override;
    virtual void Draw(const GameTimer& gt) override;
    virtual void OnCharInput(WPARAM charCode) override;
    virtual void OnTextInput(const std::wstring& text) override;
    virtual void OnCompositionInput(const std::wstring& text, bool isFinal) override;

private:
    void TrackOwned(GameObject* object, RenderItem* renderItem);
    void ReleaseOwnedObjects();
    void LogPlayerPosition(const DirectX::XMFLOAT3& position);

    std::unique_ptr<MapSystem> mMapSystem;
    std::unique_ptr<RedPortalEffect> mPortalEffect;
    std::vector<GameObject*> mOwnedObjects;
    std::vector<RenderItem*> mOwnedRenderItems;
    RenderItem* mCloudLayerA = nullptr;
    RenderItem* mCloudLayerB = nullptr;
    ChatController mChatController;
    bool mBackKeyPressed = false;
    bool mStage1KeyPressed = false;
    bool mPortalInteractKeyPressed = false;
    bool mPrintPositionKeyPressed = false;
};
