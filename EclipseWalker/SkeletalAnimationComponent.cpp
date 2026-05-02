#include "SkeletalAnimationComponent.h"

#include <utility>

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

bool SkeletalAnimationComponent::LoadAdditionalAnimation(const std::string& filePath, const std::string& clipName)
{
    if (!mLoaded)
    {
        return false;
    }

    AnimationLoader clipLoader;
    if (!clipLoader.Load(filePath, clipName) || clipLoader.m_Animations.empty())
    {
        return false;
    }

    const size_t currentClipIndex = mCurrentClipIndex;
    for (auto& animation : clipLoader.m_Animations)
    {
        mLoader.m_Animations.push_back(std::move(animation));
    }

    if (currentClipIndex < mLoader.m_Animations.size())
    {
        mCurrentClipIndex = currentClipIndex;
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

bool SkeletalAnimationComponent::TryGetSocketLocalTransform(const std::string& socketName, DirectX::XMFLOAT4X4& outTransform) const
{
    if (!mLoaded)
    {
        return false;
    }

    const auto& nodeMap = mAnimator.GetGlobalNodeMap();
    auto nodeIt = nodeMap.find(socketName);
    if (nodeIt != nodeMap.end())
    {
        outTransform = nodeIt->second;
        return true;
    }

    const auto& boneMapping = mLoader.GetBoneMapping();
    auto boneIt = boneMapping.find(socketName);
    if (boneIt == boneMapping.end())
    {
        return false;
    }

    const auto& globalBoneMatrices = mAnimator.GetGlobalBoneMatrices();
    const size_t boneIndex = static_cast<size_t>(boneIt->second);
    if (boneIndex >= globalBoneMatrices.size())
    {
        return false;
    }

    outTransform = globalBoneMatrices[boneIndex];
    return true;
}
