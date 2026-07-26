#pragma once

namespace DebugConfig
{
    constexpr bool kEnableBackendConnection = true;
    constexpr bool kEnableDbLogin = true;
    constexpr bool kAllowSoloLobbyStart = true;
    constexpr int kOfflineStartStage = 1;
    constexpr bool kDebugFullLanternGauge = false;

    constexpr const char* kServerIp = "127.0.0.1";
    //constexpr const char* kServerIp = "112.170.181.135";
    constexpr short kServerPort = 7777;
    constexpr const char* kDebugLoginId = "debug_user";
    constexpr const char* kDebugLoginPassword = "debug_pw";

    constexpr int kMaxNetworkPacketsPerFrame = 64;
    constexpr float kPlayerMoveSendIntervalSeconds = 0.05f;
    constexpr float kPlayerMovePositionEpsilon = 0.02f;
    constexpr float kPlayerMoveRotationEpsilon = 0.02f;
}
