#pragma once
#include "Player.h"

class Archer : public Player
{
public:
    Archer();
    ~Archer() override;

    PlayerClass GetClassType() const override { return PlayerClass::Archer; }

    void Skill1() override;
    void Skill2() override;

protected:
    void UpdateMeshForTier() override;
};