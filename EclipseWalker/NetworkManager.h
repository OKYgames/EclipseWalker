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

    int m_myPlayerId = -1; // ??罹먮┃??ID
    std::unordered_map<int, PKT_S_PLAYER_MOVE> m_remotePlayers; // ?ㅻⅨ ?좎??ㅼ쓽 理쒖떊 ?꾩튂 蹂닿???
private:
    NetworkManager() : m_socket(INVALID_SOCKET), m_isConnected(false), m_isRunning(false) 
    {
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData); // ?ㅽ듃?뚰겕 ?ъ슜 ?좉퀬
    }

    ~NetworkManager() 
    { 
        Disconnect(); 
        WSACleanup(); // ?ㅽ듃?뚰겕 ?ъ슜 醫낅즺
    }

    void RecvLoop();

private:
    SOCKET m_socket;
    bool m_isConnected;

    std::thread m_recvThread;
    std::atomic<bool> m_isRunning;

 
    std::queue<std::vector<char>> m_packetQueue;
    std::mutex m_queueMutex; // ?먮? ?숈떆??嫄대뱶由ъ? 紐삵븯寃??섎뒗 ?먮Ъ??
};