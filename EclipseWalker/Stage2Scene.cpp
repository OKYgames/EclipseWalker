#include "Stage2Scene.h"
#include "EclipseWalkerGame.h"
#include "MainMenuScene.h" 

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
            auto ritem = std::make_unique<RenderItem>();
            ritem->World = MathHelper::Identity4x4();
            ritem->TexTransform = MathHelper::Identity4x4();
            ritem->Geo = res->mGeometries[geoName].get();
            std::string subsetName = "subset_" + std::to_string(subset.Id);
            ritem->IndexCount = ritem->Geo->DrawArgs[subsetName].IndexCount;
            ritem->BaseVertexLocation = ritem->Geo->DrawArgs[subsetName].BaseVertexLocation;
            ritem->StartIndexLocation = ritem->Geo->DrawArgs[subsetName].StartIndexLocation;
            ritem->Mat = res->GetMaterial("Mat_" + std::to_string(subset.MaterialIndex));

            ritem->ObjCBIndex = ritems.size();
            ritem->Visible = isVisible;

            auto mapObj = std::make_unique<GameObject>();
            mapObj->SetScale(0.01f, 0.01f, 0.01f);
            mapObj->Ritem = ritem.get(); mapObj->Update();
            ritems.push_back(std::move(ritem)); objs.push_back(std::move(mapObj));
        }
        };
    CreateMapEnv("Models/Stage2Map/Stage2Map.fbx", "stage2MapGeo", true);

    mMapSystem = std::make_unique<MapSystem>();

    mMapSystem->LoadFloorCollider("Models/Stage2Map/FloorCollider.fbx", 0.01f);
    //mMapSystem->LoadWallCollider("Models/Stage2Map/Stage2Map.fbx", 0.01f);

    mGame->BuildDescriptorHeaps();
}

void Stage2Scene::Exit()
{
    OutputDebugStringA("\n[Stage 2] 종료. 메모리 해제.\n");
    // (여기에 Stage 2 맵 리소스 해제 코드 작성)
}

void Stage2Scene::Update(const GameTimer& gt)
{
    Player* pPlayer = mGame->GetPlayer();
    if (pPlayer)
    {  
        pPlayer->Update(gt, mMapSystem.get());
    }
    // Stage 2 클리어 시 (임시로 Enter 키 사용) 메인 메뉴로 돌아감
    if (GetAsyncKeyState(VK_RETURN) & 0x8000)
    {
        mGame->ChangeScene(std::make_unique<MainMenuScene>(mGame));
    }
}

void Stage2Scene::Draw(const GameTimer& gt) {}