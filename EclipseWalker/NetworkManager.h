#pragma once
#include <string>
#include <WS2tcpip.h>
#include <WinSock2.h>
#include <thread>
#include <atomic>
#include <queue>   // ★ 추가: 패킷을 담아둘 큐
#include <mutex>   // ★ 추가: 스레드 충돌 방지용 자물쇠
#include <vector>  // ★ 추가: 가변 길이 데이터 보관용
#include "Protocol.h"


class NetworkManager
{
public:
    static NetworkManager* Get()
    {
        static NetworkManager instance;
        return &instance;
    }

    // ★ 동료가 추가한 함수들 적용
    void ConnectAsync(const std::string& ip, short port);
    void Disconnect();
    void ProcessPackets(); // 메인 프레임에서 쌓인 패킷을 처리할 함수

    // 송신 함수
    void SendPacket(void* packet, int size);
    void SendLogin(const std::string& id, const std::string& pw);
    void SendPlayerMove(float x, float y, float z, float rotY);

private:
    NetworkManager() : m_socket(INVALID_SOCKET), m_isConnected(false), m_isRunning(false) {}
    ~NetworkManager() { Disconnect(); }

    void RecvLoop();

private:
    SOCKET m_socket;
    bool m_isConnected;

    std::thread m_recvThread;
    std::atomic<bool> m_isRunning;

    // ★ 패킷 보관함 (백그라운드 스레드가 넣고, 메인 스레드가 뺌)
    std::queue<std::vector<char>> m_packetQueue;
    std::mutex m_queueMutex; // 큐를 동시에 건드리지 못하게 하는 자물쇠
};