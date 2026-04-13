#pragma once
#include "d3dUtil.h"
#include "GameTimer.h"

class WorldTransitionEffect
{
public:
    WorldTransitionEffect();
    ~WorldTransitionEffect();

    void StartTransition(); // 연출 시작 트리거
    void Update(const GameTimer& gt, DirectX::XMFLOAT3& cameraPos); // 매 프레임 업데이트

    bool IsActive() const { return mIsActive; }
    bool NeedsWorldSwitch() const { return mNeedsWorldSwitch; }
    void ResetWorldSwitch() { mNeedsWorldSwitch = false; }

    // 셰이더(상수 버퍼)로 넘겨줄 효과 파라미터들
    float GetDistortionIntensity() const { return mDistortionIntensity; }
    float GetPurpleLerpAmount() const { return mPurpleLerpAmount; }
    float GetVignetteIntensity() const { return mVignetteIntensity; }
    float GetFlashAmount() const { return mFlashAmount; }
    float GetShellScale() const { return mShellScale; }

private:
    bool mIsActive = false;
    bool mNeedsWorldSwitch = false;
    bool mHasSwitched = false;
    float mTimer = 0.0f;

    // 시각 효과 수치들
    float mDistortionIntensity = 0.0f; // 왜곡 강도
    float mPurpleLerpAmount = 0.0f;    // 채도 제거 및 보라빛 렌더링 수치
    float mVignetteIntensity = 0.0f;   // 화면 테두리 어두워짐 강도
    float mFlashAmount = 0.0f;         // 화이트 플래시 (0.0 ~ 1.0)
    float mShellScale = 0.0f;          // 구체 팽창 비율 (0.0 ~ 1.0)
};
