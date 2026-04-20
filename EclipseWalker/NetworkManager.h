#pragma once
#include <string>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <thread>
#include <atomic>
#include <queue>
#include <mutex>
#include <vector>
#include <deque>
#include <unordered_map>
#include "Protocol.h"

struct ChatMessage
{
    int playerId = -1;
    std::string text;
};

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
    void SendChat(const std::string& message);
    std::vector<ChatMessage> PopChatMessages();

    int m_myPlayerId = -1;

    // 다른 플레이어 위치 데이터
    std::unordered_map<int, PKT_S_PLAYER_MOVE>   m_remotePlayers;

    // 몬스터 동기화 데이터 ← 추가
    std::unordered_map<int, PKT_S_MONSTER_SYNC>  m_remoteMonsters;
    std::mutex m_monsterMutex; // 몬스터 맵 접근용 락 ← 추가

private:
    NetworkManager() : m_socket(INVALID_SOCKET), m_isConnected(false), m_isRunning(false)
    {
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
    }

    ~NetworkManager()
    {
        Disconnect();
        WSACleanup();
    }

    void RecvLoop();

private:
    SOCKET m_socket;
    bool   m_isConnected;

    std::thread       m_recvThread;
    std::atomic<bool> m_isRunning;

    std::queue<std::vector<char>> m_packetQueue;
    std::mutex m_queueMutex;
    std::deque<ChatMessage> m_chatMessages;
    std::mutex m_chatMutex;
};
