#pragma once

#include "GameTimer.h"
#include "Player.h"
#include <DirectXCollision.h>
#include <DirectXMath.h>
#include <functional>
#include <vector>

class EclipseWalkerGame;
class GameObject;
class MapSystem;
class Monster;
class SkillEffectManager;
struct Material;
struct RenderItem;

class CombatSystem
{
public:
    struct TargetSelectionOverride
    {
        RenderItem* HighlightRenderItem = nullptr;
        DirectX::XMFLOAT3 Position = { 0.0f, 0.0f, 0.0f };
        float HalfHeight = 0.0f;
        int MonsterId = -1;
        float HitDistance = 0.0f;
    };

    explicit CombatSystem(EclipseWalkerGame* game);

    void Reset();
    void Update(const GameTimer& gt, Player* player, const std::vector<Monster*>& monsters, MapSystem* mapSystem = nullptr);
    float GetSkillCooldownRemaining(int skillIndex) const;
    float GetSkillCooldownDuration(PlayerClass playerClass, int skillIndex) const;
    void SetDamageTextCallback(std::function<void(const DirectX::XMFLOAT3&, float)> callback);
    void SetBlockedHitCallback(std::function<bool(Monster*, const DirectX::XMFLOAT3&)> callback);
    void SetSkillEffectManager(SkillEffectManager* skillEffectManager);
    void ApplyMonsterKillReward(Player* player, int experienceReward);
    void ClearSelectedTarget();
    void SetTargetSelectionEnabled(bool enabled);
    void SetTargetSelectionOverridePicker(
        std::function<bool(const DirectX::XMFLOAT3&, const DirectX::XMFLOAT3&, const Player*, TargetSelectionOverride&)> callback);
    void SetTargetSelectionOverrideValidityCallback(std::function<bool()> callback);

private:
    struct AttackProfile
    {
        float range = 0.0f;
        float radius = 0.0f;
        float damage = 0.0f;
        float coneDot = 0.0f;
        bool hitAll = false;
    };

    struct DebugHitboxSegment
    {
        GameObject* Object = nullptr;
        RenderItem* Ritem = nullptr;
    };

    struct PendingAttack
    {
        AttackProfile Profile;
        DirectX::XMFLOAT3 Origin = { 0.0f, 0.0f, 0.0f };
        float RotY = 0.0f;
        float Timer = 0.0f;
        int SkillType = 0;
        int AttackKind = 0;
        int BasicAttackVariant = 1;
        PlayerClass ClassType = PlayerClass::None;
        Player* SourcePlayer = nullptr;
        Monster* TargetMonster = nullptr;
        int TargetMonsterId = -1;
        int ImpactIndex = 0;
        int ImpactCount = 1;
    };

private:
    void UpdateCooldowns(float dt);
    void ValidateSelectedMonster(const std::vector<Monster*>& monsters);
    bool HasSelectedTarget() const;
    bool HasSelectedTargetOverride() const;
    DirectX::XMFLOAT3 GetSelectedTargetPosition() const;
    DirectX::XMFLOAT3 GetSelectedTargetGroundPosition() const;
    int GetSelectedTargetMonsterId() const;
    Monster* FindFallbackSkillTarget(Player* player, const std::vector<Monster*>& monsters) const;
    Monster* PickMonsterUnderCursor(const std::vector<Monster*>& monsters, TargetSelectionOverride* outOverrideTarget) const;
    void SetSelectedMonster(Monster* monster);
    void SetSelectedTargetOverride(const TargetSelectionOverride& overrideTarget);
    void ClearSelectedMonster();
    void UpdatePendingAttacks(float dt, const std::vector<Monster*>& monsters);
    bool ResolveArrowCollision(
        Player* player,
        const DirectX::XMFLOAT3& previousPosition,
        const DirectX::XMFLOAT3& currentPosition,
        float rotY,
        MapSystem* mapSystem,
        const std::vector<Monster*>& monsters);
    void TryBasicAttack(Player* player, const std::vector<Monster*>& monsters);
    void TrySkillAttack(Player* player, const std::vector<Monster*>& monsters, int skillIndex);
    float GetSkillManaCost(PlayerClass playerClass, int skillIndex) const;
    float GetManaRegenPerSecond(PlayerClass playerClass) const;
    AttackProfile GetProfile(PlayerClass playerClass, int playerLevel, int attackKind) const;
    float GetHitDelay(int attackKind, int basicAttackVariant) const;
    void QueueAttack(Player* player, int skillType, int attackKind, const AttackProfile& profile);
    void SendServerAttackCast(const Player* player, int skillType, float visualRange = 0.0f, float visualDelay = 0.0f, int targetMonsterId = -1) const;
    void SendServerAttack(const PendingAttack& attack) const;
    bool TryGetWarriorWeaponHitbox(DirectX::BoundingOrientedBox& outHitbox) const;
    int ResolveHitMonsters(
        const PendingAttack& attack,
        const AttackProfile& profile,
        const DirectX::XMFLOAT3& resolvedEffectCenter,
        const std::vector<Monster*>& hitMonsters,
        Monster** outFirstHitMonster = nullptr);
    int ApplyWarriorWeaponAttack(
        PendingAttack& attack,
        const std::vector<Monster*>& monsters,
        Monster** outFirstHitMonster = nullptr);
    int ApplyAttack(
        const PendingAttack& attack,
        const DirectX::XMFLOAT3& attackForward,
        const std::vector<Monster*>& monsters,
        const AttackProfile& profile,
        Monster** outFirstHitMonster = nullptr);
    void HandleDebugHitboxToggle();
    bool EnsureDebugHitbox();
    void ShowDebugHitbox(
        const DirectX::XMFLOAT3& attackOrigin,
        float attackRotY,
        const AttackProfile& profile,
        int attackKind);
    void HideDebugHitbox();
    void UpdateDebugHitbox(float dt);

private:
    EclipseWalkerGame* mGame = nullptr;
    std::vector<DebugHitboxSegment> mDebugHitboxSegments;
    float mDebugHitboxTimer = 0.0f;
    bool mDebugHitboxEnabled = false;
    bool mDebugHitboxTogglePressed = false;
    std::vector<PendingAttack> mPendingAttacks;
    std::function<void(const DirectX::XMFLOAT3&, float)> mDamageTextCallback;
    std::function<bool(Monster*, const DirectX::XMFLOAT3&)> mBlockedHitCallback;
    std::function<bool(const DirectX::XMFLOAT3&, const DirectX::XMFLOAT3&, const Player*, TargetSelectionOverride&)> mTargetSelectionOverridePicker;
    std::function<bool()> mTargetSelectionOverrideValidityCallback;
    SkillEffectManager* mSkillEffectManager = nullptr;
    Monster* mSelectedMonster = nullptr;
    bool mHasSelectedTargetOverride = false;
    DirectX::XMFLOAT3 mSelectedTargetOverridePosition = { 0.0f, 0.0f, 0.0f };
    float mSelectedTargetOverrideHalfHeight = 0.0f;
    int mSelectedTargetOverrideMonsterId = -1;
    RenderItem* mSelectedTargetRenderItem = nullptr;
    Material* mSelectedMonsterBaseMaterial = nullptr;
    DirectX::XMFLOAT4 mSelectedMonsterBaseColorMultiplier = { 1.0f, 1.0f, 1.0f, 1.0f };

    bool mLeftMousePressed = false;
    bool mQKeyPressed = false;
    bool mEKeyPressed = false;
    bool mTargetSelectionEnabled = true;

    float mBasicCooldown = 0.0f;
    float mSkill1Cooldown = 0.0f;
    float mSkill2Cooldown = 0.0f;
};
