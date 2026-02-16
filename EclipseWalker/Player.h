#pragma once
#include "d3dUtil.h"
#include "GameObject.h"
#include "Camera.h"
#include "GameTimer.h"
#include "MapSystem.h"

class Player
{
public:
    Player();
    ~Player();

    void Initialize(GameObject* playerObj, Camera* cam);
    void Update(const GameTimer& gt, MapSystem* mapSystem);
    
    DirectX::XMFLOAT3 GetPosition() const;
    void SetPosition(float x, float y, float z);
    void Jump();

private:
    void HandleInput();
    void SyncCamera();
    void ApplyPhysics(const GameTimer& gt, MapSystem* mapSystem);
private:
    Camera* mCamera = nullptr;     
    GameObject* mPlayerObject = nullptr; 

    DirectX::XMFLOAT3 mMoveDir = { 0.0f, 0.0f, 0.0f }; 
    DirectX::BoundingBox mCollider;

    float mMoveSpeed = 5.0f;
    float mVerticalVelocity = 0.0f;     // 중력/점프용 속도
    float mEyeHeight = 1.0f;
    bool mIsGrounded = false;
};