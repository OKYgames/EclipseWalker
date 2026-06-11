#include "UIManager.h"
#include "EclipseWalkerGame.h"
#include "Vertices.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

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
    constexpr float kHudHpScaleY = 0.034f;
    constexpr float kHudMpScaleY = 0.034f;
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
    constexpr float kBossBarCenterX = 0.0f;
    constexpr float kBossBarY = 0.84f;
    constexpr float kBossBarFrameAspect = 11.40f;
    constexpr float kBossBarFrameScaleY = 0.085f;
    constexpr float kBossBarFillMaxScaleX = 0.360f;
    constexpr float kBossBarFillScaleY = 0.018f;
    constexpr float kBossBarGlossScaleY = 0.005f;

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
    createUITextureMaterial("UI_LanternFrameTexMat", "UI_Lantern_Frame", DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
    createUITextureMaterial("UI_LanternRingFillTexMat", "UI_Lantern_Ring_Fill", DirectX::XMFLOAT4(0.72f, 1.0f, 0.78f, 1.0f));
    createUITextureMaterial("UI_LanternCoreGlowTexMat", "UI_Lantern_Core_Glow", DirectX::XMFLOAT4(0.85f, 1.0f, 0.86f, 0.92f));
    createUITextureMaterial("UI_SkillBarTwoSlotsTexMat", "UI_SkillBar_TwoSlots", DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
    createUITextureMaterial("UI_SkillMageHealingLightTexMat", "UI_Skill_Mage_HealingLight", DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
    createUITextureMaterial("UI_SkillMageMeteorTexMat", "UI_Skill_Mage_Meteor", DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
    createUITextureMaterial("UI_BossHpFrameTexMat", "UI_BossHp_Frame", DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
    createUITextureMaterial("UI_MirrorCrackMat", "UI_MirrorCrackOverlay", DirectX::XMFLOAT4(0.82f, 0.96f, 1.0f, 0.0f));
    createUIMaterial("UI_HpBackMat", DirectX::XMFLOAT4(0.13f, 0.025f, 0.03f, 1.0f));
    createUIMaterial("UI_HpDelayMat", DirectX::XMFLOAT4(0.95f, 0.48f, 0.22f, 1.0f));
    createUIMaterial("UI_HpMat", DirectX::XMFLOAT4(0.86f, 0.04f, 0.06f, 1.0f));
    createUIMaterial("UI_HpGlossMat", DirectX::XMFLOAT4(1.0f, 0.48f, 0.42f, 0.42f));
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
                const float angle = DirectX::XM_2PI * static_cast<float>(i) / static_cast<float>(segmentCount);
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
    const float skillBarScaleX = kSkillBarScaleY * kSkillBarAspect * lanternAspectFix;
    const float skillBarCenterX = 1.0f - kSkillBarMarginX - skillBarScaleX;
    const float skillBarCenterY = -1.0f + kSkillBarMarginY + kSkillBarScaleY;

    mHpMpFrame = createUIQuad("UI_HPMPFrameMat", kHudFrameScaleX, kHudFrameScaleY, kHudCenterX, kHudCenterY, 0.142f);
    mHpBarDelay = createUIQuad("UI_HPDelayTexMat", kHudBarMaxScaleX, kHudHpScaleY, kHudCenterX + kHudBarLeftOffsetX + kHudBarMaxScaleX, kHudHpY, 0.136f);
    mHpBarFill = createUIQuad("UI_HPFillTexMat", kHudBarMaxScaleX, kHudHpScaleY, kHudCenterX + kHudBarLeftOffsetX + kHudBarMaxScaleX, kHudHpY, 0.132f);
    mMpBarDelay = createUIQuad("UI_MPDelayTexMat", kHudBarMaxScaleX, kHudMpScaleY, kHudCenterX + kHudBarLeftOffsetX + kHudBarMaxScaleX, kHudMpY, 0.128f);
    mMpBarFill = createUIQuad("UI_MPFillTexMat", kHudBarMaxScaleX, kHudMpScaleY, kHudCenterX + kHudBarLeftOffsetX + kHudBarMaxScaleX, kHudMpY, 0.124f);
    mHpMpGloss = createUIQuad("UI_HPMPGlossMat", kHudFrameScaleX, kHudFrameScaleY, kHudCenterX, kHudCenterY, 0.120f);

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
    createUIQuad("UI_SkillMageHealingLightTexMat", skillIconScaleX, kSkillIconScaleY,
        skillBarCenterX - skillIconOffsetX, skillBarCenterY + kSkillIconOffsetY, 0.086f);
    createUIQuad("UI_SkillMageMeteorTexMat", skillIconScaleX, kSkillIconScaleY,
        skillBarCenterX + skillIconOffsetX, skillBarCenterY + kSkillIconOffsetY, 0.084f);

    auto chatLogRitem = std::make_unique<RenderItem>();
    chatLogRitem->Geo = res->mGeometries["quadGeo"].get();
    chatLogRitem->Mat = res->GetMaterial("UI_ChatLogMat");
    chatLogRitem->ObjCBIndex = static_cast<UINT>(ritems.size());
    setupRitem(chatLogRitem.get());

    auto chatLogObj = std::make_unique<GameObject>();
    chatLogObj->SetScale(0.29f, 0.16f, 1.0f);
    chatLogObj->SetPosition(-0.705f, -0.62f, 0.11f);
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
    chatInputObj->SetScale(0.29f, 0.045f, 1.0f);
    chatInputObj->SetPosition(-0.705f, -0.87f, 0.11f);
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
}

void UIManager::Update(float currentHp, float maxHp, float currentMp, float maxMp, float currentLantern, float maxLantern)
{
    float hpRatio = maxHp > 0.0f ? (currentHp / maxHp) : 0.0f;
    float mpRatio = maxMp > 0.0f ? (currentMp / maxMp) : 0.0f;
    float lanternRatio = maxLantern > 0.0f ? (currentLantern / maxLantern) : 0.0f;
    constexpr float kUiFrameDelta = 1.0f / 60.0f;
    hpRatio = (std::clamp)(hpRatio, 0.0f, 1.0f);
    mpRatio = (std::clamp)(mpRatio, 0.0f, 1.0f);
    lanternRatio = (std::clamp)(lanternRatio, 0.0f, 1.0f);

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

    for (auto& obj : mUIObjects)
    {
        obj->Update();
    }
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

    if (mFlashMat) {
        mFlashMat->DiffuseAlbedo = XMFLOAT4(1.0f, 0.95f, 0.82f, 0.0f);
        mFlashMat->NumFramesDirty = 3;
    }

    if (mBgMat) {
        mBgMat->DiffuseAlbedo = XMFLOAT4(0.95f, 0.9f, 0.72f, 0.0f);
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

void UIManager::UpdateEffect(float dt)
{
    if (!mIsFlashActive) return;

    mCurrentTime += dt;
    float bgAlpha = 0.0f;
    float flashAlpha = 0.0f;

    if (mCurrentTime < 0.22f)
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
        mBgMat->DiffuseAlbedo.w = bgAlpha;
        mBgMat->NumFramesDirty = 3;
    }

    if (mFlashMat) {
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
