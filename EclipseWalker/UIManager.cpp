#include "UIManager.h"
#include "EclipseWalkerGame.h"
#include "Vertices.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace
{
    constexpr bool kDigitSegments[10][7] =
    {
        { true,  true,  true,  true,  true,  true,  false },
        { false, true,  true,  false, false, false, false },
        { true,  true,  false, true,  true,  false, true  },
        { true,  true,  true,  true,  false, false, true  },
        { false, true,  true,  false, false, true,  true  },
        { true,  false, true,  true,  false, true,  true  },
        { true,  false, true,  true,  true,  true,  true  },
        { true,  true,  true,  false, false, false, false },
        { true,  true,  true,  true,  true,  true,  true  },
        { true,  true,  true,  true,  false, true,  true  }
    };
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

    createUIMaterial("UI_HudShadowMat", DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.52f));
    createUIMaterial("UI_HudPanelMat", DirectX::XMFLOAT4(0.025f, 0.026f, 0.032f, 0.96f));
    createUIMaterial("UI_HudFrameMat", DirectX::XMFLOAT4(0.62f, 0.66f, 0.70f, 1.0f));
    createUIMaterial("UI_HudInnerFrameMat", DirectX::XMFLOAT4(0.12f, 0.10f, 0.08f, 1.0f));
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
    createUIMaterial("UI_BossHpTextMat", DirectX::XMFLOAT4(1.0f, 0.90f, 0.45f, 1.0f));
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
    mLanternRingMat = res->GetMaterial("UI_LanternRingMat");
    mLanternGlowMat = res->GetMaterial("UI_LanternOrbGlowMat");
    mLanternIconMat = res->GetMaterial("UI_LanternIconMat");
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
            constexpr float innerRadius = 0.74f;
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

    const float barLeftEdgeX = -0.985f;
    const float hpMaxScaleX = 0.24f;
    const float mpMaxScaleX = 0.215f;
    const float hpCenterX = barLeftEdgeX + hpMaxScaleX;
    const float mpCenterX = barLeftEdgeX + mpMaxScaleX;
    const float hpY = 0.91f;
    const float mpY = 0.845f;
    const float lanternCenterX = 0.88f;
    const float lanternCenterY = 0.0f;
    const auto viewport = mGame->GetScreenViewport();
    const float lanternAspectFix = viewport.Width > 0.0f ? (viewport.Height / viewport.Width) : (9.0f / 16.0f);

    createUIQuad("UI_HudFrameMat", hpMaxScaleX + 0.011f, 0.036f, hpCenterX, hpY, 0.14f);
    createUIQuad("UI_HpBackMat", hpMaxScaleX, 0.026f, hpCenterX, hpY, 0.13f);
    mHpBarDelay = createUIQuad("UI_HpDelayMat", hpMaxScaleX, 0.026f, hpCenterX, hpY, 0.12f);
    mHpBarFill = createUIQuad("UI_HpMat", hpMaxScaleX, 0.026f, hpCenterX, hpY, 0.11f);
    mHpBarGloss = createUIQuad("UI_HpGlossMat", hpMaxScaleX, 0.006f, hpCenterX, hpY + 0.013f, 0.10f);

    createUIQuad("UI_HudFrameMat", mpMaxScaleX + 0.011f, 0.031f, mpCenterX, mpY, 0.14f);
    createUIQuad("UI_MpBackMat", mpMaxScaleX, 0.021f, mpCenterX, mpY, 0.13f);
    mMpBarDelay = createUIQuad("UI_MpDelayMat", mpMaxScaleX, 0.021f, mpCenterX, mpY, 0.12f);
    mMpBarFill = createUIQuad("UI_MpMat", mpMaxScaleX, 0.021f, mpCenterX, mpY, 0.11f);
    mMpBarGloss = createUIQuad("UI_MpGlossMat", mpMaxScaleX, 0.0045f, mpCenterX, mpY + 0.0105f, 0.10f);

    constexpr float bossBarCenterX = 0.0f;
    constexpr float bossBarY = 0.84f;
    constexpr float bossBarMaxScaleX = 0.50f;
    mBossHpFrame = createUIQuad("UI_BossHpFrameMat", bossBarMaxScaleX + 0.038f, 0.042f, bossBarCenterX, bossBarY, 0.105f);
    mBossHpBack = createUIQuad("UI_BossHpBackMat", bossBarMaxScaleX, 0.021f, bossBarCenterX, bossBarY, 0.100f);
    mBossHpDelay = createUIQuad("UI_BossHpDelayMat", bossBarMaxScaleX, 0.021f, bossBarCenterX, bossBarY, 0.095f);
    mBossHpFill = createUIQuad("UI_BossHpFillMat", bossBarMaxScaleX, 0.021f, bossBarCenterX, bossBarY, 0.090f);
    mBossHpGloss = createUIQuad("UI_BossHpGlossMat", bossBarMaxScaleX, 0.006f, bossBarCenterX, bossBarY + 0.010f, 0.085f);
    mBossHpLeftCap = createUIQuad("UI_BossHpCapMat", 0.012f, 0.052f, -bossBarMaxScaleX - 0.038f, bossBarY, 0.080f, 0.30f);
    mBossHpRightCap = createUIQuad("UI_BossHpCapMat", 0.012f, 0.052f, bossBarMaxScaleX + 0.038f, bossBarY, 0.080f, -0.30f);
    const float textX = bossBarMaxScaleX - 0.086f;
    const float textY = bossBarY;
    mBossHpXSegments[0] = createUIQuad("UI_BossHpTextMat", 0.0020f, 0.019f, textX, textY, 0.070f, 0.62f);
    mBossHpXSegments[1] = createUIQuad("UI_BossHpTextMat", 0.0020f, 0.019f, textX, textY, 0.070f, -0.62f);

    auto createDigitSegments = [&](std::array<GameObject*, 7>& segments, float centerX)
        {
            constexpr float digitY = 0.84f;
            constexpr float segmentZ = 0.070f;
            constexpr float halfWidth = 0.0085f;
            constexpr float halfThickness = 0.0018f;
            constexpr float halfHeight = 0.0080f;
            constexpr float xOffset = 0.0085f;
            constexpr float yOffset = 0.0100f;

            segments[0] = createUIQuad("UI_BossHpTextMat", halfWidth, halfThickness, centerX, digitY + yOffset, segmentZ);
            segments[1] = createUIQuad("UI_BossHpTextMat", halfThickness, halfHeight, centerX + xOffset, digitY + yOffset * 0.5f, segmentZ);
            segments[2] = createUIQuad("UI_BossHpTextMat", halfThickness, halfHeight, centerX + xOffset, digitY - yOffset * 0.5f, segmentZ);
            segments[3] = createUIQuad("UI_BossHpTextMat", halfWidth, halfThickness, centerX, digitY - yOffset, segmentZ);
            segments[4] = createUIQuad("UI_BossHpTextMat", halfThickness, halfHeight, centerX - xOffset, digitY - yOffset * 0.5f, segmentZ);
            segments[5] = createUIQuad("UI_BossHpTextMat", halfThickness, halfHeight, centerX - xOffset, digitY + yOffset * 0.5f, segmentZ);
            segments[6] = createUIQuad("UI_BossHpTextMat", halfWidth, halfThickness, centerX, digitY, segmentZ);
        };

    createDigitSegments(mBossHpTensSegments, textX + 0.030f);
    createDigitSegments(mBossHpOnesSegments, textX + 0.055f);
    HideBossHealthBar();

    const float lanternRadius = 0.095f;
    createUIMeshObject("UI_LanternRingBackMat", "uiLanternRingGeo", "ring", lanternRadius * lanternAspectFix, lanternRadius, lanternCenterX, lanternCenterY, 0.105f);
    createUIMeshObject("UI_LanternRingMat", "uiLanternRingGeo", "ring", lanternRadius * lanternAspectFix, lanternRadius, lanternCenterX, lanternCenterY, 0.095f, &mLanternRingFillRitem);
    if (mLanternRingFillRitem)
    {
        mLanternRingFillRitem->IndexCount = 0;
        mLanternRingFillRitem->NumFramesDirty = gNumFrameResources;
    }
    mLanternOrbGlow = createUIMeshObject("UI_LanternOrbGlowMat", "uiLanternDiskGeo", "disk", 0.068f * lanternAspectFix, 0.068f, lanternCenterX, lanternCenterY, 0.09f);
    createUIMeshObject("UI_LanternOrbCoreMat", "uiLanternDiskGeo", "disk", 0.033f * lanternAspectFix, 0.033f, lanternCenterX, lanternCenterY, 0.085f);
    mLanternOrbCore = createUIQuad("UI_LanternIconMat", 0.057f * lanternAspectFix, 0.057f, lanternCenterX, lanternCenterY, 0.075f);

    for (int i = 1; i < 5; ++i)
    {
        const float tickX = barLeftEdgeX + (hpMaxScaleX * 2.0f * i / 5.0f);
        createUIQuad("UI_HudInnerFrameMat", 0.0024f, 0.024f, tickX, hpY, 0.095f);
    }

    for (int i = 1; i < 4; ++i)
    {
        const float tickX = barLeftEdgeX + (mpMaxScaleX * 2.0f * i / 4.0f);
        createUIQuad("UI_HudInnerFrameMat", 0.002f, 0.019f, tickX, mpY, 0.095f);
    }

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
    if (isLanternFull)
    {
        mLanternGlowTime += kUiFrameDelta;
    }
    else
    {
        mLanternGlowTime = 0.0f;
    }

    auto updateBar = [](GameObject* bar, float ratio, float maxScaleX, float scaleY, float leftEdgeX, float y, float z)
        {
            if (!bar) return;

            const float currentScale = maxScaleX * (std::max)(ratio, 0.0f);
            bar->SetScale(currentScale, scaleY, 1.0f);
            bar->SetPosition(leftEdgeX + currentScale, y, z);
        };

    const float barLeftEdgeX = -0.985f;
    const float hpMaxScaleX = 0.24f;
    const float mpMaxScaleX = 0.215f;
    const float hpY = 0.91f;
    const float mpY = 0.845f;

    updateBar(mHpBarDelay, mHpDelayRatio, hpMaxScaleX, 0.026f, barLeftEdgeX, hpY, 0.12f);
    updateBar(mHpBarFill, hpRatio, hpMaxScaleX, 0.026f, barLeftEdgeX, hpY, 0.11f);
    updateBar(mHpBarGloss, hpRatio, hpMaxScaleX, 0.006f, barLeftEdgeX, hpY + 0.013f, 0.10f);

    updateBar(mMpBarDelay, mMpDelayRatio, mpMaxScaleX, 0.021f, barLeftEdgeX, mpY, 0.12f);
    updateBar(mMpBarFill, mpRatio, mpMaxScaleX, 0.021f, barLeftEdgeX, mpY, 0.11f);
    updateBar(mMpBarGloss, mpRatio, mpMaxScaleX, 0.0045f, barLeftEdgeX, mpY + 0.0105f, 0.10f);

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
    if (mLanternRingMat)
    {
        mLanternRingMat->DiffuseAlbedo = isLanternFull
            ? DirectX::XMFLOAT4(0.35f + glowPulse * 0.25f, 1.0f, 0.52f + glowPulse * 0.25f, 1.0f)
            : DirectX::XMFLOAT4(0.08f, 0.94f, 0.38f, 1.0f);
        mLanternRingMat->NumFramesDirty = gNumFrameResources;
    }
    if (mLanternGlowMat)
    {
        mLanternGlowMat->DiffuseAlbedo = isLanternFull
            ? DirectX::XMFLOAT4(0.28f, 1.0f, 0.48f, 0.58f + glowPulse * 0.28f)
            : DirectX::XMFLOAT4(0.14f, 0.95f, 0.42f, 0.34f);
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
        const float glowScale = 0.068f + (isLanternFull ? glowPulse * 0.018f : 0.0f);
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

    constexpr int kBossHpLayerCount = 20;
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
    setVisible(mBossHpLeftCap, true);
    setVisible(mBossHpRightCap, true);
    for (GameObject* segment : mBossHpXSegments)
    {
        setVisible(segment, true);
        if (segment != nullptr)
        {
            segment->Update();
        }
    }

    auto updateDigit = [&](std::array<GameObject*, 7>& segments, int digit)
        {
            const bool showDigit = digit >= 0 && digit <= 9;
            for (int i = 0; i < 7; ++i)
            {
                const bool visible = showDigit && kDigitSegments[digit][i];
                setVisible(segments[i], visible);
                if (segments[i] != nullptr)
                {
                    segments[i]->Update();
                }
            }
        };

    updateDigit(mBossHpTensSegments, visibleLayer >= 10 ? visibleLayer / 10 : -1);
    updateDigit(mBossHpOnesSegments, visibleLayer % 10);

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

    constexpr float bossBarMaxScaleX = 0.50f;
    constexpr float bossBarLeftEdgeX = -bossBarMaxScaleX;
    constexpr float bossBarY = 0.84f;

    updateBar(mBossHpDelay, mBossHpDelayRatio, bossBarMaxScaleX, 0.021f, bossBarLeftEdgeX, bossBarY, 0.095f);
    updateBar(mBossHpFill, layerRatio, bossBarMaxScaleX, 0.021f, bossBarLeftEdgeX, bossBarY, 0.090f);
    updateBar(mBossHpGloss, layerRatio, bossBarMaxScaleX, 0.006f, bossBarLeftEdgeX, bossBarY + 0.010f, 0.085f);

    if (mBossHpFrame) mBossHpFrame->Update();
    if (mBossHpBack) mBossHpBack->Update();
    if (mBossHpLeftCap) mBossHpLeftCap->Update();
    if (mBossHpRightCap) mBossHpRightCap->Update();
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

    for (GameObject* segment : mBossHpXSegments)
    {
        if (segment != nullptr && segment->Ritem != nullptr)
        {
            segment->Ritem->Visible = false;
            segment->Update();
        }
    }

    auto hideDigit = [](std::array<GameObject*, 7>& segments)
        {
            for (GameObject* segment : segments)
            {
                if (segment != nullptr && segment->Ritem != nullptr)
                {
                    segment->Ritem->Visible = false;
                    segment->Update();
                }
            }
        };

    hideDigit(mBossHpTensSegments);
    hideDigit(mBossHpOnesSegments);
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
