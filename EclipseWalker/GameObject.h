#pragma once
#include "d3dUtil.h" 
#include "MathHelper.h"
#include <memory>

using namespace DirectX;

struct RenderItem;
class SkeletalAnimationComponent;

class GameObject
{
public:
    GameObject();
    virtual ~GameObject();

    // 1. 변환(Transform) 설정 함수들
    void SetPosition(float x, float y, float z);
    void SetPositionOffset(float x, float y, float z);
    void SetScale(float x, float y, float z);
    void SetRotation(float x, float y, float z); 
    void SetRotationOffset(float x, float y, float z);
    void SetWorldTransform(DirectX::CXMMATRIX world);
    void ClearWorldTransformOverride();

    virtual void Update();
    void UpdateAnimation(float dt);
    SkeletalAnimationComponent* CreateSkeletalAnimationComponent();
    SkeletalAnimationComponent* GetSkeletalAnimation() { return mSkeletalAnimation.get(); }
    const SkeletalAnimationComponent* GetSkeletalAnimation() const { return mSkeletalAnimation.get(); }
    XMFLOAT3 GetPosition() const
    {
        return mPos;
    }
    XMFLOAT3 GetPositionOffset() const
    {
        return mPosOffset;
    }
    XMFLOAT3 GetRotation() const
    {
        return mRot;
    }

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
    bool mIsBillboard = false;

    bool mIsParticle = false;      
    float mAge = 0.0f;            
    float mLifeTime = 1.0f;        // 총 수명
    float mBaseScale = 1.0f;       // 기본 크기
    float mBasePosY = 0.0f;        // 처음 생성된 Y 위치
    DirectX::XMFLOAT4 mColorMultiplier = { 1.0f, 1.0f, 1.0f, 1.0f }; 

private:
    XMFLOAT3 mPos = { 0.0f, 0.0f, 0.0f };
    XMFLOAT3 mPosOffset = { 0.0f, 0.0f, 0.0f };
    XMFLOAT3 mScale = { 1.0f, 1.0f, 1.0f };
    XMFLOAT3 mRot = { 0.0f, 0.0f, 0.0f };
    XMFLOAT3 mRotOffset = { 0.0f, 0.0f, 0.0f };
    bool mUseWorldOverride = false;
    XMFLOAT4X4 mWorldOverride = MathHelper::Identity4x4();
    std::unique_ptr<SkeletalAnimationComponent> mSkeletalAnimation;
};
