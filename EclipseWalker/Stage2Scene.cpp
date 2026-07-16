#include "Stage2Scene.h"
#include "Archer.h"
#include "CharacterVisualFactory.h"
#include "DebugConfig.h"
#include "EclipseWalkerGame.h"
#include "Monster.h"
#include "NetworkManager.h"
#include "SkeletalAnimationComponent.h"
#include "UIManager.h"
#include "VillageScene.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
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
    constexpr float kStage2FloorColliderYOffset = 0.06f;
    constexpr float kStage2CloudHeightA = 44.0f;
    constexpr float kStage2CloudHeightB = 58.0f;
    constexpr float kRespawnOverlayDelaySeconds = 5.0f;
    constexpr float kGoldInteractRadius = 1.2f;
    constexpr float kGoldInteractVerticalRange = 2.5f;
    constexpr int kGoldInteractRewardAmount = 5000;
    constexpr int kStage2SkeletonSpawnBaseId = 1101;
    const DirectX::XMFLOAT3 kStage2BossLightOffset = { 2.0f, 4.0f, -1.5f };
    const DirectX::XMFLOAT3 kStage2BossLightStrength = { 1.25f, 0.55f, 0.32f };
    constexpr float kStage2BossLightRange = 16.0f;
    constexpr float kCinematicCameraMoveSpeed = 8.0f;
    constexpr float kCinematicCameraFastMoveSpeed = 22.0f;
    constexpr float kCinematicCameraTurnSpeed = 1.6f;
    constexpr float kCinematicCameraMaxPitch = DirectX::XM_PIDIV2 - 0.05f;
    constexpr float kCinematicCameraBossSideDistance = 7.0f;
    constexpr float kCinematicCameraBossSideHeight = 1.25f;
    const std::array<DirectX::XMFLOAT3, MAX_LOBBY_PLAYERS> kStage2PlayerStartPositions =
    {{
        { -27.1057f, -2.37823f, 23.4912f },
        { -25.5721f, -2.37823f, 23.7738f },
        { -28.5696f, -2.37823f, 23.3746f },
    }};

    DirectX::XMFLOAT3 GetLocalStage2PlayerStartPosition()
    {
        const int slotIndex = NetworkManager::Get()->GetLocalPlayerSlotIndex();
        const int clampedSlot =
            (std::max)(0, (std::min)(slotIndex, MAX_LOBBY_PLAYERS - 1));
        return kStage2PlayerStartPositions[clampedSlot];
    }

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

    bool IsCollectibleGoldMeshName(const std::string& meshName)
    {
        const std::string lowerName = ToLowerCopy(meshName);
        return lowerName.find("gold") != std::string::npos &&
            lowerName.find("chest") == std::string::npos;
    }

    std::wstring Utf8ToWideStage2(const std::string& text)
    {
        if (text.empty())
        {
            return L"";
        }

        const int sizeNeeded = MultiByteToWideChar(
            CP_UTF8,
            0,
            text.c_str(),
            static_cast<int>(text.size()),
            nullptr,
            0);
        if (sizeNeeded <= 0)
        {
            return L"";
        }

        std::wstring result(static_cast<size_t>(sizeNeeded), L'\0');
        MultiByteToWideChar(
            CP_UTF8,
            0,
            text.c_str(),
            static_cast<int>(text.size()),
            result.data(),
            sizeNeeded);
        return result;
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

    float WrapUnit(float value)
    {
        const float wrapped = std::fmod(value, 1.0f);
        return wrapped < 0.0f ? wrapped + 1.0f : wrapped;
    }

    void SetCloudTexTransform(
        RenderItem* renderItem,
        float tileU,
        float tileV,
        float offsetU,
        float offsetV)
    {
        if (renderItem == nullptr)
        {
            return;
        }

        DirectX::XMStoreFloat4x4(
            &renderItem->TexTransform,
            DirectX::XMMatrixScaling(tileU, tileV, 1.0f) *
            DirectX::XMMatrixTranslation(offsetU, offsetV, 0.0f));
        renderItem->NumFramesDirty = gNumFrameResources;
    }
}

Stage2Scene::Stage2Scene(EclipseWalkerGame* game, float initialSkyEclipseElapsedSeconds)
    : Scene(game)
    , mChatController(game)
    , mCombatSystem(game)
    , mDamageTextRenderer(game)
    , mPickupSystem(game, &mLanternSystem)
    , mWorldStateController(game, &mLanternSystem)
    , mInitialSkyEclipseElapsedSeconds((std::max)(0.0f, initialSkyEclipseElapsedSeconds))
    , mInitialSkyEclipseSyncTick(GetTickCount64())
{
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

    if (player->ConsumePendingImmuneText())
    {
        DirectX::XMFLOAT3 textPosition = player->GetPosition();
        textPosition.y += Player::DefaultColliderHalfHeight * 0.85f;
        mDamageTextRenderer.SpawnImmune(textPosition);
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
    UNREFERENCED_PARAMETER(player);
    mDebugColliderVisualizer.Reset();
}

void Stage2Scene::CreateMonsterHealthBar(Monster* monster)
{
    if (monster == nullptr || monster->GetType() == MonsterType::STAGE2_BOSS)
    {
        return;
    }

    auto* res = mGame->GetResources();
    auto& ritems = mGame->GetRitems();
    auto& objs = mGame->GetGameObjects();
    auto geoIt = res->mGeometries.find("quadGeo");
    if (geoIt == res->mGeometries.end())
    {
        return;
    }

    auto createBarObject = [&](const std::string& materialName, float scaleX, float scaleY)
    {
        auto ritem = std::make_unique<RenderItem>();
        ritem->Geo = geoIt->second.get();
        ritem->Mat = res->GetMaterial(materialName);
        ritem->ObjCBIndex = static_cast<UINT>(ritems.size());
        ritem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

        const auto& drawArgs = ritem->Geo->DrawArgs["quad"];
        ritem->IndexCount = drawArgs.IndexCount;
        ritem->StartIndexLocation = drawArgs.StartIndexLocation;
        ritem->BaseVertexLocation = drawArgs.BaseVertexLocation;
        ritem->Visible = false;

        auto object = std::make_unique<GameObject>();
        const DirectX::XMFLOAT3 pos = monster->GetPosition();
        object->SetScale(scaleX, scaleY, 1.0f);
        object->SetPosition(pos.x, pos.y + monster->GetColliderHalfHeight() + 0.04f, pos.z);
        object->mIsBillboard = true;
        object->Ritem = ritem.get();
        object->Update();

        GameObject* rawObject = object.get();
        TrackOwned(rawObject, ritem.get());
        ritems.push_back(std::move(ritem));
        objs.push_back(std::move(object));
        return rawObject;
    };

    MonsterHealthBar healthBar;
    healthBar.Owner = monster;
    healthBar.Back = createBarObject("MonsterHpBackMat", 0.42f, 0.04f);
    healthBar.Fill = createBarObject("MonsterHpFillMat", 0.38f, 0.024f);
    if (healthBar.Back != nullptr && healthBar.Fill != nullptr)
    {
        mMonsterHealthBars.push_back(healthBar);
    }
}

void Stage2Scene::UpdateMonsterHealthBars()
{
    const DirectX::XMFLOAT3 cameraPos = mGame->GetCamera()->GetPosition3f();

    for (auto& healthBar : mMonsterHealthBars)
    {
        Monster* monster = healthBar.Owner;
        if (monster == nullptr || healthBar.Back == nullptr || healthBar.Fill == nullptr ||
            healthBar.Back->Ritem == nullptr || healthBar.Fill->Ritem == nullptr)
        {
            continue;
        }

        const float ratio = monster->GetHealthRatio();
        const bool visible = monster->GetState() != MonsterState::DIE &&
            monster->Ritem != nullptr && monster->Ritem->Visible && ratio > 0.0f;
        healthBar.Back->Ritem->Visible = visible;
        healthBar.Fill->Ritem->Visible = visible;
        if (!visible)
        {
            continue;
        }

        const DirectX::XMFLOAT3 monsterPos = monster->GetPosition();
        const float y = monsterPos.y + monster->GetColliderHalfHeight() + 0.04f;

        float dx = cameraPos.x - monsterPos.x;
        float dz = cameraPos.z - monsterPos.z;
        const float lenSq = dx * dx + dz * dz;
        if (lenSq > 0.0001f)
        {
            const float invLen = 1.0f / sqrtf(lenSq);
            dx *= invLen;
            dz *= invLen;
        }
        else
        {
            dx = 0.0f;
            dz = 1.0f;
        }

        const float fullWidth = 0.42f;
        const float fillFullWidth = fullWidth * 0.90f;
        const float fillWidth = fillFullWidth * ratio;
        const float rightX = dz;
        const float rightZ = -dx;
        const float leftAnchorOffset = fillFullWidth - fillWidth;

        healthBar.Back->SetScale(fullWidth, 0.04f, 1.0f);
        healthBar.Back->SetPosition(monsterPos.x, y, monsterPos.z);
        healthBar.Fill->SetScale(fillWidth, 0.024f, 1.0f);
        healthBar.Fill->SetPosition(
            monsterPos.x + dx * 0.014f + rightX * leftAnchorOffset,
            y + 0.001f,
            monsterPos.z + dz * 0.014f + rightZ * leftAnchorOffset);
        healthBar.Back->Update();
        healthBar.Fill->Update();
    }
}

void Stage2Scene::ShowServerStageClear(const PKT_S_GAME_RESULT& result)
{
    if (mStageClearShown || result.resultCode != GAME_RESULT_VICTORY || mGame == nullptr)
    {
        return;
    }

    std::vector<UIManager::StageClearEntry> entries;
    const int entryCount = (std::max)(0, (std::min)(result.playerCount, MAX_LOBBY_PLAYERS));
    entries.reserve(static_cast<size_t>(entryCount));

    for (int i = 0; i < entryCount; ++i)
    {
        const std::wstring convertedName = Utf8ToWideStage2(std::string(result.playerNames[i]));
        UIManager::StageClearEntry entry;
        entry.Name = convertedName.empty()
            ? (L"Player " + std::to_wstring(result.playerIds[i]))
            : convertedName;
        entry.Damage = (std::max)(0, result.bossDamageDealt[i]);
        entries.push_back(std::move(entry));
    }

    std::sort(
        entries.begin(),
        entries.end(),
        [](const UIManager::StageClearEntry& lhs, const UIManager::StageClearEntry& rhs)
        {
            if (lhs.Damage != rhs.Damage)
            {
                return lhs.Damage > rhs.Damage;
            }

            return lhs.Name < rhs.Name;
        });

    std::vector<UIManager::StageClearRecordEntry> records;
    const int recordCount = (std::max)(0, (std::min)(result.recordCount, MAX_GAME_RECORDS));
    records.reserve(static_cast<size_t>(recordCount));
    for (int i = 0; i < recordCount; ++i)
    {
        const GameRecordSummary& source = result.records[i];
        UIManager::StageClearRecordEntry record;
        record.Rank = i + 1;
        record.ClearTimeSeconds = (std::max)(0.0f, source.clearTimeSeconds);
        record.TotalDamage = (std::max)(0, source.totalBossDamage);
        record.TopDealerName = Utf8ToWideStage2(std::string(source.topDealerName));
        record.TopDamage = (std::max)(0, source.topDamage);
        record.PartySummary = Utf8ToWideStage2(std::string(source.partySummary));
        records.push_back(std::move(record));
    }

    mRespawnOverlayActive = false;
    mRespawnButtonReady = false;
    mRespawnOverlayCountdown = 0.0f;
    mRespawnMousePressed = false;
    mRespawnRequestPending = false;
    if (auto* uiManager = mGame->GetUIManager())
    {
        uiManager->SetRespawnScreenState(false, 0.0f, false);
        uiManager->SetEclipseTimerState(false, 0.0f, 1.0f);
        uiManager->HideBossHealthBar();
        uiManager->HideMirrorCrackWarning();
        uiManager->SetStageClearScreenState(
            true,
            result.clearTimeSeconds,
            entries,
            records,
            result.currentRecordRank);
    }

    mStageClearShown = true;
}

void Stage2Scene::ShowLocalStageClear()
{
    if (mStageClearShown || mGame == nullptr)
    {
        return;
    }

    UIManager::StageClearEntry entry;
    entry.Name = Utf8ToWideStage2(NetworkManager::Get()->GetMyDisplayName());
    if (entry.Name.empty())
    {
        entry.Name = L"Player";
    }
    entry.Damage = (std::max)(0, static_cast<int>(mAccumulatedLocalBossDamage + 0.5f));

    std::vector<UIManager::StageClearEntry> entries;
    entries.push_back(std::move(entry));

    mRespawnOverlayActive = false;
    mRespawnButtonReady = false;
    mRespawnOverlayCountdown = 0.0f;
    mRespawnMousePressed = false;
    mRespawnRequestPending = false;
    if (auto* uiManager = mGame->GetUIManager())
    {
        uiManager->SetRespawnScreenState(false, 0.0f, false);
        uiManager->SetEclipseTimerState(false, 0.0f, 1.0f);
        uiManager->HideBossHealthBar();
        uiManager->HideMirrorCrackWarning();
        uiManager->SetStageClearScreenState(true, mStageClearElapsedSeconds, entries);
    }

    mStageClearShown = true;
}

void Stage2Scene::ShowEclipseGameOver(float elapsedSeconds)
{
    if (mStageClearShown || mGame == nullptr)
    {
        return;
    }

    mRespawnOverlayActive = false;
    mRespawnButtonReady = false;
    mRespawnOverlayCountdown = 0.0f;
    mRespawnMousePressed = false;
    mRespawnRequestPending = false;
    mCombatSystem.ClearSelectedTarget();
    if (elapsedSeconds >= 0.0f)
    {
        mSkyEclipseElapsedSeconds = (std::max)(mSkyEclipseElapsedSeconds, elapsedSeconds);
    }

    if (auto* uiManager = mGame->GetUIManager())
    {
        uiManager->SetRespawnScreenState(false, 0.0f, false);
        uiManager->SetEclipseTimerState(false, 0.0f, 1.0f);
        uiManager->HideBossHealthBar();
        uiManager->HideMirrorCrackWarning();
        uiManager->SetGameOverScreenState(true, mSkyEclipseElapsedSeconds);
    }

    mStageClearShown = true;
}

void Stage2Scene::UpdateStageClearState(const GameTimer& gt, Player* player)
{
    UNREFERENCED_PARAMETER(player);

    for (const PKT_S_GAME_RESULT& result : NetworkManager::Get()->PopGameResults())
    {
        if (result.resultCode == GAME_RESULT_VICTORY)
        {
            ShowServerStageClear(result);
        }
        else if (result.resultCode == GAME_RESULT_DEFEAT)
        {
            ShowEclipseGameOver(result.clearTimeSeconds);
        }
    }

    if (!mCinematicCameraDebugActive &&
        !mStageClearShown &&
        mSkyEclipseElapsedSeconds >= SkyEclipseDurationSeconds)
    {
        ShowEclipseGameOver();
    }

    if (mStageClearShown)
    {
        return;
    }

    Monster* boss = mBossController.GetBoss();
    if (boss == nullptr)
    {
        return;
    }

    const float currentBossHp = boss->GetHP();
    if (mLastObservedBossHp < 0.0f)
    {
        mLastObservedBossHp = currentBossHp;
    }

    if (NetworkManager::Get()->IsConnected())
    {
        mLastObservedBossHp = currentBossHp;
        return;
    }

    if (!mCinematicCameraDebugActive)
    {
        mStageClearElapsedSeconds += gt.DeltaTime();
    }

    if (currentBossHp < mLastObservedBossHp - 0.01f)
    {
        mAccumulatedLocalBossDamage += (mLastObservedBossHp - currentBossHp);
    }
    mLastObservedBossHp = currentBossHp;

    if (boss->GetState() == MonsterState::DYING || boss->GetState() == MonsterState::DIE || currentBossHp <= 0.0f)
    {
        ShowLocalStageClear();
    }
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
        if (NetworkManager::Get()->IsConnected())
        {
            if (!mRespawnRequestPending)
            {
                NetworkManager::Get()->SendPlayerRespawn();
                mRespawnRequestPending = true;
            }
            return;
        }

        const DirectX::XMFLOAT3 respawnPosition = player->GetPosition();
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
    mRespawnRequestPending = false;
}

void Stage2Scene::UpdateRespawnOverlay(const GameTimer& gt, Player* player, bool hasFocus)
{
    auto* uiManager = mGame != nullptr ? mGame->GetUIManager() : nullptr;
    if (player == nullptr || uiManager == nullptr)
    {
        return;
    }

    if (mStageClearShown)
    {
        mRespawnOverlayActive = false;
        mRespawnButtonReady = false;
        mRespawnOverlayCountdown = 0.0f;
        mRespawnMousePressed = false;
        mRespawnRequestPending = false;
        uiManager->SetRespawnScreenState(false, 0.0f, false);
        mWasPlayerDeadLastFrame = player->IsDead();
        return;
    }

    const bool isDead = player->IsDead();
    if (isDead && !mWasPlayerDeadLastFrame)
    {
        mRespawnOverlayActive = true;
        mRespawnButtonReady = false;
        mRespawnRequestPending = false;
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

            if (clickedButton && mRespawnButtonReady && !mRespawnRequestPending)
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
            mRespawnRequestPending = false;
        }
    }

    uiManager->SetRespawnScreenState(
        mRespawnOverlayActive,
        mRespawnOverlayCountdown,
        mRespawnButtonReady && !mRespawnRequestPending);

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
    const float elapsedSinceSync =
        mInitialSkyEclipseSyncTick > 0
        ? static_cast<float>(GetTickCount64() - mInitialSkyEclipseSyncTick) / 1000.0f
        : 0.0f;
    mSkyEclipseElapsedSeconds = (std::max)(0.0f, mInitialSkyEclipseElapsedSeconds + elapsedSinceSync);
    mStageClearMousePressed = false;
    mRespawnOverlayActive = false;
    mRespawnButtonReady = false;
    mRespawnMousePressed = false;
    mWasPlayerDeadLastFrame = false;
    mHasQueuedRespawnPacket = false;
    mRespawnRequestPending = false;
    mRespawnOverlayCountdown = 0.0f;
    mMonsterHealthBars.clear();
    mStageClearShown = false;
    mCinematicCameraDebugActive = false;
    mCinematicCameraToggleKeyPressed = false;
    mCinematicCameraPosition = { 0.0f, 0.0f, 0.0f };
    mCinematicCameraYaw = 0.0f;
    mCinematicCameraPitch = 0.0f;
    mStageClearElapsedSeconds = 0.0f;
    mAccumulatedLocalBossDamage = 0.0f;
    mLastObservedBossHp = -1.0f;
    mCombatSystem.Reset();
    mMonsterPtrs.clear();
    mMonsterTargetPos.clear();
    mMonsterServerStates.clear();
    mMonsterById.clear();
    mGoldInteractables.clear();
    mGoldInteractKeyPressed = false;

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

    auto BuildStage2Materials = [&](const std::vector<std::string>& textureNames, const std::vector<ImportedMaterialInfo>& materialInfos)
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

            const ImportedMaterialInfo* importedInfo = i < materialInfos.size() ? &materialInfos[i] : nullptr;
            XMFLOAT3 fresnelR0 = importedInfo != nullptr ? importedInfo->FresnelR0 : XMFLOAT3(0.05f, 0.05f, 0.05f);
            float roughness = importedInfo != nullptr ? importedInfo->Roughness : 0.8f;
            float metallicFactor = importedInfo != nullptr ? importedInfo->MetallicFactor : 0.0f;
            if (!metallicName.empty() && (importedInfo == nullptr || !importedInfo->HasMetallicFactor))
            {
                metallicFactor = 1.0f;
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
                    fresnelR0,
                    roughness,
                    metallicFactor);
            }

            if (Material* material = res->GetMaterial(materialName))
            {
                material->DiffuseMapName = diffuseName;
                material->NormalMapName = normalName;
                material->EmissiveMapName = emissiveName;
                material->MetallicMapName = metallicName;
                material->DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
                material->FresnelR0 = fresnelR0;
                material->Roughness = roughness;
                material->MetallicFactor = metallicFactor;
                material->IsToon = 0;
                material->IsTransparent = (baseName == "Decals") ? 3 : 0;
                material->OutlineThickness = 0.0f;
                material->NumFramesDirty = gNumFrameResources;
            }
        }

        return materialBindings;
    };

    auto ExtractTextureNames = [](const std::vector<ImportedMaterialInfo>& materialInfos)
    {
        std::vector<std::string> textureNames;
        textureNames.reserve(materialInfos.size());
        for (const ImportedMaterialInfo& materialInfo : materialInfos)
        {
            textureNames.push_back(materialInfo.DiffuseTextureName);
        }
        return textureNames;
    };

    const auto stage2MaterialInfos = ModelLoader::LoadMaterialInfos("Models/Stage2Map/Stage2Map.fbx");
    const auto stage2TextureNames = ExtractTextureNames(stage2MaterialInfos);
    LoadStage2Textures(stage2TextureNames);
    if (std::filesystem::exists(L"Textures/sky_stage2.dds"))
    {
        res->LoadTexture("sky_stage2", L"Textures/sky_stage2.dds");
    }
    else
    {
        res->LoadTexture("sky", L"Textures/sky.dds");
    }
    const auto stage2MaterialBindings = BuildStage2Materials(stage2TextureNames, stage2MaterialInfos);

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

    if (std::filesystem::exists(L"Textures/Sky/FX_CloudAlpha05.dds"))
    {
        res->LoadTexture("SkyCloudAlpha05", L"Textures/Sky/FX_CloudAlpha05.dds");
    }
    if (std::filesystem::exists(L"Textures/Sky/FX_CloudAlpha08.dds"))
    {
        res->LoadTexture("SkyCloudAlpha08", L"Textures/Sky/FX_CloudAlpha08.dds");
    }

    auto ensureCloudMaterial = [&](const std::string& materialName, const std::string& diffuseMapName, const DirectX::XMFLOAT4& albedo)
    {
        if (res->GetMaterial(materialName) == nullptr)
        {
            res->CreateMaterial(
                materialName,
                static_cast<int>(res->mMaterials.size()),
                diffuseMapName,
                "",
                "",
                "",
                albedo,
                DirectX::XMFLOAT3(0.01f, 0.01f, 0.01f),
                1.0f);
        }

        if (Material* material = res->GetMaterial(materialName))
        {
            material->DiffuseMapName = diffuseMapName;
            material->DiffuseAlbedo = albedo;
            material->IsToon = 0;
            material->IsTransparent = 2;
            material->OutlineThickness = 0.0f;
            material->NumFramesDirty = gNumFrameResources;
        }
    };

    ensureCloudMaterial("Stage2CloudLayerA", "SkyCloudAlpha05", DirectX::XMFLOAT4(1.16f, 1.02f, 1.00f, 0.30f));
    ensureCloudMaterial("Stage2CloudLayerB", "SkyCloudAlpha08", DirectX::XMFLOAT4(0.96f, 0.84f, 0.86f, 0.14f));

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
    BuildGoldInteractables();

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

    auto addCloudLayer = [&](const std::string& materialName, float y, float scale, float yaw) -> RenderItem*
    {
        auto renderItem = std::make_unique<RenderItem>();
        renderItem->World = MathHelper::Identity4x4();
        renderItem->TexTransform = MathHelper::Identity4x4();
        renderItem->ObjCBIndex = static_cast<UINT>(ritems.size());
        renderItem->Mat = res->GetMaterial(materialName);
        renderItem->Geo = res->mGeometries["quadGeo"].get();
        renderItem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        renderItem->CastShadow = false;
        renderItem->Visible = renderItem->Mat != nullptr && renderItem->Geo != nullptr;

        if (renderItem->Geo != nullptr)
        {
            auto& drawArgs = renderItem->Geo->DrawArgs["quad"];
            renderItem->IndexCount = drawArgs.IndexCount;
            renderItem->StartIndexLocation = drawArgs.StartIndexLocation;
            renderItem->BaseVertexLocation = drawArgs.BaseVertexLocation;
        }

        auto object = std::make_unique<GameObject>();
        object->Ritem = renderItem.get();
        object->SetScale(scale, scale, 1.0f);
        object->SetRotation(-DirectX::XM_PIDIV2, yaw, 0.0f);
        object->SetPosition(0.0f, y, 0.0f);
        object->Update();

        RenderItem* rawRenderItem = renderItem.get();
        TrackOwned(object.get(), rawRenderItem);
        ritems.push_back(std::move(renderItem));
        objs.push_back(std::move(object));
        return rawRenderItem;
    };

    mCloudLayerA = addCloudLayer("Stage2CloudLayerA", kStage2CloudHeightA, 460.0f, 0.24f);
    mCloudLayerB = addCloudLayer("Stage2CloudLayerB", kStage2CloudHeightB, 390.0f, -0.42f);
    SetCloudTexTransform(mCloudLayerA, 2.6f, 2.6f, 0.0f, 0.0f);
    SetCloudTexTransform(mCloudLayerB, 2.0f, 2.0f, 0.0f, 0.0f);

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

    mMapSystem->LoadFloorCollider("Models/Stage2Map/FloorCollider.fbx", kStage2MapScale, 0.0f, 0.0f, 0.0f, 0.0f, kStage2FloorColliderYOffset, 0.0f);
    mMapSystem->LoadWallCollider("Models/Stage2Map/Stage2WallCollider.fbx", kStage2MapScale);

    auto ensureHealthBarMaterial = [&](const std::string& name, const DirectX::XMFLOAT4& color)
    {
        if (res->GetMaterial(name) == nullptr)
        {
            res->CreateMaterial(
                name,
                static_cast<int>(res->mMaterials.size()),
                "white",
                "",
                "",
                "",
                color,
                DirectX::XMFLOAT3(0.01f, 0.01f, 0.01f),
                0.45f);
        }

        if (auto* mat = res->GetMaterial(name))
        {
            mat->DiffuseAlbedo = color;
            mat->IsTransparent = 1;
            mat->NumFramesDirty = gNumFrameResources;
        }
    };

    ensureHealthBarMaterial("MonsterHpBackMat", DirectX::XMFLOAT4(0.04f, 0.015f, 0.018f, 0.82f));
    ensureHealthBarMaterial("MonsterHpFillMat", DirectX::XMFLOAT4(0.95f, 0.06f, 0.04f, 0.95f));

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
        mMonsterServerStates[spawn.Id] = MonsterState::IDLE;

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
            visualSpec.AdditionalAnimationClips.push_back({ "Models/Skeleton2/Animation/Attack.fbx", "SkeletonAttack" });
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
            visualSpec.AdditionalAnimationClips.push_back({ "Models/Skeleton/Animation/Attack.fbx", "SkeletonAttack" });
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
        mMonsterById[spawn.Id] = monster.get();
        mMonsterPtrs.push_back(monster.get());
        ritems.push_back(std::move(ri));
        objs.push_back(std::move(monster));
        CreateMonsterHealthBar(mMonsterPtrs.back());
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
        const DirectX::XMFLOAT3 playerStartPosition = GetLocalStage2PlayerStartPosition();
        player->SetPosition(
            playerStartPosition.x,
            playerStartPosition.y,
            playerStartPosition.z);

        if (DebugConfig::kEnableBackendConnection)
        {
            player->ForceSendNetworkState();
        }
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
    {
        const DirectX::XMFLOAT3 bossAnchor = Stage2BossController::GetBossAnchorPosition();
        mGame->AddPointLight(
            {
                bossAnchor.x + kStage2BossLightOffset.x,
                bossAnchor.y + kStage2BossLightOffset.y,
                bossAnchor.z + kStage2BossLightOffset.z
            },
            kStage2BossLightStrength,
            kStage2BossLightRange);
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
    mCombatSystem.SetTargetSelectionOverridePicker(
        [this](
            const DirectX::XMFLOAT3& rayOrigin,
            const DirectX::XMFLOAT3& rayDirection,
            const Player* player,
            CombatSystem::TargetSelectionOverride& outOverride)
        {
            if (player == nullptr)
            {
                return false;
            }

            Stage2BossController::MirrorPickResult mirrorPick;
            if (!mBossController.TryPickMirrorTarget(
                rayOrigin,
                rayDirection,
                player->GetPosition(),
                mirrorPick))
            {
                return false;
            }

            outOverride.HighlightRenderItem = mirrorPick.HighlightRenderItem;
            outOverride.Position = mirrorPick.Position;
            outOverride.HalfHeight = mirrorPick.HalfHeight;
            outOverride.MonsterId = mirrorPick.MonsterId;
            outOverride.HitDistance = mirrorPick.HitDistance;
            return true;
        });
    mCombatSystem.SetTargetSelectionOverrideValidityCallback(
        [this]()
        {
            return mBossController.IsMirrorSplitTargetingActive();
        });
    mCombatSystem.SetSkillEffectManager(&mSkillEffectManager);
    mPickupSystem.Initialize();
    mBossController.InitializeHealthText();

    if (Monster* boss = mBossController.GetBoss())
    {
        mLastObservedBossHp = boss->GetHP();
    }

    if (auto* uiManager = mGame->GetUIManager())
    {
        uiManager->SetRespawnScreenState(false, 0.0f, false);
        uiManager->SetStageClearScreenState(false, 0.0f, {});
        uiManager->SetEclipseTimerState(
            true,
            SkyEclipseDurationSeconds - mSkyEclipseElapsedSeconds,
            mSkyEclipseElapsedSeconds / SkyEclipseDurationSeconds);
    }
}

void Stage2Scene::Exit()
{
    OutputDebugStringA("\n[Stage 2] 종료. 메모리 해제.\n");
    mSkillEffectManager.Reset();
    ReleaseOwnedObjects();
    mCloudLayerA = nullptr;
    mCloudLayerB = nullptr;
    mGame->ResetLights();
    mDebugColliderVisualizer.Reset();
    mBossController.Reset();
    mWorldStateController.Reset();
    mDomainBoundaryObj = nullptr;
    mMonsterPtrs.clear();
    mMonsterHealthBars.clear();
    mMonsterTargetPos.clear();
    mMonsterServerStates.clear();
    mMonsterById.clear();
    mChatController.Reset();
    mDamageTextRenderer.Reset();
    mCombatSystem.Reset();
    mPickupSystem.Reset();
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
    mRespawnRequestPending = false;
    mRespawnOverlayCountdown = 0.0f;
    mGoldInteractables.clear();
    mGoldInteractKeyPressed = false;
    mReturnToVillageConfirmActive = false;
    mReturnToVillageKeyPressed = false;
    mReturnToVillageDecisionKeyPressed = false;
    mReturnToVillageMousePressed = false;
    mCinematicCameraDebugActive = false;
    mCinematicCameraToggleKeyPressed = false;
    mCinematicCameraPosition = { 0.0f, 0.0f, 0.0f };
    mCinematicCameraYaw = 0.0f;
    mCinematicCameraPitch = 0.0f;

    if (auto* uiManager = mGame->GetUIManager())
    {
        uiManager->SetRespawnScreenState(false, 0.0f, false);
        uiManager->SetReturnToVillageConfirmState(false);
        uiManager->SetStageClearScreenState(false, 0.0f, {});
        uiManager->SetEclipseTimerState(false, 0.0f, 0.0f);
    }
}

void Stage2Scene::BuildGoldInteractables()
{
    mGoldInteractables.clear();

    std::vector<DirectX::XMFLOAT3> uniqueGoldPositions;
    const auto goldBounds = ModelLoader::LoadNamedMeshBounds("Models/Stage2Map/Stage2Map.fbx", "Gold");
    for (const NamedMeshBounds& goldBound : goldBounds)
    {
        if (!IsCollectibleGoldMeshName(goldBound.Name))
        {
            continue;
        }

        const DirectX::XMFLOAT3 scaledPosition =
        {
            goldBound.Center.x * kStage2MapScale,
            goldBound.Center.y * kStage2MapScale,
            goldBound.Center.z * kStage2MapScale
        };

        bool isDuplicate = false;
        for (const DirectX::XMFLOAT3& existing : uniqueGoldPositions)
        {
            const float dx = existing.x - scaledPosition.x;
            const float dy = existing.y - scaledPosition.y;
            const float dz = existing.z - scaledPosition.z;
            if ((dx * dx + dy * dy + dz * dz) <= 0.16f)
            {
                isDuplicate = true;
                break;
            }
        }

        if (!isDuplicate)
        {
            uniqueGoldPositions.push_back(scaledPosition);
        }
    }

    for (const DirectX::XMFLOAT3& position : uniqueGoldPositions)
    {
        GoldInteractable gold;
        gold.Position = position;
        gold.Radius = kGoldInteractRadius;
        mGoldInteractables.push_back(gold);
    }

    std::ostringstream goldLog;
    goldLog << "[Stage2] Registered gold interact triggers: " << mGoldInteractables.size()
        << " (pieces=" << uniqueGoldPositions.size() << ")\n";
    OutputDebugStringA(goldLog.str().c_str());
}

bool Stage2Scene::IsPlayerNearUncollectedGold(const DirectX::XMFLOAT3& playerPosition) const
{
    for (const GoldInteractable& gold : mGoldInteractables)
    {
        if (gold.Collected || gold.Pending)
        {
            continue;
        }

        const float dx = playerPosition.x - gold.Position.x;
        const float dz = playerPosition.z - gold.Position.z;
        const float dy = std::fabs(playerPosition.y - gold.Position.y);
        if ((dx * dx + dz * dz) <= (gold.Radius * gold.Radius) &&
            dy <= kGoldInteractVerticalRange)
        {
            return true;
        }
    }

    return false;
}

void Stage2Scene::EnableCinematicCameraDebug()
{
    auto* camera = mGame != nullptr ? mGame->GetCamera() : nullptr;
    if (camera == nullptr)
    {
        return;
    }

    mCinematicCameraDebugActive = true;
    mCinematicCameraPosition = camera->GetPosition3f();

    if (Monster* boss = mBossController.GetBoss())
    {
        const DirectX::XMFLOAT3 bossPosition = boss->GetPosition();
        const float bossYaw = boss->GetRotation().y;
        const DirectX::XMFLOAT3 bossLookTarget =
        {
            bossPosition.x,
            bossPosition.y + boss->GetColliderHalfHeight() * 0.65f,
            bossPosition.z
        };
        const DirectX::XMFLOAT3 bossRight =
        {
            std::cos(bossYaw),
            0.0f,
            -std::sin(bossYaw)
        };

        mCinematicCameraPosition =
        {
            bossLookTarget.x + bossRight.x * kCinematicCameraBossSideDistance,
            bossLookTarget.y + kCinematicCameraBossSideHeight,
            bossLookTarget.z + bossRight.z * kCinematicCameraBossSideDistance
        };

        const DirectX::XMVECTOR position = DirectX::XMLoadFloat3(&mCinematicCameraPosition);
        const DirectX::XMVECTOR target = DirectX::XMLoadFloat3(&bossLookTarget);
        DirectX::XMFLOAT3 look{};
        DirectX::XMStoreFloat3(&look, DirectX::XMVector3Normalize(target - position));
        mCinematicCameraYaw = std::atan2(look.x, look.z);
        mCinematicCameraPitch = std::asin((std::max)(-1.0f, (std::min)(1.0f, look.y)));
        camera->LookAt(position, target, DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
    }
    else
    {
        DirectX::XMFLOAT3 look{};
        DirectX::XMStoreFloat3(&look, DirectX::XMVector3Normalize(camera->GetLook()));
        mCinematicCameraYaw = std::atan2(look.x, look.z);
        mCinematicCameraPitch = std::asin((std::max)(-1.0f, (std::min)(1.0f, look.y)));
    }

    OutputDebugStringA("[Stage2][CinematicCamera] ON - boss right side shot, F12 toggle, WASD move, Space/Ctrl up/down, arrows look, Shift fast\n");
}

void Stage2Scene::DisableCinematicCameraDebug()
{
    if (!mCinematicCameraDebugActive)
    {
        return;
    }

    mCinematicCameraDebugActive = false;
    OutputDebugStringA("[Stage2][CinematicCamera] OFF\n");
}

void Stage2Scene::UpdateCinematicCameraDebugInput(const GameTimer& gt, bool hasFocus)
{
    const bool toggleKeyDown =
        hasFocus &&
        !mChatController.IsChatting() &&
        (GetAsyncKeyState(VK_F12) & 0x8000) != 0;
    if (toggleKeyDown && !mCinematicCameraToggleKeyPressed)
    {
        if (mCinematicCameraDebugActive)
        {
            DisableCinematicCameraDebug();
        }
        else
        {
            EnableCinematicCameraDebug();
        }
    }
    mCinematicCameraToggleKeyPressed = toggleKeyDown;

    if (!mCinematicCameraDebugActive)
    {
        return;
    }

    auto* camera = mGame != nullptr ? mGame->GetCamera() : nullptr;
    if (camera == nullptr || !hasFocus || mChatController.IsChatting())
    {
        return;
    }

    const float dt = (std::min)(gt.DeltaTime(), 0.05f);
    const float moveSpeed =
        (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0
            ? kCinematicCameraFastMoveSpeed
            : kCinematicCameraMoveSpeed;
    const float moveStep = moveSpeed * dt;
    const float turnStep = kCinematicCameraTurnSpeed * dt;

    if ((GetAsyncKeyState(VK_LEFT) & 0x8000) != 0) mCinematicCameraYaw -= turnStep;
    if ((GetAsyncKeyState(VK_RIGHT) & 0x8000) != 0) mCinematicCameraYaw += turnStep;
    if ((GetAsyncKeyState(VK_UP) & 0x8000) != 0) mCinematicCameraPitch += turnStep;
    if ((GetAsyncKeyState(VK_DOWN) & 0x8000) != 0) mCinematicCameraPitch -= turnStep;
    mCinematicCameraPitch = (std::max)(-kCinematicCameraMaxPitch, (std::min)(kCinematicCameraMaxPitch, mCinematicCameraPitch));

    const float cosPitch = std::cos(mCinematicCameraPitch);
    const DirectX::XMVECTOR forward = DirectX::XMVector3Normalize(DirectX::XMVectorSet(
        std::sin(mCinematicCameraYaw) * cosPitch,
        std::sin(mCinematicCameraPitch),
        std::cos(mCinematicCameraYaw) * cosPitch,
        0.0f));
    const DirectX::XMVECTOR right = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(
        DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f),
        forward));
    const DirectX::XMVECTOR up = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

    DirectX::XMVECTOR position = DirectX::XMLoadFloat3(&mCinematicCameraPosition);
    if ((GetAsyncKeyState('W') & 0x8000) != 0) position = DirectX::XMVectorMultiplyAdd(DirectX::XMVectorReplicate(moveStep), forward, position);
    if ((GetAsyncKeyState('S') & 0x8000) != 0) position = DirectX::XMVectorMultiplyAdd(DirectX::XMVectorReplicate(-moveStep), forward, position);
    if ((GetAsyncKeyState('D') & 0x8000) != 0) position = DirectX::XMVectorMultiplyAdd(DirectX::XMVectorReplicate(moveStep), right, position);
    if ((GetAsyncKeyState('A') & 0x8000) != 0) position = DirectX::XMVectorMultiplyAdd(DirectX::XMVectorReplicate(-moveStep), right, position);
    if ((GetAsyncKeyState(VK_SPACE) & 0x8000) != 0) position = DirectX::XMVectorMultiplyAdd(DirectX::XMVectorReplicate(moveStep), up, position);
    if ((GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0) position = DirectX::XMVectorMultiplyAdd(DirectX::XMVectorReplicate(-moveStep), up, position);

    DirectX::XMStoreFloat3(&mCinematicCameraPosition, position);
    camera->LookAt(position, position + forward, up);
}

bool Stage2Scene::TryCollectNearbyGold(Player* player)
{
    if (player == nullptr)
    {
        return false;
    }

    const DirectX::XMFLOAT3 playerPosition = player->GetPosition();
    for (GoldInteractable& gold : mGoldInteractables)
    {
        if (gold.Collected || gold.Pending)
        {
            continue;
        }

        const float dx = playerPosition.x - gold.Position.x;
        const float dz = playerPosition.z - gold.Position.z;
        const float dy = std::fabs(playerPosition.y - gold.Position.y);
        if ((dx * dx + dz * dz) > (gold.Radius * gold.Radius) ||
            dy > kGoldInteractVerticalRange)
        {
            continue;
        }

        if (DebugConfig::kEnableBackendConnection && NetworkManager::Get()->IsConnected())
        {
            NetworkManager::Get()->SendGoldPickup(
                GOLD_PICKUP_STAGE2_GROUP,
                gold.Position.x,
                gold.Position.y,
                gold.Position.z,
                gold.Radius);
            for (GoldInteractable& linkedGold : mGoldInteractables)
            {
                linkedGold.Pending = true;
            }
        }
        else
        {
            for (GoldInteractable& linkedGold : mGoldInteractables)
            {
                linkedGold.Collected = true;
                linkedGold.Pending = false;
            }
            player->AddGold(kGoldInteractRewardAmount);
        }

        std::ostringstream goldLog;
        goldLog << "[Stage2] Gold collected at x=" << gold.Position.x
            << " y=" << gold.Position.y
            << " z=" << gold.Position.z
            << " reward=" << kGoldInteractRewardAmount << "\n";
        OutputDebugStringA(goldLog.str().c_str());
        return true;
    }

    return false;
}

void Stage2Scene::UpdateMonstersFromServer()
{
    auto* nm = NetworkManager::Get();

    auto playStateTransitionSound = [&](Monster* monster, int monsterId, MonsterState nextState)
        {
            if (monster == nullptr || !monster->IsSkeletonType())
            {
                return;
            }

            const MonsterState previousState =
                (mMonsterServerStates.find(monsterId) != mMonsterServerStates.end())
                ? mMonsterServerStates[monsterId]
                : MonsterState::IDLE;

            if ((nextState == MonsterState::TRACE || nextState == MonsterState::ATTACK) &&
                previousState == MonsterState::IDLE)
            {
                monster->PlayAggroSound();
            }
            else if (nextState == MonsterState::IDLE && previousState != MonsterState::IDLE)
            {
                monster->PlayAmbientSound();
            }

            mMonsterServerStates[monsterId] = nextState;
        };

    std::vector<int> consumedHitIds;
    std::lock_guard<std::mutex> lock(nm->m_monsterMutex);

    for (auto& pair : nm->m_remoteMonsters)
    {
        const int id = pair.first;
        if (id == STAGE2_BOSS_MONSTER_ID)
        {
            continue;
        }

        PKT_S_MONSTER_SYNC& data = pair.second;

        auto it = mMonsterById.find(id);
        if (it == mMonsterById.end())
        {
            continue;
        }

        Monster* monster = it->second;
        playStateTransitionSound(
            monster,
            id,
            static_cast<MonsterState>(data.state));

        const bool isDead = data.isDead || data.state == 3 || data.remainHp <= 0;
        monster->ApplyServerState(data.state, data.remainHp, isDead, data.attackSequence);

        if (isDead)
        {
            mMonsterTargetPos.erase(id);
            continue;
        }

        mMonsterTargetPos[id] = { data.x, data.y, data.z };
        monster->SetRotation(0.0f, data.rotY * (3.14159265f / 180.0f), 0.0f);
    }

    for (auto& pair : nm->m_remoteMonsterHits)
    {
        const int id = pair.first;
        if (id == STAGE2_BOSS_MONSTER_ID)
        {
            continue;
        }

        PKT_S_MONSTER_HIT& data = pair.second;

        auto it = mMonsterById.find(id);
        if (it == mMonsterById.end())
        {
            continue;
        }

        Monster* monster = it->second;
        if (data.isDead && data.killerPlayerId == NetworkManager::Get()->m_myPlayerId)
        {
            mCombatSystem.ApplyMonsterKillReward(mGame->GetPlayer(), monster->GetExperienceReward());
        }

        if (data.damage > 0)
        {
            const DirectX::XMFLOAT3 monsterPos = monster->GetPosition();
            const DirectX::XMFLOAT3 textPosition =
            {
                monsterPos.x,
                monsterPos.y + monster->GetColliderHalfHeight() * 0.45f,
                monsterPos.z
            };
            mDamageTextRenderer.SpawnOutgoing(textPosition, static_cast<float>(data.damage));
        }

        monster->ApplyServerHit(data.remainHp, data.isDead);
        if (data.isDead)
        {
            mMonsterTargetPos.erase(id);
            mMonsterServerStates[id] = MonsterState::DIE;
        }

        consumedHitIds.push_back(id);
    }

    for (int id : consumedHitIds)
    {
        nm->m_remoteMonsterHits.erase(id);
    }
}

void Stage2Scene::Update(const GameTimer& gt)
{
    const bool wasChatting = mChatController.IsChatting();
    if (mReturnToVillageConfirmActive)
    {
        mChatController.UpdateMessagesOnly();
    }
    else
    {
        mChatController.Update(gt);
    }
    Player* pPlayer = mGame->GetPlayer();
    const bool hasFocus = (mGame != nullptr && GetForegroundWindow() == mGame->GetMainWindowHandle());
    UpdateCinematicCameraDebugInput(gt, hasFocus);
    mDamageTextRenderer.Update(gt.DeltaTime());
    if (!mCinematicCameraDebugActive)
    {
        mSkyEclipseElapsedSeconds += gt.DeltaTime();
    }
    SetCloudTexTransform(mCloudLayerA, 2.6f, 2.6f, WrapUnit(gt.TotalTime() * 0.0042f), WrapUnit(gt.TotalTime() * 0.0014f));
    SetCloudTexTransform(mCloudLayerB, 2.0f, 2.0f, WrapUnit(gt.TotalTime() * -0.0028f), WrapUnit(gt.TotalTime() * 0.0021f));

    for (const PKT_S_GOLD_UPDATE& goldUpdate : NetworkManager::Get()->PopGoldUpdates())
    {
        if (pPlayer != nullptr &&
            (NetworkManager::Get()->m_myPlayerId <= 0 ||
                goldUpdate.playerId == NetworkManager::Get()->m_myPlayerId))
        {
            pPlayer->SetGold(goldUpdate.gold);
        }

        if (goldUpdate.pickupGroupId == GOLD_PICKUP_STAGE2_GROUP && goldUpdate.pickupCollected)
        {
            for (GoldInteractable& gold : mGoldInteractables)
            {
                gold.Collected = true;
                gold.Pending = false;
            }
        }
    }

    for (const PKT_S_PLAYER_HIT& playerHit : NetworkManager::Get()->PopPlayerHits())
    {
        if (pPlayer != nullptr && playerHit.playerId == NetworkManager::Get()->m_myPlayerId)
        {
            if (playerHit.wasImmune)
            {
                DirectX::XMFLOAT3 textPosition = pPlayer->GetPosition();
                textPosition.y += Player::DefaultColliderHalfHeight * 0.85f;
                mDamageTextRenderer.SpawnImmune(textPosition);
            }
            pPlayer->ApplyServerHit(playerHit.remainHp, playerHit.isDead);
        }
        else
        {
            mGame->ApplyRemotePlayerHit(playerHit);
        }
    }

    for (const PKT_S_PLAYER_RESPAWN& respawn : NetworkManager::Get()->PopPlayerRespawns())
    {
        if (pPlayer != nullptr && respawn.playerId == NetworkManager::Get()->m_myPlayerId)
        {
            QueueRespawn(respawn);
            if (mRespawnRequestPending)
            {
                ApplyQueuedRespawn(pPlayer);
            }
        }
        else
        {
            mGame->ApplyRemotePlayerRespawn(respawn);
        }
    }

    UpdateRespawnOverlay(gt, pPlayer, hasFocus);

    const bool respawnOverlayActive =
        mGame->GetUIManager() != nullptr &&
        mGame->GetUIManager()->IsRespawnScreenActive();
    const bool stageClearOverlayActive =
        mGame->GetUIManager() != nullptr &&
        mGame->GetUIManager()->IsStageClearScreenActive();
    const bool returnKeyDown =
        hasFocus &&
        !mChatController.IsChatting() &&
        !respawnOverlayActive &&
        !stageClearOverlayActive &&
        !mStageClearShown &&
        pPlayer != nullptr &&
        !pPlayer->IsDead() &&
        (GetAsyncKeyState('B') & 0x8000) != 0;
    if (returnKeyDown && !mReturnToVillageKeyPressed)
    {
        mReturnToVillageConfirmActive = true;
        mReturnToVillageDecisionKeyPressed = false;
        mReturnToVillageMousePressed = false;
        if (auto* uiManager = mGame->GetUIManager())
        {
            uiManager->SetReturnToVillageConfirmState(true);
        }
    }
    mReturnToVillageKeyPressed = returnKeyDown;

    if (mReturnToVillageConfirmActive)
    {
        if (!hasFocus ||
            respawnOverlayActive ||
            stageClearOverlayActive ||
            mStageClearShown ||
            pPlayer == nullptr ||
            pPlayer->IsDead())
        {
            mReturnToVillageConfirmActive = false;
            mReturnToVillageDecisionKeyPressed = false;
            mReturnToVillageMousePressed = false;
            if (auto* uiManager = mGame->GetUIManager())
            {
                uiManager->SetReturnToVillageConfirmState(false);
            }
            return;
        }

        const bool yesDown = (GetAsyncKeyState('Y') & 0x8000) != 0;
        const bool noDown =
            (GetAsyncKeyState('N') & 0x8000) != 0 ||
            (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0;
        const bool mouseDown = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
        bool yesClicked = false;
        bool noClicked = false;
        if (mouseDown && !mReturnToVillageMousePressed)
        {
            if (auto* uiManager = mGame->GetUIManager())
            {
                yesClicked = uiManager->IsReturnToVillageYesButtonHovered();
                noClicked = uiManager->IsReturnToVillageNoButtonHovered();
            }
        }
        mReturnToVillageMousePressed = mouseDown;

        const bool decisionDown = yesDown || noDown;
        if ((decisionDown && !mReturnToVillageDecisionKeyPressed) || yesClicked || noClicked)
        {
            if (yesDown || yesClicked)
            {
                mReturnToVillageConfirmActive = false;
                if (auto* uiManager = mGame->GetUIManager())
                {
                    uiManager->SetReturnToVillageConfirmState(false);
                }
                mGame->RequestSceneChange(std::make_unique<VillageScene>(mGame), L"LOADING VILLAGE");
                return;
            }

            mReturnToVillageConfirmActive = false;
            if (auto* uiManager = mGame->GetUIManager())
            {
                uiManager->SetReturnToVillageConfirmState(false);
            }
        }
        mReturnToVillageDecisionKeyPressed = decisionDown;
        return;
    }

    for (const PKT_S_LANTERN_GAUGE& gaugeUpdate : NetworkManager::Get()->PopLanternGaugeUpdates())
    {
        if (pPlayer != nullptr)
        {
            pPlayer->GetLantern()->SetState(gaugeUpdate.gauge, gaugeUpdate.maxGauge, gaugeUpdate.level);
        }
    }

    for (const PKT_S_BOSS_PATTERN& bossPattern : NetworkManager::Get()->PopBossPatterns())
    {
        if (bossPattern.patternType == BOSS_PATTERN_STAGE2_MIRROR)
        {
            mCombatSystem.ClearSelectedTarget();
        }

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
                bossSync.attackSequence,
                bossSync.x,
                bossSync.y,
                bossSync.z,
                bossSync.rotY);
        }

        const auto bossHitIt = network->m_remoteMonsterHits.find(STAGE2_BOSS_MONSTER_ID);
        if (bossHitIt != network->m_remoteMonsterHits.end())
        {
            const PKT_S_MONSTER_HIT& bossHit = bossHitIt->second;
            if (bossHit.damage > 0)
            {
                if (Monster* boss = mBossController.GetBoss())
                {
                    const DirectX::XMFLOAT3 bossPos = boss->GetPosition();
                    const DirectX::XMFLOAT3 textPosition =
                    {
                        bossPos.x,
                        bossPos.y + boss->GetColliderHalfHeight() * 0.45f,
                        bossPos.z
                    };
                    mDamageTextRenderer.SpawnOutgoing(textPosition, static_cast<float>(bossHit.damage));
                }
            }
            mBossController.ApplyServerHit(bossHit.remainHp, bossHit.isDead);
            network->m_remoteMonsterHits.erase(bossHitIt);
        }
    }

    UpdateMonstersFromServer();

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

    if (pPlayer && !mCinematicCameraDebugActive)
    {  
        pPlayer->Update(gt, mMapSystem.get());
    }

    constexpr float kMonsterLerpSpeed = 14.0f;
    constexpr float kMonsterSnapDistanceSq = 25.0f;
    const float monsterLerpT = (std::min)(1.0f, kMonsterLerpSpeed * gt.DeltaTime());

    for (auto& pair : mMonsterTargetPos)
    {
        auto it = mMonsterById.find(pair.first);
        if (it == mMonsterById.end())
        {
            continue;
        }

        Monster* monster = it->second;
        DirectX::XMFLOAT3 current = monster->GetPosition();
        DirectX::XMFLOAT3 target = pair.second;
        const float targetDx = target.x - current.x;
        const float targetDy = target.y - current.y;
        const float targetDz = target.z - current.z;
        const float targetDistanceSq =
            (targetDx * targetDx) + (targetDy * targetDy) + (targetDz * targetDz);

        DirectX::XMFLOAT3 newPos = target;
        if (targetDistanceSq <= kMonsterSnapDistanceSq)
        {
            newPos =
            {
                current.x + (target.x - current.x) * monsterLerpT,
                current.y + (target.y - current.y) * monsterLerpT,
                current.z + (target.z - current.z) * monsterLerpT
            };
        }

        if (mMapSystem != nullptr)
        {
            const float groundY = mMapSystem->GetFloorHeight(newPos.x, newPos.z, newPos.y + 10.0f, 12.0f);
            if (groundY > -9000.0f)
            {
                newPos.y = groundY + monster->GetGroundOffset();
            }
        }

        monster->SetPosition(newPos.x, newPos.y, newPos.z);
        monster->GameObject::Update();
    }

    for (Monster* monster : mMonsterPtrs)
    {
        if (monster == nullptr || monster == mBossController.GetBoss())
        {
            continue;
        }

        monster->UpdateAnimationState(gt.DeltaTime());

        MonsterArrowRequest arrowRequest;
        if (monster->ConsumeArrowRequest(arrowRequest) &&
            monster->Ritem != nullptr && monster->Ritem->Visible)
        {
            mSkillEffectManager.SpawnArcherBasicArrow(
                monster->GetPosition(),
                monster->GetRotation().y,
                arrowRequest.TravelDistance,
                arrowRequest.StartDelay,
                arrowRequest.StartHeight,
                arrowRequest.StartRightOffset);
        }
    }

    std::vector<Monster*> activeMonsters;
    activeMonsters.reserve(mMonsterPtrs.size());
    for (Monster* monster : mMonsterPtrs)
    {
        if (monster != nullptr && monster->Ritem != nullptr && monster->Ritem->Visible)
        {
            activeMonsters.push_back(monster);
        }
    }

    mCombatSystem.Update(gt, pPlayer, activeMonsters, mMapSystem.get());
    UpdateMonsterHealthBars();
    std::vector<Monster*> pickupEligibleMonsters;
    pickupEligibleMonsters.reserve(mMonsterPtrs.size());
    for (Monster* monster : mMonsterPtrs)
    {
        if (monster != nullptr && monster != mBossController.GetBoss())
        {
            pickupEligibleMonsters.push_back(monster);
        }
    }
    mPickupSystem.Update(gt, pPlayer, mMapSystem.get(), pickupEligibleMonsters);
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
    if (!mStageClearShown)
    {
        mBossController.Update(gt, pPlayer, mWorldStateController.IsOtherWorld());
    }
    UpdateStageClearState(gt, pPlayer);
    if (auto* uiManager = mGame->GetUIManager())
    {
        const float eclipseProgress = mSkyEclipseElapsedSeconds / SkyEclipseDurationSeconds;
        uiManager->SetEclipseTimerState(
            !mStageClearShown,
            SkyEclipseDurationSeconds - mSkyEclipseElapsedSeconds,
            eclipseProgress);

        if (uiManager->IsStageClearScreenActive())
        {
            const bool mouseDown = hasFocus && (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
            const bool clicked = mouseDown && !mStageClearMousePressed;
            if (clicked)
            {
                if (uiManager->IsStageClearNextButtonHovered())
                {
                    uiManager->ShowStageClearRecords();
                }
                else if (uiManager->IsStageClearEndButtonHovered())
                {
                    mGame->RequestSceneChange(std::make_unique<VillageScene>(mGame), L"LOADING VILLAGE");
                    return;
                }
            }

            mStageClearMousePressed = mouseDown;
        }
        else
        {
            mStageClearMousePressed = false;
        }
    }
    UpdateStage2LanternAutoReturn(gt, pPlayer);

    const bool goldInteractKeyDown =
        hasFocus &&
        !mChatController.IsChatting() &&
        pPlayer != nullptr &&
        !pPlayer->IsDead() &&
        (GetAsyncKeyState('F') & 0x8000) != 0;
    if (goldInteractKeyDown && !mGoldInteractKeyPressed)
    {
        TryCollectNearbyGold(pPlayer);
    }
    mGoldInteractKeyPressed = goldInteractKeyDown;

    if (auto* uiManager = mGame->GetUIManager())
    {
        const bool hideChatForOverlay =
            uiManager->IsRespawnScreenActive() ||
            uiManager->IsStageClearScreenActive() ||
            mReturnToVillageConfirmActive ||
            mCinematicCameraDebugActive;
        uiManager->SetChatBoxState(
            !hideChatForOverlay && mChatController.IsChatting(),
            !hideChatForOverlay && mChatController.HasMessages());
    }

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
    if (mCinematicCameraDebugActive)
    {
        return;
    }

    bool hideForOverlay = false;
    bool showGoldPrompt = false;
    if (auto* uiManager = mGame->GetUIManager())
    {
        hideForOverlay =
            uiManager->IsRespawnScreenActive() ||
            uiManager->IsStageClearScreenActive();
    }

    if (!hideForOverlay)
    {
        if (Player* player = mGame->GetPlayer())
        {
            if (!player->IsDead() && !mChatController.IsChatting())
            {
                showGoldPrompt = IsPlayerNearUncollectedGold(player->GetPosition());
            }
        }
    }

    if (!hideForOverlay)
    {
        mDamageTextRenderer.Draw();
    }

    mBossController.Draw();
    if (auto* uiManager = mGame->GetUIManager())
    {
        uiManager->DrawCooldownOverlay();
    }
    if (!hideForOverlay)
    {
        if (!mReturnToVillageConfirmActive && showGoldPrompt)
        {
            mChatController.Draw(true, false, L"[ F ] 획득하기");
        }
        else if (!mReturnToVillageConfirmActive)
        {
            mChatController.Draw();
        }
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
                attack.rotY + DirectX::XM_PI,
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
        else if (playerClass == PlayerClass::Mage && attack.skillType == 1)
        {
            mSkillEffectManager.OnRemoteSkillCast(
                playerClass,
                attack.skillType,
                { attack.x, attack.y, attack.z },
                { attack.effectX, attack.effectY, attack.effectZ },
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
                (std::max)(attack.effectDelay, 0.0f),
                ArcherAnimationTiming::kSkillEArrowFallDurationSeconds);
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
    if (mReturnToVillageConfirmActive)
    {
        return;
    }
    mChatController.OnCharInput(charCode);
}

void Stage2Scene::OnTextInput(const std::wstring& text)
{
    if (mReturnToVillageConfirmActive)
    {
        return;
    }
    mChatController.OnTextInput(text);
}

void Stage2Scene::OnCompositionInput(const std::wstring& text, bool isFinal)
{
    if (mReturnToVillageConfirmActive)
    {
        return;
    }
    mChatController.OnCompositionInput(text, isFinal);
}
