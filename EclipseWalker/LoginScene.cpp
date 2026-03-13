#include "LoginScene.h"
#include "EclipseWalkerGame.h"
#include "MainMenuScene.h" 
#include "GameObject.h"

void LoginScene::Enter()
{
    OutputDebugStringA("[Login Scene] 진입: 타이틀 화면 로드 중...\n");

    auto res = mGame->GetResources();
    auto& ritems = mGame->GetRitems();

    auto& gameObjects = mGame->GetGameObjects();

    ritems.clear();
    gameObjects.clear();

    auto camera = mGame->GetCamera(); 
    camera->SetPosition(0.0f, 0.0f, -10.0f);
    camera->LookAt(
        DirectX::XMFLOAT3(0.0f, 0.0f, -10.0f), 
        DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f),  
        DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f)   
    );
    camera->UpdateViewMatrix();

    // =========================================================
    // 2. 타이틀 배치 
    // =========================================================
    auto titleRitem = std::make_unique<RenderItem>();

    titleRitem->TexTransform = MathHelper::Identity4x4();
    titleRitem->ObjCBIndex = 0;

    titleRitem->NumFramesDirty = 3;

    titleRitem->Mat = res->GetMaterial("TitleMat");
    titleRitem->Geo = res->mGeometries["quadGeo"].get();
    titleRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    titleRitem->IndexCount = titleRitem->Geo->DrawArgs["quad"].IndexCount;
    titleRitem->StartIndexLocation = titleRitem->Geo->DrawArgs["quad"].StartIndexLocation;
    titleRitem->BaseVertexLocation = titleRitem->Geo->DrawArgs["quad"].BaseVertexLocation;

    auto titleObj = std::make_unique<GameObject>();
    titleObj->Ritem = titleRitem.get();

    titleObj->SetScale(8.0f, 4.5f, 1.0f);
    titleObj->SetPosition(0.0f, 0.0f, 0.0f);

    ritems.push_back(std::move(titleRitem));
    gameObjects.push_back(std::move(titleObj));
    mGame->BuildDescriptorHeaps();
}

void LoginScene::Exit() 
{
    auto& ritems = mGame->GetRitems();
    auto& gameObjects = mGame->GetGameObjects(); 

    ritems.clear();
    gameObjects.clear();
}

void LoginScene::Update(const GameTimer& gt)
{
    if (GetAsyncKeyState(VK_RETURN) & 0x8000)
    {
        if (GetTickCount64() - gLastSceneChangeTime > 500)
        {
            gLastSceneChangeTime = GetTickCount64(); 
            mGame->ChangeScene(std::make_unique<MainMenuScene>(mGame));
        }
    }
}

void LoginScene::Draw(const GameTimer& gt)
{
    
}