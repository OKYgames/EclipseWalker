#pragma once
#include <cstdint>

#pragma pack(push, 1)

constexpr int MAX_LOBBY_PLAYERS = 3;

enum PacketID
{
    C_LOGIN = 1,
    S_LOGIN = 2,
    C_CHAT = 3,
    S_CHAT = 4,
    C_PLAYER_MOVE = 5,
    S_PLAYER_MOVE = 6,
    S_MONSTER_SYNC = 7,
    C_LOBBY_READY = 8,
    S_LOBBY_STATE = 9,
    C_GAME_START = 10,
    S_GAME_START = 11
};

struct PacketHeader
{
    short size;
    short id;
};

struct PKT_C_LOGIN
{
    PacketHeader header;
    char id[20];
    char password[20];
};

struct PKT_S_LOGIN
{
    PacketHeader header;
    bool success;
    int myPlayerId;
};

struct PKT_C_CHAT
{
    PacketHeader header;
    char msg[100];
};

struct PKT_S_CHAT
{
    PacketHeader header;
    int playerId;
    char msg[100];
};

struct PKT_C_PLAYER_MOVE
{
    PacketHeader header;
    float x;
    float y;
    float z;
    float rotY;
};

struct PKT_S_PLAYER_MOVE
{
    PacketHeader header;
    int playerId;
    float x;
    float y;
    float z;
    float rotY;
};

struct LobbyPlayerSlot
{
    int playerId;
    bool connected;
    bool ready;
    bool isHost;
};

struct PKT_C_LOBBY_READY
{
    PacketHeader header;
    bool ready;
};

struct PKT_S_LOBBY_STATE
{
    PacketHeader header;
    int selfPlayerId;
    int hostPlayerId;
    int playerCount;
    bool canStart;
    LobbyPlayerSlot players[MAX_LOBBY_PLAYERS];
};

struct PKT_C_GAME_START
{
    PacketHeader header;
};

struct PKT_S_GAME_START
{
    PacketHeader header;
};

// ← 추가
struct PKT_S_MONSTER_SYNC
{
    PacketHeader header;
    int   monsterId;
    int   monsterType;
    int   state;       // 0:IDLE, 1:TRACE, 2:ATTACK, 3:DIE
    float x, y, z;
    float rotY;
};

#pragma pack(pop)
