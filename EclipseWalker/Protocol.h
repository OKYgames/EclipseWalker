#pragma once

#pragma pack(push, 1)

enum PacketID
{
    C_LOGIN = 1,
    S_LOGIN = 2,
    C_CHAT = 3,
    S_CHAT = 4,
    C_PLAYER_MOVE = 5,
    S_PLAYER_MOVE = 6,
    S_MONSTER_SYNC = 7
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