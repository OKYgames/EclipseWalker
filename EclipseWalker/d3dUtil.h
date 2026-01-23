#pragma once
#include <windows.h>
#include <wrl.h>
#include <dxgi1_4.h>
#include <d3d12.h>
#include <D3Dcompiler.h>
#include <DirectXMath.h>
#include <DirectXColors.h>
#include <DirectXCollision.h>

#include <string>
#include <vector>
#include <array>
#include <comdef.h>
#include <memory>    
#include <algorithm>

#include "d3dx12.h"
#include "DDSTextureLoader.h"

#include "MathHelper.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;

// 오류 발생 시 메시지 박스를 띄우고 강제 종료하는 클래스
class DxException
{
public:
    DxException(HRESULT hr, const std::wstring& functionName, const std::wstring& filename, int lineNumber) :
        ErrorCode(hr),
        FunctionName(functionName),
        Filename(filename),
        LineNumber(lineNumber)
    {
    }

    std::wstring ToString()const
    {
        _com_error err(ErrorCode);
        std::wstring msg = err.ErrorMessage();
        return FunctionName + L" failed in " + Filename + L"; line " + std::to_wstring(LineNumber) + L"; error: " + msg;
    }

    HRESULT ErrorCode = S_OK;
    std::wstring FunctionName;
    std::wstring Filename;
    int LineNumber = -1;
};

// DX12 함수가 실패하면 예외를 던짐
#ifndef ThrowIfFailed
#define ThrowIfFailed(x) \
{ \
    HRESULT hr__ = (x); \
    std::wstring wfn = L#x; \
    if(FAILED(hr__)) { \
        throw DxException(hr__, L#x, std::wstring(__FILEW__), __LINE__); \
    } \
}
#endif

class d3dUtil
{
public:
    // 256 바이트 정렬 계산 (UploadBuffer에서 썼던 것)
    static UINT CalcConstantBufferByteSize(UINT byteSize)
    {
        return (byteSize + 255) & ~255;
    }

    // 나중에 셰이더 컴파일 할 때 쓸 함수 
    static ComPtr<ID3DBlob> CompileShader(
        const std::wstring& filename,
        const D3D_SHADER_MACRO* defines,
        const std::string& entrypoint,
        const std::string& target);

    // 기본 버퍼 생성 도우미 
    static ComPtr<ID3D12Resource> CreateDefaultBuffer(
        ID3D12Device* device,
        ID3D12GraphicsCommandList* cmdList,
        const void* initData,
        UINT64 byteSize,
        ComPtr<ID3D12Resource>& uploadBuffer)
    {
        ComPtr<ID3D12Resource> defaultBuffer;

        // 임시 객체를 변수로 저장 (L-value 만들기)
        auto heapPropsDefault = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
        auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(byteSize);

        // 1. 실제 GPU가 쓸 데이터 공간 (Default Heap) 생성
        ThrowIfFailed(device->CreateCommittedResource(
            &heapPropsDefault, 
            D3D12_HEAP_FLAG_NONE,
            &bufferDesc,       
            D3D12_RESOURCE_STATE_COMMON,
            nullptr,
            IID_PPV_ARGS(defaultBuffer.GetAddressOf())));

        auto heapPropsUpload = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        // 2. CPU -> GPU 복사용 임시 공간 (Upload Heap) 생성
        ThrowIfFailed(device->CreateCommittedResource(
            &heapPropsUpload,
            D3D12_HEAP_FLAG_NONE,
            &bufferDesc,     
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(uploadBuffer.GetAddressOf())));

        // 3. 복사할 데이터 준비
        D3D12_SUBRESOURCE_DATA subResourceData = {};
        subResourceData.pData = initData;
        subResourceData.RowPitch = byteSize;
        subResourceData.SlicePitch = subResourceData.RowPitch;

        // 리소스 배리어도 변수로 저장
        auto barrier1 = CD3DX12_RESOURCE_BARRIER::Transition(defaultBuffer.Get(),
            D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);

        // 4. 데이터 전송 명령 기록
        cmdList->ResourceBarrier(1, &barrier1); 

        UpdateSubresources<1>(cmdList, defaultBuffer.Get(), uploadBuffer.Get(), 0, 0, 1, &subResourceData);

        // 두 번째 배리어도 변수로 저장
        auto barrier2 = CD3DX12_RESOURCE_BARRIER::Transition(defaultBuffer.Get(),
            D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ);

        cmdList->ResourceBarrier(1, &barrier2); 

        return defaultBuffer;
    }
};