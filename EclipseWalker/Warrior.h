#pragma once
#include "Player.h"

class Warrior : public Player
{
public:
    Warrior();
    ~Warrior() override;

    PlayerClass GetClassType() const override { return PlayerClass::Warrior; }

    bool Skill1() override;
    bool Skill2() override;

protected:
    float GetSkillAttackLockDuration(int skillIndex) const override;
    void UpdateMeshForTier() override;
};
