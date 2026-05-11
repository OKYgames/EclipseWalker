#pragma once

#include "GameTimer.h"
#include "Player.h"
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

private:
    void UpdateCooldowns(float dt);
    void TryBasicAttack(Player* player, const std::vector<Monster*>& monsters);
    void TrySkillAttack(Player* player, const std::vector<Monster*>& monsters, int skillIndex);
    AttackProfile GetProfile(PlayerClass playerClass, int attackKind) const;
    void SendServerAttack(Player* player, int skillType) const;
    int ApplyAttack(Player* player, const std::vector<Monster*>& monsters, const AttackProfile& profile);
    bool EnsureDebugHitbox();
    void ShowDebugHitbox(Player* player, const AttackProfile& profile, int attackKind);
    void UpdateDebugHitbox(float dt);

private:
    EclipseWalkerGame* mGame = nullptr;
    std::vector<DebugHitboxSegment> mDebugHitboxSegments;
    float mDebugHitboxTimer = 0.0f;

    bool mLeftMousePressed = false;
    bool mQKeyPressed = false;
    bool mEKeyPressed = false;

    float mBasicCooldown = 0.0f;
    float mSkill1Cooldown = 0.0f;
    float mSkill2Cooldown = 0.0f;
};
