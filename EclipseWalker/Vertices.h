#pragma once
#include <DirectXMath.h>

using namespace DirectX;

namespace VertexTypes
{
    // 1. 가장 기본적인 정점 
    struct VertexPosColor
    {
        XMFLOAT3 Pos;
        XMFLOAT4 Color;
    };

    // 2. 텍스처를 입힐 수 있는 정점
    struct VertexPosTex
    {
        XMFLOAT3 Pos;
        XMFLOAT3 Normal;
        XMFLOAT2 TexC;
    };

    // 3. 조명까지 받는 복잡한 정점 
    struct VertexPosNormalTex
    {
        XMFLOAT3 Pos;
        XMFLOAT3 Normal;
        XMFLOAT2 TexC;
    };

}