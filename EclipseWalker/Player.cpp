#include "Player.h"
#include <Windows.h> 

using namespace DirectX;

Player::Player() 
{
    mTheta = 1.5f * XM_PI;
    mPhi = 0.25f * XM_PI;
    mRadius = 5.0f;
}

Player::~Player() {}

void Player::Initialize(GameObject* playerObj, Camera* cam)
{
    mPlayerObject = playerObj;
    mCamera = cam;

    // 초기 충돌 박스 설정
    mCollider.Extents = XMFLOAT3(0.15f, 0.5f, 0.15f);
}

void Player::Update(const GameTimer& gt, MapSystem* mapSystem)
{
    HandleInput();
    ApplyPhysics(gt, mapSystem);
    UpdateCamera(mapSystem);
}

void Player::HandleInput()
{
    mMoveDir = { 0.0f, 0.0f, 0.0f };

    if (GetAsyncKeyState(VK_SPACE) & 0x8000)
    {
        Jump(); 
    }

    // 1. 키보드 입력 (앞뒤/좌우)
    float inputZ = 0.0f; // W, S
    float inputX = 0.0f; // A, D

    if (GetAsyncKeyState('W') & 0x8000) inputZ += 1.0f;
    if (GetAsyncKeyState('S') & 0x8000) inputZ -= 1.0f;
    if (GetAsyncKeyState('D') & 0x8000) inputX += 1.0f;
    if (GetAsyncKeyState('A') & 0x8000) inputX -= 1.0f;

    // 입력이 없으면 종료
    if (inputZ == 0.0f && inputX == 0.0f) return;

    // 2. 카메라의 "보는 방향(Look)"과 "오른쪽 방향(Right)" 가져오기
    XMVECTOR camLook = mCamera->GetLook();
    XMVECTOR camRight = mCamera->GetRight();

    //  Y축(높이) 성분을 제거하여 "땅바닥 평면" 기준으로 만듦
    camLook = XMVectorSetY(camLook, 0.0f);
    camRight = XMVectorSetY(camRight, 0.0f);

    // 정규화 (길이를 1로)
    camLook = XMVector3Normalize(camLook);
    camRight = XMVector3Normalize(camRight);

    // 3. 카메라 기준 방향 합성
    // (카메라앞 * W입력) + (카메라오른쪽 * D입력)
    XMVECTOR targetDir = (camLook * inputZ) + (camRight * inputX);

    // 최종 방향 정규화 
    targetDir = XMVector3Normalize(targetDir);

    XMStoreFloat3(&mMoveDir, targetDir);
}

void Player::OnMouseMove(float dx, float dy)
{
    mTheta += dx;
    mPhi += dy;

    // 각도 제한
    float limit = 0.1f;
    if (mPhi < limit) mPhi = limit;
    if (mPhi > XM_PI - limit) mPhi = XM_PI - limit;
}

void Player::UpdateCamera(MapSystem* mapSystem)
{
    if (mPlayerObject == nullptr || mCamera == nullptr) return;

    // 1. 타겟 설정 (내 머리 위)
    XMFLOAT3 playerPos = mPlayerObject->GetPosition();
    float headOffset = mCollider.Extents.y * 2.0f; // 키 높이 정도
    if (headOffset < 1.0f) headOffset = 1.5f;      // 최소 높이 보장

    XMVECTOR targetPos = XMVectorSet(playerPos.x, playerPos.y + headOffset, playerPos.z, 1.0f);

    // 2. 구면 좌표계 -> 직교 좌표계 변환
    float x = mRadius * sinf(mPhi) * cosf(mTheta);
    float z = mRadius * sinf(mPhi) * sinf(mTheta);
    float y = mRadius * cosf(mPhi);

    XMVECTOR offset = XMVectorSet(x, y, z, 0.0f);
    XMVECTOR desiredPos = targetPos + offset;

    // 3. 벽 충돌 검사 (Spring Arm)
    float finalDist = mRadius;

    if (mapSystem != nullptr)
    {
        XMVECTOR camDir = XMVector3Normalize(desiredPos - targetPos);
        float hitDist = 0.0f;

        // 레이캐스트 발사
        if (mapSystem->CastRay(targetPos, camDir, mRadius, hitDist))
        {
            // 벽보다 0.5m 앞쪽으로 당김
            float adjustedDist = hitDist - 0.5f;
            if (adjustedDist < 0.5f) adjustedDist = 0.5f; // 최소 거리 유지
            finalDist = adjustedDist;
        }
    }

    // 4. 최종 카메라 위치 적용
    XMVECTOR camDir = XMVector3Normalize(desiredPos - targetPos);
    XMVECTOR finalPos = targetPos + (camDir * finalDist);

    XMFLOAT3 finalPos3;
    XMStoreFloat3(&finalPos3, finalPos);

    mCamera->SetPosition(finalPos3);
    mCamera-> LookAt(targetPos);
}

void Player::ApplyPhysics(const GameTimer& gt, MapSystem* mapSystem)
{
    float dt = gt.DeltaTime();
    XMFLOAT3 oldPos = mPlayerObject->GetPosition();
    XMFLOAT3 pos = oldPos;

    // =========================================================
    // 1. 이동 처리 (벽 충돌 검사 포함)
    // =========================================================
    if (mMoveDir.x != 0.0f || mMoveDir.z != 0.0f)
    {
        // 1-1. 회전 (바라보는 방향)
        float targetAngle = atan2f(mMoveDir.x, mMoveDir.z);
        mPlayerObject->SetRotation(0.0f, targetAngle, 0.0f);

        // 1-2. 이동 속도 계산
        float dx = mMoveDir.x * mMoveSpeed * dt;
        float dz = mMoveDir.z * mMoveSpeed * dt;

        // 1-3. 벽 검사 (CheckWall 사용)
        if (mapSystem != nullptr)
        {
            if (mapSystem->CheckWall(pos.x, pos.z, pos.y, mMoveDir.x, 0.0f))
            {
                dx = 0.0f;
            }

            if (mapSystem->CheckWall(pos.x, pos.z, pos.y, 0.0f, mMoveDir.z))
            {
                dz = 0.0f;
            }
        }

        // 1-4. 실제 이동 적용
        pos.x += dx;
        pos.z += dz;
    }

    // =========================================================
    // 2. 중력 및 바닥 처리 
    // =========================================================
    if (mapSystem != nullptr)
    {
        float halfHeight = mCollider.Extents.y;
        float floorY = mapSystem->GetFloorHeight(pos.x, pos.z, pos.y, halfHeight);
        
        float feetPos = pos.y - halfHeight;
        float stepLimit = 0.5f;

        if (floorY > feetPos + stepLimit)
        {
            pos.x = oldPos.x;
            pos.z = oldPos.z;
            floorY = mapSystem->GetFloorHeight(pos.x, pos.z, pos.y, halfHeight);
        }

        // [상태 1] 공중 판정 
        if (floorY < -9000.0f)
        {
            mVerticalVelocity -= 9.8f * dt;
            mIsGrounded = false;
        }
        else
        {
            // [상태 1] 공중 판정 
            if (feetPos > floorY + 0.01f)
            {
                mVerticalVelocity -= 9.8f * dt;
                mIsGrounded = false; 
            }
            else
            {
                // [상태 2] 착지 처리 (땅에 닿음)
                if (mVerticalVelocity <= 0.0f)
                {
                    mVerticalVelocity = 0.0f;
                    pos.y = floorY + halfHeight;

                    mIsGrounded = true; 
                }
            }
        }

        pos.y += mVerticalVelocity * dt;

        // [상태 3] 파묻힘 방지
        if (pos.y - halfHeight < floorY)
        {
            if (floorY <= feetPos + stepLimit)
            {
                pos.y = floorY + halfHeight;
                mVerticalVelocity = 0.0f;
                mIsGrounded = true;
            }
        }
    }
    mPlayerObject->SetPosition(pos.x, pos.y, pos.z);
    mCollider.Center = pos;
}

DirectX::XMFLOAT3 Player::GetPosition() const { return mPlayerObject->GetPosition(); }
void Player::SetPosition(float x, float y, float z) { mPlayerObject->SetPosition(x, y, z); }

void Player::Jump()
{
    if (mIsGrounded)
    {
        mVerticalVelocity = 5.0f;

        mIsGrounded = false;
    }
}