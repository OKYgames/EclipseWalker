#include "RecvBuffer.h"

RecvBuffer::RecvBuffer(int bufferSize) : _bufferSize(bufferSize)
{
    _capacity = bufferSize * 10; 
    _buffer.resize(_capacity);
}

RecvBuffer::~RecvBuffer()
{
}

void RecvBuffer::Clean()
{
    int dataSize = DataSize();
    if (dataSize == 0)
    {
        _readPos = _writePos = 0;
    }
    else
    {
        if (FreeSize() < _bufferSize)
        {
            ::memcpy(&_buffer[0], &_buffer[_readPos], dataSize);
            _readPos = 0;
            _writePos = dataSize;
        }
    }
}

bool RecvBuffer::OnWrite(int numOfBytes)
{
    if (numOfBytes > FreeSize()) return false;

    _writePos += numOfBytes;
    return true;
}

bool RecvBuffer::OnRead(int numOfBytes)
{
    if (numOfBytes > DataSize()) return false;

    _readPos += numOfBytes;
    return true;
}