#include "Player.h"
#include <Windows.h> // 키 입력(GetAsyncKeyState)용

using namespace DirectX;

Player::Player() {}
Player::~Player() {}

void Player::Initialize(GameObject* playerObj, Camera* cam)
{
    mPlayerObject = playerObj;
    mCamera = cam;

    // 초기 충돌 박스 설정 (플레이어 크기에 맞게 조절)
    // Center는 Update에서 매번 갱신되므로 Extents(반지름)만 설정
    mCollider.Extents = XMFLOAT3(0.3f, 0.8f, 0.3f);
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

    // [★중요] Y축(높이) 성분을 제거하여 "땅바닥 평면" 기준으로 만듦
    camLook = XMVectorSetY(camLook, 0.0f);
    camRight = XMVectorSetY(camRight, 0.0f);

    // 정규화 (길이를 1로)
    camLook = XMVector3Normalize(camLook);
    camRight = XMVector3Normalize(camRight);

    // 3. 카메라 기준 방향 합성
    // (카메라앞 * W입력) + (카메라오른쪽 * D입력)
    XMVECTOR targetDir = (camLook * inputZ) + (camRight * inputX);

    // 최종 방향 정규화 (대각선 이동 시 속도 일정하게 유지)
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
    XMFLOAT3 pos = mPlayerObject->GetPosition();
    mCollider.Center = pos;

    // 1. 이동 및 회전
    if (mMoveDir.x != 0.0f || mMoveDir.z != 0.0f)
    {
        // 이동 방향 바라보게 회전
        float targetAngle = atan2f(mMoveDir.x, mMoveDir.z);
        mPlayerObject->SetRotation(0.0f, targetAngle, 0.0f);

        float dx = mMoveDir.x * mMoveSpeed * dt;
        float dz = mMoveDir.z * mMoveSpeed * dt;

        // X축 이동
        float originalX = pos.x;
        pos.x += dx;
        mCollider.Center = pos;
        if (mapSystem && mapSystem->CheckCollision(mCollider)) pos.x = originalX;

        // Z축 이동
        float originalZ = pos.z;
        pos.z += dz;
        mCollider.Center = { pos.x, pos.y, pos.z };
        if (mapSystem && mapSystem->CheckCollision(mCollider)) pos.z = originalZ;
    }

    // 2. 중력 및 바닥 처리
    if (mapSystem)
    {
        float floorHeight = mapSystem->GetFloorHeight(pos.x, pos.z);
        float feetY = pos.y - mCollider.Extents.y;

        // 공중에 떠있으면 중력 적용
        if (feetY > floorHeight + 0.1f)
        {
            mVerticalVelocity -= 9.8f * dt;
        }
        else
        {
            // 땅에 닿음
            mVerticalVelocity = 0.0f;
            pos.y = floorHeight + mCollider.Extents.y;
        }
        pos.y += mVerticalVelocity * dt;
    }

    // 최종 위치 적용
    mPlayerObject->SetPosition(pos.x, pos.y, pos.z);
}

DirectX::XMFLOAT3 Player::GetPosition() const { return mPlayerObject->GetPosition(); }
void Player::SetPosition(float x, float y, float z) { mPlayerObject->SetPosition(x, y, z); }