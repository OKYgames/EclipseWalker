#pragma once

#include "AnimationStructures.h"
#include <DirectXMath.h>
#include <vector>

class AnimationUtils
{
public:
    static DirectX::XMVECTOR InterpolatePosition(const KeyPosition& start, const KeyPosition& end, float t)
    {
        DirectX::XMVECTOR vStart = DirectX::XMVectorSet(start.Position[0], start.Position[1], start.Position[2], 0.0f);
        DirectX::XMVECTOR vEnd = DirectX::XMVectorSet(end.Position[0], end.Position[1], end.Position[2], 0.0f);
        return DirectX::XMVectorLerp(vStart, vEnd, t);
    }

    static DirectX::XMVECTOR InterpolateScale(const KeyScale& start, const KeyScale& end, float t)
    {
        DirectX::XMVECTOR vStart = DirectX::XMVectorSet(start.Scale[0], start.Scale[1], start.Scale[2], 0.0f);
        DirectX::XMVECTOR vEnd = DirectX::XMVectorSet(end.Scale[0], end.Scale[1], end.Scale[2], 0.0f);
        return DirectX::XMVectorLerp(vStart, vEnd, t);
    }

    static DirectX::XMVECTOR InterpolateRotation(const KeyRotation& start, const KeyRotation& end, float t)
    {
        DirectX::XMVECTOR qStart = DirectX::XMVectorSet(
            start.Orientation[0],
            start.Orientation[1],
            start.Orientation[2],
            start.Orientation[3]);

        DirectX::XMVECTOR qEnd = DirectX::XMVectorSet(
            end.Orientation[0],
            end.Orientation[1],
            end.Orientation[2],
            end.Orientation[3]);

        qStart = DirectX::XMQuaternionNormalize(qStart);
        qEnd = DirectX::XMQuaternionNormalize(qEnd);

        const float dot = DirectX::XMVectorGetX(DirectX::XMVector4Dot(qStart, qEnd));
        if (dot < 0.0f)
        {
            qEnd = DirectX::XMVectorNegate(qEnd);
        }

        return DirectX::XMQuaternionSlerp(qStart, qEnd, t);
    }

    template<typename T>
    static size_t FindKeyIndex(const std::vector<T>& keys, double animationTime)
    {
        if (keys.empty() || keys.size() == 1)
        {
            return 0;
        }

        for (size_t i = 0; i < keys.size() - 1; ++i)
        {
            if (animationTime < keys[i + 1].TimeStamp)
            {
                return i;
            }
        }

        return keys.size() - 1;
    }
};
