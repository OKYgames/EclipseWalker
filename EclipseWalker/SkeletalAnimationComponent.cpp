#include "SkeletalAnimationComponent.h"

#include "AnimationUtils.h"

#include <algorithm>
#include <map>
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

    void BuildNodeMap(const NodeData& node, std::map<std::string, const NodeData*>& nodeMap)
    {
        nodeMap[node.Name] = &node;
        for (const auto& child : node.Children)
        {
            BuildNodeMap(child, nodeMap);
        }
    }

    DirectX::XMVECTOR EvaluatePosition(
        const std::vector<KeyPosition>& keys,
        double animationTime,
        DirectX::FXMVECTOR fallback)
    {
        if (keys.empty())
        {
            return fallback;
        }

        const size_t index = AnimationUtils::FindKeyIndex(keys, animationTime);
        const size_t nextIndex = (index + 1 < keys.size()) ? index + 1 : index;
        const double duration = keys[nextIndex].TimeStamp - keys[index].TimeStamp;
        const float t = duration > 0.0 ? static_cast<float>((animationTime - keys[index].TimeStamp) / duration) : 0.0f;

        const DirectX::XMVECTOR current = DirectX::XMVectorSet(
            keys[index].Position[0],
            keys[index].Position[1],
            keys[index].Position[2],
            1.0f);
        const DirectX::XMVECTOR next = DirectX::XMVectorSet(
            keys[nextIndex].Position[0],
            keys[nextIndex].Position[1],
            keys[nextIndex].Position[2],
            1.0f);

        return DirectX::XMVectorLerp(current, next, t);
    }

    DirectX::XMVECTOR EvaluateRotation(
        const std::vector<KeyRotation>& keys,
        double animationTime,
        DirectX::FXMVECTOR fallback)
    {
        if (keys.empty())
        {
            return fallback;
        }

        const size_t index = AnimationUtils::FindKeyIndex(keys, animationTime);
        const size_t nextIndex = (index + 1 < keys.size()) ? index + 1 : index;
        const double duration = keys[nextIndex].TimeStamp - keys[index].TimeStamp;
        const float t = duration > 0.0 ? static_cast<float>((animationTime - keys[index].TimeStamp) / duration) : 0.0f;

        DirectX::XMVECTOR current = DirectX::XMVectorSet(
            keys[index].Orientation[0],
            keys[index].Orientation[1],
            keys[index].Orientation[2],
            keys[index].Orientation[3]);
        DirectX::XMVECTOR next = DirectX::XMVectorSet(
            keys[nextIndex].Orientation[0],
            keys[nextIndex].Orientation[1],
            keys[nextIndex].Orientation[2],
            keys[nextIndex].Orientation[3]);

        current = DirectX::XMQuaternionNormalize(current);
        next = DirectX::XMQuaternionNormalize(next);
        if (DirectX::XMVectorGetX(DirectX::XMVector4Dot(current, next)) < 0.0f)
        {
            next = DirectX::XMVectorNegate(next);
        }

        return DirectX::XMQuaternionSlerp(current, next, t);
    }

    DirectX::XMVECTOR EvaluateScale(
        const std::vector<KeyScale>& keys,
        double animationTime,
        DirectX::FXMVECTOR fallback)
    {
        if (keys.empty())
        {
            return fallback;
        }

        const size_t index = AnimationUtils::FindKeyIndex(keys, animationTime);
        const size_t nextIndex = (index + 1 < keys.size()) ? index + 1 : index;
        const double duration = keys[nextIndex].TimeStamp - keys[index].TimeStamp;
        const float t = duration > 0.0 ? static_cast<float>((animationTime - keys[index].TimeStamp) / duration) : 0.0f;

        const DirectX::XMVECTOR current = DirectX::XMVectorSet(
            keys[index].Scale[0],
            keys[index].Scale[1],
            keys[index].Scale[2],
            0.0f);
        const DirectX::XMVECTOR next = DirectX::XMVectorSet(
            keys[nextIndex].Scale[0],
            keys[nextIndex].Scale[1],
            keys[nextIndex].Scale[2],
            0.0f);

        return DirectX::XMVectorLerp(current, next, t);
    }

    DirectX::XMMATRIX EvaluateSourceLocalTransform(
        const NodeData& sourceNode,
        const BoneAnimation& sourceAnimation,
        double animationTime)
    {
        const DirectX::XMMATRIX sourceRest = DirectX::XMLoadFloat4x4(&sourceNode.LocalTransform);

        DirectX::XMVECTOR restScale;
        DirectX::XMVECTOR restRotation;
        DirectX::XMVECTOR restTranslation;
        if (!DirectX::XMMatrixDecompose(&restScale, &restRotation, &restTranslation, sourceRest))
        {
            restScale = DirectX::XMVectorSet(1.0f, 1.0f, 1.0f, 0.0f);
            restRotation = DirectX::XMQuaternionIdentity();
            restTranslation = DirectX::XMVectorSet(
                sourceNode.LocalTransform._41,
                sourceNode.LocalTransform._42,
                sourceNode.LocalTransform._43,
                1.0f);
        }

        const DirectX::XMVECTOR scale = EvaluateScale(sourceAnimation.Scales, animationTime, restScale);
        const DirectX::XMVECTOR rotation = EvaluateRotation(sourceAnimation.Rotations, animationTime, restRotation);
        const DirectX::XMVECTOR translation = EvaluatePosition(sourceAnimation.Positions, animationTime, restTranslation);

        return DirectX::XMMatrixScalingFromVector(scale) *
            DirectX::XMMatrixRotationQuaternion(rotation) *
            DirectX::XMMatrixTranslationFromVector(translation);
    }

    DirectX::XMMATRIX RetargetLocalTransformToModelRestPose(
        DirectX::FXMMATRIX sourceAnimatedLocal,
        DirectX::CXMMATRIX sourceRestLocal,
        DirectX::CXMMATRIX targetRestLocal)
    {
        const DirectX::XMMATRIX sourceRestInverse = DirectX::XMMatrixInverse(nullptr, sourceRestLocal);
        return sourceAnimatedLocal * sourceRestInverse * targetRestLocal;
    }

    bool DecomposeRetargetedTransform(
        DirectX::FXMMATRIX transform,
        DirectX::XMVECTOR& outScale,
        DirectX::XMVECTOR& outRotation,
        DirectX::XMVECTOR& outTranslation)
    {
        if (!DirectX::XMMatrixDecompose(&outScale, &outRotation, &outTranslation, transform))
        {
            return false;
        }

        outRotation = DirectX::XMQuaternionNormalize(outRotation);
        return true;
    }

    void RetargetAnimationClipToModelRestPose(
        AnimationClip& animation,
        const NodeData& sourceRootNode,
        const NodeData& targetRootNode,
        const std::map<std::string, unsigned int>& targetBoneMapping)
    {
        std::map<std::string, const NodeData*> sourceNodes;
        std::map<std::string, const NodeData*> targetNodes;
        BuildNodeMap(sourceRootNode, sourceNodes);
        BuildNodeMap(targetRootNode, targetNodes);

        for (auto& boneAnimation : animation.BoneAnimations)
        {
            auto sourceNodeIt = sourceNodes.find(boneAnimation.BoneName);
            auto targetNodeIt = targetNodes.find(boneAnimation.BoneName);
            if (sourceNodeIt == sourceNodes.end() || targetNodeIt == targetNodes.end())
            {
                continue;
            }

            BoneAnimation sourceAnimation = boneAnimation;
            sourceAnimation.Scales.clear();
            boneAnimation.Scales.clear();

            const bool isDeformingBone =
                targetBoneMapping.find(boneAnimation.BoneName) != targetBoneMapping.end();

            const DirectX::XMMATRIX sourceRest = DirectX::XMLoadFloat4x4(&sourceNodeIt->second->LocalTransform);
            const DirectX::XMMATRIX targetRest = DirectX::XMLoadFloat4x4(&targetNodeIt->second->LocalTransform);

            if (isDeformingBone)
            {
                boneAnimation.Positions.clear();
            }
            else
            {
                for (auto& key : boneAnimation.Positions)
                {
                    const DirectX::XMMATRIX sourceAnimated =
                        EvaluateSourceLocalTransform(*sourceNodeIt->second, sourceAnimation, key.TimeStamp);
                    const DirectX::XMMATRIX retargeted =
                        RetargetLocalTransformToModelRestPose(sourceAnimated, sourceRest, targetRest);

                    DirectX::XMVECTOR scale;
                    DirectX::XMVECTOR rotation;
                    DirectX::XMVECTOR translation;
                    if (DecomposeRetargetedTransform(retargeted, scale, rotation, translation))
                    {
                        key.Position[0] = DirectX::XMVectorGetX(translation);
                        key.Position[1] = DirectX::XMVectorGetY(translation);
                        key.Position[2] = DirectX::XMVectorGetZ(translation);
                    }
                }
            }

            for (auto& key : boneAnimation.Rotations)
            {
                const DirectX::XMMATRIX sourceAnimated =
                    EvaluateSourceLocalTransform(*sourceNodeIt->second, sourceAnimation, key.TimeStamp);
                const DirectX::XMMATRIX retargeted =
                    RetargetLocalTransformToModelRestPose(sourceAnimated, sourceRest, targetRest);

                DirectX::XMVECTOR scale;
                DirectX::XMVECTOR rotation;
                DirectX::XMVECTOR translation;
                if (DecomposeRetargetedTransform(retargeted, scale, rotation, translation))
                {
                    key.Orientation[0] = DirectX::XMVectorGetX(rotation);
                    key.Orientation[1] = DirectX::XMVectorGetY(rotation);
                    key.Orientation[2] = DirectX::XMVectorGetZ(rotation);
                    key.Orientation[3] = DirectX::XMVectorGetW(rotation);
                }
            }

        }

        animation.RootNode = targetRootNode;
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
    const std::string& clipName,
    bool retargetToModelRestPose)
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
        if (retargetToModelRestPose)
        {
            RetargetAnimationClipToModelRestPose(
                animation,
                clipLoader.GetRootNode(),
                mLoader.GetRootNode(),
                mLoader.GetBoneMapping());
        }
        else
        {
            animation.RootNode = mLoader.GetRootNode();
        }

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
