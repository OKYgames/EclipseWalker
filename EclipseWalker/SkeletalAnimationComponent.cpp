#include "SkeletalAnimationComponent.h"

bool SkeletalAnimationComponent::Load(const std::string& filePath, const std::string& defaultClipName)
{
    mLoaded = mLoader.Load(filePath, defaultClipName);
    if (!mLoaded)
    {
        return false;
    }

    mAnimator.Initialize(&mLoader.m_BoneMapping, &mLoader.m_BoneInfo);
    if (!mLoader.m_Animations.empty())
    {
        mCurrentClipIndex = 0;
        mAnimator.PlayAnimation(&mLoader.m_Animations[mCurrentClipIndex]);
    }

    return true;
}

void SkeletalAnimationComponent::Update(float dt)
{
    if (!mLoaded)
    {
        return;
    }

    mAnimator.UpdateAnimation(dt);
}

bool SkeletalAnimationComponent::Play(size_t clipIndex)
{
    if (!mLoaded || clipIndex >= mLoader.m_Animations.size())
    {
        return false;
    }

    mCurrentClipIndex = clipIndex;
    mAnimator.PlayAnimation(&mLoader.m_Animations[mCurrentClipIndex]);
    return true;
}

bool SkeletalAnimationComponent::Play(const std::string& clipName)
{
    if (!mLoaded)
    {
        return false;
    }

    for (size_t i = 0; i < mLoader.m_Animations.size(); ++i)
    {
        if (mLoader.m_Animations[i].Name == clipName)
        {
            return Play(i);
        }
    }

    return false;
}
