#pragma once
#include <Windows.h>
#include <iostream>
#include <fstream>
#include <string>
#include <mutex>
#include <vector>

enum class LogType
{
    LOG_INFO,  
    LOG_WARN,  
    LOG_ERROR, 
    LOG_PACKET,
    LOG_DB     
};

class LogManager
{
public:
    static LogManager* GetInstance()
    {
        static LogManager instance;
        return &instance;
    }

    void Initialize();
    void Finalize();

    void WriteLog(LogType type, const char* fileName, int lineNo, const char* format, ...);

    void WriteHex(const char* subject, void* data, int length);

private:
    LogManager() {};
    ~LogManager() {};

    void SetColor(LogType type);

private:
    std::mutex _lock;           // 스레드 안전을 위한 자물쇠
    std::ofstream _logFile;     // 파일 입출력 객체
    HANDLE _hConsole = INVALID_HANDLE_VALUE; // 콘솔 핸들
};

// ==========================================================
// ★ 개발할 때 사용하는 매크로 이거 참고해서 사용할 것
// ==========================================================

// 예: LOG_INFO("유저 접속: %s", userId);
#define LOG_INFO(...)    LogManager::GetInstance()->WriteLog(LogType::LOG_INFO, __FILE__, __LINE__, __VA_ARGS__)

// 예: LOG_WARN("비번 틀림: %s", userId);
#define LOG_WARN(...)    LogManager::GetInstance()->WriteLog(LogType::LOG_WARN, __FILE__, __LINE__, __VA_ARGS__)

// 예: LOG_ERROR("DB 연결 실패! 에러코드: %d", errorCode);
#define LOG_ERROR(...)   LogManager::GetInstance()->WriteLog(LogType::LOG_ERROR, __FILE__, __LINE__, __VA_ARGS__)

// 예: LOG_HEX("이동 패킷", packetPtr, packetSize);
#define LOG_HEX(sub, ptr, len) LogManager::GetInstance()->WriteHex(sub, ptr, len)