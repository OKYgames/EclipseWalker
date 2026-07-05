#pragma once

#include "MapSystem.h"
#include "Scene.h"
#include <memory>
#include <vector>

class GameObject;
struct RenderItem;

class VillageScene : public Scene
{
public:
    VillageScene(EclipseWalkerGame* game) : Scene(game) {}

    virtual void Enter() override;
    virtual void Exit() override;
    virtual void Update(const GameTimer& gt) override;
    virtual void Draw(const GameTimer& gt) override;

private:
    void TrackOwned(GameObject* object, RenderItem* renderItem);
    void ReleaseOwnedObjects();

    std::unique_ptr<MapSystem> mMapSystem;
    std::vector<GameObject*> mOwnedObjects;
    std::vector<RenderItem*> mOwnedRenderItems;
    bool mBackKeyPressed = false;
    bool mStage1KeyPressed = false;
};
