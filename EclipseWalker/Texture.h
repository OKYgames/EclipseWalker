#pragma once

#include <string>
#include <wrl.h>
#include <d3d12.h> 

struct Texture
{
    std::string Name;

    // 파일 경로 
    std::wstring Filename;

    Microsoft::WRL::ComPtr<ID3D12Resource> Resource = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> UploadHeap = nullptr;
};