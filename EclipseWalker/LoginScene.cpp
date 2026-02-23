#include "LoginScene.h"
#include "EclipseWalkerGame.h"
#include "MainMenuScene.h" 

void LoginScene::Enter()
{
    OutputDebugStringA("\n[Login Scene] 진입: 로그인 대기 중... (Enter 키를 누르세요)\n");
    // (나중에 여기에 ID/PW 입력창 UI 로딩 코드가 들어갑니다)
}

void LoginScene::Exit() {}

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
    // (나중에 여기에 2D 로그인 배경 이미지 그리는 코드가 들어갑니다)
}