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
    if (_connected)
    {
        return true;
    }

    if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &_hEnv)))
    {
        std::cout << "[DB] SQLAllocHandle ENV failed" << std::endl;
        return false;
    }

    if (!SQL_SUCCEEDED(SQLSetEnvAttr(_hEnv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)SQL_OV_ODBC3, 0)))
    {
        std::cout << "[DB] SQLSetEnvAttr failed" << std::endl;
        DisconnectDB();
        return false;
    }

    if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_DBC, _hEnv, &_hDbc)))
    {
        std::cout << "[DB] SQLAllocHandle DBC failed" << std::endl;
        DisconnectDB();
        return false;
    }

    SQLWCHAR connectionString[] = L"DRIVER={MySQL ODBC 9.6 Unicode Driver};SERVER=127.0.0.1;PORT=3306;DATABASE=GameDB;USER=root;PASSWORD=1234;";
    SQLWCHAR outConnectionString[1024] = {};
    SQLSMALLINT outConnectionStringLen = 0;

    SQLRETURN ret = SQLDriverConnect(
        _hDbc,
        NULL,
        connectionString,
        SQL_NTS,
        outConnectionString,
        1024,
        &outConnectionStringLen,
        SQL_DRIVER_NOPROMPT);

    if (SQL_SUCCEEDED(ret))
    {
        _connected = true;
        std::cout << "[DB] MySQL connected" << std::endl;
        return true;
    }

    std::cout << "[DB] MySQL connection failed" << std::endl;
    DisconnectDB();
    return false;
}

void DBConnection::DisconnectDB()
{
    if (_hDbc != SQL_NULL_HDBC)
    {
        SQLDisconnect(_hDbc);
        SQLFreeHandle(SQL_HANDLE_DBC, _hDbc);
        _hDbc = SQL_NULL_HDBC;
    }

    if (_hEnv != SQL_NULL_HENV)
    {
        SQLFreeHandle(SQL_HANDLE_ENV, _hEnv);
        _hEnv = SQL_NULL_HENV;
    }

    _connected = false;
}

bool DBConnection::Login(const std::string& inputId, const std::string& inputPassword, int& outUid)
{
    outUid = 0;
    if (!_connected || _hDbc == SQL_NULL_HDBC || inputId.empty() || inputPassword.empty())
    {
        return false;
    }

    SQLHSTMT hStmt = SQL_NULL_HSTMT;
    if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_STMT, _hDbc, &hStmt)))
    {
        return false;
    }

    SQLWCHAR query[] = L"SELECT uid FROM PlayerAccount WHERE account_id = ? AND password = ?";
    SQLRETURN ret = SQLPrepare(hStmt, query, SQL_NTS);
    if (!SQL_SUCCEEDED(ret))
    {
        SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
        return false;
    }

    SQLLEN idLen = SQL_NTS;
    ret = SQLBindParameter(
        hStmt,
        1,
        SQL_PARAM_INPUT,
        SQL_C_CHAR,
        SQL_VARCHAR,
        50,
        0,
        (SQLPOINTER)inputId.c_str(),
        static_cast<SQLLEN>(inputId.size()),
        &idLen);
    if (!SQL_SUCCEEDED(ret))
    {
        SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
        return false;
    }

    SQLLEN pwLen = SQL_NTS;
    ret = SQLBindParameter(
        hStmt,
        2,
        SQL_PARAM_INPUT,
        SQL_C_CHAR,
        SQL_VARCHAR,
        255,
        0,
        (SQLPOINTER)inputPassword.c_str(),
        static_cast<SQLLEN>(inputPassword.size()),
        &pwLen);
    if (!SQL_SUCCEEDED(ret))
    {
        SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
        return false;
    }

    ret = SQLExecute(hStmt);
    if (!SQL_SUCCEEDED(ret))
    {
        SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
        return false;
    }

    ret = SQLFetch(hStmt);
    if (SQL_SUCCEEDED(ret))
    {
        SQLINTEGER dbUid = 0;
        SQLLEN cbUid = 0;
        if (SQL_SUCCEEDED(SQLGetData(hStmt, 1, SQL_C_LONG, &dbUid, 0, &cbUid)))
        {
            outUid = static_cast<int>(dbUid);
            SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
            return outUid > 0;
        }
    }

    SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
    return false;
}
