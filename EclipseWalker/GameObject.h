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
    void UpdateAnimation(float dt);

public:
    XMFLOAT4X4 World = MathHelper::Identity4x4();
    RenderItem* Ritem = nullptr;
    int NumFramesDirty = 3;

	// 애니메이션 관련 멤버 변수
    bool mIsAnimated = false;      
    float mAnimTime = 0.0f;         
    float mFrameDuration = 0.1f;    
    int mCurrFrame = 0;             
    int mNumCols = 2;               
    int mNumRows = 2;              
    int mLightIndex = -1;

private:
    XMFLOAT3 mPos = { 0.0f, 0.0f, 0.0f };
    XMFLOAT3 mScale = { 1.0f, 1.0f, 1.0f };
    XMFLOAT3 mRot = { 0.0f, 0.0f, 0.0f };
};