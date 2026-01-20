#pragma once
#include <DirectXMath.h>

// [GPU 전송용] 셰이더와 구조가 똑같아야 하는 순수 데이터 
struct Light
{
    DirectX::XMFLOAT3 Strength = { 0.5f, 0.5f, 0.5f };
    float FalloffStart = 1.0f;
    DirectX::XMFLOAT3 Direction = { 0.0f, -1.0f, 0.0f };
    float FalloffEnd = 10.0f;
    DirectX::XMFLOAT3 Position = { 0.0f, 0.0f, 0.0f };
    float SpotPower = 64.0f;
};

// [CPU 로직용] 실제 게임에서 업데이트하고 조작할 클래스
class GameLight
{
public:
    enum class Type { Directional, Point, Spot };

    GameLight()
    {
        // 기본적으로 꺼진 상태
        mType = Type::Point; 
        mData = {}; 
        mData.Strength = { 0.0f, 0.0f, 0.0f }; 
        mData.FalloffEnd = 1.0f; 
    }

    // 초기화 함수
    void InitPoint(DirectX::XMFLOAT3 pos, DirectX::XMFLOAT3 color, float range)
    {
        mType = Type::Point;
        mData.Position = pos;
        mData.Strength = color;
        mData.FalloffStart = 1.0f;
        mData.FalloffEnd = range;
        mData.Direction = { 0.0f, 0.0f, 0.0f }; 
    }

    void InitDirectional(DirectX::XMFLOAT3 dir, DirectX::XMFLOAT3 color)
    {
        mType = Type::Directional;
        mData.Direction = dir;
        mData.Strength = color;
    }

    void InitSpot(DirectX::XMFLOAT3 pos, DirectX::XMFLOAT3 dir, DirectX::XMFLOAT3 color, float range, float spotPower)
    {
        mType = Type::Spot;
        mData.Position = pos;
        mData.Direction = dir;
        mData.Strength = color;
        mData.FalloffEnd = range;
        mData.SpotPower = spotPower;
    }

    void Update(float dt)
    {
        // 예: 낮/밤 구현 시 여기서 Strength나 Direction을 서서히 변경
    }

    void SetPosition(float x, float y, float z)
    {
        mData.Position = { x, y, z };
    }

    void SetPosition(const DirectX::XMFLOAT3& pos)
    {
        mData.Position = pos;
    }

    Light GetRawData() const { return mData; }
    Type GetType() const { return mType; }
    void SetTag(const char* tag) { mTags = tag; }

private:
    Light mData;       // 실제 데이터
    Type mType;        // 조명 타입
    const char* mTags = ""; // 구분용 태그
    float mRotationAngle = 0.0f; 
};
