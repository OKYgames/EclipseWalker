#pragma once

#include "AnimationStructures.h"
#include "AnimationUtils.h"
#include <DirectXMath.h>
#include <cstddef>
#include <map>
#include <string>
#include <vector>

class Animator
{
public:
    Animator();

    void Initialize(std::map<std::string, unsigned int>* boneMapping, std::vector<BoneInfo>* boneInfo);
    void PlayAnimation(AnimationClip* animation, float blendDuration = 0.0f);
    void UpdateAnimation(float dt);
    void SetPlaybackSpeed(float speed);
    float GetPlaybackSpeed() const;

    void PauseAnimation();
    void ResumeAnimation();
    void TogglePause();
    bool IsPaused() const;

    void EnableBindPoseMode(bool enable);
    void ToggleBindPoseMode();
    bool IsBindPoseMode() const;

    const std::vector<DirectX::XMFLOAT4X4>& GetFinalBoneMatrices() const;
    const std::vector<DirectX::XMFLOAT4X4>& GetGlobalBoneMatrices() const;
    const std::map<std::string, DirectX::XMFLOAT4X4>& GetGlobalNodeMap() const;

private:
    void CalculateNodeTransform(const NodeData* node, DirectX::XMMATRIX parentTransform);
    DirectX::XMMATRIX CalculateLocalTransform(const NodeData* node, const AnimationClip* animation, float animationTime) const;
    const BoneAnimation* FindBoneAnimation(const AnimationClip* animation, const std::string& nodeName) const;
    std::size_t GetRequiredBoneMatrixCount() const;
    void ResetBoneMatrices(std::size_t boneCount);
    bool IsBlending() const;

private:
    std::map<std::string, unsigned int>* m_BoneMapping = nullptr;
    std::vector<BoneInfo>* m_BoneInfo = nullptr;

    std::vector<DirectX::XMFLOAT4X4> m_FinalBoneMatrices;
    std::vector<DirectX::XMFLOAT4X4> m_GlobalBoneMatrices;
    std::map<std::string, DirectX::XMFLOAT4X4> m_GlobalNodeTransforms;

    AnimationClip* m_CurrentAnimation = nullptr;
    AnimationClip* m_PreviousAnimation = nullptr;
    float m_CurrentTime = 0.0f;
    float m_PreviousTime = 0.0f;
    float m_BlendTime = 0.0f;
    float m_BlendDuration = 0.0f;
    float m_PlaybackSpeed = 1.0f;

    bool m_IsPaused = false;
    bool m_UseBindPoseOnly = false;

    DirectX::XMFLOAT4X4 m_GlobalInverseTransform = {};
};
