#include "DBConnection.h"
#include <iostream>

DBConnection::DBConnection() : _hEnv(SQL_NULL_HENV), _hDbc(SQL_NULL_HDBC)
{

}

DBConnection::~DBConnection()
{
    DisconnectDB();
}

bool DBConnection::ConnectDB()
{
    SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &_hEnv);
    SQLSetEnvAttr(_hEnv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)SQL_OV_ODBC3, 0);
    SQLAllocHandle(SQL_HANDLE_DBC, _hEnv, &_hDbc);

    SQLWCHAR connectionString[] = L"DRIVER={MySQL ODBC 9.6 UNICODE Driver};SERVER=127.0.0.1;PORT=3306;DATABASE=GameDB;USER=root;PASSWORD=1234;";
    SQLWCHAR outConnectionString[1024];
    SQLSMALLINT outConnectionStringLen;

    SQLRETURN ret = SQLDriverConnect(
        _hDbc, NULL, connectionString, SQL_NTS,
        outConnectionString, 1024, &outConnectionStringLen, SQL_DRIVER_NOPROMPT);

    if (SQL_SUCCEEDED(ret)) {
        std::cout << "MySQL 연결 성공!" << std::endl;
        return true;
    }

    else {
        std::cout << "MySQL 연결 실패..." << std::endl;
        return false;
    }
}

void DBConnection::DisconnectDB()
{
    if (_hDbc != SQL_NULL_HDBC) {
        SQLDisconnect(_hDbc);
        SQLFreeHandle(SQL_HANDLE_DBC, _hDbc);
        _hDbc = SQL_NULL_HDBC;

    }
    if (_hEnv != SQL_NULL_HENV) {
        SQLFreeHandle(SQL_HANDLE_ENV, _hEnv);
        _hEnv = SQL_NULL_HENV;
    }
}

bool DBConnection::Login(const std::string& inputId, const std::string& inputPassword, int& outUid)
{
    SQLHSTMT hStmt;
    SQLAllocHandle(SQL_HANDLE_STMT, _hDbc, &hStmt);

    // 1. 쿼리 준비
    SQLWCHAR query[] = L"SELECT uid, password FROM PlayerAccount WHERE account_id = ?";
    SQLPrepare(hStmt, query, SQL_NTS);

    // 2. 파라미터 바인딩 (char 타입인 SQL_C_CHAR 사용)
    SQLBindParameter(hStmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 50, 0, (SQLPOINTER)inputId.c_str(), 0, NULL);

    // 3. 쿼리 실행
    SQLExecute(hStmt);

    // 4. 결과 확인
    if (SQLFetch(hStmt) == SQL_SUCCESS || SQLFetch(hStmt) == SQL_SUCCESS_WITH_INFO) {
        SQLINTEGER dbUid;
        SQLCHAR dbPassword[256];
        SQLLEN cbUid, cbPassword;

        // 결과값 꺼내기
        SQLGetData(hStmt, 1, SQL_C_LONG, &dbUid, 0, &cbUid);
        SQLGetData(hStmt, 2, SQL_C_CHAR, dbPassword, sizeof(dbPassword), &cbPassword);

        // 5. 비밀번호 비교 (char 배열 호환)
        if (inputPassword == (char*)dbPassword) {
            outUid = dbUid;
            SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
            return true;
        }
    }

    SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
    return false;
}