#include "Camera.h"
#include <algorithm>
#include <cmath>

using namespace DirectX;

Camera::Camera()
{
    SetLens(0.25f * XM_PI, 1.0f, 1.0f, 1000.0f);
}

Camera::~Camera()
{
}

XMVECTOR Camera::GetPosition()const
{
    const XMFLOAT3 shakenPosition = GetShakenPosition3f();
    return XMLoadFloat3(&shakenPosition);
}

XMFLOAT3 Camera::GetPosition3f()const
{
    return GetShakenPosition3f();
}

void Camera::SetPosition(float x, float y, float z)
{
    mPosition = XMFLOAT3(x, y, z);
    mViewDirty = true;
}

void Camera::SetPosition(const XMFLOAT3& v)
{
    mPosition = v;
    mViewDirty = true;
}

XMVECTOR Camera::GetRight()const
{
    return XMLoadFloat3(&mRight);
}

XMVECTOR Camera::GetUp()const
{
    return XMLoadFloat3(&mUp);
}

XMVECTOR Camera::GetLook()const
{
    return XMLoadFloat3(&mLook);
}

void Camera::SetLens(float fovY, float aspect, float zn, float zf)
{
    mFovY = fovY;
    mAspect = aspect;
    mNearZ = zn;
    mFarZ = zf;

    mNearWindowHeight = 2.0f * mNearZ * tanf(0.5f * mFovY);
    mFarWindowHeight = 2.0f * mFarZ * tanf(0.5f * mFovY);

    XMMATRIX P = XMMatrixPerspectiveFovLH(mFovY, mAspect, mNearZ, mFarZ);
    XMStoreFloat4x4(&mProj, P);
}

void Camera::LookAt(FXMVECTOR pos, FXMVECTOR target, FXMVECTOR worldUp)
{
    XMVECTOR L = XMVector3Normalize(XMVectorSubtract(target, pos));
    XMVECTOR R = XMVector3Normalize(XMVector3Cross(worldUp, L));
    XMVECTOR U = XMVector3Cross(L, R);

    XMStoreFloat3(&mPosition, pos);
    XMStoreFloat3(&mLook, L);
    XMStoreFloat3(&mRight, R);
    XMStoreFloat3(&mUp, U);

    mViewDirty = true;
}

void Camera::LookAt(const XMFLOAT3& pos, const XMFLOAT3& target, const XMFLOAT3& up)
{
    XMVECTOR P = XMLoadFloat3(&pos);
    XMVECTOR T = XMLoadFloat3(&target);
    XMVECTOR U = XMLoadFloat3(&up);

    LookAt(P, T, U);
}

void Camera::LookAt(DirectX::FXMVECTOR target)
{
    LookAt(DirectX::XMLoadFloat3(&mPosition), target, DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
}

void Camera::LookAt(const DirectX::XMFLOAT3& target)
{
    DirectX::XMVECTOR t = DirectX::XMLoadFloat3(&target);
    LookAt(t);
}

void Camera::Strafe(float d)
{
    // mPosition += d * mRight
    XMVECTOR s = XMVectorReplicate(d);
    XMVECTOR r = XMLoadFloat3(&mRight);
    XMVECTOR p = XMLoadFloat3(&mPosition);
    XMStoreFloat3(&mPosition, XMVectorMultiplyAdd(s, r, p));

    mViewDirty = true;
}

void Camera::Walk(float d)
{
    // mPosition += d * mLook
    XMVECTOR s = XMVectorReplicate(d);
    XMVECTOR l = XMLoadFloat3(&mLook);
    XMVECTOR p = XMLoadFloat3(&mPosition);
    XMStoreFloat3(&mPosition, XMVectorMultiplyAdd(s, l, p));

    mViewDirty = true;
}

void Camera::Pitch(float angle)
{
    // 위아래 회전 (Right 벡터 기준)
    XMMATRIX R = XMMatrixRotationAxis(XMLoadFloat3(&mRight), angle);

    XMStoreFloat3(&mUp, XMVector3TransformNormal(XMLoadFloat3(&mUp), R));
    XMStoreFloat3(&mLook, XMVector3TransformNormal(XMLoadFloat3(&mLook), R));

    mViewDirty = true;
}

void Camera::RotateY(float angle)
{
    // 좌우 회전 (월드 Y축 기준)
    XMMATRIX R = XMMatrixRotationY(angle);

    XMStoreFloat3(&mRight, XMVector3TransformNormal(XMLoadFloat3(&mRight), R));
    XMStoreFloat3(&mUp, XMVector3TransformNormal(XMLoadFloat3(&mUp), R));
    XMStoreFloat3(&mLook, XMVector3TransformNormal(XMLoadFloat3(&mLook), R));

    mViewDirty = true;
}

void Camera::UpdateViewMatrix()
{
    if (mViewDirty)
    {
        XMVECTOR R = XMLoadFloat3(&mRight);
        XMVECTOR U = XMLoadFloat3(&mUp);
        XMVECTOR L = XMLoadFloat3(&mLook);
        const XMFLOAT3 shakenPosition = GetShakenPosition3f();
        XMVECTOR P = XMLoadFloat3(&shakenPosition);

        // 정규화 및 직교화 (오차 보정)
        L = XMVector3Normalize(L);
        U = XMVector3Normalize(XMVector3Cross(L, R));
        R = XMVector3Cross(U, L);

        // 뷰 행렬 원소 채우기
        float x = -XMVectorGetX(XMVector3Dot(P, R));
        float y = -XMVectorGetX(XMVector3Dot(P, U));
        float z = -XMVectorGetX(XMVector3Dot(P, L));

        XMStoreFloat3(&mRight, R);
        XMStoreFloat3(&mUp, U);
        XMStoreFloat3(&mLook, L);

        mView(0, 0) = mRight.x; mView(0, 1) = mUp.x; mView(0, 2) = mLook.x; mView(0, 3) = 0.0f;
        mView(1, 0) = mRight.y; mView(1, 1) = mUp.y; mView(1, 2) = mLook.y; mView(1, 3) = 0.0f;
        mView(2, 0) = mRight.z; mView(2, 1) = mUp.z; mView(2, 2) = mLook.z; mView(2, 3) = 0.0f;
        mView(3, 0) = x;        mView(3, 1) = y;        mView(3, 2) = z;        mView(3, 3) = 1.0f;

        mViewDirty = false;
    }
}

XMMATRIX Camera::GetView()const
{
    return XMLoadFloat4x4(&mView);
}

XMMATRIX Camera::GetProj()const
{
    return XMLoadFloat4x4(&mProj);
}

XMMATRIX Camera::GetViewProj()const
{
    return XMMatrixMultiply(GetView(), GetProj());
}

void Camera::StartShake(float durationSeconds, float amplitude, float frequency)
{
    if (durationSeconds <= 0.0f || amplitude <= 0.0f)
    {
        return;
    }

    mShakeDuration = (std::max)(mShakeDuration, durationSeconds);
    mShakeTimer = (std::max)(mShakeTimer, durationSeconds);
    mShakeAmplitude = (std::max)(mShakeAmplitude, amplitude);
    mShakeFrequency = (std::max)(frequency, 1.0f);
    mViewDirty = true;
}

void Camera::UpdateShake(float dt)
{
    if (mShakeTimer <= 0.0f || mShakeDuration <= 0.0f)
    {
        if (mShakeOffset.x != 0.0f || mShakeOffset.y != 0.0f || mShakeOffset.z != 0.0f)
        {
            mShakeOffset = { 0.0f, 0.0f, 0.0f };
            mViewDirty = true;
        }
        return;
    }

    mShakeTimer = (std::max)(0.0f, mShakeTimer - (std::max)(dt, 0.0f));

    const float elapsed = mShakeDuration - mShakeTimer;
    const float remainingT = (std::clamp)(mShakeTimer / mShakeDuration, 0.0f, 1.0f);
    const float envelope = remainingT * remainingT;
    const float phase = elapsed * mShakeFrequency;
    const float rightAmount = std::sin(phase * 6.2831853f) * mShakeAmplitude * envelope;
    const float upAmount = std::cos((phase * 1.37f + 0.31f) * 6.2831853f) * mShakeAmplitude * 0.65f * envelope;

    XMVECTOR offset =
        XMLoadFloat3(&mRight) * rightAmount +
        XMLoadFloat3(&mUp) * upAmount;
    XMStoreFloat3(&mShakeOffset, offset);

    if (mShakeTimer <= 0.0f)
    {
        mShakeDuration = 0.0f;
        mShakeAmplitude = 0.0f;
        mShakeOffset = { 0.0f, 0.0f, 0.0f };
    }

    mViewDirty = true;
}

XMFLOAT3 Camera::GetShakenPosition3f() const
{
    return
    {
        mPosition.x + mShakeOffset.x,
        mPosition.y + mShakeOffset.y,
        mPosition.z + mShakeOffset.z
    };
}
