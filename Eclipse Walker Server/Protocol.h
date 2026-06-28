#pragma once

#pragma pack(push, 1)

constexpr int MAX_LOBBY_PLAYERS = 3;
constexpr int MAX_CHAT_NAME = 20;
constexpr int MAX_GAME_RECORDS = 10;
constexpr int MAX_RECORD_SUMMARY = 96;
constexpr int STAGE2_BOSS_MONSTER_ID = 1001;
constexpr int STAGE2_BOSS_MONSTER_TYPE = 100;
constexpr int BOSS_PATTERN_STAGE2_SHOCKWAVE = 1;
constexpr int BOSS_PATTERN_STAGE2_WIPE = 2;
constexpr int BOSS_PATTERN_STAGE2_MIRROR = 3;
constexpr int PLAYER_ATTACK_PHASE_CAST = 0;
constexpr int PLAYER_ATTACK_PHASE_IMPACT = 1;
constexpr int PLAYER_ATTACK_HIT_SHAPE_NONE = 0;
constexpr int PLAYER_ATTACK_HIT_SHAPE_ORIENTED_BOX = 1;

struct GameRecordSummary
{
    float clearTimeSeconds;
    int totalBossDamage;
    int topDamage;
    char topDealerName[MAX_CHAT_NAME];
    char partySummary[MAX_RECORD_SUMMARY];
};

enum PacketID
{
    C_LOGIN = 1,
    S_LOGIN = 2,
    C_CHAT = 3,
    S_CHAT = 4,
    C_PLAYER_MOVE = 5,
    S_PLAYER_MOVE = 6,
    S_MONSTER_SYNC = 7,
    C_PLAYER_ATTACK = 8,
    S_MONSTER_HIT = 9,
    S_PLAYER_HIT = 10,
    S_PLAYER_ENTER = 11,
    S_PLAYER_LEAVE = 12,
    S_ROOM_INFO = 13,
    C_GAME_START = 14,
    S_GAME_START = 15,
    C_PLAYER_READY = 16,
    S_PLAYER_ATTACK = 17,
    C_LANTERN_GAUGE = 18,
    S_LANTERN_GAUGE = 19,
    C_WORLD_SHIFT = 20,
    S_WORLD_SHIFT = 21,
    C_DOOR_INTERACT = 22,
    S_DOOR_STATE = 23,
    C_PICKUP_COLLECT = 24,
    S_PICKUP_COLLECTED = 25,
    C_STAGE_CHANGE = 26,
    S_STAGE_CHANGE = 27,
    S_BOSS_PATTERN = 28,
    S_PLAYER_RESPAWN = 29,
    S_GAME_RESULT = 30,
    C_PLAYER_RESPAWN = 31,
    C_REGISTER = 32,
    S_REGISTER = 33
};

constexpr int GAME_RESULT_VICTORY = 1;

struct PacketHeader
{
    short size;
    short id;
};

struct PKT_S_PLAYER_ENTER {
    PacketHeader header;
    int playerId;
};

struct PKT_S_PLAYER_LEAVE {
    PacketHeader header;
    int playerId;
};

struct PKT_S_ROOM_INFO {
    PacketHeader header;
    int playerCount;
    int playerIds[3];
    bool readyStates[3];
    char playerNames[3][MAX_CHAT_NAME];
};

struct PKT_C_GAME_START {
    PacketHeader header;
};

struct PKT_S_GAME_START {
    PacketHeader header;
};

struct PKT_C_PLAYER_READY {
    PacketHeader header;
    bool ready;
};

struct PKT_C_PLAYER_ATTACK {
    PacketHeader header;
    int attackerId;
    int classType;
    int playerLevel;
    int targetMonsterId;
    float x, y, z;
    float rotY;
    int skillType; // 0 = 평타, 1 = 스킬1, 2 = 스킬2
    int attackPhase;
    float range;
    float radius;
    float coneDot;
    int hitShapeType;
    float hitboxCenterX;
    float hitboxCenterY;
    float hitboxCenterZ;
    float hitboxExtentX;
    float hitboxExtentY;
    float hitboxExtentZ;
    float hitboxOrientationX;
    float hitboxOrientationY;
    float hitboxOrientationZ;
    float hitboxOrientationW;
};

struct PKT_S_PLAYER_ATTACK {
    PacketHeader header;
    int playerId;
    int classType;
    int playerLevel;
    float x, y, z;
    float rotY;
    int skillType;
    int attackPhase;
    float effectX, effectY, effectZ;
    float effectRadius;
    float effectDelay;
};

struct PKT_S_MONSTER_HIT {
    PacketHeader header;
    int monsterId;
    int remainHp;
    int damage;
    int killerPlayerId;
    bool isDead;
};

struct PKT_C_LANTERN_GAUGE {
    PacketHeader header;
    float gauge;
    float maxGauge;
    int level;
};

struct PKT_S_LANTERN_GAUGE {
    PacketHeader header;
    int playerId;
    float gauge;
    float maxGauge;
    int level;
};

struct PKT_C_WORLD_SHIFT {
    PacketHeader header;
};

struct PKT_S_WORLD_SHIFT {
    PacketHeader header;
    int playerId;
};

struct PKT_C_DOOR_INTERACT {
    PacketHeader header;
    int doorId;
    bool isOpen;
};

struct PKT_S_DOOR_STATE {
    PacketHeader header;
    int doorId;
    bool isOpen;
};

struct PKT_C_PICKUP_COLLECT {
    PacketHeader header;
    int pickupId;
};

struct PKT_S_PICKUP_COLLECTED {
    PacketHeader header;
    int pickupId;
    int playerId;
};

struct PKT_C_STAGE_CHANGE {
    PacketHeader header;
    int targetStage;
};

struct PKT_S_STAGE_CHANGE {
    PacketHeader header;
    int playerId;
    int targetStage;
};

struct PKT_S_BOSS_PATTERN {
    PacketHeader header;
    int patternType;
    float x, y, z;
    float radius;
    float delay;
    int damage;
    int patternData;
};

struct PKT_S_PLAYER_HIT {
    PacketHeader header;
    int playerId;
    int remainHp;
    bool isDead;
    bool wasImmune;
};

struct PKT_S_PLAYER_RESPAWN {
    PacketHeader header;
    int playerId;
    float x, y, z;
    int remainHp;
    int classType;
    int playerLevel;
};

struct PKT_C_PLAYER_RESPAWN {
    PacketHeader header;
};

struct PKT_S_GAME_RESULT {
    PacketHeader header;
    int resultCode;
    float clearTimeSeconds;
    int playerCount;
    int playerIds[MAX_LOBBY_PLAYERS];
    int bossDamageDealt[MAX_LOBBY_PLAYERS];
    char playerNames[MAX_LOBBY_PLAYERS][MAX_CHAT_NAME];
    int currentRecordRank;
    int recordCount;
    GameRecordSummary records[MAX_GAME_RECORDS];
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
    int myPlayerId; // �� ���� �ڵ忡 �°� playerId -> myPlayerId �� ����!
};

struct PKT_C_REGISTER
{
    PacketHeader header;
    char id[20];
    char password[20];
};

struct PKT_S_REGISTER
{
    PacketHeader header;
    bool success;
};

// -------------------------------------------------
// [ä��] - ���� ���� �ذ��� ���� �߰�
// -------------------------------------------------
struct PKT_C_CHAT
{
    PacketHeader header;
    char msg[100]; // �˳��ϰ� 100����Ʈ �Ҵ�
};

struct PKT_S_CHAT
{
    PacketHeader header;
    int playerId;  // ���� ���´���
    char senderName[MAX_CHAT_NAME];
    char msg[100];
};

// -------------------------------------------------
// [�̵�]
// -------------------------------------------------
struct PKT_C_PLAYER_MOVE
{
    PacketHeader header;
    float x;
    float y;
    float z;
    float rotY;
    int animationState;
    int classType;
    int playerLevel;
};

struct PKT_S_PLAYER_MOVE
{
    PacketHeader header;
    int playerId;
    float x;
    float y;
    float z;
    float rotY;
    int animationState;
    int classType;
    int playerLevel;
};

struct PKT_S_MONSTER_SYNC
{
    PacketHeader header;
    int monsterId;
    int monsterType;
    int state;
    int attackSequence;
    float x, y, z;
    float rotY;
    int remainHp;
    bool isDead;
};
#pragma pack(pop)
