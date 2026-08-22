#include "DebugColliderVisualizer.h"
#include "EclipseWalkerGame.h"
#include "GameObject.h"
#include "Material.h"
#include "RenderItem.h"
#include "ResourceManager.h"

#include <algorithm>
#include <memory>

using namespace DirectX;

void DebugColliderVisualizer::Reset()
{
    mObjects.clear();
    mRenderItems.clear();
}

bool DebugColliderVisualizer::EnsureMaterial(
    EclipseWalkerGame* game,
    const std::string& materialName,
    const XMFLOAT4& color) const
{
    if (game == nullptr || materialName.empty())
    {
        return false;
    }

    ResourceManager* resources = game->GetResources();
    if (resources == nullptr)
    {
        return false;
    }

    if (resources->GetMaterial(materialName) == nullptr)
    {
        resources->CreateMaterial(
            materialName,
            static_cast<int>(resources->mMaterials.size()),
            "white",
            "",
            "",
            "",
            color,
            { 0.02f, 0.02f, 0.02f },
            0.65f);
    }

    Material* material = resources->GetMaterial(materialName);
    if (material == nullptr)
    {
        return false;
    }

    material->DiffuseAlbedo = color;
    material->FresnelR0 = { 0.02f, 0.02f, 0.02f };
    material->Roughness = 0.65f;
    material->IsTransparent = 1;
    material->IsToon = 0;
    material->NumFramesDirty = gNumFrameResources;
    return true;
}

bool DebugColliderVisualizer::EnsureCapacity(
    EclipseWalkerGame* game,
    size_t targetCount,
    const TrackOwnedCallback& trackOwned)
{
    if (game == nullptr || trackOwned == nullptr)
    {
        return false;
    }

    ResourceManager* resources = game->GetResources();
    if (resources == nullptr)
    {
        return false;
    }

    auto geoIt = resources->mGeometries.find("boxGeo");
    if (geoIt == resources->mGeometries.end() || geoIt->second == nullptr)
    {
        return false;
    }

    EnsureMaterial(game, "DebugColliderFallbackMat", { 1.0f, 1.0f, 1.0f, 0.22f });
    Material* fallbackMaterial = resources->GetMaterial("DebugColliderFallbackMat");
    if (fallbackMaterial == nullptr)
    {
        return false;
    }

    auto& ritems = game->GetRitems();
    auto& objects = game->GetGameObjects();
    const auto& boxArgs = geoIt->second->DrawArgs["box"];

    while (mObjects.size() < targetCount)
    {
        auto renderItem = std::make_unique<RenderItem>();
        renderItem->World = MathHelper::Identity4x4();
        renderItem->TexTransform = MathHelper::Identity4x4();
        renderItem->ObjCBIndex = static_cast<UINT>(ritems.size());
        renderItem->Geo = geoIt->second.get();
        renderItem->Mat = fallbackMaterial;
        renderItem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        renderItem->IndexCount = boxArgs.IndexCount;
        renderItem->StartIndexLocation = boxArgs.StartIndexLocation;
        renderItem->BaseVertexLocation = boxArgs.BaseVertexLocation;
        renderItem->Visible = false;
        renderItem->ColorMultiplier = { 1.0f, 1.0f, 1.0f, 1.0f };

        auto object = std::make_unique<GameObject>();
        object->Ritem = renderItem.get();
        object->SetScale(0.0f, 0.0f, 0.0f);
        object->Update();

        mObjects.push_back(object.get());
        mRenderItems.push_back(renderItem.get());
        trackOwned(object.get(), renderItem.get());
        ritems.push_back(std::move(renderItem));
        objects.push_back(std::move(object));
    }

    return true;
}

void DebugColliderVisualizer::Update(
    EclipseWalkerGame* game,
    const std::vector<Target>& targets,
    const TrackOwnedCallback& trackOwned)
{
    if (!EnsureCapacity(game, targets.size(), trackOwned))
    {
        return;
    }

    ResourceManager* resources = game->GetResources();
    for (const Target& target : targets)
    {
        EnsureMaterial(game, target.MaterialName, target.Color);
    }

    for (size_t i = 0; i < mObjects.size(); ++i)
    {
        GameObject* object = mObjects[i];
        RenderItem* renderItem = mRenderItems[i];
        if (object == nullptr || renderItem == nullptr)
        {
            continue;
        }

        if (i >= targets.size())
        {
            renderItem->Visible = false;
            renderItem->NumFramesDirty = gNumFrameResources;
            continue;
        }

        const Target& target = targets[i];
        const bool hasSize =
            target.Extents.x > 0.001f &&
            target.Extents.y > 0.001f &&
            target.Extents.z > 0.001f;
        if (!target.Visible || !hasSize)
        {
            renderItem->Visible = false;
            renderItem->NumFramesDirty = gNumFrameResources;
            continue;
        }

        Material* material = resources != nullptr ? resources->GetMaterial(target.MaterialName) : nullptr;
        if (material != nullptr)
        {
            renderItem->Mat = material;
        }

        object->SetScale(target.Extents.x, target.Extents.y, target.Extents.z);
        object->SetPosition(target.Center.x, target.Center.y, target.Center.z);
        object->SetRotation(target.Rotation.x, target.Rotation.y, target.Rotation.z);
        object->Update();

        renderItem->Visible = true;
        renderItem->ColorMultiplier = { 1.0f, 1.0f, 1.0f, 1.0f };
        renderItem->NumFramesDirty = gNumFrameResources;
    }
}
