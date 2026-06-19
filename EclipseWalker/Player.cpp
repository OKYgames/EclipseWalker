#include "Player.h"
#include <Windows.h> 
#include "NetworkManager.h"
#include "DebugConfig.h"
#include "Scene.h"
#include "SkeletalAnimationComponent.h"
#include <algorithm> 
#include <cmath>
#include <cstdlib>

using namespace DirectX;

namespace
{
    constexpr float kIdleWalkBlendDuration = 0.15f;
    constexpr float kAttackEndBlendDuration = 0.12f;
    constexpr float kAttackAnimationSpeed = 1.25f;
    constexpr float kAttack1AnimationDuration = (45.0f / 30.0f) / kAttackAnimationSpeed;
    constexpr float kAttack2AnimationDuration = (50.0f / 30.0f) / kAttackAnimationSpeed;
    constexpr float kFacingTurnSpeed = 7.5f;

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
    mIsDead = false;

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
    UpdateAnimationState();
}

void Player::Update(const GameTimer& gt, MapSystem* mapSystem)
{
    // =========================================================
    // 대쉬(Dash) 타이머 관리
    float dt = gt.DeltaTime();

    // 대쉬 쿨타임 감소
    if (mDashCooldown > 0.0f) {
        mDashCooldown -= dt;
        if (mDashCooldown < 0.0f) {
            mDashCooldown = 0.0f;
        }
    }

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
    // =========================================================

    if (mIsDead)
    {
        mMoveDir = { 0.0f, 0.0f, 0.0f };
        mIsDashing = false;
        mAttackAnimationTimer = 0.0f;
        mAttackAnimationPlaying = false;
        UpdateAnimationState();
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

    const bool isMoving = mMoveDir.x != 0.0f || mMoveDir.z != 0.0f || !mIsGrounded;
    const bool animationChanged = !mHasSentMovementState || mLastSentAnimationState != mAnimationState;
    XMFLOAT3 currentPos = GetPosition();
    const float dx = currentPos.x - mLastSentPosition.x;
    const float dy = currentPos.y - mLastSentPosition.y;
    const float dz = currentPos.z - mLastSentPosition.z;
    const float moveEpsilonSq =
        DebugConfig::kPlayerMovePositionEpsilon * DebugConfig::kPlayerMovePositionEpsilon;
    const bool positionChangedEnough = (dx * dx + dy * dy + dz * dz) >= moveEpsilonSq;
    const bool rotationChangedEnough =
        std::fabs(NormalizeAngle(mFacingRotY - mLastSentRotY)) >= DebugConfig::kPlayerMoveRotationEpsilon;
    const bool timedMoveUpdate =
        isMoving &&
        mMovePacketSendTimer >= DebugConfig::kPlayerMoveSendIntervalSeconds &&
        (positionChangedEnough || rotationChangedEnough);

    if (animationChanged || timedMoveUpdate)
    {
        NetworkManager::Get()->SendPlayerMove(
            currentPos.x,
            currentPos.y,
            currentPos.z,
            mFacingRotY,
            static_cast<int>(mAnimationState),
            static_cast<int>(GetClassType()));
        mLastSentAnimationState = mAnimationState;
        mHasSentMovementState = true;
        mMovePacketSendTimer = 0.0f;
        mLastSentPosition = currentPos;
        mLastSentRotY = mFacingRotY;
    }
}

void Player::HandleInput()
{
    if (mIsDashing) return;

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
    }

    const bool isMoving = mIsDashing || mMoveDir.x != 0.0f || mMoveDir.z != 0.0f;
    const PlayerAnimationState nextState = isMoving ? PlayerAnimationState::Walk : PlayerAnimationState::Idle;
    if (!attackJustEnded && mAnimationState == nextState)
    {
        return;
    }

    const char* clipName = (nextState == PlayerAnimationState::Walk) ? "FemaleWalk" : "FemaleIdle";
    const bool blendIdleAndWalk =
        !attackJustEnded &&
        ((mAnimationState == PlayerAnimationState::Idle && nextState == PlayerAnimationState::Walk) ||
            (mAnimationState == PlayerAnimationState::Walk && nextState == PlayerAnimationState::Idle));
    float blendDuration = 0.0f;
    if (attackJustEnded)
    {
        blendDuration = kAttackEndBlendDuration;
    }
    else if (blendIdleAndWalk)
    {
        blendDuration = kIdleWalkBlendDuration;
    }

    if (animation->Play(clipName, blendDuration, 1.0f))
    {
        mAnimationState = nextState;
    }
}

bool Player::PlayRandomBasicAttack()
{
    if (mPlayerObject == nullptr || mIsDead || mIsDashing || mAttackAnimationTimer > 0.0f)
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
    if (!animation->Play(clipName, 0.0f, kAttackAnimationSpeed))
    {
        return false;
    }

    mMoveDir = { 0.0f, 0.0f, 0.0f };
    mAttackAnimationTimer = useAttack2 ? kAttack2AnimationDuration : kAttack1AnimationDuration;
    mAttackAnimationPlaying = true;
    mLastBasicAttackVariant = useAttack2 ? 2 : 1;
    return true;
}

bool Player::PlaySkillAttack(int skillIndex)
{
    if (mPlayerObject == nullptr || mIsDead || mIsDashing || mAttackAnimationTimer > 0.0f)
    {
        return false;
    }

    auto* animation = mPlayerObject->GetSkeletalAnimation();
    if (animation == nullptr || !animation->IsLoaded())
    {
        return false;
    }

    const bool useAttack2 = skillIndex == 2;
    const char* clipName = useAttack2 ? "FemaleAttack2" : "FemaleAttack1";
    if (!animation->Play(clipName, 0.0f, kAttackAnimationSpeed))
    {
        return false;
    }

    mMoveDir = { 0.0f, 0.0f, 0.0f };
    mAttackAnimationTimer = useAttack2 ? kAttack2AnimationDuration : kAttack1AnimationDuration;
    mAttackAnimationPlaying = true;
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

void Player::Dash()
{
    // 이미 대쉬 중이거나, 쿨타임이 남아있거나, 공중에 떠있으면 대쉬 불가
    if (mIsDead || mIsDashing || mDashCooldown > 0.0f || !mIsGrounded || mAttackAnimationTimer > 0.0f)
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
    }
}

void Player::ApplyServerHit(int remainHp, bool isDead)
{
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
        mAttackAnimationTimer = 0.0f;
        mAttackAnimationPlaying = false;
        UpdateAnimationState();
    }
}

void Player::RespawnAt(float x, float y, float z, int remainHp)
{
    hp = static_cast<float>(remainHp > 0 ? remainHp : static_cast<int>(GetMaxHP()));
    if (hp > GetMaxHP())
    {
        hp = GetMaxHP();
    }

    mIsDead = false;
    mMoveDir = { 0.0f, 0.0f, 0.0f };
    mIsDashing = false;
    mDashTimer = 0.0f;
    mDashCooldown = 0.0f;
    mVerticalVelocity = 0.0f;
    mIsGrounded = false;
    mAttackAnimationTimer = 0.0f;
    mAttackAnimationPlaying = false;
    mHasSentMovementState = false;
    mMovePacketSendTimer = DebugConfig::kPlayerMoveSendIntervalSeconds;

    SetPosition(x, y, z);
    mCollider.Center = { x, y, z };
    mLastSentPosition = { x, y, z };
    mLastSentRotY = mFacingRotY;
    UpdateAnimationState();

    if (mPlayerObject != nullptr)
    {
        mPlayerObject->Update();
    }
}

void Player::Promote()
{
    if (mCurrentTier == ClassTier::Tier1) {
        mCurrentTier = ClassTier::Tier2;
        OutputDebugStringA("============= [2티어 승급 완료!] =============\n");
    }
    else if (mCurrentTier == ClassTier::Tier2) {
        mCurrentTier = ClassTier::Tier3;
        OutputDebugStringA("============= [3티어 최종 각성!] =============\n");
    }
    else {
        return; // 이미 3티어
    }

    // 외형(FBX) 교체 함수 호출 (자식 클래스인 전사, 법사 등에서 오버라이딩됨)
    UpdateMeshForTier();

    // 승급 시 체력과 마나를 꽉 채워줌
    hp = GetMaxHP();
    mp = GetMaxMP();
    mIsDead = false;
}

void Player::SetCurrentTier(ClassTier tier)
{
    if (mCurrentTier == tier)
    {
        return;
    }

    mCurrentTier = tier;
    UpdateMeshForTier();
}
