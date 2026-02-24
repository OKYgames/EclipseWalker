#include "MainMenuScene.h"
#include "EclipseWalkerGame.h"
#include "CharSelectScene.h" 

void MainMenuScene::Enter()
{

    OutputDebugStringA("\n[Main Menu Scene] 진입: 게임 시작 대기 중... (Enter 키를 누르세요)\n");
}

void MainMenuScene::Exit() {}

void MainMenuScene::Update(const GameTimer& gt)
{
    if (GetAsyncKeyState(VK_RETURN) & 0x8000)
    {
        if (GetTickCount64() - gLastSceneChangeTime > 500)
        {
            gLastSceneChangeTime = GetTickCount64();
            mGame->ChangeScene(std::make_unique<CharSelectScene>(mGame));
        }
    }
}

void MainMenuScene::Draw(const GameTimer& gt) {}