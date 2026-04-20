#pragma once
#include "Scene.h"
#include "MapSystem.h"
#include "UIManager.h"
#include <vector>
#include <memory>

class Stage2Scene : public Scene
{
public:
    Stage2Scene(EclipseWalkerGame* game) : Scene(game) {}

    virtual void Enter() override;
    virtual void Exit() override;
    virtual void Update(const GameTimer& gt) override;
    virtual void Draw(const GameTimer& gt) override;

    MapSystem* GetActiveMapSystem() { return mMapSystem.get(); }

private: 
    std::unique_ptr<MapSystem> mMapSystem;
    std::vector<GameObject*> mOwnedObjects;
    std::vector<RenderItem*> mOwnedRenderItems;
    void TrackOwned(GameObject* object, RenderItem* renderItem);
    void ReleaseOwnedObjects();
};
