#include "NetworkManager.h"
#include "DebugConfig.h"
#include <iostream>
#include <cstring>
#include <algorithm>

#pragma comment(lib, "ws2_32.lib")

namespace
{
    constexpr int kMaxPacketSize = 4096;

    int GetExpectedServerPacketSize(short packetId)
    {
        switch (packetId)
        {
        case S_LOGIN: return sizeof(PKT_S_LOGIN);
        case S_REGISTER: return sizeof(PKT_S_REGISTER);
        case S_ROOM_LIST: return sizeof(PKT_S_ROOM_LIST);
        case S_CREATE_ROOM: return sizeof(PKT_S_CREATE_ROOM);
        case S_JOIN_ROOM: return sizeof(PKT_S_JOIN_ROOM);
        case S_LEAVE_ROOM: return sizeof(PKT_S_LEAVE_ROOM);
        case S_PLAYER_MOVE: return sizeof(PKT_S_PLAYER_MOVE);
        case S_PLAYER_ATTACK: return sizeof(PKT_S_PLAYER_ATTACK);
        case S_CHAT: return sizeof(PKT_S_CHAT);
        case S_ROOM_INFO: return sizeof(PKT_S_ROOM_INFO);
        case S_PLAYER_ENTER: return sizeof(PKT_S_PLAYER_ENTER);
        case S_PLAYER_LEAVE: return sizeof(PKT_S_PLAYER_LEAVE);
        case S_GAME_START: return sizeof(PKT_S_GAME_START);
        case S_GAME_RESULT: return sizeof(PKT_S_GAME_RESULT);
        case S_WORLD_SHIFT: return sizeof(PKT_S_WORLD_SHIFT);
        case S_STAGE_CHANGE: return sizeof(PKT_S_STAGE_CHANGE);
        case S_DOOR_STATE: return sizeof(PKT_S_DOOR_STATE);
        case S_PICKUP_COLLECTED: return sizeof(PKT_S_PICKUP_COLLECTED);
        case S_MONSTER_SYNC: return sizeof(PKT_S_MONSTER_SYNC);
        case S_MONSTER_HIT: return sizeof(PKT_S_MONSTER_HIT);
        case S_PLAYER_HIT: return sizeof(PKT_S_PLAYER_HIT);
        case S_PLAYER_RESPAWN: return sizeof(PKT_S_PLAYER_RESPAWN);
        case S_BOSS_PATTERN: return sizeof(PKT_S_BOSS_PATTERN);
        case S_STAGE2_BOSS_INTRO_CUTSCENE: return sizeof(PKT_S_STAGE2_BOSS_INTRO_CUTSCENE);
        case S_LANTERN_GAUGE: return sizeof(PKT_S_LANTERN_GAUGE);
        case S_GOLD_UPDATE: return sizeof(PKT_S_GOLD_UPDATE);
        case S_SHOP_PURCHASE: return sizeof(PKT_S_SHOP_PURCHASE);
        case S_POTION_STATE: return sizeof(PKT_S_POTION_STATE);
        default: return 0;
        }
    }

    int ClampNetworkTier(int tier)
    {
        return std::clamp(tier, 1, 3);
    }

    bool IsKnownPlayerScene(int sceneId)
    {
        return sceneId == PLAYER_SCENE_VILLAGE ||
            sceneId == PLAYER_SCENE_STAGE1 ||
            sceneId == PLAYER_SCENE_STAGE2;
    }

    void LogInvalidNetworkPacket(const char* reason, short packetId, int packetSize, size_t receivedSize)
    {
        char message[192] = {};
        sprintf_s(
            message,
            "[Client] Invalid network packet skipped. reason=%s id=%d size=%d received=%zu\n",
            reason,
            packetId,
            packetSize,
            receivedSize);
        OutputDebugStringA(message);
    }
}

void NetworkManager::ApplyRoomInfo(const PKT_S_ROOM_INFO& roomInfo)
{
    std::lock_guard<std::mutex> lock(m_lobbyMutex);

    m_lobbyState.roomId = roomInfo.roomId;
    m_lobbyState.roomTitle = roomInfo.roomTitle;
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

        if (m_myPlayerId <= 0 && roomInfo.playerCount == 1)
        {
            m_myPlayerId = playerId;
            m_remotePlayers.erase(playerId);
        }

        m_lobbyState.players[i].playerId = playerId;
        m_lobbyState.players[i].displayName = roomInfo.playerNames[i];
        m_lobbyState.players[i].connected = true;
        m_lobbyState.players[i].ready = roomInfo.readyStates[i];
        if (playerId == m_myPlayerId && !m_lobbyState.players[i].displayName.empty())
        {
            m_myDisplayName = m_lobbyState.players[i].displayName;
        }
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
    if (m_isConnected.load() || m_isConnecting.exchange(true))
    {
        return;
    }

    m_connectFailed.store(false);

    std::thread([this, ip, port]() {
        SOCKET connectSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (connectSocket == INVALID_SOCKET)
        {
            m_isConnecting.store(false);
            m_connectFailed.store(true);
            return;
        }

        sockaddr_in serverAddr = {};
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(port);
        if (inet_pton(AF_INET, ip.c_str(), &serverAddr.sin_addr) != 1)
        {
            OutputDebugStringA("[Client] Invalid server IP\n");
            closesocket(connectSocket);
            m_isConnecting.store(false);
            m_connectFailed.store(true);
            return;
        }

        if (connect(connectSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
        {
            OutputDebugStringA("[Client] Connect Failed\n");
            closesocket(connectSocket);
            m_isConnecting.store(false);
            m_connectFailed.store(true);
            return;
        }

        m_socket = connectSocket;
        m_isConnected.store(true);
        m_isConnecting.store(false);
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

bool NetworkManager::IsConnecting() const
{
    return m_isConnecting.load();
}

bool NetworkManager::ConsumeConnectFailed()
{
    return m_connectFailed.exchange(false);
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
    m_isConnecting.store(false);

    if (m_recvThread.joinable())
        m_recvThread.join();
}

void NetworkManager::RecvLoop()
{
    char buffer[4096];
    std::vector<char> pendingBuffer;

    while (m_isRunning)
    {
        int len = recv(m_socket, buffer, sizeof(buffer), 0);
        if (len <= 0) break;

        pendingBuffer.insert(pendingBuffer.end(), buffer, buffer + len);

        while (pendingBuffer.size() >= sizeof(PacketHeader))
        {
            PacketHeader header = {};
            std::memcpy(&header, pendingBuffer.data(), sizeof(PacketHeader));
            const int packetSize = header.size;
            if (packetSize < static_cast<int>(sizeof(PacketHeader)) || packetSize > kMaxPacketSize)
            {
                LogInvalidNetworkPacket("bad_size", header.id, packetSize, pendingBuffer.size());
                pendingBuffer.clear();
                m_isRunning = false;
                break;
            }

            if (pendingBuffer.size() < static_cast<size_t>(packetSize))
            {
                break;
            }

            std::vector<char> packetData(pendingBuffer.begin(), pendingBuffer.begin() + packetSize);
            {
                std::lock_guard<std::mutex> lock(m_queueMutex);
                m_packetQueue.push(packetData);
            }

            pendingBuffer.erase(pendingBuffer.begin(), pendingBuffer.begin() + packetSize);
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
        const int expectedPacketSize = GetExpectedServerPacketSize(header->id);
        if (expectedPacketSize > 0 && packetData.size() < static_cast<size_t>(expectedPacketSize))
        {
            LogInvalidNetworkPacket(
                "short_packet",
                header->id,
                header->size,
                packetData.size());
            continue;
        }

        switch (header->id)
        {
        case S_LOGIN:
        {
            PKT_S_LOGIN* res = (PKT_S_LOGIN*)packetData.data();
            if (res->success)
            {
                OutputDebugStringA("[Client] 로그인 성공!\n");
                m_myPlayerId = res->myPlayerId;
                m_remotePlayers.erase(m_myPlayerId);
                SetLocalScene(PLAYER_SCENE_VILLAGE);
                SetLocalEquipmentTiers(1, 1);
                {
                    std::lock_guard<std::mutex> lock(m_lobbyMutex);
                    m_lobbyState = {};
                    m_lobbyState.selfPlayerId = m_myPlayerId;
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

        case S_REGISTER:
        {
            PKT_S_REGISTER* res = (PKT_S_REGISTER*)packetData.data();
            m_registerResult = res->success ? 1 : -1;
            OutputDebugStringA(res->success ? "[Client] Register success\n" : "[Client] Register failed\n");
            break;
        }

        case S_ROOM_LIST:
        {
            PKT_S_ROOM_LIST* res = (PKT_S_ROOM_LIST*)packetData.data();
            std::lock_guard<std::mutex> lock(m_roomListMutex);
            m_roomList.clear();

            const int roomCount = (std::min)(
                (std::max)(0, res->roomCount),
                MAX_ROOM_LIST_ROOMS);
            m_roomList.reserve(static_cast<size_t>(roomCount));
            for (int i = 0; i < roomCount; ++i)
            {
                const RoomListEntry& source = res->rooms[i];
                RoomListItem item = {};
                item.roomId = source.roomId;
                item.playerCount = source.playerCount;
                item.maxPlayers = source.maxPlayers;
                item.inGame = source.inGame;
                item.title = source.title;
                m_roomList.push_back(item);
            }
            break;
        }

        case S_CREATE_ROOM:
        {
            PKT_S_CREATE_ROOM* res = (PKT_S_CREATE_ROOM*)packetData.data();
            m_createRoomResult = res->success ? res->roomId : -1;
            break;
        }

        case S_JOIN_ROOM:
        {
            PKT_S_JOIN_ROOM* res = (PKT_S_JOIN_ROOM*)packetData.data();
            m_joinRoomResult = res->success ? res->roomId : -1;
            if (res->success)
            {
                SetLocalScene(PLAYER_SCENE_VILLAGE);
                m_remotePlayers.clear();
                {
                    std::lock_guard<std::mutex> monsterLock(m_monsterMutex);
                    m_remoteMonsters.clear();
                    m_remoteMonsterHits.clear();
                }
            }
            break;
        }

        case S_LEAVE_ROOM:
        {
            PKT_S_LEAVE_ROOM* res = (PKT_S_LEAVE_ROOM*)packetData.data();
            m_leaveRoomResult = res->success ? 1 : -1;
            if (res->success)
            {
                SetLocalScene(PLAYER_SCENE_VILLAGE);
                m_remotePlayers.clear();
                {
                    std::lock_guard<std::mutex> monsterLock(m_monsterMutex);
                    m_remoteMonsters.clear();
                    m_remoteMonsterHits.clear();
                }
                {
                    std::lock_guard<std::mutex> lock(m_lobbyMutex);
                    m_lobbyState = {};
                    m_lobbyState.selfPlayerId = m_myPlayerId;
                }
            }
            break;
        }

        case S_PLAYER_MOVE:
        {
            PKT_S_PLAYER_MOVE* res = (PKT_S_PLAYER_MOVE*)packetData.data();

            if (m_myPlayerId > 0 && res->playerId == m_myPlayerId) break;
            if (!IsKnownPlayerScene(res->currentScene))
            {
                m_remotePlayers.erase(res->playerId);
                break;
            }

            m_remotePlayers[res->playerId] = *res;
            break;
        }

        case S_PLAYER_ATTACK:
        {
            PKT_S_PLAYER_ATTACK* res = (PKT_S_PLAYER_ATTACK*)packetData.data();
            if (m_myPlayerId != -1 && res->playerId == m_myPlayerId) break;
            if (!IsKnownPlayerScene(res->currentScene) ||
                res->currentScene != m_localScene.load())
            {
                break;
            }

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
            m_remotePlayers.erase(res->playerId);
            {
                std::lock_guard<std::mutex> leaveLock(m_playerLeaveMutex);
                m_playerLeaves.push_back(res->playerId);
                while (m_playerLeaves.size() > 16)
                {
                    m_playerLeaves.pop_front();
                }
            }

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

        case S_GAME_RESULT:
        {
            PKT_S_GAME_RESULT* res = (PKT_S_GAME_RESULT*)packetData.data();
            m_pendingGameResult = res->resultCode;
            {
                std::lock_guard<std::mutex> lock(m_gameResultMutex);
                m_gameResults.push_back(*res);
                while (m_gameResults.size() > 4)
                {
                    m_gameResults.pop_front();
                }
            }
            OutputDebugStringA("[Client] Received server game result\n");
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
            m_pendingStageElapsedSeconds = (std::max)(0.0f, res->stageElapsedSeconds);
            if (res->targetStage == PLAYER_SCENE_VILLAGE)
            {
                SetLocalScene(PLAYER_SCENE_VILLAGE);
            }
            else if (res->targetStage == PLAYER_SCENE_STAGE1)
            {
                SetLocalScene(PLAYER_SCENE_STAGE1);
            }
            else if (res->targetStage == PLAYER_SCENE_STAGE2)
            {
                SetLocalScene(PLAYER_SCENE_STAGE2);
            }
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
                PKT_S_MONSTER_SYNC sync = *res;
                const bool allowHpIncrease =
                    sync.monsterId == STAGE2_BOSS_MONSTER_ID &&
                    !sync.isDead &&
                    sync.state != 3;

                auto hitIt = m_remoteMonsterHits.find(sync.monsterId);
                if (hitIt != m_remoteMonsterHits.end())
                {
                    const PKT_S_MONSTER_HIT& hit = hitIt->second;
                    const bool syncLooksLikeRespawn =
                        hit.isDead &&
                        !sync.isDead &&
                        sync.state != 3 &&
                        sync.remainHp > hit.remainHp;

                    if (syncLooksLikeRespawn)
                    {
                        m_remoteMonsterHits.erase(hitIt);
                    }
                    else if (!allowHpIncrease && (hit.isDead || hit.remainHp < sync.remainHp))
                    {
                        sync.remainHp = hit.remainHp;
                        sync.isDead = hit.isDead;
                        if (hit.isDead)
                        {
                            sync.state = 3;
                        }
                    }
                }

                auto syncIt = m_remoteMonsters.find(sync.monsterId);
                if (syncIt != m_remoteMonsters.end())
                {
                    const PKT_S_MONSTER_SYNC& previous = syncIt->second;
                    if (previous.isDead && !sync.isDead)
                    {
                        const bool syncLooksLikeRespawn =
                            sync.state != 3 &&
                            sync.remainHp > previous.remainHp;

                        if (!syncLooksLikeRespawn)
                        {
                            sync.isDead = true;
                            sync.state = 3;
                            sync.remainHp = (std::min)(sync.remainHp, previous.remainHp);
                        }
                    }
                    else if (!sync.isDead &&
                        !allowHpIncrease &&
                        previous.remainHp > 0 &&
                        sync.remainHp > previous.remainHp)
                    {
                        sync.remainHp = previous.remainHp;
                    }
                }

                m_remoteMonsters[sync.monsterId] = sync;
            }
            break;
        }

        case S_MONSTER_HIT:
        {
            PKT_S_MONSTER_HIT* res = (PKT_S_MONSTER_HIT*)packetData.data();
            {
                std::lock_guard<std::mutex> lock(m_monsterMutex);
                PKT_S_MONSTER_HIT hit = *res;
                auto existingIt = m_remoteMonsterHits.find(res->monsterId);
                if (existingIt != m_remoteMonsterHits.end())
                {
                    const PKT_S_MONSTER_HIT& previous = existingIt->second;
                    if (previous.isDead && !hit.isDead)
                    {
                        break;
                    }

                    hit.damage = (std::max)(0, previous.damage) + (std::max)(0, hit.damage);
                    hit.hitSequence = (std::max)(previous.hitSequence, hit.hitSequence);

                    if (!hit.isDead &&
                        previous.remainHp > 0 &&
                        hit.remainHp > previous.remainHp)
                    {
                        hit.remainHp = previous.remainHp;
                    }
                }

                m_remoteMonsterHits[hit.monsterId] = hit;

                auto syncIt = m_remoteMonsters.find(hit.monsterId);
                if (syncIt != m_remoteMonsters.end())
                {
                    PKT_S_MONSTER_SYNC& sync = syncIt->second;
                    if (hit.isDead || hit.remainHp < sync.remainHp)
                    {
                        sync.remainHp = hit.remainHp;
                        sync.isDead = hit.isDead;
                        if (hit.isDead)
                        {
                            sync.state = 3;
                        }
                    }
                }
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
            if (!IsKnownPlayerScene(res->currentScene) ||
                res->currentScene != m_localScene.load())
            {
                break;
            }

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
                movePkt.classType = res->classType;
                movePkt.playerLevel = res->playerLevel;
                movePkt.weaponTier = res->weaponTier;
                movePkt.armorTier = res->armorTier;
                movePkt.currentScene = res->currentScene;
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

        case S_STAGE2_BOSS_INTRO_CUTSCENE:
        {
            PKT_S_STAGE2_BOSS_INTRO_CUTSCENE* res = (PKT_S_STAGE2_BOSS_INTRO_CUTSCENE*)packetData.data();
            std::lock_guard<std::mutex> lock(m_stage2BossIntroMutex);
            m_stage2BossIntroCutscenes.push_back(*res);
            while (m_stage2BossIntroCutscenes.size() > 4)
            {
                m_stage2BossIntroCutscenes.pop_front();
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

        case S_GOLD_UPDATE:
        {
            PKT_S_GOLD_UPDATE* res = (PKT_S_GOLD_UPDATE*)packetData.data();
            std::lock_guard<std::mutex> lock(m_goldUpdateMutex);
            m_goldUpdates.push_back(*res);
            while (m_goldUpdates.size() > 32)
            {
                m_goldUpdates.pop_front();
            }
            break;
        }

        case S_SHOP_PURCHASE:
        {
            PKT_S_SHOP_PURCHASE* res = (PKT_S_SHOP_PURCHASE*)packetData.data();
            std::lock_guard<std::mutex> lock(m_shopPurchaseMutex);
            m_shopPurchaseResults.push_back(*res);
            while (m_shopPurchaseResults.size() > 16)
            {
                m_shopPurchaseResults.pop_front();
            }
            break;
        }

        case S_POTION_STATE:
        {
            PKT_S_POTION_STATE* res = (PKT_S_POTION_STATE*)packetData.data();
            std::lock_guard<std::mutex> lock(m_potionStateMutex);
            m_potionStates.push_back(*res);
            while (m_potionStates.size() > 16)
            {
                m_potionStates.pop_front();
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

void NetworkManager::SendRegister(const std::string& id, const std::string& pw)
{
    m_registerResult = 0;

    PKT_C_REGISTER pkt = {};
    pkt.header.size = sizeof(PKT_C_REGISTER);
    pkt.header.id = C_REGISTER;
    strncpy_s(pkt.id, id.c_str(), _TRUNCATE);
    strncpy_s(pkt.password, pw.c_str(), _TRUNCATE);
    SendPacket(&pkt, sizeof(PKT_C_REGISTER));
}

void NetworkManager::SendRoomListRequest()
{
    PKT_C_ROOM_LIST pkt = {};
    pkt.header.size = sizeof(PKT_C_ROOM_LIST);
    pkt.header.id = C_ROOM_LIST;
    SendPacket(&pkt, sizeof(PKT_C_ROOM_LIST));
}

void NetworkManager::SendCreateRoom(const std::string& title)
{
    m_createRoomResult = 0;
    m_joinRoomResult = 0;

    PKT_C_CREATE_ROOM pkt = {};
    pkt.header.size = sizeof(PKT_C_CREATE_ROOM);
    pkt.header.id = C_CREATE_ROOM;
    strncpy_s(pkt.title, title.c_str(), _TRUNCATE);
    SendPacket(&pkt, sizeof(PKT_C_CREATE_ROOM));
}

void NetworkManager::SendJoinRoom(int roomId)
{
    m_joinRoomResult = 0;

    PKT_C_JOIN_ROOM pkt = {};
    pkt.header.size = sizeof(PKT_C_JOIN_ROOM);
    pkt.header.id = C_JOIN_ROOM;
    pkt.roomId = roomId;
    SendPacket(&pkt, sizeof(PKT_C_JOIN_ROOM));
}

void NetworkManager::SendLeaveRoom()
{
    m_leaveRoomResult = 0;

    PKT_C_LEAVE_ROOM pkt = {};
    pkt.header.size = sizeof(PKT_C_LEAVE_ROOM);
    pkt.header.id = C_LEAVE_ROOM;
    SendPacket(&pkt, sizeof(PKT_C_LEAVE_ROOM));
}

void NetworkManager::SendPlayerMove(float x, float y, float z, float rotY, int animationState, int classType, int playerLevel)
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
    pkt.playerLevel = playerLevel;
    pkt.weaponTier = m_localWeaponTier.load();
    pkt.armorTier = m_localArmorTier.load();
    pkt.currentScene = m_localScene.load();
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

void NetworkManager::SendPlayerAttackCast(int skillType, int classType, int playerLevel, int targetMonsterId, float x, float y, float z, float rotY, float visualRange, float visualDelay)
{
    PKT_C_PLAYER_ATTACK pkt = {};
    pkt.header.size = sizeof(PKT_C_PLAYER_ATTACK);
    pkt.header.id = C_PLAYER_ATTACK;
    pkt.attackerId = m_myPlayerId;
    pkt.classType = classType;
    pkt.playerLevel = playerLevel;
    pkt.targetMonsterId = targetMonsterId;
    pkt.x = x;
    pkt.y = y;
    pkt.z = z;
    pkt.rotY = rotY;
    pkt.skillType = skillType;
    pkt.attackPhase = PLAYER_ATTACK_PHASE_CAST;
    pkt.range = visualRange;
    pkt.radius = visualDelay;
    SendPacket(&pkt, sizeof(PKT_C_PLAYER_ATTACK));
}

void NetworkManager::SendPlayerAttack(
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
    const PlayerAttackOrientedHitbox* orientedHitbox)
{
    PKT_C_PLAYER_ATTACK pkt = {};
    pkt.header.size = sizeof(PKT_C_PLAYER_ATTACK);
    pkt.header.id = C_PLAYER_ATTACK;
    pkt.attackerId = m_myPlayerId;
    pkt.classType = classType;
    pkt.playerLevel = playerLevel;
    pkt.targetMonsterId = targetMonsterId;
    pkt.x = x;
    pkt.y = y;
    pkt.z = z;
    pkt.rotY = rotY;
    pkt.skillType = skillType;
    pkt.attackPhase = PLAYER_ATTACK_PHASE_IMPACT;
    pkt.range = range;
    pkt.radius = radius;
    pkt.coneDot = coneDot;
    pkt.hitShapeType = PLAYER_ATTACK_HIT_SHAPE_NONE;
    if (orientedHitbox != nullptr)
    {
        pkt.hitShapeType = PLAYER_ATTACK_HIT_SHAPE_ORIENTED_BOX;
        pkt.hitboxCenterX = orientedHitbox->centerX;
        pkt.hitboxCenterY = orientedHitbox->centerY;
        pkt.hitboxCenterZ = orientedHitbox->centerZ;
        pkt.hitboxExtentX = orientedHitbox->extentX;
        pkt.hitboxExtentY = orientedHitbox->extentY;
        pkt.hitboxExtentZ = orientedHitbox->extentZ;
        pkt.hitboxOrientationX = orientedHitbox->orientationX;
        pkt.hitboxOrientationY = orientedHitbox->orientationY;
        pkt.hitboxOrientationZ = orientedHitbox->orientationZ;
        pkt.hitboxOrientationW = orientedHitbox->orientationW;
    }
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
        m_pendingStageElapsedSeconds = 0.0f;
        return;
    }

    SendPacket(&pkt, sizeof(PKT_C_STAGE_CHANGE));
}

void NetworkManager::SendInteractPortal()
{
    PKT_C_INTERACT_PORTAL pkt = {};
    pkt.header.size = sizeof(PKT_C_INTERACT_PORTAL);
    pkt.header.id = C_INTERACT_PORTAL;
    SendPacket(&pkt, sizeof(PKT_C_INTERACT_PORTAL));
}

void NetworkManager::SendGoldPickup(int pickupGroupId, float x, float y, float z, float radius)
{
    PKT_C_GOLD_PICKUP pkt = {};
    pkt.header.size = sizeof(PKT_C_GOLD_PICKUP);
    pkt.header.id = C_GOLD_PICKUP;
    pkt.pickupGroupId = pickupGroupId;
    pkt.x = x;
    pkt.y = y;
    pkt.z = z;
    pkt.radius = radius;
    SendPacket(&pkt, sizeof(PKT_C_GOLD_PICKUP));
}

void NetworkManager::SendShopPurchase(int shopItemId)
{
    PKT_C_SHOP_PURCHASE pkt = {};
    pkt.header.size = sizeof(PKT_C_SHOP_PURCHASE);
    pkt.header.id = C_SHOP_PURCHASE;
    pkt.shopItemId = shopItemId;
    SendPacket(&pkt, sizeof(PKT_C_SHOP_PURCHASE));
}

void NetworkManager::SendPotionUse(int slotIndex)
{
    PKT_C_POTION_USE pkt = {};
    pkt.header.size = sizeof(PKT_C_POTION_USE);
    pkt.header.id = C_POTION_USE;
    pkt.slotIndex = slotIndex;
    SendPacket(&pkt, sizeof(PKT_C_POTION_USE));
}

void NetworkManager::SendPlayerRespawn()
{
    PKT_C_PLAYER_RESPAWN pkt = {};
    pkt.header.size = sizeof(PKT_C_PLAYER_RESPAWN);
    pkt.header.id = C_PLAYER_RESPAWN;
    SendPacket(&pkt, sizeof(PKT_C_PLAYER_RESPAWN));
}

void NetworkManager::SetLocalScene(int sceneId)
{
    if (!IsKnownPlayerScene(sceneId))
    {
        return;
    }

    const int previousScene = m_localScene.exchange(sceneId);
    if (previousScene != sceneId)
    {
        m_remotePlayers.clear();
        {
            std::lock_guard<std::mutex> lock(m_remoteAttackMutex);
            m_remotePlayerAttacks.clear();
        }
        {
            std::lock_guard<std::mutex> lock(m_monsterMutex);
            m_remoteMonsters.clear();
            m_remoteMonsterHits.clear();
        }
        {
            std::lock_guard<std::mutex> lock(m_playerHitMutex);
            m_playerHits.clear();
        }
        {
            std::lock_guard<std::mutex> lock(m_playerRespawnMutex);
            m_playerRespawns.clear();
        }
        {
            std::lock_guard<std::mutex> lock(m_bossPatternMutex);
            m_bossPatterns.clear();
        }
        {
            std::lock_guard<std::mutex> lock(m_stage2BossIntroMutex);
            m_stage2BossIntroCutscenes.clear();
        }
    }
}

int NetworkManager::GetLocalScene() const
{
    return m_localScene.load();
}

void NetworkManager::SetLocalEquipmentTiers(int weaponTier, int armorTier)
{
    m_localWeaponTier = ClampNetworkTier(weaponTier);
    m_localArmorTier = ClampNetworkTier(armorTier);
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

std::vector<int> NetworkManager::PopPlayerLeaves()
{
    std::vector<int> leaves;

    std::lock_guard<std::mutex> lock(m_playerLeaveMutex);
    while (!m_playerLeaves.empty())
    {
        leaves.push_back(m_playerLeaves.front());
        m_playerLeaves.pop_front();
    }

    return leaves;
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

std::vector<PKT_S_STAGE2_BOSS_INTRO_CUTSCENE> NetworkManager::PopStage2BossIntroCutscenes()
{
    std::vector<PKT_S_STAGE2_BOSS_INTRO_CUTSCENE> cutscenes;

    std::lock_guard<std::mutex> lock(m_stage2BossIntroMutex);
    while (!m_stage2BossIntroCutscenes.empty())
    {
        cutscenes.push_back(m_stage2BossIntroCutscenes.front());
        m_stage2BossIntroCutscenes.pop_front();
    }

    return cutscenes;
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

std::vector<PKT_S_GAME_RESULT> NetworkManager::PopGameResults()
{
    std::vector<PKT_S_GAME_RESULT> results;

    std::lock_guard<std::mutex> lock(m_gameResultMutex);
    while (!m_gameResults.empty())
    {
        results.push_back(m_gameResults.front());
        m_gameResults.pop_front();
    }

    return results;
}

std::vector<PKT_S_GOLD_UPDATE> NetworkManager::PopGoldUpdates()
{
    std::vector<PKT_S_GOLD_UPDATE> updates;

    std::lock_guard<std::mutex> lock(m_goldUpdateMutex);
    while (!m_goldUpdates.empty())
    {
        updates.push_back(m_goldUpdates.front());
        m_goldUpdates.pop_front();
    }

    return updates;
}

std::vector<PKT_S_SHOP_PURCHASE> NetworkManager::PopShopPurchaseResults()
{
    std::vector<PKT_S_SHOP_PURCHASE> results;

    std::lock_guard<std::mutex> lock(m_shopPurchaseMutex);
    while (!m_shopPurchaseResults.empty())
    {
        results.push_back(m_shopPurchaseResults.front());
        m_shopPurchaseResults.pop_front();
    }

    return results;
}

std::vector<PKT_S_POTION_STATE> NetworkManager::PopPotionStates()
{
    std::vector<PKT_S_POTION_STATE> states;

    std::lock_guard<std::mutex> lock(m_potionStateMutex);
    while (!m_potionStates.empty())
    {
        states.push_back(m_potionStates.front());
        m_potionStates.pop_front();
    }

    return states;
}

LobbyStateSnapshot NetworkManager::GetLobbyState()
{
    std::lock_guard<std::mutex> lock(m_lobbyMutex);
    return m_lobbyState;
}

std::vector<RoomListItem> NetworkManager::GetRoomListSnapshot()
{
    std::lock_guard<std::mutex> lock(m_roomListMutex);
    return m_roomList;
}

int NetworkManager::GetLocalPlayerSlotIndex()
{
    std::lock_guard<std::mutex> lock(m_lobbyMutex);
    for (int i = 0; i < MAX_LOBBY_PLAYERS; ++i)
    {
        const LobbyPlayerInfo& player = m_lobbyState.players[i];
        if (player.connected && player.playerId == m_myPlayerId)
        {
            return i;
        }
    }

    return 0;
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
    return m_pendingStageChange.exchange(-1);
}

float NetworkManager::ConsumeStageElapsedSeconds()
{
    return m_pendingStageElapsedSeconds.exchange(0.0f);
}

int NetworkManager::ConsumeGameResultSignal()
{
    return m_pendingGameResult.exchange(0);
}

int NetworkManager::ConsumeLoginResult()
{
    return m_loginResult.exchange(0);
}

int NetworkManager::ConsumeRegisterResult()
{
    return m_registerResult.exchange(0);
}

int NetworkManager::ConsumeCreateRoomResult()
{
    return m_createRoomResult.exchange(0);
}

int NetworkManager::ConsumeJoinRoomResult()
{
    return m_joinRoomResult.exchange(0);
}

int NetworkManager::ConsumeLeaveRoomResult()
{
    return m_leaveRoomResult.exchange(0);
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
