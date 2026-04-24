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
    void PlayAnimation(AnimationClip* animation);
    void UpdateAnimation(float dt);

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
    BoneAnimation* FindBoneAnimation(const std::string& nodeName);
    std::size_t GetRequiredBoneMatrixCount() const;
    void ResetBoneMatrices(std::size_t boneCount);

private:
    std::map<std::string, unsigned int>* m_BoneMapping = nullptr;
    std::vector<BoneInfo>* m_BoneInfo = nullptr;

    std::vector<DirectX::XMFLOAT4X4> m_FinalBoneMatrices;
    std::vector<DirectX::XMFLOAT4X4> m_GlobalBoneMatrices;
    std::map<std::string, DirectX::XMFLOAT4X4> m_GlobalNodeTransforms;

    AnimationClip* m_CurrentAnimation = nullptr;
    float m_CurrentTime = 0.0f;

    bool m_IsPaused = false;
    bool m_UseBindPoseOnly = false;

    DirectX::XMFLOAT4X4 m_GlobalInverseTransform = {};
};
