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