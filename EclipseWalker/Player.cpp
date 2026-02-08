#include "Player.h"
#include <Windows.h> // 키 입력(GetAsyncKeyState)용

using namespace DirectX;

Player::Player() {}
Player::~Player() {}

void Player::Initialize(GameObject* playerObj, Camera* cam)
{
    mPlayerObject = playerObj;
    mCamera = cam;

    XMFLOAT3 startPos = { 0.0f, 1.0f, -10.0f }; 

    if (mPlayerObject)
        mPlayerObject->SetPosition(startPos.x, startPos.y, startPos.z);

    if (mCamera)
        mCamera->SetPosition(startPos.x, startPos.y + mEyeHeight, startPos.z);
}

void Player::Update(const GameTimer& gt)
{
    HandleInput(gt);

    // 카메라가 이동한 후, 플레이어 몸(큐브)을 카메라 위치 아래로 끌고 옴
    if (mCamera && mPlayerObject)
    {
        XMFLOAT3 camPos = mCamera->GetPosition3f();
        mPlayerObject->SetPosition(camPos.x, camPos.y - mEyeHeight, camPos.z);
        // (선택) 몸체 회전: 카메라가 보는 방향(Y축 회전)과 일치시키려면 추가 구현 필요
        // 지금은 큐브라 회전이 크게 중요하지 않음
    }
}

void Player::HandleInput(const GameTimer& gt)
{
    if (!mCamera) return;

    float dt = gt.DeltaTime();
    float speed = mMoveSpeed * dt;

    if (GetAsyncKeyState(VK_SHIFT) & 0x8000)
        speed *= 2.0f;
    if (GetAsyncKeyState('W') & 0x8000) mCamera->Walk(speed);
    if (GetAsyncKeyState('S') & 0x8000) mCamera->Walk(-speed);
    if (GetAsyncKeyState('A') & 0x8000) mCamera->Strafe(-speed);
    if (GetAsyncKeyState('D') & 0x8000) mCamera->Strafe(speed);
}

DirectX::XMFLOAT3 Player::GetPosition() const
{
    if (mPlayerObject) return mPlayerObject->GetPosition();
    return { 0.0f, 0.0f, 0.0f };
}