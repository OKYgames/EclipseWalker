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


#include <unordered_map>

class NetworkManager
{
public:
    static NetworkManager* Get()
    {
        static NetworkManager instance;
        return &instance;
    }

    void ConnectAsync(const std::string& ip, short port);
    void Disconnect();
    void ProcessPackets();

    void SendPacket(void* packet, int size);
    void SendLogin(const std::string& id, const std::string& pw);
    void SendPlayerMove(float x, float y, float z, float rotY);

    int m_myPlayerId = -1; // 내 캐릭터 ID
    std::unordered_map<int, PKT_S_PLAYER_MOVE> m_remotePlayers; // 다른 유저들의 최신 위치 보관함
private:
    NetworkManager() : m_socket(INVALID_SOCKET), m_isConnected(false), m_isRunning(false) 
    {
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData); // 네트워크 사용 신고
    }

    ~NetworkManager() 
    { 
        Disconnect(); 
        WSACleanup(); // 네트워크 사용 종료
    }

    void RecvLoop();

private:
    SOCKET m_socket;
    bool m_isConnected;

    std::thread m_recvThread;
    std::atomic<bool> m_isRunning;

 
    std::queue<std::vector<char>> m_packetQueue;
    std::mutex m_queueMutex; // 큐를 동시에 건드리지 못하게 하는 자물쇠
};