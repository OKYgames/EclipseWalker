#pragma once

#include "GameTimer.h"
#include <array>
#include <cstddef>
#include <DescriptorHeap.h>
#include <DirectXMath.h>
#include <SpriteBatch.h>
#include <SpriteFont.h>
#include <functional>
#include <memory>
#include <vector>

class DamageTextRenderer;
class EclipseWalkerGame;
class GameObject;
class MapSystem;
class Monster;
class Player;
struct RenderItem;

class Stage2BossController
{
public:
    using TrackOwnedCallback = std::function<void(GameObject*, RenderItem*)>;

    static constexpr int BossHpLayerCount = 200;
    static constexpr int BossDamageLayersPerHit = 10;

    void Initialize(
        EclipseWalkerGame* game,
        MapSystem* mapSystem,
        DamageTextRenderer* damageTextRenderer,
        const TrackOwnedCallback& trackOwned);
    void InitializeHealthText();
    void Reset();
    void Update(const GameTimer& gt, Player* player, bool isOtherWorld);
    void Draw();

    Monster* GetBoss() const { return mBoss; }
    bool IsInvulnerable() const;
    int GetCurrentHealthLayer() const;
    void ApplyServerSync(int state, float x, float y, float z, float rotY);
    void ApplyServerHit(int remainHp, bool isDead);
    void ApplyServerPattern(int patternType, float x, float y, float z, float radius, float delay, int damage);

    static DirectX::XMFLOAT3 GetBossAnchorPosition();
    static DirectX::XMFLOAT3 GetBossSpawnPosition();
    static DirectX::XMFLOAT3 GetPlayerStartPosition();

private:
    enum class BossMoveState
    {
        Idle,
        Chase,
        Strafe,
        AttackWindup,
        AttackRecover
    };

    enum class BossMirrorPatternState
    {
        Inactive,
        Summon,
        Dive,
        Hidden,
        Split
    };

    void BuildBoss();
    void BuildBossPatternIndicator();
    void BuildBossMirrorPatternObjects();
    void ShowBossPatternRadiusIndicator(const DirectX::XMFLOAT3& center, float radius, float duration);
    void UpdateBossPatternIndicator(float dt);
    void UpdateNormalBehavior(Player* player, bool isOtherWorld, float dt);
    void ResetNormalBehavior();
    void BeginBossAttack();
    void SetBossLocomotionState(bool isMoving);
    void UpdateBossAnimationDebugInput();
    bool PlayBossDebugAnimation(std::size_t clipIndex);
    void StopBossAnimationDebug();
    void FaceTowards(const DirectX::XMFLOAT3& targetPosition, float dt);
    bool MoveBoss(const DirectX::XMFLOAT3& moveDirection, float speed, float dt);
    void UpdateBossPattern150Damage(Player* player, float dt);
    void ApplyBossPattern150Damage(Player* player);
    void UpdateBossWipeDamage(Player* player, bool isOtherWorld, float dt);
    void ApplyBossWipeDamage(Player* player, bool isOtherWorld);
    void UpdateBossPatternTriggers(Player* player, int currentBossLayer);
    void TriggerBossPattern150(Player* player);
    void TriggerBossWipePattern(Player* player);
    void TriggerBossMirrorPattern(Player* player);
    void UpdateBossMirrorPattern(Player* player, bool isOtherWorld, float dt);
    void EndBossMirrorPattern();
    void UpdateBossHealthUi(Player* player, int currentBossLayer, bool isOtherWorld);
    void UpdateBossWorldVisibility(bool isOtherWorld);
    void DrawBossHealthText();
    int CalculateBossHealthLayer(float currentHp, float maxHp) const;
    bool ShouldShowBossHealth(Player* player) const;
    void TrackOwned(GameObject* object, RenderItem* renderItem) const;
    void SetPatternObjectVisible(GameObject* object, bool visible, const DirectX::XMFLOAT4& color) const;

private:
    EclipseWalkerGame* mGame = nullptr;
    MapSystem* mMapSystem = nullptr;
    DamageTextRenderer* mDamageTextRenderer = nullptr;
    TrackOwnedCallback mTrackOwned;

    Monster* mBoss = nullptr;
    GameObject* mBossPatternRadiusObj = nullptr;
    GameObject* mBossPatternRadiusRingObj = nullptr;

    bool mShowBossHealthText = false;
    bool mBossPattern150Triggered = false;
    bool mBossPattern150DamagePending = false;
    bool mBossWipeTriggered = false;
    bool mBossWipeDamagePending = false;
    bool mBossMirrorPatternTriggered = false;
    BossMoveState mBossMoveState = BossMoveState::Idle;
    BossMirrorPatternState mBossMirrorPatternState = BossMirrorPatternState::Inactive;
    int mBossHealthTextLayer = 0;
    int mBossMirrorRealIndex = 1;
    float mBossFacingYaw = 0.0f;
    float mBossAttackCooldownTimer = 0.0f;
    float mBossActionTimer = 0.0f;
    float mBossStrafeDirection = 1.0f;
    float mBossMirrorPatternTimer = 0.0f;
    float mBossMirrorResolveHp = 0.0f;
    float mBossMirrorSplitYaw = 0.0f;
    DirectX::XMFLOAT3 mBossMirrorDiveStart = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 mBossMirrorDiveTarget = { 0.0f, 0.0f, 0.0f };
    float mBossPatternRadiusTimer = 0.0f;
    float mBossPatternRadiusDuration = 0.0f;
    float mBossPattern150DamageTimer = 0.0f;
    float mBossWipeDamageTimer = 0.0f;
    float mBossWipeDamageDuration = 0.0f;
    bool mBossAttackDamageApplied = false;
    bool mBossAnimationDebugActive = false;
    bool mBossAnimationDebugPreviousKeyPressed = false;
    bool mBossAnimationDebugNextKeyPressed = false;
    bool mBossAnimationDebugReplayKeyPressed = false;
    bool mBossAnimationDebugExitKeyPressed = false;
    std::size_t mBossAnimationDebugClipIndex = static_cast<std::size_t>(-1);
    DirectX::XMFLOAT3 mBossPattern150DamageCenter = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 mBossWipeDamageCenter = { 0.0f, 0.0f, 0.0f };
    std::array<GameObject*, 3> mBossMirrorObjects{};
    std::array<GameObject*, 3> mBossMirrorFrameTopObjects{};
    std::array<GameObject*, 3> mBossMirrorFrameBottomObjects{};
    std::array<GameObject*, 3> mBossMirrorFrameLeftObjects{};
    std::array<GameObject*, 3> mBossMirrorFrameRightObjects{};
    std::array<GameObject*, 3> mBossMirrorSheenObjects{};
    std::array<GameObject*, 3> mBossMirrorCloneObjects{};

    std::unique_ptr<DirectX::DescriptorHeap> mBossHealthTextHeap;
    std::unique_ptr<DirectX::SpriteBatch> mBossHealthTextBatch;
    std::unique_ptr<DirectX::SpriteFont> mBossHealthTextFont;
};
