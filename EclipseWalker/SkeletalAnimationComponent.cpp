#include "SkeletalAnimationComponent.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <Windows.h>

namespace
{
    std::mutex gAnimationLoaderCacheMutex;
    std::unordered_map<std::string, AnimationLoader> gAnimationLoaderCache;

    std::string MakeAnimationCacheKey(
        const std::string& filePath,
        const std::string& clipName,
        bool loadAnimations,
        bool allowAnimationOnly)
    {
        return filePath + "|" + clipName + "|" +
            (loadAnimations ? "anim" : "mesh") + "|" +
            (allowAnimationOnly ? "clip" : "model");
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

    void CollectNodeNames(const NodeData& node, std::unordered_set<std::string>& names)
    {
        names.insert(node.Name);
        for (const NodeData& child : node.Children)
        {
            CollectNodeNames(child, names);
        }
    }

    std::string NormalizeNodeName(const std::string& name)
    {
        std::string normalized;
        normalized.reserve(name.size());
        for (const unsigned char ch : name)
        {
            if (std::isalnum(ch) != 0)
            {
                normalized.push_back(static_cast<char>(std::tolower(ch)));
            }
        }
        return normalized;
    }

    bool RemapAnimationChannelsToModelSkeleton(
        const AnimationLoader& modelLoader,
        AnimationLoader& clipLoader,
        const std::string& filePath)
    {
        std::unordered_set<std::string> modelNodeNames;
        CollectNodeNames(modelLoader.GetRootNode(), modelNodeNames);

        std::unordered_map<std::string, std::string> normalizedModelNames;
        std::unordered_set<std::string> ambiguousModelNames;
        for (const std::string& modelName : modelNodeNames)
        {
            const std::string normalized = NormalizeNodeName(modelName);
            if (normalized.empty())
            {
                continue;
            }

            const auto [it, inserted] = normalizedModelNames.emplace(normalized, modelName);
            if (!inserted && it->second != modelName)
            {
                ambiguousModelNames.insert(normalized);
            }
        }

        size_t exactCount = 0;
        size_t remappedCount = 0;
        size_t unmatchedCount = 0;
        size_t ambiguousCount = 0;
        std::unordered_set<std::string> matchedModelNames;

        for (AnimationClip& clip : clipLoader.m_Animations)
        {
            std::unordered_map<std::string, size_t> sourceNameCounts;
            for (const BoneAnimation& channel : clip.BoneAnimations)
            {
                ++sourceNameCounts[NormalizeNodeName(channel.BoneName)];
            }

            for (BoneAnimation& channel : clip.BoneAnimations)
            {
                if (modelNodeNames.find(channel.BoneName) != modelNodeNames.end())
                {
                    ++exactCount;
                    matchedModelNames.insert(channel.BoneName);
                    continue;
                }

                const std::string normalized = NormalizeNodeName(channel.BoneName);
                const auto modelNameIt = normalizedModelNames.find(normalized);
                const bool isAmbiguous = normalized.empty() ||
                    ambiguousModelNames.find(normalized) != ambiguousModelNames.end() ||
                    sourceNameCounts[normalized] != 1;

                if (isAmbiguous)
                {
                    ++ambiguousCount;
                    continue;
                }
                if (modelNameIt == normalizedModelNames.end())
                {
                    ++unmatchedCount;
                    continue;
                }

                channel.BoneName = modelNameIt->second;
                matchedModelNames.insert(channel.BoneName);
                ++remappedCount;
            }
        }

        size_t matchedDeformBones = 0;
        for (const BoneInfo& bone : modelLoader.GetBoneInfo())
        {
            if (matchedModelNames.find(bone.Name) != matchedModelNames.end())
            {
                ++matchedDeformBones;
            }
        }

        std::ostringstream log;
        log << "[SkeletalAnimation] UFBX channel remap: " << filePath
            << " exact=" << exactCount
            << " remapped=" << remappedCount
            << " unmatched=" << unmatchedCount
            << " ambiguous=" << ambiguousCount
            << " deformCoverage=" << matchedDeformBones
            << "/" << modelLoader.GetBoneInfo().size() << "\n";
        OutputDebugStringA(log.str().c_str());

        return exactCount + remappedCount > 0;
    }
}

bool SkeletalAnimationComponent::Load(const std::string& filePath, const std::string& defaultClipName, bool loadAnimations)
{
    const std::string cacheKey = MakeAnimationCacheKey(filePath, defaultClipName, loadAnimations, false);
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
    const bool allowAnimationOnly = std::filesystem::path(filePath).extension() == ".ufbx";
    const std::string cacheKey = MakeAnimationCacheKey(filePath, clipName, true, allowAnimationOnly);
    bool clipLoaded = TryGetCachedLoader(cacheKey, clipLoader);
    if (!clipLoaded)
    {
        clipLoaded = clipLoader.Load(filePath, clipName, true, allowAnimationOnly);
        if (clipLoaded)
        {
            StoreCachedLoader(cacheKey, clipLoader);
        }
    }

    if (!clipLoaded || clipLoader.m_Animations.empty())
    {
        return false;
    }

    if (allowAnimationOnly &&
        !RemapAnimationChannelsToModelSkeleton(mLoader, clipLoader, filePath))
    {
        OutputDebugStringA((
            "[SkeletalAnimation] UFBX clip has no channels matching the model skeleton: " +
            filePath + "\n").c_str());
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

float SkeletalAnimationComponent::GetCurrentAnimationProgress() const
{
    return mAnimator.GetCurrentAnimationProgress();
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
