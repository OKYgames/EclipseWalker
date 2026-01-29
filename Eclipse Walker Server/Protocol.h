#pragma once

typedef unsigned short ushort;
typedef unsigned int   uint;
typedef long long      int64;

// [패킷 ID 정의]
enum PacketID
{
    C_LOGIN = 1, // Client -> Server : 로그인 요청
    S_LOGIN = 2, // Server -> Client : 로그인 결과
    C_CHAT = 3, // Client -> Server : 채팅 요청
    S_CHAT = 4, // Server -> Client : 채팅 응답
};

// [패킷의 신분증 (헤더)]
#pragma pack(push, 1) // 1바이트 정렬 (중요: 빈 공간 없이 딱 붙여서 전송)
struct PacketHeader
{
    ushort size; // 패킷 크기
    ushort id;   // 패킷 ID
};

// [로그인 패킷]
struct PKT_C_LOGIN
{
    PacketHeader header;
    char id[50];
    char pw[50];
};

// [수정] 로그인 응답 (Server -> Client)
struct PKT_S_LOGIN
{
    PacketHeader header;
    bool success;
    int  playerDbId; // DB에서 발급한 고유 번호 (UID)
};

// [채팅 패킷]
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
#pragma pack(pop)