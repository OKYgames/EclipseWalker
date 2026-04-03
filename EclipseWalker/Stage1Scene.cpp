#include "Stage1Scene.h"
#include "Stage2Scene.h"
#include "EclipseWalkerGame.h"

Stage1Scene::Stage1Scene(EclipseWalkerGame* game) : Scene(game)
{
}

Stage1Scene::~Stage1Scene()
{
}

void Stage1Scene::Enter()
{
    // 1. [인게임 공통 리소스] 
    mGame->LoadSharedGameResources();

    auto res = mGame->GetResources();
    auto dev = mGame->GetDevice();
    auto cmd = mGame->GetCommandList();
    auto& ritems = mGame->GetRitems();
    auto& objs = mGame->GetGameObjects();

    OutputDebugStringA("\n[Stage 1] 씬 전용 리소스 로딩 시작...\n");

    // 2. [Stage 1 텍스처 및 재질 로드] 
    std::vector<std::string> texNames = ModelLoader::LoadTextureNames("Models/Stage1Map/RealMap.fbx");
    for (const auto& originName : texNames)
    {
        if (originName.empty()) continue;
        std::string baseName = originName.substr(0, originName.find_last_of('.'));

        auto LoadMapTex = [&](std::string suffix) {
            std::string name = baseName + suffix;
            std::wstring path = L"Models/Stage1Map/Textures/" + std::wstring(name.begin(), name.end()) + L".dds";
            if (GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES)
                res->LoadTexture(name, path);
            };
        LoadMapTex(""); LoadMapTex("_normal"); LoadMapTex("_emissive"); LoadMapTex("_metallic");
    }
    res->LoadTexture("Wood_metal_normal", L"Models/Stage1Map/Textures/Wood_metal_normal.dds");
    res->LoadTexture("Wood_metal_metallic", L"Models/Stage1Map/Textures/Wood_metal_metallic.dds");
    res->LoadTexture("sky", L"Textures/sky.dds");

    // 재질 생성
    int mapMatCount = (int)texNames.size();
    for (int i = 0; i < mapMatCount; ++i)
    {
        std::string matName = "Mat_" + std::to_string(i);
        std::string baseName = texNames[i].empty() ? "" : texNames[i].substr(0, texNames[i].find_last_of('.'));
        std::string diffName = baseName, normName = baseName + "_normal", emName = baseName + "_emissive", metName = baseName + "_metallic";
        if (baseName == "Wood_metal_albedo") { normName = "Wood_metal_normal"; metName = "Wood_metal_metallic"; }

        int newMatCBIndex = (int)res->mMaterials.size();
        res->CreateMaterial(matName, newMatCBIndex, diffName, normName, emName, metName, XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT3(0.05f, 0.05f, 0.05f), 0.8f);

        if (Material* mat = res->GetMaterial(matName)) {
            mat->IsToon = 0; mat->IsTransparent = 0; mat->NumFramesDirty = 3;
        }
    }

    // ====================================================================
    // 3. 맵 로드 & 렌더 아이템 생성 도우미 함수 (코드 중복 방지)
    // ====================================================================
    auto CreateMapEnv = [&](const std::string& fbxPath, const std::string& geoName, std::vector<RenderItem*>& targetList, int worldType) {
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
            auto ritem = std::make_unique<RenderItem>();
            ritem->World = MathHelper::Identity4x4();
            ritem->TexTransform = MathHelper::Identity4x4();
            ritem->Geo = res->mGeometries[geoName].get();
            string subsetName = "subset_" + std::to_string(subset.Id);
            ritem->IndexCount = ritem->Geo->DrawArgs[subsetName].IndexCount;
            ritem->BaseVertexLocation = ritem->Geo->DrawArgs[subsetName].BaseVertexLocation;
            ritem->StartIndexLocation = ritem->Geo->DrawArgs[subsetName].StartIndexLocation;
            ritem->Mat = res->GetMaterial("Mat_" + std::to_string(subset.MaterialIndex));
            ritem->WorldType = worldType;
            //ritem->Visible = true;
            ritem->ObjCBIndex = ritems.size();

            targetList.push_back(ritem.get());

            auto mapObj = std::make_unique<GameObject>();
            mapObj->SetScale(0.01f, 0.01f, 0.01f);
            mapObj->Ritem = ritem.get(); mapObj->Update();
            ritems.push_back(std::move(ritem)); objs.push_back(std::move(mapObj));
        }
        };

    CreateMapEnv("Models/Stage1Map/RealMap.fbx", "realMapGeo", mRealWorldRitems, 1);
    CreateMapEnv("Models/Stage1Map/OtherMap.fbx", "otherMapGeo", mOtherWorldRitems, 2);

    auto domainRi = std::make_unique<RenderItem>();
    domainRi->ObjCBIndex = (UINT)ritems.size(); // 현재 전체 아이템 개수를 인덱스로 할당

    // 박스 지오메트리를 사용 
    domainRi->Geo = res->mGeometries["boxGeo"].get();

    // 투명 처리가 가능한 재질(Fire_Mat 등)을 할당합니다.
    domainRi->Mat = res->GetMaterial("Fire_Mat");

    domainRi->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    // 지오메트리의 드로우 인자 설정
    auto& boxArgs = domainRi->Geo->DrawArgs["box"];
    domainRi->IndexCount = boxArgs.IndexCount;
    domainRi->StartIndexLocation = boxArgs.StartIndexLocation;
    domainRi->BaseVertexLocation = boxArgs.BaseVertexLocation;

    // GameObject 생성 및 연결
    auto domainObj = std::make_unique<GameObject>();
    domainObj->Ritem = domainRi.get();
    domainObj->SetScale(0.0f, 0.0f, 0.0f); // 초기 크기는 0으로 설정 (

    // 클래스 멤버 변수에 저장
    mDomainBoundaryObj = domainObj.get();

    // 엔진 전역 리스트에 등록
    ritems.push_back(std::move(domainRi));
    objs.push_back(std::move(domainObj));

    mRealMapSystem = std::make_unique<MapSystem>();
    mRealMapSystem->LoadFloorCollider("Models/Stage1Map/RealFloorCollider.fbx", 0.01f);
    mRealMapSystem->LoadWallCollider("Models/Stage1Map/RealWallCollider.fbx", 0.01f);

    mOtherMapSystem = std::make_unique<MapSystem>();
    mOtherMapSystem->LoadFloorCollider("Models/Stage1Map/OtherFloorCollider.fbx", 0.01f);
    mOtherMapSystem->LoadWallCollider("Models/Stage1Map/OtherWallCollider.fbx", 0.01f);

    // 스카이박스 및 파티클 세팅
    mSkyTexHeapIndex = res->GetTextureIndex("sky");
    mGame->CreateFire(-0.1f, 0.8f, 1.1f, 0.3f);
    mGame->CreateFire(4.1f, 0.8f, 1.1f, 0.3f);

    auto skyRitem = std::make_unique<RenderItem>();
    DirectX::XMStoreFloat4x4(&skyRitem->World, XMMatrixScaling(5000.0f, 5000.0f, 5000.0f));
    skyRitem->TexTransform = MathHelper::Identity4x4(); skyRitem->ObjCBIndex = ritems.size();
    skyRitem->Mat = res->GetMaterial("Mat_0"); skyRitem->Geo = res->mGeometries["boxGeo"].get();
    skyRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    auto& drawArgs = skyRitem->Geo->DrawArgs["box"];
    skyRitem->IndexCount = drawArgs.IndexCount; skyRitem->StartIndexLocation = drawArgs.StartIndexLocation; skyRitem->BaseVertexLocation = drawArgs.BaseVertexLocation;
    //skyRitem->Visible = true;
    ritems.push_back(std::move(skyRitem));

    BuildMonsters();
    mGame->BuildDescriptorHeaps();
}

void Stage1Scene::Exit()
{
    OutputDebugStringA("\n[Stage 1] 씬 종료,메모리 해제...\n");

    // 글로벌 리스트 가져오기
    auto& ritems = mGame->GetRitems();
    auto& objs = mGame->GetGameObjects();

    auto isStage1Obj = [](const std::unique_ptr<GameObject>& obj) {
        if (!obj->Ritem) return false;
        bool isMap = (obj->Ritem->Geo && obj->Ritem->Geo->Name.find("Map") != std::string::npos);
        bool isFire = (obj->Ritem->Mat && obj->Ritem->Mat->Name.find("Fire") != std::string::npos);
        bool isMonster = (obj->Ritem->Mat && obj->Ritem->Mat->Name.find("Monster") != std::string::npos);
        return isMap || isFire || isMonster;
        };

    auto isStage1Ritem = [](const std::unique_ptr<RenderItem>& ritem) {
        if (!ritem) return false;
        bool isMap = (ritem->Geo && ritem->Geo->Name.find("Map") != std::string::npos);
        bool isFire = (ritem->Mat && ritem->Mat->Name.find("Fire") != std::string::npos);
        bool isMonster = (ritem->Mat && ritem->Mat->Name.find("Monster") != std::string::npos);
        return isMap || isFire || isMonster;
        };

    objs.erase(std::remove_if(objs.begin(), objs.end(), isStage1Obj), objs.end());
    ritems.erase(std::remove_if(ritems.begin(), ritems.end(), isStage1Ritem), ritems.end());

    for (UINT i = 0; i < ritems.size(); ++i)
    {
        ritems[i]->ObjCBIndex = i;
        ritems[i]->NumFramesDirty = 3;
    }

    mGame->ResetLights();

    OutputDebugStringA("\n[Stage 1] 해제 완료\n");
}

void Stage1Scene::Update(const GameTimer& gt)
{
    // ====================================================================
    // 1. 차원 전환 스위치 (F키 입력 시 Player의 랜턴 기능 호출)
    // ====================================================================
    Player* pPlayer = mGame->GetPlayer();

    if (GetAsyncKeyState('F') & 0x8000)
    {
        if (!mFKeyPressed)
        {          
            if (pPlayer)
            {
                pPlayer->UseLantern();
            }
            mFKeyPressed = true;
        }
    }
    else
    {
        mFKeyPressed = false;
    }

    // G키 씬 전환 로직 (기존 유지)
    static bool isGPressed = false;
    if (GetAsyncKeyState('G') & 0x8000)
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
    // 2. 물리 및 로직 업데이트
    // ====================================================================
    MapSystem* activeMap = GetActiveMapSystem();

    if (pPlayer)
    {
        // 플레이어의 10초 타이머와 반경 계산 함수를 매 프레임 호출해야 합니다.
        pPlayer->UpdateLanternDomain(gt.DeltaTime());

        // 플레이어 물리 업데이트
        pPlayer->ApplyPhysics(gt, activeMap);

        // ====================================================================
        // 3D 영역 전개 구체 오브젝트 업데이트
        // ====================================================================
        if (mDomainBoundaryObj != nullptr)
        {
            if (pPlayer->IsDomainActive())
            {
                // 구체를 플레이어 위치로 옮깁니다.
                XMFLOAT3 pPos = pPlayer->GetPosition();
                mDomainBoundaryObj->SetPosition(pPos.x, pPos.y + 1.0f, pPos.z);

                // Player에서 계산된 실시간 반경(Radius)을 스케일에 적용합니다.
                float radius = pPlayer->GetDomainRadius();
                mDomainBoundaryObj->SetScale(radius, radius, radius);
            }
            else
            {
                // 영역이 꺼져있을 때는 크기를 0으로 만들어 숨깁니다.
                mDomainBoundaryObj->SetScale(0.0f, 0.0f, 0.0f);
            }
        }
    }

    // 몬스터 업데이트 
    for (auto* m : mMonsterPtrs)
    {
        m->Update(gt, pPlayer, activeMap);
    }
}

void Stage1Scene::Draw(const GameTimer& gt)
{
}

void Stage1Scene::BuildMonsters()
{
    auto res = mGame->GetResources();

    // 1. RenderItem 생성
    auto ri = std::make_unique<RenderItem>();
    ri->ObjCBIndex = (int)mGame->GetRitems().size();
    ri->Geo = res->mGeometries["boxGeo"].get();
    ri->Mat = res->GetMaterial("MonsterRed");

    // 서브메쉬 정보 반드시 설정
    auto& args = ri->Geo->DrawArgs["box"];
    ri->IndexCount = args.IndexCount;
    ri->StartIndexLocation = args.StartIndexLocation;
    ri->BaseVertexLocation = args.BaseVertexLocation;

    // 2. Monster 로직 클래스 생성
    auto monster = std::make_unique<Monster>(MonsterType::REAL_SKELETON_SWORD);


    monster->Initialize(ri.get(), XMFLOAT3(5.0f, 1.0f, 5.0f));
    monster->SetScale(0.2f, 0.5f, 0.2f);

    monster->Update(GameTimer(), mGame->GetPlayer(), mRealMapSystem.get());

    // 4. 엔진 전역 리스트에 등록 (소유권 이전)
    // RenderItem 등록
    mGame->GetRitems().push_back(std::move(ri));
    mMonsterPtrs.push_back(monster.get());
    mGame->GetGameObjects().push_back(std::move(monster));
}