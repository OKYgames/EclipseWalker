#include "Archer.h"

Archer::Archer()
{
    maxHp = 250.0f;
    hp = 250.0f;
    maxMp = 100.0f;
    mp = 100.0f;

    mMoveSpeed = 4.0f;
    mDashDuration = 0.25f;
    mDashSpeedMultiplier = 3.0f;
    mDashCooldownDuration = 6.0f;

    UpdateMeshForTier();
}

Archer::~Archer() {}

bool Archer::Skill1() { return true; }
bool Archer::Skill2() { return true; }
void Archer::UpdateMeshForTier() {}
