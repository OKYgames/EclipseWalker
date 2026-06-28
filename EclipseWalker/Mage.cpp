#include "Mage.h"

#include "AudioManager.h"

#include <cmath>

namespace
{
    constexpr wchar_t kMageBasicAttackSound[] = L"Sounds\\Mage\\Mage_BasicAttack.mp3";
    constexpr wchar_t kMageDashSound[] = L"Sounds\\Mage\\Mage_Dash.mp3";
    constexpr wchar_t kMageHealingLightSound[] = L"Sounds\\Mage\\Mage_HealingLight.mp3";
    constexpr wchar_t kMageMeteorMagicCircleSound[] = L"Sounds\\Mage\\Mage_Meteor_MagicCircle.mp3";
    constexpr wchar_t kMageMeteorFallSound[] = L"Sounds\\Mage\\Mage_Meteor_Fall.mp3";
    constexpr wchar_t kArcherFootstep1Sound[] = L"Sounds\\Archer\\Archer_Footstep_01.mp3";
    constexpr wchar_t kArcherFootstep2Sound[] = L"Sounds\\Archer\\Archer_Footstep_02.mp3";

    constexpr float kMageBasicAttackVolume = 0.05f;
    constexpr float kMageDashVolume = 0.10f;
    constexpr float kMageHealingLightVolume = 0.10f;
    constexpr float kMageMeteorMagicCircleVolume = 0.11f;
    constexpr float kMageMeteorFallVolume = 0.12f;
    constexpr float kArcherFootstepIntervalSeconds = 0.30f;
    constexpr float kArcherFootstepVolume = 0.08f;
}

Mage::Mage()
{
    maxHp = 150.0f;
    hp = 150.0f;
    maxMp = 300.0f;
    mp = 300.0f;

    mMoveSpeed = 3.6f;
    mDashDuration = 0.2f;
    mDashSpeedMultiplier = 4.5f;
    mDashCooldownDuration = 8.0f;

    UpdateMeshForTier();
}

Mage::~Mage() {}

bool Mage::Skill1()
{
    return true;
}
bool Mage::Skill2() { return true; }

void Mage::UpdateClassState(float dt)
{
    if (IsDead())
    {
        mFootstepTimer = 0.0f;
        mWasWalkingOnGround = false;
        mBasicAttackSoundTimer = 0.0f;
        mBasicAttackSoundPending = false;
        mHealingLightSoundTimer = 0.0f;
        mHealingLightSoundPending = false;
        mMeteorMagicCircleSoundTimer = 0.0f;
        mMeteorMagicCircleSoundPending = false;
        mMeteorFallSoundTimer = 0.0f;
        mMeteorFallSoundPending = false;
        return;
    }

    UpdatePendingSound(
        dt,
        mBasicAttackSoundTimer,
        mBasicAttackSoundPending,
        kMageBasicAttackSound,
        kMageBasicAttackVolume);
    UpdatePendingSound(
        dt,
        mHealingLightSoundTimer,
        mHealingLightSoundPending,
        kMageHealingLightSound,
        kMageHealingLightVolume);
    UpdatePendingSound(
        dt,
        mMeteorMagicCircleSoundTimer,
        mMeteorMagicCircleSoundPending,
        kMageMeteorMagicCircleSound,
        kMageMeteorMagicCircleVolume);
    UpdatePendingSound(
        dt,
        mMeteorFallSoundTimer,
        mMeteorFallSoundPending,
        kMageMeteorFallSound,
        kMageMeteorFallVolume);

    const bool isWalkingOnGround =
        mIsGrounded &&
        !mIsDashing &&
        !mIsDead &&
        !mIsSkillLeaping &&
        mAttackAnimationTimer <= 0.0f &&
        (std::fabs(mMoveDir.x) > 0.01f || std::fabs(mMoveDir.z) > 0.01f);

    if (!isWalkingOnGround)
    {
        mFootstepTimer = 0.0f;
        mWasWalkingOnGround = false;
        return;
    }

    if (!mWasWalkingOnGround)
    {
        PlayFootstep();
        mFootstepTimer = kArcherFootstepIntervalSeconds;
        mWasWalkingOnGround = true;
        return;
    }

    mFootstepTimer -= dt;
    if (mFootstepTimer <= 0.0f)
    {
        PlayFootstep();
        mFootstepTimer = kArcherFootstepIntervalSeconds;
    }
}

void Mage::OnDashStarted()
{
    AudioManager::Get().PlayEffect(kMageDashSound, kMageDashVolume);
}

void Mage::OnBasicAttackStarted(int attackVariant)
{
    (void)attackVariant;
    ScheduleSound(
        MageAnimationTiming::DelayFromProgress(
            GetAttackAnimationRemaining(),
            MageAnimationTiming::kBasicAttackSoundProgress),
        mBasicAttackSoundTimer,
        mBasicAttackSoundPending,
        kMageBasicAttackSound,
        kMageBasicAttackVolume);
}

void Mage::OnSkillAttackStarted(int skillIndex)
{
    if (skillIndex == 1)
    {
        ScheduleSound(
            MageAnimationTiming::DelayFromProgress(
                GetAttackAnimationRemaining(),
                MageAnimationTiming::kSkillQSoundProgress),
            mHealingLightSoundTimer,
            mHealingLightSoundPending,
            kMageHealingLightSound,
            kMageHealingLightVolume);
        return;
    }

    if (skillIndex == 2)
    {
        const float animationDuration = GetAttackAnimationRemaining();
        ScheduleSound(
            MageAnimationTiming::DelayFromProgress(
                animationDuration,
                MageAnimationTiming::kSkillEMagicCircleSoundProgress),
            mMeteorMagicCircleSoundTimer,
            mMeteorMagicCircleSoundPending,
            kMageMeteorMagicCircleSound,
            kMageMeteorMagicCircleVolume);
        ScheduleSound(
            MageAnimationTiming::DelayFromProgress(
                animationDuration,
                MageAnimationTiming::kSkillEMeteorFallSoundProgress),
            mMeteorFallSoundTimer,
            mMeteorFallSoundPending,
            kMageMeteorFallSound,
            kMageMeteorFallVolume);
    }
}

void Mage::ScheduleSound(
    float delay,
    float& timer,
    bool& pending,
    const wchar_t* filename,
    float volume)
{
    timer = delay;
    pending = delay > 0.0f;
    if (!pending)
    {
        AudioManager::Get().PlayEffect(filename, volume);
    }
}

void Mage::UpdatePendingSound(
    float dt,
    float& timer,
    bool& pending,
    const wchar_t* filename,
    float volume)
{
    if (!pending)
    {
        return;
    }

    timer -= dt;
    if (timer <= 0.0f)
    {
        AudioManager::Get().PlayEffect(filename, volume);
        timer = 0.0f;
        pending = false;
    }
}

void Mage::PlayFootstep()
{
    AudioManager::Get().PlayEffect(
        mNextFootstepVariant == 1 ? kArcherFootstep1Sound : kArcherFootstep2Sound,
        kArcherFootstepVolume);
    mNextFootstepVariant = (mNextFootstepVariant == 1) ? 2 : 1;
}

void Mage::UpdateMeshForTier() {}
