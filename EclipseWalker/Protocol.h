#pragma once

#pragma pack(push, 1)

constexpr int MAX_LOBBY_PLAYERS = 3;
constexpr int MAX_CHAT_NAME = 20;
constexpr int MAX_GAME_RECORDS = 10;
constexpr int MAX_RECORD_SUMMARY = 96;
constexpr int MAX_ROOM_TITLE = 32;
constexpr int MAX_ROOM_LIST_ROOMS = 12;
constexpr int STAGE2_BOSS_MONSTER_ID = 1001;
constexpr int STAGE2_BOSS_MONSTER_TYPE = 100;
constexpr int BOSS_PATTERN_STAGE2_SHOCKWAVE = 1;
constexpr int BOSS_PATTERN_STAGE2_WIPE = 2;
constexpr int BOSS_PATTERN_STAGE2_MIRROR = 3;
constexpr int BOSS_ATTACK_NONE = 0;
constexpr int BOSS_ATTACK_TWO_HIT_COMBO = 1;
constexpr int BOSS_ATTACK_THREE_HIT_COMBO = 2;
constexpr int BOSS_ATTACK_SWORD_ATTACK2 = 3;
constexpr int BOSS_ATTACK_WHIP_ATTACK = 4;
constexpr int BOSS_PHASE_IDLE = 0;
constexpr int BOSS_PHASE_CHASE = 1;
constexpr int BOSS_PHASE_ATTACK = 2;
constexpr int BOSS_PHASE_RECOVER = 3;
constexpr int BOSS_PHASE_REPOSITION = 4;
constexpr int PLAYER_ATTACK_PHASE_CAST = 0;
constexpr int PLAYER_ATTACK_PHASE_IMPACT = 1;
constexpr int PLAYER_ATTACK_HIT_SHAPE_NONE = 0;
constexpr int PLAYER_ATTACK_HIT_SHAPE_ORIENTED_BOX = 1;
constexpr float STAGE2_ECLIPSE_DURATION_SECONDS = 180.0f;
constexpr int PLAYER_SCENE_VILLAGE = 0;
constexpr int PLAYER_SCENE_STAGE1 = 1;
constexpr int PLAYER_SCENE_STAGE2 = 2;
constexpr int GOLD_PICKUP_STAGE1_GROUP = 10001;
constexpr int GOLD_PICKUP_STAGE2_GROUP = 20001;
constexpr int SHOP_CATEGORY_WEAPON = 1;
constexpr int SHOP_CATEGORY_ARMOR = 2;
constexpr int SHOP_CATEGORY_POTION = 3;
constexpr int POTION_TYPE_EMPTY = 0;
constexpr int POTION_TYPE_HP_SMALL = 1;
constexpr int POTION_TYPE_HP_MEDIUM = 2;
constexpr int POTION_TYPE_MP_SMALL = 3;
constexpr int POTION_TYPE_MP_MEDIUM = 4;
constexpr int POTION_TYPE_BATTLE_ELIXIR = 5;

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
    S_REGISTER = 33,
    C_ROOM_LIST = 34,
    S_ROOM_LIST = 35,
    C_CREATE_ROOM = 36,
    S_CREATE_ROOM = 37,
    C_JOIN_ROOM = 38,
    S_JOIN_ROOM = 39,
    C_LEAVE_ROOM = 40,
    S_LEAVE_ROOM = 41,
    C_INTERACT_PORTAL = 42,
    C_GOLD_PICKUP = 43,
    S_GOLD_UPDATE = 44,
    C_SHOP_PURCHASE = 45,
    S_SHOP_PURCHASE = 46,
    C_POTION_USE = 47,
    S_POTION_STATE = 48,
    S_STAGE2_BOSS_INTRO_CUTSCENE = 49
};

constexpr int GAME_RESULT_VICTORY = 1;
constexpr int GAME_RESULT_DEFEAT = 2;

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
    int roomId;
    int playerCount;
    char roomTitle[MAX_ROOM_TITLE];
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
    int weaponTier;
    int armorTier;
    int currentScene;
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
    int hitSequence;
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
    float stageElapsedSeconds;
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

struct PKT_S_STAGE2_BOSS_INTRO_CUTSCENE {
    PacketHeader header;
    int triggerPlayerId;
    float durationSeconds;
};

struct PKT_S_PLAYER_HIT {
    PacketHeader header;
    int playerId;
    int remainHp;
    int playerLevel;
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
    int weaponTier;
    int armorTier;
    int currentScene;
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

struct RoomListEntry
{
    int roomId;
    int playerCount;
    int maxPlayers;
    bool inGame;
    char title[MAX_ROOM_TITLE];
};

struct PKT_C_ROOM_LIST
{
    PacketHeader header;
};

struct PKT_S_ROOM_LIST
{
    PacketHeader header;
    int roomCount;
    RoomListEntry rooms[MAX_ROOM_LIST_ROOMS];
};

struct PKT_C_CREATE_ROOM
{
    PacketHeader header;
    char title[MAX_ROOM_TITLE];
};

struct PKT_S_CREATE_ROOM
{
    PacketHeader header;
    bool success;
    int roomId;
};

struct PKT_C_JOIN_ROOM
{
    PacketHeader header;
    int roomId;
};

struct PKT_S_JOIN_ROOM
{
    PacketHeader header;
    bool success;
    int roomId;
};

struct PKT_C_LEAVE_ROOM
{
    PacketHeader header;
};

struct PKT_S_LEAVE_ROOM
{
    PacketHeader header;
    bool success;
};

struct PKT_C_INTERACT_PORTAL
{
    PacketHeader header;
};

struct PKT_C_GOLD_PICKUP
{
    PacketHeader header;
    int pickupGroupId;
    float x;
    float y;
    float z;
    float radius;
};

struct PKT_S_GOLD_UPDATE
{
    PacketHeader header;
    int playerId;
    int gold;
    int pickupGroupId;
    bool pickupCollected;
};

struct PKT_C_SHOP_PURCHASE
{
    PacketHeader header;
    int shopItemId;
};

struct PKT_S_SHOP_PURCHASE
{
    PacketHeader header;
    bool success;
    int shopItemId;
    int category;
    int gold;
    int weaponTier;
    int armorTier;
    int potionSlots[3];
    int reasonCode;
};

struct PKT_C_POTION_USE
{
    PacketHeader header;
    int slotIndex;
};

struct PKT_S_POTION_STATE
{
    PacketHeader header;
    int playerId;
    bool success;
    int slotIndex;
    int potionType;
    int potionSlots[3];
    float cooldowns[3];
    int remainHp;
    float mpRestoreAmount;
    bool battleElixirActive;
    float battleElixirRemainingSeconds;
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
    int weaponTier;
    int armorTier;
    int currentScene;
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
    int weaponTier;
    int armorTier;
    int currentScene;
};

struct PKT_S_MONSTER_SYNC
{
    PacketHeader header;
    int monsterId;
    int monsterType;
    int state;
    int attackSequence;
    int targetPlayerId;
    int attackType;
    int actionPhase;
    float x, y, z;
    float rotY;
    int remainHp;
    bool isDead;
};
#pragma pack(pop)
