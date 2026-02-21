#include "Stage1Scene.h"
#include "EclipseWalkerGame.h"

Stage1Scene::Stage1Scene(EclipseWalkerGame* game) : Scene(game)
{
}

Stage1Scene::~Stage1Scene()
{
}

void Stage1Scene::Enter()
{
    // 1. [인게임 공통 리소스] 보장 (플레이어, 불꽃 등)
    mGame->LoadSharedGameResources();

    auto res = mGame->GetResources();
    auto dev = mGame->GetDevice();
    auto cmd = mGame->GetCommandList();
    auto& ritems = mGame->GetRitems();
    auto& objs = mGame->GetGameObjects();

    OutputDebugStringA("\n[Stage 1] 씬 전용 리소스 로딩 시작...\n");

    // 2. [Stage 1 전용 텍스처 로드]
    std::vector<std::string> texNames = ModelLoader::LoadTextureNames("Models/Map/Map.fbx");
    for (const auto& originName : texNames)
    {
        if (originName.empty()) continue;
        std::string baseName = originName.substr(0, originName.find_last_of('.'));

        auto LoadMapTex = [&](std::string suffix) {
            std::string name = baseName + suffix;
            std::wstring path = L"Models/Map/Textures/" + std::wstring(name.begin(), name.end()) + L".dds";
            if (GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES)
                res->LoadTexture(name, path);
            };
        LoadMapTex(""); LoadMapTex("_normal"); LoadMapTex("_emissive"); LoadMapTex("_metallic");
    }
    res->LoadTexture("Wood_metal_normal", L"Models/Map/Textures/Wood_metal_normal.dds");
    res->LoadTexture("Wood_metal_metallic", L"Models/Map/Textures/Wood_metal_metallic.dds");
    res->LoadTexture("sky", L"Textures/sky.dds");

    // 3. [Stage 1 전용 지오메트리 로드 (맵 메쉬)]
    MapMeshData mapData;
    ModelLoader::Load("Models/Map/Map.fbx", mapData);
    mMapSubsets = mapData.Subsets;

    const UINT vbByteSize = (UINT)mapData.Vertices.size() * sizeof(Vertex);
    const UINT ibByteSize = (UINT)mapData.Indices.size() * sizeof(std::uint32_t);
    auto mapGeo = std::make_unique<MeshGeometry>();
    mapGeo->Name = "mapGeo";

    D3DCreateBlob(vbByteSize, &mapGeo->VertexBufferCPU);
    CopyMemory(mapGeo->VertexBufferCPU->GetBufferPointer(), mapData.Vertices.data(), vbByteSize);
    D3DCreateBlob(ibByteSize, &mapGeo->IndexBufferCPU);
    CopyMemory(mapGeo->IndexBufferCPU->GetBufferPointer(), mapData.Indices.data(), ibByteSize);

    mapGeo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(dev, cmd, mapData.Vertices.data(), vbByteSize, mapGeo->VertexBufferUploader);
    mapGeo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(dev, cmd, mapData.Indices.data(), ibByteSize, mapGeo->IndexBufferUploader);
    mapGeo->VertexByteStride = sizeof(Vertex);
    mapGeo->VertexBufferByteSize = vbByteSize;
    mapGeo->IndexFormat = DXGI_FORMAT_R32_UINT;
    mapGeo->IndexBufferByteSize = ibByteSize;

    for (const auto& subset : mMapSubsets)
    {
        SubmeshGeometry submesh;
        submesh.IndexCount = subset.IndexCount;
        submesh.StartIndexLocation = subset.IndexStart;
        submesh.BaseVertexLocation = 0;
        mapGeo->DrawArgs["subset_" + std::to_string(subset.Id)] = submesh;
    }
    res->mGeometries[mapGeo->Name] = std::move(mapGeo);

    // 4. [Stage 1 전용 재질 생성]
    int mapMatCount = (int)texNames.size();
    for (int i = 0; i < mapMatCount; ++i)
    {
        std::string matName = "Mat_" + std::to_string(i);
        std::string baseName = texNames[i].empty() ? "" : texNames[i].substr(0, texNames[i].find_last_of('.'));

        std::string diffName = baseName, normName = baseName + "_normal", emName = baseName + "_emissive", metName = baseName + "_metallic";
        if (baseName == "Wood_metal_albedo") { normName = "Wood_metal_normal"; metName = "Wood_metal_metallic"; }

        int newMatCBIndex = (int)res->mMaterials.size();

        res->CreateMaterial(
            matName,
            newMatCBIndex, 
            diffName, normName, emName, metName,
            XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f),
            XMFLOAT3(0.05f, 0.05f, 0.05f),
            0.8f
        );

        if (Material* mat = res->GetMaterial(matName)) {
            mat->IsToon = 0;
            mat->IsTransparent = 0;
            mat->NumFramesDirty = 3;
        }
    }

    // 5. [Stage 1 전용 렌더 아이템 생성]
    for (const auto& subset : mMapSubsets)
    {
        auto ritem = std::make_unique<RenderItem>();
        ritem->World = MathHelper::Identity4x4();
        ritem->TexTransform = MathHelper::Identity4x4();
        ritem->Geo = res->mGeometries["mapGeo"].get();
        string subsetName = "subset_" + std::to_string(subset.Id);
        ritem->IndexCount = ritem->Geo->DrawArgs[subsetName].IndexCount;
        ritem->BaseVertexLocation = ritem->Geo->DrawArgs[subsetName].BaseVertexLocation;
        ritem->StartIndexLocation = ritem->Geo->DrawArgs[subsetName].StartIndexLocation;
        ritem->Mat = res->GetMaterial("Mat_" + std::to_string(subset.MaterialIndex));
        ritem->ObjCBIndex = ritems.size();

        auto mapObj = std::make_unique<GameObject>();
        mapObj->SetScale(0.01f, 0.01f, 0.01f);
        mapObj->Ritem = ritem.get(); mapObj->Update();

        ritems.push_back(std::move(ritem));
        objs.push_back(std::move(mapObj));
    }

    auto skyRitem = std::make_unique<RenderItem>();
    DirectX::XMStoreFloat4x4(&skyRitem->World, XMMatrixScaling(5000.0f, 5000.0f, 5000.0f));
    skyRitem->TexTransform = MathHelper::Identity4x4();
    skyRitem->ObjCBIndex = ritems.size();
    skyRitem->Mat = res->GetMaterial("Mat_0");
    skyRitem->Geo = res->mGeometries["boxGeo"].get();
    skyRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    auto& drawArgs = skyRitem->Geo->DrawArgs["box"];
    skyRitem->IndexCount = drawArgs.IndexCount;
    skyRitem->StartIndexLocation = drawArgs.StartIndexLocation;
    skyRitem->BaseVertexLocation = drawArgs.BaseVertexLocation;
    ritems.push_back(std::move(skyRitem));

    // 6. 맵 시스템 생성 및 스카이박스 인덱스 갱신
    mMapSystem = std::make_unique<MapSystem>();
    mMapSystem->Build(res->mGeometries["mapGeo"].get(), 0.01f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    mSkyTexHeapIndex = res->GetTextureIndex("sky");
}

void Stage1Scene::Exit()
{
}

void Stage1Scene::Update(const GameTimer& gt)
{
}

void Stage1Scene::Draw(const GameTimer& gt)
{
}