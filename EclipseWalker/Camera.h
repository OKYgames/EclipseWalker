#pragma once
#include "d3dUtil.h"

class Camera
{
public:
    Camera();
    ~Camera();

    // 1. 위치 및 방향 관련
    DirectX::XMVECTOR GetPosition()const;
    DirectX::XMFLOAT3 GetPosition3f()const;
    void SetPosition(float x, float y, float z);
    void SetPosition(const DirectX::XMFLOAT3& v);

    // 벡터 가져오기 (오른쪽, 위, 앞)
    DirectX::XMVECTOR GetRight()const;
    DirectX::XMVECTOR GetUp()const;
    DirectX::XMVECTOR GetLook()const;

    // 2. 렌즈 설정 (원근감, 시야각)
    void SetLens(float fovY, float aspect, float zn, float zf);

    // 3. 카메라 조작
    void LookAt(DirectX::FXMVECTOR pos, DirectX::FXMVECTOR target, DirectX::FXMVECTOR worldUp);
    void LookAt(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT3& target, const DirectX::XMFLOAT3& up);

    // 이동 
    void Strafe(float d); // 게걸음 (좌우)
    void Walk(float d);   // 앞뒤 이동

    // 회전 (마우스 회전)
    void Pitch(float angle); // 고개 들기/숙이기
    void RotateY(float angle); // 몸통 돌리기

    // 4. 행렬 가져오기
    void UpdateViewMatrix(); 
    DirectX::XMMATRIX GetView()const;
    DirectX::XMMATRIX GetProj()const;
    DirectX::XMMATRIX GetViewProj()const;

private:
    // 카메라의 위치와 로컬 축
    DirectX::XMFLOAT3 mPosition = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 mRight = { 1.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 mUp = { 0.0f, 1.0f, 0.0f };
    DirectX::XMFLOAT3 mLook = { 0.0f, 0.0f, 1.0f };

    // 렌즈 설정값
    float mNearZ = 0.0f;
    float mFarZ = 0.0f;
    float mAspect = 0.0f;
    float mFovY = 0.0f;
    float mNearWindowHeight = 0.0f;
    float mFarWindowHeight = 0.0f;

    bool mViewDirty = true; 

    // 최종 행렬
    DirectX::XMFLOAT4X4 mView = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };
    DirectX::XMFLOAT4X4 mProj = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };
};