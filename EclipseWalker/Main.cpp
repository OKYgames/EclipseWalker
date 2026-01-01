#include "EclipseWalkerGame.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance,
    PSTR cmdLine, int showCmd)
{
    // 디버그 모드에서 메모리 누수 감지
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    try
    {
        // 게임 객체 생성
        EclipseWalkerGame theGame(hInstance);

        // 초기화
        if (!theGame.Initialize())
            return 0;

        // 게임 루프 시작
        return theGame.Run();
    }
    catch (DxException& e)
    {
        MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
        return 0;
    }
}