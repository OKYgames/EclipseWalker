#pragma once

#include "d3dUtil.h"
#include "RenderItem.h"
#include <vector>

class GameObject;

enum class DoorState
{
    Closed,
    Opening,
    Open,
    Closing
};

struct DoorBounds
{
    DirectX::XMFLOAT3 Min = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 Max = { 0.0f, 0.0f, 0.0f };
};

class InteractiveDoor
{
public:
    void Initialize(
        const std::vector<RenderItem*>& renderItems,
        const std::vector<GameObject*>& gameObjects,
        const DoorBounds& visualLocalBounds,
        const DoorBounds& worldCollisionBounds,
        float worldScale);

    void Update(float dt);
    bool TryInteract(const DirectX::XMFLOAT3& playerPosition);
    bool IsPlayerInRange(const DirectX::XMFLOAT3& playerPosition) const;
    bool IsOpenOrOpening() const;
    void SetOpen(bool open);
    bool HasBeenOpened() const { return mHasBeenOpened; }
    bool ResolvePlayerCollision(
        const DirectX::XMFLOAT3& oldPosition,
        const DirectX::XMFLOAT3& currentPosition,
        DirectX::XMFLOAT3& resolvedPosition) const;

    bool IsBlocking() const;

private:
    DirectX::XMMATRIX BuildWorldMatrix() const;
    void ApplyWorldMatrix();
    bool OverlapsPlayer(const DirectX::XMFLOAT3& position) const;
    DirectX::XMFLOAT3 GetCollisionCenter() const;

private:
    std::vector<RenderItem*> mRenderItems;
    std::vector<GameObject*> mGameObjects;
    DoorBounds mVisualLocalBounds;
    DoorBounds mCollisionBounds;
    DirectX::XMFLOAT3 mPivotLocal = { 0.0f, 0.0f, 0.0f };

    DoorState mState = DoorState::Closed;
    float mOpenAmount = 0.0f;
    float mOpenSpeed = 1.8f;
    float mClosedAngle = 0.0f;
    float mOpenAngle = -DirectX::XM_PIDIV2;
    float mWorldScale = 1.0f;
    float mInteractRange = 1.6f;
    float mPlayerRadius = 0.18f;
    bool mHasBeenOpened = false;
};
