#include "GameObject.h"
#include "FrameResource.h" 

GameObject::GameObject()
{
}

GameObject::~GameObject()
{
}

void GameObject::SetPosition(float x, float y, float z)
{
    mPos = XMFLOAT3(x, y, z);
    NumFramesDirty = 3; 
}

void GameObject::SetScale(float x, float y, float z)
{
    mScale = XMFLOAT3(x, y, z);
    NumFramesDirty = 3;
}

void GameObject::SetRotation(float x, float y, float z)
{
    mRot = XMFLOAT3(x, y, z);
    NumFramesDirty = 3;
}

void GameObject::Update()
{
    // 크기 * 회전 * 이동 행렬 계산
    XMMATRIX S = XMMatrixScaling(mScale.x, mScale.y, mScale.z);
    XMMATRIX R = XMMatrixRotationRollPitchYaw(mRot.x, mRot.y, mRot.z);
    XMMATRIX T = XMMatrixTranslation(mPos.x, mPos.y, mPos.z);

    XMMATRIX world = S * R * T;
    XMStoreFloat4x4(&World, world);

    if (Ritem != nullptr)
    {
        Ritem->World = World;
        Ritem->NumFramesDirty = 3; 
    }

}

void GameObject::UpdateAnimation(float dt)
{
    // 1. 애니메이션 대상이 아니거나, 렌더 아이템이 없으면 패스
    if (!mIsAnimated || Ritem == nullptr) return;

    // 2. 시간 누적
    mAnimTime += dt;

    // 3. 프레임 교체 시기가 되었는가?
    if (mAnimTime >= mFrameDuration)
    {
        mAnimTime = 0.0f; // 시간 초기화
        mCurrFrame++;     // 다음 프레임으로

        // 총 프레임 수(2x2=4개)를 넘어가면 다시 0번으로 (순환)
        if (mCurrFrame >= mNumCols * mNumRows)
        {
            mCurrFrame = 0;
        }

        // ========================================================
        //  현재 프레임 번호에 맞는 텍스처 좌표 계산
        // ========================================================

        // 예: mCurrFrame이 2이면 -> (행:1, 열:0) -> 왼쪽 아래 그림
        int col = mCurrFrame % mNumCols; // 열 번호 (나머지)
        int row = mCurrFrame / mNumCols; // 행 번호 (몫)

        // UV 좌표상 이동할 거리 계산 (칸당 크기: 1.0 / 칸수)
        float stepU = 1.0f / mNumCols; // 0.5f
        float stepV = 1.0f / mNumRows; // 0.5f

        float offsetU = col * stepU;
        float offsetV = row * stepV;

        // 4. 텍스처 변환 행렬 업데이트
        XMMATRIX texScale = XMMatrixScaling(stepU, stepV, 1.0f);
        XMMATRIX texOffset = XMMatrixTranslation(offsetU, offsetV, 0.0f);

        // 스케일 먼저, 이동 나중
        XMMATRIX finalTransform = texScale * texOffset;

        XMStoreFloat4x4(&Ritem->TexTransform, finalTransform);

        Ritem->NumFramesDirty = 3; // 프레임 리소스 개수만큼 설정
    }
}