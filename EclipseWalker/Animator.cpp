#include "Animator.h"

#include <algorithm>
#include <cmath>

using namespace DirectX;

namespace
{
    XMMATRIX BlendLocalTransforms(FXMMATRIX from, CXMMATRIX to, float t)
    {
        XMVECTOR fromScale;
        XMVECTOR fromRot;
        XMVECTOR fromTrans;
        XMVECTOR toScale;
        XMVECTOR toRot;
        XMVECTOR toTrans;

        if (!XMMatrixDecompose(&fromScale, &fromRot, &fromTrans, from))
        {
            fromScale = XMVectorSet(1.0f, 1.0f, 1.0f, 0.0f);
            fromRot = XMQuaternionIdentity();
            fromTrans = XMVectorZero();
        }

        if (!XMMatrixDecompose(&toScale, &toRot, &toTrans, to))
        {
            toScale = XMVectorSet(1.0f, 1.0f, 1.0f, 0.0f);
            toRot = XMQuaternionIdentity();
            toTrans = XMVectorZero();
        }

        fromRot = XMQuaternionNormalize(fromRot);
        toRot = XMQuaternionNormalize(toRot);

        if (XMVectorGetX(XMVector4Dot(fromRot, toRot)) < 0.0f)
        {
            toRot = XMVectorNegate(toRot);
        }

        const XMVECTOR scale = XMVectorLerp(fromScale, toScale, t);
        const XMVECTOR rotation = XMQuaternionSlerp(fromRot, toRot, t);
        const XMVECTOR translation = XMVectorLerp(fromTrans, toTrans, t);

        return XMMatrixScalingFromVector(scale) *
            XMMatrixRotationQuaternion(rotation) *
            XMMatrixTranslationFromVector(translation);
    }
}

Animator::Animator()
{
    ResetBoneMatrices(1);
    XMStoreFloat4x4(&m_GlobalInverseTransform, XMMatrixIdentity());
}

void Animator::Initialize(std::map<std::string, unsigned int>* boneMapping, std::vector<BoneInfo>* boneInfo)
{
    m_BoneMapping = boneMapping;
    m_BoneInfo = boneInfo;
    ResetBoneMatrices(GetRequiredBoneMatrixCount());
}

void Animator::PlayAnimation(AnimationClip* animation, float blendDuration)
{
    if (animation != nullptr && m_CurrentAnimation != nullptr && m_CurrentAnimation != animation && blendDuration > 0.0f)
    {
        m_PreviousAnimation = m_CurrentAnimation;
        m_PreviousTime = m_CurrentTime;
        m_BlendTime = 0.0f;
        m_BlendDuration = blendDuration;
    }
    else
    {
        m_PreviousAnimation = nullptr;
        m_PreviousTime = 0.0f;
        m_BlendTime = 0.0f;
        m_BlendDuration = 0.0f;
    }

    m_CurrentAnimation = animation;
    m_CurrentTime = 0.0f;
    m_IsPaused = false;
    m_UseBindPoseOnly = false;

    if (m_CurrentAnimation)
    {
        XMMATRIX rootTransform = XMLoadFloat4x4(&m_CurrentAnimation->RootNode.LocalTransform);
        XMMATRIX globalInverse = XMMatrixInverse(nullptr, rootTransform);
        XMStoreFloat4x4(&m_GlobalInverseTransform, globalInverse);
    }
    else
    {
        XMStoreFloat4x4(&m_GlobalInverseTransform, XMMatrixIdentity());
    }
}

void Animator::SetPlaybackSpeed(float speed)
{
    m_PlaybackSpeed = (speed > 0.0f) ? speed : 1.0f;
}

float Animator::GetPlaybackSpeed() const
{
    return m_PlaybackSpeed;
}

void Animator::PauseAnimation()
{
    m_IsPaused = true;
}

void Animator::ResumeAnimation()
{
    m_IsPaused = false;
}

void Animator::TogglePause()
{
    m_IsPaused = !m_IsPaused;
}

bool Animator::IsPaused() const
{
    return m_IsPaused;
}

void Animator::EnableBindPoseMode(bool enable)
{
    m_UseBindPoseOnly = enable;
}

void Animator::ToggleBindPoseMode()
{
    m_UseBindPoseOnly = !m_UseBindPoseOnly;
}

bool Animator::IsBindPoseMode() const
{
    return m_UseBindPoseOnly;
}

void Animator::UpdateAnimation(float dt)
{
    if (m_CurrentAnimation == nullptr)
    {
        return;
    }

    const std::size_t requiredBoneCount = GetRequiredBoneMatrixCount();
    if (m_FinalBoneMatrices.size() != requiredBoneCount || m_GlobalBoneMatrices.size() != requiredBoneCount)
    {
        ResetBoneMatrices(requiredBoneCount);
    }

    if (!m_IsPaused && !m_UseBindPoseOnly)
    {
        m_CurrentTime += m_CurrentAnimation->TicksPerSecond * dt * m_PlaybackSpeed;
        if (m_CurrentAnimation->Duration > 0.0f)
        {
            m_CurrentTime = std::fmod(m_CurrentTime, m_CurrentAnimation->Duration);
        }

        if (IsBlending())
        {
            m_PreviousTime += m_PreviousAnimation->TicksPerSecond * dt * m_PlaybackSpeed;
            if (m_PreviousAnimation->Duration > 0.0f)
            {
                m_PreviousTime = std::fmod(m_PreviousTime, m_PreviousAnimation->Duration);
            }

            m_BlendTime += dt;
            if (m_BlendTime >= m_BlendDuration)
            {
                m_PreviousAnimation = nullptr;
                m_PreviousTime = 0.0f;
                m_BlendTime = 0.0f;
                m_BlendDuration = 0.0f;
            }
        }
    }

    m_GlobalNodeTransforms.clear();
    CalculateNodeTransform(&m_CurrentAnimation->RootNode, XMMatrixIdentity());
}

const std::vector<XMFLOAT4X4>& Animator::GetFinalBoneMatrices() const
{
    return m_FinalBoneMatrices;
}

const std::vector<XMFLOAT4X4>& Animator::GetGlobalBoneMatrices() const
{
    return m_GlobalBoneMatrices;
}

const std::map<std::string, XMFLOAT4X4>& Animator::GetGlobalNodeMap() const
{
    return m_GlobalNodeTransforms;
}

const BoneAnimation* Animator::FindBoneAnimation(const AnimationClip* animation, const std::string& nodeName) const
{
    if (animation == nullptr)
    {
        return nullptr;
    }

    for (const auto& boneAnim : animation->BoneAnimations)
    {
        if (boneAnim.BoneName == nodeName)
        {
            return &boneAnim;
        }
    }

    return nullptr;
}

std::size_t Animator::GetRequiredBoneMatrixCount() const
{
    std::size_t boneCount = 0;

    if (m_BoneInfo)
    {
        boneCount = m_BoneInfo->size();
    }

    if (m_BoneMapping)
    {
        for (const auto& pair : *m_BoneMapping)
        {
            const std::size_t mappedIndex = static_cast<std::size_t>(pair.second);
            if (boneCount <= mappedIndex)
            {
                boneCount = mappedIndex + 1;
            }
        }
    }

    return (boneCount > 0) ? boneCount : 1;
}

void Animator::ResetBoneMatrices(std::size_t boneCount)
{
    if (boneCount == 0)
    {
        boneCount = 1;
    }

    m_FinalBoneMatrices.resize(boneCount);
    m_GlobalBoneMatrices.resize(boneCount);

    for (std::size_t i = 0; i < boneCount; ++i)
    {
        XMStoreFloat4x4(&m_FinalBoneMatrices[i], XMMatrixIdentity());
        XMStoreFloat4x4(&m_GlobalBoneMatrices[i], XMMatrixIdentity());
    }
}

void Animator::CalculateNodeTransform(const NodeData* node, XMMATRIX parentTransform)
{
    const std::string nodeName = node->Name;
    XMMATRIX nodeTransform = XMLoadFloat4x4(&node->LocalTransform);

    if (!m_UseBindPoseOnly)
    {
        nodeTransform = CalculateLocalTransform(node, m_CurrentAnimation, m_CurrentTime);
        if (IsBlending())
        {
            const float blendAlpha = std::clamp(m_BlendTime / m_BlendDuration, 0.0f, 1.0f);
            const XMMATRIX previousTransform = CalculateLocalTransform(node, m_PreviousAnimation, m_PreviousTime);
            nodeTransform = BlendLocalTransforms(previousTransform, nodeTransform, blendAlpha);
        }
    }

    XMMATRIX globalTransform = nodeTransform * parentTransform;

    XMFLOAT4X4 globalFloat;
    XMStoreFloat4x4(&globalFloat, globalTransform);
    m_GlobalNodeTransforms[nodeName] = globalFloat;

    if (m_BoneMapping && m_BoneInfo)
    {
        auto it = m_BoneMapping->find(nodeName);
        if (it != m_BoneMapping->end())
        {
            const unsigned int boneIndex = it->second;

            if (boneIndex < m_GlobalBoneMatrices.size())
            {
                XMStoreFloat4x4(&m_GlobalBoneMatrices[boneIndex], globalTransform);
            }

            if (boneIndex < m_BoneInfo->size() && boneIndex < m_FinalBoneMatrices.size())
            {
                XMMATRIX offset = XMLoadFloat4x4(&(*m_BoneInfo)[boneIndex].OffsetMatrix);
                XMMATRIX globalInverse = XMLoadFloat4x4(&m_GlobalInverseTransform);
                XMMATRIX finalMatrix = offset * globalTransform * globalInverse;
                XMStoreFloat4x4(&m_FinalBoneMatrices[boneIndex], finalMatrix);
            }
        }
    }

    for (const auto& child : node->Children)
    {
        CalculateNodeTransform(&child, globalTransform);
    }
}

XMMATRIX Animator::CalculateLocalTransform(const NodeData* node, const AnimationClip* animation, float animationTime) const
{
    XMMATRIX nodeTransform = XMLoadFloat4x4(&node->LocalTransform);
    const BoneAnimation* boneAnim = FindBoneAnimation(animation, node->Name);
    if (boneAnim == nullptr)
    {
        return nodeTransform;
    }

    XMVECTOR baseScale;
    XMVECTOR baseRot;
    XMVECTOR baseTrans;
    if (!XMMatrixDecompose(&baseScale, &baseRot, &baseTrans, nodeTransform))
    {
        baseScale = XMVectorSet(1.0f, 1.0f, 1.0f, 0.0f);
        baseRot = XMQuaternionIdentity();
        baseTrans = XMVectorSet(node->LocalTransform._41, node->LocalTransform._42, node->LocalTransform._43, 1.0f);
    }

    XMVECTOR scaling = baseScale;
    XMVECTOR rotation = baseRot;
    XMVECTOR translation = baseTrans;

    if (!boneAnim->Scales.empty())
    {
        const size_t idx = AnimationUtils::FindKeyIndex(boneAnim->Scales, animationTime);
        const size_t nextIdx = (idx + 1 < boneAnim->Scales.size()) ? idx + 1 : idx;
        const float duration = static_cast<float>(boneAnim->Scales[nextIdx].TimeStamp - boneAnim->Scales[idx].TimeStamp);
        const float t = (duration > 0.0f) ? static_cast<float>((animationTime - boneAnim->Scales[idx].TimeStamp) / duration) : 0.0f;
        scaling = AnimationUtils::InterpolateScale(boneAnim->Scales[idx], boneAnim->Scales[nextIdx], t);
    }

    if (!boneAnim->Rotations.empty())
    {
        const size_t idx = AnimationUtils::FindKeyIndex(boneAnim->Rotations, animationTime);
        const size_t nextIdx = (idx + 1 < boneAnim->Rotations.size()) ? idx + 1 : idx;
        const float duration = static_cast<float>(boneAnim->Rotations[nextIdx].TimeStamp - boneAnim->Rotations[idx].TimeStamp);
        const float t = (duration > 0.0f) ? static_cast<float>((animationTime - boneAnim->Rotations[idx].TimeStamp) / duration) : 0.0f;
        rotation = AnimationUtils::InterpolateRotation(boneAnim->Rotations[idx], boneAnim->Rotations[nextIdx], t);
    }

    if (!boneAnim->Positions.empty())
    {
        const size_t idx = AnimationUtils::FindKeyIndex(boneAnim->Positions, animationTime);
        const size_t nextIdx = (idx + 1 < boneAnim->Positions.size()) ? idx + 1 : idx;
        const float duration = static_cast<float>(boneAnim->Positions[nextIdx].TimeStamp - boneAnim->Positions[idx].TimeStamp);
        const float t = (duration > 0.0f) ? static_cast<float>((animationTime - boneAnim->Positions[idx].TimeStamp) / duration) : 0.0f;
        translation = AnimationUtils::InterpolatePosition(boneAnim->Positions[idx], boneAnim->Positions[nextIdx], t);
    }

    return XMMatrixScalingFromVector(scaling) *
        XMMatrixRotationQuaternion(rotation) *
        XMMatrixTranslationFromVector(translation);
}

bool Animator::IsBlending() const
{
    return m_PreviousAnimation != nullptr && m_BlendDuration > 0.0f && m_BlendTime < m_BlendDuration;
}
