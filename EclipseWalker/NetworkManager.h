#pragma once
#include <string>
#include <WS2tcpip.h>
#include <WinSock2.h>
#include <thread>
#include <atomic>
#include <queue> 
#include <mutex>  
#include <vector> 
#include "Protocol.h"


class NetworkManager
{
public:
    static NetworkManager* Get()
    {
        static NetworkManager instance;
        return &instance;
    }

    // ★ 동료가 추가한 함수들 적용
    void ConnectAsync(const std::string& ip, short port);
    void Disconnect();
    void ProcessPackets(); // 메인 프레임에서 쌓인 패킷을 처리할 함수

    // 송신 함수
    void SendPacket(void* packet, int size);
    void SendLogin(const std::string& id, const std::string& pw);
    void SendPlayerMove(float x, float y, float z, float rotY);

private:
    NetworkManager() : m_socket(INVALID_SOCKET), m_isConnected(false), m_isRunning(false) {}
    ~NetworkManager() { Disconnect(); }

    void RecvLoop();

private:
    SOCKET m_socket;
    bool m_isConnected;

    std::thread m_recvThread;
    std::atomic<bool> m_isRunning;

 
    std::queue<std::vector<char>> m_packetQueue;
    std::mutex m_queueMutex; // 큐를 동시에 건드리지 못하게 하는 자물쇠
};