#pragma once
#include "GameObject.h"
#include "Camera.h"
#include "GameTimer.h"

class Player
{
public:
    Player();
    ~Player();

    void Initialize(GameObject* playerObj, Camera* cam);
    void Update(const GameTimer& gt);

    DirectX::XMFLOAT3 GetPosition() const;
private:
    void HandleInput(const GameTimer& gt);

private:
    Camera* mCamera = nullptr;     
    GameObject* mPlayerObject = nullptr; 

    float mMoveSpeed = 10.0f;        
    float mEyeHeight = 1.0f;
};