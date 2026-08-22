#pragma once

#include "AnimationLoader.h"
#include "Animator.h"
#include <DirectXMath.h>
#include <string>
#include <vector>

class SkeletalAnimationComponent
{
public:
    bool Load(const std::string& filePath, const std::string& defaultClipName = "", bool loadAnimations = true);
    bool LoadAdditionalAnimation(
        const std::string& filePath,
        const std::string& clipName = "");
    void Update(float dt);

    bool Play(size_t clipIndex, float blendDuration = 0.0f, float playbackSpeed = 1.0f, bool loop = true);
    bool Play(const std::string& clipName, float blendDuration = 0.0f, float playbackSpeed = 1.0f, bool loop = true);
    float GetClipDurationSeconds(const std::string& clipName) const;
    void SetPlaybackSpeed(float playbackSpeed);
    float GetCurrentAnimationProgress() const;
    void RequestHitStop(float durationSeconds, float timeScale);

    Animator& GetAnimator() { return mAnimator; }
    const Animator& GetAnimator() const { return mAnimator; }
    AnimationLoader& GetLoader() { return mLoader; }
    const AnimationLoader& GetLoader() const { return mLoader; }
    const std::vector<DirectX::XMFLOAT4X4>& GetFinalBoneMatrices() const { return mAnimator.GetFinalBoneMatrices(); }
    bool TryGetSocketLocalTransform(const std::string& socketName, DirectX::XMFLOAT4X4& outTransform) const;

    bool IsLoaded() const { return mLoaded; }
    size_t GetCurrentClipIndex() const { return mCurrentClipIndex; }

private:
    AnimationLoader mLoader;
    Animator mAnimator;
    bool mLoaded = false;
    size_t mCurrentClipIndex = 0;
    float mHitStopRemaining = 0.0f;
    float mHitStopTimeScale = 1.0f;
};
