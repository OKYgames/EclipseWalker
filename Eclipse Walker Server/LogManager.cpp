#include "LogManager.h"
#include <cstdarg> // 가변 인자(va_list) 사용을 위해
#include <direct.h> // 폴더 생성(_mkdir)

void LogManager::Initialize()
{
    _hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

    _mkdir("Logs");

    SYSTEMTIME st;
    GetLocalTime(&st);

    char fileName[MAX_PATH];
    snprintf(fileName, sizeof(fileName), "Logs/Log_%04d%02d%02d.txt", st.wYear, st.wMonth, st.wDay);

    _logFile.open(fileName, std::ios::app);

    if (_logFile.is_open())
    {
        _logFile << "===================================================" << std::endl;
        _logFile << "   Server Started at " << st.wHour << ":" << st.wMinute << ":" << st.wSecond << std::endl;
        _logFile << "===================================================" << std::endl;
    }
}

void LogManager::Finalize()
{
    if (_logFile.is_open())
    {
        _logFile << "================ Server Stopped ================" << std::endl;
        _logFile.close();
    }
}

void LogManager::WriteLog(LogType type, const char* fileName, int lineNo, const char* format, ...)
{
    // 멀티스레드 보호: 여러 스레드가 동시에 로그를 찍으려 할 때 섞이지 않게 함
    std::lock_guard<std::mutex> lock(_lock);

    SYSTEMTIME st;
    GetLocalTime(&st);

    char buffer[4096];
    va_list ap;
    va_start(ap, format);
    vsnprintf(buffer, sizeof(buffer), format, ap);
    va_end(ap);

    const char* typeStr = "[INFO]";

    SetColor(type); // 색상 변경

    switch (type)
    {
    case LogType::LOG_INFO:   typeStr = "[INFO]"; break;
    case LogType::LOG_WARN:   typeStr = "[WARN]"; break;
    case LogType::LOG_ERROR:  typeStr = "[ERR ]"; break;
    case LogType::LOG_PACKET: typeStr = "[PCKT]"; break;
    case LogType::LOG_DB:     typeStr = "[ DB ]"; break;
    }

    const char* shortFileName = strrchr(fileName, '\\');
    if (shortFileName) shortFileName++;
    else shortFileName = fileName;

    printf("[%02d:%02d:%02d] %s %s (%s:%d)\n",
        st.wHour, st.wMinute, st.wSecond,
        typeStr, buffer, shortFileName, lineNo);

    SetColor(LogType::LOG_INFO);

    if (_logFile.is_open())
    {
        _logFile << "[" << st.wHour << ":" << st.wMinute << ":" << st.wSecond << "] "
            << typeStr << " " << buffer
            << " (" << shortFileName << ":" << lineNo << ")" << std::endl;

        // 중요: 즉시 파일에 씀
        _logFile.flush();
    }
}

void LogManager::WriteHex(const char* subject, void* data, int length)
{
    std::lock_guard<std::mutex> lock(_lock);

    SetColor(LogType::LOG_PACKET);

    unsigned char* byteData = (unsigned char*)data;

    printf("[%s] Size: %d\n", subject, length);
    if (_logFile.is_open()) _logFile << "[" << subject << "] HEX: ";

    for (int i = 0; i < length; ++i)
    {
        printf("%02X ", byteData[i]);

        if (_logFile.is_open())
        {
            char hexBuf[10];
            snprintf(hexBuf, sizeof(hexBuf), "%02X ", byteData[i]);
            _logFile << hexBuf;
        }

        if ((i + 1) % 16 == 0) printf("\n");
    }
    printf("\n");
    if (_logFile.is_open()) _logFile << std::endl;

    SetColor(LogType::LOG_INFO);
}

void LogManager::SetColor(LogType type)
{
    if (_hConsole == INVALID_HANDLE_VALUE) return;

    WORD color = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;

    switch (type)
    {
    case LogType::LOG_INFO:
        color = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
        break;
    case LogType::LOG_WARN:
        color = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
        break;
    case LogType::LOG_ERROR:
        color = FOREGROUND_RED | FOREGROUND_INTENSITY;
        break;
    case LogType::LOG_PACKET:
        color = FOREGROUND_GREEN | FOREGROUND_INTENSITY;
        break;
    case LogType::LOG_DB:
        color = FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
        break;
    }
    SetConsoleTextAttribute(_hConsole, color);
}