#include "NetworkManager.h"
#include <iostream>

#pragma comment(lib, "ws2_32.lib")

void NetworkManager::ConnectAsync(const std::string& ip, short port)
{
    std::thread([this, ip, port]() {
        m_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (m_socket == INVALID_SOCKET) return;

        sockaddr_in serverAddr = {};
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(port);
        inet_pton(AF_INET, ip.c_str(), &serverAddr.sin_addr);

        if (connect(m_socket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
        {
            OutputDebugStringA("[Client] Connect Failed\n");
            closesocket(m_socket);
            return;
        }

        m_isConnected = true;
        m_isRunning = true;

        // 연결 성공 시 수신 스레드 시작
        m_recvThread = std::thread(&NetworkManager::RecvLoop, this);
        OutputDebugStringA("[Client] Connect Success\n");

        }).detach(); // 메인 스레드와 분리해서 백그라운드로 던짐
}

void NetworkManager::Disconnect()
{
    m_isRunning = false;
    if (m_socket != INVALID_SOCKET)
    {
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
    }
    m_isConnected = false;

    if (m_recvThread.joinable())
    {
        m_recvThread.join();
    }
}

void NetworkManager::RecvLoop()
{
    char buffer[4096];

    while (m_isRunning)
    {
        int len = recv(m_socket, buffer, sizeof(buffer), 0);
        if (len <= 0) break;

        int offset = 0;
        while (offset < len)
        {
            PacketHeader* header = (PacketHeader*)(buffer + offset);
            int packetSize = header->size;

            // 패킷 하나를 통째로 복사
            std::vector<char> packetData(buffer + offset, buffer + offset + packetSize);

            {
                std::lock_guard<std::mutex> lock(m_queueMutex);
                m_packetQueue.push(packetData);
            }

            offset += packetSize;
        }
    }
    OutputDebugStringA("[Client] 서버와 연결 끊김\n");
}

void NetworkManager::ProcessPackets()
{
    while (true)
    {
        std::vector<char> packetData;

        // 자물쇠를 걸고 큐에서 패킷을 하나 꺼냄
        {
            std::lock_guard<std::mutex> lock(m_queueMutex);
            if (m_packetQueue.empty()) break; // 큐가 비어있으면 종료

            packetData = m_packetQueue.front();
            m_packetQueue.pop();
        }

        PacketHeader* header = (PacketHeader*)packetData.data();
        switch (header->id)
        {
        case S_LOGIN:
        {
            PKT_S_LOGIN* res = (PKT_S_LOGIN*)packetData.data();
            if (res->success) OutputDebugStringA("[Client] 로그인 성공!\n");
            break;
        }
        case S_PLAYER_MOVE:
        {
            PKT_S_PLAYER_MOVE* res = (PKT_S_PLAYER_MOVE*)packetData.data();
            
            break;
        }
        }
    }
}

// 송신 함수들 (이전과 동일)
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