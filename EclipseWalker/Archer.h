#pragma once
#include "Player.h"

class Archer : public Player
{
public:
    Archer();
    ~Archer() override;

    PlayerClass GetClassType() const override { return PlayerClass::Archer; }

    bool Skill1() override;
    bool Skill2() override;

protected:
    void UpdateMeshForTier() override;
};
