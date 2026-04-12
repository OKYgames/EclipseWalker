#include "WorldTransitionEffect.h"
#include <cmath>

void WorldTransitionEffect::Start(bool toOtherWorld)
{
    mActive = true;
    mSwapFired = false;
    mTimer = 0.0f;
    mConst = {};
}

TransitionEvent WorldTransitionEffect::Update(const GameTimer& gt)
{
    if (!mActive) return TransitionEvent::NONE;

    mTimer += gt.DeltaTime();

    // ── 1단계: UV 왜곡 (0 ~ 0.5초) ──────────────────
    if (mTimer < T_DISTORT)
    {
        float t = mTimer / T_DISTORT;
        mConst.distortAmount = t;
        mConst.colorShift = 0.0f;
        mConst.vignetteStrength = t * 0.4f;
        mConst.flashAmount = 0.0f;
        return TransitionEvent::NONE;
    }

    // ── 2단계: 색조 전환 + 비네트 (0.5 ~ 1.5초) ────
    float t2 = mTimer - T_DISTORT;
    if (t2 < T_COLORSHIFT)
    {
        float t = t2 / T_COLORSHIFT;
        mConst.distortAmount = 1.0f - t * 0.5f;
        mConst.colorShift = t;
        mConst.vignetteStrength = 0.4f + t * 0.6f;
        mConst.flashAmount = 0.0f;
        return TransitionEvent::NONE;
    }

    // ── 3단계: 화이트 플래시 (1.5 ~ 1.9초) ─────────
    float t3 = mTimer - T_DISTORT - T_COLORSHIFT;
    if (t3 < T_FLASH)
    {
        float t = t3 / T_FLASH;
        mConst.flashAmount = sinf(t * 3.14159f);
        mConst.vignetteStrength = 1.0f - t;
        mConst.distortAmount = 0.5f * (1.0f - t);

        // 플래시 정점에서 딱 한 번 스왑 신호
        if (!mSwapFired && mConst.flashAmount > 0.95f)
        {
            mSwapFired = true;
            return TransitionEvent::SWAP_NOW;
        }
        return TransitionEvent::NONE;
    }

    // ── 완료 ─────────────────────────────────────────
    mActive = false;
    mConst = {};
    return TransitionEvent::DONE;
}