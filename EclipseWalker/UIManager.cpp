#include "UIManager.h"
#include "EclipseWalkerGame.h"
#include "Vertices.h"
#include <ResourceUploadBatch.h>
#include <RenderTargetState.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <exception>
#include <string>
#include <Windows.h>

namespace
{
    constexpr float kHudCenterX = -0.715f;
    constexpr float kHudCenterY = 0.77f;
    constexpr float kHudFrameScaleX = 0.285f;
    constexpr float kHudFrameScaleY = 0.185f;
    constexpr float kHudBarLeftOffsetX = -0.11f;
    constexpr float kHudBarMaxScaleX = 0.170f;
    constexpr float kHudHpY = 0.818f;
    constexpr float kHudMpY = 0.731f;
    constexpr float kHudExpY = 0.676f;
    constexpr float kHudHpScaleY = 0.034f;
    constexpr float kHudMpScaleY = 0.034f;
    constexpr float kHudExpScaleY = 0.010f;
    constexpr float kHudClassEmblemScaleY = 0.104f;
    constexpr bool kDebugAutoDrainHudBars = false;
    constexpr float kDebugHudDrainCycleSeconds = 4.0f;
    constexpr float kLanternFrameRadius = 0.170f;
    constexpr float kLanternRingRadius = 0.132f;
    constexpr float kLanternRingOffsetY = -0.004f;
    constexpr float kLanternCoreRadius = 0.078f;
    constexpr float kSkillBarAspect = 2.0f;
    constexpr float kSkillBarScaleY = 0.135f;
    constexpr float kSkillBarMarginX = 0.025f;
    constexpr float kSkillBarMarginY = 0.035f;
    constexpr float kSkillIconScaleY = 0.071f;
    constexpr float kSkillIconOffsetXFactor = 0.38f;
    constexpr float kSkillIconOffsetY = 0.0f;
    constexpr float kDashCooldownRadius = 0.061f;
    constexpr float kDashCooldownFillRadius = 0.050f;
    constexpr float kDashCooldownIconScaleY = 0.058f;
    constexpr float kDashCooldownFrameScaleY = 0.071f;
    constexpr float kDashCooldownTextScale = 0.56f;
    constexpr float kBossBarCenterX = 0.0f;
    constexpr float kBossBarY = 0.84f;
    constexpr float kBossBarFrameAspect = 11.40f;
    constexpr float kBossBarFrameScaleY = 0.085f;
    constexpr float kBossBarFillMaxScaleX = 0.360f;
    constexpr float kBossBarFillScaleY = 0.018f;
    constexpr float kBossBarGlossScaleY = 0.005f;
    constexpr float kDeathOverlayScale = 1.05f;
    constexpr float kRespawnButtonCenterX = 0.0f;
    constexpr float kRespawnButtonCenterY = -0.04f;
    constexpr float kRespawnButtonScaleX = 0.200f;
    constexpr float kRespawnButtonScaleY = 0.066f;
    constexpr float kRespawnButtonFrameScaleX = 0.212f;
    constexpr float kRespawnButtonFrameScaleY = 0.078f;
    constexpr float kRespawnButtonTextScale = 0.72f;

    DirectX::XMFLOAT4 GetLevelUpFlashColor(PlayerClass playerClass, int newLevel)
    {
        const float intensity = newLevel >= 3 ? 1.15f : 1.0f;
        switch (playerClass)
        {
        case PlayerClass::Warrior:
            return { 1.0f, 0.76f * intensity, 0.38f * intensity, 0.0f };
        case PlayerClass::Mage:
            return { 0.62f * intensity, 0.88f * intensity, 1.0f, 0.0f };
        case PlayerClass::Archer:
            return { 0.58f * intensity, 1.0f, 0.62f * intensity, 0.0f };
        case PlayerClass::None:
        default:
            return { 1.0f, 1.0f, 1.0f, 0.0f };
        }
    }

    void SetTexScale(RenderItem* ritem, float scaleU, float scaleV = 1.0f)
    {
        if (ritem == nullptr)
        {
            return;
        }

        DirectX::XMStoreFloat4x4(
            &ritem->TexTransform,
            DirectX::XMMatrixScaling(scaleU, scaleV, 1.0f));
        ritem->NumFramesDirty = gNumFrameResources;
    }
}

UIManager::UIManager(EclipseWalkerGame* game) : mGame(game)
{
}

UIManager::~UIManager()
{
}

void UIManager::BuildInGameUI()
{
    auto& ritems = mGame->GetRitems();
    auto res = mGame->GetResources();
    auto* device = mGame->GetDevice();
    auto* cmdList = mGame->GetCommandList();
    auto* cmdQueue = mGame->GetCommandQueue();

    auto createUIMaterial = [&](const std::string& name, const DirectX::XMFLOAT4& color)
        {
            res->CreateMaterial(name, static_cast<int>(res->mMaterials.size()), "white", "", "", "",
                color, DirectX::XMFLOAT3(0.01f, 0.01f, 0.01f), 0.5f);
            if (auto mat = res->GetMaterial(name))
            {
                mat->IsTransparent = 1;
                mat->NumFramesDirty = 3;
            }
        };

    auto createUITextureMaterial = [&](const std::string& name, const std::string& textureName, const DirectX::XMFLOAT4& color)
        {
            res->CreateMaterial(name, static_cast<int>(res->mMaterials.size()), textureName, "", "", "",
                color, DirectX::XMFLOAT3(0.01f, 0.01f, 0.01f), 0.5f);
            if (auto mat = res->GetMaterial(name))
            {
                mat->DiffuseMapName = textureName;
                mat->DiffuseAlbedo = color;
                mat->IsTransparent = 1;
                mat->NumFramesDirty = gNumFrameResources;
            }
        };

    createUIMaterial("UI_HudShadowMat", DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.52f));
    createUIMaterial("UI_HudPanelMat", DirectX::XMFLOAT4(0.025f, 0.026f, 0.032f, 0.96f));
    createUIMaterial("UI_HudFrameMat", DirectX::XMFLOAT4(0.62f, 0.66f, 0.70f, 1.0f));
    createUIMaterial("UI_HudInnerFrameMat", DirectX::XMFLOAT4(0.12f, 0.10f, 0.08f, 1.0f));
    createUITextureMaterial("UI_HPMPFrameMat", "UI_HPMP_Frame", DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
    createUITextureMaterial("UI_HPFillTexMat", "UI_HP_Fill", DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
    createUITextureMaterial("UI_HPDelayTexMat", "UI_HP_Fill", DirectX::XMFLOAT4(1.0f, 0.78f, 0.42f, 0.88f));
    createUITextureMaterial("UI_MPFillTexMat", "UI_MP_Fill", DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
    createUITextureMaterial("UI_MPDelayTexMat", "UI_MP_Fill", DirectX::XMFLOAT4(0.58f, 1.0f, 1.0f, 0.82f));
    createUITextureMaterial("UI_HPMPGlossMat", "UI_HPMP_Gloss", DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 0.55f));
    createUITextureMaterial("UI_ClassEmblemWarriorTexMat", "UI_ClassEmblem_Warrior", DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
    createUITextureMaterial("UI_ClassEmblemMageTexMat", "UI_ClassEmblem_Mage", DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
    createUITextureMaterial("UI_ClassEmblemArcherTexMat", "UI_ClassEmblem_Archer", DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
    createUITextureMaterial("UI_LanternFrameTexMat", "UI_Lantern_Frame", DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
    createUITextureMaterial("UI_LanternRingFillTexMat", "UI_Lantern_Ring_Fill", DirectX::XMFLOAT4(0.72f, 1.0f, 0.78f, 1.0f));
    createUITextureMaterial("UI_LanternCoreGlowTexMat", "UI_Lantern_Core_Glow", DirectX::XMFLOAT4(0.85f, 1.0f, 0.86f, 0.92f));
    createUITextureMaterial("UI_SkillBarTwoSlotsTexMat", "UI_SkillBar_TwoSlots", DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
    createUITextureMaterial("UI_SkillWarriorEarthquakeSlamTexMat", "UI_Skill_Warrior_EarthquakeSlam", DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
    createUITextureMaterial("UI_SkillWarriorGreatswordSummonTexMat", "UI_Skill_Warrior_GreatswordSummon", DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
    createUITextureMaterial("UI_SkillMageHealingLightTexMat", "UI_Skill_Mage_HealingLight", DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
    createUITextureMaterial("UI_SkillMageMeteorTexMat", "UI_Skill_Mage_Meteor", DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
    createUITextureMaterial("UI_SkillArcherWindImbuementTexMat", "UI_Skill_Archer_WindImbuement", DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
    createUITextureMaterial("UI_SkillArcherArrowRainTexMat", "UI_Skill_Archer_ArrowRain", DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
    createUITextureMaterial("UI_BossHpFrameTexMat", "UI_BossHp_Frame", DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
    createUITextureMaterial("UI_MirrorCrackMat", "UI_MirrorCrackOverlay", DirectX::XMFLOAT4(0.82f, 0.96f, 1.0f, 0.0f));
    createUIMaterial("UI_DeathOverlayMat", DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f));
    createUIMaterial("UI_RespawnButtonFrameMat", DirectX::XMFLOAT4(0.58f, 0.60f, 0.66f, 0.0f));
    createUIMaterial("UI_RespawnButtonMat", DirectX::XMFLOAT4(0.10f, 0.11f, 0.14f, 0.0f));
    createUIMaterial("UI_HpBackMat", DirectX::XMFLOAT4(0.13f, 0.025f, 0.03f, 1.0f));
    createUIMaterial("UI_HpDelayMat", DirectX::XMFLOAT4(0.95f, 0.48f, 0.22f, 1.0f));
    createUIMaterial("UI_HpMat", DirectX::XMFLOAT4(0.86f, 0.04f, 0.06f, 1.0f));
    createUIMaterial("UI_HpGlossMat", DirectX::XMFLOAT4(1.0f, 0.48f, 0.42f, 0.42f));
    createUIMaterial("UI_ExpBackMat", DirectX::XMFLOAT4(0.018f, 0.055f, 0.022f, 0.96f));
    createUIMaterial("UI_ExpMat", DirectX::XMFLOAT4(0.12f, 0.92f, 0.24f, 1.0f));
    createUIMaterial("UI_BossHpFrameMat", DirectX::XMFLOAT4(0.015f, 0.012f, 0.013f, 0.92f));
    createUIMaterial("UI_BossHpBackMat", DirectX::XMFLOAT4(0.12f, 0.018f, 0.024f, 0.94f));
    createUIMaterial("UI_BossHpDelayMat", DirectX::XMFLOAT4(0.95f, 0.52f, 0.18f, 0.95f));
    createUIMaterial("UI_BossHpFillMat", DirectX::XMFLOAT4(0.78f, 0.025f, 0.04f, 1.0f));
    createUIMaterial("UI_BossHpGlossMat", DirectX::XMFLOAT4(1.0f, 0.36f, 0.32f, 0.36f));
    createUIMaterial("UI_BossHpCapMat", DirectX::XMFLOAT4(0.70f, 0.60f, 0.42f, 0.98f));
    createUIMaterial("UI_MpBackMat", DirectX::XMFLOAT4(0.025f, 0.045f, 0.13f, 1.0f));
    createUIMaterial("UI_MpDelayMat", DirectX::XMFLOAT4(0.30f, 0.88f, 1.0f, 1.0f));
    createUIMaterial("UI_MpMat", DirectX::XMFLOAT4(0.04f, 0.30f, 0.94f, 1.0f));
    createUIMaterial("UI_MpGlossMat", DirectX::XMFLOAT4(0.55f, 0.86f, 1.0f, 0.38f));
    createUIMaterial("UI_LanternRingBackMat", DirectX::XMFLOAT4(0.04f, 0.10f, 0.075f, 0.88f));
    createUIMaterial("UI_LanternRingMat", DirectX::XMFLOAT4(0.08f, 0.94f, 0.38f, 1.0f));
    createUIMaterial("UI_LanternOrbGlowMat", DirectX::XMFLOAT4(0.14f, 0.95f, 0.42f, 0.34f));
    createUIMaterial("UI_LanternOrbCoreMat", DirectX::XMFLOAT4(0.12f, 0.72f, 0.30f, 0.85f));
    createUIMaterial("UI_SkillCooldown1BackMat", DirectX::XMFLOAT4(0.08f, 0.09f, 0.12f, 0.84f));
    createUIMaterial("UI_SkillCooldown1FillMat", DirectX::XMFLOAT4(0.03f, 0.04f, 0.05f, 0.82f));
    createUIMaterial("UI_SkillCooldown2BackMat", DirectX::XMFLOAT4(0.08f, 0.09f, 0.12f, 0.84f));
    createUIMaterial("UI_SkillCooldown2FillMat", DirectX::XMFLOAT4(0.03f, 0.04f, 0.05f, 0.82f));
    createUIMaterial("UI_DashCooldownBackMat", DirectX::XMFLOAT4(0.08f, 0.09f, 0.12f, 0.84f));
    createUIMaterial("UI_DashCooldownFillMat", DirectX::XMFLOAT4(0.03f, 0.04f, 0.05f, 0.82f));
    if (res->GetTexture("UI_DashCooldown_Frame") != nullptr)
    {
        createUITextureMaterial("UI_DashCooldownFrameTexMat", "UI_DashCooldown_Frame", DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
    }
    else
    {
        createUIMaterial("UI_DashCooldownFrameTexMat", DirectX::XMFLOAT4(0.92f, 0.95f, 1.0f, 0.98f));
    }
    createUITextureMaterial("UI_DashIconWarriorTexMat", "UI_Skill_Warrior_Dash", DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
    createUITextureMaterial("UI_DashIconMageTexMat", "UI_Skill_Mage_Dash", DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
    createUITextureMaterial("UI_DashIconArcherTexMat", "UI_Skill_Archer_Dash", DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
    res->CreateMaterial("UI_LanternIconMat", static_cast<int>(res->mMaterials.size()), "LanternIcon", "", "", "",
        DirectX::XMFLOAT4(0.18f, 1.0f, 0.36f, 1.0f), DirectX::XMFLOAT3(0.01f, 0.01f, 0.01f), 0.5f);
    if (auto mat = res->GetMaterial("UI_LanternIconMat")) { mat->IsTransparent = 1; mat->NumFramesDirty = 3; }
    mLanternRingMat = res->GetMaterial("UI_LanternRingFillTexMat");
    mLanternGlowMat = res->GetMaterial("UI_LanternCoreGlowTexMat");
    mLanternIconMat = nullptr;
    res->CreateMaterial("UI_ChatLogMat", static_cast<int>(res->mMaterials.size()), "white", "", "", "",
        DirectX::XMFLOAT4(0.05f, 0.07f, 0.09f, 0.72f), DirectX::XMFLOAT3(0.01f, 0.01f, 0.01f), 0.5f);
    res->CreateMaterial("UI_ChatInputMat", static_cast<int>(res->mMaterials.size()), "white", "", "", "",
        DirectX::XMFLOAT4(0.14f, 0.16f, 0.2f, 0.88f), DirectX::XMFLOAT3(0.01f, 0.01f, 0.01f), 0.5f);

    if (auto mat = res->GetMaterial("UI_ChatLogMat")) { mat->IsTransparent = 1; mat->NumFramesDirty = 3; }
    if (auto mat = res->GetMaterial("UI_ChatInputMat")) { mat->IsTransparent = 1; mat->NumFramesDirty = 3; }

    mChatLogMat = res->GetMaterial("UI_ChatLogMat");
    mChatInputMat = res->GetMaterial("UI_ChatInputMat");
    mBossHpBackMat = res->GetMaterial("UI_BossHpBackMat");
    mBossHpDelayMat = res->GetMaterial("UI_BossHpDelayMat");
    mBossHpFillMat = res->GetMaterial("UI_BossHpFillMat");
    mBossHpGlossMat = res->GetMaterial("UI_BossHpGlossMat");
    mMirrorCrackMat = res->GetMaterial("UI_MirrorCrackMat");
    mDeathOverlayMat = res->GetMaterial("UI_DeathOverlayMat");
    mRespawnButtonFrameMat = res->GetMaterial("UI_RespawnButtonFrameMat");
    mRespawnButtonMat = res->GetMaterial("UI_RespawnButtonMat");
    mClassEmblemWarriorMat = res->GetMaterial("UI_ClassEmblemWarriorTexMat");
    mClassEmblemMageMat = res->GetMaterial("UI_ClassEmblemMageTexMat");
    mClassEmblemArcherMat = res->GetMaterial("UI_ClassEmblemArcherTexMat");
    mSkillIcon1WarriorMat = res->GetMaterial("UI_SkillWarriorEarthquakeSlamTexMat");
    mSkillIcon2WarriorMat = res->GetMaterial("UI_SkillWarriorGreatswordSummonTexMat");
    mSkillIcon1MageMat = res->GetMaterial("UI_SkillMageHealingLightTexMat");
    mSkillIcon2MageMat = res->GetMaterial("UI_SkillMageMeteorTexMat");
    mSkillIcon1ArcherMat = res->GetMaterial("UI_SkillArcherWindImbuementTexMat");
    mSkillIcon2ArcherMat = res->GetMaterial("UI_SkillArcherArrowRainTexMat");

    auto createUIMeshGeometry = [&](const std::string& name, const std::vector<Vertex>& vertices, const std::vector<std::uint16_t>& indices, const std::string& submeshName)
        {
            if (res->mGeometries.find(name) != res->mGeometries.end())
            {
                return;
            }

            const UINT vbByteSize = static_cast<UINT>(vertices.size() * sizeof(Vertex));
            const UINT ibByteSize = static_cast<UINT>(indices.size() * sizeof(std::uint16_t));

            auto geometry = std::make_unique<MeshGeometry>();
            geometry->Name = name;

            ThrowIfFailed(D3DCreateBlob(vbByteSize, &geometry->VertexBufferCPU));
            CopyMemory(geometry->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);
            ThrowIfFailed(D3DCreateBlob(ibByteSize, &geometry->IndexBufferCPU));
            CopyMemory(geometry->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

            geometry->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(device, cmdList, vertices.data(), vbByteSize, geometry->VertexBufferUploader);
            geometry->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(device, cmdList, indices.data(), ibByteSize, geometry->IndexBufferUploader);
            geometry->VertexByteStride = sizeof(Vertex);
            geometry->VertexBufferByteSize = vbByteSize;
            geometry->IndexFormat = DXGI_FORMAT_R16_UINT;
            geometry->IndexBufferByteSize = ibByteSize;

            SubmeshGeometry submesh;
            submesh.IndexCount = static_cast<UINT>(indices.size());
            submesh.StartIndexLocation = 0;
            submesh.BaseVertexLocation = 0;
            geometry->DrawArgs[submeshName] = submesh;

            res->mGeometries[name] = std::move(geometry);
        };

    auto buildRingGeometry = [&]()
        {
            constexpr int segmentCount = 96;
            constexpr float innerRadius = 0.84f;
            constexpr float outerRadius = 1.0f;
            std::vector<Vertex> vertices;
            std::vector<std::uint16_t> indices;
            vertices.reserve((segmentCount + 1) * 2);
            indices.reserve(segmentCount * 6);

            for (int i = 0; i <= segmentCount; ++i)
            {
                const float angle = -DirectX::XM_PIDIV2 + DirectX::XM_2PI * static_cast<float>(i) / static_cast<float>(segmentCount);
                const float c = std::cos(angle);
                const float s = std::sin(angle);
                vertices.push_back(Vertex({ DirectX::XMFLOAT3(c * outerRadius, s * outerRadius, 0.0f), DirectX::XMFLOAT3(0.0f, 0.0f, -1.0f), DirectX::XMFLOAT2(0.5f + c * 0.5f, 0.5f - s * 0.5f), DirectX::XMFLOAT3(1.0f, 0.0f, 0.0f) }));
                vertices.push_back(Vertex({ DirectX::XMFLOAT3(c * innerRadius, s * innerRadius, 0.0f), DirectX::XMFLOAT3(0.0f, 0.0f, -1.0f), DirectX::XMFLOAT2(0.5f + c * innerRadius * 0.5f, 0.5f - s * innerRadius * 0.5f), DirectX::XMFLOAT3(1.0f, 0.0f, 0.0f) }));
            }

            for (int i = 0; i < segmentCount; ++i)
            {
                const std::uint16_t outer0 = static_cast<std::uint16_t>(i * 2);
                const std::uint16_t inner0 = static_cast<std::uint16_t>(i * 2 + 1);
                const std::uint16_t outer1 = static_cast<std::uint16_t>((i + 1) * 2);
                const std::uint16_t inner1 = static_cast<std::uint16_t>((i + 1) * 2 + 1);
                indices.insert(indices.end(), { outer0, outer1, inner0, inner0, outer1, inner1 });
            }

            createUIMeshGeometry("uiLanternRingGeo", vertices, indices, "ring");
        };

    auto buildDiskGeometry = [&]()
        {
            constexpr int segmentCount = 96;
            std::vector<Vertex> vertices;
            std::vector<std::uint16_t> indices;
            vertices.reserve(segmentCount + 2);
            indices.reserve(segmentCount * 3);

            vertices.push_back(Vertex({ DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f), DirectX::XMFLOAT3(0.0f, 0.0f, -1.0f), DirectX::XMFLOAT2(0.5f, 0.5f), DirectX::XMFLOAT3(1.0f, 0.0f, 0.0f) }));
            for (int i = 0; i <= segmentCount; ++i)
            {
                const float angle = DirectX::XM_PIDIV2 - DirectX::XM_2PI * static_cast<float>(i) / static_cast<float>(segmentCount);
                const float c = std::cos(angle);
                const float s = std::sin(angle);
                vertices.push_back(Vertex({ DirectX::XMFLOAT3(c, s, 0.0f), DirectX::XMFLOAT3(0.0f, 0.0f, -1.0f), DirectX::XMFLOAT2(0.5f + c * 0.5f, 0.5f - s * 0.5f), DirectX::XMFLOAT3(1.0f, 0.0f, 0.0f) }));
            }

            for (int i = 1; i <= segmentCount; ++i)
            {
                indices.insert(indices.end(), { 0, static_cast<std::uint16_t>(i), static_cast<std::uint16_t>(i + 1) });
            }

            createUIMeshGeometry("uiLanternDiskGeo", vertices, indices, "disk");
        };

    buildRingGeometry();
    buildDiskGeometry();

    auto setupRitem = [&](RenderItem* ritem) {
        ritem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        ritem->IndexCount = ritem->Geo->DrawArgs["quad"].IndexCount;
        ritem->StartIndexLocation = ritem->Geo->DrawArgs["quad"].StartIndexLocation;
        ritem->BaseVertexLocation = ritem->Geo->DrawArgs["quad"].BaseVertexLocation;
        };

    auto createUIQuad = [&](const std::string& materialName, float scaleX, float scaleY, float x, float y, float z, float rotationZ = 0.0f)
        {
            auto ritem = std::make_unique<RenderItem>();
            ritem->Geo = res->mGeometries["quadGeo"].get();
            ritem->Mat = res->GetMaterial(materialName);
            ritem->ObjCBIndex = static_cast<UINT>(ritems.size());
            setupRitem(ritem.get());

            auto object = std::make_unique<GameObject>();
            object->SetScale(scaleX, scaleY, 1.0f);
            object->SetPosition(x, y, z);
            object->SetRotation(0.0f, 0.0f, rotationZ);
            object->Ritem = ritem.get();
            object->Update();

            GameObject* rawObject = object.get();
            ritems.push_back(std::move(ritem));
            mUIObjects.push_back(std::move(object));
            return rawObject;
        };

    auto createUIMeshObject = [&](const std::string& materialName, const std::string& geometryName, const std::string& submeshName, float scaleX, float scaleY, float x, float y, float z, RenderItem** outRenderItem = nullptr)
        {
            auto ritem = std::make_unique<RenderItem>();
            ritem->Geo = res->mGeometries[geometryName].get();
            ritem->Mat = res->GetMaterial(materialName);
            ritem->ObjCBIndex = static_cast<UINT>(ritems.size());
            ritem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
            ritem->IndexCount = ritem->Geo->DrawArgs[submeshName].IndexCount;
            ritem->StartIndexLocation = ritem->Geo->DrawArgs[submeshName].StartIndexLocation;
            ritem->BaseVertexLocation = ritem->Geo->DrawArgs[submeshName].BaseVertexLocation;

            auto object = std::make_unique<GameObject>();
            object->SetScale(scaleX, scaleY, 1.0f);
            object->SetPosition(x, y, z);
            object->Ritem = ritem.get();
            object->Update();

            GameObject* rawObject = object.get();
            if (outRenderItem != nullptr)
            {
                *outRenderItem = ritem.get();
            }
            ritems.push_back(std::move(ritem));
            mUIObjects.push_back(std::move(object));
            return rawObject;
        };

    const float lanternCenterX = 0.88f;
    const float lanternCenterY = 0.0f;
    const auto viewport = mGame->GetScreenViewport();
    const float lanternAspectFix = viewport.Width > 0.0f ? (viewport.Height / viewport.Width) : (9.0f / 16.0f);
    const float classEmblemScaleX = kHudClassEmblemScaleY * lanternAspectFix;
    const float classEmblemCenterX = kHudCenterX - kHudFrameScaleX + 0.094f;
    const float classEmblemCenterY = kHudCenterY + 0.004f;
    const float skillBarScaleX = kSkillBarScaleY * kSkillBarAspect * lanternAspectFix;
    const float skillBarCenterX = 1.0f - kSkillBarMarginX - skillBarScaleX;
    const float skillBarCenterY = -1.0f + kSkillBarMarginY + kSkillBarScaleY;

    mHpMpFrame = createUIQuad("UI_HPMPFrameMat", kHudFrameScaleX, kHudFrameScaleY, kHudCenterX, kHudCenterY, 0.142f);
    mHpBarDelay = createUIQuad("UI_HPDelayTexMat", kHudBarMaxScaleX, kHudHpScaleY, kHudCenterX + kHudBarLeftOffsetX + kHudBarMaxScaleX, kHudHpY, 0.136f);
    mHpBarFill = createUIQuad("UI_HPFillTexMat", kHudBarMaxScaleX, kHudHpScaleY, kHudCenterX + kHudBarLeftOffsetX + kHudBarMaxScaleX, kHudHpY, 0.132f);
    mMpBarDelay = createUIQuad("UI_MPDelayTexMat", kHudBarMaxScaleX, kHudMpScaleY, kHudCenterX + kHudBarLeftOffsetX + kHudBarMaxScaleX, kHudMpY, 0.128f);
    mMpBarFill = createUIQuad("UI_MPFillTexMat", kHudBarMaxScaleX, kHudMpScaleY, kHudCenterX + kHudBarLeftOffsetX + kHudBarMaxScaleX, kHudMpY, 0.124f);
    mExpBarBack = createUIQuad("UI_ExpBackMat", kHudBarMaxScaleX, kHudExpScaleY, kHudCenterX + kHudBarLeftOffsetX + kHudBarMaxScaleX, kHudExpY, 0.122f);
    mExpBarFill = createUIQuad("UI_ExpMat", kHudBarMaxScaleX, kHudExpScaleY, kHudCenterX + kHudBarLeftOffsetX + kHudBarMaxScaleX, kHudExpY, 0.121f);
    mHpMpGloss = createUIQuad("UI_HPMPGlossMat", kHudFrameScaleX, kHudFrameScaleY, kHudCenterX, kHudCenterY, 0.120f);
    GameObject* classEmblem = createUIQuad("UI_ClassEmblemMageTexMat", classEmblemScaleX, kHudClassEmblemScaleY, classEmblemCenterX, classEmblemCenterY, 0.144f);
    mClassEmblemRitem = classEmblem != nullptr ? classEmblem->Ritem : nullptr;

    const float bossBarFrameScaleX = kBossBarFrameScaleY * kBossBarFrameAspect * lanternAspectFix;
    mBossHpBack = createUIQuad("UI_BossHpBackMat", kBossBarFillMaxScaleX, kBossBarFillScaleY, kBossBarCenterX, kBossBarY, 0.100f);
    mBossHpDelay = createUIQuad("UI_BossHpDelayMat", kBossBarFillMaxScaleX, kBossBarFillScaleY, kBossBarCenterX, kBossBarY, 0.095f);
    mBossHpFill = createUIQuad("UI_BossHpFillMat", kBossBarFillMaxScaleX, kBossBarFillScaleY, kBossBarCenterX, kBossBarY, 0.090f);
    mBossHpGloss = createUIQuad("UI_BossHpGlossMat", kBossBarFillMaxScaleX, kBossBarGlossScaleY, kBossBarCenterX, kBossBarY + 0.006f, 0.085f);
    mBossHpFrame = createUIQuad("UI_BossHpFrameTexMat", bossBarFrameScaleX, kBossBarFrameScaleY, kBossBarCenterX, kBossBarY, 0.080f);
    HideBossHealthBar();

    createUIQuad("UI_LanternFrameTexMat", kLanternFrameRadius * lanternAspectFix, kLanternFrameRadius, lanternCenterX, lanternCenterY, 0.103f);
    createUIMeshObject("UI_LanternRingFillTexMat", "uiLanternRingGeo", "ring", kLanternRingRadius * lanternAspectFix, kLanternRingRadius, lanternCenterX, lanternCenterY + kLanternRingOffsetY, 0.098f, &mLanternRingFillRitem);
    if (mLanternRingFillRitem)
    {
        mLanternRingFillRitem->IndexCount = 0;
        mLanternRingFillRitem->NumFramesDirty = gNumFrameResources;
    }
    mLanternOrbGlow = createUIQuad("UI_LanternCoreGlowTexMat", kLanternCoreRadius * lanternAspectFix, kLanternCoreRadius, lanternCenterX, lanternCenterY, 0.092f);
    createUIQuad("UI_SkillBarTwoSlotsTexMat", skillBarScaleX, kSkillBarScaleY, skillBarCenterX, skillBarCenterY, 0.088f);
    const float skillIconScaleX = kSkillIconScaleY * lanternAspectFix;
    const float skillIconOffsetX = skillBarScaleX * kSkillIconOffsetXFactor;
    GameObject* skillIcon1 = createUIQuad("UI_SkillMageHealingLightTexMat", skillIconScaleX, kSkillIconScaleY,
        skillBarCenterX - skillIconOffsetX, skillBarCenterY + kSkillIconOffsetY, 0.086f);
    GameObject* skillIcon2 = createUIQuad("UI_SkillMageMeteorTexMat", skillIconScaleX, kSkillIconScaleY,
        skillBarCenterX + skillIconOffsetX, skillBarCenterY + kSkillIconOffsetY, 0.084f);
    mSkillIcon1Ritem = skillIcon1 != nullptr ? skillIcon1->Ritem : nullptr;
    mSkillIcon2Ritem = skillIcon2 != nullptr ? skillIcon2->Ritem : nullptr;
    UpdateSkillIconMaterials();

    const float skillCooldownRadiusX = kDashCooldownFillRadius * lanternAspectFix;
    const float skillCooldownRadiusY = kDashCooldownFillRadius;
    const float skill1CenterX = skillBarCenterX - skillIconOffsetX;
    const float skill2CenterX = skillBarCenterX + skillIconOffsetX;
    const float skillCenterY = skillBarCenterY + kSkillIconOffsetY;

    mSkill1CooldownWidget.CenterX = skill1CenterX;
    mSkill1CooldownWidget.CenterY = skillCenterY;
    mSkill1CooldownWidget.BackMat = res->GetMaterial("UI_SkillCooldown1BackMat");
    mSkill1CooldownWidget.FillMat = res->GetMaterial("UI_SkillCooldown1FillMat");
    mSkill1CooldownWidget.IconWarriorMat = mSkillIcon1WarriorMat;
    mSkill1CooldownWidget.IconMageMat = mSkillIcon1MageMat;
    mSkill1CooldownWidget.IconArcherMat = mSkillIcon1ArcherMat;
    mSkill1CooldownWidget.IconRitem = mSkillIcon1Ritem;
    mSkill1CooldownWidget.Back = createUIMeshObject(
        "UI_SkillCooldown1BackMat",
        "uiLanternDiskGeo",
        "disk",
        skillCooldownRadiusX,
        skillCooldownRadiusY,
        skill1CenterX,
        skillCenterY,
        0.089f);
    mSkill1CooldownWidget.Fill = createUIMeshObject(
        "UI_SkillCooldown1FillMat",
        "uiLanternDiskGeo",
        "disk",
        skillCooldownRadiusX,
        skillCooldownRadiusY,
        skill1CenterX,
        skillCenterY,
        0.090f,
        &mSkill1CooldownWidget.FillRitem);
    if (mSkill1CooldownWidget.FillRitem != nullptr)
    {
        mSkill1CooldownWidget.FillRitem->IndexCount = 0;
        mSkill1CooldownWidget.FillRitem->Visible = false;
        mSkill1CooldownWidget.FillRitem->NumFramesDirty = gNumFrameResources;
    }

    mSkill2CooldownWidget.CenterX = skill2CenterX;
    mSkill2CooldownWidget.CenterY = skillCenterY;
    mSkill2CooldownWidget.BackMat = res->GetMaterial("UI_SkillCooldown2BackMat");
    mSkill2CooldownWidget.FillMat = res->GetMaterial("UI_SkillCooldown2FillMat");
    mSkill2CooldownWidget.IconWarriorMat = mSkillIcon2WarriorMat;
    mSkill2CooldownWidget.IconMageMat = mSkillIcon2MageMat;
    mSkill2CooldownWidget.IconArcherMat = mSkillIcon2ArcherMat;
    mSkill2CooldownWidget.IconRitem = mSkillIcon2Ritem;
    mSkill2CooldownWidget.Back = createUIMeshObject(
        "UI_SkillCooldown2BackMat",
        "uiLanternDiskGeo",
        "disk",
        skillCooldownRadiusX,
        skillCooldownRadiusY,
        skill2CenterX,
        skillCenterY,
        0.089f);
    mSkill2CooldownWidget.Fill = createUIMeshObject(
        "UI_SkillCooldown2FillMat",
        "uiLanternDiskGeo",
        "disk",
        skillCooldownRadiusX,
        skillCooldownRadiusY,
        skill2CenterX,
        skillCenterY,
        0.090f,
        &mSkill2CooldownWidget.FillRitem);
    if (mSkill2CooldownWidget.FillRitem != nullptr)
    {
        mSkill2CooldownWidget.FillRitem->IndexCount = 0;
        mSkill2CooldownWidget.FillRitem->Visible = false;
        mSkill2CooldownWidget.FillRitem->NumFramesDirty = gNumFrameResources;
    }

    const float dashFrameGap = 0.014f;
    const float dashCenterX = skillBarCenterX - skillBarScaleX - (kDashCooldownFrameScaleY * lanternAspectFix) - dashFrameGap;
    const float dashCenterY = skillBarCenterY - 0.002f;
    const float dashIconScaleX = kDashCooldownIconScaleY * lanternAspectFix;
    mDashCooldownWidget.CenterX = dashCenterX;
    mDashCooldownWidget.CenterY = dashCenterY;
    mDashCooldownWidget.BackMat = res->GetMaterial("UI_DashCooldownBackMat");
    mDashCooldownWidget.FillMat = res->GetMaterial("UI_DashCooldownFillMat");
    mDashCooldownWidget.IconWarriorMat = res->GetMaterial("UI_DashIconWarriorTexMat");
    mDashCooldownWidget.IconMageMat = res->GetMaterial("UI_DashIconMageTexMat");
    mDashCooldownWidget.IconArcherMat = res->GetMaterial("UI_DashIconArcherTexMat");
    mDashCooldownWidget.Back = createUIMeshObject(
        "UI_DashCooldownBackMat",
        "uiLanternDiskGeo",
        "disk",
        kDashCooldownFillRadius * lanternAspectFix,
        kDashCooldownFillRadius,
        dashCenterX,
        dashCenterY,
        0.094f);
    mDashCooldownWidget.Icon = createUIQuad(
        "UI_DashIconWarriorTexMat",
        dashIconScaleX,
        kDashCooldownIconScaleY,
        dashCenterX,
        dashCenterY,
        0.097f);
    if (mDashCooldownWidget.Icon != nullptr)
    {
        mDashCooldownWidget.IconRitem = mDashCooldownWidget.Icon->Ritem;
    }
    mDashCooldownWidget.Fill = createUIMeshObject(
        "UI_DashCooldownFillMat",
        "uiLanternDiskGeo",
        "disk",
        kDashCooldownFillRadius * lanternAspectFix,
        kDashCooldownFillRadius,
        dashCenterX,
        dashCenterY,
        0.100f,
        &mDashCooldownWidget.FillRitem);
    mDashCooldownWidget.Frame = createUIMeshObject(
        "UI_DashCooldownFrameTexMat",
        "quadGeo",
        "quad",
        kDashCooldownFrameScaleY * lanternAspectFix,
        kDashCooldownFrameScaleY,
        dashCenterX,
        dashCenterY,
        0.102f);
    if (mDashCooldownWidget.FillRitem != nullptr)
    {
        mDashCooldownWidget.FillRitem->IndexCount = 0;
        mDashCooldownWidget.FillRitem->Visible = false;
        mDashCooldownWidget.FillRitem->NumFramesDirty = gNumFrameResources;
    }
    UpdateCooldownWidget(mSkill1CooldownWidget);
    UpdateCooldownWidget(mSkill2CooldownWidget);
    UpdateCooldownWidget(mDashCooldownWidget);

    auto chatLogRitem = std::make_unique<RenderItem>();
    chatLogRitem->Geo = res->mGeometries["quadGeo"].get();
    chatLogRitem->Mat = res->GetMaterial("UI_ChatLogMat");
    chatLogRitem->ObjCBIndex = static_cast<UINT>(ritems.size());
    setupRitem(chatLogRitem.get());

    auto chatLogObj = std::make_unique<GameObject>();
    chatLogObj->SetScale(0.25f, 0.135f, 1.0f);
    chatLogObj->SetPosition(-0.735f, -0.74f, 0.11f);
    chatLogObj->Ritem = chatLogRitem.get();
    chatLogObj->Update();
    mChatLogBg = chatLogObj.get();
    ritems.push_back(std::move(chatLogRitem));
    mUIObjects.push_back(std::move(chatLogObj));

    auto chatInputRitem = std::make_unique<RenderItem>();
    chatInputRitem->Geo = res->mGeometries["quadGeo"].get();
    chatInputRitem->Mat = res->GetMaterial("UI_ChatInputMat");
    chatInputRitem->ObjCBIndex = static_cast<UINT>(ritems.size());
    setupRitem(chatInputRitem.get());

    auto chatInputObj = std::make_unique<GameObject>();
    chatInputObj->SetScale(0.25f, 0.038f, 1.0f);
    chatInputObj->SetPosition(-0.735f, -0.915f, 0.11f);
    chatInputObj->Ritem = chatInputRitem.get();
    chatInputObj->Update();
    mChatInputBg = chatInputObj.get();
    ritems.push_back(std::move(chatInputRitem));
    mUIObjects.push_back(std::move(chatInputObj));

    // 이펙트용 재질 2개 생성
    res->CreateMaterial("UI_FlashMat", static_cast<int>(res->mMaterials.size()), "white", "", "", "",
        DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 0.0f), DirectX::XMFLOAT3(0.01f, 0.01f, 0.01f), 0.5f);
    if (auto mat = res->GetMaterial("UI_FlashMat")) { mat->IsTransparent = 1; mat->NumFramesDirty = 3; }

    res->CreateMaterial("UI_ScreenBgMat", static_cast<int>(res->mMaterials.size()), "white", "", "", "",
        DirectX::XMFLOAT4(0.95f, 0.9f, 0.72f, 0.0f), DirectX::XMFLOAT3(0.01f, 0.01f, 0.01f), 0.5f);
    if (auto mat = res->GetMaterial("UI_ScreenBgMat")) { mat->IsTransparent = 1; mat->NumFramesDirty = 3; }

    auto bgRitem = std::make_unique<RenderItem>();
    bgRitem->Geo = res->mGeometries["quadGeo"].get();
    bgRitem->Mat = res->GetMaterial("UI_ScreenBgMat");
    bgRitem->ObjCBIndex = static_cast<UINT>(ritems.size());
    bgRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    bgRitem->IndexCount = bgRitem->Geo->DrawArgs["quad"].IndexCount;
    bgRitem->StartIndexLocation = bgRitem->Geo->DrawArgs["quad"].StartIndexLocation;
    bgRitem->BaseVertexLocation = bgRitem->Geo->DrawArgs["quad"].BaseVertexLocation;

    auto bgObj = std::make_unique<GameObject>();
    bgObj->Ritem = bgRitem.get();
    bgObj->SetScale(0.0f, 0.0f, 1.0f);
    bgObj->SetPosition(0.0f, 0.0f, 0.18f);
    bgObj->Update();
    mScreenBgObj = bgObj.get();

    ritems.push_back(std::move(bgRitem));
    mUIObjects.push_back(std::move(bgObj));

    auto flashRitem = std::make_unique<RenderItem>();
    flashRitem->Geo = res->mGeometries["quadGeo"].get();
    flashRitem->Mat = res->GetMaterial("UI_FlashMat");
    flashRitem->ObjCBIndex = static_cast<UINT>(ritems.size());

    flashRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    flashRitem->IndexCount = flashRitem->Geo->DrawArgs["quad"].IndexCount;
    flashRitem->StartIndexLocation = flashRitem->Geo->DrawArgs["quad"].StartIndexLocation;
    flashRitem->BaseVertexLocation = flashRitem->Geo->DrawArgs["quad"].BaseVertexLocation;

    auto flashObj = std::make_unique<GameObject>();
    flashObj->Ritem = flashRitem.get();
    flashObj->SetScale(0.0f, 0.0f, 0.0f); 
    flashObj->Update();
    mFlashObj = flashObj.get();

    ritems.push_back(std::move(flashRitem));
    mUIObjects.push_back(std::move(flashObj));
    InitializeEffect(res->GetMaterial("UI_FlashMat"), res->GetMaterial("UI_ScreenBgMat"), mFlashObj, mScreenBgObj);

    mMirrorCrackObj = createUIQuad("UI_MirrorCrackMat", 0.0f, 0.0f, 0.0f, 0.0f, 0.060f);
    if (mMirrorCrackObj != nullptr && mMirrorCrackObj->Ritem != nullptr)
    {
        mMirrorCrackObj->Ritem->Visible = false;
        mMirrorCrackObj->Ritem->NumFramesDirty = gNumFrameResources;
    }

    mDeathOverlayObj = createUIQuad(
        "UI_DeathOverlayMat",
        kDeathOverlayScale,
        kDeathOverlayScale,
        0.0f,
        0.0f,
        0.164f);
    mRespawnButtonFrameObj = createUIQuad(
        "UI_RespawnButtonFrameMat",
        kRespawnButtonFrameScaleX,
        kRespawnButtonFrameScaleY,
        kRespawnButtonCenterX,
        kRespawnButtonCenterY,
        0.166f);
    mRespawnButtonObj = createUIQuad(
        "UI_RespawnButtonMat",
        kRespawnButtonScaleX,
        kRespawnButtonScaleY,
        kRespawnButtonCenterX,
        kRespawnButtonCenterY,
        0.168f);

    auto hideDeathUiObject = [](GameObject* object)
        {
            if (object != nullptr && object->Ritem != nullptr)
            {
                object->Ritem->Visible = false;
                object->Ritem->NumFramesDirty = gNumFrameResources;
                object->Update();
            }
        };
    hideDeathUiObject(mDeathOverlayObj);
    hideDeathUiObject(mRespawnButtonFrameObj);
    hideDeathUiObject(mRespawnButtonObj);

    if (device != nullptr && cmdQueue != nullptr)
    {
        try
        {
            if (!mCooldownTextHeap)
            {
                mCooldownTextHeap = std::make_unique<DirectX::DescriptorHeap>(
                    device,
                    D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
                    D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
                    1);
            }

            if (!mCooldownTextFont || !mCooldownTextBatch)
            {
                DirectX::ResourceUploadBatch resourceUpload(device);
                resourceUpload.Begin();

                if (!mCooldownTextFont)
                {
                    mCooldownTextFont = std::make_unique<DirectX::SpriteFont>(
                        device,
                        resourceUpload,
                        L"Textures/chat_korean.spritefont",
                        mCooldownTextHeap->GetCpuHandle(0),
                        mCooldownTextHeap->GetGpuHandle(0));
                }

                if (!mCooldownTextBatch)
                {
                    DirectX::RenderTargetState rtState(DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_D24_UNORM_S8_UINT);
                    DirectX::SpriteBatchPipelineStateDescription pd(rtState);
                    mCooldownTextBatch = std::make_unique<DirectX::SpriteBatch>(device, resourceUpload, pd);
                }

                auto uploadResourcesFinished = resourceUpload.End(cmdQueue);
                uploadResourcesFinished.wait();
            }
        }
        catch (const std::exception& e)
        {
            std::string log = "[UIManager] Failed to initialize cooldown font: ";
            log += e.what();
            log += "\n";
            OutputDebugStringA(log.c_str());
            mCooldownTextFont.reset();
            mCooldownTextBatch.reset();
            mCooldownTextHeap.reset();
        }
    }
}

void UIManager::Update(float currentHp, float maxHp, float currentMp, float maxMp, float currentLantern, float maxLantern, float currentDashCooldown, float maxDashCooldown, float currentExpRatio, bool showDeathScreen)
{
    float hpRatio = maxHp > 0.0f ? (currentHp / maxHp) : 0.0f;
    float mpRatio = maxMp > 0.0f ? (currentMp / maxMp) : 0.0f;
    float lanternRatio = maxLantern > 0.0f ? (currentLantern / maxLantern) : 0.0f;
    constexpr float kUiFrameDelta = 1.0f / 60.0f;
    hpRatio = (std::clamp)(hpRatio, 0.0f, 1.0f);
    mpRatio = (std::clamp)(mpRatio, 0.0f, 1.0f);
    lanternRatio = (std::clamp)(lanternRatio, 0.0f, 1.0f);
    currentExpRatio = (std::clamp)(currentExpRatio, 0.0f, 1.0f);

    if (kDebugAutoDrainHudBars)
    {
        mDebugHudDrainTime += kUiFrameDelta;
        const float phase = std::fmod(mDebugHudDrainTime, kDebugHudDrainCycleSeconds) / kDebugHudDrainCycleSeconds;
        const float drainRatio = 1.0f - phase;
        hpRatio = drainRatio;
        mpRatio = drainRatio;
    }

    if (hpRatio > mHpDelayRatio)
        mHpDelayRatio = hpRatio;
    else
        mHpDelayRatio += (hpRatio - mHpDelayRatio) * 0.075f;

    if (mpRatio > mMpDelayRatio)
        mMpDelayRatio = mpRatio;
    else
        mMpDelayRatio += (mpRatio - mMpDelayRatio) * 0.09f;

    if (lanternRatio > mLanternDelayRatio)
        mLanternDelayRatio = lanternRatio;
    else
        mLanternDelayRatio += (lanternRatio - mLanternDelayRatio) * 0.08f;

    const bool isLanternFull = lanternRatio >= 0.999f;
    if (isLanternFull || mMirrorCrackWarningActive)
    {
        mLanternGlowTime += kUiFrameDelta;
    }
    else
    {
        mLanternGlowTime = 0.0f;
    }

    if (mMirrorCrackWarningActive)
    {
        mMirrorCrackWarningTime += kUiFrameDelta;
    }

    auto updateBar = [](GameObject* bar, float ratio, float maxScaleX, float scaleY, float leftEdgeX, float y, float z)
        {
            if (!bar) return;

            ratio = (std::clamp)(ratio, 0.0f, 1.0f);
            const float currentScale = maxScaleX * ratio;
            bar->SetScale(currentScale, scaleY, 1.0f);
            bar->SetPosition(leftEdgeX + currentScale, y, z);
            SetTexScale(bar->Ritem, ratio);
        };

    const float barLeftEdgeX = kHudCenterX + kHudBarLeftOffsetX;
    updateBar(mHpBarDelay, mHpDelayRatio, kHudBarMaxScaleX, kHudHpScaleY, barLeftEdgeX, kHudHpY, 0.136f);
    updateBar(mHpBarFill, hpRatio, kHudBarMaxScaleX, kHudHpScaleY, barLeftEdgeX, kHudHpY, 0.132f);
    updateBar(mMpBarDelay, mMpDelayRatio, kHudBarMaxScaleX, kHudMpScaleY, barLeftEdgeX, kHudMpY, 0.128f);
    updateBar(mMpBarFill, mpRatio, kHudBarMaxScaleX, kHudMpScaleY, barLeftEdgeX, kHudMpY, 0.124f);
    updateBar(mExpBarFill, currentExpRatio, kHudBarMaxScaleX, kHudExpScaleY, barLeftEdgeX, kHudExpY, 0.121f);

    const auto viewport = mGame->GetScreenViewport();
    const float lanternAspectFix = viewport.Width > 0.0f ? (viewport.Height / viewport.Width) : (9.0f / 16.0f);

    if (mLanternRingFillRitem)
    {
        constexpr UINT kIndicesPerRingSegment = 6;
        const UINT fullIndexCount = mLanternRingFillRitem->Geo->DrawArgs["ring"].IndexCount;
        const UINT segmentCount = fullIndexCount / kIndicesPerRingSegment;
        const UINT activeSegments = static_cast<UINT>(std::ceil(mLanternDelayRatio * static_cast<float>(segmentCount)));
        mLanternRingFillRitem->IndexCount = (std::min)(activeSegments, segmentCount) * kIndicesPerRingSegment;
    }

    const float glowPulse = isLanternFull ? (0.5f + 0.5f * std::sin(mLanternGlowTime * 7.5f)) : 0.0f;
    const float mirrorWarningPulse = mMirrorCrackWarningActive ? (0.5f + 0.5f * std::sin(mLanternGlowTime * 13.0f)) : 0.0f;
    if (mLanternRingMat)
    {
        mLanternRingMat->DiffuseAlbedo = isLanternFull
            ? DirectX::XMFLOAT4(0.35f + glowPulse * 0.25f, 1.0f, 0.52f + glowPulse * 0.25f, 1.0f)
            : DirectX::XMFLOAT4(0.08f, 0.94f, 0.38f, 1.0f);
        if (mMirrorCrackWarningActive)
        {
            mLanternRingMat->DiffuseAlbedo = DirectX::XMFLOAT4(0.42f + mirrorWarningPulse * 0.28f, 1.0f, 0.88f + mirrorWarningPulse * 0.12f, 1.0f);
        }
        mLanternRingMat->NumFramesDirty = gNumFrameResources;
    }
    if (mLanternGlowMat)
    {
        mLanternGlowMat->DiffuseAlbedo = isLanternFull
            ? DirectX::XMFLOAT4(0.28f, 1.0f, 0.48f, 0.52f + glowPulse * 0.14f)
            : DirectX::XMFLOAT4(0.14f, 0.95f, 0.42f, 0.34f);
        if (mMirrorCrackWarningActive)
        {
            mLanternGlowMat->DiffuseAlbedo = DirectX::XMFLOAT4(0.34f, 1.0f, 0.88f, 0.62f + mirrorWarningPulse * 0.24f);
        }
        mLanternGlowMat->NumFramesDirty = gNumFrameResources;
    }
    if (mLanternIconMat)
    {
        mLanternIconMat->DiffuseAlbedo = isLanternFull
            ? DirectX::XMFLOAT4(0.24f, 1.0f, 0.55f + glowPulse * 0.28f, 1.0f)
            : DirectX::XMFLOAT4(0.18f, 1.0f, 0.36f, 1.0f);
        mLanternIconMat->NumFramesDirty = gNumFrameResources;
    }

    if (mLanternOrbGlow)
    {
        const float glowScale =
            kLanternCoreRadius +
            (isLanternFull ? glowPulse * 0.008f : 0.0f) +
            (mMirrorCrackWarningActive ? mirrorWarningPulse * 0.014f : 0.0f);
        mLanternOrbGlow->SetScale(glowScale * lanternAspectFix, glowScale, 1.0f);
    }
    if (mLanternOrbCore)
    {
        const float coreScale = 0.057f;
        mLanternOrbCore->SetScale(coreScale * lanternAspectFix, coreScale, 1.0f);
    }

    mDashCooldownWidget.CooldownRemaining = currentDashCooldown;
    mDashCooldownWidget.CooldownDuration = maxDashCooldown;
    UpdateSkillIconMaterials();
    UpdateCooldownWidget(mSkill1CooldownWidget);
    UpdateCooldownWidget(mSkill2CooldownWidget);
    UpdateCooldownWidget(mDashCooldownWidget);
    UpdateDeathScreenVisuals(showDeathScreen);

    for (auto& obj : mUIObjects)
    {
        obj->Update();
    }
}

void UIManager::SetSkillCooldowns(float currentSkill1Cooldown, float maxSkill1Cooldown, float currentSkill2Cooldown, float maxSkill2Cooldown)
{
    mSkill1CooldownWidget.CooldownRemaining = currentSkill1Cooldown;
    mSkill1CooldownWidget.CooldownDuration = maxSkill1Cooldown;
    mSkill2CooldownWidget.CooldownRemaining = currentSkill2Cooldown;
    mSkill2CooldownWidget.CooldownDuration = maxSkill2Cooldown;
}

void UIManager::UpdateCooldownWidget(CooldownWidget& widget)
{
    const bool isDashWidget = (&widget == &mDashCooldownWidget);

    widget.CooldownRemaining = (std::max)(widget.CooldownRemaining, 0.0f);
    widget.CooldownDuration = (std::max)(widget.CooldownDuration, 0.0f);
    widget.CooldownRatio = (widget.CooldownDuration > 0.0f && widget.CooldownRemaining > 0.0f)
        ? (widget.CooldownRemaining / widget.CooldownDuration)
        : 0.0f;
    widget.CooldownRatio = (std::clamp)(widget.CooldownRatio, 0.0f, 1.0f);

    const bool isActive = widget.CooldownRatio > 0.001f;

    if (widget.FillRitem != nullptr && widget.FillRitem->Geo != nullptr)
    {
        constexpr UINT kIndicesPerDiskSegment = 3;
        const UINT fullIndexCount = widget.FillRitem->Geo->DrawArgs["disk"].IndexCount;
        const UINT segmentCount = fullIndexCount / kIndicesPerDiskSegment;
        UINT activeSegments = 0;
        if (isActive)
        {
            activeSegments = static_cast<UINT>(std::ceil(widget.CooldownRatio * static_cast<float>(segmentCount)));
            activeSegments = (std::min)(activeSegments, segmentCount);
        }

        widget.FillRitem->IndexCount = activeSegments * kIndicesPerDiskSegment;
        widget.FillRitem->Visible = activeSegments > 0;
        widget.FillRitem->NumFramesDirty = gNumFrameResources;
    }

    if (widget.BackMat != nullptr)
    {
        widget.BackMat->DiffuseAlbedo = isActive
            ? (isDashWidget
                ? DirectX::XMFLOAT4(0.08f, 0.09f, 0.12f, 0.92f)
                : DirectX::XMFLOAT4(0.05f, 0.06f, 0.08f, 0.18f))
            : (isDashWidget
                ? DirectX::XMFLOAT4(0.08f, 0.09f, 0.12f, 0.74f)
                : DirectX::XMFLOAT4(0.05f, 0.06f, 0.08f, 0.0f));
        widget.BackMat->NumFramesDirty = gNumFrameResources;
    }

    if (widget.FillMat != nullptr)
    {
        widget.FillMat->DiffuseAlbedo = isActive
            ? (isDashWidget
                ? DirectX::XMFLOAT4(0.02f, 0.03f, 0.04f, 0.82f)
                : DirectX::XMFLOAT4(0.08f, 0.10f, 0.14f, 0.28f))
            : DirectX::XMFLOAT4(0.02f, 0.03f, 0.04f, 0.0f);
        widget.FillMat->NumFramesDirty = gNumFrameResources;
    }

    Material* targetIconMat = widget.IconWarriorMat;
    if (mGame != nullptr)
    {
        switch (mGame->GetSelectedPlayerClass())
        {
        case PlayerClass::Warrior:
            targetIconMat = widget.IconWarriorMat;
            break;
        case PlayerClass::Archer:
            targetIconMat = widget.IconArcherMat;
            break;
        case PlayerClass::Mage:
        case PlayerClass::None:
        default:
            targetIconMat = widget.IconMageMat;
            break;
        }
    }

    if (widget.IconRitem != nullptr && targetIconMat != nullptr)
    {
        widget.IconRitem->Mat = targetIconMat;
        widget.IconRitem->Visible = true;
        targetIconMat->DiffuseAlbedo = isActive
            ? (isDashWidget
                ? DirectX::XMFLOAT4(0.72f, 0.72f, 0.72f, 1.0f)
                : DirectX::XMFLOAT4(0.93f, 0.93f, 0.93f, 1.0f))
            : DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
        targetIconMat->NumFramesDirty = gNumFrameResources;
    }

    if (widget.Frame != nullptr && widget.Frame->Ritem != nullptr && widget.Frame->Ritem->Mat != nullptr)
    {
        widget.Frame->Ritem->Mat->DiffuseAlbedo = isActive
            ? DirectX::XMFLOAT4(0.90f, 0.93f, 0.98f, 0.98f)
            : DirectX::XMFLOAT4(0.98f, 1.0f, 1.0f, 1.0f);
        widget.Frame->Ritem->Mat->NumFramesDirty = gNumFrameResources;
    }
}

void UIManager::UpdateSkillIconMaterials()
{
    PlayerClass activeClass = PlayerClass::Mage;
    if (mGame != nullptr)
    {
        if (auto* player = mGame->GetPlayer())
        {
            activeClass = player->GetClassType();
        }
        else
        {
            activeClass = mGame->GetSelectedPlayerClass();
        }
    }

    Material* skillIcon1Mat = mSkillIcon1MageMat;
    Material* skillIcon2Mat = mSkillIcon2MageMat;
    Material* classEmblemMat = mClassEmblemMageMat;
    switch (activeClass)
    {
    case PlayerClass::Warrior:
        skillIcon1Mat = mSkillIcon1WarriorMat;
        skillIcon2Mat = mSkillIcon2WarriorMat;
        classEmblemMat = mClassEmblemWarriorMat;
        break;
    case PlayerClass::Archer:
        skillIcon1Mat = mSkillIcon1ArcherMat;
        skillIcon2Mat = mSkillIcon2ArcherMat;
        classEmblemMat = mClassEmblemArcherMat;
        break;
    case PlayerClass::Mage:
    case PlayerClass::None:
    default:
        break;
    }

    if (mSkillIcon1Ritem != nullptr && skillIcon1Mat != nullptr)
    {
        mSkillIcon1Ritem->Mat = skillIcon1Mat;
        mSkillIcon1Ritem->NumFramesDirty = gNumFrameResources;
    }

    if (mSkillIcon2Ritem != nullptr && skillIcon2Mat != nullptr)
    {
        mSkillIcon2Ritem->Mat = skillIcon2Mat;
        mSkillIcon2Ritem->NumFramesDirty = gNumFrameResources;
    }

    if (mClassEmblemRitem != nullptr && classEmblemMat != nullptr)
    {
        mClassEmblemRitem->Mat = classEmblemMat;
        mClassEmblemRitem->Visible = true;
        mClassEmblemRitem->NumFramesDirty = gNumFrameResources;
        classEmblemMat->DiffuseAlbedo = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
        classEmblemMat->NumFramesDirty = gNumFrameResources;
    }
}

void UIManager::UpdateDeathScreenVisuals(bool showDeathScreen)
{
    mDeathScreenVisible = showDeathScreen;
    mRespawnButtonHovered = IsRespawnButtonHovered();

    const float targetBlend = showDeathScreen ? 1.0f : 0.0f;
    const float lerpFactor = showDeathScreen ? 0.18f : 0.24f;
    mDeathScreenBlend += (targetBlend - mDeathScreenBlend) * lerpFactor;
    if (std::fabs(targetBlend - mDeathScreenBlend) < 0.001f)
    {
        mDeathScreenBlend = targetBlend;
    }
    mDeathScreenBlend = (std::clamp)(mDeathScreenBlend, 0.0f, 1.0f);

    if (mGame != nullptr)
    {
        mGame->SetDeathScreenEffectAmount(mDeathScreenBlend);
    }

    const bool visible = mDeathScreenBlend > 0.001f;
    const float overlayAlpha = 0.34f * mDeathScreenBlend;
    const float buttonAlpha = (0.90f + (mRespawnButtonHovered ? 0.10f : 0.0f)) * mDeathScreenBlend;
    const float frameAlpha = (0.82f + (mRespawnButtonHovered ? 0.16f : 0.0f)) * mDeathScreenBlend;

    if (mDeathOverlayMat != nullptr)
    {
        mDeathOverlayMat->DiffuseAlbedo = DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, overlayAlpha);
        mDeathOverlayMat->NumFramesDirty = gNumFrameResources;
    }

    if (mRespawnButtonMat != nullptr)
    {
        mRespawnButtonMat->DiffuseAlbedo = mRespawnButtonHovered
            ? DirectX::XMFLOAT4(0.78f, 0.80f, 0.86f, buttonAlpha)
            : DirectX::XMFLOAT4(0.12f, 0.13f, 0.17f, buttonAlpha);
        mRespawnButtonMat->NumFramesDirty = gNumFrameResources;
    }

    if (mRespawnButtonFrameMat != nullptr)
    {
        mRespawnButtonFrameMat->DiffuseAlbedo = mRespawnButtonHovered
            ? DirectX::XMFLOAT4(0.92f, 0.95f, 1.0f, frameAlpha)
            : DirectX::XMFLOAT4(0.58f, 0.60f, 0.66f, frameAlpha);
        mRespawnButtonFrameMat->NumFramesDirty = gNumFrameResources;
    }

    auto updateVisibility = [visible](GameObject* object)
        {
            if (object != nullptr && object->Ritem != nullptr)
            {
                object->Ritem->Visible = visible;
                object->Ritem->NumFramesDirty = gNumFrameResources;
            }
        };

    updateVisibility(mDeathOverlayObj);
    updateVisibility(mRespawnButtonFrameObj);
    updateVisibility(mRespawnButtonObj);
}

bool UIManager::IsRespawnButtonHovered() const
{
    if (!mDeathScreenVisible || mGame == nullptr)
    {
        return false;
    }

    POINT cursor = {};
    if (!GetCursorPos(&cursor) || !ScreenToClient(mGame->GetMainWindowHandle(), &cursor))
    {
        return false;
    }

    RECT clientRect = {};
    if (!GetClientRect(mGame->GetMainWindowHandle(), &clientRect))
    {
        return false;
    }

    const float clientWidth = static_cast<float>(clientRect.right - clientRect.left);
    const float clientHeight = static_cast<float>(clientRect.bottom - clientRect.top);
    if (clientWidth <= 0.0f || clientHeight <= 0.0f)
    {
        return false;
    }

    const float centerX = (kRespawnButtonCenterX + 1.0f) * 0.5f * clientWidth;
    const float centerY = (1.0f - kRespawnButtonCenterY) * 0.5f * clientHeight;
    const float halfWidth = kRespawnButtonScaleX * clientWidth * 0.5f;
    const float halfHeight = kRespawnButtonScaleY * clientHeight * 0.5f;

    return std::fabs(static_cast<float>(cursor.x) - centerX) <= halfWidth &&
        std::fabs(static_cast<float>(cursor.y) - centerY) <= halfHeight;
}

bool UIManager::ConsumeRespawnButtonClick(bool hasFocus)
{
    if (!hasFocus || !mDeathScreenVisible)
    {
        mRespawnButtonPressed = false;
        return false;
    }

    const bool mouseDown = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    if (!mouseDown)
    {
        mRespawnButtonPressed = false;
        return false;
    }

    if (mRespawnButtonPressed)
    {
        return false;
    }

    mRespawnButtonPressed = true;
    return IsRespawnButtonHovered();
}

void UIManager::DrawCooldownOverlay()
{
    const bool hasActiveSkill1Cooldown = mSkill1CooldownWidget.CooldownRatio > 0.001f;
    const bool hasActiveSkill2Cooldown = mSkill2CooldownWidget.CooldownRatio > 0.001f;
    const bool hasActiveDashCooldown = mDashCooldownWidget.CooldownRatio > 0.001f;
    const bool shouldDrawRespawnText = mDeathScreenBlend > 0.001f;

    if (mGame == nullptr ||
        mCooldownTextFont == nullptr ||
        mCooldownTextBatch == nullptr ||
        mCooldownTextHeap == nullptr ||
        (!hasActiveSkill1Cooldown && !hasActiveSkill2Cooldown && !hasActiveDashCooldown && !shouldDrawRespawnText))
    {
        return;
    }

    auto* cmdList = mGame->GetCommandList();
    if (cmdList == nullptr)
    {
        return;
    }

    const auto viewport = mGame->GetScreenViewport();
    if (viewport.Width <= 0.0f || viewport.Height <= 0.0f)
    {
        return;
    }

    try
    {
        ID3D12DescriptorHeap* heaps[] = { mCooldownTextHeap->Heap() };
        cmdList->SetDescriptorHeaps(1, heaps);

        mCooldownTextBatch->SetViewport(viewport);
        mCooldownTextBatch->Begin(cmdList);
        DrawCooldownWidgetText(mSkill1CooldownWidget);
        DrawCooldownWidgetText(mSkill2CooldownWidget);
        DrawCooldownWidgetText(mDashCooldownWidget);
        DrawRespawnButtonText();
        mCooldownTextBatch->End();
    }
    catch (const std::exception& e)
    {
        std::string log = "[UIManager] Failed to draw cooldown text: ";
        log += e.what();
        log += "\n";
        OutputDebugStringA(log.c_str());
    }
}

void UIManager::DrawCooldownWidgetText(const CooldownWidget& widget)
{
    if (mGame == nullptr ||
        mCooldownTextFont == nullptr ||
        mCooldownTextBatch == nullptr ||
        widget.CooldownRatio <= 0.001f)
    {
        return;
    }

    const auto viewport = mGame->GetScreenViewport();
    const int cooldownSeconds = (std::max)(1, static_cast<int>(std::ceil(widget.CooldownRemaining)));
    const std::wstring label = std::to_wstring(cooldownSeconds);
    const DirectX::XMVECTOR textSize = mCooldownTextFont->MeasureString(label.c_str());
    const float textWidth = DirectX::XMVectorGetX(textSize) * kDashCooldownTextScale;
    const float textHeight = DirectX::XMVectorGetY(textSize) * kDashCooldownTextScale;
    const float centerX = (widget.CenterX + 1.0f) * 0.5f * viewport.Width;
    const float centerY = (1.0f - widget.CenterY) * 0.5f * viewport.Height;
    const DirectX::XMFLOAT2 textPos(
        centerX - textWidth * 0.5f,
        centerY - textHeight * 0.5f - 1.0f);

    const DirectX::XMVECTORF32 shadowColor = { 0.0f, 0.0f, 0.0f, 0.82f };
    const DirectX::XMVECTORF32 textColor = { 0.96f, 0.98f, 1.0f, 1.0f };

    mCooldownTextFont->DrawString(
        mCooldownTextBatch.get(),
        label.c_str(),
        DirectX::XMFLOAT2(textPos.x + 1.0f, textPos.y + 1.0f),
        shadowColor,
        0.0f,
        DirectX::XMFLOAT2(0.0f, 0.0f),
        kDashCooldownTextScale);
    mCooldownTextFont->DrawString(
        mCooldownTextBatch.get(),
        label.c_str(),
        textPos,
        textColor,
        0.0f,
        DirectX::XMFLOAT2(0.0f, 0.0f),
        kDashCooldownTextScale);
}

void UIManager::DrawRespawnButtonText()
{
    if (mGame == nullptr ||
        mCooldownTextFont == nullptr ||
        mCooldownTextBatch == nullptr ||
        mDeathScreenBlend <= 0.001f)
    {
        return;
    }

    const auto viewport = mGame->GetScreenViewport();
    const std::wstring label = L"RESPAWN";
    const DirectX::XMVECTOR textSize = mCooldownTextFont->MeasureString(label.c_str());
    const float textWidth = DirectX::XMVectorGetX(textSize) * kRespawnButtonTextScale;
    const float textHeight = DirectX::XMVectorGetY(textSize) * kRespawnButtonTextScale;
    const float centerX = (kRespawnButtonCenterX + 1.0f) * 0.5f * viewport.Width;
    const float centerY = (1.0f - kRespawnButtonCenterY) * 0.5f * viewport.Height;
    const DirectX::XMFLOAT2 textPos(
        centerX - textWidth * 0.5f,
        centerY - textHeight * 0.5f - 1.0f);

    const DirectX::XMVECTORF32 shadowColor = { 0.02f, 0.02f, 0.03f, 0.92f * mDeathScreenBlend };
    const DirectX::XMVECTORF32 textColor = mRespawnButtonHovered
        ? DirectX::XMVECTORF32{ 0.05f, 0.06f, 0.08f, 1.0f * mDeathScreenBlend }
        : DirectX::XMVECTORF32{ 0.96f, 0.98f, 1.0f, 1.0f * mDeathScreenBlend };

    mCooldownTextFont->DrawString(
        mCooldownTextBatch.get(),
        label.c_str(),
        DirectX::XMFLOAT2(textPos.x + 1.0f, textPos.y + 1.0f),
        shadowColor,
        0.0f,
        DirectX::XMFLOAT2(0.0f, 0.0f),
        kRespawnButtonTextScale);
    mCooldownTextFont->DrawString(
        mCooldownTextBatch.get(),
        label.c_str(),
        textPos,
        textColor,
        0.0f,
        DirectX::XMFLOAT2(0.0f, 0.0f),
        kRespawnButtonTextScale);
}

void UIManager::UpdateBossHealthBar(float currentHp, float maxHp)
{
    if (maxHp <= 0.0f || currentHp <= 0.0f)
    {
        HideBossHealthBar();
        return;
    }

    constexpr int kBossHpLayerCount = 200;
    const float clampedHp = (std::clamp)(currentHp, 0.0f, maxHp);
    const float hpPerLayer = maxHp / static_cast<float>(kBossHpLayerCount);
    const int visibleLayer = (std::clamp)(
        static_cast<int>(std::ceil(clampedHp / hpPerLayer)),
        1,
        kBossHpLayerCount);
    const float layerBaseHp = hpPerLayer * static_cast<float>(visibleLayer - 1);
    const float layerHp = clampedHp - layerBaseHp;
    const float layerRatio = (std::clamp)(layerHp / hpPerLayer, 0.0f, 1.0f);

    if (visibleLayer != mBossHpVisibleLayer)
    {
        mBossHpVisibleLayer = visibleLayer;
        mBossHpDelayRatio = 1.0f;
    }
    else if (layerRatio > mBossHpDelayRatio)
    {
        mBossHpDelayRatio = layerRatio;
    }
    else
    {
        mBossHpDelayRatio += (layerRatio - mBossHpDelayRatio) * 0.055f;
    }

    const DirectX::XMFLOAT4 nonFinalLayerColors[] =
    {
        { 0.17f, 0.62f, 0.92f, 1.0f },
        { 0.42f, 0.25f, 0.95f, 1.0f },
        { 0.84f, 0.16f, 0.76f, 1.0f },
        { 0.88f, 0.58f, 0.06f, 1.0f },
        { 0.92f, 0.22f, 0.05f, 1.0f }
    };
    constexpr int nonFinalPaletteCount = static_cast<int>(sizeof(nonFinalLayerColors) / sizeof(nonFinalLayerColors[0]));
    const DirectX::XMFLOAT4 fillColor = (visibleLayer == 1)
        ? DirectX::XMFLOAT4{ 0.78f, 0.025f, 0.04f, 1.0f }
        : nonFinalLayerColors[(kBossHpLayerCount - visibleLayer) % nonFinalPaletteCount];
    const DirectX::XMFLOAT4 nextLayerColor = (visibleLayer <= 1)
        ? DirectX::XMFLOAT4{ 0.09f, 0.012f, 0.016f, 0.94f }
        : ((visibleLayer - 1 == 1)
            ? DirectX::XMFLOAT4{ 0.78f, 0.025f, 0.04f, 1.0f }
            : nonFinalLayerColors[(kBossHpLayerCount - (visibleLayer - 1)) % nonFinalPaletteCount]);
    if (mBossHpBackMat != nullptr)
    {
        mBossHpBackMat->DiffuseAlbedo = nextLayerColor;
        mBossHpBackMat->NumFramesDirty = gNumFrameResources;
    }
    if (mBossHpFillMat != nullptr)
    {
        mBossHpFillMat->DiffuseAlbedo = fillColor;
        mBossHpFillMat->NumFramesDirty = gNumFrameResources;
    }
    if (mBossHpDelayMat != nullptr)
    {
        mBossHpDelayMat->DiffuseAlbedo = {
            (std::min)(fillColor.x + 0.24f, 1.0f),
            (std::min)(fillColor.y + 0.24f, 1.0f),
            (std::min)(fillColor.z + 0.18f, 1.0f),
            0.90f
        };
        mBossHpDelayMat->NumFramesDirty = gNumFrameResources;
    }
    if (mBossHpGlossMat != nullptr)
    {
        mBossHpGlossMat->DiffuseAlbedo = {
            (std::min)(fillColor.x + 0.35f, 1.0f),
            (std::min)(fillColor.y + 0.35f, 1.0f),
            (std::min)(fillColor.z + 0.30f, 1.0f),
            0.34f
        };
        mBossHpGlossMat->NumFramesDirty = gNumFrameResources;
    }

    auto setVisible = [](GameObject* object, bool visible)
        {
            if (object != nullptr && object->Ritem != nullptr)
            {
                object->Ritem->Visible = visible;
            }
        };

    setVisible(mBossHpFrame, true);
    setVisible(mBossHpBack, true);
    setVisible(mBossHpDelay, true);
    setVisible(mBossHpFill, true);
    setVisible(mBossHpGloss, true);

    auto updateBar = [](GameObject* bar, float ratio, float maxScaleX, float scaleY, float leftEdgeX, float y, float z)
        {
            if (bar == nullptr)
            {
                return;
            }

            const float currentScale = maxScaleX * (std::max)(ratio, 0.0f);
            bar->SetScale(currentScale, scaleY, 1.0f);
            bar->SetPosition(leftEdgeX + currentScale, y, z);
            bar->Update();
        };

    constexpr float bossBarLeftEdgeX = -kBossBarFillMaxScaleX;

    updateBar(mBossHpDelay, mBossHpDelayRatio, kBossBarFillMaxScaleX, kBossBarFillScaleY, bossBarLeftEdgeX, kBossBarY, 0.095f);
    updateBar(mBossHpFill, layerRatio, kBossBarFillMaxScaleX, kBossBarFillScaleY, bossBarLeftEdgeX, kBossBarY, 0.090f);
    updateBar(mBossHpGloss, layerRatio, kBossBarFillMaxScaleX, kBossBarGlossScaleY, bossBarLeftEdgeX, kBossBarY + 0.006f, 0.085f);

    if (mBossHpFrame) mBossHpFrame->Update();
    if (mBossHpBack) mBossHpBack->Update();
}

void UIManager::HideBossHealthBar()
{
    mBossHpDelayRatio = 1.0f;
    mBossHpVisibleLayer = 0;

    GameObject* bossBarObjects[] =
    {
        mBossHpFrame,
        mBossHpBack,
        mBossHpDelay,
        mBossHpFill,
        mBossHpGloss,
        mBossHpLeftCap,
        mBossHpRightCap
    };

    for (GameObject* object : bossBarObjects)
    {
        if (object != nullptr && object->Ritem != nullptr)
        {
            object->Ritem->Visible = false;
            object->Update();
        }
    }

}

void UIManager::ShowMirrorCrackWarning(float progress)
{
    progress = (std::clamp)(progress, 0.0f, 1.0f);
    mMirrorCrackWarningActive = true;
    mMirrorCrackWarningProgress = progress;

    if (mGame != nullptr)
    {
        mGame->SetMirrorBreakEffect(progress);
    }

    const float pulse = 0.5f + 0.5f * std::sin(mMirrorCrackWarningTime * 12.0f);
    const float alpha = (std::clamp)(0.14f + progress * 0.58f + pulse * 0.08f, 0.0f, 0.84f);

    if (mMirrorCrackMat != nullptr)
    {
        mMirrorCrackMat->DiffuseAlbedo = DirectX::XMFLOAT4(
            0.72f + pulse * 0.18f,
            0.92f + pulse * 0.08f,
            1.0f,
            alpha);
        mMirrorCrackMat->NumFramesDirty = gNumFrameResources;
    }

    if (mMirrorCrackObj != nullptr)
    {
        mMirrorCrackObj->SetScale(0.0f, 0.0f, 1.0f);
        if (mMirrorCrackObj->Ritem != nullptr)
        {
            mMirrorCrackObj->Ritem->Visible = false;
            mMirrorCrackObj->Ritem->NumFramesDirty = gNumFrameResources;
        }
        mMirrorCrackObj->Update();
    }
}

void UIManager::HideMirrorCrackWarning()
{
    mMirrorCrackWarningActive = false;
    mMirrorCrackWarningProgress = 0.0f;
    mMirrorCrackWarningTime = 0.0f;

    if (mGame != nullptr)
    {
        mGame->ClearMirrorBreakEffect();
    }

    if (mMirrorCrackMat != nullptr)
    {
        mMirrorCrackMat->DiffuseAlbedo = DirectX::XMFLOAT4(0.82f, 0.96f, 1.0f, 0.0f);
        mMirrorCrackMat->NumFramesDirty = gNumFrameResources;
    }

    if (mMirrorCrackObj != nullptr)
    {
        mMirrorCrackObj->SetScale(0.0f, 0.0f, 1.0f);
        if (mMirrorCrackObj->Ritem != nullptr)
        {
            mMirrorCrackObj->Ritem->Visible = false;
            mMirrorCrackObj->Ritem->NumFramesDirty = gNumFrameResources;
        }
        mMirrorCrackObj->Update();
    }
}

void UIManager::SetChatBoxState(bool active, bool hasMessages)
{
    if (mChatLogMat)
    {
        mChatLogMat->DiffuseAlbedo = hasMessages
            ? DirectX::XMFLOAT4(0.04f, 0.05f, 0.07f, 0.84f)
            : DirectX::XMFLOAT4(0.04f, 0.05f, 0.07f, 0.72f);
        mChatLogMat->NumFramesDirty = 3;
    }

    if (mChatInputMat)
    {
        mChatInputMat->DiffuseAlbedo = active
            ? DirectX::XMFLOAT4(0.16f, 0.12f, 0.05f, 0.94f)
            : DirectX::XMFLOAT4(0.1f, 0.12f, 0.16f, 0.9f);
        mChatInputMat->NumFramesDirty = 3;
    }

    if (mChatLogBg) mChatLogBg->Update();
    if (mChatInputBg) mChatInputBg->Update();
}

void UIManager::InitializeEffect(Material* flashMat, Material* bgMat, GameObject* flashObj, GameObject* screenBgObj)
{
    mFlashMat = flashMat;
    mBgMat = bgMat;
    mFlashObj = flashObj;
    mScreenBgObj = screenBgObj;
    mUseShortFlashProfile = false;
    mFlashPeakAlpha = 1.0f;
    mBgPeakAlpha = 1.0f;
    mFlashBaseColor = XMFLOAT4(1.0f, 1.0f, 1.0f, 0.0f);
    mBgBaseColor = XMFLOAT4(1.0f, 1.0f, 1.0f, 0.0f);

    // 평소에는 눈에 보이지 않도록 투명도(Alpha)를 0으로 꺼둡니다.
    if (mFlashMat) {
        mFlashMat->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 0.0f);
        mFlashMat->NumFramesDirty = 3;
    }
    if (mBgMat) {
        mBgMat->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 0.0f);
        mBgMat->NumFramesDirty = 3;
    }

    if (mFlashObj) {
        mFlashObj->SetScale(0.0f, 0.0f, 1.0f);
        mFlashObj->Update();
    }

    if (mScreenBgObj) {
        mScreenBgObj->SetScale(0.0f, 0.0f, 1.0f);
        mScreenBgObj->Update();
    }
}

void UIManager::TriggerFlashEffect()
{
    mIsFlashActive = true;
    mCurrentTime = 0.0f;
    mFlashDuration = 1.55f;
    mUseShortFlashProfile = false;
    mFlashPeakAlpha = 1.0f;
    mBgPeakAlpha = 1.0f;
    mFlashBaseColor = XMFLOAT4(1.0f, 0.95f, 0.82f, 0.0f);
    mBgBaseColor = XMFLOAT4(0.95f, 0.9f, 0.72f, 0.0f);

    if (mFlashMat) {
        mFlashMat->DiffuseAlbedo = mFlashBaseColor;
        mFlashMat->NumFramesDirty = 3;
    }

    if (mBgMat) {
        mBgMat->DiffuseAlbedo = mBgBaseColor;
        mBgMat->NumFramesDirty = 3;
    }

    if (mScreenBgObj) {
        mScreenBgObj->SetScale(1.05f, 1.05f, 1.0f);
        mScreenBgObj->SetPosition(0.0f, 0.0f, 0.18f);
        mScreenBgObj->Update();
    }

    if (mFlashObj) {
        mFlashObj->SetScale(1.35f, 1.35f, 1.0f);
        mFlashObj->SetPosition(0.0f, 0.0f, 0.12f);
        mFlashObj->Update();
    }
}

void UIManager::TriggerLevelUpFlashEffect(PlayerClass playerClass, int newLevel)
{
    mIsFlashActive = true;
    mCurrentTime = 0.0f;
    mFlashDuration = newLevel >= 3 ? 0.58f : 0.44f;
    mUseShortFlashProfile = true;
    mFlashPeakAlpha = newLevel >= 3 ? 0.62f : 0.44f;
    mBgPeakAlpha = newLevel >= 3 ? 0.24f : 0.16f;
    mFlashBaseColor = GetLevelUpFlashColor(playerClass, newLevel);
    mBgBaseColor = {
        mFlashBaseColor.x * 0.42f,
        mFlashBaseColor.y * 0.42f,
        mFlashBaseColor.z * 0.42f,
        0.0f
    };

    if (mFlashMat) {
        mFlashMat->DiffuseAlbedo = mFlashBaseColor;
        mFlashMat->NumFramesDirty = 3;
    }

    if (mBgMat) {
        mBgMat->DiffuseAlbedo = mBgBaseColor;
        mBgMat->NumFramesDirty = 3;
    }

    if (mScreenBgObj) {
        mScreenBgObj->SetScale(1.02f, 1.02f, 1.0f);
        mScreenBgObj->SetPosition(0.0f, 0.0f, 0.18f);
        mScreenBgObj->Update();
    }

    if (mFlashObj) {
        const float flashScale = newLevel >= 3 ? 1.25f : 1.15f;
        mFlashObj->SetScale(flashScale, flashScale, 1.0f);
        mFlashObj->SetPosition(0.0f, 0.0f, 0.12f);
        mFlashObj->Update();
    }
}

void UIManager::UpdateEffect(float dt)
{
    if (!mIsFlashActive) return;

    mCurrentTime += dt;
    float bgAlpha = 0.0f;
    float flashAlpha = 0.0f;

    if (mUseShortFlashProfile)
    {
        const float duration = (std::max)(mFlashDuration, 0.001f);
        const float normalizedTime = (std::clamp)(mCurrentTime / duration, 0.0f, 1.0f);
        if (normalizedTime < 0.18f)
        {
            const float t = normalizedTime / 0.18f;
            bgAlpha = mBgPeakAlpha * t;
            flashAlpha = mFlashPeakAlpha * t;
        }
        else if (normalizedTime < 0.42f)
        {
            const float t = (normalizedTime - 0.18f) / 0.24f;
            bgAlpha = mBgPeakAlpha + (mBgPeakAlpha * 0.45f - mBgPeakAlpha) * t;
            flashAlpha = mFlashPeakAlpha + (mFlashPeakAlpha * 0.60f - mFlashPeakAlpha) * t;
        }
        else if (normalizedTime < 1.0f)
        {
            const float t = (normalizedTime - 0.42f) / 0.58f;
            bgAlpha = (mBgPeakAlpha * 0.45f) * (1.0f - t);
            flashAlpha = (mFlashPeakAlpha * 0.60f) * (1.0f - t);
        }
        else
        {
            bgAlpha = 0.0f;
            flashAlpha = 0.0f;
            mIsFlashActive = false;
        }
    }
    else if (mCurrentTime < 0.22f)
    {
        float t = mCurrentTime / 0.22f;
        bgAlpha = 0.18f + (t * 0.45f);
        flashAlpha = 0.35f + (t * 0.45f);
    }
    else if (mCurrentTime < 0.46f)
    {
        float t = (mCurrentTime - 0.22f) / 0.24f;
        bgAlpha = 0.63f + (t * 0.28f);
        flashAlpha = 0.8f + (t * 0.2f);
    }
    else if (mCurrentTime < 1.12f)
    {
        bgAlpha = 1.0f;
        flashAlpha = 1.0f;
    }
    else if (mCurrentTime < mFlashDuration)
    {
        float t = (mCurrentTime - 1.12f) / (mFlashDuration - 1.12f);
        bgAlpha = (1.0f - t);
        flashAlpha = (1.0f - t) * 0.78f;
    }
    else
    {
        bgAlpha = 0.0f;
        flashAlpha = 0.0f;
        mIsFlashActive = false;
    }

    if (mBgMat) {
        mBgMat->DiffuseAlbedo = mBgBaseColor;
        mBgMat->DiffuseAlbedo.w = bgAlpha;
        mBgMat->NumFramesDirty = 3;
    }

    if (mFlashMat) {
        mFlashMat->DiffuseAlbedo = mFlashBaseColor;
        mFlashMat->DiffuseAlbedo.w = flashAlpha;
        mFlashMat->NumFramesDirty = 3;
    }

    if (!mIsFlashActive)
    {
        if (mScreenBgObj) {
            mScreenBgObj->SetScale(0.0f, 0.0f, 1.0f);
            mScreenBgObj->Update();
        }

        if (mFlashObj) {
            mFlashObj->SetScale(0.0f, 0.0f, 1.0f);
            mFlashObj->Update();
        }
    }
}
