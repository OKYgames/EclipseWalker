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
    if (!mIsAnimated || Ritem == nullptr) return;

    // 2. 시간 누적
    mAnimTime += dt;

    // 3. 프레임 교체 타이머
    if (mAnimTime >= mFrameDuration)
    {
        mAnimTime = 0.0f; // 시간 초기화
        mCurrFrame++;     // 다음 프레임으로

        if (mCurrFrame >= mNumCols * mNumRows) {
            mCurrFrame = 0;
        }
    } 

    int col = mCurrFrame % mNumCols;
    int row = mCurrFrame / mNumCols;

    float stepU = 1.0f / mNumCols;
    float stepV = 1.0f / mNumRows;

    float offsetU = col * stepU;
    float offsetV = row * stepV;

    XMMATRIX texScale = XMMatrixScaling(stepU, stepV, 1.0f);
    XMMATRIX texOffset = XMMatrixTranslation(offsetU, offsetV, 0.0f);

    XMMATRIX finalTransform = texScale * texOffset;
    XMStoreFloat4x4(&Ritem->TexTransform, finalTransform);

    Ritem->NumFramesDirty = 3;

    // 4. 파티클 로직 (크기, 위치, 색상 계산)
    if (mIsParticle)
    {
        mAge += dt;
        if (mAge > mLifeTime) mAge -= mLifeTime;

        float t = mAge / mLifeTime;

        float easeOut = 1.0f - (1.0f - t) * (1.0f - t);
        float finalScale = mBaseScale * (0.1f + (easeOut * 1.9f));
        float newY = mBasePosY + (t * t * 0.8f);

        DirectX::XMFLOAT4 yellow = { 3.0f, 2.5f, 0.5f, 1.0f };
        DirectX::XMFLOAT4 orange = { 2.5f, 0.8f, 0.1f, 1.0f };
        DirectX::XMFLOAT4 black = { 0.0f, 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT4 finalColor;

        if (t < 0.5f) {
            float blend = t / 0.5f;
            finalColor.x = yellow.x * (1.0f - blend) + orange.x * blend;
            finalColor.y = yellow.y * (1.0f - blend) + orange.y * blend;
            finalColor.z = yellow.z * (1.0f - blend) + orange.z * blend;
            finalColor.w = yellow.w * (1.0f - blend) + orange.w * blend;
        }
        else {
            float blend = (t - 0.5f) / 0.5f;
            finalColor.x = orange.x * (1.0f - blend) + black.x * blend;
            finalColor.y = orange.y * (1.0f - blend) + black.y * blend;
            finalColor.z = orange.z * (1.0f - blend) + black.z * blend;
            finalColor.w = orange.w * (1.0f - blend) + black.w * blend;
        }

        mColorMultiplier = finalColor;
        Ritem->ColorMultiplier = mColorMultiplier;

        SetScale(finalScale, finalScale, finalScale);
        SetPosition(mPos.x, newY, mPos.z);
        Update();
    }
}