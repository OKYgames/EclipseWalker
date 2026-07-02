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
    std::string senderName;
    std::string text;
};

struct LobbyPlayerInfo
{
    int playerId = -1;
    std::string displayName;
    bool connected = false;
    bool ready = false;
    bool isHost = false;
};

struct LobbyStateSnapshot
{
    int roomId = 0;
    std::string roomTitle;
    int selfPlayerId = -1;
    int hostPlayerId = -1;
    int playerCount = 0;
    bool canStart = false;
    std::array<LobbyPlayerInfo, MAX_LOBBY_PLAYERS> players{};
};

struct RoomListItem
{
    int roomId = 0;
    int playerCount = 0;
    int maxPlayers = MAX_LOBBY_PLAYERS;
    bool inGame = false;
    std::string title;
};

struct PlayerAttackOrientedHitbox
{
    float centerX = 0.0f;
    float centerY = 0.0f;
    float centerZ = 0.0f;
    float extentX = 0.0f;
    float extentY = 0.0f;
    float extentZ = 0.0f;
    float orientationX = 0.0f;
    float orientationY = 0.0f;
    float orientationZ = 0.0f;
    float orientationW = 1.0f;
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
    void ProcessPackets(int maxPackets);

    void SendPacket(void* packet, int size);
    void SendLogin(const std::string& id, const std::string& pw);
    void SendRegister(const std::string& id, const std::string& pw);
    void SendRoomListRequest();
    void SendCreateRoom(const std::string& title);
    void SendJoinRoom(int roomId);
    void SendLeaveRoom();
    void SendPlayerMove(float x, float y, float z, float rotY, int animationState, int classType, int playerLevel);
    void SendChat(const std::string& message);
    void SendGameStart();
    void SendPlayerReady(bool ready);
    void SendPlayerAttackCast(int skillType, int classType, int playerLevel, int targetMonsterId, float x, float y, float z, float rotY, float visualRange = 0.0f, float visualDelay = 0.0f);
    void SendPlayerAttack(
        int skillType,
        int classType,
        int playerLevel,
        int targetMonsterId,
        float x,
        float y,
        float z,
        float rotY,
        float range,
        float radius,
        float coneDot,
        const PlayerAttackOrientedHitbox* orientedHitbox = nullptr);
    void SendLanternGauge(float gauge, float maxGauge, int level);
    void SendWorldShift();
    void SendDoorInteract(int doorId, bool isOpen);
    void SendPickupCollect(int pickupId);
    void SendStageChange(int targetStage);
    void SendPlayerRespawn();
    void ClearMonsterState();
    void ClearMonsterHitState();
    std::vector<ChatMessage> PopChatMessages();
    std::vector<PKT_S_PLAYER_ATTACK> PopRemotePlayerAttacks();
    std::vector<PKT_S_PLAYER_HIT> PopPlayerHits();
    std::vector<PKT_S_PLAYER_RESPAWN> PopPlayerRespawns();
    std::vector<PKT_S_BOSS_PATTERN> PopBossPatterns();
    std::vector<PKT_S_LANTERN_GAUGE> PopLanternGaugeUpdates();
    std::vector<PKT_S_DOOR_STATE> PopDoorStates();
    std::vector<PKT_S_PICKUP_COLLECTED> PopPickupCollected();
    std::vector<PKT_S_GAME_RESULT> PopGameResults();
    LobbyStateSnapshot GetLobbyState();
    std::vector<RoomListItem> GetRoomListSnapshot();
    int GetLocalPlayerSlotIndex();
    bool ConsumeGameStartSignal();
    bool ConsumeWorldShiftSignal();
    int ConsumeStageChangeSignal();
    float ConsumeStageElapsedSeconds();
    int ConsumeGameResultSignal();
    int ConsumeLoginResult();
    int ConsumeRegisterResult();
    int ConsumeCreateRoomResult();
    int ConsumeJoinRoomResult();
    int ConsumeLeaveRoomResult();
    bool IsConnected() const;
    std::string GetMyDisplayName() const;

    int m_myPlayerId = -1;

    // 다른 플레이어 위치 데이터
    std::unordered_map<int, PKT_S_PLAYER_MOVE>   m_remotePlayers;

    // 몬스터 동기화 데이터 ← 추가
    std::unordered_map<int, PKT_S_MONSTER_SYNC>  m_remoteMonsters;
    std::unordered_map<int, PKT_S_MONSTER_HIT>   m_remoteMonsterHits;
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
    std::atomic<bool> m_isConnected;

    std::thread       m_recvThread;
    std::atomic<bool> m_isRunning;

    std::queue<std::vector<char>> m_packetQueue;
    std::mutex m_queueMutex;
    std::deque<ChatMessage> m_chatMessages;
    std::mutex m_chatMutex;
    std::deque<PKT_S_PLAYER_ATTACK> m_remotePlayerAttacks;
    std::mutex m_remoteAttackMutex;
    std::deque<PKT_S_PLAYER_HIT> m_playerHits;
    std::mutex m_playerHitMutex;
    std::deque<PKT_S_PLAYER_RESPAWN> m_playerRespawns;
    std::mutex m_playerRespawnMutex;
    std::deque<PKT_S_BOSS_PATTERN> m_bossPatterns;
    std::mutex m_bossPatternMutex;
    std::deque<PKT_S_LANTERN_GAUGE> m_lanternGaugeUpdates;
    std::mutex m_lanternGaugeMutex;
    std::deque<PKT_S_DOOR_STATE> m_doorStates;
    std::mutex m_doorStateMutex;
    std::deque<PKT_S_PICKUP_COLLECTED> m_pickupCollected;
    std::mutex m_pickupCollectedMutex;
    std::deque<PKT_S_GAME_RESULT> m_gameResults;
    std::mutex m_gameResultMutex;
    LobbyStateSnapshot m_lobbyState;
    std::mutex m_lobbyMutex;
    std::vector<RoomListItem> m_roomList;
    std::mutex m_roomListMutex;
    std::string m_myDisplayName;
    std::atomic<int> m_loginResult = 0;
    std::atomic<int> m_registerResult = 0;
    std::atomic<int> m_createRoomResult = 0;
    std::atomic<int> m_joinRoomResult = 0;
    std::atomic<int> m_leaveRoomResult = 0;
    std::atomic<bool> m_pendingGameStart = false;
    std::atomic<bool> m_pendingWorldShift = false;
    std::atomic<int> m_pendingStageChange = 0;
    std::atomic<float> m_pendingStageElapsedSeconds = 0.0f;
    std::atomic<int> m_pendingGameResult = 0;
};
