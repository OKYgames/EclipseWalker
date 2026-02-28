#include "CharSelectScene.h"
#include "EclipseWalkerGame.h"
#include "Stage1Scene.h" // 다음 씬

void CharSelectScene::Enter()
{
    OutputDebugStringA("\n[Character Select Scene] 진입: 캐릭터 렌더링 시작!\n");

    // 1. 여기서 플레이어 로드
    mGame->LoadSharedGameResources();

    // 2. 새로운 텍스처(플레이어/불꽃)가 추가되었으니 힙을 갱신합니다.
    mGame->BuildDescriptorHeaps();

    // 3. 카메라 위치를 캐릭터를 정면에서 예쁘게 바라보도록 강제 조정
    mGame->GetCamera()->SetPosition(0.0f, 1.5f, -3.0f);
}

void CharSelectScene::Exit() {}

void CharSelectScene::Update(const GameTimer& gt)
{
    if (GetAsyncKeyState(VK_RETURN) & 0x8000)
    {
        if (GetTickCount64() - gLastSceneChangeTime > 500)
        {
            gLastSceneChangeTime = GetTickCount64();
            mGame->ChangeScene(std::make_unique<Stage1Scene>(mGame));
        }
    }
}

void CharSelectScene::Draw(const GameTimer& gt)
{
    
}