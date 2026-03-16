#include "NetworkManager.h"
#include <iostream>

NetworkManager::NetworkManager() : m_socket(INVALID_SOCKET), m_isConnected(false), m_isRunning(false)
{
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        OutputDebugStringA("WSAStartup Failed!\n");
    }
}

NetworkManager::~NetworkManager()
{
    Disconnect();
    WSACleanup();
}

void NetworkManager::ConnectAsync(const std::string& ip, short port)
{
    if (m_isConnected) return; // 이미 연결되어 있으면 무시
    if (m_recvThread.joinable())
    {
        m_recvThread.join();
    }

    // 접속과 수신을 동시에 처리할 스레드를 안전하게 새로 생성
    m_isRunning = true;
    m_recvThread = std::thread(&NetworkManager::RecvLoop, this, ip, port);
}

void NetworkManager::Disconnect()
{
    m_isConnected = false;
    m_isRunning = false;

    // 소켓 닫기
    if (m_socket != INVALID_SOCKET)
    {
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
    }

    // 돌아가고 있는 수신 스레드가 완전히 종료될 때까지 기다림
    if (m_recvThread.joinable())
    {
        m_recvThread.join();
    }
}

// ==========================================
// [수신 전용 백그라운드 스레드] (서버 접속부터 패킷 수신까지 한 방에 처리)
// ==========================================
void NetworkManager::RecvLoop(std::string ip, short port)
{
    // 1. 소켓 생성
    m_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_socket == INVALID_SOCKET) return;

    // 2. 서버 주소 설정
    sockaddr_in serverAddr = {};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &serverAddr.sin_addr);

    OutputDebugStringA("[Network] Connecting to server...\n");

    // 3. 연결 시도
    if (connect(m_socket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
    {
        OutputDebugStringA("[Network] Connect Failed (Offline Mode)\n");
        closesocket(m_socket);
        m_isRunning = false;
        return;
    }

    m_isConnected = true;
    OutputDebugStringA("[Network] Connect Success\n");

    // 4. 무한 수신 루프 (패킷 쪼개짐/뭉침 완벽 방어)
    char recvBuffer[8192];
    int recvOffset = 0;

    while (m_isRunning)
    {
        int bytes = recv(m_socket, recvBuffer + recvOffset, sizeof(recvBuffer) - recvOffset, 0);

        if (bytes <= 0) // 서버와 연결이 끊김
        {
            OutputDebugStringA("[Network] Disconnected from server.\n");
            m_isConnected = false;
            break;
        }

        recvOffset += bytes;
        int readPos = 0;

        // 버퍼에 쌓인 데이터 중, 완성된 패킷만 빼서 큐에 넣기
        while (readPos + sizeof(PacketHeader) <= recvOffset)
        {
            PacketHeader* header = (PacketHeader*)(recvBuffer + readPos);

            // 헤더에 적힌 패킷 전체 크기만큼 데이터가 다 도착했는지 확인
            if (readPos + header->size <= recvOffset)
            {
                // 완전한 패킷 하나를 잘라냄
                std::vector<char> packetData(recvBuffer + readPos, recvBuffer + readPos + header->size);

                // 자물쇠를 걸고 안전하게 큐에 집어넣음
                {
                    std::lock_guard<std::mutex> lock(m_packetMutex);
                    m_packetQueue.push(packetData);
                }

                readPos += header->size;
            }
            else
            {
                // 아직 패킷이 덜 왔으면 다음 recv()를 기다림
                break;
            }
        }

        // 처리하고 남은 자투리 데이터를 버퍼 맨 앞으로 당겨놓음
        int remain = recvOffset - readPos;
        if (remain > 0)
        {
            memmove(recvBuffer, recvBuffer + readPos, remain);
        }
        recvOffset = remain;
    }

    // 루프가 끝났으면 소켓을 닫아줌
    closesocket(m_socket);
    m_socket = INVALID_SOCKET;
}

// ==========================================
// [메인 스레드용 함수] 큐에 쌓인 패킷들을 꺼내서 게임에 적용
// (매 프레임 Update 함수 등에서 호출해야 함)
// ==========================================
void NetworkManager::ProcessPackets()
{
    std::lock_guard<std::mutex> lock(m_packetMutex);

    while (!m_packetQueue.empty())
    {
        std::vector<char>& data = m_packetQueue.front();
        PacketHeader* header = (PacketHeader*)data.data();

        // 꺼낸 패킷의 ID에 따라 행동 분기
        switch (header->id)
        {
        case S_LOGIN:
        {
            PKT_S_LOGIN* res = (PKT_S_LOGIN*)data.data();
            if (res->success)
                OutputDebugStringA("[Client] Login Success!\n");
            else
                OutputDebugStringA("[Client] Login Failed!\n");
            break;
        }
        case S_CHAT:
        {
            // PKT_S_CHAT* res = (PKT_S_CHAT*)data.data();
            // 채팅 처리...
            break;
        }
        default:
            OutputDebugStringA("[Client] Unknown Packet!\n");
            break;
        }

        m_packetQueue.pop();
    }
}

// ==========================================
// 송신 함수들 (Client -> Server)
// ==========================================
void NetworkManager::SendPacket(void* packet, int size)
{
    if (!m_isConnected) return;
    send(m_socket, (char*)packet, size, 0);
}

void NetworkManager::SendLogin(const std::string& id, const std::string& pw)
{
    PKT_C_LOGIN pkt;
    pkt.header.size = sizeof(PKT_C_LOGIN);
    pkt.header.id = C_LOGIN;

    strcpy_s(pkt.id, id.c_str());
    strcpy_s(pkt.password, pw.c_str());

    SendPacket(&pkt, sizeof(PKT_C_LOGIN));
}

void NetworkManager::SendPlayerMove(float x, float y, float z, float rotY)
{
    PKT_C_PLAYER_MOVE pkt;
    pkt.header.size = sizeof(PKT_C_PLAYER_MOVE);
    pkt.header.id = C_PLAYER_MOVE;

    pkt.x = x;
    pkt.y = y;
    pkt.z = z;
    pkt.rotY = rotY;

    SendPacket(&pkt, sizeof(PKT_C_PLAYER_MOVE));
}

void NetworkManager::SendPlayerAttack(int targetId)
{
    PKT_C_PLAYER_ATTACK pkt;
    pkt.header.size = sizeof(PKT_C_PLAYER_ATTACK);
    pkt.header.id = C_PLAYER_ATTACK;
    pkt.targetId = targetId;

    SendPacket(&pkt, sizeof(PKT_C_PLAYER_ATTACK));
}