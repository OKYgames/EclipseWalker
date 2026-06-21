#include "Mage.h"

Mage::Mage()
{
    maxHp = 150.0f;
    hp = 150.0f;
    maxMp = 300.0f;
    mp = 300.0f;

    mMoveSpeed = 3.6f;
    mDashDuration = 0.2f;
    mDashSpeedMultiplier = 4.5f;
    mDashCooldownDuration = 8.0f;

    UpdateMeshForTier();
}

Mage::~Mage() {}

bool Mage::Skill1() { return true; }
bool Mage::Skill2() { return true; }
void Mage::UpdateMeshForTier() {}
