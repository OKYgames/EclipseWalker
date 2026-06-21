#pragma once
#include "d3dUtil.h"
#include "GameObject.h"
#include "Camera.h"
#include "GameTimer.h"
#include "Lantern.h"
#include "MapSystem.h"

enum class PlayerClass { Warrior, Mage, Archer, None };
enum class ClassTier { Tier1 = 1, Tier2 = 2, Tier3 = 3 };
enum class PlayerAnimationState { Idle, Walk, Dash };

class Player
{
public:
    static constexpr float DefaultColliderHalfWidth = 0.11f;
    static constexpr float DefaultColliderHalfHeight = 0.40f;
    static constexpr float DefaultVisualTargetHeight = 1.35f;
    static constexpr float DefaultVisualFloorBias = 0.03f;
    static constexpr float DefaultCameraPhi = 0.40f * 3.14159f;
    static constexpr float MinCameraPhi = 0.36f * 3.14159f;
    static constexpr float MaxCameraPhi = 0.50f * 3.14159f;

    Player();
    virtual ~Player(); 
    void Initialize(GameObject* playerObj, Camera* cam);
    void Update(const GameTimer& gt, MapSystem* mapSystem);

    DirectX::XMFLOAT3 GetPosition() const;
    void SetPosition(float x, float y, float z);
    PlayerAnimationState GetAnimationState() const { return mAnimationState; }
    float GetFacingRotY() const { return mFacingRotY; }

    void Dash();
    bool PlayRandomBasicAttack();
    bool CanPlaySkillAttack(int skillIndex) const;
    bool PlaySkillAttack(int skillIndex);
    bool ConsumeQueuedSkillAttackOverride(int skillIndex, DirectX::XMFLOAT3& outOrigin, float& outDelay);
    int GetLastBasicAttackVariant() const { return mLastBasicAttackVariant; }
    void FaceCameraForward();
    void FaceTowards(const DirectX::XMFLOAT3& targetPosition);

    void OnMouseMove(float dx, float dy);
    void UpdateCamera(MapSystem* mapSystem);

    // ==========================================
    // [스탯] (자식 클래스에서 수정 가능하게)
    // ==========================================
    float GetHP() const { return hp; }
    virtual float GetMaxHP() const { return maxHp; }
    float GetMP() const { return mp; }
    virtual float GetMaxMP() const { return maxMp; }
    float GetDashCooldownRemaining() const { return mDashCooldown > 0.0f ? mDashCooldown : 0.0f; }
    float GetDashCooldownDuration() const { return mDashCooldownDuration; }
    float GetDashCooldownRatio() const
    {
        return (mDashCooldownDuration > 0.0f && mDashCooldown > 0.0f)
            ? (mDashCooldown / mDashCooldownDuration)
            : 0.0f;
    }
    bool IsDashOnCooldown() const { return mDashCooldown > 0.0f; }

    void OnDamaged(float damage);
    void ApplyServerHit(int remainHp, bool isDead);
    void RespawnAt(float x, float y, float z, int remainHp);
    bool IsDead() const { return mIsDead; }
    void ApplyPhysics(const GameTimer& gt, MapSystem* mapSystem);

    // ==========================================
    // [직업 및 스킬 시스템] 자식 클래스에서 덮어씌울 가상(virtual) 함수들
    // ==========================================
    virtual PlayerClass GetClassType() const { return PlayerClass::None; }
    virtual bool Skill1() { return true; }
    virtual bool Skill2() { return true; }
    bool CanUseLantern() const { return GetClassType() == PlayerClass::Mage; }
    Lantern* GetLantern() { return &mLantern; }
    const Lantern* GetLantern() const { return &mLantern; }

    // ==========================================
    // 티어(승급) 시스템
    // ==========================================
    ClassTier GetCurrentTier() const { return mCurrentTier; }
    void SetCurrentTier(ClassTier tier);
    void Promote(); // 티어를 1단계 올리는 함수 (경험치 달성 시 호출)

protected:
    void HandleInput();
    void UpdateAnimationState();
    virtual float GetSkillAttackLockDuration(int skillIndex) const;
    bool StartLeapSkillMotion(int skillIndex, float forwardDistance, float arcHeight, float duration);
    virtual void UpdateMeshForTier() {} // 티어 변경 시 외형(FBX)을 교체할 함수

    ClassTier mCurrentTier = ClassTier::Tier1;
    PlayerAnimationState mAnimationState = PlayerAnimationState::Walk;
    PlayerAnimationState mLastSentAnimationState = PlayerAnimationState::Walk;
    bool mHasSentMovementState = false;
    float mMovePacketSendTimer = 0.0f;
    DirectX::XMFLOAT3 mLastSentPosition = { 0.0f, 0.0f, 0.0f };
    float mLastSentRotY = 0.0f;
    float mAttackAnimationTimer = 0.0f;
    bool mAttackAnimationPlaying = false;
    int mLastBasicAttackVariant = 1;
    bool mWarriorQMotionActive = false;
    bool mWarriorQMovedThisFrame = false;
    float mWarriorQMotionElapsed = 0.0f;
    float mWarriorQMotionDuration = 0.0f;
    float mWarriorQClipDuration = 0.0f;
    float mWarriorQSpeedUpTime = 0.0f;
    float mWarriorSkillForwardDistance = 0.0f;
    float mWarriorSkillMoveEndClipFraction = 1.0f;
    float mWarriorSkillEarlyClipFraction = 1.0f;
    float mWarriorSkillEarlyPlaybackSpeed = 1.0f;
    float mWarriorSkillLatePlaybackSpeed = 1.0f;
    DirectX::XMFLOAT3 mWarriorQMotionDirection = { 0.0f, 0.0f, 1.0f };

    Camera* mCamera = nullptr;
    GameObject* mPlayerObject = nullptr;

    DirectX::XMFLOAT3 mMoveDir = { 0.0f, 0.0f, 0.0f };
    DirectX::BoundingBox mCollider;

    float mMoveSpeed = 2.8f;
    float mFacingRotY = 0.0f;
    float mTargetFacingRotY = 0.0f;

    // ------------------------------------------
    // 대쉬(Dash) 변수
    // ------------------------------------------
    bool mIsDashing = false;           // 현재 대쉬 중인지 여부
    float mDashTimer = 0.0f;           // 대쉬가 얼마나 진행되었는지 체크
    float mDashDuration = 0.25f;       // 대쉬 유지 시간 (0.25초 동안 슉! 이동)
    float mDashSpeedMultiplier = 3.0f; // 대쉬할 때 기본 속도의 몇 배로 빨라질지
    float mDashCooldown = 0.0f;        // 대쉬 쿨타임 (연속 대쉬 방지)
    float mDashCooldownDuration = 6.0f;
    // ------------------------------------------

    float mVerticalVelocity = 0.0f;
    float mEyeHeight = 1.0f;
    bool mIsGrounded = false;
    MapSystem* mLastMapSystem = nullptr;

    bool mIsSkillLeaping = false;
    int mSkillLeapIndex = 0;
    float mSkillLeapElapsed = 0.0f;
    float mSkillLeapDuration = 0.0f;
    float mSkillLeapArcHeight = 0.0f;
    DirectX::XMFLOAT3 mSkillLeapStartPosition = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 mSkillLeapTargetPosition = { 0.0f, 0.0f, 0.0f };

    bool mHasQueuedSkillAttackOverride = false;
    int mQueuedSkillAttackIndex = 0;
    DirectX::XMFLOAT3 mQueuedSkillAttackOrigin = { 0.0f, 0.0f, 0.0f };
    float mQueuedSkillAttackDelay = 0.0f;

    float mTheta = 1.5f * 3.14159f;
    float mPhi = DefaultCameraPhi;
    float mRadius = 5.0f;

    // ------------------------------------------
    // 최대 HP/MP 변수
    // ------------------------------------------
    float maxHp = 200.0f;
    float hp = 200.0f;
    float maxMp = 100.0f;
    float mp = 100.0f;

    float mDamageTimer = 0.0f;
    bool mIsDead = false;
    Lantern mLantern;
};
