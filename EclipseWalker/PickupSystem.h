#pragma once

#include "d3dUtil.h"
#include "GameTimer.h"
#include <unordered_set>
#include <vector>

class EclipseWalkerGame;
class GameObject;
class LanternSystem;
class MapSystem;
class Monster;
class Player;
struct RenderItem;

class PickupSystem
{
public:
    PickupSystem(EclipseWalkerGame* game, LanternSystem* lanternSystem);

    void Initialize();
    void Reset();
    void Update(const GameTimer& gt, Player* player, MapSystem* mapSystem, const std::vector<Monster*>& monsters);
    void Clear();

private:
    static constexpr int kShellLayerCount = 2;

    struct PickupInstance
    {
        GameObject* object = nullptr;
        RenderItem* renderItem = nullptr;
        DirectX::XMFLOAT3 basePosition = { 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT3 baseScale = { 0.14f, 0.14f, 0.14f };
        GameObject* shellObjects[kShellLayerCount] = { nullptr, nullptr };
        RenderItem* shellRenderItems[kShellLayerCount] = { nullptr, nullptr };
        DirectX::XMFLOAT3 shellBaseScales[kShellLayerCount] =
        {
            DirectX::XMFLOAT3(0.20f, 0.20f, 0.20f),
            DirectX::XMFLOAT3(0.28f, 0.28f, 0.28f)
        };
        float verticalVelocity = 0.0f;
        float groundY = -9999.0f;
        float hoverHeight = 0.10f;
        float bobTime = 0.0f;
        float pulseOffset = 0.0f;
        bool landed = false;
        bool collected = false;
    };

private:
    void EnsureResources();
    void SpawnBattery(const DirectX::XMFLOAT3& position);
    void SpawnShellLayer(PickupInstance& pickup, int layerIndex);
    void UpdatePickupMotion(PickupInstance& pickup, float dt, MapSystem* mapSystem);
    void TryCollectPickup(PickupInstance& pickup, Player* player);

private:
    EclipseWalkerGame* mGame = nullptr;
    LanternSystem* mLanternSystem = nullptr;
    std::vector<PickupInstance> mPickups;
    std::unordered_set<const Monster*> mProcessedDeadMonsters;

    std::vector<GameObject*> mOwnedObjects;
    std::vector<RenderItem*> mOwnedRenderItems;

    float mCollectRadius = 1.1f;
    bool mInitialized = false;
};
