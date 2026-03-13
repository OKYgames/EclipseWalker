#pragma once

#ifndef _WINSOCKAPI_
#define _WINSOCKAPI_
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

#include "Protocol.h" 
#include <string>
#include <thread>
#include <atomic>
#include <mutex>   
#include <queue>  
#include <vector> 

class NetworkManager
{
public:
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
};