#pragma once
#include <string>
#include <thread>
#include <atomic>
#include <mutex>   
#include <queue>  
#include <vector> 

#include <WinSock2.h>
#include <WS2tcpip.h>
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
    NetworkManager();
    ~NetworkManager();

    void ConnectAsync(const std::string& ip, short port);
    void Disconnect();

    void SendPlayerMove(float x, float y, float z, float rotY);
    void SendPlayerAttack(int targetId);

    void ProcessPackets();

    bool IsConnected() const { return m_isConnected; }

private:
    void SendPacket(void* packet, int size);
    void NetworkTask(std::string ip, short port);

private:
    SOCKET m_socket;
    std::atomic<bool> m_isConnected;
    std::thread m_netThread;

    std::mutex m_packetMutex;
    std::queue<std::vector<char>> m_packetQueue;

    bool Connect(const std::string& ip, short port);
    void Disconnect();

    void SendPacket(void* packet, int size);
    void SendPlayerMove(float x, float y, float z, float rotY);
    void SendPlayerAttack(int targetId);

    void SendLogin(const std::string& id, const std::string& pw);

private:
    // 데이터를 계속 받을 스레드 함수
    void RecvLoop();

private:
    SOCKET m_socket;
    bool m_isConnected;

    std::thread m_recvThread;      // 수신 전담 스레드
    std::atomic<bool> m_isRunning; // 스레드를 안전하게 종료하기 위한 깃발(Flag)

};