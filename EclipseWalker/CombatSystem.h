#pragma once

#include "GameTimer.h"
#include "Player.h"
#include <DirectXMath.h>
#include <vector>

class EclipseWalkerGame;
class GameObject;
class Monster;
struct RenderItem;

class CombatSystem
{
public:
    explicit CombatSystem(EclipseWalkerGame* game);

    void Reset();
    void Update(const GameTimer& gt, Player* player, const std::vector<Monster*>& monsters);

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
    };

private:
    void UpdateCooldowns(float dt);
    void UpdatePendingAttacks(float dt, const std::vector<Monster*>& monsters);
    void TryBasicAttack(Player* player, const std::vector<Monster*>& monsters);
    void TrySkillAttack(Player* player, const std::vector<Monster*>& monsters, int skillIndex);
    AttackProfile GetProfile(PlayerClass playerClass, int attackKind) const;
    float GetHitDelay(int attackKind) const;
    void QueueAttack(Player* player, int skillType, int attackKind, const AttackProfile& profile);
    void SendServerAttack(const PendingAttack& attack) const;
    int ApplyAttack(
        const DirectX::XMFLOAT3& attackOrigin,
        const DirectX::XMFLOAT3& attackForward,
        const std::vector<Monster*>& monsters,
        const AttackProfile& profile);
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

    bool mLeftMousePressed = false;
    bool mQKeyPressed = false;
    bool mEKeyPressed = false;

    float mBasicCooldown = 0.0f;
    float mSkill1Cooldown = 0.0f;
    float mSkill2Cooldown = 0.0f;
};
