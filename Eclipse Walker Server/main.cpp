#include "LogManager.h"

int main()
{
    LogManager::GetInstance()->Initialize();

    LOG_INFO("서버 초기화 시작...");

    int port = 7777;
    LOG_INFO("포트 바인딩 성공: %d", port);

    // ... 에러 상황 예시임 ...
    bool isError = true;
    if (isError)
    {
        LOG_ERROR("치명적인 오류 발생! 코드: %d", 505); // 505은 예시이고 실제로 할땐 WSAGetLastError() 넣기
    }

    LOG_WARN("비정상적인 접근 감지 (IP: 127.0.0.1)");

    char packet[5] = { 0x01, 0x02, 0xFF, 0xAA, 0xBB };
    LOG_HEX("이동 패킷", packet, 5);

    LogManager::GetInstance()->Finalize();
    return 0;
}