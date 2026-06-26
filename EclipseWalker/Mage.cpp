#include "Mage.h"

#include "AudioManager.h"

#include <cmath>

namespace
{
    constexpr wchar_t kMageDashSound[] = L"Sounds\\Mage\\Mage_Dash.mp3";
    constexpr wchar_t kMageHealingLightSound[] = L"Sounds\\Mage\\Mage_HealingLight.mp3";
    constexpr wchar_t kMageMeteorMagicCircleSound[] = L"Sounds\\Mage\\Mage_Meteor_MagicCircle.mp3";
    constexpr wchar_t kMageMeteorFallSound[] = L"Sounds\\Mage\\Mage_Meteor_Fall.mp3";
    constexpr wchar_t kArcherFootstep1Sound[] = L"Sounds\\Archer\\Archer_Footstep_01.mp3";
    constexpr wchar_t kArcherFootstep2Sound[] = L"Sounds\\Archer\\Archer_Footstep_02.mp3";

    constexpr float kMageDashVolume = 0.10f;
    constexpr float kMageHealingLightVolume = 0.10f;
    constexpr float kMageMeteorMagicCircleVolume = 0.11f;
    constexpr float kMageMeteorFallVolume = 0.12f;
    constexpr float kArcherFootstepIntervalSeconds = 0.30f;
    constexpr float kArcherFootstepVolume = 0.08f;
    constexpr float kMageMeteorFallSoundDelaySeconds = 0.42f;
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

bool Mage::Skill1() { return true; }
bool Mage::Skill2() { return true; }

void Mage::UpdateClassState(float dt)
{
    if (IsDead())
    {
        mFootstepTimer = 0.0f;
        mWasWalkingOnGround = false;
        mMeteorFallSoundTimer = 0.0f;
        mMeteorFallSoundPending = false;
        return;
    }

    if (mMeteorFallSoundPending)
    {
        mMeteorFallSoundTimer -= dt;
        if (mMeteorFallSoundTimer <= 0.0f)
        {
            AudioManager::Get().PlayEffect(kMageMeteorFallSound, kMageMeteorFallVolume);
            mMeteorFallSoundTimer = 0.0f;
            mMeteorFallSoundPending = false;
        }
    }

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

void Mage::OnSkillAttackStarted(int skillIndex)
{
    if (skillIndex == 1)
    {
        AudioManager::Get().PlayEffect(kMageHealingLightSound, kMageHealingLightVolume);
        return;
    }

    if (skillIndex == 2)
    {
        AudioManager::Get().PlayEffect(kMageMeteorMagicCircleSound, kMageMeteorMagicCircleVolume);
        mMeteorFallSoundTimer = kMageMeteorFallSoundDelaySeconds;
        mMeteorFallSoundPending = true;
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
