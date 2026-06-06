#include "ServerPacketHandler.h"
#include "Session.h"
#include "GlobalQueue.h"
#include "Room.h"
#include "DBConnection.h"
#include <algorithm>
#include <atomic>
#include <cmath>

namespace
{
    constexpr bool kEnableDbLogin = false;
    constexpr bool kEnableDebugLogin = true;
    constexpr const char* kDebugLoginId = "debug_user";
    constexpr const char* kDebugLoginPassword = "debug_pw";
    constexpr float kLanternPickupCharge = 35.0f;
    std::atomic<int> gNextDebugPlayerId = 1000;

    bool IsDebugLogin(const std::string& id, const std::string& password)
    {
        return kEnableDebugLogin && id == kDebugLoginId && password == kDebugLoginPassword;
    }
}

namespace
{
    constexpr float kMonsterHitRadius = 0.45f;

    struct ServerAttackProfile
    {
        float range;
        float halfWidth;
        float coneDot;
        float verticalTolerance;
        int damage;
        bool hitAll;
    };

    float ClampFloat(float value, float minValue, float maxValue)
    {
        return (std::max)(minValue, (std::min)(value, maxValue));
    }

    ServerAttackProfile GetServerAttackProfile(int skillType)
    {
        switch (skillType)
        {
        case 1:
            return { 2.9f, 0.95f, 0.20f, 1.8f, 25, true };
        case 2:
            return { 3.4f, 1.15f, 0.10f, 1.8f, 40, true };
        case 0:
        default:
            return { 2.4f, 0.65f, 0.35f, 1.6f, 10, false };
        }
    }

    void ApplyClientAttackShape(ServerAttackProfile& profile, const PKT_C_PLAYER_ATTACK& pkt)
    {
        if (std::isfinite(pkt.range) && pkt.range > 0.0f)
        {
            profile.range = ClampFloat(pkt.range, 0.1f, 20.0f);
        }
        if (std::isfinite(pkt.radius) && pkt.radius > 0.0f)
        {
            profile.halfWidth = ClampFloat(pkt.radius, 0.05f, 8.0f);
        }
        if (std::isfinite(pkt.coneDot))
        {
            profile.coneDot = ClampFloat(pkt.coneDot, -0.99f, 0.99f);
        }

        profile.verticalTolerance = (std::max)(profile.verticalTolerance, 3.0f);
        profile.hitAll = true;
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

    void BroadcastMonsterHit(int monsterId, int damage)
    {
        const bool isDead = G_Room->ApplyDamageToMonster(monsterId, damage);

        PKT_S_MONSTER_HIT hitPkt = {};
        hitPkt.header.size = sizeof(PKT_S_MONSTER_HIT);
        hitPkt.header.id = PacketID::S_MONSTER_HIT;
        hitPkt.monsterId = monsterId;
        hitPkt.remainHp = G_Room->GetMonsterHp(monsterId);
        hitPkt.isDead = isDead;

        G_Room->Broadcast(&hitPkt, sizeof(hitPkt));
    }

    void BroadcastLanternState(const std::shared_ptr<Session>& session)
    {
        if (session == nullptr || G_Room == nullptr)
        {
            return;
        }

        PKT_S_LANTERN_GAUGE sendPkt = {};
        sendPkt.header.size = sizeof(PKT_S_LANTERN_GAUGE);
        sendPkt.header.id = PacketID::S_LANTERN_GAUGE;
        sendPkt.playerId = session->GetPlayerId();
        sendPkt.gauge = session->GetLanternGauge();
        sendPkt.maxGauge = session->GetLanternMaxGauge();
        sendPkt.level = session->GetLanternLevel();

        G_Room->Broadcast(&sendPkt, sizeof(sendPkt));
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


    default:
        std::cout << "Unknown Packet ID: " << header->id << std::endl;
        break;
    }
}

void ServerPacketHandler::Handle_C_PLAYER_ATTACK(std::shared_ptr<Session> session, PKT_C_PLAYER_ATTACK& pkt)
{
    PKT_C_PLAYER_ATTACK pktCopy = pkt;

    G_JobQueue->Push([session, pktCopy]()
        {
            std::cout << "[Logic Thread] Player Attack - skillType: " << pktCopy.skillType << std::endl;

            // Server-side attack profile by skill type.
            ServerAttackProfile profile = GetServerAttackProfile(pktCopy.skillType);
            ApplyClientAttackShape(profile, pktCopy);

            if (session == nullptr || session->GetPlayerId() <= 0)
            {
                return;
            }

            if (G_Room == nullptr)
            {
                return;
            }

            const float attackRotY = std::isfinite(pktCopy.rotY) ? pktCopy.rotY : 0.0f;
            PKT_S_PLAYER_ATTACK attackPkt = {};
            attackPkt.header.size = sizeof(PKT_S_PLAYER_ATTACK);
            attackPkt.header.id = PacketID::S_PLAYER_ATTACK;
            attackPkt.playerId = session->GetPlayerId();
            attackPkt.x = session->GetX();
            attackPkt.y = session->GetY();
            attackPkt.z = session->GetZ();
            attackPkt.rotY = attackRotY;
            attackPkt.skillType = pktCopy.skillType;
            G_Room->BroadcastExcept(session, &attackPkt, sizeof(attackPkt));

            // The server decides final hit results from player position and attack direction.
            if (G_Room != nullptr)
            {
                auto snapshots = G_Room->GetMonsterSnapshots();

                for (auto& m : snapshots)
                {
                    if (m.state == 3) continue; // DIE 상태 제외

                    if (IsMonsterInsideAttack(session->GetX(), session->GetY(), session->GetZ(), attackRotY, m, profile))
                    {
                        // 피해 적용 및 결과 브로드캐스트
                        BroadcastMonsterHit(m.monsterId, profile.damage);
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

            G_Room->StartStage2();

            PKT_S_STAGE_CHANGE sendPkt = {};
            sendPkt.header.size = sizeof(PKT_S_STAGE_CHANGE);
            sendPkt.header.id = PacketID::S_STAGE_CHANGE;
            sendPkt.playerId = session->GetPlayerId();
            sendPkt.targetStage = pktCopy.targetStage;

            G_Room->Broadcast(&sendPkt, sizeof(sendPkt));
            G_Room->BroadcastBossSnapshot();
        });
}

void ServerPacketHandler::Handle_C_LOGIN(std::shared_ptr<Session> session, PKT_C_LOGIN& pkt)
{
    PKT_C_LOGIN pktCopy = pkt;

    G_JobQueue->Push([session, pktCopy]()
        {
            std::cout << "[Logic Thread] Login Request Process..." << std::endl;

            std::string inputId = pktCopy.id;
            std::string inputPw = pktCopy.password;
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
            int playerId = session->GetPlayerId();
            if (playerId <= 0)
            {
                return;
            }

            session->SetPlayerInfo(playerId, pktCopy.x, pktCopy.y, pktCopy.z);

            PKT_S_PLAYER_MOVE sendPkt;
            sendPkt.header.size = sizeof(PKT_S_PLAYER_MOVE);
            sendPkt.header.id = PacketID::S_PLAYER_MOVE;
            sendPkt.playerId = playerId;
            sendPkt.x = pktCopy.x;
            sendPkt.y = pktCopy.y;
            sendPkt.z = pktCopy.z;
            sendPkt.rotY = pktCopy.rotY;
            sendPkt.animationState = pktCopy.animationState;

            if (G_Room != nullptr)
                G_Room->BroadcastExcept(session, &sendPkt, sizeof(sendPkt));
        });
}
