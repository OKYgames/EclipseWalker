#pragma once
#include "Player.h"

class Mage : public Player
{
public:
    Mage();
    ~Mage() override;

    PlayerClass GetClassType() const override { return PlayerClass::Mage; }

    void Skill1() override;
    void Skill2() override;

protected:
    void UpdateMeshForTier() override;
};