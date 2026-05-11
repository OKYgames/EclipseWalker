#include "Archer.h"
#include <Windows.h>

Archer::Archer()
{
    // 궁수: 중간 체력/마나, 가장 빠른 기본 이속, 평범한 대쉬
    maxHp = 250.0f; hp = 250.0f;
    maxMp = 100.0f; mp = 100.0f;

    mMoveSpeed = 4.0f;
    mDashDuration = 0.25f;
    mDashSpeedMultiplier = 3.0f;

    UpdateMeshForTier();
}

Archer::~Archer() {}

void Archer::Skill1() {}
void Archer::Skill2() {}
void Archer::UpdateMeshForTier() {}
