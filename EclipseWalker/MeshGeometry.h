#pragma once
#include "d3dUtil.h"

// 도형 하나를 관리하는 컨테이너
struct MeshGeometry
{
    // 1. 이름 (예: "boxGeo", "waterGeo")
    std::string Name;

    // 2. 시스템 메모리 복사본 (CPU가 들고 있는 원본 데이터)
    // ID3DBlob은 그냥 "데이터 덩어리"라고 생각
    ComPtr<ID3DBlob> VertexBufferCPU = nullptr;
    ComPtr<ID3DBlob> IndexBufferCPU = nullptr;

    // 3. GPU 메모리 (실제 그래픽카드가 쓸 데이터)
    ComPtr<ID3D12Resource> VertexBufferGPU = nullptr;
    ComPtr<ID3D12Resource> IndexBufferGPU = nullptr;

    // 4. 업로드용 임시 버퍼 (CPU -> GPU 전송 때만 잠깐 쓰고 버림)
    ComPtr<ID3D12Resource> VertexBufferUploader = nullptr;
    ComPtr<ID3D12Resource> IndexBufferUploader = nullptr;

    // 5. 데이터 정보
    UINT VertexByteStride = 0; // 점 하나 크기 (바이트)
    UINT VertexBufferByteSize = 0; // 전체 점 데이터 크기
    DXGI_FORMAT IndexFormat = DXGI_FORMAT_R16_UINT; // 인덱스 포맷 (16비트)
    UINT IndexBufferByteSize = 0; // 전체 인덱스 데이터 크기

    // 6. GPU에게 "여기서부터 여기까지 읽어"라고 알려주는 뷰(View) 반환 함수
    D3D12_VERTEX_BUFFER_VIEW VertexBufferView()const
    {
        D3D12_VERTEX_BUFFER_VIEW vbv;
        vbv.BufferLocation = VertexBufferGPU->GetGPUVirtualAddress();
        vbv.SizeInBytes = VertexBufferByteSize;
        vbv.StrideInBytes = VertexByteStride;
        return vbv;
    }

    D3D12_INDEX_BUFFER_VIEW IndexBufferView()const
    {
        D3D12_INDEX_BUFFER_VIEW ibv;
        ibv.BufferLocation = IndexBufferGPU->GetGPUVirtualAddress();
        ibv.Format = IndexFormat;
        ibv.SizeInBytes = IndexBufferByteSize;
        return ibv;
    }

    // 나중에 리소스 해제할 때 편하게 하려고 (소멸자는 기본 사용)
    void DisposeUploaders()
    {
        VertexBufferUploader = nullptr;
        IndexBufferUploader = nullptr;
    }
};