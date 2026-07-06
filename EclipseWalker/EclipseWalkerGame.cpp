#include "EclipseWalkerGame.h"
#include "AudioManager.h"
#include "CharSelectScene.h"
#include "LoadingScene.h"
#include "LoginScene.h"        
#include "MainMenuScene.h"
#include "RoomSelectScene.h"
#include "Stage1Scene.h"
#include "Stage2Scene.h"
#include "VillageScene.h"
#include "CharacterVisualFactory.h"
#include "DebugConfig.h"
#include "DDSTextureLoader.h"
#include "SkeletalAnimationComponent.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <vector>
#include <imm.h>
#include <windowsx.h>

#pragma comment(lib, "imm32.lib")

namespace
{
    constexpr bool kEnableWeaponSocketDebugInput = false;
    constexpr wchar_t kTitleBgmPath[] = L"Sounds\\bgm\\BGM_Title.mp3";
    constexpr wchar_t kCharacterSelectBgmPath[] = L"Sounds\\bgm\\BGM_CharacterSelect.mp3";
    constexpr wchar_t kFireAmbientPath[] = L"Sounds\\fire_cracking.mp3";
    constexpr wchar_t kLavaAmbientPrimaryPath[] = L"Sounds\\Lava1.mp3";
    constexpr wchar_t kLavaAmbientSecondaryPath[] = L"Sounds\\Lava2.mp3";
    constexpr float kTitleBgmVolume = 0.18f;
    constexpr float kCharacterSelectBgmVolume = 0.16f;
    constexpr float kLavaAmbientVolumeGain = 0.95f;
    constexpr float kLavaAmbientSecondaryMix = 0.60f;
    constexpr float kStage2SkyEclipseDurationSeconds = Stage2Scene::SkyEclipseDurationSeconds;
    const DirectX::XMFLOAT3 kStage2SkyEclipseDirection = { 0.02f, 0.16f, 0.60f };
    const DirectX::XMFLOAT3 kStage2SunLightDirection = { -0.18f, 0.82f, 0.42f };

    float Saturate01(float value)
    {
        return (std::max)(0.0f, (std::min)(value, 1.0f));
    }

    float SmoothStep01(float value)
    {
        const float t = Saturate01(value);
        return t * t * (3.0f - 2.0f * t);
    }

    float LerpFloat(float a, float b, float t)
    {
        return a + (b - a) * t;
    }

    DirectX::XMFLOAT3 LerpFloat3(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b, float t)
    {
        return
        {
            LerpFloat(a.x, b.x, t),
            LerpFloat(a.y, b.y, t),
            LerpFloat(a.z, b.z, t)
        };
    }

    DirectX::XMFLOAT3 NormalizeFloat3(const DirectX::XMFLOAT3& value)
    {
        const float lenSq = value.x * value.x + value.y * value.y + value.z * value.z;
        if (lenSq <= 0.000001f)
        {
            return { 0.0f, 1.0f, 0.0f };
        }

        const float invLen = 1.0f / std::sqrt(lenSq);
        return { value.x * invLen, value.y * invLen, value.z * invLen };
    }

    float GetStage2SkyEclipseProgress(const Stage2Scene* stage2)
    {
        if (stage2 == nullptr)
        {
            return 0.0f;
        }

        return Saturate01(stage2->GetSkyEclipseElapsedSeconds() / kStage2SkyEclipseDurationSeconds);
    }

    Light BuildStage2SunLight(float eclipseProgress)
    {
        const float darkness = SmoothStep01(eclipseProgress);
        const DirectX::XMFLOAT3 sunDir = NormalizeFloat3(kStage2SunLightDirection);
        const DirectX::XMFLOAT3 clearSun = { 1.18f, 0.82f, 0.44f };
        const DirectX::XMFLOAT3 eclipsedSun = { 0.035f, 0.045f, 0.075f };

        Light light = {};
        light.Direction = { -sunDir.x, -sunDir.y, -sunDir.z };
        light.Strength = LerpFloat3(clearSun, eclipsedSun, darkness);
        light.FalloffStart = 1.0f;
        light.FalloffEnd = 10.0f;
        light.SpotPower = 64.0f;
        return light;
    }

    DirectX::XMFLOAT4 BuildStage2AmbientLight(bool isOtherWorld, float eclipseProgress)
    {
        const float darkness = SmoothStep01(eclipseProgress);
        const DirectX::XMFLOAT3 clearAmbient = isOtherWorld
            ? DirectX::XMFLOAT3{ 0.20f, 0.11f, 0.24f }
            : DirectX::XMFLOAT3{ 0.30f, 0.24f, 0.19f };
        const DirectX::XMFLOAT3 eclipsedAmbient = isOtherWorld
            ? DirectX::XMFLOAT3{ 0.045f, 0.050f, 0.095f }
            : DirectX::XMFLOAT3{ 0.055f, 0.060f, 0.090f };
        const DirectX::XMFLOAT3 ambient = LerpFloat3(clearAmbient, eclipsedAmbient, darkness);
        return { ambient.x, ambient.y, ambient.z, 1.0f };
    }

    bool ContainsAsciiInsensitive(const std::string& text, const std::string& needle)
    {
        if (needle.empty())
        {
            return true;
        }

        return std::search(
            text.begin(),
            text.end(),
            needle.begin(),
            needle.end(),
            [](unsigned char lhs, unsigned char rhs)
            {
                return std::tolower(lhs) == std::tolower(rhs);
            }) != text.end();
    }

    bool IsFemalePlayerClass(PlayerClass playerClass)
    {
        return playerClass == PlayerClass::Warrior;
    }

    Material* EnsurePlayerOverlayMaterial(
        ResourceManager* resources,
        const std::string& materialName,
        const std::string& textureName,
        const std::wstring& texturePath,
        const DirectX::XMFLOAT4& color,
        float roughness)
    {
        if (resources == nullptr || materialName.empty())
        {
            return nullptr;
        }

        if (!textureName.empty() &&
            !texturePath.empty() &&
            resources->GetTexture(textureName) == nullptr &&
            std::filesystem::exists(texturePath))
        {
            resources->LoadTexture(textureName, texturePath);
        }

        const std::string diffuseName =
            (!textureName.empty() && resources->GetTexture(textureName) != nullptr) ? textureName : "white";

        if (resources->GetMaterial(materialName) == nullptr)
        {
            resources->CreateMaterial(
                materialName,
                static_cast<int>(resources->mMaterials.size()),
                diffuseName,
                "",
                "",
                "",
                color,
                DirectX::XMFLOAT3(0.06f, 0.06f, 0.06f),
                roughness);
        }

        Material* material = resources->GetMaterial(materialName);
        if (material != nullptr)
        {
            material->DiffuseMapName = diffuseName;
            material->DiffuseAlbedo = color;
            material->FresnelR0 = DirectX::XMFLOAT3(0.06f, 0.06f, 0.06f);
            material->Roughness = roughness;
            material->IsTransparent = 0;
            material->IsToon = 1;
            material->OutlineThickness = 0.008f;
            material->OutlineColor = { 0.04f, 0.04f, 0.05f, 1.0f };
            material->NumFramesDirty = gNumFrameResources;
        }

        return material;
    }

    Material* EnsurePlayerSkinMaterial(ResourceManager* resources, PlayerClass playerClass)
    {
        if (IsFemalePlayerClass(playerClass))
        {
            return EnsurePlayerOverlayMaterial(
                resources,
                "PlayerFemaleSkinMat",
                "PlayerFemaleSkinTex",
                L"Textures/P09_Female_Body_Bright_Diff.dds",
                DirectX::XMFLOAT4(1.0f, 0.94f, 0.88f, 1.0f),
                0.62f);
        }

        return EnsurePlayerOverlayMaterial(
            resources,
            "PlayerMaleSkinMat",
            "PlayerMaleSkinTex",
            L"Textures/P09_Male_Body_Bright_Diff.dds",
            DirectX::XMFLOAT4(1.0f, 0.94f, 0.88f, 1.0f),
            0.62f);
    }

    Material* EnsurePlayerHairMaterial(ResourceManager* resources)
    {
        return EnsurePlayerOverlayMaterial(
            resources,
            "PlayerHairMat",
            "",
            L"",
            DirectX::XMFLOAT4(0.070f, 0.055f, 0.045f, 1.0f),
            0.70f);
    }

    struct EquipmentAttachmentSpec
    {
        bool Enabled = false;
        std::string GeometryName;
        std::string ModelPath;
        std::string MaterialName;
        float TargetMaxDimension = 1.0f;
        DirectX::XMFLOAT3 PivotBias = { 0.0f, 0.0f, 0.0f };
        std::string SocketName;
        DirectX::XMFLOAT3 LocalPosition = { 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT3 LocalRotation = { 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT3 LocalScale = { 1.0f, 1.0f, 1.0f };
    };

    void ConfigureEquipmentSpecs(
        PlayerClass playerClass,
        ClassTier playerTier,
        EquipmentAttachmentSpec& weaponSpec,
        EquipmentAttachmentSpec& shieldSpec)
    {
        weaponSpec = {};
        shieldSpec = {};

        switch (playerClass)
        {
        case PlayerClass::Warrior:
            weaponSpec.Enabled = true;
            if (playerTier == ClassTier::Tier1)
            {
                weaponSpec.GeometryName = "warriorLv1SwordGeo";
                weaponSpec.ModelPath = "Models/Weapons/Warrior_Lv1_Sword.fbx";
                weaponSpec.MaterialName = "PlayerSwordLv1Mat";
            }
            else if (playerTier == ClassTier::Tier2)
            {
                weaponSpec.GeometryName = "warriorLv2SwordGeo";
                weaponSpec.ModelPath = "Models/Weapons/Warrior_Lv2_Sword.fbx";
                weaponSpec.MaterialName = "PlayerSwordLv2Mat";
            }
            else
            {
                weaponSpec.GeometryName = "warriorLv3SwordGeo";
                weaponSpec.ModelPath = "Models/Weapons/Warrior_Lv3_Sword.fbx";
                weaponSpec.MaterialName = "PlayerSwordMat";
            }
            weaponSpec.TargetMaxDimension = 1.0f;
            weaponSpec.SocketName = "mixamorig:RightHand";
            weaponSpec.LocalPosition = { 0.3504f, 0.1006f, 0.0685f };
            weaponSpec.LocalRotation = { 3.0769f, 1.3175f, -1.0446f };

            shieldSpec.Enabled = true;
            if (playerTier == ClassTier::Tier1)
            {
                shieldSpec.GeometryName = "warriorLv1ShieldGeo";
                shieldSpec.ModelPath = "Models/Weapons/Warrior_Lv1_Shield.fbx";
                shieldSpec.MaterialName = "PlayerShieldLv1Mat";
            }
            else if (playerTier == ClassTier::Tier2)
            {
                shieldSpec.GeometryName = "warriorLv2ShieldGeo";
                shieldSpec.ModelPath = "Models/Weapons/Warrior_Lv2_Shield.fbx";
                shieldSpec.MaterialName = "PlayerShieldLv2Mat";
            }
            else
            {
                shieldSpec.GeometryName = "warriorLv3ShieldGeo";
                shieldSpec.ModelPath = "Models/Weapons/Warrior_Lv3_Shield.fbx";
                shieldSpec.MaterialName = "PlayerShieldMat";
            }
            shieldSpec.TargetMaxDimension = 0.55f;
            shieldSpec.SocketName = "mixamorig:LeftHand";
            shieldSpec.LocalPosition = { -0.04f, -0.02f, 0.04f };
            shieldSpec.LocalRotation = { 0.0f, DirectX::XM_PI, -DirectX::XM_PIDIV2 };
            break;

        case PlayerClass::Mage:
            weaponSpec.Enabled = true;
            if (playerTier == ClassTier::Tier1)
            {
                weaponSpec.GeometryName = "wizardLv1StaffGeo";
                weaponSpec.ModelPath = "Models/Weapons/Wizard_Lv1_Staff.fbx";
                weaponSpec.MaterialName = "PlayerStaffLv1Mat";
            }
            else if (playerTier == ClassTier::Tier2)
            {
                weaponSpec.GeometryName = "wizardLv2StaffGeo";
                weaponSpec.ModelPath = "Models/Weapons/Wizard_Lv2_Staff.fbx";
                weaponSpec.MaterialName = "PlayerStaffLv2Mat";
            }
            else
            {
                weaponSpec.GeometryName = "wizardLv3StaffGeo";
                weaponSpec.ModelPath = "Models/Weapons/Wizard_Lv3_Staff.fbx";
                weaponSpec.MaterialName = "PlayerStaffMat";
            }
            weaponSpec.TargetMaxDimension = 1.25f;
            weaponSpec.SocketName = "mixamorig:LeftHand";
            weaponSpec.LocalPosition = { 0.0863f, 0.0370f, -0.0449f };
            weaponSpec.LocalRotation = { 0.3224f, 1.6521f, 0.2869f };
            break;

        case PlayerClass::Archer:
            weaponSpec.Enabled = true;
            if (playerTier == ClassTier::Tier1)
            {
                weaponSpec.GeometryName = "archerLv1BowGeo";
                weaponSpec.ModelPath = "Models/Weapons/Archer_Lv1_Bow.fbx";
                weaponSpec.MaterialName = "PlayerBowLv1Mat";
            }
            else if (playerTier == ClassTier::Tier2)
            {
                weaponSpec.GeometryName = "archerLv2BowGeo";
                weaponSpec.ModelPath = "Models/Weapons/Archer_Lv2_Bow.fbx";
                weaponSpec.MaterialName = "PlayerBowLv2Mat";
            }
            else
            {
                weaponSpec.GeometryName = "archerLv3BowGeo";
                weaponSpec.ModelPath = "Models/Weapons/Archer_Lv3_Bow.fbx";
                weaponSpec.MaterialName = "PlayerBowMat";
            }
            weaponSpec.TargetMaxDimension = 0.95f;
            weaponSpec.SocketName = "mixamorig:LeftHand";
            weaponSpec.LocalPosition = { -0.0054f, 0.0648f, -0.0186f };
            weaponSpec.LocalRotation = { -0.1048f, -1.1832f, -1.4287f };
            break;

        case PlayerClass::None:
        default:
            break;
        }
    }

    std::unique_ptr<MeshGeometry> BuildStaticModelGeometry(
        ID3D12Device* device,
        ID3D12GraphicsCommandList* cmdList,
        const std::string& geometryName,
        const std::string& modelPath,
        float targetMaxDimension,
        const DirectX::XMFLOAT3& pivotBias)
    {
        if (device == nullptr || cmdList == nullptr)
        {
            return nullptr;
        }

        MapMeshData modelData;
        if (!ModelLoader::Load(modelPath, modelData) || modelData.Vertices.empty() || modelData.Indices.empty())
        {
            std::string log = "[Weapon] Failed to load model: " + modelPath + "\n";
            OutputDebugStringA(log.c_str());
            return nullptr;
        }

        DirectX::BoundingBox rawBounds;
        DirectX::BoundingBox::CreateFromPoints(
            rawBounds,
            modelData.Vertices.size(),
            &modelData.Vertices[0].Pos,
            sizeof(Vertex));

        const float rawMaxDimension = (std::max)(
            rawBounds.Extents.x * 2.0f,
            (std::max)(rawBounds.Extents.y * 2.0f, rawBounds.Extents.z * 2.0f));
        const float normalizeScale =
            (targetMaxDimension > 0.0f && rawMaxDimension > 0.0001f) ? (targetMaxDimension / rawMaxDimension) : 1.0f;

        const DirectX::XMFLOAT3 pivot = {
            rawBounds.Center.x + rawBounds.Extents.x * pivotBias.x,
            rawBounds.Center.y + rawBounds.Extents.y * pivotBias.y,
            rawBounds.Center.z + rawBounds.Extents.z * pivotBias.z
        };

        for (auto& vertex : modelData.Vertices)
        {
            vertex.Pos.x = (vertex.Pos.x - pivot.x) * normalizeScale;
            vertex.Pos.y = (vertex.Pos.y - pivot.y) * normalizeScale;
            vertex.Pos.z = (vertex.Pos.z - pivot.z) * normalizeScale;
        }

        auto geometry = std::make_unique<MeshGeometry>();
        geometry->Name = geometryName;

        const UINT vbByteSize = static_cast<UINT>(modelData.Vertices.size() * sizeof(Vertex));
        const UINT ibByteSize = static_cast<UINT>(modelData.Indices.size() * sizeof(std::uint32_t));

        ThrowIfFailed(D3DCreateBlob(vbByteSize, &geometry->VertexBufferCPU));
        CopyMemory(geometry->VertexBufferCPU->GetBufferPointer(), modelData.Vertices.data(), vbByteSize);
        ThrowIfFailed(D3DCreateBlob(ibByteSize, &geometry->IndexBufferCPU));
        CopyMemory(geometry->IndexBufferCPU->GetBufferPointer(), modelData.Indices.data(), ibByteSize);

        geometry->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(
            device,
            cmdList,
            modelData.Vertices.data(),
            vbByteSize,
            geometry->VertexBufferUploader);
        geometry->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(
            device,
            cmdList,
            modelData.Indices.data(),
            ibByteSize,
            geometry->IndexBufferUploader);

        geometry->VertexByteStride = sizeof(Vertex);
        geometry->VertexBufferByteSize = vbByteSize;
        geometry->IndexFormat = DXGI_FORMAT_R32_UINT;
        geometry->IndexBufferByteSize = ibByteSize;

        SubmeshGeometry submesh;
        submesh.IndexCount = static_cast<UINT>(modelData.Indices.size());
        submesh.StartIndexLocation = 0;
        submesh.BaseVertexLocation = 0;
        DirectX::BoundingBox::CreateFromPoints(
            submesh.Bounds,
            modelData.Vertices.size(),
            &modelData.Vertices[0].Pos,
            sizeof(Vertex));
        geometry->DrawArgs["mesh"] = submesh;

        return geometry;
    }

    CharacterVisualSpec BuildPlayerVisualSpec(PlayerClass playerClass, ClassTier playerTier, const DirectX::XMFLOAT3& spawnPosition)
    {
        CharacterVisualSpec spec;
        spec.UseSkinned = true;
        spec.ModelPath = "Models/Player/Warrior_Lv3.fbx";
        spec.DefaultClipName = "";
        spec.LoadModelAnimations = false;
        spec.AdditionalAnimationClips.push_back({ "Models/Animated/Female_Warrior/Female_Warrior_Idle.fbx", "FemaleIdle" });
        spec.AdditionalAnimationClips.push_back({ "Models/Animated/Female_Warrior/Female_Warrior_Walk.fbx", "FemaleWalk" });
        spec.AdditionalAnimationClips.push_back({ "Models/Animated/Female_Warrior/Female_Warrior_Attack1.fbx", "FemaleAttack1" });
        spec.AdditionalAnimationClips.push_back({ "Models/Animated/Female_Warrior/Female_Warrior_Attack2.fbx", "FemaleAttack2" });
        spec.AdditionalAnimationClips.push_back({ "Models/Animated/Female_Warrior/Female_Warrior_Attack_Q.fbx", "FemaleAttackQ" });
        spec.AdditionalAnimationClips.push_back({ "Models/Animated/Female_Warrior/Female_Warrior_Attack_E.fbx", "FemaleAttackE" });
        spec.AdditionalAnimationClips.push_back({ "Models/Animated/Female_Warrior/Sword And Shield Death.fbx", "FemaleDeath" });
        spec.AdditionalAnimationClips.push_back({ "Models/Animated/Female_Warrior/Stand Up (2).fbx", "FemaleRespawn" });
        spec.AdditionalAnimationClips.push_back({ "Models/Animated/Dash.fbx", "FemaleDash" });
        spec.GeometryName = "warriorLv3Geo";
        spec.MaterialName = "PlayerWarriorLv3Mat";
        spec.DiffuseTextureName = "WarriorLv3Armor";
        spec.DiffuseTexturePath = L"Textures/P09_Female_Armor_006_Diff.dds";
        spec.DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
        spec.FresnelR0 = { 0.06f, 0.06f, 0.06f };
        spec.Roughness = 0.65f;
        spec.IsToon = true;
        spec.OutlineThickness = 0.012f;
        spec.OutlineColor = { 0.07f, 0.07f, 0.10f, 1.0f };
        spec.TargetHeight = Player::DefaultVisualTargetHeight;
        spec.SpawnPosition = spawnPosition;
        spec.UseActorOrigin = true;
        spec.OriginToFloor = Player::DefaultColliderHalfHeight + Player::DefaultVisualFloorBias;
        spec.RotationOffset = { 0.0f, DirectX::XM_PI, 0.0f };
        spec.FallbackMaterialName = "PlayerBlue";
        spec.FallbackScale = { 0.3f, 0.5f, 0.3f };

        switch (playerClass)
        {
        case PlayerClass::Mage:
            spec.ModelPath = "Models/Player/Wizard_Lv3.fbx";
            spec.AdditionalAnimationClips.clear();
            spec.AdditionalAnimationClips.push_back({ "Models/Animated/Male_Wizard/Standing Torch Idle 01.fbx", "FemaleIdle" });
            spec.AdditionalAnimationClips.push_back({ "Models/Animated/Male_Wizard/Standing Torch Walk Forward.fbx", "FemaleWalk" });
            spec.AdditionalAnimationClips.push_back({ "Models/Animated/Male_Wizard/Standing Torch Melee Attack Stab.fbx", "FemaleAttack1" });
            spec.AdditionalAnimationClips.push_back({ "Models/Animated/Male_Wizard/Standing Torch Melee Attack Stab.fbx", "FemaleAttack2" });
            spec.AdditionalAnimationClips.push_back({ "Models/Animated/Male_Wizard/Standing Torch Light Torch Q.fbx", "FemaleAttackQ" });
            spec.AdditionalAnimationClips.push_back({ "Models/Animated/Male_Wizard/Standing Torch Melee Attack E.fbx", "FemaleAttackE" });
            spec.AdditionalAnimationClips.push_back({ "Models/Animated/Male_Wizard/Standing React Death Backward.fbx", "FemaleDeath" });
            spec.AdditionalAnimationClips.push_back({ "Models/Animated/Male_Wizard/Stand Up.fbx", "FemaleRespawn" });
            spec.AdditionalAnimationClips.push_back({ "Models/Animated/Dash.fbx", "FemaleDash" });
            spec.GeometryName = "wizardLv3Geo";
            spec.MaterialName = "PlayerWizardLv3Mat";
            spec.DiffuseTextureName = "WizardLv3Armor";
            spec.DiffuseTexturePath = L"Textures/P09_Male_Armor_009_BaseMap.dds";
            spec.OutlineColor = { 0.05f, 0.06f, 0.09f, 1.0f };
            if (playerTier == ClassTier::Tier1)
            {
                spec.ModelPath = "Models/Player/Wizard_Lv1.fbx";
                spec.GeometryName = "wizardLv1Geo";
                spec.MaterialName = "PlayerWizardLv1Mat";
                spec.DiffuseTextureName = "WizardLv1Armor";
                spec.DiffuseTexturePath = L"Textures/P09_Male_Armor_007_BaseMap.dds";
            }
            else if (playerTier == ClassTier::Tier2)
            {
                spec.ModelPath = "Models/Player/Wizard_Lv2.fbx";
                spec.GeometryName = "wizardLv2Geo";
                spec.MaterialName = "PlayerWizardLv2Mat";
                spec.DiffuseTextureName = "WizardLv2Armor";
                spec.DiffuseTexturePath = L"Textures/P09_Male_Armor_008_BaseMap.dds";
            }
            break;
        case PlayerClass::Archer:
            spec.ModelPath = "Models/Player/Archer_Lv3.fbx";
            spec.AdditionalAnimationClips.clear();
            spec.AdditionalAnimationClips.push_back({ "Models/Animated/male_archer/Standing Idle.fbx", "FemaleIdle" });
            spec.AdditionalAnimationClips.push_back({ "Models/Animated/male_archer/Standing Walk Forward.fbx", "FemaleWalk" });
            spec.AdditionalAnimationClips.push_back({ "Models/Animated/male_archer/Shooting Arrow.fbx", "FemaleAttack1" });
            spec.AdditionalAnimationClips.push_back({ "Models/Animated/male_archer/Shooting Arrow.fbx", "FemaleAttack2" });
            spec.AdditionalAnimationClips.push_back({ "Models/Animated/male_archer/Shooting Arrow E.fbx", "FemaleAttackE" });
            spec.AdditionalAnimationClips.push_back({ "Models/Animated/male_archer/Standing Death Backward 01.fbx", "FemaleDeath" });
            spec.AdditionalAnimationClips.push_back({ "Models/Animated/male_archer/Stand Up.fbx", "FemaleRespawn" });
            spec.AdditionalAnimationClips.push_back({ "Models/Animated/Dash.fbx", "FemaleDash" });
            spec.GeometryName = "archerLv3Geo";
            spec.MaterialName = "PlayerArcherLv3Mat";
            spec.DiffuseTextureName = "ArcherLv3Armor";
            spec.DiffuseTexturePath = L"Textures/P09_Male_Armor_012_BaseMap.dds";
            spec.OutlineColor = { 0.06f, 0.07f, 0.05f, 1.0f };
            if (playerTier == ClassTier::Tier1)
            {
                spec.ModelPath = "Models/Player/Archer_Lv1.fbx";
                spec.GeometryName = "archerLv1Geo";
                spec.MaterialName = "PlayerArcherLv1Mat";
                spec.DiffuseTextureName = "ArcherLv1Armor";
                spec.DiffuseTexturePath = L"Textures/P09_Male_Armor_010_BaseMap.dds";
            }
            else if (playerTier == ClassTier::Tier2)
            {
                spec.ModelPath = "Models/Player/Archer_Lv2.fbx";
                spec.GeometryName = "archerLv2Geo";
                spec.MaterialName = "PlayerArcherLv2Mat";
                spec.DiffuseTextureName = "ArcherLv2Armor";
                spec.DiffuseTexturePath = L"Textures/P09_Male_Armor_011_BaseMap.dds";
            }
            break;
        case PlayerClass::Warrior:
            if (playerTier == ClassTier::Tier1)
            {
                spec.ModelPath = "Models/Player/Warrior_Lv1.fbx";
                spec.GeometryName = "warriorLv1Geo";
                spec.MaterialName = "PlayerWarriorLv1Mat";
                spec.DiffuseTextureName = "WarriorLv1Armor";
                spec.DiffuseTexturePath = L"Textures/P09_Female_Armor_004_Diff.dds";
            }
            else if (playerTier == ClassTier::Tier2)
            {
                spec.ModelPath = "Models/Player/Warrior_Lv2.fbx";
                spec.GeometryName = "warriorLv2Geo";
                spec.MaterialName = "PlayerWarriorLv2Mat";
                spec.DiffuseTextureName = "WarriorLv2Armor";
                spec.DiffuseTexturePath =
                    std::filesystem::exists("Textures/P09_Female_Armor_005_Diff.dds")
                    ? L"Textures/P09_Female_Armor_005_Diff.dds"
                    : L"Textures/P09_Female_Armor_006_Diff.dds";
            }
            break;
        case PlayerClass::None:
        default:
            break;
        }

        return spec;
    }

    void WarmGameplayCharacterVisual(
        ResourceManager* resources,
        ID3D12Device* device,
        ID3D12GraphicsCommandList* cmdList,
        PlayerClass playerClass,
        ClassTier playerTier)
    {
        if (resources == nullptr || device == nullptr || cmdList == nullptr)
        {
            return;
        }

        RenderItem scratchRenderItem;
        scratchRenderItem.ObjCBIndex = 0;

        GameObject scratchObject;
        const CharacterVisualSpec visualSpec =
            BuildPlayerVisualSpec(playerClass, playerTier, DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f));

        if (!CharacterVisualFactory::ApplyVisual(
            &scratchObject,
            &scratchRenderItem,
            device,
            cmdList,
            resources,
            visualSpec))
        {
            std::ostringstream warmLog;
            warmLog << "[Shared] Failed to warm gameplay character visual for class "
                << static_cast<int>(playerClass) << "\n";
            OutputDebugStringA(warmLog.str().c_str());
        }
    }

    void WarmAllGameplayCharacterVisuals(
        ResourceManager* resources,
        ID3D12Device* device,
        ID3D12GraphicsCommandList* cmdList)
    {
        WarmGameplayCharacterVisual(resources, device, cmdList, PlayerClass::Warrior, ClassTier::Tier1);
        WarmGameplayCharacterVisual(resources, device, cmdList, PlayerClass::Warrior, ClassTier::Tier2);
        WarmGameplayCharacterVisual(resources, device, cmdList, PlayerClass::Warrior, ClassTier::Tier3);
        WarmGameplayCharacterVisual(resources, device, cmdList, PlayerClass::Mage, ClassTier::Tier1);
        WarmGameplayCharacterVisual(resources, device, cmdList, PlayerClass::Mage, ClassTier::Tier2);
        WarmGameplayCharacterVisual(resources, device, cmdList, PlayerClass::Mage, ClassTier::Tier3);
        WarmGameplayCharacterVisual(resources, device, cmdList, PlayerClass::Archer, ClassTier::Tier1);
        WarmGameplayCharacterVisual(resources, device, cmdList, PlayerClass::Archer, ClassTier::Tier2);
        WarmGameplayCharacterVisual(resources, device, cmdList, PlayerClass::Archer, ClassTier::Tier3);
    }

    void WarmGameplayEquipmentGeometry(
        ResourceManager* resources,
        ID3D12Device* device,
        ID3D12GraphicsCommandList* cmdList,
        const EquipmentAttachmentSpec& attachmentSpec)
    {
        if (resources == nullptr || device == nullptr || cmdList == nullptr ||
            !attachmentSpec.Enabled || attachmentSpec.GeometryName.empty())
        {
            return;
        }

        if (resources->mGeometries.find(attachmentSpec.GeometryName) != resources->mGeometries.end())
        {
            return;
        }

        if (!std::filesystem::exists(attachmentSpec.ModelPath))
        {
            std::string log = "[Weapon] Missing warm model: " + attachmentSpec.ModelPath + "\n";
            OutputDebugStringA(log.c_str());
            return;
        }

        auto geometry = BuildStaticModelGeometry(
            device,
            cmdList,
            attachmentSpec.GeometryName,
            attachmentSpec.ModelPath,
            attachmentSpec.TargetMaxDimension,
            attachmentSpec.PivotBias);
        if (geometry != nullptr)
        {
            resources->mGeometries[geometry->Name] = std::move(geometry);
        }
    }

    void WarmAllGameplayEquipmentGeometries(
        ResourceManager* resources,
        ID3D12Device* device,
        ID3D12GraphicsCommandList* cmdList)
    {
        constexpr PlayerClass kClasses[3] = {
            PlayerClass::Warrior,
            PlayerClass::Mage,
            PlayerClass::Archer
        };
        constexpr ClassTier kTiers[3] = {
            ClassTier::Tier1,
            ClassTier::Tier2,
            ClassTier::Tier3
        };

        for (PlayerClass playerClass : kClasses)
        {
            for (ClassTier playerTier : kTiers)
            {
                EquipmentAttachmentSpec weaponSpec;
                EquipmentAttachmentSpec shieldSpec;
                ConfigureEquipmentSpecs(playerClass, playerTier, weaponSpec, shieldSpec);

                WarmGameplayEquipmentGeometry(resources, device, cmdList, weaponSpec);
                WarmGameplayEquipmentGeometry(resources, device, cmdList, shieldSpec);
            }
        }
    }

    const char* GetPlayerAnimationClipName(int animationState)
    {
        if (animationState == static_cast<int>(PlayerAnimationState::Dash))
        {
            return "FemaleDash";
        }
        return animationState == static_cast<int>(PlayerAnimationState::Idle)
            ? "FemaleIdle"
            : "FemaleWalk";
    }

    const char* GetPlayerAttackClipName(int skillType, PlayerClass playerClass)
    {
        if (playerClass == PlayerClass::Warrior && skillType == 1)
        {
            return "FemaleAttackQ";
        }
        if (playerClass == PlayerClass::Warrior && skillType == 2)
        {
            return "FemaleAttackE";
        }
        if (playerClass == PlayerClass::Mage && skillType == 1)
        {
            return "FemaleAttackQ";
        }
        if ((playerClass == PlayerClass::Mage || playerClass == PlayerClass::Archer) && skillType == 2)
        {
            return "FemaleAttackE";
        }
        if (playerClass == PlayerClass::Archer && skillType == 1)
        {
            return nullptr;
        }
        return (skillType == 0 || skillType == 2) ? "FemaleAttack2" : "FemaleAttack1";
    }

    PlayerClass DecodeNetworkPlayerClass(int classType)
    {
        switch (static_cast<PlayerClass>(classType))
        {
        case PlayerClass::Warrior:
            return PlayerClass::Warrior;
        case PlayerClass::Mage:
            return PlayerClass::Mage;
        case PlayerClass::Archer:
            return PlayerClass::Archer;
        case PlayerClass::None:
        default:
            return PlayerClass::Mage;
        }
    }

    bool IsValidNetworkPlayerClass(int classType)
    {
        return classType == static_cast<int>(PlayerClass::Warrior) ||
            classType == static_cast<int>(PlayerClass::Mage) ||
            classType == static_cast<int>(PlayerClass::Archer);
    }

    ClassTier DecodeNetworkPlayerTier(int playerLevel)
    {
        switch (playerLevel)
        {
        case static_cast<int>(ClassTier::Tier1):
            return ClassTier::Tier1;
        case static_cast<int>(ClassTier::Tier2):
            return ClassTier::Tier2;
        case static_cast<int>(ClassTier::Tier3):
            return ClassTier::Tier3;
        default:
            return ClassTier::Tier1;
        }
    }

    bool IsValidNetworkPlayerLevel(int playerLevel)
    {
        return playerLevel >= static_cast<int>(ClassTier::Tier1) &&
            playerLevel <= static_cast<int>(ClassTier::Tier3);
    }
}

EclipseWalkerGame::EclipseWalkerGame(HINSTANCE hInstance) : GameFramework(hInstance) {}
EclipseWalkerGame::~EclipseWalkerGame() {}

bool EclipseWalkerGame::Initialize()
{
    srand((unsigned int)time(NULL));
    m4xMsaaState = true;

    if (!GameFramework::Initialize()) return false;

    ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

    // DSV 힙 생성
    {
        D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
        dsvHeapDesc.NumDescriptors = 2;
        dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        dsvHeapDesc.NodeMask = 0;
        ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&mDsvHeap)));
        CD3DX12_CPU_DESCRIPTOR_HANDLE mainDsvHandle(mDsvHeap->GetCPUDescriptorHandleForHeapStart());
        D3D12_DEPTH_STENCIL_VIEW_DESC mainDsvDesc = {};
        mainDsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        mainDsvDesc.ViewDimension = m4xMsaaState
            ? D3D12_DSV_DIMENSION_TEXTURE2DMS
            : D3D12_DSV_DIMENSION_TEXTURE2D;
        mainDsvDesc.Flags = D3D12_DSV_FLAG_NONE;
        md3dDevice->CreateDepthStencilView(mDepthStencilBuffer.Get(), &mainDsvDesc, mainDsvHandle);
    }

    // 공용 시스템 초기화
    mResources = std::make_unique<ResourceManager>(md3dDevice.Get(), mCommandList.Get());
    mRenderer = std::make_unique<Renderer>(md3dDevice.Get());

    CD3DX12_CPU_DESCRIPTOR_HANDLE shadowHandle(mDsvHeap->GetCPUDescriptorHandleForHeapStart());
    UINT dsvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    shadowHandle.Offset(1, dsvDescriptorSize);
    mRenderer->Initialize(shadowHandle);

    // --- 占쌘억옙 占쏙옙占쏙옙 ---
    InitLights();
    BuildFrameResources();
    LoadCoreResources();  
    BuildMirrorBreakResources();

    ThrowIfFailed(mCommandList->Close());
    ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    // GPU 占쏙옙占쏙옙화
    mCurrentFence++;
    mCommandQueue->Signal(mFence.Get(), mCurrentFence);
    if (mFence->GetCompletedValue() < mCurrentFence)
    {
        HANDLE eventHandle = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        mFence->SetEventOnCompletion(mCurrentFence, eventHandle);
        WaitForSingleObject(eventHandle, INFINITE);
        CloseHandle(eventHandle);
    }

    if (DebugConfig::kEnableBackendConnection)
    {
        NetworkManager::Get()->ConnectAsync(DebugConfig::kServerIp, DebugConfig::kServerPort);
    }
    else
    {
        OutputDebugStringA("[Debug] Backend connection disabled.\n");
    }

    if (DebugConfig::kEnableDbLogin)
    {
        ChangeScene(std::make_unique<LoginScene>(this));
    }
    else
    {
        OutputDebugStringA("[Debug] DB login disabled. Starting at main menu.\n");
        ChangeScene(std::make_unique<MainMenuScene>(this));
    }
    BuildDescriptorHeaps();

    mCamera.SetLens(0.25f * 3.14f, AspectRatio(), 1.0f, 10000.0f);

    return true;
}

std::unique_ptr<Player> EclipseWalkerGame::CreatePlayerForSelectedClass() const
{
    switch (mSelectedPlayerClass)
    {
    case PlayerClass::Warrior:
        return std::make_unique<Warrior>();
    case PlayerClass::Archer:
        return std::make_unique<Archer>();
    case PlayerClass::Mage:
    case PlayerClass::None:
    default:
        return std::make_unique<Mage>();
    }
}

void EclipseWalkerGame::SetSelectedPlayerClass(PlayerClass playerClass)
{
    if (mSelectedPlayerClass == playerClass)
    {
        return;
    }

    mSelectedPlayerClass = playerClass;
    RefreshPlayerForSelectedClass();
}

void EclipseWalkerGame::SetSelectedPlayerTier(ClassTier playerTier)
{
    if (mSelectedPlayerTier == playerTier &&
        (mPlayer == nullptr || mPlayer->GetCurrentTier() == playerTier))
    {
        return;
    }

    ApplySelectedPlayerTierVisual(playerTier);
}

void EclipseWalkerGame::ApplySelectedPlayerTierVisual(ClassTier playerTier)
{
    ApplySelectedPlayerVisual(playerTier, false);
}

void EclipseWalkerGame::PrepareSelectedPlayerForNewRun()
{
    mSelectedPlayerTier = ClassTier::Tier1;
    if (mPlayer != nullptr)
    {
        mPlayer->ResetProgression();
    }

    ApplySelectedPlayerVisual(ClassTier::Tier1, true);
}

void EclipseWalkerGame::RefreshPlayerForSelectedClass()
{
    const ClassTier resolvedTier = mPlayer ? mPlayer->GetCurrentTier() : mSelectedPlayerTier;
    ApplySelectedPlayerVisual(resolvedTier, true);
}

void EclipseWalkerGame::ApplySelectedPlayerVisual(ClassTier playerTier, bool recreatePlayerInstance)
{
    mSelectedPlayerTier = playerTier;

    if (mPlayerObject == nullptr)
    {
        return;
    }

    auto previousPosition = mPlayer ? mPlayer->GetPosition() : mPlayerObject->GetPosition();

    HideOverlayRenderItems(mPlayerSkinOverlayRitems);
    ClearLocalPlayerEquipment();

    const CharacterVisualSpec visualSpec = BuildPlayerVisualSpec(mSelectedPlayerClass, playerTier, previousPosition);
    if (!CharacterVisualFactory::ApplyVisual(
        mPlayerObject,
        mPlayerObject->Ritem,
        md3dDevice.Get(),
        mCommandList.Get(),
        mResources.get(),
        visualSpec))
    {
        OutputDebugStringA("[Player] Failed to refresh player visual.\n");
    }

    BuildPlayerSkinOverlays(
        mSelectedPlayerClass,
        mPlayerObject,
        mPlayerObject ? mPlayerObject->Ritem : nullptr,
        mPlayerSkinOverlayRitems);
    BuildPlayerWeapon();

    if (recreatePlayerInstance || mPlayer == nullptr)
    {
        mPlayer = CreatePlayerForSelectedClass();
        mPlayer->Initialize(mPlayerObject, &mCamera);
    }

    if (mPlayer != nullptr)
    {
        mPlayer->SetCurrentTier(playerTier);
        mPlayer->SetPosition(previousPosition.x, previousPosition.y, previousPosition.z);
        mPlayer->ForceSendNetworkState();
    }
}

void EclipseWalkerGame::SetMirrorBreakEffect(float progress)
{
    mMirrorBreakEffectActive = true;
    mMirrorBreakEffectProgress = (std::clamp)(progress, 0.0f, 1.0f);
}

void EclipseWalkerGame::ClearMirrorBreakEffect()
{
    mMirrorBreakEffectActive = false;
    mMirrorBreakEffectProgress = 0.0f;
}

void EclipseWalkerGame::ChangeScene(std::unique_ptr<Scene> newScene)
{
    ClearMirrorBreakEffect();

    mCurrentFence++;
    mCommandQueue->Signal(mFence.Get(), mCurrentFence);
    if (mFence->GetCompletedValue() < mCurrentFence)
    {
        HANDLE eventHandle = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        mFence->SetEventOnCompletion(mCurrentFence, eventHandle);
        WaitForSingleObject(eventHandle, INFINITE);
        CloseHandle(eventHandle);
    }

    // 1. 占쏙옙占쏙옙 占쏙옙 占쏙옙占쏙옙
    if (mCurrentScene) mCurrentScene->Exit();
    ClearFireAudioEmitters();
    ClearLavaAudioEmitters();

    ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

    // 3. 占쏙옙 占쏙옙 占쏙옙占쏙옙
    mCurrentScene = std::move(newScene);
    mCurrentScene->Enter();
    UpdateSceneAudio();

    // 4. 占싸듸옙 占쏙옙占?占쏙옙 占쏙옙占쏙옙占쏙옙占쏙옙 占쏙옙占시쇽옙 占쌥곤옙 GPU占쏙옙占쏙옙 占쏙옙占쏙옙
    ThrowIfFailed(mCommandList->Close());
    ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    mCurrentFence++;
    mCommandQueue->Signal(mFence.Get(), mCurrentFence);
    if (mFence->GetCompletedValue() < mCurrentFence)
    {
        HANDLE eventHandle = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        mFence->SetEventOnCompletion(mCurrentFence, eventHandle);
        WaitForSingleObject(eventHandle, INFINITE);
        CloseHandle(eventHandle);
    }
}

void EclipseWalkerGame::RequestSceneChange(std::unique_ptr<Scene> newScene, const std::wstring& loadingLabel)
{
    if (!newScene)
    {
        return;
    }

    if (!mCurrentScene)
    {
        ChangeScene(std::move(newScene));
        return;
    }

    mPendingScene = std::move(newScene);
    mPendingSceneLoadingLabel = loadingLabel.empty() ? L"LOADING..." : loadingLabel;

    if (dynamic_cast<LoadingScene*>(mCurrentScene.get()) == nullptr)
    {
        ChangeScene(std::make_unique<LoadingScene>(this));
    }
}

void EclipseWalkerGame::FinalizePendingSceneChange()
{
    if (!mPendingScene)
    {
        return;
    }

    std::unique_ptr<Scene> nextScene = std::move(mPendingScene);
    mPendingSceneLoadingLabel = L"LOADING...";
    ChangeScene(std::move(nextScene));
}

void EclipseWalkerGame::LoadCoreResources()
{
    auto resolveTexturePath = [](const std::wstring& relativePath) -> std::wstring
    {
        namespace fs = std::filesystem;

        std::vector<fs::path> candidates;
        candidates.emplace_back(relativePath);

        wchar_t modulePath[MAX_PATH] = {};
        if (GetModuleFileNameW(nullptr, modulePath, MAX_PATH) > 0)
        {
            const fs::path exeDir = fs::path(modulePath).parent_path();
            candidates.emplace_back(exeDir / relativePath);
            candidates.emplace_back(exeDir / L".." / L".." / relativePath);
            candidates.emplace_back(exeDir / L".." / L".." / L"EclipseWalker" / relativePath);
        }

        for (const auto& candidate : candidates)
        {
            if (fs::exists(candidate))
            {
                return candidate.wstring();
            }
        }

        return relativePath;
    };

    mResources->LoadTexture("TitleTex", L"Textures/Title.dds");
    mResources->CreateMaterial("TitleMat", static_cast<int>(mResources->mMaterials.size()), "TitleTex", "", "", "",
        XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, 0.0f), 1.0f);
    mResources->GetMaterial("TitleMat")->NumFramesDirty = 3;

    std::wstring mainMenuTexturePath = resolveTexturePath(L"Textures/MainMenu.dds");
    const bool hasMainMenuTexture = std::filesystem::exists(mainMenuTexturePath);
    if (hasMainMenuTexture)
    {
        mResources->LoadTexture("MainMenuTex", mainMenuTexturePath);
    }
    else
    {
        OutputDebugStringA("[MainMenu] Textures/MainMenu.dds not found. Falling back to TitleTex.\n");
    }

    mResources->CreateMaterial(
        "MainMenuMat",
        static_cast<int>(mResources->mMaterials.size()),
        hasMainMenuTexture ? "MainMenuTex" : "TitleTex",
        "",
        "",
        "",
        XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f),
        XMFLOAT3(0.0f, 0.0f, 0.0f),
        1.0f);
    mResources->GetMaterial("MainMenuMat")->NumFramesDirty = 3;

    std::array<Vertex, 4> quadVertices = {
        Vertex({ XMFLOAT3(-1.0f, -1.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT2(0.0f, 1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f) }),
        Vertex({ XMFLOAT3(-1.0f,  1.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT2(0.0f, 0.0f), XMFLOAT3(1.0f, 0.0f, 0.0f) }),
        Vertex({ XMFLOAT3(1.0f,  1.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT2(1.0f, 0.0f), XMFLOAT3(1.0f, 0.0f, 0.0f) }),
        Vertex({ XMFLOAT3(1.0f, -1.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT2(1.0f, 1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f) })
    };
    std::array<std::uint16_t, 6> quadIndices = { 0, 1, 2, 0, 2, 3 };

    const UINT quadVbSize = (UINT)quadVertices.size() * sizeof(Vertex);
    const UINT quadIbSize = (UINT)quadIndices.size() * sizeof(std::uint16_t);
    auto quadGeo = std::make_unique<MeshGeometry>();
    quadGeo->Name = "quadGeo";
    D3DCreateBlob(quadVbSize, &quadGeo->VertexBufferCPU); CopyMemory(quadGeo->VertexBufferCPU->GetBufferPointer(), quadVertices.data(), quadVbSize);
    D3DCreateBlob(quadIbSize, &quadGeo->IndexBufferCPU);  CopyMemory(quadGeo->IndexBufferCPU->GetBufferPointer(), quadIndices.data(), quadIbSize);
    quadGeo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(), mCommandList.Get(), quadVertices.data(), quadVbSize, quadGeo->VertexBufferUploader);
    quadGeo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(), mCommandList.Get(), quadIndices.data(), quadIbSize, quadGeo->IndexBufferUploader);
    quadGeo->VertexByteStride = sizeof(Vertex); quadGeo->VertexBufferByteSize = quadVbSize;
    quadGeo->IndexFormat = DXGI_FORMAT_R16_UINT; quadGeo->IndexBufferByteSize = quadIbSize;
    SubmeshGeometry quadSubmesh; quadSubmesh.IndexCount = (UINT)quadIndices.size(); quadSubmesh.StartIndexLocation = 0; quadSubmesh.BaseVertexLocation = 0;
    quadGeo->DrawArgs["quad"] = quadSubmesh;
    mResources->mGeometries[quadGeo->Name] = std::move(quadGeo);
}

void EclipseWalkerGame::LoadSharedGameResources()
{
    if (mIsSharedResourcesLoaded)
    {
        BuildPlayerWeapon();
        return;
    }
    OutputDebugStringA("\n[Shared] 占싸곤옙占쏙옙 占쏙옙占쏙옙 占쏙옙占쌀쏙옙 占싸듸옙 占쏙옙占쏙옙...\n");

    // 1. 占쏙옙占쏙옙 占쌔쏙옙처 占싸듸옙
    mResources->LoadTexture("Fire_1", L"Models/Stage1Map/Textures/Fire_1.dds");
    mResources->LoadTexture("Blue", L"Textures/Blue.dds");
    mResources->LoadTexture("white", L"Textures/white.dds");

    // Remote players can require either body material before the local player does.
    // Preload both so skin overlays use stable texture descriptors on every client.
    EnsurePlayerSkinMaterial(mResources.get(), PlayerClass::Warrior);
    EnsurePlayerSkinMaterial(mResources.get(), PlayerClass::Mage);
    EnsurePlayerHairMaterial(mResources.get());

    if (std::filesystem::exists(L"Textures/MagicCircle.dds"))
    {
        mResources->LoadTexture("MagicCircle", L"Textures/MagicCircle.dds");
    }
    if (std::filesystem::exists(L"Textures/Effect/circle_03.dds"))
    {
        mResources->LoadTexture("Effect_Circle03", L"Textures/Effect/circle_03.dds");
    }
    if (std::filesystem::exists(L"Textures/Effect/circle_02.dds"))
    {
        mResources->LoadTexture("Effect_Circle02", L"Textures/Effect/circle_02.dds");
    }
    if (std::filesystem::exists(L"Textures/Effect/dirt_01.dds"))
    {
        mResources->LoadTexture("Effect_Dirt01", L"Textures/Effect/dirt_01.dds");
    }
    if (std::filesystem::exists(L"Textures/Effect/scratch_01.dds"))
    {
        mResources->LoadTexture("Effect_Scratch01", L"Textures/Effect/scratch_01.dds");
    }
    if (std::filesystem::exists(L"Textures/Effect/mage_basic_muzzle_01.dds"))
    {
        mResources->LoadTexture("Effect_MageBasic_Muzzle01", L"Textures/Effect/mage_basic_muzzle_01.dds");
    }
    if (std::filesystem::exists(L"Textures/Effect/mage_basic_muzzle_02.dds"))
    {
        mResources->LoadTexture("Effect_MageBasic_Muzzle02", L"Textures/Effect/mage_basic_muzzle_02.dds");
    }
    if (std::filesystem::exists(L"Textures/Effect/mage_basic_muzzle_03.dds"))
    {
        mResources->LoadTexture("Effect_MageBasic_Muzzle03", L"Textures/Effect/mage_basic_muzzle_03.dds");
    }
    if (std::filesystem::exists(L"Textures/Effect/mage_basic_muzzle_04.dds"))
    {
        mResources->LoadTexture("Effect_MageBasic_Muzzle04", L"Textures/Effect/mage_basic_muzzle_04.dds");
    }
    if (std::filesystem::exists(L"Textures/Effect/mage_basic_muzzle_05.dds"))
    {
        mResources->LoadTexture("Effect_MageBasic_Muzzle05", L"Textures/Effect/mage_basic_muzzle_05.dds");
    }
    if (std::filesystem::exists(L"Textures/Effect/mage_heal_flare_01.dds"))
    {
        mResources->LoadTexture("Effect_MageHeal_Sparkle", L"Textures/Effect/mage_heal_flare_01.dds");
    }
    else if (std::filesystem::exists(L"Textures/Effect/mage_heal_star_01.dds"))
    {
        mResources->LoadTexture("Effect_MageHeal_Sparkle", L"Textures/Effect/mage_heal_star_01.dds");
    }
    else if (std::filesystem::exists(L"Textures/Effect/mage_heal_sparkle_star_01.dds"))
    {
        mResources->LoadTexture("Effect_MageHeal_Sparkle", L"Textures/Effect/mage_heal_sparkle_star_01.dds");
    }
    else if (std::filesystem::exists(L"Textures/Effect/mage_heal_sparkle_01.dds"))
    {
        mResources->LoadTexture("Effect_MageHeal_Sparkle", L"Textures/Effect/mage_heal_sparkle_01.dds");
    }
    if (std::filesystem::exists(L"Textures/Effect/mage_heal_smoke_01.dds"))
    {
        mResources->LoadTexture("Effect_MageHeal_Smoke", L"Textures/Effect/mage_heal_smoke_01.dds");
    }
    if (std::filesystem::exists(L"Textures/Effect/mage_heal_point_01.dds"))
    {
        mResources->LoadTexture("Effect_MageHeal_Point", L"Textures/Effect/mage_heal_point_01.dds");
    }
    if (std::filesystem::exists(L"Textures/Effect/mage_meteor_circle_red_512.dds"))
    {
        mResources->LoadTexture("Effect_MageMeteor_Circle", L"Textures/Effect/mage_meteor_circle_red_512.dds");
    }
    if (std::filesystem::exists(L"Textures/Effect/mage_meteor_flame_01.dds"))
    {
        mResources->LoadTexture("Effect_MageMeteor_Flame01", L"Textures/Effect/mage_meteor_flame_01.dds");
    }
    if (std::filesystem::exists(L"Textures/Effect/mage_meteor_flame_02.dds"))
    {
        mResources->LoadTexture("Effect_MageMeteor_Flame02", L"Textures/Effect/mage_meteor_flame_02.dds");
    }
    if (std::filesystem::exists(L"Textures/Effect/mage_meteor_flame_03.dds"))
    {
        mResources->LoadTexture("Effect_MageMeteor_Flame03", L"Textures/Effect/mage_meteor_flame_03.dds");
    }
    if (std::filesystem::exists(L"Textures/Effect/mage_meteor_flame_04.dds"))
    {
        mResources->LoadTexture("Effect_MageMeteor_Flame04", L"Textures/Effect/mage_meteor_flame_04.dds");
    }
    if (std::filesystem::exists(L"Textures/Effect/meteor_fire_smoke_01.dds"))
    {
        mResources->LoadTexture("Effect_MageMeteor_FireSmoke01", L"Textures/Effect/meteor_fire_smoke_01.dds");
    }
    if (std::filesystem::exists(L"Textures/Effect/meteor_fire_smoke_02.dds"))
    {
        mResources->LoadTexture("Effect_MageMeteor_FireSmoke02", L"Textures/Effect/meteor_fire_smoke_02.dds");
    }
    if (std::filesystem::exists(L"Textures/Effect/meteor_fire_smoke_03.dds"))
    {
        mResources->LoadTexture("Effect_MageMeteor_FireSmoke03", L"Textures/Effect/meteor_fire_smoke_03.dds");
    }
    if (std::filesystem::exists(L"Textures/Effect/meteor_fire_smoke_04.dds"))
    {
        mResources->LoadTexture("Effect_MageMeteor_FireSmoke04", L"Textures/Effect/meteor_fire_smoke_04.dds");
    }
    if (std::filesystem::exists(L"Textures/Effect/meteor_fire_smoke_05.dds"))
    {
        mResources->LoadTexture("Effect_MageMeteor_FireSmoke05", L"Textures/Effect/meteor_fire_smoke_05.dds");
    }
    if (std::filesystem::exists(L"Textures/Effect/meteor_fire_smoke_06.dds"))
    {
        mResources->LoadTexture("Effect_MageMeteor_FireSmoke06", L"Textures/Effect/meteor_fire_smoke_06.dds");
    }
    if (std::filesystem::exists(L"Textures/Effect/meteor_fire_smoke_07.dds"))
    {
        mResources->LoadTexture("Effect_MageMeteor_FireSmoke07", L"Textures/Effect/meteor_fire_smoke_07.dds");
    }
    if (std::filesystem::exists(L"Textures/Effect/meteor_fire_smoke_08.dds"))
    {
        mResources->LoadTexture("Effect_MageMeteor_FireSmoke08", L"Textures/Effect/meteor_fire_smoke_08.dds");
    }
    if (std::filesystem::exists(L"Textures/Effect/meteor_shockwave_ring_01.dds"))
    {
        mResources->LoadTexture("Effect_MageMeteor_ShockwaveRing01", L"Textures/Effect/meteor_shockwave_ring_01.dds");
    }
    if (std::filesystem::exists(L"Textures/Effect/meteor_shockwave_ring_02.dds"))
    {
        mResources->LoadTexture("Effect_MageMeteor_ShockwaveRing02", L"Textures/Effect/meteor_shockwave_ring_02.dds");
    }
    if (std::filesystem::exists(L"Textures/Effect/meteor_shockwave_ring_03.dds"))
    {
        mResources->LoadTexture("Effect_MageMeteor_ShockwaveRing03", L"Textures/Effect/meteor_shockwave_ring_03.dds");
    }
    if (std::filesystem::exists(L"Textures/Effect/meteor_shockwave_ring_04.dds"))
    {
        mResources->LoadTexture("Effect_MageMeteor_ShockwaveRing04", L"Textures/Effect/meteor_shockwave_ring_04.dds");
    }
    if (std::filesystem::exists(L"Textures/Effect/archer_wind_trace_03.dds"))
    {
        mResources->LoadTexture("Effect_ArcherWind_Trace03", L"Textures/Effect/archer_wind_trace_03.dds");
    }
    if (std::filesystem::exists(L"Textures/Effect/archer_wind_trace_04.dds"))
    {
        mResources->LoadTexture("Effect_ArcherWind_Trace04", L"Textures/Effect/archer_wind_trace_04.dds");
    }
    if (std::filesystem::exists(L"Textures/Effect/archer_wind_trail_00.dds"))
    {
        mResources->LoadTexture("Effect_ArcherWind_Trail00", L"Textures/Effect/archer_wind_trail_00.dds");
    }
    if (std::filesystem::exists(L"Textures/Effect/archer_wind_twirl_02.dds"))
    {
        mResources->LoadTexture("Effect_ArcherWind_Twirl02", L"Textures/Effect/archer_wind_twirl_02.dds");
    }
    if (std::filesystem::exists(L"Textures/Effect/archer_wind_slash_02.dds"))
    {
        mResources->LoadTexture("Effect_ArcherWind_Slash02", L"Textures/Effect/archer_wind_slash_02.dds");
    }
    if (std::filesystem::exists(L"Textures/Effect/archer_buff_circle4.dds"))
    {
        mResources->LoadTexture("Effect_ArcherBuff_Circle4", L"Textures/Effect/archer_buff_circle4.dds");
    }
    if (std::filesystem::exists(L"Textures/Effect/archer_buff_whirl2.dds"))
    {
        mResources->LoadTexture("Effect_ArcherBuff_Whirl2", L"Textures/Effect/archer_buff_whirl2.dds");
    }
    if (std::filesystem::exists(L"Textures/Effect/mage_heal_point_01.dds"))
    {
        mResources->LoadTexture("Effect_ArcherBuff_Point", L"Textures/Effect/mage_heal_point_01.dds");
    }
    if (std::filesystem::exists(L"Textures/Effect/archer_buff_arrow_01.dds"))
    {
        mResources->LoadTexture("Effect_ArcherBuff_Arrow", L"Textures/Effect/archer_buff_arrow_01.dds");
    }
    if (std::filesystem::exists(L"Textures/Effect/warrior_basic_slash_01.dds"))
    {
        mResources->LoadTexture("Effect_WarriorBasic_Slash", L"Textures/Effect/warrior_basic_slash_01.dds");
    }
    if (std::filesystem::exists(L"Textures/Effect/warrior_basic_mask_01.dds"))
    {
        mResources->LoadTexture("Effect_WarriorBasic_Mask", L"Textures/Effect/warrior_basic_mask_01.dds");
    }
    if (std::filesystem::exists(L"Textures/WindRibbon_Archer.dds"))
    {
        mResources->LoadTexture("WindRibbon_Archer", L"Textures/WindRibbon_Archer.dds");
    }
    if (std::filesystem::exists(L"Textures/Effect/earthshatter_crater_01.dds"))
    {
        mResources->LoadTexture("Effect_Earthshatter_Crater", L"Textures/Effect/earthshatter_crater_01.dds");
    }
    if (std::filesystem::exists(L"Textures/Effect/earthshatter_stone_01.dds"))
    {
        mResources->LoadTexture("Effect_Earthshatter_Stone", L"Textures/Effect/earthshatter_stone_01.dds");
    }
    if (std::filesystem::exists(L"Textures/Effect/earthshatter_smoke_01.dds"))
    {
        mResources->LoadTexture("Effect_Earthshatter_Smoke", L"Textures/Effect/earthshatter_smoke_01.dds");
    }
    if (std::filesystem::exists(L"Textures/P09_Weapon_Sword_03_Diff.dds"))
    {
        mResources->LoadTexture("WarriorLv1SwordTex", L"Textures/P09_Weapon_Sword_03_Diff.dds");
    }
    if (std::filesystem::exists(L"Textures/P09_Weapon_Sword_04_Diff.dds"))
    {
        mResources->LoadTexture("WarriorLv2SwordTex", L"Textures/P09_Weapon_Sword_04_Diff.dds");
    }
    if (std::filesystem::exists(L"Textures/P09_Weapon_Sword_05_Diff.dds"))
    {
        mResources->LoadTexture("WarriorLv3SwordTex", L"Textures/P09_Weapon_Sword_05_Diff.dds");
    }
    if (std::filesystem::exists(L"Textures/P09_Weapon_Shield_03_Diff.dds"))
    {
        mResources->LoadTexture("WarriorLv1ShieldTex", L"Textures/P09_Weapon_Shield_03_Diff.dds");
    }
    if (std::filesystem::exists(L"Textures/P09_Weapon_Shield_04_Diff.dds"))
    {
        mResources->LoadTexture("WarriorLv2ShieldTex", L"Textures/P09_Weapon_Shield_04_Diff.dds");
    }
    if (std::filesystem::exists(L"Textures/P09_Weapon_Shield_05_Diff.dds"))
    {
        mResources->LoadTexture("WarriorLv3ShieldTex", L"Textures/P09_Weapon_Shield_05_Diff.dds");
    }
    if (std::filesystem::exists(L"Textures/P09_Weapon_Bow_02_BaseMap.dds"))
    {
        mResources->LoadTexture("ArcherLv1BowTex", L"Textures/P09_Weapon_Bow_02_BaseMap.dds");
    }
    if (std::filesystem::exists(L"Textures/P09_Weapon_Bow_03_BaseMap.dds"))
    {
        mResources->LoadTexture("ArcherLv2BowTex", L"Textures/P09_Weapon_Bow_03_BaseMap.dds");
    }
    if (std::filesystem::exists(L"Textures/P09_Weapon_Bow_04_BaseMap.dds"))
    {
        mResources->LoadTexture("ArcherLv3BowTex", L"Textures/P09_Weapon_Bow_04_BaseMap.dds");
    }
    if (std::filesystem::exists(L"Textures/P09_Weapon_Staff_01_BaseMap.dds"))
    {
        mResources->LoadTexture("WizardLv1StaffTex", L"Textures/P09_Weapon_Staff_01_BaseMap.dds");
    }
    if (std::filesystem::exists(L"Textures/P09_Weapon_Staff_02_BaseMap.dds"))
    {
        mResources->LoadTexture("WizardLv2StaffTex", L"Textures/P09_Weapon_Staff_02_BaseMap.dds");
    }
    if (std::filesystem::exists(L"Textures/P09_Weapon_Staff_04_BaseMap.dds"))
    {
        mResources->LoadTexture("WizardLv3StaffTex", L"Textures/P09_Weapon_Staff_04_BaseMap.dds");
    }
    if (std::filesystem::exists(L"Textures/LanternIcon.dds"))
    {
        mResources->LoadTexture("LanternIcon", L"Textures/LanternIcon.dds");
    }
    if (std::filesystem::exists(L"Textures/UI/HPMP_Frame_1024x384_ratio.dds"))
    {
        mResources->LoadTexture("UI_HPMP_Frame", L"Textures/UI/HPMP_Frame_1024x384_ratio.dds");
    }
    if (std::filesystem::exists(L"Textures/UI/HP_Fill_1024x128.dds"))
    {
        mResources->LoadTexture("UI_HP_Fill", L"Textures/UI/HP_Fill_1024x128.dds");
    }
    if (std::filesystem::exists(L"Textures/UI/MP_Fill_1024x128.dds"))
    {
        mResources->LoadTexture("UI_MP_Fill", L"Textures/UI/MP_Fill_1024x128.dds");
    }
    if (std::filesystem::exists(L"Textures/UI/HPMP_Gloss_1024x256.dds"))
    {
        mResources->LoadTexture("UI_HPMP_Gloss", L"Textures/UI/HPMP_Gloss_1024x256.dds");
    }
    if (std::filesystem::exists(L"Textures/UI/ClassEmblem_Warrior_512x512.dds"))
    {
        mResources->LoadTexture("UI_ClassEmblem_Warrior", L"Textures/UI/ClassEmblem_Warrior_512x512.dds");
    }
    if (std::filesystem::exists(L"Textures/UI/ClassEmblem_Mage_512x512.dds"))
    {
        mResources->LoadTexture("UI_ClassEmblem_Mage", L"Textures/UI/ClassEmblem_Mage_512x512.dds");
    }
    if (std::filesystem::exists(L"Textures/UI/ClassEmblem_Archer_512x512.dds"))
    {
        mResources->LoadTexture("UI_ClassEmblem_Archer", L"Textures/UI/ClassEmblem_Archer_512x512.dds");
    }
    if (std::filesystem::exists(L"Textures/UI/BossHealthBar_Thin_Frame_2048x256.dds"))
    {
        mResources->LoadTexture("UI_BossHp_Frame", L"Textures/UI/BossHealthBar_Thin_Frame_2048x256.dds");
    }
    if (std::filesystem::exists(L"Textures/UI/MirrorCrackOverlay_1920x1080.dds"))
    {
        mResources->LoadTexture("UI_MirrorCrackOverlay", L"Textures/UI/MirrorCrackOverlay_1920x1080.dds");
    }
    if (std::filesystem::exists(L"Textures/UI/Lantern_Frame_512x512.dds"))
    {
        mResources->LoadTexture("UI_Lantern_Frame", L"Textures/UI/Lantern_Frame_512x512.dds");
    }
    if (std::filesystem::exists(L"Textures/UI/Lantern_Ring_Fill_512x512.dds"))
    {
        mResources->LoadTexture("UI_Lantern_Ring_Fill", L"Textures/UI/Lantern_Ring_Fill_512x512.dds");
    }
    if (std::filesystem::exists(L"Textures/UI/Lantern_Core_Glow_512x512.dds"))
    {
        mResources->LoadTexture("UI_Lantern_Core_Glow", L"Textures/UI/Lantern_Core_Glow_512x512.dds");
    }
    if (std::filesystem::exists(L"Textures/UI/SkillBar_TwoSlots_1024x512.dds"))
    {
        mResources->LoadTexture("UI_SkillBar_TwoSlots", L"Textures/UI/SkillBar_TwoSlots_1024x512.dds");
    }
    if (std::filesystem::exists(L"Textures/UI/Skill_Mage_HealingLight_512x512.dds"))
    {
        mResources->LoadTexture("UI_Skill_Mage_HealingLight", L"Textures/UI/Skill_Mage_HealingLight_512x512.dds");
    }
    if (std::filesystem::exists(L"Textures/UI/Skill_Mage_Meteor_512x512.dds"))
    {
        mResources->LoadTexture("UI_Skill_Mage_Meteor", L"Textures/UI/Skill_Mage_Meteor_512x512.dds");
    }
    if (std::filesystem::exists(L"Textures/UI/Skill_Warrior_EarthquakeSlam_512x512.dds"))
    {
        mResources->LoadTexture("UI_Skill_Warrior_EarthquakeSlam", L"Textures/UI/Skill_Warrior_EarthquakeSlam_512x512.dds");
    }
    if (std::filesystem::exists(L"Textures/UI/Skill_Warrior_GreatswordSummon_512x512.dds"))
    {
        mResources->LoadTexture("UI_Skill_Warrior_GreatswordSummon", L"Textures/UI/Skill_Warrior_GreatswordSummon_512x512.dds");
    }
    if (std::filesystem::exists(L"Textures/UI/Skill_Archer_WindImbuement_512x512.dds"))
    {
        mResources->LoadTexture("UI_Skill_Archer_WindImbuement", L"Textures/UI/Skill_Archer_WindImbuement_512x512.dds");
    }
    if (std::filesystem::exists(L"Textures/UI/Skill_Archer_ArrowRain_512x512.dds"))
    {
        mResources->LoadTexture("UI_Skill_Archer_ArrowRain", L"Textures/UI/Skill_Archer_ArrowRain_512x512.dds");
    }
    if (std::filesystem::exists(L"Textures/UI/Skill_Warrior_Dash_512x512.dds"))
    {
        mResources->LoadTexture("UI_Skill_Warrior_Dash", L"Textures/UI/Skill_Warrior_Dash_512x512.dds");
    }
    if (std::filesystem::exists(L"Textures/UI/Skill_Mage_Dash_512x512.dds"))
    {
        mResources->LoadTexture("UI_Skill_Mage_Dash", L"Textures/UI/Skill_Mage_Dash_512x512.dds");
    }
    if (std::filesystem::exists(L"Textures/UI/Skill_Archer_Dash_512x512.dds"))
    {
        mResources->LoadTexture("UI_Skill_Archer_Dash", L"Textures/UI/Skill_Archer_Dash_512x512.dds");
    }
    if (std::filesystem::exists(L"Textures/UI/DashCooldown_Frame_512x512.dds"))
    {
        mResources->LoadTexture("UI_DashCooldown_Frame", L"Textures/UI/DashCooldown_Frame_512x512.dds");
    }
    // Box Geometry
    std::array<Vertex, 8> boxVertices = {
        Vertex({ XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT2(0.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, 0.0f) }),
        Vertex({ XMFLOAT3(-1.0f, +1.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT2(0.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, 0.0f) }),
        Vertex({ XMFLOAT3(+1.0f, +1.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT2(0.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, 0.0f) }),
        Vertex({ XMFLOAT3(+1.0f, -1.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT2(0.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, 0.0f) }),
        Vertex({ XMFLOAT3(-1.0f, -1.0f, +1.0f), XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT2(0.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, 0.0f) }),
        Vertex({ XMFLOAT3(-1.0f, +1.0f, +1.0f), XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT2(0.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, 0.0f) }),
        Vertex({ XMFLOAT3(+1.0f, +1.0f, +1.0f), XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT2(0.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, 0.0f) }),
        Vertex({ XMFLOAT3(+1.0f, -1.0f, +1.0f), XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT2(0.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, 0.0f) })
    };
    std::array<std::uint16_t, 36> boxIndices = { 0,1,2,0,2,3, 4,6,5,4,7,6, 4,5,1,4,1,0, 3,2,6,3,6,7, 1,5,6,1,6,2, 4,0,3,4,3,7 };

    const UINT boxVbSize = (UINT)boxVertices.size() * sizeof(Vertex);
    const UINT boxIbSize = (UINT)boxIndices.size() * sizeof(std::uint16_t);
    auto boxGeo = std::make_unique<MeshGeometry>();
    boxGeo->Name = "boxGeo";
    D3DCreateBlob(boxVbSize, &boxGeo->VertexBufferCPU); CopyMemory(boxGeo->VertexBufferCPU->GetBufferPointer(), boxVertices.data(), boxVbSize);
    D3DCreateBlob(boxIbSize, &boxGeo->IndexBufferCPU);  CopyMemory(boxGeo->IndexBufferCPU->GetBufferPointer(), boxIndices.data(), boxIbSize);

    boxGeo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(), mCommandList.Get(), boxVertices.data(), boxVbSize, boxGeo->VertexBufferUploader);
    boxGeo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(), mCommandList.Get(), boxIndices.data(), boxIbSize, boxGeo->IndexBufferUploader);
    boxGeo->VertexByteStride = sizeof(Vertex); boxGeo->VertexBufferByteSize = boxVbSize;
    boxGeo->IndexFormat = DXGI_FORMAT_R16_UINT; boxGeo->IndexBufferByteSize = boxIbSize;

    SubmeshGeometry boxSubmesh;
    boxSubmesh.IndexCount = (UINT)boxIndices.size(); boxSubmesh.StartIndexLocation = 0; boxSubmesh.BaseVertexLocation = 0;
    boxGeo->DrawArgs["box"] = boxSubmesh;
    mResources->mGeometries[boxGeo->Name] = std::move(boxGeo);

    std::vector<Vertex> sphereVertices;
    std::vector<std::uint16_t> sphereIndices;

    float radius = 1.0f;
    UINT sliceCount = 30; // 占썸도 占쏙옙占쏙옙 占쏙옙
    UINT stackCount = 30; // 占쏙옙占쏙옙 占쏙옙占쏙옙 占쏙옙

    // 占쏙옙占쏙옙 占쏙옙占쏙옙 (North Pole)
    sphereVertices.push_back({ XMFLOAT3(0.0f, radius, 0.0f), XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT2(0.0f, 0.0f), XMFLOAT3(1.0f, 0.0f, 0.0f) });

    float phiStep = XM_PI / stackCount;
    float thetaStep = 2.0f * XM_PI / sliceCount;

    // 占쌩곤옙 占쏙옙占쏙옙占?占쏙옙占쏙옙
    for (UINT i = 1; i <= stackCount - 1; ++i) {
        float phi = i * phiStep;
        for (UINT j = 0; j <= sliceCount; ++j) {
            float theta = j * thetaStep;

            Vertex v;
            v.Pos.x = radius * sinf(phi) * cosf(theta);
            v.Pos.y = radius * cosf(phi);
            v.Pos.z = radius * sinf(phi) * sinf(theta);

            v.Normal = v.Pos;
            XMVECTOR n = XMVector3Normalize(XMLoadFloat3(&v.Normal));
            XMStoreFloat3(&v.Normal, n);

            v.TexC.x = (float)j / sliceCount;
            v.TexC.y = (float)i / stackCount;

            v.TangentU.x = -radius * sinf(phi) * sinf(theta);
            v.TangentU.y = 0.0f;
            v.TangentU.z = radius * sinf(phi) * cosf(theta);
            XMVECTOR t = XMVector3Normalize(XMLoadFloat3(&v.TangentU));
            XMStoreFloat3(&v.TangentU, t);

            sphereVertices.push_back(v);
        }
    }

    // 占싣뤄옙占쏙옙 占쏙옙占쏙옙 (South Pole)
    sphereVertices.push_back({ XMFLOAT3(0.0f, -radius, 0.0f), XMFLOAT3(0.0f, -1.0f, 0.0f), XMFLOAT2(0.0f, 1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f) });

    // 占싸듸옙占쏙옙 占쏙옙占쏙옙: 占쏙옙占쏙옙 캡
    for (UINT i = 1; i <= sliceCount; ++i) {
        sphereIndices.push_back(0);
        sphereIndices.push_back(i + 1);
        sphereIndices.push_back(i);
    }

    // 占싸듸옙占쏙옙 占쏙옙占쏙옙: 占쌩곤옙 占쏙옙占쏙옙占?
    UINT baseIndex = 1;
    UINT ringVertexCount = sliceCount + 1;
    for (UINT i = 0; i < stackCount - 2; ++i) {
        for (UINT j = 0; j < sliceCount; ++j) {
            sphereIndices.push_back(baseIndex + i * ringVertexCount + j);
            sphereIndices.push_back(baseIndex + i * ringVertexCount + j + 1);
            sphereIndices.push_back(baseIndex + (i + 1) * ringVertexCount + j);

            sphereIndices.push_back(baseIndex + (i + 1) * ringVertexCount + j);
            sphereIndices.push_back(baseIndex + i * ringVertexCount + j + 1);
            sphereIndices.push_back(baseIndex + (i + 1) * ringVertexCount + j + 1);
        }
    }

    // 占싸듸옙占쏙옙 占쏙옙占쏙옙: 占싣뤄옙占쏙옙 캡
    UINT southPoleIndex = (UINT)sphereVertices.size() - 1;
    baseIndex = southPoleIndex - ringVertexCount;
    for (UINT i = 0; i < sliceCount; ++i) {
        sphereIndices.push_back(southPoleIndex);
        sphereIndices.push_back(baseIndex + i);
        sphereIndices.push_back(baseIndex + i + 1);
    }

    // 占쏙옙체 占쏙옙占쏙옙占싶몌옙 GPU占쏙옙 占쏙옙占싸듸옙
    const UINT sphereVbSize = (UINT)sphereVertices.size() * sizeof(Vertex);
    const UINT sphereIbSize = (UINT)sphereIndices.size() * sizeof(std::uint16_t);
    auto sphereGeo = std::make_unique<MeshGeometry>();
    sphereGeo->Name = "sphereGeo";
    D3DCreateBlob(sphereVbSize, &sphereGeo->VertexBufferCPU); CopyMemory(sphereGeo->VertexBufferCPU->GetBufferPointer(), sphereVertices.data(), sphereVbSize);
    D3DCreateBlob(sphereIbSize, &sphereGeo->IndexBufferCPU);  CopyMemory(sphereGeo->IndexBufferCPU->GetBufferPointer(), sphereIndices.data(), sphereIbSize);

    sphereGeo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(), mCommandList.Get(), sphereVertices.data(), sphereVbSize, sphereGeo->VertexBufferUploader);
    sphereGeo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(), mCommandList.Get(), sphereIndices.data(), sphereIbSize, sphereGeo->IndexBufferUploader);
    sphereGeo->VertexByteStride = sizeof(Vertex); sphereGeo->VertexBufferByteSize = sphereVbSize;
    sphereGeo->IndexFormat = DXGI_FORMAT_R16_UINT; sphereGeo->IndexBufferByteSize = sphereIbSize;

    SubmeshGeometry sphereSubmesh;
    sphereSubmesh.IndexCount = (UINT)sphereIndices.size(); sphereSubmesh.StartIndexLocation = 0; sphereSubmesh.BaseVertexLocation = 0;
    sphereGeo->DrawArgs["sphere"] = sphereSubmesh;
    mResources->mGeometries[sphereGeo->Name] = std::move(sphereGeo);

    if (std::filesystem::exists("Models/Weapons/Arrow.fbx") &&
        mResources->mGeometries.find("archerBasicArrowGeo") == mResources->mGeometries.end())
    {
        auto arrowGeo = BuildStaticModelGeometry(
            md3dDevice.Get(),
            mCommandList.Get(),
            "archerBasicArrowGeo",
            "Models/Weapons/Arrow.fbx",
            1.15f,
            { 0.0f, 0.0f, 0.0f });
        if (arrowGeo != nullptr)
        {
            mResources->mGeometries[arrowGeo->Name] = std::move(arrowGeo);
        }
    }

    if (std::filesystem::exists("Models/Weapons/Warrior_Lv3_Sword.fbx") &&
        mResources->mGeometries.find("warriorLv3SwordGeo") == mResources->mGeometries.end())
    {
        auto swordGeo = BuildStaticModelGeometry(
            md3dDevice.Get(),
            mCommandList.Get(),
            "warriorLv3SwordGeo",
            "Models/Weapons/Warrior_Lv3_Sword.fbx",
            1.0f,
            { 0.0f, 0.0f, 0.0f });
        if (swordGeo != nullptr)
        {
            mResources->mGeometries[swordGeo->Name] = std::move(swordGeo);
        }
    }

    // 3. 占쏙옙占쏙옙 占쏙옙占쏙옙 占쏙옙占쏙옙 
    mResources->CreateMaterial("Fire_Mat", static_cast<int>(mResources->mMaterials.size()), "Fire_1", "", "Fire_1", "", XMFLOAT4(1.0f, 0.34f, 0.09f, 0.82f), XMFLOAT3(0.1f, 0.1f, 0.1f), 0.1f);
    if (auto mat = mResources->GetMaterial("Fire_Mat")) { mat->IsTransparent = 1; mat->NumFramesDirty = 3; }

    mResources->CreateMaterial("PlayerBlue", static_cast<int>(mResources->mMaterials.size()), "Blue", "", "", "", XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f), XMFLOAT3(0.04f, 0.04f, 0.04f), 0.8f);
    if (auto mat = mResources->GetMaterial("PlayerBlue")) mat->NumFramesDirty = 3;

    mResources->CreateMaterial("MonsterRed", static_cast<int>(mResources->mMaterials.size()), "white", "", "", "", XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f), XMFLOAT3(0.04f, 0.04f, 0.04f), 0.8f);
    if (auto mat = mResources->GetMaterial("MonsterRed")) mat->NumFramesDirty = 3;

    mResources->CreateMaterial("MonsterOrange", static_cast<int>(mResources->mMaterials.size()), "white", "", "", "", XMFLOAT4(1.0f, 0.35f, 0.05f, 1.0f), XMFLOAT3(0.04f, 0.04f, 0.04f), 0.75f);
    if (auto mat = mResources->GetMaterial("MonsterOrange")) mat->NumFramesDirty = 3;

    auto createEquipmentMaterial = [this](const std::string& materialName, const std::string& textureName, float roughness)
    {
        mResources->CreateMaterial(
            materialName,
            static_cast<int>(mResources->mMaterials.size()),
            mResources->GetTexture(textureName) ? textureName : "white",
            "",
            "",
            "",
            XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f),
            XMFLOAT3(0.08f, 0.08f, 0.08f),
            roughness);

        if (auto mat = mResources->GetMaterial(materialName))
        {
            mat->IsToon = 1;
            mat->OutlineThickness = 0.008f;
            mat->NumFramesDirty = 3;
        }
    };

    createEquipmentMaterial("PlayerSwordLv1Mat", "WarriorLv1SwordTex", 0.35f);
    createEquipmentMaterial("PlayerSwordLv2Mat", "WarriorLv2SwordTex", 0.35f);
    createEquipmentMaterial("PlayerSwordMat", "WarriorLv3SwordTex", 0.35f);
    createEquipmentMaterial("PlayerShieldLv1Mat", "WarriorLv1ShieldTex", 0.35f);
    createEquipmentMaterial("PlayerShieldLv2Mat", "WarriorLv2ShieldTex", 0.35f);
    createEquipmentMaterial("PlayerShieldMat", "WarriorLv3ShieldTex", 0.35f);
    createEquipmentMaterial("PlayerBowLv1Mat", "ArcherLv1BowTex", 0.38f);
    createEquipmentMaterial("PlayerBowLv2Mat", "ArcherLv2BowTex", 0.38f);
    createEquipmentMaterial("PlayerBowMat", "ArcherLv3BowTex", 0.38f);
    createEquipmentMaterial("PlayerStaffLv1Mat", "WizardLv1StaffTex", 0.38f);
    createEquipmentMaterial("PlayerStaffLv2Mat", "WizardLv2StaffTex", 0.38f);
    createEquipmentMaterial("PlayerStaffMat", "WizardLv3StaffTex", 0.38f);

    mResources->CreateMaterial("ArcherArrowMat", static_cast<int>(mResources->mMaterials.size()),
        "white", "", "", "",
        XMFLOAT4(1.35f, 1.08f, 0.62f, 1.0f), XMFLOAT3(0.05f, 0.05f, 0.05f), 0.55f);
    if (auto mat = mResources->GetMaterial("ArcherArrowMat")) { mat->IsToon = 1; mat->OutlineThickness = 0.006f; mat->NumFramesDirty = 3; }

    mResources->CreateMaterial("DomainMat", static_cast<int>(mResources->mMaterials.size()), "MagicCircle", "", "", "",
        XMFLOAT4(0.1f, 0.3f, 1.0f, 1.0f), XMFLOAT3(0.5f, 0.5f, 0.5f), 0.1f);
    if (auto domainMat = mResources->GetMaterial("DomainMat"))
    {
        domainMat->IsTransparent = 1; 
        domainMat->NumFramesDirty = 3;
    }

    // 원격 플레이어가 나중에 생성되더라도 클래스별 바디/애니메이션 리소스는
    // 씬 진입 중에 미리 GPU 업로드 경로를 타게 합니다.
    WarmAllGameplayCharacterVisuals(
        mResources.get(),
        md3dDevice.Get(),
        mCommandList.Get());

    // 레벨 전환 중 새 장비 geometry를 런타임에 처음 업로드하지 않도록
    // 모든 직업/레벨 장비 모델도 씬 진입 시 미리 GPU 업로드 경로를 태웁니다.
    WarmAllGameplayEquipmentGeometries(
        mResources.get(),
        md3dDevice.Get(),
        mCommandList.Get());

    // 4. 占쏙옙占쏙옙占쏙옙트 占쏙옙占쏙옙
    BuildPlayer();

    // 5. 占시뤄옙占싱억옙 占쏙옙占쏙옙 占십깍옙화
    RefreshPlayerForSelectedClass();

	// 6. UI 占시쏙옙占쏙옙 占십깍옙화
    mUIManager = std::make_unique<UIManager>(this);
    mUIManager->BuildInGameUI();
    BuildMirrorBreakResources();
    BuildMirrorBreakQuad();
    BuildVolumetricFogQuad();
    BuildPostProcessQuad();

    mIsSharedResourcesLoaded = true;
    BuildDescriptorHeaps();
}

void EclipseWalkerGame::BuildMirrorBreakResources()
{
    if (md3dDevice == nullptr || mResources == nullptr || mClientWidth <= 0 || mClientHeight <= 0)
    {
        return;
    }

    if (mMirrorBreakRtvHeap == nullptr)
    {
        D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
        rtvHeapDesc.NumDescriptors = 1;
        rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&mMirrorBreakRtvHeap)));
    }

    D3D12_RESOURCE_DESC sceneColorDesc = CD3DX12_RESOURCE_DESC::Tex2D(
        DXGI_FORMAT_R8G8B8A8_UNORM,
        static_cast<UINT64>(mClientWidth),
        static_cast<UINT>(mClientHeight),
        1,
        1,
        1,
        0,
        D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);

    D3D12_CLEAR_VALUE clearValue = {};
    clearValue.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    clearValue.Color[0] = 0.690196097f;
    clearValue.Color[1] = 0.768627465f;
    clearValue.Color[2] = 0.870588243f;
    clearValue.Color[3] = 1.0f;

    CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);
    ThrowIfFailed(md3dDevice->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &sceneColorDesc,
        D3D12_RESOURCE_STATE_COMMON,
        &clearValue,
        IID_PPV_ARGS(&mMirrorBreakSceneColor)));

    md3dDevice->CreateRenderTargetView(
        mMirrorBreakSceneColor.Get(),
        nullptr,
        mMirrorBreakRtvHeap->GetCPUDescriptorHandleForHeapStart());
    mMirrorBreakSceneColorState = D3D12_RESOURCE_STATE_COMMON;

    auto& textureEntry = mResources->mTextures["PostSceneColor"];
    if (textureEntry == nullptr)
    {
        textureEntry = std::make_unique<Texture>();
    }

    textureEntry->Name = "PostSceneColor";
    textureEntry->Filename = L"";
    textureEntry->Resource = mMirrorBreakSceneColor;
    textureEntry->UploadHeap.Reset();

    if (auto* mirrorBreakMat = mResources->GetMaterial("MirrorBreakSceneMat"))
    {
        mirrorBreakMat->NumFramesDirty = gNumFrameResources;
    }
}

void EclipseWalkerGame::BuildMirrorBreakQuad()
{
    if (mResources == nullptr)
    {
        return;
    }

    Material* mirrorBreakMat = mResources->GetMaterial("MirrorBreakSceneMat");
    if (mirrorBreakMat == nullptr)
    {
        mResources->CreateMaterial(
            "MirrorBreakSceneMat",
            static_cast<int>(mResources->mMaterials.size()),
            "PostSceneColor",
            "",
            "UI_MirrorCrackOverlay",
            "",
            XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f),
            XMFLOAT3(0.02f, 0.02f, 0.02f),
            1.0f);
        mirrorBreakMat = mResources->GetMaterial("MirrorBreakSceneMat");
    }

    if (mirrorBreakMat == nullptr)
    {
        return;
    }

    mirrorBreakMat->DiffuseMapName = "PostSceneColor";
    mirrorBreakMat->EmissiveMapName = "UI_MirrorCrackOverlay";
    mirrorBreakMat->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    mirrorBreakMat->IsTransparent = 0;
    mirrorBreakMat->NumFramesDirty = gNumFrameResources;

    if (mMirrorBreakObject == nullptr)
    {
        auto renderItem = std::make_unique<RenderItem>();
        renderItem->Geo = mResources->mGeometries["quadGeo"].get();
        renderItem->Mat = mirrorBreakMat;
        renderItem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        renderItem->ObjCBIndex = static_cast<UINT>(mAllRitems.size());
        renderItem->IndexCount = renderItem->Geo->DrawArgs["quad"].IndexCount;
        renderItem->StartIndexLocation = renderItem->Geo->DrawArgs["quad"].StartIndexLocation;
        renderItem->BaseVertexLocation = renderItem->Geo->DrawArgs["quad"].BaseVertexLocation;
        renderItem->Visible = true;

        mMirrorBreakRitem = renderItem.get();
        mAllRitems.push_back(std::move(renderItem));

        mMirrorBreakObject = std::make_unique<GameObject>();
        mMirrorBreakObject->Ritem = mMirrorBreakRitem;
        mMirrorBreakObject->SetScale(1.0f, 1.0f, 1.0f);
        mMirrorBreakObject->SetPosition(0.0f, 0.0f, 0.0f);
        mMirrorBreakObject->Update();
    }
    else if (mMirrorBreakRitem != nullptr)
    {
        mMirrorBreakRitem->Mat = mirrorBreakMat;
        mMirrorBreakRitem->NumFramesDirty = gNumFrameResources;
    }
}

void EclipseWalkerGame::BuildVolumetricFogQuad()
{
    if (mResources == nullptr)
    {
        return;
    }

    Material* fogMat = mResources->GetMaterial("VolumetricFogCompositeMat");
    if (fogMat == nullptr)
    {
        mResources->CreateMaterial(
            "VolumetricFogCompositeMat",
            static_cast<int>(mResources->mMaterials.size()),
            "PostSceneColor",
            "",
            "",
            "",
            XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f),
            XMFLOAT3(0.02f, 0.02f, 0.02f),
            1.0f);
        fogMat = mResources->GetMaterial("VolumetricFogCompositeMat");
    }

    if (fogMat == nullptr)
    {
        return;
    }

    fogMat->DiffuseMapName = "PostSceneColor";
    fogMat->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    fogMat->IsTransparent = 0;
    fogMat->NumFramesDirty = gNumFrameResources;

    if (mVolumetricFogObject == nullptr)
    {
        auto renderItem = std::make_unique<RenderItem>();
        renderItem->Geo = mResources->mGeometries["quadGeo"].get();
        renderItem->Mat = fogMat;
        renderItem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        renderItem->ObjCBIndex = static_cast<UINT>(mAllRitems.size());
        renderItem->IndexCount = renderItem->Geo->DrawArgs["quad"].IndexCount;
        renderItem->StartIndexLocation = renderItem->Geo->DrawArgs["quad"].StartIndexLocation;
        renderItem->BaseVertexLocation = renderItem->Geo->DrawArgs["quad"].BaseVertexLocation;
        renderItem->Visible = true;

        mVolumetricFogRitem = renderItem.get();
        mAllRitems.push_back(std::move(renderItem));

        mVolumetricFogObject = std::make_unique<GameObject>();
        mVolumetricFogObject->Ritem = mVolumetricFogRitem;
        mVolumetricFogObject->SetScale(1.0f, 1.0f, 1.0f);
        mVolumetricFogObject->SetPosition(0.0f, 0.0f, 0.0f);
        mVolumetricFogObject->Update();
    }
    else if (mVolumetricFogRitem != nullptr)
    {
        mVolumetricFogRitem->Mat = fogMat;
        mVolumetricFogRitem->NumFramesDirty = gNumFrameResources;
    }
}

void EclipseWalkerGame::BuildPostProcessQuad()
{
    if (mResources == nullptr)
    {
        return;
    }

    Material* postMat = mResources->GetMaterial("PostProcessSceneMat");
    if (postMat == nullptr)
    {
        mResources->CreateMaterial(
            "PostProcessSceneMat",
            static_cast<int>(mResources->mMaterials.size()),
            "PostSceneColor",
            "",
            "",
            "",
            XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f),
            XMFLOAT3(0.02f, 0.02f, 0.02f),
            1.0f);
        postMat = mResources->GetMaterial("PostProcessSceneMat");
    }

    if (postMat == nullptr)
    {
        return;
    }

    postMat->DiffuseMapName = "PostSceneColor";
    postMat->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    postMat->IsTransparent = 0;
    postMat->NumFramesDirty = gNumFrameResources;

    if (mPostProcessObject == nullptr)
    {
        auto renderItem = std::make_unique<RenderItem>();
        renderItem->Geo = mResources->mGeometries["quadGeo"].get();
        renderItem->Mat = postMat;
        renderItem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        renderItem->ObjCBIndex = static_cast<UINT>(mAllRitems.size());
        renderItem->IndexCount = renderItem->Geo->DrawArgs["quad"].IndexCount;
        renderItem->StartIndexLocation = renderItem->Geo->DrawArgs["quad"].StartIndexLocation;
        renderItem->BaseVertexLocation = renderItem->Geo->DrawArgs["quad"].BaseVertexLocation;
        renderItem->Visible = true;
        renderItem->CastShadow = false;

        mPostProcessRitem = renderItem.get();
        mAllRitems.push_back(std::move(renderItem));

        mPostProcessObject = std::make_unique<GameObject>();
        mPostProcessObject->Ritem = mPostProcessRitem;
        mPostProcessObject->SetScale(1.0f, 1.0f, 1.0f);
        mPostProcessObject->SetPosition(0.0f, 0.0f, 0.0f);
        mPostProcessObject->Update();
    }
    else if (mPostProcessRitem != nullptr)
    {
        mPostProcessRitem->Mat = postMat;
        mPostProcessRitem->NumFramesDirty = gNumFrameResources;
    }
}

bool EclipseWalkerGame::ShouldDrawMirrorBreakEffect() const
{
    return mMirrorBreakEffectActive &&
        mMirrorBreakEffectProgress > 0.0001f &&
        mMirrorBreakSceneColor != nullptr &&
        mMirrorBreakObject != nullptr &&
        mMirrorBreakObject->Ritem != nullptr &&
        mResources != nullptr &&
        mResources->GetTextureIndex("PostSceneColor") >= 0 &&
        mResources->GetTextureIndex("UI_MirrorCrackOverlay") >= 0;
}

bool EclipseWalkerGame::ShouldDrawVolumetricFog() const
{
    return m4xMsaaState &&
        mMirrorBreakSceneColor != nullptr &&
        mVolumetricFogObject != nullptr &&
        mVolumetricFogObject->Ritem != nullptr &&
        mResources != nullptr &&
        mResources->GetTextureIndex("PostSceneColor") >= 0;
}

bool EclipseWalkerGame::ShouldDrawPostProcess() const
{
    return mPostProcessEnabled &&
        m4xMsaaState &&
        mMirrorBreakSceneColor != nullptr &&
        mPostProcessObject != nullptr &&
        mPostProcessObject->Ritem != nullptr &&
        mResources != nullptr &&
        mResources->GetTextureIndex("PostSceneColor") >= 0;
}

D3D12_CPU_DESCRIPTOR_HANDLE EclipseWalkerGame::MirrorBreakRenderTargetView() const
{
    return mMirrorBreakRtvHeap != nullptr
        ? mMirrorBreakRtvHeap->GetCPUDescriptorHandleForHeapStart()
        : D3D12_CPU_DESCRIPTOR_HANDLE{};
}

void EclipseWalkerGame::UnloadSharedGameResources()
{
    ResetRuntimeSceneObjectRefs();
    mIsSharedResourcesLoaded = false;
}

void EclipseWalkerGame::OnResize()
{
    GameFramework::OnResize();
    mCamera.SetLens(0.25f * 3.14f, AspectRatio(), 1.0f, 10000.0f);

    if (mResources != nullptr)
    {
        BuildMirrorBreakResources();
        BuildDescriptorHeaps();
    }
}

void EclipseWalkerGame::Update(const GameTimer& gt)
{
    mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % 3;
    mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();
    AudioManager::Get().Update(gt.DeltaTime());

    if (mCurrFrameResource->Fence != 0 && mFence->GetCompletedValue() < mCurrFrameResource->Fence)
    {
        HANDLE eventHandle = CreateEventEx(nullptr, nullptr, false, EVENT_ALL_ACCESS);
        ThrowIfFailed(mFence->SetEventOnCompletion(mCurrFrameResource->Fence, eventHandle));
        WaitForSingleObject(eventHandle, INFINITE);
        CloseHandle(eventHandle);
    }

    OnKeyboardInput(gt);

    if (DebugConfig::kEnableBackendConnection)
    {
        NetworkManager::Get()->ProcessPackets(DebugConfig::kMaxNetworkPacketsPerFrame);
        const int targetStage = NetworkManager::Get()->ConsumeStageChangeSignal();
        if (targetStage == 2 && dynamic_cast<Stage2Scene*>(mCurrentScene.get()) == nullptr)
        {
            const float stageElapsedSeconds = NetworkManager::Get()->ConsumeStageElapsedSeconds();
            RequestSceneChange(std::make_unique<Stage2Scene>(this, stageElapsedSeconds), L"LOADING STAGE 2");
        }
    }

    // [占쏙옙 占쏙옙占쏙옙占쏙옙트 호占쏙옙]
    if (mCurrentScene) mCurrentScene->Update(gt);
    UpdateFireAmbientAudio();
    UpdateLavaAmbientAudio();

    XMFLOAT3 camPos = mCamera.GetPosition3f();
    mCamera.UpdateViewMatrix();

    for (auto& obj : mGameObjects)
    {
        if (auto* skeletalAnimation = obj->GetSkeletalAnimation())
        {
            skeletalAnimation->Update(gt.DeltaTime());
        }

        if (obj->mIsBillboard)
        {
            XMFLOAT3 firePos = obj->GetPosition();
            float dx = camPos.x - firePos.x;
            float dz = camPos.z - firePos.z;
            obj->SetRotation(0.0f, atan2(dx, dz), 0.0f);
        }
        obj->Update();
        obj->UpdateAnimation(gt.DeltaTime());
        if (obj->mLightIndex != -1)
        {
            float flickerSpeed = 3.0f;
            float baseFlicker = 0.8f + 0.2f * sinf(gt.TotalTime() * flickerSpeed);
            float noise = (float)(rand() % 100) / 2000.0f;
            float intensity = baseFlicker + noise;
            mGameLights[obj->mLightIndex].SetStrength({ 2.8f * intensity, 0.65f * intensity, 0.16f * intensity });
        }
    }

    mSocketAttachmentSystem.Update();
    SyncPlayerSkinOverlays();

    if (auto* loadingScene = dynamic_cast<LoadingScene*>(mCurrentScene.get()))
    {
        loadingScene->EnforceHiddenGameplayRenderItems();
    }

    for (auto& item : mAllRitems)
    {
        if (item && item->IsSkybox)
        {
            DirectX::XMStoreFloat4x4(&item->World, XMMatrixScaling(5000.0f, 5000.0f, 5000.0f));
            item->NumFramesDirty = gNumFrameResources;
            break;
        }
    }

    UpdateMainPassCB(gt);
    UpdateShadowPassCB(gt);


    const bool isStage1 = dynamic_cast<Stage1Scene*>(mCurrentScene.get()) != nullptr;
    const bool isStage2 = dynamic_cast<Stage2Scene*>(mCurrentScene.get()) != nullptr;
    const bool isVillage = dynamic_cast<VillageScene*>(mCurrentScene.get()) != nullptr;

    if (mUIManager && mPlayer && (isStage1 || isStage2 || isVillage))
    {
        float curHp = mPlayer->GetHP();
        float maxHp = mPlayer->GetMaxHP();
        float curMp = mPlayer->GetMP();
        float maxMp = mPlayer->GetMaxMP();
        float curLantern = 0.0f;
        float maxLantern = 0.0f;
        float curDashCooldown = mPlayer->GetDashCooldownRemaining();
        float maxDashCooldown = mPlayer->GetDashCooldownDuration();
        float curExpRatio = mPlayer->GetExperienceProgressRatio();

        if (auto lantern = mPlayer->GetLantern())
        {
            curLantern = lantern->GetGauge();
            maxLantern = lantern->GetMaxGauge();
        }

        mUIManager->Update(curHp, maxHp, curMp, maxMp, curLantern, maxLantern, curDashCooldown, maxDashCooldown, curExpRatio);
        mUIManager->UpdateEffect(gt.DeltaTime());
    }

    UpdateObjectCBs(gt);
    UpdateSkinnedCBs(gt);
    UpdateMaterialCBs(gt);
    UpdateUIPassCB(gt);
    if (DebugConfig::kEnableBackendConnection && (isStage1 || isStage2))
    {
        UpdateRemotePlayers(gt.DeltaTime());
    }
}

static const float ClearColor[4] = { 0.690196097f, 0.768627465f, 0.870588243f, 1.0f };

void EclipseWalkerGame::Draw(const GameTimer& gt)
{
    auto cmdListAlloc = mCurrFrameResource->CmdListAlloc;
    ThrowIfFailed(cmdListAlloc->Reset());
    ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), nullptr));

    const bool isCharSelect = dynamic_cast<CharSelectScene*>(mCurrentScene.get()) != nullptr;
    auto shadowMap = mRenderer->GetShadowMap();

    // [Pass 1] Shadow
    auto barrierShadowWrite = CD3DX12_RESOURCE_BARRIER::Transition(shadowMap->Resource(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE);
    mCommandList->ResourceBarrier(1, &barrierShadowWrite);

    D3D12_VIEWPORT shadowViewport = shadowMap->Viewport(); D3D12_RECT shadowScissorRect = shadowMap->ScissorRect();
    mCommandList->RSSetViewports(1, &shadowViewport); mCommandList->RSSetScissorRects(1, &shadowScissorRect);

    D3D12_CPU_DESCRIPTOR_HANDLE shadowDsv = shadowMap->Dsv();
    mCommandList->OMSetRenderTargets(0, nullptr, false, &shadowDsv);
    mCommandList->ClearDepthStencilView(shadowDsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    if (!isCharSelect)
    {
        mRenderer->DrawScene(mCommandList.Get(), mGameObjects, mCurrFrameResource->PassCB->Resource(), mResources->GetSrvHeap(), mCurrFrameResource->ObjectCB->Resource(), mCurrFrameResource->SkinnedCB->Resource(), mCurrFrameResource->MaterialCB->Resource(), mRenderer->GetShadowPSO(), 1);
    }

    auto barrierShadowRead = CD3DX12_RESOURCE_BARRIER::Transition(shadowMap->Resource(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_GENERIC_READ);
    mCommandList->ResourceBarrier(1, &barrierShadowRead);

    // [Pass 2] Main
    mCommandList->RSSetViewports(1, &mScreenViewport);
    mCommandList->RSSetScissorRects(1, &mScissorRect);

    auto* stage1Scene = dynamic_cast<Stage1Scene*>(mCurrentScene.get());
    auto* stage2Scene = dynamic_cast<Stage2Scene*>(mCurrentScene.get());
    const bool isStage1 = stage1Scene != nullptr;
    const bool isStage2 = stage2Scene != nullptr;
    const bool isVillage = dynamic_cast<VillageScene*>(mCurrentScene.get()) != nullptr;
    const bool isWorldTransitionActive =
        (stage1Scene != nullptr && stage1Scene->IsTransitionActive()) ||
        (stage2Scene != nullptr && stage2Scene->IsTransitionActive());
    const bool shouldDrawMirrorBreak = (isStage1 || isStage2) && ShouldDrawMirrorBreakEffect();
    const bool shouldDrawVolumetricFog = isStage1 && !shouldDrawMirrorBreak && ShouldDrawVolumetricFog();
    const bool shouldDrawPostProcess =
        (isStage1 || isStage2) &&
        !isWorldTransitionActive &&
        !shouldDrawMirrorBreak &&
        !shouldDrawVolumetricFog &&
        ShouldDrawPostProcess();
    const bool uiUsesBackBufferWithoutDsv = (shouldDrawMirrorBreak && m4xMsaaState) || shouldDrawVolumetricFog;
    bool postProcessApplied = false;

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle;
    CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(DepthStencilView());
    if (shouldDrawMirrorBreak && !m4xMsaaState)
    {
        if (mMirrorBreakSceneColorState != D3D12_RESOURCE_STATE_RENDER_TARGET)
        {
            auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
                mMirrorBreakSceneColor.Get(),
                mMirrorBreakSceneColorState,
                D3D12_RESOURCE_STATE_RENDER_TARGET);
            mCommandList->ResourceBarrier(1, &barrier);
            mMirrorBreakSceneColorState = D3D12_RESOURCE_STATE_RENDER_TARGET;
        }
        rtvHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(MirrorBreakRenderTargetView());
    }
    else if (m4xMsaaState)
    {
        rtvHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(mRtvHeap->GetCPUDescriptorHandleForHeapStart()).Offset(2, mRtvDescriptorSize);
    }
    else
    {
        auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            CurrentBackBuffer(),
            D3D12_RESOURCE_STATE_PRESENT,
            D3D12_RESOURCE_STATE_RENDER_TARGET);
        mCommandList->ResourceBarrier(1, &barrier);
        rtvHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(CurrentBackBufferView());
    }

    mCommandList->ClearRenderTargetView(rtvHandle, ClearColor, 0, nullptr);
    mCommandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
    mCommandList->OMSetRenderTargets(1, &rtvHandle, true, &dsvHandle);

    const char* skyTextureName = isStage1 ? "sky_stage1" : (isStage2 ? "sky_stage2" : (isVillage ? "sky_village" : "sky"));
    int skyIdx = mResources->GetTextureIndex(skyTextureName);
    if (skyIdx == -1)
    {
        skyIdx = mResources->GetTextureIndex("sky");
    }
    if (skyIdx != -1)
    {
        int skyEclipseIdx = mResources->GetTextureIndex("white");

        mRenderer->DrawSkybox(
            mCommandList.Get(),
            mAllRitems,
            mResources->GetSrvHeap(),
            skyIdx,
            skyEclipseIdx,
            mCurrFrameResource->ObjectCB->Resource(),
            mCurrFrameResource->PassCB->Resource());
    }

    mRenderer->DrawScene(
        mCommandList.Get(),
        mGameObjects,
        mCurrFrameResource->PassCB->Resource(),
        mResources->GetSrvHeap(),
        mCurrFrameResource->ObjectCB->Resource(),
        mCurrFrameResource->SkinnedCB->Resource(),
        mCurrFrameResource->MaterialCB->Resource(),
        mRenderer->GetPSO(),
        0);
    mRenderer->DrawScene(
        mCommandList.Get(),
        mGameObjects,
        mCurrFrameResource->PassCB->Resource(),
        mResources->GetSrvHeap(),
        mCurrFrameResource->ObjectCB->Resource(),
        mCurrFrameResource->SkinnedCB->Resource(),
        mCurrFrameResource->MaterialCB->Resource(),
        mRenderer->GetOutlinePSO(),
        0);

    std::vector<GameObject*> normalTransObjs;
    std::vector<GameObject*> fogVolumeObjs;
    std::vector<GameObject*> domainObjs;
    for (auto& obj : mGameObjects)
    {
        if (obj->Ritem != nullptr && obj->Ritem->Mat != nullptr)
        {
            if (obj->Ritem->Mat->Name == "DomainMat")
            {
                domainObjs.push_back(obj.get());
            }
            else if (obj->Ritem->Mat->IsTransparent == 2)
            {
                fogVolumeObjs.push_back(obj.get());
            }
            else
            {
                normalTransObjs.push_back(obj.get());
            }
        }
        else
        {
            normalTransObjs.push_back(obj.get());
        }
    }

    mRenderer->DrawScene(
        mCommandList.Get(),
        normalTransObjs,
        mCurrFrameResource->PassCB->Resource(),
        mResources->GetSrvHeap(),
        mCurrFrameResource->ObjectCB->Resource(),
        mCurrFrameResource->SkinnedCB->Resource(),
        mCurrFrameResource->MaterialCB->Resource(),
        mRenderer->GetTransparentPSO(),
        0);
    mRenderer->DrawScene(
        mCommandList.Get(),
        fogVolumeObjs,
        mCurrFrameResource->PassCB->Resource(),
        mResources->GetSrvHeap(),
        mCurrFrameResource->ObjectCB->Resource(),
        mCurrFrameResource->SkinnedCB->Resource(),
        mCurrFrameResource->MaterialCB->Resource(),
        mRenderer->GetFogVolumePSO(),
        0);
    mRenderer->DrawScene(
        mCommandList.Get(),
        domainObjs,
        mCurrFrameResource->PassCB->Resource(),
        mResources->GetSrvHeap(),
        mCurrFrameResource->ObjectCB->Resource(),
        mCurrFrameResource->SkinnedCB->Resource(),
        mCurrFrameResource->MaterialCB->Resource(),
        mRenderer->GetDistortionPSO(),
        0);

    if (shouldDrawVolumetricFog)
    {
        D3D12_RESOURCE_BARRIER resolveToFogSourceBarriers[2] =
        {
            CD3DX12_RESOURCE_BARRIER::Transition(mMSAART.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_RESOLVE_SOURCE),
            CD3DX12_RESOURCE_BARRIER::Transition(mMirrorBreakSceneColor.Get(), mMirrorBreakSceneColorState, D3D12_RESOURCE_STATE_RESOLVE_DEST)
        };
        mCommandList->ResourceBarrier(2, resolveToFogSourceBarriers);
        mMirrorBreakSceneColorState = D3D12_RESOURCE_STATE_RESOLVE_DEST;

        mCommandList->ResolveSubresource(
            mMirrorBreakSceneColor.Get(),
            0,
            mMSAART.Get(),
            0,
            DXGI_FORMAT_R8G8B8A8_UNORM);

        D3D12_RESOURCE_BARRIER fogSampleBarriers[3] =
        {
            CD3DX12_RESOURCE_BARRIER::Transition(mMSAART.Get(), D3D12_RESOURCE_STATE_RESOLVE_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET),
            CD3DX12_RESOURCE_BARRIER::Transition(mMirrorBreakSceneColor.Get(), mMirrorBreakSceneColorState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE),
            CD3DX12_RESOURCE_BARRIER::Transition(mDepthStencilBuffer.Get(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
        };
        mCommandList->ResourceBarrier(3, fogSampleBarriers);
        mMirrorBreakSceneColorState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

        auto backBufferToRenderTarget = CD3DX12_RESOURCE_BARRIER::Transition(
            CurrentBackBuffer(),
            D3D12_RESOURCE_STATE_PRESENT,
            D3D12_RESOURCE_STATE_RENDER_TARGET);
        mCommandList->ResourceBarrier(1, &backBufferToRenderTarget);

        auto backBufferHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(CurrentBackBufferView());
        mCommandList->OMSetRenderTargets(1, &backBufferHandle, true, nullptr);

        std::vector<GameObject*> volumetricFogObjs;
        volumetricFogObjs.push_back(mVolumetricFogObject.get());
        mRenderer->DrawScene(
            mCommandList.Get(),
            volumetricFogObjs,
            mCurrFrameResource->PassCB->Resource(),
            mResources->GetSrvHeap(),
            mCurrFrameResource->ObjectCB->Resource(),
            mCurrFrameResource->SkinnedCB->Resource(),
            mCurrFrameResource->MaterialCB->Resource(),
            mRenderer->GetVolumetricFogPSO(),
            0);

        auto depthBackToWrite = CD3DX12_RESOURCE_BARRIER::Transition(
            mDepthStencilBuffer.Get(),
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            D3D12_RESOURCE_STATE_DEPTH_WRITE);
        mCommandList->ResourceBarrier(1, &depthBackToWrite);
    }

    if (shouldDrawMirrorBreak)
    {
        if (m4xMsaaState)
        {
            D3D12_RESOURCE_BARRIER resolveToSceneBarriers[2] =
            {
                CD3DX12_RESOURCE_BARRIER::Transition(mMSAART.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_RESOLVE_SOURCE),
                CD3DX12_RESOURCE_BARRIER::Transition(mMirrorBreakSceneColor.Get(), mMirrorBreakSceneColorState, D3D12_RESOURCE_STATE_RESOLVE_DEST)
            };
            mCommandList->ResourceBarrier(2, resolveToSceneBarriers);
            mMirrorBreakSceneColorState = D3D12_RESOURCE_STATE_RESOLVE_DEST;

            mCommandList->ResolveSubresource(
                mMirrorBreakSceneColor.Get(),
                0,
                mMSAART.Get(),
                0,
                DXGI_FORMAT_R8G8B8A8_UNORM);

            auto restoreMsaaBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
                mMSAART.Get(),
                D3D12_RESOURCE_STATE_RESOLVE_SOURCE,
                D3D12_RESOURCE_STATE_RENDER_TARGET);
            mCommandList->ResourceBarrier(1, &restoreMsaaBarrier);
        }

        if (mMirrorBreakSceneColorState != D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
        {
            auto sceneSampleBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
                mMirrorBreakSceneColor.Get(),
                mMirrorBreakSceneColorState,
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            mCommandList->ResourceBarrier(1, &sceneSampleBarrier);
            mMirrorBreakSceneColorState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        }

        auto backBufferToRenderTarget = CD3DX12_RESOURCE_BARRIER::Transition(
            CurrentBackBuffer(),
            D3D12_RESOURCE_STATE_PRESENT,
            D3D12_RESOURCE_STATE_RENDER_TARGET);
        mCommandList->ResourceBarrier(1, &backBufferToRenderTarget);

        auto backBufferHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(CurrentBackBufferView());
        mCommandList->ClearRenderTargetView(backBufferHandle, ClearColor, 0, nullptr);
        if (uiUsesBackBufferWithoutDsv)
        {
            mCommandList->OMSetRenderTargets(1, &backBufferHandle, true, nullptr);
        }
        else
        {
            mCommandList->OMSetRenderTargets(1, &backBufferHandle, true, &dsvHandle);
        }

        std::vector<GameObject*> mirrorBreakObjs;
        mirrorBreakObjs.push_back(mMirrorBreakObject.get());
        mRenderer->DrawScene(
            mCommandList.Get(),
            mirrorBreakObjs,
            mCurrFrameResource->PassCB->Resource(),
            mResources->GetSrvHeap(),
            mCurrFrameResource->ObjectCB->Resource(),
            mCurrFrameResource->SkinnedCB->Resource(),
            mCurrFrameResource->MaterialCB->Resource(),
            mRenderer->GetMirrorBreakPSO(),
            2);
    }

    if (mUIManager && (isStage1 || isStage2 || isVillage))
    {
        if (uiUsesBackBufferWithoutDsv)
        {
            auto backBufferHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(CurrentBackBufferView());
            mCommandList->OMSetRenderTargets(1, &backBufferHandle, true, nullptr);
        }
        else
        {
            mCommandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
        }

        std::vector<GameObject*> normalUIObjs;
        bool isFlashActive = false;
        for (auto& obj : mUIManager->GetUIObjects())
        {
            if (obj->Ritem->Mat->Name == "UI_FlashMat" && obj->Ritem->Mat->DiffuseAlbedo.w > 0.0f)
            {
                isFlashActive = true;
                break;
            }
        }

        for (auto& obj : mUIManager->GetUIObjects())
        {
            if (obj->Ritem->Mat->Name == "UI_FlashMat")
            {
                if (isFlashActive) normalUIObjs.push_back(obj.get());
            }
            else if (obj->Ritem->Mat->Name == "UI_ScreenBgMat")
            {
                if (isFlashActive) normalUIObjs.push_back(obj.get());
            }
            else
            {
                if (!isFlashActive)
                {
                    normalUIObjs.push_back(obj.get());
                }
            }
        }

        mRenderer->DrawScene(
            mCommandList.Get(),
            normalUIObjs,
            mCurrFrameResource->PassCB->Resource(),
            mResources->GetSrvHeap(),
            mCurrFrameResource->ObjectCB->Resource(),
            mCurrFrameResource->SkinnedCB->Resource(),
            mCurrFrameResource->MaterialCB->Resource(),
            mRenderer->GetUIPSO(),
            2);
    }

    if (mCurrentScene)
    {
        mCurrentScene->Draw(gt);
    }

    if (shouldDrawPostProcess)
    {
        D3D12_RESOURCE_BARRIER resolveToPostBarriers[2] =
        {
            CD3DX12_RESOURCE_BARRIER::Transition(mMSAART.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_RESOLVE_SOURCE),
            CD3DX12_RESOURCE_BARRIER::Transition(mMirrorBreakSceneColor.Get(), mMirrorBreakSceneColorState, D3D12_RESOURCE_STATE_RESOLVE_DEST)
        };
        mCommandList->ResourceBarrier(2, resolveToPostBarriers);
        mMirrorBreakSceneColorState = D3D12_RESOURCE_STATE_RESOLVE_DEST;

        mCommandList->ResolveSubresource(
            mMirrorBreakSceneColor.Get(),
            0,
            mMSAART.Get(),
            0,
            DXGI_FORMAT_R8G8B8A8_UNORM);

        D3D12_RESOURCE_BARRIER postSampleBarriers[3] =
        {
            CD3DX12_RESOURCE_BARRIER::Transition(mMSAART.Get(), D3D12_RESOURCE_STATE_RESOLVE_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET),
            CD3DX12_RESOURCE_BARRIER::Transition(mMirrorBreakSceneColor.Get(), mMirrorBreakSceneColorState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE),
            CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET)
        };
        mCommandList->ResourceBarrier(3, postSampleBarriers);
        mMirrorBreakSceneColorState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

        auto backBufferHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(CurrentBackBufferView());
        mCommandList->OMSetRenderTargets(1, &backBufferHandle, true, nullptr);

        std::vector<GameObject*> postProcessObjs;
        postProcessObjs.push_back(mPostProcessObject.get());
        mRenderer->DrawScene(
            mCommandList.Get(),
            postProcessObjs,
            mCurrFrameResource->PassCB->Resource(),
            mResources->GetSrvHeap(),
            mCurrFrameResource->ObjectCB->Resource(),
            mCurrFrameResource->SkinnedCB->Resource(),
            mCurrFrameResource->MaterialCB->Resource(),
            mRenderer->GetPostProcessPSO(),
            0);

        postProcessApplied = true;
    }

    if (m4xMsaaState && !shouldDrawMirrorBreak && !shouldDrawVolumetricFog && !postProcessApplied)
    {
        D3D12_RESOURCE_BARRIER barriers[2] =
        {
            CD3DX12_RESOURCE_BARRIER::Transition(mMSAART.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_RESOLVE_SOURCE),
            CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RESOLVE_DEST)
        };
        mCommandList->ResourceBarrier(2, barriers);
        mCommandList->ResolveSubresource(CurrentBackBuffer(), 0, mMSAART.Get(), 0, DXGI_FORMAT_R8G8B8A8_UNORM);
        D3D12_RESOURCE_BARRIER restoreBarriers[2] =
        {
            CD3DX12_RESOURCE_BARRIER::Transition(mMSAART.Get(), D3D12_RESOURCE_STATE_RESOLVE_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET),
            CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(), D3D12_RESOURCE_STATE_RESOLVE_DEST, D3D12_RESOURCE_STATE_PRESENT)
        };
        mCommandList->ResourceBarrier(2, restoreBarriers);
    }
    else
    {
        auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            CurrentBackBuffer(),
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_PRESENT);
        mCommandList->ResourceBarrier(1, &barrier);
    }

    ThrowIfFailed(mCommandList->Close());
    ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);
    ThrowIfFailed(mSwapChain->Present(0, 0));
    mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

    if (mCurrentScene)
    {
        mCurrentScene->DrawOverlay();
    }

    mCurrFrameResource->Fence = ++mCurrentFence;
    mCommandQueue->Signal(mFence.Get(), mCurrentFence);
}

void EclipseWalkerGame::BuildDescriptorHeaps()
{
    mResources->BuildDescriptorHeaps(md3dDevice.Get());
    ID3D12DescriptorHeap* srvHeap = mResources->GetSrvHeap();
    CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv(srvHeap->GetCPUDescriptorHandleForHeapStart()); hCpuSrv.Offset(1000, mCbvSrvUavDescriptorSize);
    CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv(srvHeap->GetGPUDescriptorHandleForHeapStart()); hGpuSrv.Offset(1000, mCbvSrvUavDescriptorSize);
    CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuDsv(mDsvHeap->GetCPUDescriptorHandleForHeapStart()); hCpuDsv.Offset(1, mDsvDescriptorSize);
    if (auto shadowMap = mRenderer->GetShadowMap()) shadowMap->BuildDescriptors(hCpuSrv, hGpuSrv, hCpuDsv);

    if (mDepthStencilBuffer != nullptr)
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC depthSrvDesc = {};
        depthSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        depthSrvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
        const auto depthDesc = mDepthStencilBuffer->GetDesc();
        if (depthDesc.SampleDesc.Count > 1)
        {
            depthSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMS;
        }
        else
        {
            depthSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            depthSrvDesc.Texture2D.MostDetailedMip = 0;
            depthSrvDesc.Texture2D.MipLevels = 1;
            depthSrvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
        }

        CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuDepthSrv(srvHeap->GetCPUDescriptorHandleForHeapStart());
        hCpuDepthSrv.Offset(1001, mCbvSrvUavDescriptorSize);
        md3dDevice->CreateShaderResourceView(mDepthStencilBuffer.Get(), &depthSrvDesc, hCpuDepthSrv);
    }

    for (auto& e : mResources->mMaterials)
    {
        e.second->NumFramesDirty = gNumFrameResources; 
    }
}

void EclipseWalkerGame::BuildFrameResources()
{
    UINT maxObjCount = 2000;
    UINT maxMatCount = 500;
    UINT passCount = 3;
    for (int i = 0; i < gNumFrameResources; ++i)
        mFrameResources.push_back(std::make_unique<FrameResource>(md3dDevice.Get(), passCount, maxObjCount, maxMatCount, maxObjCount));
}

void EclipseWalkerGame::CreateFire(float x, float y, float z, float scale)
{
    RegisterFireAudioEmitter(x, y, z, scale);

    int assignedLightIndex = -1;
    if (mCurrentLightIndex < MaxLights) {
        assignedLightIndex = mCurrentLightIndex;
        mGameLights[assignedLightIndex].InitPoint({ x, y + 1.5f, z }, { 2.6f, 0.62f, 0.15f }, 13.0f);
        mCurrentLightIndex++;
    }
    int numParticles = 6;

    for (int i = 0; i < numParticles; ++i)
    {
        int startFrame = rand() % 4;

        auto fire = std::make_unique<RenderItem>();

        float uOffset = (startFrame % 2) * 0.5f;
        float vOffset = (startFrame / 2) * 0.5f;

        // 크기는 절반(0.5)으로 줄이고, 계산한 위치(Offset)로 텍스처 UV를 이동시킵니다.
        DirectX::XMMATRIX texScale = DirectX::XMMatrixScaling(0.5f, 0.5f, 1.0f);
        DirectX::XMMATRIX texOffset = DirectX::XMMatrixTranslation(uOffset, vOffset, 0.0f);
        DirectX::XMStoreFloat4x4(&fire->TexTransform, DirectX::XMMatrixMultiply(texScale, texOffset));
        // =========================================================

        fire->Geo = mResources->mGeometries["quadGeo"].get();
        fire->Mat = mResources->GetMaterial("Fire_Mat");
        fire->ObjCBIndex = static_cast<UINT>(mAllRitems.size());
        fire->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

        if (fire->Geo && fire->Geo->DrawArgs.count("quad")) {
            fire->IndexCount = fire->Geo->DrawArgs["quad"].IndexCount;
            fire->StartIndexLocation = fire->Geo->DrawArgs["quad"].StartIndexLocation;
            fire->BaseVertexLocation = fire->Geo->DrawArgs["quad"].BaseVertexLocation;
        }

        auto obj = std::make_unique<GameObject>();
        obj->Ritem = fire.get();

        float randomOffsetX = ((static_cast<float>(rand()) / RAND_MAX) - 0.5f) * 0.15f;
        float randomOffsetZ = ((static_cast<float>(rand()) / RAND_MAX) - 0.5f) * 0.15f;

        obj->SetPosition(x + randomOffsetX, y, z + randomOffsetZ);

        obj->mIsAnimated = true;
        obj->mCurrFrame = startFrame;
        obj->mFrameDuration = 9999.0f; 
        obj->mNumCols = 2;
        obj->mNumRows = 2;
        obj->mIsBillboard = true;

        // 파티클 팽창 애니메이션 설정
        obj->mIsParticle = true;
        obj->mLifeTime = 0.6f;
        obj->mBaseScale = scale;
        obj->mBasePosY = y - 0.1f;

        obj->mAge = (static_cast<float>(rand()) / RAND_MAX) * 0.6f;

        if (i == 0) obj->mLightIndex = assignedLightIndex;
        else obj->mLightIndex = -1;

        obj->Update();
        mAllRitems.push_back(std::move(fire));
        mGameObjects.push_back(std::move(obj));
    }
}

void EclipseWalkerGame::RegisterFireAudioEmitter(float x, float y, float z, float scale)
{
    FireAudioEmitter emitter;
    emitter.Position = { x, y, z };
    emitter.InnerRadius = (std::max)(1.25f, scale * 4.0f);
    emitter.OuterRadius = (std::max)(4.0f, 1.5f + scale * 10.0f);
    emitter.MaxVolume = (std::min)(0.16f, 0.08f + scale * 0.08f);
    mFireAudioEmitters.push_back(emitter);
}

void EclipseWalkerGame::ClearFireAudioEmitters()
{
    mFireAudioEmitters.clear();
    if (mFireLoopHandle != AudioManager::InvalidClipHandle)
    {
        AudioManager::Get().StopEffect(mFireLoopHandle);
        mFireLoopHandle = AudioManager::InvalidClipHandle;
    }
}

void EclipseWalkerGame::RegisterLavaAudioEmitter(
    float x,
    float y,
    float z,
    float innerRadius,
    float outerRadius,
    float maxVolume)
{
    LavaAudioEmitter emitter;
    emitter.Position = { x, y, z };
    emitter.InnerRadius = (std::max)(0.5f, innerRadius);
    emitter.OuterRadius = (std::max)(emitter.InnerRadius + 0.5f, outerRadius);
    emitter.MaxVolume = (std::clamp)(maxVolume, 0.0f, 1.0f);
    mLavaAudioEmitters.push_back(emitter);
}

void EclipseWalkerGame::ClearLavaAudioEmitters()
{
    mLavaAudioEmitters.clear();

    if (mLavaLoopHandlePrimary != AudioManager::InvalidClipHandle)
    {
        AudioManager::Get().StopEffect(mLavaLoopHandlePrimary);
        mLavaLoopHandlePrimary = AudioManager::InvalidClipHandle;
    }

    if (mLavaLoopHandleSecondary != AudioManager::InvalidClipHandle)
    {
        AudioManager::Get().StopEffect(mLavaLoopHandleSecondary);
        mLavaLoopHandleSecondary = AudioManager::InvalidClipHandle;
    }
}

void EclipseWalkerGame::UpdateFireAmbientAudio()
{
    if (mPlayer == nullptr || mFireAudioEmitters.empty())
    {
        if (mFireLoopHandle != AudioManager::InvalidClipHandle)
        {
            AudioManager::Get().StopEffect(mFireLoopHandle);
            mFireLoopHandle = AudioManager::InvalidClipHandle;
        }
        return;
    }

    const DirectX::XMFLOAT3 playerPos = mPlayer->GetPosition();
    float nearestVolume = 0.0f;

    for (const FireAudioEmitter& emitter : mFireAudioEmitters)
    {
        const float dx = playerPos.x - emitter.Position.x;
        const float dz = playerPos.z - emitter.Position.z;
        const float distance = std::sqrt(dx * dx + dz * dz);

        float volume = 0.0f;
        if (distance <= emitter.InnerRadius)
        {
            volume = emitter.MaxVolume;
        }
        else if (distance < emitter.OuterRadius)
        {
            const float ratio = 1.0f - ((distance - emitter.InnerRadius) / (emitter.OuterRadius - emitter.InnerRadius));
            volume = emitter.MaxVolume * (std::clamp)(ratio, 0.0f, 1.0f);
        }

        nearestVolume = (std::max)(nearestVolume, volume);
    }

    if (nearestVolume <= 0.001f)
    {
        if (mFireLoopHandle != AudioManager::InvalidClipHandle)
        {
            AudioManager::Get().StopEffect(mFireLoopHandle);
            mFireLoopHandle = AudioManager::InvalidClipHandle;
        }
        return;
    }

    if (mFireLoopHandle == AudioManager::InvalidClipHandle)
    {
        mFireLoopHandle = AudioManager::Get().PlayLoop(kFireAmbientPath, nearestVolume);
        return;
    }

    AudioManager::Get().SetVolume(mFireLoopHandle, nearestVolume);
}

void EclipseWalkerGame::UpdateLavaAmbientAudio()
{
    if (mPlayer == nullptr || mLavaAudioEmitters.empty())
    {
        if (mLavaLoopHandlePrimary != AudioManager::InvalidClipHandle)
        {
            AudioManager::Get().StopEffect(mLavaLoopHandlePrimary);
            mLavaLoopHandlePrimary = AudioManager::InvalidClipHandle;
        }
        if (mLavaLoopHandleSecondary != AudioManager::InvalidClipHandle)
        {
            AudioManager::Get().StopEffect(mLavaLoopHandleSecondary);
            mLavaLoopHandleSecondary = AudioManager::InvalidClipHandle;
        }
        return;
    }

    const DirectX::XMFLOAT3 playerPos = mPlayer->GetPosition();
    float nearestVolume = 0.0f;

    for (const LavaAudioEmitter& emitter : mLavaAudioEmitters)
    {
        const float dx = playerPos.x - emitter.Position.x;
        const float dz = playerPos.z - emitter.Position.z;
        const float distance = std::sqrt(dx * dx + dz * dz);

        float volume = 0.0f;
        if (distance <= emitter.InnerRadius)
        {
            volume = emitter.MaxVolume;
        }
        else if (distance < emitter.OuterRadius)
        {
            const float ratio = 1.0f - ((distance - emitter.InnerRadius) / (emitter.OuterRadius - emitter.InnerRadius));
            volume = emitter.MaxVolume * (std::clamp)(ratio, 0.0f, 1.0f);
        }

        nearestVolume = (std::max)(nearestVolume, volume);
    }

    if (nearestVolume <= 0.001f)
    {
        if (mLavaLoopHandlePrimary != AudioManager::InvalidClipHandle)
        {
            AudioManager::Get().StopEffect(mLavaLoopHandlePrimary);
            mLavaLoopHandlePrimary = AudioManager::InvalidClipHandle;
        }
        if (mLavaLoopHandleSecondary != AudioManager::InvalidClipHandle)
        {
            AudioManager::Get().StopEffect(mLavaLoopHandleSecondary);
            mLavaLoopHandleSecondary = AudioManager::InvalidClipHandle;
        }
        return;
    }

    const float boostedVolume = (std::clamp)(nearestVolume * kLavaAmbientVolumeGain, 0.0f, 1.0f);
    if (mLavaLoopHandlePrimary == AudioManager::InvalidClipHandle)
    {
        mLavaLoopHandlePrimary = AudioManager::Get().PlayLoop(kLavaAmbientPrimaryPath, boostedVolume);
    }
    else
    {
        AudioManager::Get().SetVolume(mLavaLoopHandlePrimary, boostedVolume);
    }

    const float secondaryVolume = boostedVolume * kLavaAmbientSecondaryMix;
    if (mLavaLoopHandleSecondary == AudioManager::InvalidClipHandle)
    {
        mLavaLoopHandleSecondary = AudioManager::Get().PlayLoop(kLavaAmbientSecondaryPath, secondaryVolume);
    }
    else
    {
        AudioManager::Get().SetVolume(mLavaLoopHandleSecondary, secondaryVolume);
    }
}

void EclipseWalkerGame::UpdateSceneAudio()
{
    if (dynamic_cast<CharSelectScene*>(mCurrentScene.get()) != nullptr)
    {
        PlaySceneBgm(kCharacterSelectBgmPath, kCharacterSelectBgmVolume);
        return;
    }

    if (dynamic_cast<LoginScene*>(mCurrentScene.get()) != nullptr ||
        dynamic_cast<RoomSelectScene*>(mCurrentScene.get()) != nullptr ||
        dynamic_cast<MainMenuScene*>(mCurrentScene.get()) != nullptr)
    {
        PlaySceneBgm(kTitleBgmPath, kTitleBgmVolume);
        return;
    }

    StopSceneBgm();
}

void EclipseWalkerGame::PlaySceneBgm(const std::wstring& relativePath, float volumeScale)
{
    if (relativePath.empty())
    {
        StopSceneBgm();
        return;
    }

    if (mBgmHandle != AudioManager::InvalidClipHandle && mCurrentBgmPath == relativePath)
    {
        AudioManager::Get().SetVolume(mBgmHandle, volumeScale);
        return;
    }

    StopSceneBgm();
    mBgmHandle = AudioManager::Get().PlayLoop(relativePath, volumeScale);
    mCurrentBgmPath = (mBgmHandle != AudioManager::InvalidClipHandle) ? relativePath : L"";
}

void EclipseWalkerGame::StopSceneBgm()
{
    if (mBgmHandle != AudioManager::InvalidClipHandle)
    {
        AudioManager::Get().StopEffect(mBgmHandle);
        mBgmHandle = AudioManager::InvalidClipHandle;
    }

    mCurrentBgmPath.clear();
}

void EclipseWalkerGame::BuildPlayer()
{
    auto playerRitem = std::make_unique<RenderItem>();
    playerRitem->World = MathHelper::Identity4x4();
    playerRitem->TexTransform = MathHelper::Identity4x4();
    playerRitem->ObjCBIndex = static_cast<UINT>(mAllRitems.size());
    playerRitem->NumFramesDirty = gNumFrameResources;

    auto playerObj = std::make_unique<GameObject>();
    playerObj->Ritem = playerRitem.get();

    mPlayerObject = playerObj.get();
    mAllRitems.push_back(std::move(playerRitem));
    mGameObjects.push_back(std::move(playerObj));
}

void EclipseWalkerGame::BuildPlayerSkinOverlays(
    PlayerClass playerClass,
    GameObject* parentObject,
    RenderItem* parentRitem,
    std::vector<RenderItem*>& outOverlayRitems)
{
    outOverlayRitems.clear();

    if (parentObject == nullptr ||
        parentRitem == nullptr ||
        parentRitem->Geo == nullptr ||
        !parentRitem->IsSkinned)
    {
        return;
    }

    Material* skinMaterial = EnsurePlayerSkinMaterial(mResources.get(), playerClass);
    Material* hairMaterial = EnsurePlayerHairMaterial(mResources.get());
    if (skinMaterial == nullptr && hairMaterial == nullptr)
    {
        return;
    }

    struct OverlaySubset
    {
        const std::string* Name = nullptr;
        const SubmeshGeometry* Submesh = nullptr;
    };

    std::vector<OverlaySubset> overlaySubsets;
    for (const auto& drawArgPair : parentRitem->Geo->DrawArgs)
    {
        const std::string& subsetName = drawArgPair.first;
        if (subsetName == "skinnedMesh")
        {
            continue;
        }

        overlaySubsets.push_back({ &drawArgPair.first, &drawArgPair.second });
    }

    std::stable_sort(
        overlaySubsets.begin(),
        overlaySubsets.end(),
        [](const OverlaySubset& lhs, const OverlaySubset& rhs)
        {
            auto priority = [](const std::string& name)
            {
                if (ContainsAsciiInsensitive(name, "hair"))
                {
                    return 2;
                }

                if (ContainsAsciiInsensitive(name, "face"))
                {
                    return 1;
                }

                return 0;
            };

            return priority(*lhs.Name) < priority(*rhs.Name);
        });

    bool createdSubsetRenderItems = false;
    for (const auto& overlaySubset : overlaySubsets)
    {
        const std::string& subsetName = *overlaySubset.Name;
        const SubmeshGeometry& submesh = *overlaySubset.Submesh;
        const bool isFaceSubset = ContainsAsciiInsensitive(subsetName, "face");
        const bool isHairSubset = ContainsAsciiInsensitive(subsetName, "hair");
        const bool isSkinSubset =
            isFaceSubset ||
            ContainsAsciiInsensitive(subsetName, "skin") ||
            ContainsAsciiInsensitive(submesh.MaterialName, "skin");

        Material* material = parentRitem->Mat;
        if (isSkinSubset)
        {
            material = skinMaterial;
        }
        else if (isHairSubset)
        {
            material = hairMaterial;
        }

        if (material == nullptr)
        {
            continue;
        }

        auto overlayRitem = std::make_unique<RenderItem>();
        overlayRitem->World = parentRitem->World;
        overlayRitem->TexTransform = MathHelper::Identity4x4();
        overlayRitem->ObjCBIndex = static_cast<UINT>(mAllRitems.size());
        overlayRitem->NumFramesDirty = gNumFrameResources;
        overlayRitem->Geo = parentRitem->Geo;
        overlayRitem->Mat = material;
        overlayRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        overlayRitem->IndexCount = submesh.IndexCount;
        overlayRitem->StartIndexLocation = submesh.StartIndexLocation;
        overlayRitem->BaseVertexLocation = submesh.BaseVertexLocation;
        overlayRitem->IsSkinned = true;
        overlayRitem->SkinnedCBIndex = parentRitem->SkinnedCBIndex;
        overlayRitem->Visible = true;

        outOverlayRitems.push_back(overlayRitem.get());
        auto overlayObject = std::make_unique<GameObject>();
        overlayObject->Ritem = overlayRitem.get();
        overlayObject->SetWorldTransform(DirectX::XMLoadFloat4x4(&parentRitem->World));

        mAllRitems.push_back(std::move(overlayRitem));
        mGameObjects.push_back(std::move(overlayObject));
        createdSubsetRenderItems = true;
    }

    if (createdSubsetRenderItems)
    {
        parentRitem->Visible = false;
        parentRitem->NumFramesDirty = gNumFrameResources;
    }
}

void EclipseWalkerGame::SyncPlayerSkinOverlays()
{
    auto syncOverlays = [](GameObject* parentObject, const std::vector<RenderItem*>& overlayRitems)
    {
        if (parentObject == nullptr || parentObject->Ritem == nullptr)
        {
            for (auto* overlayRitem : overlayRitems)
            {
                if (overlayRitem != nullptr)
                {
                    overlayRitem->Visible = false;
                    overlayRitem->NumFramesDirty = gNumFrameResources;
                }
            }
            return;
        }

        for (auto* overlayRitem : overlayRitems)
        {
            if (overlayRitem == nullptr)
            {
                continue;
            }

            overlayRitem->World = parentObject->Ritem->World;
            overlayRitem->Visible = true;
            overlayRitem->SkinnedCBIndex = parentObject->Ritem->SkinnedCBIndex;
            overlayRitem->NumFramesDirty = gNumFrameResources;
        }
    };

    syncOverlays(mPlayerObject, mPlayerSkinOverlayRitems);
    for (auto& pair : mRemotePlayerSkinOverlayRitems)
    {
        auto playerIt = mRemotePlayerObjects.find(pair.first);
        syncOverlays(playerIt != mRemotePlayerObjects.end() ? playerIt->second : nullptr, pair.second);
    }
}

void EclipseWalkerGame::BuildPlayerEquipment(
    GameObject* parentObject,
    PlayerClass playerClass,
    ClassTier playerTier,
    GameObject*& outWeaponObject,
    GameObject*& outShieldObject,
    bool ignoreParentVisibility)
{
    outWeaponObject = nullptr;
    outShieldObject = nullptr;

    if (parentObject == nullptr)
    {
        return;
    }

    EquipmentAttachmentSpec weaponSpec;
    EquipmentAttachmentSpec shieldSpec;
    ConfigureEquipmentSpecs(playerClass, playerTier, weaponSpec, shieldSpec);

    auto buildAttachedItem = [this, parentObject, ignoreParentVisibility](
        GameObject*& outObject,
        const EquipmentAttachmentSpec& attachmentSpec)
    {
        if (!attachmentSpec.Enabled || !std::filesystem::exists(attachmentSpec.ModelPath))
        {
            if (attachmentSpec.Enabled)
            {
                std::string log = "[Weapon] Missing model: " + attachmentSpec.ModelPath + "\n";
                OutputDebugStringA(log.c_str());
            }
            return;
        }

        if (mResources->mGeometries.find(attachmentSpec.GeometryName) == mResources->mGeometries.end())
        {
            auto geometry = BuildStaticModelGeometry(
                md3dDevice.Get(),
                mCommandList.Get(),
                attachmentSpec.GeometryName,
                attachmentSpec.ModelPath,
                attachmentSpec.TargetMaxDimension,
                attachmentSpec.PivotBias);
            if (geometry == nullptr)
            {
                return;
            }

            mResources->mGeometries[geometry->Name] = std::move(geometry);
        }

        auto geoIt = mResources->mGeometries.find(attachmentSpec.GeometryName);
        Material* material = mResources->GetMaterial(attachmentSpec.MaterialName);
        if (geoIt == mResources->mGeometries.end() || material == nullptr)
        {
            std::string log = "[Weapon] Missing geometry or material. geo=" +
                attachmentSpec.GeometryName + ", mat=" + attachmentSpec.MaterialName + "\n";
            OutputDebugStringA(log.c_str());
            return;
        }

        auto* geometry = geoIt->second.get();
        auto submeshIt = geometry->DrawArgs.find("mesh");
        if (submeshIt == geometry->DrawArgs.end())
        {
            std::string log = "[Weapon] Missing draw arg 'mesh': " + attachmentSpec.GeometryName + "\n";
            OutputDebugStringA(log.c_str());
            return;
        }

        auto item = std::make_unique<RenderItem>();
        item->World = MathHelper::Identity4x4();
        item->TexTransform = MathHelper::Identity4x4();
        item->ObjCBIndex = static_cast<UINT>(mAllRitems.size());
        item->Geo = geometry;
        item->Mat = material;
        item->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        item->IndexCount = submeshIt->second.IndexCount;
        item->StartIndexLocation = submeshIt->second.StartIndexLocation;
        item->BaseVertexLocation = submeshIt->second.BaseVertexLocation;
        item->Visible = false;
        item->NumFramesDirty = gNumFrameResources;
        const UINT attachedIndexCount = item->IndexCount;

        auto object = std::make_unique<GameObject>();
        object->Ritem = item.get();
        object->Update();

        outObject = object.get();
        mAllRitems.push_back(std::move(item));
        mGameObjects.push_back(std::move(object));

        SocketAttachmentDesc socketDesc;
        socketDesc.ParentObject = parentObject;
        socketDesc.ChildObject = outObject;
        socketDesc.SocketName = attachmentSpec.SocketName;
        socketDesc.LocalPosition = attachmentSpec.LocalPosition;
        socketDesc.LocalRotation = attachmentSpec.LocalRotation;
        socketDesc.LocalScale = attachmentSpec.LocalScale;
        socketDesc.IgnoreParentVisibility = ignoreParentVisibility;
        mSocketAttachmentSystem.Attach(socketDesc);

        std::ostringstream log;
        log << "[Weapon] Attached "
            << attachmentSpec.ModelPath
            << " geo=" << attachmentSpec.GeometryName
            << " mat=" << attachmentSpec.MaterialName
            << " socket=" << attachmentSpec.SocketName
            << " indices=" << attachedIndexCount << "\n";
        OutputDebugStringA(log.str().c_str());
    };

    if (weaponSpec.Enabled)
    {
        if (parentObject == mPlayerObject)
        {
            mDebugWeaponSocketName = weaponSpec.SocketName;
            mDebugWeaponSocketPosition = weaponSpec.LocalPosition;
            mDebugWeaponSocketRotation = weaponSpec.LocalRotation;
            mDebugWeaponSocketScale = weaponSpec.LocalScale;
        }
        buildAttachedItem(outWeaponObject, weaponSpec);
    }

    if (shieldSpec.Enabled)
    {
        buildAttachedItem(outShieldObject, shieldSpec);
    }
}

void EclipseWalkerGame::ClearSocketAttachments()
{
    mSocketAttachmentSystem.Clear();
}

void EclipseWalkerGame::BuildPlayerWeapon()
{
    if (mPlayerObject == nullptr)
    {
        return;
    }

    ClearLocalPlayerEquipment();

    BuildPlayerEquipment(
        mPlayerObject,
        mSelectedPlayerClass,
        mSelectedPlayerTier,
        mPlayerWeaponObject,
        mPlayerShieldObject);
}

void EclipseWalkerGame::ResetRuntimeSceneObjectRefs()
{
    ClearSocketAttachments();
    ClearFireAudioEmitters();
    ClearLavaAudioEmitters();

    mPlayerObject = nullptr;
    mPlayerWeaponObject = nullptr;
    mPlayerShieldObject = nullptr;
    mPlayerSkinOverlayRitems.clear();
    mPlayer.reset();

    mRemotePlayerObjects.clear();
    mRemotePlayerWeaponObjects.clear();
    mRemotePlayerShieldObjects.clear();
    mRemotePlayerSkinOverlayRitems.clear();
    mRemotePlayerMotionStates.clear();
    mRemotePlayerVisualClasses.clear();
    mRemotePlayerVisualTiers.clear();
    mRemotePlayerAnimationStates.clear();
    mRemotePlayerAttackEndTicks.clear();
}

void EclipseWalkerGame::HideOverlayRenderItems(std::vector<RenderItem*>& overlayRitems)
{
    for (auto* overlayRitem : overlayRitems)
    {
        if (overlayRitem != nullptr)
        {
            overlayRitem->Visible = false;
            overlayRitem->NumFramesDirty = gNumFrameResources;
        }
    }

    overlayRitems.clear();
}

void EclipseWalkerGame::ClearLocalPlayerEquipment()
{
    auto detachObject = [this](GameObject*& object)
    {
        if (object == nullptr)
        {
            return;
        }

        mSocketAttachmentSystem.Detach(object);
        if (object->Ritem != nullptr)
        {
            object->Ritem->Visible = false;
            object->Ritem->NumFramesDirty = gNumFrameResources;
        }

        object = nullptr;
    };

    detachObject(mPlayerWeaponObject);
    detachObject(mPlayerShieldObject);
}

void EclipseWalkerGame::HideRemotePlayer(int playerId)
{
    auto hideObject = [this](GameObject* object)
    {
        if (object == nullptr)
        {
            return;
        }

        mSocketAttachmentSystem.Detach(object);
        if (object->Ritem != nullptr)
        {
            object->Ritem->Visible = false;
        }
    };

    auto weaponIt = mRemotePlayerWeaponObjects.find(playerId);
    if (weaponIt != mRemotePlayerWeaponObjects.end())
    {
        hideObject(weaponIt->second);
        mRemotePlayerWeaponObjects.erase(weaponIt);
    }

    auto shieldIt = mRemotePlayerShieldObjects.find(playerId);
    if (shieldIt != mRemotePlayerShieldObjects.end())
    {
        hideObject(shieldIt->second);
        mRemotePlayerShieldObjects.erase(shieldIt);
    }

    auto overlayIt = mRemotePlayerSkinOverlayRitems.find(playerId);
    if (overlayIt != mRemotePlayerSkinOverlayRitems.end())
    {
        for (auto* overlayRitem : overlayIt->second)
        {
            if (overlayRitem != nullptr)
            {
                overlayRitem->Visible = false;
                overlayRitem->NumFramesDirty = gNumFrameResources;
            }
        }
        mRemotePlayerSkinOverlayRitems.erase(overlayIt);
    }

    auto playerIt = mRemotePlayerObjects.find(playerId);
    if (playerIt != mRemotePlayerObjects.end())
    {
        hideObject(playerIt->second);
        mRemotePlayerObjects.erase(playerIt);
    }

    mRemotePlayerAnimationStates.erase(playerId);
    mRemotePlayerAttackEndTicks.erase(playerId);
    mRemotePlayerDeadStates.erase(playerId);
    mRemotePlayerRespawnEndTicks.erase(playerId);
    mRemotePlayerMotionStates.erase(playerId);
    mRemotePlayerVisualClasses.erase(playerId);
    mRemotePlayerVisualTiers.erase(playerId);
}

GameObject* EclipseWalkerGame::EnsureRemotePlayerObject(
    int playerId,
    int classType,
    int playerLevel,
    const DirectX::XMFLOAT3& spawnPosition,
    float rotY)
{
    if (playerId <= 0 || !IsValidNetworkPlayerClass(classType) || !IsValidNetworkPlayerLevel(playerLevel))
    {
        return nullptr;
    }

    auto visualClassIt = mRemotePlayerVisualClasses.find(playerId);
    auto visualTierIt = mRemotePlayerVisualTiers.find(playerId);
    if (mRemotePlayerObjects.find(playerId) != mRemotePlayerObjects.end() &&
        (visualClassIt == mRemotePlayerVisualClasses.end() || visualClassIt->second != classType ||
            visualTierIt == mRemotePlayerVisualTiers.end() || visualTierIt->second != playerLevel))
    {
        HideRemotePlayer(playerId);
    }

    auto existingIt = mRemotePlayerObjects.find(playerId);
    if (existingIt != mRemotePlayerObjects.end())
    {
        return existingIt->second;
    }

    OutputDebugStringA("[Client] 새 원격 플레이어 등장 3D 오브젝트 생성\n");

    auto ritem = std::make_unique<RenderItem>();
    ritem->TexTransform = MathHelper::Identity4x4();
    ritem->ObjCBIndex = static_cast<UINT>(mAllRitems.size());
    ritem->NumFramesDirty = 3;

    auto newPlayerObj = std::make_unique<GameObject>();
    const PlayerClass remotePlayerClass = DecodeNetworkPlayerClass(classType);
    const ClassTier remotePlayerTier = DecodeNetworkPlayerTier(playerLevel);
    const CharacterVisualSpec visualSpec = BuildPlayerVisualSpec(remotePlayerClass, remotePlayerTier, spawnPosition);
    const size_t textureCountBefore = mResources->mTextures.size();
    const size_t materialCountBefore = mResources->mMaterials.size();
    if (!CharacterVisualFactory::ApplyVisual(
        newPlayerObj.get(),
        ritem.get(),
        md3dDevice.Get(),
        mCommandList.Get(),
        mResources.get(),
        visualSpec))
    {
        OutputDebugStringA("[Client] Remote player visual fell back to box\n");
    }

    newPlayerObj->SetRotation(0.0f, rotY, 0.0f);
    newPlayerObj->Update();

    mRemotePlayerObjects[playerId] = newPlayerObj.get();
    mRemotePlayerVisualClasses[playerId] = classType;
    mRemotePlayerVisualTiers[playerId] = playerLevel;
    mRemotePlayerAnimationStates[playerId] = -1;
    mAllRitems.push_back(std::move(ritem));
    mGameObjects.push_back(std::move(newPlayerObj));

    BuildPlayerSkinOverlays(
        remotePlayerClass,
        mRemotePlayerObjects[playerId],
        mRemotePlayerObjects[playerId] ? mRemotePlayerObjects[playerId]->Ritem : nullptr,
        mRemotePlayerSkinOverlayRitems[playerId]);

    GameObject* remoteWeaponObject = nullptr;
    GameObject* remoteShieldObject = nullptr;
    BuildPlayerEquipment(
        mRemotePlayerObjects[playerId],
        remotePlayerClass,
        remotePlayerTier,
        remoteWeaponObject,
        remoteShieldObject);
    if (remoteWeaponObject != nullptr)
    {
        mRemotePlayerWeaponObjects[playerId] = remoteWeaponObject;
    }
    if (remoteShieldObject != nullptr)
    {
        mRemotePlayerShieldObjects[playerId] = remoteShieldObject;
    }

    if (mResources->mTextures.size() != textureCountBefore ||
        mResources->mMaterials.size() != materialCountBefore)
    {
        BuildDescriptorHeaps();
    }

    return mRemotePlayerObjects[playerId];
}

void EclipseWalkerGame::UpdateWeaponSocketDebug(const GameTimer& gt)
{
    if (mPlayerObject == nullptr || mPlayerWeaponObject == nullptr)
    {
        return;
    }

    auto IsDown = [](int key)
    {
        return (GetAsyncKeyState(key) & 0x8000) != 0;
    };

    const float dt = (std::min)(gt.DeltaTime(), 0.05f);
    const float moveStep = 0.45f * dt;
    const float rotationStep = DirectX::XM_PIDIV2 * dt;
    bool changed = false;

    if (IsDown('J')) { mDebugWeaponSocketPosition.x -= moveStep; changed = true; }
    if (IsDown('L')) { mDebugWeaponSocketPosition.x += moveStep; changed = true; }
    if (IsDown('O')) { mDebugWeaponSocketPosition.y -= moveStep; changed = true; }
    if (IsDown('U')) { mDebugWeaponSocketPosition.y += moveStep; changed = true; }
    if (IsDown('K')) { mDebugWeaponSocketPosition.z -= moveStep; changed = true; }
    if (IsDown('I')) { mDebugWeaponSocketPosition.z += moveStep; changed = true; }

    if (IsDown(VK_NUMPAD4)) { mDebugWeaponSocketRotation.x -= rotationStep; changed = true; }
    if (IsDown(VK_NUMPAD6)) { mDebugWeaponSocketRotation.x += rotationStep; changed = true; }
    if (IsDown(VK_NUMPAD2)) { mDebugWeaponSocketRotation.y -= rotationStep; changed = true; }
    if (IsDown(VK_NUMPAD8)) { mDebugWeaponSocketRotation.y += rotationStep; changed = true; }
    if (IsDown(VK_NUMPAD7)) { mDebugWeaponSocketRotation.z -= rotationStep; changed = true; }
    if (IsDown(VK_NUMPAD9)) { mDebugWeaponSocketRotation.z += rotationStep; changed = true; }

    const bool printDown = IsDown(VK_F8);
    if (printDown && !mWeaponSocketDebugPrintWasDown)
    {
        LogWeaponSocketDebug();
    }
    mWeaponSocketDebugPrintWasDown = printDown;

    if (!changed)
    {
        return;
    }

    ApplyWeaponSocketDebug();
    mWeaponSocketDebugLogTimer -= dt;
    if (mWeaponSocketDebugLogTimer <= 0.0f)
    {
        LogWeaponSocketDebug();
        mWeaponSocketDebugLogTimer = 0.18f;
    }
}

void EclipseWalkerGame::ApplyWeaponSocketDebug()
{
    if (mPlayerObject == nullptr || mPlayerWeaponObject == nullptr)
    {
        return;
    }

    SocketAttachmentDesc socketDesc;
    socketDesc.ParentObject = mPlayerObject;
    socketDesc.ChildObject = mPlayerWeaponObject;
    socketDesc.SocketName = mDebugWeaponSocketName;
    socketDesc.LocalPosition = mDebugWeaponSocketPosition;
    socketDesc.LocalRotation = mDebugWeaponSocketRotation;
    socketDesc.LocalScale = mDebugWeaponSocketScale;
    socketDesc.IgnoreParentVisibility = true;
    mSocketAttachmentSystem.Attach(socketDesc);
}

void EclipseWalkerGame::LogWeaponSocketDebug() const
{
    constexpr float kRadToDeg = 57.2957795f;

    std::ostringstream log;
    log << std::fixed << std::setprecision(4)
        << "[WeaponSocketDebug]\n"
        << "Socket: " << mDebugWeaponSocketName << "\n"
        << "Position: { "
        << mDebugWeaponSocketPosition.x << "f, "
        << mDebugWeaponSocketPosition.y << "f, "
        << mDebugWeaponSocketPosition.z << "f }\n"
        << "Rotation: { "
        << mDebugWeaponSocketRotation.x << "f, "
        << mDebugWeaponSocketRotation.y << "f, "
        << mDebugWeaponSocketRotation.z << "f }\n"
        << "RotationDeg: { "
        << mDebugWeaponSocketRotation.x * kRadToDeg << ", "
        << mDebugWeaponSocketRotation.y * kRadToDeg << ", "
        << mDebugWeaponSocketRotation.z * kRadToDeg << " }\n"
        << "Code:\n"
        << "    { "
        << mDebugWeaponSocketPosition.x << "f, "
        << mDebugWeaponSocketPosition.y << "f, "
        << mDebugWeaponSocketPosition.z << "f },\n"
        << "    { "
        << mDebugWeaponSocketRotation.x << "f, "
        << mDebugWeaponSocketRotation.y << "f, "
        << mDebugWeaponSocketRotation.z << "f },\n";

    OutputDebugStringA(log.str().c_str());
}

void EclipseWalkerGame::UpdateObjectCBs(const GameTimer& gt)
{
    auto currObjectCB = mCurrFrameResource->ObjectCB.get();
    for (auto& e : mAllRitems)
    {
        if (e->NumFramesDirty > 0)
        {
            XMMATRIX world = XMLoadFloat4x4(&e->World);
            ObjectConstants objConstants;
            DirectX::XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));
            XMMATRIX texTransform = XMLoadFloat4x4(&e->TexTransform);
            DirectX::XMStoreFloat4x4(&objConstants.TexTransform, XMMatrixTranspose(texTransform));
            objConstants.ColorMultiplier = e->ColorMultiplier;
            currObjectCB->CopyData(e->ObjCBIndex, objConstants);
            e->NumFramesDirty--;
        }
    }
}

void EclipseWalkerGame::UpdateSkinnedCBs(const GameTimer& gt)
{
    UNREFERENCED_PARAMETER(gt);

    auto currSkinnedCB = mCurrFrameResource->SkinnedCB.get();
    for (auto& obj : mGameObjects)
    {
        if (obj == nullptr || obj->Ritem == nullptr || !obj->Ritem->IsSkinned)
        {
            continue;
        }

        auto* skeletalAnimation = obj->GetSkeletalAnimation();
        if (skeletalAnimation == nullptr)
        {
            continue;
        }

        SkinnedConstants skinnedConstants = {};
        for (int i = 0; i < MaxBones; ++i)
        {
            DirectX::XMStoreFloat4x4(&skinnedConstants.BoneTransforms[i], DirectX::XMMatrixTranspose(DirectX::XMMatrixIdentity()));
        }

        const auto& finalMatrices = skeletalAnimation->GetFinalBoneMatrices();
        const size_t matrixCount = std::min<size_t>(finalMatrices.size(), MaxBones);
        for (size_t i = 0; i < matrixCount; ++i)
        {
            DirectX::XMMATRIX finalMatrix = DirectX::XMLoadFloat4x4(&finalMatrices[i]);
            DirectX::XMStoreFloat4x4(&skinnedConstants.BoneTransforms[i], DirectX::XMMatrixTranspose(finalMatrix));
        }

        currSkinnedCB->CopyData(obj->Ritem->SkinnedCBIndex, skinnedConstants);
    }
}

void EclipseWalkerGame::UpdateMaterialCBs(const GameTimer& gt)
{
    auto currMaterialCB = mCurrFrameResource->MaterialCB.get();
    for (auto& e : mResources->mMaterials)
    {
        Material* mat = e.second.get();
        if (mat->NumFramesDirty > 0)
        {
            MaterialConstants matConstants;
            matConstants.DiffuseAlbedo = mat->DiffuseAlbedo; matConstants.FresnelR0 = mat->FresnelR0; matConstants.Roughness = mat->Roughness;
            matConstants.OutlineColor = mat->OutlineColor; matConstants.OutlineThickness = mat->OutlineThickness;
            matConstants.IsToon = mat->IsToon; matConstants.IsTransparent = mat->IsTransparent;
            matConstants.DiffuseMapIndex = mResources->GetTextureIndex(mat->DiffuseMapName);
            matConstants.NormalMapIndex = mResources->GetTextureIndex(mat->NormalMapName);
            matConstants.EmissiveMapIndex = mResources->GetTextureIndex(mat->EmissiveMapName);
            matConstants.MetallicMapIndex = mResources->GetTextureIndex(mat->MetallicMapName);
            matConstants.MetallicFactor = mat->MetallicFactor;
            currMaterialCB->CopyData(mat->MatCBIndex, matConstants);
            mat->NumFramesDirty--;
        }
    }
}

void EclipseWalkerGame::InitLights()
{
    mGameLights.resize(MaxLights);
    mGameLights[0].InitDirectional({ 0.3f, -1.0f, 0.3f }, { 0.8f, 0.8f, 0.8f });
    mCurrentLightIndex = 1;
}

void EclipseWalkerGame::ApplyCharacterSelectLighting(const DirectX::XMFLOAT3& focusPosition)
{
    mGameLights.resize(MaxLights);
    for (auto& light : mGameLights)
    {
        light.InitPoint({ 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, 1.0f);
    }

    mGameLights[0].InitDirectional({ -0.18f, -0.48f, 0.72f }, { 0.42f, 0.42f, 0.46f });
    mGameLights[1].InitPoint(
        { focusPosition.x, focusPosition.y + 0.10f, focusPosition.z - 2.85f },
        { 1.55f, 1.38f, 1.18f },
        6.5f);
    mGameLights[2].InitPoint(
        { focusPosition.x - 1.35f, focusPosition.y + 0.45f, focusPosition.z - 1.65f },
        { 0.42f, 0.50f, 0.70f },
        5.0f);
    mCurrentLightIndex = 3;
}

float EclipseWalkerGame::AspectRatio() const { return static_cast<float>(mClientWidth) / mClientHeight; }

void EclipseWalkerGame::UpdateMainPassCB(const GameTimer& gt)
{
    XMMATRIX view = mCamera.GetView(); XMMATRIX proj = mCamera.GetProj(); XMMATRIX viewProj = XMMatrixMultiply(view, proj);
    XMMATRIX invView = MathHelper::Inverse(view); XMMATRIX invProj = MathHelper::Inverse(proj); XMMATRIX invViewProj = MathHelper::Inverse(viewProj);
    const bool isMenuScene =
        dynamic_cast<LoginScene*>(mCurrentScene.get()) != nullptr ||
        dynamic_cast<RoomSelectScene*>(mCurrentScene.get()) != nullptr ||
        dynamic_cast<MainMenuScene*>(mCurrentScene.get()) != nullptr;
    auto stage1 = dynamic_cast<Stage1Scene*>(mCurrentScene.get());
    auto stage2 = dynamic_cast<Stage2Scene*>(mCurrentScene.get());
    const bool isVillage = dynamic_cast<VillageScene*>(mCurrentScene.get()) != nullptr;
    const float stage2EclipseProgress = GetStage2SkyEclipseProgress(stage2);
    Light effectiveSunLight = mGameLights[0].GetRawData();
    if (stage2)
    {
        effectiveSunLight = BuildStage2SunLight(stage2EclipseProgress);
    }
    else if (isVillage)
    {
        effectiveSunLight = BuildStage2SunLight(0.0f);
    }

    PassConstants mMainPassCB;
    DirectX::XMStoreFloat4x4(&mMainPassCB.View, XMMatrixTranspose(view)); DirectX::XMStoreFloat4x4(&mMainPassCB.InvView, XMMatrixTranspose(invView));
    DirectX::XMStoreFloat4x4(&mMainPassCB.Proj, XMMatrixTranspose(proj)); DirectX::XMStoreFloat4x4(&mMainPassCB.InvProj, XMMatrixTranspose(invProj));
    DirectX::XMStoreFloat4x4(&mMainPassCB.ViewProj, XMMatrixTranspose(viewProj)); DirectX::XMStoreFloat4x4(&mMainPassCB.InvViewProj, XMMatrixTranspose(invViewProj));

    XMVECTOR lightDir = XMLoadFloat3(&effectiveSunLight.Direction);

    XMVECTOR targetPos = mCamera.GetPosition();
    XMVECTOR lightPos = targetPos - (100.0f * lightDir);
    XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f); 
    XMMATRIX lightView = XMMatrixLookAtLH(lightPos, targetPos, up);
    XMMATRIX lightProj = XMMatrixOrthographicLH(100.0f, 100.0f, 1.0f, 1000.0f);

    XMMATRIX T(0.5f, 0.0f, 0.0f, 0.0f, 0.0f, -0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.5f, 0.5f, 0.0f, 1.0f);
    XMMATRIX S = lightView * lightProj * T;

    DirectX::XMStoreFloat4x4(&mMainPassCB.ShadowTransform, XMMatrixTranspose(S));
    mMainPassCB.EyePosW = mCamera.GetPosition3f(); mMainPassCB.RenderTargetSize = { (float)mClientWidth, (float)mClientHeight };
    mMainPassCB.InvRenderTargetSize = { 1.0f / mClientWidth, 1.0f / mClientHeight };
    mMainPassCB.NearZ = 1.0f; mMainPassCB.FarZ = 10000.0f; mMainPassCB.TotalTime = gt.TotalTime(); mMainPassCB.DeltaTime = gt.DeltaTime();
    mMainPassCB.AmbientLight = { 0.25f, 0.25f, 0.35f, 1.0f };
    mMainPassCB.SkyEclipseDirection = { 0.0f, 0.0f, 1.0f, 0.0f };
    mMainPassCB.SkyEclipseParams = { 0.0f, 0.35f, 1.0f, 0.0f };
    for (int i = 0; i < MaxLights; ++i) { mGameLights[i].Update(gt.DeltaTime()); mMainPassCB.Lights[i] = mGameLights[i].GetRawData(); }
    mMainPassCB.Lights[0] = effectiveSunLight;

    if (mPlayer) {
        mMainPassCB.DomainCenter = mPlayer->GetPosition();
    }
    else {
        mMainPassCB.DomainCenter = { 0.0f, 0.0f, 0.0f };
    }

    if (stage1) {
        mMainPassCB.DomainRadius = stage1->GetDomainRadius();
        mMainPassCB.IsDomainActive = stage1->GetIsDomainActive() ? 1 : 0;
        mMainPassCB.FogPad = { 1.0f, 0.0f };

        if (stage1->IsOtherWorld())
        {
            mMainPassCB.FogColor = { 0.04f, 0.05f, 0.12f, 1.0f };
            mMainPassCB.FogStart = 0.5f;
            mMainPassCB.FogRange = 11.0f;
            mMainPassCB.SkyTint = { 0.26f, 0.12f, 0.32f, 1.0f };
            mMainPassCB.HeightFogTop = 5.0f;
            mMainPassCB.HeightFogRange = 9.0f;
            mMainPassCB.HeightFogStrength = 0.75f;
        }
        else
        {
            mMainPassCB.FogColor = { 0.16f, 0.18f, 0.22f, 1.0f };
            mMainPassCB.FogStart = 0.75f;
            mMainPassCB.FogRange = 12.0f;
            mMainPassCB.SkyTint = { 0.52f, 0.16f, 0.18f, 1.0f };
            mMainPassCB.HeightFogTop = 5.5f;
            mMainPassCB.HeightFogRange = 9.0f;
            mMainPassCB.HeightFogStrength = 0.65f;
        }
    }
    else if (stage2) {
        mMainPassCB.DomainRadius = stage2->GetDomainRadius();
        mMainPassCB.IsDomainActive = stage2->GetIsDomainActive() ? 1 : 0;
        mMainPassCB.FogColor = { 0.16f, 0.18f, 0.22f, 1.0f };
        mMainPassCB.FogStart = 28.0f;
        mMainPassCB.FogRange = 120.0f;
        mMainPassCB.SkyTint = stage2->IsOtherWorld()
            ? DirectX::XMFLOAT4{ 0.26f, 0.12f, 0.32f, 1.0f }
            : DirectX::XMFLOAT4{ 0.52f, 0.16f, 0.18f, 1.0f };
        mMainPassCB.HeightFogTop = -1000.0f;
        mMainPassCB.HeightFogRange = 1.0f;
        mMainPassCB.HeightFogStrength = 0.0f;
        mMainPassCB.AmbientLight = BuildStage2AmbientLight(stage2->IsOtherWorld(), stage2EclipseProgress);
        mMainPassCB.SkyEclipseDirection =
        {
            kStage2SkyEclipseDirection.x,
            kStage2SkyEclipseDirection.y,
            kStage2SkyEclipseDirection.z,
            1.0f
        };
        mMainPassCB.SkyEclipseParams =
        {
            stage2EclipseProgress,
            0.24f,
            1.0f,
            kStage2SkyEclipseDurationSeconds
        };
    }
    else if (isVillage) {
        mMainPassCB.DomainRadius = 0.0f;
        mMainPassCB.IsDomainActive = 0;
        mMainPassCB.FogColor = { 0.16f, 0.18f, 0.22f, 1.0f };
        mMainPassCB.FogStart = 28.0f;
        mMainPassCB.FogRange = 120.0f;
        mMainPassCB.SkyTint = { 0.52f, 0.16f, 0.18f, 1.0f };
        mMainPassCB.HeightFogTop = -1000.0f;
        mMainPassCB.HeightFogRange = 1.0f;
        mMainPassCB.HeightFogStrength = 0.0f;
        mMainPassCB.AmbientLight = BuildStage2AmbientLight(false, 0.0f);
        mMainPassCB.SkyEclipseDirection =
        {
            kStage2SkyEclipseDirection.x,
            kStage2SkyEclipseDirection.y,
            kStage2SkyEclipseDirection.z,
            1.0f
        };
        mMainPassCB.SkyEclipseParams =
        {
            0.0f,
            0.16f,
            1.0f,
            kStage2SkyEclipseDurationSeconds
        };
    }
    else {
        mMainPassCB.DomainRadius = 0.0f;
        mMainPassCB.IsDomainActive = 0;
        mMainPassCB.FogColor = { 0.16f, 0.18f, 0.22f, 1.0f };
        mMainPassCB.FogStart = 28.0f;
        mMainPassCB.FogRange = 120.0f;
        mMainPassCB.SkyTint = { 1.0f, 1.0f, 1.0f, 1.0f };
        mMainPassCB.HeightFogTop = -1000.0f;
        mMainPassCB.HeightFogRange = 1.0f;
        mMainPassCB.HeightFogStrength = 0.0f;
    }

    if (isMenuScene)
    {
        mMainPassCB.AmbientLight = { 1.0f, 1.0f, 1.0f, 1.0f };
        for (int i = 0; i < MaxLights; ++i)
        {
            mMainPassCB.Lights[i].Strength = { 0.0f, 0.0f, 0.0f };
        }

        mMainPassCB.DomainRadius = 0.0f;
        mMainPassCB.IsDomainActive = 0;
        mMainPassCB.FogColor = { 0.0f, 0.0f, 0.0f, 1.0f };
        mMainPassCB.FogStart = 10000.0f;
        mMainPassCB.FogRange = 1.0f;
        mMainPassCB.SkyTint = { 1.0f, 1.0f, 1.0f, 1.0f };
        mMainPassCB.HeightFogTop = -1000.0f;
        mMainPassCB.HeightFogRange = 1.0f;
        mMainPassCB.HeightFogStrength = 0.0f;
    }

    const bool isWorldTransitionActive =
        (stage1 != nullptr && stage1->IsTransitionActive()) ||
        (stage2 != nullptr && stage2->IsTransitionActive());
    mMainPassCB.FogPad.y = (mPostProcessEnabled && !isWorldTransitionActive) ? 1.0f : 0.0f;
    mCurrFrameResource->PassCB->CopyData(0, mMainPassCB);
}

void EclipseWalkerGame::UpdateShadowPassCB(const GameTimer& gt)
{
    auto stage2 = dynamic_cast<Stage2Scene*>(mCurrentScene.get());
    const bool isVillage = dynamic_cast<VillageScene*>(mCurrentScene.get()) != nullptr;
    Light sunLight = mGameLights[0].GetRawData();
    if (stage2)
    {
        sunLight = BuildStage2SunLight(GetStage2SkyEclipseProgress(stage2));
    }
    else if (isVillage)
    {
        sunLight = BuildStage2SunLight(0.0f);
    }

    XMVECTOR lightDir = XMLoadFloat3(&sunLight.Direction);
    XMVECTOR targetPos = mCamera.GetPosition();
    XMVECTOR lightPos = targetPos - (100.0f * lightDir);
    XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    XMMATRIX lightView = XMMatrixLookAtLH(lightPos, targetPos, up);
    XMMATRIX lightProj = XMMatrixOrthographicLH(100.0f, 100.0f, 1.0f, 1000.0f);

    XMMATRIX viewProj = XMMatrixMultiply(lightView, lightProj);

    PassConstants shadowPassCB;
    DirectX::XMStoreFloat4x4(&shadowPassCB.View, XMMatrixTranspose(lightView)); DirectX::XMStoreFloat4x4(&shadowPassCB.Proj, XMMatrixTranspose(lightProj));
    DirectX::XMStoreFloat4x4(&shadowPassCB.ViewProj, XMMatrixTranspose(viewProj));
    shadowPassCB.EyePosW = { 0.0f, 0.0f, 0.0f }; shadowPassCB.RenderTargetSize = { 4096.0f, 4096.0f };
    shadowPassCB.InvRenderTargetSize = { 1.0f / 4096.0f, 1.0f / 4096.0f };

    shadowPassCB.NearZ = 1.0f; shadowPassCB.FarZ = 1000.0f;

    mCurrFrameResource->PassCB->CopyData(1, shadowPassCB);
}

void EclipseWalkerGame::UpdateUIPassCB(const GameTimer& gt)
{
    PassConstants uiPassCB;
    ZeroMemory(&uiPassCB, sizeof(PassConstants)); 

    uiPassCB.AmbientLight = { 1.0f, 1.0f, 1.0f, 1.0f };

    XMMATRIX view = XMMatrixIdentity();
    XMMATRIX proj = XMMatrixIdentity();
    XMMATRIX viewProj = XMMatrixMultiply(view, proj);

    DirectX::XMStoreFloat4x4(&uiPassCB.View, XMMatrixTranspose(view));
    DirectX::XMStoreFloat4x4(&uiPassCB.InvView, XMMatrixTranspose(view));
    DirectX::XMStoreFloat4x4(&uiPassCB.Proj, XMMatrixTranspose(proj));
    DirectX::XMStoreFloat4x4(&uiPassCB.InvProj, XMMatrixTranspose(proj));
    DirectX::XMStoreFloat4x4(&uiPassCB.ViewProj, XMMatrixTranspose(viewProj));
    DirectX::XMStoreFloat4x4(&uiPassCB.InvViewProj, XMMatrixTranspose(viewProj));

    uiPassCB.RenderTargetSize = { (float)mClientWidth, (float)mClientHeight };
    uiPassCB.InvRenderTargetSize = { 1.0f / mClientWidth, 1.0f / mClientHeight };
    uiPassCB.NearZ = 0.0f;
    uiPassCB.FarZ = 1.0f;
    uiPassCB.TotalTime = gt.TotalTime();
    uiPassCB.DeltaTime = gt.DeltaTime();
    uiPassCB.DomainRadius = mMirrorBreakEffectProgress;
    uiPassCB.IsDomainActive = mMirrorBreakEffectActive ? 1 : 0;
    uiPassCB.FogPad.y = mPostProcessEnabled ? 1.0f : 0.0f;

    mCurrFrameResource->PassCB->CopyData(2, uiPassCB);
}

LRESULT EclipseWalkerGame::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_LBUTTONDOWN: case WM_MBUTTONDOWN: case WM_RBUTTONDOWN: OnMouseDown(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)); return 0;
    case WM_LBUTTONUP: case WM_MBUTTONUP: case WM_RBUTTONUP: OnMouseUp(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)); return 0;
    case WM_MOUSEMOVE: OnMouseMove(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)); return 0;
    case WM_IME_COMPOSITION:
    {
        if (mCurrentScene != nullptr)
        {
            HIMC imeContext = ImmGetContext(hwnd);
            if (imeContext)
            {
                if (lParam & GCS_COMPSTR)
                {
                    LONG byteCount = ImmGetCompositionStringW(imeContext, GCS_COMPSTR, nullptr, 0);
                    std::wstring composing;
                    if (byteCount > 0)
                    {
                        composing.resize(static_cast<size_t>(byteCount / sizeof(wchar_t)));
                        ImmGetCompositionStringW(imeContext, GCS_COMPSTR, composing.data(), byteCount);
                    }
                    mCurrentScene->OnCompositionInput(composing, false);
                }

                if (lParam & GCS_RESULTSTR)
                {
                    LONG byteCount = ImmGetCompositionStringW(imeContext, GCS_RESULTSTR, nullptr, 0);
                    if (byteCount > 0)
                    {
                        std::wstring result(static_cast<size_t>(byteCount / sizeof(wchar_t)), L'\0');
                        ImmGetCompositionStringW(imeContext, GCS_RESULTSTR, result.data(), byteCount);
                        mPendingImeCharSkips += static_cast<int>(result.size());
                        mCurrentScene->OnCompositionInput(result, true);
                    }
                }
                ImmReleaseContext(hwnd, imeContext);
            }
        }
        return 0;
    }
    case WM_CHAR:
    {
        if (mCurrentScene != nullptr)
        {
            if (wParam == VK_RETURN || wParam == VK_BACK || wParam == VK_TAB)
            {
                mCurrentScene->OnCharInput(wParam);
            }
            else if (wParam >= 32)
            {
                if (mPendingImeCharSkips > 0)
                {
                    --mPendingImeCharSkips;
                    return 0;
                }

                std::wstring text(1, static_cast<wchar_t>(wParam));
                mCurrentScene->OnTextInput(text);
            }
        }
    }
    return 0;
    }
    return GameFramework::MsgProc(hwnd, msg, wParam, lParam);
}
void EclipseWalkerGame::UpdatePlayerTierDebugInput()
{
    constexpr int kTierKeys[3] = { '1', '2', '3' };
    constexpr ClassTier kTiers[3] = {
        ClassTier::Tier1,
        ClassTier::Tier2,
        ClassTier::Tier3
    };

    for (int i = 0; i < 3; ++i)
    {
        const bool keyDown = (GetAsyncKeyState(kTierKeys[i]) & 0x8000) != 0;
        if (keyDown && !mDebugTierKeyPressed[i])
        {
            if (mPlayerObject != nullptr)
            {
                const ClassTier requestedTier = kTiers[i];
                ApplySelectedPlayerTierVisual(requestedTier);

                std::ostringstream oss;
                oss << "[PlayerTierDebug] Switched player visual to tier " << (i + 1) << "\n";
                OutputDebugStringA(oss.str().c_str());
            }
        }

        mDebugTierKeyPressed[i] = keyDown;
    }
}

void EclipseWalkerGame::OnKeyboardInput(const GameTimer& gt)
{
    if (gIsChatInputActive) return;
    if (GetForegroundWindow() != mhMainWnd) return;

    if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) PostQuitMessage(0);

    const bool postProcessToggleDown = (GetAsyncKeyState(VK_F9) & 0x8000) != 0;
    if (postProcessToggleDown && !mPostProcessToggleKeyPressed)
    {
        mPostProcessEnabled = !mPostProcessEnabled;
        OutputDebugStringA(mPostProcessEnabled ? "[PostProcess] ON\n" : "[PostProcess] OFF\n");
    }
    mPostProcessToggleKeyPressed = postProcessToggleDown;

    UpdatePlayerTierDebugInput();

    if (kEnableWeaponSocketDebugInput)
    {
        UpdateWeaponSocketDebug(gt);
    }
}
void EclipseWalkerGame::OnMouseDown(WPARAM btnState, int x, int y) { mLastMousePos.x = x; mLastMousePos.y = y; SetCapture(mhMainWnd); SetFocus(mhMainWnd); }
void EclipseWalkerGame::OnMouseUp(WPARAM btnState, int x, int y) { ReleaseCapture(); }
void EclipseWalkerGame::OnMouseMove(WPARAM btnState, int x, int y) {
    if ((btnState & MK_RBUTTON) != 0) {
        float dx = XMConvertToRadians(0.25f * static_cast<float>(x - mLastMousePos.x)); float dy = XMConvertToRadians(0.25f * static_cast<float>(y - mLastMousePos.y));
        if (mPlayer) mPlayer->OnMouseMove(dx, dy);
    }
    mLastMousePos.x = x; mLastMousePos.y = y;
}

void EclipseWalkerGame::UpdateRemotePlayers(float dt)
{
    constexpr float kRemotePlayerSmoothingRate = 16.0f;
    constexpr float kRemotePlayerTeleportDistance = 6.0f;

    auto* network = NetworkManager::Get();
    auto& remoteDataMap = network->m_remotePlayers;
    const int myPlayerId = network->m_myPlayerId;
    if (myPlayerId > 0)
    {
        remoteDataMap.erase(myPlayerId);
        HideRemotePlayer(myPlayerId);
    }

    const unsigned long long now = GetTickCount64();

    for (auto& pair : remoteDataMap)
    {
        int playerId = pair.first;
        PKT_S_PLAYER_MOVE& data = pair.second;
        if (myPlayerId > 0 && playerId == myPlayerId)
        {
            continue;
        }

        const DirectX::XMFLOAT3 spawnPosition = { data.x, data.y, data.z };
        GameObject* targetObj = EnsureRemotePlayerObject(
            playerId,
            data.classType,
            data.playerLevel,
            spawnPosition,
            data.rotY);
        if (targetObj == nullptr)
        {
            continue;
        }

        RemotePlayerMotionState& motion = mRemotePlayerMotionStates[playerId];
        const DirectX::XMFLOAT3 serverPosition = { data.x, data.y, data.z };

        if (!motion.initialized)
        {
            motion.targetPosition = serverPosition;
            motion.currentYaw = data.rotY;
            motion.initialized = true;
            targetObj->SetPosition(serverPosition.x, serverPosition.y, serverPosition.z);
            targetObj->SetRotation(0.0f, motion.currentYaw, 0.0f);
        }
        else
        {
            motion.targetPosition = serverPosition;

            const DirectX::XMFLOAT3 currentPosition = targetObj->GetPosition();
            const float dx = motion.targetPosition.x - currentPosition.x;
            const float dy = motion.targetPosition.y - currentPosition.y;
            const float dz = motion.targetPosition.z - currentPosition.z;
            const float distanceSq = dx * dx + dy * dy + dz * dz;
            const float teleportDistanceSq = kRemotePlayerTeleportDistance * kRemotePlayerTeleportDistance;

            if (distanceSq >= teleportDistanceSq)
            {
                targetObj->SetPosition(
                    motion.targetPosition.x,
                    motion.targetPosition.y,
                    motion.targetPosition.z);
                motion.currentYaw = data.rotY;
            }
            else
            {
                const float blend = 1.0f - std::exp(-kRemotePlayerSmoothingRate * (std::max)(0.0f, dt));
                targetObj->SetPosition(
                    currentPosition.x + dx * blend,
                    currentPosition.y + dy * blend,
                    currentPosition.z + dz * blend);
                motion.currentYaw += std::remainder(data.rotY - motion.currentYaw, DirectX::XM_2PI) * blend;
            }

            targetObj->SetRotation(0.0f, motion.currentYaw, 0.0f);
        }

        const int animationState = data.animationState;
        const bool isDead = mRemotePlayerDeadStates[playerId];
        bool isRespawning = false;
        auto respawnEndIt = mRemotePlayerRespawnEndTicks.find(playerId);
        if (respawnEndIt != mRemotePlayerRespawnEndTicks.end())
        {
            isRespawning = respawnEndIt->second > now;
            if (!isRespawning)
            {
                mRemotePlayerRespawnEndTicks.erase(respawnEndIt);
                mRemotePlayerAnimationStates[playerId] = -1;
            }
        }

        bool attackActive = false;
        auto attackEndIt = mRemotePlayerAttackEndTicks.find(playerId);
        if (attackEndIt != mRemotePlayerAttackEndTicks.end())
        {
            attackActive = attackEndIt->second > now;
            if (!attackActive)
            {
                mRemotePlayerAttackEndTicks.erase(attackEndIt);
                mRemotePlayerAnimationStates[playerId] = -1;
            }
        }

        if (!isDead && !isRespawning && !attackActive && mRemotePlayerAnimationStates[playerId] != animationState)
        {
            if (auto* animation = targetObj->GetSkeletalAnimation())
            {
                if (animation->Play(GetPlayerAnimationClipName(animationState)))
                {
                    mRemotePlayerAnimationStates[playerId] = animationState;
                }
            }
        }

        targetObj->Update(); // ← 핵심: 이게 없어서 화면에 안 보였던 것
    }

    for (const PKT_S_PLAYER_ATTACK& attack : NetworkManager::Get()->PopRemotePlayerAttacks())
    {
        if (attack.attackPhase != PLAYER_ATTACK_PHASE_CAST &&
            attack.attackPhase != PLAYER_ATTACK_PHASE_IMPACT)
        {
            continue;
        }

        const bool attackHasValidClass = IsValidNetworkPlayerClass(attack.classType);
        const bool attackHasValidLevel = IsValidNetworkPlayerLevel(attack.playerLevel);
        GameObject* targetObj = nullptr;
        if (attackHasValidClass && attackHasValidLevel)
        {
            const bool hadRemoteData = remoteDataMap.find(attack.playerId) != remoteDataMap.end();
            PKT_S_PLAYER_MOVE& remoteData = remoteDataMap[attack.playerId];
            remoteData.header.size = sizeof(PKT_S_PLAYER_MOVE);
            remoteData.header.id = S_PLAYER_MOVE;
            remoteData.playerId = attack.playerId;
            remoteData.classType = attack.classType;
            remoteData.playerLevel = attack.playerLevel;
            if (attack.attackPhase == PLAYER_ATTACK_PHASE_CAST || !hadRemoteData)
            {
                remoteData.x = attack.x;
                remoteData.y = attack.y;
                remoteData.z = attack.z;
                remoteData.rotY = attack.rotY;
            }

            const DirectX::XMFLOAT3 spawnPosition = { remoteData.x, remoteData.y, remoteData.z };
            targetObj = EnsureRemotePlayerObject(
                attack.playerId,
                attack.classType,
                attack.playerLevel,
                spawnPosition,
                remoteData.rotY);
        }

        if (mCurrentScene != nullptr)
        {
            mCurrentScene->OnRemotePlayerAttack(attack);
        }

        if (targetObj == nullptr)
        {
            auto it = mRemotePlayerObjects.find(attack.playerId);
            if (it == mRemotePlayerObjects.end() || it->second == nullptr)
            {
                continue;
            }
            targetObj = it->second;
        }

        if (attack.attackPhase == PLAYER_ATTACK_PHASE_CAST)
        {
            targetObj->SetPosition(attack.x, attack.y, attack.z);
            targetObj->SetRotation(0.0f, attack.rotY, 0.0f);
            mRemotePlayerMotionStates[attack.playerId] = {
                { attack.x, attack.y, attack.z },
                attack.rotY,
                true
            };
        }

        if (attack.attackPhase == PLAYER_ATTACK_PHASE_CAST && !mRemotePlayerDeadStates[attack.playerId])
        {
            if (auto* animation = targetObj->GetSkeletalAnimation())
            {
                const PlayerClass remotePlayerClass = DecodeNetworkPlayerClass(attack.classType);
                const char* clipName = GetPlayerAttackClipName(attack.skillType, remotePlayerClass);
                if (clipName != nullptr && animation->Play(clipName, 0.08f, 1.25f))
                {
                    const float clipDuration = animation->GetClipDurationSeconds(clipName);
                    const unsigned long long durationMs = static_cast<unsigned long long>(
                        (std::max)(0.1f, clipDuration / 1.25f) * 1000.0f);
                    mRemotePlayerAttackEndTicks[attack.playerId] = GetTickCount64() + durationMs;
                    mRemotePlayerAnimationStates[attack.playerId] = -1;
                }
            }
        }

        targetObj->Update();
    }
}

void EclipseWalkerGame::ApplyRemotePlayerHit(const PKT_S_PLAYER_HIT& playerHit)
{
    if (playerHit.playerId <= 0 || playerHit.playerId == NetworkManager::Get()->m_myPlayerId ||
        (!playerHit.isDead && playerHit.remainHp > 0))
    {
        return;
    }

    auto playerIt = mRemotePlayerObjects.find(playerHit.playerId);
    if (playerIt == mRemotePlayerObjects.end() || playerIt->second == nullptr)
    {
        return;
    }

    mRemotePlayerDeadStates[playerHit.playerId] = true;
    mRemotePlayerRespawnEndTicks.erase(playerHit.playerId);
    mRemotePlayerAttackEndTicks.erase(playerHit.playerId);
    mRemotePlayerAnimationStates[playerHit.playerId] = -1;

    if (auto* animation = playerIt->second->GetSkeletalAnimation())
    {
        animation->Play("FemaleDeath", 0.14f, 1.0f, false);
    }
}

void EclipseWalkerGame::ApplyRemotePlayerRespawn(const PKT_S_PLAYER_RESPAWN& respawn)
{
    if (respawn.playerId <= 0 || respawn.playerId == NetworkManager::Get()->m_myPlayerId)
    {
        return;
    }

    const DirectX::XMFLOAT3 spawnPosition = { respawn.x, respawn.y, respawn.z };
    GameObject* playerObject = EnsureRemotePlayerObject(
        respawn.playerId,
        respawn.classType,
        respawn.playerLevel,
        spawnPosition,
        0.0f);
    if (playerObject == nullptr)
    {
        return;
    }

    playerObject->SetPosition(respawn.x, respawn.y, respawn.z);
    RemotePlayerMotionState& motion = mRemotePlayerMotionStates[respawn.playerId];
    motion.targetPosition = spawnPosition;
    motion.initialized = true;

    mRemotePlayerDeadStates[respawn.playerId] = false;
    mRemotePlayerAttackEndTicks.erase(respawn.playerId);
    mRemotePlayerAnimationStates[respawn.playerId] = -1;
    mRemotePlayerRespawnEndTicks.erase(respawn.playerId);

    if (auto* animation = playerObject->GetSkeletalAnimation())
    {
        const float duration = animation->GetClipDurationSeconds("FemaleRespawn");
        if (duration > 0.0f && animation->Play("FemaleRespawn", 0.18f, 1.0f, false))
        {
            const unsigned long long durationMs = static_cast<unsigned long long>(duration * 1000.0f);
            mRemotePlayerRespawnEndTicks[respawn.playerId] = GetTickCount64() + durationMs;
        }
    }

    playerObject->Update();
}
