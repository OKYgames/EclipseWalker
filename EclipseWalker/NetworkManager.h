#pragma once

#ifndef _WINSOCKAPI_
#define _WINSOCKAPI_
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

#include "Protocol.h" 
#include <string>

class NetworkManager
{
public:
    NetworkManager();
    ~NetworkManager();

    // 서버 접속 함수
    bool Connect(const std::string& ip, short port);

    // 연결 종료 함수
    void Disconnect();

    // ==========================================
    // [패킷 전송 함수들]
    // ==========================================
    void SendPlayerMove(float x, float y, float z, float rotY);
    void SendPlayerAttack(int targetId);

private:
    // 실제 패킷을 네트워크로 쏘는 내부 함수
    void SendPacket(void* packet, int size);

private:
    SOCKET m_socket;
    bool m_isConnected;
};