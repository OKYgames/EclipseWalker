#pragma once

#include <Windows.h>
#include <DirectXMath.h>
#include <cstdint>

class MathHelper
{
public:
    static const float Pi;
    static const float Infinity;

    // 랜덤 함수 
    static float RandF()
    {
        return (float)rand() / (float)RAND_MAX;
    }

    static float RandF(float a, float b)
    {
        return a + RandF() * (b - a);
    }

    // 템플릿 헬퍼 (최소, 최대, 클램프)
    template<typename T>
    static T Min(const T& a, const T& b)
    {
        return a < b ? a : b;
    }

    template<typename T>
    static T Max(const T& a, const T& b)
    {
        return a > b ? a : b;
    }

    template<typename T>
    static T Lerp(const T& a, const T& b, float t)
    {
        return a + (b - a) * t;
    }

    template<typename T>
    static T Clamp(const T& x, const T& low, const T& high)
    {
        return x < low ? low : (x > high ? high : x);
    }

    // DirectX 관련 헬퍼 
    static DirectX::XMFLOAT4X4 Identity4x4()
    {
        static DirectX::XMFLOAT4X4 I(
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f);

        return I;
    }

    // 구면 좌표계 -> 직교 좌표계 변환 (카메라 위치 계산용)
    static DirectX::XMVECTOR SphericalToCartesian(float radius, float theta, float phi)
    {
        return DirectX::XMVectorSet(
            radius * sinf(phi) * cosf(theta),
            radius * cosf(phi),
            radius * sinf(phi) * sinf(theta),
            1.0f);
    }

	// 행렬 역행렬 계산
    static DirectX::XMMATRIX Inverse(DirectX::CXMMATRIX M)
    {
        DirectX::XMVECTOR det = DirectX::XMMatrixDeterminant(M);
        return DirectX::XMMatrixInverse(&det, M);
    }
};