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

bool NetworkManager::Connect(const std::string& ip, short port)
{
    // TCP 소켓 생성
    m_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_socket == INVALID_SOCKET) return false;

    // 접속할 서버 주소 설정
    sockaddr_in serverAddr = {};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &serverAddr.sin_addr);

    // 서버로 연결 시도
    if (connect(m_socket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
    {
        OutputDebugStringA("Connect Failed\n");
        closesocket(m_socket);
        return false;
    }

    m_isConnected = true;
    OutputDebugStringA("Connect Success\n");
    return true;
}

void NetworkManager::Disconnect()
{
    if (m_socket != INVALID_SOCKET)
    {
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
    }
    m_isConnected = false;
}

// ==========================================
// [패킷 조립 및 전송 구현부]
// ==========================================

void NetworkManager::SendPacket(void* packet, int size)
{
    if (!m_isConnected) return;
    send(m_socket, (char*)packet, size, 0);
}

void NetworkManager::SendPlayerMove(float x, float y, float z, float rotY)
{
    CS_PlayerMove pkt;
    pkt.header.size = sizeof(CS_PlayerMove);
    pkt.header.type = PacketType::CS_PLAYER_MOVE;

    pkt.x = x;
    pkt.y = y;
    pkt.z = z;
    pkt.rotY = rotY;

    SendPacket(&pkt, sizeof(CS_PlayerMove));
}

void NetworkManager::SendPlayerAttack(int targetId)
{
    CS_PlayerAttack pkt;
    pkt.header.size = sizeof(CS_PlayerAttack);
    pkt.header.type = PacketType::CS_PLAYER_ATTACK;
    pkt.targetId = targetId;

    SendPacket(&pkt, sizeof(CS_PlayerAttack));
}