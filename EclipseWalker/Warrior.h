#pragma once
#include "AudioManager.h"
#include "Player.h"

class Warrior : public Player
{
public:
    Warrior();
    ~Warrior() override;

    PlayerClass GetClassType() const override { return PlayerClass::Warrior; }
    float GetBasicAttackSpeedMultiplier() const override;

    bool Skill1() override;
    bool Skill2() override;

protected:
    void UpdateClassState(float dt) override;
    float GetSkillAttackLockDuration(int skillIndex) const override;
    void OnDashStarted() override;
    void OnBasicAttackStarted(int attackVariant) override;
    void OnSkillAttackStarted(int skillIndex) override;
    void UpdateMeshForTier() override;

private:
    void PlayRandomShout() const;
    void PlayFootstep();
    void StopSkill2MagicCircleSound();

private:
    float mFootstepTimer = 0.0f;
    float mBasicAttackSoundTimer = 0.0f;
    float mSkill2MagicCircleStopTimer = 0.0f;
    int mNextFootstepVariant = 1;
    bool mWasWalkingOnGround = false;
    bool mBasicAttackSoundPending = false;
    AudioManager::ClipHandle mSkill2MagicCircleHandle = AudioManager::InvalidClipHandle;
};
