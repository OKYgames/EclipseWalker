#include "Player.h"
#include <Windows.h> 
#include "NetworkManager.h"
#include "DebugConfig.h"
#include "Scene.h"
#include "SkeletalAnimationComponent.h"
#include <algorithm> 
#include <cmath>
#include <cstdlib>
#include <sstream>

using namespace DirectX;

namespace
{
    constexpr float kIdleWalkBlendDuration = 0.15f;
    constexpr float kAttackStartBlendDuration = 0.08f;
    constexpr float kAttackEndBlendDuration = 0.12f;
    constexpr float kDashStartBlendDuration = 0.05f;
    constexpr float kDashEndBlendDuration = 0.10f;
    constexpr float kDeathBlendDuration = 0.14f;
    constexpr float kRespawnBlendDuration = 0.18f;
    constexpr float kAttackAnimationSpeed = 1.25f;
    constexpr float kMageQAnimationPlaybackSpeed = kAttackAnimationSpeed * 1.50f;
    constexpr float kMageEAnimationPlaybackSpeed = kAttackAnimationSpeed * 0.80f;
    constexpr float kArcherEAnimationPlaybackSpeed = kAttackAnimationSpeed * 1.30f;
    constexpr float kAttack1AnimationDuration = (45.0f / 30.0f) / kAttackAnimationSpeed;
    constexpr float kAttack2AnimationDuration = (50.0f / 30.0f) / kAttackAnimationSpeed;
    constexpr float kWarriorQForwardDistance = 2.5f;
    constexpr float kWarriorQMoveStartClipFraction = 0.11f;
    constexpr float kWarriorQMoveEndClipFraction = 0.66f;
    constexpr float kWarriorQStopDistance = 0.6f;
    constexpr float kWarriorQMaxVisualArcHeight = 0.2f;
    constexpr float kWarriorEForwardDistance = 0.7f;
    constexpr float kWarriorEMoveEndClipFraction = kWarriorQMoveEndClipFraction;
    constexpr float kWarriorQEarlyClipFraction = 0.40f;
    constexpr float kWarriorQEarlyPlaybackSpeed = kAttackAnimationSpeed;
    constexpr float kWarriorQLatePlaybackSpeed = 1.65f;
    constexpr float kWarriorEEarlyClipFraction = 0.40f;
    constexpr float kWarriorEEarlyPlaybackSpeed = 1.0f;
    constexpr float kWarriorELatePlaybackSpeed = kAttackAnimationSpeed;
    constexpr float kFacingTurnSpeed = 7.5f;
    constexpr float kArcherBasicAttackFastStart = 0.10f;
    constexpr float kArcherBasicAttackFastEnd = 0.40f;
    constexpr float kArcherBasicAttackSlowStart = 0.85f;
    constexpr float kArcherBasicAttackSlowEnd = 0.90f;
    constexpr float kArcherBasicAttackFastScale = 1.50f;
    constexpr float kArcherBasicAttackSlowScale =
        (kArcherBasicAttackSlowEnd - kArcherBasicAttackSlowStart) /
        ((kArcherBasicAttackSlowEnd - kArcherBasicAttackSlowStart) +
            ((kArcherBasicAttackFastEnd - kArcherBasicAttackFastStart) -
                ((kArcherBasicAttackFastEnd - kArcherBasicAttackFastStart) / kArcherBasicAttackFastScale)));

    float SmoothStep(float value)
    {
        const float t = std::clamp(value, 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    }

    float GetArcherBasicAttackPlaybackSpeed(float clipProgress)
    {
        if (clipProgress >= kArcherBasicAttackFastStart && clipProgress < kArcherBasicAttackFastEnd)
        {
            return kAttackAnimationSpeed * kArcherBasicAttackFastScale;
        }

        if (clipProgress >= kArcherBasicAttackSlowStart && clipProgress < kArcherBasicAttackSlowEnd)
        {
            return kAttackAnimationSpeed * kArcherBasicAttackSlowScale;
        }

        return kAttackAnimationSpeed;
    }

    float GetWarriorSkillClipProgress(
        float elapsed,
        float clipDuration,
        float speedUpTime,
        float earlyClipFraction,
        float earlyPlaybackSpeed,
        float latePlaybackSpeed)
    {
        if (clipDuration <= 0.0f)
        {
            return 0.0f;
        }
        if (elapsed <= speedUpTime)
        {
            return std::clamp(
                elapsed * earlyPlaybackSpeed / clipDuration,
                0.0f,
                earlyClipFraction);
        }

        return std::clamp(
            earlyClipFraction +
                (elapsed - speedUpTime) * latePlaybackSpeed / clipDuration,
            earlyClipFraction,
            1.0f);
    }

    float GetMoveProgress(float clipProgress, float moveStartFraction, float moveEndFraction)
    {
        const float moveDuration = moveEndFraction - moveStartFraction;
        if (moveDuration <= 0.0001f)
        {
            return clipProgress >= moveEndFraction ? 1.0f : 0.0f;
        }

        return std::clamp(
            (clipProgress - moveStartFraction) / moveDuration,
            0.0f,
            1.0f);
    }

    float NormalizeAngle(float angle)
    {
        while (angle > XM_PI)
        {
            angle -= XM_2PI;
        }
        while (angle < -XM_PI)
        {
            angle += XM_2PI;
        }
        return angle;
    }

    float MoveAngleTowards(float current, float target, float maxDelta)
    {
        const float delta = NormalizeAngle(target - current);
        if (std::fabs(delta) <= maxDelta)
        {
            return NormalizeAngle(target);
        }

        return NormalizeAngle(current + std::clamp(delta, -maxDelta, maxDelta));
    }

    XMFLOAT3 Lerp3(const XMFLOAT3& a, const XMFLOAT3& b, float t)
    {
        return
        {
            a.x + (b.x - a.x) * t,
            a.y + (b.y - a.y) * t,
            a.z + (b.z - a.z) * t
        };
    }

    constexpr int kLevel2ExperienceThreshold = 30;
    constexpr int kLevel3ExperienceThreshold = 60;

    int GetExperienceThresholdForLevel(int level)
    {
        switch (std::clamp(level, Player::MinProgressionLevel, Player::MaxProgressionLevel))
        {
        case 3:
            return kLevel3ExperienceThreshold;
        case 2:
            return kLevel2ExperienceThreshold;
        case 1:
        default:
            return 0;
        }
    }

    ClassTier TierFromLevel(int level)
    {
        switch (std::clamp(level, Player::MinProgressionLevel, Player::MaxProgressionLevel))
        {
        case 3:
            return ClassTier::Tier3;
        case 2:
            return ClassTier::Tier2;
        case 1:
        default:
            return ClassTier::Tier1;
        }
    }

    int LevelFromTier(ClassTier tier)
    {
        const int rawLevel = static_cast<int>(tier);
        return std::clamp(rawLevel, Player::MinProgressionLevel, Player::MaxProgressionLevel);
    }
}

Player::Player()
{
    mTheta = 1.5f * XM_PI;
    mPhi = DefaultCameraPhi;
    mRadius = 5.0f;
}

Player::~Player() {}

void Player::Initialize(GameObject* playerObj, Camera* cam)
{
    mPlayerObject = playerObj;
    mCamera = cam;
    hp = GetMaxHP();
    mp = GetMaxMP();
    mIsDead = false;
    mDeathAnimationStarted = false;
    mRespawnAnimationPlaying = false;
    mRespawnAnimationTimer = 0.0f;

    // 초기 충돌 박스 설정
    mCollider.Extents = XMFLOAT3(DefaultColliderHalfWidth, DefaultColliderHalfHeight, DefaultColliderHalfWidth);

    mMoveDir = { 0.0f, 0.0f, 0.0f };
    mFacingRotY = 0.0f;
    mTargetFacingRotY = mFacingRotY;
    mAnimationState = PlayerAnimationState::Walk;
    mLastSentAnimationState = PlayerAnimationState::Walk;
    mHasSentMovementState = false;
    mMovePacketSendTimer = DebugConfig::kPlayerMoveSendIntervalSeconds;
    mLastSentPosition = GetPosition();
    mLastSentRotY = mFacingRotY;
    mLastMapSystem = nullptr;
    mIsSkillLeaping = false;
    mSkillLeapIndex = 0;
    mSkillLeapElapsed = 0.0f;
    mSkillLeapDuration = 0.0f;
    mSkillLeapArcHeight = 0.0f;
    mSkillLeapStartPosition = GetPosition();
    mSkillLeapTargetPosition = GetPosition();
    mHasQueuedSkillAttackOverride = false;
    mQueuedSkillAttackIndex = 0;
    mQueuedSkillAttackOrigin = GetPosition();
    mQueuedSkillAttackDelay = 0.0f;
    mWarriorQMotionActive = false;
    mWarriorQMovedThisFrame = false;
    mWarriorQMotionElapsed = 0.0f;
    mWarriorQMotionDuration = 0.0f;
    mWarriorQClipDuration = 0.0f;
    mWarriorQSpeedUpTime = 0.0f;
    mWarriorSkillVisualArcHeight = 0.0f;
    mBasePositionOffset = (mPlayerObject != nullptr)
        ? mPlayerObject->GetPositionOffset()
        : XMFLOAT3(0.0f, 0.0f, 0.0f);
    ApplyVisualPositionOffset(0.0f);
    UpdateAnimationState();
}

void Player::Update(const GameTimer& gt, MapSystem* mapSystem)
{
    // =========================================================
    // 대쉬(Dash) 타이머 관리
    float dt = gt.DeltaTime();
    mLastMapSystem = mapSystem;
    mWarriorQMovedThisFrame = false;

    // 대쉬 쿨타임 감소
    if (mDashCooldown > 0.0f) {
        mDashCooldown -= dt;
        if (mDashCooldown < 0.0f) {
            mDashCooldown = 0.0f;
        }
    }

    UpdateClassState(dt);

    // 대쉬 지속 시간 감소 및 종료 체크
    if (mIsDashing) {
        mDashTimer -= dt;
        if (mDashTimer <= 0.0f) {
            mIsDashing = false; // 대쉬 종료
        }
    }

    if (mAttackAnimationTimer > 0.0f)
    {
        mAttackAnimationTimer -= dt;
        if (mAttackAnimationTimer < 0.0f)
        {
            mAttackAnimationTimer = 0.0f;
        }
    }

    if (mWarriorQMotionActive && mPlayerObject != nullptr)
    {
        if (auto* animation = mPlayerObject->GetSkeletalAnimation())
        {
            animation->SetPlaybackSpeed(
                mWarriorQMotionElapsed >= mWarriorQSpeedUpTime
                    ? mWarriorSkillLatePlaybackSpeed
                    : mWarriorSkillEarlyPlaybackSpeed);
        }
    }

    if (mArcherBasicAttackRetimingActive)
    {
        if (mAttackAnimationTimer > 0.0f && mPlayerObject != nullptr)
        {
            if (auto* animation = mPlayerObject->GetSkeletalAnimation())
            {
                animation->SetPlaybackSpeed(
                    GetArcherBasicAttackPlaybackSpeed(animation->GetCurrentAnimationProgress()) *
                    (std::max)(GetBasicAttackSpeedMultiplier(), 1.0f));
            }
        }
        else
        {
            mArcherBasicAttackRetimingActive = false;
        }
    }
    // =========================================================

    if (mIsDead)
    {
        mMoveDir = { 0.0f, 0.0f, 0.0f };
        mIsDashing = false;
        mIsSkillLeaping = false;
        mHasQueuedSkillAttackOverride = false;
        mAttackAnimationTimer = 0.0f;
        mAttackAnimationPlaying = false;
        mArcherBasicAttackRetimingActive = false;
        mWarriorQMotionActive = false;
        mWarriorSkillVisualArcHeight = 0.0f;
        ApplyVisualPositionOffset(0.0f);
        EnterDeathAnimationState();
        ApplyPhysics(gt, mapSystem);
        UpdateCamera(mapSystem);
        return;
    }

    if (mRespawnAnimationPlaying)
    {
        mMoveDir = { 0.0f, 0.0f, 0.0f };
        mRespawnAnimationTimer -= gt.DeltaTime();
        if (mRespawnAnimationTimer <= 0.0f)
        {
            mRespawnAnimationTimer = 0.0f;
            mRespawnAnimationPlaying = false;
            mAnimationState = PlayerAnimationState::Walk;
            UpdateAnimationState();
        }

        ApplyPhysics(gt, mapSystem);
        UpdateCamera(mapSystem);
        return;
    }

    HandleInput();
    UpdateAnimationState();
    ApplyPhysics(gt, mapSystem);
    UpdateCamera(mapSystem);

    if (hp < 0.0f)
    {
        hp = 0.0f;
    }
    mMovePacketSendTimer += dt;

    const bool animationChanged = !mHasSentMovementState || mLastSentAnimationState != mAnimationState;
    const int currentClassType = static_cast<int>(GetClassType());
    const int currentPlayerLevel = GetLevel();
    const bool visualStateChanged =
        !mHasSentMovementState ||
        mLastSentClassType != currentClassType ||
        mLastSentPlayerLevel != currentPlayerLevel;
    XMFLOAT3 currentPos = GetPosition();
    const float dx = currentPos.x - mLastSentPosition.x;
    const float dy = currentPos.y - mLastSentPosition.y;
    const float dz = currentPos.z - mLastSentPosition.z;
    const float moveEpsilonSq =
        DebugConfig::kPlayerMovePositionEpsilon * DebugConfig::kPlayerMovePositionEpsilon;
    const bool positionChangedEnough = (dx * dx + dy * dy + dz * dz) >= moveEpsilonSq;
    const bool rotationChangedEnough =
        std::fabs(NormalizeAngle(mFacingRotY - mLastSentRotY)) >= DebugConfig::kPlayerMoveRotationEpsilon;
    const bool timedTransformUpdate =
        mMovePacketSendTimer >= DebugConfig::kPlayerMoveSendIntervalSeconds &&
        (positionChangedEnough || rotationChangedEnough);

    if (animationChanged || visualStateChanged || timedTransformUpdate)
    {
        NetworkManager::Get()->SendPlayerMove(
            currentPos.x,
            currentPos.y,
            currentPos.z,
            mFacingRotY,
            static_cast<int>(mAnimationState),
            currentClassType,
            currentPlayerLevel);
        mLastSentAnimationState = mAnimationState;
        mLastSentClassType = currentClassType;
        mLastSentPlayerLevel = currentPlayerLevel;
        mHasSentMovementState = true;
        mMovePacketSendTimer = 0.0f;
        mLastSentPosition = currentPos;
        mLastSentRotY = mFacingRotY;
    }
}

void Player::ForceSendNetworkState()
{
    const XMFLOAT3 currentPos = GetPosition();
    const int currentClassType = static_cast<int>(GetClassType());
    const int currentPlayerLevel = GetLevel();

    NetworkManager::Get()->SendPlayerMove(
        currentPos.x,
        currentPos.y,
        currentPos.z,
        mFacingRotY,
        static_cast<int>(mAnimationState),
        currentClassType,
        currentPlayerLevel);

    mLastSentAnimationState = mAnimationState;
    mLastSentClassType = currentClassType;
    mLastSentPlayerLevel = currentPlayerLevel;
    mHasSentMovementState = true;
    mMovePacketSendTimer = 0.0f;
    mLastSentPosition = currentPos;
    mLastSentRotY = mFacingRotY;
}

void Player::HandleInput()
{
    if (mIsDashing || mIsSkillLeaping) return;

    mMoveDir = { 0.0f, 0.0f, 0.0f };

    if (mAttackAnimationTimer > 0.0f)
    {
        return;
    }

    if (gIsChatInputActive)
        return;

    if (GetForegroundWindow() != GetActiveWindow())
        return;

    if (GetAsyncKeyState(VK_SPACE) & 0x8000)
    {
        Dash();
    }

    // 1. 키보드 입력 (앞뒤/좌우)
    float inputZ = 0.0f; // W, S
    float inputX = 0.0f; // A, D

    if (GetAsyncKeyState('W') & 0x8000) inputZ += 0.5f;
    if (GetAsyncKeyState('S') & 0x8000) inputZ -= 0.5f;
    if (GetAsyncKeyState('D') & 0x8000) inputX += 0.5f;
    if (GetAsyncKeyState('A') & 0x8000) inputX -= 0.5f;

    // 입력이 없으면 종료
    if (inputZ == 0.0f && inputX == 0.0f) return;

    // 2. 카메라 기준 방향 합성
    XMVECTOR camLook = XMVector3Normalize(XMVectorSetY(mCamera->GetLook(), 0.0f));
    XMVECTOR camRight = XMVector3Normalize(XMVectorSetY(mCamera->GetRight(), 0.0f));

    XMVECTOR targetDir = XMVector3Normalize((camLook * inputZ) + (camRight * inputX));
    XMStoreFloat3(&mMoveDir, targetDir);
}

void Player::UpdateAnimationState()
{
    if (mIsDead || mRespawnAnimationPlaying)
    {
        return;
    }

    if (mPlayerObject == nullptr)
    {
        return;
    }

    auto* animation = mPlayerObject->GetSkeletalAnimation();
    if (animation == nullptr || !animation->IsLoaded())
    {
        return;
    }

    const bool attackJustEnded = mAttackAnimationPlaying && mAttackAnimationTimer <= 0.0f;
    if (mAttackAnimationTimer > 0.0f)
    {
        return;
    }

    if (attackJustEnded)
    {
        mAttackAnimationPlaying = false;
        mArcherBasicAttackRetimingActive = false;
    }

    const bool isMoving = mMoveDir.x != 0.0f || mMoveDir.z != 0.0f;
    const PlayerAnimationState nextState = mIsDashing
        ? PlayerAnimationState::Dash
        : (isMoving ? PlayerAnimationState::Walk : PlayerAnimationState::Idle);
    if (!attackJustEnded && mAnimationState == nextState)
    {
        return;
    }

    const char* clipName = "FemaleIdle";
    if (nextState == PlayerAnimationState::Walk)
    {
        clipName = "FemaleWalk";
    }
    else if (nextState == PlayerAnimationState::Dash)
    {
        clipName = "FemaleDash";
    }

    const bool blendIdleAndWalk =
        !attackJustEnded &&
        ((mAnimationState == PlayerAnimationState::Idle && nextState == PlayerAnimationState::Walk) ||
            (mAnimationState == PlayerAnimationState::Walk && nextState == PlayerAnimationState::Idle));
    float blendDuration = 0.0f;
    if (attackJustEnded)
    {
        blendDuration = kAttackEndBlendDuration;
    }
    else if (mAnimationState == PlayerAnimationState::Dash)
    {
        blendDuration = kDashEndBlendDuration;
    }
    else if (nextState == PlayerAnimationState::Dash)
    {
        blendDuration = kDashStartBlendDuration;
    }
    else if (blendIdleAndWalk)
    {
        blendDuration = kIdleWalkBlendDuration;
    }

    float playbackSpeed = 1.0f;
    if (nextState == PlayerAnimationState::Dash)
    {
        const float clipDuration = animation->GetClipDurationSeconds(clipName);
        if (clipDuration > 0.0f && mDashDuration > 0.0f)
        {
            playbackSpeed = clipDuration / mDashDuration;
        }
    }

    if (animation->Play(clipName, blendDuration, playbackSpeed))
    {
        mAnimationState = nextState;
    }
}

void Player::EnterDeathAnimationState()
{
    if (mDeathAnimationStarted)
    {
        return;
    }

    mDeathAnimationStarted = true;
    mRespawnAnimationPlaying = false;
    mRespawnAnimationTimer = 0.0f;

    if (mPlayerObject == nullptr)
    {
        return;
    }

    if (auto* animation = mPlayerObject->GetSkeletalAnimation())
    {
        animation->Play("FemaleDeath", kDeathBlendDuration, 1.0f, false);
    }
}

void Player::StartRespawnAnimation()
{
    mDeathAnimationStarted = false;
    mRespawnAnimationPlaying = false;
    mRespawnAnimationTimer = 0.0f;

    if (mPlayerObject == nullptr)
    {
        return;
    }

    auto* animation = mPlayerObject->GetSkeletalAnimation();
    if (animation == nullptr || !animation->IsLoaded())
    {
        return;
    }

    const float clipDuration = animation->GetClipDurationSeconds("FemaleRespawn");
    if (clipDuration > 0.0f &&
        animation->Play("FemaleRespawn", kRespawnBlendDuration, 1.0f, false))
    {
        mRespawnAnimationPlaying = true;
        mRespawnAnimationTimer = clipDuration;
    }
}

bool Player::PlayRandomBasicAttack()
{
    if (mPlayerObject == nullptr || mIsDead || mRespawnAnimationPlaying || mIsDashing || mIsSkillLeaping || mAttackAnimationTimer > 0.0f)
    {
        return false;
    }

    auto* animation = mPlayerObject->GetSkeletalAnimation();
    if (animation == nullptr || !animation->IsLoaded())
    {
        return false;
    }

    const bool useAttack2 = GetClassType() != PlayerClass::Archer && (std::rand() % 2) == 0;
    const char* clipName = useAttack2 ? "FemaleAttack2" : "FemaleAttack1";
    const float basicAttackSpeedMultiplier =
        GetClassType() == PlayerClass::Archer
            ? (std::max)(GetBasicAttackSpeedMultiplier(), 1.0f)
            : 1.0f;
    if (!animation->Play(clipName, kAttackStartBlendDuration, kAttackAnimationSpeed * basicAttackSpeedMultiplier))
    {
        return false;
    }

    mMoveDir = { 0.0f, 0.0f, 0.0f };
    mAttackAnimationTimer =
        (useAttack2 ? kAttack2AnimationDuration : kAttack1AnimationDuration) /
        basicAttackSpeedMultiplier;
    mAttackAnimationPlaying = true;
    mArcherBasicAttackRetimingActive = GetClassType() == PlayerClass::Archer;
    mLastBasicAttackVariant = useAttack2 ? 2 : 1;
    OnBasicAttackStarted(mLastBasicAttackVariant);
    return true;
}

bool Player::CanPlaySkillAttack(int skillIndex) const
{
    if (!IsSkillUnlocked(skillIndex))
    {
        return false;
    }

    const bool allowInitialLeapSkillCast =
        mIsSkillLeaping &&
        mSkillLeapIndex == skillIndex &&
        mSkillLeapElapsed <= 0.0001f;

    if (mPlayerObject == nullptr || mIsDead || mRespawnAnimationPlaying || mIsDashing || mAttackAnimationTimer > 0.0f)
    {
        return false;
    }

    if (mIsSkillLeaping && !allowInitialLeapSkillCast)
    {
        return false;
    }

    auto* animation = mPlayerObject->GetSkeletalAnimation();
    return animation != nullptr && animation->IsLoaded();
}

bool Player::PlaySkillAttack(int skillIndex)
{
    if (!CanPlaySkillAttack(skillIndex))
    {
        return false;
    }

    auto* animation = mPlayerObject->GetSkeletalAnimation();
    const bool hasPendingSkillTarget = mHasPendingSkillTargetPosition;
    const XMFLOAT3 pendingSkillTarget = mPendingSkillTargetPosition;
    mHasPendingSkillTargetPosition = false;
    const bool useWarriorQ = GetClassType() == PlayerClass::Warrior && skillIndex == 1;
    const bool useWarriorE = GetClassType() == PlayerClass::Warrior && skillIndex == 2;
    const bool useMageQ = GetClassType() == PlayerClass::Mage && skillIndex == 1;
    const bool useMageE = GetClassType() == PlayerClass::Mage && skillIndex == 2;
    const bool useArcherWindImbuement = GetClassType() == PlayerClass::Archer && skillIndex == 1;
    const bool useArcherE = GetClassType() == PlayerClass::Archer && skillIndex == 2;
    const bool useWarriorMovementSkill = useWarriorQ || useWarriorE;
    if (useWarriorMovementSkill && !mIsGrounded)
    {
        return false;
    }

    if (useArcherWindImbuement)
    {
        OnSkillAttackStarted(skillIndex);
        return true;
    }

    const char* clipName = (useWarriorQ || useMageQ)
        ? "FemaleAttackQ"
        : ((useWarriorE || useMageE || useArcherE) ? "FemaleAttackE" : "FemaleAttack1");
    float playbackSpeed = kAttackAnimationSpeed;
    if (useWarriorQ)
    {
        playbackSpeed = kWarriorQEarlyPlaybackSpeed;
    }
    else if (useWarriorE)
    {
        playbackSpeed = kWarriorEEarlyPlaybackSpeed;
    }
    else if (useMageQ)
    {
        playbackSpeed = kMageQAnimationPlaybackSpeed;
    }
    else if (useMageE)
    {
        playbackSpeed = kMageEAnimationPlaybackSpeed;
    }
    else if (useArcherE)
    {
        playbackSpeed = kArcherEAnimationPlaybackSpeed;
    }
    if (!animation->Play(clipName, kAttackStartBlendDuration, playbackSpeed))
    {
        return false;
    }

    mMoveDir = { 0.0f, 0.0f, 0.0f };
    mAttackAnimationTimer = GetSkillAttackLockDuration(skillIndex);
    mWarriorQMotionActive = false;
    mWarriorQMovedThisFrame = false;
    mArcherBasicAttackRetimingActive = false;
    mWarriorQMotionElapsed = 0.0f;
    mWarriorQMotionDuration = 0.0f;
    mWarriorQClipDuration = 0.0f;
    mWarriorQSpeedUpTime = 0.0f;
    mWarriorSkillVisualArcHeight = 0.0f;
    ApplyVisualPositionOffset(0.0f);
    if (useWarriorMovementSkill)
    {
        const float clipDuration = animation->GetClipDurationSeconds(clipName);
        mWarriorQClipDuration = clipDuration > 0.0f
            ? clipDuration
            : kAttack1AnimationDuration * kAttackAnimationSpeed;
        if (useWarriorQ)
        {
            mWarriorSkillForwardDistance = kWarriorQForwardDistance;
            mWarriorSkillMoveStartClipFraction = kWarriorQMoveStartClipFraction;
            mWarriorSkillMoveEndClipFraction = kWarriorQMoveEndClipFraction;
            mWarriorSkillEarlyClipFraction = kWarriorQEarlyClipFraction;
            mWarriorSkillEarlyPlaybackSpeed = kWarriorQEarlyPlaybackSpeed;
            mWarriorSkillLatePlaybackSpeed = kWarriorQLatePlaybackSpeed;

            if (hasPendingSkillTarget)
            {
                const XMFLOAT3 startPosition = GetPosition();
                const float dx = pendingSkillTarget.x - startPosition.x;
                const float dz = pendingSkillTarget.z - startPosition.z;
                const float targetDistance = std::sqrt(dx * dx + dz * dz);
                const float distanceRatio = (std::max)(
                    targetDistance / kWarriorQForwardDistance,
                    0.0f);

                mWarriorSkillForwardDistance = (std::max)(
                    targetDistance - kWarriorQStopDistance,
                    0.0f);
                mWarriorSkillVisualArcHeight = kWarriorQMaxVisualArcHeight * distanceRatio;
            }
        }
        else
        {
            mWarriorSkillForwardDistance = kWarriorEForwardDistance;
            mWarriorSkillMoveStartClipFraction = 0.0f;
            mWarriorSkillMoveEndClipFraction = kWarriorEMoveEndClipFraction;
            mWarriorSkillEarlyClipFraction = kWarriorEEarlyClipFraction;
            mWarriorSkillEarlyPlaybackSpeed = kWarriorEEarlyPlaybackSpeed;
            mWarriorSkillLatePlaybackSpeed = kWarriorELatePlaybackSpeed;
        }
        mWarriorQSpeedUpTime =
            mWarriorQClipDuration * mWarriorSkillEarlyClipFraction /
                mWarriorSkillEarlyPlaybackSpeed;
        mAttackAnimationTimer = mWarriorQSpeedUpTime +
            mWarriorQClipDuration * (1.0f - mWarriorSkillEarlyClipFraction) /
                mWarriorSkillLatePlaybackSpeed;
        mWarriorQMotionActive = true;
        mWarriorQMotionElapsed = 0.0f;
        mWarriorQMotionDuration = mAttackAnimationTimer;
        mWarriorQMotionDirection = {
            std::sin(mFacingRotY),
            0.0f,
            std::cos(mFacingRotY)
        };
    }
    else
    {
        const float clipDuration = animation->GetClipDurationSeconds(clipName);
        mAttackAnimationTimer = clipDuration > 0.0f
            ? clipDuration / playbackSpeed
            : GetSkillAttackLockDuration(skillIndex);
    }
    mAttackAnimationPlaying = true;
    OnSkillAttackStarted(skillIndex);
    return true;
}

void Player::SetPendingSkillTargetPosition(const XMFLOAT3& targetPosition)
{
    mHasPendingSkillTargetPosition = true;
    mPendingSkillTargetPosition = targetPosition;
}

float Player::GetSkillAttackLockDuration(int skillIndex) const
{
    return skillIndex == 2 ? kAttack2AnimationDuration : kAttack1AnimationDuration;
}

bool Player::ConsumeQueuedSkillAttackOverride(int skillIndex, XMFLOAT3& outOrigin, float& outDelay)
{
    if (!mHasQueuedSkillAttackOverride || mQueuedSkillAttackIndex != skillIndex)
    {
        return false;
    }

    outOrigin = mQueuedSkillAttackOrigin;
    outDelay = mQueuedSkillAttackDelay;
    mHasQueuedSkillAttackOverride = false;
    mQueuedSkillAttackIndex = 0;
    mQueuedSkillAttackDelay = 0.0f;
    return true;
}

bool Player::StartLeapSkillMotion(int skillIndex, float forwardDistance, float arcHeight, float duration)
{
    if (mPlayerObject == nullptr ||
        mIsDead ||
        mIsDashing ||
        mIsSkillLeaping ||
        !mIsGrounded ||
        duration <= 0.05f ||
        forwardDistance <= 0.05f)
    {
        return false;
    }

    const XMFLOAT3 startPosition = GetPosition();
    const XMFLOAT3 forward = { std::sin(mFacingRotY), 0.0f, std::cos(mFacingRotY) };
    XMFLOAT3 landingPosition = startPosition;

    if (mLastMapSystem != nullptr)
    {
        constexpr float kLeapSampleStep = 0.22f;
        float travelled = 0.0f;

        while (travelled < forwardDistance)
        {
            const float feetPos = landingPosition.y - mCollider.Extents.y;
            if (mLastMapSystem->CheckWall(landingPosition.x, landingPosition.z, feetPos, forward.x, forward.z))
            {
                break;
            }

            travelled = (std::min)(forwardDistance, travelled + kLeapSampleStep);
            landingPosition.x = startPosition.x + forward.x * travelled;
            landingPosition.z = startPosition.z + forward.z * travelled;
        }

        if (travelled <= 0.05f)
        {
            return false;
        }

        const float probeStartY = (std::max)(startPosition.y, landingPosition.y) + 3.0f;
        const float floorY = mLastMapSystem->GetFloorHeight(landingPosition.x, landingPosition.z, probeStartY, 16.0f);
        if (floorY > -8000.0f)
        {
            landingPosition.y = floorY + mCollider.Extents.y;
        }
    }
    else
    {
        landingPosition.x += forward.x * forwardDistance;
        landingPosition.z += forward.z * forwardDistance;
    }

    const float planarDx = landingPosition.x - startPosition.x;
    const float planarDz = landingPosition.z - startPosition.z;
    if ((planarDx * planarDx + planarDz * planarDz) <= 0.01f)
    {
        return false;
    }

    mMoveDir = { 0.0f, 0.0f, 0.0f };
    mVerticalVelocity = 0.0f;
    mIsGrounded = false;
    mIsSkillLeaping = true;
    mSkillLeapIndex = skillIndex;
    mSkillLeapElapsed = 0.0f;
    mSkillLeapDuration = duration;
    mSkillLeapArcHeight = arcHeight;
    mSkillLeapStartPosition = startPosition;
    mSkillLeapTargetPosition = landingPosition;

    mHasQueuedSkillAttackOverride = true;
    mQueuedSkillAttackIndex = skillIndex;
    mQueuedSkillAttackOrigin = landingPosition;
    mQueuedSkillAttackDelay = duration;
    return true;
}

void Player::FaceCameraForward()
{
    if (mCamera == nullptr || mPlayerObject == nullptr)
    {
        return;
    }

    XMVECTOR look = XMVectorSetY(mCamera->GetLook(), 0.0f);
    const float lengthSq = XMVectorGetX(XMVector3LengthSq(look));
    if (lengthSq <= 0.0001f)
    {
        return;
    }

    XMFLOAT3 forward;
    XMStoreFloat3(&forward, XMVector3Normalize(look));

    mFacingRotY = atan2f(forward.x, forward.z);
    mTargetFacingRotY = mFacingRotY;
    mPlayerObject->SetRotation(0.0f, mFacingRotY, 0.0f);
}

void Player::FaceTowards(const XMFLOAT3& targetPosition)
{
    if (mPlayerObject == nullptr)
    {
        return;
    }

    const XMFLOAT3 playerPos = GetPosition();
    const float dx = targetPosition.x - playerPos.x;
    const float dz = targetPosition.z - playerPos.z;
    if ((dx * dx + dz * dz) <= 0.0001f)
    {
        return;
    }

    mFacingRotY = atan2f(dx, dz);
    mTargetFacingRotY = mFacingRotY;
    mPlayerObject->SetRotation(0.0f, mFacingRotY, 0.0f);
}

void Player::OnMouseMove(float dx, float dy)
{
    mTheta += dx;
    mPhi += dy;

    if (mPhi < MinCameraPhi) mPhi = MinCameraPhi;
    if (mPhi > MaxCameraPhi) mPhi = MaxCameraPhi;
}

void Player::UpdateCamera(MapSystem* mapSystem)
{
    if (mPlayerObject == nullptr || mCamera == nullptr) return;

    // 1. 타겟 설정 (내 머리 위)
    XMFLOAT3 playerPos = mPlayerObject->GetPosition();
    float headOffset = mCollider.Extents.y * 2.0f;
    if (headOffset < 1.0f) headOffset = 1.5f;

    XMVECTOR targetPos = XMVectorSet(playerPos.x, playerPos.y + headOffset, playerPos.z, 1.0f);

    // 2. 구면 좌표계 -> 직교 좌표계 변환 (원하는 카메라 위치)
    float x = mRadius * sinf(mPhi) * cosf(mTheta);
    float z = mRadius * sinf(mPhi) * sinf(mTheta);
    float y = mRadius * cosf(mPhi);

    XMVECTOR offset = XMVectorSet(x, y, z, 0.0f);
    XMVECTOR desiredPos = targetPos + offset;

    XMVECTOR camDir = XMVector3Normalize(desiredPos - targetPos);

    // 3. 벽 충돌 검사 (Spring Arm)
    float finalDist = mRadius;
    if (mapSystem != nullptr)
    {
        float hitDist = 0.0f;

        // 미리 계산해둔 camDir 사용
        if (mapSystem->CastRay(targetPos, camDir, mRadius, hitDist))
        {
            float adjustedDist = hitDist - 0.5f;
            if (adjustedDist < 0.5f) adjustedDist = 0.5f;
            finalDist = adjustedDist;
        }
    }

    // 4. 최종 카메라 위치 적용
    // 미리 계산해둔 camDir 사용
    XMVECTOR finalPos = targetPos + (camDir * finalDist);

    XMFLOAT3 finalPos3;
    XMStoreFloat3(&finalPos3, finalPos);

    mCamera->SetPosition(finalPos3);
    mCamera->LookAt(targetPos);
}

void Player::ApplyPhysics(const GameTimer& gt, MapSystem* mapSystem)
{
    float dt = gt.DeltaTime();
    if (dt > 0.05f) dt = 0.05f;
    XMFLOAT3 oldPos = mPlayerObject->GetPosition();
    XMFLOAT3 pos = oldPos;

    if (mIsSkillLeaping)
    {
        mFacingRotY = MoveAngleTowards(mFacingRotY, mTargetFacingRotY, kFacingTurnSpeed * dt);
        mPlayerObject->SetRotation(0.0f, mFacingRotY, 0.0f);

        mSkillLeapElapsed += dt;
        const float t = (std::clamp)(mSkillLeapElapsed / (std::max)(mSkillLeapDuration, 0.0001f), 0.0f, 1.0f);
        pos = Lerp3(mSkillLeapStartPosition, mSkillLeapTargetPosition, t);
        pos.y += 4.0f * mSkillLeapArcHeight * t * (1.0f - t);

        if (t >= 1.0f)
        {
            pos = mSkillLeapTargetPosition;
            mIsSkillLeaping = false;
            mSkillLeapIndex = 0;
            mSkillLeapElapsed = 0.0f;
            mSkillLeapDuration = 0.0f;
            mSkillLeapArcHeight = 0.0f;
            mIsGrounded = true;
        }
        else
        {
            mIsGrounded = false;
        }

        mVerticalVelocity = 0.0f;
        mPlayerObject->SetPosition(pos.x, pos.y, pos.z);
        mCollider.Center = pos;
        return;
    }

    // =========================================================
    // 1. 이동 (대쉬 가속도 적용) 및 벽 충돌 처리
    // =========================================================
    if (mMoveDir.x != 0.0f || mMoveDir.z != 0.0f)
    {
        mTargetFacingRotY = atan2f(mMoveDir.x, mMoveDir.z);

        //대쉬 중이라면 기본 속도(mMoveSpeed)에 배수(mDashSpeedMultiplier)를 곱해줍니다.
        float currentSpeed = mIsDashing ? (mMoveSpeed * mDashSpeedMultiplier) : mMoveSpeed;

        float dx = mMoveDir.x * currentSpeed * dt;
        float dz = mMoveDir.z * currentSpeed * dt;

        if (mapSystem != nullptr)
        {
            float feetPos = pos.y - mCollider.Extents.y;
            if (mapSystem->CheckWall(pos.x, pos.z, feetPos, mMoveDir.x, 0.0f)) dx = 0.0f;
            if (mapSystem->CheckWall(pos.x, pos.z, feetPos, 0.0f, mMoveDir.z)) dz = 0.0f;
        }
        pos.x += dx;
        pos.z += dz;
    }

    if (mWarriorQMotionActive && mWarriorQMotionDuration > 0.0f)
    {
        const float previousProgress = GetWarriorSkillClipProgress(
            mWarriorQMotionElapsed,
            mWarriorQClipDuration,
            mWarriorQSpeedUpTime,
            mWarriorSkillEarlyClipFraction,
            mWarriorSkillEarlyPlaybackSpeed,
            mWarriorSkillLatePlaybackSpeed);
        mWarriorQMotionElapsed = (std::min)(
            mWarriorQMotionElapsed + dt,
            mWarriorQMotionDuration);
        const float currentProgress = GetWarriorSkillClipProgress(
            mWarriorQMotionElapsed,
            mWarriorQClipDuration,
            mWarriorQSpeedUpTime,
            mWarriorSkillEarlyClipFraction,
            mWarriorSkillEarlyPlaybackSpeed,
            mWarriorSkillLatePlaybackSpeed);
        const float previousMoveProgress = GetMoveProgress(
            previousProgress,
            mWarriorSkillMoveStartClipFraction,
            mWarriorSkillMoveEndClipFraction);
        const float currentMoveProgress = GetMoveProgress(
            currentProgress,
            mWarriorSkillMoveStartClipFraction,
            mWarriorSkillMoveEndClipFraction);
        const float previousMoveCurve = SmoothStep(previousMoveProgress);
        const float currentMoveCurve = SmoothStep(currentMoveProgress);
        const float visualYOffset =
            4.0f * currentMoveCurve * (1.0f - currentMoveCurve) *
            mWarriorSkillVisualArcHeight;
        const float distanceDelta =
            (currentMoveCurve - previousMoveCurve) *
                mWarriorSkillForwardDistance;

        float dx = mWarriorQMotionDirection.x * distanceDelta;
        float dz = mWarriorQMotionDirection.z * distanceDelta;
        if (mapSystem != nullptr)
        {
            const float feetPos = pos.y - mCollider.Extents.y;
            if (mapSystem->CheckWall(
                pos.x, pos.z, feetPos, mWarriorQMotionDirection.x, 0.0f))
            {
                dx = 0.0f;
            }
            if (mapSystem->CheckWall(
                pos.x, pos.z, feetPos, 0.0f, mWarriorQMotionDirection.z))
            {
                dz = 0.0f;
            }
        }

        pos.x += dx;
        pos.z += dz;
        mWarriorQMovedThisFrame = dx != 0.0f || dz != 0.0f;
        if (mWarriorQMotionElapsed >= mWarriorQMotionDuration)
        {
            mWarriorQMotionActive = false;
            mWarriorSkillVisualArcHeight = 0.0f;
            ApplyVisualPositionOffset(0.0f);
        }
        else
        {
            ApplyVisualPositionOffset(visualYOffset);
        }
    }

    mFacingRotY = MoveAngleTowards(mFacingRotY, mTargetFacingRotY, kFacingTurnSpeed * dt);
    mPlayerObject->SetRotation(0.0f, mFacingRotY, 0.0f);

    // =========================================================
    // 2. 중력 및 바닥 처리 
    // =========================================================
    if (mapSystem != nullptr)
    {
        float halfHeight = mCollider.Extents.y;
        float feetPos = pos.y - halfHeight;
        float rayStartY = feetPos + 1.0f;
        float floorY = mapSystem->GetFloorHeight(pos.x, pos.z, rayStartY, 1000.0f);

        if (floorY < -8000.0f)
        {
            pos.x = oldPos.x;
            pos.z = oldPos.z;
            floorY = mapSystem->GetFloorHeight(pos.x, pos.z, rayStartY, 1000.0f);
            mVerticalVelocity = 0.0f;
            mIsGrounded = true;
        }
        else
        {
            if (feetPos < floorY)
            {
                pos.y = floorY + halfHeight;
                mVerticalVelocity = 0.0f;
                mIsGrounded = true;
            }
            else if (feetPos >= floorY && (feetPos - floorY) <= 0.5f && mVerticalVelocity <= 0.0f)
            {
                pos.y = floorY + halfHeight;
                mVerticalVelocity = 0.0f;
                mIsGrounded = true;
            }
            else
            {
                mVerticalVelocity -= 9.8f * dt;
                if (mVerticalVelocity < -50.0f) mVerticalVelocity = -50.0f;
                mIsGrounded = false;
            }
        }
        pos.y += mVerticalVelocity * dt;
    }

    mPlayerObject->SetPosition(pos.x, pos.y, pos.z);
    mCollider.Center = pos;
}

DirectX::XMFLOAT3 Player::GetPosition() const { return mPlayerObject->GetPosition(); }
void Player::SetPosition(float x, float y, float z) { mPlayerObject->SetPosition(x, y, z); }

void Player::ApplyVisualPositionOffset(float extraY)
{
    if (mPlayerObject == nullptr)
    {
        return;
    }

    mPlayerObject->SetPositionOffset(
        mBasePositionOffset.x,
        mBasePositionOffset.y + extraY,
        mBasePositionOffset.z);
}

void Player::Dash()
{
    // 이미 대쉬 중이거나, 쿨타임이 남아있거나, 공중에 떠있으면 대쉬 불가
    if (mIsDead || mIsDashing || mIsSkillLeaping || mDashCooldown > 0.0f || !mIsGrounded || mAttackAnimationTimer > 0.0f)
    {
        return;
    }

    if (mMoveDir.x == 0.0f && mMoveDir.z == 0.0f)
    {
        XMVECTOR camLook = XMVector3Normalize(XMVectorSetY(mCamera->GetLook(), 0.0f));
        XMStoreFloat3(&mMoveDir, camLook);
    }

    mIsDashing = true;
    mDashTimer = mDashDuration; // 0.25초 동안 돌진
    mDashCooldown = mDashCooldownDuration;
    OnDashStarted();

    OutputDebugStringA("[Player] 대쉬 발동!\n");
}

void Player::OnDamaged(float damage)
{
    if (mIsDead)
    {
        return;
    }

    if (mIsDashing)
    {
        OutputDebugStringA("[Player] 회피 성공! (무적)\n");
        return;
    }

    hp -= damage;

    if (hp <= 0.0f)
    {
        hp = 0.0f;
        mIsDead = true;
        EnterDeathAnimationState();
    }
}

bool Player::HasMP(float amount) const
{
    return amount <= 0.0f || mp + 0.001f >= amount;
}

bool Player::TrySpendMP(float amount)
{
    if (!HasMP(amount))
    {
        return false;
    }

    mp = (std::max)(0.0f, mp - (std::max)(amount, 0.0f));
    return true;
}

void Player::RestoreMP(float amount)
{
    if (amount <= 0.0f)
    {
        return;
    }

    mp = (std::min)(GetMaxMP(), mp + amount);
}

void Player::RefillMP()
{
    mp = GetMaxMP();
}

void Player::ApplyServerHit(int remainHp, bool isDead)
{
    const bool wasDead = mIsDead;
    hp = static_cast<float>(remainHp);
    if (hp < 0.0f)
    {
        hp = 0.0f;
    }
    if (hp > GetMaxHP())
    {
        hp = GetMaxHP();
    }

    mIsDead = isDead || hp <= 0.0f;
    if (mIsDead)
    {
        hp = 0.0f;
        mMoveDir = { 0.0f, 0.0f, 0.0f };
        mIsDashing = false;
        mIsSkillLeaping = false;
        mHasQueuedSkillAttackOverride = false;
        mAttackAnimationTimer = 0.0f;
        mAttackAnimationPlaying = false;
        mArcherBasicAttackRetimingActive = false;
        mWarriorQMotionActive = false;
        mWarriorSkillVisualArcHeight = 0.0f;
        ApplyVisualPositionOffset(0.0f);
        if (!wasDead)
        {
            EnterDeathAnimationState();
        }
    }
}

void Player::RespawnAt(float x, float y, float z, int remainHp)
{
    hp = static_cast<float>(remainHp > 0 ? remainHp : static_cast<int>(GetMaxHP()));
    if (hp > GetMaxHP())
    {
        hp = GetMaxHP();
    }
    RefillMP();

    mIsDead = false;
    mDeathAnimationStarted = false;
    mMoveDir = { 0.0f, 0.0f, 0.0f };
    mIsDashing = false;
    mIsSkillLeaping = false;
    mDashTimer = 0.0f;
    mDashCooldown = 0.0f;
    mVerticalVelocity = 0.0f;
    mIsGrounded = false;
    mSkillLeapIndex = 0;
    mSkillLeapElapsed = 0.0f;
    mSkillLeapDuration = 0.0f;
    mSkillLeapArcHeight = 0.0f;
    mAttackAnimationTimer = 0.0f;
    mAttackAnimationPlaying = false;
    mArcherBasicAttackRetimingActive = false;
    mHasQueuedSkillAttackOverride = false;
    mQueuedSkillAttackIndex = 0;
    mQueuedSkillAttackOrigin = { x, y, z };
    mQueuedSkillAttackDelay = 0.0f;
    mWarriorQMotionActive = false;
    mWarriorQMovedThisFrame = false;
    mWarriorQMotionElapsed = 0.0f;
    mWarriorQMotionDuration = 0.0f;
    mWarriorQClipDuration = 0.0f;
    mWarriorQSpeedUpTime = 0.0f;
    mWarriorSkillVisualArcHeight = 0.0f;
    ApplyVisualPositionOffset(0.0f);
    mHasSentMovementState = false;
    mMovePacketSendTimer = DebugConfig::kPlayerMoveSendIntervalSeconds;

    SetPosition(x, y, z);
    mCollider.Center = { x, y, z };
    mLastSentPosition = { x, y, z };
    mLastSentRotY = mFacingRotY;
    StartRespawnAnimation();
    if (!mRespawnAnimationPlaying)
    {
        mAnimationState = PlayerAnimationState::Walk;
        UpdateAnimationState();
    }

    if (mPlayerObject != nullptr)
    {
        mPlayerObject->Update();
    }
}

int Player::GetExperienceToNextLevel() const
{
    if (mLevel >= MaxProgressionLevel)
    {
        return 0;
    }

    const int nextThreshold = GetExperienceThresholdForLevel(mLevel + 1);
    return (std::max)(0, nextThreshold - mExperience);
}

float Player::GetExperienceProgressRatio() const
{
    if (mLevel >= MaxProgressionLevel)
    {
        return 1.0f;
    }

    const int currentThreshold = GetExperienceThresholdForLevel(mLevel);
    const int nextThreshold = GetExperienceThresholdForLevel(mLevel + 1);
    const int requiredExperience = (std::max)(nextThreshold - currentThreshold, 1);
    const int earnedExperience = (std::clamp)(mExperience - currentThreshold, 0, requiredExperience);
    return static_cast<float>(earnedExperience) / static_cast<float>(requiredExperience);
}

bool Player::AddExperience(int amount)
{
    if (amount <= 0)
    {
        return false;
    }

    const int previousLevel = mLevel;
    const int maxTrackedExperience = GetExperienceThresholdForLevel(MaxProgressionLevel);
    mExperience = (std::min)(mExperience + amount, maxTrackedExperience);

    while (mLevel < MaxProgressionLevel &&
        mExperience >= GetExperienceThresholdForLevel(mLevel + 1))
    {
        ++mLevel;
    }

    std::ostringstream gainLog;
    gainLog << "[PlayerExp] +" << amount << " exp";
    if (mLevel < MaxProgressionLevel)
    {
        const int nextThreshold = GetExperienceThresholdForLevel(mLevel + 1);
        gainLog << " (" << mExperience << "/" << nextThreshold << ")";
    }
    else
    {
        gainLog << " (max level)";
    }
    gainLog << "\n";
    OutputDebugStringA(gainLog.str().c_str());

    if (mLevel == previousLevel)
    {
        return false;
    }

    SetCurrentTier(TierFromLevel(mLevel));
    hp = GetMaxHP();
    mp = GetMaxMP();
    mIsDead = false;

    std::ostringstream levelLog;
    levelLog << "[PlayerLevel] Level " << mLevel << " reached. Tier visual should refresh.\n";
    OutputDebugStringA(levelLog.str().c_str());
    return true;
}

void Player::ResetProgression()
{
    mLevel = MinProgressionLevel;
    mExperience = 0;
    mCurrentTier = ClassTier::Tier1;
    hp = GetMaxHP();
    mp = GetMaxMP();
    mIsDead = false;
}

void Player::Promote()
{
    if (mLevel >= MaxProgressionLevel)
    {
        return;
    }

    ++mLevel;
    mExperience = (std::max)(mExperience, GetExperienceThresholdForLevel(mLevel));
    SetCurrentTier(TierFromLevel(mLevel));
    hp = GetMaxHP();
    mp = GetMaxMP();
    mIsDead = false;
}

void Player::SetCurrentTier(ClassTier tier)
{
    const ClassTier resolvedTier = TierFromLevel(static_cast<int>(tier));
    const int resolvedLevel = LevelFromTier(resolvedTier);

    if (mCurrentTier == resolvedTier)
    {
        mLevel = resolvedLevel;
        return;
    }

    mCurrentTier = resolvedTier;
    mLevel = resolvedLevel;
    UpdateMeshForTier();
}
