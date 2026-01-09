#pragma once
#include <WinSock2.h>
#include <MSWSock.h>
#include <WS2tcpip.h>
#include <Windows.h>
#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <memory>
#include "LogManager.h"

#pragma comment(lib, "ws2_32.lib")

using BYTE = unsigned char;

// IOCP 이벤트 타입
enum class EventType
{
    Connect,
    Disconnect,
    Accept,
    Recv,
    Send
};

struct IocpEvent : public OVERLAPPED
{
    EventType type;
    void* owner; // 누가 보냈는지 (Session 등)

    IocpEvent(EventType t) : type(t), owner(nullptr)
    {
        Internal = 0; 
        InternalHigh = 0; 
        Offset = 0; 
        OffsetHigh = 0; 
        hEvent = 0;
    }
};