#include "Warrior.h"

#include "AudioManager.h"

#include <cmath>
#include <cstdlib>
#include <Windows.h>

namespace
{
    constexpr wchar_t kWarriorDashSound[] = L"Sounds\\Warrior\\Warrior_Dash.mp3";
    constexpr wchar_t kWarriorBasicAttackSound[] = L"Sounds\\Warrior\\Warrior_BasicAttack.mp3";
    constexpr wchar_t kWarriorSkill1JumpSound[] = L"Sounds\\Warrior\\Warrior_EarthquakeSlam_Jump.mp3";
    constexpr wchar_t kWarriorSkill2MagicCircleSound[] = L"Sounds\\Warrior\\Warrior_GreatswordSummon_MagicCircle.mp3";
    constexpr wchar_t kWarriorFootstep1Sound[] = L"Sounds\\Warrior\\Warrior_Footstep_01.mp3";
    constexpr wchar_t kWarriorFootstep2Sound[] = L"Sounds\\Warrior\\Warrior_Footstep_02.mp3";
    constexpr wchar_t kWarriorShout1Sound[] = L"Sounds\\Warrior\\Warrior_Shout_01.mp3";
    constexpr wchar_t kWarriorShout2Sound[] = L"Sounds\\Warrior\\Warrior_Shout_02.mp3";
    constexpr float kWarriorFootstepIntervalSeconds = 0.34f;
    constexpr float kWarriorBasicAttackSoundDelaySeconds = 0.25f;
    constexpr float kWarriorDashVolume = 0.12f;
    constexpr float kWarriorBasicAttackVolume = 0.11f;
    constexpr float kWarriorSkillCastVolume = 0.12f;
    constexpr float kWarriorFootstepVolume = 0.09f;
    constexpr float kWarriorShoutVolume = 0.10f;
}

Warrior::Warrior()
{
    maxHp = 500.0f;
    hp = 500.0f;
    maxMp = 50.0f;
    mp = 50.0f;

    mMoveSpeed = 3.4f;
    mDashDuration = 0.35f;
    mDashSpeedMultiplier = 2.5f;
    mDashCooldownDuration = 8.0f;

    UpdateMeshForTier();
}

Warrior::~Warrior() {}

bool Warrior::Skill1()
{
    OutputDebugStringA("[Warrior] Skill1: earthshatter triggered\n");
    return true;
}

bool Warrior::Skill2()
{
    OutputDebugStringA("[Warrior] Skill2: greatsword swing triggered\n");
    return true;
}

void Warrior::UpdateClassState(float dt)
{
    if (mBasicAttackSoundPending)
    {
        if (mIsDead)
        {
            mBasicAttackSoundPending = false;
            mBasicAttackSoundTimer = 0.0f;
        }
        else
        {
            mBasicAttackSoundTimer -= dt;
            if (mBasicAttackSoundTimer <= 0.0f)
            {
                AudioManager::Get().PlayEffect(
                    kWarriorBasicAttackSound,
                    kWarriorBasicAttackVolume);
                mBasicAttackSoundPending = false;
                mBasicAttackSoundTimer = 0.0f;
            }
        }
    }

    if (mSkill2MagicCircleHandle != AudioManager::InvalidClipHandle)
    {
        mSkill2MagicCircleStopTimer -= dt;
        if (mSkill2MagicCircleStopTimer <= 0.0f || mIsDead)
        {
            StopSkill2MagicCircleSound();
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
        mFootstepTimer = kWarriorFootstepIntervalSeconds;
        mWasWalkingOnGround = true;
        return;
    }

    mFootstepTimer -= dt;
    if (mFootstepTimer <= 0.0f)
    {
        PlayFootstep();
        mFootstepTimer = kWarriorFootstepIntervalSeconds;
    }
}

float Warrior::GetSkillAttackLockDuration(int skillIndex) const
{
    return Player::GetSkillAttackLockDuration(skillIndex);
}

void Warrior::OnDashStarted()
{
    AudioManager::Get().PlayEffect(kWarriorDashSound, kWarriorDashVolume);
}

void Warrior::OnBasicAttackStarted(int attackVariant)
{
    (void)attackVariant;
    mBasicAttackSoundTimer = kWarriorBasicAttackSoundDelaySeconds;
    mBasicAttackSoundPending = true;
}

void Warrior::OnSkillAttackStarted(int skillIndex)
{
    if (skillIndex == 1)
    {
        AudioManager::Get().PlayEffect(kWarriorSkill1JumpSound, kWarriorSkillCastVolume);
        PlayRandomShout();
        return;
    }

    if (skillIndex == 2)
    {
        StopSkill2MagicCircleSound();
        mSkill2MagicCircleHandle = AudioManager::Get().PlayEffect(
            kWarriorSkill2MagicCircleSound,
            kWarriorSkillCastVolume);
        mSkill2MagicCircleStopTimer = mAttackAnimationTimer;
        PlayRandomShout();
    }
}

void Warrior::UpdateMeshForTier()
{
    if (mPlayerObject == nullptr) return;

    if (mCurrentTier == ClassTier::Tier1) {
        OutputDebugStringA("[Warrior Model] Tier1 loaded\n");
    }
    else if (mCurrentTier == ClassTier::Tier2) {
        OutputDebugStringA("[Warrior Model] Tier2 loaded\n");
    }
    else if (mCurrentTier == ClassTier::Tier3) {
        OutputDebugStringA("[Warrior Model] Tier3 loaded\n");
    }
}

void Warrior::PlayRandomShout() const
{
    AudioManager::Get().PlayEffect(
        (std::rand() % 2) == 0 ? kWarriorShout1Sound : kWarriorShout2Sound,
        kWarriorShoutVolume);
}

void Warrior::PlayFootstep()
{
    AudioManager::Get().PlayEffect(
        mNextFootstepVariant == 1 ? kWarriorFootstep1Sound : kWarriorFootstep2Sound,
        kWarriorFootstepVolume);
    mNextFootstepVariant = (mNextFootstepVariant == 1) ? 2 : 1;
}

void Warrior::StopSkill2MagicCircleSound()
{
    if (mSkill2MagicCircleHandle == AudioManager::InvalidClipHandle)
    {
        mSkill2MagicCircleStopTimer = 0.0f;
        return;
    }

    AudioManager::Get().StopEffect(mSkill2MagicCircleHandle);
    mSkill2MagicCircleHandle = AudioManager::InvalidClipHandle;
    mSkill2MagicCircleStopTimer = 0.0f;
}
