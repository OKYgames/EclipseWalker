#include "Mage.h"
#include <Windows.h>

Mage::Mage()
{
    // 마법사: 낮은 체력, 높은 마나, 짧고 매우 빠른 대쉬 (점멸 느낌)
    maxHp = 150.0f; hp = 150.0f;
    maxMp = 300.0f; mp = 300.0f;

    mMoveSpeed = 3.5f;
    mDashDuration = 0.2f;
    mDashSpeedMultiplier = 4.5f;

    UpdateMeshForTier();
}

Mage::~Mage() {}

void Mage::Skill1() {}
void Mage::Skill2() {}
void Mage::UpdateMeshForTier() {}
