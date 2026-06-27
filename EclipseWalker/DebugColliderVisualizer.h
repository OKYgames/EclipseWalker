#pragma once

#include <DirectXMath.h>
#include <functional>
#include <string>
#include <vector>

class EclipseWalkerGame;
class GameObject;
struct RenderItem;

class DebugColliderVisualizer
{
public:
    struct Target
    {
        DirectX::XMFLOAT3 Center = { 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT3 Extents = { 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT3 Rotation = { 0.0f, 0.0f, 0.0f };
        std::string MaterialName;
        DirectX::XMFLOAT4 Color = { 1.0f, 1.0f, 1.0f, 0.25f };
        bool Visible = true;
    };

    using TrackOwnedCallback = std::function<void(GameObject*, RenderItem*)>;

    void Reset();
    void Update(EclipseWalkerGame* game, const std::vector<Target>& targets, const TrackOwnedCallback& trackOwned);

private:
    bool EnsureMaterial(EclipseWalkerGame* game, const std::string& materialName, const DirectX::XMFLOAT4& color) const;
    bool EnsureCapacity(EclipseWalkerGame* game, size_t targetCount, const TrackOwnedCallback& trackOwned);

private:
    std::vector<GameObject*> mObjects;
    std::vector<RenderItem*> mRenderItems;
};
