#pragma once

#pragma pack(push, 1)

enum PacketID
{
    C_LOGIN = 1,
    S_LOGIN = 2,
    C_CHAT = 3,         // ★ 채팅 복구
    S_CHAT = 4,         // ★ 채팅 복구
    C_PLAYER_MOVE = 5,
    S_PLAYER_MOVE = 6
};

struct PacketHeader
{
    short size;
    short id;
};

// -------------------------------------------------
// [로그인]
// -------------------------------------------------
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
    int myPlayerId; // ★ 서버 코드에 맞게 playerId -> myPlayerId 로 수정!
};

// -------------------------------------------------
// [채팅] - 서버 에러 해결을 위해 추가
// -------------------------------------------------
struct PKT_C_CHAT
{
    PacketHeader header;
    char msg[100]; // 넉넉하게 100바이트 할당
};

struct PKT_S_CHAT
{
    PacketHeader header;
    int playerId;  // 누가 보냈는지
    char msg[100];
};

// -------------------------------------------------
// [이동]
// -------------------------------------------------
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

#pragma pack(pop)