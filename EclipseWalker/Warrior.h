#pragma once
#include "Player.h"

class Warrior : public Player
{
public:
    Warrior();
    ~Warrior() override;

    PlayerClass GetClassType() const override { return PlayerClass::Warrior; }

    // 전사 전용 스킬
    void Skill1() override; 
    void Skill2() override;

protected:
    // 전사의 승급 시 외형 변경
    void UpdateMeshForTier() override;
};