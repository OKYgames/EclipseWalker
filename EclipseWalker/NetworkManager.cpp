#include "NetworkManager.h"
#include <iostream>
#include <cstring>

#pragma comment(lib, "ws2_32.lib")

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

        m_isConnected = true;
        m_isRunning = true;

        m_recvThread = std::thread(&NetworkManager::RecvLoop, this);
        OutputDebugStringA("[Client] Connect Success\n");

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
    m_isConnected = false;

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
    OutputDebugStringA("[Client] 서버 연결 끊김\n");
}

void NetworkManager::ProcessPackets()
{
    while (true)
    {
        std::vector<char> packetData;
        {
            std::lock_guard<std::mutex> lock(m_queueMutex);
            if (m_packetQueue.empty()) break;
            packetData = m_packetQueue.front();
            m_packetQueue.pop();
        }

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

        case S_CHAT:
        {
            PKT_S_CHAT* res = (PKT_S_CHAT*)packetData.data();

            ChatMessage chatMessage;
            chatMessage.playerId = res->playerId;
            chatMessage.text = res->msg;

            std::lock_guard<std::mutex> lock(m_chatMutex);
            m_chatMessages.push_back(chatMessage);
            while (m_chatMessages.size() > 32)
            {
                m_chatMessages.pop_front();
            }
            break;
        }

        case S_LOBBY_STATE:
        {
            PKT_S_LOBBY_STATE* res = (PKT_S_LOBBY_STATE*)packetData.data();

            LobbyStateSnapshot snapshot;
            snapshot.selfPlayerId = res->selfPlayerId;
            snapshot.hostPlayerId = res->hostPlayerId;
            snapshot.playerCount = res->playerCount;
            snapshot.canStart = res->canStart;

            for (int i = 0; i < MAX_LOBBY_PLAYERS; ++i)
            {
                snapshot.players[i].playerId = res->players[i].playerId;
                snapshot.players[i].connected = res->players[i].connected;
                snapshot.players[i].ready = res->players[i].ready;
                snapshot.players[i].isHost = res->players[i].isHost;
            }

            {
                std::lock_guard<std::mutex> lock(m_lobbyMutex);
                m_lobbyState = snapshot;
            }

            if (res->selfPlayerId != -1)
            {
                m_myPlayerId = res->selfPlayerId;
            }
            break;
        }

        case S_GAME_START:
        {
            m_pendingGameStart = true;
            break;
        }

        // ← 추가: 몬스터 동기화 패킷 처리
        case S_MONSTER_SYNC:
        {
            PKT_S_MONSTER_SYNC* res = (PKT_S_MONSTER_SYNC*)packetData.data();
            {
                std::lock_guard<std::mutex> lock(m_monsterMutex);
                m_remoteMonsters[res->monsterId] = *res;
            }
            break;
        }
        }
    }
}

void NetworkManager::SendPacket(void* packet, int size)
{
    if (!m_isConnected) return;
    send(m_socket, (char*)packet, size, 0);
}

void NetworkManager::SendLogin(const std::string& id, const std::string& pw)
{
    PKT_C_LOGIN pkt;
    pkt.header.size = sizeof(PKT_C_LOGIN);
    pkt.header.id = C_LOGIN;
    strcpy_s(pkt.id, id.c_str());
    strcpy_s(pkt.password, pw.c_str());
    SendPacket(&pkt, sizeof(PKT_C_LOGIN));
}

void NetworkManager::SendPlayerMove(float x, float y, float z, float rotY)
{
    PKT_C_PLAYER_MOVE pkt;
    pkt.header.size = sizeof(PKT_C_PLAYER_MOVE);
    pkt.header.id = C_PLAYER_MOVE;
    pkt.x = x;
    pkt.y = y;
    pkt.z = z;
    pkt.rotY = rotY;
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

void NetworkManager::SendLobbyReady(bool ready)
{
    PKT_C_LOBBY_READY pkt = {};
    pkt.header.size = sizeof(PKT_C_LOBBY_READY);
    pkt.header.id = C_LOBBY_READY;
    pkt.ready = ready;
    SendPacket(&pkt, sizeof(PKT_C_LOBBY_READY));
}

void NetworkManager::SendGameStart()
{
    PKT_C_GAME_START pkt = {};
    pkt.header.size = sizeof(PKT_C_GAME_START);
    pkt.header.id = C_GAME_START;
    SendPacket(&pkt, sizeof(PKT_C_GAME_START));
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

LobbyStateSnapshot NetworkManager::GetLobbyState()
{
    std::lock_guard<std::mutex> lock(m_lobbyMutex);
    return m_lobbyState;
}

bool NetworkManager::ConsumeGameStartSignal()
{
    return m_pendingGameStart.exchange(false);
}
