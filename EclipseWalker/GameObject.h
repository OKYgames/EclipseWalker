#pragma once
#include "d3dUtil.h" 
#include "MathHelper.h"

using namespace DirectX;

struct RenderItem;

class GameObject
{
public:
    GameObject();
    virtual ~GameObject();

    // 1. 변환(Transform) 설정 함수들
    void SetPosition(float x, float y, float z);
    void SetScale(float x, float y, float z);
    void SetRotation(float x, float y, float z); 

    virtual void Update();

public:
    // 월드 행렬 (위치+회전+크기 정보가 합쳐진 최종 행렬)
    XMFLOAT4X4 World = MathHelper::Identity4x4();

    RenderItem* Ritem = nullptr;

    // 값이 변했는지 체크 (DX12 최적화용)
    int NumFramesDirty = 3;

private:
    XMFLOAT3 mPos = { 0.0f, 0.0f, 0.0f };
    XMFLOAT3 mScale = { 1.0f, 1.0f, 1.0f };
    XMFLOAT3 mRot = { 0.0f, 0.0f, 0.0f };
};