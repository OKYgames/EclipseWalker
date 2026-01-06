#pragma once
#include "d3dUtil.h" 
#include "UploadBuffer.h" 
#include "RenderItem.h"


struct FrameResource
{
public:
    FrameResource(ID3D12Device* device, UINT passCount, UINT objectCount);
    ~FrameResource();

    // 1. 명령 할당자 (Command Allocator)
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CmdListAlloc;

    // 2. 객체 상수 버퍼
    std::unique_ptr<UploadBuffer<ObjectConstants>> ObjectCB = nullptr;

    // 3. 펜스 값 (Fence Value)
    UINT64 Fence = 0;
};