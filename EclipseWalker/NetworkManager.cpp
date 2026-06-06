#include "NetworkManager.h"
#include "DebugConfig.h"
#include <iostream>
#include <cstring>
#include <algorithm>

#pragma comment(lib, "ws2_32.lib")

void NetworkManager::ApplyRoomInfo(const PKT_S_ROOM_INFO& roomInfo)
{
    std::lock_guard<std::mutex> lock(m_lobbyMutex);

    m_lobbyState.playerCount = roomInfo.playerCount;
    for (auto& player : m_lobbyState.players)
    {
        player = {};
    }

    for (int i = 0; i < MAX_LOBBY_PLAYERS; ++i)
    {
        const int playerId = roomInfo.playerIds[i];
        if (playerId <= 0)
        {
            continue;
        }

        m_lobbyState.players[i].playerId = playerId;
        m_lobbyState.players[i].connected = true;
        m_lobbyState.players[i].ready = roomInfo.readyStates[i];
    }

    RebuildLobbyStateMetadata();
}

void NetworkManager::RebuildLobbyStateMetadata()
{
    int connectedCount = 0;
    int hostPlayerId = -1;

    for (auto& player : m_lobbyState.players)
    {
        player.isHost = false;
        if (!player.connected || player.playerId <= 0)
        {
            player.playerId = -1;
            player.connected = false;
            player.ready = false;
            continue;
        }

        ++connectedCount;
        if (hostPlayerId == -1)
        {
            hostPlayerId = player.playerId;
            player.isHost = true;
        }
    }

    m_lobbyState.playerCount = connectedCount;
    m_lobbyState.hostPlayerId = hostPlayerId;
    m_lobbyState.selfPlayerId = m_myPlayerId;
    m_lobbyState.canStart = false;
}

void NetworkManager::ConnectAsync(const std::string& ip, short port)
{
    std::thread([this, ip, port]() {
        m_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (m_socket == INVALID_SOCKET) return;

        sockaddr_in serverAddr = {};
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(port);
        inet_pton(AF_INET, ip.c_str(), &serverAddr.sin_addr);

        if (connect(m_socket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
        {
            OutputDebugStringA("[Client] Connect Failed\n");
            closesocket(m_socket);
            return;
        }

        m_isConnected.store(true);
        m_isRunning = true;

        m_recvThread = std::thread(&NetworkManager::RecvLoop, this);
        OutputDebugStringA("[Client] Connect Success\n");

        if (!DebugConfig::kEnableDbLogin)
        {
            OutputDebugStringA("[Debug] DB login disabled. Sending debug login packet.\n");
            SendLogin(DebugConfig::kDebugLoginId, DebugConfig::kDebugLoginPassword);
        }

        }).detach();
}

void NetworkManager::Disconnect()
{
    m_isRunning = false;
    if (m_socket != INVALID_SOCKET)
    {
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
    }
    m_isConnected.store(false);

    if (m_recvThread.joinable())
        m_recvThread.join();
}

void NetworkManager::RecvLoop()
{
    char buffer[4096];

    while (m_isRunning)
    {
        int len = recv(m_socket, buffer, sizeof(buffer), 0);
        if (len <= 0) break;

        int offset = 0;
        while (offset < len)
        {
            PacketHeader* header = (PacketHeader*)(buffer + offset);
            int packetSize = header->size;

            std::vector<char> packetData(buffer + offset, buffer + offset + packetSize);
            {
                std::lock_guard<std::mutex> lock(m_queueMutex);
                m_packetQueue.push(packetData);
            }

            offset += packetSize;
        }
    }
    m_isConnected.store(false);
    OutputDebugStringA("[Client] 서버 연결 끊김\n");
}

void NetworkManager::ProcessPackets(int maxPackets)
{
    int processedPackets = 0;
    while (maxPackets <= 0 || processedPackets < maxPackets)
    {
        std::vector<char> packetData;
        {
            std::lock_guard<std::mutex> lock(m_queueMutex);
            if (m_packetQueue.empty()) break;
            packetData = m_packetQueue.front();
            m_packetQueue.pop();
        }

        ++processedPackets;
        PacketHeader* header = (PacketHeader*)packetData.data();
        switch (header->id)
        {
        case S_LOGIN:
        {
            PKT_S_LOGIN* res = (PKT_S_LOGIN*)packetData.data();
            if (res->success)
            {
                OutputDebugStringA("[Client] 로그인 성공!\n");
                m_myPlayerId = res->myPlayerId;
                std::lock_guard<std::mutex> lock(m_lobbyMutex);
                m_lobbyState.selfPlayerId = m_myPlayerId;
                if (m_lobbyState.playerCount == 0 && m_myPlayerId > 0)
                {
                    m_lobbyState.hostPlayerId = m_myPlayerId;
                    m_lobbyState.playerCount = 1;
                    m_lobbyState.players[0].playerId = m_myPlayerId;
                    m_lobbyState.players[0].connected = true;
                    m_lobbyState.players[0].ready = false;
                    m_lobbyState.players[0].isHost = true;
                }
                m_loginResult = 1;
            }
            else
            {
                OutputDebugStringA("[Client] Login failed\n");
                m_myPlayerId = -1;
                m_loginResult = -1;
            }
            break;
        }

        case S_PLAYER_MOVE:
        {
            PKT_S_PLAYER_MOVE* res = (PKT_S_PLAYER_MOVE*)packetData.data();

            // m_myPlayerId가 -1이면 필터링 생략 (DB 로그인 전 임시)
            if (m_myPlayerId != -1 && res->playerId == m_myPlayerId) break;

            m_remotePlayers[res->playerId] = *res;
            break;
        }

        case S_PLAYER_ATTACK:
        {
            PKT_S_PLAYER_ATTACK* res = (PKT_S_PLAYER_ATTACK*)packetData.data();
            if (m_myPlayerId != -1 && res->playerId == m_myPlayerId) break;

            std::lock_guard<std::mutex> lock(m_remoteAttackMutex);
            m_remotePlayerAttacks.push_back(*res);
            while (m_remotePlayerAttacks.size() > 32)
            {
                m_remotePlayerAttacks.pop_front();
            }
            break;
        }

        case S_CHAT:
        {
            PKT_S_CHAT* res = (PKT_S_CHAT*)packetData.data();
            if (m_myPlayerId != -1 && res->playerId == m_myPlayerId) break;

            ChatMessage chatMessage;
            chatMessage.playerId = res->playerId;
            chatMessage.senderName = res->senderName;
            chatMessage.text = res->msg;

            std::lock_guard<std::mutex> lock(m_chatMutex);
            m_chatMessages.push_back(chatMessage);
            while (m_chatMessages.size() > 32)
            {
                m_chatMessages.pop_front();
            }
            break;
        }

        case S_ROOM_INFO:
        {
            PKT_S_ROOM_INFO* res = (PKT_S_ROOM_INFO*)packetData.data();
            ApplyRoomInfo(*res);
            break;
        }

        case S_PLAYER_ENTER:
        {
            PKT_S_PLAYER_ENTER* res = (PKT_S_PLAYER_ENTER*)packetData.data();
            std::lock_guard<std::mutex> lock(m_lobbyMutex);

            bool exists = false;
            for (const auto& player : m_lobbyState.players)
            {
                if (player.connected && player.playerId == res->playerId)
                {
                    exists = true;
                    break;
                }
            }

            if (!exists)
            {
                for (auto& player : m_lobbyState.players)
                {
                    if (!player.connected)
                    {
                        player.playerId = res->playerId;
                        player.connected = true;
                        break;
                    }
                }
            }

            RebuildLobbyStateMetadata();
            break;
        }

        case S_PLAYER_LEAVE:
        {
            PKT_S_PLAYER_LEAVE* res = (PKT_S_PLAYER_LEAVE*)packetData.data();
            std::lock_guard<std::mutex> lock(m_lobbyMutex);

            for (auto& player : m_lobbyState.players)
            {
                if (player.connected && player.playerId == res->playerId)
                {
                    player = {};
                    break;
                }
            }

            RebuildLobbyStateMetadata();
            break;
        }

        case S_GAME_START:
        {
            m_pendingGameStart = true;
            break;
        }

        case S_WORLD_SHIFT:
        {
            PKT_S_WORLD_SHIFT* res = (PKT_S_WORLD_SHIFT*)packetData.data();
            (void)res;
            OutputDebugStringA("[Client] Received server world shift\n");
            m_pendingWorldShift = true;
            break;
        }

        case S_STAGE_CHANGE:
        {
            PKT_S_STAGE_CHANGE* res = (PKT_S_STAGE_CHANGE*)packetData.data();
            m_pendingStageChange = res->targetStage;
            break;
        }

        // ← 추가: 몬스터 동기화 패킷 처리
        case S_DOOR_STATE:
        {
            PKT_S_DOOR_STATE* res = (PKT_S_DOOR_STATE*)packetData.data();
            std::lock_guard<std::mutex> lock(m_doorStateMutex);
            m_doorStates.push_back(*res);
            while (m_doorStates.size() > 32)
            {
                m_doorStates.pop_front();
            }
            break;
        }

        case S_PICKUP_COLLECTED:
        {
            PKT_S_PICKUP_COLLECTED* res = (PKT_S_PICKUP_COLLECTED*)packetData.data();
            std::lock_guard<std::mutex> lock(m_pickupCollectedMutex);
            m_pickupCollected.push_back(*res);
            while (m_pickupCollected.size() > 64)
            {
                m_pickupCollected.pop_front();
            }
            break;
        }

        case S_MONSTER_SYNC:
        {
            PKT_S_MONSTER_SYNC* res = (PKT_S_MONSTER_SYNC*)packetData.data();
            {
                std::lock_guard<std::mutex> lock(m_monsterMutex);
                m_remoteMonsters[res->monsterId] = *res;
            }
            break;
        }

        case S_MONSTER_HIT:
        {
            PKT_S_MONSTER_HIT* res = (PKT_S_MONSTER_HIT*)packetData.data();
            {
                std::lock_guard<std::mutex> lock(m_monsterMutex);
                m_remoteMonsterHits[res->monsterId] = *res;
            }
            break;
        }

        case S_PLAYER_HIT:
        {
            PKT_S_PLAYER_HIT* res = (PKT_S_PLAYER_HIT*)packetData.data();
            std::lock_guard<std::mutex> lock(m_playerHitMutex);
            m_playerHits.push_back(*res);
            while (m_playerHits.size() > 32)
            {
                m_playerHits.pop_front();
            }
            break;
        }

        case S_PLAYER_RESPAWN:
        {
            PKT_S_PLAYER_RESPAWN* res = (PKT_S_PLAYER_RESPAWN*)packetData.data();
            if (m_myPlayerId <= 0 || res->playerId != m_myPlayerId)
            {
                PKT_S_PLAYER_MOVE movePkt = {};
                movePkt.header.size = sizeof(PKT_S_PLAYER_MOVE);
                movePkt.header.id = S_PLAYER_MOVE;
                movePkt.playerId = res->playerId;
                movePkt.x = res->x;
                movePkt.y = res->y;
                movePkt.z = res->z;
                movePkt.rotY = 0.0f;
                movePkt.animationState = 0;
                auto classIt = m_remotePlayers.find(res->playerId);
                movePkt.classType = (classIt != m_remotePlayers.end()) ? classIt->second.classType : 1;
                m_remotePlayers[res->playerId] = movePkt;
            }

            std::lock_guard<std::mutex> lock(m_playerRespawnMutex);
            m_playerRespawns.push_back(*res);
            while (m_playerRespawns.size() > 16)
            {
                m_playerRespawns.pop_front();
            }
            break;
        }

        case S_BOSS_PATTERN:
        {
            PKT_S_BOSS_PATTERN* res = (PKT_S_BOSS_PATTERN*)packetData.data();
            std::lock_guard<std::mutex> lock(m_bossPatternMutex);
            m_bossPatterns.push_back(*res);
            while (m_bossPatterns.size() > 16)
            {
                m_bossPatterns.pop_front();
            }
            break;
        }

        case S_LANTERN_GAUGE:
        {
            PKT_S_LANTERN_GAUGE* res = (PKT_S_LANTERN_GAUGE*)packetData.data();
            std::lock_guard<std::mutex> lock(m_lanternGaugeMutex);
            m_lanternGaugeUpdates.push_back(*res);
            while (m_lanternGaugeUpdates.size() > 16)
            {
                m_lanternGaugeUpdates.pop_front();
            }
            break;
        }
        }
    }
}

void NetworkManager::SendPacket(void* packet, int size)
{
    if (!m_isConnected.load()) return;
    send(m_socket, (char*)packet, size, 0);
}

void NetworkManager::SendLogin(const std::string& id, const std::string& pw)
{
    m_loginResult = 0;
    m_myDisplayName = id;

    PKT_C_LOGIN pkt;
    pkt.header.size = sizeof(PKT_C_LOGIN);
    pkt.header.id = C_LOGIN;
    strcpy_s(pkt.id, id.c_str());
    strcpy_s(pkt.password, pw.c_str());
    SendPacket(&pkt, sizeof(PKT_C_LOGIN));
}

void NetworkManager::SendPlayerMove(float x, float y, float z, float rotY, int animationState, int classType)
{
    PKT_C_PLAYER_MOVE pkt;
    pkt.header.size = sizeof(PKT_C_PLAYER_MOVE);
    pkt.header.id = C_PLAYER_MOVE;
    pkt.x = x;
    pkt.y = y;
    pkt.z = z;
    pkt.rotY = rotY;
    pkt.animationState = animationState;
    pkt.classType = classType;
    SendPacket(&pkt, sizeof(PKT_C_PLAYER_MOVE));
}

void NetworkManager::SendChat(const std::string& message)
{
    PKT_C_CHAT pkt = {};
    pkt.header.size = sizeof(PKT_C_CHAT);
    pkt.header.id = C_CHAT;

    strncpy_s(pkt.msg, message.c_str(), _TRUNCATE);
    SendPacket(&pkt, sizeof(PKT_C_CHAT));
}

void NetworkManager::SendGameStart()
{
    PKT_C_GAME_START pkt = {};
    pkt.header.size = sizeof(PKT_C_GAME_START);
    pkt.header.id = C_GAME_START;
    SendPacket(&pkt, sizeof(PKT_C_GAME_START));
}

void NetworkManager::SendPlayerReady(bool ready)
{
    PKT_C_PLAYER_READY pkt = {};
    pkt.header.size = sizeof(PKT_C_PLAYER_READY);
    pkt.header.id = C_PLAYER_READY;
    pkt.ready = ready;
    SendPacket(&pkt, sizeof(PKT_C_PLAYER_READY));
}

void NetworkManager::SendPlayerAttack(int skillType, float x, float y, float z, float rotY, float range, float radius, float coneDot)
{
    PKT_C_PLAYER_ATTACK pkt = {};
    pkt.header.size = sizeof(PKT_C_PLAYER_ATTACK);
    pkt.header.id = C_PLAYER_ATTACK;
    pkt.attackerId = m_myPlayerId;
    pkt.x = x;
    pkt.y = y;
    pkt.z = z;
    pkt.rotY = rotY;
    pkt.skillType = skillType;
    pkt.range = range;
    pkt.radius = radius;
    pkt.coneDot = coneDot;
    SendPacket(&pkt, sizeof(PKT_C_PLAYER_ATTACK));
}

void NetworkManager::SendLanternGauge(float gauge, float maxGauge, int level)
{
    PKT_C_LANTERN_GAUGE pkt = {};
    pkt.header.size = sizeof(PKT_C_LANTERN_GAUGE);
    pkt.header.id = C_LANTERN_GAUGE;
    pkt.gauge = gauge;
    pkt.maxGauge = maxGauge;
    pkt.level = level;
    SendPacket(&pkt, sizeof(PKT_C_LANTERN_GAUGE));
}

void NetworkManager::SendWorldShift()
{
    PKT_C_WORLD_SHIFT pkt = {};
    pkt.header.size = sizeof(PKT_C_WORLD_SHIFT);
    pkt.header.id = C_WORLD_SHIFT;

    if (!m_isConnected.load())
    {
        OutputDebugStringA("[Client] Queued local world shift offline\n");
        m_pendingWorldShift = true;
        return;
    }

    SendPacket(&pkt, sizeof(PKT_C_WORLD_SHIFT));
    OutputDebugStringA("[Client] Sent world shift request\n");
}

void NetworkManager::SendDoorInteract(int doorId, bool isOpen)
{
    PKT_C_DOOR_INTERACT pkt = {};
    pkt.header.size = sizeof(PKT_C_DOOR_INTERACT);
    pkt.header.id = C_DOOR_INTERACT;
    pkt.doorId = doorId;
    pkt.isOpen = isOpen;
    SendPacket(&pkt, sizeof(PKT_C_DOOR_INTERACT));
}

void NetworkManager::SendPickupCollect(int pickupId)
{
    PKT_C_PICKUP_COLLECT pkt = {};
    pkt.header.size = sizeof(PKT_C_PICKUP_COLLECT);
    pkt.header.id = C_PICKUP_COLLECT;
    pkt.pickupId = pickupId;
    SendPacket(&pkt, sizeof(PKT_C_PICKUP_COLLECT));
}

void NetworkManager::SendStageChange(int targetStage)
{
    PKT_C_STAGE_CHANGE pkt = {};
    pkt.header.size = sizeof(PKT_C_STAGE_CHANGE);
    pkt.header.id = C_STAGE_CHANGE;
    pkt.targetStage = targetStage;

    if (!m_isConnected)
    {
        m_pendingStageChange = targetStage;
        return;
    }

    SendPacket(&pkt, sizeof(PKT_C_STAGE_CHANGE));
}

void NetworkManager::ClearMonsterState()
{
    std::lock_guard<std::mutex> lock(m_monsterMutex);
    m_remoteMonsters.clear();
    m_remoteMonsterHits.clear();
}

void NetworkManager::ClearMonsterHitState()
{
    std::lock_guard<std::mutex> lock(m_monsterMutex);
    m_remoteMonsterHits.clear();
}

std::vector<ChatMessage> NetworkManager::PopChatMessages()
{
    std::vector<ChatMessage> messages;

    std::lock_guard<std::mutex> lock(m_chatMutex);
    while (!m_chatMessages.empty())
    {
        messages.push_back(m_chatMessages.front());
        m_chatMessages.pop_front();
    }

    return messages;
}

std::vector<PKT_S_PLAYER_ATTACK> NetworkManager::PopRemotePlayerAttacks()
{
    std::vector<PKT_S_PLAYER_ATTACK> attacks;

    std::lock_guard<std::mutex> lock(m_remoteAttackMutex);
    while (!m_remotePlayerAttacks.empty())
    {
        attacks.push_back(m_remotePlayerAttacks.front());
        m_remotePlayerAttacks.pop_front();
    }

    return attacks;
}

std::vector<PKT_S_PLAYER_HIT> NetworkManager::PopPlayerHits()
{
    std::vector<PKT_S_PLAYER_HIT> hits;

    std::lock_guard<std::mutex> lock(m_playerHitMutex);
    while (!m_playerHits.empty())
    {
        hits.push_back(m_playerHits.front());
        m_playerHits.pop_front();
    }

    return hits;
}

std::vector<PKT_S_PLAYER_RESPAWN> NetworkManager::PopPlayerRespawns()
{
    std::vector<PKT_S_PLAYER_RESPAWN> respawns;

    std::lock_guard<std::mutex> lock(m_playerRespawnMutex);
    while (!m_playerRespawns.empty())
    {
        respawns.push_back(m_playerRespawns.front());
        m_playerRespawns.pop_front();
    }

    return respawns;
}

std::vector<PKT_S_BOSS_PATTERN> NetworkManager::PopBossPatterns()
{
    std::vector<PKT_S_BOSS_PATTERN> patterns;

    std::lock_guard<std::mutex> lock(m_bossPatternMutex);
    while (!m_bossPatterns.empty())
    {
        patterns.push_back(m_bossPatterns.front());
        m_bossPatterns.pop_front();
    }

    return patterns;
}

std::vector<PKT_S_LANTERN_GAUGE> NetworkManager::PopLanternGaugeUpdates()
{
    std::vector<PKT_S_LANTERN_GAUGE> updates;

    std::lock_guard<std::mutex> lock(m_lanternGaugeMutex);
    while (!m_lanternGaugeUpdates.empty())
    {
        updates.push_back(m_lanternGaugeUpdates.front());
        m_lanternGaugeUpdates.pop_front();
    }

    return updates;
}

std::vector<PKT_S_DOOR_STATE> NetworkManager::PopDoorStates()
{
    std::vector<PKT_S_DOOR_STATE> states;

    std::lock_guard<std::mutex> lock(m_doorStateMutex);
    while (!m_doorStates.empty())
    {
        states.push_back(m_doorStates.front());
        m_doorStates.pop_front();
    }

    return states;
}

std::vector<PKT_S_PICKUP_COLLECTED> NetworkManager::PopPickupCollected()
{
    std::vector<PKT_S_PICKUP_COLLECTED> pickups;

    std::lock_guard<std::mutex> lock(m_pickupCollectedMutex);
    while (!m_pickupCollected.empty())
    {
        pickups.push_back(m_pickupCollected.front());
        m_pickupCollected.pop_front();
    }

    return pickups;
}

LobbyStateSnapshot NetworkManager::GetLobbyState()
{
    std::lock_guard<std::mutex> lock(m_lobbyMutex);
    return m_lobbyState;
}

bool NetworkManager::ConsumeGameStartSignal()
{
    return m_pendingGameStart.exchange(false);
}

bool NetworkManager::ConsumeWorldShiftSignal()
{
    return m_pendingWorldShift.exchange(false);
}

int NetworkManager::ConsumeStageChangeSignal()
{
    return m_pendingStageChange.exchange(0);
}

int NetworkManager::ConsumeLoginResult()
{
    return m_loginResult.exchange(0);
}

bool NetworkManager::IsConnected() const
{
    return m_isConnected.load();
}

std::string NetworkManager::GetMyDisplayName() const
{
    if (!m_myDisplayName.empty())
    {
        return m_myDisplayName;
    }

    if (m_myPlayerId > 0)
    {
        return "Player " + std::to_string(m_myPlayerId);
    }

    return "Me";
}
