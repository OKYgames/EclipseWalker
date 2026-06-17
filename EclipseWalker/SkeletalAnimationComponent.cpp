#include "SkeletalAnimationComponent.h"

#include <algorithm>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>

namespace
{
    std::mutex gAnimationLoaderCacheMutex;
    std::unordered_map<std::string, AnimationLoader> gAnimationLoaderCache;

    std::string MakeAnimationCacheKey(
        const std::string& filePath,
        const std::string& clipName,
        bool loadAnimations)
    {
        return filePath + "|" + clipName + "|" + (loadAnimations ? "anim" : "mesh");
    }

    bool TryGetCachedLoader(const std::string& cacheKey, AnimationLoader& outLoader)
    {
        std::lock_guard<std::mutex> lock(gAnimationLoaderCacheMutex);
        auto it = gAnimationLoaderCache.find(cacheKey);
        if (it == gAnimationLoaderCache.end())
        {
            return false;
        }

        outLoader = it->second;
        return true;
    }

    void StoreCachedLoader(const std::string& cacheKey, const AnimationLoader& loader)
    {
        std::lock_guard<std::mutex> lock(gAnimationLoaderCacheMutex);
        gAnimationLoaderCache.emplace(cacheKey, loader);
    }
}

bool SkeletalAnimationComponent::Load(const std::string& filePath, const std::string& defaultClipName, bool loadAnimations)
{
    const std::string cacheKey = MakeAnimationCacheKey(filePath, defaultClipName, loadAnimations);
    mLoaded = TryGetCachedLoader(cacheKey, mLoader);
    if (!mLoaded)
    {
        mLoaded = mLoader.Load(filePath, defaultClipName, loadAnimations);
        if (mLoaded)
        {
            StoreCachedLoader(cacheKey, mLoader);
        }
    }

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

bool SkeletalAnimationComponent::LoadAdditionalAnimation(
    const std::string& filePath,
    const std::string& clipName)
{
    if (!mLoaded)
    {
        return false;
    }

    AnimationLoader clipLoader;
    const std::string cacheKey = MakeAnimationCacheKey(filePath, clipName, true);
    bool clipLoaded = TryGetCachedLoader(cacheKey, clipLoader);
    if (!clipLoaded)
    {
        clipLoaded = clipLoader.Load(filePath, clipName);
        if (clipLoaded)
        {
            StoreCachedLoader(cacheKey, clipLoader);
        }
    }

    if (!clipLoaded || clipLoader.m_Animations.empty())
    {
        return false;
    }

    const size_t currentClipIndex = mCurrentClipIndex;
    for (auto animation : clipLoader.m_Animations)
    {
        animation.RootNode = mLoader.GetRootNode();
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

bool SkeletalAnimationComponent::Play(size_t clipIndex, float blendDuration, float playbackSpeed)
{
    if (!mLoaded || clipIndex >= mLoader.m_Animations.size())
    {
        return false;
    }

    mCurrentClipIndex = clipIndex;
    mAnimator.SetPlaybackSpeed(playbackSpeed);
    mAnimator.PlayAnimation(&mLoader.m_Animations[mCurrentClipIndex], blendDuration);
    return true;
}

bool SkeletalAnimationComponent::Play(const std::string& clipName, float blendDuration, float playbackSpeed)
{
    if (!mLoaded)
    {
        return false;
    }

    for (size_t i = 0; i < mLoader.m_Animations.size(); ++i)
    {
        if (mLoader.m_Animations[i].Name == clipName)
        {
            return Play(i, blendDuration, playbackSpeed);
        }
    }

    return false;
}

float SkeletalAnimationComponent::GetClipDurationSeconds(const std::string& clipName) const
{
    if (!mLoaded)
    {
        return 0.0f;
    }

    for (const auto& animation : mLoader.m_Animations)
    {
        if (animation.Name == clipName)
        {
            const float ticksPerSecond = animation.TicksPerSecond > 0.0f ? animation.TicksPerSecond : 24.0f;
            return animation.Duration > 0.0f ? animation.Duration / ticksPerSecond : 0.0f;
        }
    }

    return 0.0f;
}

void SkeletalAnimationComponent::SetPlaybackSpeed(float playbackSpeed)
{
    mAnimator.SetPlaybackSpeed(playbackSpeed);
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
