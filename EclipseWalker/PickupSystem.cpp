#include "PickupSystem.h"

#include "EclipseWalkerGame.h"
#include "GameObject.h"
#include "LanternSystem.h"
#include "MapSystem.h"
#include "Monster.h"
#include "NetworkManager.h"
#include "Player.h"
#include "RenderItem.h"
#include "ResourceManager.h"
#include <Windows.h>
#include <algorithm>
#include <cmath>

using namespace DirectX;

PickupSystem::PickupSystem(EclipseWalkerGame* game, LanternSystem* lanternSystem)
    : mGame(game)
    , mLanternSystem(lanternSystem)
{
}

void PickupSystem::Initialize()
{
    EnsureResources();
    mInitialized = true;
}

void PickupSystem::Reset()
{
    Clear();
    mProcessedDeadMonsters.clear();
    mCollectedPickupIds.clear();
    mPendingPickupIds.clear();
}

void PickupSystem::Update(const GameTimer& gt, Player* player, MapSystem* mapSystem, const std::vector<Monster*>& monsters)
{
    if (!mInitialized || player == nullptr)
    {
        return;
    }

    for (const PKT_S_PICKUP_COLLECTED& pickupCollected : NetworkManager::Get()->PopPickupCollected())
    {
        ApplyPickupCollected(
            pickupCollected.pickupId,
            player,
            pickupCollected.playerId == NetworkManager::Get()->m_myPlayerId);
    }

    for (size_t i = 0; i < monsters.size(); ++i)
    {
        Monster* monster = monsters[i];
        if (monster == nullptr)
        {
            continue;
        }

        if (monster->GetState() != MonsterState::DIE)
        {
            continue;
        }

        if (!mProcessedDeadMonsters.insert(monster).second)
        {
            continue;
        }

        if (monster->Ritem != nullptr)
        {
            monster->Ritem->Visible = false;
        }

        const int pickupId = static_cast<int>(i) + 1;
        if (mCollectedPickupIds.find(pickupId) == mCollectedPickupIds.end())
        {
            SpawnBattery(pickupId, monster->GetPosition());
        }
    }

    for (auto& pickup : mPickups)
    {
        if (pickup.collected || pickup.object == nullptr || pickup.renderItem == nullptr)
        {
            continue;
        }

        UpdatePickupMotion(pickup, gt.DeltaTime(), mapSystem);
        TryCollectPickup(pickup, player);
    }
}

void PickupSystem::Clear()
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

    for (UINT i = 0; i < ritems.size(); ++i)
    {
        ritems[i]->ObjCBIndex = i;
        ritems[i]->NumFramesDirty = 3;
    }

    mPickups.clear();
    mOwnedObjects.clear();
    mOwnedRenderItems.clear();
}

void PickupSystem::EnsureResources()
{
    auto* resources = mGame->GetResources();

    if (resources->GetMaterial("LanternSoulCoreMat") != nullptr &&
        resources->GetMaterial("LanternSoulShellMat") != nullptr)
    {
        return;
    }

    resources->CreateMaterial(
        "LanternSoulCoreMat",
        static_cast<int>(resources->mMaterials.size()),
        "white",
        "",
        "",
        "",
        XMFLOAT4(0.32f, 1.0f, 0.42f, 1.0f),
        XMFLOAT3(0.06f, 0.24f, 0.08f),
        0.08f);

    if (auto* material = resources->GetMaterial("LanternSoulCoreMat"))
    {
        material->NumFramesDirty = 3;
        material->IsTransparent = 0;
        material->IsToon = 0;
    }

    resources->CreateMaterial(
        "LanternSoulShellMat",
        static_cast<int>(resources->mMaterials.size()),
        "white",
        "",
        "",
        "",
        XMFLOAT4(0.18f, 0.92f, 0.28f, 0.26f),
        XMFLOAT3(0.04f, 0.16f, 0.06f),
        0.02f);

    if (auto* material = resources->GetMaterial("LanternSoulShellMat"))
    {
        material->NumFramesDirty = 3;
        material->IsTransparent = 1;
        material->IsToon = 0;
    }
}

void PickupSystem::SpawnBattery(int pickupId, const XMFLOAT3& position)
{
    auto& ritems = mGame->GetRitems();
    auto& objs = mGame->GetGameObjects();
    auto* resources = mGame->GetResources();

    auto renderItem = std::make_unique<RenderItem>();
    renderItem->World = MathHelper::Identity4x4();
    renderItem->TexTransform = MathHelper::Identity4x4();
    renderItem->Geo = resources->mGeometries["sphereGeo"].get();
    renderItem->Mat = resources->GetMaterial("LanternSoulCoreMat");
    renderItem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    renderItem->ObjCBIndex = static_cast<UINT>(ritems.size());

    auto& drawArgs = renderItem->Geo->DrawArgs["sphere"];
    renderItem->IndexCount = drawArgs.IndexCount;
    renderItem->StartIndexLocation = drawArgs.StartIndexLocation;
    renderItem->BaseVertexLocation = drawArgs.BaseVertexLocation;
    renderItem->Visible = true;

    auto object = std::make_unique<GameObject>();
    object->Ritem = renderItem.get();
    object->SetScale(0.14f, 0.14f, 0.14f);
    object->SetPosition(position.x, position.y + 0.60f, position.z);
    object->Update();

    PickupInstance pickup;
    pickup.pickupId = pickupId;
    pickup.object = object.get();
    pickup.renderItem = renderItem.get();
    pickup.basePosition = { position.x, position.y + 0.60f, position.z };
    pickup.baseScale = { 0.14f, 0.14f, 0.14f };
    pickup.verticalVelocity = 0.0f;
    pickup.groundY = -9999.0f;
    pickup.hoverHeight = 0.10f;
    pickup.bobTime = static_cast<float>(mPickups.size()) * 0.45f;
    pickup.pulseOffset = static_cast<float>(mPickups.size()) * 0.73f;

    mOwnedObjects.push_back(object.get());
    mOwnedRenderItems.push_back(renderItem.get());

    ritems.push_back(std::move(renderItem));
    objs.push_back(std::move(object));
    mPickups.push_back(pickup);

    PickupInstance& createdPickup = mPickups.back();
    for (int i = 0; i < kShellLayerCount; ++i)
    {
        SpawnShellLayer(createdPickup, i);
    }

    OutputDebugStringA("[PickupSystem] 영혼형 랜턴 배터리 드랍\n");
}

void PickupSystem::SpawnShellLayer(PickupInstance& pickup, int layerIndex)
{
    if (layerIndex < 0 || layerIndex >= kShellLayerCount)
    {
        return;
    }

    auto& ritems = mGame->GetRitems();
    auto& objs = mGame->GetGameObjects();
    auto* resources = mGame->GetResources();

    auto renderItem = std::make_unique<RenderItem>();
    renderItem->World = MathHelper::Identity4x4();
    renderItem->TexTransform = MathHelper::Identity4x4();
    renderItem->Geo = resources->mGeometries["sphereGeo"].get();
    renderItem->Mat = resources->GetMaterial("LanternSoulShellMat");
    renderItem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    renderItem->ObjCBIndex = static_cast<UINT>(ritems.size());

    auto& drawArgs = renderItem->Geo->DrawArgs["sphere"];
    renderItem->IndexCount = drawArgs.IndexCount;
    renderItem->StartIndexLocation = drawArgs.StartIndexLocation;
    renderItem->BaseVertexLocation = drawArgs.BaseVertexLocation;
    renderItem->Visible = true;

    auto object = std::make_unique<GameObject>();
    object->Ritem = renderItem.get();
    object->SetScale(
        pickup.shellBaseScales[layerIndex].x,
        pickup.shellBaseScales[layerIndex].x,
        pickup.shellBaseScales[layerIndex].z);
    object->SetPosition(pickup.basePosition.x, pickup.basePosition.y, pickup.basePosition.z);
    object->Update();

    pickup.shellObjects[layerIndex] = object.get();
    pickup.shellRenderItems[layerIndex] = renderItem.get();

    mOwnedObjects.push_back(object.get());
    mOwnedRenderItems.push_back(renderItem.get());

    ritems.push_back(std::move(renderItem));
    objs.push_back(std::move(object));
}

void PickupSystem::UpdatePickupMotion(PickupInstance& pickup, float dt, MapSystem* mapSystem)
{
    pickup.bobTime += dt;

    if (!pickup.landed && mapSystem != nullptr)
    {
        const float floorY = mapSystem->GetFloorHeight(
            pickup.basePosition.x,
            pickup.basePosition.z,
            pickup.basePosition.y + 2.0f,
            8.0f);

        pickup.verticalVelocity -= 14.0f * dt;
        pickup.basePosition.y += pickup.verticalVelocity * dt;

        if (floorY > -9000.0f)
        {
            const float restY = floorY + pickup.hoverHeight;
            if (pickup.basePosition.y <= restY)
            {
                pickup.basePosition.y = restY;
                pickup.groundY = floorY;
                pickup.verticalVelocity = 0.0f;
                pickup.landed = true;
            }
        }
    }
    else if (pickup.landed)
    {
        pickup.basePosition.y = pickup.groundY + pickup.hoverHeight;
    }

    const float pulse = 1.0f + (sinf((pickup.bobTime * 5.2f) + pickup.pulseOffset) * 0.08f);
    const float bobOffset = pickup.landed ? (sinf(pickup.bobTime * 2.8f) * 0.05f) : 0.0f;
    const float swayX = pickup.landed ? (sinf((pickup.bobTime * 1.6f) + pickup.pulseOffset) * 0.015f) : 0.0f;
    const float swayZ = pickup.landed ? (cosf((pickup.bobTime * 1.9f) + pickup.pulseOffset) * 0.015f) : 0.0f;
    const float rotationY = pickup.bobTime * 0.9f;

    pickup.object->SetPosition(
        pickup.basePosition.x + swayX,
        pickup.basePosition.y + bobOffset,
        pickup.basePosition.z + swayZ);
    pickup.object->SetScale(
        pickup.baseScale.x * pulse,
        pickup.baseScale.y * pulse,
        pickup.baseScale.z * pulse);
    pickup.object->SetRotation(0.0f, rotationY, 0.0f);
    pickup.object->Update();

    if (pickup.renderItem != nullptr)
    {
        const float coreGlow = 0.90f + (sinf((pickup.bobTime * 6.0f) + pickup.pulseOffset) * 0.10f);
        pickup.renderItem->ColorMultiplier = XMFLOAT4(coreGlow, coreGlow, coreGlow, 1.0f);
        pickup.renderItem->NumFramesDirty = 3;
    }

    for (int i = 0; i < kShellLayerCount; ++i)
    {
        GameObject* shellObject = pickup.shellObjects[i];
        RenderItem* shellRenderItem = pickup.shellRenderItems[i];
        if (shellObject == nullptr || shellRenderItem == nullptr)
        {
            continue;
        }

        const float layerPhase = pickup.pulseOffset + (i * 0.85f);
        const float layerPulse = 1.0f + (sinf((pickup.bobTime * (3.2f + i)) + layerPhase) * (0.12f + i * 0.04f));
        const float layerYOffset = (i + 1) * 0.01f;
        const float layerRotY = rotationY * (0.6f + i * 0.35f);
        const float alpha = 0.24f - (i * 0.06f) + (sinf((pickup.bobTime * 4.5f) + layerPhase) * 0.03f);

        shellObject->SetPosition(
            pickup.basePosition.x + swayX,
            pickup.basePosition.y + bobOffset + layerYOffset,
            pickup.basePosition.z + swayZ);
        shellObject->SetScale(
            pickup.shellBaseScales[i].x * layerPulse,
            pickup.shellBaseScales[i].y * layerPulse,
            pickup.shellBaseScales[i].z * layerPulse);
        shellObject->SetRotation(0.0f, layerRotY, 0.0f);
        shellObject->Update();

        shellRenderItem->ColorMultiplier = XMFLOAT4(0.56f, 1.0f, 0.60f, std::clamp(alpha, 0.08f, 0.28f));
        shellRenderItem->NumFramesDirty = 3;
    }
}

void PickupSystem::TryCollectPickup(PickupInstance& pickup, Player* player)
{
    if (mCollectedPickupIds.find(pickup.pickupId) != mCollectedPickupIds.end() ||
        mPendingPickupIds.find(pickup.pickupId) != mPendingPickupIds.end())
    {
        return;
    }

    const XMFLOAT3 playerPos = player->GetPosition();
    const XMFLOAT3 pickupPos = pickup.object->GetPosition();

    const float dx = playerPos.x - pickupPos.x;
    const float dy = playerPos.y - pickupPos.y;
    const float dz = playerPos.z - pickupPos.z;
    const float distanceSq = (dx * dx) + (dy * dy) + (dz * dz);

    if (distanceSq > (mCollectRadius * mCollectRadius))
    {
        return;
    }

    mPendingPickupIds.insert(pickup.pickupId);
    NetworkManager::Get()->SendPickupCollect(pickup.pickupId);
}

void PickupSystem::ApplyPickupCollected(int pickupId, Player* player, bool grantCharge)
{
    if (!mCollectedPickupIds.insert(pickupId).second)
    {
        return;
    }

    mPendingPickupIds.erase(pickupId);

    if (grantCharge && mLanternSystem != nullptr)
    {
        const float addedCharge = mLanternSystem->AddPickupCharge(player);
        if (addedCharge > 0.0f)
        {
            NetworkManager::Get()->SendLanternGauge(
                mLanternSystem->GetGauge(player),
                mLanternSystem->GetMaxGauge(player),
                mLanternSystem->GetLevel(player));
        }
    }

    for (auto& pickup : mPickups)
    {
        if (pickup.pickupId == pickupId)
        {
            HidePickup(pickup);
            break;
        }
    }

    OutputDebugStringA("[PickupSystem] Lantern battery collected sync\n");
}

void PickupSystem::HidePickup(PickupInstance& pickup)
{
    pickup.collected = true;
    if (pickup.renderItem != nullptr)
    {
        pickup.renderItem->Visible = false;
    }
    for (int i = 0; i < kShellLayerCount; ++i)
    {
        if (pickup.shellRenderItems[i] != nullptr)
        {
            pickup.shellRenderItems[i]->Visible = false;
        }
    }

    OutputDebugStringA("[PickupSystem] 랜턴 배터리 획득\n");
}
