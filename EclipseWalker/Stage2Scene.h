#pragma once
#include "Scene.h"
#include "CombatSystem.h"
#include "LanternSystem.h"
#include "MapSystem.h"
#include "UIManager.h"
#include "WorldStateController.h"
#include <vector>
#include <memory>

class Monster;

class Stage2Scene : public Scene
{
public:
    Stage2Scene(EclipseWalkerGame* game)
        : Scene(game)
        , mCombatSystem(game)
        , mWorldStateController(game, &mLanternSystem)
    {
    }

    virtual void Enter() override;
    virtual void Exit() override;
    virtual void Update(const GameTimer& gt) override;
    virtual void Draw(const GameTimer& gt) override;

    MapSystem* GetActiveMapSystem() { return mMapSystem.get(); }
    float GetDomainRadius() const { return mWorldStateController.GetDomainRadius(); }
    bool GetIsDomainActive() const { return mWorldStateController.IsDomainActive(); }
    bool IsOtherWorld() const { return mWorldStateController.IsOtherWorld(); }

private: 
    std::unique_ptr<MapSystem> mMapSystem;
    std::vector<GameObject*> mOwnedObjects;
    std::vector<RenderItem*> mOwnedRenderItems;
    std::vector<Monster*> mMonsterPtrs;
    GameObject* mDomainBoundaryObj = nullptr;
    Monster* mBoss = nullptr;
    CombatSystem mCombatSystem;
    LanternSystem mLanternSystem;
    WorldStateController mWorldStateController;
    bool mLanternUiClickPressed = false;
    bool mDebugPositionPrintKeyPressed = false;
    float mDebugBossHpDrainTimer = 0.0f;

    void TrackOwned(GameObject* object, RenderItem* renderItem);
    void ReleaseOwnedObjects();
    void BuildBoss();
    void LogPlayerPosition(const DirectX::XMFLOAT3& position);
};
