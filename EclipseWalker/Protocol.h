#pragma once

// 메모리 1바이트 정렬 (서버-클라 크기 불일치 방지용)
#pragma pack(push, 1)

// 패킷 ID 목록
enum PacketID
{
    C_LOGIN = 1,        // Client -> Server: 로그인 요청
    S_LOGIN = 2,        // Server -> Client: 로그인 결과
    C_CHAT = 3,         // Client -> Server: 채팅 보내기
    S_CHAT = 4,         // Server -> Client: 채팅 뿌리기
    C_PLAYER_MOVE = 5,  // Client -> Server: 내 캐릭터 움직임
    S_PLAYER_MOVE = 6,  // Server -> Client: 다른 캐릭터 움직임 뿌리기
    C_PLAYER_ATTACK = 7 // Client -> Server: 공격 (추후 구현용)
};

// 모든 패킷의 맨 앞에 붙는 공통 헤더
struct PacketHeader
{
    short size; // 패킷의 전체 크기
    short id;   // 패킷의 종류 (PacketID)
};

// -------------------------------------------------
// [로그인 관련 패킷]
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
    int playerId; // 부여받은 고유 ID (UID)
};

// -------------------------------------------------
// [이동 관련 패킷]
// -------------------------------------------------
struct PKT_C_PLAYER_MOVE
{
    PacketHeader header;
    float x;
    float y;
    float z;
    float rotY; // 바라보는 방향
};

struct PKT_S_PLAYER_MOVE
{
    PacketHeader header;
    int playerId; // 누가 움직였는지
    float x;
    float y;
    float z;
    float rotY;
};

#pragma pack(pop) // 정렬 설정 복구