#pragma once

#include <string>
#include <thread>
#include <atomic>
#include <mutex>   
#include <queue>  
#include <vector> 
#include <WinSock2.h>
#include <WS2tcpip.h>
#include "Protocol.h"

class NetworkManager
{
public:
    // 싱글톤 (어디서든 NetworkManager::Get()-> 함수() 형태로 호출 가능)
    static NetworkManager* Get()
    {
        static NetworkManager instance;
        return &instance;
    }

private:
    // 싱글톤이므로 함부로 생성하지 못하게 private 처리
    NetworkManager();
    ~NetworkManager();

public:
    // ==========================================
    // 1. 연결 및 종료
    // ==========================================
    void ConnectAsync(const std::string& ip, short port);
    void Disconnect();
    bool IsConnected() const { return m_isConnected; }

    // ==========================================
    // 2. 패킷 송신 (클라이언트 -> 서버)
    // ==========================================
    void SendLogin(const std::string& id, const std::string& pw);
    void SendPlayerMove(float x, float y, float z, float rotY);
    void SendPlayerAttack(int targetId);

    // ==========================================
    // 3. 패킷 처리 (메인 스레드의 Update에서 호출)
    // ==========================================
    void ProcessPackets();

private:
    // ==========================================
    // 4. 내부 작동 함수 (외부에서 호출 불가)
    // ==========================================
    bool Connect(const std::string& ip, short port);
    void SendPacket(void* packet, int size);

    // 데이터를 계속 받을 수신 전담 스레드 함수
    void RecvLoop(std::string ip, short port);

private:
    // ==========================================
    // 5. 멤버 변수
    // ==========================================
    SOCKET m_socket;

    // 스레드 환경에서 안전하게 접근하기 위해 atomic 사용 (기존의 그냥 bool은 삭제)
    std::atomic<bool> m_isConnected;
    std::atomic<bool> m_isRunning;   // 스레드를 안전하게 종료하기 위한 깃발

    std::thread m_recvThread;        // 수신 전담 스레드

    // 큐에 패킷을 넣고 뺄 때 꼬이지 않도록 Mutex 장착
    std::mutex m_packetMutex;
    std::queue<std::vector<char>> m_packetQueue;
};