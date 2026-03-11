#pragma once
#include <cstdint>

#pragma pack(push, 1)

// 패킷 타입 정의 (Enum)
enum class PacketType : uint16_t
{
    // CS = Client to Server
    CS_LOGIN = 100,
    CS_PLAYER_MOVE = 101,
    CS_PLAYER_ATTACK = 102,

    // SC = Server to Client 
    SC_LOGIN_OK = 200,
    SC_ADD_OBJECT = 201,     // 나, 다른 유저, 또는 몬스터가 시야에 들어옴
    SC_REMOVE_OBJECT = 202,  // 누군가 시야에서 사라짐 (죽거나 멀어짐)
    SC_UPDATE_POSITION = 203,// 이동 동기화
    SC_MONSTER_STATE = 204,  // 몬스터의 상태(추격, 공격 등) 동기화
    SC_DAMAGE_INFO = 205     // 데미지 및 체력 동기화
};

// 모든 패킷의 기본이 되는 헤더 
struct PacketHeader
{
    uint16_t size; 
    PacketType type;
};

// =========================================================
// [Client -> Server] 패킷들
// =========================================================

struct CS_PlayerMove
{
    PacketHeader header;
    float x, y, z;       // 이동하려는 위치
    float rotY;          // 바라보는 방향
};

struct CS_PlayerAttack
{
    PacketHeader header;
    int targetId;        // 내가 때린 대상의 ID (허공에 스윙했으면 -1)
};


// =========================================================
// [Server -> Client] 패킷들
// =========================================================

// 게임 내 객체 타입 
enum class ObjectType : uint8_t { PLAYER = 0, MONSTER = 1 };

// 시야에 새로운 오브젝트가 나타났을 때 (생성)
struct SC_AddObject
{
    PacketHeader header;
    int objectId;        // 서버가 부여한 고유 ID
    uint8_t objType;     // ObjectType (0: 유저, 1: 몬스터)
    float x, y, z;       // 초기 위치
    float hp, maxHp;     // 체력 정보
};

// 위치 동기화 (서버가 확정한 위치)
struct SC_UpdatePosition
{
    PacketHeader header;
    int objectId;        // 누구의 위치인가?
    float x, y, z;
    float rotY;
};

// 몬스터 상태 동기화
struct SC_MonsterState
{
    PacketHeader header;
    int monsterId;       // 어떤 몬스터인가?
    uint8_t state;       // 0: IDLE, 1: TRACE, 2: ATTACK, 3: DIE
    int targetId;        // 쫓고 있는 타겟의 ID (-1이면 타겟 없음)
};

// 누군가 데미지를 입었을 때
struct SC_DamageInfo
{
    PacketHeader header;
    int targetId;        // 맞은 대상의 ID
    int attackerId;      // 때린 대상의 ID
    float damage;        // 입은 데미지
    float remainHp;      // 남은 체력 
};

#pragma pack(pop) 