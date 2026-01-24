#include "Define.h"
#include "IocpCore.h"
#include "Session.h"
#include "LogManager.h"
#include "ServerPacketHandler.h" // 패킷 핸들러 추가
#include "GlobalQueue.h"         // JobQueue 추가
#include "Room.h"

// [전역 변수]
// 잡 큐는 여기서 실제로 할당해야 함 (extern의 실체)
// GlobalQueue.cpp에서 정의했다면 여긴 필요 없지만, 
// 만약 링킹 에러가 나면 GlobalQueue.cpp에 있는 G_JobQueue = nullptr; 확인
// (보통 GlobalQueue.cpp에 정의를 두는 게 정석입니다. 여기서는 사용만 합니다.)

// 세션 관리용 컨테이너
std::vector<std::shared_ptr<Session>> G_Sessions;

class GameSession : public Session
{
public:
    virtual void OnConnected() override
    {
        LOG_INFO("Client Connected!");

        G_Room->Enter(shared_from_this());
    }

    virtual void OnDisconnected() override
    {
        LOG_WARN("Client Disconnected");

        G_Room->Leave(shared_from_this());
        // 나중에 여기서 G_Sessions에서 이 세션을 빼주는 코드가 필요함 (동기화 주의)
    }

    virtual int OnRecv(BYTE* buffer, int len) override
    {
        // [수정] 이제 여기서 직접 패킷을까지 않고, 핸들러에게 넘깁니다.
        // TCP의 특성상 패킷이 뭉치거나 잘려 올 수 있으므로 루프를 돕니다.

        int processLen = 0;

        while (true)
        {
            int dataSize = len - processLen;

            // 1. 최소한 헤더 크기만큼은 왔는지 확인
            if (dataSize < sizeof(PacketHeader))
                break;

            // 헤더 부분을 살짝 읽어봄 (패킷 전체 크기를 알기 위해)
            PacketHeader* header = (PacketHeader*)&buffer[processLen];

            // 2. 헤더에 적힌 패킷 크기만큼 데이터가 충분히 왔는지 확인
            if (dataSize < header->size)
                break;

            // 3. 패킷 조립 가능! 핸들러에게 토스
            // shared_from_this()를 통해 나 자신(Session)을 안전하게 넘김
            ServerPacketHandler::HandlePacket(shared_from_this(), &buffer[processLen], header->size);

            processLen += header->size;
        }

        return processLen; // 처리한 만큼의 길이를 리턴
    }
};

int main()
{
    // 1. 로그 매니저 초기화
    LogManager::GetInstance()->Initialize();

    // 2. [추가] 일감 큐(Job Queue) 생성
    G_JobQueue = new GlobalQueue();

    // 3. 윈속 초기화
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    // 4. IOCP 코어 초기화 (네트워크 스레드들이 여기서 생성됨)
    IocpCore iocp;
    iocp.Initialize();

    // 5. [추가] 로직 스레드(Logic Thread) 생성 (주방장 고용)
    // 이 스레드는 네트워크 통신은 안 하고, 큐에 쌓인 게임 로직만 무한히 처리함
    std::thread logicThread([]()
        {
            while (true)
            {
                // 큐에 일감이 있으면 꺼내서 실행, 없으면 대기(Sleep)
                G_JobQueue->Execute();
            }
        });
    // 메인 스레드랑 분리해서 백그라운드에서 돌게 함
    logicThread.detach();

    // 6. 리스닝 소켓 설정
    SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, 0);
    SOCKADDR_IN serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddr.sin_port = htons(7777);

    bind(listenSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr));
    listen(listenSocket, SOMAXCONN);

    LOG_INFO("Listening on Port 7777...");

    // 7. 메인 스레드는 클라이언트 접속 받는 역할(Accept)만 전담
    while (true)
    {
        SOCKADDR_IN clientAddr;
        int addrLen = sizeof(clientAddr);
        SOCKET clientSocket = accept(listenSocket, (SOCKADDR*)&clientAddr, &addrLen);

        if (clientSocket != INVALID_SOCKET)
        {
            // 새 세션 생성
            std::shared_ptr<GameSession> session = std::make_shared<GameSession>();

            // 소켓 및 IOCP 등록
            session->Init(clientSocket, clientAddr);
            iocp.Register(session);

            // 세션 관리 목록에 추가 (동기화 필요하지만 일단 단순 추가)
            G_Sessions.push_back(session);
        }
    }

    // 종료 처리는 생략 (서버는 보통 강제종료 전까진 안 꺼짐)
    WSACleanup();
}