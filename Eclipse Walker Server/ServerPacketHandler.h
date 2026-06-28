#pragma once
#include "Protocol.h"
#include <memory>
#include <iostream>
#include "Define.h"

// 전방 선언 (Session 헤더를 여기서 include 하면 순환 참조 날 수 있음)
class Session;

class ServerPacketHandler
{
public:
    // 모든 패킷 처리는 이 함수로 시작됨
    static void HandlePacket(std::shared_ptr<Session> session, BYTE* buffer, int len);

    // 각 패킷별 처리 함수들
    static void Handle_C_LOGIN(std::shared_ptr<Session> session, PKT_C_LOGIN& pkt);
    static void Handle_C_CHAT(std::shared_ptr<Session> session, PKT_C_CHAT& pkt);

    static void Handle_C_PLAYER_MOVE(std::shared_ptr<Session> session, PKT_C_PLAYER_MOVE& pkt);

    static void Handle_C_PLAYER_ATTACK(std::shared_ptr<Session> session, PKT_C_PLAYER_ATTACK& pkt);
    static void Handle_C_GAME_START(std::shared_ptr<Session> session, PKT_C_GAME_START& pkt);
    static void Handle_C_PLAYER_READY(std::shared_ptr<Session> session, PKT_C_PLAYER_READY& pkt);
    static void Handle_C_LANTERN_GAUGE(std::shared_ptr<Session> session, PKT_C_LANTERN_GAUGE& pkt);
    static void Handle_C_WORLD_SHIFT(std::shared_ptr<Session> session, PKT_C_WORLD_SHIFT& pkt);
    static void Handle_C_DOOR_INTERACT(std::shared_ptr<Session> session, PKT_C_DOOR_INTERACT& pkt);
    static void Handle_C_PICKUP_COLLECT(std::shared_ptr<Session> session, PKT_C_PICKUP_COLLECT& pkt);
    static void Handle_C_STAGE_CHANGE(std::shared_ptr<Session> session, PKT_C_STAGE_CHANGE& pkt);
};
