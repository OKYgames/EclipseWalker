#include "Animator.h"

#include <cmath>

using namespace DirectX;

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

void Animator::PlayAnimation(AnimationClip* animation)
{
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
        m_CurrentTime += m_CurrentAnimation->TicksPerSecond * dt;
        if (m_CurrentAnimation->Duration > 0.0f)
        {
            m_CurrentTime = std::fmod(m_CurrentTime, m_CurrentAnimation->Duration);
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

BoneAnimation* Animator::FindBoneAnimation(const std::string& nodeName)
{
    if (m_CurrentAnimation == nullptr)
    {
        return nullptr;
    }

    for (auto& boneAnim : m_CurrentAnimation->BoneAnimations)
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
        BoneAnimation* boneAnim = FindBoneAnimation(nodeName);
        if (boneAnim)
        {
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
                const size_t idx = AnimationUtils::FindKeyIndex(boneAnim->Scales, m_CurrentTime);
                const size_t nextIdx = (idx + 1 < boneAnim->Scales.size()) ? idx + 1 : idx;
                const float duration = static_cast<float>(boneAnim->Scales[nextIdx].TimeStamp - boneAnim->Scales[idx].TimeStamp);
                const float t = (duration > 0.0f) ? static_cast<float>((m_CurrentTime - boneAnim->Scales[idx].TimeStamp) / duration) : 0.0f;
                scaling = AnimationUtils::InterpolateScale(boneAnim->Scales[idx], boneAnim->Scales[nextIdx], t);
            }

            if (!boneAnim->Rotations.empty())
            {
                const size_t idx = AnimationUtils::FindKeyIndex(boneAnim->Rotations, m_CurrentTime);
                const size_t nextIdx = (idx + 1 < boneAnim->Rotations.size()) ? idx + 1 : idx;
                const float duration = static_cast<float>(boneAnim->Rotations[nextIdx].TimeStamp - boneAnim->Rotations[idx].TimeStamp);
                const float t = (duration > 0.0f) ? static_cast<float>((m_CurrentTime - boneAnim->Rotations[idx].TimeStamp) / duration) : 0.0f;
                rotation = AnimationUtils::InterpolateRotation(boneAnim->Rotations[idx], boneAnim->Rotations[nextIdx], t);
            }

            if (!boneAnim->Positions.empty())
            {
                const size_t idx = AnimationUtils::FindKeyIndex(boneAnim->Positions, m_CurrentTime);
                const size_t nextIdx = (idx + 1 < boneAnim->Positions.size()) ? idx + 1 : idx;
                const float duration = static_cast<float>(boneAnim->Positions[nextIdx].TimeStamp - boneAnim->Positions[idx].TimeStamp);
                const float t = (duration > 0.0f) ? static_cast<float>((m_CurrentTime - boneAnim->Positions[idx].TimeStamp) / duration) : 0.0f;
                translation = AnimationUtils::InterpolatePosition(boneAnim->Positions[idx], boneAnim->Positions[nextIdx], t);
            }

            nodeTransform =
                XMMatrixScalingFromVector(scaling) *
                XMMatrixRotationQuaternion(rotation) *
                XMMatrixTranslationFromVector(translation);
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
