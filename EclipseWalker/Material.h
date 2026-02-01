#pragma once

#include "d3dUtil.h"

struct Material
{
    // 재질 이름 
    std::string Name;

    // 상수 버퍼 내에서의 인덱스 (GPU가 찾을 주소)
    int MatCBIndex = -1;

    // 텍스처 인덱스 
    int DiffuseSrvHeapIndex = -1;

    // 실제 데이터
    DirectX::XMFLOAT4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
    DirectX::XMFLOAT3 FresnelR0 = { 0.01f, 0.01f, 0.01f };
    float Roughness = 0.25f;
    int IsToon;

    // 쉐이더 업데이트용 헬퍼 함수
    Material() {
        DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
        FresnelR0 = { 0.01f, 0.01f, 0.01f };
        Roughness = 0.25f;
    }
};