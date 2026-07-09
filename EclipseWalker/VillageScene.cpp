#include "VillageScene.h"

#include "Camera.h"
#include "DebugConfig.h"
#include "EclipseWalkerGame.h"
#include "GameObject.h"
#include "MainMenuScene.h"
#include "MathHelper.h"
#include "MeshGeometry.h"
#include "ModelLoader.h"
#include "NetworkManager.h"
#include "Player.h"
#include "RenderItem.h"
#include "ResourceManager.h"
#include "Stage1Scene.h"
#include "d3dUtil.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <sstream>
#include <unordered_map>

using namespace DirectX;

namespace
{
    constexpr char kVillageMapPath[] = "Models/Village/village.fbx";
    constexpr char kVillageFloorColliderPath[] = "Models/Village/VillageFloorCollider.fbx";
    constexpr char kVillageWallColliderPath[] = "Models/Village/VillageWallCollider.fbx";
    constexpr float kVillageMapScale = 1.0f;
    constexpr float kVillageMinCameraDistance = 22.0f;
    constexpr float kVillageRotationX = 0.0f;
    constexpr float kVillageSpawnProbeY = 120.0f;
    constexpr float kVillageFallbackSpawnY = 4.0f;
    constexpr float kVillageCloudHeightA = 185.0f;
    constexpr float kVillageCloudHeightB = 235.0f;
    constexpr float kVillagePortalPosX = -0.030413f;
    constexpr float kVillagePortalPosY = 2.308053f;
    constexpr float kVillagePortalPosZ = -40.4005f;
    constexpr float kVillagePortalInteractRange = 2.4f;

    struct VillageMaterialBinding
    {
        std::string MaterialName;
        bool HideSubset = false;
    };

    std::string ToLowerAscii(std::string value)
    {
        for (char& ch : value)
        {
            if (ch >= 'A' && ch <= 'Z')
            {
                ch = static_cast<char>(ch - 'A' + 'a');
            }
        }

        return value;
    }

    void BackfillVillageDiffuseNamesFromKnownMapping(
        std::vector<ImportedMaterialInfo>& targetInfos)
    {
        if (targetInfos.empty())
        {
            return;
        }

        static const std::unordered_map<std::string, std::string> kDiffuseByMaterialName =
        {
            { "ground", "*0" },
            { "fence_f_1311_2422_2333", "*3" },
            { "gate", "*6" },
            { "material_8", "*9" },
            { "scaffold", "*12" },
            { "material", "*15" },
            { "church", "*18" },
            { "market", "*21" },
            { "home", "*24" },
            { "material_7", "*27" },
            { "tower", "*30" },
            { "stockade", "*33" },
        };

        for (ImportedMaterialInfo& targetInfo : targetInfos)
        {
            if (!targetInfo.DiffuseTextureName.empty() || targetInfo.MaterialName.empty())
            {
                continue;
            }

            const auto it = kDiffuseByMaterialName.find(ToLowerAscii(targetInfo.MaterialName));
            if (it != kDiffuseByMaterialName.end())
            {
                targetInfo.DiffuseTextureName = it->second;
            }
        }
    }

    bool IsPlayerNearVillagePortal(const XMFLOAT3& position)
    {
        const float dx = position.x - kVillagePortalPosX;
        const float dz = position.z - kVillagePortalPosZ;
        return (dx * dx + dz * dz) <= (kVillagePortalInteractRange * kVillagePortalInteractRange);
    }

    bool TryLoadVillageTexture(
        ResourceManager* resources,
        const std::filesystem::path& baseDirectory,
        const std::string& sourceTextureName,
        const std::string& resourceName)
    {
        if (resources == nullptr || sourceTextureName.empty())
        {
            return false;
        }

        if (resources->GetTexture(resourceName) != nullptr)
        {
            return true;
        }

        const bool isEmbeddedReference =
            !sourceTextureName.empty() &&
            sourceTextureName[0] == '*' &&
            sourceTextureName.size() > 1;
        const std::filesystem::path sourcePath(sourceTextureName);
        const std::string stem = sourcePath.stem().string();
        const std::wstring stemWide(stem.begin(), stem.end());
        const std::wstring fileWide = std::wstring(sourceTextureName.begin(), sourceTextureName.end());
        std::wstring embeddedFileWide;
        if (isEmbeddedReference)
        {
            const std::string embeddedIndex = sourceTextureName.substr(1);
            embeddedFileWide = L"embedded_" + std::wstring(embeddedIndex.begin(), embeddedIndex.end()) + L".dds";
        }

        const std::filesystem::path candidates[] =
        {
            isEmbeddedReference ? (baseDirectory / L"village_textures" / embeddedFileWide) : std::filesystem::path(),
            isEmbeddedReference ? (std::filesystem::path(L"Textures/village_textures") / embeddedFileWide) : std::filesystem::path(),
            baseDirectory / fileWide,
            baseDirectory / (stemWide + L".dds"),
            baseDirectory / (stemWide + L".jpg"),
            baseDirectory / (stemWide + L".jpeg"),
            std::filesystem::path(L"Textures") / (stemWide + L".dds"),
            std::filesystem::path(L"Textures") / (stemWide + L".jpg")
        };

        for (const std::filesystem::path& candidate : candidates)
        {
            if (std::filesystem::exists(candidate))
            {
                resources->LoadTexture(resourceName, candidate.wstring());
                return true;
            }
        }

        return false;
    }

    const char* ResolveVillageColliderPath(const char* primaryPath, const char* legacyPath)
    {
        if (std::filesystem::exists(std::filesystem::path(primaryPath)))
        {
            return primaryPath;
        }

        if (legacyPath != nullptr && std::filesystem::exists(std::filesystem::path(legacyPath)))
        {
            return legacyPath;
        }

        return primaryPath;
    }

    float WrapUnit(float value)
    {
        const float wrapped = std::fmod(value, 1.0f);
        return wrapped < 0.0f ? wrapped + 1.0f : wrapped;
    }

    void SetCloudTexTransform(
        RenderItem* renderItem,
        float tileU,
        float tileV,
        float offsetU,
        float offsetV)
    {
        if (renderItem == nullptr)
        {
            return;
        }

        DirectX::XMStoreFloat4x4(
            &renderItem->TexTransform,
            DirectX::XMMatrixScaling(tileU, tileV, 1.0f) *
            DirectX::XMMatrixTranslation(offsetU, offsetV, 0.0f));
        renderItem->NumFramesDirty = gNumFrameResources;
    }
}

VillageScene::VillageScene(EclipseWalkerGame* game)
    : Scene(game)
    , mChatController(game)
{
}

void VillageScene::TrackOwned(GameObject* object, RenderItem* renderItem)
{
    if (object != nullptr)
    {
        mOwnedObjects.push_back(object);
    }
    if (renderItem != nullptr)
    {
        mOwnedRenderItems.push_back(renderItem);
    }
}

void VillageScene::ReleaseOwnedObjects()
{
    auto& ritems = mGame->GetRitems();
    auto& objs = mGame->GetGameObjects();

    objs.erase(
        std::remove_if(
            objs.begin(),
            objs.end(),
            [&](const std::unique_ptr<GameObject>& object)
            {
                return std::find(mOwnedObjects.begin(), mOwnedObjects.end(), object.get()) != mOwnedObjects.end();
            }),
        objs.end());

    ritems.erase(
        std::remove_if(
            ritems.begin(),
            ritems.end(),
            [&](const std::unique_ptr<RenderItem>& renderItem)
            {
                return std::find(mOwnedRenderItems.begin(), mOwnedRenderItems.end(), renderItem.get()) != mOwnedRenderItems.end();
            }),
        ritems.end());

    mOwnedObjects.clear();
    mOwnedRenderItems.clear();
}

void VillageScene::LogPlayerPosition(const XMFLOAT3& position)
{
    std::ostringstream log;
    log << "[Debug][PlayerPos] x=" << position.x
        << " y=" << position.y
        << " z=" << position.z << "\n";
    OutputDebugStringA(log.str().c_str());
}

void VillageScene::Enter()
{
    mBackKeyPressed = false;
    mStage1KeyPressed = false;
    gIsChatInputActive = false;
    gIsLanternUiInputActive = false;

    mGame->LoadSharedGameResources();
    mGame->ResetLights();

    auto* resources = mGame->GetResources();
    auto* device = mGame->GetDevice();
    auto* commandList = mGame->GetCommandList();
    auto& ritems = mGame->GetRitems();
    auto& objects = mGame->GetGameObjects();

    if (std::filesystem::exists(L"Textures/sky_stage2.dds"))
    {
        resources->LoadTexture("sky_village", L"Textures/sky_stage2.dds");
    }
    else if (std::filesystem::exists(L"Textures/sky.dds"))
    {
        resources->LoadTexture("sky_village", L"Textures/sky.dds");
    }

    if (resources->GetMaterial("VillageFallbackMat") == nullptr)
    {
        resources->CreateMaterial(
            "VillageFallbackMat",
            static_cast<int>(resources->mMaterials.size()),
            "white",
            "",
            "",
            "",
            XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f),
            XMFLOAT3(0.05f, 0.05f, 0.05f),
            0.72f);
    }

    if (Material* fallbackMaterial = resources->GetMaterial("VillageFallbackMat"))
    {
        fallbackMaterial->IsToon = 0;
        fallbackMaterial->IsTransparent = 0;
        fallbackMaterial->OutlineThickness = 0.0f;
        fallbackMaterial->NumFramesDirty = gNumFrameResources;
    }

    if (std::filesystem::exists(L"Textures/Sky/FX_CloudAlpha05.dds"))
    {
        resources->LoadTexture("SkyCloudAlpha05", L"Textures/Sky/FX_CloudAlpha05.dds");
    }
    if (std::filesystem::exists(L"Textures/Sky/FX_CloudAlpha08.dds"))
    {
        resources->LoadTexture("SkyCloudAlpha08", L"Textures/Sky/FX_CloudAlpha08.dds");
    }

    auto ensureCloudMaterial = [&](const std::string& materialName, const std::string& diffuseMapName, const XMFLOAT4& albedo)
    {
        if (resources->GetMaterial(materialName) == nullptr)
        {
            resources->CreateMaterial(
                materialName,
                static_cast<int>(resources->mMaterials.size()),
                diffuseMapName,
                "",
                "",
                "",
                albedo,
                XMFLOAT3(0.01f, 0.01f, 0.01f),
                1.0f);
        }

        if (Material* material = resources->GetMaterial(materialName))
        {
            material->DiffuseMapName = diffuseMapName;
            material->DiffuseAlbedo = albedo;
            material->IsToon = 0;
            material->IsTransparent = 2;
            material->OutlineThickness = 0.0f;
            material->NumFramesDirty = gNumFrameResources;
        }
    };

    ensureCloudMaterial("VillageCloudLayerA", "SkyCloudAlpha05", XMFLOAT4(1.12f, 1.06f, 1.08f, 0.34f));
    ensureCloudMaterial("VillageCloudLayerB", "SkyCloudAlpha08", XMFLOAT4(0.94f, 0.90f, 0.92f, 0.16f));

    auto skyRitem = std::make_unique<RenderItem>();
    XMStoreFloat4x4(&skyRitem->World, XMMatrixScaling(5000.0f, 5000.0f, 5000.0f));
    skyRitem->TexTransform = MathHelper::Identity4x4();
    skyRitem->ObjCBIndex = static_cast<UINT>(ritems.size());
    skyRitem->Mat = resources->GetMaterial("VillageFallbackMat");
    skyRitem->Geo = resources->mGeometries["boxGeo"].get();
    skyRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    auto& skyArgs = skyRitem->Geo->DrawArgs["box"];
    skyRitem->IndexCount = skyArgs.IndexCount;
    skyRitem->StartIndexLocation = skyArgs.StartIndexLocation;
    skyRitem->BaseVertexLocation = skyArgs.BaseVertexLocation;
    skyRitem->Visible = true;
    skyRitem->IsSkybox = true;
    TrackOwned(nullptr, skyRitem.get());
    ritems.push_back(std::move(skyRitem));

    auto addCloudLayer = [&](const std::string& materialName, float y, float scale, float yaw) -> RenderItem*
    {
        auto renderItem = std::make_unique<RenderItem>();
        renderItem->World = MathHelper::Identity4x4();
        renderItem->TexTransform = MathHelper::Identity4x4();
        renderItem->ObjCBIndex = static_cast<UINT>(ritems.size());
        renderItem->Mat = resources->GetMaterial(materialName);
        renderItem->Geo = resources->mGeometries["quadGeo"].get();
        renderItem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        renderItem->CastShadow = false;
        renderItem->Visible = renderItem->Mat != nullptr && renderItem->Geo != nullptr;

        if (renderItem->Geo != nullptr)
        {
            auto& drawArgs = renderItem->Geo->DrawArgs["quad"];
            renderItem->IndexCount = drawArgs.IndexCount;
            renderItem->StartIndexLocation = drawArgs.StartIndexLocation;
            renderItem->BaseVertexLocation = drawArgs.BaseVertexLocation;
        }

        auto object = std::make_unique<GameObject>();
        object->Ritem = renderItem.get();
        object->SetScale(scale, scale, 1.0f);
        object->SetRotation(-DirectX::XM_PIDIV2, yaw, 0.0f);
        object->SetPosition(0.0f, y, 0.0f);
        object->Update();

        RenderItem* rawRenderItem = renderItem.get();
        TrackOwned(object.get(), rawRenderItem);
        ritems.push_back(std::move(renderItem));
        objects.push_back(std::move(object));
        return rawRenderItem;
    };

    mCloudLayerA = addCloudLayer("VillageCloudLayerA", kVillageCloudHeightA, 1800.0f, 0.18f);
    mCloudLayerB = addCloudLayer("VillageCloudLayerB", kVillageCloudHeightB, 1500.0f, -0.31f);
    SetCloudTexTransform(mCloudLayerA, 2.8f, 2.8f, 0.0f, 0.0f);
    SetCloudTexTransform(mCloudLayerB, 2.1f, 2.1f, 0.0f, 0.0f);

    if (!std::filesystem::exists(std::filesystem::path(kVillageMapPath)))
    {
        OutputDebugStringA("[VillageScene] village.fbx not found. Expected Models/Village/village.fbx\n");
        return;
    }

    const std::filesystem::path villagePath = std::filesystem::path(kVillageMapPath);
    const std::filesystem::path villageDir = villagePath.parent_path();
    auto materialInfos = ModelLoader::LoadMaterialInfos(kVillageMapPath);
    const bool allDiffuseNamesMissing = !materialInfos.empty() &&
        std::all_of(
            materialInfos.begin(),
            materialInfos.end(),
            [](const ImportedMaterialInfo& info)
            {
                return info.DiffuseTextureName.empty();
            });
    if (allDiffuseNamesMissing)
    {
        BackfillVillageDiffuseNamesFromKnownMapping(materialInfos);
    }
    std::vector<VillageMaterialBinding> materialBindings(materialInfos.size());

    for (size_t i = 0; i < materialInfos.size(); ++i)
    {
        const ImportedMaterialInfo& info = materialInfos[i];
        const std::string textureStem = info.DiffuseTextureName.empty()
            ? ""
            : std::filesystem::path(info.DiffuseTextureName).stem().string();
        std::string textureResourceName = textureStem.empty()
            ? ""
            : "Village_Tex_" + textureStem + "_" + std::to_string(i);

        std::string diffuseMapName = "white";
        if (!textureResourceName.empty() &&
            TryLoadVillageTexture(resources, villageDir, info.DiffuseTextureName, textureResourceName))
        {
            diffuseMapName = textureResourceName;
        }

        const std::string materialName = "Village_Mat_" + std::to_string(i);
        materialBindings[i].MaterialName = materialName;

        if (resources->GetMaterial(materialName) == nullptr)
        {
            resources->CreateMaterial(
                materialName,
                static_cast<int>(resources->mMaterials.size()),
                diffuseMapName,
                "",
                "",
                "",
                XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f),
                info.FresnelR0,
                info.Roughness,
                info.MetallicFactor);
        }

        if (Material* material = resources->GetMaterial(materialName))
        {
            material->DiffuseMapName = diffuseMapName;
            material->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
            material->FresnelR0 = info.FresnelR0;
            material->Roughness = info.Roughness;
            material->MetallicFactor = info.MetallicFactor;
            material->IsToon = 0;
            material->IsTransparent = 0;
            material->OutlineThickness = 0.0f;
            material->NumFramesDirty = gNumFrameResources;
        }
    }

    MapMeshData mapData;
    if (!ModelLoader::Load(kVillageMapPath, mapData) || mapData.Vertices.empty() || mapData.Indices.empty())
    {
        OutputDebugStringA("[VillageScene] Failed to load village.fbx mesh data.\n");
        return;
    }

    XMFLOAT3 minBounds = mapData.Vertices.front().Pos;
    XMFLOAT3 maxBounds = mapData.Vertices.front().Pos;
    for (const Vertex& vertex : mapData.Vertices)
    {
        minBounds.x = (std::min)(minBounds.x, vertex.Pos.x);
        minBounds.y = (std::min)(minBounds.y, vertex.Pos.y);
        minBounds.z = (std::min)(minBounds.z, vertex.Pos.z);
        maxBounds.x = (std::max)(maxBounds.x, vertex.Pos.x);
        maxBounds.y = (std::max)(maxBounds.y, vertex.Pos.y);
        maxBounds.z = (std::max)(maxBounds.z, vertex.Pos.z);
    }

    auto villageGeo = std::make_unique<MeshGeometry>();
    villageGeo->Name = "villageMapGeo";

    const UINT vbByteSize = static_cast<UINT>(mapData.Vertices.size() * sizeof(Vertex));
    const UINT ibByteSize = static_cast<UINT>(mapData.Indices.size() * sizeof(std::uint32_t));

    D3DCreateBlob(vbByteSize, &villageGeo->VertexBufferCPU);
    CopyMemory(villageGeo->VertexBufferCPU->GetBufferPointer(), mapData.Vertices.data(), vbByteSize);
    D3DCreateBlob(ibByteSize, &villageGeo->IndexBufferCPU);
    CopyMemory(villageGeo->IndexBufferCPU->GetBufferPointer(), mapData.Indices.data(), ibByteSize);
    villageGeo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(
        device,
        commandList,
        mapData.Vertices.data(),
        vbByteSize,
        villageGeo->VertexBufferUploader);
    villageGeo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(
        device,
        commandList,
        mapData.Indices.data(),
        ibByteSize,
        villageGeo->IndexBufferUploader);
    villageGeo->VertexByteStride = sizeof(Vertex);
    villageGeo->VertexBufferByteSize = vbByteSize;
    villageGeo->IndexFormat = DXGI_FORMAT_R32_UINT;
    villageGeo->IndexBufferByteSize = ibByteSize;

    for (const Subset& subset : mapData.Subsets)
    {
        SubmeshGeometry submesh;
        submesh.IndexCount = subset.IndexCount;
        submesh.StartIndexLocation = subset.IndexStart;
        submesh.BaseVertexLocation = 0;
        villageGeo->DrawArgs["subset_" + std::to_string(subset.Id)] = submesh;
    }
    resources->mGeometries[villageGeo->Name] = std::move(villageGeo);

    const XMFLOAT3 sourceCenter =
    {
        (minBounds.x + maxBounds.x) * 0.5f,
        (minBounds.y + maxBounds.y) * 0.5f,
        (minBounds.z + maxBounds.z) * 0.5f
    };
    const XMFLOAT3 sourceExtents =
    {
        (maxBounds.x - minBounds.x) * 0.5f,
        (maxBounds.y - minBounds.y) * 0.5f,
        (maxBounds.z - minBounds.z) * 0.5f
    };
    const float radius = (std::max)(
        kVillageMinCameraDistance,
        (std::max)(sourceExtents.x, sourceExtents.z) * kVillageMapScale * 2.4f);
    const XMFLOAT3 worldOffset =
    {
        -sourceCenter.x * kVillageMapScale,
        -minBounds.y * kVillageMapScale,
        -sourceCenter.z * kVillageMapScale
    };

    for (const Subset& subset : mapData.Subsets)
    {
        const bool isBrokenGroundStrip =
            subset.MaterialIndex == 0 &&
            subset.IndexCount <= 60;
        if (isBrokenGroundStrip)
        {
            continue;
        }

        auto renderItem = std::make_unique<RenderItem>();
        renderItem->World = MathHelper::Identity4x4();
        renderItem->TexTransform = MathHelper::Identity4x4();
        renderItem->Geo = resources->mGeometries["villageMapGeo"].get();
        renderItem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        renderItem->ObjCBIndex = static_cast<UINT>(ritems.size());
        renderItem->Visible = true;

        const std::string subsetName = "subset_" + std::to_string(subset.Id);
        auto& drawArgs = renderItem->Geo->DrawArgs[subsetName];
        renderItem->IndexCount = drawArgs.IndexCount;
        renderItem->BaseVertexLocation = drawArgs.BaseVertexLocation;
        renderItem->StartIndexLocation = drawArgs.StartIndexLocation;

        if (subset.MaterialIndex < materialBindings.size())
        {
            renderItem->Mat = resources->GetMaterial(materialBindings[subset.MaterialIndex].MaterialName);
        }
        if (renderItem->Mat == nullptr)
        {
            renderItem->Mat = resources->GetMaterial("VillageFallbackMat");
        }

        auto object = std::make_unique<GameObject>();
        object->Ritem = renderItem.get();
        object->SetScale(kVillageMapScale, kVillageMapScale, kVillageMapScale);
        object->SetRotation(kVillageRotationX, 0.0f, 0.0f);
        object->SetPosition(worldOffset.x, worldOffset.y, worldOffset.z);
        object->Update();

        TrackOwned(object.get(), renderItem.get());
        ritems.push_back(std::move(renderItem));
        objects.push_back(std::move(object));
    }

    mGame->BuildDescriptorHeaps();
    mChatController.Initialize();

    mMapSystem = std::make_unique<MapSystem>();
    const char* floorColliderPath = ResolveVillageColliderPath(
        kVillageFloorColliderPath,
        "Models/Village/VillageFloorCollider.fbx");
    const char* wallColliderPath = ResolveVillageColliderPath(
        kVillageWallColliderPath,
        "Models/Village/VillageWallCollider.fbx");

    if (!mMapSystem->LoadFloorCollider(
        floorColliderPath,
        kVillageMapScale,
        0.0f,
        0.0f,
        0.0f,
        worldOffset.x,
        worldOffset.y,
        worldOffset.z))
    {
        OutputDebugStringA("[VillageScene] Failed to load floor collider.\n");
    }
    if (std::filesystem::exists(std::filesystem::path(wallColliderPath)))
    {
        if (!mMapSystem->LoadWallCollider(
            wallColliderPath,
            kVillageMapScale,
            0.0f,
            0.0f,
            0.0f,
            worldOffset.x,
            worldOffset.y,
            worldOffset.z))
        {
            OutputDebugStringA("[VillageScene] Failed to load wall collider.\n");
        }
    }

    if (Player* player = mGame->GetPlayer())
    {
        XMFLOAT3 playerStartPosition = { 0.0f, kVillageSpawnProbeY, 0.0f };
        if (mMapSystem != nullptr)
        {
            const float floorY = mMapSystem->GetFloorHeight(
                playerStartPosition.x,
                playerStartPosition.z,
                playerStartPosition.y,
                kVillageSpawnProbeY * 2.0f);
            if (floorY > -9000.0f)
            {
                playerStartPosition.y = floorY;
            }
            else
            {
                playerStartPosition.y = kVillageFallbackSpawnY;
            }
        }

        player->SetPosition(
            playerStartPosition.x,
            playerStartPosition.y,
            playerStartPosition.z);
        player->UpdateCamera(mMapSystem.get());

        if (DebugConfig::kEnableBackendConnection)
        {
            player->ForceSendNetworkState();
        }

        XMFLOAT3 portalPosition =
        {
            kVillagePortalPosX,
            kVillagePortalPosY,
            kVillagePortalPosZ
        };

        mPortalEffect = std::make_unique<RedPortalEffect>();
        RedPortalEffect::Settings portalSettings;
        portalSettings.Position = portalPosition;
        portalSettings.PortalWidth = 1.48f;
        portalSettings.PortalHeight = 2.18f;
        portalSettings.SmokeMaxParticles = 156;
        portalSettings.SmokeSpawnRate = 108.0f;
        portalSettings.SmokeLifetimeMin = 1.45f;
        portalSettings.SmokeLifetimeMax = 2.65f;
        portalSettings.SmokeStartScaleMin = 0.42f;
        portalSettings.SmokeStartScaleMax = 0.64f;
        portalSettings.SmokeEndScaleMin = 0.72f;
        portalSettings.SmokeEndScaleMax = 1.02f;
        portalSettings.SmokeBandInnerScale = 0.80f;
        portalSettings.SmokeBandOuterScale = 1.10f;
        portalSettings.SmokeBandVerticalJitter = 0.22f;
        portalSettings.SmokeBandTangentialJitter = 0.42f;
        portalSettings.SmokeAlphaStartMin = 0.72f;
        portalSettings.SmokeAlphaStartMax = 0.96f;
        portalSettings.SparkMaxParticles = 32;
        portalSettings.SparkSpawnRate = 18.0f;
        portalSettings.SparkLifetimeMin = 0.26f;
        portalSettings.SparkLifetimeMax = 0.60f;
        portalSettings.SparkLengthMin = 0.10f;
        portalSettings.SparkLengthMax = 0.26f;
        portalSettings.SparkWidthMin = 0.018f;
        portalSettings.SparkWidthMax = 0.040f;
        portalSettings.SmokeColor = { 1.56f, 0.20f, 0.20f, 0.98f };
        portalSettings.RingInnerColor = { 2.10f, 0.22f, 0.16f, 0.98f };
        portalSettings.CenterColor = { 0.92f, 0.92f, 0.94f, 0.88f };
        portalSettings.SparkColor = { 2.40f, 2.40f, 2.40f, 1.00f };
        mPortalEffect->Init(
            mGame,
            [this](GameObject* object, RenderItem* renderItem)
            {
                TrackOwned(object, renderItem);
            },
            portalSettings);
    }
}

void VillageScene::Exit()
{
    ReleaseOwnedObjects();
    mCloudLayerA = nullptr;
    mCloudLayerB = nullptr;
    mPortalEffect.reset();
    mGame->ResetLights();
    mMapSystem.reset();
    mChatController.Reset();
    gIsChatInputActive = false;
    gIsLanternUiInputActive = false;
}

void VillageScene::Update(const GameTimer& gt)
{
    mChatController.Update(gt);

    if (auto* uiManager = mGame->GetUIManager())
    {
        uiManager->SetChatBoxState(
            mChatController.IsChatting(),
            mChatController.HasMessages());
    }

    const float totalTime = gt.TotalTime();
    SetCloudTexTransform(mCloudLayerA, 2.8f, 2.8f, WrapUnit(totalTime * 0.0055f), WrapUnit(totalTime * 0.0018f));
    SetCloudTexTransform(mCloudLayerB, 2.1f, 2.1f, WrapUnit(totalTime * -0.0032f), WrapUnit(totalTime * 0.0024f));
    if (mPortalEffect)
    {
        mPortalEffect->Update(gt.DeltaTime());
    }

    const bool hasFocus = GetForegroundWindow() == mGame->GetMainWindowHandle();
    if (!hasFocus)
    {
        mBackKeyPressed = false;
        mStage1KeyPressed = false;
        mPortalInteractKeyPressed = false;
        mPrintPositionKeyPressed = false;
        return;
    }

    if (!mChatController.IsChatting() && (GetAsyncKeyState(VK_ESCAPE) & 0x8000))
    {
        if (!mBackKeyPressed)
        {
            mBackKeyPressed = true;
            mGame->ChangeScene(std::make_unique<MainMenuScene>(mGame));
            return;
        }
    }
    else
    {
        mBackKeyPressed = false;
    }

    if (!mChatController.IsChatting() && (GetAsyncKeyState('V') & 0x8000))
    {
        if (!mStage1KeyPressed)
        {
            mStage1KeyPressed = true;
            mGame->RequestSceneChange(std::make_unique<Stage1Scene>(mGame), L"LOADING STAGE 1");
            return;
        }
    }
    else
    {
        mStage1KeyPressed = false;
    }

    Player* player = mGame->GetPlayer();
    const bool portalInteractKeyDown = !mChatController.IsChatting() && (GetAsyncKeyState('F') & 0x8000) != 0;
    if (player != nullptr && !player->IsDead() && portalInteractKeyDown && !mPortalInteractKeyPressed)
    {
        if (IsPlayerNearVillagePortal(player->GetPosition()))
        {
            if (DebugConfig::kEnableBackendConnection)
            {
                NetworkManager::Get()->SendStageChange(1);
            }
            else
            {
                mGame->RequestSceneChange(std::make_unique<Stage1Scene>(mGame), L"LOADING STAGE 1");
            }
            mPortalInteractKeyPressed = true;
            return;
        }
    }
    mPortalInteractKeyPressed = portalInteractKeyDown;

    const bool printPositionKeyDown = !mChatController.IsChatting() && (GetAsyncKeyState(VK_RETURN) & 0x8000) != 0;
    if (player != nullptr && printPositionKeyDown && !mPrintPositionKeyPressed)
    {
        mPrintPositionKeyPressed = true;
        LogPlayerPosition(player->GetPosition());
    }
    else if (!printPositionKeyDown)
    {
        mPrintPositionKeyPressed = false;
    }

    if (player != nullptr)
    {
        player->Update(gt, mMapSystem.get());
    }
}

void VillageScene::Draw(const GameTimer& gt)
{
    UNREFERENCED_PARAMETER(gt);
    if (auto* uiManager = mGame->GetUIManager())
    {
        uiManager->DrawCooldownOverlay();
    }
    mChatController.Draw();
}

void VillageScene::OnCharInput(WPARAM charCode)
{
    mChatController.OnCharInput(charCode);
}

void VillageScene::OnTextInput(const std::wstring& text)
{
    mChatController.OnTextInput(text);
}

void VillageScene::OnCompositionInput(const std::wstring& text, bool isFinal)
{
    mChatController.OnCompositionInput(text, isFinal);
}
