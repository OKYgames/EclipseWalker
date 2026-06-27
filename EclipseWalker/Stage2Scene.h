#pragma once
#include "Scene.h"
#include "ChatController.h"
#include "CombatSystem.h"
#include "DamageTextRenderer.h"
#include "DebugColliderVisualizer.h"
#include "LanternSystem.h"
#include "MapSystem.h"
#include "Monster.h"
#include "PickupSystem.h"
#include "SkillEffectManager.h"
#include "Stage2BossController.h"
#include "WorldStateController.h"
#include <vector>
#include <memory>
#include <unordered_map>

class Stage2Scene : public Scene
{
public:
    Stage2Scene(EclipseWalkerGame* game)
        : Scene(game)
        , mChatController(game)
        , mCombatSystem(game)
        , mDamageTextRenderer(game)
        , mPickupSystem(game, &mLanternSystem)
        , mWorldStateController(game, &mLanternSystem)
    {
    }

    virtual void Enter() override;
    virtual void Exit() override;
    virtual void Update(const GameTimer& gt) override;
    virtual void Draw(const GameTimer& gt) override;
    virtual void OnRemotePlayerAttack(const PKT_S_PLAYER_ATTACK& attack) override;
    virtual void OnCharInput(WPARAM charCode) override;
    virtual void OnTextInput(const std::wstring& text) override;
    virtual void OnCompositionInput(const std::wstring& text, bool isFinal) override;

    MapSystem* GetActiveMapSystem() { return mMapSystem.get(); }
    float GetDomainRadius() const { return mWorldStateController.GetDomainRadius(); }
    bool GetIsDomainActive() const { return mWorldStateController.IsDomainActive(); }
    bool IsOtherWorld() const { return mWorldStateController.IsOtherWorld(); }

private: 
    struct MonsterHealthBar
    {
        Monster* Owner = nullptr;
        GameObject* Back = nullptr;
        GameObject* Fill = nullptr;
    };

    std::unique_ptr<MapSystem> mMapSystem;
    std::vector<GameObject*> mOwnedObjects;
    std::vector<RenderItem*> mOwnedRenderItems;
    std::vector<Monster*> mMonsterPtrs;
    std::vector<MonsterHealthBar> mMonsterHealthBars;
    std::unordered_map<int, DirectX::XMFLOAT3> mMonsterTargetPos;
    std::unordered_map<int, MonsterState> mMonsterServerStates;
    std::unordered_map<int, Monster*> mMonsterById;
    GameObject* mDomainBoundaryObj = nullptr;
    ChatController mChatController;
    CombatSystem mCombatSystem;
    DamageTextRenderer mDamageTextRenderer;
    SkillEffectManager mSkillEffectManager;
    DebugColliderVisualizer mDebugColliderVisualizer;
    Stage2BossController mBossController;
    LanternSystem mLanternSystem;
    PickupSystem mPickupSystem;
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
    void CreateMonsterHealthBar(Monster* monster);
    void UpdateMonsterHealthBars();
    void UpdateMonstersFromServer();
    void FillStage2LanternGauge(Player* player);
    void UpdateStage2LanternAutoReturn(const GameTimer& gt, Player* player);
    void QueueRespawn(const PKT_S_PLAYER_RESPAWN& respawn);
    void UpdateRespawnOverlay(const GameTimer& gt, Player* player, bool hasFocus);
    void ApplyQueuedRespawn(Player* player);
    void UpdateStageClearState(const GameTimer& gt, Player* player);
    void ShowServerStageClear(const PKT_S_GAME_RESULT& result);
    void ShowLocalStageClear();

    bool mRespawnOverlayActive = false;
    bool mRespawnButtonReady = false;
    bool mRespawnMousePressed = false;
    bool mWasPlayerDeadLastFrame = false;
    bool mHasQueuedRespawnPacket = false;
    bool mRespawnRequestPending = false;
    float mRespawnOverlayCountdown = 0.0f;
    PKT_S_PLAYER_RESPAWN mQueuedRespawnPacket = {};
    bool mStageClearShown = false;
    float mStageClearElapsedSeconds = 0.0f;
    float mAccumulatedLocalBossDamage = 0.0f;
    float mLastObservedBossHp = -1.0f;
};
