#include "Stage2Scene.h"
#include "CharacterVisualFactory.h"
#include "EclipseWalkerGame.h"
#include "Monster.h"
#include "NetworkManager.h"
#include "SkeletalAnimationComponent.h"
#include <ResourceUploadBatch.h>
#include <RenderTargetState.h>
#include <algorithm>
#include <cmath>
#include <exception>
#include <filesystem>
#include <sstream>
#include <Windows.h>

namespace
{
    struct MapMaterialBinding
    {
        std::string MaterialName;
        bool HideSubset = false;
    };

    constexpr float kStage2MapScale = 0.014f;
    constexpr float kStage2WorldScale = kStage2MapScale / 0.01f;
    const DirectX::XMFLOAT3 kStage2BossAnchorPosition = { -8.81673f, 6.01219f, 23.2462f };
    const DirectX::XMFLOAT3 kStage2BossSpawnPosition = { -8.81673f, 7.71219f, 23.2462f };
    const DirectX::XMFLOAT3 kStage2PlayerStartPosition = { -4.81673f, 6.01219f, 23.2462f };
    constexpr int kBossHpLayerCount = 20;
    constexpr float kBossBarY = 0.84f;
    constexpr float kBossBarMaxScaleX = 0.38f;

    bool IsLanternUIClicked(EclipseWalkerGame* game)
    {
        if (game == nullptr)
        {
            return false;
        }

        POINT cursor{};
        if (!GetCursorPos(&cursor) || !ScreenToClient(game->GetMainWindowHandle(), &cursor))
        {
            return false;
        }

        RECT clientRect{};
        if (!GetClientRect(game->GetMainWindowHandle(), &clientRect))
        {
            return false;
        }

        const float clientWidth = static_cast<float>(clientRect.right - clientRect.left);
        const float clientHeight = static_cast<float>(clientRect.bottom - clientRect.top);
        if (clientWidth <= 0.0f || clientHeight <= 0.0f)
        {
            return false;
        }

        constexpr float lanternCenterNdcX = 0.88f;
        constexpr float lanternCenterNdcY = 0.0f;
        constexpr float lanternClickRadiusNdc = 0.18f;

        const float centerX = (lanternCenterNdcX + 1.0f) * 0.5f * clientWidth;
        const float centerY = (1.0f - lanternCenterNdcY) * 0.5f * clientHeight;
        const float radius = lanternClickRadiusNdc * 0.5f * clientHeight;

        const float dx = static_cast<float>(cursor.x) - centerX;
        const float dy = static_cast<float>(cursor.y) - centerY;
        return (dx * dx + dy * dy) <= (radius * radius);
    }
}

void Stage2Scene::TrackOwned(GameObject* object, RenderItem* renderItem)
{
    if (object) mOwnedObjects.push_back(object);
    if (renderItem) mOwnedRenderItems.push_back(renderItem);
}

void Stage2Scene::ReleaseOwnedObjects()
{
    auto& ritems = mGame->GetRitems();
    auto& objs = mGame->GetGameObjects();

    objs.erase(std::remove_if(objs.begin(), objs.end(),
        [&](const std::unique_ptr<GameObject>& object)
        {
            return std::find(mOwnedObjects.begin(), mOwnedObjects.end(), object.get()) != mOwnedObjects.end();
        }),
        objs.end());

    ritems.erase(std::remove_if(ritems.begin(), ritems.end(),
        [&](const std::unique_ptr<RenderItem>& renderItem)
        {
            return std::find(mOwnedRenderItems.begin(), mOwnedRenderItems.end(), renderItem.get()) != mOwnedRenderItems.end();
        }),
        ritems.end());

    mOwnedObjects.clear();
    mOwnedRenderItems.clear();
}

void Stage2Scene::LogPlayerPosition(const XMFLOAT3& position)
{
    std::ostringstream log;
    log << "[Debug][PlayerPos] x=" << position.x
        << " y=" << position.y
        << " z=" << position.z << "\n";
    OutputDebugStringA(log.str().c_str());
}

void Stage2Scene::UpdateIncomingDamageText(Player* player)
{
    if (player == nullptr)
    {
        mHasLastPlayerHpForDamageText = false;
        return;
    }

    const float currentHp = player->GetHP();
    if (!mHasLastPlayerHpForDamageText)
    {
        mLastPlayerHpForDamageText = currentHp;
        mHasLastPlayerHpForDamageText = true;
        return;
    }

    const float damage = mLastPlayerHpForDamageText - currentHp;
    if (damage > 0.01f)
    {
        DirectX::XMFLOAT3 textPosition = player->GetPosition();
        textPosition.y += Player::DefaultColliderHalfHeight * 0.85f;
        mDamageTextRenderer.SpawnIncoming(textPosition, damage);
    }

    mLastPlayerHpForDamageText = currentHp;
}

int Stage2Scene::CalculateBossHealthLayer(float currentHp, float maxHp) const
{
    if (maxHp <= 0.0f || currentHp <= 0.0f)
    {
        return 0;
    }

    const float clampedHp = (std::clamp)(currentHp, 0.0f, maxHp);
    const float hpPerLayer = maxHp / static_cast<float>(kBossHpLayerCount);
    return (std::clamp)(
        static_cast<int>(std::ceil(clampedHp / hpPerLayer)),
        1,
        kBossHpLayerCount);
}

void Stage2Scene::InitializeBossHealthText()
{
    auto* device = mGame != nullptr ? mGame->GetDevice() : nullptr;
    auto* cmdQueue = mGame != nullptr ? mGame->GetCommandQueue() : nullptr;
    if (device == nullptr || cmdQueue == nullptr)
    {
        return;
    }

    try
    {
        if (!mBossHealthTextHeap)
        {
            mBossHealthTextHeap = std::make_unique<DirectX::DescriptorHeap>(
                device,
                D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
                D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
                1);
        }

        if (!mBossHealthTextFont || !mBossHealthTextBatch)
        {
            DirectX::ResourceUploadBatch resourceUpload(device);
            resourceUpload.Begin();

            if (!mBossHealthTextFont)
            {
                mBossHealthTextFont = std::make_unique<DirectX::SpriteFont>(
                    device,
                    resourceUpload,
                    L"Textures/chat_korean.spritefont",
                    mBossHealthTextHeap->GetCpuHandle(0),
                    mBossHealthTextHeap->GetGpuHandle(0));
            }

            if (!mBossHealthTextBatch)
            {
                DirectX::RenderTargetState rtState(DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_D24_UNORM_S8_UINT);
                DirectX::SpriteBatchPipelineStateDescription pd(rtState);
                mBossHealthTextBatch = std::make_unique<DirectX::SpriteBatch>(device, resourceUpload, pd);
            }

            auto uploadResourcesFinished = resourceUpload.End(cmdQueue);
            uploadResourcesFinished.wait();
        }
    }
    catch (const std::exception& e)
    {
        std::string log = "[Stage2BossUI] Failed to initialize boss HP font: ";
        log += e.what();
        log += "\n";
        OutputDebugStringA(log.c_str());

        mBossHealthTextFont.reset();
        mBossHealthTextBatch.reset();
        mBossHealthTextHeap.reset();
    }
}

void Stage2Scene::DrawBossHealthText()
{
    if (!mShowBossHealthText ||
        mBossHealthTextLayer <= 0 ||
        !mBossHealthTextFont ||
        !mBossHealthTextBatch ||
        !mBossHealthTextHeap)
    {
        return;
    }

    auto* cmdList = mGame != nullptr ? mGame->GetCommandList() : nullptr;
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
        ID3D12DescriptorHeap* heaps[] = { mBossHealthTextHeap->Heap() };
        cmdList->SetDescriptorHeaps(1, heaps);

        mBossHealthTextBatch->SetViewport(viewport);
        mBossHealthTextBatch->Begin(cmdList);

        const std::wstring label = L"x" + std::to_wstring(mBossHealthTextLayer);
        constexpr float textScale = 0.42f;
        constexpr float rightPadding = 30.0f;

        const DirectX::XMVECTOR textSize = mBossHealthTextFont->MeasureString(label.c_str());
        const float textWidth = DirectX::XMVectorGetX(textSize) * textScale;
        const float textHeight = DirectX::XMVectorGetY(textSize) * textScale;
        const float barRightPixel = (kBossBarMaxScaleX + 1.0f) * 0.5f * viewport.Width;
        const float barCenterYPixel = (1.0f - kBossBarY) * 0.5f * viewport.Height;
        const DirectX::XMFLOAT2 textPos(
            barRightPixel - textWidth - rightPadding,
            barCenterYPixel - textHeight * 0.5f - 1.0f);

        const DirectX::XMVECTORF32 shadowColor = { 0.0f, 0.0f, 0.0f, 0.72f };
        const DirectX::XMVECTORF32 textColor = { 1.0f, 0.92f, 0.48f, 1.0f };

        mBossHealthTextFont->DrawString(
            mBossHealthTextBatch.get(),
            label.c_str(),
            DirectX::XMFLOAT2(textPos.x + 1.0f, textPos.y + 1.0f),
            shadowColor,
            0.0f,
            DirectX::XMFLOAT2(0.0f, 0.0f),
            textScale);
        mBossHealthTextFont->DrawString(
            mBossHealthTextBatch.get(),
            label.c_str(),
            textPos,
            textColor,
            0.0f,
            DirectX::XMFLOAT2(0.0f, 0.0f),
            textScale);

        mBossHealthTextBatch->End();
    }
    catch (const std::exception& e)
    {
        std::string log = "[Stage2BossUI] Failed to draw boss HP font: ";
        log += e.what();
        log += "\n";
        OutputDebugStringA(log.c_str());
        mShowBossHealthText = false;
    }
}

void Stage2Scene::BuildBoss()
{
    auto* res = mGame->GetResources();
    auto* device = mGame->GetDevice();
    auto* cmdList = mGame->GetCommandList();
    if (res == nullptr || device == nullptr || cmdList == nullptr)
    {
        return;
    }

    auto bossRitem = std::make_unique<RenderItem>();
    bossRitem->ObjCBIndex = static_cast<UINT>(mGame->GetRitems().size());

    auto boss = std::make_unique<Monster>(MonsterType::STAGE2_BOSS);
    boss->Initialize(bossRitem.get(), kStage2BossSpawnPosition);

    CharacterVisualSpec visualSpec;
    visualSpec.UseSkinned = true;
    visualSpec.ModelPath = "Models/Skeleton/Model/Skeleton.fbx";
    visualSpec.DefaultClipName = "";
    visualSpec.LoadModelAnimations = false;
    visualSpec.AdditionalAnimationClips.push_back({ "Models/Skeleton/Animation/IDLE.fbx", "SkeletonIdle" });
    visualSpec.AdditionalAnimationClips.push_back({ "Models/Skeleton/Animation/Damage.fbx", "SkeletonDamage" });
    visualSpec.AdditionalAnimationClips.push_back({ "Models/Skeleton/Animation/Death.fbx", "SkeletonDeath" });
    visualSpec.GeometryName = "stage2BossSkeletonGeo";
    visualSpec.MaterialName = "Stage2BossMat";
    visualSpec.DiffuseTextureName = "Stage2BossTex";
    visualSpec.DiffuseTexturePath = L"Textures/Warrior Skeleton Classic.dds";
    visualSpec.DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
    visualSpec.FresnelR0 = { 0.05f, 0.05f, 0.05f };
    visualSpec.Roughness = 0.72f;
    visualSpec.IsToon = true;
    visualSpec.OutlineThickness = 0.02f;
    visualSpec.OutlineColor = { 0.04f, 0.03f, 0.03f, 1.0f };
    visualSpec.FallbackMaterialName = "MonsterRed";
    visualSpec.FallbackScale = { 0.8f, 2.1f, 0.8f };
    visualSpec.SpawnPosition = kStage2BossSpawnPosition;
    visualSpec.UseActorOrigin = true;
    visualSpec.OriginToFloor = boss->GetColliderHalfHeight();
    visualSpec.RotationOffset = { 0.0f, DirectX::XM_PI, 0.0f };
    visualSpec.TargetHeight = boss->GetColliderHalfHeight() * 2.0f;

    if (!CharacterVisualFactory::ApplyVisual(
        boss.get(),
        bossRitem.get(),
        device,
        cmdList,
        res,
        visualSpec))
    {
        OutputDebugStringA("[Stage2Boss] Failed to build boss visual\n");
        return;
    }

    const float dx = kStage2PlayerStartPosition.x - kStage2BossAnchorPosition.x;
    const float dz = kStage2PlayerStartPosition.z - kStage2BossAnchorPosition.z;
    boss->SetRotation(0.0f, std::atan2(dx, dz), 0.0f);
    boss->GameObject::Update();

    if (auto* animation = boss->GetSkeletalAnimation())
    {
        animation->Play("SkeletonIdle");
    }

    mBoss = boss.get();
    mMonsterPtrs.push_back(mBoss);
    TrackOwned(mBoss, bossRitem.get());
    mGame->GetRitems().push_back(std::move(bossRitem));
    mGame->GetGameObjects().push_back(std::move(boss));

    OutputDebugStringA("[Stage2Boss] Temporary boss spawned near debug position\n");
}

void Stage2Scene::Enter()
{
    OutputDebugStringA("\n[Stage 2 Scene] 진입: 두 번째 스테이지 로딩!\n");
    mDebugPositionPrintKeyPressed = false;
    mDebugOutgoingDamageKeyPressed = false;
    mDebugIncomingDamageKeyPressed = false;
    mLanternUiClickPressed = false;
    mHasLastPlayerHpForDamageText = false;
    mCombatSystem.Reset();
    mMonsterPtrs.clear();

    // 공통 리소스(셰이더, UI 등) 로드
    mGame->LoadSharedGameResources();

    auto res = mGame->GetResources();
    auto dev = mGame->GetDevice();
    auto cmd = mGame->GetCommandList();
    auto& ritems = mGame->GetRitems();
    auto& objs = mGame->GetGameObjects();

    auto LoadStage2Textures = [&](const std::vector<std::string>& textureNames)
    {
        for (const auto& originName : textureNames)
        {
            if (originName.empty()) continue;

            const std::string baseName = originName.substr(0, originName.find_last_of('.'));
            auto TryLoadTexture = [&](const std::string& suffix)
            {
                const std::string textureName = baseName + suffix;
                const std::wstring texturePath =
                    L"Models/Stage2Map/Textures/" + std::wstring(textureName.begin(), textureName.end()) + L".dds";

                if (std::filesystem::exists(texturePath))
                {
                    res->LoadTexture(textureName, texturePath);
                }
            };

            TryLoadTexture("");
            TryLoadTexture("_normal");
            TryLoadTexture("_emissive");
            TryLoadTexture("_metallic");
        }
    };

    auto BuildStage2Materials = [&](const std::vector<std::string>& textureNames)
    {
        std::vector<MapMaterialBinding> materialBindings(textureNames.size());

        for (size_t i = 0; i < textureNames.size(); ++i)
        {
            const std::string& originName = textureNames[i];
            const std::string baseName = originName.empty() ? "" : originName.substr(0, originName.find_last_of('.'));
            const bool shouldHideSubset = baseName.empty() || (res->GetTexture(baseName) == nullptr);

            std::string diffuseName = baseName;
            std::string normalName = baseName.empty() ? "" : baseName + "_normal";
            std::string emissiveName = baseName.empty() ? "" : baseName + "_emissive";
            std::string metallicName = baseName.empty() ? "" : baseName + "_metallic";

            if (shouldHideSubset)
            {
                materialBindings[i].HideSubset = true;
                continue;
            }
            if (!normalName.empty() && res->GetTexture(normalName) == nullptr)
            {
                normalName.clear();
            }
            if (!emissiveName.empty() && res->GetTexture(emissiveName) == nullptr)
            {
                emissiveName.clear();
            }
            if (!metallicName.empty() && res->GetTexture(metallicName) == nullptr)
            {
                metallicName.clear();
            }

            const std::string materialName = "Stage2_Mat_" + std::to_string(i);
            materialBindings[i].MaterialName = materialName;

            if (res->GetMaterial(materialName) == nullptr)
            {
                res->CreateMaterial(
                    materialName,
                    static_cast<int>(res->mMaterials.size()),
                    diffuseName,
                    normalName,
                    emissiveName,
                    metallicName,
                    { 1.0f, 1.0f, 1.0f, 1.0f },
                    { 0.05f, 0.05f, 0.05f },
                    0.8f);
            }

            if (Material* material = res->GetMaterial(materialName))
            {
                material->DiffuseMapName = diffuseName;
                material->NormalMapName = normalName;
                material->EmissiveMapName = emissiveName;
                material->MetallicMapName = metallicName;
                material->DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
                material->FresnelR0 = { 0.05f, 0.05f, 0.05f };
                material->Roughness = 0.8f;
                material->IsToon = 0;
                material->IsTransparent = 0;
                material->NumFramesDirty = gNumFrameResources;
            }
        }

        return materialBindings;
    };

    const auto stage2TextureNames = ModelLoader::LoadTextureNames("Models/Stage2Map/Stage2Map.fbx");
    LoadStage2Textures(stage2TextureNames);
    res->LoadTexture("sky", L"Textures/sky.dds");
    const auto stage2MaterialBindings = BuildStage2Materials(stage2TextureNames);

    if (res->GetMaterial("MapFallbackMat") == nullptr)
    {
        res->CreateMaterial(
            "MapFallbackMat",
            static_cast<int>(res->mMaterials.size()),
            "white",
            "",
            "",
            "",
            { 1.0f, 1.0f, 1.0f, 1.0f },
            { 0.05f, 0.05f, 0.05f },
            0.8f);
    }

    if (Material* fallbackMat = res->GetMaterial("MapFallbackMat"))
    {
        fallbackMat->NumFramesDirty = gNumFrameResources;
    }

    if (res->GetMaterial("Stage2AbyssCoverMat") == nullptr)
    {
        res->CreateMaterial(
            "Stage2AbyssCoverMat",
            static_cast<int>(res->mMaterials.size()),
            "white",
            "",
            "",
            "",
            { 0.16f, 0.06f, 0.07f, 1.0f },
            { 0.02f, 0.02f, 0.02f },
            1.0f);
    }

    if (Material* abyssCoverMat = res->GetMaterial("Stage2AbyssCoverMat"))
    {
        abyssCoverMat->IsToon = 0;
        abyssCoverMat->IsTransparent = 0;
        abyssCoverMat->NumFramesDirty = gNumFrameResources;
    }

    if (res->GetMaterial("Stage2AbyssFogMat") == nullptr)
    {
        res->CreateMaterial(
            "Stage2AbyssFogMat",
            static_cast<int>(res->mMaterials.size()),
            "white",
            "",
            "",
            "",
            { 0.10f, 0.09f, 0.16f, 0.16f },
            { 0.02f, 0.02f, 0.02f },
            1.0f);
    }

    if (Material* abyssFogMat = res->GetMaterial("Stage2AbyssFogMat"))
    {
        abyssFogMat->IsToon = 0;
        abyssFogMat->IsTransparent = 2;
        abyssFogMat->NumFramesDirty = gNumFrameResources;
    }

    auto CreateMapEnv = [&](const std::string& fbxPath, const std::string& geoName, bool isVisible) {
        MapMeshData mapData;
        ModelLoader::Load(fbxPath, mapData);
        auto mapGeo = std::make_unique<MeshGeometry>();
        mapGeo->Name = geoName;

        const UINT vbByteSize = (UINT)mapData.Vertices.size() * sizeof(Vertex);
        const UINT ibByteSize = (UINT)mapData.Indices.size() * sizeof(std::uint32_t);

        D3DCreateBlob(vbByteSize, &mapGeo->VertexBufferCPU); CopyMemory(mapGeo->VertexBufferCPU->GetBufferPointer(), mapData.Vertices.data(), vbByteSize);
        D3DCreateBlob(ibByteSize, &mapGeo->IndexBufferCPU); CopyMemory(mapGeo->IndexBufferCPU->GetBufferPointer(), mapData.Indices.data(), ibByteSize);
        mapGeo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(dev, cmd, mapData.Vertices.data(), vbByteSize, mapGeo->VertexBufferUploader);
        mapGeo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(dev, cmd, mapData.Indices.data(), ibByteSize, mapGeo->IndexBufferUploader);
        mapGeo->VertexByteStride = sizeof(Vertex); mapGeo->VertexBufferByteSize = vbByteSize;
        mapGeo->IndexFormat = DXGI_FORMAT_R32_UINT; mapGeo->IndexBufferByteSize = ibByteSize;

        for (const auto& subset : mapData.Subsets) {
            SubmeshGeometry submesh; submesh.IndexCount = subset.IndexCount; submesh.StartIndexLocation = subset.IndexStart; submesh.BaseVertexLocation = 0;
            mapGeo->DrawArgs["subset_" + std::to_string(subset.Id)] = submesh;
        }
        res->mGeometries[mapGeo->Name] = std::move(mapGeo);

        for (const auto& subset : mapData.Subsets) {
            if (subset.MaterialIndex >= stage2MaterialBindings.size())
            {
                continue;
            }

            const auto& materialBinding = stage2MaterialBindings[subset.MaterialIndex];
            if (materialBinding.HideSubset)
            {
                std::ostringstream hiddenLog;
                hiddenLog << "[Stage2Scene] Hidden subset with missing diffuse texture: "
                    << subset.Name << " (material index " << subset.MaterialIndex << ")\n";
                OutputDebugStringA(hiddenLog.str().c_str());
                continue;
            }

            auto ritem = std::make_unique<RenderItem>();
            ritem->World = MathHelper::Identity4x4();
            ritem->TexTransform = MathHelper::Identity4x4();
            ritem->Geo = res->mGeometries[geoName].get();
            std::string subsetName = "subset_" + std::to_string(subset.Id);
            ritem->IndexCount = ritem->Geo->DrawArgs[subsetName].IndexCount;
            ritem->BaseVertexLocation = ritem->Geo->DrawArgs[subsetName].BaseVertexLocation;
            ritem->StartIndexLocation = ritem->Geo->DrawArgs[subsetName].StartIndexLocation;
            ritem->Mat = res->GetMaterial(materialBinding.MaterialName);

            ritem->ObjCBIndex = static_cast<UINT>(ritems.size());
            ritem->Visible = isVisible;

            auto mapObj = std::make_unique<GameObject>();
            mapObj->SetScale(kStage2MapScale, kStage2MapScale, kStage2MapScale);
            mapObj->Ritem = ritem.get(); mapObj->Update();
            TrackOwned(mapObj.get(), ritem.get());
            ritems.push_back(std::move(ritem)); objs.push_back(std::move(mapObj));
        }
        };
    CreateMapEnv("Models/Stage2Map/Stage2Map.fbx", "stage2MapGeo", true);

    auto AddAbyssCoverBox = [&](const XMFLOAT3& scale, const XMFLOAT3& position)
    {
        auto ritem = std::make_unique<RenderItem>();
        ritem->World = MathHelper::Identity4x4();
        XMStoreFloat4x4(
            &ritem->World,
            XMMatrixScaling(scale.x, scale.y, scale.z) * XMMatrixTranslation(position.x, position.y, position.z));
        ritem->TexTransform = MathHelper::Identity4x4();
        ritem->ObjCBIndex = static_cast<UINT>(ritems.size());
        ritem->Mat = res->GetMaterial("Stage2AbyssCoverMat");
        ritem->Geo = res->mGeometries["boxGeo"].get();
        ritem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        auto& abyssArgs = ritem->Geo->DrawArgs["box"];
        ritem->IndexCount = abyssArgs.IndexCount;
        ritem->StartIndexLocation = abyssArgs.StartIndexLocation;
        ritem->BaseVertexLocation = abyssArgs.BaseVertexLocation;
        ritem->Visible = true;
        TrackOwned(nullptr, ritem.get());
        ritems.push_back(std::move(ritem));
    };

    AddAbyssCoverBox(
        { 300.0f * kStage2WorldScale, 12.0f * kStage2WorldScale, 300.0f * kStage2WorldScale },
        { 0.0f, -18.0f * kStage2WorldScale, 0.0f });

    auto abyssFogRitem = std::make_unique<RenderItem>();
    abyssFogRitem->World = MathHelper::Identity4x4();
    XMStoreFloat4x4(
        &abyssFogRitem->World,
        XMMatrixScaling(210.0f * kStage2WorldScale, 18.0f * kStage2WorldScale, 210.0f * kStage2WorldScale) *
        XMMatrixTranslation(0.0f, -9.0f * kStage2WorldScale, 0.0f));
    abyssFogRitem->TexTransform = MathHelper::Identity4x4();
    abyssFogRitem->ObjCBIndex = static_cast<UINT>(ritems.size());
    abyssFogRitem->Mat = res->GetMaterial("Stage2AbyssFogMat");
    abyssFogRitem->Geo = res->mGeometries["boxGeo"].get();
    abyssFogRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    {
        auto& abyssFogArgs = abyssFogRitem->Geo->DrawArgs["box"];
        abyssFogRitem->IndexCount = abyssFogArgs.IndexCount;
        abyssFogRitem->StartIndexLocation = abyssFogArgs.StartIndexLocation;
        abyssFogRitem->BaseVertexLocation = abyssFogArgs.BaseVertexLocation;
    }
    abyssFogRitem->Visible = true;
    TrackOwned(nullptr, abyssFogRitem.get());
    ritems.push_back(std::move(abyssFogRitem));

    auto skyRitem = std::make_unique<RenderItem>();
    DirectX::XMStoreFloat4x4(&skyRitem->World, XMMatrixScaling(5000.0f, 5000.0f, 5000.0f));
    skyRitem->TexTransform = MathHelper::Identity4x4();
    skyRitem->ObjCBIndex = static_cast<UINT>(ritems.size());
    skyRitem->Mat = res->GetMaterial("MapFallbackMat");
    skyRitem->Geo = res->mGeometries["boxGeo"].get();
    skyRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    auto& skyDrawArgs = skyRitem->Geo->DrawArgs["box"];
    skyRitem->IndexCount = skyDrawArgs.IndexCount;
    skyRitem->StartIndexLocation = skyDrawArgs.StartIndexLocation;
    skyRitem->BaseVertexLocation = skyDrawArgs.BaseVertexLocation;
    skyRitem->Visible = true;
    skyRitem->IsSkybox = true;
    TrackOwned(nullptr, skyRitem.get());
    ritems.push_back(std::move(skyRitem));

    auto domainRi = std::make_unique<RenderItem>();
    domainRi->ObjCBIndex = static_cast<UINT>(ritems.size());
    domainRi->Geo = res->mGeometries["sphereGeo"].get();
    domainRi->Mat = res->GetMaterial("DomainMat");
    domainRi->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    auto& domainArgs = domainRi->Geo->DrawArgs["sphere"];
    domainRi->IndexCount = domainArgs.IndexCount;
    domainRi->StartIndexLocation = domainArgs.StartIndexLocation;
    domainRi->BaseVertexLocation = domainArgs.BaseVertexLocation;
    domainRi->Visible = false;

    auto domainObj = std::make_unique<GameObject>();
    domainObj->Ritem = domainRi.get();
    domainObj->SetScale(0.0f, 0.0f, 0.0f);
    mDomainBoundaryObj = domainObj.get();

    TrackOwned(domainObj.get(), domainRi.get());
    ritems.push_back(std::move(domainRi));
    objs.push_back(std::move(domainObj));
    mWorldStateController.Initialize(mDomainBoundaryObj, nullptr, nullptr);

    mMapSystem = std::make_unique<MapSystem>();

    mMapSystem->LoadFloorCollider("Models/Stage2Map/FloorCollider.fbx", kStage2MapScale);
    //mMapSystem->LoadWallCollider("Models/Stage2Map/Stage2Map.fbx", 0.01f);

    if (Player* player = mGame->GetPlayer())
    {
        player->SetPosition(
            kStage2PlayerStartPosition.x,
            kStage2PlayerStartPosition.y,
            kStage2PlayerStartPosition.z);
        mLanternSystem.ResetGauge(player);
    }

    BuildBoss();

    mGame->BuildDescriptorHeaps();
    mChatController.Initialize();
    mDamageTextRenderer.Initialize();
    mDamageTextRenderer.Reset();
    mCombatSystem.SetDamageTextCallback(
        [this](const DirectX::XMFLOAT3& worldPosition, float damage)
        {
            mDamageTextRenderer.SpawnOutgoing(worldPosition, damage);
        });
    InitializeBossHealthText();
}

void Stage2Scene::Exit()
{
    OutputDebugStringA("\n[Stage 2] 종료. 메모리 해제.\n");
    ReleaseOwnedObjects();
    mWorldStateController.Reset();
    mDomainBoundaryObj = nullptr;
    mBoss = nullptr;
    mMonsterPtrs.clear();
    mChatController.Reset();
    mDamageTextRenderer.Reset();
    mCombatSystem.Reset();
    mShowBossHealthText = false;
    mHasLastPlayerHpForDamageText = false;
    mBossHealthTextLayer = 0;
    mBossHealthTextFont.reset();
    mBossHealthTextBatch.reset();
    mBossHealthTextHeap.reset();
    if (auto* uiManager = mGame->GetUIManager())
    {
        uiManager->HideBossHealthBar();
    }
    mLanternUiClickPressed = false;
    gIsLanternUiInputActive = false;
    mDebugPositionPrintKeyPressed = false;
    mDebugOutgoingDamageKeyPressed = false;
    mDebugIncomingDamageKeyPressed = false;
}

void Stage2Scene::Update(const GameTimer& gt)
{
    const bool wasChatting = mChatController.IsChatting();
    mChatController.Update(gt);
    mDamageTextRenderer.Update(gt.DeltaTime());

    if (auto* uiManager = mGame->GetUIManager())
    {
        uiManager->SetChatBoxState(mChatController.IsChatting(), mChatController.HasMessages());
    }

    Player* pPlayer = mGame->GetPlayer();
    const bool hasFocus = (mGame != nullptr && GetForegroundWindow() == mGame->GetMainWindowHandle());

    if (NetworkManager::Get()->ConsumeWorldShiftSignal() && pPlayer != nullptr)
    {
        if (mWorldStateController.StartSyncedTransition(pPlayer))
        {
            mLanternSystem.ResetGauge(pPlayer);
        }
    }

    const bool lanternMouseDown = hasFocus && (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    const bool lanternUiPressed = lanternMouseDown && IsLanternUIClicked(mGame);
    gIsLanternUiInputActive = lanternUiPressed;
    if (!mChatController.IsChatting() &&
        pPlayer != nullptr &&
        lanternUiPressed &&
        !mLanternUiClickPressed &&
        !mWorldStateController.IsTransitionActive())
    {
        if (mLanternSystem.CanTriggerWorldShift(pPlayer))
        {
            OutputDebugStringA("[Lantern] Stage2 UI clicked. Sending world shift\n");
            NetworkManager::Get()->SendWorldShift();
        }
        else
        {
            OutputDebugStringA("[Lantern] Stage2 UI clicked but player cannot trigger world shift\n");
        }
    }
    mLanternUiClickPressed = lanternUiPressed;

    mWorldStateController.Update(gt, pPlayer, true);

    if (pPlayer)
    {  
        pPlayer->Update(gt, mMapSystem.get());
    }

    mCombatSystem.Update(gt, pPlayer, mMonsterPtrs);

    if (mBoss != nullptr && mBoss->GetState() != MonsterState::DIE)
    {
        mBoss->UpdateAnimationState(gt.DeltaTime());
    }

    bool shouldShowBossHealth = false;
    if (pPlayer != nullptr && mBoss != nullptr && mBoss->GetState() != MonsterState::DIE)
    {
        const DirectX::XMFLOAT3 playerPos = pPlayer->GetPosition();
        const float dx = playerPos.x - kStage2BossAnchorPosition.x;
        const float dz = playerPos.z - kStage2BossAnchorPosition.z;
        constexpr float bossAreaRadius = 12.0f;
        shouldShowBossHealth = (dx * dx + dz * dz) <= (bossAreaRadius * bossAreaRadius);
    }

    mShowBossHealthText = shouldShowBossHealth;
    mBossHealthTextLayer = shouldShowBossHealth ? CalculateBossHealthLayer(mBoss->GetHP(), mBoss->GetMaxHP()) : 0;

    if (auto* uiManager = mGame->GetUIManager())
    {
        if (shouldShowBossHealth)
        {
            uiManager->UpdateBossHealthBar(mBoss->GetHP(), mBoss->GetMaxHP());
        }
        else
        {
            uiManager->HideBossHealthBar();
        }
    }

    const bool debugOutgoingDamageKeyDown = hasFocus &&
        !mChatController.IsChatting() &&
        (GetAsyncKeyState(VK_F6) & 0x8000) != 0;
    if (debugOutgoingDamageKeyDown && !mDebugOutgoingDamageKeyPressed)
    {
        DirectX::XMFLOAT3 textPosition =
            (mBoss != nullptr) ? mBoss->GetPosition() :
            (pPlayer != nullptr ? pPlayer->GetPosition() : kStage2BossAnchorPosition);
        textPosition.y += (mBoss != nullptr ? mBoss->GetColliderHalfHeight() * 0.45f : Player::DefaultColliderHalfHeight * 0.85f);
        mDamageTextRenderer.SpawnOutgoing(textPosition, 123.0f);
        OutputDebugStringA("[DamageText][Debug] Spawn outgoing damage text\n");
    }
    mDebugOutgoingDamageKeyPressed = debugOutgoingDamageKeyDown;

    const bool debugIncomingDamageKeyDown = hasFocus &&
        !mChatController.IsChatting() &&
        (GetAsyncKeyState(VK_F7) & 0x8000) != 0;
    if (debugIncomingDamageKeyDown && !mDebugIncomingDamageKeyPressed)
    {
        DirectX::XMFLOAT3 textPosition =
            (pPlayer != nullptr) ? pPlayer->GetPosition() : kStage2PlayerStartPosition;
        textPosition.y += Player::DefaultColliderHalfHeight * 0.85f;
        mDamageTextRenderer.SpawnIncoming(textPosition, 77.0f);
        OutputDebugStringA("[DamageText][Debug] Spawn incoming damage text\n");
    }
    mDebugIncomingDamageKeyPressed = debugIncomingDamageKeyDown;

    const bool printPositionKeyDown = hasFocus && (GetAsyncKeyState(VK_RETURN) & 0x8000) != 0;
    if (!wasChatting && pPlayer != nullptr && printPositionKeyDown && !mDebugPositionPrintKeyPressed)
    {
        LogPlayerPosition(pPlayer->GetPosition());
    }
    mDebugPositionPrintKeyPressed = printPositionKeyDown;
    UpdateIncomingDamageText(pPlayer);
}

void Stage2Scene::Draw(const GameTimer& gt)
{
    UNREFERENCED_PARAMETER(gt);
    mDamageTextRenderer.Draw();
    DrawBossHealthText();
    mChatController.Draw();
}

void Stage2Scene::OnCharInput(WPARAM charCode)
{
    mChatController.OnCharInput(charCode);
}

void Stage2Scene::OnTextInput(const std::wstring& text)
{
    mChatController.OnTextInput(text);
}

void Stage2Scene::OnCompositionInput(const std::wstring& text, bool isFinal)
{
    mChatController.OnCompositionInput(text, isFinal);
}
