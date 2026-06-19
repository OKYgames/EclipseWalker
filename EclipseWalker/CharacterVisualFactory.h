#pragma once

#include "GameObject.h"
#include "RenderItem.h"
#include "ResourceManager.h"
#include <string>
#include <vector>

struct CharacterAnimationClipSpec
{
    std::string FilePath;
    std::string ClipName;
};

struct CharacterVisualSpec
{
    bool UseSkinned = false;

    std::string ModelPath;
    std::string DefaultClipName;
    bool LoadModelAnimations = true;
    std::vector<CharacterAnimationClipSpec> AdditionalAnimationClips;
    std::string GeometryName;

    std::string MaterialName;
    std::string DiffuseTextureName;
    std::wstring DiffuseTexturePath;
    std::string EmissiveTextureName;
    std::wstring EmissiveTexturePath;
    DirectX::XMFLOAT4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
    DirectX::XMFLOAT3 FresnelR0 = { 0.06f, 0.06f, 0.06f };
    float Roughness = 0.65f;
    bool IsToon = false;
    float OutlineThickness = 0.05f;
    DirectX::XMFLOAT4 OutlineColor = { 0.0f, 0.0f, 0.0f, 1.0f };

    std::string FallbackMaterialName;
    std::string FallbackGeometryName = "boxGeo";
    std::string FallbackSubmeshName = "box";
    DirectX::XMFLOAT3 FallbackScale = { 0.3f, 0.5f, 0.3f };

    DirectX::XMFLOAT3 SpawnPosition = { 0.0f, 0.0f, 0.0f };
    bool UseActorOrigin = false;
    bool CenterBoundsXZ = true;
    float OriginToFloor = 0.0f;
    DirectX::XMFLOAT3 RotationOffset = { 0.0f, 0.0f, 0.0f };
    float TargetHeight = 1.8f;
};

class CharacterVisualFactory
{
public:
    static bool ApplyVisual(
        GameObject* object,
        RenderItem* renderItem,
        ID3D12Device* device,
        ID3D12GraphicsCommandList* cmdList,
        ResourceManager* resources,
        const CharacterVisualSpec& spec);

private:
    static bool ApplySkinnedVisual(
        GameObject* object,
        RenderItem* renderItem,
        ID3D12Device* device,
        ID3D12GraphicsCommandList* cmdList,
        ResourceManager* resources,
        const CharacterVisualSpec& spec);

    static bool ApplyFallbackVisual(
        GameObject* object,
        RenderItem* renderItem,
        ResourceManager* resources,
        const CharacterVisualSpec& spec);
};
