#include "PickupSystem.h"

#include "EclipseWalkerGame.h"
#include "GameObject.h"
#include "Monster.h"
#include "Player.h"
#include "RenderItem.h"
#include "ResourceManager.h"
#include <Windows.h>
#include <algorithm>
#include <cmath>

using namespace DirectX;

PickupSystem::PickupSystem(EclipseWalkerGame* game)
    : mGame(game)
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
}

void PickupSystem::Update(const GameTimer& gt, Player* player, const std::vector<Monster*>& monsters)
{
    if (!mInitialized || player == nullptr)
    {
        return;
    }

    for (Monster* monster : monsters)
    {
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

        SpawnBattery(monster->GetPosition());
    }

    for (auto& pickup : mPickups)
    {
        if (pickup.collected || pickup.object == nullptr || pickup.renderItem == nullptr)
        {
            continue;
        }

        UpdatePickupMotion(pickup, gt.DeltaTime());
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

    if (resources->GetMaterial("LanternBatteryMat") != nullptr)
    {
        return;
    }

    resources->CreateMaterial(
        "LanternBatteryMat",
        resources->mMaterials.size(),
        "white",
        "",
        "",
        "",
        XMFLOAT4(0.95f, 0.78f, 0.24f, 1.0f),
        XMFLOAT3(0.45f, 0.32f, 0.08f),
        0.18f);

    if (auto* material = resources->GetMaterial("LanternBatteryMat"))
    {
        material->NumFramesDirty = 3;
        material->IsTransparent = 0;
        material->IsToon = 0;
    }
}

void PickupSystem::SpawnBattery(const XMFLOAT3& position)
{
    auto& ritems = mGame->GetRitems();
    auto& objs = mGame->GetGameObjects();
    auto* resources = mGame->GetResources();

    auto renderItem = std::make_unique<RenderItem>();
    renderItem->World = MathHelper::Identity4x4();
    renderItem->TexTransform = MathHelper::Identity4x4();
    renderItem->Geo = resources->mGeometries["sphereGeo"].get();
    renderItem->Mat = resources->GetMaterial("LanternBatteryMat");
    renderItem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    renderItem->ObjCBIndex = static_cast<UINT>(ritems.size());

    auto& drawArgs = renderItem->Geo->DrawArgs["sphere"];
    renderItem->IndexCount = drawArgs.IndexCount;
    renderItem->StartIndexLocation = drawArgs.StartIndexLocation;
    renderItem->BaseVertexLocation = drawArgs.BaseVertexLocation;
    renderItem->Visible = true;

    auto object = std::make_unique<GameObject>();
    object->Ritem = renderItem.get();
    object->SetScale(0.18f, 0.24f, 0.18f);
    object->SetPosition(position.x, position.y + 0.55f, position.z);
    object->Update();

    PickupInstance pickup;
    pickup.object = object.get();
    pickup.renderItem = renderItem.get();
    pickup.basePosition = { position.x, position.y + 0.55f, position.z };
    pickup.bobTime = static_cast<float>(mPickups.size()) * 0.45f;

    mOwnedObjects.push_back(object.get());
    mOwnedRenderItems.push_back(renderItem.get());

    ritems.push_back(std::move(renderItem));
    objs.push_back(std::move(object));
    mPickups.push_back(pickup);

    OutputDebugStringA("[PickupSystem] 랜턴 배터리 드랍\n");
}

void PickupSystem::UpdatePickupMotion(PickupInstance& pickup, float dt)
{
    pickup.bobTime += dt;

    const float bobOffset = sinf(pickup.bobTime * 2.6f) * 0.12f;
    const float rotationY = pickup.bobTime * 1.8f;

    pickup.object->SetPosition(pickup.basePosition.x, pickup.basePosition.y + bobOffset, pickup.basePosition.z);
    pickup.object->SetRotation(0.0f, rotationY, 0.0f);
    pickup.object->Update();
}

void PickupSystem::TryCollectPickup(PickupInstance& pickup, Player* player)
{
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

    if (player->GetLantern() != nullptr)
    {
        player->GetLantern()->AddCharge(mPickupChargeAmount);
    }

    pickup.collected = true;
    pickup.renderItem->Visible = false;

    OutputDebugStringA("[PickupSystem] 랜턴 배터리 획득\n");
}
