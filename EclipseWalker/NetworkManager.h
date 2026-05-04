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
#include <array>
#include "Protocol.h"

struct ChatMessage
{
    int playerId = -1;
    std::string text;
};

struct LobbyPlayerInfo
{
    int playerId = -1;
    bool connected = false;
    bool ready = false;
    bool isHost = false;
};

struct LobbyStateSnapshot
{
    int selfPlayerId = -1;
    int hostPlayerId = -1;
    int playerCount = 0;
    bool canStart = false;
    std::array<LobbyPlayerInfo, MAX_LOBBY_PLAYERS> players{};
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
    void SendGameStart();
    void SendPlayerReady(bool ready);
    std::vector<ChatMessage> PopChatMessages();
    LobbyStateSnapshot GetLobbyState();
    bool ConsumeGameStartSignal();

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
    void ApplyRoomInfo(const PKT_S_ROOM_INFO& roomInfo);
    void RebuildLobbyStateMetadata();

private:
    SOCKET m_socket;
    bool   m_isConnected;

    std::thread       m_recvThread;
    std::atomic<bool> m_isRunning;

    std::queue<std::vector<char>> m_packetQueue;
    std::mutex m_queueMutex;
    std::deque<ChatMessage> m_chatMessages;
    std::mutex m_chatMutex;
    LobbyStateSnapshot m_lobbyState;
    std::mutex m_lobbyMutex;
    std::atomic<bool> m_pendingGameStart = false;
};
