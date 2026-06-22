#include "Warrior.h"

#include <Windows.h>

Warrior::Warrior()
{
    maxHp = 500.0f;
    hp = 500.0f;
    maxMp = 50.0f;
    mp = 50.0f;

    mMoveSpeed = 3.4f;
    mDashDuration = 0.35f;
    mDashSpeedMultiplier = 2.5f;
    mDashCooldownDuration = 8.0f;

    UpdateMeshForTier();
}

Warrior::~Warrior() {}

bool Warrior::Skill1()
{
    OutputDebugStringA("[Warrior] Skill1: earthshatter triggered\n");
    return true;
}

bool Warrior::Skill2()
{
    OutputDebugStringA("[Warrior] Skill2: greatsword swing triggered\n");
    return true;
}

float Warrior::GetSkillAttackLockDuration(int skillIndex) const
{
    return Player::GetSkillAttackLockDuration(skillIndex);
}

void Warrior::UpdateMeshForTier()
{
    if (mPlayerObject == nullptr) return;

    if (mCurrentTier == ClassTier::Tier1) {
        OutputDebugStringA("[Warrior Model] Tier1 loaded\n");
    }
    else if (mCurrentTier == ClassTier::Tier2) {
        OutputDebugStringA("[Warrior Model] Tier2 loaded\n");
    }
    else if (mCurrentTier == ClassTier::Tier3) {
        OutputDebugStringA("[Warrior Model] Tier3 loaded\n");
    }
}
