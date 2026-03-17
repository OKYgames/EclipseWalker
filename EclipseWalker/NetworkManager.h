#pragma once
#include <string>
#include <WinSock2.h>
#include <thread>  
#include <atomic>  
#include "Protocol.h"

class NetworkManager
{
public:
    
    static NetworkManager* Get()
    {
        static NetworkManager instance;
        return &instance;
    }

    bool Connect(const std::string& ip, short port);
    void Disconnect();

    void SendPacket(void* packet, int size);
    void SendLogin(const std::string& id, const std::string& pw);
    void SendPlayerMove(float x, float y, float z, float rotY);
    void SendPlayerAttack(int targetId);

private:
    NetworkManager() : m_socket(INVALID_SOCKET), m_isConnected(false), m_isRunning(false) {}
    ~NetworkManager() { Disconnect(); }

    void RecvLoop();

private:
    SOCKET m_socket;
    bool m_isConnected;

    std::thread m_recvThread;      // 수신 전담 스레드
    std::atomic<bool> m_isRunning; // 스레드 동작 여부 플래그
};