#include "WorldTransitionEffect.h"
#include <cmath>
#include <cstdlib> // rand()

WorldTransitionEffect::WorldTransitionEffect() {}
WorldTransitionEffect::~WorldTransitionEffect() {}

namespace
{
    float Clamp01(float value)
    {
        if (value < 0.0f) return 0.0f;
        if (value > 1.0f) return 1.0f;
        return value;
    }

    float SmoothStep01(float value)
    {
        value = Clamp01(value);
        return value * value * (3.0f - 2.0f * value);
    }
}

void WorldTransitionEffect::StartTransition()
{
    mIsActive = true;
    mNeedsWorldSwitch = false;
    mHasSwitched = false;
    mTimer = 0.0f;

    mDistortionIntensity = 0.0f;
    mPurpleLerpAmount = 0.0f;
    mVignetteIntensity = 0.0f;
    mFlashAmount = 0.0f;
    mShellScale = 0.0f;
}

void WorldTransitionEffect::Update(const GameTimer& gt, DirectX::XMFLOAT3& cameraPos)
{
    if (!mIsActive) return;

    mTimer += gt.DeltaTime();

    constexpr float kChargeEnd = 0.35f;
    constexpr float kVeilEnd = 0.85f;
    constexpr float kFlashPeak = 1.05f;
    constexpr float kFadeEnd = 1.55f;

    mDistortionIntensity = 0.0f;
    mPurpleLerpAmount = 0.0f;
    mVignetteIntensity = 0.0f;
    mFlashAmount = 0.0f;

    if (mTimer < kChargeEnd)
    {
        const float t = SmoothStep01(mTimer / kChargeEnd);
        mDistortionIntensity = 0.35f + (t * 1.15f);
        mPurpleLerpAmount = t * 0.18f;
        mVignetteIntensity = t * 0.2f;
        mShellScale = 0.08f + (t * 0.32f);

        float shakeForce = 0.02f + (t * 0.06f);
        cameraPos.x += ((rand() % 100 / 100.0f) - 0.5f) * shakeForce;
        cameraPos.y += ((rand() % 100 / 100.0f) - 0.5f) * shakeForce;
        cameraPos.z += ((rand() % 100 / 100.0f) - 0.5f) * shakeForce;
    }
    else if (mTimer < kVeilEnd)
    {
        const float t = SmoothStep01((mTimer - kChargeEnd) / (kVeilEnd - kChargeEnd));
        const float pulse = 0.5f + (0.5f * sinf((mTimer - kChargeEnd) * 16.0f));

        mDistortionIntensity = 1.45f - (t * 0.65f);
        mPurpleLerpAmount = 0.18f + (t * 0.62f);
        mVignetteIntensity = 0.2f + (t * 0.55f);
        mShellScale = 0.4f + (t * 0.48f);

        float shakeForce = 0.02f + (pulse * 0.03f);
        cameraPos.x += ((rand() % 100 / 100.0f) - 0.5f) * shakeForce;
        cameraPos.y += ((rand() % 100 / 100.0f) - 0.5f) * shakeForce;
    }
    else if (mTimer < kFlashPeak)
    {
        const float t = Clamp01((mTimer - kVeilEnd) / (kFlashPeak - kVeilEnd));
        mDistortionIntensity = 0.8f + ((1.0f - t) * 0.25f);
        mPurpleLerpAmount = 0.8f + (t * 0.2f);
        mVignetteIntensity = 0.75f + (t * 0.25f);
        mFlashAmount = sinf(t * 3.141592f);
        mShellScale = 0.88f + (t * 0.55f);

        if (t >= 0.5f && !mHasSwitched)
        {
            mNeedsWorldSwitch = true;
            mHasSwitched = true;
        }
    }
    else if (mTimer < kFadeEnd)
    {
        const float t = SmoothStep01((mTimer - kFlashPeak) / (kFadeEnd - kFlashPeak));
        mDistortionIntensity = (1.0f - t) * 0.35f;
        mPurpleLerpAmount = (1.0f - t) * 0.4f;
        mVignetteIntensity = (1.0f - t) * 0.35f;
        mFlashAmount = (1.0f - t) * 0.25f;
        mShellScale = 1.0f - (t * 0.75f);
    }
    else
    {
        mIsActive = false;
        mNeedsWorldSwitch = false;
        mTimer = 0.0f;
        mDistortionIntensity = 0.0f;
        mPurpleLerpAmount = 0.0f;
        mVignetteIntensity = 0.0f;
        mFlashAmount = 0.0f;
        mShellScale = 0.0f;
    }
}
