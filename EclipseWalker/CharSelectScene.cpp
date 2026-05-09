#include "CharSelectScene.h"
#include "EclipseWalkerGame.h"
#include "Stage1Scene.h" // 다음 씬

namespace
{
    const char* ToClassName(PlayerClass playerClass)
    {
        switch (playerClass)
        {
        case PlayerClass::Warrior: return "Warrior";
        case PlayerClass::Mage: return "Mage";
        case PlayerClass::Archer: return "Archer";
        default: return "None";
        }
    }
}

void CharSelectScene::Enter()
{
    OutputDebugStringA("\n[Character Select Scene] 진입: 캐릭터 렌더링 시작!\n");

    // 1. 여기서 플레이어 로드
    mLeftKeyPressed = (GetAsyncKeyState(VK_LEFT) & 0x8000) != 0;
    mRightKeyPressed = (GetAsyncKeyState(VK_RIGHT) & 0x8000) != 0;
    mEnterKeyPressed = (GetAsyncKeyState(VK_RETURN) & 0x8000) != 0;

    mGame->LoadSharedGameResources();

    // 2. 새로운 텍스처(플레이어/불꽃)가 추가되었으니 힙을 갱신합니다.
    mGame->BuildDescriptorHeaps();

    // 3. 카메라 위치를 캐릭터를 정면에서 예쁘게 바라보도록 강제 조정
    mGame->GetCamera()->SetPosition(0.0f, 1.5f, -3.0f);

    if (mGame->GetSelectedPlayerClass() == PlayerClass::None)
    {
        mGame->SetSelectedPlayerClass(PlayerClass::Mage);
    }

    std::string debugText = "[Character Select] Current class: ";
    debugText += ToClassName(mGame->GetSelectedPlayerClass());
    debugText += "\n";
    OutputDebugStringA(debugText.c_str());
}

void CharSelectScene::Exit() {}

void CharSelectScene::Update(const GameTimer& gt)
{
    UNREFERENCED_PARAMETER(gt);

    if (GetForegroundWindow() != mGame->GetMainWindowHandle())
    {
        mLeftKeyPressed = false;
        mRightKeyPressed = false;
        mEnterKeyPressed = false;
        return;
    }

    if (GetAsyncKeyState(VK_LEFT) & 0x8000)
    {
        if (!mLeftKeyPressed)
        {
            CycleSelection(-1);
            mLeftKeyPressed = true;
        }
    }
    else
    {
        mLeftKeyPressed = false;
    }

    if (GetAsyncKeyState(VK_RIGHT) & 0x8000)
    {
        if (!mRightKeyPressed)
        {
            CycleSelection(1);
            mRightKeyPressed = true;
        }
    }
    else
    {
        mRightKeyPressed = false;
    }

    if (GetAsyncKeyState('1') & 0x8000) mGame->SetSelectedPlayerClass(PlayerClass::Warrior);
    if (GetAsyncKeyState('2') & 0x8000) mGame->SetSelectedPlayerClass(PlayerClass::Mage);
    if (GetAsyncKeyState('3') & 0x8000) mGame->SetSelectedPlayerClass(PlayerClass::Archer);

    if (GetAsyncKeyState(VK_RETURN) & 0x8000)
    {
        if (!mEnterKeyPressed && GetTickCount64() - gLastSceneChangeTime > 500)
        {
            mEnterKeyPressed = true;
            gLastSceneChangeTime = GetTickCount64();
            mGame->ChangeScene(std::make_unique<Stage1Scene>(mGame));
        }
    }
    else
    {
        mEnterKeyPressed = false;
    }
}

void CharSelectScene::Draw(const GameTimer& gt)
{
    
}

void CharSelectScene::CycleSelection(int direction)
{
    static const PlayerClass classes[] = {
        PlayerClass::Warrior,
        PlayerClass::Mage,
        PlayerClass::Archer
    };

    int currentIndex = 1;
    for (int i = 0; i < 3; ++i)
    {
        if (classes[i] == mGame->GetSelectedPlayerClass())
        {
            currentIndex = i;
            break;
        }
    }

    currentIndex = (currentIndex + direction + 3) % 3;
    mGame->SetSelectedPlayerClass(classes[currentIndex]);

    std::string debugText = "[Character Select] Current class: ";
    debugText += ToClassName(classes[currentIndex]);
    debugText += "\n";
    OutputDebugStringA(debugText.c_str());
}
