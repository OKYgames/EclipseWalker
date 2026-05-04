#include "Stage1Scene.h"
#include "Stage2Scene.h"
#include "CharacterVisualFactory.h"
#include "EclipseWalkerGame.h"
#include "InteractiveDoor.h"
#include "NetworkManager.h"
#include "SkeletalAnimationComponent.h"
#include <Windows.h>
#include <algorithm>
#include <cfloat>
#include <sstream>

namespace
{
    struct MapMaterialBinding
    {
        std::string MaterialName;
        bool HideSubset = false;
    };

    constexpr float kStage1MapScale = 0.014f;
    constexpr float kStage1WorldScale = kStage1MapScale / 0.01f;
    constexpr bool kSpawnAnimatedTestActor = false;
    constexpr bool kDebugHighlightStoneLadders = false;
    constexpr bool kDebugColorizeMapMaterials = false;

    DirectX::XMFLOAT3 ScaleStage1Position(float x, float y, float z)
    {
        return { x * kStage1WorldScale, y * kStage1WorldScale, z * kStage1WorldScale };
    }

    DoorBounds CalculateBounds(const std::vector<Vertex>& vertices, float scale)
    {
        DoorBounds bounds;
        bounds.Min = { FLT_MAX, FLT_MAX, FLT_MAX };
        bounds.Max = { -FLT_MAX, -FLT_MAX, -FLT_MAX };

        for (const auto& vertex : vertices)
        {
            const float x = vertex.Pos.x * scale;
            const float y = vertex.Pos.y * scale;
            const float z = vertex.Pos.z * scale;

            bounds.Min.x = (std::min)(bounds.Min.x, x);
            bounds.Min.y = (std::min)(bounds.Min.y, y);
            bounds.Min.z = (std::min)(bounds.Min.z, z);
            bounds.Max.x = (std::max)(bounds.Max.x, x);
            bounds.Max.y = (std::max)(bounds.Max.y, y);
            bounds.Max.z = (std::max)(bounds.Max.z, z);
        }

        return bounds;
    }
}

Stage1Scene::Stage1Scene(EclipseWalkerGame* game)
    : Scene(game)
    , mChatController(game)
    , mCombatSystem(game)
    , mPickupSystem(game, &mLanternSystem)
    , mWorldStateController(game, &mLanternSystem)
{
}

Stage1Scene::~Stage1Scene()
{
}

void Stage1Scene::TrackOwned(GameObject* object, RenderItem* renderItem)
{
    if (object) mOwnedObjects.push_back(object);
    if (renderItem) mOwnedRenderItems.push_back(renderItem);
}

void Stage1Scene::ReleaseOwnedObjects()
{
    auto& ritems = mGame->GetRitems();
    auto& objs = mGame->GetGameObjects();

    objs.erase(std::remove_if(objs.begin(), objs.end(),
        [&](const std::unique_ptr<GameObject>& object)
        {
            return std::find(mOwnedObjects.begin(), mOwnedObjects.end(), object.get()) != mOwnedObjects.end();
        }),
        objs.end());

    ritems.erase(std::remove_if(ritems.begin(), ritems.end(),
        [&](const std::unique_ptr<RenderItem>& renderItem)
        {
            return std::find(mOwnedRenderItems.begin(), mOwnedRenderItems.end(), renderItem.get()) != mOwnedRenderItems.end();
        }),
        ritems.end());

    mOwnedObjects.clear();
    mOwnedRenderItems.clear();
}

void Stage1Scene::Enter()
{
    // 1. [인게임 공통 리소스] 
    mGame->LoadSharedGameResources();
    mGame->RefreshPlayerForSelectedClass();
    NetworkManager::Get()->ClearMonsterState();

    auto res = mGame->GetResources();
    auto dev = mGame->GetDevice();
    auto cmd = mGame->GetCommandList();
    auto& ritems = mGame->GetRitems();
    auto& objs = mGame->GetGameObjects();

    OutputDebugStringA("\n[Stage 1] 씬 전용 리소스 로딩 시작...\n");

    // 2. [Stage 1 텍스처 및 재질 로드]
    auto LoadMapTextures = [&](const std::vector<std::string>& textureNames)
    {
        for (const auto& originName : textureNames)
        {
            if (originName.empty()) continue;
            std::string baseName = originName.substr(0, originName.find_last_of('.'));

            auto LoadMapTex = [&](const std::string& suffix)
            {
                std::string name = baseName + suffix;
                std::wstring path = L"Models/Stage1Map/Textures/" + std::wstring(name.begin(), name.end()) + L".dds";
                if (GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES)
                {
                    res->LoadTexture(name, path);
                }
            };

            LoadMapTex("");
            LoadMapTex("_normal");
            LoadMapTex("_emissive");
            LoadMapTex("_metallic");
        }
    };

    auto realTexNames = ModelLoader::LoadTextureNames("Models/Stage1Map/RealMap_NoDoor.fbx");
    auto otherTexNames = ModelLoader::LoadTextureNames("Models/Stage1Map/OtherMap.fbx");
    LoadMapTextures(realTexNames);
    LoadMapTextures(otherTexNames);

    res->LoadTexture("Wood_metal_normal", L"Models/Stage1Map/Textures/Wood_metal_normal.dds");
    res->LoadTexture("Wood_metal_metallic", L"Models/Stage1Map/Textures/Wood_metal_metallic.dds");
    res->LoadTexture("sky", L"Textures/sky.dds");
    res->LoadTexture("MagicCircle", L"Textures/MagicCircle.dds");

    auto BuildMapMaterials = [&](const std::string& mapTag, const std::vector<std::string>& textureNames)
    {
        std::vector<MapMaterialBinding> materialBindings(textureNames.size());

        for (size_t i = 0; i < textureNames.size(); ++i)
        {
            const std::string& originName = textureNames[i];
            std::string baseName = originName.empty() ? "" : originName.substr(0, originName.find_last_of('.'));
            const bool shouldHideSubset = baseName.empty();
            std::string diffName = baseName;
            std::string normName = baseName.empty() ? "" : baseName + "_normal";
            std::string emName = baseName.empty() ? "" : baseName + "_emissive";
            std::string metName = baseName.empty() ? "" : baseName + "_metallic";

            if (baseName == "Wood_metal_albedo")
            {
                normName = "Wood_metal_normal";
                metName = "Wood_metal_metallic";
            }

            if (diffName.empty() || res->GetTexture(diffName) == nullptr)
            {
                diffName = "white";
            }
            if (!normName.empty() && res->GetTexture(normName) == nullptr)
            {
                normName.clear();
            }
            if (!emName.empty() && res->GetTexture(emName) == nullptr)
            {
                emName.clear();
            }
            if (!metName.empty() && res->GetTexture(metName) == nullptr)
            {
                metName.clear();
            }

            XMFLOAT4 diffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
            XMFLOAT3 fresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05f);
            float roughness = 0.8f;

            if (kDebugColorizeMapMaterials)
            {
                // 텍스처 대신 재질 종류별 단색을 강제로 써서 어떤 머티리얼이 보이는지 즉시 판별한다.
                normName.clear();
                emName.clear();
                metName.clear();
                diffName = "white";

                if (baseName == "Stones")
                {
                    diffuseAlbedo = XMFLOAT4(0.1f, 1.0f, 0.2f, 1.0f);
                }
                else if (baseName == "Wood_metal_albedo")
                {
                    diffuseAlbedo = XMFLOAT4(0.15f, 0.45f, 1.0f, 1.0f);
                }
                else if (baseName == "Decals")
                {
                    diffuseAlbedo = XMFLOAT4(1.0f, 0.9f, 0.15f, 1.0f);
                }
                else if (baseName.empty())
                {
                    diffuseAlbedo = XMFLOAT4(1.0f, 0.1f, 1.0f, 1.0f);
                }
                else
                {
                    diffuseAlbedo = XMFLOAT4(0.15f, 1.0f, 1.0f, 1.0f);
                }
            }

            std::string matName = mapTag + "_Mat_" + std::to_string(i);
            materialBindings[i].MaterialName = matName;
            materialBindings[i].HideSubset = shouldHideSubset;

            if (res->GetMaterial(matName) == nullptr)
            {
                res->CreateMaterial(
                    matName,
                    static_cast<int>(res->mMaterials.size()),
                    diffName,
                    normName,
                    emName,
                    metName,
                    diffuseAlbedo,
                    fresnelR0,
                    roughness);
            }

            if (Material* mat = res->GetMaterial(matName))
            {
                mat->DiffuseMapName = diffName;
                mat->NormalMapName = normName;
                mat->EmissiveMapName = emName;
                mat->MetallicMapName = metName;
                mat->DiffuseAlbedo = diffuseAlbedo;
                mat->FresnelR0 = fresnelR0;
                mat->Roughness = roughness;
                mat->IsToon = 0;
                mat->IsTransparent = 0;
                mat->NumFramesDirty = gNumFrameResources;
            }
        }

        return materialBindings;
    };

    const auto realMaterialNames = BuildMapMaterials("RealMap", realTexNames);
    const auto otherMaterialNames = BuildMapMaterials("OtherMap", otherTexNames);

    if (res->GetMaterial("MapFallbackMat") == nullptr)
    {
        res->CreateMaterial(
            "MapFallbackMat",
            static_cast<int>(res->mMaterials.size()),
            "white",
            "",
            "",
            "",
            XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f),
            XMFLOAT3(0.05f, 0.05f, 0.05f),
            0.8f);
    }

    if (auto* fallbackMat = res->GetMaterial("MapFallbackMat"))
    {
        fallbackMat->NumFramesDirty = gNumFrameResources;
    }

    if (kDebugHighlightStoneLadders && res->GetMaterial("DebugLadderMat") == nullptr)
    {
        res->CreateMaterial(
            "DebugLadderMat",
            static_cast<int>(res->mMaterials.size()),
            "white",
            "",
            "",
            "",
            XMFLOAT4(0.1f, 1.0f, 0.2f, 1.0f),
            XMFLOAT3(0.05f, 0.05f, 0.05f),
            0.8f);
    }

    if (auto* debugLadderMat = res->GetMaterial("DebugLadderMat"))
    {
        debugLadderMat->NumFramesDirty = gNumFrameResources;
    }

    if (res->GetMaterial("Stage1AbyssCoverMat") == nullptr)
    {
        res->CreateMaterial(
            "Stage1AbyssCoverMat",
            static_cast<int>(res->mMaterials.size()),
            "white",
            "",
            "",
            "",
            XMFLOAT4(0.18f, 0.07f, 0.08f, 1.0f),
            XMFLOAT3(0.02f, 0.02f, 0.02f),
            1.0f);
    }

    if (auto* abyssCoverMat = res->GetMaterial("Stage1AbyssCoverMat"))
    {
        abyssCoverMat->IsToon = 0;
        abyssCoverMat->IsTransparent = 0;
        abyssCoverMat->NumFramesDirty = gNumFrameResources;
    }

    if (res->GetMaterial("Stage1AbyssFogMat") == nullptr)
    {
        res->CreateMaterial(
            "Stage1AbyssFogMat",
            static_cast<int>(res->mMaterials.size()),
            "white",
            "",
            "",
            "",
            XMFLOAT4(0.16f, 0.08f, 0.09f, 0.16f),
            XMFLOAT3(0.02f, 0.02f, 0.02f),
            1.0f);
    }

    if (auto* abyssFogMat = res->GetMaterial("Stage1AbyssFogMat"))
    {
        abyssFogMat->IsToon = 0;
        abyssFogMat->IsTransparent = 2;
        abyssFogMat->NumFramesDirty = gNumFrameResources;
    }

    // ====================================================================
    // 3. 맵 로드 & 렌더 아이템 생성 도우미 함수 (코드 중복 방지)
    // ====================================================================
    auto CreateMapEnv = [&](const std::string& fbxPath, const std::string& geoName, const std::vector<MapMaterialBinding>& materialBindings, std::vector<RenderItem*>& targetList, bool isVisible) {
        MapMeshData mapData;
        ModelLoader::Load(fbxPath, mapData);
        auto mapGeo = std::make_unique<MeshGeometry>();
        mapGeo->Name = geoName;

        const UINT vbByteSize = (UINT)mapData.Vertices.size() * sizeof(Vertex);
        const UINT ibByteSize = (UINT)mapData.Indices.size() * sizeof(std::uint32_t);

        D3DCreateBlob(vbByteSize, &mapGeo->VertexBufferCPU); CopyMemory(mapGeo->VertexBufferCPU->GetBufferPointer(), mapData.Vertices.data(), vbByteSize);
        D3DCreateBlob(ibByteSize, &mapGeo->IndexBufferCPU); CopyMemory(mapGeo->IndexBufferCPU->GetBufferPointer(), mapData.Indices.data(), ibByteSize);
        mapGeo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(dev, cmd, mapData.Vertices.data(), vbByteSize, mapGeo->VertexBufferUploader);
        mapGeo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(dev, cmd, mapData.Indices.data(), ibByteSize, mapGeo->IndexBufferUploader);
        mapGeo->VertexByteStride = sizeof(Vertex); mapGeo->VertexBufferByteSize = vbByteSize;
        mapGeo->IndexFormat = DXGI_FORMAT_R32_UINT; mapGeo->IndexBufferByteSize = ibByteSize;

        for (const auto& subset : mapData.Subsets) {
            SubmeshGeometry submesh; submesh.IndexCount = subset.IndexCount; submesh.StartIndexLocation = subset.IndexStart; submesh.BaseVertexLocation = 0;
            mapGeo->DrawArgs["subset_" + std::to_string(subset.Id)] = submesh;
        }
        res->mGeometries[mapGeo->Name] = std::move(mapGeo);

        // 렌더 아이템 생성 및 리스트 등록
        for (const auto& subset : mapData.Subsets) {
            if (subset.MaterialIndex < materialBindings.size() && materialBindings[subset.MaterialIndex].HideSubset)
            {
                std::ostringstream hiddenSubsetLog;
                hiddenSubsetLog << "[Stage1] Hiding subset with empty diffuse material -> subset="
                    << subset.Id << " name=" << subset.Name
                    << " materialIndex=" << subset.MaterialIndex << "\n";
                OutputDebugStringA(hiddenSubsetLog.str().c_str());
                continue;
            }

            auto ritem = std::make_unique<RenderItem>();
            ritem->World = MathHelper::Identity4x4();
            ritem->TexTransform = MathHelper::Identity4x4();
            ritem->Geo = res->mGeometries[geoName].get();
            string subsetName = "subset_" + std::to_string(subset.Id);
            ritem->IndexCount = ritem->Geo->DrawArgs[subsetName].IndexCount;
            ritem->BaseVertexLocation = ritem->Geo->DrawArgs[subsetName].BaseVertexLocation;
            ritem->StartIndexLocation = ritem->Geo->DrawArgs[subsetName].StartIndexLocation;
            const bool isStoneLadderSubset =
                subset.Name.find("Stone_ladder") != std::string::npos ||
                subset.Name.find("Stone_fence_ladder") != std::string::npos;

            if (kDebugHighlightStoneLadders && isStoneLadderSubset)
            {
                ritem->Mat = res->GetMaterial("DebugLadderMat");

                std::ostringstream ladderLog;
                ladderLog << "[Stage1] Debug ladder override -> subset=" << subset.Id
                    << " name=" << subset.Name << "\n";
                OutputDebugStringA(ladderLog.str().c_str());
            }
            else if (subset.MaterialIndex < materialBindings.size())
            {
                ritem->Mat = res->GetMaterial(materialBindings[subset.MaterialIndex].MaterialName);
            }
            else
            {
                ritem->Mat = nullptr;
            }
            if (ritem->Mat == nullptr)
            {
                std::ostringstream missingMatLog;
                missingMatLog << "[Stage1] Missing material for subset " << subset.Id
                    << " materialIndex=" << subset.MaterialIndex << ", using MapFallbackMat\n";
                OutputDebugStringA(missingMatLog.str().c_str());
                ritem->Mat = res->GetMaterial("MapFallbackMat");
            }
            ritem->ObjCBIndex = static_cast<UINT>(ritems.size());

            //맵의 현재 가시성(Visible) 설정
            ritem->Visible = isVisible;

            targetList.push_back(ritem.get());

            auto mapObj = std::make_unique<GameObject>();
            mapObj->SetScale(kStage1MapScale, kStage1MapScale, kStage1MapScale);
            mapObj->Ritem = ritem.get(); mapObj->Update();
            TrackOwned(mapObj.get(), ritem.get());
            ritems.push_back(std::move(ritem)); objs.push_back(std::move(mapObj));
        }
        };

    // 현실 맵 로드 (처음엔 보이게 true)
    CreateMapEnv("Models/Stage1Map/RealMap_NoDoor.fbx", "realMapGeo", realMaterialNames, mRealWorldRitems, true);
    // 이면 맵 로드 (처음엔 안 보이게 false)
    CreateMapEnv("Models/Stage1Map/OtherMap.fbx", "otherMapGeo", otherMaterialNames, mOtherWorldRitems, false);

    auto AddAbyssCoverBox = [&](const XMFLOAT3& scale, const XMFLOAT3& position)
    {
        auto ritem = std::make_unique<RenderItem>();
        ritem->World = MathHelper::Identity4x4();
        XMStoreFloat4x4(
            &ritem->World,
            XMMatrixScaling(scale.x, scale.y, scale.z) * XMMatrixTranslation(position.x, position.y, position.z));
        ritem->TexTransform = MathHelper::Identity4x4();
        ritem->ObjCBIndex = static_cast<UINT>(ritems.size());
        ritem->Mat = res->GetMaterial("Stage1AbyssCoverMat");
        ritem->Geo = res->mGeometries["boxGeo"].get();
        ritem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        auto& abyssArgs = ritem->Geo->DrawArgs["box"];
        ritem->IndexCount = abyssArgs.IndexCount;
        ritem->StartIndexLocation = abyssArgs.StartIndexLocation;
        ritem->BaseVertexLocation = abyssArgs.BaseVertexLocation;
        ritem->Visible = true;
        TrackOwned(nullptr, ritem.get());
        ritems.push_back(std::move(ritem));
    };

    AddAbyssCoverBox(
        { 320.0f * kStage1WorldScale, 12.0f * kStage1WorldScale, 320.0f * kStage1WorldScale },
        ScaleStage1Position(2.0f, -18.0f, -8.0f));

    auto abyssFogRitem = std::make_unique<RenderItem>();
    abyssFogRitem->World = MathHelper::Identity4x4();
    XMStoreFloat4x4(
        &abyssFogRitem->World,
        XMMatrixScaling(220.0f * kStage1WorldScale, 18.0f * kStage1WorldScale, 220.0f * kStage1WorldScale) *
        XMMatrixTranslation(2.0f * kStage1WorldScale, -9.0f * kStage1WorldScale, -8.0f * kStage1WorldScale));
    abyssFogRitem->TexTransform = MathHelper::Identity4x4();
    abyssFogRitem->ObjCBIndex = static_cast<UINT>(ritems.size());
    abyssFogRitem->Mat = res->GetMaterial("Stage1AbyssFogMat");
    abyssFogRitem->Geo = res->mGeometries["boxGeo"].get();
    abyssFogRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    {
        auto& abyssFogArgs = abyssFogRitem->Geo->DrawArgs["box"];
        abyssFogRitem->IndexCount = abyssFogArgs.IndexCount;
        abyssFogRitem->StartIndexLocation = abyssFogArgs.StartIndexLocation;
        abyssFogRitem->BaseVertexLocation = abyssFogArgs.BaseVertexLocation;
    }
    abyssFogRitem->Visible = true;
    TrackOwned(nullptr, abyssFogRitem.get());
    ritems.push_back(std::move(abyssFogRitem));

    mRealMapSystem = std::make_unique<MapSystem>();
    mRealMapSystem->LoadFloorCollider("Models/Stage1Map/RealFloorCollider.fbx", kStage1MapScale);
    mRealMapSystem->LoadWallCollider("Models/Stage1Map/RealWallCollider.fbx", kStage1MapScale);

    mOtherMapSystem = std::make_unique<MapSystem>();
    mOtherMapSystem->LoadFloorCollider("Models/Stage1Map/OtherFloorCollider.fbx", kStage1MapScale);
    mOtherMapSystem->LoadWallCollider("Models/Stage1Map/OtherWallCollider.fbx", kStage1MapScale);

    // 스카이박스 및 파티클 세팅
    mSkyTexHeapIndex = res->GetTextureIndex("sky");
    {
        const auto fire0 = ScaleStage1Position(-0.1f, 0.8f, 1.1f);
        const auto fire1 = ScaleStage1Position(4.1f, 0.8f, 1.1f);
        const auto fire2 = ScaleStage1Position(2.0f, -3.10f, -26.0f);
        const auto fire3 = ScaleStage1Position(-0.01f, -0.58f, 9.0f);
        const auto fire4 = ScaleStage1Position(3.99f, -0.58f, 9.0f);
        mGame->CreateFire(fire0.x, fire0.y, fire0.z, 0.3f);
        mGame->CreateFire(fire1.x, fire1.y, fire1.z, 0.3f);
        mGame->CreateFire(fire2.x, fire2.y, fire2.z, 0.3f);
        mGame->CreateFire(fire3.x, fire3.y, fire3.z, 0.3f);
        mGame->CreateFire(fire4.x, fire4.y, fire4.z, 0.3f);
    }

    auto skyRitem = std::make_unique<RenderItem>();
    DirectX::XMStoreFloat4x4(&skyRitem->World, XMMatrixScaling(5000.0f, 5000.0f, 5000.0f));
    skyRitem->TexTransform = MathHelper::Identity4x4(); skyRitem->ObjCBIndex = static_cast<UINT>(ritems.size());
    skyRitem->Mat = res->GetMaterial("MapFallbackMat"); skyRitem->Geo = res->mGeometries["boxGeo"].get();
    skyRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    auto& drawArgs = skyRitem->Geo->DrawArgs["box"];
    skyRitem->IndexCount = drawArgs.IndexCount; skyRitem->StartIndexLocation = drawArgs.StartIndexLocation; skyRitem->BaseVertexLocation = drawArgs.BaseVertexLocation;
    skyRitem->Visible = true;
    skyRitem->IsSkybox = true;
    TrackOwned(nullptr, skyRitem.get());
    ritems.push_back(std::move(skyRitem));

    auto domainRi = std::make_unique<RenderItem>();
    domainRi->ObjCBIndex = static_cast<UINT>(ritems.size());
    domainRi->Geo = res->mGeometries["sphereGeo"].get();
    domainRi->Mat = res->GetMaterial("DomainMat");
    domainRi->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    auto& args = domainRi->Geo->DrawArgs["sphere"];
    domainRi->IndexCount = args.IndexCount;
    domainRi->StartIndexLocation = args.StartIndexLocation;
    domainRi->BaseVertexLocation = args.BaseVertexLocation;
    domainRi->Visible = false; 

    auto domainObj = std::make_unique<GameObject>();
    domainObj->Ritem = domainRi.get();
    domainObj->SetScale(0.0f, 0.0f, 0.0f);

    mDomainBoundaryObj = domainObj.get(); 

    TrackOwned(domainObj.get(), domainRi.get());
    ritems.push_back(std::move(domainRi));
    objs.push_back(std::move(domainObj));
    mWorldStateController.Initialize(mDomainBoundaryObj, &mRealWorldRitems, &mOtherWorldRitems);

    BuildInteractiveDoors();
    BuildMonsters();
    if (kSpawnAnimatedTestActor)
    {
        BuildAnimatedTestActor();
    }
    mGame->BuildDescriptorHeaps();
    mChatController.Initialize();
    mPickupSystem.Initialize();
}

void Stage1Scene::Exit()
{
    OutputDebugStringA("\n[Stage 1] 씬 종료,메모리 해제...\n");

    auto& ritems = mGame->GetRitems();
    ReleaseOwnedObjects();

    auto& objs = mGame->GetGameObjects();
    objs.erase(std::remove_if(objs.begin(), objs.end(),
        [](const std::unique_ptr<GameObject>& obj)
        {
            return obj->Ritem && obj->Ritem->Mat && obj->Ritem->Mat->Name.find("Fire") != std::string::npos;
        }),
        objs.end());

    ritems.erase(std::remove_if(ritems.begin(), ritems.end(),
        [](const std::unique_ptr<RenderItem>& ritem)
        {
            return ritem && ritem->Mat &&
                (ritem->Mat->Name.find("Fire") != std::string::npos || ritem->Mat->Name.find("Monster") != std::string::npos);
        }),
        ritems.end());

    for (UINT i = 0; i < ritems.size(); ++i)
    {
        ritems[i]->ObjCBIndex = i;
        ritems[i]->NumFramesDirty = 3;
    }

    mGame->ResetLights();
    mChatController.Reset();
    mCombatSystem.Reset();
    mPickupSystem.Reset();
    mWorldStateController.Reset();
    mRealWorldRitems.clear();
    mOtherWorldRitems.clear();
    mMonsterPtrs.clear();
    mMonsterTargetPos.clear();
    mMonsterById.clear();
    mDoors.clear();
    mDoorInteractKeyPressed = false;
    mDomainBoundaryObj = nullptr;

    OutputDebugStringA("\n[Stage 1] 해제 완료\n");
}

void Stage1Scene::Update(const GameTimer& gt)
{
    mChatController.Update(gt);

    if (auto* uiManager = mGame->GetUIManager())
    {
        uiManager->SetChatBoxState(mChatController.IsChatting(), mChatController.HasMessages());
    }

    Player* pPlayer = mGame->GetPlayer();

    bool doorInteractionConsumed = false;
    const bool fKeyDown = (GetAsyncKeyState('F') & 0x8000) != 0;
    if (!mChatController.IsChatting() && fKeyDown && !mDoorInteractKeyPressed)
    {
        const XMFLOAT3 playerPos = pPlayer->GetPosition();
        for (auto& door : mDoors)
        {
            if (door && door->TryInteract(playerPos))
            {
                doorInteractionConsumed = true;
                break;
            }
        }
    }
    mDoorInteractKeyPressed = fKeyDown;

    for (auto& door : mDoors)
    {
        if (door)
        {
            door->Update(gt.DeltaTime());
        }
    }

    mWorldStateController.Update(gt, pPlayer, true);

    static bool isGPressed = false;
    if (!mChatController.IsChatting() && (GetAsyncKeyState('G') & 0x8000))
    {
        if (!isGPressed)
        {
            mGame->ChangeScene(std::make_unique<Stage2Scene>(mGame));
            isGPressed = true;
            return;
        }
    }
    else
    {
        isGPressed = false;
    }

    // ====================================================================
    // 2. 현재 활성화된 맵 시스템 가져와서 적용하기
    // ====================================================================
    MapSystem* activeMap = GetActiveMapSystem();

    // 플레이어 물리 업데이트
    const XMFLOAT3 oldPlayerPos = pPlayer->GetPosition();
    pPlayer->ApplyPhysics(gt, activeMap);
    for (const auto& door : mDoors)
    {
        if (door == nullptr) continue;

        XMFLOAT3 resolvedPos;
        if (door->ResolvePlayerCollision(oldPlayerPos, pPlayer->GetPosition(), resolvedPos))
        {
            pPlayer->SetPosition(resolvedPos.x, resolvedPos.y, resolvedPos.z);
        }
    }

    UpdateMonstersFromServer();

    float lerpSpeed = 12.0f; // 높을수록 빠르게 따라감
    float t = min(1.0f, lerpSpeed * gt.DeltaTime());

    for (auto& pair : mMonsterTargetPos)
    {
        auto it = mMonsterById.find(pair.first);
        if (it == mMonsterById.end()) continue;

        Monster* m = it->second;
        XMFLOAT3 current = m->GetPosition();
        XMFLOAT3 target = pair.second;

        XMFLOAT3 newPos =
        {
            current.x + (target.x - current.x) * t,
            current.y + (target.y - current.y) * t,
            current.z + (target.z - current.z) * t
        };

        m->SetPosition(newPos.x, newPos.y, newPos.z);
        m->GameObject::Update();
    }

    mCombatSystem.Update(gt, pPlayer, mMonsterPtrs);
    mPickupSystem.Update(gt, pPlayer, activeMap, mMonsterPtrs);
}

void Stage1Scene::Draw(const GameTimer& gt)
{
    UNREFERENCED_PARAMETER(gt);
    mChatController.Draw();
}

void Stage1Scene::DrawOverlay()
{
}

void Stage1Scene::BuildInteractiveDoors()
{
    auto* res = mGame->GetResources();
    auto* dev = mGame->GetDevice();
    auto* cmd = mGame->GetCommandList();
    auto& ritems = mGame->GetRitems();

    if (res->GetTexture("Wood_metal_albedo") == nullptr)
    {
        res->LoadTexture("Wood_metal_albedo", L"Models/Stage1Map/Textures/Wood_metal_albedo.dds");
    }

    if (res->GetMaterial("Stage1DoorMat") == nullptr)
    {
        res->CreateMaterial(
            "Stage1DoorMat",
            static_cast<int>(res->mMaterials.size()),
            "Wood_metal_albedo",
            "Wood_metal_normal",
            "",
            "Wood_metal_metallic",
            { 1.0f, 1.0f, 1.0f, 1.0f },
            { 0.08f, 0.08f, 0.08f },
            0.55f);
    }

    MapMeshData doorData;
    if (!ModelLoader::Load("Models/Stage1Map/Stage1_door.fbx", doorData) || doorData.Vertices.empty())
    {
        OutputDebugStringA("[Stage1] Failed to load Stage1_door.fbx\n");
        return;
    }

    auto doorGeo = std::make_unique<MeshGeometry>();
    doorGeo->Name = "stage1DoorGeo";

    const UINT vbByteSize = static_cast<UINT>(doorData.Vertices.size() * sizeof(Vertex));
    const UINT ibByteSize = static_cast<UINT>(doorData.Indices.size() * sizeof(std::uint32_t));

    D3DCreateBlob(vbByteSize, &doorGeo->VertexBufferCPU);
    CopyMemory(doorGeo->VertexBufferCPU->GetBufferPointer(), doorData.Vertices.data(), vbByteSize);
    D3DCreateBlob(ibByteSize, &doorGeo->IndexBufferCPU);
    CopyMemory(doorGeo->IndexBufferCPU->GetBufferPointer(), doorData.Indices.data(), ibByteSize);

    doorGeo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(dev, cmd, doorData.Vertices.data(), vbByteSize, doorGeo->VertexBufferUploader);
    doorGeo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(dev, cmd, doorData.Indices.data(), ibByteSize, doorGeo->IndexBufferUploader);
    doorGeo->VertexByteStride = sizeof(Vertex);
    doorGeo->VertexBufferByteSize = vbByteSize;
    doorGeo->IndexFormat = DXGI_FORMAT_R32_UINT;
    doorGeo->IndexBufferByteSize = ibByteSize;

    for (const auto& subset : doorData.Subsets)
    {
        SubmeshGeometry submesh;
        submesh.IndexCount = subset.IndexCount;
        submesh.StartIndexLocation = subset.IndexStart;
        submesh.BaseVertexLocation = 0;
        doorGeo->DrawArgs["subset_" + std::to_string(subset.Id)] = submesh;
    }

    res->mGeometries[doorGeo->Name] = std::move(doorGeo);

    std::vector<RenderItem*> doorRenderItems;
    std::vector<GameObject*> doorObjects;
    for (const auto& subset : doorData.Subsets)
    {
        auto ritem = std::make_unique<RenderItem>();
        ritem->World = MathHelper::Identity4x4();
        ritem->TexTransform = MathHelper::Identity4x4();
        ritem->ObjCBIndex = static_cast<UINT>(ritems.size());
        ritem->Mat = res->GetMaterial("Stage1DoorMat");
        ritem->Geo = res->mGeometries["stage1DoorGeo"].get();
        ritem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

        const std::string subsetName = "subset_" + std::to_string(subset.Id);
        const auto& drawArgs = ritem->Geo->DrawArgs[subsetName];
        ritem->IndexCount = drawArgs.IndexCount;
        ritem->StartIndexLocation = drawArgs.StartIndexLocation;
        ritem->BaseVertexLocation = drawArgs.BaseVertexLocation;
        ritem->Visible = true;

        doorRenderItems.push_back(ritem.get());
        mRealWorldRitems.push_back(ritem.get());
        auto doorObject = std::make_unique<GameObject>();
        doorObject->Ritem = ritem.get();
        doorObjects.push_back(doorObject.get());

        TrackOwned(doorObject.get(), ritem.get());
        ritems.push_back(std::move(ritem));
        mGame->GetGameObjects().push_back(std::move(doorObject));
    }

    MapMeshData doorColliderData;
    DoorBounds collisionBounds = CalculateBounds(doorData.Vertices, kStage1MapScale);
    if (ModelLoader::Load("Models/Stage1Map/Door_Collider.fbx", doorColliderData) && !doorColliderData.Vertices.empty())
    {
        collisionBounds = CalculateBounds(doorColliderData.Vertices, kStage1MapScale);
    }
    else
    {
        OutputDebugStringA("[Stage1] Door_Collider.fbx missing, using visual bounds as collider\n");
    }

    auto door = std::make_unique<InteractiveDoor>();
    door->Initialize(
        doorRenderItems,
        doorObjects,
        CalculateBounds(doorData.Vertices, 1.0f),
        collisionBounds,
        kStage1MapScale);
    mDoors.push_back(std::move(door));
}

void Stage1Scene::BuildMonsters()
{
    auto* res = mGame->GetResources();
    auto* device = mGame->GetDevice();
    auto* cmdList = mGame->GetCommandList();
    const DirectX::XMFLOAT3 spawnPosition = ScaleStage1Position(5.0f, 1.0f, 5.0f);

    // 1. RenderItem 생성
    auto ri = std::make_unique<RenderItem>();
    ri->ObjCBIndex = static_cast<UINT>(mGame->GetRitems().size());

    // 2. Monster 로직 클래스 생성
    auto monster = std::make_unique<Monster>(MonsterType::REAL_SKELETON_SWORD);
    monster->Initialize(ri.get(), spawnPosition);

    CharacterVisualSpec visualSpec;
    visualSpec.SpawnPosition = spawnPosition;
    visualSpec.FallbackMaterialName = "MonsterRed";
    visualSpec.FallbackScale = { 0.2f, 0.5f, 0.2f };

    CharacterVisualFactory::ApplyVisual(
        monster.get(),
        ri.get(),
        device,
        cmdList,
        res,
        visualSpec);

    monster->Update(GameTimer(), mGame->GetPlayer(), mRealMapSystem.get());
    
    // ← 추가: ID로 빠르게 찾을 수 있도록 등록
    // 서버의 InitMonsters()에서 monsterId = 1로 설정했으므로 1 사용
    mMonsterById[1] = monster.get();

    // 4. 엔진 전역 리스트에 등록 (소유권 이전)
    // RenderItem 등록
    TrackOwned(monster.get(), ri.get());
    mGame->GetRitems().push_back(std::move(ri));
    mMonsterPtrs.push_back(monster.get());
    mGame->GetGameObjects().push_back(std::move(monster));
}

void Stage1Scene::BuildAnimatedTestActor()
{
    auto* device = mGame->GetDevice();
    auto* cmdList = mGame->GetCommandList();
    auto* resources = mGame->GetResources();
    auto& ritems = mGame->GetRitems();
    auto& objs = mGame->GetGameObjects();

    auto renderItem = std::make_unique<RenderItem>();
    renderItem->World = MathHelper::Identity4x4();
    renderItem->TexTransform = MathHelper::Identity4x4();
    renderItem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    renderItem->ObjCBIndex = static_cast<UINT>(ritems.size());

    auto object = std::make_unique<GameObject>();
    CharacterVisualSpec visualSpec;
    visualSpec.UseSkinned = true;
    visualSpec.ModelPath = "Models/Animated/Female_Walking_naked.fbx";
    visualSpec.DefaultClipName = "FemaleWalk";
    visualSpec.GeometryName = "animatedFemaleWalkGeo";
    visualSpec.MaterialName = "AnimatedDebugMat";
    visualSpec.DiffuseTextureName = "AnimatedFemaleBody";
    visualSpec.DiffuseTexturePath = L"Textures/P09_Female_Body_Bright_Diff.dds";
    visualSpec.DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
    visualSpec.FresnelR0 = { 0.06f, 0.06f, 0.06f };
    visualSpec.Roughness = 0.65f;
    visualSpec.FallbackMaterialName = "PlayerBlue";
    visualSpec.FallbackScale = { 0.3f, 0.5f, 0.3f };
    visualSpec.SpawnPosition = ScaleStage1Position(2.5f, 0.8f, 5.0f);
    visualSpec.TargetHeight = 1.8f;

    if (!CharacterVisualFactory::ApplyVisual(
        object.get(),
        renderItem.get(),
        device,
        cmdList,
        resources,
        visualSpec))
    {
        OutputDebugStringA("[Stage1] Animated test actor skipped: visual factory failed\n");
        return;
    }

    TrackOwned(object.get(), renderItem.get());
    ritems.push_back(std::move(renderItem));
    objs.push_back(std::move(object));

    OutputDebugStringA("[Stage1] Animated test actor created\n");
}

// 이거 추가했다!!!!!!!!!!!!!<---------------------------------------- 서버싸개
void Stage1Scene::UpdateMonstersFromServer()
{
    auto* nm = NetworkManager::Get();

    std::lock_guard<std::mutex> lock(nm->m_monsterMutex);

    for (auto& pair : nm->m_remoteMonsters)
    {
        int id = pair.first;
        PKT_S_MONSTER_SYNC& data = pair.second;

        auto it = mMonsterById.find(id);
        if (it == mMonsterById.end()) continue;

        // 직접 위치 적용 대신 목표 위치만 저장
        mMonsterTargetPos[id] = { data.x, data.y, data.z };

        // 회전은 바로 적용해도 끊겨 보이지 않음
        it->second->SetRotation(0.0f, data.rotY * (3.14159265f / 180.0f), 0.0f);
    }

    for (auto& pair : nm->m_remoteMonsterHits)
    {
        int id = pair.first;
        PKT_S_MONSTER_HIT& data = pair.second;

        auto it = mMonsterById.find(id);
        if (it == mMonsterById.end()) continue;

        it->second->ApplyServerHit(data.remainHp, data.isDead);
        if (data.isDead)
        {
            mMonsterTargetPos.erase(id);
        }
    }
}

void Stage1Scene::OnCharInput(WPARAM charCode)
{
    mChatController.OnCharInput(charCode);
}

void Stage1Scene::OnTextInput(const std::wstring& text)
{
    mChatController.OnTextInput(text);
}

void Stage1Scene::OnCompositionInput(const std::wstring& text, bool isFinal)
{
    mChatController.OnCompositionInput(text, isFinal);
}
