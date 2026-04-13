#include "Stage1Scene.h"
#include "Stage2Scene.h"
#include "EclipseWalkerGame.h"
#include <Windows.h>
#include <algorithm>

Stage1Scene::Stage1Scene(EclipseWalkerGame* game)
    : Scene(game)
    , mChatController(game)
    , mWorldStateController(game)
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
    res->LoadTexture("MagicCircle", L"Textures/MagicCircle.dds");

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
    auto CreateMapEnv = [&](const std::string& fbxPath, const std::string& geoName, std::vector<RenderItem*>& targetList, bool isVisible) {
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
            ritem->ObjCBIndex = ritems.size();

            //맵의 현재 가시성(Visible) 설정
            ritem->Visible = isVisible;

            targetList.push_back(ritem.get());

            auto mapObj = std::make_unique<GameObject>();
            mapObj->SetScale(0.01f, 0.01f, 0.01f);
            mapObj->Ritem = ritem.get(); mapObj->Update();
            TrackOwned(mapObj.get(), ritem.get());
            ritems.push_back(std::move(ritem)); objs.push_back(std::move(mapObj));
        }
        };

    // 현실 맵 로드 (처음엔 보이게 true)
    CreateMapEnv("Models/Stage1Map/RealMap.fbx", "realMapGeo", mRealWorldRitems, true);
    // 이면 맵 로드 (처음엔 안 보이게 false)
    CreateMapEnv("Models/Stage1Map/OtherMap.fbx", "otherMapGeo", mOtherWorldRitems, false);

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
    mGame->CreateFire(2.0f, -3.10f, -26.0f, 0.3f); 
    mGame->CreateFire(-0.01f, -0.58f, 9.0f, 0.3f); 
    mGame->CreateFire(3.99f, -0.58f, 9.0f, 0.3f);

    auto skyRitem = std::make_unique<RenderItem>();
    DirectX::XMStoreFloat4x4(&skyRitem->World, XMMatrixScaling(5000.0f, 5000.0f, 5000.0f));
    skyRitem->TexTransform = MathHelper::Identity4x4(); skyRitem->ObjCBIndex = ritems.size();
    skyRitem->Mat = res->GetMaterial("Mat_0"); skyRitem->Geo = res->mGeometries["boxGeo"].get();
    skyRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    auto& drawArgs = skyRitem->Geo->DrawArgs["box"];
    skyRitem->IndexCount = drawArgs.IndexCount; skyRitem->StartIndexLocation = drawArgs.StartIndexLocation; skyRitem->BaseVertexLocation = drawArgs.BaseVertexLocation;
    skyRitem->Visible = true;
    TrackOwned(nullptr, skyRitem.get());
    ritems.push_back(std::move(skyRitem));

    auto domainRi = std::make_unique<RenderItem>();
    domainRi->ObjCBIndex = (UINT)ritems.size();
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

    BuildMonsters();
    mGame->BuildDescriptorHeaps();
    mChatController.Initialize();
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
    mWorldStateController.Reset();
    mRealWorldRitems.clear();
    mOtherWorldRitems.clear();
    mMonsterPtrs.clear();
    mMonsterTargetPos.clear();
    mMonsterById.clear();
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
    mWorldStateController.Update(gt, pPlayer, mChatController.IsChatting());

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
    pPlayer->ApplyPhysics(gt, activeMap);

    // 몬스터들도 현재 맵 지형 위를 걷도록 업데이트
    for (auto* m : mMonsterPtrs)
    {
        m->Update(gt, pPlayer, activeMap);
    }
    UpdateMonstersFromServer(); // 여기부터 밑에 만졌다 !!!!!!!!!!!!<--------------------------------
    // 걍 이건 AI 딸깍 한거임 감안해주셈
    // ← 여기 추가: 매 프레임 목표 위치로 부드럽게 보간
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
}

void Stage1Scene::Draw(const GameTimer& gt)
{
    UNREFERENCED_PARAMETER(gt);
    mChatController.Draw();
}

void Stage1Scene::DrawOverlay()
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
