#pragma once
#include "Player.h"

class Mage : public Player
{
public:
    Mage();
    ~Mage() override;

    PlayerClass GetClassType() const override { return PlayerClass::Mage; }

    bool Skill1() override;
    bool Skill2() override;

protected:
    void UpdateMeshForTier() override;
};
