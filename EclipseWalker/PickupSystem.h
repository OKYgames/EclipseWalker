#pragma once

#include "d3dUtil.h"
#include "GameTimer.h"
#include <unordered_set>
#include <vector>

class EclipseWalkerGame;
class GameObject;
class Monster;
class Player;
struct RenderItem;

class PickupSystem
{
public:
    explicit PickupSystem(EclipseWalkerGame* game);

    void Initialize();
    void Reset();
    void Update(const GameTimer& gt, Player* player, const std::vector<Monster*>& monsters);
    void Clear();

private:
    struct PickupInstance
    {
        GameObject* object = nullptr;
        RenderItem* renderItem = nullptr;
        DirectX::XMFLOAT3 basePosition = { 0.0f, 0.0f, 0.0f };
        float bobTime = 0.0f;
        bool collected = false;
    };

private:
    void EnsureResources();
    void SpawnBattery(const DirectX::XMFLOAT3& position);
    void UpdatePickupMotion(PickupInstance& pickup, float dt);
    void TryCollectPickup(PickupInstance& pickup, Player* player);

private:
    EclipseWalkerGame* mGame = nullptr;
    std::vector<PickupInstance> mPickups;
    std::unordered_set<const Monster*> mProcessedDeadMonsters;

    std::vector<GameObject*> mOwnedObjects;
    std::vector<RenderItem*> mOwnedRenderItems;

    float mPickupChargeAmount = 35.0f;
    float mCollectRadius = 1.1f;
    bool mInitialized = false;
};
