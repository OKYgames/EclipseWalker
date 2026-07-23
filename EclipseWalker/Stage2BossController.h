#pragma once

#include "AudioManager.h"
#include "GameTimer.h"
#include "DebugColliderVisualizer.h"
#include <array>
#include <cstddef>
#include <cstdint>
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
    struct MirrorPickResult
    {
        RenderItem* HighlightRenderItem = nullptr;
        DirectX::XMFLOAT3 Position = { 0.0f, 0.0f, 0.0f };
        float HalfHeight = 0.0f;
        int MonsterId = -1;
        float HitDistance = 0.0f;
    };

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
    bool IsMirrorPatternActive() const;
    bool IsMirrorSplitTargetingActive() const;
    bool TryPickMirrorTarget(
        const DirectX::XMFLOAT3& rayOrigin,
        const DirectX::XMFLOAT3& rayDirection,
        const DirectX::XMFLOAT3& playerPosition,
        MirrorPickResult& outTarget) const;
    int GetCurrentHealthLayer() const;
    void ApplyServerSync(int state, int attackSequence, int targetPlayerId, int attackType, int actionPhase, float x, float y, float z, float rotY, int remainHp, bool isDead);
    void ApplyServerHit(int remainHp, bool isDead);
    void ApplyServerPattern(int patternType, float x, float y, float z, float radius, float delay, int damage, int patternData);

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
        Split,
        Reveal
    };

    enum class BossBasicAttackType
    {
        TwoHitCombo,
        ThreeHitCombo,
        SwordAttack2,
        WhipAttack
    };

    enum class BossScriptedAnimationState
    {
        None,
        SpawnSummon,
        Pattern150Roar,
        WipeSummon,
        WipeSwordAttack
    };

    struct BossAttackHitBox
    {
        float TriggerProgress = 0.5f;
        DirectX::XMFLOAT3 LocalCenter = { 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT3 Extents = { 0.0f, 0.0f, 0.0f };
    };

    struct BossAttackProfile
    {
        const char* ClipName = "";
        const char* AttackLabel = "";
        float FallbackDuration = 1.0f;
        float StepMoveEndProgress = 0.35f;
        float Damage = 0.0f;
        std::size_t HitCount = 0;
        std::array<BossAttackHitBox, 3> HitBoxes{};
    };

    void BuildBoss();
    void BuildBossPatternIndicator();
    void BuildBossMirrorPatternObjects();
    void BuildBossMirrorSpotlightObjects();
    void ShowBossPatternRadiusIndicator(const DirectX::XMFLOAT3& center, float radius, float duration);
    void UpdateBossPatternIndicator(float dt);
    void UpdateNormalBehavior(Player* player, bool isOtherWorld, float dt);
    void ResetNormalBehavior();
    void BeginBossAttack();
    void SelectBossBasicAttack();
    bool SetBossBasicAttackFromNetwork(int attackType);
    bool PlaySelectedBossBasicAttack();
    const BossAttackProfile& GetSelectedBossAttackProfile() const;
    DirectX::XMFLOAT3 RotateBossAttackLocalOffset(const DirectX::XMFLOAT3& localOffset) const;
    bool DoesPlayerOverlapBossAttackHitBox(Player* player, const BossAttackHitBox& hitBox) const;
    void UpdateBossAttackSequence(Player* player, bool isOtherWorld, float dt);
    void UpdateBossAttackDebugVisualizer(bool isOtherWorld);
    bool PlayBossScriptedAnimation(
        BossScriptedAnimationState state,
        const char* clipName,
        float fallbackDuration,
        float blendDuration);
    void UpdateBossScriptedAnimation(float dt);
    bool IsBossScriptedAnimationActive() const;
    void SetBossLocomotionState(bool isMoving);
    void UpdateBossAnimationDebugInput();
    bool PlayBossDebugAnimation(std::size_t clipIndex);
    void StopBossAnimationDebug();
    void UpdateServerFacingYaw(float dt);
    void FaceTowards(const DirectX::XMFLOAT3& targetPosition, float dt);
    bool MoveBoss(const DirectX::XMFLOAT3& moveDirection, float speed, float dt);
    void UpdateBossPattern150Damage(Player* player, float dt);
    void ApplyBossPattern150Damage(Player* player);
    void UpdateBossWipeDamage(Player* player, bool isOtherWorld, float dt);
    void ApplyBossWipeDamage(Player* player, bool isOtherWorld);
    void UpdateBossPatternTriggers(Player* player, int currentBossLayer);
    void TriggerBossPattern150(Player* player);
    void TriggerBossWipePattern(Player* player);
    void TriggerBossMirrorPattern(Player* player, int mirrorRealIndex = -1);
    void UpdateBossMirrorPattern(Player* player, bool isOtherWorld, float dt);
    void UpdateBossMirrorRealShadow(bool isOtherWorld);
    void BeginBossMirrorReveal();
    void EndBossMirrorPattern();
    void StopBossPattern150Sound();
    void UpdateBossHealthUi(Player* player, int currentBossLayer, bool isOtherWorld);
    void UpdateBossWorldVisibility(bool isOtherWorld);
    void UpdateBossMirrorSpotlights(bool isOtherWorld);
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
    BossBasicAttackType mBossBasicAttackType = BossBasicAttackType::TwoHitCombo;
    BossScriptedAnimationState mBossScriptedAnimationState = BossScriptedAnimationState::None;
    int mBossHealthTextLayer = 0;
    int mLastServerState = -1;
    int mLastServerAttackSequence = 0;
    int mBossMirrorRealIndex = 1;
    float mBossFacingYaw = 0.0f;
    float mBossAttackCooldownTimer = 0.0f;
    float mBossActionTimer = 0.0f;
    float mBossAttackRecoverDuration = 0.7f;
    float mBossScriptedAnimationTimer = 0.0f;
    float mBossStrafeDirection = 1.0f;
    float mBossServerFacingYaw = 0.0f;
    float mBossMirrorPatternTimer = 0.0f;
    float mBossMirrorHealTimer = 0.0f;
    float mBossMirrorResolveHp = 0.0f;
    float mBossMirrorSplitYaw = 0.0f;
    DirectX::XMFLOAT3 mBossMirrorDiveStart = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 mBossMirrorDiveTarget = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 mBossMirrorRevealStart = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 mBossMirrorRevealTarget = { 0.0f, 0.0f, 0.0f };
    float mBossMirrorRevealTargetYaw = 0.0f;
    float mBossMirrorRevealTimer = 0.0f;
    float mBossPatternRadiusTimer = 0.0f;
    float mBossPatternRadiusDuration = 0.0f;
    float mBossPattern150DamageTimer = 0.0f;
    float mBossWipeDamageTimer = 0.0f;
    float mBossWipeDamageDuration = 0.0f;
    float mBossAttackAnimationDuration = 0.0f;
    std::size_t mBossAttackNextHitIndex = 0;
    AudioManager::ClipHandle mBossPattern150SoundHandle = AudioManager::InvalidClipHandle;
    bool mBossDeathSoundPlayed = false;
    bool mHasBossServerFacingYaw = false;
    std::uint32_t mBossAttackRandomState = 0x5EED1234u;
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
    std::array<GameObject*, 3> mBossMirrorSpotlightBeamObjects{};
    GameObject* mBossMirrorRealShadowObject = nullptr;
    std::array<int, 3> mBossMirrorSpotLightIndices = { -1, -1, -1 };
    DebugColliderVisualizer mBossAttackDebugVisualizer;

    std::unique_ptr<DirectX::DescriptorHeap> mBossHealthTextHeap;
    std::unique_ptr<DirectX::SpriteBatch> mBossHealthTextBatch;
    std::unique_ptr<DirectX::SpriteFont> mBossHealthTextFont;
};
