#pragma once
#include "GameTimer.h"

enum class TransitionEvent { NONE, SWAP_NOW, DONE };

struct TransitionConstants {
    float distortAmount = 0.0f;
    float colorShift = 0.0f;
    float vignetteStrength = 0.0f;
    float flashAmount = 0.0f;
    float pad[4] = {};
};

class WorldTransitionEffect
{
public:
    void Start(bool toOtherWorld);
    TransitionEvent Update(const GameTimer& gt);

    bool IsActive() const { return mActive; }
    TransitionConstants GetConstants() const { return mConst; }

private:
    bool  mActive = false;
    bool  mSwapFired = false;
    float mTimer = 0.0f;

    static constexpr float T_DISTORT = 0.5f;
    static constexpr float T_COLORSHIFT = 1.0f;
    static constexpr float T_FLASH = 0.4f;

    TransitionConstants mConst = {};
};