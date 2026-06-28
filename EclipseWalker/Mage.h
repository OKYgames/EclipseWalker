#pragma once
#include "Player.h"

namespace MageAnimationTiming
{
    // 0.0f = animation start, 1.0f = animation end.
    inline constexpr float kBasicAttackSoundProgress = 0.63f;
    inline constexpr float kBasicAttackEffectProgress = 0.6f;

    inline constexpr float kSkillQSoundProgress = 0.00f;
    inline constexpr float kSkillQEffectProgress = 0.00f;

    inline constexpr float kSkillEMagicCircleSoundProgress = 0.00f;
    inline constexpr float kSkillEMeteorFallSoundProgress = 0.17f;
    inline constexpr float kSkillEMeteorImpactProgress = 0.45f;

    inline float DelayFromProgress(float animationDuration, float progress)
    {
        const float clampedProgress = progress < 0.0f ? 0.0f : (progress > 1.0f ? 1.0f : progress);
        return animationDuration > 0.0f ? animationDuration * clampedProgress : 0.0f;
    }
}

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
    void ScheduleSound(
        float delay,
        float& timer,
        bool& pending,
        const wchar_t* filename,
        float volume);
    void UpdatePendingSound(
        float dt,
        float& timer,
        bool& pending,
        const wchar_t* filename,
        float volume);

    float mFootstepTimer = 0.0f;
    int mNextFootstepVariant = 1;
    bool mWasWalkingOnGround = false;
    float mBasicAttackSoundTimer = 0.0f;
    bool mBasicAttackSoundPending = false;
    float mHealingLightSoundTimer = 0.0f;
    bool mHealingLightSoundPending = false;
    float mMeteorMagicCircleSoundTimer = 0.0f;
    bool mMeteorMagicCircleSoundPending = false;
    float mMeteorFallSoundTimer = 0.0f;
    bool mMeteorFallSoundPending = false;
};
