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
    void UpdateClassState(float dt) override;
    void OnDashStarted() override;
    void OnBasicAttackStarted(int attackVariant) override;
    void OnSkillAttackStarted(int skillIndex) override;
    void UpdateMeshForTier() override;

private:
    void PlayFootstep();

    float mFootstepTimer = 0.0f;
    int mNextFootstepVariant = 1;
    bool mWasWalkingOnGround = false;
    float mMeteorFallSoundTimer = 0.0f;
    bool mMeteorFallSoundPending = false;
};
