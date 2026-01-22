#include "Session.h"

Session::Session() : _recvBuffer(65536), _recvEvent(EventType::Recv), _sendEvent(EventType::Send), _socket(INVALID_SOCKET)
{
    _recvEvent.owner = this;
    _sendEvent.owner = this;
}

Session::~Session()
{
    closesocket(_socket);
}

void Session::Init(SOCKET socket, SOCKADDR_IN address)
{
    _socket = socket;
    _addr = address;
    OnConnected();
    RegisterRecv();
}

// [핵심 수정] Send는 이제 안전하게 큐에 넣기만 한다.
void Session::Send(void* msg, int len)
{
    std::lock_guard<std::mutex> lock(_lock);

    // 1. 보낼 데이터를 벡터로 만들어서 큐에 복사 (보관)
    std::vector<BYTE> sendData;
    sendData.resize(len);
    memcpy(sendData.data(), msg, len);

    _sendQueue.push(sendData);

    // 2. 만약 현재 보내고 있는 중이 아니라면, 전송 시작!
    if (_sendRegistered == false)
    {
        RegisterSend();
    }
}

// [핵심 추가] 실제 전송을 담당하는 함수 (내부에서만 호출됨)
// 주의: 반드시 _lock이 잡힌 상태에서 호출해야 함
void Session::RegisterSend()
{
    if (_sendRegistered) return; // 이미 보내고 있으면 패스

    _sendRegistered = true; // "나 지금 보내는 중이야" 깃발 듦

    // 큐의 맨 앞에 있는 데이터를 가져옴
    std::vector<BYTE>& sendData = _sendQueue.front();

    _sendWsaBuf.buf = (char*)sendData.data();
    _sendWsaBuf.len = (ULONG)sendData.size();

    DWORD numOfBytes = 0;

    // 비동기 전송 시작
    if (WSASend(_socket, &_sendWsaBuf, 1, &numOfBytes, 0, &_sendEvent, nullptr) == SOCKET_ERROR)
    {
        if (WSAGetLastError() != WSA_IO_PENDING)
        {
            LOG_ERROR("Send Error: %d", WSAGetLastError());
            _sendRegistered = false; // 실패했으면 깃발 내림
        }
    }
}

void Session::RegisterRecv()
{
    _recvBuffer.Clean();

    WSABUF wsaBuf;
    wsaBuf.buf = (char*)_recvBuffer.WritePos();
    wsaBuf.len = _recvBuffer.FreeSize();

    DWORD numOfBytes = 0;
    DWORD flags = 0;

    if (WSARecv(_socket, &wsaBuf, 1, &numOfBytes, &flags, &_recvEvent, nullptr) == SOCKET_ERROR)
    {
        if (WSAGetLastError() != WSA_IO_PENDING)
        {
            LOG_ERROR("Recv Error: %d", WSAGetLastError());
        }
    }
}

void Session::Dispatch(IocpEvent* iocpEvent, int numOfBytes)
{
    switch (iocpEvent->type)
    {
    case EventType::Recv:
        HandleRecv(numOfBytes);
        break;
    case EventType::Send:
        HandleSend(numOfBytes);
        break;
    }
}

void Session::HandleRecv(int numOfBytes)
{
    if (numOfBytes == 0)
    {
        OnDisconnected();
        return;
    }

    if (_recvBuffer.OnWrite(numOfBytes) == false)
    {
        OnDisconnected();
        return;
    }

    int processLen = OnRecv(_recvBuffer.ReadPos(), _recvBuffer.DataSize());

    if (processLen < 0 || processLen > _recvBuffer.DataSize())
    {
        OnDisconnected();
        return;
    }

    if (_recvBuffer.OnRead(processLen) == false)
    {
        OnDisconnected();
        return;
    }

    RegisterRecv();
}

// [핵심 수정] 전송 완료 통지가 왔을 때
void Session::HandleSend(int numOfBytes)
{
    std::lock_guard<std::mutex> lock(_lock);

    // 1. 방금 보낸 패킷은 임무 완수했으니 큐에서 삭제
    _sendQueue.pop();

    // 2. 깃발 내림 (전송 끝)
    _sendRegistered = false;

    // 3. 큐에 보낼 게 더 남았나 확인
    if (_sendQueue.empty() == false)
    {
        RegisterSend(); // 남은 거 마저 보내!
    }

    OnSend(numOfBytes);
}