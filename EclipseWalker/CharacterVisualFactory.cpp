#include "CharacterVisualFactory.h"

#include "SkeletalAnimationComponent.h"
#include "SkinnedMeshBuilder.h"
#include <Windows.h>
#include <filesystem>
#include <sstream>

using namespace DirectX;

namespace
{
    Material* EnsureMaterial(ResourceManager* resources, const CharacterVisualSpec& spec)
    {
        if (resources == nullptr || spec.MaterialName.empty())
        {
            return nullptr;
        }

        if (!spec.DiffuseTextureName.empty() &&
            !spec.DiffuseTexturePath.empty() &&
            resources->GetTexture(spec.DiffuseTextureName) == nullptr &&
            std::filesystem::exists(spec.DiffuseTexturePath))
        {
            resources->LoadTexture(spec.DiffuseTextureName, spec.DiffuseTexturePath);
        }

        const std::string diffuseTexture =
            (resources->GetTexture(spec.DiffuseTextureName) != nullptr) ? spec.DiffuseTextureName : "white";

        if (resources->GetMaterial(spec.MaterialName) == nullptr)
        {
            resources->CreateMaterial(
                spec.MaterialName,
                static_cast<int>(resources->mMaterials.size()),
                diffuseTexture,
                "",
                "",
                "",
                spec.DiffuseAlbedo,
                spec.FresnelR0,
                spec.Roughness);
        }

        Material* material = resources->GetMaterial(spec.MaterialName);
        if (material != nullptr)
        {
            material->DiffuseMapName = diffuseTexture;
            material->DiffuseAlbedo = spec.DiffuseAlbedo;
            material->FresnelR0 = spec.FresnelR0;
            material->Roughness = spec.Roughness;
            material->IsTransparent = 0;
            material->IsToon = spec.IsToon ? 1 : 0;
            material->OutlineThickness = spec.OutlineThickness;
            material->OutlineColor = spec.OutlineColor;
            material->NumFramesDirty = gNumFrameResources;
        }

        return material;
    }

    void ApplyGroundedPlacement(GameObject* object, const SubmeshGeometry& submesh, const CharacterVisualSpec& spec)
    {
        constexpr float kMinHeight = 0.001f;

        const float rawHeight = (std::max)(kMinHeight, submesh.Bounds.Extents.y * 2.0f);
        const float uniformScale = spec.TargetHeight / rawHeight;
        const float minY = submesh.Bounds.Center.y - submesh.Bounds.Extents.y;
        const float centerX = submesh.Bounds.Center.x;
        const float centerZ = submesh.Bounds.Center.z;

        object->SetScale(uniformScale, uniformScale, uniformScale);
        object->SetRotationOffset(spec.RotationOffset.x, spec.RotationOffset.y, spec.RotationOffset.z);
        if (spec.UseActorOrigin)
        {
            object->SetPositionOffset(
                -centerX * uniformScale,
                -minY * uniformScale - spec.OriginToFloor,
                -centerZ * uniformScale);
            object->SetPosition(
                spec.SpawnPosition.x,
                spec.SpawnPosition.y,
                spec.SpawnPosition.z);
        }
        else
        {
            object->SetPositionOffset(0.0f, 0.0f, 0.0f);
            object->SetPosition(
                spec.SpawnPosition.x - centerX * uniformScale,
                spec.SpawnPosition.y - minY * uniformScale,
                spec.SpawnPosition.z - centerZ * uniformScale);
        }
        object->Update();
    }
}

bool CharacterVisualFactory::ApplyVisual(
    GameObject* object,
    RenderItem* renderItem,
    ID3D12Device* device,
    ID3D12GraphicsCommandList* cmdList,
    ResourceManager* resources,
    const CharacterVisualSpec& spec)
{
    if (object == nullptr || renderItem == nullptr || resources == nullptr)
    {
        return false;
    }

    if (spec.UseSkinned && ApplySkinnedVisual(object, renderItem, device, cmdList, resources, spec))
    {
        return true;
    }

    return ApplyFallbackVisual(object, renderItem, resources, spec);
}

bool CharacterVisualFactory::ApplySkinnedVisual(
    GameObject* object,
    RenderItem* renderItem,
    ID3D12Device* device,
    ID3D12GraphicsCommandList* cmdList,
    ResourceManager* resources,
    const CharacterVisualSpec& spec)
{
    if (device == nullptr || cmdList == nullptr || spec.ModelPath.empty() || spec.GeometryName.empty())
    {
        return false;
    }

    if (!std::filesystem::exists(spec.ModelPath))
    {
        std::ostringstream missingLog;
        missingLog << "[CharacterVisualFactory] Missing skinned model: " << spec.ModelPath << "\n";
        OutputDebugStringA(missingLog.str().c_str());
        return false;
    }

    Material* material = EnsureMaterial(resources, spec);
    if (material == nullptr)
    {
        return false;
    }

    object->Ritem = renderItem;
    auto* animation = object->CreateSkeletalAnimationComponent();
    if (animation == nullptr || !animation->Load(spec.ModelPath, spec.DefaultClipName, spec.LoadModelAnimations))
    {
        std::ostringstream loadLog;
        loadLog << "[CharacterVisualFactory] Failed to load animation: " << spec.ModelPath << "\n";
        OutputDebugStringA(loadLog.str().c_str());
        return false;
    }

    for (const auto& clipSpec : spec.AdditionalAnimationClips)
    {
        if (clipSpec.FilePath.empty())
        {
            continue;
        }

        if (!std::filesystem::exists(clipSpec.FilePath))
        {
            std::ostringstream missingClipLog;
            missingClipLog << "[CharacterVisualFactory] Missing animation clip: " << clipSpec.FilePath << "\n";
            OutputDebugStringA(missingClipLog.str().c_str());
            continue;
        }

        if (!animation->LoadAdditionalAnimation(clipSpec.FilePath, clipSpec.ClipName))
        {
            std::ostringstream clipLoadLog;
            clipLoadLog << "[CharacterVisualFactory] Failed to load animation clip: " << clipSpec.FilePath << "\n";
            OutputDebugStringA(clipLoadLog.str().c_str());
        }
    }

    if (resources->mGeometries.find(spec.GeometryName) == resources->mGeometries.end())
    {
        auto geometry = SkinnedMeshBuilder::BuildMeshGeometry(
            device,
            cmdList,
            spec.GeometryName,
            animation->GetLoader());

        if (geometry == nullptr)
        {
            return false;
        }

        resources->mGeometries[geometry->Name] = std::move(geometry);
    }

    auto* geometry = resources->mGeometries[spec.GeometryName].get();
    auto geometryIt = geometry->DrawArgs.find("skinnedMesh");
    if (geometry == nullptr || geometryIt == geometry->DrawArgs.end())
    {
        return false;
    }

    const auto& submesh = geometryIt->second;
    renderItem->Geo = geometry;
    renderItem->Mat = material;
    renderItem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    renderItem->IndexCount = submesh.IndexCount;
    renderItem->StartIndexLocation = submesh.StartIndexLocation;
    renderItem->BaseVertexLocation = submesh.BaseVertexLocation;
    renderItem->IsSkinned = true;
    renderItem->SkinnedCBIndex = renderItem->ObjCBIndex;
    renderItem->Visible = true;

    ApplyGroundedPlacement(object, submesh, spec);
    return true;
}

bool CharacterVisualFactory::ApplyFallbackVisual(
    GameObject* object,
    RenderItem* renderItem,
    ResourceManager* resources,
    const CharacterVisualSpec& spec)
{
    auto geoIt = resources->mGeometries.find(spec.FallbackGeometryName);
    Material* material = resources->GetMaterial(spec.FallbackMaterialName);
    if (geoIt == resources->mGeometries.end() || material == nullptr)
    {
        return false;
    }

    material->IsToon = spec.IsToon ? 1 : 0;
    material->OutlineThickness = spec.OutlineThickness;
    material->OutlineColor = spec.OutlineColor;
    material->NumFramesDirty = gNumFrameResources;

    auto* geometry = geoIt->second.get();
    auto submeshIt = geometry->DrawArgs.find(spec.FallbackSubmeshName);
    if (submeshIt == geometry->DrawArgs.end())
    {
        return false;
    }

    const auto& submesh = submeshIt->second;
    renderItem->Geo = geometry;
    renderItem->Mat = material;
    renderItem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    renderItem->IndexCount = submesh.IndexCount;
    renderItem->StartIndexLocation = submesh.StartIndexLocation;
    renderItem->BaseVertexLocation = submesh.BaseVertexLocation;
    renderItem->IsSkinned = false;
    renderItem->SkinnedCBIndex = UINT(-1);
    renderItem->Visible = true;

    object->Ritem = renderItem;
    object->SetScale(spec.FallbackScale.x, spec.FallbackScale.y, spec.FallbackScale.z);
    object->SetRotationOffset(spec.RotationOffset.x, spec.RotationOffset.y, spec.RotationOffset.z);
    if (spec.UseActorOrigin)
    {
        const float minY = submesh.Bounds.Center.y - submesh.Bounds.Extents.y;
        const float centerX = submesh.Bounds.Center.x;
        const float centerZ = submesh.Bounds.Center.z;
        object->SetPositionOffset(
            -centerX * spec.FallbackScale.x,
            -minY * spec.FallbackScale.y - spec.OriginToFloor,
            -centerZ * spec.FallbackScale.z);
        object->SetPosition(spec.SpawnPosition.x, spec.SpawnPosition.y, spec.SpawnPosition.z);
    }
    else
    {
        object->SetPositionOffset(0.0f, 0.0f, 0.0f);
        object->SetPosition(spec.SpawnPosition.x, spec.SpawnPosition.y, spec.SpawnPosition.z);
    }
    object->Update();
    return true;
}
