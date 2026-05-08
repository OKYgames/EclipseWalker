#include "Player.h"
#include <Windows.h> 
#include "NetworkManager.h"
#include "Scene.h"
#include "SkeletalAnimationComponent.h"
#include <algorithm> 

using namespace DirectX;

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

    // 초기 충돌 박스 설정
    mCollider.Extents = XMFLOAT3(DefaultColliderHalfWidth, DefaultColliderHalfHeight, DefaultColliderHalfWidth);

    mMoveDir = { 0.0f, 0.0f, 0.0f };
    mFacingRotY = 0.0f;
    mAnimationState = PlayerAnimationState::Walk;
    mLastSentAnimationState = PlayerAnimationState::Walk;
    mHasSentMovementState = false;
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
    }

    // 대쉬 지속 시간 감소 및 종료 체크
    if (mIsDashing) {
        mDashTimer -= dt;
        if (mDashTimer <= 0.0f) {
            mIsDashing = false; // 대쉬 종료
        }
    }
    // =========================================================

    HandleInput();
    UpdateAnimationState();
    ApplyPhysics(gt, mapSystem);
    UpdateCamera(mapSystem);

    if (hp < 0.0f)
    {
        hp = 0.0f;
    }
    const bool isMoving = mMoveDir.x != 0.0f || mMoveDir.z != 0.0f || !mIsGrounded;
    const bool animationChanged = !mHasSentMovementState || mLastSentAnimationState != mAnimationState;
    if (isMoving || animationChanged)
    {
        XMFLOAT3 currentPos = GetPosition();

        NetworkManager::Get()->SendPlayerMove(
            currentPos.x,
            currentPos.y,
            currentPos.z,
            mFacingRotY,
            static_cast<int>(mAnimationState));
        mLastSentAnimationState = mAnimationState;
        mHasSentMovementState = true;
    }
}

void Player::HandleInput()
{
    if (mIsDashing) return;

    mMoveDir = { 0.0f, 0.0f, 0.0f };

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

    const bool isMoving = mIsDashing || mMoveDir.x != 0.0f || mMoveDir.z != 0.0f;
    const PlayerAnimationState nextState = isMoving ? PlayerAnimationState::Walk : PlayerAnimationState::Idle;
    if (mAnimationState == nextState)
    {
        return;
    }

    const char* clipName = (nextState == PlayerAnimationState::Walk) ? "FemaleWalk" : "FemaleIdle";
    if (animation->Play(clipName))
    {
        mAnimationState = nextState;
    }
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
        float targetAngle = atan2f(mMoveDir.x, mMoveDir.z);
        mFacingRotY = targetAngle;
        mPlayerObject->SetRotation(0.0f, targetAngle, 0.0f);

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
    if (mIsDashing || mDashCooldown > 0.0f || !mIsGrounded)
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
    mDashCooldown = 6.0f;       // 1초 쿨타임

    OutputDebugStringA("[Player] 대쉬 발동!\n");
}

void Player::OnDamaged(float damage)
{
    if (mIsDashing)
    {
        OutputDebugStringA("[Player] 회피 성공! (무적)\n");
        return;
    }

    hp -= damage;

    if (hp <= 0.0f)
    {
        hp = 0.0f;
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
}
