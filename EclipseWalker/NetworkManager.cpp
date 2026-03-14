#include "NetworkManager.h"
#include <iostream>

NetworkManager::NetworkManager() : m_socket(INVALID_SOCKET), m_isConnected(false)
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

// ==========================================
// [비동기 접속 시작] 
// ==========================================
void NetworkManager::ConnectAsync(const std::string& ip, short port)
{
    // 스레드를 생성하여 백그라운드에서 접속 및 수신 루프를 돌림
    m_netThread = std::thread(&NetworkManager::NetworkTask, this, ip, port);
    m_netThread.detach();
}

// ==========================================
// [네트워크 전용 백그라운드 루프] (스레드)
// ==========================================
void NetworkManager::NetworkTask(std::string ip, short port)
{
    // TCP 소켓 생성
    m_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_socket == INVALID_SOCKET) return;

    // 접속할 서버 주소 설정
    sockaddr_in serverAddr = {};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &serverAddr.sin_addr);

    OutputDebugStringA("[Network] Connecting to server...\n");

    // 서버로 연결 시도
    if (connect(m_socket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
    {
        OutputDebugStringA("[Network] Connect Failed (Offline Mode)\n");
        closesocket(m_socket);
        return;
    }

    m_isConnected = true;
<<<<<<< Updated upstream
    OutputDebugStringA("[Network] Connect Success\n");

    // TCP 패킷 조립용 버퍼
    char recvBuffer[8192];
    int recvOffset = 0;

    // 무한 수신 루프
    while (m_isConnected)
    {
        int bytes = recv(m_socket, recvBuffer + recvOffset, sizeof(recvBuffer) - recvOffset, 0);

        // 서버 연결 끊김
        if (bytes <= 0)
        {
            OutputDebugStringA("[Network] Disconnected from server.\n");
            Disconnect();
            break;
        }

        recvOffset += bytes;
        int readPos = 0;

        // TCP 패킷 쪼개짐/뭉침 방지: 헤더의 size만큼 정확히 잘라서 큐에 넣음
        while (readPos + sizeof(PacketHeader) <= recvOffset)
        {
            PacketHeader* header = (PacketHeader*)(recvBuffer + readPos);

            if (readPos + header->size <= recvOffset)
            {
                std::vector<char> packetData(recvBuffer + readPos, recvBuffer + readPos + header->size);

                {
                    std::lock_guard<std::mutex> lock(m_packetMutex); 
                    m_packetQueue.push(packetData); 
                }

                readPos += header->size;
            }
            else
            {
                break; 
            }
        }

        // 처리하고 남은 자투리를 버퍼 앞으로 당김
        int remain = recvOffset - readPos;
        if (remain > 0)
        {
            memmove(recvBuffer, recvBuffer + readPos, remain);
        }
        recvOffset = remain;
    }
}

// ==========================================
// [메인 스레드에서 주기적으로 호출하는 처리기]
// ==========================================
void NetworkManager::ProcessPackets()
{
    std::lock_guard<std::mutex> lock(m_packetMutex); 

    while (!m_packetQueue.empty())
    {
        std::vector<char>& data = m_packetQueue.front();
        PacketHeader* header = (PacketHeader*)data.data();

        //이곳에 서버에서 받은 패킷들을 게임 로직에 적용하는 코드를 추가
        // switch (header->type) { case ... }

        m_packetQueue.pop(); 
    }
=======
    m_isRunning = true;
    m_recvThread = std::thread(&NetworkManager::RecvLoop, this);

    OutputDebugStringA("Connect Success\n");
    return true;
>>>>>>> Stashed changes
}

void NetworkManager::Disconnect()
{
<<<<<<< Updated upstream
    m_isConnected = false;
=======
    m_isRunning = false;

>>>>>>> Stashed changes
    if (m_socket != INVALID_SOCKET)
    {
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
    }
<<<<<<< Updated upstream
}

// ==========================================
// [패킷 전송 구현부]
// ==========================================
=======
    m_isConnected = false;

    if (m_recvThread.joinable()) {
        m_recvThread.join();
    }
}
>>>>>>> Stashed changes

void NetworkManager::SendPacket(void* packet, int size)
{
    if (!m_isConnected) return;
    send(m_socket, (char*)packet, size, 0);
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
    PKT_C_PLAYER_ATTACK pkt; // 구조체 이름 변경
    pkt.header.size = sizeof(PKT_C_PLAYER_ATTACK);
    pkt.header.id = C_PLAYER_ATTACK; // 변수명 type을 id로 변경, enum 값 변경
    pkt.targetId = targetId;

    SendPacket(&pkt, sizeof(PKT_C_PLAYER_ATTACK));
}

void NetworkManager::RecvLoop()
{
    char buffer[4096]; // 패킷을 담을 바구니

    while (m_isRunning)
    {
        int len = recv(m_socket, buffer, sizeof(buffer), 0);

        if (len <= 0)
        {
            OutputDebugStringA("[Client] 서버와 연결이 끊어졌습니다.\n");
            break;
        }

        PacketHeader* header = (PacketHeader*)buffer;

        // 서버가 보낸 패킷 ID에 따라 분기 처리
        switch (header->id)
        {
        case S_LOGIN:
        {
            PKT_S_LOGIN* res = (PKT_S_LOGIN*)buffer;
            if (res->success)
            {
                OutputDebugStringA("[Client] 로그인 성공! 내 UID: ");
        
            }
            else
            {
                OutputDebugStringA("[Client] 로그인 실패! 아이디나 비번이 틀렸습니다.\n");
            }
            break;
        }
        case S_CHAT:
        {
            PKT_S_CHAT* res = (PKT_S_CHAT*)buffer;
        
            break;
        }
        default:
            OutputDebugStringA("[Client] 알 수 없는 패킷 수신!\n");
            break;
        }
    }
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