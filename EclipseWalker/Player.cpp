#include "Player.h"
#include <Windows.h> 

using namespace DirectX;

Player::Player() {}
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
    SyncCamera();
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

void Player::SyncCamera()
{
    if (mPlayerObject == nullptr || mCamera == nullptr) return;

    // 플레이어 위치
    XMFLOAT3 pPos = mPlayerObject->GetPosition();
    XMVECTOR playerPos = XMLoadFloat3(&pPos);

    // 카메라 방향
    XMVECTOR lookDir = mCamera->GetLook();

    // -lookDir = 뒤쪽 방향
    XMVECTOR targetPos = playerPos - (lookDir * 7.0f);
    targetPos = targetPos + XMVectorSet(0.0f, 2.0f, 0.0f, 0.0f);

    // 카메라 위치 강제 설정
    XMFLOAT3 finalPos;
    XMStoreFloat3(&finalPos, targetPos);
    mCamera->SetPosition(finalPos.x, finalPos.y, finalPos.z);
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