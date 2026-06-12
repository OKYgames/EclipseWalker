#pragma once
#include "Scene.h"
#include "ChatController.h"
#include "CombatSystem.h"
#include "DamageTextRenderer.h"
#include "DebugColliderVisualizer.h"
#include "LanternSystem.h"
#include "MapSystem.h"
#include "Stage2BossController.h"
#include "WorldStateController.h"
#include <vector>
#include <memory>

class Monster;

class Stage2Scene : public Scene
{
public:
    Stage2Scene(EclipseWalkerGame* game)
        : Scene(game)
        , mChatController(game)
        , mCombatSystem(game)
        , mDamageTextRenderer(game)
        , mWorldStateController(game, &mLanternSystem)
    {
    }

    virtual void Enter() override;
    virtual void Exit() override;
    virtual void Update(const GameTimer& gt) override;
    virtual void Draw(const GameTimer& gt) override;
    virtual void OnCharInput(WPARAM charCode) override;
    virtual void OnTextInput(const std::wstring& text) override;
    virtual void OnCompositionInput(const std::wstring& text, bool isFinal) override;

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
    ChatController mChatController;
    CombatSystem mCombatSystem;
    DamageTextRenderer mDamageTextRenderer;
    DebugColliderVisualizer mDebugColliderVisualizer;
    Stage2BossController mBossController;
    LanternSystem mLanternSystem;
    WorldStateController mWorldStateController;
    bool mLanternUiClickPressed = false;
    bool mDebugPositionPrintKeyPressed = false;
    bool mDebugOutgoingDamageKeyPressed = false;
    bool mDebugIncomingDamageKeyPressed = false;
    bool mHasLastPlayerHpForDamageText = false;
    float mLastPlayerHpForDamageText = 0.0f;
    bool mWasOtherWorldLastFrame = false;
    bool mStage2LanternAutoReturnPending = false;
    float mStage2LanternAutoReturnElapsed = 0.0f;

    void TrackOwned(GameObject* object, RenderItem* renderItem);
    void ReleaseOwnedObjects();
    void LogPlayerPosition(const DirectX::XMFLOAT3& position);
    void UpdateIncomingDamageText(Player* player);
    void UpdateDebugColliders(Player* player);
    void FillStage2LanternGauge(Player* player);
    void UpdateStage2LanternAutoReturn(const GameTimer& gt, Player* player);
};
