#pragma once
#pragma once

#include <windows.h>
#include <sql.h>
#include <sqlext.h>
#include <string>

// DB 연결을 전담하는 싱글톤(Singleton) 클래스
class DBConnection
{
public:
    // 어디서든 DBConnection::GetInstance()->함수명() 으로 접근할 수 있게 해줍니다.
    static DBConnection* GetInstance()
    {
        static DBConnection instance;
        return &instance;
    }

public:
    // DB 연결 및 해제 함수
    bool ConnectDB();
    void DisconnectDB();

    // 실제 로그인 검증 함수
    bool Login(const std::string& inputId, const std::string& inputPassword, int& outUid);

private:
    // 싱글톤 패턴을 위해 생성자와 소멸자는 private으로 숨깁니다.
    DBConnection();
    ~DBConnection();

private:
    // ODBC 통신에 필요한 핵심 핸들 2가지
    SQLHENV _hEnv;  // 환경 핸들
    SQLHDBC _hDbc;  // 연결 핸들
};