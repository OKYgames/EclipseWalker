#include "InteractiveDoor.h"
#include "FrameResource.h"
#include "GameObject.h"
#include <algorithm>
#include <cmath>

using namespace DirectX;

namespace
{
    float Clamp01(float value)
    {
        return std::clamp(value, 0.0f, 1.0f);
    }
}

void InteractiveDoor::Initialize(
    const std::vector<RenderItem*>& renderItems,
    const std::vector<GameObject*>& gameObjects,
    const DoorBounds& visualLocalBounds,
    const DoorBounds& worldCollisionBounds,
    float worldScale)
{
    mRenderItems = renderItems;
    mGameObjects = gameObjects;
    mVisualLocalBounds = visualLocalBounds;
    mCollisionBounds = worldCollisionBounds;
    mWorldScale = worldScale;

    const float extentX = mVisualLocalBounds.Max.x - mVisualLocalBounds.Min.x;
    const float extentZ = mVisualLocalBounds.Max.z - mVisualLocalBounds.Min.z;

    mPivotLocal.y = 0.0f;
    if (extentX >= extentZ)
    {
        mPivotLocal.x = mVisualLocalBounds.Min.x;
        mPivotLocal.z = (mVisualLocalBounds.Min.z + mVisualLocalBounds.Max.z) * 0.5f;
    }
    else
    {
        mPivotLocal.x = (mVisualLocalBounds.Min.x + mVisualLocalBounds.Max.x) * 0.5f;
        mPivotLocal.z = mVisualLocalBounds.Min.z;
    }

    ApplyWorldMatrix();
}

void InteractiveDoor::Update(float dt)
{
    if (mState == DoorState::Opening)
    {
        mOpenAmount = Clamp01(mOpenAmount + mOpenSpeed * dt);
        if (mOpenAmount >= 1.0f)
        {
            mState = DoorState::Open;
        }
        ApplyWorldMatrix();
    }
    else if (mState == DoorState::Closing)
    {
        mOpenAmount = Clamp01(mOpenAmount - mOpenSpeed * dt);
        if (mOpenAmount <= 0.0f)
        {
            mState = DoorState::Closed;
        }
        ApplyWorldMatrix();
    }
}

bool InteractiveDoor::TryInteract(const XMFLOAT3& playerPosition)
{
    if (!IsPlayerInRange(playerPosition))
    {
        return false;
    }

    if (mState == DoorState::Closed || mState == DoorState::Closing)
    {
        mState = DoorState::Opening;
    }
    else
    {
        mState = DoorState::Closing;
    }

    return true;
}

bool InteractiveDoor::IsPlayerInRange(const XMFLOAT3& playerPosition) const
{
    const XMFLOAT3 center = GetCollisionCenter();
    const float dx = playerPosition.x - center.x;
    const float dz = playerPosition.z - center.z;
    return (dx * dx + dz * dz) <= (mInteractRange * mInteractRange);
}

bool InteractiveDoor::ResolvePlayerCollision(
    const XMFLOAT3& oldPosition,
    const XMFLOAT3& currentPosition,
    XMFLOAT3& resolvedPosition) const
{
    if (!IsBlocking() || !OverlapsPlayer(currentPosition))
    {
        return false;
    }

    if (!OverlapsPlayer(oldPosition))
    {
        resolvedPosition = oldPosition;
        return true;
    }

    resolvedPosition = currentPosition;

    const float leftPen = std::abs((currentPosition.x + mPlayerRadius) - mCollisionBounds.Min.x);
    const float rightPen = std::abs(mCollisionBounds.Max.x - (currentPosition.x - mPlayerRadius));
    const float frontPen = std::abs((currentPosition.z + mPlayerRadius) - mCollisionBounds.Min.z);
    const float backPen = std::abs(mCollisionBounds.Max.z - (currentPosition.z - mPlayerRadius));

    const float minPen = (std::min)((std::min)(leftPen, rightPen), (std::min)(frontPen, backPen));
    if (minPen == leftPen)
    {
        resolvedPosition.x = mCollisionBounds.Min.x - mPlayerRadius;
    }
    else if (minPen == rightPen)
    {
        resolvedPosition.x = mCollisionBounds.Max.x + mPlayerRadius;
    }
    else if (minPen == frontPen)
    {
        resolvedPosition.z = mCollisionBounds.Min.z - mPlayerRadius;
    }
    else
    {
        resolvedPosition.z = mCollisionBounds.Max.z + mPlayerRadius;
    }

    return true;
}

bool InteractiveDoor::IsBlocking() const
{
    return mOpenAmount < 0.65f;
}

XMMATRIX InteractiveDoor::BuildWorldMatrix() const
{
    const float angle = mClosedAngle + (mOpenAngle * mOpenAmount);
    return XMMatrixTranslation(-mPivotLocal.x, -mPivotLocal.y, -mPivotLocal.z) *
        XMMatrixRotationY(angle) *
        XMMatrixTranslation(mPivotLocal.x, mPivotLocal.y, mPivotLocal.z) *
        XMMatrixScaling(mWorldScale, mWorldScale, mWorldScale);
}

void InteractiveDoor::ApplyWorldMatrix()
{
    const XMMATRIX world = BuildWorldMatrix();
    XMFLOAT4X4 worldFloat;
    XMStoreFloat4x4(&worldFloat, world);

    for (auto* renderItem : mRenderItems)
    {
        if (renderItem == nullptr) continue;

        renderItem->World = worldFloat;
        renderItem->NumFramesDirty = gNumFrameResources;
    }

    for (auto* object : mGameObjects)
    {
        if (object == nullptr) continue;
        object->SetWorldTransform(world);
    }
}

bool InteractiveDoor::OverlapsPlayer(const XMFLOAT3& position) const
{
    return position.x + mPlayerRadius > mCollisionBounds.Min.x &&
        position.x - mPlayerRadius < mCollisionBounds.Max.x &&
        position.z + mPlayerRadius > mCollisionBounds.Min.z &&
        position.z - mPlayerRadius < mCollisionBounds.Max.z &&
        position.y > mCollisionBounds.Min.y - 1.0f &&
        position.y < mCollisionBounds.Max.y + 2.0f;
}

XMFLOAT3 InteractiveDoor::GetCollisionCenter() const
{
    return {
        (mCollisionBounds.Min.x + mCollisionBounds.Max.x) * 0.5f,
        (mCollisionBounds.Min.y + mCollisionBounds.Max.y) * 0.5f,
        (mCollisionBounds.Min.z + mCollisionBounds.Max.z) * 0.5f
    };
}
