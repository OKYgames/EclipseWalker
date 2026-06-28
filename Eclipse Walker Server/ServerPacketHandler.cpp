#include "ServerPacketHandler.h"
#include "Session.h"
#include "GlobalQueue.h"
#include "Room.h"
#include "DBConnection.h"
#include <DirectXCollision.h>
#include <DirectXMath.h>
#include <algorithm>
#include <atomic>
#include <cmath>

namespace
{
    constexpr bool kEnableDbLogin = true;
    constexpr bool kEnableDebugLogin = false;
    constexpr const char* kDebugLoginId = "debug_user";
    constexpr const char* kDebugLoginPassword = "debug_pw";
    constexpr float kLanternPickupCharge = 35.0f;
    constexpr float kMaxPlayerMoveSpeed = 18.0f;
    constexpr float kPlayerMoveBurstDistance = 1.25f;
    constexpr float kMaxPlayerWorldCoordinate = 500.0f;
    std::atomic<int> gNextDebugPlayerId = 1000;

    bool IsDebugLogin(const std::string& id, const std::string& password)
    {
        return kEnableDebugLogin && id == kDebugLoginId && password == kDebugLoginPassword;
    }

    std::string ReadPacketString(const char* text, size_t maxLength)
    {
        size_t length = 0;
        while (length < maxLength && text[length] != '\0')
        {
            ++length;
        }

        return std::string(text, length);
    }
}

namespace
{
    constexpr float kMonsterHitRadius = 0.45f;
    constexpr int kMageHealingLightClassType = 1;
    constexpr int kMageHealingLightSkillType = 1;
    constexpr int kMageHealingLightAmount = 100;
    constexpr float kMageHealingLightRadius = 8.0f;
    constexpr float kArcherWindImbuementDuration = 6.0f;
    constexpr float kArcherWindImbuementAttackSpeedMultiplier = 1.45f;
    constexpr int kArcherArrowRainImpactCount = 3;
    constexpr int kArcherArrowRainDamagePerImpact = 17;

    struct ServerAttackProfile
    {
        float range;
        float halfWidth;
        float coneDot;
        float verticalTolerance;
        int damage;
        float cooldownSeconds;
    };

    int GetBasicAttackDamageForLevel(int playerLevel)
    {
        switch (playerLevel)
        {
        case 2:
            return 15;
        case 3:
            return 20;
        case 1:
        default:
            return 10;
        }
    }

    bool TryGetServerAttackProfile(int classType, int playerLevel, int skillType, ServerAttackProfile& outProfile)
    {
        if (skillType < 0 || skillType > 2)
        {
            return false;
        }

        const int basicDamage = GetBasicAttackDamageForLevel(playerLevel);

        switch (classType)
        {
        case 0: // Warrior
            if (skillType == 0) outProfile = { 0.46f, 0.48f, 0.55f, 3.0f, basicDamage, 0.28f };
            else if (skillType == 1) outProfile = { 0.76f, 0.90f, 0.35f, 3.0f, 35, 6.00f };
            else outProfile = { 1.20f, 1.20f, -1.0f, 3.0f, 45, 10.00f };
            return true;

        case 1: // Mage
            if (skillType == 0) outProfile = { 2.00f, 0.50f, 0.55f, 3.0f, basicDamage, 0.28f };
            else if (skillType == 1) outProfile = { kMageHealingLightRadius, kMageHealingLightRadius, -1.0f, 4.0f, 0, 6.00f };
            else outProfile = { 2.85f, 2.85f, -1.0f, 3.0f, 60, 12.00f };
            return true;

        case 2: // Archer
            if (skillType == 0) outProfile = { 2.40f, 0.72f, 0.91f, 3.0f, basicDamage, 0.28f };
            else if (skillType == 1) outProfile = { 3.00f, 0.50f, 0.60f, 3.0f, 25, 8.00f };
            else outProfile = { 3.60f, 0.60f, 0.50f, 3.0f, kArcherArrowRainDamagePerImpact, 10.00f };
            return true;

        default:
            return false;
        }
    }

    std::string GetSessionDisplayName(const std::shared_ptr<Session>& session)
    {
        if (session == nullptr)
        {
            return "Unknown";
        }

        const std::string& displayName = session->GetDisplayName();
        if (!displayName.empty())
        {
            return displayName;
        }

        return "Player " + std::to_string(session->GetPlayerId());
    }

    bool IsMonsterInsideAttack(float attackerX, float attackerY, float attackerZ, float attackRotY, const MonsterSnapshot& monster, const ServerAttackProfile& profile)
    {
        if (fabsf(monster.y - attackerY) > profile.verticalTolerance)
        {
            return false;
        }

        const float monsterHitRadius = (monster.monsterId == STAGE2_BOSS_MONSTER_ID) ? 1.25f : kMonsterHitRadius;
        const float dx = monster.x - attackerX;
        const float dz = monster.z - attackerZ;
        const float maxRange = profile.range + monsterHitRadius;
        const float distanceSq = (dx * dx) + (dz * dz);
        if (distanceSq > maxRange * maxRange)
        {
            return false;
        }

        if (profile.coneDot <= -0.999f)
        {
            return true;
        }

        const float distance = sqrtf(distanceSq);
        const float forwardX = sinf(attackRotY);
        const float forwardZ = cosf(attackRotY);
        const float dirX = (distance > 0.001f) ? (dx / distance) : forwardX;
        const float dirZ = (distance > 0.001f) ? (dz / distance) : forwardZ;
        const float dot = (dirX * forwardX) + (dirZ * forwardZ);
        if (dot < profile.coneDot)
        {
            return false;
        }

        const float projected = (dx * forwardX) + (dz * forwardZ);
        if (projected < 0.0f || projected > profile.range + monsterHitRadius)
        {
            return false;
        }

        const float sideX = dx - (forwardX * projected);
        const float sideZ = dz - (forwardZ * projected);
        const float sideLimit = profile.halfWidth + monsterHitRadius;
        return ((sideX * sideX) + (sideZ * sideZ)) <= (sideLimit * sideLimit);
    }

    bool IsMageHealingLight(int classType, int skillType)
    {
        return classType == kMageHealingLightClassType &&
            skillType == kMageHealingLightSkillType;
    }

    int GetExpectedImpactCount(int classType, int skillType)
    {
        return (classType == 2 && skillType == 2) ? kArcherArrowRainImpactCount : 1;
    }

    int GetAppliedMonsterDamage(int classType, int skillType, int baseDamage, int monsterId)
    {
        (void)classType;
        (void)skillType;
        (void)monsterId;
        return baseDamage;
    }

    const MonsterSnapshot* FindLiveMonsterSnapshot(const std::vector<MonsterSnapshot>& snapshots, int monsterId)
    {
        if (monsterId <= 0)
        {
            return nullptr;
        }

        auto it = std::find_if(
            snapshots.begin(),
            snapshots.end(),
            [monsterId](const MonsterSnapshot& monster)
            {
                return monster.monsterId == monsterId && monster.state != 3;
            });

        return it != snapshots.end() ? &(*it) : nullptr;
    }

    float GetSkillVisualRadius(int classType, int skillType, const ServerAttackProfile& profile)
    {
        if (classType == 0 && skillType == 1) return 2.4f;
        if (classType == 0 && skillType == 2) return 1.2f;
        if (classType == 1 && skillType == 1) return kMageHealingLightRadius;
        if (classType == 1 && skillType == 2) return 2.85f;
        if (classType == 2 && skillType == 1) return 3.0f;
        if (classType == 2 && skillType == 2) return 2.35f;
        return (std::max)(profile.range, profile.halfWidth);
    }

    float GetSkillPreviewDelay(int classType, int skillType)
    {
        if (classType == 0 && skillType == 2) return 1.35f;
        if (classType == 1 && skillType == 2) return 1.15f;
        if (classType == 2 && skillType == 2) return 0.72f;
        return 0.0f;
    }

    bool UsesTargetedAreaEffect(int classType, int skillType)
    {
        return skillType == 2 &&
            (classType == 0 || classType == 1 || classType == 2);
    }

    float GetMonsterSkillEffectY(const MonsterSnapshot& monster)
    {
        constexpr float kEffectGroundLift = 0.02f;
        constexpr float kStage2BossFloorOffset = 1.7f;

        if (monster.monsterId == STAGE2_BOSS_MONSTER_ID)
        {
            return monster.y - kStage2BossFloorOffset + kEffectGroundLift;
        }

        return monster.y;
    }

    void SetSkillEffectCenterFromMonster(
        const MonsterSnapshot& monster,
        float& effectX,
        float& effectY,
        float& effectZ)
    {
        effectX = monster.x;
        effectY = GetMonsterSkillEffectY(monster);
        effectZ = monster.z;
    }

    void ResolveSkillEffectCenter(
        int classType,
        int skillType,
        int targetMonsterId,
        float attackX,
        float attackY,
        float attackZ,
        float attackRotY,
        const ServerAttackProfile& profile,
        const std::vector<MonsterSnapshot>& snapshots,
        float& effectX,
        float& effectY,
        float& effectZ)
    {
        effectX = attackX;
        effectY = attackY;
        effectZ = attackZ;

        if (!UsesTargetedAreaEffect(classType, skillType))
        {
            return;
        }

        if (const MonsterSnapshot* target = FindLiveMonsterSnapshot(snapshots, targetMonsterId))
        {
            SetSkillEffectCenterFromMonster(*target, effectX, effectY, effectZ);
            return;
        }

        // Targeted area skills already send the intended ground impact point in attackX/Y/Z.
        // If the original target has moved or died, keep that ground point instead of
        // shifting the center forward again.
        if (UsesTargetedAreaEffect(classType, skillType))
        {
            return;
        }

        effectX += sinf(attackRotY) * profile.range;
        effectZ += cosf(attackRotY) * profile.range;

        for (const auto& monster : snapshots)
        {
            if (monster.state == 3) continue;

            if (IsMonsterInsideAttack(attackX, attackY, attackZ, attackRotY, monster, profile))
            {
                SetSkillEffectCenterFromMonster(monster, effectX, effectY, effectZ);
                return;
            }
        }
    }

    bool IsFiniteAttackTransform(const PKT_C_PLAYER_ATTACK& pkt)
    {
        return std::isfinite(pkt.x) &&
            std::isfinite(pkt.y) &&
            std::isfinite(pkt.z) &&
            std::isfinite(pkt.rotY);
    }

    bool IsValidAttackOrigin(const PKT_C_PLAYER_ATTACK& pkt)
    {
        return IsFiniteAttackTransform(pkt) &&
            std::fabs(pkt.x) <= kMaxPlayerWorldCoordinate &&
            std::fabs(pkt.y) <= kMaxPlayerWorldCoordinate &&
            std::fabs(pkt.z) <= kMaxPlayerWorldCoordinate;
    }

    float ClampedPositiveOrDefault(float value, float fallback, float minValue, float maxValue)
    {
        if (!std::isfinite(value) || value <= 0.0f)
        {
            return fallback;
        }

        return (std::min)((std::max)(value, minValue), maxValue);
    }

    void BroadcastMonsterHit(int monsterId, int damage, int attackerPlayerId);

    DirectX::XMFLOAT3 GetMonsterHurtboxExtents(const MonsterSnapshot& monster)
    {
        if (monster.monsterId == STAGE2_BOSS_MONSTER_ID || monster.type == STAGE2_BOSS_MONSTER_TYPE)
        {
            return { 2.45f, 2.65f, 2.45f };
        }

        switch (monster.type)
        {
        case 0: // Server skeleton archer
        case 1: // Client enum fallback: REAL_SKELETON_ARCHER
            return { 0.82f, 1.28f, 0.82f };

        case 2: // Server skeleton sword / client REAL_SKELETON_SWORD
            return { 0.88f, 1.34f, 0.88f };

        case 3: // Client enum fallback: SPECTRAL_BRAWLER
            return { 0.64f, 0.96f, 0.64f };

        case 4: // Client enum fallback: SPECTRAL_ARCHER
            return { 0.58f, 0.90f, 0.58f };

        case 5: // Client enum fallback: SPECTRAL_IMP
            return { 0.46f, 0.72f, 0.46f };

        default:
            return { 0.82f, 1.28f, 0.82f };
        }
    }

    bool TryBuildClientOrientedHitbox(const PKT_C_PLAYER_ATTACK& pkt, DirectX::BoundingOrientedBox& outHitbox)
    {
        if (pkt.hitShapeType != PLAYER_ATTACK_HIT_SHAPE_ORIENTED_BOX)
        {
            return false;
        }

        const bool hasFiniteValues =
            std::isfinite(pkt.hitboxCenterX) &&
            std::isfinite(pkt.hitboxCenterY) &&
            std::isfinite(pkt.hitboxCenterZ) &&
            std::isfinite(pkt.hitboxExtentX) &&
            std::isfinite(pkt.hitboxExtentY) &&
            std::isfinite(pkt.hitboxExtentZ) &&
            std::isfinite(pkt.hitboxOrientationX) &&
            std::isfinite(pkt.hitboxOrientationY) &&
            std::isfinite(pkt.hitboxOrientationZ) &&
            std::isfinite(pkt.hitboxOrientationW);
        if (!hasFiniteValues)
        {
            return false;
        }

        if (std::fabs(pkt.hitboxCenterX) > kMaxPlayerWorldCoordinate ||
            std::fabs(pkt.hitboxCenterY) > kMaxPlayerWorldCoordinate ||
            std::fabs(pkt.hitboxCenterZ) > kMaxPlayerWorldCoordinate)
        {
            return false;
        }

        constexpr float kMinHitboxExtent = 0.01f;
        constexpr float kMaxHitboxExtent = 5.0f;
        if (pkt.hitboxExtentX < kMinHitboxExtent || pkt.hitboxExtentX > kMaxHitboxExtent ||
            pkt.hitboxExtentY < kMinHitboxExtent || pkt.hitboxExtentY > kMaxHitboxExtent ||
            pkt.hitboxExtentZ < kMinHitboxExtent || pkt.hitboxExtentZ > kMaxHitboxExtent)
        {
            return false;
        }

        DirectX::XMVECTOR orientation = DirectX::XMVectorSet(
            pkt.hitboxOrientationX,
            pkt.hitboxOrientationY,
            pkt.hitboxOrientationZ,
            pkt.hitboxOrientationW);
        const float orientationLengthSq = DirectX::XMVectorGetX(DirectX::XMVector4LengthSq(orientation));
        if (!std::isfinite(orientationLengthSq) || orientationLengthSq <= 0.000001f)
        {
            return false;
        }

        orientation = DirectX::XMQuaternionNormalize(orientation);
        outHitbox.Center = { pkt.hitboxCenterX, pkt.hitboxCenterY, pkt.hitboxCenterZ };
        outHitbox.Extents = { pkt.hitboxExtentX, pkt.hitboxExtentY, pkt.hitboxExtentZ };
        DirectX::XMStoreFloat4(&outHitbox.Orientation, orientation);
        return true;
    }

    bool TryApplyClientOrientedHitboxAttack(
        const PKT_C_PLAYER_ATTACK& pkt,
        const std::vector<MonsterSnapshot>& snapshots,
        int classType,
        int skillType,
        int damage,
        int attackerPlayerId)
    {
        DirectX::BoundingOrientedBox attackHitbox;
        if (!TryBuildClientOrientedHitbox(pkt, attackHitbox))
        {
            return false;
        }

        for (const MonsterSnapshot& monster : snapshots)
        {
            if (monster.state == 3)
            {
                continue;
            }

            DirectX::BoundingBox hurtbox;
            hurtbox.Center = { monster.x, monster.y, monster.z };
            hurtbox.Extents = GetMonsterHurtboxExtents(monster);
            if (attackHitbox.Intersects(hurtbox))
            {
                BroadcastMonsterHit(
                    monster.monsterId,
                    GetAppliedMonsterDamage(classType, skillType, damage, monster.monsterId),
                    attackerPlayerId);
            }
        }

        return true;
    }

    void BroadcastMonsterHit(int monsterId, int damage, int attackerPlayerId)
    {
        int appliedDamage = 0;
        const bool isDead = G_Room->ApplyDamageToMonster(monsterId, damage, attackerPlayerId, &appliedDamage);
        if (appliedDamage <= 0)
        {
            return;
        }

        PKT_S_MONSTER_HIT hitPkt = {};
        hitPkt.header.size = sizeof(PKT_S_MONSTER_HIT);
        hitPkt.header.id = PacketID::S_MONSTER_HIT;
        hitPkt.monsterId = monsterId;
        hitPkt.remainHp = G_Room->GetMonsterHp(monsterId);
        hitPkt.damage = appliedDamage;
        hitPkt.killerPlayerId = isDead ? attackerPlayerId : -1;
        hitPkt.isDead = isDead;

        G_Room->Broadcast(&hitPkt, sizeof(hitPkt));

        if (isDead && monsterId == STAGE2_BOSS_MONSTER_ID && G_Room->CompleteStage2Boss())
        {
            PKT_S_GAME_RESULT resultPkt = {};
            resultPkt.header.size = sizeof(PKT_S_GAME_RESULT);
            resultPkt.header.id = PacketID::S_GAME_RESULT;
            resultPkt.resultCode = GAME_RESULT_VICTORY;
            G_Room->FillStage2GameResultPacket(resultPkt);
            G_Room->Broadcast(&resultPkt, sizeof(resultPkt));
        }
    }

    void BroadcastLanternState(const std::shared_ptr<Session>& session)
    {
        if (session == nullptr || G_Room == nullptr)
        {
            return;
        }

        G_Room->BroadcastLanternStates();
    }

}

void ServerPacketHandler::HandlePacket(std::shared_ptr<Session> session, BYTE* buffer, int len)
{
    PacketHeader* header = reinterpret_cast<PacketHeader*>(buffer);

    switch (header->id)
    {
    case PacketID::C_LOGIN:
    {
        if (len < sizeof(PKT_C_LOGIN)) break;
        PKT_C_LOGIN* pkt = reinterpret_cast<PKT_C_LOGIN*>(buffer);
        Handle_C_LOGIN(session, *pkt);
    }
    break;

    case PacketID::C_REGISTER:
    {
        if (len < sizeof(PKT_C_REGISTER)) break;
        PKT_C_REGISTER* pkt = reinterpret_cast<PKT_C_REGISTER*>(buffer);
        Handle_C_REGISTER(session, *pkt);
    }
    break;

    case PacketID::C_CHAT:
    {
        if (len < sizeof(PKT_C_CHAT)) break;
        PKT_C_CHAT* pkt = reinterpret_cast<PKT_C_CHAT*>(buffer);
        Handle_C_CHAT(session, *pkt);
    }
    break;

    case PacketID::C_PLAYER_MOVE:
    {
        if (len < sizeof(PKT_C_PLAYER_MOVE)) break;
        PKT_C_PLAYER_MOVE* pkt = reinterpret_cast<PKT_C_PLAYER_MOVE*>(buffer);
        Handle_C_PLAYER_MOVE(session, *pkt);
    }
    break;
    case PacketID::C_PLAYER_ATTACK:
    {
        if (len < sizeof(PKT_C_PLAYER_ATTACK)) break;
        PKT_C_PLAYER_ATTACK* pkt = reinterpret_cast<PKT_C_PLAYER_ATTACK*>(buffer);
        Handle_C_PLAYER_ATTACK(session, *pkt);
    }
    break;

    case PacketID::C_GAME_START:
    {
        if (len < sizeof(PKT_C_GAME_START)) break;
        PKT_C_GAME_START* pkt = reinterpret_cast<PKT_C_GAME_START*>(buffer);
        Handle_C_GAME_START(session, *pkt);
    }
    break;

    case PacketID::C_PLAYER_READY:
    {
        if (len < sizeof(PKT_C_PLAYER_READY)) break;
        PKT_C_PLAYER_READY* pkt = reinterpret_cast<PKT_C_PLAYER_READY*>(buffer);
        Handle_C_PLAYER_READY(session, *pkt);
    }
    break;

    case PacketID::C_LANTERN_GAUGE:
    {
        if (len < sizeof(PKT_C_LANTERN_GAUGE)) break;
        PKT_C_LANTERN_GAUGE* pkt = reinterpret_cast<PKT_C_LANTERN_GAUGE*>(buffer);
        Handle_C_LANTERN_GAUGE(session, *pkt);
    }
    break;

    case PacketID::C_WORLD_SHIFT:
    {
        if (len < sizeof(PKT_C_WORLD_SHIFT)) break;
        PKT_C_WORLD_SHIFT* pkt = reinterpret_cast<PKT_C_WORLD_SHIFT*>(buffer);
        Handle_C_WORLD_SHIFT(session, *pkt);
    }
    break;

    case PacketID::C_DOOR_INTERACT:
    {
        if (len < sizeof(PKT_C_DOOR_INTERACT)) break;
        PKT_C_DOOR_INTERACT* pkt = reinterpret_cast<PKT_C_DOOR_INTERACT*>(buffer);
        Handle_C_DOOR_INTERACT(session, *pkt);
    }
    break;

    case PacketID::C_PICKUP_COLLECT:
    {
        if (len < sizeof(PKT_C_PICKUP_COLLECT)) break;
        PKT_C_PICKUP_COLLECT* pkt = reinterpret_cast<PKT_C_PICKUP_COLLECT*>(buffer);
        Handle_C_PICKUP_COLLECT(session, *pkt);
    }
    break;

    case PacketID::C_STAGE_CHANGE:
    {
        if (len < sizeof(PKT_C_STAGE_CHANGE)) break;
        PKT_C_STAGE_CHANGE* pkt = reinterpret_cast<PKT_C_STAGE_CHANGE*>(buffer);
        Handle_C_STAGE_CHANGE(session, *pkt);
    }
    break;

    case PacketID::C_PLAYER_RESPAWN:
    {
        if (len < sizeof(PKT_C_PLAYER_RESPAWN)) break;
        PKT_C_PLAYER_RESPAWN* pkt = reinterpret_cast<PKT_C_PLAYER_RESPAWN*>(buffer);
        Handle_C_PLAYER_RESPAWN(session, *pkt);
    }
    break;

    default:
        std::cout << "Unknown Packet ID: " << header->id << std::endl;
        break;
    }
}

void ServerPacketHandler::Handle_C_PLAYER_RESPAWN(std::shared_ptr<Session> session, PKT_C_PLAYER_RESPAWN& pkt)
{
    (void)pkt;

    G_JobQueue->Push([session]()
        {
            if (session == nullptr || G_Room == nullptr)
            {
                return;
            }

            G_Room->RequestPlayerRespawn(session);
        });
}

void ServerPacketHandler::Handle_C_PLAYER_ATTACK(std::shared_ptr<Session> session, PKT_C_PLAYER_ATTACK& pkt)
{
    PKT_C_PLAYER_ATTACK pktCopy = pkt;

    G_JobQueue->Push([session, pktCopy]()
        {
            std::cout << "[Logic Thread] Player Attack - skillType: " << pktCopy.skillType << std::endl;

            if (session == nullptr || session->GetPlayerId() <= 0)
            {
                return;
            }

            if (G_Room == nullptr)
            {
                return;
            }

            if (!G_Room->IsCombatActive())
            {
                return;
            }

            bool playerHpChanged = false;
            if (!session->RegisterPlayerClass(pktCopy.classType, &playerHpChanged) ||
                !session->RegisterPlayerLevel(pktCopy.playerLevel))
            {
                return;
            }

            if (playerHpChanged)
            {
                G_Room->BroadcastPlayerHp(session);
            }

            const int playerClassType = session->GetPlayerClassType();
            const int playerLevel = session->GetPlayerLevel();
            ServerAttackProfile profile = {};
            if (!TryGetServerAttackProfile(playerClassType, playerLevel, pktCopy.skillType, profile))
            {
                return;
            }
            const bool hasValidClientAttackOrigin = IsValidAttackOrigin(pktCopy);

            if (pktCopy.attackPhase == PLAYER_ATTACK_PHASE_CAST)
            {
                const float attackCooldownSeconds =
                    (playerClassType == 2 &&
                        pktCopy.skillType == 0 &&
                        session->HasArcherAttackSpeedBuff())
                    ? (profile.cooldownSeconds / kArcherWindImbuementAttackSpeedMultiplier)
                    : profile.cooldownSeconds;
                if (!session->TryBeginPlayerAttack(
                    pktCopy.skillType,
                    attackCooldownSeconds,
                    GetExpectedImpactCount(playerClassType, pktCopy.skillType)))
                {
                    return;
                }

                if (playerClassType == 2 && pktCopy.skillType == 1)
                {
                    session->ActivateArcherAttackSpeedBuff(kArcherWindImbuementDuration);
                }

                const float castX = hasValidClientAttackOrigin ? pktCopy.x : session->GetX();
                const float castY = hasValidClientAttackOrigin ? pktCopy.y : session->GetY();
                const float castZ = hasValidClientAttackOrigin ? pktCopy.z : session->GetZ();
                const float castRotY = hasValidClientAttackOrigin
                    ? std::remainder(pktCopy.rotY, 2.0f * 3.14159265f)
                    : session->GetRotY();
                float effectX = castX;
                float effectY = castY;
                float effectZ = castZ;
                const auto snapshots = G_Room->GetMonsterSnapshots();
                ResolveSkillEffectCenter(
                    playerClassType,
                    pktCopy.skillType,
                    pktCopy.targetMonsterId,
                    castX,
                    castY,
                    castZ,
                    castRotY,
                    profile,
                    snapshots,
                    effectX,
                    effectY,
                    effectZ);

                PKT_S_PLAYER_ATTACK castPkt = {};
                castPkt.header.size = sizeof(PKT_S_PLAYER_ATTACK);
                castPkt.header.id = PacketID::S_PLAYER_ATTACK;
                castPkt.playerId = session->GetPlayerId();
                castPkt.classType = playerClassType;
                castPkt.playerLevel = playerLevel;
                castPkt.x = castX;
                castPkt.y = castY;
                castPkt.z = castZ;
                castPkt.rotY = castRotY;
                castPkt.skillType = pktCopy.skillType;
                castPkt.attackPhase = PLAYER_ATTACK_PHASE_CAST;
                castPkt.effectX = effectX;
                castPkt.effectY = effectY;
                castPkt.effectZ = effectZ;
                castPkt.effectRadius = (playerClassType == 2 && pktCopy.skillType == 0)
                    ? ClampedPositiveOrDefault(pktCopy.range, (std::max)(profile.range * 2.5f, 6.0f), 3.0f, 30.0f)
                    : GetSkillVisualRadius(playerClassType, pktCopy.skillType, profile);
                castPkt.effectDelay = (playerClassType == 2 && pktCopy.skillType == 0)
                    ? ClampedPositiveOrDefault(pktCopy.radius, 0.0f, 0.0f, 2.0f)
                    : GetSkillPreviewDelay(playerClassType, pktCopy.skillType);
                G_Room->BroadcastExcept(session, &castPkt, sizeof(castPkt));

                if (IsMageHealingLight(playerClassType, pktCopy.skillType))
                {
                    G_Room->HealPlayersAround(
                        session->GetPlayerId(),
                        castX,
                        castY,
                        castZ,
                        kMageHealingLightRadius,
                        kMageHealingLightAmount);
                    session->TryConsumePlayerAttackImpact(pktCopy.skillType);
                }
                return;
            }

            if (pktCopy.attackPhase != PLAYER_ATTACK_PHASE_IMPACT ||
                !session->TryConsumePlayerAttackImpact(pktCopy.skillType))
            {
                return;
            }

            if (IsMageHealingLight(playerClassType, pktCopy.skillType))
            {
                G_Room->HealPlayersAround(
                    session->GetPlayerId(),
                    session->GetX(),
                    session->GetY(),
                    session->GetZ(),
                    kMageHealingLightRadius,
                    kMageHealingLightAmount);
                return;
            }

            ServerAttackProfile hitProfile = profile;
            const bool useClientArcherHit =
                playerClassType == 2 &&
                hasValidClientAttackOrigin &&
                std::isfinite(pktCopy.range) &&
                std::isfinite(pktCopy.radius) &&
                std::isfinite(pktCopy.coneDot);
            const bool useClientMageBasicHit =
                playerClassType == 1 &&
                pktCopy.skillType == 0 &&
                hasValidClientAttackOrigin &&
                std::isfinite(pktCopy.range) &&
                std::isfinite(pktCopy.radius) &&
                std::isfinite(pktCopy.coneDot);
            const bool useClientWarriorWeaponHit =
                playerClassType == 0 &&
                pktCopy.skillType == 0 &&
                pktCopy.hitShapeType == PLAYER_ATTACK_HIT_SHAPE_ORIENTED_BOX;

            if (useClientArcherHit)
            {
                hitProfile.range = ClampedPositiveOrDefault(
                    pktCopy.range,
                    profile.range,
                    0.05f,
                    pktCopy.skillType == 0 ? 4.0f : (std::max)(profile.range, 4.0f));
                hitProfile.halfWidth = ClampedPositiveOrDefault(
                    pktCopy.radius,
                    profile.halfWidth,
                    0.05f,
                    3.0f);
                hitProfile.coneDot = (std::min)((std::max)(pktCopy.coneDot, -1.0f), 1.0f);
            }
            else if (useClientMageBasicHit)
            {
                hitProfile.range = ClampedPositiveOrDefault(
                    pktCopy.range,
                    profile.range,
                    0.05f,
                    12.0f);
                hitProfile.halfWidth = ClampedPositiveOrDefault(
                    pktCopy.radius,
                    profile.halfWidth,
                    0.05f,
                    2.0f);
                hitProfile.coneDot = (std::min)((std::max)(pktCopy.coneDot, -1.0f), 1.0f);
            }

            const bool useClientAttackTransform =
                useClientArcherHit ||
                useClientMageBasicHit ||
                (useClientWarriorWeaponHit && hasValidClientAttackOrigin);
            const float attackX = useClientAttackTransform ? pktCopy.x : session->GetX();
            const float attackY = useClientAttackTransform ? pktCopy.y : session->GetY();
            const float attackZ = useClientAttackTransform ? pktCopy.z : session->GetZ();
            const float attackRotY = useClientAttackTransform
                ? std::remainder(pktCopy.rotY, 2.0f * 3.14159265f)
                : session->GetRotY();

            float effectX = attackX;
            float effectY = attackY;
            float effectZ = attackZ;
            const auto snapshots = G_Room->GetMonsterSnapshots();
            ResolveSkillEffectCenter(
                playerClassType,
                pktCopy.skillType,
                pktCopy.targetMonsterId,
                attackX,
                attackY,
                attackZ,
                attackRotY,
                hitProfile,
                snapshots,
                effectX,
                effectY,
                effectZ);

            const bool useSkillEffectCenterForHit =
                UsesTargetedAreaEffect(playerClassType, pktCopy.skillType);
            const float hitCenterX = useSkillEffectCenterForHit ? effectX : attackX;
            const float hitCenterY = useSkillEffectCenterForHit ? effectY : attackY;
            const float hitCenterZ = useSkillEffectCenterForHit ? effectZ : attackZ;

            PKT_S_PLAYER_ATTACK attackPkt = {};
            attackPkt.header.size = sizeof(PKT_S_PLAYER_ATTACK);
            attackPkt.header.id = PacketID::S_PLAYER_ATTACK;
            attackPkt.playerId = session->GetPlayerId();
            attackPkt.classType = playerClassType;
            attackPkt.playerLevel = playerLevel;
            attackPkt.x = attackX;
            attackPkt.y = attackY;
            attackPkt.z = attackZ;
            attackPkt.rotY = attackRotY;
            attackPkt.skillType = pktCopy.skillType;
            attackPkt.attackPhase = PLAYER_ATTACK_PHASE_IMPACT;
            attackPkt.effectX = effectX;
            attackPkt.effectY = effectY;
            attackPkt.effectZ = effectZ;
            attackPkt.effectRadius = GetSkillVisualRadius(playerClassType, pktCopy.skillType, hitProfile);
            attackPkt.effectDelay = 0.0f;
            G_Room->BroadcastExcept(session, &attackPkt, sizeof(attackPkt));

            if (IsMageHealingLight(playerClassType, pktCopy.skillType))
            {
                G_Room->HealPlayersAround(
                    session->GetPlayerId(),
                    attackX,
                    attackY,
                    attackZ,
                    kMageHealingLightRadius,
                    kMageHealingLightAmount);
                return;
            }

            // The server decides final hit results from the accepted attack origin and direction.
            if (G_Room != nullptr)
            {
                if (useClientWarriorWeaponHit &&
                    TryApplyClientOrientedHitboxAttack(
                        pktCopy,
                        snapshots,
                        playerClassType,
                        pktCopy.skillType,
                        hitProfile.damage,
                        session->GetPlayerId()))
                {
                    return;
                }

                bool directArcherTargetApplied = false;

                if (useClientArcherHit && pktCopy.targetMonsterId > 0)
                {
                    auto targetIt = std::find_if(
                        snapshots.begin(),
                        snapshots.end(),
                        [&pktCopy](const MonsterSnapshot& monster)
                        {
                            return monster.monsterId == pktCopy.targetMonsterId && monster.state != 3;
                        });

                    if (targetIt != snapshots.end())
                    {
                        BroadcastMonsterHit(
                            pktCopy.targetMonsterId,
                            GetAppliedMonsterDamage(playerClassType, pktCopy.skillType, hitProfile.damage, pktCopy.targetMonsterId),
                            session->GetPlayerId());
                        directArcherTargetApplied = true;
                    }
                }

                for (auto& m : snapshots)
                {
                    if (m.state == 3) continue; // DIE 상태 제외

                    if (directArcherTargetApplied && m.monsterId == pktCopy.targetMonsterId) continue;

                    if (IsMonsterInsideAttack(hitCenterX, hitCenterY, hitCenterZ, attackRotY, m, hitProfile))
                    {
                        // 피해 적용 및 결과 브로드캐스트
                        BroadcastMonsterHit(
                            m.monsterId,
                            GetAppliedMonsterDamage(playerClassType, pktCopy.skillType, hitProfile.damage, m.monsterId),
                            session->GetPlayerId());
                    }
                }
            }
        });
}

void ServerPacketHandler::Handle_C_LANTERN_GAUGE(std::shared_ptr<Session> session, PKT_C_LANTERN_GAUGE& pkt)
{
    (void)pkt;

    G_JobQueue->Push([session]()
        {
            if (session == nullptr || session->GetPlayerId() <= 0 || G_Room == nullptr)
            {
                return;
            }

            BroadcastLanternState(session);
        });
}

void ServerPacketHandler::Handle_C_WORLD_SHIFT(std::shared_ptr<Session> session, PKT_C_WORLD_SHIFT& pkt)
{
    UNREFERENCED_PARAMETER(pkt);

    G_JobQueue->Push([session]()
        {
            if (session == nullptr || session->GetPlayerId() <= 0 || G_Room == nullptr)
            {
                return;
            }

            if (!session->CanUseWorldShift())
            {
                BroadcastLanternState(session);
                return;
            }

            G_Room->StartWorldShiftForAll(5.0f);
            G_Room->ConsumeLanternForAll();

            PKT_S_WORLD_SHIFT sendPkt = {};
            sendPkt.header.size = sizeof(PKT_S_WORLD_SHIFT);
            sendPkt.header.id = PacketID::S_WORLD_SHIFT;
            sendPkt.playerId = session->GetPlayerId();

            G_Room->Broadcast(&sendPkt, sizeof(sendPkt));
            BroadcastLanternState(session);
        });
}

void ServerPacketHandler::Handle_C_DOOR_INTERACT(std::shared_ptr<Session> session, PKT_C_DOOR_INTERACT& pkt)
{
    PKT_C_DOOR_INTERACT pktCopy = pkt;

    G_JobQueue->Push([session, pktCopy]()
        {
            if (session == nullptr || session->GetPlayerId() <= 0 || G_Room == nullptr)
            {
                return;
            }

            if (!G_Room->SetDoorOpen(pktCopy.doorId, pktCopy.isOpen))
            {
                return;
            }

            PKT_S_DOOR_STATE sendPkt = {};
            sendPkt.header.size = sizeof(PKT_S_DOOR_STATE);
            sendPkt.header.id = PacketID::S_DOOR_STATE;
            sendPkt.doorId = pktCopy.doorId;
            sendPkt.isOpen = G_Room->GetDoorOpen(pktCopy.doorId);

            G_Room->Broadcast(&sendPkt, sizeof(sendPkt));
        });
}

void ServerPacketHandler::Handle_C_PICKUP_COLLECT(std::shared_ptr<Session> session, PKT_C_PICKUP_COLLECT& pkt)
{
    PKT_C_PICKUP_COLLECT pktCopy = pkt;

    G_JobQueue->Push([session, pktCopy]()
        {
            if (session == nullptr || session->GetPlayerId() <= 0 || G_Room == nullptr)
            {
                return;
            }

            if (!G_Room->MarkPickupCollected(pktCopy.pickupId))
            {
                return;
            }

            PKT_S_PICKUP_COLLECTED sendPkt = {};
            sendPkt.header.size = sizeof(PKT_S_PICKUP_COLLECTED);
            sendPkt.header.id = PacketID::S_PICKUP_COLLECTED;
            sendPkt.pickupId = pktCopy.pickupId;
            sendPkt.playerId = session->GetPlayerId();

            G_Room->Broadcast(&sendPkt, sizeof(sendPkt));

            G_Room->AddLanternChargeForAll(kLanternPickupCharge);
            BroadcastLanternState(session);
        });
}

void ServerPacketHandler::Handle_C_STAGE_CHANGE(std::shared_ptr<Session> session, PKT_C_STAGE_CHANGE& pkt)
{
    PKT_C_STAGE_CHANGE pktCopy = pkt;

    G_JobQueue->Push([session, pktCopy]()
        {
            if (session == nullptr || session->GetPlayerId() <= 0 || G_Room == nullptr)
            {
                return;
            }

            if (pktCopy.targetStage != 2)
            {
                return;
            }

            if (!G_Room->StartStage2())
            {
                return;
            }

            PKT_S_STAGE_CHANGE sendPkt = {};
            sendPkt.header.size = sizeof(PKT_S_STAGE_CHANGE);
            sendPkt.header.id = PacketID::S_STAGE_CHANGE;
            sendPkt.playerId = session->GetPlayerId();
            sendPkt.targetStage = pktCopy.targetStage;

            G_Room->Broadcast(&sendPkt, sizeof(sendPkt));
            G_Room->BroadcastBossSnapshot();
            G_Room->BroadcastLanternStates();
        });
}

void ServerPacketHandler::Handle_C_LOGIN(std::shared_ptr<Session> session, PKT_C_LOGIN& pkt)
{
    PKT_C_LOGIN pktCopy = pkt;

    G_JobQueue->Push([session, pktCopy]()
        {
            std::cout << "[Logic Thread] Login Request Process..." << std::endl;

            std::string inputId = ReadPacketString(pktCopy.id, sizeof(pktCopy.id));
            std::string inputPw = ReadPacketString(pktCopy.password, sizeof(pktCopy.password));
            int userUid = 0;
            bool isLoginSuccess = false;

            if (IsDebugLogin(inputId, inputPw))
            {
                userUid = gNextDebugPlayerId.fetch_add(1);
                isLoginSuccess = true;
                LOG_INFO("Debug login accepted. id=%s uid=%d", inputId.c_str(), userUid);
            }
            else if (kEnableDbLogin)
            {
                isLoginSuccess = DBConnection::GetInstance()->Login(inputId, inputPw, userUid);
            }
            else
            {
                LOG_WARN("Login rejected while DB login is disabled. id=%s", inputId.c_str());
            }

            PKT_S_LOGIN sendPkt;
            sendPkt.header.size = sizeof(PKT_S_LOGIN);
            sendPkt.header.id = PacketID::S_LOGIN;

            if (isLoginSuccess)
            {
                session->SetDisplayName(inputId);
                session->SetPlayerInfo(userUid, 0.0f, 0.0f, 0.0f);
                if (G_Room != nullptr)
                {
                    G_Room->Enter(session);
                }
                return;
            }
            else
            {
                sendPkt.success = false;
                sendPkt.myPlayerId = 0;
            }

            session->Send(&sendPkt, sizeof(sendPkt));
        });
}

void ServerPacketHandler::Handle_C_REGISTER(std::shared_ptr<Session> session, PKT_C_REGISTER& pkt)
{
    PKT_C_REGISTER pktCopy = pkt;

    G_JobQueue->Push([session, pktCopy]()
        {
            const std::string inputId = ReadPacketString(pktCopy.id, sizeof(pktCopy.id));
            const std::string inputPw = ReadPacketString(pktCopy.password, sizeof(pktCopy.password));
            bool registerSuccess = false;

            if (kEnableDbLogin && !inputId.empty() && !inputPw.empty())
            {
                registerSuccess = DBConnection::GetInstance()->RegisterAccount(inputId, inputPw);
            }

            if (registerSuccess)
            {
                LOG_INFO("Register accepted. id=%s", inputId.c_str());
            }
            else
            {
                LOG_WARN("Register rejected. id=%s", inputId.c_str());
            }

            PKT_S_REGISTER sendPkt = {};
            sendPkt.header.size = sizeof(PKT_S_REGISTER);
            sendPkt.header.id = PacketID::S_REGISTER;
            sendPkt.success = registerSuccess;
            session->Send(&sendPkt, sizeof(sendPkt));
        });
}

void ServerPacketHandler::Handle_C_CHAT(std::shared_ptr<Session> session, PKT_C_CHAT& pkt)
{
    PKT_C_CHAT pktCopy = pkt;

    G_JobQueue->Push([session, pktCopy]()
        {
            std::cout << "[Logic Thread] Chat Broadcast: " << pktCopy.msg << std::endl;

            PKT_S_CHAT sendPkt = {};
            sendPkt.header.size = sizeof(PKT_S_CHAT);
            sendPkt.header.id = PacketID::S_CHAT;
            sendPkt.playerId = session->GetPlayerId();
            strncpy_s(sendPkt.senderName, GetSessionDisplayName(session).c_str(), _TRUNCATE);
            strcpy_s(sendPkt.msg, pktCopy.msg);

            if (G_Room != nullptr)
                G_Room->BroadcastExcept(session, &sendPkt, sizeof(sendPkt));
        });
}

void ServerPacketHandler::Handle_C_GAME_START(std::shared_ptr<Session> session, PKT_C_GAME_START& pkt)
{
    UNREFERENCED_PARAMETER(pkt);

    G_JobQueue->Push([session]()
        {
            if (G_Room == nullptr)
            {
                return;
            }

            if (!G_Room->CanStartGame(session))
            {
                return;
            }

            G_Room->InitMonsters();
            G_Room->ResetPlayerCombatStates();
            G_Room->ApplyStage1StartPositions();
            G_Room->SetGameStarted(true);

            PKT_S_GAME_START sendPkt = {};
            sendPkt.header.size = sizeof(PKT_S_GAME_START);
            sendPkt.header.id = PacketID::S_GAME_START;
            G_Room->Broadcast(&sendPkt, sizeof(sendPkt));
            G_Room->BroadcastMonsterSnapshots();
        });
}

void ServerPacketHandler::Handle_C_PLAYER_READY(std::shared_ptr<Session> session, PKT_C_PLAYER_READY& pkt)
{
    const bool ready = pkt.ready;

    G_JobQueue->Push([session, ready]()
        {
            if (G_Room != nullptr)
            {
                G_Room->SetPlayerReady(session, ready);
            }
        });
}

void ServerPacketHandler::Handle_C_PLAYER_MOVE(std::shared_ptr<Session> session, PKT_C_PLAYER_MOVE& pkt)
{
    PKT_C_PLAYER_MOVE pktCopy = pkt;

    G_JobQueue->Push([session, pktCopy]()
        {
            if (session == nullptr)
            {
                return;
            }

            int playerId = session->GetPlayerId();
            if (playerId <= 0)
            {
                return;
            }

            if (!std::isfinite(pktCopy.x) ||
                !std::isfinite(pktCopy.y) ||
                !std::isfinite(pktCopy.z) ||
                !std::isfinite(pktCopy.rotY))
            {
                return;
            }

            if (std::fabs(pktCopy.x) > kMaxPlayerWorldCoordinate ||
                std::fabs(pktCopy.y) > kMaxPlayerWorldCoordinate ||
                std::fabs(pktCopy.z) > kMaxPlayerWorldCoordinate)
            {
                return;
            }

            const float normalizedRotY = std::remainder(pktCopy.rotY, 2.0f * 3.14159265f);
            bool playerHpChanged = false;
            if (!session->RegisterPlayerClass(pktCopy.classType, &playerHpChanged))
            {
                return;
            }

            if (!session->RegisterPlayerLevel(pktCopy.playerLevel))
            {
                return;
            }

            if (playerHpChanged && G_Room != nullptr)
            {
                G_Room->BroadcastPlayerHp(session);
            }

            if (!session->TryUpdatePlayerPosition(
                pktCopy.x,
                pktCopy.y,
                pktCopy.z,
                normalizedRotY,
                kMaxPlayerMoveSpeed,
                kPlayerMoveBurstDistance))
            {
                return;
            }

            PKT_S_PLAYER_MOVE sendPkt;
            sendPkt.header.size = sizeof(PKT_S_PLAYER_MOVE);
            sendPkt.header.id = PacketID::S_PLAYER_MOVE;
            sendPkt.playerId = playerId;
            sendPkt.x = pktCopy.x;
            sendPkt.y = pktCopy.y;
            sendPkt.z = pktCopy.z;
            sendPkt.rotY = normalizedRotY;
            sendPkt.animationState = pktCopy.animationState;
            sendPkt.classType = session->GetPlayerClassType();
            sendPkt.playerLevel = session->GetPlayerLevel();

            if (G_Room != nullptr)
                G_Room->BroadcastExcept(session, &sendPkt, sizeof(sendPkt));
        });
}
