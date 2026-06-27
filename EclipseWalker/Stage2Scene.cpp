#include "Stage2Scene.h"
#include "CharacterVisualFactory.h"
#include "EclipseWalkerGame.h"
#include "Monster.h"
#include "NetworkManager.h"
#include "SkeletalAnimationComponent.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <mutex>
#include <sstream>
#include <Windows.h>

namespace
{
    struct Stage2MonsterSpawn
    {
        int Id;
        MonsterType Type;
        DirectX::XMFLOAT3 Position;
    };

    struct MapMaterialBinding
    {
        std::string MaterialName;
        bool HideSubset = false;
    };

    constexpr float kStage2MapScale = 0.014f;
    constexpr float kStage2WorldScale = kStage2MapScale / 0.01f;
    constexpr float kRespawnOverlayDelaySeconds = 5.0f;
    constexpr int kStage2SkeletonSpawnBaseId = 1101;

    float GetStage2MonsterColliderHalfHeight(MonsterType type)
    {
        switch (type)
        {
        case MonsterType::REAL_SKELETON_ARCHER:
        case MonsterType::REAL_SKELETON_SWORD:
            return 1.0f;
        default:
            return 0.5f;
        }
    }

    bool TryPlaceStage2MonsterOnFloor(
        MapSystem* mapSystem,
        MonsterType type,
        const DirectX::XMFLOAT3& candidatePosition,
        DirectX::XMFLOAT3& outPosition)
    {
        if (mapSystem == nullptr)
        {
            return false;
        }

        const float floorY = mapSystem->GetFloorHeight(
            candidatePosition.x,
            candidatePosition.z,
            candidatePosition.y + 50.0f,
            120.0f);
        if (floorY <= -9000.0f)
        {
            return false;
        }

        outPosition =
        {
            candidatePosition.x,
            floorY + GetStage2MonsterColliderHalfHeight(type),
            candidatePosition.z
        };
        return true;
    }

    const std::array<Stage2MonsterSpawn, 20> kStage2SkeletonSpawns =
    {
        Stage2MonsterSpawn{ kStage2SkeletonSpawnBaseId + 0,  MonsterType::REAL_SKELETON_SWORD,  { -9.40608f, -2.37823f,   9.0817f } },
        Stage2MonsterSpawn{ kStage2SkeletonSpawnBaseId + 1,  MonsterType::REAL_SKELETON_ARCHER, { -3.57432f, -2.37823f,   9.14398f } },
        Stage2MonsterSpawn{ kStage2SkeletonSpawnBaseId + 2,  MonsterType::REAL_SKELETON_SWORD,  { -5.49912f,  0.409166f, -1.35533f } },
        Stage2MonsterSpawn{ kStage2SkeletonSpawnBaseId + 3,  MonsterType::REAL_SKELETON_ARCHER, { -5.68869f,  0.409166f, -3.96669f } },
        Stage2MonsterSpawn{ kStage2SkeletonSpawnBaseId + 4,  MonsterType::REAL_SKELETON_SWORD,  { -9.66672f, -2.37823f,  -8.98436f } },
        Stage2MonsterSpawn{ kStage2SkeletonSpawnBaseId + 5,  MonsterType::REAL_SKELETON_ARCHER, { -13.9063f, -2.37823f, -14.2775f } },
        Stage2MonsterSpawn{ kStage2SkeletonSpawnBaseId + 6,  MonsterType::REAL_SKELETON_SWORD,  { -5.00478f, -2.37823f, -22.1984f } },
        Stage2MonsterSpawn{ kStage2SkeletonSpawnBaseId + 7,  MonsterType::REAL_SKELETON_ARCHER, { -2.36333f, -2.37823f, -20.7057f } },
        Stage2MonsterSpawn{ kStage2SkeletonSpawnBaseId + 8,  MonsterType::REAL_SKELETON_SWORD,  { 10.6695f,  -2.37823f, -22.9485f } },
        Stage2MonsterSpawn{ kStage2SkeletonSpawnBaseId + 9,  MonsterType::REAL_SKELETON_ARCHER, { 10.0559f,  -2.37823f, -14.2374f } },
        Stage2MonsterSpawn{ kStage2SkeletonSpawnBaseId + 10, MonsterType::REAL_SKELETON_SWORD,  { 10.2588f,  -0.992236f,  3.82154f } },
        Stage2MonsterSpawn{ kStage2SkeletonSpawnBaseId + 11, MonsterType::REAL_SKELETON_ARCHER, { 12.6068f,  -0.992236f,  3.3069f } },
        Stage2MonsterSpawn{ kStage2SkeletonSpawnBaseId + 12, MonsterType::REAL_SKELETON_SWORD,  { 19.1168f,  -2.38803f, -7.4035f } },
        Stage2MonsterSpawn{ kStage2SkeletonSpawnBaseId + 13, MonsterType::REAL_SKELETON_ARCHER, { 21.2676f,  -2.38803f, -7.87047f } },
        Stage2MonsterSpawn{ kStage2SkeletonSpawnBaseId + 14, MonsterType::REAL_SKELETON_SWORD,  { -0.77279f,  0.410567f, -6.81426f } },
        Stage2MonsterSpawn{ kStage2SkeletonSpawnBaseId + 15, MonsterType::REAL_SKELETON_ARCHER, { -1.34973f,  0.410567f, -3.53298f } },
        Stage2MonsterSpawn{ kStage2SkeletonSpawnBaseId + 16, MonsterType::REAL_SKELETON_SWORD,  { -1.36433f,  0.410567f,  0.608799f } },
        Stage2MonsterSpawn{ kStage2SkeletonSpawnBaseId + 17, MonsterType::REAL_SKELETON_ARCHER, { 3.58349f,   0.410567f,  2.19363f } },
        Stage2MonsterSpawn{ kStage2SkeletonSpawnBaseId + 18, MonsterType::REAL_SKELETON_SWORD,  { 3.86871f,   0.410567f, -0.590942f } },
        Stage2MonsterSpawn{ kStage2SkeletonSpawnBaseId + 19, MonsterType::REAL_SKELETON_ARCHER, { 4.76613f,   0.410567f, -4.00824f } },
    };

    std::string ToLowerCopy(const std::string& value)
    {
        std::string lower = value;
        std::transform(
            lower.begin(),
            lower.end(),
            lower.begin(),
            [](unsigned char c)
            {
                return static_cast<char>(std::tolower(c));
            });
        return lower;
    }

    bool IsLavaSubsetName(const std::string& subsetName)
    {
        return ToLowerCopy(subsetName).find("lava") != std::string::npos;
    }

    bool TryComputeSubsetBounds(
        const MapMeshData& mapData,
        const Subset& subset,
        DirectX::XMFLOAT3& outCenter,
        DirectX::XMFLOAT3& outExtents)
    {
        if (subset.IndexCount == 0 ||
            subset.IndexStart + subset.IndexCount > mapData.Indices.size())
        {
            return false;
        }

        const float maxFloat = (std::numeric_limits<float>::max)();
        DirectX::XMFLOAT3 minPos{ maxFloat, maxFloat, maxFloat };
        DirectX::XMFLOAT3 maxPos{ -maxFloat, -maxFloat, -maxFloat };

        for (UINT i = 0; i < subset.IndexCount; ++i)
        {
            const std::uint32_t vertexIndex = mapData.Indices[subset.IndexStart + i];
            if (vertexIndex >= mapData.Vertices.size())
            {
                continue;
            }

            const Vertex& vertex = mapData.Vertices[vertexIndex];
            minPos.x = (std::min)(minPos.x, vertex.Pos.x);
            minPos.y = (std::min)(minPos.y, vertex.Pos.y);
            minPos.z = (std::min)(minPos.z, vertex.Pos.z);
            maxPos.x = (std::max)(maxPos.x, vertex.Pos.x);
            maxPos.y = (std::max)(maxPos.y, vertex.Pos.y);
            maxPos.z = (std::max)(maxPos.z, vertex.Pos.z);
        }

        if (minPos.x > maxPos.x || minPos.y > maxPos.y || minPos.z > maxPos.z)
        {
            return false;
        }

        outCenter =
        {
            (minPos.x + maxPos.x) * 0.5f,
            (minPos.y + maxPos.y) * 0.5f,
            (minPos.z + maxPos.z) * 0.5f
        };
        outExtents =
        {
            (maxPos.x - minPos.x) * 0.5f,
            (maxPos.y - minPos.y) * 0.5f,
            (maxPos.z - minPos.z) * 0.5f
        };
        return true;
    }

    DirectX::XMFLOAT3 ScaleStage2Position(float x, float y, float z)
    {
        return { x * kStage2WorldScale, y * kStage2WorldScale, z * kStage2WorldScale };
    }

    constexpr float kStage2LanternAutoReturnDelaySeconds = 5.0f;

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

void Stage2Scene::UpdateDebugColliders(Player* player)
{
    std::vector<DebugColliderVisualizer::Target> targets;
    if (player != nullptr)
    {
        targets.push_back({
            player->GetPosition(),
            { Player::DefaultColliderHalfWidth, Player::DefaultColliderHalfHeight, Player::DefaultColliderHalfWidth },
            "DebugColliderPlayerMat",
            { 0.10f, 1.0f, 0.25f, 0.30f },
            true
            });
    }

    for (Monster* monster : mMonsterPtrs)
    {
        if (monster == nullptr || monster->GetState() == MonsterState::DIE)
        {
            continue;
        }

        const bool isBoss = monster->GetType() == MonsterType::STAGE2_BOSS;
        targets.push_back({
            monster->GetPosition(),
            monster->GetHurtboxExtents(),
            isBoss ? "DebugColliderBossMat" : "DebugColliderMonsterMat",
            isBoss ? DirectX::XMFLOAT4{ 1.0f, 0.12f, 0.06f, 0.32f } : DirectX::XMFLOAT4{ 1.0f, 0.82f, 0.08f, 0.26f },
            true
            });
    }

    mDebugColliderVisualizer.Update(
        mGame,
        targets,
        [this](GameObject* object, RenderItem* renderItem)
        {
            TrackOwned(object, renderItem);
        });
}

void Stage2Scene::QueueRespawn(const PKT_S_PLAYER_RESPAWN& respawn)
{
    mQueuedRespawnPacket = respawn;
    mHasQueuedRespawnPacket = true;
}

void Stage2Scene::ApplyQueuedRespawn(Player* player)
{
    if (player == nullptr)
    {
        return;
    }

    if (mHasQueuedRespawnPacket)
    {
        player->RespawnAt(
            mQueuedRespawnPacket.x,
            mQueuedRespawnPacket.y,
            mQueuedRespawnPacket.z,
            mQueuedRespawnPacket.remainHp);
    }
    else
    {
        const DirectX::XMFLOAT3 respawnPosition = Stage2BossController::GetPlayerStartPosition();
        player->RespawnAt(
            respawnPosition.x,
            respawnPosition.y,
            respawnPosition.z,
            static_cast<int>(player->GetMaxHP()));
    }

    player->UpdateCamera(mMapSystem.get());
    mRespawnOverlayActive = false;
    mRespawnButtonReady = false;
    mRespawnOverlayCountdown = 0.0f;
    mRespawnMousePressed = false;
    mWasPlayerDeadLastFrame = false;
    mHasQueuedRespawnPacket = false;
}

void Stage2Scene::UpdateRespawnOverlay(const GameTimer& gt, Player* player, bool hasFocus)
{
    auto* uiManager = mGame != nullptr ? mGame->GetUIManager() : nullptr;
    if (player == nullptr || uiManager == nullptr)
    {
        return;
    }

    const bool isDead = player->IsDead();
    if (isDead && !mWasPlayerDeadLastFrame)
    {
        mRespawnOverlayActive = true;
        mRespawnButtonReady = false;
        mRespawnOverlayCountdown = kRespawnOverlayDelaySeconds;
        mRespawnMousePressed = false;
    }

    if (mRespawnOverlayActive)
    {
        if (isDead)
        {
            mRespawnOverlayCountdown = (std::max)(0.0f, mRespawnOverlayCountdown - gt.DeltaTime());
            mRespawnButtonReady = mRespawnOverlayCountdown <= 0.0f;

            const bool mouseDown = hasFocus && (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
            const bool clickedButton =
                mouseDown &&
                !mRespawnMousePressed &&
                uiManager->IsRespawnButtonHovered();
            mRespawnMousePressed = mouseDown;

            if (clickedButton && mRespawnButtonReady)
            {
                ApplyQueuedRespawn(player);
            }
        }
        else
        {
            mRespawnOverlayActive = false;
            mRespawnButtonReady = false;
            mRespawnOverlayCountdown = 0.0f;
            mRespawnMousePressed = false;
            mHasQueuedRespawnPacket = false;
        }
    }

    uiManager->SetRespawnScreenState(
        mRespawnOverlayActive,
        mRespawnOverlayCountdown,
        mRespawnButtonReady);

    mWasPlayerDeadLastFrame = player->IsDead();
}

void Stage2Scene::Enter()
{
    OutputDebugStringA("\n[Stage 2 Scene] 진입: 두 번째 스테이지 로딩!\n");
    mDebugPositionPrintKeyPressed = false;
    mDebugOutgoingDamageKeyPressed = false;
    mDebugIncomingDamageKeyPressed = false;
    mLanternUiClickPressed = false;
    mHasLastPlayerHpForDamageText = false;
    mWasOtherWorldLastFrame = false;
    mStage2LanternAutoReturnPending = false;
    mStage2LanternAutoReturnElapsed = 0.0f;
    mRespawnOverlayActive = false;
    mRespawnButtonReady = false;
    mRespawnMousePressed = false;
    mWasPlayerDeadLastFrame = false;
    mHasQueuedRespawnPacket = false;
    mRespawnOverlayCountdown = 0.0f;
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
                const std::wstring textureFileName = std::wstring(textureName.begin(), textureName.end()) + L".dds";
                const std::wstring candidatePaths[] =
                {
                    L"Models/Stage2Map/Textures/" + textureFileName,
                    L"Models/Stage1Map/Textures/" + textureFileName,
                    L"Textures/" + textureFileName
                };

                for (const std::wstring& texturePath : candidatePaths)
                {
                    if (std::filesystem::exists(texturePath))
                    {
                        res->LoadTexture(textureName, texturePath);
                        return;
                    }
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
            const bool shouldHideSubset = baseName.empty();

            std::string diffuseName = baseName;
            std::string normalName = baseName.empty() ? "" : baseName + "_normal";
            std::string emissiveName = baseName.empty() ? "" : baseName + "_emissive";
            std::string metallicName = baseName.empty() ? "" : baseName + "_metallic";

            if (baseName == "Wood_metal_albedo")
            {
                normalName = "Wood_metal_normal";
                metallicName = "Wood_metal_metallic";
            }

            if (shouldHideSubset)
            {
                materialBindings[i].HideSubset = true;
                continue;
            }
            if (diffuseName.empty() || res->GetTexture(diffuseName) == nullptr)
            {
                diffuseName = "white";
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
                material->IsTransparent = (baseName == "Decals") ? 3 : 0;
                material->OutlineThickness = 0.0f;
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
        fallbackMat->IsToon = 0;
        fallbackMat->IsTransparent = 0;
        fallbackMat->OutlineThickness = 0.0f;
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
            const MapMaterialBinding* materialBinding = nullptr;
            if (subset.MaterialIndex < stage2MaterialBindings.size())
            {
                materialBinding = &stage2MaterialBindings[subset.MaterialIndex];
            }

            if (materialBinding != nullptr && materialBinding->HideSubset)
            {
                std::ostringstream hiddenLog;
                hiddenLog << "[Stage2Scene] Hidden subset with empty diffuse assignment: "
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
            if (materialBinding != nullptr)
            {
                ritem->Mat = res->GetMaterial(materialBinding->MaterialName);
            }

            if (ritem->Mat == nullptr)
            {
                std::ostringstream missingMatLog;
                missingMatLog << "[Stage2Scene] Missing material for subset "
                    << subset.Id << " name=" << subset.Name
                    << " materialIndex=" << subset.MaterialIndex
                    << ", using MapFallbackMat\n";
                OutputDebugStringA(missingMatLog.str().c_str());
                ritem->Mat = res->GetMaterial("MapFallbackMat");
            }

            ritem->ObjCBIndex = static_cast<UINT>(ritems.size());
            ritem->Visible = isVisible;

            auto mapObj = std::make_unique<GameObject>();
            mapObj->SetScale(kStage2MapScale, kStage2MapScale, kStage2MapScale);
            mapObj->Ritem = ritem.get(); mapObj->Update();
            TrackOwned(mapObj.get(), ritem.get());
            ritems.push_back(std::move(ritem)); objs.push_back(std::move(mapObj));
        }

        if (geoName == "stage2MapGeo")
        {
            const auto lavaBounds = ModelLoader::LoadNamedMeshBounds(
                "Models/Stage2Map/Stage2Map.fbx",
                "lava");

            int lavaEmitterCount = 0;
            for (const NamedMeshBounds& lavaBound : lavaBounds)
            {
                const DirectX::XMFLOAT3 scaledCenter =
                {
                    lavaBound.Center.x * kStage2MapScale,
                    lavaBound.Center.y * kStage2MapScale,
                    lavaBound.Center.z * kStage2MapScale
                };

                const float scaledExtentX = lavaBound.Extents.x * kStage2MapScale;
                const float scaledExtentZ = lavaBound.Extents.z * kStage2MapScale;
                const float footprintRadius = (std::max)(scaledExtentX, scaledExtentZ);
                const float innerRadius = (std::clamp)(footprintRadius * 0.45f, 1.5f, 3.5f);
                const float outerRadius = (std::clamp)(footprintRadius * 0.95f, innerRadius + 2.0f, 7.5f);

                mGame->RegisterLavaAudioEmitter(
                    scaledCenter.x,
                    scaledCenter.y,
                    scaledCenter.z,
                    innerRadius,
                    outerRadius,
                    0.10f);
                ++lavaEmitterCount;
            }

            if (lavaEmitterCount == 0)
            {
                std::ostringstream lavaMissingLog;
                lavaMissingLog
                    << "[Stage2Scene] Lava audio source not found from Stage2Map node scan. "
                    << "fallback emitter enabled.\n";
                OutputDebugStringA(lavaMissingLog.str().c_str());

                mGame->RegisterLavaAudioEmitter(0.0f, 0.0f, 0.0f, 4.0f, 9.0f, 0.10f);
                lavaEmitterCount = 1;
            }

            std::ostringstream lavaLog;
            lavaLog << "[Stage2Scene] Registered lava audio emitters: " << lavaEmitterCount << "\n";
            OutputDebugStringA(lavaLog.str().c_str());
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
    mMapSystem->LoadWallCollider("Models/Stage2Map/Stage2WallCollider.fbx", kStage2MapScale);

    for (const Stage2MonsterSpawn& spawn : kStage2SkeletonSpawns)
    {
        DirectX::XMFLOAT3 spawnPosition{};
        if (!TryPlaceStage2MonsterOnFloor(mMapSystem.get(), spawn.Type, spawn.Position, spawnPosition))
        {
            std::ostringstream log;
            log << "[Stage2] Monster spawn skipped: no floor under spawn id="
                << spawn.Id
                << " x=" << spawn.Position.x
                << " z=" << spawn.Position.z << "\n";
            OutputDebugStringA(log.str().c_str());
            continue;
        }

        auto ri = std::make_unique<RenderItem>();
        ri->ObjCBIndex = static_cast<UINT>(mGame->GetRitems().size());

        auto monster = std::make_unique<Monster>(spawn.Type);
        monster->SetNetworkId(spawn.Id);
        monster->Initialize(ri.get(), spawnPosition);

        CharacterVisualSpec visualSpec;
        visualSpec.UseSkinned = true;
        visualSpec.DefaultClipName = "";
        visualSpec.LoadModelAnimations = false;
        visualSpec.SpawnPosition = spawnPosition;
        visualSpec.FallbackMaterialName = "MonsterRed";
        visualSpec.FallbackScale = DirectX::XMFLOAT3{ 0.2f, 0.5f, 0.2f };

        if (spawn.Type == MonsterType::REAL_SKELETON_ARCHER)
        {
            visualSpec.ModelPath = "Models/Skeleton2/Model/Skeleton2.fbx";
            visualSpec.AdditionalAnimationClips.push_back({ "Models/Skeleton2/Animation/IDLE.fbx", "SkeletonIdle" });
            visualSpec.AdditionalAnimationClips.push_back({ "Models/Skeleton2/Animation/Walk.fbx", "SkeletonWalk" });
            visualSpec.AdditionalAnimationClips.push_back({ "Models/Skeleton2/Animation/Damage.fbx", "SkeletonDamage" });
            visualSpec.AdditionalAnimationClips.push_back({ "Models/Skeleton2/Animation/Death.fbx", "SkeletonDeath" });
            visualSpec.GeometryName = "stage2SkeletonArcherGeo_" + std::to_string(spawn.Id);
            visualSpec.MaterialName = "stage2SkeletonArcherMat_" + std::to_string(spawn.Id);
            visualSpec.DiffuseTextureName = "stage2SkeletonArcherTex_" + std::to_string(spawn.Id);
            visualSpec.DiffuseTexturePath = L"Textures/Archer Skeleton Classic.dds";
        }
        else
        {
            visualSpec.ModelPath = "Models/Skeleton/Model/Skeleton.fbx";
            visualSpec.AdditionalAnimationClips.push_back({ "Models/Skeleton/Animation/IDLE.fbx", "SkeletonIdle" });
            visualSpec.AdditionalAnimationClips.push_back({ "Models/Skeleton/Animation/Walk.fbx", "SkeletonWalk" });
            visualSpec.AdditionalAnimationClips.push_back({ "Models/Skeleton/Animation/Damage.fbx", "SkeletonDamage" });
            visualSpec.AdditionalAnimationClips.push_back({ "Models/Skeleton/Animation/Death.fbx", "SkeletonDeath" });
            visualSpec.GeometryName = "stage2SkeletonSwordGeo_" + std::to_string(spawn.Id);
            visualSpec.MaterialName = "stage2SkeletonSwordMat_" + std::to_string(spawn.Id);
            visualSpec.DiffuseTextureName = "stage2SkeletonSwordTex_" + std::to_string(spawn.Id);
            visualSpec.DiffuseTexturePath = L"Textures/Warrior Skeleton Classic.dds";
        }

        visualSpec.DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
        visualSpec.FresnelR0 = { 0.05f, 0.05f, 0.05f };
        visualSpec.Roughness = 0.75f;
        visualSpec.IsToon = false;
        visualSpec.OutlineThickness = 0.0f;
        visualSpec.OutlineColor = { 0.0f, 0.0f, 0.0f, 1.0f };
        visualSpec.TargetHeight = monster->GetColliderHalfHeight() * 2.0f;
        visualSpec.UseActorOrigin = true;
        visualSpec.OriginToFloor = monster->GetColliderHalfHeight();
        visualSpec.RotationOffset = { 0.0f, DirectX::XM_PI, 0.0f };

        CharacterVisualFactory::ApplyVisual(
            monster.get(),
            ri.get(),
            dev,
            cmd,
            res,
            visualSpec);

        if (auto* animation = monster->GetSkeletalAnimation())
        {
            animation->Play("SkeletonIdle");
        }

        monster->Update(GameTimer(), mGame->GetPlayer(), mMapSystem.get());
        TrackOwned(monster.get(), ri.get());
        mMonsterPtrs.push_back(monster.get());
        ritems.push_back(std::move(ri));
        objs.push_back(std::move(monster));
    }

    auto CreateTrackedStage2Fire = [&](const DirectX::XMFLOAT3& position, float scale)
    {
        const size_t objectStartIndex = objs.size();
        const size_t renderItemStartIndex = ritems.size();

        mGame->CreateFire(position.x, position.y, position.z, scale);

        for (size_t i = objectStartIndex; i < objs.size(); ++i)
        {
            TrackOwned(objs[i].get(), nullptr);
        }

        for (size_t i = renderItemStartIndex; i < ritems.size(); ++i)
        {
            TrackOwned(nullptr, ritems[i].get());
        }
    };

    const auto fire0 = ScaleStage2Position(-5.01f, 1.368f, -4.005f);
    const auto fire1 = ScaleStage2Position(-5.01f, 1.368f, 0.002f);
    CreateTrackedStage2Fire(fire0, 0.35f);
    CreateTrackedStage2Fire(fire1, 0.35f);

    if (Player* player = mGame->GetPlayer())
    {
        const DirectX::XMFLOAT3 playerStartPosition = Stage2BossController::GetPlayerStartPosition();
        player->SetPosition(
            playerStartPosition.x,
            playerStartPosition.y,
            playerStartPosition.z);
        FillStage2LanternGauge(player);
    }

    mBossController.Initialize(
        mGame,
        mMapSystem.get(),
        &mDamageTextRenderer,
        [this](GameObject* object, RenderItem* renderItem)
        {
            TrackOwned(object, renderItem);
        });
    if (Monster* boss = mBossController.GetBoss())
    {
        mMonsterPtrs.push_back(boss);
    }

    mGame->BuildDescriptorHeaps();
    mChatController.Initialize();
    mDamageTextRenderer.Initialize();
    mDamageTextRenderer.Reset();
    mSkillEffectManager.Initialize(
        mGame,
        [this](GameObject* object, RenderItem* renderItem)
        {
            TrackOwned(object, renderItem);
        });
    mCombatSystem.SetDamageTextCallback(
        [this](const DirectX::XMFLOAT3& worldPosition, float damage)
        {
            mDamageTextRenderer.SpawnOutgoing(worldPosition, damage);
        });
    mCombatSystem.SetBlockedHitCallback(
        [this](Monster* monster, const DirectX::XMFLOAT3& worldPosition)
        {
            if (monster == nullptr || monster != mBossController.GetBoss() || !mBossController.IsInvulnerable())
            {
                return false;
            }

            mDamageTextRenderer.SpawnImmune(worldPosition);
            return true;
        });
    mCombatSystem.SetSkillEffectManager(&mSkillEffectManager);
    mBossController.InitializeHealthText();
}

void Stage2Scene::Exit()
{
    OutputDebugStringA("\n[Stage 2] 종료. 메모리 해제.\n");
    mSkillEffectManager.Reset();
    ReleaseOwnedObjects();
    mGame->ResetLights();
    mDebugColliderVisualizer.Reset();
    mBossController.Reset();
    mWorldStateController.Reset();
    mDomainBoundaryObj = nullptr;
    mMonsterPtrs.clear();
    mChatController.Reset();
    mDamageTextRenderer.Reset();
    mCombatSystem.Reset();
    mHasLastPlayerHpForDamageText = false;
    mLanternUiClickPressed = false;
    gIsLanternUiInputActive = false;
    mDebugPositionPrintKeyPressed = false;
    mDebugOutgoingDamageKeyPressed = false;
    mDebugIncomingDamageKeyPressed = false;
    mWasOtherWorldLastFrame = false;
    mStage2LanternAutoReturnPending = false;
    mStage2LanternAutoReturnElapsed = 0.0f;
    mRespawnOverlayActive = false;
    mRespawnButtonReady = false;
    mRespawnMousePressed = false;
    mWasPlayerDeadLastFrame = false;
    mHasQueuedRespawnPacket = false;
    mRespawnOverlayCountdown = 0.0f;

    if (auto* uiManager = mGame->GetUIManager())
    {
        uiManager->SetRespawnScreenState(false, 0.0f, false);
    }
}

void Stage2Scene::Update(const GameTimer& gt)
{
    const bool wasChatting = mChatController.IsChatting();
    mChatController.Update(gt);
    mDamageTextRenderer.Update(gt.DeltaTime());

    Player* pPlayer = mGame->GetPlayer();
    const bool hasFocus = (mGame != nullptr && GetForegroundWindow() == mGame->GetMainWindowHandle());

    for (const PKT_S_PLAYER_HIT& playerHit : NetworkManager::Get()->PopPlayerHits())
    {
        if (pPlayer != nullptr && playerHit.playerId == NetworkManager::Get()->m_myPlayerId)
        {
            pPlayer->ApplyServerHit(playerHit.remainHp, playerHit.isDead);
        }
    }

    for (const PKT_S_PLAYER_RESPAWN& respawn : NetworkManager::Get()->PopPlayerRespawns())
    {
        if (pPlayer != nullptr && respawn.playerId == NetworkManager::Get()->m_myPlayerId)
        {
            QueueRespawn(respawn);
        }
    }

    UpdateRespawnOverlay(gt, pPlayer, hasFocus);

    if (auto* uiManager = mGame->GetUIManager())
    {
        const bool hideChatForRespawn = uiManager->IsRespawnScreenActive();
        uiManager->SetChatBoxState(
            !hideChatForRespawn && mChatController.IsChatting(),
            !hideChatForRespawn && mChatController.HasMessages());
    }

    for (const PKT_S_LANTERN_GAUGE& gaugeUpdate : NetworkManager::Get()->PopLanternGaugeUpdates())
    {
        if (pPlayer != nullptr)
        {
            pPlayer->GetLantern()->SetState(gaugeUpdate.gauge, gaugeUpdate.maxGauge, gaugeUpdate.level);
        }
    }

    FillStage2LanternGauge(pPlayer);

    for (const PKT_S_BOSS_PATTERN& bossPattern : NetworkManager::Get()->PopBossPatterns())
    {
        mBossController.ApplyServerPattern(
            bossPattern.patternType,
            bossPattern.x,
            bossPattern.y,
            bossPattern.z,
            bossPattern.radius,
            bossPattern.delay,
            bossPattern.damage,
            bossPattern.patternData);
    }

    {
        NetworkManager* network = NetworkManager::Get();
        std::lock_guard<std::mutex> lock(network->m_monsterMutex);

        const auto bossSyncIt = network->m_remoteMonsters.find(STAGE2_BOSS_MONSTER_ID);
        if (bossSyncIt != network->m_remoteMonsters.end())
        {
            const PKT_S_MONSTER_SYNC& bossSync = bossSyncIt->second;
            mBossController.ApplyServerSync(
                bossSync.state,
                bossSync.x,
                bossSync.y,
                bossSync.z,
                bossSync.rotY);
        }

        const auto bossHitIt = network->m_remoteMonsterHits.find(STAGE2_BOSS_MONSTER_ID);
        if (bossHitIt != network->m_remoteMonsterHits.end())
        {
            const PKT_S_MONSTER_HIT& bossHit = bossHitIt->second;
            mBossController.ApplyServerHit(bossHit.remainHp, bossHit.isDead);
            network->m_remoteMonsterHits.erase(bossHitIt);
        }
    }

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
        !pPlayer->IsDead() &&
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

    for (Monster* monster : mMonsterPtrs)
    {
        if (monster == nullptr || monster == mBossController.GetBoss())
        {
            continue;
        }

        monster->Update(gt, pPlayer, mMapSystem.get());
    }

    mCombatSystem.Update(gt, pPlayer, mMonsterPtrs, mMapSystem.get());
    if (auto* uiManager = mGame->GetUIManager())
    {
        const PlayerClass playerClass = pPlayer != nullptr ? pPlayer->GetClassType() : PlayerClass::None;
        uiManager->SetSkillCooldowns(
            mCombatSystem.GetSkillCooldownRemaining(1),
            mCombatSystem.GetSkillCooldownDuration(playerClass, 1),
            mCombatSystem.GetSkillCooldownRemaining(2),
            mCombatSystem.GetSkillCooldownDuration(playerClass, 2));
    }
    mSkillEffectManager.Update(gt.DeltaTime());
    mBossController.Update(gt, pPlayer, mWorldStateController.IsOtherWorld());
    UpdateStage2LanternAutoReturn(gt, pPlayer);
    FillStage2LanternGauge(pPlayer);

    const bool debugOutgoingDamageKeyDown = hasFocus &&
        !mChatController.IsChatting() &&
        (GetAsyncKeyState(VK_F6) & 0x8000) != 0;
    if (debugOutgoingDamageKeyDown && !mDebugOutgoingDamageKeyPressed)
    {
        Monster* boss = mBossController.GetBoss();
        const DirectX::XMFLOAT3 bossAnchorPosition = Stage2BossController::GetBossAnchorPosition();
        DirectX::XMFLOAT3 textPosition =
            (boss != nullptr) ? boss->GetPosition() :
            (pPlayer != nullptr ? pPlayer->GetPosition() : bossAnchorPosition);
        textPosition.y += (boss != nullptr ? boss->GetColliderHalfHeight() * 0.45f : Player::DefaultColliderHalfHeight * 0.85f);
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
            (pPlayer != nullptr) ? pPlayer->GetPosition() : Stage2BossController::GetPlayerStartPosition();
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
    UpdateDebugColliders(pPlayer);
}

void Stage2Scene::FillStage2LanternGauge(Player* player)
{
    if (player == nullptr || !player->CanUseLantern())
    {
        return;
    }

    Lantern* lantern = player->GetLantern();
    if (lantern == nullptr)
    {
        return;
    }

    const float maxGauge = (std::max)(1.0f, lantern->GetMaxGauge());
    lantern->SetState(maxGauge, maxGauge, lantern->GetLevel());
}

void Stage2Scene::UpdateStage2LanternAutoReturn(const GameTimer& gt, Player* player)
{
    const bool isOtherWorld = mWorldStateController.IsOtherWorld();

    if (isOtherWorld && !mWasOtherWorldLastFrame)
    {
        mStage2LanternAutoReturnPending = true;
        mStage2LanternAutoReturnElapsed = 0.0f;
        OutputDebugStringA("[Lantern][Stage2] Entered other world. Auto return armed for 2 seconds\n");
    }
    else if (!isOtherWorld)
    {
        mStage2LanternAutoReturnPending = false;
        mStage2LanternAutoReturnElapsed = 0.0f;
    }

    if (mStage2LanternAutoReturnPending)
    {
        mStage2LanternAutoReturnElapsed += gt.DeltaTime();

        if (mStage2LanternAutoReturnElapsed >= kStage2LanternAutoReturnDelaySeconds &&
            player != nullptr &&
            !mWorldStateController.IsTransitionActive())
        {
            if (mWorldStateController.StartSyncedTransition(player))
            {
                OutputDebugStringA("[Lantern][Stage2] Auto returning to real world after 2 seconds\n");
            }

            mStage2LanternAutoReturnPending = false;
            mStage2LanternAutoReturnElapsed = 0.0f;
        }
    }

    mWasOtherWorldLastFrame = isOtherWorld;
}

void Stage2Scene::Draw(const GameTimer& gt)
{
    UNREFERENCED_PARAMETER(gt);
    bool showRespawnOverlay = false;
    if (auto* uiManager = mGame->GetUIManager())
    {
        showRespawnOverlay = uiManager->IsRespawnScreenActive();
    }

    if (!showRespawnOverlay)
    {
        mDamageTextRenderer.Draw();
    }

    mBossController.Draw();
    if (auto* uiManager = mGame->GetUIManager())
    {
        uiManager->DrawCooldownOverlay();
    }
    if (!showRespawnOverlay)
    {
        mChatController.Draw();
    }
}

void Stage2Scene::OnRemotePlayerAttack(const PKT_S_PLAYER_ATTACK& attack)
{
    const PlayerClass playerClass = static_cast<PlayerClass>(attack.classType);
    if (playerClass != PlayerClass::Warrior &&
        playerClass != PlayerClass::Mage &&
        playerClass != PlayerClass::Archer)
    {
        return;
    }

    if (attack.attackPhase == PLAYER_ATTACK_PHASE_CAST)
    {
        if (playerClass == PlayerClass::Archer && attack.skillType == 0)
        {
            mSkillEffectManager.SpawnArcherBasicArrow(
                { attack.x, attack.y, attack.z },
                attack.rotY,
                attack.effectRadius,
                attack.effectDelay);
        }
        else if (playerClass == PlayerClass::Mage && attack.skillType == 0)
        {
            mSkillEffectManager.SpawnMageBasicOrb(
                { attack.x, attack.y, attack.z },
                attack.rotY,
                attack.effectRadius,
                attack.effectDelay);
        }
        else if (playerClass == PlayerClass::Archer && attack.skillType == 1)
        {
            mSkillEffectManager.OnRemoteSkillCast(
                playerClass,
                attack.skillType,
                { attack.x, attack.y, attack.z },
                { attack.effectX, attack.effectY, attack.effectZ },
                attack.rotY,
                attack.effectRadius);
        }
        else if (playerClass == PlayerClass::Mage && attack.skillType == 2)
        {
            mSkillEffectManager.PreviewMageMeteor(
                { attack.effectX, attack.effectY, attack.effectZ },
                attack.effectRadius,
                (std::max)(attack.effectDelay, 0.16f));
        }
        else if (playerClass == PlayerClass::Archer && attack.skillType == 2)
        {
            mSkillEffectManager.PreviewArcherArrowRain(
                { attack.effectX, attack.effectY, attack.effectZ },
                attack.effectRadius,
                (std::max)(attack.effectDelay, 0.16f));
        }
        return;
    }

    if (attack.attackPhase != PLAYER_ATTACK_PHASE_IMPACT)
    {
        return;
    }

    mSkillEffectManager.OnRemoteSkillCast(
        playerClass,
        attack.skillType,
        { attack.x, attack.y, attack.z },
        { attack.effectX, attack.effectY, attack.effectZ },
        attack.rotY,
        attack.effectRadius);
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
