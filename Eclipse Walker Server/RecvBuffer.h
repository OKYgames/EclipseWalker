#pragma once
#include "Define.h"
#include <vector>

class RecvBuffer
{
public:
    RecvBuffer(int bufferSize);
    ~RecvBuffer();

    BYTE* WritePos() { return &_buffer[_writePos]; }

    int FreeSize() { return _capacity - _writePos; }

    bool OnWrite(int numOfBytes);

    BYTE* ReadPos() { return &_buffer[_readPos]; }

    int DataSize() { return _writePos - _readPos; }

    bool OnRead(int numOfBytes);

    void Clean();

private:
    int _capacity = 0;
    int _bufferSize = 0;
    int _readPos = 0;
    int _writePos = 0;
    std::vector<BYTE> _buffer;
};