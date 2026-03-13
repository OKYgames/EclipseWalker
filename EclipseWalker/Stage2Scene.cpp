#include "Stage2Scene.h"
#include "EclipseWalkerGame.h"
#include "MainMenuScene.h" 

void Stage2Scene::Enter()
{
    OutputDebugStringA("\n[Stage 2 Scene] 진입: 두 번째 스테이지 로딩!\n");

    mGame->LoadSharedGameResources(); // 이미 로드되어 있으면 무시됨(안전함)

    // (나중에 여기에 Stage2.fbx 로딩 코드를 작성합니다)
    // 지금은 Stage 1과 구분하기 위해 배경을 살짝 다르게 한다고 가정

    mGame->BuildDescriptorHeaps();
}

void Stage2Scene::Exit()
{
    OutputDebugStringA("\n[Stage 2] 종료. 메모리 해제.\n");
    // (여기에 Stage 2 맵 리소스 해제 코드 작성)
}

void Stage2Scene::Update(const GameTimer& gt)
{
    // Stage 2 클리어 시 (임시로 Enter 키 사용) 메인 메뉴로 돌아감
    if (GetAsyncKeyState(VK_RETURN) & 0x8000)
    {
        mGame->ChangeScene(std::make_unique<MainMenuScene>(mGame));
    }
}

void Stage2Scene::Draw(const GameTimer& gt) {}