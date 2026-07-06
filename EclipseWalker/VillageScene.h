#pragma once

#include "ChatController.h"
#include "MapSystem.h"
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

    std::unique_ptr<MapSystem> mMapSystem;
    std::vector<GameObject*> mOwnedObjects;
    std::vector<RenderItem*> mOwnedRenderItems;
    RenderItem* mCloudLayerA = nullptr;
    RenderItem* mCloudLayerB = nullptr;
    ChatController mChatController;
    bool mBackKeyPressed = false;
    bool mStage1KeyPressed = false;
};
