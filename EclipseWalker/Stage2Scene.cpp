#include "Stage2Scene.h"
#include "EclipseWalkerGame.h"
#include "MainMenuScene.h" 
#include <algorithm>
#include <filesystem>
#include <sstream>

namespace
{
    struct MapMaterialBinding
    {
        std::string MaterialName;
        bool HideSubset = false;
    };

    constexpr float kStage2MapScale = 0.014f;
    constexpr float kStage2WorldScale = kStage2MapScale / 0.01f;
}

void Stage2Scene::TrackOwned(GameObject* object, RenderItem* renderItem)
{
    if (object) mOwnedObjects.push_back(object);
    if (renderItem) mOwnedRenderItems.push_back(renderItem);
}

void Stage2Scene::ReleaseOwnedObjects()
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

void Stage2Scene::Enter()
{
    OutputDebugStringA("\n[Stage 2 Scene] 진입: 두 번째 스테이지 로딩!\n");

    // 공통 리소스(셰이더, UI 등) 로드
    mGame->LoadSharedGameResources();

    auto res = mGame->GetResources();
    auto dev = mGame->GetDevice();
    auto cmd = mGame->GetCommandList();
    auto& ritems = mGame->GetRitems();
    auto& objs = mGame->GetGameObjects();

    auto LoadStage2Textures = [&](const std::vector<std::string>& textureNames)
    {
        for (const auto& originName : textureNames)
        {
            if (originName.empty()) continue;

            const std::string baseName = originName.substr(0, originName.find_last_of('.'));
            auto TryLoadTexture = [&](const std::string& suffix)
            {
                const std::string textureName = baseName + suffix;
                const std::wstring texturePath =
                    L"Models/Stage2Map/Textures/" + std::wstring(textureName.begin(), textureName.end()) + L".dds";

                if (std::filesystem::exists(texturePath))
                {
                    res->LoadTexture(textureName, texturePath);
                }
            };

            TryLoadTexture("");
            TryLoadTexture("_normal");
            TryLoadTexture("_emissive");
            TryLoadTexture("_metallic");
        }
    };

    auto BuildStage2Materials = [&](const std::vector<std::string>& textureNames)
    {
        std::vector<MapMaterialBinding> materialBindings(textureNames.size());

        for (size_t i = 0; i < textureNames.size(); ++i)
        {
            const std::string& originName = textureNames[i];
            const std::string baseName = originName.empty() ? "" : originName.substr(0, originName.find_last_of('.'));
            const bool shouldHideSubset = baseName.empty() || (res->GetTexture(baseName) == nullptr);

            std::string diffuseName = baseName;
            std::string normalName = baseName.empty() ? "" : baseName + "_normal";
            std::string emissiveName = baseName.empty() ? "" : baseName + "_emissive";
            std::string metallicName = baseName.empty() ? "" : baseName + "_metallic";

            if (shouldHideSubset)
            {
                materialBindings[i].HideSubset = true;
                continue;
            }
            if (!normalName.empty() && res->GetTexture(normalName) == nullptr)
            {
                normalName.clear();
            }
            if (!emissiveName.empty() && res->GetTexture(emissiveName) == nullptr)
            {
                emissiveName.clear();
            }
            if (!metallicName.empty() && res->GetTexture(metallicName) == nullptr)
            {
                metallicName.clear();
            }

            const std::string materialName = "Stage2_Mat_" + std::to_string(i);
            materialBindings[i].MaterialName = materialName;

            if (res->GetMaterial(materialName) == nullptr)
            {
                res->CreateMaterial(
                    materialName,
                    static_cast<int>(res->mMaterials.size()),
                    diffuseName,
                    normalName,
                    emissiveName,
                    metallicName,
                    { 1.0f, 1.0f, 1.0f, 1.0f },
                    { 0.05f, 0.05f, 0.05f },
                    0.8f);
            }

            if (Material* material = res->GetMaterial(materialName))
            {
                material->DiffuseMapName = diffuseName;
                material->NormalMapName = normalName;
                material->EmissiveMapName = emissiveName;
                material->MetallicMapName = metallicName;
                material->DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
                material->FresnelR0 = { 0.05f, 0.05f, 0.05f };
                material->Roughness = 0.8f;
                material->IsToon = 0;
                material->IsTransparent = 0;
                material->NumFramesDirty = gNumFrameResources;
            }
        }

        return materialBindings;
    };

    const auto stage2TextureNames = ModelLoader::LoadTextureNames("Models/Stage2Map/Stage2Map.fbx");
    LoadStage2Textures(stage2TextureNames);
    const auto stage2MaterialBindings = BuildStage2Materials(stage2TextureNames);

    if (res->GetMaterial("Stage2AbyssCoverMat") == nullptr)
    {
        res->CreateMaterial(
            "Stage2AbyssCoverMat",
            static_cast<int>(res->mMaterials.size()),
            "white",
            "",
            "",
            "",
            { 0.16f, 0.06f, 0.07f, 1.0f },
            { 0.02f, 0.02f, 0.02f },
            1.0f);
    }

    if (Material* abyssCoverMat = res->GetMaterial("Stage2AbyssCoverMat"))
    {
        abyssCoverMat->IsToon = 0;
        abyssCoverMat->IsTransparent = 0;
        abyssCoverMat->NumFramesDirty = gNumFrameResources;
    }

    if (res->GetMaterial("Stage2AbyssFogMat") == nullptr)
    {
        res->CreateMaterial(
            "Stage2AbyssFogMat",
            static_cast<int>(res->mMaterials.size()),
            "white",
            "",
            "",
            "",
            { 0.10f, 0.09f, 0.16f, 0.16f },
            { 0.02f, 0.02f, 0.02f },
            1.0f);
    }

    if (Material* abyssFogMat = res->GetMaterial("Stage2AbyssFogMat"))
    {
        abyssFogMat->IsToon = 0;
        abyssFogMat->IsTransparent = 2;
        abyssFogMat->NumFramesDirty = gNumFrameResources;
    }

    auto CreateMapEnv = [&](const std::string& fbxPath, const std::string& geoName, bool isVisible) {
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

        for (const auto& subset : mapData.Subsets) {
            if (subset.MaterialIndex >= stage2MaterialBindings.size())
            {
                continue;
            }

            const auto& materialBinding = stage2MaterialBindings[subset.MaterialIndex];
            if (materialBinding.HideSubset)
            {
                std::ostringstream hiddenLog;
                hiddenLog << "[Stage2Scene] Hidden subset with missing diffuse texture: "
                    << subset.Name << " (material index " << subset.MaterialIndex << ")\n";
                OutputDebugStringA(hiddenLog.str().c_str());
                continue;
            }

            auto ritem = std::make_unique<RenderItem>();
            ritem->World = MathHelper::Identity4x4();
            ritem->TexTransform = MathHelper::Identity4x4();
            ritem->Geo = res->mGeometries[geoName].get();
            std::string subsetName = "subset_" + std::to_string(subset.Id);
            ritem->IndexCount = ritem->Geo->DrawArgs[subsetName].IndexCount;
            ritem->BaseVertexLocation = ritem->Geo->DrawArgs[subsetName].BaseVertexLocation;
            ritem->StartIndexLocation = ritem->Geo->DrawArgs[subsetName].StartIndexLocation;
            ritem->Mat = res->GetMaterial(materialBinding.MaterialName);

            ritem->ObjCBIndex = static_cast<UINT>(ritems.size());
            ritem->Visible = isVisible;

            auto mapObj = std::make_unique<GameObject>();
            mapObj->SetScale(kStage2MapScale, kStage2MapScale, kStage2MapScale);
            mapObj->Ritem = ritem.get(); mapObj->Update();
            TrackOwned(mapObj.get(), ritem.get());
            ritems.push_back(std::move(ritem)); objs.push_back(std::move(mapObj));
        }
        };
    CreateMapEnv("Models/Stage2Map/Stage2Map.fbx", "stage2MapGeo", true);

    auto AddAbyssCoverBox = [&](const XMFLOAT3& scale, const XMFLOAT3& position)
    {
        auto ritem = std::make_unique<RenderItem>();
        ritem->World = MathHelper::Identity4x4();
        XMStoreFloat4x4(
            &ritem->World,
            XMMatrixScaling(scale.x, scale.y, scale.z) * XMMatrixTranslation(position.x, position.y, position.z));
        ritem->TexTransform = MathHelper::Identity4x4();
        ritem->ObjCBIndex = static_cast<UINT>(ritems.size());
        ritem->Mat = res->GetMaterial("Stage2AbyssCoverMat");
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
        { 300.0f * kStage2WorldScale, 12.0f * kStage2WorldScale, 300.0f * kStage2WorldScale },
        { 0.0f, -18.0f * kStage2WorldScale, 0.0f });

    auto abyssFogRitem = std::make_unique<RenderItem>();
    abyssFogRitem->World = MathHelper::Identity4x4();
    XMStoreFloat4x4(
        &abyssFogRitem->World,
        XMMatrixScaling(210.0f * kStage2WorldScale, 18.0f * kStage2WorldScale, 210.0f * kStage2WorldScale) *
        XMMatrixTranslation(0.0f, -9.0f * kStage2WorldScale, 0.0f));
    abyssFogRitem->TexTransform = MathHelper::Identity4x4();
    abyssFogRitem->ObjCBIndex = static_cast<UINT>(ritems.size());
    abyssFogRitem->Mat = res->GetMaterial("Stage2AbyssFogMat");
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

    mMapSystem = std::make_unique<MapSystem>();

    mMapSystem->LoadFloorCollider("Models/Stage2Map/FloorCollider.fbx", kStage2MapScale);
    //mMapSystem->LoadWallCollider("Models/Stage2Map/Stage2Map.fbx", 0.01f);

    mGame->BuildDescriptorHeaps();
}

void Stage2Scene::Exit()
{
    OutputDebugStringA("\n[Stage 2] 종료. 메모리 해제.\n");
    ReleaseOwnedObjects();
}

void Stage2Scene::Update(const GameTimer& gt)
{
    Player* pPlayer = mGame->GetPlayer();
    if (pPlayer)
    {  
        pPlayer->Update(gt, mMapSystem.get());
    }
    // Stage 2 클리어 시 (임시로 Enter 키 사용) 메인 메뉴로 돌아감
    const bool hasFocus = (mGame != nullptr && GetForegroundWindow() == mGame->GetMainWindowHandle());
    if (hasFocus && (GetAsyncKeyState(VK_RETURN) & 0x8000))
    {
        mGame->ChangeScene(std::make_unique<MainMenuScene>(mGame));
    }
}

void Stage2Scene::Draw(const GameTimer& gt) {}
